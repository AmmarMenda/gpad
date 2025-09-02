#include "gpad.h"
#include <gtksourceview/gtksource.h>

/* ---------------------------
   Caret-follow + debounce
   --------------------------- */

static gboolean highlight_timeout_trampoline(gpointer user_data) {
    TabInfo *tab = (TabInfo*)user_data;
    if (!tab) return G_SOURCE_REMOVE;
    /* If Tree-sitter highlighting is still desired, keep this call; otherwise, remove the debounce entirely. */
    highlight_buffer_sync(tab->buffer, &tab->ts_tree, tab->lang_type); /* safe with GtkSource if using non-overlapping tags [6] */
    tab->highlight_source_id = 0;
    return G_SOURCE_REMOVE;
}

static void on_buffer_changed(GtkTextBuffer *buffer, gpointer user_data) {
    (void)buffer;
    TabInfo *tab = (TabInfo*)user_data;
    if (!tab) return;
    if (!tab->dirty) { tab->dirty = TRUE; update_tab_label(tab); } /* GtkTextBuffer dirty UI state [6] */
    if (tab->highlight_source_id) { g_source_remove(tab->highlight_source_id); tab->highlight_source_id = 0; } /* GLib main loop [7] */
    /* Optionally schedule Tree‑sitter highlight if used alongside GtkSource */
    /* tab->highlight_source_id = g_timeout_add(150, highlight_timeout_trampoline, tab); */
}

/* Keep caret in view with a relative alignment when it reaches the top/bottom band */
static void on_cursor_mark_set(GtkTextBuffer *buffer, GtkTextIter *iter, GtkTextMark *mark, gpointer user_data) {
    (void)iter;
    TabInfo *tab = (TabInfo*)user_data;
    if (!tab || !tab->text_view || !tab->auto_scroll_enabled) return; /* guards [5][8] */
    if (mark != gtk_text_buffer_get_insert(buffer)) return;            /* only caret moves [6] */

    GtkTextIter cur;
    gtk_text_buffer_get_iter_at_mark(buffer, &cur, mark);              /* caret iter [6] */

    int ly = 0, lh = 0;
    gtk_text_view_get_line_yrange(GTK_TEXT_VIEW(tab->text_view), &cur, &ly, &lh); /* per-line metrics [9] */

    GdkRectangle vis = {0};
    gtk_text_view_get_visible_rect(GTK_TEXT_VIEW(tab->text_view), &vis);          /* visible rect [10] */

    const int scrolloff = 3;
    int margin = scrolloff * (lh > 0 ? lh : 1);
    int top_th = vis.y + margin;
    int bot_th = vis.y + vis.height - margin;

    if (ly < top_th || (ly + lh) > bot_th) {
        gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(tab->text_view),
                                     mark,
                                     tab->auto_scroll_within, TRUE, 0.0, tab->auto_scroll_yalign); /* caret-follow [4] */
    }
}

/* ---------------------------
   Tab label and tags (tags optional with GtkSource)
   --------------------------- */

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
    gtk_label_set_markup(GTK_LABEL(label), markup); /* label markup [11] */
    g_free(markup);
    g_free(basename);
}

void setup_highlighting_tags(GtkTextBuffer *buffer) {
    /* Keep only if using custom tags in addition to GtkSource; otherwise, can be a no-op. */
    gtk_text_buffer_create_tag(buffer, "comment",   "foreground", "#6A9955", "style", PANGO_STYLE_ITALIC, NULL);
    gtk_text_buffer_create_tag(buffer, "string",    "foreground", "#CE9178", NULL);
    gtk_text_buffer_create_tag(buffer, "preproc",   "foreground", "#9B9B9B", NULL);
    gtk_text_buffer_create_tag(buffer, "keyword",   "foreground", "#569CD6", "weight", PANGO_WEIGHT_BOLD, NULL);
    gtk_text_buffer_create_tag(buffer, "control",   "foreground", "#C586C0", "weight", PANGO_WEIGHT_BOLD, NULL);
    gtk_text_buffer_create_tag(buffer, "type",      "foreground", "#4EC9B0", NULL);
    gtk_text_buffer_create_tag(buffer, "number",    "foreground", "#B5CEA8", NULL);
    gtk_text_buffer_create_tag(buffer, "function",  "foreground", "#DCDCAA", NULL);
    gtk_text_buffer_create_tag(buffer, "constant",  "foreground", "#4FC1FF", NULL);
    gtk_text_buffer_create_tag(buffer, "decorator", "foreground", "#B5CEA8", "style", PANGO_STYLE_ITALIC, NULL);
}

