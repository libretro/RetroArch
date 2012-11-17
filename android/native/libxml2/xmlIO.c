/*
 * xmlIO.c : implementation of the I/O interfaces used by the parser
 *
 * See Copyright for the status of this software.
 *
 * daniel@veillard.com
 *
 * 14 Nov 2000 ht - for VMS, truncated name of long functions to under 32 char
 */

#define IN_LIBXML
#include "libxml.h"

#include <string.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif


#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_ZLIB_H
#include <zlib.h>
#endif
#ifdef HAVE_LZMA_H
#include <lzma.h>
#endif

#if defined(WIN32) || defined(_WIN32)
#include <windows.h>
#endif

#if defined(_WIN32_WCE)
#include <winnls.h> /* for CP_UTF8 */
#endif

/* Figure a portable way to know if a file is a directory. */
#ifndef HAVE_STAT
#  ifdef HAVE__STAT
     /* MS C library seems to define stat and _stat. The definition
        is identical. Still, mapping them to each other causes a warning. */
#    ifndef _MSC_VER
#      define stat(x,y) _stat(x,y)
#    endif
#    define HAVE_STAT
#  endif
#else
#  ifdef HAVE__STAT
#    if defined(_WIN32) || defined (__DJGPP__) && !defined (__CYGWIN__)
#      define stat _stat
#    endif
#  endif
#endif
#ifdef HAVE_STAT
#  ifndef S_ISDIR
#    ifdef _S_ISDIR
#      define S_ISDIR(x) _S_ISDIR(x)
#    else
#      ifdef S_IFDIR
#        ifndef S_IFMT
#          ifdef _S_IFMT
#            define S_IFMT _S_IFMT
#          endif
#        endif
#        ifdef S_IFMT
#          define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#        endif
#      endif
#    endif
#  endif
#endif

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/xmlIO.h>
#include <libxml/uri.h>
#include <libxml/nanohttp.h>
#include <libxml/xmlerror.h>
#include <libxml/globals.h>

#define MINLEN 4000

/*
 * Input I/O callback sets
 */
typedef struct _xmlInputCallback {
    xmlInputMatchCallback matchcallback;
    xmlInputOpenCallback opencallback;
    xmlInputReadCallback readcallback;
    xmlInputCloseCallback closecallback;
} xmlInputCallback;

#define MAX_INPUT_CALLBACK 15

static xmlInputCallback xmlInputCallbackTable[MAX_INPUT_CALLBACK];
static int xmlInputCallbackNr = 0;
static int xmlInputCallbackInitialized = 0;

/************************************************************************
 *									*
 *		Tree memory error handler				*
 *									*
 ************************************************************************/

static const char *IOerr[] = {
    "Unknown IO error",         /* UNKNOWN */
    "Permission denied",	/* EACCES */
    "Resource temporarily unavailable",/* EAGAIN */
    "Bad file descriptor",	/* EBADF */
    "Bad message",		/* EBADMSG */
    "Resource busy",		/* EBUSY */
    "Operation canceled",	/* ECANCELED */
    "No child processes",	/* ECHILD */
    "Resource deadlock avoided",/* EDEADLK */
    "Domain error",		/* EDOM */
    "File exists",		/* EEXIST */
    "Bad address",		/* EFAULT */
    "File too large",		/* EFBIG */
    "Operation in progress",	/* EINPROGRESS */
    "Interrupted function call",/* EINTR */
    "Invalid argument",		/* EINVAL */
    "Input/output error",	/* EIO */
    "Is a directory",		/* EISDIR */
    "Too many open files",	/* EMFILE */
    "Too many links",		/* EMLINK */
    "Inappropriate message buffer length",/* EMSGSIZE */
    "Filename too long",	/* ENAMETOOLONG */
    "Too many open files in system",/* ENFILE */
    "No such device",		/* ENODEV */
    "No such file or directory",/* ENOENT */
    "Exec format error",	/* ENOEXEC */
    "No locks available",	/* ENOLCK */
    "Not enough space",		/* ENOMEM */
    "No space left on device",	/* ENOSPC */
    "Function not implemented",	/* ENOSYS */
    "Not a directory",		/* ENOTDIR */
    "Directory not empty",	/* ENOTEMPTY */
    "Not supported",		/* ENOTSUP */
    "Inappropriate I/O control operation",/* ENOTTY */
    "No such device or address",/* ENXIO */
    "Operation not permitted",	/* EPERM */
    "Broken pipe",		/* EPIPE */
    "Result too large",		/* ERANGE */
    "Read-only file system",	/* EROFS */
    "Invalid seek",		/* ESPIPE */
    "No such process",		/* ESRCH */
    "Operation timed out",	/* ETIMEDOUT */
    "Improper link",		/* EXDEV */
    "Attempt to load network entity %s", /* XML_IO_NETWORK_ATTEMPT */
    "encoder error",		/* XML_IO_ENCODER */
    "flush error",
    "write error",
    "no input",
    "buffer full",
    "loading error",
    "not a socket",		/* ENOTSOCK */
    "already connected",	/* EISCONN */
    "connection refused",	/* ECONNREFUSED */
    "unreachable network",	/* ENETUNREACH */
    "adddress in use",		/* EADDRINUSE */
    "already in use",		/* EALREADY */
    "unknown address familly",	/* EAFNOSUPPORT */
};

#if defined(_WIN32) || defined (__DJGPP__) && !defined (__CYGWIN__)
/**
 * __xmlIOWin32UTF8ToWChar:
 * @u8String:  uft-8 string
 *
 * Convert a string from utf-8 to wchar (WINDOWS ONLY!)
 */
static wchar_t *
__xmlIOWin32UTF8ToWChar(const char *u8String)
{
    wchar_t *wString = NULL;

    if (u8String) {
        int wLen =
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, u8String,
                                -1, NULL, 0);
        if (wLen) {
            wString = xmlMalloc(wLen * sizeof(wchar_t));
            if (wString) {
                if (MultiByteToWideChar
                    (CP_UTF8, 0, u8String, -1, wString, wLen) == 0) {
                    xmlFree(wString);
                    wString = NULL;
                }
            }
        }
    }

    return wString;
}
#endif

/**
 * xmlIOErrMemory:
 * @extra:  extra informations
 *
 * Handle an out of memory condition
 */
static void
xmlIOErrMemory(const char *extra)
{
    __xmlSimpleError(XML_FROM_IO, XML_ERR_NO_MEMORY, NULL, NULL, extra);
}

/**
 * __xmlIOErr:
 * @code:  the error number
 * @
 * @extra:  extra informations
 *
 * Handle an I/O error
 */
