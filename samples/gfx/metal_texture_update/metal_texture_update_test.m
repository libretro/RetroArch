/* metal_texture_update_test.m -- the two Metal behaviours the surface
 * and overlay paths are built on, asserted on a real device.
 *
 * Neither is exercised by anything else we run: the driver's Metal
 * paths have only ever been compiled. This does not link metal.m -
 * that drags in the whole frontend - it reproduces the two patterns
 * the driver uses and checks Metal actually behaves the way they
 * assume.
 *
 *   1. In-place update. gfx_surface streams a preview frame by
 *      replaceRegion: on a texture the GPU may still be sampling, and
 *      unlike Vulkan (a fence check), D3D11 (DO_NOT_WAIT) or D3D12 (a
 *      fence tag), the Metal path has nothing to drop a frame with.
 *      So: fill, encode a pass that samples it, replace the contents
 *      while that pass is in flight, and read the texture back. What
 *      must hold is that the write lands and the texture is not
 *      corrupted - Apple's managed/shared storage is documented to
 *      make replaceRegion: safe from the CPU, and if that is wrong on
 *      a real device this is where it shows.
 *
 *   2. Borrowed textures. The overlay pack owns its MTLTextures and
 *      hands the driver a page of them; the page keeps them in an
 *      NSMutableArray and a page switch replaces that array. The
 *      textures must survive the switch, and releasing the page must
 *      not take them with it - a pack freed mid-upload is the case
 *      that broke gl2 on Android. So: two pages over one set of
 *      textures, switch, drop, and use the textures afterwards.
 *
 *   3. The lent framebuffer. FrameView lendFramebuffer: hands a core
 *      the contents of a shared-storage MTLBuffer with a linear
 *      texture over it at the tight row pitch (width * bpp), and a
 *      frame pushed as a window into it is blitted out with a source
 *      origin. So: for the widths cores use, in BGRA8 and R16Uint
 *      (RGB565's texture), make the buffer and the linear texture at
 *      the tight pitch, write a pattern from the CPU, blit a window
 *      out with an origin, and read it back - the window's pixels,
 *      not the loan's corner. Then write again after the blit's
 *      command buffer completed and blit again: the second write is
 *      what the GPU reads, which is the ordering the next lend's
 *      waitUntilCompleted relies on.
 *
 * A failure here is a real defect in an assumption the design rests
 * on, not a test artefact.
 */

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned failures;

#define CHECK(cond, ...) do { \
   if (!(cond)) { printf("[FAIL] "); printf(__VA_ARGS__); printf("\n"); failures++; } \
} while (0)

#define W 64
#define H 64

static id<MTLTexture> make_texture(id<MTLDevice> dev)
{
   MTLTextureDescriptor *td =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                         width:W
                                                        height:H
                                                     mipmapped:NO];
   td.usage = MTLTextureUsageShaderRead;
#if TARGET_OS_OSX
   td.storageMode = MTLStorageModeManaged;
#else
   td.storageMode = MTLStorageModeShared;
#endif
   return [dev newTextureWithDescriptor:td];
}

static void fill(uint32_t *px, uint32_t value)
{
   unsigned i;
   for (i = 0; i < W * H; i++)
      px[i] = value;
}

static uint32_t first_pixel(id<MTLTexture> t)
{
   uint32_t out = 0;
   [t getBytes:&out
       bytesPerRow:sizeof(uint32_t)
        fromRegion:MTLRegionMake2D(0, 0, 1, 1)
       mipmapLevel:0];
   return out;
}

/* One texture, many replaceRegion: calls with GPU work queued against
 * it in between - the animated preview's steady state. */
static void lane_in_place_update(id<MTLDevice> dev, id<MTLCommandQueue> q)
{
   id<MTLTexture> t = make_texture(dev);
   uint32_t *px     = (uint32_t*)malloc(W * H * sizeof(uint32_t));
   unsigned frame;

   CHECK(t != nil, "texture creation failed");
   CHECK(px != NULL, "out of memory");
   if (!t || !px)
      return;

   for (frame = 0; frame < 64; frame++)
   {
      uint32_t want = 0xff000000u | (frame * 0x010203u);
      id<MTLCommandBuffer> cb;

      fill(px, want);
      [t replaceRegion:MTLRegionMake2D(0, 0, W, H)
           mipmapLevel:0
             withBytes:px
           bytesPerRow:4 * W];

      /* Work that reads the texture, left in flight on purpose: the
       * next iteration replaces its contents without waiting, which
       * is what the surface does with a preview frame. */
      cb = [q commandBuffer];
      {
         id<MTLBlitCommandEncoder> blit = [cb blitCommandEncoder];
#if TARGET_OS_OSX
         [blit synchronizeResource:t];
#endif
         [blit endEncoding];
      }
      [cb commit];

      if ((frame & 7) == 7)
      {
         [cb waitUntilCompleted];
         CHECK(first_pixel(t) == want,
               "frame %u: texture holds %08x, wanted %08x",
               frame, first_pixel(t), want);
      }
   }
   free(px);
}

