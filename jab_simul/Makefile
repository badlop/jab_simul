CC=gcc
JABD=../jabberd
CFLAGS=-g -I$(JABD)/ -Wall
APPNAME=jab_simul

LIBS=-lssl -lcrypto -lncurses
#LIBS=

jabberd_XODEOBJECTS = \
	$(JABD)/lib/expat.o \
	$(JABD)/lib/genhash.o \
	$(JABD)/lib/hashtable.o \
	$(JABD)/lib/jid.o \
	$(JABD)/lib/jpacket.o \
	$(JABD)/lib/jutil.o \
	$(JABD)/lib/karma.o \
	$(JABD)/lib/pool.o \
	$(JABD)/lib/pproxy.o \
	$(JABD)/lib/rate.o \
	$(JABD)/lib/sha.o \
	$(JABD)/lib/snprintf.o \
	$(JABD)/lib/socket.o \
	$(JABD)/lib/str.o \
	$(JABD)/lib/xmlnode.o \
	$(JABD)/lib/xmlparse.o \
	$(JABD)/lib/xmlrole.o \
	$(JABD)/lib/xmltok.o \
	$(JABD)/lib/xstream.o \
	$(JABD)/lib/xhash.o

local_OBJECTS= \
	$(APPNAME)-main.o \
	$(APPNAME)-ssl.o \
	thread-main.o \
	$(APPNAME)-conn.o \
	$(APPNAME)-admin.o \
	$(APPNAME)-user.o \
	$(APPNAME)-repeater.o \
	$(APPNAME)-roster.o \
	namespace_pars.o \
	namespace_scan.o \
	libjabber_ext.o \
	karma.o

all_OBJECTS= $(jabberd_XODEOBJECTS) $(local_OBJECTS)

all: $(APPNAME)

$(APPNAME): $(all_OBJECTS)
	$(CC) -o $(APPNAME) $(all_OBJECTS) $(CFLAGS) $(LDFLAGS) $(LIBS)

$(local_OBJECTS): $(APPNAME)-main.h config.h karma.h wp_defines.h $(APPNAME)-admin.h $(APPNAME)-roster.h

#$(APPNAME)-main.o: $(APPNAME)-main.cc changelog.inc
#	gcc -o $@ -c $(APPNAME)-main.cc $(CFLAGS)

$(APPNAME)-main.o: $(APPNAME)-main.c changelog.inc
	gcc -o $@ -c $(APPNAME)-main.c $(CFLAGS)

config.h: CHANGELOG
	./configure

changelog.inc: CHANGELOG
	./configure

clean:
	rm -f $(jabberd_XODEOBJECTS) $(local_OBJECTS) $(APPNAME) changelog.inc config.h namespace_scan.c namespace_pars.c y.tab.*

clean-stuff:
	rm -f *~

namespace_scan.c: namespace_scan.l y.tab.h namespace_type.h
	lex -t namespace_scan.l > namespace_scan.c
#	lex -d -t namespace.l > namespace.c

y.tab.h: namespace_pars.y
	yacc -t -d $<
