/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vulkan_texture_size_test.c).
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Regression test for the 32-bit-overflow fix in
 * gfx/drivers/vulkan.c::vulkan_create_texture's staging-buffer
 * sizing.
 *
 * Pre-fix the buffer-size calculation read:
 *
 *   uint32_t buffer_width;
 *   buffer_width      = width * vulkan_format_to_bpp(format);
 *   buffer_width      = (buffer_width + 3u) & ~3u;
 *   buffer_info.size  = buffer_width * height;
 *
 * Both multiplications are unsigned*unsigned in 32-bit, before
 * the result is implicitly widened to VkDeviceSize (uint64_t)
 * on the assignment to buffer_info.size.  With dimensions large
 * enough to wrap (e.g. width=65536, height=16385, bpp=4 ->
 * 0x1_0004_0000 truncates to 0x40000), the staging buffer was
 * allocated at the wrapped (small) size while the per-row
 * upload memcpy loop later in the same function walked the
 * full width x height.  The mapped region was smaller than
 * what the loop wrote, so the memcpy walked past the mapping
 * into adjacent heap memory.
 *
 * Reachable from libretro cores supplying oversized
 * retro_framebuffer dimensions (vulkan_get_current_sw_framebuffer
 * passes through to vulkan_create_texture), and from
 * vulkan_load_texture / vulkan_set_texture_frame, which take
 * dimensions originating in image decoders.  Particularly
 * relevant on 32-bit platforms (3DS, Vita, PSP, Wii, Wii U,
 * older Android, 32-bit Windows) where the implicit widening
 * to size_t at the next stage doesn't help either.
 *
 * Fix: compute the staging-buffer size in 64-bit, and widen
 * the upload loop's stride and per-row copy size to size_t.
 *
 * IMPORTANT: this test keeps verbatim copies of the post-fix
 * size calculation and upload-loop arithmetic from
 * gfx/drivers/vulkan.c.  If vulkan_create_texture amends them,
 * the copies below must follow.  Convention used by the
 * security regression tests in samples/tasks/.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Minimal mirror of the bpp lookup from vulkan.c.  Only the
 * formats actually used by the production path matter. */
static unsigned vulkan_format_to_bpp_mock(unsigned format_id)
{
   switch (format_id)
   {
      case 0:  return 4;  /* B8G8R8A8_UNORM */
      case 1:  return 2;  /* R5G6B5_UNORM_PACK16 */
      case 2:  return 2;  /* B4G4R4A4 etc. */
      case 3:  return 1;  /* R8_UNORM */
      default: return 0;
   }
}

/* === verbatim copy of the post-fix size calculation from
 *     gfx/drivers/vulkan.c::vulkan_create_texture.  If the
 *     production function amends this arithmetic, the copy
 *     must follow. === */
static uint64_t compute_buffer_size(unsigned width, unsigned height,
      unsigned format_id)
{
   uint64_t buffer_size_64;
   uint32_t buffer_width;

   buffer_width   = width * vulkan_format_to_bpp_mock(format_id);
   buffer_width   = (buffer_width + 3u) & ~3u;
   buffer_size_64 = (uint64_t)buffer_width * (uint64_t)height;

   return buffer_size_64;
}
/* === end verbatim copy === */

/* === verbatim copy of the post-fix upload-loop size calculations
 *     from vulkan_create_texture, the STREAMED/STAGING case.  The
 *     production loop is:
 *         size_t stride    = (size_t)tex.width * (size_t)bpp;
 *         size_t row_bytes = (size_t)width     * (size_t)bpp;
 *         for (y = 0; y < tex.height; ...; src += stride)
 *            memcpy(dst, src, row_bytes);
 *     If vulkan.c amends, this copy must follow. === */
static void compute_upload_strides(unsigned tex_width, unsigned width,
      unsigned bpp, size_t *stride_out, size_t *row_bytes_out)
{
   *stride_out    = (size_t)tex_width * (size_t)bpp;
   *row_bytes_out = (size_t)width     * (size_t)bpp;
}
/* === end verbatim copy === */

static int failures = 0;

/* Probe: typical small dimensions, no risk of any overflow.
 * Verifies the post-fix arithmetic agrees with the obvious
 * calculation. */
