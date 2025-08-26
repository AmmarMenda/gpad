#include "gpad.h"

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
        show_recent_files_panel();
    } else if (strcmp(action_name, "browser") == 0) {
        const char *home_dir = g_get_home_dir();
        refresh_file_tree(home_dir);
    }
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
