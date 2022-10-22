#include <ext2_wrapper.h>
#include <print_lib.h>

#include <unistd.h>
#include <sys/stat.h>

static inline uint32_t calc_size_read(uint32_t unreaded_size, uint32_t blk_size)
{
	return unreaded_size >= blk_size ? blk_size : unreaded_size;
}

static inline int copy_indir_block(viewer_t viewer, void* user_data, int in, uint32_t indir_blk_pos, uint32_t blk_size,
							       uint8_t* indir_itable_buf, uint64_t* unreaded_size_ptr, uint32_t* unreaded_blks)
{
	uint32_t pos = blk_size * indir_blk_pos;
	uint64_t unreaded_size = *unreaded_size_ptr;

	CHECK_NNEG(lseek(in, pos, SEEK_SET));
	CHECK_TRUE(read(in, indir_itable_buf, blk_size) == blk_size);
	const uint32_t* const indir_itable = (uint32_t*)indir_itable_buf;

	const uint32_t indir_blocks_count = blk_size / sizeof(uint32_t);
	const uint32_t i_indir_blk_end = min(*unreaded_blks, indir_blocks_count);
	for (uint32_t i_indir_blk = 0; i_indir_blk < i_indir_blk_end; ++i_indir_blk)
	{
		const uint32_t size_read = calc_size_read(unreaded_size, blk_size);
		const uint32_t pos = indir_itable[i_indir_blk] * blk_size;
        CHECK_NNEG(lseek(in, pos, SEEK_SET));
		CHECK_NNEG(viewer(user_data, pos, size_read));
		unreaded_size -= size_read;
	}

	*unreaded_size_ptr = unreaded_size;
	*unreaded_blks -= i_indir_blk_end;
	return 0;
}

// This function execute viewer on all data blocks accotiated with inode
int view_inode(int in, const struct ext2_super_block* sblk, const struct ext2_inode* inode,
               viewer_t viewer, void* user_data)
{
	const uint32_t blk_size = 1024u << sblk->s_log_block_size;

	// Prepare params for read
	const uint64_t file_size = inode->i_size + ((uint64_t)inode->i_size_high << 32);
	uint64_t unreaded_size = file_size;
	uint32_t unreaded_blks = unreaded_size / blk_size + !!(unreaded_size % blk_size);

	// Read direct blocks
	const uint32_t i_dir_blk_end = min(unreaded_blks, (uint32_t)EXT2_NDIR_BLOCKS);
	for (uint32_t i_dir_blk = 0; i_dir_blk < i_dir_blk_end; ++i_dir_blk)
	{
		const uint32_t size_read = calc_size_read(unreaded_size, blk_size);
		const uint32_t pos = inode->i_block[i_dir_blk] * blk_size;
        CHECK_NNEG(lseek(in, pos, SEEK_SET));
		CHECK_NNEG(viewer(user_data, pos, size_read));

		unreaded_size -= size_read;
	}
	unreaded_blks -= i_dir_blk_end;

	// Read indirect blocks
	uint8_t indir_itable_buf[blk_size];
	const uint32_t indir_blk_pos = inode->i_block[EXT2_IND_BLOCK];
	CHECK_NNEG(copy_indir_block(viewer, user_data, in, indir_blk_pos, blk_size, indir_itable_buf,
								&unreaded_size, &unreaded_blks));

	if (unreaded_size == 0)
		return 0;

	// Read double indirect blocks
	uint8_t dbl_indir_itable_buf[blk_size];
	const uint32_t dbl_indir_blk_pos = blk_size * inode->i_block[EXT2_DIND_BLOCK];
	CHECK_NNEG(lseek(in, dbl_indir_blk_pos, SEEK_SET));
	CHECK_TRUE(read(in, dbl_indir_itable_buf, blk_size) == blk_size);
	const uint32_t* const dbl_indir_itable = (uint32_t*)dbl_indir_itable_buf;

	const uint32_t index_per_blk = blk_size / sizeof(uint32_t);
	for (uint32_t i_indir = 0; i_indir < index_per_blk; ++i_indir)
	{
		const uint32_t indir_blk_pos = dbl_indir_itable[i_indir];
		CHECK_NNEG(copy_indir_block(viewer, user_data, in, indir_blk_pos, blk_size, indir_itable_buf,
									&unreaded_size, &unreaded_blks));

		if (unreaded_size == 0)
			return 0;
	}

	return unreaded_blks ? -1 : 0;
}
