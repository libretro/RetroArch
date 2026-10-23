/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#define _POSIX_C_SOURCE 200809L
#ifdef NDEBUG
#error Native VA acceptance requires assertions
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/rvp9_hw.h>
#include "../videotoolbox_vp9/native_fixture.h"

static uint32_t read32(const uint8_t *p)
{
   return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static uint32_t hash_sample(uint32_t hash, unsigned value, unsigned depth)
{
   hash = (hash ^ (value & 255)) * 16777619u;
   return depth == 8 ? hash : (hash ^ (value >> 8)) * 16777619u;
}

static void fixture_check(const uint8_t *data, size_t size,
      const uint32_t *expected, unsigned depth)
{
   rvp9_dec *decoder = (rvp9_dec *)calloc(1, sizeof(*decoder));
   size_t offset = 32;
   unsigned count = 0;
   assert(decoder && size > 44 && !memcmp(data, "DKIF", 4));
   while (offset < size)
   {
      uint32_t hash = 2166136261u, length;
      unsigned plane, x, y;
      int shown = -1;
      assert(count < 4 && size - offset >= 12);
      length = read32(data + offset);
      offset += 12;
      assert(length && length <= size - offset);
      assert(rvp9_decode_frame(decoder, data + offset, length, &shown) >= 0);
      assert(shown >= 0 && decoder->fbs[shown].w == 128
            && decoder->fbs[shown].h == 96);
      for (plane = 0; plane < 3; plane++)
      {
         const uint8_t *pixels = plane == 0 ? decoder->fbs[shown].y
               : plane == 1                 ? decoder->fbs[shown].u
                                            : decoder->fbs[shown].v;
         unsigned width = plane ? 64 : 128;
         unsigned height = plane ? 48 : 96;
         unsigned stride = plane ? decoder->uvs : decoder->ys;
         for (y = 0; y < height; y++)
            for (x = 0; x < width; x++)
            {
               size_t at = (size_t)y * stride + x;
               unsigned value
                     = depth == 8 ? pixels[at] : ((const uint16_t *)pixels)[at];
               hash = hash_sample(hash, value, depth);
            }
      }
      assert(hash == expected[count]);
      count++;
      offset += length;
   }
   assert(count == 4);
   rvp9_free(decoder);
   free(decoder);
}

#if defined(__linux__) && !defined(VAAPI_VP9_FIXTURE_ONLY)
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <time.h>
#include "../../../gfx/common/vaapi_vp9_owner.h"
#include "../vaapi_hevc/native_pixels.h"

#define VA_OK(call) assert((call) == VA_STATUS_SUCCESS)
static vaapi_vp9_owner_t *owner;
static rvp9_hw_session_t state;
static rvp9_hdr header;

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
   delay.tv_sec = 0;
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

#ifdef NATIVE_VULKAN_IMPORT
#include "../vaapi_hevc/native_import.h"
#endif

static uint32_t pixels(const vaapi_frame_view_t *view, unsigned depth,
      const video_hw_frame_t *lease)
{
   const vaapi_hevc_export_t *source = view->source;
   const vaapi_hevc_session_t *session = source->native_session;
   VADisplay display;
   VAImageFormat *formats, *selected = NULL;
   VAImage image;
   uint32_t hash = 2166136261u, checked[3];
   unsigned plane, x, y, fourcc = depth == 8 ? VA_FOURCC_NV12 : VA_FOURCC_P010;
   void *mapped = NULL;
   const uint8_t *data;
   int maximum, count, i;
   assert(source->active && source->ready && session && session->ready);
   assert(source->slot < session->slots && view->width == 128
         && view->height == 96 && !view->x && !view->y);
   display = session->display;
   maximum = vaMaxNumImageFormats(display);
   assert(maximum > 0 && maximum <= 256);
   formats = (VAImageFormat *)calloc((size_t)maximum, sizeof(*formats));
   assert(formats);
   count = maximum;
   VA_OK(vaQueryImageFormats(display, formats, &count));
   assert(count >= 0 && count <= maximum);
   for (i = 0; i < count; i++)
      if (formats[i].fourcc == fourcc
            && (depth == 8 || formats[i].byte_order == VA_LSB_FIRST))
         selected = &formats[i];
   assert(selected);
   VA_OK(vaCreateImage(display, selected, 128, 96, &image));
   free(formats);
   /* Test-only readback; the production owner exports opaque GPU leases. */
   VA_OK(vaGetImage(display, session->surfaces[source->slot], 0, 0, 128, 96,
         image.image_id));
   VA_OK(vaMapBuffer(display, image.buf, &mapped));
   data = (const uint8_t *)mapped;
   assert(image.format.fourcc == fourcc);
   assert(data && native_image_crc(&image, data, 0, 0, 128, 96, checked));
   for (plane = 0; plane < 3; plane++)
      for (y = 0; y < (plane ? 48u : 96u); y++)
         for (x = 0; x < (plane ? 64u : 128u); x++)
         {
            unsigned bytes = depth == 8 ? 1 : 2;
            size_t at = image.offsets[plane ? 1 : 0]
                  + (size_t)y * image.pitches[plane ? 1 : 0]
                  + (plane ? x * 2 + plane - 1 : x) * bytes;
            unsigned value = data[at];
            if (depth == 10)
               value = (value | ((unsigned)data[at + 1] << 8)) >> 6;
            hash = hash_sample(hash, value, depth);
         }
#ifdef NATIVE_VULKAN_IMPORT
   native_import_check(view, lease, &image, data);
#else
   (void)lease;
#endif
   VA_OK(vaUnmapBuffer(display, image.buf));
   VA_OK(vaDestroyImage(display, image.image_id));
   return hash;
}

static void return_frame(video_hw_frame_t *frame)
{
   assert(vaapi_frame_finish(
         frame->owner, frame->done, frame->done_value, NULL));
   memset(frame, 0, sizeof(*frame));
}

static int execute(const char *node, unsigned major_id, unsigned minor_id,
      const uint8_t *data, size_t size, const uint32_t *expected,
      unsigned depth)
{
   rvp9_hw_ops_t ops;
   video_color_metadata_t color;
   video_hw_frame_t held;
   vaapi_frame_view_t held_view;
   unsigned cycle, before = fd_count();
   memset(&held, 0, sizeof(held));
   assert(!rvp9_parse_frame_header(
         data + 44, read32(data + 32), NULL, NULL, &header));
   memset(&color, 0, sizeof(color));
   color.primaries = color.transfer = color.matrix = color.range = 1;
   for (cycle = 0; cycle < 2; cycle++)
   {
      size_t offset = 32;
      unsigned count = 0;
      uint64_t deadline;
      memset(&ops, 0, sizeof(ops));
      if (!vaapi_vp9_owner_open(
                &owner, node, major_id, minor_id, &header, &color, 10, &ops))
      {
         deadline = now_usec() + 10000000u;
         while (owner && !vaapi_vp9_owner_retire(owner))
            pause_until(deadline);
         owner = NULL;
         fprintf(stderr, "UNAVAILABLE: required native VA VP9 %u-bit owner\n",
               depth);
         return 77;
      }
      assert(rvp9_hw_session_init(&state, &ops, 10));
      while (offset < size)
      {
         uint32_t length;
         video_hw_frame_t frame;
         vaapi_frame_view_t view;
         int result;
         assert(count < 4 && size - offset >= 12);
         length = read32(data + offset);
         offset += 12;
         assert(length && length <= size - offset);
         deadline = now_usec() + 10000000u;
         do
         {
            result = rvp9_hw_session_submit(&state, data + offset, length);
            assert(result >= 0);
            if (result)
               pause_until(deadline);
         } while (result);
         do
         {
            result = rvp9_hw_session_output(&state, &frame);
            assert(result >= 0);
            if (result)
               pause_until(deadline);
         } while (result);
         assert(vaapi_frame_claim(
               frame.owner, frame.done, frame.done_value, frame.image, &view));
         assert(view.color.primaries == 1 && view.color.transfer == 1
               && view.color.matrix == 1 && view.color.range == 1);
         assert(pixels(&view, depth, &frame) == expected[count]);
         if (!count)
         {
            held = frame;
            held_view = view;
         }
         else
            return_frame(&frame);
         count++;
         offset += length;
      }
      assert(count == 4);
      owner = vaapi_vp9_owner_detach(&ops);
      assert(owner && !ops.decoder && !vaapi_vp9_owner_retire(owner));
      assert(pixels(&held_view, depth, &held) == expected[0]);
      return_frame(&held);
      deadline = now_usec() + 10000000u;
      while (!vaapi_vp9_owner_retire(owner))
         pause_until(deadline);
      owner = NULL;
      assert(fd_count() == before);
   }
   printf("PASS: native VA VP9 %u-bit, 8 pictures, retained exports and "
          "reopen\n",
         depth);
   return 0;
}
#endif

int main(int argc, char **argv)
{
   fixture_check(native_ivf8, sizeof(native_ivf8), native_hash8, 8);
   fixture_check(native_ivf10, sizeof(native_ivf10), native_hash10, 10);
#ifdef VAAPI_VP9_FIXTURE_ONLY
   (void)argc;
   (void)argv;
   puts("PASS: VP9 native acceptance fixtures match independent reference "
        "hashes");
   return 0;
#elif defined(__linux__)
   {
      struct stat status;
      int result;
      if (argc != 2 || stat(argv[1], &status) || !S_ISCHR(status.st_mode))
      {
         fputs("UNAVAILABLE: supply an accessible DRM render node\n", stderr);
         return 77;
      }
#ifdef NATIVE_VULKAN_IMPORT
      if (!native_import_open(major(status.st_rdev), minor(status.st_rdev)))
      {
         native_import_close();
         fputs("UNAVAILABLE: no matching Vulkan DRM modifier import device\n",
               stderr);
         return 77;
      }
#endif
      result = execute(argv[1], major(status.st_rdev), minor(status.st_rdev),
            native_ivf8, sizeof(native_ivf8), native_hash8, 8);
      if (!result)
         result = execute(argv[1], major(status.st_rdev), minor(status.st_rdev),
               native_ivf10, sizeof(native_ivf10), native_hash10, 10);
#ifdef NATIVE_VULKAN_IMPORT
      native_import_close();
      if (!result)
      {
         assert(imported == 20 && !import_errors && import_pixels);
         printf("PASS: 20 VP9 native VA modifier imports, %u RGBA pixels, max "
                "error %u\n",
               import_pixels, import_max_error);
      }
#endif
      return result;
   }
#else
   (void)argc;
   (void)argv;
   fputs("UNAVAILABLE: native VA VP9 acceptance requires Linux\n", stderr);
   return 77;
#endif
}
