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
#include <formats/rav1_hw.h>

static rav1_hw_session_t state;
/* Keep parser/native probe frames separate under the stack ceiling. */
static int (*volatile submit_packet)(
      rav1_hw_session_t *, const uint8_t *, size_t) = rav1_hw_session_submit;
static rav1_sequence_t sequence;
static unsigned depth, position, grain;
static size_t offsets[4], lengths[4];
static const uint8_t *reference;
static size_t reference_size;

static uint32_t read32(const uint8_t *p)
{
   return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}
static uint32_t hash_sample(uint32_t hash, unsigned value, unsigned bits)
{
   hash = (hash ^ (value & 255)) * 16777619u;
   return bits == 8 ? hash : (hash ^ (value >> 8)) * 16777619u;
}
static uint8_t *load(const char *path, size_t *size)
{
   FILE *file = fopen(path, "rb");
   long length;
   uint8_t *data;
   assert(file && !fseek(file, 0, SEEK_END));
   length = ftell(file);
   assert(length > 0 && length <= 16 * 1024 * 1024);
   assert(!fseek(file, 0, SEEK_SET));
   *size = (size_t)length;
   data = (uint8_t *)malloc(*size);
   assert(data && fread(data, 1, *size, file) == *size && !fclose(file));
   return data;
}
static uint32_t expected_hash(unsigned index)
{
   unsigned bytes = depth == 8 ? 1 : 2;
   size_t frame_size = 128u * 96u * 3u / 2u * bytes, i;
   uint32_t hash = 2166136261u;
   assert(index < 4 && reference_size == frame_size * 4);
   for (i = index * frame_size; i < (index + 1) * frame_size; i += bytes)
   {
      unsigned value = reference[i];
      if (bytes == 2)
         value |= (unsigned)reference[i + 1] << 8;
      assert(value < (1u << depth));
      hash = hash_sample(hash, value, depth);
   }
   return hash;
}
static void fixture_check(const uint8_t *data, size_t size)
{
   size_t at = 32;
   unsigned i;
   bool found = false;
   assert(size > 44 && !memcmp(data, "DKIF", 4) && data[6] == 32 && !data[7]
         && !memcmp(data + 8, "AV01", 4) && data[12] == 128 && !data[13]
         && data[14] == 96 && !data[15]);
   for (i = 0; i < 4; i++)
   {
      size_t next = 0;
      rav1_obu_t obu;
      assert(size - at >= 12);
      lengths[i] = read32(data + at);
      at += 12;
      offsets[i] = at;
      assert(lengths[i] && lengths[i] <= size - at);
      while (rav1_obu_next(&obu, data + at, lengths[i], &next, false) == 1)
         if (obu.type == 1 && !found)
         {
            assert(rav1_sequence_parse(
                  &sequence, obu.payload, obu.payload_size));
            found = true;
         }
      assert(next == lengths[i] && expected_hash(i));
      at += lengths[i];
   }
   assert(at == size && found && !sequence.profile && !sequence.monochrome
         && sequence.bit_depth == depth && sequence.max_width == 128
         && sequence.max_height == 96 && sequence.color_primaries == 1
         && sequence.transfer_characteristics == 1
         && sequence.matrix_coefficients == 1 && !sequence.color_range
         && sequence.chroma_sample_position == position
         && sequence.film_grain_params_present == grain);
}

#if defined(__linux__) && !defined(VAAPI_AV1_FIXTURE_ONLY)
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <time.h>
#include "../../../gfx/common/vaapi_av1_owner.h"
#include "../vaapi_hevc/native_pixels.h"
#define VA_OK(call) assert((call) == VA_STATUS_SUCCESS)
static vaapi_av1_owner_t *owner;
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
static bool (*volatile open_import)(unsigned, unsigned) = native_import_open;
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

static uint32_t (*volatile check_pixels)(
      const vaapi_frame_view_t *, unsigned, const video_hw_frame_t *) = pixels;

static void return_frame(video_hw_frame_t *frame)
{
   assert(vaapi_frame_finish(
         frame->owner, frame->done, frame->done_value, NULL));
   memset(frame, 0, sizeof(*frame));
}

#else
static unsigned probe_grained;
static int probe_decode(void *context, const uint8_t *data, size_t size,
      const rav1_sequence_t *seq, const rav1_hw_picture_t *picture)
{
   assert(context == &state && data && size && seq->bit_depth == depth
         && picture->setup.size.upscaled_width == 128
         && picture->setup.size.height == 96);
   if (picture->grain.params.apply)
      probe_grained++;
   return 0;
}
static int probe_output(void *context, unsigned slot, const rav1_grain_t *g,
      video_hw_frame_t *frame)
{
   assert(context == &state && slot < 9 && g);
   memset(frame, 0, sizeof(*frame));
   frame->image = slot + 1;
   frame->width = 128;
   frame->height = 96;
   frame->pixfmt = depth == 8 ? IMAGE_PIXFMT_NV12 : IMAGE_PIXFMT_P010;
   return 0;
}
#endif

