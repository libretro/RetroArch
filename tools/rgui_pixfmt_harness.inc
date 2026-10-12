/* The harness tools/rgui_pixfmt_check.c compiles around the pixel
 * format conversion block it lifts out of menu/drivers/rgui.c (written
 * to rgui_pixfmt_block.h in a scratch directory). Not built on its
 * own. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_inline.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static const char *g_ident = "";

static const char *video_driver_get_ident(void)
{
   return g_ident;
}

static bool string_is_equal(const char *a, const char *b)
{
   return strcmp(a, b) == 0;
}

/* ---- lifted verbatim from menu/drivers/rgui.c ---- */
#include "rgui_pixfmt_block.h"
/* ---- end lifted region ---- */

typedef uint16_t (*conv_t)(uint32_t);
typedef uint16_t (*convd_t)(uint32_t, unsigned, unsigned);

static const char *g_chan[4] = { "alpha", "red", "green", "blue" };
static const unsigned g_shift[4] = { 24, 16, 8, 0 };

static int g_failures = 0;

/* Build a test colour that varies only channel `ch`.
 *
 * The RGB sweeps hold alpha opaque so the formats that premultiply
 * (1555, 565, 5A3) are not also being swept through their alpha
 * fade.  The alpha sweep holds RGB at zero, which stays zero under
 * premultiplication - so whatever moves is alpha's own field, if it
 * has one, and nothing else. */
static uint32_t test_colour(int ch, unsigned v)
{
   if (ch == 0)
      return (uint32_t)v << 24;
   return (uint32_t)0xFF000000u | ((uint32_t)v << g_shift[ch]);
}

/* Bits that CHANGE as one channel sweeps its full range.  Bits that
 * are merely set - a forced alpha bit, a pinned neighbour - are held
 * constant and correctly do not count as this channel's. */
static uint16_t reach_plain(conv_t f, int ch)
{
   uint16_t base = 0;
   uint16_t acc  = 0;
   unsigned v;

   for (v = 0; v < 256; v++)
   {
      uint16_t o = f(test_colour(ch, v));
      if (v == 0)
         base = o;
      acc |= (uint16_t)(o ^ base);
   }
   return acc;
}

static uint16_t reach_dither(convd_t f, int ch)
{
   uint16_t base = 0;
   uint16_t acc  = 0;
   unsigned v, x, y;

   for (v = 0; v < 256; v++)
      for (y = 0; y < 4; y++)
         for (x = 0; x < 4; x++)
         {
            uint16_t o = f(test_colour(ch, v), x, y);
            if (v == 0 && x == 0 && y == 0)
               base = o;
            acc |= (uint16_t)(o ^ base);
         }
   return acc;
}

static unsigned first_bit(uint16_t mask)
{
   unsigned i;
   for (i = 0; i < 16; i++)
      if (mask & (1u << i))
         return i;
   return 0;
}

static int is_contiguous(uint16_t mask)
{
   /* A contiguous run, shifted down, is one less than a power of two. */
   uint16_t m = (uint16_t)(mask >> first_bit(mask));
   return (m & (uint16_t)(m + 1)) == 0;
}

static unsigned field_of(uint16_t pixel, uint16_t mask)
{
   return (unsigned)((pixel & mask) >> first_bit(mask));
}

static void report(const char *ident, const char *msg)
{
   printf("[fail] %-11s %s\n", ident, msg);
   g_failures++;
}