/* Two pages over one set of textures: the page holds them, a switch
 * replaces the page, and dropping a page must not take the pack's
 * textures with it. */
static void lane_borrowed_textures(id<MTLDevice> dev)
{
   NSMutableArray *pack = [NSMutableArray arrayWithCapacity:3];
   uint32_t *px         = (uint32_t*)malloc(W * H * sizeof(uint32_t));
   unsigned i;

   CHECK(px != NULL, "out of memory");
   if (!px)
      return;
   for (i = 0; i < 3; i++)
   {
      id<MTLTexture> t = make_texture(dev);
      CHECK(t != nil, "pack texture %u", i);
      if (!t)
      {
         free(px);
         return;
      }
      fill(px, 0xff000000u | (i + 1));
      [t replaceRegion:MTLRegionMake2D(0, 0, W, H)
           mipmapLevel:0
             withBytes:px
           bytesPerRow:4 * W];
      [pack addObject:t];
   }

   @autoreleasepool
   {
      NSMutableArray *page = [NSMutableArray arrayWithCapacity:2];
      [page addObject:pack[0]];
      [page addObject:pack[1]];
      /* A page switch: the driver builds a new list over the same
       * textures and lets the old one go. */
      page = [NSMutableArray arrayWithCapacity:2];
      [page addObject:pack[1]];
      [page addObject:pack[2]];
      page = nil;
   }

   /* The pack still owns them, and their contents are intact. */
   for (i = 0; i < 3; i++)
   {
      id<MTLTexture> t = pack[i];
      uint32_t want    = 0xff000000u | (i + 1);
      CHECK(t != nil, "pack texture %u went with the page", i);
      if (t)
         CHECK(first_pixel(t) == want,
               "pack texture %u holds %08x after the page switch, wanted %08x",
               i, first_pixel(t), want);
   }
   free(px);
}

/* The lent framebuffer: a linear texture over a shared buffer at the
 * tight pitch, a window blitted out of it with an origin, and a CPU
 * write after the reader completed. */
static void lane_lent_framebuffer(id<MTLDevice> dev, id<MTLCommandQueue> q,
      MTLPixelFormat fmt, unsigned bpp, unsigned width, unsigned height)
{
   NSUInteger align  = [dev minimumLinearTextureAlignmentForPixelFormat:fmt];
   NSUInteger tight  = (NSUInteger)width * bpp;
   NSUInteger stride;
   const unsigned win_x = 8, win_y = 4, win_w = width - 16, win_h = height - 8;
   id<MTLBuffer>  loan;
   id<MTLTexture> loan_tex, dst;
   MTLTextureDescriptor *td;
   unsigned pass;

   if (align < 16)
      align = 16;
   stride = (tight + align - 1) & ~(align - 1);
   /* The driver declines a padded pitch and the core keeps its own
    * buffer; nothing to assert then. Widths cores use should not be
    * padded on any Apple GPU, so report it. */
   if (stride != tight)
   {
      printf("[note] lent framebuffer: %ux%u fmt %u needs a row pitch of %lu, "
             "not %lu - the driver declines this lend\n",
             width, height, (unsigned)fmt,
             (unsigned long)stride, (unsigned long)tight);
      return;
   }

   loan = [dev newBufferWithLength:stride * height
                           options:MTLResourceStorageModeShared];
   CHECK(loan != nil, "lent framebuffer: buffer %ux%u fmt %u", width, height, (unsigned)fmt);
   if (!loan)
      return;
   td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:fmt
                                                           width:width
                                                          height:height
                                                       mipmapped:NO];
   td.storageMode = MTLStorageModeShared;
   td.usage       = MTLTextureUsageShaderRead;
   loan_tex = [loan newTextureWithDescriptor:td offset:0 bytesPerRow:stride];
   CHECK(loan_tex != nil, "lent framebuffer: linear texture over the buffer at pitch %lu",
         (unsigned long)stride);
   if (!loan_tex)
      return;

   td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:fmt
                                                           width:win_w
                                                          height:win_h
                                                       mipmapped:NO];
   td.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
