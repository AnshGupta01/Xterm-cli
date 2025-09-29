#include "io.h"
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

ssize_t safe_read(int fd, void* buf, size_t count) {
    ssize_t result;
    do {
        result = read(fd, buf, count);
    } while (result < 0 && errno == EINTR);
    return result;
}

ssize_t safe_write(int fd, const void* buf, size_t count) {
    ssize_t result;
    do {
        result = write(fd, buf, count);
    } while (result < 0 && errno == EINTR);
    return result;
}

int create_pipe(int pipefd[2]) {
    return pipe(pipefd);
}

void close_pipe(int pipefd[2]) {
    if (pipefd[0] >= 0) close(pipefd[0]);
    if (pipefd[1] >= 0) close(pipefd[1]);
}

int redirect_input(const char* filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) return -1;
    
    if (dup2(fd, STDIN_FILENO) < 0) {
        close(fd);
        return -1;
    }
    
    close(fd);
    return 0;
}

int redirect_output(const char* filename, int append) {
    int flags = O_WRONLY | O_CREAT;
    if (append) {
        flags |= O_APPEND;
    } else {
        flags |= O_TRUNC;
    }
    
    int fd = open(filename, flags, 0644);
    if (fd < 0) return -1;
    
    if (dup2(fd, STDOUT_FILENO) < 0) {
        close(fd);
        return -1;
    }
    
    close(fd);
    return 0;
}
