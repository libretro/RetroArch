/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (r7z_archive.c).
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

/* 7z container reader. See r7z_archive.h for scope and for the rules
 * this file follows about untrusted input.
 *
 * Structure of an archive, briefly, since the parser below follows it
 * directly:
 *
 *   signature (32 bytes)   magic, version, and the offset/size/CRC of
 *                          the header, which lives at the end
 *   header                 either a plain header or an encoded one,
 *                          which is itself a one-folder archive whose
 *                          decoded bytes are the plain header
 *   PackInfo               sizes of the packed streams, in order
 *   UnpackInfo             folders: for each, its coder chain and the
 *                          unpacked size of each coder's output
 *   SubStreamsInfo         how each folder's output splits into files,
 *                          with sizes and CRCs
 *   FilesInfo              names, and which entries are empty or
 *                          directories
 *
 * The header is a sequence of id-tagged blocks. Ids arrive as
 * variable-length numbers, as does nearly every count and size, so the
 * reader below is built around one primitive that cannot run past the
 * end of the buffer.
 */

#include <stdlib.h>
#include <string.h>

#include <7z/r7z_archive.h>
#include <7z/r7z_lzma.h>
#include <7z/r7z_lzma_stream.h>
#include <7z/r7z_lzma2.h>
#include <7z/r7z_bcj2.h>
#include <7z/r7z_filters.h>

/* --------------------------------------------------------------------
 * Header block ids
 * -------------------------------------------------------------------- */

#define ID_END                 0
#define ID_HEADER              1
#define ID_ARCHIVE_PROPERTIES  2
#define ID_ADDITIONAL_STREAMS  3
#define ID_MAIN_STREAMS        4
#define ID_FILES_INFO          5
#define ID_PACK_INFO           6
#define ID_UNPACK_INFO         7
#define ID_SUBSTREAMS_INFO     8
#define ID_SIZE                9
#define ID_CRC                10
#define ID_FOLDER             11
#define ID_CODERS_UNPACK_SIZE 12
#define ID_NUM_UNPACK_STREAM  13
#define ID_EMPTY_STREAM       14
#define ID_EMPTY_FILE         15
#define ID_ANTI               16
#define ID_NAME               17
#define ID_CTIME              18
#define ID_ATIME              19
#define ID_MTIME              20
#define ID_WIN_ATTRIB         21
#define ID_COMMENT            22
#define ID_ENCODED_HEADER     23
#define ID_START_POS          24
#define ID_DUMMY              25

/* Coder method ids, as stored in the folder's coder list. */
#define METHOD_COPY   0x00
#define METHOD_DELTA  0x03
#define METHOD_LZMA2  0x21
#define METHOD_LZMA   0x030101
#define METHOD_BCJ_X86 0x03030103
#define METHOD_BCJ2   0x0303011B
#define METHOD_BCJ    0x04
#define METHOD_ARM64  0x0A
#define METHOD_BRA_PPC   0x03030205
#define METHOD_BRA_IA64  0x03030401
#define METHOD_BRA_ARM   0x03030501
#define METHOD_BRA_ARMT  0x03030701
#define METHOD_BRA_SPARC 0x03030805

/* Ceilings. These are not format limits; they are what a sane archive
 * of ROMs contains. Their job is to stop a header claiming four
 * billion of something and having us try to allocate for it before we
 * have read a single byte of the thing it is counting. */
#define MAX_ENTRIES  (1u << 22)
#define MAX_FOLDERS  (1u << 20)
#define MAX_STREAMS  (1u << 22)
#define MAX_CODERS   64
#define MAX_BONDS    64

/* --------------------------------------------------------------------
 * Bounds-checked reader
 * -------------------------------------------------------------------- */

typedef struct
{
   const uint8_t *p;
   const uint8_t *end;
} rd_t;

static int rd_byte(rd_t *r, uint8_t *v)
{
   if (r->p >= r->end)
      return R7Z_ERROR_DATA;
   *v = *r->p++;
   return R7Z_OK;
}

static int rd_skip(rd_t *r, uint64_t n)
{
   if ((uint64_t)(r->end - r->p) < n)
      return R7Z_ERROR_DATA;
   r->p += (size_t)n;
   return R7Z_OK;
}

static int rd_u32le(rd_t *r, uint32_t *v)
{
   if ((size_t)(r->end - r->p) < 4)
      return R7Z_ERROR_DATA;
   *v = (uint32_t)r->p[0] | ((uint32_t)r->p[1] << 8)
      | ((uint32_t)r->p[2] << 16) | ((uint32_t)r->p[3] << 24);
   r->p += 4;
   return R7Z_OK;
}

/* The variable-length number the format uses everywhere: the top bits
 * of the first byte say how many more bytes follow. */
static int rd_num(rd_t *r, uint64_t *value)
{
   uint8_t  first;
   uint8_t  mask;
   uint8_t  b;
   unsigned i;
   uint64_t v = 0;
   int      res;

   if ((res = rd_byte(r, &first)) != R7Z_OK)
      return res;

   if ((first & 0x80) == 0)
   {
      *value = first;
      return R7Z_OK;
   }

   if ((res = rd_byte(r, &b)) != R7Z_OK)
      return res;

   if ((first & 0x40) == 0)
   {
      *value = (((uint64_t)first & 0x3F) << 8) | b;
      return R7Z_OK;
   }

   {
      uint8_t b2;
      if ((res = rd_byte(r, &b2)) != R7Z_OK)
         return res;
      v = (uint64_t)b | ((uint64_t)b2 << 8);
   }

   mask = 0x20;
   for (i = 2; i < 8; i++)
   {
      if ((first & mask) == 0)
      {
         uint64_t high = (uint64_t)(first & (uint8_t)(mask - 1));
         v |= high << (8 * i);
         *value = v;
         return R7Z_OK;
      }
      if ((res = rd_byte(r, &b)) != R7Z_OK)
         return res;
      v |= (uint64_t)b << (8 * i);
      mask = (uint8_t)(mask >> 1);
   }
   *value = v;
   return R7Z_OK;
}

/* A count that must fit in 32 bits and must not exceed what the
 * remaining input could plausibly describe. `per` is the minimum number
 * of header bytes each counted item needs; passing 0 means the items
 * carry no per-item bytes here, in which case only the ceiling
 * applies. */
static int rd_count(rd_t *r, uint32_t *out, uint32_t ceiling, size_t per)
{
   uint64_t v;
   int      res = rd_num(r, &v);

   if (res != R7Z_OK)
      return res;
   if (v > (uint64_t)ceiling)
      return R7Z_ERROR_DATA;
   if (per != 0)
   {
      uint64_t avail = (uint64_t)(r->end - r->p);
      if (v > avail / (uint64_t)per)
         return R7Z_ERROR_DATA;
   }
   *out = (uint32_t)v;
   return R7Z_OK;
}

/* Bit vector: one bit per item, MSB first. */
static int rd_bits(rd_t *r, uint8_t *bits, uint32_t num)
{
   uint32_t i;
   uint8_t  b    = 0;
   uint8_t  mask = 0;

   for (i = 0; i < num; i++)
   {
      if (mask == 0)
      {
         int res = rd_byte(r, &b);
         if (res != R7Z_OK)
            return res;
         mask = 0x80;
      }
      bits[i] = (uint8_t)((b & mask) != 0);
      mask = (uint8_t)(mask >> 1);
   }
   return R7Z_OK;
}

/* A bit vector preceded by an "all defined" shortcut byte. */
static int rd_bits_maybe_all(rd_t *r, uint8_t *bits, uint32_t num)
{
   uint8_t all;
   int     res = rd_byte(r, &all);

   if (res != R7Z_OK)
      return res;
   if (all != 0)
   {
      uint32_t i;
      for (i = 0; i < num; i++)
         bits[i] = 1;
      return R7Z_OK;
   }
   return rd_bits(r, bits, num);
}

/* Skip an id-tagged block whose size is not otherwise known. Only used
 * for the properties this reader ignores. */
static int rd_skip_sized(rd_t *r)
{
   uint64_t size;
   int      res = rd_num(r, &size);

   if (res != R7Z_OK)
      return res;
   return rd_skip(r, size);
}

/* --------------------------------------------------------------------
 * Parsed structures
 * -------------------------------------------------------------------- */

typedef struct
{
   uint64_t method;
   uint32_t num_in;
   uint32_t num_out;
   uint32_t prop_offset;   /* into a->props */
   uint32_t prop_size;
} coder_t;

typedef struct
{
   uint32_t num_coders;
   uint32_t coder_first;   /* into a->coders */
   uint32_t num_bonds;
   uint32_t bond_first;    /* into a->bonds, pairs of (in, out) */
   uint32_t num_pack;
   uint32_t pack_first;    /* into a->folder_pack, indices into pack sizes */
   uint32_t main_out;      /* which coder output is the folder's output */
   uint64_t unpack_size;   /* size of that output */
   uint64_t pack_offset;   /* byte offset of this folder's first packed stream */
   uint32_t num_unpack_streams;
   uint32_t first_entry;   /* index of first entry in this folder */
   uint32_t has_crc;
   uint32_t crc;
} folder_t;

struct r7z_archive
{
   const uint8_t *data;
   size_t         len;

   /* Decoded header, when the archive used an encoded one. */
   uint8_t       *header_buf;

   uint64_t      *pack_sizes;
   uint32_t       num_pack;
   uint64_t       pack_base;      /* file offset of packed data area */

   coder_t       *coders;
   uint32_t       num_coders;
   uint8_t       *props;
   uint32_t       props_len;
   uint32_t      *bonds;
   uint32_t       num_bonds;
   uint32_t      *folder_pack;
   uint32_t       num_folder_pack;
   uint64_t      *coder_unpack_sizes;
   uint32_t       num_coder_unpack_sizes;

   folder_t      *folders;
   uint32_t       num_folders;

   /* Last folder decoded, kept so that extracting several members of
    * one solid folder decodes it once rather than once per member.
    * cached_folder is 0xFFFFFFFF when nothing is held. */
   uint8_t       *cached_data;
   size_t         cached_len;
   uint32_t       cached_folder;

   /* A folder decode that has been started and paused part way.
    *
    * Only the LZMA and LZMA2 stages pause: they are 97% of the time
    * and their decoders already take an output limit. Every other
    * coder in a chain, the branch filters included, runs to
    * completion in one go because each is a single linear pass of a
    * few milliseconds over the whole buffer.
    *
    * pend_folder is 0xFFFFFFFF when nothing is in flight. */
   uint32_t       pend_folder;
   uint32_t       pend_coder;      /* index within the folder's chain */
   uint8_t       *pend_in;         /* previous stage's output */
   size_t         pend_in_len;
   uint8_t       *pend_out;        /* buffer being filled */
   size_t         pend_out_len;
   void          *pend_dec;        /* rlzma2_dec_t, when one is live */
   uint16_t      *pend_probs;
   size_t         pend_fed;        /* input consumed by this stage */
   uint32_t       pend_base;       /* coder_unpack_sizes base for folder */

   /* A BCJ2 folder part way through. Its four inputs must all be
    * complete before the converter can run, but they need not be
    * resolved in the same call: one port per call keeps each call
    * bounded by that port's own decode. */
   uint8_t       *pend_b2buf[4];
   size_t         pend_b2len[4];
   uint32_t       pend_b2port;     /* next port to resolve, 0..4 */
   uint32_t       pend_b2at;       /* index of the BCJ2 coder */
   uint32_t       pend_b2in;       /* input-port base of that coder */
   uint32_t       pend_b2active;
   /* The conversion itself, once all four inputs are in. It is one
    * linear pass over the whole folder, so on a multi-megabyte folder
    * it is worth slicing too. */
   void          *pend_b2st;
   uint8_t       *pend_b2dst;
   size_t         pend_b2dstlen;
   size_t         pend_b2done;

