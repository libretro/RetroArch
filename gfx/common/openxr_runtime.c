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

#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <retro_miscellaneous.h>
#include <compat/strl.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "openxr_runtime.h"

#include "../../msg_hash.h"
#include "../../runloop.h"
#include "../../verbosity.h"

/* The Steam Frame's controllers; newer than the bundled headers. */
#define OPENXR_FRAME_EXT "XR_VALVE_frame_controller_interaction"

PFN_xrVoidFunction openxr_runtime_proc(const openxr_runtime_t *rt,
      const char *name)
{
   PFN_xrVoidFunction fn = NULL;
   if (rt->GetInstanceProcAddr(rt->instance, name, &fn) != XR_SUCCESS)
      return NULL;
   return fn;
}

void openxr_runtime_notify(unsigned msg)
{
   const char *s = msg_hash_to_str((enum msg_hash_enums)msg);
   runloop_msg_queue_push(s, strlen(s), 2, 240, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
}

static bool openxr_runtime_has_extension(
      PFN_xrEnumerateInstanceExtensionProperties enum_exts,
      uint32_t count, const char *name)
{
   uint32_t i;
   bool found                   = false;
   XrExtensionProperties *props = (XrExtensionProperties*)
      calloc(count, sizeof(*props));
   if (!props)
      return false;
   for (i = 0; i < count; i++)
      props[i].type = XR_TYPE_EXTENSION_PROPERTIES;
   if (XR_SUCCEEDED(enum_exts(NULL, count, &count, props)))
      for (i = 0; i < count && !found; i++)
         found = string_is_equal(props[i].extensionName, name);
   free(props);
   return found;
}

/* Before anything else is asked of the loader. */
static bool openxr_runtime_start_loader(openxr_runtime_t *rt)
{
   XrResult res;
   PFN_xrInitializeLoaderKHR init = NULL;
   if (     XR_FAILED(res = rt->GetInstanceProcAddr(XR_NULL_HANDLE,
               "xrInitializeLoaderKHR", (PFN_xrVoidFunction*)&init))
         || !init
         || XR_FAILED(res = init(rt->platform.loader)))
   {
      RARCH_WARN("[OpenXR] The loader could not be started (%d).\n",
            (int)res);
      return false;
   }
   return true;
}

/* An optional extension the runtime has, while there is room for it. */
static bool openxr_runtime_want_extension(
      PFN_xrEnumerateInstanceExtensionProperties enum_exts, uint32_t count,
      const char **exts, uint32_t *num_exts, uint32_t cap, const char *name)
{
   if (!openxr_runtime_has_extension(enum_exts, count, name))
      return false;
   if (*num_exts >= cap)
   {
      RARCH_WARN("[OpenXR] No room to enable %s.\n", name);
      return false;
   }
   exts[(*num_exts)++] = name;
   return true;
}

#define OPENXR_RUNTIME_WANT(name) openxr_runtime_want_extension(enum_exts, \
      count, exts, &num_exts, cap, (name))

