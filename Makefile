EXT = /opt/homebrew/
SRC_DIR = src
BIN_DIR = bin
TEST_DIR = test

CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11

INCLUDES =  -I$(SRC_DIR) -I$(EXT)include
LDFLAGS  = -L$(EXT)lib -Wl
LDLIBS   = -lcrypto -lz -Wl,-w

DEBUG ?= 1

ifeq ($(DEBUG), 1)
    CFLAGS  += -g -D_DEBUG -fsanitize=address
	LDFLAGS += -fsanitize=address
    BIN_DIR  = bin/debug
else
    CFLAGS += -O2
    BIN_DIR = bin/release
endif

# Detect OS
ifeq ($(OS), Windows_NT)
    MKDIR   = if not exist "$(BIN_DIR)" mkdir "$(BIN_DIR)"
    RM      = rmdir /s /q
    TARGET      = gordit.exe
    TEST_TARGET = $(BIN_DIR)/test.exe
	LDLIBS += -lws2_32
else
    MKDIR   = mkdir -p $(BIN_DIR)
    RM      = rm -rf
    TARGET      = gordit
    TEST_TARGET = $(BIN_DIR)/test
endif

# All source files except main.c
SRCS = $(filter-out $(SRC_DIR)/main.c, $(wildcard $(SRC_DIR)/*.c))
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BIN_DIR)/%.o, $(SRCS))

.PHONY: all test clean clean-all

all: $(TARGET)

$(TARGET): $(OBJS) $(BIN_DIR)/main.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_TARGET): $(OBJS) $(BIN_DIR)/test.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

# Compile src/*.c -> bin/*.o
$(BIN_DIR)/%.o: $(SRC_DIR)/%.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Compile test/test.c -> bin/test.o
$(BIN_DIR)/test.o: $(TEST_DIR)/test.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(BIN_DIR):
	$(MKDIR)

clean:
	$(RM) $(BIN_DIR)

clean-all:
ifeq ($(OS), Windows_NT)
	rmdir /s /q bin
else
	rm -rf bin
endif