/* ---------------------------
   Tab switching
   --------------------------- */

void on_tab_switched(GtkNotebook *notebook, GtkWidget *page, guint page_num, gpointer user_data) {
    (void)notebook; (void)page; (void)page_num; (void)user_data;

    if (is_sidebar_visible() && side_panel && gtk_widget_is_visible(side_panel)) {
        refresh_file_tree_current();
        g_print("Tab switched - scheduling file browser refresh\n");
    }

    TabInfo *tab = get_current_tab_info();
    if (tab && tab->auto_scroll_enabled) {
        GtkTextMark *insert = gtk_text_buffer_get_insert(tab->buffer); /* caret mark [6] */
        gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(tab->text_view),
                                     insert,
                                     tab->auto_scroll_within, TRUE, 0.0, tab->auto_scroll_yalign); /* align on activation [4] */
    }
}

static void on_tab_close_button_clicked(GtkButton *button, gpointer user_data) {
    (void)button; (void)user_data;
    close_current_tab();
}

/* ---------------------------
   Create a new tab/page (GtkSourceView)
   --------------------------- */

static void create_tab_internal(const char *filename, gboolean hide_sidebar) {
    g_print("create_tab_internal: filename='%s', hide_sidebar=%s\n",
            filename ? filename : "NULL", hide_sidebar ? "TRUE" : "FALSE");
    show_notebook();

    if (!global_notebook) {
        g_warning("Cannot create tab: notebook not initialized yet");
        return;
    }

    /* Switch to already-open file if present */
    if (filename && *filename) {
        for (int i = 0; i < gtk_notebook_get_n_pages(global_notebook); ++i) {
            GtkWidget *page = gtk_notebook_get_nth_page(global_notebook, i); /* page by index [12] */
            if (!page) continue;
            TabInfo *info = (TabInfo*)g_object_get_data(G_OBJECT(page), "tab_info"); /* retrieve TabInfo [13] */
            if (info && info->filename && strcmp(info->filename, filename) == 0) {
                gtk_notebook_set_current_page(global_notebook, i);
                g_print("File already open, switching to existing tab\n");
                return;
            }
        }
    }

    if (hide_sidebar) { hide_panels(); set_sidebar_visible(FALSE); } /* UI control [14] */

    /* Editor: SourceView inside ScrolledWindow (SourceView is a TextView subclass) */
    GtkWidget *scroller = gtk_scrolled_window_new();
    GtkSourceView *sview = GTK_SOURCE_VIEW(gtk_source_view_new());          /* source editor [5] */
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), GTK_WIDGET(sview)); /* direct child [3] */
    gtk_widget_set_hexpand(scroller, TRUE);
    gtk_widget_set_vexpand(scroller, TRUE);

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(sview));
    GtkSourceBuffer *sbuf = GTK_SOURCE_BUFFER(buffer);

    if (!scroller || !sview || !buffer) {
        g_error("Failed to create tab UI elements");
        return;
    }

    /* Editor features */
    gtk_source_view_set_show_line_numbers(sview, TRUE);                    /* built-in gutter [1] */
    gtk_source_view_set_highlight_current_line(sview, TRUE);               /* QoL highlight [5] */
    gtk_source_view_set_tab_width(sview, 4);
    gtk_source_view_set_insert_spaces_instead_of_tabs(sview, TRUE);

    gtk_text_view_set_monospace(GTK_TEXT_VIEW(sview), TRUE);               /* monospace font [5] */
    gtk_text_buffer_set_enable_undo(buffer, TRUE);                         /* undo enabled [6] */
    gtk_text_buffer_set_max_undo_levels(buffer, 100);                      /* bound history [15] */

    /* Guess language from filename and enable syntax */
    GtkSourceLanguageManager *lm = gtk_source_language_manager_get_default(); /* language manager [2] */
    GtkSourceLanguage *lang = gtk_source_language_manager_guess_language(lm, filename, NULL); /* by filename [2] */
    if (lang) {
        gtk_source_buffer_set_language(sbuf, lang);
        gtk_source_buffer_set_highlight_syntax(sbuf, TRUE);
    }

    /* Optional: style scheme (e.g., “oblivion” or any installed scheme id) */
    GtkSourceStyleSchemeManager *sm = gtk_source_style_scheme_manager_get_default(); /* scheme manager [16] */
    GtkSourceStyleScheme *scheme = gtk_source_style_scheme_manager_get_scheme(sm, "oblivion");
    if (scheme) {
        gtk_source_buffer_set_style_scheme(sbuf, scheme);                  /* apply scheme [17] */
    }

    /* TabInfo */
    TabInfo *tab = g_new0(TabInfo, 1);
    tab->scrolled_window  = scroller;                                      /* page widget */
    tab->text_view        = GTK_WIDGET(sview);                              /* editor view */
    tab->buffer           = buffer;                                         /* text buffer */
    tab->filename         = (filename && *filename) ? g_strdup(filename) : NULL;
    tab->dirty            = FALSE;
    tab->lang_type        = get_language_from_filename(filename);           /* keep existing type map */
    tab->ts_tree          = NULL;

    tab->auto_scroll_enabled = TRUE;
    tab->auto_scroll_yalign  = 0.30;
    tab->auto_scroll_within  = 0.10;

    tab->highlight_source_id    = 0;
    tab->buffer_changed_handler = 0;
    tab->cursor_mark_handler    = 0;
    tab->modified_close_handler = 0;

    /* If also applying custom tags, prepare them; else can be omitted */
    setup_highlighting_tags(buffer);

    /* Load file content (block 'changed' while setting text to avoid dirty) */
    if (filename && *filename) {
        gchar *contents = NULL;
        GError *err = NULL;
        g_print("Loading file content: %s\n", filename);
        if (g_file_get_contents(filename, &contents, NULL, &err)) {
            g_signal_handlers_block_by_func(buffer, G_CALLBACK(on_buffer_changed), tab);
            gtk_text_buffer_set_text(buffer, contents, -1);                /* irreversible replace [18] */
            g_signal_handlers_unblock_by_func(buffer, G_CALLBACK(on_buffer_changed), tab);

            /* If Tree-sitter overlays are used, call once here; else rely on GtkSource syntax */
            /* highlight_buffer_sync(buffer, &tab->ts_tree, tab->lang_type); */

            add_to_recent_files(filename);
            g_free(contents);
            g_print("Successfully loaded file: %s\n", filename);
        } else {
            g_warning("Failed to load file %s: %s", filename, err ? err->message : "Unknown error");
            if (err) g_error_free(err);
        }
    }

    /* Tab header with close button */
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

    /* Store TabInfo on page; free on destroy */
    g_object_set_data_full(G_OBJECT(scroller), "tab_info", tab, g_free);   /* attach data [19] */

    /* Connect handlers */
    tab->buffer_changed_handler = g_signal_connect(buffer, "changed",  G_CALLBACK(on_buffer_changed),  tab); /* dirty + debounce [6] */
    tab->cursor_mark_handler    = g_signal_connect(buffer, "mark-set", G_CALLBACK(on_cursor_mark_set), tab); /* caret-follow [6] */
    g_signal_connect(close_btn, "clicked", G_CALLBACK(on_tab_close_button_clicked), NULL);

    /* Add page and focus editor */
    gtk_notebook_append_page(global_notebook, scroller, tab_label_box);    /* add page [14] */
    gtk_notebook_set_current_page(global_notebook, gtk_notebook_get_n_pages(global_notebook) - 1); /* select [14] */
    gtk_widget_grab_focus(GTK_WIDGET(sview));
    g_print("Tab creation completed successfully\n");
}