   r7z_entry_t   *entries;
   uint32_t       num_entries;
   uint16_t      *names;
   size_t         names_len;
};

/* Defined with the resumable decoder below; declared here because
 * r7z_archive_close() has to release a decode left in flight. */
static void decode_pending_reset(r7z_archive_t *a);

/* --------------------------------------------------------------------
 * CRC32
 * -------------------------------------------------------------------- */

/* Prefixed because griffin builds every file in this directory into a
 * single translation unit, so a file-scope crc_table would be liable to
 * collide with another unit's. */
static uint32_t r7z_crc_table[256];
static int      r7z_crc_ready = 0;

static void crc_init(void)
{
   uint32_t i, j, c;

   if (r7z_crc_ready)
      return;
   for (i = 0; i < 256; i++)
   {
      c = i;
      for (j = 0; j < 8; j++)
         c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
      r7z_crc_table[i] = c;
   }
   r7z_crc_ready = 1;
}

static uint32_t crc_calc(const uint8_t *p, size_t len)
{
   uint32_t c = 0xFFFFFFFFu;
   size_t   i;

   crc_init();
   for (i = 0; i < len; i++)
      c = r7z_crc_table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
   return c ^ 0xFFFFFFFFu;
}

/* --------------------------------------------------------------------
 * PackInfo
 *
 *   ID_PACK_INFO base_offset num_streams
 *     [ID_SIZE size...]
 *     [ID_CRC ...]        (ignored)
 *   ID_END
 * -------------------------------------------------------------------- */

static int parse_pack_info(r7z_archive_t *a, rd_t *r)
{
   uint64_t base;
   uint32_t i;
   int      res;

   if ((res = rd_num(r, &base)) != R7Z_OK)
      return res;
   if ((res = rd_count(r, &a->num_pack, MAX_STREAMS, 1)) != R7Z_OK)
      return res;

   a->pack_base = base;

   if (a->num_pack != 0)
   {
      a->pack_sizes = (uint64_t *)calloc(a->num_pack, sizeof(uint64_t));
      if (!a->pack_sizes)
         return R7Z_ERROR_MEM;
   }

   for (;;)
   {
      uint64_t id;
      if ((res = rd_num(r, &id)) != R7Z_OK)
         return res;
      if (id == ID_END)
         break;
      if (id == ID_SIZE)
      {
         for (i = 0; i < a->num_pack; i++)
            if ((res = rd_num(r, &a->pack_sizes[i])) != R7Z_OK)
               return res;
      }
      else if (id == ID_CRC)
      {
         /* Packed-stream CRCs. Not used: the unpacked CRCs in
          * SubStreamsInfo are what entries are checked against. */
         uint8_t *defs = NULL;
         uint32_t n;

         if (a->num_pack != 0)
         {
            defs = (uint8_t *)calloc(a->num_pack, 1);
            if (!defs)
               return R7Z_ERROR_MEM;
         }
         res = rd_bits_maybe_all(r, defs, a->num_pack);
         if (res == R7Z_OK)
            for (n = 0; n < a->num_pack; n++)
               if (defs[n])
               {
                  uint32_t dummy;
                  res = rd_u32le(r, &dummy);
                  if (res != R7Z_OK)
                     break;
               }
         free(defs);
         if (res != R7Z_OK)
            return res;
      }
      else if ((res = rd_skip_sized(r)) != R7Z_OK)
         return res;
   }

   /* Every packed stream must lie inside the file. Checked here as a
    * running total so a later folder cannot be handed an offset that
    * wrapped. */
   {
      uint64_t total = 0;
      for (i = 0; i < a->num_pack; i++)
      {
         if (a->pack_sizes[i] > (uint64_t)a->len)
            return R7Z_ERROR_DATA;
         if (total > (uint64_t)a->len - a->pack_sizes[i])
            return R7Z_ERROR_DATA;
         total += a->pack_sizes[i];
      }
      if (base > (uint64_t)a->len || total > (uint64_t)a->len - base)
         return R7Z_ERROR_DATA;
   }

   return R7Z_OK;
}

/* --------------------------------------------------------------------
 * Folders
 *
 * A folder is a small dataflow graph: coders with input and output
 * ports, bonds joining one coder's output to another's input, and
 * packed streams feeding the inputs left unbonded. This reader accepts
 * the shapes 7z actually produces and rejects the rest.
 * -------------------------------------------------------------------- */

static int parse_folder(r7z_archive_t *a, rd_t *r, folder_t *f,
      uint32_t *coder_pos, uint32_t *bond_pos, uint32_t *pack_pos)
{
   uint32_t i;
   uint32_t num_in_total  = 0;
   uint32_t num_out_total = 0;
   int      res;

   if ((res = rd_count(r, &f->num_coders, MAX_CODERS, 2)) != R7Z_OK)
      return res;
   if (f->num_coders == 0)
      return R7Z_ERROR_DATA;
   if (*coder_pos + f->num_coders > a->num_coders)
      return R7Z_ERROR_DATA;

   f->coder_first = *coder_pos;

   for (i = 0; i < f->num_coders; i++)
   {
      coder_t *c = &a->coders[*coder_pos + i];
      uint8_t  flags;
      uint32_t id_size;

      if ((res = rd_byte(r, &flags)) != R7Z_OK)
         return res;

      id_size = (uint32_t)(flags & 0x0F);
      if (id_size > 8)
         return R7Z_ERROR_DATA;

      c->method = 0;
      {
         uint32_t k;
         for (k = 0; k < id_size; k++)
         {
            uint8_t b;
            if ((res = rd_byte(r, &b)) != R7Z_OK)
               return res;
            c->method = (c->method << 8) | (uint64_t)b;
         }
      }

      if (flags & 0x10)
      {
         /* Complex coder: explicit port counts. */
         if ((res = rd_count(r, &c->num_in, MAX_CODERS, 0)) != R7Z_OK)
            return res;
         if ((res = rd_count(r, &c->num_out, MAX_CODERS, 0)) != R7Z_OK)
            return res;
         if (c->num_in == 0 || c->num_out == 0)
            return R7Z_ERROR_DATA;
      }
      else
      {
         c->num_in  = 1;
         c->num_out = 1;
      }

      if (flags & 0x80)
         return R7Z_ERROR_UNSUPPORTED;   /* alternate methods */

      c->prop_size   = 0;
      c->prop_offset = 0;
      if (flags & 0x20)
      {
         uint64_t psize;
         if ((res = rd_num(r, &psize)) != R7Z_OK)
            return res;
         if (psize > (uint64_t)(r->end - r->p))
            return R7Z_ERROR_DATA;
         if (psize > 0xFFFFu)
            return R7Z_ERROR_DATA;
         if (a->props_len + (uint32_t)psize > a->props_len
               && a->props != NULL)
         {
            memcpy(a->props + a->props_len, r->p, (size_t)psize);
            c->prop_offset = a->props_len;
            c->prop_size   = (uint32_t)psize;
            a->props_len  += (uint32_t)psize;
         }
         r->p += (size_t)psize;
      }

      num_in_total  += c->num_in;
      num_out_total += c->num_out;
      if (num_in_total > MAX_CODERS * 4 || num_out_total > MAX_CODERS * 4)
         return R7Z_ERROR_DATA;
   }

   *coder_pos += f->num_coders;

   /* Bonds: one fewer than the number of outputs, by construction. */
   if (num_out_total == 0)
      return R7Z_ERROR_DATA;
   f->num_bonds = num_out_total - 1;
   if (f->num_bonds > MAX_BONDS)
      return R7Z_ERROR_DATA;
   if (*bond_pos + f->num_bonds * 2 > a->num_bonds)
      return R7Z_ERROR_DATA;

   f->bond_first = *bond_pos;
   for (i = 0; i < f->num_bonds; i++)
   {
      uint64_t in_idx, out_idx;
      if ((res = rd_num(r, &in_idx)) != R7Z_OK)
         return res;
      if ((res = rd_num(r, &out_idx)) != R7Z_OK)
         return res;
      if (in_idx >= num_in_total || out_idx >= num_out_total)
         return R7Z_ERROR_DATA;
      a->bonds[*bond_pos + i * 2]     = (uint32_t)in_idx;
      a->bonds[*bond_pos + i * 2 + 1] = (uint32_t)out_idx;
   }
   *bond_pos += f->num_bonds * 2;

   /* Packed streams feed the inputs no bond covers. */
   f->num_pack = num_in_total - f->num_bonds;
   if (f->num_pack == 0 || f->num_pack > MAX_CODERS)
      return R7Z_ERROR_DATA;
   if (*pack_pos + f->num_pack > a->num_folder_pack)
      return R7Z_ERROR_DATA;

   f->pack_first = *pack_pos;
   if (f->num_pack == 1)
   {
      /* Implicit: the one unbonded input. */
      uint32_t k;
      uint32_t found = 0xFFFFFFFFu;
      for (k = 0; k < num_in_total; k++)
      {
         uint32_t b;
         int used = 0;
         for (b = 0; b < f->num_bonds; b++)
            if (a->bonds[f->bond_first + b * 2] == k)
            {
               used = 1;
               break;
            }
         if (!used)
         {
            found = k;
            break;
         }
      }
      if (found == 0xFFFFFFFFu)
         return R7Z_ERROR_DATA;
      a->folder_pack[*pack_pos] = found;
   }
   else
   {
      for (i = 0; i < f->num_pack; i++)
      {
         uint64_t idx;
         if ((res = rd_num(r, &idx)) != R7Z_OK)
            return res;
         if (idx >= num_in_total)
            return R7Z_ERROR_DATA;
         a->folder_pack[*pack_pos + i] = (uint32_t)idx;
      }
   }
   *pack_pos += f->num_pack;

   /* The folder's output is the one coder output no bond consumes. */
   {
      uint32_t k;
      uint32_t found = 0xFFFFFFFFu;
      for (k = 0; k < num_out_total; k++)
      {
         uint32_t b;
         int used = 0;
         for (b = 0; b < f->num_bonds; b++)
            if (a->bonds[f->bond_first + b * 2 + 1] == k)
            {
               used = 1;
               break;
            }
         if (!used)
         {
            found = k;
            break;
         }
      }
      if (found == 0xFFFFFFFFu)
         return R7Z_ERROR_DATA;
      f->main_out = found;
   }

   return R7Z_OK;
}

/* --------------------------------------------------------------------
 * UnpackInfo
 *
 *   ID_UNPACK_INFO
 *     ID_FOLDER num_folders external folder...
 *     ID_CODERS_UNPACK_SIZE size...   (one per coder output, all folders)
 *     [ID_CRC ...]
 *   ID_END
 *
 * The two passes over the folder list are unavoidable: the counts of
 * coders, bonds and packed streams are only known by walking the
 * folders, and the arrays holding them have to be sized first.
 * -------------------------------------------------------------------- */

