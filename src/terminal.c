#include "xterm.h"

/* Append text to a tab’s display buffer with simple truncation when near limit */
void add_text_to_buffer(Tab *tab, char *text) {
    if (!tab || !text) return;

    int tlen = strlen(text);
    int blen = strlen(tab->display_buffer);

    if (blen + tlen >= MAX_BUFFER_SIZE - 100) {
        int cut = MAX_BUFFER_SIZE / 3;
        memmove(tab->display_buffer,
                tab->display_buffer + cut,
                blen - cut + 1);
        blen = strlen(tab->display_buffer);
    }

    if (blen + tlen < MAX_BUFFER_SIZE - 1) {
        strcat(tab->display_buffer, text);
    }
}

/* Add a command to history (skip immediate duplicates; cap size with half-shift) */
void add_command_to_history(Terminal *term, char *command) {
    if (!command || strlen(command) == 0) return;

    if (term->history_count > 0 &&
        strcmp(term->command_history[term->history_count - 1], command) == 0) {
        return;
    }

    if (term->history_count >= MAX_HISTORY) {
        int shift = MAX_HISTORY / 2;
        for (int i = 0; i < MAX_HISTORY - shift; i++) {
            strcpy(term->command_history[i], term->command_history[i + shift]);
        }
        term->history_count = MAX_HISTORY - shift;
    }

    strncpy(term->command_history[term->history_count], command, MAX_CMD_LEN - 1);
    term->command_history[term->history_count][MAX_CMD_LEN - 1] = '\0';
    term->history_count++;
    term->history_position = term->history_count;
}

/* Create and initialize a Terminal with defaults and persisted history */
Terminal* create_new_terminal() {
    Terminal *term = malloc(sizeof(Terminal));
    if (!term) return NULL;

    memset(term, 0, sizeof(Terminal));

    for (int i = 0; i < MAX_TABS; i++) {
        term->tabs[i].is_active = 0;
        term->tabs[i].shell_process_id = -1;
        term->tabs[i].input_pipe[0] = term->tabs[i].input_pipe[1] = -1;
        term->tabs[i].output_pipe[0] = term->tabs[i].output_pipe[1] = -1;
        term->tabs[i].display_buffer[0] = '\0';
        term->tabs[i].current_command[0] = '\0';
        term->tabs[i].cursor_position = 0;
    }

    term->current_tab_index = 0;
    term->total_tabs = 0;
    term->history_count = 0;
    term->history_position = 0;
    term->search_mode = 0;

    /* Load prior session history if available */
    load_history_from_file(term);

    return term;
}

/* Gracefully tear down terminal: save history, stop shells, close pipes, free X11 */
void cleanup_terminal_resources(Terminal *term) {
    if (!term) return;

    save_history_to_file(term);

    for (int i = 0; i < MAX_TABS; i++) {
        if (!term->tabs[i].is_active) continue;

        if (term->tabs[i].shell_process_id > 0) {
            kill(term->tabs[i].shell_process_id, SIGTERM);
            waitpid(term->tabs[i].shell_process_id, NULL, 0);
        }

        if (term->tabs[i].input_pipe[0]  >= 0) close(term->tabs[i].input_pipe[0]);
        if (term->tabs[i].input_pipe[1]  >= 0) close(term->tabs[i].input_pipe[1]);
        if (term->tabs[i].output_pipe[0] >= 0) close(term->tabs[i].output_pipe[0]);
        if (term->tabs[i].output_pipe[1] >= 0) close(term->tabs[i].output_pipe[1]);
    }

    if (term->graphics_context) XFreeGC(term->x11_display, term->graphics_context);
    if (term->x11_window)       XDestroyWindow(term->x11_display, term->x11_window);
    if (term->x11_display)      XCloseDisplay(term->x11_display);

    free(term);
}

/* Open a new tab backed by a bash shell; set non-blocking read end */
void initialize_new_tab(Terminal *term, int tab_index) {
    if (tab_index >= MAX_TABS || tab_index < 0) return;

    Tab *tab = &term->tabs[tab_index];

    tab->is_active = 1;
    tab->shell_process_id = -1;
    tab->display_buffer[0] = '\0';
    strcpy(tab->current_command, "anshgpt@xterm> ");
    tab->cursor_position = strlen(tab->current_command);

    if (pipe(tab->input_pipe) == -1 || pipe(tab->output_pipe) == -1) {
        perror("pipe creation failed");
        tab->is_active = 0;
        return;
    }

    tab->shell_process_id = fork();

    if (tab->shell_process_id == 0) {
        /* Child shell: wire stdin/stdout/stderr to pipes */
        dup2(tab->input_pipe[0], STDIN_FILENO);
        close(tab->input_pipe[0]);
        close(tab->input_pipe[1]);

        dup2(tab->output_pipe[1], STDOUT_FILENO);
        dup2(tab->output_pipe[1], STDERR_FILENO);
        close(tab->output_pipe[0]);
        close(tab->output_pipe[1]);

        execlp("bash", "bash", "--norc", "--noprofile", NULL);
        exit(1);

    } else if (tab->shell_process_id > 0) {
        /* Parent: close unused ends and set non-blocking read */
        close(tab->input_pipe[0]);
        close(tab->output_pipe[1]);

        fcntl(tab->output_pipe[0], F_SETFL, O_NONBLOCK);

        printf("Created tab %d with shell process %d\n", tab_index, tab->shell_process_id);

    } else {
        perror("fork failed");
        tab->is_active = 0;
        close(tab->input_pipe[0]);
        close(tab->input_pipe[1]);
        close(tab->output_pipe[0]);
        close(tab->output_pipe[1]);
    }
}

/* Switch active tab if the destination exists and is active */
void switch_to_tab(Terminal *term, int tab_index) {
    if (tab_index >= 0 && tab_index < MAX_TABS &&
        tab_index < term->total_tabs && term->tabs[tab_index].is_active) {
        term->current_tab_index = tab_index;
        draw_terminal_window(term);
    }
}

/* Close current tab: terminate shell, close pipes, pick next tab or exit */
void close_current_tab(Terminal *term) {
    int idx = term->current_tab_index;
    Tab *cur = &term->tabs[idx];

    if (!cur->is_active) return;

    if (cur->shell_process_id > 0) {
        kill(cur->shell_process_id, SIGTERM);
        usleep(100000);
        if (kill(cur->shell_process_id, 0) == 0) {
            kill(cur->shell_process_id, SIGKILL);
        }
        waitpid(cur->shell_process_id, NULL, 0);
    }

    if (cur->input_pipe[1]  >= 0) close(cur->input_pipe[1]);
    if (cur->output_pipe[0] >= 0) close(cur->output_pipe[0]);

    cur->is_active = 0;
    cur->shell_process_id = -1;

    int next = -1;
    for (int i = 0; i < term->total_tabs; i++) {
        if (term->tabs[i].is_active) { next = i; break; }
    }

    if (next == -1) {
        cleanup_terminal_resources(term);
        printf("All tabs closed. Exiting XTerm.\n");
        exit(0);
    } else {
        term->current_tab_index = next;
        draw_terminal_window(term);
    }
}

/* Trim leading and trailing ASCII whitespace in place; return start pointer */
char* trim_whitespace(char *str) {
    if (!str) return str;

    while (*str && (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r')) str++;
    if (*str == 0) return str;

    char *end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }
    return str;
}
