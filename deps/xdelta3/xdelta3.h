/* xdelta3 - delta compression tools and library
   Copyright 2016 Joshua MacDonald

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

/* To learn more about Xdelta, start by reading xdelta3.c.  If you are
 * ready to use the API, continue reading here.  There are two
 * interfaces -- xd3_encode_input and xd3_decode_input -- plus a dozen
 * or so related calls.  This interface is styled after Zlib. */

#ifndef _XDELTA3_H_
#define _XDELTA3_H_

#define _POSIX_SOURCE 200112L
#define _ISOC99_SOURCE
#define _C99_SOURCE

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/****************************************************************/

/* Default configured value of stream->winsize.  If the program
 * supplies xd3_encode_input() with data smaller than winsize the
 * stream will automatically buffer the input, otherwise the input
 * buffer is used directly.
 */
#ifndef XD3_DEFAULT_WINSIZE
#define XD3_DEFAULT_WINSIZE (1U << 23)
#endif

/* Default total size of the source window used in xdelta3-main.h */
#ifndef XD3_DEFAULT_SRCWINSZ
#define XD3_DEFAULT_SRCWINSZ (1U << 26)
#endif

/* When Xdelta requests a memory allocation for certain buffers, it
 * rounds up to units of at least this size.  The code assumes (and
 * asserts) that this is a power-of-two. */
#ifndef XD3_ALLOCSIZE
#define XD3_ALLOCSIZE (1U<<14)
#endif

/* The XD3_HARDMAXWINSIZE parameter is a safety mechanism to protect
 * decoders against malicious files.  The decoder will never decode a
 * window larger than this.  If the file specifies VCD_TARGET the
 * decoder may require two buffers of this size.
 *
 * 8-16MB is reasonable, probably don't need to go larger. */
#ifndef XD3_HARDMAXWINSIZE
#define XD3_HARDMAXWINSIZE (1U<<26)
#endif
/* The IOPT_SIZE value sets the size of a buffer used to batch
 * overlapping copy instructions before they are optimized by picking
 * the best non-overlapping ranges.  The larger this buffer, the
 * longer a forced xd3_srcwin_setup() decision is held off.  Setting
 * this value to 0 causes an unlimited buffer to be used. */
#ifndef XD3_DEFAULT_IOPT_SIZE
#define XD3_DEFAULT_IOPT_SIZE    (1U<<15)
#endif

/* The maximum distance backward to search for small matches */
#ifndef XD3_DEFAULT_SPREVSZ
#define XD3_DEFAULT_SPREVSZ (1U<<18)
#endif

/* The default compression level */
#ifndef XD3_DEFAULT_LEVEL
#define XD3_DEFAULT_LEVEL 3
#endif

#ifndef XD3_DEFAULT_SECONDARY_LEVEL
#define XD3_DEFAULT_SECONDARY_LEVEL 6
#endif

#ifndef XD3_USE_LARGEFILE64
#define XD3_USE_LARGEFILE64 1
#endif

/* The source window size is limited to 2GB unless
 * XD3_USE_LARGESIZET is defined to 1. */
#ifndef XD3_USE_LARGESIZET
#define XD3_USE_LARGESIZET 1
#endif

/* Sizes and addresses within VCDIFF windows are represented as usize_t
 *
 * For source-file offsets and total file sizes, total input and
 * output counts, the xoff_t type is used.  The decoder and encoder
 * generally check for overflow of the xoff_t size (this is tested at
 * the 32bit boundary [xdelta3-test.h]).
 */
#ifndef _WIN32
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdint.h>
#else /* WIN32 case */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef WINVER
#if XD3_USE_LARGEFILE64
/* 64 bit file offsets: uses GetFileSizeEx and SetFilePointerEx. */
#define WINVER		0x0500
#define _WIN32_WINNT	0x0500
#else /* xoff_t is 32bit */
/* 32 bit file offsets: uses GetFileSize and SetFilePointer. */
#define WINVER		0x0400
#define _WIN32_WINNT	0x0400
#endif /* if XD3_USE_LARGEFILE64 */
#endif /* ifndef WINVER */

#include <windows.h>

/* _MSV_VER is defined by Microsoft tools, not by Mingw32 */
#ifdef _MSC_VER
typedef signed int     ssize_t;
typedef int pid_t;
#if _MSC_VER < 1600
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long  uint32_t;
typedef ULONGLONG      uint64_t;
#else /* _MSC_VER >= 1600 */
/* For MSVC10 and above */
#include <stdint.h>
#define inline __inline
#endif /* _MSC_VER < 1600 */
#else /* _MSC_VER not defined  */
/* Mingw32 */
#include <stdint.h>
#endif /* _MSC_VER defined */

#endif /* _WIN32 defined */

/* Settings based on the size of xoff_t (32 vs 64 file offsets) */
#if XD3_USE_LARGEFILE64
/* xoff_t is a 64-bit type */
#define __USE_FILE_OFFSET64 1 /* GLIBC: for 64bit fileops. */

#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

static_assert(SIZEOF_SIZE_T == sizeof(size_t), "SIZEOF_SIZE_T not correctly set");
static_assert(SIZEOF_UNSIGNED_LONG_LONG == sizeof(unsigned long long), "SIZEOF_UNSIGNED_LONG_LONG not correctly set");

/* Set a xoff_t typedef and the "Q" printf insert. */
#if defined(_WIN32)
typedef uint64_t xoff_t;
#define Q "I64"
#elif SIZEOF_UNSIGNED_LONG == 8
typedef unsigned long xoff_t;
#define Q "l"
#elif SIZEOF_SIZE_T == 8
typedef size_t xoff_t;
#define Q "z"
#elif SIZEOF_UNSIGNED_LONG_LONG == 8
typedef unsigned long long xoff_t;
#define Q "ll"
#endif /* typedef and #define Q */

#define SIZEOF_XOFF_T 8

#else /* XD3_USE_LARGEFILE64 == 0 */

#if SIZEOF_UNSIGNED_INT == 4
typedef unsigned int xoff_t;
#elif SIZEOF_UNSIGNED_LONG == 4
typedef unsigned long xoff_t;
#else
typedef uint32_t xoff_t;
#endif /* xoff_t is 32 bits */

#define SIZEOF_XOFF_T 4
#define Q
#endif /* 64 vs 32 bit xoff_t */

/* Settings based on the size of usize_t (32 and 64 bit window size) */
#if XD3_USE_LARGESIZET

