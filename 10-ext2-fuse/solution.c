#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include <print_lib.h>
#include <ext2_wrapper.h>
#include "solution.h"
#include "ext2_fs_kernel.h"

#define UNUSED(obj) (void)obj

struct fs_desc_t
{
	int img;
	struct ext2_super_block* sblk;
	struct ext2_group_desc* bg_decs_table;
	uint8_t* blk_buf;
};

static struct fs_desc_t fs_desc;

static void* my_fs_init(struct fuse_conn_info* conn, struct fuse_config* cfg)
{
	UNUSED(conn);
	cfg->kernel_cache = 1;

	cfg->set_uid = 1;
	cfg->uid = getgid();

	cfg->set_gid = 1;
	cfg->gid = getgid();

	cfg->set_mode = 1;
	cfg->umask = ~S_IRUSR;

	return NULL;
}

static int check_abs_path(const char* path)
{
	return path && *path == '/' ? 0 : -EINVAL;
}

static int my_fs_getattr(const char* path,
						 struct stat* stbuf,
						 struct fuse_file_info* file_info)
{
	UNUSED(file_info);

	if (check_abs_path(path) < 0)
		return -ENOENT;

	memset(stbuf, 0, sizeof(*stbuf));

	// Return index inode of the object by path
	int inode_index = path2inode(path, fs_desc.img, fs_desc.sblk, fs_desc.bg_decs_table, fs_desc.blk_buf);
	if (inode_index < 0)
		return inode_index;

	// Get file type
	const uint32_t catalog_inode_pos = get_inode_pos(fs_desc.sblk, fs_desc.bg_decs_table, inode_index);
	struct ext2_inode inode = {};
	CHECK_NNEG(read_range(fs_desc.img, &inode, catalog_inode_pos, sizeof(inode)));

	stbuf->st_mode = inode_index;
	stbuf->st_mode = inode.i_mode;
	stbuf->st_size = inode.i_size + ((size_t)inode.i_size_high << 32);
	stbuf->st_nlink = inode.i_links_count;

	return 0;
}

struct fill_catalog_files_data_t
{
	void* filler_buf_out;
	fuse_fill_dir_t filler;

	int in;
	uint8_t* blk_buf;
	char name_buf[EXT2_NAME_LEN + 1];
};

static int readdir_filler(void* data_ptr,
						  off_t blk_pos,
						  uint32_t blk_size)
{
	if (blk_pos == 0)
		return BLK_VIEWER_END;

	struct fill_catalog_files_data_t* data = (struct fill_catalog_files_data_t*)data_ptr;
	CHECK_NNEG(read_blk(data->in, data->blk_buf, blk_pos, blk_size));
	const struct ext2_dir_entry_2* entry = (struct ext2_dir_entry_2*)data->blk_buf;

	uint32_t unreaded_size = blk_size;
	while(unreaded_size)
	{
		CHECK_TRUE(entry->name_len <= EXT2_NAME_LEN);
		memcpy(data->name_buf, entry->name, entry->name_len);
		data->name_buf[entry->name_len] = '\0';

		data->filler(data->filler_buf_out, data->name_buf, NULL, 0, 0);

		unreaded_size -= entry->rec_len;
		entry = (struct ext2_dir_entry_2*)((uint8_t*)entry + entry->rec_len);
	}

	return BLK_VIEWER_CONT;
}

static int my_fs_readdir(const char* path,
						 void* filler_buf_out,
						 fuse_fill_dir_t filler,
						 off_t off,
						 struct fuse_file_info* file_info,
						 enum fuse_readdir_flags flags)
{
	UNUSED(off);
	UNUSED(file_info);
	UNUSED(flags);

	if (check_abs_path(path) < 0)
		return -ENOENT;

	// Return index inode of the object by path
	int catalog_inode_index = path2inode(path, fs_desc.img, fs_desc.sblk, fs_desc.bg_decs_table, fs_desc.blk_buf);
	if (catalog_inode_index < 0)
		return catalog_inode_index;

	// Check that it is catalog
	const uint32_t catalog_inode_pos = get_inode_pos(fs_desc.sblk, fs_desc.bg_decs_table, catalog_inode_index);
	struct ext2_inode catalog_inode = {};
	CHECK_NNEG(read_range(fs_desc.img, &catalog_inode, catalog_inode_pos, sizeof(catalog_inode)));
	if ((catalog_inode.i_mode & S_IFMT) != S_IFDIR)
		return -ENOTDIR;

	// Fill file names of catalog
	struct fill_catalog_files_data_t data = {
		.filler_buf_out = filler_buf_out,
		.filler = filler,

		.blk_buf = fs_desc.blk_buf,
		.in = fs_desc.img
	};
	CHECK_NNEG(view_blocks(fs_desc.img, fs_desc.sblk, &catalog_inode, readdir_filler, &data));

	return 0;
}

static int my_fs_open(const char* path,
					  struct fuse_file_info* file_info)
{
	if ((file_info->flags & O_ACCMODE) != O_RDONLY)
	{
		return -EROFS;
	}

	if (check_abs_path(path) < 0)
		return -ENOENT;

	// Return index inode of the object by path
	int catalog_inode_index = path2inode(path, fs_desc.img, fs_desc.sblk, fs_desc.bg_decs_table, fs_desc.blk_buf);
	if (catalog_inode_index < 0)
		return catalog_inode_index;

