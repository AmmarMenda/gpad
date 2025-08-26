#include <gtk/gtk.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

// Make tree-sitter optional to avoid linking issues
#ifdef HAVE_TREE_SITTER
#include <tree_sitter/api.h>
#endif

// Enum for supported languages
typedef enum {
    LANG_C,
    LANG_PYTHON,
    LANG_DART,
    LANG_UNKNOWN
} LanguageType;

// Data structure to hold information for each tab
typedef struct {
    GtkWidget *scrolled_window;
    GtkWidget *text_view;
    GtkTextBuffer *buffer;
    char *filename;
    gboolean dirty;
    LanguageType lang_type;
#ifdef HAVE_TREE_SITTER
    TSTree *ts_tree;
#endif
} TabInfo;

// Column enumeration for TreeView
enum {
    COLUMN_NAME,
    COLUMN_PATH,
    COLUMN_IS_DIR,
    N_COLUMNS
};

// Global references
static GtkWidget *global_window = NULL;
static GtkNotebook *global_notebook = NULL;
static GtkTreeView *file_tree_view = NULL;
static GtkTreeStore *file_tree_store = NULL;
static GtkWidget *side_panel = NULL;
static GtkWidget *recent_panel = NULL;
static GtkWidget *panel_container = NULL;
static GtkListBox *recent_list_box = NULL;
static char *current_directory = NULL;
static GtkRecentManager *recent_manager = NULL;
static gboolean app_initialized = FALSE;

#ifdef HAVE_TREE_SITTER
static TSParser *ts_parser = NULL;

// Forward declarations for grammar parsers
TSLanguage *tree_sitter_c(void);
TSLanguage *tree_sitter_python(void);
TSLanguage *tree_sitter_dart(void);
#endif

// Forward declarations for local functions
static void populate_file_tree(GtkTreeStore *store, GtkTreeIter *parent, const char *path);
static void show_recent_files_panel(void);
static void hide_panels(void);
static void refresh_file_tree(const char *directory);
static void show_file_browser_panel(void);
static void populate_recent_files(void);
static void on_buffer_changed(GtkTextBuffer *buffer, gpointer user_data);
static void create_new_tab(const char *filename);
static TabInfo* get_current_tab_info(void);
static gboolean close_current_tab(void);
static void save_current_tab(void);
static void initialize_application(GtkApplication *app);

#ifdef HAVE_TREE_SITTER
static void highlight_buffer_sync(GtkTextBuffer *buffer, TSTree **ts_tree, LanguageType lang);
static void apply_tags_recursive(TSNode node, GtkTextBuffer *buffer, const char *source_code, LanguageType lang);
#endif

// Add file to recent manager
static void add_to_recent_files(const char *filename) {
    if (!recent_manager || !filename || !g_path_is_absolute(filename)) return;

    char *uri = g_filename_to_uri(filename, NULL, NULL);
    if (uri) {
        gtk_recent_manager_add_item(recent_manager, uri);
        g_free(uri);
    }
}

