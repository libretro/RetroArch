/* Copyright  (C) 2010-2024 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (audio_transfer.c).
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>

#include <formats/audio.h>
#include <compat/strcasestr.h>
#include <string/stdstring.h>

#ifdef HAVE_RFLAC
#include <formats/rflac.h>
#endif
#ifdef HAVE_RVORBIS
#include <formats/rvorbis.h>
#endif
#ifdef HAVE_RMP3
#include <formats/rmp3.h>
#endif
#ifdef HAVE_RWAV
#include <formats/rwav.h>
#endif
#ifdef HAVE_RMODTRACKER
#include <formats/rmodtracker.h>
#endif
#ifdef HAVE_ROPUS
#include <formats/ropus.h>
#endif
#ifdef HAVE_RAAC
#include <formats/raac.h>
#ifdef HAVE_RMP4
#include <formats/rmp4.h>
#endif
#endif
#if defined(HAVE_RWEBM) && (defined(HAVE_ROPUS) || defined(HAVE_RVORBIS))
#include <formats/rwebm.h>
#endif

/* One transfer context per codec. Each backend keeps only what it needs;
 * the enum 'type' handed to every entry point selects which arm runs, the
 * same switch-dispatch pattern formats/image_transfer.c uses. */

/* WAV is decoded whole by rwav_load() at start(); the context then holds the
 * decoded interleaved 16-bit PCM and a per-frame read cursor so read_s16 /
 * read_f32 can pull it out in bounded chunks like the other codecs. */
#ifdef HAVE_RWAV
struct audio_transfer_wav
{
   const void *data;    /* encoded bytes from set_buffer_ptr (caller-owned) */
   size_t      size;
   rwav_t      wav;     /* decoded PCM + format, valid after start()         */
   int         opened;  /* rwav_load succeeded                               */
   size_t      cursor;  /* next frame to hand out                            */
};
#endif

#ifdef HAVE_RFLAC
struct audio_transfer_flac
{
   const void *data;    /* encoded bytes from set_buffer_ptr (caller-owned) */
   size_t      size;
   rflac      *handle;  /* opened decoder, NULL until start() succeeds      */
};
#endif

#ifdef HAVE_RVORBIS
struct audio_transfer_vorbis
{
   const void *data;
   size_t      size;
   rvorbis    *handle;
   int         channels; /* cached from rvorbis_get_info at start           */
   /* Demuxed-input path (set_demuxed_ptr): the container's CodecPrivate
    * (the 3 xiph-laced Vorbis headers) and the concatenated audio packets.
    * NULL setup means the plain self-framed path. */
   const void *setup;
   size_t      setup_size;
   const void *packets;
   size_t      packets_size;
   const uint32_t *pkt_sizes;
   size_t      num_packets;
   /* Backing buffer materialised by start() for the decoder to read from,
    * kept alive for the decoder's lifetime and freed in free().  rvorbis
    * decodes Ogg-framed input and reads lazily from the buffer it was
    * opened over, so when fed a demuxed stream start() synthesises an Ogg
    * stream here; a decoder that consumed raw packets directly would leave
    * this NULL.  Format-neutral by intent -- it is decoder scratch, not
    * part of the stream's identity. */
   uint8_t    *synth;
   size_t      synth_size;
   /* WebM buffer mode (.weba): packets extracted from the container at
    * start() into these owned arrays, then handed through the demuxed
    * machinery above (rvorbis wants Ogg framing, which that path
    * synthesises). */
   uint8_t    *own_pkts;
   uint32_t   *own_sizes;
};
#endif

#ifdef HAVE_RMP3
struct audio_transfer_mp3
{
   const void *data;
   size_t      size;
   rmp3        handle;   /* dr_mp3 initialises this in place (by value)      */
   int         inited;   /* handle is embedded, so track init state a flag   */
};
#endif

#ifdef HAVE_RMODTRACKER
struct audio_transfer_mod
{
   const void  *data;   /* module bytes from set_buffer_ptr (caller-owned)  */
   size_t       size;
   rmodtracker *handle; /* replayer, NULL until start() succeeds            */
};
#endif

/* Vorbis and MP3 expose native s16 reads (rvorbis quantises once during
 * its interleave copy; rmp3's synthesis filter emits s16 directly), so
 * the s16 pipeline below never touches float. */

enum audio_type_enum audio_decode_get_type(const char *path)
{
   if (string_is_empty(path))
      return AUDIO_TYPE_NONE;
   if (strcasestr(path, ".flac"))
      return AUDIO_TYPE_FLAC;
   if (strcasestr(path, ".ogg"))
      return AUDIO_TYPE_VORBIS;
   if (strcasestr(path, ".mp3"))
      return AUDIO_TYPE_MP3;
   if (strcasestr(path, ".wav"))
      return AUDIO_TYPE_WAV;
#ifdef HAVE_RMODTRACKER
   if (     strcasestr(path, ".mod")
         || strcasestr(path, ".s3m")
         || strcasestr(path, ".xm"))
      return AUDIO_TYPE_MOD;
#endif
   return AUDIO_TYPE_NONE;
}

#ifdef HAVE_ROPUS
struct audio_transfer_opus
{
   /* Demuxed input (set_demuxed_ptr): OpusHead as setup, concatenated
    * packets, and the per-packet byte lengths (required -- Opus packets
    * are delimited by the container). */
   const void *setup;
   size_t      setup_size;
   const uint8_t *packets;
   size_t      packets_size;
   const uint32_t *pkt_sizes;
   size_t      num_packets;
   /* Buffer input (set_buffer_ptr): a whole Ogg Opus (.opus) file,
    * paged per RFC 3533/7845; packets are read from the pages in
    * place, assembled only when one spans pages, and the final page's
    * granule position bounds emission the way the reference decoder's
    * does. */
   const uint8_t *buf;
   size_t      buf_size;
   int         ogg;          /* buffer is an Ogg Opus stream             */
#ifdef HAVE_RWEBM
   rwebm_t    *demux;        /* buffer is WebM audio (.weba)             */
   int         track_idx;
#endif
   size_t      pg_off;       /* byte offset of the current page          */
   size_t      body_off;     /* byte offset of the current segment       */
   unsigned    seg_idx;      /* next segment in the current page         */
   unsigned    seg_count;
   size_t      audio_off;    /* first audio page (for rewind)            */
   int64_t     limit;        /* emission bound from the end granule      */
   int64_t     emitted;      /* frames handed out so far                 */
   uint8_t     asm_buf[61500]; /* only for packets spanning pages        */
   ropus_t    *handle;
   unsigned    channels;
   size_t      pkt_index;   /* next packet to decode                     */
   size_t      pkt_offset;  /* byte offset of that packet                */
   unsigned    preskip_left;
   int         fmt;         /* 0 none, 1 s16, 2 f32 (pending buf type)   */
   /* Decoded-but-unconsumed frames from the last packet. */
   size_t      pend_frames;
   size_t      pend_pos;
   int16_t     pend_s16[5760 * 2];
   float       pend_f32[5760 * 2];
};

#ifdef HAVE_RWEBM
/* Opus packet duration in 48 kHz frames from the TOC (RFC 6716 s3). */
static int64_t audio_transfer_opus_pkt_frames(const uint8_t *d, size_t n)
{
   static const int16_t fs[32] = {
      480, 960, 1920, 2880, 480, 960, 1920, 2880,   /* SILK NB/MB      */
      480, 960, 1920, 2880,                         /* SILK WB         */
      480, 960, 480, 960,                           /* hybrid          */
      120, 240, 480, 960, 120, 240, 480, 960,       /* CELT NB/WB      */
      120, 240, 480, 960, 120, 240, 480, 960        /* CELT SWB/FB     */
   };
   int count;
   if (!n)
      return 0;
   switch (d[0] & 3)
   {
      case 0: count = 1; break;
      case 3: count = n >= 2 ? (d[1] & 0x3F) : 0; break;
      default: count = 2; break;
   }
   return (int64_t)fs[d[0] >> 3] * count;
}
#endif

/* Validate the Ogg page at off and return its total size (header plus
 * body), or 0 if there is no valid page there.  On success the body
 * offset and segment count are stored. */