static int execute(const char *node, const uint8_t *data)
{
   rav1_hw_ops_t ops;
   unsigned cycle;
#if defined(__linux__) && !defined(VAAPI_AV1_FIXTURE_ONLY)
   struct stat status;
   video_hw_frame_t held;
   vaapi_frame_view_t held_view;
   unsigned before;
   if (stat(node, &status) || !S_ISCHR(status.st_mode))
   {
      fputs("UNAVAILABLE: accessible DRM render node required\n", stderr);
      return 77;
   }
#ifdef NATIVE_VULKAN_IMPORT
   if (!open_import(major(status.st_rdev), minor(status.st_rdev)))
   {
      native_import_close();
      fputs("UNAVAILABLE: matching Vulkan DRM import device required\n",
            stderr);
      return 77;
   }
#endif
   before = fd_count();
#else
   (void)node;
#if !defined(VAAPI_AV1_FIXTURE_ONLY)
   fputs("UNAVAILABLE: native VA AV1 acceptance requires Linux\n", stderr);
   return 77;
#endif
#endif
   for (cycle = 0; cycle < 3; cycle++)
   {
      unsigned first = cycle == 1 ? 2 : 0, i, outputs = 0;
      memset(&ops, 0, sizeof(ops));
#if defined(__linux__) && !defined(VAAPI_AV1_FIXTURE_ONLY)
      {
         video_color_metadata_t color;
         memset(&color, 0, sizeof(color));
         color.primaries = color.transfer = color.matrix = color.range = 1;
         if (!vaapi_av1_owner_open(&owner, node, major(status.st_rdev),
                   minor(status.st_rdev), &sequence, &color, 32, &ops))
         {
            uint64_t deadline = now_usec() + 10000000u;
            while (owner && !vaapi_av1_owner_retire(owner))
               pause_until(deadline);
            owner = NULL;
#ifdef NATIVE_VULKAN_IMPORT
            native_import_close();
#endif
            fputs("UNAVAILABLE: required native VA AV1 owner\n", stderr);
            return 77;
         }
      }
      memset(&held, 0, sizeof(held));
#else
      probe_grained = 0;
      ops.decoder = &state;
      ops.decode = probe_decode;
      ops.output = probe_output;
#endif
      assert(rav1_hw_session_init(&state, &sequence, &ops, 9));
      for (i = first; i < 4; i++)
      {
         size_t at = 0;
         rav1_obu_t obu;
         while (rav1_obu_next(&obu, data + offsets[i], lengths[i], &at, false)
               == 1)
         {
            video_hw_frame_t frame;
            int result;
#if defined(__linux__) && !defined(VAAPI_AV1_FIXTURE_ONLY)
            uint64_t deadline = now_usec() + 10000000u;
#endif
            if (obu.type != 3 && obu.type != 6)
               continue;
            do
            {
               result = submit_packet(&state, obu.data, obu.size);
               assert(result >= 0);
#if defined(__linux__) && !defined(VAAPI_AV1_FIXTURE_ONLY)
               if (result)
                  pause_until(deadline);
#else
               assert(!result);
#endif
            } while (result);
            if (state.output_slot < 0)
               continue;
            do
            {
               result = rav1_hw_session_output(&state, &frame);
               assert(result >= 0);
#if defined(__linux__) && !defined(VAAPI_AV1_FIXTURE_ONLY)
               if (result)
                  pause_until(deadline);
#else
               assert(!result);
#endif
            } while (result);
            assert(outputs == i - first);
#if defined(__linux__) && !defined(VAAPI_AV1_FIXTURE_ONLY)
            {
               vaapi_frame_view_t view;
               assert(vaapi_frame_claim(frame.owner, frame.done,
                     frame.done_value, frame.image, &view));
               assert(view.chroma_position == position
                     && view.color.primaries == 1 && view.color.transfer == 1
                     && view.color.matrix == 1 && view.color.range == 1);
               assert(check_pixels(&view, depth, &frame) == expected_hash(i));
               if (!outputs)
               {
                  held = frame;
                  held_view = view;
               }
               else
                  return_frame(&frame);
            }
#endif
            outputs++;
         }
         assert(at == lengths[i]);
      }
      assert(outputs == 4 - first);
#if !defined(__linux__) || defined(VAAPI_AV1_FIXTURE_ONLY)
      assert(grain ? probe_grained > 0 : probe_grained == 0);
#endif
#if defined(__linux__) && !defined(VAAPI_AV1_FIXTURE_ONLY)
      owner = vaapi_av1_owner_detach(&ops);
      assert(owner && !ops.decoder && !vaapi_av1_owner_retire(owner));
      assert(check_pixels(&held_view, depth, &held) == expected_hash(first));
      return_frame(&held);
      {
         uint64_t deadline = now_usec() + 10000000u;
         while (!vaapi_av1_owner_retire(owner))
            pause_until(deadline);
      }
      owner = NULL;
      assert(fd_count() == before);
#endif
   }
#if defined(__linux__) && !defined(VAAPI_AV1_FIXTURE_ONLY)
#ifdef NATIVE_VULKAN_IMPORT
   native_import_close();
   assert(imported == 13 && !import_errors && import_pixels);
#endif
   puts("PASS: native VA AV1 pixels, 10 displayed pictures, indexed "
        "seek/reopen, retained grain exports and FD restoration");
#else
   puts("PASS: AV1 fixture framing/reference bounds and parsed seek/reopen "
        "contracts; no native decode executed");
#endif
   return 0;
}
int main(int argc, char **argv)
{
   uint8_t *data, *raw;
   size_t size;
   int result;
#ifdef VAAPI_AV1_FIXTURE_ONLY
   puts("MODE: fixture-only");
#elif defined(NATIVE_VULKAN_IMPORT)
   puts("MODE: native-import");
#else
   puts("MODE: native-decode");
#endif
   assert(argc == 7);
   depth = (unsigned)atoi(argv[4]);
   position = (unsigned)atoi(argv[5]);
   grain = (unsigned)atoi(argv[6]);
   assert((depth == 8 || depth == 10) && position <= 2 && grain <= 1);
   data = load(argv[2], &size);
   raw = load(argv[3], &reference_size);
   reference = raw;
   fixture_check(data, size);
   result = execute(argv[1], data);
   free(raw);
   free(data);
   return result;
}
