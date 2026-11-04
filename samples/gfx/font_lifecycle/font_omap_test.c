/* The OMAP framebuffer driver's on-screen message path, omap_render_msg()
 * in the shipping gfx/drivers/omap_gfx.c, blended into a page in memory.
 *
 * A glyph is clipped to the page before it is blended: one partly off
 * the top or left edge loses the rows or columns there, and one wholly
 * off it draws nothing. FreeType's glyph boxes are their ink, so with
 * the message at the top of the page a glyph sitting above the
 * baseline, as '-' does, lies wholly above the page, and one with a
 * descender, as 'g', reaches into it. The page is exactly its pixels
 * in size, so a blend running past it is a heap overflow the sanitizer
 * reports.
 *
 * omap_gfx.c is included here; the rest of the driver is never called,
 * and its frontend references are left unresolved at link time. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../gfx/drivers/omap_gfx.c"

int read_should_fail = 0;
extern int read_real_files;

static int fails = 0;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); fails++; } } while (0)

#define PAGE_W 320
#define PAGE_H 120

static unsigned page_ink(const uint32_t *px)
{
   unsigned i, n = 0;
   for (i = 0; i < PAGE_W * PAGE_H; i++)
      n += (px[i] & 0xFFFFFFu) != 0;
   return n;
}

int main(void)
{
   static const char *dejavu =
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
   omap_video_t vid;
   omapfb_data_t data;
   omapfb_state_t state;
   omapfb_page_t page;
   uint32_t *px;
   FILE *f;

   if (!(f = fopen(dejavu, "rb")))
   {
      printf("  skip: no %s on this runner\n", dejavu);
      return 0;
   }
   fclose(f);
   read_real_files = 1;

   memset(&vid, 0, sizeof(vid));
   memset(&data, 0, sizeof(data));
   memset(&state, 0, sizeof(state));
   memset(&page, 0, sizeof(page));
   /* Exactly the page's pixels: a blend past it is an overflow */
   px                  = (uint32_t*)calloc(PAGE_W * PAGE_H, sizeof(*px));
   page.buf            = px;
   state.si.xres       = PAGE_W;
   data.current_state  = &state;
   data.cur_page       = &page;
   data.bpp            = 4;
   vid.omap            = &data;
   vid.bytes_per_pixel = 4;
   vid.dims            = VIDEO_SCALE_PACK(PAGE_W, PAGE_H);
   vid.font_rgb[0]     = vid.font_rgb[1] = vid.font_rgb[2] = 255;

   if (!font_renderer_create_default(&vid.font_driver, &vid.font,
            dejavu, 24, FONT_ATLAS_FORMAT_A8))
   {
      printf("  FAIL: the font\n");
      return 1;
   }

   /* At the top of the page: '-' lies above it, wholly clipped */
   config_get_ptr()->floats.video_msg_pos_x = 0.1f;
   config_get_ptr()->floats.video_msg_pos_y = 1.0f;
   font_driver_frame_begin();
   omap_render_msg(&vid, "--");
   CHECK(page_ink(px) == 0, "a glyph wholly above the page draws nothing");

   /* A descender there reaches down into the page and is clipped */
   omap_render_msg(&vid, "g");
   CHECK(page_ink(px) > 0, "a glyph partly above the page draws its rest");

   /* Mid-page, whole */
   memset(px, 0, PAGE_W * PAGE_H * sizeof(*px));
   config_get_ptr()->floats.video_msg_pos_y = 0.5f;
   font_driver_frame_begin();
   omap_render_msg(&vid, "-T-");
   CHECK(page_ink(px) > 0, "a message mid-page draws");

   vid.font_driver->free(vid.font);
   free(px);
   if (fails)
      return 1;
   printf("  omap: glyphs clipped to the page, nothing blended past it\n");
   return 0;
}
