#include "gpad.h"

// Global variable definitions
GtkWidget *global_window = NULL;
GtkNotebook *global_notebook = NULL;
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

// Initialize the main application window and components
void initialize_application(GtkApplication *app) {
    if (app_initialized) return;  // Prevent double initialization

    g_print("Initializing GPad editor...\n");

    // Create main window
    GtkWidget *window = gtk_application_window_new(app);
    global_window = window;
    gtk_window_set_title(GTK_WINDOW(window), "GPad - Multi-Tab Editor");
    gtk_window_set_default_size(GTK_WINDOW(window), 1200, 800);

    // Initialize recent manager
    recent_manager = gtk_recent_manager_get_default();

    // Create main layout
    GtkWidget *main_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_window_set_child(GTK_WINDOW(window), main_paned);

    // Create side panel container
    panel_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_paned_set_start_child(GTK_PANED(main_paned), panel_container);

    // Create side panels
    side_panel = create_file_tree_view();
    gtk_box_append(GTK_BOX(panel_container), side_panel);

    recent_panel = create_recent_files_panel();
    gtk_box_append(GTK_BOX(panel_container), recent_panel);

    // Create editor area
    GtkWidget *editor_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_paned_set_end_child(GTK_PANED(main_paned), editor_box);

    // Create notebook for tabs
    global_notebook = GTK_NOTEBOOK(gtk_notebook_new());
    gtk_notebook_set_scrollable(global_notebook, TRUE);
    gtk_widget_set_hexpand(GTK_WIDGET(global_notebook), TRUE);
    gtk_widget_set_vexpand(GTK_WIDGET(global_notebook), TRUE);
    gtk_box_append(GTK_BOX(editor_box), GTK_WIDGET(global_notebook));

    // Set initial paned position and hide panels
    gtk_paned_set_position(GTK_PANED(main_paned), 280);
    gtk_widget_set_visible(panel_container, FALSE);

#ifdef HAVE_TREE_SITTER
    init_tree_sitter();
#endif

    // Apply dark theme CSS
    GtkCssProvider *css_provider = gtk_css_provider_new();
    const char *css_data =
        "textview { "
        "  background-color: #1E1E1E; "
        "  color: #D4D4D4; "
        "  font-family: 'JetBrains Mono', 'Source Code Pro', 'Consolas', monospace; "
        "  font-size: 11pt; "
        "} "
        "notebook tab { "
        "  padding: 8px 12px; "
        "} "
        "notebook tab button { "
        "  min-width: 16px; "
        "  min-height: 16px; "
        "  margin-left: 6px; "
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

    const char *filename = (const char *)user_data;
    if (filename && g_file_test(filename, G_FILE_TEST_EXISTS)) {
        create_new_tab(filename);
    } else {
        create_new_tab(NULL);
    }
}

// Handle command line arguments
static void handle_command_line(GApplication *app, GApplicationCommandLine *cmdline, gpointer user_data) {
    (void)user_data; // Suppress unused parameter warning

    gchar **argv;
    gint argc;
    argv = g_application_command_line_get_arguments(cmdline, &argc);

    if (!app_initialized) {
        initialize_application(GTK_APPLICATION(app));
        create_new_tab(NULL);
    }

    for (int i = 1; i < argc; i++) {
        if (g_file_test(argv[i], G_FILE_TEST_EXISTS)) {
            create_new_tab(argv[i]);
            g_print("Opening file from command line: %s\n", argv[i]);
        } else {
            g_warning("File does not exist: %s", argv[i]);
        }
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