void
__xmlIOErr(int domain, int code, const char *extra)
{
    unsigned int idx;

    if (code == 0) {
#ifdef HAVE_ERRNO_H
	if (errno == 0) code = 0;
#ifdef EACCES
        else if (errno == EACCES) code = XML_IO_EACCES;
#endif
#ifdef EAGAIN
        else if (errno == EAGAIN) code = XML_IO_EAGAIN;
#endif
#ifdef EBADF
        else if (errno == EBADF) code = XML_IO_EBADF;
#endif
#ifdef EBADMSG
        else if (errno == EBADMSG) code = XML_IO_EBADMSG;
#endif
#ifdef EBUSY
        else if (errno == EBUSY) code = XML_IO_EBUSY;
#endif
#ifdef ECANCELED
        else if (errno == ECANCELED) code = XML_IO_ECANCELED;
#endif
#ifdef ECHILD
        else if (errno == ECHILD) code = XML_IO_ECHILD;
#endif
#ifdef EDEADLK
        else if (errno == EDEADLK) code = XML_IO_EDEADLK;
#endif
#ifdef EDOM
        else if (errno == EDOM) code = XML_IO_EDOM;
#endif
#ifdef EEXIST
        else if (errno == EEXIST) code = XML_IO_EEXIST;
#endif
#ifdef EFAULT
        else if (errno == EFAULT) code = XML_IO_EFAULT;
#endif
#ifdef EFBIG
        else if (errno == EFBIG) code = XML_IO_EFBIG;
#endif
#ifdef EINPROGRESS
        else if (errno == EINPROGRESS) code = XML_IO_EINPROGRESS;
#endif
#ifdef EINTR
        else if (errno == EINTR) code = XML_IO_EINTR;
#endif
#ifdef EINVAL
        else if (errno == EINVAL) code = XML_IO_EINVAL;
#endif
#ifdef EIO
        else if (errno == EIO) code = XML_IO_EIO;
#endif
#ifdef EISDIR
        else if (errno == EISDIR) code = XML_IO_EISDIR;
#endif
#ifdef EMFILE
        else if (errno == EMFILE) code = XML_IO_EMFILE;
#endif
#ifdef EMLINK
        else if (errno == EMLINK) code = XML_IO_EMLINK;
#endif
#ifdef EMSGSIZE
        else if (errno == EMSGSIZE) code = XML_IO_EMSGSIZE;
#endif
#ifdef ENAMETOOLONG
        else if (errno == ENAMETOOLONG) code = XML_IO_ENAMETOOLONG;
#endif
#ifdef ENFILE
        else if (errno == ENFILE) code = XML_IO_ENFILE;
#endif
#ifdef ENODEV
        else if (errno == ENODEV) code = XML_IO_ENODEV;
#endif
#ifdef ENOENT
        else if (errno == ENOENT) code = XML_IO_ENOENT;
#endif
#ifdef ENOEXEC
        else if (errno == ENOEXEC) code = XML_IO_ENOEXEC;
#endif
#ifdef ENOLCK
        else if (errno == ENOLCK) code = XML_IO_ENOLCK;
#endif
#ifdef ENOMEM
        else if (errno == ENOMEM) code = XML_IO_ENOMEM;
#endif
#ifdef ENOSPC
        else if (errno == ENOSPC) code = XML_IO_ENOSPC;
#endif
#ifdef ENOSYS
        else if (errno == ENOSYS) code = XML_IO_ENOSYS;
#endif
#ifdef ENOTDIR
        else if (errno == ENOTDIR) code = XML_IO_ENOTDIR;
#endif
#ifdef ENOTEMPTY
        else if (errno == ENOTEMPTY) code = XML_IO_ENOTEMPTY;
#endif
#ifdef ENOTSUP
        else if (errno == ENOTSUP) code = XML_IO_ENOTSUP;
#endif
#ifdef ENOTTY
        else if (errno == ENOTTY) code = XML_IO_ENOTTY;
#endif
#ifdef ENXIO
        else if (errno == ENXIO) code = XML_IO_ENXIO;
#endif
#ifdef EPERM
        else if (errno == EPERM) code = XML_IO_EPERM;
#endif
#ifdef EPIPE
        else if (errno == EPIPE) code = XML_IO_EPIPE;
#endif
#ifdef ERANGE
        else if (errno == ERANGE) code = XML_IO_ERANGE;
#endif
#ifdef EROFS
        else if (errno == EROFS) code = XML_IO_EROFS;
#endif
#ifdef ESPIPE
        else if (errno == ESPIPE) code = XML_IO_ESPIPE;
#endif
#ifdef ESRCH
        else if (errno == ESRCH) code = XML_IO_ESRCH;
#endif
#ifdef ETIMEDOUT
        else if (errno == ETIMEDOUT) code = XML_IO_ETIMEDOUT;
#endif
#ifdef EXDEV
        else if (errno == EXDEV) code = XML_IO_EXDEV;
#endif
#ifdef ENOTSOCK
        else if (errno == ENOTSOCK) code = XML_IO_ENOTSOCK;
#endif
#ifdef EISCONN
        else if (errno == EISCONN) code = XML_IO_EISCONN;
#endif
#ifdef ECONNREFUSED
        else if (errno == ECONNREFUSED) code = XML_IO_ECONNREFUSED;
#endif
#ifdef ETIMEDOUT
        else if (errno == ETIMEDOUT) code = XML_IO_ETIMEDOUT;
#endif
#ifdef ENETUNREACH
        else if (errno == ENETUNREACH) code = XML_IO_ENETUNREACH;
#endif
#ifdef EADDRINUSE
        else if (errno == EADDRINUSE) code = XML_IO_EADDRINUSE;
#endif
#ifdef EINPROGRESS
        else if (errno == EINPROGRESS) code = XML_IO_EINPROGRESS;
#endif
#ifdef EALREADY
        else if (errno == EALREADY) code = XML_IO_EALREADY;
#endif
#ifdef EAFNOSUPPORT
        else if (errno == EAFNOSUPPORT) code = XML_IO_EAFNOSUPPORT;
#endif
        else code = XML_IO_UNKNOWN;
#endif /* HAVE_ERRNO_H */
    }
    idx = 0;
    if (code >= XML_IO_UNKNOWN) idx = code - XML_IO_UNKNOWN;
    if (idx >= (sizeof(IOerr) / sizeof(IOerr[0]))) idx = 0;

    __xmlSimpleError(domain, code, NULL, IOerr[idx], extra);
}

/**
 * xmlIOErr:
 * @code:  the error number
 * @extra:  extra informations
 *
 * Handle an I/O error
 */
static void
xmlIOErr(int code, const char *extra)
{
    __xmlIOErr(XML_FROM_IO, code, extra);
}

/**
 * __xmlLoaderErr:
 * @ctx: the parser context
 * @extra:  extra informations
 *
 * Handle a resource access error
 */
void
__xmlLoaderErr(void *ctx, const char *msg, const char *filename)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlStructuredErrorFunc schannel = NULL;
    xmlGenericErrorFunc channel = NULL;
    void *data = NULL;
    xmlErrorLevel level = XML_ERR_ERROR;

    if ((ctxt != NULL) && (ctxt->disableSAX != 0) &&
        (ctxt->instate == XML_PARSER_EOF))
	return;
    if ((ctxt != NULL) && (ctxt->sax != NULL)) {
        if (ctxt->validate) {
	    channel = ctxt->sax->error;
	    level = XML_ERR_ERROR;
	} else {
	    channel = ctxt->sax->warning;
	    level = XML_ERR_WARNING;
	}
	if (ctxt->sax->initialized == XML_SAX2_MAGIC)
	    schannel = ctxt->sax->serror;
	data = ctxt->userData;
    }
    __xmlRaiseError(schannel, channel, data, ctxt, NULL, XML_FROM_IO,
                    XML_IO_LOAD_ERROR, level, NULL, 0,
		    filename, NULL, NULL, 0, 0,
		    msg, filename);

}

/************************************************************************
 *									*
 *		Tree memory error handler				*
 *									*
 ************************************************************************/
/**
 * xmlNormalizeWindowsPath:
 * @path: the input file path
 *
 * This function is obsolete. Please see xmlURIFromPath in uri.c for
 * a better solution.
 *
 * Returns a canonicalized version of the path
 */
xmlChar *
xmlNormalizeWindowsPath(const xmlChar *path)
{
    return xmlCanonicPath(path);
}

/**
 * xmlCleanupInputCallbacks:
 *
 * clears the entire input callback table. this includes the
 * compiled-in I/O.
 */
