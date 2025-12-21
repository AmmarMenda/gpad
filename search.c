#include "gpad.h"
#include "search.h"
#include <string.h>
#include <ctype.h>

static GtkWidget *search_revealer = NULL;
static GtkWidget *search_entry = NULL;
static GtkWidget *search_prev_btn = NULL;
static GtkWidget *search_next_btn = NULL;
static GtkWidget *search_label = NULL;

/* ---------------------------------------------------------
   Boyer-Moore Algorithm Implementation
   --------------------------------------------------------- */

#define ALPHABET_SIZE 256

static void compute_bad_char_table(const char *pattern, int m, int bad_char[ALPHABET_SIZE]) {
    for (int i = 0; i < ALPHABET_SIZE; i++)
        bad_char[i] = -1;

    for (int i = 0; i < m; i++)
        bad_char[(unsigned char)pattern[i]] = i;
}

/*
 * exact_match_boyer_moore:
 * Finds all occurrences of `pattern` in `text`.
 * Returns a GArray of integer offsets (byte indices).
 * The caller must free the GArray.
 */
static GArray* exact_match_boyer_moore(const char *text, const char *pattern) {
    GArray *results = g_array_new(FALSE, FALSE, sizeof(int));
    if (!text || !pattern || !*pattern) return results;

    int n = strlen(text);
    int m = strlen(pattern);
    if (m > n) return results;

    int bad_char[ALPHABET_SIZE];
    compute_bad_char_table(pattern, m, bad_char);

    int s = 0; // s is shift of the pattern with respect to text
    while (s <= (n - m)) {
        int j = m - 1;

        /* Keep reducing j while characters of pattern and text are matching
           at this shift s */
        while (j >= 0 && pattern[j] == text[s + j])
            j--;

        if (j < 0) {
            // Pattern found at index s
            g_array_append_val(results, s);

            /* Shift the pattern so that the next character in text aligns
               with the last occurrence of it in pattern.
               The condition s+m < n is necessary for the case when
               pattern occurs at the end of text */
            s += (s + m < n) ? m - bad_char[(unsigned char)text[s + m]] : 1;
        } else {
            /* Shift the pattern so that the bad character in text aligns
               with the last occurrence of it in pattern. The max function
               is used to make sure that we get a positive shift.
               We may get a negative shift if the last occurrence of bad
               character in pattern is on the right side of the current
               character. */
            int bc_shift = j - bad_char[(unsigned char)text[s + j]];
            s += (1 > bc_shift) ? 1 : bc_shift;
        }
    }

    return results;
}

/* ---------------------------------------------------------
   Search Logic Integation
   --------------------------------------------------------- */

static void clear_search_highlights(GtkTextBuffer *buffer) {
    if (!buffer) return;
    GtkTextTagTable *table = gtk_text_buffer_get_tag_table(buffer);
    GtkTextTag *tag = gtk_text_tag_table_lookup(table, "search-result");
    if (tag) {
        GtkTextIter start, end;
        gtk_text_buffer_get_bounds(buffer, &start, &end);
        gtk_text_buffer_remove_tag(buffer, tag, &start, &end);
    }
}

