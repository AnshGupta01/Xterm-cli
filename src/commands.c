#include "xterm.h"
#include <sys/stat.h>
#include <pwd.h>

// MultiWatch globals 
pid_t multiwatch_pids[10] = {0};
int multiwatch_pipes[10] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
int multiwatch_count = 0;
int multiwatch_active = 0;

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

void save_history_to_file(Terminal *term) {
    FILE *history_file = fopen("xterm_history.txt", "w");
    if (!history_file) return;
    
    fprintf(history_file, "# XTerm Command History\n");
    time_t current_time = time(NULL);
    fprintf(history_file, "# Generated on: %s", ctime(&current_time));
    
    for (int i = 0; i < term->history_count; i++) {
        fprintf(history_file, "%s\n", term->command_history[i]);
    }
    fclose(history_file);
}

void load_history_from_file(Terminal *term) {
    FILE *history_file = fopen("xterm_history.txt", "r");
    if (!history_file) return;
    
    char line_buffer[MAX_CMD_LEN];
    while (fgets(line_buffer, sizeof(line_buffer), history_file) && term->history_count < MAX_HISTORY) {
        if (line_buffer[0] == '#') continue;  // Skip comments
        
        char *newline_pos = strchr(line_buffer, '\n');
        if (newline_pos) *newline_pos = '\0';
        if (strlen(line_buffer) > 0) {
            strncpy(term->command_history[term->history_count], line_buffer, MAX_CMD_LEN - 1);
            term->command_history[term->history_count][MAX_CMD_LEN - 1] = '\0';
            term->history_count++;
        }
    }
    fclose(history_file);
    term->history_position = term->history_count;
}

void builtin_history_display(Terminal *term) {
    Tab *current_tab = &term->tabs[term->current_tab_index];
    
    if (term->history_count == 0) {
        add_text_to_buffer(current_tab, "No commands in history\n");
        return;
    }
    
    enter_scroll_mode(current_tab, "Command History");
    ScrollView *scroll = &current_tab->scroll_view;
    
    char header_text[256];
    snprintf(header_text, sizeof(header_text), "Total commands in history: %d", term->history_count);
    add_line_to_scroll(scroll, header_text);
    add_line_to_scroll(scroll, "=======================================");
    add_line_to_scroll(scroll, "");
    
    // Show recent commands (up to HISTORY_DISPLAY)
    int commands_to_show = (term->history_count > HISTORY_DISPLAY) ? HISTORY_DISPLAY : term->history_count;
    int start_index = term->history_count - commands_to_show;
    
    for (int i = start_index; i < term->history_count; i++) {
        char history_line[MAX_CMD_LEN + 20];
        snprintf(history_line, sizeof(history_line), "%4d: %s", i + 1, term->command_history[i]);
        add_line_to_scroll(scroll, history_line);
    }
    
    if (term->history_count > HISTORY_DISPLAY) {
        add_line_to_scroll(scroll, "");
        add_line_to_scroll(scroll, "=======================================");
        char footer_text[256];
        snprintf(footer_text, sizeof(footer_text), "(%d older commands not shown)", term->history_count - HISTORY_DISPLAY);
        add_line_to_scroll(scroll, footer_text);
        add_line_to_scroll(scroll, "Use PageUp/PageDown or Arrow keys to scroll");
        add_line_to_scroll(scroll, "Press ESC to exit history view");
    }
    
    scroll_to_bottom(scroll);
}

void builtin_search_history(Terminal *term) {
    Tab *current_tab = &term->tabs[term->current_tab_index];
    if (strlen(term->search_term) == 0) {
        add_text_to_buffer(current_tab, "No search term provided\n");
        return;
    }
    
    // Try exact match first
    for (int i = term->history_count - 1; i >= 0; i--) {
        if (strcmp(term->command_history[i], term->search_term) == 0) {
            snprintf(current_tab->current_command, MAX_CMD_LEN,
                     "anshgpt@xterm> %s", term->command_history[i]);
            current_tab->cursor_position = strlen(current_tab->current_command);
            return;
        }
    }
    
    // Try partial match
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
                 "anshgpt@xterm> %s", term->command_history[best_match]);
        current_tab->cursor_position = strlen(current_tab->current_command);
    } else {
        add_text_to_buffer(current_tab, "No match found in history\n");
    }
}

