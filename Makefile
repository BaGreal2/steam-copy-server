CC = gcc
CFLAGS = -Wall -Wextra -Iinclude $(addprefix -I, $(wildcard $(LIB_DIR)/*))
LDFLAGS = -lsqlite3

TARGET = bin/server

SRC_DIR = src
LIB_DIR = lib
OBJ_DIR = obj

SRC = $(wildcard $(SRC_DIR)/*.c) $(wildcard $(LIB_DIR)/*/*.c)
OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(wildcard $(SRC_DIR)/*.c)) \
      $(patsubst $(LIB_DIR)/%/*.c, $(OBJ_DIR)/%.o, $(wildcard $(LIB_DIR)/*/*.c))

$(TARGET): $(OBJ)
	$(CC) $(OBJ) $(CFLAGS) $(LDFLAGS) -o $(TARGET)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(LIB_DIR)/%/*.c
	mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(TARGET) $(OBJ_DIR)/*.o
