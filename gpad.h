#ifndef GPAD_H
#define GPAD_H

#include <gtk/gtk.h>
#include <glib.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

G_BEGIN_DECLS

 
#ifdef HAVE_TREE_SITTER
  #include <tree_sitter/api.h>
  typedef TSTree *TSTreePtr;           
#else
  typedef void  *TSTreePtr;            
#endif

 
typedef enum {
    LANG_C,
    LANG_PYTHON,
    LANG_DART,
    LANG_UNKNOWN
} LanguageType;

 
typedef struct {
    GtkWidget     *scrolled_window;        
    GtkWidget     *text_view;              
    GtkTextBuffer *buffer;                 
    char          *filename;               
    gboolean       dirty;                  
    LanguageType   lang_type;              

     
    TSTreePtr      ts_tree;

     
    gboolean       auto_scroll_enabled;
    double         auto_scroll_yalign;
    double         auto_scroll_within;

     
    gulong         buffer_changed_handler;
    gulong         cursor_mark_handler;
    gulong         modified_close_handler;
    guint          highlight_source_id;
} TabInfo;

 
enum {
    COLUMN_ICON,
    COLUMN_NAME,
    COLUMN_PATH,
    COLUMN_IS_DIR,
    COLUMN_HIGHLIGHT,   
    N_COLUMNS
};

 
extern GtkWidget        *global_window;
extern GtkNotebook      *global_notebook;
extern GtkWidget        *editor_stack;
extern GtkWidget        *welcome_screen;
extern GtkWidget        *footer_label;
extern GtkTreeView      *file_tree_view;
extern GtkTreeStore     *file_tree_store;
extern GtkWidget        *side_panel;
extern GtkWidget        *recent_panel;
extern GtkWidget        *panel_container;
extern GtkListBox       *recent_list_box;
extern char             *current_directory;
extern GtkRecentManager *recent_manager;
extern gboolean          app_initialized;

#ifdef HAVE_TREE_SITTER
extern TSParser *ts_parser;
 
const TSLanguage *tree_sitter_c(void);
const TSLanguage *tree_sitter_python(void);
const TSLanguage *tree_sitter_dart(void);
#endif

 
/** Initializes the main application. */
void initialize_application(GtkApplication *app);
/** Cleans up resources. */
void cleanup_resources(void);
/** Shows the welcome screen. */
void show_welcome_screen(void);
/** Shows the notebook (tabs). */
void show_notebook(void);

 
/** Creates the welcome screen widget. */
GtkWidget* create_welcome_screen(void);

 
/** Creates a new tab. */
void     create_new_tab(const char *filename);
/** Creates a new tab from sidebar selection. */
void     create_new_tab_from_sidebar(const char *filename);
/** Returns info for current tab. */
TabInfo* get_current_tab_info(void);
/** Closes the current tab. */
gboolean close_current_tab(void);
/** Updates the tab's visual label. */
void     update_tab_label(TabInfo *tab_info);
/** Sets up highlighting tags for a buffer. */
void     setup_highlighting_tags(GtkTextBuffer *buffer);
/** Signal handler for tab switch. */
void     on_tab_switched(GtkNotebook *notebook, GtkWidget *page, guint page_num, gpointer user_data);
/** Enables/disables auto-scroll. */
void     set_auto_scroll_enabled_current(gboolean enabled);

 
/** Saves current tab. */
void         save_current_tab(void);
/** Opens file dialog. */
void         open_file_dialog(void);
/** Adds to recent files list. */
void         add_to_recent_files(const char *filename);
/** Detects language from filename. */
LanguageType get_language_from_filename(const char *filename);

 
/** Synchronously highlights a buffer. */
void     highlight_buffer_sync(GtkTextBuffer *buffer, TSTreePtr *ts_tree, LanguageType lang);
/** Timeout callback for highlighting. */
gboolean highlight_timeout_callback(gpointer user_data);
/** Initializes tree-sitter. */
void     init_tree_sitter(void);
/** Cleans up tree-sitter. */
void     cleanup_tree_sitter(void);

 
/** Creates the file tree view widget. */
GtkWidget* create_file_tree_view(void);
/** Refreshes file tree for generic directory. */
void       refresh_file_tree(const char *directory);
/** Refreshes file tree for current directory. */
void       refresh_file_tree_current(void);
/** Populates file tree. */
void       populate_file_tree(GtkTreeStore *store, GtkTreeIter *parent, const char *path);
/** Highlights a specific file in binary tree. */
void       highlight_current_file(const char *filepath);

 
/** Creates the recent files panel. */
GtkWidget* create_recent_files_panel(void);
/** Shows the recent files panel in sidebar. */
void       show_recent_files_panel(void);
/** Shows the file browser in sidebar. */
void       show_file_browser_panel(void);
/** Hides all sidebar panels. */
void       hide_panels(void);
/** Populates recent files list. */
void       populate_recent_files(void);

 
/** Sets up shortcuts. */
void     setup_shortcuts(GtkApplication *app);
/** Callback for actions. */
void     action_callback(GSimpleAction *action, GVariant *parameter, gpointer user_data);
/** Checks sidebar visibility. */
gboolean is_sidebar_visible(void);
/** Sets sidebar visibility. */
void     set_sidebar_visible(gboolean visible);
/** Undoes last action in current tab. */
void     undo_current_tab(void);
/** Redoes last undone action in current tab. */
void     redo_current_tab(void);

G_END_DECLS
#endif  
