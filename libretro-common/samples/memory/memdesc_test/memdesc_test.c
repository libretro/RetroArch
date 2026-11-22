/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (memdesc_test.c).
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* memdesc_preprocess and memdesc_find on SNES-style maps, on the
 * refusals and on an empty map: each address must reach the descriptor
 * and offset that RetroArch's own reading of the map gives. */

#include <stdio.h>
#include <string.h>

#include <memory/memdesc.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

static int failures;
static unsigned char host[1];

static void set(struct retro_memory_descriptor *d, size_t start,
      size_t select, size_t disconnect, size_t len, size_t offset)
{
   memset(d, 0, sizeof(*d));
   d->ptr        = host;
   d->start      = start;
   d->select     = select;
   d->disconnect = disconnect;
   d->len        = len;
   d->offset     = offset;
}

/* Claims address 0 only, with no memory behind it. */
static void set_terminator(struct retro_memory_descriptor *d)
{
   memset(d, 0, sizeof(*d));
   d->select = 0xFFFFFF;
}

static int at(const struct retro_memory_descriptor *d, unsigned n,
      size_t address, size_t *off)
{
   const struct retro_memory_descriptor *desc =
      memdesc_find(d, n, address, off);
   return desc ? (int)(desc - d) : -1;
}

static void expect_bool(const char *test, const char *what,
      bool got, bool want)
{
   if (got != want)
   {
      printf("FAIL %s: %s is %d, want %d\n", test, what, got, want);
      failures++;
   }
}

static void expect_size(const char *test, const char *what,
      size_t got, size_t want)
{
   if (got != want)
   {
      printf("FAIL %s: %s is 0x%lx, want 0x%lx\n", test, what,
            (unsigned long)got, (unsigned long)want);
      failures++;
   }
}

static void expect_at(const char *test,
      const struct retro_memory_descriptor *d, unsigned n,
      size_t address, int want, size_t want_off)
{
   size_t off = 0;
   int    got = at(d, n, address, &off);

   if (got != want)
   {
      printf("FAIL %s: 0x%06lx is claimed by %d, want %d\n", test,
            (unsigned long)address, got, want);
      failures++;
   }
   else if (got >= 0 && off != want_off)
   {
      printf("FAIL %s: 0x%06lx is at offset 0x%lx, want 0x%lx\n", test,
            (unsigned long)address, (unsigned long)off,
            (unsigned long)want_off);
      failures++;
   }
}

static void test_wram(void)
{
   const char *t = "test_wram";
   struct retro_memory_descriptor d[4];

   set(&d[0], 0x7E0000, 0,        0, 0x20000, 0);
   set(&d[1], 0,        0xC0E000, 0, 0x2000,  0);
   set(&d[2], 0x800000, 0xC0E000, 0, 0x2000,  0);
   set_terminator(&d[3]);

   expect_bool(t, "prep", memdesc_preprocess(d, COUNT(d)), true);
   /* The top address, 0xFFFFFF, shows in the select made from len. */
   expect_size(t, "d0.select",     d[0].select,     0xFE0000);
   expect_size(t, "d1.disconnect", d[1].disconnect, 0x3F0000);
   expect_size(t, "d2.disconnect", d[2].disconnect, 0x3F0000);
   expect_size(t, "d3.len",        d[3].len,        1);

   expect_at(t, d, COUNT(d), 0x7E1234,  0, 0x1234);
   expect_at(t, d, COUNT(d), 0x7FFFFF,  0, 0x1FFFF);
   expect_at(t, d, COUNT(d), 0x001234,  1, 0x1234);
   expect_at(t, d, COUNT(d), 0x3F1FFF,  1, 0x1FFF);
   expect_at(t, d, COUNT(d), 0xBF1FFF,  2, 0x1FFF);
   expect_at(t, d, COUNT(d), 0x002000, -1, 0);
   expect_at(t, d, COUNT(d), 0x402000, -1, 0);
}

static void test_wram_alternate(void)
{
   const char *t = "test_wram_alternate";
   struct retro_memory_descriptor d[2];

   set(&d[0], 0, 0x40E000, ~(size_t)0x1FFF, 0, 0);
   set_terminator(&d[1]);

   expect_bool(t, "prep", memdesc_preprocess(d, COUNT(d)), true);
   expect_size(t, "d0.len", d[0].len, 0x2000);

   expect_at(t, d, COUNT(d), 0x801234,  0, 0x1234);
   expect_at(t, d, COUNT(d), 0x3F1FFF,  0, 0x1FFF);
   expect_at(t, d, COUNT(d), 0x402000, -1, 0);
}

