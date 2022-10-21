#include <solution.h>

#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <linux/fs.h>
#include <ext2fs/ext2fs.h>
#include <math.h>

#define dump(obj) printf(#obj ": %u\n", obj)
#define dump_lu(obj) printf(#obj ": %lu\n", obj)
#define dump_b(obj) printf(#obj ": %d\n", !!(obj))
#define dump_x(obj) printf(#obj ": %x\n", obj)

static void print_location(int fd, const char* file, long line, const char* func)
{
	dprintf(fd, "%s:%li in %s function", file, line, func);
}

#define PRINT_ERR(expression)													\
	do {																		\
		print_location(STDERR_FILENO, __FILE__, __LINE__, __PRETTY_FUNCTION__); \
		dprintf(STDERR_FILENO, "\n");											\
		perror(#expression);													\
	} while(0)

#define CHECK_NNEG(func)	\
	do {					\
		int res = ((func)); \
		if (res < 0)		\
		{					\
			PRINT_ERR(func);\
			return -errno;	\
		}					\
	} while(0)

#define CHECK_TRUE(cond)	\
	do {					\
		if (!(cond))		\
		{					\
			PRINT_ERR(cond);\
			return -errno;	\
		}					\
	} while(0)

#define min(a,b) 				\
   ({ __typeof__ (a) _a = (a); 	\
       __typeof__ (b) _b = (b); \
     _a > _b ? _b : _a; })

int copy_file(int in, int out, struct ext2_inode* inode, struct ext2_super_block* sblk)
{
	const uint32_t blk_size = 1024u << sblk->s_log_block_size;

	// Prepare params for read
	const uint64_t file_size = inode->i_size + ((uint64_t)inode->i_size_high << 32);
	uint8_t buf[blk_size];
	uint64_t unreaded_size = file_size;
	uint32_t unreaded_blks = unreaded_size / blk_size + !!(unreaded_size % blk_size);

	// Read direct blocks
	const uint32_t i_dir_blk_end = min(unreaded_blks, (uint32_t)EXT2_NDIR_BLOCKS);
	for (uint32_t i_dir_blk = 0; i_dir_blk < i_dir_blk_end; ++i_dir_blk)
	{
		const uint32_t size_read = unreaded_size >= blk_size ? blk_size : unreaded_size;
		CHECK_TRUE(lseek(in, inode->i_block[i_dir_blk] * blk_size, SEEK_SET) >= 0
				   && read(in, buf, size_read) == size_read
				   && write(out, buf, size_read) == size_read);

		unreaded_size -= size_read;
	}
	unreaded_blks -= i_dir_blk_end;

	// Read indirect blocks
	uint8_t indir_itable_buf[blk_size];
	CHECK_NNEG(lseek(in, blk_size * inode->i_block[EXT2_IND_BLOCK], SEEK_SET));
	CHECK_TRUE(read(in, indir_itable_buf, blk_size) == blk_size);
	const uint32_t* const indir_itable = (uint32_t*)indir_itable_buf;

	const uint32_t indir_blocks_count = blk_size / sizeof(uint32_t);
	const uint32_t i_indir_blk_end = min(unreaded_blks, indir_blocks_count);
	for (uint32_t i_indir_blk = 0; i_indir_blk < i_indir_blk_end; ++i_indir_blk)
	{
		const uint32_t size_read = unreaded_size >= blk_size ? blk_size : unreaded_size;
		CHECK_TRUE(lseek(in, indir_itable[i_indir_blk] * blk_size, SEEK_SET) >= 0
				   && read(in, buf, size_read) == size_read
				   && write(out, buf, size_read) == size_read);

		unreaded_size -= size_read;
	}
	unreaded_blks -= i_indir_blk_end;

	return unreaded_blks ? -1 : 0;
}

int dump_file(int img, int inode_nr, int out)
{
	CHECK_NNEG(lseek(img, SUPERBLOCK_OFFSET, SEEK_SET));

	struct ext2_super_block sblk = {};
	CHECK_TRUE(read(img, &sblk, sizeof(sblk)) == sizeof(sblk));
	CHECK_TRUE(sblk.s_magic == EXT2_SUPER_MAGIC);

	const uint32_t blk_size = 1024u << sblk.s_log_block_size;

	uint8_t block_group_desc_table[blk_size];
	memset(block_group_desc_table, 0, sizeof(block_group_desc_table));
	CHECK_NNEG(lseek(img, blk_size * (sblk.s_first_data_block + 1), SEEK_SET));
	CHECK_TRUE(read(img, block_group_desc_table, blk_size) == blk_size);
	struct ext2_group_desc* bg = (struct ext2_group_desc*)block_group_desc_table;

	uint32_t inode_group = (inode_nr - 1) / sblk.s_inodes_per_group;
	uint32_t inode_local_index = (inode_nr - 1) % sblk.s_inodes_per_group;

	const uint32_t inode_pos = bg[inode_group].bg_inode_table * blk_size +
							   inode_local_index * sblk.s_inode_size;
	CHECK_NNEG(lseek(img, inode_pos, SEEK_SET));
	struct ext2_inode inode = {};
	CHECK_TRUE(read(img, &inode, sizeof(struct ext2_inode)) == sizeof(struct ext2_inode));

	CHECK_NNEG(copy_file(img, out, &inode, &sblk));

	return 0;
}