enum openxr_runtime_result openxr_runtime_create_instance(
      openxr_runtime_t *rt, PFN_xrGetInstanceProcAddr get_proc,
      const char *graphics_ext)
{
   XrResult res;
   uint32_t count = 0;
   uint32_t num_exts;
   uint32_t cap;
   /* The graphics and binding extensions, then the optional ones. */
   const char *exts[10];
   XrInstanceCreateInfo ici;
   PFN_xrEnumerateInstanceExtensionProperties enum_exts;
   PFN_xrCreateInstance create_instance;

   rt->GetInstanceProcAddr = get_proc;
   if (rt->platform.loader && !openxr_runtime_start_loader(rt))
      return OPENXR_RUNTIME_UNAVAILABLE;
   enum_exts       = (PFN_xrEnumerateInstanceExtensionProperties)
      openxr_runtime_proc(rt, "xrEnumerateInstanceExtensionProperties");
   create_instance = (PFN_xrCreateInstance)
      openxr_runtime_proc(rt, "xrCreateInstance");
   if (!enum_exts || !create_instance)
   {
      RARCH_WARN("[OpenXR] No runtime (the loader lacks xrCreateInstance or xrEnumerateInstanceExtensionProperties).\n");
      return OPENXR_RUNTIME_UNAVAILABLE;
   }
   if (XR_FAILED(res = enum_exts(NULL, 0, &count, NULL)))
   {
      RARCH_WARN("[OpenXR] No runtime (xrEnumerateInstanceExtensionProperties: %d).\n",
            (int)res);
      return OPENXR_RUNTIME_UNAVAILABLE;
   }
   if (!openxr_runtime_has_extension(enum_exts, count, graphics_ext))
   {
      RARCH_WARN("[OpenXR] The runtime lacks %s.\n", graphics_ext);
      return OPENXR_RUNTIME_UNAVAILABLE;
   }
   exts[0]  = graphics_ext;
   num_exts = 1;
   if (     rt->binding_ext
         && !string_is_equal(rt->binding_ext, graphics_ext))
   {
      if (!openxr_runtime_has_extension(enum_exts, count, rt->binding_ext))
      {
         RARCH_WARN("[OpenXR] The runtime lacks %s.\n", rt->binding_ext);
         return OPENXR_RUNTIME_UNAVAILABLE;
      }
      exts[num_exts++] = rt->binding_ext;
   }
   /* The platform's goes last, its room kept. */
   cap = (uint32_t)ARRAY_SIZE(exts) - (rt->platform.instance_ext ? 1 : 0);
   rt->frame_controller = OPENXR_RUNTIME_WANT(OPENXR_FRAME_EXT);
   rt->refresh_ext      = OPENXR_RUNTIME_WANT(
         XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME);
   /* The hands for a core's RETRO_ENVIRONMENT_GET_VR_CONTROLLER_POSES:
    * the palm pose, and the joints where the headset sees them. */
   rt->palm_pose        = OPENXR_RUNTIME_WANT(
         XR_EXT_PALM_POSE_EXTENSION_NAME);
   rt->hand_tracking    = OPENXR_RUNTIME_WANT(
         XR_EXT_HAND_TRACKING_EXTENSION_NAME);
   if (rt->hand_tracking)
      rt->hand_data_source = OPENXR_RUNTIME_WANT(
            XR_EXT_HAND_TRACKING_DATA_SOURCE_EXTENSION_NAME);
   /* To ask for the clocks a core's frame needs: see
    * openxr_frame_ask_perf. */
   rt->perf_settings    = OPENXR_RUNTIME_WANT(
         XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME);
   cap = (uint32_t)ARRAY_SIZE(exts);
   if (rt->platform.instance_ext)
      OPENXR_RUNTIME_WANT(rt->platform.instance_ext);

   ici.type                               = XR_TYPE_INSTANCE_CREATE_INFO;
   ici.createFlags                        = 0;
   ici.applicationInfo.applicationVersion = 0;
   ici.applicationInfo.engineVersion      = 0;
   ici.enabledApiLayerCount               = 0;
   ici.enabledApiLayerNames               = NULL;
   ici.next                               = rt->platform.instance_next;
   strlcpy(ici.applicationInfo.applicationName, "RetroArch",
         sizeof(ici.applicationInfo.applicationName));
   strlcpy(ici.applicationInfo.engineName, "RetroArch",
         sizeof(ici.applicationInfo.engineName));
   ici.applicationInfo.apiVersion = XR_API_VERSION_1_0;
   ici.enabledExtensionCount      = num_exts;
   ici.enabledExtensionNames      = exts;
   if (XR_FAILED(res = create_instance(&ici, &rt->instance)))
   {
      RARCH_WARN("[OpenXR] No runtime (xrCreateInstance: %d).\n", (int)res);
      rt->instance = XR_NULL_HANDLE;
      return OPENXR_RUNTIME_UNAVAILABLE;
   }
   if (!OPENXR_FN(rt, rt, DestroyInstance))
   {
      RARCH_ERR("[OpenXR] The runtime lacks functions headset output needs.\n");
      return OPENXR_RUNTIME_FAILED;
   }
   /* Without them the rate is still measured, never asked for. */
   if (     rt->refresh_ext
         && (  !OPENXR_FN(rt, rt, EnumerateDisplayRefreshRatesFB)
            || !OPENXR_FN(rt, rt, RequestDisplayRefreshRateFB)))
      rt->refresh_ext = false;
   if (     rt->perf_settings
         && !OPENXR_FN(rt, rt, PerfSettingsSetPerformanceLevelEXT))
      rt->perf_settings = false;
   return OPENXR_RUNTIME_OK;
}

