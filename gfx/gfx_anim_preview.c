/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* Moved out of gfx/gfx_thumbnail.c so the desktop companions play a
 * preview exactly the way the menu's File Browser does. The bodies
 * below are gfx_thumbnail's, with its thumbnail-specific state replaced
 * by the session struct; the reasoning comments travel with them. */

#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <compat/strl.h>
#include <string/stdstring.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <formats/image.h>
#include <formats/data_transfer.h>
#include <memory/mem_stats.h>
#ifdef HAVE_RPNG
#include <formats/rpng.h>
#endif

#include "gfx_anim_preview.h"
#include "../configuration.h"

/* Preview audio: decode the animated thumbnail's audio track and loop
 * it through the audio mixer while the animation is shown. */
#if (defined(HAVE_RWEBM) || defined(HAVE_RMP4)) && \
      defined(HAVE_AUDIOMIXER) && \
      (defined(HAVE_ROPUS) || defined(HAVE_RVORBIS) || defined(HAVE_RAAC))
#define GFX_ANIM_PREVIEW_AUDIO 1
#include <audio/audio_mixer.h>
#include "../audio/audio_driver.h"
/* The mixer streams the audio track and decodes on the flush, so no
 * PCM is buffered up front and the clip is not length-capped. */
#define GFX_ANIM_PREVIEW_AUDIO_NAME   "__gfx_thumb_preview"
#define GFX_ANIM_PREVIEW_AUDIO_FEED_BUDGET (256 * 1024)
#define GFX_ANIM_PREVIEW_AUDIO_WINDOW_KEEP (2 * 1024 * 1024)
#define GFX_ANIM_PREVIEW_AUDIO_HEAD_SLACK  (1 * 1024 * 1024)
#define GFX_ANIM_PREVIEW_AUDIO_LOOKAHEAD   (2 * 1024 * 1024)
#define GFX_ANIM_PREVIEW_AUDIO_MARGIN      (1 * 1024 * 1024)
#define GFX_ANIM_PREVIEW_AUDIO_HEAD_MAX    (32 * 1024 * 1024)
#endif

/* --- memory admission ---------------------------------------------------- */

/* Memory an active animation pins: the file buffer for its lifetime,
 * two XRGB canvases, and decoder working state - for MP4/H.264 a
 * decoded-picture buffer of reference and reorder pictures, bounded
 * generously at 24 I420 frames. px may be 0 before the stream has
 * been opened (file buffer only). */
static uint64_t gfx_anim_preview_mem_need(uint64_t file_len, uint64_t px)
{
   return file_len + px * 4 * 3 + (px * 3 / 2) * 24 + (1 << 20);
}

/* Admission: scale with the heap when the platform reports free
 * memory, keep the static caps when it cannot (they return 0). */
static bool gfx_anim_preview_whole_ok(uint64_t file_len, uint64_t px)
{
   uint64_t free_mem = mem_stats_free();
   if (free_mem)
      return (file_len <= GFX_ANIM_PREVIEW_ABS_MAX_FILE)
          && (gfx_anim_preview_mem_need(file_len, px) <= free_mem / 4);
   return (file_len <= GFX_ANIM_PREVIEW_MAX_FILE)
       && (px == 0 || px <= GFX_ANIM_PREVIEW_MAX_PIXELS);
}

/* Admission for the windowed path: a windowed open commits only the
 * head plus the sliding window, never the whole file, so charge that
 * and the decoder's own buffers. */
static bool gfx_anim_preview_window_ok(uint64_t px)
{
   uint64_t free_mem = mem_stats_free();
   uint64_t win      = (uint64_t)GFX_ANIM_PREVIEW_WINDOW_KEEP
                     + GFX_ANIM_PREVIEW_WINDOW_AHEAD
                     + GFX_ANIM_PREVIEW_WINDOW_BACK;
   if (free_mem)
      return gfx_anim_preview_mem_need(win, px) <= free_mem / 4;
   return (px == 0 || px <= GFX_ANIM_PREVIEW_MAX_PIXELS);
}

bool gfx_anim_preview_admit(uint64_t charge, uint64_t px)
{
   return gfx_anim_preview_whole_ok(charge, px);
}

bool gfx_anim_preview_window_admit(uint64_t px)
{
   return gfx_anim_preview_window_ok(px);
}

