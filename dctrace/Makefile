#
# Makefile for the trace utility.
#
# M. Tim Jones <mtj@mtjones.com>
# Edited by: Andress Barajas
#

CC = gcc

OBJS = trace.o symbols.o stack.o priorityqueue.o

dctrace: $(OBJS)
	gcc -o $@ $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -Wall -c $<

install: dctrace
	cp dctrace /usr/local/bin

clean:
	rm -f dctrace *.o
