#include "gpad.h"
#include <gtksourceview/gtksource.h>



/**
 * Trampoline function to call the highlight sync from a timeout source.
 */
static gboolean highlight_timeout_trampoline(gpointer user_data) {
    TabInfo *tab = (TabInfo*)user_data;
    if (!tab) return G_SOURCE_REMOVE;

    highlight_buffer_sync(tab->buffer, &tab->ts_tree, tab->lang_type);
    tab->highlight_source_id = 0;
    return G_SOURCE_REMOVE;
}

/**
 * Detects changes in the text buffer to mark the tab as dirty.
 */
static void on_buffer_changed(GtkTextBuffer *buffer, gpointer user_data) {
    (void)buffer;
    TabInfo *tab = (TabInfo*)user_data;
    if (!tab) return;
    if (!tab->dirty) { tab->dirty = TRUE; update_tab_label(tab); }
    if (tab->highlight_source_id) { g_source_remove(tab->highlight_source_id); tab->highlight_source_id = 0; }


}


/**
 * Signal handler for cursor movement to update status bar and handle auto-scroll.
 */
static void on_cursor_mark_set(GtkTextBuffer *buffer, GtkTextIter *iter, GtkTextMark *mark, gpointer user_data) {
    (void)iter;
    TabInfo *tab = (TabInfo*)user_data;
    if (!tab || !tab->text_view || !tab->auto_scroll_enabled) return;
    if (mark != gtk_text_buffer_get_insert(buffer)) return;

    GtkTextIter cur;
    gtk_text_buffer_get_iter_at_mark(buffer, &cur, mark);

    int ly = 0, lh = 0;
    gtk_text_view_get_line_yrange(GTK_TEXT_VIEW(tab->text_view), &cur, &ly, &lh);

    GdkRectangle vis = {0};
    gtk_text_view_get_visible_rect(GTK_TEXT_VIEW(tab->text_view), &vis);

    const int scrolloff = 3;
    int margin = scrolloff * (lh > 0 ? lh : 1);
    int top_th = vis.y + margin;
    int bot_th = vis.y + vis.height - margin;

    if (ly < top_th || (ly + lh) > bot_th) {
        gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(tab->text_view),
                                     mark,
                                     tab->auto_scroll_within, TRUE, 0.0, tab->auto_scroll_yalign);
    }


    if (footer_label) {
        guint line = gtk_text_iter_get_line(&cur) + 1;
        guint col = gtk_text_iter_get_line_offset(&cur) + 1;
        char *status = g_strdup_printf("Ln %u, Col %u", line, col);
        gtk_label_set_text(GTK_LABEL(footer_label), status);
        g_free(status);
    }
}



/**
 * Updates the visual label of a tab, adding an asterisk if the content is dirty.
 */
void update_tab_label(TabInfo *tab_info) {
    if (!global_notebook || !tab_info) return;
    GtkWidget *page = tab_info->scrolled_window;
    GtkWidget *label_box = gtk_notebook_get_tab_label(global_notebook, page);
    if (!label_box) return;
    GtkWidget *label = gtk_widget_get_first_child(label_box);
    if (!label) return;

    char *basename = tab_info->filename ? g_path_get_basename(tab_info->filename) : NULL;
    const char *display = basename ? basename : "Untitled";

    char *markup = tab_info->dirty
        ? g_strdup_printf("<i>%s*</i>", display)
        : g_strdup(display);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    g_free(markup);
    g_free(basename);
}

/**
 * Defines syntax highlighting tags for the GtkTextBuffer.
 */
void setup_highlighting_tags(GtkTextBuffer *buffer) {

    gtk_text_buffer_create_tag(buffer, "comment",   "foreground", "#8E908C", "style", PANGO_STYLE_ITALIC, NULL);
    gtk_text_buffer_create_tag(buffer, "string",    "foreground", "#2AA198", NULL);
    gtk_text_buffer_create_tag(buffer, "preproc",   "foreground", "#CB4B16", NULL);
    gtk_text_buffer_create_tag(buffer, "keyword",   "foreground", "#859900", "weight", PANGO_WEIGHT_BOLD, NULL);
    gtk_text_buffer_create_tag(buffer, "control",   "foreground", "#B58900", "weight", PANGO_WEIGHT_BOLD, NULL);
    gtk_text_buffer_create_tag(buffer, "type",      "foreground", "#268BD2", NULL);
    gtk_text_buffer_create_tag(buffer, "number",    "foreground", "#D33682", NULL);
    gtk_text_buffer_create_tag(buffer, "function",  "foreground", "#268BD2", "weight", PANGO_WEIGHT_BOLD, NULL);
    gtk_text_buffer_create_tag(buffer, "constant",  "foreground", "#6C71C4", NULL);
    gtk_text_buffer_create_tag(buffer, "decorator", "foreground", "#B58900", "style", PANGO_STYLE_ITALIC, NULL);
}



