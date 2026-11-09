/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VULKAN_ENCODE_H264_SESSION_H
#define RARCH_VULKAN_ENCODE_H264_SESSION_H
#include "../include/vulkan/vulkan.h"
#include "vulkan_encode_h264_params.h"
#include <retro_common_api.h>
RETRO_BEGIN_DECLS

typedef struct vulkan_encode_h264_session_ops
{
   PFN_vkGetPhysicalDeviceQueueFamilyProperties2 families;
   PFN_vkGetPhysicalDeviceMemoryProperties memory;
   PFN_vkGetPhysicalDeviceVideoCapabilitiesKHR capabilities;
   PFN_vkGetPhysicalDeviceVideoFormatPropertiesKHR formats;
   PFN_vkCreateVideoSessionKHR create;
   PFN_vkDestroyVideoSessionKHR destroy;
   PFN_vkGetVideoSessionMemoryRequirementsKHR requirements;
   PFN_vkBindVideoSessionMemoryKHR bind;
   PFN_vkAllocateMemory allocate;
   PFN_vkFreeMemory free_memory;
   PFN_vkCreateVideoSessionParametersKHR parameters;
   PFN_vkDestroyVideoSessionParametersKHR destroy_parameters;
   PFN_vkGetEncodedVideoSessionParametersKHR headers;
} vulkan_encode_h264_session_ops_t;

typedef struct vulkan_encode_h264_session_config
{
   VkPhysicalDevice physical;
   VkDevice device;
   vulkan_encode_h264_config_t video;
   unsigned family;
} vulkan_encode_h264_session_config_t;

typedef struct vulkan_encode_h264_session
{
   const vulkan_encode_h264_session_ops_t *ops;
   VkDevice device;
   VkVideoSessionKHR session;
   VkVideoSessionParametersKHR parameters;
   VkDeviceMemory memory[32];
   VkVideoEncodeH264ProfileInfoKHR codec_profile;
   VkVideoProfileInfoKHR profile;
   VkVideoProfileListInfoKHR profiles;
   VkVideoCapabilitiesKHR capabilities;
   VkVideoEncodeCapabilitiesKHR encode;
   VkVideoEncodeH264CapabilitiesKHR codec;
   vulkan_encode_h264_params_t params;
   unsigned memory_count;
} vulkan_encode_h264_session_t;

/* Worker startup only. Borrows device/dispatch until free. The device must
 * enable video queue/encode/H264 and the selected queue family. Owns session,
 * parameters and their memory; no frames are submitted by this component. */
vulkan_encode_h264_session_t *vulkan_encode_h264_session_new(
      const vulkan_encode_h264_session_config_t *config,
      const vulkan_encode_h264_session_ops_t *ops);
/* Immutable actual driver parameter sets, including overrides. Caller owns
 * storage. Insufficient capacity writes no bytes; *size reports the need. */
bool vulkan_encode_h264_session_headers(vulkan_encode_h264_session_t *session,
      void *data, size_t capacity, size_t *size);
/* Worker only, after every GPU/CPU user of the session and headers retires.
 * Startup failure has never submitted work and cleans up immediately. */
void vulkan_encode_h264_session_free(vulkan_encode_h264_session_t *session);
RETRO_END_DECLS
#endif
