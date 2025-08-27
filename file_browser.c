#include "gpad.h"

// Suppress deprecation warnings for tree view functions
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

// FORWARD DECLARATION - Fix compilation error
static void apply_compact_styling_direct(GtkWidget *tree_view);

// Structure for deferred update data
typedef struct {
    GtkTreeStore *store;
    char *directory_path;
} UpdateData;

// Global flags to prevent issues
static gboolean refresh_in_progress = FALSE;
static gboolean file_opening_in_progress = FALSE;

// Get icon name based on file extension
static const char* get_file_icon(const char *filename, gboolean is_dir) {
    if (is_dir) {
        return "folder";
    }

    if (!filename) {
        return "text-x-generic";
    }

    // Find file extension
    const char *ext = strrchr(filename, '.');
    if (!ext) {
        return "text-x-generic";
    }

    // Map extensions to icon names
    if (g_strcmp0(ext, ".c") == 0) return "text-x-csrc";
    if (g_strcmp0(ext, ".h") == 0) return "text-x-chdr";
    if (g_strcmp0(ext, ".cpp") == 0) return "text-x-c++src";
    if (g_strcmp0(ext, ".hpp") == 0) return "text-x-c++hdr";
    if (g_strcmp0(ext, ".py") == 0) return "text-x-python";
    if (g_strcmp0(ext, ".dart") == 0) return "application-dart";
    if (g_strcmp0(ext, ".js") == 0) return "text-javascript";
    if (g_strcmp0(ext, ".html") == 0) return "text-html";
    if (g_strcmp0(ext, ".css") == 0) return "text-css";
    if (g_strcmp0(ext, ".json") == 0) return "application-json";
    if (g_strcmp0(ext, ".xml") == 0) return "text-xml";
    if (g_strcmp0(ext, ".md") == 0) return "text-x-markdown";
    if (g_strcmp0(ext, ".txt") == 0) return "text-plain";
    if (g_strcmp0(ext, ".pdf") == 0) return "application-pdf";
    if (g_strcmp0(ext, ".png") == 0) return "image-png";
    if (g_strcmp0(ext, ".jpg") == 0 || g_strcmp0(ext, ".jpeg") == 0) return "image-jpeg";
    if (g_strcmp0(ext, ".gif") == 0) return "image-gif";
    if (g_strcmp0(ext, ".svg") == 0) return "image-svg+xml";
    if (g_strcmp0(ext, ".zip") == 0) return "application-zip";
    if (g_strcmp0(ext, ".tar") == 0 || g_strcmp0(ext, ".gz") == 0) return "application-x-archive";
    if (g_strcmp0(ext, ".exe") == 0) return "application-x-executable";
    if (g_strcmp0(ext, ".sh") == 0) return "text-x-script";
    if (g_strcmp0(ext, ".makefile") == 0 || g_strcmp0(ext, ".mk") == 0) return "text-x-makefile";

    // Default icon for unknown types
    return "text-x-generic";
}

// Deferred refresh callback - runs safely outside signal handlers
static gboolean deferred_refresh_callback(gpointer user_data) {
    UpdateData *data = (UpdateData*)user_data;

    if (!data || !data->store || !data->directory_path) {
        g_warning("deferred_refresh_callback: Invalid data");
        refresh_in_progress = FALSE;
        if (data) {
            g_free(data->directory_path);
            g_free(data);
        }
        return G_SOURCE_REMOVE;
    }

    g_print("Performing deferred tree refresh for: %s\n", data->directory_path);

    // Now it's safe to clear and repopulate the tree store
    gtk_tree_store_clear(data->store);
    populate_file_tree(data->store, NULL, data->directory_path);

    // Update current directory
    g_free(current_directory);
    current_directory = g_strdup(data->directory_path);

    // Cleanup
    g_free(data->directory_path);
    g_free(data);
    refresh_in_progress = FALSE;

    return G_SOURCE_REMOVE;
}

// Get directory of current tab's file, or home directory if no file
static char* get_current_tab_directory(void) {
    TabInfo *tab_info = get_current_tab_info();

    if (tab_info && tab_info->filename) {
        char *dir = g_path_get_dirname(tab_info->filename);
        g_print("Current file directory: %s\n", dir);
        return dir;
    } else {
        const char *home_dir = g_get_home_dir();
        g_print("Using home directory: %s\n", home_dir);
        return g_strdup(home_dir);
    }
}

// Schedule a safe tree refresh
static void schedule_tree_refresh(const char *directory) {
    if (!directory || refresh_in_progress) {
        g_print("Refresh already in progress or invalid directory\n");
        return;
    }

    UpdateData *data = g_new0(UpdateData, 1);
    data->store = file_tree_store;
    data->directory_path = g_strdup(directory);

    refresh_in_progress = TRUE;
    g_idle_add(deferred_refresh_callback, data);
}