static size_t audio_transfer_ogg_page(const uint8_t *buf, size_t size,
      size_t off, size_t *body, unsigned *nsegs)
{
   size_t hdr, total;
   unsigned i, n;
   if (off + 27 > size)
      return 0;
   if (memcmp(buf + off, "OggS", 4) != 0 || buf[off + 4] != 0)
      return 0;
   n   = buf[off + 26];
   hdr = 27 + n;
   if (off + hdr > size)
      return 0;
   total = hdr;
   for (i = 0; i < n; i++)
      total += buf[off + 27 + i];
   if (off + total > size)
      return 0;
   if (body)
      *body = off + hdr;
   if (nsegs)
      *nsegs = n;
   return total;
}
#endif

#ifdef HAVE_RAAC
struct audio_transfer_aac
{
   /* Demuxed input (set_demuxed_ptr): the AudioSpecificConfig as
    * setup, concatenated raw access units, and the per-packet byte
    * lengths (required -- AAC access units are delimited by the
    * container). */
   const void *setup;
   size_t      setup_size;
   const uint8_t *packets;
   size_t      packets_size;
   const uint32_t *pkt_sizes;
   size_t      num_packets;
   raac_t     *handle;
   unsigned    channels;
   size_t      pkt_index;   /* next packet to decode                     */
   size_t      pkt_offset;  /* byte offset of that packet                */
   /* Buffer input (set_buffer_ptr): a whole file.  An ADTS stream
    * (.aac) is walked here directly; an MP4/M4A is demuxed with rmp4
    * when it is built in, packets streaming from the demuxer on
    * demand with the edit list's start trim picked up from the
    * track. */
   const uint8_t *buf;
   size_t      buf_size;
   int         adts;        /* buffer is an ADTS stream                  */
   size_t      adts_pos;    /* byte cursor of the next ADTS frame        */
#ifdef HAVE_RMP4
   rmp4_t     *demux;
   int         track_idx;
#endif
   /* Frames the container's edit list trims from the stream start (the
    * encoder delay); set with audio_transfer_set_start_trim before
    * audio_transfer_start. */
   uint64_t    start_trim;
   uint64_t    trim_left;
   int         fmt;         /* 0 none, 1 s16, 2 f32 (pending buf type)   */
   /* Decoded-but-unconsumed frames from the last packet. */
   size_t      pend_frames;
   size_t      pend_pos;
   int16_t     pend_s16[1024 * 2];
   float       pend_f32[1024 * 2];
};
#endif

void *audio_transfer_new(enum audio_type_enum type)
{
   switch (type)
   {
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
         return calloc(1, sizeof(struct audio_transfer_flac));
#endif
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
         return calloc(1, sizeof(struct audio_transfer_vorbis));
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
         return calloc(1, sizeof(struct audio_transfer_mp3));
#endif
#ifdef HAVE_RMODTRACKER
      case AUDIO_TYPE_MOD:
         return calloc(1, sizeof(struct audio_transfer_mod));
#endif
#ifdef HAVE_ROPUS
      case AUDIO_TYPE_OPUS:
         return calloc(1, sizeof(struct audio_transfer_opus));
#endif
#ifdef HAVE_RAAC
      case AUDIO_TYPE_AAC:
         return calloc(1, sizeof(struct audio_transfer_aac));
#endif
      case AUDIO_TYPE_WAV:
#ifdef HAVE_RWAV
         return calloc(1, sizeof(struct audio_transfer_wav));
#endif
      case AUDIO_TYPE_NONE:
      default:
         break;
   }
   return NULL;
}

void audio_transfer_set_buffer_ptr(void *data, enum audio_type_enum type,
      void *ptr, size_t len)
{
   switch (type)
   {
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
      {
         struct audio_transfer_flac *fl = (struct audio_transfer_flac*)data;
         if (fl)
         {
            fl->data = ptr;
            fl->size = len;
         }
         break;
      }
#endif
#ifdef HAVE_ROPUS
      case AUDIO_TYPE_OPUS:
      {
         struct audio_transfer_opus *op = (struct audio_transfer_opus*)data;
         if (op)
         {
            op->buf      = (const uint8_t*)ptr;
            op->buf_size = len;
         }
         break;
      }
#endif
#ifdef HAVE_RAAC
      case AUDIO_TYPE_AAC:
      {
         struct audio_transfer_aac *ac = (struct audio_transfer_aac*)data;
         if (ac)
         {
            ac->buf      = (const uint8_t*)ptr;
            ac->buf_size = len;
         }
         break;
      }
#endif
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
      {
         struct audio_transfer_vorbis *v = (struct audio_transfer_vorbis*)data;
         if (v)
         {
            v->data = ptr;
            v->size = len;
         }
         break;
      }
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
      {
         struct audio_transfer_mp3 *m = (struct audio_transfer_mp3*)data;
         if (m)
         {
            m->data = ptr;
            m->size = len;
         }
         break;
      }
#endif
#ifdef HAVE_RMODTRACKER
      case AUDIO_TYPE_MOD:
      {
         struct audio_transfer_mod *md = (struct audio_transfer_mod*)data;
         if (md)
         {
            md->data = ptr;
            md->size = len;
         }
         break;
      }
#endif
      case AUDIO_TYPE_WAV:
#ifdef HAVE_RWAV
      {
         struct audio_transfer_wav *w = (struct audio_transfer_wav*)data;
         if (w)
         {
            w->data = ptr;
            w->size = len;
         }
         break;
      }
#endif
      case AUDIO_TYPE_NONE:
      default:
         break;
   }
}

#ifdef HAVE_RVORBIS
/* --- Ogg repackaging for demuxed Vorbis --------------------------------
 *
 * WebM (and Matroska generally) carries Vorbis as raw packets: the three
 * setup headers live in CodecPrivate, xiph-laced, and the audio frames are
 * bare packets.  rvorbis only decodes Ogg-framed input.  Rather than adding
 * a raw-packet path to the decoder (and risking its byte-determinism), we
 * wrap the demuxed packets in a synthetic Ogg stream here and hand that to
 * the unchanged rvorbis_open_memory.
 *
 * rvorbis discards the page serial number and does not verify the page
 * CRC (see start_page_no_capturepattern), so a correct segment table and
 * packet payload suffice; the CRC is written correct anyway, cheaply.
 */

/* Standard Ogg CRC-32: poly 0x04c11db7, no input/output reflection,
 * init 0. Computed directly (no table) -- it runs once per page at open. */
static uint32_t audio_ogg_crc_poly(uint32_t crc)
{
   int j;
   for (j = 0; j < 8; j++)
      crc = (crc & 0x80000000u) ? (crc << 1) ^ 0x04c11db7u : (crc << 1);
   return crc;
}
static uint32_t audio_ogg_crc(const uint8_t *buf, size_t len)
{
   uint32_t crc = 0;
   size_t   i;
   for (i = 0; i < len; i++)
   {
      crc ^= (uint32_t)buf[i] << 24;
      crc  = audio_ogg_crc_poly(crc);
   }
   return crc;
}

/* Append one Ogg page carrying the given packets. Returns new write offset
 * or 0 on overflow. Each packet must be < 255*255 bytes (true for Vorbis
 * setup and typical audio frames). */
static size_t audio_ogg_emit_page(uint8_t *out, size_t cap, size_t at,
      uint32_t serial, uint32_t seqno, int64_t granule,
      const uint8_t * const *pkts, const uint32_t *plens, int npk,
      int bos, int eos)
{
   uint8_t  segtab[255];
   int      nseg = 0;
   int      k, s;
   size_t   body = 0, hdr, total, i;
   uint32_t crc;
   for (k = 0; k < npk; k++)
   {
      uint32_t l = plens[k];
      while (l >= 255) { if (nseg >= 255) return 0; segtab[nseg++] = 255; l -= 255; }
      if (nseg >= 255) return 0;
      segtab[nseg++] = (uint8_t)l;
      body += plens[k];
   }
   hdr   = 27 + (size_t)nseg;
   total = hdr + body;
   if (at + total > cap)
      return 0;
   memcpy(out + at, "OggS", 4);
   out[at + 4] = 0;                                    /* version           */
   out[at + 5] = (uint8_t)((bos ? 2 : 0) | (eos ? 4 : 0));
   for (i = 0; i < 8; i++)
      out[at + 6 + i]  = (uint8_t)((uint64_t)granule >> (8 * i));
   for (i = 0; i < 4; i++)
      out[at + 14 + i] = (uint8_t)(serial >> (8 * i));
   for (i = 0; i < 4; i++)
      out[at + 18 + i] = (uint8_t)(seqno  >> (8 * i));
   out[at + 22] = out[at + 23] = out[at + 24] = out[at + 25] = 0; /* CRC 0  */
   out[at + 26] = (uint8_t)nseg;
   memcpy(out + at + 27, segtab, (size_t)nseg);
   { size_t o = at + hdr;
     for (k = 0; k < npk; k++) { memcpy(out + o, pkts[k], plens[k]); o += plens[k]; } }
   crc = audio_ogg_crc(out + at, total);
   for (i = 0; i < 4; i++)
      out[at + 22 + i] = (uint8_t)(crc >> (8 * i));
   (void)s;
   return at + total;
}

