#include "gpad.h"

// Add file to recent manager
void add_to_recent_files(const char *filename) {
    if (!recent_manager || !filename || !g_path_is_absolute(filename)) return;

    char *uri = g_filename_to_uri(filename, NULL, NULL);
    if (uri) {
        gtk_recent_manager_add_item(recent_manager, uri);
        g_free(uri);
    }
}

// Get language type based on filename
LanguageType get_language_from_filename(const char *filename) {
    if (!filename) return LANG_UNKNOWN;

    if (g_str_has_suffix(filename, ".c") || g_str_has_suffix(filename, ".h")) {
        return LANG_C;
    }
    if (g_str_has_suffix(filename, ".py")) {
        return LANG_PYTHON;
    }
    if (g_str_has_suffix(filename, ".dart")) {
        return LANG_DART;
    }
    return LANG_UNKNOWN;
}

// Save tab content to file
static void save_tab_content(TabInfo *tab_info) {
    if (!tab_info->filename) return;

    FILE *f = fopen(tab_info->filename, "w");
    if (f) {
        GtkTextIter start, end;
        gtk_text_buffer_get_bounds(tab_info->buffer, &start, &end);
        char *text = gtk_text_buffer_get_text(tab_info->buffer, &start, &end, FALSE);
        fputs(text, f);
        fclose(f);
        g_free(text);

        tab_info->dirty = FALSE;
        update_tab_label(tab_info);
        add_to_recent_files(tab_info->filename);
        g_print("Saved file: %s\n", tab_info->filename);
    } else {
        g_warning("Failed to save file: %s", tab_info->filename);
    }
}

// Save as dialog finish callback
static void save_as_finish(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    TabInfo *tab_info = (TabInfo*)user_data;
    GError *error = NULL;
    GFile *file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(source_object), res, &error);

    if (file) {
        g_free(tab_info->filename);
        tab_info->filename = g_file_get_path(file);
        tab_info->lang_type = get_language_from_filename(tab_info->filename);
        update_tab_label(tab_info);
        save_tab_content(tab_info);
        g_object_unref(file);
    } else if (error) {
        g_warning("Save dialog error: %s", error->message);
        g_error_free(error);
    }
}

// Save current tab
void save_current_tab(void) {
    TabInfo *tab_info = get_current_tab_info();
    if (!tab_info) return;

    if (tab_info->filename) {
        save_tab_content(tab_info);
    } else {
        GtkFileDialog *dialog = gtk_file_dialog_new();
        gtk_file_dialog_set_title(dialog, "Save File");
        gtk_file_dialog_save(dialog, GTK_WINDOW(global_window), NULL, save_as_finish, tab_info);
        g_object_unref(dialog);
    }
}

// Open file dialog finish callback
// Open file dialog finish callback
static void open_finish(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    (void)user_data;  // Suppress unused parameter warning

    GError *error = NULL;
    GFile *file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source_object), res, &error);

    if (file) {
        char *filename = g_file_get_path(file);
        create_new_tab(filename);
        g_free(filename);
        g_object_unref(file);
    } else if (error) {
        g_warning("Open dialog error: %s", error->message);
        g_error_free(error);
    }
}
// Open file dialog
void open_file_dialog(void) {
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Open File");
    gtk_file_dialog_open(dialog, GTK_WINDOW(global_window), NULL, open_finish, NULL);
    g_object_unref(dialog);
}
