/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <string.h>
#include "../../../gfx/gfx_surface_player.c"

static unsigned imported, shown, failures;
static uint32_t received;

settings_t *config_get_ptr(void)
{
   failures++;
   return NULL;
}

bool image_transfer_anim_stream_stop_hw(void *stream, enum image_type_enum type)
{
   (void)stream;
   (void)type;
   failures++;
   return false;
}

bool video_player_request_software(video_player_t *p)
{
   (void)p;
   failures++;
   return false;
}

/* The format path must not enter CPU playback or upload methods. */
bool gfx_surface_slot_writable(gfx_surface_t *s, unsigned slot)
{ (void)s; (void)slot; failures++; return false; }
uint32_t *gfx_surface_slot_begin(gfx_surface_t *s, unsigned slot)
{ (void)s; (void)slot; failures++; return NULL; }
void gfx_surface_slot_end(gfx_surface_t *s, unsigned slot)
{ (void)s; (void)slot; failures++; }
bool gfx_surface_query_requirements(unsigned width, gfx_surface_requirements_t *req)
{ (void)width; (void)req; failures++; return false; }
bool gfx_surface_wants_rgba(void)
{ failures++; return false; }
bool image_transfer_anim_stream_is_hdr(const void *stream, enum image_type_enum type)
{ (void)stream; (void)type; failures++; return false; }
video_player_t *video_player_new(void *stream, enum image_type_enum type,
      int loops, const video_player_sink_t *sink, unsigned flags)
{ (void)stream; (void)type; (void)loops; (void)sink; (void)flags; failures++; return NULL; }
void video_player_free(video_player_t *p)
{ (void)p; failures++; }
void video_player_slot_released(video_player_t *p, unsigned slot)
{ (void)p; (void)slot; failures++; }
enum gfx_surface_submit_result gfx_surface_submit(gfx_surface_t *s, unsigned slot, bool rgba)
{ (void)s; (void)slot; (void)rgba; failures++; return GFX_SURFACE_SUBMIT_FAILED; }
enum gfx_surface_submit_result gfx_surface_submit_pixels(gfx_surface_t *s,
      const void *pixels, bool rgba)
{ (void)s; (void)pixels; (void)rgba; failures++; return GFX_SURFACE_SUBMIT_FAILED; }

enum gfx_surface_submit_result gfx_surface_submit_import(gfx_surface_t *surface,
      const video_frame_import_t *frame)
{
   (void)surface;
   imported++;
   received = frame->pixfmt;
   if (frame->plane_views[0] != 11 || frame->plane_views[1] != 12) failures++;
   if (frame->array_slice != 7 || frame->crop_x != 2 || frame->crop_y != 4) failures++;
   return GFX_SURFACE_SUBMIT_DONE;
}

static void did_show(void *user, gfx_surface_t *surface)
{
   (void)user; (void)surface;
   shown++;
}

int main(void)
{
   gfx_surface_player_t player;
   gfx_surface_t surface;
   video_hw_frame_t hw;
   video_player_frame_t frame;
   unsigned formats[6] = { 0, IMAGE_PIXFMT_NV12, IMAGE_PIXFMT_P010,
      IMAGE_PIXFMT_YUV8_FP16, IMAGE_PIXFMT_YUV10_FP16, IMAGE_PIXFMT_8888 };
   unsigned i;
   memset(&player, 0, sizeof(player));
   memset(&surface, 0, sizeof(surface));
   memset(&hw, 0, sizeof(hw));
   memset(&frame, 0, sizeof(frame));
   player.cb.shown = did_show;
   frame.hw = &hw;
   hw.image = 1; hw.width = hw.height = 64;
   hw.plane_views[0] = 11; hw.plane_views[1] = 12;
   hw.array_slice = 7; hw.crop_x = 2; hw.crop_y = 4;
   for (i = 0; i < 6; i++)
   {
      enum video_player_present result;
      hw.pixfmt = formats[i];
      result = gfx_surface_player_present_hw(&player, &surface, &frame);
      if (i == 5)
      {
         if (result != VIDEO_PLAYER_PRESENT_FAILED || imported != 5) failures++;
      }
      else if (result != VIDEO_PLAYER_PRESENT_DONE
            || received != (formats[i] ? formats[i] : IMAGE_PIXFMT_NV12)) failures++;
   }
   if (shown != 5) failures++;
   if (IMAGE_PIXFMT_BPP(IMAGE_PIXFMT_YUV8_FP16) != 8
         || IMAGE_PIXFMT_FRAME_SIZE(IMAGE_PIXFMT_YUV10_FP16, 64, 32) != 16384
         || IMAGE_PIXFMT_PLANES(IMAGE_PIXFMT_YUV10_FP16) != 1) failures++;
   printf("GPU frame format transport: %u failures\n", failures);
   return failures ? 1 : 0;
}