/* Split the xiph-laced CodecPrivate into its 3 header pointers/lengths.
 * Returns 1 on success. */
static int audio_vorbis_split_setup(const uint8_t *priv, size_t size,
      const uint8_t *hdr[3], uint32_t hlen[3])
{
   size_t p, sum;
   int    i;
   if (!priv || size < 3 || priv[0] != 2) /* nheaders-1 == 2 */
      return 0;
   p = 1;
   for (i = 0; i < 2; i++)
   {
      uint32_t l = 0;
      while (p < size && priv[p] == 255) { l += 255; p++; }
      if (p >= size) return 0;
      l += priv[p++];
      hlen[i] = l;
   }
   sum = (size_t)hlen[0] + hlen[1];
   if (p + sum > size)
      return 0;
   hlen[2]  = (uint32_t)(size - p - sum);
   hdr[0]   = priv + p;
   hdr[1]   = priv + p + hlen[0];
   hdr[2]   = priv + p + hlen[0] + hlen[1];
   return 1;
}

/* Build a synthetic Ogg stream from demuxed Vorbis setup + packets.
 * Stores the malloc'd buffer in v->synth / v->synth_size. Returns 1 on
 * success. */
static int audio_vorbis_build_ogg(struct audio_transfer_vorbis *v)
{
   const uint8_t *hdr[3];
   uint32_t       hlen[3];
   const uint8_t *pkts[2];
   uint32_t       plens[2];
   size_t         cap, at, i, off;
   uint32_t       seq = 0;
   uint8_t       *out;
   const uint32_t serial = 0x0057454Du; /* 'WEM' */
   if (!audio_vorbis_split_setup((const uint8_t*)v->setup, v->setup_size,
            hdr, hlen))
      return 0;
   /* Worst case: every packet gets its own page (27 + lacing + body).
    * Lacing is ceil(len/255)+1 bytes; bound generously. */
   cap = v->setup_size + v->packets_size + 64;
   cap += (v->num_packets + 4) * 64;
   cap += (v->packets_size / 255) + (v->setup_size / 255) + 64;
   out = (uint8_t*)malloc(cap);
   if (!out)
      return 0;
   at = 0;
   /* Page 0: identification header, BOS, single packet. */
   pkts[0] = hdr[0]; plens[0] = hlen[0];
   at = audio_ogg_emit_page(out, cap, at, serial, seq++, 0, pkts, plens, 1, 1, 0);
   if (!at) { free(out); return 0; }
   /* Page 1: comment + setup headers. */
   pkts[0] = hdr[1]; plens[0] = hlen[1];
   pkts[1] = hdr[2]; plens[1] = hlen[2];
   at = audio_ogg_emit_page(out, cap, at, serial, seq++, 0, pkts, plens, 2, 0, 0);
   if (!at) { free(out); return 0; }
   /* Audio pages: one packet per page. Granule is unknown per-packet here;
    * rvorbis derives sample counts from the decoded frames, so a monotone
    * placeholder granule suffices. */
   off = 0;
   for (i = 0; i < v->num_packets; i++)
   {
      uint32_t len = v->pkt_sizes ? v->pkt_sizes[i]
                   : (uint32_t)v->packets_size; /* single blob if unsplit */
      const uint8_t *pk = (const uint8_t*)v->packets + off;
      int eos = (i + 1 == v->num_packets);
      pkts[0] = pk; plens[0] = len;
      at = audio_ogg_emit_page(out, cap, at, serial, seq++,
            (int64_t)(i + 1) * 1024, pkts, plens, 1, 0, eos);
      if (!at) { free(out); return 0; }
      off += len;
   }
   v->synth      = out;
   v->synth_size = at;
   return 1;
}
#endif

bool audio_transfer_set_demuxed_ptr(void *data, enum audio_type_enum type,
      const void *setup, size_t setup_size,
      const void *packets, size_t packets_size,
      const uint32_t *sizes, size_t num_packets)
{
   switch (type)
   {
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
      {
         struct audio_transfer_vorbis *v = (struct audio_transfer_vorbis*)data;
         if (!v)
            return false;
         v->setup        = setup;
         v->setup_size   = setup_size;
         v->packets      = packets;
         v->packets_size = packets_size;
         v->pkt_sizes    = sizes;
         v->num_packets  = num_packets;
         return true;
      }
#endif
#ifdef HAVE_ROPUS
      case AUDIO_TYPE_OPUS:
      {
         struct audio_transfer_opus *op = (struct audio_transfer_opus*)data;
         if (!op)
            return false;
         op->setup        = setup;
         op->setup_size   = setup_size;
         op->packets      = (const uint8_t*)packets;
         op->packets_size = packets_size;
         op->pkt_sizes    = sizes;
         op->num_packets  = num_packets;
         return true;
      }
#endif
#ifdef HAVE_RAAC
      case AUDIO_TYPE_AAC:
      {
         struct audio_transfer_aac *ac = (struct audio_transfer_aac*)data;
         if (!ac)
            return false;
         ac->setup        = setup;
         ac->setup_size   = setup_size;
         ac->packets      = (const uint8_t*)packets;
         ac->packets_size = packets_size;
         ac->pkt_sizes    = sizes;
         ac->num_packets  = num_packets;
         return true;
      }
#endif
      case AUDIO_TYPE_NONE:
      default:
         break;
   }
   (void)data; (void)setup; (void)setup_size;
   (void)packets; (void)packets_size; (void)sizes; (void)num_packets;
   return false;
}

enum audio_type_enum audio_transfer_ogg_audio_type(const void *buf,
      size_t len)
{
   const uint8_t *b = (const uint8_t*)buf;
   unsigned nsegs, i;
   size_t first, plen = 0;
   if (!b || len < 28
         || b[0] != 'O' || b[1] != 'g' || b[2] != 'g' || b[3] != 'S'
         || b[4] != 0)
      return AUDIO_TYPE_NONE;
   /* first page: the identification header sits alone in it; its
    * opening bytes name the codec */
   nsegs = b[26];
   first = 27 + nsegs;
   if (len < first + 8)
      return AUDIO_TYPE_NONE;
   for (i = 0; i < nsegs; i++)
      plen += b[27 + i];
   if (plen < 7 || len < first + plen)
      return AUDIO_TYPE_NONE;
#ifdef HAVE_ROPUS
   if (plen >= 8 && !memcmp(b + first, "OpusHead", 8))
      return AUDIO_TYPE_OPUS;
#endif
#ifdef HAVE_RVORBIS
   if (!memcmp(b + first, "\x01vorbis", 7))
      return AUDIO_TYPE_VORBIS;
#endif
   return AUDIO_TYPE_NONE;
}

enum audio_type_enum audio_transfer_webm_audio_type(const void *buf,
      size_t len)
{
#if defined(HAVE_RWEBM) && (defined(HAVE_ROPUS) || defined(HAVE_RVORBIS))
   rwebm_t *wm;
   int i;
   enum audio_type_enum found = AUDIO_TYPE_NONE;
   const uint8_t *b = (const uint8_t*)buf;
   if (!b || len < 4
         || b[0] != 0x1A || b[1] != 0x45 || b[2] != 0xDF || b[3] != 0xA3)
      return AUDIO_TYPE_NONE;
   if (!(wm = rwebm_open_memory(buf, len)))
      return AUDIO_TYPE_NONE;
   for (i = 0; i < rwebm_num_tracks(wm) && found == AUDIO_TYPE_NONE; i++)
   {
      const rwebm_track *t = rwebm_get_track(wm, i);
      if (!t || t->type != RWEBM_TRACK_AUDIO || !t->codec_private_size)
         continue;
#ifdef HAVE_ROPUS
      if (t->codec == RWEBM_CODEC_OPUS)
         found = AUDIO_TYPE_OPUS;
#endif
#ifdef HAVE_RVORBIS
      if (t->codec == RWEBM_CODEC_VORBIS)
         found = AUDIO_TYPE_VORBIS;
#endif
   }
   rwebm_close(wm);
   return found;
#else
   (void)buf;
   (void)len;
   return AUDIO_TYPE_NONE;
#endif
}

