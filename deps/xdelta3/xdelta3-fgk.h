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

   For demonstration purposes only.
 */

#ifndef _XDELTA3_FGK_h_
#define _XDELTA3_FGK_h_

/* An implementation of the FGK algorithm described by D.E. Knuth in
 * "Dynamic Huffman Coding" in Journal of Algorithms 6. */

/* A 32bit counter (fgk_weight) is used as the frequency counter for
 * nodes in the huffman tree.  TODO: Need oto test for overflow and/or
 * reset stats. */

typedef struct _fgk_stream fgk_stream;
typedef struct _fgk_node   fgk_node;
typedef struct _fgk_block  fgk_block;
typedef unsigned int       fgk_bit;
typedef uint32_t           fgk_weight;

struct _fgk_block {
  union {
    fgk_node  *un_leader;
    fgk_block *un_freeptr;
  } un;
};

#define block_leader  un.un_leader
#define block_freeptr un.un_freeptr

/* The code can also support fixed huffman encoding/decoding. */
#define IS_ADAPTIVE 1

/* weight is a count of the number of times this element has been seen
 * in the current encoding/decoding.  parent, right_child, and
 * left_child are pointers defining the tree structure.  right and
 * left point to neighbors in an ordered sequence of weights.  The
 * left child of a node is always guaranteed to have weight not
 * greater than its sibling.  fgk_blockLeader points to the element
 * with the same weight as itself which is closest to the next
 * increasing weight block.  */
struct _fgk_node
{
  fgk_weight  weight;
  fgk_node   *parent;
  fgk_node   *left_child;
  fgk_node   *right_child;
  fgk_node   *left;
  fgk_node   *right;
  fgk_block  *my_block;
};

/* alphabet_size is the a count of the number of possible leaves in
 * the huffman tree.  The number of total nodes counting internal
 * nodes is ((2 * alphabet_size) - 1).  zero_freq_count is the number
 * of elements remaining which have zero frequency.  zero_freq_exp and
 * zero_freq_rem satisfy the equation zero_freq_count =
 * 2^zero_freq_exp + zero_freq_rem.  root_node is the root of the
 * tree, which is initialized to a node with zero frequency and
 * contains the 0th such element.  free_node contains a pointer to the
 * next available fgk_node space.  alphabet contains all the elements
 * and is indexed by N.  remaining_zeros points to the head of the
 * list of zeros.  */
struct _fgk_stream
{
  usize_t alphabet_size;
  usize_t zero_freq_count;
  usize_t zero_freq_exp;
  usize_t zero_freq_rem;
  usize_t coded_depth;

  usize_t total_nodes;
  usize_t total_blocks;

  fgk_bit *coded_bits;

  fgk_block *block_array;
  fgk_block *free_block;

  fgk_node *decode_ptr;
  fgk_node *remaining_zeros;
  fgk_node *alphabet;
  fgk_node *root_node;
  fgk_node *free_node;
};

/*********************************************************************/
/*                             Encoder                               */
/*********************************************************************/

static fgk_stream*     fgk_alloc           (xd3_stream *stream /*, usize_t alphabet_size */);
static int             fgk_init            (xd3_stream *stream,
					    fgk_stream *h, 
					    int is_encode);
static usize_t         fgk_encode_data     (fgk_stream *h,
					    usize_t    n);
static inline fgk_bit  fgk_get_encoded_bit (fgk_stream *h);

static int             xd3_encode_fgk      (xd3_stream  *stream,
					    fgk_stream  *sec_stream,
					    xd3_output  *input,
					    xd3_output  *output,
					    xd3_sec_cfg *cfg);

/*********************************************************************/
/* 			       Decoder                               */
/*********************************************************************/

static inline int      fgk_decode_bit      (fgk_stream *h,
					    fgk_bit     b);
static usize_t         fgk_decode_data     (fgk_stream *h);
static void            fgk_destroy         (xd3_stream *stream,
					    fgk_stream *h);

