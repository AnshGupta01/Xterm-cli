#include "xterm.h"

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
    
    // Load history from file on startup
    load_history_from_file(term);
    
    return term;
}

void cleanup_terminal_resources(Terminal *term) {
    if (!term) return;
    
    // Save history before cleanup
    save_history_to_file(term);
    
    for (int i = 0; i < MAX_TABS; i++) {
        if (term->tabs[i].is_active) {
            if (term->tabs[i].shell_process_id > 0) {
                kill(term->tabs[i].shell_process_id, SIGTERM);
                waitpid(term->tabs[i].shell_process_id, NULL, 0);
            }
            
            if (term->tabs[i].input_pipe[0] >= 0) close(term->tabs[i].input_pipe[0]);
            if (term->tabs[i].input_pipe[1] >= 0) close(term->tabs[i].input_pipe[1]);
            if (term->tabs[i].output_pipe[0] >= 0) close(term->tabs[i].output_pipe[0]);
            if (term->tabs[i].output_pipe[1] >= 0) close(term->tabs[i].output_pipe[1]);
        }
    }
    
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
    strcpy(tab->current_command, "user@myterm> ");
    tab->cursor_position = strlen(tab->current_command);
    
    if (pipe(tab->input_pipe) == -1 || pipe(tab->output_pipe) == -1) {
        perror("pipe creation failed");
        tab->is_active = 0;
        return;
    }
    
    tab->shell_process_id = fork();
    
    if (tab->shell_process_id == 0) {
        // Child process
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
        term->current_tab_index = tab_index;
        draw_terminal_window(term);
    }
}

void close_current_tab(Terminal *term) {
    int current_index = term->current_tab_index;
    Tab *current_tab = &term->tabs[current_index];
    
    if (!current_tab->is_active) return;
    
    if (current_tab->shell_process_id > 0) {
        kill(current_tab->shell_process_id, SIGTERM);
        usleep(100000);
        
        if (kill(current_tab->shell_process_id, 0) == 0) {
            kill(current_tab->shell_process_id, SIGKILL);
        }
        waitpid(current_tab->shell_process_id, NULL, 0);
    }
    
    if (current_tab->input_pipe[1] >= 0) close(current_tab->input_pipe[1]);
    if (current_tab->output_pipe[0] >= 0) close(current_tab->output_pipe[0]);
    
    current_tab->is_active = 0;
    current_tab->shell_process_id = -1;
    
    int next_tab = -1;
    for (int i = 0; i < term->total_tabs; i++) {
        if (term->tabs[i].is_active) {
            next_tab = i;
            break;
        }
    }
    
    if (next_tab == -1) {
        cleanup_terminal_resources(term);
        printf("All tabs closed. Exiting MyTerm.\n");
        exit(0);
    } else {
        term->current_tab_index = next_tab;
        draw_terminal_window(term);
    }
}

void add_text_to_buffer(Tab *tab, char *text) {
    if (!tab || !text) return;
    
    int text_length = strlen(text);
    int buffer_length = strlen(tab->display_buffer);
    
    if (buffer_length + text_length >= MAX_BUFFER_SIZE - 100) {
        int remove_amount = MAX_BUFFER_SIZE / 3;
        memmove(tab->display_buffer, 
                tab->display_buffer + remove_amount, 
                buffer_length - remove_amount + 1);
        buffer_length = strlen(tab->display_buffer);
    }
    
    if (buffer_length + text_length < MAX_BUFFER_SIZE - 1) {
        strcat(tab->display_buffer, text);
    }
}

void add_command_to_history(Terminal *term, char *command) {
    if (!command || strlen(command) == 0) return;
    
    // Don't add duplicate commands
    if (term->history_count > 0 && 
        strcmp(term->command_history[term->history_count - 1], command) == 0) {
        return;
    }
    
    // Ensure we don't exceed MAX_HISTORY (10000)
    if (term->history_count >= MAX_HISTORY) {
        // Shift history array left by half to make room
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

char* trim_whitespace(char *str) {
    if (!str) return str;
    
    while (*str && (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r')) {
        str++;
    }
    
    if (*str == 0) return str;
    
    char *end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }
    
    return str;
}