#include "gpad.h"

// Track sidebar state and which panel is currently showing
static gboolean sidebar_visible = FALSE;
typedef enum {
    SIDEBAR_NONE,
    SIDEBAR_FILE_BROWSER,
    SIDEBAR_RECENT_FILES
} SidebarType;
static SidebarType current_sidebar = SIDEBAR_NONE;

// Action callback for menu items and shortcuts
void action_callback(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
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
        // Toggle recent files panel
        if (sidebar_visible && current_sidebar == SIDEBAR_RECENT_FILES) {
            // Recent files panel is currently showing, hide it
            hide_panels();
            sidebar_visible = FALSE;
            current_sidebar = SIDEBAR_NONE;
        } else {
            // Either no sidebar is showing or file browser is showing
            // Show recent files panel
            show_recent_files_panel();
            sidebar_visible = TRUE;
            current_sidebar = SIDEBAR_RECENT_FILES;
        }
    } else if (strcmp(action_name, "browser") == 0) {
        // Toggle file browser panel
        if (sidebar_visible && current_sidebar == SIDEBAR_FILE_BROWSER) {
            // File browser is currently showing, hide it
            hide_panels();
            sidebar_visible = FALSE;
            current_sidebar = SIDEBAR_NONE;
        } else {
            // Either no sidebar is showing or recent files is showing
            // Show file browser panel
            const char *home_dir = g_get_home_dir();
            refresh_file_tree(home_dir);
            sidebar_visible = TRUE;
            current_sidebar = SIDEBAR_FILE_BROWSER;
        }
    }
}

// Get sidebar visibility state
gboolean is_sidebar_visible(void) {
    return sidebar_visible && gtk_widget_get_visible(panel_container);
}

// Set sidebar visibility state
void set_sidebar_visible(gboolean visible) {
    sidebar_visible = visible;
    if (!visible) {
        current_sidebar = SIDEBAR_NONE;
    }
}

// Get current sidebar type
SidebarType get_current_sidebar_type(void) {
    return current_sidebar;
}

// Setup keyboard shortcuts and actions
void setup_shortcuts(GtkApplication *app) {
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
