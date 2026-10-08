/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rrar_ppmd7.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* PPMd var.H decoding for RAR 2.9; see rrar_ppmd7.h. */

#include <stdlib.h>
#include <string.h>

#include <retro_inline.h>

#include "rrar_ppmd7.h"

#define PPMD_UNIT_SIZE      12
#define PPMD_MAX_FREQ       124
#define PPMD_MAX_ORDER      64
#define PPMD_INT_BITS       7
#define PPMD_PERIOD_BITS    7
#define PPMD_BIN_SCALE      (1u << (PPMD_INT_BITS + PPMD_PERIOD_BITS))
#define PPMD_NUM_INDEXES    RRAR_PPMD7_NUM_INDEXES

#if defined(__GNUC__)
#define PPMD_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER) && _MSC_VER >= 1300
#define PPMD_NOINLINE __declspec(noinline)
#else
#define PPMD_NOINLINE
#endif

/* RAR's range coder: renormalise while the top byte is settled, or while
 * the range has fallen under the bottom, in which case it is cut to what
 * is left before the next multiple of it. */
#define PPMD_RC_TOP         (1u << 24)
#define PPMD_RC_BOTTOM      (1u << 15)

/* A symbol and how often it was seen in a context; the context it leads
 * to, or the text after it, split in two halves to keep the state at six
 * bytes on a two-byte boundary. */
typedef struct rrar_ppmd7_state
{
   uint8_t  symbol;
   uint8_t  freq;
   uint16_t successor_lo;
   uint16_t successor_hi;
} ppmd_state_t;

/* A context of one unit: with more than one symbol, the sum of their
 * counts and its block of states; with one, that state in their place.
 * The halves of the link keep the union on a two-byte boundary, where
 * the one state's fields fall. */
typedef struct rrar_ppmd7_context
{
   uint16_t num_stats;
   union
   {
      struct
      {
         uint16_t summ_freq;
         uint16_t stats_lo;
         uint16_t stats_hi;
      } m;
      ppmd_state_t one;
   } u;
   uint32_t suffix;
} ppmd_context_t;

/* A free block while blocks are glued: stamp sits where a context keeps
 * num_stats and a state its symbol and freq, so a block in use never
 * reads as free (0). */
typedef struct ppmd_node
{
   uint16_t stamp;
   uint16_t nu;
   uint32_t next;
   uint32_t prev;
} ppmd_node_t;

#define PPMD_PTR(p, ref)          ((void *)((p)->base + (ref)))
#define PPMD_CTX(p, ref)          ((ppmd_context_t *)((p)->base + (ref)))
#define PPMD_NODE(p, ref)         ((ppmd_node_t *)((p)->base + (ref)))
#define PPMD_STATS_REF(ctx)       ((uint32_t)(ctx)->u.m.stats_lo | ((uint32_t)(ctx)->u.m.stats_hi << 16))
#define PPMD_STATS(p, ctx)        ((ppmd_state_t *)((p)->base + PPMD_STATS_REF(ctx)))
#define PPMD_SUFFIX(p, ctx)       PPMD_CTX(p, (ctx)->suffix)
#define PPMD_REF(p, ptr)          ((uint32_t)((uint8_t *)(ptr) - (p)->base))
#define PPMD_ONE_STATE(ctx)       (&(ctx)->u.one)
#define PPMD_SUCCESSOR(s)         ((uint32_t)(s)->successor_lo | ((uint32_t)(s)->successor_hi << 16))
#define PPMD_U2B(nu)              ((uint32_t)(nu) * PPMD_UNIT_SIZE)
#define PPMD_U2I(p, nu)           ((p)->units2indx[(nu) - 1])
#define PPMD_I2U(p, indx)         ((p)->indx2units[indx])

#define PPMD_SET_SUCCESSOR(s, v) \
   do \
   { \
      ppmd_state_t *st_  = (s); \
      uint32_t succ_     = (v); \
      st_->successor_lo  = (uint16_t)succ_; \
      st_->successor_hi  = (uint16_t)(succ_ >> 16); \
   } while (0)

#define PPMD_SET_STATS(ctx, v) \
   do \
   { \
      ppmd_context_t *ctx_ = (ctx); \
      uint32_t ref_        = (v); \
      ctx_->u.m.stats_lo   = (uint16_t)ref_; \
      ctx_->u.m.stats_hi   = (uint16_t)(ref_ >> 16); \
   } while (0)

/* The mean a binary context's probability moves by. */
#define PPMD_GET_MEAN(prob)       (((prob) + (1 << (PPMD_PERIOD_BITS - 2))) >> PPMD_PERIOD_BITS)

