/* Copyright  (C) 2010-2025 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rdts.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <string.h>

#include <formats/rdts.h>

#define L_FL   0x001u
#define L_FR   0x002u
#define L_FC   0x004u
#define L_LFE  0x008u
#define L_BL   0x010u
#define L_BR   0x020u
#define L_FLC  0x040u
#define L_FRC  0x080u
#define L_BC   0x100u
#define L_SL   0x200u
#define L_SR   0x400u

#define RDTS_SYNC_CORE_16BE  0x7FFE8001u
#define RDTS_SYNC_CORE_16LE  0xFE7F0180u
#define RDTS_SYNC_CORE_14BE  0x1FFFE800u
#define RDTS_SYNC_CORE_14LE  0xFF1F00E8u
#define RDTS_SYNC_SUBSTREAM  0x64582025u

/* A reader over the frame's bits, which for the 14-bit packings are
 * not the bytes' bits: those carry fourteen bits of stream in each
 * 16-bit word, the top two of every word discarded. Reading through
 * this means the header fields below are written once, in the
 * standard's own bit order, whatever shape the frame arrived in. */
typedef struct
{
   const uint8_t    *p;
   size_t            len;       /* bytes */
   size_t            bit;       /* bits consumed of the stream */
   enum rdts_packing packing;
   bool              over;
} rdts_br_t;

static void rdts_br_init(rdts_br_t *b, const uint8_t *p, size_t len,
      enum rdts_packing packing)
{
   b->p       = p;
   b->len     = len;
   b->bit     = 0;
   b->packing = packing;
   b->over    = false;
}

/* One bit of the stream, wherever the packing put it. */
static unsigned rdts_br_bit(rdts_br_t *b)
{
   size_t   word, in_word, byte;
   unsigned v;

   switch (b->packing)
   {
      case RDTS_PACK_16BE:
      case RDTS_PACK_16LE:
         word    = b->bit >> 4;
         in_word = b->bit & 15u;
         byte    = word * 2 + (in_word < 8 ? 0 : 1);
         if (b->packing == RDTS_PACK_16LE)
            byte = word * 2 + (in_word < 8 ? 1 : 0);
         if (byte >= b->len) { b->over = true; return 0; }
         v = (b->p[byte] >> (7 - (in_word & 7u))) & 1u;
         break;
      default:
      {
         /* fourteen bits a word, the word's top two dropped */
         unsigned bits_in;
         word    = b->bit / 14;
         in_word = b->bit % 14;
         bits_in = (unsigned)in_word + 2;      /* skip the two spare bits */
         byte    = word * 2 + (bits_in < 8 ? 0 : 1);
         if (b->packing == RDTS_PACK_14LE)
            byte = word * 2 + (bits_in < 8 ? 1 : 0);
         if (byte >= b->len) { b->over = true; return 0; }
         v = (b->p[byte] >> (7 - (bits_in & 7u))) & 1u;
         break;
      }
   }
   b->bit++;
   return v;
}

static uint32_t rdts_br_get(rdts_br_t *b, unsigned n)
{
   uint32_t v = 0;
   while (n--)
      v = (v << 1) | rdts_br_bit(b);
   return v;
}

static void rdts_br_skip(rdts_br_t *b, unsigned n)
{
   while (n--)
      rdts_br_bit(b);
}

/* ---- the tables the header indexes -------------------------------- */

static const unsigned rdts_sample_rates[16] =
{
   0, 8000, 16000, 32000, 0, 0, 11025, 22050,
   44100, 0, 0, 12000, 24000, 48000, 0, 0
};

/* Nominal bit rates, bits per second. 29 is open, 30 variable, 31
 * lossless; all three are reported as zero, since none of them is a
 * number the frame carries. */
static const unsigned rdts_bit_rates[32] =
{
    32000,   56000,   64000,   96000,  112000,  128000,  192000,  224000,
   256000,  320000,  384000,  448000,  512000,  576000,  640000,  768000,
   896000, 1024000, 1152000, 1280000, 1344000, 1408000, 1411200, 1472000,
  1536000, 1920000, 2048000, 3072000, 3840000,       0,       0,       0
};