static int             xd3_decode_fgk      (xd3_stream     *stream,
					    fgk_stream     *sec_stream,
					    const uint8_t **input,
					    const uint8_t  *const input_end,
					    uint8_t       **output,
					    const uint8_t  *const output_end);

/*********************************************************************/
/* 			       Private                               */
/*********************************************************************/

static unsigned int fgk_find_nth_zero        (fgk_stream *h, usize_t n);
static usize_t      fgk_nth_zero             (fgk_stream *h, usize_t n);
static void         fgk_update_tree          (fgk_stream *h, usize_t n);
static fgk_node*    fgk_increase_zero_weight (fgk_stream *h, usize_t n);
static void         fgk_eliminate_zero       (fgk_stream* h, fgk_node *node);
static void         fgk_move_right           (fgk_stream *h, fgk_node *node);
static void         fgk_promote              (fgk_stream *h, fgk_node *node);
static void         fgk_init_node            (fgk_node *node, usize_t i, usize_t size);
static fgk_block*   fgk_make_block           (fgk_stream *h, fgk_node *l);
static void         fgk_free_block           (fgk_stream *h, fgk_block *b);
static void         fgk_factor_remaining     (fgk_stream *h);
static inline void  fgk_swap_ptrs            (fgk_node **one, fgk_node **two);

/*********************************************************************/
/* 			    Basic Routines                           */
/*********************************************************************/

/* returns an initialized huffman encoder for an alphabet with the
 * given size.  returns NULL if enough memory cannot be allocated */
static fgk_stream* fgk_alloc (xd3_stream *stream /*, int alphabet_size0 */)
{
  usize_t alphabet_size0 = ALPHABET_SIZE;
  fgk_stream *h;

  if ((h = (fgk_stream*) xd3_alloc (stream, 1, sizeof (fgk_stream))) == NULL)
    {
      return NULL;
    }

  h->total_nodes  = (2 * alphabet_size0) - 1;
  h->total_blocks = (2 * h->total_nodes);
  h->alphabet     = (fgk_node*)  xd3_alloc (stream, h->total_nodes,    sizeof (fgk_node));
  h->block_array  = (fgk_block*) xd3_alloc (stream, h->total_blocks,   sizeof (fgk_block));
  h->coded_bits   = (fgk_bit*)   xd3_alloc (stream, alphabet_size0, sizeof (fgk_bit));

  if (h->coded_bits  == NULL ||
      h->alphabet    == NULL ||
      h->block_array == NULL)
    {
      fgk_destroy (stream, h);
      return NULL;
    }

  h->alphabet_size   = alphabet_size0;

  return h;
}

static int fgk_init (xd3_stream *stream, fgk_stream *h, int is_encode)
{
  usize_t ui;
  ssize_t si;

  h->root_node       = h->alphabet;
  h->decode_ptr      = h->root_node;
  h->free_node       = h->alphabet + h->alphabet_size;
  h->remaining_zeros = h->alphabet;
  h->coded_depth     = 0;
  h->zero_freq_count = h->alphabet_size + 2;

  /* after two calls to factor_remaining, zero_freq_count == alphabet_size */
  fgk_factor_remaining(h); /* set ZFE and ZFR */
  fgk_factor_remaining(h); /* set ZFDB according to prev state */

  IF_DEBUG (memset (h->alphabet, 0, sizeof (h->alphabet[0]) * h->total_nodes));

  for (ui = 0; ui < h->total_blocks-1; ui += 1)
    {
      h->block_array[ui].block_freeptr = &h->block_array[ui + 1];
    }

  h->block_array[h->total_blocks - 1].block_freeptr = NULL;
  h->free_block = h->block_array;

  /* Zero frequency nodes are inserted in the first alphabet_size
   * positions, with Value, weight, and a pointer to the next zero
   * frequency node.  */
  for (si = h->alphabet_size - 1; si >= 0; si -= 1)
    {
      fgk_init_node (h->alphabet + si, (usize_t) si, h->alphabet_size);
    }

  return 0;
}

static void fgk_swap_ptrs(fgk_node **one, fgk_node **two)
{
  fgk_node *tmp = *one;
  *one = *two;
  *two = tmp;
}

