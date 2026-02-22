# Essence Makefile
CC ?= gcc
CFLAGS = -g

# Directories
BUILD_DIR = build
SRC_DIR = source
INCLUDE_DIR = include

CFLAGS += -I$(INCLUDE_DIR) 

# Source files/objects
SRC_FILES = $(shell find $(SRC_DIR) -type f -name "*.c")
SRC_OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRC_FILES))

# Targets

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c Makefile
	-@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/essence: $(SRC_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(SRC_OBJECTS)

all: $(BUILD_DIR)/essence

install:
	cp $(BUILD_DIR)/essence $(DESTDIR)/usr/bin/essence

clean:
	rm -rf $(BUILD_DIR)
