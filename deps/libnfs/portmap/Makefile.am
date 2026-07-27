noinst_LTLIBRARIES = libportmap.la

portmap_SOURCES_GENERATED =
portmap_HEADERS_GENERATED =
portmap_GENERATED = $(portmap_SOURCES_GENERATED) $(portmap_HEADERS_GENERATED)

CLEANFILES = $(portmap_GENERATED) portmap-stamp

libportmap_la_CPPFLAGS = -I$(abs_top_srcdir)/include \
			 -I$(abs_top_srcdir)/include/nfsc \
			 -I$(abs_top_srcdir)/win32
libportmap_la_SOURCES = \
	$(portmap_SOURCES_GENERATED) \
	portmap.c libnfs-raw-portmap.c libnfs-raw-portmap.h

$(portmap_GENERATED) : portmap-stamp
portmap-stamp : portmap.x
	rm -f $(portmap_GENERATED)
	touch portmap-stamp

compile_rpc:	
	cat portmap.x | head -29 >libnfs-raw-portmap.h
	rpcgen -h portmap.x | sed -e "s/#include <rpc\/rpc.h>/#include <nfsc\/libnfs-zdr.h>/" | sed -e "s/xdr/zdr/g" -e "s/XDR/ZDR/g" -e "s/ CLIENT / void /g" -e "s/SVCXPRT /void /g" -e "s/bool_t/uint32_t/g" -e "s/u_int/uint32_t/" >> libnfs-raw-portmap.h
	cat portmap.x | head -29 >libnfs-raw-portmap.c
	rpcgen -c portmap.x | sed -e "s/#include \".*portmap.h\"/#include \"libnfs-zdr.h\"\n#include \"libnfs-raw-portmap.h\"/" -e "s/xdr/zdr/g" -e "s/XDR/ZDR/g" -e "s/register int32_t \*buf;//" -e "s/bool_t/uint32_t/g" -e "s/[ \t]*buf = /		int32_t *buf;\n		buf = /" -e "s/\(u_int \*\)/uint32_t \*/" >> libnfs-raw-portmap.c
