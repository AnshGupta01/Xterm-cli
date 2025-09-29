#include "../include/myterm.h"

void process_command(Terminal *term, char *command) {
    // Remove leading/trailing whitespace
    command = trim_whitespace(command);

    if (strlen(command) == 0) return;

    // Check for built-in commands
    char **tokens = tokenize_command(command);
    if (!tokens) return;

    if (strcmp(tokens[0], "cd") == 0) {
        builtin_cd(tokens);
    } else if (strcmp(tokens[0], "history") == 0) {
        builtin_history(term);
    } else if (strcmp(tokens[0], "multiWatch") == 0) {
        builtin_multiwatch(term, tokens);
    } else if (strchr(command, '|')) {
        // Handle pipes
        handle_pipes(term, command);
    } else {
        // Execute external command
        execute_external_command(term, tokens);
    }

    free_tokens(tokens);
}

void execute_external_command(Terminal *term, char **args) {
    if (!args || !args[0]) return;

    int input_fd = -1, output_fd = -1;

    // Handle redirection
    handle_redirection(args, &input_fd, &output_fd);

    pid_t pid = fork();
    if (pid == 0) {
        // Child process

        // Handle input redirection
        if (input_fd != -1) {
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);
        }

        // Handle output redirection  
        if (output_fd != -1) {
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
        }

        // Execute command
        execvp(args[0], args);
        perror("execvp");
        exit(1);
    } else if (pid > 0) {
        // Parent process
        if (input_fd != -1) close(input_fd);
        if (output_fd != -1) close(output_fd);

        // Wait for child to complete
        int status;
        waitpid(pid, &status, 0);
    } else {
        perror("fork");
    }
}

void handle_redirection(char **args, int *input_fd, int *output_fd) {
    for (int i = 0; args[i]; i++) {
        if (strcmp(args[i], "<") == 0 && args[i+1]) {
            // Input redirection
            *input_fd = open(args[i+1], O_RDONLY);
            if (*input_fd == -1) {
                perror("open input file");
            }
            // Remove redirection from args
            free(args[i]);
            free(args[i+1]);
            args[i] = NULL;
            break;
        } else if (strcmp(args[i], ">") == 0 && args[i+1]) {
            // Output redirection
            *output_fd = open(args[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (*output_fd == -1) {
                perror("open output file");
            }
            // Remove redirection from args
            free(args[i]);
            free(args[i+1]);
            args[i] = NULL;
            break;
        }
    }
}

void handle_pipes(Terminal *term, char *command) {
    char ***commands = NULL;
    int num_commands = 0;

    parse_command(command, &commands, &num_commands);

    if (num_commands <= 1) return;

    int pipes[MAX_PIPES][2];
    pid_t pids[MAX_PIPES];

    // Create pipes
    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            return;
        }
    }

    // Create processes
    for (int i = 0; i < num_commands; i++) {
        pids[i] = fork();

        if (pids[i] == 0) {
            // Child process

            // Set up input
            if (i > 0) {
                dup2(pipes[i-1][0], STDIN_FILENO);
            }

            // Set up output
            if (i < num_commands - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            // Close all pipes in child
            for (int j = 0; j < num_commands - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            // Execute command
            execvp(commands[i][0], commands[i]);
            perror("execvp");
            exit(1);
        }
    }

    // Close all pipes in parent
    for (int i = 0; i < num_commands - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // Wait for all children
    for (int i = 0; i < num_commands; i++) {
        waitpid(pids[i], NULL, 0);
    }

    // Free commands
    for (int i = 0; i < num_commands; i++) {
        free_tokens(commands[i]);
    }
    free(commands);
}

void parse_command(char *command, char ***commands, int *num_commands) {
    *num_commands = 1;

    // Count pipes
    for (char *p = command; *p; p++) {
        if (*p == '|') (*num_commands)++;
    }

    *commands = malloc(*num_commands * sizeof(char**));

    char *cmd_start = command;
    int cmd_index = 0;

    for (char *p = command; *p; p++) {
        if (*p == '|') {
            *p = '\0';
            (*commands)[cmd_index++] = tokenize_command(cmd_start);
            cmd_start = p + 1;
            while (*cmd_start == ' ') cmd_start++; // Skip whitespace
        }
    }

    // Handle last command
    (*commands)[cmd_index] = tokenize_command(cmd_start);
}

char** tokenize_command(char *command) {
    char **tokens = malloc(MAX_ARGS * sizeof(char*));
    int token_count = 0;

    // Make a copy of the command to avoid modifying original
    char *cmd_copy = malloc(strlen(command) + 1);
    strcpy(cmd_copy, command);

    char *token = strtok(cmd_copy, " \t\n");
    while (token && token_count < MAX_ARGS - 1) {
        tokens[token_count] = malloc(strlen(token) + 1);
        strcpy(tokens[token_count], token);
        token_count++;
        token = strtok(NULL, " \t\n");
    }

    tokens[token_count] = NULL;
    free(cmd_copy);
    return tokens;
}

void free_tokens(char **tokens) {
    if (!tokens) return;

    for (int i = 0; tokens[i]; i++) {
        free(tokens[i]);
    }
    free(tokens);
}

void builtin_cd(char **args) {
    char *dir = args[1] ? args[1] : getenv("HOME");

    if (chdir(dir) != 0) {
        perror("cd");
    }
}

void builtin_history(Terminal *term) {
    Tab *tab = &term->tabs[term->current_tab];

    int start = (term->history_count > HISTORY_DISPLAY) ? 
                term->history_count - HISTORY_DISPLAY : 0;

    for (int i = start; i < term->history_count; i++) {
        char line[MAX_COMMAND_LENGTH + 16];
        snprintf(line, sizeof(line), "%d: %s\n", i + 1, term->history[i]);
        append_to_buffer(term, line);
    }
}

void builtin_multiwatch(Terminal *term, char **args) {
    if (!args[1]) {
        append_to_buffer(term, "Usage: multiWatch [\"cmd1\", \"cmd2\", ...]\n");
        return;
    }

    // Parse commands from args
    int num_cmds = 0;
    char *commands[MAX_ARGS];

    for (int i = 1; args[i] && num_cmds < MAX_ARGS; i++) {
        if (args[i][0] == '\"') {
            // Remove quotes
            commands[num_cmds] = args[i] + 1;
            int len = strlen(commands[num_cmds]);
            if (len > 0 && commands[num_cmds][len-1] == '\"') {
                commands[num_cmds][len-1] = '\0';
            }
            num_cmds++;
        }
    }

    if (num_cmds == 0) {
        append_to_buffer(term, "No valid commands found\n");
        return;
    }

    // Simple implementation - execute commands once and show output
    for (int i = 0; i < num_cmds; i++) {
        char output_header[256];
        time_t current_time = time(NULL);
        snprintf(output_header, sizeof(output_header), 
                "\"%s\" , %s----------------------------------------------------\n",
                commands[i], ctime(&current_time));
        append_to_buffer(term, output_header);

        // Execute command and capture output
        FILE *fp = popen(commands[i], "r");
        if (fp) {
            char output_line[256];
            while (fgets(output_line, sizeof(output_line), fp)) {
                append_to_buffer(term, output_line);
            }
            pclose(fp);
        }

        append_to_buffer(term, "----------------------------------------------------\n");
    }
}