static void test_lorom(void)
{
   const char *t = "test_lorom";
   struct retro_memory_descriptor d[3];

   set(&d[0], 0x008000, 0x408000, 0x8000, 0x80000, 0);
   set(&d[1], 0x400000, 0x400000, 0x8000, 0x80000, 0);
   set_terminator(&d[2]);

   expect_bool(t, "prep", memdesc_preprocess(d, COUNT(d)), true);
   expect_size(t, "d0.disconnect", d[0].disconnect, 0xB08000);

   expect_at(t, d, COUNT(d), 0x008000,  0, 0);
   expect_at(t, d, COUNT(d), 0x00FFFF,  0, 0x7FFF);
   expect_at(t, d, COUNT(d), 0x018000,  0, 0x8000);
   expect_at(t, d, COUNT(d), 0x0FFFFF,  0, 0x7FFFF);
   expect_at(t, d, COUNT(d), 0x108000,  0, 0);
   expect_at(t, d, COUNT(d), 0x408000,  1, 0);
   expect_at(t, d, COUNT(d), 0x7F8000,  1, 0x78000);
   expect_at(t, d, COUNT(d), 0x808000,  0, 0);
   expect_at(t, d, COUNT(d), 0x000000,  2, 0);
   expect_at(t, d, COUNT(d), 0x007FFF, -1, 0);
}

static void test_hirom(void)
{
   const char *t = "test_hirom";
   struct retro_memory_descriptor d[3];

   set(&d[0], 0x400000, 0x400000, 0, 0x400000, 0);
   set(&d[1], 0x008000, 0x408000, 0, 0x400000, 0x8000);
   set_terminator(&d[2]);

   expect_bool(t, "prep", memdesc_preprocess(d, COUNT(d)), true);
   expect_size(t, "d0.disconnect", d[0].disconnect, 0x800000);

   expect_at(t, d, COUNT(d), 0x008000,  1, 0);
   expect_at(t, d, COUNT(d), 0x3FFFFF,  1, 0x3F7FFF);
   expect_at(t, d, COUNT(d), 0x400000,  0, 0);
   expect_at(t, d, COUNT(d), 0x7FFFFF,  0, 0x3FFFFF);
   expect_at(t, d, COUNT(d), 0xC00000,  0, 0);
   expect_at(t, d, COUNT(d), 0x808000,  1, 0);
   expect_at(t, d, COUNT(d), 0x007FFF, -1, 0);
}

/* A disconnect bit below len doubles the window and becomes a select
 * bit. */
static void test_select0_disconnect(void)
{
   const char *t = "test_select0_disconnect";
   struct retro_memory_descriptor d[1];

   set(&d[0], 0x60000000UL, 0, 2, 8, 8);

   expect_bool(t, "prep", memdesc_preprocess(d, COUNT(d)), true);
   expect_size(t, "d0.select", d[0].select, 0x7FFFFFF2UL);

   expect_at(t, d, COUNT(d), 0x60000001UL,  0, 1);
   expect_at(t, d, COUNT(d), 0x60000004UL,  0, 2);
   expect_at(t, d, COUNT(d), 0x60000008UL,  0, 4);
   expect_at(t, d, COUNT(d), 0x6000000CUL,  0, 6);
   expect_at(t, d, COUNT(d), 0x6000000DUL,  0, 7);
   expect_at(t, d, COUNT(d), 0x60000002UL, -1, 0);
   expect_at(t, d, COUNT(d), 0x6000000FUL, -1, 0);
   expect_at(t, d, COUNT(d), 0x60000010UL, -1, 0);
}

static void test_refused(void)
{
   const char *t = "test_refused";
   struct retro_memory_descriptor d[2];

   /* len is not a power of two: refused at d0, so d1 stays raw. */
   set(&d[0], 0x400000, 0, 0, 0x300000, 0);
   set(&d[1], 0x7E0000, 0, 0, 0x20000,  0);
   expect_bool(t, "non-power-of-two prep", memdesc_preprocess(d, 2), false);
   expect_size(t, "non-power-of-two d0.select", d[0].select, 0);
   expect_size(t, "non-power-of-two d1.select", d[1].select, 0);

   /* start has bits outside the select just written. */
   set(&d[0], 0x7E1000, 0, 0, 0x2000, 0);
   expect_bool(t, "unaligned prep", memdesc_preprocess(d, 1), false);
   expect_size(t, "unaligned d0.select", d[0].select, 0x7FE000);

   set(&d[0], 0x8100, 0x408000, 0, 0x8000, 0);
   expect_bool(t, "start outside select prep",
         memdesc_preprocess(d, 1), false);

   set(&d[0], 0, 0, 0, 0, 0);
   expect_bool(t, "empty prep", memdesc_preprocess(d, 1), false);

   /* Never preprocessed: select 0 matches start..start+len as is. */
   set(&d[0], 0x7E0000, 0, 2, 0x20000, 0);
   expect_at(t, d, 1, 0x7E0002, 0, 2);
}

static void test_empty_map(void)
{
   const char *t = "test_empty_map";
   size_t    off = 0;

   expect_bool(t, "prep", memdesc_preprocess(NULL, 0), true);
   expect_bool(t, "found", memdesc_find(NULL, 0, 0x7E0000, &off) != NULL,
         false);
}

int main(void)
{
   test_wram();
   test_wram_alternate();
   test_lorom();
   test_hirom();
   test_select0_disconnect();
   test_refused();
   test_empty_map();

   if (failures)
   {
      printf("memdesc_test: %d failures\n", failures);
      return 1;
   }

   printf("PASS memdesc_test\n");
   return 0;
}
