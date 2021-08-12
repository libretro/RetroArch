/* trees.c -- output deflated data using Huffman coding
 * Copyright (C) 1995-2012 Jean-loup Gailly
 * detect_data_type() function provided freely by Cosmin Truta, 2006
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/*
 *  ALGORITHM
 *
 *      The "deflation" process uses several Huffman trees. The more
 *      common source values are represented by shorter bit sequences.
 *
 *      Each code tree is stored in a compressed form which is itself
 * a Huffman encoding of the lengths of all the code strings (in
 * ascending order by source values).  The actual code strings are
 * reconstructed from the lengths in the inflate process, as described
 * in the deflate specification.
 *
 *  REFERENCES
 *
 *      Deutsch, L.P.,"'Deflate' Compressed Data Format Specification".
 *      Available in ftp.uu.net:/pub/archiving/zip/doc/deflate-1.1.doc
 *
 *      Storer, James A.
 *          Data Compression:  Methods and Theory, pp. 49-50.
 *          Computer Science Press, 1988.  ISBN 0-7167-8156-5.
 *
 *      Sedgewick, R.
 *          Algorithms, p290.
 *          Addison-Wesley, 1983. ISBN 0-201-06672-6.
 */

/* @(#) $Id$ */

#include "deflate.h"

/* ===========================================================================
 * Constants
 */

#define MAX_BL_BITS 7
/* Bit length codes must not exceed MAX_BL_BITS bits */

#define END_BLOCK 256
/* end of block literal code */

#define REP_3_6      16
/* repeat previous bit length 3-6 times (2 bits of repeat count) */

#define REPZ_3_10    17
/* repeat a zero length 3-10 times  (3 bits of repeat count) */

#define REPZ_11_138  18
/* repeat a zero length 11-138 times  (7 bits of repeat count) */

static const int extra_lbits[LENGTH_CODES] /* extra bits for each length code */
= {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};

