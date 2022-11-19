#include <ext2_wrapper.h>
#include <print_lib.h>

#include <unistd.h>
#include <sys/stat.h>

int read_range(int fd, void* buf, uint32_t pos, uint32_t size)
{
	CHECK_TRUE(pread(fd, buf, size, pos) == size);
	return 0;
}

int read_blk(int fd, void* buf, uint32_t blk_pos, uint32_t blk_size)
{
	const int32_t pos = blk_pos * blk_size;
	CHECK_NNEG(read_range(fd, buf, pos, blk_size));
	return 0;
}

int read_sblk(int fd, struct ext2_super_block* sblk_buf)
{
	const uint32_t size = sizeof(struct ext2_super_block);
	CHECK_NNEG(read_range(fd, sblk_buf, SUPERBLOCK_OFFSET, size));
	CHECK_TRUE(sblk_buf->s_magic == EXT2_SUPER_MAGIC);
	return 0;
}

uint32_t get_blk_size(const struct ext2_super_block* sblk)
{
	return 1024u << sblk->s_log_block_size;
}

uint32_t get_inode_group(const struct ext2_super_block* sblk, uint32_t inode_nr)
{
	return (inode_nr - 1) / sblk->s_inodes_per_group;
}

uint32_t get_inode_local_index(const struct ext2_super_block* sblk, uint32_t inode_nr)
{
	return (inode_nr - 1) % sblk->s_inodes_per_group;
}

uint32_t get_inode_pos(const struct ext2_super_block* sblk,
					   const struct ext2_group_desc* bg_desc_table, uint32_t inode_nr)
{
	const uint32_t blk_size = get_blk_size(sblk);
	const uint32_t group = get_inode_group(sblk, inode_nr);
	const uint32_t index = get_inode_local_index(sblk, inode_nr);

	return bg_desc_table[group].bg_inode_table * blk_size + index * sblk->s_inode_size;
}

// Contract: exist indirect valid index (indir_table_blk_pos != 0)
static int view_blocks_indir_block(viewer_t viewer, void* user_data,
								   int fd, uint32_t indir_table_blk_pos, uint32_t blk_size, uint8_t* indir_itable_buf)
{
	// Read table of block indexes
	CHECK_NNEG(read_blk(fd, indir_itable_buf, indir_table_blk_pos, blk_size));
	const uint32_t* const indir_itable = (uint32_t*)indir_itable_buf;

	// View all used blocks
	const uint32_t indir_blocks_count = blk_size / sizeof(uint32_t);
	for (uint32_t i_blk = 0; i_blk < indir_blocks_count; ++i_blk)
	{
		int res = viewer(user_data, indir_itable[i_blk], blk_size);
		if (res == BLK_VIEWER_END)
			return BLK_VIEWER_END;

		CHECK_NNEG(res);
	}

	return BLK_VIEWER_CONT;
}

// This function execute viewer on all data blocks accotiated with inode
int view_blocks(int fd, const struct ext2_super_block* sblk, const struct ext2_inode* inode,
                viewer_t viewer, void* user_data)
{
	const uint32_t blk_size = 1024u << sblk->s_log_block_size;

	// Read direct blocks
	for (uint32_t i_dir_blk = 0; i_dir_blk < EXT2_NDIR_BLOCKS; ++i_dir_blk)
	{
		int res = viewer(user_data, inode->i_block[i_dir_blk], blk_size);
		if (res == BLK_VIEWER_END)
			return 0;

		CHECK_NNEG(res);
	}

	const uint32_t indir_blk_pos = inode->i_block[EXT2_IND_BLOCK];
	if (indir_blk_pos == 0)
		return 0;

	// Read indirect blocks
	uint8_t blk_buf[blk_size];
	int res = view_blocks_indir_block(viewer, user_data, fd, indir_blk_pos, blk_size, blk_buf);
	if (res == BLK_VIEWER_END || inode->i_block[EXT2_DIND_BLOCK] == 0)
		return 0;

	CHECK_NNEG(res);

	// Read double indirect blocks
	uint8_t dbl_indir_itable_buf[blk_size];
	CHECK_NNEG(read_blk(fd, dbl_indir_itable_buf, inode->i_block[EXT2_DIND_BLOCK], blk_size));
	const uint32_t* const dbl_indir_itable = (uint32_t*)dbl_indir_itable_buf;

	const uint32_t index_per_blk = blk_size / sizeof(uint32_t);
	for (uint32_t i_dbl_indir_blk = 0; i_dbl_indir_blk < index_per_blk; ++i_dbl_indir_blk)
	{
		const uint32_t indir_blk_pos = dbl_indir_itable[i_dbl_indir_blk];
		res = view_blocks_indir_block(viewer, user_data, fd, indir_blk_pos, blk_size, blk_buf);
		if (res == BLK_VIEWER_END)
			return 0;

		CHECK_NNEG(res);
	}

	errno = EFBIG;
	return -errno;
}

static inline uint64_t copyer_calc_size_read(uint64_t unreaded_size, uint32_t blk_size)
{
	return unreaded_size >= blk_size ? blk_size : unreaded_size;
}

int copyer(void* data_ptr, off64_t blk_pos, uint32_t blk_size)
{
    struct copyer_data_t* data = (struct copyer_data_t*)data_ptr;

	const ssize_t size_read = copyer_calc_size_read(data->unreaded_size, blk_size);
	const off64_t pos = blk_pos * blk_size;
	CHECK_TRUE(pread64(data->in, data->buf, size_read, pos) == size_read
			   && write(data->out, data->buf, size_read) == size_read);
	
	data->unreaded_size -= size_read;
	return data->unreaded_size == 0 ? BLK_VIEWER_END : BLK_VIEWER_CONT;
}

// TODO: implement viewer for view_blocks and solve copy task (04) use new interface