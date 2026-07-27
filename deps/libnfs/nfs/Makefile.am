noinst_LTLIBRARIES = libnfs.la

nfs_SOURCES_GENERATED =
nfs_HEADERS_GENERATED =
nfs_GENERATED = $(nfs_SOURCES_GENERATED) $(nfs_HEADERS_GENERATED)

CLEANFILES = $(nfs_GENERATED) nfs-stamp

libnfs_la_CPPFLAGS = -I$(abs_top_srcdir)/include \
		     -I$(abs_top_srcdir)/include/nfsc \
		     -I$(abs_top_srcdir)/win32
libnfs_la_SOURCES = \
	$(nfs_SOURCES_GENERATED) \
	nfs.c nfsacl.c libnfs-raw-nfs.c libnfs-raw-nfs.h

$(nfs_GENERATED) : nfs-stamp
nfs-stamp : nfs.x
	rm -f $(nfs_GENERATED)
	touch nfs-stamp

compile_rpc:	
	cat nfs.x | head -29 >libnfs-raw-nfs.h
	rpcgen -h nfs.x | sed -e "s/#include <rpc\/rpc.h>/#include <nfsc\/libnfs-zdr.h>/" -e "s/xdr/zdr/g" -e "s/XDR/ZDR/g" -e "s/#define _NFS_H_RPCGEN/#define _NFS_H_RPCGEN\n#include <nfsc\/libnfs-zdr.h>/g" -e "s/#define NFS3_COOKIEVERFSIZE 8/#define NFS3_COOKIEVERFSIZE 8\n\n/g" -e "s/ CLIENT / void /g" -e "s/SVCXPRT /void /g" -e "s/bool_t/uint32_t/g" | head -n -5 >> libnfs-raw-nfs.h
	cat libnfs-raw-nfs.h.extra >>libnfs-raw-nfs.h
	cat nfs.x | head -29 >libnfs-raw-nfs.c
	rpcgen -c nfs.x | sed -e "s/#include \".*nfs.h\"/#include \"libnfs-xdr.h\"\n#include \"libnfs-raw-nfs.h\"/" -e "s/xdr/zdr/g" -e "s/XDR/ZDR/g" -e "s/register int32_t \*buf;/register int32_t *buf;\n	buf = NULL;/" -e "s/bool_t/uint32_t/g" >> libnfs-raw-nfs.c
