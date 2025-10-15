#ifndef XTERM_REDEFINED_H
#define XTERM_REDEFINED_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <locale.h>
#include <dirent.h>
#include <time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

// Basic window config
#define WINDOW_WIDTH 900
#define WINDOW_HEIGHT 700
#define MAX_TABS 5
#define MAX_CMD_LEN 1024
#define MAX_BUFFER_SIZE 8192
#define MAX_HISTORY 10000
#define HISTORY_DISPLAY 1000
#define MAX_SCROLL_LINES 2000

typedef struct {
    char content[MAX_SCROLL_LINES][MAX_CMD_LEN];
    int total_lines;
    int scroll_offset;
    int visible_lines;
    int is_active;  
    char title[256];
} ScrollView;

typedef struct {
    pid_t shell_process_id;
    int input_pipe[2];
    int output_pipe[2];
    char display_buffer[MAX_BUFFER_SIZE];
    char current_command[MAX_CMD_LEN];
    int cursor_position;
    int is_active;
    ScrollView scroll_view;
} Tab;

typedef struct {
    Display *x11_display;
    Window x11_window;
    GC graphics_context;
    
    Tab tabs[MAX_TABS];
    int current_tab_index;
    int total_tabs;
    
    char command_history[MAX_HISTORY][MAX_CMD_LEN];
    int history_count;
    int history_position;
    int search_mode;
    char search_term[MAX_CMD_LEN];
    
    int multiwatch_running;
    int multiwatch_interrupted;
} Terminal;

extern Terminal *g_terminal;

extern pid_t multiwatch_pids[10];
extern int multiwatch_pipes[10];
extern int multiwatch_count;
extern int multiwatch_active;

// Basic helper stuff
void add_command_to_history(Terminal *term, char *command);
void add_text_to_buffer(Tab *tab, char *text);
void autocomplete_filename(Terminal *term, char *partial_name);
char* trim_whitespace(char *str);
void parse_multiwatch_commands(char *args, char commands[][MAX_CMD_LEN], int *count);

// Scroll functions
void init_scroll_view(ScrollView *scroll);
void enter_scroll_mode(Tab *tab, const char *title);
void exit_scroll_mode(Tab *tab);
void add_line_to_scroll(ScrollView *scroll, const char *line);
void scroll_up(ScrollView *scroll);
void scroll_down(ScrollView *scroll);
void scroll_page_up(ScrollView *scroll);
void scroll_page_down(ScrollView *scroll);
void scroll_to_top(ScrollView *scroll);
void scroll_to_bottom(ScrollView *scroll);

// Commands 
void builtin_history_display(Terminal *term);
void builtin_multiwatch(Terminal *term, char *args);
void builtin_search_history(Terminal *term);

void check_multiwatch_output(Terminal *term);
void multiwatch_signal_handler(int sig);

// Core terminal stuff
Terminal* create_new_terminal();
void cleanup_terminal_resources(Terminal *term);

void execute_user_command(Terminal *term, char *command);
void handle_input_redirection(char *command, char *input_file);
void handle_output_redirection(char *command, char *output_file);
void handle_pipe_commands(Terminal *term, char *command);
void run_external_command(Terminal *term, char *cmd_line);

// File operations
void load_history_from_file(Terminal *term);
void save_history_to_file(Terminal *term);

// Signal stuff  
void setup_signal_handlers();
void handle_sigint(int sig);
void handle_sigtstp(int sig);

// Tab operations
void close_current_tab(Terminal *term);
void initialize_new_tab(Terminal *term, int tab_index);
void switch_to_tab(Terminal *term, int tab_index);

// GUI functions
void draw_terminal_window(Terminal *term);
void draw_scroll_view(Terminal *term, ScrollView *scroll);
void handle_x11_events(Terminal *term);
void process_keyboard_input(Terminal *term, XKeyEvent *event);
void process_scroll_input(Terminal *term, XKeyEvent *event);
void setup_x11_gui(Terminal *term);

#endif