/* Set a usize_ttypedef and the "W" printf insert. */
#if defined(_WIN32)
typedef uint64_t usize_t;
#define W "I64"
#elif SIZEOF_UNSIGNED_LONG == 8
typedef unsigned long usize_t;
#define W "l"
#elif SIZEOF_SIZE_T == 8
typedef size_t usize_t;
#define W "z"
#elif SIZEOF_UNSIGNED_LONG_LONG == 8
typedef unsigned long long usize_t;
#define W "ll"
#endif /* typedef and #define W */

#define SIZEOF_USIZE_T 8

#else /* XD3_USE_LARGESIZET == 0 */

#if SIZEOF_UNSIGNED_INT == 4
typedef unsigned int usize_t;
#elif SIZEOF_UNSIGNED_LONG == 4
typedef unsigned long usize_t;
#else
typedef uint32_t usize_t;
#endif /* usize_t is 32 bits */

#define SIZEOF_USIZE_T 4
#define W

#endif /* 64 vs 32 bit usize_t */

/* Settings based on the size of size_t (the system-provided,
 * usually-but-maybe-not an unsigned type) */
#if SIZEOF_SIZE_T == 4
#define Z "z"
#elif SIZEOF_SIZE_T == 8
#ifdef _WIN32
#define Z "I64"
#else /* !_WIN32 */
#define Z "z"
#endif /* Windows or not */
#else
#error Bad configure script
#endif /* size_t printf flags */

#define USE_UINT32 (SIZEOF_USIZE_T == 4 || \
		    SIZEOF_XOFF_T == 4 || REGRESSION_TEST)
#define USE_UINT64 (SIZEOF_USIZE_T == 8 || \
		    SIZEOF_XOFF_T == 8 || REGRESSION_TEST)

#ifndef UNALIGNED_OK
#ifdef HAVE_ALIGNED_ACCESS_REQUIRED
#define UNALIGNED_OK 0
#else
/* This generally includes all Windows builds. */
#define UNALIGNED_OK 1
#endif
#endif

/**********************************************************************/

/* Whether to build the encoder, otherwise only build the decoder. */
#ifndef XD3_ENCODER
#define XD3_ENCODER 1
#endif

/* The code returned when main() fails, also defined in system
   includes. */
#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

/* REGRESSION TEST enables the "xdelta3 test" command, which runs a
   series of self-tests. */
#ifndef REGRESSION_TEST
#define REGRESSION_TEST 0
#endif

/* XD3_DEBUG=1 enables assertions and various statistics.  Levels > 1
 * enable some additional output only useful during development and
 * debugging. */
#ifndef XD3_DEBUG
#define XD3_DEBUG 0
#endif

#ifndef PYTHON_MODULE
#define PYTHON_MODULE 0
#endif

#ifndef SWIG_MODULE
#define SWIG_MODULE 0
#endif

#ifndef NOT_MAIN
#define NOT_MAIN 0
#endif

/* There are three string matching functions supplied: one fast, one
 * slow (default), and one soft-configurable.  To disable any of
 * these, use the following definitions. */
#ifndef XD3_BUILD_SLOW
#define XD3_BUILD_SLOW 1
#endif
#ifndef XD3_BUILD_FAST
#define XD3_BUILD_FAST 1
#endif
#ifndef XD3_BUILD_FASTER
#define XD3_BUILD_FASTER 1
#endif
#ifndef XD3_BUILD_FASTEST
#define XD3_BUILD_FASTEST 1
#endif
#ifndef XD3_BUILD_SOFT
#define XD3_BUILD_SOFT 1
#endif
#ifndef XD3_BUILD_DEFAULT
#define XD3_BUILD_DEFAULT 1
#endif

#if XD3_DEBUG
#include <stdio.h>
#endif

typedef struct _xd3_stream             xd3_stream;
typedef struct _xd3_source             xd3_source;
typedef struct _xd3_hash_cfg           xd3_hash_cfg;
typedef struct _xd3_smatcher           xd3_smatcher;
typedef struct _xd3_rinst              xd3_rinst;
typedef struct _xd3_dinst              xd3_dinst;
typedef struct _xd3_hinst              xd3_hinst;
typedef struct _xd3_winst              xd3_winst;
typedef struct _xd3_rpage              xd3_rpage;
typedef struct _xd3_addr_cache         xd3_addr_cache;
typedef struct _xd3_output             xd3_output;
typedef struct _xd3_desect             xd3_desect;
typedef struct _xd3_iopt_buflist       xd3_iopt_buflist;
typedef struct _xd3_rlist              xd3_rlist;
typedef struct _xd3_sec_type           xd3_sec_type;
typedef struct _xd3_sec_cfg            xd3_sec_cfg;
typedef struct _xd3_sec_stream         xd3_sec_stream;
typedef struct _xd3_config             xd3_config;
typedef struct _xd3_code_table_desc    xd3_code_table_desc;
typedef struct _xd3_code_table_sizes   xd3_code_table_sizes;
typedef struct _xd3_slist              xd3_slist;
typedef struct _xd3_whole_state        xd3_whole_state;
typedef struct _xd3_wininfo            xd3_wininfo;

/* The stream configuration has three callbacks functions, all of
 * which may be supplied with NULL values.  If config->getblk is
 * provided as NULL, the stream returns XD3_GETSRCBLK. */

typedef void*  (xd3_alloc_func)    (void       *opaque,
				    size_t      items,
				    usize_t     size);
typedef void   (xd3_free_func)     (void       *opaque,
				    void       *address);

typedef int    (xd3_getblk_func)   (xd3_stream *stream,
				    xd3_source *source,
				    xoff_t      blkno);

typedef const xd3_dinst* (xd3_code_table_func) (void);


#ifdef _WIN32
#define vsnprintf_func _vsnprintf
#define snprintf_func _snprintf
#else
#define vsnprintf_func vsnprintf
#define snprintf_func snprintf
#endif
#define short_sprintf(sb,fmt,...) \
  snprintf_func((sb).buf,sizeof((sb).buf),fmt,__VA_ARGS__)

/* Type used for short snprintf calls. */
typedef struct {
  char buf[48];
} shortbuf;

#ifndef PRINTF_ATTRIBUTE
#ifdef __GNUC__
#define PRINTF_ATTRIBUTE(x,y) __attribute__ ((__format__ (__printf__, x, y)))
#else
#define PRINTF_ATTRIBUTE(x,y)
#endif
#endif

/* Underlying xprintf() */
int xsnprintf_func (char *str, size_t n, const char *fmt, ...)
  PRINTF_ATTRIBUTE(3,4);

