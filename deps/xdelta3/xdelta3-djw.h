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

#ifndef _XDELTA3_DJW_H_
#define _XDELTA3_DJW_H_

/* The following people deserve much credit for the algorithms and
 * techniques contained in this file:

 Julian Seward
 Bzip2 sources, implementation of the multi-table Huffman technique.

 Jean-loup Gailly and Mark Adler and L. Peter Deutsch
 Zlib source code, RFC 1951

 Daniel S. Hirschberg and Debra A. LeLewer
 "Efficient Decoding of Prefix Codes"
 Communications of the ACM, April 1990 33(4).

 David J. Wheeler
 Program bred3.c, bexp3 and accompanying documents bred3.ps, huff.ps.
 This contains the idea behind the multi-table Huffman and 1-2 coding
 techniques.
 ftp://ftp.cl.cam.ac.uk/users/djw3/

*/

/* OPT: during the multi-table iteration, pick the worst-overall
 * performing table and replace it with exactly the frequencies of the
 * worst-overall performing sector or N-worst performing sectors. */

/* REF: See xdfs-0.222 and xdfs-0.226 for some old experiments with
 * the Bzip prefix coding strategy.  xdfs-0.256 contains the last of
 * the other-format tests, including RFC1950 and the RFC1950+MTF
 * tests. */

#define DJW_MAX_CODELEN      20U /* Maximum length of an alphabet code. */

/* Code lengths are themselves code-length encoded, so the total number of
 * codes is: [RUN_0, RUN_1, 1-DJW_MAX_CODELEN] */
#define DJW_TOTAL_CODES      (DJW_MAX_CODELEN+2)

#define RUN_0                0U /* Symbols used in MTF+1/2 coding. */
#define RUN_1                1U

/* Number of code lengths always encoded (djw_encode_basic array) */
#define DJW_BASIC_CODES      5U  
#define DJW_RUN_CODES        2U  /* Number of run codes */

/* Offset of extra codes */
#define DJW_EXTRA_12OFFSET   (DJW_BASIC_CODES + DJW_RUN_CODES)

/* Number of optionally encoded code lengths (djw_encode_extra array) */
#define DJW_EXTRA_CODES      15U

/* Number of bits to code [0-DJW_EXTRA_CODES] */
#define DJW_EXTRA_CODE_BITS  4U  

#define DJW_MAX_GROUPS       8U  /* Max number of group coding tables */
#define DJW_GROUP_BITS       3U  /* Number of bits to code [1-DJW_MAX_GROUPS] */

#define DJW_SECTORSZ_MULT     5U  /* Multiplier for encoded sectorsz */
#define DJW_SECTORSZ_BITS     5U  /* Number of bits to code group size */
#define DJW_SECTORSZ_MAX      ((1U << DJW_SECTORSZ_BITS) * DJW_SECTORSZ_MULT)

/* Maximum number of iterations to find group tables. */
#define DJW_MAX_ITER         6U
/* Minimum number of bits an iteration must reduce coding by. */
#define DJW_MIN_IMPROVEMENT  20U 

/* Maximum code length of a prefix code length */
#define DJW_MAX_CLCLEN       15U

/* Number of bits to code [0-DJW_MAX_CLCLEN] */
#define DJW_CLCLEN_BITS      4U  

#define DJW_MAX_GBCLEN       7U  /* Maximum code length of a group selector */

/* Number of bits to code [0-DJW_MAX_GBCLEN]
 * TODO: Actually, should never have zero code lengths here, or else a group
 * went unused.  Write a test for this: if a group goes unused, eliminate
 * it? */
#define DJW_GBCLEN_BITS      3U

/* It has to save at least this many bits... */
#define EFFICIENCY_BITS      16U

typedef struct _djw_stream   djw_stream;
typedef struct _djw_heapen   djw_heapen;
typedef struct _djw_prefix   djw_prefix;
typedef uint32_t             djw_weight;

struct _djw_heapen
{
  uint32_t depth;
  uint32_t freq;
  uint32_t parent;
};

struct _djw_prefix
{
  usize_t   scount;
  uint8_t *symbol;
  usize_t   mcount;
  uint8_t *mtfsym;
  uint8_t *repcnt;
};

struct _djw_stream
{
  int unused;
};

/* Each Huffman table consists of 256 "code length" (CLEN) codes,
 * which are themselves Huffman coded after eliminating repeats and
 * move-to-front coding.  The prefix consists of all the CLEN codes in
 * djw_encode_basic plus a 4-bit value stating how many of the
 * djw_encode_extra codes are actually coded (the rest are presumed
 * zero, or unused CLEN codes).
 *
 * These values of these two arrays were arrived at by studying the
 * distribution of min and max clen over a collection of DATA, INST,
 * and ADDR inputs.  The goal is to specify the order of
 * djw_extra_codes that is most likely to minimize the number of extra
 * codes that must be encoded.
 *
 * Results: 158896 sections were counted by compressing files (window
 * size 512K) listed with: `find / -type f ( -user jmacd -o -perm +444
 * )`
 *
 * The distribution of CLEN codes for each efficient invocation of the
 * secondary compressor (taking the best number of groups/sector size)
 * was recorded.  Then we look at the distribution of min and max clen
 * values, counting the number of times the value C_low is less than
 * the min and C_high is greater than the max.  Values >= C_high and
 * <= C_low will not have their lengths coded.  The results are sorted
 * and the least likely 15 are placed into the djw_encode_extra[]
 * array in order.  These values are used as the initial MTF ordering.

 clow[1] = 155119
 clow[2] = 140325
 clow[3] = 84072
 ---
 clow[4] = 7225
 clow[5] = 1093
 clow[6] = 215
 ---
 chigh[4] = 1
 chigh[5] = 30
 chigh[6] = 218
 chigh[7] = 2060
 chigh[8] = 13271
 ---
 chigh[9] = 39463
 chigh[10] = 77360
 chigh[11] = 118298
 chigh[12] = 141360
 chigh[13] = 154086
 chigh[14] = 157967
 chigh[15] = 158603
 chigh[16] = 158864
 chigh[17] = 158893
 chigh[18] = 158895
 chigh[19] = 158896
 chigh[20] = 158896

*/

static const uint8_t djw_encode_12extra[DJW_EXTRA_CODES] =
  {
    9, 10, 3, 11, 2, 12, 13, 1, 14, 15, 16, 17, 18, 19, 20,
  };

static const uint8_t djw_encode_12basic[DJW_BASIC_CODES] =
  {
    4, 5, 6, 7, 8,
  };

/*********************************************************************/
/*                              DECLS                                */
/*********************************************************************/

static djw_stream*     djw_alloc           (xd3_stream *stream);
static int             djw_init            (xd3_stream *stream, 
					    djw_stream *h,
					    int is_encode);
static void            djw_destroy         (xd3_stream *stream,
					    djw_stream *h);

#if XD3_ENCODER
static int             xd3_encode_huff     (xd3_stream   *stream,
					    djw_stream  *sec_stream,
					    xd3_output   *input,
					    xd3_output   *output,
					    xd3_sec_cfg  *cfg);
#endif

static int             xd3_decode_huff     (xd3_stream     *stream,
					    djw_stream    *sec_stream,
					    const uint8_t **input,
					    const uint8_t  *const input_end,
					    uint8_t       **output,
					    const uint8_t  *const output_end);

