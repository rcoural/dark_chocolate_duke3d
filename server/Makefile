# Dark Chocolate Duke3D relay server.
# No engine, no SDL, no DUKE3D.GRP - just POSIX sockets. Builds anywhere with cc.
CC      ?= cc
CFLAGS  ?= -O2 -Wall
PREFIX  ?= /usr/local

duke3d-relay: duke3d-relay.c
	$(CC) $(CFLAGS) -o $@ $<

install: duke3d-relay
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 0755 duke3d-relay $(DESTDIR)$(PREFIX)/bin/duke3d-relay

clean:
	rm -f duke3d-relay

.PHONY: install clean
