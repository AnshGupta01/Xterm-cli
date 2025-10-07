#include "xterm.h"

// Global pointer for signal handling.
Terminal *g_terminal = NULL;

int main() {
    setlocale(LC_ALL, "");

    printf("Initializing Xterm...\n");

    Terminal *term_instance = create_new_terminal();
    if (term_instance == NULL) {
        fprintf(stderr, "Fatal: Could not allocate memory for terminal state.\n");
        return EXIT_FAILURE;
    }
    g_terminal = term_instance;

    setup_signal_handlers();
    setup_x11_gui(g_terminal);

    // first tab.
    {
        const int initial_tab_index = 0;
        initialize_new_tab(g_terminal, initial_tab_index);
        g_terminal->current_tab_index = initial_tab_index;
        g_terminal->total_tabs = 1;
    }

    printf("Terminal is ready.\n");

    // This function blocks until the user closes the window.
    handle_x11_events(g_terminal);

    printf("Exiting.\n");
    cleanup_terminal_resources(g_terminal);

    return EXIT_SUCCESS;
}