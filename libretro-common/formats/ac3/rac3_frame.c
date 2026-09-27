/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rac3_frame.c).
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* The frame layer of AC-3 and E-AC-3, from ATSC A/52: the sync
 * information, the bit stream information as far as the frame's size
 * and content, and the CRC. Written from the standard's text and
 * tables. */

#include <string.h>

#include <formats/rac3.h>

/* ---- the standard's tables ---------------------------------------- */

/* Table 5.18: frame sizes in 16-bit words by frmsizecod for the three
 * sample rates, and the nominal bit rate of each pair of codes. The
 * odd code at 44.1 kHz is one word longer than the even one. */
static const uint16_t rac3_bitrate_kbps[19] = {
   32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 448, 512, 576, 640
};

/* At 44.1 kHz a frame is not a whole number of words: 1536 samples at
 * the bit rate is kbps * 320 / 147 words, and the standard gives each
 * rate two codes, the even for the size rounded down and the odd for
 * one more, which an encoder alternates to hold the rate exactly. */
static unsigned rac3_frame_words(unsigned fscod, unsigned frmsizecod)
{
   unsigned kbps = rac3_bitrate_kbps[frmsizecod >> 1];
   switch (fscod)
   {
      case 0:  return kbps * 2;    /* 48 kHz: 1536 samples at kbps is exactly 2 words per kbps */
      case 1:  return (kbps * 320u) / 147u + (frmsizecod & 1);
      case 2:  return kbps * 3;    /* 32 kHz */
      default: return 0;
   }
}

/* Channels per acmod, without the LFE. */
static const uint8_t rac3_acmod_channels[8] = { 2, 1, 2, 3, 3, 4, 4, 5 };

/* The frontend's mask per acmod, without the LFE: A/52's Ls/Rs as
 * the side pair, the lone S as the back centre. */
static const uint32_t rac3_acmod_layout[8] = {
   0x001u | 0x002u,                                   /* 1+1: two mono, carried as L and R */
   0x004u,                                            /* 1/0: C */
   0x001u | 0x002u,                                   /* 2/0: L R */
   0x001u | 0x002u | 0x004u,                          /* 3/0: L C R */
   0x001u | 0x002u | 0x100u,                          /* 2/1: L R S */
   0x001u | 0x002u | 0x004u | 0x100u,                 /* 3/1: L C R S */
   0x001u | 0x002u | 0x200u | 0x400u,                 /* 2/2: L R Ls Rs */
   0x001u | 0x002u | 0x004u | 0x200u | 0x400u         /* 3/2: L C R Ls Rs */
};

/* ---- a bit reader for the header ---------------------------------- */

typedef struct
{
   const uint8_t *p;
   size_t         len;
   size_t         bit;
   bool           over;
} rac3_bits_t;

static unsigned rac3_bits_get(rac3_bits_t *b, unsigned n)
{
   unsigned v = 0;
   while (n--)
   {
      unsigned byte = (unsigned)(b->bit >> 3);
      if (byte >= b->len)
      {
         b->over = true;
         return 0;
      }
      v = (v << 1) | ((b->p[byte] >> (7 - (b->bit & 7))) & 1u);
      b->bit++;
   }
   return v;
}

static void rac3_bits_skip(rac3_bits_t *b, unsigned n)
{
   b->bit += n;
   if ((b->bit + 7) >> 3 > b->len)
      b->over = true;
}

/* ---- AC-3 (bsid <= 8) --------------------------------------------- */

static enum rac3_status rac3_parse_ac3(rac3_bits_t *b, rac3_frame_info_t *info)
{
   unsigned fscod, frmsizecod, bsid, bsmod, acmod, lfeon, words;

