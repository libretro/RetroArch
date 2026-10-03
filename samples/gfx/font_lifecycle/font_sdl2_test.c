/* SDL2's legacy on-screen message path, sdl2_render_msg() in the
 * shipping gfx/drivers/sdl2_gfx.c, drawn by real SDL2 into a software
 * renderer's surface.
 *
 * The path draws from a texture made from the glyph cache's atlas.
 * The cache rasterizes glyphs on demand, so the texture has to follow
 * it, and the message is UTF-8. Each check maps every inked texel of a
 * glyph in the atlas to the surface pixel it has to land on.
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
