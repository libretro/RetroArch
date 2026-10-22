/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <vulkan/vulkan_core.h>
#include "../../../gfx/common/mediacodec_hevc_registry.h"
#include "../../../gfx/common/mediacodec_vp9_registry.h"
#define VK_CTX_FLAG_ANDROID_IMAGES 1024

typedef struct test_context
{
   void *device;
   unsigned flags, video_codec_operations;
} test_context_t;

typedef struct vk
{
   test_context_t *context;
} vk_t;

#ifdef HAVE_THREADS
static const rh265_hw_backend_t *hevc_offer;
static const rvp9_hw_backend_t *vp9_offer;
static unsigned hevc_init, vp9_init;
static bool fail_hevc, fail_vp9;
static int token;

JNIEnv *rjni_env(void)
{
   return (JNIEnv *)&token;
}

void rh265_set_hw_backend(const rh265_hw_backend_t *offer)
{
   hevc_offer = offer;
}

int rh265_unset_hw_backend(const rh265_hw_backend_t *offer)
{
   if (!offer || hevc_offer != offer)
      return 0;
   hevc_offer = NULL;
   return 1;
}

void rvp9_set_hw_backend(const rvp9_hw_backend_t *offer)
{
   vp9_offer = offer;
}

int rvp9_unset_hw_backend(const rvp9_hw_backend_t *offer)
{
   if (!offer || vp9_offer != offer)
      return 0;
   vp9_offer = NULL;
   return 1;
}

static bool open_hevc(void *user, const rh265_video *source,
      const rh265_video_params_t *params, unsigned slots, rh265_hw_ops_t *ops)
{
   (void)user;
   (void)source;
   (void)params;
   (void)slots;
   (void)ops;
   return false;
}

static bool open_vp9(void *user, const rvp9_hdr *header,
      const video_color_metadata_t *color, unsigned slots, rvp9_hw_ops_t *ops)
{
   (void)user;
   (void)header;
   (void)color;
   (void)slots;
   (void)ops;
   return false;
}

static void close_hevc(rh265_hw_ops_t *ops)
{
   (void)ops;
}

static void close_vp9(rvp9_hw_ops_t *ops)
{
   (void)ops;
}

bool mediacodec_hevc_backend_init(mediacodec_hevc_backend_t *b,
      JNIEnv *(*environment)(void *), void *user, unsigned input,
      unsigned sessions)
{
   assert(environment(user) == (JNIEnv *)&token && !user
          && input == 64u * 1024u * 1024u && sessions == 8);
   hevc_init++;
   if (fail_hevc)
      return false;
   b->offer.open  = open_hevc;
   b->offer.close = close_hevc;
   return true;
}

bool mediacodec_vp9_backend_init(mediacodec_vp9_backend_t *b,
      JNIEnv *(*environment)(void *), void *user, unsigned input,
      unsigned sessions)
{
   assert(environment(user) == (JNIEnv *)&token && !user
          && input == 64u * 1024u * 1024u && sessions == 8);
   vp9_init++;
   if (fail_vp9)
      return false;
   b->offer.open_color     = open_vp9;
   b->offer.close          = close_vp9;
   b->offer.packet_decoder = true;
   return true;
}
#endif
#include "../../../gfx/drivers/vulkan_android_backend.h"

int main(void)
{
#ifdef VULKAN_ANDROID_DECODERS
   test_context_t context;
   vk_t a, b;
   rh265_hw_backend_t foreign_hevc;
   rvp9_hw_backend_t foreign_vp9;
   memset(&context, 0, sizeof(context));
   memset(&foreign_hevc, 0, sizeof(foreign_hevc));
   memset(&foreign_vp9, 0, sizeof(foreign_vp9));
   a.context = b.context = &context;
   vulkan_android_decoder_publish(NULL, false);
   vulkan_android_decoder_publish(&a, false);
   assert(!hevc_init && !vp9_init);
   context.device = &token;
   vulkan_android_decoder_publish(&a, false);
   assert(!hevc_init && !vp9_init);
   context.flags = VK_CTX_FLAG_ANDROID_IMAGES;
   fail_hevc = fail_vp9 = true;
   vulkan_android_decoder_publish(&a, false);
   assert(!hevc_offer && !vp9_offer);
   fail_hevc = fail_vp9 = false;
   vulkan_android_decoder_publish(&a, false);
#ifdef HAVE_RMP4
   assert(hevc_offer == &vulkan_android_hevc.offer && hevc_init == 2);
#endif
#ifdef HAVE_RVP9
   assert(vp9_offer == &vulkan_android_vp9.offer && vp9_init == 2);
#endif
   vulkan_android_decoder_withdraw(&b);
#ifdef HAVE_RMP4
   assert(hevc_offer == &vulkan_android_hevc.offer);
#endif
#ifdef HAVE_RVP9
   assert(vp9_offer == &vulkan_android_vp9.offer);
#endif
   context.video_codec_operations =
         VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR;
   hevc_offer = &foreign_hevc;
   vulkan_android_decoder_publish(&a, true);
   assert(hevc_offer == &foreign_hevc);
#ifdef HAVE_RVP9
   assert(vp9_offer == &vulkan_android_vp9.offer);
#endif
   context.video_codec_operations |=
         VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR;
   vp9_offer = &foreign_vp9;
   vulkan_android_decoder_publish(&a, true);
   assert(hevc_offer == &foreign_hevc && vp9_offer == &foreign_vp9);
   vulkan_android_decoder_publish(&b, false);
#ifdef HAVE_RMP4
   assert(hevc_offer == &vulkan_android_hevc.offer && hevc_init == 2);
#endif
#ifdef HAVE_RVP9
   assert(vp9_offer == &vulkan_android_vp9.offer && vp9_init == 2);
#endif
   vulkan_android_decoder_withdraw(&a);
   context.flags = 0;
   vulkan_android_decoder_publish(&b, false);
#ifdef HAVE_RMP4
   assert(!hevc_offer);
#endif
#ifdef HAVE_RVP9
   assert(!vp9_offer);
#endif
#else
#ifdef HAVE_THREADS
#error threaded renderer registry missing
#endif
#endif
   puts("PASS: Android renderer admission, codec preference, failed init retry and lifecycle ownership");
   return 0;
}
