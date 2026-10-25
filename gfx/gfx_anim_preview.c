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
#include <formats/video_player.h>
#include <retro_atomic.h>
#include <features/features_cpu.h>
#include <memory/mem_stats.h>
#ifdef HAVE_RPNG
#include <formats/rpng.h>
#endif
#ifdef HAVE_RWEBP
#include <formats/rwebp.h>
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
#include <formats/audio.h>
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

static void gfx_anim_preview_size_feed(gfx_anim_preview_t *p);
static void gfx_anim_preview_feed_geometry(void *stream,
      enum image_type_enum type, size_t len, size_t *ahead_out,
      size_t *back_out);

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

/* --- probe ----------------------------------------------------------------- */

int gfx_anim_preview_probe(const char *path)
{
   enum image_type_enum type;
   uint8_t *head;
   int64_t got;
   int more = 0, result = 0;
   RFILE *fp;

   if (string_is_empty(path))
      return -1;
   type = image_texture_get_type(path);
   if (type == IMAGE_TYPE_WEBM || type == IMAGE_TYPE_MP4)
      return 1;
   if (type != IMAGE_TYPE_PNG && type != IMAGE_TYPE_WEBP)
      return 0;
   if (!(fp = filestream_open(path,
         RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE)))
      return -1;
   head = (uint8_t*)malloc(type == IMAGE_TYPE_PNG ? 4096 : 32);
   if (!head)
   {
      filestream_close(fp);
      return -1;
   }
   got = filestream_read(fp, head, (type == IMAGE_TYPE_PNG) ? 4096 : 32);
   filestream_close(fp);
   if (got <= 0)
   {
      free(head);
      return -1;
   }
#ifdef HAVE_RPNG
   if (type == IMAGE_TYPE_PNG)
      result = rpng_is_apng_ex(head, (size_t)got, &more) ? 1 : 0;
#endif
#ifdef HAVE_RWEBP
   if (type == IMAGE_TYPE_WEBP)
      result = rwebp_is_animated_ex(head, (size_t)got, &more) ? 1 : 0;
#endif
   (void)more;
   free(head);
   return result;
}

/* --- open ------------------------------------------------------------------ */

static gfx_anim_preview_t *gfx_anim_preview_open_internal(const char *path,
      int png_probe, bool hardware);

gfx_anim_preview_t *gfx_anim_preview_open(const char *path, int png_probe)
{
   return gfx_anim_preview_open_internal(path, png_probe, false);
}

gfx_anim_preview_t *gfx_anim_preview_open_hw(const char *path, int png_probe)
{
   return gfx_anim_preview_open_internal(path, png_probe, true);
}

static gfx_anim_preview_t *gfx_anim_preview_open_internal(const char *path,
      int png_probe, bool hardware)
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
         if (hardware)
            stream = image_transfer_anim_stream_new_hw_deferred_avail(
                  (void*)base, blen, avail, type, &need_more,
                  &need_lo, &need_hi);
         else
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
          * window - the lookahead the feed will keep, sized to the
          * content - so the first frames decode from resident bytes. */
         size_t fl = image_transfer_anim_stream_media_floor(stream, type);
         size_t ahead, back, hi;
         gfx_anim_preview_feed_geometry(stream, type, blen, &ahead, &back);
         hi = fl + ahead;
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
      /* NULL with need_more clear is conclusive for every streaming
       * type (a still, or malformed): no whole-buffer retry.  There
       * used to be one for animated WEBP, which had no partial open;
       * it read the entire file into the reservation - for every
       * still WEBP thumbnail too, only to learn it was a still - and
       * then played the animation from a fully resident buffer with
       * the windowed flag still set, so admission charged one window
       * for a whole file. */
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
   p->seekable   = image_transfer_anim_stream_seekable(stream, type);
   /* Movie time starts at the container origin, even when audio
    * precedes its first video picture. The player schedules that gap. */
   p->timeline_start_us = 0;
   p->windowed   = reserved;
   p->audio_slot = -1;
   p->path       = strldup(path, strlen(path) + 1);
   {
      /* The stream fills the axes separately; they are one word here. */
      unsigned sw = 0;
      unsigned sh = 0;
      image_transfer_anim_stream_get_info(stream, type, &sw, &sh,
            &p->num_frames, &p->loop_count);
      p->dims    = VIDEO_SCALE_PACK(sw, sh);
   }
   /* Ask for ARGB once, before any frame: the answer holds for the
    * animation's life (a repeat ask after the first frame is refused by
    * APNG even though the order stands - deciding per frame here once
    * swapped R and B from the second frame on). */
   p->native_argb = image_transfer_anim_stream_set_argb(stream, type, 1);
   gfx_anim_preview_size_feed(p);
   if (p->num_frames < 2 && type != IMAGE_TYPE_WEBM && type != IMAGE_TYPE_MP4)
   {
      /* one frame is a still */
      gfx_anim_preview_close(p);
      return NULL;
   }
   return p;
}

/* Size the moving window to the content. A fixed 8 MiB ahead and
 * 8 MiB behind is 13 s each way of a 5 Mbit/s trailer, resident for
 * nothing; at the bitrate two seconds ahead and half a second back
 * is what a per-tick refill needs, and a low-bitrate file gets the
 * floor. The frame-indexed types name no duration and keep the
 * ceilings. */
