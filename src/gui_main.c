// gui_main.c
// Minimal X11 terminal-like editor: prompt, input editing, caret aligned via font metrics.
// Build: cc gui_main.c -o myterm -lX11

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 4096
#define LINE_H   20

// Draw the current prompt and input with a caret positioned using actual font metrics
static void draw_line(Display* dpy, Window win, GC gc,
                      XFontStruct* font,
                      const char* prompt, int x0, int y,
                      const char* input, int len, int cursor) {
    XClearWindow(dpy, win);

    int plen = (int)strlen(prompt);
    XDrawString(dpy, win, gc, x0, y, prompt, plen);

    // Pixel width of prompt and input prefix using loaded font
    int px_prompt = XTextWidth(font, prompt, plen);
    int x_input   = x0 + px_prompt;

    XDrawString(dpy, win, gc, x_input, y, input, len);

    int px_cursor = (cursor > 0) ? XTextWidth(font, input, cursor) : 0;
    int cx = x_input + px_cursor;

    // Vertical caret
    XDrawLine(dpy, win, gc, cx, y + 3, cx, y - font->ascent);
}

// Advance one logical line; simple scroll back to top when reaching window bottom
static void advance_line(int* y, int* line_no, int height,
                         Display* dpy, Window win) {
    (*line_no)++;
    *y += LINE_H;
    if (*y > height - 10) {
        *y = 20;
        *line_no = 1;
        XClearWindow(dpy, win);
    }
}

int main(void) {
    setlocale(LC_ALL, "");

    Display* display = XOpenDisplay(NULL);
    if (!display) { fprintf(stderr, "Cannot open display\n"); return 1; }

    int screen = DefaultScreen(display);
    Window win = XCreateSimpleWindow(display, RootWindow(display, screen),
                                     10, 10, 800, 600, 1,
                                     BlackPixel(display, screen),
                                     WhitePixel(display, screen));
    XStoreName(display, win, "MyTerm - Prompt Input");
    XSelectInput(display, win, StructureNotifyMask | ExposureMask | KeyPressMask);
    XMapWindow(display, win);

    XFontStruct* font = XLoadQueryFont(display, "fixed");
    if (!font) { fprintf(stderr, "Cannot load font 'fixed'\n"); return 1; }

    GC gc = XCreateGC(display, win, 0, NULL);
    XSetForeground(display, gc, BlackPixel(display, screen));
    XSetFont(display, gc, font->fid);

    const char* prompt = "user@myterm> ";
    int x0 = 10;
    int y = 20;
    int height = 600; // updated on ConfigureNotify
    int line_no = 1;

    char input[MAX_LINE] = {0};
    int len = 0, cursor = 0;

    // Initial draw
    draw_line(display, win, gc, font, prompt, x0, y, input, len, cursor);

    XEvent ev;
    for (;;) {
        XNextEvent(display, &ev);
        if (ev.type == DestroyNotify) break;

        if (ev.type == ConfigureNotify) {
            height = ev.xconfigure.height;
        } else if (ev.type == Expose) {
            draw_line(display, win, gc, font, prompt, x0, y, input, len, cursor);
        } else if (ev.type == KeyPress) {
            char buf[64];
            KeySym ks;
            int n = XLookupString(&ev.xkey, buf, sizeof(buf), &ks, NULL);

            int ctrl = (ev.xkey.state & ControlMask) != 0;
            if (ctrl && (ks == XK_a || ks == XK_A)) {   // Ctrl+A -> Home
                cursor = 0;
                draw_line(display, win, gc, font, prompt, x0, y, input, len, cursor);
                continue;
            }
            if (ctrl && (ks == XK_e || ks == XK_E)) {   // Ctrl+E -> End
                cursor = len;
                draw_line(display, win, gc, font, prompt, x0, y, input, len, cursor);
                continue;
            }

            if (ks == XK_Return || ks == XK_KP_Enter) {
                advance_line(&y, &line_no, height, display, win);
                len = 0; cursor = 0; input[0] = 0;
                draw_line(display, win, gc, font, prompt, x0, y, input, len, cursor);
                continue;
            }

            if (ks == XK_BackSpace) {
                if (cursor > 0) {
                    memmove(input + cursor - 1, input + cursor, (size_t)(len - cursor));
                    len--; cursor--;
                    input[len] = 0;
                }
                draw_line(display, win, gc, font, prompt, x0, y, input, len, cursor);
                continue;
            }

            // Insert printable text
            if (n > 0) {
                if (len + n < MAX_LINE) {
                    memmove(input + cursor + n, input + cursor, (size_t)(len - cursor));
                    memcpy(input + cursor, buf, (size_t)n);
                    len += n; cursor += n;
                    input[len] = 0;
                }
                draw_line(display, win, gc, font, prompt, x0, y, input, len, cursor);
            }
        }
    }

    XFreeGC(display, gc);
    XFreeFont(display, font);
    XCloseDisplay(display);
    return 0;
}