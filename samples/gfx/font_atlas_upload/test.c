/* The font atlas uploads of the GX2, GXM and RSX drivers
 * (extracted by run.py) against textures in memory.
 *
 * The glyph cache grows its atlas, and each driver makes its texture
 * again at the new size; when that fails the old, smaller texture
 * stays, and the dirty region, the whole grown atlas, is larger than
 * it. Every texture here is allocated exactly its size, so an upload
 * that is not clamped to the texture runs past it, which the sanitizer
 * reports, and every byte it does write is checked against the atlas.
 * GX2's glyph sprites are flushed from the CPU's cache before the GPU
 * reads them: the flushed size must be that of the sprites. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <boolean.h>

#define VIDEO_SCALE_PACK(w, h) (((unsigned)(w) << 16) | (unsigned)(h))
#define VIDEO_SCALE_W(p)       ((unsigned)(p) >> 16)
#define VIDEO_SCALE_H(p)       ((unsigned)(p) & 0xFFFFu)
#define FONT_ATLAS_FORMAT_A8   0
#define FONT_ATLAS_FORMAT_A16  1

struct font_atlas
{
   uint8_t *buffer;
   unsigned width, height;
   unsigned dirty_xy0, dirty_xy1;
   int format;
   bool dirty;
};

/* A 32x32 atlas whose texel (x, y) is a function of both */
static uint8_t atlas_buf[32 * 32];
static struct font_atlas atlas;

static uint8_t texel(unsigned x, unsigned y)
{
   return (uint8_t)(x * 7 + y * 13 + 1);
}

static void atlas_grown(void)
{
   unsigned x, y;
   for (y = 0; y < 32; y++)
      for (x = 0; x < 32; x++)
         atlas_buf[y * 32 + x] = texel(x, y);
   memset(&atlas, 0, sizeof(atlas));
   atlas.buffer    = atlas_buf;
   atlas.width     = 32;
   atlas.height    = 32;
   /* as font_cache_grow leaves it: all of it, to be sent again */
   atlas.dirty     = true;
   atlas.dirty_xy0 = 0;
   atlas.dirty_xy1 = VIDEO_SCALE_PACK(32, 32);
}

/* Rows @y0 to @h of a @w wide texture, @pitch bytes a row, hold the
 * atlas's */
static void check_rows(const uint8_t *tex, unsigned w, unsigned y0,
      unsigned h, unsigned pitch)
{
   unsigned x, y;
   for (y = y0; y < h; y++)
      for (x = 0; x < w; x++)
         assert(tex[y * pitch + x] == texel(x, y));
}
#define check_texture(tex, w, h, pitch) check_rows(tex, w, 0, h, pitch)

/* ---- GX2 ---------------------------------------------------------- */
typedef unsigned u32;
#define GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER 1
#define GX2_INVALIDATE_MODE_CPU_TEXTURE          2
/* What the last invalidate covered */
static int gx2_mode;
static const uint8_t *gx2_at;
static size_t gx2_len;
static void GX2Invalidate(int mode, void *at, u32 len)
{
   gx2_mode = mode;
   gx2_at   = (const uint8_t*)at;
   gx2_len  = len;
}
/* gfx/drivers/gx2_shaders/sprite.h's shape: nine 32-bit words */
typedef struct
{
   struct { float x, y, width, height; } pos;
   struct { float u, v, width, height; } coord;
   u32 color;
} sprite_vertex_t;
typedef struct
{
   struct
   {
      sprite_vertex_t *v;
      int size;
      int current;
   } vertex_cache;
} wiiu_video_t;
typedef struct
{
   struct
   {
      struct { void *image; unsigned width, height, pitch; } surface;
   } texture;
   struct font_atlas *atlas;
} gx2_font_t;
#include "gx2.inc"

