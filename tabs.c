#include "gpad.h"

// Signal handler for text buffer changes
static void on_buffer_changed(GtkTextBuffer *buffer, gpointer user_data) {
    TabInfo *tab_info = (TabInfo*)user_data;
    if (!tab_info->dirty) {
        tab_info->dirty = TRUE;
        update_tab_label(tab_info);
    }

    // Use timeout for debounced highlighting (works with or without tree-sitter)
    static guint source_id = 0;
    if (source_id > 0) g_source_remove(source_id);
    source_id = g_timeout_add(150, highlight_timeout_callback, tab_info);
}

// Tab close button callback
static void on_tab_close_button_clicked(GtkButton *button, gpointer user_data) {
    (void)button; // Suppress unused parameter warning
    (void)user_data; // Suppress unused parameter warning
    close_current_tab();
}

// Update tab label to show dirty state
void update_tab_label(TabInfo *tab_info) {
    if (!global_notebook || !tab_info) return;

    GtkWidget *tab_page = tab_info->scrolled_window;
    GtkWidget *label_box = gtk_notebook_get_tab_label(global_notebook, tab_page);
    if (!label_box) return;

    GtkWidget *label = gtk_widget_get_first_child(label_box);
    if (!label) return;

    const char *basename = tab_info->filename ? g_path_get_basename(tab_info->filename) : "Untitled";
    char *markup;

    if (tab_info->dirty) {
        markup = g_strdup_printf("<i>%s*</i>", basename);
    } else {
        markup = g_strdup(basename);
    }

    gtk_label_set_markup(GTK_LABEL(label), markup);
    g_free(markup);

    if (tab_info->filename) {
        g_free((char*)basename);
    }
}

// Setup syntax highlighting tags
void setup_highlighting_tags(GtkTextBuffer *buffer) {
    gtk_text_buffer_create_tag(buffer, "comment", "foreground", "#6A9955", "style", PANGO_STYLE_ITALIC, NULL);
    gtk_text_buffer_create_tag(buffer, "string", "foreground", "#CE9178", NULL);
    gtk_text_buffer_create_tag(buffer, "preproc", "foreground", "#9B9B9B", NULL);
    gtk_text_buffer_create_tag(buffer, "keyword", "foreground", "#569CD6", "weight", PANGO_WEIGHT_BOLD, NULL);
    gtk_text_buffer_create_tag(buffer, "control", "foreground", "#C586C0", "weight", PANGO_WEIGHT_BOLD, NULL);
    gtk_text_buffer_create_tag(buffer, "type", "foreground", "#4EC9B0", NULL);
    gtk_text_buffer_create_tag(buffer, "number", "foreground", "#B5CEA8", NULL);
    gtk_text_buffer_create_tag(buffer, "function", "foreground", "#DCDCAA", NULL);
    gtk_text_buffer_create_tag(buffer, "constant", "foreground", "#4FC1FF", NULL);
    gtk_text_buffer_create_tag(buffer, "decorator", "foreground", "#B5CEA8", "style", PANGO_STYLE_ITALIC, NULL);
}

