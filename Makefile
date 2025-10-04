# Makefile for ls-v1.0.0
# Author: Your Name / Roll Number
# Version: 1.0.0

# Compiler and flags
CC = gcc
CFLAGS = -Wall -g

# Directory paths
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# Target executable name
TARGET = $(BIN_DIR)/ls

# Source and object files
SRC = $(SRC_DIR)/ls-v1.5.0.c
OBJ = $(OBJ_DIR)/ls-v1.5.0.o

# Default target
all: $(TARGET)

# Build target
$(TARGET): $(OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

# Object file rule
$(OBJ): $(SRC)
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $(SRC) -o $(OBJ)

# Clean build files
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# Run the program
run: all
	./$(TARGET)