static void gfx_anim_preview_feed_geometry(void *stream,
      enum image_type_enum type, size_t len, size_t *ahead_out,
      size_t *back_out)
{
   int64_t dur_ns;
   *ahead_out = GFX_ANIM_PREVIEW_WINDOW_AHEAD;
   *back_out  = GFX_ANIM_PREVIEW_WINDOW_BACK;
   dur_ns = image_transfer_anim_stream_duration_ns(stream, type);
   if (dur_ns > 0 && len > 0)
   {
      /* bytes per second, rounded up; 64-bit throughout */
      uint64_t bps   = ((uint64_t)len * 1000000000ull + (uint64_t)dur_ns - 1)
                     / (uint64_t)dur_ns;
      uint64_t ahead = bps * GFX_ANIM_PREVIEW_FEED_AHEAD_SECS;
      uint64_t back  = bps * GFX_ANIM_PREVIEW_FEED_BACK_MSECS / 1000;
      if (ahead < GFX_ANIM_PREVIEW_FEED_AHEAD_MIN) ahead = GFX_ANIM_PREVIEW_FEED_AHEAD_MIN;
      if (back  < GFX_ANIM_PREVIEW_FEED_BACK_MIN)  back  = GFX_ANIM_PREVIEW_FEED_BACK_MIN;
      if (ahead < GFX_ANIM_PREVIEW_WINDOW_AHEAD)   *ahead_out = (size_t)ahead;
      if (back  < GFX_ANIM_PREVIEW_WINDOW_BACK)    *back_out  = (size_t)back;
   }
}

static void gfx_anim_preview_size_feed(gfx_anim_preview_t *p)
{
   gfx_anim_preview_feed_geometry(p->stream, p->type, p->len,
         &p->feed_ahead, &p->feed_back);
}

/* --- feeding --------------------------------------------------------------- */

/* The feeder is the decode's thread; this is anyone's. The window's
 * extent is read by the feeder after each feed and kept where any
 * thread may read it, rather than the window's own bookkeeping being
 * read while the feeder moves it. */
size_t gfx_anim_preview_resident_bytes(const gfx_anim_preview_t *p)
{
   if (!p)
      return 0;
   if (!p->windowed || !p->dt)
      return p->len;
   return (size_t)retro_atomic_load_acquire_int(&p->resident_seen);
}

bool gfx_anim_preview_feed(gfx_anim_preview_t *p)
{
   size_t tell, floor_off, anchor, pin_lo = 0, pin_hi = 0;
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
   image_transfer_anim_stream_pin_span(p->stream, p->type, &pin_lo, &pin_hi);
   if (!data_transfer_window_pin(p->dt, pin_lo, pin_hi)) return false;
   tell      = image_transfer_anim_stream_feed_frontier(p->stream, p->type);
   floor_off = image_transfer_anim_stream_media_floor(p->stream, p->type);
   anchor    = (tell > 0) ? tell : floor_off;
   if (anchor > 0)
   {
      size_t margin = p->feed_back;
      size_t ahead  = p->feed_ahead;
      size_t budget = GFX_ANIM_PREVIEW_FEED_BUDGET;
      size_t hi;
      size_t res_hi = 0;
      size_t span_lo = 0, span_hi = 0;
      bool demand_jump = false;
      if (p->type == IMAGE_TYPE_WEBM && (anchor > p->feed_res_hi
            || anchor < p->feed_tell))
      {
         data_transfer_window_rewind(p->dt);
         data_transfer_window_rebase(p->dt, anchor);
         p->feed_tell = p->feed_res_hi = anchor;
         demand_jump = true;
      }
      if (anchor > floor_off && anchor - floor_off < margin)
         margin = anchor - floor_off;
      /* A frame larger than the lookahead (a lossless 4K WEBP or APNG
       * frame can be tens of MB) would never fit below the wall: the
       * decoder refuses it, consumed never moves, and the paced feed
       * extends to the same short target every tick.  The indexing
       * types name the next frame's span; cover it in one unpaced
       * extend, exactly the burst a lap or an open already pays. */
      image_transfer_anim_stream_next_span(p->stream, p->type,
            &span_lo, &span_hi);
      if (span_hi > span_lo && span_hi > anchor && span_hi - anchor > ahead)
      {
         ahead  = span_hi - anchor;
         /* Movie packets exceeding lookahead still use bounded reads. */
         budget = (p->type == IMAGE_TYPE_WEBM || p->type == IMAGE_TYPE_MP4)
            ? GFX_ANIM_PREVIEW_FEED_BUDGET : 0;
      }
      /* The paced budget is sized for a decoder that takes one sample
       * a tick. One catching up takes three or four - it passes the
       * pictures nothing references over - and a fast one takes them
       * at whatever rate it decodes; either can eat through the
       * lookahead faster than half a megabyte a tick refills it, and
       * then it stands at the wall for the forty ticks the refill
       * takes. So the pacing holds only while the feed is comfortably
       * ahead: once the resident lookahead is below half its target,
       * the tick extends unpaced, the burst a lap or an open already
       * pays, and the decoder never sees the wall. */
      else
      {
         /* The budget follows the decoder's appetite: at least twice
          * what it took since the last feed, so a decoder taking four
          * samples a tick - pictures decoding concurrently take them
          * in bursts - is kept ahead of without the lookahead ever
          * running down. And when it has run down, the shortfall is
          * made up over a few ticks rather than in one read: a burst
          * of two seconds of 4K from disk on the decoding thread is
          * itself the pause it was meant to prevent. Only a decoder
          * already past the frontier gets the unpaced extend, since
          * its next read faults otherwise. */
         size_t took = (anchor > p->feed_tell) ? anchor - p->feed_tell : 0;
         /* file-derived sizes: saturate rather than wrap, or a
          * decoder that took past half the address space would be
          * budgeted next to nothing */
         size_t twice = (took > (size_t)-1 / 2) ? (size_t)-1 : took * 2;
         if (budget < twice)
            budget = twice;
         if (p->feed_res_hi > anchor && p->feed_res_hi - anchor < ahead / 2)
         {
            size_t shortfall = ahead - (p->feed_res_hi - anchor);
            if (budget < shortfall / 4)
               budget = shortfall / 4;
         }
         else if (p->feed_res_hi <= anchor)
            budget = 0;
      }
      if (demand_jump) budget = GFX_ANIM_PREVIEW_FEED_BUDGET;
      p->feed_tell = anchor;
      hi = (ahead > (size_t)-1 - anchor) ? (size_t)-1 : anchor + ahead;
      if (!data_transfer_window_feed_budget(p->dt, anchor,
               ahead, margin, budget, &res_hi))
         return false;
      p->feed_res_hi = res_hi;
      {
         size_t r = data_transfer_window_resident(p->dt);
         retro_atomic_store_release_int(&p->resident_seen,
               r > (size_t)0x7fffffff ? 0x7fffffff : (int)r);
      }
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
   retro_atomic_int_t refs; /* session and mixer each retain the window */
} gfx_anim_preview_audio_win_t;

static void gfx_anim_preview_audio_win_release(void *owner)
{
   gfx_anim_preview_audio_win_t *w = (gfx_anim_preview_audio_win_t*)owner;
   if (!w)
      return;
   if (retro_atomic_fetch_sub_int(&w->refs, 1) != 1)
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
      if (p->audio_voice)
         audio_driver_mixer_remove_stream_clocked((unsigned)p->audio_slot,
               p->audio_voice, p->audio_serial);
      else
      {
         /* Non-clocked stand-ins and a stream that created no voice. */
         const char *name = audio_driver_mixer_get_stream_name((unsigned)p->audio_slot);
         if (name && string_is_equal(name, GFX_ANIM_PREVIEW_AUDIO_NAME))
            audio_driver_mixer_remove_stream((unsigned)p->audio_slot);
      }
   }
   gfx_anim_preview_audio_win_release(p->audio_owner);
   p->audio_owner = NULL;
   p->audio_voice = NULL;
   p->audio_serial = 0;
   p->audio_dt   = NULL;
   p->audio_hi   = 0;
   p->audio_tell = 0;
   p->audio_slot = -1;
}

