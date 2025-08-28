#include "gpad.h"

// Structure to hold line number data for each tab
typedef struct {
    GtkWidget *line_numbers_view;
    GtkTextBuffer *line_numbers_buffer;
    GtkTextBuffer *main_buffer;
    gulong buffer_changed_handler;
    gulong mark_set_handler;
    int last_line_count;  // Cache to avoid unnecessary updates
} LineNumberData;

// Calculate number of digits needed for line count
static int calculate_line_number_width(int line_count) {
    int digits = 1;
    int temp = line_count;
    while (temp >= 10) {
        digits++;
        temp /= 10;
    }
    return MAX(digits, 3); // Minimum 3 digits for consistent width
}

// Synchronize font and text properties using CSS (GTK4 compatible)
static void synchronize_text_views(GtkWidget *line_view, GtkWidget *main_view) {
    // Ensure both use monospace fonts
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(line_view), TRUE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(main_view), TRUE);

    // Create CSS provider for consistent font styling
    GtkCssProvider *provider = gtk_css_provider_new();
    const char *css =
        "textview { "
        "  font-family: 'JetBrains Mono', 'Source Code Pro', 'Consolas', monospace; "
        "  font-size: 11pt; "
        "  line-height: 1.2; "
        "} ";

    gtk_css_provider_load_from_data(provider, css, -1);

    // Apply CSS to both views for consistent fonts
    gtk_style_context_add_provider(gtk_widget_get_style_context(line_view),
                                   GTK_STYLE_PROVIDER(provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    gtk_style_context_add_provider(gtk_widget_get_style_context(main_view),
                                   GTK_STYLE_PROVIDER(provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_object_unref(provider);

    // Synchronize line spacing - CRITICAL for alignment
    gtk_text_view_set_pixels_above_lines(GTK_TEXT_VIEW(line_view), 0);
    gtk_text_view_set_pixels_below_lines(GTK_TEXT_VIEW(line_view), 0);
    gtk_text_view_set_pixels_inside_wrap(GTK_TEXT_VIEW(line_view), 0);

    gtk_text_view_set_pixels_above_lines(GTK_TEXT_VIEW(main_view), 0);
    gtk_text_view_set_pixels_below_lines(GTK_TEXT_VIEW(main_view), 0);
    gtk_text_view_set_pixels_inside_wrap(GTK_TEXT_VIEW(main_view), 0);

    // Synchronize margins for perfect alignment
    int top_margin = gtk_text_view_get_top_margin(GTK_TEXT_VIEW(main_view));
    int bottom_margin = gtk_text_view_get_bottom_margin(GTK_TEXT_VIEW(main_view));

    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(line_view), top_margin);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(line_view), bottom_margin);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(line_view), 4);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(line_view), 8);
}

// Update line numbers content with precise formatting
static void update_line_numbers(GtkTextBuffer *buffer, gpointer user_data) {
    LineNumberData *ln_data = (LineNumberData*)user_data;

    if (!ln_data || !ln_data->line_numbers_buffer) return;

    int line_count = gtk_text_buffer_get_line_count(buffer);

    // Skip if line count hasn't changed (performance optimization)
    if (line_count == ln_data->last_line_count) return;

    ln_data->last_line_count = line_count;

    g_print("Updating line numbers for %d lines\n", line_count);

    // Calculate consistent width for all line numbers
    int width = calculate_line_number_width(line_count);

    GString *numbers = g_string_new("");

    // Generate line numbers with consistent formatting
    for (int i = 1; i <= line_count; i++) {
        if (i == line_count) {
            // CRITICAL: Last line has no trailing newline to match main buffer exactly
            g_string_append_printf(numbers, "%*d", width, i);
        } else {
            g_string_append_printf(numbers, "%*d\n", width, i);
        }
    }

    // Block signals to prevent recursion during buffer update
    g_signal_handlers_block_by_func(ln_data->line_numbers_buffer, update_line_numbers, ln_data);
    gtk_text_buffer_set_text(ln_data->line_numbers_buffer, numbers->str, -1);
    g_signal_handlers_unblock_by_func(ln_data->line_numbers_buffer, update_line_numbers, ln_data);

    // Dynamically adjust line number view width
    int char_width = 8; // Approximate monospace character width in pixels
    int needed_width = (width + 2) * char_width + 12; // +2 for padding, +12 for margins
    gtk_widget_set_size_request(ln_data->line_numbers_view, needed_width, -1);

    g_string_free(numbers, TRUE);
}

