/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rrar_archive.c).
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* The unpacking of RAR 2.9 data here - the block header and code tables,
 * the symbols of the LZ coder with their length and distance tables, how
 * a filter is read, which program it is, and what the standard filters
 * do - follows libarchive's archive_read_support_format_rar.c:
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * Copyright (c) 2011 Andres Mejia
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The unpacking of RAR 5 data - the block header, the code tables, the
 * symbols and how lengths and distances are made up from them, and the
 * four filters - follows libarchive's archive_read_support_format_rar5.c:
 *
 * Copyright (c) 2018 Grzegorz Antoniak (http://antoniak.org)
 * All rights reserved.
 *
 * (under the same two-clause licence as above)
 *
 * How it is put together is this file's own. libarchive unpacks through
 * a sliding window, a block at a time, and runs each filter when its
 * block has gone by. Here a member is unpacked whole into the buffer the
 * caller is given, and that buffer is the window: a match is a copy
 * from earlier in it, however far back. The filters are only noted as
 * they are met, and run once the member is all there - they change
 * bytes that later matches still have to copy from as they were.
 */

#include <stdlib.h>
#include <string.h>

#include <retro_inline.h>
#include <encodings/crc32.h>
#include <rar/rrar_archive.h>

#include "rrar_ppmd7.h"

/* ------------------------------------------------------------ container */

#define RAR_HEAD_MAIN      0x73
#define RAR_HEAD_FILE      0x74
#define RAR_HEAD_END       0x7b

#define RAR_MAIN_VOLUME    0x0001
#define RAR_MAIN_SOLID     0x0008
#define RAR_MAIN_PASSWORD  0x0080

#define RAR_FILE_SPLIT_BEFORE 0x0001
#define RAR_FILE_SPLIT_AFTER  0x0002
#define RAR_FILE_PASSWORD     0x0004
#define RAR_FILE_SOLID        0x0010
#define RAR_FILE_DIRECTORY    0x00e0   /* (the dictionary size bits, all set) */
#define RAR_FILE_LARGE        0x0100
#define RAR_FILE_UNICODE      0x0200

#define RAR_HEAD_ADD_SIZE     0x8000

#define RAR_METHOD_STORED     0x30

/* ------------------------------------------------------------- unpacking */

#define MAINCODE_SIZE       299
/* RAR 5's four codes, and the largest any code here has */
#define MAINCODE5_SIZE      306
#define OFFSETCODE5_SIZE    64
#define LOWOFFSETCODE5_SIZE 16
#define LENGTHCODE5_SIZE    44
#define HUFFMAN_TABLE5_SIZE (MAINCODE5_SIZE + OFFSETCODE5_SIZE + LOWOFFSETCODE5_SIZE + LENGTHCODE5_SIZE)
#define MAX_CODE_SIZE       MAINCODE5_SIZE
/* a RAR 5 filter's block may be this long */
#define FILTER5_BLOCK_MAX   0x400000
#define OFFSETCODE_SIZE     60
#define LOWOFFSETCODE_SIZE  17
#define LENGTHCODE_SIZE     28
#define HUFFMAN_TABLE_SIZE  (MAINCODE_SIZE + OFFSETCODE_SIZE + LOWOFFSETCODE_SIZE + LENGTHCODE_SIZE)
#define PRECODE_SIZE        20
#define MAX_CODE_LENGTH     15
#define QUICK_BITS          10

/* The memory a filter has: its block may be no longer. */
#define VM_MEMORY_SIZE      0x40000
#define FILTER_WORK_SIZE    0x3c000
#define FILTER_GLOBAL_MAX   (0x2000 - 0x40)

enum
{
   FILTER_DELTA = 1,
   FILTER_E8,
   FILTER_E8E9,
   FILTER_RGB,
   FILTER_AUDIO,
   /* RAR 5's: told by a number in the data, not by a program */
   FILTER5_DELTA,
   FILTER5_E8,
   FILTER5_E8E9,
   FILTER5_ARM
};

typedef struct rrar_huff
{
   uint16_t count[MAX_CODE_LENGTH + 1];
   uint16_t first[MAX_CODE_LENGTH + 1];   /* first code of each length */
   uint16_t index[MAX_CODE_LENGTH + 1];   /* and where its symbols start */
   uint32_t limit[MAX_CODE_LENGTH + 1];   /* one past the last, as 16 bits from the top */
   uint16_t symbol[MAX_CODE_SIZE];
   uint16_t quick[1 << QUICK_BITS];       /* length << 12 | symbol, 0: longer */
} rrar_huff_t;

/* Bits, the first of a byte first. */
typedef struct rrar_bits
{
   const uint8_t *p;
   const uint8_t *end;
   uint64_t       buf;     /* the bits to come, from the top */
   int            bits;    /* how many of them */
   uint32_t       past;    /* bytes made up after the end */
} rrar_bits_t;

typedef struct rrar_program
{
   uint32_t kind;          /* FILTER_* */
   uint32_t usage;
   uint32_t old_length;
} rrar_program_t;

typedef struct rrar_filter
{
   uint64_t start;         /* in the member */
   uint32_t length;
   uint32_t kind;
   uint32_t r0, r1, r4;    /* the registers the standard filters read */
} rrar_filter_t;

typedef struct rrar_unpack
{
   rrar_bits_t br;
   uint8_t    *out;
   uint64_t    pos;
   uint64_t    size;

   rrar_huff_t main_code;
   rrar_huff_t offset_code;
   rrar_huff_t low_offset_code;
   rrar_huff_t length_code;
   uint8_t     lengths[HUFFMAN_TABLE5_SIZE];

   uint32_t    old_offset[4];
   uint32_t    last_offset;
   uint32_t    last_length;
   uint32_t    last_low_offset;
   uint32_t    low_offset_repeats;

   int         is_ppmd;
   int         ppmd_valid;
   int         ppmd_escape;
   CPpmd7           ppmd;
   CPpmd7z_RangeDec range;
   IByteIn          byte_in;

   rrar_program_t *programs;
   uint32_t        num_programs;
   uint32_t        cap_programs;
   uint32_t        last_program;
   rrar_filter_t  *filters;
   uint32_t        num_filters;
   uint32_t        cap_filters;
} rrar_unpack_t;

struct rrar_archive
{
   const uint8_t *data;
   size_t         len;
   rrar_entry_t  *entries;
   char          *names;
   uint32_t       num_entries;
   uint32_t       cap_entries;
   size_t         names_len;
   size_t         names_cap;
};

static INLINE uint32_t rd16(const uint8_t *p)
{
   return (uint32_t)p[0] | ((uint32_t)p[1] << 8);
}

static INLINE uint32_t rd32(const uint8_t *p)
{
   return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static INLINE void wr32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)v;
   p[1] = (uint8_t)(v >> 8);
   p[2] = (uint8_t)(v >> 16);
   p[3] = (uint8_t)(v >> 24);
}

/* ------------------------------------------------------------------ bits */

static INLINE void br_fill(rrar_bits_t *br)
{
   if (br->bits > 32)
      return;
   if (br->end - br->p >= 4)
   {
      /* four bytes in one go, anywhere but at the very end */
      uint32_t w = ((uint32_t)br->p[0] << 24) | ((uint32_t)br->p[1] << 16)
                 | ((uint32_t)br->p[2] << 8)  |  (uint32_t)br->p[3];
      br->buf  |= (uint64_t)w << (32 - br->bits);
      br->p    += 4;
      br->bits += 32;
      return;
   }
   while (br->bits <= 56)
   {
      uint64_t byte = 0;
      if (br->p < br->end)
         byte = *br->p++;
      else
         br->past++;
      br->buf  |= byte << (56 - br->bits);
      br->bits += 8;
   }
}

/* The next @n bits, 1 to 32, without taking them. */
static INLINE uint32_t br_peek(rrar_bits_t *br, int n)
{
   br_fill(br);
   return (uint32_t)(br->buf >> (64 - n));
}

static INLINE void br_skip(rrar_bits_t *br, int n)
{
   br->buf  <<= n;
   br->bits  -= n;
}

static INLINE uint32_t br_get(rrar_bits_t *br, int n)
{
   uint32_t v = br_peek(br, n);
   br_skip(br, n);
   return v;
}

static uint32_t br_get32(rrar_bits_t *br)
{
   return br_get(br, 32);
}

/* To the start of the next byte. */
static void br_align(rrar_bits_t *br)
{
   br_skip(br, br->bits & 7);
}

/* More was read than the data had: up to 8 bytes are looked at ahead of
 * where the reading is, so that many past the end mean nothing yet. */
static INLINE int br_overrun(const rrar_bits_t *br)
{
   return br->past > 8;
}

/* --------------------------------------------------------------- Huffman */

/* The code RAR means by a list of lengths: codes of a length in the
 * order of their symbols, shorter lengths first. False if the lengths
 * are of no prefix code. */
