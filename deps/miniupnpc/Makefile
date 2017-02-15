# $Id: Makefile,v 1.134 2016/10/07 09:04:36 nanard Exp $
# MiniUPnP Project
# http://miniupnp.free.fr/
# http://miniupnp.tuxfamily.org/
# https://github.com/miniupnp/miniupnp
# (c) 2005-2016 Thomas Bernard
# to install use :
# $ make DESTDIR=/tmp/dummylocation install
# or
# $ INSTALLPREFIX=/usr/local make install
# or
# $ make install (default INSTALLPREFIX is /usr)
OS = $(shell uname -s)
VERSION = $(shell cat VERSION)

ifeq ($(OS), Darwin)
JARSUFFIX=mac
LIBTOOL ?= $(shell which libtool)
endif
ifeq ($(OS), Linux)
JARSUFFIX=linux
endif
ifneq (,$(findstring NT-5.1,$(OS)))
JARSUFFIX=win32
endif

HAVE_IPV6 ?= yes
export HAVE_IPV6

CC ?= gcc
#AR = gar
#CFLAGS = -O -g -DDEBUG
CFLAGS ?= -O
CFLAGS += -Wall
CFLAGS += -W -Wstrict-prototypes
CFLAGS += -fno-common
CFLAGS += -DMINIUPNPC_SET_SOCKET_TIMEOUT
CFLAGS += -DMINIUPNPC_GET_SRC_ADDR
CFLAGS += -D_BSD_SOURCE
CFLAGS += -D_DEFAULT_SOURCE
ifeq ($(OS), NetBSD)
CFLAGS += -D_NETBSD_SOURCE
endif
ifneq ($(OS), FreeBSD)
ifneq ($(OS), Darwin)
#CFLAGS += -D_POSIX_C_SOURCE=200112L
CFLAGS += -D_XOPEN_SOURCE=600
endif
endif
#CFLAGS += -ansi
# -DNO_GETADDRINFO
INSTALL = install
SH = /bin/sh

ifeq (SunOS, $(OS))
  LDLIBS=-lsocket -lnsl -lresolv
  CFLAGS += -D__EXTENSIONS__
  CFLAGS += -std=c99
endif

# APIVERSION is used to build SONAME
APIVERSION = 16

SRCS = igd_desc_parse.c miniupnpc.c minixml.c minisoap.c miniwget.c \
       upnpcommands.c upnpreplyparse.c \
       minixmlvalid.c minissdpc.c \
       upnperrors.c \
       connecthostport.c portlistingparse.c receivedata.c \
       upnpdev.c miniupnpcmodule.c

LIBOBJS = miniwget.o minixml.o igd_desc_parse.o minisoap.o \
          miniupnpc.o upnpreplyparse.o upnpcommands.o upnperrors.o \
          connecthostport.o portlistingparse.o receivedata.o upnpdev.o

ifneq ($(OS), AmigaOS)
ifeq (,$(findstring CYGWIN,$(OS)))
CFLAGS := -fPIC $(CFLAGS)
endif
LIBOBJS := $(LIBOBJS) minissdpc.o
endif

OBJS = $(patsubst %.c,%.o,$(SRCS))

# HEADERS to install
HEADERS = miniupnpc.h miniwget.h upnpcommands.h igd_desc_parse.h \
          upnpreplyparse.h upnperrors.h miniupnpctypes.h \
          portlistingparse.h \
          upnpdev.h \
          miniupnpc_declspec.h

# library names
LIBRARY = libminiupnpc.a
ifeq ($(OS), Darwin)
  SHAREDLIBRARY = libminiupnpc.dylib
  SONAME = $(basename $(SHAREDLIBRARY)).$(APIVERSION).dylib
  CFLAGS := -D_DARWIN_C_SOURCE $(CFLAGS)
else
ifeq ($(JARSUFFIX), win32)
  SHAREDLIBRARY = miniupnpc.dll
else
  # Linux/BSD/etc.
  SHAREDLIBRARY = libminiupnpc.so
  SONAME = $(SHAREDLIBRARY).$(APIVERSION)
endif
endif

EXECUTABLES = upnpc-static

ifneq ($(OS), AmigaOS)
EXECUTABLES := $(EXECUTABLES) upnpc-shared
endif

