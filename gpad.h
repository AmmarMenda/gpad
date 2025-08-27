#ifndef GPAD_H
#define GPAD_H

#include <gtk/gtk.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

// Make tree-sitter optional to avoid linking issues
#ifdef HAVE_TREE_SITTER
#include <tree_sitter/api.h>
// Use the actual TSTree type when available
typedef TSTree* TSTreePtr;
#else
// Use void* as a placeholder when tree-sitter is not available
typedef void* TSTreePtr;
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
    TSTreePtr ts_tree;  // This works regardless of tree-sitter availability
    void *line_number_data;  // NEW: Line number data for cleanup
} TabInfo;

// Column enumeration for TreeView
enum {
    COLUMN_ICON,
    COLUMN_NAME,
    COLUMN_PATH,
    COLUMN_IS_DIR,
    N_COLUMNS
};

// Global references
extern GtkWidget *global_window;
extern GtkNotebook *global_notebook;
extern GtkWidget *editor_stack;  // Stack to switch between welcome and notebook
extern GtkWidget *welcome_screen; // Welcome screen widget
extern GtkTreeView *file_tree_view;
extern GtkTreeStore *file_tree_store;
extern GtkWidget *side_panel;
extern GtkWidget *recent_panel;
extern GtkWidget *panel_container;
extern GtkListBox *recent_list_box;
extern char *current_directory;
extern GtkRecentManager *recent_manager;
extern gboolean app_initialized;

#ifdef HAVE_TREE_SITTER
extern TSParser *ts_parser;
// Forward declarations for grammar parsers
TSLanguage *tree_sitter_c(void);
TSLanguage *tree_sitter_python(void);
TSLanguage *tree_sitter_dart(void);
#endif

// Function declarations from different modules
// main.c
void initialize_application(GtkApplication *app);
void cleanup_resources(void);
void show_welcome_screen(void);
void show_notebook(void);

// welcome.c
GtkWidget* create_welcome_screen(void);

// line_numbers.c - NEW
GtkWidget* create_line_numbers_for_textview(GtkWidget *text_view, TabInfo *tab_info);
void cleanup_line_numbers(TabInfo *tab_info);

// tabs.c
void create_new_tab(const char *filename);
void create_new_tab_from_sidebar(const char *filename);
TabInfo* get_current_tab_info(void);
gboolean close_current_tab(void);
void update_tab_label(TabInfo *tab_info);
void setup_highlighting_tags(GtkTextBuffer *buffer);
void on_tab_switched(GtkNotebook *notebook, GtkWidget *page, guint page_num, gpointer user_data);
void check_tabs_and_show_welcome(void);

// file_ops.c
void save_current_tab(void);
void open_file_dialog(void);
void add_to_recent_files(const char *filename);
LanguageType get_language_from_filename(const char *filename);

// syntax.c - Using TSTreePtr which is compatible with both cases
void highlight_buffer_sync(GtkTextBuffer *buffer, TSTreePtr *ts_tree, LanguageType lang);
gboolean highlight_timeout_callback(gpointer user_data);
void init_tree_sitter(void);
void cleanup_tree_sitter(void);

// file_browser.c
GtkWidget* create_file_tree_view(void);
void refresh_file_tree(const char *directory);
void refresh_file_tree_current(void);
void populate_file_tree(GtkTreeStore *store, GtkTreeIter *parent, const char *path);

// ui_panels.c
GtkWidget* create_recent_files_panel(void);
void show_recent_files_panel(void);
void show_file_browser_panel(void);
void hide_panels(void);
void populate_recent_files(void);

// actions.c
void setup_shortcuts(GtkApplication *app);
void action_callback(GSimpleAction *action, GVariant *parameter, gpointer user_data);
gboolean is_sidebar_visible(void);
void set_sidebar_visible(gboolean visible);
void undo_current_tab(void);
void redo_current_tab(void);

#endif // GPAD_H