#if TARGET_OS_OSX
   td.storageMode = MTLStorageModeManaged;
#else
   td.storageMode = MTLStorageModeShared;
#endif
   dst = [dev newTextureWithDescriptor:td];
   CHECK(dst != nil, "lent framebuffer: window texture");
   if (!dst)
      return;

   /* Two passes: the second writes the loan again after the first
    * blit's command buffer completed, as the driver does between
    * lends, and must be what the GPU reads. */
   for (pass = 1; pass <= 2; pass++)
   {
      unsigned x, y;
      uint8_t *base = (uint8_t *)loan.contents;
      id<MTLCommandBuffer> cb;
      id<MTLBlitCommandEncoder> bce;

      /* Pixel (x, y) carries x + y * width + pass in its low bits,
       * so a window's origin and stride both show in the read-back. */
      for (y = 0; y < height; y++)
         for (x = 0; x < width; x++)
         {
            uint32_t v = x + y * width + pass;
            uint8_t *px = base + y * stride + x * bpp;
            if (bpp == 4)
               *(uint32_t *)px = v;
            else
               *(uint16_t *)px = (uint16_t)v;
         }

      cb  = [q commandBuffer];
      bce = [cb blitCommandEncoder];
      [bce copyFromTexture:loan_tex
               sourceSlice:0
               sourceLevel:0
              sourceOrigin:MTLOriginMake(win_x, win_y, 0)
                sourceSize:MTLSizeMake(win_w, win_h, 1)
                 toTexture:dst
          destinationSlice:0
          destinationLevel:0
         destinationOrigin:MTLOriginMake(0, 0, 0)];
#if TARGET_OS_OSX
      [bce synchronizeResource:dst];
#endif
      [bce endEncoding];
      [cb commit];
      [cb waitUntilCompleted];

      {
         /* The window's four corners, read back from the blit's target. */
         const unsigned cx[4] = { 0, win_w - 1, 0,         win_w - 1 };
         const unsigned cy[4] = { 0, 0,         win_h - 1, win_h - 1 };
         unsigned c;
         for (c = 0; c < 4; c++)
         {
            uint8_t  px[4] = { 0, 0, 0, 0 };
            uint32_t got, want = (cx[c] + win_x) + (cy[c] + win_y) * width + pass;
            [dst getBytes:px
               bytesPerRow:bpp
                fromRegion:MTLRegionMake2D(cx[c], cy[c], 1, 1)
               mipmapLevel:0];
            got = (bpp == 4) ? *(uint32_t *)px : *(uint16_t *)px;
            if (bpp == 2)
               want &= 0xffff;
            CHECK(got == want,
                  "lent framebuffer %ux%u fmt %u pass %u: window pixel (%u,%u) read %u, wanted %u",
                  width, height, (unsigned)fmt, pass, cx[c], cy[c], got, want);
         }
      }
   }
}

int main(void)
{
   @autoreleasepool
   {
      id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
      id<MTLCommandQueue> q;

      if (!dev)
      {
         /* No device: say so and do not pretend to have tested. */
         printf("metal_texture_update: no Metal device, skipping\n");
         return 77;
      }
      printf("metal_texture_update: %s\n", [[dev name] UTF8String]);
      q = [dev newCommandQueue];
      CHECK(q != nil, "command queue");
      if (!q)
         return 2;

      lane_in_place_update(dev, q);
      lane_borrowed_textures(dev);
      /* The widths cores lend at: the harness core's 320, beetle-psx's
       * 700 scanout surface, a 640 line. */
      lane_lent_framebuffer(dev, q, MTLPixelFormatBGRA8Unorm, 4, 320, 240);
      lane_lent_framebuffer(dev, q, MTLPixelFormatBGRA8Unorm, 4, 700, 480);
      lane_lent_framebuffer(dev, q, MTLPixelFormatBGRA8Unorm, 4, 640, 480);
      lane_lent_framebuffer(dev, q, MTLPixelFormatR16Uint,    2, 320, 240);
      lane_lent_framebuffer(dev, q, MTLPixelFormatR16Uint,    2, 700, 480);
   }

   if (!failures)
      printf("[pass] metal_texture_update: in-place updates, borrowed "
             "textures and the lent framebuffer behave\n");
   printf(failures ? "FAIL\n" : "PASS\n");
   return failures ? 1 : 0;
}
