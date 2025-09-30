#ifndef XTERM_H
#define XTERM_H

// Standard includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <locale.h>
#include <time.h>
#include <dirent.h>

// X11 includes
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>

// Configuration constants
#define MAX_TABS 5
#define MAX_CMD_LEN 1024
#define MAX_BUFFER_SIZE 8192
#define MAX_HISTORY 10000          
#define HISTORY_DISPLAY 1000       
#define WINDOW_WIDTH 900
#define WINDOW_HEIGHT 700

// Tab structure - each tab is independent shell
typedef struct {
    int is_active;
    pid_t shell_process_id;
    int input_pipe[2];     // pipe to send input to shell
    int output_pipe[2];    // pipe to get output from shell
    char display_buffer[MAX_BUFFER_SIZE];
    char current_command[MAX_CMD_LEN];
    int cursor_position;
} Tab;

// Main terminal structure
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
    
    // MultiWatch state
    int multiwatch_running;
    int multiwatch_interrupted;
} Terminal;

// Function declarations
Terminal* create_new_terminal();
void cleanup_terminal_resources(Terminal *term);

// Tab management functions
void initialize_new_tab(Terminal *term, int tab_index);
void switch_to_tab(Terminal *term, int tab_index);
void close_current_tab(Terminal *term);

// X11 GUI functions
void setup_x11_gui(Terminal *term);
void draw_terminal_window(Terminal *term);
void handle_x11_events(Terminal *term);
void process_keyboard_input(Terminal *term, XKeyEvent *event);

// Command execution functions
void execute_user_command(Terminal *term, char *command);
void run_external_command(Terminal *term, char *cmd_line);
void handle_input_redirection(char *command, char *input_file);
void handle_output_redirection(char *command, char *output_file);
void handle_pipe_commands(Terminal *term, char *command);

// Built-in command functions
void builtin_multiwatch(Terminal *term, char *args);
void builtin_history_display(Terminal *term);
void builtin_search_history(Terminal *term);

// Helper functions
void add_text_to_buffer(Tab *tab, char *text);
void add_command_to_history(Terminal *term, char *command);
void autocomplete_filename(Terminal *term, char *partial_name);
char* trim_whitespace(char *str);
void parse_multiwatch_commands(char *args, char commands[][MAX_CMD_LEN], int *count);

// History file functions
void save_history_to_file(Terminal *term);
void load_history_from_file(Terminal *term);

// Signal handling
void setup_signal_handlers();
void handle_sigint(int sig);
void handle_sigtstp(int sig);
void multiwatch_signal_handler(int sig);

// MultiWatch globals (defined in commands.c)
extern pid_t multiwatch_pids[10];
extern int   multiwatch_pipes[10];
extern int   multiwatch_count;
extern int   multiwatch_active;

// MultiWatch helper
void check_multiwatch_output(Terminal *term);

// Global terminal pointer for signal handling
extern Terminal *g_terminal;

#endif