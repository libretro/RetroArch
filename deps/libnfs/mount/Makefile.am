noinst_LTLIBRARIES = libmount.la

mount_SOURCES_GENERATED =
mount_HEADERS_GENERATED =
mount_GENERATED = $(mount_SOURCES_GENERATED) $(mount_HEADERS_GENERATED)

CLEANFILES = $(mount_GENERATED) mount-stamp

libmount_la_CPPFLAGS = -I$(abs_top_srcdir)/include \
		       -I$(abs_top_srcdir)/include/nfsc \
		       -I$(abs_top_srcdir)/win32
libmount_la_SOURCES = \
	$(mount_SOURCES_GENERATED) \
	mount.c libnfs-raw-mount.c libnfs-raw-mount.h

$(mount_GENERATED) : mount-stamp
mount-stamp : mount.x
	rm -f $(mount_GENERATED)
	touch mount-stamp

compile_rpc:
	cat mount.x | head -29 >libnfs-raw-mount.h
	rpcgen -h mount.x | sed -e "s/#include <rpc\/rpc.h>/#include <nfsc\/libnfs-zdr.h>/" | sed -e "s/xdr/zdr/g" -e "s/XDR/ZDR/g" -e "s/ CLIENT / void /g" -e "s/SVCXPRT /void /g" -e "s/bool_t/uint32_t/g" -e "s/u_int /uint32_t /g" >> libnfs-raw-mount.h
	cat mount.x | head -29 >libnfs-raw-mount.c
	rpcgen -c mount.x | sed -e "s/#include \".*mount.h\"/#include \"libnfs-xdr.h\"\n#include \"libnfs-raw-mount.h\"/" -e "s/xdr/zdr/g" -e "s/XDR/ZDR/g" -e "s/register int32_t \*buf;/register int32_t *buf;\n	buf = NULL;/" -e "s/bool_t/uint32_t/g" -e "s/u_int /uint32_t /g" >> libnfs-raw-mount.c
