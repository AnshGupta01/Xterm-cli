#include "xterm.h"

void init_scroll_view(ScrollView *scroll) {
    if (!scroll) return;
    
    scroll->total_lines = 0;
    scroll->scroll_offset = 0;
    scroll->visible_lines = 0;
    scroll->is_active = 0;
    scroll->title[0] = '\0';
    
    for (int i = 0; i < MAX_SCROLL_LINES; i++) {
        scroll->content[i][0] = '\0';
    }
}

void enter_scroll_mode(Tab *tab, const char *title) {
    if (!tab) return;
    
    init_scroll_view(&tab->scroll_view);
    tab->scroll_view.is_active = 1;
    
    if (title) {
        strncpy(tab->scroll_view.title, title, sizeof(tab->scroll_view.title) - 1);
        tab->scroll_view.title[sizeof(tab->scroll_view.title) - 1] = '\0';
    }
}

void exit_scroll_mode(Tab *tab) {
    if (!tab) return;
    tab->scroll_view.is_active = 0;
    init_scroll_view(&tab->scroll_view);
}

void add_line_to_scroll(ScrollView *scroll, const char *line) {
    if (!scroll || !line || scroll->total_lines >= MAX_SCROLL_LINES) return;
    
    strncpy(scroll->content[scroll->total_lines], line, MAX_CMD_LEN - 1);
    scroll->content[scroll->total_lines][MAX_CMD_LEN - 1] = '\0';
    scroll->total_lines++;
}

// Basic scroll navigation
void scroll_up(ScrollView *scroll) {
    if (!scroll || !scroll->is_active) return;
    if (scroll->scroll_offset > 0) {
        scroll->scroll_offset--;
    }
}

void scroll_down(ScrollView *scroll) {
    if (!scroll || !scroll->is_active) return;
    int max_offset = scroll->total_lines - scroll->visible_lines;
    if (max_offset < 0) max_offset = 0;
    if (scroll->scroll_offset < max_offset) {
        scroll->scroll_offset++;
    }
}

void scroll_page_up(ScrollView *scroll) {
    if (!scroll || !scroll->is_active) return;
    scroll->scroll_offset -= scroll->visible_lines;
    if (scroll->scroll_offset < 0) scroll->scroll_offset = 0;
}

void scroll_page_down(ScrollView *scroll) {
    if (!scroll || !scroll->is_active) return;
    int max_offset = scroll->total_lines - scroll->visible_lines;
    if (max_offset < 0) max_offset = 0;
    scroll->scroll_offset += scroll->visible_lines;
    if (scroll->scroll_offset > max_offset) scroll->scroll_offset = max_offset;
}

void scroll_to_top(ScrollView *scroll) {
    if (!scroll || !scroll->is_active) return;
    scroll->scroll_offset = 0;
}

void scroll_to_bottom(ScrollView *scroll) {
    if (!scroll || !scroll->is_active) return;
    int max_offset = scroll->total_lines - scroll->visible_lines;
    if (max_offset < 0) max_offset = 0;
    scroll->scroll_offset = max_offset;
}

