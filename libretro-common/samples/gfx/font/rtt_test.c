/* rtt against the shapes its test fonts were drawn with.
 *
 * Two fonts made by rtt_fonts.py hold the same glyphs: a 600-unit
 * square, a circle of radius 400 (quadratic arcs in the TrueType one,
 * cubics through a local and a global subroutine in the CFF one), a
 * composite or two-contour glyph, and a small triangle. The CFF one is
 * CID-keyed, its glyphs split over two font dicts by FDSelect format 3,
 * with stem hints and a hintmask in the square.
 *
 * Checked: the character map (format 4 and 12), advances, vertical
 * metrics and kerning; each glyph's control box and outline; TrueType
 * points as stored and a composite's components; that a
 * rendered square covers its area exactly and a circle its area to
 * within the flattening, at sub-pixel offsets too; that both fonts draw
 * the square byte for byte the same; the 16-bit output; and that every
 * truncation of each font, and fonts with bytes corrupted at random,
 * are refused or drawn without reading outside them. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#include <gfx/font/rtt.h>

#include "rtt_fonts.h"

static int fails;

static void check(const char *what, int ok)
{
   printf("  %-62s %s\n", what, ok ? "ok" : "FAIL");
   if (!ok)
      fails++;
}

typedef struct
{
   int   moves, lines, quads, cubics;
   float x, y; /* the last point */
} counts_t;

static void c_move(void *u, float x, float y)
{
   counts_t *c = (counts_t*)u;
   c->moves++; c->x = x; c->y = y;
}
static void c_line(void *u, float x, float y)
{
   counts_t *c = (counts_t*)u;
   c->lines++; c->x = x; c->y = y;
}
static void c_quad(void *u, float cx, float cy, float x, float y)
{
   counts_t *c = (counts_t*)u;
   (void)cx; (void)cy;
   c->quads++; c->x = x; c->y = y;
}
static void c_cubic(void *u, float ax, float ay, float bx, float by,
      float x, float y)
{
   counts_t *c = (counts_t*)u;
   (void)ax; (void)ay; (void)bx; (void)by;
   c->cubics++; c->x = x; c->y = y;
}

static counts_t outline(const rtt_font_t *f, rtt_scratch_t *sc, int gi)
{
   counts_t c;
   rtt_outline_sink s;
   memset(&c, 0, sizeof(c));
   s.user = &c; s.move_to = c_move; s.line_to = c_line;
   s.quad_to = c_quad; s.cubic_to = c_cubic;
   rtt_glyph_outline(f, sc, gi, &s);
   return c;
}

static int near(float a, float b, float tol)
{
   return fabs((double)a - (double)b) <= tol;
}

/* Sum of coverage, in pixels. */
static double area(const uint8_t *px, int w, int h)
{
   double s = 0;
   int    i;
   for (i = 0; i < w * h; i++)
      s += px[i] / 255.0;
   return s;
}

static uint8_t bitmap[200 * 200], bitmap2[200 * 200];
static uint16_t bitmap16[200 * 200];