void
xmlCleanupInputCallbacks(void)
{
    int i;

    if (!xmlInputCallbackInitialized)
        return;

    for (i = xmlInputCallbackNr - 1; i >= 0; i--) {
        xmlInputCallbackTable[i].matchcallback = NULL;
        xmlInputCallbackTable[i].opencallback = NULL;
        xmlInputCallbackTable[i].readcallback = NULL;
        xmlInputCallbackTable[i].closecallback = NULL;
    }

    xmlInputCallbackNr = 0;
    xmlInputCallbackInitialized = 0;
}

/**
 * xmlPopInputCallbacks:
 *
 * Clear the top input callback from the input stack. this includes the
 * compiled-in I/O.
 *
 * Returns the number of input callback registered or -1 in case of error.
 */
int
xmlPopInputCallbacks(void)
{
    if (!xmlInputCallbackInitialized)
        return(-1);

    if (xmlInputCallbackNr <= 0)
        return(-1);

    xmlInputCallbackNr--;
    xmlInputCallbackTable[xmlInputCallbackNr].matchcallback = NULL;
    xmlInputCallbackTable[xmlInputCallbackNr].opencallback = NULL;
    xmlInputCallbackTable[xmlInputCallbackNr].readcallback = NULL;
    xmlInputCallbackTable[xmlInputCallbackNr].closecallback = NULL;

    return(xmlInputCallbackNr);
}

/************************************************************************
 *									*
 *		Standard I/O for file accesses				*
 *									*
 ************************************************************************/

#if defined(_WIN32) || defined (__DJGPP__) && !defined (__CYGWIN__)

/**
 *  xmlWrapOpenUtf8:
 * @path:  the path in utf-8 encoding
 * @mode:  type of access (0 - read, 1 - write)
 *
 * function opens the file specified by @path
 *
 */
static FILE*
xmlWrapOpenUtf8(const char *path,int mode)
{
    FILE *fd = NULL;
    wchar_t *wPath;

    wPath = __xmlIOWin32UTF8ToWChar(path);
    if(wPath)
    {
       fd = _wfopen(wPath, mode ? L"wb" : L"rb");
       xmlFree(wPath);
    }
    /* maybe path in native encoding */
    if(fd == NULL)
       fd = fopen(path, mode ? "wb" : "rb");

    return fd;
}

#ifdef HAVE_ZLIB_H
static gzFile
xmlWrapGzOpenUtf8(const char *path, const char *mode)
{
    gzFile fd;
    wchar_t *wPath;

    fd = gzopen (path, mode);
    if (fd)
        return fd;

    wPath = __xmlIOWin32UTF8ToWChar(path);
    if(wPath)
    {
	int d, m = (strstr(mode, "r") ? O_RDONLY : O_RDWR);
#ifdef _O_BINARY
        m |= (strstr(mode, "b") ? _O_BINARY : 0);
#endif
	d = _wopen(wPath, m);
	if (d >= 0)
	    fd = gzdopen(d, mode);
        xmlFree(wPath);
    }

    return fd;
}
#endif

/**
 *  xmlWrapStatUtf8:
 * @path:  the path in utf-8 encoding
 * @info:  structure that stores results
 *
 * function obtains information about the file or directory
 *
 */
static int
xmlWrapStatUtf8(const char *path,struct stat *info)
{
#ifdef HAVE_STAT
    int retval = -1;
    wchar_t *wPath;

    wPath = __xmlIOWin32UTF8ToWChar(path);
    if (wPath)
    {
       retval = _wstat(wPath,info);
       xmlFree(wPath);
    }
    /* maybe path in native encoding */
    if(retval < 0)
       retval = stat(path,info);
    return retval;
#else
    return -1;
#endif
}

/**
 *  xmlWrapOpenNative:
 * @path:  the path
 * @mode:  type of access (0 - read, 1 - write)
 *
 * function opens the file specified by @path
 *
 */
static FILE*
xmlWrapOpenNative(const char *path,int mode)
{
    return fopen(path,mode ? "wb" : "rb");
}

/**
 *  xmlWrapStatNative:
 * @path:  the path
 * @info:  structure that stores results
 *
 * function obtains information about the file or directory
 *
 */
static int
xmlWrapStatNative(const char *path,struct stat *info)
{
#ifdef HAVE_STAT
    return stat(path,info);
#else
    return -1;
#endif
}

typedef int (* xmlWrapStatFunc) (const char *f, struct stat *s);
static xmlWrapStatFunc xmlWrapStat = xmlWrapStatNative;
typedef FILE* (* xmlWrapOpenFunc)(const char *f,int mode);
static xmlWrapOpenFunc xmlWrapOpen = xmlWrapOpenNative;
#ifdef HAVE_ZLIB_H
typedef gzFile (* xmlWrapGzOpenFunc) (const char *f, const char *mode);
static xmlWrapGzOpenFunc xmlWrapGzOpen = gzopen;
#endif
/**
 * xmlInitPlatformSpecificIo:
 *
 * Initialize platform specific features.
 */
static void
xmlInitPlatformSpecificIo(void)
{
    static int xmlPlatformIoInitialized = 0;
    OSVERSIONINFO osvi;

    if(xmlPlatformIoInitialized)
      return;

    osvi.dwOSVersionInfoSize = sizeof(osvi);

    if(GetVersionEx(&osvi) && (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT)) {
      xmlWrapStat = xmlWrapStatUtf8;
      xmlWrapOpen = xmlWrapOpenUtf8;
#ifdef HAVE_ZLIB_H
      xmlWrapGzOpen = xmlWrapGzOpenUtf8;
#endif
    } else {
      xmlWrapStat = xmlWrapStatNative;
      xmlWrapOpen = xmlWrapOpenNative;
#ifdef HAVE_ZLIB_H
      xmlWrapGzOpen = gzopen;
#endif
    }

    xmlPlatformIoInitialized = 1;
    return;
}

#endif

/**
 * xmlCheckFilename:
 * @path:  the path to check
 *
 * function checks to see if @path is a valid source
 * (file, socket...) for XML.
 *
 * if stat is not available on the target machine,
 * returns 1.  if stat fails, returns 0 (if calling
 * stat on the filename fails, it can't be right).
 * if stat succeeds and the file is a directory,
 * returns 2.  otherwise returns 1.
 */

int
xmlCheckFilename (const char *path)
{
#ifdef HAVE_STAT
	struct stat stat_buffer;
#endif
	if (path == NULL)
		return(0);

#ifdef HAVE_STAT
#if defined(_WIN32) || defined (__DJGPP__) && !defined (__CYGWIN__)
    if (xmlWrapStat(path, &stat_buffer) == -1)
        return 0;
#else
    if (stat(path, &stat_buffer) == -1)
        return 0;
#endif
#ifdef S_ISDIR
    if (S_ISDIR(stat_buffer.st_mode))
        return 2;
#endif
#endif /* HAVE_STAT */
    return 1;
}

static int
xmlNop(void) {
    return(0);
}

/**
 * xmlFdRead:
 * @context:  the I/O context
 * @buffer:  where to drop data
 * @len:  number of bytes to read
 *
 * Read @len bytes to @buffer from the I/O channel.
 *
 * Returns the number of bytes written
 */
static int
xmlFdRead (void * context, char * buffer, int len) {
    int ret;

    ret = read((int) (long) context, &buffer[0], len);
    if (ret < 0) xmlIOErr(0, "read()");
    return(ret);
}

/**
 * xmlFdClose:
 * @context:  the I/O context
 *
 * Close an I/O channel
 *
 * Returns 0 in case of success and error code otherwise
 */
