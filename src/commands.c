#include "xterm.h"
#include <regex.h>
#include <errno.h>

/* MultiWatch globals (defined here, extern in header) */
pid_t multiwatch_pids[10] = {0};
int multiwatch_pipes[10] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
int multiwatch_count = 0;
int multiwatch_active = 0;

/* Wire up signals: forward SIGINT/SIGTSTP; ignore SIGCHLD/SIGPIPE */
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

/* Overwrite history file with the current snapshot */
void save_history_to_file(Terminal *term) {
    FILE *f = fopen("xterm_history.txt", "w");
    if (!f) return;
    fprintf(f, "# XTerm Command History\n");
    time_t now = time(NULL);
    fprintf(f, "# Generated on: %s", ctime(&now));
    for (int i = 0; i < term->history_count; i++) {
        fprintf(f, "%s\n", term->command_history[i]);
    }
    fclose(f);
}

/* Load history if file exists; skip comment lines */
void load_history_from_file(Terminal *term) {
    FILE *f = fopen("xterm_history.txt", "r");
    if (!f) return;
    char line[MAX_CMD_LEN];
    while (fgets(line, sizeof(line), f) && term->history_count < MAX_HISTORY) {
        if (line[0] == '#') continue;
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        if (strlen(line) > 0) {
            strncpy(term->command_history[term->history_count], line, MAX_CMD_LEN - 1);
            term->command_history[term->history_count][MAX_CMD_LEN - 1] = '\0';
            term->history_count++;
        }
    }
    fclose(f);
    term->history_position = term->history_count;
}

/* Display command history in scrollable format */
void builtin_history_display(Terminal *term) {
    Tab *current_tab = &term->tabs[term->current_tab_index];
    
    if (term->history_count == 0) {
        add_text_to_buffer(current_tab, "No commands in history\n");
        return;
    }
    
    // Enter scroll mode
    enter_scroll_mode(current_tab, "Command History");
    ScrollView *scroll = &current_tab->scroll_view;
    
    // Add header information
    char header[256];
    snprintf(header, sizeof(header), "Total commands in history: %d", term->history_count);
    add_line_to_scroll(scroll, header);
    add_line_to_scroll(scroll, "=====================================");
    add_line_to_scroll(scroll, "");
    
    // Determine how many commands to show (up to HISTORY_DISPLAY or all if less)
    int commands_to_show = (term->history_count > HISTORY_DISPLAY) ? HISTORY_DISPLAY : term->history_count;
    int start_index = term->history_count - commands_to_show;
    
    // Add commands to scroll view
    for (int i = start_index; i < term->history_count; i++) {
        char line[MAX_CMD_LEN + 20];
        snprintf(line, sizeof(line), "%4d: %s", i + 1, term->command_history[i]);
        add_line_to_scroll(scroll, line);
    }
    
    // Add footer if there are older commands not shown
    if (term->history_count > HISTORY_DISPLAY) {
        add_line_to_scroll(scroll, "");
        add_line_to_scroll(scroll, "=====================================");
        char footer[256];
        snprintf(footer, sizeof(footer), "(%d older commands not shown)", term->history_count - HISTORY_DISPLAY);
        add_line_to_scroll(scroll, footer);
        add_line_to_scroll(scroll, "Use PageUp/PageDown or Arrow keys to scroll");
        add_line_to_scroll(scroll, "Press ESC to exit history view");
    }
    
    // Start at the bottom of the history (most recent commands)
    scroll_to_bottom(scroll);
}

/* Search newest-first for exact, else partial (len >= 2); populate prompt line on hit */
void builtin_search_history(Terminal *term) {
    Tab *current_tab = &term->tabs[term->current_tab_index];
    if (strlen(term->search_term) == 0) {
        add_text_to_buffer(current_tab, "No search term provided\n");
        return;
    }

    // First try exact match
    for (int i = term->history_count - 1; i >= 0; i--) {
        if (strcmp(term->command_history[i], term->search_term) == 0) {
            snprintf(current_tab->current_command, MAX_CMD_LEN, 
                "anshgpt@xterm> %s", term->command_history[i]);
            current_tab->cursor_position = strlen(current_tab->current_command);
            return;
        }
    }

    // Then try partial match
    int best = -1;
    if (strlen(term->search_term) >= 2) {
        for (int i = term->history_count - 1; i >= 0; i--) {
            if (strstr(term->command_history[i], term->search_term)) {
                best = i;
                break;
            }
        }
    }

    if (best >= 0) {
        snprintf(current_tab->current_command, MAX_CMD_LEN, 
            "anshgpt@xterm> %s", term->command_history[best]);
        current_tab->cursor_position = strlen(current_tab->current_command);
    } else {
        add_text_to_buffer(current_tab, "No match for search term in history\n");
    }
}

