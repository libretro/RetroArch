/* rvorbis - Ogg Vorbis decoder (stb_vorbis-derived), memory-only.
 *
 * The public API in <formats/rvorbis.h> is the pull interface used by
 * libretro-common's audio_transfer: open a whole file from memory,
 * query the stream info, pull interleaved f32 frames, seek by sample.
 *
 * Decode pipeline, in the order the code runs it:
 *
 *   Ogg layer   - page capture, segment table and packet framing
 *                 (start_page / next_segment / get8_packet);
 *   bitstream   - a 32-bit accumulator over the packet bytes feeds the
 *                 Huffman fast tables (prep_huffman / get_bits);
 *   headers     - codebooks, floors and residues are parsed and
 *                 preprocessed once (start_decoder), including the
 *                 fast-Huffman lookup tables and pre-expanded codebook
 *                 multiplicands used by the hot paths;
 *   audio       - each packet decodes floor curves and residue vectors
 *                 (decode_residue), undoes channel coupling, applies
 *                 the floor, and runs the inverse MDCT (SIMD kernels
 *                 near imdct_step3_iter0_loop) followed by the windowed
 *                 overlap-add in vorbis_finish_frame.
 */
/* Two complete pipelines, selected at runtime by which read the mixer
 * calls: rvorbis_get_samples_float_interleaved drives the float
 * pipeline (float MDCT and windowing, SIMD where available, native
 * float out), and rvorbis_get_samples_s16_interleaved drives the s16
 * pipeline (Q28 fixed-point inverse MDCT and windowing straight to the
 * s16 quantisation at the interleave copy).  Uniform Q28 samples with
 * Q27 coefficients: measured maxima across the corpus (including
 * full-scale square waves) stay below 1.7 at every point of the MDCT,
 * so the +-8 range of Q28 leaves ample headroom with no inter-stage
 * rescaling. */
#define RVQ_QBITS 28
#define RVQ_MULQ(x, c) \
   ((int32_t)((((int64_t)(x) * (c)) + ((int64_t)1 << 26)) >> 27))

#include <stdlib.h>
#include <string.h>
#include <math.h>
#if defined(_MSC_VER)
#include <malloc.h> /* MSVC provides alloca here */
#elif !defined(alloca)
#define alloca __builtin_alloca /* gcc/clang builtin: no header, portable everywhere */
#endif
#include <formats/rvorbis.h>

/*   ERROR CODES */

enum RVorbisError
{
   RVORBIS__no_error,

   RVORBIS_need_more_data=1,             /* not a real error */

   RVORBIS_outofmem,                     /* not enough memory */
   RVORBIS_feature_not_supported,        /* uses floor 0 */
   RVORBIS_too_many_channels,            /* RVORBIS_MAX_CHANNELS is too small */

   RVORBIS_unexpected_eof=10,            /* file is truncated? */
   RVORBIS_seek_invalid,                 /* seek past EOF */

   /* decoding errors (corrupt/invalid stream) -- you probably
    * don't care about the exact details of these */

   /* vorbis errors: */
   RVORBIS_invalid_setup=20,
   RVORBIS_invalid_stream,

   /* ogg errors: */
   RVORBIS_missing_capture_pattern=30,
   RVORBIS_invalid_stream_structure_version,
   RVORBIS_continued_packet_flag_invalid,
   RVORBIS_incorrect_stream_serial_number,
   RVORBIS_invalid_first_page,
   RVORBIS_bad_packet_type,
   RVORBIS_cant_find_last_page,
   RVORBIS_seek_failed
};

/* Global configuration settings (e.g. set these in the project/makefile),
 * or just set them in this file at the top (although ideally the first few
 * should be visible when the header file is compiled too, although it's not
 * crucial)
 */

/* RVORBIS_MAX_CHANNELS [number]
 *     Globally define this to the maximum number of channels you need.
 *     The spec does not put a restriction on channels except that
 *     the count is stored in a byte, so 255 is the hard limit.
 *     Reducing this saves about 16 bytes per value, so using 16 saves
 *     (255-16)*16 or around 4KB. Plus anything other memory usage
 *     I forgot to account for. Can probably go as low as 8 (7.1 audio),
 *     6 (5.1 audio), or 2 (stereo only).
 */
#ifndef RVORBIS_MAX_CHANNELS
#define RVORBIS_MAX_CHANNELS    16  /* enough for anyone? */
#endif

/* RVORBIS_FAST_HUFFMAN_LENGTH [number]
 *     sets the log size of the huffman-acceleration table.  Maximum
 *     supported value is 24. with larger numbers, more decodings are O(1),
 *     but the table size is larger so worse cache missing, so you'll have
 *     to probe (and try multiple ogg vorbis files) to find the sweet spot.
 */
#ifndef RVORBIS_FAST_HUFFMAN_LENGTH
#define RVORBIS_FAST_HUFFMAN_LENGTH   10
#endif

/* RVORBIS_FAST_BINARY_LENGTH [number]
 *     sets the log size of the binary-search acceleration table. this
 *     is used in similar fashion to the fast-huffman size to set initial
 *     parameters for the binary search

 * RVORBIS_FAST_HUFFMAN_INT
 *     The fast huffman tables are much more efficient if they can be
 *     stored as 16-bit results instead of 32-bit results. This restricts
 *     the codebooks to having only 65535 possible outcomes, though.
 *     (At least, accelerated by the huffman table.)
 */
#ifndef RVORBIS_FAST_HUFFMAN_INT
#define RVORBIS_FAST_HUFFMAN_SHORT
#endif

/* RVORBIS_CODEBOOK_SHORTS
 *     The vorbis file format encodes VQ codebook floats as ax+b where a and
 *     b are floating point per-codebook constants, and x is a 16-bit int.
 *     Normally, rvorbis decodes them to floats rather than leaving them
 *     as 16-bit ints and computing ax+b while decoding. This is a speed/space
 *     tradeoff; you can save space by defining this flag.
 */
#ifndef RVORBIS_CODEBOOK_SHORTS
#define RVORBIS_CODEBOOK_FLOATS
#endif

#include <retro_inline.h>

#define MAX_BLOCKSIZE_LOG  13   /* from specification */

#ifdef RVORBIS_CODEBOOK_FLOATS
typedef float rvorbis_codetype;
#else
typedef uint16_t rvorbis_codetype;
#endif

/* @NOTE
 *
 * Some arrays below are tagged "//varies", which means it's actually
 * a variable-sized piece of data, but rather than malloc I assume it's
 * small enough it's better to just allocate it all together with the
 * main thing
 *
 * Most of the variables are specified with the smallest size I could pack
 * them into. It might give better performance to make them all full-sized
 * integers. It should be safe to freely rearrange the structures or change
 * the sizes larger--nothing relies on silently truncating etc., nor the
 * order of variables.
 */

#define FAST_HUFFMAN_TABLE_SIZE   (1 << RVORBIS_FAST_HUFFMAN_LENGTH)
#define FAST_HUFFMAN_TABLE_MASK   (FAST_HUFFMAN_TABLE_SIZE - 1)

typedef struct
{
   int dimensions, entries;
   uint8_t *codeword_lengths;
   float  minimum_value;
   float  delta_value;
   uint8_t  value_bits;
   uint8_t  lookup_type;
   uint8_t  sequence_p;
   uint8_t  sparse;
   uint32_t lookup_values;
   rvorbis_codetype *multiplicands;
   uint32_t *codewords;
#ifdef RVORBIS_FAST_HUFFMAN_SHORT
    int16_t  fast_huffman[FAST_HUFFMAN_TABLE_SIZE];
#else
    int32_t  fast_huffman[FAST_HUFFMAN_TABLE_SIZE];
#endif
   uint32_t *sorted_codewords;
   int    *sorted_values;
   int     sorted_entries;
} Codebook;

typedef struct
{
   uint8_t order;
   uint16_t rate;
   uint16_t bark_map_size;
   uint8_t amplitude_bits;
   uint8_t amplitude_offset;
   uint8_t number_of_books;
   uint8_t book_list[16];             /* varies */
} Floor0;

typedef struct
{
   uint8_t partitions;
   uint8_t partition_class_list[32]; /* varies */
   uint8_t class_dimensions[16];     /* varies */
   uint8_t class_subclasses[16];     /* varies */
   uint8_t class_masterbooks[16];    /* varies */
   int16_t subclass_books[16][8];    /* varies */
   uint16_t Xlist[31*8+2];           /* varies */
   uint8_t sorted_order[31*8+2];
   uint8_t neighbors[31*8+2][2];
   uint8_t floor1_multiplier;
   uint8_t rangebits;
   int values;
} Floor1;

typedef union
{
   Floor0 floor0;
   Floor1 floor1;
} Floor;

typedef struct
{
   uint32_t begin, end;
   uint32_t part_size;
   uint8_t classifications;
   uint8_t classbook;
   uint8_t **classdata;
   int16_t (*residue_books)[8];
} Residue;

typedef struct
{
   uint8_t magnitude;
   uint8_t angle;
   uint8_t mux;
} MappingChannel;

typedef struct
{
   uint16_t coupling_steps;
   MappingChannel *chan;
   uint8_t  submaps;
   uint8_t  submap_floor[15];   /* varies */
   uint8_t  submap_residue[15]; /* varies */
} Mapping;

typedef struct
{
   uint8_t blockflag;
   uint8_t mapping;
   uint16_t windowtype;
   uint16_t transformtype;
} Mode;

typedef struct
{
   uint32_t  goal_crc;    /* expected crc if match */
   int     bytes_left;  /* bytes left in packet */
   uint32_t  crc_so_far;  /* running crc */
   int     bytes_done;  /* bytes processed in _current_ chunk */
   uint32_t  sample_loc;  /* granule pos encoded in page */
} CRCscan;

typedef struct
{
   uint32_t page_start, page_end;
   uint32_t after_previous_page_start;
   uint32_t first_decoded_sample;
   uint32_t last_decoded_sample;
} ProbedPage;

struct rvorbis
{
  /* user-accessible info */
   unsigned int sample_rate;
   int channels;

   unsigned int setup_memory_required;
   unsigned int temp_memory_required;
   unsigned int setup_temp_memory_required;

   uint8_t *stream;
   uint8_t *stream_start;
   uint8_t *stream_end;

   uint32_t stream_len;

   uint8_t  push_mode;

   uint32_t first_audio_page_offset;

   ProbedPage p_first, p_last;

  /* memory management */
   rvorbis_alloc alloc;
   int setup_offset;
   int temp_offset;

  /* run-time results */
   int eof;
   enum RVorbisError error;

  /* user-useful data */

  /* header info */
   int blocksize[2];
   int blocksize_0, blocksize_1;
   int codebook_count;
   Codebook *codebooks;
   int floor_count;
   uint16_t floor_types[64]; /* varies */
   Floor *floor_config;
   int residue_count;
   uint16_t residue_types[64]; /* varies */
   Residue *residue_config;
   int mapping_count;
   Mapping *mapping;
   int mode_count;
   Mode mode_config[64];  /* varies */

   uint32_t total_samples;

  /* decode buffer */
   float *channel_buffers[RVORBIS_MAX_CHANNELS];
   float *outputs        [RVORBIS_MAX_CHANNELS];

   float *previous_window[RVORBIS_MAX_CHANNELS];
   int previous_length;

   int16_t *finalY[RVORBIS_MAX_CHANNELS];

   uint32_t current_loc; /* sample location of next frame to decode */
   int    current_loc_valid;

  /* per-blocksize precomputed data */

   /* twiddle factors */
   float *A[2],*B[2],*C[2];
   float *window[2];
   /* Q27 integer twins of A/B/C and the window, used by the fixed-point
    * s16 route on scalar targets (NULL on SIMD targets). */
   int32_t *A_q[2], *B_q[2], *C_q[2];
   int32_t *window_q[2];
   /* Output-format latch: 0 = float pipeline, 1 = s16 pipeline.  Only
    * meaningful where the s16 route is fixed-point. */
   int s16_mode;
   uint16_t *bit_reverse[2];

  /* current page/packet/segment streaming info */
   uint32_t serial; /* stream serial number for verification */
   int last_page;
   int segment_count;
   uint8_t segments[255];
   uint8_t page_flag;
   uint8_t bytes_in_seg;
   uint8_t first_decode;
   int next_seg;
   int last_seg;  /* flag that we're on the last segment */
   int last_seg_which; /* what was the segment number of the last seg? */
   uint32_t acc;
   int valid_bits;
   int packet_bytes;
   int end_seg_with_known_loc;
   uint32_t known_loc_for_packet;
   int discard_samples_deferred;
   uint32_t samples_output;

  /* push mode scanning */
   int page_crc_tests; /* only in push_mode: number of tests active; -1 if not searching */

  /* sample-access */
   int channel_buffer_start;
   int channel_buffer_end;
};

typedef struct rvorbis vorb;

static int error(vorb *f, enum RVorbisError e)
{
   f->error = e;
   if (!f->eof && e != RVORBIS_need_more_data)
      f->error=e; /* breakpoint for debugging */
   return 0;
}

/* these functions are used for allocating temporary memory
 * while decoding. if you can afford the stack space, use
 * alloca(); otherwise, provide a temp buffer and it will
 * allocate out of those.
 */

#define array_size_required(count,size)  (count*(sizeof(void *)+(size)))

#define temp_alloc(f,size)              (f->alloc.alloc_buffer ? setup_temp_malloc(f,size) : alloca(size))
#define temp_alloc_save(f)              ((f)->temp_offset)
#define temp_alloc_restore(f,p)         ((f)->temp_offset = (p))

#define temp_block_array(f,count,size)  make_block_array(temp_alloc(f,array_size_required(count,size)), count, size)

/* given a sufficiently large block of memory, make an array of pointers to subblocks of it */
static void *make_block_array(void *mem, int count, int size)
{
   int i;
   void ** p = (void **) mem;
   char *q = (char *) (p + count);
   for (i=0; i < count; ++i)
   {
      p[i] = q;
      q += size;
   }
   return p;
}

static void *setup_malloc(vorb *f, int sz)
{
   sz = (sz+3) & ~3;
   f->setup_memory_required += sz;
   if (f->alloc.alloc_buffer)
   {
      void *p = (char *) f->alloc.alloc_buffer + f->setup_offset;
      if (f->setup_offset + sz > f->temp_offset) return NULL;
      f->setup_offset += sz;
      return p;
   }
   return sz ? malloc(sz) : NULL;
}

static void setup_free(vorb *f, void *p)
{
   if (f->alloc.alloc_buffer) return; /* do nothing; setup mem is not a stack */
   free(p);
}

static void *setup_temp_malloc(vorb *f, int sz)
{
   sz = (sz+3) & ~3;
   if (f->alloc.alloc_buffer)
   {
      if (f->temp_offset - sz < f->setup_offset)
         return NULL;
      f->temp_offset -= sz;
      return (char *) f->alloc.alloc_buffer + f->temp_offset;
   }
   return malloc(sz);
}

static void setup_temp_free(vorb *f, void *p, int sz)
{
   if (f->alloc.alloc_buffer)
   {
      f->temp_offset += (sz+3)&~3;
      return;
   }
   free(p);
}

#define CRC32_POLY    0x04c11db7   /* from spec */

static uint32_t rvorbis_crc_table[256];
static void crc32_init(void)
{
   int i,j;
   uint32_t s;
   for(i=0; i < 256; i++)
   {
      for (s=i<<24, j=0; j < 8; ++j)
         s = (s << 1) ^ (s >= (1U<<31) ? CRC32_POLY : 0);
      rvorbis_crc_table[i] = s;
   }
}

static INLINE uint32_t crc32_update(uint32_t crc, uint8_t byte)
{
   return (crc << 8) ^ rvorbis_crc_table[byte ^ (crc >> 24)];
}

/* used in setup, and for huffman that doesn't go fast path */
static unsigned int bit_reverse(unsigned int n)
{
  n = ((n & 0xAAAAAAAA) >>  1) | ((n & 0x55555555) << 1);
  n = ((n & 0xCCCCCCCC) >>  2) | ((n & 0x33333333) << 2);
  n = ((n & 0xF0F0F0F0) >>  4) | ((n & 0x0F0F0F0F) << 4);
  n = ((n & 0xFF00FF00) >>  8) | ((n & 0x00FF00FF) << 8);
  return (n >> 16) | (n << 16);
}

static float square(float x) { return x * x; }

/* this is a weird definition of log2() for which log2(1) = 1, log2(2) = 2, log2(4) = 3
 * as required by the specification. fast(?) implementation from stb.h
 * @OPTIMIZE: called multiple times per-packet with "constants"; move to setup
 */
static int ilog(int32_t n)
{
   static signed char log2_4[16] = { 0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4 };

   /* 2 compares if n < 16, 3 compares otherwise (4 if signed or n > 1<<29) */
   if (n < (1 << 14))
        if (n < (1 <<  4))        return     0 + log2_4[n      ];
        else if (n < (1 <<  9))      return  5 + log2_4[n >>  5];
             else                     return 10 + log2_4[n >> 10];
   else if (n < (1 << 24))
             if (n < (1 << 19))      return 15 + log2_4[n >> 15];
             else                     return 20 + log2_4[n >> 20];
        else if (n < (1 << 29))      return 25 + log2_4[n >> 25];
             else if (n < (1 << 31)) return 30 + log2_4[n >> 30];
                  else                return 0; /* signed n returns 0 */
}

#ifndef M_PI
  #define M_PI  3.14159265358979323846264f  /* from CRC */
#endif

/* code length assigned to a value with no huffman encoding */
#define NO_CODE   255

/* LEAF SETUP FUNCTIONS */

/* these functions are only called at setup, and only a few times
 * per file */

static float float32_unpack(uint32_t x)
{
   /* from the specification */
   uint32_t mantissa = x & 0x1fffff;
   uint32_t sign = x & 0x80000000;
   uint32_t exp = (x & 0x7fe00000) >> 21;
   double res = sign ? -(double)mantissa : (double)mantissa;
   return (float) ldexp((float)res, exp-788);
}


/* zlib & jpeg huffman tables assume that the output symbols
 * can either be arbitrarily arranged, or have monotonically
 * increasing frequencies--they rely on the lengths being sorted;
 * this makes for a very simple generation algorithm.
 * vorbis allows a huffman table with non-sorted lengths. This
 * requires a more sophisticated construction, since symbols in
 * order do not map to huffman codes "in order".
 */
static void add_entry(Codebook *c, uint32_t huff_code, int symbol, int count, int len, uint32_t *values)
{
   if (!c->sparse)
      c->codewords      [symbol] = huff_code;
   else
   {
      c->codewords       [count] = huff_code;
      c->codeword_lengths[count] = len;
      values             [count] = symbol;
   }
}

static int compute_codewords(Codebook *c, uint8_t *len, int n, uint32_t *values)
{
   int i,k,m=0;
   uint32_t available[32];

   memset(available, 0, sizeof(available));
   /* find the first entry */
   for (k=0; k < n; ++k) if (len[k] < NO_CODE) break;
   if (k == n)
   {
      assert(c->sorted_entries == 0);
      return 1;
   }
   /* add to the list */
   add_entry(c, 0, k, m++, len[k], values);
   /* add all available leaves */
   for (i=1; i <= len[k]; ++i)
      available[i] = 1 << (32-i);
   /* note that the above code treats the first case specially,
    * but it's really the same as the following code, so they
    * could probably be combined (except the initial code is 0,
    * and I use 0 in available[] to mean 'empty') */
   for (i=k+1; i < n; ++i)
   {
      uint32_t res;
      int z = len[i], y;
      if (z == NO_CODE)
         continue;
      /* find lowest available leaf (should always be earliest,
       * which is what the specification calls for)
       * note that this property, and the fact we can never have
       * more than one free leaf at a given level, isn't totally
       * trivial to prove, but it seems true and the assert never
       * fires, so! */
      while (z > 0 && !available[z]) --z;
      if (z == 0) { assert(0); return 0; }
      res = available[z];
      available[z] = 0;
      add_entry(c, bit_reverse(res), i, m++, len[i], values);
      /* propogate availability up the tree */
      if (z != len[i]) {
         for (y=len[i]; y > z; --y) {
            assert(available[y] == 0);
            available[y] = res + (1 << (32-y));
         }
      }
   }
   return 1;
}

/* accelerated huffman table allows fast O(1) match of all symbols
 * of length <= RVORBIS_FAST_HUFFMAN_LENGTH */
static void compute_accelerated_huffman(Codebook *c)
{
   int i, len;
   for (i=0; i < FAST_HUFFMAN_TABLE_SIZE; ++i)
      c->fast_huffman[i] = -1;

   len = c->sparse ? c->sorted_entries : c->entries;
#ifdef RVORBIS_FAST_HUFFMAN_SHORT
   if (len > 32767) len = 32767; /* largest possible value we can encode! */
#endif
   for (i=0; i < len; ++i)
   {
      if (c->codeword_lengths[i] <= RVORBIS_FAST_HUFFMAN_LENGTH)
      {
         uint32_t z = c->sparse ? bit_reverse(c->sorted_codewords[i]) : c->codewords[i];
         /* set table entries for all bit combinations in the higher bits */
         while (z < FAST_HUFFMAN_TABLE_SIZE)
         {
             c->fast_huffman[z] = i;
             z += 1 << c->codeword_lengths[i];
         }
      }
   }
}

#ifdef _MSC_VER
#define STBV_CDECL __cdecl
#else
#define STBV_CDECL
#endif

static int STBV_CDECL uint32_t_compare(const void *p, const void *q)
{
   uint32_t x = * (uint32_t *) p;
   uint32_t y = * (uint32_t *) q;
   return x < y ? -1 : x > y;
}

static int include_in_sort(Codebook *c, uint8_t len)
{
   if (c->sparse)
   {
      assert(len != NO_CODE);
      return 1;
   }
   if (len == NO_CODE)
      return 0;
   if (len > RVORBIS_FAST_HUFFMAN_LENGTH)
      return 1;
   return 0;
}

/* if the fast table above doesn't work, we want to binary
 * search them... need to reverse the bits */