void autocomplete_filename(Terminal *term, char *partial_command) {
    Tab *current_tab = &term->tabs[term->current_tab_index];
    char *last_space = strrchr(partial_command, ' ');
    char *filename_part = last_space ? last_space + 1 : partial_command;
    
    if (strlen(filename_part) == 0) return;
    
    DIR *current_dir = opendir(".");
    if (!current_dir) return;
    
    char matches[20][256];
    int match_count = 0;
    struct dirent *entry;
    
    while ((entry = readdir(current_dir)) != NULL && match_count < 20) {
        if (entry->d_name[0] == '.' && filename_part[0] != '.') continue;
        if (strncmp(entry->d_name, filename_part, strlen(filename_part)) == 0) {
            strncpy(matches[match_count], entry->d_name, sizeof(matches[match_count]) - 1);
            matches[match_count][sizeof(matches[match_count]) - 1] = '\0';
            match_count++;
        }
    }
    closedir(current_dir);
    
    if (match_count == 1) {
        // Single match - complete it
        if (last_space) {
            *(last_space + 1) = '\0';
            snprintf(current_tab->current_command, MAX_CMD_LEN,
                     "anshgpt@xterm> %s%s", partial_command, matches[0]);
        } else {
            snprintf(current_tab->current_command, MAX_CMD_LEN,
                     "anshgpt@xterm> %s", matches[0]);
        }
    } else if (match_count > 1) {
        // Multiple matches - show them in scroll mode
        enter_scroll_mode(current_tab, "File Completion Matches");
        ScrollView *scroll = &current_tab->scroll_view;
        
        char header[256];
        snprintf(header, sizeof(header), "Matching files for '%s':", filename_part);
        add_line_to_scroll(scroll, header);
        add_line_to_scroll(scroll, "========================");
        add_line_to_scroll(scroll, "");
        
        for (int i = 0; i < match_count; i++) {
            add_line_to_scroll(scroll, matches[i]);
        }
        
        add_line_to_scroll(scroll, "");
        add_line_to_scroll(scroll, "Press ESC to exit, then retype command with full filename");
    }
}

void check_multiwatch_output(Terminal *term) {
    if (!multiwatch_active) return;
    
    Tab *current_tab = &term->tabs[term->current_tab_index];
    int any_active = 0;
    
    for (int i = 0; i < multiwatch_count; i++) {
        if (multiwatch_pipes[i] < 0) continue;
        
        char output_buffer[512];
        ssize_t bytes_read;
        
        while ((bytes_read = read(multiwatch_pipes[i], output_buffer, sizeof(output_buffer) - 1)) > 0) {
            output_buffer[bytes_read] = '\0';
            add_text_to_buffer(current_tab, output_buffer);
            any_active = 1;
        }
        
        if (bytes_read == 0) {
            // Child finished
            close(multiwatch_pipes[i]);
            multiwatch_pipes[i] = -1;
            waitpid(multiwatch_pids[i], NULL, WNOHANG);
            multiwatch_pids[i] = 0;
        } else if (bytes_read < 0 && errno == EAGAIN) {
            any_active = 1;
        }
    }
    
    if (!any_active) {
        multiwatch_active = 0;
        multiwatch_count = 0;
        term->multiwatch_running = 0;
        add_text_to_buffer(current_tab, "multiWatch completed\n");
    }
}

