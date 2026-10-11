/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __VULKAN_OPENXR_H
#define __VULKAN_OPENXR_H

#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <rthreads/rthreads.h>

#include "../include/vulkan/vulkan.h"

#ifndef XR_USE_GRAPHICS_API_VULKAN
#define XR_USE_GRAPHICS_API_VULKAN
#endif
#ifdef _WIN32
/* openxr_platform.h's Win32 bindings name IUnknown. */
#include <unknwn.h>
#endif
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include "openxr_swapchain.h"
#include "../video_xr.h"

RETRO_BEGIN_DECLS

/* Headset output through OpenXR: the runtime's instance, the Vulkan
 * instance and device it asks for, its session, and the thread that
 * runs the headset's frame loop, on the loader built into the tree. */
typedef struct vulkan_openxr vulkan_openxr_t;

#define VULKAN_OPENXR_MAX_EXTS 16

/* The runtime's instance and headset, or NULL after a notification.
 * enable1 is for a core that makes its own device with create_device
 * (v1): the runtime then only lists what the instance and device
 * need. api_version is the Vulkan version the instance will ask for. */
vulkan_openxr_t *vulkan_openxr_new(bool enable1, uint32_t api_version);

/* Before the Vulkan device is destroyed. NULL is fine. */
void vulkan_openxr_free(vulkan_openxr_t *xr);

/* OpenXR failed before the device existed: log, tell the user, free. */
void vulkan_openxr_drop(vulkan_openxr_t *xr);

/* A kept Vulkan context cannot take the runtime in: tell the user. */
void vulkan_openxr_needs_reload(void);

bool vulkan_openxr_uses_enable2(const vulkan_openxr_t *xr);

/* XR_KHR_vulkan_enable: the extensions the runtime needs, valid until
 * vulkan_openxr_free(). Returns how many were written. */
unsigned vulkan_openxr_instance_extensions(const vulkan_openxr_t *xr,
      const char **out, unsigned cap);
unsigned vulkan_openxr_device_extensions(const vulkan_openxr_t *xr,
      const char **out, unsigned cap);

/* XR_KHR_vulkan_enable2: vkCreateInstance and vkCreateDevice through
 * the runtime, which adds what it needs. */
VkResult vulkan_openxr_create_instance(vulkan_openxr_t *xr,
      PFN_vkGetInstanceProcAddr gipa, const VkInstanceCreateInfo *info,
      VkInstance *instance);
VkResult vulkan_openxr_create_device(vulkan_openxr_t *xr,
      PFN_vkGetInstanceProcAddr gipa, VkPhysicalDevice gpu,
      const VkDeviceCreateInfo *info, VkDevice *device);

/* The GPU the headset is attached to, or VK_NULL_HANDLE. */
VkPhysicalDevice vulkan_openxr_gpu(vulkan_openxr_t *xr, VkInstance instance);

/* What the headset input needs from a session, valid between the hooks'
 * session_created and session_destroying. */
typedef struct vulkan_openxr_handles
{
   vulkan_openxr_t *xr;
   XrInstance instance;
   XrSession session;
   XrSpace local_space;
   PFN_xrGetInstanceProcAddr get_proc;
   /* XR_VALVE_frame_controller_interaction is enabled. */
   bool frame_controller;
} vulkan_openxr_handles_t;

#define VULKAN_OPENXR_MAX_EXTRA_LAYERS 4

typedef struct vulkan_openxr_hooks
{
   /* The session and its spaces exist and its frame loop has not
    * started: make, suggest and attach action sets here, and destroy
    * them in session_destroying. A kept device's instance gets a new
    * session after the runtime ends one. */
   void (*session_created)(void *user, const vulkan_openxr_handles_t *handles);
   /* The frame loop has stopped; the session is destroyed next. */
   void (*session_destroying)(void *user, vulkan_openxr_t *xr);
   /* The XR thread, each headset frame: up to cap layers drawn over
    * RetroArch's. Returns how many were written. */
   unsigned (*frame_layers)(void *user, XrTime display_time,
         const XrCompositionLayerBaseHeader **layers, unsigned cap);
   void *user;
} vulkan_openxr_hooks_t;

/* Main thread, while the XR thread is stopped. NULL clears. */
void vulkan_openxr_set_hooks(const vulkan_openxr_hooks_t *hooks);

/* Makes the session on the first call, and again when the runtime ended
 * the last one, then (again after a stop) starts the XR thread. Every
 * OpenXR call that may use the queue holds queue_lock. False when the
 * session could not be made. */
bool vulkan_openxr_start(vulkan_openxr_t *xr, VkInstance instance,
      VkPhysicalDevice gpu, VkDevice device, uint32_t queue_family,
      slock_t *queue_lock);

/* Stops the XR thread; the session stays. NULL is fine. */
void vulkan_openxr_stop(vulkan_openxr_t *xr);