static void test_typical_dimensions(void)
{
   unsigned w = 320, h = 240, fmt = 0;  /* RGBA8888 */
   uint64_t got      = compute_buffer_size(w, h, fmt);
   uint64_t expected = (uint64_t)((w * 4 + 3) & ~3u) * (uint64_t)h;

   if (got != expected)
   {
      printf("[ERROR] typical 320x240: got %llu, expected %llu\n",
            (unsigned long long)got, (unsigned long long)expected);
      failures++;
   }
   else
      printf("[SUCCESS] typical 320x240: size=%llu bytes\n",
            (unsigned long long)got);
}

/* Probe: 4096x2160 (4K) RGBA8888 -- common modern resolution.
 * Result is 35 MiB-ish, well below 32-bit wrap, but exercises
 * a realistically-large legitimate case to confirm the cast
 * widening doesn't reject anything that should pass. */
static void test_4k_rgba(void)
{
   unsigned w = 4096, h = 2160, fmt = 0;
   uint64_t got      = compute_buffer_size(w, h, fmt);
   uint64_t expected = (uint64_t)((w * 4 + 3) & ~3u) * (uint64_t)h;

   if (got != expected)
   {
      printf("[ERROR] 4K RGBA: got %llu, expected %llu\n",
            (unsigned long long)got, (unsigned long long)expected);
      failures++;
   }
   else
      printf("[SUCCESS] 4K RGBA 4096x2160: size=%llu bytes (%.1f MiB)\n",
            (unsigned long long)got, (double)got / (1024.0 * 1024.0));
}

/* Probe: dimensions that wrap a 32-bit unsigned multiplication.
 * width=65536, height=16385, bpp=4: 65536*4 = 262144 (fine);
 * 262144*16385 = 0x1_0004_0000 (33 bits).  Pre-fix this
 * truncated to 0x40000 (256 KiB).  Post-fix it stays
 * 0x100040000 = 4_295_229_440.  This is the smoking-gun test:
 * verify the result exceeds UINT32_MAX. */
static void test_overflow_wrap_case(void)
{
   const unsigned w = 65536, h = 16385, fmt = 0;
   const uint32_t prefix_buffer_width   = (w * 4 + 3) & ~3u;
   /* What pre-fix arithmetic would have produced (kept locally
    * for the assertion only -- never used to size anything). */
   const uint32_t prefix_size_truncated = prefix_buffer_width * h;
   const uint64_t expected_full         = (uint64_t)prefix_buffer_width * (uint64_t)h;

   uint64_t got = compute_buffer_size(w, h, fmt);

   if (got != expected_full)
   {
      printf("[ERROR] 65536x16385: got %llu, expected %llu\n",
            (unsigned long long)got,
            (unsigned long long)expected_full);
      failures++;
      return;
   }
   if (got <= UINT32_MAX)
   {
      printf("[ERROR] 65536x16385 should produce %llu (>UINT32_MAX), got %llu\n",
            (unsigned long long)expected_full,
            (unsigned long long)got);
      failures++;
      return;
   }

   printf("[SUCCESS] 65536x16385x4 = %llu bytes (pre-fix would truncate to %u)\n",
         (unsigned long long)got, prefix_size_truncated);
}

/* Probe: multiple wrap-triggering shapes.  Each pair has
 * w*bpp*h > UINT32_MAX. */
static void test_assorted_overflow_shapes(void)
{
   struct {
      unsigned w, h, fmt;
      const char *desc;
   } cases[] = {
      { 65536, 16385, 0, "65536x16385 RGBA8888" },
      { 32768, 32769, 0, "32768x32769 RGBA8888" },
      { 65536, 65535, 1, "65536x65535 RGB565 (2bpp)" },
      { 65536, 65536, 0, "65536x65536 RGBA8888 (16 GiB)" }
   };
   const unsigned n = sizeof(cases) / sizeof(cases[0]);
   unsigned i;
   bool any_fail = false;

   for (i = 0; i < n; i++)
   {
      const uint32_t bpp      = vulkan_format_to_bpp_mock(cases[i].fmt);
      const uint32_t bw       = (cases[i].w * bpp + 3) & ~3u;
      const uint64_t expected = (uint64_t)bw * (uint64_t)cases[i].h;
      uint64_t got = compute_buffer_size(cases[i].w, cases[i].h, cases[i].fmt);

      if (got != expected)
      {
         printf("[ERROR] %s: got %llu, expected %llu\n",
               cases[i].desc,
               (unsigned long long)got,
               (unsigned long long)expected);
         failures++;
         any_fail = true;
      }
   }
   if (!any_fail)
      printf("[SUCCESS] all %u overflow shapes computed correctly in 64-bit\n",
            n);
}

