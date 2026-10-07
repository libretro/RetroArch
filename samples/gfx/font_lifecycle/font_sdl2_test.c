/* SDL2's legacy on-screen message path, sdl2_render_msg() in the
 * shipping gfx/drivers/sdl2_gfx.c, drawn by real SDL2 into a software
 * renderer's surface.
 *
 * The path draws from a texture made from the glyph cache's atlas.
 * The cache rasterizes glyphs on demand, so the texture has to follow
 * it, and the message is UTF-8. Each check maps every inked texel of a
 * glyph in the atlas to the surface pixel it has to land on.
 *
 * The raster font's texture, which takes only the atlas region new
 * glyphs went into, is checked against the atlas texel for texel.
 *
 * sdl2_gfx.c is included here; the rest of the driver is never called,
 * and its frontend references are left unresolved at link time. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../gfx/drivers/sdl2_gfx.c"

int read_should_fail = 0;
extern int read_real_files;

static int fails = 0;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); fails++; } } while (0)

#define SURF_W 480
#define SURF_H 200

static SDL_Surface  *surf;
static sdl2_video_t  vid_st;

static void clear_surface(void)
{
   SDL_SetRenderDrawColor(vid_st.renderer, 0, 0, 0, 255);
   SDL_RenderClear(vid_st.renderer);
}

static int lit(int x, int y)
{
   uint32_t px;
   uint8_t r, g, b;
   if (x < 0 || y < 0 || x >= SURF_W || y >= SURF_H)
      return 0;
   px = ((uint32_t*)surf->pixels)[(size_t)y * (surf->pitch / 4) + x];
   SDL_GetRGB(px, surf->format, &r, &g, &b);
   return r || g || b;
}

static int surface_ink(void)
{
   int x, y, n = 0;
   for (y = 0; y < SURF_H; y++)
      for (x = 0; x < SURF_W; x++)
         n += lit(x, y);
   return n;
}

/* Inked texels of @code's glyph found at their pixels with the pen at
 * (@pen_x, @pen_y), y down; *missing counts those that are not */
static int glyph_lands(uint32_t code, int pen_x, int pen_y, int *missing)
{
   const struct font_glyph *g = vid_st.font_driver->get_glyph(vid_st.font_data, code);
   struct font_atlas *atlas   = vid_st.font_driver->get_atlas(vid_st.font_data);
   unsigned u, v;
   int found = 0;
   *missing  = 0;
   if (!g || !atlas)
   {
      (*missing)++;
      return 0;
   }
   for (v = 0; v < g->height; v++)
      for (u = 0; u < g->width; u++)
      {
         if (!atlas->buffer[(size_t)(g->atlas_offset_y + v) * atlas->width
               + g->atlas_offset_x + u])
            continue;
         if (lit(pen_x + g->draw_offset_x + (int)u, pen_y + g->draw_offset_y + (int)v))
            found++;
         else
            (*missing)++;
      }
   return found;
}

/* Whether @rf's texture holds its atlas: drawn a tile at a time over
 * black, each pixel's red is the coverage the atlas has there */
static int raster_matches(sdl2_raster_t *rf)
{
   const struct font_atlas *a = rf->atlas;
   unsigned tx, ty, x, y;
   int bad = 0;
   SDL_SetTextureBlendMode(rf->tex, SDL_BLENDMODE_BLEND);
   for (ty = 0; ty < a->height; ty += SURF_H)
      for (tx = 0; tx < a->width; tx += SURF_W)
      {
         SDL_Rect src, dst;
         src.x = (int)tx;
         src.y = (int)ty;
         src.w = (int)(a->width  - tx < SURF_W ? a->width  - tx : SURF_W);
         src.h = (int)(a->height - ty < SURF_H ? a->height - ty : SURF_H);
         dst.x = 0;
         dst.y = 0;
         dst.w = src.w;
         dst.h = src.h;
         clear_surface();
         SDL_RenderCopy(vid_st.renderer, rf->tex, &src, &dst);
         for (y = 0; y < (unsigned)src.h; y++)
            for (x = 0; x < (unsigned)src.w; x++)
            {
               uint8_t r, g, b;
               int     want = a->buffer[(size_t)(ty + y) * a->width + tx + x];
               SDL_GetRGB(((uint32_t*)surf->pixels)
                     [(size_t)y * (surf->pitch / 4) + x], surf->format,
                     &r, &g, &b);
               if (r - want > 1 || want - r > 1)
                  bad++;
            }
      }
   return bad == 0;
}

