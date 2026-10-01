/* A codepoint the font has no glyph for is drawn from a fallback face,
 * whose file is read on a thread of its own the first time any font
 * needs it. Two fonts on two threads ask for the same fallback at once:
 * the file must be read once and handed to both, and every glyph has to
 * arrive. Run under TSan as well as ASan. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <boolean.h>
#include "gfx/font_driver.h"

int read_should_fail = 0;
extern int read_real_files;

static int fails = 0;
#define CHECK(c,m) do { if(!(c)) { printf("  FAIL: %s\n", m); fails++; } } while (0)

/* Cyrillic: not among the built-in glyphs, in the general fallback */
static const uint32_t codes[] = { 0x0416, 0x0414, 0x042F, 0x0436 };

typedef struct
{
   int got;
} worker_t;

static void *worker(void *arg)
{
   worker_t *w                       = (worker_t*)arg;
   const font_renderer_driver_t *drv = NULL;
   void *handle                      = NULL;
   unsigned i;
   int spins;

   /* A path that cannot be read: the built-in glyphs, which have no
    * Cyrillic, so every code here goes to the fallback */
   if (!font_renderer_create_default(&drv, &handle,
            "/nonexistent/font.ttf", 16, FONT_ATLAS_FORMAT_A8))
      return NULL;

   for (i = 0; i < sizeof(codes) / sizeof(codes[0]); i++)
   {
      for (spins = 0; spins < 5000; spins++)
      {
         const struct font_glyph *g = drv->get_glyph(handle, codes[i]);
         if (g && g->width && g->height)
         {
            w->got++;
            break;
         }
         usleep(1000);
      }
   }

   drv->free(handle);
   return NULL;
}

int main(void)
{
   static const char *dejavu =
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
   char dir[]  = "/tmp/font_fallback_XXXXXX";
   char link_path[256];
   font_data_t *font;
   pthread_t t[2];
   worker_t  w[2];
   FILE *f;

   if (!(f = fopen(dejavu, "rb")))
   {
      printf("  skip: no %s on this runner\n", dejavu);
      return 0;
   }
   fclose(f);

   if (!mkdtemp(dir))
      return 1;
   snprintf(link_path, sizeof(link_path), "%s/fallback-font.ttf", dir);
   if (symlink(dejavu, link_path))
      return 1;

   read_real_files = 1;

   /* The pkg directory reaches font_driver.c the way the menu gives it */
   font = (font_data_t*)calloc(1, sizeof(*font));
   font_driver_set_language_font(font, dir, NULL);
   font_driver_set_language_font(font, NULL, NULL);
   free(font);

   memset(w, 0, sizeof(w));
   pthread_create(&t[0], NULL, worker, &w[0]);
   pthread_create(&t[1], NULL, worker, &w[1]);
   pthread_join(t[0], NULL);
   pthread_join(t[1], NULL);

   CHECK(w[0].got == 4, "font 1 drew every fallback glyph");
   CHECK(w[1].got == 4, "font 2 drew every fallback glyph");
   printf("  fallback glyphs: %d and %d of 4\n", w[0].got, w[1].got);

   unlink(link_path);
   rmdir(dir);

   if (fails)
      printf("%d failure(s)\n", fails);
   else
      printf("all checks passed (0 failures)\n");
   return fails ? 1 : 0;
}
