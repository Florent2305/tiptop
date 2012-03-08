package = tiptop
version = 1.0
tarname = $(package)
distdir = $(tarname)-$(version)


all clean tiptop:
	cd src && $(MAKE) $@


dist: $(distdir).tar.gz

$(distdir).tar.gz: $(distdir)
	tar chof - $(distdir) | gzip -9 -c > $@
	rm -rf $(distdir)

distcheck: $(distdir).tar.gz
	gzip -cd $(distdir).tar.gz | tar xvf -
	cd $(distdir) && $(MAKE) all
	cd $(distdir) && $(MAKE) clean
	rm -rf $(distdir)
	@echo "*** Package $(distdir).tar.gz is ready for distribution."


$(distdir): FORCE
	mkdir -p $(distdir)/src
	cp tiptop.1 $(distdir)
	cp README $(distdir)
	cp Makefile $(distdir)
	cp src/Makefile $(distdir)/src
	cp src/conf.c $(distdir)/src
	cp src/conf.h $(distdir)/src
	cp src/debug.c $(distdir)/src
	cp src/debug.h $(distdir)/src
	cp src/hash.c $(distdir)/src
	cp src/hash.h $(distdir)/src
	cp src/helpwin.c $(distdir)/src
	cp src/helpwin.h $(distdir)/src
	cp src/options.c $(distdir)/src
	cp src/options.h $(distdir)/src
	cp src/pmc.c $(distdir)/src
	cp src/pmc.h $(distdir)/src
	cp src/process.c $(distdir)/src
	cp src/process.h $(distdir)/src
	cp src/requisite.c $(distdir)/src
	cp src/requisite.h $(distdir)/src
	cp src/screen.c $(distdir)/src
	cp src/screen.h $(distdir)/src
	cp src/screens-intel.c $(distdir)/src
	cp src/screens-intel.h $(distdir)/src
	cp src/spawn.c $(distdir)/src
	cp src/spawn.h $(distdir)/src
	cp src/tiptop.c $(distdir)/src
	cp src/utils.c $(distdir)/src
	cp src/utils.h $(distdir)/src
	cp src/version.c $(distdir)/src
	cp src/version.h $(distdir)/src

FORCE:
	-rm $(distdir).tar.gz > /dev/null 2>&1
	-rm -rf $(distdir) > /dev/null 2>&1

.PHONY: FORCE all clean dist
