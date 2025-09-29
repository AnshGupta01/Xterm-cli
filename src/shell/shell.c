#include "shell.h"
#include "../io/io.h"
#include "../gui/gui.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

volatile sig_atomic_t g_interrupted = 0;
volatile pid_t g_foreground_pid = 0;

int shell_init(void) {
    return 0;
}

void shell_cleanup(void) {
    // Kill any remaining processes
}

ShellProcess* shell_create_process(void) {
    ShellProcess* proc = malloc(sizeof(ShellProcess));
    if (!proc) return NULL;
    
    memset(proc, 0, sizeof(ShellProcess));
    
    int in_pipe[2], out_pipe[2], err_pipe[2];
    
    if (create_pipe(in_pipe) < 0 || 
        create_pipe(out_pipe) < 0 || 
        create_pipe(err_pipe) < 0) {
        free(proc);
        return NULL;
    }
    
    pid_t pid = fork();
    if (pid < 0) {
        close_pipe(in_pipe);
        close_pipe(out_pipe);
        close_pipe(err_pipe);
        free(proc);
        return NULL;
    }
    
    if (pid == 0) {
        // Child process - become shell
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);
        
        close_pipe(in_pipe);
        close_pipe(out_pipe);
        close_pipe(err_pipe);
        
        // Start shell
        execl("/bin/bash", "bash", "--norc", "-i", NULL);
        execl("/bin/sh", "sh", "-i", NULL);
        _exit(127);
    }
    
    // Parent process
    close(in_pipe[0]);
    close(out_pipe[1]);
    close(err_pipe[1]);
    
    proc->pid = pid;
    proc->in_fd = in_pipe[1];
    proc->out_fd = out_pipe[0];
    proc->err_fd = err_pipe[0];
    proc->pgid = pid;
    proc->running = 1;
    
    set_nonblocking(proc->out_fd);
    set_nonblocking(proc->err_fd);
    
    return proc;
}

void shell_destroy_process(ShellProcess* proc) {
    if (!proc) return;
    
    if (proc->running && proc->pid > 0) {
        kill(proc->pid, SIGTERM);
        sleep(1);
        kill(proc->pid, SIGKILL);
        waitpid(proc->pid, NULL, 0);
    }
    
    if (proc->in_fd > 0) close(proc->in_fd);
    if (proc->out_fd > 0) close(proc->out_fd);
    if (proc->err_fd > 0) close(proc->err_fd);
    
    free(proc);
}

int shell_execute_line(const char* line, int in_fd, int out_fd) {
    Pipeline* pipeline = parse_command_line(line);
    if (!pipeline) return -1;
    
    // Handle built-ins first
    if (pipeline->count == 1 && is_builtin(pipeline->commands[0].argv[0])) {
        int result = execute_builtin(&pipeline->commands[0]);
        free_pipeline(pipeline);
        return result;
    }
    
    // Execute pipeline
    int result = 0;
    pid_t* pids = malloc(pipeline->count * sizeof(pid_t));
    int pipes[MAX_PIPES][2];
    
    // Create pipes
    for (int i = 0; i < pipeline->count - 1; i++) {
        if (create_pipe(pipes[i]) < 0) {
            result = -1;
            goto cleanup;
        }
    }
    
    // Execute commands
    for (int i = 0; i < pipeline->count; i++) {
        Command* cmd = &pipeline->commands[i];
        
        pid_t pid = fork();
        if (pid < 0) {
            result = -1;
            goto cleanup;
        }
        
        if (pid == 0) {
            // Child process
            setpgid(0, 0); // Create new process group
            
            // Set up input
            if (i == 0) {
                if (cmd->input_file) {
                    int fd = redirect_input(cmd->input_file);
                    if (fd < 0) _exit(126);
                }
            } else {
                dup2(pipes[i-1][0], STDIN_FILENO);
            }
            
            // Set up output
            if (i == pipeline->count - 1) {
                if (cmd->output_file) {
                    int fd = redirect_output(cmd->output_file, cmd->append_output);
                    if (fd < 0) _exit(126);
                }
            } else {
                dup2(pipes[i][1], STDOUT_FILENO);
            }
            
            // Close all pipe fds
            for (int j = 0; j < pipeline->count - 1; j++) {
                close_pipe(pipes[j]);
            }
            
            // Execute command
            execvp(cmd->argv[0], cmd->argv);
            _exit(127);
        }
        
        pids[i] = pid;
        if (i == 0) g_foreground_pid = pid;
    }
    
    // Close pipes in parent
    for (int i = 0; i < pipeline->count - 1; i++) {
        close_pipe(pipes[i]);
    }
    
    // Wait for processes
    if (!pipeline->background) {
        for (int i = 0; i < pipeline->count; i++) {
            int status;
            waitpid(pids[i], &status, 0);
        }
        g_foreground_pid = 0;
    }
    
cleanup:
    free(pids);
    free_pipeline(pipeline);
    return result;
}

void setup_signal_handlers(void) {
    signal(SIGINT, handle_sigint);
    signal(SIGTSTP, handle_sigtstp);
    signal(SIGPIPE, SIG_IGN);
}

void handle_sigint(int sig) {
    (void)sig;
    g_interrupted = 1;
    if (g_foreground_pid > 0) {
        kill(-g_foreground_pid, SIGINT);
    }
}

void handle_sigtstp(int sig) {
    (void)sig;
    if (g_foreground_pid > 0) {
        kill(-g_foreground_pid, SIGTSTP);
    }
}