static void highlight_results(GtkTextBuffer *buffer, GArray *offsets, int pattern_len) {
    if (!buffer || !offsets) return;

    /* Create tag if not exists */
    GtkTextTagTable *table = gtk_text_buffer_get_tag_table(buffer);
    GtkTextTag *tag = gtk_text_tag_table_lookup(table, "search-result");
    if (!tag) {
        tag = gtk_text_buffer_create_tag(buffer, "search-result",
                                       "background", "#FFFF00",
                                       "foreground", "#000000",
                                       NULL);
    }

    /* Iterate results and apply tag */
    /* Note: Offsets are byte offsets. Need to convert to iters safely. */
    /* This can be slow for large files if we iterate from start every time.
       A better way for GtkTextBuffer is iter_forward_chars, but we have byte offsets Boyer-Moore. 
       Let's iterate text and sync iters. */
    
    /* Optimization: Get text, search on it, use byte index to get iter?
       gtk_text_buffer_get_iter_at_offset uses CHAR offset, not BYTE offset.
       We need to be careful with UTF-8. 
       Boyer-Moore works on bytes.
    */
    
    GtkTextIter iter;
    gtk_text_buffer_get_start_iter(buffer, &iter);
    
    for (guint i = 0; i < offsets->len; i++) {
        int byte_offset = g_array_index(offsets, int, i);
        
        GtkTextIter start_iter = iter;
        // set iter to byte_offset. This is O(N) potentially if done repeatedly from start.
        // But since offsets are sorted, we can move forward.
        // Actually, gtk_text_buffer_get_iter_at_byte_offset is what we want on GTK4?
        // Checking docs... usually gtk_text_iter logic handles chars.
        // Let's restart from start for simplicity or use logic.
        
        // Use a fresh iter from start for each (simple but potentially slow for huge files)
        // Or keep a running one.
        // Let's try running one.
        
        // Wait, 'byte_offset' is from start of string.
        gtk_text_buffer_get_iter_at_offset(buffer, &start_iter, 0); // Reset to 0 char
        
        // We have byte offset, need char offset for GTK? or does GTK have byte index?
        // gtk_text_buffer_get_iter_at_line_index works for byte index within line.
        // But we have global byte index.
        
        // Let's use the simplest approach for now:
        // Get full text, find char offset corresponding to byte offset?
        // Or just trust that if we used get_text, the bytes match.
        // Actually, implementing Boyer-Moore on GtkTextBuffer content requires handling UTF8.
        // Standard BM is byte-based. If text is valid UTF8, it works find for substring search.
        
        // To highlight, we need `start` and `end` iters.
        /* 
           Simpler approach for integration: 
           Use gtk_text_iter_forward_search? 
           User specifically asked for Boyer-Moore. So we MUST use our BM implementation.
           So we perform search on the char* string, get results (byte offsets), 
           and map them back to GtkTextIters.
        */
        
        // Mapping byte offset to iter:
        // There isn't a direct "get_iter_at_byte_offset" global function easily.
        // We can walk.
        
        // To do this efficiently:
        // We really just need to find the iters.
        // Let's assume standard UTF-8.
        
        /* 
           Safe way: 
           Use the iter from previous match?
           But we need to know how many bytes to advance.
        */
        
    }
    
    // RE-EVALUATION: Mapping byte offsets to Iters efficiently is tricky without scanning.
    // However, since we have the full text, we can convert byte offset to char offset if we walk the string.
    // Or, we can just use gtk_text_iter_forward_to_line_end etc.
    
    // Let's use a simpler mapping:
    // Convert byte offset to char count using g_utf8_pointer_to_offset.
    
    char *text = NULL;
    GtkTextIter start_bounds, end_bounds;
    gtk_text_buffer_get_bounds(buffer, &start_bounds, &end_bounds);
    text = gtk_text_buffer_get_text(buffer, &start_bounds, &end_bounds, FALSE);
    
    if (!text) return;
    
    for (guint i = 0; i < offsets->len; i++) {
        int byte_offset = g_array_index(offsets, int, i);
        long char_offset = g_utf8_pointer_to_offset(text, text + byte_offset);
        
        GtkTextIter match_start, match_end;
        gtk_text_buffer_get_iter_at_offset(buffer, &match_start, (gint)char_offset);
        match_end = match_start;
        
        /* Advance end by pattern CHAR length */
         long pattern_char_len = g_utf8_strlen(text + byte_offset, pattern_len); 
         // wait, pattern_len is bytes?
         // We should compute pattern char len once.
         
        gtk_text_iter_forward_chars(&match_end, (gint)pattern_char_len); // This might be wrong if pattern_len is bytes passed in.
        
         // Actually, let's just use forward_chars.
         // We need char length of pattern.
         
         gtk_text_buffer_apply_tag(buffer, tag, &match_start, &match_end);
    }
    
    g_free(text);
}

