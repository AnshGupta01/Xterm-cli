#include "../include/myterm.h"

// Global terminal instance for signal handling
Terminal *g_terminal = NULL;

int main() {
    // Set up locale for Unicode support
    setup_locale();

    // Create terminal instance
    g_terminal = create_terminal();
    if (!g_terminal) {
        fprintf(stderr, "Failed to create terminal\n");
        return 1;
    }

    // Load command history from file
    load_history_from_file(g_terminal);

    // Initialize X11 GUI
    init_x11(g_terminal);

    // Setup signal handlers
    setup_signal_handlers(g_terminal);

    // Create initial tab
    create_new_tab(g_terminal);

    printf("MyTerm initialized successfully. Use XQuartz for display.\n");
    printf("Starting main event loop...\n");

    // Main event loop
    handle_x11_events(g_terminal);

    // Cleanup
    save_history_to_file(g_terminal);
    cleanup_terminal(g_terminal);
    return 0;
}