   /* syncinfo: crc1 (16), fscod (2), frmsizecod (6) */
   rac3_bits_skip(b, 16);
   fscod      = rac3_bits_get(b, 2);
   frmsizecod = rac3_bits_get(b, 6);
   /* bsi: bsid (5), bsmod (3), acmod (3), ... */
   bsid       = rac3_bits_get(b, 5);
   bsmod      = rac3_bits_get(b, 3);
   acmod      = rac3_bits_get(b, 3);
   if (b->over)
      return RAC3_NEED_MORE;
   if (fscod == 3 || frmsizecod > 37)
      return RAC3_BAD;
   /* The mixing levels come between acmod and lfeon: cmixlev when
    * there is a centre and more than one front, surmixlev when there
    * is a surround, dsurmod for 2/0. */
   if ((acmod & 1) && acmod != 1)
      rac3_bits_skip(b, 2);             /* cmixlev */
   if (acmod & 4)
      rac3_bits_skip(b, 2);             /* surmixlev */
   if (acmod == 2)
      rac3_bits_skip(b, 2);             /* dsurmod */
   lfeon = rac3_bits_get(b, 1);
   info->dialnorm = rac3_bits_get(b, 5);
   if (b->over)
      return RAC3_NEED_MORE;

   words                = rac3_frame_words(fscod, frmsizecod);
   info->kind           = RAC3_KIND_AC3;
   info->bsid           = bsid;
   info->frame_bytes    = words * 2;
   info->sample_rate    = fscod == 0 ? 48000 : fscod == 1 ? 44100 : 32000;
   info->samples        = 1536;
   info->blocks         = 6;
   info->bitrate        = rac3_bitrate_kbps[frmsizecod >> 1] * 1000;
   info->acmod          = acmod;
   info->bsmod          = bsmod;
   info->lfe            = lfeon != 0;
   info->channels       = rac3_acmod_channels[acmod] + (lfeon ? 1 : 0);
   info->layout         = rac3_acmod_layout[acmod] | (lfeon ? 0x008u : 0);
   info->stream_type    = RAC3_STREAM_INDEPENDENT;
   info->substream_id   = 0;
   if (!info->dialnorm)
      info->dialnorm    = 31;
   return RAC3_OK;
}

/* ---- E-AC-3 (bsid 11..16) ----------------------------------------- */

static enum rac3_status rac3_parse_eac3(rac3_bits_t *b, rac3_frame_info_t *info)
{
   unsigned strmtyp, substreamid, frmsiz, fscod, fscod2, numblkscod, acmod, lfeon, bsid;
   static const uint8_t blocks_of[4] = { 1, 2, 3, 6 };

   /* bsi: strmtyp (2), substreamid (3), frmsiz (11), fscod (2),
    * fscod2 or numblkscod (2), acmod (3), lfeon (1), bsid (5),
    * dialnorm (5) ... */
   strmtyp     = rac3_bits_get(b, 2);
   substreamid = rac3_bits_get(b, 3);
   frmsiz      = rac3_bits_get(b, 11);
   fscod       = rac3_bits_get(b, 2);
   if (fscod == 3)
   {
      /* The reduced rates: fscod2 gives 24, 22.05 or 16 kHz, and the
       * frame is always six blocks. */
      fscod2     = rac3_bits_get(b, 2);
      numblkscod = 3;
      if (fscod2 == 3)
         return RAC3_BAD;
      info->sample_rate = fscod2 == 0 ? 24000 : fscod2 == 1 ? 22050 : 16000;
   }
   else
   {
      numblkscod = rac3_bits_get(b, 2);
      info->sample_rate = fscod == 0 ? 48000 : fscod == 1 ? 44100 : 32000;
   }
   acmod = rac3_bits_get(b, 3);
   lfeon = rac3_bits_get(b, 1);
   bsid  = rac3_bits_get(b, 5);
   info->dialnorm = rac3_bits_get(b, 5);
   if (b->over)
      return RAC3_NEED_MORE;
   if (strmtyp == RAC3_STREAM_RESERVED)
      return RAC3_BAD;

