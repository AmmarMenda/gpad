#include "gpad.h"

// Track sidebar state and which panel is currently showing
static gboolean sidebar_visible = FALSE;
typedef enum {
    SIDEBAR_NONE,
    SIDEBAR_FILE_BROWSER,
    SIDEBAR_RECENT_FILES
} SidebarType;
static SidebarType current_sidebar = SIDEBAR_NONE;

// Undo current tab
void undo_current_tab(void) {
    TabInfo *tab_info = get_current_tab_info();
    if (!tab_info || !tab_info->buffer) {
        g_print("No tab available for undo\n");
        return;
    }

    if (gtk_text_buffer_get_can_undo(tab_info->buffer)) {
        gtk_text_buffer_undo(tab_info->buffer);
        g_print("Undo performed\n");
    } else {
        g_print("Nothing to undo\n");
    }
}

// Redo current tab
void redo_current_tab(void) {
    TabInfo *tab_info = get_current_tab_info();
    if (!tab_info || !tab_info->buffer) {
        g_print("No tab available for redo\n");
        return;
    }

    if (gtk_text_buffer_get_can_redo(tab_info->buffer)) {
        gtk_text_buffer_redo(tab_info->buffer);
        g_print("Redo performed\n");
    } else {
        g_print("Nothing to redo\n");
    }
}

// Action callback for menu items and shortcuts
void action_callback(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)parameter;
    (void)user_data;

    const char *action_name = action ? g_action_get_name(G_ACTION(action)) : "";
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
    } else if (strcmp(action_name, "undo") == 0) {
        undo_current_tab();
    } else if (strcmp(action_name, "redo") == 0) {
        redo_current_tab();
    } else if (strcmp(action_name, "recent") == 0) {
        // Toggle recent files panel
        if (sidebar_visible && current_sidebar == SIDEBAR_RECENT_FILES) {
            hide_panels();
            sidebar_visible = FALSE;
            current_sidebar = SIDEBAR_NONE;
        } else {
            show_recent_files_panel();
            sidebar_visible = TRUE;
            current_sidebar = SIDEBAR_RECENT_FILES;
        }
    } else if (strcmp(action_name, "browser") == 0) {
        // Toggle file browser panel
        if (sidebar_visible && current_sidebar == SIDEBAR_FILE_BROWSER) {
            hide_panels();
            sidebar_visible = FALSE;
            current_sidebar = SIDEBAR_NONE;
        } else {
            refresh_file_tree_current();
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

// Setup keyboard shortcuts and actions
void setup_shortcuts(GtkApplication *app) {
    // Fixed GActionEntry array with proper field initialization
    const GActionEntry app_entries[] = {
        {"new", action_callback, NULL, NULL, NULL, {0, 0, 0}},
        {"open", action_callback, NULL, NULL, NULL, {0, 0, 0}},
        {"save", action_callback, NULL, NULL, NULL, {0, 0, 0}},
        {"close", action_callback, NULL, NULL, NULL, {0, 0, 0}},
        {"quit", action_callback, NULL, NULL, NULL, {0, 0, 0}},
        {"undo", action_callback, NULL, NULL, NULL, {0, 0, 0}},
        {"redo", action_callback, NULL, NULL, NULL, {0, 0, 0}},
        {"recent", action_callback, NULL, NULL, NULL, {0, 0, 0}},
        {"browser", action_callback, NULL, NULL, NULL, {0, 0, 0}}
    };

    g_action_map_add_action_entries(G_ACTION_MAP(app), app_entries, G_N_ELEMENTS(app_entries), app);

    // IMPORTANT: Set keyboard shortcuts at APPLICATION level, not window level
    gtk_application_set_accels_for_action(app, "app.new", (const char*[]){"<Control>n", NULL});
    gtk_application_set_accels_for_action(app, "app.open", (const char*[]){"<Control>o", NULL});
    gtk_application_set_accels_for_action(app, "app.save", (const char*[]){"<Control>s", NULL});
    gtk_application_set_accels_for_action(app, "app.close", (const char*[]){"<Control>w", NULL});
    gtk_application_set_accels_for_action(app, "app.quit", (const char*[]){"<Control>q", NULL});
    gtk_application_set_accels_for_action(app, "app.undo", (const char*[]){"<Control>z", NULL});
    gtk_application_set_accels_for_action(app, "app.redo", (const char*[]){"<Control>y", NULL});
    gtk_application_set_accels_for_action(app, "app.recent", (const char*[]){"<Control>r", NULL});
    gtk_application_set_accels_for_action(app, "app.browser", (const char*[]){"<Control>b", NULL});

    g_print("Application shortcuts set up successfully\n");
}
