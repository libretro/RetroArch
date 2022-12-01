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

   -------------------------------------------------------------------

			       Xdelta 3

   The goal of this library is to to implement both the (stand-alone)
   data-compression and delta-compression aspects of VCDIFF encoding, and
   to support a programming interface that works like Zlib
   (http://www.gzip.org/zlib.html). See RFC3284: The VCDIFF Generic
   Differencing and Compression Data Format.

   VCDIFF is a unified encoding that combines data-compression and
   delta-encoding ("differencing").

   VCDIFF has a detailed byte-code instruction set with many features.
   The instruction format supports an immediate size operand for small
   COPYs and ADDs (e.g., under 18 bytes).  There are also instruction
   "modes", which are used to compress COPY addresses by using two
   address caches.  An instruction mode refers to slots in the NEAR
   and SAME caches for recent addresses.  NEAR remembers the
   previous 4 (by default) COPY addresses, and SAME catches
   frequent re-uses of the same address using a 3-way (by default)
   256-entry associative cache of [ADDR mod 256], the encoded byte.
   A hit in the NEAR/SAME cache requires 0/1 ADDR bytes.

   VCDIFF has a default instruction table, but an alternate
   instruction tables may themselves be be delta-compressed and
   included in the encoding header.  This allows even more freedom.
   There are 9 instruction modes in the default code table, 4 near, 3
   same, VCD_SELF (absolute encoding) and VCD_HERE (relative to the
   current position).

   ----------------------------------------------------------------------

  			      Algorithms

   Aside from the details of encoding and decoding, there are a bunch
   of algorithms needed.

   1. STRING-MATCH.  A two-level fingerprinting approach is used.  A
   single loop computes the two checksums -- small and large -- at
   successive offsets in the TARGET file.  The large checksum is more
   accurate and is used to discover SOURCE matches, which are
   potentially very long.  The small checksum is used to discover
   copies within the TARGET.  Small matching, which is more expensive,
   usually dominates the large STRING-MATCH costs in this code - the
   more exhaustive the search, the better the results.  Either of the
   two string-matching mechanisms may be disabled.

   2. INSTRUCTION SELECTION.  The IOPT buffer here represents a queue
   used to store overlapping copy instructions.  There are two possible
   optimizations that go beyond a greedy search.  Both of these fall
   into the category of "non-greedy matching" optimizations.

   The first optimization stems from backward SOURCE-COPY matching.
   When a new SOURCE-COPY instruction covers a previous instruction in
   the target completely, it is erased from the queue.  Randal Burns
   originally analyzed these algorithms and did a lot of related work
   (\cite the 1.5-pass algorithm).

   The second optimization comes by the encoding of common very-small
   COPY and ADD instructions, for which there are special DOUBLE-code
   instructions, which code two instructions in a single byte.

   The cost of bad instruction-selection overhead is relatively high
   for data-compression, relative to delta-compression, so this second
   optimization is fairly important.  With "lazy" matching (the name
   used in Zlib for a similar optimization), the string-match
   algorithm searches after a match for potential overlapping copy
   instructions.  In Xdelta and by default, VCDIFF, the minimum match
   size is 4 bytes, whereas Zlib searches with a 3-byte minimum.  This
   feature, combined with double instructions, provides a nice
   challenge.  Search in this file for "black magic", a heuristic.

   3. STREAM ALIGNMENT.  Stream alignment is needed to compress large
   inputs in constant space.  See xd3_srcwin_move_point().

   4. WINDOW SELECTION.  When the IOPT buffer flushes, in the first call
   to xd3_iopt_finish_encoding containing any kind of copy instruction,
   the parameters of the source window must be decided: the offset into
   the source and the length of the window.  Since the IOPT buffer is
   finite, the program may be forced to fix these values before knowing
   the best offset/length.

   5. SECONDARY COMPRESSION.  VCDIFF supports a secondary encoding to
   be applied to the individual sections of the data format, which are
   ADDRess, INSTruction, and DATA.  Several secondary compressor
   variations are implemented here, although none is standardized yet.

   One is an adaptive huffman algorithm -- the FGK algorithm (Faller,
   Gallager, and Knuth, 1985).  This compressor is extremely slow.

   The other is a simple static Huffman routine, which is the base
   case of a semi-adaptive scheme published by D.J. Wheeler and first
   widely used in bzip2 (by Julian Seward).  This is a very
   interesting algorithm, originally published in nearly cryptic form
   by D.J. Wheeler. !!!NOTE!!! Because these are not standardized,
   secondary compression remains off by default.
   ftp://ftp.cl.cam.ac.uk/users/djw3/bred3.{c,ps}
   --------------------------------------------------------------------

			    Other Features

   1. USER CONVENIENCE

   For user convenience, it is essential to recognize Gzip-compressed
   files and automatically Gzip-decompress them prior to
   delta-compression (or else no delta-compression will be achieved
   unless the user manually decompresses the inputs).  The compressed
   represention competes with Xdelta, and this must be hidden from the
   command-line user interface.  The Xdelta-1.x encoding was simple, not
   compressed itself, so Xdelta-1.x uses Zlib internally to compress the
   representation.

   This implementation supports external compression, which implements
   the necessary fork() and pipe() mechanics.  There is a tricky step
   involved to support automatic detection of a compressed input in a
   non-seekable input.  First you read a bit of the input to detect
   magic headers.  When a compressed format is recognized, exec() the
   external compression program and create a second child process to
   copy the original input stream. [Footnote: There is a difficulty
   related to using Gzip externally. It is not possible to decompress
   and recompress a Gzip file transparently.  If FILE.GZ had a
   cryptographic signature, then, after: (1) Gzip-decompression, (2)
   Xdelta-encoding, (3) Gzip-compression the signature could be
   broken.  The only way to solve this problem is to guess at Gzip's
   compression level or control it by other means.  I recommend that
   specific implementations of any compression scheme store
   information needed to exactly re-compress the input, that way
   external compression is transparent - however, this won't happen
   here until it has stabilized.]

   2. APPLICATION-HEADER

   This feature was introduced in RFC3284.  It allows any application
   to include a header within the VCDIFF file format.  This allows
   general inter-application data exchange with support for
   application-specific extensions to communicate metadata.

   3. VCDIFF CHECKSUM

   An optional checksum value is included with each window, which can
   be used to validate the final result.  This verifies the correct source
   file was used for decompression as well as the obvious advantage:
   checking the implementation (and underlying) correctness.

   4. LIGHT WEIGHT

   The code makes efforts to avoid copying data more than necessary.
   The code delays many initialization tasks until the first use, it
   optimizes for identical (perfectly matching) inputs.  It does not
   compute any checksums until the first lookup misses.  Memory usage
   is reduced.  String-matching is templatized (by slightly gross use
   of CPP) to hard-code alternative compile-time defaults.  The code
   has few outside dependencies.
   ----------------------------------------------------------------------

		The default rfc3284 instruction table:
		    (see RFC for the explanation)

           TYPE      SIZE     MODE    TYPE     SIZE     MODE     INDEX
   --------------------------------------------------------------------
       1.  Run         0        0     Noop       0        0        0
       2.  Add    0, [1,17]     0     Noop       0        0      [1,18]
       3.  Copy   0, [4,18]     0     Noop       0        0     [19,34]
       4.  Copy   0, [4,18]     1     Noop       0        0     [35,50]
       5.  Copy   0, [4,18]     2     Noop       0        0     [51,66]
       6.  Copy   0, [4,18]     3     Noop       0        0     [67,82]
       7.  Copy   0, [4,18]     4     Noop       0        0     [83,98]
       8.  Copy   0, [4,18]     5     Noop       0        0     [99,114]
       9.  Copy   0, [4,18]     6     Noop       0        0    [115,130]
      10.  Copy   0, [4,18]     7     Noop       0        0    [131,146]
      11.  Copy   0, [4,18]     8     Noop       0        0    [147,162]
      12.  Add       [1,4]      0     Copy     [4,6]      0    [163,174]
      13.  Add       [1,4]      0     Copy     [4,6]      1    [175,186]
      14.  Add       [1,4]      0     Copy     [4,6]      2    [187,198]
      15.  Add       [1,4]      0     Copy     [4,6]      3    [199,210]
      16.  Add       [1,4]      0     Copy     [4,6]      4    [211,222]
      17.  Add       [1,4]      0     Copy     [4,6]      5    [223,234]
      18.  Add       [1,4]      0     Copy       4        6    [235,238]
      19.  Add       [1,4]      0     Copy       4        7    [239,242]
      20.  Add       [1,4]      0     Copy       4        8    [243,246]
      21.  Copy        4      [0,8]   Add        1        0    [247,255]
   --------------------------------------------------------------------

		     Reading the source: Overview

   This file includes itself in several passes to macro-expand certain
   sections with variable forms.  Just read ahead, there's only a
   little confusion.  I know this sounds ugly, but hard-coding some of
   the string-matching parameters results in a 10-15% increase in
   string-match performance.  The only time this hurts is when you have
   unbalanced #if/endifs.

   A single compilation unit tames the Makefile.  In short, this is to
   allow the above-described hack without an explodingMakefile.  The
   single compilation unit includes the core library features,
   configurable string-match templates, optional main() command-line
   tool, misc optional features, and a regression test.  Features are
   controled with CPP #defines, see Makefile.am.

   The initial __XDELTA3_C_HEADER_PASS__ starts first, the _INLINE_ and
   _TEMPLATE_ sections follow.  Easy stuff first, hard stuff last.

   Optional features include:

     xdelta3-main.h     The command-line interface, external compression
                        support, POSIX-specific, info & VCDIFF-debug tools.
     xdelta3-second.h   The common secondary compression routines.
     xdelta3-decoder.h  All decoding routines.
     xdelta3-djw.h      The semi-adaptive huffman secondary encoder.
     xdelta3-fgk.h      The adaptive huffman secondary encoder.
     xdelta3-test.h     The unit test covers major algorithms,
                        encoding and decoding.  There are single-bit
                        error decoding tests.  There are 32/64-bit file size
                        boundary tests.  There are command-line tests.
                        There are compression tests.  There are external
                        compression tests.  There are string-matching tests.
			There should be more tests...

   Additional headers include:

     xdelta3.h          The public header file.
     xdelta3-cfgs.h     The default settings for default, built-in
                        encoders.  These are hard-coded at
                        compile-time.  There is also a single
                        soft-coded string matcher for experimenting
                        with arbitrary values.
     xdelta3-list.h     A cyclic list template

   Misc little debug utilities:

     badcopy.c          Randomly modifies an input file based on two
                        parameters: (1) the probability that a byte in
                        the file is replaced with a pseudo-random value,
                        and (2) the mean change size.  Changes are
                        generated using an expoential distribution
                        which approximates the expected error_prob
			distribution.
   --------------------------------------------------------------------

   This file itself is unusually large.  I hope to defend this layout
   with lots of comments.  Everything in this file is related to
   encoding and decoding.  I like it all together - the template stuff
   is just a hack. */

#ifndef __XDELTA3_C_HEADER_PASS__
#define __XDELTA3_C_HEADER_PASS__

#include "xdelta3.h"
#include "xdelta3-internal.h"

/***********************************************************************
 STATIC CONFIGURATION
 ***********************************************************************/

#ifndef XD3_MAIN                  /* the main application */
#define XD3_MAIN 0
#endif

#ifndef VCDIFF_TOOLS
#define VCDIFF_TOOLS XD3_MAIN
#endif

#ifndef SECONDARY_FGK    /* one from the algorithm preservation department: */
#define SECONDARY_FGK 0  /* adaptive Huffman routines */
#endif

#ifndef SECONDARY_DJW    /* semi-adaptive/static Huffman for the eventual */
#define SECONDARY_DJW 0  /* standardization, off by default until such time. */
#endif

#ifndef SECONDARY_LZMA
#ifdef HAVE_LZMA_H
#define SECONDARY_LZMA 1
#else
#define SECONDARY_LZMA 0
#endif
#endif

#if XD3_ENCODER
#define IF_ENCODER(x) x
#else
#define IF_ENCODER(x)
#endif

/***********************************************************************/

  /* header indicator bits */
#define VCD_SECONDARY (1U << 0)  /* uses secondary compressor */
#define VCD_CODETABLE (1U << 1)  /* supplies code table data */
#define VCD_APPHEADER (1U << 2)  /* supplies application data */
#define VCD_INVHDR    (~0x7U)

  /* window indicator bits */
#define VCD_SOURCE   (1U << 0)  /* copy window in source file */
#define VCD_TARGET   (1U << 1)  /* copy window in target file */
#define VCD_ADLER32  (1U << 2)  /* has adler32 checksum */
#define VCD_INVWIN   (~0x7U)

#define VCD_SRCORTGT (VCD_SOURCE | VCD_TARGET)

  /* delta indicator bits */
#define VCD_DATACOMP (1U << 0)
#define VCD_INSTCOMP (1U << 1)
#define VCD_ADDRCOMP (1U << 2)
#define VCD_INVDEL   (~0x7U)

typedef enum {
  VCD_DJW_ID    = 1,
  VCD_LZMA_ID   = 2,
  VCD_FGK_ID    = 16  /* Note: these are not standard IANA-allocated IDs! */
} xd3_secondary_ids;

typedef enum {
  SEC_NOFLAGS     = 0,

  /* Note: SEC_COUNT_FREQS Not implemented (to eliminate 1st Huffman pass) */
  SEC_COUNT_FREQS = (1 << 0)
} xd3_secondary_flags;

typedef enum {
  DATA_SECTION, /* These indicate which section to the secondary
                 * compressor. */
  INST_SECTION, /* The header section is not compressed, therefore not
                 * listed here. */
  ADDR_SECTION
} xd3_section_type;

typedef unsigned int xd3_rtype;

/***********************************************************************/

#include "xdelta3-list.h"

#if XD3_ENCODER
XD3_MAKELIST(xd3_rlist, xd3_rinst, link);
#endif

/***********************************************************************/

#define SECONDARY_MIN_SAVINGS 2  /* Secondary compression has to save
				    at least this many bytes. */
#define SECONDARY_MIN_INPUT   10 /* Secondary compression needs at
				    least this many bytes. */

#define VCDIFF_MAGIC1  0xd6  /* 1st file byte */
#define VCDIFF_MAGIC2  0xc3  /* 2nd file byte */
#define VCDIFF_MAGIC3  0xc4  /* 3rd file byte */
#define VCDIFF_VERSION 0x00  /* 4th file byte */

#define VCD_SELF       0     /* 1st address mode */
#define VCD_HERE       1     /* 2nd address mode */

#define SECONDARY_ANY (SECONDARY_DJW || SECONDARY_FGK || SECONDARY_LZMA)

#define ALPHABET_SIZE      256  /* Used in test code--size of the secondary
				 * compressor alphabet. */

#define HASH_CKOFFSET      1U   /* Table entries distinguish "no-entry" from
				 * offset 0 using this offset. */

#define MAX_MATCH_SPLIT   18U   /* VCDIFF code table: 18 is the default limit
				 * for direct-coded ADD sizes */

#define LEAST_MATCH_INCR  0   /* The least number of bytes an overlapping
			       * match must beat the preceding match by.  This
			       * is a bias for the lazy match optimization.  A
			       * non-zero value means that an adjacent match
			       * has to be better by more than the step
			       * between them.  0. */

#define MIN_MATCH         4U  /* VCDIFF code table: MIN_MATCH=4 */
#define MIN_RUN           8U  /* The shortest run, if it is shorter than this
			       * an immediate add/copy will be just as good.
			       * ADD1/COPY6 = 1I+1D+1A bytes, RUN18 =
			       * 1I+1D+1A. */

#define MAX_MODES         9  /* Maximum number of nodes used for
			      * compression--does not limit decompression. */

#define ENC_SECTS         4  /* Number of separate output sections. */

#define HDR_TAIL(s)  ((s)->enc_tails[0])
#define DATA_TAIL(s) ((s)->enc_tails[1])
#define INST_TAIL(s) ((s)->enc_tails[2])
#define ADDR_TAIL(s) ((s)->enc_tails[3])

#define HDR_HEAD(s)  ((s)->enc_heads[0])
#define DATA_HEAD(s) ((s)->enc_heads[1])
#define INST_HEAD(s) ((s)->enc_heads[2])
#define ADDR_HEAD(s) ((s)->enc_heads[3])

/* Template instances. */
#if XD3_BUILD_SLOW
#define IF_BUILD_SLOW(x) x
#else
#define IF_BUILD_SLOW(x)
#endif
#if XD3_BUILD_FAST
#define IF_BUILD_FAST(x) x
#else
#define IF_BUILD_FAST(x)
#endif
#if XD3_BUILD_FASTER
#define IF_BUILD_FASTER(x) x
#else
#define IF_BUILD_FASTER(x)
#endif
#if XD3_BUILD_FASTEST
#define IF_BUILD_FASTEST(x) x
#else
#define IF_BUILD_FASTEST(x)
#endif
#if XD3_BUILD_SOFT
#define IF_BUILD_SOFT(x) x
#else
#define IF_BUILD_SOFT(x)
#endif
#if XD3_BUILD_DEFAULT
#define IF_BUILD_DEFAULT(x) x
#else
#define IF_BUILD_DEFAULT(x)
#endif

/* Update the run-length state */
#define NEXTRUN(c) do { if ((c) == run_c) { run_l += 1; } \
  else { run_c = (c); run_l = 1; } } while (0)

/* This CPP-conditional stuff can be cleaned up... */
#if REGRESSION_TEST
#define IF_REGRESSION(x) x
#else
#define IF_REGRESSION(x)
#endif

/***********************************************************************/

#if XD3_ENCODER
static void*       xd3_alloc0 (xd3_stream *stream,
			       usize_t      elts,
			       usize_t      size);


static int         xd3_alloc_iopt (xd3_stream *stream, usize_t elts);

static void        xd3_free_output (xd3_stream *stream,
				    xd3_output *output);

static int         xd3_emit_double (xd3_stream *stream, xd3_rinst *first,
				    xd3_rinst *second, uint8_t code);
static int         xd3_emit_single (xd3_stream *stream, xd3_rinst *single,
				    uint8_t code);

static usize_t      xd3_sizeof_output (xd3_output *output);
static void        xd3_encode_reset (xd3_stream *stream);

static int         xd3_source_match_setup (xd3_stream *stream, xoff_t srcpos);
static int         xd3_source_extend_match (xd3_stream *stream);
static int         xd3_srcwin_setup (xd3_stream *stream);
static usize_t     xd3_iopt_last_matched (xd3_stream *stream);
static int         xd3_emit_uint32_t (xd3_stream *stream, xd3_output **output,
				      uint32_t num);

static usize_t xd3_smatch (xd3_stream *stream,
			   usize_t base,
			   usize_t scksum,
			   usize_t *match_offset);