static int
xmlFdClose (void * context) {
    int ret;
    ret = close((int) (long) context);
    if (ret < 0) xmlIOErr(0, "close()");
    return(ret);
}

/**
 * xmlFileMatch:
 * @filename:  the URI for matching
 *
 * input from FILE *
 *
 * Returns 1 if matches, 0 otherwise
 */
int
xmlFileMatch (const char *filename ATTRIBUTE_UNUSED) {
    return(1);
}

/**
 * xmlFileOpen_real:
 * @filename:  the URI for matching
 *
 * input from FILE *, supports compressed input
 * if @filename is " " then the standard input is used
 *
 * Returns an I/O context or NULL in case of error
 */
static void *
xmlFileOpen_real (const char *filename) {
    const char *path = NULL;
    FILE *fd;

    if (filename == NULL)
        return(NULL);

    if (!strcmp(filename, "-")) {
	fd = stdin;
	return((void *) fd);
    }

    if (!xmlStrncasecmp(BAD_CAST filename, BAD_CAST "file://localhost/", 17)) {
#if defined (_WIN32) || defined (__DJGPP__) && !defined(__CYGWIN__)
	path = &filename[17];
#else
	path = &filename[16];
#endif
    } else if (!xmlStrncasecmp(BAD_CAST filename, BAD_CAST "file:///", 8)) {
#if defined (_WIN32) || defined (__DJGPP__) && !defined(__CYGWIN__)
	path = &filename[8];
#else
	path = &filename[7];
#endif
    } else if (!xmlStrncasecmp(BAD_CAST filename, BAD_CAST "file:/", 6)) {
        /* lots of generators seems to lazy to read RFC 1738 */
#if defined (_WIN32) || defined (__DJGPP__) && !defined(__CYGWIN__)
	path = &filename[6];
#else
	path = &filename[5];
#endif
    } else
	path = filename;

    if (path == NULL)
	return(NULL);
    if (!xmlCheckFilename(path))
        return(NULL);

#if defined(_WIN32) || defined (__DJGPP__) && !defined (__CYGWIN__)
    fd = xmlWrapOpen(path, 0);
#else
    fd = fopen(path, "r");
#endif /* WIN32 */
    if (fd == NULL) xmlIOErr(0, path);
    return((void *) fd);
}

/**
 * xmlFileOpen:
 * @filename:  the URI for matching
 *
 * Wrapper around xmlFileOpen_real that try it with an unescaped
 * version of @filename, if this fails fallback to @filename
 *
 * Returns a handler or NULL in case or failure
 */
void *
xmlFileOpen (const char *filename) {
    char *unescaped;
    void *retval;

    retval = xmlFileOpen_real(filename);
    if (retval == NULL) {
	unescaped = xmlURIUnescapeString(filename, 0, NULL);
	if (unescaped != NULL) {
	    retval = xmlFileOpen_real(unescaped);
	    xmlFree(unescaped);
	}
    }

    return retval;
}

/**
 * xmlFileRead:
 * @context:  the I/O context
 * @buffer:  where to drop data
 * @len:  number of bytes to write
 *
 * Read @len bytes to @buffer from the I/O channel.
 *
 * Returns the number of bytes written or < 0 in case of failure
 */
int
xmlFileRead (void * context, char * buffer, int len) {
    int ret;
    if ((context == NULL) || (buffer == NULL))
        return(-1);
    ret = fread(&buffer[0], 1,  len, (FILE *) context);
    if (ret < 0) xmlIOErr(0, "fread()");
    return(ret);
}

/**
 * xmlFileClose:
 * @context:  the I/O context
 *
 * Close an I/O channel
 *
 * Returns 0 or -1 in case of error
 */
int
xmlFileClose (void * context) {
    FILE *fil;
    int ret;

    if (context == NULL)
        return(-1);
    fil = (FILE *) context;
    if ((fil == stdout) || (fil == stderr)) {
        ret = fflush(fil);
	if (ret < 0)
	    xmlIOErr(0, "fflush()");
	return(0);
    }
    if (fil == stdin)
	return(0);
    ret = ( fclose((FILE *) context) == EOF ) ? -1 : 0;
    if (ret < 0)
        xmlIOErr(0, "fclose()");
    return(ret);
}

/**
 * xmlFileFlush:
 * @context:  the I/O context
 *
 * Flush an I/O channel
 */
static int
xmlFileFlush (void * context) {
    int ret;

    if (context == NULL)
        return(-1);
    ret = ( fflush((FILE *) context) == EOF ) ? -1 : 0;
    if (ret < 0)
        xmlIOErr(0, "fflush()");
    return(ret);
}

#ifdef HAVE_ZLIB_H
/************************************************************************
 *									*
 *		I/O for compressed file accesses			*
 *									*
 ************************************************************************/
/**
 * xmlGzfileMatch:
 * @filename:  the URI for matching
 *
 * input from compressed file test
 *
 * Returns 1 if matches, 0 otherwise
 */
static int
xmlGzfileMatch (const char *filename ATTRIBUTE_UNUSED) {
    return(1);
}

/**
 * xmlGzfileOpen_real:
 * @filename:  the URI for matching
 *
 * input from compressed file open
 * if @filename is " " then the standard input is used
 *
 * Returns an I/O context or NULL in case of error
 */
static void *
xmlGzfileOpen_real (const char *filename) {
    const char *path = NULL;
    gzFile fd;

    if (!strcmp(filename, "-")) {
        fd = gzdopen(dup(0), "rb");
	return((void *) fd);
    }

    if (!xmlStrncasecmp(BAD_CAST filename, BAD_CAST "file://localhost/", 17))
#if defined (_WIN32) || defined (__DJGPP__) && !defined(__CYGWIN__)
	path = &filename[17];
#else
	path = &filename[16];
#endif
    else if (!xmlStrncasecmp(BAD_CAST filename, BAD_CAST "file:///", 8)) {
#if defined (_WIN32) || defined (__DJGPP__) && !defined(__CYGWIN__)
	path = &filename[8];
#else
	path = &filename[7];
#endif
    } else
	path = filename;

    if (path == NULL)
	return(NULL);
    if (!xmlCheckFilename(path))
        return(NULL);

#if defined(_WIN32) || defined (__DJGPP__) && !defined (__CYGWIN__)
    fd = xmlWrapGzOpen(path, "rb");
#else
    fd = gzopen(path, "rb");
#endif
    return((void *) fd);
}

/**
 * xmlGzfileOpen:
 * @filename:  the URI for matching
 *
 * Wrapper around xmlGzfileOpen if the open fais, it will
 * try to unescape @filename
 */
static void *
xmlGzfileOpen (const char *filename) {
    char *unescaped;
    void *retval;

    retval = xmlGzfileOpen_real(filename);
    if (retval == NULL) {
	unescaped = xmlURIUnescapeString(filename, 0, NULL);
	if (unescaped != NULL) {
	    retval = xmlGzfileOpen_real(unescaped);
	}
	xmlFree(unescaped);
    }
    return retval;
}

/**
 * xmlGzfileRead:
 * @context:  the I/O context
 * @buffer:  where to drop data
 * @len:  number of bytes to write
 *
 * Read @len bytes to @buffer from the compressed I/O channel.
 *
 * Returns the number of bytes written
 */
static int
xmlGzfileRead (void * context, char * buffer, int len) {
    int ret;

    ret = gzread((gzFile) context, &buffer[0], len);
    if (ret < 0) xmlIOErr(0, "gzread()");
    return(ret);
}

