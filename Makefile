DESTDIR:=
PREFIX:=/usr/local

CFLAGS=-Wall -g

.PHONY:	all clean install uninstall

all:		neocon

clean:
		rm -f neocon

install:	neocon
		install -D -m 555 neocon $(DESTDIR)$(PREFIX)/bin/neocon

uninstall:
		rm -f $(DESTDIR)$(PREFIX)/bin/neocon