static const uint16_t ppmd_init_bin_esc[8] = {
   0x3CDD, 0x1F3F, 0x59BF, 0x48F3, 0x64A1, 0x5ABC, 0x6632, 0x6051
};
static const uint8_t ppmd_exp_escape[16] = {
   25, 14, 9, 7, 5, 5, 4, 4, 4, 3, 3, 3, 2, 2, 2, 2
};

/* ----------------------------------------------------------- range coder */

static INLINE uint32_t ppmd_rc_byte(rrar_ppmd7_range_t *rc)
{
   if (rc->p < rc->end)
      return *rc->p++;
   rc->past++;
   return 0;
}

static INLINE void ppmd_rc_normalize(rrar_ppmd7_range_t *rc)
{
   for (;;)
   {
      if ((rc->low ^ (rc->low + rc->range)) >= PPMD_RC_TOP)
      {
         if (rc->range >= PPMD_RC_BOTTOM)
            break;
         rc->range = (0u - rc->low) & (PPMD_RC_BOTTOM - 1);
      }
      rc->code    = (rc->code << 8) | ppmd_rc_byte(rc);
      rc->range <<= 8;
      rc->low   <<= 8;
   }
}

/* Where the code falls among @total. */
static INLINE uint32_t ppmd_rc_threshold(rrar_ppmd7_range_t *rc,
      uint32_t total)
{
   return (rc->code - rc->low) / (rc->range /= total);
}

static INLINE void ppmd_rc_decode(rrar_ppmd7_range_t *rc,
      uint32_t start, uint32_t size)
{
   rc->low   += start * rc->range;
   rc->range *= size;
   ppmd_rc_normalize(rc);
}

/* A binary decision with @size0 of PPMD_BIN_SCALE for 0. The scale is a
 * power of two, so the threshold needs no division: the code is under
 * size0 * range exactly when the quotient is under size0. */
static INLINE unsigned ppmd_rc_bit(rrar_ppmd7_range_t *rc, uint32_t size0)
{
   rc->range >>= PPMD_INT_BITS + PPMD_PERIOD_BITS;
   if (rc->code - rc->low < size0 * rc->range)
   {
      rc->range *= size0;
      ppmd_rc_normalize(rc);
      return 0;
   }
   rc->low   += size0 * rc->range;
   rc->range *= PPMD_BIN_SCALE - size0;
   ppmd_rc_normalize(rc);
   return 1;
}

/* ------------------------------------------------------------- allocator */

/* A free block's link to the next of its size sits in its first four
 * bytes, where the block's own fields are when in use. */
static INLINE void ppmd_insert_node(rrar_ppmd7_t *p, void *node,
      unsigned indx)
{
   memcpy(node, &p->free_list[indx], sizeof(uint32_t));
   p->free_list[indx] = PPMD_REF(p, node);
}

static INLINE void *ppmd_remove_node(rrar_ppmd7_t *p, unsigned indx)
{
   void *node = PPMD_PTR(p, p->free_list[indx]);
   memcpy(&p->free_list[indx], node, sizeof(uint32_t));
   return node;
}

/* The units of a block of size @old_indx past the first @new_indx's
 * worth go back on the free lists. */
static void ppmd_split_block(rrar_ppmd7_t *p, void *ptr,
      unsigned old_indx, unsigned new_indx)
{
   unsigned i;
   unsigned nu = PPMD_I2U(p, old_indx) - PPMD_I2U(p, new_indx);
   ptr         = (uint8_t *)ptr + PPMD_U2B(PPMD_I2U(p, new_indx));
   if (PPMD_I2U(p, i = PPMD_U2I(p, nu)) != nu)
   {
      unsigned k = PPMD_I2U(p, --i);
      ppmd_insert_node(p, (uint8_t *)ptr + PPMD_U2B(k), nu - k - 1);
   }
   ppmd_insert_node(p, ptr, i);
}

/* Every free block into one list, neighbours merged, and back onto the
 * free lists by size. The list's head is the spare unit past the end of
 * the model's memory. */