// Populate the file tree with icons
void populate_file_tree(GtkTreeStore *store, GtkTreeIter *parent, const char *path) {
    if (!store || !path) {
        g_warning("populate_file_tree: Invalid parameters");
        return;
    }

    DIR *dir = opendir(path);
    if (!dir) {
        g_warning("populate_file_tree: Cannot open directory: %s", path);
        return;
    }

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
        const char *icon = get_file_icon(name, TRUE);

        gtk_tree_store_append(store, &iter, parent);
        gtk_tree_store_set(store, &iter,
                          COLUMN_ICON, icon,
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
        const char *icon = get_file_icon(name, FALSE);

        gtk_tree_store_append(store, &iter, parent);
        gtk_tree_store_set(store, &iter,
                          COLUMN_ICON, icon,
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
    (void)user_data;
    (void)path;

    if (!tree_view || !iter) {
        return;
    }

    GtkTreeStore *store = GTK_TREE_STORE(gtk_tree_view_get_model(tree_view));
    if (!store) return;

    GtkTreeModel *model = GTK_TREE_MODEL(store);
    GtkTreeIter child;

    if (gtk_tree_model_iter_children(model, &child, iter)) {
        gchar* name;
        gtk_tree_model_get(model, &child, COLUMN_NAME, &name, -1);

        if (name == NULL) {
            gtk_tree_store_remove(store, &child);
            gchar *dir_path;
            gtk_tree_model_get(model, iter, COLUMN_PATH, &dir_path, -1);
            if (dir_path) {
                populate_file_tree(store, iter, dir_path);
                g_free(dir_path);
            }
        }
        g_free(name);
    }
}

// Row activation handler - double-click to open files
static void on_row_activated(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data) {
    (void)tree_view;
    (void)column;
    (void)user_data;

    if (file_opening_in_progress) {
        g_print("File opening already in progress, ignoring activation\n");
        return;
    }

    if (!path || !file_tree_store) {
        g_warning("Invalid path or tree store");
        return;
    }

    file_opening_in_progress = TRUE;

    GtkTreeIter iter;
    if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(file_tree_store), &iter, path)) {
        g_warning("Could not get iterator for path");
        file_opening_in_progress = FALSE;
        return;
    }

    gchar *file_path = NULL;
    gboolean is_dir = FALSE;

    gtk_tree_model_get(GTK_TREE_MODEL(file_tree_store), &iter,
                      COLUMN_PATH, &file_path,
                      COLUMN_IS_DIR, &is_dir, -1);

    g_print("Row activated: path='%s', is_dir=%s\n",
            file_path ? file_path : "NULL",
            is_dir ? "TRUE" : "FALSE");

    // Only open files, not directories
    if (!is_dir && file_path && *file_path != '\0') {
        if (g_file_test(file_path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
            g_print("Opening SINGLE file: %s\n", file_path);
            create_new_tab_from_sidebar(file_path);
        } else {
            g_warning("File does not exist: %s", file_path);
        }
    } else {
        g_print("Directory or invalid path, not opening\n");
    }

    g_free(file_path);
    file_opening_in_progress = FALSE;
}

// Function to refresh the file tree with current tab's directory
void refresh_file_tree_current(void) {
    if (!file_tree_store) {
        g_warning("refresh_file_tree_current: file_tree_store is NULL");
        return;
    }

    char *directory = get_current_tab_directory();
    if (!directory) {
        g_warning("refresh_file_tree_current: Could not get current directory");
        return;
    }

    schedule_tree_refresh(directory);
    show_file_browser_panel();

    g_free(directory);
}

// Function to refresh the file tree with specific directory
void refresh_file_tree(const char *directory) {
    if (!file_tree_store) {
        g_warning("refresh_file_tree: file_tree_store is NULL");
        return;
    }

    if (!directory) {
        g_warning("refresh_file_tree: directory is NULL");
        return;
    }

    schedule_tree_refresh(directory);
    show_file_browser_panel();
}

// Create the file tree view with ULTRA-COMPACT direct size control
GtkWidget* create_file_tree_view(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);  // Minimal spacing
    gtk_widget_set_margin_start(box, 6);    // Reduced margins
    gtk_widget_set_margin_end(box, 6);
    gtk_widget_set_margin_top(box, 6);
    gtk_widget_set_margin_bottom(box, 6);

    // Add header
    GtkWidget *header = gtk_label_new("<b>File Browser</b>");
    gtk_label_set_use_markup(GTK_LABEL(header), TRUE);
    gtk_label_set_xalign(GTK_LABEL(header), 0.0);
    gtk_box_append(GTK_BOX(box), header);

    // Add subtitle with shortcut hint
    GtkWidget *subtitle = gtk_label_new("<small>Double-click to open files</small>");
    gtk_label_set_use_markup(GTK_LABEL(subtitle), TRUE);
    gtk_label_set_xalign(GTK_LABEL(subtitle), 0.0);
    gtk_widget_set_opacity(subtitle, 0.7);
    gtk_box_append(GTK_BOX(box), subtitle);

    // Create scrolled window for tree view
    GtkWidget *scrolled_window = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled_window, TRUE);
    gtk_box_append(GTK_BOX(box), scrolled_window);

    // Create tree store with icon column
    file_tree_store = gtk_tree_store_new(N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
    if (!file_tree_store) {
        g_error("Failed to create file tree store");
        return box;
    }

    file_tree_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(file_tree_store)));
    if (!file_tree_view) {
        g_error("Failed to create file tree view");
        return box;
    }

    // Configure tree view for maximum compactness
    gtk_tree_view_set_headers_visible(file_tree_view, FALSE);
    gtk_tree_view_set_enable_tree_lines(file_tree_view, FALSE);
    gtk_tree_view_set_show_expanders(file_tree_view, TRUE);
    gtk_tree_view_set_level_indentation(file_tree_view, 8); // Very small indentation

    // Create ULTRA-SMALL icon renderer
    GtkCellRenderer *icon_renderer = gtk_cell_renderer_pixbuf_new();
    g_object_set(icon_renderer,
                 "width", 14,           // Very small fixed width
                 "height", 14,          // Very small fixed height
                 "xpad", 0,             // No horizontal padding
                 "ypad", 0,             // No vertical padding
                 "xalign", 0.0,         // Left align
                 "yalign", 0.5,         // Center vertically
                 NULL);

    // Create ultra-compact text renderer
    GtkCellRenderer *text_renderer = gtk_cell_renderer_text_new();
    g_object_set(text_renderer,
                 "xpad", 2,             // Minimal horizontal padding
                 "ypad", 0,             // No vertical padding
                 "font", "Sans 7",      // Very small font
                 "height", 16,          // Force very small row height
                 "yalign", 0.5,         // Center vertically
                 NULL);

    // Create combined column with MINIMAL spacing
    GtkTreeViewColumn *column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, "Files");
    gtk_tree_view_column_set_spacing(column, 1);    // Absolute minimal spacing
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);

    // Pack renderers with minimal space
    gtk_tree_view_column_pack_start(column, icon_renderer, FALSE);
    gtk_tree_view_column_add_attribute(column, icon_renderer, "icon-name", COLUMN_ICON);

    gtk_tree_view_column_pack_start(column, text_renderer, TRUE);
    gtk_tree_view_column_add_attribute(column, text_renderer, "text", COLUMN_NAME);

    // Add column to tree view
    gtk_tree_view_append_column(file_tree_view, column);

    // Apply ultra-compact styling
    apply_compact_styling_direct(GTK_WIDGET(file_tree_view));

    // Connect signals
    g_signal_connect(file_tree_view, "row-activated", G_CALLBACK(on_row_activated), NULL);
    g_signal_connect(file_tree_view, "row-expanded", G_CALLBACK(on_row_expanded), NULL);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), GTK_WIDGET(file_tree_view));

    return box;
}