/*********************************************************************/
/*                             HUFFMAN                               */
/*********************************************************************/

static djw_stream*
djw_alloc (xd3_stream *stream)
{
  return (djw_stream*) xd3_alloc (stream, sizeof (djw_stream), 1);
}

static int
djw_init (xd3_stream *stream, djw_stream *h, int is_encode)
{
  /* Fields are initialized prior to use. */
  return 0;
}

static void
djw_destroy (xd3_stream *stream,
	     djw_stream *h)
{
  xd3_free (stream, h);
}


/*********************************************************************/
/*                               HEAP                                */
/*********************************************************************/

static inline int
heap_less (const djw_heapen *a, const djw_heapen *b)
{
  return a->freq   < b->freq ||
    (a->freq  == b->freq &&
     a->depth  < b->depth);
}

static inline void
heap_insert (usize_t *heap, const djw_heapen *ents, usize_t p, const usize_t e)
{
  /* Insert ents[e] into next slot heap[p] */
  usize_t pp = p/2; /* P's parent */

  while (heap_less (& ents[e], & ents[heap[pp]]))
    {
      heap[p] = heap[pp];
      p  = pp;
      pp = p/2;
    }

  heap[p] = e;
}

static inline djw_heapen*
heap_extract (usize_t *heap, const djw_heapen *ents, usize_t heap_last)
{
  usize_t smallest = heap[1];
  usize_t p, pc, t;

  /* Caller decrements heap_last, so heap_last+1 is the replacement elt. */
  heap[1] = heap[heap_last+1];

  /* Re-heapify */
  for (p = 1; ; p = pc)
    {
      pc = p*2;

      /* Reached bottom of heap */
      if (pc > heap_last) { break; }

      /* See if second child is smaller. */
      if (pc < heap_last && heap_less (& ents[heap[pc+1]], & ents[heap[pc]]))
	{
	  pc += 1;
	}

      /* If pc is not smaller than p, heap property re-established. */
      if (! heap_less (& ents[heap[pc]], & ents[heap[p]])) { break; }

      t = heap[pc];
      heap[pc] = heap[p];
      heap[p] = t;
    }

  return (djw_heapen*) & ents[smallest];
}

#if XD3_DEBUG
static void
heap_check (usize_t *heap, djw_heapen *ents, usize_t heap_last)
{
  usize_t i;
  for (i = 1; i <= heap_last; i += 1)
    {
      /* Heap property: child not less than parent */
      XD3_ASSERT (! heap_less (& ents[heap[i]], & ents[heap[i/2]]));

      IF_DEBUG2 (DP(RINT "heap[%"W"u] = %u\n", i, ents[heap[i]].freq));
    }
}
#endif

/*********************************************************************/
/*                             MTF, 1/2                              */
/*********************************************************************/

static inline usize_t
djw_update_mtf (uint8_t *mtf, usize_t mtf_i)
{
  int k;
  usize_t sym = mtf[mtf_i];

  for (k = mtf_i; k != 0; k -= 1) { mtf[k] = mtf[k-1]; }

  mtf[0] = sym;
  return sym;
}

static inline void
djw_update_1_2 (int *mtf_run, usize_t *mtf_i,
		uint8_t *mtfsym, djw_weight *freq)
{
  uint8_t code;
  
  do
    {
      /* Offset by 1, since any number of RUN_ symbols implies run>0... */
      *mtf_run -= 1;

      code = (*mtf_run & 1) ? RUN_1 : RUN_0;

      mtfsym[(*mtf_i)++] = code;
      freq[code] += 1;
      *mtf_run >>= 1;
    }
  while (*mtf_run >= 1);

  *mtf_run = 0;
}

static void
djw_init_clen_mtf_1_2 (uint8_t *clmtf)
{
  usize_t i, cl_i = 0;

  clmtf[cl_i++] = 0;
  for (i = 0; i < DJW_BASIC_CODES; i += 1)
    {
      clmtf[cl_i++] = djw_encode_12basic[i];
    }
  for (i = 0; i < DJW_EXTRA_CODES; i += 1)
    {
      clmtf[cl_i++] = djw_encode_12extra[i];
    }
}

/*********************************************************************/
/*                           PREFIX CODES                            */
/*********************************************************************/
#if XD3_ENCODER
static usize_t
djw_build_prefix (const djw_weight *freq, uint8_t *clen, usize_t asize, usize_t maxlen)
{
  /* Heap with 0th entry unused, prefix tree with up to ALPHABET_SIZE-1
   * internal nodes, never more than ALPHABET_SIZE entries actually in the
   * heap (minimum weight subtrees during prefix construction).  First
   * ALPHABET_SIZE entries are the actual symbols, next ALPHABET_SIZE-1 are
   * internal nodes. */
  djw_heapen ents[ALPHABET_SIZE * 2];
  usize_t heap[ALPHABET_SIZE + 1];

  usize_t heap_last; /* Index of the last _valid_ heap entry. */
  usize_t ents_size; /* Number of entries, including 0th fake entry */
  usize_t  overflow;  /* Number of code lengths that overflow */
  usize_t total_bits;
  usize_t i;

  IF_DEBUG (usize_t first_bits = 0);

  /* Insert real symbol frequences. */
  for (i = 0; i < asize; i += 1)
    {
      ents[i+1].freq = freq[i];
      IF_DEBUG2 (DP(RINT "ents[%"W"i] = freq[%"W"u] = %d\n",
			i+1, i, freq[i]));
    }

 again:

  /* The loop is re-entered each time an overflow occurs.  Re-initialize... */
  heap_last = 0;
  ents_size = 1;
  overflow  = 0;
  total_bits = 0;

  /* 0th entry terminates the while loop in heap_insert (it's the parent of
   * the smallest element, always less-than) */
  heap[0] = 0;
  ents[0].depth = 0;
  ents[0].freq  = 0;

  /* Initial heap. */
  for (i = 0; i < asize; i += 1, ents_size += 1)
    {
      ents[ents_size].depth  = 0;
      ents[ents_size].parent = 0;

      if (ents[ents_size].freq != 0)
	{
	  heap_insert (heap, ents, ++heap_last, ents_size);
	}
    }

  IF_DEBUG (heap_check (heap, ents, heap_last));

  /* Must be at least one symbol, or else we can't get here. */
  XD3_ASSERT (heap_last != 0);

  /* If there is only one symbol, fake a second to prevent zero-length
   * codes. */
  if (heap_last == 1)
    {
      /* Pick either the first or last symbol. */
      usize_t s = freq[0] ? asize-1 : 0;
      ents[s+1].freq = 1;
      goto again;
    }

  /* Build prefix tree. */
  while (heap_last > 1)
    {
      djw_heapen *h1 = heap_extract (heap, ents, --heap_last);
      djw_heapen *h2 = heap_extract (heap, ents, --heap_last);

      ents[ents_size].freq   = h1->freq + h2->freq;
      ents[ents_size].depth  = 1 + xd3_max (h1->depth, h2->depth);
      ents[ents_size].parent = 0;

      h1->parent = h2->parent = ents_size;

      heap_insert (heap, ents, ++heap_last, ents_size++);
    }

  IF_DEBUG (heap_check (heap, ents, heap_last));

  /* Now compute prefix code lengths, counting parents. */
  for (i = 1; i < asize+1; i += 1)
    {
      usize_t b = 0;

      if (ents[i].freq != 0)
	{
	  usize_t p = i;

	  while ((p = ents[p].parent) != 0) { b += 1; }

	  if (b > maxlen) { overflow = 1; }

	  total_bits += b * freq[i-1];
	}

      /* clen is 0-origin, unlike ents. */
      IF_DEBUG2 (DP(RINT "clen[%"W"u] = %"W"u\n", i-1, b));
      clen[i-1] = b;
    }

  IF_DEBUG (if (first_bits == 0) first_bits = total_bits);

  if (! overflow)
    {
      IF_DEBUG2 (if (first_bits != total_bits)
      {
	DP(RINT "code length overflow changed %"W"u bits\n",
	   total_bits - first_bits);
      });
      return total_bits;
    }

  /* OPT: There is a non-looping way to fix overflow shown in zlib, but this
   * is easier (for now), as done in bzip2. */
  for (i = 1; i < asize+1; i += 1)
    {
      ents[i].freq = ents[i].freq / 2 + 1;
    }

  goto again;
}