/* ---- GXM ---------------------------------------------------------- */
typedef struct { void *data; unsigned width, height; } SceGxmTexture;
typedef struct { SceGxmTexture gxm_tex; } gxm_texture_t;
static void *sceGxmTextureGetData(const SceGxmTexture *t) { return t->data; }
static unsigned sceGxmTextureGetWidth(const SceGxmTexture *t) { return t->width; }
static unsigned sceGxmTextureGetHeight(const SceGxmTexture *t) { return t->height; }
/* U8 rows, the width to 8, as gxm_gfx.c lays them */
static unsigned gxm_texture_get_stride(const gxm_texture_t *t)
{
   return (sceGxmTextureGetWidth(&t->gxm_tex) + 7) & ~7u;
}
typedef struct
{
   gxm_texture_t *texture;
   struct font_atlas *atlas;
} vita_font_t;
#include "gxm.inc"

/* ---- RSX ---------------------------------------------------------- */
typedef uint8_t u8;
typedef struct
{
   struct { void *data; } texture;
   struct font_atlas *atlas;
   unsigned tex_dims;
} rsx_font_t;
#include "rsx.inc"

int main(void)
{
   /* GX2: the sprites' bytes, and an image the atlas outgrew */
   {
      static sprite_vertex_t sprites[8];
      wiiu_video_t wiiu;
      gx2_font_t font;
      uint8_t *image = (uint8_t*)malloc(16 * 16);
      memset(&wiiu, 0, sizeof(wiiu));
      wiiu.vertex_cache.v       = sprites;
      wiiu.vertex_cache.current = 2;
      gx2_font_invalidate_sprites(&wiiu, 3);
      assert(gx2_mode == GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER);
      assert(gx2_at == (const uint8_t*)&sprites[2]);
      assert(gx2_len == 3 * sizeof(sprite_vertex_t));

      atlas_grown();
      memset(&font, 0, sizeof(font));
      font.atlas                    = &atlas;
      font.texture.surface.image    = image;
      font.texture.surface.width    = 16;
      font.texture.surface.height   = 16;
      font.texture.surface.pitch    = 16;
      gx2_font_upload_dirty(&font);
      check_texture(image, 16, 16, 16);
      assert(gx2_mode == GX2_INVALIDATE_MODE_CPU_TEXTURE);
      assert(gx2_at == image && gx2_len == 16 * 16);
      assert(!atlas.dirty);
      free(image);
   }

   /* GXM: a texture the atlas outgrew, and an empty dirty box */
   {
      gxm_texture_t texture;
      vita_font_t font;
      uint8_t *data = (uint8_t*)malloc(16 * 16);
      atlas_grown();
      texture.gxm_tex.data   = data;
      texture.gxm_tex.width  = 16;
      texture.gxm_tex.height = 16;
      font.texture           = &texture;
      font.atlas             = &atlas;
      gxm_font_upload_dirty(&font);
      check_texture(data, 16, 16, 16);
      assert(!atlas.dirty);
      memset(data, 0, 16 * 16);
      atlas.dirty     = true;
      atlas.dirty_xy0 = VIDEO_SCALE_PACK(8, 8);
      atlas.dirty_xy1 = VIDEO_SCALE_PACK(8, 8);
      gxm_font_upload_dirty(&font);
      check_texture(data, 16, 16, 16);
      free(data);
   }

   /* RSX: texture memory the atlas outgrew, rows and band */
   {
      rsx_font_t font;
      uint8_t *data = (uint8_t*)malloc(16 * 16);
      atlas_grown();
      font.texture.data = data;
      font.atlas        = &atlas;
      font.tex_dims     = VIDEO_SCALE_PACK(16, 16);
      rsx_font_copy_dirty(&font);
      check_texture(data, 16, 16, 16);
      free(data);
      data              = (uint8_t*)malloc(32 * 32);
      font.texture.data = data;
      font.tex_dims     = VIDEO_SCALE_PACK(32, 32);
      atlas.dirty_xy0   = VIDEO_SCALE_PACK(0, 4);
      atlas.dirty_xy1   = VIDEO_SCALE_PACK(32, 32);
      rsx_font_copy_dirty(&font);
      check_rows(data, 32, 4, 32, 32);
      free(data);
   }

   puts("font atlas uploads: GX2, GXM and RSX clamped to a texture the"
         " atlas outgrew, GX2 sprites flushed whole PASS");
   return 0;
}