static int xd3_string_match_init (xd3_stream *stream);
static uint32_t xd3_scksum (uint32_t *state, const uint8_t *seg,
			    const usize_t ln);
static usize_t xd3_comprun (const uint8_t *seg, usize_t slook, uint8_t *run_cp);
static int xd3_srcwin_move_point (xd3_stream *stream,
				  usize_t *next_move_point);

static int xd3_emit_run (xd3_stream *stream, usize_t pos,
			 usize_t size, uint8_t *run_c);
static xoff_t xd3_source_cksum_offset(xd3_stream *stream, usize_t low);
static void xd3_scksum_insert (xd3_stream *stream,
			       usize_t inx,
			       usize_t scksum,
			       usize_t pos);


#if XD3_DEBUG
static void xd3_verify_run_state (xd3_stream    *stream,
				  const uint8_t *inp,
				  usize_t        x_run_l,
				  uint8_t       *x_run_c);
static void xd3_verify_large_state (xd3_stream *stream,
				    const uint8_t *inp,
				    usize_t x_cksum);
static void xd3_verify_small_state (xd3_stream    *stream,
				    const uint8_t *inp,
				    uint32_t       x_cksum);

#endif /* XD3_DEBUG */
#endif /* XD3_ENCODER */

static int         xd3_decode_allocate (xd3_stream *stream, usize_t size,
					uint8_t **copied1, usize_t *alloc1);

static void*       xd3_alloc (xd3_stream *stream, usize_t elts, usize_t size);
static void        xd3_free  (xd3_stream *stream, void *ptr);

const char* xd3_strerror (int ret)
{
  switch (ret)
    {
    case XD3_INPUT: return "XD3_INPUT";
    case XD3_OUTPUT: return "XD3_OUTPUT";
    case XD3_GETSRCBLK: return "XD3_GETSRCBLK";
    case XD3_GOTHEADER: return "XD3_GOTHEADER";
    case XD3_WINSTART: return "XD3_WINSTART";
    case XD3_WINFINISH: return "XD3_WINFINISH";
    case XD3_TOOFARBACK: return "XD3_TOOFARBACK";
    case XD3_INTERNAL: return "XD3_INTERNAL";
    case XD3_INVALID: return "XD3_INVALID";
    case XD3_INVALID_INPUT: return "XD3_INVALID_INPUT";
    case XD3_NOSECOND: return "XD3_NOSECOND";
    case XD3_UNIMPLEMENTED: return "XD3_UNIMPLEMENTED";
    }
  return NULL;
}

/***********************************************************************/

#define xd3_sec_data(s) ((s)->sec_stream_d)
#define xd3_sec_inst(s) ((s)->sec_stream_i)
#define xd3_sec_addr(s) ((s)->sec_stream_a)

struct _xd3_sec_type
{
  uint8_t       id;
  const char *name;
  xd3_secondary_flags flags;

  /* xd3_sec_stream is opaque to the generic code */
  xd3_sec_stream* (*alloc)   (xd3_stream     *stream);
  void            (*destroy) (xd3_stream     *stream,
			      xd3_sec_stream *sec);
  int             (*init)    (xd3_stream     *stream,
			      xd3_sec_stream *sec_stream,
			      int             is_encode);
  int             (*decode)  (xd3_stream     *stream,
			      xd3_sec_stream *sec_stream,
			      const uint8_t **input,
			      const uint8_t  *input_end,
			      uint8_t       **output,
			      const uint8_t  *output_end);
#if XD3_ENCODER
  int             (*encode)  (xd3_stream     *stream,
			      xd3_sec_stream *sec_stream,
			      xd3_output     *input,
			      xd3_output     *output,
			      xd3_sec_cfg    *cfg);
#endif
};

#define BIT_STATE_ENCODE_INIT { 0, 1 }
#define BIT_STATE_DECODE_INIT { 0, 0x100 }

typedef struct _bit_state bit_state;
struct _bit_state
{
  uint8_t cur_byte;
  usize_t cur_mask;
};

#if SECONDARY_ANY == 0
#define IF_SEC(x)
#define IF_NSEC(x) x
#else /* yuck */
#define IF_SEC(x) x
#define IF_NSEC(x)
static int
xd3_decode_secondary (xd3_stream      *stream,
		      xd3_desect      *sect,
		      xd3_sec_stream **sec_streamp);
#if XD3_ENCODER
static int
xd3_encode_secondary (xd3_stream      *stream,
		      xd3_output     **head,
		      xd3_output     **tail,
		      xd3_sec_stream **sec_streamp,
		      xd3_sec_cfg     *cfg,
		      int             *did_it);
#endif
#endif /* SECONDARY_ANY */

#if SECONDARY_FGK
extern const xd3_sec_type fgk_sec_type;
#define IF_FGK(x) x
#define FGK_CASE(s) \
  s->sec_type = & fgk_sec_type; \
  break;
#else
#define IF_FGK(x)
#define FGK_CASE(s) \
  s->msg = "unavailable secondary compressor: FGK Adaptive Huffman"; \
  return XD3_INTERNAL;
#endif

#if SECONDARY_DJW
extern const xd3_sec_type djw_sec_type;
#define IF_DJW(x) x
#define DJW_CASE(s) \
  s->sec_type = & djw_sec_type; \
  break;
#else
#define IF_DJW(x)
#define DJW_CASE(s) \
  s->msg = "unavailable secondary compressor: DJW Static Huffman"; \
  return XD3_INTERNAL;
#endif

#if SECONDARY_LZMA
extern const xd3_sec_type lzma_sec_type;
#define IF_LZMA(x) x
#define LZMA_CASE(s) \
  s->sec_type = & lzma_sec_type; \
  break;
#else
#define IF_LZMA(x)
#define LZMA_CASE(s) \
  s->msg = "unavailable secondary compressor: LZMA"; \
  return XD3_INTERNAL;
#endif

/***********************************************************************/

#include "xdelta3-hash.h"

/* Process template passes - this includes xdelta3.c several times. */
#define __XDELTA3_C_TEMPLATE_PASS__
#include "xdelta3-cfgs.h"
#undef __XDELTA3_C_TEMPLATE_PASS__

/* Process the inline pass. */
#define __XDELTA3_C_INLINE_PASS__
#include "xdelta3.c"
#undef __XDELTA3_C_INLINE_PASS__

/* Secondary compression */
#if SECONDARY_ANY
#include "xdelta3-second.h"
#endif

#if SECONDARY_FGK
#include "xdelta3-fgk.h"
const xd3_sec_type fgk_sec_type =
{
  VCD_FGK_ID,
  "FGK Adaptive Huffman",
  SEC_NOFLAGS,
  (xd3_sec_stream* (*)(xd3_stream*)) fgk_alloc,
  (void (*)(xd3_stream*, xd3_sec_stream*)) fgk_destroy,
  (int (*)(xd3_stream*, xd3_sec_stream*, int)) fgk_init,
  (int (*)(xd3_stream*, xd3_sec_stream*, const uint8_t**, const uint8_t*,
	   uint8_t**, const uint8_t*)) xd3_decode_fgk,
  IF_ENCODER((int (*)(xd3_stream*, xd3_sec_stream*, xd3_output*,
		      xd3_output*, xd3_sec_cfg*))   xd3_encode_fgk)
};
#endif

#if SECONDARY_DJW
#include "xdelta3-djw.h"
const xd3_sec_type djw_sec_type =
{
  VCD_DJW_ID,
  "Static Huffman",
  SEC_COUNT_FREQS,
  (xd3_sec_stream* (*)(xd3_stream*)) djw_alloc,
  (void (*)(xd3_stream*, xd3_sec_stream*)) djw_destroy,
  (int (*)(xd3_stream*, xd3_sec_stream*, int)) djw_init,
  (int (*)(xd3_stream*, xd3_sec_stream*, const uint8_t**, const uint8_t*,
	   uint8_t**, const uint8_t*)) xd3_decode_huff,
  IF_ENCODER((int (*)(xd3_stream*, xd3_sec_stream*, xd3_output*,
		      xd3_output*, xd3_sec_cfg*))   xd3_encode_huff)
};
#endif

#if SECONDARY_LZMA
#include "xdelta3-lzma.h"
const xd3_sec_type lzma_sec_type =
{
  VCD_LZMA_ID,
  "lzma",
  SEC_NOFLAGS,
  (xd3_sec_stream* (*)(xd3_stream*)) xd3_lzma_alloc,
  (void (*)(xd3_stream*, xd3_sec_stream*)) xd3_lzma_destroy,
  (int (*)(xd3_stream*, xd3_sec_stream*, int)) xd3_lzma_init,
  (int (*)(xd3_stream*, xd3_sec_stream*, const uint8_t**, const uint8_t*,
	   uint8_t**, const uint8_t*)) xd3_decode_lzma,
  IF_ENCODER((int (*)(xd3_stream*, xd3_sec_stream*, xd3_output*,
		      xd3_output*, xd3_sec_cfg*))   xd3_encode_lzma)
};
#endif

#if XD3_MAIN || PYTHON_MODULE || SWIG_MODULE || NOT_MAIN
#include "xdelta3-main.h"
#endif

#if REGRESSION_TEST
#include "xdelta3-test.h"
#endif

#endif /* __XDELTA3_C_HEADER_PASS__ */
#ifdef __XDELTA3_C_INLINE_PASS__

/****************************************************************
 Instruction tables
 *****************************************************************/

/* The following code implements a parametrized description of the
 * code table given above for a few reasons.  It is not necessary for
 * implementing the standard, to support compression with variable
 * tables, so an implementation is only required to know the default
 * code table to begin decompression.  (If the encoder uses an
 * alternate table, the table is included in compressed form inside
 * the VCDIFF file.)
 *
 * Before adding variable-table support there were two functions which
 * were hard-coded to the default table above.
 * xd3_compute_default_table() would create the default table by
 * filling a 256-elt array of xd3_dinst values.  The corresponding
 * function, xd3_choose_instruction(), would choose an instruction
 * based on the hard-coded parameters of the default code table.
 *
 * Notes: The parametrized code table description here only generates
 * tables of a certain regularity similar to the default table by
 * allowing to vary the distribution of single- and
 * double-instructions and change the number of near and same copy
 * modes.  More exotic tables are only possible by extending this
 * code.
 *
 * For performance reasons, both the parametrized and non-parametrized
 * versions of xd3_choose_instruction remain.  The parametrized
 * version is only needed for testing multi-table decoding support.
 * If ever multi-table encoding is required, this can be optimized by
 * compiling static functions for each table.
 */

/* The XD3_CHOOSE_INSTRUCTION calls xd3_choose_instruction with the
 * table description when GENERIC_ENCODE_TABLES are in use.  The
 * IF_GENCODETBL macro enables generic-code-table specific code
 * (removed 10/2014). */
#define XD3_CHOOSE_INSTRUCTION(stream,prev,inst) \
  xd3_choose_instruction (prev, inst)

/* This structure maintains information needed by
 * xd3_choose_instruction to compute the code for a double instruction
 * by first indexing an array of code_table_sizes by copy mode, then
 * using (offset + (muliplier * X)) */
struct _xd3_code_table_sizes {
  uint8_t cpy_max;
  uint8_t offset;
  uint8_t mult;
};

/* This contains a complete description of a code table. */
struct _xd3_code_table_desc
{
  /* Assumes a single RUN instruction */
  /* Assumes that MIN_MATCH is 4 */

  uint8_t add_sizes;            /* Number of immediate-size single
				   adds (default 17) */
  uint8_t near_modes;           /* Number of near copy modes (default 4) */
  uint8_t same_modes;           /* Number of same copy modes (default 3) */
  uint8_t cpy_sizes;            /* Number of immediate-size single
				   copies (default 15) */

  uint8_t addcopy_add_max;      /* Maximum add size for an add-copy
				   double instruction, all modes
				   (default 4) */
  uint8_t addcopy_near_cpy_max; /* Maximum cpy size for an add-copy
				   double instruction, up through
				   VCD_NEAR modes (default 6) */
  uint8_t addcopy_same_cpy_max; /* Maximum cpy size for an add-copy
				   double instruction, VCD_SAME modes
				   (default 4) */

  uint8_t copyadd_add_max;      /* Maximum add size for a copy-add
				   double instruction, all modes
				   (default 1) */
  uint8_t copyadd_near_cpy_max; /* Maximum cpy size for a copy-add
				   double instruction, up through
				   VCD_NEAR modes (default 4) */
  uint8_t copyadd_same_cpy_max; /* Maximum cpy size for a copy-add
				   double instruction, VCD_SAME modes
				   (default 4) */

  xd3_code_table_sizes addcopy_max_sizes[MAX_MODES];
  xd3_code_table_sizes copyadd_max_sizes[MAX_MODES];
};

/* The rfc3284 code table is represented: */
static const xd3_code_table_desc __rfc3284_code_table_desc = {
  17, /* add sizes */
  4,  /* near modes */
  3,  /* same modes */
  15, /* copy sizes */

  4,  /* add-copy max add */
  6,  /* add-copy max cpy, near */
  4,  /* add-copy max cpy, same */

  1,  /* copy-add max add */
  4,  /* copy-add max cpy, near */
  4,  /* copy-add max cpy, same */

  /* addcopy */
  { {6,163,3},{6,175,3},{6,187,3},{6,199,3},{6,211,3},{6,223,3},
    {4,235,1},{4,239,1},{4,243,1} },
  /* copyadd */
  { {4,247,1},{4,248,1},{4,249,1},{4,250,1},{4,251,1},{4,252,1},
    {4,253,1},{4,254,1},{4,255,1} },
};

/* Computes code table entries of TBL using the specified description. */
static void
xd3_build_code_table (const xd3_code_table_desc *desc, xd3_dinst *tbl)
{
  uint8_t size1, size2;
  uint8_t mode;
  usize_t cpy_modes = 2U + desc->near_modes + desc->same_modes;
  xd3_dinst *d = tbl;

  (d++)->type1 = XD3_RUN;
  (d++)->type1 = XD3_ADD;

  for (size1 = 1; size1 <= desc->add_sizes; size1 += 1, d += 1)
    {
      d->type1 = XD3_ADD;
      d->size1 = size1;
    }

  for (mode = 0; mode < cpy_modes; mode += 1)
    {
      (d++)->type1 = XD3_CPY + mode;

      for (size1 = MIN_MATCH; size1 < MIN_MATCH + desc->cpy_sizes;
	   size1 += 1, d += 1)
	{
	  d->type1 = XD3_CPY + mode;
	  d->size1 = size1;
	}
    }

  for (mode = 0; mode < cpy_modes; mode += 1)
    {
      for (size1 = 1; size1 <= desc->addcopy_add_max; size1 += 1)
	{
	  usize_t max = (mode < 2U + desc->near_modes) ?
	    desc->addcopy_near_cpy_max :
	    desc->addcopy_same_cpy_max;

	  for (size2 = MIN_MATCH; size2 <= max; size2 += 1, d += 1)
	    {
	      d->type1 = XD3_ADD;
	      d->size1 = size1;
	      d->type2 = XD3_CPY + mode;
	      d->size2 = size2;
	    }
	}
    }

  for (mode = 0; mode < cpy_modes; mode += 1)
    {
      usize_t max = (mode < 2U + desc->near_modes) ?
	desc->copyadd_near_cpy_max :
	desc->copyadd_same_cpy_max;

      for (size1 = MIN_MATCH; size1 <= max; size1 += 1)
	{
	  for (size2 = 1; size2 <= desc->copyadd_add_max; size2 += 1, d += 1)
	    {
	      d->type1 = XD3_CPY + mode;
	      d->size1 = size1;
	      d->type2 = XD3_ADD;
	      d->size2 = size2;
	    }
	}
    }

  XD3_ASSERT (d - tbl == 256);
}

/* This function generates the static default code table. */
static const xd3_dinst*
xd3_rfc3284_code_table (void)
{
  static xd3_dinst __rfc3284_code_table[256];

  if (__rfc3284_code_table[0].type1 != XD3_RUN)
    {
      xd3_build_code_table (& __rfc3284_code_table_desc, __rfc3284_code_table);
    }

  return __rfc3284_code_table;
}

#if XD3_ENCODER
/* This version of xd3_choose_instruction is hard-coded for the default
   table. */
static void
xd3_choose_instruction (xd3_rinst *prev, xd3_rinst *inst)
{
  switch (inst->type)
    {
    case XD3_RUN:
      inst->code1 = 0;
      break;

    case XD3_ADD:
      inst->code1 = 1;

      if (inst->size <= 17)
	{
	  inst->code1 += inst->size;

	  if ( (inst->size == 1) &&
	       (prev != NULL) &&
	       (prev->size == 4) &&
	       (prev->type >= XD3_CPY) )
	    {
	      prev->code2 = 247 + (prev->type - XD3_CPY);
	    }
	}

      break;

    default:
      {
	uint8_t mode = inst->type - XD3_CPY;

	XD3_ASSERT (inst->type >= XD3_CPY && inst->type < 12);

	inst->code1 = 19 + 16 * mode;

	if (inst->size <= 18 && inst->size >= 4)
	  {
	    inst->code1 += inst->size - 3;

	    if ( (prev != NULL) &&
		 (prev->type == XD3_ADD) &&
		 (prev->size <= 4) )
	      {
		if ( (inst->size <= 6) &&
		     (mode       <= 5) )
		  {
		    prev->code2 = (uint8_t)(163 + (mode * 12) +
					    (3 * (prev->size - 1)) +
					    (inst->size - 4));
		    XD3_ASSERT (prev->code2 <= 234);
		  }
		else if ( (inst->size == 4) &&
			  (mode       >= 6) )
		  {
		    prev->code2 = 235 + ((mode - 6) * 4) + (prev->size - 1);

		    XD3_ASSERT (prev->code2 <= 246);
		  }
	      }
	  }

	XD3_ASSERT (inst->code1 <= 162);
      }
      break;
    }
}
#endif /* XD3_ENCODER */

/***********************************************************************/

static inline void
xd3_swap_uint8p (uint8_t** p1, uint8_t** p2)
{
  uint8_t *t = (*p1);
  (*p1) = (*p2);
  (*p2) = t;
}

static inline void
xd3_swap_usize_t (usize_t* p1, usize_t* p2)
{
  usize_t t = (*p1);
  (*p1) = (*p2);
  (*p2) = t;
}

/* It's not constant time, but it computes the log. */
static int
xd3_check_pow2 (xoff_t value, usize_t *logof)
{
  xoff_t x = 1;
  usize_t nolog;
  if (logof == NULL) {
    logof = &nolog;
  }

  *logof = 0;

  for (; x != 0; x <<= 1, *logof += 1)
    {
      if (x == value)
	{
	  return 0;
	}
    }

  return XD3_INTERNAL;
}

usize_t
xd3_pow2_roundup (usize_t x)
{
  usize_t i = 1;
  while (x > i) {
    i <<= 1U;
  }
  return i;
}