bool audio_transfer_set_start_trim(void *data, enum audio_type_enum type,
      uint64_t frames)
{
   switch (type)
   {
#ifdef HAVE_RAAC
      case AUDIO_TYPE_AAC:
      {
         struct audio_transfer_aac *ac = (struct audio_transfer_aac*)data;
         if (!ac)
            return false;
         ac->start_trim = frames;
         ac->trim_left  = frames;
         return true;
      }
#endif
      case AUDIO_TYPE_NONE:
      default:
         break;
   }
   return false;
}

bool audio_transfer_start(void *data, enum audio_type_enum type)
{
   switch (type)
   {
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
      {
         struct audio_transfer_flac *fl = (struct audio_transfer_flac*)data;
         if (!fl || !fl->data)
            return false;
         fl->handle = rflac_open_memory(fl->data, fl->size);
         return fl->handle != NULL;
      }
#endif
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
      {
         struct audio_transfer_vorbis *v = (struct audio_transfer_vorbis*)data;
         const unsigned char *buf;
         int   len, err = 0;
         if (!v)
            return false;
#ifdef HAVE_RWEBM
         /* Buffer mode, WebM audio (.weba): extract the Vorbis track's
          * setup and packets from the container, then reuse the
          * demuxed path's Ogg synthesis below (rvorbis wants Ogg
          * framing). */
         if (!v->setup && v->data && v->size >= 4
               && ((const uint8_t*)v->data)[0] == 0x1A
               && ((const uint8_t*)v->data)[1] == 0x45
               && ((const uint8_t*)v->data)[2] == 0xDF
               && ((const uint8_t*)v->data)[3] == 0xA3)
         {
            rwebm_t *wm = rwebm_open_memory(v->data, v->size);
            const rwebm_track *at = NULL;
            int i, track = -1;
            rwebm_packet pkt;
            size_t napkts = 0, abytes = 0, off = 0, k = 0;
            if (!wm)
               return false;
            for (i = 0; i < rwebm_num_tracks(wm); i++)
            {
               const rwebm_track *t = rwebm_get_track(wm, i);
               if (t && t->type == RWEBM_TRACK_AUDIO
                     && t->codec == RWEBM_CODEC_VORBIS
                     && t->codec_private_size)
               {
                  at    = t;
                  track = i;
                  break;
               }
            }
            if (!at)
            {
               rwebm_close(wm);
               return false;
            }
            while (rwebm_read_packet(wm, &pkt) == 1)
               if (pkt.track == track)
               {
                  napkts++;
                  abytes += pkt.size;
               }
            rwebm_rewind(wm);
            v->own_pkts  = (uint8_t*)malloc(abytes ? abytes : 1);
            v->own_sizes = (uint32_t*)malloc(
                  (napkts ? napkts : 1) * sizeof(uint32_t));
            if (!v->own_pkts || !v->own_sizes || !napkts)
            {
               rwebm_close(wm);
               return false;
            }
            while (rwebm_read_packet(wm, &pkt) == 1)
            {
               if (pkt.track != track)
                  continue;
               memcpy(v->own_pkts + off, pkt.data, pkt.size);
               v->own_sizes[k++] = (uint32_t)pkt.size;
               off += pkt.size;
            }
            v->setup        = at->codec_private;
            v->setup_size   = at->codec_private_size;
            v->packets      = v->own_pkts;
            v->packets_size = off;
            v->pkt_sizes    = v->own_sizes;
            v->num_packets  = k;
            rwebm_close(wm);
         }
#endif
         if (v->setup)
         {
            /* Demuxed path: repackage the container's raw Vorbis setup +
             * packets into a synthetic Ogg stream, then open that. */
            if (!audio_vorbis_build_ogg(v))
               return false;
            buf = v->synth;
            len = (int)v->synth_size;
         }
         else
         {
            if (!v->data)
               return false;
            buf = (const unsigned char*)v->data;
            len = (int)v->size;
         }
         v->handle = rvorbis_open_memory(buf, len, &err, NULL);
         if (!v->handle)
            return false;
         v->channels = rvorbis_get_info(v->handle).channels;
         return true;
      }
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
      {
         struct audio_transfer_mp3 *m = (struct audio_transfer_mp3*)data;
         if (!m || !m->data)
            return false;
         m->inited = (rmp3_init_memory(&m->handle, m->data, m->size) != 0);
         return m->inited != 0;
      }
#endif
#ifdef HAVE_RMODTRACKER
      case AUDIO_TYPE_MOD:
      {
         struct audio_transfer_mod *md = (struct audio_transfer_mod*)data;
         if (!md || !md->data)
            return false;
         md->handle = rmodtracker_open_memory(md->data, md->size);
         return md->handle != NULL;
      }
#endif
      case AUDIO_TYPE_WAV:
#ifdef HAVE_RWAV
      {
         struct audio_transfer_wav *w = (struct audio_transfer_wav*)data;
         if (!w || !w->data)
            return false;
         /* rwav only handles 8- and 16-bit PCM; decode the whole buffer up
          * front (WAV has no incremental decode - it is raw PCM already). */
         if (rwav_load(&w->wav, w->data, w->size) != RWAV_ITERATE_DONE)
            return false;
         if (w->wav.bitspersample != 16 && w->wav.bitspersample != 8)
         {
            rwav_free(&w->wav);
            return false;
         }
         w->opened = 1;
         w->cursor = 0;
         return true;
      }
#endif
#ifdef HAVE_ROPUS
      case AUDIO_TYPE_OPUS:
      {
         struct audio_transfer_opus *op = (struct audio_transfer_opus*)data;
         if (!op)
            return false;
#ifdef HAVE_RWEBM
         /* buffer mode, WebM audio (.weba): demux with rwebm; packets
          * stream from the demuxer on demand.  The exact decodable
          * length comes from the packets' TOC durations less pre-skip
          * and the container's end trimming (DiscardPadding). */
         if (!op->setup && op->buf && op->buf_size >= 4
               && op->buf[0] == 0x1A && op->buf[1] == 0x45
               && op->buf[2] == 0xDF && op->buf[3] == 0xA3)
         {
            const rwebm_track *at = NULL;
            int i;
            rwebm_packet pkt;
            int64_t toc = 0, discard_ns = 0, preskip = 0;
            if (!(op->demux = rwebm_open_memory(op->buf, op->buf_size)))
               return false;
            op->track_idx = -1;
            for (i = 0; i < rwebm_num_tracks(op->demux); i++)
            {
               const rwebm_track *t = rwebm_get_track(op->demux, i);
               if (t && t->type == RWEBM_TRACK_AUDIO
                     && t->codec == RWEBM_CODEC_OPUS
                     && t->codec_private_size)
               {
                  at            = t;
                  op->track_idx = i;
                  break;
               }
            }
            if (!at)
               return false;
            op->handle = ropus_open(at->codec_private,
                  at->codec_private_size);
            if (!op->handle)
               return false;
            while (rwebm_read_packet(op->demux, &pkt) == 1)
            {
               if (pkt.track != op->track_idx)
                  continue;
               toc += audio_transfer_opus_pkt_frames(pkt.data, pkt.size);
               if (pkt.discard_padding > 0)
                  discard_ns += pkt.discard_padding;
            }
            rwebm_rewind(op->demux);
            preskip   = (int64_t)ropus_preskip(op->handle);
            op->limit = -1;
            if (toc > 0)
            {
               op->limit = toc - preskip
                  - (discard_ns * 48000 + 500000000) / 1000000000;
               if (op->limit < 0)
                  op->limit = 0;
            }
            op->emitted = 0;
         }
         else
#endif
         /* buffer mode: a whole Ogg Opus file.  The ID header sits
          * alone on the first page (RFC 7845), the comment header
          * finishes on its own page(s), and audio pages follow; the
          * final page's granule position, less the pre-skip, is the
          * stream's exact decodable length. */
         if (!op->setup && op->buf)
         {
            size_t body = 0, off, total;
            unsigned nsegs = 0;
            int64_t  last_granule = -1;
            total = audio_transfer_ogg_page(op->buf, op->buf_size, 0,
                  &body, &nsegs);
            if (!total || !(op->buf[5] & 0x02))   /* first page: BOS  */
               return false;
            op->handle = ropus_open(op->buf + body, total - (body - 0));
            if (!op->handle)
               return false;
            /* skip the comment header: pages until a lacing value
             * below 255 completes the packet */
            off = total;
            for (;;)
            {
               unsigned i;
               int done = 0;
               size_t psz = audio_transfer_ogg_page(op->buf, op->buf_size,
                     off, &body, &nsegs);
               if (!psz)
                  return false;
               for (i = 0; i < nsegs; i++)
                  if (op->buf[off + 27 + i] < 255)
                     done = 1;
               off += psz;
               if (done)
                  break;
            }
            op->ogg       = 1;
            op->audio_off = off;
            op->pg_off    = off;
            op->seg_idx   = 0;
            op->body_off  = 0;
            /* end granule: walk the remaining pages once */
            while (off < op->buf_size)
            {
               size_t psz = audio_transfer_ogg_page(op->buf, op->buf_size,
                     off, NULL, NULL);
               uint64_t g;
               unsigned k;
               if (!psz)
                  break;
               g = 0;
               for (k = 0; k < 8; k++)
                  g |= (uint64_t)op->buf[off + 6 + k] << (8 * k);
               if (g != (uint64_t)-1)
                  last_granule = (int64_t)g;
               off += psz;
            }
            op->limit = -1;
            if (last_granule >= 0)
            {
               op->limit = last_granule - (int64_t)ropus_preskip(op->handle);
               if (op->limit < 0)
                  op->limit = 0;
            }
            op->emitted = 0;
         }
         else
         {
            if (!op->setup || !op->packets || !op->pkt_sizes)
               return false;      /* no input set                        */
            op->handle = ropus_open(op->setup, op->setup_size);
            if (!op->handle)
               return false;
         }
         op->channels     = ropus_channels(op->handle);
         op->preskip_left = ropus_preskip(op->handle);
         op->pkt_index    = 0;
         op->pkt_offset   = 0;
         op->pend_frames  = 0;
         op->pend_pos     = 0;
         op->fmt          = 0;
         return true;
      }
#endif
#ifdef HAVE_RAAC
      case AUDIO_TYPE_AAC:
      {
         struct audio_transfer_aac *ac = (struct audio_transfer_aac*)data;
         if (!ac)
            return false;
         /* buffer mode, ADTS stream: the header carries the setup,
          * so synthesise the AudioSpecificConfig from it (LC only,
          * matching the decoder's scope) and walk frames in place */
         if (!ac->setup && ac->buf && ac->buf_size >= 7
               && ac->buf[0] == 0xFF && (ac->buf[1] & 0xF6) == 0xF0)
         {
            unsigned profile = (ac->buf[2] >> 6) & 3;   /* 1 = LC     */
            unsigned sfi     = (ac->buf[2] >> 2) & 15;
            unsigned chcfg   = ((ac->buf[2] & 1) << 2)
                             | ((ac->buf[3] >> 6) & 3);
            uint8_t  asc[2];
            if (profile != 1 || sfi > 12 || chcfg < 1 || chcfg > 2)
               return false;
            asc[0] = (uint8_t)((2u << 3) | (sfi >> 1));
            asc[1] = (uint8_t)(((sfi & 1) << 7) | (chcfg << 3));
            ac->handle = raac_open(asc, 2);
            if (!ac->handle)
               return false;
            ac->adts       = 1;
            ac->adts_pos   = 0;
            ac->start_trim = 0;   /* ADTS carries no delay signalling  */
         }
         else
#ifdef HAVE_RMP4
         /* buffer mode: a whole MP4/M4A; demux it here */
         if (!ac->setup && ac->buf)
         {
            const rmp4_track *at = NULL;
            int i;
            if (!(ac->demux = rmp4_open_memory(ac->buf, ac->buf_size)))
               return false;
            ac->track_idx = -1;
            for (i = 0; i < rmp4_num_tracks(ac->demux); i++)
            {
               const rmp4_track *t = rmp4_get_track(ac->demux, i);
               if (t && t->type == RMP4_TRACK_AUDIO
                     && t->codec == RMP4_CODEC_AAC
                     && t->codec_private_size)
               {
                  at            = t;
                  ac->track_idx = i;
                  break;
               }
            }
            if (!at)
               return false;
            ac->handle = raac_open(at->codec_private,
                  at->codec_private_size);
            if (!ac->handle)
               return false;
            /* the track's edit list carries the encoder delay */
            ac->start_trim = at->media_skip;
         }
         else
#endif
         {
            if (!ac->setup || !ac->packets || !ac->pkt_sizes)
               return false;      /* no input set                        */
            ac->handle = raac_open(ac->setup, ac->setup_size);
            if (!ac->handle)
               return false;
         }
         ac->channels    = raac_channels(ac->handle);
         ac->trim_left   = ac->start_trim;
         ac->pkt_index   = 0;
         ac->pkt_offset  = 0;
         ac->pend_frames = 0;
         ac->pend_pos    = 0;
         ac->fmt         = 0;
         return true;
      }
#endif
      case AUDIO_TYPE_NONE:
      default:
         break;
   }
   return false;
}

