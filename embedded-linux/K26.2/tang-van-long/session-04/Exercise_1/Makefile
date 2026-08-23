CC     = gcc
CFLAGS = -Wall -Wextra -g

.PHONY: all clean

all: run

manager: manager.c
	$(CC) $(CFLAGS) -o manager manager.c

run: manager
	./manager

clean:
	rm -f manager