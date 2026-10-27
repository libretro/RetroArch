/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#if defined(__APPLE__) && defined(HAVE_THREADS)
#include <assert.h>
#include <dlfcn.h>
#include <features/features_cpu.h>
#include <rthreads/rthreads.h>
#include "../../../gfx/common/videotoolbox_vp9_owner.h"
#include "native_fixture.h"

static rvp9_hdr header;
static videotoolbox_vp9_owner_t *owner;
static retro_atomic_int_t stop;

static uint32_t read32(const uint8_t *p)
{
   return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
      | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void wake(void *user)
{
   (void)user;
}

static void worker(void *user)
{
   (void)user;
   while (!retro_atomic_load_acquire_int(&stop))
      if (!videotoolbox_vp9_owner_worker(owner))
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
   assert(CVPixelBufferGetPixelFormatType(image) == (depth == 8
         ? kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange : 0x78343230u));
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
               value = ((unsigned)base[at] | ((unsigned)base[at + 1] << 8)) >> 6;
            hash = (hash ^ (value & 255)) * 16777619u;
            if (bytes == 2)
               hash = (hash ^ (value >> 8)) * 16777619u;
         }
   }
   assert(!CVPixelBufferUnlockBaseAddress(image, kCVPixelBufferLock_ReadOnly));
   return hash;
}

static int run(const uint8_t *data, size_t size, const uint32_t *expected,
      unsigned depth)
{
   video_color_metadata_t color;
   videotoolbox_vp9_result_t result;
   videotoolbox_vp9_register_t registration;
   CVPixelBufferRef held = NULL;
   unsigned cycle, count;
   size_t offset;
   OSStatus status;
   void *symbol = dlsym(RTLD_DEFAULT, "VTRegisterSupplementalVideoDecoderIfAvailable");
   memset(&registration, 0, sizeof(registration));
   assert(sizeof(registration) == sizeof(symbol));
   memcpy(&registration, &symbol, sizeof(registration));
   assert(size > 44 && !memcmp(data, "DKIF", 4));
   assert(!rvp9_parse_frame_header(data + 44, read32(data + 32),
            NULL, NULL, &header));
   memset(&color, 0, sizeof(color));
   color.primaries = color.transfer = color.matrix = color.range = 1;
   for (cycle = 0; cycle < 2; cycle++)
   {
      sthread_t *thread;
      CFTypeRef hardware = NULL;
      int64_t deadline;
      int ret;
      owner = videotoolbox_vp9_owner_new(&header, &color, 1, 10,
            65536, registration, wake, NULL, &status);
      if (!owner)
      {
         fprintf(stderr, "UNAVAILABLE: required VP9 %u-bit hardware session: %ld\n",
               depth, (long)status);
         if (held)
            CVPixelBufferRelease(held);
         return 77;
      }
      assert(!VTSessionCopyProperty(owner->session,
               kVTDecompressionPropertyKey_UsingHardwareAcceleratedVideoDecoder,
               kCFAllocatorDefault, &hardware));
      assert(hardware && CFEqual(hardware, kCFBooleanTrue));
      CFRelease(hardware);
      retro_atomic_int_init(&stop, 0);
      thread = sthread_create(worker, NULL);
      assert(thread);
      for (count = 0, offset = 32; offset < size; count++)
      {
         uint32_t length;
         assert(count < 4 && size - offset >= 12);
         length = read32(data + offset);
         offset += 12;
         assert(length && length <= size - offset);
         deadline = cpu_features_get_time_usec() + 5000000;
         do
         {
            ret = videotoolbox_vp9_owner_append(owner,
                  data + offset, length, true, 0);
            assert(ret >= 0 && cpu_features_get_time_usec() < deadline);
            if (ret)
               sthread_yield();
         } while (ret);
         do
         {
            ret = videotoolbox_vp9_owner_take(owner, 0, &result);
            assert(ret >= 0 && cpu_features_get_time_usec() < deadline);
            if (ret)
               sthread_yield();
         } while (ret);
         assert(!result.status && result.image);
         {
            uint32_t actual = image_hash(result.image, depth);
            if (actual != expected[count])
               fprintf(stderr, "VP9 %u-bit cycle %u frame %u: %08lx != %08lx\n",
                     depth, cycle, count, (unsigned long)actual,
                     (unsigned long)expected[count]);
            assert(actual == expected[count]);
         }
         if (!cycle && !count)
            held = result.image;
         else
            CVPixelBufferRelease(result.image);
         offset += length;
      }
      assert(count == 4);
      deadline = cpu_features_get_time_usec() + 5000000;
      do
      {
         ret = videotoolbox_vp9_owner_finish(owner, false);
         assert(ret >= 0 && cpu_features_get_time_usec() < deadline);
         if (ret)
            sthread_yield();
      } while (ret);
      retro_atomic_store_release_int(&stop, 1);
      sthread_join(thread);
      videotoolbox_vp9_owner_close(owner);
      do
      {
         ret = videotoolbox_vp9_owner_retire_poll(&owner);
         assert(cpu_features_get_time_usec() < deadline);
         if (!ret)
            sthread_yield();
      } while (!ret);
      assert(held && image_hash(held, depth) == expected[0]);
   }
   CVPixelBufferRelease(held);
   printf("PASS: native VP9 %u-bit hardware pixels, serialized executor, reopen and retained image after retirement\n", depth);
   return 0;
}

int main(void)
{
   int ret = run(native_ivf8, sizeof(native_ivf8), native_hash8, 8);
   if (!ret)
      ret = run(native_ivf10, sizeof(native_ivf10), native_hash10, 10);
   return ret;
}
#else
int main(void)
{
   puts("UNAVAILABLE: native VideoToolbox VP9 pixel test requires Apple threads");
   return 77;
}
#endif