static xoff_t
xd3_xoff_roundup (xoff_t x)
{
  xoff_t i = 1;
  while (x > i) {
    i <<= 1U;
  }
  return i;
}

static usize_t
xd3_round_blksize (usize_t sz, usize_t blksz)
{
  usize_t mod = sz & (blksz-1);

  XD3_ASSERT (xd3_check_pow2 (blksz, NULL) == 0);

  if (mod == 0)
    {
      return sz;
    }

  if (sz > USIZE_T_MAXBLKSZ)
    {
      return USIZE_T_MAXBLKSZ;
    }

  return sz + (blksz - mod);
}

/***********************************************************************
 Adler32 stream function: code copied from Zlib, defined in RFC1950
 ***********************************************************************/

#define A32_BASE 65521L /* Largest prime smaller than 2^16 */
#define A32_NMAX 5552   /* NMAX is the largest n such that 255n(n+1)/2
			   + (n+1)(BASE-1) <= 2^32-1 */

#define A32_DO1(buf,i)  {s1 += buf[i]; s2 += s1;}
#define A32_DO2(buf,i)  A32_DO1(buf,i); A32_DO1(buf,i+1);
#define A32_DO4(buf,i)  A32_DO2(buf,i); A32_DO2(buf,i+2);
#define A32_DO8(buf,i)  A32_DO4(buf,i); A32_DO4(buf,i+4);
#define A32_DO16(buf)   A32_DO8(buf,0); A32_DO8(buf,8);

static uint32_t adler32 (uint32_t adler, const uint8_t *buf, usize_t len)
{
    uint32_t s1 = adler & 0xffffU;
    uint32_t s2 = (adler >> 16) & 0xffffU;
    int k;

    while (len > 0)
      {
        k    = (len < A32_NMAX) ? len : A32_NMAX;
        len -= k;

	while (k >= 16)
	  {
	    A32_DO16(buf);
	    buf += 16;
            k -= 16;
	  }

	if (k != 0)
	  {
	    do
	      {
		s1 += *buf++;
		s2 += s1;
	      }
	    while (--k);
	  }

        s1 %= A32_BASE;
        s2 %= A32_BASE;
    }

    return (s2 << 16) | s1;
}

/***********************************************************************
 Run-length function
 ***********************************************************************/

#if XD3_ENCODER
static usize_t
xd3_comprun (const uint8_t *seg, usize_t slook, uint8_t *run_cp)
{
  usize_t i;
  usize_t run_l = 0;
  uint8_t run_c = 0;

  for (i = 0; i < slook; i += 1)
    {
      NEXTRUN(seg[i]);
    }

  (*run_cp) = run_c;

  return run_l;
}
#endif

/***********************************************************************
 Basic encoder/decoder functions
 ***********************************************************************/

#if XD3_ENCODER
inline int
xd3_emit_byte (xd3_stream  *stream,
	       xd3_output **outputp,
	       uint8_t      code)
{
  xd3_output *output = (*outputp);

  if (output->next == output->avail)
    {
      xd3_output *aoutput;

      if ((aoutput = xd3_alloc_output (stream, output)) == NULL)
	{
	  return ENOMEM;
	}

      output = (*outputp) = aoutput;
    }

  output->base[output->next++] = code;

  return 0;
}

inline int
xd3_emit_bytes (xd3_stream     *stream,
		xd3_output    **outputp,
		const uint8_t  *base,
		usize_t         size)
{
  xd3_output *output = (*outputp);

  do
    {
      usize_t take;

      if (output->next == output->avail)
	{
	  xd3_output *aoutput;

	  if ((aoutput = xd3_alloc_output (stream, output)) == NULL)
	    {
	      return ENOMEM;
	    }

	  output = (*outputp) = aoutput;
	}

      take = xd3_min (output->avail - output->next, size);

      memcpy (output->base + output->next, base, (size_t) take);

      output->next += take;
      size -= take;
      base += take;
    }
  while (size > 0);

  return 0;
}
#endif /* XD3_ENCODER */

/***********************************************************************
 Address cache stuff
 ***********************************************************************/

static int
xd3_alloc_cache (xd3_stream *stream)
{
  if (stream->acache.near_array != NULL)
    {
      xd3_free (stream, stream->acache.near_array);
    }

  if (stream->acache.same_array != NULL)
    {
      xd3_free (stream, stream->acache.same_array);
    }

  if (((stream->acache.s_near > 0) &&
       (stream->acache.near_array = (usize_t*)
	xd3_alloc (stream, stream->acache.s_near,
		   (usize_t) sizeof (usize_t)))
       == NULL) ||
      ((stream->acache.s_same > 0) &&
       (stream->acache.same_array = (usize_t*)
	xd3_alloc (stream, stream->acache.s_same * 256,
		   (usize_t) sizeof (usize_t)))
       == NULL))
    {
      return ENOMEM;
    }

  return 0;
}

void
xd3_init_cache (xd3_addr_cache* acache)
{
  if (acache->s_near > 0)
    {
      memset (acache->near_array, 0, acache->s_near * sizeof (usize_t));
      acache->next_slot = 0;
    }

  if (acache->s_same > 0)
    {
      memset (acache->same_array, 0, acache->s_same * 256 * sizeof (usize_t));
    }
}

static void
xd3_update_cache (xd3_addr_cache* acache, usize_t addr)
{
  if (acache->s_near > 0)
    {
      acache->near_array[acache->next_slot] = addr;
      acache->next_slot = (acache->next_slot + 1) % acache->s_near;
    }

  if (acache->s_same > 0)
    {
      acache->same_array[addr % (acache->s_same*256)] = addr;
    }
}

#if XD3_ENCODER
/* OPT: this gets called a lot, can it be optimized? */
static int
xd3_encode_address (xd3_stream *stream,
		    usize_t addr,
		    usize_t here,
		    uint8_t* mode)
{
  usize_t d, bestd;
  usize_t i, bestm;
  int ret;
  xd3_addr_cache* acache = & stream->acache;

#define SMALLEST_INT(x) do { if (((x) & ~127U) == 0) { goto good; } } while (0)

  /* Attempt to find the address mode that yields the smallest integer value
   * for "d", the encoded address value, thereby minimizing the encoded size
   * of the address. */
  bestd = addr;
  bestm = VCD_SELF;

  XD3_ASSERT (addr < here);

  SMALLEST_INT (bestd);

  if ((d = here-addr) < bestd)
    {
      bestd = d;
      bestm = VCD_HERE;

      SMALLEST_INT (bestd);
    }

  for (i = 0; i < acache->s_near; i += 1)
    {
      /* Note: If we used signed computation here, we'd could compte d
       * and then check (d >= 0 && d < bestd). */
      if (addr >= acache->near_array[i])
	{
	  d = addr - acache->near_array[i];

	  if (d < bestd)
	    {
	      bestd = d;
	      bestm = i+2; /* 2 counts the VCD_SELF, VCD_HERE modes */

	      SMALLEST_INT (bestd);
	    }
	}
    }

  if (acache->s_same > 0 &&
      acache->same_array[d = addr%(acache->s_same*256)] == addr)
    {
      bestd = d%256;
      /* 2 + s_near offsets past the VCD_NEAR modes */
      bestm = acache->s_near + 2 + d/256;

      if ((ret = xd3_emit_byte (stream, & ADDR_TAIL (stream), bestd)))
	{
	  return ret;
	}
    }
  else
    {
    good:

      if ((ret = xd3_emit_size (stream, & ADDR_TAIL (stream), bestd)))
	{
	  return ret;
	}
    }

  xd3_update_cache (acache, addr);

  (*mode) += bestm;

  return 0;
}
#endif

static int
xd3_decode_address (xd3_stream *stream, usize_t here,
		    usize_t mode, const uint8_t **inpp,
		    const uint8_t *max, usize_t *valp)
{
  int ret;
  usize_t same_start = 2 + stream->acache.s_near;

  if (mode < same_start)
    {
      if ((ret = xd3_read_size (stream, inpp, max, valp))) { return ret; }

      switch (mode)
	{
	case VCD_SELF:
	  break;
	case VCD_HERE:
	  (*valp) = here - (*valp);
	  break;
	default:
	  (*valp) += stream->acache.near_array[mode - 2];
	  break;
	}
    }
  else
    {
      if (*inpp == max)
	{
	  stream->msg = "address underflow";
	  return XD3_INVALID_INPUT;
	}

      mode -= same_start;

      (*valp) = stream->acache.same_array[mode*256 + (**inpp)];

      (*inpp) += 1;
    }

  xd3_update_cache (& stream->acache, *valp);

  return 0;
}

/***********************************************************************
 Alloc/free
***********************************************************************/

static void*
__xd3_alloc_func (void* opaque, size_t items, usize_t size)
{
  return malloc (items * (size_t) size);
}

static void
__xd3_free_func (void* opaque, void* address)
{
  free (address);
}

static void*
xd3_alloc (xd3_stream *stream,
	   usize_t      elts,
	   usize_t      size)
{
  void *a = stream->alloc (stream->opaque, elts, size);

  if (a != NULL)
    {
      IF_DEBUG (stream->alloc_cnt += 1);
      IF_DEBUG2 (DP(RINT "[stream %p malloc] size %"W"u ptr %p\n",
		    (void*)stream, elts * size, a));
    }
  else
    {
      stream->msg = "out of memory";
    }

  return a;
}

static void
xd3_free (xd3_stream *stream,
	  void       *ptr)
{
  if (ptr != NULL)
    {
      IF_DEBUG (stream->free_cnt += 1);
      XD3_ASSERT (stream->free_cnt <= stream->alloc_cnt);
      IF_DEBUG2 (DP(RINT "[stream %p free] %p\n",
		    (void*)stream, ptr));
      stream->free (stream->opaque, ptr);
    }
}

#if XD3_ENCODER
static void*
xd3_alloc0 (xd3_stream *stream,
	    usize_t      elts,
	    usize_t      size)
{
  void *a = xd3_alloc (stream, elts, size);

  if (a != NULL)
    {
      memset (a, 0, (size_t) (elts * size));
    }

  return a;
}

xd3_output*
xd3_alloc_output (xd3_stream *stream,
		  xd3_output *old_output)
{
  xd3_output *output;
  uint8_t    *base;

  if (stream->enc_free != NULL)
    {
      output = stream->enc_free;
      stream->enc_free = output->next_page;
    }
  else
    {
      if ((output = (xd3_output*) xd3_alloc (stream, 1,
					     (usize_t) sizeof (xd3_output)))
	  == NULL)
	{
	  return NULL;
	}

      if ((base = (uint8_t*) xd3_alloc (stream, XD3_ALLOCSIZE,
					sizeof (uint8_t))) == NULL)
	{
	  xd3_free (stream, output);
	  return NULL;
	}

      output->base  = base;
      output->avail = XD3_ALLOCSIZE;
    }

  output->next = 0;

  if (old_output)
    {
      old_output->next_page = output;
    }

  output->next_page = NULL;

  return output;
}

static usize_t
xd3_sizeof_output (xd3_output *output)
{
  usize_t s = 0;

  for (; output; output = output->next_page)
    {
      s += output->next;
    }

  return s;
}

static void
xd3_freelist_output (xd3_stream *stream,
		     xd3_output *output)
{
  xd3_output *tmp;

  while (output)
    {
      tmp    = output;
      output = output->next_page;

      tmp->next = 0;
      tmp->next_page = stream->enc_free;
      stream->enc_free = tmp;
    }
}

static void
xd3_free_output (xd3_stream *stream,
		 xd3_output *output)
{
  xd3_output *next;

 again:
  if (output == NULL)
    {
      return;
    }

  next = output->next_page;

  xd3_free (stream, output->base);
  xd3_free (stream, output);

  output = next;
  goto again;
}
#endif /* XD3_ENCODER */

void
xd3_free_stream (xd3_stream *stream)
{
  xd3_iopt_buflist *blist = stream->iopt_alloc;

  while (blist != NULL)
    {
      xd3_iopt_buflist *tmp = blist;
      blist = blist->next;
      xd3_free (stream, tmp->buffer);
      xd3_free (stream, tmp);
    }

#if XD3_ENCODER
  xd3_free (stream, stream->large_table);
  xd3_free (stream, stream->small_table);
  xd3_free (stream, stream->large_hash.powers);
  xd3_free (stream, stream->small_hash.powers);
  xd3_free (stream, stream->small_prev);

  {
    int i;
    for (i = 0; i < ENC_SECTS; i += 1)
      {
	xd3_free_output (stream, stream->enc_heads[i]);
      }
    xd3_free_output (stream, stream->enc_free);
  }
#endif

  xd3_free (stream, stream->acache.near_array);
  xd3_free (stream, stream->acache.same_array);

  xd3_free (stream, stream->inst_sect.copied1);
  xd3_free (stream, stream->addr_sect.copied1);
  xd3_free (stream, stream->data_sect.copied1);

  if (stream->dec_lastwin != stream->dec_buffer)
    {
      xd3_free (stream, (uint8_t*) stream->dec_lastwin);
    }
  xd3_free (stream, stream->dec_buffer);

  xd3_free (stream, stream->buf_in);
  xd3_free (stream, stream->dec_appheader);
  xd3_free (stream, stream->dec_codetbl);
  xd3_free (stream, stream->code_table_alloc);

#if SECONDARY_ANY
  xd3_free (stream, stream->inst_sect.copied2);
  xd3_free (stream, stream->addr_sect.copied2);
  xd3_free (stream, stream->data_sect.copied2);

  if (stream->sec_type != NULL)
    {
      stream->sec_type->destroy (stream, stream->sec_stream_d);
      stream->sec_type->destroy (stream, stream->sec_stream_i);
      stream->sec_type->destroy (stream, stream->sec_stream_a);
    }
#endif

  xd3_free (stream, stream->whole_target.adds);
  xd3_free (stream, stream->whole_target.inst);
  xd3_free (stream, stream->whole_target.wininfo);

  XD3_ASSERT (stream->alloc_cnt == stream->free_cnt);

  memset (stream, 0, sizeof (xd3_stream));
}

#if (XD3_DEBUG > 1 || VCDIFF_TOOLS)
static const char*
xd3_rtype_to_string (xd3_rtype type, int print_mode)
{
  switch (type)
    {
    case XD3_NOOP:
      return "NOOP ";
    case XD3_RUN:
      return "RUN  ";
    case XD3_ADD:
      return "ADD  ";
    default: break;
    }
  if (! print_mode)
    {
      return "CPY  ";
    }
  switch (type)
    {
    case XD3_CPY + 0: return "CPY_0";
    case XD3_CPY + 1: return "CPY_1";
    case XD3_CPY + 2: return "CPY_2";
    case XD3_CPY + 3: return "CPY_3";
    case XD3_CPY + 4: return "CPY_4";
    case XD3_CPY + 5: return "CPY_5";
    case XD3_CPY + 6: return "CPY_6";
    case XD3_CPY + 7: return "CPY_7";
    case XD3_CPY + 8: return "CPY_8";
    case XD3_CPY + 9: return "CPY_9";
    default:          return "CPY>9";
    }
}
#endif

/****************************************************************
 Stream configuration
 ******************************************************************/

int
xd3_config_stream(xd3_stream *stream,
		  xd3_config *config)
{
  int ret;
  xd3_config defcfg;
  xd3_smatcher *smatcher = &stream->smatcher;

  if (config == NULL)
    {
      config = & defcfg;
      memset (config, 0, sizeof (*config));
    }

  /* Initial setup: no error checks yet */
  memset (stream, 0, sizeof (*stream));

  stream->winsize = config->winsize ? config->winsize : XD3_DEFAULT_WINSIZE;
  stream->sprevsz = config->sprevsz ? config->sprevsz : XD3_DEFAULT_SPREVSZ;

  if (config->iopt_size == 0)
    {
      stream->iopt_size = XD3_ALLOCSIZE / sizeof(xd3_rinst);
      stream->iopt_unlimited = 1;
    }
  else
    {
      stream->iopt_size = config->iopt_size;
    }

  stream->getblk    = config->getblk;
  stream->alloc     = config->alloc ? config->alloc : __xd3_alloc_func;
  stream->free      = config->freef ? config->freef : __xd3_free_func;
  stream->opaque    = config->opaque;
  stream->flags     = config->flags;

  /* Secondary setup. */
  stream->sec_data  = config->sec_data;
  stream->sec_inst  = config->sec_inst;
  stream->sec_addr  = config->sec_addr;

  stream->sec_data.data_type = DATA_SECTION;
  stream->sec_inst.data_type = INST_SECTION;
  stream->sec_addr.data_type = ADDR_SECTION;

  /* Check static sizes. */
  if (sizeof (usize_t) != SIZEOF_USIZE_T ||
      sizeof (xoff_t) != SIZEOF_XOFF_T ||
      (ret = xd3_check_pow2(XD3_ALLOCSIZE, NULL)))
    {
      stream->msg = "incorrect compilation: wrong integer sizes";
      return XD3_INTERNAL;
    }

  /* Check/set secondary compressor. */
  switch (stream->flags & XD3_SEC_TYPE)
    {
    case 0:
      if (stream->flags & XD3_SEC_NOALL)
	{
	  stream->msg = "XD3_SEC flags require a secondary compressor type";
	  return XD3_INTERNAL;
	}
      break;
    case XD3_SEC_FGK:
      FGK_CASE (stream);
    case XD3_SEC_DJW:
      DJW_CASE (stream);
    case XD3_SEC_LZMA:
      LZMA_CASE (stream);
    default:
      stream->msg = "too many secondary compressor types set";
      return XD3_INTERNAL;
    }

  stream->code_table_desc = & __rfc3284_code_table_desc;
  stream->code_table_func = xd3_rfc3284_code_table;

  /* Check sprevsz */
  if (smatcher->small_chain == 1 &&
      smatcher->small_lchain == 1)
    {
      stream->sprevsz = 0;
    }
  else
    {
      if ((ret = xd3_check_pow2 (stream->sprevsz, NULL)))
	{
	  stream->msg = "sprevsz is required to be a power of two";
	  return XD3_INTERNAL;
	}

      stream->sprevmask = stream->sprevsz - 1;
    }

  /* Default scanner settings. */
#if XD3_ENCODER
  switch (config->smatch_cfg)
    {
      IF_BUILD_SOFT(case XD3_SMATCH_SOFT:
      {
	*smatcher = config->smatcher_soft;
	smatcher->string_match = __smatcher_soft.string_match;
	smatcher->name = __smatcher_soft.name;
	if (smatcher->large_look  < MIN_MATCH ||
	    smatcher->large_step  < 1         ||
	    smatcher->small_look  < MIN_MATCH)
	  {
	    stream->msg = "invalid soft string-match config";
	    return XD3_INVALID;
	  }
	break;
      })

      IF_BUILD_DEFAULT(case XD3_SMATCH_DEFAULT:
		    *smatcher = __smatcher_default;
		    break;)
      IF_BUILD_SLOW(case XD3_SMATCH_SLOW:
		    *smatcher = __smatcher_slow;
		    break;)
      IF_BUILD_FASTEST(case XD3_SMATCH_FASTEST:
		    *smatcher = __smatcher_fastest;
		    break;)
      IF_BUILD_FASTER(case XD3_SMATCH_FASTER:
		    *smatcher = __smatcher_faster;
		    break;)
      IF_BUILD_FAST(case XD3_SMATCH_FAST:
		    *smatcher = __smatcher_fast;
		    break;)
    default:
      stream->msg = "invalid string match config type";
      return XD3_INTERNAL;
    }

  if (config->smatch_cfg == XD3_SMATCH_DEFAULT &&
      (stream->flags & XD3_COMPLEVEL_MASK) != 0)
    {
      int level = (stream->flags & XD3_COMPLEVEL_MASK) >> XD3_COMPLEVEL_SHIFT;

      switch (level)
	{
	case 1:
	  IF_BUILD_FASTEST(*smatcher = __smatcher_fastest;
			   break;)
	case 2:
	  IF_BUILD_FASTER(*smatcher = __smatcher_faster;
			   break;)
	case 3: case 4: case 5:
	  IF_BUILD_FAST(*smatcher = __smatcher_fast;
			break;)
	case 6:
	  IF_BUILD_DEFAULT(*smatcher = __smatcher_default;
			   break;)
	default:
	  IF_BUILD_SLOW(*smatcher = __smatcher_slow;
			break;)
	  IF_BUILD_DEFAULT(*smatcher = __smatcher_default;
			   break;)
	  IF_BUILD_FAST(*smatcher = __smatcher_fast;
			break;)
	  IF_BUILD_FASTER(*smatcher = __smatcher_faster;
			break;)
	  IF_BUILD_FASTEST(*smatcher = __smatcher_fastest;
			   break;)
	}
    }
#endif

  return 0;
}