bool audio_transfer_is_valid(void *data, enum audio_type_enum type)
{
   switch (type)
   {
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
      {
         struct audio_transfer_flac *fl = (struct audio_transfer_flac*)data;
         return (fl && fl->handle);
      }
#endif
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
      {
         struct audio_transfer_vorbis *v = (struct audio_transfer_vorbis*)data;
         return (v && v->handle);
      }
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
      {
         struct audio_transfer_mp3 *m = (struct audio_transfer_mp3*)data;
         return (m && m->inited);
      }
#endif
#ifdef HAVE_RMODTRACKER
      case AUDIO_TYPE_MOD:
      {
         struct audio_transfer_mod *md = (struct audio_transfer_mod*)data;
         return (md && md->handle);
      }
#endif
      case AUDIO_TYPE_WAV:
#ifdef HAVE_RWAV
      {
         struct audio_transfer_wav *w = (struct audio_transfer_wav*)data;
         return w && w->opened;
      }
#endif
#ifdef HAVE_ROPUS
      case AUDIO_TYPE_OPUS:
      {
         struct audio_transfer_opus *op = (struct audio_transfer_opus*)data;
         return op && op->handle;
      }
#endif
#ifdef HAVE_RAAC
      case AUDIO_TYPE_AAC:
      {
         struct audio_transfer_aac *ac = (struct audio_transfer_aac*)data;
         return ac && ac->handle;
      }
#endif
      case AUDIO_TYPE_NONE:
      default:
         break;
   }
   return false;
}