static void check_ident(const char *ident)
{
   char buf[256];
   conv_t  plain;
   convd_t dith;
   uint16_t rp[4], rd[4];
   int ch, other;

   int before = g_failures;

   g_ident = ident;
   (void)rgui_set_pixel_format_function();
   plain = argb32_to_pixel_platform_format;
   dith  = argb32_to_pixel_platform_format_dither;

   for (ch = 0; ch < 4; ch++)
   {
      rp[ch] = reach_plain(plain, ch);
      rd[ch] = reach_dither(dith, ch);
   }

   /* 1. Channels must not share bits. */
   for (ch = 0; ch < 4; ch++)
      for (other = ch + 1; other < 4; other++)
      {
         uint16_t overlap = (uint16_t)(rp[ch] & rp[other]);
         if (overlap)
         {
            sprintf(buf,
               "%s and %s share bits %04x (%s=%04x %s=%04x) - a "
               "quantised width does not match its packing shift",
               g_chan[ch], g_chan[other], overlap,
               g_chan[ch], rp[ch], g_chan[other], rp[other]);
            report(ident, buf);
         }
      }

   for (ch = 0; ch < 4; ch++)
   {
      uint16_t mask = rp[ch];
      unsigned full, v, x, y, prev;

      /* A channel with no field of its own (alpha under 565) is
       * legitimately absent; nothing further to check. */
      if (!mask)
      {
         if (rd[ch])
         {
            sprintf(buf, "%s is absent from the plain converter but "
               "reaches %04x in the dithered one", g_chan[ch], rd[ch]);
            report(ident, buf);
         }
         continue;
      }

      /* 2. Contiguity. */
      if (!is_contiguous(mask))
      {
         sprintf(buf, "%s occupies discontiguous bits %04x",
            g_chan[ch], mask);
         report(ident, buf);
         continue;
      }

      /* 5a. The dithered twin must cover the same field exactly. */
      if (rd[ch] != mask)
      {
         sprintf(buf, "%s reaches %04x plain but %04x dithered - "
            "mismatched converter pair, or the dither spills",
            g_chan[ch], mask, rd[ch]);
         report(ident, buf);
      }

      full = (unsigned)(mask >> first_bit(mask));

      /* 3. Endpoints exact. */
      if (field_of(plain(test_colour(ch, 0)), mask) != 0)
      {
         sprintf(buf, "%s: input 0 does not give an empty field",
            g_chan[ch]);
         report(ident, buf);
      }
      if (field_of(plain(test_colour(ch, 255)), mask) != full)
      {
         sprintf(buf, "%s: input 255 gives %u, not full scale %u",
            g_chan[ch],
            field_of(plain(test_colour(ch, 255)), mask), full);
         report(ident, buf);
      }

      /* 4. Monotonic in the input. */
      prev = 0;
      for (v = 0; v < 256; v++)
      {
         unsigned cur = field_of(plain(test_colour(ch, v)), mask);
         if (cur < prev)
         {
            sprintf(buf, "%s: field falls from %u to %u at input %u "
               "- the channel is wrapping", g_chan[ch], prev, cur, v);
            report(ident, buf);
            break;
         }
         prev = cur;
      }

      for (y = 0; y < 4; y++)
         for (x = 0; x < 4; x++)
         {
            prev = 0;
            for (v = 0; v < 256; v++)
            {
               unsigned cur =
                  field_of(dith(test_colour(ch, v), x, y), mask);
               if (cur < prev)
               {
                  sprintf(buf, "%s: dithered field falls from %u to "
                     "%u at input %u, cell (%u,%u)",
                     g_chan[ch], prev, cur, v, x, y);
                  report(ident, buf);
                  v = 256;
                  x = 4;
                  y = 4;
                  break;
               }
               prev = cur;
            }
         }

      /* 5b. Dither may only nudge a level, never restate it. */
      for (v = 0; v < 256; v++)
      {
         unsigned p = field_of(plain(test_colour(ch, v)), mask);
         for (y = 0; y < 4; y++)
            for (x = 0; x < 4; x++)
            {
               unsigned d =
                  field_of(dith(test_colour(ch, v), x, y), mask);
               unsigned diff = (d > p) ? (d - p) : (p - d);
               if (diff > 1)
               {
                  sprintf(buf, "%s: dithered field %u differs from "
                     "plain %u by %u levels at input %u",
                     g_chan[ch], d, p, diff, v);
                  report(ident, buf);
                  v = 256;
                  x = 4;
                  y = 4;
               }
            }
      }
   }

   if (g_failures == before)
   {
      printf("[ok]   %-11s", ident);
      for (ch = 0; ch < 4; ch++)
         printf(" %s=%04x", g_chan[ch], rp[ch]);
      printf("\n");
   }
}

int main(void)
{
   size_t i;

   for (i = 0; i < ARRAY_SIZE(rgui_pixel_format_map); i++)
      check_ident(rgui_pixel_format_map[i].driver_ident);

   /* The fallback path: an ident no row matches. */
   check_ident("no_such_video_driver");

   if (g_failures)
   {
      printf("\n%d contract violation(s).\n", g_failures);
      return 1;
   }
   printf("\nAll converters honour the contract.\n");
   return 0;
}
