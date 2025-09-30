#include "xterm.h"

Terminal *g_terminal = NULL;

int main() {
    // Setup unicode support
    setlocale(LC_ALL, "");
    
    printf("Starting MyTerm - CS69201 Project\n");
    
    // Create terminal instance
    g_terminal = create_new_terminal();
    if (!g_terminal) {
        printf("Error: Failed to create terminal\n");
        return 1;
    }
    
    // Setup signal handlers for Ctrl+C, Ctrl+Z
    setup_signal_handlers();
    
    // Initialize X11 GUI
    setup_x11_gui(g_terminal);
    
    // Create first tab
    initialize_new_tab(g_terminal, 0);
    g_terminal->total_tabs = 1;
    g_terminal->current_tab_index = 0;
    
    printf("MyTerm initialized successfully\n");
    
    // Start main event loop
    handle_x11_events(g_terminal);
    
    // Cleanup and exit
    cleanup_terminal_resources(g_terminal);
    return 0;
}