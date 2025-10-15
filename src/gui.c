#include "xterm.h"

/* Initialize scroll view structure */
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

/* Enter scroll mode with given title */
void enter_scroll_mode(Tab *tab, const char *title) {
    if (!tab) return;
    init_scroll_view(&tab->scroll_view);
    tab->scroll_view.is_active = 1;
    if (title) {
        strncpy(tab->scroll_view.title, title, sizeof(tab->scroll_view.title) - 1);
        tab->scroll_view.title[sizeof(tab->scroll_view.title) - 1] = '\0';
    }
}

/* Exit scroll mode */
void exit_scroll_mode(Tab *tab) {
    if (!tab) return;
    tab->scroll_view.is_active = 0;
    init_scroll_view(&tab->scroll_view);
}

/* Add a line to scrollable content */
void add_line_to_scroll(ScrollView *scroll, const char *line) {
    if (!scroll || !line || scroll->total_lines >= MAX_SCROLL_LINES) return;
    strncpy(scroll->content[scroll->total_lines], line, MAX_CMD_LEN - 1);
    scroll->content[scroll->total_lines][MAX_CMD_LEN - 1] = '\0';
    scroll->total_lines++;
}

/* Scroll movement functions */
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

/* Draw scrollable content view */
void draw_scroll_view(Terminal *term, ScrollView *scroll) {
    if (!term || !scroll || !scroll->is_active) return;
    
    int y0 = 50, y1 = WINDOW_HEIGHT - 80, lh = 16;
    int max_lines = (y1 - y0) / lh;
    scroll->visible_lines = max_lines;
    
    // Clear the area
    XClearArea(term->x11_display, term->x11_window, 0, y0, WINDOW_WIDTH, y1 - y0, False);
    
    // Draw title bar
    XSetForeground(term->x11_display, term->graphics_context, 0x0066CC);
    char title_bar[512];
    snprintf(title_bar, sizeof(title_bar), "%s (Lines %d-%d of %d) - Use PageUp/PageDown/Arrows to scroll, ESC to exit", 
        scroll->title, 
        scroll->scroll_offset + 1, 
        scroll->scroll_offset + max_lines > scroll->total_lines ? scroll->total_lines : scroll->scroll_offset + max_lines,
        scroll->total_lines);
    XDrawString(term->x11_display, term->x11_window, term->graphics_context,
        10, y0 - 10, title_bar, strlen(title_bar));
    
    // Reset color to black
    XSetForeground(term->x11_display, term->graphics_context,
        BlackPixel(term->x11_display, DefaultScreen(term->x11_display)));
    
    // Draw separator line
    XDrawLine(term->x11_display, term->x11_window, term->graphics_context,
        10, y0 - 5, WINDOW_WIDTH - 10, y0 - 5);
    
    // Draw visible lines
    int y = y0;
    for (int i = scroll->scroll_offset; i < scroll->total_lines && i < scroll->scroll_offset + max_lines; i++) {
        if (y >= y1) break;
        int len = strlen(scroll->content[i]);
        if (len > 0 && len < 200) {
            XDrawString(term->x11_display, term->x11_window, term->graphics_context,
                10, y, scroll->content[i], len);
        }
        y += lh;
    }
    
    // Draw scroll indicator on the right
    if (scroll->total_lines > max_lines) {
        int bar_height = y1 - y0;
        int bar_x = WINDOW_WIDTH - 20;
        
        // Draw scroll bar background
        XDrawRectangle(term->x11_display, term->x11_window, term->graphics_context,
            bar_x, y0, 10, bar_height);
        
        // Calculate scroll thumb position and size
        int thumb_height = (bar_height * max_lines) / scroll->total_lines;
        if (thumb_height < 10) thumb_height = 10;
        
        int thumb_pos = (bar_height * scroll->scroll_offset) / scroll->total_lines;
        if (thumb_pos + thumb_height > bar_height) {
            thumb_pos = bar_height - thumb_height;
        }
        
        // Draw scroll thumb
        XFillRectangle(term->x11_display, term->x11_window, term->graphics_context,
            bar_x + 1, y0 + thumb_pos, 8, thumb_height);
    }
}

