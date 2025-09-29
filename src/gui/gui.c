#include "gui.h"
#include "../shell/shell.h"
#include "../features/features.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include<stdio.h>

int gui_init(GUI* gui) {
    memset(gui, 0, sizeof(GUI));
    
    gui->display = XOpenDisplay(NULL);
    if (!gui->display) {
        fprintf(stderr, "Cannot open X display\n");
        return -1;
    }
    
    gui->screen = DefaultScreen(gui->display);
    gui->window_width = 1000;
    gui->window_height = 700;
    
    gui->window = XCreateSimpleWindow(
        gui->display,
        RootWindow(gui->display, gui->screen),
        10, 10, gui->window_width, gui->window_height, 1,
        BlackPixel(gui->display, gui->screen),
        WhitePixel(gui->display, gui->screen)
    );
    
    XStoreName(gui->display, gui->window, "XTerm Project - Advanced Terminal");
    
    gui->font = XLoadQueryFont(gui->display, "fixed");
    if (!gui->font) {
        gui->font = XLoadQueryFont(gui->display, "*");
    }
    
    gui->gc = XCreateGC(gui->display, gui->window, 0, NULL);
    XSetForeground(gui->display, gui->gc, BlackPixel(gui->display, gui->screen));
    if (gui->font) {
        XSetFont(gui->display, gui->gc, gui->font->fid);
    }
    
    XSelectInput(gui->display, gui->window, 
                 ExposureMask | KeyPressMask | StructureNotifyMask | ButtonPressMask);
    XMapWindow(gui->display, gui->window);
    
    gui->running = 1;
    pthread_mutex_init(&gui->mutex, NULL);
    
    return 0;
}

void gui_cleanup(GUI* gui) {
    if (!gui) return;
    
    pthread_mutex_destroy(&gui->mutex);
    
    // Clean up tabs
    Tab* tab = gui->tabs;
    while (tab) {
        Tab* next = tab->next;
        tab_destroy(tab);
        tab = next;
    }
    
    if (gui->gc) XFreeGC(gui->display, gui->gc);
    if (gui->font) XFreeFont(gui->display, gui->font);
    if (gui->window) XDestroyWindow(gui->display, gui->window);
    if (gui->display) XCloseDisplay(gui->display);
}

void gui_run(GUI* gui) {
    XEvent event;
    struct pollfd fds[MAX_TABS + 1];
    
    while (gui->running) {
        // Set up polling
        int nfds = 0;
        fds[nfds].fd = ConnectionNumber(gui->display);
        fds[nfds].events = POLLIN;
        nfds++;
        
        // Add active tab shell descriptors
        Tab* tab = gui->tabs;
        while (tab && nfds < MAX_TABS + 1) {
            if (tab->shell_out_fd > 0) {
                fds[nfds].fd = tab->shell_out_fd;
                fds[nfds].events = POLLIN;
                nfds++;
            }
            tab = tab->next;
        }
        
        int ready = poll(fds, nfds, 50);
        if (ready < 0 && errno != EINTR) break;
        
        // Handle X11 events
        while (XPending(gui->display)) {
            XNextEvent(gui->display, &event);
            switch (event.type) {
                case Expose:
                    gui_draw_text(gui);
                    gui_draw_input(gui);
                    gui_draw_tabs(gui);
                    break;
                case KeyPress:
                    gui_handle_key_press(gui, &event.xkey);
                    break;
                case ConfigureNotify:
                    gui->window_width = event.xconfigure.width;
                    gui->window_height = event.xconfigure.height;
                    break;
                case DestroyNotify:
                    gui->running = 0;
                    break;
                case ButtonPress:
                    // Handle tab clicks
                    if (event.xbutton.y < TAB_HEIGHT) {
                        int tab_width = gui->window_width / gui->tab_count;
                        int tab_id = event.xbutton.x / tab_width;
                        tab_switch(gui, tab_id);
                    }
                    break;
            }
        }
        
        // Handle shell output
        tab = gui->tabs;
        int fd_index = 1;
        while (tab && fd_index < nfds) {
            if (tab->shell_out_fd > 0 && fds[fd_index].revents & POLLIN) {
                char buffer[4096];
                ssize_t n = read(tab->shell_out_fd, buffer, sizeof(buffer) - 1);
                if (n > 0) {
                    buffer[n] = '\0';
                    if (tab == gui->current_tab) {
                        gui_append_text(gui, buffer);
                    } else {
                        // Buffer for inactive tab
                        size_t current_len = tab->text_buffer.length;
                        size_t space = MAX_BUFFER - current_len - 1;
                        if (n < space) {
                            memcpy(tab->text_buffer.buffer + current_len, buffer, n);
                            tab->text_buffer.length += n;
                            tab->text_buffer.buffer[tab->text_buffer.length] = '\0';
                        }
                    }
                }
            }
            tab = tab->next;
            fd_index++;
        }
    }
}