/**
 * xmlGzfileClose:
 * @context:  the I/O context
 *
 * Close a compressed I/O channel
 */
static int
xmlGzfileClose (void * context) {
    int ret;

    ret =  (gzclose((gzFile) context) == Z_OK ) ? 0 : -1;
    if (ret < 0) xmlIOErr(0, "gzclose()");
    return(ret);
}
#endif /* HAVE_ZLIB_H */

#ifdef HAVE_LZMA_H
/************************************************************************
 *									*
 *		I/O for compressed file accesses			*
 *									*
 ************************************************************************/
#include "xzlib.h"
/**
 * xmlXzfileMatch:
 * @filename:  the URI for matching
 *
 * input from compressed file test
 *
 * Returns 1 if matches, 0 otherwise
 */
static int
xmlXzfileMatch (const char *filename ATTRIBUTE_UNUSED) {
    return(1);
}

/**
 * xmlXzFileOpen_real:
 * @filename:  the URI for matching
 *
 * input from compressed file open
 * if @filename is " " then the standard input is used
 *
 * Returns an I/O context or NULL in case of error
 */
static void *
xmlXzfileOpen_real (const char *filename) {
    const char *path = NULL;
    xzFile fd;

    if (!strcmp(filename, "-")) {
        fd = __libxml2_xzdopen(dup(0), "rb");
	return((void *) fd);
    }

    if (!xmlStrncasecmp(BAD_CAST filename, BAD_CAST "file://localhost/", 17)) {
	path = &filename[16];
    } else if (!xmlStrncasecmp(BAD_CAST filename, BAD_CAST "file:///", 8)) {
	path = &filename[7];
    } else if (!xmlStrncasecmp(BAD_CAST filename, BAD_CAST "file:/", 6)) {
        /* lots of generators seems to lazy to read RFC 1738 */
	path = &filename[5];
    } else
	path = filename;

    if (path == NULL)
	return(NULL);
    if (!xmlCheckFilename(path))
        return(NULL);

    fd = __libxml2_xzopen(path, "rb");
    return((void *) fd);
}

/**
 * xmlXzfileOpen:
 * @filename:  the URI for matching
 *
 * Wrapper around xmlXzfileOpen_real that try it with an unescaped
 * version of @filename, if this fails fallback to @filename
 *
 * Returns a handler or NULL in case or failure
 */
static void *
xmlXzfileOpen (const char *filename) {
    char *unescaped;
    void *retval;

    retval = xmlXzfileOpen_real(filename);
    if (retval == NULL) {
	unescaped = xmlURIUnescapeString(filename, 0, NULL);
	if (unescaped != NULL) {
	    retval = xmlXzfileOpen_real(unescaped);
	}
	xmlFree(unescaped);
    }

    return retval;
}

/**
 * xmlXzfileRead:
 * @context:  the I/O context
 * @buffer:  where to drop data
 * @len:  number of bytes to write
 *
 * Read @len bytes to @buffer from the compressed I/O channel.
 *
 * Returns the number of bytes written
 */
static int
xmlXzfileRead (void * context, char * buffer, int len) {
    int ret;

    ret = __libxml2_xzread((xzFile) context, &buffer[0], len);
    if (ret < 0) xmlIOErr(0, "xzread()");
    return(ret);
}

/**
 * xmlXzfileClose:
 * @context:  the I/O context
 *
 * Close a compressed I/O channel
 */
static int
xmlXzfileClose (void * context) {
    int ret;

    ret =  (__libxml2_xzclose((xzFile) context) == LZMA_OK ) ? 0 : -1;
    if (ret < 0) xmlIOErr(0, "xzclose()");
    return(ret);
}
#endif /* HAVE_LZMA_H */

/**
 * xmlRegisterInputCallbacks:
 * @matchFunc:  the xmlInputMatchCallback
 * @openFunc:  the xmlInputOpenCallback
 * @readFunc:  the xmlInputReadCallback
 * @closeFunc:  the xmlInputCloseCallback
 *
 * Register a new set of I/O callback for handling parser input.
 *
 * Returns the registered handler number or -1 in case of error
 */
int
xmlRegisterInputCallbacks(xmlInputMatchCallback matchFunc,
	xmlInputOpenCallback openFunc, xmlInputReadCallback readFunc,
	xmlInputCloseCallback closeFunc) {
    if (xmlInputCallbackNr >= MAX_INPUT_CALLBACK) {
	return(-1);
    }
    xmlInputCallbackTable[xmlInputCallbackNr].matchcallback = matchFunc;
    xmlInputCallbackTable[xmlInputCallbackNr].opencallback = openFunc;
    xmlInputCallbackTable[xmlInputCallbackNr].readcallback = readFunc;
    xmlInputCallbackTable[xmlInputCallbackNr].closecallback = closeFunc;
    xmlInputCallbackInitialized = 1;
    return(xmlInputCallbackNr++);
}

/**
 * xmlRegisterDefaultInputCallbacks:
 *
 * Registers the default compiled-in I/O handlers.
 */
void
xmlRegisterDefaultInputCallbacks(void) {
    if (xmlInputCallbackInitialized)
	return;

#if defined(_WIN32) || defined (__DJGPP__) && !defined (__CYGWIN__)
    xmlInitPlatformSpecificIo();
#endif

    xmlRegisterInputCallbacks(xmlFileMatch, xmlFileOpen,
	                      xmlFileRead, xmlFileClose);
#ifdef HAVE_ZLIB_H
    xmlRegisterInputCallbacks(xmlGzfileMatch, xmlGzfileOpen,
	                      xmlGzfileRead, xmlGzfileClose);
#endif /* HAVE_ZLIB_H */
#ifdef HAVE_LZMA_H
    xmlRegisterInputCallbacks(xmlXzfileMatch, xmlXzfileOpen,
	                      xmlXzfileRead, xmlXzfileClose);
#endif /* HAVE_ZLIB_H */

    xmlInputCallbackInitialized = 1;
}


/**
 * xmlAllocParserInputBuffer:
 * @enc:  the charset encoding if known
 *
 * Create a buffered parser input for progressive parsing
 *
 * Returns the new parser input or NULL
 */
xmlParserInputBufferPtr
xmlAllocParserInputBuffer(xmlCharEncoding enc) {
    xmlParserInputBufferPtr ret;

    ret = (xmlParserInputBufferPtr) xmlMalloc(sizeof(xmlParserInputBuffer));
    if (ret == NULL) {
	xmlIOErrMemory("creating input buffer");
	return(NULL);
    }
    memset(ret, 0, (size_t) sizeof(xmlParserInputBuffer));
    ret->buffer = xmlBufferCreateSize(2 * xmlDefaultBufferSize);
    if (ret->buffer == NULL) {
        xmlFree(ret);
	return(NULL);
    }
    ret->buffer->alloc = XML_BUFFER_ALLOC_DOUBLEIT;
    ret->encoder = xmlGetCharEncodingHandler(enc);
    if (ret->encoder != NULL)
        ret->raw = xmlBufferCreateSize(2 * xmlDefaultBufferSize);
    else
        ret->raw = NULL;
    ret->readcallback = NULL;
    ret->closecallback = NULL;
    ret->context = NULL;
    ret->compressed = -1;
    ret->rawconsumed = 0;

    return(ret);
}

/**
 * xmlFreeParserInputBuffer:
 * @in:  a buffered parser input
 *
 * Free up the memory used by a buffered parser input
 */