bool gfx_anim_preview_mem_ok(gfx_anim_preview_t *p, uint64_t px)
{
   if (!p)
      return false;
   return p->windowed ? gfx_anim_preview_window_ok(px)
                      : gfx_anim_preview_whole_ok((uint64_t)p->len, px);
}

/* --- open ------------------------------------------------------------------ */

gfx_anim_preview_t *gfx_anim_preview_open(const char *path, int png_probe)
{
   enum image_type_enum type;
   gfx_anim_preview_t *p;
   data_transfer_t *dt;
   const uint8_t *base;
   size_t blen = 0;
   bool reserved;
   void *stream = NULL;

   if (string_is_empty(path))
      return NULL;

   /* Cheap gate: only container types with an animation decoder.
    * PNG is included for APNG. */
   type = image_texture_get_type(path);
   if (   (type != IMAGE_TYPE_PNG)
       && (type != IMAGE_TYPE_WEBP)
       && (type != IMAGE_TYPE_WEBM)
       && (type != IMAGE_TYPE_MP4))
      return NULL;

#ifdef HAVE_RPNG
   /* PNG is the dominant thumbnail format and almost all of them are
    * still images, so deciding "animated?" only after reading the whole
    * file would add a full extra read to the common path.  APNG puts
    * its acTL control chunk before the first IDAT, i.e. within the
    * first few hundred bytes, so probe a small header window first and
    * bail out early for ordinary PNGs.  A caller with a verdict from a
    * still decode that held the whole file passes it and skips the
    * probe. */
   if (type == IMAGE_TYPE_PNG && png_probe == 0)
      return NULL;
   if (type == IMAGE_TYPE_PNG && png_probe < 0)
   {
      uint8_t *probe;
      int64_t  got  = 0;
      int      more = 0;
      RFILE   *fp   = filestream_open(path,
            RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
      if (!fp)
         return NULL;
      if (!(probe = (uint8_t*)malloc(4096)))
      {
         filestream_close(fp);
         return NULL;
      }
      got = filestream_read(fp, probe, 4096);
      filestream_close(fp);
      if (got <= 0)
      {
         free(probe);
         return NULL;
      }
      if (!rpng_is_apng_ex(probe, (size_t)got, &more))
      {
         (void)more;
         free(probe);
         return NULL;
      }
      free(probe);
   }
#else
   (void)png_probe;
#endif

   /* Open the file as a sliding window: address space for the whole
    * file is reserved, but only [tell - margin, tell + lookahead) is
    * ever committed, so a long video costs its window rather than its
    * length.  On a platform with no address-space reservation
    * data_transfer_open_window fills the whole file before returning,
    * so the absolute bound is applied BEFORE the open there. */
   if (!data_transfer_reserve_supported())
   {
      int64_t fsz = path_get_size(path);
      if (fsz <= 0 || fsz > GFX_ANIM_PREVIEW_ABS_MAX_FILE)
         return NULL;
   }

   if (!(dt = data_transfer_open_window(path, GFX_ANIM_PREVIEW_WINDOW_KEEP)))
      return NULL;
   reserved = data_transfer_window_is_reserved(dt);
   if (!(base = data_transfer_window_base(dt, &blen)) || blen == 0)
   {
      data_transfer_free(dt);
      return NULL;
   }
   if (   (!reserved && blen > GFX_ANIM_PREVIEW_ABS_MAX_FILE)
       || !(reserved ? gfx_anim_preview_window_ok(0)
                     : gfx_anim_preview_whole_ok((uint64_t)blen, 0)))
   {
      data_transfer_free(dt);
      return NULL;
   }

   /* The demuxer must see the whole logical length; only the head is
    * resident at this point, so open progressively and grow the window
    * until the header/index is covered. */
   {
      int need_more = 0;
      size_t need_lo = 0, need_hi = 0;
      size_t avail  = GFX_ANIM_PREVIEW_WINDOW_KEEP;
      int    jumps  = 0;
      if (avail > blen)
         avail = blen;
      for (;;)
      {
         stream = image_transfer_anim_stream_new_avail(
               (void*)base, blen, avail, type, &need_more,
               &need_lo, &need_hi);
         if (stream || !need_more || avail >= blen)
            break;
         if (need_hi > need_lo && need_hi > avail && need_hi <= blen)
         {
            /* The demuxer named the exact bytes that unblock it (a box
             * header past the wall, or a trailing moov body): commit
             * just that island. */
            if (++jumps > 64 ||
                !data_transfer_window_ensure(dt, need_lo, need_hi))
               break;
            avail = need_hi;
            continue;
         }
         avail += GFX_ANIM_PREVIEW_WINDOW_KEEP;
         if (avail > blen)
            avail = blen;
         if (!data_transfer_window_extend(dt, avail))
            break;
      }
      if (stream)
      {
         /* Restart the read frontier at the media floor and prime one
          * window so the first frames decode from resident bytes. */
         size_t fl = image_transfer_anim_stream_media_floor(stream, type);
         size_t hi = fl + GFX_ANIM_PREVIEW_WINDOW_KEEP
               + GFX_ANIM_PREVIEW_WINDOW_AHEAD;
         if (hi > blen)
            hi = blen;
         data_transfer_window_rebase(dt, fl);
         if (!data_transfer_window_extend(dt, hi))
         {
            image_transfer_anim_stream_free(stream, type);
            stream = NULL;
         }
         else
            image_transfer_anim_stream_set_avail(stream, type, hi);
      }
      /* Types without a progressive open (animated WEBP) return NULL
       * with need_more clear: fall back to the whole buffer. */
      if (!stream && !need_more)
      {
         if (data_transfer_window_extend(dt, blen))
            stream = image_transfer_anim_stream_new((void*)base, blen, type);
      }
   }

   if (!stream)
   {
      data_transfer_free(dt);
      return NULL;             /* still image or malformed */
   }

   if (!(p = (gfx_anim_preview_t*)calloc(1, sizeof(*p))))
   {
      image_transfer_anim_stream_free(stream, type);
      data_transfer_free(dt);
      return NULL;
   }
   p->stream     = stream;
   p->dt         = dt;
   p->base       = base;
   p->len        = blen;
   p->type       = type;
   p->windowed   = reserved;
   p->audio_slot = -1;
   p->path       = strldup(path, strlen(path) + 1);
   image_transfer_anim_stream_get_info(stream, type, &p->width, &p->height,
         &p->num_frames, &p->loop_count);
   /* Ask for ARGB once, before any frame: the answer holds for the
    * animation's life (a repeat ask after the first frame is refused by
    * APNG even though the order stands - deciding per frame here once
    * swapped R and B from the second frame on). */
   p->native_argb = image_transfer_anim_stream_set_argb(stream, type, 1);
   if (p->num_frames < 2 && type != IMAGE_TYPE_WEBM && type != IMAGE_TYPE_MP4)
   {
      /* one frame is a still */
      gfx_anim_preview_close(p);
      return NULL;
   }
   return p;
}

/* --- feeding --------------------------------------------------------------- */

bool gfx_anim_preview_feed(gfx_anim_preview_t *p)
{
   size_t tell, floor_off, anchor;
   if (!p || !p->stream)
      return false;
   if (!p->windowed || !p->dt)
      return true;

   /* Keep the committed range straddling the decoder's byte frontier.
    * The demuxers report a monotonic consumed offset and a fixed media
    * floor, so the feeder can decommit behind and commit ahead without
    * touching the range the decoder is reading; the margin exceeds the
    * largest single frame's packet by orders of magnitude. Anchor at
    * the media floor until the decoder has consumed anything. */
   tell      = image_transfer_anim_stream_consumed(p->stream, p->type);
   floor_off = image_transfer_anim_stream_media_floor(p->stream, p->type);
   anchor    = (tell > 0) ? tell : floor_off;
   if (anchor > 0)
   {
      size_t margin = GFX_ANIM_PREVIEW_WINDOW_BACK;
      size_t hi     = anchor + GFX_ANIM_PREVIEW_WINDOW_AHEAD;
      size_t res_hi = 0;
      if (anchor > floor_off && anchor - floor_off < margin)
         margin = anchor - floor_off;
      if (!data_transfer_window_feed_budget(p->dt, anchor,
               GFX_ANIM_PREVIEW_WINDOW_AHEAD, margin,
               GFX_ANIM_PREVIEW_FEED_BUDGET, &res_hi))
         return false;
      /* The demuxer's bound follows what the feed made resident, both
       * ways (a loop's rewind drops the frontier back to the head). */
      if (hi > res_hi)
         hi = res_hi;
      if (hi > p->len)
         hi = p->len;
      image_transfer_anim_stream_set_avail(p->stream, p->type, hi);
   }
   return true;
}

const uint32_t *gfx_anim_preview_next(gfx_anim_preview_t *p,
      int *duration_ms, bool *native_argb)
{
   const uint32_t *frame;
   int d = 0;
   if (!p || !p->stream)
      return NULL;
   frame = image_transfer_anim_stream_next(p->stream, p->type, &d);
   if (native_argb)
      *native_argb = p->native_argb;
   /* <= 0 is undefined by the container spec (browsers substitute
    * 100 ms); very small durations are floored so a hostile file cannot
    * request thousands of decodes per second. */
   if (d <= 0)
      d = GFX_ANIM_PREVIEW_DUR_DEFAULT;
   if (d < GFX_ANIM_PREVIEW_DUR_MIN)
      d = GFX_ANIM_PREVIEW_DUR_MIN;
   if (duration_ms)
      *duration_ms = d;
   return frame;
}

void gfx_anim_preview_rewind(gfx_anim_preview_t *p)
{
   if (p && p->stream)
      image_transfer_anim_stream_rewind(p->stream, p->type);
}

/* --- audio ------------------------------------------------------------------ */

#if defined(GFX_ANIM_PREVIEW_AUDIO)
typedef struct
{
   struct data_transfer *dt;
} gfx_anim_preview_audio_win_t;

static void gfx_anim_preview_audio_win_release(void *owner)
{
   gfx_anim_preview_audio_win_t *w = (gfx_anim_preview_audio_win_t*)owner;
   if (!w)
      return;
   if (w->dt)
      data_transfer_free(w->dt);
   free(w);
}

/* End offset of the MP4 moov box within the head, or 0. */
static size_t gfx_anim_preview_audio_meta_end(struct data_transfer *dt,
      size_t blen, size_t *lo_out)
{
   size_t pos = 0;
   int    n   = 0;
   while (pos + 8 <= blen && n++ < 64)
   {
      uint8_t  h[16];
      uint64_t sz;
      size_t   hs = 8;
      if (!data_transfer_window_peek(dt, pos, h, 8))
         return 0;
      sz = ((uint64_t)h[0] << 24) | ((uint64_t)h[1] << 16)
         | ((uint64_t)h[2] << 8)  |  (uint64_t)h[3];
      if (sz == 1)
      {
         int i;
         if (!data_transfer_window_peek(dt, pos + 8, h + 8, 8))
            return 0;
         sz = 0;
         for (i = 0; i < 8; i++)
            sz = (sz << 8) | (uint64_t)h[8 + i];
         hs = 16;
      }
      else if (sz == 0)
         sz = (uint64_t)blen - (uint64_t)pos;
      if (sz < hs || sz > (uint64_t)blen - (uint64_t)pos)
         return 0;
      if (h[4] == 'm' && h[5] == 'o' && h[6] == 'o' && h[7] == 'v')
      {
         if (lo_out)
            *lo_out = pos;
         return (size_t)((uint64_t)pos + sz);
      }
      pos += (size_t)sz;
   }
   return 0;
}

static void gfx_anim_preview_audio_stop_slot(gfx_anim_preview_t *p)
{
   if (p->audio_slot >= 0)
   {
      /* Only touch the slot if it still holds our stream. */
      const char *name = audio_driver_mixer_get_stream_name((unsigned)p->audio_slot);
      if (name && string_is_equal(name, GFX_ANIM_PREVIEW_AUDIO_NAME))
         audio_driver_mixer_remove_stream((unsigned)p->audio_slot);
   }
   /* the mixer owns (and frees) the audio window through buf_owner */
   p->audio_dt   = NULL;
   p->audio_hi   = 0;
   p->audio_slot = -1;
}
#endif

void gfx_anim_preview_audio_begin(gfx_anim_preview_t *p)
{
#if defined(GFX_ANIM_PREVIEW_AUDIO)
   gfx_anim_preview_audio_win_t *w;
   const uint8_t *base = NULL;
   size_t blen = 0, floor_off, keep, island_hi = 0;

   if (!p || !p->stream || !p->path || !*p->path)
      return;
   if (   (p->type != IMAGE_TYPE_WEBM && p->type != IMAGE_TYPE_MP4)
       || !config_get_ptr()->bools.menu_thumbnail_preview_audio)
      return;
   if (p->audio_dt)
      return;                   /* already streaming */

   /* A second window over the same file, independent of the video one:
    * the two decoders run on different threads and each needs its own
    * committed range. The head must permanently cover the container's
    * metadata (the AAC arm borrows codec_private out of the moov for
    * the decoder's whole life); the video open already found where the
    * media starts. */
   floor_off = image_transfer_anim_stream_media_floor(p->stream, p->type);
   keep      = floor_off + GFX_ANIM_PREVIEW_AUDIO_HEAD_SLACK;
   if (keep < GFX_ANIM_PREVIEW_AUDIO_WINDOW_KEEP)
      keep = GFX_ANIM_PREVIEW_AUDIO_WINDOW_KEEP;
   if (keep > GFX_ANIM_PREVIEW_AUDIO_HEAD_MAX)
      keep = GFX_ANIM_PREVIEW_AUDIO_HEAD_MAX;

   if (!(w = (gfx_anim_preview_audio_win_t*)calloc(1, sizeof(*w))))
      return;
   if (!(w->dt = data_transfer_open_window(p->path, keep)))
   {
      free(w);
      return;
   }
   if (p->type == IMAGE_TYPE_MP4)
   {
      size_t probe_len = 0;
      if (data_transfer_window_base(w->dt, &probe_len) && probe_len)
      {
         size_t meta_lo  = 0;
         size_t meta_end = gfx_anim_preview_audio_meta_end(w->dt, probe_len, &meta_lo);
         if (!meta_end)
         {  /* no moov found: leave the floor-derived head */ }
         else if (meta_end <= GFX_ANIM_PREVIEW_AUDIO_HEAD_MAX)
         {
            size_t need = meta_end;
            if (floor_off + GFX_ANIM_PREVIEW_AUDIO_HEAD_SLACK > need)
               need = floor_off + GFX_ANIM_PREVIEW_AUDIO_HEAD_SLACK;
            if (need < GFX_ANIM_PREVIEW_AUDIO_WINDOW_KEEP)
               need = GFX_ANIM_PREVIEW_AUDIO_WINDOW_KEEP;
            if (need > probe_len)
               need = probe_len;
            if (need > GFX_ANIM_PREVIEW_AUDIO_HEAD_MAX)
            {
               need      = meta_end;
               island_hi = meta_end;
               if (need < GFX_ANIM_PREVIEW_AUDIO_WINDOW_KEEP)
                  need = GFX_ANIM_PREVIEW_AUDIO_WINDOW_KEEP;
            }
            if (need > keep)
            {
               if (!data_transfer_window_grow_keep(w->dt, need))
               {
                  gfx_anim_preview_audio_win_release(w);
                  return;
               }
               keep = need;
            }
         }
         else
         {
            if (!data_transfer_window_ensure(w->dt, meta_lo, meta_end))
            {
               gfx_anim_preview_audio_win_release(w);
               return;
            }
            island_hi = meta_end;
         }
      }
   }
   if (!(base = data_transfer_window_base(w->dt, &blen)) || !blen)
   {
      gfx_anim_preview_audio_win_release(w);
      return;
   }
   p->audio_hi = island_hi ? blen : ((keep < blen) ? keep : blen);

   {
      audio_mixer_stream_params_t params;
      int out_slot = -1;
      params.buf                 = (void*)base;
      params.bufsize             = blen;
      params.basename            = strldup(GFX_ANIM_PREVIEW_AUDIO_NAME,
            strlen(GFX_ANIM_PREVIEW_AUDIO_NAME) + 1);
      params.cb                  = NULL;
      params.buf_owner           = w;
      params.buf_owner_free      = gfx_anim_preview_audio_win_release;
      params.out_slot            = &out_slot;
      params.slot_selection_idx  = 0;
      params.volume              = 1.0f;
      params.slot_selection_type = AUDIO_MIXER_SLOT_SELECTION_AUTOMATIC;
      params.stream_type         = AUDIO_STREAM_TYPE_SYSTEM;
      params.type                = (p->type == IMAGE_TYPE_WEBM)
                                    ? AUDIO_MIXER_TYPE_WEBA
                                    : AUDIO_MIXER_TYPE_M4A;
      params.state               = AUDIO_STREAM_STATE_PLAYING_LOOPED;
      params.end_granule         = 0;
      params.avail               = p->audio_hi;
      if (!audio_driver_mixer_add_stream(&params))
      {
         free(params.basename);
         p->audio_hi = 0;
         /* the mixer did not take the window: release it ourselves */
         gfx_anim_preview_audio_win_release(w);
         return;
      }
      free(params.basename);
      p->audio_dt   = w->dt;
      p->audio_slot = out_slot;
      if (island_hi)
      {
         p->audio_hi = (keep < blen) ? keep : blen;
         audio_driver_mixer_stream_set_avail((unsigned)out_slot, p->audio_hi);
      }
   }
#else
   (void)p;
#endif
}

bool gfx_anim_preview_audio_feed(gfx_anim_preview_t *p)
{
#if defined(GFX_ANIM_PREVIEW_AUDIO)
   int64_t tell;
   if (!p || !p->audio_dt || p->audio_slot < 0)
      return true;
   /* The decoder runs on the audio thread out of this mapping: feed
    * first, then raise the bound to what the feed actually committed. */
   tell = audio_driver_mixer_stream_byte_tell((unsigned)p->audio_slot);
   if (tell >= 0)
   {
      size_t anchor = (size_t)tell;
      size_t hi     = anchor + GFX_ANIM_PREVIEW_AUDIO_LOOKAHEAD;
      size_t res_hi = 0;
      if (hi > p->len)
         hi = p->len;
      if (!data_transfer_window_feed_budget(p->audio_dt, anchor,
               GFX_ANIM_PREVIEW_AUDIO_LOOKAHEAD, GFX_ANIM_PREVIEW_AUDIO_MARGIN,
               GFX_ANIM_PREVIEW_AUDIO_FEED_BUDGET, &res_hi))
      {
         /* the window cannot be maintained: stop rather than play
          * against a frozen one */
         gfx_anim_preview_audio_stop_slot(p);
         return false;
      }
      if (hi > res_hi)
         hi = res_hi;
      if (hi > p->audio_hi)
      {
         p->audio_hi = hi;
         audio_driver_mixer_stream_set_avail((unsigned)p->audio_slot, hi);
      }
   }
   return true;
#else
   (void)p;
   return true;
#endif
}

void gfx_anim_preview_audio_stop(gfx_anim_preview_t *p)
{
#if defined(GFX_ANIM_PREVIEW_AUDIO)
   if (p)
      gfx_anim_preview_audio_stop_slot(p);
#else
   (void)p;
#endif
}

/* --- wrap / release (non-owning) -------------------------------------------- */

gfx_anim_preview_t *gfx_anim_preview_wrap(void *stream,
      enum image_type_enum type, struct data_transfer *dt,
      const uint8_t *base, size_t len, bool windowed, const char *path)
{
   gfx_anim_preview_t *p;
   if (!stream)
      return NULL;
   if (!(p = (gfx_anim_preview_t*)calloc(1, sizeof(*p))))
      return NULL;
   p->stream     = stream;
   p->dt         = dt;
   p->base       = base;
   p->len        = len;
   p->type       = type;
   p->windowed   = windowed;
   p->audio_slot = -1;
   p->path       = path ? strldup(path, strlen(path) + 1) : NULL;
   image_transfer_anim_stream_get_info(stream, type, &p->width, &p->height,
         &p->num_frames, &p->loop_count);
   /* gfx_thumbnail's worker asks the stream itself per job; the answer
    * recorded here is for callers that draw through the session. */
   p->native_argb = image_transfer_anim_stream_set_argb(stream, type, 1);
   return p;
}

void gfx_anim_preview_release(gfx_anim_preview_t *p)
{
   if (!p)
      return;
   gfx_anim_preview_audio_stop(p);
   free(p->path);
   free(p);
}

/* --- close ------------------------------------------------------------------ */

void gfx_anim_preview_close(gfx_anim_preview_t *p)
{
   if (!p)
      return;
   gfx_anim_preview_audio_stop(p);
   if (p->stream)
      image_transfer_anim_stream_free(p->stream, p->type);
   if (p->dt)
      data_transfer_free(p->dt);
   free(p->path);
   free(p);
}
