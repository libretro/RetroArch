/* Real FrameView GPU import and texture/history helpers, without window/UI. */
#ifdef __APPLE__
typedef NativeContext FrameContext;
#else
typedef MockContext FrameContext;
typedef NSSize CGSize;
#define CGSizeMake NSMakeSize
#endif
enum { RPixelFormatBGRA8Unorm, RPixelFormatBGRX8Unorm, RPixelFormatBGR10A2Unorm };
typedef struct texture
{
   __unsafe_unretained id<MTLTexture> view;
   struct { float x, y, z, w; } size_data;
} texture_t;
struct frame_shader { int history_size; };
@interface FrameView : NSObject
{
@public
   FrameContext *_context;
   id<MTLTexture> _src;
   bool resize_render_targets;
   BOOL _movieFrame, _movieHDR, _srcDirty;
   const void *_pendingSrc;
   int _loanLast, _format;
   unsigned history_updates, viewport_updates;
   CGSize _size;
   struct frame_shader *_shader;
   bool init_history;
   struct { struct { texture_t texture[4]; } frame; } _engine;
   texture_t _moviePrevious[4];
}
@property(nonatomic) CGSize size;
- (void)syncViewport;
- (void)_clearMovieFrame;
- (void)_updateHistory;
- (void)_ensureFront;
- (void)_initTexture:(texture_t*)texture withDescriptor:(MTLTextureDescriptor*)descriptor;
- (enum video_texture_update)importMovieFrame:(const video_frame_import_t*)frame;
@end
@implementation FrameView
@synthesize size = _size;
- (void)syncViewport { viewport_updates++; }
- (void)dealloc
{
   for (unsigned i = 0; i < 4; i++)
   {
      RARCH_STRUCT_ASSIGN(_engine.frame.texture[i].view, nil);
      RARCH_STRUCT_ASSIGN(_moviePrevious[i].view, nil);
   }
   RARCH_RELEASE(_src);
   RARCH_SUPER_DEALLOC();
}
/* SHIPPING_FRAME_METHODS */
@end
