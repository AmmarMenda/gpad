#include <gtk/gtk.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <tree_sitter/api.h>

// Enum for supported languages
typedef enum {
    LANG_C,
    LANG_PYTHON,
    LANG_DART,
    LANG_UNKNOWN
} LanguageType;

// Global references
static GtkTextView *global_text_view = NULL;
static GtkWidget *global_window = NULL;
static GtkTreeView *file_tree_view = NULL;
static GtkTreeStore *file_tree_store = NULL;
static GtkWidget *side_panel = NULL;
static GtkWidget *recent_panel = NULL;
static GtkWidget *panel_container = NULL;
static GtkListBox *recent_list_box = NULL;
static char *current_directory = NULL;
static GtkRecentManager *recent_manager = NULL;

// Tree-sitter globals
static TSParser *ts_parser = NULL;
static TSTree *ts_tree = NULL;
static LanguageType current_language_type = LANG_UNKNOWN;
static guint highlight_timer_id = 0;

// Forward declarations for grammar parsers
TSLanguage *tree_sitter_c(void);
TSLanguage *tree_sitter_python(void);
TSLanguage *tree_sitter_dart(void);

// Forward declarations for local functions
static void load_file_into_text_view(GtkTextView *text_view, const char *filename);
static void populate_file_tree(GtkTreeStore *store, GtkTreeIter *parent, const char *path);
static void show_recent_files_panel(void);
static void hide_recent_files_panel(void);
static void refresh_file_tree(const char *directory);
static void show_file_browser_panel(void);
static void populate_recent_files(void);
static void highlight_buffer_sync(GtkTextBuffer *buffer);
static void on_buffer_changed(GtkTextBuffer *buffer, gpointer user_data);
static void apply_tags_recursive(TSNode node, GtkTextBuffer *buffer, const char *source_code, LanguageType lang);
static TSLanguage *get_language_from_filename(const char *filename);

// Function to check if path is a directory
static gboolean is_directory(const char *path) {
    struct stat statbuf;
    if (stat(path, &statbuf) != 0) return FALSE;
    return S_ISDIR(statbuf.st_mode);
}

// Add file to recent manager
static void add_to_recent_files(const char *filename) {
    if (!recent_manager || !filename) return;
    char *uri = g_filename_to_uri(filename, NULL, NULL);
    if (uri) {
        gtk_recent_manager_add_item(recent_manager, uri);
        g_free(uri);
    }
}

