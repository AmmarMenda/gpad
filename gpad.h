#ifndef GPAD_H
#define GPAD_H

#include <gtk/gtk.h>
#include <glib.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

/* Ensure C++ callers see unmangled symbols */
G_BEGIN_DECLS

/* Optional Tree-sitter integration */
#ifdef HAVE_TREE_SITTER
  #include <tree_sitter/api.h>
  typedef TSTree* TSTreePtr;          /* actual type when available */
#else
  typedef void*  TSTreePtr;           /* opaque placeholder otherwise */
#endif

/* Supported languages for editor logic */
typedef enum {
    LANG_C,
    LANG_PYTHON,
    LANG_DART,
    LANG_UNKNOWN
} LanguageType;

/* Per-tab state (compatible with GtkSourceView since it subclasses GtkTextView) */
typedef struct {
    GtkWidget     *scrolled_window;       /* page widget (e.g., scroller) */
    GtkWidget     *text_view;             /* editor widget (GtkTextView or GtkSourceView) */
    GtkTextBuffer *buffer;                /* GtkTextBuffer or GtkSourceBuffer (subclass) */
    char          *filename;              /* NULL for unsaved buffer */
    gboolean       dirty;                 /* unsaved changes marker */
    LanguageType   lang_type;             /* coarse language id for auxiliary features */

    /* Tree-sitter parse tree (unused if relying solely on GtkSourceView syntax) */
    TSTreePtr      ts_tree;

    /* NEW: auto-scroll behavior relative to caret */
    gboolean       auto_scroll_enabled;   /* default TRUE */
    double         auto_scroll_yalign;    /* e.g., 0.30 keeps caret near upper third */
    double         auto_scroll_within;    /* e.g., 0.10 margin to avoid jitter */

    /* Signal handler IDs and timers for robust cleanup */
    gulong         buffer_changed_handler;   /* “changed” on GtkTextBuffer */
    gulong         cursor_mark_handler;      /* “mark-set” for caret tracking */
    gulong         modified_close_handler;   /* “modified-changed” for close-after-save */
    guint          highlight_source_id;      /* per-tab debounce source id (0 if none) */
} TabInfo;

/* File tree columns */
enum {
    COLUMN_ICON,
    COLUMN_NAME,
    COLUMN_PATH,
    COLUMN_IS_DIR,
    N_COLUMNS
};

/* Globals */
extern GtkWidget        *global_window;
extern GtkNotebook      *global_notebook;
extern GtkWidget        *editor_stack;
extern GtkWidget        *welcome_screen;
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
/* Grammar symbols should be const TSLanguage* in C API examples */
const TSLanguage *tree_sitter_c(void);
const TSLanguage *tree_sitter_python(void);
const TSLanguage *tree_sitter_dart(void);
#endif

/* main.c */
void initialize_application(GtkApplication *app);
void cleanup_resources(void);
void show_welcome_screen(void);
void show_notebook(void);

/* welcome.c */
GtkWidget* create_welcome_screen(void);

/* tabs.c (GtkSourceView-based editor) */
void     create_new_tab(const char *filename);
void     create_new_tab_from_sidebar(const char *filename);
TabInfo* get_current_tab_info(void);
gboolean close_current_tab(void);
void     update_tab_label(TabInfo *tab_info);
void     setup_highlighting_tags(GtkTextBuffer *buffer);
void     on_tab_switched(GtkNotebook *notebook, GtkWidget *page, guint page_num, gpointer user_data);
/* Optional: toggle caret auto-scroll for current tab */
void     set_auto_scroll_enabled_current(gboolean enabled);

/* file_ops.c */
void         save_current_tab(void);
void         open_file_dialog(void);
void         add_to_recent_files(const char *filename);
LanguageType get_language_from_filename(const char *filename);

/* syntax.c (optional if using GtkSourceView syntax exclusively) */
void     highlight_buffer_sync(GtkTextBuffer *buffer, TSTreePtr *ts_tree, LanguageType lang);
gboolean highlight_timeout_callback(gpointer user_data);
void     init_tree_sitter(void);
void     cleanup_tree_sitter(void);

/* file_browser.c */
GtkWidget* create_file_tree_view(void);
void       refresh_file_tree(const char *directory);
void       refresh_file_tree_current(void);
void       populate_file_tree(GtkTreeStore *store, GtkTreeIter *parent, const char *path);

/* ui_panels.c */
GtkWidget* create_recent_files_panel(void);
void       show_recent_files_panel(void);
void       show_file_browser_panel(void);
void       hide_panels(void);
void       populate_recent_files(void);

/* actions.c */
void     setup_shortcuts(GtkApplication *app);
void     action_callback(GSimpleAction *action, GVariant *parameter, gpointer user_data);
gboolean is_sidebar_visible(void);
void     set_sidebar_visible(gboolean visible);
void     undo_current_tab(void);
void     redo_current_tab(void);

G_END_DECLS

#endif /* GPAD_H */