static int huff_build(rrar_huff_t *h, const uint8_t *lengths, unsigned n)
{
   uint16_t next[MAX_CODE_LENGTH + 1];
   uint32_t code  = 0;
   uint32_t space = 1u << MAX_CODE_LENGTH;
   unsigned i, len, total = 0;

   memset(h->count, 0, sizeof(h->count));
   memset(h->quick, 0, sizeof(h->quick));
   for (i = 0; i < n; i++)
      h->count[lengths[i] & 15]++;
   h->count[0] = 0;

   for (len = 1; len <= MAX_CODE_LENGTH; len++)
   {
      uint32_t need = (uint32_t)h->count[len] << (MAX_CODE_LENGTH - len);
      if (need > space)
         return 0;
      space        -= need;
      h->first[len] = (uint16_t)code;
      h->index[len] = (uint16_t)total;
      next[len]     = (uint16_t)total;
      total        += h->count[len];
      code          = code + h->count[len];
      h->limit[len] = code << (16 - len);
      code        <<= 1;
   }

   for (i = 0; i < n; i++)
   {
      len = lengths[i] & 15;
      if (!len)
         continue;
      h->symbol[next[len]] = (uint16_t)i;
      if (len <= QUICK_BITS)
      {
         uint32_t c    = h->first[len] + (next[len] - h->index[len]);
         uint32_t from = c << (QUICK_BITS - len);
         uint32_t to   = from + (1u << (QUICK_BITS - len));
         for (; from < to; from++)
            h->quick[from] = (uint16_t)((len << 12) | i);
      }
      next[len]++;
   }
   return 1;
}

/* The next symbol, or -1 if the bits are no code of this table. */
static INLINE int huff_decode(rrar_bits_t *br, const rrar_huff_t *h)
{
   uint32_t v = br_peek(br, 16);
   uint32_t e = h->quick[v >> (16 - QUICK_BITS)];
   unsigned len;

   if (e)
   {
      br_skip(br, (int)(e >> 12));
      return (int)(e & 0xfff);
   }
   for (len = QUICK_BITS + 1; len <= MAX_CODE_LENGTH; len++)
   {
      if (v < h->limit[len])
      {
         unsigned i = h->index[len] + ((v >> (16 - len)) - h->first[len]);
         br_skip(br, (int)len);
         return h->symbol[i];
      }
   }
   return -1;
}

/* ------------------------------------------------------------------ PPMd */

static Byte ppmd_read(void *ud)
{
   return (Byte)br_get((rrar_bits_t *)ud, 8);
}

/* A byte of what the member says besides its data - a filter - by
 * whichever coder the block is in. */
static int unpack_byte(rrar_unpack_t *u)
{
   if (u->is_ppmd)
      return rrar_ppmd7_decode_symbol(&u->ppmd, &u->range);
   return (int)br_get(&u->br, 8);
}

/* ---------------------------------------------------------- block header */

/* Where a block starts: which coder it is in, and that coder's tables or
 * model. */
static int unpack_tables(rrar_unpack_t *u)
{
   rrar_bits_t *br = &u->br;
   uint8_t      pre_lengths[PRECODE_SIZE];
   rrar_huff_t *pre;
   int          i, r = RRAR_ERROR_DATA;

   br_align(br);

   u->is_ppmd = (int)br_get(br, 1);
   if (u->is_ppmd)
   {
      unsigned flags  = br_get(br, 7);
      unsigned mem_mb = 0;

      /* a new model: how much memory it has */
      if (flags & 0x20)
         mem_mb = br_get(br, 8) + 1;
      if (flags & 0x40)
         u->ppmd_escape = (int)br_get(br, 8);

      u->byte_in.ud   = br;
      u->byte_in.Read = ppmd_read;
      if (flags & 0x20)
      {
         unsigned order = (flags & 0x1f) + 1;
         if (order > 16)
            order = 16 + (order - 16) * 3;
         if (order == 1)
            return RRAR_ERROR_DATA;
         if (!rrar_ppmd7_alloc(&u->ppmd, mem_mb << 20))
            return RRAR_ERROR_MEM;
         if (!rrar_ppmd7_range_init(&u->range, &u->byte_in))
            return RRAR_ERROR_DATA;
         rrar_ppmd7_init(&u->ppmd, order);
         u->ppmd_valid = 1;
      }
      else
      {
         /* the model goes on from where the last PPMd block left it */
         if (!u->ppmd_valid)
            return RRAR_ERROR_DATA;
         if (!rrar_ppmd7_range_init(&u->range, &u->byte_in))
            return RRAR_ERROR_DATA;
      }
      return br_overrun(br) ? RRAR_ERROR_DATA : RRAR_OK;
   }

   /* (what the low distance bits repeat belongs to the tables) */
   u->last_low_offset    = 0;
   u->low_offset_repeats = 0;

   /* the lengths are differences from the last tables', unless told not */
   if (!br_get(br, 1))
      memset(u->lengths, 0, sizeof(u->lengths));

   memset(pre_lengths, 0, sizeof(pre_lengths));
   for (i = 0; i < PRECODE_SIZE;)
   {
      pre_lengths[i++] = (uint8_t)br_get(br, 4);
      if (pre_lengths[i - 1] == 0xf)
      {
         unsigned zeros = br_get(br, 4);
         if (zeros)
         {
            unsigned j;
            i--;
            for (j = 0; j < zeros + 2 && i < PRECODE_SIZE; j++)
               pre_lengths[i++] = 0;
         }
      }
   }

   if (!(pre = (rrar_huff_t *)malloc(sizeof(*pre))))
      return RRAR_ERROR_MEM;
   if (!huff_build(pre, pre_lengths, PRECODE_SIZE))
      goto done;

   for (i = 0; i < HUFFMAN_TABLE_SIZE;)
   {
      int val = huff_decode(br, pre);
      int n, j;

      if (val < 0 || br_overrun(br))
         goto done;
      if (val < 16)
      {
         u->lengths[i] = (uint8_t)((u->lengths[i] + val) & 0xf);
         i++;
      }
      else if (val < 18)
      {
         /* the last length again, a few or many times */
         if (i == 0)
            goto done;
         n = val == 16 ? (int)br_get(br, 3) + 3 : (int)br_get(br, 7) + 11;
         for (j = 0; j < n && i < HUFFMAN_TABLE_SIZE; j++, i++)
            u->lengths[i] = u->lengths[i - 1];
      }
      else
      {
         /* zeros */
         n = val == 18 ? (int)br_get(br, 3) + 3 : (int)br_get(br, 7) + 11;
         for (j = 0; j < n && i < HUFFMAN_TABLE_SIZE; j++)
            u->lengths[i++] = 0;
      }
   }

   if (     huff_build(&u->main_code, u->lengths, MAINCODE_SIZE)
         && huff_build(&u->offset_code, u->lengths + MAINCODE_SIZE, OFFSETCODE_SIZE)
         && huff_build(&u->low_offset_code,
               u->lengths + MAINCODE_SIZE + OFFSETCODE_SIZE, LOWOFFSETCODE_SIZE)
         && huff_build(&u->length_code,
               u->lengths + MAINCODE_SIZE + OFFSETCODE_SIZE + LOWOFFSETCODE_SIZE,
               LENGTHCODE_SIZE))
      r = RRAR_OK;

done:
   free(pre);
   return r;
}

/* --------------------------------------------------------------- filters */

/* A number as the filters' virtual machine writes them. */
static uint32_t vm_number(rrar_bits_t *br)
{
   uint32_t val;

   switch (br_get(br, 2))
   {
      case 0:
         return br_get(br, 4);
      case 1:
         val = br_get(br, 8);
         if (val >= 16)
            return val;
         return 0xffffff00u | (val << 4) | br_get(br, 4);
      case 2:
         return br_get(br, 16);
   }
   return br_get32(br);
}

/* Which of the standard filters a program is: they are told by their
 * length and checksum, and run as what they are known to do. */
static uint32_t filter_kind(const uint8_t *code, uint32_t len)
{
   uint32_t crc = encoding_crc32(0, code, len);

   if (len == 0x1d && crc == 0x0e06077du)
      return FILTER_DELTA;
   if (len == 0x35 && crc == 0xad576887u)
      return FILTER_E8;
   if (len == 0x39 && crc == 0x3cd7e57eu)
      return FILTER_E8E9;
   if (len == 0x95 && crc == 0x1c2c5dc8u)
      return FILTER_RGB;
   if (len == 0xd8 && crc == 0xbc85e701u)
      return FILTER_AUDIO;
   return 0;
}

/* A filter's description, @length bytes of it: which program (a new one
 * comes with its code), the block it is for, and its registers. */