static void compute_sorted_huffman(Codebook *c, uint8_t *lengths, uint32_t *values)
{
   int i, len;
   /* build a list of all the entries
    * OPTIMIZATION: don't include the short ones, since they'll be caught by FAST_HUFFMAN.
    * this is kind of a frivolous optimization--I don't see any performance improvement,
    * but it's like 4 extra lines of code, so. */
   if (!c->sparse)
   {
      int k = 0;
      for (i=0; i < c->entries; ++i)
         if (include_in_sort(c, lengths[i]))
            c->sorted_codewords[k++] = bit_reverse(c->codewords[i]);
      assert(k == c->sorted_entries);
   }
   else
   {
      for (i=0; i < c->sorted_entries; ++i)
         c->sorted_codewords[i] = bit_reverse(c->codewords[i]);
   }

   qsort(c->sorted_codewords, c->sorted_entries, sizeof(c->sorted_codewords[0]), uint32_t_compare);
   c->sorted_codewords[c->sorted_entries] = 0xffffffff;

   len = c->sparse ? c->sorted_entries : c->entries;
   /* now we need to indicate how they correspond; we could either
    *   #1: sort a different data structure that says who they correspond to
    *   #2: for each sorted entry, search the original list to find who corresponds
    *   #3: for each original entry, find the sorted entry
    * #1 requires extra storage, #2 is slow, #3 can use binary search! */
   for (i=0; i < len; ++i) {
      int huff_len = c->sparse ? lengths[values[i]] : lengths[i];
      if (include_in_sort(c,huff_len))
      {
         uint32_t code = bit_reverse(c->codewords[i]);
         int x=0, n=c->sorted_entries;
         while (n > 1)
         {
            /* invariant: sc[x] <= code < sc[x+n] */
            int m = x + (n >> 1);
            if (c->sorted_codewords[m] <= code)
            {
               x = m;
               n -= (n>>1);
            }
            else
               n >>= 1;
         }
         assert(c->sorted_codewords[x] == code);
         if (c->sparse)
         {
            c->sorted_values[x] = values[i];
            c->codeword_lengths[x] = huff_len;
         }
         else
            c->sorted_values[x] = i;
      }
   }
}

/* only run while parsing the header (3 times) */
static int vorbis_validate(uint8_t *data)
{
   static uint8_t vorbis[6] = { 'v', 'o', 'r', 'b', 'i', 's' };
   return memcmp(data, vorbis, 6) == 0;
}

/* called from setup only, once per code book
 * (formula implied by specification) */
static int lookup1_values(int entries, int dim)
{
   int r = (int) floor(exp((float) log((float) entries) / dim));
   if ((int) floor(pow((float) r+1, dim)) <= entries)   /* (int) cast for MinGW warning; */
      ++r;                                              /* floor() to avoid _ftol() when non-CRT */
   assert(pow((float) r+1, dim) > entries);
   assert((int) floor(pow((float) r, dim)) <= entries); /* (int),floor() as above */
   return r;
}

/* called twice per file */
static void compute_twiddle_factors(int n, float *A, float *B, float *C)
{
   int n4 = n >> 2, n8 = n >> 3;
   int k,k2;

   for (k=k2=0; k < n4; ++k,k2+=2)
   {
      A[k2  ] = (float)  cos(4*k*M_PI/n);
      A[k2+1] = (float) -sin(4*k*M_PI/n);
      B[k2  ] = (float)  cos((k2+1)*M_PI/n/2) * 0.5f;
      B[k2+1] = (float)  sin((k2+1)*M_PI/n/2) * 0.5f;
   }
   for (k=k2=0; k < n8; ++k,k2+=2)
   {
      C[k2  ] = (float)  cos(2*(k2+1)*M_PI/n);
      C[k2+1] = (float) -sin(2*(k2+1)*M_PI/n);
   }
}

static void compute_window(int n, float *window)
{
   int n2 = n >> 1, i;
   for (i=0; i < n2; ++i)
      window[i] = (float) sin(0.5 * M_PI * square((float) sin((i - 0 + 0.5) / n2 * 0.5 * M_PI)));
}

static void compute_bitreverse(int n, uint16_t *rev)
{
   int ld = ilog(n) - 1; /* ilog is off-by-one from normal definitions */
   int i, n8 = n >> 3;
   for (i=0; i < n8; ++i)
      rev[i] = (bit_reverse(i) >> (32-ld+3)) << 2;
}

static int init_blocksize(vorb *f, int b, int n)
{
   int n2 = n >> 1, n4 = n >> 2, n8 = n >> 3;
   f->A[b] = (float *) setup_malloc(f, sizeof(float) * n2);
   f->B[b] = (float *) setup_malloc(f, sizeof(float) * n2);
   f->C[b] = (float *) setup_malloc(f, sizeof(float) * n4);
   if (!f->A[b] || !f->B[b] || !f->C[b]) return error(f, RVORBIS_outofmem);
   compute_twiddle_factors(n, f->A[b], f->B[b], f->C[b]);
   f->window[b] = (float *) setup_malloc(f, sizeof(float) * n2);
   if (!f->window[b]) return error(f, RVORBIS_outofmem);
   compute_window(n, f->window[b]);

   {
      /* Q27 integer twins for the fixed-point s16 pipeline, generated
       * from the float tables computed above. */
      int k;
      f->A_q[b]      = (int32_t *) setup_malloc(f, sizeof(int32_t) * n2);
      f->B_q[b]      = (int32_t *) setup_malloc(f, sizeof(int32_t) * n2);
      f->C_q[b]      = (int32_t *) setup_malloc(f, sizeof(int32_t) * n4);
      f->window_q[b] = (int32_t *) setup_malloc(f, sizeof(int32_t) * n2);
      if (!f->A_q[b] || !f->B_q[b] || !f->C_q[b] || !f->window_q[b])
         return error(f, RVORBIS_outofmem);
      for (k = 0; k < n2; k++)
      {
         f->A_q[b][k] = (int32_t)(f->A[b][k] * (float)(1 << 27)
               + ((f->A[b][k] >= 0.0f) ? 0.5f : -0.5f));
         f->B_q[b][k] = (int32_t)(f->B[b][k] * (float)(1 << 27)
               + ((f->B[b][k] >= 0.0f) ? 0.5f : -0.5f));
         f->window_q[b][k] = (int32_t)(f->window[b][k] * (float)(1 << 27)
               + ((f->window[b][k] >= 0.0f) ? 0.5f : -0.5f));
      }
      for (k = 0; k < n4; k++)
         f->C_q[b][k] = (int32_t)(f->C[b][k] * (float)(1 << 27)
               + ((f->C[b][k] >= 0.0f) ? 0.5f : -0.5f));
   }
   f->bit_reverse[b] = (uint16_t *) setup_malloc(f, sizeof(uint16_t) * n8);
   if (!f->bit_reverse[b]) return error(f, RVORBIS_outofmem);
   compute_bitreverse(n, f->bit_reverse[b]);
   return 1;
}

static void neighbors(uint16_t *x, int n, int *plow, int *phigh)
{
   int low = -1;
   int high = 65536;
   int i;
   for (i=0; i < n; ++i)
   {
      if (x[i] > low  && x[i] < x[n])
      {
         *plow  = i;
         low    = x[i];
      }
      if (x[i] < high && x[i] > x[n])
      {
         *phigh = i;
         high = x[i];
      }
   }
}

/* this has been repurposed so y is now the original index instead of y */
typedef struct
{
   uint16_t x,y;
} STBV_Point;

static int STBV_CDECL point_compare(const void *p, const void *q)
{
   STBV_Point *a = (STBV_Point *) p;
   STBV_Point *b = (STBV_Point *) q;
   return a->x < b->x ? -1 : a->x > b->x;
}

/* END LEAF SETUP FUNCTIONS */

static uint8_t get8(vorb *z)
{
   if (z->stream >= z->stream_end)
   {
      z->eof = 1;
      return 0;
   }
   return *z->stream++;
}

static uint32_t get32(vorb *f)
{
   uint32_t x = get8(f);
   x += get8(f) << 8;
   x += get8(f) << 16;
   x += get8(f) << 24;
   return x;
}

static int getn(vorb *z, uint8_t *data, int n)
{
   if (z->stream+n > z->stream_end)
   {
      z->eof = 1;
      return 0;
   }
   memcpy(data, z->stream, n);
   z->stream += n;
   return 1;
}

static void skip(vorb *z, int n)
{
   z->stream += n;
   if (z->stream >= z->stream_end) z->eof = 1;
   return;
}

static int set_file_offset(rvorbis *f, unsigned int loc)
{
   f->eof = 0;
   if (f->stream_start + loc >= f->stream_end || f->stream_start + loc < f->stream_start)
   {
      f->stream = f->stream_end;
      f->eof    = 1;
      return 0;
   }
   f->stream = f->stream_start + loc;
   return 1;
}

static uint8_t ogg_page_header[4] = { 0x4f, 0x67, 0x67, 0x53 };

#define PAGEFLAG_continued_packet   1
#define PAGEFLAG_first_page         2
#define PAGEFLAG_last_page          4

static int start_page_no_capturepattern(vorb *f)
{
   uint32_t loc0,loc1,n;
   /* stream structure version */
   if (0 != get8(f))
      return error(f, RVORBIS_invalid_stream_structure_version);
   /* header flag */
   f->page_flag = get8(f);
   /* absolute granule position */
   loc0 = get32(f);
   loc1 = get32(f);
   /* @TODO: validate loc0,loc1 as valid positions?
    * stream serial number -- vorbis doesn't interleave, so discard */
   get32(f);
   /*if (f->serial != get32(f)) return error(f, RVORBIS_incorrect_stream_serial_number);
    * page sequence number */
   n = get32(f);
   f->last_page = n;
   /* CRC32 */
   get32(f);
   /* page_segments */
   f->segment_count = get8(f);
   if (!getn(f, f->segments, f->segment_count))
      return error(f, RVORBIS_unexpected_eof);
   /* assume we _don't_ know any the sample position of any segments */
   f->end_seg_with_known_loc = -2;
   if (loc0 != ~0U || loc1 != ~0U)
   {
      int i;
      /* determine which packet is the last one that will complete */
      for (i=f->segment_count-1; i >= 0; --i)
         if (f->segments[i] < 255)
            break;
      /* 'i' is now the index of the _last_ segment of a packet that ends */
      if (i >= 0)
      {
         f->end_seg_with_known_loc = i;
         f->known_loc_for_packet   = loc0;
      }
   }
   if (f->first_decode)
   {
      int i,len;
      ProbedPage p;
      len = 0;
      for (i=0; i < f->segment_count; ++i)
         len += f->segments[i];
      len += 27 + f->segment_count;
      p.page_start = f->first_audio_page_offset;
      p.page_end = p.page_start + len;
      p.after_previous_page_start = p.page_start;
      p.first_decoded_sample = 0;
      p.last_decoded_sample = loc0;
      f->p_first = p;
   }
   f->next_seg = 0;
   return 1;
}

static int start_page(vorb *f)
{
   if (0x4f != get8(f))
      return error(f, RVORBIS_missing_capture_pattern);
   if (0x67 != get8(f))
      return error(f, RVORBIS_missing_capture_pattern);
   if (0x67 != get8(f))
      return error(f, RVORBIS_missing_capture_pattern);
   if (0x53 != get8(f))
      return error(f, RVORBIS_missing_capture_pattern);
   return start_page_no_capturepattern(f);
}

static int start_packet(vorb *f)
{
   while (f->next_seg == -1)
   {
      if (!start_page(f))
         return 0;
      if (f->page_flag & PAGEFLAG_continued_packet)
         return error(f, RVORBIS_continued_packet_flag_invalid);
   }
   f->last_seg     = 0;
   f->valid_bits   = 0;
   f->packet_bytes = 0;
   f->bytes_in_seg = 0;
   /* f->next_seg is now valid */
   return 1;
}

static int maybe_start_packet(vorb *f)
{
   if (f->next_seg == -1)
   {
      int x = get8(f);
      if (f->eof)
         return 0; /* EOF at page boundary is not an error! */
      if (0x4f != x      )
         return error(f, RVORBIS_missing_capture_pattern);
      if (0x67 != get8(f))
         return error(f, RVORBIS_missing_capture_pattern);
      if (0x67 != get8(f))
         return error(f, RVORBIS_missing_capture_pattern);
      if (0x53 != get8(f))
         return error(f, RVORBIS_missing_capture_pattern);
      if (!start_page_no_capturepattern(f))
         return 0;
      if (f->page_flag & PAGEFLAG_continued_packet)
      {
         /* set up enough state that we can read this packet if we want,
          * e.g. during recovery */
         f->last_seg = 0;
         f->bytes_in_seg = 0;
         return error(f, RVORBIS_continued_packet_flag_invalid);
      }
   }
   return start_packet(f);
}

static int next_segment(vorb *f)
{
   int len;
   if (f->last_seg)
      return 0;
   if (f->next_seg == -1)
   {
      f->last_seg_which = f->segment_count-1; /* in case start_page fails */
      if (!start_page(f))
      {
         f->last_seg = 1;
         return 0;
      }
      if (!(f->page_flag & PAGEFLAG_continued_packet)) return error(f, RVORBIS_continued_packet_flag_invalid);
   }
   len = f->segments[f->next_seg++];
   if (len < 255)
   {
      f->last_seg = 1;
      f->last_seg_which = f->next_seg-1;
   }
   if (f->next_seg >= f->segment_count)
      f->next_seg = -1;
   assert(f->bytes_in_seg == 0);
   f->bytes_in_seg = len;
   return len;
}

#define EOP    (-1)
#define INVALID_BITS  (-1)

static int get8_packet_raw(vorb *f)
{
   if (!f->bytes_in_seg) {  /* CLANG! */
      if (f->last_seg) return EOP;
      else if (!next_segment(f)) return EOP;
   }
   assert(f->bytes_in_seg > 0);
   --f->bytes_in_seg;
   ++f->packet_bytes;
   return get8(f);
}

static int get8_packet(vorb *f)
{
   int x = get8_packet_raw(f);
   f->valid_bits = 0;
   return x;
}

/* @OPTIMIZE: this is the secondary bit decoder, 
 * so it's probably not as important
 * as the huffman decoder? */
static uint32_t get_bits(vorb *f, int n)
{
   uint32_t z;

   if (f->valid_bits < 0)
      return 0;
   if (f->valid_bits < n)
   {
      if (n > 24)
      {
         /* The accumulator technique below would not 
          * work correctly in this case */
         z  = get_bits(f, 24);
         z += get_bits(f, n-24) << 24;
         return z;
      }
      if (f->valid_bits == 0)
         f->acc = 0;
      while (f->valid_bits < n)
      {
         int z = get8_packet_raw(f);
         if (z == EOP)
         {
            f->valid_bits = INVALID_BITS;
            return 0;
         }
         f->acc += z << f->valid_bits;
         f->valid_bits += 8;
      }
   }
   if (f->valid_bits < 0)
      return 0;
   z = f->acc & ((1 << n)-1);
   f->acc >>= n;
   f->valid_bits -= n;
   return z;
}

/* @OPTIMIZE: primary accumulator for huffman
 * expand the buffer to as many bits as possible without 
 * reading off end of packet it might be nice to allow 
 * f->valid_bits and f->acc to be stored in registers,
 * e.g. cache them locally and decode locally */
static INLINE void prep_huffman(vorb *f)
{
   if (f->valid_bits <= 24)
   {
      if (f->valid_bits == 0)
         f->acc = 0;
      do
      {
         int z;
         if (f->last_seg && !f->bytes_in_seg)
            return;
         z = get8_packet_raw(f);
         if (z == EOP)
            return;
         f->acc += z << f->valid_bits;
         f->valid_bits += 8;
      } while (f->valid_bits <= 24);
   }
}

enum
{
   RVORBIS_packet_id = 1,
   RVORBIS_packet_comment = 3,
   RVORBIS_packet_setup = 5
};

static int codebook_decode_scalar_raw(vorb *f, Codebook *c)
{
   int i;
   prep_huffman(f);

   assert(c->sorted_codewords || c->codewords);
   /* cases to use binary search: sorted_codewords && !c->codewords
    *                             sorted_codewords && c->entries > 8 */
   if (c->entries > 8 ? c->sorted_codewords!=NULL : !c->codewords)
   {
      /* binary search */
      uint32_t code = bit_reverse(f->acc);
      int x=0, n=c->sorted_entries, len;

      while (n > 1)
      {
         /* invariant: sc[x] <= code < sc[x+n] */
         int m = x + (n >> 1);
         if (c->sorted_codewords[m] <= code)
         {
            x = m;
            n -= (n>>1);
         }
         else
            n >>= 1;
      }
      /* x is now the sorted index */
      if (!c->sparse)
         x = c->sorted_values[x];
      /* x is now sorted index if sparse, or symbol otherwise */
      len = c->codeword_lengths[x];
      if (f->valid_bits >= len)
      {
         f->acc >>= len;
         f->valid_bits -= len;
         return x;
      }

      f->valid_bits = 0;
      return -1;
   }

   /* if small, linear search */
   assert(!c->sparse);
   for (i=0; i < c->entries; ++i)
   {
      if (c->codeword_lengths[i] == NO_CODE)
         continue;
      if (c->codewords[i] == (f->acc & ((1 << c->codeword_lengths[i])-1)))
      {
         if (f->valid_bits >= c->codeword_lengths[i])
         {
            f->acc >>= c->codeword_lengths[i];
            f->valid_bits -= c->codeword_lengths[i];
            return i;
         }
         f->valid_bits = 0;
         return -1;
      }
   }

   error(f, RVORBIS_invalid_stream);
   f->valid_bits = 0;
   return -1;
}


/* Hot path of the residue and floor decode: one Huffman symbol via the
 * precomputed fast table, falling back to the sorted-codeword search
 * only on long codes.  Called tens of millions of times per second of
 * audio, so the fast path is inlined into its callers; the slow path
 * stays out of line. */
static INLINE int codebook_decode_scalar(vorb *f, Codebook *c)
{
   int i;
   if (f->valid_bits < RVORBIS_FAST_HUFFMAN_LENGTH)
      prep_huffman(f);
   /* fast huffman table lookup */
   i = f->acc & FAST_HUFFMAN_TABLE_MASK;
   i = c->fast_huffman[i];
   if (i >= 0)
   {
      f->acc >>= c->codeword_lengths[i];
      f->valid_bits -= c->codeword_lengths[i];
      if (f->valid_bits < 0)
      {
         f->valid_bits = 0;
         return -1;
      }
      return i;
   }
   return codebook_decode_scalar_raw(f,c);
}

/* CODEBOOK_ELEMENT_FAST is an optimization for the CODEBOOK_FLOATS case
 * where we avoid one addition */
#ifndef RVORBIS_CODEBOOK_FLOATS
   #define CODEBOOK_ELEMENT(c,off)          (c->multiplicands[off] * c->delta_value + c->minimum_value)
   #define CODEBOOK_ELEMENT_FAST(c,off)     (c->multiplicands[off] * c->delta_value)
   #define CODEBOOK_ELEMENT_BASE(c)         (c->minimum_value)
#else
   #define CODEBOOK_ELEMENT(c,off)          (c->multiplicands[off])
   #define CODEBOOK_ELEMENT_FAST(c,off)     (c->multiplicands[off])
   #define CODEBOOK_ELEMENT_BASE(c)         (0)
#endif

static int codebook_decode_start(vorb *f, Codebook *c)
{
   int z = -1;

   /* type 0 is only legal in a scalar context */
   if (c->lookup_type == 0)
      error(f, RVORBIS_invalid_stream);
   else
   {
      z = codebook_decode_scalar(f,c);
      if (c->sparse)
         assert(z < c->sorted_entries);
      if (z < 0) /* check for EOP */
      {
         if (!f->bytes_in_seg)
            if (f->last_seg)
               return z;
         error(f, RVORBIS_invalid_stream);
      }
   }
   return z;
}

static int codebook_decode(vorb *f, Codebook *c, float *output, int len)
{
   int i,z = codebook_decode_start(f,c);
   if (z < 0)
      return 0;
   if (len > c->dimensions)
      len = c->dimensions;

   z *= c->dimensions;
   if (c->sequence_p)
   {
      float last = CODEBOOK_ELEMENT_BASE(c);
      for (i=0; i < len; ++i)
      {
         float val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
         output[i] += val;
         last = val + c->minimum_value;
      }
   }
   else
   {
      float last = CODEBOOK_ELEMENT_BASE(c);
      for (i=0; i < len; ++i)
         output[i] += CODEBOOK_ELEMENT_FAST(c,z+i) + last;
   }

   return 1;
}

static int codebook_decode_step(vorb *f, Codebook *c, float *output, int len, int step)
{
   int i,z = codebook_decode_start(f,c);
   float last = CODEBOOK_ELEMENT_BASE(c);
   if (z < 0)
      return 0;
   if (len > c->dimensions)
      len = c->dimensions;

   z *= c->dimensions;
   for (i=0; i < len; ++i)
   {
      float val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
      output[i*step] += val;
      if (c->sequence_p)
         last = val;
   }

   return 1;
}

static int codebook_decode_deinterleave_repeat(vorb *f, Codebook *c, float **outputs, int ch, int *c_inter_p, int *p_inter_p, int len, int total_decode)
{
   int c_inter = *c_inter_p;
   int p_inter = *p_inter_p;
   int i,z, effective = c->dimensions;

   /* type 0 is only legal in a scalar context */
   if (c->lookup_type == 0)   return error(f, RVORBIS_invalid_stream);

   while (total_decode > 0)
   {
      float last = CODEBOOK_ELEMENT_BASE(c);
      z = codebook_decode_scalar(f,c);
      assert(!c->sparse || z < c->sorted_entries);
      if (z < 0)
      {
         if (!f->bytes_in_seg)
            if (f->last_seg) return 0;
         return error(f, RVORBIS_invalid_stream);
      }

      /* if this will take us off the end of the buffers, stop short!
       * we check by computing the length of the virtual interleaved
       * buffer (len*ch), our current offset within it (p_inter*ch)+(c_inter),
       * and the length we'll be using (effective) */
      if (c_inter + p_inter*ch + effective > len * ch) {
         effective = len*ch - (p_inter*ch - c_inter);
      }

      z *= c->dimensions;
      if (c->sequence_p) {
         for (i=0; i < effective; ++i) {
            float val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
            if (outputs[c_inter])
               outputs[c_inter][p_inter] += val;
            if (++c_inter == ch) { c_inter = 0; ++p_inter; }
            last = val;
         }
      } else {
         for (i=0; i < effective; ++i) {
            float val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
            if (outputs[c_inter])
               outputs[c_inter][p_inter] += val;
            if (++c_inter == ch) { c_inter = 0; ++p_inter; }
         }
      }

      total_decode -= effective;
   }
   *c_inter_p = c_inter;
   *p_inter_p = p_inter;
   return 1;
}