/* Stops the XR thread alone, before a core's context_destroy: the core
 * may wait on the device there without the queue lock. NULL is fine. */
void vulkan_openxr_stop_thread(vulkan_openxr_t *xr);

/* The session failed on a device made for it: frees, tells the user,
 * and has the next video init skip the runtime. */
void vulkan_openxr_drop_and_reinit(vulkan_openxr_t *xr);

/* Until the runtime ends the session or its thread stops: the headset
 * shows both eyes. */
bool vulkan_openxr_alive(vulkan_openxr_t *xr);

/* While stopped: the runtime lost the instance, so no session can be
 * made on its device again. */
bool vulkan_openxr_lost(const vulkan_openxr_t *xr);

/* The session's focus as the XR thread last saw it, and its latest
 * predicted display time. */
bool vulkan_openxr_focused(vulkan_openxr_t *xr);
XrTime vulkan_openxr_predicted_time(vulkan_openxr_t *xr);

#define VULKAN_OPENXR_MAX_IMAGES OPENXR_MAX_IMAGES

/* Video thread: whether to draw for the headset now. */
bool vulkan_openxr_should_draw(vulkan_openxr_t *xr);

/* Headset pixels per radian; 0 until the first headset frame. */
float vulkan_openxr_pixels_per_radian(vulkan_openxr_t *xr);

/* Any thread: the headset's refresh rate from its measured period, 0
 * until VIDEO_XR_PERIODS frames agree. */
float vulkan_openxr_refresh_rate(vulkan_openxr_t *xr);

/* Video thread, each frame: the XR thread ticks every interval headset
 * frames; 0 stops. */
void vulkan_openxr_set_pacing(vulkan_openxr_t *xr, unsigned interval);

/* Video thread, once a core frame: wait for the next tick while the
 * headset shows the session, at most two tick intervals; while it
 * doesn't, sleep to where the next tick would be. */
void vulkan_openxr_pace_wait(vulkan_openxr_t *xr);

/* Video thread, a paced frame that waits for no tick: the next wait
 * wants a fresh one. */
void vulkan_openxr_pace_skip(vulkan_openxr_t *xr);

/* The rates XR_FB_display_refresh_rate lists for the session, none
 * without it; up to cap of them. */
unsigned vulkan_openxr_refresh_rates(const vulkan_openxr_t *xr,
      float *rates, unsigned cap);

/* Any thread: the XR thread asks the headset for hz (one it listed; 0
 * asks nothing), once a session and value. */
void vulkan_openxr_request_rate(vulkan_openxr_t *xr, float hz);

/* The largest swapchain side the headset takes. */
unsigned vulkan_openxr_max_dim(const vulkan_openxr_t *xr);

bool vulkan_openxr_supports_format(const vulkan_openxr_t *xr,
      VkFormat format);

/* Video thread. A slot's swapchains, replacing any it had; they take
 * the queue lock themselves. images[i][l] is image i of layer l's. */
bool vulkan_openxr_slot_create(vulkan_openxr_t *xr, unsigned slot,
      VkFormat format, bool mutable_format, unsigned dims, unsigned layers,
      VkImage (*images)[2], unsigned *num_images);
void vulkan_openxr_slot_destroy(vulkan_openxr_t *xr, unsigned slot);

/* Video thread, holding the queue lock. Acquire keeps an image of each
 * layer until they can be written, never waiting on the compositor:
 * false means try next frame. index[l] is layer l's. Release after the
 * frame that drew them is submitted. */
bool vulkan_openxr_slot_acquire(vulkan_openxr_t *xr, unsigned slot,
      unsigned *index);
void vulkan_openxr_slot_release(vulkan_openxr_t *xr, unsigned slot);
/* Video thread. The slot's quads are left out again until it releases
 * another image. */
void vulkan_openxr_slot_forget(vulkan_openxr_t *xr, unsigned slot);

/* The quads the XR thread submits from now on; a quad whose slot has
 * not released an image yet is left out. */
void vulkan_openxr_publish(vulkan_openxr_t *xr,
      const video_xr_quad_set_t *set);

/* Where screen 0 is centred in front of. */
void vulkan_openxr_get_anchor(vulkan_openxr_t *xr, video_xr_pose_t *anchor);

/* Any thread: the quads the XR thread submits. */
bool vulkan_openxr_get_quads(vulkan_openxr_t *xr, video_xr_quad_set_t *out);

/* Any thread: place the screens in front of where the headset looks, at
 * its next frame. */
void vulkan_openxr_request_recenter(vulkan_openxr_t *xr);

#define VULKAN_OPENXR_CURSOR_DIM 32

/* The laser's round dot: one image, made at each start and freed at
 * each stop, for layers the hooks add. XR_NULL_HANDLE if it could not
 * be made. The XR thread may use it. */
XrSwapchain vulkan_openxr_cursor(const vulkan_openxr_t *xr);

RETRO_END_DECLS

#endif
