/* inffast.c -- fast decoding
 * Copyright (C) 1995-2008, 2010, 2013 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zutil.h"
#include "inftrees.h"
#include "inflate.h"
#include "inffast.h"

#ifndef ASMINF

/*
   Decode literal, length, and distance codes and write out the resulting
   literal and match bytes until either not enough input or output is
   available, an end-of-block is encountered, or a data error is encountered.
   When large enough input and output buffers are supplied to inflate(), for
   example, a 16K input buffer and a 64K output buffer, more than 95% of the
   inflate execution time is spent in this routine.

   Entry assumptions:

   state->mode == LEN
   strm->avail_in >= 6
   strm->avail_out >= 258
   start >= strm->avail_out
   state->bits < 8

   On return, state->mode is one of:

   LEN -- ran out of enough output space or enough available input
   TYPE -- reached end of block code, inflate() to interpret next block
   BAD -- error in block data

Notes:

- The maximum input bits used by a length/distance pair is 15 bits for the
length code, 5 bits for the length extra, 15 bits for the distance code,
and 13 bits for the distance extra.  This totals 48 bits, or six bytes.
Therefore if strm->avail_in >= 6, then there is enough input to avoid
checking for available input while decoding.

- The maximum bytes that a single length/distance pair can output is 258
bytes, which is the maximum length that can be coded.  inflate_fast()
requires strm->avail_out >= 258 for each loop to avoid checking for
output space.
*/
void ZLIB_INTERNAL inflate_fast(z_streamp strm, unsigned start)
{
   code const *here;           /* retrieved table entry */
   unsigned op;                /* code bits, operation, extra bits, or */
   /*  window position, window bytes to copy */
   unsigned len;               /* match length, unused bytes */
   unsigned dist;              /* match distance */
   unsigned char FAR *from;    /* where to copy match from */

   /* copy state to local variables */
   struct inflate_state FAR *state = (struct inflate_state FAR *)strm->state;
   unsigned char FAR           *in = strm->next_in;
   unsigned char FAR         *last = in + (strm->avail_in - 5);
   unsigned char FAR         *out  = strm->next_out;
   unsigned char FAR         *beg  = out - (start - strm->avail_out);
   unsigned char FAR         *end  = out + (strm->avail_out - 257);
   unsigned                  wsize = state->wsize;
   unsigned                  whave = state->whave;
   unsigned                  wnext = state->wnext;
   unsigned char FAR       *window = state->window;
   unsigned long              hold = state->hold;
   unsigned                   bits = state->bits;
   code const FAR           *lcode = state->lencode;
   code const FAR           *dcode = state->distcode;
   unsigned                  lmask = (UINT32_C(1) << state->lenbits) - 1;
   unsigned                  dmask = (UINT32_C(1) << state->distbits) - 1;

   /* decode literals and length/distances until end-of-block or not enough
      input data or output space */
   do
   {
      if (bits < 15)
      {
         hold += (unsigned long)(*in++) << bits;
         bits += 8;
         hold += (unsigned long)(*in++) << bits;
         bits += 8;
      }
      here = lcode + (hold & lmask);
dolen:
      op = (unsigned)(here->bits);
      hold >>= op;
      bits -= op;
      op = (unsigned)(here->op);
      if (op == 0)                          /* literal */
         *out++ = (unsigned char)(here->val);
      else if (op & 16)                     /* length base */
      {
         len = (unsigned)(here->val);
         op &= 15;                           /* number of extra bits */
         if (op)
         {
            if (bits < op)
            {
               hold += (unsigned long)(*in++) << bits;
               bits += 8;
            }
            len   += (unsigned)hold & ((UINT32_C(1) << op) - 1);
            hold >>= op;
            bits  -= op;
         }

         if (bits < 15)
         {
            hold += (unsigned long)(*in++) << bits;
            bits += 8;
            hold += (unsigned long)(*in++) << bits;
            bits += 8;
         }

         here = dcode + (hold & dmask);

dodist:
         op = (unsigned)(here->bits);
         hold >>= op;
         bits -= op;
         op = (unsigned)(here->op);
         if (op & 16)                       /* distance base */
         {
            dist = (unsigned)(here->val);
            op &= 15;                       /* number of extra bits */
            if (bits < op)
            {
               hold += (unsigned long)(*in++) << bits;
               bits += 8;
               if (bits < op)
               {
                  hold += (unsigned long)(*in++) << bits;
                  bits += 8;
               }
            }
            dist  += (unsigned)hold & ((UINT32_C(1) << op) - 1);
            hold >>= op;
            bits  -= op;
            op     = (unsigned)(out - beg);     /* max distance in output */
            if (dist > op)                      /* see if copy from window */
            {
               op  = dist - op;                 /* distance back in window */
               if (op > whave)
               {
                  if (state->sane)
                  {
                     strm->msg =
                        (char *)"invalid distance too far back";
                     state->mode = BAD;
                     break;
                  }
               }
               from = window;
               if (wnext == 0)             /* very common case */
               {
                  from += wsize - op;
                  if (op < len)           /* some from window */
                  {
                     len -= op;
                     do {
                        *out++ = *from++;
                     } while (--op);
                     from = out - dist;  /* rest from output */
                  }
               }
               else if (wnext < op)        /* wrap around window */
               {
                  from += wsize + wnext - op;
                  op -= wnext;
                  if (op < len)           /* some from end of window */
                  {
                     len -= op;
                     do {
                        *out++ = *from++;
                     } while (--op);
                     from = window;
                     if (wnext < len)    /* some from start of window */
                     {
                        op = wnext;
                        len -= op;
                        do {
                           *out++ = *from++;
                        } while (--op);
                        from = out - dist;      /* rest from output */
                     }
                  }
               }
               else                        /* contiguous in window */
               {
                  from += wnext - op;
                  if (op < len)           /* some from window */
                  {
                     len -= op;
                     do {
                        *out++ = *from++;
                     } while (--op);
                     from = out - dist;  /* rest from output */
                  }
               }

               while (len > 2)
               {
                  *out++ = *from++;
                  *out++ = *from++;
                  *out++ = *from++;
                  len -= 3;
               }

               if (len)
               {
                  *out++ = *from++;
                  if (len > 1)
                     *out++ = *from++;
               }
            }
            else
            {
               from = out - dist;          /* copy direct from output */
               do {                        /* minimum length is three */
                  *out++ = *from++;
                  *out++ = *from++;
                  *out++ = *from++;
                  len -= 3;
               }while(len > 2);

               if (len)
               {
                  *out++ = *from++;
                  if (len > 1)
                     *out++ = *from++;
               }
            }
         }
         else if ((op & 64) == 0)            /* 2nd level distance code */
         {
            here = dcode + here->val + (hold & ((UINT32_C(1) << op) - 1));
            goto dodist;
         }
         else
         {
            strm->msg   = (char *)"invalid distance code";
            state->mode = BAD;
            break;
         }
      }
      else if ((op & 64) == 0)                /* 2nd level length code */
      {
         here = lcode + here->val + (hold & ((UINT32_C(1) << op) - 1));
         goto dolen;
      }
      else if (op & 32)                       /* end-of-block */
      {
         state->mode = TYPE;
         break;
      }
      else
      {
         strm->msg = (char *)"invalid literal/length code";
         state->mode = BAD;
         break;
      }
   } while (in < last && out < end);

   /* return unused bytes (on entry, bits < 8, so in won't go too far back) */
   len              = bits >> 3;
   in              -= len;
   bits            -= len << 3;
   hold            &= (UINT32_C(1) << bits) - 1;

   /* update state and return */
   strm->next_in    = in;
   strm->next_out   = out;
   strm->avail_in   = (unsigned)(in < last ? 5 + (last - in) : 5 - (in - last));
   strm->avail_out  = (unsigned)(out < end ?
         257 + (end - out) : 257 - (out - end));
   state->hold      = hold;
   state->bits      = bits;
}

/*
   inflate_fast() speedups that turned out slower (on a PowerPC G3 750CXe):
   - Using bit fields for code structure
   - Different op definition to avoid & for extra bits (do & for table bits)
   - Three separate decoding do-loops for direct, window, and wnext == 0
   - Special case for distance > 1 copies to do overlapped load and store copy
   - Explicit branch predictions (based on measured branch probabilities)
   - Deferring match copy and interspersed it with decoding subsequent codes
   - Swapping literal/length else
   - Swapping window/direct else
   - Larger unrolled copy loops (three is about right)
   - Moving len -= 3 statement into middle of loop
   */

#endif /* !ASMINF */
