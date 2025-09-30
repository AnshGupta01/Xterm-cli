#include "xterm.h"

void setup_x11_gui(Terminal *term) {
    term->x11_display = XOpenDisplay(NULL);
    if (!term->x11_display) {
        printf("Error: Cannot open X11 display. Make sure XQuartz is running.\n");
        exit(1);
    }
    
    int screen = DefaultScreen(term->x11_display);
    
    term->x11_window = XCreateSimpleWindow(
        term->x11_display,
        RootWindow(term->x11_display, screen),
        100, 100, WINDOW_WIDTH, WINDOW_HEIGHT, 2,
        BlackPixel(term->x11_display, screen), WhitePixel(term->x11_display, screen)
    );
    
    XStoreName(term->x11_display, term->x11_window, "MyTerm - CS69201 Project");
    term->graphics_context = XCreateGC(term->x11_display, term->x11_window, 0, NULL);
    XSetForeground(term->x11_display, term->graphics_context, BlackPixel(term->x11_display, screen));
    XSelectInput(term->x11_display, term->x11_window, ExposureMask | KeyPressMask | ButtonPressMask);
    XMapWindow(term->x11_display, term->x11_window);
    XFlush(term->x11_display);
    
    printf("X11 GUI initialized successfully\n");
}

void draw_terminal_window(Terminal *term) {
    XClearWindow(term->x11_display, term->x11_window);
    
    // Draw tab headers
    int tab_width = WINDOW_WIDTH / MAX_TABS;
    for (int i = 0; i < MAX_TABS; i++) {
        int x_position = i * tab_width;
        char tab_label[32];
        snprintf(tab_label, sizeof(tab_label), "Tab %d", i + 1);
        
        if (i < term->total_tabs && term->tabs[i].is_active) {
            if (i == term->current_tab_index) {
                XFillRectangle(term->x11_display, term->x11_window, term->graphics_context,
                              x_position + 1, 1, tab_width - 2, 24);
                XSetForeground(term->x11_display, term->graphics_context,
                              WhitePixel(term->x11_display, DefaultScreen(term->x11_display)));
                XDrawString(term->x11_display, term->x11_window, term->graphics_context,
                           x_position + 10, 16, tab_label, strlen(tab_label));
                XSetForeground(term->x11_display, term->graphics_context,
                              BlackPixel(term->x11_display, DefaultScreen(term->x11_display)));
            } else {
                XDrawRectangle(term->x11_display, term->x11_window, term->graphics_context,
                              x_position, 0, tab_width, 25);
                XDrawString(term->x11_display, term->x11_window, term->graphics_context,
                           x_position + 10, 16, tab_label, strlen(tab_label));
            }
        }
    }
    
    // Draw content of current tab with AUTO-SCROLLING
    if (term->current_tab_index < term->total_tabs && 
        term->tabs[term->current_tab_index].is_active) {
        
        Tab *current_tab = &term->tabs[term->current_tab_index];
        
        // Calculate display area
        int display_start_y = 50;
        int display_end_y = WINDOW_HEIGHT - 80;
        int line_height = 16;
        int max_display_lines = (display_end_y - display_start_y) / line_height;
        
        // Split buffer into lines with auto-scrolling
        char *buffer_copy = malloc(strlen(current_tab->display_buffer) + 1);
        if (buffer_copy) {
            strcpy(buffer_copy, current_tab->display_buffer);
            
            char *lines[1000];
            int total_lines = 0;
            
            char *line_start = buffer_copy;
            char *newline_pos;
            
            while (line_start && *line_start && total_lines < 1000) {
                newline_pos = strchr(line_start, '\n');
                
                if (newline_pos) {
                    *newline_pos = '\0';
                    lines[total_lines] = line_start;
                    total_lines++;
                    line_start = newline_pos + 1;
                } else {
                    if (strlen(line_start) > 0) {
                        lines[total_lines] = line_start;
                        total_lines++;
                    }
                    break;
                }
            }
            
            // AUTOMATIC SCROLLING: Show last N lines that fit on screen
            int start_line = (total_lines > max_display_lines) ? 
                            total_lines - max_display_lines : 0;
            
            int y_position = display_start_y;
            for (int i = start_line; i < total_lines && y_position < display_end_y; i++) {
                int line_length = strlen(lines[i]);
                if (line_length > 0 && line_length < 200) {
                    XDrawString(term->x11_display, term->x11_window, term->graphics_context,
                               10, y_position, lines[i], line_length);
                }
                y_position += line_height;
            }
            
            free(buffer_copy);
        }
        
        // Draw current command input line at bottom with COLORED PROMPT
        int input_y = WINDOW_HEIGHT - 30;
        
        // Draw "user@myterm> " in blue color
        XSetForeground(term->x11_display, term->graphics_context, 0x0066CC);
        char prompt[] = "user@myterm> ";
        XDrawString(term->x11_display, term->x11_window, term->graphics_context,
                   10, input_y, prompt, strlen(prompt));
        
        // Draw user input in black color
        XSetForeground(term->x11_display, term->graphics_context,
                      BlackPixel(term->x11_display, DefaultScreen(term->x11_display)));
        
        int user_input_start = strlen(prompt);
        if (strlen(current_tab->current_command) > user_input_start) {
            char *user_text = current_tab->current_command + user_input_start;
            XDrawString(term->x11_display, term->x11_window, term->graphics_context,
                       10 + (user_input_start * 6), input_y, user_text, strlen(user_text));
        }
        
        // Draw cursor
        int cursor_x = 10 + (current_tab->cursor_position * 6);
        XDrawLine(term->x11_display, term->x11_window, term->graphics_context,
                 cursor_x, input_y - 12, cursor_x, input_y + 2);
    }
    
    XFlush(term->x11_display);
}