// Get language type based on filename
static LanguageType get_language_from_filename(const char *filename) {
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

// Show file browser panel
static void show_file_browser_panel(void) {
    if (!panel_container || !side_panel) return;

    gtk_widget_set_visible(recent_panel, FALSE);
    gtk_widget_set_visible(side_panel, TRUE);
    gtk_widget_set_visible(panel_container, TRUE);
}

// Show recent files panel
static void show_recent_files_panel(void) {
    if (!panel_container || !recent_panel) return;

    populate_recent_files();
    gtk_widget_set_visible(side_panel, FALSE);
    gtk_widget_set_visible(recent_panel, TRUE);
    gtk_widget_set_visible(panel_container, TRUE);
}

// Hide all side panels
static void hide_panels(void) {
    if (panel_container) {
        gtk_widget_set_visible(panel_container, FALSE);
    }
}

// Update tab label to show dirty state
static void update_tab_label(TabInfo *tab_info) {
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
static void setup_highlighting_tags(GtkTextBuffer *buffer) {
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

#ifdef HAVE_TREE_SITTER
// Recursive function to traverse the syntax tree and apply tags
static void apply_tags_recursive(TSNode node, GtkTextBuffer *buffer, const char *source_code, LanguageType lang) {
    if (ts_node_is_null(node)) return;

    const char *type = ts_node_type(node);
    const char *tag_name = NULL;

    // Language-specific syntax highlighting
    if (lang == LANG_C) {
        if (strcmp(type, "comment") == 0) tag_name = "comment";
        else if (strcmp(type, "string_literal") == 0 || strcmp(type, "char_literal") == 0) tag_name = "string";
        else if (strstr(type, "preproc") != NULL) tag_name = "preproc";
        else if (strcmp(type, "return") == 0 || strcmp(type, "if") == 0 || strcmp(type, "for") == 0 ||
                strcmp(type, "while") == 0 || strcmp(type, "break") == 0 || strcmp(type, "case") == 0) tag_name = "control";
        else if (strcmp(type, "storage_class_specifier") == 0 || strcmp(type, "type_qualifier") == 0 ||
                strcmp(type, "struct") == 0 || strcmp(type, "typedef") == 0) tag_name = "keyword";
        else if (strcmp(type, "primitive_type") == 0 || strcmp(type, "type_identifier") == 0) tag_name = "type";
        else if (strcmp(type, "number_literal") == 0) tag_name = "number";
    } else if (lang == LANG_PYTHON) {
        if (strcmp(type, "comment") == 0) tag_name = "comment";
        else if (strcmp(type, "string") == 0) tag_name = "string";
        else if (strcmp(type, "from") == 0 || strcmp(type, "import") == 0 || strcmp(type, "as") == 0) tag_name = "preproc";
        else if (strcmp(type, "if") == 0 || strcmp(type, "for") == 0 || strcmp(type, "while") == 0 ||
                strcmp(type, "return") == 0 || strcmp(type, "in") == 0 || strcmp(type, "try") == 0 ||
                strcmp(type, "except") == 0) tag_name = "control";
        else if (strcmp(type, "def") == 0 || strcmp(type, "class") == 0 || strcmp(type, "pass") == 0) tag_name = "keyword";
        else if (strcmp(type, "type") == 0) tag_name = "type";
        else if (strcmp(type, "integer") == 0 || strcmp(type, "float") == 0) tag_name = "number";
        else if (strcmp(type, "decorator") == 0) tag_name = "decorator";
    } else if (lang == LANG_DART) {
        if (strcmp(type, "comment") == 0) tag_name = "comment";
        else if (strcmp(type, "string_literal") == 0) tag_name = "string";
        else if (strcmp(type, "import_directive") == 0 || strcmp(type, "export_directive") == 0) tag_name = "preproc";
        else if (strcmp(type, "if_statement") == 0 || strcmp(type, "for_statement") == 0 ||
                strcmp(type, "while_statement") == 0 || strcmp(type, "return_statement") == 0) tag_name = "control";
        else if (strcmp(type, "class_definition") == 0 || strcmp(type, "final") == 0 ||
                strcmp(type, "const") == 0 || strcmp(type, "static") == 0) tag_name = "keyword";
        else if (strcmp(type, "type_name") == 0 || strcmp(type, "primitive_type") == 0) tag_name = "type";
        else if (strcmp(type, "number_literal") == 0) tag_name = "number";
        else if (strcmp(type, "annotation") == 0) tag_name = "decorator";
    }

    if (tag_name) {
        uint32_t start_byte = ts_node_start_byte(node);
        uint32_t end_byte = ts_node_end_byte(node);

        GtkTextIter start_iter, end_iter;
        gtk_text_buffer_get_iter_at_offset(buffer, &start_iter, start_byte);
        gtk_text_buffer_get_iter_at_offset(buffer, &end_iter, end_byte);
        gtk_text_buffer_apply_tag_by_name(buffer, tag_name, &start_iter, &end_iter);
    }

    // Recursively process children
    for (uint32_t i = 0; i < ts_node_child_count(node); ++i) {
        apply_tags_recursive(ts_node_child(node, i), buffer, source_code, lang);
    }
}

// Main function to trigger highlighting
static void highlight_buffer_sync(GtkTextBuffer *buffer, TSTree **ts_tree, LanguageType lang) {
    if (lang == LANG_UNKNOWN || !ts_parser) return;

    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);

    if (gtk_text_iter_get_offset(&start) == gtk_text_iter_get_offset(&end)) return;

    char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

    // Remove existing tags
    gtk_text_buffer_remove_all_tags(buffer, &start, &end);

    // Set language for parser
    TSLanguage *ts_lang = NULL;
    if (lang == LANG_C) ts_lang = tree_sitter_c();
    else if (lang == LANG_PYTHON) ts_lang = tree_sitter_python();
    else if (lang == LANG_DART) ts_lang = tree_sitter_dart();

    if (ts_lang) {
        ts_parser_set_language(ts_parser, ts_lang);

        if (*ts_tree) ts_tree_delete(*ts_tree);
        *ts_tree = ts_parser_parse_string(ts_parser, NULL, text, strlen(text));

        if (*ts_tree) {
            TSNode root_node = ts_tree_root_node(*ts_tree);
            apply_tags_recursive(root_node, buffer, text, lang);
        }
    }

    g_free(text);
}

// Timeout callback for debounced highlighting
static gboolean highlight_timeout_callback(gpointer user_data) {
    TabInfo *tab_info = (TabInfo*)user_data;
    highlight_buffer_sync(tab_info->buffer, &tab_info->ts_tree, tab_info->lang_type);
    return G_SOURCE_REMOVE;
}
#endif

// Signal handler for text buffer changes
static void on_buffer_changed(GtkTextBuffer *buffer, gpointer user_data) {
    TabInfo *tab_info = (TabInfo*)user_data;

    if (!tab_info->dirty) {
        tab_info->dirty = TRUE;
        update_tab_label(tab_info);
    }

#ifdef HAVE_TREE_SITTER
    static guint source_id = 0;
    if (source_id > 0) g_source_remove(source_id);
    source_id = g_timeout_add(150, highlight_timeout_callback, tab_info);
#endif
}

// Recent file item click handler
static void on_recent_file_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data) {
    const char *filename = (const char *)g_object_get_data(G_OBJECT(row), "filename");
    if (filename) {
        create_new_tab(filename);
    }
}

// Populate recent files list
static void populate_recent_files(void) {
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
static GtkWidget* create_recent_files_panel(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    GtkWidget *header = gtk_label_new("<b>Recent Files</b>");
    gtk_label_set_use_markup(GTK_LABEL(header), TRUE);
    gtk_box_append(GTK_BOX(box), header);

    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_box_append(GTK_BOX(box), scrolled);

    recent_list_box = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_selection_mode(recent_list_box, GTK_SELECTION_NONE);
    g_signal_connect(recent_list_box, "row-activated", G_CALLBACK(on_recent_file_activated), NULL);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), GTK_WIDGET(recent_list_box));

    return box;
}

