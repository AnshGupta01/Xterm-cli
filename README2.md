# MyTerm - Custom Terminal with X11 GUI

A custom terminal emulator built with X11 for macOS, designed to run with XQuartz. This project implements a fully functional shell with GUI, supporting multiple tabs, command history, pipes, redirection, and advanced features.

## Features Implemented

### ✅ Core Features
- **X11 Graphical User Interface** - Custom terminal window with XQuartz
- **Multiple Tab Support** - Switch between independent shell instances
- **External Command Execution** - Run any system command
- **Unicode Input Support** - Handle multilingual text input
- **Input/Output Redirection** - Support for `<` and `>` operators
- **Pipe Support** - Chain commands with `|` operator
- **Built-in Commands** - `cd`, `history`, and custom `multiWatch`

### ✅ Advanced Features
- **Line Navigation** - Ctrl+A (start) and Ctrl+E (end) shortcuts
- **Signal Handling** - Ctrl+C (interrupt) and Ctrl+Z (suspend) support
- **Command History** - Store and search through 10,000 commands
- **History Search** - Ctrl+R for interactive history search
- **Autocomplete** - Tab completion for filenames
- **MultiWatch Command** - Monitor multiple commands simultaneously

## Requirements

- **macOS** (tested on macOS 10.14+)
- **XQuartz** - X11 server for macOS
- **Xcode Command Line Tools** - For compilation
- **Homebrew** (optional, for easy XQuartz installation)

## Quick Start

### 1. Install Prerequisites

```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install XQuartz via Homebrew (recommended)
make install-deps

# OR download manually from https://www.xquartz.org
```

### 2. Setup Environment

```bash
# Clone or extract the project
cd MyTerm

# Setup environment variables
make setup-env

# Test XQuartz installation
make test-x11
```

### 3. Build and Run

```bash
# Build the project
make

# Run the terminal
make run
```

**Important**: After installing XQuartz, you must **log out and log back in** for the changes to take effect.

## Manual Setup (if make commands fail)

### Install XQuartz manually:
1. Download from [https://www.xquartz.org](https://www.xquartz.org)
2. Install the .pkg file
3. Log out and log back in
4. Open XQuartz application

### Set environment variable:
```bash
export DISPLAY=:0
```

### Compile manually:
```bash
gcc -std=c11 -Wall -Wextra -O2 -I/opt/X11/include -Iinclude src/*.c -L/opt/X11/lib -lX11 -lXext -o myterm
```

## Usage

### Starting the Terminal
```bash
./myterm
```

### Key Bindings
- **Ctrl+A** - Move cursor to beginning of line
- **Ctrl+E** - Move cursor to end of line
- **Ctrl+C** - Interrupt current command
- **Ctrl+Z** - Suspend current command
- **Ctrl+R** - Search command history
- **Ctrl+T** - Create new tab
- **F1-F5** - Switch between tabs
- **Tab** - Autocomplete filenames

### Built-in Commands
- `cd [directory]` - Change directory
- `history` - Show command history
- `multiWatch ["cmd1", "cmd2", ...]` - Monitor multiple commands

### Examples

```bash
# Basic commands
user@myterm> ls -la
user@myterm> cd /tmp
user@myterm> pwd

# Redirection
user@myterm> ls > files.txt
user@myterm> sort < files.txt
user@myterm> cat input.txt > output.txt

# Pipes
user@myterm> ls *.txt | wc -l
user@myterm> ps aux | grep myterm | head -5

# MultiWatch
user@myterm> multiWatch ["date", "uptime", "who"]
```

## Project Structure

```
MyTerm/
├── src/
│   ├── main.c          # Main program entry point
│   ├── terminal.c      # Core terminal management
│   ├── gui.c           # X11 GUI functions
│   ├── command.c       # Command processing
│   ├── history.c       # History management
│   └── utils.c         # Utility functions
├── include/
│   └── myterm.h        # Header file with all declarations
├── build/              # Object files (created during build)
├── Makefile           # Build system
└── README.md          # This file
```

## Troubleshooting

### Common Issues

**"Cannot connect to X server"**
- Ensure XQuartz is running: `open -a XQuartz`
- Set display: `export DISPLAY=:0`
- Check if XQuartz is installed: `ls /opt/X11`

**"X11/Xlib.h not found"**
- Install XQuartz properly
- Verify installation: `/opt/X11/include/X11/Xlib.h` should exist

**Window doesn't appear**
- Check XQuartz is running in the dock
- Try: `xeyes` to test X11 forwarding
- Restart XQuartz application

**Commands not working**
- The terminal uses `/bin/sh` by default
- Some advanced bash features may not work
- Try basic POSIX commands first

### Debug Mode

```bash
# Build with debug information
make debug

# Run with verbose output
./myterm
```

## Design Decisions

### Architecture
- **Modular Design**: Each feature is in its own source file
- **Process Isolation**: Each tab runs an independent shell process
- **Non-blocking I/O**: Uses select() for responsive GUI
- **Pipe Communication**: Parent-child communication via pipes

### XQuartz Compatibility
- Uses standard Xlib functions for maximum compatibility
- Handles display connection gracefully
- Works with default XQuartz installation paths

### Memory Management
- Static buffers for performance
- Proper cleanup on exit
- Safe string operations throughout

## Limitations

- **Platform**: macOS with XQuartz only
- **Shell**: Uses `/bin/sh` instead of full bash
- **Font**: Fixed-width font only
- **Colors**: Basic black/white color scheme
- **Resize**: Window resizing may cause display issues

## Future Enhancements

- [ ] Color syntax highlighting
- [ ] Configurable fonts and colors
- [ ] Window resizing support
- [ ] Mouse selection and copy/paste
- [ ] Background process management
- [ ] Configuration file support
- [ ] Plugin system for custom commands

## License

This project is created for educational purposes. Feel free to modify and distribute as needed.

## Contributing

This is a class project, but suggestions and improvements are welcome:
1. Fork the repository
2. Create a feature branch
3. Test thoroughly on macOS with XQuartz
4. Submit a pull request

## Support

For issues specific to this implementation:
- Check XQuartz installation first
- Verify environment variables are set
- Test with simple commands before complex ones
- Check the debug output for detailed error messages

---

**Note**: This terminal is designed as a learning project to understand system calls, process management, and X11 programming. It implements the core functionality required by the project specification while maintaining code readability and modularity.
