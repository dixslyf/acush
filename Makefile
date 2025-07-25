EXE := acush

BUILD_DIR := build
SRC_DIR := src

# Find all C source code files in the source directory.
SRCS := $(shell find $(SRC_DIR) -name '*.c')

# Prepend `BUILD_DIR` and appends `.o` to every source code file.
# E.g., `src/shell.c` -> `build/src/shell.c.o`.
SRC_OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)

# Makefiles generated by the compiler via the `-MMD` flag.
# Lists dependencies on `.c` and `.h` files for each object file.
# See https://gcc.gnu.org/onlinedocs/gcc/Preprocessor-Options.html.
OBJ_DEPS := $(SRC_OBJS:%.o=%.d)

# Find all folders in `src/`.
INC_DIRS := $(shell find $(SRC_DIR) -type d)

# Append `-I` to each folder's name in `src/` to let `gcc` know to include them when searching for headers.
CFLAGS := $(addprefix -I,$(INC_DIRS))

# Make the compiler generate Makefiles describing the object dependencies.
CFLAGS += -MMD -Wall

# Build the executable.
$(BUILD_DIR)/$(EXE): $(SRC_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

# Build the corresponding object file for each C source code file.
# At the same time, generate dependency Makefiles using `-MMD`.
$(BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Include the dependency Makefiles generated by the compiler.
# Dependencies are only useful when recompiling, so these dependencies
# are not too important for the first build.
-include $(OBJ_DEPS)

.PHONY: clean
clean:
	rm -r $(BUILD_DIR)
