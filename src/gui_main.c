#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <locale.h>
#include <stdio.h>
#include <X11/Xutil.h>
#include <string.h>

#define MAXBUF 8192

static char buf[MAXBUF];
static size_t len = 0;

static void redraw(Display* dpy, Window win, GC gc) {
    XClearWindow(dpy, win);
    int x = 10, y = 30, line_h = 18;
    int start = 0;
    for (int i = 0; i <= (int)len; ++i) {
        if (i == (int)len || buf[i] == '\n') {
            int seglen = i - start;
            if (seglen > 0) XDrawString(dpy, win, gc, x, y, buf + start, seglen);
            y += line_h;
            start = i + 1;
        }
    }
}

int main(void) {
    setlocale(LC_ALL, "");

    Display* dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "Cannot open display\n");
        return 1;
    }

    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);

    unsigned long black = BlackPixel(dpy, screen);
    unsigned long white = WhitePixel(dpy, screen);

    Window win = XCreateSimpleWindow(dpy, root, 100, 100, 800, 400, 1, black, white);
    XStoreName(dpy, win, "MyTerm - Phase 1 (C)");
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | StructureNotifyMask);
    XMapWindow(dpy, win);

    GC gc = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc, black);

    XEvent ev;
    while (1) {
        XNextEvent(dpy, &ev);

        if (ev.type == Expose) {
            redraw(dpy, win, gc);
        } 
        else if (ev.type == KeyPress) {
            KeySym ks;
            char ch[8];
            int n = XLookupString(&ev.xkey, ch, sizeof(ch), &ks, NULL);

            if (ks == XK_Escape) break;

            if (ks == XK_BackSpace) {
                if (len > 0) { len--; buf[len] = '\0'; }
                redraw(dpy, win, gc);
                continue;
            }

            // Normalize Enter to newline and skip raw bytes for this key
            if (ks == XK_Return || ks == XK_KP_Enter) {
                if (len < MAXBUF - 1) {
                    buf[len++] = '\n';
                    buf[len] = '\0';
                }
                redraw(dpy, win, gc);
                continue;
            }

            if (n > 0) {
                for (int i = 0; i < n && len < MAXBUF - 1; ++i) {
                    char c = ch[i];
                    if (c == '\r') c = '\n';
                    buf[len++] = c;
                }
                buf[len] = '\0';
                redraw(dpy, win, gc);
            }
        } 
        else if (ev.type == DestroyNotify) {
            break;
        }
    }

    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}