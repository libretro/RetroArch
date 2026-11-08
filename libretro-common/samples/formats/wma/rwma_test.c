/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rwma_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including without
 * limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies
 * or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "../../../formats/wma/rwma_internal.h"
#include "../../../formats/wma/rwma_mdct.h"
#include "../../../formats/wma/rwma_pro_tables.h"
#include <float.h>
#include <formats/rwma.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                                       \
   do                                                                                  \
   {                                                                                   \
      if (!(x))                                                                        \
      {                                                                                \
         fprintf(stderr, "rwma line %u\n", (unsigned)__LINE__);                        \
         exit(1);                                                                      \
      }                                                                                \
   } while (0)
static unsigned allocations, fail_allocation;
static uint8_t encoded[4096];
static float history[2][768], half_mdct[512];
static float math_storage[8192 * 8], transform_input[8192], transform_output[8192];
static rwma_mdct math_state;
static unsigned bit_at;
void *rwma_test_calloc(size_t n, size_t size)
{
   allocations++;
   if (fail_allocation && allocations == fail_allocation)
      return NULL;
   return calloc(n, size);
}
void rwma_test_free(void *ptr) { free(ptr); }
static void put(unsigned value, unsigned count)
{
   unsigned i;
   CHECK(bit_at + count <= sizeof(encoded) * 8);
   for (i = count; i; i--)
   {
      if ((value >> (i - 1)) & 1)
         encoded[bit_at / 8] |= 1u << (7 - bit_at % 8);
      bit_at++;
   }
}
static void symbol(const rwma_vlc_node *nodes, unsigned count, int value)
{
   unsigned i, node = 0, parent, path = 0, bits = 0;
   for (i = 0; i < count; i++)
      if (nodes[i].next[0] < 0 && nodes[i].next[1] < 0 && nodes[i].symbol == value)
      {
         node = i;
         break;
      }
   CHECK(i < count);
   while (node)
   {
      for (parent = 0; parent < node; parent++)
         if (nodes[parent].next[0] == (int)node || nodes[parent].next[1] == (int)node)
            break;
      CHECK(parent < node && bits < 31);
      path |= (nodes[parent].next[1] == (int)node) << bits++;
      node = parent;
   }
   put(path, bits);
}
static unsigned xma_frame(unsigned channels, unsigned offset, unsigned frequency,
                          int negative, int coded)
{
   unsigned c, x, band = 1, previous = 0, start, length;
   memset(encoded, 0, sizeof(encoded));
   bit_at = offset;
   put(0, 15);
   put(1, 1);
   put(0, 1);
   if (channels == 2)
      put(0, 1);
   put(0, 8);
   put(0, 1);
   put(0, 1);
   put(0, 1);
   if (channels == 2)
   {
      put(0, 1);
      put(2, 2);
   }
   for (c = 0; c < channels; c++)
      put(coded != 0, 1);
   if (coded)
   {
      put(1, 1);
      for (c = 0; c < channels; c++)
         put(0, 8);
      put(0, 6);
      if (channels == 2)
      {
         put(0, 3);
         put(0, 1);
         put(0, 1);
      }
      for (x = 0; x < 28 && previous < 512; x++)
      {
         unsigned next = ((1024 * critical_freq[x]) / 48000 + 2) & ~3u;
         if (next > previous)
         {
            band++;
            previous = next;
         }
         if (next >= 512)
            break;
      }
      for (c = 0; c < channels; c++)
      {
         put(0, 2);
         for (x = 0; x < band - 1; x++)
            symbol(sf_vlc, RWMA_ARRAY_SIZE(sf_vlc), 0);
      }
      for (c = 0; c < channels; c++)
      {
         put(0, 1);
         symbol(coef_vlc0, RWMA_ARRAY_SIZE(coef_vlc0), 0);
         put(0, 1);
         put(1, 8);
         if (frequency == 0)
            put(0, 1);
         else if (frequency <= 4)
         {
            put(1, 1);
            put(0, 1);
            put(frequency - 1, 2);
         }
         else
         {
            put(1, 1);
            put(1, 1);
            put(0, 1);
            put(frequency - 4, 9);
         }
         put(negative ? 0 : 1, 1);
         if (frequency < 511)
            symbol(coef_vlc0, RWMA_ARRAY_SIZE(coef_vlc0), 1);
      }
   }
   put(0, 2);
   length = bit_at - offset;
   start = bit_at;
   bit_at = offset;
   put(length, 15);
   bit_at = start;
   return length;
}
static void reference(unsigned channels, unsigned frequency, int negative, int coded)
{
   unsigned c, i;
   double scale = 1.0 / 256 / 32768;
   double gain = (float)pow(10.0, 4.5);
   for (i = 0; i < 512; i++)
      half_mdct[i] =
          coded ? (float)((negative ? -gain : gain) * scale *
                          cos(RWMA_PI / 512 * (511 - i + 0.5) * (frequency + 0.5)))
                : 0;
   for (c = 0; c < channels; c++)
   {
      memcpy(history[c] + 256, half_mdct, sizeof(half_mdct));
      for (i = 0; i < 256; i++)
      {
         float a = history[c][i], b = history[c][511 - i];
         float wi = (float)sin((i + 0.5) * RWMA_PI / 1024);
         float wj = (float)sin((511 - i + 0.5) * RWMA_PI / 1024);
         history[c][i] = a * wj - b * wi;
         history[c][511 - i] = a * wi + b * wj;
      }
   }
}
static void xma_tests(void)
{
   rwma_config cfg;
   rwma_t *d;
   rwma_frame frame;
   unsigned channels, pass, count = 0, i, c, before;
   memset(&cfg, 0, sizeof(cfg));
   cfg.codec = RWMA_XMA_RAW;
   cfg.sample_rate = 48000;
   for (channels = 1; channels <= 2; channels++)
   {
      cfg.channels = channels;
      CHECK(rwma_create(&d, &cfg) == RWMA_OK);
      memset(history, 0, sizeof(history));
      before = allocations;
      for (pass = 0; pass < 128; pass++)
      {
         unsigned offset = pass % 8, frequency = (pass * 37) % 512;
         unsigned length =
             xma_frame(channels, offset, frequency, pass & 1, pass % 3 != 0);
         CHECK(rwma_decode_xma(d, encoded, (bit_at + 7) / 8, offset, length, &frame) ==
               RWMA_OK);
         CHECK(frame.samples == 512 && frame.channels == channels);
         reference(channels, frequency, pass & 1, pass % 3 != 0);
         for (c = 0; c < channels; c++)
         {
            for (i = 0; i < 512; i++)
            {
               double expected = history[c][i];
#ifdef RWMA_TEST_BAD_POLARITY
               expected = -expected;
#endif
               CHECK(fabs(frame.planes[c][i] - expected) < 0.00000003);
               count++;
            }
            memmove(history[c], history[c] + 512, 256 * sizeof(float));
         }
      }
      CHECK(allocations == before);
      rwma_reset(d);
      CHECK(allocations == before);
      rwma_free(d);
   }
   printf("PASS XMA: %u analytical PCM comparisons, arbitrary bit offsets, mono/stereo "
          "and reused overlap\n",
          count);
}
static void transform_tests(void)
{
   unsigned n, k, j;
   rwma_mdct_init(&math_state, math_storage, 8192);
   for (n = 64; n <= 8192; n *= 2)
   {
      for (j = 0; j < n; j++)
         transform_input[j] = (float)sin(j * 0.13);
      rwma_mdct_run(&math_state, transform_output, transform_input, n, 0.01f);
      for (k = 0; k < n; k++)
      {
         double value = 0;
         for (j = 0; j < n; j++)
            value += transform_input[j] * cos(RWMA_PI / n * (n - k - 0.5) * (j + 0.5));
         CHECK(fabs(transform_output[k] - value * 0.01) < 0.00001);
      }
   }
   puts("PASS radix-2 IMDCT against independent cosine sums");
}
static void failure_tests(void)
{
   rwma_config cfg;
   rwma_t *d = NULL;
   rwma_frame frame;
   unsigned i, j, channels, before, seed = 7;
   uint8_t extra[18] = {0};
   size_t used;
   int ret;
   memset(&cfg, 0, sizeof(cfg));
   cfg.codec = RWMA_XMA_RAW;
   cfg.sample_rate = 48000;
   cfg.channels = 2;
   before = allocations;
   fail_allocation = before + 1;
   CHECK(rwma_create(&d, &cfg) == RWMA_NO_MEMORY && !d);
   fail_allocation = allocations + 2;
   CHECK(rwma_create(&d, &cfg) == RWMA_NO_MEMORY && !d);
   fail_allocation = 0;
   CHECK(rwma_create(&d, &cfg) == RWMA_OK);
   before = allocations;
   for (i = 0; i < 4000; i++)
   {
      unsigned bytes = 8 + i % 248, bits = bytes * 8;
      for (j = 0; j < bytes; j++)
      {
         seed = seed * 1664525u + 1013904223u;
         encoded[j] = (uint8_t)(seed >> 24);
      }
      encoded[0] = (uint8_t)(bits >> 7);
      encoded[1] = (uint8_t)((encoded[1] & 1) | ((bits & 127) << 1));
      rwma_reset(d);
      ret = rwma_decode_xma(d, encoded, bytes, 0, bits, &frame);
      if (ret)
         CHECK(frame.samples == 0 && !frame.planes[0]);
      else
         for (channels = 0; channels < frame.channels; channels++)
            for (j = 0; j < frame.samples; j++)
               CHECK(frame.planes[channels][j] <= FLT_MAX &&
                     frame.planes[channels][j] >= -FLT_MAX);
   }
   CHECK(allocations == before);
   rwma_free(d);
   cfg.extra = extra;
   cfg.extra_size = sizeof(extra);
   cfg.block_align = 64;
   cfg.bit_rate = 128000;
   for (cfg.codec = RWMA_PRO; cfg.codec <= RWMA_V2; cfg.codec++)
   {
      memset(extra, 0, sizeof(extra));
      extra[0] = 16;
      extra[2] = 3;
      extra[14] = 0xe0;
      extra[4] = 1;
      if (cfg.codec == RWMA_PRO)
         extra[4] = 0;
      CHECK(rwma_create(&d, &cfg) == RWMA_OK);
      before = allocations;
      for (i = 0; i < 2000; i++)
      {
         for (j = 0; j < 64; j++)
         {
            seed = seed * 1664525u + 1013904223u;
            encoded[j] = (uint8_t)(seed >> 24);
         }
         rwma_reset(d);
         ret = rwma_decode_packet(d, encoded, 64, &used, &frame);
         if (ret)
            CHECK(frame.samples == 0 && !frame.planes[0]);
         else
            CHECK(used <= 64);
      }
      CHECK(allocations == before);
      rwma_free(d);
   }
   puts("PASS allocation failure, malformed frames and no decode/reset allocations");
}
static void packet_tests(void)
{
   rwma_config cfg;
   rwma_t *d;
   rwma_frame frame;
   uint8_t extra[10] = {0};
   unsigned packet, calls, frames = 0, i, c, before;
   size_t consumed;
   memset(&cfg, 0, sizeof(cfg));
   cfg.codec = RWMA_V2;
   cfg.channels = 2;
   cfg.sample_rate = 48000;
   cfg.bit_rate = 128000;
   cfg.block_align = 64;
   cfg.extra = extra;
   cfg.extra_size = sizeof(extra);
   extra[4] = 3;
   CHECK(rwma_create(&d, &cfg) == RWMA_OK);
   CHECK(rwma_decode_packet(d, NULL, 0, &consumed, &frame) == RWMA_OK
         && !frame.samples && !consumed);
   before = allocations;
   for (packet = 0; packet < 4; packet++)
   {
      memset(encoded, 0, sizeof(encoded));
      bit_at = 0;
      put(packet, 4);
      put(packet ? 2 : 3, 4);
      put(0, 13);
      for (calls = 0; calls < 8; calls++)
      {
         CHECK(rwma_decode_packet(d, encoded, 64, &consumed, &frame) == RWMA_OK);
         CHECK(frame.samples == 2048 && frame.channels == 2);
         for (c = 0; c < 2; c++)
            for (i = 0; i < frame.samples; i++)
               CHECK(frame.planes[c][i] == 0);
         frames++;
         if (consumed)
         {
            CHECK(consumed == 64);
            break;
         }
      }
      CHECK(calls == 1);
   }
   CHECK(frames == 8);
   CHECK(rwma_decode_packet(d, NULL, 0, &consumed, &frame) == RWMA_OK &&
         frame.samples == 2048);
   CHECK(rwma_decode_packet(d, NULL, 0, &consumed, &frame) == RWMA_OK &&
         !frame.samples);
   CHECK(allocations == before);
   rwma_free(d);
   puts("PASS WMA packet reservoir, borrowed per-frame output and one-shot EOF drain");
}
static void pro_tests(void)
{
   rwma_config cfg;
   rwma_t *d;
   rwma_frame frame;
   uint8_t extra[18] = {0};
   unsigned pass, c, i, frames = 0;
   memset(&cfg, 0, sizeof(cfg));
   cfg.codec = RWMA_PRO;
   cfg.channels = 2;
   cfg.sample_rate = 48000;
   cfg.block_align = 2048;
   cfg.extra = extra;
   cfg.extra_size = sizeof(extra);
   extra[0] = 16;
   extra[2] = 3;
   extra[14] = 0xd6;
   extra[15] = 0x10;
   CHECK(rwma_create(&d, &cfg) == RWMA_OK);
   {
      size_t used;
      CHECK(rwma_decode_packet(d, NULL, 0, &used, &frame) == RWMA_OK
            && !frame.samples && !used);
   }
   memset(history, 0, sizeof(history));
   for (pass = 0; pass < 32; pass++)
   {
      const uint8_t *data = encoded;
      size_t left = 2048, used;
      unsigned guard = 0, frequency = pass * 13;
      xma_frame(2, 21, frequency, pass & 1, 1);
      bit_at = 0;
      put(pass & 15, 4);
      put(0, 2);
      put(0, 15);
      reference(2, frequency, pass & 1, 1);
      while (left)
      {
         CHECK(rwma_decode_packet(d, data, left, &used, &frame) == RWMA_OK);
         CHECK(used <= left && ++guard < 8);
         if (frame.samples)
         {
            CHECK(pass != 0 && frame.samples == 512 && frame.channels == 2);
            for (c = 0; c < 2; c++)
               for (i = 0; i < 512; i++)
                  CHECK(fabs(frame.planes[c][i] - history[c][i]) < 0.00000003);
            frames++;
         }
         data += used;
         left -= used;
      }
      for (c = 0; c < 2; c++)
         memmove(history[c], history[c] + 512, 256 * sizeof(float));
   }
   CHECK(frames == 31);
   rwma_free(d);
   puts("PASS WMA Pro nonzero spectral packets, priming and overlap continuity");
}
int main(void)
{
   transform_tests();
   xma_tests();
   failure_tests();
   packet_tests();
   pro_tests();
   return 0;
}