/* Takes huffman transmitter h and n, the nth elt in the alphabet, and
 * returns the number of required to encode n. */
static usize_t fgk_encode_data (fgk_stream* h, usize_t n)
{
  fgk_node *target_ptr = h->alphabet + n;

  XD3_ASSERT (n < h->alphabet_size);

  h->coded_depth = 0;

  /* First encode the binary representation of the nth remaining
   * zero frequency element in reverse such that bit, which will be
   * encoded from h->coded_depth down to 0 will arrive in increasing
   * order following the tree path.  If there is only one left, it
   * is not neccesary to encode these bits. */
  if (IS_ADAPTIVE && target_ptr->weight == 0)
    {
      usize_t where, shift;
      usize_t bits;

      where = fgk_find_nth_zero(h, n);
      shift = 1;

      if (h->zero_freq_rem == 0)
	{
	  bits = h->zero_freq_exp;
	}
      else
	{
	  bits = h->zero_freq_exp + 1;
	}

      while (bits > 0)
	{
	  h->coded_bits[h->coded_depth++] = (shift & where) && 1;

	  bits   -= 1;
	  shift <<= 1;
	};

      target_ptr = h->remaining_zeros;
    }

  /* The path from root to node is filled into coded_bits in reverse so
   * that it is encoded in the right order */
  while (target_ptr != h->root_node)
    {
      h->coded_bits[h->coded_depth++] = (target_ptr->parent->right_child == target_ptr);

      target_ptr = target_ptr->parent;
    }

  if (IS_ADAPTIVE)
    {
      fgk_update_tree(h, n);
    }

  return h->coded_depth;
}

/* Should be called as many times as fgk_encode_data returns.
 */
static inline fgk_bit fgk_get_encoded_bit (fgk_stream *h)
{
  XD3_ASSERT (h->coded_depth > 0);

  return h->coded_bits[--h->coded_depth];
}

/* This procedure updates the tree after alphabet[n] has been encoded
 * or decoded.
 */
static void fgk_update_tree (fgk_stream *h, usize_t n)
{
  fgk_node *incr_node;

  if (h->alphabet[n].weight == 0)
    {
      incr_node = fgk_increase_zero_weight (h, n);
    }
  else
    {
      incr_node = h->alphabet + n;
    }

  while (incr_node != h->root_node)
    {
      fgk_move_right (h, incr_node);
      fgk_promote    (h, incr_node);
      incr_node->weight += 1;   /* incr the parent */
      incr_node = incr_node->parent; /* repeat */
    }

  h->root_node->weight += 1;
}

static void fgk_move_right (fgk_stream *h, fgk_node *move_fwd)
{
  fgk_node **fwd_par_ptr, **back_par_ptr;
  fgk_node *move_back, *tmp;

  move_back = move_fwd->my_block->block_leader;

  if (move_fwd         == move_back ||
      move_fwd->parent == move_back ||
      move_fwd->weight == 0)
    {
      return;
    }

  move_back->right->left = move_fwd;

  if (move_fwd->left)
    {
      move_fwd->left->right = move_back;
    }

  tmp = move_fwd->right;
  move_fwd->right = move_back->right;

  if (tmp == move_back)
    {
      move_back->right = move_fwd;
    }
  else
    {
      tmp->left = move_back;
      move_back->right = tmp;
    }

  tmp = move_back->left;
  move_back->left = move_fwd->left;

  if (tmp == move_fwd)
    {
      move_fwd->left = move_back;
    }
  else
    {
      tmp->right = move_fwd;
      move_fwd->left = tmp;
    }

  if (move_fwd->parent->right_child == move_fwd)
    {
      fwd_par_ptr = &move_fwd->parent->right_child;
    }
  else
    {
      fwd_par_ptr = &move_fwd->parent->left_child;
    }

  if (move_back->parent->right_child == move_back)
    {
      back_par_ptr = &move_back->parent->right_child;
    }
  else
    {
      back_par_ptr = &move_back->parent->left_child;
    }

  fgk_swap_ptrs (&move_fwd->parent, &move_back->parent);
  fgk_swap_ptrs (fwd_par_ptr, back_par_ptr);

  move_fwd->my_block->block_leader = move_fwd;
}

