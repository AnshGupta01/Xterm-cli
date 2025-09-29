#ifndef SHELL_H
#define SHELL_H

#include <sys/types.h>
#include <signal.h>

#define MAX_ARGS 64
#define MAX_PIPES 32

typedef struct {
    char** argv;
    int argc;
    char* input_file;
    char* output_file;
    int append_output;
} Command;

typedef struct {
    Command* commands;
    int count;
    int background;
} Pipeline;

typedef struct {
    pid_t pid;
    int in_fd;
    int out_fd;
    int err_fd;
    int pgid;
    int running;
} ShellProcess;

// Shell functions
int shell_init(void);
void shell_cleanup(void);
ShellProcess* shell_create_process(void);
void shell_destroy_process(ShellProcess* proc);
int shell_execute_line(const char* line, int in_fd, int out_fd);

// Parser functions
Pipeline* parse_command_line(const char* line);
void free_pipeline(Pipeline* pipeline);
Command* parse_command(char** tokens, int* index, int token_count);

// Built-in commands
int is_builtin(const char* cmd);
int execute_builtin(Command* cmd);

// Signal handling
void setup_signal_handlers(void);
void handle_sigint(int sig);
void handle_sigtstp(int sig);

extern volatile sig_atomic_t g_interrupted;
extern volatile pid_t g_foreground_pid;

#endif