void draw_scroll_view(Terminal *term, ScrollView *scroll) {
    if (!term || !scroll || !scroll->is_active) return;
    
    int start_y = 50;
    int end_y = WINDOW_HEIGHT - 80;
    int line_height = 16;
    int max_visible_lines = (end_y - start_y) / line_height;
    scroll->visible_lines = max_visible_lines;
    
    // Clear drawing area
    XClearArea(term->x11_display, term->x11_window, 0, start_y, WINDOW_WIDTH, end_y - start_y, False);
    
    // Draw title bar
    XSetForeground(term->x11_display, term->graphics_context, 0x0066CC);
    char title_text[512];
    snprintf(title_text, sizeof(title_text), "%s (Lines %d-%d of %d) - Use arrows/PageUp/PageDown to scroll, ESC to exit",
             scroll->title,
             scroll->scroll_offset + 1,
             scroll->scroll_offset + max_visible_lines > scroll->total_lines ? scroll->total_lines : scroll->scroll_offset + max_visible_lines,
             scroll->total_lines);
    XDrawString(term->x11_display, term->x11_window, term->graphics_context,
                10, start_y - 10, title_text, strlen(title_text));
    
    // Reset to black for content
    XSetForeground(term->x11_display, term->graphics_context,
                   BlackPixel(term->x11_display, DefaultScreen(term->x11_display)));
    
    // Draw separator
    XDrawLine(term->x11_display, term->x11_window, term->graphics_context,
              10, start_y - 5, WINDOW_WIDTH - 10, start_y - 5);
    
    // Draw the visible lines
    int current_y = start_y;
    for (int i = scroll->scroll_offset; i < scroll->total_lines && i < scroll->scroll_offset + max_visible_lines; i++) {
        if (current_y >= end_y) break;
        int line_len = strlen(scroll->content[i]);
        if (line_len > 0 && line_len < 200) {
            XDrawString(term->x11_display, term->x11_window, term->graphics_context,
                        10, current_y, scroll->content[i], line_len);
        }
        current_y += line_height;
    }
    
    // Draw scroll indicator if needed
    if (scroll->total_lines > max_visible_lines) {
        int scrollbar_height = end_y - start_y;
        int scrollbar_x = WINDOW_WIDTH - 20;
        
        XDrawRectangle(term->x11_display, term->x11_window, term->graphics_context,
                       scrollbar_x, start_y, 10, scrollbar_height);
        
        int thumb_height = (scrollbar_height * max_visible_lines) / scroll->total_lines;
        if (thumb_height < 10) thumb_height = 10;
        
        int thumb_position = (scrollbar_height * scroll->scroll_offset) / scroll->total_lines;
        if (thumb_position + thumb_height > scrollbar_height) {
            thumb_position = scrollbar_height - thumb_height;
        }
        
        XFillRectangle(term->x11_display, term->x11_window, term->graphics_context,
                       scrollbar_x + 1, start_y + thumb_position, 8, thumb_height);
    }
}

