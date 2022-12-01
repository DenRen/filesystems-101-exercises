#include <sys/stat.h>

#include "ext2_fs_kernel.h"

struct find_file_in_dir_data_t
{
	int img_fd;				// File descriptor of file system image
	uint8_t* blk_buf;		// For read blk
	const char* file_name;	// Finded file
	unsigned file_name_len;	// Size of finded file
	uint32_t found_inode;	// Inode of found file
};

static int finder_file_in_dir(void* data_ptr,
                              off_t blk_pos,
                              uint32_t blk_size)
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

// path: /hello/file
// path: /hello/dir
// path: /hello/dir/
int path2inode(const char* path,
			   const int img,
			   const struct ext2_super_block* sblk,
			   const struct ext2_group_desc* bg_decs_table,
			   uint8_t* blk_buf) // Size must be not less then get_blk_size(sblk)
{
	int cur_dir_inode = EXT2_ROOT_INO;
	const char* folder_name = path + 1;	// Remove path of root ('/') dir
	while (folder_name != NULL && *folder_name != '\0')
	{
		// folder_name: hello/file
		// folder_name: dir
		const char* folder_name_end = strchr(folder_name, '/');
		const unsigned folder_name_len = folder_name_end != NULL
										 ? folder_name_end - folder_name	// Search folder
										 : (unsigned)strlen(folder_name);	// Search file or folder

		if (folder_name == 0)	// path: /hello/dir/ <- last char is '/'
			break;

		const uint32_t catalog_inode_pos = get_inode_pos(sblk, bg_decs_table, cur_dir_inode);
		struct ext2_inode catalog_inode = {};
		CHECK_NNEG(read_range(img, &catalog_inode, catalog_inode_pos, sizeof(catalog_inode)));
		if ((catalog_inode.i_mode & S_IFMT) != S_IFDIR)
			return -ENOTDIR;

		struct find_file_in_dir_data_t data =
		{
			.blk_buf = blk_buf,
			.img_fd = img,
			.file_name = folder_name,
			.file_name_len = folder_name_len,
			.found_inode = -1u
		};
		CHECK_NNEG(view_blocks(img, sblk, &catalog_inode, finder_file_in_dir, &data));
		if (data.found_inode == -1u)
			return -ENOENT;

		cur_dir_inode = data.found_inode;
		if (folder_name_end == NULL)
			break;

		folder_name = folder_name_end + 1;
	}

	return cur_dir_inode;
}
