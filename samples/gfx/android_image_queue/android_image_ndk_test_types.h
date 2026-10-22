#include <stdint.h>
typedef int media_status_t;
typedef struct AImageReader
{
   int unused;
} AImageReader;
typedef struct AImage
{
   int unused;
} AImage;
typedef struct ANativeWindow
{
   int unused;
} ANativeWindow;
typedef struct AHardwareBuffer
{
   int unused;
} AHardwareBuffer;
typedef struct AImageCropRect
{
   int32_t left, top, right, bottom;
} AImageCropRect;
#define AMEDIA_OK 0
#define AMEDIA_IMGREADER_NO_BUFFER_AVAILABLE -1
#define AMEDIA_IMGREADER_MAX_IMAGES_ACQUIRED -2
#define AIMAGE_FORMAT_PRIVATE 34
#define AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE 256

typedef struct AHardwareBuffer_Desc
{
   uint32_t width, height, layers, format;
   uint64_t usage;
   uint32_t stride, rfu0;
   uint64_t rfu1;
} AHardwareBuffer_Desc;
#define AHARDWAREBUFFER_USAGE_PROTECTED_CONTENT 16384