static const int extra_dbits[D_CODES] /* extra bits for each distance code */
= {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

static const int extra_blbits[BL_CODES]/* extra bits for each bit length code */
= {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,3,7};

static const uch bl_order[BL_CODES]
= {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
/* The lengths of the bit length codes are sent in order of decreasing
 * probability, to avoid transmitting the lengths for unused bit length codes.
 */

/* ===========================================================================
 * Local data. These are initialized only once.
 */

#define DIST_CODE_LEN  512 /* see definition of array dist_code below */

#include "trees.h"

struct static_tree_desc_s {
   const ct_data *static_tree;  /* static tree or NULL */
   const intf *extra_bits;      /* extra bits for each code or NULL */
   int     extra_base;          /* base index for extra_bits */
   int     elems;               /* max number of elements in the tree */
   int     max_length;          /* max bit length for the codes */
};

static static_tree_desc  static_l_desc =
{static_ltree, extra_lbits, LITERALS+1, L_CODES, MAX_BITS};

static static_tree_desc  static_d_desc =
{static_dtree, extra_dbits, 0,          D_CODES, MAX_BITS};

static static_tree_desc  static_bl_desc =
{(const ct_data *)0, extra_blbits, 0,   BL_CODES, MAX_BL_BITS};

/* ===========================================================================
 * Local (static) routines in this file.
 */

static void compress_block (deflate_state *s, const ct_data *ltree,
         const ct_data *dtree);
static int  detect_data_type (deflate_state *s);
static unsigned bi_reverse (unsigned value, int length);
static void bi_windup      (deflate_state *s);
static void bi_flush       (deflate_state *s);

#  define send_code(s, c, tree) send_bits(s, tree[c].Code, tree[c].Len)
/* Send a code of the given tree. c and tree must not have side effects */

/* ===========================================================================
 * Output a short LSB first on the stream.
 * IN assertion: there is enough room in pendingBuf.
 */
#define put_short(s, w) { \
   put_byte(s, (uch)((w) & 0xff)); \
   put_byte(s, (uch)((ush)(w) >> 8)); \
}

/* ===========================================================================
 * Send a value on a given number of bits.
 * IN assertion: length <= 16 and value fits in length bits.
 */

#define send_bits(s, value, length) \
{ int len = length;\
   if (s->bi_valid > (int)Buf_size - len) {\
      int val = value;\
      s->bi_buf |= (ush)val << s->bi_valid;\
      put_short(s, s->bi_buf);\
      s->bi_buf = (ush)val >> (Buf_size - s->bi_valid);\
      s->bi_valid += len - Buf_size;\
   } else {\
      s->bi_buf |= (ush)(value) << s->bi_valid;\
      s->bi_valid += len;\
   }\
}

/* ===========================================================================
 * Initialize a new block.
 */
static void init_block(deflate_state *s)
{
   int n; /* iterates over tree elements */

   /* Initialize the trees. */
   for (n = 0; n < L_CODES;  n++) s->dyn_ltree[n].Freq = 0;
   for (n = 0; n < D_CODES;  n++) s->dyn_dtree[n].Freq = 0;
   for (n = 0; n < BL_CODES; n++) s->bl_tree[n].Freq = 0;

   s->dyn_ltree[END_BLOCK].Freq = 1;
   s->opt_len = s->static_len = 0L;
   s->last_lit = s->matches = 0;
}


/* the arguments must not have side effects */

/* ===========================================================================
 * Initialize the tree data structures for a new zlib stream.
 */
void ZLIB_INTERNAL _tr_init(deflate_state *s)
{
   s->l_desc.dyn_tree = s->dyn_ltree;
   s->l_desc.stat_desc = &static_l_desc;

   s->d_desc.dyn_tree = s->dyn_dtree;
   s->d_desc.stat_desc = &static_d_desc;

   s->bl_desc.dyn_tree = s->bl_tree;
   s->bl_desc.stat_desc = &static_bl_desc;

   s->bi_buf = 0;
   s->bi_valid = 0;

   /* Initialize the first block of the first file: */
   init_block(s);
}

#define SMALLEST 1
/* Index within the heap array of least frequent node in the Huffman tree */


/* ===========================================================================
 * Remove the smallest element from the heap and recreate the heap with
 * one less element. Updates heap and heap_len.
 */
#define pqremove(s, tree, top) \
{\
   top = s->heap[SMALLEST]; \
   s->heap[SMALLEST] = s->heap[s->heap_len--]; \
   pqdownheap(s, tree, SMALLEST); \
}

/* ===========================================================================
 * Compares to subtrees, using the tree depth as tie breaker when
 * the subtrees have equal frequency. This minimizes the worst case length.
 */
#define smaller(tree, n, m, depth) \
   (tree[n].Freq < tree[m].Freq || \
    (tree[n].Freq == tree[m].Freq && depth[n] <= depth[m]))

/* ===========================================================================
 * Restore the heap property by moving down the tree starting at node k,
 * exchanging a node with the smallest of its two sons if necessary, stopping
 * when the heap property is re-established (each father smaller than its
 * two sons).
 */
static void pqdownheap(deflate_state *s, ct_data *tree, int k)
{
   int v = s->heap[k];
   int j = k << 1;  /* left son of k */
   while (j <= s->heap_len) {
      /* Set j to the smallest of the two sons: */
      if (j < s->heap_len &&
            smaller(tree, s->heap[j+1], s->heap[j], s->depth)) {
         j++;
      }
      /* Exit if v is smaller than both sons */
      if (smaller(tree, v, s->heap[j], s->depth)) break;

      /* Exchange v with the smallest son */
      s->heap[k] = s->heap[j];  k = j;

      /* And continue down the tree, setting j to the left son of k */
      j <<= 1;
   }
   s->heap[k] = v;
}

/* ===========================================================================
 * Compute the optimal bit lengths for a tree and update the total bit length
 * for the current block.
 * IN assertion: the fields freq and dad are set, heap[heap_max] and
 *    above are the tree nodes sorted by increasing frequency.
 * OUT assertions: the field len is set to the optimal bit length, the
 *     array bl_count contains the frequencies for each bit length.
 *     The length opt_len is updated; static_len is also updated if stree is
 *     not null.
 */
static void gen_bitlen(deflate_state *s, tree_desc *desc)
{
   ct_data *tree        = desc->dyn_tree;
   int max_code         = desc->max_code;
   const ct_data *stree = desc->stat_desc->static_tree;
   const intf *extra    = desc->stat_desc->extra_bits;
   int base             = desc->stat_desc->extra_base;
   int max_length       = desc->stat_desc->max_length;
   int h;              /* heap index */
   int n, m;           /* iterate over the tree elements */
   int bits;           /* bit length */
   int xbits;          /* extra bits */
   ush f;              /* frequency */
   int overflow = 0;   /* number of elements with bit length too large */

   for (bits = 0; bits <= MAX_BITS; bits++) s->bl_count[bits] = 0;

   /* In a first pass, compute the optimal bit lengths (which may
    * overflow in the case of the bit length tree).
    */
   tree[s->heap[s->heap_max]].Len = 0; /* root of the heap */

   for (h = s->heap_max+1; h < HEAP_SIZE; h++) {
      n = s->heap[h];
      bits = tree[tree[n].Dad].Len + 1;
      if (bits > max_length)
      {
         bits = max_length;
         overflow++;
      }
      tree[n].Len = (ush)bits;
      /* We overwrite tree[n].Dad which is no longer needed */

      if (n > max_code) continue; /* not a leaf node */

      s->bl_count[bits]++;
      xbits = 0;
      if (n >= base) xbits = extra[n-base];
      f = tree[n].Freq;
      s->opt_len += (ulg)f * (bits + xbits);
      if (stree) s->static_len += (ulg)f * (stree[n].Len + xbits);
   }
   if (overflow == 0) return;

   Trace((stderr,"\nbit length overflow\n"));
   /* This happens for example on obj2 and pic of the Calgary corpus */

   /* Find the first bit length which could increase: */
   do {
      bits = max_length-1;
      while (s->bl_count[bits] == 0) bits--;
      s->bl_count[bits]--;      /* move one leaf down the tree */
      s->bl_count[bits+1] += 2; /* move one overflow item as its brother */
      s->bl_count[max_length]--;
      /* The brother of the overflow item also moves one step up,
       * but this does not affect bl_count[max_length]
       */
      overflow -= 2;
   } while (overflow > 0);

   /* Now recompute all bit lengths, scanning in increasing frequency.
    * h is still equal to HEAP_SIZE. (It is simpler to reconstruct all
    * lengths instead of fixing only the wrong ones. This idea is taken
    * from 'ar' written by Haruhiko Okumura.)
    */
   for (bits = max_length; bits != 0; bits--) {
      n = s->bl_count[bits];
      while (n != 0) {
         m = s->heap[--h];
         if (m > max_code) continue;
         if ((unsigned) tree[m].Len != (unsigned) bits) {
            Trace((stderr,"code %d bits %d->%d\n", m, tree[m].Len, bits));
            s->opt_len += ((long)bits - (long)tree[m].Len)
               *(long)tree[m].Freq;
            tree[m].Len = (ush)bits;
         }
         n--;
      }
   }
}

/* ===========================================================================
 * Generate the codes for a given tree and bit counts (which need not be
 * optimal).
 * IN assertion: the array bl_count contains the bit length statistics for
 * the given tree and the field len is set for all tree elements.
 * OUT assertion: the field code is set for all tree elements of non
 *     zero code length.
 */
static void gen_codes (ct_data *tree, int max_code, ushf *bl_count)
{
   ush next_code[MAX_BITS+1]; /* next code value for each bit length */
   ush codes = 0;              /* running code value */
   int bits;                  /* bit index */
   int n;                     /* code index */

   /* The distribution counts are first used to generate the code values
    * without bit reversal.
    */
   for (bits = 1; bits <= MAX_BITS; bits++) {
      next_code[bits] = codes = (codes + bl_count[bits-1]) << 1;
   }
   /* Check that the bit counts in bl_count are consistent. The last code
    * must be all ones.
    */
   Assert (codes + bl_count[MAX_BITS]-1 == (1<<MAX_BITS)-1,
         "inconsistent bit counts");

   for (n = 0;  n <= max_code; n++) {
      int len = tree[n].Len;
      if (len == 0) continue;
      /* Now reverse the bits */
      tree[n].Code = bi_reverse(next_code[len]++, len);

      Tracecv(tree != static_ltree, (stderr,"\nn %3d %c l %2d c %4x (%x) ",
               n, (isgraph(n) ? n : ' '), len, tree[n].Code, next_code[len]-1));
   }
}

/* ===========================================================================
 * Construct one Huffman tree and assigns the code bit strings and lengths.
 * Update the total bit length for the current block.
 * IN assertion: the field freq is set for all tree elements.
 * OUT assertions: the fields len and code are set to the optimal bit length
 *     and corresponding code. The length opt_len is updated; static_len is
 *     also updated if stree is not null. The field max_code is set.
 */
static void build_tree(deflate_state *s, tree_desc *desc)
{
   ct_data *tree         = desc->dyn_tree;
   const ct_data *stree  = desc->stat_desc->static_tree;
   int elems             = desc->stat_desc->elems;
   int n, m;          /* iterate over heap elements */
   int max_code = -1; /* largest code with non zero frequency */
   int node;          /* new node being created */

   /* Construct the initial heap, with least frequent element in
    * heap[SMALLEST]. The sons of heap[n] are heap[2*n] and heap[2*n+1].
    * heap[0] is not used.
    */
   s->heap_len = 0;
   s->heap_max = HEAP_SIZE;

   for (n = 0; n < elems; n++) {
      if (tree[n].Freq != 0) {
         s->heap[++(s->heap_len)] = max_code = n;
         s->depth[n] = 0;
      } else {
         tree[n].Len = 0;
      }
   }

   /* The pkzip format requires that at least one distance code exists,
    * and that at least one bit should be sent even if there is only one
    * possible code. So to avoid special checks later on we force at least
    * two codes of non zero frequency.
    */
   while (s->heap_len < 2) {
      node = s->heap[++(s->heap_len)] = (max_code < 2 ? ++max_code : 0);
      tree[node].Freq = 1;
      s->depth[node] = 0;
      s->opt_len--; if (stree) s->static_len -= stree[node].Len;
      /* node is 0 or 1 so it does not have extra bits */
   }
   desc->max_code = max_code;

   /* The elements heap[heap_len/2+1 .. heap_len] are leaves of the tree,
    * establish sub-heaps of increasing lengths:
    */
   for (n = s->heap_len/2; n >= 1; n--) pqdownheap(s, tree, n);

   /* Construct the Huffman tree by repeatedly combining the least two
    * frequent nodes.
    */
   node = elems;              /* next internal node of the tree */
   do {
      pqremove(s, tree, n);  /* n = node of least frequency */
      m = s->heap[SMALLEST]; /* m = node of next least frequency */

      s->heap[--(s->heap_max)] = n; /* keep the nodes sorted by frequency */
      s->heap[--(s->heap_max)] = m;

      /* Create a new node father of n and m */
      tree[node].Freq = tree[n].Freq + tree[m].Freq;
      s->depth[node] = (uch)((s->depth[n] >= s->depth[m] ?
               s->depth[n] : s->depth[m]) + 1);
      tree[n].Dad = tree[m].Dad = (ush)node;
#ifdef DUMP_BL_TREE
      if (tree == s->bl_tree) {
         fprintf(stderr,"\nnode %d(%d), sons %d(%d) %d(%d)",
               node, tree[node].Freq, n, tree[n].Freq, m, tree[m].Freq);
      }
#endif
      /* and insert the new node in the heap */
      s->heap[SMALLEST] = node++;
      pqdownheap(s, tree, SMALLEST);

   } while (s->heap_len >= 2);

   s->heap[--(s->heap_max)] = s->heap[SMALLEST];

   /* At this point, the fields freq and dad are set. We can now
    * generate the bit lengths.
    */
   gen_bitlen(s, (tree_desc *)desc);

   /* The field len is now set, we can generate the bit codes */
   gen_codes ((ct_data *)tree, max_code, s->bl_count);
}

/* ===========================================================================
 * Scan a literal or distance tree to determine the frequencies of the codes
 * in the bit length tree.
 */
static void scan_tree (deflate_state *s, ct_data *tree, int max_code)
{
   int n;                     /* iterates over all tree elements */
   int prevlen = -1;          /* last emitted length */
   int curlen;                /* length of current code */
   int nextlen = tree[0].Len; /* length of next code */
   int count = 0;             /* repeat count of the current code */
   int max_count = 7;         /* max repeat count */
   int min_count = 4;         /* min repeat count */

   if (nextlen == 0)
   {
      max_count = 138;
      min_count = 3;
   }
   tree[max_code+1].Len = (ush)0xffff; /* guard */

   for (n = 0; n <= max_code; n++) {
      curlen = nextlen; nextlen = tree[n+1].Len;
      if (++count < max_count && curlen == nextlen) {
         continue;
      } else if (count < min_count) {
         s->bl_tree[curlen].Freq += count;
      } else if (curlen != 0) {
         if (curlen != prevlen) s->bl_tree[curlen].Freq++;
         s->bl_tree[REP_3_6].Freq++;
      } else if (count <= 10) {
         s->bl_tree[REPZ_3_10].Freq++;
      } else {
         s->bl_tree[REPZ_11_138].Freq++;
      }
      count = 0; prevlen = curlen;
      if (nextlen == 0) {
         max_count = 138;
         min_count = 3;
      } else if (curlen == nextlen) {
         max_count = 6;
         min_count = 3;
      } else {
         max_count = 7;
         min_count = 4;
      }
   }
}

/* ===========================================================================
 * Send a literal or distance tree in compressed form, using the codes in
 * bl_tree.
 */
static void send_tree (deflate_state *s, ct_data *tree, int max_code)
{
   int n;                     /* iterates over all tree elements */
   int prevlen = -1;          /* last emitted length */
   int curlen;                /* length of current code */
   int nextlen = tree[0].Len; /* length of next code */
   int count = 0;             /* repeat count of the current code */
   int max_count = 7;         /* max repeat count */
   int min_count = 4;         /* min repeat count */

   /* tree[max_code+1].Len = -1; */  /* guard already set */
   if (nextlen == 0)
   {
      max_count = 138;
      min_count = 3;
   }

   for (n = 0; n <= max_code; n++) {
      curlen = nextlen; nextlen = tree[n+1].Len;
      if (++count < max_count && curlen == nextlen) {
         continue;
      } else if (count < min_count) {
         do { send_code(s, curlen, s->bl_tree); } while (--count != 0);

      } else if (curlen != 0) {
         if (curlen != prevlen) {
            send_code(s, curlen, s->bl_tree); count--;
         }
         Assert(count >= 3 && count <= 6, " 3_6?");
         send_code(s, REP_3_6, s->bl_tree); send_bits(s, count-3, 2);

      } else if (count <= 10) {
         send_code(s, REPZ_3_10, s->bl_tree); send_bits(s, count-3, 3);

      } else {
         send_code(s, REPZ_11_138, s->bl_tree); send_bits(s, count-11, 7);
      }
      count = 0; prevlen = curlen;
      if (nextlen == 0) {
         max_count = 138;
         min_count = 3;
      } else if (curlen == nextlen) {
         max_count = 6;
         min_count = 3;
      } else {
         max_count = 7;
         min_count = 4;
      }
   }
}

/* ===========================================================================
 * Construct the Huffman tree for the bit lengths and return the index in
 * bl_order of the last bit length code to send.
 */
static int build_bl_tree(deflate_state *s)
{
   int max_blindex;  /* index of last bit length code of non zero freq */

   /* Determine the bit length frequencies for literal and distance trees */
   scan_tree(s, (ct_data *)s->dyn_ltree, s->l_desc.max_code);
   scan_tree(s, (ct_data *)s->dyn_dtree, s->d_desc.max_code);

   /* Build the bit length tree: */
   build_tree(s, (tree_desc *)(&(s->bl_desc)));
   /* opt_len now includes the length of the tree representations, except
    * the lengths of the bit lengths codes and the 5+5+4 bits for the counts.
    */

   /* Determine the number of bit length codes to send. The pkzip format
    * requires that at least 4 bit length codes be sent. (appnote.txt says
    * 3 but the actual value used is 4.)
    */
   for (max_blindex = BL_CODES-1; max_blindex >= 3; max_blindex--) {
      if (s->bl_tree[bl_order[max_blindex]].Len != 0) break;
   }
   /* Update opt_len to include the bit length tree and counts */
   s->opt_len += 3*(max_blindex+1) + 5+5+4;

   return max_blindex;
}

/* ===========================================================================
 * Send the header for a block using dynamic Huffman trees: the counts, the
 * lengths of the bit length codes, the literal tree and the distance tree.
 * IN assertion: lcodes >= 257, dcodes >= 1, blcodes >= 4.
 */
static void send_all_trees(deflate_state *s, int lcodes, int dcodes, int blcodes)
{
   int rank;                    /* index in bl_order */

   Assert (lcodes >= 257 && dcodes >= 1 && blcodes >= 4, "not enough codes");
   Assert (lcodes <= L_CODES && dcodes <= D_CODES && blcodes <= BL_CODES,
         "too many codes");
   send_bits(s, lcodes-257, 5); /* not +255 as stated in appnote.txt */
   send_bits(s, dcodes-1,   5);
   send_bits(s, blcodes-4,  4); /* not -3 as stated in appnote.txt */
   for (rank = 0; rank < blcodes; rank++) {
      send_bits(s, s->bl_tree[bl_order[rank]].Len, 3);
   }

   send_tree(s, (ct_data *)s->dyn_ltree, lcodes-1); /* literal tree */

   send_tree(s, (ct_data *)s->dyn_dtree, dcodes-1); /* distance tree */
}

/* ===========================================================================
 * Send a stored block
 */
void ZLIB_INTERNAL _tr_stored_block(deflate_state *s, charf *buf, ulg stored_len, int last)
{
   send_bits(s, (STORED_BLOCK<<1)+last, 3);    /* send block type */
   bi_windup(s);        /* align on byte boundary */
   put_short(s, (ush)stored_len);
   put_short(s, (ush)~stored_len);
   memcpy(s->pending_buf + s->pending, buf, stored_len);
   s->pending += stored_len;
}

/* ===========================================================================
 * Flush the bits in the bit buffer to pending output (leaves at most 7 bits)
 */
void ZLIB_INTERNAL _tr_flush_bits(deflate_state *s)
{
   bi_flush(s);
}

/* ===========================================================================
 * Send one empty static block to give enough lookahead for inflate.
 * This takes 10 bits, of which 7 may remain in the bit buffer.
 */
void ZLIB_INTERNAL _tr_align(deflate_state *s)
{
   send_bits(s, STATIC_TREES<<1, 3);
   send_code(s, END_BLOCK, static_ltree);
   bi_flush(s);
}

/* ===========================================================================
 * Determine the best encoding for the current block: dynamic trees, static
 * trees or store, and output the encoded block to the zip file.
 */
void ZLIB_INTERNAL _tr_flush_block(deflate_state *s, charf *buf, ulg stored_len, int last)
{
   ulg opt_lenb, static_lenb; /* opt_len and static_len in bytes */
   int max_blindex = 0;  /* index of last bit length code of non zero freq */

   /* Build the Huffman trees unless a stored block is forced */
   if (s->level > 0) {

      /* Check if the file is binary or text */
      if (s->strm->data_type == Z_UNKNOWN)
         s->strm->data_type = detect_data_type(s);

      /* Construct the literal and distance trees */
      build_tree(s, (tree_desc *)(&(s->l_desc)));

      build_tree(s, (tree_desc *)(&(s->d_desc)));
      /* At this point, opt_len and static_len are the total bit lengths of
       * the compressed block data, excluding the tree representations.
       */

      /* Build the bit length tree for the above two trees, and get the index
       * in bl_order of the last bit length code to send.
       */
      max_blindex = build_bl_tree(s);

      /* Determine the best encoding. Compute the block lengths in bytes. */
      opt_lenb = (s->opt_len+3+7)>>3;
      static_lenb = (s->static_len+3+7)>>3;

      if (static_lenb <= opt_lenb) opt_lenb = static_lenb;

   } else {
      Assert(buf != (char*)0, "lost buf");
      opt_lenb = static_lenb = stored_len + 5; /* force a stored block */
   }

#ifdef FORCE_STORED
   if (buf != (char*)0) { /* force stored block */
#else
      if (stored_len+4 <= opt_lenb && buf != (char*)0) {
         /* 4: two words for the lengths */
#endif
         /* The test buf != NULL is only necessary if LIT_BUFSIZE > WSIZE.
          * Otherwise we can't have processed more than WSIZE input bytes since
          * the last block flush, because compression would have been
          * successful. If LIT_BUFSIZE <= WSIZE, it is never too late to
          * transform a block into a stored block.
          */
         _tr_stored_block(s, buf, stored_len, last);

#ifdef FORCE_STATIC
      } else if (static_lenb >= 0) { /* force static trees */
#else
      } else if (s->strategy == Z_FIXED || static_lenb == opt_lenb) {
#endif
         send_bits(s, (STATIC_TREES<<1)+last, 3);
         compress_block(s, (const ct_data *)static_ltree,
               (const ct_data *)static_dtree);
      } else {
         send_bits(s, (DYN_TREES<<1)+last, 3);
         send_all_trees(s, s->l_desc.max_code+1, s->d_desc.max_code+1,
               max_blindex+1);
         compress_block(s, (const ct_data *)s->dyn_ltree,
               (const ct_data *)s->dyn_dtree);
      }
      Assert (s->compressed_len == s->bits_sent, "bad compressed size");
      /* The above check is made mod 2^32, for files larger than 512 MB
       * and uLong implemented on 32 bits.
       */
      init_block(s);

      if (last) {
         bi_windup(s);
      }
   }

   /* ===========================================================================
    * Save the match info and tally the frequency counts. Return true if
    * the current block must be flushed.
    */
   int ZLIB_INTERNAL _tr_tally (deflate_state *s, unsigned dist, unsigned lc)
   {
      s->d_buf[s->last_lit] = (ush)dist;
      s->l_buf[s->last_lit++] = (uch)lc;
      if (dist == 0) {
         /* lc is the unmatched char */
         s->dyn_ltree[lc].Freq++;
      } else {
         s->matches++;
         /* Here, lc is the match length - MIN_MATCH */
         dist--;             /* dist = match distance - 1 */
         Assert((ush)dist < (ush)MAX_DIST(s) &&
               (ush)lc <= (ush)(MAX_MATCH-MIN_MATCH) &&
               (ush)d_code(dist) < (ush)D_CODES,  "_tr_tally: bad match");

         s->dyn_ltree[_length_code[lc]+LITERALS+1].Freq++;
         s->dyn_dtree[d_code(dist)].Freq++;
      }

#ifdef TRUNCATE_BLOCK
      /* Try to guess if it is profitable to stop the current block here */
      if ((s->last_lit & 0x1fff) == 0 && s->level > 2) {
         /* Compute an upper bound for the compressed length */
         ulg out_length = (ulg)s->last_lit*8L;
         ulg in_length = (ulg)((long)s->strstart - s->block_start);
         int dcode;
         for (dcode = 0; dcode < D_CODES; dcode++) {
            out_length += (ulg)s->dyn_dtree[dcode].Freq *
               (5L+extra_dbits[dcode]);
         }
         out_length >>= 3;
         if (s->matches < s->last_lit/2 && out_length < in_length/2) return 1;
      }
#endif
      return (s->last_lit == s->lit_bufsize-1);
      /* We avoid equality with lit_bufsize because of wraparound at 64K
       * on 16 bit machines and because stored blocks are restricted to
       * 64K-1 bytes.
       */
   }

   /* ===========================================================================
    * Send the block data compressed using the given Huffman trees
    */
   static void compress_block(deflate_state *s, const ct_data *ltree, const ct_data *dtree)
   {
      unsigned dist;      /* distance of matched string */
      int lc;             /* match length or unmatched char (if dist == 0) */
      unsigned lx = 0;    /* running index in l_buf */
      unsigned codes;      /* the code to send */
      int extra;          /* number of extra bits to send */

      if (s->last_lit != 0) do {
         dist = s->d_buf[lx];
         lc = s->l_buf[lx++];
         if (dist == 0) {
            send_code(s, lc, ltree); /* send a literal byte */
            Tracecv(isgraph(lc), (stderr," '%c' ", lc));
         } else {
            /* Here, lc is the match length - MIN_MATCH */
            codes = _length_code[lc];
            send_code(s, codes + LITERALS+1, ltree); /* send the length code */
            extra = extra_lbits[codes];
            if (extra != 0) {
               lc -= base_length[codes];
               send_bits(s, lc, extra);       /* send the extra length bits */
            }
            dist--; /* dist is now the match distance - 1 */
            codes = d_code(dist);
            Assert (codes < D_CODES, "bad d_code");

            send_code(s, codes, dtree);       /* send the distance code */
            extra = extra_dbits[codes];
            if (extra != 0) {
               dist -= base_dist[codes];
               send_bits(s, dist, extra);   /* send the extra distance bits */
            }
         } /* literal or match pair ? */

         /* Check that the overlay between pending_buf and d_buf+l_buf is ok: */
         Assert((uInt)(s->pending) < s->lit_bufsize + 2*lx,
               "pendingBuf overflow");

      } while (lx < s->last_lit);

      send_code(s, END_BLOCK, ltree);
   }

   /* ===========================================================================
    * Check if the data type is TEXT or BINARY, using the following algorithm:
    * - TEXT if the two conditions below are satisfied:
    *    a) There are no non-portable control characters belonging to the
    *       "black list" (0..6, 14..25, 28..31).
    *    b) There is at least one printable character belonging to the
    *       "white list" (9 {TAB}, 10 {LF}, 13 {CR}, 32..255).
    * - BINARY otherwise.
    * - The following partially-portable control characters form a
    *   "gray list" that is ignored in this detection algorithm:
    *   (7 {BEL}, 8 {BS}, 11 {VT}, 12 {FF}, 26 {SUB}, 27 {ESC}).
    * IN assertion: the fields Freq of dyn_ltree are set.
    */
   static int detect_data_type(deflate_state *s)
   {
      /* black_mask is the bit mask of black-listed bytes
       * set bits 0..6, 14..25, and 28..31
       * 0xf3ffc07f = binary 11110011111111111100000001111111
       */
      unsigned long black_mask = 0xf3ffc07fUL;
      int n;

      /* Check for non-textual ("black-listed") bytes. */
      for (n = 0; n <= 31; n++, black_mask >>= 1)
         if ((black_mask & 1) && (s->dyn_ltree[n].Freq != 0))
            return Z_BINARY;

      /* Check for textual ("white-listed") bytes. */
      if (s->dyn_ltree[9].Freq != 0 || s->dyn_ltree[10].Freq != 0
            || s->dyn_ltree[13].Freq != 0)
         return Z_TEXT;
      for (n = 32; n < LITERALS; n++)
         if (s->dyn_ltree[n].Freq != 0)
            return Z_TEXT;

      /* There are no "black-listed" or "white-listed" bytes:
       * this stream either is empty or has tolerated ("gray-listed") bytes only.
       */
      return Z_BINARY;
   }

   /* ===========================================================================
    * Reverse the first len bits of a code, using straightforward code (a faster
    * method would use a table)
    * IN assertion: 1 <= len <= 15
    */
   static unsigned bi_reverse(unsigned codes, int len)
   {
      register unsigned res = 0;
      do {
         res |= codes & 1;
         codes >>= 1;
         res   <<= 1;
      } while (--len > 0);
      return res >> 1;
   }

   /* ===========================================================================
    * Flush the bit buffer, keeping at most 7 bits in it.
    */
   static void bi_flush(deflate_state *s)
   {
      if (s->bi_valid == 16) {
         put_short(s, s->bi_buf);
         s->bi_buf = 0;
         s->bi_valid = 0;
      } else if (s->bi_valid >= 8) {
         put_byte(s, (Byte)s->bi_buf);
         s->bi_buf >>= 8;
         s->bi_valid -= 8;
      }
   }

   /* ===========================================================================
    * Flush the bit buffer and align the output on a byte boundary
    */
   static void bi_windup(deflate_state *s)
   {
      if (s->bi_valid > 8) {
         put_short(s, s->bi_buf);
      } else if (s->bi_valid > 0) {
         put_byte(s, (Byte)s->bi_buf);
      }
      s->bi_buf = 0;
      s->bi_valid = 0;
   }
