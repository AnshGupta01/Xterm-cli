# Makefile for MyTerm - Custom Terminal with X11 GUI
# Optimized for macOS with XQuartz

# Compiler settings
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2 -g
INCLUDES = -I/opt/X11/include -Iinclude
LDFLAGS = -L/opt/X11/lib
LIBS = -lX11 -lXext

# Directories
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
TARGET = myterm

# Source files
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
DEPS = $(OBJECTS:.o=.d)

# Default target
all: $(TARGET)

# Create build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build target
$(TARGET): $(BUILD_DIR) $(OBJECTS)
	@echo "Linking $(TARGET)..."
	$(CC) $(OBJECTS) $(LDFLAGS) $(LIBS) -o $(TARGET)
	@echo "Build complete! Run with: ./$(TARGET)"

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

# Include dependency files
-include $(DEPS)

# Clean build files
clean:
	@echo "Cleaning build files..."
	rm -rf $(BUILD_DIR)
	rm -f $(TARGET)
	rm -f .temp.*.txt
	rm -f .myterm_history

# Install XQuartz (requires Homebrew)
install-deps:
	@echo "Installing XQuartz via Homebrew..."
	@if command -v brew >/dev/null 2>&1; then \
		brew install --cask xquartz; \
		echo "Please log out and log back in to complete XQuartz installation."; \
	else \
		echo "Homebrew not found. Please install XQuartz manually from https://www.xquartz.org"; \
	fi

# Setup environment for XQuartz
setup-env:
	@echo "Setting up environment for XQuartz..."
	@echo "export DISPLAY=:0" >> ~/.bashrc
	@echo "export DISPLAY=:0" >> ~/.zshrc
	@echo "Environment setup complete. Please restart your terminal or run: source ~/.bashrc"

# Test XQuartz installation
test-x11:
	@echo "Testing X11 installation..."
	@if [ -d "/opt/X11" ]; then \
		echo "✓ XQuartz is installed"; \
		if pgrep -f "XQuartz" > /dev/null; then \
			echo "✓ XQuartz is running"; \
		else \
			echo "✗ XQuartz is not running. Please start XQuartz application."; \
		fi; \
	else \
		echo "✗ XQuartz is not installed. Run 'make install-deps'"; \
	fi
	@echo "If X11 test fails, try: export DISPLAY=:0"

# Run the program
run: $(TARGET)
	@echo "Starting MyTerm..."
	@if ! pgrep -f "XQuartz" > /dev/null; then \
		echo "Starting XQuartz..."; \
		open -a XQuartz; \
		sleep 3; \
	fi
	@export DISPLAY=:0 && ./$(TARGET)

# Debug version
debug: CFLAGS += -DDEBUG -g3 -O0
debug: $(TARGET)

# Help target
help:
	@echo "MyTerm Makefile - Available targets:"
	@echo "  all          - Build the terminal application"
	@echo "  clean        - Remove all build files"
	@echo "  install-deps - Install XQuartz via Homebrew"
	@echo "  setup-env    - Setup environment variables"
	@echo "  test-x11     - Test XQuartz installation"
	@echo "  run          - Build and run the application"
	@echo "  debug        - Build debug version"
	@echo "  help         - Show this help message"
	@echo ""
	@echo "Requirements:"
	@echo "  - macOS with XQuartz installed"
	@echo "  - Xcode Command Line Tools (xcode-select --install)"
	@echo ""
	@echo "First time setup:"
	@echo "  1. make install-deps"
	@echo "  2. Log out and log back in"
	@echo "  3. make setup-env"
	@echo "  4. make test-x11"
	@echo "  5. make run"

# Phony targets
.PHONY: all clean install-deps setup-env test-x11 run debug help