bool audio_transfer_info(void *data, enum audio_type_enum type,
      unsigned *channels, unsigned *rate, uint64_t *total_frames)
{
   switch (type)
   {
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
      {
         struct audio_transfer_flac *fl = (struct audio_transfer_flac*)data;
         if (!fl || !fl->handle)
            return false;
         if (channels)
            *channels     = (unsigned)fl->handle->channels;
         if (rate)
            *rate         = (unsigned)fl->handle->sampleRate;
         if (total_frames)
            *total_frames = (uint64_t)fl->handle->totalPCMFrameCount;
         return true;
      }
#endif
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
      {
         struct audio_transfer_vorbis *v = (struct audio_transfer_vorbis*)data;
         rvorbis_info info;
         if (!v || !v->handle)
            return false;
         info = rvorbis_get_info(v->handle);
         if (channels)
            *channels     = (unsigned)info.channels;
         if (rate)
            *rate         = (unsigned)info.sample_rate;
         if (total_frames)
            *total_frames = (uint64_t)rvorbis_stream_length_in_samples(v->handle);
         return true;
      }
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
      {
         struct audio_transfer_mp3 *m = (struct audio_transfer_mp3*)data;
         if (!m || !m->inited)
            return false;
         if (channels)
            *channels     = (unsigned)m->handle.channels;
         if (rate)
            *rate         = (unsigned)m->handle.sampleRate;
         if (total_frames) /* streaming; length not tracked here */
            *total_frames = 0;
         return true;
      }
#endif
#ifdef HAVE_RMODTRACKER
      case AUDIO_TYPE_MOD:
      {
         struct audio_transfer_mod *md = (struct audio_transfer_mod*)data;
         if (!md || !md->handle)
            return false;
         if (channels)  /* the replayer always mixes interleaved stereo */
            *channels     = 2;
         if (rate)
            *rate         = (unsigned)rmodtracker_sample_rate(md->handle);
         if (total_frames) /* one pass through the sequence */
            *total_frames = (uint64_t)rmodtracker_duration_frames(md->handle);
         return true;
      }
#endif
      case AUDIO_TYPE_WAV:
#ifdef HAVE_RWAV
      {
         struct audio_transfer_wav *w = (struct audio_transfer_wav*)data;
         if (!w || !w->opened)
            return false;
         if (channels)
            *channels     = w->wav.numchannels;
         if (rate)
            *rate         = w->wav.samplerate;
         if (total_frames) /* WAV is fully decoded, so length is known */
            *total_frames = (uint64_t)w->wav.numsamples;
         return true;
      }
#endif
#ifdef HAVE_ROPUS
      case AUDIO_TYPE_OPUS:
      {
         struct audio_transfer_opus *op = (struct audio_transfer_opus*)data;
         if (!op || !op->handle)
            return false;
         if (channels)
            *channels = op->channels;
         if (rate)
            *rate = 48000;
         if (total_frames)
            *total_frames = 0;   /* unknown without scanning            */
         return true;
      }
#endif
#ifdef HAVE_RAAC
      case AUDIO_TYPE_AAC:
      {
         struct audio_transfer_aac *ac = (struct audio_transfer_aac*)data;
         if (!ac || !ac->handle)
            return false;
         if (channels)
            *channels = ac->channels;
         if (rate)
            *rate = raac_sample_rate(ac->handle);
         if (total_frames)
            *total_frames = 0;   /* the caller knows the packet count   */
         return true;
      }
#endif
      case AUDIO_TYPE_NONE:
      default:
         break;
   }
   return false;
}

#ifdef HAVE_ROPUS
/* Assemble the next Opus packet from the Ogg pages.  Returns 1 with
 * the packet bytes (aliasing the buffer unless it spans pages, in
 * which case it is copied into asm_buf), 0 at end of stream, < 0 on a
 * malformed stream. */
static int audio_transfer_opus_next_pkt(struct audio_transfer_opus *op,
      const uint8_t **pdata, uint32_t *plen)
{
   size_t asm_len = 0;
   int    spans   = 0;
   for (;;)
   {
      size_t body = 0;
      unsigned nsegs = 0;
      size_t psz = audio_transfer_ogg_page(op->buf, op->buf_size,
            op->pg_off, &body, &nsegs);
      size_t start;
      size_t run = 0;
      int    done = 0;
      if (!psz)
         return asm_len ? -1 : 0;   /* out of pages                   */
      if (op->seg_idx == 0)
         op->body_off = body;
      if (asm_len && op->seg_idx == 0 && !(op->buf[op->pg_off + 5] & 0x01))
         return -1;                 /* continuation page not flagged  */
      start = op->body_off;
      while (op->seg_idx < nsegs)
      {
         unsigned lace = op->buf[op->pg_off + 27 + op->seg_idx];
         run += lace;
         op->seg_idx++;
         if (lace < 255)
         {
            done = 1;
            break;
         }
      }
      op->body_off = start + run;
      if (op->seg_idx >= nsegs && !done)
      {
         /* packet continues on the next page: stash what we have */
         if (run)
         {
            if (asm_len + run > sizeof(op->asm_buf))
               return -1;
            memcpy(op->asm_buf + asm_len, op->buf + start, run);
            asm_len += run;
         }
         spans       = 1;
         op->pg_off += psz;
         op->seg_idx = 0;
         continue;
      }
      if (op->seg_idx >= nsegs)
      {
         op->pg_off += psz;
         op->seg_idx = 0;
      }
      if (!spans)
      {
         *pdata = op->buf + start;
         *plen  = (uint32_t)run;
      }
      else
      {
         if (asm_len + run > sizeof(op->asm_buf))
            return -1;
         memcpy(op->asm_buf + asm_len, op->buf + start, run);
         asm_len += run;
         *pdata  = op->asm_buf;
         *plen   = (uint32_t)asm_len;
      }
      return 1;
   }
}

/* Decode the next Opus packet into the pending buffer in the requested
 * format (1 = s16, 2 = f32), honouring pre-skip.  Returns frames now
 * pending, 0 at end of stream, < 0 on error. */
static int audio_transfer_opus_fill(struct audio_transfer_opus *op, int fmt)
{
   while (op->pend_frames == 0)
   {
      const uint8_t *pdata;
      uint32_t plen;
      int r;
      unsigned skip;
#ifdef HAVE_RWEBM
      if (op->demux)
      {
         /* .weba: pull the next packet from the demuxer */
         rwebm_packet pkt;
         for (;;)
         {
            if (rwebm_read_packet(op->demux, &pkt) != 1)
               return 0;
            if (pkt.track == op->track_idx)
               break;
         }
         pdata = pkt.data;
         plen  = (uint32_t)pkt.size;
      }
      else
#endif
      if (op->ogg)
      {
         r = audio_transfer_opus_next_pkt(op, &pdata, &plen);
         if (r <= 0)
            return r;
      }
      else
      {
         if (op->pkt_index >= op->num_packets)
            return 0;
         plen = op->pkt_sizes[op->pkt_index];
         if (op->pkt_offset + plen > op->packets_size)
            return -1;
         pdata = op->packets + op->pkt_offset;
         op->pkt_offset += plen;
         op->pkt_index++;
      }
      if (fmt == 1)
         r = ropus_decode_s16(op->handle, pdata, plen, op->pend_s16);
      else
         r = ropus_decode_f32(op->handle, pdata, plen, op->pend_f32);
      if (r < 0)
         return -1;
      op->pend_frames = (size_t)r;
      op->pend_pos    = 0;
      skip = op->preskip_left;
      if (skip)
      {
         if (skip >= op->pend_frames)
         {
            op->preskip_left -= (unsigned)op->pend_frames;
            op->pend_frames = 0;   /* whole packet skipped; loop        */
         }
         else
         {
            op->pend_pos      = skip;
            op->pend_frames  -= skip;
            op->preskip_left  = 0;
         }
      }
      /* Buffer modes: the end granule (Ogg) or the TOC total less end
       * trimming (WebM) bounds emission; frames now pending will all
       * be consumed, so credit them here. */
      if ((op->ogg
#ifdef HAVE_RWEBM
               || op->demux
#endif
            ) && op->limit >= 0 && op->pend_frames)
      {
         int64_t left = op->limit - op->emitted;
         if (left <= 0)
         {
            op->pend_frames = 0;
            return 0;
         }
         if ((int64_t)op->pend_frames > left)
            op->pend_frames = (size_t)left;
         op->emitted += (int64_t)op->pend_frames;
      }
   }
   op->fmt = fmt;
   return (int)op->pend_frames;
}
#endif

#ifdef HAVE_RAAC
/* Decode the next AAC access unit into the pending buffer in the
 * requested format (1 = s16, 2 = f32), honouring the edit list's start
 * trim.  Returns frames now pending, 0 at end of stream, < 0 on
 * error. */
