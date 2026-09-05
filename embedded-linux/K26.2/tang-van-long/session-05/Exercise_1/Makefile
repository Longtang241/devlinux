CC = gcc
CFLAGS = -Wall -Wextra -pthread

TARGET = booking
SOURCE = booking.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(SOURCE) -o $(TARGET)

clean:
	rm -f $(TARGET)

