/* Real Objective-C ownership around the shipping pool selector and teardown.
 * This runs with GNUstep MRC and with Apple Foundation ARC, without a GPU. */
#import <Foundation/Foundation.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <defines/cocoa_defines.h>
#define GFX_MAX_FRAME_HISTORY 2
@protocol MTLTexture <NSObject> @end
static unsigned live_versions;
@interface PoolVersion : NSObject @end
@implementation PoolVersion
- (id)init { if ((self = [super init])) live_versions++; return self; }
- (void)dealloc
{
   live_versions--;
   RARCH_SUPER_DEALLOC();
}
@end
@interface Texture : NSObject
{
@public
   /* POOL_FIELDS */
   id<MTLTexture> _texture;
   id _sampler, _staging;
}
@property(nonatomic, strong) id<MTLTexture> texture;
@property(nonatomic, strong) id sampler, staging;
@end
@implementation Texture
@synthesize texture = _texture, sampler = _sampler, staging = _staging;
/* POOL_DEALLOC */
@end

typedef struct {
   struct { struct { struct { __unsafe_unretained id<MTLTexture> view; }
      texture[GFX_MAX_FRAME_HISTORY+1]; } frame; } engine;
   struct { unsigned count; } history_ring;
   bool history_borrowed;
} metal_view_chain_t;
typedef struct { metal_view_chain_t *media_chain; } PoolViews;
@interface MetalDriver : NSObject
{ @public PoolViews _views; }
@property(readonly) PoolViews *views;
@end
@implementation MetalDriver
- (PoolViews *)views { return &_views; }
@end
static id<MTLTexture> metal_media_new_texture(id<MTLTexture> source)
{
   (void)source;
   return (id<MTLTexture>)RARCH_AUTORELEASE_R([PoolVersion new]);
}
/* POOL_SELECTOR */
int main(void)
{
   metal_view_chain_t chain = {0};
   unsigned i;
   RARCH_AUTORELEASEPOOL_BEGIN
   MetalDriver *driver = [MetalDriver new];
   chain.history_ring.count = 2;
   chain.history_borrowed = true;
   driver->_views.media_chain = &chain;
   RARCH_AUTORELEASEPOOL_BEGIN
   Texture *owner = [Texture new];
   owner.texture = (id<MTLTexture>)RARCH_AUTORELEASE_R([PoolVersion new]);
   owner->_mediaTextures = (__typeof__(owner->_mediaTextures))calloc(GFX_MAX_FRAME_HISTORY+2, sizeof(*owner->_mediaTextures));
   assert(owner->_mediaTextures);
   RARCH_STRUCT_ASSIGN(owner->_mediaTextures[0], owner.texture);
   owner->_mediaTextureCount = 1;
   RARCH_STRUCT_ASSIGN(chain.engine.frame.texture[0].view, owner.texture);
   for (i=0; i<128; i++)
   {
      RARCH_AUTORELEASEPOOL_BEGIN
      id<MTLTexture> writable = metal_media_writable(driver, owner);
      assert(writable && writable != chain.engine.frame.texture[0].view
            && writable != chain.engine.frame.texture[1].view);
      owner.texture = writable;
      RARCH_STRUCT_ASSIGN(chain.engine.frame.texture[1].view,
            chain.engine.frame.texture[0].view);
      RARCH_STRUCT_ASSIGN(chain.engine.frame.texture[0].view, owner.texture);
      assert(owner->_mediaTextureCount <= 3);
      RARCH_AUTORELEASEPOOL_END
   }
   RARCH_RELEASE_NIL(owner);
   RARCH_AUTORELEASEPOOL_END
   /* The producer is gone; history still owns its two immutable versions. */
   assert(live_versions == 2);
   RARCH_STRUCT_ASSIGN(chain.engine.frame.texture[0].view, nil);
   RARCH_STRUCT_ASSIGN(chain.engine.frame.texture[1].view, nil);
   assert(live_versions == 0);
   RARCH_RELEASE_NIL(driver);
   RARCH_AUTORELEASEPOOL_END
   puts("Metal history pool: bounded versions survive producer close and release cleanly");
   return 0;
}
