# XTerm Project - Advanced X11 Terminal

A feature-rich terminal emulator built with X11, implementing advanced shell functionality including tabs, history, autocomplete, and multi-command monitoring.

## Features

- **Phase 1**: Basic X11 terminal window with text rendering
- **Phase 2**: Shell process integration with pipes
- **Phase 3**: Built-in commands and working directory management
- **Phase 4**: Multiline Unicode input support
- **Phase 5**: I/O redirection (< > >>)
- **Phase 6**: Multi-stage pipelines
- **Phase 7**: Line editing (Ctrl+A, Ctrl+E)
- **Phase 8**: Signal handling (Ctrl+C, Ctrl+Z)
- **Phase 9**: Searchable command history (Ctrl+R)
- **Phase 10**: Multiple shell tabs
- **Phase 11**: Filename autocomplete (Tab)
- **Phase 12**: MultiWatch command for parallel monitoring

## Build && Run Instructions

`git clone <repository> && cd xterm-project && make && ./xterm`


## Usage

### Basic Commands
- All standard shell commands work
- Built-ins: `cd`, `pwd`, `clear`, `exit`, `history`
- I/O redirection: `cmd < input > output`
- Pipelines: `cmd1 | cmd2 | cmd3`

### Key Bindings
- **`Ctrl+A`**: Move to beginning of line
- **`Ctrl+E`**: Move to end of line
- **`Ctrl+C`**: Interrupt foreground process
- **`Ctrl+Z`**: Suspend foreground process
- **`Ctrl+R`**: Search command history
- **`Tab`**: Autocomplete filenames

### MultiWatch
`multiwatch ["ls -la","ps aux","date"]`

Monitors multiple commands in parallel with timestamped output.

### Tabs
- Click on tab headers to switch
- New tabs create independent shell sessions
- Each tab maintains separate history and state

## Requirements

- X11 development libraries
- GCC or compatible C compiler
- POSIX-compliant system (Linux/macOS/BSD)
