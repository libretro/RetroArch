/* The font atlas uploads of the GX2, GXM, RSX and D3D11 drivers
 * (extracted by run.py) against textures in memory.
 *
 * The glyph cache grows its atlas, and each driver makes its texture
 * again at the new size; when that fails the old, smaller texture
 * stays, and the dirty region, the whole grown atlas, is larger than
 * it. Every texture here is allocated exactly its size, so an upload
 * that is not clamped to the texture runs past it, which the sanitizer
 * reports, and every byte it does write is checked against the atlas.
 * GX2's glyph sprites are flushed from the CPU's cache before the GPU
 * reads them: the flushed size must be that of the sprites. D3D11's
 * font texture is made again on a device whose texture creation fails:
 * the texture the font has must be the one it had, unreleased. */
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
#define FONT_ATLAS_DIRTY_MAX   16

struct font_atlas
{
   uint8_t *buffer;
   unsigned width, height;
   unsigned dirty_xy0, dirty_xy1;
   unsigned dirty_rect[FONT_ATLAS_DIRTY_MAX][2];
   unsigned dirty_rects;
   int format;
   bool dirty;
};

/* gfx/font_driver.h's readers */
static unsigned font_atlas_dirty_rects(const struct font_atlas *a)
{
   return a->dirty_rects ? a->dirty_rects : 1;
}
static void font_atlas_dirty_rect(const struct font_atlas *a,
      unsigned i, unsigned *xy0, unsigned *xy1)
{
   if (a->dirty_rects)
   {
      *xy0 = a->dirty_rect[i][0];
      *xy1 = a->dirty_rect[i][1];
   }
   else
   {
      *xy0 = a->dirty_xy0;
      *xy1 = a->dirty_xy1;
   }
}

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

/* Two cells in opposite corners dirtied since the last upload */
static void atlas_two_cells(void)
{
   atlas.dirty          = true;
   atlas.dirty_xy0      = 0;
   atlas.dirty_xy1      = VIDEO_SCALE_PACK(32, 32);
   atlas.dirty_rect[0][0] = 0;
   atlas.dirty_rect[0][1] = VIDEO_SCALE_PACK(4, 4);
   atlas.dirty_rect[1][0] = VIDEO_SCALE_PACK(24, 26);
   atlas.dirty_rect[1][1] = VIDEO_SCALE_PACK(32, 32);
   atlas.dirty_rects    = 2;
}

/* A 32x32 texture, @pitch bytes a row, filled with 0xEE, holds the
 * atlas's texels in the two cells and nothing anywhere else */
