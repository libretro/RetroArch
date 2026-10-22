/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Included after the private renderer types. Offer storage survives reinit. */
static vaapi_vp9_backend_t vulkan_vaapi_vp9_backend;

static bool vulkan_vaapi_vp9_decoder_publish(vk_t *vk, bool native_video)
{
   if (!vk || !vk->context || !vk->context->device
         || !vk->context->prime_import_device
         || !vk->context->drm_identity.valid
         || (native_video
               && (vk->context->video_codec_operations
                     & VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR)))
   {
      if (vk)
         vaapi_vp9_backend_withdraw(&vulkan_vaapi_vp9_backend, vk);
      return false;
   }
   if (!vulkan_vaapi_vp9_backend.offer.open
         && !vaapi_vp9_backend_init(&vulkan_vaapi_vp9_backend, 8))
      return false;
   return vaapi_vp9_backend_publish(&vulkan_vaapi_vp9_backend, vk,
         vk->context->drm_identity.major_id,
         vk->context->drm_identity.minor_id);
}