// Suppress deprecation warnings for tree view functions
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

// Populate the file tree
static void populate_file_tree(GtkTreeStore *store, GtkTreeIter *parent, const char *path) {
    DIR *dir = opendir(path);
    if (!dir) return;

    struct dirent *entry;
    GPtrArray *dirs = g_ptr_array_new_with_free_func(g_free);
    GPtrArray *files = g_ptr_array_new_with_free_func(g_free);

    while ((entry = readdir(dir))) {
        if (entry->d_name[0] == '.') continue;

        char *full_path = g_build_filename(path, entry->d_name, NULL);
        if (g_file_test(full_path, G_FILE_TEST_IS_DIR)) {
            g_ptr_array_add(dirs, g_strdup(entry->d_name));
        } else {
            g_ptr_array_add(files, g_strdup(entry->d_name));
        }
        g_free(full_path);
    }
    closedir(dir);

    g_ptr_array_sort(dirs, (GCompareFunc)strcmp);
    g_ptr_array_sort(files, (GCompareFunc)strcmp);

    // Add directories first
    for (guint i = 0; i < dirs->len; i++) {
        GtkTreeIter iter;
        char *name = (char *)dirs->pdata[i];
        char *full_path = g_build_filename(path, name, NULL);

        gtk_tree_store_append(store, &iter, parent);
        gtk_tree_store_set(store, &iter,
                          COLUMN_NAME, name,
                          COLUMN_PATH, full_path,
                          COLUMN_IS_DIR, TRUE, -1);

        // Add dummy child for lazy loading
        GtkTreeIter dummy_iter;
        gtk_tree_store_append(store, &dummy_iter, &iter);

        g_free(full_path);
    }

    // Add files
    for (guint i = 0; i < files->len; i++) {
        GtkTreeIter iter;
        char *name = (char *)files->pdata[i];
        char *full_path = g_build_filename(path, name, NULL);

        gtk_tree_store_append(store, &iter, parent);
        gtk_tree_store_set(store, &iter,
                          COLUMN_NAME, name,
                          COLUMN_PATH, full_path,
                          COLUMN_IS_DIR, FALSE, -1);

        g_free(full_path);
    }

    g_ptr_array_free(dirs, TRUE);
    g_ptr_array_free(files, TRUE);
}

