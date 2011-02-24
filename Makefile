CFLAGS=-O2 -Wall -DKERNEL31 -DHAS_CURSES

all: tiptop

tiptop: tiptop.o pmc.o process.o
	$(CC) -O2 -o tiptop tiptop.o pmc.o process.o -lcurses


clean:
	/bin/rm -f tiptop.o pmc.o process.o tiptop


depend:
	makedepend -Y *.c

# DO NOT DELETE

pmc.o: pmc.h
process.o: pmc.h process.h
tiptop.o: pmc.h process.h