static int count_folders(rd_t probe, uint32_t num_folders,
      uint32_t *n_coders, uint32_t *n_bonds, uint32_t *n_pack,
      uint32_t *n_props)
{
   uint32_t fi;

   *n_coders = 0;
   *n_bonds  = 0;
   *n_pack   = 0;
   *n_props  = 0;

   for (fi = 0; fi < num_folders; fi++)
   {
      uint32_t nc, i;
      uint32_t in_total  = 0;
      uint32_t out_total = 0;
      int      res;

      if ((res = rd_count(&probe, &nc, MAX_CODERS, 2)) != R7Z_OK)
         return res;
      if (nc == 0)
         return R7Z_ERROR_DATA;

      for (i = 0; i < nc; i++)
      {
         uint8_t  flags;
         uint32_t id_size;
         uint32_t ni = 1, no = 1;

         if ((res = rd_byte(&probe, &flags)) != R7Z_OK)
            return res;
         id_size = (uint32_t)(flags & 0x0F);
         if (id_size > 8)
            return R7Z_ERROR_DATA;
         if ((res = rd_skip(&probe, id_size)) != R7Z_OK)
            return res;

         if (flags & 0x10)
         {
            if ((res = rd_count(&probe, &ni, MAX_CODERS, 0)) != R7Z_OK)
               return res;
            if ((res = rd_count(&probe, &no, MAX_CODERS, 0)) != R7Z_OK)
               return res;
            if (ni == 0 || no == 0)
               return R7Z_ERROR_DATA;
         }
         if (flags & 0x80)
            return R7Z_ERROR_UNSUPPORTED;
         if (flags & 0x20)
         {
            uint64_t psize;
            if ((res = rd_num(&probe, &psize)) != R7Z_OK)
               return res;
            if (psize > 0xFFFFu)
               return R7Z_ERROR_DATA;
            if ((res = rd_skip(&probe, psize)) != R7Z_OK)
               return res;
            *n_props += (uint32_t)psize;
         }
         in_total  += ni;
         out_total += no;
         if (in_total > MAX_CODERS * 4 || out_total > MAX_CODERS * 4)
            return R7Z_ERROR_DATA;
      }

      if (out_total == 0)
         return R7Z_ERROR_DATA;
      {
         uint32_t nb = out_total - 1;
         uint32_t np;

         if (nb > MAX_BONDS)
            return R7Z_ERROR_DATA;
         for (i = 0; i < nb; i++)
         {
            uint64_t x;
            if ((res = rd_num(&probe, &x)) != R7Z_OK)
               return res;
            if ((res = rd_num(&probe, &x)) != R7Z_OK)
               return res;
         }
         if (in_total < nb)
            return R7Z_ERROR_DATA;
         np = in_total - nb;
         if (np == 0 || np > MAX_CODERS)
            return R7Z_ERROR_DATA;
         if (np > 1)
            for (i = 0; i < np; i++)
            {
               uint64_t x;
               if ((res = rd_num(&probe, &x)) != R7Z_OK)
                  return res;
            }

         *n_coders += nc;
         *n_bonds  += nb * 2;
         *n_pack   += np;
      }
   }
   return R7Z_OK;
}

static int parse_unpack_info(r7z_archive_t *a, rd_t *r)
{
   uint64_t id;
   uint32_t i;
   uint8_t  external;
   uint32_t coder_pos = 0, bond_pos = 0, pack_pos = 0;
   int      res;

   if ((res = rd_num(r, &id)) != R7Z_OK)
      return res;
   if (id != ID_FOLDER)
      return R7Z_ERROR_DATA;

   if ((res = rd_count(r, &a->num_folders, MAX_FOLDERS, 2)) != R7Z_OK)
      return res;
   if ((res = rd_byte(r, &external)) != R7Z_OK)
      return res;
   if (external != 0)
      return R7Z_ERROR_UNSUPPORTED;   /* folders stored elsewhere */

   if (a->num_folders != 0)
   {
      uint32_t nc, nb, np, npr;

      /* Size the arrays by walking the folder list without storing
       * anything, then walk it again for real. */
      if ((res = count_folders(*r, a->num_folders, &nc, &nb, &np, &npr))
            != R7Z_OK)
         return res;

      a->num_coders      = nc;
      a->num_bonds       = nb;
      a->num_folder_pack = np;

      if (nc)
      {
         a->coders = (coder_t *)calloc(nc, sizeof(coder_t));
         if (!a->coders)
            return R7Z_ERROR_MEM;
      }
      if (nb)
      {
         a->bonds = (uint32_t *)calloc(nb, sizeof(uint32_t));
         if (!a->bonds)
            return R7Z_ERROR_MEM;
      }
      if (np)
      {
         a->folder_pack = (uint32_t *)calloc(np, sizeof(uint32_t));
         if (!a->folder_pack)
            return R7Z_ERROR_MEM;
      }
      if (npr)
      {
         a->props = (uint8_t *)calloc(npr, 1);
         if (!a->props)
            return R7Z_ERROR_MEM;
      }

      a->folders = (folder_t *)calloc(a->num_folders, sizeof(folder_t));
      if (!a->folders)
         return R7Z_ERROR_MEM;

      for (i = 0; i < a->num_folders; i++)
      {
         res = parse_folder(a, r, &a->folders[i],
               &coder_pos, &bond_pos, &pack_pos);
         if (res != R7Z_OK)
            return res;
      }
   }

   /* Each folder's packed streams follow one another in file order. */
   {
      uint64_t off = 0;
      uint32_t pk  = 0;
      for (i = 0; i < a->num_folders; i++)
      {
         uint32_t k;
         a->folders[i].pack_offset = off;
         for (k = 0; k < a->folders[i].num_pack; k++)
         {
            if (pk >= a->num_pack)
               return R7Z_ERROR_DATA;
            if (a->pack_sizes[pk] > (uint64_t)a->len - off)
               return R7Z_ERROR_DATA;
            off += a->pack_sizes[pk];
            pk++;
         }
      }
   }

   if ((res = rd_num(r, &id)) != R7Z_OK)
      return res;
   if (id != ID_CODERS_UNPACK_SIZE)
      return R7Z_ERROR_DATA;

   {
      uint32_t total_out = 0;
      for (i = 0; i < a->num_folders; i++)
      {
         uint32_t k;
         for (k = 0; k < a->folders[i].num_coders; k++)
            total_out += a->coders[a->folders[i].coder_first + k].num_out;
      }
      a->num_coder_unpack_sizes = total_out;
      if (total_out)
      {
         a->coder_unpack_sizes =
            (uint64_t *)calloc(total_out, sizeof(uint64_t));
         if (!a->coder_unpack_sizes)
            return R7Z_ERROR_MEM;
         for (i = 0; i < total_out; i++)
            if ((res = rd_num(r, &a->coder_unpack_sizes[i])) != R7Z_OK)
               return res;
      }
   }

   /* Give each folder the size of its own output. */
   {
      uint32_t base = 0;
      for (i = 0; i < a->num_folders; i++)
      {
         folder_t *f = &a->folders[i];
         uint32_t  k;
         uint32_t  outs = 0;

         for (k = 0; k < f->num_coders; k++)
            outs += a->coders[f->coder_first + k].num_out;
         if (f->main_out >= outs || base + f->main_out >= a->num_coder_unpack_sizes)
            return R7Z_ERROR_DATA;
         f->unpack_size = a->coder_unpack_sizes[base + f->main_out];
         base += outs;
      }
   }

   for (;;)
   {
      if ((res = rd_num(r, &id)) != R7Z_OK)
         return res;
      if (id == ID_END)
         break;
      if (id == ID_CRC)
      {
         uint8_t *defs = NULL;
         if (a->num_folders)
         {
            defs = (uint8_t *)calloc(a->num_folders, 1);
            if (!defs)
               return R7Z_ERROR_MEM;
         }
         res = rd_bits_maybe_all(r, defs, a->num_folders);
         if (res == R7Z_OK)
            for (i = 0; i < a->num_folders; i++)
               if (defs[i])
               {
                  uint32_t v;
                  if ((res = rd_u32le(r, &v)) != R7Z_OK)
                     break;
                  a->folders[i].crc     = v;
                  a->folders[i].has_crc = 1;
               }
         free(defs);
         if (res != R7Z_OK)
            return res;
      }
      else if ((res = rd_skip_sized(r)) != R7Z_OK)
         return res;
   }

   return R7Z_OK;
}

/* --------------------------------------------------------------------
 * SubStreamsInfo
 *
 * How each folder's single output splits into files: how many, their
 * sizes (the last one implied by the folder size), and their CRCs.
 * -------------------------------------------------------------------- */

typedef struct
{
   uint64_t *sizes;
   uint32_t *crcs;
   uint8_t  *crc_defined;
   uint32_t  num;
} substreams_t;

static void substreams_free(substreams_t *s)
{
   free(s->sizes);
   free(s->crcs);
   free(s->crc_defined);
   s->sizes = NULL;
   s->crcs = NULL;
   s->crc_defined = NULL;
   s->num = 0;
}

static int parse_substreams_info(r7z_archive_t *a, rd_t *r,
      substreams_t *ss)
{
   uint64_t id;
   uint32_t i, k, pos = 0;
   uint32_t total = 0;
   int      res;
   int      have_counts = 0;

   for (i = 0; i < a->num_folders; i++)
      a->folders[i].num_unpack_streams = 1;

   if ((res = rd_num(r, &id)) != R7Z_OK)
      return res;

   if (id == ID_NUM_UNPACK_STREAM)
   {
      have_counts = 1;
      for (i = 0; i < a->num_folders; i++)
      {
         uint32_t n;
         if ((res = rd_count(r, &n, MAX_ENTRIES, 0)) != R7Z_OK)
            return res;
         a->folders[i].num_unpack_streams = n;
      }
      if ((res = rd_num(r, &id)) != R7Z_OK)
         return res;
   }

   for (i = 0; i < a->num_folders; i++)
   {
      uint32_t n = a->folders[i].num_unpack_streams;
      if (n > MAX_ENTRIES || total > MAX_ENTRIES - n)
         return R7Z_ERROR_DATA;
      total += n;
   }
   ss->num = total;

   if (total)
   {
      ss->sizes = (uint64_t *)calloc(total, sizeof(uint64_t));
      ss->crcs  = (uint32_t *)calloc(total, sizeof(uint32_t));
      ss->crc_defined = (uint8_t *)calloc(total, 1);
      if (!ss->sizes || !ss->crcs || !ss->crc_defined)
         return R7Z_ERROR_MEM;
   }

   /* Sizes: all but the last of each folder are stored; the last is
    * whatever is left of the folder's output. */
   if (id == ID_SIZE)
   {
      for (i = 0; i < a->num_folders; i++)
      {
         uint32_t n = a->folders[i].num_unpack_streams;
         uint64_t sum = 0;

         if (n == 0)
            continue;
         for (k = 0; k + 1 < n; k++)
         {
            uint64_t v;
            if ((res = rd_num(r, &v)) != R7Z_OK)
               return res;
            if (v > a->folders[i].unpack_size - sum)
               return R7Z_ERROR_DATA;
            ss->sizes[pos + k] = v;
            sum += v;
         }
         ss->sizes[pos + n - 1] = a->folders[i].unpack_size - sum;
         pos += n;
      }
      if ((res = rd_num(r, &id)) != R7Z_OK)
         return res;
   }
   else
   {
      /* No explicit sizes: each folder holds exactly one stream, its
       * whole output. */
      for (i = 0; i < a->num_folders; i++)
      {
         if (a->folders[i].num_unpack_streams == 1)
            ss->sizes[pos] = a->folders[i].unpack_size;
         else if (a->folders[i].num_unpack_streams != 0)
            return R7Z_ERROR_DATA;
         pos += a->folders[i].num_unpack_streams;
      }
   }

   for (;;)
   {
      if (id == ID_END)
         break;
      if (id == ID_CRC)
      {
         /* CRCs are stored only for streams that do not already have
          * one as a whole-folder CRC. */
         uint32_t num_unknown = 0;
         uint8_t *defs;
         uint32_t di = 0;

         for (i = 0; i < a->num_folders; i++)
            if (a->folders[i].num_unpack_streams != 1
                  || !a->folders[i].has_crc)
               num_unknown += a->folders[i].num_unpack_streams;

         defs = num_unknown
            ? (uint8_t *)calloc(num_unknown, 1) : NULL;
         if (num_unknown && !defs)
            return R7Z_ERROR_MEM;

         res = rd_bits_maybe_all(r, defs, num_unknown);
         if (res != R7Z_OK)
         {
            free(defs);
            return res;
         }

         pos = 0;
         for (i = 0; i < a->num_folders && res == R7Z_OK; i++)
         {
            uint32_t n = a->folders[i].num_unpack_streams;
            if (n == 1 && a->folders[i].has_crc)
            {
               ss->crcs[pos] = a->folders[i].crc;
               ss->crc_defined[pos] = 1;
               pos++;
               continue;
            }
            for (k = 0; k < n; k++)
            {
               if (di < num_unknown && defs[di])
               {
                  uint32_t v;
                  if ((res = rd_u32le(r, &v)) != R7Z_OK)
                     break;
                  ss->crcs[pos] = v;
                  ss->crc_defined[pos] = 1;
               }
               di++;
               pos++;
            }
         }
         free(defs);
         if (res != R7Z_OK)
            return res;
      }
      else if ((res = rd_skip_sized(r)) != R7Z_OK)
         return res;

      if ((res = rd_num(r, &id)) != R7Z_OK)
         return res;
   }

   (void)have_counts;
   return R7Z_OK;
}

