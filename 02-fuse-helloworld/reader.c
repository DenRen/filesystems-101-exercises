#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#if 0
int main ()
{
    const char path[] = "1/hello";
    int fd = open(path, O_RDWR);
    if (fd == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    struct stat fi = {0};
    if (fstat(fd, &fi) == -1)
    {
        perror("fstat");
        exit(EXIT_FAILURE);
    }
    size_t size = fi.st_size;
    char name[256] = {0};

#if 0
    read(fd, name, size);

    printf ("my_pid: %d\n", getpid());
    printf("%s", name);
#else
    int res = write(fd, name, 5);
    if (res == -1)
    {
        perror("write");
        printf("errno == EROFS: %d\n", errno == EROFS);
        exit(EXIT_FAILURE);
    }

#endif
    close (fd);
    exit(EXIT_SUCCESS);
}
#endif