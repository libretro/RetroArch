AC_INIT([libsmb2],[6.1.0],[ronniesahlberg@gmail.com])

AC_PREREQ([2.69])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([-Wall foreign subdir-objects 1.11])
AC_CANONICAL_HOST
AC_CONFIG_MACRO_DIR([m4])

m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

dnl  Do not add default CFLAGS in AC_PROG_CC
: ${CFLAGS=""}
AC_PROG_CC
LT_INIT

AM_PROG_CC_C_O

dnl  We always want 64 bit file offsets
CFLAGS="${CFLAGS} -D_FILE_OFFSET_BITS=64"

AC_ARG_ENABLE([examples],
              [AS_HELP_STRING([--enable-examples],
                              [Build example programs])])

AM_CONDITIONAL([ENABLE_EXAMPLES],
               [test "$enable_examples" = "yes"])

AC_ARG_WITH([libkrb5],
            [AS_HELP_STRING([--without-libkrb5],
                            [Do not link with libkrb5 and use builtin
                             NTLMSSP module for authentication instead.])])

AS_IF([test "x$with_libkrb5" != "xno"], [
    MAYBE_LIBKRB5="-lgssapi_krb5"
    AC_DEFINE([HAVE_LIBKRB5], [1], [Whether we use gssapi_krb5 or not])
    AC_MSG_NOTICE([Build with gssapi_krb5 support])
    dnl  Check for gssapi/gssapi.h
    AC_CHECK_HEADERS([gssapi/gssapi.h], [], [
        AC_MSG_ERROR([You need gssapi development files to compile libsmb2.])
    ])
], [
    MAYBE_LIBKRB5=""
    AC_MSG_NOTICE([Build WITHOUT gssapi_krb5 support])
])

AC_ARG_WITH([lingering_TCP_sockets],
            [AS_HELP_STRING([--without-lingering-TCP-sockets],
                            [Do not allow TCP sockets to linger after closure.])])

AS_IF([test "x$with_lingering_TCP_sockets" != "xno"], [
    AC_DEFINE([CONFIGURE_OPTION_TCP_LINGER], [1], [Whether or not TCP sockets should be allowed to linger after closure])
    AC_MSG_NOTICE([Build with lingering TCP sockets])
], [
    AC_DEFINE([CONFIGURE_OPTION_TCP_LINGER], [0], [Whether or not TCP sockets should be allowed to linger after closure])
    AC_MSG_NOTICE([Build without lingering TCP sockets])
])

AC_SUBST([MAYBE_LIBKRB5])

AC_ARG_ENABLE([werror],
              [AS_HELP_STRING([--disable-werror],
              [Disables building with -Werror by default])])

AS_IF([test "$GCC" = "yes"], [
    WARN_CFLAGS="-Wall -Wshadow -Wno-write-strings -Wstrict-prototypes -Wpointer-arith -Wcast-align -Wno-strict-aliasing"
    AS_IF([test "$enable_werror" != "no"], [
        WARN_CFLAGS="${WARN_CFLAGS} -Werror"
    ])
])
AC_SUBST([WARN_CFLAGS])

LIBSOCKET=
SYS=

case $host in
  *solaris*)
    AC_CHECK_LIB([socket], [main], , [AC_MSG_ERROR([Can not find required library])])
    AC_CHECK_LIB([nsl],    [main], , [AC_MSG_ERROR([Can not find required library])])
    ;;
  *mingw32* | *cygwin* | *wince* | *mingwce*)
    LIBSOCKET='-lws2_32'
    SYS=mingw32
    ;;
  *)
    ;;
esac
AM_CONDITIONAL([HAVE_WIN32], [test "${SYS}" = "mingw32"])
AC_SUBST([LIBSOCKET])

AC_CHECK_LIB([dl], [dlsym], [], [AC_MSG_ERROR([dlsym not found, libdl is required])])

dnl  Check for poll.h
AC_CHECK_HEADERS([poll.h])

dnl  Check for sys/poll.h
AC_CHECK_HEADERS([sys/poll.h])

dnl  Check for unistd.h
AC_CHECK_HEADERS([unistd.h])

dnl  Check for sys/unistd.h
AC_CHECK_HEADERS([sys/unistd.h])

dnl  Check for netdb.h
AC_CHECK_HEADERS([netdb.h])

dnl  Check for sys/ioctl.h
AC_CHECK_HEADERS([sys/ioctl.h])

dnl  Check for sys/socket.h
AC_CHECK_HEADERS([sys/socket.h])

dnl  Check for sys/uio.h
AC_CHECK_HEADERS([sys/uio.h])

dnl  Check for sys/_iovec.h
AC_CHECK_HEADERS([sys/_iovec.h])

dnl  Check for netinet/tcp.h
AC_CHECK_HEADERS([netinet/tcp.h])

dnl  Check for netinet/in.h
AC_CHECK_HEADERS([netinet/in.h])

dnl  Check for arpa/inet.h
AC_CHECK_HEADERS([arpa/inet.h])

dnl  Check for time.h
AC_CHECK_HEADERS([time.h])

dnl  Check for sys/time.h
AC_CHECK_HEADERS([sys/time.h])

dnl  Check for strings.h
AC_CHECK_HEADERS([strings.h])

dnl  Check for fcntl.h
AC_CHECK_HEADERS([fcntl.h])

dnl  Check for sys/fcntl.h
AC_CHECK_HEADERS([sys/fcntl.h])

dnl  Check for errno.h
AC_CHECK_HEADERS([errno.h])

dnl  Check for sys/errno.h
AC_CHECK_HEADERS([sys/errno.h])

dnl  Check if sockaddr data struct includes a "sa_len"
AC_CHECK_MEMBER([struct sockaddr.sa_len], [
    AC_DEFINE([HAVE_SOCKADDR_LEN], [1], [Whether sockaddr struct has sa_len])
], [], [
#include <sys/types.h>
#include <sys/socket.h>
])

dnl  Check if sockaddr_storage struct includes a "ss_family"
AC_CHECK_MEMBER([struct sockaddr_storage.ss_family], [
    AC_DEFINE([HAVE_SOCKADDR_STORAGE], [1], [Whether we have sockaddr_Storage])
], [], [
#include <sys/types.h>
#include <sys/socket.h>
])

dnl  Check if sockaddr_storage struct includes a "l_linger"
AC_CHECK_MEMBER([struct linger.l_linger], [
    AC_DEFINE([HAVE_LINGER], [1], [Whether we have linger])
], [], [
#include <sys/types.h>
#include <sys/socket.h>
])

dnl  Output
AC_CONFIG_FILES([
    Makefile
    examples/Makefile
    include/Makefile
    lib/Makefile
    tests/Makefile
    utils/Makefile
])

AC_CONFIG_FILES([libsmb2.pc])
AC_OUTPUT
