#include "gpad.h"
#include "search.h"

/* Logical state (UI truth uses gtk_widget_is_visible) */
static gboolean sidebar_visible = FALSE;
typedef enum { SIDEBAR_NONE = 0, SIDEBAR_FILE_BROWSER, SIDEBAR_RECENT_FILES } SidebarType;
static SidebarType current_sidebar = SIDEBAR_NONE;

/* Undo/Redo */
void undo_current_tab(void) {
    TabInfo *t = get_current_tab_info();
    if (!t || !t->buffer) { g_print("No tab for undo\n"); return; }
    if (gtk_text_buffer_get_can_undo(t->buffer)) gtk_text_buffer_undo(t->buffer);
    else g_print("Nothing to undo\n");
}
void redo_current_tab(void) {
    TabInfo *t = get_current_tab_info();
    if (!t || !t->buffer) { g_print("No tab for redo\n"); return; }
    if (gtk_text_buffer_get_can_redo(t->buffer)) gtk_text_buffer_redo(t->buffer);
    else g_print("Nothing to redo\n");
}

/* Effective visibility for the whole sidebar container */
gboolean is_sidebar_visible(void) {
    return panel_container && gtk_widget_is_visible(panel_container);
}

/* Keep logical flag in sync; reset current type when hiding */
void set_sidebar_visible(gboolean visible) {
    sidebar_visible = visible;
    if (!visible) current_sidebar = SIDEBAR_NONE;
}

/* Actions */
void action_callback(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)parameter; (void)user_data;
    const char *name = action ? g_action_get_name(G_ACTION(action)) : "";
    if (strcmp(name, "new") == 0) {
        create_new_tab(NULL);
    } else if (strcmp(name, "open") == 0) {
        open_file_dialog();
    } else if (strcmp(name, "save") == 0) {
        save_current_tab();
    } else if (strcmp(name, "close") == 0) {
        close_current_tab();
    } else if (strcmp(name, "quit") == 0) {
        gtk_window_close(GTK_WINDOW(global_window));
    } else if (strcmp(name, "undo") == 0) {
        undo_current_tab();
    } else if (strcmp(name, "redo") == 0) {
        redo_current_tab();
    } else if (strcmp(name, "recent") == 0) {
        if (is_sidebar_visible() && current_sidebar == SIDEBAR_RECENT_FILES) {
            hide_panels();
            set_sidebar_visible(FALSE);
            current_sidebar = SIDEBAR_NONE;
        } else {
            show_recent_files_panel();
            set_sidebar_visible(TRUE);
            current_sidebar = SIDEBAR_RECENT_FILES;
        }
    } else if (strcmp(name, "browser") == 0) {
        if (is_sidebar_visible() && current_sidebar == SIDEBAR_FILE_BROWSER) {
            hide_panels();
            set_sidebar_visible(FALSE);
            current_sidebar = SIDEBAR_NONE;
        } else {
            show_file_browser_panel();
            refresh_file_tree_current();
            set_sidebar_visible(TRUE);
            current_sidebar = SIDEBAR_FILE_BROWSER;
        }
    } else if (strcmp(name, "find") == 0) {
        toggle_search_bar();
    }
}

/* Register actions and accelerators */
void setup_shortcuts(GtkApplication *app) {
    const GActionEntry entries[] = {
        {"new",    action_callback, NULL, NULL, NULL},
        {"open",   action_callback, NULL, NULL, NULL},
        {"save",   action_callback, NULL, NULL, NULL},
        {"close",  action_callback, NULL, NULL, NULL},
        {"quit",   action_callback, NULL, NULL, NULL},
        {"undo",   action_callback, NULL, NULL, NULL},
        {"redo",   action_callback, NULL, NULL, NULL},
        {"recent", action_callback, NULL, NULL, NULL},
        {"browser",action_callback, NULL, NULL, NULL},
        {"find",   action_callback, NULL, NULL, NULL},
    };
    g_action_map_add_action_entries(G_ACTION_MAP(app), entries, G_N_ELEMENTS(entries), app);

    gtk_application_set_accels_for_action(app, "app.new",    (const char*[]){"<primary>n", NULL});
    gtk_application_set_accels_for_action(app, "app.open",   (const char*[]){"<primary>o", NULL});
    gtk_application_set_accels_for_action(app, "app.save",   (const char*[]){"<primary>s", NULL});
    gtk_application_set_accels_for_action(app, "app.close",  (const char*[]){"<primary>w", NULL});
    gtk_application_set_accels_for_action(app, "app.quit",   (const char*[]){"<primary>q", NULL});
    gtk_application_set_accels_for_action(app, "app.undo",   (const char*[]){"<primary>z", NULL});
    gtk_application_set_accels_for_action(app, "app.redo",   (const char*[]){"<primary>y", NULL});
    gtk_application_set_accels_for_action(app, "app.recent", (const char*[]){"<primary>r", NULL});
    gtk_application_set_accels_for_action(app, "app.browser",(const char*[]){"<primary>b", NULL});
    gtk_application_set_accels_for_action(app, "app.find",   (const char*[]){"<primary>f", NULL});
}
