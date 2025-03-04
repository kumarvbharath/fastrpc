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
BASE_SRC = $(wildcard $(SRC_DIR)/*.c)
CORE_SRC = $(wildcard $(SRC_DIR)/core/*.c)
REMOTE_SRC = $(wildcard $(SRC_DIR)/remote/*.c)
TEST_SRC = $(wildcard $(TEST_DIR)/test_*.c)

# All source files
SRC_FILES = $(BASE_SRC) $(CORE_SRC) $(REMOTE_SRC)

# Object files
OBJ_FILES = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC_FILES))
TEST_OBJ = $(patsubst $(TEST_DIR)/%.c,$(OBJ_DIR)/%.o,$(TEST_SRC))

# Test targets
TEST_BINS = test_fastrpc test_rpcmem test_config test_remote

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
	@echo $(OBJ_FILES)
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
test_%: $(OBJ_FILES) $(OBJ_DIR)/test_%.o
	$(CC) -o $@ $^ $(LDFLAGS)
	@echo "Running $@..."
	./$@ || (echo "Test $@ failed"; exit 1)

# Test target
test: $(TEST_BINS)
	@echo "All tests completed successfully"
	@mkdir -p $(COV_DIR)
	@for file in $(OBJ_FILES); do \
		folder=$$(dirname $$file | sed 's|obj/||'); \
		filename=$$(basename $$file .o); \
		src_file=$$(echo $$file | sed 's|obj|src|' | sed 's|.o|.c|'); \
    	gcov -f -o $$file $$src_file > $(COV_DIR)/$${folder}_$${filename}_coverage.txt; \
	done

# Coverage targets
coverage:
	@echo "\nCode Coverage Summary:"
	@echo "======================"
	@grep "Lines executed" $(COV_DIR)/coverage.txt | sed 's/^/  /';

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