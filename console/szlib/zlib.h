/* zlib.h -- interface of the 'zlib' general purpose compression library
  version 1.1.4, March 11th, 2002

  Copyright (C) 1995-2002 Jean-loup Gailly and Mark Adler

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Jean-loup Gailly        Mark Adler
  jloup@gzip.org          madler@alumni.caltech.edu


  The data format used by the zlib library is described by RFCs (Request for
  Comments) 1950 to 1952 in the files ftp://ds.internic.net/rfc/rfc1950.txt
  (zlib format), rfc1951.txt (deflate format) and rfc1952.txt (gzip format).
*/

/* This source as presented is a modified version of original zlib for use with SSNES,
 * and must not be confused with the original software. */

#ifndef _ZLIB_H
#define _ZLIB_H

#include "zconf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZLIB_VERSION "1.1.4"

typedef voidpf (*alloc_func) (voidpf opaque, unsigned int items, unsigned int size);
typedef void   (*free_func)  (voidpf opaque, voidpf address);

struct internal_state;

typedef struct z_stream_s {
    Bytef    *next_in;  /* next input byte */
    unsigned int     avail_in;  /* number of bytes available at next_in */
    unsigned long    total_in;  /* total nb of input bytes read so far */

    Bytef    *next_out; /* next output byte should be put there */
    unsigned int     avail_out; /* remaining free space at next_out */
    unsigned long    total_out; /* total nb of bytes output so far */

    char     *msg;      /* last error message, NULL if no error */
    struct internal_state *state; /* not visible by applications */

    alloc_func zalloc;  /* used to allocate the internal state */
    free_func  zfree;   /* used to free the internal state */
    voidpf     opaque;  /* private data object passed to zalloc and zfree */

    int     data_type;  /* best guess about the data type: ascii or binary */
    unsigned long   adler;      /* adler32 value of the uncompressed data */
    unsigned long   reserved;   /* reserved for future use */
} z_stream;

typedef z_stream *z_streamp;

/* constants */

#define Z_NO_FLUSH      0
#define Z_PARTIAL_FLUSH 1 /* will be removed, use Z_SYNC_FLUSH instead */
#define Z_SYNC_FLUSH    2
#define Z_FULL_FLUSH    3
#define Z_FINISH        4
/* Allowed flush values; see deflate() below for details */

#define Z_OK            0
#define Z_STREAM_END    1
#define Z_NEED_DICT     2
#define Z_ERRNO        (-1)
#define Z_STREAM_ERROR (-2)
#define Z_DATA_ERROR   (-3)
#define Z_MEM_ERROR    (-4)
#define Z_BUF_ERROR    (-5)
#define Z_VERSION_ERROR (-6)
/* Return codes for the compression/decompression functions. Negative
 * values are errors, positive values are used for special but normal events.
 */

#define Z_NO_COMPRESSION         0
#define Z_BEST_SPEED             1
#define Z_BEST_COMPRESSION       9
#define Z_DEFAULT_COMPRESSION  (-1)
/* compression levels */

#define Z_FILTERED            1
#define Z_HUFFMAN_ONLY        2
#define Z_DEFAULT_STRATEGY    0

#define Z_BINARY   0
#define Z_ASCII    1
#define Z_UNKNOWN  2
/* Possible values of the data_type field */

#define Z_DEFLATED   8
/* The deflate compression method (the only one supported in this version) */

#define Z_NULL  0  /* for initializing zalloc, zfree, opaque */

#define zlib_version zlibVersion()
/* for compatibility with versions < 1.0.2 */

// basic functions

typedef voidp gzFile;

extern const char *  zlibVersion (void);

extern int  inflate (z_streamp strm, int flush);

extern int  inflateEnd (z_streamp strm);

// The following functions are needed only in some special applications.

extern int  inflateReset (z_streamp strm);

// utility functions

extern gzFile	gzopen  (const char *path, const char *mode);
extern gzFile	gzdopen  (int fd, const char *mode);
extern int	gzread  (gzFile file, voidp buf, unsigned len);
extern int	gzwrite (gzFile file, const voidp buf, unsigned len);
extern int	gzprintf (gzFile file, const char *format, ...);
extern int	gzputs (gzFile file, const char *s);
extern char *	gzgets (gzFile file, char *buf, int len);
extern int	gzputc (gzFile file, int c);
extern int	gzgetc (gzFile file);
extern int	gzflush (gzFile file, int flush);
extern z_off_t	gzseek (gzFile file, z_off_t offset, int whence);
extern int     gzrewind (gzFile file);
extern z_off_t     gztell (gzFile file);
extern int  gzeof (gzFile file);
extern int     gzclose (gzFile file);
extern const char *  gzerror (gzFile file, int *errnum);

/* checksum functions */

extern unsigned long  adler32 (unsigned long adler, const Bytef *buf, unsigned int len);
extern unsigned long  crc32   (unsigned long crc, const Bytef *buf, unsigned int len);

/* various hacks, don't look :) */

extern int  inflateInit_ (z_streamp strm, const char *version, int stream_size);
extern int  inflateInit2_ (z_streamp strm, int  windowBits, const char *version, int stream_size);
#define inflateInit(strm) \
        inflateInit_((strm),                ZLIB_VERSION, sizeof(z_stream))
#define inflateInit2(strm, windowBits) \
        inflateInit2_((strm), (windowBits), ZLIB_VERSION, sizeof(z_stream))

extern const char   *  zError           (int err);

#ifdef __cplusplus
}
#endif

#endif /* _ZLIB_H */
