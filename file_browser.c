#include "gpad.h"
#include <glib/gstdio.h>
#include <pango/pango.h>

 


 
GtkTreeView  *file_tree_view  = NULL;
GtkTreeStore *file_tree_store = NULL;

 
#define MAX_RECURSE_DEPTH 2

 
static void setup_columns(GtkTreeView *tv);
static void apply_compact_renderers(GtkTreeViewColumn *col, GtkCellRenderer *text, GtkCellRenderer *icon);
static gboolean path_is_dir(const char *full);
static void populate_dir_recursive(GtkTreeStore *store, GtkTreeIter *parent, const char *path, int depth);
static void rebuild_for_directory(const char *dir_path);
static void on_row_activated(GtkTreeView *tv, GtkTreePath *tpath, GtkTreeViewColumn *col, gpointer user_data);

 
/**
 * Configures cell renderers for a compact display in the file tree.
 */
static void apply_compact_renderers(GtkTreeViewColumn *col, GtkCellRenderer *text, GtkCellRenderer *icon) {
    g_object_set(text, "xpad", 2, "ypad", 0, "ellipsize", PANGO_ELLIPSIZE_MIDDLE, NULL);
    PangoFontDescription *fd = pango_font_description_from_string("10pt");
    g_object_set(text, "font-desc", fd, NULL);
    pango_font_description_free(fd);
    gtk_cell_renderer_text_set_fixed_height_from_font(GTK_CELL_RENDERER_TEXT(text), 1);
    g_object_set(icon, "xpad", 1, "ypad", 0, "icon-size", GTK_ICON_SIZE_NORMAL, NULL);
}

 
/**
 * Data function for cell highlighting based on the current active file.
 */
static void highlight_cell_data_func(GtkTreeViewColumn *col, GtkCellRenderer *renderer,
                                     GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data) {
    gboolean highlight = FALSE;
    gtk_tree_model_get(model, iter, COLUMN_HIGHLIGHT, &highlight, -1);
    if (highlight)
        g_object_set(renderer, "cell-background", "#ffea82", "cell-background-set", TRUE, NULL);
    else
        g_object_set(renderer, "cell-background-set", FALSE, NULL);
}

 
/**
 * Sets up the columns for the file tree view.
 */
static void setup_columns(GtkTreeView *tv) {
    GtkTreeViewColumn *col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, "Name");
    GtkCellRenderer *icon = gtk_cell_renderer_pixbuf_new();
    GtkCellRenderer *text = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, icon, FALSE);
    gtk_tree_view_column_add_attribute(col, icon, "icon-name", COLUMN_ICON);
    gtk_tree_view_column_pack_start(col, text, TRUE);
    gtk_tree_view_column_add_attribute(col, text, "text", COLUMN_NAME);
    apply_compact_renderers(col, text, icon);
    gtk_tree_view_column_set_cell_data_func(col, text, highlight_cell_data_func, NULL, NULL);
    gtk_tree_view_append_column(tv, col);
    gtk_tree_view_set_fixed_height_mode(tv, TRUE);
}

 
/**
 * Checks if a given path is a directory.
 */
static gboolean path_is_dir(const char *full) {
    return full && g_file_test(full, G_FILE_TEST_IS_DIR);
}

/**
 * Returns the appropriate icon name for a file or directory.
 */
static const char* icon_name_for(const char *full, gboolean is_dir) {
    (void)full;
    return is_dir ? "folder" : "text-x-generic";
}

 
/**
 * Recursively populates the tree store with files and directories.
 */
static void populate_dir_recursive(GtkTreeStore *store, GtkTreeIter *parent, const char *path, int depth) {
    if (!path || depth > MAX_RECURSE_DEPTH) return;
    GError *err = NULL;
    GDir *dir = g_dir_open(path, 0, &err);
    if (!dir) { if (err) { g_warning("Open dir '%s' failed: %s", path, err->message); g_error_free(err);} return; }
    const gchar *name;
    while ((name = g_dir_read_name(dir)) != NULL) {
        if (name[0]=='.') continue;
        char *full = g_build_filename(path, name, NULL);
        gboolean is_dir = path_is_dir(full);
        GtkTreeIter it;
        gtk_tree_store_append(store, &it, parent);
        gtk_tree_store_set(store, &it,
            COLUMN_ICON, icon_name_for(full, is_dir),
            COLUMN_NAME, name,
            COLUMN_PATH, full,
            COLUMN_IS_DIR, is_dir,
            COLUMN_HIGHLIGHT, FALSE,
            -1);
        if (is_dir && depth < MAX_RECURSE_DEPTH)
            populate_dir_recursive(store, &it, full, depth + 1);
        g_free(full);
    }
    g_dir_close(dir);
}

 
/**
 * Rebuilds the entire file tree for a specific directory.
 */
static void rebuild_for_directory(const char *dir_path) {
    if (!file_tree_store || !dir_path) return;
    gtk_tree_store_clear(file_tree_store);
    GtkTreeIter root;
    gtk_tree_store_append(file_tree_store, &root, NULL);
    char *base = g_path_get_basename(dir_path);
    gtk_tree_store_set(file_tree_store, &root,
        COLUMN_ICON, "folder",
        COLUMN_NAME, base,
        COLUMN_PATH, dir_path,
        COLUMN_IS_DIR, TRUE,
        COLUMN_HIGHLIGHT, FALSE,
        -1);
    g_free(base);
    populate_dir_recursive(file_tree_store, &root, dir_path, 1);
}

 
/**
 * Callback for when a row in the file tree is activated (double-clicked or Enter).
 */