/* --------------------------------------------------------------------
 * FilesInfo
 * -------------------------------------------------------------------- */

static int parse_files_info(r7z_archive_t *a, rd_t *r, substreams_t *ss)
{
   uint32_t num_files;
   uint8_t *empty_stream = NULL;
   uint8_t *empty_file   = NULL;
   uint32_t num_empty    = 0;
   int      res;

   if ((res = rd_count(r, &num_files, MAX_ENTRIES, 1)) != R7Z_OK)
      return res;

   a->num_entries = num_files;
   if (num_files)
   {
      a->entries = (r7z_entry_t *)calloc(num_files, sizeof(r7z_entry_t));
      empty_stream = (uint8_t *)calloc(num_files, 1);
      if (!a->entries || !empty_stream)
      {
         free(empty_stream);
         return R7Z_ERROR_MEM;
      }
   }

   for (;;)
   {
      uint64_t id, size;
      const uint8_t *next;

      if ((res = rd_num(r, &id)) != R7Z_OK)
         goto fail;
      if (id == ID_END)
         break;
      if ((res = rd_num(r, &size)) != R7Z_OK)
         goto fail;
      if (size > (uint64_t)(r->end - r->p))
      {
         res = R7Z_ERROR_DATA;
         goto fail;
      }
      next = r->p + (size_t)size;

      switch (id)
      {
         case ID_EMPTY_STREAM:
            if ((res = rd_bits(r, empty_stream, num_files)) != R7Z_OK)
               goto fail;
            {
               uint32_t i;
               num_empty = 0;
               for (i = 0; i < num_files; i++)
                  if (empty_stream[i])
                     num_empty++;
            }
            break;

         case ID_EMPTY_FILE:
            if (num_empty)
            {
               empty_file = (uint8_t *)calloc(num_empty, 1);
               if (!empty_file)
               {
                  res = R7Z_ERROR_MEM;
                  goto fail;
               }
               if ((res = rd_bits(r, empty_file, num_empty)) != R7Z_OK)
                  goto fail;
            }
            break;

         case ID_NAME:
         {
            uint8_t  external;
            size_t   avail;
            size_t   nchars;
            size_t   i;

            if ((res = rd_byte(r, &external)) != R7Z_OK)
               goto fail;
            if (external != 0)
            {
               res = R7Z_ERROR_UNSUPPORTED;
               goto fail;
            }
            avail = (size_t)(next - r->p);
            if ((avail & 1) != 0)
            {
               res = R7Z_ERROR_DATA;
               goto fail;
            }
            nchars = avail / 2;
            a->names = (uint16_t *)calloc(nchars ? nchars : 1,
                  sizeof(uint16_t));
            if (!a->names)
            {
               res = R7Z_ERROR_MEM;
               goto fail;
            }
            for (i = 0; i < nchars; i++)
               a->names[i] = (uint16_t)((uint16_t)r->p[i * 2]
                     | ((uint16_t)r->p[i * 2 + 1] << 8));
            a->names_len = nchars;

            /* Point each entry at its name and check every one is
             * terminated inside the block. */
            {
               size_t   at = 0;
               uint32_t fi;
               for (fi = 0; fi < num_files; fi++)
               {
                  size_t start = at;
                  while (at < nchars && a->names[at] != 0)
                     at++;
                  if (at >= nchars)
                  {
                     res = R7Z_ERROR_DATA;
                     goto fail;
                  }
                  a->entries[fi].name = a->names + start;
                  at++;
               }
            }
            break;
         }

         default:
            break;
      }

      r->p = next;
   }

   /* Attach stream data to the entries that have any. */
   {
      uint32_t fi;
      uint32_t si = 0;
      uint32_t ei = 0;
      uint32_t folder = 0;
      uint32_t in_folder = 0;
      uint64_t off = 0;

      for (fi = 0; fi < num_files; fi++)
      {
         r7z_entry_t *e = &a->entries[fi];

         if (empty_stream[fi])
         {
            /* No stream: either a directory or a zero-length file. */
            e->size   = 0;
            e->is_dir = (empty_file && ei < num_empty)
               ? (uint32_t)(!empty_file[ei]) : 1u;
            e->folder = 0xFFFFFFFFu;
            ei++;
            continue;
         }

         if (si >= ss->num)
         {
            res = R7Z_ERROR_DATA;
            goto fail;
         }

         /* Walk to the folder this stream belongs to. */
         while (folder < a->num_folders
               && in_folder >= a->folders[folder].num_unpack_streams)
         {
            folder++;
            in_folder = 0;
            off = 0;
         }
         if (folder >= a->num_folders)
         {
            res = R7Z_ERROR_DATA;
            goto fail;
         }

         e->size     = ss->sizes[si];
         e->crc      = ss->crcs[si];
         e->has_crc  = ss->crc_defined[si];
         e->is_dir   = 0;
         e->folder   = folder;
         e->offset_in_folder = off;

         off += e->size;
         in_folder++;
         si++;
      }
   }

   free(empty_stream);
   free(empty_file);
   return R7Z_OK;

fail:
   free(empty_stream);
   free(empty_file);
   return res;
}

/* --------------------------------------------------------------------
 * Folder decoding
 *
 * A folder is decoded by running its coder chain from the packed
 * streams to the folder output. Only simple shapes are accepted: one
 * chain of single-input coders, or a BCJ2 coder taking four inputs at
 * the end of one.
 * -------------------------------------------------------------------- */

static int decode_coder(r7z_archive_t *a, const coder_t *c,
      const uint8_t *in, size_t in_len,
      uint8_t *out, size_t out_len)
{
   switch (c->method)
   {
      case METHOD_COPY:
         if (in_len != out_len)
            return R7Z_ERROR_DATA;
         memcpy(out, in, out_len);
         return R7Z_OK;

      case METHOD_LZMA:
      {
         rlzma_dec_t *dec;
         int          res;

         if (c->prop_size < RLZMA_PROPS_SIZE)
            return R7Z_ERROR_DATA;
         dec = (rlzma_dec_t *)malloc(sizeof(*dec));
         if (!dec)
            return R7Z_ERROR_MEM;
         if (rlzma_dec_init(dec, a->props + c->prop_offset) != RLZMA_OK)
         {
            free(dec);
            return R7Z_ERROR_DATA;
         }
         res = rlzma_dec_decode(dec, out, out_len, in, in_len);
         free(dec);
         return (res == RLZMA_OK) ? R7Z_OK : R7Z_ERROR_DATA;
      }

      case METHOD_LZMA2:
      {
         rlzma2_dec_t *dec;
         uint16_t     *probs;
         size_t        fed = 0;
         size_t        produced = 0;
         int           res = R7Z_OK;

         if (c->prop_size < 1)
            return R7Z_ERROR_DATA;

         dec   = (rlzma2_dec_t *)malloc(sizeof(*dec));
         probs = (uint16_t *)malloc(RLZMA2_NUM_PROBS * sizeof(uint16_t));
         if (!dec || !probs)
         {
            free(dec);
            free(probs);
            return R7Z_ERROR_MEM;
         }

         /* The output buffer is the dictionary window. Every folder is
          * decoded whole, so it is always large enough for any
          * distance the stream can legally use. */
         if (rlzma2_dec_init(dec, a->props[c->prop_offset], probs,
                  out, out_len ? out_len : 1) != RLZMA_OK)
         {
            free(dec);
            free(probs);
            return R7Z_ERROR_DATA;
         }

         while (produced < out_len)
         {
            size_t got = in_len - fed;
            int    status;

            if (rlzma2_dec_decode(dec, out_len, in + fed, &got, 1,
                     &status) != RLZMA_OK)
            {
               res = R7Z_ERROR_DATA;
               break;
            }
            fed      += got;
            produced  = dec->lzma.dic_pos;
            if (status == RLZMA2_STATUS_FINISHED)
               break;
            if (got == 0 && fed >= in_len)
               break;
         }
         if (res == R7Z_OK && produced != out_len)
            res = R7Z_ERROR_DATA;

         free(dec);
         free(probs);
         return res;
      }

      case METHOD_DELTA:
      {
         uint32_t dist;
         if (c->prop_size < 1)
            return R7Z_ERROR_DATA;
         if (in_len != out_len)
            return R7Z_ERROR_DATA;
         dist = (uint32_t)a->props[c->prop_offset] + 1;
         if (dist > RFILTERS_DELTA_STATE_SIZE)
            return R7Z_ERROR_DATA;
         memcpy(out, in, out_len);
         {
            uint8_t state[RFILTERS_DELTA_STATE_SIZE];
            memset(state, 0, sizeof(state));
            rfilters_delta_decode(state, (unsigned)dist, out, out_len);
         }
         return R7Z_OK;
      }

      case METHOD_BCJ:
      case METHOD_BCJ_X86:
         if (in_len != out_len)
            return R7Z_ERROR_DATA;
         memcpy(out, in, out_len);
         {
            uint32_t state = 0;
            rfilters_x86_decode(&state, 0, out, out_len);
         }
         return R7Z_OK;

      case METHOD_BRA_ARM:
         if (in_len != out_len)
            return R7Z_ERROR_DATA;
         memcpy(out, in, out_len);
         rfilters_arm_decode(0, out, out_len);
         return R7Z_OK;

      case METHOD_BRA_ARMT:
         if (in_len != out_len)
            return R7Z_ERROR_DATA;
         memcpy(out, in, out_len);
         rfilters_armt_decode(0, out, out_len);
         return R7Z_OK;

      case METHOD_BRA_PPC:
         if (in_len != out_len)
            return R7Z_ERROR_DATA;
         memcpy(out, in, out_len);
         rfilters_ppc_decode(0, out, out_len);
         return R7Z_OK;

      case METHOD_BRA_SPARC:
         if (in_len != out_len)
            return R7Z_ERROR_DATA;
         memcpy(out, in, out_len);
         rfilters_sparc_decode(0, out, out_len);
         return R7Z_OK;

      case METHOD_BRA_IA64:
         if (in_len != out_len)
            return R7Z_ERROR_DATA;
         memcpy(out, in, out_len);
         rfilters_ia64_decode(0, out, out_len);
         return R7Z_OK;

      default:
         break;
   }
   return R7Z_ERROR_UNSUPPORTED;
}


/* Decode a BCJ2 folder.
 *
 * The BCJ2 coder takes four inputs: main, call, jump and rc. Each is
 * fed either straight from a packed stream or through a chain of
 * single-input coders (7z usually compresses main and call with LZMA).
 * This resolves each input independently and then runs the converter.
 */
static int folder_input_offsets(r7z_archive_t *a, uint32_t fi,
      uint32_t *pack_first_index, uint64_t *pack_file_offset)
{
   uint32_t k;
   uint32_t pk = 0;

   for (k = 0; k < fi; k++)
      pk += a->folders[k].num_pack;
   if (pk >= a->num_pack)
      return R7Z_ERROR_DATA;
   *pack_first_index = pk;
   *pack_file_offset = a->pack_base + a->folders[fi].pack_offset;
   return R7Z_OK;
}

