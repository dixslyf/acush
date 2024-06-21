CC := gcc

EXE := shell

BUILD_DIR := build
SRC_DIR := src
TESTS_DIR := tests
TEST_RUNNER := $(TESTS_DIR)/run-tests.sh

# Find all C source code files in the source directory.
SRCS := $(shell find $(SRC_DIR) -name '*.c')

# Prepend `BUILD_DIR` and appends `.o` to every source code file.
# E.g., `src/shell.c` -> `build/src/shell.c.o`.
SRC_OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)

# Find all folders in `src/`.
INC_DIRS := $(shell find $(SRC_DIR) -type d)

# Append `-I` to each folder's name in `src/` to let `gcc` know to include them when searching for headers.
CFLAGS := $(addprefix -I,$(INC_DIRS))

# Find all C source code files in the tests directory.
TESTS := $(shell find $(TESTS_DIR) -name '*.c')

# Prepend `BUILD_DIR` to and remove `.c` from every test source code file.
# E.g., `tests/foobar.c` -> `build/tests/foobar`.
TEST_EXES := $(TESTS:%.c=$(BUILD_DIR)/%)

# Like `TEST_EXES`, but with each string in single quotes for passing to a shell as arguments.
TEST_EXES_QUOTED := $(TEST_EXES:%='%')

# Build the executable.
$(BUILD_DIR)/$(EXE): $(SRC_OBJS)
	$(CC) $^ -o $@

# Build the corresponding object file for each C source code file.
$(BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $^ -o $@

# Run tests.
test: $(TEST_EXES)
	$(TEST_RUNNER) $(TEST_EXES_QUOTED)

# Build test executables.
$(BUILD_DIR)/$(TESTS_DIR)/%: $(TESTS_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(TESTS_DIR) $^ -o $@

.PHONY: clean
clean:
	rm -r $(BUILD_DIR)
