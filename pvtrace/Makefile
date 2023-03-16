#
# Makefile for the trace utility.
#
# M. Tim Jones <mtj@mtjones.com>
# Edited by: Andress Barajas
#

CC = gcc

OBJS = trace.o symbols.o stack.o priorityqueue.o

pvtrace: $(OBJS)
	gcc -o $@ $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -Wall -c $<

install: pvtrace
	cp pvtrace /usr/local/bin

clean:
	rm -f pvtrace *.o
