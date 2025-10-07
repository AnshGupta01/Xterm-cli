#include "xterm.h"
#include <errno.h>

/* MultiWatch globals (defined here, extern in header) */
pid_t multiwatch_pids[10]  = {0};
int   multiwatch_pipes[10] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
int   multiwatch_count     = 0;
int   multiwatch_active    = 0;

/* Wire up signals: forward SIGINT/SIGTSTP; ignore SIGCHLD/SIGPIPE */
void setup_signal_handlers() {
    signal(SIGINT,  handle_sigint);
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
    FILE *f = fopen("myterm_history.txt", "w");
    if (!f) return;

    fprintf(f, "# MyTerm Command History\n");
    time_t now = time(NULL);
    fprintf(f, "# Generated on: %s", ctime(&now));

    for (int i = 0; i < term->history_count; i++) {
        fprintf(f, "%s\n", term->command_history[i]);
    }
    fclose(f);
}

/* Load history if file exists; skip comment lines */
void load_history_from_file(Terminal *term) {
    FILE *f = fopen("myterm_history.txt", "r");
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

/* Print a window-fit slice of command history */
void builtin_history_display(Terminal *term) {
    Tab *current_tab = &term->tabs[term->current_tab_index];

    int avail = (WINDOW_HEIGHT - 150) / 16;
    int max_lines = avail - 5;
    if (max_lines < 10) max_lines = 10;
    if (max_lines > 50) max_lines = 50;

    add_text_to_buffer(current_tab, "Command History:\n");

    if (term->history_count == 0) {
        add_text_to_buffer(current_tab, "No commands in history\n");
        return;
    }

    int start = (term->history_count > max_lines)
              ? term->history_count - max_lines
              : 0;
    int shown = term->history_count - start;

    char header[128];
    snprintf(header, sizeof(header),
             "Showing %d most recent commands (out of %d total):\n",
             shown, term->history_count);
    add_text_to_buffer(current_tab, header);
    add_text_to_buffer(current_tab, "----------------------------------------\n");

    for (int i = start; i < term->history_count; i++) {
        char line[MAX_CMD_LEN + 20];
        snprintf(line, sizeof(line), "%4d: %s\n", i + 1, term->command_history[i]);
        add_text_to_buffer(current_tab, line);
    }

    if (term->history_count > max_lines) {
        char footer[128];
        snprintf(footer, sizeof(footer), "----------------------------------------\n");
        add_text_to_buffer(current_tab, footer);
        snprintf(footer, sizeof(footer), "(%d older commands not shown)\n",
                 term->history_count - max_lines);
        add_text_to_buffer(current_tab, footer);
    }
}

/* Search newest-first for exact, else partial (len >= 2); populate prompt line on hit */
void builtin_search_history(Terminal *term) {
    Tab *current_tab = &term->tabs[term->current_tab_index];

    if (strlen(term->search_term) == 0) {
        add_text_to_buffer(current_tab, "No search term provided\n");
        return;
    }

    for (int i = term->history_count - 1; i >= 0; i--) {
        if (strcmp(term->command_history[i], term->search_term) == 0) {
            snprintf(current_tab->current_command, MAX_CMD_LEN,
                     "user@myterm> %s", term->command_history[i]);
            current_tab->cursor_position = strlen(current_tab->current_command);
            return;
        }
    }

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
                 "user@myterm> %s", term->command_history[best]);
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
        if (last_space) {
            *(last_space + 1) = '\0';
            snprintf(current_tab->current_command, MAX_CMD_LEN,
                     "user@myterm> %s%s", partial_command, matches[0]);
        } else {
            snprintf(current_tab->current_command, MAX_CMD_LEN,
                     "user@myterm> %s", matches[0]);
        }
    } else if (count > 1) {
        add_text_to_buffer(current_tab, "\nMatching files:\n");
        for (int i = 0; i < count; i += 3) {
            char line[256] = "";
            for (int j = 0; j < 3 && i + j < count; j++) {
                char cell[32];
                snprintf(cell, sizeof(cell), "%-20s", matches[i + j]);
                strncat(line, cell, sizeof(line) - strlen(line) - 1);
            }
            strncat(line, "\n", sizeof(line) - strlen(line) - 1);
            add_text_to_buffer(current_tab, line);
        }
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
        multiwatch_count  = 0;
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
        multiwatch_count  = 0;
    }

    char commands[10][MAX_CMD_LEN];
    int command_count = 0;

    char *trimmed_args = trim_whitespace(args);
    char *lb = strchr(trimmed_args, '[');
    char *rb = strrchr(trimmed_args, ']');
    if (!lb || !rb || lb >= rb) {
        add_text_to_buffer(current_tab, "Error: Use format multiWatch [\"cmd1\", \"cmd2\"]\n");
        return;
    }

    lb++;
    *rb = '\0';

    for (char *tok = strtok(lb, ","); tok && command_count < 10; tok = strtok(NULL, ",")) {
        tok = trim_whitespace(tok);
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
    draw_terminal_window(term);

    for (int i = 0; i < command_count; i++) {
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("pipe failed");
            continue;
        }

        pid_t pid = fork();
        if (pid == 0) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            dup2(pipefd[1], STDERR_FILENO);
            close(pipefd[1]);

            setvbuf(stdout, NULL, _IONBF, 0);
            setvbuf(stderr, NULL, _IONBF, 0);

            printf("[multiWatch child %d started]\n", i);
            fflush(stdout);

            for (int loop = 0; loop < 10; loop++) {
                time_t now = time(NULL);
                char *timestr = ctime(&now);
                if (timestr && strlen(timestr) > 0) timestr[strlen(timestr) - 1] = '\0';

                printf("%s, %s\n", commands[i], timestr ? timestr : "");
                printf("----------------------------------------------------\n");
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

                printf("----------------------------------------------------\n");
                fflush(stdout);
                sleep(2);
            }

            printf("[multiWatch child %d finished]\n", i);
            fflush(stdout);
            exit(0);

        } else if (pid > 0) {
            close(pipefd[1]);
            int flags = fcntl(pipefd[0], F_GETFL, 0);
            fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);

            multiwatch_pids[i]  = pid;
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

/* Send a command line to the active tab’s shell via stdin pipe */
void run_external_command(Terminal *term, char *cmd_line) {
    Tab *current_tab = &term->tabs[term->current_tab_index];
    if (!current_tab->is_active || current_tab->input_pipe[1] < 0) return;

    char command_with_newline[MAX_CMD_LEN + 2];
    snprintf(command_with_newline, sizeof(command_with_newline), "%s\n", cmd_line);

    ssize_t bytes_written = write(current_tab->input_pipe[1],
                                  command_with_newline,
                                  strlen(command_with_newline));
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

    add_command_to_history(term, trimmed_command);
    save_history_to_file(term);

    char echo_line[MAX_CMD_LEN + 20];
    snprintf(echo_line, sizeof(echo_line), "user@myterm> %s\n", trimmed_command);
    add_text_to_buffer(current_tab, echo_line);

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

    run_external_command(term, trimmed_command);
}