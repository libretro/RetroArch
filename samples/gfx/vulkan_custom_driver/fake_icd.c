/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */

/* A stand-in Vulkan driver for vulkan_custom_driver_desktop_test: the
 * desktop glue only opens it, looks up vk_icdGetInstanceProcAddr and asks
 * which module a device's vkQueueSubmit lives in. Built as the custom
 * driver and, under another name, as the system one; with
 * -DFAKE_ICD_LAYER it has no vk_icdGetInstanceProcAddr, as a layer. */
#include <stddef.h>

#ifdef _WIN32
#define FAKE_EXPORT __declspec(dllexport)
#else
#define FAKE_EXPORT __attribute__((visibility("default")))
#endif

typedef void (*fake_void_fn)(void);

static int fake_submits;

/* Stands in for the device's vkQueueSubmit */
FAKE_EXPORT int fake_queue_submit(void)
{
   return ++fake_submits;
}

#ifndef FAKE_ICD_LAYER
FAKE_EXPORT fake_void_fn vk_icdGetInstanceProcAddr(void *instance,
      const char *name)
{
   (void)instance;
   (void)name;
   return NULL;
}
#endif
