#include "../include/myterm.h"

void add_to_history(Terminal *term, char *command) {
    if (strlen(command) == 0) return;

    // Don't add duplicate consecutive commands
    if (term->history_count > 0 && 
        strcmp(term->history[term->history_count - 1], command) == 0) {
        return;
    }

    // Add to history array
    if (term->history_count < MAX_HISTORY) {
        safe_strcpy(term->history[term->history_count], command, MAX_COMMAND_LENGTH);
        term->history_count++;
    } else {
        // Shift history array left and add new command at end
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            strcpy(term->history[i], term->history[i + 1]);
        }
        safe_strcpy(term->history[MAX_HISTORY - 1], command, MAX_COMMAND_LENGTH);
    }
}

void search_history(Terminal *term, char *search_term) {
    if (strlen(search_term) == 0) {
        append_to_buffer(term, "No search term provided\n");
        return;
    }

    Tab *tab = &term->tabs[term->current_tab];
    int found = 0;
    int best_match_idx = -1;

    // Search for exact matches first
    for (int i = term->history_count - 1; i >= 0; i--) {
        if (strcmp(term->history[i], search_term) == 0) {
            // Exact match found
            char result[MAX_COMMAND_LENGTH + 32];
            snprintf(result, sizeof(result), "Found: %s\n", term->history[i]);
            append_to_buffer(term, result);

            // Copy to current line
            snprintf(tab->current_line, sizeof(tab->current_line), "%s%s", PROMPT, term->history[i]);
            tab->cursor_pos = strlen(tab->current_line);
            found = 1;
            break;
        }
    }

    if (!found) {
        // Look for partial matches
        for (int i = term->history_count - 1; i >= 0; i--) {
            char *match = strstr(term->history[i], search_term);
            if (match) {
                best_match_idx = i;
                break;
            }
        }

        if (best_match_idx != -1) {
            char result[MAX_COMMAND_LENGTH + 32];
            snprintf(result, sizeof(result), "Partial match: %s\n", term->history[best_match_idx]);
            append_to_buffer(term, result);

            // Copy to current line
            snprintf(tab->current_line, sizeof(tab->current_line), "%s%s", PROMPT, term->history[best_match_idx]);
            tab->cursor_pos = strlen(tab->current_line);
        } else {
            append_to_buffer(term, "No match for search term in history\n");
        }
    }
}

void load_history_from_file(Terminal *term) {
    FILE *history_file = fopen(".myterm_history", "r");
    if (!history_file) return;

    char line[MAX_COMMAND_LENGTH];
    while (fgets(line, sizeof(line), history_file) && term->history_count < MAX_HISTORY) {
        // Remove newline
        line[strcspn(line, "\n")] = '\0';

        if (strlen(line) > 0) {
            safe_strcpy(term->history[term->history_count], line, MAX_COMMAND_LENGTH);
            term->history_count++;
        }
    }

    fclose(history_file);
}

void save_history_to_file(Terminal *term) {
    FILE *history_file = fopen(".myterm_history", "w");
    if (!history_file) return;

    for (int i = 0; i < term->history_count; i++) {
        fprintf(history_file, "%s\n", term->history[i]);
    }

    fclose(history_file);
}
