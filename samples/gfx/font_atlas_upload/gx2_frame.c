/* The GX2 driver's frame texture, extracted from the shipping source:
 * a frame goes into the image the last frame does not show, which then
 * becomes the shown one; a core is lent that same image, and a frame it
 * wrote there is shown without a copy. */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <boolean.h>
#include <libretro.h>

typedef uint32_t u32;
#define GX2_INVALIDATE_MODE_CPU_TEXTURE 2

typedef struct
{
   struct
   {
      unsigned width, height, pitch;
      u32      imageSize;
      void    *image;
   } surface;
} GX2Texture;

typedef struct
{
   GX2Texture texture;
   void      *frame_image[2];
   unsigned   frame_slot;
   unsigned   width, height;
   bool       rgb32;
} wiiu_video_t;

static unsigned    copies, inits, invalidates;
static const void *invalidated;

static void *test_memcpy(void *dst, const void *src, size_t n)
{
   copies++;
   return memmove(dst, src, n);
}
#define memcpy test_memcpy

static void GX2Invalidate(int mode, void *at, u32 len)
{
   assert(mode == GX2_INVALIDATE_MODE_CPU_TEXTURE);
   assert(len);
   invalidates++;
   invalidated = at;
}

/* As the driver's: both images made again at the frame's size, a pitch
 * of whole 32-pixel lines, the first one shown */
static bool wiiu_init_frame_textures(wiiu_video_t *w, unsigned width,
      unsigned height)
{
   unsigned i;
   unsigned bpp = w->rgb32 ? 4 : 2;
   free(w->frame_image[0]);
   free(w->frame_image[1]);
   w->texture.surface.width     = width;
   w->texture.surface.height    = height;
   w->texture.surface.pitch     = (width + 31) & ~31u;
   w->texture.surface.imageSize = w->texture.surface.pitch * height * bpp;
   for (i = 0; i < 2; i++)
      w->frame_image[i] = calloc(1, w->texture.surface.imageSize);
   w->frame_slot            = 0;
   w->texture.surface.image = w->frame_image[0];
   inits++;
   return true;
}

#include "gx2_frame.inc"
#undef memcpy

#define W 10
#define H 4

static void fill(uint32_t *p, unsigned pitch_px, uint32_t seed)
{
   unsigned x, y;
   for (y = 0; y < H; y++)
      for (x = 0; x < W; x++)
         p[y * pitch_px + x] = seed + y * 1000 + x;
}

static bool same(const uint32_t *a, unsigned ap, const uint32_t *b,
      unsigned bp)
{
   unsigned y;
   for (y = 0; y < H; y++)
      if (memcmp(a + y * ap, b + y * bp, W * 4))
         return false;
   return true;
}

int main(void)
{
   wiiu_video_t w;
   struct retro_framebuffer fb;
   uint32_t core[16 * H];
   unsigned tex_pitch;

   memset(&w, 0, sizeof(w));
   w.rgb32 = true;

   /* No texture yet: nothing to lend */
   memset(&fb, 0, sizeof(fb));
   fb.width = W; fb.height = H;
   assert(!gx2_get_current_software_framebuffer(&w, &fb));

   /* A frame of the core's own: the images made, copied into the one
    * not shown, which is then shown */
   fill(core, 16, 1);
   gx2_frame_upload(&w, core, W, H, 16 * 4);
   tex_pitch = w.texture.surface.pitch;
   assert(inits == 1 && w.frame_slot == 1);
   assert(w.texture.surface.image == w.frame_image[1]);
   assert(same((const uint32_t*)w.frame_image[1], tex_pitch, core, 16));
   assert(invalidated == w.frame_image[1]);

   /* Lent: the image the last frame does not show, at the texture's
    * pitch, cached */
   assert(gx2_get_current_software_framebuffer(&w, &fb));
   assert(fb.data == w.frame_image[0]);
   assert(fb.pitch == tex_pitch * 4);
   assert(fb.format == RETRO_PIXEL_FORMAT_XRGB8888);
   assert(fb.memory_flags == RETRO_MEMORY_TYPE_CACHED);

   /* Written there and pushed: shown from there, nothing copied */
   fill((uint32_t*)fb.data, tex_pitch, 2);
   copies = 0;
   gx2_frame_upload(&w, fb.data, W, H, (unsigned)fb.pitch);
   assert(copies == 0 && inits == 1);
   assert(w.frame_slot == 0 && w.texture.surface.image == w.frame_image[0]);
   assert(invalidated == w.frame_image[0]);

   /* The next lend is the other image: never the one being shown */
   assert(gx2_get_current_software_framebuffer(&w, &fb));
   assert(fb.data == w.frame_image[1]);

   /* A lent frame pushed smaller is still shown in place */
   fill((uint32_t*)fb.data, tex_pitch, 3);
   copies = 0;
   gx2_frame_upload(&w, fb.data, W - 2, H - 1, (unsigned)fb.pitch);
   assert(copies == 0 && inits == 1 && w.frame_slot == 1);

   /* Refused: another size, and a 16-bit frame, which is byte-swapped
    * on its way in */
   fb.width = W + 1;
   assert(!gx2_get_current_software_framebuffer(&w, &fb));
   fb.width = W;
   w.rgb32  = false;
   assert(!gx2_get_current_software_framebuffer(&w, &fb));

   /* RGB565 goes in byte-swapped, into the image not shown */
   {
      uint16_t px[16 * H];
      const uint16_t *img;
      unsigned i;
      for (i = 0; i < 16 * H; i++)
         px[i] = (uint16_t)(0x1234 + i);
      gx2_frame_upload(&w, px, W + 6, H, 16 * 2);
      img = (const uint16_t*)w.texture.surface.image;
      assert(inits == 2);
      assert(img == w.frame_image[w.frame_slot]);
      assert(img[0] == 0x3412);
      assert(img[w.texture.surface.pitch + 1]
            == __builtin_bswap16((uint16_t)(0x1234 + 17)));
   }

   free(w.frame_image[0]);
   free(w.frame_image[1]);
   puts("PASS GX2 frame texture: shown image never lent, lent frames not copied");
   return 0;
}