// Handle tree row expansion
static void on_row_expanded(GtkTreeView *tree_view, GtkTreeIter *iter, GtkTreePath *path, gpointer user_data) {
    GtkTreeStore *store = GTK_TREE_STORE(gtk_tree_view_get_model(tree_view));
    GtkTreeModel *model = GTK_TREE_MODEL(store);

    GtkTreeIter child;
    if (gtk_tree_model_iter_children(model, &child, iter)) {
        gchar* name;
        gtk_tree_model_get(model, &child, COLUMN_NAME, &name, -1);

        if (name == NULL) {
            gtk_tree_store_remove(store, &child);
            gchar *dir_path;
            gtk_tree_model_get(model, iter, COLUMN_PATH, &dir_path, -1);
            populate_file_tree(store, iter, dir_path);
            g_free(dir_path);
        }
        g_free(name);
    }
}

// Handle file selection in tree
static void on_file_selected(GtkTreeSelection *selection, gpointer user_data) {
    GtkTreeIter iter;
    GtkTreeModel *model;
    gchar *file_path;
    gboolean is_dir;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gtk_tree_model_get(model, &iter,
                          COLUMN_PATH, &file_path,
                          COLUMN_IS_DIR, &is_dir, -1);

        if (!is_dir && file_path) {
            create_new_tab(file_path);
        }
        g_free(file_path);
    }
}

// Function to refresh the file tree
static void refresh_file_tree(const char *directory) {
    if (!file_tree_store || !directory) return;

    gtk_tree_store_clear(file_tree_store);

    g_free(current_directory);
    current_directory = g_strdup(directory);

    populate_file_tree(file_tree_store, NULL, directory);
    show_file_browser_panel();
}

// Create the file tree view
static GtkWidget* create_file_tree_view() {
    GtkWidget *scrolled_window;

    file_tree_store = gtk_tree_store_new(N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
    file_tree_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(file_tree_store)));

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes("Files", renderer, "text", COLUMN_NAME, NULL);
    gtk_tree_view_append_column(file_tree_view, column);

    GtkTreeSelection *selection = gtk_tree_view_get_selection(file_tree_view);
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
    g_signal_connect(selection, "changed", G_CALLBACK(on_file_selected), NULL);
    g_signal_connect(file_tree_view, "row-expanded", G_CALLBACK(on_row_expanded), NULL);

    scrolled_window = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), GTK_WIDGET(file_tree_view));

    return scrolled_window;
}

#pragma GCC diagnostic pop

