# Simple Makefile for GPad Multi-Tab Editor

CC = gcc
CFLAGS = -Wall -Wextra -std=c11
GTK_FLAGS = $(shell pkg-config --cflags --libs gtk4 gtksourceview-5)

# Source files
SOURCES = main.c tabs.c file_ops.c syntax.c file_browser.c ui_panels.c actions.c welcome.c
PARSERS = parser.o python_parser.o python_scanner.o dart_parser.o dart_scanner.o

TARGET = gpad

.PHONY: all with-treesitter clean help

# Default: build without tree-sitter
all:
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET) $(GTK_FLAGS)

# Build with tree-sitter (your method)
with-treesitter: $(PARSERS)
	$(CC) -DHAVE_TREE_SITTER $(CFLAGS) $(SOURCES) $(PARSERS) -o $(TARGET) $(GTK_FLAGS) -ltree-sitter

# Parser object rules
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(TARGET)

help:
	@echo "Targets:"
	@echo "  all            - Build without tree-sitter"
	@echo "  with-treesitter - Build with tree-sitter support"
	@echo "  clean          - Remove built files"