static void on_row_activated(GtkTreeView *tv, GtkTreePath *tpath, GtkTreeViewColumn *col, gpointer user_data) {
    GtkTreeModel *model = gtk_tree_view_get_model(tv);
    GtkTreeIter it;
    if (!gtk_tree_model_get_iter(model, &it, tpath)) return;
    gchar *path = NULL;
    gboolean is_dir = FALSE;
    gtk_tree_model_get(model, &it, COLUMN_PATH, &path, COLUMN_IS_DIR, &is_dir, -1);
    if (!path) return;
    if (is_dir) {
        if (gtk_tree_view_row_expanded(tv, tpath))
            gtk_tree_view_collapse_row(tv, tpath);
        else
            gtk_tree_view_expand_row(tv, tpath, FALSE);
    } else {
        create_new_tab_from_sidebar(path);  
    }
    g_free(path);
}

 
/**
 * Creates the file tree view widget and its container.
 */
GtkWidget* create_file_tree_view(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_start(box, 6);
    gtk_widget_set_margin_end(box, 6);
    gtk_widget_set_margin_top(box, 6);
    gtk_widget_set_margin_bottom(box, 6);
    GtkWidget *hdr = gtk_label_new("Files");
    gtk_widget_add_css_class(hdr, "heading");
    gtk_box_append(GTK_BOX(box), hdr);
    file_tree_store = gtk_tree_store_new(N_COLUMNS,
        G_TYPE_STRING,    
        G_TYPE_STRING,    
        G_TYPE_STRING,    
        G_TYPE_BOOLEAN,   
        G_TYPE_BOOLEAN    
    );
    GtkWidget *scroller = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroller, TRUE);
    gtk_widget_set_hexpand(scroller, TRUE);
    gtk_box_append(GTK_BOX(box), scroller);
    file_tree_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(file_tree_store)));
    gtk_widget_add_css_class(GTK_WIDGET(file_tree_view), "compact-browser");
    gtk_tree_view_set_show_expanders(file_tree_view, TRUE);
    gtk_tree_view_set_level_indentation(file_tree_view, 6);
    gtk_tree_view_set_headers_visible(file_tree_view, FALSE);
    gtk_tree_view_set_enable_search(file_tree_view, TRUE);
    gtk_tree_view_set_activate_on_single_click(file_tree_view, TRUE);
    setup_columns(file_tree_view);
    g_signal_connect(file_tree_view, "row-activated", G_CALLBACK(on_row_activated), NULL);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), GTK_WIDGET(file_tree_view));
    return box;
}

 
/**
 * Refreshes the file tree to show the contents of a given directory.
 */
void refresh_file_tree(const char *directory) {
    const char *dir = (directory && *directory) ? directory : g_get_home_dir();
    rebuild_for_directory(dir);
}

 
/**
 * Refreshes the file tree based on the current active tab's directory or the last known directory.
 */
void refresh_file_tree_current(void) {
    TabInfo *tab = get_current_tab_info();
    if (tab && tab->filename && *tab->filename) {
        char *d = g_path_get_dirname(tab->filename);
        rebuild_for_directory(d);
        g_free(d);
        return;
    }
    const char *dir = (current_directory && *current_directory) ? current_directory : g_get_home_dir();
    rebuild_for_directory(dir);
}

 
/**
 * Helper to start recursive population of the file tree.
 */
void populate_file_tree(GtkTreeStore *store, GtkTreeIter *parent, const char *path) {
    populate_dir_recursive(store, parent, path, 1);
}

 
/**
 * Clears the highlight status from all items in the tree.
 */
static void clear_current_file_highlight(GtkTreeModel *model, GtkTreeIter *iter) {
    GtkTreeIter child;
    gboolean has_child = gtk_tree_model_iter_children(model, &child, iter);
    while (has_child) {
        gtk_tree_store_set(GTK_TREE_STORE(model), &child, COLUMN_HIGHLIGHT, FALSE, -1);
        clear_current_file_highlight(model, &child);
        has_child = gtk_tree_model_iter_next(model, &child);
    }
}

/**
 * Recursively finds a file in the tree and sets its highlight status.
 */
static gboolean find_and_highlight_file(GtkTreeModel *model, GtkTreeIter *iter, const char *filepath) {
    GtkTreeIter child;
    gboolean has_child = gtk_tree_model_iter_children(model, &child, iter);
    while (has_child) {
        gchar *path = NULL;
        gtk_tree_model_get(model, &child, COLUMN_PATH, &path, -1);
        if (path && filepath && strcmp(path, filepath) == 0) {
            gtk_tree_store_set(GTK_TREE_STORE(model), &child, COLUMN_HIGHLIGHT, TRUE, -1);
            g_free(path);
            return TRUE;
        }
        g_free(path);
        if (find_and_highlight_file(model, &child, filepath))
            return TRUE;
        has_child = gtk_tree_model_iter_next(model, &child);
    }
    return FALSE;
}

/**
 * Highlights the entry in the file tree corresponding to the given file path.
 */
void highlight_current_file(const char *filepath) {
    if (!file_tree_view || !file_tree_store) return;
    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(file_tree_store), &iter);
    while (valid) {
        clear_current_file_highlight(GTK_TREE_MODEL(file_tree_store), &iter);
        if (find_and_highlight_file(GTK_TREE_MODEL(file_tree_store), &iter, filepath))
            break;
        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(file_tree_store), &iter);
    }
}
