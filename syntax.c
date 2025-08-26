#include "gpad.h"

#ifdef HAVE_TREE_SITTER

// Recursive function to traverse the syntax tree and apply tags
static void apply_tags_recursive(TSNode node, GtkTextBuffer *buffer, const char *source_code, LanguageType lang) {
    if (ts_node_is_null(node)) return;

    const char *type = ts_node_type(node);
    const char *tag_name = NULL;

    // Language-specific syntax highlighting
    if (lang == LANG_C) {
        if (strcmp(type, "comment") == 0) tag_name = "comment";
        else if (strcmp(type, "string_literal") == 0 || strcmp(type, "char_literal") == 0) tag_name = "string";
        else if (strstr(type, "preproc") != NULL) tag_name = "preproc";
        else if (strcmp(type, "return") == 0 || strcmp(type, "if") == 0 || strcmp(type, "for") == 0 ||
                strcmp(type, "while") == 0 || strcmp(type, "break") == 0 || strcmp(type, "case") == 0) tag_name = "control";
        else if (strcmp(type, "storage_class_specifier") == 0 || strcmp(type, "type_qualifier") == 0 ||
                strcmp(type, "struct") == 0 || strcmp(type, "typedef") == 0) tag_name = "keyword";
        else if (strcmp(type, "primitive_type") == 0 || strcmp(type, "type_identifier") == 0) tag_name = "type";
        else if (strcmp(type, "number_literal") == 0) tag_name = "number";
    } else if (lang == LANG_PYTHON) {
        if (strcmp(type, "comment") == 0) tag_name = "comment";
        else if (strcmp(type, "string") == 0) tag_name = "string";
        else if (strcmp(type, "from") == 0 || strcmp(type, "import") == 0 || strcmp(type, "as") == 0) tag_name = "preproc";
        else if (strcmp(type, "if") == 0 || strcmp(type, "for") == 0 || strcmp(type, "while") == 0 ||
                strcmp(type, "return") == 0 || strcmp(type, "in") == 0 || strcmp(type, "try") == 0 ||
                strcmp(type, "except") == 0) tag_name = "control";
        else if (strcmp(type, "def") == 0 || strcmp(type, "class") == 0 || strcmp(type, "pass") == 0) tag_name = "keyword";
        else if (strcmp(type, "type") == 0) tag_name = "type";
        else if (strcmp(type, "integer") == 0 || strcmp(type, "float") == 0) tag_name = "number";
        else if (strcmp(type, "decorator") == 0) tag_name = "decorator";
    } else if (lang == LANG_DART) {
        if (strcmp(type, "comment") == 0) tag_name = "comment";
        else if (strcmp(type, "string_literal") == 0) tag_name = "string";
        else if (strcmp(type, "import_directive") == 0 || strcmp(type, "export_directive") == 0) tag_name = "preproc";
        else if (strcmp(type, "if_statement") == 0 || strcmp(type, "for_statement") == 0 ||
                strcmp(type, "while_statement") == 0 || strcmp(type, "return_statement") == 0) tag_name = "control";
        else if (strcmp(type, "class_definition") == 0 || strcmp(type, "final") == 0 ||
                strcmp(type, "const") == 0 || strcmp(type, "static") == 0) tag_name = "keyword";
        else if (strcmp(type, "type_name") == 0 || strcmp(type, "primitive_type") == 0) tag_name = "type";
        else if (strcmp(type, "number_literal") == 0) tag_name = "number";
        else if (strcmp(type, "annotation") == 0) tag_name = "decorator";
    }

    if (tag_name) {
        uint32_t start_byte = ts_node_start_byte(node);
        uint32_t end_byte = ts_node_end_byte(node);

        GtkTextIter start_iter, end_iter;
        gtk_text_buffer_get_iter_at_offset(buffer, &start_iter, start_byte);
        gtk_text_buffer_get_iter_at_offset(buffer, &end_iter, end_byte);

        gtk_text_buffer_apply_tag_by_name(buffer, tag_name, &start_iter, &end_iter);
    }

    // Recursively process children
    for (uint32_t i = 0; i < ts_node_child_count(node); ++i) {
        apply_tags_recursive(ts_node_child(node, i), buffer, source_code, lang);
    }
}

// Main function to trigger highlighting
void highlight_buffer_sync(GtkTextBuffer *buffer, TSTreePtr *ts_tree, LanguageType lang) {
    if (lang == LANG_UNKNOWN || !ts_parser) return;

    TSTree **actual_tree = (TSTree**)ts_tree;  // Cast back to TSTree**

    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);

    if (gtk_text_iter_get_offset(&start) == gtk_text_iter_get_offset(&end)) return;

    char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

    // Remove existing tags
    gtk_text_buffer_remove_all_tags(buffer, &start, &end);

    // Set language for parser
    TSLanguage *ts_lang = NULL;
    if (lang == LANG_C) ts_lang = tree_sitter_c();
    else if (lang == LANG_PYTHON) ts_lang = tree_sitter_python();
    else if (lang == LANG_DART) ts_lang = tree_sitter_dart();

    if (ts_lang) {
        ts_parser_set_language(ts_parser, ts_lang);

        if (*actual_tree) ts_tree_delete(*actual_tree);
        *actual_tree = ts_parser_parse_string(ts_parser, NULL, text, strlen(text));

        if (*actual_tree) {
            TSNode root_node = ts_tree_root_node(*actual_tree);
            apply_tags_recursive(root_node, buffer, text, lang);
        }
    }

    g_free(text);
}

// Timeout callback for debounced highlighting
gboolean highlight_timeout_callback(gpointer user_data) {
    TabInfo *tab_info = (TabInfo*)user_data;
    highlight_buffer_sync(tab_info->buffer, &tab_info->ts_tree, tab_info->lang_type);
    return G_SOURCE_REMOVE;
}

// Initialize tree-sitter parser
void init_tree_sitter(void) {
    ts_parser = ts_parser_new();
    if (!ts_parser) {
        g_warning("Failed to create tree-sitter parser. Syntax highlighting disabled.");
    }
}

// Cleanup tree-sitter resources
void cleanup_tree_sitter(void) {
    if (ts_parser) {
        ts_parser_delete(ts_parser);
        ts_parser = NULL;
    }
}

#else
// Stub implementations when tree-sitter is not available

void highlight_buffer_sync(GtkTextBuffer *buffer, TSTreePtr *ts_tree, LanguageType lang) {
    // Do nothing when tree-sitter is not available
    (void)buffer;
    (void)ts_tree;
    (void)lang;
}

gboolean highlight_timeout_callback(gpointer user_data) {
    // Do nothing when tree-sitter is not available
    (void)user_data;
    return G_SOURCE_REMOVE;
}

void init_tree_sitter(void) {
    // Do nothing when tree-sitter is not available
}

void cleanup_tree_sitter(void) {
    // Do nothing when tree-sitter is not available
}

#endif // HAVE_TREE_SITTER
