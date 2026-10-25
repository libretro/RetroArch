#include <stdio.h>
#include <stdlib.h>
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>

static AVBufferRef *references[64];
static AVCodecHWConfig hardware;
static unsigned created, released, live, failures;
static int refuse;

static const AVCodecHWConfig *fixture_config(const AVCodec *codec, int index)
{
   (void)codec;
   return index ? NULL : &hardware;
}

static int fixture_create(AVBufferRef **out, enum AVHWDeviceType type,
      const char *device, AVDictionary *options, int flags)
{
   AVBufferRef *reference;
   (void)type;
   (void)device;
   (void)options;
   (void)flags;
   *out = NULL;
   if (refuse || created == sizeof(references) / sizeof(references[0]))
      return -1;
   reference = (AVBufferRef*)malloc(sizeof(*reference));
   if (!reference)
      return -1;
   reference->buffer = NULL;
   reference->data   = NULL;
   reference->size   = 0;
   references[created++] = reference;
   live++;
   *out = reference;
   return 0;
}

static void fixture_unref(AVBufferRef **reference)
{
   unsigned i;
   if (!*reference)
      return;
   for (i = 0; i < created; i++)
      if (references[i] == *reference)
      {
         free(*reference);
         references[i] = NULL;
         *reference = NULL;
         released++;
         live--;
         return;
      }
   failures++;
   *reference = NULL;
}

#define avcodec_get_hw_config fixture_config
#define av_hwdevice_ctx_create fixture_create
#define av_buffer_unref fixture_unref
#define av_hwdevice_get_type_name(type) "fixture"
#define av_get_pix_fmt_name(format) "fixture"
#undef av_err2str
#define av_err2str(error) "fixture"
#include "../../../cores/libretro-ffmpeg/ffmpeg_core.c"

static void fixture_log(enum retro_log_level level, const char *format, ...)
{
   (void)level;
   (void)format;
}

#define CHECK(test) do { if (!(test)) { \
   printf("FAIL line %u: %s\n", (unsigned)__LINE__, #test); failures++; \
} } while (0)

int main(void)
{
   static AVCodec codec;
   static AVCodecContext context;
   const enum AVPixelFormat accepted[] = { AV_PIX_FMT_CUDA, AV_PIX_FMT_NONE };
   const enum AVPixelFormat rejected[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };
   AVBufferRef *first;
   unsigned i;
   codec.name = "fixture";
   context.codec = &codec;
   hardware.pix_fmt = AV_PIX_FMT_CUDA;
   hardware.methods = AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX;
   hardware.device_type = AV_HWDEVICE_TYPE_CUDA;
   log_cb = fixture_log;

   CHECK(init_hw_decoder(&context, AV_HWDEVICE_TYPE_CUDA, NULL) == AV_PIX_FMT_CUDA);
   first = context.hw_device_ctx;
   CHECK(first && live == 1 && created == 1 && released == 0);
   refuse = 1;
   CHECK(init_hw_decoder(&context, AV_HWDEVICE_TYPE_CUDA, accepted) == AV_PIX_FMT_NONE);
   CHECK(context.hw_device_ctx == first && live == 1 && released == 0);
   refuse = 0;
   CHECK(init_hw_decoder(&context, AV_HWDEVICE_TYPE_NONE, accepted) == AV_PIX_FMT_NONE);
   CHECK(init_hw_decoder(&context, AV_HWDEVICE_TYPE_CUDA, rejected) == AV_PIX_FMT_NONE);
   CHECK(context.hw_device_ctx == first && created == 1 && released == 0);

   for (i = 0; i < 32; i++)
      CHECK(init_hw_decoder(&context, AV_HWDEVICE_TYPE_CUDA, accepted) == AV_PIX_FMT_CUDA);
   CHECK(live == 1 && created == 33 && released == 32);
   fixture_unref(&context.hw_device_ctx);
   CHECK(!live && released == created);
   for (i = 0; i < created; i++)
      fixture_unref(&references[i]);
   printf("hardware contexts: %u admitted, %u released, %u failures\n",
         created, released, failures);
   return failures ? 1 : 0;
}
