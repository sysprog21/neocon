DESTDIR:=
PREFIX:=/usr/local

CFLAGS=-Wall -g

.PHONY:	all clean install uninstall

all:		neocon

clean:
		rm -f neocon

install:	neocon
		install -D -m 555 neocon $(PREFIX)/bin

uninstall:
		rm -f $(PREFIX)/bin/neocon
