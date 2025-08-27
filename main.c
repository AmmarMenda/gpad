#include "gpad.h"

// Global variable definitions
GtkWidget *global_window = NULL;
GtkNotebook *global_notebook = NULL;
GtkWidget *editor_stack = NULL;
GtkWidget *welcome_screen = NULL;
GtkTreeView *file_tree_view = NULL;
GtkTreeStore *file_tree_store = NULL;
GtkWidget *side_panel = NULL;
GtkWidget *recent_panel = NULL;
GtkWidget *panel_container = NULL;
GtkListBox *recent_list_box = NULL;
char *current_directory = NULL;
GtkRecentManager *recent_manager = NULL;
gboolean app_initialized = FALSE;

#ifdef HAVE_TREE_SITTER
TSParser *ts_parser = NULL;
#endif

// Forward declarations for static functions used only in this file
static void on_page_removed(GtkNotebook *notebook, GtkWidget *child, guint page_num, gpointer user_data);
static gboolean update_after_tab_close(gpointer user_data);
static gboolean on_key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data);

// Show welcome screen
void show_welcome_screen(void) {
    if (editor_stack && welcome_screen) {
        gtk_stack_set_visible_child(GTK_STACK(editor_stack), welcome_screen);
        // IMPORTANT: Set focus to window to ensure shortcuts work
        gtk_widget_grab_focus(global_window);
        g_print("Showing welcome screen\n");
    }
}

// Show notebook (tabs area)
void show_notebook(void) {
    if (editor_stack && global_notebook) {
        gtk_stack_set_visible_child(GTK_STACK(editor_stack), GTK_WIDGET(global_notebook));
        g_print("Showing notebook\n");
    }
}

// Handle page removed from notebook - UPDATE SIDEBAR AND CHECK FOR WELCOME
static void on_page_removed(GtkNotebook *notebook, GtkWidget *child, guint page_num, gpointer user_data) {
    (void)child;
    (void)page_num;
    (void)user_data;

    g_print("Page removed, checking remaining tabs...\n");

    // Small delay to let GTK finish the removal process
    g_idle_add((GSourceFunc)update_after_tab_close, notebook);
}

