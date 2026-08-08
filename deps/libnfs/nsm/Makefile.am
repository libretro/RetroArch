noinst_LTLIBRARIES = libnsm.la

nsm_SOURCES_GENERATED =
nsm_HEADERS_GENERATED =
nsm_GENERATED = $(nsm_SOURCES_GENERATED) $(nsm_HEADERS_GENERATED)

CLEANFILES = $(nsm_GENERATED) nsm-stamp

libnsm_la_CPPFLAGS = -I$(abs_top_srcdir)/include \
		     -I$(abs_top_srcdir)/include/nfsc \
		     -I$(abs_top_srcdir)/win32
libnsm_la_SOURCES = \
	$(nsm_SOURCES_GENERATED) \
	nsm.c libnfs-raw-nsm.c libnfs-raw-nsm.h

$(nsm_GENERATED) : nsm-stamp
nsm-stamp : nsm.x
	rm -f $(nsm_GENERATED)
	touch nsm-stamp

compile_rpc:	
	cat nsm.x | head -29 >libnfs-raw-nsm.h
	rpcgen -h nsm.x | sed -e "s/#include <rpc\/rpc.h>/#include <nfsc\/libnfs-zdr.h>/" | sed -e "s/xdr/zdr/g" -e "s/XDR/ZDR/g" -e "s/ CLIENT / void /g" -e "s/SVCXPRT /void /g" -e "s/bool_t/uint32_t/g" >> libnfs-raw-nsm.h
	cat nsm.x | head -29 >libnfs-raw-nsm.c
	rpcgen -c nsm.x | sed -e "s/#include \".*nsm.h\"/#include \"libnfs-xdr.h\"\n#include \"libnfs-raw-nsm.h\"/" -e "s/xdr/zdr/g" -e "s/XDR/ZDR/g" -e "s/register int32_t \*buf;/register int32_t *buf;\n	buf = NULL;/" -e "s/bool_t/uint32_t/g" >> libnfs-raw-nsm.c