	return 0;
}

struct reader_data_t
{
	uint8_t* out_buf;			// Buffer for readed data
    int in;						// Descriptors of opened src and dest files
	uint64_t unreaded_size;		// Size of file in bytes
};

static inline uint64_t copyer_calc_size_read(uint64_t unreaded_size,
											 uint32_t blk_size)
{
	return unreaded_size >= blk_size ? blk_size : unreaded_size;
}

static int reader(void* data_ptr,
				  off64_t blk_pos,
				  uint32_t blk_size)
{
    struct reader_data_t* data = (struct reader_data_t*)data_ptr;

	const int size_read = copyer_calc_size_read(data->unreaded_size, blk_size);
	if (blk_pos)
	{
		const off64_t pos = blk_pos * blk_size;
		CHECK_TRUE(pread64(data->in, data->out_buf, size_read, pos) == size_read);
	}
	else
	{
		memset(data->out_buf, 0, size_read);
	}

	data->out_buf += size_read;

	data->unreaded_size -= size_read;
	return data->unreaded_size == 0 ? BLK_VIEWER_END : BLK_VIEWER_CONT;
}

// TODO: stop to ignore offset
static int my_fs_read(const char* path,
					  char* buf,
					  size_t size,
					  off_t off,
					  struct fuse_file_info* file_info)
{
	UNUSED(off);
	UNUSED(file_info);
	if (check_abs_path(path) < 0)
		return -ENOENT;

	// Return index inode of the object by path
	int inode_index = path2inode(path, fs_desc.img, fs_desc.sblk, fs_desc.bg_decs_table, fs_desc.blk_buf);
	if (inode_index < 0)
		return inode_index;

	const uint32_t inode_pos = get_inode_pos(fs_desc.sblk, fs_desc.bg_decs_table, inode_index);
	struct ext2_inode inode = {};
	CHECK_NNEG(read_range(fs_desc.img, &inode, inode_pos, sizeof(inode)));
	if ((inode.i_mode & S_IFMT) != S_IFREG)
		return -EISDIR;

	struct reader_data_t copy_data = {
        .in = fs_desc.img,
		.unreaded_size = min(get_inode_file_size(&inode), size),
		.out_buf = (uint8_t*)buf
    };

	if (copy_data.unreaded_size)
		CHECK_NNEG(view_blocks(fs_desc.img, fs_desc.sblk, &inode, reader, &copy_data));

	return size;
}

static int my_fs_write(const char* path,
					   const char* src,
					   size_t size,
					   off_t off,
					   struct fuse_file_info* file_info)
{
	UNUSED(path);
	UNUSED(src);
	UNUSED(size);
	UNUSED(off);
	UNUSED(file_info);

	return -EROFS;
}

static int my_fs_access(const char* path,
					    int mode)
{
	UNUSED(path);

	return (mode & W_OK) ? -EROFS : 0;
}

static const struct fuse_operations ext2_ops = {
	.init 		= my_fs_init,
	.getattr 	= my_fs_getattr,
	.open  		= my_fs_open,
	.readdir 	= my_fs_readdir,
	.read 		= my_fs_read,
	.write 		= my_fs_write,
	.access 	= my_fs_access
};

int ext2fuse(int img,
			 const char *mntp)
{
	struct ext2_super_block* sblk = malloc(sizeof(struct ext2_super_block));
	CHECK_NNEG(read_sblk(img, sblk));

	const uint32_t blk_size = get_blk_size(sblk);

	uint8_t* bg_desc_table_buf = malloc(blk_size);
	CHECK_NNEG(read_blk(img, bg_desc_table_buf, sblk->s_first_data_block + 1, blk_size));
	struct ext2_group_desc* bg_decs_table = (struct ext2_group_desc*)bg_desc_table_buf;

	// Find folder inode index
	uint8_t* blk_buf = malloc(blk_size);

	struct fs_desc_t fs_desc_ref = {
		.img = img,
		.sblk = sblk,
		.bg_decs_table = bg_decs_table,
		.blk_buf = blk_buf
	};

	fs_desc = fs_desc_ref;

	#if 0
		// Return index inode of the object by path
		int catalog_inode_index = path2inode("/", img, &sblk, bg_decs_table, blk_buf);
		if (catalog_inode_index < 0)
			return -errno;

		// Check that it is catalog
		const uint32_t catalog_inode_pos = get_inode_pos(&sblk, bg_decs_table, catalog_inode_index);
		struct ext2_inode catalog_inode = {};
		CHECK_NNEG(read_range(img, &catalog_inode, catalog_inode_pos, sizeof(catalog_inode)));
		if ((catalog_inode.i_mode & S_IFMT) != S_IFDIR)
			return -ENOTDIR;

		// Fill file names of catalog
		struct fill_catalog_files_data_t data = {
			.blk_buf = blk_buf,
			.in = img
		};
		CHECK_NNEG(view_blocks(img, &sblk, &catalog_inode, readdir_filler, &data));

		return 0;
	#endif

	char *argv[] = {"a.out", (char *)mntp, NULL};
	return fuse_main(2, argv, &ext2_ops, NULL);
}
