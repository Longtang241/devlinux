CC     = gcc
CFLAGS = -Wall -Wextra -g

.PHONY: all clean

all: manager searcher

manager: manager.c
	$(CC) $(CFLAGS) -o manager manager.c

searcher: searcher.c
	$(CC) $(CFLAGS) -o searcher searcher.c

clean:
	rm -f manager searcher