// Get TSLanguage based on filename
static TSLanguage* get_language_from_filename(const char *filename) {
    if (g_str_has_suffix(filename, ".c") || g_str_has_suffix(filename, ".h")) {
        current_language_type = LANG_C;
        return tree_sitter_c();
    }
    if (g_str_has_suffix(filename, ".py")) {
        current_language_type = LANG_PYTHON;
        return tree_sitter_python();
    }
    if (g_str_has_suffix(filename, ".dart")) {
        current_language_type = LANG_DART;
        return tree_sitter_dart();
    }
    current_language_type = LANG_UNKNOWN;
    return NULL;
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

// Hide all panels
static void hide_recent_files_panel(void) {
    if (recent_panel) {
        gtk_widget_set_visible(recent_panel, FALSE);
    }
}

// Setup syntax highlighting tags
static void setup_highlighting_tags(GtkTextBuffer *buffer) {
    gtk_text_buffer_create_tag(buffer, "comment", "foreground", "#6A9955", "style", PANGO_STYLE_ITALIC, NULL);
    gtk_text_buffer_create_tag(buffer, "string", "foreground", "#CE9178", NULL);
    gtk_text_buffer_create_tag(buffer, "preproc", "foreground", "#9B9B9B", NULL);
    gtk_text_buffer_create_tag(buffer, "keyword", "foreground", "#569CD6", "font-weight", PANGO_WEIGHT_BOLD, NULL);
    gtk_text_buffer_create_tag(buffer, "control", "foreground", "#C586C0", "font-weight", PANGO_WEIGHT_BOLD, NULL);
    gtk_text_buffer_create_tag(buffer, "type", "foreground", "#4EC9B0", NULL);
    gtk_text_buffer_create_tag(buffer, "number", "foreground", "#B5CEA8", NULL);
    gtk_text_buffer_create_tag(buffer, "function", "foreground", "#DCDCAA", NULL);
    gtk_text_buffer_create_tag(buffer, "constant", "foreground", "#4FC1FF", NULL);
    gtk_text_buffer_create_tag(buffer, "decorator", "foreground", "#B5CEA8", "style", PANGO_STYLE_ITALIC, NULL);
}

// Recursive function to traverse the syntax tree and apply tags
static void apply_tags_recursive(TSNode node, GtkTextBuffer *buffer, const char *source_code, LanguageType lang) {
    if (ts_node_is_null(node)) return;

    const char *type = ts_node_type(node);
    const char *tag_name = NULL;

    // Language-specific highlighting rules
    if (lang == LANG_C) {
        if (strcmp(type, "comment") == 0) tag_name = "comment";
        else if (strcmp(type, "string_literal") == 0 || strcmp(type, "char_literal") == 0) tag_name = "string";
        else if (strstr(type, "preproc") != NULL) tag_name = "preproc";
        else if (strcmp(type, "return") == 0 || strcmp(type, "if") == 0 || strcmp(type, "for") == 0 || strcmp(type, "while") == 0 || strcmp(type, "break") == 0 || strcmp(type, "case") == 0) tag_name = "control";
        else if (strcmp(type, "storage_class_specifier") == 0 || strcmp(type, "type_qualifier") == 0 || strcmp(type, "struct") == 0 || strcmp(type, "typedef") == 0) tag_name = "keyword";
        else if (strcmp(type, "primitive_type") == 0 || strcmp(type, "type_identifier") == 0) tag_name = "type";
        else if (strcmp(type, "number_literal") == 0) tag_name = "number";
        else if (strcmp(type, "call_expression") == 0) {
             TSNode func_name_node = ts_node_child_by_field_name(node, "function", 8);
             if(!ts_node_is_null(func_name_node)) apply_tags_recursive(func_name_node, buffer, source_code, lang);
        } else if (strcmp(type, "identifier") == 0) {
            TSNode parent = ts_node_parent(node);
            if(parent.id && strcmp(ts_node_type(parent), "function_declarator") == 0) tag_name = "function";
        }
    } else if (lang == LANG_PYTHON) {
        if (strcmp(type, "comment") == 0) tag_name = "comment";
        else if (strcmp(type, "string") == 0) tag_name = "string";
        else if (strcmp(type, "from") == 0 || strcmp(type, "import") == 0 || strcmp(type, "as") == 0) tag_name = "preproc";
        else if (strcmp(type, "if") == 0 || strcmp(type, "for") == 0 || strcmp(type, "while") == 0 || strcmp(type, "return") == 0 || strcmp(type, "in") == 0 || strcmp(type, "try") == 0 || strcmp(type, "except") == 0) tag_name = "control";
        else if (strcmp(type, "def") == 0 || strcmp(type, "class") == 0 || strcmp(type, "pass") == 0) tag_name = "keyword";
        else if (strcmp(type, "type") == 0) tag_name = "type";
        else if (strcmp(type, "integer") == 0 || strcmp(type, "float") == 0) tag_name = "number";
        else if (strcmp(type, "decorator") == 0) tag_name = "decorator";
        else if (strcmp(type, "call") == 0) {
             TSNode func_name_node = ts_node_child_by_field_name(node, "function", 8);
             if(!ts_node_is_null(func_name_node)) apply_tags_recursive(func_name_node, buffer, source_code, lang);
        } else if (strcmp(type, "identifier") == 0) {
             TSNode parent = ts_node_parent(node);
             if(parent.id && (strcmp(ts_node_type(parent), "function_definition") == 0 || strcmp(ts_node_type(parent), "class_definition") == 0)) tag_name = "function";
        }
    } else if (lang == LANG_DART) {
        if (strcmp(type, "comment") == 0) tag_name = "comment";
        else if (strcmp(type, "string_literal") == 0) tag_name = "string";
        else if (strcmp(type, "import_directive") == 0 || strcmp(type, "export_directive") == 0) tag_name = "preproc";
        else if (strcmp(type, "if_statement") == 0 || strcmp(type, "for_statement") == 0 || strcmp(type, "while_statement") == 0 || strcmp(type, "return_statement") == 0) tag_name = "control";
        else if (strcmp(type, "class_definition") == 0 || strcmp(type, "final") == 0 || strcmp(type, "const") == 0 || strcmp(type, "static") == 0) tag_name = "keyword";
        else if (strcmp(type, "type_name") == 0 || strcmp(type, "primitive_type") == 0) tag_name = "type";
        else if (strcmp(type, "number_literal") == 0) tag_name = "number";
        else if (strcmp(type, "annotation") == 0) tag_name = "decorator";
        else if (strcmp(type, "method_invocation") == 0) {
             TSNode func_name_node = ts_node_child_by_field_name(node, "identifier", 10);
             if(!ts_node_is_null(func_name_node)) apply_tags_recursive(func_name_node, buffer, source_code, lang);
        } else if (strcmp(type, "identifier") == 0) {
             TSNode parent = ts_node_parent(node);
             if(parent.id && (strcmp(ts_node_type(parent), "function_signature") == 0 || strcmp(ts_node_type(parent), "class_declaration") == 0)) tag_name = "function";
        }
    }


    if (tag_name) {
        uint32_t start_byte = ts_node_start_byte(node);
        uint32_t end_byte = ts_node_end_byte(node);

        gint start_char_offset = g_utf8_pointer_to_offset(source_code, source_code + start_byte);
        gint end_char_offset = g_utf8_pointer_to_offset(source_code, source_code + end_byte);

        GtkTextIter start_iter, end_iter;
        gtk_text_buffer_get_iter_at_offset(buffer, &start_iter, start_char_offset);
        gtk_text_buffer_get_iter_at_offset(buffer, &end_iter, end_char_offset);

        gtk_text_buffer_apply_tag_by_name(buffer, tag_name, &start_iter, &end_iter);
    }

    // Recurse to children
    for (uint32_t i = 0; i < ts_node_child_count(node); ++i) {
        apply_tags_recursive(ts_node_child(node, i), buffer, source_code, lang);
    }
}

// Main function to trigger highlighting
void highlight_buffer_sync(GtkTextBuffer *buffer) {
    if (current_language_type == LANG_UNKNOWN) return;

    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);

    if (gtk_text_iter_get_offset(&start) == gtk_text_iter_get_offset(&end)) return;

    char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    gtk_text_buffer_remove_all_tags(buffer, &start, &end);

    if (ts_tree) ts_tree_delete(ts_tree);
    ts_tree = ts_parser_parse_string(ts_parser, NULL, text, strlen(text));

    if (ts_tree) {
        TSNode root_node = ts_tree_root_node(ts_tree);
        apply_tags_recursive(root_node, buffer, text, current_language_type);
    } else {
        g_warning("Tree-sitter parsing failed.");
    }

    g_free(text);
}


