CC = gcc
CFLAGS = -Wall -Wextra -pthread -I./inc -fPIC -g
LDFLAGS = -lpthread
LIBFLAGS = -shared

# Directory structure
SRC_DIR = src
TEST_DIR = test
OBJ_DIR = obj
LIB_DIR = lib

# Source files
FASTRPC_SRC = $(wildcard $(SRC_DIR)/fastrpc/*.c)
SRC_FILES = $(FASTRPC_SRC)

# Test files
TEST_FILES = $(wildcard $(TEST_DIR)/*.c)

# Object files
OBJ_FILES = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC_FILES))
TEST_OBJ_FILES = $(patsubst $(TEST_DIR)/%.c,$(OBJ_DIR)/%.o,$(TEST_FILES))

# Targets
LIB_TARGET = $(LIB_DIR)/libdsprpc.so
TEST_TARGETS = $(patsubst $(TEST_DIR)/test_%.c,%_test,$(TEST_FILES))

.PHONY: all clean test

# Default target builds everything and runs tests
all: $(LIB_TARGET) test

# Library target
$(LIB_TARGET): $(OBJ_FILES)
	@mkdir -p $(LIB_DIR)
	$(CC) $(LIBFLAGS) -o $@ $^ $(LDFLAGS)

# Object files for fastrpc
$(OBJ_DIR)/fastrpc/%.o: $(SRC_DIR)/fastrpc/%.c
	@mkdir -p $(OBJ_DIR)/fastrpc
	$(CC) $(CFLAGS) -c -o $@ $<

# Object files for tests
$(OBJ_DIR)/%.o: $(TEST_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Test targets
%_test: $(OBJ_FILES) $(OBJ_DIR)/test_%.o
	$(CC) -o $@ $^ $(LDFLAGS)
	@echo "Running $@..."
	./$@

# Test rule runs all tests
test: $(TEST_TARGETS)

# Clean rule
clean:
	rm -rf $(OBJ_DIR) $(LIB_DIR) *_test

# Show help
help:
	@echo "Available targets:"
	@echo "  all        - Build library and run all tests (default)"
	@echo "  test       - Build and run all tests"
	@echo "  clean      - Remove all built files"
	@echo "  help       - Show this help message"