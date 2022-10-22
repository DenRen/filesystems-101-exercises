#include <solution.h>
#include <print_lib.h>
#include <ext2_wrapper.h>

struct copy_data_t
{
    uint8_t* buf;
    int in, out;
};

static int copyer(void* data_ptr, off_t pos, uint32_t size_read)
{
    (void)pos;

    struct copy_data_t* data = (struct copy_data_t*)data_ptr;
	CHECK_TRUE(read(data->in, data->buf, size_read) == size_read
			   && write(data->out, data->buf, size_read) == size_read);
	return 0;
}

int dump_file_impl(int img, int inode_nr, int out)
{
	// Read ext2 super block and ext2 inode
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

	// Prepare buffer for viewer-copyer
    uint8_t buf[blk_size];
    struct copy_data_t copy_data = {
        .buf = buf,
        .in = img,
        .out = out
    };
	
	CHECK_NNEG(view_inode(img, &sblk, &inode, copyer, &copy_data));

	return 0;
}

int dump_file(int img, int inode_nr, int out)
{
	CHECK_NNEG(dump_file_impl(img, inode_nr, out));
	return 0;
}