enum openxr_runtime_result openxr_runtime_find_system(openxr_runtime_t *rt)
{
   XrResult res;
   uint32_t count = 0;
   XrSystemGetInfo sgi;
   XrSystemProperties props;
   XrSystemHandTrackingPropertiesEXT hand_props;
   XrViewConfigurationView views[2];
   XrEnvironmentBlendMode modes[8];
   PFN_xrGetSystem get_system;
   PFN_xrGetSystemProperties get_system_properties;
   PFN_xrEnumerateViewConfigurationViews enum_views;
   PFN_xrEnumerateEnvironmentBlendModes enum_modes;

   get_system            = (PFN_xrGetSystem)
      openxr_runtime_proc(rt, "xrGetSystem");
   get_system_properties = (PFN_xrGetSystemProperties)
      openxr_runtime_proc(rt, "xrGetSystemProperties");
   enum_views            = (PFN_xrEnumerateViewConfigurationViews)
      openxr_runtime_proc(rt, "xrEnumerateViewConfigurationViews");
   enum_modes            = (PFN_xrEnumerateEnvironmentBlendModes)
      openxr_runtime_proc(rt, "xrEnumerateEnvironmentBlendModes");
   if (!get_system || !get_system_properties || !enum_views || !enum_modes)
   {
      RARCH_ERR("[OpenXR] The runtime lacks functions headset output needs.\n");
      return OPENXR_RUNTIME_FAILED;
   }

   sgi.type       = XR_TYPE_SYSTEM_GET_INFO;
   sgi.next       = NULL;
   sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
   if (XR_FAILED(res = get_system(rt->instance, &sgi, &rt->system)))
   {
      RARCH_WARN("[OpenXR] No headset (xrGetSystem: %d).\n", (int)res);
      return OPENXR_RUNTIME_UNAVAILABLE;
   }

   rt->max_dim    = 4096;
   rt->max_layers = XR_MIN_COMPOSITION_LAYERS_SUPPORTED;
   props.type      = XR_TYPE_SYSTEM_PROPERTIES;
   props.next      = NULL;
   hand_props.type = XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT;
   hand_props.next = NULL;
   if (rt->hand_tracking)
      props.next   = &hand_props;
   if (XR_SUCCEEDED(get_system_properties(rt->instance, rt->system, &props)))
   {
      uint32_t mw = props.graphicsProperties.maxSwapchainImageWidth;
      uint32_t mh = props.graphicsProperties.maxSwapchainImageHeight;
      if (mw && mw < rt->max_dim)
         rt->max_dim = mw;
      if (mh && mh < rt->max_dim)
         rt->max_dim = mh;
      if (props.graphicsProperties.maxLayerCount)
         rt->max_layers = props.graphicsProperties.maxLayerCount;
      if (rt->hand_tracking && !hand_props.supportsHandTracking)
         rt->hand_tracking = false;
      RARCH_LOG("[OpenXR] Headset: %s.\n", props.systemName);
   }

   views[0].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
   views[0].next = NULL;
   views[1].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
   views[1].next = NULL;
   count         = 0;
   if (     XR_FAILED(enum_views(rt->instance, rt->system,
               XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2, &count, views))
         || count < 1)
   {
      RARCH_WARN("[OpenXR] No headset (no stereo view configuration).\n");
      return OPENXR_RUNTIME_UNAVAILABLE;
   }
   rt->rec_width  = views[0].recommendedImageRectWidth;
   rt->rec_height = views[0].recommendedImageRectHeight;

   count          = 0;
   rt->blend_mode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
   if (     XR_SUCCEEDED(enum_modes(rt->instance, rt->system,
               XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 8, &count, modes))
         && count)
   {
      uint32_t i;
      /* A runtime with passthrough may list a blend first. */
      rt->blend_mode = modes[0];
      for (i = 0; i < count && i < 8; i++)
         if (modes[i] == XR_ENVIRONMENT_BLEND_MODE_OPAQUE)
            rt->blend_mode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
   }
   return OPENXR_RUNTIME_OK;
}

void openxr_runtime_deinit(openxr_runtime_t *rt)
{
   if (rt->instance && rt->DestroyInstance)
      rt->DestroyInstance(rt->instance);
   rt->instance = XR_NULL_HANDLE;
}
