DESTDIR :=
PREFIX := /usr/local

CFLAGS = -Wall -g -std=gnu99

.PHONY:	all clean install uninstall

all: neocon

clean:
	$(RM) neocon

install: neocon
	install -D -m 555 neocon $(DESTDIR)$(PREFIX)/bin/neocon

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/neocon