static void ppmd_glue_free_blocks(rrar_ppmd7_t *p)
{
   unsigned i;
   uint32_t head = p->align_offset + p->size;
   uint32_t n    = head;

   p->glue_count = 255;

   for (i = 0; i < PPMD_NUM_INDEXES; i++)
   {
      uint16_t nu   = PPMD_I2U(p, i);
      uint32_t next = p->free_list[i];
      p->free_list[i] = 0;
      while (next != 0)
      {
         ppmd_node_t *node    = PPMD_NODE(p, next);
         node->next           = n;
         PPMD_NODE(p, n)->prev = next;
         n                    = next;
         memcpy(&next, node, sizeof(uint32_t));
         node->stamp          = 0;
         node->nu             = nu;
      }
   }
   PPMD_NODE(p, head)->stamp = 1;
   PPMD_NODE(p, head)->next  = n;
   PPMD_NODE(p, n)->prev     = head;
   if (p->lo_unit != p->hi_unit)
      ((ppmd_node_t *)p->lo_unit)->stamp = 1;

   /* each block takes in the free blocks that follow it */
   while (n != head)
   {
      ppmd_node_t *node = PPMD_NODE(p, n);
      uint32_t nu       = node->nu;
      for (;;)
      {
         ppmd_node_t *node2 = node + nu;
         uint16_t head2[2];
         /* the block may be in use: its first four bytes as they are */
         memcpy(head2, node2, sizeof(head2));
         nu += head2[1];
         if (head2[0] != 0 || nu >= 0x10000)
            break;
         PPMD_NODE(p, node2->prev)->next = node2->next;
         PPMD_NODE(p, node2->next)->prev = node2->prev;
         node->nu                        = (uint16_t)nu;
      }
      n = node->next;
   }

   for (n = PPMD_NODE(p, head)->next; n != head; )
   {
      ppmd_node_t *node = PPMD_NODE(p, n);
      uint32_t next     = node->next;
      unsigned nu;
      for (nu = node->nu; nu > 128; nu -= 128, node += 128)
         ppmd_insert_node(p, node, PPMD_NUM_INDEXES - 1);
      if (PPMD_I2U(p, i = PPMD_U2I(p, nu)) != nu)
      {
         unsigned k = PPMD_I2U(p, --i);
         ppmd_insert_node(p, node + k, nu - k - 1);
      }
      ppmd_insert_node(p, node, i);
      n = next;
   }
}

/* A block when its own list and the gap between the unit areas are
 * empty: glued lists, a larger block split, or the top of the text area.
 * NULL when there is none. */
static void *ppmd_alloc_units_rare(rrar_ppmd7_t *p, unsigned indx)
{
   unsigned i;
   void *block;

   if (p->glue_count == 0)
   {
      ppmd_glue_free_blocks(p);
      if (p->free_list[indx] != 0)
         return ppmd_remove_node(p, indx);
   }

   i = indx;
   do
   {
      if (++i == PPMD_NUM_INDEXES)
      {
         uint32_t num_bytes = PPMD_U2B(PPMD_I2U(p, indx));
         p->glue_count--;
         if ((uint32_t)(p->units_start - p->text) > num_bytes)
            return p->units_start -= num_bytes;
         return NULL;
      }
   } while (p->free_list[i] == 0);

   block = ppmd_remove_node(p, i);
   ppmd_split_block(p, block, i, indx);
   return block;
}

static INLINE void *ppmd_alloc_units(rrar_ppmd7_t *p, unsigned indx)
{
   uint32_t num_bytes;
   if (p->free_list[indx] != 0)
      return ppmd_remove_node(p, indx);
   num_bytes = PPMD_U2B(PPMD_I2U(p, indx));
   if (num_bytes <= (uint32_t)(p->hi_unit - p->lo_unit))
   {
      void *block  = p->lo_unit;
      p->lo_unit  += num_bytes;
      return block;
   }
   return ppmd_alloc_units_rare(p, indx);
}

/* A block of states down from @old_nu units to @new_nu: moved when a
 * block of the smaller size is free, else split in place. */
static void *ppmd_shrink_units(rrar_ppmd7_t *p, void *old_ptr,
      unsigned old_nu, unsigned new_nu)
{
   unsigned i0 = PPMD_U2I(p, old_nu);
   unsigned i1 = PPMD_U2I(p, new_nu);
   if (i0 == i1)
      return old_ptr;
   if (p->free_list[i1] != 0)
   {
      void *ptr = ppmd_remove_node(p, i1);
      memcpy(ptr, old_ptr, PPMD_U2B(new_nu));
      ppmd_insert_node(p, old_ptr, i0);
      return ptr;
   }
   ppmd_split_block(p, old_ptr, i0, i1);
   return old_ptr;
}

/* ----------------------------------------------------------------- model */

