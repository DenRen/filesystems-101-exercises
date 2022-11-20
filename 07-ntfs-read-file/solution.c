#include <solution.h>
#include <print_lib.h>

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

#define __timespec_defined

#include <ntfs-3g/bootsect.h>
#include <ntfs-3g/device_io.h>
#include <ntfs-3g/dir.h>

#define UNUSED(var) (void)var

static int open_fd_safe(struct ntfs_device *dev, int flags)
{
	UNUSED(dev);
	UNUSED(flags);
	return 0;
}

static int close_fd_safe(struct ntfs_device *dev)
{
	free(dev->d_private);
	return 0;
}

static void get_fd_safe_dev_ops(struct ntfs_device_operations* ops)
{
	*ops = ntfs_device_unix_io_ops;
	ops->open = open_fd_safe;
	ops->close = close_fd_safe;
}

static ntfs_inode* pathname_to_inode(ntfs_volume *vol, const char *_path)
{
	if (_path == NULL || *_path != '/')
	{
		errno = ENOENT;
		return NULL;
	}

	// Get root
	ntfs_inode* inode = ntfs_inode_open(vol, FILE_root);
	if (inode == NULL)
		return NULL;

	char* path = strdup(_path + 1);
	char* save_path = path;

	while(path && *path != '\0')
	{
		char* sep = strchr(path, '/');
		if (sep != NULL)
			*sep = '\0';

		ntfschar* unicode = NULL;
		int len_unicode = ntfs_mbstoucs(path, &unicode);
		if (len_unicode < 0)
			return NULL;
		
		u64 inode_num = ntfs_inode_lookup_by_name(inode, unicode, len_unicode);
		ntfs_inode_close(inode);
		free(unicode);
		if (inode_num == -1ul)
		{
			errno = ENOENT;
			return NULL;
		}
		
		inode_num = MREF(inode_num);
		inode = ntfs_inode_open(vol, inode_num);
		if (inode == NULL)
		{
			errno = ENOENT;
			return NULL;
		}

		path = sep == NULL ? NULL : ++sep;
	}

	free(save_path);
	return inode;
}

int dump_file(int img, const char *path, int out)
{
	// Notes:
	// 	* I implemented a bit hacky solution
	//	* I use goto only for exit with error
	//	  and don't forget to free the memory

	struct ntfs_device_operations ops = {};
	get_fd_safe_dev_ops(&ops);

	struct ntfs_device* dev = ntfs_device_alloc("", 0, &ops, NULL);
	if ((dev->d_private = malloc(sizeof(int))) == NULL)
		return -errno;
	*(int*)(dev->d_private) = img;

	ntfs_volume* vol = ntfs_device_mount(dev, NTFS_MNT_RDONLY);
	if (vol == NULL || ntfs_version_is_supported(vol) == -1)
	{
		ntfs_device_free(dev);
		return -errno;
	}
	errno = 0;

	// We need to have the self errno variable, because the NTFS API functions
	// change errno to 0 if all good
	int local_errno = 0;

	ntfs_inode* file_inode = pathname_to_inode(vol, path);
	if (file_inode == NULL)
	{
		local_errno = errno;
		goto umount;
	}
	if (file_inode->mrec->flags & MFT_RECORD_IS_DIRECTORY)
	{
		local_errno = ENOTDIR;
		goto umount;
	}

	ntfs_attr* na = ntfs_attr_open(file_inode, AT_DATA, AT_UNNAMED, 0);
	if (na == NULL)
	{
		local_errno = errno;
		goto close_inode;
	}

	u8 buf[NTFS_BUF_SIZE];
	s64 size = na->data_size;
	u64 offset = 0;
	while(size)
	{
		s64 size_read = ntfs_attr_pread(na, offset, NTFS_BUF_SIZE, buf);
		if (size_read == 0)
		{
			local_errno = errno;
			goto close_attr;
		}

		s64 size_write = write(out, buf, size_read);
		if (size_write != size_read)
		{
			local_errno = errno;
			goto close_attr;
		}

		size -= size_read;
		offset += size_read;
	}

close_attr:
	ntfs_attr_close(na);

close_inode:
	ntfs_inode_close(file_inode);

umount:
	ntfs_umount(vol, TRUE);

	if (local_errno)
		errno = local_errno;
	perror("le: ");

	return -(local_errno ? local_errno: errno);
}
