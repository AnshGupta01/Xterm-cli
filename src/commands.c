#include "xterm.h"
#include <errno.h>

// MultiWatch global variables (defined here, declared extern in header)
pid_t multiwatch_pids[10] = {0};
int   multiwatch_pipes[10] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
int   multiwatch_count = 0;
int   multiwatch_active = 0;

void execute_user_command(Terminal *term, char *command) {
    if (!command || strlen(command) == 0) return;
    
    Tab *current_tab = &term->tabs[term->current_tab_index];
    char *trimmed_command = trim_whitespace(command);
    
    if (strlen(trimmed_command) == 0) return;
    
    add_command_to_history(term, trimmed_command);
    save_history_to_file(term);
    
    char echo_line[MAX_CMD_LEN + 20];
    snprintf(echo_line, sizeof(echo_line), "user@myterm> %s\n", trimmed_command);
    add_text_to_buffer(current_tab, echo_line);
    
    // Check for built-in commands
    if (strcmp(trimmed_command, "history") == 0) {
        builtin_history_display(term);
        return;
    }
    
    if (strncmp(trimmed_command, "multiWatch", 10) == 0) {
        builtin_multiwatch(term, trimmed_command + 10);
        return;
    }
    
    if (strcmp(trimmed_command, "exit") == 0) {
        close_current_tab(term);
        return;
    }
    
    // External commands
    run_external_command(term, trimmed_command);
}

void run_external_command(Terminal *term, char *cmd_line) {
    Tab *current_tab = &term->tabs[term->current_tab_index];
    
    if (!current_tab->is_active || current_tab->input_pipe[1] < 0) return;
    
    char command_with_newline[MAX_CMD_LEN + 2];
    snprintf(command_with_newline, sizeof(command_with_newline), "%s\n", cmd_line);
    
    ssize_t bytes_written = write(current_tab->input_pipe[1], 
                                 command_with_newline, strlen(command_with_newline));
    
    if (bytes_written < 0) {
        add_text_to_buffer(current_tab, "Error: Failed to send command to shell\n");
    }
}

void builtin_multiwatch(Terminal *term, char *args) {
    Tab *current_tab = &term->tabs[term->current_tab_index];
    
    if (!args || strlen(trim_whitespace(args)) == 0) {
        add_text_to_buffer(current_tab, "Usage: multiWatch [\"cmd1\", \"cmd2\", \"cmd3\"]\n");
        return;
    }
    
    // Stop any existing multiwatch
    if (multiwatch_active) {
        for (int i = 0; i < multiwatch_count; i++) {
            if (multiwatch_pids[i] > 0) {
                kill(multiwatch_pids[i], SIGKILL);
                waitpid(multiwatch_pids[i], NULL, WNOHANG);
                multiwatch_pids[i] = 0;
            }
            if (multiwatch_pipes[i] >= 0) {
                close(multiwatch_pipes[i]);
                multiwatch_pipes[i] = -1;
            }
        }
        multiwatch_active = 0;
        multiwatch_count = 0;
    }
    
    // Parse commands
    char commands[10][MAX_CMD_LEN];
    int command_count = 0;
    
    char *trimmed_args = trim_whitespace(args);
    char *start = strchr(trimmed_args, '[');
    char *end = strrchr(trimmed_args, ']');
    
    if (!start || !end || start >= end) {
        add_text_to_buffer(current_tab, "Error: Use format multiWatch [\"cmd1\", \"cmd2\"]\n");
        return;
    }
    
    start++;
    *end = '\0';
    
    char *token = strtok(start, ",");
    while (token && command_count < 10) {
        token = trim_whitespace(token);
        if (token[0] == '"') token++;
        char *quote_end = strrchr(token, '"');
        if (quote_end) *quote_end = '\0';
        
        if (strlen(token) > 0) {
            strncpy(commands[command_count], token, MAX_CMD_LEN - 1);
            commands[command_count][MAX_CMD_LEN - 1] = '\0';
            command_count++;
        }
        token = strtok(NULL, ",");
    }
    
    if (command_count == 0) {
        add_text_to_buffer(current_tab, "No valid commands found\n");
        return;
    }
    
    // Start multiwatch processes
    add_text_to_buffer(current_tab, "Starting multiWatch (Ctrl+C to stop)...\n");
    draw_terminal_window(term);
    
    for (int i = 0; i < command_count; i++) {
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("pipe failed");
            continue;
        }
        
        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            dup2(pipefd[1], STDERR_FILENO);
            close(pipefd[1]);
            
            // Disable buffering for immediate output
            setvbuf(stdout, NULL, _IONBF, 0);
            setvbuf(stderr, NULL, _IONBF, 0);
            
            printf("[multiWatch child %d started]\n", i);
            fflush(stdout);
            
            // Execution loop - run 10 times max
            for (int loop = 0; loop < 10; loop++) {
                time_t now = time(NULL);
                char *timestr = ctime(&now);
                timestr[strlen(timestr)-1] = '\0';
                
                printf("%s, %s\n", commands[i], timestr);
                printf("----------------------------------------------------\n");
                fflush(stdout);
                
                // Use popen to capture command output reliably
                FILE *fp = popen(commands[i], "r");
                if (fp) {
                    char line[256];
                    while (fgets(line, sizeof(line), fp)) {
                        fputs(line, stdout);
                    }
                    pclose(fp);
                } else {
                    printf("Error executing command: %s\n", commands[i]);
                }
                
                printf("----------------------------------------------------\n");
                fflush(stdout);
                
                sleep(2);
            }
            printf("[multiWatch child %d finished]\n", i);
            fflush(stdout);
            exit(0);
            
        } else if (pid > 0) {
            // Parent process
            close(pipefd[1]);
            
            // Make read end non-blocking
            int flags = fcntl(pipefd[0], F_GETFL, 0);
            fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);
            
            multiwatch_pids[i] = pid;
            multiwatch_pipes[i] = pipefd[0];
            multiwatch_count++;
        } else {
            perror("fork failed");
            close(pipefd[0]);
            close(pipefd[1]);
        }
    }
    
    if (multiwatch_count > 0) {
        multiwatch_active = 1;
        term->multiwatch_running = 1;
    }
}

