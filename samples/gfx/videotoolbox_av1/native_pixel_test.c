/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#define _POSIX_C_SOURCE 200809L
#ifdef NDEBUG
#error Native VideoToolbox acceptance requires assertions
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/rav1_hw.h>

#if defined(__APPLE__) && defined(HAVE_THREADS)
static rav1_hw_session_t state;
/* Keep parser/native probe frames separate under the stack ceiling. */
static int (*volatile submit_packet)(
      rav1_hw_session_t *, const uint8_t *, size_t) = rav1_hw_session_submit;
#endif
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

#if defined(__APPLE__) && defined(HAVE_THREADS)
#include <dlfcn.h>
#include <features/features_cpu.h>
#include <rthreads/rthreads.h>
#include "../../../gfx/common/videotoolbox_av1_owner.h"
static videotoolbox_av1_owner_t *owner;
static retro_atomic_int_t stop;
static unsigned displayed, applied_grain, held_index;
static CVPixelBufferRef held;

static void wake(void *user)
{
   (void)user;
}
static void worker(void *user)
{
   (void)user;
   while (!retro_atomic_load_acquire_int(&stop))
      if (!videotoolbox_av1_owner_worker(owner))
         sthread_yield();
}
static uint32_t image_hash(CVPixelBufferRef image, unsigned depth)
{
   uint32_t hash = 2166136261u;
   unsigned plane, x, y;
   unsigned bytes = depth == 8 ? 1 : 2;
   assert(image && CVPixelBufferGetWidth(image) == 128
         && CVPixelBufferGetHeight(image) == 96
         && CVPixelBufferGetPlaneCount(image) == 2);
   assert(CVPixelBufferGetPixelFormatType(image)
         == (depth == 8 ? kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange
                        : 0x78343230u));
   assert(!CVPixelBufferLockBaseAddress(image, kCVPixelBufferLock_ReadOnly));
   for (plane = 0; plane < 3; plane++)
   {
      unsigned native_plane = plane ? 1 : 0;
      unsigned width = plane ? 64 : 128;
      unsigned height = plane ? 48 : 96;
      size_t stride = CVPixelBufferGetBytesPerRowOfPlane(image, native_plane);
      const uint8_t *base = (const uint8_t *)CVPixelBufferGetBaseAddressOfPlane(
            image, native_plane);
      assert(base && stride >= width * bytes * (plane ? 2 : 1));
      for (y = 0; y < height; y++)
         for (x = 0; x < width; x++)
         {
            size_t at = (size_t)y * stride
                  + (plane ? x * 2 + plane - 1 : x) * bytes;
            unsigned value = base[at];
            if (bytes == 2)
               value = ((unsigned)base[at] | ((unsigned)base[at + 1] << 8))
                     >> 6;
            hash = (hash ^ (value & 255)) * 16777619u;
            if (bytes == 2)
               hash = (hash ^ (value >> 8)) * 16777619u;
         }
   }
   assert(!CVPixelBufferUnlockBaseAddress(image, kCVPixelBufferLock_ReadOnly));
   return hash;
}

