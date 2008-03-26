CC ?= gcc
CFLAGS = -Wall -O2 -g

all:	test

%.o:	%.c
	$(CC) $(CFLAGS) -c $<

directedgraph.o:	directedgraph.c directedgraph.h
test.o:	test.c directedgraph.h

test:	test.o directedgraph.o
	$(CC) $(CFLAGS) -o test test.o directedgraph.o

clean:	
	rm -f *.o test
