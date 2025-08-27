#include "gpad.h"

// Structure to hold line number data for each tab
typedef struct {
    GtkWidget *line_numbers_view;
    GtkTextBuffer *line_numbers_buffer;
    GtkTextBuffer *main_buffer;
    gulong buffer_changed_handler;
    gulong mark_set_handler;
} LineNumberData;

// Calculate width needed for line numbers based on line count
static int calculate_line_number_width(int line_count) {
    if (line_count < 10) return 2;
    if (line_count < 100) return 3;
    if (line_count < 1000) return 4;
    if (line_count < 10000) return 5;
    return 6; // Support up to 999,999 lines
}

// Update line numbers content with proper formatting for large numbers
static void update_line_numbers(GtkTextBuffer *buffer, gpointer user_data) {
    LineNumberData *ln_data = (LineNumberData*)user_data;

    if (!ln_data || !ln_data->line_numbers_buffer) return;

    int line_count = gtk_text_buffer_get_line_count(buffer);
    if (line_count <= 0) return;

    // Calculate required width for line numbers
    int width = calculate_line_number_width(line_count);

    GString *numbers = g_string_new("");

    // Generate line numbers with consistent formatting
    for (int i = 1; i <= line_count; i++) {
        g_string_append_printf(numbers, "%*d\n", width, i);
    }

    // Remove the last newline to match main buffer
    if (numbers->len > 0 && numbers->str[numbers->len - 1] == '\n') {
        g_string_truncate(numbers, numbers->len - 1);
    }

    // Block signals to prevent recursion
    g_signal_handlers_block_by_func(ln_data->line_numbers_buffer, update_line_numbers, ln_data);
    gtk_text_buffer_set_text(ln_data->line_numbers_buffer, numbers->str, -1);
    g_signal_handlers_unblock_by_func(ln_data->line_numbers_buffer, update_line_numbers, ln_data);

    // Adjust line number view width based on content
    int char_width = 8; // Approximate character width in pixels for monospace font
    int needed_width = (width + 1) * char_width + 10; // +1 for space, +10 for padding
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

    // Remove existing highlighting
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

// Create line numbers widget for a text view
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
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(line_numbers_view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(line_numbers_view), GTK_WRAP_NONE);

    // Set initial size (will be adjusted dynamically)
    gtk_widget_set_size_request(line_numbers_view, 60, -1);
    gtk_widget_add_css_class(line_numbers_view, "line-numbers");

    // IMPORTANT: Ensure text view expands to fill available space
    gtk_widget_set_hexpand(text_view, TRUE);
    gtk_widget_set_vexpand(text_view, TRUE);

    // Create highlighting tag for current line
    gtk_text_buffer_create_tag(line_numbers_buffer, "current-line",
                              "background", "#094771",
                              "foreground", "#FFFFFF",
                              NULL);

    // Synchronize scrolling
    GtkAdjustment *main_vadj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(text_view));
    gtk_scrollable_set_vadjustment(GTK_SCROLLABLE(line_numbers_view), main_vadj);

    // Create line number data structure
    LineNumberData *ln_data = g_new0(LineNumberData, 1);
    ln_data->line_numbers_view = line_numbers_view;
    ln_data->line_numbers_buffer = line_numbers_buffer;
    ln_data->main_buffer = tab_info->buffer;

    // Connect signals
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

    g_print("Created line numbers container with text view\n");

    return container;
}

// Cleanup line number data when tab is closed
void cleanup_line_numbers(TabInfo *tab_info) {
    if (!tab_info || !tab_info->line_number_data) return;

    LineNumberData *ln_data = (LineNumberData*)tab_info->line_number_data;

    // Disconnect signals
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
