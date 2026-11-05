CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = dish
SRG = shell.c

$(TARGET): $(SRC) shell.h
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)