static size_t gfx_anim_preview_audio_lookahead(const gfx_anim_preview_t *p)
{
   /* Audio demuxing also walks the interleaved video packets. */
   return p->feed_ahead > GFX_ANIM_PREVIEW_AUDIO_LOOKAHEAD
      ? p->feed_ahead : GFX_ANIM_PREVIEW_AUDIO_LOOKAHEAD;
}
/* Exclusive-owner source open shared by initial playback and worker
 * preparation. The sound and session each retain their own window ref. */
static gfx_anim_preview_audio_win_t *gfx_anim_preview_audio_source(
      const gfx_anim_preview_t *p, size_t *initial_hi, size_t *feed_hi)
{
   gfx_anim_preview_audio_win_t *w;
   const uint8_t *base;
   size_t blen, floor_off, keep, island_hi = 0, prime_hi, ahead;
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
      return NULL;
   retro_atomic_store_relaxed_int(&w->refs, 1);
   if (!(w->dt = data_transfer_open_window(p->path, keep)))
   {
      free(w);
      return NULL;
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
                  return NULL;
               }
               keep = need;
            }
         }
         else
         {
            if (!data_transfer_window_ensure(w->dt, meta_lo, meta_end))
            {
               gfx_anim_preview_audio_win_release(w);
               return NULL;
            }
            island_hi = meta_end;
         }
      }
   }
   if (!(base = data_transfer_window_base(w->dt, &blen)) || !blen)
   {
      gfx_anim_preview_audio_win_release(w);
      return NULL;
   }
   ahead = gfx_anim_preview_audio_lookahead(p);
   prime_hi = floor_off > (size_t)-1 - ahead
      ? (size_t)-1 : floor_off + ahead;
   if (prime_hi > blen)
      prime_hi = blen;
   if (prime_hi < keep)
      prime_hi = keep < blen ? keep : blen;
   data_transfer_window_rebase(w->dt, floor_off);
   if (!data_transfer_window_extend(w->dt, prime_hi))
   {
      gfx_anim_preview_audio_win_release(w);
      return NULL;
   }
   *initial_hi = island_hi ? blen : prime_hi;
   *feed_hi = prime_hi;
   return w;

}
#endif

/* No mixer or session state is touched while this object is prepared.
 * The published decoder subsequently belongs to its sound/voice. */
struct gfx_anim_preview_audio_prepared
{
#ifdef GFX_ANIM_PREVIEW_AUDIO
   gfx_anim_preview_audio_win_t *window;
   void *transfer;
   audio_mixer_sound_t *sound;
   size_t hi, tell, ahead, feed_anchor;
   enum audio_type_enum type;
   enum audio_mixer_type mixer_type;
   unsigned mix_rate;
   bool s16;
#else
   unsigned unused;
#endif
};