void gui_draw_text(GUI* gui) {
    if (!gui->current_tab) return;
    
    pthread_mutex_lock(&gui->mutex);
    
    int y_start = TAB_HEIGHT + 20;
    int y = y_start;
    int x = 10;
    
    XClearArea(gui->display, gui->window, 0, TAB_HEIGHT, 
               gui->window_width, gui->window_height - TAB_HEIGHT - 30, False);
    
    TextBuffer* buf = &gui->current_tab->text_buffer;
    char* text = buf->buffer;
    char* line_start = text;
    
    for (char* p = text; *p && y < gui->window_height - 50; p++) {
        if (*p == '\n' || p - line_start > 100) {
            int len = p - line_start;
            if (len > 0) {
                XDrawString(gui->display, gui->window, gui->gc, x, y, line_start, len);
            }
            y += LINE_HEIGHT;
            line_start = p + 1;
        }
    }
    
    // Draw remaining text
    if (line_start < text + buf->length) {
        int len = (text + buf->length) - line_start;
        XDrawString(gui->display, gui->window, gui->gc, x, y, line_start, len);
    }
    
    pthread_mutex_unlock(&gui->mutex);
}

void gui_draw_input(GUI* gui) {
    if (!gui->current_tab) return;
    
    int y = gui->window_height - 20;
    int x = 10;
    
    XClearArea(gui->display, gui->window, 0, gui->window_height - 30,
               gui->window_width, 30, False);
    
    // Draw prompt
    XDrawString(gui->display, gui->window, gui->gc, x, y, "$ ", 2);
    x += 20;
    
    // Draw input
    InputLine* input = &gui->current_tab->input_line;
    if (input->input_len > 0) {
        XDrawString(gui->display, gui->window, gui->gc, x, y, 
                   input->input, input->input_len);
    }
    
    // Draw cursor
    int cursor_x = x;
    if (gui->font) {
        cursor_x += XTextWidth(gui->font, input->input, input->cursor_pos);
    } else {
        cursor_x += input->cursor_pos * 8;
    }
    XDrawLine(gui->display, gui->window, gui->gc, cursor_x, y + 3, cursor_x, y - 12);
}

void gui_draw_tabs(GUI* gui) {
    XClearArea(gui->display, gui->window, 0, 0, gui->window_width, TAB_HEIGHT, False);
    
    if (gui->tab_count == 0) return;
    
    int tab_width = gui->window_width / gui->tab_count;
    int x = 0;
    
    Tab* tab = gui->tabs;
    while (tab) {
        // Draw tab background
        if (tab == gui->current_tab) {
            XSetForeground(gui->display, gui->gc, 
                          WhitePixel(gui->display, gui->screen) * 0.8);
            XFillRectangle(gui->display, gui->window, gui->gc, x, 0, tab_width, TAB_HEIGHT);
            XSetForeground(gui->display, gui->gc, BlackPixel(gui->display, gui->screen));
        }
        
        // Draw tab border
        XDrawRectangle(gui->display, gui->window, gui->gc, x, 0, tab_width, TAB_HEIGHT);
        
        // Draw tab title
        int title_x = x + 10;
        int title_y = TAB_HEIGHT / 2 + 5;
        XDrawString(gui->display, gui->window, gui->gc, title_x, title_y, 
                   tab->title, strlen(tab->title));
        
        x += tab_width;
        tab = tab->next;
    }
}