/* The channel arrangements (AMODE), and the mask each produces. The
 * standard's own channel order within the frame differs from the
 * mask's ascending-bit order; what a decoder writes out is the mask
 * order, which is what this reports. */
static uint32_t rdts_amode_layout(unsigned amode, unsigned *channels)
{
   uint32_t l;
   unsigned n;
   switch (amode)
   {
      case 0:  l = L_FC;                                  n = 1; break; /* mono */
      case 1:  l = L_FL | L_FR;                           n = 2; break; /* dual mono */
      case 2:  l = L_FL | L_FR;                           n = 2; break; /* L R */
      case 3:  l = L_FL | L_FR;                           n = 2; break; /* (L+R) (L-R) */
      case 4:  l = L_FL | L_FR;                           n = 2; break; /* LT RT */
      case 5:  l = L_FL | L_FR | L_FC;                    n = 3; break; /* C L R */
      case 6:  l = L_FL | L_FR | L_BC;                    n = 3; break; /* L R S */
      case 7:  l = L_FL | L_FR | L_FC | L_BC;             n = 4; break; /* C L R S */
      case 8:  l = L_FL | L_FR | L_SL | L_SR;             n = 4; break; /* L R SL SR */
      case 9:  l = L_FL | L_FR | L_FC | L_SL | L_SR;      n = 5; break; /* C L R SL SR */
      case 10: l = L_FL | L_FR | L_FLC | L_FRC | L_SL | L_SR;
               n = 6; break;                                            /* CL CR L R SL SR */
      case 11: l = L_FL | L_FR | L_BL | L_BR | L_SL | L_SR;
               n = 6; break;                                            /* L R LR RR SL SR */
      case 12: l = L_FL | L_FR | L_FC | L_BC | L_SL | L_SR;
               n = 6; break;                                            /* C L R LS RS OV */
      case 13: l = L_FL | L_FR | L_FC | L_FLC | L_FRC | L_SL | L_SR;
               n = 7; break;
      case 14: l = L_FL | L_FR | L_FLC | L_FRC | L_SL | L_SR | L_BL | L_BR;
               n = 8; break;
      case 15: l = L_FL | L_FR | L_FC | L_FLC | L_FRC | L_SL | L_SR | L_BC;
               n = 8; break;
      default: l = 0; n = 0; break;
   }
   if (channels)
      *channels = n;
   return l;
}

/* ---- the header --------------------------------------------------- */

static bool rdts_sync_at(const uint8_t *p, size_t len, enum rdts_packing *packing,
      bool *substream)
{
   uint32_t w;
   if (len < 4)
      return false;
   w = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
     | ((uint32_t)p[2] << 8)  | p[3];
   *substream = false;
   switch (w)
   {
      case RDTS_SYNC_CORE_16BE: *packing = RDTS_PACK_16BE; return true;
      case RDTS_SYNC_CORE_16LE: *packing = RDTS_PACK_16LE; return true;
      case RDTS_SYNC_CORE_14BE: *packing = RDTS_PACK_14BE; return true;
      case RDTS_SYNC_CORE_14LE: *packing = RDTS_PACK_14LE; return true;
      case RDTS_SYNC_SUBSTREAM:
         *packing   = RDTS_PACK_16BE;
         *substream = true;
         return true;
      default: break;
   }
   return false;
}

/* The DTS-HD extension substream header, which says only how long it
 * is and what it carries; the asset descriptors inside it are where
 * the extension kinds are named. */
static enum rdts_status rdts_parse_substream(const uint8_t *src, size_t len,
      rdts_frame_info_t *info)
{
   rdts_br_t b;
   unsigned  header_bits, blown_up;

   if (len < 14)
      return RDTS_NEED_MORE;

   rdts_br_init(&b, src, len, RDTS_PACK_16BE);
   rdts_br_skip(&b, 32);            /* the sync word */
   rdts_br_skip(&b, 8);             /* user-defined bits */
   rdts_br_skip(&b, 2);             /* substream index */
   blown_up = rdts_br_get(&b, 1);   /* header and frame size widths */
   header_bits = blown_up ? 12 : 8;
   info->frame_bytes = 0;
   {
      unsigned hsize = rdts_br_get(&b, header_bits) + 1;
      unsigned fsize = rdts_br_get(&b, blown_up ? 20 : 16) + 1;
      if (b.over)
         return RDTS_NEED_MORE;
      (void)hsize;
      info->frame_bytes = fsize;
      info->core_bytes  = fsize;
   }
   if (!info->frame_bytes)
      return RDTS_BAD;

   info->kind         = RDTS_KIND_SUBSTREAM;
   info->packing      = RDTS_PACK_16BE;
   /* What the substream carries is named in its asset descriptors,
    * which this does not walk; a caller that needs the core's shape
    * reads the core frame that precedes it, which is where a stream
    * that must stay playable on a plain decoder keeps it. */
   info->extensions  |= RDTS_EXT_XLL;
   return RDTS_OK;
}

