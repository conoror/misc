CC=gcc

imginf: imginf.h imginf.c ggetopt.h ggetopt.c jpginf.c exif.c pnginf.c
	$(CC) -Wall -O2 -o imginf imginf.c ggetopt.c jpginf.c exif.c pnginf.c -lm