void handle_x11_events(Terminal *term) {
    XEvent event;
    fd_set file_descriptors;
    int x11_connection_fd = ConnectionNumber(term->x11_display);
    struct timeval timeout;
    
    while (1) {
        // Process all pending X11 events
        while (XPending(term->x11_display)) {
            XNextEvent(term->x11_display, &event);
            
            switch (event.type) {
                case Expose:
                    draw_terminal_window(term);
                    break;
                    
                case KeyPress:
                    process_keyboard_input(term, &event.xkey);
                    draw_terminal_window(term);
                    break;
                    
                case ButtonPress:
                    if (event.xbutton.y < 25) {
                        int tab_width = WINDOW_WIDTH / MAX_TABS;
                        int clicked_tab = event.xbutton.x / tab_width;
                        
                        if (clicked_tab < term->total_tabs && clicked_tab >= 0 && 
                            term->tabs[clicked_tab].is_active) {
                            switch_to_tab(term, clicked_tab);
                        }
                    }
                    break;
            }
        }
        
        // Check for output from shell processes
        FD_ZERO(&file_descriptors);
        FD_SET(x11_connection_fd, &file_descriptors);
        int max_fd = x11_connection_fd;
        
        for (int i = 0; i < term->total_tabs; i++) {
            if (term->tabs[i].is_active && term->tabs[i].output_pipe[0] >= 0) {
                FD_SET(term->tabs[i].output_pipe[0], &file_descriptors);
                if (term->tabs[i].output_pipe[0] > max_fd) {
                    max_fd = term->tabs[i].output_pipe[0];
                }
            }
        }
        
        timeout.tv_sec = 0;
        timeout.tv_usec = 20000;
        
        int select_result = select(max_fd + 1, &file_descriptors, NULL, NULL, &timeout);
        
        if (select_result > 0) {
            for (int i = 0; i < term->total_tabs; i++) {
                if (term->tabs[i].is_active && term->tabs[i].output_pipe[0] >= 0 &&
                    FD_ISSET(term->tabs[i].output_pipe[0], &file_descriptors)) {
                    
                    char output_buffer[256];
                    ssize_t bytes_read = read(term->tabs[i].output_pipe[0], 
                                             output_buffer, sizeof(output_buffer) - 1);
                    
                    if (bytes_read > 0) {
                        output_buffer[bytes_read] = '\0';
                        add_text_to_buffer(&term->tabs[i], output_buffer);
                        
                        if (i == term->current_tab_index) {
                            draw_terminal_window(term);
                        }
                    } else if (bytes_read == 0) {
                        close(term->tabs[i].output_pipe[0]);
                        term->tabs[i].output_pipe[0] = -1;
                    }
                }
            }
        }
        
        // CRITICAL: Check multiwatch output every loop iteration
        check_multiwatch_output(term);
        if (multiwatch_active) {
            draw_terminal_window(term);
        }
    }
}

