#ifndef GUI_H
#define GUI_H

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <pthread.h>

#define MAX_LINE 4096
#define MAX_BUFFER 65536
#define MAX_TABS 10
#define LINE_HEIGHT 16
#define TAB_HEIGHT 25

typedef struct {
    char buffer[MAX_BUFFER];
    size_t length;
    size_t scroll_offset;
} TextBuffer;

typedef struct {
    char input[MAX_LINE];
    size_t input_len;
    size_t cursor_pos;
} InputLine;

typedef struct Tab Tab;
struct Tab {
    int id;
    char title[64];
    TextBuffer text_buffer;
    InputLine input_line;
    int shell_pid;
    int shell_in_fd;
    int shell_out_fd;
    int shell_err_fd;
    int active;
    Tab* next;
};

typedef struct {
    Display* display;
    Window window;
    GC gc;
    XFontStruct* font;
    int screen;
    int window_width;
    int window_height;
    Tab* tabs;
    Tab* current_tab;
    int tab_count;
    int running;
    pthread_mutex_t mutex;
} GUI;

// GUI functions
int gui_init(GUI* gui);
void gui_cleanup(GUI* gui);
void gui_run(GUI* gui);
void gui_draw_text(GUI* gui);
void gui_draw_input(GUI* gui);
void gui_draw_tabs(GUI* gui);
void gui_append_text(GUI* gui, const char* text);
void gui_clear_screen(GUI* gui);
void gui_handle_key_press(GUI* gui, XKeyEvent* event);

// Tab management
Tab* tab_create(int id, const char* title);
void tab_destroy(Tab* tab);
void tab_switch(GUI* gui, int tab_id);
Tab* tab_new(GUI* gui);
void tab_close(GUI* gui, int tab_id);

extern GUI* g_gui;

#endif