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

#include <stdlib.h>

#include "gfx_surface_player.h"
#include "gfx_instrument.h"
#include "video_defines.h"

/* An animation's frames are linear scRGB half floats where the source
 * is HDR and the driver offers such a texture (only while the output
 * is HDR); 8888 everywhere else. */
static int gfx_surface_player_start(void *user, unsigned width,
      unsigned height, unsigned num_slots)
{
   gfx_surface_player_t *gsp = (gfx_surface_player_t*)user;
   gfx_surface_requirements_t req;
   bool have_req   = gfx_surface_query_requirements(0, &req);
   uint32_t pixfmt = IMAGE_PIXFMT_8888;
   gfx_surface_t *s;

   if (!VIDEO_SCALE_FITS(width, height) || !num_slots)
      return -1;
   if (     have_req && (req.formats & IMAGE_PIXFMT_FP16)
         && image_transfer_anim_stream_is_hdr(gsp->stream, gsp->type))
      pixfmt = IMAGE_PIXFMT_FP16;
   if (!(s = gsp->cb.surface(gsp->cb.user,
         VIDEO_SCALE_PACK(width, height), num_slots, pixfmt)))
      return -1;
   gsp->rgba     = have_req && req.rgba;
   gsp->threaded = num_slots > 1;
   return (gsp->rgba ? VIDEO_PLAYER_RGBA : 0)
        | (s->pixfmt == IMAGE_PIXFMT_FP16 ? VIDEO_PLAYER_FP16 : 0);
}

/* On the worker a slot is taken until its job settles, so the memory
 * stays even if the texture behind it is replaced, and a lent slot the
 * GPU still reads is not handed out. On the polling thread the one
 * slot is written only once the last frame is off to the video thread. */
static uint32_t *gfx_surface_player_slot_begin(void *user, unsigned slot)
{
   gfx_surface_player_t *gsp = (gfx_surface_player_t*)user;
   gfx_surface_t *s          = (gfx_surface_t*)*gsp->surface;
   if (!s || !gfx_surface_slot_writable(s, slot))
      return NULL;
   if (gsp->threaded)
      return gfx_surface_slot_begin(s, slot);
   if (s->inflight)
      return NULL;
   return s->slots[slot];
}

static void gfx_surface_player_slot_end(void *user, unsigned slot)
{
   gfx_surface_player_t *gsp = (gfx_surface_player_t*)user;
   gfx_surface_t *s          = (gfx_surface_t*)*gsp->surface;
   if (s)
      gfx_surface_slot_end(s, slot);
}

static enum video_player_present gfx_surface_player_present(void *user,
      const video_player_frame_t *fr)
{
   gfx_surface_player_t *gsp = (gfx_surface_player_t*)user;
   gfx_surface_t *s          = (gfx_surface_t*)*gsp->surface;
   enum gfx_surface_submit_result res;

   if (!s)
      return VIDEO_PLAYER_PRESENT_FAILED;
   res = fr->pixels
      ? gfx_surface_submit_pixels(s, fr->pixels, gsp->rgba)
      : gfx_surface_submit(s, fr->slot, gsp->rgba);
   switch (res)
   {
      case GFX_SURFACE_SUBMIT_DONE:
         gsp->cb.shown(gsp->cb.user, s);
         return VIDEO_PLAYER_PRESENT_DONE;
      case GFX_SURFACE_SUBMIT_QUEUED:
         return VIDEO_PLAYER_PRESENT_QUEUED;
      case GFX_SURFACE_SUBMIT_BUSY:
         return VIDEO_PLAYER_PRESENT_BUSY;
      case GFX_SURFACE_SUBMIT_DROPPED:
         /* the worker's frame waits in its slot for the next try; one
          * decoded in line is passed over, the next is already due */
         return gsp->threaded
            ? VIDEO_PLAYER_PRESENT_BUSY : VIDEO_PLAYER_PRESENT_DONE;
      default:
         break;
   }
   /* nothing uploaded: the frame is gone, the playback goes on */
   return VIDEO_PLAYER_PRESENT_DONE;
}

static bool gfx_surface_player_feed(void *user)
{
   gfx_surface_player_t *gsp = (gfx_surface_player_t*)user;
   return gsp->cb.feed(gsp->cb.user);
}

static void gfx_surface_player_decoded(void *user, enum video_player_how how)
{
   (void)user;
   GFX_INSTR_INC(GFX_INSTR_ANIM_FRAME);
   switch (how)
   {
      case VIDEO_PLAYER_DIRECT:
         GFX_INSTR_INC(GFX_INSTR_ANIM_DIRECT);
         break;
      case VIDEO_PLAYER_COPY:
         GFX_INSTR_INC(GFX_INSTR_ANIM_COPY);
         break;
      case VIDEO_PLAYER_SWIZZLE:
         GFX_INSTR_INC(GFX_INSTR_ANIM_SWIZZLE);
         break;
      default:
         break;
   }
}

gfx_surface_player_t *gfx_surface_player_new(void *stream,
      enum image_type_enum type, int loop_count, void **surface,
      const gfx_surface_player_cbs_t *cb, unsigned flags)
{
   video_player_sink_t sink;
   gfx_surface_player_t *gsp;

   if (!surface || !cb || !cb->surface || !cb->shown)
      return NULL;
   if (!(gsp = (gfx_surface_player_t*)calloc(1, sizeof(*gsp))))
      return NULL;
   gsp->stream     = stream;
   gsp->type       = type;
   gsp->surface    = surface;
   gsp->cb         = *cb;
   sink.user       = gsp;
   sink.start      = gfx_surface_player_start;
   sink.slot_begin = gfx_surface_player_slot_begin;
   sink.slot_end   = gfx_surface_player_slot_end;
   sink.present    = gfx_surface_player_present;
   sink.feed       = cb->feed ? gfx_surface_player_feed : NULL;
   sink.decoded    = gfx_surface_player_decoded;
   if (!(gsp->vp = video_player_new(stream, type, loop_count, &sink,
         flags)))
   {
      free(gsp);
      return NULL;
   }
   return gsp;
}

void gfx_surface_player_free(gfx_surface_player_t *gsp)
{
   if (!gsp)
      return;
   video_player_free(gsp->vp);
   free(gsp);
}

bool gfx_surface_player_release(gfx_surface_player_t *gsp,
      gfx_surface_t *s, unsigned slot)
{
   /* Only a two-slot surface is fed by the worker, whose frame sits in
    * its slot until shown; a frame decoded in line has a successor due
    * already. */
   if (     s->dropped && s->num_slots > 1
         && gfx_surface_submit(s, slot,
               gsp ? gsp->rgba : gfx_surface_wants_rgba())
            == GFX_SURFACE_SUBMIT_QUEUED)
      return true;
   if (gsp)
      video_player_slot_released(gsp->vp, slot);
   return false;
}
