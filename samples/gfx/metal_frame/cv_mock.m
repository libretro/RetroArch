typedef const void *CFStringRef;
#define kCVImageBufferColorPrimariesKey ((CFStringRef)@"ColorPrimariesKey")
#define kCVImageBufferTransferFunctionKey ((CFStringRef)@"TransferFunctionKey")
#define kCVImageBufferYCbCrMatrixKey ((CFStringRef)@"YCbCrMatrixKey")
#define kCVImageBufferColorPrimaries_ITU_R_709_2 ((CFStringRef)@"ColorPrimaries_ITU_R_709_2")
#define kCVImageBufferColorPrimaries_ITU_R_2020 ((CFStringRef)@"ColorPrimaries_ITU_R_2020")
#define kCVImageBufferTransferFunction_ITU_R_709_2 ((CFStringRef)@"TransferFunction_ITU_R_709_2")
#define kCVImageBufferTransferFunction_sRGB ((CFStringRef)@"TransferFunction_sRGB")
#define kCVImageBufferTransferFunction_SMPTE_ST_2084_PQ ((CFStringRef)@"TransferFunction_SMPTE_ST_2084_PQ")
#define kCVImageBufferTransferFunction_ITU_R_2100_HLG ((CFStringRef)@"TransferFunction_ITU_R_2100_HLG")
#define kCVImageBufferYCbCrMatrix_ITU_R_709_2 ((CFStringRef)@"YCbCrMatrix_ITU_R_709_2")
#define kCVImageBufferYCbCrMatrix_ITU_R_2020 ((CFStringRef)@"YCbCrMatrix_ITU_R_2020")
enum { kCVReturnSuccess, kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
   kCVPixelFormatType_420YpCbCr8BiPlanarFullRange,
   kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange,
   kCVPixelFormatType_420YpCbCr10BiPlanarFullRange };
