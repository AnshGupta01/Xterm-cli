#ifndef XTERM_REDEFINED_H
#define XTERM_REDEFINED_H

/* --- Core System and Standard Libraries --- */
#include <dirent.h>
#include <fcntl.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* --- X Window System Libraries --- */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

/* --- Configuration Macros --- */
#define WINDOW_WIDTH 900
#define WINDOW_HEIGHT 700
#define MAX_TABS 5
#define MAX_CMD_LEN 1024
#define MAX_BUFFER_SIZE 8192
#define MAX_HISTORY 10000
#define HISTORY_DISPLAY 1000

/* --- Data Structures --- */

//Singel tab.
typedef struct {
    pid_t shell_process_id;
    int input_pipe[2];
    int output_pipe[2];
    char display_buffer[MAX_BUFFER_SIZE];
    char current_command[MAX_CMD_LEN];
    int cursor_position;
    int is_active;
} Tab;

//Main app
typedef struct {
    // X11 GUI components
    Display *x11_display;
    Window x11_window;
    GC graphics_context;
    
    // Tab management
    Tab tabs[MAX_TABS];
    int current_tab_index;
    int total_tabs;
    
    // Command history and search state
    char command_history[MAX_HISTORY][MAX_CMD_LEN];
    int history_count;
    int history_position;
    int search_mode;
    char search_term[MAX_CMD_LEN];
    
    // MultiWatch feature
    int multiwatch_running;
    int multiwatch_interrupted;
} Terminal;

// Global pointer to the main terminal instance for signal handlers.
extern Terminal *g_terminal;

// Globals for the MultiWatch command functionality (defined in commands.c).
extern pid_t multiwatch_pids[10];
extern int multiwatch_pipes[10];
extern int multiwatch_count;
extern int multiwatch_active;

// Helper Functions
void add_command_to_history(Terminal *term, char *command);
void add_text_to_buffer(Tab *tab, char *text);
void autocomplete_filename(Terminal *term, char *partial_name);
char* trim_whitespace(char *str);
void parse_multiwatch_commands(char *args, char commands[][MAX_CMD_LEN], int *count);

// Built-in Commands
void builtin_history_display(Terminal *term);
void builtin_multiwatch(Terminal *term, char *args);
void builtin_search_history(Terminal *term);

// MultiWatch Specific
void check_multiwatch_output(Terminal *term);
void multiwatch_signal_handler(int sig);

// Core Terminal and Resource Management
Terminal* create_new_terminal();
void cleanup_terminal_resources(Terminal *term);

// Command Execution
void execute_user_command(Terminal *term, char *command);
void handle_input_redirection(char *command, char *input_file);
void handle_output_redirection(char *command, char *output_file);
void handle_pipe_commands(Terminal *term, char *command);
void run_external_command(Terminal *term, char *cmd_line);

// History File I/O
void load_history_from_file(Terminal *term);
void save_history_to_file(Terminal *term);

// Signal Handling
void setup_signal_handlers();
void handle_sigint(int sig);
void handle_sigtstp(int sig);

// Tab Management
void close_current_tab(Terminal *term);
void initialize_new_tab(Terminal *term, int tab_index);
void switch_to_tab(Terminal *term, int tab_index);

// X11 GUI and Event Handling
void draw_terminal_window(Terminal *term);
void handle_x11_events(Terminal *term);
void process_keyboard_input(Terminal *term, XKeyEvent *event);
void setup_x11_gui(Terminal *term);

#endif