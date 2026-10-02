/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rinflate_window_test.c).
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
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Output-buffer handling of the built-in inflater (encodings/deflate.h).
 *
 * A stream full of back-references reaching up to 32 KiB is compressed
 * with rdeflate and decoded with rinflate in the ways callers drive it:
 *
 *   whole     - one output buffer for the whole stream, input fed in
 *               slices: the buffer stays bound across many calls.
 *   window    - an output window bound once and kept across many
 *               calls, rebound only when it fills (how the ZIP backend
 *               streams a member to disk): back-references from the
 *               new window into the old one must resolve.
 *   slice     - a fresh output slice bound before every call.
 *   dict      - the window lane after a dictionary primed the ring.
 *
 * Every lane compares the decoded bytes with the original.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <encodings/deflate.h>

#define DATA_SIZE (600 * 1024)

static unsigned failures = 0;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
         failures++; \
      } \
   } while (0)

static uint32_t lcg_state = 0x1234567u;

static uint32_t lcg(void)
{
   lcg_state = lcg_state * 1103515245u + 12345u;
   return lcg_state >> 8;
}

/* Runs of random bytes, copies from up to 32 KiB back and short-period
 * repeats, so the stream has matches at every distance class */
static void build_data(uint8_t *d, size_t len)
{
   size_t pos = 0;

   while (pos < len)
   {
      size_t run  = 16 + lcg() % 512;
      unsigned op = lcg() % 4;
      size_t i;

      if (run > len - pos)
         run = len - pos;

      if (op == 0 || pos < 1024)
      {
         for (i = 0; i < run; i++)
            d[pos + i] = (uint8_t)lcg();
      }
      else if (op == 3)
      {
         size_t period = 1 + lcg() % 7;
         for (i = 0; i < run; i++)
            d[pos + i] = (i < period) ? (uint8_t)lcg() : d[pos + i - period];
      }
      else
      {
         size_t max  = pos < 32768 ? pos : 32768;
         size_t dist = 1 + lcg() % max;
         for (i = 0; i < run; i++)
            d[pos + i] = d[pos + i - dist];
      }
      pos += run;
   }
}

static bool compress_raw(const uint8_t *in, size_t in_len,
      uint8_t **out, size_t *out_len)
{
   size_t cap  = in_len + in_len / 8 + 4096;
   size_t used = 0;
   void *z     = rdeflate_new(6, -15);
   uint8_t *buf;

   if (!z)
      return false;
   if (!(buf = (uint8_t*)malloc(cap)))
   {
      rdeflate_free(z);
      return false;
   }

   rdeflate_set_in(z, in, in_len);
   rdeflate_finish(z);
   for (;;)
   {
      size_t r = 0, w = 0;
      int st;

      rdeflate_set_out(z, buf + used, cap - used);
      st    = rdeflate_process(z, &r, &w);
      used += w;
      if (st == RDEFLATE_PROCESS_END)
         break;
      if (st == RDEFLATE_PROCESS_ERROR || used == cap)
      {
         free(buf);
         rdeflate_free(z);
         return false;
      }
   }

   rdeflate_free(z);
   *out     = buf;
   *out_len = used;
   return true;
}

/* Decodes @comp into @dst through an output window of @win bytes fed
 * @in_slice bytes of input at a time.  With @rebind_each_call the
 * window is bound before every call; otherwise it stays bound until it
 * fills.  @skip leading bytes of the original are primed as a
 * dictionary instead of decoded.  Returns the bytes decoded, or
 * (size_t)-1 on a decoder error or a stream that does not end. */
