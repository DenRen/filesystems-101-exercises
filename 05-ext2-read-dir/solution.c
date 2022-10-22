#include <solution.h>
#include <ext2_wrapper.h>

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

static int read_range(int fd, void* buf, uint32_t pos, uint32_t size)
{
	CHECK_TRUE(lseek(fd, pos, SEEK_SET) >= 0
			   && read(fd, buf, size) == size);
	return 0;
}

static int read_blk(int fd, void* buf, uint32_t blk_pos, uint32_t blk_size)
{
	const int32_t pos = blk_pos * blk_size;
	CHECK_NNEG(read_range(fd, buf, pos, blk_size));
	return 0;
}

static int read_sblk(int fd, struct ext2_super_block* sblk_buf)
{
	const uint32_t size = sizeof(struct ext2_super_block);
	CHECK_NNEG(read_range(fd, sblk_buf, SUPERBLOCK_OFFSET, size));
	CHECK_TRUE(sblk_buf->s_magic == EXT2_SUPER_MAGIC);
	return 0;
}

static uint32_t get_blk_size(const struct ext2_super_block* sblk)
{
	return 1024u << sblk->s_log_block_size;
}

static uint32_t get_inode_group(const struct ext2_super_block* sblk, uint32_t inode_nr)
{
	return (inode_nr - 1) / sblk->s_inodes_per_group;
}

static uint32_t get_inode_local_index(const struct ext2_super_block* sblk, uint32_t inode_nr)
{
	return (inode_nr - 1) % sblk->s_inodes_per_group;
}

static uint32_t get_inode_pos(const struct ext2_super_block* sblk,
							  const struct ext2_group_desc* bg_desc_table, uint32_t inode_nr)
{
	const uint32_t blk_size = get_blk_size(sblk);
	const uint32_t group = get_inode_group(sblk, inode_nr);
	const uint32_t index = get_inode_local_index(sblk, inode_nr);

	return bg_desc_table[group].bg_inode_table * blk_size + index * sblk->s_inode_size;
}

struct dump_dir_data_t
{
	uint8_t* buf;
	int in, out;
};

static int print_name_nl(int out, const char* name, uint8_t len)
{
	CHECK_TRUE(write(out, name, len) == len 
			   && write(out, "\n", 1) == 1);
	return 0;
}

static int dir_dumper(void* data_ptr, off_t pos, uint32_t size_read)
{
	(void)pos;
	struct dump_dir_data_t* data = (struct dump_dir_data_t*)data_ptr;
	
	CHECK_TRUE(read(data->in, data->buf, size_read) == size_read);
	const struct ext2_dir_entry_2* entry = (struct ext2_dir_entry_2*)data->buf;

	while(entry->inode)
	{
		CHECK_NNEG(print_name_nl(data->out, entry->name, entry->name_len));
		
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
	CHECK_NNEG(view_inode(img, &sblk, &inode, dir_dumper, &data));

	return 0;
}
