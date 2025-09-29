#ifndef MYTERM_H
#define MYTERM_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <locale.h>
#include <time.h>
#include <dirent.h>
#include <termios.h>

// Constants
#define MAX_TABS 5
#define BUFFER_SIZE 4096
#define MAX_COMMAND_LENGTH 1024
#define MAX_ARGS 64
#define MAX_PIPES 16
#define MAX_HISTORY 10000
#define HISTORY_DISPLAY 1000
#define FONT_HEIGHT 16
#define FONT_WIDTH 8
#define PROMPT "user@myterm> "
#define TEMP_FILE_PREFIX ".temp."

// Terminal dimensions
#define TERM_WIDTH 80
#define TERM_HEIGHT 24
#define WINDOW_WIDTH (TERM_WIDTH * FONT_WIDTH)
#define WINDOW_HEIGHT (TERM_HEIGHT * FONT_HEIGHT + 30) // +30 for tab bar

// Colors
#define BLACK_PIXEL 0x000000
#define WHITE_PIXEL 0xFFFFFF
#define GREEN_PIXEL 0x00FF00
#define BLUE_PIXEL 0x0000FF

// Tab structure
typedef struct {
    int active;
    pid_t shell_pid;
    int pipe_in[2];
    int pipe_out[2];
    char buffer[BUFFER_SIZE];
    int buffer_pos;
    char current_line[MAX_COMMAND_LENGTH];
    int cursor_pos;
    int line_count;
} Tab;

// Terminal structure
typedef struct {
    Display *display;
    Window window;
    GC gc;
    int screen;
    XFontStruct *font;
    Tab tabs[MAX_TABS];
    int current_tab;
    int num_tabs;
    char history[MAX_HISTORY][MAX_COMMAND_LENGTH];
    int history_count;
    int history_pos;
    char search_term[MAX_COMMAND_LENGTH];
    int search_mode;
} Terminal;

// Function declarations
// Terminal management
Terminal* create_terminal(void);
void cleanup_terminal(Terminal *term);
void setup_locale(void);

// Tab management
void init_tab(Terminal *term, int tab_index);
void switch_tab(Terminal *term, int tab_index);
void create_new_tab(Terminal *term);
void close_tab(Terminal *term, int tab_index);

// X11 GUI functions
void init_x11(Terminal *term);
void draw_terminal(Terminal *term);
void draw_tabs(Terminal *term);
void draw_text_buffer(Terminal *term);
void draw_cursor(Terminal *term, int line_x, int line_y, int char_width, int line_height); // NEW FUNCTION
void handle_x11_events(Terminal *term);
void handle_key_press(Terminal *term, XKeyEvent *event);

// Command processing
void process_command(Terminal *term, char *command);
void execute_external_command(Terminal *term, char **args);
void handle_redirection(char **args, int *input_fd, int *output_fd);
void handle_pipes(Terminal *term, char *command);
void parse_command(char *command, char ***commands, int *num_commands);

// Built-in commands
void builtin_cd(char **args);
void builtin_history(Terminal *term);
void builtin_multiwatch(Terminal *term, char **args);

// History management
void add_to_history(Terminal *term, char *command);
void search_history(Terminal *term, char *search_term);
void load_history_from_file(Terminal *term);    // NEW FUNCTION
void save_history_to_file(Terminal *term);      // NEW FUNCTION

// Signal handling
void setup_signal_handlers(Terminal *term);
void sigint_handler(int sig);
void sigtstp_handler(int sig);

// Utilities
void append_to_buffer(Terminal *term, const char *text);
void clear_current_line(Terminal *term);
void move_cursor_start(Terminal *term);
void move_cursor_end(Terminal *term);
char** tokenize_command(char *command);
void free_tokens(char **tokens);
void autocomplete_filename(Terminal *term, char *partial_name);

// Unicode support
void setup_unicode(void);
int is_utf8_continuation(unsigned char c);

// String utilities
char* trim_whitespace(char* str);
int count_words(const char* str);
void safe_strcpy(char* dest, const char* src, size_t dest_size);
void safe_strcat(char* dest, const char* src, size_t dest_size);

// File utilities
int file_exists(const char* filename);
long file_size(const char* filename);

// Memory management utilities
void* safe_malloc(size_t size);
void* safe_realloc(void* ptr, size_t size);

// Time utilities
char* get_timestamp(void);

// Color utilities
const char* get_color_code(int color);
void print_colored(const char* text, int color);

// Debug functions
void debug_print(const char *format, ...);

#endif // MYTERM_H
