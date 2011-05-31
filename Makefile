TARGET=TARGET_X86

CFLAGS=-O2 -Wall -DKERNEL31 -DHAS_CURSES -D$(TARGET)
CFLAGS+=$(XCFLAGS)

OBJS=tiptop.o pmc.o process.o requisite.o conf.o screen.o screens-intel.o \
     utils.o debug.o version.o

all: tiptop

tiptop: $(OBJS)
	$(CC) -O2 -o tiptop $(OBJS) -lcurses

version.o: version.c
	$(CC) $(CFLAGS) -DCOMPILE_HOST="\""`hostname`"\"" \
                        -DCOMPILE_DATE="\"`date`\"" \
                        -c version.c

clean:
	/bin/rm -f $(OBJS) tiptop


depend:
	makedepend -Y *.c

# DO NOT DELETE

conf.o: conf.h
pmc.o: pmc.h
process.o: pmc.h process.h screen.h
requisite.o: pmc.h requisite.h
screen.o: debug.h screen.h screens-intel.h utils.h
tiptop.o: conf.h requisite.h pmc.h process.h screen.h utils.h version.h
utils.o: debug.h utils.h
version.o: version.h
