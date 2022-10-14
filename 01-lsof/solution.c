#include <solution.h>
#include <solution.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>

struct buf_t
{
	char* ptr;
	size_t size;	
};

void buf_create(struct buf_t* buf, size_t size)
{
	buf->ptr = calloc(size, sizeof(char));
	if (buf->ptr == NULL)
	{
		perror("calloc");
		exit(EXIT_FAILURE);
	}
	buf->size = size;
}

void buf_reserve(struct buf_t* buf, size_t new_size)
{
	if (new_size > buf->size)
	{
		buf->ptr = realloc(buf->ptr, new_size * sizeof(char));
		if (buf->ptr == NULL)
		{
			perror("realloc");
			exit(EXIT_FAILURE);
		}
		buf->size = new_size;
	}
}

void buf_free(struct buf_t* buf)
{
	free(buf->ptr);
	buf->ptr = NULL;
	buf->size = 0;
}

// contract: size buf must be minimum PATH_MAX
void read_proc_fd(struct buf_t* buf, char* fd_path)
{
	DIR* fd_dir = opendir(fd_path);
	if (fd_dir == NULL)
	{
		report_error(fd_path, errno);
		return;
	}

	// Prepare buf
	const size_t path_len_begin = strlen(fd_path);

	struct dirent* cur_file = NULL;
	while ((cur_file = readdir(fd_dir)) != NULL)
	{
		if (cur_file->d_type == DT_LNK)
		{
			strcat(fd_path, cur_file->d_name);

			// Get size for prepare buf
			struct stat info = {0};
			if (lstat(fd_path, &info) == -1)
			{
				report_error(fd_path, errno);
			}
			else
			{
				const size_t buf_min_size = info.st_size + 1;
				buf_reserve(buf, buf_min_size);

				ssize_t len = readlink(fd_path, buf->ptr, buf->size);
				if (len == -1 || (size_t)len == buf->size)
				{
					report_error(fd_path, errno);
				}
				else
				{
					buf->ptr[len] = '\0';
					report_file(buf->ptr);
				}
			}

			fd_path[path_len_begin] = '\0';
		}
	}

	closedir(fd_dir);
}

void lsof(void)
{
	const char proc_dir_name[] = "/proc/";
	DIR* proc_dir = opendir(proc_dir_name);
	if (proc_dir == NULL)
	{
		report_error(proc_dir_name, errno);
		exit(EXIT_FAILURE);
	}

	// Init the buffer for work with paths
	char str_buf[256] = { 0 };
	const ssize_t proc_dir_name_len = strlen(proc_dir_name);
	strcpy(str_buf, proc_dir_name);

	struct buf_t buf = {};
	buf_create(&buf, PATH_MAX);

	// Read procs dir
	errno = 0;
	struct dirent* cur_obj = NULL;
	while ((cur_obj = readdir(proc_dir)) != NULL)
	{
		if (cur_obj->d_type == DT_DIR &&
			cur_obj->d_name != NULL && isdigit(cur_obj->d_name[0]))
		{
			strcat(str_buf, cur_obj->d_name);
			
			strcat(str_buf, "/fd/");
			read_proc_fd(&buf, str_buf);
			str_buf[proc_dir_name_len] = '\0';
		}
	}

	buf_free(&buf);
	closedir(proc_dir);
	exit(EXIT_SUCCESS);
}