/**
 * Signal handler for when the active notebook tab changes.
 */
void on_tab_switched(GtkNotebook *notebook, GtkWidget *page, guint page_num, gpointer user_data) {
    (void)notebook; (void)page; (void)page_num; (void)user_data;

    TabInfo *tab = get_current_tab_info();
    if (tab && tab->filename) {
        char *current_file_dir = g_path_get_dirname(tab->filename);
        if (current_directory && strcmp(current_directory, current_file_dir) != 0) {
            g_free(current_directory);
            current_directory = current_file_dir;
            refresh_file_tree(current_directory);
        } else if (!current_directory) {
            current_directory = current_file_dir;
            refresh_file_tree(current_directory);
        } else {
            g_free(current_file_dir);
        }
        highlight_current_file(tab->filename);
    } else if (current_directory) {
        refresh_file_tree(current_directory);
    }

    if (tab && tab->buffer && footer_label) {
        GtkTextIter cursor_iter;
        GtkTextMark *insert = gtk_text_buffer_get_insert(tab->buffer);
        gtk_text_buffer_get_iter_at_mark(tab->buffer, &cursor_iter, insert);
        guint line = gtk_text_iter_get_line(&cursor_iter) + 1;
        guint col = gtk_text_iter_get_line_offset(&cursor_iter) + 1;
        char *status = g_strdup_printf("Ln %u, Col %u", line, col);
        gtk_label_set_text(GTK_LABEL(footer_label), status);
        g_free(status);
    } else if (footer_label) {
        gtk_label_set_text(GTK_LABEL(footer_label), "");
    }

    if (tab && tab->auto_scroll_enabled) {
        GtkTextMark *insert = gtk_text_buffer_get_insert(tab->buffer);
        gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(tab->text_view),
                                     insert,
                                     tab->auto_scroll_within, TRUE, 0.0, tab->auto_scroll_yalign);
    }
}

/**
 * Callback for the close button on a tab.
 */
static void on_tab_close_button_clicked(GtkButton *button, gpointer user_data) {
    (void)button; (void)user_data;
    close_current_tab();
}



/**
 * Internal helper to create and initialize a new tab with GtkSourceView.
 */