static void ppmd_restart_model(rrar_ppmd7_t *p)
{
   unsigned i, k, m;
   ppmd_state_t *s;

   memset(p->free_list, 0, sizeof(p->free_list));
   p->text         = p->base + p->align_offset;
   p->hi_unit      = p->text + p->size;
   p->lo_unit      = p->hi_unit - p->size / 8 / PPMD_UNIT_SIZE * 7 * PPMD_UNIT_SIZE;
   p->units_start  = p->lo_unit;
   p->glue_count   = 0;

   p->order_fall   = p->max_order;
   p->init_rl      = -(int32_t)((p->max_order < 12) ? p->max_order : 12) - 1;
   p->run_length   = p->init_rl;
   p->prev_success = 0;

   p->hi_unit     -= PPMD_UNIT_SIZE;
   p->min_ctx      = (ppmd_context_t *)p->hi_unit;
   p->max_ctx      = p->min_ctx;
   p->min_ctx->suffix    = 0;
   p->min_ctx->num_stats = 256;
   p->min_ctx->u.m.summ_freq = 256 + 1;

   s               = (ppmd_state_t *)p->lo_unit;
   p->found        = s;
   p->lo_unit     += PPMD_U2B(256 / 2);
   PPMD_SET_STATS(p->min_ctx, PPMD_REF(p, s));
   for (i = 0; i < 256; i++, s++)
   {
      s->symbol       = (uint8_t)i;
      s->freq         = 1;
      s->successor_lo = 0;
      s->successor_hi = 0;
   }

   for (i = 0; i < 128; i++)
      for (k = 0; k < 8; k++)
      {
         uint16_t val = (uint16_t)(PPMD_BIN_SCALE - ppmd_init_bin_esc[k] / (i + 2));
         for (m = 0; m < 64; m += 8)
            p->bin_summ[i][k + m] = val;
      }

   for (i = 0; i < 25; i++)
      for (k = 0; k < 16; k++)
      {
         rrar_ppmd7_see_t *see = &p->see[i][k];
         see->shift = PPMD_PERIOD_BITS - 4;
         see->summ  = (uint16_t)((5 * i + 10) << see->shift);
         see->count = 4;
      }
}

/* The contexts the symbol just found leads to, from the longest one that
 * already has it down to min_ctx. NULL when memory runs out. */
static ppmd_context_t *ppmd_create_successors(rrar_ppmd7_t *p, bool skip)
{
   ppmd_state_t   *ps[PPMD_MAX_ORDER];
   ppmd_state_t    up_state;
   ppmd_state_t   *s;
   ppmd_context_t *c         = p->min_ctx;
   uint32_t        up_branch = PPMD_SUCCESSOR(p->found);
   uint8_t         symbol    = p->found->symbol;
   unsigned        num_ps    = 0;

   if (!skip)
      ps[num_ps++] = p->found;

   while (c->suffix)
   {
      uint32_t successor;
      c = PPMD_SUFFIX(p, c);
      if (c->num_stats != 1)
         for (s = PPMD_STATS(p, c); s->symbol != symbol; s++);
      else
         s = PPMD_ONE_STATE(c);
      successor = PPMD_SUCCESSOR(s);
      if (successor != up_branch)
      {
         c = PPMD_CTX(p, successor);
         if (num_ps == 0)
            return c;
         break;
      }
      ps[num_ps++] = s;
   }

   up_state.symbol = *(const uint8_t *)PPMD_PTR(p, up_branch);
   PPMD_SET_SUCCESSOR(&up_state, up_branch + 1);

   if (c->num_stats == 1)
      up_state.freq = PPMD_ONE_STATE(c)->freq;
   else
   {
      uint32_t cf, s0;
      for (s = PPMD_STATS(p, c); s->symbol != up_state.symbol; s++);
      cf            = (uint32_t)s->freq - 1;
      s0            = (uint32_t)c->u.m.summ_freq - c->num_stats - cf;
      up_state.freq = (uint8_t)(1 + ((2 * cf <= s0)
            ? (5 * cf > s0)
            : ((2 * cf + 3 * s0 - 1) / (2 * s0))));
   }

   while (num_ps != 0)
   {
      ppmd_context_t *c1;
      if (p->hi_unit != p->lo_unit)
         c1 = (ppmd_context_t *)(p->hi_unit -= PPMD_UNIT_SIZE);
      else if (p->free_list[0] != 0)
         c1 = (ppmd_context_t *)ppmd_remove_node(p, 0);
      else if (!(c1 = (ppmd_context_t *)ppmd_alloc_units_rare(p, 0)))
         return NULL;
      c1->num_stats = 1;
      memcpy(PPMD_ONE_STATE(c1), &up_state, sizeof(up_state));
      c1->suffix    = PPMD_REF(p, c);
      PPMD_SET_SUCCESSOR(ps[--num_ps], PPMD_REF(p, c1));
      c             = c1;
   }

   return c;
}

static INLINE void ppmd_swap_states(ppmd_state_t *a, ppmd_state_t *b)
{
   ppmd_state_t t = *a;
   *a             = *b;
   *b             = t;
}

/* After a symbol: its count in the next shorter context, the contexts it
 * leads to, and it added to every context from max_ctx down to the one
 * it was found in. */