static int audio_transfer_aac_fill(struct audio_transfer_aac *ac, int fmt)
{
   while (ac->pend_frames == 0)
   {
      const uint8_t *pdata;
      uint32_t plen;
      int r;
      uint64_t skip;
      if (ac->adts)
      {
         /* walk the next ADTS frame: 12-bit sync, CRC flag choosing a
          * 7- or 9-byte header, 13-bit total frame length */
         size_t   pos = ac->adts_pos;
         unsigned hdr, flen;
         if (pos + 7 > ac->buf_size)
            return 0;
         if (ac->buf[pos] != 0xFF || (ac->buf[pos + 1] & 0xF6) != 0xF0)
            return 0;             /* lost sync: treat as end of stream */
         hdr  = (ac->buf[pos + 1] & 1) ? 7 : 9;
         flen = ((unsigned)(ac->buf[pos + 3] & 3) << 11)
              | ((unsigned)ac->buf[pos + 4] << 3)
              | ((unsigned)ac->buf[pos + 5] >> 5);
         if (flen <= hdr || pos + flen > ac->buf_size)
            return 0;
         pdata        = ac->buf + pos + hdr;
         plen         = flen - hdr;
         ac->adts_pos = pos + flen;
      }
      else
#ifdef HAVE_RMP4
      if (ac->demux)
      {
         /* buffer mode: pull the next access unit from the demuxer */
         rmp4_packet pkt;
         for (;;)
         {
            if (rmp4_read_packet(ac->demux, &pkt) != 1)
               return 0;
            if (pkt.track == ac->track_idx)
               break;
         }
         pdata = pkt.data;
         plen  = (uint32_t)pkt.size;
      }
      else
#endif
      {
         if (ac->pkt_index >= ac->num_packets)
            return 0;
         plen = ac->pkt_sizes[ac->pkt_index];
         if (ac->pkt_offset + plen > ac->packets_size)
            return -1;
         pdata = ac->packets + ac->pkt_offset;
         ac->pkt_offset += plen;
         ac->pkt_index++;
      }
      if (fmt == 1)
         r = raac_decode_s16(ac->handle, pdata, plen, ac->pend_s16);
      else
         r = raac_decode_f32(ac->handle, pdata, plen, ac->pend_f32);
      if (r < 0)
         return -1;
      ac->pend_frames = (size_t)r;
      ac->pend_pos    = 0;
      skip = ac->trim_left;
      if (skip)
      {
         if (skip >= ac->pend_frames)
         {
            ac->trim_left  -= ac->pend_frames;
            ac->pend_frames = 0;   /* whole packet trimmed; loop        */
         }
         else
         {
            ac->pend_pos     = (size_t)skip;
            ac->pend_frames -= (size_t)skip;
            ac->trim_left    = 0;
         }
      }
   }
   ac->fmt = fmt;
   return (int)ac->pend_frames;
}
#endif

int audio_transfer_read_s16(void *data, enum audio_type_enum type,
      int16_t *out, size_t frames, size_t *frames_out)
{
   size_t produced = 0;

   switch (type)
   {
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
      {
         struct audio_transfer_flac *fl = (struct audio_transfer_flac*)data;
         if (!fl || !fl->handle)
            return AUDIO_PROCESS_ERROR;
         produced = (size_t)rflac_read_pcm_frames_s16(
               fl->handle, (uint64_t)frames, out);
         break;
      }
#endif
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
      {
         struct audio_transfer_vorbis *v = (struct audio_transfer_vorbis*)data;
         if (!v || !v->handle)
            return AUDIO_PROCESS_ERROR;
         produced = (size_t)rvorbis_get_samples_s16_interleaved(
               v->handle, v->channels, out, (int)frames * v->channels);
         break;
      }
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
      {
         struct audio_transfer_mp3 *m = (struct audio_transfer_mp3*)data;
         if (!m || !m->inited)
            return AUDIO_PROCESS_ERROR;
         produced = (size_t)rmp3_read_s16(&m->handle, (uint64_t)frames, out);
         break;
      }
#endif
#ifdef HAVE_RMODTRACKER
      case AUDIO_TYPE_MOD:
      {
         struct audio_transfer_mod *md = (struct audio_transfer_mod*)data;
         if (!md || !md->handle)
            return AUDIO_PROCESS_ERROR;
         produced = rmodtracker_get_samples_s16_interleaved(
               md->handle, out, frames);
         break;
      }
#endif
#ifdef HAVE_ROPUS
      case AUDIO_TYPE_OPUS:
      {
         struct audio_transfer_opus *op = (struct audio_transfer_opus*)data;
         if (!op || !op->handle || op->fmt == 2)
            return AUDIO_PROCESS_ERROR;
         while (produced < frames)
         {
            size_t take;
            int r = audio_transfer_opus_fill(op, 1);
            if (r < 0)
               return AUDIO_PROCESS_ERROR;
            if (r == 0)
               break;
            take = frames - produced;
            if (take > op->pend_frames)
               take = op->pend_frames;
            memcpy(out + produced * op->channels,
                  op->pend_s16 + op->pend_pos * op->channels,
                  take * op->channels * sizeof(int16_t));
            op->pend_pos    += take;
            op->pend_frames -= take;
            produced        += take;
         }
         break;
      }
#endif
#ifdef HAVE_RAAC
      case AUDIO_TYPE_AAC:
      {
         struct audio_transfer_aac *ac = (struct audio_transfer_aac*)data;
         if (!ac || !ac->handle || ac->fmt == 2)
            return AUDIO_PROCESS_ERROR;
         while (produced < frames)
         {
            size_t take;
            int r = audio_transfer_aac_fill(ac, 1);
            if (r < 0)
               return AUDIO_PROCESS_ERROR;
            if (r == 0)
               break;
            take = frames - produced;
            if (take > ac->pend_frames)
               take = ac->pend_frames;
            memcpy(out + produced * ac->channels,
                  ac->pend_s16 + ac->pend_pos * ac->channels,
                  take * ac->channels * sizeof(int16_t));
            ac->pend_pos    += take;
            ac->pend_frames -= take;
            produced        += take;
         }
         break;
      }
#endif
      case AUDIO_TYPE_WAV:
#ifdef HAVE_RWAV
      {
         struct audio_transfer_wav *w = (struct audio_transfer_wav*)data;
         size_t avail, want, ch, i;
         if (!w || !w->opened)
            return AUDIO_PROCESS_ERROR;
         ch    = (size_t)w->wav.numchannels;
         avail = w->wav.numsamples - w->cursor;
         want  = (frames < avail) ? frames : avail;
         if (w->wav.bitspersample == 16)
         {
            const int16_t *src = (const int16_t*)w->wav.samples;
            memcpy(out, src + w->cursor * ch, want * ch * sizeof(int16_t));
         }
         else /* 8-bit unsigned PCM -> signed 16-bit */
         {
            const uint8_t *src = (const uint8_t*)w->wav.samples;
            for (i = 0; i < want * ch; i++)
               out[i] = (int16_t)(((int)src[w->cursor * ch + i] - 128) << 8);
         }
         w->cursor += want;
         produced   = want;
         break;
      }
#endif
      case AUDIO_TYPE_NONE:
      default:
         return AUDIO_PROCESS_ERROR;
   }

   if (frames_out)
      *frames_out = produced;
   return (produced == 0) ? AUDIO_PROCESS_END : AUDIO_PROCESS_NEXT;
}