void gfx_anim_preview_audio_prepare_free(gfx_anim_preview_audio_prepared_t *a)
{
   if (!a) return;
#ifdef GFX_ANIM_PREVIEW_AUDIO
   audio_mixer_destroy(a->sound);
   if (a->transfer) audio_transfer_free(a->transfer, a->type);
   gfx_anim_preview_audio_win_release(a->window);
#endif
   free(a);
}

gfx_anim_preview_audio_prepared_t *gfx_anim_preview_audio_prepare_new(
      const gfx_anim_preview_t *p, int64_t target_us, unsigned mix_rate, bool s16)
{
#ifdef GFX_ANIM_PREVIEW_AUDIO
   gfx_anim_preview_audio_prepared_t *a;
   const uint8_t *base;
   size_t len, initial_hi, feed_hi;
   unsigned rate;
   uint64_t seconds, frame;
   if (!p || (p->type != IMAGE_TYPE_MP4 && p->type != IMAGE_TYPE_WEBM) || !p->stream || !p->path
         || target_us < 0 || !mix_rate)
      return NULL;
   if (!(a = (gfx_anim_preview_audio_prepared_t*)calloc(1, sizeof(*a))))
      return NULL;
   a->mix_rate = mix_rate;
   a->s16 = s16;
   a->ahead = gfx_anim_preview_audio_lookahead(p);
   if (!(a->window = gfx_anim_preview_audio_source(p, &initial_hi, &feed_hi)))
      goto failed;
   base = data_transfer_window_base(a->window->dt, &len);
   a->type = p->type == IMAGE_TYPE_WEBM
      ? audio_transfer_webm_audio_type(base, initial_hi) : AUDIO_TYPE_AAC;
   a->mixer_type = p->type == IMAGE_TYPE_WEBM
      ? AUDIO_MIXER_TYPE_WEBA : AUDIO_MIXER_TYPE_M4A;
   if (a->type != AUDIO_TYPE_AAC && a->type != AUDIO_TYPE_OPUS
         && a->type != AUDIO_TYPE_VORBIS) goto failed;
   a->sound = a->mixer_type == AUDIO_MIXER_TYPE_WEBA
      ? audio_mixer_load_weba_avail((void*)base, len, initial_hi)
      : audio_mixer_load_m4a((void*)base, len);
   if (!a->sound) goto failed;
   retro_atomic_fetch_add_int(&a->window->refs, 1);
   audio_mixer_sound_set_data_owner(a->sound, a->window,
         gfx_anim_preview_audio_win_release);
   if (!(a->transfer = audio_transfer_new(a->type))) goto failed;
   audio_transfer_set_buffer_ptr(a->transfer, a->type, (void*)base, len);
   audio_transfer_set_output_rate(a->transfer, a->type, mix_rate);
   audio_transfer_set_avail(a->transfer, a->type, initial_hi);
   if (!audio_transfer_start(a->transfer, a->type)
         || !audio_transfer_info(a->transfer, a->type, NULL, &rate, NULL)
         || !rate) goto failed;
   seconds = (uint64_t)target_us / 1000000;
   if (seconds > (uint64_t)-1 / rate) goto failed;
   frame = seconds * rate;
   seconds = ((uint64_t)target_us % 1000000) * rate / 1000000;
   if (frame > (uint64_t)-1 - seconds) goto failed;
   frame += seconds;
   if (!audio_transfer_seek_begin_format(a->transfer, a->type, frame, s16))
      goto failed;
   a->hi = feed_hi;
   a->tell = audio_transfer_buffer_tell(a->transfer, a->type);
   return a;
failed:
   gfx_anim_preview_audio_prepare_free(a);
#else
   (void)p; (void)target_us; (void)mix_rate; (void)s16;
#endif
   return NULL;
}

int gfx_anim_preview_audio_prepare_step(gfx_anim_preview_audio_prepared_t *a)
{
#ifdef GFX_ANIM_PREVIEW_AUDIO
   size_t len, hi, res_hi, anchor, ahead, requested, prefix;
   int result;
   if (!a || !a->window) return -1;
   if (!a->transfer) return a->sound ? 1 : -1;
   if (!data_transfer_window_base(a->window->dt, &len)) return -1;
   anchor = audio_transfer_buffer_tell(a->transfer, a->type);
   ahead = a->mixer_type == AUDIO_MIXER_TYPE_WEBA ? 16 : a->ahead;
   requested = audio_transfer_buffer_read_end(a->transfer, a->type);
   if (requested > anchor && requested - anchor > ahead)
      ahead = requested - anchor;
   /* A far seek must not make feed_budget fill the gap from the old
    * frontier, or take its unbudgeted catch-up arm. Reserve the target
    * region and commit only this bounded prefix before normal feeding. */
   if (anchor > a->hi || anchor < a->feed_anchor)
      data_transfer_window_rewind(a->window->dt);
   data_transfer_window_rebase(a->window->dt, anchor);
   prefix = ahead < GFX_ANIM_PREVIEW_AUDIO_FEED_BUDGET
      ? ahead : GFX_ANIM_PREVIEW_AUDIO_FEED_BUDGET;
   hi = anchor > (size_t)-1 - prefix ? len : anchor + prefix;
   if (hi > len) hi = len;
   if (!data_transfer_window_extend(a->window->dt, hi)
         || !data_transfer_window_feed_budget(a->window->dt, anchor,
            ahead, GFX_ANIM_PREVIEW_AUDIO_MARGIN,
            GFX_ANIM_PREVIEW_AUDIO_FEED_BUDGET, &res_hi))
      return -1;
   a->hi = res_hi;
   a->feed_anchor = anchor;
   audio_transfer_set_avail(a->transfer, a->type, a->hi);
   result = audio_transfer_seek_step(a->transfer, a->type);
   a->tell = audio_transfer_buffer_tell(a->transfer, a->type);
   if (result != 1) return result;
   audio_mixer_sound_set_avail(a->sound, a->hi);
   if (!audio_mixer_sound_set_prepared_transfer(a->sound, a->transfer,
            a->mix_rate, a->s16))
      return -1;
   a->transfer = NULL;
   return 1;
#else
   (void)a;
   return -1;
#endif
}

