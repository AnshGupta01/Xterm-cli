# XTerm Project - Advanced X11 Terminal

A feature-rich terminal emulator built with X11, implementing advanced shell functionality including tabs, history, autocomplete, and multi-command monitoring.

## Project Structure
```
XTerm/
├── src/
│   ├── main.c          # Main program entry point
│   ├── terminal.c      # Core terminal management
│   ├── gui.c           # X11 GUI functions
│   ├── commands.c       # Command processing
├── include/
│   └── xterm.h        # Header file with all declarations
├── build/              # Object files (created during build)
├── Makefile           # Build system
└── README.md          # This file
```

## Features Implemented

1. **X11 Graphical Interface** - Basic window with tabs
2. **Multiple Tabs** - Up to 3 tabs, switch with clicks or Ctrl+T
3. **Command Execution** - Executes external commands via shell
4. **Built-in Commands**:
   - `history` - Shows command history 
   - `exit` - Closes current tab
   - `multiWatch [\"cmd1\", \"cmd2\"]` - Parallel command execution
   - `Tab`: Auto-complete filenames
5. **Basic Navigation**:
   - `Ctrl+A` - Move to start of line
   - `Ctrl+E` - Move to end of line
   - `Ctrl+R` - Search history
   - `Ctrl+C` - Interrupt command
   - `Ctrl+T` - New tab
   - `Ctrl+W` - Close current tab
   - `Ctrl+Z`: Suspend command (move to background)
   - `Left/Right`: Move cursor character by character
   - `Up/Down`: Browse command history

## Basic Commands
- All standard shell commands work
- Built-ins: `cd`, `pwd`, `clear`, `exit`, `history`
- I/O redirection: `cmd < input > output`
- Pipelines: `cmd1 | cmd2 | cmd3`

## Build && Run Instructions
`git clone <repository> && cd xterm-project && make && ./xterm`

## Files
- `xterm.h` - Header with all declarations
- `main.c` - Entry point
- `terminal.c` - Core terminal logic and tab management
- `gui.c` - X11 GUI handling and event processing  
- `commands.c` - Command execution and built-ins
- `Makefile` - Build configuration

## Requirements
- **macOS** (tested on macOS 10.14+)
- **XQuartz** - X11 server for macOS
- **Xcode Command Line Tools** - For compilation
- **Homebrew** (optional, for easy XQuartz installation)