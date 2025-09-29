#include "shell.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char** tokenize(const char* line, int* count) {
    char** tokens = malloc(MAX_ARGS * sizeof(char*));
    *count = 0;
    
    char* line_copy = strdup(line);
    char* token = strtok(line_copy, " \t\n");
    
    while (token && *count < MAX_ARGS - 1) {
        tokens[*count] = strdup(token);
        (*count)++;
        token = strtok(NULL, " \t\n");
    }
    
    tokens[*count] = NULL;
    free(line_copy);
    return tokens;
}

static void free_tokens(char** tokens, int count) {
    for (int i = 0; i < count; i++) {
        free(tokens[i]);
    }
    free(tokens);
}

Pipeline* parse_command_line(const char* line) {
    if (!line || !*line) return NULL;
    
    // Check for background
    int background = 0;
    char* line_copy = strdup(line);
    char* amp = strrchr(line_copy, '&');
    if (amp) {
        *amp = '\0';
        background = 1;
    }
    
    Pipeline* pipeline = malloc(sizeof(Pipeline));
    memset(pipeline, 0, sizeof(Pipeline));
    pipeline->background = background;
    
    // Split by pipes
    char* pipe_token = strtok(line_copy, "|");
    int cmd_count = 0;
    
    pipeline->commands = malloc(MAX_PIPES * sizeof(Command));
    
    while (pipe_token && cmd_count < MAX_PIPES) {
        // Parse individual command
        int token_count;
        char** tokens = tokenize(pipe_token, &token_count);
        
        if (token_count > 0) {
            Command* cmd = &pipeline->commands[cmd_count];
            memset(cmd, 0, sizeof(Command));
            
            // Parse tokens for redirections
            cmd->argv = malloc((token_count + 1) * sizeof(char*));
            int arg_count = 0;
            
            for (int i = 0; i < token_count; i++) {
                if (strcmp(tokens[i], "<") == 0 && i + 1 < token_count) {
                    cmd->input_file = strdup(tokens[++i]);
                } else if (strcmp(tokens[i], ">") == 0 && i + 1 < token_count) {
                    cmd->output_file = strdup(tokens[++i]);
                    cmd->append_output = 0;
                } else if (strcmp(tokens[i], ">>") == 0 && i + 1 < token_count) {
                    cmd->output_file = strdup(tokens[++i]);
                    cmd->append_output = 1;
                } else {
                    cmd->argv[arg_count++] = strdup(tokens[i]);
                }
            }
            
            cmd->argv[arg_count] = NULL;
            cmd->argc = arg_count;
            cmd_count++;
        }
        
        free_tokens(tokens, token_count);
        pipe_token = strtok(NULL, "|");
    }
    
    pipeline->count = cmd_count;
    free(line_copy);
    
    if (cmd_count == 0) {
        free_pipeline(pipeline);
        return NULL;
    }
    
    return pipeline;
}

void free_pipeline(Pipeline* pipeline) {
    if (!pipeline) return;
    
    for (int i = 0; i < pipeline->count; i++) {
        Command* cmd = &pipeline->commands[i];
        
        if (cmd->argv) {
            for (int j = 0; j < cmd->argc; j++) {
                free(cmd->argv[j]);
            }
            free(cmd->argv);
        }
        
        free(cmd->input_file);
        free(cmd->output_file);
    }
    
    free(pipeline->commands);
    free(pipeline);
}
