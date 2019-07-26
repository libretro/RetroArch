/* zutil.c -- target dependent utility functions for the compression library
 * Copyright (C) 1995-2005, 2010, 2011, 2012 Jean-loup Gailly.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

#include "zutil.h"
#ifndef Z_SOLO
#  include "gzguts.h"
#endif

char z_errmsg[10][21] = {
   "need dictionary",     /* Z_NEED_DICT       2  */
   "stream end",          /* Z_STREAM_END      1  */
   "",                    /* Z_OK              0  */
   "file error",          /* Z_ERRNO         (-1) */
   "stream error",        /* Z_STREAM_ERROR  (-2) */
   "data error",          /* Z_DATA_ERROR    (-3) */
   "insufficient memory", /* Z_MEM_ERROR     (-4) */
   "buffer error",        /* Z_BUF_ERROR     (-5) */
   "incompatible version",/* Z_VERSION_ERROR (-6) */
   ""};


const char * zlibVersion(void)
{
   return ZLIB_VERSION;
}

uLong zlibCompileFlags(void)
{
   uLong flags;

   flags = 0;
   switch ((int)(sizeof(uInt))) {
      case 2:     break;
      case 4:     flags += 1;     break;
      case 8:     flags += 2;     break;
      default:    flags += 3;
   }
   switch ((int)(sizeof(uLong))) {
      case 2:     break;
      case 4:     flags += 1 << 2;        break;
      case 8:     flags += 2 << 2;        break;
      default:    flags += 3 << 2;
   }
   switch ((int)(sizeof(voidpf))) {
      case 2:     break;
      case 4:     flags += 1 << 4;        break;
      case 8:     flags += 2 << 4;        break;
      default:    flags += 3 << 4;
   }
   switch ((int)(sizeof(z_off_t))) {
      case 2:     break;
      case 4:     flags += 1 << 6;        break;
      case 8:     flags += 2 << 6;        break;
      default:    flags += 3 << 6;
   }
#ifdef DEBUG
   flags += 1 << 8;
#endif
#if defined(ASMV) || defined(ASMINF)
   flags += 1 << 9;
#endif
#ifdef ZLIB_WINAPI
   flags += 1 << 10;
#endif
#ifdef BUILDFIXED
   flags += 1 << 12;
#endif
#ifdef DYNAMIC_CRC_TABLE
   flags += 1 << 13;
#endif
#ifdef NO_GZCOMPRESS
   flags += 1L << 16;
#endif
#ifdef NO_GZIP
   flags += 1L << 17;
#endif
#ifdef PKZIP_BUG_WORKAROUND
   flags += 1L << 20;
#endif
#ifdef FASTEST
   flags += 1L << 21;
#endif
#if defined(STDC) || defined(Z_HAVE_STDARG_H)
#  ifdef NO_vsnprintf
   flags += 1L << 25;
#    ifdef HAS_vsprintf_void
   flags += 1L << 26;
#    endif
#  else
#    ifdef HAS_vsnprintf_void
   flags += 1L << 26;
#    endif
#  endif
#else
   flags += 1L << 24;
#  ifdef NO_snprintf
   flags += 1L << 25;
#    ifdef HAS_sprintf_void
   flags += 1L << 26;
#    endif
#  else
#    ifdef HAS_snprintf_void
   flags += 1L << 26;
#    endif
#  endif
#endif
   return flags;
}

#ifdef DEBUG

#  ifndef verbose
#    define verbose 0
#  endif
int ZLIB_INTERNAL z_verbose = verbose;

void ZLIB_INTERNAL z_error (char *m)
{
   fprintf(stderr, "%s\n", m);
   exit(1);
}
#endif

/* exported to allow conversion of error code to string for compress() and
 * uncompress()
 */
const char * zError(int err)
{
   return ERR_MSG(err);
}

#if defined(_WIN32_WCE)
/* The Microsoft C Run-Time Library for Windows CE doesn't have
 * errno.  We define it as a global variable to simplify porting.
 * Its value is always 0 and should not be used.
 */
int errno = 0;
#endif

voidpf ZLIB_INTERNAL zcalloc (voidpf opaque, unsigned items, unsigned size)
{
   if (opaque) items += size - size; /* make compiler happy */
   return sizeof(uInt) > 2 ? (voidpf)malloc(items * size) :
      (voidpf)calloc(items, size);
}

void ZLIB_INTERNAL zcfree (voidpf opaque, voidpf ptr)
{
   free(ptr);
   if (opaque) return; /* make compiler happy */
}