   info->kind           = RAC3_KIND_EAC3;
   info->bsid           = bsid;
   info->frame_bytes    = (frmsiz + 1) * 2;
   info->blocks         = blocks_of[numblkscod];
   info->samples        = info->blocks * 256;
   info->bitrate        = 0;
   info->acmod          = acmod;
   info->bsmod          = 0;             /* E-AC-3 carries it later in bsi, when at all */
   info->lfe            = lfeon != 0;
   info->channels       = rac3_acmod_channels[acmod] + (lfeon ? 1 : 0);
   info->layout         = rac3_acmod_layout[acmod] | (lfeon ? 0x008u : 0);
   info->stream_type    = (enum rac3_stream_type)strmtyp;
   info->substream_id   = substreamid;
   if (!info->dialnorm)
      info->dialnorm    = 31;
   return RAC3_OK;
}

enum rac3_status rac3_parse_frame_info(const uint8_t *src, size_t len, rac3_frame_info_t *info)
{
   rac3_bits_t b;
   unsigned    bsid;

   if (!src || !info)
      return RAC3_BAD;
   memset(info, 0, sizeof(*info));
   if (len < 2)
      return RAC3_NEED_MORE;
   if (src[0] != 0x0B || src[1] != 0x77)
      return RAC3_NO_SYNC;
   if (len < 6)
      return RAC3_NEED_MORE;

   /* bsid sits at the same place in both syntaxes: the 5 bits at
    * bit offset 40 (byte 5, top five bits). */
   bsid = (src[5] >> 3) & 0x1F;

   b.p    = src + 2;
   b.len  = len - 2;
   b.bit  = 0;
   b.over = false;

   if (bsid <= 8)
      return rac3_parse_ac3(&b, info);
   if (bsid >= 11 && bsid <= 16)
      return rac3_parse_eac3(&b, info);
   return RAC3_BAD;
}

size_t rac3_find_sync(const uint8_t *src, size_t len, size_t from)
{
   size_t i;
   for (i = from; i + 6 <= len; i++)
   {
      rac3_frame_info_t a, n;
      if (src[i] != 0x0B || src[i + 1] != 0x77)
         continue;
      if (rac3_parse_frame_info(src + i, len - i, &a) != RAC3_OK || !a.frame_bytes)
         continue;
      /* The next frame must be where this one's size says, or the
       * stream must end there. */
      if (i + a.frame_bytes >= len)
         return i;
      if (rac3_parse_frame_info(src + i + a.frame_bytes, len - i - a.frame_bytes, &n) == RAC3_OK)
         return i;
   }
   return len;
}

uint16_t rac3_crc16(const uint8_t *src, size_t len)
{
   /* CRC-16, polynomial 0x8005 (x^16 + x^15 + x^2 + 1), no
    * reflection, initial 0, as A/52 section 7.10 defines it. */
   uint16_t crc = 0;
   size_t i;
   for (i = 0; i < len; i++)
   {
      unsigned k;
      crc ^= (uint16_t)src[i] << 8;
      for (k = 0; k < 8; k++)
         crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x8005u) : (uint16_t)(crc << 1);
   }
   return crc;
}

bool rac3_frame_crc_ok(const uint8_t *src, size_t frame_bytes)
{
   rac3_frame_info_t info;
   if (rac3_parse_frame_info(src, frame_bytes, &info) != RAC3_OK
         || info.frame_bytes != frame_bytes)
      return false;
   if (info.kind == RAC3_KIND_AC3)
   {
      /* crc1 covers words 1 .. 5/8 of the frame (the first five
       * eighths, the sync word and crc1 itself excluded from the
       * data but the check is that the CRC over bytes 2..frame*5/8
       * inclusive of crc1's field is zero); crc2 the rest, with the
       * frame's last two bytes as the check. Both computed such that
       * the running CRC over data plus its check word is zero. */
      /* The standard's frmsize_5_8: (words >> 1) + (words >> 3), each
       * truncated, in words. */
      size_t words        = frame_bytes >> 1;
      size_t five_eighths = ((words >> 1) + (words >> 3)) * 2;
      if (rac3_crc16(src + 2, five_eighths - 2) != 0)
         return false;
      return rac3_crc16(src + five_eighths, frame_bytes - five_eighths) == 0;
   }
   /* E-AC-3: one CRC over the frame from byte 2 through the check
    * word at the end. */
   return rac3_crc16(src + 2, frame_bytes - 2) == 0;
}