/***********************************************************
 Getblk interface
 ***********************************************************/

inline
xoff_t xd3_source_eof(const xd3_source *src)
{
  xoff_t r = (src->max_blkno << src->shiftby) + (xoff_t)src->onlastblk;
  return r;
}

inline
usize_t xd3_bytes_on_srcblk (xd3_source *src, xoff_t blkno)
{
  usize_t r = (blkno == src->max_blkno ?
	       src->onlastblk :
	       src->blksize);
  return r;
}

/* This function interfaces with the client getblk function, checks
 * its results, updates max_blkno, onlastblk, eof_known. */
static int
xd3_getblk (xd3_stream *stream, xoff_t blkno)
{
  int ret;
  xd3_source *source = stream->src;

  if (source->curblk == NULL || blkno != source->curblkno)
    {
      source->getblkno = blkno;

      if (stream->getblk == NULL)
	{
	  IF_DEBUG2 (DP(RINT "[getblk] XD3_GETSRCBLK %"Q"u\n", blkno));
	  stream->msg = "getblk source input";
	  return XD3_GETSRCBLK;
	}

      ret = stream->getblk (stream, source, blkno);
      if (ret != 0)
	{
	  IF_DEBUG2 (DP(RINT "[getblk] app error blkno %"Q"u: %s\n",
			blkno, xd3_strerror (ret)));
	  return ret;
	}

      IF_DEBUG2 (DP(RINT "[getblk] read source block %"Q"u onblk "
		    "%"W"u blksize %"W"u max_blkno %"Q"u\n", blkno, source->onblk,
		    source->blksize, source->max_blkno));
    }

  if (blkno > source->max_blkno)
    {
      source->max_blkno = blkno;

      if (source->onblk == source->blksize)
	{
	  IF_DEBUG1 (DP(RINT "[getblk] full source blkno %"Q"u: "
			"source length unknown %"Q"u\n",
			blkno,
			xd3_source_eof (source)));
	}
      else if (!source->eof_known)
	{
	  IF_DEBUG1 (DP(RINT "[getblk] eof block has %"W"u bytes; "
			"source length known %"Q"u\n",
			xd3_bytes_on_srcblk (source, blkno),
			xd3_source_eof (source)));
	  source->eof_known = 1;
	}
    }

  XD3_ASSERT (source->curblk != NULL);

  if (blkno == source->max_blkno)
    {
      /* In case the application sets the source as 1 block w/ a
       * preset buffer. */
      source->onlastblk = source->onblk;
    }
  return 0;
}

/***********************************************************
 Stream open/close
 ***************************************************************/

int
xd3_set_source (xd3_stream *stream,
		xd3_source *src)
{
  usize_t shiftby;

  stream->src = src;
  src->srclen  = 0;
  src->srcbase = 0;

  /* Enforce power-of-two blocksize so that source-block number
   * calculations are cheap. */
  if (xd3_check_pow2 (src->blksize, &shiftby) != 0)
    {
      src->blksize = xd3_pow2_roundup(src->blksize);
      xd3_check_pow2 (src->blksize, &shiftby);
      IF_DEBUG1 (DP(RINT "raising src_blksz to %"W"u\n", src->blksize));
    }

  src->shiftby = shiftby;
  src->maskby = (1ULL << shiftby) - 1ULL;

  if (xd3_check_pow2 (src->max_winsize, NULL) != 0)
    {
      src->max_winsize = xd3_xoff_roundup(src->max_winsize);
      IF_DEBUG1 (DP(RINT "raising src_maxsize to %"W"u\n", src->blksize));
    }
  src->max_winsize = xd3_max (src->max_winsize, XD3_ALLOCSIZE);
  return 0;
}

int
xd3_set_source_and_size (xd3_stream *stream,
			 xd3_source *user_source,
			 xoff_t source_size) {
  int ret = xd3_set_source (stream, user_source);
  if (ret == 0)
    {
      stream->src->eof_known = 1;
      IF_DEBUG2 (DP(RINT "[set source] size known %"Q"u\n",
		    source_size));
      xd3_blksize_div(source_size,
		      stream->src,
		      &stream->src->max_blkno,
		      &stream->src->onlastblk);

      IF_DEBUG1 (DP(RINT "[set source] size known %"Q"u max_blkno %"Q"u\n",
		    source_size, stream->src->max_blkno));
    }
  return ret;
}

void
xd3_abort_stream (xd3_stream *stream)
{
  stream->dec_state = DEC_ABORTED;
  stream->enc_state = ENC_ABORTED;
}

int
xd3_close_stream (xd3_stream *stream)
{
  if (stream->enc_state != 0 && stream->enc_state != ENC_ABORTED)
    {
      if (stream->buf_leftover != NULL)
	{
	  stream->msg = "encoding is incomplete";
	  return XD3_INTERNAL;
	}

      if (stream->enc_state == ENC_POSTWIN)
	{
#if XD3_ENCODER
	  xd3_encode_reset (stream);
#endif
	  stream->current_window += 1;
	  stream->enc_state = ENC_INPUT;
	}

      /* If encoding, should be ready for more input but not actually
	 have any. */
      if (stream->enc_state != ENC_INPUT || stream->avail_in != 0)
	{
	  stream->msg = "encoding is incomplete";
	  return XD3_INTERNAL;
	}
    }
  else
    {
      switch (stream->dec_state)
	{
	case DEC_VCHEAD:
	case DEC_WININD:
	  /* TODO: Address the zero-byte ambiguity.  Does the encoder
	   * emit a window or not?  If so, then catch an error here.
	   * If not, need another routine to say
	   * decode_at_least_one_if_empty. */
	case DEC_ABORTED:
	  break;
	default:
	  /* If decoding, should be ready for the next window. */
	  stream->msg = "eof in decode";
	  return XD3_INVALID_INPUT;
	}
    }

  return 0;
}

/**************************************************************
 Application header
 ****************************************************************/

int
xd3_get_appheader (xd3_stream  *stream,
		   uint8_t    **data,
		   usize_t      *size)
{
  if (stream->dec_state < DEC_WININD)
    {
      stream->msg = "application header not available";
      return XD3_INTERNAL;
    }

  (*data) = stream->dec_appheader;
  (*size) = stream->dec_appheadsz;
  return 0;
}

/**********************************************************
 Decoder stuff
 *************************************************/

#include "xdelta3-decode.h"

/****************************************************************
 Encoder stuff
 *****************************************************************/

#if XD3_ENCODER
void
xd3_set_appheader (xd3_stream    *stream,
		   const uint8_t *data,
		   usize_t         size)
{
  stream->enc_appheader = data;
  stream->enc_appheadsz = size;
}

#if XD3_DEBUG
static int
xd3_iopt_check (xd3_stream *stream)
{
  usize_t ul = xd3_rlist_length (& stream->iopt_used);
  usize_t fl = xd3_rlist_length (& stream->iopt_free);

  return (ul + fl + (stream->iout ? 1 : 0)) == stream->iopt_size;
}
#endif

static xd3_rinst*
xd3_iopt_free (xd3_stream *stream, xd3_rinst *i)
{
  xd3_rinst *n = xd3_rlist_remove (i);
  xd3_rlist_push_back (& stream->iopt_free, i);
  return n;
}

static void
xd3_iopt_free_nonadd (xd3_stream *stream, xd3_rinst *i)
{
  if (i->type != XD3_ADD)
    {
      xd3_rlist_push_back (& stream->iopt_free, i);
    }
}

/* When an instruction is ready to flush from the iopt buffer, this
 * function is called to produce an encoding.  It writes the
 * instruction plus size, address, and data to the various encoding
 * sections. */
static int
xd3_iopt_finish_encoding (xd3_stream *stream, xd3_rinst *inst)
{
  int ret;

  /* Check for input overflow. */
  XD3_ASSERT (inst->pos + inst->size <= stream->avail_in);

  switch (inst->type)
    {
    case XD3_CPY:
      {
	/* the address may have an offset if there is a source window. */
	usize_t addr;
	xd3_source *src = stream->src;

	if (src != NULL)
	  {
	    /* If there is a source copy, the source must have its
	     * source window decided before we can encode.  This can
	     * be bad -- we have to make this decision even if no
	     * source matches have been found. */
	    if (stream->srcwin_decided == 0)
	      {
		if ((ret = xd3_srcwin_setup (stream))) { return ret; }
	      }
	    else
	      {
		stream->srcwin_decided_early = (!stream->src->eof_known ||
						(stream->srcwin_cksum_pos <
						 xd3_source_eof (stream->src)));
	      }

	    /* xtra field indicates the copy is from the source */
	    if (inst->xtra)
	      {
		XD3_ASSERT (inst->addr >= src->srcbase);
		XD3_ASSERT (inst->addr + inst->size <=
			    src->srcbase + src->srclen);
		addr = inst->addr - src->srcbase;
		stream->n_scpy += 1;
		stream->l_scpy += inst->size;
	      }
	    else
	      {
		/* with source window: target copy address is offset
		 * by taroff. */
		addr = stream->taroff + inst->addr;
		stream->n_tcpy += 1;
		stream->l_tcpy += inst->size;
	      }
	  }
	else
	  {
	    addr = inst->addr;
	    stream->n_tcpy += 1;
	    stream->l_tcpy += inst->size;
	  }

	/* Note: used to assert inst->size >= MIN_MATCH, but not true
	 * for merge operations & identical match heuristics. */
	/* the "here" position is always offset by taroff */
	if ((ret = xd3_encode_address (stream, addr, inst->pos + stream->taroff,
				       & inst->type)))
	  {
	    return ret;
	  }

	IF_DEBUG2 ({
	  static int cnt;
	  DP(RINT "[iopt copy:%d] pos %"Q"u-%"Q"u addr %"Q"u-%"Q"u size %"W"u\n",
		   cnt++,
		   stream->total_in + inst->pos,
		   stream->total_in + inst->pos + inst->size,
		   inst->addr, inst->addr + inst->size, inst->size);
	});
	break;
      }
    case XD3_RUN:
      {
	if ((ret = xd3_emit_byte (stream, & DATA_TAIL (stream), inst->xtra))) { return ret; }

	stream->n_run += 1;
	stream->l_run += inst->size;

	IF_DEBUG2 ({
	  static int cnt;
	  DP(RINT "[iopt run:%d] pos %"Q"u size %"W"u\n", cnt++, stream->total_in + inst->pos, inst->size);
	});
	break;
      }
    case XD3_ADD:
      {
	if ((ret = xd3_emit_bytes (stream, & DATA_TAIL (stream),
				   stream->next_in + inst->pos, inst->size))) { return ret; }

	stream->n_add += 1;
	stream->l_add += inst->size;

	IF_DEBUG2 ({
	  static int cnt;
	  DP(RINT "[iopt add:%d] pos %"Q"u size %"W"u\n", cnt++, stream->total_in + inst->pos, inst->size);
	});

	break;
      }
    }

  /* This is the only place stream->unencoded_offset is incremented. */
  XD3_ASSERT (stream->unencoded_offset == inst->pos);
  stream->unencoded_offset += inst->size;

  inst->code2 = 0;

  XD3_CHOOSE_INSTRUCTION (stream, stream->iout, inst);

  if (stream->iout != NULL)
    {
      if (stream->iout->code2 != 0)
	{
	  if ((ret = xd3_emit_double (stream, stream->iout, inst, 
				      stream->iout->code2))) { return ret; }

	  xd3_iopt_free_nonadd (stream, stream->iout);
	  xd3_iopt_free_nonadd (stream, inst);
	  stream->iout = NULL;
	  return 0;
	}
      else
	{
	  if ((ret = xd3_emit_single (stream, stream->iout, stream->iout->code1))) { return ret; }

	  xd3_iopt_free_nonadd (stream, stream->iout);
	}
    }

  stream->iout = inst;

  return 0;
}

/* This possibly encodes an add instruction, iadd, which must remain
 * on the stack until the following call to
 * xd3_iopt_finish_encoding. */
static int
xd3_iopt_add (xd3_stream *stream, usize_t pos, xd3_rinst *iadd)
{
  int ret;
  usize_t off = stream->unencoded_offset;

  if (pos > off)
    {
      iadd->type = XD3_ADD;
      iadd->pos  = off;
      iadd->size = pos - off;

      if ((ret = xd3_iopt_finish_encoding (stream, iadd))) { return ret; }
    }

  return 0;
}

/* This function calls xd3_iopt_finish_encoding to finish encoding an
 * instruction, and it may also produce an add instruction for an
 * unmatched region. */
static int
xd3_iopt_add_encoding (xd3_stream *stream, xd3_rinst *inst)
{
  int ret;
  xd3_rinst iadd;

  if ((ret = xd3_iopt_add (stream, inst->pos, & iadd))) { return ret; }

  if ((ret = xd3_iopt_finish_encoding (stream, inst))) { return ret; }

  return 0;
}

/* Generates a final add instruction to encode the remaining input. */
static int
xd3_iopt_add_finalize (xd3_stream *stream)
{
  int ret;
  xd3_rinst iadd;

  if ((ret = xd3_iopt_add (stream, stream->avail_in, & iadd))) { return ret; }

  if (stream->iout)
    {
      if ((ret = xd3_emit_single (stream, stream->iout, stream->iout->code1))) { return ret; }

      xd3_iopt_free_nonadd (stream, stream->iout);
      stream->iout = NULL;
    }

  return 0;
}

/* Compact the instruction buffer by choosing the best non-overlapping
 * instructions when lazy string-matching.  There are no ADDs in the
 * iopt buffer because those are synthesized in xd3_iopt_add_encoding
 * and during xd3_iopt_add_finalize. */
static int
xd3_iopt_flush_instructions (xd3_stream *stream, int force)
{
  xd3_rinst *r1 = xd3_rlist_front (& stream->iopt_used);
  xd3_rinst *r2;
  xd3_rinst *r3;
  usize_t r1end;
  usize_t r2end;
  usize_t r2off;
  usize_t r2moff;
  usize_t gap;
  usize_t flushed;
  int ret;

  XD3_ASSERT (xd3_iopt_check (stream));

  /* Note: once tried to skip this step if it's possible to assert
   * there are no overlapping instructions.  Doesn't work because
   * xd3_opt_erase leaves overlapping instructions. */
  while (! xd3_rlist_end (& stream->iopt_used, r1) &&
	 ! xd3_rlist_end (& stream->iopt_used, r2 = xd3_rlist_next (r1)))
    {
      r1end = r1->pos + r1->size;

      /* If the instructions do not overlap, continue. */
      if (r1end <= r2->pos)
	{
	  r1 = r2;
	  continue;
	}

      r2end = r2->pos + r2->size;

      /* The min_match adjustments prevent this. */
      XD3_ASSERT (r2end > (r1end + LEAST_MATCH_INCR));

      /* If r3 is available... */
      if (! xd3_rlist_end (& stream->iopt_used, r3 = xd3_rlist_next (r2)))
	{
	  /* If r3 starts before r1 finishes or just about, r2 is irrelevant */
	  if (r3->pos <= r1end + 1)
	    {
	      xd3_iopt_free (stream, r2);
	      continue;
	    }
	}
      else if (! force)
	{
	  /* Unless force, end the loop when r3 is not available. */
	  break;
	}

      r2off  = r2->pos - r1->pos;
      r2moff = r2end - r1end;
      gap    = r2end - r1->pos;

      /* If the two matches overlap almost entirely, choose the better match
       * and discard the other.  The else branch can still create inefficient
       * copies, e.g., a 4-byte copy that takes 4 bytes to encode, which
       * xd3_smatch() wouldn't allow by its crude efficiency check.  However,
       * in this case there are adjacent copies which mean the add would cost
       * one extra byte.  Allow the inefficiency here. */
      if (gap < 2*MIN_MATCH || r2moff <= 2 || r2off <= 2)
	{
	  /* Only one match should be used, choose the longer one. */
	  if (r1->size < r2->size)
	    {
	      xd3_iopt_free (stream, r1);
	      r1 = r2;
	    }
	  else
	    {
	      /* We are guaranteed that r1 does not overlap now, so advance past r2 */
	      r1 = xd3_iopt_free (stream, r2);
	    }
	  continue;
	}
      else
	{
	  /* Shorten one of the instructions -- could be optimized
	   * based on the address cache. */
	  usize_t average;
	  usize_t newsize;
	  usize_t adjust1;

	  XD3_ASSERT (r1end > r2->pos && r2end > r1->pos);

	  /* Try to balance the length of both instructions, but avoid
	   * making both longer than MAX_MATCH_SPLIT . */
	  average = gap / 2;
	  newsize = xd3_min (MAX_MATCH_SPLIT, gap - average);

	  /* Should be possible to simplify this code. */
	  if (newsize > r1->size)
	    {
	      /* shorten r2 */
	      adjust1 = r1end - r2->pos;
	    }
	  else if (newsize > r2->size)
	    {
	      /* shorten r1 */
	      adjust1 = r1end - r2->pos;

	      XD3_ASSERT (r1->size > adjust1);

	      r1->size -= adjust1;

	      /* don't shorten r2 */
	      adjust1 = 0;
	    }
	  else
	    {
	      /* shorten r1 */
	      adjust1 = r1->size - newsize;

	      if (r2->pos > r1end - adjust1)
		{
		  adjust1 -= r2->pos - (r1end - adjust1);
		}

	      XD3_ASSERT (r1->size > adjust1);

	      r1->size -= adjust1;

	      /* shorten r2 */
	      XD3_ASSERT (r1->pos + r1->size >= r2->pos);

	      adjust1 = r1->pos + r1->size - r2->pos;
	    }

	  /* Fallthrough above if-else, shorten r2 */
	  XD3_ASSERT (r2->size > adjust1);

	  r2->size -= adjust1;
	  r2->pos  += adjust1;
	  r2->addr += adjust1;

	  XD3_ASSERT (r1->size >= MIN_MATCH);
	  XD3_ASSERT (r2->size >= MIN_MATCH);

	  r1 = r2;
	}
    }

  XD3_ASSERT (xd3_iopt_check (stream));

  /* If forcing, pick instructions until the list is empty, otherwise
   * this empties 50% of the queue. */
  for (flushed = 0; ! xd3_rlist_empty (& stream->iopt_used); )
    {
      xd3_rinst *renc = xd3_rlist_pop_front (& stream->iopt_used);
      if ((ret = xd3_iopt_add_encoding (stream, renc)))
	{
	  return ret;
	}

      if (! force)
	{
	  if (++flushed > stream->iopt_size / 2)
	    {
	      break;
	    }

	  /* If there are only two instructions remaining, break,
	   * because they were not optimized.  This means there were
	   * more than 50% eliminated by the loop above. */
 	  r1 = xd3_rlist_front (& stream->iopt_used);
 	  if (xd3_rlist_end(& stream->iopt_used, r1) ||
 	      xd3_rlist_end(& stream->iopt_used, r2 = xd3_rlist_next (r1)) ||
 	      xd3_rlist_end(& stream->iopt_used, r3 = xd3_rlist_next (r2)))
 	    {
 	      break;
 	    }
	}
    }

  XD3_ASSERT (xd3_iopt_check (stream));

  XD3_ASSERT (!force || xd3_rlist_length (& stream->iopt_used) == 0);

  return 0;
}