void process_keyboard_input(Terminal *term, XKeyEvent *event) {
    if (term->current_tab_index >= term->total_tabs || 
        !term->tabs[term->current_tab_index].is_active) {
        return;
    }
    
    Tab *current_tab = &term->tabs[term->current_tab_index];
    KeySym key_symbol = XLookupKeysym(event, 0);
    
    // Handle Control key combinations
    if (event->state & ControlMask) {
        switch (key_symbol) {
            case XK_c:
                // Ctrl+C - Handle multiWatch interruption FIRST
                if (multiwatch_active) {
                    // Drain any remaining output before killing
                    for (int i = 0; i < multiwatch_count; i++) {
                        if (multiwatch_pipes[i] >= 0) {
                            char buf[512];
                            ssize_t n;
                            while ((n = read(multiwatch_pipes[i], buf, sizeof(buf)-1)) > 0) {
                                buf[n] = '\0';
                                add_text_to_buffer(current_tab, buf);
                            }
                            close(multiwatch_pipes[i]);
                            multiwatch_pipes[i] = -1;
                        }
                        if (multiwatch_pids[i] > 0) {
                            kill(multiwatch_pids[i], SIGKILL);
                            waitpid(multiwatch_pids[i], NULL, WNOHANG);
                            multiwatch_pids[i] = 0;
                        }
                    }
                    multiwatch_active = 0;
                    multiwatch_count = 0;
                    term->multiwatch_running = 0;
                    
                    add_text_to_buffer(current_tab, "\nmultiWatch interrupted by Ctrl+C\n");
                    strcpy(current_tab->current_command, "user@myterm> ");
                    current_tab->cursor_position = strlen(current_tab->current_command);
                    return;
                }
                
                // Normal Ctrl+C for shell
                if (current_tab->shell_process_id > 0) {
                    kill(current_tab->shell_process_id, SIGINT);
                }
                return;
                
            case XK_z:
                if (current_tab->shell_process_id > 0) {
                    kill(current_tab->shell_process_id, SIGTSTP);
                    add_text_to_buffer(current_tab, "\n[Process suspended - use 'fg' to resume]\n");
                }
                return;
                
            case XK_a:
                current_tab->cursor_position = strlen("user@myterm> ");
                return;
                
            case XK_e:
                current_tab->cursor_position = strlen(current_tab->current_command);
                return;
                
            case XK_r:
                term->search_mode = 1;
                strcpy(current_tab->current_command, "Search: ");
                current_tab->cursor_position = strlen(current_tab->current_command);
                term->search_term[0] = '\0';
                return;
                
            case XK_t:
                if (term->total_tabs < MAX_TABS) {
                    initialize_new_tab(term, term->total_tabs);
                    if (term->tabs[term->total_tabs].is_active) {
                        term->current_tab_index = term->total_tabs;
                        term->total_tabs++;
                    }
                }
                return;
        }
    }
    
    // Handle special keys
    switch (key_symbol) {
        case XK_Return:
        case XK_KP_Enter:
            if (term->search_mode) {
                builtin_search_history(term);
                term->search_mode = 0;
            } else {
                int prompt_length = strlen("user@myterm> ");
                if (strlen(current_tab->current_command) > prompt_length) {
                    char command[MAX_CMD_LEN];
                    strcpy(command, current_tab->current_command + prompt_length);
                    execute_user_command(term, command);
                }
                
                strcpy(current_tab->current_command, "user@myterm> ");
                current_tab->cursor_position = strlen(current_tab->current_command);
            }
            break;
            
        case XK_BackSpace: {
            int prompt_length = term->search_mode ? strlen("Search: ") : strlen("user@myterm> ");
            if (current_tab->cursor_position > prompt_length) {
                int command_length = strlen(current_tab->current_command);
                
                for (int i = current_tab->cursor_position - 1; i < command_length; i++) {
                    current_tab->current_command[i] = current_tab->current_command[i + 1];
                }
                current_tab->cursor_position--;
                
                if (term->search_mode) {
                    int search_length = strlen(term->search_term);
                    if (search_length > 0) {
                        term->search_term[search_length - 1] = '\0';
                    }
                }
            }
            break;
        }
            
        case XK_Left: {
            int prompt_length = term->search_mode ? strlen("Search: ") : strlen("user@myterm> ");
            if (current_tab->cursor_position > prompt_length) {
                current_tab->cursor_position--;
            }
            break;
        }
            
        case XK_Right:
            if (current_tab->cursor_position < strlen(current_tab->current_command)) {
                current_tab->cursor_position++;
            }
            break;
            
        case XK_Up:
            if (!term->search_mode && term->history_count > 0) {
                if (term->history_position > 0) {
                    term->history_position--;
                }
                snprintf(current_tab->current_command, MAX_CMD_LEN,
                        "user@myterm> %s", term->command_history[term->history_position]);
                current_tab->cursor_position = strlen(current_tab->current_command);
            }
            break;
            
        case XK_Down:
            if (!term->search_mode && term->history_count > 0) {
                if (term->history_position < term->history_count - 1) {
                    term->history_position++;
                    snprintf(current_tab->current_command, MAX_CMD_LEN,
                            "user@myterm> %s", term->command_history[term->history_position]);
                } else {
                    strcpy(current_tab->current_command, "user@myterm> ");
                }
                current_tab->cursor_position = strlen(current_tab->current_command);
            }
            break;
            
        case XK_Tab:
            if (!term->search_mode) {
                int prompt_length = strlen("user@myterm> ");
                if (strlen(current_tab->current_command) > prompt_length) {
                    char *command_part = current_tab->current_command + prompt_length;
                    autocomplete_filename(term, command_part);
                    current_tab->cursor_position = strlen(current_tab->current_command);
                }
            }
            break;
            
        case XK_Escape:
            if (term->search_mode) {
                term->search_mode = 0;
                strcpy(current_tab->current_command, "user@myterm> ");
                current_tab->cursor_position = strlen(current_tab->current_command);
            }
            break;
            
        default: {
            char character_buffer[10];
            int characters_typed = XLookupString(event, character_buffer, 
                                               sizeof(character_buffer) - 1, NULL, NULL);
            
            if (characters_typed == 1 && character_buffer[0] >= 32 && character_buffer[0] <= 126) {
                int command_length = strlen(current_tab->current_command);
                
                if (command_length < MAX_CMD_LEN - 2) {
                    for (int i = command_length; i >= current_tab->cursor_position; i--) {
                        current_tab->current_command[i + 1] = current_tab->current_command[i];
                    }
                    
                    current_tab->current_command[current_tab->cursor_position] = character_buffer[0];
                    current_tab->cursor_position++;
                    
                    if (term->search_mode) {
                        int search_length = strlen(term->search_term);
                        if (search_length < MAX_CMD_LEN - 1) {
                            term->search_term[search_length] = character_buffer[0];
                            term->search_term[search_length + 1] = '\0';
                        }
                    }
                }
            }
            break;
        }
    }
}