#include <solution.h>

#include <fuse.h>
#include <string.h>
#include <errno.h>

const char g_file_name[] = "hello";
const char g_file_content[] = "Hello fs world!\n";

static void* my_fs_init(struct fuse_conn_info* conn, struct fuse_config* cfg)
{
	(void) conn;
	cfg->kernel_cache = 1;

	const pid_t pid = getpid();
	cfg->set_uid = 1;
	cfg->uid = pid;

	cfg->set_gid = 1;
	cfg->gid = pid;

	return NULL;
}

static int my_fs_getattr(const char* path, struct stat* stbuf, struct fuse_file_info* file_info)
{
	(void) file_info;

	memset(stbuf, 0, sizeof(*stbuf));

	int res = 0;
	if (strcmp(path, "/") == 0)
	{
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	}
	else if (strcmp(path + 1, g_file_name) == 0)
	{
		stbuf->st_mode = S_IFREG | 0666;
		stbuf->st_nlink = 1;
		stbuf->st_size = strlen(g_file_content);
	}
	else
	{
		res = -ENOENT;
	}
	
	return res;
}

static int my_fs_open(const char* path, struct fuse_file_info* file_info)
{
	if (strcmp(path + 1, g_file_name) != 0)
	{
		return -ENOENT;
	}

	if ((file_info->flags & O_ACCMODE) != O_RDONLY)
	{
		return -EACCES;
	}

	return 0;
}

static int my_fs_read(const char* path, char* buf, size_t size, off_t off, struct fuse_file_info* file_info)
{
	(void) file_info;

	if (strcmp(path + 1, g_file_name) != 0)
	{
		return -ENOENT;
	}

	const ssize_t len = strlen(g_file_content);
	if (off < len)
	{
		if (off + size > len)
		{
			size = len - off;
		}

		memcpy(buf, g_file_content + off, size);
	}
	else
	{
		size = 0;
	}

	return size;
}

static int my_fs_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
						 off_t off, struct fuse_file_info* file_info, enum fuse_readdir_flags flags)
{
	(void) off;
	(void) file_info;
	(void) flags;

	if (strcmp(path, "/") != 0)
	{
		return -ENOENT;
	}

	filler(buf, ".", NULL, 0, 0);
	filler(buf, "..", NULL, 0, 0);
	filler(buf, g_file_name, NULL, 0, 0);

	return 0;
}

static const struct fuse_operations hellofs_ops = {
	  .init 	= my_fs_init
	, .getattr 	= my_fs_getattr
	, .open 	= my_fs_open
	, .read 	= my_fs_read
	, .readdir	= my_fs_readdir
};

int helloworld(const char *mntp)
{
#if 0
	char *argv[] = {"exercise", "-f", (char *)mntp, NULL};
	reurn fuse_main(3, argv, &hellofs_ops, NULL);
#else
	char *argv[] = {"./a.out", (char *)mntp, NULL};
	return fuse_main(2, argv, &hellofs_ops, NULL);
#endif
}
