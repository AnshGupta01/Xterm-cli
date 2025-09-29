#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define MAXBUF 65536
#define LINE_HEIGHT 18

// Display history buffer and current input line
static char disp[MAXBUF];
static size_t dlen = 0;
static size_t prompt_off = 0; // index in disp where the editable input begins (after prompt)

static char inbuf[4096];
static size_t ilen = 0;

static const char *PROMPT = "> ";

// Child process fds
static int fd_to_child = -1;       // parent -> child stdin (write end)
static int fd_from_child_out = -1; // child stdout -> parent (read end)
static int fd_from_child_err = -1; // child stderr -> parent (read end)
static pid_t child_pid = -1;

static void append_disp(const char *s, size_t n)
{
    size_t space = (MAXBUF - 1) - dlen;
    if (n > space)
        n = space;
    if (n > 0)
    {
        memcpy(disp + dlen, s, n);
        dlen += n;
        disp[dlen] = '\0';
    }
}

static void append_cstr(const char *s)
{
    append_disp(s, strlen(s));
}

static void redraw(Display *dpy, Window win, GC gc)
{
    XClearWindow(dpy, win);
    int x = 10, y = 30;
    int start = 0;
    for (int i = 0; i <= (int)dlen; ++i)
    {
        if (i == (int)dlen || disp[i] == '\n')
        {
            int seglen = i - start;
            if (seglen > 0)
                XDrawString(dpy, win, gc, x, y, disp + start, seglen);
            y += LINE_HEIGHT;
            start = i + 1;
        }
    }
}

static void set_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int start_child(void)
{
    int to_child[2], from_out[2], from_err[2];
    if (pipe(to_child) == -1)
    {
        perror("pipe to_child");
        return -1;
    }
    if (pipe(from_out) == -1)
    {
        perror("pipe from_out");
        return -1;
    }
    if (pipe(from_err) == -1)
    {
        perror("pipe from_err");
        return -1;
    }

    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork");
        return -1;
    }
    if (pid == 0)
    {
        if (dup2(to_child[0], STDIN_FILENO) == -1)
        {
            perror("dup2 stdin");
            _exit(127);
        }
        if (dup2(from_out[1], STDOUT_FILENO) == -1)
        {
            perror("dup2 stdout");
            _exit(127);
        }
        if (dup2(from_err[1], STDERR_FILENO) == -1)
        {
            perror("dup2 stderr");
            _exit(127);
        }
        close(to_child[0]);
        close(to_child[1]);
        close(from_out[0]);
        close(from_out[1]);
        close(from_err[0]);
        close(from_err[1]);

        // Prefer zsh on macOS; reads commands from stdin with -s
        execl("/bin/zsh", "zsh", "-s", (char *)NULL);
        perror("exec zsh");
        _exit(127);
    }
    // Parent
    close(to_child[0]);
    close(from_out[1]);
    close(from_err[1]);
    fd_to_child = to_child[1];
    fd_from_child_out = from_out[0];
    fd_from_child_err = from_err[0];
    set_nonblock(fd_from_child_out);
    set_nonblock(fd_from_child_err);
    child_pid = pid;
    return 0;
}

// Start a new prompt: write the prompt, then mark where input begins
static void new_prompt(Display *dpy, Window win, GC gc)
{
    append_cstr(PROMPT); // draw "> "
    prompt_off = dlen;   // editable input starts after the prompt
    redraw(dpy, win, gc);
}

