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

   /* The dirty region is the bounding box of the cells drawn since the
    * consumer last cleared it, each axis on its own */
   {
      struct font_atlas *atlas = drv->get_atlas(h);
      const struct font_glyph *a, *b;
      unsigned ax, ay, bx, by;
      atlas->dirty = false;
      font_driver_frame_begin();
      a  = drv->get_glyph(h, 0x0391);
      ax = a ? VIDEO_SCALE_W(a->atlas_pos) : 0;
      ay = a ? VIDEO_SCALE_H(a->atlas_pos) : 0;
      b  = drv->get_glyph(h, 0x03A9);
      bx = b ? VIDEO_SCALE_W(b->atlas_pos) : 0;
      by = b ? VIDEO_SCALE_H(b->atlas_pos) : 0;
      CHECK(a && b && atlas->dirty, "dirty: two new glyphs mark the atlas");
      CHECK(   VIDEO_SCALE_W(atlas->dirty_xy0) == (ax < bx ? ax : bx)
            && VIDEO_SCALE_H(atlas->dirty_xy0) == (ay < by ? ay : by),
            "dirty: top-left is the least of each axis");
      CHECK(   VIDEO_SCALE_W(atlas->dirty_xy1) > (ax > bx ? ax : bx)
            && VIDEO_SCALE_H(atlas->dirty_xy1) > (ay > by ? ay : by)
            && VIDEO_SCALE_W(atlas->dirty_xy1) <= atlas->width
            && VIDEO_SCALE_H(atlas->dirty_xy1) <= atlas->height,
            "dirty: bottom-right past both cells, inside the atlas");
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
      atlas->max_dims = VIDEO_SCALE_PACK(4096, 4096);

      font_driver_frame_begin();
      g  = drv->get_glyph(h, 'A');
      ax = VIDEO_SCALE_W(g->atlas_pos);
      ay = VIDEO_SCALE_H(g->atlas_pos);
      undrawn = frame(drv, h, 0x0100, &moved);
      CHECK(undrawn > 0, "growing: the first frame still runs out");
      CHECK(drv->get_atlas(h)->width == w0,
            "growing: not within the frame that ran out");

      font_driver_frame_begin();
      atlas = drv->get_atlas(h);
      CHECK(atlas->width == 2 * w0 && atlas->height == 2 * h0,
            "growing: twice as wide and high a frame later");
      CHECK(atlas->dirty && atlas->dirty_xy0 == 0
            && atlas->dirty_xy1 == VIDEO_SCALE_PACK(atlas->width,
               atlas->height),
            "growing: all of it marked for upload");
      g = drv->get_glyph(h, 'A');
      CHECK(VIDEO_SCALE_W(g->atlas_pos) == ax && VIDEO_SCALE_H(g->atlas_pos) == ay,
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

   /* A cell never used goes before any glyph is evicted: a glyph new
    * to a font in a later frame takes an empty cell, and the ones
    * rasterized when it was made stay. Nothing is looked up first, as
    * that would make the looked-up glyphs the newest. */
   if (!font_renderer_create_default(&drv, &h, dejavu, 16,
            FONT_ATLAS_FORMAT_A8))
      return 1;
   {
      struct font_atlas *atlas = drv->get_atlas(h);
      unsigned cp;
      atlas->dirty = false;
      font_driver_frame_begin();
      CHECK(drv->get_glyph(h, 0x0391) && atlas->dirty
            && atlas->dirty_xy0 != 0,
            "unused cells: a new glyph takes one past the first");
      atlas->dirty = false;
      for (cp = 32; cp < 127; cp++)
         drv->get_glyph(h, cp);
      CHECK(!atlas->dirty, "unused cells: every ASCII glyph still cached");
   }
   drv->free(h);

   /* The dirty region is sent as rectangles round the cells drawn
    * into, not as the one rectangle round them all: a frame's new
    * glyphs land wherever cells are free, far apart */
   if (!font_renderer_create_default(&drv, &h, dejavu, 16,
            FONT_ATLAS_FORMAT_A8))
      return 1;
   {
      struct font_atlas *atlas;
      unsigned i, r, n, covered = 0;
      size_t   sum = 0;
      const struct font_glyph *g[6];
      font_driver_frame_begin();
      for (i = 0; i < 256; i++)
         drv->get_glyph(h, 0x0100 + i);
      font_driver_frame_begin();
      atlas = drv->get_atlas(h);
      atlas->dirty = false;
      /* Keep all but six cells, spread over the atlas, in use */
      for (i = 0; i < 256; i++)
         if (i % 50)
            drv->get_glyph(h, 0x0100 + i);
      for (i = 0; i < 6; i++)
         g[i] = drv->get_glyph(h, 0x0400 + i);
      n = font_atlas_dirty_rects(atlas);
      for (r = 0; r < n; r++)
      {
         unsigned xy0, xy1;
         font_atlas_dirty_rect(atlas, r, &xy0, &xy1);
         sum += VIDEO_SCALE_AREA(xy1 - xy0);
         for (i = 0; i < 6; i++)
            if (     g[i]
                  && VIDEO_SCALE_W(g[i]->atlas_pos) >= VIDEO_SCALE_W(xy0)
                  && VIDEO_SCALE_H(g[i]->atlas_pos) >= VIDEO_SCALE_H(xy0)
                  && VIDEO_SCALE_W(g[i]->atlas_pos) <  VIDEO_SCALE_W(xy1)
                  && VIDEO_SCALE_H(g[i]->atlas_pos) <  VIDEO_SCALE_H(xy1))
               covered |= 1u << i;
      }
      CHECK(atlas->dirty && covered == 0x3f,
            "dirty rects: every new glyph's cell is in one");
      CHECK(sum * 4 < VIDEO_SCALE_AREA(atlas->dirty_xy1 - atlas->dirty_xy0),
            "dirty rects: far fewer pixels than the one rectangle round them");
   }
   drv->free(h);

   /* Misses take cells least recently used first, with hits between
    * them and across more of them than one search of the cells keeps
    * in hand: 256 glyphs fill the cells; a new glyph takes the oldest,
    * glyph 0's, the next 99 are looked up again, and 99 more new ones
    * take exactly the cells of the 99 after those */
   if (!font_renderer_create_default(&drv, &h, dejavu, 16,
            FONT_ATLAS_FORMAT_A8))
      return 1;
   {
      static unsigned pos[256];
      unsigned i, j, misplaced = 0;
      const struct font_glyph *g;
      font_driver_frame_begin();
      for (i = 0; i < 256; i++)
      {
         g      = drv->get_glyph(h, 0x0100 + i);
         pos[i] = g ? g->atlas_pos : 0;
      }
      font_driver_frame_begin();
      g = drv->get_glyph(h, 0x0400);
      CHECK(g && g->atlas_pos == pos[0], "lru: the oldest cell goes first");
      for (i = 1; i < 100; i++)
         drv->get_glyph(h, 0x0100 + i);
      for (i = 1; i < 100; i++)
      {
         g = drv->get_glyph(h, 0x0400 + i);
         for (j = 100; j < 199 && (!g || pos[j] != g->atlas_pos); j++);
         if (j == 199)
            misplaced++;
      }
      CHECK(misplaced == 0, "lru: new glyphs take the least recent cells");
      drv->get_atlas(h)->dirty = false;
      for (i = 1; i < 100; i++)
         drv->get_glyph(h, 0x0100 + i);
      CHECK(!drv->get_atlas(h)->dirty,
            "lru: the glyphs looked up again stayed cached");
   }
   drv->free(h);

   if (fails)
      printf("%d failure(s)\n", fails);
   else
      printf("all checks passed (0 failures)\n");
   return fails ? 1 : 0;
}