bool gfx_anim_preview_audio_adopt(gfx_anim_preview_t *p,
      gfx_anim_preview_audio_prepared_t *a)
{
   bool ok = false;
#ifdef GFX_ANIM_PREVIEW_AUDIO
   if (p && a && a->sound && !a->transfer)
   {
      audio_mixer_stream_params_t params;
      int slot = -1;
      gfx_anim_preview_audio_stop_slot(p);
      memset(&params, 0, sizeof(params));
      params.basename = (char*)GFX_ANIM_PREVIEW_AUDIO_NAME;
      params.out_slot = &slot;
      params.volume = 1.0f;
      params.slot_selection_type = AUDIO_MIXER_SLOT_SELECTION_AUTOMATIC;
      params.stream_type = AUDIO_STREAM_TYPE_SYSTEM;
      params.type = a->mixer_type;
      params.state = p->loop_count ? AUDIO_STREAM_STATE_PLAYING
            : AUDIO_STREAM_STATE_PLAYING_LOOPED;
      audio_mixer_sound_set_initial_paused(a->sound, p->paused);
      ok = audio_driver_mixer_add_prepared_stream(&params, a->sound,
            &p->audio_voice, &p->audio_serial);
      a->sound = NULL;
      if (ok)
      {
         p->audio_owner = a->window;
         p->audio_dt = a->window->dt;
         p->audio_hi = a->hi;
         p->audio_tell = a->tell;
         p->audio_slot = slot;
         a->window = NULL;
      }
   }
#else
   (void)p;
#endif
   gfx_anim_preview_audio_prepare_free(a);
   return ok;
}

bool gfx_anim_preview_seek_request(gfx_anim_preview_t *p,
      unsigned mix_rate, bool s16)
{
   if (!p || !p->seekable || mix_rate > 0x3fffffffU)
      return false;
   retro_atomic_store_release_int(&p->seek_audio_format,
         (int)(mix_rate | (s16 ? 0x40000000U : 0)));
   p->seeking = true;
   p->seek_failed = false;
   gfx_anim_preview_set_paused(p, p->paused);
   return true;
}

static bool gfx_anim_preview_seek_feed(gfx_anim_preview_t *p)
{
   size_t anchor, hi, res_hi, ahead, back, span_lo = 0, span_hi = 0;
   size_t pin_lo = 0, pin_hi = 0;
   int sparse;
   if (!p->windowed || !p->dt) return true;
   image_transfer_anim_stream_pin_span(p->stream, p->type, &pin_lo, &pin_hi);
   if (!data_transfer_window_pin(p->dt, pin_lo, pin_hi)) return false;
   anchor = image_transfer_anim_stream_feed_frontier(p->stream, p->type);
   if (!anchor) anchor = image_transfer_anim_stream_media_floor(p->stream, p->type);
   sparse = image_transfer_anim_stream_seek_span(p->stream, p->type,
         &span_lo, &span_hi);
   if (anchor > p->len)
      return false;
   if (sparse && span_lo >= p->seek_feed_anchor && span_hi <= p->seek_feed_hi)
      return true;
   ahead = sparse ? span_hi - span_lo : p->feed_ahead;
   if (sparse && ahead < GFX_ANIM_PREVIEW_SEEK_AHEAD
         && (anchor <= p->seek_feed_hi
            || anchor - p->seek_feed_hi <= GFX_ANIM_PREVIEW_SEEK_AHEAD))
      ahead = p->len - anchor < GFX_ANIM_PREVIEW_SEEK_AHEAD
         ? p->len - anchor : GFX_ANIM_PREVIEW_SEEK_AHEAD;
   back = sparse ? 0 : p->feed_back;
   if (!sparse)
      image_transfer_anim_stream_next_span(p->stream, p->type, &span_lo, &span_hi);
   if (span_hi > span_lo && span_hi > anchor && span_hi - anchor > ahead)
      ahead = span_hi - anchor;
   if ((!sparse && anchor > p->seek_feed_hi) || anchor < p->seek_feed_anchor)
      data_transfer_window_rewind(p->dt);
   if (sparse)
      data_transfer_window_skip(p->dt, anchor);
   else
      data_transfer_window_rebase(p->dt, anchor);
   hi = anchor > (size_t)-1 - GFX_ANIM_PREVIEW_FEED_BUDGET
      ? p->len : anchor + GFX_ANIM_PREVIEW_FEED_BUDGET;
   if (sparse) hi = anchor + ahead;
   if (hi > p->len) hi = p->len;
   if (!data_transfer_window_extend(p->dt, hi)
         || !data_transfer_window_feed_budget(p->dt, anchor, ahead,
            back, GFX_ANIM_PREVIEW_FEED_BUDGET, &res_hi))
      return false;
   p->seek_feed_hi = p->feed_res_hi = res_hi;
   p->seek_feed_anchor = p->feed_tell = anchor;
   image_transfer_anim_stream_set_avail(p->stream, p->type, res_hi);
   {
      size_t resident = data_transfer_window_resident(p->dt);
      retro_atomic_store_release_int(&p->resident_seen,
            resident > 0x7fffffffU ? 0x7fffffff : (int)resident);
   }
   return true;
}

