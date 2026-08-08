AM_CFLAGS=$(WARN_CFLAGS)

lib_LTLIBRARIES = libnfs.la

libnfs_la_CPPFLAGS = -I$(abs_top_srcdir)/include \
		     -I$(abs_top_srcdir)/include/nfsc \
		     -I$(abs_top_srcdir)/mount \
		     -I$(abs_top_srcdir)/nfs \
		     -I$(abs_top_srcdir)/nfs4 \
		     -I$(abs_top_srcdir)/nlm \
		     -I$(abs_top_srcdir)/nsm \
		     -I$(abs_top_srcdir)/portmap \
		     -I$(abs_top_srcdir)/rquota \
		     -I$(abs_top_srcdir)/win32 \
		     "-D_U_=__attribute__((unused))"

libnfs_la_SOURCES = \
	init.c \
	krb5-wrapper.c \
	libnfs.c \
	libnfs-sync.c \
	libnfs-zdr.c \
	multithreading.c \
	nfs_v3.c \
	nfs_v4.c \
	pdu.c \
	socket.c \
	../win32/win32_compat.c

SOCURRENT=16
SOREVISION=2
SOAGE=0
libnfs_la_LDFLAGS = -version-info $(SOCURRENT):$(SOREVISION):$(SOAGE) \
	$(MAYBE_LIBKRB5)
if HAVE_WIN32
libnfs_la_LDFLAGS += -no-undefined
endif

libnfs_la_LIBADD = \
	../mount/libmount.la \
	../nfs/libnfs.la \
	../nfs4/libnfs4.la \
	../nlm/libnlm.la \
	../nsm/libnsm.la \
	../portmap/libportmap.la \
	../rquota/librquota.la \
	$(LIBSOCKET) \
	$(LIBPTHREAD)

if HAVE_TLS
libnfs_la_CPPFLAGS += -I$(abs_top_srcdir)/tls
libnfs_la_LIBADD += ../tls/libtls.la -lgnutls
endif
