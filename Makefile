CC = gcc
# Add coverage flags to CFLAGS
CFLAGS = -Wall -Wextra -pthread -I./inc -fPIC -g --coverage -fprofile-arcs -ftest-coverage
# Add gcov library to LDFLAGS
LDFLAGS = -lpthread -lgcov --coverage
LIBFLAGS = -shared

# Directory structure
SRC_DIR = src
TEST_DIR = test
OBJ_DIR = obj
LIB_DIR = lib

# Add coverage directory
COV_DIR = coverage

# Coverage flags
GCOV_FLAGS = -fprofile-arcs -ftest-coverage
CFLAGS += $(GCOV_FLAGS)
LDFLAGS += --coverage

# Source files
FASTRPC_SRC = $(wildcard $(SRC_DIR)/fastrpc/*.c)
SRC_FILES = $(FASTRPC_SRC)

# Remote files
REMOTE_SRC = $(wildcard $(SRC_DIR)/remote/*.c)
SRC_FILES += $(REMOTE_SRC)

# Test files
TEST_FILES = $(wildcard $(TEST_DIR)/*.c)

# Object files
OBJ_FILES = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC_FILES))
TEST_OBJ_FILES = $(patsubst $(TEST_DIR)/%.c,$(OBJ_DIR)/%.o,$(TEST_FILES))

# Targets
LIB_TARGET = $(LIB_DIR)/libdsprpc.so
TEST_TARGETS = $(patsubst $(TEST_DIR)/test_%.c,%_test,$(TEST_FILES))

.PHONY: all test

# Default target builds everything and runs tests
all: $(LIB_TARGET) test coverage-summary

# Library target
$(LIB_TARGET): $(OBJ_FILES)
	@mkdir -p $(LIB_DIR)
	$(CC) $(LIBFLAGS) -o $@ $^ $(LDFLAGS)

# Object files for fastrpc
$(OBJ_DIR)/fastrpc/%.o: $(SRC_DIR)/fastrpc/%.c
	@mkdir -p $(OBJ_DIR)/fastrpc
	$(CC) $(CFLAGS) -c -o $@ $<

# Object files for remote
$(OBJ_DIR)/remote/%.o: $(SRC_DIR)/remote/%.c
	@mkdir -p $(OBJ_DIR)/remote
	$(CC) $(CFLAGS) -c -o $@ $<

# Object files for tests
$(OBJ_DIR)/%.o: $(TEST_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Test targets
%_test: $(OBJ_FILES) $(OBJ_DIR)/test_%.o
	$(CC) -o $@ $^ $(LDFLAGS)
	@echo "Running $@..."
	@mkdir -p $(COV_DIR)/$*
	./$@ || exit 1
	@for dir in fastrpc remote; do \
		mkdir -p $(OBJ_DIR)/$$dir; \
		mv -f $$dir/*.gcda $(OBJ_DIR)/$$dir/ 2>/dev/null || true; \
		mv -f $$dir/*.gcno $(OBJ_DIR)/$$dir/ 2>/dev/null || true; \
	done
	@gcov -o $(OBJ_DIR)/fastrpc -o $(OBJ_DIR)/remote $(SRC_FILES) 2>/dev/null | \
		grep -v "No executable lines" | \
		grep -v "cannot open" > $(COV_DIR)/$*/coverage.txt

# Test rule runs all tests
test: $(TEST_TARGETS)

# Clean rule
clean:
	rm -rf $(OBJ_DIR) $(LIB_DIR) $(COV_DIR) *_test
	find . -name "*.gc*" -delete
	find . -name "*.gcov" -delete

# Add coverage report generation
coverage-%: %_test
	@echo "Generating coverage report for $*..."
	@gcov -o $(OBJ_DIR) $(SRC_FILES) 2>/dev/null | \
		grep -v "No executable lines" | \
		grep -v "cannot open" > $(COV_DIR)/$*/coverage.txt
	@echo "Coverage files:"
	@find $(OBJ_DIR) -name "*.gc*" -ls
	@echo "Coverage summary for $*:"
	@grep "File '.*'" $(COV_DIR)/$*/coverage.txt || true
	@grep "Lines executed" $(COV_DIR)/$*/coverage.txt || true

# Add target for all coverage reports
coverage: $(TEST_TARGETS)
	@mkdir -p $(COV_DIR)
	@echo "Generating coverage reports..."
	@for test in $(TEST_TARGETS); do \
		mkdir -p $(COV_DIR)/$$test; \
		gcov -o $(OBJ_DIR) $(SRC_FILES) 2>/dev/null | \
			grep -v "No executable lines" | \
			grep -v "cannot open" > $(COV_DIR)/$$test/coverage.txt; \
	done
	@echo "Coverage reports available in $(COV_DIR)/"

# Add coverage summary target
coverage-summary: $(TEST_TARGETS)
	@echo "\nCode Coverage Summary:"
	@echo "======================"
	@for test in $(TEST_TARGETS); do \
		echo "\nCoverage for $$test:"; \
		mkdir -p $(COV_DIR)/$$test; \
		gcov -o $(OBJ_DIR) $(SRC_FILES) 2>/dev/null | \
			grep -v "No executable lines" | \
			grep -v "cannot open" | \
			grep "Lines executed" | \
			sed 's/^/  /'; \
	done
	@echo "\nDetailed coverage reports available in $(COV_DIR)/"

# Show help
help:
	@echo "Available targets:"
	@echo "  all        - Build library, run tests and show coverage (default)"
	@echo "  test       - Build and run all tests"
	@echo "  clean      - Remove all built files and coverage data"
	@echo "  coverage-summary - Show coverage summary for all tests"
	@echo "  coverage-<test> - Generate coverage report for specific test"
	@echo "  help       - Show this help message"