// Load file function
static void load_file_into_text_view(GtkTextView *text_view, const char *filename) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(text_view);
    TSLanguage *lang = get_language_from_filename(filename);

    // Set the parser's language
    if (lang) {
        ts_parser_set_language(ts_parser, lang);
    } else {
        current_language_type = LANG_UNKNOWN; // Reset for unsupported files
    }

    FILE *f = fopen(filename, "r");
    if (f != NULL) {
        fseek(f, 0, SEEK_END);
        long file_size = ftell(f);
        rewind(f);
        char *file_contents = g_malloc(file_size + 1);
        fread(file_contents, 1, file_size, f);
        file_contents[file_size] = '\0';
        fclose(f);

        // Block "changed" signal during text set, then unblock
        g_signal_handlers_block_by_func(buffer, (gpointer)on_buffer_changed, NULL);
        gtk_text_buffer_set_text(buffer, file_contents, -1);
        g_signal_handlers_unblock_by_func(buffer, (gpointer)on_buffer_changed, NULL);

        if (lang) {
            highlight_buffer_sync(buffer);
        } else { // Clear tags for unsupported files
            GtkTextIter f_start, f_end;
            gtk_text_buffer_get_bounds(buffer, &f_start, &f_end);
            gtk_text_buffer_remove_all_tags(buffer, &f_start, &f_end);
        }

        // Add to recent files
        add_to_recent_files(filename);

        // Update window title
        char *basename = g_path_get_basename(filename);
        char *title = g_strdup_printf("GPad - %s", basename);
        gtk_window_set_title(GTK_WINDOW(global_window), title);
        g_free(basename);
        g_free(title);

        g_free(file_contents);
    }
}

// Recent file item click handler
static void on_recent_file_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data) {
    const char *filename = (const char *)g_object_get_data(G_OBJECT(row), "filename");
    if (filename && global_text_view) {
        load_file_into_text_view(global_text_view, filename);
        if (g_file_test(filename, G_FILE_TEST_EXISTS)) {
            char *directory = g_path_get_dirname(filename);
            refresh_file_tree(directory);
            g_free(directory);
        }
    }
}

