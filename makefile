# Makefile for GPad Multi-Tab Editor

CC = gcc
CFLAGS = -Wall -Wextra -std=c11
PKGS = gtk4

# Get GTK+ compilation flags
GTK_CFLAGS = $(shell pkg-config --cflags $(PKGS))
GTK_LIBS = $(shell pkg-config --libs $(PKGS))

# Main source files
MAIN_SOURCES = main.c tabs.c file_ops.c file_browser.c ui_panels.c actions.c syntax.c
MAIN_OBJECTS = $(MAIN_SOURCES:.c=.o)

# Tree-sitter parser objects
TREE_SITTER_OBJECTS = parser.o python_parser.o python_scanner.o dart_parser.o dart_scanner.o

# Target executable
TARGET = gpad

.PHONY: all clean install uninstall with-treesitter parsers help

# Default build (without tree-sitter)
all: clean-objects $(MAIN_OBJECTS)
	$(CC) $(MAIN_OBJECTS) -o $(TARGET) $(GTK_LIBS)

# Build with tree-sitter (your compilation method)
with-treesitter: clean-objects
	@echo "Building with tree-sitter support..."
	@$(MAKE) parsers
	@$(MAKE) compile-with-treesitter

compile-with-treesitter: $(TREE_SITTER_OBJECTS)
	@echo "Compiling main sources with tree-sitter..."
	$(CC) -DHAVE_TREE_SITTER $(GTK_CFLAGS) $(CFLAGS) -c main.c -o main.o
	$(CC) -DHAVE_TREE_SITTER $(GTK_CFLAGS) $(CFLAGS) -c tabs.c -o tabs.o
	$(CC) -DHAVE_TREE_SITTER $(GTK_CFLAGS) $(CFLAGS) -c file_ops.c -o file_ops.o
	$(CC) -DHAVE_TREE_SITTER $(GTK_CFLAGS) $(CFLAGS) -c file_browser.c -o file_browser.o
	$(CC) -DHAVE_TREE_SITTER $(GTK_CFLAGS) $(CFLAGS) -c ui_panels.c -o ui_panels.o
	$(CC) -DHAVE_TREE_SITTER $(GTK_CFLAGS) $(CFLAGS) -c actions.c -o actions.o
	$(CC) -DHAVE_TREE_SITTER $(GTK_CFLAGS) $(CFLAGS) -c syntax.c -o syntax.o
	@echo "Linking final executable..."
	$(CC) -DHAVE_TREE_SITTER $(MAIN_OBJECTS) $(TREE_SITTER_OBJECTS) -o $(TARGET) $(GTK_LIBS) -ltree-sitter

# Compile main source files (default)
%.o: %.c gpad.h
	$(CC) $(GTK_CFLAGS) $(CFLAGS) -c $< -o $@

# Build tree-sitter parsers
parsers: $(TREE_SITTER_OBJECTS)
	@echo "Tree-sitter parsers ready."

# Tree-sitter parser compilation rules
parser.o: parser.c
	@echo "Compiling C parser..."
	$(CC) $(CFLAGS) -c parser.c -o parser.o

python_parser.o: python_parser.c
	@echo "Compiling Python parser..."
	$(CC) $(CFLAGS) -c python_parser.c -o python_parser.o

python_scanner.o: python_scanner.c
	@echo "Compiling Python scanner..."
	$(CC) $(CFLAGS) -c python_scanner.c -o python_scanner.o

dart_parser.o: dart_parser.c
	@echo "Compiling Dart parser..."
	$(CC) $(CFLAGS) -c dart_parser.c -o dart_parser.o

dart_scanner.o: dart_scanner.c
	@echo "Compiling Dart scanner..."
	$(CC) $(CFLAGS) -c dart_scanner.c -o dart_scanner.o

clean-objects:
	@rm -f $(MAIN_OBJECTS)

clean:
	rm -f $(MAIN_OBJECTS) $(TREE_SITTER_OBJECTS) $(TARGET)

install: $(TARGET)
	install -Dm755 $(TARGET) /usr/local/bin/$(TARGET)

uninstall:
	rm -f /usr/local/bin/$(TARGET)

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: with-treesitter

# Help target
help:
	@echo "Available targets:"
	@echo "  all            - Build the editor (without tree-sitter)"
	@echo "  with-treesitter - Build with syntax highlighting support"
	@echo "  parsers        - Build only the tree-sitter parsers"
	@echo "  debug          - Build with debug symbols and tree-sitter"
	@echo "  clean          - Remove built files"
	@echo "  install        - Install to /usr/local/bin"
	@echo "  uninstall      - Remove from /usr/local/bin"
	@echo ""
	@echo "Tree-sitter parser files needed:"
	@echo "  parser.c, python_parser.c, python_scanner.c"
	@echo "  dart_parser.c, dart_scanner.c"