/* Produce the bytes arriving at one input port of the folder's coder
 * graph. Returns a malloc'd buffer the caller frees. */
static int resolve_input(r7z_archive_t *a, uint32_t fi, uint32_t port,
      uint32_t coder_out_base, uint8_t **out, size_t *out_len)
{
   folder_t *f = &a->folders[fi];
   uint32_t  b;
   uint32_t  pk_first;
   uint64_t  pk_off;
   uint32_t  i;
   int       res;

   if ((res = folder_input_offsets(a, fi, &pk_first, &pk_off)) != R7Z_OK)
      return res;

   /* Fed by a packed stream? */
   for (i = 0; i < f->num_pack; i++)
   {
      if (a->folder_pack[f->pack_first + i] == port)
      {
         uint64_t off = pk_off;
         uint32_t k;
         size_t   n;

         for (k = 0; k < i; k++)
         {
            if (pk_first + k >= a->num_pack)
               return R7Z_ERROR_DATA;
            off += a->pack_sizes[pk_first + k];
         }
         if (pk_first + i >= a->num_pack)
            return R7Z_ERROR_DATA;
         if (a->pack_sizes[pk_first + i] > (uint64_t)((size_t)-1))
            return R7Z_ERROR_DATA;
         n = (size_t)a->pack_sizes[pk_first + i];
         if (off > (uint64_t)a->len - 32
               || (uint64_t)n > (uint64_t)a->len - 32 - off)
            return R7Z_ERROR_DATA;

         *out = (uint8_t *)malloc(n ? n : 1);
         if (!*out)
            return R7Z_ERROR_MEM;
         memcpy(*out, a->data + 32 + off, n);
         *out_len = n;
         return R7Z_OK;
      }
   }

   /* Otherwise a bond feeds it from some coder's output. */
   for (b = 0; b < f->num_bonds; b++)
   {
      if (a->bonds[f->bond_first + b * 2] != port)
         continue;
      {
         uint32_t out_port = a->bonds[f->bond_first + b * 2 + 1];
         uint32_t ci;
         uint32_t seen = 0;

         /* Which coder owns that output port. */
         for (ci = 0; ci < f->num_coders; ci++)
         {
            const coder_t *c = &a->coders[f->coder_first + ci];
            if (out_port < seen + c->num_out)
            {
               uint8_t *in = NULL;
               size_t   in_len = 0;
               size_t   want;
               uint8_t *dst;
               uint32_t in_base = 0;
               uint32_t z;

               if (c->num_in != 1)
                  return R7Z_ERROR_UNSUPPORTED;

               /* Input port index of this coder. */
               for (z = 0; z < ci; z++)
                  in_base += a->coders[f->coder_first + z].num_in;

               res = resolve_input(a, fi, in_base, coder_out_base,
                     &in, &in_len);
               if (res != R7Z_OK)
                  return res;

               if (coder_out_base + out_port >= a->num_coder_unpack_sizes)
               {
                  free(in);
                  return R7Z_ERROR_DATA;
               }
               if (a->coder_unpack_sizes[coder_out_base + out_port]
                     > (uint64_t)((size_t)-1))
               {
                  free(in);
                  return R7Z_ERROR_DATA;
               }
               want = (size_t)a->coder_unpack_sizes[coder_out_base + out_port];

               dst = (uint8_t *)malloc(want ? want : 1);
               if (!dst)
               {
                  free(in);
                  return R7Z_ERROR_MEM;
               }
               res = decode_coder(a, c, in, in_len, dst, want);
               free(in);
               if (res != R7Z_OK)
               {
                  free(dst);
                  return res;
               }
               *out     = dst;
               *out_len = want;
               return R7Z_OK;
            }
            seen += c->num_out;
         }
         return R7Z_ERROR_DATA;
      }
   }

   return R7Z_ERROR_DATA;
}

static int decode_folder_bcj2(r7z_archive_t *a, uint32_t fi,
      uint32_t bcj2_at, uint8_t **out)
{
   folder_t *f = &a->folders[fi];
   uint8_t  *bufs[4];
   size_t    lens[4];
   uint32_t  in_base = 0;
   uint32_t  coder_out_base = 0;
   uint32_t  i;
   uint8_t  *dst = NULL;
   size_t    dst_len;
   int       res = R7Z_OK;

   for (i = 0; i < 4; i++)
   {
      bufs[i] = NULL;
      lens[i] = 0;
   }

   if (a->coders[f->coder_first + bcj2_at].num_in != 4)
      return R7Z_ERROR_UNSUPPORTED;

   for (i = 0; i < bcj2_at; i++)
      in_base += a->coders[f->coder_first + i].num_in;
   for (i = 0; i < fi; i++)
   {
      uint32_t k;
      for (k = 0; k < a->folders[i].num_coders; k++)
         coder_out_base += a->coders[a->folders[i].coder_first + k].num_out;
   }

   for (i = 0; i < 4; i++)
   {
      res = resolve_input(a, fi, in_base + i, coder_out_base,
            &bufs[i], &lens[i]);
      if (res != R7Z_OK)
         goto done;
   }

   if (f->unpack_size > (uint64_t)((size_t)-1))
   {
      res = R7Z_ERROR_DATA;
      goto done;
   }
   dst_len = (size_t)f->unpack_size;
   dst = (uint8_t *)malloc(dst_len ? dst_len : 1);
   if (!dst)
   {
      res = R7Z_ERROR_MEM;
      goto done;
   }

   res = r7z_bcj2_decode(dst, dst_len,
         bufs[0], lens[0], bufs[1], lens[1],
         bufs[2], lens[2], bufs[3], lens[3]);
   if (res != RBCJ2_OK)
   {
      res = R7Z_ERROR_DATA;
      free(dst);
      dst = NULL;
      goto done;
   }

   if (f->has_crc && crc_calc(dst, dst_len) != f->crc)
   {
      res = R7Z_ERROR_CRC;
      free(dst);
      dst = NULL;
      goto done;
   }

   *out = dst;
   res  = R7Z_OK;

done:
   for (i = 0; i < 4; i++)
      free(bufs[i]);
   return res;
}

/* Decode one folder in full. Returns a malloc'd buffer of
 * f->unpack_size bytes. */
static int decode_folder(r7z_archive_t *a, uint32_t fi, uint8_t **out)
{
   folder_t *f;
   uint8_t  *cur      = NULL;
   size_t    cur_len  = 0;
   uint32_t  ci;
   int       res;

   if (fi >= a->num_folders)
      return R7Z_ERROR_PARAM;
   f = &a->folders[fi];

   if (f->unpack_size > (uint64_t)((size_t)-1))
      return R7Z_ERROR_DATA;

   /* BCJ2 folders are the one branching shape this reader accepts: a
    * BCJ2 coder with four inputs, each fed either by a packed stream
    * directly or by the output of a single-input coder chain hanging
    * off one. Anything else is rejected rather than guessed at. */
   {
      uint32_t bcj2_at = 0xFFFFFFFFu;
      for (ci = 0; ci < f->num_coders; ci++)
         if (a->coders[f->coder_first + ci].method == METHOD_BCJ2)
            bcj2_at = ci;

      if (bcj2_at != 0xFFFFFFFFu)
         return decode_folder_bcj2(a, fi, bcj2_at, out);
   }

   /* Simple chain: the packed stream feeds coder 0, whose output feeds
    * coder 1, and so on. Sizes come from coder_unpack_sizes. */
   {
      uint32_t base = 0;
      uint32_t k;
      uint64_t pack_off;
      uint32_t pk;

      for (k = 0; k < fi; k++)
      {
         uint32_t m;
         for (m = 0; m < a->folders[k].num_coders; m++)
            base += a->coders[a->folders[k].coder_first + m].num_out;
      }

      /* Locate this folder's packed bytes. */
      pk = 0;
      for (k = 0; k < fi; k++)
         pk += a->folders[k].num_pack;
      if (pk >= a->num_pack)
         return R7Z_ERROR_DATA;
      pack_off = a->pack_base + f->pack_offset;
      if (pack_off > (uint64_t)a->len)
         return R7Z_ERROR_DATA;

      cur_len = (size_t)a->pack_sizes[pk];
      if (cur_len > a->len - (size_t)pack_off - 32)
      {
         /* 32 accounts for the signature header the offsets are
          * relative to. */
         if ((uint64_t)cur_len + pack_off + 32 > (uint64_t)a->len)
            return R7Z_ERROR_DATA;
      }
      cur = (uint8_t *)malloc(cur_len ? cur_len : 1);
      if (!cur)
         return R7Z_ERROR_MEM;
      memcpy(cur, a->data + 32 + pack_off, cur_len);

      for (ci = 0; ci < f->num_coders; ci++)
      {
         const coder_t *c = &a->coders[f->coder_first + ci];
         uint8_t       *next;
         size_t         next_len;

         if (base + ci >= a->num_coder_unpack_sizes)
         {
            free(cur);
            return R7Z_ERROR_DATA;
         }
         if (a->coder_unpack_sizes[base + ci] > (uint64_t)((size_t)-1))
         {
            free(cur);
            return R7Z_ERROR_DATA;
         }
         next_len = (size_t)a->coder_unpack_sizes[base + ci];

         next = (uint8_t *)malloc(next_len ? next_len : 1);
         if (!next)
         {
            free(cur);
            return R7Z_ERROR_MEM;
         }

         res = decode_coder(a, c, cur, cur_len, next, next_len);
         free(cur);
         if (res != R7Z_OK)
         {
            free(next);
            return res;
         }
         cur     = next;
         cur_len = next_len;
      }
   }

   if (cur_len != (size_t)f->unpack_size)
   {
      free(cur);
      return R7Z_ERROR_DATA;
   }

   if (f->has_crc && crc_calc(cur, cur_len) != f->crc)
   {
      free(cur);
      return R7Z_ERROR_CRC;
   }

   *out = cur;
   return R7Z_OK;
}

/* --------------------------------------------------------------------
 * Header parsing and the public entry points
 * -------------------------------------------------------------------- */

static int parse_streams_info(r7z_archive_t *a, rd_t *r, substreams_t *ss)
{
   uint64_t id;
   int      res;
   int      seen_sub = 0;

   if ((res = rd_num(r, &id)) != R7Z_OK)
      return res;

   if (id == ID_PACK_INFO)
   {
      if ((res = parse_pack_info(a, r)) != R7Z_OK)
         return res;
      if ((res = rd_num(r, &id)) != R7Z_OK)
         return res;
   }

   if (id == ID_UNPACK_INFO)
   {
      if ((res = parse_unpack_info(a, r)) != R7Z_OK)
         return res;
      if ((res = rd_num(r, &id)) != R7Z_OK)
         return res;
   }

   if (id == ID_SUBSTREAMS_INFO)
   {
      if ((res = parse_substreams_info(a, r, ss)) != R7Z_OK)
         return res;
      seen_sub = 1;
      if ((res = rd_num(r, &id)) != R7Z_OK)
         return res;
   }

   if (!seen_sub)
   {
      /* No SubStreamsInfo: one stream per folder, each the whole
       * folder output. */
      uint32_t i;
      ss->num = a->num_folders;
      if (ss->num)
      {
         ss->sizes = (uint64_t *)calloc(ss->num, sizeof(uint64_t));
         ss->crcs  = (uint32_t *)calloc(ss->num, sizeof(uint32_t));
         ss->crc_defined = (uint8_t *)calloc(ss->num, 1);
         if (!ss->sizes || !ss->crcs || !ss->crc_defined)
            return R7Z_ERROR_MEM;
         for (i = 0; i < a->num_folders; i++)
         {
            a->folders[i].num_unpack_streams = 1;
            ss->sizes[i] = a->folders[i].unpack_size;
            ss->crcs[i]  = a->folders[i].crc;
            ss->crc_defined[i] = (uint8_t)a->folders[i].has_crc;
         }
      }
   }

   if (id != ID_END)
      return R7Z_ERROR_DATA;
   return R7Z_OK;
}