static int codebook_decode_deinterleave_repeat_2(vorb *f, Codebook *c, float **outputs, int *c_inter_p, int *p_inter_p, int len, int total_decode)
{
   int c_inter = *c_inter_p;
   int p_inter = *p_inter_p;
   int i,z, effective = c->dimensions;

   /* type 0 is only legal in a scalar context */
   if (c->lookup_type == 0)   return error(f, RVORBIS_invalid_stream);

   while (total_decode > 0)
   {
      float last = CODEBOOK_ELEMENT_BASE(c);
      z = codebook_decode_scalar(f,c);

      if (z < 0) {
         if (!f->bytes_in_seg)
            if (f->last_seg) return 0;
         return error(f, RVORBIS_invalid_stream);
      }

      /* if this will take us off the end of the buffers, stop short!
       * we check by computing the length of the virtual interleaved
       * buffer (len*ch), our current offset within it (p_inter*ch)+(c_inter),
       * and the length we'll be using (effective)
       */
      if (c_inter + p_inter*2 + effective > len * 2)
         effective = len*2 - (p_inter*2 - c_inter);

      z *= c->dimensions;
      if (c->sequence_p) {
         /* haven't optimized this case because I don't have any examples */
         for (i=0; i < effective; ++i) {
            float val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
            if (outputs[c_inter])
               outputs[c_inter][p_inter] += val;
            if (++c_inter == 2) { c_inter = 0; ++p_inter; }
            last = val;
         }
      } else {
         i=0;
         if (c_inter == 1) {
            float val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
            if (outputs[c_inter])
               outputs[c_inter][p_inter] += val;
            c_inter = 0; ++p_inter;
            ++i;
         }
         {
            float *z0 = outputs[0];
            float *z1 = outputs[1];
            for (; i+1 < effective;) {
               float v0 = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
               float v1 = CODEBOOK_ELEMENT_FAST(c,z+i+1) + last;
               if (z0)
                  z0[p_inter] += v0;
               if (z1)
                  z1[p_inter] += v1;
               ++p_inter;
               i += 2;
            }
         }
         if (i < effective) {
            float val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
            if (outputs[c_inter])
               outputs[c_inter][p_inter] += val;
            if (++c_inter == 2) { c_inter = 0; ++p_inter; }
         }
      }

      total_decode -= effective;
   }
   *c_inter_p = c_inter;
   *p_inter_p = p_inter;
   return 1;
}

static int predict_point(int x, int x0, int x1, int y0, int y1)
{
   int dy = y1 - y0;
   int adx = x1 - x0;
   /* @OPTIMIZE: force int division to round in the right direction... is this necessary on x86? */
   int err = abs(dy) * (x - x0);
   int off = err / adx;
   return dy < 0 ? y0 - off : y0 + off;
}

/* the following table is block-copied from the specification */
static float inverse_db_table[256] =
{
  1.0649863e-07f, 1.1341951e-07f, 1.2079015e-07f, 1.2863978e-07f,
  1.3699951e-07f, 1.4590251e-07f, 1.5538408e-07f, 1.6548181e-07f,
  1.7623575e-07f, 1.8768855e-07f, 1.9988561e-07f, 2.1287530e-07f,
  2.2670913e-07f, 2.4144197e-07f, 2.5713223e-07f, 2.7384213e-07f,
  2.9163793e-07f, 3.1059021e-07f, 3.3077411e-07f, 3.5226968e-07f,
  3.7516214e-07f, 3.9954229e-07f, 4.2550680e-07f, 4.5315863e-07f,
  4.8260743e-07f, 5.1396998e-07f, 5.4737065e-07f, 5.8294187e-07f,
  6.2082472e-07f, 6.6116941e-07f, 7.0413592e-07f, 7.4989464e-07f,
  7.9862701e-07f, 8.5052630e-07f, 9.0579828e-07f, 9.6466216e-07f,
  1.0273513e-06f, 1.0941144e-06f, 1.1652161e-06f, 1.2409384e-06f,
  1.3215816e-06f, 1.4074654e-06f, 1.4989305e-06f, 1.5963394e-06f,
  1.7000785e-06f, 1.8105592e-06f, 1.9282195e-06f, 2.0535261e-06f,
  2.1869758e-06f, 2.3290978e-06f, 2.4804557e-06f, 2.6416497e-06f,
  2.8133190e-06f, 2.9961443e-06f, 3.1908506e-06f, 3.3982101e-06f,
  3.6190449e-06f, 3.8542308e-06f, 4.1047004e-06f, 4.3714470e-06f,
  4.6555282e-06f, 4.9580707e-06f, 5.2802740e-06f, 5.6234160e-06f,
  5.9888572e-06f, 6.3780469e-06f, 6.7925283e-06f, 7.2339451e-06f,
  7.7040476e-06f, 8.2047000e-06f, 8.7378876e-06f, 9.3057248e-06f,
  9.9104632e-06f, 1.0554501e-05f, 1.1240392e-05f, 1.1970856e-05f,
  1.2748789e-05f, 1.3577278e-05f, 1.4459606e-05f, 1.5399272e-05f,
  1.6400004e-05f, 1.7465768e-05f, 1.8600792e-05f, 1.9809576e-05f,
  2.1096914e-05f, 2.2467911e-05f, 2.3928002e-05f, 2.5482978e-05f,
  2.7139006e-05f, 2.8902651e-05f, 3.0780908e-05f, 3.2781225e-05f,
  3.4911534e-05f, 3.7180282e-05f, 3.9596466e-05f, 4.2169667e-05f,
  4.4910090e-05f, 4.7828601e-05f, 5.0936773e-05f, 5.4246931e-05f,
  5.7772202e-05f, 6.1526565e-05f, 6.5524908e-05f, 6.9783085e-05f,
  7.4317983e-05f, 7.9147585e-05f, 8.4291040e-05f, 8.9768747e-05f,
  9.5602426e-05f, 0.00010181521f, 0.00010843174f, 0.00011547824f,
  0.00012298267f, 0.00013097477f, 0.00013948625f, 0.00014855085f,
  0.00015820453f, 0.00016848555f, 0.00017943469f, 0.00019109536f,
  0.00020351382f, 0.00021673929f, 0.00023082423f, 0.00024582449f,
  0.00026179955f, 0.00027881276f, 0.00029693158f, 0.00031622787f,
  0.00033677814f, 0.00035866388f, 0.00038197188f, 0.00040679456f,
  0.00043323036f, 0.00046138411f, 0.00049136745f, 0.00052329927f,
  0.00055730621f, 0.00059352311f, 0.00063209358f, 0.00067317058f,
  0.00071691700f, 0.00076350630f, 0.00081312324f, 0.00086596457f,
  0.00092223983f, 0.00098217216f, 0.0010459992f,  0.0011139742f,
  0.0011863665f,  0.0012634633f,  0.0013455702f,  0.0014330129f,
  0.0015261382f,  0.0016253153f,  0.0017309374f,  0.0018434235f,
  0.0019632195f,  0.0020908006f,  0.0022266726f,  0.0023713743f,
  0.0025254795f,  0.0026895994f,  0.0028643847f,  0.0030505286f,
  0.0032487691f,  0.0034598925f,  0.0036847358f,  0.0039241906f,
  0.0041792066f,  0.0044507950f,  0.0047400328f,  0.0050480668f,
  0.0053761186f,  0.0057254891f,  0.0060975636f,  0.0064938176f,
  0.0069158225f,  0.0073652516f,  0.0078438871f,  0.0083536271f,
  0.0088964928f,  0.009474637f,   0.010090352f,   0.010746080f,
  0.011444421f,   0.012188144f,   0.012980198f,   0.013823725f,
  0.014722068f,   0.015678791f,   0.016697687f,   0.017782797f,
  0.018938423f,   0.020169149f,   0.021479854f,   0.022875735f,
  0.024362330f,   0.025945531f,   0.027631618f,   0.029427276f,
  0.031339626f,   0.033376252f,   0.035545228f,   0.037855157f,
  0.040315199f,   0.042935108f,   0.045725273f,   0.048696758f,
  0.051861348f,   0.055231591f,   0.058820850f,   0.062643361f,
  0.066714279f,   0.071049749f,   0.075666962f,   0.080584227f,
  0.085821044f,   0.091398179f,   0.097337747f,   0.10366330f,
  0.11039993f,    0.11757434f,    0.12521498f,    0.13335215f,
  0.14201813f,    0.15124727f,    0.16107617f,    0.17154380f,
  0.18269168f,    0.19456402f,    0.20720788f,    0.22067342f,
  0.23501402f,    0.25028656f,    0.26655159f,    0.28387361f,
  0.30232132f,    0.32196786f,    0.34289114f,    0.36517414f,
  0.38890521f,    0.41417847f,    0.44109412f,    0.46975890f,
  0.50028648f,    0.53279791f,    0.56742212f,    0.60429640f,
  0.64356699f,    0.68538959f,    0.72993007f,    0.77736504f,
  0.82788260f,    0.88168307f,    0.9389798f,     1.0f
};


/* @OPTIMIZE: if you want to replace this bresenham line-drawing routine,
 * note that you must produce bit-identical output to decode correctly;
 * this specific sequence of operations is specified in the spec (it's
 * drawing integer-quantized frequency-space lines that the encoder
 * expects to be exactly the same)
 *     ... also, isn't the whole point of Bresenham's algorithm to NOT
 * have to divide in the setup? sigh.
 */

static INLINE void draw_line(float *output, int x0, int y0, int x1, int y1, int n)
{
   int dy = y1 - y0;
   int adx = x1 - x0;
   int ady = abs(dy);
   int x=x0,y=y0;
   int err = 0;
   int sy;
   int base = dy / adx;

   if (dy < 0)
      sy = base - 1;
   else
      sy = base+1;

   ady -= abs(base) * adx;
   if (x1 > n)
      x1 = n;
   output[x] *= inverse_db_table[y];
   for (++x; x < x1; ++x)
   {
      err += ady;
      if (err >= adx)
      {
         err -= adx;
         y += sy;
      }
      else
         y += base;
      output[x] *= inverse_db_table[y];
   }
}

static int residue_decode(vorb *f, Codebook *book, float *target, int offset, int n, int rtype)
{
   int k;
   if (rtype == 0)
   {
      int step = n / book->dimensions;
      for (k=0; k < step; ++k)
         if (!codebook_decode_step(f, book, target+offset+k, n-offset-k, step))
            return 0;
   }
   else
   {
      for (k=0; k < n; ) {
         if (!codebook_decode(f, book, target+offset, n-k))
            return 0;
         k += book->dimensions;
         offset += book->dimensions;
      }
   }
   return 1;
}

static void decode_residue(vorb *f, float *residue_buffers[], int ch, int n, int rn, uint8_t *do_not_decode)
{
   int i,j,pass;
   Residue *r = f->residue_config + rn;
   int rtype = f->residue_types[rn];
   int c = r->classbook;
   int classwords = f->codebooks[c].dimensions;
   int n_read = r->end - r->begin;
   int part_read = n_read / r->part_size;
   int temp_alloc_point = temp_alloc_save(f);
   uint8_t ***part_classdata = (uint8_t ***) temp_block_array(f,f->channels, part_read * sizeof(**part_classdata));

   for (i=0; i < ch; ++i)
      if (!do_not_decode[i])
         memset(residue_buffers[i], 0, sizeof(float) * n);

   if (rtype == 2 && ch != 1) {
      for (j=0; j < ch; ++j)
         if (!do_not_decode[j])
            break;
      if (j == ch)
         goto done;

      for (pass=0; pass < 8; ++pass)
      {
         int pcount = 0, class_set = 0;
         if (ch == 2)
         {
            while (pcount < part_read) {
               int z = r->begin + pcount*r->part_size;
               int c_inter = (z & 1), p_inter = z>>1;
               if (pass == 0) {
                  Codebook *c = f->codebooks+r->classbook;
                  int q = codebook_decode_scalar(f,c);
                  if (c->sparse)
                     q = c->sorted_values[q];
                  if (q == EOP)
                     goto done;
                  part_classdata[0][class_set] = r->classdata[q];
               }
               for (i=0; i < classwords && pcount < part_read; ++i, ++pcount) {
                  int z = r->begin + pcount*r->part_size;
                  int c = part_classdata[0][class_set][i];
                  int b = r->residue_books[c][pass];
                  if (b >= 0) {
                     Codebook *book = f->codebooks + b;
                     /* saves 1% */
                     if (!codebook_decode_deinterleave_repeat_2(f, book, residue_buffers, &c_inter, &p_inter, n, r->part_size))
                        goto done;
                  } else {
                     z += r->part_size;
                     c_inter = z & 1;
                     p_inter = z >> 1;
                  }
               }
               ++class_set;
            }
         }
         else if (ch == 1)
         {
            while (pcount < part_read) {
               int z = r->begin + pcount*r->part_size;
               int c_inter = 0, p_inter = z;
               if (pass == 0) {
                  Codebook *c = f->codebooks+r->classbook;
                  int q = codebook_decode_scalar(f,c);
                  if (c->sparse)
                     q = c->sorted_values[q];
                  if (q == EOP) goto done;
                  part_classdata[0][class_set] = r->classdata[q];
               }
               for (i=0; i < classwords && pcount < part_read; ++i, ++pcount) {
                  int z = r->begin + pcount*r->part_size;
                  int c = part_classdata[0][class_set][i];
                  int b = r->residue_books[c][pass];
                  if (b >= 0) {
                     Codebook *book = f->codebooks + b;
                     if (!codebook_decode_deinterleave_repeat(f, book, residue_buffers, ch, &c_inter, &p_inter, n, r->part_size))
                        goto done;
                  } else {
                     z += r->part_size;
                     c_inter = 0;
                     p_inter = z;
                  }
               }
               ++class_set;
            }
         } else {
            while (pcount < part_read) {
               int z = r->begin + pcount*r->part_size;
               int c_inter = z % ch, p_inter = z/ch;
               if (pass == 0) {
                  Codebook *c = f->codebooks+r->classbook;
                  int q = codebook_decode_scalar(f,c);
                  if (c->sparse)
                     q = c->sorted_values[q];
                  if (q == EOP)
                     goto done;
                  part_classdata[0][class_set] = r->classdata[q];
               }
               for (i=0; i < classwords && pcount < part_read; ++i, ++pcount) {
                  int z = r->begin + pcount*r->part_size;
                  int c = part_classdata[0][class_set][i];
                  int b = r->residue_books[c][pass];
                  if (b >= 0) {
                     Codebook *book = f->codebooks + b;
                     if (!codebook_decode_deinterleave_repeat(f, book, residue_buffers, ch, &c_inter, &p_inter, n, r->part_size))
                        goto done;
                  } else {
                     z += r->part_size;
                     c_inter = z % ch;
                     p_inter = z / ch;
                  }
               }
               ++class_set;
            }
         }
      }
      goto done;
   }

   for (pass=0; pass < 8; ++pass)
   {
      int pcount = 0, class_set=0;
      while (pcount < part_read) {
         if (pass == 0)
         {
            for (j=0; j < ch; ++j) {
               if (!do_not_decode[j]) {
                  Codebook *c = f->codebooks+r->classbook;
                  int temp = codebook_decode_scalar(f,c);
                  if (c->sparse)
                     temp = c->sorted_values[temp];
                  if (temp == EOP)
                     goto done;
                  part_classdata[j][class_set] = r->classdata[temp];
               }
            }
         }
         for (i=0; i < classwords && pcount < part_read; ++i, ++pcount) {
            for (j=0; j < ch; ++j) {
               if (!do_not_decode[j]) {
                  int c = part_classdata[j][class_set][i];
                  int b = r->residue_books[c][pass];
                  if (b >= 0) {
                     float *target = residue_buffers[j];
                     int offset = r->begin + pcount * r->part_size;
                     int n = r->part_size;
                     Codebook *book = f->codebooks + b;
                     if (!residue_decode(f, book, target, offset, n, rtype))
                        goto done;
                  }
               }
            }
         }
         ++class_set;
      }
   }
done:
   temp_alloc_restore(f,temp_alloc_point);
}

/* the following were split out into separate functions while optimizing;
 * they could be pushed back up but eh. __forceinline showed no change;
 * they're probably already being inlined. */
/* -----------------------------------------------------------------------
 * SIMD selection for the inverse MDCT
 *
 * Selected purely at compile time: on every configuration that defines
 * these macros the compiler is already free to emit the same
 * instructions in ordinary code, so no runtime dispatch is needed.
 * ----------------------------------------------------------------------- */
#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#include <immintrin.h>
#define RVORBIS_HAVE_SSE 1
#elif defined(__ARM_NEON) || defined(__aarch64__) || defined(__ARM_NEON__) || defined(_M_ARM64)
#include <arm_neon.h>
#define RVORBIS_HAVE_NEON 1
#endif

#ifndef RVORBIS_HAVE_SSE
#define RVORBIS_HAVE_SSE 0
#endif
#ifndef RVORBIS_HAVE_NEON
#define RVORBIS_HAVE_NEON 0
#endif
#if RVORBIS_HAVE_SSE
/* One radix butterfly over two adjacent complex pairs (four floats,
 * descending addresses: pair 0 at e[0]/e[-1], pair 1 at e[-2]/e[-3]).
 *
 *    k_re = e0[re] - e2[re]        e0[re] += e2[re]
 *    k_im = e0[im] - e2[im]        e0[im] += e2[im]
 *    e2[re] = k_re*A0 - k_im*A1
 *    e2[im] = k_im*A0 + k_re*A1
 *
 * ca carries (A0, A0) per pair, cb carries (A1, -A1) per pair with the
 * sign folded in, so the twiddle is a plain mul/mul/add with exactly
 * the same per-element IEEE operations (and results) as the scalar
 * code.  ds swaps re and im within each pair to line the products up.
 */
static INLINE void imdct_butterfly4_sse(float *e0, float *e2,
      __m128 ca, __m128 cb)
{
   __m128 v0 = _mm_loadu_ps(e0 - 3);
   __m128 v2 = _mm_loadu_ps(e2 - 3);
   __m128 d  = _mm_sub_ps(v0, v2);
   __m128 ds = _mm_shuffle_ps(d, d, _MM_SHUFFLE(2, 3, 0, 1));
   _mm_storeu_ps(e0 - 3, _mm_add_ps(v0, v2));
   _mm_storeu_ps(e2 - 3, _mm_add_ps(_mm_mul_ps(d, ca),
         _mm_mul_ps(ds, cb)));
}

#endif

#if RVORBIS_HAVE_NEON
/* NEON twin of the butterfly above; lane 3 is the first pair's re
 * slot, so the descending four floats at e - 3 map to lanes 0..3 in
 * ascending address order.  vrev64q swaps re and im within each pair.
 * The coefficient vectors carry the twiddle signs pre-folded (lanes 1
 * and 3 negated via sign-bit xor, which is exact), keeping the
 * per-element operations identical to the scalar code. */
static INLINE uint32x4_t imdct_twiddle_signmask_neon(void)
{
   uint32x4_t m = vdupq_n_u32(0);
   m = vsetq_lane_u32(0x80000000u, m, 1);
   m = vsetq_lane_u32(0x80000000u, m, 3);
   return m;
}

/* (mag0, mag0) in lanes 2..3 and (mag1, mag1) in lanes 0..1, with the
 * odd lanes sign-flipped: { -mag1, mag1, -mag0, mag0 } by ascending
 * lane.  Used for the A1-style coefficients of both pairs. */
static INLINE float32x4_t imdct_coeff_pair_neon(float mag0, float mag1,
      uint32x4_t signmask)
{
   float32x4_t c = vcombine_f32(vdup_n_f32(mag1), vdup_n_f32(mag0));
   return vreinterpretq_f32_u32(veorq_u32(vreinterpretq_u32_f32(c),
         signmask));
}

static INLINE void imdct_butterfly4_neon(float *e0, float *e2,
      float32x4_t ca, float32x4_t cb)
{
   float32x4_t v0 = vld1q_f32(e0 - 3);
   float32x4_t v2 = vld1q_f32(e2 - 3);
   float32x4_t d  = vsubq_f32(v0, v2);
   float32x4_t ds = vrev64q_f32(d);
   vst1q_f32(e0 - 3, vaddq_f32(v0, v2));
   vst1q_f32(e2 - 3, vaddq_f32(vmulq_f32(d, ca), vmulq_f32(ds, cb)));
}
#endif

static void imdct_step3_iter0_loop(int n, float *e, int i_off, int k_off, float *A)
{
   float *ee0 = e + i_off;
   float *ee2 = ee0 + k_off;
   int i;

   assert((n & 3) == 0);
   for (i=(n>>2); i > 0; --i)
   {
#if RVORBIS_HAVE_SSE
      imdct_butterfly4_sse(ee0, ee2,
            _mm_set_ps(A[0], A[0], A[8], A[8]),
            _mm_set_ps(-A[1], A[1], -A[9], A[9]));
      imdct_butterfly4_sse(ee0 - 4, ee2 - 4,
            _mm_set_ps(A[16], A[16], A[24], A[24]),
            _mm_set_ps(-A[17], A[17], -A[25], A[25]));
      A += 32;
#elif RVORBIS_HAVE_NEON
      uint32x4_t sm = imdct_twiddle_signmask_neon();
      imdct_butterfly4_neon(ee0, ee2,
            vcombine_f32(vdup_n_f32(A[8]), vdup_n_f32(A[0])),
            imdct_coeff_pair_neon(A[1], A[9], sm));
      imdct_butterfly4_neon(ee0 - 4, ee2 - 4,
            vcombine_f32(vdup_n_f32(A[24]), vdup_n_f32(A[16])),
            imdct_coeff_pair_neon(A[17], A[25], sm));
      A += 32;
#else
      float k00_20  = ee0[ 0] - ee2[ 0];
      float k01_21  = ee0[-1] - ee2[-1];
      ee0[ 0] += ee2[ 0];
      ee0[-1] += ee2[-1];
      ee2[ 0] = k00_20 * A[0] - k01_21 * A[1];
      ee2[-1] = k01_21 * A[0] + k00_20 * A[1];
      A += 8;

      k00_20  = ee0[-2] - ee2[-2];
      k01_21  = ee0[-3] - ee2[-3];
      ee0[-2] += ee2[-2];
      ee0[-3] += ee2[-3];
      ee2[-2] = k00_20 * A[0] - k01_21 * A[1];
      ee2[-3] = k01_21 * A[0] + k00_20 * A[1];
      A += 8;

      k00_20  = ee0[-4] - ee2[-4];
      k01_21  = ee0[-5] - ee2[-5];
      ee0[-4] += ee2[-4];
      ee0[-5] += ee2[-5];
      ee2[-4] = k00_20 * A[0] - k01_21 * A[1];
      ee2[-5] = k01_21 * A[0] + k00_20 * A[1];
      A += 8;

      k00_20  = ee0[-6] - ee2[-6];
      k01_21  = ee0[-7] - ee2[-7];
      ee0[-6] += ee2[-6];
      ee0[-7] += ee2[-7];
      ee2[-6] = k00_20 * A[0] - k01_21 * A[1];
      ee2[-7] = k01_21 * A[0] + k00_20 * A[1];
      A += 8;
#endif
      ee0 -= 8;
      ee2 -= 8;
   }
}