int gfx_anim_preview_seek_step(void *user, int64_t target_us,
      bool restart, int64_t *position_us)
{
   gfx_anim_preview_t *p = (gfx_anim_preview_t*)user;
   int result;
   if (!p || !position_us) return -1;
   if (restart)
   {
      const int64_t maximum = (int64_t)((~(uint64_t)0) >> 1);
      if (target_us < 0) target_us = 0;
      target_us = target_us > maximum - p->timeline_start_us
         ? maximum : target_us + p->timeline_start_us;
      gfx_anim_preview_audio_prepare_free(p->seek_audio);
      p->seek_audio = NULL;
      p->seek_format = (unsigned)retro_atomic_load_acquire_int(&p->seek_audio_format);
      p->seek_phase = 1;
      p->seek_feed_hi = p->feed_res_hi;
      p->seek_feed_anchor = p->feed_tell;
      if (!image_transfer_anim_stream_seek_begin(p->stream, p->type, target_us))
         return -1;
      if (p->windowed && !data_transfer_window_pin(p->dt, 0, 0)) return -1;
   }
   if (p->seek_phase == 1)
   {
      int64_t started = cpu_features_get_time_usec();
      unsigned budget = 1024, feeds = 1;
      if (!gfx_anim_preview_seek_feed(p)) return -1;
      do
      {
         result = image_transfer_anim_stream_seek_step(p->stream, p->type,
               &p->seek_video_us);
         if (result || cpu_features_get_time_usec() - started >= 4000)
            break;
         if (image_transfer_anim_stream_waiting(p->stream, p->type))
         {
            size_t lo, hi;
            if (p->windowed && p->dt
                  && image_transfer_anim_stream_seek_span(p->stream,
                     p->type, &lo, &hi))
            {
               if (!gfx_anim_preview_seek_feed(p)) return -1;
               continue;
            }
         }
         if (p->windowed && p->dt)
         {
            size_t anchor = image_transfer_anim_stream_feed_frontier(p->stream, p->type);
            size_t lo = 0, hi = 0;
            int sparse = image_transfer_anim_stream_seek_span(p->stream,
                  p->type, &lo, &hi);
            if (!anchor) anchor = image_transfer_anim_stream_media_floor(p->stream, p->type);
            if (!sparse)
               image_transfer_anim_stream_next_span(p->stream, p->type, &lo, &hi);
            if (anchor < p->seek_feed_anchor
                  || (hi > lo && lo < p->seek_feed_anchor))
               break;
            if (anchor >= p->seek_feed_hi
                  || (hi > lo && hi > p->seek_feed_hi))
            {
               if (!sparse && feeds >= 8) break;
               if (!gfx_anim_preview_seek_feed(p)) return -1;
               feeds++;
               continue;
            }
         }
         if (image_transfer_anim_stream_waiting(p->stream, p->type))
            break;
      } while (--budget);
      if (result != 1)
      {
         size_t lo = 0, hi = 0;
         bool local_input = p->windowed && p->dt
            && image_transfer_anim_stream_seek_span(p->stream, p->type, &lo, &hi);
         if (!local_input && p->windowed && p->dt)
         {
            size_t frontier = image_transfer_anim_stream_feed_frontier(p->stream, p->type);
            image_transfer_anim_stream_next_span(p->stream, p->type, &lo, &hi);
            local_input = (hi > lo && (lo < p->seek_feed_anchor || hi > p->seek_feed_hi))
               || frontier >= p->seek_feed_hi;
         }
         return !result && (local_input
               || !image_transfer_anim_stream_waiting(p->stream, p->type))
            ? VIDEO_PLAYER_SEEK_PROGRESS : result;
      }
      p->seek_video_us = p->seek_video_us > p->timeline_start_us
         ? p->seek_video_us - p->timeline_start_us : 0;
      p->seek_phase = 2;
      return VIDEO_PLAYER_SEEK_PROGRESS;
   }
   if (p->seek_phase == 2 && (p->seek_format & 0x3fffffffU))
   {
      p->seek_audio = gfx_anim_preview_audio_prepare_new(p, p->seek_video_us,
            p->seek_format & 0x3fffffffU, (p->seek_format & 0x40000000U) != 0);
      p->seek_phase = 3;
      return VIDEO_PLAYER_SEEK_PROGRESS;
   }
   if (p->seek_phase < 2 || p->seek_phase > 4) return -1;
   if (p->seek_phase == 3 && p->seek_audio)
   {
      result = gfx_anim_preview_audio_prepare_step(p->seek_audio);
      if (!result) return VIDEO_PLAYER_SEEK_PROGRESS;
      if (result < 0)
      {
         /* A missing/failed audio track must not discard valid video. */
         gfx_anim_preview_audio_prepare_free(p->seek_audio);
         p->seek_audio = NULL;
      }
      p->seek_phase = 4;
      return VIDEO_PLAYER_SEEK_PROGRESS;
   }
   *position_us = p->seek_video_us;
   return 1;
}

