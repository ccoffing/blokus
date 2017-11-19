CFLAGS?=-Wall -Wextra
LIBS?=-L/usr/local/lib
INCS?=-I/usr/local/include

CFLAGS+=-std=c99
LIBS+=-lSDL

release: CFLAGS+=-DNDEBUG -O2
release: blokus

debug: CFLAGS+=-DDEBUG -g
debug: blokus

blokus: blokus.c
	$(CC) $(CFLAGS) $(LIBS) $(INCS) blokus.c -o blokus

clean:
	rm -f blokus