// ULTRA-AGGRESSIVE compact styling function - NOW PROPERLY DECLARED
// ULTRA-AGGRESSIVE compact styling function - FIXED for GTK4
static void apply_compact_styling_direct(GtkWidget *tree_view) {
    GtkCssProvider *provider = gtk_css_provider_new();

    // Ultra-aggressive CSS that forces very small sizes
    const char *css =
        "treeview { "
        "  font-size: 7pt; "
        "  -GtkTreeView-vertical-separator: 0; "
        "  -GtkTreeView-horizontal-separator: 0; "
        "} "
        "treeview row { "
        "  min-height: 16px; "
        "  padding: 0px 2px; "
        "  margin: 0px; "
        "} "
        "treeview cell { "
        "  padding: 0px 1px; "
        "  margin: 0px; "
        "} "
        "treeview image { "
        "  min-width: 12px; "
        "  min-height: 12px; "
        "  padding: 0px; "
        "  margin: 1px; "
        "} "
        "treeview label { "
        "  font-size: 7pt; "
        "  padding: 0px; "
        "  margin: 0px; "
        "} ";

    // FIXED: GTK4 uses only 3 arguments - no error parameter, no return value
    gtk_css_provider_load_from_data(provider, css, -1);

    // Apply with maximum priority to override theme
    gtk_style_context_add_provider(gtk_widget_get_style_context(tree_view),
                                   GTK_STYLE_PROVIDER(provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);

    g_object_unref(provider);
}

#pragma GCC diagnostic pop
