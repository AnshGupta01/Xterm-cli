# MyTerm Design Documentation

This document describes the design and implementation details of MyTerm, a custom terminal emulator with X11 GUI for macOS.

## Architecture Overview

### High-Level Design

MyTerm follows a modular architecture with clear separation of concerns:

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   X11 GUI       │    │  Terminal Core  │    │ Command Engine  │
│   (gui.c)       │◄──►│  (terminal.c)   │◄──►│  (command.c)    │
└─────────────────┘    └─────────────────┘    └─────────────────┘
         │                       │                       │
         ▼                       ▼                       ▼
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Event Loop    │    │   Tab Manager   │    │  History Mgr    │
│   (main.c)      │    │   (terminal.c)  │    │  (history.c)    │
└─────────────────┘    └─────────────────┘    └─────────────────┘
                                │
                                ▼
                       ┌─────────────────┐
                       │    Utilities    │
                       │    (utils.c)    │
                       └─────────────────┘
```

### Core Components

1. **Main Event Loop** (`main.c`) - Application entry point and initialization
2. **Terminal Management** (`terminal.c`) - Core terminal state and tab management  
3. **X11 GUI** (`gui.c`) - Graphics rendering and event handling
4. **Command Processing** (`command.c`) - Command parsing and execution
5. **History Management** (`history.c`) - Command history and search
6. **Utilities** (`utils.c`) - Helper functions and signal handling

## Feature Implementation Details

### 1. Graphical User Interface (X11)

**Design Approach**: Direct Xlib programming for maximum control and compatibility with XQuartz.

**Implementation**:
```c
// Connection and window creation
Display* display = XOpenDisplay(NULL);
Window window = XCreateSimpleWindow(...);

// Event handling
XSelectInput(display, window, ExposureMask | KeyPressMask | ButtonPressMask);

// Rendering
XDrawString(display, window, gc, x, y, text, length);
```

**Key Features**:
- **Window Management**: Creates a fixed-size window optimized for terminal use
- **Font Handling**: Uses system fixed-width font for consistent character spacing
- **Event Processing**: Handles keyboard input, mouse clicks, and window events
- **Double Buffering**: Minimizes flicker during redraws

**Design Decisions**:
- Used basic Xlib instead of higher-level toolkits for educational value
- Fixed window size to simplify text positioning calculations
- Monospace font assumption for character grid alignment

### 2. Multiple Tab Support

**Design Approach**: Each tab maintains its own shell process and communication pipes.

**Data Structure**:
```c
typedef struct {
    int active;                    // Tab state
    pid_t shell_pid;              // Child shell process ID
    int pipe_in[2], pipe_out[2];  // Bidirectional communication
    char buffer[BUFFER_SIZE];      // Display buffer
    char current_line[MAX_COMMAND_LENGTH]; // Input line
    int cursor_pos;               // Cursor position
} Tab;
```

**Implementation Details**:
- **Process Isolation**: Each tab forks a separate `/bin/sh` process
- **IPC Mechanism**: Uses pipe() system calls for parent-child communication
- **Non-blocking I/O**: fcntl() with O_NONBLOCK for responsive interface
- **Tab Switching**: F1-F5 keys and mouse clicks on tab bar

**Challenges Solved**:
- **Zombie Processes**: Proper cleanup with waitpid() and SIGCHLD handling
- **Buffer Synchronization**: Separate display buffers prevent tab interference
- **Process Communication**: Robust pipe handling with error checking

### 3. External Command Execution

**Design Approach**: Standard fork-exec pattern with comprehensive error handling.

**Implementation Flow**:
```c
pid_t pid = fork();
if (pid == 0) {
    // Child process
    handle_redirection(args, &input_fd, &output_fd);
    execvp(args[0], args);
    exit(1);
} else {
    // Parent process
    waitpid(pid, &status, 0);
}
```

**Features**:
- **Command Parsing**: Tokenizes input with proper quote handling
- **Path Resolution**: Uses execvp() for automatic PATH searching
- **Error Handling**: Comprehensive error reporting with perror()
- **Process Management**: Proper cleanup and status checking

### 4. Unicode and Multilingual Input

**Design Approach**: UTF-8 support with locale configuration.

**Implementation**:
```c
void setup_locale(void) {
    setlocale(LC_ALL, "");
    setlocale(LC_CTYPE, "en_US.UTF-8");
}

