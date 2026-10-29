/* gfx/drivers/gxm_gfx.c's upload of the core's frame into the texture
 * it is drawn from, on the host against the tree's psp2 stubs, with GPU
 * memory blocks malloc'd at the size asked for and a guard after each.
 * Held to:
 *
 * - each texture row gets the frame's row, the frame's width of it and
 *   no more, whether the core's rows are longer than the texture's,
 *   as long, or shorter;
 * - nothing is written past the texture's memory, a width that is not
 *   a multiple of 8 included, and nothing read past the frame's last
 *   row's pixels;
 * - rows that line up go over in one copy, others one copy a row;
 * - a frame of new dimensions gets a new texture, the old one freed. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#ifdef _WIN32
/* The driver names its clip planes near and far, which windef.h,
 * reached through the libretro-common headers, defines away */
#include <windows.h>
#undef near
#undef far
#endif

/* The driver's two tables made static and so, unused, never emitted,
 * with all the driver they reach: the upload, and what it calls, is
 * all that is left to link. Their extern declarations are read first,
 * as the driver would. */
#include "../../../gfx/video_driver.h"
#include "../../../gfx/gfx_display.h"
#define video_gxm           static unused_video_gxm
#define gfx_display_ctx_gxm static unused_gfx_display_ctx_gxm

static unsigned memcpys;
static void *count_memcpy(void *dst, const void *src, size_t n);
#define memcpy(dst, src, n) count_memcpy(dst, src, n)
#include "../../../gfx/drivers/gxm_gfx.c"
#undef memcpy
/* psp_defines.h sends printf to the Vita's own */
#undef printf

int sceClibPrintf(const char *fmt, ...)
{
   int n;
   va_list ap;
   va_start(ap, fmt);
   n = vprintf(fmt, ap);
   va_end(ap);
   return n;
}

static void *count_memcpy(void *dst, const void *src, size_t n)
{
   memcpys++;
   return memcpy(dst, src, n);
}

static unsigned failures;

#define CHECK(c, what) do { if (!(c)) { \
   printf("FAIL: %s (%s:%d)\n", what, __FILE__, __LINE__); \
   failures++; } else printf("   ok   %s\n", what); } while (0)

/* ---- memory blocks ---- */
#define MAX_BLOCKS 16
#define GUARD      65536

static uint8_t *blocks[MAX_BLOCKS + 1];
static size_t   block_size[MAX_BLOCKS + 1];
static unsigned blocks_live;

SceUID sceKernelAllocMemBlock(const char *name, SceKernelMemBlockType type,
      SceSize size, SceKernelAllocMemBlockOpt *opt)
{
   int i;
   (void)name; (void)type; (void)opt;
   for (i = 1; i <= MAX_BLOCKS; i++)
      if (!blocks[i])
      {
         blocks[i]     = (uint8_t*)malloc(size + GUARD);
         block_size[i] = size;
         memset(blocks[i] + size, 0x5a, GUARD);
         blocks_live++;
         return i;
      }
   return -1;
}

int sceKernelGetMemBlockBase(SceUID uid, void **base)
{
   if (uid < 1 || uid > MAX_BLOCKS || !blocks[uid])
      return -1;
   *base = blocks[uid];
   return 0;
}

int sceKernelFreeMemBlock(SceUID uid)
{
   if (uid < 1 || uid > MAX_BLOCKS || !blocks[uid])
      return -1;
   free(blocks[uid]);
   blocks[uid] = NULL;
   blocks_live--;
   return 0;
}

static int guards_intact(void)
{
   int i, j;
   for (i = 1; i <= MAX_BLOCKS; i++)
      if (blocks[i])
         for (j = 0; j < GUARD; j++)
            if (blocks[i][block_size[i] + j] != 0x5a)
               return 0;
   return 1;
}

int sceGxmMapMemory(void *base, SceSize size, SceGxmMemoryAttribFlags attr)
{ (void)base; (void)size; (void)attr; return 0; }
int sceGxmUnmapMemory(void *base) { (void)base; return 0; }
void sceGxmFinish(SceGxmContext *context) { (void)context; }
int sceGxmDestroyRenderTarget(SceGxmRenderTarget *target)
{ (void)target; return 0; }

/* ---- textures: the stub's fields are the target's 30-bit ones, so the
 * host keeps each texture's own here ---- */
struct tex_rec
{
   const SceGxmTexture *tex;
   void *data;
   unsigned w, h;
   SceGxmTextureFormat fmt;
};
static struct tex_rec texs[MAX_BLOCKS];

static struct tex_rec *tex_find(const SceGxmTexture *t, int make)
{
   int i;
   for (i = 0; i < MAX_BLOCKS; i++)
      if (texs[i].tex == t)
         return &texs[i];
   if (make)
      for (i = 0; i < MAX_BLOCKS; i++)
         if (!texs[i].tex)
            return &texs[i];
   return NULL;
}

