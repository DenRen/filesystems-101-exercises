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

void read_proc_fd(char* fd_path)
{
	DIR* fd_dir = opendir(fd_path);
	if (fd_dir == NULL)
	{
		report_error(fd_path, errno);
		return;
	}

	// Prepare buf
	char file_name_buf[2048] = { 0 };
	const size_t size_buf = sizeof(file_name_buf);
	const size_t path_len_begin = strlen(fd_path);

	struct dirent* cur_file = NULL;
	while ((cur_file = readdir(fd_dir)) != NULL)
	{
		if (cur_file->d_type == DT_LNK)
		{
			strcat(fd_path, cur_file->d_name);
			ssize_t len = readlink(fd_path, file_name_buf, size_buf);
			if (len == -1 || len == size_buf)
			{
				report_error(fd_path, errno);
			}
			else
			{
				file_name_buf[len] = '\0';
				report_file(file_name_buf);
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
	strncpy(str_buf, proc_dir_name, proc_dir_name_len);

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
			read_proc_fd(str_buf);

			str_buf[proc_dir_name_len] = '\0';
		}
	}

	closedir(proc_dir);
	exit(EXIT_SUCCESS);
}