static void check_two_cells(const uint8_t *tex, unsigned pitch)
{
   unsigned x, y;
   for (y = 0; y < 32; y++)
      for (x = 0; x < 32; x++)
      {
         bool in = (x < 4 && y < 4) || (x >= 24 && y >= 26);
         assert(tex[y * pitch + x] == (in ? texel(x, y) : 0xEE));
      }
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

/* ---- D3D11 -------------------------------------------------------- */
typedef unsigned DXGI_FORMAT;
#define DXGI_FORMAT_A8_UNORM  65
#define DXGI_FORMAT_R16_UNORM 56
#define RARCH_FILTER_LINEAR 1
#define RARCH_WRAP_BORDER   0
typedef struct object { int refs; } object;
typedef object *D3D11SamplerStateRef;
typedef struct
{
   object *handle, *staging, *view;
   struct { unsigned Width, Height; DXGI_FORMAT Format; } desc;
   D3D11SamplerStateRef sampler;
} d3d11_texture_t;
typedef struct
{
   void *device, *context;
   D3D11SamplerStateRef samplers[2][4];
} d3d11_video_t;
typedef struct
{
   d3d11_texture_t texture;
   struct font_atlas *atlas;
} d3d11_font_t;
static object made[3][3];
static unsigned makes;
static bool make_fails;
static unsigned uploads;
/* As the driver's: makes the resources, or fails having made the
 * texture but none of its views */
static bool d3d11_init_texture(void *device, d3d11_texture_t *t)
{
   object *o = made[makes % 3];
   (void)device;
   makes++;
   o[0].refs = 1;
   t->handle = &o[0];
   if (make_fails)
      return false;
   o[1].refs  = o[2].refs = 1;
   t->staging = &o[1];
   t->view    = &o[2];
   return true;
}
/* As the driver's: lets go of each resource, leaving the pointers */
static void release(object *o) { if (o) { assert(o->refs > 0); o->refs--; } }
static void d3d11_release_texture(d3d11_texture_t *t)
{
   release(t->handle);
   release(t->staging);
   release(t->view);
}
static void d3d11_update_texture(void *ctx, unsigned w, unsigned h,
      unsigned pitch, DXGI_FORMAT fmt, const void *data, d3d11_texture_t *t)
{
   (void)ctx; (void)w; (void)h; (void)pitch; (void)fmt; (void)data;
   assert(t->staging && t->staging->refs > 0);
   uploads++;
}
static void d3d11_font_update_atlas_region(void *ctx, d3d11_font_t *font,
      unsigned xy0, unsigned xy1)
{
   (void)ctx; (void)xy0; (void)xy1;
   assert(font->texture.staging && font->texture.staging->refs > 0);
   uploads++;
}
#include "d3d11.inc"

/* A context whose staging texture can be the GPU's still, and the
 * texture and staging texels it copies between */
typedef long HRESULT;
typedef unsigned UINT;
#define FAILED(hr) ((hr) < 0)
#define DXGI_ERROR_WAS_STILL_DRAWING (-2005270518L)
#define D3D11_MAP_WRITE 2
#define D3D11_MAP_FLAG_DO_NOT_WAIT 0x100000L
typedef object *D3D11Resource;
typedef struct { void *pData; UINT RowPitch, DepthPitch; } D3D11_MAPPED_SUBRESOURCE;
typedef struct { UINT left, top, front, right, bottom, back; } D3D11_BOX;
static uint8_t tex_px[32 * 32], staging_px[32 * 32];
static bool still_drawing;
static unsigned maps, copies, updates;
typedef struct ctx ctx_t;
struct ctx
{
   struct ctx_vtbl
   {
      HRESULT (*Map)(ctx_t*, D3D11Resource, UINT, int, long,
            D3D11_MAPPED_SUBRESOURCE*);
      void (*Unmap)(ctx_t*, D3D11Resource, UINT);
      void (*UpdateSubresource)(ctx_t*, D3D11Resource, UINT,
            const D3D11_BOX*, const void*, UINT, UINT);
      void (*CopySubresourceRegion)(ctx_t*, D3D11Resource, UINT, UINT,
            UINT, UINT, D3D11Resource, UINT, const D3D11_BOX*);
   } *lpVtbl;
};
typedef ctx_t *D3D11DeviceContext;
static HRESULT ctx_map(ctx_t *c, D3D11Resource r, UINT s, int t, long f,
      D3D11_MAPPED_SUBRESOURCE *m)
{
   (void)c; (void)r; (void)s; (void)t; (void)f;
   if (still_drawing)
      return DXGI_ERROR_WAS_STILL_DRAWING;
   maps++;
   m->pData    = staging_px;
   m->RowPitch = 32;
   return 0;
}
static void ctx_unmap(ctx_t *c, D3D11Resource r, UINT s)
{
   (void)c; (void)r; (void)s;
}
static void ctx_update(ctx_t *c, D3D11Resource r, UINT s,
      const D3D11_BOX *b, const void *src, UINT pitch, UINT depth)
{
   UINT y;
   (void)c; (void)r; (void)s; (void)depth;
   updates++;
   for (y = b->top; y < b->bottom; y++)
      memcpy(tex_px + y * 32 + b->left,
            (const uint8_t*)src + (y - b->top) * pitch, b->right - b->left);
}
static void ctx_copy(ctx_t *c, D3D11Resource dst, UINT ds, UINT x, UINT y,
      UINT z, D3D11Resource src, UINT ss, const D3D11_BOX *b)
{
   UINT row;
   (void)c; (void)dst; (void)ds; (void)z; (void)src; (void)ss;
   copies++;
   for (row = 0; row < b->bottom - b->top; row++)
      memcpy(tex_px + (y + row) * 32 + x,
            staging_px + (b->top + row) * 32 + b->left, b->right - b->left);
}
static struct ctx_vtbl ctx_vtbl = { ctx_map, ctx_unmap, ctx_update, ctx_copy };
#include "d3d11_rects.inc"

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

      /* two cells far apart go in on their own, each flushed */
      image = (uint8_t*)malloc(32 * 32);
      memset(image, 0xEE, 32 * 32);
      font.texture.surface.image  = image;
      font.texture.surface.width  = 32;
      font.texture.surface.height = 32;
      font.texture.surface.pitch  = 32;
      atlas_two_cells();
      gx2_font_upload_dirty(&font);
      check_two_cells(image, 32);
      assert(gx2_at == image + 26 * 32 + 24 && gx2_len == 5 * 32 + 8);
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

      /* two cells far apart go in on their own */
      data = (uint8_t*)malloc(32 * 32);
      memset(data, 0xEE, 32 * 32);
      texture.gxm_tex.data   = data;
      texture.gxm_tex.width  = 32;
      texture.gxm_tex.height = 32;
      atlas_two_cells();
      gxm_font_upload_dirty(&font);
      check_two_cells(data, 32);
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

   /* D3D11: the texture made again, and made again on a device that
    * cannot make it */
   {
      d3d11_video_t d3d11;
      d3d11_font_t font;
      object *handle, *staging, *view;
      memset(&d3d11, 0, sizeof(d3d11));
      memset(&font, 0, sizeof(font));
      memset(made, 0, sizeof(made));
      makes = uploads = 0;
      make_fails = false;
      atlas_grown();
      atlas.width = atlas.height = 16;
      font.atlas = &atlas;
      d3d11_font_make_texture(&d3d11, &font);
      assert(font.texture.view && uploads == 1 && !atlas.dirty);
      handle  = font.texture.handle;
      staging = font.texture.staging;
      view    = font.texture.view;
      /* it grows; the new texture cannot be made */
      atlas_grown();
      make_fails = true;
      d3d11_font_make_texture(&d3d11, &font);
      assert(font.texture.handle == handle && font.texture.staging == staging
            && font.texture.view == view);
      assert(handle->refs == 1 && staging->refs == 1 && view->refs == 1);
      assert(font.texture.desc.Width == 16 && uploads == 1);
      /* the next time it can */
      make_fails = false;
      d3d11_font_make_texture(&d3d11, &font);
      assert(font.texture.desc.Width == 32 && uploads == 2 && !atlas.dirty);
      assert(handle->refs == 0 && staging->refs == 0 && view->refs == 0);
      assert(font.texture.view && font.texture.view->refs == 1);
   }

   /* D3D11: two cells far apart go in on their own, through one map of
    * the staging texture, or through the driver while it is the GPU's */
   {
      static object staging_obj = { 1 }, handle_obj = { 1 };
      d3d11_font_t font;
      ctx_t ctx;
      unsigned pass, i, n, r[FONT_ATLAS_DIRTY_MAX][2];
      ctx.lpVtbl = &ctx_vtbl;
      for (pass = 0; pass < 2; pass++)
      {
         memset(&font, 0, sizeof(font));
         atlas_grown();
         atlas_two_cells();
         font.atlas                = &atlas;
         font.texture.handle       = &handle_obj;
         font.texture.staging      = &staging_obj;
         font.texture.desc.Width   = 32;
         font.texture.desc.Height  = 32;
         memset(tex_px, 0xEE, sizeof(tex_px));
         memset(staging_px, 0xEE, sizeof(staging_px));
         maps = copies = updates = 0;
         still_drawing = pass == 1;
         n = font_atlas_dirty_rects(&atlas);
         for (i = 0; i < n; i++)
            font_atlas_dirty_rect(&atlas, i, &r[i][0], &r[i][1]);
         d3d11_font_update_atlas_rects(&ctx, &font,
               (const unsigned (*)[2])r, n);
         check_two_cells(tex_px, 32);
         if (still_drawing)
            assert(updates == 2 && !maps && !copies);
         else
            assert(maps == 1 && copies == 2 && !updates);
      }
   }

   puts("font atlas uploads: GX2, GXM and RSX clamped to a texture the"
         " atlas outgrew, GX2 sprites flushed whole, D3D11 keeps its"
         " texture when a new one cannot be made, dirty cells far apart"
         " sent on their own PASS");
   return 0;
}
