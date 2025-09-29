UNAME_S := $(shell uname -s)

# Compiler and flags
CC = gcc
CFLAGS = -O2 -Wall -Wextra -std=c11
SRC = src/gui_main.c
BIN = myterm

# Platform-specific X11 flags
ifeq ($(UNAME_S),Darwin)
    # macOS with XQuartz
    CFLAGS += -I/opt/X11/include
    LDFLAGS = -L/opt/X11/lib -lX11 -lXext
else
    # Linux
    LDFLAGS = -lX11
endif

# Default target
all: $(BIN)

# Link the binary
$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $(BIN) $(SRC) $(LDFLAGS)

# Run convenience target
run: $(BIN)
	./$(BIN)

# Clean build artifacts
clean:
	rm -f $(BIN)