/* Shifts node, the leader of its block, into the next block. */
static void fgk_promote (fgk_stream *h, fgk_node *node)
{
  fgk_node *my_left, *my_right;
  fgk_block *cur_block;

  my_right  = node->right;
  my_left   = node->left;
  cur_block = node->my_block;

  if (node->weight == 0)
    {
      return;
    }

  /* if left is right child, parent of remaining zeros case (?), means parent
   * has same weight as right child. */
  if (my_left == node->right_child &&
      node->left_child &&
      node->left_child->weight == 0)
    {
      XD3_ASSERT (node->left_child == h->remaining_zeros);
      XD3_ASSERT (node->right_child->weight == (node->weight+1)); /* child weight was already incremented */
      
      if (node->weight == (my_right->weight - 1) && my_right != h->root_node)
	{
	  fgk_free_block (h, cur_block);
	  node->my_block    = my_right->my_block;
	  my_left->my_block = my_right->my_block;
	}

      return;
    }

  if (my_left == h->remaining_zeros)
    {
      return;
    }

  /* true if not the leftmost node */
  if (my_left->my_block == cur_block)
    {
      my_left->my_block->block_leader = my_left;
    }
  else
    {
      fgk_free_block (h, cur_block);
    }

  /* node->parent != my_right */
  if ((node->weight == (my_right->weight - 1)) && (my_right != h->root_node))
    {
      node->my_block = my_right->my_block;
    }
  else
    {
      node->my_block = fgk_make_block (h, node);
    }
}

/* When an element is seen the first time this is called to remove it from the list of
 * zero weight elements and introduce a new internal node to the tree.  */
static fgk_node* fgk_increase_zero_weight (fgk_stream *h, usize_t n)
{
  fgk_node *this_zero, *new_internal, *zero_ptr;

  this_zero = h->alphabet + n;

  if (h->zero_freq_count == 1)
    {
      /* this is the last one */
      this_zero->right_child = NULL;

      if (this_zero->right->weight == 1)
	{
	  this_zero->my_block = this_zero->right->my_block;
	}
      else
	{
	  this_zero->my_block = fgk_make_block (h, this_zero);
	}

      h->remaining_zeros = NULL;

      return this_zero;
    }

  zero_ptr = h->remaining_zeros;

  new_internal = h->free_node++;

  new_internal->parent      = zero_ptr->parent;
  new_internal->right       = zero_ptr->right;
  new_internal->weight      = 0;
  new_internal->right_child = this_zero;
  new_internal->left        = this_zero;

  if (h->remaining_zeros == h->root_node)
    {
      /* This is the first element to be coded */
      h->root_node           = new_internal;
      this_zero->my_block    = fgk_make_block (h, this_zero);
      new_internal->my_block = fgk_make_block (h, new_internal);
    }
  else
    {
      new_internal->right->left = new_internal;

      if (zero_ptr->parent->right_child == zero_ptr)
	{
	  zero_ptr->parent->right_child = new_internal;
	}
      else
	{
	  zero_ptr->parent->left_child = new_internal;
	}

      if (new_internal->right->weight == 1)
	{
	  new_internal->my_block = new_internal->right->my_block;
	}
      else
	{
	  new_internal->my_block = fgk_make_block (h, new_internal);
	}

      this_zero->my_block = new_internal->my_block;
    }

  fgk_eliminate_zero (h, this_zero);

  new_internal->left_child = h->remaining_zeros;

  this_zero->right       = new_internal;
  this_zero->left        = h->remaining_zeros;
  this_zero->parent      = new_internal;
  this_zero->left_child  = NULL;
  this_zero->right_child = NULL;

  h->remaining_zeros->parent = new_internal;
  h->remaining_zeros->right  = this_zero;

  return this_zero;
}

