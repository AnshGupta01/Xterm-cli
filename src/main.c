#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <signal.h>
#include "gui/gui.h"
#include "shell/shell.h"
#include "features/features.h"

GUI* g_gui = NULL;

int main(void) {
    setlocale(LC_ALL, "");
    
    GUI gui;
    g_gui = &gui;
    
    if (gui_init(&gui) != 0) {
        fprintf(stderr, "Failed to initialize GUI\n");
        return 1;
    }
    
    if (shell_init() != 0) {
        fprintf(stderr, "Failed to initialize shell\n");
        gui_cleanup(&gui);
        return 1;
    }
    
    history_init();
    setup_signal_handlers();
    
    // Create initial tab
    Tab* initial_tab = tab_new(&gui);
    if (!initial_tab) {
        fprintf(stderr, "Failed to create initial tab\n");
        shell_cleanup();
        gui_cleanup(&gui);
        return 1;
    }
    
    // Run main GUI loop
    gui_run(&gui);
    
    // Cleanup
    multiwatch_cleanup();
    history_save();
    shell_cleanup();
    gui_cleanup(&gui);
    
    return 0;
}