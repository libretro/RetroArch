noinst_LTLIBRARIES = librquota.la

rquota_SOURCES_GENERATED =
rquota_HEADERS_GENERATED =
rquota_GENERATED = $(rquota_SOURCES_GENERATED) $(rquota_HEADERS_GENERATED)

CLEANFILES = $(rquota_GENERATED) rquota-stamp

librquota_la_CPPFLAGS = -I$(abs_top_srcdir)/include \
			-I$(abs_top_srcdir)/include/nfsc \
			-I$(abs_top_srcdir)/win32
librquota_la_SOURCES = \
	$(rquota_SOURCES_GENERATED) \
	rquota.c libnfs-raw-rquota.c libnfs-raw-rquota.h

$(rquota_GENERATED) : rquota-stamp
rquota-stamp : rquota.x
	rm -f $(rquota_GENERATED)
	touch rquota-stamp

compile_rpc:
	cat rquota.x | head -29 >libnfs-raw-rquota.h
	rpcgen -h rquota.x | sed -e "s/#include <rpc\/rpc.h>/#include <nfsc\/libnfs-zdr.h>/" | sed -e "s/xdr/zdr/g" -e "s/XDR/ZDR/g" -e "s/ CLIENT / void /g" -e "s/SVCXPRT /void /g" -e "s/bool_t/uint32_t/g" >> libnfs-raw-rquota.h
	cat rquota.x | head -29 >libnfs-raw-rquota.c
	rpcgen -c rquota.x | sed -e "s/#include \".*rquota.h\"/#include \"libnfs-xdr.h\"\n#include \"libnfs-raw-rquota.h\"/" -e "s/xdr/zdr/g" -e "s/XDR/ZDR/g" -e "s/register int32_t \*buf;/register int32_t *buf;\n	buf = NULL;/" -e "s/bool_t/uint32_t/g" >> libnfs-raw-rquota.c
