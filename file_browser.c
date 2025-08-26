#include "gpad.h"

// Suppress deprecation warnings for tree view functions
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

// Populate the file tree
void populate_file_tree(GtkTreeStore *store, GtkTreeIter *parent, const char *path) {
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
void refresh_file_tree(const char *directory) {
    if (!file_tree_store || !directory) return;
    
    gtk_tree_store_clear(file_tree_store);
    g_free(current_directory);
    current_directory = g_strdup(directory);
    
    populate_file_tree(file_tree_store, NULL, directory);
    show_file_browser_panel();
}

// Create the file tree view
GtkWidget* create_file_tree_view(void) {
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
