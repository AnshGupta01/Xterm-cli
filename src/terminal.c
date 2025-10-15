#include "xterm.h"

void add_text_to_buffer(Tab *tab, char *text) {
    if (!tab || !text) return;
    
    int text_len = strlen(text);
    int buffer_len = strlen(tab->display_buffer);
    
    // Simple buffer management - cut some if getting full
    if (buffer_len + text_len >= MAX_BUFFER_SIZE - 100) {
        int cut_amount = MAX_BUFFER_SIZE / 3;
        memmove(tab->display_buffer,
                tab->display_buffer + cut_amount,
                buffer_len - cut_amount + 1);
        buffer_len = strlen(tab->display_buffer);
    }
    
    if (buffer_len + text_len < MAX_BUFFER_SIZE - 1) {
        strcat(tab->display_buffer, text);
    }
}

void add_command_to_history(Terminal *term, char *command) {
    if (!command || strlen(command) == 0) return;
    
    // Don't add same command twice in a row
    if (term->history_count > 0 &&
        strcmp(term->command_history[term->history_count - 1], command) == 0) {
        return;
    }
    
    // If history is full, remove older half
    if (term->history_count >= MAX_HISTORY) {
        int shift_amount = MAX_HISTORY / 2;
        for (int i = 0; i < MAX_HISTORY - shift_amount; i++) {
            strcpy(term->command_history[i], term->command_history[i + shift_amount]);
        }
        term->history_count = MAX_HISTORY - shift_amount;
    }
    
    strncpy(term->command_history[term->history_count], command, MAX_CMD_LEN - 1);
    term->command_history[term->history_count][MAX_CMD_LEN - 1] = '\0';
    term->history_count++;
    term->history_position = term->history_count;
}

Terminal* create_new_terminal() {
    Terminal *term = malloc(sizeof(Terminal));
    if (!term) return NULL;
    
    memset(term, 0, sizeof(Terminal));
    
    // Initialize all tabs as inactive  
    for (int i = 0; i < MAX_TABS; i++) {
        term->tabs[i].is_active = 0;
        term->tabs[i].shell_process_id = -1;
        term->tabs[i].input_pipe[0] = term->tabs[i].input_pipe[1] = -1;
        term->tabs[i].output_pipe[0] = term->tabs[i].output_pipe[1] = -1;
        term->tabs[i].display_buffer[0] = '\0';
        term->tabs[i].current_command[0] = '\0';
        term->tabs[i].cursor_position = 0;
        
        init_scroll_view(&term->tabs[i].scroll_view);
    }
    
    term->current_tab_index = 0;
    term->total_tabs = 0;
    term->history_count = 0;
    term->history_position = 0;
    term->search_mode = 0;
    
    load_history_from_file(term);
    return term;
}

void cleanup_terminal_resources(Terminal *term) {
    if (!term) return;
    
    save_history_to_file(term);
    
    // Clean up all active tabs
    for (int i = 0; i < MAX_TABS; i++) {
        if (!term->tabs[i].is_active) continue;
        
        if (term->tabs[i].shell_process_id > 0) {
            kill(term->tabs[i].shell_process_id, SIGTERM);
            waitpid(term->tabs[i].shell_process_id, NULL, 0);
        }
        
        if (term->tabs[i].input_pipe[0] >= 0) close(term->tabs[i].input_pipe[0]);
        if (term->tabs[i].input_pipe[1] >= 0) close(term->tabs[i].input_pipe[1]);
        if (term->tabs[i].output_pipe[0] >= 0) close(term->tabs[i].output_pipe[0]);
        if (term->tabs[i].output_pipe[1] >= 0) close(term->tabs[i].output_pipe[1]);
        
        if (term->tabs[i].scroll_view.is_active) {
            exit_scroll_mode(&term->tabs[i]);
        }
    }
    
    // Clean up X11 stuff
    if (term->graphics_context) XFreeGC(term->x11_display, term->graphics_context);
    if (term->x11_window) XDestroyWindow(term->x11_display, term->x11_window);
    if (term->x11_display) XCloseDisplay(term->x11_display);
    
    free(term);
}

void initialize_new_tab(Terminal *term, int tab_index) {
    if (tab_index >= MAX_TABS || tab_index < 0) return;
    
    Tab *tab = &term->tabs[tab_index];
    tab->is_active = 1;
    tab->shell_process_id = -1;
    tab->display_buffer[0] = '\0';
    strcpy(tab->current_command, "anshgpt@xterm> ");
    tab->cursor_position = strlen(tab->current_command);
    
    init_scroll_view(&tab->scroll_view);
    
    if (pipe(tab->input_pipe) == -1 || pipe(tab->output_pipe) == -1) {
        perror("pipe creation failed");
        tab->is_active = 0;
        return;
    }
    
    tab->shell_process_id = fork();
    if (tab->shell_process_id == 0) {
        // Child process - setup shell
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
        // Parent process
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

void switch_to_tab(Terminal *term, int tab_index) {
    if (tab_index >= 0 && tab_index < MAX_TABS &&
        tab_index < term->total_tabs && term->tabs[tab_index].is_active) {
        
        // Exit scroll mode on current tab if it's active
        if (term->tabs[term->current_tab_index].scroll_view.is_active) {
            exit_scroll_mode(&term->tabs[term->current_tab_index]);
        }
        
        term->current_tab_index = tab_index;
        draw_terminal_window(term);
    }
}

void close_current_tab(Terminal *term) {
    int current_idx = term->current_tab_index;
    Tab *current_tab = &term->tabs[current_idx];
    
    if (!current_tab->is_active) return;
    
    if (current_tab->scroll_view.is_active) {
        exit_scroll_mode(current_tab);
    }
    
    if (current_tab->shell_process_id > 0) {
        kill(current_tab->shell_process_id, SIGTERM);
        usleep(100000);  // Give it time to close nicely
        if (kill(current_tab->shell_process_id, 0) == 0) {
            kill(current_tab->shell_process_id, SIGKILL);  // Force kill if needed
        }
        waitpid(current_tab->shell_process_id, NULL, 0);
    }
    
    if (current_tab->input_pipe[1] >= 0) close(current_tab->input_pipe[1]);
    if (current_tab->output_pipe[0] >= 0) close(current_tab->output_pipe[0]);
    
    current_tab->is_active = 0;
    current_tab->shell_process_id = -1;
    
    // Find next active tab
    int next_tab = -1;
    for (int i = 0; i < term->total_tabs; i++) {
        if (term->tabs[i].is_active) {
            next_tab = i;
            break;
        }
    }
    
    if (next_tab == -1) {
        cleanup_terminal_resources(term);
        printf("All tabs closed. Goodbye!\n");
        exit(0);
    } else {
        term->current_tab_index = next_tab;
        draw_terminal_window(term);
    }
}

char* trim_whitespace(char *str) {
    if (!str) return str;
    
    // Trim leading whitespace
    while (*str && (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r')) str++;
    
    if (*str == 0) return str;
    
    // Trim trailing whitespace
    char *end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }
    
    return str;
}