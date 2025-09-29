#include "features.h"
#include "../gui/gui.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static char history[MAX_HISTORY][MAX_LINE];
static int history_count = 0;
static int history_pos = 0;

void history_init(void) {
    memset(history, 0, sizeof(history));
    history_load();
}

void history_add(const char* command) {
    if (!command || !*command) return;
    
    // Don't add duplicate of last command
    if (history_count > 0 && strcmp(history[history_pos], command) == 0) {
        return;
    }
    
    history_pos = (history_pos + 1) % MAX_HISTORY;
    strncpy(history[history_pos], command, MAX_LINE - 1);
    history[history_pos][MAX_LINE - 1] = '\0';
    
    if (history_count < MAX_HISTORY) {
        history_count++;
    }
}

char* history_search(const char* pattern) {
    if (!pattern || !*pattern) return NULL;
    
    int start = history_pos;
    int best_match = -1;
    int best_score = 0;
    
    for (int i = 0; i < history_count; i++) {
        int idx = (start - i + MAX_HISTORY) % MAX_HISTORY;
        
        // Exact match
        if (strstr(history[idx], pattern)) {
            return history[idx];
        }
        
        // Substring match scoring
        char* p = history[idx];
        char* q = (char*)pattern;
        int score = 0;
        int consecutive = 0;
        
        while (*p && *q) {
            if (*p == *q) {
                consecutive++;
                score += consecutive;
                q++;
            } else {
                consecutive = 0;
            }
            p++;
        }
        
        if (score > 2 && score > best_score) {
            best_score = score;
            best_match = idx;
        }
    }
    
    return best_match >= 0 ? history[best_match] : NULL;
}

void history_save(void) {
    FILE* f = fopen(".xterm_history", "w");
    if (!f) return;
    
    int start = (history_pos - history_count + 1 + MAX_HISTORY) % MAX_HISTORY;
    for (int i = 0; i < history_count; i++) {
        int idx = (start + i) % MAX_HISTORY;
        fprintf(f, "%s\n", history[idx]);
    }
    
    fclose(f);
}

void history_load(void) {
    FILE* f = fopen(".xterm_history", "r");
    if (!f) return;
    
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f) && history_count < MAX_HISTORY) {
        // Remove newline
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        
        if (*line) {
            history_add(line);
        }
    }
    
    fclose(f);
}

int history_builtin(char** args) {
    int count = DISPLAY_HISTORY;
    if (count > history_count) count = history_count;
    
    int start = (history_pos - count + 1 + MAX_HISTORY) % MAX_HISTORY;
    
    for (int i = 0; i < count; i++) {
        int idx = (start + i) % MAX_HISTORY;
        char line[64];
        snprintf(line, sizeof(line), "%4d  %s\n", i + 1, history[idx]);
        
        if (g_gui) {
            gui_append_text(g_gui, line);
        }
    }
    
    return 0;
}
