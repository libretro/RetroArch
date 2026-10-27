/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Included after the private renderer types. */
#include <jni/rjni_env.h>
#ifdef HAVE_RMP4
#include "../common/mediacodec_hevc_registry.h"
#endif
#ifdef HAVE_RVP9
#include "../common/mediacodec_vp9_registry.h"
#endif
#ifdef HAVE_RAV1
#include "../common/mediacodec_av1_registry.h"
#endif
#if defined(MEDIACODEC_HEVC_REGISTRY) || defined(MEDIACODEC_VP9_REGISTRY)      \
      || defined(MEDIACODEC_AV1_REGISTRY)
#define VULKAN_ANDROID_DECODERS 1
#ifdef HAVE_RMP4
static mediacodec_hevc_registry_t vulkan_android_hevc;
#endif
#ifdef HAVE_RVP9
static mediacodec_vp9_registry_t vulkan_android_vp9;
#endif
#ifdef HAVE_RAV1
static mediacodec_av1_registry_t vulkan_android_av1;
#endif

static JNIEnv *vulkan_android_decoder_environment(void *user)
{
   (void)user;
   return rjni_env();
}

static void vulkan_android_decoder_withdraw(vk_t *vk)
{
#ifdef HAVE_RMP4
   mediacodec_hevc_registry_withdraw(&vulkan_android_hevc, vk);
#endif
#ifdef HAVE_RVP9
   mediacodec_vp9_registry_withdraw(&vulkan_android_vp9, vk);
#endif
#ifdef HAVE_RAV1
   mediacodec_av1_registry_withdraw(&vulkan_android_av1, vk);
#endif
}

static void vulkan_android_decoder_publish(vk_t *vk, bool native_video)
{
   unsigned codecs;
   if (!vk || !vk->context || !vk->context->device
         || !(vk->context->flags & VK_CTX_FLAG_ANDROID_IMAGES))
   {
      if (vk)
         vulkan_android_decoder_withdraw(vk);
      return;
   }
   codecs = native_video ? vk->context->video_codec_operations : 0;
#ifdef HAVE_RMP4
   if (codecs & VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR)
      mediacodec_hevc_registry_withdraw(&vulkan_android_hevc, vk);
   else if (vulkan_android_hevc.offer.open
         || mediacodec_hevc_registry_init(&vulkan_android_hevc,
               vulkan_android_decoder_environment, NULL, 64u * 1024u * 1024u,
               8))
      mediacodec_hevc_registry_publish(&vulkan_android_hevc, vk);
#endif
#ifdef HAVE_RVP9
   if (codecs & VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR)
      mediacodec_vp9_registry_withdraw(&vulkan_android_vp9, vk);
   else if (vulkan_android_vp9.offer.open_color
         || mediacodec_vp9_registry_init(&vulkan_android_vp9,
               vulkan_android_decoder_environment, NULL, 64u * 1024u * 1024u,
               8))
      mediacodec_vp9_registry_publish(&vulkan_android_vp9, vk);
#endif
#ifdef HAVE_RAV1
   if (codecs & VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR)
      mediacodec_av1_registry_withdraw(&vulkan_android_av1, vk);
   else if (vulkan_android_av1.offer.open_config
         || mediacodec_av1_registry_init(&vulkan_android_av1,
               vulkan_android_decoder_environment, NULL, 64u * 1024u * 1024u,
               8))
      mediacodec_av1_registry_publish(&vulkan_android_av1, vk);
#endif
}
#endif
