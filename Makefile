TARGET=TARGET_X86

CFLAGS=-O2 -Wall -DKERNEL31 -DHAS_CURSES -D$(TARGET)
LDFLAGS=-O2

CFLAGS+=$(XCFLAGS)
LDFLAGS+=$(XLDFLAGS)

OBJS=tiptop.o pmc.o process.o requisite.o conf.o screen.o screens-intel.o \
     debug.o version.o helpwin.o options.o hash.o

all: tiptop

debug:
	make clean
	make XCFLAGS="-g -O0" XLDFLAGS="-g"

tiptop: $(OBJS)
	$(CC) $(LDFLAGS) -o tiptop $(OBJS) -lcurses

version.o: version.c
	$(CC) $(CFLAGS) -DCOMPILE_HOST="\""`hostname`"\"" \
                        -DCOMPILE_DATE="\"`date`\"" \
                        -c version.c

clean:
	/bin/rm -f $(OBJS) tiptop


depend:
	makedepend -Y -DTARGET_X86 -DHAS_CURSES *.c

# DO NOT DELETE

conf.o: conf.h options.h
debug.o: debug.h options.h
hash.o: hash.h process.h screen.h options.h
helpwin.o: helpwin.h screen.h options.h
options.o: options.h version.h
pmc.o: pmc.h
process.o: hash.h process.h screen.h options.h pmc.h utils.h
requisite.o: pmc.h requisite.h
screen.o: options.h screen.h screens-intel.h
screens-intel.o: screen.h options.h screens-intel.h
tiptop.o: conf.h options.h helpwin.h screen.h pmc.h process.h requisite.h
utils.o: debug.h utils.h
version.o: version.h