void
xmlFreeParserInputBuffer(xmlParserInputBufferPtr in) {
    if (in == NULL) return;

    if (in->raw) {
        xmlBufferFree(in->raw);
	in->raw = NULL;
    }
    if (in->encoder != NULL) {
        xmlCharEncCloseFunc(in->encoder);
    }
    if (in->closecallback != NULL) {
	in->closecallback(in->context);
    }
    if (in->buffer != NULL) {
        xmlBufferFree(in->buffer);
	in->buffer = NULL;
    }

    xmlFree(in);
}


xmlParserInputBufferPtr
__xmlParserInputBufferCreateFilename(const char *URI, xmlCharEncoding enc) {
    xmlParserInputBufferPtr ret;
    int i = 0;
    void *context = NULL;

    if (xmlInputCallbackInitialized == 0)
	xmlRegisterDefaultInputCallbacks();

    if (URI == NULL) return(NULL);

    /*
     * Try to find one of the input accept method accepting that scheme
     * Go in reverse to give precedence to user defined handlers.
     */
    if (context == NULL) {
	for (i = xmlInputCallbackNr - 1;i >= 0;i--) {
	    if ((xmlInputCallbackTable[i].matchcallback != NULL) &&
		(xmlInputCallbackTable[i].matchcallback(URI) != 0)) {
		context = xmlInputCallbackTable[i].opencallback(URI);
		if (context != NULL) {
		    break;
		}
	    }
	}
    }
    if (context == NULL) {
	return(NULL);
    }

    /*
     * Allocate the Input buffer front-end.
     */
    ret = xmlAllocParserInputBuffer(enc);
    if (ret != NULL) {
	ret->context = context;
	ret->readcallback = xmlInputCallbackTable[i].readcallback;
	ret->closecallback = xmlInputCallbackTable[i].closecallback;
#ifdef HAVE_ZLIB_H
	if ((xmlInputCallbackTable[i].opencallback == xmlGzfileOpen) &&
		(strcmp(URI, "-") != 0)) {
#if defined(ZLIB_VERNUM) && ZLIB_VERNUM >= 0x1230
            ret->compressed = !gzdirect(context);
#else
	    if (((z_stream *)context)->avail_in > 4) {
	        char *cptr, buff4[4];
		cptr = (char *) ((z_stream *)context)->next_in;
		if (gzread(context, buff4, 4) == 4) {
		    if (strncmp(buff4, cptr, 4) == 0)
		        ret->compressed = 0;
		    else
		        ret->compressed = 1;
		    gzrewind(context);
		}
	    }
#endif
	}
#endif
    }
    else
      xmlInputCallbackTable[i].closecallback (context);

    return(ret);
}

/**
 * xmlParserInputBufferCreateFilename:
 * @URI:  a C string containing the URI or filename
 * @enc:  the charset encoding if known
 *
 * Create a buffered parser input for the progressive parsing of a file
 * If filename is "-' then we use stdin as the input.
 * Automatic support for ZLIB/Compress compressed document is provided
 * by default if found at compile-time.
 * Do an encoding check if enc == XML_CHAR_ENCODING_NONE
 *
 * Returns the new parser input or NULL
 */
xmlParserInputBufferPtr
xmlParserInputBufferCreateFilename(const char *URI, xmlCharEncoding enc) {
    if ((xmlParserInputBufferCreateFilenameValue)) {
		return xmlParserInputBufferCreateFilenameValue(URI, enc);
	}
	return __xmlParserInputBufferCreateFilename(URI, enc);
}

/**
 * xmlParserInputBufferCreateFile:
 * @file:  a FILE*
 * @enc:  the charset encoding if known
 *
 * Create a buffered parser input for the progressive parsing of a FILE *
 * buffered C I/O
 *
 * Returns the new parser input or NULL
 */
xmlParserInputBufferPtr
xmlParserInputBufferCreateFile(FILE *file, xmlCharEncoding enc) {
    xmlParserInputBufferPtr ret;

    if (xmlInputCallbackInitialized == 0)
	xmlRegisterDefaultInputCallbacks();

    if (file == NULL) return(NULL);

    ret = xmlAllocParserInputBuffer(enc);
    if (ret != NULL) {
        ret->context = file;
	ret->readcallback = xmlFileRead;
	ret->closecallback = xmlFileFlush;
    }

    return(ret);
}

/**
 * xmlParserInputBufferCreateFd:
 * @fd:  a file descriptor number
 * @enc:  the charset encoding if known
 *
 * Create a buffered parser input for the progressive parsing for the input
 * from a file descriptor
 *
 * Returns the new parser input or NULL
 */
xmlParserInputBufferPtr
xmlParserInputBufferCreateFd(int fd, xmlCharEncoding enc) {
    xmlParserInputBufferPtr ret;

    if (fd < 0) return(NULL);

    ret = xmlAllocParserInputBuffer(enc);
    if (ret != NULL) {
        ret->context = (void *) (long) fd;
	ret->readcallback = xmlFdRead;
	ret->closecallback = xmlFdClose;
    }

    return(ret);
}

/**
 * xmlParserInputBufferCreateMem:
 * @mem:  the memory input
 * @size:  the length of the memory block
 * @enc:  the charset encoding if known
 *
 * Create a buffered parser input for the progressive parsing for the input
 * from a memory area.
 *
 * Returns the new parser input or NULL
 */
xmlParserInputBufferPtr
xmlParserInputBufferCreateMem(const char *mem, int size, xmlCharEncoding enc) {
    xmlParserInputBufferPtr ret;
    int errcode;

    if (size <= 0) return(NULL);
    if (mem == NULL) return(NULL);

    ret = xmlAllocParserInputBuffer(enc);
    if (ret != NULL) {
        ret->context = (void *) mem;
	ret->readcallback = (xmlInputReadCallback) xmlNop;
	ret->closecallback = NULL;
	errcode = xmlBufferAdd(ret->buffer, (const xmlChar *) mem, size);
	if (errcode != 0) {
	    xmlFree(ret);
	    return(NULL);
	}
    }

    return(ret);
}

/**
 * xmlParserInputBufferCreateStatic:
 * @mem:  the memory input
 * @size:  the length of the memory block
 * @enc:  the charset encoding if known
 *
 * Create a buffered parser input for the progressive parsing for the input
 * from an immutable memory area. This will not copy the memory area to
 * the buffer, but the memory is expected to be available until the end of
 * the parsing, this is useful for example when using mmap'ed file.
 *
 * Returns the new parser input or NULL
 */
xmlParserInputBufferPtr
xmlParserInputBufferCreateStatic(const char *mem, int size,
                                 xmlCharEncoding enc) {
    xmlParserInputBufferPtr ret;

    if (size <= 0) return(NULL);
    if (mem == NULL) return(NULL);

    ret = (xmlParserInputBufferPtr) xmlMalloc(sizeof(xmlParserInputBuffer));
    if (ret == NULL) {
	xmlIOErrMemory("creating input buffer");
	return(NULL);
    }
    memset(ret, 0, (size_t) sizeof(xmlParserInputBuffer));
    ret->buffer = xmlBufferCreateStatic((void *)mem, (size_t) size);
    if (ret->buffer == NULL) {
        xmlFree(ret);
	return(NULL);
    }
    ret->encoder = xmlGetCharEncodingHandler(enc);
    if (ret->encoder != NULL)
        ret->raw = xmlBufferCreateSize(2 * xmlDefaultBufferSize);
    else
        ret->raw = NULL;
    ret->compressed = -1;
    ret->context = (void *) mem;
    ret->readcallback = NULL;
    ret->closecallback = NULL;

    return(ret);
}


