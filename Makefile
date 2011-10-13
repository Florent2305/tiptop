# Define one of the TARGET_xxx or NOTARGET
# TARGET=-DNOTARGET
TARGET=-DTARGET_X86

CFLAGS=-O2 -Wall -DKERNEL31 $(TARGET) -DHAS_CURSES 
LDFLAGS=-O2 -lcurses

CFLAGS+=$(XCFLAGS)
LDFLAGS+=$(XLDFLAGS)

OBJS=tiptop.o pmc.o process.o requisite.o conf.o screen.o \
     debug.o version.o helpwin.o options.o hash.o spawn.o \
     screens-intel.o


all: tiptop

debug:
	make clean
	make XCFLAGS+="-g -O0 -DDEBUG" XLDFLAGS="-g"

release:
	make clean
	make XCFLAGS+="-DNDEBUG"
	strip tiptop

dist: release
	tar zcvf tiptop.tar.gz tiptop tiptop.1 README

tiptop: $(OBJS)
	$(CC) -o tiptop $(OBJS) $(LDFLAGS)

version.o: version.c
	$(CC) $(CFLAGS) -DCOMPILE_HOST="\""`hostname`"\"" \
                        -DCOMPILE_DATE="\"`date`\"" \
                        -c version.c

clean:
	/bin/rm -f $(OBJS) tiptop tiptop.tar.gz


depend:
	makedepend -Y -DDEBUG $(TARGET) -DHAS_CURSES *.c

# DO NOT DELETE

conf.o: conf.h options.h
debug.o: debug.h options.h
hash.o: hash.h process.h screen.h options.h
helpwin.o: helpwin.h screen.h options.h
options.o: options.h version.h
pmc.o: pmc.h
process.o: hash.h process.h screen.h options.h pmc.h spawn.h utils.h
requisite.o: pmc.h requisite.h
screen.o: options.h screen.h
screens-intel.o: screen.h options.h screens-intel.h
spawn.o: options.h process.h screen.h
tiptop.o: conf.h options.h helpwin.h screen.h pmc.h process.h requisite.h
tiptop.o: spawn.h
utils.o: debug.h utils.h
version.o: version.h