void gfx_anim_preview_seek_cancel(gfx_anim_preview_t *p)
{
   if (!p || !p->seeking) return;
   gfx_anim_preview_audio_prepare_free(p->seek_audio);
   p->seek_audio = NULL;
   gfx_anim_preview_set_paused(p, true);
   p->seeking = false;
   p->seek_failed = true;
}

void gfx_anim_preview_seek_commit(gfx_anim_preview_t *p, int64_t position_us)
{
   gfx_anim_preview_audio_prepared_t *audio;
   if (!p || !p->seeking) return;
   audio = p->seek_audio;
   p->seek_audio = NULL;
   gfx_anim_preview_audio_stop(p);
   if (audio) gfx_anim_preview_audio_adopt(p, audio);
   p->seeking = false;
   p->seek_failed = false;
   p->media_origin_us = position_us;
   p->media_clock_us = p->clock_us;
   p->media_clock_valid = true;
   gfx_anim_preview_set_paused(p, p->paused);
}

void gfx_anim_preview_audio_begin(gfx_anim_preview_t *p)
{
#if defined(GFX_ANIM_PREVIEW_AUDIO)
   gfx_anim_preview_audio_win_t *w;
   const uint8_t *base = NULL;
   size_t blen = 0, prime_hi;

   if (!p || p->paused || p->seeking || !p->stream || !p->path || !*p->path)
      return;
   if (   (p->type != IMAGE_TYPE_WEBM && p->type != IMAGE_TYPE_MP4)
       || !config_get_ptr()->bools.menu_thumbnail_preview_audio)
      return;
   if (p->audio_dt)
      return;                   /* already streaming */

   if (!(w = gfx_anim_preview_audio_source(p, &p->audio_hi, &prime_hi)))
      return;
   base = data_transfer_window_base(w->dt, &blen);

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
      params.state               = p->loop_count ? AUDIO_STREAM_STATE_PLAYING
            : AUDIO_STREAM_STATE_PLAYING_LOOPED;
      params.end_granule         = 0;
      params.avail               = p->audio_hi;
      retro_atomic_fetch_add_int(&w->refs, 1);
      if (!audio_driver_mixer_add_stream_clocked(&params,
               &p->audio_voice, &p->audio_serial))
      {
         /* The mixer returned its reference; release the session's. */
         gfx_anim_preview_audio_win_release(w);
         free(params.basename);
         p->audio_hi = 0;
         return;
      }
      free(params.basename);
      p->audio_owner = w;
      p->audio_dt   = w->dt;
      p->audio_slot = out_slot;
      p->audio_hi = prime_hi;
      /* Every windowed stream decodes under a bound from its first
       * read: without one the decoder trusts the demuxer's offsets
       * outright, and the first lap's end shows why that cannot be -
       * it loops and reads the head at once, before the feeder's next
       * tick has brought the head back, into pages given up long ago.
       * The bound is what makes it stand at the wall instead. */
      if (p->audio_voice)
         audio_mixer_voice_set_avail_bound(p->audio_voice,
               p->audio_hi, p->audio_serial);
      else
         audio_driver_mixer_stream_set_avail((unsigned)out_slot, p->audio_hi);
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
   if (p->audio_voice)
   {
      size_t serial;
      if (!audio_mixer_voice_clock(p->audio_voice, NULL, NULL, &serial)
            || serial != p->audio_serial)
      {
         gfx_anim_preview_audio_stop_slot(p);
         return false;
      }
   }
   /* The decoder runs on the audio thread out of this mapping: feed
    * first, then raise the bound to what the feed actually committed. */
   tell = p->audio_voice
      ? (int64_t)audio_mixer_voice_buffer_tell(p->audio_voice)
      : audio_driver_mixer_stream_byte_tell((unsigned)p->audio_slot);
   if (p->audio_voice)
   {
      size_t serial;
      if (!audio_mixer_voice_clock(p->audio_voice, NULL, NULL, &serial)
            || serial != p->audio_serial)
      {
         gfx_anim_preview_audio_stop_slot(p);
         return false;
      }
   }
   if (tell >= 0)
   {
      size_t anchor = (size_t)tell;
      size_t ahead  = gfx_anim_preview_audio_lookahead(p);
      size_t hi     = (anchor > (size_t)-1 - ahead)
                    ? (size_t)-1 : anchor + ahead;
      size_t res_hi = 0;
      size_t budget = GFX_ANIM_PREVIEW_AUDIO_FEED_BUDGET;
      size_t requested = p->audio_voice
         ? audio_mixer_voice_buffer_read_end(p->audio_voice, p->audio_serial) : 0;
      if (requested > anchor && requested - anchor > ahead)
      {
         ahead = requested - anchor;
         hi = requested;
      }
      if (hi > p->len)
         hi = p->len;
      /* The decoder looped: it stands at the wall until this tick has
       * brought the head back and published a bound for the new lap,
       * so the bound published for the old lap counts for nothing. */
      if (anchor < p->audio_tell)
      {
         p->audio_hi = 0;
         budget = 0;
      }
      p->audio_tell = anchor;
      if (!data_transfer_window_feed_budget(p->audio_dt, anchor,
               ahead, GFX_ANIM_PREVIEW_AUDIO_MARGIN,
               budget, &res_hi))
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
         if (p->audio_voice)
            audio_mixer_voice_set_avail_bound(p->audio_voice,
                  hi, p->audio_serial);
         else
            audio_driver_mixer_stream_set_avail((unsigned)p->audio_slot, hi);
      }
   }
   return true;