int main(void)
{
   static const char *dejavu =
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
   struct font_line_metrics *lm = NULL;
   int found, missing, line_h;
   const int pen_x = (int)(0.1f * SURF_W);
   const int pen_y = (int)((1.0f - 0.5f) * SURF_H);
   FILE *f;

   if (!(f = fopen(dejavu, "rb")))
   {
      printf("  skip: no %s on this runner\n", dejavu);
      return 0;
   }
   fclose(f);
   read_real_files = 1;

   if (!(surf = SDL_CreateRGBSurfaceWithFormat(0, SURF_W, SURF_H, 32,
               SDL_PIXELFORMAT_XRGB8888))
         || !(vid_st.renderer = SDL_CreateSoftwareRenderer(surf)))
   {
      printf("  skip: no SDL2 software renderer\n");
      return 0;
   }
   vid_st.vp.dims = VIDEO_SCALE_PACK(SURF_W, SURF_H);
   config_get_ptr()->bools.video_font_enable  = true;
   config_get_ptr()->floats.video_msg_color_r = 1.0f;
   config_get_ptr()->floats.video_msg_color_g = 1.0f;
   config_get_ptr()->floats.video_msg_color_b = 1.0f;
   sdl2_init_font(&vid_st, dejavu, 16);
   CHECK(vid_st.font_data && vid_st.font.tex, "the OSD font and its texture");
   if (!vid_st.font_data || !vid_st.font.tex)
      return 1;
   vid_st.font_driver->get_line_metrics(vid_st.font_data, &lm);
   line_h = (int)(lm->height + 0.5f);

   /* Where the message position puts it */
   font_driver_frame_begin();
   clear_surface();
   sdl2_render_msg(&vid_st, "T", 0.1f, 0.5f);
   found = glyph_lands('T', pen_x, pen_y, &missing);
   CHECK(found > 0 && missing == 0, "'T' lands texel for texel");
   CHECK(surface_ink() == found, "nothing drawn but 'T'");

   /* UTF-8: two bytes, one glyph - and, ASCII being rasterized when
    * the font is made, a glyph new to the atlas since the texture was */
   font_driver_frame_begin();
   clear_surface();
   sdl2_render_msg(&vid_st, "\xC3\xA9", 0.1f, 0.5f);
   found = glyph_lands(0xE9, pen_x, pen_y, &missing);
   CHECK(found > 0 && missing == 0, "U+00E9 drawn as one glyph");
   CHECK(surface_ink() == found, "and nothing else");

   /* A second line, one line height down */
   font_driver_frame_begin();
   clear_surface();
   sdl2_render_msg(&vid_st, "T\nW", 0.1f, 0.5f);
   found = glyph_lands('W', pen_x, pen_y + line_h, &missing);
   CHECK(found > 0 && missing == 0, "the second line is a line height down");

   /* The raster font's texture is made whole, after which only the
    * region new glyphs were drawn into is converted and sent */
   {
      sdl2_raster_t *rf = (sdl2_raster_t*)sdl2_raster_font_init(&vid_st,
            dejavu, 16, false);
      CHECK(rf && rf->tex, "the raster font and its texture");
      if (rf && rf->tex)
      {
         static const uint32_t first[2] = { 0x0391, 0x0410 };
         struct font_atlas *a = rf->atlas;
         uint32_t cp;
         int i;
         CHECK(raster_matches(rf), "raster: the texture made whole");
         /* The second batch takes cells clear of the atlas's corner */
         for (i = 0; i < 2; i++)
         {
            font_driver_frame_begin();
            for (cp = first[i]; cp < first[i] + 24; cp++)
               rf->font_driver->get_glyph(rf->font_data, cp);
            CHECK(a->dirty && a->dirty_xy1
                  != VIDEO_SCALE_PACK(a->width, a->height),
                  "raster: new glyphs dirty part of the atlas");
            CHECK(i == 0 || a->dirty_xy0 != 0,
                  "raster: a region off the atlas's corner");
            sdl2_raster_font_upload_atlas(rf);
            CHECK(!a->dirty, "raster: the upload takes the region");
            CHECK(raster_matches(rf), "raster: the texture after a region");
         }
         sdl2_raster_font_free(rf, false);
      }
   }

   SDL_DestroyTexture(vid_st.font.tex);
   vid_st.font_driver->free(vid_st.font_data);
   SDL_DestroyRenderer(vid_st.renderer);
   SDL_FreeSurface(surf);

   if (fails)
   {
      printf("font_sdl2_test: %d failure(s)\n", fails);
      return 1;
   }
   printf("font_sdl2_test: all checks passed\n");
   return 0;
}
