/* Native decoded-frame acceptance; window and viewport state are test-only. */
#include "movie_reference.h"
@interface NativeContext : NSObject
{
@public
   id<MTLDevice> _device;
   id<MTLCommandBuffer> _buffer;
   void *_frameSDRPipeline;
}
@property(readonly) id<MTLDevice> device;
@property(readonly) id<MTLCommandBuffer> blitCommandBuffer;
@property(readonly) void *frameSDRPipeline;
@property(readonly) BOOL movieHDRSupported;
@end
@implementation NativeContext
- (id<MTLDevice>)device { return _device; }
- (id<MTLCommandBuffer>)blitCommandBuffer { return _buffer; }
- (void *)frameSDRPipeline { return _frameSDRPipeline; }
- (BOOL)movieHDRSupported { return YES; }
- (void)dealloc { if (_frameSDRPipeline) CFRelease(_frameSDRPipeline); }
@end

/* SHIPPING_FRAME_VIEW */

static NativeContext *hevc_movie_context;
static FrameView *hevc_movie_view;
static struct frame_shader hevc_movie_history = { 2 };

static id<MTLTexture> hevc_movie_import(id<MTLDevice> device,
      id<MTLCommandBuffer> buffer, const video_frame_import_t *frame)
{
   if (!hevc_movie_context)
   {
      hevc_movie_context = [NativeContext new];
      hevc_movie_context->_device = device;
      hevc_movie_context->_frameSDRPipeline = metal_frame_sdr_pipeline_create((__bridge void*)device);
      assert(hevc_movie_context->_frameSDRPipeline);
      hevc_movie_view = [FrameView new];
      hevc_movie_view->_context = hevc_movie_context;
      hevc_movie_view->_shader = &hevc_movie_history;
      hevc_movie_view->init_history = YES;
   }
   assert(hevc_movie_context.device == device);
   hevc_movie_context->_buffer = buffer;
   unsigned previous = hevc_movie_view->history_updates;
   assert([hevc_movie_view importMovieFrame:frame] == VIDEO_TEXTURE_UPDATE_DONE);
   assert(hevc_movie_view->_movieFrame && !hevc_movie_view->_srcDirty);
   assert(hevc_movie_view->history_updates == previous + 1);
   id<MTLTexture> front = hevc_movie_view->_engine.frame.texture[0].view;
   bool hdr = frame->color.transfer == 16 || frame->color.transfer == 18;
   assert(hevc_movie_view->_movieHDR == hdr);
   assert(front && front.pixelFormat == (hdr ? MTLPixelFormatRGBA16Float : MTLPixelFormatRGBA8Unorm));
   return front;
}

void videotoolbox_hevc_test_movie_reset(void)
{
   assert(hevc_movie_context && hevc_movie_view);
   assert([hevc_movie_view importMovieFrame:NULL] == VIDEO_TEXTURE_UPDATE_DONE);
   assert(!hevc_movie_view->_movieFrame);
   [hevc_movie_view _ensureFront];
   assert(hevc_movie_view->_engine.frame.texture[0].view.pixelFormat == MTLPixelFormatBGRA8Unorm);
   hevc_movie_context->_buffer = nil;
   hevc_movie_view = nil;
   hevc_movie_context = nil;
   puts("VideoToolbox movie: decoded stream history and same-size CPU transition passed");
}

static unsigned hevc_movie_code(const unsigned char *data, unsigned index, unsigned depth)
{
   return depth == 10 ? ((const uint16_t*)data)[index] >> 6 : data[index];
}

/* Independent double-precision BT.709 equations, derived from Kr/Kb.
 * The YUV readbacks are separately checked against FFmpeg plane CRCs. */
static void hevc_movie_pixels(id<MTLTexture> y_texture, id<MTLTexture> uv_texture,
      id<MTLTexture> rgba_texture, unsigned depth, unsigned full, unsigned transfer)
{
   bool hdr = transfer == 16 || transfer == 18;
   unsigned char *y = malloc(8192), *uv = malloc(4096), *rgba = malloc(hdr ? 32768 : 16384);
   unsigned worst = 0;
   assert(y && uv && rgba && (transfer == 1 || transfer == 13 || hdr));
   [y_texture getBytes:y bytesPerRow:64 * (depth == 10 ? 2 : 1)
      fromRegion:MTLRegionMake2D(0, 0, 64, 64) mipmapLevel:0];
   [uv_texture getBytes:uv bytesPerRow:64 * (depth == 10 ? 2 : 1)
      fromRegion:MTLRegionMake2D(0, 0, 32, 32) mipmapLevel:0];
   [rgba_texture getBytes:rgba bytesPerRow:hdr ? 512 : 256
      fromRegion:MTLRegionMake2D(0, 0, 64, 64) mipmapLevel:0];
   for (unsigned row = 0; row < 64; row++) for (unsigned col = 0; col < 64; col++)
   {
      unsigned chroma = (row / 2 * 32 + col / 2) * 2;
      unsigned char reference[4];
      if (hdr)
      {
         double expected[4];
         const uint16_t *actual = (const uint16_t*)rgba + (row * 64 + col) * 4;
         hevc_movie_hdr_reference(hevc_movie_code(y, row * 64 + col, depth),
               hevc_movie_code(uv, chroma, depth), hevc_movie_code(uv, chroma + 1, depth),
               depth, full, transfer, expected);
         for (unsigned c = 0; c < 3; c++)
         {
            double value = hevc_movie_half(actual[c]);
            assert(((actual[c] >> 10) & 31) != 31);
            double tolerance = fmax(0.002, fabs(expected[c]) * 0.002);
            if (fabs(value - expected[c]) > tolerance)
               fprintf(stderr, "Decoded HDR transfer %u pixel %u,%u channel %u: %.9g expected %.9g\n",
                     transfer, col, row, c, value, expected[c]);
            assert(fabs(value - expected[c]) <= tolerance);
         }
         assert(actual[3] == 0x3c00);
         continue;
      }
      hevc_movie_reference(hevc_movie_code(y, row * 64 + col, depth),
            hevc_movie_code(uv, chroma, depth), hevc_movie_code(uv, chroma + 1, depth),
            depth, full, transfer, reference);
      for (unsigned channel = 0; channel < 3; channel++)
      {
         int delta = (int)rgba[(row * 64 + col) * 4 + channel] - reference[channel];
         unsigned error = (unsigned)(delta < 0 ? -delta : delta);
         if (error > worst) worst = error;
         if (error > 1) fprintf(stderr, "HEVC movie %u-bit range %u pixel %u,%u channel %u: error %u\n",
               depth, full, col, row, channel, error);
         assert(error <= 1);
      }
      assert(rgba[(row * 64 + col) * 4 + 3] == 255);
   }
   free(y); free(uv); free(rgba);
   if (hdr)
      puts("VideoToolbox movie HDR: coded PQ/HLG through shipping FrameView preserves linear scRGB");
   else
      printf("VideoToolbox movie: decoded image through shipping FrameView, max error %u/255\n", worst);
}
