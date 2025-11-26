AM_CFLAGS=$(WARN_CFLAGS)

lib_LTLIBRARIES = libsmb2.la

libsmb2_la_CPPFLAGS = -I$(abs_top_srcdir)/include \
		      -I$(abs_top_srcdir)/include/smb2 \
		      "-D_U_=__attribute__((unused))"

libsmb2_la_SOURCES = \
	aes.h \
	aes_reference.c \
	aes.c \
	aes128ccm.h \
	aes_apple.c \
	aes128ccm.c \
	alloc.c \
	asn1-ber.c \
	compat.c \
	compat.h \
	dcerpc.c \
	dcerpc-lsa.c \
	dcerpc-srvsvc.c \
	errors.c \
	init.c \
	hmac.c \
	hmac-md5.h \
	hmac-md5.c \
	krb5-wrapper.h \
	krb5-wrapper.c \
	libsmb2.c \
	md4.h \
	md4c.c \
	md5.h \
	md5.c \
	ntlmssp.h \
	ntlmssp.c \
	pdu.c \
	sha.h \
	sha-private.h \
	sha1.c \
	sha224-256.c \
	sha384-512.c \
	smb2-cmd-close.c \
	smb2-cmd-create.c \
	smb2-cmd-echo.c \
	smb2-cmd-error.c \
	smb2-cmd-flush.c \
	smb2-cmd-ioctl.c \
	smb2-cmd-lock.c \
	smb2-cmd-logoff.c \
	smb2-cmd-negotiate.c \
	smb2-cmd-notify-change.c \
    smb2-cmd-oplock-break.c \
	smb2-cmd-query-directory.c \
	smb2-cmd-query-info.c \
	smb2-cmd-read.c \
	smb2-cmd-session-setup.c \
	smb2-cmd-set-info.c \
	smb2-cmd-tree-connect.c \
	smb2-cmd-tree-disconnect.c \
	smb2-cmd-write.c \
	smb2-data-file-info.c \
	smb2-data-filesystem-info.c \
	smb2-data-reparse-point.c \
	smb2-data-security-descriptor.c \
	smb2-share-enum.c \
	smb3-seal.h \
	smb3-seal.c \
	smb2-signing.h \
	smb2-signing.c \
	socket.c \
	spnego-wrapper.c \
	sync.c \
	timestamps.c \
	unicode.c \
	usha.c

SOCURRENT=6
SOREVISION=1
SOAGE=0
libsmb2_la_LDFLAGS = \
	-version-info $(SOCURRENT):$(SOREVISION):$(SOAGE) -bindir $(bindir) \
	-no-undefined -export-symbols ${srcdir}/libsmb2.syms $(MAYBE_LIBKRB5)

dist_noinst_DATA = libsmb2.syms