// Deferred update after tab close - runs after GTK finishes page removal
static gboolean update_after_tab_close(gpointer user_data) {
    GtkNotebook *notebook = GTK_NOTEBOOK(user_data);

    int num_pages = gtk_notebook_get_n_pages(notebook);
    g_print("Number of pages remaining: %d\n", num_pages);

    if (num_pages == 0) {
        // No tabs left, show welcome screen and hide sidebar
        g_print("No tabs left - showing welcome screen\n");
        show_welcome_screen();
        hide_panels();
        set_sidebar_visible(FALSE);
    } else {
        // Get current active tab and update sidebar to its directory
        gint current_page = gtk_notebook_get_current_page(notebook);
        g_print("Current page after close: %d\n", current_page);

        if (current_page >= 0) {
            GtkWidget *page_widget = gtk_notebook_get_nth_page(notebook, current_page);
            if (page_widget) {
                TabInfo *info = (TabInfo*)g_object_get_data(G_OBJECT(page_widget), "tab_info");

                if (info && info->filename) {
                    g_print("Updating sidebar to directory of: %s\n", info->filename);
                    // Update sidebar to show directory of current file
                    if (is_sidebar_visible() && gtk_widget_get_visible(side_panel)) {
                        refresh_file_tree_current();
                    }
                } else {
                    g_print("Current tab has no filename, using home directory\n");
                    // Current tab has no file (new document), show home directory
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

// Key event handler for the main window - ENSURE SHORTCUTS ALWAYS WORK
static gboolean on_key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
    (void)controller;
    (void)keycode;
    (void)user_data;

    // Handle shortcuts manually if needed - this ensures they work regardless of focus
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
                // Create a fake action to trigger browser toggle
                GSimpleAction *fake_action = g_simple_action_new("browser", NULL);
                action_callback(fake_action, NULL, NULL);
                g_object_unref(fake_action);
                return TRUE;
            case GDK_KEY_r:
                g_print("Ctrl+R pressed - toggle recent files\n");
                // Create a fake action to trigger recent toggle
                GSimpleAction *fake_action2 = g_simple_action_new("recent", NULL);
                action_callback(fake_action2, NULL, NULL);
                g_object_unref(fake_action2);
                return TRUE;
        }
    }

    return FALSE; // Let other handlers process the event
}

// Initialize the main application window and components
void initialize_application(GtkApplication *app) {
    if (app_initialized) return;

    g_print("Initializing GPad editor...\n");

    // Create main window
    GtkWidget *window = gtk_application_window_new(app);
    global_window = window;
    gtk_window_set_title(GTK_WINDOW(window), "GPad - Multi-Tab Editor");
    gtk_window_set_default_size(GTK_WINDOW(window), 1200, 800);

    // IMPORTANT: Add key event controller to main window for global shortcuts
    GtkEventController *key_controller = gtk_event_controller_key_new();
    gtk_widget_add_controller(window, key_controller);
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(on_key_pressed), NULL);

    // Initialize recent manager
    recent_manager = gtk_recent_manager_get_default();

    // Create main layout with proper sizing
    GtkWidget *main_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_window_set_child(GTK_WINDOW(window), main_paned);

    // Create side panel container with minimum width
    panel_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(panel_container, 250, -1);
    gtk_paned_set_start_child(GTK_PANED(main_paned), panel_container);
    gtk_paned_set_shrink_start_child(GTK_PANED(main_paned), FALSE);
    gtk_paned_set_resize_start_child(GTK_PANED(main_paned), FALSE);

    // Create side panels
    side_panel = create_file_tree_view();
    gtk_box_append(GTK_BOX(panel_container), side_panel);

    recent_panel = create_recent_files_panel();
    gtk_box_append(GTK_BOX(panel_container), recent_panel);

    // Create editor area
    GtkWidget *editor_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_paned_set_end_child(GTK_PANED(main_paned), editor_box);
    gtk_paned_set_shrink_end_child(GTK_PANED(main_paned), FALSE);
    gtk_paned_set_resize_end_child(GTK_PANED(main_paned), TRUE);

    // Create stack to switch between welcome screen and notebook
    editor_stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(editor_stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(editor_stack), 200);
    gtk_widget_set_hexpand(editor_stack, TRUE);
    gtk_widget_set_vexpand(editor_stack, TRUE);
    gtk_box_append(GTK_BOX(editor_box), editor_stack);

    // Create welcome screen
    welcome_screen = create_welcome_screen();
    gtk_stack_add_named(GTK_STACK(editor_stack), welcome_screen, "welcome");

    // Create notebook for tabs
    global_notebook = GTK_NOTEBOOK(gtk_notebook_new());
    gtk_notebook_set_scrollable(global_notebook, TRUE);
    gtk_widget_set_hexpand(GTK_WIDGET(global_notebook), TRUE);
    gtk_widget_set_vexpand(GTK_WIDGET(global_notebook), TRUE);
    gtk_stack_add_named(GTK_STACK(editor_stack), GTK_WIDGET(global_notebook), "notebook");

    // Connect notebook signals
    g_signal_connect(global_notebook, "switch-page", G_CALLBACK(on_tab_switched), NULL);
    g_signal_connect(global_notebook, "page-removed", G_CALLBACK(on_page_removed), NULL);

    // Set initial paned position (300px for sidebar)
    gtk_paned_set_position(GTK_PANED(main_paned), 300);

    // Initially hide panels and show welcome screen
    gtk_widget_set_visible(panel_container, FALSE);
    show_welcome_screen();

#ifdef HAVE_TREE_SITTER
    init_tree_sitter();
#endif

// Apply dark theme CSS with MUCH MORE AGGRESSIVE compact styling
GtkCssProvider *css_provider = gtk_css_provider_new();
const char *css_data =
    "textview { "
    "  background-color: #1E1E1E; "
    "  color: #D4D4D4; "
    "  font-family: 'JetBrains Mono', 'Source Code Pro', 'Consolas', monospace; "
    "  font-size: 11pt; "
    "} "
    "textview.line-numbers { "
    "  background-color: #2D2D30; "
    "  color: #858585; "
    "  padding-right: 8px; "
    "  padding-left: 4px; "
    "  border-right: 1px solid #3E3E42; "
    "  font-size: 10pt; "
    "} "
    "notebook { "
    "  min-height: 30px; "
    "} "
    "notebook tab { "
    "  padding: 8px 12px; "
    "  min-width: 50px; "
    "  min-height: 24px; "
    "} "
    "notebook tab button { "
    "  min-width: 16px; "
    "  min-height: 16px; "
    "  margin-left: 6px; "
    "  padding: 2px; "
    "} "
    "notebook tab label { "
    "  min-height: 16px; "
    "  margin: 0; "
    "  padding: 0; "
    "} "
    "paned { "
    "  min-width: 100px; "
    "  min-height: 100px; "
    "} "
    "treeview { "
    "  background-color: #252526; "
    "  color: #CCCCCC; "
    "  font-size: 8pt; "              // MUCH smaller font
    "  min-height: 100px; "
    "} "
    "treeview:selected { "
    "  background-color: #094771; "
    "} "
    "treeview.compact-file-browser { "          // AGGRESSIVE compact styling
    "  font-size: 8pt !important; "            // Force small font
    "  -gtk-icon-size: 12px; "                 // Force very small icons
    "} "
    "treeview.compact-file-browser row { "      // VERY compact rows
    "  min-height: 18px !important; "          // Force small row height
    "  padding: 1px 2px !important; "          // Force minimal padding
    "  margin: 0 !important; "                 // Remove margins
    "} "
    "treeview.compact-file-browser cell { "     // VERY compact cells
    "  padding: 1px 2px !important; "          // Force minimal cell padding
    "  margin: 0 !important; "                 // Remove cell margins
    "} "
    "treeview.compact-file-browser image { "    // Small icons
    "  min-width: 12px !important; "           // Force small icon width
    "  min-height: 12px !important; "          // Force small icon height
    "  padding: 0 !important; "                // Remove icon padding
    "  margin: 2px !important; "               // Minimal icon margin
    "} "
    "treeview.compact-file-browser label { "    // Compact text
    "  padding: 0 2px !important; "            // Minimal text padding
    "  margin: 0 !important; "                 // Remove text margins
    "  font-size: 8pt !important; "            // Force small text
    "} "
    "listbox { "
    "  background-color: #252526; "
    "  min-height: 50px; "
    "} "
    "listbox row { "
    "  padding: 6px; "
    "  color: #CCCCCC; "
    "  min-height: 20px; "
    "} "
    "listbox row:hover { "
    "  background-color: #2A2D2E; "
    "} "
    "stack { "
    "  background-color: #1E1E1E; "
    "  min-width: 100px; "
    "  min-height: 100px; "
    "} "
    "box { "
    "  min-height: 0; "
    "  min-width: 0; "
    "}";

    gtk_css_provider_load_from_string(css_provider, css_data);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(css_provider);

    // Mark app as initialized
    app_initialized = TRUE;

    // Show window
    gtk_window_present(GTK_WINDOW(window));
    g_print("GPad editor initialized successfully.\n");
}
// Main application activation callback
static void activate(GtkApplication *app, gpointer user_data) {
    initialize_application(app);

    // Don't create initial tab - show welcome screen instead
    const char *filename = (const char *)user_data;
    if (filename && g_file_test(filename, G_FILE_TEST_EXISTS)) {
        create_new_tab(filename);
    }
    // If no filename, welcome screen is already showing
}

// Handle command line arguments
static void handle_command_line(GApplication *app, GApplicationCommandLine *cmdline, gpointer user_data) {
    (void)user_data; // Suppress unused parameter warning

    gchar **argv;
    gint argc;
    argv = g_application_command_line_get_arguments(cmdline, &argc);

    if (!app_initialized) {
        initialize_application(GTK_APPLICATION(app));
    }

    gboolean opened_file = FALSE;
    for (int i = 1; i < argc; i++) {
        if (g_file_test(argv[i], G_FILE_TEST_EXISTS)) {
            create_new_tab(argv[i]);
            opened_file = TRUE;
            g_print("Opening file from command line: %s\n", argv[i]);
        } else {
            g_warning("File does not exist: %s", argv[i]);
        }
    }

    // If no files were opened, make sure welcome screen is visible
    if (!opened_file) {
        show_welcome_screen();
    }

    g_strfreev(argv);
    if (global_window) {
        gtk_window_present(GTK_WINDOW(global_window));
    }
}

// Cleanup function
void cleanup_resources(void) {
#ifdef HAVE_TREE_SITTER
    cleanup_tree_sitter();
#endif
    g_free(current_directory);
    current_directory = NULL;
}

// Main function
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
