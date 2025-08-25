# Compiler and Flags
CC = gcc
CFLAGS = -std=c99 \
         -D_POSIX_C_SOURCE=200809L \
         -D_XOPEN_SOURCE=700 \
         -Wall -Wextra -Werror \
         -Wno-unused-parameter \
         -fno-asm
LDFLAGS = # Add any linking flags here if needed, e.g., -lm

# Directories
# Assumes your .c files are in a 'src' directory and .h files are in 'include'
SRC_DIR = src
OBJ_DIR = obj
INCLUDE_DIR = include

# Find all .c source files in the source directory
SOURCES = $(wildcard $(SRC_DIR)/*.c)

# Create a list of object file names by replacing .c with .o and changing the directory
OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SOURCES))

# The final executable file, placed in the root directory
EXECUTABLE = shell.out

# --- Rules ---

# The default rule: 'make all' or just 'make'
all: $(EXECUTABLE)

# Rule to link the object files into the final executable
$(EXECUTABLE): $(OBJECTS)
	@echo "Linking..."
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "Build complete. Executable is at $(EXECUTABLE)"

# Rule to compile a .c source file into a .o object file
# This is a pattern rule that applies to all .c files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "Compiling $<..."
	@mkdir -p $(OBJ_DIR) # Create the 'obj' directory if it doesn't exist
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

# Rule to clean up the project (remove compiled files)
clean:
	@echo "Cleaning up..."
	rm -rf $(OBJ_DIR) $(EXECUTABLE)

# Phony targets are rules that don't represent actual files
.PHONY: all clean