int audio_transfer_read_f32(void *data, enum audio_type_enum type,
      float *out, size_t frames, size_t *frames_out)
{
   size_t produced = 0;

   switch (type)
   {
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
      {
         struct audio_transfer_flac *fl = (struct audio_transfer_flac*)data;
         if (!fl || !fl->handle)
            return AUDIO_PROCESS_ERROR;
         produced = (size_t)rflac_read_pcm_frames_f32(
               fl->handle, (uint64_t)frames, out);
         break;
      }
#endif
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
      {
         struct audio_transfer_vorbis *v = (struct audio_transfer_vorbis*)data;
         int got;
         if (!v || !v->handle)
            return AUDIO_PROCESS_ERROR;
         got = rvorbis_get_samples_float_interleaved(v->handle, v->channels,
               out, (int)(frames * (size_t)v->channels));
         produced = (got > 0) ? (size_t)got : 0;
         break;
      }
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
      {
         struct audio_transfer_mp3 *m = (struct audio_transfer_mp3*)data;
         if (!m || !m->inited)
            return AUDIO_PROCESS_ERROR;
         produced = (size_t)rmp3_read_f32(&m->handle, (uint64_t)frames, out);
         break;
      }
#endif
#ifdef HAVE_RMODTRACKER
      case AUDIO_TYPE_MOD:
      {
         struct audio_transfer_mod *md = (struct audio_transfer_mod*)data;
         if (!md || !md->handle)
            return AUDIO_PROCESS_ERROR;
         produced = rmodtracker_get_samples_float_interleaved(
               md->handle, out, frames);
         break;
      }
#endif
      case AUDIO_TYPE_WAV:
#ifdef HAVE_RWAV
      {
         struct audio_transfer_wav *w = (struct audio_transfer_wav*)data;
         size_t avail, want, ch, i;
         if (!w || !w->opened)
            return AUDIO_PROCESS_ERROR;
         ch    = (size_t)w->wav.numchannels;
         avail = w->wav.numsamples - w->cursor;
         want  = (frames < avail) ? frames : avail;
         if (w->wav.bitspersample == 16)
         {
            const int16_t *src = (const int16_t*)w->wav.samples + w->cursor * ch;
            for (i = 0; i < want * ch; i++)
               out[i] = (float)src[i] * (1.0f / 32768.0f);
         }
         else /* 8-bit unsigned PCM */
         {
            const uint8_t *src = (const uint8_t*)w->wav.samples + w->cursor * ch;
            for (i = 0; i < want * ch; i++)
               out[i] = ((float)src[i] - 128.0f) * (1.0f / 128.0f);
         }
         w->cursor += want;
         produced   = want;
         break;
      }
#endif
#ifdef HAVE_ROPUS
      case AUDIO_TYPE_OPUS:
      {
         struct audio_transfer_opus *op = (struct audio_transfer_opus*)data;
         if (!op || !op->handle || op->fmt == 1)
            return AUDIO_PROCESS_ERROR;
         while (produced < frames)
         {
            size_t take;
            int r = audio_transfer_opus_fill(op, 2);
            if (r < 0)
               return AUDIO_PROCESS_ERROR;
            if (r == 0)
               break;
            take = frames - produced;
            if (take > op->pend_frames)
               take = op->pend_frames;
            memcpy(out + produced * op->channels,
                  op->pend_f32 + op->pend_pos * op->channels,
                  take * op->channels * sizeof(float));
            op->pend_pos    += take;
            op->pend_frames -= take;
            produced        += take;
         }
         break;
      }
#endif
#ifdef HAVE_RAAC
      case AUDIO_TYPE_AAC:
      {
         struct audio_transfer_aac *ac = (struct audio_transfer_aac*)data;
         if (!ac || !ac->handle || ac->fmt == 1)
            return AUDIO_PROCESS_ERROR;
         while (produced < frames)
         {
            size_t take;
            int r = audio_transfer_aac_fill(ac, 2);
            if (r < 0)
               return AUDIO_PROCESS_ERROR;
            if (r == 0)
               break;
            take = frames - produced;
            if (take > ac->pend_frames)
               take = ac->pend_frames;
            memcpy(out + produced * ac->channels,
                  ac->pend_f32 + ac->pend_pos * ac->channels,
                  take * ac->channels * sizeof(float));
            ac->pend_pos    += take;
            ac->pend_frames -= take;
            produced        += take;
         }
         break;
      }
#endif
      case AUDIO_TYPE_NONE:
      default:
         return AUDIO_PROCESS_ERROR;
   }

   if (frames_out)
      *frames_out = produced;
   return (produced == 0) ? AUDIO_PROCESS_END : AUDIO_PROCESS_NEXT;
}

bool audio_transfer_seek(void *data, enum audio_type_enum type,
      uint64_t frame)
{
   switch (type)
   {
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
      {
         struct audio_transfer_flac *fl = (struct audio_transfer_flac*)data;
         if (!fl || !fl->handle)
            return false;
         return rflac_seek_to_pcm_frame(fl->handle,
               (uint64_t)frame) != 0;
      }
#endif
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
      {
         struct audio_transfer_vorbis *v = (struct audio_transfer_vorbis*)data;
         if (!v || !v->handle)
            return false;
         if (frame == 0) /* loop-to-start: seek_start always succeeds */
         {
            rvorbis_seek_start(v->handle);
            return true;
         }
         return rvorbis_seek(v->handle, (unsigned int)frame) != 0;
      }
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
      {
         struct audio_transfer_mp3 *m = (struct audio_transfer_mp3*)data;
         if (!m || !m->inited)
            return false;
         return rmp3_seek_to_frame(&m->handle, (uint64_t)frame) != 0;
      }
#endif
#ifdef HAVE_RMODTRACKER
      case AUDIO_TYPE_MOD:
      {
         struct audio_transfer_mod *md = (struct audio_transfer_mod*)data;
         if (!md || !md->handle)
            return false;
         if (frame == 0) /* loop-to-start: rewind always succeeds */
         {
            rmodtracker_rewind(md->handle);
            return true;
         }
         return false;   /* trackers are sequenced; mid-song seek unsupported */
      }
#endif
      case AUDIO_TYPE_WAV:
#ifdef HAVE_RWAV
      {
         struct audio_transfer_wav *w = (struct audio_transfer_wav*)data;
         if (!w || !w->opened || frame > (uint64_t)w->wav.numsamples)
            return false;
         w->cursor = (size_t)frame;
         return true;
      }
#endif
#ifdef HAVE_ROPUS
      case AUDIO_TYPE_OPUS:
      {
         struct audio_transfer_opus *op = (struct audio_transfer_opus*)data;
         if (!op || !op->handle || frame != 0)
            return false;        /* rewind-only (used by looping)       */
         ropus_reset(op->handle);
#ifdef HAVE_RWEBM
         if (op->demux)
            rwebm_rewind(op->demux);
#endif
         op->pkt_index    = 0;
         op->pkt_offset   = 0;
         op->pg_off       = op->audio_off;
         op->seg_idx      = 0;
         op->body_off     = 0;
         op->emitted      = 0;
         op->pend_frames  = 0;
         op->pend_pos     = 0;
         op->preskip_left = ropus_preskip(op->handle);
         op->fmt          = 0;
         return true;
      }
#endif
#ifdef HAVE_RAAC
      case AUDIO_TYPE_AAC:
      {
         struct audio_transfer_aac *ac = (struct audio_transfer_aac*)data;
         if (!ac || !ac->handle || frame != 0)
            return false;        /* rewind-only (used by looping)       */
         raac_reset(ac->handle);
         ac->adts_pos = 0;
#ifdef HAVE_RMP4
         if (ac->demux)
            rmp4_rewind(ac->demux);
#endif
         ac->pkt_index   = 0;
         ac->pkt_offset  = 0;
         ac->pend_frames = 0;
         ac->pend_pos    = 0;
         ac->trim_left   = ac->start_trim;
         ac->fmt         = 0;
         return true;
      }
#endif
      case AUDIO_TYPE_NONE:
      default:
         break;
   }
   return false;
}

void audio_transfer_free(void *data, enum audio_type_enum type)
{
   if (!data)
      return;

   switch (type)
   {
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
      {
         struct audio_transfer_flac *fl = (struct audio_transfer_flac*)data;
         if (fl->handle)
            rflac_close(fl->handle);
         break;
      }
#endif
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
      {
         struct audio_transfer_vorbis *v = (struct audio_transfer_vorbis*)data;
         if (v->handle)
            rvorbis_close(v->handle);
         free(v->synth);
         free(v->own_pkts);
         free(v->own_sizes);
         break;
      }
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
      {
         struct audio_transfer_mp3 *m = (struct audio_transfer_mp3*)data;
         if (m->inited)
            rmp3_uninit(&m->handle);
         break;
      }
#endif
#ifdef HAVE_RMODTRACKER
      case AUDIO_TYPE_MOD:
      {
         struct audio_transfer_mod *md = (struct audio_transfer_mod*)data;
         if (md->handle)
            rmodtracker_close(md->handle);
         break;
      }
#endif
      case AUDIO_TYPE_WAV:
#ifdef HAVE_RWAV
      {
         struct audio_transfer_wav *w = (struct audio_transfer_wav*)data;
         if (w && w->opened)
            rwav_free(&w->wav);
         break;
      }
#endif
#ifdef HAVE_ROPUS
      case AUDIO_TYPE_OPUS:
      {
         struct audio_transfer_opus *op = (struct audio_transfer_opus*)data;
         if (op && op->handle)
            ropus_close(op->handle);
#ifdef HAVE_RWEBM
         if (op && op->demux)
            rwebm_close(op->demux);
#endif
         break;
      }
#endif
#ifdef HAVE_RAAC
      case AUDIO_TYPE_AAC:
      {
         struct audio_transfer_aac *ac = (struct audio_transfer_aac*)data;
         if (ac && ac->handle)
            raac_close(ac->handle);
#ifdef HAVE_RMP4
         if (ac && ac->demux)
            rmp4_close(ac->demux);
#endif
         break;
      }
#endif
      case AUDIO_TYPE_NONE:
      default:
         break;
   }

   free(data);
}