/* Probe: per-row upload arithmetic.  Mimics the inner loop's
 * memcpy length and source-stride calculation under an
 * AddressSanitizer-instrumented allocation that's exactly the
 * computed buffer size.
 *
 * If the post-fix `(size_t)w * (size_t)bpp` is reverted to
 * `unsigned w * unsigned bpp`, the row_bytes value would wrap
 * to a small number, the memcpy would copy too little, and the
 * per-row pointer math (dst += stride) would also wrap.  Test
 * with a moderately large but not absurd shape to keep the
 * malloc tractable. */
static void test_upload_loop_strides(void)
{
   /* 2048x16 RGBA8888 -- 128 KiB total, easy to alloc.
    * Each row is 8192 bytes, 16 rows. */
   const unsigned w   = 2048, h = 16;
   const unsigned bpp = 4;
   const size_t expected_stride = (size_t)w * (size_t)bpp;
   const size_t total_size      = expected_stride * (size_t)h;
   size_t stride, row_bytes;
   uint8_t *buf;
   uint8_t *src;
   unsigned y;

   compute_upload_strides(w, w, bpp, &stride, &row_bytes);

   if (stride != expected_stride || row_bytes != expected_stride)
   {
      printf("[ERROR] upload strides: stride=%zu row_bytes=%zu, expected %zu\n",
            stride, row_bytes, expected_stride);
      failures++;
      return;
   }

   /* Allocate destination + source at the computed size.  ASan
    * instruments both; the per-row memcpy walks every byte in
    * the destination and reads every byte in the source. */
   buf = (uint8_t*)malloc(total_size);
   src = (uint8_t*)malloc(total_size);
   if (!buf || !src)
   {
      printf("[ERROR] malloc failed (size=%zu)\n", total_size);
      free(buf); free(src);
      failures++;
      return;
   }

   memset(src, 0xA5, total_size);
   memset(buf, 0,    total_size);

   /* Mirror the production upload loop. */
   {
      uint8_t       *dst    = buf;
      const uint8_t *srcptr = src;
      for (y = 0; y < h; y++, dst += stride, srcptr += stride)
         memcpy(dst, srcptr, row_bytes);
   }

   /* Spot-check first and last byte and each row boundary --
    * if the pointer math wrapped, these would be wrong. */
   if (   buf[0]                             != 0xA5
       || buf[total_size - 1]                != 0xA5
       || buf[(h - 1) * expected_stride]     != 0xA5
       || buf[(h - 1) * expected_stride + 1] != 0xA5)
   {
      printf("[ERROR] upload loop produced inconsistent buffer contents\n");
      failures++;
   }
   else
      printf("[SUCCESS] upload loop: %u rows of %zu bytes, no OOB writes\n",
            h, row_bytes);

   free(buf);
   free(src);
}

/* Probe: confirm the harness arithmetic itself isn't silently
 * collapsing to 32-bit.  16 GiB result expected. */
static void test_size_holds_full_64bit(void)
{
   uint64_t got = compute_buffer_size(65536, 65536, 0);
   uint64_t expected = (uint64_t)((65536u * 4u + 3u) & ~3u) * 65536ull;
   if (got != expected)
   {
      printf("[ERROR] 65536x65536 RGBA: got %llu, harness expected %llu\n",
            (unsigned long long)got,
            (unsigned long long)expected);
      failures++;
      return;
   }
   /* 65536 * 4 = 262144, * 65536 = 17_179_869_184 = 16 GiB. */
   if (got != 17179869184ull)
   {
      printf("[ERROR] 65536x65536 RGBA: expected 16 GiB exactly, got %llu\n",
            (unsigned long long)got);
      failures++;
      return;
   }
   printf("[SUCCESS] size holds full 64-bit: 65536x65536 = 16 GiB\n");
}

int main(void)
{
   test_typical_dimensions();
   test_4k_rgba();
   test_overflow_wrap_case();
   test_assorted_overflow_shapes();
   test_upload_loop_strides();
   test_size_holds_full_64bit();

   if (failures)
   {
      printf("\n%d vulkan_texture_size test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll vulkan_texture_size regression tests passed.\n");
   return 0;
}
