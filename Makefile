CFLAGS=-O2 -Wall -DKERNEL31 -DHAS_CURSES
CFLAGS+=$(XCFLAGS)

OBJS=tiptop.o pmc.o process.o requisite.o conf.o screen.o
all: tiptop

tiptop: $(OBJS)
	$(CC) -O2 -o tiptop $(OBJS) -lcurses


clean:
	/bin/rm -f $(OBJS) tiptop


depend:
	makedepend -Y *.c

# DO NOT DELETE

screen.o: screen.h
conf.o: conf.h
pmc.o: pmc.h
process.o: pmc.h process.h
requisite.o: pmc.h
tiptop.o: requisite.h pmc.h process.h conf.h
