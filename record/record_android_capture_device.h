/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RECORD_ANDROID_CAPTURE_DEVICE_H
#define RECORD_ANDROID_CAPTURE_DEVICE_H
#include <vulkan/vulkan_core.h>
#include <boolean.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS
/* Intrusive cold-owner registration. All operations use the serialized render
 * owner, including construction, collection and context teardown. Workers never
 * access this list. Storage and context survive actual device destruction and
 * retained GLES reads; registration requires no allocation or frame work.
 * Callbacks cannot mutate the list or free their registration storage. */
typedef struct record_android_capture_device
{
   struct record_android_capture_device *next;
   void *context;
   bool (*prepare)(void *context, bool queue_quiesced);
   void (*destroyed)(void *context);
   VkDevice device;
   bool prepared;
} record_android_capture_device_t;
bool record_android_capture_device_attach(
      record_android_capture_device_t *owner);
/* Only after all owned Vulkan resources have retired, or actual device death.
 * The node must stay registered through partial construction and cancellation.
 */
void record_android_capture_device_detach(
      record_android_capture_device_t *owner);
bool record_android_capture_device_pending(VkDevice device);
/* Renderer quiescent. queue_quiesced is proof from the existing device drain,
 * never inferred from cancellation. False retains device and every owner. */
bool record_android_capture_device_prepare(
      VkDevice device, bool queue_quiesced);
/* Only after vkDestroyDevice. Refuses notification unless every matching owner
 * was prepared. Each callback occurs once; retained CPU/GLES owners stay listed
 * with a null device until their eventual collection. */
bool record_android_capture_device_destroyed(VkDevice device);
RETRO_END_DECLS
#endif