static int parse_header(r7z_archive_t *a, rd_t *r)
{
   uint64_t     id;
   substreams_t ss;
   int          res;

   memset(&ss, 0, sizeof(ss));

   if ((res = rd_num(r, &id)) != R7Z_OK)
      return res;
   if (id != ID_HEADER)
      return R7Z_ERROR_DATA;

   if ((res = rd_num(r, &id)) != R7Z_OK)
      return res;

   if (id == ID_ARCHIVE_PROPERTIES)
   {
      for (;;)
      {
         uint64_t pid;
         if ((res = rd_num(r, &pid)) != R7Z_OK)
            return res;
         if (pid == ID_END)
            break;
         if ((res = rd_skip_sized(r)) != R7Z_OK)
            return res;
      }
      if ((res = rd_num(r, &id)) != R7Z_OK)
         return res;
   }

   if (id == ID_ADDITIONAL_STREAMS)
      return R7Z_ERROR_UNSUPPORTED;

   if (id == ID_MAIN_STREAMS)
   {
      if ((res = parse_streams_info(a, r, &ss)) != R7Z_OK)
      {
         substreams_free(&ss);
         return res;
      }
      if ((res = rd_num(r, &id)) != R7Z_OK)
      {
         substreams_free(&ss);
         return res;
      }
   }

   if (id == ID_FILES_INFO)
   {
      res = parse_files_info(a, r, &ss);
      substreams_free(&ss);
      if (res != R7Z_OK)
         return res;
      if ((res = rd_num(r, &id)) != R7Z_OK)
         return res;
   }
   else
      substreams_free(&ss);

   if (id != ID_END)
      return R7Z_ERROR_DATA;
   return R7Z_OK;
}

/* An encoded header is a one-folder archive whose decoded output is
 * the real header. Parse just enough to decode that folder, then start
 * again on the result. */
static int decode_encoded_header(r7z_archive_t *a, rd_t *r,
      uint8_t **out, size_t *out_len)
{
   r7z_archive_t tmp;
   substreams_t  ss;
   int           res;
   uint8_t      *buf = NULL;

   memset(&tmp, 0, sizeof(tmp));
   memset(&ss, 0, sizeof(ss));
   tmp.data = a->data;
   tmp.len  = a->len;

   res = parse_streams_info(&tmp, r, &ss);
   substreams_free(&ss);

   if (res == R7Z_OK)
   {
      if (tmp.num_folders != 1)
         res = R7Z_ERROR_DATA;
      else
         res = decode_folder(&tmp, 0, &buf);
   }

   if (res == R7Z_OK)
   {
      *out     = buf;
      *out_len = (size_t)tmp.folders[0].unpack_size;
   }

   free(tmp.pack_sizes);
   free(tmp.coders);
   free(tmp.props);
   free(tmp.bonds);
   free(tmp.folder_pack);
   free(tmp.coder_unpack_sizes);
   free(tmp.folders);
   return res;
}

int r7z_archive_open(r7z_archive_t **out, const uint8_t *data, size_t len)
{
   static const uint8_t sig[R7Z_SIGNATURE_SIZE] =
      { '7', 'z', 0xBC, 0xAF, 0x27, 0x1C };
   r7z_archive_t *a;
   uint64_t       next_off, next_size;
   uint32_t       next_crc;
   rd_t           r;
   int            res;

   if (!out || !data)
      return R7Z_ERROR_PARAM;
   *out = NULL;

   if (len < 32)
      return R7Z_ERROR_DATA;
   if (memcmp(data, sig, R7Z_SIGNATURE_SIZE) != 0)
      return R7Z_ERROR_DATA;

   next_off  = (uint64_t)data[12] | ((uint64_t)data[13] << 8)
             | ((uint64_t)data[14] << 16) | ((uint64_t)data[15] << 24)
             | ((uint64_t)data[16] << 32) | ((uint64_t)data[17] << 40)
             | ((uint64_t)data[18] << 48) | ((uint64_t)data[19] << 56);
   next_size = (uint64_t)data[20] | ((uint64_t)data[21] << 8)
             | ((uint64_t)data[22] << 16) | ((uint64_t)data[23] << 24)
             | ((uint64_t)data[24] << 32) | ((uint64_t)data[25] << 40)
             | ((uint64_t)data[26] << 48) | ((uint64_t)data[27] << 56);
   next_crc  = (uint32_t)data[28] | ((uint32_t)data[29] << 8)
             | ((uint32_t)data[30] << 16) | ((uint32_t)data[31] << 24);

   /* The header sits after the 32-byte signature block. Both bounds
    * are checked against the real file length before either is used. */
   if (next_off > (uint64_t)len - 32)
      return R7Z_ERROR_DATA;
   if (next_size > (uint64_t)len - 32 - next_off)
      return R7Z_ERROR_DATA;
   if (next_size == 0)
      return R7Z_ERROR_DATA;

   if (crc_calc(data + 32 + next_off, (size_t)next_size) != next_crc)
      return R7Z_ERROR_CRC;

   a = (r7z_archive_t *)calloc(1, sizeof(*a));
   if (!a)
      return R7Z_ERROR_MEM;
   a->data          = data;
   a->len           = len;
   a->cached_folder = 0xFFFFFFFFu;
   a->pend_folder   = 0xFFFFFFFFu;

   r.p   = data + 32 + next_off;
   r.end = r.p + (size_t)next_size;

   /* An encoded header needs decoding before it can be parsed. */
   {
      uint64_t id;
      rd_t     probe = r;

      if (rd_num(&probe, &id) == R7Z_OK && id == ID_ENCODED_HEADER)
      {
         uint8_t *hdr = NULL;
         size_t   hdr_len = 0;

         res = decode_encoded_header(a, &probe, &hdr, &hdr_len);
         if (res != R7Z_OK)
         {
            r7z_archive_close(a);
            return res;
         }
         a->header_buf = hdr;
         r.p   = hdr;
         r.end = hdr + hdr_len;
      }
   }

   res = parse_header(a, &r);
   if (res != R7Z_OK)
   {
      r7z_archive_close(a);
      return res;
   }

   *out = a;
   return R7Z_OK;
}

void r7z_archive_close(r7z_archive_t *a)
{
   if (!a)
      return;
   decode_pending_reset(a);
   free(a->cached_data);
   free(a->header_buf);
   free(a->pack_sizes);
   free(a->coders);
   free(a->props);
   free(a->bonds);
   free(a->folder_pack);
   free(a->coder_unpack_sizes);
   free(a->folders);
   free(a->entries);
   free(a->names);
   free(a);
}

uint32_t r7z_archive_num_entries(const r7z_archive_t *a)
{
   return a ? a->num_entries : 0;
}

const r7z_entry_t *r7z_archive_entry(const r7z_archive_t *a, uint32_t index)
{
   if (!a || index >= a->num_entries)
      return NULL;
   return &a->entries[index];
}


/* --------------------------------------------------------------------
 * Resumable folder decode
 *
 * decode_folder() above runs a folder's whole coder chain in one call.
 * That is right for callers that want the bytes now, but it is also a
 * single uninterruptible block of work: a 2.8 MiB solid folder takes
 * about 67 ms, four dropped frames.
 *
 * These two functions do the same work in bounded slices. Only the
 * LZMA and LZMA2 stages pause, because they are 97% of the time and
 * their decoders already accept an output limit. Every other coder,
 * the branch filters included, is a single linear pass of one to three
 * milliseconds over the whole buffer, so it runs to completion within
 * one slice rather than being restructured.
 * -------------------------------------------------------------------- */

/* Bytes of folder output to produce per slice. On incompressible data
 * one slice of this size costs about 1.8 ms, roughly a ninth of a
 * 60 Hz frame, which leaves room for everything else in the frame.
 * Larger slices scale linearly: 256 KiB is already 7 ms. */
#define R7Z_SLICE_BYTES (64 * 1024)

static void decode_pending_reset(r7z_archive_t *a)
{
   if (!a)
      return;
   free(a->pend_in);
   free(a->pend_out);
   free(a->pend_dec);
   free(a->pend_probs);
   a->pend_in     = NULL;
   a->pend_out    = NULL;
   a->pend_dec    = NULL;
   a->pend_probs  = NULL;
   a->pend_in_len = 0;
   a->pend_out_len = 0;
   a->pend_fed    = 0;
   {
      uint32_t k;
      for (k = 0; k < 4; k++)
      {
         free(a->pend_b2buf[k]);
         a->pend_b2buf[k] = NULL;
         a->pend_b2len[k] = 0;
      }
   }
   free(a->pend_b2st);
   free(a->pend_b2dst);
   a->pend_b2st     = NULL;
   a->pend_b2dst    = NULL;
   a->pend_b2dstlen = 0;
   a->pend_b2done   = 0;
   a->pend_b2port   = 0;
   a->pend_b2at     = 0;
   a->pend_b2in     = 0;
   a->pend_b2active = 0;

   a->pend_folder = 0xFFFFFFFFu;
   a->pend_coder  = 0;
   a->pend_base   = 0;
}

/* True when this coder is one whose decode can be paused. Both LZMA
 * flavours qualify: LZMA2 through r7z_lzma2, plain LZMA through
 * r7z_lzma_stream. Everything else is a single linear pass. */
static int coder_is_resumable(const coder_t *c)
{
   return (c->method == METHOD_LZMA2 || c->method == METHOD_LZMA);
}

/* Run one slice of a paused LZMA2 stage. Returns R7Z_OK with *done set
 * when the stage has produced its whole output. */
static int decode_slice_lzma2(r7z_archive_t *a, const coder_t *c, int *done)
{
   rlzma2_dec_t *dec = (rlzma2_dec_t *)a->pend_dec;
   size_t        limit;
   size_t        got;
   int           status;

   *done = 0;

   limit = dec->lzma.dic_pos + R7Z_SLICE_BYTES;
   if (limit > a->pend_out_len)
      limit = a->pend_out_len;

   got = a->pend_in_len - a->pend_fed;

   if (rlzma2_dec_decode(dec, limit, a->pend_in + a->pend_fed, &got, 1,
            &status) != RLZMA_OK)
      return R7Z_ERROR_DATA;

   a->pend_fed += got;

   if (dec->lzma.dic_pos >= a->pend_out_len
         || status == RLZMA2_STATUS_FINISHED)
   {
      if (dec->lzma.dic_pos != a->pend_out_len)
         return R7Z_ERROR_DATA;
      *done = 1;
      return R7Z_OK;
   }

   /* No input consumed and none left: the stream ended early. */
   if (got == 0 && a->pend_fed >= a->pend_in_len)
      return R7Z_ERROR_DATA;

   (void)c;
   return R7Z_OK;
}

/* Run one slice of a paused plain-LZMA stage. */
static int decode_slice_lzma(r7z_archive_t *a, int *done)
{
   rlzma_stream_t *s = (rlzma_stream_t *)a->pend_dec;
   size_t          limit;
   size_t          got;
   int             status;

   *done = 0;

   limit = s->dic_pos + R7Z_SLICE_BYTES;
   if (limit > a->pend_out_len)
      limit = a->pend_out_len;

   got = a->pend_in_len - a->pend_fed;

   if (rlzma_stream_decode(s, limit, a->pend_in + a->pend_fed, &got, 1,
            &status) != RLZMA_OK)
      return R7Z_ERROR_DATA;

   a->pend_fed += got;

   if (s->dic_pos >= a->pend_out_len
         || status == RLZMA_STATUS_FINISHED)
   {
      if (s->dic_pos != a->pend_out_len)
         return R7Z_ERROR_DATA;
      *done = 1;
      return R7Z_OK;
   }

   if (got == 0 && a->pend_fed >= a->pend_in_len)
      return R7Z_ERROR_DATA;

   return R7Z_OK;
}

