#ifndef FEATURES_H
#define FEATURES_H

#include "../gui/gui.h"

#define MAX_HISTORY 10000
#define DISPLAY_HISTORY 1000

// History management
void history_init(void);
void history_add(const char* command);
char* history_search(const char* pattern);
void history_save(void);
void history_load(void);
int history_builtin(char** args);

// Autocomplete
char** autocomplete_filename(const char* prefix, int* count);
void autocomplete_free(char** matches, int count);
int handle_tab_completion(GUI* gui);

// MultiWatch
int multiwatch_builtin(char** args);
void multiwatch_cleanup(void);

#endif
