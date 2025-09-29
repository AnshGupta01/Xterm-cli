#include "../include/myterm.h"

void init_x11(Terminal *term) {
    // Connect to X server
    term->display = XOpenDisplay(NULL);
    if (!term->display) {
        fprintf(stderr, "Error: Cannot connect to X server. Make sure XQuartz is running.\n");
        fprintf(stderr, "Try: export DISPLAY=:0\n");
        exit(1);
    }

    term->screen = DefaultScreen(term->display);

    // Create window
    term->window = XCreateSimpleWindow(
        term->display,
        RootWindow(term->display, term->screen),
        100, 100,  // x, y position
        WINDOW_WIDTH, WINDOW_HEIGHT,
        1,         // border width
        BlackPixel(term->display, term->screen),  // border color
        WhitePixel(term->display, term->screen)   // background color
    );

    // Set window properties
    XStoreName(term->display, term->window, "MyTerm - Custom Terminal");

    // Load font - try multiple options for better compatibility
    const char* font_names[] = {
        "-*-courier-medium-r-*-*-12-*-*-*-*-*-*-*",
        "-*-fixed-medium-r-*-*-12-*-*-*-*-*-*-*", 
        "fixed",
        "*"
    };

    term->font = NULL;
    for (int i = 0; i < 4 && !term->font; i++) {
        term->font = XLoadQueryFont(term->display, font_names[i]);
    }

    if (!term->font) {
        fprintf(stderr, "Warning: Could not load any font\n");
        // Continue anyway - X11 will use default
    }

    // Create graphics context
    XGCValues gc_values;
    if (term->font) {
        gc_values.font = term->font->fid;
    }
    gc_values.foreground = BlackPixel(term->display, term->screen);
    gc_values.background = WhitePixel(term->display, term->screen);

    unsigned long valuemask = GCForeground | GCBackground;
    if (term->font) {
        valuemask |= GCFont;
    }

    term->gc = XCreateGC(term->display, term->window, valuemask, &gc_values);

    // Select events we want to receive
    XSelectInput(term->display, term->window, 
                ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask | FocusChangeMask);

    // Map window (make it visible)
    XMapWindow(term->display, term->window);

    // Flush all X requests - reduce latency
    XFlush(term->display);
    XSync(term->display, False);  // Force synchronization
}

void draw_terminal(Terminal *term) {
    // Clear window
    XClearWindow(term->display, term->window);

    // Draw tab bar
    draw_tabs(term);

    // Draw text buffer
    draw_text_buffer(term);

    // Flush changes immediately - reduce latency
    XFlush(term->display);
}

void draw_tabs(Terminal *term) {
    int tab_width = WINDOW_WIDTH / MAX_TABS;
    int tab_height = 25;  // Slightly larger for better visibility

    for (int i = 0; i < MAX_TABS; i++) {
        int x = i * tab_width;
        int y = 0;

        // Draw tab background
        if (i < term->num_tabs && term->tabs[i].active) {
            if (i == term->current_tab) {
                // Active tab - white background
                XSetForeground(term->display, term->gc, WhitePixel(term->display, term->screen));
                XFillRectangle(term->display, term->window, term->gc, 
                              x, y, tab_width, tab_height);
                XSetForeground(term->display, term->gc, BlackPixel(term->display, term->screen));
            } else {
                // Inactive tab - light gray background
                XSetForeground(term->display, term->gc, 0xE0E0E0);
                XFillRectangle(term->display, term->window, term->gc, 
                              x, y, tab_width, tab_height);
                XSetForeground(term->display, term->gc, BlackPixel(term->display, term->screen));
            }

            // Draw tab label
            char tab_label[16];
            sprintf(tab_label, "Tab %d", i + 1);
            int text_x = x + 8;
            int text_y = y + 16;
            XDrawString(term->display, term->window, term->gc,
                       text_x, text_y, tab_label, strlen(tab_label));
        }

        // Draw tab border
        XDrawRectangle(term->display, term->window, term->gc,
                      x, y, tab_width, tab_height);
    }
}