static void ppmd_update_model(rrar_ppmd7_t *p)
{
   ppmd_context_t *c;
   uint32_t successor;
   unsigned s0, ns;
   uint8_t  symbol      = p->found->symbol;
   uint8_t  found_freq  = p->found->freq;
   uint32_t f_successor = PPMD_SUCCESSOR(p->found);

   if (found_freq < PPMD_MAX_FREQ / 4 && p->min_ctx->suffix != 0)
   {
      c = PPMD_SUFFIX(p, p->min_ctx);
      if (c->num_stats == 1)
      {
         ppmd_state_t *s = PPMD_ONE_STATE(c);
         if (s->freq < 32)
            s->freq++;
      }
      else
      {
         ppmd_state_t *s = PPMD_STATS(p, c);
         if (s->symbol != symbol)
         {
            do
            {
               s++;
            } while (s->symbol != symbol);
            if (s[0].freq >= s[-1].freq)
            {
               ppmd_swap_states(&s[0], &s[-1]);
               s--;
            }
         }
         if (s->freq < PPMD_MAX_FREQ - 9)
         {
            s->freq     += 2;
            c->u.m.summ_freq += 2;
         }
      }
   }

   if (p->order_fall == 0)
   {
      p->min_ctx = p->max_ctx = ppmd_create_successors(p, true);
      if (!p->min_ctx)
      {
         ppmd_restart_model(p);
         return;
      }
      PPMD_SET_SUCCESSOR(p->found, PPMD_REF(p, p->min_ctx));
      return;
   }

   *p->text++ = symbol;
   successor  = PPMD_REF(p, p->text);
   if (p->text >= p->units_start)
   {
      ppmd_restart_model(p);
      return;
   }

   if (f_successor)
   {
      if (f_successor <= successor)
      {
         /* it leads into the text: make the contexts it means */
         ppmd_context_t *cs = ppmd_create_successors(p, false);
         if (!cs)
         {
            ppmd_restart_model(p);
            return;
         }
         f_successor = PPMD_REF(p, cs);
      }
      if (--p->order_fall == 0)
      {
         successor  = f_successor;
         p->text   -= (p->max_ctx != p->min_ctx);
      }
   }
   else
   {
      PPMD_SET_SUCCESSOR(p->found, successor);
      f_successor = PPMD_REF(p, p->min_ctx);
   }

   ns         = p->min_ctx->num_stats;
   s0         = p->min_ctx->u.m.summ_freq - ns - (found_freq - 1);

   for (c = p->max_ctx; c != p->min_ctx; c = PPMD_SUFFIX(p, c))
   {
      ppmd_state_t *s;
      uint32_t cf, sf;
      unsigned ns1 = c->num_stats;

      if (ns1 != 1)
      {
         if ((ns1 & 1) == 0)
         {
            /* the states fill their block: one unit more */
            unsigned old_nu = ns1 >> 1;
            unsigned i      = PPMD_U2I(p, old_nu);
            if (i != PPMD_U2I(p, old_nu + 1))
            {
               void *ptr = ppmd_alloc_units(p, i + 1);
               void *old_ptr;
               if (!ptr)
               {
                  ppmd_restart_model(p);
                  return;
               }
               old_ptr  = PPMD_STATS(p, c);
               memcpy(ptr, old_ptr, PPMD_U2B(old_nu));
               ppmd_insert_node(p, old_ptr, i);
               PPMD_SET_STATS(c, PPMD_REF(p, ptr));
            }
         }
         c->u.m.summ_freq = (uint16_t)(c->u.m.summ_freq + (2 * ns1 < ns)
               + 2 * ((4 * ns1 <= ns) & (c->u.m.summ_freq <= 8 * ns1)));
      }
      else
      {
         /* one state becomes a block of states */
         ppmd_state_t one;
         if (!(s = (ppmd_state_t *)ppmd_alloc_units(p, 0)))
         {
            ppmd_restart_model(p);
            return;
         }
         memcpy(&one, PPMD_ONE_STATE(c), sizeof(one));
         *s       = one;
         PPMD_SET_STATS(c, PPMD_REF(p, s));
         if (s->freq < PPMD_MAX_FREQ / 4 - 1)
            s->freq <<= 1;
         else
            s->freq   = PPMD_MAX_FREQ - 4;
         c->u.m.summ_freq = (uint16_t)(s->freq + p->init_esc + (ns > 3));
      }

      cf = 2 * (uint32_t)found_freq * (c->u.m.summ_freq + 6);
      sf = (uint32_t)s0 + c->u.m.summ_freq;
      if (cf < 6 * sf)
      {
         cf            = 1 + (cf > sf) + (cf >= 4 * sf);
         c->u.m.summ_freq += 3;
      }
      else
      {
         cf           = 4 + (cf >= 9 * sf) + (cf >= 12 * sf) + (cf >= 15 * sf);
         c->u.m.summ_freq = (uint16_t)(c->u.m.summ_freq + cf);
      }

      s            = PPMD_STATS(p, c) + ns1;
      PPMD_SET_SUCCESSOR(s, successor);
      s->symbol    = symbol;
      s->freq      = (uint8_t)cf;
      c->num_stats = (uint16_t)(ns1 + 1);
   }

   p->min_ctx = p->max_ctx = PPMD_CTX(p, f_successor);
}