static void
djw_build_codes (usize_t *codes, const uint8_t *clen, usize_t asize, usize_t abs_max)
{
  usize_t i, l;
  usize_t min_clen = DJW_MAX_CODELEN;
  usize_t max_clen = 0;
  usize_t code = 0;

  /* Find the min and max code length */
  for (i = 0; i < asize; i += 1)
    {
      if (clen[i] > 0 && clen[i] < min_clen)
	{
	  min_clen = clen[i];
	}

      max_clen = xd3_max (max_clen, (usize_t) clen[i]);
    }

  XD3_ASSERT (max_clen <= abs_max);

  /* Generate a code for each symbol with the appropriate length. */
  for (l = min_clen; l <= max_clen; l += 1)
    {
      for (i = 0; i < asize; i += 1)
	{
	  if (clen[i] == l)
	    {
	      codes[i] = code++;
	    } 
	}

      code <<= 1;
    }

  IF_DEBUG2 ({
      for (i = 0; i < asize; i += 1)
	{
	  DP(RINT "code[%"W"u] = %"W"u\n", i, codes[i]);
	}
    });
}

/*********************************************************************/
/*			      MOVE-TO-FRONT                          */
/*********************************************************************/
static void
djw_compute_mtf_1_2 (djw_prefix  *prefix,
		     uint8_t     *mtf,
		     djw_weight  *freq_out,
		     usize_t      nsym)
{
  size_t i, j, k;
  usize_t sym;
  usize_t size = prefix->scount;
  usize_t mtf_i = 0;
  int mtf_run = 0;

  /* This +2 is for the RUN_0, RUN_1 codes */
  memset (freq_out, 0, sizeof (freq_out[0]) * (nsym+2));

  for (i = 0; i < size; )
    {
      /* OPT: Bzip optimizes this algorithm a little by effectively checking
       * j==0 before the MTF update. */
      sym = prefix->symbol[i++];

      for (j = 0; mtf[j] != sym; j += 1) { }

      XD3_ASSERT (j <= nsym);

      for (k = j; k >= 1; k -= 1) { mtf[k] = mtf[k-1]; }

      mtf[0] = sym;

      if (j == 0)
	{
	  mtf_run += 1;
	  continue;
	}

      if (mtf_run > 0)
	{
	  djw_update_1_2 (& mtf_run, & mtf_i, prefix->mtfsym, freq_out);
	}

      /* Non-zero symbols are offset by RUN_1 */
      prefix->mtfsym[mtf_i++] = (uint8_t)(j+RUN_1);
      freq_out[j+RUN_1] += 1;
    }

  if (mtf_run > 0)
    {
      djw_update_1_2 (& mtf_run, & mtf_i, prefix->mtfsym, freq_out);
    }

  prefix->mcount = mtf_i;
}

/* Counts character frequencies of the input buffer, returns the size. */
static usize_t
djw_count_freqs (djw_weight *freq, xd3_output *input)
{
  xd3_output *in;
  usize_t size = 0;

  memset (freq, 0, sizeof (freq[0]) * ALPHABET_SIZE);

  for (in = input; in; in = in->next_page)
    {
      const uint8_t *p     = in->base;
      const uint8_t *p_max = p + in->next;

      size += in->next;

      do
	{
	  ++freq[*p];
	}
      while (++p < p_max);
    }

  IF_DEBUG2 ({int i;
  DP(RINT "freqs: ");
  for (i = 0; i < ALPHABET_SIZE; i += 1)
    {
      DP(RINT "%u ", freq[i]);
    }
  DP(RINT "\n");});

  return size;
}

static void
djw_compute_multi_prefix (usize_t     groups,
			  uint8_t     clen[DJW_MAX_GROUPS][ALPHABET_SIZE],
			  djw_prefix *prefix)
{
  usize_t gp, i;
      
  prefix->scount = ALPHABET_SIZE;
  memcpy (prefix->symbol, clen[0], ALPHABET_SIZE);

  for (gp = 1; gp < groups; gp += 1)
    {
      for (i = 0; i < ALPHABET_SIZE; i += 1)
	{
	  if (clen[gp][i] == 0)
	    {
	      continue;
	    }

	  prefix->symbol[prefix->scount++] = clen[gp][i];
	}
    }
}

static void
djw_compute_prefix_1_2 (djw_prefix *prefix, djw_weight *freq)
{
  /* This +1 is for the 0 code-length. */
  uint8_t clmtf[DJW_MAX_CODELEN+1];

  djw_init_clen_mtf_1_2 (clmtf);

  djw_compute_mtf_1_2 (prefix, clmtf, freq, DJW_MAX_CODELEN);
}

static int
djw_encode_prefix (xd3_stream   *stream,
		   xd3_output  **output,
		   bit_state    *bstate,
		   djw_prefix   *prefix)
{
  int ret;
  size_t i;
  usize_t num_to_encode;
  djw_weight clfreq[DJW_TOTAL_CODES];
  uint8_t    clclen[DJW_TOTAL_CODES];
  usize_t    clcode[DJW_TOTAL_CODES];

  /* Move-to-front encode prefix symbols, count frequencies */
  djw_compute_prefix_1_2 (prefix, clfreq);

  /* Compute codes */
  djw_build_prefix (clfreq, clclen, DJW_TOTAL_CODES, DJW_MAX_CLCLEN);
  djw_build_codes  (clcode, clclen, DJW_TOTAL_CODES, DJW_MAX_CLCLEN);

  /* Compute number of extra codes beyond basic ones for this template. */
  num_to_encode = DJW_TOTAL_CODES;
  while (num_to_encode > DJW_EXTRA_12OFFSET && clclen[num_to_encode-1] == 0)
    {
      num_to_encode -= 1;
    }
  XD3_ASSERT (num_to_encode - DJW_EXTRA_12OFFSET < (1 << DJW_EXTRA_CODE_BITS));

  /* Encode: # of extra codes */
  if ((ret = xd3_encode_bits (stream, output, bstate, DJW_EXTRA_CODE_BITS,
			      num_to_encode - DJW_EXTRA_12OFFSET)))
    {
      return ret;
    }

  /* Encode: MTF code lengths */
  for (i = 0; i < num_to_encode; i += 1)
    {
      if ((ret = xd3_encode_bits (stream, output, bstate,
				  DJW_CLCLEN_BITS, clclen[i])))
	{
	  return ret;
	}
    }

  /* Encode: CLEN code lengths */
  for (i = 0; i < prefix->mcount; i += 1)
    {
      usize_t mtf_sym = prefix->mtfsym[i];
      usize_t bits    = clclen[mtf_sym];
      usize_t code    = clcode[mtf_sym];

      if ((ret = xd3_encode_bits (stream, output, bstate, bits, code)))
	{
	  return ret;
	}
    }

  return 0;
}

