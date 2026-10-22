/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif
#include "android_image_ndk.h"
#if defined(__ANDROID__) || defined(ANDROID_IMAGE_NDK_TEST)
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dynamic/dylib.h>
#ifdef ANDROID_IMAGE_NDK_TEST
#include "android_image_ndk_test_types.h"
#else
#include <compat/android_native_window.h>
#if !defined(__cplusplus) \
      && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L) \
      && !defined(inline)
#define inline __inline__
#define ANDROID_IMAGE_UNDEF_INLINE
#endif
#include <media/NdkImageReader.h>
#include <android/hardware_buffer.h>
#ifdef ANDROID_IMAGE_UNDEF_INLINE
#undef inline
#undef ANDROID_IMAGE_UNDEF_INLINE
#endif
#endif
typedef media_status_t (*image_new_fn)(
      int32_t, int32_t, int32_t, uint64_t, int32_t, AImageReader **);
typedef media_status_t (*image_window_fn)(AImageReader *, ANativeWindow **);
typedef media_status_t (*image_next_fn)(AImageReader *, AImage **, int *);
typedef void (*image_reader_delete_fn)(AImageReader *);
typedef void (*image_delete_fn)(AImage *);
typedef media_status_t (*image_buffer_fn)(const AImage *, AHardwareBuffer **);
typedef media_status_t (*image_int_fn)(const AImage *, int32_t *);
typedef media_status_t (*image_crop_fn)(const AImage *, AImageCropRect *);
typedef media_status_t (*image_time_fn)(const AImage *, int64_t *);
typedef void (*buffer_describe_fn)(
      const AHardwareBuffer *, AHardwareBuffer_Desc *);
#if defined(__ANDROID__) && defined(__clang__) && __ANDROID_API__ >= 26
/* Type-only checks do not introduce native symbol references. */
#define IMAGE_ABI(type, symbol) \
   __extension__ typedef char symbol##_abi \
         [__builtin_types_compatible_p(type, __typeof__(&symbol)) ? 1 : -1]