static int filter_parse(rrar_unpack_t *u, const uint8_t *bytes, uint32_t length, unsigned flags)
{
   rrar_bits_t     br;
   rrar_program_t *prog = NULL;
   rrar_filter_t  *f;
   uint32_t        num, block_length, i;
   uint32_t        regs[7];
   uint64_t        block_start;

   memset(&br, 0, sizeof(br));
   br.p   = bytes;
   br.end = bytes + length;
   memset(regs, 0, sizeof(regs));

   if (flags & 0x80)
   {
      num = vm_number(&br);
      if (num == 0)
         u->num_programs = 0;    /* the programs so far are forgotten */
      else
         num--;
      if (num > u->num_programs)
         return RRAR_ERROR_DATA;
      u->last_program = num;
   }
   else
      num = u->last_program;

   if (num < u->num_programs)
   {
      prog = &u->programs[num];
      prog->usage++;
   }

   block_start = (uint64_t)vm_number(&br) + u->pos;
   if (flags & 0x40)
      block_start += 258;
   if (flags & 0x20)
      block_length = vm_number(&br);
   else
      block_length = prog ? prog->old_length : 0;

   regs[4] = block_length;
   if (flags & 0x10)
   {
      unsigned mask = br_get(&br, 7);
      for (i = 0; i < 7; i++)
         if (mask & (1u << i))
            regs[i] = vm_number(&br);
   }

   if (!prog)
   {
      uint32_t       len = vm_number(&br);
      uint8_t       *code;
      uint8_t        x = 0;
      uint32_t       kind;

      if (len == 0 || len > 0x10000 || br_overrun(&br))
         return RRAR_ERROR_DATA;
      if (!(code = (uint8_t *)malloc(len)))
         return RRAR_ERROR_MEM;
      for (i = 0; i < len; i++)
         code[i] = (uint8_t)br_get(&br, 8);
      for (i = 1; i < len; i++)
         x ^= code[i];
      kind = (x == code[0] && !br_overrun(&br)) ? filter_kind(code, len) : 0;
      i    = x == code[0];
      free(code);
      if (!i || br_overrun(&br))
         return RRAR_ERROR_DATA;
      if (!kind)
         return RRAR_ERROR_UNSUPPORTED;   /* a program of the archive's own */

      if (u->num_programs == u->cap_programs)
      {
         uint32_t cap = u->cap_programs ? u->cap_programs * 2 : 8;
         rrar_program_t *p = (rrar_program_t *)realloc(u->programs, cap * sizeof(*p));
         if (!p)
            return RRAR_ERROR_MEM;
         u->programs     = p;
         u->cap_programs = cap;
      }
      prog        = &u->programs[u->num_programs++];
      prog->kind  = kind;
      prog->usage = 0;
   }
   prog->old_length = block_length;

   if (flags & 0x08)
   {
      /* data of the program's own: the standard ones take none */
      uint32_t len = vm_number(&br);
      if (len > FILTER_GLOBAL_MAX)
         return RRAR_ERROR_DATA;
      for (i = 0; i < len; i++)
         br_get(&br, 8);
   }
   if (br_overrun(&br))
      return RRAR_ERROR_DATA;

   if (     block_length > VM_MEMORY_SIZE
         || block_start < u->pos
         || block_start > u->size
         || block_length > u->size - block_start)
      return RRAR_ERROR_DATA;

   if (u->num_filters == u->cap_filters)
   {
      uint32_t cap = u->cap_filters ? u->cap_filters * 2 : 64;
      rrar_filter_t *p = (rrar_filter_t *)realloc(u->filters, cap * sizeof(*p));
      if (!p)
         return RRAR_ERROR_MEM;
      u->filters     = p;
      u->cap_filters = cap;
   }
   f         = &u->filters[u->num_filters++];
   f->start  = block_start;
   f->length = block_length;
   f->kind   = prog->kind;
   f->r0     = regs[0];
   f->r1     = regs[1];
   f->r4     = regs[4];
   return RRAR_OK;
}

/* A filter in the data: a byte of flags, its length, and the description. */
static int filter_read(rrar_unpack_t *u)
{
   uint8_t *code;
   int      flags, v, r;
   uint32_t length, i;

   if ((flags = unpack_byte(u)) < 0)
      return RRAR_ERROR_DATA;
   length = ((unsigned)flags & 7) + 1;
   if (length == 7)
   {
      if ((v = unpack_byte(u)) < 0)
         return RRAR_ERROR_DATA;
      length = (uint32_t)v + 7;
   }
   else if (length == 8)
   {
      if ((v = unpack_byte(u)) < 0)
         return RRAR_ERROR_DATA;
      length = (uint32_t)v << 8;
      if ((v = unpack_byte(u)) < 0)
         return RRAR_ERROR_DATA;
      length |= (uint32_t)v;
   }
   if (!(code = (uint8_t *)malloc(length ? length : 1)))
      return RRAR_ERROR_MEM;
   for (i = 0; i < length; i++)
   {
      if ((v = unpack_byte(u)) < 0 || br_overrun(&u->br))
      {
         free(code);
         return RRAR_ERROR_DATA;
      }
      code[i] = (uint8_t)v;
   }
   r = filter_parse(u, code, length, (unsigned)flags);
   free(code);
   return r;
}

/* x86: the 32-bit addresses after CALL (and JMP), which were made
 * absolute for packing, are relative again. */
static void filter_e8(uint8_t *mem, uint32_t length, uint32_t pos, int e9_also)
{
   const uint32_t file_size = 0x1000000;
   uint32_t i;

   if (length <= 4)
      return;
   for (i = 0; i <= length - 5; i++)
   {
      if (mem[i] == 0xe8 || (e9_also && mem[i] == 0xe9))
      {
         uint32_t cur     = pos + i + 1;
         uint32_t address = rd32(mem + i + 1);

         if (address & 0x80000000u)
         {
            /* negative: within the file if no further back than here */
            if (cur >= (0u - address))
               wr32(mem + i + 1, address + file_size);
         }
         else if (address < file_size)
            wr32(mem + i + 1, address - cur);
         i += 4;
      }
   }
}

/* Bytes that were stored as differences from the byte @channels back. */
static void filter_delta(const uint8_t *src, uint8_t *dst, uint32_t length, uint32_t channels)
{
   uint32_t i, idx;

   for (i = 0; i < channels; i++)
   {
      uint8_t last = 0;
      for (idx = i; idx < length; idx += channels)
         last = dst[idx] = (uint8_t)(last - *src++);
   }
}

static INLINE uint32_t abs_diff(int v)
{
   return (uint32_t)(v < 0 ? -v : v);
}

/* A picture of 3 bytes to a pixel, @stride bytes to a row, stored as
 * what was left after predicting each byte from its neighbours. */
static void filter_rgb(const uint8_t *src, uint8_t *dst, uint32_t length,
      uint32_t stride, uint32_t byte_offset)
{
   uint32_t i, j;

   for (i = 0; i < 3; i++)
   {
      uint8_t byte = 0;
      for (j = i; j < length; j += 3)
      {
         if (j >= stride)
         {
            const uint8_t *prev = dst + j - stride;
            if (j - stride + 3 < length)
            {
               uint32_t d1 = abs_diff((int)prev[3] - prev[0]);
               uint32_t d2 = abs_diff((int)byte - prev[0]);
               uint32_t d3 = abs_diff((int)prev[3] - prev[0] + byte - prev[0]);
               if (d1 > d2 || d1 > d3)
                  byte = d2 <= d3 ? prev[3] : prev[0];
            }
         }
         byte  -= *src++;
         dst[j] = byte;
      }
   }
   for (i = byte_offset; i + 2 < length; i += 3)
   {
      dst[i]     = (uint8_t)(dst[i] + dst[i + 1]);
      dst[i + 2] = (uint8_t)(dst[i + 2] + dst[i + 1]);
   }
}

/* Sound, @channels of it, each byte stored as its difference from a
 * prediction that adapts as it goes. */
static void filter_audio(const uint8_t *src, uint8_t *dst, uint32_t length, uint32_t channels)
{
   uint32_t i, j;

   for (i = 0; i < channels; i++)
   {
      int8_t   weight[5];
      int16_t  delta[4];
      int8_t   last_delta = 0;
      int      error[11];
      uint32_t count = 0;
      uint8_t  last_byte = 0;

      memset(weight, 0, sizeof(weight));
      memset(delta, 0, sizeof(delta));
      memset(error, 0, sizeof(error));

      for (j = i; j < length; j += channels)
      {
         int8_t  d = (int8_t)*src++;
         uint8_t pred, byte;
         int     pred_error;

         delta[2] = delta[1];
         delta[1] = (int16_t)(last_delta - delta[0]);
         delta[0] = last_delta;
         pred     = (uint8_t)(((8 * last_byte + weight[0] * delta[0]
                     + weight[1] * delta[1] + weight[2] * delta[2]) >> 3) & 0xff);
         byte     = (uint8_t)((pred - d) & 0xff);

         pred_error = d * 8;
         error[0]  += (int)abs_diff(pred_error);
         error[1]  += (int)abs_diff(pred_error - delta[0]);
         error[2]  += (int)abs_diff(pred_error + delta[0]);
         error[3]  += (int)abs_diff(pred_error - delta[1]);
         error[4]  += (int)abs_diff(pred_error + delta[1]);
         error[5]  += (int)abs_diff(pred_error - delta[2]);
         error[6]  += (int)abs_diff(pred_error + delta[2]);

         last_delta = (int8_t)(byte - last_byte);
         dst[j]     = last_byte = byte;

         if (!(count++ & 0x1f))
         {
            unsigned k, best = 0;
            for (k = 1; k < 7; k++)
               if (error[k] < error[best])
                  best = k;
            memset(error, 0, sizeof(error));
            switch (best)
            {
               case 1: if (weight[0] >= -16) weight[0]--; break;
               case 2: if (weight[0] < 16)   weight[0]++; break;
               case 3: if (weight[1] >= -16) weight[1]--; break;
               case 4: if (weight[1] < 16)   weight[1]++; break;
               case 5: if (weight[2] >= -16) weight[2]--; break;
               case 6: if (weight[2] < 16)   weight[2]++; break;
            }
         }
      }
   }
}