// Create recent files list
static void populate_recent_files(void) {
    if (!recent_list_box || !recent_manager) return;
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(GTK_WIDGET(recent_list_box))) != NULL) {
        gtk_list_box_remove(recent_list_box, child);
    }
    GList *items = gtk_recent_manager_get_items(recent_manager);
    int count = 0;
    for (GList *l = items; l && count < 10; l = l->next) {
        GtkRecentInfo *info = (GtkRecentInfo *)l->data;
        const char *mime_type = gtk_recent_info_get_mime_type(info);
        if (mime_type && (g_str_has_prefix(mime_type, "text/") || g_str_has_prefix(mime_type, "application/") )) {
            const char *uri = gtk_recent_info_get_uri(info);
            char *filename = g_filename_from_uri(uri, NULL, NULL);
            if (filename && g_file_test(filename, G_FILE_TEST_EXISTS)) {
                GtkWidget *row = gtk_list_box_row_new();
                char *basename = g_path_get_basename(filename);
                char *display_text = g_strdup_printf("📄 %s", basename);
                GtkWidget *label = gtk_label_new(display_text);
                gtk_label_set_xalign(GTK_LABEL(label), 0.0);
                gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
                g_object_set_data_full(G_OBJECT(row), "filename", g_strdup(filename), g_free);
                gtk_list_box_append(recent_list_box, row);
                g_free(basename);
                g_free(display_text);
                count++;
            }
            g_free(filename);
        }
    }
    g_list_free_full(items, (GDestroyNotify)gtk_recent_info_unref);
    if (count == 0) {
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label = gtk_label_new("No recent files");
        gtk_widget_set_sensitive(row, FALSE);
        gtk_label_set_xalign(GTK_LABEL(label), 0.5);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        gtk_list_box_append(recent_list_box, row);
    }
}

// Create recent files panel
static GtkWidget* create_recent_files_panel(void) {
    GtkWidget *box, *header, *scrolled;
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(box, 20);
    gtk_widget_set_margin_end(box, 20);
    gtk_widget_set_margin_top(box, 20);
    gtk_widget_set_margin_bottom(box, 20);
    header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(header), "<b>Recent Files</b>");
    gtk_label_set_xalign(GTK_LABEL(header), 0.0);
    gtk_box_append(GTK_BOX(box), header);
    scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled, -1, 300);
    gtk_box_append(GTK_BOX(box), scrolled);
    recent_list_box = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_selection_mode(recent_list_box, GTK_SELECTION_NONE);
    g_signal_connect(recent_list_box, "row-activated", G_CALLBACK(on_recent_file_activated), NULL);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), GTK_WIDGET(recent_list_box));
    GtkWidget *instructions = gtk_label_new("Click on a file to open it");
    gtk_label_set_xalign(GTK_LABEL(instructions), 0.0);
    gtk_widget_add_css_class(instructions, "dim-label");
    gtk_box_append(GTK_BOX(box), instructions);
    return box;
}

// Function to populate the file tree
static void populate_file_tree(GtkTreeStore *store, GtkTreeIter *parent, const char *path) {
    DIR *dir;
    struct dirent *entry;
    char full_path[1024];
    GtkTreeIter iter;
    dir = opendir(path);
    if (dir == NULL) return;
    GPtrArray *directories = g_ptr_array_new_with_free_func(g_free);
    GPtrArray *files = g_ptr_array_new_with_free_func(g_free);
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        if (is_directory(full_path)) g_ptr_array_add(directories, g_strdup(entry->d_name));
        else g_ptr_array_add(files, g_strdup(entry->d_name));
    }
    closedir(dir);
    g_ptr_array_sort(directories, (GCompareFunc)strcmp);
    g_ptr_array_sort(files, (GCompareFunc)strcmp);
    for (guint i = 0; i < directories->len; i++) {
        char *name = (char *)directories->pdata[i];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, name);
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        gtk_tree_store_append(store, &iter, parent);
        gtk_tree_store_set(store, &iter, 0, name, 1, full_path, 2, TRUE, -1);
        GtkTreeIter dummy;
        gtk_tree_store_append(store, &dummy, &iter);
        gtk_tree_store_set(store, &dummy, 0, "Loading...", 1, "", 2, FALSE, -1);
        #pragma GCC diagnostic pop
    }
    for (guint i = 0; i < files->len; i++) {
        char *name = (char *)files->pdata[i];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, name);
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        gtk_tree_store_append(store, &iter, parent);
        gtk_tree_store_set(store, &iter, 0, name, 1, full_path, 2, FALSE, -1);
        #pragma GCC diagnostic pop
    }
    g_ptr_array_free(directories, TRUE);
    g_ptr_array_free(files, TRUE);
}