static void test_font(const char *name, const uint8_t *data, size_t size,
      int is_cff)
{
   rtt_font_t    f;
   rtt_scratch_t sc;
   char          what[96];
   int           off, adv = 0, lsb = 0;
   float         x0, y0, x1, y1;
   counts_t      c;
   const float   s = 0.1f; /* 1000 units to 100 pixels */

   memset(&sc, 0, sizeof(sc));
   printf("%s:\n", name);
   off = rtt_font_offset_for_index(data, size, 0);
   check("opens", off == 0 && rtt_init(&f, data, size, off));
   check(is_cff ? "CFF outlines, CID-keyed" : "glyf outlines",
         f.is_cff == is_cff && (!is_cff || f.cff_fdselect));
   check("no second face", rtt_font_offset_for_index(data, size, 1) < 0);

   check("cmap: A O C . are glyphs 1 2 3 4, B none",
         rtt_find_glyph(&f, 'A') == 1 && rtt_find_glyph(&f, 'O') == 2
         && rtt_find_glyph(&f, 'C') == 3 && rtt_find_glyph(&f, '.') == 4
         && rtt_find_glyph(&f, 'B') == 0);
   check("cmap: U+1F600 past the BMP", rtt_find_glyph(&f, 0x1F600) == 2);
   rtt_glyph_hmetrics(&f, 1, &adv, &lsb);
   check("advance and side bearing", adv == 800 && lsb == 100);

   /* square: 100..700 x 0..600 */
   check("square's control box",
         rtt_glyph_cbox(&f, &sc, 1, &x0, &y0, &x1, &y1)
         && x0 == 100 && y0 == 0 && x1 == 700 && y1 == 600);
   {
      int bx0, by0, bx1, by1;
      check("square's box", rtt_glyph_box(&f, 1, &bx0, &by0, &bx1, &by1)
            && bx0 == 100 && by0 == 0 && bx1 == 700 && by1 == 600);
   }
   c = outline(&f, &sc, 1);
   check("square's outline: one contour of lines",
         c.moves == 1 && c.lines >= 3 && !c.quads && !c.cubics);

   /* circle: centre (500, 400), radius 400 */
   check("circle's control box takes in the curve controls",
         rtt_glyph_cbox(&f, &sc, 2, &x0, &y0, &x1, &y1)
         && near(x0, 100, 70) && near(x1, 900, 70)
         && near(y0, 0, 70) && near(y1, 800, 70)
         && x0 <= 100.01f && x1 >= 899.99f && y0 <= 0.01f && y1 >= 799.99f);
   c = outline(&f, &sc, 2);
   check(is_cff ? "circle's outline: cubics from both subroutines"
                : "circle's outline: quadratic arcs",
         c.moves == 1 && (is_cff ? c.cubics == 4 : c.quads == 8)
         && near(c.x, 900, 1) && near(c.y, 400, 1));

   if (is_cff)
      check("two contours: a square and a stroke",
            rtt_glyph_cbox(&f, &sc, 3, &x0, &y0, &x1, &y1)
            && x0 == 0 && y0 == 0 && x1 == 805 && y1 == 600
            && outline(&f, &sc, 3).moves == 2);
   else
      check("composite: a square and a half-size one moved",
            rtt_glyph_cbox(&f, &sc, 3, &x0, &y0, &x1, &y1)
            && x0 == 100 && y0 == 0 && x1 == 1150 && y1 == 600
            && outline(&f, &sc, 3).moves == 2);
   check("empty glyph: no box", !rtt_glyph_cbox(&f, &sc, 0, &x0, &y0, &x1, &y1));
   check("out of range: nothing", !rtt_glyph_cbox(&f, &sc, 99, &x0, &y0, &x1, &y1)
         && outline(&f, &sc, -1).moves == 0);

   /* The square at 0.1 is 60 x 60 pixels from (10, 0): drawn at whole
    * and at half-pixel offsets, it covers 3600 exactly. */
   rtt_render_glyph_at(&f, &sc, bitmap, 80, 80, 80, s, s, 0.0f, -70.0f, 1, 0);
   snprintf(what, sizeof(what), "square: %.2f pixels of 3600", area(bitmap, 80, 80));
   check(what, near((float)area(bitmap, 80, 80), 3600.0f, 1.0f));
   check("square: edges land on pixel edges",
         bitmap[10 * 80 + 9] == 0 && bitmap[10 * 80 + 10] == 255
         && bitmap[10 * 80 + 69] == 255 && bitmap[10 * 80 + 70] == 0
         && bitmap[9 * 80 + 30] == 0 && bitmap[10 * 80 + 30] == 255);
   rtt_render_glyph_at(&f, &sc, bitmap, 80, 80, 80, s, s, -0.5f, -70.5f, 1, 0);
   check("square half a pixel over: same area, half-covered edges",
         near((float)area(bitmap, 80, 80), 3600.0f, 1.0f)
         && near(bitmap[11 * 80 + 10], 128, 1) && near(bitmap[11 * 80 + 70], 128, 1));

   /* The circle is radius 40 pixels: pi * 1600 */
   rtt_set_flatness(&f, 0.05f);
   rtt_render_glyph_at(&f, &sc, bitmap, 100, 100, 100, s, s, 0.0f, -90.0f, 2, 0);
   snprintf(what, sizeof(what), "circle: %.1f pixels of %.1f", area(bitmap, 100, 100),
         3.14159265 * 1600.0);
   check(what, near((float)area(bitmap, 100, 100), (float)(3.14159265 * 1600.0),
            (float)(3.14159265 * 1600.0 * 0.004)));
   rtt_set_flatness(&f, 0.0f);

   rtt_render_glyph_at(&f, &sc, bitmap16, 80, 80, 80, s, s, 0.0f, -70.0f, 1, 1);
   check("16-bit: full coverage is 65535",
         bitmap16[30 * 80 + 30] == 65535 && bitmap16[30 * 80 + 5] == 0);

   /* rtt_render_glyph places the box's corner at the origin */
   rtt_render_glyph(&f, &sc, bitmap2, 60, 60, 60, s, s, 1, 100, 600, 0);
   check("render at the glyph's box: filled to its edges",
         near((float)area(bitmap2, 60, 60), 3600.0f, 1.0f));

   if (is_cff)
   {
      check("stored points and components: TrueType only",
            rtt_glyph_points(&f, 1, NULL, NULL, NULL, NULL, 0, 0, NULL) == -1
            && rtt_glyph_components(&f, 3, NULL, 0) == -1);
      check("no vertical metrics", !rtt_glyph_vmetrics(&f, 1, &adv, &lsb)
            && !rtt_vhea_metrics(&f, NULL, NULL, NULL));
      check("no kerning", rtt_kerning(&f, 1, 2) == 0);
   }
   else
   {
      int asc = 0, desc = 0;
      rtt_component_t comp[4];
      int32_t xs[16], ys[16];
      uint8_t on[16];
      uint16_t ends[4];
      int ncont = 0;
      check("stored points: the square's four corners, one contour",
            rtt_glyph_points(&f, 1, xs, ys, on, ends, 16, 4, &ncont) == 4
            && ncont == 1 && ends[0] == 3 && on[0] && on[3]
            && xs[0] == 100 && ys[0] == 0 && xs[2] == 700 && ys[2] == 600);
      check("stored points: counts alone, and a composite refused",
            rtt_glyph_points(&f, 2, NULL, NULL, NULL, NULL, 0, 0, &ncont) > 8
            && ncont == 1
            && rtt_glyph_points(&f, 3, xs, ys, on, ends, 16, 4, &ncont) == -1
            && rtt_glyph_points(&f, 2, xs, ys, on, ends, 4, 4, &ncont) == -1);
      check("components: the square, then a half-size one at (800, 100)",
            rtt_glyph_components(&f, 3, comp, 4) == 2
            && comp[0].glyph == 1 && comp[0].xx == 0x4000 && (comp[0].flags & 2)
            && comp[1].glyph == 1 && comp[1].arg1 == 800 && comp[1].arg2 == 100
            && comp[1].xx == 0x2000 && comp[1].yy == 0x2000
            && comp[1].xy == 0 && comp[1].yx == 0);
      check("components: a simple glyph has none",
            rtt_glyph_components(&f, 1, comp, 4) == 0);
      check("vertical metrics", rtt_glyph_vmetrics(&f, 1, &adv, &lsb)
            && adv == 900 && lsb == 200 && rtt_glyph_vmetrics(&f, 2, &adv, &lsb)
            && adv == 1000 && lsb == 100);
      check("vhea", rtt_vhea_metrics(&f, &asc, &desc, NULL)
            && asc == 500 && desc == -500);
      check("kerning pairs, and none elsewhere",
            rtt_kerning(&f, 1, 2) == -50 && rtt_kerning(&f, 2, 1) == 30
            && rtt_kerning(&f, 3, 4) == 7 && rtt_kerning(&f, 1, 3) == 0);
   }
   {
      uint32_t len = 0;
      const uint8_t *os2 = rtt_table(&f, "OS/2", &len);
      check("a table, in place", os2 && os2 > data && os2 + len <= data + size
            && !rtt_table(&f, "none", NULL));
   }
   rtt_scratch_free(&sc);
}

