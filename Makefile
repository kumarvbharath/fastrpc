# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -pthread -I./inc -fPIC -g
LDFLAGS = -lpthread
LIBFLAGS = -shared

# Coverage flags
GCOV_FLAGS = -fprofile-arcs -ftest-coverage
COV_LDFLAGS = -lgcov --coverage

# Directories
SRC_DIR = src
TEST_DIR = test
OBJ_DIR = obj
LIB_DIR = lib
COV_DIR = coverage

# Source organization
CORE_SRC = $(SRC_DIR)/init.c \
		   $(SRC_DIR)/procbuf.c
FASTRPC_SRC = $(wildcard $(SRC_DIR)/fastrpc/*.c)
REMOTE_SRC = $(wildcard $(SRC_DIR)/remote/*.c)
TEST_SRC = $(wildcard $(TEST_DIR)/test_*.c)

# All source files
SRC_FILES = $(CORE_SRC) $(FASTRPC_SRC) $(REMOTE_SRC)

# Object files
OBJ_FILES = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC_FILES))
TEST_OBJ = $(patsubst $(TEST_DIR)/%.c,$(OBJ_DIR)/%.o,$(TEST_SRC))

# Test targets
TEST_BINS = $(patsubst $(TEST_DIR)/test_%.c,%_test,$(TEST_SRC))

# Library target
LIB_TARGET = $(LIB_DIR)/libdsprpc.so

# Add coverage to flags when building tests
ifeq ($(MAKECMDGOALS),test)
	CFLAGS += $(GCOV_FLAGS)
	LDFLAGS += $(COV_LDFLAGS)
endif

.PHONY: all test coverage

all: $(LIB_TARGET)

# Library build
$(LIB_TARGET): $(OBJ_FILES)
	@mkdir -p $(LIB_DIR)
	$(CC) $(LIBFLAGS) -o $@ $^ $(LDFLAGS)

# Object file rules
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ_DIR)/%.o: $(TEST_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

# Test binary rules
%_test: $(OBJ_FILES) $(OBJ_DIR)/test_%.o
	$(CC) -o $@ $^ $(LDFLAGS)
	@echo "Running $@..."
	@mkdir -p $(COV_DIR)/$*
	./$@ || (echo "Test $@ failed"; exit 1)
	@gcov -o $(OBJ_DIR) $(SRC_FILES) > $(COV_DIR)/$*/coverage.txt

# Test target
test: $(TEST_BINS)
	@echo "All tests completed successfully"

# Coverage targets
coverage:
	@echo "\nCode Coverage Summary:"
	@echo "======================"
	@for test in $(TEST_BINS); do \
		echo "\n$$test:"; \
		grep "Lines executed" $(COV_DIR)/$${test%_test}/coverage.txt | sed 's/^/  /'; \
	done

clean:
	rm -rf $(OBJ_DIR) $(LIB_DIR) $(COV_DIR) *_test
	find . -name "*.gc*" -delete
	find . -name "*.gcov" -delete

help:
	@echo "Available targets:"
	@echo "  all              - Build library (default)"
	@echo "  test             - Build and run tests with coverage"
	@echo "  coverage		  - Show coverage summary"
	@echo "  clean            - Clean build artifacts"
	@echo "  help             - Show this help"