LIBDIR ?= lib
# install directories
ifeq ($(strip $(PREFIX)),)
INSTALLPREFIX ?= /usr
else
INSTALLPREFIX ?= $(PREFIX)
endif
INSTALLDIRINC = $(INSTALLPREFIX)/include/miniupnpc
INSTALLDIRLIB = $(INSTALLPREFIX)/$(LIBDIR)
INSTALLDIRBIN = $(INSTALLPREFIX)/bin
INSTALLDIRMAN = $(INSTALLPREFIX)/share/man

FILESTOINSTALL = $(LIBRARY) $(EXECUTABLES)
ifneq ($(OS), AmigaOS)
FILESTOINSTALL := $(FILESTOINSTALL) $(SHAREDLIBRARY)
endif


.PHONY:	install clean depend all check test everything \
	updateversion
#	validateminixml validateminiwget

all:	$(LIBRARY) $(EXECUTABLES)

test:	check

check:	validateminixml validateminiwget validateupnpreplyparse \
	validateportlistingparse validateigddescparse

everything:	all

validateminixml:	minixmlvalid
	@echo "minixml validation test"
	./minixmlvalid
	touch $@

validateminiwget:	testminiwget minihttptestserver testminiwget.sh
	@echo "miniwget validation test"
	./testminiwget.sh
	touch $@

validateupnpreplyparse:	testupnpreplyparse testupnpreplyparse.sh
	@echo "upnpreplyparse validation test"
	./testupnpreplyparse.sh
	touch $@

validateportlistingparse:	testportlistingparse
	@echo "portlistingparse validation test"
	./testportlistingparse
	touch $@

validateigddescparse:	testigddescparse
	@echo "igd desc parse validation test"
	./testigddescparse testdesc/new_LiveBox_desc.xml testdesc/new_LiveBox_desc.values
	./testigddescparse testdesc/linksys_WAG200G_desc.xml testdesc/linksys_WAG200G_desc.values
	touch $@

clean:
	$(RM) $(LIBRARY) $(SHAREDLIBRARY) $(EXECUTABLES) $(OBJS) miniupnpcstrings.h

distclean: clean

updateversion:	miniupnpc.h
	cp miniupnpc.h miniupnpc.h.bak
	sed 's/\(.*MINIUPNPC_API_VERSION\s\+\)[0-9]\+/\1$(APIVERSION)/' < miniupnpc.h.bak > miniupnpc.h

install:	updateversion $(FILESTOINSTALL)
	$(INSTALL) -d $(DESTDIR)$(INSTALLDIRINC)
	$(INSTALL) -m 644 $(HEADERS) $(DESTDIR)$(INSTALLDIRINC)
	$(INSTALL) -d $(DESTDIR)$(INSTALLDIRLIB)
	$(INSTALL) -m 644 $(LIBRARY) $(DESTDIR)$(INSTALLDIRLIB)
ifneq ($(OS), AmigaOS)
	$(INSTALL) -m 644 $(SHAREDLIBRARY) $(DESTDIR)$(INSTALLDIRLIB)/$(SONAME)
	ln -fs $(SONAME) $(DESTDIR)$(INSTALLDIRLIB)/$(SHAREDLIBRARY)
endif
	$(INSTALL) -d $(DESTDIR)$(INSTALLDIRBIN)
ifeq ($(OS), AmigaOS)
	$(INSTALL) -m 755 upnpc-static $(DESTDIR)$(INSTALLDIRBIN)/upnpc
else
	$(INSTALL) -m 755 upnpc-shared $(DESTDIR)$(INSTALLDIRBIN)/upnpc
endif
	$(INSTALL) -m 755 external-ip.sh $(DESTDIR)$(INSTALLDIRBIN)/external-ip
ifneq ($(OS), AmigaOS)
	$(INSTALL) -d $(DESTDIR)$(INSTALLDIRMAN)/man3
	$(INSTALL) -m 644 man3/miniupnpc.3 $(DESTDIR)$(INSTALLDIRMAN)/man3/miniupnpc.3
ifeq ($(OS), Linux)
	gzip -f $(DESTDIR)$(INSTALLDIRMAN)/man3/miniupnpc.3
endif
endif