// Handle cursor position changes to highlight current line
static void on_mark_set(GtkTextBuffer *buffer, GtkTextIter *iter, GtkTextMark *mark, gpointer user_data) {
    (void)iter; // Suppress unused parameter warning

    LineNumberData *ln_data = (LineNumberData*)user_data;

    if (!ln_data || !ln_data->line_numbers_buffer) return;

    // Only handle cursor position changes (insert mark)
    if (mark != gtk_text_buffer_get_insert(buffer)) return;

    // Get current line number
    GtkTextIter cursor_iter;
    gtk_text_buffer_get_iter_at_mark(buffer, &cursor_iter, mark);
    int current_line = gtk_text_iter_get_line(&cursor_iter);

    // Remove existing highlighting from all lines
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(ln_data->line_numbers_buffer, &start, &end);
    gtk_text_buffer_remove_tag_by_name(ln_data->line_numbers_buffer, "current-line", &start, &end);

    // Highlight current line number
    if (current_line >= 0) {
        GtkTextIter line_start, line_end;
        if (gtk_text_buffer_get_iter_at_line(ln_data->line_numbers_buffer, &line_start, current_line)) {
            line_end = line_start;
            if (!gtk_text_iter_ends_line(&line_end)) {
                gtk_text_iter_forward_to_line_end(&line_end);
            }
            gtk_text_buffer_apply_tag_by_name(ln_data->line_numbers_buffer, "current-line", &line_start, &line_end);
        }
    }
}

// Create line numbers widget with PERFECT synchronization
GtkWidget* create_line_numbers_for_textview(GtkWidget *text_view, TabInfo *tab_info) {
    if (!text_view || !tab_info) return NULL;

    // Create horizontal container
    GtkWidget *container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    // Create line numbers view
    GtkWidget *line_numbers_view = gtk_text_view_new();
    GtkTextBuffer *line_numbers_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(line_numbers_view));

    // Configure line numbers view
    gtk_text_view_set_editable(GTK_TEXT_VIEW(line_numbers_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(line_numbers_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(line_numbers_view), GTK_WRAP_NONE);
    gtk_text_view_set_accepts_tab(GTK_TEXT_VIEW(line_numbers_view), FALSE);

    // Set initial size
    gtk_widget_set_size_request(line_numbers_view, 50, -1);
    gtk_widget_add_css_class(line_numbers_view, "line-numbers");

    // CRITICAL: Ensure text view expands to fill available space
    gtk_widget_set_hexpand(text_view, TRUE);
    gtk_widget_set_vexpand(text_view, TRUE);

    // CRITICAL: Synchronize fonts and text properties for perfect alignment
    synchronize_text_views(line_numbers_view, text_view);

    // Create highlighting tag for current line
    gtk_text_buffer_create_tag(line_numbers_buffer, "current-line",
                              "background", "#094771",
                              "foreground", "#FFFFFF",
                              NULL);

    // CRITICAL: Synchronize scrolling with shared adjustments
    GtkAdjustment *main_vadj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(text_view));
    gtk_scrollable_set_vadjustment(GTK_SCROLLABLE(line_numbers_view), main_vadj);

    // Also synchronize horizontal adjustment to prevent any drift
    GtkAdjustment *main_hadj = gtk_scrollable_get_hadjustment(GTK_SCROLLABLE(text_view));
    gtk_scrollable_set_hadjustment(GTK_SCROLLABLE(line_numbers_view), main_hadj);

    // Create line number data structure
    LineNumberData *ln_data = g_new0(LineNumberData, 1);
    ln_data->line_numbers_view = line_numbers_view;
    ln_data->line_numbers_buffer = line_numbers_buffer;
    ln_data->main_buffer = tab_info->buffer;
    ln_data->last_line_count = 0;

    // Connect signals for updates
    ln_data->buffer_changed_handler = g_signal_connect(tab_info->buffer, "changed",
                                                      G_CALLBACK(update_line_numbers), ln_data);
    ln_data->mark_set_handler = g_signal_connect(tab_info->buffer, "mark-set",
                                                G_CALLBACK(on_mark_set), ln_data);

    // Store line number data in tab info for cleanup
    tab_info->line_number_data = ln_data;

    // Initial update of line numbers
    update_line_numbers(tab_info->buffer, ln_data);

    // Pack widgets - line numbers first, then main text view
    gtk_box_append(GTK_BOX(container), line_numbers_view);
    gtk_box_append(GTK_BOX(container), text_view);

    g_print("Created perfectly synchronized line numbers container\n");

    return container;
}

// Cleanup line number data when tab is closed
void cleanup_line_numbers(TabInfo *tab_info) {
    if (!tab_info || !tab_info->line_number_data) return;

    LineNumberData *ln_data = (LineNumberData*)tab_info->line_number_data;

    // Disconnect signals to prevent memory leaks
    if (ln_data->main_buffer && ln_data->buffer_changed_handler) {
        g_signal_handler_disconnect(ln_data->main_buffer, ln_data->buffer_changed_handler);
    }
    if (ln_data->main_buffer && ln_data->mark_set_handler) {
        g_signal_handler_disconnect(ln_data->main_buffer, ln_data->mark_set_handler);
    }

    // Free data
    g_free(ln_data);
    tab_info->line_number_data = NULL;
}
