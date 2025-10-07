CC       = gcc
CFLAGS   = -Wall -std=c99 -Iinclude -I/opt/X11/include
LDFLAGS  = -L/opt/X11/lib
LIBS     = -lX11

SRC_DIR   = src
INC_DIR   = include
BUILD_DIR = build
BIN       = $(BUILD_DIR)/xterm

SRCS := $(SRC_DIR)/main.c \
        $(SRC_DIR)/terminal.c \
        $(SRC_DIR)/gui.c \
        $(SRC_DIR)/commands.c

OBJS := $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Default target
all: $(BIN)

# Ensure build directory exists before compiling any object
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile: src/foo.c -> build/foo.o (requires build/ to exist)
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(INC_DIR)/xterm.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link all objects into build/xterm
$(BIN): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $(BIN) $(LIBS)

# Run from build
run: $(BIN)
	./$(BIN)

# Clean artifacts
clean:
	rm -rf $(BUILD_DIR)

re: clean all

.PHONY: all run clean re