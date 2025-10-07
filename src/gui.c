#include "xterm.h"

/* Draw all visible UI: tabs, buffer (auto-scroll), prompt, input, cursor */
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

    /* Buffer with auto-scrolling */
    if (term->current_tab_index < term->total_tabs &&
        term->tabs[term->current_tab_index].is_active) {

        Tab *cur = &term->tabs[term->current_tab_index];
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

        /* Prompt + input */
        int input_y = WINDOW_HEIGHT - 30;
        XSetForeground(term->x11_display, term->graphics_context, 0x0066CC);
        char prompt[] = "user@myterm> ";
        XDrawString(term->x11_display, term->x11_window, term->graphics_context,
                    10, input_y, prompt, strlen(prompt));

        XSetForeground(term->x11_display, term->graphics_context,
                       BlackPixel(term->x11_display, DefaultScreen(term->x11_display)));

        int pfx = strlen(prompt);
        if (strlen(cur->current_command) > pfx) {
            char *txt = cur->current_command + pfx;
            XDrawString(term->x11_display, term->x11_window, term->graphics_context,
                        10 + (pfx * 6), input_y, txt, strlen(txt));
        }

        /* Cursor */
        int cx = 10 + (cur->cursor_position * 6);
        XDrawLine(term->x11_display, term->x11_window, term->graphics_context,
                  cx, input_y - 12, cx, input_y + 2);
    }

    XFlush(term->x11_display);
}

/* Keyboard handling: editing, history nav, search mode, tab mgmt, signals, autocomplete */
void process_keyboard_input(Terminal *term, XKeyEvent *event) {
    if (term->current_tab_index >= term->total_tabs ||
        !term->tabs[term->current_tab_index].is_active) {
        return;
    }

    Tab *cur = &term->tabs[term->current_tab_index];
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
                    strcpy(cur->current_command, "user@myterm> ");
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
                cur->cursor_position = strlen("user@myterm> ");
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
                int pl = strlen("user@myterm> ");
                if (strlen(cur->current_command) > pl) {
                    char cmd[MAX_CMD_LEN];
                    strcpy(cmd, cur->current_command + pl);
                    execute_user_command(term, cmd);
                }
                strcpy(cur->current_command, "user@myterm> ");
                cur->cursor_position = strlen(cur->current_command);
            }
            break;
        }

        case XK_BackSpace: {
            int pl = term->search_mode ? strlen("Search: ")
                                       : strlen("user@myterm> ");
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
            int pl = term->search_mode ? strlen("Search: ")
                                       : strlen("user@myterm> ");
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
                         "user@myterm> %s",
                         term->command_history[term->history_position]);
                cur->cursor_position = strlen(cur->current_command);
            }
            break;

        case XK_Down:
            if (!term->search_mode && term->history_count > 0) {
                if (term->history_position < term->history_count - 1) {
                    term->history_position++;
                    snprintf(cur->current_command, MAX_CMD_LEN,
                             "user@myterm> %s",
                             term->command_history[term->history_position]);
                } else {
                    strcpy(cur->current_command, "user@myterm> ");
                }
                cur->cursor_position = strlen(cur->current_command);
            }
            break;

        case XK_Tab:
            if (!term->search_mode) {
                int pl = strlen("user@myterm> ");
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
                strcpy(cur->current_command, "user@myterm> ");
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

    XStoreName(term->x11_display, term->x11_window, "MyTerm - CS69201 Project");
    term->graphics_context = XCreateGC(term->x11_display, term->x11_window, 0, NULL);
    XSetForeground(term->x11_display, term->graphics_context,
                   BlackPixel(term->x11_display, scr));
    XSelectInput(term->x11_display, term->x11_window,
                 ExposureMask | KeyPressMask | ButtonPressMask);
    XMapWindow(term->x11_display, term->x11_window);
    XFlush(term->x11_display);

    printf("X11 GUI initialized successfully\n");
}