/* Draw all visible UI: tabs, buffer (auto-scroll), prompt, input, cursor or scroll view */
void draw_terminal_window(Terminal *term) {
    XClearWindow(term->x11_display, term->x11_window);
    
    /* Tabs */
    int tab_width = WINDOW_WIDTH / MAX_TABS;
    for (int i = 0; i < MAX_TABS; i++) {
        int x = i * tab_width;
        char label[32];
        snprintf(label, sizeof(label), "Tab %d", i + 1);
        if (i < term->total_tabs && term->tabs[i].is_active) {
            if (i == term->current_tab_index) {
                XFillRectangle(term->x11_display, term->x11_window, term->graphics_context,
                    x + 1, 1, tab_width - 2, 24);
                XSetForeground(term->x11_display, term->graphics_context,
                    WhitePixel(term->x11_display, DefaultScreen(term->x11_display)));
                XDrawString(term->x11_display, term->x11_window, term->graphics_context,
                    x + 10, 16, label, strlen(label));
                XSetForeground(term->x11_display, term->graphics_context,
                    BlackPixel(term->x11_display, DefaultScreen(term->x11_display)));
            } else {
                XDrawRectangle(term->x11_display, term->x11_window, term->graphics_context,
                    x, 0, tab_width, 25);
                XDrawString(term->x11_display, term->x11_window, term->graphics_context,
                    x + 10, 16, label, strlen(label));
            }
        }
    }
    
    if (term->current_tab_index < term->total_tabs &&
        term->tabs[term->current_tab_index].is_active) {
        Tab *cur = &term->tabs[term->current_tab_index];
        
        // Check if we're in scroll mode
        if (cur->scroll_view.is_active) {
            draw_scroll_view(term, &cur->scroll_view);
        } else {
            /* Buffer with auto-scrolling */
            int y0 = 50, y1 = WINDOW_HEIGHT - 80, lh = 16;
            int max_lines = (y1 - y0) / lh;
            
            char *copy = malloc(strlen(cur->display_buffer) + 1);
            if (copy) {
                strcpy(copy, cur->display_buffer);
                char *lines[1000];
                int n = 0;
                char *p = copy;
                while (p && *p && n < 1000) {
                    char *nl = strchr(p, '\n');
                    if (nl) {
                        *nl = '\0';
                        lines[n++] = p;
                        p = nl + 1;
                    } else {
                        if (*p) lines[n++] = p;
                        break;
                    }
                }
                
                int start = (n > max_lines) ? (n - max_lines) : 0;
                int y = y0;
                for (int i = start; i < n && y < y1; i++) {
                    int len = strlen(lines[i]);
                    if (len > 0 && len < 200) {
                        XDrawString(term->x11_display, term->x11_window, term->graphics_context,
                            10, y, lines[i], len);
                    }
                    y += lh;
                }
                free(copy);
            }
            
            /* Prompt + input - FIXED for search mode */
            int input_y = WINDOW_HEIGHT - 30;
            XSetForeground(term->x11_display, term->graphics_context, 0x0066CC);
            
            // Determine prompt based on current mode
            char *prompt;
            int prompt_len;
            if (term->search_mode) {
                prompt = "Search: ";
                prompt_len = strlen("Search: ");
            } else {
                prompt = "anshgpt@xterm> ";
                prompt_len = strlen("anshgpt@xterm> ");
            }
            
            // Draw the prompt
            XDrawString(term->x11_display, term->x11_window, term->graphics_context,
                10, input_y, prompt, prompt_len);
            
            // Reset color to black for user input
            XSetForeground(term->x11_display, term->graphics_context,
                BlackPixel(term->x11_display, DefaultScreen(term->x11_display)));
            
            // Draw user input (after the prompt)
            if (strlen(cur->current_command) > prompt_len) {
                char *user_input = cur->current_command + prompt_len;
                XDrawString(term->x11_display, term->x11_window, term->graphics_context,
                    10 + (prompt_len * 6), input_y, user_input, strlen(user_input));
            }
            
            /* Cursor - positioned correctly based on current mode */
            int cx = 10 + (cur->cursor_position * 6);
            XDrawLine(term->x11_display, term->x11_window, term->graphics_context,
                cx, input_y - 12, cx, input_y + 2);
        }
    }
    XFlush(term->x11_display);
}