/* RAR 5's x86 filter: as RAR 2.9's, with the position taken within 16 MB. */
static void filter5_e8(uint8_t *mem, uint32_t length, uint64_t start, int e9_also)
{
   const uint32_t file_size = 0x1000000;
   uint32_t i;

   for (i = 0; i + 4 < length;)
   {
      uint8_t b = mem[i++];
      if (b == 0xe8 || (e9_also && b == 0xe9))
      {
         uint32_t offset  = (uint32_t)((start + i) % file_size);
         uint32_t address = rd32(mem + i);

         if (address & 0x80000000u)
         {
            if (!((address + offset) & 0x80000000u))
               wr32(mem + i, address + file_size);
         }
         else if ((address - file_size) & 0x80000000u)
            wr32(mem + i, address - offset);
         i += 4;
      }
   }
}

/* ARM: the 24-bit word addresses after BL are relative again. */
static void filter5_arm(uint8_t *mem, uint32_t length, uint64_t start)
{
   uint32_t i;

   for (i = 0; i + 3 < length; i += 4)
   {
      if (mem[i + 3] == 0xeb)
      {
         uint32_t offset = rd32(mem + i) & 0x00ffffffu;
         offset -= (uint32_t)((start + i) / 4);
         wr32(mem + i, (offset & 0x00ffffffu) | 0xeb000000u);
      }
   }
}

/* The member is all unpacked: its filters, in the order they came. Two
 * for the same block are one after the other on it. */
static int filters_run(rrar_unpack_t *u)
{
   uint8_t *work = NULL;
   uint32_t n;
   int      r = RRAR_OK;

   for (n = 0; n < u->num_filters && r == RRAR_OK; n++)
   {
      const rrar_filter_t *f = &u->filters[n];
      uint8_t  *block  = u->out + (size_t)f->start;
      uint32_t  length = f->length;

      if (f->kind >= FILTER5_DELTA)
      {
         /* RAR 5's: their blocks are up to 4 MB, and nothing but the
          * delta filter needs a second copy */
         if (f->kind == FILTER5_E8 || f->kind == FILTER5_E8E9)
            filter5_e8(block, length, f->start, f->kind == FILTER5_E8E9);
         else if (f->kind == FILTER5_ARM)
            filter5_arm(block, length, f->start);
         else
         {
            uint8_t *copy = (uint8_t *)malloc(length ? length : 1);
            if (!copy)
            {
               r = RRAR_ERROR_MEM;
               break;
            }
            filter_delta(block, copy, length, f->r0);
            memcpy(block, copy, length);
            free(copy);
         }
         continue;
      }

      /* (checked when it was read against the member's size; a filter
       * whose result is not its block's length would move everything
       * after it) */
      if (f->r4 != length)
      {
         r = RRAR_ERROR_DATA;
         break;
      }
      if (f->kind == FILTER_E8 || f->kind == FILTER_E8E9)
      {
         if (length > FILTER_WORK_SIZE)
            r = RRAR_ERROR_DATA;
         else
            filter_e8(block, length, (uint32_t)f->start, f->kind == FILTER_E8E9);
         continue;
      }

      if (length > FILTER_WORK_SIZE / 2)
      {
         r = RRAR_ERROR_DATA;
         break;
      }
      if (!work && !(work = (uint8_t *)malloc(FILTER_WORK_SIZE / 2)))
      {
         r = RRAR_ERROR_MEM;
         break;
      }
      switch (f->kind)
      {
         case FILTER_DELTA:
            if (f->r0 == 0 || f->r0 > 128)
               r = RRAR_ERROR_DATA;
            else
               filter_delta(block, work, length, f->r0);
            break;
         case FILTER_RGB:
            if (f->r0 > length || length < 3 || f->r1 > 2)
               r = RRAR_ERROR_DATA;
            else
            {
               /* (a row no wider than a pixel is predicted from bytes
                * not written yet: let them be something) */
               memset(work, 0, length);
               filter_rgb(block, work, length, f->r0, f->r1);
            }
            break;
         case FILTER_AUDIO:
            if (f->r0 == 0 || f->r0 > 128)
               r = RRAR_ERROR_DATA;
            else
               filter_audio(block, work, length, f->r0);
            break;
         default:
            r = RRAR_ERROR_UNSUPPORTED;
            break;
      }
      if (r == RRAR_OK)
         memcpy(block, work, length);
   }
   free(work);
   return r;
}

/* ------------------------------------------------------------ the member */

static INLINE void emit_literal(rrar_unpack_t *u, uint8_t byte)
{
   u->out[(size_t)u->pos++] = byte;
}

/* A match that reaches back before the member's start, or to no
 * distance at all: what is not there is zeros. */
static size_t copy_match_edge(uint8_t *out, size_t pos, uint32_t offset, uint32_t length)
{
   uint8_t *dst = out + pos;

   if (offset == 0)
      memset(dst, 0, length);
   else
   {
      size_t   before = offset - pos;
      uint32_t zeros  = before < length ? (uint32_t)before : length;
      uint32_t i;

      memset(dst, 0, zeros);
      /* (pos + i is at least offset from here on) */
      for (i = zeros; i < length; i++)
         dst[i] = out[pos + i - offset];
   }
   return pos + length;
}

/* @length bytes from @offset back, to @out at @pos. Past the member's end
 * they are not written. The position after them. */
static INLINE size_t copy_match(uint8_t *out, size_t pos, size_t size,
      uint32_t offset, uint32_t length)
{
   uint8_t       *dst;
   const uint8_t *src;
   size_t         next;

   if (length > size - pos)
      length = (uint32_t)(size - pos);
   if (offset == 0 || offset > pos)
      return copy_match_edge(out, pos, offset, length);
   next = pos + length;

   dst = out + pos;
   src = dst - offset;
   if (offset >= 8 && size - pos >= (size_t)length + 8)
   {
      /* eight bytes at a time; the few too many are written over by
       * what comes next */
      uint8_t *end = dst + length;
      do
      {
         memcpy(dst, src, 8);
         dst += 8;
         src += 8;
      } while (dst < end);
   }
   else if (offset >= length)
      memcpy(dst, src, length);
   else
      while (length--)
         *dst++ = *src++;
   return next;
}

static void emit_match(rrar_unpack_t *u, uint32_t offset, uint32_t length)
{
   u->pos = copy_match(u->out, (size_t)u->pos, (size_t)u->size, offset, length);
}

/* What an LZ block's symbols stand for. */
#define LZ_END_OF_FILE  1
#define LZ_NEW_TABLE    2

/* The symbols of an LZ block, until it is over - new tables, or the end
 * of the file - or the member is full. The reading and the writing are
 * kept in local variables: a store of a byte through the state's
 * pointer would have the compiler read every field of the state again
 * after it. Returns LZ_END_OF_FILE, LZ_NEW_TABLE, RRAR_OK (the member is
 * full) or an error. */
