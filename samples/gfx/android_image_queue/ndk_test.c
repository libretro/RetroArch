/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <dynamic/dylib.h>
#include "android_image_ndk_test_types.h"
#include "../../../gfx/common/android_image_ndk.h"
static AImageReader reader;
static ANativeWindow window;
static AImage image;
static AHardwareBuffer buffer;
static int loads, unloads, readers, images, missing, lookup, fail_create,
      fail_window, metadata_fail, available, bad_desc, empty_success;
static media_status_t make(int32_t w, int32_t h, int32_t format, uint64_t usage,
      int32_t slots, AImageReader **out)
{
   assert(w == 320 && h == 192 && format == 34 && usage == 256 && slots == 2);
   *out = &reader;
   readers++;
   return fail_create ? -99 : 0;
}
static media_status_t get_window(AImageReader *r, ANativeWindow **out)
{
   assert(r == &reader);
   *out = &window;
   return fail_window ? -99 : 0;
}
static media_status_t next(AImageReader *r, AImage **out, int *fd)
{
   assert(r == &reader);
   *fd = -1;
   *out = NULL;
   if (!available)
      return -1;
   available = 0;
   if (empty_success)
      return 0;
   *out = &image;
   images++;
   return 0;
}
static void del_reader(AImageReader *r)
{
   assert(r == &reader);
   readers--;
}
static void del_image(AImage *im)
{
   assert(im == &image);
   images--;
}
static media_status_t get_buffer(const AImage *im, AHardwareBuffer **out)
{
   assert(im == &image);
   *out = &buffer;
   return metadata_fail ? -99 : 0;
}
static media_status_t width(const AImage *im, int32_t *out)
{
   assert(im == &image);
   *out = 320;
   return 0;
}
static media_status_t height(const AImage *im, int32_t *out)
{
   assert(im == &image);
   *out = 192;
   return 0;
}
static media_status_t crop(const AImage *im, AImageCropRect *out)
{
   assert(im == &image);
   out->left = out->top = 0;
   out->right = 320;
   out->bottom = 192;
   return 0;
}
static media_status_t timestamp(const AImage *im, int64_t *out)
{
   assert(im == &image);
   *out = 1000;
   return 0;
}
static void describe_buffer(const AHardwareBuffer *b, AHardwareBuffer_Desc *d)
{
   assert(b == &buffer);
   memset(d, 0, sizeof(*d));
   d->width = 320;
   d->height = 192;
   d->layers = 1;
   d->usage = 256;
   if (bad_desc == 1)
      d->width = 319;
   if (bad_desc == 2)
      d->layers = 2;
   if (bad_desc == 3)
      d->usage = 0;
   if (bad_desc == 4)
      d->usage |= AHARDWAREBUFFER_USAGE_PROTECTED_CONTENT;
}
dylib_t dylib_load(const char *path)
{
   assert(!strcmp(path, "libmediandk.so") || !strcmp(path, "libandroid.so"));
   loads++;
   if (!strcmp(path, "libmediandk.so"))
      lookup = 0;
   return &reader;
}
void dylib_close(dylib_t lib)
{
   assert(lib == &reader);
   unloads++;
}
function_t dylib_proc(dylib_t lib, const char *name)
{
   assert(lib == &reader);
   if (++lookup == missing)
      return NULL;
#define FN(symbol, fn) \
   if (!strcmp(name, symbol)) \
   return (function_t)fn
   FN("AImageReader_newWithUsage", make);
   FN("AImageReader_getWindow", get_window);
   FN("AImageReader_acquireNextImageAsync", next);
   FN("AImageReader_delete", del_reader);
   FN("AImage_delete", del_image);
   FN("AImage_getHardwareBuffer", get_buffer);
   FN("AImage_getWidth", width);
   FN("AImage_getHeight", height);
   FN("AImage_getCropRect", crop);
   FN("AImage_getTimestamp", timestamp);
   FN("AHardwareBuffer_describe", describe_buffer);
#undef FN
   assert(0);
   return NULL;
}
int main(void)
{
   android_image_queue_t *q;
   android_image_view_t view;
   void *out;
   int i;
   for (i = 1; i <= 11; i++)
   {
      missing = i;
      assert(!android_image_ndk_new(320, 192, 2, &out) && !out);
      assert(!readers && loads == unloads);
   }
   missing = 0;
   fail_create = 1;
   assert(!android_image_ndk_new(320, 192, 2, &out));
   assert(!readers && loads == unloads);
   fail_create = 0;
   fail_window = 1;
   assert(!android_image_ndk_new(320, 192, 2, &out));
   assert(!readers && loads == unloads);
   fail_window = 0;
   q = android_image_ndk_new(320, 192, 2, &out);
   assert(q && out == &window);
   assert(android_image_queue_acquire(q, &view) == 0);
   available = 1;
   assert(android_image_queue_acquire(q, &view) == 1 && view.buffer == &buffer
         && view.timestamp_ns == 1000);
   android_image_queue_close(q);
   assert(!android_image_queue_free(q));
   assert(readers == 1 && images == 1 && loads != unloads);
   assert(android_image_queue_return(view.ticket, view.generation, -1));
   assert(android_image_queue_poll(q) == 1);
   assert(android_image_queue_free(q) && !readers && !images
         && loads == unloads);
   q = android_image_ndk_new(320, 192, 2, &out);
   assert(q);
   available = empty_success = 1;
   assert(android_image_queue_acquire(q, &view) == -1 && !view.ticket);
   assert(!android_image_queue_poll(q));
   android_image_queue_close(q);
   assert(android_image_queue_free(q));
   assert(!readers && !images && loads == unloads);
   empty_success = 0;
   q = android_image_ndk_new(320, 192, 2, &out);
   assert(q);
   available = metadata_fail = 1;
   assert(android_image_queue_acquire(q, &view) == -1);
   assert(android_image_queue_poll(q) == 1);
   android_image_queue_close(q);
   assert(android_image_queue_free(q));
   assert(!readers && !images && loads == unloads);
   metadata_fail = 0;
   for (i = 1; i <= 4; i++)
   {
      bad_desc = i;
      q = android_image_ndk_new(320, 192, 2, &out);
      assert(q);
      available = 1;
      assert(android_image_queue_acquire(q, &view) == -1);
      assert(android_image_queue_poll(q) == 1);
      android_image_queue_close(q);
      assert(android_image_queue_free(q));
      assert(!readers && !images && loads == unloads);
   }
   puts("PASS: runtime NDK reader missing symbols, partial startup, borrowed "
        "hardware buffer and delayed reader/library retirement");
   return 0;
}