static int
xd3_iopt_get_slot (xd3_stream *stream, xd3_rinst** iptr)
{
  xd3_rinst *i;
  int ret;

  if (xd3_rlist_empty (& stream->iopt_free))
    {
      if (stream->iopt_unlimited)
	{
	  usize_t elts = XD3_ALLOCSIZE / sizeof(xd3_rinst);

	  if ((ret = xd3_alloc_iopt (stream, elts)))
	    {
	      return ret;
	    }

	  stream->iopt_size += elts;
	}
      else
	{
	  if ((ret = xd3_iopt_flush_instructions (stream, 0))) { return ret; }

	  XD3_ASSERT (! xd3_rlist_empty (& stream->iopt_free));
	}
    }

  i = xd3_rlist_pop_back (& stream->iopt_free);

  xd3_rlist_push_back (& stream->iopt_used, i);

  (*iptr) = i;

  ++stream->i_slots_used;

  return 0;
}

/* A copy is about to be emitted that extends backwards to POS,
 * therefore it may completely cover some existing instructions in the
 * buffer.  If an instruction is completely covered by this new match,
 * erase it.  If the new instruction is covered by the previous one,
 * return 1 to skip it. */
static void
xd3_iopt_erase (xd3_stream *stream, usize_t pos, usize_t size)
{
  while (! xd3_rlist_empty (& stream->iopt_used))
    {
      xd3_rinst *r = xd3_rlist_back (& stream->iopt_used);

      /* Verify that greedy is working.  The previous instruction
       * should end before the new one begins. */
      XD3_ASSERT ((stream->flags & XD3_BEGREEDY) == 0 || (r->pos + r->size <= pos));
      /* Verify that min_match is working.  The previous instruction
       * should end before the new one ends. */
      XD3_ASSERT ((stream->flags & XD3_BEGREEDY) != 0 || (r->pos + r->size < pos + size));

      /* See if the last instruction starts before the new
       * instruction.  If so, there is nothing to erase. */
      if (r->pos < pos)
	{
	  return;
	}

      /* Otherwise, the new instruction covers the old one, delete it
	 and repeat. */
      xd3_rlist_remove (r);
      xd3_rlist_push_back (& stream->iopt_free, r);
      --stream->i_slots_used;
    }
}

/* This function tells the last matched input position. */
static usize_t
xd3_iopt_last_matched (xd3_stream *stream)
{
  xd3_rinst *r;

  if (xd3_rlist_empty (& stream->iopt_used))
    {
      return 0;
    }

  r = xd3_rlist_back (& stream->iopt_used);

  return r->pos + r->size;
}

/*********************************************************
 Emit routines
 ***********************************************************/

static int
xd3_emit_single (xd3_stream *stream, xd3_rinst *single, uint8_t code)
{
  int has_size = stream->code_table[code].size1 == 0;
  int ret;

  IF_DEBUG2 (DP(RINT "[emit1] %"W"u %s (%"W"u) code %u\n",
		single->pos,
		xd3_rtype_to_string ((xd3_rtype) single->type, 0),
		single->size,
		code));

  if ((ret = xd3_emit_byte (stream, & INST_TAIL (stream), code)))
    {
      return ret;
    }

  if (has_size)
    {
      if ((ret = xd3_emit_size (stream, & INST_TAIL (stream), single->size)))
        {
          return ret;
        }
    }

  return 0;
}

static int
xd3_emit_double (xd3_stream *stream, xd3_rinst *first,
                 xd3_rinst *second, uint8_t code)
{
  int ret;

  /* All double instructions use fixed sizes, so all we need to do is
   * output the instruction code, no sizes. */
  XD3_ASSERT (stream->code_table[code].size1 != 0 &&
	      stream->code_table[code].size2 != 0);

  if ((ret = xd3_emit_byte (stream, & INST_TAIL (stream), code)))
    {
      return ret;
    }

  IF_DEBUG2 (DP(RINT "[emit2]: %"W"u %s (%"W"u) %s (%"W"u) code %u\n",
		first->pos,
		xd3_rtype_to_string ((xd3_rtype) first->type, 0),
		first->size,
		xd3_rtype_to_string ((xd3_rtype) second->type, 0),
		second->size,
		code));

  return 0;
}

/* This enters a potential run instruction into the iopt buffer.  The
 * position argument is relative to the target window. */
static int
xd3_emit_run (xd3_stream *stream, usize_t pos, usize_t size, uint8_t *run_c)
{
  xd3_rinst* ri;
  int ret;

  if ((ret = xd3_iopt_get_slot (stream, & ri))) { return ret; }

  ri->type = XD3_RUN;
  ri->xtra = *run_c;
  ri->pos  = pos;
  ri->size = size;

  return 0;
}

/* This enters a potential copy instruction into the iopt buffer.  The
 * position argument is relative to the target window.. */
int
xd3_found_match (xd3_stream *stream, usize_t pos,
		 usize_t size, xoff_t addr, int is_source)
{
  xd3_rinst* ri;
  int ret;

  if ((ret = xd3_iopt_get_slot (stream, & ri))) { return ret; }

  ri->type = XD3_CPY;
  ri->xtra = is_source;
  ri->pos  = pos;
  ri->size = size;
  ri->addr = addr;

  return 0;
}