enum rdts_status rdts_parse_frame_info(const uint8_t *src, size_t len,
      rdts_frame_info_t *info)
{
   rdts_br_t         b;
   enum rdts_packing packing;
   bool              substream;
   unsigned          blocks, fsize, amode, sfreq, rate, ext_audio_id;
   unsigned          ext_audio, lff, channels = 0;

   if (!src || !info)
      return RDTS_BAD;
   if (len < 4)
      return RDTS_NEED_MORE;
   if (!rdts_sync_at(src, len, &packing, &substream))
      return RDTS_NO_SYNC;

   memset(info, 0, sizeof(*info));

   if (substream)
      return rdts_parse_substream(src, len, info);

   /* The core header is 96 bits at most; a 14-bit frame spreads those
    * over more bytes than a 16-bit one does. */
   if (len < 16)
      return RDTS_NEED_MORE;

   rdts_br_init(&b, src, len, packing);
   rdts_br_skip(&b, 32);               /* the sync word */
   rdts_br_skip(&b, 1);                /* frame type */
   rdts_br_skip(&b, 5);                /* deficit sample count */
   info->crc_present = rdts_br_get(&b, 1) != 0;
   blocks            = rdts_br_get(&b, 7) + 1;
   fsize             = rdts_br_get(&b, 14) + 1;
   amode             = rdts_br_get(&b, 6);
   sfreq             = rdts_br_get(&b, 4);
   rate              = rdts_br_get(&b, 5);
   rdts_br_skip(&b, 1);                /* reserved */
   rdts_br_skip(&b, 1);                /* embedded dynamic range */
   rdts_br_skip(&b, 1);                /* embedded time stamp */
   rdts_br_skip(&b, 1);                /* auxiliary data */
   rdts_br_skip(&b, 1);                /* HDCD */
   ext_audio_id      = rdts_br_get(&b, 3);
   ext_audio         = rdts_br_get(&b, 1);
   rdts_br_skip(&b, 1);                /* audio sync word insertion */
   lff               = rdts_br_get(&b, 2);

   if (b.over)
      return RDTS_NEED_MORE;

   /* Blocks below five do not decode to anything; a frame shorter
    * than its own header is not a frame. */
   if (blocks < 5 || fsize < 96)
      return RDTS_BAD;
   if (!rdts_sample_rates[sfreq])
      return RDTS_BAD;
   if (lff == 3)
      return RDTS_BAD;

   info->kind        = RDTS_KIND_CORE;
   info->packing     = packing;
   info->blocks      = blocks;
   info->samples     = blocks * 32;
   info->core_bytes  = fsize;
   /* A 14-bit stream carries fourteen bits per sixteen, so the bytes
    * it occupies are the core's scaled by 8/7, rounded up to the word
    * the packing counts in. */
   if (packing == RDTS_PACK_14BE || packing == RDTS_PACK_14LE)
      info->frame_bytes = (unsigned)(((uint64_t)fsize * 8 + 6) / 7);
   else
      info->frame_bytes = fsize;
   info->frame_bytes = (info->frame_bytes + 1u) & ~1u;

   info->sample_rate = rdts_sample_rates[sfreq];
   info->bitrate     = rdts_bit_rates[rate];
   info->amode       = amode;
   info->layout      = rdts_amode_layout(amode, &channels);
   if (!info->layout)
      return RDTS_BAD;
   info->lfe         = (lff != 0);
   if (info->lfe)
   {
      info->layout |= L_LFE;
      channels++;
   }
   info->channels    = channels;

   /* The rest of the frame header, then the primary audio coding
    * header behind it. Walking this costs a few hundred bits and buys
    * the one thing a caller here needs to know beyond the shape: what
    * a frame would take to decode. */
   {
      unsigned cpf = 0, vernum;
      /* CPF sat in the header above; it is read again here rather
       * than carried, since the bits before it are all fixed-width. */
      rdts_br_t h;
      rdts_br_init(&h, src, len, packing);
      rdts_br_skip(&h, 32 + 1 + 5);
      cpf = rdts_br_get(&h, 1);
      rdts_br_skip(&h, 7 + 14 + 6 + 4 + 5);   /* blocks, size, amode, sfreq, rate */
      rdts_br_skip(&h, 1 + 1 + 1 + 1 + 1);    /* reserved, dynf, timef, auxf, hdcd */
      rdts_br_skip(&h, 3 + 1 + 1 + 2);        /* ext id, ext, aspf, lff */
      info->predictor_history = rdts_br_get(&h, 1) != 0;
      if (cpf)
         rdts_br_skip(&h, 16);                /* HCRC */
      rdts_br_skip(&h, 1);                    /* FILTS */
      vernum = rdts_br_get(&h, 4);
      rdts_br_skip(&h, 2 + 3 + 1 + 1);        /* CHIST, PCMR, SUMF, SUMS */
      if (vernum < 7)
         rdts_br_skip(&h, 4);                 /* DIALNORM, where the version has it */
      else
         rdts_br_skip(&h, 4);                 /* unspecified, same width */

      /* Table 5-21, the primary audio coding header. */
      {
         unsigned nsubfs = rdts_br_get(&h, 4) + 1;
         unsigned npchs  = rdts_br_get(&h, 3) + 1;
         unsigned ch, subs_max = 0, vq_min = 32;
         unsigned nsubs[8], nvqsub[8];
         bool     joint = false;

         if (npchs <= 8 && !h.over)
         {
            for (ch = 0; ch < npchs; ch++)
            {
               nsubs[ch] = rdts_br_get(&h, 5) + 2;
               if (nsubs[ch] > subs_max)
                  subs_max = nsubs[ch];
            }
            for (ch = 0; ch < npchs; ch++)
            {
               nvqsub[ch] = rdts_br_get(&h, 5) + 1;
               if (nvqsub[ch] < vq_min)
                  vq_min = nvqsub[ch];
            }
            for (ch = 0; ch < npchs; ch++)
               if (rdts_br_get(&h, 3) != 0)    /* JOINX */
                  joint = true;

            if (!h.over)
            {
               info->coding_header_read = true;
               info->subframes          = nsubfs;
               info->prim_channels      = npchs;
               info->subbands           = subs_max;
               info->vq_start_subband   = vq_min;
               info->joint_intensity    = joint;
               /* Subbands from nVQSUB up to nSUBS-1 are vector
                * quantised, so the quantiser is in use only where the
                * start is below the subbands the channel codes -
                * equal means the region is empty. Those subbands come
                * from the codebook the standard leaves out. */
               for (ch = 0; ch < npchs; ch++)
                  if (nvqsub[ch] < nsubs[ch])
                     info->needs_vq_tables = true;
            }
         }
      }
   }

   /* The extension the core names, where it says one follows. */
   if (ext_audio)
   {
      switch (ext_audio_id)
      {
         case 0: info->extensions |= RDTS_EXT_XCH;  break;
         case 2: info->extensions |= RDTS_EXT_X96;  break;
         case 6: info->extensions |= RDTS_EXT_XXCH; break;
         default: break;
      }
   }
   return RDTS_OK;
}