void check_multiwatch_output(Terminal *term) {
    if (!multiwatch_active) return;
    
    Tab *current_tab = &term->tabs[term->current_tab_index];
    int still_active = 0;
    
    for (int i = 0; i < multiwatch_count; i++) {
        if (multiwatch_pipes[i] >= 0) {
            char buffer[512];
            ssize_t bytes;
            
            // Read all available data (drain pipe)
            while ((bytes = read(multiwatch_pipes[i], buffer, sizeof(buffer) - 1)) > 0) {
                buffer[bytes] = '\0';
                add_text_to_buffer(current_tab, buffer);
                still_active = 1;
            }
            
            if (bytes == 0) {
                // Process finished (EOF)
                close(multiwatch_pipes[i]);
                multiwatch_pipes[i] = -1;
                waitpid(multiwatch_pids[i], NULL, WNOHANG);
                multiwatch_pids[i] = 0;
            } else if (bytes < 0 && errno == EAGAIN) {
                // No data available but process still running
                still_active = 1;
            }
        }
    }
    
    if (!still_active) {
        // All processes finished
        multiwatch_active = 0;
        multiwatch_count = 0;
        term->multiwatch_running = 0;
        add_text_to_buffer(current_tab, "multiWatch completed\n");
    }
}

void builtin_history_display(Terminal *term) {
    Tab *current_tab = &term->tabs[term->current_tab_index];
    
    // Calculate available display lines
    int available_lines = (WINDOW_HEIGHT - 150) / 16;
    int max_history_lines = available_lines - 5;
    
    if (max_history_lines < 10) max_history_lines = 10;
    if (max_history_lines > 50) max_history_lines = 50;
    
    add_text_to_buffer(current_tab, "Command History:\n");
    
    if (term->history_count == 0) {
        add_text_to_buffer(current_tab, "No commands in history\n");
        return;
    }
    
    int start_index = (term->history_count > max_history_lines) ? 
                      term->history_count - max_history_lines : 0;
    int displayed_count = term->history_count - start_index;
    
    char header[128];
    snprintf(header, sizeof(header), "Showing %d most recent commands (out of %d total):\n", 
             displayed_count, term->history_count);
    add_text_to_buffer(current_tab, header);
    add_text_to_buffer(current_tab, "----------------------------------------\n");
    
    for (int i = start_index; i < term->history_count; i++) {
        char history_line[MAX_CMD_LEN + 20];
        snprintf(history_line, sizeof(history_line), "%4d: %s\n", 
                i + 1, term->command_history[i]);
        add_text_to_buffer(current_tab, history_line);
    }
    
    if (term->history_count > max_history_lines) {
        char footer[128];
        snprintf(footer, sizeof(footer), "----------------------------------------\n");
        add_text_to_buffer(current_tab, footer);
        snprintf(footer, sizeof(footer), "(%d older commands not shown)\n", 
                term->history_count - max_history_lines);
        add_text_to_buffer(current_tab, footer);
    }
}