// Handle tree row expansion
static void on_row_expanded(GtkTreeView *tree_view, GtkTreeIter *iter, GtkTreePath *path, gpointer user_data) {
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    GtkTreeStore *store = GTK_TREE_STORE(gtk_tree_view_get_model(tree_view));
    gchar *dir_path;
    gboolean is_dir;
    GtkTreeIter child;
    gtk_tree_model_get(GTK_TREE_MODEL(store), iter, 1, &dir_path, 2, &is_dir, -1);
    if (!is_dir) { g_free(dir_path); return; }
    if (gtk_tree_model_iter_children(GTK_TREE_MODEL(store), &child, iter)) {
        gchar *child_name;
        gtk_tree_model_get(GTK_TREE_MODEL(store), &child, 0, &child_name, -1);
        if (g_strcmp0(child_name, "Loading...") == 0) {
            gtk_tree_store_remove(store, &child);
            populate_file_tree(store, iter, dir_path);
        }
        g_free(child_name);
    }
    #pragma GCC diagnostic pop
    g_free(dir_path);
}

// Handle file selection in tree
static void on_file_selected(GtkTreeSelection *selection, gpointer user_data) {
    GtkTreeIter iter;
    GtkTreeModel *model;
    gchar *file_path;
    gboolean is_dir;
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gtk_tree_model_get(model, &iter, 1, &file_path, 2, &is_dir, -1);
        if (!is_dir && file_path && strlen(file_path) > 0) {
            load_file_into_text_view(global_text_view, file_path);
        }
        g_free(file_path);
    }
    #pragma GCC diagnostic pop
}

// Function to refresh the file tree
static void refresh_file_tree(const char *directory) {
    if (!file_tree_store || !directory) return;
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    gtk_tree_store_clear(file_tree_store);
    #pragma GCC diagnostic pop
    g_free(current_directory);
    current_directory = g_strdup(directory);
    populate_file_tree(file_tree_store, NULL, directory);
    show_file_browser_panel();
}

// Create the file tree view
static GtkWidget* create_file_tree_view() {
    GtkWidget *scrolled_window;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkTreeSelection *selection;
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    file_tree_store = gtk_tree_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
    file_tree_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(file_tree_store)));
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Files", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(file_tree_view, column);
    selection = gtk_tree_view_get_selection(file_tree_view);
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
    g_signal_connect(selection, "changed", G_CALLBACK(on_file_selected), NULL);
    #pragma GCC diagnostic pop
    g_signal_connect(file_tree_view, "row-expanded", G_CALLBACK(on_row_expanded), NULL);
    scrolled_window = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), GTK_WIDGET(file_tree_view));
    gtk_widget_set_size_request(scrolled_window, 200, -1);
    return scrolled_window;
}

static void file_opened(GObject *object, GAsyncResult *res, gpointer user_data) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(object);
    GFile *file = NULL;
    GError *error = NULL;
    file = gtk_file_dialog_open_finish(dialog, res, &error);
    if (error) { g_error_free(error); return; }
    if (file) {
        char *filename = g_file_get_path(file);
        if (filename) {
            load_file_into_text_view(GTK_TEXT_VIEW(user_data), filename);
            char *directory = g_path_get_dirname(filename);
            refresh_file_tree(directory);
            g_free(directory);
            g_free(filename);
        }
        g_object_unref(file);
    }
}

static void file_saved(GObject *object, GAsyncResult *res, gpointer user_data) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(object);
    GError *error = NULL;
    GFile *file = gtk_file_dialog_save_finish(dialog, res, &error);
    if (error) { g_error_free(error); return; }
    if (file) {
        char *filename = g_file_get_path(file);
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(user_data));
        if (filename) {
            FILE *f = fopen(filename, "w");
            if (f != NULL) {
                GtkTextIter start, end;
                gtk_text_buffer_get_start_iter(buffer, &start);
                gtk_text_buffer_get_end_iter(buffer, &end);
                char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
                fputs(text, f);
                fclose(f);
                g_free(text);
                add_to_recent_files(filename);
            }
            g_free(filename);
        }
        g_object_unref(file);
    }
}