void draw_terminal_window(Terminal *term) {
    XClearWindow(term->x11_display, term->x11_window);
    
    // Draw tabs at top
    int tab_width = WINDOW_WIDTH / MAX_TABS;
    for (int i = 0; i < MAX_TABS; i++) {
        int tab_x = i * tab_width;
        char tab_label[32];
        snprintf(tab_label, sizeof(tab_label), "Tab %d", i + 1);
        
        if (i < term->total_tabs && term->tabs[i].is_active) {
            if (i == term->current_tab_index) {
                // Active tab - filled
                XFillRectangle(term->x11_display, term->x11_window, term->graphics_context,
                               tab_x + 1, 1, tab_width - 2, 24);
                XSetForeground(term->x11_display, term->graphics_context,
                               WhitePixel(term->x11_display, DefaultScreen(term->x11_display)));
                XDrawString(term->x11_display, term->x11_window, term->graphics_context,
                            tab_x + 10, 16, tab_label, strlen(tab_label));
                XSetForeground(term->x11_display, term->graphics_context,
                               BlackPixel(term->x11_display, DefaultScreen(term->x11_display)));
            } else {
                // Inactive tab - outlined
                XDrawRectangle(term->x11_display, term->x11_window, term->graphics_context,
                               tab_x, 0, tab_width, 25);
                XDrawString(term->x11_display, term->x11_window, term->graphics_context,
                            tab_x + 10, 16, tab_label, strlen(tab_label));
            }
        }
    }
    
    if (term->current_tab_index < term->total_tabs &&
        term->tabs[term->current_tab_index].is_active) {
        Tab *current_tab = &term->tabs[term->current_tab_index];
        
        if (current_tab->scroll_view.is_active) {
            draw_scroll_view(term, &current_tab->scroll_view);
        } else {
            // Regular terminal view
            int content_start_y = 50;
            int content_end_y = WINDOW_HEIGHT - 80;
            int line_height = 16;
            int max_lines = (content_end_y - content_start_y) / line_height;
            
            // Parse and display buffer content
            char *buffer_copy = malloc(strlen(current_tab->display_buffer) + 1);
            if (buffer_copy) {
                strcpy(buffer_copy, current_tab->display_buffer);
                char *lines[1000];
                int line_count = 0;
                char *line_ptr = buffer_copy;
                
                while (line_ptr && *line_ptr && line_count < 1000) {
                    char *newline = strchr(line_ptr, '\n');
                    if (newline) {
                        *newline = '\0';
                        lines[line_count++] = line_ptr;
                        line_ptr = newline + 1;
                    } else {
                        if (*line_ptr) lines[line_count++] = line_ptr;
                        break;
                    }
                }
                
                // Auto-scroll to show recent lines
                int start_line = (line_count > max_lines) ? (line_count - max_lines) : 0;
                int current_y = content_start_y;
                for (int i = start_line; i < line_count && current_y < content_end_y; i++) {
                    int line_len = strlen(lines[i]);
                    if (line_len > 0 && line_len < 200) {
                        XDrawString(term->x11_display, term->x11_window, term->graphics_context,
                                    10, current_y, lines[i], line_len);
                    }
                    current_y += line_height;
                }
                free(buffer_copy);
            }
            
            // Draw input area
            int input_y = WINDOW_HEIGHT - 30;
            XSetForeground(term->x11_display, term->graphics_context, 0x0066CC);
            
            // Show appropriate prompt
            char *prompt_text;
            int prompt_len;
            if (term->search_mode) {
                prompt_text = "Search: ";
                prompt_len = strlen("Search: ");
            } else {
                prompt_text = "anshgpt@xterm> ";
                prompt_len = strlen("anshgpt@xterm> ");
            }
            
            XDrawString(term->x11_display, term->x11_window, term->graphics_context,
                        10, input_y, prompt_text, prompt_len);
            
            // Reset color for user input
            XSetForeground(term->x11_display, term->graphics_context,
                           BlackPixel(term->x11_display, DefaultScreen(term->x11_display)));
            
            // Draw user input (after prompt)
            if (strlen(current_tab->current_command) > prompt_len) {
                char *user_input = current_tab->current_command + prompt_len;
                XDrawString(term->x11_display, term->x11_window, term->graphics_context,
                            10 + (prompt_len * 6), input_y, user_input, strlen(user_input));
            }
            
            // Draw cursor
            int cursor_x = 10 + (current_tab->cursor_position * 6);
            XDrawLine(term->x11_display, term->x11_window, term->graphics_context,
                      cursor_x, input_y - 12, cursor_x, input_y + 2);
        }
    }
    
    XFlush(term->x11_display);
}

void process_scroll_input(Terminal *term, XKeyEvent *event) {
    Tab *current_tab = &term->tabs[term->current_tab_index];
    ScrollView *scroll = &current_tab->scroll_view;
    KeySym key_symbol = XLookupKeysym(event, 0);
    
    switch (key_symbol) {
        case XK_Escape:
            exit_scroll_mode(current_tab);
            break;
        case XK_Up:
            scroll_up(scroll);
            break;
        case XK_Down:
            scroll_down(scroll);
            break;
        case XK_Page_Up:
            scroll_page_up(scroll);
            break;
        case XK_Page_Down:
            scroll_page_down(scroll);
            break;
        case XK_Home:
            scroll_to_top(scroll);
            break;
        case XK_End:
            scroll_to_bottom(scroll);
            break;
    }
}