void gui_append_text(GUI* gui, const char* text) {
    if (!gui->current_tab || !text) return;
    
    pthread_mutex_lock(&gui->mutex);
    
    TextBuffer* buf = &gui->current_tab->text_buffer;
    size_t len = strlen(text);
    size_t space = MAX_BUFFER - buf->length - 1;
    
    if (len < space) {
        memcpy(buf->buffer + buf->length, text, len);
        buf->length += len;
        buf->buffer[buf->length] = '\0';
    } else {
        // Buffer overflow, shift content
        size_t shift = len;
        if (shift > buf->length / 2) shift = buf->length / 2;
        memmove(buf->buffer, buf->buffer + shift, buf->length - shift);
        buf->length -= shift;
        memcpy(buf->buffer + buf->length, text, len);
        buf->length += len;
        buf->buffer[buf->length] = '\0';
    }
    
    pthread_mutex_unlock(&gui->mutex);
    
    // Trigger redraw
    XExposeEvent expose = {0};
    expose.type = Expose;
    expose.window = gui->window;
    XSendEvent(gui->display, gui->window, False, ExposureMask, (XEvent*)&expose);
    XFlush(gui->display);
}

void gui_clear_screen(GUI* gui) {
    if (!gui->current_tab) return;
    
    pthread_mutex_lock(&gui->mutex);
    gui->current_tab->text_buffer.length = 0;
    gui->current_tab->text_buffer.buffer[0] = '\0';
    pthread_mutex_unlock(&gui->mutex);
    
    gui_draw_text(gui);
}

void gui_handle_key_press(GUI* gui, XKeyEvent* event) {
    if (!gui->current_tab) return;
    
    KeySym keysym;
    char buffer[32];
    int len = XLookupString(event, buffer, sizeof(buffer), &keysym, NULL);
    
    InputLine* input = &gui->current_tab->input_line;
    int ctrl = (event->state & ControlMask) != 0;
    
    // Handle special keys
    if (ctrl) {
        switch (keysym) {
            case XK_a:
            case XK_A:
                input->cursor_pos = 0;
                gui_draw_input(gui);
                return;
            case XK_e:
            case XK_E:
                input->cursor_pos = input->input_len;
                gui_draw_input(gui);
                return;
            case XK_c:
            case XK_C:
                if (g_foreground_pid > 0) {
                    kill(-g_foreground_pid, SIGINT);
                }
                return;
            case XK_z:
            case XK_Z:
                if (g_foreground_pid > 0) {
                    kill(-g_foreground_pid, SIGTSTP);
                }
                return;
            case XK_r:
            case XK_R:
                // History search - simplified implementation
                gui_append_text(gui, "\n(reverse-i-search): ");
                gui_draw_input(gui);
                return;
        }
    }
    
    switch (keysym) {
        case XK_Return:
        case XK_KP_Enter: {
            // Execute command
            char* command = malloc(input->input_len + 1);
            memcpy(command, input->input, input->input_len);
            command[input->input_len] = '\0';
            
            gui_append_text(gui, "\n$ ");
            gui_append_text(gui, command);
            gui_append_text(gui, "\n");
            
            if (strlen(command) > 0) {
                history_add(command);
                
                if (gui->current_tab->shell_in_fd > 0) {
                    write(gui->current_tab->shell_in_fd, command, strlen(command));
                    write(gui->current_tab->shell_in_fd, "\n", 1);
                }
            }
            
            // Clear input
            input->input_len = 0;
            input->cursor_pos = 0;
            input->input[0] = '\0';
            
            free(command);
            gui_draw_input(gui);
            break;
        }
        case XK_BackSpace:
            if (input->cursor_pos > 0) {
                memmove(input->input + input->cursor_pos - 1,
                       input->input + input->cursor_pos,
                       input->input_len - input->cursor_pos);
                input->cursor_pos--;
                input->input_len--;
                input->input[input->input_len] = '\0';
                gui_draw_input(gui);
            }
            break;
        case XK_Left:
            if (input->cursor_pos > 0) {
                input->cursor_pos--;
                gui_draw_input(gui);
            }
            break;
        case XK_Right:
            if (input->cursor_pos < input->input_len) {
                input->cursor_pos++;
                gui_draw_input(gui);
            }
            break;
        case XK_Tab:
            handle_tab_completion(gui);
            break;
        default:
            if (len > 0 && buffer[0] >= 32 && buffer[0] < 127) {
                if (input->input_len + len < MAX_LINE - 1) {
                    memmove(input->input + input->cursor_pos + len,
                           input->input + input->cursor_pos,
                           input->input_len - input->cursor_pos);
                    memcpy(input->input + input->cursor_pos, buffer, len);
                    input->cursor_pos += len;
                    input->input_len += len;
                    input->input[input->input_len] = '\0';
                    gui_draw_input(gui);
                }
            }
            break;
    }
}