static void create_tab_internal(const char *filename, gboolean hide_sidebar) {
    g_print("create_tab_internal: filename='%s', hide_sidebar=%s\n",
            filename ? filename : "NULL", hide_sidebar ? "TRUE" : "FALSE");
    show_notebook();

    if (!global_notebook) {
        g_warning("Cannot create tab: notebook not initialized yet");
        return;
    }


    if (filename && *filename) {
        for (int i = 0; i < gtk_notebook_get_n_pages(global_notebook); ++i) {
            GtkWidget *page = gtk_notebook_get_nth_page(global_notebook, i);
            if (!page) continue;
            TabInfo *info = (TabInfo*)g_object_get_data(G_OBJECT(page), "tab_info");
            if (info && info->filename && strcmp(info->filename, filename) == 0) {
                gtk_notebook_set_current_page(global_notebook, i);
                g_print("File already open, switching to existing tab\n");
                return;
            }
        }
    }

    if (hide_sidebar) { hide_panels(); set_sidebar_visible(FALSE); }


    GtkWidget *scroller = gtk_scrolled_window_new();
    GtkSourceView *sview = GTK_SOURCE_VIEW(gtk_source_view_new());
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), GTK_WIDGET(sview));
    gtk_widget_set_hexpand(scroller, TRUE);
    gtk_widget_set_vexpand(scroller, TRUE);

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(sview));
    GtkSourceBuffer *sbuf = GTK_SOURCE_BUFFER(buffer);

    if (!scroller || !sview || !buffer) {
        g_error("Failed to create tab UI elements");
        return;
    }


    gtk_source_view_set_show_line_numbers(sview, TRUE);
    gtk_source_view_set_highlight_current_line(sview, TRUE);
    gtk_source_view_set_tab_width(sview, 4);
    gtk_source_view_set_insert_spaces_instead_of_tabs(sview, TRUE);
    gtk_source_view_set_show_right_margin(sview, TRUE);
    gtk_source_view_set_right_margin_position(sview, 80);
    gtk_source_view_set_auto_indent(sview, TRUE);

    gtk_text_view_set_monospace(GTK_TEXT_VIEW(sview), TRUE);
    gtk_text_buffer_set_enable_undo(buffer, TRUE);
    gtk_text_buffer_set_max_undo_levels(buffer, 100);


    GtkSourceLanguageManager *lm = gtk_source_language_manager_get_default();
    GtkSourceLanguage *lang = gtk_source_language_manager_guess_language(lm, filename, NULL);
    if (lang) {
        gtk_source_buffer_set_language(sbuf, lang);
        gtk_source_buffer_set_highlight_syntax(sbuf, TRUE);
    }


    GtkSourceStyleSchemeManager *sm = gtk_source_style_scheme_manager_get_default();
    GtkSourceStyleScheme *scheme = gtk_source_style_scheme_manager_get_scheme(sm, "classic");
    if (scheme) {
        gtk_source_buffer_set_style_scheme(sbuf, scheme);
    }


    TabInfo *tab = g_new0(TabInfo, 1);
    tab->scrolled_window  = scroller;
    tab->text_view        = GTK_WIDGET(sview);
    tab->buffer           = buffer;
    tab->filename         = (filename && *filename) ? g_strdup(filename) : NULL;
    tab->dirty            = FALSE;
    tab->lang_type        = get_language_from_filename(filename);
    tab->ts_tree          = NULL;

    tab->auto_scroll_enabled = TRUE;
    tab->auto_scroll_yalign  = 0.30;
    tab->auto_scroll_within  = 0.10;

    tab->highlight_source_id    = 0;
    tab->buffer_changed_handler = 0;
    tab->cursor_mark_handler    = 0;
    tab->modified_close_handler = 0;


    setup_highlighting_tags(buffer);


    if (filename && *filename) {
        gchar *contents = NULL;
        GError *err = NULL;
        g_print("Loading file content: %s\n", filename);
        if (g_file_get_contents(filename, &contents, NULL, &err)) {
            g_signal_handlers_block_by_func(buffer, G_CALLBACK(on_buffer_changed), tab);
            gtk_text_buffer_set_text(buffer, contents, -1);
            g_signal_handlers_unblock_by_func(buffer, G_CALLBACK(on_buffer_changed), tab);




            add_to_recent_files(filename);
            g_free(contents);
            g_print("Successfully loaded file: %s\n", filename);
        } else {
            g_warning("Failed to load file %s: %s", filename, err ? err->message : "Unknown error");
            if (err) g_error_free(err);
        }
    }


    GtkWidget *tab_label_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    char *base = filename && *filename ? g_path_get_basename(filename) : NULL;
    const char *title = base ? base : "Untitled";
    GtkWidget *tab_label = gtk_label_new(title);
    GtkWidget *close_btn = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_button_set_has_frame(GTK_BUTTON(close_btn), FALSE);
    gtk_widget_set_size_request(tab_label_box, 60, 24);
    gtk_widget_set_size_request(tab_label, 30, 16);
    gtk_widget_set_size_request(close_btn, 16, 16);
    gtk_box_append(GTK_BOX(tab_label_box), tab_label);
    gtk_box_append(GTK_BOX(tab_label_box), close_btn);
    g_free(base);


    g_object_set_data_full(G_OBJECT(scroller), "tab_info", tab, g_free);


    tab->buffer_changed_handler = g_signal_connect(buffer, "changed",  G_CALLBACK(on_buffer_changed),  tab);
    tab->cursor_mark_handler    = g_signal_connect(buffer, "mark-set", G_CALLBACK(on_cursor_mark_set), tab);
    g_signal_connect(close_btn, "clicked", G_CALLBACK(on_tab_close_button_clicked), NULL);


    gtk_notebook_append_page(global_notebook, scroller, tab_label_box);
    gtk_notebook_set_current_page(global_notebook, gtk_notebook_get_n_pages(global_notebook) - 1);
    gtk_widget_grab_focus(GTK_WIDGET(sview));
    g_print("Tab creation completed successfully\n");
}

/**
 * Public API to create a new tab and hide the sidebar.
 */
void create_new_tab(const char *filename) {
    if (filename != NULL)
        create_tab_internal(filename, TRUE);
   else
       g_print("Ready to create new file: %s\n", filename);
}
/**
 * Public API to create a new tab without hiding the sidebar.
 */
void create_new_tab_from_sidebar(const char *filename) { create_tab_internal(filename, FALSE); }

/**
 * Watches for buffer modifications to finish closing a tab after a save.
 */
