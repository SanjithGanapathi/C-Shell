# Compiler and Flags
# CC: The C compiler to use (gcc)
# CFLAGS: Compiler flags
# -Wall -Wextra: Enable all standard and extra warnings to catch potential bugs
# -g: Include debugging information in the executables
# -std=c99: Use the C99 standard for better compatibility
CC = gcc
CFLAGS = -Wall -Wextra -g -std=c99

# Executable names
SERVER_EXEC = server
CLIENT_EXEC = client

# Source and Header files
SERVER_SRC = server.c
CLIENT_SRC = client.c
HEADER = sham.h

# The default target, executed when you just run `make` or `make all`
# It depends on the server and client executables, so it will build both.
all: $(SERVER_EXEC) $(CLIENT_EXEC)

# Rule to build the server executable
# It depends on its source file and the shared header file.
# If either server.c or sham.h changes, this rule will be re-run.
$(SERVER_EXEC): $(SERVER_SRC) $(HEADER)
	$(CC) $(CFLAGS) -o $(SERVER_EXEC) $(SERVER_SRC)

# Rule to build the client executable
# It depends on its source file and the shared header file.
# If either client.c or sham.h changes, this rule will be re-run.
$(CLIENT_EXEC): $(CLIENT_SRC) $(HEADER)
	$(CC) $(CFLAGS) -o $(CLIENT_EXEC) $(CLIENT_SRC)

# Rule to clean up the directory
# Removes the compiled server and client executables
clean:
	rm -f $(SERVER_EXEC) $(CLIENT_EXEC)

# .PHONY tells make that 'all' and 'clean' are not actual files
.PHONY: all clean