static void imdct_step3_inner_r_loop(int lim, float *e, int d0, int k_off, float *A, int k1)
{
   int i;
#if !RVORBIS_HAVE_SSE && !RVORBIS_HAVE_NEON
   float k00_20, k01_21;
#endif

   float *e0 = e + d0;
   float *e2 = e0 + k_off;

   for (i=lim >> 2; i > 0; --i) {
#if RVORBIS_HAVE_SSE
      imdct_butterfly4_sse(e0, e2,
            _mm_set_ps(A[0], A[0], A[k1], A[k1]),
            _mm_set_ps(-A[1], A[1], -A[k1 + 1], A[k1 + 1]));
      imdct_butterfly4_sse(e0 - 4, e2 - 4,
            _mm_set_ps(A[2*k1], A[2*k1], A[3*k1], A[3*k1]),
            _mm_set_ps(-A[2*k1 + 1], A[2*k1 + 1], -A[3*k1 + 1], A[3*k1 + 1]));
      A += 4*k1;
#elif RVORBIS_HAVE_NEON
      uint32x4_t sm = imdct_twiddle_signmask_neon();
      imdct_butterfly4_neon(e0, e2,
            vcombine_f32(vdup_n_f32(A[k1]), vdup_n_f32(A[0])),
            imdct_coeff_pair_neon(A[1], A[k1 + 1], sm));
      imdct_butterfly4_neon(e0 - 4, e2 - 4,
            vcombine_f32(vdup_n_f32(A[3*k1]), vdup_n_f32(A[2*k1])),
            imdct_coeff_pair_neon(A[2*k1 + 1], A[3*k1 + 1], sm));
      A += 4*k1;
#else
      k00_20 = e0[-0] - e2[-0];
      k01_21 = e0[-1] - e2[-1];
      e0[-0] += e2[-0];
      e0[-1] += e2[-1];
      e2[-0] = (k00_20)*A[0] - (k01_21) * A[1];
      e2[-1] = (k01_21)*A[0] + (k00_20) * A[1];

      A += k1;

      k00_20 = e0[-2] - e2[-2];
      k01_21 = e0[-3] - e2[-3];
      e0[-2] += e2[-2];
      e0[-3] += e2[-3];
      e2[-2] = (k00_20)*A[0] - (k01_21) * A[1];
      e2[-3] = (k01_21)*A[0] + (k00_20) * A[1];

      A += k1;

      k00_20 = e0[-4] - e2[-4];
      k01_21 = e0[-5] - e2[-5];
      e0[-4] += e2[-4];
      e0[-5] += e2[-5];
      e2[-4] = (k00_20)*A[0] - (k01_21) * A[1];
      e2[-5] = (k01_21)*A[0] + (k00_20) * A[1];

      A += k1;

      k00_20 = e0[-6] - e2[-6];
      k01_21 = e0[-7] - e2[-7];
      e0[-6] += e2[-6];
      e0[-7] += e2[-7];
      e2[-6] = (k00_20)*A[0] - (k01_21) * A[1];
      e2[-7] = (k01_21)*A[0] + (k00_20) * A[1];

      A += k1;
#endif
      e0 -= 8;
      e2 -= 8;
   }
}

static void imdct_step3_inner_s_loop(int n, float *e, int i_off, int k_off, float *A, int a_off, int k0)
{
   int i;
   float A0 = A[0];
   float A1 = A[0+1];
   float A2 = A[0+a_off];
   float A3 = A[0+a_off+1];
   float A4 = A[0+a_off*2+0];
   float A5 = A[0+a_off*2+1];
   float A6 = A[0+a_off*3+0];
   float A7 = A[0+a_off*3+1];

#if RVORBIS_HAVE_SSE
   /* The twiddles are loop-invariant here, so the coefficient vectors
    * are built once. */
   __m128 ca01 = _mm_set_ps(A0, A0, A2, A2);
   __m128 cb01 = _mm_set_ps(-A1, A1, -A3, A3);
   __m128 ca23 = _mm_set_ps(A4, A4, A6, A6);
   __m128 cb23 = _mm_set_ps(-A5, A5, -A7, A7);
#elif RVORBIS_HAVE_NEON
   uint32x4_t sm    = imdct_twiddle_signmask_neon();
   float32x4_t ca01 = vcombine_f32(vdup_n_f32(A2), vdup_n_f32(A0));
   float32x4_t cb01 = imdct_coeff_pair_neon(A1, A3, sm);
   float32x4_t ca23 = vcombine_f32(vdup_n_f32(A6), vdup_n_f32(A4));
   float32x4_t cb23 = imdct_coeff_pair_neon(A5, A7, sm);
#else
   float k00,k11;
#endif

   float *ee0 = e  +i_off;
   float *ee2 = ee0+k_off;

   for (i=n; i > 0; --i) {
#if RVORBIS_HAVE_SSE
      imdct_butterfly4_sse(ee0, ee2, ca01, cb01);
      imdct_butterfly4_sse(ee0 - 4, ee2 - 4, ca23, cb23);
#elif RVORBIS_HAVE_NEON
      imdct_butterfly4_neon(ee0, ee2, ca01, cb01);
      imdct_butterfly4_neon(ee0 - 4, ee2 - 4, ca23, cb23);
#else
      k00     = ee0[ 0] - ee2[ 0];
      k11     = ee0[-1] - ee2[-1];
      ee0[ 0] =  ee0[ 0] + ee2[ 0];
      ee0[-1] =  ee0[-1] + ee2[-1];
      ee2[ 0] = (k00) * A0 - (k11) * A1;
      ee2[-1] = (k11) * A0 + (k00) * A1;

      k00     = ee0[-2] - ee2[-2];
      k11     = ee0[-3] - ee2[-3];
      ee0[-2] =  ee0[-2] + ee2[-2];
      ee0[-3] =  ee0[-3] + ee2[-3];
      ee2[-2] = (k00) * A2 - (k11) * A3;
      ee2[-3] = (k11) * A2 + (k00) * A3;

      k00     = ee0[-4] - ee2[-4];
      k11     = ee0[-5] - ee2[-5];
      ee0[-4] =  ee0[-4] + ee2[-4];
      ee0[-5] =  ee0[-5] + ee2[-5];
      ee2[-4] = (k00) * A4 - (k11) * A5;
      ee2[-5] = (k11) * A4 + (k00) * A5;

      k00     = ee0[-6] - ee2[-6];
      k11     = ee0[-7] - ee2[-7];
      ee0[-6] =  ee0[-6] + ee2[-6];
      ee0[-7] =  ee0[-7] + ee2[-7];
      ee2[-6] = (k00) * A6 - (k11) * A7;
      ee2[-7] = (k11) * A6 + (k00) * A7;
#endif
      ee0 -= k0;
      ee2 -= k0;
   }
}

static INLINE void iter_54(float *z)
{
   float k00,k11,k22,k33;
   float y0,y1,y2,y3;

   k00  = z[ 0] - z[-4];
   y0   = z[ 0] + z[-4];
   y2   = z[-2] + z[-6];
   k22  = z[-2] - z[-6];

   z[-0] = y0 + y2;      /* z0 + z4 + z2 + z6 */
   z[-2] = y0 - y2;      /* z0 + z4 - z2 - z6 */

   /* done with y0,y2 */

   k33  = z[-3] - z[-7];

   z[-4] = k00 + k33;    /* z0 - z4 + z3 - z7 */
   z[-6] = k00 - k33;    /* z0 - z4 - z3 + z7 */

   /* done with k33 */

   k11  = z[-1] - z[-5];
   y1   = z[-1] + z[-5];
   y3   = z[-3] + z[-7];

   z[-1] = y1 + y3;      /* z1 + z5 + z3 + z7 */
   z[-3] = y1 - y3;      /* z1 + z5 - z3 - z7 */
   z[-5] = k11 - k22;    /* z1 - z5 + z2 - z6 */
   z[-7] = k11 + k22;    /* z1 - z5 - z2 + z6 */
}

static void imdct_step3_inner_s_loop_ld654(int n, float *e, int i_off, float *A, int base_n)
{
   int a_off = base_n >> 3;
   float A2 = A[0+a_off];
   float *z = e + i_off;
   float *base = z - 16 * n;

   while (z > base) {
      float k00,k11;

      k00   = z[-0] - z[-8];
      k11   = z[-1] - z[-9];
      z[-0] = z[-0] + z[-8];
      z[-1] = z[-1] + z[-9];
      z[-8] =  k00;
      z[-9] =  k11 ;

      k00    = z[ -2] - z[-10];
      k11    = z[ -3] - z[-11];
      z[ -2] = z[ -2] + z[-10];
      z[ -3] = z[ -3] + z[-11];
      z[-10] = (k00+k11) * A2;
      z[-11] = (k11-k00) * A2;

      k00    = z[-12] - z[ -4];  /* reverse to avoid a unary negation */
      k11    = z[ -5] - z[-13];
      z[ -4] = z[ -4] + z[-12];
      z[ -5] = z[ -5] + z[-13];
      z[-12] = k11;
      z[-13] = k00;

      k00    = z[-14] - z[ -6];  /* reverse to avoid a unary negation */
      k11    = z[ -7] - z[-15];
      z[ -6] = z[ -6] + z[-14];
      z[ -7] = z[ -7] + z[-15];
      z[-14] = (k00+k11) * A2;
      z[-15] = (k00-k11) * A2;

      iter_54(z);
      iter_54(z-8);
      z -= 16;
   }
}

/* float -> Q28, branchless on soft-float targets: sign and saturation
 * come from the bit pattern, so the float work is one multiply, one
 * add and the conversion.  Saturates at |x| >= 8.0 (measured decode
 * maxima stay below 1.7). */
static INLINE int32_t rvq_float_to_q(float x)
{
   union { float f; uint32_t u; } b, half;
   float y;
   b.f = x;
   if ((b.u & 0x7F800000u) >= (130u << 23))
      return (b.u >> 31) ? (int32_t)-0x7FFFFFFF - 1 : (int32_t)0x7FFFFFFF;
   y      = x * (float)(1 << RVQ_QBITS);
   half.f = y;
   half.u = 0x3F000000u | (half.u & 0x80000000u);
   return (int32_t)(y + half.f);
}

/* -----------------------------------------------------------------------
 * Fixed-point inverse MDCT: the scalar float kernels below, transcribed
 * to Q28 samples and Q27 twiddles.  Each product is one 32x32->64
 * multiply and a round-to-nearest shift.
 * ----------------------------------------------------------------------- */
static INLINE void iter_54_q(int32_t *z)
{
   int32_t k00,k11,k22,k33;
   int32_t y0,y1,y2,y3;

   k00  = z[ 0] - z[-4];
   y0   = z[ 0] + z[-4];
   y2   = z[-2] + z[-6];
   k22  = z[-2] - z[-6];

   z[-0] = y0 + y2;      /* z0 + z4 + z2 + z6 */
   z[-2] = y0 - y2;      /* z0 + z4 - z2 - z6 */

   /* done with y0,y2 */

   k33  = z[-3] - z[-7];

   z[-4] = k00 + k33;    /* z0 - z4 + z3 - z7 */
   z[-6] = k00 - k33;    /* z0 - z4 - z3 + z7 */

   /* done with k33 */

   k11  = z[-1] - z[-5];
   y1   = z[-1] + z[-5];
   y3   = z[-3] + z[-7];

   z[-1] = y1 + y3;      /* z1 + z5 + z3 + z7 */
   z[-3] = y1 - y3;      /* z1 + z5 - z3 - z7 */
   z[-5] = k11 - k22;    /* z1 - z5 + z2 - z6 */
   z[-7] = k11 + k22;    /* z1 - z5 - z2 + z6 */
}

static void imdct_step3_iter0_loop_q(int n, int32_t *e, int i_off, int k_off, int32_t *A)
{
   int32_t *ee0 = e + i_off;
   int32_t *ee2 = ee0 + k_off;
   int i;

   assert((n & 3) == 0);
   for (i=(n>>2); i > 0; --i)
   {
      int32_t k00_20  = ee0[ 0] - ee2[ 0];
      int32_t k01_21  = ee0[-1] - ee2[-1];
      ee0[ 0] += ee2[ 0];
      ee0[-1] += ee2[-1];
      ee2[ 0] = RVQ_MULQ(k00_20, A[0]) - RVQ_MULQ(k01_21, A[1]);
      ee2[-1] = RVQ_MULQ(k01_21, A[0]) + RVQ_MULQ(k00_20, A[1]);
      A += 8;

      k00_20  = ee0[-2] - ee2[-2];
      k01_21  = ee0[-3] - ee2[-3];
      ee0[-2] += ee2[-2];
      ee0[-3] += ee2[-3];
      ee2[-2] = RVQ_MULQ(k00_20, A[0]) - RVQ_MULQ(k01_21, A[1]);
      ee2[-3] = RVQ_MULQ(k01_21, A[0]) + RVQ_MULQ(k00_20, A[1]);
      A += 8;

      k00_20  = ee0[-4] - ee2[-4];
      k01_21  = ee0[-5] - ee2[-5];
      ee0[-4] += ee2[-4];
      ee0[-5] += ee2[-5];
      ee2[-4] = RVQ_MULQ(k00_20, A[0]) - RVQ_MULQ(k01_21, A[1]);
      ee2[-5] = RVQ_MULQ(k01_21, A[0]) + RVQ_MULQ(k00_20, A[1]);
      A += 8;

      k00_20  = ee0[-6] - ee2[-6];
      k01_21  = ee0[-7] - ee2[-7];
      ee0[-6] += ee2[-6];
      ee0[-7] += ee2[-7];
      ee2[-6] = RVQ_MULQ(k00_20, A[0]) - RVQ_MULQ(k01_21, A[1]);
      ee2[-7] = RVQ_MULQ(k01_21, A[0]) + RVQ_MULQ(k00_20, A[1]);
      A += 8;
      ee0 -= 8;
      ee2 -= 8;
   }
}

static void imdct_step3_inner_r_loop_q(int lim, int32_t *e, int d0, int k_off, int32_t *A, int k1)
{
   int i;
   int32_t k00_20, k01_21;

   int32_t *e0 = e + d0;
   int32_t *e2 = e0 + k_off;

   for (i=lim >> 2; i > 0; --i) {
      k00_20 = e0[-0] - e2[-0];
      k01_21 = e0[-1] - e2[-1];
      e0[-0] += e2[-0];
      e0[-1] += e2[-1];
      e2[-0] = RVQ_MULQ((k00_20), A[0]) - RVQ_MULQ((k01_21), A[1]);
      e2[-1] = RVQ_MULQ((k01_21), A[0]) + RVQ_MULQ((k00_20), A[1]);

      A += k1;

      k00_20 = e0[-2] - e2[-2];
      k01_21 = e0[-3] - e2[-3];
      e0[-2] += e2[-2];
      e0[-3] += e2[-3];
      e2[-2] = RVQ_MULQ((k00_20), A[0]) - RVQ_MULQ((k01_21), A[1]);
      e2[-3] = RVQ_MULQ((k01_21), A[0]) + RVQ_MULQ((k00_20), A[1]);

      A += k1;

      k00_20 = e0[-4] - e2[-4];
      k01_21 = e0[-5] - e2[-5];
      e0[-4] += e2[-4];
      e0[-5] += e2[-5];
      e2[-4] = RVQ_MULQ((k00_20), A[0]) - RVQ_MULQ((k01_21), A[1]);
      e2[-5] = RVQ_MULQ((k01_21), A[0]) + RVQ_MULQ((k00_20), A[1]);

      A += k1;

      k00_20 = e0[-6] - e2[-6];
      k01_21 = e0[-7] - e2[-7];
      e0[-6] += e2[-6];
      e0[-7] += e2[-7];
      e2[-6] = RVQ_MULQ((k00_20), A[0]) - RVQ_MULQ((k01_21), A[1]);
      e2[-7] = RVQ_MULQ((k01_21), A[0]) + RVQ_MULQ((k00_20), A[1]);

      A += k1;
      e0 -= 8;
      e2 -= 8;
   }
}

static void imdct_step3_inner_s_loop_q(int n, int32_t *e, int i_off, int k_off, int32_t *A, int a_off, int k0)
{
   int i;
   int32_t A0 = A[0];
   int32_t A1 = A[0+1];
   int32_t A2 = A[0+a_off];
   int32_t A3 = A[0+a_off+1];
   int32_t A4 = A[0+a_off*2+0];
   int32_t A5 = A[0+a_off*2+1];
   int32_t A6 = A[0+a_off*3+0];
   int32_t A7 = A[0+a_off*3+1];

   int32_t k00,k11;

   int32_t *ee0 = e  +i_off;
   int32_t *ee2 = ee0+k_off;

   for (i=n; i > 0; --i) {
      k00     = ee0[ 0] - ee2[ 0];
      k11     = ee0[-1] - ee2[-1];
      ee0[ 0] =  ee0[ 0] + ee2[ 0];
      ee0[-1] =  ee0[-1] + ee2[-1];
      ee2[ 0] = RVQ_MULQ((k00), A0) - RVQ_MULQ((k11), A1);
      ee2[-1] = RVQ_MULQ((k11), A0) + RVQ_MULQ((k00), A1);

      k00     = ee0[-2] - ee2[-2];
      k11     = ee0[-3] - ee2[-3];
      ee0[-2] =  ee0[-2] + ee2[-2];
      ee0[-3] =  ee0[-3] + ee2[-3];
      ee2[-2] = RVQ_MULQ((k00), A2) - RVQ_MULQ((k11), A3);
      ee2[-3] = RVQ_MULQ((k11), A2) + RVQ_MULQ((k00), A3);

      k00     = ee0[-4] - ee2[-4];
      k11     = ee0[-5] - ee2[-5];
      ee0[-4] =  ee0[-4] + ee2[-4];
      ee0[-5] =  ee0[-5] + ee2[-5];
      ee2[-4] = RVQ_MULQ((k00), A4) - RVQ_MULQ((k11), A5);
      ee2[-5] = RVQ_MULQ((k11), A4) + RVQ_MULQ((k00), A5);

      k00     = ee0[-6] - ee2[-6];
      k11     = ee0[-7] - ee2[-7];
      ee0[-6] =  ee0[-6] + ee2[-6];
      ee0[-7] =  ee0[-7] + ee2[-7];
      ee2[-6] = RVQ_MULQ((k00), A6) - RVQ_MULQ((k11), A7);
      ee2[-7] = RVQ_MULQ((k11), A6) + RVQ_MULQ((k00), A7);
      ee0 -= k0;
      ee2 -= k0;
   }
}

static void imdct_step3_inner_s_loop_ld654_q(int n, int32_t *e, int i_off, int32_t *A, int base_n)
{
   int a_off = base_n >> 3;
   int32_t A2 = A[0+a_off];
   int32_t *z = e + i_off;
   int32_t *base = z - 16 * n;

   while (z > base) {
      int32_t k00,k11;

      k00   = z[-0] - z[-8];
      k11   = z[-1] - z[-9];
      z[-0] = z[-0] + z[-8];
      z[-1] = z[-1] + z[-9];
      z[-8] =  k00;
      z[-9] =  k11 ;

      k00    = z[ -2] - z[-10];
      k11    = z[ -3] - z[-11];
      z[ -2] = z[ -2] + z[-10];
      z[ -3] = z[ -3] + z[-11];
      z[-10] = RVQ_MULQ((k00+k11), A2);
      z[-11] = RVQ_MULQ((k11-k00), A2);

      k00    = z[-12] - z[ -4];  /* reverse to avoid a unary negation */
      k11    = z[ -5] - z[-13];
      z[ -4] = z[ -4] + z[-12];
      z[ -5] = z[ -5] + z[-13];
      z[-12] = k11;
      z[-13] = k00;

      k00    = z[-14] - z[ -6];  /* reverse to avoid a unary negation */
      k11    = z[ -7] - z[-15];
      z[ -6] = z[ -6] + z[-14];
      z[ -7] = z[ -7] + z[-15];
      z[-14] = RVQ_MULQ((k00+k11), A2);
      z[-15] = RVQ_MULQ((k00-k11), A2);

      iter_54_q(z);
      iter_54_q(z-8);
      z -= 16;
   }
}