/* Halve min_ctx's counts, the found state first, and drop the states
 * that reach zero. */
static void ppmd_rescale(rrar_ppmd7_t *p)
{
   unsigned i, adder, sum_freq, esc_freq;
   ppmd_context_t *mc  = p->min_ctx;
   ppmd_state_t *stats = PPMD_STATS(p, mc);
   ppmd_state_t *s     = p->found;

   {
      ppmd_state_t t = *s;
      for (; s != stats; s--)
         s[0] = s[-1];
      *s = t;
   }

   esc_freq  = mc->u.m.summ_freq - s->freq;
   s->freq  += 4;
   adder     = (p->order_fall != 0);
   s->freq   = (uint8_t)((s->freq + adder) >> 1);
   sum_freq  = s->freq;

   i = mc->num_stats - 1;
   do
   {
      esc_freq -= (++s)->freq;
      s->freq   = (uint8_t)((s->freq + adder) >> 1);
      sum_freq += s->freq;
      if (s[0].freq > s[-1].freq)
      {
         /* keep the states in order of their counts */
         ppmd_state_t *s1 = s;
         ppmd_state_t  t  = *s1;
         do
         {
            s1[0] = s1[-1];
         } while (--s1 != stats && t.freq > s1[-1].freq);
         *s1 = t;
      }
   } while (--i);

   if (s->freq == 0)
   {
      unsigned num_stats = mc->num_stats;
      unsigned n0, n1;
      do
      {
         i++;
      } while ((--s)->freq == 0);
      esc_freq     += i;
      mc->num_stats = (uint16_t)(mc->num_stats - i);
      if (mc->num_stats == 1)
      {
         ppmd_state_t t = *stats;
         do
         {
            t.freq     = (uint8_t)(t.freq - (t.freq >> 1));
            esc_freq >>= 1;
         } while (esc_freq > 1);
         ppmd_insert_node(p, stats, PPMD_U2I(p, (num_stats + 1) >> 1));
         p->found = PPMD_ONE_STATE(mc);
         memcpy(p->found, &t, sizeof(t));
         return;
      }
      n0 = (num_stats + 1) >> 1;
      n1 = (mc->num_stats + 1) >> 1;
      if (n0 != n1)
         PPMD_SET_STATS(mc, PPMD_REF(p, ppmd_shrink_units(p, stats, n0, n1)));
   }
   mc->u.m.summ_freq = (uint16_t)(sum_freq + esc_freq - (esc_freq >> 1));
   p->found      = PPMD_STATS(p, mc);
}

/* The escape estimate for min_ctx with @num_masked of its symbols
 * already ruled out, and the SEE context it came from. */
static rrar_ppmd7_see_t *ppmd_make_esc_freq(rrar_ppmd7_t *p,
      unsigned num_masked, uint32_t *esc_freq)
{
   rrar_ppmd7_see_t *see;
   ppmd_context_t *mc  = p->min_ctx;
   unsigned non_masked = mc->num_stats - num_masked;
   unsigned r;

   if (mc->num_stats == 256)
   {
      *esc_freq = 1;
      return &p->dummy_see;
   }

   see = p->see[p->ns2indx[non_masked - 1]]
       + (non_masked < (unsigned)PPMD_SUFFIX(p, mc)->num_stats - mc->num_stats)
       + 2 * (mc->u.m.summ_freq < 11 * mc->num_stats)
       + 4 * (num_masked > non_masked)
       + p->hi_bits_flag;
   r          = see->summ >> see->shift;
   see->summ  = (uint16_t)(see->summ - r);
   *esc_freq  = r + (r == 0);
   return see;
}

static INLINE void ppmd_see_update(rrar_ppmd7_see_t *see)
{
   if (see->shift < PPMD_PERIOD_BITS && --see->count == 0)
   {
      see->summ <<= 1;
      see->count  = (uint8_t)(3 << see->shift++);
   }
}

/* Where the model goes after a symbol: straight to the context the found
 * state leads to when nothing has to be learnt. */
static INLINE void ppmd_next_context(rrar_ppmd7_t *p)
{
   ppmd_context_t *c = PPMD_CTX(p, PPMD_SUCCESSOR(p->found));
   if (p->order_fall == 0 && (uint8_t *)c > p->text)
      p->min_ctx = p->max_ctx = c;
   else
      ppmd_update_model(p);
}

