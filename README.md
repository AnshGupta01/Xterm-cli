# Xterm-project
A GUI terminal based on the X11 library with advanced functionalities.

### Phase 0: Project setup

- Create a new C/C++ project with a Makefile, link against X11 (e.g., -lX11), and set locale for Unicode rendering; structure code into gui/, shell/, io/, features/ modules for clarity.
- Prepare a README (build and run steps) and a DESIGNDOC that will be filled as features are implemented, with one section per requirement per the submission guideline.


### Phase 1: Minimal X11 terminal window

- What to build: open an X11 display, create/map a window, handle events, maintain an in-memory text buffer, and draw text using XDrawString; echo keystrokes into the buffer and repaint on KeyPress.
- How it works: XOpenDisplay connects to the X server; XCreateSimpleWindow allocates a top-level window; XMapWindow shows it; XNextEvent yields events like KeyPress; a buffer mirrors what should be on-screen and is painted every expose/input.


### Phase 2: One shell process wired via pipes

- What to build: fork a child that execvp("/bin/sh","-c",...) or directly exec external commands; create pipes to feed child stdin and capture child stdout/stderr; render all child output inside the X11 window; implement a simple prompt and input line.
- How it works: pipe makes two FDs per stream; in the child, dup2 the read end to fd 0 (stdin) and the write end to fd 1/2 (stdout/stderr), then execvp; in the parent, close the child ends and read from pipes while collecting GUI input and writing to child stdin; this satisfies “run external command.”


### Phase 3: Built-ins and working directory

- What to build: parse the input line into argv tokens and detect built-ins: cd, exit, history (stub); run built-ins in the parent without exec; run everything else via fork/exec; keep current working directory state.
- How it works: cd requires chdir in the parent since a child’s directory change would not affect the shell; execvp is used for ls, gcc, ./a.out; parent waits or continues depending on foreground/background rules to be added later.


### Phase 4: Multiline Unicode input

- What to build: allow literal newlines in the input area and correct rendering of UTF-8, including the sample multilingual echo; use setlocale and handle read/write as byte streams; implement soft wrap or accept explicit newlines.
- How it works: setlocale enables multibyte handling; the GUI text buffer stores UTF-8; write system call sends bytes to child; X11 draw uses the buffer; test with the provided echo example to validate correctness.


### Phase 5: I/O redirection (<, >, both)

- What to build: extend parser to detect < infile and > outfile anywhere in the command; open files accordingly; before exec in the child, dup2 opened FDs to stdin/stdout; support combined input and output redirection.
- How it works: per Maurice Bach style, close(0); dup(fd_in) redirects stdin; similarly for stdout; after dup2, execvp runs the program with redirected streams; verify with ./a.out < infile.txt > outfile.txt and ls > abc.


### Phase 6: Pipelines with N stages

- What to build: parse cmd1 | cmd2 | ... | cmdN; create N-1 pipes; fork N children; wire stdout of i to pipe[i] write end and stdin of i+1 to pipe[i] read end via dup2; close unused ends in each process; run all in parallel; collect final output into the GUI. 
- How it works: the parent constructs the pipeline graph, each child does its own dup2, then execvp; this generalizes the ls *.txt | wc -l and cat | sort | more examples; parent can wait for all PIDs to finish or stream output live. 


### Phase 7: Line editing Ctrl+A and Ctrl+E

- What to build: capture raw keypresses for the input line to support moving the cursor to start and end; maintain a cursor index within the input buffer and redraw appropriately using ANSI movements or direct GUI cursor logic.
- How it works: detect ASCII 0x01 (Ctrl+A) and 0x05 (Ctrl+E); in a TTY, termios raw mode is used; here, X11 delivers KeyPress events, but mimic the semantics by updating the cursor index and redrawing the line region; improves terminal feel per requirement.


### Phase 8: Signals, Ctrl+C and Ctrl+Z

- What to build: ensure Ctrl+C interrupts the foreground child process but not the shell; ensure Ctrl+Z stops the foreground child and returns control (optionally track it as a background job); reprint the prompt after signals.
- How it works: place child into its own process group and send SIGINT/SIGTSTP to that group; the shell must not terminate on SIGINT; after interruption or stop, the GUI input resumes and the shell stays alive; this matches the spec’s behavior.


### Phase 9: Searchable history and history command

- What to build: persist up to 10,000 commands; implement history to show most recent 1,000; implement Ctrl+R to prompt for a search term, then return the most recent exact match; if none, compute the command(s) with the largest longest-substring match length > 2, else print “No match…”; integrate with GUI prompts.
- How it works: maintain an in-memory deque and optionally a file like bash; implement substring scoring and recency tie-break; on Ctrl+R, switch input mode to a mini-prompt, accept a string, perform search, display results back into the terminal buffer.


### Phase 10: Tabs with multiple shell instances

- What to build: add a tab bar and ability to create/switch tabs; each tab owns an independent child shell process, its own stdin/stdout pipes, and its own GUI buffers; only the active tab receives keystrokes and paints incoming output; allow closing tabs cleanly.
- How it works: the GUI tracks a list of tab contexts; switching tabs changes the active context and file descriptors polled for output; aligns with the requirement that each tab is a separate shell instance started via fork+exec.


### Phase 11: Autocomplete for filenames on Tab

- What to build: on Tab, scan the working directory for entries matching the current token prefix; if one match, complete to full filename; if multiple, extend to the longest common prefix and, if still ambiguous, list options with numbered choices and await a selection; if none, do nothing.
- How it works: parse caret location to find the active token; use directory listing to gather candidates; compute the longest common prefix; integrate with the GUI to render the disambiguation list inline and then insert the chosen completion while preserving the rest of the command line.


### Phase 12: multiWatch command

- What to build: implement multiWatch ["cmd1","cmd2",...] as a shell built-in that runs commands in parallel processes, streams their outputs interleaved with headers containing the command name and current time, and exits cleanly on Ctrl+C, killing children and deleting temp files; enforce the exact output format.
- How it works: for each command, fork/exec and redirect its stdout/stderr to a hidden temp file named .temp.PID.txt; the parent opens these files and uses select/poll on their descriptors; whenever data is available, read and print a block with the header and separators; on SIGINT, terminate all children, close FDs, and unlink the temp files.


## These above steps and their phases assisted me in building the project.