static void
djw_compute_selector_1_2 (djw_prefix *prefix,
			  usize_t     groups,
			  djw_weight *gbest_freq)
{
  uint8_t grmtf[DJW_MAX_GROUPS];
  usize_t i;

  for (i = 0; i < groups; i += 1) { grmtf[i] = i; }

  djw_compute_mtf_1_2 (prefix, grmtf, gbest_freq, groups);
}

static int
xd3_encode_howmany_groups (xd3_stream *stream,
			   xd3_sec_cfg *cfg,
			   usize_t input_size,
			   usize_t *ret_groups,
			   usize_t *ret_sector_size)
{
  usize_t cfg_groups = 0;
  usize_t cfg_sector_size = 0;
  usize_t sugg_groups = 0;
  usize_t sugg_sector_size = 0;

  if (cfg->ngroups != 0)
    {
      if (cfg->ngroups > DJW_MAX_GROUPS)
	{
	  stream->msg = "invalid secondary encoder group number";
	  return XD3_INTERNAL;
	}

      cfg_groups = cfg->ngroups;
    }

  if (cfg->sector_size != 0)
    {
      if (cfg->sector_size < DJW_SECTORSZ_MULT ||
	  cfg->sector_size > DJW_SECTORSZ_MAX ||
	  (cfg->sector_size % DJW_SECTORSZ_MULT) != 0)
	{
	  stream->msg = "invalid secondary encoder sector size";
	  return XD3_INTERNAL;
	}

      cfg_sector_size = cfg->sector_size;
    }

  if (cfg_groups == 0 || cfg_sector_size == 0)
    {
      /* These values were found empirically using xdelta3-tune around version
       * xdfs-0.256. */
      switch (cfg->data_type)
	{
	case DATA_SECTION:
	  if      (input_size < 1000)   { sugg_groups = 1; sugg_sector_size = 0; }
	  else if (input_size < 4000)   { sugg_groups = 2; sugg_sector_size = 10; }
	  else if (input_size < 7000)   { sugg_groups = 3; sugg_sector_size = 10; }
	  else if (input_size < 10000)  { sugg_groups = 4; sugg_sector_size = 10; }
	  else if (input_size < 25000)  { sugg_groups = 5; sugg_sector_size = 10; }
	  else if (input_size < 50000)  { sugg_groups = 7; sugg_sector_size = 20; }
	  else if (input_size < 100000) { sugg_groups = 8; sugg_sector_size = 30; }
	  else                          { sugg_groups = 8; sugg_sector_size = 70; }
	  break;
	case INST_SECTION:
	  if      (input_size < 7000)   { sugg_groups = 1; sugg_sector_size = 0; }
	  else if (input_size < 10000)  { sugg_groups = 2; sugg_sector_size = 50; }
	  else if (input_size < 25000)  { sugg_groups = 3; sugg_sector_size = 50; }
	  else if (input_size < 50000)  { sugg_groups = 6; sugg_sector_size = 40; }
	  else if (input_size < 100000) { sugg_groups = 8; sugg_sector_size = 40; }
	  else                          { sugg_groups = 8; sugg_sector_size = 40; }
	  break;
	case ADDR_SECTION:
	  if      (input_size < 9000)   { sugg_groups = 1; sugg_sector_size = 0; }
	  else if (input_size < 25000)  { sugg_groups = 2; sugg_sector_size = 130; }
	  else if (input_size < 50000)  { sugg_groups = 3; sugg_sector_size = 130; }
	  else if (input_size < 100000) { sugg_groups = 5; sugg_sector_size = 130; }
	  else                          { sugg_groups = 7; sugg_sector_size = 130; }
	  break;
	}

      if (cfg_groups == 0)
	{
	  cfg_groups = sugg_groups;
	}

      if (cfg_sector_size == 0)
	{
	  cfg_sector_size = sugg_sector_size;
	}
    }

  if (cfg_groups != 1 && cfg_sector_size == 0)
    {
      switch (cfg->data_type)
	{
	case DATA_SECTION:
	  cfg_sector_size = 20;
	  break;
	case INST_SECTION:
	  cfg_sector_size = 50;
	  break;
	case ADDR_SECTION:
	  cfg_sector_size = 130;
	  break;
	}
    }

  (*ret_groups)     = cfg_groups;
  (*ret_sector_size) = cfg_sector_size;

  XD3_ASSERT (cfg_groups > 0 && cfg_groups <= DJW_MAX_GROUPS);
  XD3_ASSERT (cfg_groups == 1 ||
	      (cfg_sector_size >= DJW_SECTORSZ_MULT &&
	       cfg_sector_size <= DJW_SECTORSZ_MAX));

  return 0;
}