static void inverse_mdct_q(int32_t *buffer, int n, vorb *f, int blocktype)
{
   int n2 = n >> 1, n4 = n >> 2, n8 = n >> 3, l;
   int ld;
   /* @OPTIMIZE: reduce register pressure by using fewer variables? */
   int save_point = temp_alloc_save(f);
   int32_t *buf2 = (int32_t *)temp_alloc(f, n2 * sizeof(*buf2));
   int32_t *u=NULL,*v=NULL;
   /* twiddle factors */
   int32_t *A = f->A_q[blocktype];

   /* IMDCT algorithm from "The use of multirate filter banks for coding of high quality digital audio"
    * See notes about bugs in that paper in less-optimal implementation 'inverse_mdct_old' after this function.

    * kernel from paper


    * merged:
    *   copy and reflect spectral data
    *   step 0

    * note that it turns out that the items added together during
    * this step are, in fact, being added to themselves (as reflected
    * by step 0). inexplicable inefficiency! this became obvious
    * once I combined the passes.

    * so there's a missing 'times 2' here (for adding X to itself).
    * this propogates through linearly to the end, where the numbers
    * are 1/2 too small, and need to be compensated for.
    */

   {
      int32_t *d,*e, *AA, *e_stop;
      d = &buf2[n2-2];
      AA = A;
      e = &buffer[0];
      e_stop = &buffer[n2];
      while (e != e_stop) {
         d[1] = RVQ_MULQ(e[0], AA[0]) - RVQ_MULQ(e[2], AA[1]);
         d[0] = RVQ_MULQ(e[0], AA[1]) + RVQ_MULQ(e[2], AA[0]);
         d -= 2;
         AA += 2;
         e += 4;
      }

      e = &buffer[n2-3];
      while (d >= buf2) {
         d[1] = RVQ_MULQ(-e[2], AA[0]) - RVQ_MULQ(-e[0], AA[1]);
         d[0] = RVQ_MULQ(-e[2], AA[1]) + RVQ_MULQ(-e[0], AA[0]);
         d -= 2;
         AA += 2;
         e -= 4;
      }
   }

   /* now we use symbolic names for these, so that we can
    * possibly swap their meaning as we change which operations
    * are in place */

   u = buffer;
   v = buf2;

   /* step 2    (paper output is w, now u)
    * this could be in place, but the data ends up in the wrong
    * place... _somebody_'s got to swap it, so this is nominated */
   {
      int32_t *AA = &A[n2-8];
      int32_t *d0,*d1, *e0, *e1;

      e0 = &v[n4];
      e1 = &v[0];

      d0 = &u[n4];
      d1 = &u[0];

      while (AA >= A) {
         int32_t v40_20, v41_21;

         v41_21 = e0[1] - e1[1];
         v40_20 = e0[0] - e1[0];
         d0[1]  = e0[1] + e1[1];
         d0[0]  = e0[0] + e1[0];
         d1[1]  = RVQ_MULQ(v41_21, AA[4]) - RVQ_MULQ(v40_20, AA[5]);
         d1[0]  = RVQ_MULQ(v40_20, AA[4]) + RVQ_MULQ(v41_21, AA[5]);

         v41_21 = e0[3] - e1[3];
         v40_20 = e0[2] - e1[2];
         d0[3]  = e0[3] + e1[3];
         d0[2]  = e0[2] + e1[2];
         d1[3]  = RVQ_MULQ(v41_21, AA[0]) - RVQ_MULQ(v40_20, AA[1]);
         d1[2]  = RVQ_MULQ(v40_20, AA[0]) + RVQ_MULQ(v41_21, AA[1]);
         AA -= 8;

         d0 += 4;
         d1 += 4;
         e0 += 4;
         e1 += 4;
      }
   }

   /* step 3 */
   ld = ilog(n) - 1; /* ilog is off-by-one from normal definitions */

   /* optimized step 3:

    * the original step3 loop can be nested r inside s or s inside r;
    * it's written originally as s inside r, but this is dumb when r
    * iterates many times, and s few. So I have two copies of it and
    * switch between them halfway.

    * this is iteration 0 of step 3 */
   imdct_step3_iter0_loop_q(n >> 4, u, n2-1-n4*0, -(n >> 3), A);
   imdct_step3_iter0_loop_q(n >> 4, u, n2-1-n4*1, -(n >> 3), A);

   /* this is iteration 1 of step 3 */
   imdct_step3_inner_r_loop_q(n >> 5, u, n2-1 - n8*0, -(n >> 4), A, 16);
   imdct_step3_inner_r_loop_q(n >> 5, u, n2-1 - n8*1, -(n >> 4), A, 16);
   imdct_step3_inner_r_loop_q(n >> 5, u, n2-1 - n8*2, -(n >> 4), A, 16);
   imdct_step3_inner_r_loop_q(n >> 5, u, n2-1 - n8*3, -(n >> 4), A, 16);

   l=2;
   for (; l < (ld-3)>>1; ++l) {
      int k0 = n >> (l+2), k0_2 = k0>>1;
      int lim = 1 << (l+1);
      int i;
      for (i=0; i < lim; ++i)
         imdct_step3_inner_r_loop_q(n >> (l+4), u, n2-1 - k0*i, -k0_2, A, 1 << (l+3));
   }

   for (; l < ld-6; ++l) {
      int k0 = n >> (l+2), k1 = 1 << (l+3), k0_2 = k0>>1;
      int rlim = n >> (l+6), r;
      int lim = 1 << (l+1);
      int i_off;
      int32_t *A0 = A;
      i_off = n2-1;
      for (r=rlim; r > 0; --r) {
         imdct_step3_inner_s_loop_q(lim, u, i_off, -k0_2, A0, k1, k0);
         A0 += k1*4;
         i_off -= 8;
      }
   }

   /* iterations with count:
    *   ld-6,-5,-4 all interleaved together
    *       the big win comes from getting rid of needless flops
    *         due to the constants on pass 5 & 4 being all 1 and 0;
    *       combining them to be simultaneous to improve cache made little difference
    */
   imdct_step3_inner_s_loop_ld654_q(n >> 5, u, n2-1, A, n);

   /* output is u

    * step 4, 5, and 6
    * cannot be in-place because of step 5 */
   {
      uint16_t *bitrev = f->bit_reverse[blocktype];
      /* weirdly, I'd have thought reading sequentially and writing
       * erratically would have been better than vice-versa, but in
       * fact that's not what my testing showed. (That is, with
       * j = bitreverse(i), do you read i and write j, or read j and write i.) */

      int32_t *d0 = &v[n4-4];
      int32_t *d1 = &v[n2-4];
      while (d0 >= v) {
         int k4;

         k4 = bitrev[0];
         d1[3] = u[k4+0];
         d1[2] = u[k4+1];
         d0[3] = u[k4+2];
         d0[2] = u[k4+3];

         k4 = bitrev[1];
         d1[1] = u[k4+0];
         d1[0] = u[k4+1];
         d0[1] = u[k4+2];
         d0[0] = u[k4+3];

         d0 -= 4;
         d1 -= 4;
         bitrev += 2;
      }
   }
   /* (paper output is u, now v) */


   /* data must be in buf2 */
   assert(v == buf2);

   /* step 7   (paper output is v, now v)
    * this is now in place */
   {
      int32_t *C = f->C_q[blocktype];
      int32_t *d, *e;

      d = v;
      e = v + n2 - 4;

      while (d < e) {
         int32_t a02,a11,b0,b1,b2,b3;

         a02 = d[0] - e[2];
         a11 = d[1] + e[3];

         b0 = RVQ_MULQ(a02, C[1]) + RVQ_MULQ(a11, C[0]);
         b1 = RVQ_MULQ(a11, C[1]) - RVQ_MULQ(a02, C[0]);

         b2 = d[0] + e[ 2];
         b3 = d[1] - e[ 3];

         d[0] = b2 + b0;
         d[1] = b3 + b1;
         e[2] = b2 - b0;
         e[3] = b1 - b3;

         a02 = d[2] - e[0];
         a11 = d[3] + e[1];

         b0 = RVQ_MULQ(a02, C[3]) + RVQ_MULQ(a11, C[2]);
         b1 = RVQ_MULQ(a11, C[3]) - RVQ_MULQ(a02, C[2]);

         b2 = d[2] + e[ 0];
         b3 = d[3] - e[ 1];

         d[2] = b2 + b0;
         d[3] = b3 + b1;
         e[0] = b2 - b0;
         e[1] = b1 - b3;

         C += 4;
         d += 4;
         e -= 4;
      }
   }

   /* data must be in buf2


    * step 8+decode   (paper output is X, now buffer)
    * this generates pairs of data a la 8 and pushes them directly through
    * the decode kernel (pushing rather than pulling) to avoid having
    * to make another pass later

    * this cannot POSSIBLY be in place, so we refer to the buffers directly
    */

   {
      int32_t *d0,*d1,*d2,*d3;

      int32_t *B = f->B_q[blocktype] + n2 - 8;
      int32_t *e = buf2 + n2 - 8;
      d0 = &buffer[0];
      d1 = &buffer[n2-4];
      d2 = &buffer[n2];
      d3 = &buffer[n-4];
      while (e >= v) {
         int32_t p0,p1,p2,p3;

         p3 =  RVQ_MULQ(e[6], B[7]) - RVQ_MULQ(e[7], B[6]);
         p2 = -RVQ_MULQ(e[6], B[6]) - RVQ_MULQ(e[7], B[7]);

         d0[0] =   p3;
         d1[3] = - p3;
         d2[0] =   p2;
         d3[3] =   p2;

         p1 =  RVQ_MULQ(e[4], B[5]) - RVQ_MULQ(e[5], B[4]);
         p0 = -RVQ_MULQ(e[4], B[4]) - RVQ_MULQ(e[5], B[5]);

         d0[1] =   p1;
         d1[2] = - p1;
         d2[1] =   p0;
         d3[2] =   p0;

         p3 =  RVQ_MULQ(e[2], B[3]) - RVQ_MULQ(e[3], B[2]);
         p2 = -RVQ_MULQ(e[2], B[2]) - RVQ_MULQ(e[3], B[3]);

         d0[2] =   p3;
         d1[1] = - p3;
         d2[2] =   p2;
         d3[1] =   p2;

         p1 =  RVQ_MULQ(e[0], B[1]) - RVQ_MULQ(e[1], B[0]);
         p0 = -RVQ_MULQ(e[0], B[0]) - RVQ_MULQ(e[1], B[1]);

         d0[3] =   p1;
         d1[0] = - p1;
         d2[3] =   p0;
         d3[0] =   p0;

         B -= 8;
         e -= 8;
         d0 += 4;
         d2 += 4;
         d1 -= 4;
         d3 -= 4;
      }
   }

   temp_alloc_restore(f,save_point);
}
static void inverse_mdct(float *buffer, int n, vorb *f, int blocktype)
{
   int n2 = n >> 1, n4 = n >> 2, n8 = n >> 3, l;
   int ld;
   /* @OPTIMIZE: reduce register pressure by using fewer variables? */
   int save_point = temp_alloc_save(f);
   float *buf2 = (float *)temp_alloc(f, n2 * sizeof(*buf2));
   float *u=NULL,*v=NULL;
   /* twiddle factors */
   float *A = f->A[blocktype];

   /* IMDCT algorithm from "The use of multirate filter banks for coding of high quality digital audio"
    * See notes about bugs in that paper in less-optimal implementation 'inverse_mdct_old' after this function.

    * kernel from paper


    * merged:
    *   copy and reflect spectral data
    *   step 0

    * note that it turns out that the items added together during
    * this step are, in fact, being added to themselves (as reflected
    * by step 0). inexplicable inefficiency! this became obvious
    * once I combined the passes.

    * so there's a missing 'times 2' here (for adding X to itself).
    * this propogates through linearly to the end, where the numbers
    * are 1/2 too small, and need to be compensated for.
    */

   {
      float *d,*e, *AA, *e_stop;
      d = &buf2[n2-2];
      AA = A;
      e = &buffer[0];
      e_stop = &buffer[n2];
#if RVORBIS_HAVE_SSE
      /* Two output pairs per step.  e advances by 4 per pair, so the
       * four needed samples (e[0], e[2], e[4], e[6]) are the even lanes
       * of two loads; d walks down by 2 per pair, so the two output
       * pairs land in one descending 4-float store at d - 2 holding
       * { pair1.d0, pair1.d1, pair0.d0, pair0.d1 }.  Both loops in
       * this block run n/8 scalar iterations, which is even for every
       * valid Vorbis block size (64..8192), so fusing two iterations
       * per pass never leaves a remainder. */
      while (e != e_stop) {
         __m128 lo  = _mm_loadu_ps(e);          /* e[0..3]  */
         __m128 hi  = _mm_loadu_ps(e + 4);      /* e[4..7]  */
         __m128 g   = _mm_shuffle_ps(hi, lo, _MM_SHUFFLE(2, 0, 2, 0));
         /* g = { e[4], e[6], e[0], e[2] }; both lanes of an output pair
          * use e[0] as one factor and e[2] as the other, so broadcast
          * the even and odd gather lanes within each pair. */
         __m128 ev  = _mm_shuffle_ps(g, g, _MM_SHUFFLE(2, 2, 0, 0));
         /* ev  = { e[4], e[4], e[0], e[0] } */
         __m128 evs = _mm_shuffle_ps(g, g, _MM_SHUFFLE(3, 3, 1, 1));
         /* evs = { e[6], e[6], e[2], e[2] } */
         __m128 ca  = _mm_set_ps(AA[0], AA[1], AA[2], AA[3]);
         __m128 cb  = _mm_set_ps(-AA[1], AA[0], -AA[3], AA[2]);
         /* lane3 = d[1]  = e[0]*AA[0] - e[2]*AA[1]
          * lane2 = d[0]  = e[0]*AA[1] + e[2]*AA[0]
          * lane1 = d[-1] = e[4]*AA[2] - e[6]*AA[3]
          * lane0 = d[-2] = e[4]*AA[3] + e[6]*AA[2] */
         _mm_storeu_ps(d - 2, _mm_add_ps(_mm_mul_ps(ev, ca),
               _mm_mul_ps(evs, cb)));
         d -= 4;
         AA += 4;
         e += 8;
      }
#elif RVORBIS_HAVE_NEON
      /* vld2 deinterleaves, so the even-address samples e[0], e[2],
       * e[4], e[6] arrive as one register.  Output lanes and operand
       * order match the scalar code exactly (see the SSE variant for
       * the layout notes; both loops run an even number of scalar
       * iterations, so no remainder handling is needed). */
      while (e != e_stop) {
         float32x4x2_t de = vld2q_f32(e);       /* de.val[0] = { e[0], e[2], e[4], e[6] } */
         float32x2_t glo  = vget_low_f32(de.val[0]);   /* { e[0], e[2] } */
         float32x2_t ghi  = vget_high_f32(de.val[0]);  /* { e[4], e[6] } */
         float32x4_t ev   = vcombine_f32(vdup_lane_f32(ghi, 0),
               vdup_lane_f32(glo, 0));          /* { e[4], e[4], e[0], e[0] } */
         float32x4_t evs  = vcombine_f32(vdup_lane_f32(ghi, 1),
               vdup_lane_f32(glo, 1));          /* { e[6], e[6], e[2], e[2] } */
         float32x4_t aa   = vld1q_f32(AA);      /* { AA[0..3] } */
         float32x4_t ca   = vcombine_f32(
               vrev64_f32(vget_high_f32(aa)),   /* { AA[3], AA[2] } */
               vrev64_f32(vget_low_f32(aa)));   /* { AA[1], AA[0] } */
         float32x4_t cb   = vreinterpretq_f32_u32(veorq_u32(
               vreinterpretq_u32_f32(vextq_f32(aa, aa, 2)),
               imdct_twiddle_signmask_neon()));
         /* ca = { AA[3], AA[2], AA[1], AA[0] }, cb = { AA[2], -AA[3], AA[0], -AA[1] } */
         vst1q_f32(d - 2, vaddq_f32(vmulq_f32(ev, ca), vmulq_f32(evs, cb)));
         d -= 4;
         AA += 4;
         e += 8;
      }
#else
      while (e != e_stop) {
         d[1] = (e[0] * AA[0] - e[2]*AA[1]);
         d[0] = (e[0] * AA[1] + e[2]*AA[0]);
         d -= 2;
         AA += 2;
         e += 4;
      }
#endif

      e = &buffer[n2-3];
#if RVORBIS_HAVE_SSE
      while (d >= buf2) {
         __m128 lo  = _mm_loadu_ps(e - 5);      /* e[-5..-2] */
         __m128 hi  = _mm_loadu_ps(e - 1);      /* e[-1..2]  */
         __m128 g   = _mm_shuffle_ps(lo, hi, _MM_SHUFFLE(3, 1, 3, 1));
         /* g = { e[-4], e[-2], e[0], e[2] } */
         __m128 ev  = _mm_shuffle_ps(g, g, _MM_SHUFFLE(3, 3, 1, 1));
         /* ev  = { e[-2], e[-2], e[2], e[2] } */
         __m128 evs = _mm_shuffle_ps(g, g, _MM_SHUFFLE(2, 2, 0, 0));
         /* evs = { e[-4], e[-4], e[0], e[0] } */
         __m128 ca  = _mm_set_ps(-AA[0], -AA[1], -AA[2], -AA[3]);
         __m128 cb  = _mm_set_ps(AA[1], -AA[0], AA[3], -AA[2]);
         /* lane3 = d[1]  = -e[2]*AA[0] + e[0]*AA[1]
          * lane2 = d[0]  = -e[2]*AA[1] - e[0]*AA[0]
          * lane1 = d[-1] = -e[-2]*AA[2] + e[-4]*AA[3]
          * lane0 = d[-2] = -e[-2]*AA[3] - e[-4]*AA[2] */
         _mm_storeu_ps(d - 2, _mm_add_ps(_mm_mul_ps(ev, ca),
               _mm_mul_ps(evs, cb)));
         d -= 4;
         AA += 4;
         e -= 8;
      }
#elif RVORBIS_HAVE_NEON
      while (d >= buf2) {
         float32x4x2_t de = vld2q_f32(e - 5);   /* de.val[1] = { e[-4], e[-2], e[0], e[2] } */
         float32x2_t glo  = vget_low_f32(de.val[1]);   /* { e[-4], e[-2] } */
         float32x2_t ghi  = vget_high_f32(de.val[1]);  /* { e[0], e[2] } */
         float32x4_t ev   = vcombine_f32(vdup_lane_f32(glo, 1),
               vdup_lane_f32(ghi, 1));          /* { e[-2], e[-2], e[2], e[2] } */
         float32x4_t evs  = vcombine_f32(vdup_lane_f32(glo, 0),
               vdup_lane_f32(ghi, 0));          /* { e[-4], e[-4], e[0], e[0] } */
         float32x4_t aa   = vld1q_f32(AA);
         float32x4_t ca   = vnegq_f32(vcombine_f32(
               vrev64_f32(vget_high_f32(aa)),
               vrev64_f32(vget_low_f32(aa)))); /* { -AA[3], -AA[2], -AA[1], -AA[0] } */
         float32x4_t cb   = vreinterpretq_f32_u32(veorq_u32(
               vreinterpretq_u32_f32(vextq_f32(aa, aa, 2)),
               vsetq_lane_u32(0x80000000u,
                     vsetq_lane_u32(0x80000000u, vdupq_n_u32(0), 0), 2)));
         /* cb = { -AA[2], AA[3], -AA[0], AA[1] } */
         vst1q_f32(d - 2, vaddq_f32(vmulq_f32(ev, ca), vmulq_f32(evs, cb)));
         d -= 4;
         AA += 4;
         e -= 8;
      }
#else
      while (d >= buf2) {
         d[1] = (-e[2] * AA[0] - -e[0]*AA[1]);
         d[0] = (-e[2] * AA[1] + -e[0]*AA[0]);
         d -= 2;
         AA += 2;
         e -= 4;
      }
#endif
   }

   /* now we use symbolic names for these, so that we can
    * possibly swap their meaning as we change which operations
    * are in place */

   u = buffer;
   v = buf2;

   /* step 2    (paper output is w, now u)
    * this could be in place, but the data ends up in the wrong
    * place... _somebody_'s got to swap it, so this is nominated */
   {
      float *AA = &A[n2-8];
      float *d0,*d1, *e0, *e1;

      e0 = &v[n4];
      e1 = &v[0];

      d0 = &u[n4];
      d1 = &u[0];

      while (AA >= A) {
#if RVORBIS_HAVE_SSE
         /* Two complex pairs per iteration, ascending and contiguous.
          * Same mul/mul/add twiddle as the butterfly helpers: the odd
          * (imaginary) lanes take the minus form, so the swapped-pair
          * vector carries pre-negated coefficients in those lanes. */
         __m128 v0 = _mm_loadu_ps(e0);
         __m128 v1 = _mm_loadu_ps(e1);
         __m128 d  = _mm_sub_ps(v0, v1);
         __m128 ds = _mm_shuffle_ps(d, d, _MM_SHUFFLE(2, 3, 0, 1));
         __m128 ca = _mm_set_ps(AA[0], AA[0], AA[4], AA[4]);
         __m128 cb = _mm_set_ps(-AA[1], AA[1], -AA[5], AA[5]);
         _mm_storeu_ps(d0, _mm_add_ps(v0, v1));
         _mm_storeu_ps(d1, _mm_add_ps(_mm_mul_ps(d, ca),
               _mm_mul_ps(ds, cb)));
#else
         float v40_20, v41_21;

         v41_21 = e0[1] - e1[1];
         v40_20 = e0[0] - e1[0];
         d0[1]  = e0[1] + e1[1];
         d0[0]  = e0[0] + e1[0];
         d1[1]  = v41_21*AA[4] - v40_20*AA[5];
         d1[0]  = v40_20*AA[4] + v41_21*AA[5];

         v41_21 = e0[3] - e1[3];
         v40_20 = e0[2] - e1[2];
         d0[3]  = e0[3] + e1[3];
         d0[2]  = e0[2] + e1[2];
         d1[3]  = v41_21*AA[0] - v40_20*AA[1];
         d1[2]  = v40_20*AA[0] + v41_21*AA[1];
#endif
         AA -= 8;

         d0 += 4;
         d1 += 4;
         e0 += 4;
         e1 += 4;
      }
   }

   /* step 3 */
   ld = ilog(n) - 1; /* ilog is off-by-one from normal definitions */

   /* optimized step 3:

    * the original step3 loop can be nested r inside s or s inside r;
    * it's written originally as s inside r, but this is dumb when r
    * iterates many times, and s few. So I have two copies of it and
    * switch between them halfway.

    * this is iteration 0 of step 3 */
   imdct_step3_iter0_loop(n >> 4, u, n2-1-n4*0, -(n >> 3), A);
   imdct_step3_iter0_loop(n >> 4, u, n2-1-n4*1, -(n >> 3), A);

   /* this is iteration 1 of step 3 */
   imdct_step3_inner_r_loop(n >> 5, u, n2-1 - n8*0, -(n >> 4), A, 16);
   imdct_step3_inner_r_loop(n >> 5, u, n2-1 - n8*1, -(n >> 4), A, 16);
   imdct_step3_inner_r_loop(n >> 5, u, n2-1 - n8*2, -(n >> 4), A, 16);
   imdct_step3_inner_r_loop(n >> 5, u, n2-1 - n8*3, -(n >> 4), A, 16);

   l=2;
   for (; l < (ld-3)>>1; ++l) {
      int k0 = n >> (l+2), k0_2 = k0>>1;
      int lim = 1 << (l+1);
      int i;
      for (i=0; i < lim; ++i)
         imdct_step3_inner_r_loop(n >> (l+4), u, n2-1 - k0*i, -k0_2, A, 1 << (l+3));
   }

   for (; l < ld-6; ++l) {
      int k0 = n >> (l+2), k1 = 1 << (l+3), k0_2 = k0>>1;
      int rlim = n >> (l+6), r;
      int lim = 1 << (l+1);
      int i_off;
      float *A0 = A;
      i_off = n2-1;
      for (r=rlim; r > 0; --r) {
         imdct_step3_inner_s_loop(lim, u, i_off, -k0_2, A0, k1, k0);
         A0 += k1*4;
         i_off -= 8;
      }
   }

   /* iterations with count:
    *   ld-6,-5,-4 all interleaved together
    *       the big win comes from getting rid of needless flops
    *         due to the constants on pass 5 & 4 being all 1 and 0;
    *       combining them to be simultaneous to improve cache made little difference
    */
   imdct_step3_inner_s_loop_ld654(n >> 5, u, n2-1, A, n);

   /* output is u

    * step 4, 5, and 6
    * cannot be in-place because of step 5 */
   {
      uint16_t *bitrev = f->bit_reverse[blocktype];
      /* weirdly, I'd have thought reading sequentially and writing
       * erratically would have been better than vice-versa, but in
       * fact that's not what my testing showed. (That is, with
       * j = bitreverse(i), do you read i and write j, or read j and write i.) */

      float *d0 = &v[n4-4];
      float *d1 = &v[n2-4];
      while (d0 >= v) {
         int k4;

         k4 = bitrev[0];
         d1[3] = u[k4+0];
         d1[2] = u[k4+1];
         d0[3] = u[k4+2];
         d0[2] = u[k4+3];

         k4 = bitrev[1];
         d1[1] = u[k4+0];
         d1[0] = u[k4+1];
         d0[1] = u[k4+2];
         d0[0] = u[k4+3];

         d0 -= 4;
         d1 -= 4;
         bitrev += 2;
      }
   }
   /* (paper output is u, now v) */


   /* data must be in buf2 */
   assert(v == buf2);

   /* step 7   (paper output is v, now v)
    * this is now in place */
   {
      float *C = f->C[blocktype];
      float *d, *e;

      d = v;
      e = v + n2 - 4;

      while (d < e) {
         float a02,a11,b0,b1,b2,b3;

         a02 = d[0] - e[2];
         a11 = d[1] + e[3];

         b0 = C[1]*a02 + C[0]*a11;
         b1 = C[1]*a11 - C[0]*a02;

         b2 = d[0] + e[ 2];
         b3 = d[1] - e[ 3];

         d[0] = b2 + b0;
         d[1] = b3 + b1;
         e[2] = b2 - b0;
         e[3] = b1 - b3;

         a02 = d[2] - e[0];
         a11 = d[3] + e[1];

         b0 = C[3]*a02 + C[2]*a11;
         b1 = C[3]*a11 - C[2]*a02;

         b2 = d[2] + e[ 0];
         b3 = d[3] - e[ 1];

         d[2] = b2 + b0;
         d[3] = b3 + b1;
         e[0] = b2 - b0;
         e[1] = b1 - b3;

         C += 4;
         d += 4;
         e -= 4;
      }
   }

   /* data must be in buf2


    * step 8+decode   (paper output is X, now buffer)
    * this generates pairs of data a la 8 and pushes them directly through
    * the decode kernel (pushing rather than pulling) to avoid having
    * to make another pass later

    * this cannot POSSIBLY be in place, so we refer to the buffers directly
    */

   {
      float *d0,*d1,*d2,*d3;

      float *B = f->B[blocktype] + n2 - 8;
      float *e = buf2 + n2 - 8;
      d0 = &buffer[0];
      d1 = &buffer[n2-4];
      d2 = &buffer[n2];
      d3 = &buffer[n-4];
      while (e >= v) {
         float p0,p1,p2,p3;

         p3 =  e[6]*B[7] - e[7]*B[6];
         p2 = -e[6]*B[6] - e[7]*B[7];

         d0[0] =   p3;
         d1[3] = - p3;
         d2[0] =   p2;
         d3[3] =   p2;

         p1 =  e[4]*B[5] - e[5]*B[4];
         p0 = -e[4]*B[4] - e[5]*B[5];

         d0[1] =   p1;
         d1[2] = - p1;
         d2[1] =   p0;
         d3[2] =   p0;

         p3 =  e[2]*B[3] - e[3]*B[2];
         p2 = -e[2]*B[2] - e[3]*B[3];

         d0[2] =   p3;
         d1[1] = - p3;
         d2[2] =   p2;
         d3[1] =   p2;

         p1 =  e[0]*B[1] - e[1]*B[0];
         p0 = -e[0]*B[0] - e[1]*B[1];

         d0[3] =   p1;
         d1[0] = - p1;
         d2[3] =   p0;
         d3[0] =   p0;

         B -= 8;
         e -= 8;
         d0 += 4;
         d2 += 4;
         d1 -= 4;
         d3 -= 4;
      }
   }

   temp_alloc_restore(f,save_point);
}