/* Every truncation and random corruption: refused, or drawn within
 * bounds (the sanitizers watch the reads). */
static void test_damage(const char *name, const uint8_t *data, size_t size)
{
   uint8_t      *copy = (uint8_t*)malloc(size);
   rtt_scratch_t sc;
   size_t        cut;
   unsigned      seed = 12345, round;
   int           opened = 0;
   char          what[96];

   memset(&sc, 0, sizeof(sc));
   for (cut = 0; cut < size; cut++)
   {
      rtt_font_t f;
      int        gi;
      /* a copy of exactly cut bytes, so a read past it is caught */
      uint8_t   *part = (uint8_t*)malloc(cut ? cut : 1);
      memcpy(part, data, cut);
      if (rtt_init(&f, part, cut, 0))
      {
         opened++;
         for (gi = 0; gi < 6; gi++)
         {
            float a, b, c, d;
            rtt_glyph_cbox(&f, &sc, gi, &a, &b, &c, &d);
            rtt_render_glyph_at(&f, &sc, bitmap, 64, 64, 64, 0.05f, 0.05f,
                  0.0f, -50.0f, gi, 0);
            rtt_find_glyph(&f, 'A' + gi);
            rtt_kerning(&f, gi, gi + 1);
         }
      }
      free(part);
   }
   snprintf(what, sizeof(what), "%s: every truncation (%d opened)", name, opened);
   check(what, 1);

   for (round = 0; round < 1500; round++)
   {
      rtt_font_t f;
      int        k, gi;
      memcpy(copy, data, size);
      for (k = 0; k < 4; k++)
      {
         seed = seed * 1103515245u + 12345u;
         copy[(seed >> 8) % size] = (uint8_t)(seed >> 20);
      }
      if (!rtt_init(&f, copy, size, rtt_font_offset_for_index(copy, size, 0)))
         continue;
      for (gi = 0; gi < 6; gi++)
         rtt_render_glyph_at(&f, &sc, bitmap, 64, 64, 64, 0.05f, 0.05f,
               0.0f, -50.0f, gi, 0);
   }
   snprintf(what, sizeof(what), "%s: 1500 corrupted copies", name);
   check(what, 1);
   rtt_scratch_free(&sc);
   free(copy);
}