static int
xd3_emit_hdr (xd3_stream *stream)
{
  int  ret;
  int  use_secondary = stream->sec_type != NULL;
  int  use_adler32   = stream->flags & (XD3_ADLER32 | XD3_ADLER32_RECODE);
  int  vcd_source    = xd3_encoder_used_source (stream);
  uint8_t win_ind = 0;
  uint8_t del_ind = 0;
  usize_t enc_len;
  usize_t tgt_len;
  usize_t data_len;
  usize_t inst_len;
  usize_t addr_len;

  if (stream->current_window == 0)
    {
      uint8_t hdr_ind = 0;
      int use_appheader  = stream->enc_appheader != NULL;

      if (use_secondary)  { hdr_ind |= VCD_SECONDARY; }
      if (use_appheader)  { hdr_ind |= VCD_APPHEADER; }

      if ((ret = xd3_emit_byte (stream, & HDR_TAIL (stream),
				VCDIFF_MAGIC1)) != 0 ||
	  (ret = xd3_emit_byte (stream, & HDR_TAIL (stream),
				VCDIFF_MAGIC2)) != 0 ||
	  (ret = xd3_emit_byte (stream, & HDR_TAIL (stream),
				VCDIFF_MAGIC3)) != 0 ||
	  (ret = xd3_emit_byte (stream, & HDR_TAIL (stream),
				VCDIFF_VERSION)) != 0 ||
	  (ret = xd3_emit_byte (stream, & HDR_TAIL (stream), hdr_ind)) != 0)
	{
	  return ret;
	}

      /* Secondary compressor ID */
#if SECONDARY_ANY
      if (use_secondary &&
	  (ret = xd3_emit_byte (stream, & HDR_TAIL (stream),
				stream->sec_type->id)))
	{
	  return ret;
	}
#endif

      /* Application header */
      if (use_appheader)
	{
	  if ((ret = xd3_emit_size (stream, & HDR_TAIL (stream),
				    stream->enc_appheadsz)) ||
	      (ret = xd3_emit_bytes (stream, & HDR_TAIL (stream),
				     stream->enc_appheader,
				     stream->enc_appheadsz)))
	    {
	      return ret;
	    }
	}
    }

  /* try to compress this window */
#if SECONDARY_ANY
  if (use_secondary)
    {
      int data_sec = 0;
      int inst_sec = 0;
      int addr_sec = 0;

#     define ENCODE_SECONDARY_SECTION(UPPER,LOWER) \
             ((stream->flags & XD3_SEC_NO ## UPPER) == 0 && \
              (ret = xd3_encode_secondary (stream, \
					   & UPPER ## _HEAD (stream), \
					   & UPPER ## _TAIL (stream), \
					& xd3_sec_ ## LOWER (stream), \
				        & stream->sec_ ## LOWER, \
					   & LOWER ## _sec)))

      if (ENCODE_SECONDARY_SECTION (DATA, data) ||
	  ENCODE_SECONDARY_SECTION (INST, inst) ||
	  ENCODE_SECONDARY_SECTION (ADDR, addr))
	{
	  return ret;
	}

      del_ind |= (data_sec ? VCD_DATACOMP : 0);
      del_ind |= (inst_sec ? VCD_INSTCOMP : 0);
      del_ind |= (addr_sec ? VCD_ADDRCOMP : 0);
    }
#endif

  /* if (vcd_target) { win_ind |= VCD_TARGET; } */
  if (vcd_source)  { win_ind |= VCD_SOURCE; }
  if (use_adler32) { win_ind |= VCD_ADLER32; }

  /* window indicator */
  if ((ret = xd3_emit_byte (stream, & HDR_TAIL (stream), win_ind)))
    {
      return ret;
    }

  /* source window */
  if (vcd_source)
    {
      /* or (vcd_target) { ... } */
      if ((ret = xd3_emit_size (stream, & HDR_TAIL (stream),
				stream->src->srclen)) ||
	  (ret = xd3_emit_offset (stream, & HDR_TAIL (stream),
				  stream->src->srcbase))) { return ret; }
    }

  tgt_len  = stream->avail_in;
  data_len = xd3_sizeof_output (DATA_HEAD (stream));
  inst_len = xd3_sizeof_output (INST_HEAD (stream));
  addr_len = xd3_sizeof_output (ADDR_HEAD (stream));

  /* The enc_len field is a redundency for future extensions. */
  enc_len = (1 + (xd3_sizeof_size (tgt_len) +
		  xd3_sizeof_size (data_len) +
		  xd3_sizeof_size (inst_len) +
		  xd3_sizeof_size (addr_len)) +
	     data_len +
	     inst_len +
	     addr_len +
	     (use_adler32 ? 4 : 0));

  if ((ret = xd3_emit_size (stream, & HDR_TAIL (stream), enc_len)) ||
      (ret = xd3_emit_size (stream, & HDR_TAIL (stream), tgt_len)) ||
      (ret = xd3_emit_byte (stream, & HDR_TAIL (stream), del_ind)) ||
      (ret = xd3_emit_size (stream, & HDR_TAIL (stream), data_len)) ||
      (ret = xd3_emit_size (stream, & HDR_TAIL (stream), inst_len)) ||
      (ret = xd3_emit_size (stream, & HDR_TAIL (stream), addr_len)))
    {
      return ret;
    }

  if (use_adler32)
    {
      uint8_t  send[4];
      uint32_t a32;

      if (stream->flags & XD3_ADLER32)
	{
	  a32 = adler32 (1L, stream->next_in, stream->avail_in);
	}
      else
	{
	  a32 = stream->recode_adler32;
	}

      /* Four bytes. */
      send[0] = (uint8_t) (a32 >> 24);
      send[1] = (uint8_t) (a32 >> 16);
      send[2] = (uint8_t) (a32 >> 8);
      send[3] = (uint8_t) (a32 & 0x000000FFU);

      if ((ret = xd3_emit_bytes (stream, & HDR_TAIL (stream), send, 4)))
	{
	  return ret;
	}
    }

  return 0;
}

/****************************************************************
 Encode routines
 ****************************************************************/

static int
xd3_encode_buffer_leftover (xd3_stream *stream)
{
  usize_t take;
  usize_t room;

  /* Allocate the buffer. */
  if (stream->buf_in == NULL &&
      (stream->buf_in = (uint8_t*) xd3_alloc (stream, stream->winsize, 1)) == NULL)
    {
      return ENOMEM;
    }

  IF_DEBUG2 (DP(RINT "[leftover] flush?=%s\n", (stream->flags & XD3_FLUSH) ? "yes" : "no"));

  /* Take leftover input first. */
  if (stream->buf_leftover != NULL)
    {
      XD3_ASSERT (stream->buf_avail == 0);
      XD3_ASSERT (stream->buf_leftavail < stream->winsize);

      IF_DEBUG2 (DP(RINT "[leftover] previous %"W"u avail %"W"u\n",
		    stream->buf_leftavail, stream->avail_in));

      memcpy (stream->buf_in, stream->buf_leftover, stream->buf_leftavail);

      stream->buf_leftover = NULL;
      stream->buf_avail    = stream->buf_leftavail;
    }

  /* Copy into the buffer. */
  room = stream->winsize - stream->buf_avail;
  take = xd3_min (room, stream->avail_in);

  memcpy (stream->buf_in + stream->buf_avail, stream->next_in, take);

  stream->buf_avail += take;

  if (take < stream->avail_in)
    {
      /* Buffer is full */
      stream->buf_leftover  = stream->next_in  + take;
      stream->buf_leftavail = stream->avail_in - take;
    }
  else if ((stream->buf_avail < stream->winsize) && !(stream->flags & XD3_FLUSH))
    {
      /* Buffer has space */
      IF_DEBUG2 (DP(RINT "[leftover] emptied %"W"u\n", take));
      return XD3_INPUT;
    }

  /* Use the buffer: */
  IF_DEBUG2 (DP(RINT "[leftover] take %"W"u remaining %"W"u\n", take, stream->buf_leftavail));
  stream->next_in   = stream->buf_in;
  stream->avail_in  = stream->buf_avail;
  stream->buf_avail = 0;

  return 0;
}

/* Allocates one block of xd3_rlist elements */
static int
xd3_alloc_iopt (xd3_stream *stream, usize_t elts)
{
  usize_t i;
  xd3_iopt_buflist* last =
    (xd3_iopt_buflist*) xd3_alloc (stream, sizeof (xd3_iopt_buflist), 1);

  if (last == NULL ||
      (last->buffer = (xd3_rinst*) xd3_alloc (stream, sizeof (xd3_rinst), elts)) == NULL)
    {
      return ENOMEM;
    }

  last->next = stream->iopt_alloc;
  stream->iopt_alloc = last;

  for (i = 0; i < elts; i += 1)
    {
      xd3_rlist_push_back (& stream->iopt_free, & last->buffer[i]);
    }

  return 0;
}

/* This function allocates all memory initially used by the encoder. */
static int
xd3_encode_init (xd3_stream *stream, int full_init)
{
  int ret;
  int i;

  if (full_init)
    {
      int large_comp = (stream->src != NULL);
      int small_comp = ! (stream->flags & XD3_NOCOMPRESS);

      /* Memory allocations for checksum tables are delayed until
       * xd3_string_match_init in the first call to string_match--that way
       * identical or short inputs require no table allocation. */
      if (large_comp)
	{
	  /* TODO Need to check for overflow here. */
	  usize_t hash_values = stream->src->max_winsize /
	                        stream->smatcher.large_step;

	  if ((ret = xd3_size_hashtable (stream,
					 hash_values,
					 stream->smatcher.large_look,
					 & stream->large_hash)))
	    {
	      return ret;
	    }
	}

      if (small_comp)
	{
	  /* TODO: This is under devel: used to have min (sprevsz) here, which sort
	   * of makes sense, but observed fast performance w/ larger tables, which
	   * also sort of makes sense. @@@ */
	  usize_t hash_values = stream->winsize;

	  if ((ret = xd3_size_hashtable (stream,
					 hash_values,
					 stream->smatcher.small_look,
					 & stream->small_hash)))
	    {
	      return ret;
	    }
	}
    }

  /* data buffers */
  for (i = 0; i < ENC_SECTS; i += 1)
    {
      if ((stream->enc_heads[i] =
	   stream->enc_tails[i] =
	   xd3_alloc_output (stream, NULL)) == NULL)
	{
	  return ENOMEM;
	}
    }

  /* iopt buffer */
  xd3_rlist_init (& stream->iopt_used);
  xd3_rlist_init (& stream->iopt_free);

  if (xd3_alloc_iopt (stream, stream->iopt_size) != 0) { goto fail; }

  XD3_ASSERT (xd3_rlist_length (& stream->iopt_free) == stream->iopt_size);
  XD3_ASSERT (xd3_rlist_length (& stream->iopt_used) == 0);

  /* address cache, code table */
  stream->acache.s_near = stream->code_table_desc->near_modes;
  stream->acache.s_same = stream->code_table_desc->same_modes;
  stream->code_table    = stream->code_table_func ();

  return xd3_alloc_cache (stream);

 fail:

  return ENOMEM;
}

int
xd3_encode_init_full (xd3_stream *stream)
{
  return xd3_encode_init (stream, 1);
}

int
xd3_encode_init_partial (xd3_stream *stream)
{
  return xd3_encode_init (stream, 0);
}

/* Called after the ENC_POSTOUT state, this puts the output buffers
 * back into separate lists and re-initializes some variables.  (The
 * output lists were spliced together during the ENC_FLUSH state.) */
static void
xd3_encode_reset (xd3_stream *stream)
{
  int i;
  xd3_output *olist;

  stream->avail_in     = 0;
  stream->small_reset  = 1;
  stream->i_slots_used = 0;

  if (stream->src != NULL)
    {
      stream->src->srcbase   = 0;
      stream->src->srclen    = 0;
      stream->srcwin_decided = 0;
      stream->srcwin_decided_early = 0;
      stream->match_minaddr  = 0;
      stream->match_maxaddr  = 0;
      stream->taroff         = 0;
    }

  /* Reset output chains. */
  olist = stream->enc_heads[0];

  for (i = 0; i < ENC_SECTS; i += 1)
    {
      XD3_ASSERT (olist != NULL);

      stream->enc_heads[i] = olist;
      stream->enc_tails[i] = olist;
      olist = olist->next_page;

      stream->enc_heads[i]->next = 0;
      stream->enc_heads[i]->next_page = NULL;

      stream->enc_tails[i]->next_page = NULL;
      stream->enc_tails[i] = stream->enc_heads[i];
    }

  xd3_freelist_output (stream, olist);
}

/* The main encoding routine. */
int
xd3_encode_input (xd3_stream *stream)
{
  int ret, i;

  if (stream->dec_state != 0)
    {
      stream->msg = "encoder/decoder transition";
      return XD3_INTERNAL;
    }

  switch (stream->enc_state)
    {
    case ENC_INIT:
      /* Only reached on first time through: memory setup. */
      if ((ret = xd3_encode_init_full (stream))) { return ret; }

      stream->enc_state = ENC_INPUT;

    case ENC_INPUT:

      /* If there is no input yet, just return.  This checks for
       * next_in == NULL, not avail_in == 0 since zero bytes is a
       * valid input.  There is an assertion in xd3_avail_input() that
       * next_in != NULL for this reason.  By returning right away we
       * avoid creating an input buffer before the caller has supplied
       * its first data.  It is possible for xd3_avail_input to be
       * called both before and after the first call to
       * xd3_encode_input(). */
      if (stream->next_in == NULL)
	{
	  return XD3_INPUT;
	}

    enc_flush:
      /* See if we should buffer the input: either if there is already
       * a leftover buffer, or if the input is short of winsize
       * without flush.  The label at this point is reached by a goto
       * below, when there is leftover input after postout. */
      if ((stream->buf_leftover != NULL) ||
	  (stream->buf_avail != 0) ||
	  (stream->avail_in < stream->winsize && ! (stream->flags & XD3_FLUSH)))
	{
	  if ((ret = xd3_encode_buffer_leftover (stream))) { return ret; }
	}

      /* Initalize the address cache before each window. */
      xd3_init_cache (& stream->acache);

      stream->input_position    = 0;
      stream->min_match = MIN_MATCH;
      stream->unencoded_offset = 0;

      stream->enc_state = ENC_SEARCH;

      IF_DEBUG2 (DP(RINT "[WINSTART:%"Q"u] input bytes %"W"u offset %"Q"u\n",
		    stream->current_window, stream->avail_in,
		    stream->total_in));
      return XD3_WINSTART;

    case ENC_SEARCH:
      IF_DEBUG2 (DP(RINT "[SEARCH] match_state %d avail_in %"W"u %s\n",
		    stream->match_state, stream->avail_in,
		    stream->src ? "source" : "no source"));

      /* Reentrant matching. */
      if (stream->src != NULL)
	{
	  switch (stream->match_state)
	    {
	    case MATCH_TARGET:
	      /* Try matching forward at the start of the target.
	       * This is entered the first time through, to check for
	       * a perfect match, and whenever there is a source match
	       * that extends to the end of the previous window.  The
	       * match_srcpos field is initially zero and later set
	       * during xd3_source_extend_match. */

	      if (stream->avail_in > 0)
		{
		  /* This call can't fail because the source window is
		   * unrestricted. */
		  ret = xd3_source_match_setup (stream, stream->match_srcpos);
		  XD3_ASSERT (ret == 0);
		  stream->match_state = MATCH_FORWARD;
		}
	      else
		{
		  stream->match_state = MATCH_SEARCHING;
		  stream->match_fwd = 0;
		}
	      XD3_ASSERT (stream->match_fwd == 0);

	    case MATCH_FORWARD:
	    case MATCH_BACKWARD:
	      if (stream->avail_in != 0)
		{
		  if ((ret = xd3_source_extend_match (stream)) != 0)
		    {
		      return ret;
		    }

		  /* The search has to make forward progress here
		   * or else it can get stuck in a match-backward
		   * (getsrcblk) then match-forward (getsrcblk),
		   * find insufficient match length, then repeat

		   * exactly the same search.
		   */
		  stream->input_position += stream->match_fwd;
		}

	    case MATCH_SEARCHING:
	      /* Continue string matching.  (It's possible that the
	       * initial match continued through the entire input, in
	       * which case we're still in MATCH_FORWARD and should
	       * remain so for the next input window.) */
	      break;
	    }
	}

      /* String matching... */
      if (stream->avail_in != 0 &&
	  (ret = stream->smatcher.string_match (stream)))
	{
	  return ret;
	}

      stream->enc_state = ENC_INSTR;

    case ENC_INSTR:
      /* Note: Jump here to encode VCDIFF deltas w/o using this
       * string-matching code.  Merging code enters here. */

      /* Flush the instrution buffer, then possibly add one more
       * instruction, then emit the header. */
      if ((ret = xd3_iopt_flush_instructions (stream, 1)) ||
          (ret = xd3_iopt_add_finalize (stream)))
	{
	  return ret;
	}

      stream->enc_state = ENC_FLUSH;

    case ENC_FLUSH:
      /* Note: main_recode_func() bypasses string-matching by setting
       * ENC_FLUSH. */
      if ((ret = xd3_emit_hdr (stream)))
	{
	  return ret;
	}

      /* Begin output. */
      stream->enc_current = HDR_HEAD (stream);

      /* Chain all the outputs together.  After doing this, it looks
       * as if there is only one section.  The other enc_heads are set
       * to NULL to avoid freeing them more than once. */
       for (i = 1; i < ENC_SECTS; i += 1)
	{
	  stream->enc_tails[i-1]->next_page = stream->enc_heads[i];
	  stream->enc_heads[i] = NULL;
	}

    enc_output:

      stream->enc_state  = ENC_POSTOUT;
      stream->next_out   = stream->enc_current->base;
      stream->avail_out  = stream->enc_current->next;
      stream->total_out += stream->avail_out;

      /* If there is any output in this buffer, return it, otherwise
       * fall through to handle the next buffer or finish the window
       * after all buffers have been output. */
      if (stream->avail_out > 0)
	{
	  /* This is the only place xd3_encode returns XD3_OUTPUT */
	  return XD3_OUTPUT;
	}

    case ENC_POSTOUT:

      if (stream->avail_out != 0)
	{
	  stream->msg = "missed call to consume output";
	  return XD3_INTERNAL;
	}

      /* Continue outputting one buffer at a time, until the next is NULL. */
      if ((stream->enc_current = stream->enc_current->next_page) != NULL)
	{
	  goto enc_output;
	}

      stream->total_in += stream->avail_in;
      stream->enc_state = ENC_POSTWIN;

      IF_DEBUG2 (DP(RINT "[WINFINISH:%"Q"u] in=%"Q"u\n",
		    stream->current_window,
		    stream->total_in));
      return XD3_WINFINISH;

    case ENC_POSTWIN:

      xd3_encode_reset (stream);

      stream->current_window += 1;
      stream->enc_state = ENC_INPUT;

      /* If there is leftover input to flush, repeat. */
      if (stream->buf_leftover != NULL)
	{
	  goto enc_flush;
	}

      /* Ready for more input. */
      return XD3_INPUT;

    default:
      stream->msg = "invalid state";
      return XD3_INTERNAL;
    }
}
#endif /* XD3_ENCODER */

/*****************************************************************
 Client convenience functions
 ******************************************************************/

int
xd3_process_stream (int            is_encode,
		    xd3_stream    *stream,
		    int          (*func) (xd3_stream *),
		    int            close_stream,
		    const uint8_t *input,
		    usize_t        input_size,
		    uint8_t       *output,
		    usize_t       *output_size,
		    usize_t        output_size_max)
{
  usize_t ipos = 0;
  usize_t n = xd3_min (stream->winsize, input_size);

  (*output_size) = 0;

  stream->flags |= XD3_FLUSH;

  xd3_avail_input (stream, input + ipos, n);
  ipos += n;

  for (;;)
    {
      int ret;
      switch ((ret = func (stream)))
	{
	case XD3_OUTPUT: { /* memcpy below */ break; }
	case XD3_INPUT: {
	  n = xd3_min(stream->winsize, input_size - ipos);
	  if (n == 0) 
	    {
	      goto done;
	    }
	  xd3_avail_input (stream, input + ipos, n);
	  ipos += n;
	  continue;
	}
	case XD3_GOTHEADER: { /* ignore */ continue; }
	case XD3_WINSTART: { /* ignore */ continue; }
	case XD3_WINFINISH: { /* ignore */ continue; }
	case XD3_GETSRCBLK:
	  {
	    /* When the getblk function is NULL, it is necessary to
	     * provide the complete source as a single block using
	     * xd3_set_source_and_size, otherwise this error.  The
	     * library should never ask for another source block. */
	    stream->msg = "library requested source block";
	    return XD3_INTERNAL;
	  }
	case 0:
	  {
	    /* xd3_encode_input/xd3_decode_input never return 0 */
	    stream->msg = "invalid return: 0";
	    return XD3_INTERNAL;
	  }
	default:
	  return ret;
	}

      if (*output_size + stream->avail_out > output_size_max)
	{
	  stream->msg = "insufficient output space";
	  return ENOSPC;
	}

      memcpy (output + *output_size, stream->next_out, stream->avail_out);

      *output_size += stream->avail_out;

      xd3_consume_output (stream);
    }
 done:
  return (close_stream == 0) ? 0 : xd3_close_stream (stream);
}

static int
xd3_process_memory (int            is_encode,
		    int          (*func) (xd3_stream *),
		    const uint8_t *input,
		    usize_t        input_size,
		    const uint8_t *source,
		    usize_t        source_size,
		    uint8_t       *output,
		    usize_t       *output_size,
		    usize_t        output_size_max,
		    int            flags) {
  xd3_stream stream;
  xd3_config config;
  xd3_source src;
  int ret;

  memset (& stream, 0, sizeof (stream));
  memset (& config, 0, sizeof (config));

  if (input == NULL || output == NULL) {
    stream.msg = "invalid input/output buffer";
    ret = XD3_INTERNAL;
    goto exit;
  }

  config.flags = flags;

  if (is_encode)
    {
      config.winsize = xd3_min(input_size, (usize_t) XD3_DEFAULT_WINSIZE);
      config.sprevsz = xd3_pow2_roundup (config.winsize);
    }

  if ((ret = xd3_config_stream (&stream, &config)) != 0)
    {
      goto exit;
    }

  if (source != NULL)
    {
      memset (& src, 0, sizeof (src));

      src.blksize = source_size;
      src.onblk = source_size;
      src.curblk = source;
      src.curblkno = 0;
      src.max_winsize = source_size;

      if ((ret = xd3_set_source_and_size (&stream, &src, source_size)) != 0)
	{
	  goto exit;
	}
    }

  if ((ret = xd3_process_stream (is_encode,
				 & stream,
				 func, 1,
				 input, input_size,
				 output,
				 output_size,
				 output_size_max)) != 0)
    {
      goto exit;
    }

 exit:
  if (ret != 0)
    {
      IF_DEBUG2 (DP(RINT "process_memory: %d: %s\n", ret, stream.msg));
    }
  xd3_free_stream(&stream);
  return ret;
}

int
xd3_decode_stream (xd3_stream    *stream,
		   const uint8_t *input,
		   usize_t        input_size,
		   uint8_t       *output,
		   usize_t       *output_size,
		   usize_t        output_size_max)
{
  return xd3_process_stream (0, stream, & xd3_decode_input, 1,
			     input, input_size,
			     output, output_size, output_size_max);
}

int
xd3_decode_memory (const uint8_t *input,
		   usize_t        input_size,
		   const uint8_t *source,
		   usize_t        source_size,
		   uint8_t       *output,
		   usize_t       *output_size,
		   usize_t        output_size_max,
		   int            flags) {
  return xd3_process_memory (0, & xd3_decode_input,
			     input, input_size,
			     source, source_size,
			     output, output_size, output_size_max,
			     flags);
}


#if XD3_ENCODER
int
xd3_encode_stream (xd3_stream    *stream,
		   const uint8_t *input,
		   usize_t         input_size,
		   uint8_t       *output,
		   usize_t        *output_size,
		   usize_t         output_size_max)
{
  return xd3_process_stream (1, stream, & xd3_encode_input, 1,
			     input, input_size,
			     output, output_size, output_size_max);
}

int
xd3_encode_memory (const uint8_t *input,
		   usize_t        input_size,
		   const uint8_t *source,
		   usize_t        source_size,
		   uint8_t       *output,
		   usize_t        *output_size,
		   usize_t        output_size_max,
		   int            flags) {
  return xd3_process_memory (1, & xd3_encode_input,
			     input, input_size,
			     source, source_size,
			     output, output_size, output_size_max,
			     flags);
}
#endif


/*************************************************************
 String matching helpers
 *************************************************************/

#if XD3_ENCODER
/* Do the initial xd3_string_match() checksum table setup.
 * Allocations are delayed until first use to avoid allocation
 * sometimes (e.g., perfect matches, zero-length inputs). */
static int
xd3_string_match_init (xd3_stream *stream)
{
  const int DO_SMALL = ! (stream->flags & XD3_NOCOMPRESS);
  const int DO_LARGE = (stream->src != NULL);

  if (DO_LARGE && stream->large_table == NULL)
    {
      if ((stream->large_table =
	   (usize_t*) xd3_alloc0 (stream, stream->large_hash.size, sizeof (usize_t))) == NULL)
	{
	  return ENOMEM;
	}
    }

  if (DO_SMALL)
    {
      /* Subsequent calls can return immediately after checking reset. */
      if (stream->small_table != NULL)
	{
	  /* The target hash table is reinitialized once per window. */
	  /* TODO: This would not have to be reinitialized if absolute
	   * offsets were being stored. */
	  if (stream->small_reset)
	    {
	      stream->small_reset = 0;
	      memset (stream->small_table, 0,
		      sizeof (usize_t) * stream->small_hash.size);
	    }

	  return 0;
	}

      if ((stream->small_table =
	   (usize_t*) xd3_alloc0 (stream,
				  stream->small_hash.size,
				  sizeof (usize_t))) == NULL)
	{
	  return ENOMEM;
	}

      /* If there is a previous table needed. */
      if (stream->smatcher.small_lchain > 1 ||
	  stream->smatcher.small_chain > 1)
	{
	  if ((stream->small_prev =
	       (xd3_slist*) xd3_alloc (stream,
				       stream->sprevsz,
				       sizeof (xd3_slist))) == NULL)
	    {
	      return ENOMEM;
	    }
	}
    }

  return 0;
}

#if XD3_USE_LARGEFILE64 && !XD3_USE_LARGESIZET
/* This function handles the 32/64bit ambiguity -- file positions are 64bit
 * but the hash table for source-offsets is 32bit. */
static xoff_t
xd3_source_cksum_offset(xd3_stream *stream, usize_t low)
{
  xoff_t scp = stream->srcwin_cksum_pos;
  xoff_t s0 = scp >> 32;

  usize_t sr = (usize_t) scp;

  if (s0 == 0) {
    return low;
  }

  /* This should not be >= because srcwin_cksum_pos is the next
   * position to index. */
  if (low > sr) {
    return (--s0 << 32) | low;
  }

  return (s0 << 32) | low;
}
#else
static xoff_t
xd3_source_cksum_offset(xd3_stream *stream, usize_t low)
{
  return low;
}
#endif

/* This function sets up the stream->src fields srcbase, srclen.  The
 * call is delayed until these values are needed to encode a copy
 * address.  At this point the decision has to be made. */
static int
xd3_srcwin_setup (xd3_stream *stream)
{
  xd3_source *src = stream->src;
  xoff_t length, x;

  /* Check the undecided state. */
  XD3_ASSERT (src->srclen == 0 && src->srcbase == 0);

  /* Avoid repeating this call. */
  stream->srcwin_decided = 1;

  /* If the stream is flushing, then the iopt buffer was able to
   * contain the complete encoding.  If no copies were issued no
   * source window is actually needed.  This prevents the VCDIFF
   * header from including source base/len.  xd3_emit_hdr checks for
   * srclen == 0. */
  if (stream->enc_state == ENC_INSTR && stream->match_maxaddr == 0)
    {
      goto done;
    }

  /* Check for overflow, srclen is usize_t - this can't happen unless
   * XD3_DEFAULT_SRCBACK and related parameters are extreme - should
   * use smaller windows. */
  length = stream->match_maxaddr - stream->match_minaddr;

  x = USIZE_T_MAX;
  if (length > x)
    {
      stream->msg = "source window length overflow (not 64bit)";
      return XD3_INTERNAL;
    }

  /* If ENC_INSTR, then we know the exact source window to use because
   * no more copies can be issued. */
  if (stream->enc_state == ENC_INSTR)
    {
      src->srcbase = stream->match_minaddr;
      src->srclen  = (usize_t) length;
      XD3_ASSERT (src->srclen);
      goto done;
    }

  /* Otherwise, we have to make a guess.  More copies may still be
   * issued, but we have to decide the source window base and length
   * now.  
   * TODO: This may not working well in practice, more testing needed. */
  src->srcbase = stream->match_minaddr;
  src->srclen  = xd3_max ((usize_t) length,
			  stream->avail_in + (stream->avail_in >> 2));

  if (src->eof_known)
    {
      /* Note: if the source size is known, we must reduce srclen or
       * code that expects to pass a single block w/ getblk == NULL
       * will not function, as the code will return GETSRCBLK asking
       * for the second block. */
      src->srclen = xd3_min (src->srclen, xd3_source_eof(src) - src->srcbase);
    }
  IF_DEBUG1 (DP(RINT "[srcwin_setup_constrained] base %"Q"u len %"W"u\n",
		src->srcbase, src->srclen));

  XD3_ASSERT (src->srclen);
 done:
  /* Set the taroff.  This convenience variable is used even when
     stream->src == NULL. */
  stream->taroff = src->srclen;
  return 0;
}

/* Sets the bounding region for a newly discovered source match, prior
 * to calling xd3_source_extend_match().  This sets the match_maxfwd,
 * match_maxback variables.  Note: srcpos is an absolute position
 * (xoff_t) but the match_maxfwd, match_maxback variables are usize_t.
 * Returns 0 if the setup succeeds, or 1 if the source position lies
 * outside an already-decided srcbase/srclen window. */
static int
xd3_source_match_setup (xd3_stream *stream, xoff_t srcpos)
{
  xd3_source *const src = stream->src;
  usize_t greedy_or_not;

  stream->match_maxback = 0;
  stream->match_maxfwd  = 0;
  stream->match_back    = 0;
  stream->match_fwd     = 0;

  /* This avoids a non-blocking endless loop caused by scanning
   * backwards across a block boundary, only to find not enough
   * matching bytes to beat the current min_match due to a better lazy
   * target match: the re-entry to xd3_string_match() repeats the same
   * long match because the input position hasn't changed.  TODO: if
   * ever duplicates are added to the source hash table, this logic
   * won't suffice to avoid loops.  See testing/regtest.cc's
   * TestNonBlockingProgress test! */
  if (srcpos != 0 && srcpos == stream->match_last_srcpos)
    {
      IF_DEBUG2(DP(RINT "[match_setup] looping failure\n"));
      goto bad;
    }

  /* Implement src->max_winsize, which prevents the encoder from seeking
   * back further than the LRU cache maintaining FIFO discipline, (to
   * avoid seeking). */
  if (srcpos < stream->srcwin_cksum_pos &&
      stream->srcwin_cksum_pos - srcpos > src->max_winsize)
    {
      IF_DEBUG2(DP(RINT "[match_setup] rejected due to src->max_winsize "
		   "distance eof=%"Q"u srcpos=%"Q"u max_winsz=%"Q"u\n",
		   xd3_source_eof (src),
		   srcpos, src->max_winsize));
      goto bad;
    }

  /* There are cases where the above test does not reject a match that
   * will experience XD3_TOOFARBACK at the first xd3_getblk call
   * because the input may have advanced up to one block beyond the
   * actual EOF. */
  IF_DEBUG2(DP(RINT "[match_setup] %"Q"u srcpos %"Q"u, "
	       "src->max_winsize %"Q"u\n",
	       stream->total_in + stream->input_position,
	       srcpos, src->max_winsize));

  /* Going backwards, the 1.5-pass algorithm allows some
   * already-matched input may be covered by a longer source match.
   * The greedy algorithm does not allow this.
   * TODO: Measure this. */
  if (stream->flags & XD3_BEGREEDY)
    {
      /* The greedy algorithm allows backward matching to the last
       * matched position. */
      greedy_or_not = xd3_iopt_last_matched (stream);
    }
  else
    {
      /* The 1.5-pass algorithm allows backward matching to go back as
       * far as the unencoded offset, which is updated as instructions
       * pass out of the iopt buffer.  If this (default) is chosen, it
       * means xd3_iopt_erase may be called to eliminate instructions
       * when a covering source match is found. */
      greedy_or_not = stream->unencoded_offset;
    }

  /* Backward target match limit. */
  XD3_ASSERT (stream->input_position >= greedy_or_not);
  stream->match_maxback = stream->input_position - greedy_or_not;

  /* Forward target match limit. */
  XD3_ASSERT (stream->avail_in > stream->input_position);
  stream->match_maxfwd = stream->avail_in - stream->input_position;

  /* Now we take the source position into account.  It depends whether
   * the srclen/srcbase have been decided yet. */
  if (stream->srcwin_decided == 0)
    {
      /* Unrestricted case: the match can cover the entire source,
       * 0--src->size.  We compare the usize_t
       * match_maxfwd/match_maxback against the xoff_t
       * src->size/srcpos values and take the min. */
      /* TODO #if XD3_USE_LARGESIZET ? */
      if (srcpos < stream->match_maxback)
	{
	  stream->match_maxback = (usize_t) srcpos;
	}

      if (src->eof_known)
	{
	  xoff_t srcavail = xd3_source_eof (src) - srcpos;

	  if (srcavail < stream->match_maxfwd)
	    {
	      stream->match_maxfwd = (usize_t) srcavail;
	    }
	}

      IF_DEBUG2(DP(RINT
		   "[match_setup] srcpos %"Q"u (tgtpos %"Q"u) "
		   "unrestricted maxback %"W"u maxfwd %"W"u\n",
		   srcpos,
		   stream->total_in + stream->input_position,
		   stream->match_maxback,
		   stream->match_maxfwd));
      goto good;
    }

  /* Decided some source window. */
  XD3_ASSERT (src->srclen > 0);

  /* Restricted case: fail if the srcpos lies outside the source window */
  if ((srcpos < src->srcbase) ||
      (srcpos > (src->srcbase + src->srclen)))
    {
      IF_DEBUG1(DP(RINT "[match_setup] restricted source window failure\n"));
      goto bad;
    }
  else
    {
      usize_t srcavail;

      srcavail = (usize_t) (srcpos - src->srcbase);
      if (srcavail < stream->match_maxback)
	{
	  stream->match_maxback = srcavail;
	}

      srcavail = src->srcbase + src->srclen - srcpos;
      if (srcavail < stream->match_maxfwd)
	{
	  stream->match_maxfwd = srcavail;
	}

      IF_DEBUG2(DP(RINT
		   "[match_setup] srcpos %"Q"u (tgtpos %"Q"u) "
		   "restricted maxback %"W"u maxfwd %"W"u\n",
		   srcpos,
		   stream->total_in + stream->input_position,
		   stream->match_maxback,
		   stream->match_maxfwd));
      goto good;
    }

 good:
  stream->match_state  = MATCH_BACKWARD;
  stream->match_srcpos = srcpos;
  stream->match_last_srcpos = srcpos;
  return 0;

 bad:
  stream->match_state  = MATCH_SEARCHING;
  stream->match_last_srcpos = srcpos;
  return 1;
}

static inline usize_t
xd3_forward_match(const uint8_t *s1c, const uint8_t *s2c, usize_t n)
{
  usize_t i = 0;
#if UNALIGNED_OK
  usize_t nint = n / sizeof(int);

  if (nint >> 3)
    {
      usize_t j = 0;
      const int *s1 = (const int*)s1c;
      const int *s2 = (const int*)s2c;
      usize_t nint_8 = nint - 8;

      while (i <= nint_8 &&
	     s1[i++] == s2[j++] &&
	     s1[i++] == s2[j++] &&
	     s1[i++] == s2[j++] &&
	     s1[i++] == s2[j++] &&
	     s1[i++] == s2[j++] &&
	     s1[i++] == s2[j++] &&
	     s1[i++] == s2[j++] &&
	     s1[i++] == s2[j++]) { }

      i = (i - 1) * sizeof(int);
    }
#endif

  while (i < n && s1c[i] == s2c[i])
    {
      i++;
    }
  return i;
}

/* This function expands the source match backward and forward.  It is
 * reentrant, since xd3_getblk may return XD3_GETSRCBLK, so most
 * variables are kept in xd3_stream.  There are two callers of this
 * function, the string_matching routine when a checksum match is
 * discovered, and xd3_encode_input whenever a continuing (or initial)
 * match is suspected.  The two callers do different things with the
 * input_position, thus this function leaves that variable untouched.
 * If a match is taken the resulting stream->match_fwd is left
 * non-zero. */
static int
xd3_source_extend_match (xd3_stream *stream)
{
  int ret;
  xd3_source *const src = stream->src;
  xoff_t matchoff;  /* matchoff is the current right/left-boundary of
		       the source match being tested. */
  usize_t streamoff; /* streamoff is the current right/left-boundary
			of the input match being tested. */
  xoff_t tryblk;    /* tryblk, tryoff are the block, offset position
		       of matchoff */
  usize_t tryoff;
  usize_t tryrem;    /* tryrem is the number of matchable bytes */
  usize_t matched;

  IF_DEBUG2(DP(RINT "[extend match] srcpos %"Q"u\n",
	       stream->match_srcpos));

  XD3_ASSERT (src != NULL);

  /* Does it make sense to compute backward match AFTER forward match? */
  if (stream->match_state == MATCH_BACKWARD)
    {
      /* Note: this code is practically duplicated below, substituting
       * match_fwd/match_back and direction. */
      matchoff  = stream->match_srcpos - stream->match_back;
      streamoff = stream->input_position - stream->match_back;
      xd3_blksize_div (matchoff, src, &tryblk, &tryoff);

      /* this loops backward over source blocks */
      while (stream->match_back < stream->match_maxback)
	{
	  /* see if we're backing across a source block boundary */
	  if (tryoff == 0)
	    {
	      tryoff  = src->blksize;
	      tryblk -= 1;
	    }

	  if ((ret = xd3_getblk (stream, tryblk)))
	    {
	      if (ret == XD3_TOOFARBACK)
		{
		  IF_DEBUG2(DP(RINT "[maxback] %"Q"u TOOFARBACK: %"W"u INP %"Q"u CKSUM %"Q"u\n",
			       tryblk, stream->match_back,
			       stream->total_in + stream->input_position,
			       stream->srcwin_cksum_pos));

		  /* the starting position is too far back. */
		  if (stream->match_back == 0)
		    {
		      XD3_ASSERT(stream->match_fwd == 0);
		      goto donefwd;
		    }

		  /* search went too far back, continue forward. */
		  goto doneback;
		}

	      /* could be a XD3_GETSRCBLK failure. */
	      return ret;
	    }

	  tryrem = xd3_min (tryoff, stream->match_maxback - stream->match_back);

	  IF_DEBUG2(DP(RINT "[maxback] maxback %"W"u trysrc %"Q"u/%"W"u tgt %"W"u tryrem %"W"u\n",
		       stream->match_maxback, tryblk, tryoff, streamoff, tryrem));

	  /* TODO: This code can be optimized similar to xd3_match_forward() */
	  for (; tryrem != 0; tryrem -= 1, stream->match_back += 1)
	    {
	      if (src->curblk[tryoff-1] != stream->next_in[streamoff-1])
		{
		  goto doneback;
		}

	      tryoff    -= 1;
	      streamoff -= 1;
	    }
	}

    doneback:
      stream->match_state = MATCH_FORWARD;
    }

  XD3_ASSERT (stream->match_state == MATCH_FORWARD);

  matchoff  = stream->match_srcpos + stream->match_fwd;
  streamoff = stream->input_position + stream->match_fwd;
  xd3_blksize_div (matchoff, src, & tryblk, & tryoff);

  /* Note: practically the same code as backwards case above: same comments */
  while (stream->match_fwd < stream->match_maxfwd)
    {
      if (tryoff == src->blksize)
	{
	  tryoff  = 0;
	  tryblk += 1;
	}

      if ((ret = xd3_getblk (stream, tryblk)))
	{
	  if (ret == XD3_TOOFARBACK)
	    {
	      IF_DEBUG2(DP(RINT "[maxfwd] %"Q"u TOOFARBACK: %"W"u INP %"Q"u CKSUM %"Q"u\n",
			   tryblk, stream->match_fwd,
			   stream->total_in + stream->input_position,
			   stream->srcwin_cksum_pos));
	      goto donefwd;
	    }

	  /* could be a XD3_GETSRCBLK failure. */
	  return ret;
	}

      tryrem = xd3_min(stream->match_maxfwd - stream->match_fwd,
		   src->onblk - tryoff);

      if (tryrem == 0)
	{
	  /* Generally, this means we have a power-of-two size source
	   * and we just found the end-of-file, in this case it's an
	   * empty block. */
	  XD3_ASSERT (src->onblk < src->blksize);
	  break;
	}

      matched = xd3_forward_match(src->curblk + tryoff,
				  stream->next_in + streamoff,
				  tryrem);
      tryoff += matched;
      streamoff += matched;
      stream->match_fwd += matched;

      if (tryrem != matched)
	{
	  break;
	}
    }

 donefwd:
  stream->match_state = MATCH_SEARCHING;

  IF_DEBUG2(DP(RINT "[extend match] input %"Q"u srcpos %"Q"u len %"W"u\n",
	       stream->input_position + stream->total_in,
	       stream->match_srcpos,
	       stream->match_fwd));

  /* If the match ends short of the last instruction end, we probably
   * don't want it.  There is the possibility that a copy ends short
   * of the last copy but also goes further back, in which case we
   * might want it.  This code does not implement such: if so we would
   * need more complicated xd3_iopt_erase logic. */
  if (stream->match_fwd < stream->min_match)
    {
      stream->match_fwd = 0;
    }
  else
    {
      usize_t total  = stream->match_fwd + stream->match_back;

      /* Correct the variables to remove match_back from the equation. */
      usize_t target_position = stream->input_position - stream->match_back;
      usize_t match_length   = stream->match_back      + stream->match_fwd;
      xoff_t match_position  = stream->match_srcpos    - stream->match_back;
      xoff_t match_end       = stream->match_srcpos    + stream->match_fwd;

      /* At this point we may have to erase any iopt-buffer
       * instructions that are fully covered by a backward-extending
       * copy. */
      if (stream->match_back > 0)
	{
	  xd3_iopt_erase (stream, target_position, total);
	}

      stream->match_back = 0;

      /* Update ranges.  The first source match occurs with both
	 values set to 0. */
      if (stream->match_maxaddr == 0 ||
	  match_position < stream->match_minaddr)
	{
	  stream->match_minaddr = match_position;
	}

      if (match_end > stream->match_maxaddr)
	{
	  /* Note: per-window */
	  stream->match_maxaddr = match_end;
	}

      if (match_end > stream->maxsrcaddr)
	{
	  /* Note: across windows */
	  stream->maxsrcaddr = match_end;
	}

      IF_DEBUG2 ({
	static int x = 0;
	DP(RINT "[source match:%d] length %"W"u <inp %"Q"u %"Q"u>  <src %"Q"u %"Q"u> (%s)\n",
	   x++,
	   match_length,
	   stream->total_in + target_position,
	   stream->total_in + target_position + match_length,
	   match_position,
	   match_position + match_length,
	   (stream->total_in + target_position == match_position) ? "same" : "diff");
      });

      if ((ret = xd3_found_match (stream,
				  /* decoder position */ target_position,
				  /* length */ match_length,
				  /* address */ match_position,
				  /* is_source */ 1)))
	{
	  return ret;
	}

      /* If the match ends with the available input: */
      if (target_position + match_length == stream->avail_in)
	{
	  /* Setup continuing match for the next window. */
	  stream->match_state  = MATCH_TARGET;
	  stream->match_srcpos = match_end;
	}
    }

  return 0;
}

/* Update the small hash.  Values in the small_table are offset by
 * HASH_CKOFFSET (1) to distinguish empty buckets from real offsets. */
static void
xd3_scksum_insert (xd3_stream *stream,
		   usize_t inx,
		   usize_t scksum,
		   usize_t pos)
{
  /* If we are maintaining previous duplicates. */
  if (stream->small_prev)
    {
      usize_t    last_pos = stream->small_table[inx];
      xd3_slist *pos_list = & stream->small_prev[pos & stream->sprevmask];

      /* Note last_pos is offset by HASH_CKOFFSET. */
      pos_list->last_pos = last_pos;
    }

  /* Enter the new position into the hash bucket. */
  stream->small_table[inx] = pos + HASH_CKOFFSET;
}

#if XD3_DEBUG
static int
xd3_check_smatch (const uint8_t *ref0, const uint8_t *inp0,
		  const uint8_t *inp_max, usize_t cmp_len)
{
  usize_t i;

  for (i = 0; i < cmp_len; i += 1)
    {
      XD3_ASSERT (ref0[i] == inp0[i]);
    }

  if (inp0 + cmp_len < inp_max)
    {
      XD3_ASSERT (inp0[i] != ref0[i]);
    }

  return 1;
}
#endif /* XD3_DEBUG */

/* When the hash table indicates a possible small string match, it
 * calls this routine to find the best match.  The first matching
 * position is taken from the small_table, HASH_CKOFFSET is subtracted
 * to get the actual position.  After checking that match, if previous
 * linked lists are in use (because stream->smatcher.small_chain > 1),
 * previous matches are tested searching for the longest match.  If
 * (stream->min_match > MIN_MATCH) then a lazy match is in effect.
 */
static usize_t
xd3_smatch (xd3_stream *stream,
	    usize_t base,
	    usize_t scksum,
	    usize_t *match_offset)
{
  usize_t cmp_len;
  usize_t match_length = 0;
  usize_t chain = (stream->min_match == MIN_MATCH ?
                   stream->smatcher.small_chain :
                   stream->smatcher.small_lchain);
  const uint8_t *inp_max = stream->next_in + stream->avail_in;
  const uint8_t *inp;
  const uint8_t *ref;

  SMALL_HASH_DEBUG1 (stream, stream->next_in + stream->input_position);

  XD3_ASSERT (stream->min_match + stream->input_position <= stream->avail_in);

  base -= HASH_CKOFFSET;

 again:

  IF_DEBUG2 (DP(RINT "smatch at base=%"W"u inp=%"W"u cksum=%"W"u\n", base,
                stream->input_position, scksum));

  /* For small matches, we can always go to the end-of-input because
   * the matching position must be less than the input position. */
  XD3_ASSERT (base < stream->input_position);

  ref = stream->next_in + base;
  inp = stream->next_in + stream->input_position;

  SMALL_HASH_DEBUG2 (stream, ref);

  /* Expand potential match forward. */
  while (inp < inp_max && *inp == *ref)
    {
      ++inp;
      ++ref;
    }

  cmp_len = (usize_t)(inp - (stream->next_in + stream->input_position));

  /* Verify correctness */
  XD3_ASSERT (xd3_check_smatch (stream->next_in + base,
				stream->next_in + stream->input_position,
				inp_max, cmp_len));

  /* Update longest match */
  if (cmp_len > match_length)
    {
      ( match_length) = cmp_len;
      (*match_offset) = base;

      /* Stop if we match the entire input or have a long_enough match. */
      if (inp == inp_max || cmp_len >= stream->smatcher.long_enough)
	{
	  goto done;
	}
    }

  /* If we have not reached the chain limit, see if there is another
     previous position. */
  while (--chain != 0)
    {
      /* Calculate the previous offset. */
      usize_t prev_pos = stream->small_prev[base & stream->sprevmask].last_pos;
      usize_t diff_pos;

       if (prev_pos == 0)
 	{
 	  break;
 	}

      prev_pos -= HASH_CKOFFSET;

      if (prev_pos > base)
        {
          break;
        }

      base = prev_pos;

      XD3_ASSERT (stream->input_position > base);
      diff_pos = stream->input_position - base;

      /* Stop searching if we go beyond sprevsz, since those entries
       * are for unrelated checksum entries. */
      if (diff_pos & ~stream->sprevmask)
        {
          break;
        }

      goto again;
    }

 done:
  /* Crude efficiency test: if the match is very short and very far back, it's
   * unlikely to help, but the exact calculation requires knowing the state of
   * the address cache and adjacent instructions, which we can't do here.
   * Rather than encode a probably inefficient copy here and check it later
   * (which complicates the code a lot), do this:
   */
  if (match_length == 4 && stream->input_position - (*match_offset) >= 1<<14)
    {
      /* It probably takes >2 bytes to encode an address >= 2^14 from here */
      return 0;
    }
  if (match_length == 5 && stream->input_position - (*match_offset) >= 1<<21)
    {
      /* It probably takes >3 bytes to encode an address >= 2^21 from here */
      return 0;
    }

  /* It's unlikely that a window is large enough for the (match_length == 6 &&
   * address >= 2^28) check */
  return match_length;
}

#if XD3_DEBUG
static void
xd3_verify_small_state (xd3_stream    *stream,
			const uint8_t *inp,
			uint32_t       x_cksum)
{
  uint32_t state;
  uint32_t cksum = xd3_scksum (&state, inp, stream->smatcher.small_look);

  XD3_ASSERT (cksum == x_cksum);
}

static void
xd3_verify_large_state (xd3_stream    *stream,
			const uint8_t *inp,
			usize_t        x_cksum)
{
  usize_t cksum = xd3_large_cksum (&stream->large_hash, inp, stream->smatcher.large_look);
  XD3_ASSERT (cksum == x_cksum);
}
static void
xd3_verify_run_state (xd3_stream    *stream,
		      const uint8_t *inp,
		      usize_t        x_run_l,
		      uint8_t       *x_run_c)
{
  usize_t slook = stream->smatcher.small_look;
  uint8_t run_c;
  usize_t run_l = xd3_comprun (inp, slook, &run_c);

  XD3_ASSERT (run_l == 0 || run_c == *x_run_c);
  XD3_ASSERT (x_run_l > slook || run_l == x_run_l);
}
#endif /* XD3_DEBUG */

/* This function computes more source checksums to advance the window.
 * Called at every entrance to the string-match loop and each time
 * stream->input_position reaches the value returned as
 * *next_move_point.  NB: this is one of the most expensive functions
 * in this code and also the most critical for good compression.
 */
static int
xd3_srcwin_move_point (xd3_stream *stream, usize_t *next_move_point)
{
  /* the source file is indexed until this point */
  xoff_t target_cksum_pos;
  /* the absolute target file input position */
  xoff_t absolute_input_pos;

  if (stream->src->eof_known)
    {
      xoff_t source_size = xd3_source_eof (stream->src);
      XD3_ASSERT(stream->srcwin_cksum_pos <= source_size);

      if (stream->srcwin_cksum_pos == source_size)
	{
	  *next_move_point = USIZE_T_MAX;
	  return 0;
	}
    }

  absolute_input_pos = stream->total_in + stream->input_position;

  /* Immediately read the entire window. 
   *
   * Note: this reverses a long held policy, at this point in the
   * code, of advancing relatively slowly as the input is read, which
   * results in better compression for very-similar inputs, but worse
   * compression where data is deleted near the beginning of the file.
   * 
   * The new policy is simpler, somewhat slower and can benefit, or
   * slightly worsen, compression performance. */
  if (absolute_input_pos < stream->src->max_winsize / 2)
    {
      target_cksum_pos = stream->src->max_winsize;
    }
  else
    {
      /* TODO: The addition of 2 blocks here is arbitrary.  Do a
       * better job of stream alignment based on observed source copy
       * addresses, and when both input sizes are known, the
       * difference in size. */
      target_cksum_pos = absolute_input_pos +
	stream->src->max_winsize / 2 +
	stream->src->blksize * 2;
      target_cksum_pos &= ~stream->src->maskby;
    }

  /* A long match may have extended past srcwin_cksum_pos.  Don't
   * start checksumming already-matched source data. */
  if (stream->maxsrcaddr > stream->srcwin_cksum_pos)
    {
      stream->srcwin_cksum_pos = stream->maxsrcaddr;
    }

  if (target_cksum_pos < stream->srcwin_cksum_pos)
    {
      target_cksum_pos = stream->srcwin_cksum_pos;
    }

  while (stream->srcwin_cksum_pos < target_cksum_pos &&
	 (!stream->src->eof_known ||
	  stream->srcwin_cksum_pos < xd3_source_eof (stream->src)))
    {
      xoff_t  blkno;
      xoff_t  blkbaseoffset;
      usize_t blkrem;
      ssize_t oldpos;  /* Using ssize_t because of a  */
      ssize_t blkpos;  /* do { blkpos-- }
			  while (blkpos >= oldpos); */
      int ret;
      xd3_blksize_div (stream->srcwin_cksum_pos,
		       stream->src, &blkno, &blkrem);
      oldpos = blkrem;

      if ((ret = xd3_getblk (stream, blkno)))
	{
	  /* TOOFARBACK should never occur here, since we read forward. */
	  if (ret == XD3_TOOFARBACK)
	    {
 	      ret = XD3_INTERNAL;
	    }

	  IF_DEBUG1 (DP(RINT
			"[srcwin_move_point] async getblk return for %"Q"u: %s\n",
			blkno, xd3_strerror (ret)));
	  return ret;
	}

      IF_DEBUG1 (DP(RINT
		    "[srcwin_move_point] block %"Q"u T=%"Q"u S=%"Q"u L=%"Q"u EOF=%"Q"u %s\n",
		    blkno,
		    stream->total_in + stream->input_position,
		    stream->srcwin_cksum_pos,
		    target_cksum_pos,
		    xd3_source_eof (stream->src),
		    stream->src->eof_known ? "known" : "unknown"));

      blkpos = xd3_bytes_on_srcblk (stream->src, blkno);

      if (blkpos < (ssize_t) stream->smatcher.large_look)
	{
	  stream->srcwin_cksum_pos = (blkno + 1) * stream->src->blksize;
	  IF_DEBUG2 (DP(RINT "[srcwin_move_point] continue (end-of-block): %"Z"d\n", blkpos));
	  continue;
	}

      /* This inserts checksums for the entire block, in reverse,
       * starting from the end of the block.  This logic does not test
       * stream->srcwin_cksum_pos because it always advances it to the
       * start of the next block.
       *
       * oldpos is the srcwin_cksum_pos within this block.  blkpos is
       * the number of bytes available.  Each iteration inspects
       * large_look bytes then steps back large_step bytes.  The
       * if-stmt above ensures at least one large_look of data. */
      blkpos -= stream->smatcher.large_look;
      blkbaseoffset = stream->src->blksize * blkno;

      do
	{
	  /* TODO: This would be significantly faster if the compiler
	   * knew stream->smatcher.large_look (which the template for
	   * xd3_string_match_* allows). */
	  usize_t cksum = xd3_large_cksum (&stream->large_hash, 
					   stream->src->curblk + blkpos,
					   stream->smatcher.large_look);
	  usize_t hval = xd3_checksum_hash (& stream->large_hash, cksum);

	  stream->large_table[hval] =
	    (usize_t) (blkbaseoffset +
		       (xoff_t)(blkpos + HASH_CKOFFSET));

	  IF_DEBUG (stream->large_ckcnt += 1);

	  blkpos -= stream->smatcher.large_step;
	}
      while (blkpos >= oldpos);

      stream->srcwin_cksum_pos = (blkno + 1) * stream->src->blksize;
    }

  IF_DEBUG1 (DP(RINT
		"[srcwin_move_point] exited loop T=%"Q"u "
		"S=%"Q"u EOF=%"Q"u %s\n",
		stream->total_in + stream->input_position,
		stream->srcwin_cksum_pos,
		xd3_source_eof (stream->src),
		stream->src->eof_known ? "known" : "unknown"));

  if (stream->src->eof_known)
    {
      xoff_t source_size = xd3_source_eof (stream->src);
      if (stream->srcwin_cksum_pos >= source_size)
	{
	  /* This invariant is needed for xd3_source_cksum_offset() */
	  stream->srcwin_cksum_pos = source_size;
	  *next_move_point = USIZE_T_MAX;
	  IF_DEBUG1 (DP(RINT
			"[srcwin_move_point] finished with source input\n"));
	  return 0;
	}
    }

  /* How long until this function should be called again. */
  XD3_ASSERT(stream->srcwin_cksum_pos >= target_cksum_pos);

  *next_move_point = stream->input_position +
    stream->src->blksize -
    ((stream->srcwin_cksum_pos - target_cksum_pos) & stream->src->maskby);
  
  IF_DEBUG2 (DP(RINT
		"[srcwin_move_point] finished T=%"Q"u "
		"S=%"Q"u L=%"Q"u EOF=%"Q"u %s again in %"W"u\n",
		stream->total_in + stream->input_position,
		stream->srcwin_cksum_pos,
		target_cksum_pos,
		xd3_source_eof (stream->src),
		stream->src->eof_known ? "known" : "unknown",
		*next_move_point - stream->input_position));

  return 0;
}

#endif /* XD3_ENCODER */

/********************************************************************
 TEMPLATE pass
 *********************************************************************/

#endif /* __XDELTA3_C_INLINE_PASS__ */
#ifdef __XDELTA3_C_TEMPLATE_PASS__

#if XD3_ENCODER

/********************************************************************
 Templates
 *******************************************************************/

/* Template macros */
#define XD3_TEMPLATE(x)      XD3_TEMPLATE2(x,TEMPLATE)
#define XD3_TEMPLATE2(x,n)   XD3_TEMPLATE3(x,n)
#define XD3_TEMPLATE3(x,n)   x ## n
#define XD3_STRINGIFY(x)     XD3_STRINGIFY2(x)
#define XD3_STRINGIFY2(x)    #x

static int XD3_TEMPLATE(xd3_string_match_) (xd3_stream *stream);

static const xd3_smatcher XD3_TEMPLATE(__smatcher_) =
{
  XD3_STRINGIFY(TEMPLATE),
  XD3_TEMPLATE(xd3_string_match_),
#if SOFTCFG == 1
  0, 0, 0, 0, 0, 0, 0
#else
  LLOOK, LSTEP, SLOOK, SCHAIN, SLCHAIN, MAXLAZY, LONGENOUGH
#endif
};

static int
XD3_TEMPLATE(xd3_string_match_) (xd3_stream *stream)
{
  const int      DO_SMALL = ! (stream->flags & XD3_NOCOMPRESS);
  const int      DO_LARGE = (stream->src != NULL);
  const int      DO_RUN   = (1);

  const uint8_t *inp;
  uint32_t       scksum = 0;
  uint32_t       scksum_state = 0;
  usize_t        lcksum = 0;
  usize_t        sinx;
  usize_t        linx;
  uint8_t        run_c;
  usize_t        run_l;
  int            ret;
  usize_t        match_length;
  usize_t        match_offset = 0;
  usize_t        next_move_point = 0;

  IF_DEBUG2(DP(RINT "[string_match] initial entry %"W"u\n", stream->input_position));

  /* If there will be no compression due to settings or short input,
   * skip it entirely. */
  if (! (DO_SMALL || DO_LARGE || DO_RUN) ||
      stream->input_position + SLOOK > stream->avail_in) { goto loopnomore; }

  if ((ret = xd3_string_match_init (stream))) { return ret; }

  /* The restartloop label is reached when the incremental loop state
   * needs to be reset. */
 restartloop:

  IF_DEBUG2(DP(RINT "[string_match] restartloop %"W"u\n", stream->input_position));

  /* If there is not enough input remaining for any kind of match,
     skip it. */
  if (stream->input_position + SLOOK > stream->avail_in) { goto loopnomore; }

  /* Now reset the incremental loop state: */

  /* The min_match variable is updated to avoid matching the same lazy
   * match over and over again.  For example, if you find a (small)
   * match of length 9 at one position, you will likely find a match
   * of length 8 at the next position. */
  if (xd3_iopt_last_matched (stream) > stream->input_position)
    {
      stream->min_match = xd3_max (MIN_MATCH,
				   1 + xd3_iopt_last_matched(stream) -
				   stream->input_position);
    }
  else
    {
      stream->min_match = MIN_MATCH;
    }

  /* The current input byte. */
  inp = stream->next_in + stream->input_position;

  /* Small match state. */
  if (DO_SMALL)
    {
      scksum = xd3_scksum (&scksum_state, inp, SLOOK);
    }

  /* Run state. */
  if (DO_RUN)
    {
      run_l = xd3_comprun (inp, SLOOK, & run_c);
    }

  /* Large match state.  We continue the loop even after not enough
   * bytes for LLOOK remain, so always check stream->input_position in
   * DO_LARGE code. */
  if (DO_LARGE && (stream->input_position + LLOOK <= stream->avail_in))
    {
      /* Source window: next_move_point is the point that
       * stream->input_position must reach before computing more
       * source checksum.  Note: this is called unconditionally
       * the first time after reentry, subsequent calls will be
       * avoided if next_move_point is > input_position */
      if ((ret = xd3_srcwin_move_point (stream, & next_move_point)))
	{
	  return ret;
	}

      lcksum = xd3_large_cksum (&stream->large_hash, inp, LLOOK);
    }

  /* TRYLAZYLEN: True if a certain length match should be followed by
   * lazy search.  This checks that LEN is shorter than MAXLAZY and
   * that there is enough leftover data to consider lazy matching.
   * "Enough" is set to 2 since the next match will start at the next
   * offset, it must match two extra characters. */
#define TRYLAZYLEN(LEN,POS,MAX) ((MAXLAZY) > 0 && (LEN) < (MAXLAZY) \
				 && (POS) + (LEN) <= (MAX) - 2)

  /* HANDLELAZY: This statement is called each time an instruciton is
   * emitted (three cases).  If the instruction is large enough, the
   * loop is restarted, otherwise lazy matching may ensue. */
#define HANDLELAZY(mlen) \
  if (TRYLAZYLEN ((mlen), (stream->input_position), (stream->avail_in))) \
    { stream->min_match = (mlen) + LEAST_MATCH_INCR; goto updateone; } \
  else \
    { stream->input_position += (mlen); goto restartloop; }

  /* Now loop over one input byte at a time until a match is found... */
  for (;; inp += 1, stream->input_position += 1)
    {
      /* Now we try three kinds of string match in order of expense:
       * run, large match, small match. */

      /* Expand the start of a RUN.  The test for (run_l == SLOOK)
       * avoids repeating this check when we pass through a run area
       * performing lazy matching.  The run is only expanded once when
       * the min_match is first reached.  If lazy matching is
       * performed, the run_l variable will remain inconsistent until
       * the first non-running input character is reached, at which
       * time the run_l may then again grow to SLOOK. */
      if (DO_RUN && run_l == SLOOK)
	{
	  usize_t max_len = stream->avail_in - stream->input_position;

	  IF_DEBUG (xd3_verify_run_state (stream, inp, run_l, &run_c));

	  while (run_l < max_len && inp[run_l] == run_c) { run_l += 1; }

	  /* Output a RUN instruction. */
	  if (run_l >= stream->min_match && run_l >= MIN_RUN)
	    {
	      if ((ret = xd3_emit_run (stream, stream->input_position,
				       run_l, &run_c))) { return ret; }

	      HANDLELAZY (run_l);
	    }
	}

      /* If there is enough input remaining. */
      if (DO_LARGE && (stream->input_position + LLOOK <= stream->avail_in))
	{
	  if ((stream->input_position >= next_move_point) &&
	      (ret = xd3_srcwin_move_point (stream, & next_move_point)))
	    {
	      return ret;
	    }

	  linx = xd3_checksum_hash (& stream->large_hash, lcksum);

	  IF_DEBUG (xd3_verify_large_state (stream, inp, lcksum));

	  if (stream->large_table[linx] != 0)
	    {
	      /* the match_setup will fail if the source window has
	       * been decided and the match lies outside it.
	       * OPT: Consider forcing a window at this point to
	       * permit a new source window. */
	      xoff_t adj_offset =
		xd3_source_cksum_offset(stream,
					stream->large_table[linx] -
					HASH_CKOFFSET);
	      if (xd3_source_match_setup (stream, adj_offset) == 0)
		{
		  if ((ret = xd3_source_extend_match (stream)))
		    {
		      return ret;
		    }

		  /* Update stream position.  match_fwd is zero if no
		   * match. */
		  if (stream->match_fwd > 0)
		    {
		      HANDLELAZY (stream->match_fwd);
		    }
		}
	    }
	}

      /* Small matches. */
      if (DO_SMALL)
	{
	  sinx = xd3_checksum_hash (& stream->small_hash, scksum);

	  /* Verify incremental state in debugging mode. */
	  IF_DEBUG (xd3_verify_small_state (stream, inp, scksum));

	  /* Search for the longest match */
	  if (stream->small_table[sinx] != 0)
	    {
	      match_length = xd3_smatch (stream,
					 stream->small_table[sinx],
					 scksum,
					 & match_offset);
	    }
	  else
	    {
	      match_length = 0;
	    }

	  /* Insert a hash for this string. */
	  xd3_scksum_insert (stream, sinx, scksum, stream->input_position);

	  /* Maybe output a COPY instruction */
	  if (match_length >= stream->min_match)
	    {
	      IF_DEBUG2 ({
		static int x = 0;
		DP(RINT "[target match:%d] <inp %"W"u %"W"u>  <cpy %"W"u %"W"u> "
		   "(-%"W"d) [ %"W"u bytes ]\n",
		   x++,
		   stream->input_position,
		   stream->input_position + match_length,
		   match_offset,
		   match_offset + match_length,
		   stream->input_position - match_offset,
		   match_length);
	      });

	      if ((ret = xd3_found_match (stream,
					  /* decoder position */
					  stream->input_position,
					  /* length */ match_length,
					  /* address */ (xoff_t) match_offset,
					  /* is_source */ 0)))
		{
		  return ret;
		}

	      /* Copy instruction. */
	      HANDLELAZY (match_length);
	    }
	}

      /* The logic above prevents excess work during lazy matching by
       * increasing min_match to avoid smaller matches.  Each time we
       * advance stream->input_position by one, the minimum match
       * shortens as well.  */
      if (stream->min_match > MIN_MATCH)
	{
	  stream->min_match -= 1;
	}

    updateone:

      /* See if there are no more incremental cksums to compute. */
      if (stream->input_position + SLOOK == stream->avail_in)
	{
	  goto loopnomore;
	}

      /* Compute next RUN, CKSUM */
      if (DO_RUN)
	{
	  NEXTRUN (inp[SLOOK]);
	}

      if (DO_SMALL)
	{
	  scksum = xd3_small_cksum_update (&scksum_state, inp, SLOOK);
	}

      if (DO_LARGE && (stream->input_position + LLOOK < stream->avail_in))
	{
	  lcksum = xd3_large_cksum_update (&stream->large_hash, lcksum, inp, LLOOK);
	}
    }

 loopnomore:
  return 0;
}

#endif /* XD3_ENCODER */
#endif /* __XDELTA3_C_TEMPLATE_PASS__ */