/* When a zero frequency element is encoded, it is followed by the
 * binary representation of the index into the remaining elements.
 * Sets a cache to the element before it so that it can be removed
 * without calling this procedure again.  */
static unsigned int fgk_find_nth_zero (fgk_stream* h, usize_t n)
{
  fgk_node *target_ptr = h->alphabet + n;
  fgk_node *head_ptr = h->remaining_zeros;
  unsigned int idx = 0;

  while (target_ptr != head_ptr)
    {
      head_ptr = head_ptr->right_child;
      idx += 1;
    }

  return idx;
}

/* Splices node out of the list of zeros. */
static void fgk_eliminate_zero (fgk_stream* h, fgk_node *node)
{
  if (h->zero_freq_count == 1)
    {
      return;
    }

  fgk_factor_remaining(h);

  if (node->left_child == NULL)
    {
      h->remaining_zeros = h->remaining_zeros->right_child;
      h->remaining_zeros->left_child = NULL;
    }
  else if (node->right_child == NULL)
    {
      node->left_child->right_child = NULL;
    }
  else
    {
      node->right_child->left_child = node->left_child;
      node->left_child->right_child = node->right_child;
    }
}

static void fgk_init_node (fgk_node *node, usize_t i, usize_t size)
{
  if (i < size - 1)
    {
      node->right_child = node + 1;
    }
  else
    {
      node->right_child = NULL;
    }

  if (i >= 1)
    {
      node->left_child = node - 1;
    }
  else
    {
      node->left_child = NULL;
    }

  node->weight      = 0;
  node->parent      = NULL;
  node->right = NULL;
  node->left  = NULL;
  node->my_block    = NULL;
}

/* The data structure used is an array of blocks, which are unions of
 * free pointers and huffnode pointers.  free blocks are a linked list
 * of free blocks, the front of which is h->free_block.  The used
 * blocks are pointers to the head of each block.  */
static fgk_block* fgk_make_block (fgk_stream *h, fgk_node* lead)
{
  fgk_block *ret = h->free_block;

  XD3_ASSERT (h->free_block != NULL);

  h->free_block = h->free_block->block_freeptr;

  ret->block_leader = lead;

  return ret;
}

/* Restores the block to the front of the free list. */
static void fgk_free_block (fgk_stream *h, fgk_block *b)
{
  b->block_freeptr = h->free_block;
  h->free_block = b;
}

/* sets zero_freq_count, zero_freq_rem, and zero_freq_exp to satsity
 * the equation given above.  */
static void fgk_factor_remaining (fgk_stream *h)
{
  unsigned int i;

  i = (--h->zero_freq_count);
  h->zero_freq_exp = 0;

  while (i > 1)
    {
      h->zero_freq_exp += 1;
      i >>= 1;
    }

  i = 1 << h->zero_freq_exp;

  h->zero_freq_rem = h->zero_freq_count - i;
}

/* receives a bit at a time and returns true when a complete code has
 * been received.
 */
static inline int fgk_decode_bit (fgk_stream* h, fgk_bit b)
{
  XD3_ASSERT (b == 1 || b == 0);

  if (IS_ADAPTIVE && h->decode_ptr->weight == 0)
    {
      usize_t bitsreq;

      if (h->zero_freq_rem == 0)
	{
	  bitsreq = h->zero_freq_exp;
	}
      else
	{
	  bitsreq = h->zero_freq_exp + 1;
	}

      h->coded_bits[h->coded_depth] = b;
      h->coded_depth += 1;

      return h->coded_depth >= bitsreq;
    }
  else
    {
      if (b)
	{
	  h->decode_ptr = h->decode_ptr->right_child;
	}
      else
	{
	  h->decode_ptr = h->decode_ptr->left_child;
	}

      if (h->decode_ptr->left_child == NULL)
	{
	  /* If the weight is non-zero, finished. */
	  if (h->decode_ptr->weight != 0)
	    {
	      return 1;
	    }

	  /* zero_freq_count is dropping to 0, finished. */
	  return h->zero_freq_count == 1;
	}
      else
	{
	  return 0;
	}
    }
}