bool rdts_burst_type(const rdts_frame_info_t *info,
      unsigned *iec61937_type, unsigned *pcm_frames)
{
   unsigned type, frames;
   if (!info || info->kind != RDTS_KIND_CORE)
      return false;
   switch (info->samples)
   {
      case 512:  type = 11; frames = 512;  break;   /* IEC61937_DTS_I */
      case 1024: type = 12; frames = 1024; break;   /* IEC61937_DTS_II */
      case 2048: type = 13; frames = 2048; break;   /* IEC61937_DTS_III */
      default:
         /* A core frame's sample count is 32 times its block count,
          * which runs from 5 to 127 - so counts other than the three
          * the standard gives a burst for do occur, and there is
          * nowhere to put them. */
         return false;
   }
   /* The burst has to hold the frame: a frame longer than the period
    * would overrun what the receiver expects, which is the one thing
    * the wrap cannot paper over. */
   if (info->core_bytes > frames * 4)
      return false;
   if (iec61937_type)
      *iec61937_type = type;
   if (pcm_frames)
      *pcm_frames = frames;
   return true;
}

bool rdts_decodable_from_spec(const rdts_frame_info_t *info)
{
   if (!info || info->kind != RDTS_KIND_CORE)
      return false;
   if (!info->coding_header_read)
      return false;
   /* The high-frequency codebook is the one the standard does not
    * carry, so a frame that needs it cannot be decoded from the
    * standard. The ADPCM coefficient codebook is omitted too, and a
    * frame whose predictors are on needs that; predictor_history says
    * the history crosses frames, which is not the same question, so
    * what is reported here is the one that can be answered from the
    * coding header alone. */
   return !info->needs_vq_tables;
}