#else
   (void)p;
   return true;
#endif
}

void gfx_anim_preview_set_repeat(gfx_anim_preview_t *p, bool repeat)
{
   if (!p)
      return;
   p->loop_count = repeat ? 0 : 1;
#if defined(GFX_ANIM_PREVIEW_AUDIO)
   if (p->audio_slot >= 0)
   {
      const char *name
            = audio_driver_mixer_get_stream_name((unsigned)p->audio_slot);
      if (name && string_is_equal(name, GFX_ANIM_PREVIEW_AUDIO_NAME))
         audio_driver_mixer_stream_set_repeat((unsigned)p->audio_slot, repeat);
   }
#endif
}

void gfx_anim_preview_set_paused(gfx_anim_preview_t *p, bool paused)
{
   if (!p)
      return;
   p->paused = paused;
#if defined(GFX_ANIM_PREVIEW_AUDIO)
   if (p->audio_slot >= 0)
   {
      const char *name = audio_driver_mixer_get_stream_name(
            (unsigned)p->audio_slot);
      if (name && string_is_equal(name, GFX_ANIM_PREVIEW_AUDIO_NAME))
         audio_driver_mixer_stream_set_paused((unsigned)p->audio_slot, paused || p->seeking);
   }
#endif
}

int64_t gfx_anim_preview_clock(gfx_anim_preview_t *p, int64_t wall_us)
{
   const uint64_t maximum = (~(uint64_t)0) >> 1;
   uint64_t advance;
   size_t frames = 0;
   size_t serial = 0;
   unsigned rate = 0;
   bool audio = false;
   if (!p)
      return wall_us;
   if (!p->clock_initialized)
   {
      p->clock_initialized = true;
      p->clock_us = p->clock_wall_us = wall_us;
   }
   advance = wall_us > p->clock_wall_us
      ? (uint64_t)wall_us - (uint64_t)p->clock_wall_us : 0;
#if defined(GFX_ANIM_PREVIEW_AUDIO)
   audio = p->audio_voice
      && audio_mixer_voice_clock(p->audio_voice, &frames, &rate, &serial)
      && serial == p->audio_serial && rate;
#endif
   if (audio)
   {
      size_t delta;
      uint64_t fraction, seconds, tail;
      if (!p->clock_audio || p->clock_serial != serial)
      {
         p->clock_frames = 0;
         p->clock_fraction = 0;
      }
      else if (p->clock_rate != rate)
      {
         p->clock_frames = frames;
         p->clock_fraction = 0;
      }
      delta = frames - p->clock_frames;
      fraction = (uint64_t)(delta % rate) * 1000000
         + p->clock_fraction;
      seconds = delta / rate;
      tail = fraction / rate;
      advance = seconds > maximum / 1000000 ? maximum
         : seconds * 1000000;
      advance = advance > maximum - tail ? maximum : advance + tail;
      if (!p->paused && !p->seeking)
         p->clock_fraction = (unsigned)(fraction % rate);
      p->clock_frames = frames;
      p->clock_rate = rate;
      p->clock_serial = serial;
   }
   else
      p->clock_fraction = 0;
   if (advance > maximum)
      advance = maximum;
   if (!p->paused && !p->seeking)
      p->clock_us = p->clock_us > (int64_t)(maximum - advance)
         ? (int64_t)maximum : p->clock_us + (int64_t)advance;
   if (wall_us > p->clock_wall_us)
      p->clock_wall_us = wall_us;
   p->clock_audio = audio;
   return p->clock_us;
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
   p->seekable   = image_transfer_anim_stream_seekable(stream, type);
   p->timeline_start_us = 0;
   p->windowed   = windowed;
   p->audio_slot = -1;
   p->path       = path ? strldup(path, strlen(path) + 1) : NULL;
   {
      /* The stream fills the axes separately; they are one word here. */
      unsigned sw = 0;
      unsigned sh = 0;
      image_transfer_anim_stream_get_info(stream, type, &sw, &sh,
            &p->num_frames, &p->loop_count);
      p->dims    = VIDEO_SCALE_PACK(sw, sh);
   }
   /* gfx_thumbnail's worker asks the stream itself per job; the answer
    * recorded here is for callers that draw through the session. */
   p->native_argb = image_transfer_anim_stream_set_argb(stream, type, 1);
   gfx_anim_preview_size_feed(p);
   return p;
}

bool gfx_anim_preview_windowed(const gfx_anim_preview_t *p)
{
   return p && p->windowed;
}

void gfx_anim_preview_release(gfx_anim_preview_t *p)
{
   if (!p)
      return;
   gfx_anim_preview_audio_stop(p);
   gfx_anim_preview_audio_prepare_free(p->seek_audio);
   free(p->path);
   free(p);
}

/* --- close ------------------------------------------------------------------ */

void gfx_anim_preview_close(gfx_anim_preview_t *p)
{
   if (!p)
      return;
   gfx_anim_preview_audio_stop(p);
   gfx_anim_preview_audio_prepare_free(p->seek_audio);
   if (p->stream)
      image_transfer_anim_stream_free(p->stream, p->type);
   if (p->dt)
      data_transfer_free(p->dt);
   free(p->path);
   free(p);
}