int main(void)
{
   rtt_font_t    a, b;
   rtt_scratch_t sa, sb;

   test_font("TrueType", rtt_test_ttf, sizeof(rtt_test_ttf), 0);
   test_font("CFF", rtt_test_cff, sizeof(rtt_test_cff), 1);

   printf("both:\n");
   memset(&sa, 0, sizeof(sa));
   memset(&sb, 0, sizeof(sb));
   rtt_init(&a, rtt_test_ttf, sizeof(rtt_test_ttf), 0);
   rtt_init(&b, rtt_test_cff, sizeof(rtt_test_cff), 0);
   rtt_render_glyph_at(&a, &sa, bitmap, 80, 80, 80, 0.1f, 0.1f, 0.3f, -70.7f, 1, 0);
   rtt_render_glyph_at(&b, &sb, bitmap2, 80, 80, 80, 0.1f, 0.1f, 0.3f, -70.7f, 1, 0);
   check("the square, byte for byte the same from either font",
         !memcmp(bitmap, bitmap2, 80 * 80));
   rtt_scratch_free(&sa);
   rtt_scratch_free(&sb);

   test_damage("TrueType", rtt_test_ttf, sizeof(rtt_test_ttf));
   test_damage("CFF", rtt_test_cff, sizeof(rtt_test_cff));

   printf("rtt_test: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
         fails, fails == 1 ? "" : "s");
   return fails ? 1 : 0;
}
