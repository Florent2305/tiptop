CFLAGS=-O2 -Wall -DKERNEL31 
CFLAGS+=$(XCFLAGS)

all: tiptop

tiptop: tiptop.o pmc.o process.o requisite.o conf.o
	$(CC) -O2 -o tiptop tiptop.o pmc.o process.o requisite.o conf.o -lcurses


clean:
	/bin/rm -f tiptop.o pmc.o process.o requisite.o conf.o tiptop


depend:
	makedepend -Y *.c

# DO NOT DELETE

conf.o: conf.h
pmc.o: pmc.h
process.o: pmc.h process.h
requisite.o: pmc.h
tiptop.o: requisite.h pmc.h process.h conf.h