/* Set up the stage at a->pend_coder, ready for slicing or immediate
 * execution. */
static int decode_stage_begin(r7z_archive_t *a, const folder_t *f)
{
   const coder_t *c = &a->coders[f->coder_first + a->pend_coder];
   size_t         out_len;

   if (a->pend_base + a->pend_coder >= a->num_coder_unpack_sizes)
      return R7Z_ERROR_DATA;
   if (a->coder_unpack_sizes[a->pend_base + a->pend_coder]
         > (uint64_t)((size_t)-1))
      return R7Z_ERROR_DATA;

   out_len = (size_t)a->coder_unpack_sizes[a->pend_base + a->pend_coder];

   if (!(a->pend_out = (uint8_t *)malloc(out_len ? out_len : 1)))
      return R7Z_ERROR_MEM;
   a->pend_out_len = out_len;
   a->pend_fed     = 0;

   if (!coder_is_resumable(c))
      return R7Z_OK;

   /* Stand up a decoder that survives between slices. */
   if (c->method == METHOD_LZMA2)
   {
      if (c->prop_size < 1)
         return R7Z_ERROR_DATA;

      a->pend_dec   = malloc(sizeof(rlzma2_dec_t));
      a->pend_probs = (uint16_t *)malloc(RLZMA2_NUM_PROBS
            * sizeof(uint16_t));
      if (!a->pend_dec || !a->pend_probs)
         return R7Z_ERROR_MEM;

      if (rlzma2_dec_init((rlzma2_dec_t *)a->pend_dec,
               a->props[c->prop_offset], a->pend_probs,
               a->pend_out, a->pend_out_len ? a->pend_out_len : 1)
            != RLZMA_OK)
         return R7Z_ERROR_DATA;

      return R7Z_OK;
   }

   /* Plain LZMA. The streaming decoder takes a caller-sized
    * probability array, and 7z permits lc + lp up to 12, so the size
    * comes from the stream's own properties rather than a fixed
    * maximum. */
   {
      uint32_t d, lc, lp;

      if (c->prop_size < RLZMA_PROPS_SIZE)
         return R7Z_ERROR_DATA;

      d = (uint32_t)a->props[c->prop_offset];
      if (d >= 9 * 5 * 5)
         return R7Z_ERROR_DATA;
      lc = d % 9;
      d /= 9;
      lp = d % 5;
      if (lc + lp > RLZMA_STREAM_LCLP_MAX)
         return R7Z_ERROR_DATA;

      a->pend_dec   = malloc(sizeof(rlzma_stream_t));
      a->pend_probs = (uint16_t *)malloc(
            (size_t)RLZMA_STREAM_NUM_PROBS(lc, lp) * sizeof(uint16_t));
      if (!a->pend_dec || !a->pend_probs)
         return R7Z_ERROR_MEM;

      if (rlzma_stream_init((rlzma_stream_t *)a->pend_dec,
               a->props + c->prop_offset, a->pend_probs,
               a->pend_out, a->pend_out_len ? a->pend_out_len : 1)
            != RLZMA_OK)
         return R7Z_ERROR_DATA;

      rlzma_stream_reset((rlzma_stream_t *)a->pend_dec);
   }

   return R7Z_OK;
}

/* Finish the current stage, hand its output to the next, and advance.
 * Returns 1 when the whole chain is done. */
static int decode_stage_finish(r7z_archive_t *a, const folder_t *f)
{
   free(a->pend_dec);
   free(a->pend_probs);
   a->pend_dec   = NULL;
   a->pend_probs = NULL;

   free(a->pend_in);
   a->pend_in     = a->pend_out;
   a->pend_in_len = a->pend_out_len;
   a->pend_out    = NULL;
   a->pend_out_len = 0;

   a->pend_coder++;
   return (a->pend_coder >= f->num_coders) ? 1 : 0;
}



/* Find the coder feeding an input port, when that port is fed by a
 * bond from a coder whose own input is a packed stream. That is the
 * shape every BCJ2 folder 7-Zip produces has: BCJ2 plus one LZMA per
 * port, no chains.
 *
 * Returns 1 and fills the outputs when the port matches that shape, 0
 * when it does not and the caller should fall back to resolving the
 * port whole.
 */
static int bcj2_port_coder(r7z_archive_t *a, uint32_t fi, uint32_t port,
      uint32_t *coder_idx, uint32_t *out_index)
{
   folder_t *f = &a->folders[fi];
   uint32_t  b;

   for (b = 0; b < f->num_bonds; b++)
   {
      uint32_t out_port;
      uint32_t ci;
      uint32_t seen = 0;

      if (a->bonds[f->bond_first + b * 2] != port)
         continue;

      out_port = a->bonds[f->bond_first + b * 2 + 1];

      for (ci = 0; ci < f->num_coders; ci++)
      {
         const coder_t *c = &a->coders[f->coder_first + ci];

         if (out_port < seen + c->num_out)
         {
            uint32_t in_base = 0;
            uint32_t z;
            uint32_t k;

            /* Only the single-input, sliceable case. */
            if (c->num_in != 1 || !coder_is_resumable(c))
               return 0;

            for (z = 0; z < ci; z++)
               in_base += a->coders[f->coder_first + z].num_in;

            /* And only when that input is a packed stream directly,
             * so there is no chain behind it to resume. */
            for (k = 0; k < f->num_pack; k++)
               if (a->folder_pack[f->pack_first + k] == in_base)
               {
                  *coder_idx = ci;
                  *out_index = out_port;
                  return 1;
               }
            return 0;
         }
         seen += c->num_out;
      }
      return 0;
   }
   return 0;
}

/* One call's worth of a BCJ2 folder.
 *
 * The converter needs all four inputs complete before it can emit a
 * byte, so this cannot interleave them the way a linear chain is
 * sliced. What it can do is resolve one input per call: the cost of a
 * call is then one input's decode rather than all four plus the
 * conversion. On a 4.9 MiB folder that is the difference between one
 * 85 ms block and four bounded ones.
 *
 * Sets *done when the folder is complete and cached.
 */
static int decode_folder_bcj2_slice(r7z_archive_t *a, uint32_t fi,
      uint32_t bcj2_at, int *done)
{
   folder_t *f = &a->folders[fi];
   uint32_t  i;
   int       res;

   *done = 0;

   if (!a->pend_b2active)
   {
      /* Starting: work out the port base and the coder-output base
       * once, then resolve a port per call from here. */
      if (a->coders[f->coder_first + bcj2_at].num_in != 4)
         return R7Z_ERROR_UNSUPPORTED;
      if (f->unpack_size > (uint64_t)((size_t)-1))
         return R7Z_ERROR_DATA;

      decode_pending_reset(a);

      a->pend_folder   = fi;
      a->pend_b2at     = bcj2_at;
      a->pend_b2port   = 0;
      a->pend_b2active = 1;

      a->pend_b2in = 0;
      for (i = 0; i < bcj2_at; i++)
         a->pend_b2in += a->coders[f->coder_first + i].num_in;

      a->pend_base = 0;
      for (i = 0; i < fi; i++)
      {
         uint32_t k;
         for (k = 0; k < a->folders[i].num_coders; k++)
            a->pend_base +=
               a->coders[a->folders[i].coder_first + k].num_out;
      }
      return R7Z_OK;   /* setup only; decode starts next call */
   }

   if (a->pend_b2port < 4)
   {
      uint32_t ci, oi;

      i = a->pend_b2port;

      /* When the port is fed by a single sliceable coder reading a
       * packed stream directly - which is what every BCJ2 folder
       * 7-Zip writes looks like - slice that coder rather than
       * decoding it whole. Otherwise the call would be bounded by the
       * largest input, and BCJ2's main stream is nearly the whole
       * folder. */
      if (bcj2_port_coder(a, fi, a->pend_b2in + i, &ci, &oi))
      {
         const coder_t *pc = &a->coders[f->coder_first + ci];

         if (!a->pend_out)
         {
            /* Begin this port's coder: point the stage machinery at
             * it and let the existing slicer do the work. */
            a->pend_coder = ci;
            a->pend_fed   = 0;

            {
               uint32_t pk_first;
               uint64_t pk_off;
               uint32_t k, in_base = 0;
               size_t   n = 0;

               if ((res = folder_input_offsets(a, fi, &pk_first, &pk_off))
                     != R7Z_OK)
               {
                  decode_pending_reset(a);
                  return res;
               }
               for (k = 0; k < ci; k++)
                  in_base += a->coders[f->coder_first + k].num_in;

               for (k = 0; k < f->num_pack; k++)
                  if (a->folder_pack[f->pack_first + k] == in_base)
                  {
                     uint64_t off = pk_off;
                     uint32_t z;

                     for (z = 0; z < k; z++)
                        off += a->pack_sizes[pk_first + z];
                     if (pk_first + k >= a->num_pack
                           || a->pack_sizes[pk_first + k]
                              > (uint64_t)((size_t)-1))
                     {
                        decode_pending_reset(a);
                        return R7Z_ERROR_DATA;
                     }
                     n = (size_t)a->pack_sizes[pk_first + k];
                     if (off > (uint64_t)a->len - 32
                           || (uint64_t)n > (uint64_t)a->len - 32 - off)
                     {
                        decode_pending_reset(a);
                        return R7Z_ERROR_DATA;
                     }
                     free(a->pend_in);
                     if (!(a->pend_in = (uint8_t *)malloc(n ? n : 1)))
                     {
                        decode_pending_reset(a);
                        return R7Z_ERROR_MEM;
                     }
                     memcpy(a->pend_in, a->data + 32 + off, n);
                     a->pend_in_len = n;
                     break;
                  }
            }

            if ((res = decode_stage_begin(a, f)) != R7Z_OK)
            {
               decode_pending_reset(a);
               return res;
            }
            return R7Z_OK;
         }

         {
            int stage_done = 0;

            res = (pc->method == METHOD_LZMA2)
               ? decode_slice_lzma2(a, pc, &stage_done)
               : decode_slice_lzma(a, &stage_done);

            if (res != R7Z_OK)
            {
               decode_pending_reset(a);
               return res;
            }
            if (!stage_done)
               return R7Z_OK;

            /* Port complete: hand its buffer over and move on. */
            free(a->pend_dec);
            free(a->pend_probs);
            a->pend_dec   = NULL;
            a->pend_probs = NULL;

            a->pend_b2buf[i] = a->pend_out;
            a->pend_b2len[i] = a->pend_out_len;
            a->pend_out      = NULL;
            a->pend_out_len  = 0;
            a->pend_b2port++;
            return R7Z_OK;
         }
      }

      /* Not the simple shape: resolve it whole. Correct, but not
       * exercised by any test - no 7-Zip version writes a BCJ2 folder
       * whose port is fed by a chain. See the coverage note in
       * r7z_bcj2.h before changing this. */
      res = resolve_input(a, fi, a->pend_b2in + i, a->pend_base,
            &a->pend_b2buf[i], &a->pend_b2len[i]);
      if (res != R7Z_OK)
      {
         decode_pending_reset(a);
         return res;
      }
      a->pend_b2port++;
      return R7Z_OK;
   }

   /* All four inputs are in. The conversion is a single linear pass
    * over the whole folder, so slice it as well rather than paying it
    * all in the call that happens to finish the last input. */
   if (!a->pend_b2st)
   {
      a->pend_b2dstlen = (size_t)f->unpack_size;
      a->pend_b2dst    = (uint8_t *)malloc(
            a->pend_b2dstlen ? a->pend_b2dstlen : 1);
      a->pend_b2st     = malloc(sizeof(r7z_bcj2_state_t));
      if (!a->pend_b2dst || !a->pend_b2st)
      {
         decode_pending_reset(a);
         return R7Z_ERROR_MEM;
      }
      r7z_bcj2_state_init((r7z_bcj2_state_t *)a->pend_b2st);
      a->pend_b2done = 0;
      return R7Z_OK;
   }

   {
      size_t limit = a->pend_b2done + R7Z_SLICE_BYTES;

      if (limit > a->pend_b2dstlen)
         limit = a->pend_b2dstlen;

      res = r7z_bcj2_decode_part((r7z_bcj2_state_t *)a->pend_b2st,
            a->pend_b2dst, a->pend_b2dstlen, limit,
            a->pend_b2buf[0], a->pend_b2len[0],
            a->pend_b2buf[1], a->pend_b2len[1],
            a->pend_b2buf[2], a->pend_b2len[2],
            a->pend_b2buf[3], a->pend_b2len[3]);

      if (res == RBCJ2_PENDING)
      {
         a->pend_b2done = ((r7z_bcj2_state_t *)a->pend_b2st)->dst_pos;
         return R7Z_OK;
      }
      if (res != RBCJ2_OK)
      {
         decode_pending_reset(a);
         return R7Z_ERROR_DATA;
      }
   }

   if (f->has_crc
         && crc_calc(a->pend_b2dst, a->pend_b2dstlen) != f->crc)
   {
      decode_pending_reset(a);
      return R7Z_ERROR_CRC;
   }

   free(a->cached_data);
   a->cached_data   = a->pend_b2dst;
   a->cached_len    = a->pend_b2dstlen;
   a->cached_folder = fi;
   a->pend_b2dst    = NULL;

   decode_pending_reset(a);
   *done = 1;
   return R7Z_OK;
}

