#include <sys/types.h>
typedef int media_status_t;
typedef struct AMediaCodec
{
   int unused;
} AMediaCodec;
typedef struct AMediaFormat
{
   int unused;
} AMediaFormat;
typedef struct ANativeWindow
{
   int unused;
} ANativeWindow;
typedef struct AMediaCrypto
{
   int unused;
} AMediaCrypto;
typedef struct AMediaCodecBufferInfo
{
   int32_t offset, size;
   int64_t presentationTimeUs;
   uint32_t flags;
} AMediaCodecBufferInfo;
#define AMEDIA_OK 0
#define AMEDIACODEC_INFO_TRY_AGAIN_LATER -1
#define AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED -2
#define AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED -3
#define AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG 2
#define AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM 4