static int
xd3_encode_huff (xd3_stream   *stream,
		 djw_stream   *h,
		 xd3_output   *input,
		 xd3_output   *output,
		 xd3_sec_cfg  *cfg)
{
  int         ret;
  usize_t     groups, sector_size;
  bit_state   bstate = BIT_STATE_ENCODE_INIT;
  xd3_output *in;
  usize_t     output_bits;
  usize_t     input_bits;
  usize_t     input_bytes;
  usize_t     initial_offset = output->next;
  djw_weight  real_freq[ALPHABET_SIZE];
  uint8_t    *gbest = NULL;
  uint8_t    *gbest_mtf = NULL;

  input_bytes = djw_count_freqs (real_freq, input);
  input_bits  = input_bytes * 8;

  XD3_ASSERT (input_bytes > 0);

  if ((ret = xd3_encode_howmany_groups (stream, cfg, input_bytes,
					& groups, & sector_size)))
    {
      return ret;
    }

  if (0)
    {
    regroup:
      /* Sometimes we dynamically decide there are too many groups.  Arrive
       * here. */
      output->next = initial_offset;
      xd3_bit_state_encode_init (& bstate);
    }

  /* Encode: # of groups (3 bits) */
  if ((ret = xd3_encode_bits (stream, & output, & bstate,
			      DJW_GROUP_BITS, groups-1))) { goto failure; }

  if (groups == 1)
    {
      /* Single Huffman group. */
      usize_t    code[ALPHABET_SIZE]; /* Codes */
      uint8_t    clen[ALPHABET_SIZE];
      uint8_t    prefix_mtfsym[ALPHABET_SIZE];
      djw_prefix prefix;

      output_bits =
	djw_build_prefix (real_freq, clen, ALPHABET_SIZE, DJW_MAX_CODELEN);
      djw_build_codes (code, clen, ALPHABET_SIZE, DJW_MAX_CODELEN);

      if (output_bits + EFFICIENCY_BITS >= input_bits && ! cfg->inefficient)
	{
	  goto nosecond;
	}

      /* Encode: prefix */
      prefix.mtfsym = prefix_mtfsym;
      prefix.symbol = clen;
      prefix.scount = ALPHABET_SIZE;

      if ((ret = djw_encode_prefix (stream, & output, & bstate, & prefix)))
	{
	  goto failure;
	}

      if (output_bits + (8 * output->next) + EFFICIENCY_BITS >=
	  input_bits && ! cfg->inefficient)
	{
	  goto nosecond;
	}

      /* Encode: data */
      for (in = input; in; in = in->next_page)
	{
	  const uint8_t *p     = in->base;
	  const uint8_t *p_max = p + in->next;

	  do
	    {
	      usize_t sym  = *p++;
	      usize_t bits = clen[sym];

	      IF_DEBUG (output_bits -= bits);

	      if ((ret = xd3_encode_bits (stream, & output,
					  & bstate, bits, code[sym])))
		{
		  goto failure;
		}
	    }
	  while (p < p_max);
	}

      XD3_ASSERT (output_bits == 0);
    }
  else
    {
      /* DJW Huffman */
      djw_weight evolve_freq[DJW_MAX_GROUPS][ALPHABET_SIZE];
      uint8_t evolve_clen[DJW_MAX_GROUPS][ALPHABET_SIZE];
      djw_weight left = input_bytes;
      usize_t gp;
      usize_t niter = 0;
      usize_t select_bits;
      usize_t sym1 = 0, sym2 = 0, s;
      usize_t gcost[DJW_MAX_GROUPS];
      usize_t gbest_code[DJW_MAX_GROUPS+2];
      uint8_t gbest_clen[DJW_MAX_GROUPS+2];
      usize_t  gbest_max = 1 + (input_bytes - 1) / sector_size;
      usize_t best_bits = 0;
      usize_t  gbest_no;
      usize_t  gpcnt;
      const uint8_t *p;
      IF_DEBUG2 (usize_t gcount[DJW_MAX_GROUPS]);

      /* Encode: sector size (5 bits) */
      if ((ret = xd3_encode_bits (stream, & output, & bstate,
				  DJW_SECTORSZ_BITS,
				  (sector_size/DJW_SECTORSZ_MULT)-1)))
	{
	  goto failure;
	}

      /* Dynamic allocation. */
      if (gbest == NULL)
	{
	  if ((gbest = (uint8_t*) xd3_alloc (stream, gbest_max, 1)) == NULL)
	    {
	      ret = ENOMEM;
	      goto failure;
	    }
	}

      if (gbest_mtf == NULL)
	{
	  if ((gbest_mtf = (uint8_t*) xd3_alloc (stream, gbest_max, 1)) == NULL)
	    {
	      ret = ENOMEM;
	      goto failure;
	    }
	}

      /* OPT: Some of the inner loops can be optimized, as shown in bzip2 */

      /* Generate initial code length tables. */
      for (gp = 0; gp < groups; gp += 1)
	{
	  djw_weight sum  = 0;
	  djw_weight goal = left / (groups - gp);

	  IF_DEBUG2 (usize_t nz = 0);

	  /* Due to the single-code granularity of this distribution, it may
	   * be that we can't generate a distribution for each group.  In that
	   * case subtract one group and try again.  If (inefficient), we're
	   * testing group behavior, so don't mess things up. */
	  if (goal == 0 && !cfg->inefficient)
	    {
	      IF_DEBUG2 (DP(RINT "too many groups (%"W"u), dropping one\n",
			    groups));
	      groups -= 1;
	      goto regroup;
	    }

	  /* Sum == goal is possible when (cfg->inefficient)... */
	  while (sum < goal)
	    {
	      XD3_ASSERT (sym2 < ALPHABET_SIZE);
	      IF_DEBUG2 (nz += real_freq[sym2] != 0);
	      sum += real_freq[sym2++];
	    }

	  IF_DEBUG2(DP(RINT "group %"W"u has symbols %"W"u..%"W"u (%"W"u non-zero) "
		       "(%u/%"W"u = %.3f)\n",
		       gp, sym1, sym2, nz, sum,
		       input_bytes, sum / (double)input_bytes););

	  for (s = 0; s < ALPHABET_SIZE; s += 1)
	    {
	      evolve_clen[gp][s] = (s >= sym1 && s <= sym2) ? 1 : 16;
	    }

	  left -= sum;
	  sym1  = sym2+1;
	}

    repeat:

      niter += 1;
      gbest_no = 0;
      memset (evolve_freq, 0, sizeof (evolve_freq[0]) * groups);
      IF_DEBUG2 (memset (gcount, 0, sizeof (gcount[0]) * groups));

      /* For each input page (loop is irregular to allow non-pow2-size group
       * size. */
      in = input;
      p  = in->base;

      /* For each group-size sector. */
      do
	{
	  const uint8_t *p0  = p;
	  xd3_output    *in0 = in;
	  usize_t best   = 0;
	  usize_t winner = 0;

	  /* Select best group for each sector, update evolve_freq. */
	  memset (gcost, 0, sizeof (gcost[0]) * groups);

	  /* For each byte in sector. */
	  for (gpcnt = 0; gpcnt < sector_size; gpcnt += 1)
	    {
	      /* For each group. */
	      for (gp = 0; gp < groups; gp += 1)
		{
		  gcost[gp] += evolve_clen[gp][*p];
		}

	      /* Check end-of-input-page. */
#             define GP_PAGE()                \
	      if ((usize_t)(++p - in->base) == in->next) \
		{                             \
		  in = in->next_page;         \
		  if (in == NULL) { break; }  \
		  p  = in->base;              \
		}

	      GP_PAGE ();
	    }

	  /* Find min cost group for this sector */
	  best = USIZE_T_MAX;
	  for (gp = 0; gp < groups; gp += 1)
	    {
	      if (gcost[gp] < best) 
		{ 
		  best = gcost[gp]; 
		  winner = gp; 
		}
	    }

	  XD3_ASSERT(gbest_no < gbest_max);
	  gbest[gbest_no++] = winner;
	  IF_DEBUG2 (gcount[winner] += 1);

	  p  = p0;
	  in = in0;

	  /* Update group frequencies. */
	  for (gpcnt = 0; gpcnt < sector_size; gpcnt += 1)
	    {
	      evolve_freq[winner][*p] += 1;

	      GP_PAGE ();
	    }
	}
      while (in != NULL);

      XD3_ASSERT (gbest_no == gbest_max);

      /* Recompute code lengths. */
      output_bits = 0;
      for (gp = 0; gp < groups; gp += 1)
	{
	  int i;
	  uint8_t evolve_zero[ALPHABET_SIZE];
	  int any_zeros = 0;

	  memset (evolve_zero, 0, sizeof (evolve_zero));

	  /* Cannot allow a zero clen when the real frequency is non-zero.
	   * Note: this means we are going to encode a fairly long code for
	   * these unused entries.  An improvement would be to implement a
	   * NOTUSED code for when these are actually zero, but this requires
	   * another data structure (evolve_zero) since we don't know when
	   * evolve_freq[i] == 0...  Briefly tested, looked worse. */
	  for (i = 0; i < ALPHABET_SIZE; i += 1)
	    {
	      if (evolve_freq[gp][i] == 0 && real_freq[i] != 0)
		{
		  evolve_freq[gp][i] = 1;
		  evolve_zero[i] = 1;
		  any_zeros = 1;
		}
	    }

	  output_bits += djw_build_prefix (evolve_freq[gp], evolve_clen[gp],
					   ALPHABET_SIZE, DJW_MAX_CODELEN);

	  /* The above faking of frequencies does not matter for the last
	   * iteration, but we don't know when that is yet.  However, it also
	   * breaks the output_bits computation.  Necessary for accuracy, and
	   * for the (output_bits==0) assert after all bits are output. */
	  if (any_zeros)
	    {
	      IF_DEBUG2 (usize_t save_total = output_bits);

	      for (i = 0; i < ALPHABET_SIZE; i += 1)
		{
		  if (evolve_zero[i]) { output_bits -= evolve_clen[gp][i]; }
		}

	      IF_DEBUG2 (DP(RINT "evolve_zero reduced %"W"u bits in group %"W"u\n",
			    save_total - output_bits, gp));
	    }
	}

      IF_DEBUG2(
	DP(RINT "pass %"W"u total bits: %"W"u group uses: ", niter, output_bits);
	for (gp = 0; gp < groups; gp += 1) { DP(RINT "%"W"u ", gcount[gp]); }
	DP(RINT "\n");
	);

      /* End iteration. */

      IF_DEBUG2 (if (niter > 1 && best_bits < output_bits) {
	DP(RINT "iteration lost %"W"u bits\n", output_bits - best_bits); });

      if (niter == 1 || (niter < DJW_MAX_ITER &&
			 (best_bits - output_bits) >= DJW_MIN_IMPROVEMENT))
	{
	  best_bits = output_bits;
	  goto repeat;
	}

      /* Efficiency check. */
      if (output_bits + EFFICIENCY_BITS >= input_bits && ! cfg->inefficient)
	{
	  goto nosecond;
	}

      IF_DEBUG2 (DP(RINT "djw compression: %"W"u -> %0.3f\n",
		    input_bytes, output_bits / 8.0));

      /* Encode: prefix */
      {
	uint8_t     prefix_symbol[DJW_MAX_GROUPS * ALPHABET_SIZE];
	uint8_t     prefix_mtfsym[DJW_MAX_GROUPS * ALPHABET_SIZE];
	uint8_t     prefix_repcnt[DJW_MAX_GROUPS * ALPHABET_SIZE];
	djw_prefix prefix;

	prefix.symbol = prefix_symbol;
	prefix.mtfsym = prefix_mtfsym;
	prefix.repcnt = prefix_repcnt;

	djw_compute_multi_prefix (groups, evolve_clen, & prefix);
	if ((ret = djw_encode_prefix (stream, & output, & bstate, & prefix)))
	  {
	    goto failure;
	  }
      }

      /* Encode: selector frequencies */
      {
	/* DJW_MAX_GROUPS +2 is for RUN_0, RUN_1 symbols. */
	djw_weight gbest_freq[DJW_MAX_GROUPS+2];
	djw_prefix gbest_prefix;
	usize_t i;

	gbest_prefix.scount = gbest_no;
	gbest_prefix.symbol = gbest;
	gbest_prefix.mtfsym = gbest_mtf;

	djw_compute_selector_1_2 (& gbest_prefix, groups, gbest_freq);

	select_bits =
	  djw_build_prefix (gbest_freq, gbest_clen, groups+1, DJW_MAX_GBCLEN);
	djw_build_codes  (gbest_code, gbest_clen, groups+1, DJW_MAX_GBCLEN);

	for (i = 0; i < groups+1; i += 1)
	  {
	    if ((ret = xd3_encode_bits (stream, & output, & bstate,
					DJW_GBCLEN_BITS, gbest_clen[i])))
	      {
		goto failure;
	      }
	  }

	for (i = 0; i < gbest_prefix.mcount; i += 1)
	  {
	    usize_t gp_mtf      = gbest_mtf[i];
	    usize_t gp_sel_bits = gbest_clen[gp_mtf];
	    usize_t gp_sel_code = gbest_code[gp_mtf];

	    XD3_ASSERT (gp_mtf < groups+1);

	    if ((ret = xd3_encode_bits (stream, & output, & bstate,
					gp_sel_bits, gp_sel_code)))
	      {
		goto failure;
	      }

	    IF_DEBUG (select_bits -= gp_sel_bits);
	  }

	XD3_ASSERT (select_bits == 0);
      }

      /* Efficiency check. */
      if (output_bits + select_bits + (8 * output->next) +
	  EFFICIENCY_BITS >= input_bits && ! cfg->inefficient)
	{
	  goto nosecond;
	}

      /* Encode: data */
      {
	usize_t evolve_code[DJW_MAX_GROUPS][ALPHABET_SIZE];
	usize_t sector = 0;

	/* Build code tables for each group. */
	for (gp = 0; gp < groups; gp += 1)
	  {
	    djw_build_codes (evolve_code[gp], evolve_clen[gp],
			     ALPHABET_SIZE, DJW_MAX_CODELEN);
	  }

	/* Now loop over the input. */
	in = input;
	p  = in->base;

	do
	  {
	    /* For each sector. */
	    usize_t   gp_best  = gbest[sector];
	    usize_t *gp_codes = evolve_code[gp_best];
	    uint8_t *gp_clens = evolve_clen[gp_best];

	    XD3_ASSERT (sector < gbest_no);

	    sector += 1;

	    /* Encode the sector data. */
	    for (gpcnt = 0; gpcnt < sector_size; gpcnt += 1)
	      {
		usize_t sym  = *p;
		usize_t bits = gp_clens[sym];
		usize_t code = gp_codes[sym];

		IF_DEBUG (output_bits -= bits);

		if ((ret = xd3_encode_bits (stream, & output, & bstate,
					    bits, code)))
		  {
		    goto failure;
		  }

		GP_PAGE ();
	      }
	  }
	while (in != NULL);

	XD3_ASSERT (select_bits == 0);
	XD3_ASSERT (output_bits == 0);
      }
    }

  ret = xd3_flush_bits (stream, & output, & bstate);

  if (0)
    {
    nosecond:
      stream->msg = "secondary compression was inefficient";
      ret = XD3_NOSECOND;
    }

 failure:

  xd3_free (stream, gbest);
  xd3_free (stream, gbest_mtf);
  return ret;
}
#endif /* XD3_ENCODER */