install-static:	updateversion $(FILESTOINSTALL)
	$(INSTALL) -d $(DESTDIR)$(INSTALLDIRINC)
	$(INSTALL) -m 644 $(HEADERS) $(DESTDIR)$(INSTALLDIRINC)
	$(INSTALL) -d $(DESTDIR)$(INSTALLDIRLIB)
	$(INSTALL) -m 644 $(LIBRARY) $(DESTDIR)$(INSTALLDIRLIB)
	$(INSTALL) -d $(DESTDIR)$(INSTALLDIRBIN)
	$(INSTALL) -m 755 external-ip.sh $(DESTDIR)$(INSTALLDIRBIN)/external-ip

cleaninstall:
	$(RM) -r $(DESTDIR)$(INSTALLDIRINC)
	$(RM) $(DESTDIR)$(INSTALLDIRLIB)/$(LIBRARY)
	$(RM) $(DESTDIR)$(INSTALLDIRLIB)/$(SHAREDLIBRARY)

depend:
	makedepend -Y -- $(CFLAGS) -- $(SRCS) 2>/dev/null

$(LIBRARY):	$(LIBOBJS)
ifeq ($(OS), Darwin)
	$(LIBTOOL) -static -o $@ $?
else
	$(AR) crs $@ $?
endif

$(SHAREDLIBRARY):	$(LIBOBJS)
ifeq ($(OS), Darwin)
#	$(CC) -dynamiclib $(LDFLAGS) -Wl,-install_name,$(SONAME) -o $@ $^
	$(CC) -dynamiclib $(LDFLAGS) -Wl,-install_name,$(INSTALLDIRLIB)/$(SONAME) -o $@ $^
else
	$(CC) -shared $(LDFLAGS) -Wl,-soname,$(SONAME) -o $@ $^
endif

upnpc-static:	$(LIBRARY)
	$(CC) $(LDFLAGS) -o $@ $^ $(LOADLIBES) $(LDLIBS)

upnpc-shared:	$(SHAREDLIBRARY)
	$(CC) $(LDFLAGS) -o $@ $^ $(LOADLIBES) $(LDLIBS)

minixmlvalid:	minixml.o minixmlvalid.o

miniupnpcstrings.h:	miniupnpcstrings.h.in updateminiupnpcstrings.sh VERSION
	$(SH) updateminiupnpcstrings.sh

# DO NOT DELETE THIS LINE -- make depend depends on it.

igd_desc_parse.o: igd_desc_parse.h
miniupnpc.o: miniupnpc.h miniupnpc_declspec.h igd_desc_parse.h upnpdev.h
miniupnpc.o: minissdpc.h miniwget.h minisoap.h minixml.h upnpcommands.h
miniupnpc.o: upnpreplyparse.h portlistingparse.h miniupnpctypes.h
miniupnpc.o: connecthostport.h
minixml.o: minixml.h
minisoap.o: minisoap.h miniupnpcstrings.h
miniwget.o: miniupnpcstrings.h miniwget.h miniupnpc_declspec.h
miniwget.o: connecthostport.h receivedata.h
upnpcommands.o: upnpcommands.h upnpreplyparse.h portlistingparse.h
upnpcommands.o: miniupnpc_declspec.h miniupnpctypes.h miniupnpc.h
upnpcommands.o: igd_desc_parse.h upnpdev.h
upnpreplyparse.o: upnpreplyparse.h minixml.h
minixmlvalid.o: minixml.h
minissdpc.o: minissdpc.h miniupnpc_declspec.h upnpdev.h miniupnpc.h
minissdpc.o: igd_desc_parse.h receivedata.h codelength.h
upnperrors.o: upnperrors.h miniupnpc_declspec.h upnpcommands.h
upnperrors.o: upnpreplyparse.h portlistingparse.h miniupnpctypes.h
upnperrors.o: miniupnpc.h igd_desc_parse.h upnpdev.h
connecthostport.o: connecthostport.h
portlistingparse.o: portlistingparse.h miniupnpc_declspec.h miniupnpctypes.h
portlistingparse.o: minixml.h
receivedata.o: receivedata.h
upnpdev.o: upnpdev.h miniupnpc_declspec.h
testportlistingparse.o: miniupnpctypes.h
miniupnpcmodule.o: miniupnpc.h miniupnpc_declspec.h igd_desc_parse.h
miniupnpcmodule.o: upnpdev.h upnpcommands.h upnpreplyparse.h
miniupnpcmodule.o: portlistingparse.h miniupnpctypes.h upnperrors.h