/**
 * xmlParserInputBufferCreateIO:
 * @ioread:  an I/O read function
 * @ioclose:  an I/O close function
 * @ioctx:  an I/O handler
 * @enc:  the charset encoding if known
 *
 * Create a buffered parser input for the progressive parsing for the input
 * from an I/O handler
 *
 * Returns the new parser input or NULL
 */
xmlParserInputBufferPtr
xmlParserInputBufferCreateIO(xmlInputReadCallback   ioread,
	 xmlInputCloseCallback  ioclose, void *ioctx, xmlCharEncoding enc) {
    xmlParserInputBufferPtr ret;

    if (ioread == NULL) return(NULL);

    ret = xmlAllocParserInputBuffer(enc);
    if (ret != NULL) {
        ret->context = (void *) ioctx;
	ret->readcallback = ioread;
	ret->closecallback = ioclose;
    }

    return(ret);
}

/**
 * xmlParserInputBufferCreateFilenameDefault:
 * @func: function pointer to the new ParserInputBufferCreateFilenameFunc
 *
 * Registers a callback for URI input file handling
 *
 * Returns the old value of the registration function
 */
xmlParserInputBufferCreateFilenameFunc
xmlParserInputBufferCreateFilenameDefault(xmlParserInputBufferCreateFilenameFunc func)
{
    xmlParserInputBufferCreateFilenameFunc old = xmlParserInputBufferCreateFilenameValue;
    if (old == NULL) {
		old = __xmlParserInputBufferCreateFilename;
	}

    xmlParserInputBufferCreateFilenameValue = func;
    return(old);
}

/**
 * xmlOutputBufferCreateFilenameDefault:
 * @func: function pointer to the new OutputBufferCreateFilenameFunc
 *
 * Registers a callback for URI output file handling
 *
 * Returns the old value of the registration function
 */
xmlOutputBufferCreateFilenameFunc
xmlOutputBufferCreateFilenameDefault(xmlOutputBufferCreateFilenameFunc func)
{
    xmlOutputBufferCreateFilenameFunc old = xmlOutputBufferCreateFilenameValue;
    xmlOutputBufferCreateFilenameValue = func;
    return(old);
}

/**
 * xmlParserInputBufferPush:
 * @in:  a buffered parser input
 * @len:  the size in bytes of the array.
 * @buf:  an char array
 *
 * Push the content of the arry in the input buffer
 * This routine handle the I18N transcoding to internal UTF-8
 * This is used when operating the parser in progressive (push) mode.
 *
 * Returns the number of chars read and stored in the buffer, or -1
 *         in case of error.
 */
int
xmlParserInputBufferPush(xmlParserInputBufferPtr in,
	                 int len, const char *buf) {
    int nbchars = 0;
    int ret;

    if (len < 0) return(0);
    if ((in == NULL) || (in->error)) return(-1);
    if (in->encoder != NULL) {
        unsigned int use;

        /*
	 * Store the data in the incoming raw buffer
	 */
        if (in->raw == NULL) {
	    in->raw = xmlBufferCreate();
	}
	ret = xmlBufferAdd(in->raw, (const xmlChar *) buf, len);
	if (ret != 0)
	    return(-1);

	/*
	 * convert as much as possible to the parser reading buffer.
	 */
	use = in->raw->use;
	nbchars = xmlCharEncInFunc(in->encoder, in->buffer, in->raw);
	if (nbchars < 0) {
	    xmlIOErr(XML_IO_ENCODER, NULL);
	    in->error = XML_IO_ENCODER;
	    return(-1);
	}
	in->rawconsumed += (use - in->raw->use);
    } else {
	nbchars = len;
        ret = xmlBufferAdd(in->buffer, (xmlChar *) buf, nbchars);
	if (ret != 0)
	    return(-1);
    }
    return(nbchars);
}

/**
 * endOfInput:
 *
 * When reading from an Input channel indicated end of file or error
 * don't reread from it again.
 */
static int
endOfInput (void * context ATTRIBUTE_UNUSED,
	    char * buffer ATTRIBUTE_UNUSED,
	    int len ATTRIBUTE_UNUSED) {
    return(0);
}

/**
 * xmlParserInputBufferGrow:
 * @in:  a buffered parser input
 * @len:  indicative value of the amount of chars to read
 *
 * Grow up the content of the input buffer, the old data are preserved
 * This routine handle the I18N transcoding to internal UTF-8
 * This routine is used when operating the parser in normal (pull) mode
 *
 * TODO: one should be able to remove one extra copy by copying directly
 *       onto in->buffer or in->raw
 *
 * Returns the number of chars read and stored in the buffer, or -1
 *         in case of error.
 */
int
xmlParserInputBufferGrow(xmlParserInputBufferPtr in, int len) {
    char *buffer = NULL;
    int res = 0;
    int nbchars = 0;
    int buffree;
    unsigned int needSize;

    if ((in == NULL) || (in->error)) return(-1);
    if ((len <= MINLEN) && (len != 4))
        len = MINLEN;

    buffree = in->buffer->size - in->buffer->use;
    if (buffree <= 0) {
	xmlIOErr(XML_IO_BUFFER_FULL, NULL);
	in->error = XML_IO_BUFFER_FULL;
	return(-1);
    }

    needSize = in->buffer->use + len + 1;
    if (needSize > in->buffer->size){
        if (!xmlBufferResize(in->buffer, needSize)){
	    xmlIOErrMemory("growing input buffer");
	    in->error = XML_ERR_NO_MEMORY;
            return(-1);
        }
    }
    buffer = (char *)&in->buffer->content[in->buffer->use];

    /*
     * Call the read method for this I/O type.
     */
    if (in->readcallback != NULL) {
	res = in->readcallback(in->context, &buffer[0], len);
	if (res <= 0)
	    in->readcallback = endOfInput;
    } else {
	xmlIOErr(XML_IO_NO_INPUT, NULL);
	in->error = XML_IO_NO_INPUT;
	return(-1);
    }
    if (res < 0) {
	return(-1);
    }
    len = res;
    if (in->encoder != NULL) {
        unsigned int use;

        /*
	 * Store the data in the incoming raw buffer
	 */
        if (in->raw == NULL) {
	    in->raw = xmlBufferCreate();
	}
	res = xmlBufferAdd(in->raw, (const xmlChar *) buffer, len);
	if (res != 0)
	    return(-1);

	/*
	 * convert as much as possible to the parser reading buffer.
	 */
	use = in->raw->use;
	nbchars = xmlCharEncInFunc(in->encoder, in->buffer, in->raw);
	if (nbchars < 0) {
	    xmlIOErr(XML_IO_ENCODER, NULL);
	    in->error = XML_IO_ENCODER;
	    return(-1);
	}
	in->rawconsumed += (use - in->raw->use);
    } else {
	nbchars = len;
   	in->buffer->use += nbchars;
	buffer[nbchars] = 0;
    }
    return(nbchars);
}

/**
 * xmlParserInputBufferRead:
 * @in:  a buffered parser input
 * @len:  indicative value of the amount of chars to read
 *
 * Refresh the content of the input buffer, the old data are considered
 * consumed
 * This routine handle the I18N transcoding to internal UTF-8
 *
 * Returns the number of chars read and stored in the buffer, or -1
 *         in case of error.
 */
int
xmlParserInputBufferRead(xmlParserInputBufferPtr in, int len) {
    if ((in == NULL) || (in->error)) return(-1);
    if (in->readcallback != NULL)
	return(xmlParserInputBufferGrow(in, len));
    else if ((in->buffer != NULL) &&
             (in->buffer->alloc == XML_BUFFER_ALLOC_IMMUTABLE))
	return(0);
    else
        return(-1);
}

