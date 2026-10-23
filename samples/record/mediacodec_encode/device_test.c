/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../../record/record_android_capture_device.c"
#define VIDEO_FLAG_CACHE_CONTEXT 1
#define RARCH_ERR(message) ((void)0)
struct test_context
{
   struct
   {
      VkDevice device;
   } context;
};
struct owner_state
{
   record_android_capture_device_t node;
   unsigned prepares, destroyed;
   bool ready, proof;
};
static unsigned device_destroys, cached;
static VkDevice expected_device;
static void VKAPI_CALL destroy_device(
      VkDevice device, const VkAllocationCallbacks *callbacks)
{
   assert(device == expected_device && !callbacks);
   device_destroys++;
}
#define vkDestroyDevice destroy_device
#include "device_teardown_fixture.h"
static bool prepare_owner(void *context, bool proof)
{
   struct owner_state *s = (struct owner_state *)context;
   assert(!device_destroys && !s->destroyed);
   s->prepares++;
   s->proof = proof;
   return s->ready && proof;
}
static void owner_destroyed(void *context)
{
   struct owner_state *s = (struct owner_state *)context;
   assert(device_destroys == 1 && s->node.prepared && !s->node.device);
   s->destroyed++;
}
static void init(struct owner_state *s, VkDevice device)
{
   memset(s, 0, sizeof(*s));
   s->node.context = s;
   s->node.device = device;
   s->node.prepare = prepare_owner;
   s->node.destroyed = owner_destroyed;
   assert(record_android_capture_device_attach(&s->node));
   assert(!record_android_capture_device_attach(&s->node));
}
static void run(VkResult idle)
{
   struct owner_state a, b, other;
   bool proof = idle == VK_SUCCESS || idle == VK_ERROR_DEVICE_LOST;
   VkDevice device = (VkDevice)(uintptr_t)11;
   VkDevice second = (VkDevice)(uintptr_t)12;
   device_destroys = cached = 0;
   expected_device = device;
   init(&a, device);
   init(&b, device);
   init(&other, second);
   a.ready = other.ready = true;
   /* One owner refuses closure; no actual destruction or cache transfer. */
   teardown(device, idle, VIDEO_FLAG_CACHE_CONTEXT);
   assert(!device_destroys && !cached && !a.destroyed && !b.destroyed);
   assert(a.proof == proof && b.proof == proof);
   assert(a.node.prepared == proof && !b.node.prepared);
   assert(!other.prepares && !other.destroyed);
   assert(!record_android_capture_device_destroyed(device));
   assert(record_android_capture_device_pending(device));
   b.ready = true;
   teardown(device, VK_SUCCESS, VIDEO_FLAG_CACHE_CONTEXT);
   assert(device_destroys == 1 && !cached);
   assert(a.destroyed == 1 && b.destroyed == 1 && !other.destroyed);
   assert(a.prepares == (proof ? 1u : 2u) && b.prepares == 2);
   assert(!record_android_capture_device_pending(device));
   assert(record_android_capture_device_pending(second));
   assert(record_android_capture_device_destroyed(device));
   assert(a.destroyed == 1 && b.destroyed == 1);
   record_android_capture_device_detach(&a.node);
   record_android_capture_device_detach(&b.node);
   record_android_capture_device_detach(&other.node);
   assert(!record_acdevice_owners);
}
int main(void)
{
   run(VK_SUCCESS);
   run(VK_ERROR_DEVICE_LOST);
   run(VK_ERROR_OUT_OF_HOST_MEMORY);
   run(VK_ERROR_OUT_OF_DEVICE_MEMORY);
   run(VK_ERROR_UNKNOWN);
   device_destroys = cached = 0;
   teardown((VkDevice)(uintptr_t)11, VK_SUCCESS, VIDEO_FLAG_CACHE_CONTEXT);
   assert(cached == 1 && !device_destroys);
   assert(!record_android_capture_device_attach(NULL));
   assert(!record_android_capture_device_pending(VK_NULL_HANDLE));
   assert(!record_android_capture_device_prepare(VK_NULL_HANDLE, true));
   assert(!record_android_capture_device_destroyed(VK_NULL_HANDLE));
   record_android_capture_device_detach(NULL);
   puts("PASS actual Vulkan teardown fragments: matching owners, drain proof, "
        "cache refusal and post-destruction notification");
   return 0;
}