static float *get_window(vorb *f, int len)
{
   len <<= 1;
   if (len == f->blocksize_0) return f->window[0];
   if (len == f->blocksize_1) return f->window[1];
   assert(0);
   return NULL;
}

static int do_floor(vorb *f, Mapping *map, int i, int n, float *target, int16_t *finalY, uint8_t *step2_flag)
{
   Floor1 *g;
   int j,q, lx = 0, ly;
   int n2 = n >> 1;
   int s = map->chan[i].mux, floor;
   floor = map->submap_floor[s];
   if (f->floor_types[floor] == 0)
      return error(f, RVORBIS_invalid_stream);
   g  = &f->floor_config[floor].floor1;
   ly = finalY[0] * g->floor1_multiplier;

   for (q=1; q < g->values; ++q)
   {
      j = g->sorted_order[q];
      if (finalY[j] >= 0)
      {
         int hy = finalY[j] * g->floor1_multiplier;
         int hx = g->Xlist[j];
         draw_line(target, lx,ly, hx,hy, n2);
         lx = hx;
         ly = hy;
      }
   }

   /* Optimization of: draw_line(target, lx,ly, n,ly, n2); */
   if (lx < n2)
      for (j=lx; j < n2; ++j)
         target[j] *= inverse_db_table[ly];
   return 1;
}

static int vorbis_decode_initial(vorb *f, int *p_left_start, int *p_left_end, int *p_right_start, int *p_right_end, int *mode)
{
   Mode *m;
   int i, n, prev, next, window_center;
   f->channel_buffer_start = f->channel_buffer_end = 0;

  retry:
   if (f->eof) return 0;
   if (!maybe_start_packet(f))
      return 0;
   /* check packet type */
   if (get_bits(f,1) != 0)
   {
      while (EOP != get8_packet(f));
      goto retry;
   }

   if (f->alloc.alloc_buffer)
      assert(f->alloc.alloc_buffer_length_in_bytes == f->temp_offset);

   i = get_bits(f, ilog(f->mode_count-1));
   if (i == EOP) return 0;
   if (i >= f->mode_count) return 0;
   *mode = i;
   m = f->mode_config + i;
   if (m->blockflag) {
      n = f->blocksize_1;
      prev = get_bits(f,1);
      next = get_bits(f,1);
   } else {
      prev = next = 0;
      n = f->blocksize_0;
   }

   /* WINDOWING */
   window_center = n >> 1;
   if (m->blockflag && !prev) {
      *p_left_start = (n - f->blocksize_0) >> 2;
      *p_left_end   = (n + f->blocksize_0) >> 2;
   } else {
      *p_left_start = 0;
      *p_left_end   = window_center;
   }
   if (m->blockflag && !next) {
      *p_right_start = (n*3 - f->blocksize_0) >> 2;
      *p_right_end   = (n*3 + f->blocksize_0) >> 2;
   } else {
      *p_right_start = window_center;
      *p_right_end   = n;
   }
   return 1;
}

static int vorbis_decode_packet_rest(vorb *f, int *len, Mode *m, int left_start, int left_end, int right_start, int right_end, int *p_left)
{
   Mapping *map;
   int i,j,k,n,n2;
   int zero_channel[256];
   int really_zero_channel[256];

/* WINDOWING */

   n = f->blocksize[m->blockflag];
   map = &f->mapping[m->mapping];

/* FLOORS */
   n2 = n >> 1;

   for (i=0; i < f->channels; ++i) {
      int s = map->chan[i].mux, floor;
      zero_channel[i] = 0;
      floor = map->submap_floor[s];
      if (f->floor_types[floor] == 0) {
         return error(f, RVORBIS_invalid_stream);
      } else {
         Floor1 *g = &f->floor_config[floor].floor1;
         if (get_bits(f, 1)) {
            short *finalY;
            uint8_t step2_flag[256];
            static int range_list[4] = { 256, 128, 86, 64 };
            int range = range_list[g->floor1_multiplier-1];
            int offset = 2;
            finalY = f->finalY[i];
            finalY[0] = get_bits(f, ilog(range)-1);
            finalY[1] = get_bits(f, ilog(range)-1);
            for (j=0; j < g->partitions; ++j) {
               int pclass = g->partition_class_list[j];
               int cdim = g->class_dimensions[pclass];
               int cbits = g->class_subclasses[pclass];
               int csub = (1 << cbits)-1;
               int cval = 0;
               if (cbits)
               {
                  Codebook *c = f->codebooks + g->class_masterbooks[pclass];
                  cval = codebook_decode_scalar(f,c);
                  if (c->sparse)
                     cval = c->sorted_values[cval];
               }
               for (k=0; k < cdim; ++k)
               {
                  int book = g->subclass_books[pclass][cval & csub];
                  cval = cval >> cbits;
                  if (book >= 0)
                  {
                     Codebook *c = f->codebooks + book;
                     int temp = codebook_decode_scalar(f,c);
                     if (c->sparse)
                        temp = c->sorted_values[temp];
                     finalY[offset++] = temp;
                  }
                  else
                     finalY[offset++] = 0;
               }
            }
            if (f->valid_bits == INVALID_BITS) goto error; /* behavior according to spec */
            step2_flag[0] = step2_flag[1] = 1;
            for (j=2; j < g->values; ++j)
            {
               int low, high, pred, highroom, lowroom, room, val;
               low = g->neighbors[j][0];
               high = g->neighbors[j][1];
               pred = predict_point(g->Xlist[j], g->Xlist[low], g->Xlist[high], finalY[low], finalY[high]);
               val = finalY[j];
               highroom = range - pred;
               lowroom = pred;
               if (highroom < lowroom)
                  room = highroom * 2;
               else
                  room = lowroom * 2;
               if (val) {
                  step2_flag[low] = step2_flag[high] = 1;
                  step2_flag[j] = 1;
                  if (val >= room)
                     if (highroom > lowroom)
                        finalY[j] = val - lowroom + pred;
                     else
                        finalY[j] = pred - val + highroom - 1;
                  else
                     if (val & 1)
                        finalY[j] = pred - ((val+1)>>1);
                     else
                        finalY[j] = pred + (val>>1);
               }
               else
               {
                  step2_flag[j] = 0;
                  finalY[j] = pred;
               }
            }

            /* defer final floor computation until _after_ residue */
            for (j=0; j < g->values; ++j)
            {
               if (!step2_flag[j])
                  finalY[j] = -1;
            }
         }
         else
         {
error:
            zero_channel[i] = 1;
         }
         /* So we just defer everything else to later */

         /* at this point we've decoded the floor into buffer */
      }
   }
   /* at this point we've decoded all floors */

   if (f->alloc.alloc_buffer)
      assert(f->alloc.alloc_buffer_length_in_bytes == f->temp_offset);

   /* re-enable coupled channels if necessary */
   memcpy(really_zero_channel, zero_channel, sizeof(really_zero_channel[0]) * f->channels);
   for (i=0; i < map->coupling_steps; ++i)
      if (!zero_channel[map->chan[i].magnitude] || !zero_channel[map->chan[i].angle]) {
         zero_channel[map->chan[i].magnitude] = zero_channel[map->chan[i].angle] = 0;
      }

/* RESIDUE DECODE */
   for (i=0; i < map->submaps; ++i)
   {
      float *residue_buffers[RVORBIS_MAX_CHANNELS];
      int r;
      uint8_t do_not_decode[256] = {0};
      int ch = 0;
      for (j=0; j < f->channels; ++j)
      {
         if (map->chan[j].mux == i)
         {
            if (zero_channel[j])
            {
               do_not_decode[ch] = 1;
               residue_buffers[ch] = NULL;
            }
            else
            {
               do_not_decode[ch] = 0;
               residue_buffers[ch] = f->channel_buffers[j];
            }
            ++ch;
         }
      }
      r = map->submap_residue[i];
      decode_residue(f, residue_buffers, ch, n2, r, do_not_decode);
   }

   if (f->alloc.alloc_buffer)
      assert(f->alloc.alloc_buffer_length_in_bytes == f->temp_offset);

/* INVERSE COUPLING */
   for (i = map->coupling_steps-1; i >= 0; --i) {
      int n2 = n >> 1;
      float *m = f->channel_buffers[map->chan[i].magnitude];
      float *a = f->channel_buffers[map->chan[i].angle    ];
      for (j=0; j < n2; ++j) {
         float a2,m2;
         if (m[j] > 0)
            if (a[j] > 0)
            {
                m2 = m[j];
                a2 = m[j] - a[j];
            }
            else
            {
                a2 = m[j];
                m2 = m[j] + a[j];
            }
         else
            if (a[j] > 0)
            {
                m2 = m[j];
                a2 = m[j] + a[j];
            }
            else
            {
                a2 = m[j];
                m2 = m[j] - a[j];
            }
         m[j] = m2;
         a[j] = a2;
      }
   }

   /* finish decoding the floors */
   for (i=0; i < f->channels; ++i)
   {
      if (really_zero_channel[i])
         memset(f->channel_buffers[i], 0, sizeof(*f->channel_buffers[i]) * n2);
      else
         do_floor(f, map, i, n, f->channel_buffers[i], f->finalY[i], NULL);
   }

   /* INVERSE MDCT */
   if (f->s16_mode)
   {
      /* s16 pipeline: floor/residue produce float spectral samples;
       * convert the live half to Q28 in place and run the fixed-point
       * MDCT.  Everything to the s16 interleave stays integer. */
      for (i=0; i < f->channels; ++i)
      {
         float   *bf = f->channel_buffers[i];
         int32_t *bq = (int32_t *) f->channel_buffers[i];
         int j;
         for (j=0; j < n >> 1; ++j)
            bq[j] = rvq_float_to_q(bf[j]);
         inverse_mdct_q(bq, n, f, m->blockflag);
      }
   }
   else
   {
      for (i=0; i < f->channels; ++i)
         inverse_mdct(f->channel_buffers[i], n, f, m->blockflag);
   }

   /* this shouldn't be necessary, unless we exited on an error
    * and want to flush to get to the next packet */
   while (get8_packet_raw(f) != EOP);

   if (f->first_decode)
   {
      /* assume we start so first non-discarded sample is sample 0
       * this isn't to spec, but spec would require us to read ahead
       * and decode the size of all current frames--could be done,
       * but presumably it's not a commonly used feature */
      f->current_loc = -n2; /* start of first frame is positioned for discard */
      /* we might have to discard samples "from" the next frame too,
       * if we're lapping a large block then a small at the start? */
      f->discard_samples_deferred = n - right_end;
      f->current_loc_valid = 1;
      f->first_decode = 0;
   }
   else if (f->discard_samples_deferred)
   {
      left_start += f->discard_samples_deferred;
      *p_left = left_start;
      f->discard_samples_deferred = 0;
   } else if (f->previous_length == 0 && f->current_loc_valid) {
      /* we're recovering from a seek... that means we're going to discard
       * the samples from this packet even though we know our position from
       * the last page header, so we need to update the position based on
       * the discarded samples here
       * but wait, the code below is going to add this in itself even
       * on a discard, so we don't need to do it here... */
   }

   /* check if we have ogg information about the sample # for this packet */
   if (f->last_seg_which == f->end_seg_with_known_loc) {
      /* if we have a valid current loc, and this is final: */
      if (f->current_loc_valid && (f->page_flag & PAGEFLAG_last_page)) {
         uint32_t current_end = f->known_loc_for_packet - (n-right_end);
         /* then let's infer the size of the (probably) short final frame */
         if (current_end < f->current_loc + right_end)
         {
            /* negative truncation, that's impossible! */
            if (current_end < f->current_loc)
               *len = 0;
            else
               *len = current_end - f->current_loc;
            *len += left_start;
            f->current_loc += *len;
            return 1;
         }
      }
      /* otherwise, just set our sample loc
       * guess that the ogg granule pos refers to the _middle_ of the
       * last frame?
       * set f->current_loc to the position of left_start */
      f->current_loc = f->known_loc_for_packet - (n2-left_start);
      f->current_loc_valid = 1;
   }
   if (f->current_loc_valid)
      f->current_loc += (right_start - left_start);

   if (f->alloc.alloc_buffer)
      assert(f->alloc.alloc_buffer_length_in_bytes == f->temp_offset);
   *len = right_end;  /* ignore samples after the window goes to 0 */
   return 1;
}

static int vorbis_decode_packet(vorb *f, int *len, int *p_left, int *p_right)
{
   int mode, left_end, right_end;
   if (!vorbis_decode_initial(f, p_left, &left_end, p_right,
            &right_end, &mode))
      return 0;
   return vorbis_decode_packet_rest(f, len, f->mode_config + mode, *p_left, left_end, *p_right, right_end, p_left);
}

static int vorbis_finish_frame(rvorbis *f, int len, int left, int right)
{
   int prev,i;
   /* we use right&left (the start of the right- and left-window sin()-regions)
    * to determine how much to return, rather than inferring from the rules
    * (same result, clearer code); 'left' indicates where our sin() window
    * starts, therefore where the previous window's right edge starts, and
    * therefore where to start mixing from the previous buffer. 'right'
    * indicates where our sin() ending-window starts, therefore that's where
    * we start saving, and where our returned-data ends.

    * mixin from previous window */
   /* Q28 overlap-add for the s16 pipeline (the buffers hold integers) */
   if (f->s16_mode && f->previous_length)
   {
      int i, j, n = f->previous_length;
      const int32_t *wq = (n == f->blocksize_0 >> 1) ? f->window_q[0] : f->window_q[1];
      for (i=0; i < f->channels; ++i)
      {
         int32_t *buf  = (int32_t *) f->channel_buffers[i] + left;
         int32_t *prev = (int32_t *) f->previous_window[i];
         for (j=0; j < n; ++j)
            buf[j] = RVQ_MULQ(buf[j], wq[j])
                   + RVQ_MULQ(prev[j], wq[n-1-j]);
      }
   }
   else if (f->previous_length)
   {
      int i,j, n = f->previous_length;
      float *w = get_window(f, n);
      for (i=0; i < f->channels; ++i)
      {
         float *buf  = f->channel_buffers[i] + left;
         float *prev = f->previous_window[i];
         j = 0;
#if RVORBIS_HAVE_SSE
         /* Overlap-add with the window applied forwards to this frame
          * and backwards to the previous one; the reversed window taps
          * are one lane-reversed load.  Same mul/mul/add per element
          * as the scalar loop, so the results are identical. */
         for (; j + 3 < n; j += 4)
         {
            __m128 vb = _mm_loadu_ps(buf + j);
            __m128 vp = _mm_loadu_ps(prev + j);
            __m128 wf = _mm_loadu_ps(w + j);
            __m128 wr = _mm_loadu_ps(w + n - 4 - j);
            wr = _mm_shuffle_ps(wr, wr, _MM_SHUFFLE(0, 1, 2, 3));
            _mm_storeu_ps(buf + j, _mm_add_ps(_mm_mul_ps(vb, wf),
                  _mm_mul_ps(vp, wr)));
         }
#elif RVORBIS_HAVE_NEON
         for (; j + 3 < n; j += 4)
         {
            float32x4_t vb = vld1q_f32(buf + j);
            float32x4_t vp = vld1q_f32(prev + j);
            float32x4_t wf = vld1q_f32(w + j);
            float32x4_t wr = vld1q_f32(w + n - 4 - j);
            wr = vrev64q_f32(wr);
            wr = vcombine_f32(vget_high_f32(wr), vget_low_f32(wr));
            vst1q_f32(buf + j, vaddq_f32(vmulq_f32(vb, wf),
                  vmulq_f32(vp, wr)));
         }
#endif
         for (; j < n; ++j)
            buf[j] = buf[j]*w[j] + prev[j]*w[n-1-j];
      }
   }

   prev = f->previous_length;

   /* last half of this data becomes previous window */
   f->previous_length = len - right;

   /* @OPTIMIZE: could avoid this copy by double-buffering the
    * output (flipping previous_window with channel_buffers), but
    * then previous_window would have to be 2x as large, and
    * channel_buffers couldn't be temp mem (although they're NOT
    * currently temp mem, they could be (unless we want to level
    * performance by spreading out the computation)) */
   /* channel_buffers and previous_window are distinct allocations, so
    * the save is a straight copy.  A truncated short frame can have
    * len < right (the scalar loop simply ran zero times); guard so the
    * size below cannot go negative. */
   if (len > right)
   {
      for (i=0; i < f->channels; ++i)
         memcpy(f->previous_window[i], f->channel_buffers[i] + right,
               (size_t)(len - right) * sizeof(float));
   }

   /* There was no previous packet, so this data isn't valid...
    * this isn't entirely true, only the would-have-overlapped data
    * isn't valid, but this seems to be what the spec requires */
   if (!prev)
      return 0;

   /* truncate a short frame */
   if (len < right)
      right = len;

   f->samples_output += right-left;

   return right - left;
}

static void vorbis_pump_first_frame(rvorbis *f)
{
   int len, right, left;
   if (vorbis_decode_packet(f, &len, &left, &right))
      vorbis_finish_frame(f, len, left, right);
}