void create_new_tab(const char *filename) { create_tab_internal(filename, TRUE); }      /* hide sidebar [14] */
void create_new_tab_from_sidebar(const char *filename) { create_tab_internal(filename, FALSE); } /* keep sidebar [14] }

/* ---------------------------
   Save-before-close flow
   --------------------------- */

static void on_buffer_modified_for_close(GtkTextBuffer *buffer, gpointer user_data) {
    TabInfo *tab = (TabInfo*)user_data;
    if (!tab) return;
    if (!gtk_text_buffer_get_modified(buffer)) {                              /* modified -> FALSE [20] */
        if (tab->modified_close_handler) {
            g_signal_handler_disconnect(buffer, tab->modified_close_handler);
            tab->modified_close_handler = 0;
        }
        tab->dirty = FALSE;
        close_current_tab();
    }
}

static void on_confirm_close_response(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    gint choice = gtk_alert_dialog_choose_finish(GTK_ALERT_DIALOG(source_object), res, &error); /* async finish [21] */
    TabInfo *tab = (TabInfo*)user_data;
    if (!tab) return;

    if (error) {
        g_warning("Alert dialog error: %s", error->message);
        g_error_free(error);
        return;
    }

    if (choice == 1) { /* Save */
        if (tab->filename && *tab->filename) {
            save_current_tab();
            tab->dirty = FALSE;
            close_current_tab();
        } else {
            save_current_tab(); /* Save As async path */
            if (!tab->modified_close_handler) {
                tab->modified_close_handler =
                    g_signal_connect(tab->buffer, "modified-changed",
                                     G_CALLBACK(on_buffer_modified_for_close), tab); /* observe clean [20] */
            }
        }
    } else if (choice == 2) { /* Close without Saving */
        tab->dirty = FALSE;
        gtk_text_buffer_set_modified(tab->buffer, FALSE);                      /* reflect state [20] */
        close_current_tab();
    }
    /* 0 = Cancel -> do nothing */
}

