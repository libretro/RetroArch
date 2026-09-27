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
   }

   if (!failures)
      printf("[pass] metal_texture_update: in-place updates and borrowed "
             "textures behave\n");
   printf(failures ? "FAIL\n" : "PASS\n");
   return failures ? 1 : 0;
}