void perform_search(const char *text) {
    if (!text || !*text) return;

    TabInfo *tab = get_current_tab_info();
    if (!tab || !tab->buffer) return;

    clear_search_highlights(tab->buffer);

    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(tab->buffer, &start, &end);
    char *content = gtk_text_buffer_get_text(tab->buffer, &start, &end, FALSE);

    if (!content) return;

    GArray *results = exact_match_boyer_moore(content, text);
    
    if (results->len > 0) {
        // Highlight logic
        int pattern_len = strlen(text); // Bytes
        long pattern_char_len = g_utf8_strlen(text, -1);
        
        // Apply tags
        GtkTextTagTable *table = gtk_text_buffer_get_tag_table(tab->buffer);
        GtkTextTag *tag = gtk_text_tag_table_lookup(table, "search-result");
        if (!tag) {
            tag = gtk_text_buffer_create_tag(tab->buffer, "search-result",
                                           "background", "#FFFF00",
                                           "foreground", "#000000",
                                           NULL);
        }
        
        for (guint i = 0; i < results->len; i++) {
            int byte_offset = g_array_index(results, int, i);
            long char_offset = g_utf8_pointer_to_offset(content, content + byte_offset);
            
            GtkTextIter m_start, m_end;
            gtk_text_buffer_get_iter_at_offset(tab->buffer, &m_start, (gint)char_offset);
            m_end = m_start;
            gtk_text_iter_forward_chars(&m_end, (gint)pattern_char_len);
            
            gtk_text_buffer_apply_tag(tab->buffer, tag, &m_start, &m_end);
            
            // Scroll to first result
            if (i == 0) {
                 gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(tab->text_view), &m_start, 0.0, FALSE, 0, 0);
            }
        }
        
        char *status = g_strdup_printf("%u found", results->len);
        if (search_label) gtk_label_set_text(GTK_LABEL(search_label), status);
        g_free(status);

    } else {
        if (search_label) gtk_label_set_text(GTK_LABEL(search_label), "No results");
    }

    g_array_free(results, TRUE);
    g_free(content);
}

static void on_search_changed(GtkSearchEntry *entry, gpointer user_data) {
    (void)user_data;
    const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (text && strlen(text) > 0) {
        perform_search(text);
    } else {
         TabInfo *tab = get_current_tab_info();
         if (tab) clear_search_highlights(tab->buffer);
         if (search_label) gtk_label_set_text(GTK_LABEL(search_label), "");
    }
}

static void find_match(gboolean forward) {
    const char *text = gtk_editable_get_text(GTK_EDITABLE(search_entry));
    if (!text || !*text) return;

    TabInfo *tab = get_current_tab_info();
    if (!tab || !tab->buffer) return;

    GtkTextTagTable *table = gtk_text_buffer_get_tag_table(tab->buffer);
    GtkTextTag *tag = gtk_text_tag_table_lookup(table, "search-result");
    if (!tag) return;

    GtkTextIter iter;
    GtkTextMark *insert = gtk_text_buffer_get_insert(tab->buffer);
    gtk_text_buffer_get_iter_at_mark(tab->buffer, &iter, insert);
    
    /* Move slightly to avoid sticking to current match start if just landed there */
    /* If forward, check if we are at start of a match. If so, inside loop we might need care.
       gtk_text_iter_forward_to_tag_toggle moves to next toggle. 
       If we are at start, it moves to end.
       If we are strictly before, it moves to start.
    */
    
    GtkTextIter search_iter = iter;
    gboolean wrapped = FALSE;
    
    /* Only wrap once */
    while (TRUE) {
        gboolean found_toggle = FALSE;
        if (forward) {
            found_toggle = gtk_text_iter_forward_to_tag_toggle(&search_iter, tag);
        } else {
            found_toggle = gtk_text_iter_backward_to_tag_toggle(&search_iter, tag);
        }

        if (!found_toggle) {
            if (wrapped) break;
            wrapped = TRUE;
            if (forward) gtk_text_buffer_get_start_iter(tab->buffer, &search_iter);
            else gtk_text_buffer_get_end_iter(tab->buffer, &search_iter);
            continue;
        }

        /* Check if this toggle is the START of the tag (for forward) or END (which means prev starts before) */
        
        if (forward) {
            if (gtk_text_iter_starts_tag(&search_iter, tag)) {
                /* Found start of a match */
                gtk_text_buffer_place_cursor(tab->buffer, &search_iter);
                gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(tab->text_view), insert, 0.0, FALSE, 0, 0);
                return;
            }
        } else {
             /* Backward: backward_to_tag_toggle moves to the toggle BEFORE.
                If we land on a toggle, it might be start or end.
                If currently inside text, backward toggle is START of current.
             */
             if (gtk_text_iter_starts_tag(&search_iter, tag)) {
                 /* We moved back and hit a start. This is a match start. */
                 gtk_text_buffer_place_cursor(tab->buffer, &search_iter);
                 gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(tab->text_view), insert, 0.0, FALSE, 0, 0);
                 return;
             }
        }
    }
}

