include config.mk

OBJ = init.o
BIN = init

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f $(BIN) $(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)$(MANPREFIX)/man8
	sed "s/VERSION/$(VERSION)/g" < $(BIN).8 > $(DESTDIR)$(MANPREFIX)/man8/$(BIN).8

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(BIN)
	rm -f $(DESTDIR)$(MANPREFIX)/man8/$(BIN).8

dist: clean
	mkdir -p init-$(VERSION)
	cp LICENSE Makefile README config.mk init.8 init.c init-$(VERSION)
	tar -cf init-$(VERSION).tar init-$(VERSION)
	gzip init-$(VERSION).tar
	rm -rf init-$(VERSION)

clean:
	rm -f $(BIN) $(OBJ) init-$(VERSION).tar.gz

.PHONY:
	all install uninstall dist clean
