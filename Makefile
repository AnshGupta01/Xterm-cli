UNAME_S := $(shell uname -s)

CC = gcc
CFLAGS = -O2 -Wall -Wextra -std=c11 -g
BIN = xterm
SRCDIR = src

# Platform-specific X11 flags
ifeq ($(UNAME_S),Darwin)
    CFLAGS += -I/opt/X11/include
    LDFLAGS = -L/opt/X11/lib -lX11 -lXext -lpthread
else
    LDFLAGS = -lX11 -lpthread
endif

# Source files
SOURCES = $(SRCDIR)/main.c \
          $(SRCDIR)/gui/gui.c \
          $(SRCDIR)/gui/tabs.c \
          $(SRCDIR)/shell/shell.c \
          $(SRCDIR)/shell/parser.c \
          $(SRCDIR)/shell/builtins.c \
          $(SRCDIR)/io/io.c \
          $(SRCDIR)/features/history.c \
          $(SRCDIR)/features/autocomplete.c \
          $(SRCDIR)/features/multiwatch.c

OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean run install

all: $(BIN)

$(BIN): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(BIN) $(OBJECTS) .temp.*

run: $(BIN)
	./$(BIN)

install: $(BIN)
	cp $(BIN) /usr/local/bin/

# Dependencies (simplified)
$(SRCDIR)/main.o: $(SRCDIR)/gui/gui.h $(SRCDIR)/shell/shell.h
$(SRCDIR)/gui/gui.o: $(SRCDIR)/gui/gui.h $(SRCDIR)/shell/shell.h
$(SRCDIR)/gui/tabs.o: $(SRCDIR)/gui/gui.h
$(SRCDIR)/shell/shell.o: $(SRCDIR)/shell/shell.h $(SRCDIR)/io/io.h
$(SRCDIR)/shell/parser.o: $(SRCDIR)/shell/shell.h
$(SRCDIR)/shell/builtins.o: $(SRCDIR)/shell/shell.h $(SRCDIR)/features/features.h
$(SRCDIR)/io/io.o: $(SRCDIR)/io/io.h
$(SRCDIR)/features/history.o: $(SRCDIR)/features/features.h
$(SRCDIR)/features/autocomplete.o: $(SRCDIR)/features/features.h
$(SRCDIR)/features/multiwatch.o: $(SRCDIR)/features/features.h