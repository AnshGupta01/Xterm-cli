#include "../include/myterm.h"
#include <stdarg.h>

void autocomplete_filename(Terminal *term, char *partial_name) {
    DIR *dir;
    struct dirent *entry;
    char matches[256][256];
    int match_count = 0;
    int partial_len = strlen(partial_name);

    if (partial_len == 0) return;

    // Open current directory
    dir = opendir(".");
    if (!dir) return;

    // Find matching files
    while ((entry = readdir(dir)) != NULL && match_count < 256) {
        if (strncmp(entry->d_name, partial_name, partial_len) == 0) {
            safe_strcpy(matches[match_count], entry->d_name, 256);
            match_count++;
        }
    }
    closedir(dir);

    Tab *tab = &term->tabs[term->current_tab];

    if (match_count == 0) {
        // No matches
        return;
    } else if (match_count == 1) {
        // Single match - complete it
        char *completion = matches[0] + partial_len;
        int completion_len = strlen(completion);

        if (tab->cursor_pos + completion_len < MAX_COMMAND_LENGTH - 1) {
            safe_strcat(tab->current_line, completion, MAX_COMMAND_LENGTH);
            tab->cursor_pos += completion_len;

            // Add space after filename
            if (tab->cursor_pos < MAX_COMMAND_LENGTH - 2) {
                safe_strcat(tab->current_line, " ", MAX_COMMAND_LENGTH);
                tab->cursor_pos++;
            }
        }
    } else {
        // Multiple matches - find common prefix
        int common_len = strlen(matches[0]);

        for (int i = 1; i < match_count; i++) {
            int j;
            for (j = 0; j < common_len && j < strlen(matches[i]); j++) {
                if (matches[0][j] != matches[i][j]) {
                    break;
                }
            }
            common_len = j;
        }

        if (common_len > partial_len) {
            // Add common prefix
            int add_len = common_len - partial_len;
            char common_part[256];
            strncpy(common_part, matches[0] + partial_len, add_len);
            common_part[add_len] = '\0';

            if (tab->cursor_pos + add_len < MAX_COMMAND_LENGTH - 1) {
                safe_strcat(tab->current_line, common_part, MAX_COMMAND_LENGTH);
                tab->cursor_pos += add_len;
            }
        } else {
            // Show all matches
            append_to_buffer(term, "\nMatches:\n");
            for (int i = 0; i < match_count; i++) {
                char match_line[300];
                snprintf(match_line, sizeof(match_line), "%d. %s\n", i + 1, matches[i]);
                append_to_buffer(term, match_line);
            }
        }
    }
}

void setup_unicode(void) {
    setlocale(LC_ALL, "");
    setlocale(LC_CTYPE, "en_US.UTF-8");
}

int is_utf8_continuation(unsigned char c) {
    return (c & 0xC0) == 0x80;
}

// Signal handling functions
extern Terminal *g_terminal;

void setup_signal_handlers(Terminal *term) {
    signal(SIGINT, sigint_handler);
    signal(SIGTSTP, sigtstp_handler);
    signal(SIGCHLD, SIG_IGN); // Prevent zombie processes
}

void sigint_handler(int sig) {
    // Handle Ctrl+C
    if (g_terminal) {
        Tab *tab = &g_terminal->tabs[g_terminal->current_tab];
        if (tab->active && tab->shell_pid > 0) {
            kill(tab->shell_pid, SIGINT);
        }
    }
}

void sigtstp_handler(int sig) {
    // Handle Ctrl+Z  
    if (g_terminal) {
        Tab *tab = &g_terminal->tabs[g_terminal->current_tab];
        if (tab->active && tab->shell_pid > 0) {
            kill(tab->shell_pid, SIGTSTP);
        }
    }
}

// String manipulation utilities
char* trim_whitespace(char* str) {
    char* end;

    // Trim leading space
    while (*str == ' ' || *str == '\t') str++;

    if (*str == 0) return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        end--;
    }

    // Write new null terminator
    *(end + 1) = '\0';

    return str;
}

int count_words(const char* str) {
    int count = 0;
    int in_word = 0;

    while (*str) {
        if (*str != ' ' && *str != '\t') {
            if (!in_word) {
                count++;
                in_word = 1;
            }
        } else {
            in_word = 0;
        }
        str++;
    }

    return count;
}

void safe_strcpy(char* dest, const char* src, size_t dest_size) {
    if (dest_size == 0) return;

    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

void safe_strcat(char* dest, const char* src, size_t dest_size) {
    size_t dest_len = strlen(dest);
    if (dest_len >= dest_size - 1) return;

    size_t available = dest_size - dest_len - 1;
    strncat(dest, src, available);
}

// File utilities
int file_exists(const char* filename) {
    return access(filename, F_OK) == 0;
}

long file_size(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) return -1;

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fclose(file);

    return size;
}

// Memory management utilities
void* safe_malloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    memset(ptr, 0, size);
    return ptr;
}

void* safe_realloc(void* ptr, size_t size) {
    void* new_ptr = realloc(ptr, size);
    if (!new_ptr) {
        fprintf(stderr, "Memory reallocation failed\n");
        free(ptr);
        exit(1);
    }
    return new_ptr;
}

// Time utilities
char* get_timestamp() {
    static char timestamp[32];
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);

    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    return timestamp;
}

// Color utilities for terminal output
const char* get_color_code(int color) {
    switch (color) {
        case 0: return "\033[0m";   // Reset
        case 1: return "\033[31m";  // Red
        case 2: return "\033[32m";  // Green
        case 3: return "\033[33m";  // Yellow
        case 4: return "\033[34m";  // Blue
        case 5: return "\033[35m";  // Magenta
        case 6: return "\033[36m";  // Cyan
        case 7: return "\033[37m";  // White
        default: return "\033[0m";
    }
}

void print_colored(const char* text, int color) {
    printf("%s%s%s", get_color_code(color), text, get_color_code(0));
}