static int unpack_lz(rrar_unpack_t *u)
{
   static const uint8_t length_bases[] =
      {   0,   1,   2,   3,   4,   5,   6,
          7,   8,  10,  12,  14,  16,  20,
         24,  28,  32,  40,  48,  56,  64,
         80,  96, 112, 128, 160, 192, 224 };
   static const uint8_t length_bits[] =
      { 0, 0, 0, 0, 0, 0, 0,
        0, 1, 1, 1, 1, 2, 2,
        2, 2, 3, 3, 3, 3, 4,
        4, 4, 4, 5, 5, 5, 5 };
   static const uint32_t offset_bases[] =
      {       0,       1,       2,       3,       4,       6,
              8,      12,      16,      24,      32,      48,
             64,      96,     128,     192,     256,     384,
            512,     768,    1024,    1536,    2048,    3072,
           4096,    6144,    8192,   12288,   16384,   24576,
          32768,   49152,   65536,   98304,  131072,  196608,
         262144,  327680,  393216,  458752,  524288,  589824,
         655360,  720896,  786432,  851968,  917504,  983040,
        1048576, 1310720, 1572864, 1835008, 2097152, 2359296,
        2621440, 2883584, 3145728, 3407872, 3670016, 3932160 };
   static const uint8_t offset_bits[] =
      {  0,  0,  0,  0,  1,  1,  2,  2,  3,  3,  4,  4,
         5,  5,  6,  6,  7,  7,  8,  8,  9,  9, 10, 10,
        11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16,
        16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
        18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18 };
   static const uint8_t short_bases[] = { 0, 4, 8, 16, 32, 64, 128, 192 };
   static const uint8_t short_bits[]  = { 2, 2, 3, 4, 5, 6, 6, 6 };
   rrar_bits_t br   = u->br;
   uint8_t    *out  = u->out;
   size_t      pos  = (size_t)u->pos;
   size_t      size = (size_t)u->size;
   int         r    = RRAR_OK;

   while (pos < size)
   {
      int      symbol = huff_decode(&br, &u->main_code);
      uint32_t offs, len;
      int      i;

      if (symbol < 256)
      {
         if (symbol < 0)
         {
            r = RRAR_ERROR_DATA;
            break;
         }
         out[pos++] = (uint8_t)symbol;
         continue;
      }
      /* (bytes are made up past the end of the data: an archive that is
       * cut short is seen here, or by the caller) */
      if (br_overrun(&br))
      {
         r = RRAR_ERROR_DATA;
         break;
      }
      if (symbol == 256)
      {
         /* the end of the block: of the file too, or new tables */
         r = br_get(&br, 1) ? LZ_NEW_TABLE : LZ_END_OF_FILE;
         break;
      }
      if (symbol == 257)
      {
         u->br  = br;
         u->pos = pos;
         r      = filter_read(u);
         br     = u->br;
         if (r != RRAR_OK)
            break;
         continue;
      }
      if (symbol == 258)
      {
         /* the last match again */
         if (u->last_length == 0)
            continue;
         offs = u->last_offset;
         len  = u->last_length;
      }
      else if (symbol <= 262)
      {
         /* one of the last four distances */
         int idx = symbol - 259;
         int len_symbol;

         offs = u->old_offset[idx];
         if ((len_symbol = huff_decode(&br, &u->length_code)) < 0
               || len_symbol >= (int)sizeof(length_bases))
         {
            r = RRAR_ERROR_DATA;
            break;
         }
         len = length_bases[len_symbol] + 2;
         if (length_bits[len_symbol])
            len += br_get(&br, length_bits[len_symbol]);
         for (i = idx; i > 0; i--)
            u->old_offset[i] = u->old_offset[i - 1];
         u->old_offset[0] = offs;
      }
      else if (symbol <= 270)
      {
         /* two bytes from close by */
         offs = short_bases[symbol - 263] + 1;
         if (short_bits[symbol - 263])
            offs += br_get(&br, short_bits[symbol - 263]);
         len = 2;
         for (i = 3; i > 0; i--)
            u->old_offset[i] = u->old_offset[i - 1];
         u->old_offset[0] = offs;
      }
      else
      {
         int offs_symbol;

         if (symbol - 271 >= (int)sizeof(length_bases))
         {
            r = RRAR_ERROR_DATA;
            break;
         }
         len = length_bases[symbol - 271] + 3;
         if (length_bits[symbol - 271])
            len += br_get(&br, length_bits[symbol - 271]);

         if ((offs_symbol = huff_decode(&br, &u->offset_code)) < 0
               || offs_symbol >= (int)(sizeof(offset_bases) / sizeof(offset_bases[0])))
         {
            r = RRAR_ERROR_DATA;
            break;
         }
         offs = offset_bases[offs_symbol] + 1;
         if (offset_bits[offs_symbol])
         {
            if (offs_symbol > 9)
            {
               /* the low four bits have a code of their own, and repeat */
               if (offset_bits[offs_symbol] > 4)
                  offs += br_get(&br, offset_bits[offs_symbol] - 4) << 4;
               if (u->low_offset_repeats)
               {
                  u->low_offset_repeats--;
                  offs += u->last_low_offset;
               }
               else
               {
                  int low = huff_decode(&br, &u->low_offset_code);
                  if (low < 0)
                  {
                     r = RRAR_ERROR_DATA;
                     break;
                  }
                  if (low == 16)
                  {
                     u->low_offset_repeats = 15;
                     offs += u->last_low_offset;
                  }
                  else
                  {
                     offs += (uint32_t)low;
                     u->last_low_offset = (uint32_t)low;
                  }
               }
            }
            else
               offs += br_get(&br, offset_bits[offs_symbol]);
         }
         if (offs >= 0x40000)
            len++;
         if (offs >= 0x2000)
            len++;
         for (i = 3; i > 0; i--)
            u->old_offset[i] = u->old_offset[i - 1];
         u->old_offset[0] = offs;
      }
      u->last_offset = offs;
      u->last_length = len;
      pos = copy_match(out, pos, size, offs, len);
   }

   u->br  = br;
   u->pos = pos;
   if (r == RRAR_OK && br_overrun(&br))
      r = RRAR_ERROR_DATA;
   return r;
}

/* RAR 2.9: blocks of LZ symbols and blocks of PPMd, with filters among
 * them, to the member's size or the end of its data. */
static int unpack29(rrar_unpack_t *u)
{
   int new_table = 1;
   int r;

   while (u->pos < u->size)
   {
      int      symbol, code, i;
      uint32_t offs;

      if (br_overrun(&u->br))
         return RRAR_ERROR_DATA;
      if (new_table)
      {
         if ((r = unpack_tables(u)) != RRAR_OK)
            return r;
         new_table = 0;
      }

      if (!u->is_ppmd)
      {
         r = unpack_lz(u);
         if (r == LZ_NEW_TABLE)
            new_table = 1;
         else
            return r == LZ_END_OF_FILE ? RRAR_OK : r;
         continue;
      }

      if ((symbol = rrar_ppmd7_decode_symbol(&u->ppmd, &u->range)) < 0)
         return RRAR_ERROR_DATA;
      if (symbol != u->ppmd_escape)
      {
         emit_literal(u, (uint8_t)symbol);
         continue;
      }
      /* the escape byte: what comes after says what is meant */
      if ((code = rrar_ppmd7_decode_symbol(&u->ppmd, &u->range)) < 0)
         return RRAR_ERROR_DATA;
      switch (code)
      {
         case 0:     /* a new block */
            new_table = 1;
            break;
         case 2:     /* the end of the data */
            return RRAR_OK;
         case 3:
            if ((r = filter_read(u)) != RRAR_OK)
               return r;
            break;
         case 4:     /* a match: three bytes of distance, one of length */
            offs = 0;
            for (i = 2; i >= 0; i--)
            {
               if ((code = rrar_ppmd7_decode_symbol(&u->ppmd, &u->range)) < 0)
                  return RRAR_ERROR_DATA;
               offs |= (uint32_t)code << (i * 8);
            }
            if ((code = rrar_ppmd7_decode_symbol(&u->ppmd, &u->range)) < 0)
               return RRAR_ERROR_DATA;
            emit_match(u, offs + 2, (uint32_t)code + 32);
            break;
         case 5:     /* the last byte again */
            if ((code = rrar_ppmd7_decode_symbol(&u->ppmd, &u->range)) < 0)
               return RRAR_ERROR_DATA;
            emit_match(u, 1, (uint32_t)code + 4);
            break;
         default:    /* the escape byte itself */
            emit_literal(u, (uint8_t)symbol);
            break;
      }
   }
   return RRAR_OK;
}

/* ----------------------------------------------------------------- RAR 5 */

/* A length, from its code: the first eight are themselves, the rest a
 * few bits more each. */
static INLINE uint32_t length5(rrar_bits_t *br, unsigned slot)
{
   uint32_t length = 2;
   unsigned bits   = 0;

   if (slot < 8)
      length += slot;
   else
   {
      bits    = slot / 4 - 1;
      length += (uint32_t)(4 | (slot & 3)) << bits;
   }
   if (bits)
      length += br_get(br, (int)bits);
   return length;
}

/* A number in a filter's description: one to four bytes, the low one
 * first. */
static uint32_t filter5_number(rrar_bits_t *br)
{
   unsigned bytes = br_get(br, 2) + 1;
   uint32_t v = 0;
   unsigned i;

   for (i = 0; i < bytes; i++)
      v += br_get(br, 8) << (i * 8);
   return v;
}

/* The code tables a block starts with: the lengths of a code of 20,
 * which the lengths of the four others are written in. Unlike RAR
 * 2.9's they are whole lengths, not differences from the last. */
static int unpack5_tables(rrar_unpack_t *u, rrar_bits_t *br)
{
   uint8_t      pre_lengths[PRECODE_SIZE];
   rrar_huff_t *pre;
   int          i, r = RRAR_ERROR_DATA;

   for (i = 0; i < PRECODE_SIZE;)
   {
      unsigned v = br_get(br, 4);
      if (v == 15)
      {
         unsigned zeros = br_get(br, 4);
         if (!zeros)
            pre_lengths[i++] = 15;
         else
         {
            unsigned j;
            for (j = 0; j < zeros + 2 && i < PRECODE_SIZE; j++)
               pre_lengths[i++] = 0;
         }
      }
      else
         pre_lengths[i++] = (uint8_t)v;
   }

   if (!(pre = (rrar_huff_t *)malloc(sizeof(*pre))))
      return RRAR_ERROR_MEM;
   if (!huff_build(pre, pre_lengths, PRECODE_SIZE))
      goto done;

   for (i = 0; i < HUFFMAN_TABLE5_SIZE;)
   {
      int val = huff_decode(br, pre);
      int n, j;

      if (val < 0 || br_overrun(br))
         goto done;
      if (val < 16)
         u->lengths[i++] = (uint8_t)val;
      else if (val < 18)
      {
         if (i == 0)
            goto done;
         n = val == 16 ? (int)br_get(br, 3) + 3 : (int)br_get(br, 7) + 11;
         for (j = 0; j < n && i < HUFFMAN_TABLE5_SIZE; j++, i++)
            u->lengths[i] = u->lengths[i - 1];
      }
      else
      {
         n = val == 18 ? (int)br_get(br, 3) + 3 : (int)br_get(br, 7) + 11;
         for (j = 0; j < n && i < HUFFMAN_TABLE5_SIZE; j++)
            u->lengths[i++] = 0;
      }
   }

   if (     huff_build(&u->main_code, u->lengths, MAINCODE5_SIZE)
         && huff_build(&u->offset_code, u->lengths + MAINCODE5_SIZE, OFFSETCODE5_SIZE)
         && huff_build(&u->low_offset_code,
               u->lengths + MAINCODE5_SIZE + OFFSETCODE5_SIZE, LOWOFFSETCODE5_SIZE)
         && huff_build(&u->length_code,
               u->lengths + MAINCODE5_SIZE + OFFSETCODE5_SIZE + LOWOFFSETCODE5_SIZE,
               LENGTHCODE5_SIZE))
      r = RRAR_OK;

done:
   free(pre);
   return r;
}