static void open_file_dialog(GtkWidget *widget, gpointer user_data) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(gtk_file_dialog_new());
    GtkWidget *window = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(widget)));
    gtk_file_dialog_open(dialog, GTK_WINDOW(window), NULL, file_opened, user_data);
}

static void save_file_dialog(GtkWidget *widget, gpointer user_data) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(gtk_file_dialog_new());
    GtkWidget *window = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(widget)));
    gtk_file_dialog_save(dialog, GTK_WINDOW(window), NULL, file_saved, user_data);
}

// Keyboard shortcut callbacks
static gboolean shortcut_open(GtkWidget *widget, GVariant *args, gpointer user_data) { if (global_text_view) open_file_dialog(widget, global_text_view); return TRUE; }
static gboolean shortcut_save(GtkWidget *widget, GVariant *args, gpointer user_data) { if (global_text_view) save_file_dialog(widget, global_text_view); return TRUE; }
static gboolean shortcut_new(GtkWidget *widget, GVariant *args, gpointer user_data) {
    if (global_text_view) {
        gtk_text_buffer_set_text(gtk_text_view_get_buffer(global_text_view), "", -1);
        gtk_window_set_title(GTK_WINDOW(global_window), "GPad - Text Editor");
        if (ts_tree) { ts_tree_delete(ts_tree); ts_tree = NULL; }
        hide_recent_files_panel();
    }
    return TRUE;
}
static gboolean shortcut_quit(GtkWidget *widget, GVariant *args, gpointer user_data) { if (global_window) gtk_window_close(GTK_WINDOW(global_window)); return TRUE; }
static gboolean shortcut_select_all(GtkWidget *widget, GVariant *args, gpointer user_data) { if (global_text_view) { GtkTextBuffer *b = gtk_text_view_get_buffer(global_text_view); GtkTextIter s, e; gtk_text_buffer_get_bounds(b, &s, &e); gtk_text_buffer_select_range(b, &s, &e); } return TRUE; }
static gboolean shortcut_toggle_panel(GtkWidget *widget, GVariant *args, gpointer user_data) { if (panel_container) gtk_widget_set_visible(panel_container, !gtk_widget_get_visible(panel_container)); return TRUE; }
static gboolean shortcut_show_recent(GtkWidget *widget, GVariant *args, gpointer user_data) { if (recent_panel) { if (gtk_widget_get_visible(recent_panel)) hide_recent_files_panel(); else show_recent_files_panel(); } return TRUE; }

static void setup_shortcuts(GtkWidget *window) {
    GtkEventController *controller = gtk_shortcut_controller_new();
    gtk_widget_add_controller(window, controller);
    #define ADD_SHORTCUT(key, mask, callback) gtk_shortcut_controller_add_shortcut(GTK_SHORTCUT_CONTROLLER(controller), gtk_shortcut_new(gtk_keyval_trigger_new(key, mask), gtk_callback_action_new(callback, NULL, NULL)))
    ADD_SHORTCUT(GDK_KEY_o, GDK_CONTROL_MASK, shortcut_open);
    ADD_SHORTCUT(GDK_KEY_s, GDK_CONTROL_MASK, shortcut_save);
    ADD_SHORTCUT(GDK_KEY_n, GDK_CONTROL_MASK, shortcut_new);
    ADD_SHORTCUT(GDK_KEY_q, GDK_CONTROL_MASK, shortcut_quit);
    ADD_SHORTCUT(GDK_KEY_a, GDK_CONTROL_MASK, shortcut_select_all);
    ADD_SHORTCUT(GDK_KEY_b, GDK_CONTROL_MASK, shortcut_toggle_panel);
    ADD_SHORTCUT(GDK_KEY_r, GDK_CONTROL_MASK, shortcut_show_recent);
    #undef ADD_SHORTCUT
}

// Timeout callback for debounced highlighting
static gboolean highlight_timeout_callback(gpointer user_data) {
    highlight_buffer_sync(GTK_TEXT_BUFFER(user_data));
    highlight_timer_id = 0;
    return G_SOURCE_REMOVE;
}

// Signal handler for text buffer changes
static void on_buffer_changed(GtkTextBuffer *buffer, gpointer user_data) {
    if (highlight_timer_id > 0) g_source_remove(highlight_timer_id);
    highlight_timer_id = g_timeout_add(150, highlight_timeout_callback, buffer);
}

