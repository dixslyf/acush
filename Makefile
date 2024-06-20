CC := gcc

EXE := shell

BUILD_DIR := build
SRC_DIR := src

# Find all C source code files in the source directory.
SRCS := $(shell find $(SRC_DIR) -name '*.c')

# Prepend `BUILD_DIR` and appends `.o` to every source code file.
# E.g., `src/shell.c` -> `build/src/shell.c.o`.
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)

# Find all folders in `src/`.
INC_DIRS := $(shell find $(SRC_DIR) -type d)

# Append `-I` to each folder's name in `src/` to let `gcc` know to include them when searching for headers.
CFLAGS := $(addprefix -I,$(INC_DIRS))

# Build the executable.
$(BUILD_DIR)/$(EXE): $(OBJS)
	$(CC) $^ -o $@

# Build the corresponding object file for each C source code file.
$(BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $^ -o $@

.PHONY: clean
clean:
	rm -r $(BUILD_DIR)