/* A filter in the data: where its block starts from here, how long it
 * is, and which of four it is. */
static int unpack5_filter(rrar_unpack_t *u, rrar_bits_t *br, size_t pos)
{
   uint32_t       start  = filter5_number(br);
   uint32_t       length = filter5_number(br);
   unsigned       type   = br_get(br, 3);
   uint32_t       channels = type == 0 ? br_get(br, 5) + 1 : 0;
   uint64_t       block  = (uint64_t)pos + start;
   rrar_filter_t *f;

   if (br_overrun(br) || length < 4 || length > FILTER5_BLOCK_MAX)
      return RRAR_ERROR_DATA;
   if (type > 3)
      return RRAR_ERROR_UNSUPPORTED;
   /* within the member, and after the last filter's block */
   if (block > u->size || length > u->size - block)
      return RRAR_ERROR_DATA;
   if (u->num_filters)
   {
      const rrar_filter_t *last = &u->filters[u->num_filters - 1];
      if (block < last->start + last->length)
         return RRAR_ERROR_DATA;
   }

   if (u->num_filters == u->cap_filters)
   {
      uint32_t cap = u->cap_filters ? u->cap_filters * 2 : 64;
      rrar_filter_t *p = (rrar_filter_t *)realloc(u->filters, cap * sizeof(*p));
      if (!p)
         return RRAR_ERROR_MEM;
      u->filters     = p;
      u->cap_filters = cap;
   }
   f         = &u->filters[u->num_filters++];
   f->start  = block;
   f->length = length;
   f->kind   = FILTER5_DELTA + type;
   f->r0     = channels;
   f->r1     = 0;
   f->r4     = length;
   return RRAR_OK;
}

/* RAR 5: blocks, each with a header of its own length in bytes and bits,
 * of the symbols of one LZ coder - there is no PPMd, and the filters are
 * four fixed ones. */
static int unpack50(rrar_unpack_t *u)
{
   const uint8_t *p   = u->br.p;
   const uint8_t *end = u->br.end;
   uint8_t       *out  = u->out;
   size_t         pos  = 0;
   size_t         size = (size_t)u->size;
   int            have_tables = 0;
   int            r;

   while (pos < size)
   {
      rrar_bits_t    br;
      const uint8_t *data;
      unsigned       flags, count, i;
      uint32_t       block_size = 0;
      uint64_t       end_bits;
      uint8_t        sum;
      int            block_done = 0;

      /* the block's header: flags, a checksum, and its size */
      if (end - p < 3)
         return RRAR_ERROR_DATA;
      flags = p[0];
      count = ((flags >> 3) & 7) + 1;
      if (count > 3 || (size_t)(end - p) < 2 + count)
         return RRAR_ERROR_DATA;
      sum = (uint8_t)(0x5a ^ flags);
      for (i = 0; i < count; i++)
      {
         block_size |= (uint32_t)p[2 + i] << (i * 8);
         sum        ^= p[2 + i];
      }
      if (sum != p[1])
         return RRAR_ERROR_DATA;
      data = p + 2 + count;
      if (block_size > (size_t)(end - data))
         return RRAR_ERROR_DATA;
      /* the last byte's bits are not all the block's */
      end_bits = block_size ? (uint64_t)(block_size - 1) * 8 + (flags & 7) + 1 : 0;

      memset(&br, 0, sizeof(br));
      br.p   = data;
      br.end = data + block_size;

      if (flags & 0x80)
      {
         if ((r = unpack5_tables(u, &br)) != RRAR_OK)
            return r;
         have_tables = 1;
      }
      else if (!have_tables)
         return RRAR_ERROR_DATA;

      while (pos < size)
      {
         int      symbol;
         uint32_t len;
         uint64_t dist;

         /* (the bits read ahead are at most 8 bytes: further than that
          * from the block's end, it has not been reached) */
         if (br.end - br.p <= 8
               && (uint64_t)(br.p - data) * 8 + (uint64_t)br.past * 8 - (uint64_t)br.bits >= end_bits)
         {
            block_done = 1;
            break;
         }

         symbol = huff_decode(&br, &u->main_code);
         if (symbol < 256)
         {
            if (symbol < 0)
               return RRAR_ERROR_DATA;
            out[pos++] = (uint8_t)symbol;
            continue;
         }
         if (symbol >= 262)
         {
            int      slot;
            unsigned bits;

            len = length5(&br, (unsigned)symbol - 262);
            if ((slot = huff_decode(&br, &u->offset_code)) < 0)
               return RRAR_ERROR_DATA;
            dist = 1;
            if (slot < 4)
            {
               bits  = 0;
               dist += (unsigned)slot;
            }
            else
            {
               bits  = (unsigned)slot / 2 - 1;
               dist += (uint64_t)(2 | (slot & 1)) << bits;
            }
            if (bits)
            {
               if (bits >= 4)
               {
                  int low;
                  /* the low four bits have a code of their own */
                  if (bits > 4)
                     dist += (uint64_t)br_get(&br, (int)bits - 4) << 4;
                  if ((low = huff_decode(&br, &u->low_offset_code)) < 0)
                     return RRAR_ERROR_DATA;
                  dist += (unsigned)low;
               }
               else
                  dist += br_get(&br, (int)bits);
            }
            if (dist > 0x100)
            {
               len++;
               if (dist > 0x2000)
               {
                  len++;
                  if (dist > 0x40000)
                     len++;
               }
            }
            /* (a distance of 4 GB or more is from before the member) */
            if (dist > 0xffffffffu)
               dist = 0xffffffffu;
            u->old_offset[3] = u->old_offset[2];
            u->old_offset[2] = u->old_offset[1];
            u->old_offset[1] = u->old_offset[0];
            u->old_offset[0] = (uint32_t)dist;
            u->last_length   = len;
         }
         else if (symbol == 256)
         {
            if ((r = unpack5_filter(u, &br, pos)) != RRAR_OK)
               return r;
            continue;
         }
         else if (symbol == 257)
         {
            /* the last match again */
            if (!u->last_length)
               continue;
            len  = u->last_length;
            dist = u->old_offset[0];
         }
         else
         {
            /* one of the last four distances, which becomes the first */
            int idx = symbol - 258;
            int slot, i2;

            dist = u->old_offset[idx];
            for (i2 = idx; i2 > 0; i2--)
               u->old_offset[i2] = u->old_offset[i2 - 1];
            u->old_offset[0] = (uint32_t)dist;
            if ((slot = huff_decode(&br, &u->length_code)) < 0)
               return RRAR_ERROR_DATA;
            len = length5(&br, (unsigned)slot);
            u->last_length = len;
         }
         if (br_overrun(&br))
            return RRAR_ERROR_DATA;
         pos = copy_match(out, pos, size, (uint32_t)dist, len);
      }

      if (br_overrun(&br))
         return RRAR_ERROR_DATA;
      if (!block_done)
         break;                     /* the member is full */
      if (flags & 0x40)
         break;                     /* the last block */
      p = data + block_size;
   }
   u->pos = pos;
   return RRAR_OK;
}

static int unpack_member(const uint8_t *packed, size_t packed_len,
      uint8_t *out, size_t out_len, int rar5)
{
   rrar_unpack_t *u = (rrar_unpack_t *)calloc(1, sizeof(*u));
   int r;

   if (!u)
      return RRAR_ERROR_MEM;
   u->br.p        = packed;
   u->br.end      = packed + packed_len;
   u->out         = out;
   u->size        = out_len;
   u->ppmd_escape = 2;
   rrar_ppmd7_construct(&u->ppmd);

   r = rar5 ? unpack50(u) : unpack29(u);
   if (r == RRAR_OK && u->pos != u->size)
      r = RRAR_ERROR_DATA;
   if (r == RRAR_OK)
      r = filters_run(u);

   rrar_ppmd7_free(&u->ppmd);
   free(u->programs);
   free(u->filters);
   free(u);
   return r;
}

/* -------------------------------------------------------------- headers */

/* A name with the Unicode flag: the name in the system's code page, a
 * zero, and then its UTF-16 form packed against it. To UTF-8. Returns
 * the bytes written, without the terminator, or 0 if they do not fit. */