// Internal function to create tab (common logic)
static void create_tab_internal(const char *filename, gboolean hide_sidebar) {
    if (!global_notebook) {
        g_warning("Cannot create tab: notebook not initialized yet");
        return;
    }

    // Check if file is already open
    if (filename) {
        for (int i = 0; i < gtk_notebook_get_n_pages(global_notebook); ++i) {
            GtkWidget *page = gtk_notebook_get_nth_page(global_notebook, i);
            TabInfo *info = (TabInfo*)g_object_get_data(G_OBJECT(page), "tab_info");
            if (info && info->filename && strcmp(info->filename, filename) == 0) {
                gtk_notebook_set_current_page(global_notebook, i);
                return;
            }
        }
    }

    // Only hide panels if requested (not from sidebar)
    if (hide_sidebar) {
        hide_panels();
        set_sidebar_visible(FALSE);
    }

    // Create UI elements
    GtkWidget *scrolled_window = gtk_scrolled_window_new();
    GtkWidget *text_view = gtk_text_view_new();
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));

    gtk_text_view_set_monospace(GTK_TEXT_VIEW(text_view), TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), text_view);

    // Create tab label with close button
    GtkWidget *tab_label_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    const char *display_name = filename ? g_path_get_basename(filename) : "Untitled";
    GtkWidget *tab_label = gtk_label_new(display_name);
    GtkWidget *close_button = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_button_set_has_frame(GTK_BUTTON(close_button), FALSE);

    gtk_box_append(GTK_BOX(tab_label_box), tab_label);
    gtk_box_append(GTK_BOX(tab_label_box), close_button);

    // Create tab info structure
    TabInfo *tab_info = g_new0(TabInfo, 1);
    tab_info->scrolled_window = scrolled_window;
    tab_info->text_view = text_view;
    tab_info->buffer = buffer;
    tab_info->filename = filename ? g_strdup(filename) : NULL;
    tab_info->dirty = FALSE;
    tab_info->lang_type = get_language_from_filename(filename);
    tab_info->ts_tree = NULL;  // Initialize to NULL regardless of tree-sitter availability

    g_object_set_data_full(G_OBJECT(scrolled_window), "tab_info", tab_info, g_free);

    // Connect signals
    g_signal_connect(buffer, "changed", G_CALLBACK(on_buffer_changed), tab_info);
    g_signal_connect(close_button, "clicked", G_CALLBACK(on_tab_close_button_clicked), NULL);

    // Add to notebook
    gtk_notebook_append_page(global_notebook, scrolled_window, tab_label_box);
    gtk_notebook_set_current_page(global_notebook, gtk_notebook_get_n_pages(global_notebook) - 1);

    // Setup syntax highlighting
    setup_highlighting_tags(buffer);

    // Load file content if filename provided
    if (filename) {
        gchar *contents;
        GError *error = NULL;
        if (g_file_get_contents(filename, &contents, NULL, &error)) {
            g_signal_handlers_block_by_func(buffer, G_CALLBACK(on_buffer_changed), tab_info);
            gtk_text_buffer_set_text(buffer, contents, -1);
            g_signal_handlers_unblock_by_func(buffer, G_CALLBACK(on_buffer_changed), tab_info);

            // Trigger initial highlighting (works with or without tree-sitter)
            highlight_buffer_sync(buffer, &tab_info->ts_tree, tab_info->lang_type);

            add_to_recent_files(filename);
            g_free(contents);
            g_print("Opened file: %s\n", filename);
        } else {
            g_warning("Failed to load file %s: %s", filename, error ? error->message : "Unknown error");
            if (error) g_error_free(error);
        }

        if (filename != display_name) {
            g_free((char*)display_name);
        }
    }

    gtk_widget_grab_focus(text_view);
}

// Create new tab (hides sidebar - for Ctrl+N, file dialogs, etc.)
void create_new_tab(const char *filename) {
    create_tab_internal(filename, TRUE);  // Hide sidebar
}

// Create new tab from sidebar (keeps sidebar open)
void create_new_tab_from_sidebar(const char *filename) {
    create_tab_internal(filename, FALSE);  // Keep sidebar open
}

// Get current tab info
TabInfo* get_current_tab_info(void) {
    if (!global_notebook) return NULL;

    gint page_num = gtk_notebook_get_current_page(global_notebook);
    if (page_num < 0) return NULL;

    GtkWidget *current_page = gtk_notebook_get_nth_page(global_notebook, page_num);
    return (TabInfo*)g_object_get_data(G_OBJECT(current_page), "tab_info");
}

// Close confirmation dialog callback
static void on_confirm_close_response(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    gint choice = gtk_alert_dialog_choose_finish(GTK_ALERT_DIALOG(source_object), res, &error);
    TabInfo *tab_info = (TabInfo*)user_data;

    if (error) {
        g_warning("Alert dialog error: %s", error->message);
        g_error_free(error);
        return;
    }

    if (choice == 1) { // Save
        save_current_tab();
    } else if (choice == 2) { // Close without Saving
        tab_info->dirty = FALSE;
        close_current_tab();
    }
}

// Close current tab
gboolean close_current_tab(void) {
    TabInfo *tab_info = get_current_tab_info();
    if (!tab_info || !global_notebook) return FALSE;

    if (tab_info->dirty) {
        const char *display_name = tab_info->filename ? g_path_get_basename(tab_info->filename) : "Untitled";
        GtkAlertDialog *dialog = gtk_alert_dialog_new("Save changes to \"%s\" before closing?", display_name);
        gtk_alert_dialog_set_detail(dialog, "Your changes will be lost if you don't save them.");

        const char* buttons[] = {"Cancel", "_Save", "Close without Saving", NULL};
        gtk_alert_dialog_set_buttons(dialog, buttons);
        gtk_alert_dialog_set_default_button(dialog, 1);
        gtk_alert_dialog_set_cancel_button(dialog, 0);

        gtk_alert_dialog_choose(dialog, GTK_WINDOW(global_window), NULL, on_confirm_close_response, tab_info);

        if (tab_info->filename && display_name != tab_info->filename) {
            g_free((char*)display_name);
        }
        return TRUE;
    }

    // Close tab immediately
    gint page_num = gtk_notebook_get_current_page(global_notebook);
    gtk_notebook_remove_page(global_notebook, page_num);

#ifdef HAVE_TREE_SITTER
    if (tab_info->ts_tree) {
        TSTree *actual_tree = (TSTree*)tab_info->ts_tree;
        ts_tree_delete(actual_tree);
    }
#endif

    // Create new empty tab if no tabs left
    if (gtk_notebook_get_n_pages(global_notebook) == 0) {
        create_new_tab(NULL);
    }

    return FALSE;
}