/*********************************************************************/
/*                              DECODE                               */
/*********************************************************************/

static void
djw_build_decoder (xd3_stream    *stream,
		   usize_t        asize,
		   usize_t        abs_max,
		   const uint8_t *clen,
		   uint8_t       *inorder,
		   usize_t       *base,
		   usize_t       *limit,
		   usize_t       *min_clenp,
		   usize_t       *max_clenp)
{
  usize_t i, l;
  const uint8_t *ci;
  usize_t nr_clen [DJW_TOTAL_CODES];
  usize_t tmp_base[DJW_TOTAL_CODES];
  usize_t min_clen;
  usize_t max_clen;

  /* Assumption: the two temporary arrays are large enough to hold abs_max. */
  XD3_ASSERT (abs_max <= DJW_MAX_CODELEN);

  /* This looks something like the start of zlib's inftrees.c */
  memset (nr_clen, 0, sizeof (nr_clen[0]) * (abs_max+1));

  /* Count number of each code length */
  i  = asize;
  ci = clen;
  do
    {
      /* Caller _must_ check that values are in-range.  Most of the time the
       * caller decodes a specific number of bits, which imply the max value,
       * and the other time the caller decodes a huffman value, which must be
       * in-range.  Therefore, its an assertion and this function cannot
       * otherwise fail. */
      XD3_ASSERT (*ci <= abs_max);

      nr_clen[*ci++]++;
    }
  while (--i != 0);

  /* Compute min, max. */
  for (i = 1; i <= abs_max; i += 1) { if (nr_clen[i]) { break; } }
  min_clen = i;
  for (i = abs_max; i != 0; i -= 1) { if (nr_clen[i]) { break; } }
  max_clen = i;

  /* Fill the BASE, LIMIT table. */
  tmp_base[min_clen] = 0;
  base[min_clen]     = 0;
  limit[min_clen]    = nr_clen[min_clen] - 1;
  for (i = min_clen + 1; i <= max_clen; i += 1)
    {
      usize_t last_limit = ((limit[i-1] + 1) << 1);
      tmp_base[i] = tmp_base[i-1] + nr_clen[i-1];
      limit[i]    = last_limit + nr_clen[i] - 1;
      base[i]     = last_limit - tmp_base[i];
    }

  /* Fill the inorder array, canonically ordered codes. */
  ci = clen;
  for (i = 0; i < asize; i += 1)
    {
      if ((l = *ci++) != 0)
	{
	  inorder[tmp_base[l]++] = i;
	}
    }

  *min_clenp = min_clen;
  *max_clenp = max_clen;
}