/* -------------------------------------------------------------- decoding */

/* After an escape from min_ctx: down the suffixes, leaving out the
 * symbols already ruled out, until one of the rest is the symbol. Out of
 * line, so that the common paths in rrar_ppmd7_decode_symbol carry
 * neither the mask nor the registers this needs. A ruled-out symbol's
 * mask is 0 and the rest are all ones, so a count is taken or left out
 * without a branch. */
static PPMD_NOINLINE int ppmd_decode_escape(rrar_ppmd7_t *p,
      rrar_ppmd7_range_t *rc)
{
   int8_t mask[256];
   ppmd_context_t *mc = p->min_ctx;
   ppmd_state_t *s;
   unsigned n;

   memset(mask, -1, sizeof(mask));
   if (mc->num_stats == 1)
      mask[PPMD_ONE_STATE(mc)->symbol] = 0;
   else
      for (s = PPMD_STATS(p, mc), n = mc->num_stats; n != 0; n--, s++)
         mask[s->symbol] = 0;

   for (;;)
   {
      rrar_ppmd7_see_t *see;
      uint32_t freq_sum, count, hi_cnt;
      unsigned num_masked = mc->num_stats;

      do
      {
         p->order_fall++;
         if (!mc->suffix)
            return -1;
         mc = PPMD_SUFFIX(p, mc);
      } while (mc->num_stats == num_masked);
      p->min_ctx = mc;

      hi_cnt = 0;
      s      = PPMD_STATS(p, mc);
      n      = mc->num_stats;
      if (n & 1)
      {
         hi_cnt = s->freq & (uint32_t)(int32_t)mask[s->symbol];
         s++;
      }
      for (n >>= 1; n != 0; n--, s += 2)
      {
         hi_cnt += s[0].freq & (uint32_t)(int32_t)mask[s[0].symbol];
         hi_cnt += s[1].freq & (uint32_t)(int32_t)mask[s[1].symbol];
      }

      see       = ppmd_make_esc_freq(p, num_masked, &freq_sum);
      freq_sum += hi_cnt;
      count     = ppmd_rc_threshold(rc, freq_sum);

      if (count < hi_cnt)
      {
         /* the first state whose running count passes the code */
         uint8_t symbol;
         int32_t left = (int32_t)count;
         s = PPMD_STATS(p, mc);
         for (;;)
         {
            left -= s->freq & (uint32_t)(int32_t)mask[s->symbol];
            if (left < 0)
               break;
            s++;
         }
         ppmd_rc_decode(rc, count - (uint32_t)left - s->freq, s->freq);
         ppmd_see_update(see);
         p->found       = s;
         symbol         = s->symbol;
         mc->u.m.summ_freq += 4;
         if ((s->freq += 4) > PPMD_MAX_FREQ)
            ppmd_rescale(p);
         p->run_length  = p->init_rl;
         ppmd_update_model(p);
         return symbol;
      }
      if (count >= freq_sum)
         return -2;
      ppmd_rc_decode(rc, hi_cnt, freq_sum - hi_cnt);
      see->summ = (uint16_t)(see->summ + freq_sum);

      for (s = PPMD_STATS(p, mc), n = mc->num_stats; n != 0; n--, s++)
         mask[s->symbol] = 0;
   }
}