void process_keyboard_input(Terminal *term, XKeyEvent *event) {
    if (term->current_tab_index >= term->total_tabs ||
        !term->tabs[term->current_tab_index].is_active) {
        return;
    }
    
    Tab *current_tab = &term->tabs[term->current_tab_index];
    
    // Handle scroll mode separately
    if (current_tab->scroll_view.is_active) {
        process_scroll_input(term, event);
        return;
    }
    
    KeySym key_symbol = XLookupKeysym(event, 0);
    
    // Handle control key combinations
    if (event->state & ControlMask) {
        switch (key_symbol) {
            case XK_c:
                // Stop multiwatch if running
                if (multiwatch_active) {
                    for (int i = 0; i < multiwatch_count; i++) {
                        if (multiwatch_pipes[i] >= 0) {
                            char temp_buffer[512];
                            ssize_t bytes_read;
                            while ((bytes_read = read(multiwatch_pipes[i], temp_buffer, sizeof(temp_buffer) - 1)) > 0) {
                                temp_buffer[bytes_read] = '\0';
                                add_text_to_buffer(current_tab, temp_buffer);
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
                    strcpy(current_tab->current_command, "anshgpt@xterm> ");
                    current_tab->cursor_position = strlen(current_tab->current_command);
                    return;
                }
                if (current_tab->shell_process_id > 0) kill(current_tab->shell_process_id, SIGINT);
                return;
            
            case XK_z:
                if (current_tab->shell_process_id > 0) {
                    kill(current_tab->shell_process_id, SIGTSTP);
                    add_text_to_buffer(current_tab, "\n[Process suspended - use 'fg' to resume]\n");
                }
                return;
            
            case XK_a:
                if (term->search_mode) {
                    current_tab->cursor_position = strlen("Search: ");
                } else {
                    current_tab->cursor_position = strlen("anshgpt@xterm> ");
                }
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
        case XK_KP_Enter: {
            if (term->search_mode) {
                builtin_search_history(term);
                term->search_mode = 0;
            } else {
                int prompt_len = strlen("anshgpt@xterm> ");
                if (strlen(current_tab->current_command) > prompt_len) {
                    char command_text[MAX_CMD_LEN];
                    strcpy(command_text, current_tab->current_command + prompt_len);
                    execute_user_command(term, command_text);
                }
                strcpy(current_tab->current_command, "anshgpt@xterm> ");
                current_tab->cursor_position = strlen(current_tab->current_command);
            }
            break;
        }
        
        case XK_BackSpace: {
            int prompt_len;
            if (term->search_mode) {
                prompt_len = strlen("Search: ");
            } else {
                prompt_len = strlen("anshgpt@xterm> ");
            }
            
            if (current_tab->cursor_position > prompt_len) {
                int cmd_len = strlen(current_tab->current_command);
                for (int i = current_tab->cursor_position - 1; i < cmd_len; i++) {
                    current_tab->current_command[i] = current_tab->current_command[i + 1];
                }
                current_tab->cursor_position--;
                if (term->search_mode) {
                    int search_len = strlen(term->search_term);
                    if (search_len > 0) term->search_term[search_len - 1] = '\0';
                }
            }
            break;
        }
        
        case XK_Left: {
            int prompt_len;
            if (term->search_mode) {
                prompt_len = strlen("Search: ");
            } else {
                prompt_len = strlen("anshgpt@xterm> ");
            }
            
            if (current_tab->cursor_position > prompt_len) current_tab->cursor_position--;
            break;
        }
        
        case XK_Right:
            if (current_tab->cursor_position < strlen(current_tab->current_command)) current_tab->cursor_position++;
            break;
        
        case XK_Up:
            if (!term->search_mode && term->history_count > 0) {
                if (term->history_position > 0) term->history_position--;
                snprintf(current_tab->current_command, MAX_CMD_LEN,
                         "anshgpt@xterm> %s",
                         term->command_history[term->history_position]);
                current_tab->cursor_position = strlen(current_tab->current_command);
            }
            break;
        
        case XK_Down:
            if (!term->search_mode && term->history_count > 0) {
                if (term->history_position < term->history_count - 1) {
                    term->history_position++;
                    snprintf(current_tab->current_command, MAX_CMD_LEN,
                             "anshgpt@xterm> %s",
                             term->command_history[term->history_position]);
                } else {
                    strcpy(current_tab->current_command, "anshgpt@xterm> ");
                }
                current_tab->cursor_position = strlen(current_tab->current_command);
            }
            break;
        
        case XK_Tab:
            if (!term->search_mode) {
                int prompt_len = strlen("anshgpt@xterm> ");
                if (strlen(current_tab->current_command) > prompt_len) {
                    char *partial = current_tab->current_command + prompt_len;
                    autocomplete_filename(term, partial);
                    current_tab->cursor_position = strlen(current_tab->current_command);
                }
            }
            break;
        
        case XK_Escape:
            if (term->search_mode) {
                term->search_mode = 0;
                strcpy(current_tab->current_command, "anshgpt@xterm> ");
                current_tab->cursor_position = strlen(current_tab->current_command);
            }
            break;
        
        default: {
            char input_buffer[10];
            int bytes_read = XLookupString(event, input_buffer, sizeof(input_buffer) - 1, NULL, NULL);
            if (bytes_read == 1 && input_buffer[0] >= 32 && input_buffer[0] <= 126) {
                int cmd_len = strlen(current_tab->current_command);
                if (cmd_len < MAX_CMD_LEN - 2) {
                    // Insert character at cursor position
                    for (int i = cmd_len; i >= current_tab->cursor_position; i--) {
                        current_tab->current_command[i + 1] = current_tab->current_command[i];
                    }
                    current_tab->current_command[current_tab->cursor_position] = input_buffer[0];
                    current_tab->cursor_position++;
                    if (term->search_mode) {
                        int search_len = strlen(term->search_term);
                        if (search_len < MAX_CMD_LEN - 1) {
                            term->search_term[search_len] = input_buffer[0];
                            term->search_term[search_len + 1] = '\0';
                        }
                    }
                }
            }
            break;
        }
    }
}

void handle_x11_events(Terminal *term) {
    XEvent event;
    fd_set read_fds;
    int x11_fd = ConnectionNumber(term->x11_display);
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
        
        // Check for shell output using select
        FD_ZERO(&read_fds);
        FD_SET(x11_fd, &read_fds);
        int max_fd = x11_fd;
        
        for (int i = 0; i < term->total_tabs; i++) {
            if (term->tabs[i].is_active && term->tabs[i].output_pipe[0] >= 0) {
                FD_SET(term->tabs[i].output_pipe[0], &read_fds);
                if (term->tabs[i].output_pipe[0] > max_fd) max_fd = term->tabs[i].output_pipe[0];
            }
        }
        
        timeout.tv_sec = 0;
        timeout.tv_usec = 20000;  // 20ms timeout
        int select_result = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (select_result > 0) {
            for (int i = 0; i < term->total_tabs; i++) {
                if (term->tabs[i].is_active &&
                    term->tabs[i].output_pipe[0] >= 0 &&
                    FD_ISSET(term->tabs[i].output_pipe[0], &read_fds)) {
                    char output_buffer[256];
                    ssize_t bytes_read = read(term->tabs[i].output_pipe[0], output_buffer, sizeof(output_buffer) - 1);
                    if (bytes_read > 0) {
                        output_buffer[bytes_read] = '\0';
                        add_text_to_buffer(&term->tabs[i], output_buffer);
                        if (i == term->current_tab_index) draw_terminal_window(term);
                    } else if (bytes_read == 0) {
                        close(term->tabs[i].output_pipe[0]);
                        term->tabs[i].output_pipe[0] = -1;
                    }
                }
            }
        }
        
        // Always check multiwatch and redraw if active
        check_multiwatch_output(term);
        if (multiwatch_active) draw_terminal_window(term);
    }
}

void setup_x11_gui(Terminal *term) {
    term->x11_display = XOpenDisplay(NULL);
    if (!term->x11_display) {
        printf("ERROR: Can't open X11 display. Is XQuartz running?\n");
        exit(1);
    }
    
    int screen = DefaultScreen(term->x11_display);
    term->x11_window = XCreateSimpleWindow(
        term->x11_display,
        RootWindow(term->x11_display, screen),
        100, 100, WINDOW_WIDTH, WINDOW_HEIGHT, 2,
        BlackPixel(term->x11_display, screen),
        WhitePixel(term->x11_display, screen)
    );
    
    XStoreName(term->x11_display, term->x11_window, "Xterm - Project by Ansh Gupta");
    term->graphics_context = XCreateGC(term->x11_display, term->x11_window, 0, NULL);
    XSetForeground(term->x11_display, term->graphics_context,
                   BlackPixel(term->x11_display, screen));
    
    XSelectInput(term->x11_display, term->x11_window,
                 ExposureMask | KeyPressMask | ButtonPressMask);
    XMapWindow(term->x11_display, term->x11_window);
    XFlush(term->x11_display);
    
    printf("X11 GUI ready\n");
}