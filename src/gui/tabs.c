#include "gui.h"
#include "../shell/shell.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include<stdio.h>

Tab* tab_create(int id, const char* title) {
    Tab* tab = malloc(sizeof(Tab));
    if (!tab) return NULL;
    
    memset(tab, 0, sizeof(Tab));
    tab->id = id;
    strncpy(tab->title, title, sizeof(tab->title) - 1);
    
    return tab;
}

void tab_destroy(Tab* tab) {
    if (!tab) return;
    
    if (tab->shell_pid > 0) {
        kill(tab->shell_pid, SIGTERM);
        waitpid(tab->shell_pid, NULL, 0);
    }
    
    if (tab->shell_in_fd > 0) close(tab->shell_in_fd);
    if (tab->shell_out_fd > 0) close(tab->shell_out_fd);
    if (tab->shell_err_fd > 0) close(tab->shell_err_fd);
    
    free(tab);
}

void tab_switch(GUI* gui, int tab_id) {
    Tab* tab = gui->tabs;
    while (tab) {
        if (tab->id == tab_id) {
            gui->current_tab = tab;
            gui_draw_text(gui);
            gui_draw_input(gui);
            gui_draw_tabs(gui);
            return;
        }
        tab = tab->next;
    }
}

Tab* tab_new(GUI* gui) {
    // Find next available ID
    int next_id = 0;
    Tab* tab = gui->tabs;
    while (tab) {
        if (tab->id >= next_id) next_id = tab->id + 1;
        tab = tab->next;
    }
    
    char title[64];
    snprintf(title, sizeof(title), "Shell %d", next_id + 1);
    
    Tab* new_tab = tab_create(next_id, title);
    if (!new_tab) return NULL;
    
    // Create shell process
    ShellProcess* proc = shell_create_process();
    if (!proc) {
        tab_destroy(new_tab);
        return NULL;
    }
    
    new_tab->shell_pid = proc->pid;
    new_tab->shell_in_fd = proc->in_fd;
    new_tab->shell_out_fd = proc->out_fd;
    new_tab->shell_err_fd = proc->err_fd;
    
    // Add to tab list
    if (!gui->tabs) {
        gui->tabs = new_tab;
    } else {
        Tab* last = gui->tabs;
        while (last->next) last = last->next;
        last->next = new_tab;
    }
    
    gui->tab_count++;
    gui->current_tab = new_tab;
    
    return new_tab;
}

void tab_close(GUI* gui, int tab_id) {
    if (gui->tab_count <= 1) return; // Keep at least one tab
    
    Tab* prev = NULL;
    Tab* tab = gui->tabs;
    
    while (tab) {
        if (tab->id == tab_id) {
            if (prev) {
                prev->next = tab->next;
            } else {
                gui->tabs = tab->next;
            }
            
            if (gui->current_tab == tab) {
                gui->current_tab = gui->tabs;
            }
            
            tab_destroy(tab);
            gui->tab_count--;
            
            gui_draw_tabs(gui);
            return;
        }
        prev = tab;
        tab = tab->next;
    }
}