#include "features.h"
#include "../gui/gui.h"
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>  // for snprintf


char** autocomplete_filename(const char* prefix, int* count) {
    *count = 0;
    
    DIR* dir = opendir(".");
    if (!dir) return NULL;
    
    char** matches = malloc(256 * sizeof(char*));
    int capacity = 256;
    
    struct dirent* entry;
    while ((entry = readdir(dir))) {
        if (strncmp(entry->d_name, prefix, strlen(prefix)) == 0) {
            if (*count >= capacity - 1) {
                capacity *= 2;
                matches = realloc(matches, capacity * sizeof(char*));
            }
            
            matches[*count] = strdup(entry->d_name);
            (*count)++;
        }
    }
    
    closedir(dir);
    
    if (*count == 0) {
        free(matches);
        return NULL;
    }
    
    matches[*count] = NULL;
    return matches;
}

void autocomplete_free(char** matches, int count) {
    if (!matches) return;
    
    for (int i = 0; i < count; i++) {
        free(matches[i]);
    }
    free(matches);
}

static char* find_common_prefix(char** matches, int count) {
    if (count == 0) return NULL;
    if (count == 1) return strdup(matches[0]);
    
    int min_len = strlen(matches[0]);
    for (int i = 1; i < count; i++) {
        int len = strlen(matches[i]);
        if (len < min_len) min_len = len;
    }
    
    int common = 0;
    for (int i = 0; i < min_len; i++) {
        char c = matches[0][i];
        int same = 1;
        
        for (int j = 1; j < count; j++) {
            if (matches[j][i] != c) {
                same = 0;
                break;
            }
        }
        
        if (same) {
            common++;
        } else {
            break;
        }
    }
    
    if (common == 0) return NULL;
    
    char* prefix = malloc(common + 1);
    strncpy(prefix, matches[0], common);
    prefix[common] = '\0';
    
    return prefix;
}

int handle_tab_completion(GUI* gui) {
    if (!gui->current_tab) return 0;
    
    InputLine* input = &gui->current_tab->input_line;
    
    // Find current token
    char* line = input->input;
    int pos = input->cursor_pos;
    
    // Find start of current token
    int start = pos;
    while (start > 0 && line[start - 1] != ' ' && line[start - 1] != '\t') {
        start--;
    }
    
    // Find end of current token
    int end = pos;
    while (end < input->input_len && line[end] != ' ' && line[end] != '\t') {
        end++;
    }
    
    // Extract token
    int token_len = end - start;
    char* token = malloc(token_len + 1);
    strncpy(token, line + start, token_len);
    token[token_len] = '\0';
    
    // Get matches
    int count;
    char** matches = autocomplete_filename(token, &count);
    
    if (count == 0) {
        // No matches
        free(token);
        return 0;
    }
    
    if (count == 1) {
        // Single match - complete
        int completion_len = strlen(matches[0]);
        int insert_len = completion_len - token_len;
        
        if (input->input_len + insert_len < MAX_LINE - 1) {
            // Make space
            memmove(line + end + insert_len, line + end, input->input_len - end);
            
            // Insert completion
            memcpy(line + start, matches[0], completion_len);
            
            input->input_len += insert_len;
            input->cursor_pos = start + completion_len;
            input->input[input->input_len] = '\0';
            
            gui_draw_input(gui);
        }
    } else {
        // Multiple matches
        char* common = find_common_prefix(matches, count);
        
        if (common && strlen(common) > strlen(token)) {
            // Complete to common prefix
            int common_len = strlen(common);
            int insert_len = common_len - token_len;
            
            if (input->input_len + insert_len < MAX_LINE - 1) {
                memmove(line + end + insert_len, line + end, input->input_len - end);
                memcpy(line + start, common, common_len);
                
                input->input_len += insert_len;
                input->cursor_pos = start + common_len;
                input->input[input->input_len] = '\0';
                
                gui_draw_input(gui);
            }
        }
        
        // Show options
        gui_append_text(gui, "\nOptions:\n");
        for (int i = 0; i < count; i++) {
            char line_buf[128];
            snprintf(line_buf, sizeof(line_buf), "%2d) %s\n", i + 1, matches[i]);
            gui_append_text(gui, line_buf);
        }
        
        free(common);
    }
    
    free(token);
    autocomplete_free(matches, count);
    return 1;
}