IMAGE_ABI(image_new_fn, AImageReader_newWithUsage);
IMAGE_ABI(image_window_fn, AImageReader_getWindow);
IMAGE_ABI(image_next_fn, AImageReader_acquireNextImageAsync);
IMAGE_ABI(image_reader_delete_fn, AImageReader_delete);
IMAGE_ABI(image_delete_fn, AImage_delete);
IMAGE_ABI(image_buffer_fn, AImage_getHardwareBuffer);
IMAGE_ABI(image_int_fn, AImage_getWidth);
IMAGE_ABI(image_int_fn, AImage_getHeight);
IMAGE_ABI(image_crop_fn, AImage_getCropRect);
IMAGE_ABI(image_time_fn, AImage_getTimestamp);
IMAGE_ABI(buffer_describe_fn, AHardwareBuffer_describe);
#undef IMAGE_ABI
#endif
typedef struct android_ndk_reader android_ndk_reader_t;
typedef struct android_ndk_image
{
   android_ndk_reader_t *owner;
   AImage *image;
} android_ndk_image_t;
struct android_ndk_reader
{
   dylib_t library, android_library;
   AImageReader *reader;
   image_next_fn next;
   image_reader_delete_fn delete_reader;
   image_delete_fn delete_image;
   image_buffer_fn buffer;
   image_int_fn width, height;
   image_crop_fn crop;
   image_time_fn timestamp;
   buffer_describe_fn describe_buffer;
   unsigned count;
   android_ndk_image_t image[32];
};
static int android_ndk_acquire(void *context, void **image, int *fd)
{
   android_ndk_reader_t *r = (android_ndk_reader_t *)context;
   unsigned i;
   media_status_t status;
   *image = NULL;
   *fd = -1;
   for (i = 0; i < r->count; i++)
      if (!r->image[i].image)
         break;
   if (i == r->count)
      return 0;
   status = r->next(r->reader, &r->image[i].image, fd);
   if (status != AMEDIA_OK || !r->image[i].image)
   {
      if (r->image[i].image)
      {
         *image = &r->image[i];
         return -1;
      }
      if (*fd >= 0)
         close(*fd);
      *fd = -1;
      return status == AMEDIA_IMGREADER_NO_BUFFER_AVAILABLE
                  || status == AMEDIA_IMGREADER_MAX_IMAGES_ACQUIRED
            ? 0
            : -1;
   }
   *image = &r->image[i];
   return 1;
}
static bool android_ndk_describe(void *context, android_image_view_t *view)
{
   android_ndk_image_t *image = (android_ndk_image_t *)context;
   android_ndk_reader_t *r = image->owner;
   int32_t width, height;
   AImageCropRect crop;
   AHardwareBuffer *buffer = NULL;
   AHardwareBuffer_Desc desc;
   if (!image->image || r->buffer(image->image, &buffer) != AMEDIA_OK || !buffer
         || r->width(image->image, &width) != AMEDIA_OK || width <= 0
         || r->height(image->image, &height) != AMEDIA_OK || height <= 0
         || r->crop(image->image, &crop) != AMEDIA_OK || crop.left < 0
         || crop.top < 0 || crop.right < 0 || crop.bottom < 0
         || r->timestamp(image->image, &view->timestamp_ns) != AMEDIA_OK)
      return false;
   memset(&desc, 0, sizeof(desc));
   r->describe_buffer(buffer, &desc);
   if (desc.width < (unsigned)width || desc.height < (unsigned)height
         || desc.width > 16384 || desc.height > 16384 || desc.layers != 1
         || !(desc.usage & AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE)
         || (desc.usage & AHARDWAREBUFFER_USAGE_PROTECTED_CONTENT))
      return false;
   view->buffer_width = desc.width;
   view->buffer_height = desc.height;
   view->buffer = buffer;
   view->width = (unsigned)width;
   view->height = (unsigned)height;
   view->left = (unsigned)crop.left;
   view->top = (unsigned)crop.top;
   view->right = (unsigned)crop.right;
   view->bottom = (unsigned)crop.bottom;
   return true;
}
static void android_ndk_delete_image(void *context)
{
   android_ndk_image_t *image = (android_ndk_image_t *)context;
   image->owner->delete_image(image->image);
   image->image = NULL;
}
static void android_ndk_delete_reader(void *context)
{
   android_ndk_reader_t *r = (android_ndk_reader_t *)context;
   r->delete_reader(r->reader);
   dylib_close(r->library);
   dylib_close(r->android_library);
   free(r);
}
android_image_queue_t *android_image_ndk_new(
      unsigned width, unsigned height, unsigned slots, void **window)
{
   android_ndk_reader_t *r;
   android_image_queue_t *queue;
   android_image_ops_t ops;
   image_new_fn make;
   image_window_fn get_window;
   ANativeWindow *native_window = NULL;
   unsigned i;
   if (window)
      *window = NULL;
   if (!window || !width || !height || width > 16384 || height > 16384 || !slots
         || slots > 32)
      return NULL;
   r = (android_ndk_reader_t *)calloc(1, sizeof(*r));
   if (!r)
      return NULL;
   r->library = dylib_load("libmediandk.so");
   if (!r->library)
      goto failed;
#define LOAD(target, type, name) \
   do \
   { \
      target = (type)dylib_proc(r->library, name); \
      if (!target) \
         goto failed; \
   } while (0)
   LOAD(make, image_new_fn, "AImageReader_newWithUsage");
   LOAD(get_window, image_window_fn, "AImageReader_getWindow");
   LOAD(r->next, image_next_fn, "AImageReader_acquireNextImageAsync");
   LOAD(r->delete_reader, image_reader_delete_fn, "AImageReader_delete");
   LOAD(r->delete_image, image_delete_fn, "AImage_delete");
   LOAD(r->buffer, image_buffer_fn, "AImage_getHardwareBuffer");
   LOAD(r->width, image_int_fn, "AImage_getWidth");
   LOAD(r->height, image_int_fn, "AImage_getHeight");
   LOAD(r->crop, image_crop_fn, "AImage_getCropRect");
   LOAD(r->timestamp, image_time_fn, "AImage_getTimestamp");
#undef LOAD
   r->android_library = dylib_load("libandroid.so");
   if (!r->android_library)
      goto failed;
   r->describe_buffer = (buffer_describe_fn)dylib_proc(
         r->android_library, "AHardwareBuffer_describe");
   if (!r->describe_buffer)
      goto failed;
   if (make((int32_t)width, (int32_t)height, AIMAGE_FORMAT_PRIVATE,
             AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE, (int32_t)slots,
             &r->reader)
               != AMEDIA_OK
         || !r->reader || get_window(r->reader, &native_window) != AMEDIA_OK
         || !native_window)
      goto failed;
   r->count = slots;
   for (i = 0; i < slots; i++)
      r->image[i].owner = r;
   ops.acquire = android_ndk_acquire;
   ops.describe = android_ndk_describe;
   ops.delete_image = android_ndk_delete_image;
   ops.delete_reader = android_ndk_delete_reader;
   queue = android_image_queue_new(r, slots, &ops);
   if (!queue)
      goto failed;
   *window = native_window;
   return queue;
failed:
   if (r->reader && r->delete_reader)
      r->delete_reader(r->reader);
   if (r->library)
      dylib_close(r->library);
   if (r->android_library)
      dylib_close(r->android_library);
   free(r);
   return NULL;
}
#else
android_image_queue_t *android_image_ndk_new(
      unsigned width, unsigned height, unsigned slots, void **window)
{
   (void)width;
   (void)height;
   (void)slots;
   if (window)
      *window = NULL;
   return NULL;
}
#endif