static size_t decode(const uint8_t *comp, size_t comp_len,
      uint8_t *dst, size_t dst_len, size_t win, size_t in_slice,
      bool rebind_each_call, const uint8_t *dict, size_t dict_len)
{
   size_t total  = 0;
   size_t filled = 0;
   size_t in_pos = 0;
   bool ended    = false;
   void *z       = rinflate_new(-15);
   uint8_t *wbuf = (uint8_t*)malloc(win);

   if (!z || !wbuf)
   {
      free(wbuf);
      rinflate_free(z);
      return (size_t)-1;
   }

   if (dict)
      rinflate_set_dictionary(z, dict, dict_len);
   rinflate_set_out(z, wbuf, win);

   while (!ended && in_pos < comp_len)
   {
      size_t n = comp_len - in_pos;
      if (n > in_slice)
         n = in_slice;
      rinflate_set_in(z, comp + in_pos, n);
      in_pos += n;

      for (;;)
      {
         size_t r = 0, w = 0;
         int st;

         if (filled == win || (rebind_each_call && filled))
         {
            if (total + filled > dst_len)
               goto fail;
            memcpy(dst + total, wbuf, filled);
            total += filled;
            filled = 0;
            /* Overwritten before the next call, so a decoder reading
             * history back out of the old window sees garbage */
            memset(wbuf, 0xA5, win);
            rinflate_set_out(z, wbuf, win);
         }

         st      = rinflate_process(z, &r, &w);
         filled += w;
         if (st == RDEFLATE_PROCESS_ERROR)
            goto fail;
         if (st == RDEFLATE_PROCESS_END)
         {
            ended = true;
            break;
         }
         if (r == 0 && w == 0)
            break;
      }
   }

   if (!ended || total + filled > dst_len)
      goto fail;
   memcpy(dst + total, wbuf, filled);
   total += filled;

   free(wbuf);
   rinflate_free(z);
   return total;

fail:
   free(wbuf);
   rinflate_free(z);
   return (size_t)-1;
}

static void lane(const char *name, const uint8_t *comp, size_t comp_len,
      const uint8_t *orig, size_t orig_len, uint8_t *dst,
      size_t win, size_t in_slice, bool rebind_each_call)
{
   size_t got;
   bool same;

   memset(dst, 0, orig_len);
   got  = decode(comp, comp_len, dst, orig_len, win, in_slice,
         rebind_each_call, NULL, 0);
   same = (got == orig_len) && !memcmp(dst, orig, orig_len);
   printf("[%-6s] window %7lu, input slices %6lu: %s\n", name,
         (unsigned long)win, (unsigned long)in_slice,
         same ? "matches" : "differs");
   CHECK(same, "decoded bytes differ from the original");
}

int main(void)
{
   static const size_t windows[] = { 32768, 65536, 100003, 262144 };
   static const size_t slices[]  = { 4096, 131072 };
   uint8_t *orig = (uint8_t*)malloc(DATA_SIZE);
   uint8_t *dst  = (uint8_t*)malloc(DATA_SIZE);
   uint8_t *comp = NULL;
   size_t comp_len = 0;
   size_t i, j;

   if (!orig || !dst)
   {
      fprintf(stderr, "FAIL: out of memory\n");
      return 1;
   }

   build_data(orig, DATA_SIZE);
   if (!compress_raw(orig, DATA_SIZE, &comp, &comp_len))
   {
      fprintf(stderr, "FAIL: could not compress the fixture\n");
      return 1;
   }
   printf("fixture: %lu bytes, %lu compressed\n",
         (unsigned long)DATA_SIZE, (unsigned long)comp_len);
   CHECK(comp_len < DATA_SIZE * 3 / 4, "fixture has too few matches");

   for (j = 0; j < sizeof(slices) / sizeof(slices[0]); j++)
      lane("whole", comp, comp_len, orig, DATA_SIZE, dst,
            DATA_SIZE, slices[j], false);

   for (i = 0; i < sizeof(windows) / sizeof(windows[0]); i++)
      for (j = 0; j < sizeof(slices) / sizeof(slices[0]); j++)
         lane("window", comp, comp_len, orig, DATA_SIZE, dst,
               windows[i], slices[j], false);

   lane("slice", comp, comp_len, orig, DATA_SIZE, dst, 1000, 4096, true);

   /* dict: a primed ring ahead of the stream must not shift where the
    * stream's own history lies once the window is rebound */
   {
      size_t got;
      memset(dst, 0, DATA_SIZE);
      got = decode(comp, comp_len, dst, DATA_SIZE, 65536, 4096, false,
            orig, 4096);
      printf("[dict  ] window   65536, input slices   4096: %s\n",
            (got == DATA_SIZE && !memcmp(dst, orig, DATA_SIZE))
            ? "matches" : "differs");
      CHECK(got == DATA_SIZE && !memcmp(dst, orig, DATA_SIZE),
            "decoded bytes differ after a dictionary");
   }

   free(comp);
   free(orig);
   free(dst);

   if (failures)
   {
      fprintf(stderr, "%u check(s) failed\n", failures);
      return 1;
   }
   printf("[pass] rinflate_window_test\n");
   return 0;
}
