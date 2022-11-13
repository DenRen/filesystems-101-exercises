#include <solution.h>
#include <print_lib.h>
#include <ext2_wrapper.h>

static inline uint64_t calc_size_read(uint64_t unreaded_size, uint32_t blk_size)
{
	return unreaded_size >= blk_size ? blk_size : unreaded_size;
}

int copyer(void* data_ptr, off64_t blk_pos, uint32_t blk_size)
{
    struct copyer_data_t* data = (struct copyer_data_t*)data_ptr;

	const ssize_t size_read = calc_size_read(data->unreaded_size, blk_size);
	const off64_t pos = blk_pos * blk_size;
	CHECK_TRUE(pread64(data->in, data->buf, size_read, pos) == size_read
			   && write(data->out, data->buf, size_read) == size_read);
	
	data->unreaded_size -= size_read;
	return data->unreaded_size == 0 ? BLK_VIEWER_END : BLK_VIEWER_CONT;
}

int dump_file_impl(int img, int inode_nr, int out)
{
	// Read ext2 super block and ext2 inode
	struct ext2_super_block sblk = {};
	CHECK_NNEG(read_sblk(img, &sblk));

	const uint32_t blk_size = get_blk_size(&sblk);

	uint8_t bg_desc_table_buf[blk_size];
	CHECK_NNEG(read_blk(img, bg_desc_table_buf, sblk.s_first_data_block + 1, blk_size));
	struct ext2_group_desc* bg_decs_table = (struct ext2_group_desc*)bg_desc_table_buf;

	const uint32_t inode_pos = get_inode_pos(&sblk, bg_decs_table, inode_nr);
	struct ext2_inode inode = {};
	CHECK_NNEG(read_range(img, &inode, inode_pos, sizeof(inode)));

	// Prepare buffer for viewer-copyer
    uint8_t buf[blk_size];
    struct copyer_data_t copy_data = {
        .buf = buf,
        .in = img,
        .out = out,
		.unreaded_size = inode.i_size + ((uint64_t)inode.i_size_high << 32)
    };
	
	if (copy_data.unreaded_size)
		CHECK_NNEG(view_blocks(img, &sblk, &inode, copyer, &copy_data));

	return 0;
}

int dump_file(int img, int inode_nr, int out)
{
	CHECK_NNEG(dump_file_impl(img, inode_nr, out));
	return 0;
}
