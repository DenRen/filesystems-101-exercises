#include <solution.h>
#include <solution.h>
#include <print_lib.h>
#include <ext2_wrapper.h>

#include <sys/stat.h>

static const char* check_path(const char* path)
{
	if (path == NULL)
	{
		static const char err_msg[] = "Path is NULL";
		return err_msg;
	}
	if (*path != '/')
	{
		static const char err_msg[] = "Path is NOT absolute";
		return err_msg;
	}
	if (path[strlen(path) - 1] == '/')
	{
		static const char err_msg[] = "Path cannot end on '/'";
		return err_msg;
	}
	
	return NULL; // path is correct
}

struct find_file_in_dir_data_t
{
	int img_fd;				// File descriptor of file system image
	uint8_t* blk_buf;		// For read blk
	const char* file_name;	// Finded file
	unsigned file_name_len;	// Size of finded file
	uint32_t found_inode;	// Inode of found file
};

static int finder_file_in_dir(void* data_ptr, off_t blk_pos, uint32_t blk_size)
{
	if (blk_pos == 0)
		return BLK_VIEWER_END;

	struct find_file_in_dir_data_t* data = (struct find_file_in_dir_data_t*)data_ptr;
	CHECK_NNEG(read_blk(data->img_fd, data->blk_buf, blk_pos, blk_size));
	const struct ext2_dir_entry_2* entry = (struct ext2_dir_entry_2*)data->blk_buf;

	const unsigned file_name_len = data->file_name_len;
	uint32_t unreaded_size = blk_size;
	while(unreaded_size)
	{
		CHECK_TRUE(entry->name_len <= EXT2_NAME_LEN);
		
		if (entry->name_len == file_name_len &&
			memcmp(entry->name, data->file_name, file_name_len) == 0)
		{
			data->found_inode = entry->inode;
			return BLK_VIEWER_END;
		}

		unreaded_size -= entry->rec_len;
		entry = (struct ext2_dir_entry_2*)((uint8_t*)entry + entry->rec_len);
	}

	return BLK_VIEWER_CONT;
}

int dump_file(int img, const char *path, int out)
{
	const char* const check_path_err_msg = check_path(path);
	if (check_path_err_msg != NULL)
	{
		fprintf(stderr, "%s", check_path_err_msg);
		return -EINVAL;
	}

	struct ext2_super_block sblk = {};
	CHECK_NNEG(read_sblk(img, &sblk));

	const uint32_t blk_size = get_blk_size(&sblk);

	uint8_t bg_desc_table_buf[blk_size];
	CHECK_NNEG(read_blk(img, bg_desc_table_buf, sblk.s_first_data_block + 1, blk_size));
	struct ext2_group_desc* bg_decs_table = (struct ext2_group_desc*)bg_desc_table_buf;

	// Go by path
	uint8_t blk_buf[blk_size];

	int cur_dir_inode = EXT2_ROOT_INO;
	const char* folder_name = path + 1;	// Remove path of root ('/') dir
	for (;;)
	{
		const char* folder_name_end = strchr(folder_name, '/');
		const unsigned folder_name_len = folder_name_end != NULL
										 ? folder_name_end - folder_name	// Search folder
										 : (unsigned)strlen(folder_name);	// Search file
		
		const uint32_t inode_pos = get_inode_pos(&sblk, bg_decs_table, cur_dir_inode);
		struct ext2_inode inode = {};
		CHECK_NNEG(read_range(img, &inode, inode_pos, sizeof(inode)));
		if ((inode.i_mode & S_IFMT) != S_IFDIR)
			return -ENOTDIR;

		struct find_file_in_dir_data_t data = {
			.blk_buf = blk_buf,
			.img_fd = img,
			.file_name = folder_name,
			.file_name_len = folder_name_len,
			.found_inode = -1u
		};
		CHECK_NNEG(view_blocks(img, &sblk, &inode, finder_file_in_dir, &data));

		if (data.found_inode == -1u)
			return -ENOENT;
		
		cur_dir_inode = data.found_inode;
		if (folder_name_end == NULL)
			break;

		folder_name = folder_name_end + 1;	// It will work, because path check function
											// checked path on '/' at the end of the path
	}
	
	// Dump file
	const int file_inode_nr = cur_dir_inode;
	const uint32_t file_inode_pos = get_inode_pos(&sblk, bg_decs_table, file_inode_nr);
	struct ext2_inode file_inode = {};
	CHECK_NNEG(read_range(img, &file_inode, file_inode_pos, sizeof(file_inode)));

	// Prepare buffer for viewer-copyer
    struct copyer_data_t copy_data = {
        .buf = blk_buf,
        .in = img,
        .out = out,
		.unreaded_size = file_inode.i_size + ((uint64_t)file_inode.i_size_high << 32)
    };
	
	if (copy_data.unreaded_size)
		CHECK_NNEG(view_blocks(img, &sblk, &file_inode, copyer, &copy_data));

	return 0;
}
