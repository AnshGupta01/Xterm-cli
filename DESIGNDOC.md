# XTerm Design Documentation
This document describes the design and implementation details of XTerm, a custom terminal emulator with X11 GUI for macOS.

## High-Level Design

XTerm follows a modular architecture with clear separation of concerns:

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   X11 GUI       │    │  Terminal Core  │    │ Command Engine  │
│   (gui.c)       │◄──►│  (terminal.c)   │◄──►│  (commands.c)   │
└─────────────────┘    └─────────────────┘    └─────────────────┘
         │                       │                       
         ▼                       ▼                       
┌─────────────────┐    ┌─────────────────┐    
│   Event Loop    │    │   Tab Manager   │    
│   (main.c)      │    │   (terminal.c)  │   
└─────────────────┘    └─────────────────┘  
```

## Overview
This project implements a minimal, educational terminal emulator on top of the X11 window system with core features: multiple tabs (up to 3), command execution (built-ins and external), command history, simple navigation, basic completion, and Unicode-capable multiline input/output using setlocale(), read(), and write(). The focus is clarity and simplicity appropriate for a college project.

## Goals
- Provide a simple, responsive X11 GUI with tabs and a prompt.
- Execute built-in commands and external programs with basic redirection and pipelines.
- Support Unicode input/output and multiline text handling.
- Maintain a clean, modular codebase with minimal dependencies.

## System Context
- OS: Unix-like (macOS with XQuartz, Linux with X11)
- GUI: X11 (Xlib)
- Language/Std: C (C11), Make build system
- Dependencies: X11 headers and libs, standard C library

## High-Level Architecture
- GUI Layer (X11): window creation, tab bar, text rendering, event loop.
- Input Layer: key/mouse handling, history navigation, basic completion, Unicode decoding.
- Command Layer: built-ins, external command execution, redirection/pipeline handling.
- Unicode Layer: locale setup, UTF-8 ↔ wide conversions, multiline parsing.
- State/Model: terminal state (tabs, buffers, cursor, history), simple screen flow.

## Module Breakdown
- `include/xterm.h`
  - Shared types, constants, and function prototypes.
  - Terminal state struct: display, window, GC, font, dimensions, tab state, input buffer (wchar_t), history.
- `src/main.c`
  - Entry point, setup_locale(), signal setup, terminal init, GUI init, event loop dispatch, cleanup.
- `src/gui.c`
  - X11 initialization (XOpenDisplay, window, GC, font), event selection.
  - Rendering: clear, draw tabs, draw prompt, draw input, cursor.
  - Event loop handlers: Expose, KeyPress, ButtonPress.
  - Tab interactions: create/close/switch; basic layout and scrolling.
- `src/terminal.c`
  - Terminal state helpers: clear screen, scroll, cursor movement.
  - Multiline parsing (split on \n and escaped \n), validation, simple completions.
  - Optional state save/load, error reporting utilities.
- `src/commands.c`
  - Command dispatcher: built-ins first, otherwise external.
  - Built-ins: exit, clear, history, cd, pwd (as applicable).
  - External exec via system() with temp-file capture for simplicity.
  - Output display bridging to GUI with Unicode conversions.

## Key Flows
1) `Startup`
   - setlocale(LC_ALL, ""), initialize Terminal and X11, draw initial UI.
2) `Input Handling`
   - KeyPress → translate bytes to wchar_t with mbrtowc → append to input_buffer.
   - Special keys: Enter (process), Backspace, arrows (history), Tab (basic completion), Ctrl+T/W/C.
3) `Command Execution`
   - Convert wide input to UTF-8.
   - Match built-ins; on external, run via system() with output redirected to a temp file.
   - Read results and display.
4) `Unicode & Multiline`
   - Internal input kept as wchar_t.
   - wide_to_utf8/utf8_to_wide for GUI/system boundaries.
   - Parse \n sequences and actual newlines; display line by line.


## Features Implemented

### X11 Graphical Interface

The terminal uses Xlib to create a simple window with basic GUI controls. Tabs are displayed at the top with click and keyboard navigation support. The display area supports drawing text lines with scrolling and a visible cursor indication.

### Multiple Tabs

Supports up to 5 tabs, each running its own shell process connected through pipes. Tabs can be opened with Ctrl+T and closed with Ctrl+W. Tab switching is possible via mouse clicks on tabs or programmatically.

### Command Execution

User commands are processed with a basic dispatcher prioritizing built-in commands (e.g., history, exit, multiWatch). External commands run via shell processes through pipes, with stdout/stderr captured and displayed asynchronously.

### Unicode Support

Unicode input and output are enabled by setting locale (setlocale) and handling UTF-8 encoded text internally. This allows proper rendering of international characters and multi-byte sequences in the terminal display.

### Command History

A command history buffer stores up to 10,000 recent commands, supporting navigation with arrow keys and search mode (Ctrl+R). History persists across sessions through a simple file save and load mechanism.

### MultiWatch Command

Enables running multiple commands in parallel as child processes, capturing and displaying their output concurrently within the terminal window. MultiWatch processes can be interrupted with Ctrl+C.

### Input Line Editing and Completion

Supports line editing with cursor movement (arrow keys), backspace, and tab-based filename autocomplete using directory listing. The prompt dynamically adjusts during search mode and normal input.

### Scrollable Output View

Large outputs such as command history or file completion results are shown in a scrollable view with paging and line-by-line scrolling using keyboard keys (arrows, PageUp/Down, Home/End).

### Signal Handling and Process Control

Keyboard signals Ctrl+C and Ctrl+Z send SIGINT and SIGTSTP respectively to running shell processes for interruption or suspension, enabling typical Unix shell behavior inside tabs.

## Error Handling
- Checks on X11 resources, memory allocation, and conversions.
- Graceful fallbacks (e.g., ASCII draw when conversion fails).
- Informative messages in-terminal and stderr for critical errors.

## Limitations & Future Work
- No PTY/termios-based emulation; limited escape sequence handling.
- Single font and basic metrics; no dynamic reflow or text selection.
- Future improvements: PTY integration, colors, file-system completion, history persistence.

## Rationale for Simplicity
- Prioritizes readability.
- Uses standard library and Xlib only.
- Clear separation of concerns enables incremental enhancement.
