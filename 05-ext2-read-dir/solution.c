#include <solution.h>
#include <print_lib.h>
#include <ext2_wrapper.h>

#include <sys/stat.h>

// #define ENABLE_MYSELF_REPORT_FILE

#ifdef ENABLE_MYSELF_REPORT_FILE
void report_file(int inode_nr, char type, const char *name)
{
	printf("%d %x %s\n", inode_nr, type, name);
}
#endif

struct dump_dir_data_t
{
	int in, out;
	uint8_t* buf;
	char name[EXT2_NAME_LEN + 1];
};

static int dir_dumper(void* data_ptr, off_t blk_pos, uint32_t blk_size)
{
	if (blk_pos == 0)
		return BLK_VIEWER_END;

	struct dump_dir_data_t* data = (struct dump_dir_data_t*)data_ptr;
	CHECK_NNEG(read_blk(data->in, data->buf, blk_pos, blk_size));
	const struct ext2_dir_entry_2* entry = (struct ext2_dir_entry_2*)data->buf;

	while(entry->inode)
	{
		CHECK_TRUE(entry->name_len <= EXT2_NAME_LEN);
		memcpy(data->name, entry->name, entry->name_len);
		data->name[entry->name_len] = '\0';

		report_file(entry->inode, entry->file_type, entry->name);

		entry = (struct ext2_dir_entry_2*)((uint8_t*)entry + entry->rec_len);
	}

	return 0;
}

int dump_dir(int img, int inode_nr)
{
	struct ext2_super_block sblk = {};
	CHECK_NNEG(read_sblk(img, &sblk));

	const uint32_t blk_size = get_blk_size(&sblk);

	uint8_t bg_desc_table_buf[blk_size];
	CHECK_NNEG(read_blk(img, bg_desc_table_buf, sblk.s_first_data_block + 1, blk_size));
	struct ext2_group_desc* bg_decs_table = (struct ext2_group_desc*)bg_desc_table_buf;

	const uint32_t inode_pos = get_inode_pos(&sblk, bg_decs_table, inode_nr);
	struct ext2_inode inode = {};
	CHECK_NNEG(read_range(img, &inode, inode_pos, sizeof(inode)));
	CHECK_TRUE((inode.i_mode & S_IFMT) == S_IFDIR);

	uint8_t buf[blk_size];
	struct dump_dir_data_t data = {
		.buf = buf,
		.in = img,
		.out = STDOUT_FILENO
	};
	CHECK_NNEG(view_blocks(img, &sblk, &inode, dir_dumper, &data));

	return 0;
}