static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window, *main_paned, *editor_box, *scrolled_window;
    GtkWidget *open_button, *save_button, *recent_button, *button_box;
    GtkTextBuffer *buffer;
    GtkCssProvider *css_provider;

    window = gtk_application_window_new(app);
    global_window = window;
    gtk_window_set_title(GTK_WINDOW(window), "GPad - Text Editor");
    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);
    recent_manager = gtk_recent_manager_get_default();
    main_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_window_set_child(GTK_WINDOW(window), main_paned);
    panel_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(panel_container, 250, -1);
    gtk_paned_set_start_child(GTK_PANED(main_paned), panel_container);
    side_panel = create_file_tree_view();
    gtk_widget_set_visible(side_panel, FALSE);
    gtk_box_append(GTK_BOX(panel_container), side_panel);
    recent_panel = create_recent_files_panel();
    gtk_widget_set_visible(recent_panel, FALSE);
    gtk_box_append(GTK_BOX(panel_container), recent_panel);
    editor_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_paned_set_end_child(GTK_PANED(main_paned), editor_box);
    scrolled_window = gtk_scrolled_window_new();
    gtk_widget_set_hexpand(scrolled_window, TRUE);
    gtk_widget_set_vexpand(scrolled_window, TRUE);
    gtk_box_append(GTK_BOX(editor_box), scrolled_window);
    global_text_view = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_wrap_mode(global_text_view, GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_monospace(global_text_view, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), GTK_WIDGET(global_text_view));
    button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_append(GTK_BOX(editor_box), button_box);
    open_button = gtk_button_new_with_label("Open (Ctrl+O)");
    g_signal_connect(open_button, "clicked", G_CALLBACK(open_file_dialog), global_text_view);
    gtk_box_append(GTK_BOX(button_box), open_button);
    save_button = gtk_button_new_with_label("Save (Ctrl+S)");
    g_signal_connect(save_button, "clicked", G_CALLBACK(save_file_dialog), global_text_view);
    gtk_box_append(GTK_BOX(button_box), save_button);
    recent_button = gtk_button_new_with_label("Recent (Ctrl+R)");
    g_signal_connect_swapped(recent_button, "clicked", G_CALLBACK(show_recent_files_panel), NULL);
    gtk_box_append(GTK_BOX(button_box), recent_button);
    gtk_paned_set_position(GTK_PANED(main_paned), 250);
    setup_shortcuts(window);

    ts_parser = ts_parser_new();
    buffer = gtk_text_view_get_buffer(global_text_view);
    setup_highlighting_tags(buffer);
    g_signal_connect(buffer, "changed", G_CALLBACK(on_buffer_changed), NULL);
    css_provider = gtk_css_provider_new();
    const char *css_data = "textview { background-color: #1E1E1E; color: #D4D4D4; font-family: monospace; caret-color: #AEAFAD; } textview selection { background-color: #264F78; }";
    gtk_css_provider_load_from_string(css_provider, css_data);
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css_provider);

    const char *filename = (const char *)user_data;
    if (filename != NULL) {
        load_file_into_text_view(global_text_view, filename);
        char *directory = g_path_get_dirname(filename);
        refresh_file_tree(directory);
        g_free(directory);
    } else {
        show_recent_files_panel();
    }
    gtk_window_present(GTK_WINDOW(window));
}

static void handle_command_line(GApplication *app, GApplicationCommandLine *cmdline, gpointer user_data) {
    gchar **argv;
    gint argc;
    argv = g_application_command_line_get_arguments(cmdline, &argc);
    g_application_activate(app);
    if (argc > 1 && global_text_view) {
        load_file_into_text_view(global_text_view, argv[1]);
        char *directory = g_path_get_dirname(argv[1]);
        refresh_file_tree(directory);
        g_free(directory);
    }
    g_strfreev(argv);
}

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;
    app = gtk_application_new("org.gtk.gpad.multilang", G_APPLICATION_HANDLES_COMMAND_LINE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), (argc > 1) ? argv[1] : NULL);
    g_signal_connect(app, "command-line", G_CALLBACK(handle_command_line), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    if (ts_tree) ts_tree_delete(ts_tree);
    if (ts_parser) ts_parser_delete(ts_parser);
    g_object_unref(app);
    return status;
}
