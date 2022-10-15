#include <solution.h>

#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

const char g_file_name[] = "hello";
const char g_file_content[] = "hello, %d\n";

static void* my_fs_init(struct fuse_conn_info* conn, struct fuse_config* cfg)
{
	(void)conn;
	cfg->kernel_cache = 1;

	cfg->set_uid = 1;
	cfg->uid = getgid();

	cfg->set_gid = 1;
	cfg->gid = getgid();

	cfg->set_mode = 1;
	cfg->umask = ~S_IRUSR;

	return NULL;
}

static int my_fs_getattr(const char* path, struct stat* stbuf, struct fuse_file_info* file_info)
{
	(void)file_info;

	memset(stbuf, 0, sizeof(*stbuf));

	int res = 0;
	if (strcmp(path, "/") == 0)
	{
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	}
	else if (strcmp(path + 1, g_file_name) == 0)
	{
		stbuf->st_mode = S_IFREG | S_IRUSR;
		stbuf->st_nlink = 1;

		char buf[512] = {};
		struct fuse_context* ctx = fuse_get_context();
		snprintf(buf, sizeof(buf), g_file_content, ctx->pid);
		stbuf->st_size = strlen(buf);
	}
	else
	{
		res = -ENOENT;
	}

	return res;
}

static int my_fs_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
	off_t off, struct fuse_file_info* file_info, enum fuse_readdir_flags flags)
{
	(void)off;
	(void)file_info;
	(void)flags;

	if (strcmp(path, "/") != 0)
	{
		return -ENOENT;
	}

	filler(buf, ".", NULL, 0, 0);
	filler(buf, "..", NULL, 0, 0);
	filler(buf, g_file_name, NULL, 0, 0);

	return 0;
}

static int my_fs_open(const char* path, struct fuse_file_info* file_info)
{
	(void)file_info;

	if ((file_info->flags & O_ACCMODE) != O_RDONLY)
	{
		return -EROFS;
	}

	if (strcmp(path + 1, g_file_name) != 0)
	{
		return -ENOENT;
	}

	return 0;
}

static int my_fs_read(const char* path, char* buf, size_t size, off_t off, struct fuse_file_info* file_info)
{
	(void)file_info;

	if (strcmp(path + 1, g_file_name) != 0)
	{
		return -ENOENT;
	}

	char tmp_buf[512] = { 0 };
	struct fuse_context* ctx = fuse_get_context();
	snprintf(tmp_buf, sizeof(tmp_buf), g_file_content, ctx->pid);
	const size_t len = strlen(tmp_buf);

	if ((size_t)off < len)
	{
		if (off + size > len)
		{
			size = len - off;
		}

		memcpy(buf, tmp_buf + off, size);
	}
	else
	{
		size = 0;
	}

	return size;
}

static int my_fs_write(const char* path, const char* src, size_t size, off_t off, struct fuse_file_info* file_info)
{
	(void)src;
	(void)size;
	(void)off;
	(void)file_info;

	if (strcmp(path + 1, g_file_name) != 0)
	{
		return -EINVAL;
	}

	return -EROFS;
}

static int my_fs_rename(const char* path, const char* new_name, unsigned int flags)
{
	(void)new_name;
	(void)flags;

	if (strcmp(path + 1, g_file_name) != 0)
	{
		return -ENOENT;
	}

	return -EROFS;
}

static int my_fs_setxattr(const char* path, const char* name, const char* value, size_t size, int flags)
{
	(void)name;
	(void)flags;
	(void)value;
	(void)size;
	(void)flags;

	if (strcmp(path + 1, g_file_name) != 0)
	{
		return -ENOENT;
	}

	return -EROFS;
}
static int my_fs_removexattr(const char* path, const char* name)
{
	(void)name;

	if (strcmp(path + 1, g_file_name) != 0)
	{
		return -ENOENT;
	}

	return -EROFS;
}

static int my_fs_access(const char* path, int mode)
{
	(void)path;

	if ((mode & W_OK) != 0)
	{
		return -EROFS;
	}

	return 0;
}

static int my_fs_unlink(const char* path)
{
	(void)path;
	return -EROFS;
}

static const struct fuse_operations hellofs_ops = {
	  .init = my_fs_init
	, .getattr = my_fs_getattr
	, .readdir = my_fs_readdir
	, .open = my_fs_open
	, .read = my_fs_read
	, .write = my_fs_write
	, .rename = my_fs_rename
	, .setxattr = my_fs_setxattr
	, .removexattr = my_fs_removexattr
	, .access = my_fs_access
	, .unlink = my_fs_unlink
};

int helloworld(const char* mntp)
{
	char* argv[] = { "./a.out", (char*)mntp, NULL };
	return fuse_main(2, argv, &hellofs_ops, NULL);
}
