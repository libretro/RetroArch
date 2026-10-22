/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#define _POSIX_C_SOURCE 200809L
#ifdef NDEBUG
#error Native VA acceptance requires assertions
#endif
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <time.h>
#include "../../../gfx/common/vaapi_hevc_owner.h"
#include <formats/image_yuv_blit.h>
#include "native_pixels.h"

typedef struct codec_case
{
   const char *name;
   const uint8_t *hvcc;
   size_t hvcc_size;
   const uint8_t *data;
   size_t size;
   unsigned depth, full;
   uint32_t crc[3];
} codec_case_t;

typedef struct sequence_packet
{
   int64_t dts;
   const uint8_t *data;
   size_t size;
   uint32_t crc[3];
   unsigned display_index, slice_type;
} sequence_packet_t;

typedef struct sequence_case
{
   codec_case_t picture;
   const sequence_packet_t *packets;
   unsigned reordered;
} sequence_case_t;

#include "../videotoolbox_hevc/sequence_data.h"
#include "../videotoolbox_hevc/reorder_data.h"

#define VA_OK(call)                                                            \
   do                                                                          \
   {                                                                           \
      VAStatus status = (call);                                                \
      if (status != VA_STATUS_SUCCESS)                                         \
         fprintf(stderr, "%s: VA error %d\n", #call, (int)status);             \
      assert(status == VA_STATUS_SUCCESS);                                     \
   } while (0)

static const char *node;
static unsigned major_id, minor_id, shown, modes, total, selected_mode;
static const sequence_case_t *active;
static vaapi_hevc_owner_t *opened, *retired;
static video_hw_frame_t held;
static vaapi_frame_view_t held_view;

static uint64_t now_usec(void)
{
   struct timespec ts;
   assert(!clock_gettime(CLOCK_MONOTONIC, &ts));
   return (uint64_t)ts.tv_sec * 1000000u + (unsigned)ts.tv_nsec / 1000u;
}

static void pause_until(uint64_t deadline)
{
   struct timespec delay;
   assert(now_usec() < deadline);
   delay.tv_sec  = 0;
   delay.tv_nsec = 1000000;
   assert(!nanosleep(&delay, NULL) || errno == EINTR);
}

static unsigned fd_count(void)
{
   DIR *directory = opendir("/proc/self/fd");
   unsigned count = 0;
   assert(directory);
   errno = 0;
   while (readdir(directory))
      count++;
   assert(!errno && !closedir(directory));
   return count;
}

static void retire_owner(void)
{
   uint64_t deadline = now_usec() + 10000000u;
   while (retired && !vaapi_hevc_owner_retire(retired))
      pause_until(deadline);
   retired = NULL;
}

static bool open_owner(void *user, const rh265_video *source,
      const rh265_video_params_t *params, unsigned slots, rh265_hw_ops_t *ops)
{
   video_color_metadata_t color;
   unsigned yuv = IMAGE_YUV_FLAG_BT709;
   (void)user;
   assert(!opened && !retired);
   memset(&color, 0, sizeof(color));
   color.primaries = color.matrix = 1;
   color.transfer                 = 1;
   color.range                    = active->picture.full ? 2 : 1;
   if (active->picture.full)
      yuv |= IMAGE_YUV_FLAG_FULL_RANGE;
   if (!vaapi_hevc_owner_open_source(&opened, source, node, major_id, minor_id,
            params, &color, yuv, slots, 65536, ops))
   {
      retired = opened;
      opened  = NULL;
      retire_owner();
      return false;
   }
   return true;
}

static void close_owner(rh265_hw_ops_t *ops)
{
   assert(opened && !retired);
   retired = vaapi_hevc_owner_detach(ops);
   assert(retired == opened);
   opened = NULL;
}

/* Test-only CPU transfer. Production decode/export/import adds no readback. */
static void compare_pixels(const vaapi_frame_view_t *view,
      const uint32_t expected[3], unsigned display_index)
{
   const vaapi_hevc_export_t *source = view->source;
   const vaapi_hevc_session_t *session;
   VAImageFormat *formats, *selected = NULL;
   VAImage image;
   VADisplay display;
   VASurfaceID surface;
   void *mapped = NULL;
   uint32_t crc[3];
   unsigned fourcc =
         active->picture.depth == 10 ? VA_FOURCC_P010 : VA_FOURCC_NV12;
   int maximum, count, i;
   assert(source && source->active && source->ready && source->pool);
   session = source->pool->session;
   display = session->display;
   assert(source->slot < session->slots);
   surface = session->surfaces[source->slot];
   maximum = vaMaxNumImageFormats(display);
   assert(maximum > 0 && maximum <= 256);
   formats = (VAImageFormat *)calloc((size_t)maximum, sizeof(*formats));
   assert(formats);
   count = maximum;
   VA_OK(vaQueryImageFormats(display, formats, &count));
   assert(count >= 0 && count <= maximum);
   for (i = 0; i < count; i++)
      if (formats[i].fourcc == fourcc
            && (fourcc != VA_FOURCC_P010
                  || formats[i].byte_order == VA_LSB_FIRST))
         selected = &formats[i];
   assert(selected);
   VA_OK(vaCreateImage(
         display, selected, (int)session->width, (int)session->height, &image));
   free(formats);
   VA_OK(vaGetImage(display, surface, 0, 0, session->width, session->height,
         image.image_id));
   VA_OK(vaMapBuffer(display, image.buf, &mapped));
   assert(mapped && image.format.fourcc == fourcc);
   assert(native_image_crc(&image, (const uint8_t *)mapped, view->x, view->y,
         view->width, view->height, crc));
   VA_OK(vaUnmapBuffer(display, image.buf));
   VA_OK(vaDestroyImage(display, image.image_id));
   if (memcmp(crc, expected, sizeof(crc)))
      fprintf(stderr,
            "%s display %u: CRC %08x/%08x/%08x expected %08x/%08x/%08x\n",
            active->picture.name, display_index, (unsigned)crc[0],
            (unsigned)crc[1], (unsigned)crc[2], (unsigned)expected[0],
            (unsigned)expected[1], (unsigned)expected[2]);
   assert(!memcmp(crc, expected, sizeof(crc)));
   assert(session->slice_mode == VA_DEC_SLICE_MODE_NORMAL
         || session->slice_mode == VA_DEC_SLICE_MODE_BASE);
   modes |= session->slice_mode;
   if (selected_mode)
      assert(selected_mode == session->slice_mode);
   selected_mode = session->slice_mode;
}

static const uint32_t *display_crc(unsigned display)
{
   unsigned i;
   for (i = 0; i < 8; i++)
      if (active->packets[i].display_index == display)
         return active->packets[i].crc;
   assert(0);
   return NULL;
}

static void output_picture(rh265_video *source)
{
   video_hw_frame_t frame;
   vaapi_frame_view_t view;
   assert(shown < 8 && !rh265_video_hw_frame(source, &frame));
   assert(vaapi_frame_callback(frame.abandon));
   assert(vaapi_frame_claim(
         frame.owner, frame.done, frame.done_value, frame.image, &view));
   assert(frame.width == 64 && frame.height == 64);
   compare_pixels(&view, display_crc(shown), shown);
   if (shown == 7)
   {
      held      = frame;
      held_view = view;
   }
   else
   {
      video_hw_frame_abandon(&frame);
      assert(vaapi_frame_finish(
            frame.owner, frame.done, frame.done_value, NULL));
   }
   shown++;
   total++;
}

static bool execute(const sequence_case_t *fixture)
{
   rh265_video *source;
   uint64_t deadline;
   unsigned packet, before = fd_count();
   int result;
   active        = fixture;
   shown         = 0;
   selected_mode = 0;
   source        = rh265_video_open();
   assert(source
         && !rh265_video_set_extradata(
               source, fixture->picture.hvcc, fixture->picture.hvcc_size));
   if (!rh265_video_set_hw(source, 1))
   {
      rh265_video_close(source);
      assert(!opened && !retired && fd_count() == before);
      fprintf(stderr, "UNAVAILABLE: mandatory native HEVC admission for %s\n",
            fixture->picture.name);
      return false;
   }
   for (packet = 0; packet < 8; packet++)
   {
      deadline = now_usec() + 10000000u;
      do
      {
         result = rh265_video_decode(source, fixture->packets[packet].data,
               fixture->packets[packet].size);
         assert(result >= 0 && result <= 3);
         if (result == 1 || result == 3)
            output_picture(source);
         if (result == 2 || result == 3)
            pause_until(deadline);
      } while (result == 2 || result == 3);
   }
   deadline = now_usec() + 10000000u;
   while (shown < 8)
   {
      result = rh265_video_drain(source);
      assert(result == 0 || result == 2);
      if (!result)
         output_picture(source);
      else
         pause_until(deadline);
   }
   rh265_video_close(source);
   assert(retired && !vaapi_hevc_owner_retire(retired));
   compare_pixels(&held_view, display_crc(7), 7);
   video_hw_frame_abandon(&held);
   assert(vaapi_frame_finish(held.owner, held.done, held.done_value, NULL));
   retire_owner();
   assert(fd_count() == before);
   printf(
         "PASS native VA decode/export: %s, mode=%s, 8 display-order CRCs, held output after source close, FD baseline restored\n",
         fixture->picture.name,
         selected_mode == VA_DEC_SLICE_MODE_NORMAL ? "NORMAL" : "BASE");
   return true;
}

int main(int argc, char **argv)
{
   rh265_hw_backend_t backend;
   struct stat status;
   unsigned round, i;
   if (argc != 2 || stat(argv[1], &status) || !S_ISCHR(status.st_mode))
   {
      fprintf(stderr,
            "UNAVAILABLE: supply an accessible DRM render node: native_decode /dev/dri/renderD128\n");
      return 2;
   }
   node          = argv[1];
   major_id      = major(status.st_rdev);
   minor_id      = minor(status.st_rdev);
   backend.user  = NULL;
   backend.open  = open_owner;
   backend.close = close_owner;
   rh265_set_hw_backend(&backend);
   for (round = 0; round < 2; round++)
   {
      for (i = 0; i < sizeof(sequence_cases) / sizeof(sequence_cases[0]); i++)
         if (!execute(&sequence_cases[i]))
         {
            assert(rh265_unset_hw_backend(&backend));
            return 2;
         }
      for (i = 0; i < sizeof(reorder_cases) / sizeof(reorder_cases[0]); i++)
         if (!execute(&reorder_cases[i]))
         {
            assert(rh265_unset_hw_backend(&backend));
            return 2;
         }
   }
   assert(total == 128 && rh265_unset_hw_backend(&backend));
   printf(
         "PASS native VA oracle: %u pictures, selected slice-mode mask %u; Vulkan modifier import remains separate\n",
         total, modes);
   return 0;
}
