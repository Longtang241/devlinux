CC = gcc
CFLAGS = -Wall -Wextra -pthread

TARGET = print_queue
SOURCE = print_queue.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(SOURCE) -o $(TARGET)

clean:
	rm -f $(TARGET)