void draw_text_buffer(Terminal *term) {
    if (term->current_tab >= term->num_tabs || !term->tabs[term->current_tab].active) {
        return;
    }

    Tab *tab = &term->tabs[term->current_tab];
    int y_offset = 30;  // Start below tab bar
    int line_height = 16;  // Fixed line height for consistency
    int char_width = 8;    // Fixed character width
    int margin_left = 8;   // Left margin

    // Get font metrics if available
    if (term->font) {
        line_height = term->font->ascent + term->font->descent + 2;
        char_width = term->font->max_bounds.width;
    }

    // Draw buffer content line by line
    char *buffer_copy = malloc(strlen(tab->buffer) + 1);
    strcpy(buffer_copy, tab->buffer);

    char *line_start = buffer_copy;
    int line_num = 0;
    int max_display_lines = (WINDOW_HEIGHT - y_offset - 40) / line_height;  // Leave room for input

    // Count total lines first
    int total_lines = 0;
    for (char *p = buffer_copy; *p; p++) {
        if (*p == '\n') total_lines++;
    }

    // Start from the bottom if we have too many lines
    int start_line = (total_lines > max_display_lines) ? total_lines - max_display_lines : 0;
    int current_line = 0;

    for (char *p = buffer_copy; *p && line_num < max_display_lines; p++) {
        if (*p == '\n') {
            if (current_line >= start_line) {
                // Draw this line
                *p = '\0';  // Temporarily null-terminate
                if (strlen(line_start) > 0) {
                    XDrawString(term->display, term->window, term->gc,
                               margin_left, y_offset + (line_num * line_height) + line_height - 3,
                               line_start, strlen(line_start));
                }
                line_num++;
            }
            current_line++;
            line_start = p + 1;
        }
    }

    free(buffer_copy);

    // Draw current input line
    int input_y = y_offset + (line_num * line_height) + line_height - 3;

    // Ensure input line is visible
    if (input_y > WINDOW_HEIGHT - 30) {
        input_y = WINDOW_HEIGHT - 25;
    }

    XDrawString(term->display, term->window, term->gc,
               margin_left, input_y, tab->current_line, strlen(tab->current_line));

    // Draw cursor - IMPROVED IMPLEMENTATION
    draw_cursor(term, margin_left, input_y, char_width, line_height);
}

void draw_cursor(Terminal *term, int line_x, int line_y, int char_width, int line_height) {
    Tab *tab = &term->tabs[term->current_tab];

    // Calculate cursor position
    int cursor_char_pos = tab->cursor_pos;
    int cursor_x = line_x + (cursor_char_pos * char_width);
    int cursor_y_top = line_y - line_height + 4;
    int cursor_y_bottom = line_y + 2;

    // Make cursor visible
    XSetForeground(term->display, term->gc, BlackPixel(term->display, term->screen));

    // Draw cursor as a vertical line
    XDrawLine(term->display, term->window, term->gc,
              cursor_x, cursor_y_top, cursor_x, cursor_y_bottom);

    // Draw a slightly thicker cursor for better visibility
    XDrawLine(term->display, term->window, term->gc,
              cursor_x + 1, cursor_y_top, cursor_x + 1, cursor_y_bottom);
}

void handle_x11_events(Terminal *term) {
    XEvent event;
    fd_set read_fds;
    int x11_fd = ConnectionNumber(term->display);
    struct timeval short_timeout = {0, 10000}; // 10ms - much shorter for responsiveness

    while (1) {
        // Check for X11 events immediately
        while (XPending(term->display)) {
            XNextEvent(term->display, &event);

            switch (event.type) {
                case Expose:
                    if (event.xexpose.count == 0) {
                        draw_terminal(term);
                    }
                    break;

                case KeyPress:
                    handle_key_press(term, &event.xkey);
                    // Immediate redraw after key press
                    draw_terminal(term);
                    break;

                case ButtonPress:
                    // Handle tab clicks
                    if (event.xbutton.y < 25) {  // Updated for new tab height
                        int tab_width = WINDOW_WIDTH / MAX_TABS;
                        int clicked_tab = event.xbutton.x / tab_width;
                        if (clicked_tab < term->num_tabs && 
                            term->tabs[clicked_tab].active) {
                            switch_tab(term, clicked_tab);
                        }
                    }
                    break;

                case ConfigureNotify:
                    // Handle window resize
                    draw_terminal(term);
                    break;

                case FocusIn:
                    // Redraw when window gains focus
                    draw_terminal(term);
                    break;
            }
        }

        // Check for output from shell processes with short timeout
        FD_ZERO(&read_fds);
        FD_SET(x11_fd, &read_fds);
        int max_fd = x11_fd;

        for (int i = 0; i < term->num_tabs; i++) {
            if (term->tabs[i].active) {
                FD_SET(term->tabs[i].pipe_out[0], &read_fds);
                if (term->tabs[i].pipe_out[0] > max_fd) {
                    max_fd = term->tabs[i].pipe_out[0];
                }
            }
        }

        int result = select(max_fd + 1, &read_fds, NULL, NULL, &short_timeout);

        if (result > 0) {
            // Check shell output
            for (int i = 0; i < term->num_tabs; i++) {
                if (term->tabs[i].active && 
                    FD_ISSET(term->tabs[i].pipe_out[0], &read_fds)) {

                    char output[1024];
                    ssize_t bytes_read = read(term->tabs[i].pipe_out[0], 
                                            output, sizeof(output) - 1);
                    if (bytes_read > 0) {
                        output[bytes_read] = '\0';
                        append_to_buffer(term, output);
                        if (i == term->current_tab) {
                            draw_terminal(term);
                        }
                    }
                }
            }
        }

        // Much shorter sleep for better responsiveness
        usleep(5000); // 5ms delay instead of 10ms
    }
}

