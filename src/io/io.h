#ifndef IO_H
#define IO_H

#include <sys/types.h>

// I/O utilities
int set_nonblocking(int fd);
ssize_t safe_read(int fd, void* buf, size_t count);
ssize_t safe_write(int fd, const void* buf, size_t count);
int create_pipe(int pipefd[2]);
void close_pipe(int pipefd[2]);

// File operations
int redirect_input(const char* filename);
int redirect_output(const char* filename, int append);

#endif
