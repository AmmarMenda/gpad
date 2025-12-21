#ifndef SEARCH_H
#define SEARCH_H

#include <gtk/gtk.h>

GtkWidget* init_search_ui(void);
void toggle_search_bar(void);
void perform_search(const char *text);

#endif
