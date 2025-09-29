#include "../include/myterm.h"

// Global variables for signal handling
extern Terminal *g_terminal;
static volatile sig_atomic_t interrupted = 0;

Terminal* create_terminal(void) {
    Terminal *term = malloc(sizeof(Terminal));
    if (!term) return NULL;

    // Initialize terminal structure
    memset(term, 0, sizeof(Terminal));
    term->current_tab = 0;
    term->num_tabs = 0;
    term->history_count = 0;
    term->history_pos = 0;
    term->search_mode = 0;

    return term;
}

void cleanup_terminal(Terminal *term) {
    if (!term) return;

    // Close all tabs
    for (int i = 0; i < term->num_tabs; i++) {
        close_tab(term, i);
    }

    // Clean up X11 resources
    if (term->font) XFreeFont(term->display, term->font);
    if (term->gc) XFreeGC(term->display, term->gc);
    if (term->window) XDestroyWindow(term->display, term->window);
    if (term->display) XCloseDisplay(term->display);

    free(term);
}

void setup_locale(void) {
    setlocale(LC_ALL, "");
    setlocale(LC_CTYPE, "en_US.UTF-8");
}

void init_tab(Terminal *term, int tab_index) {
    if (tab_index >= MAX_TABS) return;

    Tab *tab = &term->tabs[tab_index];
    memset(tab, 0, sizeof(Tab));

    tab->active = 1;
    tab->buffer_pos = 0;
    tab->cursor_pos = strlen(PROMPT); // Initialize cursor after prompt
    tab->line_count = 0;

    // Initialize current line with prompt
    strcpy(tab->current_line, PROMPT);

    // Create pipes for communication with shell process
    if (pipe(tab->pipe_in) == -1 || pipe(tab->pipe_out) == -1) {
        perror("pipe");
        return;
    }

    // Fork shell process
    tab->shell_pid = fork();
    if (tab->shell_pid == 0) {
        // Child process - shell
        close(tab->pipe_in[1]);  // Close write end of input pipe
        close(tab->pipe_out[0]); // Close read end of output pipe

        // Redirect stdin and stdout to pipes
        dup2(tab->pipe_in[0], STDIN_FILENO);
        dup2(tab->pipe_out[1], STDOUT_FILENO);
        dup2(tab->pipe_out[1], STDERR_FILENO);

        // Close unused pipe ends
        close(tab->pipe_in[0]);
        close(tab->pipe_out[1]);

        // Execute shell
        execl("/bin/sh", "sh", NULL);
        exit(1);
    } else if (tab->shell_pid > 0) {
        // Parent process
        close(tab->pipe_in[0]);  // Close read end of input pipe  
        close(tab->pipe_out[1]); // Close write end of output pipe

        // Make pipes non-blocking
        fcntl(tab->pipe_out[0], F_SETFL, O_NONBLOCK);
        fcntl(tab->pipe_in[1], F_SETFL, O_NONBLOCK);
    }
}

void switch_tab(Terminal *term, int tab_index) {
    if (tab_index >= 0 && tab_index < term->num_tabs && 
        term->tabs[tab_index].active) {
        term->current_tab = tab_index;
        draw_terminal(term);
    }
}

void create_new_tab(Terminal *term) {
    if (term->num_tabs >= MAX_TABS) return;

    int new_tab = term->num_tabs++;
    init_tab(term, new_tab);
    term->current_tab = new_tab;

    draw_terminal(term);
}

void close_tab(Terminal *term, int tab_index) {
    if (tab_index >= term->num_tabs || !term->tabs[tab_index].active) return;

    Tab *tab = &term->tabs[tab_index];

    // Kill shell process
    if (tab->shell_pid > 0) {
        kill(tab->shell_pid, SIGTERM);
        waitpid(tab->shell_pid, NULL, 0);
    }

    // Close pipes
    if (tab->pipe_in[1] >= 0) close(tab->pipe_in[1]);
    if (tab->pipe_out[0] >= 0) close(tab->pipe_out[0]);

    tab->active = 0;

    // If this was the current tab, switch to another
    if (term->current_tab == tab_index) {
        for (int i = 0; i < term->num_tabs; i++) {
            if (term->tabs[i].active) {
                term->current_tab = i;
                break;
            }
        }
    }
}

void append_to_buffer(Terminal *term, const char *text) {
    Tab *tab = &term->tabs[term->current_tab];
    int len = strlen(text);

    if (tab->buffer_pos + len < BUFFER_SIZE - 1) {
        strcpy(tab->buffer + tab->buffer_pos, text);
        tab->buffer_pos += len;
        tab->line_count++;
    }
}

void clear_current_line(Terminal *term) {
    Tab *tab = &term->tabs[term->current_tab];
    memset(tab->current_line, 0, sizeof(tab->current_line));
    strcpy(tab->current_line, PROMPT);
    tab->cursor_pos = strlen(PROMPT);
}

void move_cursor_start(Terminal *term) {
    Tab *tab = &term->tabs[term->current_tab];
    tab->cursor_pos = strlen(PROMPT);
}

void move_cursor_end(Terminal *term) {
    Tab *tab = &term->tabs[term->current_tab];
    tab->cursor_pos = strlen(tab->current_line);
}

void debug_print(const char *format, ...) {
    #ifdef DEBUG
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    #endif
}
