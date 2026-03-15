#ifndef SEARCH_H
#define SEARCH_H

#include <gtk/gtk.h>

/** Initializes search UI components. */
GtkWidget* init_search_ui(void);
/** Toggles search bar visibility. */
void toggle_search_bar(void);
/** Performs matching and highlighting for search term. */
void perform_search(const char *text);

#endif