void handle_key_press(Terminal *term, XKeyEvent *event) {
    char buffer[32];
    KeySym keysym;
    int len = XLookupString(event, buffer, sizeof(buffer), &keysym, NULL);

    Tab *tab = &term->tabs[term->current_tab];

    // Handle special keys
    if (event->state & ControlMask) {
        switch (keysym) {
            case XK_a:  // Ctrl+A - move to start of line
                move_cursor_start(term);
                return; // No need to redraw here, will be redrawn in main loop

            case XK_e:  // Ctrl+E - move to end of line
                move_cursor_end(term);
                return;

            case XK_c:  // Ctrl+C - interrupt
                kill(tab->shell_pid, SIGINT);
                clear_current_line(term);
                return;

            case XK_z:  // Ctrl+Z - suspend
                kill(tab->shell_pid, SIGTSTP);
                return;

            case XK_r:  // Ctrl+R - search history
                term->search_mode = 1;
                strcpy(tab->current_line, "Enter search term: ");
                tab->cursor_pos = strlen(tab->current_line);
                return;

            case XK_t:  // Ctrl+T - new tab
                create_new_tab(term);
                return;
        }
    }

    // Handle arrow keys for cursor movement
    switch (keysym) {
        case XK_Left:
            if (tab->cursor_pos > strlen(PROMPT)) {
                tab->cursor_pos--;
            }
            return;

        case XK_Right:
            if (tab->cursor_pos < strlen(tab->current_line)) {
                tab->cursor_pos++;
            }
            return;

        case XK_Home:
            move_cursor_start(term);
            return;

        case XK_End:
            move_cursor_end(term);
            return;
    }

    // Handle function keys for tab switching
    if (keysym >= XK_F1 && keysym <= XK_F5) {
        int tab_num = keysym - XK_F1;
        if (tab_num < term->num_tabs && term->tabs[tab_num].active) {
            switch_tab(term, tab_num);
        }
        return;
    }

    // Handle regular keys
    if (len > 0) {
        char c = buffer[0];

        switch (c) {
            case '\r':
            case '\n':  // Enter key
                if (term->search_mode) {
                    // Process search
                    char search_term[MAX_COMMAND_LENGTH];
                    strcpy(search_term, tab->current_line + strlen("Enter search term: "));
                    search_history(term, search_term);
                    term->search_mode = 0;
                } else {
                    // Process command
                    char command[MAX_COMMAND_LENGTH];
                    strcpy(command, tab->current_line + strlen(PROMPT));

                    // Add to history
                    if (strlen(command) > 0) {
                        add_to_history(term, command);
                    }

                    // Send command to shell
                    strcat(command, "\n");
                    write(tab->pipe_in[1], command, strlen(command));

                    // Add command to display buffer
                    append_to_buffer(term, tab->current_line);
                    append_to_buffer(term, "\n");
                }

                clear_current_line(term);
                break;

            case '\b':
            case 127:  // Backspace
                if (tab->cursor_pos > strlen(PROMPT)) {
                    // Remove character at cursor position
                    int line_len = strlen(tab->current_line);
                    if (tab->cursor_pos > 0 && tab->cursor_pos <= line_len) {
                        memmove(tab->current_line + tab->cursor_pos - 1,
                               tab->current_line + tab->cursor_pos,
                               line_len - tab->cursor_pos + 1);
                        tab->cursor_pos--;
                    }
                }
                break;

            case '\t':  // Tab key - autocomplete
                if (!term->search_mode) {
                    char *last_word = strrchr(tab->current_line, ' ');
                    if (last_word) {
                        autocomplete_filename(term, last_word + 1);
                    } else {
                        autocomplete_filename(term, tab->current_line + strlen(PROMPT));
                    }
                }
                break;

            default:
                // Regular character input - insert at cursor position
                if (c >= 32 && c < 127) {
                    int line_len = strlen(tab->current_line);
                    if (line_len < MAX_COMMAND_LENGTH - 1) {
                        // Make room for new character
                        memmove(tab->current_line + tab->cursor_pos + 1,
                               tab->current_line + tab->cursor_pos,
                               line_len - tab->cursor_pos + 1);

                        // Insert new character
                        tab->current_line[tab->cursor_pos] = c;
                        tab->cursor_pos++;
                    }
                }
                break;
        }
    }
}