/* ---------------------------
   Close current tab
   --------------------------- */

gboolean close_current_tab(void) {
    TabInfo *tab = get_current_tab_info();
    if (!tab || !global_notebook) return FALSE;

    if (tab->dirty) {
        char *base = tab->filename ? g_path_get_basename(tab->filename) : NULL;
        const char *shown = base ? base : "Untitled";
        GtkAlertDialog *dlg = gtk_alert_dialog_new("Save changes to \"%s\" before closing?", shown); /* alert [22] */
        gtk_alert_dialog_set_detail(dlg, "Your changes will be lost if you don't save them.");
        const char* buttons[] = {"Cancel", "_Save", "Close without Saving", NULL};
        gtk_alert_dialog_set_buttons(dlg, buttons);
        gtk_alert_dialog_set_default_button(dlg, 1);
        gtk_alert_dialog_set_cancel_button(dlg, 0);
        gtk_alert_dialog_choose(dlg, GTK_WINDOW(global_window), NULL, on_confirm_close_response, tab); /* async choose [23] */
        g_free(base);
        return TRUE;
    }

    /* Cancel debounce */
    if (tab->highlight_source_id) { g_source_remove(tab->highlight_source_id); tab->highlight_source_id = 0; } /* GLib [7] */

    /* Disconnect handlers */
    if (tab->buffer && tab->buffer_changed_handler) {
        g_signal_handler_disconnect(tab->buffer, tab->buffer_changed_handler); tab->buffer_changed_handler = 0; /* [6] */
    }
    if (tab->buffer && tab->cursor_mark_handler) {
        g_signal_handler_disconnect(tab->buffer, tab->cursor_mark_handler);    tab->cursor_mark_handler = 0;    /* [6] */
    }
    if (tab->buffer && tab->modified_close_handler) {
        g_signal_handler_disconnect(tab->buffer, tab->modified_close_handler); tab->modified_close_handler = 0;  /* [20] */
    }

#ifdef HAVE_TREE_SITTER
    if (tab->ts_tree) {
        TSTree *t = (TSTree*)tab->ts_tree;
        ts_tree_delete(t);                                                     /* free parse tree [24] */
        tab->ts_tree = NULL;
    }
#endif

    /* Remove the page; TabInfo freed via g_object_set_data_full destroy notify */
    int page = gtk_notebook_get_current_page(global_notebook);                 /* index [25] */
    gtk_notebook_remove_page(global_notebook, page);                           /* remove [14] */
    return FALSE;
}

/* ---------------------------
   Utilities
   --------------------------- */

TabInfo* get_current_tab_info(void) {
    if (!global_notebook) return NULL;
    int idx = gtk_notebook_get_current_page(global_notebook);                  /* -1 if none [25] */
    if (idx < 0) return NULL;
    GtkWidget *page = gtk_notebook_get_nth_page(global_notebook, idx);         /* page by index [12] */
    if (!page) return NULL;
    return (TabInfo*)g_object_get_data(G_OBJECT(page), "tab_info");            /* TabInfo retrieval [13] */
}

void set_auto_scroll_enabled_current(gboolean enabled) {
    TabInfo *tab = get_current_tab_info();
    if (!tab) return;
    tab->auto_scroll_enabled = enabled;
    if (enabled) {
        GtkTextMark *insert = gtk_text_buffer_get_insert(tab->buffer);         /* caret mark [6] */
        gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(tab->text_view),
                                     insert,
                                     tab->auto_scroll_within, TRUE, 0.0, tab->auto_scroll_yalign); /* align now [4] */
    }
}