/* Do one slice of a folder decode.
 *
 * Returns R7Z_OK with *done set when the folder is complete, in which
 * case the decoded bytes are left in the archive's folder cache. Any
 * error clears the pending state.
 */
static int decode_folder_slice(r7z_archive_t *a, uint32_t fi, int *done)
{
   folder_t      *f;
   const coder_t *c;
   int            res;

   *done = 0;

   if (fi >= a->num_folders)
      return R7Z_ERROR_PARAM;
   f = &a->folders[fi];

   /* A BCJ2 folder already in flight continues in its own slicer. */
   if (a->pend_b2active && a->pend_folder == fi)
      return decode_folder_bcj2_slice(a, fi, a->pend_b2at, done);

   /* A request for a different folder abandons whatever was in flight;
    * only one decode is ever pending. */
   if (a->pend_folder != fi)
   {
      uint32_t k;

      decode_pending_reset(a);

      if (f->unpack_size > (uint64_t)((size_t)-1))
         return R7Z_ERROR_DATA;

      /* A BCJ2 folder has its own slicer: its four inputs cannot be
       * interleaved, but they can be resolved one per call. */
      for (k = 0; k < f->num_coders; k++)
         if (a->coders[f->coder_first + k].method == METHOD_BCJ2)
            return decode_folder_bcj2_slice(a, fi, k, done);

      a->pend_folder = fi;
      a->pend_coder  = 0;

      for (k = 0; k < fi; k++)
      {
         uint32_t m;
         for (m = 0; m < a->folders[k].num_coders; m++)
            a->pend_base += a->coders[a->folders[k].coder_first + m].num_out;
      }

      /* The chain's first input is the folder's packed bytes. */
      {
         uint32_t pk = 0;
         uint64_t off;
         size_t   n;

         for (k = 0; k < fi; k++)
            pk += a->folders[k].num_pack;
         if (pk >= a->num_pack)
         {
            decode_pending_reset(a);
            return R7Z_ERROR_DATA;
         }
         off = a->pack_base + f->pack_offset;
         if (a->pack_sizes[pk] > (uint64_t)((size_t)-1))
         {
            decode_pending_reset(a);
            return R7Z_ERROR_DATA;
         }
         n = (size_t)a->pack_sizes[pk];
         if (off > (uint64_t)a->len - 32
               || (uint64_t)n > (uint64_t)a->len - 32 - off)
         {
            decode_pending_reset(a);
            return R7Z_ERROR_DATA;
         }
         if (!(a->pend_in = (uint8_t *)malloc(n ? n : 1)))
         {
            decode_pending_reset(a);
            return R7Z_ERROR_MEM;
         }
         memcpy(a->pend_in, a->data + 32 + off, n);
         a->pend_in_len = n;
      }

      if ((res = decode_stage_begin(a, f)) != R7Z_OK)
      {
         decode_pending_reset(a);
         return res;
      }
   }

   c = &a->coders[f->coder_first + a->pend_coder];

   if (coder_is_resumable(c))
   {
      int stage_done = 0;

      res = (c->method == METHOD_LZMA2)
         ? decode_slice_lzma2(a, c, &stage_done)
         : decode_slice_lzma(a, &stage_done);

      if (res != R7Z_OK)
      {
         decode_pending_reset(a);
         return res;
      }
      if (!stage_done)
         return R7Z_OK;   /* more slices to come */
   }
   else
   {
      /* Not sliceable: run it whole. One to three milliseconds. */
      res = decode_coder(a, c, a->pend_in, a->pend_in_len,
            a->pend_out, a->pend_out_len);
      if (res != R7Z_OK)
      {
         decode_pending_reset(a);
         return res;
      }
   }

   if (!decode_stage_finish(a, f))
      {
         /* Next stage in the chain. */
         if ((res = decode_stage_begin(a, f)) != R7Z_OK)
         {
            decode_pending_reset(a);
            return res;
         }
         return R7Z_OK;
      }

   /* Chain complete. pend_in holds the folder output. */
   if (a->pend_in_len != (size_t)f->unpack_size)
   {
      decode_pending_reset(a);
      return R7Z_ERROR_DATA;
   }
   if (f->has_crc && crc_calc(a->pend_in, a->pend_in_len) != f->crc)
   {
      decode_pending_reset(a);
      return R7Z_ERROR_CRC;
   }

   free(a->cached_data);
   a->cached_data   = a->pend_in;
   a->cached_len    = a->pend_in_len;
   a->cached_folder = fi;

   a->pend_in     = NULL;
   a->pend_in_len = 0;
   decode_pending_reset(a);

   *done = 1;
   return R7Z_OK;
}


/* Copy one entry out of the folder cache, which the caller has already
 * ensured holds the right folder. Shared by both extract entry
 * points. */
static int extract_from_cache(r7z_archive_t *a, const r7z_entry_t *e,
      uint8_t **out, size_t *out_len)
{
   uint8_t *buf;

   {
      uint64_t fsize = a->folders[e->folder].unpack_size;

      if (e->offset_in_folder > fsize
            || e->size > fsize - e->offset_in_folder)
         return R7Z_ERROR_DATA;
   }

   if (e->size > (uint64_t)((size_t)-1)
         || e->offset_in_folder > (uint64_t)((size_t)-1))
      return R7Z_ERROR_DATA;

   if (!(buf = (uint8_t *)malloc((size_t)e->size)))
      return R7Z_ERROR_MEM;

   memcpy(buf, a->cached_data + (size_t)e->offset_in_folder,
         (size_t)e->size);

   if (e->has_crc && crc_calc(buf, (size_t)e->size) != e->crc)
   {
      free(buf);
      return R7Z_ERROR_CRC;
   }

   *out     = buf;
   *out_len = (size_t)e->size;
   return R7Z_OK;
}

int r7z_archive_extract_slice(r7z_archive_t *a, uint32_t index,
      uint8_t **out, size_t *out_len)
{
   const r7z_entry_t *e;
   int                done = 0;
   int                res;

   if (!a || !out || !out_len)
      return R7Z_ERROR_PARAM;
   if (index >= a->num_entries)
      return R7Z_ERROR_PARAM;

   e = &a->entries[index];

   if (e->is_dir)
      return R7Z_ERROR_PARAM;

   if (e->size == 0)
   {
      if (!(*out = (uint8_t *)malloc(1)))
         return R7Z_ERROR_MEM;
      *out_len = 0;
      return R7Z_OK;
   }

   if (e->folder >= a->num_folders)
      return R7Z_ERROR_DATA;

   /* Already decoded, from this call sequence or an earlier entry in
    * the same folder. */
   if (a->cached_folder == e->folder && a->cached_data)
      return extract_from_cache(a, e, out, out_len);

   res = decode_folder_slice(a, e->folder, &done);

   if (res == R7Z_ERROR_UNSUPPORTED)
   {
      /* A shape the slicer will not take, BCJ2 today. Decode it whole:
       * correct, and the caller sees one long call rather than many
       * short ones. */
      uint8_t *folder_data;

      if ((res = decode_folder(a, e->folder, &folder_data)) != R7Z_OK)
         return res;

      free(a->cached_data);
      a->cached_data   = folder_data;
      a->cached_len    = (size_t)a->folders[e->folder].unpack_size;
      a->cached_folder = e->folder;

      return extract_from_cache(a, e, out, out_len);
    }

   if (res != R7Z_OK)
      return res;
   if (!done)
      return R7Z_PENDING;

   return extract_from_cache(a, e, out, out_len);
}

int r7z_archive_extract(r7z_archive_t *a, uint32_t index,
      uint8_t **out, size_t *out_len)
{
   const r7z_entry_t *e;
   uint8_t           *folder_data;
   uint8_t           *buf;
   int                res;

   if (!a || !out || !out_len)
      return R7Z_ERROR_PARAM;
   if (index >= a->num_entries)
      return R7Z_ERROR_PARAM;

   e = &a->entries[index];
   *out = NULL;
   *out_len = 0;

   if (e->is_dir)
      return R7Z_ERROR_PARAM;

   if (e->size == 0)
   {
      buf = (uint8_t *)malloc(1);
      if (!buf)
         return R7Z_ERROR_MEM;
      *out = buf;
      *out_len = 0;
      return R7Z_OK;
   }

   if (e->folder >= a->num_folders)
      return R7Z_ERROR_DATA;

   /* Decode the folder unless the last call already left it here. A
    * solid folder holds many members, and without this each one costs
    * a full decode of everything around it. */
   if (a->cached_folder == e->folder && a->cached_data)
      folder_data = a->cached_data;
   else
   {
      res = decode_folder(a, e->folder, &folder_data);
      if (res != R7Z_OK)
         return res;

      free(a->cached_data);
      a->cached_data   = folder_data;
      a->cached_len    = (size_t)a->folders[e->folder].unpack_size;
      a->cached_folder = e->folder;
   }

   {
      uint64_t fsize = a->folders[e->folder].unpack_size;

      if (e->offset_in_folder > fsize
            || e->size > fsize - e->offset_in_folder)
         return R7Z_ERROR_DATA;
   }

   /* Sizes are 64-bit in the format but the buffer below is indexed by
    * size_t. decode_folder() already refuses a folder it cannot
    * address, which makes this unreachable for a member inside one,
    * but the narrowing is here and should not depend on a check in
    * another function to stay safe. */
   if (e->size > (uint64_t)((size_t)-1)
         || e->offset_in_folder > (uint64_t)((size_t)-1))
      return R7Z_ERROR_DATA;

   buf = (uint8_t *)malloc((size_t)e->size);
   if (!buf)
      return R7Z_ERROR_MEM;
   memcpy(buf, folder_data + (size_t)e->offset_in_folder, (size_t)e->size);

   if (e->has_crc && crc_calc(buf, (size_t)e->size) != e->crc)
   {
      free(buf);
      return R7Z_ERROR_CRC;
   }

   *out     = buf;
   *out_len = (size_t)e->size;
   return R7Z_OK;
}