int sceGxmTextureInitLinear(SceGxmTexture *texture, const void *data,
      SceGxmTextureFormat fmt, unsigned int w, unsigned int h,
      unsigned int mips)
{
   struct tex_rec *r = tex_find(texture, 1);
   (void)mips;
   r->tex  = texture;
   r->data = (void*)data;
   r->w    = w;
   r->h    = h;
   r->fmt  = fmt;
   return 0;
}
void *sceGxmTextureGetData(const SceGxmTexture *t)
{ return tex_find(t, 0)->data; }
unsigned int sceGxmTextureGetWidth(const SceGxmTexture *t)
{ return tex_find(t, 0)->w; }
unsigned int sceGxmTextureGetHeight(const SceGxmTexture *t)
{ return tex_find(t, 0)->h; }
SceGxmTextureFormat sceGxmTextureGetFormat(const SceGxmTexture *t)
{ return tex_find(t, 0)->fmt; }
int sceGxmTextureSetMagFilter(SceGxmTexture *t, SceGxmTextureFilter f)
{ (void)t; (void)f; return 0; }
int sceGxmTextureSetMinFilter(SceGxmTexture *t, SceGxmTextureFilter f)
{ (void)t; (void)f; return 0; }
int sceGxmTextureSetPalette(SceGxmTexture *t, const void *p)
{ (void)t; (void)p; return 0; }

/* ---- the checks ---- */

/* A frame of @h rows @pitch bytes apart, allocated to the end of its
 * last row's @w pixels and no further */
static uint8_t *make_frame(unsigned w, unsigned h, unsigned pitch,
      unsigned bpp, unsigned seed)
{
   size_t   size = (size_t)pitch * (h - 1) + (size_t)w * bpp;
   uint8_t *f    = (uint8_t*)malloc(size);
   size_t   i;
   for (i = 0; i < size; i++)
      f[i] = (uint8_t)(i * 7 + seed);
   return f;
}

static int rows_match(const vita_video_t *vita, const uint8_t *frame,
      unsigned w, unsigned h, unsigned pitch, unsigned bpp)
{
   const uint8_t *tex = (const uint8_t*)
      sceGxmTextureGetData(&vita->texture->gxm_tex);
   unsigned stride    = gxm_texture_get_stride(vita->texture);
   unsigned y;
   for (y = 0; y < h; y++)
      if (memcmp(tex + (size_t)y * stride, frame + (size_t)y * pitch,
               (size_t)w * bpp))
         return 0;
   return 1;
}

/* Past the frame's width, each texture row as it was made: cleared.
 * Rows that go over in one copy bring the frame's padding with them,
 * so only those copied a row at a time are held to it. */
static int padding_clear(const vita_video_t *vita, unsigned w, unsigned h,
      unsigned bpp)
{
   const uint8_t *tex = (const uint8_t*)
      sceGxmTextureGetData(&vita->texture->gxm_tex);
   unsigned stride    = gxm_texture_get_stride(vita->texture);
   unsigned y, x;
   for (y = 0; y < h; y++)
      for (x = w * bpp; x < stride; x++)
         if (tex[(size_t)y * stride + x])
            return 0;
   return 1;
}

static void upload(vita_video_t *vita, unsigned w, unsigned h,
      unsigned pitch, unsigned bpp, unsigned seed, unsigned expect_copies,
      const char *what)
{
   uint8_t *frame = make_frame(w, h, pitch, bpp, seed);
   unsigned m0    = memcpys;
   char     msg[160];

   gxm_upload_frame(vita, frame, VIDEO_SCALE_PACK(w, h), pitch);
   snprintf(msg, sizeof(msg), "%s: rows copied", what);
   CHECK(vita->texture && rows_match(vita, frame, w, h, pitch, bpp), msg);
   snprintf(msg, sizeof(msg), "%s: nothing past the texture", what);
   CHECK(guards_intact(), msg);
   if (expect_copies > 1)
   {
      snprintf(msg, sizeof(msg), "%s: no more than the width a row", what);
      CHECK(padding_clear(vita, w, h, bpp), msg);
   }
   snprintf(msg, sizeof(msg), "%s: %u copies (%u made)", what,
         expect_copies, memcpys - m0);
   CHECK(memcpys - m0 == expect_copies, msg);
   free(frame);
}

int main(void)
{
   vita_video_t vita;

   /* GPU memory rounded to 4 KiB, not the 256 KiB of CDRAM, so the
    * guard sits close behind each texture */
   MemBlockType = SCE_KERNEL_MEMBLOCK_TYPE_USER_RW;

   memset(&vita, 0, sizeof(vita));
   vita.format = SCE_GXM_TEXTURE_FORMAT_X8U8U8U8_1RGB;

   /* A core's rows longer than the texture's, a width not of 8 */
   upload(&vita, 257, 224, 512 * 4, 4, 1, 224,
         "xrgb, 257 wide, rows of 512");
   /* Rows as long as the texture's: one copy */
   upload(&vita, 320, 240, 320 * 4, 4, 2, 1, "xrgb, rows aligned");
   CHECK(blocks_live == 1, "the old texture freed for the new size");

   gxm_free_texture(vita.texture);
   memset(&vita, 0, sizeof(vita));
   vita.format = SCE_GXM_TEXTURE_FORMAT_R5G6B5;
   /* 250 wide, so the texture's rows are 256 */
   upload(&vita, 250, 100, 300 * 2, 2, 4, 100, "rgb565, rows of 300");
   /* Shorter rows than the texture's */
   upload(&vita, 250, 100, 250 * 2, 2, 5, 100, "rgb565, rows of 250");
   upload(&vita, 250, 100, 256 * 2, 2, 3, 1, "rgb565, rows of 256");
   gxm_free_texture(vita.texture);
   CHECK(blocks_live == 0, "every texture freed");

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("PASS gxm_frame_upload_test\n");
   return 0;
}