/* XPR(NT "", ...) (used by main) prefixes an "xdelta3: " to the output. */
void xprintf(const char *fmt, ...) PRINTF_ATTRIBUTE(1,2);
#define XPR xprintf
#define NT "xdelta3: "
#define NTR ""
/* DP(RINT ...) */
#define DP   xprintf
#define RINT ""

#if XD3_DEBUG
#define XD3_ASSERT(x)				     \
  do {						     \
    if (! (x)) {				     \
      DP(RINT "%s:%d: XD3 assertion failed: %s\n",   \
	 __FILE__, __LINE__, #x);		     \
      abort (); } } while (0)
#else
#define XD3_ASSERT(x) (void)0
#endif  /* XD3_DEBUG */

#define xd3_max(x,y) ((x) < (y) ? (y) : (x))
#define xd3_min(x,y) ((x) < (y) ? (x) : (y))

/****************************************************************
 PUBLIC ENUMS
 ******************************************************************/

/* These are the five ordinary status codes returned by the
 * xd3_encode_input() and xd3_decode_input() state machines. */
typedef enum {

  /* An application must be prepared to handle these five return
   * values from either xd3_encode_input or xd3_decode_input, except
   * in the case of no-source compression, in which case XD3_GETSRCBLK
   * is never returned.  More detailed comments for these are given in
   * xd3_encode_input and xd3_decode_input comments, below. */
  XD3_INPUT     = -17703, /* need input */
  XD3_OUTPUT    = -17704, /* have output */
  XD3_GETSRCBLK = -17705, /* need a block of source input (with no
			   * xd3_getblk function), a chance to do
			   * non-blocking read. */
  XD3_GOTHEADER = -17706, /* (decode-only) after the initial VCDIFF &
			     first window header */
  XD3_WINSTART  = -17707, /* notification: returned before a window is
			   * processed, giving a chance to
			   * XD3_SKIP_WINDOW or not XD3_SKIP_EMIT that
			   * window. */
  XD3_WINFINISH  = -17708, /* notification: returned after
			      encode/decode & output for a window */
  XD3_TOOFARBACK = -17709, /* (encoder only) may be returned by
			      getblk() if the block is too old */
  XD3_INTERNAL   = -17710, /* internal error */
  XD3_INVALID    = -17711, /* invalid config */
  XD3_INVALID_INPUT = -17712, /* invalid input/decoder error */
  XD3_NOSECOND    = -17713, /* when secondary compression finds no
			       improvement. */
  XD3_UNIMPLEMENTED = -17714  /* currently VCD_TARGET, VCD_CODETABLE */
} xd3_rvalues;

/* special values in config->flags */
typedef enum
{
  XD3_JUST_HDR       = (1 << 1),   /* used by VCDIFF tools, see
				      xdelta3-main.h. */
  XD3_SKIP_WINDOW    = (1 << 2),   /* used by VCDIFF tools, see
				      xdelta3-main.h. */
  XD3_SKIP_EMIT      = (1 << 3),   /* used by VCDIFF tools, see
				      xdelta3-main.h. */
  XD3_FLUSH          = (1 << 4),   /* flush the stream buffer to
				      prepare for
				      xd3_stream_close(). */

  XD3_SEC_DJW        = (1 << 5),   /* use DJW static huffman */
  XD3_SEC_FGK        = (1 << 6),   /* use FGK adaptive huffman */
  XD3_SEC_LZMA       = (1 << 24),  /* use LZMA secondary */

  XD3_SEC_TYPE       = (XD3_SEC_DJW | XD3_SEC_FGK | XD3_SEC_LZMA),

  XD3_SEC_NODATA     = (1 << 7),   /* disable secondary compression of
				      the data section. */
  XD3_SEC_NOINST     = (1 << 8),   /* disable secondary compression of
				      the inst section. */
  XD3_SEC_NOADDR     = (1 << 9),   /* disable secondary compression of
				      the addr section. */

  XD3_SEC_NOALL      = (XD3_SEC_NODATA | XD3_SEC_NOINST | XD3_SEC_NOADDR),

  XD3_ADLER32        = (1 << 10),  /* enable checksum computation in
				      the encoder. */
  XD3_ADLER32_NOVER  = (1 << 11),  /* disable checksum verification in
				      the decoder. */

  XD3_NOCOMPRESS     = (1 << 13),  /* disable ordinary data
				    * compression feature, only search
				    * the source, not the target. */
  XD3_BEGREEDY       = (1 << 14),  /* disable the "1.5-pass
				    * algorithm", instead use greedy
				    * matching.  Greedy is off by
				    * default. */
  XD3_ADLER32_RECODE = (1 << 15),  /* used by "recode". */

  /* 4 bits to set the compression level the same as the command-line
   * setting -1 through -9 (-0 corresponds to the XD3_NOCOMPRESS flag,
   * and is independent of compression level).  This is for
   * convenience, especially with xd3_encode_memory(). */

  XD3_COMPLEVEL_SHIFT = 20,  /* 20 - 23 */
  XD3_COMPLEVEL_MASK = (0xF << XD3_COMPLEVEL_SHIFT),
  XD3_COMPLEVEL_1 = (1 << XD3_COMPLEVEL_SHIFT),
  XD3_COMPLEVEL_2 = (2 << XD3_COMPLEVEL_SHIFT),
  XD3_COMPLEVEL_3 = (3 << XD3_COMPLEVEL_SHIFT),
  XD3_COMPLEVEL_6 = (6 << XD3_COMPLEVEL_SHIFT),
  XD3_COMPLEVEL_9 = (9 << XD3_COMPLEVEL_SHIFT)

} xd3_flags;

/* The values of this enumeration are set in xd3_config using the
 * smatch_cfg variable.  It can be set to default, slow, fast, etc.,
 * and soft. */
typedef enum
{
  XD3_SMATCH_DEFAULT = 0, /* Flags may contain XD3_COMPLEVEL bits,
			     else default. */
  XD3_SMATCH_SLOW    = 1,
  XD3_SMATCH_FAST    = 2,
  XD3_SMATCH_FASTER  = 3,
  XD3_SMATCH_FASTEST = 4,
  XD3_SMATCH_SOFT    = 5
} xd3_smatch_cfg;

/*********************************************************************
 PRIVATE ENUMS
**********************************************************************/

/* stream->match_state is part of the xd3_encode_input state machine
 *  for source matching:
 *
 *  1. the XD3_GETSRCBLK block-read mechanism means reentrant matching
 *  2. this state spans encoder windows: a match and end-of-window
 *  will continue in the next 3. the initial target byte and source
 *  byte are a presumed match, to avoid some computation in case the
 *  inputs are identical.
 */
typedef enum {

  MATCH_TARGET    = 0, /* in this state, attempt to match the start of
			* the target with the previously set source
			* address (initially 0). */
  MATCH_BACKWARD  = 1, /* currently expanding a match backward in the
			  source/target. */
  MATCH_FORWARD   = 2, /* currently expanding a match forward in the
			  source/target. */
  MATCH_SEARCHING = 3  /* currently searching for a match. */

} xd3_match_state;

/* The xd3_encode_input state machine steps through these states in
 * the following order.  The matcher is reentrant and returns
 * XD3_INPUT whenever it requires more data.  After receiving
 * XD3_INPUT, if the application reads EOF it should call
 * xd3_stream_close().
 */
typedef enum {

  ENC_INIT      = 0, /* xd3_encode_input has never been called. */
  ENC_INPUT     = 1, /* waiting for xd3_avail_input () to be called. */
  ENC_SEARCH    = 2, /* currently searching for matches. */
  ENC_INSTR     = 3, /* currently formatting output. */
  ENC_FLUSH     = 4, /* currently emitting output. */
  ENC_POSTOUT   = 5, /* after an output section. */
  ENC_POSTWIN   = 6, /* after all output sections. */
  ENC_ABORTED   = 7  /* abort. */
} xd3_encode_state;

/* The xd3_decode_input state machine steps through these states in
 * the following order.  The matcher is reentrant and returns
 * XD3_INPUT whenever it requires more data.  After receiving
 * XD3_INPUT, if the application reads EOF it should call
 * xd3_stream_close().
 *
 * 0-8:   the VCDIFF header
 * 9-18:  the VCDIFF window header
 * 19-21: the three primary sections: data, inst, addr
 * 22:    producing output: returns XD3_OUTPUT, possibly XD3_GETSRCBLK,
 * 23:    return XD3_WINFINISH, set state=9 to decode more input
 */
typedef enum {

  DEC_VCHEAD   = 0, /* VCDIFF header */
  DEC_HDRIND   = 1, /* header indicator */

  DEC_SECONDID = 2, /* secondary compressor ID */

  DEC_TABLEN   = 3, /* code table length */
  DEC_NEAR     = 4, /* code table near */
  DEC_SAME     = 5, /* code table same */
  DEC_TABDAT   = 6, /* code table data */

  DEC_APPLEN   = 7, /* application data length */
  DEC_APPDAT   = 8, /* application data */

  DEC_WININD   = 9, /* window indicator */

  DEC_CPYLEN   = 10, /* copy window length */
  DEC_CPYOFF   = 11, /* copy window offset */

  DEC_ENCLEN   = 12, /* length of delta encoding */
  DEC_TGTLEN   = 13, /* length of target window */
  DEC_DELIND   = 14, /* delta indicator */

  DEC_DATALEN  = 15, /* length of ADD+RUN data */
  DEC_INSTLEN  = 16, /* length of instruction data */
  DEC_ADDRLEN  = 17, /* length of address data */

  DEC_CKSUM    = 18, /* window checksum */

  DEC_DATA     = 19, /* data section */
  DEC_INST     = 20, /* instruction section */
  DEC_ADDR     = 21, /* address section */

  DEC_EMIT     = 22, /* producing data */

  DEC_FINISH   = 23, /* window finished */

  DEC_ABORTED  = 24  /* xd3_abort_stream */
} xd3_decode_state;

/************************************************************
 internal types
 ************************************************************/

/* instruction lists used in the IOPT buffer */
struct _xd3_rlist
{
  xd3_rlist  *next;
  xd3_rlist  *prev;
};

/* the raw encoding of an instruction used in the IOPT buffer */
struct _xd3_rinst
{
  uint8_t     type;
  uint8_t     xtra;
  uint8_t     code1;
  uint8_t     code2;
  usize_t      pos;
  usize_t      size;
  xoff_t      addr;
  xd3_rlist   link;
};

/* the code-table form of an single- or double-instruction */
struct _xd3_dinst
{
  uint8_t     type1;
  uint8_t     size1;
  uint8_t     type2;
  uint8_t     size2;
};

/* the decoded form of a single (half) instruction. */
struct _xd3_hinst
{
  uint8_t    type;
  usize_t    size;
  usize_t    addr;
};

/* the form of a whole-file instruction */
struct _xd3_winst
{
  uint8_t type;  /* RUN, ADD, COPY */
  uint8_t mode;  /* 0, VCD_SOURCE, VCD_TARGET */
  usize_t size;
  xoff_t  addr;
  xoff_t  position;  /* absolute position of this inst */
};

/* used by the encoder to buffer output in sections.  list of blocks. */
struct _xd3_output
{
  uint8_t    *base;
  usize_t     next;
  usize_t     avail;
  xd3_output *next_page;
};

/* used by the decoder to buffer input in sections. */
struct _xd3_desect
{
  const uint8_t *buf;
  const uint8_t *buf_max;
  usize_t        size;
  usize_t        pos;

  /* used in xdelta3-decode.h */
  uint8_t       *copied1;
  usize_t        alloc1;

  /* used in xdelta3-second.h */
  uint8_t       *copied2;
  usize_t        alloc2;
};

/* the VCDIFF address cache, see the RFC */
struct _xd3_addr_cache
{
  usize_t  s_near;
  usize_t  s_same;
  usize_t  next_slot;  /* the circular index for near */
  usize_t *near_array; /* array of size s_near        */
  usize_t *same_array; /* array of size s_same*256    */
};

/* the IOPT buffer list is just a list of buffers, which may be allocated
 * during encode when using an unlimited buffer. */
struct _xd3_iopt_buflist
{
  xd3_rinst *buffer;
  xd3_iopt_buflist *next;
};

/* This is the record of a pre-compiled configuration, a subset of
   xd3_config. */
struct _xd3_smatcher
{
  const char        *name;
  int             (*string_match) (xd3_stream  *stream);
  usize_t            large_look;
  usize_t            large_step;
  usize_t            small_look;
  usize_t            small_chain;
  usize_t            small_lchain;
  usize_t            max_lazy;
  usize_t            long_enough;
};

/* hash table size & power-of-two hash function. */
struct _xd3_hash_cfg
{
  usize_t  size;       // Number of buckets
  usize_t  shift;
  usize_t  mask;
  usize_t  look;       // How wide is this checksum
  usize_t  multiplier; // K * powers[0]
  usize_t *powers;     // Array of [0,look) where powers[look-1] == 1
                       // and powers[N] = powers[N+1]*K (Rabin-Karp)
};

/* the sprev list */
struct _xd3_slist
{
  usize_t     last_pos;
};

/* window info (for whole state) */
struct _xd3_wininfo {
  xoff_t offset;
  usize_t length;
  uint32_t adler32;
};

/* whole state for, e.g., merge */
struct _xd3_whole_state {
  usize_t addslen;
  uint8_t *adds;
  usize_t  adds_alloc;

  usize_t instlen;
  xd3_winst *inst;
  usize_t  inst_alloc;

  usize_t wininfolen;
  xd3_wininfo *wininfo;
  usize_t wininfo_alloc;

  xoff_t length;
};

/********************************************************************
 public types
 *******************************************************************/

/* Settings for the secondary compressor. */
struct _xd3_sec_cfg
{
  int                data_type;     /* Which section. (set automatically) */
  usize_t            ngroups;       /* Number of DJW Huffman groups. */
  usize_t            sector_size;   /* Sector size. */
  int                inefficient;   /* If true, ignore efficiency check [avoid XD3_NOSECOND]. */
};

/* This is the user-visible stream configuration. */
struct _xd3_config
{
  usize_t             winsize;       /* The encoder window size. */
  usize_t             sprevsz;       /* How far back small string
					matching goes */
  usize_t             iopt_size;     /* entries in the
					instruction-optimizing
					buffer */

  xd3_getblk_func   *getblk;        /* The three callbacks. */
  xd3_alloc_func    *alloc;
  xd3_free_func     *freef;
  void              *opaque;        /* Not used. */
  uint32_t           flags;         /* stream->flags are initialized
				     * from xd3_config & never
				     * modified by the library.  Use
				     * xd3_set_flags to modify flags
				     * settings mid-stream. */

  xd3_sec_cfg       sec_data;       /* Secondary compressor config: data */
  xd3_sec_cfg       sec_inst;       /* Secondary compressor config: inst */
  xd3_sec_cfg       sec_addr;       /* Secondary compressor config: addr */

  xd3_smatch_cfg     smatch_cfg;    /* See enum: use fields below  for
				       soft config */
  xd3_smatcher       smatcher_soft;
};

/* The primary source file object. You create one of these objects and
 * initialize the first four fields.  This library maintains the next
 * 5 fields.  The configured getblk implementation is responsible for
 * setting the final 3 fields when called (and/or when XD3_GETSRCBLK
 * is returned).
 */
struct _xd3_source
{
  /* you set */
  usize_t             blksize;       /* block size */
  const char         *name;          /* its name, for debug/print
					purposes */
  void               *ioh;           /* opaque handle */
  xoff_t              max_winsize;   /* maximum visible buffer */

  /* getblk sets */
  xoff_t              curblkno;      /* current block number: client
					sets after getblk request */
  usize_t             onblk;         /* number of bytes on current
					block: client sets,  must be >= 0
				        and <= blksize */
  const uint8_t      *curblk;        /* current block array: client
					sets after getblk request */

  /* xd3 sets */
  usize_t             srclen;        /* length of this source window */
  xoff_t              srcbase;       /* offset of this source window
					in the source itself */
  usize_t             shiftby;       /* for power-of-two blocksizes */
  usize_t             maskby;        /* for power-of-two blocksizes */
  xoff_t              cpyoff_blocks; /* offset of dec_cpyoff in blocks */
  usize_t             cpyoff_blkoff; /* offset of copy window in
					blocks, remainder */
  xoff_t              getblkno;      /* request block number: xd3 sets
					current getblk request */

  /* See xd3_getblk() */
  xoff_t              max_blkno;  /* Maximum block, if eof is known,
				   * otherwise, equals frontier_blkno
				   * (initially 0). */
  usize_t             onlastblk;  /* Number of bytes on max_blkno */
  int                 eof_known;  /* Set to true when the first
				   * partial block is read. */
};

/* The primary xd3_stream object, used for encoding and decoding.  You
 * may access only two fields: avail_out, next_out.  Use the methods
 * above to operate on xd3_stream. */
struct _xd3_stream
{
  /* input state */
  const uint8_t    *next_in;          /* next input byte */
  usize_t           avail_in;         /* number of bytes available at
					 next_in */
  xoff_t            total_in;         /* how many bytes in */

  /* output state */
  uint8_t          *next_out;         /* next output byte */
  usize_t           avail_out;        /* number of bytes available at
					 next_out */
  usize_t           space_out;        /* total out space */
  xoff_t            current_window;   /* number of windows encoded/decoded */
  xoff_t            total_out;        /* how many bytes out */

  /* to indicate an error, xd3 sets */
  const char       *msg;              /* last error message, NULL if
					 no error */

  /* source configuration */
  xd3_source       *src;              /* source array */

  /* encoder memory configuration */
  usize_t           winsize;          /* suggested window size */
  usize_t           sprevsz;          /* small string, previous window
					 size (power of 2) */
  usize_t           sprevmask;        /* small string, previous window
					 size mask */
  usize_t           iopt_size;
  usize_t           iopt_unlimited;

  /* general configuration */
  xd3_getblk_func  *getblk;           /* set nxtblk, nxtblkno to scanblkno */
  xd3_alloc_func   *alloc;            /* malloc function */
  xd3_free_func    *free;             /* free function */
  void*             opaque;           /* private data object passed to
					 alloc, free, and getblk */
  uint32_t          flags;            /* various options */

  /* secondary compressor configuration */
  xd3_sec_cfg       sec_data;         /* Secondary compressor config: data */
  xd3_sec_cfg       sec_inst;         /* Secondary compressor config: inst */
  xd3_sec_cfg       sec_addr;         /* Secondary compressor config: addr */

  xd3_smatcher      smatcher;

  usize_t           *large_table;      /* table of large checksums */
  xd3_hash_cfg       large_hash;       /* large hash config */

  usize_t           *small_table;      /* table of small checksums */
  xd3_slist         *small_prev;       /* table of previous offsets,
					  circular linked list */
  int                small_reset;      /* true if small table should
					  be reset */

  xd3_hash_cfg       small_hash;       /* small hash config */
  xd3_addr_cache     acache;           /* the vcdiff address cache */
  xd3_encode_state   enc_state;        /* state of the encoder */

  usize_t            taroff;           /* base offset of the target input */
  usize_t            input_position;   /* current input position */
  usize_t            min_match;        /* current minimum match
					  length, avoids redundent
					  matches */
  usize_t            unencoded_offset; /* current input, first
				       * unencoded offset. this value
				       * is <= the first instruction's
				       * position in the iopt buffer,
				       * if there is at least one
				       * match in the buffer. */

  /* SRCWIN */
  int                srcwin_decided;    /* boolean: true if srclen and
					   srcbase have been
					   decided. */
  int                srcwin_decided_early;  /* boolean: true if srclen
					       and srcbase were
					       decided early. */
  xoff_t             srcwin_cksum_pos;  /* Source checksum position */

  /* MATCH */
  xd3_match_state    match_state;      /* encoder match state */
  xoff_t             match_srcpos;     /* current match source
					  position relative to
					  srcbase */
  xoff_t             match_last_srcpos;  /* previously attempted
					  * srcpos, to avoid loops. */
  xoff_t             match_minaddr;    /* smallest matching address to
				       * set window params (reset each
				       * window xd3_encode_reset) */
  xoff_t             match_maxaddr;    /* largest matching address to
				       * set window params (reset each
				       * window xd3_encode_reset) */
  usize_t            match_back;       /* match extends back so far */
  usize_t            match_maxback;    /* match extends back maximum */
  usize_t            match_fwd;        /* match extends forward so far */
  usize_t            match_maxfwd;     /* match extends forward maximum */

  xoff_t             maxsrcaddr;      /* address of the last source
					 match (across windows) */

  uint8_t          *buf_in;           /* for saving buffered input */
  usize_t           buf_avail;        /* amount of saved input */
  const uint8_t    *buf_leftover;     /* leftover content of next_in
					 (i.e., user's buffer) */
  usize_t            buf_leftavail;    /* amount of leftover content */

  xd3_output       *enc_current;      /* current output buffer */
  xd3_output       *enc_free;         /* free output buffers */
  xd3_output       *enc_heads[4];     /* array of encoded outputs:
					 head of chain */
  xd3_output       *enc_tails[4];     /* array of encoded outputs:
					 tail of chain */
  uint32_t          recode_adler32;   /* set the adler32 checksum
				       * during "recode". */

  xd3_rlist         iopt_used;        /* instruction optimizing buffer */
  xd3_rlist         iopt_free;
  xd3_rinst        *iout;             /* next single instruction */
  xd3_iopt_buflist *iopt_alloc;

  const uint8_t    *enc_appheader;    /* application header to encode */
  usize_t            enc_appheadsz;    /* application header size */

  /* decoder stuff */
  xd3_decode_state  dec_state;        /* current DEC_XXX value */
  usize_t           dec_hdr_ind;      /* VCDIFF header indicator */
  usize_t           dec_win_ind;      /* VCDIFF window indicator */
  usize_t           dec_del_ind;      /* VCDIFF delta indicator */

  uint8_t           dec_magic[4];     /* First four bytes */
  usize_t           dec_magicbytes;   /* Magic position. */

  usize_t           dec_secondid;     /* Optional secondary compressor ID. */

  usize_t           dec_codetblsz;    /* Optional code table: length. */
  uint8_t          *dec_codetbl;      /* Optional code table: storage. */
  usize_t           dec_codetblbytes; /* Optional code table: position. */

  usize_t           dec_appheadsz;    /* Optional application header:
					 size. */
  uint8_t          *dec_appheader;    /* Optional application header:
					 storage */
  usize_t           dec_appheadbytes; /* Optional application header:
					 position. */

  usize_t            dec_cksumbytes;   /* Optional checksum: position. */
  uint8_t           dec_cksum[4];     /* Optional checksum: storage. */
  uint32_t          dec_adler32;      /* Optional checksum: value. */

  usize_t            dec_cpylen;       /* length of copy window
					  (VCD_SOURCE or VCD_TARGET) */
  xoff_t             dec_cpyoff;       /* offset of copy window
					  (VCD_SOURCE or VCD_TARGET) */
  usize_t            dec_enclen;       /* length of delta encoding */
  usize_t            dec_tgtlen;       /* length of target window */

#if USE_UINT64
  uint64_t          dec_64part;       /* part of a decoded uint64_t */
#endif
#if USE_UINT32
  uint32_t          dec_32part;       /* part of a decoded uint32_t */
#endif

  xoff_t            dec_winstart;     /* offset of the start of
                                         current target window */
  xoff_t            dec_window_count; /* == current_window + 1 in
                                         DEC_FINISH */
  usize_t            dec_winbytes;     /* bytes of the three sections
                                          so far consumed */
  usize_t            dec_hdrsize;      /* VCDIFF + app header size */

  const uint8_t    *dec_tgtaddrbase;  /* Base of decoded target
                                         addresses (addr >=
                                         dec_cpylen). */
  const uint8_t    *dec_cpyaddrbase;  /* Base of decoded copy
                                         addresses (addr <
                                         dec_cpylen). */

  usize_t            dec_position;     /* current decoder position
                                          counting the cpylen
                                          offset */
  usize_t            dec_maxpos;       /* maximum decoder position
                                          counting the cpylen
                                          offset */
  xd3_hinst         dec_current1;     /* current instruction */
  xd3_hinst         dec_current2;     /* current instruction */

  uint8_t          *dec_buffer;       /* Decode buffer */
  uint8_t          *dec_lastwin;      /* In case of VCD_TARGET, the
                                         last target window. */
  usize_t            dec_lastlen;      /* length of the last target
                                          window */
  xoff_t            dec_laststart;    /* offset of the start of last
                                         target window */
  usize_t            dec_lastspace;    /* allocated space of last
                                          target window, for reuse */

  xd3_desect        inst_sect;        /* staging area for decoding
                                         window sections */
  xd3_desect        addr_sect;
  xd3_desect        data_sect;

  xd3_code_table_func       *code_table_func;
  const xd3_dinst           *code_table;
  const xd3_code_table_desc *code_table_desc;
  xd3_dinst                 *code_table_alloc;

  /* secondary compression */
  const xd3_sec_type *sec_type;
  xd3_sec_stream     *sec_stream_d;
  xd3_sec_stream     *sec_stream_i;
  xd3_sec_stream     *sec_stream_a;

  /* state for reconstructing whole files (e.g., for merge), this only
   * supports loading USIZE_T_MAX instructions, adds, etc. */
  xd3_whole_state     whole_target;

  /* statistics */
  xoff_t            n_scpy;
  xoff_t            n_tcpy;
  xoff_t            n_add;
  xoff_t            n_run;

  xoff_t            l_scpy;
  xoff_t            l_tcpy;
  xoff_t            l_add;
  xoff_t            l_run;

  usize_t           i_slots_used;

#if XD3_DEBUG
  usize_t            large_ckcnt;

  /* memory usage */
  usize_t            alloc_cnt;
  usize_t            free_cnt;
#endif
};

/**************************************************************************
 PUBLIC FUNCTIONS
 **************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus


/* This function configures an xd3_stream using the provided in-memory
 * input buffer, source buffer, output buffer, and flags.  The output
 * array must be large enough or else ENOSPC will be returned.  This
 * is the simplest in-memory encoding interface. */
int     xd3_encode_memory (const uint8_t *input,
			   usize_t        input_size,
			   const uint8_t *source,
			   usize_t        source_size,
			   uint8_t       *output_buffer,
			   usize_t       *output_size,
			   usize_t        avail_output,
			   int            flags);

/* The reverse of xd3_encode_memory. */
int     xd3_decode_memory (const uint8_t *input,
			   usize_t        input_size,
			   const uint8_t *source,
			   usize_t        source_size,
			   uint8_t       *output_buf,
			   usize_t       *output_size,
			   usize_t        avail_output,
			   int            flags);

/* This function encodes an in-memory input using a pre-configured
 * xd3_stream.  This allows the caller to set a variety of options
 * which are not available in the xd3_encode/decode_memory()
 * functions.
 *
 * The output array must be large enough to hold the output or else
 * ENOSPC is returned.  The source (if any) should be set using
 * xd3_set_source_and_size() with a single-block xd3_source.  This
 * calls the underlying non-blocking interfaces,
 * xd3_encode/decode_input(), handling the necessary input/output
 * states.  This method may be considered a reference for any
 * application using xd3_encode_input() directly.
 *
 *   xd3_stream stream;
 *   xd3_config config;
 *   xd3_source src;
 *
 *   memset (& src, 0, sizeof (src));
 *   memset (& stream, 0, sizeof (stream));
 *   memset (& config, 0, sizeof (config));
 *
 *   if (source != NULL)
 *     {
 *       src.size = source_size;
 *       src.blksize = source_size;
 *       src.curblkno = 0;
 *       src.onblk = source_size;
 *       src.curblk = source;
 *       src.max_winsize = source_size;
 *       xd3_set_source(&stream, &src);
 *     }
 *
 *   config.flags = flags;
 *   config.winsize = input_size;
 *
 *   ... set smatcher, appheader, encoding-table, compression-level, etc.
 *
 *   xd3_config_stream(&stream, &config);
 *   xd3_encode_stream(&stream, ...);
 *   xd3_free_stream(&stream);
 */
int     xd3_encode_stream (xd3_stream    *stream,
			   const uint8_t *input,
			   usize_t         input_size,
			   uint8_t       *output,
			   usize_t        *output_size,
			   usize_t         avail_output);

/* The reverse of xd3_encode_stream. */
int     xd3_decode_stream (xd3_stream    *stream,
			   const uint8_t *input,
			   usize_t        input_size,
			   uint8_t       *output,
			   usize_t       *output_size,
			   usize_t        avail_size);

/* This is the non-blocking interface.
 *
 * Handling input and output states is the same for encoding or
 * decoding using the xd3_avail_input() and xd3_consume_output()
 * routines, inlined below.
 *
 * Return values:
 *
 *   XD3_INPUT: the process requires more input: call
 *               xd3_avail_input() then repeat
 *
 *   XD3_OUTPUT: the process has more output: read stream->next_out,
 *               stream->avail_out, then call xd3_consume_output(),
 *               then repeat
 *
 *   XD3_GOTHEADER: (decoder-only) notification returned following the
 *               VCDIFF header and first window header.  the decoder
 *               may use the header to configure itself.
 *
 *   XD3_WINSTART: a general notification returned once for each
 *               window except the 0-th window, which is implied by
 *               XD3_GOTHEADER.  It is recommended to use a
 *               switch-stmt such as:
 *
 *                 ...
 *               again:
 *                 switch ((ret = xd3_decode_input (stream))) {
 *                    case XD3_GOTHEADER: {
 *                      assert(stream->current_window == 0);
 *                      stuff;
 *                    }
 *                    // fallthrough
 *                    case XD3_WINSTART: {
 *                      something(stream->current_window);
 *                      goto again;
 *                    }
 *                    ...
 *
 *   XD3_WINFINISH: a general notification, following the complete
 *               input & output of a window.  at this point,
 *               stream->total_in and stream->total_out are consistent
 *               for either encoding or decoding.
 *
 *   XD3_GETSRCBLK: If the xd3_getblk() callback is NULL, this value
 *               is returned to initiate a non-blocking source read.
 */
int     xd3_decode_input  (xd3_stream    *stream);
int     xd3_encode_input  (xd3_stream    *stream);

/* The xd3_config structure is used to initialize a stream - all data
 * is copied into stream so config may be a temporary variable.  See
 * the [documentation] or comments on the xd3_config structure. */
int     xd3_config_stream (xd3_stream    *stream,
			   xd3_config    *config);

/* Since Xdelta3 doesn't open any files, xd3_close_stream is just an
 * error check that the stream is in a proper state to be closed: this
 * means the encoder is flushed and the decoder is at a window
 * boundary.  The application is responsible for freeing any of the
 * resources it supplied. */
int     xd3_close_stream (xd3_stream    *stream);

/* This arranges for closes the stream to succeed.  Does not free the
 * stream.*/
void    xd3_abort_stream (xd3_stream    *stream);

/* xd3_free_stream frees all memory allocated for the stream.  The
 * application is responsible for freeing any of the resources it
 * supplied. */
void    xd3_free_stream   (xd3_stream    *stream);

/* This function informs the encoder or decoder that source matching
 * (i.e., delta-compression) is possible.  For encoding, this should
 * be called before the first xd3_encode_input.  A NULL source is
 * ignored.  For decoding, this should be called before the first
 * window is decoded, but the appheader may be read first
 * (XD3_GOTHEADER).  After decoding the header, call xd3_set_source()
 * if you have a source file.  Note: if (stream->dec_win_ind & VCD_SOURCE)
 * is true, it means the first window expects there to be a source file.
 */
int     xd3_set_source    (xd3_stream    *stream,
			   xd3_source    *source);

/* If the source size is known, call this instead of xd3_set_source().
 * to avoid having stream->getblk called (and/or to avoid XD3_GETSRCBLK).
 *
 * Follow these steps:
  xd3_source source;
  memset(&source, 0, sizeof(source));
  source.blksize  = size;
  source.onblk    = size;
  source.curblk   = buf;
  source.curblkno = 0;
  int ret = xd3_set_source_and_size(&stream, &source, size);
  ...
 */
int     xd3_set_source_and_size (xd3_stream    *stream,
				 xd3_source    *source,
				 xoff_t         source_size);

/* This should be called before the first call to xd3_encode_input()
 * to include application-specific data in the VCDIFF header. */
void    xd3_set_appheader (xd3_stream    *stream,
			   const uint8_t *data,
			   usize_t        size);

/* xd3_get_appheader may be called in the decoder after XD3_GOTHEADER.
 * For convenience, the decoder always adds a single byte padding to
 * the end of the application header, which is set to zero in case the
 * application header is a string. */
int     xd3_get_appheader (xd3_stream     *stream,
			   uint8_t       **data,
			   usize_t        *size);

/* To generate a VCDIFF encoded delta with xd3_encode_init() from
 * another format, use:
 *
 *   xd3_encode_init_partial() -- initialze encoder state (w/o hash tables)
 *   xd3_init_cache() -- reset VCDIFF address cache
 *   xd3_found_match() -- to report a copy instruction
 *
 * set stream->enc_state to ENC_INSTR and call xd3_encode_input as usual.
 */
int xd3_encode_init_partial (xd3_stream *stream);
void xd3_init_cache (xd3_addr_cache* acache);
int xd3_found_match (xd3_stream *stream,
		     usize_t pos, usize_t size,
		     xoff_t addr, int is_source);

/* Gives an error string for xdelta3-speficic errors, returns NULL for
   system errors */
const char* xd3_strerror (int ret);

/* For convenience, zero & initialize the xd3_config structure with
   specified flags. */
static inline
void    xd3_init_config (xd3_config *config,
			 uint32_t    flags)
{
  memset (config, 0, sizeof (*config));
  config->flags = flags;
}

/* This supplies some input to the stream.
 *
 * For encoding, if the input is larger than the configured window
 * size (xd3_config.winsize), the entire input will be consumed and
 * encoded anyway.  If you wish to strictly limit the window size,
 * limit the buffer passed to xd3_avail_input to the window size.
 *
 * For encoding, if the input is smaller than the configured window
 * size (xd3_config.winsize), the library will create a window-sized
 * buffer and accumulate input until a full-sized window can be
 * encoded.  XD3_INPUT will be returned.  The input must remain valid
 * until the next time xd3_encode_input() returns XD3_INPUT.
 *
 * For decoding, the input will be consumed entirely before XD3_INPUT
 * is returned again.
 */
static inline
void    xd3_avail_input  (xd3_stream    *stream,
			  const uint8_t *idata,
			  usize_t         isize)
{
  /* Even if isize is zero, the code expects a non-NULL idata.  Why?
   * It uses this value to determine whether xd3_avail_input has ever
   * been called.  If xd3_encode_input is called before
   * xd3_avail_input it will return XD3_INPUT right away without
   * allocating a stream->winsize buffer.  This is to avoid an
   * unwanted allocation. */
  XD3_ASSERT (idata != NULL || isize == 0);

  stream->next_in  = idata;
  stream->avail_in = isize;
}

/* This acknowledges receipt of output data, must be called after any
 * XD3_OUTPUT return. */
static inline
void xd3_consume_output (xd3_stream  *stream)
{
  stream->avail_out  = 0;
}

/* These are set for each XD3_WINFINISH return. */
static inline
int xd3_encoder_used_source (xd3_stream *stream) {
  return stream->src != NULL && stream->src->srclen > 0;
}
static inline
xoff_t xd3_encoder_srcbase (xd3_stream *stream) {
  return stream->src->srcbase;
}
static inline
usize_t xd3_encoder_srclen (xd3_stream *stream) {
  return stream->src->srclen;
}

/* Checks for legal flag changes. */
static inline
void xd3_set_flags (xd3_stream *stream, uint32_t flags)
{
  /* The bitwise difference should contain only XD3_FLUSH or
     XD3_SKIP_WINDOW */
  XD3_ASSERT(((flags ^ stream->flags) & ~(XD3_FLUSH | XD3_SKIP_WINDOW)) == 0);
  stream->flags = flags;
}

/* Gives some extra information about the latest library error, if any
 * is known. */
static inline
const char* xd3_errstring (xd3_stream  *stream)
{
  return stream->msg ? stream->msg : "";
}


/* 64-bit divisions are expensive, which is why we require a
 * power-of-two source->blksize.  To relax this restriction is
 * relatively easy, see the history for xd3_blksize_div(). */
static inline
void xd3_blksize_div (const xoff_t offset,
		      const xd3_source *source,
		      xoff_t *blkno,
		      usize_t *blkoff) {
  *blkno = offset >> source->shiftby;
  *blkoff = offset & source->maskby;
  XD3_ASSERT (*blkoff < source->blksize);
}

static inline
void xd3_blksize_add (xoff_t *blkno,
		      usize_t *blkoff,
		      const xd3_source *source,
		      const usize_t add)
{
  usize_t blkdiff;

  /* Does not check for overflow, checked in xdelta3-decode.h. */
  *blkoff += add;
  blkdiff = *blkoff >> source->shiftby;

  if (blkdiff)
    {
      *blkno += blkdiff;
      *blkoff &= source->maskby;
    }

  XD3_ASSERT (*blkoff < source->blksize);
}

#ifdef __cplusplus
} // extern "C"
#endif

#define XD3_NOOP 0U
#define XD3_ADD 1U
#define  XD3_RUN 2U
#define  XD3_CPY 3U /* XD3_CPY rtypes are represented as (XD3_CPY +
                     * copy-mode value) */

#if XD3_DEBUG
#define IF_DEBUG(x) x
#else
#define IF_DEBUG(x)
#endif
#if XD3_DEBUG > 1
#define IF_DEBUG1(x) x
#else
#define IF_DEBUG1(x)
#endif
#if XD3_DEBUG > 2
#define IF_DEBUG2(x) x
#else
#define IF_DEBUG2(x)
#endif

#define SIZEOF_ARRAY(x) (sizeof(x) / sizeof(x[0]))

#endif /* _XDELTA3_H_ */
