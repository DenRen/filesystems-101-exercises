#include <solution.h>

#include <liburing.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/types.h>

enum {
	TASK_READ,
	TASK_WRITE
};

struct task_t
{
	size_t offset;
	size_t size;
	int type;
};

static off_t get_file_size(int in)
{
	struct stat in_info = {};
	if (fstat(in, &in_info) == -1)
	{
		return -errno;
	}

	return in_info.st_size;
}

static int prepare_read_bufs(uint8_t** read_bufs, int num_bufs, size_t buf_size)
{
	if (read_bufs == NULL || num_bufs <= 0 || buf_size == 0)
	{
		return -EINVAL;
	}

	read_bufs[0] = malloc(num_bufs * buf_size);
	if (read_bufs[0] == NULL)
	{
		return -errno;
	}

	for (int i_buf = 1; i_buf < num_bufs; ++i_buf)
	{
		read_bufs[i_buf] = read_bufs[i_buf - 1] + buf_size;
	}

	return 0;
}

// contract: unreaded_size != 0
// return: read_size of task
static inline size_t get_read_task(struct task_t* task, size_t buf_size, size_t file_size, size_t unreaded_size)
{
	const size_t offset = file_size - unreaded_size;
	const size_t read_size = (offset + buf_size) <= file_size ? buf_size : unreaded_size;
	struct task_t read_task = {
		.offset = offset,
		.size = read_size,
		.type = TASK_READ
	};

	*task = read_task;
	return read_size;
}

// Contract: all params are valid
static int copy_impl(int in, int out,
					 uint8_t** const read_bufs, const int num_bufs, const size_t buf_size,
					 const size_t file_size, struct io_uring* ring)
{
	size_t unreaded_size = file_size;

	struct task_t read_tasks[num_bufs];
	int num_active_workers = 0;
	for (int i_worker = 0; i_worker < num_bufs; ++i_worker)
	{
		// Prepare read task
		if (unreaded_size == 0)
		{
			break;
		}

		// Create read req
		struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
		if (sqe == NULL)
		{
			break;
		}
		
		struct task_t* read_task = &read_tasks[i_worker];
		unreaded_size -= get_read_task(read_task, buf_size, file_size, unreaded_size);

		io_uring_prep_read(sqe, in, read_bufs[i_worker], read_task->size, read_task->offset);
		sqe->user_data = i_worker;

		++num_active_workers;
	}

	size_t unwrited_size = file_size;
	while (unwrited_size != 0)
	{
		if (io_uring_submit(ring) < 0)
		{
			perror("io_uring_submit");
			return -errno;
		}

		// Get and check cqe correct
		struct io_uring_cqe* cqe = NULL;
		if (io_uring_wait_cqe(ring, &cqe) < 0)
		{
			perror("io_uring_peek_cqe");
			return -errno;
		}

		if (cqe->res < 0)
		{
			perror("read");
			return -errno;
		}

		const int i_worker = cqe->user_data;
		struct task_t* task = &read_tasks[i_worker];
		if (task->size != (size_t)cqe->res)
		{
			printf("task->size: %lu, cqe->res: %lu\n", task->size, (size_t)cqe->res);
			perror("Incorrect size of read");
			return -errno;
		}
		io_uring_cqe_seen(ring, cqe);

		// Process task
		switch (task->type)
		{
			case TASK_READ:
			{
				// Create task on write
				task->type = TASK_WRITE;
				struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
				io_uring_prep_write(sqe, out, read_bufs[i_worker], task->size, task->offset);
				sqe->user_data = i_worker;
			} break;
			case TASK_WRITE:
			{
				unwrited_size -= task->size;

				// Prepare read task
				if (unreaded_size != 0)
				{
					// Create task on read
					unreaded_size -= get_read_task(task, buf_size, file_size, unreaded_size);

					// Create read req
					struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
					io_uring_prep_read(sqe, in, read_bufs[i_worker], task->size, task->offset);
					sqe->user_data = i_worker;
				}
				else
				{
					--num_active_workers;
				}
			} break;
			default:
			{
				perror("default");
				return -1;
			}
		};

	}

	if (io_uring_submit_and_wait(ring, num_active_workers) < 0)
	{
		perror("io_uring_submit_and_wait");
		return -errno;
	}

	return 0;
}

// todo: prepare size of file before write
int copy(int in, int out)
{
	const size_t buf_size = 512 * 1024;
	const int num_bufs = get_nprocs_conf() / 2;

	// Get size of file
	const off_t file_size = get_file_size(in);
	if (file_size < 0)
	{
		perror("get_file_size");
		return file_size;
	}

	// Truncate file
	if (ftruncate(out, file_size) < 0)
	{
		perror("ftruncate");
		return -errno;
	}

	// Prepare buffers for read
	uint8_t* read_bufs[num_bufs];
	int err = prepare_read_bufs(read_bufs, num_bufs, buf_size);
	if (err < 0)
	{
		perror("prepare_read_bufs");
		return err;
	}

	// Prepare io_uring
	struct io_uring_params params = {};
	struct io_uring ring;

	err = io_uring_queue_init_params(4, &ring, &params);
	if (err < 0)
	{
		perror("io_uring_queue_init_params");
		free(read_bufs[0]);
		return err;
	}

	err = copy_impl(in, out, read_bufs, num_bufs, buf_size, file_size, &ring);
	if (err < 0)
	{
		perror("copy_impl");
	}

	// Free all structures
	io_uring_queue_exit(&ring);
	free(read_bufs[0]);

	return err >= 0 ? 0 : err;
}