int rrar_ppmd7_decode_symbol(rrar_ppmd7_t *p, rrar_ppmd7_range_t *rc)
{
   ppmd_context_t *mc = p->min_ctx;

   if (mc->num_stats != 1)
   {
      uint32_t count, hi_cnt;
      unsigned i;
      ppmd_state_t *s = PPMD_STATS(p, mc);
      count           = ppmd_rc_threshold(rc, mc->u.m.summ_freq);
      if (count < (hi_cnt = s->freq))
      {
         /* the most probable symbol */
         uint8_t symbol;
         ppmd_rc_decode(rc, 0, s->freq);
         p->found         = s;
         symbol           = s->symbol;
         p->prev_success  = (2 * (unsigned)s->freq > mc->u.m.summ_freq);
         p->run_length   += p->prev_success;
         mc->u.m.summ_freq   += 4;
         if ((s->freq += 4) > PPMD_MAX_FREQ)
            ppmd_rescale(p);
         ppmd_next_context(p);
         return symbol;
      }
      p->prev_success = 0;
      i = mc->num_stats - 1;
      do
      {
         if ((hi_cnt += (++s)->freq) > count)
         {
            uint8_t symbol;
            ppmd_rc_decode(rc, hi_cnt - s->freq, s->freq);
            p->found       = s;
            symbol         = s->symbol;
            s->freq       += 4;
            mc->u.m.summ_freq += 4;
            if (s[0].freq > s[-1].freq)
            {
               ppmd_swap_states(&s[0], &s[-1]);
               p->found = --s;
               if (s->freq > PPMD_MAX_FREQ)
                  ppmd_rescale(p);
            }
            ppmd_next_context(p);
            return symbol;
         }
      } while (--i);
      if (count >= mc->u.m.summ_freq)
         return -2;
      p->hi_bits_flag = p->hb2flag[p->found->symbol];
      ppmd_rc_decode(rc, hi_cnt, mc->u.m.summ_freq - hi_cnt);
   }
   else
   {
      ppmd_state_t *one = PPMD_ONE_STATE(mc);
      uint16_t *prob    = &p->bin_summ[one->freq - 1][
              p->prev_success
            + p->ns2bsindx[PPMD_SUFFIX(p, mc)->num_stats - 1]
            + (p->hi_bits_flag = p->hb2flag[p->found->symbol])
            + 2 * p->hb2flag[one->symbol]
            + ((p->run_length >> 26) & 0x20)];
      if (ppmd_rc_bit(rc, *prob) == 0)
      {
         uint8_t symbol;
         *prob           = (uint16_t)(*prob + (1 << PPMD_INT_BITS) - PPMD_GET_MEAN(*prob));
         p->found        = one;
         symbol          = one->symbol;
         one->freq       = (uint8_t)(one->freq + (one->freq < 128));
         p->prev_success = 1;
         p->run_length++;
         ppmd_next_context(p);
         return symbol;
      }
      *prob           = (uint16_t)(*prob - PPMD_GET_MEAN(*prob));
      p->init_esc     = ppmd_exp_escape[*prob >> 10];
      p->prev_success = 0;
   }

   return ppmd_decode_escape(p, rc);
}

/* ------------------------------------------------------------------- API */

void rrar_ppmd7_construct(rrar_ppmd7_t *p)
{
   unsigned i, k, m;

   p->base = NULL;
   p->size = 0;

   /* block sizes in units: 1 to 4 one apart, then two, three and four */
   for (i = 0, k = 0; i < PPMD_NUM_INDEXES; i++)
   {
      unsigned step = (i >= 12) ? 4 : (i >> 2) + 1;
      do
      {
         p->units2indx[k++] = (uint8_t)i;
      } while (--step);
      p->indx2units[i] = (uint8_t)k;
   }

   p->ns2bsindx[0] = 0 << 1;
   p->ns2bsindx[1] = 1 << 1;
   memset(p->ns2bsindx + 2,  2 << 1, 9);
   memset(p->ns2bsindx + 11, 3 << 1, 256 - 11);

   for (i = 0; i < 3; i++)
      p->ns2indx[i] = (uint8_t)i;
   for (m = i, k = 1; i < 256; i++)
   {
      p->ns2indx[i] = (uint8_t)m;
      if (--k == 0)
         k = ++m - 2;
   }

   memset(p->hb2flag, 0, 0x40);
   memset(p->hb2flag + 0x40, 8, 0x100 - 0x40);
}

void rrar_ppmd7_free(rrar_ppmd7_t *p)
{
   free(p->base);
   p->base = NULL;
   p->size = 0;
}

bool rrar_ppmd7_alloc(rrar_ppmd7_t *p, uint32_t size)
{
   if (p->base && p->size == size)
      return true;
   /* the root context takes a unit of its own */
   if (size < PPMD_UNIT_SIZE)
      return false;
   rrar_ppmd7_free(p);
   /* never 0, so no link into the model is 0; a spare unit past the end
    * heads the list of free blocks when they are glued */
   p->align_offset = 4 - (size & 3);
   if (!(p->base = (uint8_t *)malloc((size_t)p->align_offset + size
               + PPMD_UNIT_SIZE)))
      return false;
   p->size = size;
   return true;
}

void rrar_ppmd7_init(rrar_ppmd7_t *p, unsigned max_order)
{
   p->max_order       = max_order;
   ppmd_restart_model(p);
   p->dummy_see.shift = PPMD_PERIOD_BITS;
   p->dummy_see.summ  = 0;
   p->dummy_see.count = 64;
}

bool rrar_ppmd7_range_init(rrar_ppmd7_range_t *rc, const uint8_t *data,
      const uint8_t *end, uint32_t past)
{
   unsigned i;
   rc->p     = data;
   rc->end   = end;
   rc->past  = past;
   rc->low   = 0;
   rc->range = 0xFFFFFFFFu;
   rc->code  = 0;
   for (i = 0; i < 4; i++)
      rc->code = (rc->code << 8) | ppmd_rc_byte(rc);
   return rc->code < 0xFFFFFFFFu;
}