/**
 * xmlParserGetDirectory:
 * @filename:  the path to a file
 *
 * lookup the directory for that file
 *
 * Returns a new allocated string containing the directory, or NULL.
 */
char *
xmlParserGetDirectory(const char *filename) {
    char *ret = NULL;
    char dir[1024];
    char *cur;

#ifdef _WIN32_WCE  /* easy way by now ... wince does not have dirs! */
    return NULL;
#endif

    if (xmlInputCallbackInitialized == 0)
	xmlRegisterDefaultInputCallbacks();

    if (filename == NULL) return(NULL);

#if defined(WIN32) && !defined(__CYGWIN__)
#   define IS_XMLPGD_SEP(ch) ((ch=='/')||(ch=='\\'))
#else
#   define IS_XMLPGD_SEP(ch) (ch=='/')
#endif

    strncpy(dir, filename, 1023);
    dir[1023] = 0;
    cur = &dir[strlen(dir)];
    while (cur > dir) {
         if (IS_XMLPGD_SEP(*cur)) break;
	 cur --;
    }
    if (IS_XMLPGD_SEP(*cur)) {
        if (cur == dir) dir[1] = 0;
	else *cur = 0;
	ret = xmlMemStrdup(dir);
    } else {
        if (getcwd(dir, 1024) != NULL) {
	    dir[1023] = 0;
	    ret = xmlMemStrdup(dir);
	}
    }
    return(ret);
#undef IS_XMLPGD_SEP
}

/****************************************************************
 *								*
 *		External entities loading			*
 *								*
 ****************************************************************/

/**
 * xmlCheckHTTPInput:
 * @ctxt: an XML parser context
 * @ret: an XML parser input
 *
 * Check an input in case it was created from an HTTP stream, in that
 * case it will handle encoding and update of the base URL in case of
 * redirection. It also checks for HTTP errors in which case the input
 * is cleanly freed up and an appropriate error is raised in context
 *
 * Returns the input or NULL in case of HTTP error.
 */
xmlParserInputPtr
xmlCheckHTTPInput(xmlParserCtxtPtr ctxt, xmlParserInputPtr ret) {
    return(ret);
}

static int xmlNoNetExists(const char *URL) {
    const char *path;

    if (URL == NULL)
	return(0);

    if (!xmlStrncasecmp(BAD_CAST URL, BAD_CAST "file://localhost/", 17))
#if defined (_WIN32) || defined (__DJGPP__) && !defined(__CYGWIN__)
	path = &URL[17];
#else
	path = &URL[16];
#endif
    else if (!xmlStrncasecmp(BAD_CAST URL, BAD_CAST "file:///", 8)) {
#if defined (_WIN32) || defined (__DJGPP__) && !defined(__CYGWIN__)
	path = &URL[8];
#else
	path = &URL[7];
#endif
    } else
	path = URL;

    return xmlCheckFilename(path);
}

/**
 * xmlDefaultExternalEntityLoader:
 * @URL:  the URL for the entity to load
 * @ID:  the System ID for the entity to load
 * @ctxt:  the context in which the entity is called or NULL
 *
 * By default we don't load external entitites, yet.
 *
 * Returns a new allocated xmlParserInputPtr, or NULL.
 */
static xmlParserInputPtr
xmlDefaultExternalEntityLoader(const char *URL, const char *ID,
                               xmlParserCtxtPtr ctxt)
{
    xmlParserInputPtr ret = NULL;
    xmlChar *resource = NULL;

    if ((ctxt != NULL) && (ctxt->options & XML_PARSE_NONET)) {
        int options = ctxt->options;

	ctxt->options -= XML_PARSE_NONET;
        ret = xmlNoNetExternalEntityLoader(URL, ID, ctxt);
	ctxt->options = options;
	return(ret);
    }

    if (resource == NULL)
        resource = (xmlChar *) URL;

    if (resource == NULL) {
        if (ID == NULL)
            ID = "NULL";
        __xmlLoaderErr(ctxt, "failed to load external entity \"%s\"\n", ID);
        return (NULL);
    }
    ret = xmlNewInputFromFile(ctxt, (const char *) resource);
    if ((resource != NULL) && (resource != (xmlChar *) URL))
        xmlFree(resource);
    return (ret);
}

static xmlExternalEntityLoader xmlCurrentExternalEntityLoader =
       xmlDefaultExternalEntityLoader;

/**
 * xmlSetExternalEntityLoader:
 * @f:  the new entity resolver function
 *
 * Changes the defaultexternal entity resolver function for the application
 */
void
xmlSetExternalEntityLoader(xmlExternalEntityLoader f) {
    xmlCurrentExternalEntityLoader = f;
}

/**
 * xmlGetExternalEntityLoader:
 *
 * Get the default external entity resolver function for the application
 *
 * Returns the xmlExternalEntityLoader function pointer
 */
xmlExternalEntityLoader
xmlGetExternalEntityLoader(void) {
    return(xmlCurrentExternalEntityLoader);
}

/**
 * xmlLoadExternalEntity:
 * @URL:  the URL for the entity to load
 * @ID:  the Public ID for the entity to load
 * @ctxt:  the context in which the entity is called or NULL
 *
 * Load an external entity, note that the use of this function for
 * unparsed entities may generate problems
 *
 * Returns the xmlParserInputPtr or NULL
 */
xmlParserInputPtr
xmlLoadExternalEntity(const char *URL, const char *ID,
                      xmlParserCtxtPtr ctxt) {
    if ((URL != NULL) && (xmlNoNetExists(URL) == 0)) {
	char *canonicFilename;
	xmlParserInputPtr ret;

	canonicFilename = (char *) xmlCanonicPath((const xmlChar *) URL);
	if (canonicFilename == NULL) {
            xmlIOErrMemory("building canonical path\n");
	    return(NULL);
	}

	ret = xmlCurrentExternalEntityLoader(canonicFilename, ID, ctxt);
	xmlFree(canonicFilename);
	return(ret);
    }
    return(xmlCurrentExternalEntityLoader(URL, ID, ctxt));
}

/************************************************************************
 *									*
 *		Disabling Network access				*
 *									*
 ************************************************************************/

/**
 * xmlNoNetExternalEntityLoader:
 * @URL:  the URL for the entity to load
 * @ID:  the System ID for the entity to load
 * @ctxt:  the context in which the entity is called or NULL
 *
 * A specific entity loader disabling network accesses, though still
 * allowing local catalog accesses for resolution.
 *
 * Returns a new allocated xmlParserInputPtr, or NULL.
 */
xmlParserInputPtr
xmlNoNetExternalEntityLoader(const char *URL, const char *ID,
                             xmlParserCtxtPtr ctxt) {
    xmlParserInputPtr input = NULL;
    xmlChar *resource = NULL;

    if (resource == NULL)
	resource = (xmlChar *) URL;

    if (resource != NULL) {
        if ((!xmlStrncasecmp(BAD_CAST resource, BAD_CAST "ftp://", 6)) ||
            (!xmlStrncasecmp(BAD_CAST resource, BAD_CAST "http://", 7))) {
            xmlIOErr(XML_IO_NETWORK_ATTEMPT, (const char *) resource);
	    if (resource != (xmlChar *) URL)
		xmlFree(resource);
	    return(NULL);
	}
    }
    input = xmlDefaultExternalEntityLoader((const char *) resource, ID, ctxt);
    if (resource != (xmlChar *) URL)
	xmlFree(resource);
    return(input);
}

#define bottom_xmlIO
#include "elfgcchack.h"