static inline int
djw_decode_symbol (xd3_stream     *stream,
		   bit_state      *bstate,
		   const uint8_t **input,
		   const uint8_t  *input_end,
		   const uint8_t  *inorder,
		   const usize_t  *base,
		   const usize_t  *limit,
		   usize_t         min_clen,
		   usize_t         max_clen,
		   usize_t         *sym,
		   usize_t          max_sym)
{
  usize_t code = 0;
  usize_t bits = 0;

  /* OPT: Supposedly a small lookup table improves speed here... */

  /* Code outline is similar to xd3_decode_bits... */
  if (bstate->cur_mask == 0x100) { goto next_byte; }

  for (;;)
    {
      do
	{
	  if (bits == max_clen) { goto corrupt; }

	  bits += 1;
	  code  = (code << 1);

	  if (bstate->cur_byte & bstate->cur_mask) { code |= 1; }

	  bstate->cur_mask <<= 1;

	  if (bits >= min_clen && code <= limit[bits]) { goto done; }
	}
      while (bstate->cur_mask != 0x100);

    next_byte:

      if (*input == input_end)
	{
	  stream->msg = "secondary decoder end of input";
	  return XD3_INVALID_INPUT;
	}

      bstate->cur_byte = *(*input)++;
      bstate->cur_mask = 1;
    }

 done:

  if (base[bits] <= code)
    {
      usize_t offset = code - base[bits];

      if (offset <= max_sym)
	{
	  IF_DEBUG2 (DP(RINT "(j) %"W"u ", code));
	  *sym = inorder[offset];
	  return 0;
	}
    }

 corrupt:
  stream->msg = "secondary decoder invalid code";
  return XD3_INVALID_INPUT;
}

static int
djw_decode_clclen (xd3_stream     *stream,
		   bit_state      *bstate,
		   const uint8_t **input,
		   const uint8_t  *input_end,
		   uint8_t        *cl_inorder,
		   usize_t        *cl_base,
		   usize_t        *cl_limit,
		   usize_t        *cl_minlen,
		   usize_t        *cl_maxlen,
		   uint8_t        *cl_mtf)
{
  int ret;
  uint8_t cl_clen[DJW_TOTAL_CODES];
  usize_t num_codes, value;
  usize_t i;

  /* How many extra code lengths to encode. */
  if ((ret = xd3_decode_bits (stream, bstate, input,
			      input_end, DJW_EXTRA_CODE_BITS, & num_codes)))
    {
      return ret;
    }

  num_codes += DJW_EXTRA_12OFFSET;

  /* Read num_codes. */
  for (i = 0; i < num_codes; i += 1)
    {
      if ((ret = xd3_decode_bits (stream, bstate, input,
				  input_end, DJW_CLCLEN_BITS, & value)))
	{
	  return ret;
	}

      cl_clen[i] = value;
    }

  /* Set the rest to zero. */
  for (; i < DJW_TOTAL_CODES; i += 1) { cl_clen[i] = 0; }

  /* No need to check for in-range clen values, because: */
  XD3_ASSERT (1 << DJW_CLCLEN_BITS == DJW_MAX_CLCLEN + 1);

  /* Build the code-length decoder. */
  djw_build_decoder (stream, DJW_TOTAL_CODES, DJW_MAX_CLCLEN,
		     cl_clen, cl_inorder, cl_base,
		     cl_limit, cl_minlen, cl_maxlen);

  /* Initialize the MTF state. */
  djw_init_clen_mtf_1_2 (cl_mtf);

  return 0;
}

static inline int
djw_decode_1_2 (xd3_stream     *stream,
		bit_state      *bstate,
		const uint8_t **input,
		const uint8_t  *input_end,
		const uint8_t  *inorder,
		const usize_t  *base,
		const usize_t  *limit,
		const usize_t  *minlen,
		const usize_t  *maxlen,
		uint8_t        *mtfvals,
		usize_t         elts,
		usize_t         skip_offset,
		uint8_t        *values)
{
  usize_t n = 0, rep = 0, mtf = 0, s = 0;
  int ret;
  
  while (n < elts)
    {
      /* Special case inside generic code: CLEN only: If not the first group,
       * we already know the zero frequencies. */
      if (skip_offset != 0 && n >= skip_offset && values[n-skip_offset] == 0)
	{
	  values[n++] = 0;
	  continue;
	}

      /* Repeat last symbol. */
      if (rep != 0)
	{
	  values[n++] = mtfvals[0];
	  rep -= 1;
	  continue;
	}

      /* Symbol following last repeat code. */
      if (mtf != 0)
	{
	  usize_t sym = djw_update_mtf (mtfvals, mtf);
	  values[n++] = sym;
	  mtf = 0;
	  continue;
	}

      /* Decode next symbol/repeat code. */
      if ((ret = djw_decode_symbol (stream, bstate, input, input_end,
				    inorder, base, limit, *minlen, *maxlen,
				    & mtf, DJW_TOTAL_CODES))) { return ret; }

      if (mtf <= RUN_1)
	{
	  /* Repetition. */
	  rep = ((mtf + 1) << s);
	  mtf = 0;
	  s += 1;
	}
      else
	{
	  /* Remove the RUN_1 MTF offset. */
	  mtf -= 1;
	  s = 0;
	}
    }

  /* If (rep != 0) there were too many codes received. */
  if (rep != 0)
    {
      stream->msg = "secondary decoder invalid repeat code";
      return XD3_INVALID_INPUT;
    }
  
  return 0;
}