static int start_decoder(vorb *f)
{
   uint8_t header[6], x,y;
   int len,i,j,k, max_submaps = 0;
   int longest_floorlist=0;

   /* first page, first packet */

   if (!start_page(f))
      return 0;

   /* validate page flag */
   if (!(f->page_flag & PAGEFLAG_first_page))
      return error(f, RVORBIS_invalid_first_page);
   if (f->page_flag & PAGEFLAG_last_page)
      return error(f, RVORBIS_invalid_first_page);
   if (f->page_flag & PAGEFLAG_continued_packet)
      return error(f, RVORBIS_invalid_first_page);
   /* check for expected packet length */
   if (f->segment_count != 1)
      return error(f, RVORBIS_invalid_first_page);
   if (f->segments[0] != 30)
      return error(f, RVORBIS_invalid_first_page);
   /* read packet
    * check packet header */
   if (get8(f) != RVORBIS_packet_id)
      return error(f, RVORBIS_invalid_first_page);
   if (!getn(f, header, 6))
      return error(f, RVORBIS_unexpected_eof);
   if (!vorbis_validate(header))
      return error(f, RVORBIS_invalid_first_page);
   /* vorbis_version */
   if (get32(f) != 0)
      return error(f, RVORBIS_invalid_first_page);
   f->channels = get8(f);
   if (!f->channels)
      return error(f, RVORBIS_invalid_first_page);
   if (f->channels > RVORBIS_MAX_CHANNELS)
      return error(f, RVORBIS_too_many_channels);
   f->sample_rate = get32(f);
   if (!f->sample_rate)
      return error(f, RVORBIS_invalid_first_page);
   get32(f); /* bitrate_maximum */
   get32(f); /* bitrate_nominal */
   get32(f); /* bitrate_minimum */
   x = get8(f);
   {
      int log0 = x & 15;
      int log1 = x >> 4;
      f->blocksize_0 = 1 << log0;
      f->blocksize_1 = 1 << log1;
      if (log0 < 6 || log0 > 13)
         return error(f, RVORBIS_invalid_setup);
      if (log1 < 6 || log1 > 13)
         return error(f, RVORBIS_invalid_setup);
      if (log0 > log1)
         return error(f, RVORBIS_invalid_setup);
   }

   /* framing_flag */
   x = get8(f);
   if (!(x & 1))
      return error(f, RVORBIS_invalid_first_page);

   /* second packet! */
   if (!start_page(f))
      return 0;

   if (!start_packet(f))
      return 0;

   do
   {
      len = next_segment(f);
      skip(f, len);
      f->bytes_in_seg = 0;
   } while (len);

   /* third packet! */
   if (!start_packet(f))
      return 0;

   crc32_init(); /* always init it, to avoid multithread race conditions */

   if (get8_packet(f) != RVORBIS_packet_setup)
      return error(f, RVORBIS_invalid_setup);
   for (i=0; i < 6; ++i) header[i] = get8_packet(f);
   if (!vorbis_validate(header))
      return error(f, RVORBIS_invalid_setup);

   /* codebooks */

   f->codebook_count = get_bits(f,8) + 1;
   f->codebooks = (Codebook *) setup_malloc(f, sizeof(*f->codebooks) * f->codebook_count);
   if (!f->codebooks)
      return error(f, RVORBIS_outofmem);
   memset(f->codebooks, 0, sizeof(*f->codebooks) * f->codebook_count);
   for (i=0; i < f->codebook_count; ++i)
   {
      uint32_t *values;
      int ordered, sorted_count;
      int total=0;
      uint8_t *lengths;
      Codebook *c = f->codebooks+i;
      x = get_bits(f, 8);
      if (x != 0x42)
         return error(f, RVORBIS_invalid_setup);
      x = get_bits(f, 8);
      if (x != 0x43)
         return error(f, RVORBIS_invalid_setup);
      x = get_bits(f, 8);
      if (x != 0x56)
         return error(f, RVORBIS_invalid_setup);
      x = get_bits(f, 8);
      c->dimensions = (get_bits(f, 8)<<8) + x;
      x = get_bits(f, 8);
      y = get_bits(f, 8);
      c->entries = (get_bits(f, 8)<<16) + (y<<8) + x;
      ordered = get_bits(f,1);
      c->sparse = ordered ? 0 : get_bits(f,1);

      if (c->sparse)
         lengths = (uint8_t *) setup_temp_malloc(f, c->entries);
      else
         lengths = c->codeword_lengths = (uint8_t *) setup_malloc(f, c->entries);

      if (!lengths)
         return error(f, RVORBIS_outofmem);

      if (ordered)
      {
         int current_entry = 0;
         int current_length = get_bits(f,5) + 1;
         while (current_entry < c->entries)
         {
            int limit = c->entries - current_entry;
            int n = get_bits(f, ilog(limit));
            if (current_entry + n > (int) c->entries)
               return error(f, RVORBIS_invalid_setup);
            memset(lengths + current_entry, current_length, n);
            current_entry += n;
            ++current_length;
         }
      }
      else
      {
         for (j=0; j < c->entries; ++j)
         {
            int present = c->sparse ? get_bits(f,1) : 1;
            if (present)
            {
               lengths[j] = get_bits(f, 5) + 1;
               ++total;
            }
            else
               lengths[j] = NO_CODE;
         }
      }

      if (c->sparse && total >= c->entries >> 2)
      {
         /* convert sparse items to non-sparse! */
         if (c->entries > (int) f->setup_temp_memory_required)
            f->setup_temp_memory_required = c->entries;

         c->codeword_lengths = (uint8_t *) setup_malloc(f, c->entries);
         memcpy(c->codeword_lengths, lengths, c->entries);
         setup_temp_free(f, lengths, c->entries); /* note this is only safe if there have been no intervening temp mallocs! */
         lengths = c->codeword_lengths;
         c->sparse = 0;
      }

      /* compute the size of the sorted tables */
      if (c->sparse)
         sorted_count = total;
      else
      {
         sorted_count = 0;
         for (j=0; j < c->entries; ++j)
            if (lengths[j] > RVORBIS_FAST_HUFFMAN_LENGTH && lengths[j] != NO_CODE)
               ++sorted_count;
      }

      c->sorted_entries = sorted_count;
      values = NULL;

      if (!c->sparse)
      {
         c->codewords = (uint32_t *) setup_malloc(f, sizeof(c->codewords[0]) * c->entries);
         if (!c->codewords)
            return error(f, RVORBIS_outofmem);
      }
      else
      {
         unsigned int size;
         if (c->sorted_entries)
         {
            c->codeword_lengths = (uint8_t *) setup_malloc(f, c->sorted_entries);
            if (!c->codeword_lengths)
               return error(f, RVORBIS_outofmem);
            c->codewords = (uint32_t *) setup_temp_malloc(f, sizeof(*c->codewords) * c->sorted_entries);
            if (!c->codewords)
               return error(f, RVORBIS_outofmem);
            values = (uint32_t *) setup_temp_malloc(f, sizeof(*values) * c->sorted_entries);
            if (!values)
               return error(f, RVORBIS_outofmem);
         }
         size = c->entries + (sizeof(*c->codewords) + sizeof(*values)) * c->sorted_entries;
         if (size > f->setup_temp_memory_required)
            f->setup_temp_memory_required = size;
      }

      if (!compute_codewords(c, lengths, c->entries, values))
      {
         if (c->sparse)
            setup_temp_free(f, values, 0);
         return error(f, RVORBIS_invalid_setup);
      }

      if (c->sorted_entries)
      {
         /* allocate an extra slot for sentinels */
         c->sorted_codewords = (uint32_t *) setup_malloc(f, sizeof(*c->sorted_codewords) * (c->sorted_entries+1));
         /* allocate an extra slot at the front so that c->sorted_values[-1] is defined
          * so that we can catch that case without an extra if */
         c->sorted_values    = ( int   *) setup_malloc(f, sizeof(*c->sorted_values   ) * (c->sorted_entries+1));
         if (c->sorted_values) { ++c->sorted_values; c->sorted_values[-1] = -1; }
         compute_sorted_huffman(c, lengths, values);
      }

      if (c->sparse) {
         setup_temp_free(f, values, sizeof(*values)*c->sorted_entries);
         setup_temp_free(f, c->codewords, sizeof(*c->codewords)*c->sorted_entries);
         setup_temp_free(f, lengths, c->entries);
         c->codewords = NULL;
      }

      compute_accelerated_huffman(c);

      c->lookup_type = get_bits(f, 4);
      if (c->lookup_type > 2) return error(f, RVORBIS_invalid_setup);
      if (c->lookup_type > 0) {
         uint16_t *mults;
         c->minimum_value = float32_unpack(get_bits(f, 32));
         c->delta_value = float32_unpack(get_bits(f, 32));
         c->value_bits = get_bits(f, 4)+1;
         c->sequence_p = get_bits(f,1);
         if (c->lookup_type == 1)
            c->lookup_values = lookup1_values(c->entries, c->dimensions);
         else
            c->lookup_values = c->entries * c->dimensions;
         mults = (uint16_t *) setup_temp_malloc(f, sizeof(mults[0]) * c->lookup_values);
         if (!mults)
            return error(f, RVORBIS_outofmem);
         for (j=0; j < (int) c->lookup_values; ++j)
         {
            int q = get_bits(f, c->value_bits);
            if (q == EOP)
            {
               setup_temp_free(f,mults,sizeof(mults[0])*c->lookup_values);
               return error(f, RVORBIS_invalid_setup);
            }
            mults[j] = q;
         }

         if (c->lookup_type == 1)
         {
            int len, sparse = c->sparse;
            /* pre-expand the lookup1-style multiplicands, to avoid a divide in the inner loop */
            if (sparse)
            {
               if (c->sorted_entries == 0)
                  goto skip;
               c->multiplicands = (rvorbis_codetype *) setup_malloc(f, sizeof(c->multiplicands[0]) * c->sorted_entries * c->dimensions);
            } else
               c->multiplicands = (rvorbis_codetype *) setup_malloc(f, sizeof(c->multiplicands[0]) * c->entries        * c->dimensions);
            if (!c->multiplicands) { setup_temp_free(f,mults,sizeof(mults[0])*c->lookup_values); return error(f, RVORBIS_outofmem); }
            len = sparse ? c->sorted_entries : c->entries;
            for (j=0; j < len; ++j) {
               int z = sparse ? c->sorted_values[j] : j, div=1;
               for (k=0; k < c->dimensions; ++k) {
                  int off = (z / div) % c->lookup_values;
                  c->multiplicands[j*c->dimensions + k] =
#ifndef RVORBIS_CODEBOOK_FLOATS
                     mults[off];
#else
                  mults[off]*c->delta_value + c->minimum_value;
                  /* in this case (and this case only) we could pre-expand c->sequence_p,
                   * and throw away the decode logic for it; have to ALSO do
                   * it in the case below, but it can only be done if
                   *    RVORBIS_CODEBOOK_FLOATS     */
#endif
                  div *= c->lookup_values;
               }
            }
            setup_temp_free(f, mults,sizeof(mults[0])*c->lookup_values);
            c->lookup_type = 2;
         }
         else
         {
            c->multiplicands = (rvorbis_codetype *) setup_malloc(f, sizeof(c->multiplicands[0]) * c->lookup_values);
#ifndef RVORBIS_CODEBOOK_FLOATS
            memcpy(c->multiplicands, mults, sizeof(c->multiplicands[0]) * c->lookup_values);
#else
            for (j=0; j < (int) c->lookup_values; ++j)
               c->multiplicands[j] = mults[j] * c->delta_value + c->minimum_value;
#endif
            setup_temp_free(f, mults,sizeof(mults[0])*c->lookup_values);
         }
skip:;

#ifdef RVORBIS_CODEBOOK_FLOATS
     if (c->lookup_type == 2 && c->sequence_p) {
        for (j=1; j < (int) c->lookup_values; ++j)
           c->multiplicands[j] = c->multiplicands[j-1];
        c->sequence_p = 0;
     }
#endif
      }
   }

   /* time domain transfers (notused) */

   x = get_bits(f, 6) + 1;
   for (i=0; i < x; ++i)
   {
      uint32_t z = get_bits(f, 16);
      if (z != 0)
         return error(f, RVORBIS_invalid_setup);
   }

   /* Floors */
   f->floor_count  = get_bits(f, 6)+1;
   f->floor_config = (Floor *)setup_malloc(f, f->floor_count * sizeof(*f->floor_config));

   for (i=0; i < f->floor_count; ++i)
   {
      f->floor_types[i] = get_bits(f, 16);
      if (f->floor_types[i] > 1)
         return error(f, RVORBIS_invalid_setup);
      if (f->floor_types[i] == 0)
      {
         Floor0 *g           = &f->floor_config[i].floor0;
         g->order            = get_bits(f,8);
         g->rate             = get_bits(f,16);
         g->bark_map_size    = get_bits(f,16);
         g->amplitude_bits   = get_bits(f,6);
         g->amplitude_offset = get_bits(f,8);
         g->number_of_books  = get_bits(f,4) + 1;
         for (j=0; j < g->number_of_books; ++j)
            g->book_list[j]  = get_bits(f,8);
         return error(f, RVORBIS_feature_not_supported);
      }
      else
      {
         STBV_Point p[31*8+2];
         Floor1 *g = &f->floor_config[i].floor1;
         int max_class = -1;
         g->partitions = get_bits(f, 5);
         for (j=0; j < g->partitions; ++j) {
            g->partition_class_list[j] = get_bits(f, 4);
            if (g->partition_class_list[j] > max_class)
               max_class = g->partition_class_list[j];
         }
         for (j=0; j <= max_class; ++j) {
            g->class_dimensions[j] = get_bits(f, 3)+1;
            g->class_subclasses[j] = get_bits(f, 2);
            if (g->class_subclasses[j]) {
               g->class_masterbooks[j] = get_bits(f, 8);
               if (g->class_masterbooks[j] >= f->codebook_count) return error(f, RVORBIS_invalid_setup);
            }
            for (k=0; k < 1 << g->class_subclasses[j]; ++k) {
               g->subclass_books[j][k] = get_bits(f,8)-1;
               if (g->subclass_books[j][k] >= f->codebook_count) return error(f, RVORBIS_invalid_setup);
            }
         }
         g->floor1_multiplier = get_bits(f,2)+1;
         g->rangebits = get_bits(f,4);
         g->Xlist[0] = 0;
         g->Xlist[1] = 1 << g->rangebits;
         g->values = 2;
         for (j=0; j < g->partitions; ++j) {
            int c = g->partition_class_list[j];
            for (k=0; k < g->class_dimensions[c]; ++k) {
               g->Xlist[g->values] = get_bits(f, g->rangebits);
               ++g->values;
            }
         }
         /* precompute the sorting */
         for (j=0; j < g->values; ++j) {
            p[j].x = g->Xlist[j];
            p[j].y = j;
         }
         qsort(p, g->values, sizeof(p[0]), point_compare);
         for (j=0; j < g->values; ++j)
            g->sorted_order[j] = (uint8_t) p[j].y;
         /* precompute the neighbors */
         for (j=2; j < g->values; ++j)
         {
            int low = 0;
            int hi  = 0;
            neighbors(g->Xlist, j, &low,&hi);
            g->neighbors[j][0] = low;
            g->neighbors[j][1] = hi;
         }

         if (g->values > longest_floorlist)
            longest_floorlist = g->values;
      }
   }

   /* Residue */
   f->residue_count = get_bits(f, 6)+1;
   f->residue_config = (Residue *) setup_malloc(f, f->residue_count * sizeof(*f->residue_config));
   for (i=0; i < f->residue_count; ++i)
   {
      uint8_t residue_cascade[64];
      Residue *r = f->residue_config+i;
      f->residue_types[i] = get_bits(f, 16);
      if (f->residue_types[i] > 2) return error(f, RVORBIS_invalid_setup);
      r->begin = get_bits(f, 24);
      r->end = get_bits(f, 24);
      r->part_size = get_bits(f,24)+1;
      r->classifications = get_bits(f,6)+1;
      r->classbook = get_bits(f,8);
      for (j=0; j < r->classifications; ++j) {
         uint8_t high_bits=0;
         uint8_t low_bits=get_bits(f,3);
         if (get_bits(f,1))
            high_bits = get_bits(f,5);
         residue_cascade[j] = high_bits*8 + low_bits;
      }
      r->residue_books = (short (*)[8]) setup_malloc(f, sizeof(r->residue_books[0]) * r->classifications);
      for (j=0; j < r->classifications; ++j) {
         for (k=0; k < 8; ++k) {
            if (residue_cascade[j] & (1 << k)) {
               r->residue_books[j][k] = get_bits(f, 8);
               if (r->residue_books[j][k] >= f->codebook_count) return error(f, RVORBIS_invalid_setup);
            } else {
               r->residue_books[j][k] = -1;
            }
         }
      }
      /* precompute the classifications[] array to avoid inner-loop mod/divide
       * call it 'classdata' since we already have r->classifications */
      r->classdata = (uint8_t **) setup_malloc(f, sizeof(*r->classdata) * f->codebooks[r->classbook].entries);
      if (!r->classdata) return error(f, RVORBIS_outofmem);
      memset(r->classdata, 0, sizeof(*r->classdata) * f->codebooks[r->classbook].entries);
      for (j=0; j < f->codebooks[r->classbook].entries; ++j) {
         int classwords = f->codebooks[r->classbook].dimensions;
         int temp = j;
         r->classdata[j] = (uint8_t *) setup_malloc(f, sizeof(r->classdata[j][0]) * classwords);
         for (k=classwords-1; k >= 0; --k) {
            r->classdata[j][k] = temp % r->classifications;
            temp /= r->classifications;
         }
      }
   }

   f->mapping_count = get_bits(f,6)+1;
   f->mapping = (Mapping *) setup_malloc(f, f->mapping_count * sizeof(*f->mapping));
   for (i=0; i < f->mapping_count; ++i) {
      Mapping *m = f->mapping + i;
      int mapping_type = get_bits(f,16);
      if (mapping_type != 0) return error(f, RVORBIS_invalid_setup);
      m->chan = (MappingChannel *) setup_malloc(f, f->channels * sizeof(*m->chan));
      if (get_bits(f,1))
         m->submaps = get_bits(f,4)+1;
      else
         m->submaps = 1;
      if (m->submaps > max_submaps)
         max_submaps = m->submaps;
      if (get_bits(f,1)) {
         m->coupling_steps = get_bits(f,8)+1;
         for (k=0; k < m->coupling_steps; ++k) {
            m->chan[k].magnitude = get_bits(f, ilog(f->channels-1));
            m->chan[k].angle = get_bits(f, ilog(f->channels-1));
            if (m->chan[k].magnitude >= f->channels)        return error(f, RVORBIS_invalid_setup);
            if (m->chan[k].angle     >= f->channels)        return error(f, RVORBIS_invalid_setup);
            if (m->chan[k].magnitude == m->chan[k].angle)   return error(f, RVORBIS_invalid_setup);
         }
      } else
         m->coupling_steps = 0;

      /* reserved field */
      if (get_bits(f,2)) return error(f, RVORBIS_invalid_setup);
      if (m->submaps > 1) {
         for (j=0; j < f->channels; ++j) {
            m->chan[j].mux = get_bits(f, 4);
            if (m->chan[j].mux >= m->submaps)                return error(f, RVORBIS_invalid_setup);
         }
      } else
         /* @SPECIFICATION: this case is missing from the spec */
         for (j=0; j < f->channels; ++j)
            m->chan[j].mux = 0;

      for (j=0; j < m->submaps; ++j) {
         get_bits(f,8); /* discard */
         m->submap_floor[j] = get_bits(f,8);
         m->submap_residue[j] = get_bits(f,8);
         if (m->submap_floor[j] >= f->floor_count)      return error(f, RVORBIS_invalid_setup);
         if (m->submap_residue[j] >= f->residue_count)  return error(f, RVORBIS_invalid_setup);
      }
   }

   /* Modes */
   f->mode_count = get_bits(f, 6)+1;
   for (i=0; i < f->mode_count; ++i) {
      Mode *m = f->mode_config+i;
      m->blockflag = get_bits(f,1);
      m->windowtype = get_bits(f,16);
      m->transformtype = get_bits(f,16);
      m->mapping = get_bits(f,8);
      if (m->windowtype != 0)                 return error(f, RVORBIS_invalid_setup);
      if (m->transformtype != 0)              return error(f, RVORBIS_invalid_setup);
      if (m->mapping >= f->mapping_count)     return error(f, RVORBIS_invalid_setup);
   }

   while (get8_packet_raw(f) != EOP);

   f->previous_length = 0;

   for (i=0; i < f->channels; ++i) {
      f->channel_buffers[i] = (float *) setup_malloc(f, sizeof(float) * f->blocksize_1);
      f->previous_window[i] = (float *) setup_malloc(f, sizeof(float) * f->blocksize_1/2);
      f->finalY[i]          = (int16_t *) setup_malloc(f, sizeof(int16_t) * longest_floorlist);
   }

   if (!init_blocksize(f, 0, f->blocksize_0)) return 0;
   if (!init_blocksize(f, 1, f->blocksize_1)) return 0;
   f->blocksize[0] = f->blocksize_0;
   f->blocksize[1] = f->blocksize_1;

   /* compute how much temporary memory is needed */

   /* 1. */
   {
      uint32_t imdct_mem = (f->blocksize_1 * sizeof(float) >> 1);
      uint32_t classify_mem;
      int i,max_part_read=0;
      for (i=0; i < f->residue_count; ++i) {
         Residue *r = f->residue_config + i;
         int n_read = r->end - r->begin;
         int part_read = n_read / r->part_size;
         if (part_read > max_part_read)
            max_part_read = part_read;
      }
      classify_mem = f->channels * (sizeof(void*) + max_part_read * sizeof(uint8_t *));

      f->temp_memory_required = classify_mem;
      if (imdct_mem > f->temp_memory_required)
         f->temp_memory_required = imdct_mem;
   }

   f->first_decode = 1;

   if (f->alloc.alloc_buffer) {
      assert(f->temp_offset == f->alloc.alloc_buffer_length_in_bytes);
      /* check if there's enough temp memory so we don't error later */
      if (f->setup_offset + sizeof(*f) + f->temp_memory_required > (unsigned) f->temp_offset)
         return error(f, RVORBIS_outofmem);
   }

   f->first_audio_page_offset = (unsigned int)(f->stream - f->stream_start);

   return 1;
}

static void vorbis_deinit(rvorbis *p)
{
   int i,j;
   for (i=0; i < p->residue_count; ++i) {
      Residue *r = p->residue_config+i;
      if (r->classdata) {
         for (j=0; j < p->codebooks[r->classbook].entries; ++j)
            setup_free(p, r->classdata[j]);
         setup_free(p, r->classdata);
      }
      setup_free(p, r->residue_books);
   }

   if (p->codebooks) {
      for (i=0; i < p->codebook_count; ++i) {
         Codebook *c = p->codebooks + i;
         setup_free(p, c->codeword_lengths);
         setup_free(p, c->multiplicands);
         setup_free(p, c->codewords);
         setup_free(p, c->sorted_codewords);
         /* c->sorted_values[-1] is the first entry in the array */
         setup_free(p, c->sorted_values ? c->sorted_values-1 : NULL);
      }
      setup_free(p, p->codebooks);
   }
   setup_free(p, p->floor_config);
   setup_free(p, p->residue_config);
   for (i=0; i < p->mapping_count; ++i)
      setup_free(p, p->mapping[i].chan);
   setup_free(p, p->mapping);
   for (i=0; i < p->channels; ++i) {
      setup_free(p, p->channel_buffers[i]);
      setup_free(p, p->previous_window[i]);
      setup_free(p, p->finalY[i]);
   }
   for (i=0; i < 2; ++i) {
      setup_free(p, p->A[i]);
      setup_free(p, p->B[i]);
      setup_free(p, p->C[i]);
      setup_free(p, p->window[i]);
      setup_free(p, p->A_q[i]);
      setup_free(p, p->B_q[i]);
      setup_free(p, p->C_q[i]);
      setup_free(p, p->window_q[i]);
      setup_free(p, p->bit_reverse[i]);
   }
}

void rvorbis_close(rvorbis *p)
{
   if (p)
   {
      vorbis_deinit(p);
      setup_free(p,p);
   }
}

static void vorbis_init(rvorbis *p, rvorbis_alloc *z)
{
   memset(p, 0, sizeof(*p)); /* NULL out all malloc'd pointers to start */
   if (z)
   {
      p->alloc = *z;
      p->alloc.alloc_buffer_length_in_bytes = (p->alloc.alloc_buffer_length_in_bytes+3) & ~3;
      p->temp_offset = p->alloc.alloc_buffer_length_in_bytes;
   }
   p->eof            = 0;
   p->error          = RVORBIS__no_error;
   p->stream         = NULL;
   p->codebooks      = NULL;
   p->page_crc_tests = -1;
}

rvorbis_info rvorbis_get_info(rvorbis *f)
{
   rvorbis_info d;
   d.channels                   = f->channels;
   d.sample_rate                = f->sample_rate;
   d.setup_memory_required      = f->setup_memory_required;
   d.setup_temp_memory_required = f->setup_temp_memory_required;
   d.temp_memory_required       = f->temp_memory_required;
   d.max_frame_size             = f->blocksize_1 >> 1;
   return d;
}

int rvorbis_get_error(rvorbis *f)
{
   int e = f->error;
   f->error = RVORBIS__no_error;
   return e;
}

static rvorbis * vorbis_alloc(rvorbis *f)
{
   rvorbis *p = (rvorbis *)setup_malloc(f, sizeof(*p));
   return p;
}

/* DATA-PULLING API */

static uint32_t vorbis_find_page(rvorbis *f, uint32_t *end, uint32_t *last)
{
   for(;;)
   {
      int n;
      if (f->eof) return 0;
      n = get8(f);
      if (n == 0x4f) { /* page header */
         unsigned int retry_loc = (unsigned int)(f->stream - f->stream_start);
         int i;
         /* check if we're off the end of a file_section stream */
         if (retry_loc - 25 > f->stream_len)
            return 0;
         /* check the rest of the header */
         for (i=1; i < 4; ++i)
            if (get8(f) != ogg_page_header[i])
               break;
         if (f->eof)
            return 0;
         if (i == 4)
         {
            uint8_t header[27];
            uint32_t i, crc, goal, len;
            for (i=0; i < 4; ++i)
               header[i] = ogg_page_header[i];
            for (; i < 27; ++i)
               header[i] = get8(f);
            if (f->eof) return 0;
            if (header[4] != 0) goto invalid;
            goal = header[22] + (header[23] << 8) + (header[24]<<16) + (header[25]<<24);
            for (i=22; i < 26; ++i)
               header[i] = 0;
            crc = 0;
            for (i=0; i < 27; ++i)
               crc = crc32_update(crc, header[i]);
            len = 0;
            for (i=0; i < header[26]; ++i)
            {
               int s = get8(f);
               crc = crc32_update(crc, s);
               len += s;
            }
            if (len && f->eof) return 0;
            for (i=0; i < len; ++i)
               crc = crc32_update(crc, get8(f));
            /* finished parsing probable page */
            if (crc == goal)
            {
               /* we could now check that it's either got the last
                * page flag set, OR it's followed by the capture
                * pattern, but I guess TECHNICALLY you could have
                * a file with garbage between each ogg page and recover
                * from it automatically? So even though that paranoia
                * might decrease the chance of an invalid decode by
                * another 2^32, not worth it since it would hose those
                * invalid-but-useful files? */
               if (end)
                  *end = (unsigned int)(f->stream - f->stream_start);
               if (last)
               {
                  if (header[5] & 0x04)
                     *last = 1;
                  else
                     *last = 0;
               }
               set_file_offset(f, retry_loc-1);
               return 1;
            }
         }
        invalid:
         /* not a valid page, so rewind and look for next one */
         set_file_offset(f, retry_loc);
      }
   }
}