static int decode(void *context, const uint8_t *packet, size_t size,
      const rav1_sequence_t *header, const rav1_hw_picture_t *picture)
{
   bool show = picture->header.show_frame || picture->header.show_existing;
   int result;
   assert(context == owner && !memcmp(header, &sequence, sizeof(sequence)));
   result = videotoolbox_av1_owner_append(
         owner, packet, size, show, picture->slot);
   if (!result && picture->grain.params.apply)
      applied_grain++;
   return result;
}
static void release_frame(void *image, uint64_t done, uint64_t value)
{
   (void)done;
   (void)value;
   CVPixelBufferRelease((CVPixelBufferRef)image);
}
static int output(void *context, unsigned slot, const rav1_grain_t *params,
      video_hw_frame_t *out)
{
   videotoolbox_av1_result_t result;
   int status;
   assert(context == owner && params);
   status = videotoolbox_av1_owner_take(owner, slot, &result);
   if (status)
      return status;
   assert(!result.status && result.image);
   assert(image_hash(result.image, depth) == expected_hash(displayed));
   if (!held)
   {
      held = CVPixelBufferRetain(result.image);
      held_index = displayed;
   }
   memset(out, 0, sizeof(*out));
   out->owner = result.image;
   out->image = (uint64_t)(uintptr_t)result.image;
   out->abandon = release_frame;
   out->width = 128;
   out->height = 96;
   out->pixfmt = depth == 8 ? IMAGE_PIXFMT_NV12 : IMAGE_PIXFMT_P010;
   return 0;
}
static void pending(int64_t deadline)
{
   assert(cpu_features_get_time_usec() < deadline);
   sthread_yield();
}
static int execute(const uint8_t *data)
{
   video_color_metadata_t color;
   videotoolbox_av1_register_t registration = NULL;
   rav1_obu_t sequence_obu;
   size_t offset = 0;
   void *symbol
         = dlsym(RTLD_DEFAULT, "VTRegisterSupplementalVideoDecoderIfAvailable");
   unsigned cycle;
   sthread_t *thread = NULL;
   assert(sizeof(registration) == sizeof(symbol));
   memcpy(&registration, &symbol, sizeof(registration));
   while (rav1_obu_next(
                &sequence_obu, data + offsets[0], lengths[0], &offset, false)
         == 1)
      if (sequence_obu.type == 1)
         break;
   assert(sequence_obu.type == 1);
   memset(&color, 0, sizeof(color));
   color.primaries = color.transfer = color.matrix = color.range = 1;
   for (cycle = 0; cycle < 3; cycle++)
   {
      rav1_hw_ops_t ops;
      CFTypeRef hardware = NULL;
      OSStatus status;
      unsigned first = cycle == 1 ? 2 : 0, i, outputs = 0;
      int result;
      int64_t deadline;
      if (cycle == 1)
      {
         assert(owner && thread && !videotoolbox_av1_owner_restart(owner));
      }
      else
      {
         owner = videotoolbox_av1_owner_new(&sequence, &color, &sequence_obu,
               10, 1048576, registration, wake, NULL, &status);
         if (!owner)
         {
            if (status != kVTCouldNotFindVideoDecoderErr
                  && status != kVTVideoDecoderUnsupportedDataFormatErr
                  && status != kVTVideoDecoderNotAvailableNowErr)
            {
               fprintf(stderr, "FAIL: AV1 native setup: %ld\n", (long)status);
               return 1;
            }
            fprintf(stderr,
                  "UNAVAILABLE: required AV1 %u-bit hardware session: %ld\n",
                  depth, (long)status);
            return 77;
         }
         assert(!VTSessionCopyProperty(owner->session,
               kVTDecompressionPropertyKey_UsingHardwareAcceleratedVideoDecoder,
               kCFAllocatorDefault, &hardware));
         assert(hardware && CFEqual(hardware, kCFBooleanTrue));
         CFRelease(hardware);
         held = NULL;
         retro_atomic_int_init(&stop, 0);
         thread = sthread_create(worker, NULL);
         assert(thread);
      }
      memset(&ops, 0, sizeof(ops));
      ops.decoder = owner;
      ops.decode = decode;
      ops.output = output;
      assert(rav1_hw_session_init_mode(&state, &sequence, &ops, 9, true));
      applied_grain = 0;
      for (i = first; i < 4; i++)
      {
         size_t at = 0;
         rav1_obu_t obu;
         displayed = i;
         while (rav1_obu_next(&obu, data + offsets[i], lengths[i], &at, false)
               == 1)
         {
            video_hw_frame_t frame;
            if (obu.type != 3 && obu.type != 6)
               continue;
            deadline = cpu_features_get_time_usec() + 10000000;
            do
            {
               result = submit_packet(&state, obu.data, obu.size);
               assert(result >= 0);
               if (result == 1)
                  pending(deadline);
            } while (result == 1);
            if (state.output_slot < 0)
               continue;
            do
            {
               result = rav1_hw_session_output(&state, &frame);
               assert(result >= 0);
               if (result)
                  pending(deadline);
            } while (result);
            video_hw_frame_abandon(&frame);
            outputs++;
         }
         assert(at == lengths[i]);
      }
      assert(outputs == 4 - first && held);
      assert(grain ? applied_grain > 0 : applied_grain == 0);
      deadline = cpu_features_get_time_usec() + 10000000;
      do
      {
         result = videotoolbox_av1_owner_finish(owner, false);
         assert(result >= 0);
         if (result)
            pending(deadline);
      } while (result);
      if (!cycle)
      {
         assert(image_hash(held, depth) == expected_hash(held_index));
         continue;
      }
      retro_atomic_store_release_int(&stop, 1);
      sthread_join(thread);
      videotoolbox_av1_owner_close(owner);
      while (!videotoolbox_av1_owner_retire_poll(&owner))
         pending(deadline);
      assert(image_hash(held, depth) == expected_hash(held_index));
      CVPixelBufferRelease(held);
      held = NULL;
   }
   puts("PASS: native AV1 hardware pixels, grain, ten displayed pictures, "
        "warm keyframe seek/reopen and retained CoreVideo image; no Metal "
        "import "
        "claim");
   return 0;
}
#else
static int execute(const uint8_t *data)
{
   (void)data;
   fputs("UNAVAILABLE: native VideoToolbox AV1 acceptance requires Apple "
         "threads\n",
         stderr);
   return 77;
}
#endif
int main(int argc, char **argv)
{
   uint8_t *data, *raw;
   size_t size;
   int result;
   puts("MODE: native-decode");
   assert(argc == 7);
   depth = (unsigned)atoi(argv[4]);
   position = (unsigned)atoi(argv[5]);
   grain = (unsigned)atoi(argv[6]);
   assert((depth == 8 || depth == 10) && position <= 2 && grain <= 1);
   data = load(argv[2], &size);
   raw = load(argv[3], &reference_size);
   reference = raw;
   fixture_check(data, size);
   result = execute(data);
   free(raw);
   free(data);
   return result;
}