/* Process keyboard input in scroll mode */
void process_scroll_input(Terminal *term, XKeyEvent *event) {
    Tab *cur = &term->tabs[term->current_tab_index];
    ScrollView *scroll = &cur->scroll_view;
    KeySym ks = XLookupKeysym(event, 0);
    
    switch (ks) {
        case XK_Escape:
            exit_scroll_mode(cur);
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

/* Keyboard handling: editing, history nav, search mode, tab mgmt, signals, autocomplete, scrolling */
void process_keyboard_input(Terminal *term, XKeyEvent *event) {
    if (term->current_tab_index >= term->total_tabs ||
        !term->tabs[term->current_tab_index].is_active) {
        return;
    }
    
    Tab *cur = &term->tabs[term->current_tab_index];
    
    // If in scroll mode, handle scroll input
    if (cur->scroll_view.is_active) {
        process_scroll_input(term, event);
        return;
    }
    
    KeySym ks = XLookupKeysym(event, 0);
    
    /* Control combos */
    if (event->state & ControlMask) {
        switch (ks) {
            case XK_c:
                /* Break multiWatch first */
                if (multiwatch_active) {
                    for (int i = 0; i < multiwatch_count; i++) {
                        if (multiwatch_pipes[i] >= 0) {
                            char buf[512];
                            ssize_t n;
                            while ((n = read(multiwatch_pipes[i], buf, sizeof(buf) - 1)) > 0) {
                                buf[n] = '\0';
                                add_text_to_buffer(cur, buf);
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
                    add_text_to_buffer(cur, "\nmultiWatch interrupted by Ctrl+C\n");
                    strcpy(cur->current_command, "anshgpt@xterm> ");
                    cur->cursor_position = strlen(cur->current_command);
                    return;
                }
                if (cur->shell_process_id > 0) kill(cur->shell_process_id, SIGINT);
                return;
                
            case XK_z:
                if (cur->shell_process_id > 0) {
                    kill(cur->shell_process_id, SIGTSTP);
                    add_text_to_buffer(cur, "\n[Process suspended - use 'fg' to resume]\n");
                }
                return;
                
            case XK_a:
                if (term->search_mode) {
                    cur->cursor_position = strlen("Search: ");
                } else {
                    cur->cursor_position = strlen("anshgpt@xterm> ");
                }
                return;
                
            case XK_e:
                cur->cursor_position = strlen(cur->current_command);
                return;
                
            case XK_r:
                term->search_mode = 1;
                strcpy(cur->current_command, "Search: ");
                cur->cursor_position = strlen(cur->current_command);
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
    
    /* Special keys */
    switch (ks) {
        case XK_Return:
        case XK_KP_Enter: {
            if (term->search_mode) {
                builtin_search_history(term);
                term->search_mode = 0;
            } else {
                int pl = strlen("anshgpt@xterm> ");
                if (strlen(cur->current_command) > pl) {
                    char cmd[MAX_CMD_LEN];
                    strcpy(cmd, cur->current_command + pl);
                    execute_user_command(term, cmd);
                }
                strcpy(cur->current_command, "anshgpt@xterm> ");
                cur->cursor_position = strlen(cur->current_command);
            }
            break;
        }
        
        case XK_BackSpace: {
            int pl;
            if (term->search_mode) {
                pl = strlen("Search: ");
            } else {
                pl = strlen("anshgpt@xterm> ");
            }
            
            if (cur->cursor_position > pl) {
                int L = strlen(cur->current_command);
                for (int i = cur->cursor_position - 1; i < L; i++) {
                    cur->current_command[i] = cur->current_command[i + 1];
                }
                cur->cursor_position--;
                if (term->search_mode) {
                    int sl = strlen(term->search_term);
                    if (sl > 0) term->search_term[sl - 1] = '\0';
                }
            }
            break;
        }
        
        case XK_Left: {
            int pl;
            if (term->search_mode) {
                pl = strlen("Search: ");
            } else {
                pl = strlen("anshgpt@xterm> ");
            }
            
            if (cur->cursor_position > pl) cur->cursor_position--;
            break;
        }
        
        case XK_Right:
            if (cur->cursor_position < strlen(cur->current_command)) cur->cursor_position++;
            break;
            
        case XK_Up:
            if (!term->search_mode && term->history_count > 0) {
                if (term->history_position > 0) term->history_position--;
                snprintf(cur->current_command, MAX_CMD_LEN,
                    "anshgpt@xterm> %s",
                    term->command_history[term->history_position]);
                cur->cursor_position = strlen(cur->current_command);
            }
            break;
            
        case XK_Down:
            if (!term->search_mode && term->history_count > 0) {
                if (term->history_position < term->history_count - 1) {
                    term->history_position++;
                    snprintf(cur->current_command, MAX_CMD_LEN,
                        "anshgpt@xterm> %s",
                        term->command_history[term->history_position]);
                } else {
                    strcpy(cur->current_command, "anshgpt@xterm> ");
                }
                cur->cursor_position = strlen(cur->current_command);
            }
            break;
            
        case XK_Tab:
            if (!term->search_mode) {
                int pl = strlen("anshgpt@xterm> ");
                if (strlen(cur->current_command) > pl) {
                    char *part = cur->current_command + pl;
                    autocomplete_filename(term, part);
                    cur->cursor_position = strlen(cur->current_command);
                }
            }
            break;
            
        case XK_Escape:
            if (term->search_mode) {
                term->search_mode = 0;
                strcpy(cur->current_command, "anshgpt@xterm> ");
                cur->cursor_position = strlen(cur->current_command);
            }
            break;
            
        default: {
            char buf[10];
            int n = XLookupString(event, buf, sizeof(buf) - 1, NULL, NULL);
            if (n == 1 && buf[0] >= 32 && buf[0] <= 126) {
                int L = strlen(cur->current_command);
                if (L < MAX_CMD_LEN - 2) {
                    for (int i = L; i >= cur->cursor_position; i--) {
                        cur->current_command[i + 1] = cur->current_command[i];
                    }
                    cur->current_command[cur->cursor_position] = buf[0];
                    cur->cursor_position++;
                    if (term->search_mode) {
                        int sl = strlen(term->search_term);
                        if (sl < MAX_CMD_LEN - 1) {
                            term->search_term[sl] = buf[0];
                            term->search_term[sl + 1] = '\0';
                        }
                    }
                }
            }
            break;
        }
    }
}

/* Event loop: handle X events, read shell pipes via select, and poll multiWatch */
void handle_x11_events(Terminal *term) {
    XEvent ev;
    fd_set rfds;
    int xfd = ConnectionNumber(term->x11_display);
    struct timeval tv;
    
    while (1) {
        /* Drain all X events */
        while (XPending(term->x11_display)) {
            XNextEvent(term->x11_display, &ev);
            switch (ev.type) {
                case Expose:
                    draw_terminal_window(term);
                    break;
                    
                case KeyPress:
                    process_keyboard_input(term, &ev.xkey);
                    draw_terminal_window(term);
                    break;
                    
                case ButtonPress:
                    if (ev.xbutton.y < 25) {
                        int tab_w = WINDOW_WIDTH / MAX_TABS;
                        int clicked = ev.xbutton.x / tab_w;
                        if (clicked < term->total_tabs && clicked >= 0 &&
                            term->tabs[clicked].is_active) {
                            switch_to_tab(term, clicked);
                        }
                    }
                    break;
            }
        }
        
        /* Shell outputs via select */
        FD_ZERO(&rfds);
        FD_SET(xfd, &rfds);
        int maxfd = xfd;
        
        for (int i = 0; i < term->total_tabs; i++) {
            if (term->tabs[i].is_active && term->tabs[i].output_pipe[0] >= 0) {
                FD_SET(term->tabs[i].output_pipe[0], &rfds);
                if (term->tabs[i].output_pipe[0] > maxfd) maxfd = term->tabs[i].output_pipe[0];
            }
        }
        
        tv.tv_sec = 0;
        tv.tv_usec = 20000;
        int rc = select(maxfd + 1, &rfds, NULL, NULL, &tv);
        
        if (rc > 0) {
            for (int i = 0; i < term->total_tabs; i++) {
                if (term->tabs[i].is_active &&
                    term->tabs[i].output_pipe[0] >= 0 &&
                    FD_ISSET(term->tabs[i].output_pipe[0], &rfds)) {
                    char obuf[256];
                    ssize_t n = read(term->tabs[i].output_pipe[0], obuf, sizeof(obuf) - 1);
                    if (n > 0) {
                        obuf[n] = '\0';
                        add_text_to_buffer(&term->tabs[i], obuf);
                        if (i == term->current_tab_index) draw_terminal_window(term);
                    } else if (n == 0) {
                        close(term->tabs[i].output_pipe[0]);
                        term->tabs[i].output_pipe[0] = -1;
                    }
                }
            }
        }
        
        /* Always poll multiWatch and refresh when active */
        check_multiwatch_output(term);
        if (multiwatch_active) draw_terminal_window(term);
    }
}

/* Initialize X11 window, GC, and input masks */
void setup_x11_gui(Terminal *term) {
    term->x11_display = XOpenDisplay(NULL);
    if (!term->x11_display) {
        printf("Error: Cannot open X11 display. Make sure XQuartz is running.\n");
        exit(1);
    }
    
    int scr = DefaultScreen(term->x11_display);
    term->x11_window = XCreateSimpleWindow(
        term->x11_display,
        RootWindow(term->x11_display, scr),
        100, 100, WINDOW_WIDTH, WINDOW_HEIGHT, 2,
        BlackPixel(term->x11_display, scr),
        WhitePixel(term->x11_display, scr)
    );
    
    XStoreName(term->x11_display, term->x11_window, "Xterm - Project by Ansh Gupta");
    term->graphics_context = XCreateGC(term->x11_display, term->x11_window, 0, NULL);
    XSetForeground(term->x11_display, term->graphics_context,
        BlackPixel(term->x11_display, scr));
    
    XSelectInput(term->x11_display, term->x11_window,
        ExposureMask | KeyPressMask | ButtonPressMask);
    XMapWindow(term->x11_display, term->x11_window);
    XFlush(term->x11_display);
    
    printf("X11 GUI initialized successfully\n");
}