static unsigned cv_pixels_live, cv_views_live;
@interface MockCVPixels : NSObject
{ @public unsigned format, fail_plane, null_plane, flipped; NSDictionary *tags; }
@end
@implementation MockCVPixels
- (id)init { if ((self = [super init])) cv_pixels_live++; return self; }
- (void)dealloc { cv_pixels_live--; [tags release]; [super dealloc]; }
@end
@interface MockCVView : NSObject
{ @public MockTexture *texture; unsigned flipped; }
@end
@implementation MockCVView
- (id)init { if ((self = [super init])) cv_views_live++; return self; }
- (void)dealloc { cv_views_live--; [texture release]; [super dealloc]; }
@end
typedef MockCVPixels *CVPixelBufferRef;
typedef MockCVView *CVMetalTextureRef;
typedef id<MTLDevice> CVMetalTextureCacheRef;
static int CVPixelBufferIsPlanar(CVPixelBufferRef p) { (void)p; return 1; }
static size_t CVPixelBufferGetPlaneCount(CVPixelBufferRef p) { (void)p; return 2; }
static unsigned CVPixelBufferGetPixelFormatType(CVPixelBufferRef p) { return p->format; }
static size_t CVPixelBufferGetWidthOfPlane(CVPixelBufferRef p, size_t i)
{ (void)p; return i ? 5 : 10; }
static size_t CVPixelBufferGetHeightOfPlane(CVPixelBufferRef p, size_t i)
{ (void)p; return i ? 4 : 8; }
static int CVMetalTextureCacheCreateTextureFromImage(const void *allocator,
      CVMetalTextureCacheRef device, CVPixelBufferRef pixels, const void *attrs,
      MTLPixelFormat format, size_t width, size_t height, size_t i, CVMetalTextureRef *out)
{
   MockCVView *view = [MockCVView new];
   (void)allocator; (void)attrs;
   view->texture = [MockTexture new];
   view->texture->_device = [device retain];
   view->texture->_width = width; view->texture->_height = height;
   view->texture->_pixelFormat = format;
   *out = view; view->flipped = pixels->flipped;
   if (pixels->null_plane == i + 1) { [view->texture release]; view->texture = nil; }
   return pixels->fail_plane == i + 1 ? -1 : kCVReturnSuccess;
}
static id<MTLTexture> CVMetalTextureGetTexture(CVMetalTextureRef view) { return view->texture; }
static int CVMetalTextureIsFlipped(CVMetalTextureRef view) { return !view->flipped; }
static int CFEqual(CFTypeRef a, CFTypeRef b) { return [(id)a isEqual:(id)b]; }
static CFTypeRef CVBufferGetAttachment(CVPixelBufferRef p, CFStringRef key, void *mode)
{ (void)mode; return (CFTypeRef)[p->tags objectForKey:(id)key]; }
/* SHIPPING_CV */
static bool cv_encode(void *user, const metal_frame_view_t *view, void *commands)
{
   MockBuffer *buffer = commands;
   unsigned *range = user;
   assert(buffer->sequence++ == 1 && view->color.range == *range);
   assert(view->planes[0] && view->planes[1] && view->color.max_cll == 1500);
   assert(view->color.primaries == 9 && view->color.transfer == 16);
   return true;
}
static void test_cv_color_mock(void)
{
   unsigned format, variant;
   for (format = 1; format <= 4; format++) for (variant = 0; variant < 15; variant++)
   {
      @autoreleasepool
      {
         MockDevice *device = [MockDevice new];
         MockCVPixels *pixels = [MockCVPixels new];
         MockEvent *ready = [MockEvent new], *done = [MockEvent new];
         MTLSharedEventListener *listener = [MTLSharedEventListener new];
         metal_frame_source_t source, original;
         video_hw_frame_t frame, untouched;
         unsigned primary = variant == 11 || variant == 12 ? 9 : 1;
         unsigned transfer = variant == 11 ? 16 : variant == 12 ? 18 : variant == 13 ? 13 : 1;
         unsigned matrix = primary;
         bool refused = variant >= 4 && variant <= 10;
         NSMutableDictionary *tags = [NSMutableDictionary dictionary];
         [tags setObject:(id)(primary == 9 ? kCVImageBufferColorPrimaries_ITU_R_2020
            : kCVImageBufferColorPrimaries_ITU_R_709_2) forKey:(id)kCVImageBufferColorPrimariesKey];
         [tags setObject:(id)(transfer == 16 ? kCVImageBufferTransferFunction_SMPTE_ST_2084_PQ
            : transfer == 18 ? kCVImageBufferTransferFunction_ITU_R_2100_HLG
            : transfer == 13 ? kCVImageBufferTransferFunction_sRGB
            : kCVImageBufferTransferFunction_ITU_R_709_2) forKey:(id)kCVImageBufferTransferFunctionKey];
         [tags setObject:(id)(matrix == 9 ? kCVImageBufferYCbCrMatrix_ITU_R_2020
            : kCVImageBufferYCbCrMatrix_ITU_R_709_2) forKey:(id)kCVImageBufferYCbCrMatrixKey];
         memset(&source, 0, sizeof(source));
         if (!variant || (variant >= 3 && variant <= 6))
            source.color.primaries = source.color.transfer = source.color.matrix = 1;
         if (variant == 2)
            source.color.primaries = source.color.transfer = source.color.matrix = 2;
         if (variant == 4) source.color.primaries = 9;
         if (variant == 5) source.color.transfer = 16;
         if (variant == 6) source.color.matrix = 9;
         if (variant >= 7 && variant <= 9)
            [tags setObject:@"Unknown" forKey:(id)(variant == 7 ? kCVImageBufferColorPrimariesKey
               : variant == 8 ? kCVImageBufferTransferFunctionKey : kCVImageBufferYCbCrMatrixKey)];
         if (variant == 10) [tags setObject:@7 forKey:(id)kCVImageBufferColorPrimariesKey];
         if (variant == 14)
         {
            [tags removeObjectForKey:(id)kCVImageBufferTransferFunctionKey];
            [tags removeObjectForKey:(id)kCVImageBufferYCbCrMatrixKey];
            transfer = matrix = 0;
         }
         pixels->format = format;
         if (variant) pixels->tags = [tags copy];
         source.device = device; source.ready = ready; source.done = done; source.listener = listener;
         source.ready_value = 3; source.done_value = 7;
         source.width = 7; source.height = 5; source.crop_x = 2; source.crop_y = 2;
         source.pixfmt = format <= 2 ? IMAGE_PIXFMT_NV12 : IMAGE_PIXFMT_P010;
         source.color.max_cll = 1500; source.color.max_fall = 400;
         source.color.mastering.present = 1;
         original = source;
         memset(&frame, 0xa5, sizeof(frame)); untouched = frame;
         assert(metal_frame_create_cv(&source, pixels, device, &frame) == !refused);
         assert(!memcmp(&source, &original, sizeof(source)));
         if (refused)
            assert(!memcmp(&frame, &untouched, sizeof(frame)) && !cv_views_live && !live_textures);
         else
         {
            assert(frame.color.primaries == primary && frame.color.transfer == transfer
               && frame.color.matrix == matrix);
            assert(frame.color.max_cll == 1500 && frame.color.max_fall == 400
               && !memcmp(&frame.color.mastering, &source.color.mastering, sizeof(source.color.mastering)));
            video_hw_frame_abandon(&frame);
            [ready setSignaledValue:3];
            assert(done.signaledValue == 7);
         }
         [pixels release]; [ready release]; [done release]; [listener release]; [device release];
         assert(!cv_pixels_live && !cv_views_live && !live_textures);
      }
   }
   puts("CoreVideo color mock: 60 attachment/consistency cases passed");
}

