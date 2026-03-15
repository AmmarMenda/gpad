#include "gpad.h"
#include "search.h"
#include <string.h>
#include <ctype.h>

static GtkWidget *search_revealer = NULL;
static GtkWidget *search_entry = NULL;
static GtkWidget *search_prev_btn = NULL;
static GtkWidget *search_next_btn = NULL;
static GtkWidget *search_label = NULL;

 

#define ALPHABET_SIZE 256

/**
 * Computes the bad character table for the Boyer-Moore string search algorithm.
 */
static void compute_bad_char_table(const char *pattern, int m, int bad_char[ALPHABET_SIZE]) {
    for (int i = 0; i < ALPHABET_SIZE; i++)
        bad_char[i] = -1;

    for (int i = 0; i < m; i++)
        bad_char[(unsigned char)pattern[i]] = i;
}

 
/**
 * Implements the Boyer-Moore algorithm for exact string matching.
 * Returns an array of byte offsets where the pattern was found.
 */
static GArray* exact_match_boyer_moore(const char *text, const char *pattern) {
    GArray *results = g_array_new(FALSE, FALSE, sizeof(int));
    if (!text || !pattern || !*pattern) return results;

    int n = strlen(text);
    int m = strlen(pattern);
    if (m > n) return results;

    int bad_char[ALPHABET_SIZE];
    compute_bad_char_table(pattern, m, bad_char);

    int s = 0;  
    while (s <= (n - m)) {
        int j = m - 1;

         
        while (j >= 0 && pattern[j] == text[s + j])
            j--;

        if (j < 0) {
             
            g_array_append_val(results, s);

             
            s += (s + m < n) ? m - bad_char[(unsigned char)text[s + m]] : 1;
        } else {
             
            int bc_shift = j - bad_char[(unsigned char)text[s + j]];
            s += (1 > bc_shift) ? 1 : bc_shift;
        }
    }

    return results;
}

 

/**
 * Removes all search result highlights from the text buffer.
 */
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

/**
 * Highlights find results in the buffer based on provided offsets.
 */
static void highlight_results(GtkTextBuffer *buffer, GArray *offsets, int pattern_len) {
    if (!buffer || !offsets) return;

     
    GtkTextTagTable *table = gtk_text_buffer_get_tag_table(buffer);
    GtkTextTag *tag = gtk_text_tag_table_lookup(table, "search-result");
    if (!tag) {
        tag = gtk_text_buffer_create_tag(buffer, "search-result",
                                       "background", "#FFFF00",
                                       "foreground", "#000000",
                                       NULL);
    }

     
     
     
    
     
    
    GtkTextIter iter;
    gtk_text_buffer_get_start_iter(buffer, &iter);
    
    for (guint i = 0; i < offsets->len; i++) {
        int byte_offset = g_array_index(offsets, int, i);
        
        GtkTextIter start_iter = iter;
         
         
         
         
         
        
         
         
         
        
         
        gtk_text_buffer_get_iter_at_offset(buffer, &start_iter, 0);  
        
         
         
         
        
         
         
         
         
         
        
         
         
        
         
         
         
        
         
         
         
        
         
        
    }
    
     
     
     
    
     
     
    
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
        
         
         long pattern_char_len = g_utf8_strlen(text + byte_offset, pattern_len); 
          
          
         
        gtk_text_iter_forward_chars(&match_end, (gint)pattern_char_len);  
        
          
          
         
         gtk_text_buffer_apply_tag(buffer, tag, &match_start, &match_end);
    }
    
    g_free(text);
}

/**
 * Main search function that finds and highlights matches in the current tab.
 */
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
         
        int pattern_len = strlen(text);  
        long pattern_char_len = g_utf8_strlen(text, -1);
        
         
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

/**
 * Signal handler for changes in the search entry text.
 */
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

/**
 * Navigates to the next or previous search match and focuses it.
 */
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
    
     
     
    
    GtkTextIter search_iter = iter;
    gboolean wrapped = FALSE;
    
     
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

         
        
        if (forward) {
            if (gtk_text_iter_starts_tag(&search_iter, tag)) {
                 
                gtk_text_buffer_place_cursor(tab->buffer, &search_iter);
                gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(tab->text_view), insert, 0.0, FALSE, 0, 0);
                return;
            }
        } else {
              
             if (gtk_text_iter_starts_tag(&search_iter, tag)) {
                  
                 gtk_text_buffer_place_cursor(tab->buffer, &search_iter);
                 gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(tab->text_view), insert, 0.0, FALSE, 0, 0);
                 return;
             }
        }
    }
}

/**
 * Callback for the 'Next' match button.
 */
static void on_next_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn; (void)user_data;
    find_match(TRUE);
}

/**
 * Callback for the 'Previous' match button.
 */
static void on_prev_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn; (void)user_data;
    find_match(FALSE);
}

/**
 * Signal handler for stopping a search (e.g., pressing Enter or closing).
 */
static void on_search_stop(GtkSearchEntry *entry, gpointer user_data) {
    (void)entry; (void)user_data;
    toggle_search_bar();  
     
     
     
}

/**
 * Key press handler for the search entry (e.g., handling Escape).
 */
static gboolean on_key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
    (void)controller; (void)keycode; (void)state; (void)user_data;
    if (keyval == GDK_KEY_Escape) {
        toggle_search_bar();
        return TRUE;
    }
    return FALSE;
}

/**
 * Toggles the visibility of the search bar revealer.
 */
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

/**
 * Initializes the search bar UI components.
 */
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