static size_t name_unicode(const uint8_t *name, size_t name_len, size_t ascii_len,
      char *out, size_t out_cap)
{
   const uint8_t *p   = name + ascii_len + 1;
   size_t         end = name_len - ascii_len - 1;
   size_t         off = 0, n = 0, chars = 0;
   unsigned       high, flag_byte = 0, flag_bits = 0;
   uint32_t       pending = 0;      /* a leading surrogate waiting for its pair */

   if (end < 1)
      return 0;
   high = p[off++];

   /* (it has as many characters as the code-page name, at most) */
   while ((off < end && chars < ascii_len) || pending)
   {
      uint32_t c[130];
      unsigned count = 0, k;

      if (off < end && chars < ascii_len)
      {
         if (!flag_bits)
         {
            flag_byte = p[off++];
            flag_bits = 8;
            if (off >= end)
               break;
         }
         flag_bits -= 2;
         switch ((flag_byte >> flag_bits) & 3)
         {
            case 0:
               c[count++] = p[off++];
               break;
            case 1:
               c[count++] = p[off++] | (high << 8);
               break;
            case 2:
               if (off + 1 >= end)
                  return 0;
               c[count++] = p[off] | ((uint32_t)p[off + 1] << 8);
               off += 2;
               break;
            default:
            {
               /* a run taken from the code-page name, shifted */
               unsigned length = p[off++];
               unsigned extra  = 0, hi = 0;
               if (length & 0x80)
               {
                  if (off >= end)
                     return 0;
                  extra = p[off++];
                  hi    = high;
               }
               length = (length & 0x7f) + 2;
               while (length-- && chars + count < ascii_len)
               {
                  c[count] = ((name[chars + count] + extra) & 0xff) | (hi << 8);
                  count++;
               }
               break;
            }
         }
      }
      else
      {
         /* a lone surrogate at the end */
         pending = 0;
         break;
      }

      for (k = 0; k < count; k++)
      {
         uint32_t cp = c[k];
         chars++;
         if (pending)
         {
            if (cp >= 0xdc00 && cp < 0xe000)
               cp = 0x10000 + ((pending - 0xd800) << 10) + (cp - 0xdc00);
            pending = 0;
         }
         else if (cp >= 0xd800 && cp < 0xdc00)
         {
            pending = cp;
            continue;
         }
         if (n + 4 >= out_cap)
            return 0;
         if (cp < 0x80)
            out[n++] = (char)cp;
         else if (cp < 0x800)
         {
            out[n++] = (char)(0xc0 | (cp >> 6));
            out[n++] = (char)(0x80 | (cp & 0x3f));
         }
         else if (cp < 0x10000)
         {
            out[n++] = (char)(0xe0 | (cp >> 12));
            out[n++] = (char)(0x80 | ((cp >> 6) & 0x3f));
            out[n++] = (char)(0x80 | (cp & 0x3f));
         }
         else
         {
            out[n++] = (char)(0xf0 | (cp >> 18));
            out[n++] = (char)(0x80 | ((cp >> 12) & 0x3f));
            out[n++] = (char)(0x80 | ((cp >> 6) & 0x3f));
            out[n++] = (char)(0x80 | (cp & 0x3f));
         }
      }
   }
   return n;
}

/* Takes the entry's name: as UTF-8, with '/' between its parts. */
static int add_name(rrar_archive_t *a, const uint8_t *name, size_t name_len,
      int unicode, size_t *at)
{
   /* (a UTF-16 unit is at most three bytes of UTF-8) */
   size_t need = name_len * 3 + 8;
   size_t n    = 0, i;
   char  *dst;

   if (a->names_len + need > a->names_cap)
   {
      size_t cap = a->names_cap ? a->names_cap * 2 : 1024;
      char  *p;
      while (cap < a->names_len + need)
         cap *= 2;
      if (!(p = (char *)realloc(a->names, cap)))
         return RRAR_ERROR_MEM;
      a->names     = p;
      a->names_cap = cap;
   }
   dst = a->names + a->names_len;

   if (unicode)
   {
      size_t ascii_len = 0;
      while (ascii_len < name_len && name[ascii_len])
         ascii_len++;
      if (ascii_len < name_len)
         n = name_unicode(name, name_len, ascii_len, dst, need);
      if (!n)
      {
         /* no second form (the name is UTF-8 already), or a bad one */
         memcpy(dst, name, ascii_len);
         n = ascii_len;
      }
   }
   else
   {
      while (n < name_len && name[n])
      {
         dst[n] = (char)name[n];
         n++;
      }
   }
   for (i = 0; i < n; i++)
      if (dst[i] == '\\')
         dst[i] = '/';
   dst[n] = '\0';
   *at    = a->names_len;
   a->names_len += n + 1;
   return RRAR_OK;
}

static int parse_headers(rrar_archive_t *a)
{
   const uint8_t *d   = a->data;
   size_t         len = a->len;
   size_t         pos = RRAR_SIGNATURE_SIZE;
   size_t        *name_at = NULL;
   int            solid = 0, seen_main = 0;
   int            r = RRAR_OK;
   uint32_t       i;

   while (pos + 7 <= len)
   {
      uint32_t crc   = rd16(d + pos);
      unsigned type  = d[pos + 2];
      unsigned flags = rd16(d + pos + 3);
      size_t   size  = rd16(d + pos + 5);
      uint64_t add   = 0;

      if (size < 7 || size > len - pos)
      {
         r = RRAR_ERROR_DATA;
         break;
      }
      /* (a file's header is checked whole; the others only as far as
       * their fixed part, which differs by kind: the kinds read here are
       * checked whole) */
      if (type == RAR_HEAD_END)
         break;

      if (type == RAR_HEAD_MAIN)
      {
         if (size < 13
               || (encoding_crc32(0, d + pos + 2, 11) & 0xffff) != crc)
         {
            r = RRAR_ERROR_DATA;
            break;
         }
         if (flags & (RAR_MAIN_VOLUME | RAR_MAIN_PASSWORD))
         {
            r = RRAR_ERROR_UNSUPPORTED;
            break;
         }
         solid     = (flags & RAR_MAIN_SOLID) != 0;
         seen_main = 1;
      }
      else if (type == RAR_HEAD_FILE)
      {
         rrar_entry_t *e;
         uint64_t      packed, unpacked;
         size_t        fixed = 32, name_len;
         int           is_dir;

         if (size < fixed
               || (encoding_crc32(0, d + pos + 2, size - 2) & 0xffff) != crc)
         {
            r = RRAR_ERROR_DATA;
            break;
         }
         packed   = rd32(d + pos + 7);
         unpacked = rd32(d + pos + 11);
         name_len = rd16(d + pos + 26);
         if (flags & RAR_FILE_LARGE)
         {
            if (size < fixed + 8)
            {
               r = RRAR_ERROR_DATA;
               break;
            }
            packed   |= (uint64_t)rd32(d + pos + 32) << 32;
            unpacked |= (uint64_t)rd32(d + pos + 36) << 32;
            fixed    += 8;
         }
         if (name_len > size - fixed || packed > len - pos - size)
         {
            r = RRAR_ERROR_DATA;
            break;
         }
         add = packed;

         if (a->num_entries == a->cap_entries)
         {
            uint32_t      cap = a->cap_entries ? a->cap_entries * 2 : 16;
            rrar_entry_t *p   = (rrar_entry_t *)realloc(a->entries, cap * sizeof(*p));
            size_t       *q   = p ? (size_t *)realloc(name_at, cap * sizeof(*q)) : NULL;
            if (p)
               a->entries = p;
            if (q)
               name_at = q;
            if (!p || !q)
            {
               r = RRAR_ERROR_MEM;
               break;
            }
            a->cap_entries = cap;
         }
         if ((r = add_name(a, d + pos + fixed, name_len,
                     (flags & RAR_FILE_UNICODE) != 0, &name_at[a->num_entries])) != RRAR_OK)
            break;

         is_dir = (flags & RAR_FILE_DIRECTORY) == RAR_FILE_DIRECTORY;
         e      = &a->entries[a->num_entries++];
         memset(e, 0, sizeof(*e));
         e->size        = unpacked;
         e->packed_size = packed;
         e->data_offset = pos + size;
         e->crc         = rd32(d + pos + 16);
         e->has_crc     = 1;
         e->version     = d[pos + 24];
         e->method      = d[pos + 25];
         e->is_dir      = (uint8_t)is_dir;
         e->supported   = !is_dir
            && !(flags & (RAR_FILE_SPLIT_BEFORE | RAR_FILE_SPLIT_AFTER | RAR_FILE_PASSWORD))
            && unpacked <= (uint64_t)((size_t)-1) / 2
            && (e->method == RAR_METHOD_STORED
                  ? packed == unpacked
                  : (!solid && !(flags & RAR_FILE_SOLID)
                     && e->method >= 0x31 && e->method <= 0x35
                     && (e->version == 29 || e->version == 36)));
      }
      else if (flags & RAR_HEAD_ADD_SIZE)
      {
         /* some other block with data after it */
         if (size < 11)
         {
            r = RRAR_ERROR_DATA;
            break;
         }
         add = rd32(d + pos + 7);
         if (add > len - pos - size)
         {
            r = RRAR_ERROR_DATA;
            break;
         }
      }
      pos += size + (size_t)add;
   }

   if (r == RRAR_OK && !seen_main)
      r = RRAR_ERROR_DATA;
   for (i = 0; r == RRAR_OK && i < a->num_entries; i++)
      a->entries[i].name = a->names + name_at[i];
   free(name_at);
   return r;
}