static inline int
djw_decode_prefix (xd3_stream     *stream,
		   bit_state      *bstate,
		   const uint8_t **input,
		   const uint8_t  *input_end,
		   const uint8_t  *cl_inorder,
		   const usize_t  *cl_base,
		   const usize_t  *cl_limit,
		   const usize_t  *cl_minlen,
		   const usize_t  *cl_maxlen,
		   uint8_t        *cl_mtf,
		   usize_t         groups,
		   uint8_t        *clen)
{
  return djw_decode_1_2 (stream, bstate, input, input_end,
			 cl_inorder, cl_base, cl_limit,
			 cl_minlen, cl_maxlen, cl_mtf,
			 ALPHABET_SIZE * groups, ALPHABET_SIZE, clen);
}

static int
xd3_decode_huff (xd3_stream     *stream,
		 djw_stream    *h,
		 const uint8_t **input_pos,
		 const uint8_t  *const input_end,
		 uint8_t       **output_pos,
		 const uint8_t  *const output_end)
{
  const uint8_t *input = *input_pos;
  uint8_t  *output = *output_pos;
  bit_state bstate = BIT_STATE_DECODE_INIT;
  uint8_t  *sel_group = NULL;
  usize_t    groups, gp;
  usize_t    output_bytes = (usize_t)(output_end - output);
  usize_t    sector_size;
  usize_t    sectors;
  int ret;

  /* Invalid input. */
  if (output_bytes == 0)
    {
      stream->msg = "secondary decoder invalid input";
      return XD3_INVALID_INPUT;
    }

  /* Decode: number of groups */
  if ((ret = xd3_decode_bits (stream, & bstate, & input,
			      input_end, DJW_GROUP_BITS, & groups)))
    {
      goto fail;
    }

  groups += 1;

  if (groups > 1)
    {
      /* Decode: group size */
      if ((ret = xd3_decode_bits (stream, & bstate, & input,
				  input_end, DJW_SECTORSZ_BITS,
				  & sector_size))) { goto fail; }
      
      sector_size = (sector_size + 1) * DJW_SECTORSZ_MULT;
    }
  else
    {
      /* Default for groups == 1 */
      sector_size = output_bytes;
    }

  sectors = 1 + (output_bytes - 1) / sector_size;

  /* TODO: In the case of groups==1, lots of extra stack space gets used here.
   * Could dynamically allocate this memory, which would help with excess
   * parameter passing, too.  Passing too many parameters in this file,
   * simplify it! */

  /* Outer scope: per-group symbol decoder tables. */
  {
    uint8_t inorder[DJW_MAX_GROUPS][ALPHABET_SIZE];
    usize_t base   [DJW_MAX_GROUPS][DJW_TOTAL_CODES];
    usize_t limit  [DJW_MAX_GROUPS][DJW_TOTAL_CODES];
    usize_t minlen [DJW_MAX_GROUPS];
    usize_t maxlen [DJW_MAX_GROUPS];

    /* Nested scope: code length decoder tables. */
    {
      uint8_t clen      [DJW_MAX_GROUPS][ALPHABET_SIZE];
      uint8_t cl_inorder[DJW_TOTAL_CODES];
      usize_t cl_base   [DJW_MAX_CLCLEN+2];
      usize_t cl_limit  [DJW_MAX_CLCLEN+2];
      uint8_t cl_mtf    [DJW_TOTAL_CODES];
      usize_t cl_minlen;
      usize_t cl_maxlen;

      /* Compute the code length decoder. */
      if ((ret = djw_decode_clclen (stream, & bstate, & input, input_end,
				    cl_inorder, cl_base, cl_limit, & cl_minlen,
				    & cl_maxlen, cl_mtf))) { goto fail; }

      /* Now decode each group decoder. */
      if ((ret = djw_decode_prefix (stream, & bstate, & input, input_end,
				    cl_inorder, cl_base, cl_limit,
				    & cl_minlen, & cl_maxlen, cl_mtf,
				    groups, clen[0]))) { goto fail; }

      /* Prepare the actual decoding tables. */
      for (gp = 0; gp < groups; gp += 1)
	{
	  djw_build_decoder (stream, ALPHABET_SIZE, DJW_MAX_CODELEN,
			     clen[gp], inorder[gp], base[gp], limit[gp],
			     & minlen[gp], & maxlen[gp]);
	}
    }

    /* Decode: selector clens. */
    {
      uint8_t sel_inorder[DJW_MAX_GROUPS+2];
      usize_t sel_base   [DJW_MAX_GBCLEN+2];
      usize_t sel_limit  [DJW_MAX_GBCLEN+2];
      uint8_t sel_mtf    [DJW_MAX_GROUPS+2];
      usize_t sel_minlen;
      usize_t sel_maxlen;

      /* Setup group selection. */
      if (groups > 1)
	{
	  uint8_t sel_clen[DJW_MAX_GROUPS+1];

	  for (gp = 0; gp < groups+1; gp += 1)
	    {
	      usize_t value;

	      if ((ret = xd3_decode_bits (stream, & bstate, & input,
					  input_end, DJW_GBCLEN_BITS,
					  & value))) { goto fail; }

	      sel_clen[gp] = value;
	      sel_mtf[gp]  = gp;
	    }

	  if ((sel_group = (uint8_t*) xd3_alloc (stream, sectors, 1)) == NULL)
	    {
	      ret = ENOMEM;
	      goto fail;
	    }

	  djw_build_decoder (stream, groups+1, DJW_MAX_GBCLEN, sel_clen,
			     sel_inorder, sel_base, sel_limit,
			     & sel_minlen, & sel_maxlen);

	  if ((ret = djw_decode_1_2 (stream, & bstate, & input, input_end,
				     sel_inorder, sel_base,
				     sel_limit, & sel_minlen,
				     & sel_maxlen, sel_mtf,
				     sectors, 0, sel_group))) { goto fail; }
	}

      /* Now decode each sector. */
      {
	/* Initialize for (groups==1) case. */
	uint8_t *gp_inorder = inorder[0]; 
	usize_t *gp_base    = base[0];
	usize_t *gp_limit   = limit[0];
	usize_t  gp_minlen  = minlen[0];
	usize_t  gp_maxlen  = maxlen[0];
	usize_t c;

	for (c = 0; c < sectors; c += 1)
	  {
	    usize_t n;

	    if (groups >= 2)
	      {
		gp = sel_group[c];

		XD3_ASSERT (gp < groups);

		gp_inorder = inorder[gp];
		gp_base    = base[gp];
		gp_limit   = limit[gp];
		gp_minlen  = minlen[gp];
		gp_maxlen  = maxlen[gp];
	      }

	    if (output_end < output)
	      {
		stream->msg = "secondary decoder invalid input";
		return XD3_INVALID_INPUT;
	      }
	    
	    /* Decode next sector. */
	    n = xd3_min (sector_size, (usize_t) (output_end - output));

	    do
	      {
		usize_t sym;

		if ((ret = djw_decode_symbol (stream, & bstate,
					      & input, input_end,
					      gp_inorder, gp_base,
					      gp_limit, gp_minlen, gp_maxlen,
					      & sym, ALPHABET_SIZE)))
		  {
		    goto fail;
		  }

		*output++ = sym;
	      }
	    while (--n);
	  }
      }
    }
  }

  IF_REGRESSION (if ((ret = xd3_test_clean_bits (stream, & bstate)))
		   { goto fail; });
  XD3_ASSERT (ret == 0);

 fail:
  xd3_free (stream, sel_group);

  (*input_pos) = input;
  (*output_pos) = output;
  return ret;
}

#endif
