noinst_PROGRAMS = smb2-cp smb2-ls

AM_CPPFLAGS = \
	-I$(abs_top_srcdir)/include \
	-I$(abs_top_srcdir)/include/smb2 \
	"-D_U_=__attribute__((unused))" \
	-Wall -Werror

COMMON_LIBS = ../lib/libsmb2.la
smb2_ls_LDADD = $(COMMON_LIBS)
smb2_cp_LDADD = $(COMMON_LIBS)