size_t rdts_find_sync(const uint8_t *src, size_t len, size_t from)
{
   size_t at;
   if (!src || from >= len)
      return len;
   for (at = from; at + 4 <= len; at++)
   {
      rdts_frame_info_t first, second;
      if (rdts_parse_frame_info(src + at, len - at, &first) != RDTS_OK)
         continue;
      /* A sync word that happens to fall inside a frame is common;
       * what tells a real one apart is that the frame it claims ends
       * where another begins. A frame at the end of the buffer has
       * nothing to agree with and is taken on its own. */
      if (at + first.frame_bytes + 4 > len)
         return at;
      if (rdts_parse_frame_info(src + at + first.frame_bytes,
               len - at - first.frame_bytes, &second) == RDTS_OK)
         return at;
   }
   return len;
}

size_t rdts_to_core(const rdts_frame_info_t *info,
      const uint8_t *src, size_t src_len, uint8_t *out, size_t out_len)
{
   rdts_br_t b;
   size_t    i, bits;

   if (!info || !src || !out)
      return 0;
   if (out_len < info->core_bytes)
      return 0;

   if (info->packing == RDTS_PACK_16BE)
   {
      size_t n = info->core_bytes < src_len ? info->core_bytes : src_len;
      memcpy(out, src, n);
      return n;
   }

   /* Read through the packing and write plain 16-bit big-endian: the
    * bit order of the stream is the same in all four shapes, so this
    * is the same walk the header parse makes. */
   rdts_br_init(&b, src, src_len, info->packing);
   bits = (size_t)info->core_bytes * 8;
   memset(out, 0, info->core_bytes);
   for (i = 0; i < bits; i++)
   {
      unsigned bit = rdts_br_bit(&b);
      if (b.over)
         break;
      if (bit)
         out[i >> 3] |= (uint8_t)(0x80u >> (i & 7u));
   }
   return i / 8;
}

bool rdts_scan(const uint8_t *src, size_t len,
      rdts_frame_info_t *first, size_t *frames, size_t *samples)
{
   rdts_frame_info_t info, head;
   size_t at = 0, n = 0, total = 0;
   bool   have_head = false;

   if (!src)
      return false;

   while (at + 4 <= len)
   {
      if (rdts_parse_frame_info(src + at, len - at, &info) != RDTS_OK)
         break;
      if (!info.frame_bytes || at + info.frame_bytes > len)
         break;
      if (!have_head && info.kind == RDTS_KIND_CORE)
      {
         head      = info;
         have_head = true;
      }
      /* A substream carries no samples of its own: what it extends
       * was counted with the core frame it follows. */
      if (info.kind == RDTS_KIND_CORE)
      {
         total += info.samples;
         n++;
      }
      at += info.frame_bytes;
   }

   if (!have_head)
      return false;
   if (first)
      *first = head;
   if (frames)
      *frames = n;
   if (samples)
      *samples = total;
   return true;
}
