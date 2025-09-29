#include "features.h"
#include "../gui/gui.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <poll.h>
#include <stdio.h>
#include <fcntl.h>

#define MAX_WATCHED_COMMANDS 10

static struct {
    char command[MAX_LINE];
    pid_t pid;
    int fd;
    char temp_file[64];
} watched_commands[MAX_WATCHED_COMMANDS];

static int watch_count = 0;
static volatile int multiwatch_running = 0;

static void multiwatch_signal_handler(int sig) {
    (void)sig;
    multiwatch_running = 0;
}

int multiwatch_builtin(char** args) {
    if (!args[1]) {
        if (g_gui) {
            gui_append_text(g_gui, "Usage: multiwatch [\"cmd1\",\"cmd2\",...]\n");
        }
        return -1;
    }
    
    // Parse command list
    char* cmd_list = args[1];
    char* commands[MAX_WATCHED_COMMANDS];
    int cmd_count = 0;
    
    // Simple parsing - split by comma
    char* token = strtok(cmd_list, ",");
    while (token && cmd_count < MAX_WATCHED_COMMANDS) {
        // Remove quotes
        while (*token == '"' || *token == ' ') token++;
        char* end = token + strlen(token) - 1;
        while (end > token && (*end == '"' || *end == ' ')) {
            *end = '\0';
            end--;
        }
        
        commands[cmd_count++] = strdup(token);
        token = strtok(NULL, ",");
    }
    
    if (cmd_count == 0) {
        gui_append_text(g_gui, "No commands specified\n");
        return -1;
    }
    
    // Set up signal handler
    signal(SIGINT, multiwatch_signal_handler);
    multiwatch_running = 1;
    watch_count = cmd_count;
    
    // Start commands
    for (int i = 0; i < cmd_count; i++) {
        snprintf(watched_commands[i].temp_file, sizeof(watched_commands[i].temp_file),
                ".temp.%d.txt", getpid() + i);
        
        strncpy(watched_commands[i].command, commands[i], MAX_LINE - 1);
        
        pid_t pid = fork();
        if (pid == 0) {
            // Child: redirect output to temp file
            FILE* f = fopen(watched_commands[i].temp_file, "w");
            if (f) {
                dup2(fileno(f), STDOUT_FILENO);
                dup2(fileno(f), STDERR_FILENO);
                fclose(f);
            }
            
            execl("/bin/sh", "sh", "-c", commands[i], NULL);
            _exit(127);
        }
        
        watched_commands[i].pid = pid;
        watched_commands[i].fd = -1; // Will open later
        
        free(commands[i]);
    }
    
    // Monitor loop
    struct pollfd fds[MAX_WATCHED_COMMANDS];
    
    while (multiwatch_running) {
        int nfds = 0;
        
        // Open temp files and set up polling
        for (int i = 0; i < watch_count; i++) {
            if (watched_commands[i].fd < 0) {
                watched_commands[i].fd = open(watched_commands[i].temp_file, O_RDONLY | O_NONBLOCK);
            }
            
            if (watched_commands[i].fd >= 0) {
                fds[nfds].fd = watched_commands[i].fd;
                fds[nfds].events = POLLIN;
                nfds++;
            }
        }
        
        int ready = poll(fds, nfds, 1000);
        if (ready > 0) {
            for (int i = 0; i < watch_count; i++) {
                if (watched_commands[i].fd >= 0) {
                    char buffer[4096];
                    ssize_t n = read(watched_commands[i].fd, buffer, sizeof(buffer) - 1);
                    
                    if (n > 0) {
                        buffer[n] = '\0';
                        
                        // Get current time
                        time_t now = time(NULL);
                        struct tm* tm_info = localtime(&now);
                        char time_str[64];
                        strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
                        
                        // Format output
                        char header[256];
                        snprintf(header, sizeof(header),
                                "=== %s [%s] ===\n", watched_commands[i].command, time_str);
                        
                        if (g_gui) {
                            gui_append_text(g_gui, header);
                            gui_append_text(g_gui, buffer);
                            gui_append_text(g_gui, "========================\n");
                        }
                    }
                }
            }
        }
        
        // Check if any processes have finished
        for (int i = 0; i < watch_count; i++) {
            if (watched_commands[i].pid > 0) {
                int status;
                pid_t result = waitpid(watched_commands[i].pid, &status, WNOHANG);
                if (result > 0) {
                    watched_commands[i].pid = 0; // Mark as finished
                }
            }
        }
    }
    
    // Cleanup
    multiwatch_cleanup();
    
    if (g_gui) {
        gui_append_text(g_gui, "multiWatch terminated\n");
    }
    
    return 0;
}

void multiwatch_cleanup(void) {
    for (int i = 0; i < watch_count; i++) {
        if (watched_commands[i].pid > 0) {
            kill(watched_commands[i].pid, SIGTERM);
            waitpid(watched_commands[i].pid, NULL, 0);
        }
        
        if (watched_commands[i].fd >= 0) {
            close(watched_commands[i].fd);
            watched_commands[i].fd = -1;
        }
        
        unlink(watched_commands[i].temp_file);
    }
    
    watch_count = 0;
}