static void on_buffer_modified_for_close(GtkTextBuffer *buffer, gpointer user_data) {
    TabInfo *tab = (TabInfo*)user_data;
    if (!tab) return;
    if (!gtk_text_buffer_get_modified(buffer)) {
        if (tab->modified_close_handler) {
            g_signal_handler_disconnect(buffer, tab->modified_close_handler);
            tab->modified_close_handler = 0;
        }
        tab->dirty = FALSE;
        close_current_tab();
    }
}

/**
 * Handles the user's response to the unsaved changes dialog.
 */
static void on_confirm_close_response(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    gint choice = gtk_alert_dialog_choose_finish(GTK_ALERT_DIALOG(source_object), res, &error);
    TabInfo *tab = (TabInfo*)user_data;
    if (!tab) return;

    if (error) {
        g_warning("Alert dialog error: %s", error->message);
        g_error_free(error);
        return;
    }

    if (choice == 1) {
        if (tab->filename && *tab->filename) {
            save_current_tab();
            tab->dirty = FALSE;
            close_current_tab();
        } else {
            save_current_tab();
            if (!tab->modified_close_handler) {
                tab->modified_close_handler =
                    g_signal_connect(tab->buffer, "modified-changed",
                                     G_CALLBACK(on_buffer_modified_for_close), tab);
            }
        }
    } else if (choice == 2) {
        tab->dirty = FALSE;
        gtk_text_buffer_set_modified(tab->buffer, FALSE);
        close_current_tab();
    }

}



/**
 * Closes the currently active tab, prompting to save if necessary.
 */
gboolean close_current_tab(void) {
    TabInfo *tab = get_current_tab_info();
    if (!tab || !global_notebook) return FALSE;

    if (tab->dirty) {
        char *base = tab->filename ? g_path_get_basename(tab->filename) : NULL;
        const char *shown = base ? base : "Untitled";
        GtkAlertDialog *dlg = gtk_alert_dialog_new("Save changes to \"%s\" before closing?", shown);
        gtk_alert_dialog_set_detail(dlg, "Your changes will be lost if you don't save them.");
        const char* buttons[] = {"Cancel", "_Save", "Close without Saving", NULL};
        gtk_alert_dialog_set_buttons(dlg, buttons);
        gtk_alert_dialog_set_default_button(dlg, 1);
        gtk_alert_dialog_set_cancel_button(dlg, 0);
        gtk_alert_dialog_choose(dlg, GTK_WINDOW(global_window), NULL, on_confirm_close_response, tab);
        g_free(base);
        return TRUE;
    }


    if (tab->highlight_source_id) { g_source_remove(tab->highlight_source_id); tab->highlight_source_id = 0; }


    if (tab->buffer && tab->buffer_changed_handler) {
        g_signal_handler_disconnect(tab->buffer, tab->buffer_changed_handler); tab->buffer_changed_handler = 0;
    }
    if (tab->buffer && tab->cursor_mark_handler) {
        g_signal_handler_disconnect(tab->buffer, tab->cursor_mark_handler);    tab->cursor_mark_handler = 0;
    }
    if (tab->buffer && tab->modified_close_handler) {
        g_signal_handler_disconnect(tab->buffer, tab->modified_close_handler); tab->modified_close_handler = 0;
    }

#ifdef HAVE_TREE_SITTER
    if (tab->ts_tree) {
        TSTree *t = (TSTree*)tab->ts_tree;
        ts_tree_delete(t);
        tab->ts_tree = NULL;
    }
#endif


    int page = gtk_notebook_get_current_page(global_notebook);
    gtk_notebook_remove_page(global_notebook, page);
    return FALSE;
}



/**
 * Gets the metadata associated with the current active tab.
 */
TabInfo* get_current_tab_info(void) {
    if (!global_notebook) return NULL;
    int idx = gtk_notebook_get_current_page(global_notebook);
    if (idx < 0) return NULL;
    GtkWidget *page = gtk_notebook_get_nth_page(global_notebook, idx);
    if (!page) return NULL;
    return (TabInfo*)g_object_get_data(G_OBJECT(page), "tab_info");
}

/**
 * Enables or disables automatic scrolling to the cursor for the current tab.
 */
void set_auto_scroll_enabled_current(gboolean enabled) {
    TabInfo *tab = get_current_tab_info();
    if (!tab) return;
    tab->auto_scroll_enabled = enabled;
    if (enabled) {
        GtkTextMark *insert = gtk_text_buffer_get_insert(tab->buffer);
        gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(tab->text_view),
                                     insert,
                                     tab->auto_scroll_within, TRUE, 0.0, tab->auto_scroll_yalign);
    }
}