static void on_next_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn; (void)user_data;
    find_match(TRUE);
}

static void on_prev_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn; (void)user_data;
    find_match(FALSE);
}

static void on_search_stop(GtkSearchEntry *entry, gpointer user_data) {
    (void)entry; (void)user_data;
    toggle_search_bar(); // Hide on Escape logic check? Or just hide.
    // Actually search entry 'stop-search' signal is usually clearing or escape.
    // Let's just hide.
    /* But 'stop-search' might clear text too? */
}

static gboolean on_key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
    (void)controller; (void)keycode; (void)state; (void)user_data;
    if (keyval == GDK_KEY_Escape) {
        toggle_search_bar();
        return TRUE;
    }
    return FALSE;
}

void toggle_search_bar(void) {
    if (!search_revealer) return;
    
    gboolean revealed = gtk_revealer_get_reveal_child(GTK_REVEALER(search_revealer));
    
    if (revealed) {
        gtk_revealer_set_reveal_child(GTK_REVEALER(search_revealer), FALSE);
        TabInfo *tab = get_current_tab_info();
        if (tab) {
             clear_search_highlights(tab->buffer);
             gtk_widget_grab_focus(tab->text_view);
        }
    } else {
        gtk_revealer_set_reveal_child(GTK_REVEALER(search_revealer), TRUE);
        gtk_widget_grab_focus(search_entry);
        
        // Pre-fill with selection if exists?
        TabInfo *tab = get_current_tab_info();
        if (tab && tab->buffer) {
             GtkTextIter start, end;
             if (gtk_text_buffer_get_selection_bounds(tab->buffer, &start, &end)) {
                 char *sel = gtk_text_buffer_get_text(tab->buffer, &start, &end, FALSE);
                 gtk_editable_set_text(GTK_EDITABLE(search_entry), sel);
                 g_free(sel);
             }
        }
    }
}

GtkWidget* init_search_ui(void) {
    search_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(search_revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
    
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(box, "search-bar");
    gtk_widget_set_margin_start(box, 10);
    gtk_widget_set_margin_end(box, 10);
    gtk_widget_set_margin_top(box, 5);
    gtk_widget_set_margin_bottom(box, 5);
    
    search_entry = gtk_search_entry_new();
    gtk_widget_set_hexpand(search_entry, TRUE);
    g_signal_connect(search_entry, "search-changed", G_CALLBACK(on_search_changed), NULL);
    g_signal_connect(search_entry, "stop-search", G_CALLBACK(on_search_stop), NULL);

    GtkEventController *controller = gtk_event_controller_key_new();
    g_signal_connect(controller, "key-pressed", G_CALLBACK(on_key_pressed), NULL);
    gtk_widget_add_controller(search_entry, controller);

    
    search_prev_btn = gtk_button_new_from_icon_name("go-up-symbolic");
    gtk_widget_set_tooltip_text(search_prev_btn, "Previous Match");
    g_signal_connect(search_prev_btn, "clicked", G_CALLBACK(on_prev_clicked), NULL);

    search_next_btn = gtk_button_new_from_icon_name("go-down-symbolic");
    gtk_widget_set_tooltip_text(search_next_btn, "Next Match");
    g_signal_connect(search_next_btn, "clicked", G_CALLBACK(on_next_clicked), NULL);
    
    search_label = gtk_label_new("");
    
    GtkWidget *close_btn = gtk_button_new_from_icon_name("window-close-symbolic");
    g_signal_connect_swapped(close_btn, "clicked", G_CALLBACK(toggle_search_bar), NULL);
    gtk_button_set_has_frame(GTK_BUTTON(close_btn), FALSE);

    gtk_box_append(GTK_BOX(box), search_entry);
    gtk_box_append(GTK_BOX(box), search_label);
    gtk_box_append(GTK_BOX(box), search_prev_btn);
    gtk_box_append(GTK_BOX(box), search_next_btn);
    gtk_box_append(GTK_BOX(box), close_btn);
    
    gtk_revealer_set_child(GTK_REVEALER(search_revealer), box);
    
    return search_revealer;
}
