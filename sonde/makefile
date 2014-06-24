# $Id: makefile,v 1.2 2008/10/09 11:26:58 walter Exp $
#CC=cris-gcc -mlinux
CC=gcc
CPPFLAGS=-I.
CFLAGS=-Wall -g 
LOADLIBES=-lm

sonde: sonde.o parse.o output.o

clean:
	$(RM) *.o *~ sonde