int is_utf8_continuation(unsigned char c) {
    return (c & 0xC0) == 0x80;
}
```

**Features**:
- **Locale Setup**: Proper UTF-8 locale configuration
- **Character Handling**: Multi-byte character support in input processing
- **Display**: X11 string functions handle UTF-8 automatically with proper locale

### 5. Input/Output Redirection

**Design Approach**: File descriptor manipulation using dup2() system call.

**Implementation**:
```c
void handle_redirection(char **args, int *input_fd, int *output_fd) {
    for (int i = 0; args[i]; i++) {
        if (strcmp(args[i], "<") == 0) {
            *input_fd = open(args[i+1], O_RDONLY);
            dup2(*input_fd, STDIN_FILENO);
        } else if (strcmp(args[i], ">") == 0) {
            *output_fd = open(args[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            dup2(*output_fd, STDOUT_FILENO);
        }
    }
}
```

**Supported Formats**:
- `command < input.txt` - Input redirection
- `command > output.txt` - Output redirection  
- `command < input.txt > output.txt` - Combined redirection

**Design Decisions**:
- **File Descriptor Management**: Careful opening/closing to prevent leaks
- **Error Handling**: Proper error reporting for file access issues
- **Argument Parsing**: Removes redirection tokens from command arguments

### 6. Pipe Support

**Design Approach**: Multi-process pipeline with intermediate pipe creation.

**Implementation Strategy**:
```c
// Create N-1 pipes for N commands
int pipes[MAX_PIPES][2];
for (int i = 0; i < num_commands - 1; i++) {
    pipe(pipes[i]);
}

// Fork N processes with appropriate stdin/stdout connections
for (int i = 0; i < num_commands; i++) {
    if (fork() == 0) {
        // Setup input from previous pipe
        if (i > 0) dup2(pipes[i-1][0], STDIN_FILENO);
        // Setup output to next pipe  
        if (i < num_commands - 1) dup2(pipes[i][1], STDOUT_FILENO);

        execvp(command[i][0], command[i]);
    }
}
```

**Features**:
- **Multi-stage Pipelines**: Supports unlimited command chaining
- **Process Synchronization**: Proper coordination between pipeline stages
- **Resource Management**: All pipes closed appropriately to prevent deadlocks

### 7. MultiWatch Command

**Design Approach**: Parallel process monitoring with select() multiplexing.

**Implementation Details**:
```c
void builtin_multiwatch(Terminal *term, char **args) {
    // Fork processes for each command
    for (int i = 0; i < num_cmds; i++) {
        pids[i] = fork();
        if (pids[i] == 0) {
            // Redirect output to temporary file
            sprintf(temp_file, ".temp.%d.txt", getpid());
            int fd = open(temp_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            dup2(fd, STDOUT_FILENO);

            // Execute command repeatedly
            while (1) {
                system(commands[i]);
                sleep(1);
            }
        }
    }

    // Monitor output with select()
    fd_set read_fds;
    while (1) {
        FD_ZERO(&read_fds);
        // Add temp file descriptors to set
        select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        // Process ready file descriptors
    }
}
```

**Key Features**:
- **Parallel Execution**: Each command runs in its own process
- **Output Multiplexing**: Uses select() to monitor multiple file descriptors
- **Timestamping**: Adds Unix timestamps to command outputs
- **Resource Cleanup**: Properly kills processes and removes temp files

### 8. Line Navigation (Ctrl+A, Ctrl+E)

**Design Approach**: Cursor position tracking with key combination detection.

**Implementation**:
```c
void handle_key_press(Terminal *term, XKeyEvent *event) {
    if (event->state & ControlMask) {
        switch (keysym) {
            case XK_a:  // Ctrl+A
                tab->cursor_pos = strlen(PROMPT);
                break;
            case XK_e:  // Ctrl+E  
                tab->cursor_pos = strlen(tab->current_line);
                break;
        }
    }
}
```

**Features**:
- **Control Key Detection**: X11 modifier state checking
- **Prompt Awareness**: Cursor positioning respects prompt length
- **Visual Feedback**: Immediate cursor position update and redraw

### 9. Signal Handling (Ctrl+C, Ctrl+Z)

**Design Approach**: Hierarchical signal propagation from GUI to shell processes.

**Implementation**:
```c
void setup_signal_handlers(Terminal *term) {
    signal(SIGINT, sigint_handler);
    signal(SIGTSTP, sigtstp_handler);
}

void sigint_handler(int sig) {
    if (g_terminal) {
        Tab *tab = &g_terminal->tabs[g_terminal->current_tab];
        if (tab->shell_pid > 0) {
            kill(tab->shell_pid, SIGINT);
        }
    }
}
```

**Features**:
- **Signal Interception**: GUI captures Ctrl+C and Ctrl+Z
- **Process Targeting**: Forwards signals to active tab's shell process
- **Shell Protection**: Terminal itself doesn't exit on Ctrl+C

### 10. Command History

**Design Approach**: Circular buffer with persistent file storage.

**Data Structure**:
```c
char history[MAX_HISTORY][MAX_COMMAND_LENGTH];
int history_count;
int history_pos;
```

**Implementation**:
```c
void add_to_history(Terminal *term, char *command) {
    // Avoid duplicates
    if (term->history_count > 0 && 
        strcmp(term->history[term->history_count - 1], command) == 0) {
        return;
    }

    // Add to circular buffer
    if (term->history_count < MAX_HISTORY) {
        strcpy(term->history[term->history_count++], command);
    } else {
        // Shift left and add at end
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            strcpy(term->history[i], term->history[i + 1]);
        }
        strcpy(term->history[MAX_HISTORY - 1], command);
    }

    // Persist to file
    FILE *f = fopen(".myterm_history", "a");
    fprintf(f, "%s\n", command);
    fclose(f);
}
```

**Features**:
- **Capacity**: Stores up to 10,000 commands in memory
- **Persistence**: Automatic saving to `.myterm_history` file
- **Duplicate Prevention**: Avoids consecutive identical commands
- **Display**: `history` command shows recent 1,000 entries

### 11. History Search (Ctrl+R)

**Design Approach**: Interactive search with exact and partial matching.

**Implementation**:
```c
void search_history(Terminal *term, char *search_term) {
    // First pass: exact matches
    for (int i = term->history_count - 1; i >= 0; i--) {
        if (strcmp(term->history[i], search_term) == 0) {
            // Copy to current line and return
            sprintf(tab->current_line, "%s%s", PROMPT, term->history[i]);
            return;
        }
    }

    // Second pass: partial matches
    int best_match_length = 0;
    int best_match_idx = -1;

    for (int i = term->history_count - 1; i >= 0; i--) {
        char *match = strstr(term->history[i], search_term);
        if (match && strlen(search_term) > 2) {
            // Find longest common substring
            int common_length = calculate_common_prefix(term->history[i], search_term);
            if (common_length > best_match_length) {
                best_match_length = common_length;
                best_match_idx = i;
            }
        }
    }

    // Use best partial match or report no match
}
```

**Features**:
- **Interactive Mode**: Ctrl+R enters search mode with special prompt
- **Exact Matching**: Prioritizes perfect matches
- **Partial Matching**: Falls back to substring matches (minimum 3 characters)
- **Smart Ranking**: Longest common prefix determines best match

### 12. Filename Autocomplete

**Design Approach**: Directory scanning with prefix matching and common substring calculation.

**Implementation**:
```c
void autocomplete_filename(Terminal *term, char *partial_name) {
    DIR *dir = opendir(".");
    struct dirent *entry;
    char matches[256][256];
    int match_count = 0;

    // Find matching files
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, partial_name, strlen(partial_name)) == 0) {
            strcpy(matches[match_count++], entry->d_name);
        }
    }

    if (match_count == 1) {
        // Single match: complete it
        char *completion = matches[0] + strlen(partial_name);
        strcat(tab->current_line, completion);
        strcat(tab->current_line, " ");
    } else if (match_count > 1) {
        // Multiple matches: show common prefix or list all
        int common_len = find_common_prefix(matches, match_count);
        if (common_len > strlen(partial_name)) {
            // Add common prefix
            strncat(tab->current_line, 
                   matches[0] + strlen(partial_name), 
                   common_len - strlen(partial_name));
        } else {
            // Show all matches
            for (int i = 0; i < match_count; i++) {
                sprintf(match_line, "%d. %s\n", i + 1, matches[i]);
                append_to_buffer(term, match_line);
            }
        }
    }
}
```

**Features**:
- **Directory Scanning**: Uses opendir()/readdir() for file enumeration
- **Prefix Matching**: Compares filename prefixes with partial input
- **Smart Completion**: Adds common prefix when multiple matches exist
- **Interactive Selection**: Lists options when no common prefix available

## System Call Usage

### Process Management
- `fork()` - Create child processes for tabs and commands
- `execvp()` - Execute external commands with PATH resolution
- `waitpid()` - Wait for child process completion
- `kill()` - Send signals to processes (SIGINT, SIGTSTP, SIGTERM)

### File I/O
- `pipe()` - Create communication channels between processes
- `dup2()` - Redirect file descriptors for input/output redirection
- `open()` - Open files for redirection and temporary storage
- `read()/write()` - Non-blocking I/O for pipe communication
- `fcntl()` - Set non-blocking mode on file descriptors

### System Information
- `select()` - Multiplex I/O operations for responsive GUI
- `opendir()/readdir()` - Directory traversal for autocomplete
- `signal()` - Set up signal handlers for Ctrl+C and Ctrl+Z

### Memory and String Operations
- Standard C library functions for string manipulation
- Dynamic memory allocation for command tokenization
- Locale functions for Unicode support

## Error Handling Strategy

### Defensive Programming
- **Null Pointer Checks**: All pointer dereferences are validated
- **Buffer Bounds**: String operations use safe versions (strncpy, strncat)
- **System Call Validation**: All system calls checked for error return values

### Graceful Degradation  
- **X11 Connection Failure**: Clear error message with setup instructions
- **File Access Errors**: Continue operation with user notification
- **Process Creation Failure**: Fallback to error reporting without crash

### Resource Management
- **File Descriptor Tracking**: Systematic opening/closing of all file descriptors
- **Memory Cleanup**: Proper deallocation of dynamically allocated memory
- **Process Cleanup**: Termination and cleanup of all child processes on exit

## Performance Considerations

### Memory Usage
- **Static Buffers**: Fixed-size buffers avoid dynamic allocation overhead
- **Circular Buffers**: Efficient history management without shifting
- **String Pooling**: Reuse of common strings like prompts

### I/O Efficiency
- **Non-blocking I/O**: Prevents GUI freezing during command execution
- **Batch Operations**: Group related X11 operations for efficiency
- **Selective Redraws**: Only update changed portions of display

### Process Management
- **Process Pooling**: Reuse shell processes across command executions
- **Signal Optimization**: Direct signal delivery to target processes
- **Cleanup Scheduling**: Deferred cleanup to avoid blocking main thread

## Testing Strategy

### Unit Testing Approach
- **Function Isolation**: Each module tested independently
- **Mock Dependencies**: X11 operations mocked for headless testing
- **Edge Case Coverage**: Boundary conditions and error paths tested

### Integration Testing
- **Multi-tab Operations**: Verify tab isolation and communication
- **Command Pipeline Testing**: Complex pipe chains with various commands
- **Signal Handling**: Interrupt and suspend behavior under load

### Platform Testing
- **XQuartz Versions**: Compatibility with different XQuartz releases
- **macOS Versions**: Testing across supported macOS versions
- **Hardware Variants**: Intel and Apple Silicon compatibility

## Future Improvements

### Performance Optimizations
- **Double Buffering**: Reduce screen flicker during updates
- **Font Caching**: Cache font metrics for faster text rendering
- **Command Prediction**: Preload commonly used commands

### Feature Extensions
- **Syntax Highlighting**: Color coding for different command types
- **Mouse Selection**: Text selection and copy/paste support
- **Window Management**: Resizable windows with dynamic layout
- **Configuration System**: User-customizable settings and themes

### Code Quality Improvements
- **Error Recovery**: Better handling of partial failures
- **Memory Optimization**: Reduce memory footprint for large histories
- **Code Documentation**: Comprehensive inline documentation
- **Automated Testing**: Continuous integration with automated test suite

---

This design document provides a comprehensive overview of MyTerm's architecture and implementation details. The modular design enables easy maintenance and future enhancements while providing a solid foundation for understanding Unix system programming concepts.
