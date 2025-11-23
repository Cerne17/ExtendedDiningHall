# Makefile para Dining Hall Problem

CC = gcc
# Flags: -Wall (avisos), -pthread (threads), -O2 (otimização)
CFLAGS = -Wall -pthread -O2

TARGET = dining_hall
SRC = dining_hall.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET) 10
