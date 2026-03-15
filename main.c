#include "gpad.h"
#include "search.h"


GtkWidget *global_window = NULL;
GtkNotebook *global_notebook = NULL;
GtkWidget *editor_stack = NULL;
GtkWidget *side_panel = NULL;
GtkWidget *recent_panel = NULL;
GtkWidget *panel_container = NULL;
GtkListBox *recent_list_box = NULL;
char *current_directory = NULL;
GtkRecentManager *recent_manager = NULL;
gboolean app_initialized = FALSE;
GtkWidget *footer_label = NULL;
static GtkCssProvider *current_css_provider = NULL;
static gboolean is_dark_mode = FALSE;

#ifdef HAVE_TREE_SITTER
TSParser *ts_parser = NULL;
#endif


static void on_page_removed(GtkNotebook *notebook, GtkWidget *child, guint page_num, gpointer user_data);
static gboolean update_after_tab_close(gpointer user_data);
static gboolean on_key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data);





/**
 * Switches the editor stack to show the notebook (tabs area).
 */
void show_notebook(void) {
    if (editor_stack && global_notebook) {
        gtk_stack_set_visible_child(GTK_STACK(editor_stack), GTK_WIDGET(global_notebook));
        g_print("Showing notebook\n");
    }
}


/**
 * Signal handler called when a page is removed from the notebook.
 * It schedules a deferred update to check if the welcome screen should be shown.
 */
static void on_page_removed(GtkNotebook *notebook, GtkWidget *child, guint page_num, gpointer user_data) {
    (void)child;
    (void)page_num;
    (void)user_data;

    g_print("Page removed, checking remaining tabs...\n");


    g_idle_add((GSourceFunc)update_after_tab_close, notebook);
}


/**
 * Deferred callback to update the UI state after a tab has been closed.
 * Handles showing the welcome screen if no tabs are left or updating the sidebar.
 */
static gboolean update_after_tab_close(gpointer user_data) {
    GtkNotebook *notebook = GTK_NOTEBOOK(user_data);

    int num_pages = gtk_notebook_get_n_pages(notebook);
    g_print("Number of pages remaining: %d\n", num_pages);

    if (num_pages == 0) {

        g_print("No tabs left - showing welcome screen\n");
        create_new_tab(NULL);
        hide_panels();
        set_sidebar_visible(FALSE);
        if (footer_label) {
            gtk_label_set_text(GTK_LABEL(footer_label), "");
        }
    } else {

        gint current_page = gtk_notebook_get_current_page(notebook);
        g_print("Current page after close: %d\n", current_page);

        if (current_page >= 0) {
            GtkWidget *page_widget = gtk_notebook_get_nth_page(notebook, current_page);
            if (page_widget) {
                TabInfo *info = (TabInfo*)g_object_get_data(G_OBJECT(page_widget), "tab_info");

                if (info && info->filename) {
                    g_print("Updating sidebar to directory of: %s\n", info->filename);

                    if (is_sidebar_visible() && gtk_widget_get_visible(side_panel)) {
                        refresh_file_tree_current();
                    }
                } else {
                    g_print("Current tab has no filename, using home directory\n");

                    if (is_sidebar_visible() && gtk_widget_get_visible(side_panel)) {
                        const char *home_dir = g_get_home_dir();
                        refresh_file_tree(home_dir);
                    }
                }
            }
        }
    }

    return G_SOURCE_REMOVE;
}


/**
 * Global key event handler for the main window to handle keyboard shortcuts.
 */
static gboolean on_key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
    (void)controller;
    (void)keycode;
    (void)user_data;


    if (state & GDK_CONTROL_MASK) {
        switch (keyval) {
            case GDK_KEY_n:
                g_print("Ctrl+N pressed - creating new tab\n");
                create_new_tab(NULL);
                return TRUE;
            case GDK_KEY_o:
                g_print("Ctrl+O pressed - opening file dialog\n");
                open_file_dialog();
                return TRUE;
            case GDK_KEY_s:
                g_print("Ctrl+S pressed - saving current tab\n");
                save_current_tab();
                return TRUE;
            case GDK_KEY_w:
                g_print("Ctrl+W pressed - closing current tab\n");
                close_current_tab();
                return TRUE;
            case GDK_KEY_q:
                g_print("Ctrl+Q pressed - quitting application\n");
                gtk_window_close(GTK_WINDOW(global_window));
                return TRUE;
            case GDK_KEY_z:
                g_print("Ctrl+Z pressed - undo\n");
                undo_current_tab();
                return TRUE;
            case GDK_KEY_y:
                g_print("Ctrl+Y pressed - redo\n");
                redo_current_tab();
                return TRUE;
            case GDK_KEY_b:
                g_print("Ctrl+B pressed - toggle file browser\n");

                GSimpleAction *fake_action = g_simple_action_new("browser", NULL);
                action_callback(fake_action, NULL, NULL);
                g_object_unref(fake_action);
                return TRUE;
            case GDK_KEY_r:
                g_print("Ctrl+R pressed - toggle recent files\n");

                GSimpleAction *fake_action2 = g_simple_action_new("recent", NULL);
                action_callback(fake_action2, NULL, NULL);
                g_object_unref(fake_action2);
                return TRUE;
        }
    }

    return FALSE;
}