static usize_t fgk_nth_zero (fgk_stream* h, usize_t n)
{
  fgk_node *ret = h->remaining_zeros;

  /* ERROR: if during this loop (ret->right_child == NULL) then the
   * encoder's zero count is too high.  Could return an error code
   * now, but is probably unnecessary overhead, since the caller
   * should check integrity anyway. */
  for (; n != 0 && ret->right_child != NULL; n -= 1)
    {
      ret = ret->right_child;
    }

  return (usize_t)(ret - h->alphabet);
}

/* once fgk_decode_bit returns 1, this retrieves an index into the
 * alphabet otherwise this returns 0, indicating more bits are
 * required.
 */
static usize_t fgk_decode_data (fgk_stream* h)
{
  usize_t elt = (usize_t)(h->decode_ptr - h->alphabet);

  if (IS_ADAPTIVE && h->decode_ptr->weight == 0) {
    usize_t i = 0;
    usize_t n = 0;

    if (h->coded_depth > 0) 
      {
	for (; i < h->coded_depth - 1; i += 1)
	  {
	    n |= h->coded_bits[i];
	    n <<= 1;
	  }
      }

    n |= h->coded_bits[i];
    elt = fgk_nth_zero(h, n);
  }

  h->coded_depth = 0;

  if (IS_ADAPTIVE)
    {
      fgk_update_tree(h, elt);
    }

  h->decode_ptr = h->root_node;

  return elt;
}

static void fgk_destroy (xd3_stream *stream,
			 fgk_stream *h)
{
  if (h != NULL)
    {
      xd3_free (stream, h->alphabet);
      xd3_free (stream, h->coded_bits);
      xd3_free (stream, h->block_array);
      xd3_free (stream, h);
    }
}

/*********************************************************************/
/* 			       Xdelta                                */
/*********************************************************************/

static int
xd3_encode_fgk (xd3_stream *stream, fgk_stream *sec_stream, xd3_output *input, xd3_output *output, xd3_sec_cfg *cfg)
{
  bit_state   bstate = BIT_STATE_ENCODE_INIT;
  xd3_output *cur_page;
  int ret;

  /* OPT: quit compression early if it looks bad */
  for (cur_page = input; cur_page; cur_page = cur_page->next_page)
    {
      const uint8_t *inp     = cur_page->base;
      const uint8_t *inp_max = inp + cur_page->next;

      while (inp < inp_max)
	{
	  usize_t bits = fgk_encode_data (sec_stream, *inp++);

	  while (bits--)
	    {
	      if ((ret = xd3_encode_bit (stream, & output, & bstate, fgk_get_encoded_bit (sec_stream)))) { return ret; }
	    }
	}
    }

  return xd3_flush_bits (stream, & output, & bstate);
}

static int
xd3_decode_fgk (xd3_stream     *stream,
		fgk_stream     *sec_stream,
		const uint8_t **input_pos,
		const uint8_t  *const input_max,
		uint8_t       **output_pos,
		const uint8_t  *const output_max)
{
  bit_state bstate;
  uint8_t *output = *output_pos;
  const uint8_t *input = *input_pos;

  for (;;)
    {
      if (input == input_max)
	{
	  stream->msg = "secondary decoder end of input";
	  return XD3_INTERNAL;
	}

      bstate.cur_byte = *input++;

      for (bstate.cur_mask = 1; bstate.cur_mask != 0x100; bstate.cur_mask <<= 1)
	{
	  int done = fgk_decode_bit (sec_stream, (bstate.cur_byte & bstate.cur_mask) ? 1U : 0U);

	  if (! done) { continue; }

	  *output++ = fgk_decode_data (sec_stream);

	  if (output == output_max)
	    {
	      /* During regression testing: */
	      IF_REGRESSION ({
		int ret;
		bstate.cur_mask <<= 1;
		if ((ret = xd3_test_clean_bits (stream, & bstate))) { return ret; }
	      });

	      (*output_pos) = output;
	      (*input_pos) = input;
	      return 0;
	    }
	}
    }
}

#endif /* _XDELTA3_FGK_ */