/* A number as RAR 5's headers write them: seven bits to a byte, the low
 * ones first, the top bit saying there is more. */
static int rd_vint(const uint8_t *d, size_t end, size_t *pos, uint64_t *v)
{
   uint64_t r = 0;
   unsigned shift;

   for (shift = 0; shift < 70; shift += 7)
   {
      uint8_t b;
      if (*pos >= end)
         return 0;
      b  = d[(*pos)++];
      if (shift < 64)
         r |= (uint64_t)(b & 0x7f) << shift;
      if (!(b & 0x80))
      {
         *v = r;
         return 1;
      }
   }
   return 0;
}

/* The RAR 5 container: every header a CRC-32, its size, its kind and
 * flags, and what the kind has; a file's data after its header. */
static int parse_headers5(rrar_archive_t *a)
{
   const uint8_t *d   = a->data;
   size_t         len = a->len;
   size_t         pos = 8;
   size_t        *name_at = NULL;
   int            seen_main = 0, ended = 0;
   int            r = RRAR_OK;
   uint32_t       i;

   while (!ended && pos + 4 < len)
   {
      size_t   q = pos + 4, head_end;
      uint64_t hsize, type, flags, extra = 0, data_size = 0;

      if (!rd_vint(d, len, &q, &hsize) || hsize > 0x200000 || hsize > len - q)
      {
         r = RRAR_ERROR_DATA;
         break;
      }
      head_end = q + (size_t)hsize;
      if (encoding_crc32(0, d + pos + 4, head_end - (pos + 4)) != rd32(d + pos))
      {
         r = RRAR_ERROR_DATA;
         break;
      }
      if (     !rd_vint(d, head_end, &q, &type)
            || !rd_vint(d, head_end, &q, &flags)
            || ((flags & 1) && !rd_vint(d, head_end, &q, &extra))
            || ((flags & 2) && !rd_vint(d, head_end, &q, &data_size))
            || extra > head_end - q
            || data_size > len - head_end)
      {
         r = RRAR_ERROR_DATA;
         break;
      }

      switch (type)
      {
         case 1:     /* the archive */
         {
            uint64_t aflags;
            if (!rd_vint(d, head_end, &q, &aflags))
               r = RRAR_ERROR_DATA;
            else if (aflags & 1)
               r = RRAR_ERROR_UNSUPPORTED;      /* a volume */
            seen_main = 1;
            break;
         }
         case 4:     /* the headers after this are encrypted */
            r = RRAR_ERROR_UNSUPPORTED;
            break;
         case 5:
            ended = 1;
            break;
         case 2:     /* a file */
         {
            rrar_entry_t *e;
            uint64_t      fflags, unpacked, attr, info, host, name_len;
            uint32_t      crc = 0;
            size_t        extra_at = head_end - (size_t)extra;
            size_t        x;
            unsigned      method, version;
            int           encrypted = 0, is_dir;

            if (     !rd_vint(d, extra_at, &q, &fflags)
                  || !rd_vint(d, extra_at, &q, &unpacked)
                  || !rd_vint(d, extra_at, &q, &attr))
            {
               r = RRAR_ERROR_DATA;
               break;
            }
            if (fflags & 2)
            {
               if (extra_at - q < 4)
               {
                  r = RRAR_ERROR_DATA;
                  break;
               }
               q += 4;        /* modification time */
            }
            if (fflags & 4)
            {
               if (extra_at - q < 4)
               {
                  r = RRAR_ERROR_DATA;
                  break;
               }
               crc = rd32(d + q);
               q  += 4;
            }
            if (     !rd_vint(d, extra_at, &q, &info)
                  || !rd_vint(d, extra_at, &q, &host)
                  || !rd_vint(d, extra_at, &q, &name_len)
                  || name_len > extra_at - q)
            {
               r = RRAR_ERROR_DATA;
               break;
            }

            /* the records after the name: one of them says the data is
             * encrypted */
            for (x = extra_at; x < head_end;)
            {
               uint64_t rsize, rtype;
               size_t   y;
               if (!rd_vint(d, head_end, &x, &rsize) || rsize > head_end - x)
                  break;
               y = x;
               if (rd_vint(d, x + (size_t)rsize, &y, &rtype) && rtype == 1)
                  encrypted = 1;
               x += (size_t)rsize;
            }

            if (a->num_entries == a->cap_entries)
            {
               uint32_t      cap = a->cap_entries ? a->cap_entries * 2 : 16;
               rrar_entry_t *p   = (rrar_entry_t *)realloc(a->entries, cap * sizeof(*p));
               size_t       *n   = p ? (size_t *)realloc(name_at, cap * sizeof(*n)) : NULL;
               if (p)
                  a->entries = p;
               if (n)
                  name_at = n;
               if (!p || !n)
               {
                  r = RRAR_ERROR_MEM;
                  break;
               }
               a->cap_entries = cap;
            }
            /* (the name is UTF-8 as it is) */
            if ((r = add_name(a, d + q, (size_t)name_len, 0, &name_at[a->num_entries])) != RRAR_OK)
               break;

            version = (unsigned)(info & 0x3f);
            method  = (unsigned)((info >> 7) & 7);
            is_dir  = (fflags & 1) != 0;
            e       = &a->entries[a->num_entries++];
            memset(e, 0, sizeof(*e));
            e->size        = unpacked;
            e->packed_size = data_size;
            e->data_offset = head_end;
            e->crc         = crc;
            e->has_crc     = (fflags & 4) != 0;
            e->version     = 50;
            e->method      = (uint8_t)(RAR_METHOD_STORED + method);
            e->is_dir      = (uint8_t)is_dir;
            e->supported   = !is_dir
               && !encrypted
               && !(flags & (0x08 | 0x10))            /* split between volumes */
               && !(fflags & 8)                       /* of a size not known */
               && unpacked <= (uint64_t)((size_t)-1) / 2
               && (method == 0
                     ? data_size == unpacked
                     : (version == 0 && !(info & 0x40) && method <= 5));
            (void)attr;
            (void)host;
            break;
         }
         default:    /* a service header, or a kind from later: with its data, skipped */
            break;
      }
      if (r != RRAR_OK)
         break;
      pos = head_end + (size_t)data_size;
   }

   if (r == RRAR_OK && !seen_main)
      r = RRAR_ERROR_DATA;
   for (i = 0; r == RRAR_OK && i < a->num_entries; i++)
      a->entries[i].name = a->names + name_at[i];
   free(name_at);
   return r;
}

/* ------------------------------------------------------------------ API */

int rrar_archive_open(rrar_archive_t **out, const uint8_t *data, size_t len)
{
   rrar_archive_t *a;
   int             r;

   if (!out)
      return RRAR_ERROR_PARAM;
   *out = NULL;
   if (!data || len < RRAR_SIGNATURE_SIZE)
      return RRAR_ERROR_PARAM;
   if (memcmp(data, "Rar!\x1a\x07", 6))
      return RRAR_ERROR_DATA;
   /* the seventh byte tells the two containers apart */
   if (data[6] > 1 || (data[6] == 1 && (len < 8 || data[7] != 0)))
      return RRAR_ERROR_UNSUPPORTED;

   if (!(a = (rrar_archive_t *)calloc(1, sizeof(*a))))
      return RRAR_ERROR_MEM;
   a->data = data;
   a->len  = len;
   if ((r = data[6] ? parse_headers5(a) : parse_headers(a)) != RRAR_OK)
   {
      rrar_archive_close(a);
      return r;
   }
   *out = a;
   return RRAR_OK;
}

void rrar_archive_close(rrar_archive_t *a)
{
   if (!a)
      return;
   free(a->entries);
   free(a->names);
   free(a);
}

uint32_t rrar_archive_num_entries(const rrar_archive_t *a)
{
   return a ? a->num_entries : 0;
}

const rrar_entry_t *rrar_archive_entry(const rrar_archive_t *a, uint32_t index)
{
   if (!a || index >= a->num_entries)
      return NULL;
   return &a->entries[index];
}

int rrar_archive_extract(rrar_archive_t *a, uint32_t index,
      uint8_t **out, size_t *out_len)
{
   const rrar_entry_t *e;
   const uint8_t      *packed;
   uint8_t            *buf;
   size_t              size;
   int                 r = RRAR_OK;

   if (out)
      *out = NULL;
   if (!a || !out || !out_len || index >= a->num_entries)
      return RRAR_ERROR_PARAM;
   e = &a->entries[index];
   if (!e->supported)
      return RRAR_ERROR_UNSUPPORTED;

   size   = (size_t)e->size;
   packed = a->data + (size_t)e->data_offset;
   if (!(buf = (uint8_t *)malloc(size ? size : 1)))
      return RRAR_ERROR_MEM;

   if (e->method == RAR_METHOD_STORED)
      memcpy(buf, packed, size);
   else if (size)
      r = unpack_member(packed, (size_t)e->packed_size, buf, size, e->version == 50);

   if (r == RRAR_OK && e->has_crc && encoding_crc32(0, buf, size) != e->crc)
      r = RRAR_ERROR_CRC;
   if (r != RRAR_OK)
   {
      free(buf);
      return r;
   }
   *out     = buf;
   *out_len = size;
   return RRAR_OK;
}
