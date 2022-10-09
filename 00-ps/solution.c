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

static int str2pid(const char *str, int *pid)
{
	errno = 0;
	char *endptr = NULL;
	*pid = strtol(str, &endptr, 10);

	/* Check for various possible errors */
	if (errno != 0 || endptr == str || *endptr != '\0')
	{
		return -1;
	}
	else
	{
		return 0;
	}
}

static void print_err(const char *path_dir, const char *path)
{
	const size_t path_dir_len = strlen(path_dir);
	const size_t path_len = strlen(path);

	char *err_msg = (char *)calloc(path_dir_len + path_len + 1, sizeof(char));
	if (err_msg == NULL)
	{
		perror("calloc");
		exit(EXIT_FAILURE);
	}

	strncpy(err_msg, path_dir, path_dir_len);
	strncpy(err_msg + path_dir_len, path, path_len);

	report_error(err_msg, errno);

	free(err_msg);
}

static ssize_t read_proc_file(const char *path, char *buf, size_t buf_size)
{
	int fd = open(path, O_RDONLY);
	if (fd == -1)
	{
		return -1;
	}

	ssize_t readen = 0, count = 0;
	while ((count = read(fd, buf + readen, buf_size - readen)) > 0)
	{
		readen += count;
	}

	close(fd);
	return errno == 0 ? readen : -1;
}

static size_t get_count_symb(const char *buf, size_t size, char symbol)
{
	size_t number = 0;
	const char *const buf_end = buf + size;
	while (1)
	{
		buf = memchr(buf, symbol, buf_end - buf - 1);
		if (buf == NULL)
		{
			return number;
		}

		++number;
		++buf;
	}
}

static void fill_ptrs(char *buf, size_t size, char symbol, char **ptrs)
{
	if (size == 0)
	{
		*ptrs = NULL;
		return;
	}

	char *const buf_end = buf + size;

	*ptrs++ = buf++;
	while (buf < buf_end)
	{
		buf = memchr(buf, symbol, buf_end - buf - 1);
		if (buf == NULL)
		{
			return;
		}

		*ptrs++ = ++buf;
	}
}

static char **get_ptrs(char *buf, size_t size)
{
	if (size == 0)
	{
		char **ptrs = (char **)calloc(1, sizeof(char *));
		if (ptrs == NULL)
		{
			perror("calloc");
			exit(EXIT_FAILURE);
		}

		*ptrs = NULL;
		return ptrs;
	}

	// Calc number of words
	size_t num_symb = 1 + get_count_symb(buf, size, '\0');

	char **ptrs = (char **)calloc(num_symb + 1, sizeof(char *));
	if (ptrs == NULL)
	{
		exit(EXIT_FAILURE);
	}

	fill_ptrs(buf, size, '\0', ptrs);
	return ptrs;
}

// pid_t pid, const char *exe, char **argv, char **envp
static void process_proc_dir(char *str_buf, const char *path)
{
	// Calc PID -------------------------------------------------------------------------
	pid_t pid = 0;
	if (str2pid(path, &pid) == -1)
	{
		print_err(str_buf, path);
		return;
	}

	strcat(str_buf, path);
	const size_t str_buf_save_size = strlen(str_buf);

	// const char* exe, char** argv -----------------------------------------------------
	strcat(str_buf, "/cmdline");

	static char cmdline_buf[1024 * 1024];
	const size_t cmdline_buf_size = sizeof(cmdline_buf);

	ssize_t read_size = read_proc_file(str_buf, cmdline_buf, cmdline_buf_size);
	if (read_size == -1)
	{
		report_error((const char *)str_buf, errno);
		return;
	}

	if ((size_t)(read_size + 1) > cmdline_buf_size)
	{
		errno = ENOMEM;
		exit(EXIT_FAILURE);
	}
	cmdline_buf[read_size] = '\0';
	const char *exe = cmdline_buf;
	char **argv = get_ptrs(cmdline_buf, read_size);

	// char** envp ----------------------------------------------------------------------
	str_buf[str_buf_save_size] = '\0';
	strcat(str_buf, "/environ");

	static char environ_buf[1024 * 1024];
	const size_t environ_buf_size = sizeof(environ_buf);

	read_size = read_proc_file(str_buf, environ_buf, environ_buf_size);
	if (read_size == -1)
	{
		report_error((const char *)str_buf, errno);
		free(argv);
		return;
	}

	if ((size_t)(read_size + 1) > environ_buf_size)
	{
		errno = ENOMEM;
		exit(EXIT_FAILURE);
	}
	environ_buf[read_size] = '\0';
	char **envp = get_ptrs(environ_buf, read_size);

	// Report
	report_process(pid, exe, argv, envp);

	free(argv);
	free(envp);
}

void ps(void)
{
	const char proc_dir_name[] = "/proc/";
	DIR *proc_dir = opendir(proc_dir_name);
	if (proc_dir == NULL)
	{
		report_error(proc_dir_name, errno);
		exit(EXIT_FAILURE);
	}

	// Init the buffer for work with paths
	char str_buf[256] = {0};
	const ssize_t proc_dir_name_len = strlen(proc_dir_name);
	strncpy(str_buf, proc_dir_name, proc_dir_name_len);

	// Read procs dir
	errno = 0;
	struct dirent *cur_obj = NULL;
	while ((cur_obj = readdir(proc_dir)) != NULL)
	{
		if (cur_obj->d_type == DT_DIR &&
			cur_obj->d_name != NULL && isdigit(cur_obj->d_name[0]))
		{
			process_proc_dir(str_buf, cur_obj->d_name);
			str_buf[proc_dir_name_len] = '\0';
		}
	}

	closedir(proc_dir);
	exit(EXIT_SUCCESS);
}
