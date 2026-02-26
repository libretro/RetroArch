if ENABLE_EXAMPLES
    MAYBE_EXAMPLES = examples
endif

SUBDIRS = include lib utils . $(MAYBE_EXAMPLES)
ACLOCAL_AMFLAGS = -I m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libsmb2.pc

EXTRA_DIST = \
	libsmb2.pc.in

test: $(SUBDIRS)
	cd tests; make test
