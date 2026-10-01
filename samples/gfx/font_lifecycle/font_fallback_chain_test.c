/* A codepoint whose fallback face lacks it is drawn from a second face
 * where one is known to carry its range - a circled number from the CJK
 * face when the general one has none - and a codepoint no face is known
 * to carry does not have the other fallback files read for it. The
 * general face here is DejaVu Sans Mono and the CJK one DejaVu Sans,
 * which has the circled numbers Mono lacks. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <boolean.h>
#include "gfx/font_driver.h"

int read_should_fail = 0;
extern int  read_real_files;
int stub_real_read(const char *name);

static int fails = 0;
#define CHECK(c,m) do { if(!(c)) { printf("  FAIL: %s\n", m); fails++; } } while (0)

static int drawn(const font_renderer_driver_t *drv, void *h, uint32_t code)
{
   int spins;
   for (spins = 0; spins < 5000; spins++)
   {
      const struct font_glyph *g = drv->get_glyph(h, code);
      if (g && g->width && g->height)
         return 1;
      usleep(1000);
   }
   return 0;
}

static int read_file(const char *name)
{
   return stub_real_read(name);
}

int main(void)
{
   static const char *dir_fonts =
      "/usr/share/fonts/truetype/dejavu/";
   char dir[]  = "/tmp/font_chain_XXXXXX";
   char from[512], to[512];
   const font_renderer_driver_t *drv = NULL;
   void *h                           = NULL;
   font_data_t *font;
   int spins;
   FILE *f;

   snprintf(from, sizeof(from), "%sDejaVuSansMono.ttf", dir_fonts);
   if (!(f = fopen(from, "rb")))
   {
      printf("  skip: no DejaVu fonts on this runner\n");
      return 0;
   }
   fclose(f);
   if (!mkdtemp(dir))
      return 1;
   snprintf(to, sizeof(to), "%s/fallback-font.ttf", dir);
   if (symlink(from, to))
      return 1;
   snprintf(from, sizeof(from), "%sDejaVuSans.ttf", dir_fonts);
   snprintf(to, sizeof(to), "%s/chinese-fallback-font.ttf", dir);
   if (symlink(from, to))
      return 1;

   read_real_files = 1;
   font = (font_data_t*)calloc(1, sizeof(*font));
   font_driver_set_language_font(font, dir, NULL);
   font_driver_set_language_font(font, NULL, NULL);
   free(font);

   /* The built-in glyphs only: every code here misses */
   if (!font_renderer_create_default(&drv, &h, "/nonexistent/font.ttf",
            16, FONT_ATLAS_FORMAT_A8))
      return 1;

   /* Devanagari: the general face, which has none, and no other */
   for (spins = 0; spins < 200; spins++)
   {
      drv->get_glyph(h, 0x0905);
      usleep(1000);
   }
   CHECK(read_file("fallback-font.ttf"), "the general fallback was read");
   CHECK(!read_file("chinese-fallback-font.ttf"),
         "nothing else was read for a code no face is known to have");

   /* Circled digit one: not in the general face, in the CJK one */
   CHECK(drawn(drv, h, 0x2460), "circled one drawn from the second face");
   CHECK(read_file("chinese-fallback-font.ttf"), "the CJK fallback was read");

   /* Ethiopic goes to its own face, which this directory lacks: it is
    * looked for, and nothing breaks for want of it */
   for (spins = 0; spins < 200; spins++)
   {
      drv->get_glyph(h, 0x1200);
      usleep(1000);
   }
   CHECK(read_file("ethiopic-fallback-font.ttf"),
         "Ethiopic looked for in its own fallback");

   drv->free(h);
   snprintf(to, sizeof(to), "%s/fallback-font.ttf", dir);
   unlink(to);
   snprintf(to, sizeof(to), "%s/chinese-fallback-font.ttf", dir);
   unlink(to);
   rmdir(dir);

   if (fails)
      printf("%d failure(s)\n", fails);
   else
      printf("all checks passed (0 failures)\n");
   return fails ? 1 : 0;
}