void builtin_multiwatch(Terminal *term, char *args) {
    Tab *current_tab = &term->tabs[term->current_tab_index];
    
    if (!args || strlen(trim_whitespace(args)) == 0) {
        add_text_to_buffer(current_tab, "Usage: multiWatch [\"cmd1\", \"cmd2\", \"cmd3\"]\n");
        return;
    }
    
    // Kill existing multiwatch processes if running
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
    
    // Parse commands from format ["cmd1", "cmd2", "cmd3"]
    char commands[10][MAX_CMD_LEN];
    int command_count = 0;
    
    char *trimmed_args = trim_whitespace(args);
    char *left_bracket = strchr(trimmed_args, '[');
    char *right_bracket = strrchr(trimmed_args, ']');
    
    if (!left_bracket || !right_bracket || left_bracket >= right_bracket) {
        add_text_to_buffer(current_tab, "Error: Use format multiWatch [\"cmd1\", \"cmd2\"]\n");
        return;
    }
    
    left_bracket++;  // Move past '['
    *right_bracket = '\0';  // Terminate at ']'
    
    // Parse comma-separated commands
    for (char *token = strtok(left_bracket, ","); token && command_count < 10; token = strtok(NULL, ",")) {
        token = trim_whitespace(token);
        
        // Remove quotes
        if (token[0] == '"') token++;
        char *quote_end = strrchr(token, '"');
        if (quote_end) *quote_end = '\0';
        
        if (strlen(token) > 0) {
            strncpy(commands[command_count], token, MAX_CMD_LEN - 1);
            commands[command_count][MAX_CMD_LEN - 1] = '\0';
            command_count++;
        }
    }
    
    if (command_count == 0) {
        add_text_to_buffer(current_tab, "No valid commands found\n");
        return;
    }
    
    add_text_to_buffer(current_tab, "Starting multiWatch (Ctrl+C to stop)...\n");
    draw_terminal_window(g_terminal);
    
    // Spawn child processes for each command
    for (int i = 0; i < command_count; i++) {
        int pipe_fds[2];
        if (pipe(pipe_fds) == -1) {
            perror("pipe failed");
            continue;
        }
        
        pid_t child_pid = fork();
        if (child_pid == 0) {
            // Child process
            close(pipe_fds[0]);  // Close read end
            dup2(pipe_fds[1], STDOUT_FILENO);
            dup2(pipe_fds[1], STDERR_FILENO);
            close(pipe_fds[1]);
            
            // Make output unbuffered
            setvbuf(stdout, NULL, _IONBF, 0);
            setvbuf(stderr, NULL, _IONBF, 0);
            
            printf("[multiWatch child %d started]\n", i);
            fflush(stdout);
            
            // Execute command in a loop (10 iterations with 2 second delays)
            for (int loop = 0; loop < 10; loop++) {
                time_t now = time(NULL);
                char *timestr = ctime(&now);
                if (timestr && strlen(timestr) > 0) timestr[strlen(timestr) - 1] = '\0';
                
                printf("%s, %s\n", commands[i], timestr ? timestr : "");
                printf("--------------------------------------------------\n");
                fflush(stdout);
                
                FILE *command_output = popen(commands[i], "r");
                if (command_output) {
                    char line[256];
                    while (fgets(line, sizeof(line), command_output)) {
                        fputs(line, stdout);
                    }
                    pclose(command_output);
                } else {
                    printf("Error executing command: %s\n", commands[i]);
                }
                
                printf("--------------------------------------------------\n");
                fflush(stdout);
                sleep(2);
            }
            
            printf("[multiWatch child %d finished]\n", i);
            fflush(stdout);
            exit(0);
        } else if (child_pid > 0) {
            // Parent process
            close(pipe_fds[1]);  // Close write end
            
            // Set read end to non-blocking
            int flags = fcntl(pipe_fds[0], F_GETFL, 0);
            fcntl(pipe_fds[0], F_SETFL, flags | O_NONBLOCK);
            
            multiwatch_pids[i] = child_pid;
            multiwatch_pipes[i] = pipe_fds[0];
            multiwatch_count++;
        } else {
            perror("fork failed");
            close(pipe_fds[0]);
            close(pipe_fds[1]);
        }
    }
    
    if (multiwatch_count > 0) {
        multiwatch_active = 1;
        term->multiwatch_running = 1;
    }
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

void execute_user_command(Terminal *term, char *command) {
    if (!command || strlen(command) == 0) return;
    
    Tab *current_tab = &term->tabs[term->current_tab_index];
    char *trimmed_command = trim_whitespace(command);
    
    if (strlen(trimmed_command) == 0) return;
    
    // Add to history and save
    add_command_to_history(term, trimmed_command);
    save_history_to_file(term);
    
    // Echo the command (only if not in scroll mode)
    if (!current_tab->scroll_view.is_active) {
        char echo_line[MAX_CMD_LEN + 20];
        snprintf(echo_line, sizeof(echo_line), "anshgpt@xterm> %s\n", trimmed_command);
        add_text_to_buffer(current_tab, echo_line);
    }
    
    // Handle built-in commands
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
    
    // Execute as external command
    run_external_command(term, trimmed_command);
}