/* seek is implemented with 'interpolation search'--this is like
 * binary search, but we use the data values to estimate the likely
 * location of the data item (plus a bit of a bias so when the
 * estimation is wrong we don't waste overly much time)
 */

#define SAMPLE_unknown  0xffffffff

/* ogg vorbis, in its insane infinite wisdom, only provides
 * information about the sample at the END of the page.
 * therefore we COULD have the data we need in the current
 * page, and not know it. we could just use the end location
 * as our only knowledge for bounds, seek back, and eventually
 * the binary search finds it. or we can try to be smart and
 * not waste time trying to locate more pages. we try to be
 * smart, since this data is already in memory anyway, so
 * doing needless I/O would be crazy!
 */
static int vorbis_analyze_page(rvorbis *f, ProbedPage *z)
{
   uint8_t lacing[255];
   uint8_t packet_type[255];
   int num_packet, packet_start;
   int i,len;
   uint32_t samples;
   uint8_t header[27] = {0};

   /* record where the page starts */
   z->page_start = (unsigned int)(f->stream - f->stream_start);

   /* parse the header */
   getn(f, header, 27);
   assert(header[0] == 'O' && header[1] == 'g' && header[2] == 'g' && header[3] == 'S');
   getn(f, lacing, header[26]);

   /* determine the length of the payload */
   len = 0;
   for (i=0; i < header[26]; ++i)
      len += lacing[i];

   /* this implies where the page ends */
   z->page_end = z->page_start + 27 + header[26] + len;

   /* read the last-decoded sample out of the data */
   z->last_decoded_sample = header[6] + (header[7] << 8) + (header[8] << 16) + (header[9] << 24);

   if (header[5] & 4)
   {
      /* if this is the last page, it's not possible to work
       * backwards to figure out the first sample! whoops! fuck. */
      z->first_decoded_sample = SAMPLE_unknown;
      set_file_offset(f, z->page_start);
      return 1;
   }

   /* scan through the frames to determine the sample-count of each one...
    * our goal is the sample # of the first fully-decoded sample on the
    * page, which is the first decoded sample of the 2nd packet */

   num_packet=0;

   packet_start = ((header[5] & 1) == 0);

   for (i=0; i < header[26]; ++i)
   {
      if (packet_start)
      {
         uint8_t n,b;
         if (lacing[i] == 0)
            goto bail; /* trying to read from zero-length packet */
         n = get8(f);
         /* if bottom bit is non-zero, we've got corruption */
         if (n & 1)
            goto bail;
         n >>= 1;
         b = ilog(f->mode_count-1);
         n &= (1 << b)-1;
         if (n >= f->mode_count)
            goto bail;
         packet_type[num_packet++] = f->mode_config[n].blockflag;
         skip(f, lacing[i]-1);
      }
      else
         skip(f, lacing[i]);
      packet_start = (lacing[i] < 255);
   }

   /* now that we know the sizes of all the pages, we can start determining
    * how much sample data there is. */

   samples = 0;

   /* for the last packet, we step by its whole length, because the definition
    * is that we encoded the end sample loc of the 'last packet completed',
    * where 'completed' refers to packets being split, and we are left to guess
    * what 'end sample loc' means. we assume it means ignoring the fact that
    * the last half of the data is useless without windowing against the next
    * packet... (so it's not REALLY complete in that sense)
    */
   if (num_packet > 1)
      samples += f->blocksize[packet_type[num_packet-1]];

   for (i=num_packet-2; i >= 1; --i)
   {
      /* now, for this packet, how many samples do we have that
       * do not overlap the following packet? */
      if (packet_type[i] == 1)
         if (packet_type[i+1] == 1)
            samples += f->blocksize_1 >> 1;
         else
            samples += ((f->blocksize_1 - f->blocksize_0) >> 2) + (f->blocksize_0 >> 1);
      else
         samples += f->blocksize_0 >> 1;
   }
   /* now, at this point, we've rewound to the very beginning of the
    * _second_ packet. if we entirely discard the first packet after
    * a seek, this will be exactly the right sample number. HOWEVER!
    * we can't as easily compute this number for the LAST page. The
    * only way to get the sample offset of the LAST page is to use
    * the end loc from the previous page. But what that returns us
    * is _exactly_ the place where we get our first non-overlapped
    * sample. (I think. Stupid spec for being ambiguous.) So for
    * consistency it's better to do that here, too. However, that
    * will then require us to NOT discard all of the first frame we
    * decode, in some cases, which means an even weirder frame size
    * and extra code. what a fucking pain.

    * we're going to discard the first packet if we
    * start the seek here, so we don't care about it. (we could actually
    * do better; if the first packet is long, and the previous packet
    * is short, there's actually data in the first half of the first
    * packet that doesn't need discarding... but not worth paying the
    * effort of tracking that of that here and in the seeking logic)
    * except crap, if we infer it from the _previous_ packet's end
    * location, we DO need to use that definition... and we HAVE to
    * infer the start loc of the LAST packet from the previous packet's
    * end location. fuck you, ogg vorbis. */

   z->first_decoded_sample = z->last_decoded_sample - samples;

   /* restore file state to where we were */
   set_file_offset(f, z->page_start);
   return 1;

   /* restore file state to where we were */
  bail:
   set_file_offset(f, z->page_start);
   return 0;
}

/* Latch the output pipeline (0 = float, 1 = fixed-point s16).  In
 * normal mixer use this runs once, on the first read; a mid-stream
 * switch converts the persistent overlap state and any undelivered
 * samples of the current frame so playback continues seamlessly. */
static void rvorbis_set_output_mode(vorb *f, int s16)
{
   int i, j;
   if (f->s16_mode == s16)
      return;
   f->s16_mode = s16;
   for (i=0; i < f->channels; ++i)
   {
      float   *pf = f->previous_window[i];
      int32_t *pq = (int32_t *) f->previous_window[i];
      float   *cf = f->channel_buffers[i];
      int32_t *cq = (int32_t *) f->channel_buffers[i];
      if (s16)
      {
         for (j=0; j < f->previous_length; ++j)
            pq[j] = rvq_float_to_q(pf[j]);
         for (j=f->channel_buffer_start; j < f->channel_buffer_end; ++j)
            cq[j] = rvq_float_to_q(cf[j]);
      }
      else
      {
         for (j=0; j < f->previous_length; ++j)
            pf[j] = pq[j] * (1.0f / (float)(1 << RVQ_QBITS));
         for (j=f->channel_buffer_start; j < f->channel_buffer_end; ++j)
            cf[j] = cq[j] * (1.0f / (float)(1 << RVQ_QBITS));
      }
   }
}

static int rvorbis_get_frame_float(rvorbis *f, int *channels, float ***output)
{
   int len, right,left,i;
   if (!vorbis_decode_packet(f, &len, &left, &right))
   {
      f->channel_buffer_start = f->channel_buffer_end = 0;
      return 0;
   }

   len = vorbis_finish_frame(f, len, left, right);
   for (i=0; i < f->channels; ++i)
      f->outputs[i] = f->channel_buffers[i] + left;

   f->channel_buffer_start = left;
   f->channel_buffer_end   = left+len;

   if (channels)
      *channels            = f->channels;
   if (output)
      *output              = f->outputs;
   return len;
}


/* --- ported from current stb_vorbis: correct coarse+linear seek --- */

void rvorbis_seek_start(rvorbis *f); /* forward decl (defined below) */
unsigned int rvorbis_stream_length_in_samples(rvorbis *f); /* forward decl */

/* Read a page header at the current offset into z, then restore offset. */
static int get_seek_page_info(rvorbis *f, ProbedPage *z)
{
   uint8_t header[27], lacing[255];
   int i, len;

   z->page_start = (unsigned int)(f->stream - f->stream_start);

   if (!getn(f, header, 27))
      return 0;
   if (header[0] != 'O' || header[1] != 'g' || header[2] != 'g' || header[3] != 'S')
      return 0;
   if (!getn(f, lacing, header[26]))
      return 0;

   len = 0;
   for (i = 0; i < header[26]; ++i)
      len += lacing[i];

   z->page_end = z->page_start + 27 + header[26] + len;

   z->last_decoded_sample = header[6] + (header[7] << 8)
                          + (header[8] << 16) + (header[9] << 24);

   set_file_offset(f, z->page_start);
   return 1;
}

/* Seek back to the page preceding limit_offset while locating a packet start. */
static int go_to_page_before(rvorbis *f, unsigned int limit_offset)
{
   unsigned int previous_safe;
   uint32_t end;

   if (limit_offset >= 65536 && limit_offset - 65536 >= f->first_audio_page_offset)
      previous_safe = limit_offset - 65536;
   else
      previous_safe = f->first_audio_page_offset;

   set_file_offset(f, previous_safe);

   while (vorbis_find_page(f, &end, NULL))
   {
      if (end >= limit_offset && (unsigned int)(f->stream - f->stream_start) < limit_offset)
         return 1;
      set_file_offset(f, end);
   }

   return 0;
}

/* vorbis_decode_initial without advancing (rewinds the bytes it read). */
static int peek_decode_initial(rvorbis *f, int *p_left_start, int *p_left_end,
      int *p_right_start, int *p_right_end, int *mode)
{
   int bits_read, bytes_read;

   if (!vorbis_decode_initial(f, p_left_start, p_left_end, p_right_start, p_right_end, mode))
      return 0;

   bits_read = 1 + ilog(f->mode_count - 1);
   if (f->mode_config[*mode].blockflag)
      bits_read += 2;
   bytes_read = (bits_read + 7) / 8;

   f->bytes_in_seg  += bytes_read;
   f->packet_bytes  -= bytes_read;
   skip(f, -bytes_read);
   if (f->next_seg == -1)
      f->next_seg = f->segment_count - 1;
   else
      f->next_seg--;
   f->valid_bits = 0;

   return 1;
}

/* Coarse page-level search: leaves current_loc_valid set and
 * current_loc <= sample_number. Mirrors stb_vorbis seek_to_sample_coarse. */
static int seek_to_sample_coarse(rvorbis *f, uint32_t sample_number)
{
   ProbedPage left, right, mid;
   int i, start_seg_with_known_loc, end_pos, page_start;
   uint32_t delta, stream_length, padding, last_sample_limit;
   double offset = 0.0, bytes_per_sample = 0.0;
   int probe = 0;

   stream_length = rvorbis_stream_length_in_samples(f);
   if (stream_length == 0)            return error(f, RVORBIS_cant_find_last_page);
   if (sample_number > stream_length) return error(f, RVORBIS_seek_invalid);

   padding = ((f->blocksize_1 - f->blocksize_0) >> 2);
   if (sample_number < padding)
      last_sample_limit = 0;
   else
      last_sample_limit = sample_number - padding;

   left = f->p_first;
   while (left.last_decoded_sample == ~0U)
   {
      set_file_offset(f, left.page_end);
      if (!get_seek_page_info(f, &left)) goto error;
   }

   right = f->p_last;
   assert(right.last_decoded_sample != ~0U);

   if (last_sample_limit <= left.last_decoded_sample)
   {
      rvorbis_seek_start(f);
      if (f->current_loc > sample_number)
         return error(f, RVORBIS_seek_failed);
      return 1;
   }

   while (left.page_end != right.page_start)
   {
      assert(left.page_end < right.page_start);
      delta = right.page_start - left.page_end;
      if (delta <= 65536)
      {
         set_file_offset(f, left.page_end);
      }
      else
      {
         if (probe < 2)
         {
            if (probe == 0)
            {
               double data_bytes = right.page_end - left.page_start;
               bytes_per_sample = data_bytes / right.last_decoded_sample;
               offset = left.page_start + bytes_per_sample * (last_sample_limit - left.last_decoded_sample);
            }
            else
            {
               double err = ((double) last_sample_limit - mid.last_decoded_sample) * bytes_per_sample;
               if (err >= 0 && err <  8000) err =  8000;
               if (err <  0 && err > -8000) err = -8000;
               offset += err * 2;
            }

            if (offset < left.page_end)
               offset = left.page_end;
            if (offset > right.page_start - 65536)
               offset = right.page_start - 65536;

            set_file_offset(f, (unsigned int) offset);
         }
         else
         {
            set_file_offset(f, left.page_end + (delta / 2) - 32768);
         }

         if (!vorbis_find_page(f, NULL, NULL)) goto error;
      }

      for (;;)
      {
         if (!get_seek_page_info(f, &mid)) goto error;
         if (mid.last_decoded_sample != ~0U) break;
         set_file_offset(f, mid.page_end);
         assert(mid.page_start < right.page_start);
      }

      if (mid.page_start == right.page_start)
      {
         if (probe >= 2 || delta <= 65536)
            break;
      }
      else
      {
         if (last_sample_limit < mid.last_decoded_sample)
            right = mid;
         else
            left = mid;
      }

      ++probe;
   }

   page_start = left.page_start;
   set_file_offset(f, page_start);
   if (!start_page(f)) return error(f, RVORBIS_seek_failed);
   end_pos = f->end_seg_with_known_loc;
   assert(end_pos >= 0);

   for (;;)
   {
      for (i = end_pos; i > 0; --i)
         if (f->segments[i-1] != 255)
            break;

      start_seg_with_known_loc = i;

      if (start_seg_with_known_loc > 0 || !(f->page_flag & PAGEFLAG_continued_packet))
         break;

      if (!go_to_page_before(f, page_start))
         goto error;

      page_start = (unsigned int)(f->stream - f->stream_start);
      if (!start_page(f)) goto error;
      end_pos = f->segment_count - 1;
   }

   f->current_loc_valid = 0;
   f->last_seg          = 0;
   f->valid_bits        = 0;
   f->packet_bytes      = 0;
   f->bytes_in_seg      = 0;
   f->previous_length   = 0;
   f->next_seg          = start_seg_with_known_loc;

   for (i = 0; i < start_seg_with_known_loc; i++)
      skip(f, f->segments[i]);

   vorbis_pump_first_frame(f);
   if (f->current_loc > sample_number)
      return error(f, RVORBIS_seek_failed);
   return 1;

error:
   rvorbis_seek_start(f);
   return error(f, RVORBIS_seek_failed);
}

unsigned int rvorbis_stream_length_in_samples(rvorbis *f)
{
   uint32_t end, last_page_loc;
   if (!f->total_samples)
   {
      uint32_t previous_safe;
      uint32_t last;
      uint32_t lo,hi;
      char header[6];
      /* First, store the current decode position so we can restore it */
      uint32_t restore_offset = (unsigned int)(f->stream - f->stream_start);

      /* Now we want to seek back 64K from the end (the last page must
       * be at most a little less than 64K, but let's allow a little slop) */
      if (f->stream_len >= 65536 && f->stream_len-65536 >= f->first_audio_page_offset)
         previous_safe = f->stream_len - 65536;
      else
         previous_safe = f->first_audio_page_offset;

      set_file_offset(f, previous_safe);
      /* previous_safe is now our candidate 'earliest known place that seeking
       * to will lead to the final page' */

      if (!vorbis_find_page(f, &end, &last))
      {
         /* if we can't find a page, we're hosed! */
         f->error = RVORBIS_cant_find_last_page;
         f->total_samples = 0xffffffff;
         goto done;
      }

      /* check if there are more pages */
      last_page_loc = (unsigned int)(f->stream - f->stream_start);

      /* stop when the last_page flag is set, not when we reach eof;
       * this allows us to stop short of a 'file_section' end without
       * explicitly checking the length of the section */
      while (!last)
      {
         set_file_offset(f, end);
         /* The last page we found didn't have the 'last page' flag
          * set. whoops! */
         if (!vorbis_find_page(f, &end, &last))
            break;
         previous_safe = last_page_loc+1;
         last_page_loc = (unsigned int)(f->stream - f->stream_start);
      }

      set_file_offset(f, last_page_loc);

      /* parse the header */
      getn(f, (unsigned char *)header, 6);
      /* extract the absolute granule position */
      lo = get32(f);
      hi = get32(f);
      if (lo == 0xffffffff && hi == 0xffffffff)
      {
         f->error = RVORBIS_cant_find_last_page;
         f->total_samples = SAMPLE_unknown;
         goto done;
      }
      if (hi)
         lo = 0xfffffffe; /* saturate */
      f->total_samples                    = lo;

      f->p_last.page_start                = last_page_loc;
      f->p_last.page_end                  = end;
      f->p_last.last_decoded_sample       = lo;
      f->p_last.first_decoded_sample      = SAMPLE_unknown;
      f->p_last.after_previous_page_start = previous_safe;

done:
      set_file_offset(f, restore_offset);
   }
   if (f->total_samples != SAMPLE_unknown)
      return f->total_samples;
   return 0;
}

/* seek to the frame containing sample_number; leaves current_loc <= it. */
static int rvorbis_seek_frame(rvorbis *f, unsigned int sample_number)
{
   uint32_t max_frame_samples;

   if (!seek_to_sample_coarse(f, sample_number))
      return 0;

   assert(f->current_loc_valid);
   assert(f->current_loc <= sample_number);

   /* linear search for the relevant packet */
   max_frame_samples = (f->blocksize_1*3 - f->blocksize_0) >> 2;
   while (f->current_loc < sample_number)
   {
      int left_start, left_end, right_start, right_end, mode, frame_samples;
      if (!peek_decode_initial(f, &left_start, &left_end, &right_start, &right_end, &mode))
         return error(f, RVORBIS_seek_failed);
      frame_samples = right_start - left_start;
      if (f->current_loc + frame_samples > sample_number)
      {
         return 1; /* the next frame will contain the sample */
      }
      else if (f->current_loc + frame_samples + max_frame_samples > sample_number)
      {
         /* there's a chance the frame after this could contain the sample */
         vorbis_pump_first_frame(f);
      }
      else
      {
         /* this frame is too early to be relevant */
         f->current_loc += frame_samples;
         f->previous_length = 0;
         maybe_start_packet(f);
         while (get8_packet_raw(f) != EOP)
            ;
      }
   }
   if (f->current_loc != sample_number) return error(f, RVORBIS_seek_failed);
   return 1;
}

int rvorbis_seek(rvorbis *f, unsigned int sample_number)
{
   /* clamp to valid range using the known last-page granule */
   if (f->p_last.page_start == 0)
   {
      if (rvorbis_stream_length_in_samples(f) == 0)
         return error(f, RVORBIS_cant_find_last_page);
   }
   if (f->p_last.last_decoded_sample != ~0U
         && sample_number >= f->p_last.last_decoded_sample)
      sample_number = f->p_last.last_decoded_sample - 1;

   if (!rvorbis_seek_frame(f, sample_number))
      return 0;

   if (sample_number != f->current_loc)
   {
      int n;
      uint32_t frame_start = f->current_loc;
      rvorbis_get_frame_float(f, &n, NULL);
      assert(sample_number > frame_start);
      assert(f->channel_buffer_start + (int)(sample_number - frame_start) <= f->channel_buffer_end);
      f->channel_buffer_start += (sample_number - frame_start);
   }

   return 1;
}

void rvorbis_seek_start(rvorbis *f)
{
   set_file_offset(f, f->first_audio_page_offset);
   f->previous_length = 0;
   f->first_decode    = 1;
   f->next_seg        = -1;
   vorbis_pump_first_frame(f);
}

rvorbis * rvorbis_open_memory(const unsigned char *data, int len, int *error, rvorbis_alloc *alloc)
{
   rvorbis *f, p;
   if (!data)
      return NULL;
   vorbis_init(&p, alloc);
   p.stream       = (uint8_t *) data;
   p.stream_end   = (uint8_t *) data + len;
   p.stream_start = (uint8_t *) p.stream;
   p.stream_len   = len;
   p.push_mode    = 0;
   if (start_decoder(&p))
   {
      f = vorbis_alloc(&p);
      if (f)
      {
         *f = p;
         vorbis_pump_first_frame(f);
         return f;
      }
   }
   if (error) *error = p.error;
   vorbis_deinit(&p);
   return NULL;
}

int rvorbis_get_samples_float_interleaved(rvorbis *f, int channels,
      float *buffer, int num_floats)
{
   float **outputs;
   int len = num_floats / channels;
   int n=0;
   int z = f->channels;

   rvorbis_set_output_mode(f, 0);
   if (z > channels)
      z = channels;
   while (n < len)
   {
      int i,j;
      int k = f->channel_buffer_end - f->channel_buffer_start;
      if (n+k >= len) k = len - n;
      for (j=0; j < k; ++j)
      {
         for (i=0; i < z; ++i)
            *buffer++ = f->channel_buffers[i][f->channel_buffer_start+j];
         for (   ; i < channels; ++i)
            *buffer++ = 0;
      }
      n += k;
      f->channel_buffer_start += k;
      if (n == len)
         break;
      if (!rvorbis_get_frame_float(f, NULL, &outputs))
         break;
   }
   return n;
}

/* Native signed 16-bit output: identical to the float read except that
 * quantisation (round half away from zero, clamped) happens during the
 * interleave copy, so the samples go from the per-channel decode
 * buffers to the caller's buffer in a single pass with no intermediate
 * float staging.  Vorbis is a float-internal codec, so one
 * quantisation at the output boundary is the minimum possible. */
int rvorbis_get_samples_s16_interleaved(rvorbis *f, int channels,
      int16_t *buffer, int num_shorts)
{
   float **outputs;
   int len = num_shorts / channels;
   int n=0;
   int z = f->channels;

   rvorbis_set_output_mode(f, 1);
   if (z > channels)
      z = channels;
   while (n < len)
   {
      int i,j;
      int k = f->channel_buffer_end - f->channel_buffer_start;
      if (n+k >= len) k = len - n;
      for (j=0; j < k; ++j)
      {
         /* Q28 buffers: s16 = round(x * 32768) = round(v / 2^13),
          * rounded half away from zero and clamped as the float
          * quantiser below. */
         for (i=0; i < z; ++i)
         {
            int32_t v = ((int32_t *) f->channel_buffers[i])[f->channel_buffer_start+j];
            int32_t q = (v >= 0) ? ((v + (1 << 12)) >> 13)
                                 : -((-v + (1 << 12)) >> 13);
            if (q >  32767)
               q =  32767;
            if (q < -32768)
               q = -32768;
            *buffer++ = (int16_t)q;
         }

         for (   ; i < channels; ++i)
            *buffer++ = 0;
      }
      n += k;
      f->channel_buffer_start += k;
      if (n == len)
         break;
      if (!rvorbis_get_frame_float(f, NULL, &outputs))
         break;
   }
   return n;
}