int main(void)
{
    setlocale(LC_ALL, "");

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy)
    {
        fprintf(stderr, "Cannot open display\n");
        return 1;
    }

    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);
    unsigned long black = BlackPixel(dpy, screen);
    unsigned long white = WhitePixel(dpy, screen);

    Window win = XCreateSimpleWindow(dpy, root, 100, 100, 900, 500, 1, black, white);
    XStoreName(dpy, win, "MyTerm");
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | StructureNotifyMask);
    XMapWindow(dpy, win);

    GC gc = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc, black);

    if (start_child() != 0)
    {
        append_cstr("Failed to start child\n");
    }

    // Seed first prompt
    new_prompt(dpy, win, gc);

    struct pollfd pfds[2];

    for (;;)
    {
        // 1) Drain pending X events first
        while (XPending(dpy))
        {
            XEvent ev;
            XNextEvent(dpy, &ev);

            if (ev.type == Expose)
            {
                redraw(dpy, win, gc);
            }
            else if (ev.type == KeyPress)
            {
                KeySym ks;
                char ch[8];
                int n = XLookupString(&ev.xkey, ch, sizeof(ch), &ks, NULL);

                if (ks == XK_Escape)
                    goto quit;

                if (ks == XK_BackSpace)
                {
                    if (ilen > 0)
                        ilen--;
                    // Truncate to start of input (after prompt) and repaint input
                    dlen = prompt_off;
                    disp[dlen] = 0;
                    if (ilen > 0)
                        append_disp(inbuf, ilen);
                    redraw(dpy, win, gc);
                    continue;
                }

                if (ks == XK_Return || ks == XK_KP_Enter)
                {
                    // 1) Finalize current editable line: keep prompt, drop transient edit
                    dlen = prompt_off;
                    disp[dlen] = 0;

                    // 2) Echo exactly once into history
                    append_disp(inbuf, ilen);
                    append_cstr("\n");

                    // 3) Send exactly once to child
                    if (fd_to_child >= 0)
                    {
                        if (ilen > 0)
                            write(fd_to_child, inbuf, ilen);
                        write(fd_to_child, "\n", 1);
                    }

                    // 4) Reset input and start a fresh prompt
                    ilen = 0;
                    inbuf[0] = 0;
                    new_prompt(dpy, win, gc); // writes "> " then sets prompt_off = dlen
                    continue;
                }

                if (n > 0)
                {
                    for (int i = 0; i < n && ilen < sizeof(inbuf) - 1; ++i)
                    {
                        char c = ch[i];
                        if (c == '\r')
                            c = '\n';
                        if (c == '\n')
                            continue; // do not insert raw LF into input
                        inbuf[ilen++] = c;
                    }
                    inbuf[ilen] = 0;

                    // Repaint current prompt line (keep the "> ")
                    dlen = prompt_off;
                    disp[dlen] = 0;
                    append_disp(inbuf, ilen);
                    redraw(dpy, win, gc);
                }
            }
            else if (ev.type == DestroyNotify)
            {
                goto quit;
            }
        }

        // 2) Poll child pipes with a short timeout
        int nfds = 0;
        if (fd_from_child_out >= 0)
        {
            pfds[nfds].fd = fd_from_child_out;
            pfds[nfds].events = POLLIN;
            pfds[nfds].revents = 0;
            nfds++;
        }
        if (fd_from_child_err >= 0)
        {
            pfds[nfds].fd = fd_from_child_err;
            pfds[nfds].events = POLLIN;
            pfds[nfds].revents = 0;
            nfds++;
        }

        int timeout_ms = 16; // ~60Hz UI tick
        int pr = nfds ? poll(pfds, nfds, timeout_ms) : (usleep(timeout_ms * 1000), 0);

        if (pr > 0)
        {
            int idx = 0;
            if (fd_from_child_out >= 0)
            {
                if (pfds[idx].revents & POLLIN)
                {
                    char tmp[2048];
                    ssize_t r = read(fd_from_child_out, tmp, sizeof(tmp));
                    if (r > 0)
                    {
                        append_disp(tmp, (size_t)r);
                        redraw(dpy, win, gc);
                    }
                    else if (r == 0)
                    {
                        close(fd_from_child_out);
                        fd_from_child_out = -1;
                    }
                }
                idx++;
            }
            if (fd_from_child_err >= 0)
            {
                if (pfds[idx].revents & POLLIN)
                {
                    char tmp[2048];
                    ssize_t r = read(fd_from_child_err, tmp, sizeof(tmp));
                    if (r > 0)
                    {
                        append_disp(tmp, (size_t)r);
                        redraw(dpy, win, gc);
                    }
                    else if (r == 0)
                    {
                        close(fd_from_child_err);
                        fd_from_child_err = -1;
                    }
                }
            }
        }
    }

quit:
    if (fd_to_child >= 0)
        close(fd_to_child);
    if (fd_from_child_out >= 0)
        close(fd_from_child_out);
    if (fd_from_child_err >= 0)
        close(fd_from_child_err);
    if (child_pid > 0)
    {
        int st;
        waitpid(child_pid, &st, 0);
    }

    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}