noinst_LTLIBRARIES = libnlm.la

nlm_SOURCES_GENERATED =
nlm_HEADERS_GENERATED =
nlm_GENERATED = $(nlm_SOURCES_GENERATED) $(nlm_HEADERS_GENERATED)

CLEANFILES = $(nlm_GENERATED) nlm-stamp

libnlm_la_CPPFLAGS = -I$(abs_top_srcdir)/include \
		     -I$(abs_top_srcdir)/include/nfsc \
		     -I$(abs_top_srcdir)/win32
libnlm_la_SOURCES = \
	$(nlm_SOURCES_GENERATED) \
	nlm.c libnfs-raw-nlm.c libnfs-raw-nlm.h

$(nlm_GENERATED) : nlm-stamp
nlm-stamp : nlm.x
	rm -f $(nlm_GENERATED)
	touch nlm-stamp

compile_rpc:	
	cat nlm.x | head -29 >libnfs-raw-nlm.h
	rpcgen -h nlm.x | sed -e "s/#include <rpc\/rpc.h>/#include <nfsc\/libnfs-zdr.h>/" | sed -e "s/xdr/zdr/g" -e "s/XDR/ZDR/g" -e "s/ CLIENT / void /g" -e "s/SVCXPRT /void /g" -e "s/bool_t/uint32_t/g" >> libnfs-raw-nlm.h
	cat nlm.x | head -29 >libnfs-raw-nlm.c
	rpcgen -c nlm.x | sed -e "s/#include \".*nlm.h\"/#include \"libnfs-xdr.h\"\n#include \"libnfs-raw-nlm.h\"/" -e "s/xdr/zdr/g" -e "s/XDR/ZDR/g" -e "s/register int32_t \*buf;/register int32_t *buf;\n	buf = NULL;/" -e "s/bool_t/uint32_t/g" >> libnfs-raw-nlm.c
