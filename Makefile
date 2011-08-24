TARGET=TARGET_X86

CFLAGS=-Os -Wall -DKERNEL31 -DHAS_CURSES -D$(TARGET)
LDFLAGS=-Os

CFLAGS+=$(XCFLAGS)
LDFLAGS+=$(XLDFLAGS)

OBJS=tiptop.o pmc.o process.o requisite.o conf.o screen.o screens-intel.o \
     utils.o debug.o version.o helpwin.o options.o

all: tiptop

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

conf.o: conf.h
debug.o: options.h
helpwin.o: screen.h
options.o: options.h version.h
pmc.o: pmc.h
process.o: pmc.h process.h screen.h utils.h
requisite.o: pmc.h requisite.h
screen.o: debug.h screen.h screens-intel.h utils.h
screens-intel.o: screen.h screens-intel.h
tiptop.o: conf.h helpwin.h screen.h options.h pmc.h process.h requisite.h
tiptop.o: utils.h
utils.o: debug.h utils.h
version.o: version.h
