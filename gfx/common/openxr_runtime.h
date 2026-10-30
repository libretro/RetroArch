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

#ifndef __OPENXR_RUNTIME_H
#define __OPENXR_RUNTIME_H

#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>

#include <openxr/openxr.h>

RETRO_BEGIN_DECLS

enum openxr_runtime_result
{
   OPENXR_RUNTIME_OK = 0,
   /* No runtime or no headset: headset output is off, nothing failed. */
   OPENXR_RUNTIME_UNAVAILABLE,
   OPENXR_RUNTIME_FAILED
};

/* A platform whose loader has to be started first (Android): what it
 * is started with, and what the instance is told. */
typedef struct openxr_runtime_platform
{
   const XrLoaderInitInfoBaseHeaderKHR *loader;  /* NULL: none to start */
   const void *instance_next;   /* chained into XrInstanceCreateInfo */
   const char *instance_ext;    /* enabled when the runtime lists it */
} openxr_runtime_platform_t;

typedef struct openxr_runtime
{
   PFN_xrGetInstanceProcAddr GetInstanceProcAddr;
   PFN_xrDestroyInstance DestroyInstance;
   PFN_xrEnumerateDisplayRefreshRatesFB EnumerateDisplayRefreshRatesFB;
   PFN_xrRequestDisplayRefreshRateFB RequestDisplayRefreshRateFB;
   PFN_xrPerfSettingsSetPerformanceLevelEXT PerfSettingsSetPerformanceLevelEXT;
   XrInstance instance;
   XrSystemId system;
   XrEnvironmentBlendMode blend_mode;
   unsigned max_dim;          /* the largest swapchain side */
   unsigned max_width;        /* the widest swapchain, 0 if not said */
   uint32_t rec_width;        /* one eye's recommended image width */
   uint32_t rec_height;       /* and its height */
   uint32_t max_layers;       /* the most layers a frame may end with */
   bool frame_controller;     /* XR_VALVE_frame_controller_interaction */
   bool refresh_ext;          /* XR_FB_display_refresh_rate */
   bool palm_pose;            /* XR_EXT_palm_pose */
   bool hand_tracking;        /* XR_EXT_hand_tracking. */
   bool hand_data_source;     /* XR_EXT_hand_tracking_data_source */
   bool perf_settings;        /* XR_EXT_performance_settings */
   openxr_runtime_platform_t platform;   /* set before create_instance */
   const char *binding_ext;
} openxr_runtime_t;

/* The instance, with graphics_ext and the optional extensions the
 * runtime offers. get_proc is the loader's xrGetInstanceProcAddr. */
enum openxr_runtime_result openxr_runtime_create_instance(
      openxr_runtime_t *rt, PFN_xrGetInstanceProcAddr get_proc,
      const char *graphics_ext);

/* The headset, its largest swapchain, one eye's size and the blend
 * mode. After the binding has loaded its own functions. */
enum openxr_runtime_result openxr_runtime_find_system(openxr_runtime_t *rt);

void openxr_runtime_deinit(openxr_runtime_t *rt);

PFN_xrVoidFunction openxr_runtime_proc(const openxr_runtime_t *rt,
      const char *name);

/* A message_queue warning; msg is an enum msg_hash_enums. */
void openxr_runtime_notify(unsigned msg);

/* An instance function into obj's field named like it, without "xr". */
#define OPENXR_FN(rt, obj, name) \
   ((obj)->name = (PFN_xr##name)openxr_runtime_proc((rt), "xr" #name))

RETRO_END_DECLS

#endif
