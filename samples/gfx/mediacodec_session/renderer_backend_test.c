/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <vulkan/vulkan_core.h>
#include "../../../gfx/common/mediacodec_hevc_backend.h"
#include "../../../gfx/common/mediacodec_vp9_backend.h"
#include "../../../gfx/common/mediacodec_av1_backend.h"
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
static const rav1_hw_backend_t *av1_offer;
static unsigned hevc_init, vp9_init, av1_init;
static bool fail_hevc, fail_vp9, fail_av1;
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

void rav1_set_hw_backend(const rav1_hw_backend_t *offer)
{
   av1_offer = offer;
}

int rav1_unset_hw_backend(const rav1_hw_backend_t *offer)
{
   if (!offer || av1_offer != offer)
      return 0;
   av1_offer = NULL;
   return 1;
}

static bool open_av1(void *user, const rav1_sequence_t *sequence,
      const rav1_obu_t *obu, const video_color_metadata_t *color,
      unsigned slots, rav1_hw_ops_t *ops)
{
   (void)user;
   (void)sequence;
   (void)obu;
   (void)color;
   (void)slots;
   (void)ops;
   return false;
}

static void close_av1(rav1_hw_ops_t *ops)
{
   (void)ops;
}

static int drain_av1(void *decoder, bool discard)
{
   (void)decoder;
   (void)discard;
   return 0;
}

bool mediacodec_av1_backend_init(mediacodec_av1_backend_t *b,
      JNIEnv *(*environment)(void *), void *user, unsigned input,
      unsigned sessions)
{
   assert(environment(user) == (JNIEnv *)&token && !user
         && input == 64u * 1024u * 1024u && sessions == 8);
   av1_init++;
   if (fail_av1)
      return false;
   b->offer.open_config = open_av1;
   b->offer.close = close_av1;
   b->offer.drain = drain_av1;
   b->offer.packet_decoder = true;
   return true;
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
   b->offer.open = open_hevc;
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
   b->offer.open_color = open_vp9;
   b->offer.close = close_vp9;
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
   rav1_hw_backend_t foreign_av1;
   memset(&context, 0, sizeof(context));
   memset(&foreign_hevc, 0, sizeof(foreign_hevc));
   memset(&foreign_vp9, 0, sizeof(foreign_vp9));
   memset(&foreign_av1, 0, sizeof(foreign_av1));
   a.context = b.context = &context;
   vulkan_android_decoder_publish(NULL, false);
   vulkan_android_decoder_publish(&a, false);
   assert(!hevc_init && !vp9_init && !av1_init);
   context.device = &token;
   vulkan_android_decoder_publish(&a, false);
   assert(!hevc_init && !vp9_init && !av1_init);
   context.flags = VK_CTX_FLAG_ANDROID_IMAGES;
   fail_hevc = fail_vp9 = fail_av1 = true;
   vulkan_android_decoder_publish(&a, false);
   assert(!hevc_offer && !vp9_offer && !av1_offer);
   fail_hevc = fail_vp9 = fail_av1 = false;
   vulkan_android_decoder_publish(&a, false);
#ifdef HAVE_RMP4
   assert(hevc_offer == &vulkan_android_hevc.offer && hevc_init == 2);
#endif
#ifdef HAVE_RVP9
   assert(vp9_offer == &vulkan_android_vp9.offer && vp9_init == 2);
#endif
#ifdef HAVE_RAV1
   assert(av1_offer == &vulkan_android_av1.offer && av1_init == 2);
   assert(av1_offer->drain == drain_av1 && av1_offer->packet_decoder);
#endif
   vulkan_android_decoder_withdraw(&b);
#ifdef HAVE_RMP4
   assert(hevc_offer == &vulkan_android_hevc.offer);
#endif
#ifdef HAVE_RVP9
   assert(vp9_offer == &vulkan_android_vp9.offer);
#endif
#ifdef HAVE_RAV1
   assert(av1_offer == &vulkan_android_av1.offer);
#endif
   context.video_codec_operations
         = VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR;
   hevc_offer = &foreign_hevc;
   vulkan_android_decoder_publish(&a, true);
   assert(hevc_offer == &foreign_hevc);
#ifdef HAVE_RVP9
   assert(vp9_offer == &vulkan_android_vp9.offer);
#endif
#ifdef HAVE_RAV1
   assert(av1_offer == &vulkan_android_av1.offer);
#endif
   context.video_codec_operations
         |= VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR;
   vp9_offer = &foreign_vp9;
   vulkan_android_decoder_publish(&a, true);
   assert(hevc_offer == &foreign_hevc && vp9_offer == &foreign_vp9);
   context.video_codec_operations
         |= VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR;
   av1_offer = &foreign_av1;
   vulkan_android_decoder_publish(&a, true);
   assert(av1_offer == &foreign_av1);
   vulkan_android_decoder_publish(&b, false);
#ifdef HAVE_RMP4
   assert(hevc_offer == &vulkan_android_hevc.offer && hevc_init == 2);
#endif
#ifdef HAVE_RVP9
   assert(vp9_offer == &vulkan_android_vp9.offer && vp9_init == 2);
#endif
#ifdef HAVE_RAV1
   assert(av1_offer == &vulkan_android_av1.offer && av1_init == 2);
   assert(av1_offer->drain == drain_av1 && av1_offer->packet_decoder);
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
#ifdef HAVE_RAV1
   assert(!av1_offer);
#endif
#else
#if defined(HAVE_THREADS)                                                      \
      && (defined(HAVE_RMP4) || defined(HAVE_RVP9) || defined(HAVE_RAV1))
#if defined(RETRO_ATOMIC_LOCK_FREE) && defined(RETRO_ATOMIC_HAS_PTR)
#error threaded renderer registry missing
#endif
#endif
#endif
   puts("PASS: Android renderer admission, codec preference, failed init retry "
        "and lifecycle ownership");
   return 0;
}