/* File-name completion in CWD: expand single match or list multiple */
void autocomplete_filename(Terminal *term, char *partial_command) {
    Tab *current_tab = &term->tabs[term->current_tab_index];
    char *last_space = strrchr(partial_command, ' ');
    char *name_part = last_space ? last_space + 1 : partial_command;
    
    if (strlen(name_part) == 0) return;

    DIR *dir = opendir(".");
    if (!dir) return;

    char matches[20][256];
    int count = 0;
    struct dirent *ent;
    
    while ((ent = readdir(dir)) != NULL && count < 20) {
        if (ent->d_name[0] == '.' && name_part[0] != '.') continue;
        if (strncmp(ent->d_name, name_part, strlen(name_part)) == 0) {
            strncpy(matches[count], ent->d_name, sizeof(matches[count]) - 1);
            matches[count][sizeof(matches[count]) - 1] = '\0';
            count++;
        }
    }
    closedir(dir);

    if (count == 1) {
        // Single match - complete it
        if (last_space) {
            *(last_space + 1) = '\0';
            snprintf(current_tab->current_command, MAX_CMD_LEN, 
                "anshgpt@xterm> %s%s", partial_command, matches[0]);
        } else {
            snprintf(current_tab->current_command, MAX_CMD_LEN, 
                "anshgpt@xterm> %s", matches[0]);
        }
    } else if (count > 1) {
        // Multiple matches - show them in scroll mode
        enter_scroll_mode(current_tab, "File Completion Matches");
        ScrollView *scroll = &current_tab->scroll_view;
        
        char header[256];
        snprintf(header, sizeof(header), "Matching files for '%s':", name_part);
        add_line_to_scroll(scroll, header);
        add_line_to_scroll(scroll, "=========================");
        add_line_to_scroll(scroll, "");
        
        for (int i = 0; i < count; i++) {
            add_line_to_scroll(scroll, matches[i]);
        }
        
        add_line_to_scroll(scroll, "");
        add_line_to_scroll(scroll, "Press ESC to exit, then retype command with full filename");
    }
}

/* Pump outputs from children; clean up on EOF; finalize when all done */
void check_multiwatch_output(Terminal *term) {
    if (!multiwatch_active) return;
    
    Tab *current_tab = &term->tabs[term->current_tab_index];
    int active_any = 0;
    
    for (int i = 0; i < multiwatch_count; i++) {
        if (multiwatch_pipes[i] < 0) continue;
        
        char buffer[512];
        ssize_t nread;
        
        while ((nread = read(multiwatch_pipes[i], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[nread] = '\0';
            add_text_to_buffer(current_tab, buffer);
            active_any = 1;
        }
        
        if (nread == 0) {
            // EOF - child finished
            close(multiwatch_pipes[i]);
            multiwatch_pipes[i] = -1;
            waitpid(multiwatch_pids[i], NULL, WNOHANG);
            multiwatch_pids[i] = 0;
        } else if (nread < 0 && errno == EAGAIN) {
            active_any = 1;
        }
    }
    
    if (!active_any) {
        multiwatch_active = 0;
        multiwatch_count = 0;
        term->multiwatch_running = 0;
        add_text_to_buffer(current_tab, "multiWatch completed\n");
    }
}

/* multiWatch: parse commands, spawn children, and non-blocking stream their output */
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

    // Parse commands from the format ["cmd1", "cmd2", "cmd3"]
    char commands[10][MAX_CMD_LEN];
    int command_count = 0;
    
    char *trimmed_args = trim_whitespace(args);
    char *lb = strchr(trimmed_args, '[');
    char *rb = strrchr(trimmed_args, ']');
    
    if (!lb || !rb || lb >= rb) {
        add_text_to_buffer(current_tab, "Error: Use format multiWatch [\"cmd1\", \"cmd2\"]\n");
        return;
    }
    
    lb++; // Move past '['
    *rb = '\0'; // Terminate at ']'
    
    // Parse comma-separated commands
    for (char *tok = strtok(lb, ","); tok && command_count < 10; tok = strtok(NULL, ",")) {
        tok = trim_whitespace(tok);
        
        // Remove quotes
        if (tok[0] == '"') tok++;
        char *qe = strrchr(tok, '"');
        if (qe) *qe = '\0';
        
        if (strlen(tok) > 0) {
            strncpy(commands[command_count], tok, MAX_CMD_LEN - 1);
            commands[command_count][MAX_CMD_LEN - 1] = '\0';
            command_count++;
        }
    }
    
    if (command_count == 0) {
        add_text_to_buffer(current_tab, "No valid commands found\n");
        return;
    }

    add_text_to_buffer(current_tab, "Starting multiWatch (Ctrl+C to stop)...\n");
    draw_terminal_window(g_terminal);  // Use the global terminal pointer

    // Spawn child processes for each command
    for (int i = 0; i < command_count; i++) {
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("pipe failed");
            continue;
        }

        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            close(pipefd[0]); // Close read end
            dup2(pipefd[1], STDOUT_FILENO);
            dup2(pipefd[1], STDERR_FILENO);
            close(pipefd[1]);
            
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
                
                printf("--------------------------------------------------\n");
                fflush(stdout);
                sleep(2);
            }
            
            printf("[multiWatch child %d finished]\n", i);
            fflush(stdout);
            exit(0);
        } else if (pid > 0) {
            // Parent process
            close(pipefd[1]); // Close write end
            
            // Set read end to non-blocking
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

/* Send a command line to the active tab's shell via stdin pipe */
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

/* Entry point for processing a user line: history, built-ins, else hand to shell */
void execute_user_command(Terminal *term, char *command) {
    if (!command || strlen(command) == 0) return;
    
    Tab *current_tab = &term->tabs[term->current_tab_index];
    char *trimmed_command = trim_whitespace(command);
    
    if (strlen(trimmed_command) == 0) return;
    
    // Add to history and save it
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