// Tab close button callback
static void on_tab_close_button_clicked(GtkButton *button, gpointer user_data) {
    close_current_tab();
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
static void save_current_tab(void) {
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
static void open_finish(GObject *source_object, GAsyncResult *res, gpointer user_data) {
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
static void open_file_dialog(void) {
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Open File");
    gtk_file_dialog_open(dialog, GTK_WINDOW(global_window), NULL, open_finish, NULL);
    g_object_unref(dialog);
}

// Create new tab
void create_new_tab(const char *filename) {
    // Safety check: Don't create tabs before the notebook is initialized
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

    hide_panels();

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
#ifdef HAVE_TREE_SITTER
    tab_info->ts_tree = NULL;
#endif

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

#ifdef HAVE_TREE_SITTER
            highlight_buffer_sync(buffer, &tab_info->ts_tree, tab_info->lang_type);
#endif
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

// Get current tab info
TabInfo* get_current_tab_info() {
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
        // Close will happen after save
    } else if (choice == 2) { // Close without Saving
        tab_info->dirty = FALSE; // Force close
        close_current_tab();
    }
    // choice 0 is "Cancel" -> do nothing
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

    // Cleanup will happen in the tab_info destructor via g_object_set_data_full
#ifdef HAVE_TREE_SITTER
    if (tab_info->ts_tree) {
        ts_tree_delete(tab_info->ts_tree);
    }
#endif

    // Create new empty tab if no tabs left
    if (gtk_notebook_get_n_pages(global_notebook) == 0) {
        create_new_tab(NULL);
    }

    return FALSE;
}

// Action callback for menu items and shortcuts
static void action_callback(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    const char *action_name = g_action_get_name(G_ACTION(action));

    g_print("Action triggered: %s\n", action_name);

    if (strcmp(action_name, "new") == 0) {
        create_new_tab(NULL);
    } else if (strcmp(action_name, "open") == 0) {
        open_file_dialog();
    } else if (strcmp(action_name, "save") == 0) {
        save_current_tab();
    } else if (strcmp(action_name, "close") == 0) {
        close_current_tab();
    } else if (strcmp(action_name, "quit") == 0) {
        gtk_window_close(GTK_WINDOW(global_window));
    } else if (strcmp(action_name, "recent") == 0) {
        show_recent_files_panel();
    } else if (strcmp(action_name, "browser") == 0) {
        const char *home_dir = g_get_home_dir();
        refresh_file_tree(home_dir);
    }
}

// Setup keyboard shortcuts and actions
static void setup_shortcuts(GtkApplication *app) {
    const GActionEntry app_entries[] = {
        {"new", action_callback, NULL, NULL, NULL},
        {"open", action_callback, NULL, NULL, NULL},
        {"save", action_callback, NULL, NULL, NULL},
        {"close", action_callback, NULL, NULL, NULL},
        {"quit", action_callback, NULL, NULL, NULL},
        {"recent", action_callback, NULL, NULL, NULL},
        {"browser", action_callback, NULL, NULL, NULL}
    };

    g_action_map_add_action_entries(G_ACTION_MAP(app), app_entries, G_N_ELEMENTS(app_entries), app);

    // Set keyboard shortcuts
    gtk_application_set_accels_for_action(app, "app.new", (const char*[]){"<Control>n", NULL});
    gtk_application_set_accels_for_action(app, "app.open", (const char*[]){"<Control>o", NULL});
    gtk_application_set_accels_for_action(app, "app.save", (const char*[]){"<Control>s", NULL});
    gtk_application_set_accels_for_action(app, "app.close", (const char*[]){"<Control>w", NULL});
    gtk_application_set_accels_for_action(app, "app.quit", (const char*[]){"<Control>q", NULL});
    gtk_application_set_accels_for_action(app, "app.recent", (const char*[]){"<Control>r", NULL});
    gtk_application_set_accels_for_action(app, "app.browser", (const char*[]){"<Control>b", NULL});
}

// FIXED: Centralized initialization function
static void initialize_application(GtkApplication *app) {
    if (app_initialized) return;  // Prevent double initialization

    g_print("Initializing GPad editor...\n");

    // Create main window
    GtkWidget *window = gtk_application_window_new(app);
    global_window = window;
    gtk_window_set_title(GTK_WINDOW(window), "GPad - Multi-Tab Editor");
    gtk_window_set_default_size(GTK_WINDOW(window), 1200, 800);

    // Initialize recent manager
    recent_manager = gtk_recent_manager_get_default();

    // Create main layout
    GtkWidget *main_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_window_set_child(GTK_WINDOW(window), main_paned);

    // Create side panel container
    panel_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_paned_set_start_child(GTK_PANED(main_paned), panel_container);

    // Create side panels
    side_panel = create_file_tree_view();
    gtk_box_append(GTK_BOX(panel_container), side_panel);

    recent_panel = create_recent_files_panel();
    gtk_box_append(GTK_BOX(panel_container), recent_panel);

    // Create editor area
    GtkWidget *editor_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_paned_set_end_child(GTK_PANED(main_paned), editor_box);

    // Create notebook for tabs
    global_notebook = GTK_NOTEBOOK(gtk_notebook_new());
    gtk_notebook_set_scrollable(global_notebook, TRUE);
    gtk_widget_set_hexpand(GTK_WIDGET(global_notebook), TRUE);
    gtk_widget_set_vexpand(GTK_WIDGET(global_notebook), TRUE);
    gtk_box_append(GTK_BOX(editor_box), GTK_WIDGET(global_notebook));

    // Set initial paned position and hide panels
    gtk_paned_set_position(GTK_PANED(main_paned), 280);
    gtk_widget_set_visible(panel_container, FALSE);

#ifdef HAVE_TREE_SITTER
    // Initialize tree-sitter parser
    ts_parser = ts_parser_new();
    if (!ts_parser) {
        g_warning("Failed to create tree-sitter parser. Syntax highlighting disabled.");
    }
#endif

    // Apply dark theme CSS
    GtkCssProvider *css_provider = gtk_css_provider_new();
    const char *css_data =
        "textview { "
        "  background-color: #1E1E1E; "
        "  color: #D4D4D4; "
        "  font-family: 'JetBrains Mono', 'Source Code Pro', 'Consolas', monospace; "
        "  font-size: 11pt; "
        "} "
        "notebook tab { "
        "  padding: 8px 12px; "
        "} "
        "notebook tab button { "
        "  min-width: 16px; "
        "  min-height: 16px; "
        "  margin-left: 6px; "
        "}";

    gtk_css_provider_load_from_string(css_provider, css_data);

    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(css_provider);

    // Mark app as initialized
    app_initialized = TRUE;

    // Show window
    gtk_window_present(GTK_WINDOW(window));
    g_print("GPad editor initialized successfully.\n");
}

// Main application activation callback
static void activate(GtkApplication *app, gpointer user_data) {
    // Initialize the application first
    initialize_application(app);

    // Create initial tab
    const char *filename = (const char *)user_data;
    if (filename && g_file_test(filename, G_FILE_TEST_EXISTS)) {
        create_new_tab(filename);
    } else {
        create_new_tab(NULL);
    }
}

// FIXED: Handle command line arguments properly
static void handle_command_line(GApplication *app, GApplicationCommandLine *cmdline, gpointer user_data) {
    gchar **argv;
    gint argc;

    argv = g_application_command_line_get_arguments(cmdline, &argc);

    // Initialize the application if not already done
    if (!app_initialized) {
        initialize_application(GTK_APPLICATION(app));
        create_new_tab(NULL);  // Create at least one tab
    }

    // Open files from command line
    for (int i = 1; i < argc; i++) {
        if (g_file_test(argv[i], G_FILE_TEST_EXISTS)) {
            create_new_tab(argv[i]);
            g_print("Opening file from command line: %s\n", argv[i]);
        } else {
            g_warning("File does not exist: %s", argv[i]);
        }
    }

    g_strfreev(argv);

    if (global_window) {
        gtk_window_present(GTK_WINDOW(global_window));
    }
}

// Cleanup function
static void cleanup_resources(void) {
#ifdef HAVE_TREE_SITTER
    if (ts_parser) {
        ts_parser_delete(ts_parser);
        ts_parser = NULL;
    }
#endif

    g_free(current_directory);
    current_directory = NULL;
}

// Main function
int main(int argc, char **argv) {
    g_print("Starting GPad Multi-Tab Editor...\n");

    // Create application
    GtkApplication *app = gtk_application_new("org.gtk.gpad.multitab", G_APPLICATION_HANDLES_COMMAND_LINE);
    if (!app) {
        g_error("Failed to create GTK application");
        return 1;
    }

    // Connect signals - FIXED: No longer pass filename to activate
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    g_signal_connect(app, "command-line", G_CALLBACK(handle_command_line), NULL);

    // Setup keyboard shortcuts
    setup_shortcuts(app);

    // Run application
    int status = g_application_run(G_APPLICATION(app), argc, argv);

    // Cleanup
    cleanup_resources();
    g_object_unref(app);

    g_print("GPad editor exited with status: %d\n", status);
    return status;
}
