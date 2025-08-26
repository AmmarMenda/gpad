#include "gpad.h"

// Recent file item click handler
static void on_recent_file_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data) {
    const char *filename = (const char *)g_object_get_data(G_OBJECT(row), "filename");
    if (filename) {
        // Use the new function that keeps sidebar open
        create_new_tab_from_sidebar(filename);
    }
}

// Populate recent files list
void populate_recent_files(void) {
    if (!recent_list_box || !recent_manager) return;

    // Clear existing items
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(GTK_WIDGET(recent_list_box))) != NULL) {
        gtk_list_box_remove(recent_list_box, child);
    }

    GList *items = gtk_recent_manager_get_items(recent_manager);
    int count = 0;

    for (GList *l = items; l && count < 15; l = l->next) {
        GtkRecentInfo *info = (GtkRecentInfo *)l->data;
        const char *uri = gtk_recent_info_get_uri(info);
        char *filename = g_filename_from_uri(uri, NULL, NULL);

        if (filename && g_file_test(filename, G_FILE_TEST_EXISTS)) {
            GtkWidget *row = gtk_list_box_row_new();
            char *basename = g_path_get_basename(filename);
            GtkWidget *label = gtk_label_new(basename);
            gtk_label_set_xalign(GTK_LABEL(label), 0.0);
            gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_MIDDLE);

            gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
            g_object_set_data_full(G_OBJECT(row), "filename", g_strdup(filename), g_free);
            gtk_list_box_append(recent_list_box, row);

            g_free(basename);
            count++;
        }
        g_free(filename);
    }

    g_list_free_full(items, (GDestroyNotify)gtk_recent_info_unref);

    if (count == 0) {
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label = gtk_label_new("No recent files");
        gtk_widget_set_sensitive(row, FALSE);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        gtk_list_box_append(recent_list_box, row);
    }
}

// Create recent files panel
GtkWidget* create_recent_files_panel(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(box, 10);
    gtk_widget_set_margin_end(box, 10);
    gtk_widget_set_margin_top(box, 10);
    gtk_widget_set_margin_bottom(box, 10);

    GtkWidget *header = gtk_label_new("<b>Recent Files</b>");
    gtk_label_set_use_markup(GTK_LABEL(header), TRUE);
    gtk_label_set_xalign(GTK_LABEL(header), 0.0);
    gtk_box_append(GTK_BOX(box), header);

    GtkWidget *subtitle = gtk_label_new("<small>Ctrl+R to toggle</small>");
    gtk_label_set_use_markup(GTK_LABEL(subtitle), TRUE);
    gtk_label_set_xalign(GTK_LABEL(subtitle), 0.0);
    gtk_widget_set_opacity(subtitle, 0.7);
    gtk_box_append(GTK_BOX(box), subtitle);

    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_box_append(GTK_BOX(box), scrolled);

    recent_list_box = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_selection_mode(recent_list_box, GTK_SELECTION_NONE);
    g_signal_connect(recent_list_box, "row-activated", G_CALLBACK(on_recent_file_activated), NULL);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), GTK_WIDGET(recent_list_box));

    return box;
}

// Show file browser panel
void show_file_browser_panel(void) {
    if (!panel_container || !side_panel) return;

    gtk_widget_set_visible(recent_panel, FALSE);
    gtk_widget_set_visible(side_panel, TRUE);
    gtk_widget_set_visible(panel_container, TRUE);
    g_print("Showing file browser panel\n");
}

// Show recent files panel
void show_recent_files_panel(void) {
    if (!panel_container || !recent_panel) return;

    populate_recent_files();
    gtk_widget_set_visible(side_panel, FALSE);
    gtk_widget_set_visible(recent_panel, TRUE);
    gtk_widget_set_visible(panel_container, TRUE);
    g_print("Showing recent files panel\n");
}

// Hide all side panels
void hide_panels(void) {
    if (panel_container) {
        gtk_widget_set_visible(panel_container, FALSE);
        g_print("Hiding all panels\n");
    }
}
