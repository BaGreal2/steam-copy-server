CC = gcc
CFLAGS = -Wall -Wextra
LDFLAGS = -lsqlite3

TARGET = server
SRC = server.c cJSON.c

$(TARGET): $(SRC)
	$(CC) $(SRC) $(CFLAGS) $(LDFLAGS) -o $(TARGET)
.PHONY: clean
clean:
	rm -f $(TARGET)
