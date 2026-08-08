SUBDIRS = doc mount nfs nfs4 nlm nsm portmap rquota 
ACLOCAL_AMFLAGS = -I m4

if HAVE_TLS
SUBDIRS += tls
endif
SUBDIRS +=  lib include $(MAYBE_UTILS) . $(MAYBE_EXAMPLES) $(MAYBE_SERVERS)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libnfs.pc

EXTRA_DIST = \
	README \
	COPYING \
	LICENCE-BSD.txt \
	LICENCE-GPL-3.txt \
	LICENCE-LGPL-2.1.txt \
	examples \
	libnfs.pc.in

test: $(SUBDIRS)
	cd tests; make test
