TARGET=TARGET_X86

CFLAGS=-O2 -Wall -DKERNEL31 -DHAS_CURSES -D$(TARGET)
CFLAGS+=$(XCFLAGS)

OBJS=tiptop.o pmc.o process.o requisite.o conf.o screen.o screens-intel.o \
     utils.o

all: tiptop

tiptop: $(OBJS)
	$(CC) -O2 -o tiptop $(OBJS) -lcurses


clean:
	/bin/rm -f $(OBJS) tiptop


depend:
	makedepend -Y *.c

# DO NOT DELETE

conf.o: conf.h
pmc.o: pmc.h
process.o: pmc.h process.h screen.h
requisite.o: pmc.h requisite.h
screen.o: screen.h screens-intel.h utils.h
tiptop.o: conf.h requisite.h pmc.h process.h screen.h utils.h
utils.o: utils.h