static void test_cv_mock(void)
{
   unsigned format, action;
   for (format = 1; format <= 4; format++) for (action = 0; action < 8; action++)
   {
      @autoreleasepool
      {
         MockDevice *device = [MockDevice new], *other = [MockDevice new];
         MockCVPixels *pixels = [MockCVPixels new];
         MockEvent *ready = [MockEvent new], *done = [MockEvent new];
         MTLSharedEventListener *listener = [MTLSharedEventListener new];
         MockBuffer *buffer = [MockBuffer new];
         metal_frame_source_t source, original;
         video_hw_frame_t frame, untouched;
         video_frame_import_t imported;
         unsigned range = format == 2 || format == 4 ? 2 : 1;
         memset(&source, 0, sizeof(source));
         pixels->format = format;
         source.device = device; source.ready = ready; source.done = done; source.listener = listener;
         source.ready_value = 3; source.done_value = 7;
         source.width = 7; source.height = 5; source.crop_x = 2; source.crop_y = 2;
         source.pixfmt = format <= 2 ? IMAGE_PIXFMT_NV12 : IMAGE_PIXFMT_P010;
         source.yuv = IMAGE_YUV_FLAG_FULL_RANGE;
         source.color.primaries = 9; source.color.transfer = 16; source.color.max_cll = 1500;
         buffer->_device = [device retain];
         memset(&frame, 0xa5, sizeof(frame)); untouched = frame;
         source.color.range = range == 2 ? 1 : 2;
         assert(!metal_frame_create_cv(&source, pixels, device, &frame)); source.color.range = 0;
         source.planes[0] = device;
         assert(!metal_frame_create_cv(&source, pixels, device, &frame)); source.planes[0] = NULL;
         source.backing[2] = device;
         assert(!metal_frame_create_cv(&source, pixels, device, &frame)); source.backing[2] = NULL;
         source.crop_x = 1;
         assert(!metal_frame_create_cv(&source, pixels, device, &frame)); source.crop_x = 2;
         assert(!cv_views_live && !live_textures && !memcmp(&frame, &untouched, sizeof(frame)));
         pixels->fail_plane = action == 2 ? 1 : action == 3 ? 2 : 0;
         pixels->null_plane = action == 4 ? 2 : 0;
         pixels->flipped = action == 6;
         original = source;
         if (action >= 2 && action <= 6)
         {
            assert(!metal_frame_create_cv(&source, pixels, action == 5 ? other : device, &frame));
            assert(!memcmp(&frame, &untouched, sizeof(frame)) && !cv_views_live && !live_textures);
            [pixels release];
         }
         else
         {
            assert(metal_frame_create_cv(&source, pixels, device, &frame));
            assert(!memcmp(&source, &original, sizeof(source)));
            assert(frame.color.range == range && frame.color.max_cll == 1500);
            assert(!!(frame.yuv & IMAGE_YUV_FLAG_FULL_RANGE) == (range == 2));
            [pixels release];
            assert(cv_pixels_live == 1 && cv_views_live == 2 && live_textures == 2);
            if (!action) video_hw_frame_abandon(&frame);
            else
            {
               memset(&imported, 0, sizeof(imported));
               imported.image = frame.image; imported.owner = frame.owner; imported.abandon = frame.abandon;
               imported.wait = frame.wait; imported.wait_value = frame.wait_value;
               imported.done = frame.done; imported.done_value = frame.done_value;
               imported.pixfmt = frame.pixfmt; imported.dims = VIDEO_SCALE_PACK(7, 5);
               imported.crop_x = 2; imported.crop_y = 2;
               assert(metal_frame_encode(&imported, device, buffer, cv_encode, &range));
            }
            if (action == 7) [buffer complete:MTLCommandBufferStatusError];
            assert(done.signaledValue == 0 && cv_pixels_live == 1);
            [ready setSignaledValue:3];
            if (action == 1) [buffer complete:MTLCommandBufferStatusCompleted];
            assert(done.signaledValue == 7 && !cv_pixels_live && !cv_views_live && !live_textures);
         }
         [buffer release]; [listener release]; [ready release]; [done release];
         [device release]; [other release];
         assert(!cv_pixels_live && !cv_views_live && !live_textures);
      }
   }
   puts("CoreVideo Metal mock: 32 ownership/failure cases passed");
}
