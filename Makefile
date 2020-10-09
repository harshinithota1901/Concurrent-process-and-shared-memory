CC=gcc
CFLAGS=-Wall -ggdb

default: master palin

shared.o: shared.c palindrome.h
	$(CC) $(CFLAGS) -c shared.c

master: master.c palindrome.h shared.o
	$(CC) $(CFLAGS) master.c shared.o -o master

palin: palin.c palindrome.h shared.o
	$(CC) $(CFLAGS) palin.c shared.o -o palin

clean:
	rm -f master palin shared.o
	rm -f output.txt palin.out nopalin.out
