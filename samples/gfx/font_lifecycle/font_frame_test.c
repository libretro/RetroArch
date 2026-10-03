/* A cell looked up in a frame is not handed to another codepoint before
 * the frame ends: past what the cache holds, the rest of a frame's
 * glyphs go undrawn instead of overwriting cells already drawn from, and
 * the next frame can take every cell the last one used. Without frame
 * marks the cache is plain LRU, as it was. DejaVu Sans, for its
 * thousands of glyphs. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <boolean.h>
#include "gfx/font_driver.h"

int read_should_fail = 0;
extern int read_real_files;

static int fails = 0;
#define CHECK(c,m) do { if(!(c)) { printf("  FAIL: %s\n", m); fails++; } } while (0)

/* 300 distinct codepoints DejaVu Sans has, from @base */
static unsigned frame(const font_renderer_driver_t *drv, void *h,
      uint32_t base, int *moved)
{
   unsigned i, undrawn = 0;
   const struct font_glyph *first = drv->get_glyph(h, base);
   for (i = 1; i < 300; i++)
      if (!drv->get_glyph(h, base + i))
         undrawn++;
   *moved = drv->get_glyph(h, base) != first;
   return undrawn;
}

int main(void)
{
   static const char *dejavu =
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
   const font_renderer_driver_t *drv = NULL;
   void *h                           = NULL;
   int moved;
   unsigned undrawn;
   FILE *f;

   if (!(f = fopen(dejavu, "rb")))
   {
      printf("  skip: no %s on this runner\n", dejavu);
      return 0;
   }
   fclose(f);
   read_real_files = 1;
   if (!font_renderer_create_default(&drv, &h, dejavu, 16,
            FONT_ATLAS_FORMAT_A8))
      return 1;

   /* Before any frame mark: LRU, every glyph drawn, cells reused */
   undrawn = frame(drv, h, 0x0100, &moved);
   CHECK(undrawn == 0, "no frame marks: every glyph drawn");

   /* A frame needing more than the cache holds */
   font_driver_frame_begin();
   undrawn = frame(drv, h, 0x0100, &moved);
   CHECK(!moved, "a cell drawn from this frame was not reused in it");
   CHECK(undrawn > 0 && undrawn < 300, "the overflow goes undrawn");
   printf("  frame 1: %u of 300 undrawn\n", undrawn);

   /* The next frame, other text: last frame's cells are free again */
   font_driver_frame_begin();
   undrawn = frame(drv, h, 0x0400, &moved);
   CHECK(!moved, "next frame: no cell reused within it");
   CHECK(undrawn == 300 - 256, "next frame: every cell available again");
   printf("  frame 2: %u of 300 undrawn\n", undrawn);

   /* A frame that fits draws everything */
   font_driver_frame_begin();
   {
      unsigned i, missing = 0;
      for (i = 0; i < 200; i++)
         if (!drv->get_glyph(h, 0x0100 + i))
            missing++;
      CHECK(missing == 0, "a frame that fits draws every glyph");
   }

   drv->free(h);

   /* A consumer that can take a bigger texture: the frame after one
    * that ran out finds the atlas grown, every earlier glyph where it
    * was, and draws all it asks for */
   if (!font_renderer_create_default(&drv, &h, dejavu, 16,
            FONT_ATLAS_FORMAT_A8))
      return 1;
   {
      struct font_atlas *atlas = drv->get_atlas(h);
      unsigned w0 = atlas->width, h0 = atlas->height;
      const struct font_glyph *g;
      unsigned ax, ay;
      atlas->max_width  = 4096;
      atlas->max_height = 4096;

      font_driver_frame_begin();
      g  = drv->get_glyph(h, 'A');
      ax = g->atlas_offset_x;
      ay = g->atlas_offset_y;
      undrawn = frame(drv, h, 0x0100, &moved);
      CHECK(undrawn > 0, "growing: the first frame still runs out");
      CHECK(drv->get_atlas(h)->width == w0,
            "growing: not within the frame that ran out");

      font_driver_frame_begin();
      atlas = drv->get_atlas(h);
      CHECK(atlas->width == 2 * w0 && atlas->height == 2 * h0,
            "growing: twice as wide and high a frame later");
      CHECK(atlas->dirty && atlas->dirty_x1 == atlas->width
            && atlas->dirty_y1 == atlas->height,
            "growing: all of it marked for upload");
      g = drv->get_glyph(h, 'A');
      CHECK(g->atlas_offset_x == ax && g->atlas_offset_y == ay,
            "growing: an earlier glyph keeps its cell");
      undrawn = frame(drv, h, 0x0100, &moved);
      CHECK(undrawn == 0 && !moved, "growing: the frame draws everything");

      /* 1200 a frame, past the 1024 cells: grows again, to 4096 */
      font_driver_frame_begin();
      undrawn  = frame(drv, h, 0x0400, &moved);
      undrawn += frame(drv, h, 0x0400 + 300, &moved);
      undrawn += frame(drv, h, 0x0400 + 600, &moved);
      undrawn += frame(drv, h, 0x0400 + 900, &moved);
      CHECK(undrawn > 0, "growing: 1200 overflows 1024 cells");
      font_driver_frame_begin();
      atlas = drv->get_atlas(h);
      CHECK(atlas->width == 4 * w0, "growing: grows a second time");
      printf("  atlas %ux%u, then %ux%u\n", w0, h0, atlas->width,
            atlas->height);
   }
   drv->free(h);
   if (fails)
      printf("%d failure(s)\n", fails);
   else
      printf("all checks passed (0 failures)\n");
   return fails ? 1 : 0;
}
