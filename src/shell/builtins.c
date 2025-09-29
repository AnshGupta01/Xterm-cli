#include "shell.h"
#include "../features/features.h"
#include "../gui/gui.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include<stdio.h>


static const char* builtin_commands[] = {
    "cd", "pwd", "exit", "history", "clear", "multiwatch", NULL
};

int is_builtin(const char* cmd) {
    for (int i = 0; builtin_commands[i]; i++) {
        if (strcmp(cmd, builtin_commands[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

int execute_builtin(Command* cmd) {
    if (!cmd || !cmd->argv[0]) return -1;
    
    if (strcmp(cmd->argv[0], "cd") == 0) {
        const char* dir = cmd->argc > 1 ? cmd->argv[1] : getenv("HOME");
        if (chdir(dir) != 0) {
            perror("cd");
            return -1;
        }
        return 0;
    }
    
    if (strcmp(cmd->argv[0], "pwd") == 0) {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd))) {
            if (g_gui) {
                gui_append_text(g_gui, cwd);
                gui_append_text(g_gui, "\n");
            }
        }
        return 0;
    }
    
    if (strcmp(cmd->argv[0], "exit") == 0) {
        if (g_gui) {
            g_gui->running = 0;
        }
        exit(0);
    }
    
    if (strcmp(cmd->argv[0], "clear") == 0) {
        if (g_gui) {
            gui_clear_screen(g_gui);
        }
        return 0;
    }
    
    if (strcmp(cmd->argv[0], "history") == 0) {
        return history_builtin(cmd->argv);
    }
    
    if (strcmp(cmd->argv[0], "multiwatch") == 0) {
        return multiwatch_builtin(cmd->argv);
    }
    
    return -1;
}