void builtin_search_history(Terminal *term) {
    Tab *current_tab = &term->tabs[term->current_tab_index];
    
    if (strlen(term->search_term) == 0) {
        add_text_to_buffer(current_tab, "No search term provided\n");
        return;
    }
    
    // Search for exact match first
    for (int i = term->history_count - 1; i >= 0; i--) {
        if (strcmp(term->command_history[i], term->search_term) == 0) {
            snprintf(current_tab->current_command, MAX_CMD_LEN,
                    "user@myterm> %s", term->command_history[i]);
            current_tab->cursor_position = strlen(current_tab->current_command);
            return;
        }
    }
    
    // Search for partial match
    int best_match = -1;
    if (strlen(term->search_term) >= 2) {
        for (int i = term->history_count - 1; i >= 0; i--) {
            if (strstr(term->command_history[i], term->search_term)) {
                best_match = i;
                break;
            }
        }
    }
    
    if (best_match >= 0) {
        snprintf(current_tab->current_command, MAX_CMD_LEN,
                "user@myterm> %s", term->command_history[best_match]);
        current_tab->cursor_position = strlen(current_tab->current_command);
    } else {
        add_text_to_buffer(current_tab, "No match for search term in history\n");
    }
}

void autocomplete_filename(Terminal *term, char *partial_command) {
    Tab *current_tab = &term->tabs[term->current_tab_index];
    
    char *last_space = strrchr(partial_command, ' ');
    char *filename_part = last_space ? last_space + 1 : partial_command;
    
    if (strlen(filename_part) == 0) return;
    
    DIR *directory = opendir(".");
    if (!directory) return;
    
    char matches[20][256];
    int match_count = 0;
    
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL && match_count < 20) {
        if (entry->d_name[0] == '.' && filename_part[0] != '.') continue;
        
        if (strncmp(entry->d_name, filename_part, strlen(filename_part)) == 0) {
            strcpy(matches[match_count], entry->d_name);
            match_count++;
        }
    }
    closedir(directory);
    
    if (match_count == 1) {
        if (last_space) {
            *(last_space + 1) = '\0';
            snprintf(current_tab->current_command, MAX_CMD_LEN,
                    "user@myterm> %s%s", partial_command, matches[0]);
        } else {
            snprintf(current_tab->current_command, MAX_CMD_LEN,
                    "user@myterm> %s", matches[0]);
        }
    } else if (match_count > 1) {
        add_text_to_buffer(current_tab, "\nMatching files:\n");
        
        for (int i = 0; i < match_count; i += 3) {
            char match_line[256] = "";
            for (int j = 0; j < 3 && i + j < match_count; j++) {
                char item[32];
                snprintf(item, sizeof(item), "%-20s", matches[i + j]);
                strcat(match_line, item);
            }
            strcat(match_line, "\n");
            add_text_to_buffer(current_tab, match_line);
        }
    }
}

void save_history_to_file(Terminal *term) {
    FILE *history_file = fopen("myterm_history.txt", "w");
    if (!history_file) return;
    
    fprintf(history_file, "# MyTerm Command History\n");
    time_t now = time(NULL);
    fprintf(history_file, "# Generated on: %s", ctime(&now));
    
    for (int i = 0; i < term->history_count; i++) {
        fprintf(history_file, "%s\n", term->command_history[i]);
    }
    
    fclose(history_file);
}

void load_history_from_file(Terminal *term) {
    FILE *history_file = fopen("myterm_history.txt", "r");
    if (!history_file) return;
    
    char line[MAX_CMD_LEN];
    while (fgets(line, sizeof(line), history_file) && term->history_count < MAX_HISTORY) {
        if (line[0] == '#') continue;
        
        char *newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        if (strlen(line) > 0) {
            strncpy(term->command_history[term->history_count], line, MAX_CMD_LEN - 1);
            term->command_history[term->history_count][MAX_CMD_LEN - 1] = '\0';
            term->history_count++;
        }
    }
    
    fclose(history_file);
    term->history_position = term->history_count;
}

void setup_signal_handlers() {
    signal(SIGINT, handle_sigint);
    signal(SIGTSTP, handle_sigtstp);
    signal(SIGCHLD, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
}

void handle_sigint(int sig) {
    if (g_terminal && g_terminal->current_tab_index < g_terminal->total_tabs) {
        Tab *current_tab = &g_terminal->tabs[g_terminal->current_tab_index];
        if (current_tab->is_active && current_tab->shell_process_id > 0) {
            kill(current_tab->shell_process_id, SIGINT);
        }
    }
}

void handle_sigtstp(int sig) {
    if (g_terminal && g_terminal->current_tab_index < g_terminal->total_tabs) {
        Tab *current_tab = &g_terminal->tabs[g_terminal->current_tab_index];
        if (current_tab->is_active && current_tab->shell_process_id > 0) {
            kill(current_tab->shell_process_id, SIGTSTP);
        }
    }
}