/**
 * Loads and applies a CSS theme to the application.
 */
static void load_theme(const char *theme_path) {
    GdkDisplay *display = gdk_display_get_default();


    if (current_css_provider) {
        gtk_style_context_remove_provider_for_display(
            display,
            GTK_STYLE_PROVIDER(current_css_provider)
        );
        g_object_unref(current_css_provider);
        current_css_provider = NULL;
    }

    if (!theme_path) return;


    current_css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_path(current_css_provider, theme_path);

    gtk_style_context_add_provider_for_display(
        display,
        GTK_STYLE_PROVIDER(current_css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
}


/**
 * Callback for the theme toggle button to switch between light and dark modes.
 */
static void on_theme_toggle_clicked(GtkButton *button, gpointer user_data) {
    (void)user_data;
    is_dark_mode = !is_dark_mode;

    if (is_dark_mode) {
        load_theme("cyberpunk-theme.css");

        gtk_button_set_icon_name(button, "weather-clear-symbolic");
    } else {
        load_theme("old-macos-theme.css");

        gtk_button_set_icon_name(button, "weather-clear-night-symbolic");
    }
}


/**
 * Initializes the main application window, UI components, and layouts.
 */
void initialize_application(GtkApplication *app) {
    if (app_initialized) return;

    g_print("Initializing GPad editor...\n");


    GtkWidget *window = gtk_application_window_new(app);
    global_window = window;
    gtk_window_set_title(GTK_WINDOW(window), "GPad - Multi-Tab Editor");
    gtk_window_set_default_size(GTK_WINDOW(window), 1200, 800);


    GtkEventController *key_controller = gtk_event_controller_key_new();
    gtk_widget_add_controller(window, key_controller);
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(on_key_pressed), NULL);


    recent_manager = gtk_recent_manager_get_default();


    GtkWidget *main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(window), main_vbox);


    GtkWidget *main_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(main_vbox), main_paned);
    gtk_widget_set_hexpand(main_paned, TRUE);
    gtk_widget_set_vexpand(main_paned, TRUE);


    panel_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(panel_container, 250, -1);
    gtk_paned_set_start_child(GTK_PANED(main_paned), panel_container);
    gtk_paned_set_shrink_start_child(GTK_PANED(main_paned), FALSE);
    gtk_paned_set_resize_start_child(GTK_PANED(main_paned), FALSE);


    side_panel = create_file_tree_view();
    gtk_box_append(GTK_BOX(panel_container), side_panel);

    recent_panel = create_recent_files_panel();
    gtk_box_append(GTK_BOX(panel_container), recent_panel);


    GtkWidget *editor_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_paned_set_end_child(GTK_PANED(main_paned), editor_box);
    gtk_paned_set_shrink_end_child(GTK_PANED(main_paned), FALSE);
    gtk_paned_set_resize_end_child(GTK_PANED(main_paned), TRUE);


    editor_stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(editor_stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(editor_stack), 200);
    gtk_widget_set_hexpand(editor_stack, TRUE);
    gtk_widget_set_vexpand(editor_stack, TRUE);
    gtk_box_append(GTK_BOX(editor_box), editor_stack);




    global_notebook = GTK_NOTEBOOK(gtk_notebook_new());
    gtk_notebook_set_scrollable(global_notebook, TRUE);
    gtk_widget_set_hexpand(GTK_WIDGET(global_notebook), TRUE);
    gtk_widget_set_vexpand(GTK_WIDGET(global_notebook), TRUE);
    gtk_stack_add_named(GTK_STACK(editor_stack), GTK_WIDGET(global_notebook), "notebook");


    g_signal_connect(global_notebook, "switch-page", G_CALLBACK(on_tab_switched), NULL);
    g_signal_connect(global_notebook, "page-removed", G_CALLBACK(on_page_removed), NULL);


    gtk_paned_set_position(GTK_PANED(main_paned), 300);


    gtk_widget_set_visible(panel_container, FALSE);

