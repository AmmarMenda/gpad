#include "gpad.h"

// Create the welcome screen widget
GtkWidget* create_welcome_screen(void) {
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_halign(main_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(main_box, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start(main_box, 40);
    gtk_widget_set_margin_end(main_box, 40);
    gtk_widget_set_margin_top(main_box, 40);
    gtk_widget_set_margin_bottom(main_box, 40);
    
    // App title
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), 
        "<span size='xx-large' weight='bold'>GPad</span>\n"
        "<span size='large' alpha='70%'>Multi-Tab Text Editor</span>");
    gtk_label_set_justify(GTK_LABEL(title), GTK_JUSTIFY_CENTER);
    gtk_box_append(GTK_BOX(main_box), title);
    
    // Separator
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_margin_top(separator, 20);
    gtk_widget_set_margin_bottom(separator, 20);
    gtk_box_append(GTK_BOX(main_box), separator);
    
    // Shortcuts section
    GtkWidget *shortcuts_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(shortcuts_title), 
        "<span size='large' weight='bold'>Keyboard Shortcuts</span>");
    gtk_box_append(GTK_BOX(main_box), shortcuts_title);
    
    // Create shortcuts grid
    GtkWidget *shortcuts_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(shortcuts_grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(shortcuts_grid), 20);
    gtk_widget_set_halign(shortcuts_grid, GTK_ALIGN_CENTER);
    
    // Array of shortcuts
    struct {
        const char *shortcut;
        const char *description;
    } shortcuts[] = {
        {"Ctrl+N", "Create new file"},
        {"Ctrl+O", "Open file"},
        {"Ctrl+S", "Save file"},
        {"Ctrl+W", "Close tab"},
        {"Ctrl+Z", "Undo"},
        {"Ctrl+Y", "Redo"},
        {"Ctrl+B", "Toggle file browser"},
        {"Ctrl+R", "Toggle recent files"},
        {"Ctrl+Q", "Quit application"}
    };
    
    int num_shortcuts = sizeof(shortcuts) / sizeof(shortcuts[0]);
    
    for (int i = 0; i < num_shortcuts; i++) {
        // Shortcut key
        GtkWidget *key_label = gtk_label_new(NULL);
        char *key_markup = g_strdup_printf(
            "<span font='monospace' weight='bold' background='#3C3C3C' "
            "foreground='#FFFFFF' size='small'>  %s  </span>", 
            shortcuts[i].shortcut);
        gtk_label_set_markup(GTK_LABEL(key_label), key_markup);
        gtk_widget_set_halign(key_label, GTK_ALIGN_END);
        gtk_grid_attach(GTK_GRID(shortcuts_grid), key_label, 0, i, 1, 1);
        g_free(key_markup);
        
        // Description
        GtkWidget *desc_label = gtk_label_new(shortcuts[i].description);
        gtk_widget_set_halign(desc_label, GTK_ALIGN_START);
        gtk_widget_set_opacity(desc_label, 0.8);
        gtk_grid_attach(GTK_GRID(shortcuts_grid), desc_label, 1, i, 1, 1);
    }
    
    gtk_box_append(GTK_BOX(main_box), shortcuts_grid);
    
    // Footer
    GtkWidget *footer = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(footer), 
        "<span size='small' alpha='60%'>Press <b>Ctrl+N</b> to create a new file or <b>Ctrl+O</b> to open an existing file</span>");
    gtk_label_set_justify(GTK_LABEL(footer), GTK_JUSTIFY_CENTER);
    gtk_widget_set_margin_top(footer, 30);
    gtk_box_append(GTK_BOX(main_box), footer);
    
    return main_box;
}