#ifdef HAVE_TREE_SITTER
    init_tree_sitter();
#endif


    GtkWidget *search_bar = init_search_ui();
    gtk_box_append(GTK_BOX(main_vbox), search_bar);


    GtkWidget *footer_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_size_request(footer_box, -1, 32);
    gtk_widget_add_css_class(footer_box, "footer");
    gtk_box_append(GTK_BOX(main_vbox), footer_box);

    footer_label = gtk_label_new("");
    gtk_widget_set_margin_start(footer_label, 10);
    gtk_widget_set_margin_end(footer_label, 10);
    gtk_widget_set_hexpand(footer_label, TRUE);
    gtk_widget_set_halign(footer_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(footer_box), footer_label);



    GtkWidget *theme_btn = gtk_button_new_from_icon_name("weather-clear-night-symbolic");
    gtk_button_set_has_frame(GTK_BUTTON(theme_btn), FALSE);
    gtk_widget_set_tooltip_text(theme_btn, "Toggle Dark Mode");
    gtk_widget_set_margin_end(theme_btn, 5);
    gtk_widget_set_margin_top(theme_btn, 2);
    gtk_widget_set_margin_bottom(theme_btn, 2);
    g_signal_connect(theme_btn, "clicked", G_CALLBACK(on_theme_toggle_clicked), NULL);
    gtk_box_append(GTK_BOX(footer_box), theme_btn);


    load_theme("old-macos-theme.css");
    is_dark_mode = FALSE;


    app_initialized = TRUE;


    gtk_window_present(GTK_WINDOW(window));
    g_print("GPad editor initialized successfully.\n");
}

/**
 * Callback for the application 'activate' signal.
 */
static void activate(GtkApplication *app, gpointer user_data) {
    initialize_application(app);


    const char *filename = (const char *)user_data;
    if (filename && g_file_test(filename, G_FILE_TEST_EXISTS)) {
        create_new_tab(filename);
    }

}


/**
 * Handles command line arguments to open files when the application is launched.
 */
static void handle_command_line(GApplication *app, GApplicationCommandLine *cmdline, gpointer user_data) {
    (void)user_data;

    gchar **argv;
    gint argc;
    argv = g_application_command_line_get_arguments(cmdline, &argc);
    if (!app_initialized) {
        initialize_application(GTK_APPLICATION(app));
    }
    gboolean opened_file = FALSE;
    for (int i = 1; i < argc; i++) {
        if (g_file_test(argv[i], G_FILE_TEST_IS_REGULAR)) {
            create_new_tab(argv[i]);
            opened_file = TRUE;
            g_print("Opening file from command line: %s\n", argv[i]);
        } else {
            create_new_tab(argv[i]);
            opened_file = TRUE;
            g_print("GPad: Target path set to '%s'\n", argv[i]);
        }
    }
    if (!opened_file) {
        create_new_tab(NULL);
    }
    g_strfreev(argv);
    if (global_window) {
        gtk_window_present(GTK_WINDOW(global_window));
    }
}


/**
 * Cleans up application resources before exit.
 */
void cleanup_resources(void) {
#ifdef HAVE_TREE_SITTER
    cleanup_tree_sitter();
#endif
    g_free(current_directory);
    current_directory = NULL;
}


/**
 * Entry point of the application.
 */
int main(int argc, char **argv) {
    g_print("Starting GPad Multi-Tab Editor...\n");

    GtkApplication *app = gtk_application_new("org.gtk.gpad.multitab", G_APPLICATION_HANDLES_COMMAND_LINE);
    if (!app) {
        g_error("Failed to create GTK application");
        return 1;
    }

    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    g_signal_connect(app, "command-line", G_CALLBACK(handle_command_line), NULL);

    setup_shortcuts(app);

    int status = g_application_run(G_APPLICATION(app), argc, argv);

    cleanup_resources();
    g_object_unref(app);
    g_print("GPad editor exited with status: %d\n", status);
    return status;
}
