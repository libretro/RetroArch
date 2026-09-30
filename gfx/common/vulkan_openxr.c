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
#include <math.h>
#include <string.h>

#include <boolean.h>
#include <compat/strl.h>
#include <features/features_cpu.h>
#include <retro_atomic.h>
#include <retro_timers.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "vksym.h"
#include "vulkan_openxr.h"
#include "../video_defines.h"
#include "../video_driver.h"

#include "../../msg_hash.h"
#include "../../runloop.h"
#include "../../verbosity.h"

#define VULKAN_OPENXR_EXT_BUF 1024
#define VULKAN_OPENXR_MAX_FORMATS 64
/* The Steam Frame's controllers; newer than the bundled headers. */
#define VULKAN_OPENXR_FRAME_EXT "XR_VALVE_frame_controller_interaction"

/* A swapchain per layer, not one array: SteamVR on the Steam Frame
 * ignores a quad's imageArrayIndex and shows layer 0. */
struct vulkan_openxr_slot
{
   XrSwapchain swapchains[2];   /* changed under the queue lock and lock */
   unsigned dims;
   unsigned layers;
   uint32_t index[2];           /* video thread */
   bool acquired[2];
   bool waited[2];
   retro_atomic_int_t content;  /* every layer has released an image */
};

struct vulkan_openxr
{
   XrInstance instance;
   XrSystemId system;
   XrEnvironmentBlendMode blend_mode;
   unsigned max_dim;
   uint32_t rec_width;
   bool enable2;
   bool frame_controller;
   bool refresh_ext;               /* XR_FB_display_refresh_rate */
   /* The session is made on the thread that makes the device; its frame
    * loop runs on the XR thread. lock guards what both read. */
   XrSession session;
   XrSpace local_space;
   XrSpace view_space;
   VkDevice device;
   slock_t *lock;
   slock_t *queue_lock;
   sthread_t *thread;
   XrTime predicted_time;          /* lock */
   float px_per_rad;               /* lock; the XR thread writes it */
   retro_atomic_int_t quit;
   retro_atomic_int_t state;       /* XrSessionState */
   retro_atomic_int_t alive;
   retro_atomic_int_t recenter;
   bool running;                   /* XR thread */
   bool ended;                     /* XR thread, or while it is stopped */
   /* The instance was lost too, so a kept device cannot get a new
    * session. Written like ended. */
   bool lost;
   bool frame_failed;              /* XR thread */
   /* The headset's period as the XR thread measures it, and the one it
    * published, 0 until known. */
   video_xr_period_t period;       /* XR thread */
   retro_atomic_int_t period_ns;
   /* Pacing: every interval headset frames the XR thread bumps tick_seq
    * and signals tick; the video thread waits on it once a core frame,
    * or on the clock while the headset doesn't show the session. */
   scond_t *tick;                  /* with lock */
   uint64_t tick_seq;              /* lock */
   retro_atomic_int_t interval;
   unsigned tick_count;            /* XR thread */
   unsigned tick_interval;         /* XR thread */
   uint64_t tick_seen;             /* video thread */
   int64_t pace_anchor_ns;         /* video thread */
   unsigned pace_mode;             /* video thread: 0, 1 ticks, 2 clock */
   bool tick_late;                 /* video thread: warned, no tick since */
   /* XR_FB_display_refresh_rate: the rates the session lists, the one
    * to ask for (float bits, 0 for none), and the last one asked. */
   float rates[VIDEO_HEADSET_MAX_RATES];
   unsigned num_rates;
   retro_atomic_int_t want_rate;
   float asked_rate;               /* XR thread */
   int64_t formats[VULKAN_OPENXR_MAX_FORMATS];
   uint32_t num_formats;
   struct vulkan_openxr_slot slots[VIDEO_XR_MAX_SLOTS];
   video_xr_quad_set_t quads;      /* lock */
   video_xr_pose_t anchor;         /* lock */
   XrSwapchain cursor;             /* start to stop; the XR thread reads it */

   /* XR_KHR_vulkan_enable's lists, split in place. */
   char inst_ext_buf[VULKAN_OPENXR_EXT_BUF];
   char dev_ext_buf[VULKAN_OPENXR_EXT_BUF];
   const char *inst_exts[VULKAN_OPENXR_MAX_EXTS];
   const char *dev_exts[VULKAN_OPENXR_MAX_EXTS];
   unsigned num_inst_exts;
   unsigned num_dev_exts;

   PFN_xrGetInstanceProcAddr GetInstanceProcAddr;
   PFN_xrDestroyInstance DestroyInstance;
   PFN_xrPollEvent PollEvent;
   PFN_xrCreateSession CreateSession;
   PFN_xrDestroySession DestroySession;
   PFN_xrBeginSession BeginSession;
   PFN_xrEndSession EndSession;
   PFN_xrCreateReferenceSpace CreateReferenceSpace;
   PFN_xrDestroySpace DestroySpace;
   PFN_xrLocateViews LocateViews;
   PFN_xrLocateSpace LocateSpace;
   PFN_xrWaitFrame WaitFrame;
   PFN_xrBeginFrame BeginFrame;
   PFN_xrEndFrame EndFrame;
   PFN_xrEnumerateSwapchainFormats EnumerateSwapchainFormats;
   PFN_xrCreateSwapchain CreateSwapchain;
   PFN_xrDestroySwapchain DestroySwapchain;
   PFN_xrEnumerateSwapchainImages EnumerateSwapchainImages;
   PFN_xrAcquireSwapchainImage AcquireSwapchainImage;
   PFN_xrWaitSwapchainImage WaitSwapchainImage;
   PFN_xrReleaseSwapchainImage ReleaseSwapchainImage;
   PFN_xrGetVulkanGraphicsRequirements2KHR GetVulkanGraphicsRequirements2KHR;
   PFN_xrCreateVulkanInstanceKHR CreateVulkanInstanceKHR;
   PFN_xrCreateVulkanDeviceKHR CreateVulkanDeviceKHR;
   PFN_xrGetVulkanGraphicsDevice2KHR GetVulkanGraphicsDevice2KHR;
   PFN_xrGetVulkanGraphicsRequirementsKHR GetVulkanGraphicsRequirementsKHR;
   PFN_xrGetVulkanGraphicsDeviceKHR GetVulkanGraphicsDeviceKHR;
   PFN_xrGetVulkanInstanceExtensionsKHR GetVulkanInstanceExtensionsKHR;
   PFN_xrGetVulkanDeviceExtensionsKHR GetVulkanDeviceExtensionsKHR;
   PFN_xrEnumerateDisplayRefreshRatesFB EnumerateDisplayRefreshRatesFB;
   PFN_xrRequestDisplayRefreshRateFB RequestDisplayRefreshRateFB;
};

static PFN_xrVoidFunction vulkan_openxr_proc(const vulkan_openxr_t *xr,
      const char *name)
{
   PFN_xrVoidFunction fn = NULL;
   if (xr->GetInstanceProcAddr(xr->instance, name, &fn) != XR_SUCCESS)
      return NULL;
   return fn;
}

/* An instance function into the field named like it, without "xr". */
#define VULKAN_OPENXR_FN(xr, name) \
   ((xr)->name = (PFN_xr##name)vulkan_openxr_proc((xr), "xr" #name))

static vulkan_openxr_hooks_t vulkan_openxr_hooks;
/* Set after a session fails on a device made for it: the reinit that
 * follows builds the device without the runtime. */
static bool vulkan_openxr_skip_once;

static void vulkan_openxr_notify(enum msg_hash_enums msg)
{
   const char *s = msg_hash_to_str(msg);
   runloop_msg_queue_push(s, strlen(s), 2, 240, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
}

static bool vulkan_openxr_has_extension(
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

/* Splits a space-separated list in buf into names; false when there
 * are more than VULKAN_OPENXR_MAX_EXTS. */
static bool vulkan_openxr_split(char *buf, const char **names,
      unsigned *count)
{
   unsigned n = 0;
   char *p    = buf;
   for (;;)
   {
      while (*p == ' ')
         *p++ = '\0';
      if (!*p)
         break;
      if (n == VULKAN_OPENXR_MAX_EXTS)
         return false;
      names[n++] = p;
      while (*p && *p != ' ')
         p++;
   }
   *count = n;
   return true;
}

/* False rather than a list cut short, which would leave the runtime
 * without extensions it needs. */
static bool vulkan_openxr_list(vulkan_openxr_t *xr,
      PFN_xrGetVulkanInstanceExtensionsKHR get, char *buf,
      const char **names, unsigned *count)
{
   uint32_t len = 0;
   *count       = 0;
   if (     XR_FAILED(get(xr->instance, xr->system, 0, &len, NULL))
         || len > VULKAN_OPENXR_EXT_BUF)
      return false;
   if (!len)
      return true;
   if (XR_FAILED(get(xr->instance, xr->system, len, &len, buf)))
      return false;
   buf[VULKAN_OPENXR_EXT_BUF - 1] = '\0';
   return vulkan_openxr_split(buf, names, count);
}

vulkan_openxr_t *vulkan_openxr_new(bool enable1, uint32_t api_version)
{
   XrResult res;
   XrVersion api;
   uint32_t count = 0;
   uint32_t num_exts;
   const char *exts[3];
   XrInstanceCreateInfo ici;
   XrSystemGetInfo sgi;
   XrSystemProperties props;
   XrViewConfigurationView views[2];
   XrEnvironmentBlendMode modes[8];
   XrGraphicsRequirementsVulkanKHR reqs;
   const char *ext;
   PFN_xrEnumerateInstanceExtensionProperties enum_exts;
   PFN_xrCreateInstance create_instance;
   PFN_xrGetSystem get_system;
   PFN_xrGetSystemProperties get_system_properties;
   PFN_xrEnumerateViewConfigurationViews enum_views;
   PFN_xrEnumerateEnvironmentBlendModes enum_modes;
   vulkan_openxr_t *xr;

   if (vulkan_openxr_skip_once)
   {
      vulkan_openxr_skip_once = false;
      RARCH_LOG("[OpenXR] Starting once without headset output after a failure.\n");
      return NULL;
   }
   if (!(xr = (vulkan_openxr_t*)calloc(1, sizeof(*xr))))
      return NULL;
   xr->enable2 = !enable1;
   ext         = enable1 ? XR_KHR_VULKAN_ENABLE_EXTENSION_NAME
                         : XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME;

   xr->GetInstanceProcAddr = xrGetInstanceProcAddr;
   enum_exts       = (PFN_xrEnumerateInstanceExtensionProperties)
      vulkan_openxr_proc(xr, "xrEnumerateInstanceExtensionProperties");
   create_instance = (PFN_xrCreateInstance)
      vulkan_openxr_proc(xr, "xrCreateInstance");
   if (!enum_exts || !create_instance)
   {
      RARCH_WARN("[OpenXR] No runtime (the loader lacks xrCreateInstance or xrEnumerateInstanceExtensionProperties).\n");
      goto unavailable;
   }

   if (XR_FAILED(res = enum_exts(NULL, 0, &count, NULL)))
   {
      RARCH_WARN("[OpenXR] No runtime (xrEnumerateInstanceExtensionProperties: %d).\n",
            (int)res);
      goto unavailable;
   }
   if (!vulkan_openxr_has_extension(enum_exts, count, ext))
   {
      RARCH_WARN("[OpenXR] The runtime lacks %s.\n", ext);
      goto unavailable;
   }
   exts[0]  = ext;
   num_exts = 1;
   if (vulkan_openxr_has_extension(enum_exts, count, VULKAN_OPENXR_FRAME_EXT))
   {
      exts[num_exts++]     = VULKAN_OPENXR_FRAME_EXT;
      xr->frame_controller = true;
   }
   if (vulkan_openxr_has_extension(enum_exts, count,
            XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME))
   {
      exts[num_exts++] = XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME;
      xr->refresh_ext  = true;
   }

   memset(&ici, 0, sizeof(ici));
   ici.type                       = XR_TYPE_INSTANCE_CREATE_INFO;
   strlcpy(ici.applicationInfo.applicationName, "RetroArch",
         sizeof(ici.applicationInfo.applicationName));
   strlcpy(ici.applicationInfo.engineName, "RetroArch",
         sizeof(ici.applicationInfo.engineName));
   ici.applicationInfo.apiVersion = XR_API_VERSION_1_0;
   ici.enabledExtensionCount      = num_exts;
   ici.enabledExtensionNames      = exts;
   if (XR_FAILED(res = create_instance(&ici, &xr->instance)))
   {
      RARCH_WARN("[OpenXR] No runtime (xrCreateInstance: %d).\n", (int)res);
      xr->instance = XR_NULL_HANDLE;
      goto unavailable;
   }

   get_system            = (PFN_xrGetSystem)
      vulkan_openxr_proc(xr, "xrGetSystem");
   get_system_properties = (PFN_xrGetSystemProperties)
      vulkan_openxr_proc(xr, "xrGetSystemProperties");
   enum_views            = (PFN_xrEnumerateViewConfigurationViews)
      vulkan_openxr_proc(xr, "xrEnumerateViewConfigurationViews");
   enum_modes            = (PFN_xrEnumerateEnvironmentBlendModes)
      vulkan_openxr_proc(xr, "xrEnumerateEnvironmentBlendModes");
   if (     !VULKAN_OPENXR_FN(xr, DestroyInstance)
         || !get_system || !get_system_properties
         || !enum_views || !enum_modes)
      goto missing;
   /* Without them the rate is still measured, never asked for. */
   if (     xr->refresh_ext
         && (  !VULKAN_OPENXR_FN(xr, EnumerateDisplayRefreshRatesFB)
            || !VULKAN_OPENXR_FN(xr, RequestDisplayRefreshRateFB)))
      xr->refresh_ext = false;
   if (enable1)
   {
      if (     !VULKAN_OPENXR_FN(xr, GetVulkanGraphicsRequirementsKHR)
            || !VULKAN_OPENXR_FN(xr, GetVulkanGraphicsDeviceKHR)
            || !VULKAN_OPENXR_FN(xr, GetVulkanInstanceExtensionsKHR)
            || !VULKAN_OPENXR_FN(xr, GetVulkanDeviceExtensionsKHR))
         goto missing;
   }
   else if (!VULKAN_OPENXR_FN(xr, GetVulkanGraphicsRequirements2KHR)
         || !VULKAN_OPENXR_FN(xr, CreateVulkanInstanceKHR)
         || !VULKAN_OPENXR_FN(xr, CreateVulkanDeviceKHR)
         || !VULKAN_OPENXR_FN(xr, GetVulkanGraphicsDevice2KHR))
      goto missing;

   memset(&sgi, 0, sizeof(sgi));
   sgi.type       = XR_TYPE_SYSTEM_GET_INFO;
   sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
   if (XR_FAILED(res = get_system(xr->instance, &sgi, &xr->system)))
   {
      RARCH_WARN("[OpenXR] No headset (xrGetSystem: %d).\n", (int)res);
      goto unavailable;
   }

   xr->max_dim = 4096;
   memset(&props, 0, sizeof(props));
   props.type  = XR_TYPE_SYSTEM_PROPERTIES;
   if (XR_SUCCEEDED(get_system_properties(xr->instance, xr->system, &props)))
   {
      uint32_t mw = props.graphicsProperties.maxSwapchainImageWidth;
      uint32_t mh = props.graphicsProperties.maxSwapchainImageHeight;
      if (mw && mw < xr->max_dim)
         xr->max_dim = mw;
      if (mh && mh < xr->max_dim)
         xr->max_dim = mh;
      RARCH_LOG("[OpenXR] Headset: %s.\n", props.systemName);
   }

   memset(views, 0, sizeof(views));
   views[0].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
   views[1].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
   count         = 0;
   if (     XR_FAILED(enum_views(xr->instance, xr->system,
               XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2, &count, views))
         || count < 1)
   {
      RARCH_WARN("[OpenXR] No headset (no stereo view configuration).\n");
      goto unavailable;
   }
   xr->rec_width  = views[0].recommendedImageRectWidth;

   count          = 0;
   xr->blend_mode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
   if (     XR_SUCCEEDED(enum_modes(xr->instance, xr->system,
               XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 8, &count, modes))
         && count)
      xr->blend_mode = modes[0];

   /* The runtime must be asked before any Vulkan object is made. */
   memset(&reqs, 0, sizeof(reqs));
   reqs.type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR;
   res       = enable1
      ? xr->GetVulkanGraphicsRequirementsKHR(xr->instance, xr->system, &reqs)
      : xr->GetVulkanGraphicsRequirements2KHR(xr->instance, xr->system, &reqs);
   if (XR_FAILED(res))
   {
      RARCH_ERR("[OpenXR] xrGetVulkanGraphicsRequirements failed (%d).\n",
            (int)res);
      goto failed;
   }
   RARCH_LOG("[OpenXR] Vulkan through %s, %u.%u to %u.%u.\n", ext,
         (unsigned)XR_VERSION_MAJOR(reqs.minApiVersionSupported),
         (unsigned)XR_VERSION_MINOR(reqs.minApiVersionSupported),
         (unsigned)XR_VERSION_MAJOR(reqs.maxApiVersionSupported),
         (unsigned)XR_VERSION_MINOR(reqs.maxApiVersionSupported));

   /* Major and minor only. The maximum is what the runtime was tested
    * with, so only a newer major is refused. */
   api = XR_MAKE_VERSION(VK_VERSION_MAJOR(api_version),
         VK_VERSION_MINOR(api_version), 0);
   if (     api < XR_MAKE_VERSION(
               XR_VERSION_MAJOR(reqs.minApiVersionSupported),
               XR_VERSION_MINOR(reqs.minApiVersionSupported), 0)
         || VK_VERSION_MAJOR(api_version)
            > XR_VERSION_MAJOR(reqs.maxApiVersionSupported))
   {
      RARCH_WARN("[OpenXR] The runtime does not take Vulkan %u.%u.\n",
            (unsigned)VK_VERSION_MAJOR(api_version),
            (unsigned)VK_VERSION_MINOR(api_version));
      goto failed;
   }

   if (     enable1
         && (  !vulkan_openxr_list(xr, xr->GetVulkanInstanceExtensionsKHR,
                  xr->inst_ext_buf, xr->inst_exts, &xr->num_inst_exts)
            || !vulkan_openxr_list(xr, xr->GetVulkanDeviceExtensionsKHR,
                  xr->dev_ext_buf, xr->dev_exts, &xr->num_dev_exts)))
   {
      RARCH_ERR("[OpenXR] The runtime's Vulkan extension lists could not be read.\n");
      goto failed;
   }
   return xr;

unavailable:
   vulkan_openxr_notify(MSG_OPENXR_UNAVAILABLE);
   vulkan_openxr_free(xr);
   return NULL;
missing:
   RARCH_ERR("[OpenXR] The runtime lacks functions headset output needs.\n");
failed:
   vulkan_openxr_notify(MSG_OPENXR_FAILED);
   vulkan_openxr_free(xr);
   return NULL;
}

/* After vulkan_openxr_stop(). */
static void vulkan_openxr_destroy_session(vulkan_openxr_t *xr)
{
   if (xr->session && vulkan_openxr_hooks.session_destroying)
      vulkan_openxr_hooks.session_destroying(vulkan_openxr_hooks.user, xr);
   if (xr->view_space)
      xr->DestroySpace(xr->view_space);
   if (xr->local_space)
      xr->DestroySpace(xr->local_space);
   if (xr->session)
      xr->DestroySession(xr->session);
   xr->view_space  = XR_NULL_HANDLE;
   xr->local_space = XR_NULL_HANDLE;
   xr->session     = XR_NULL_HANDLE;
}

void vulkan_openxr_free(vulkan_openxr_t *xr)
{
   if (!xr)
      return;
   vulkan_openxr_stop(xr);
   vulkan_openxr_destroy_session(xr);
   if (xr->instance && xr->DestroyInstance)
      xr->DestroyInstance(xr->instance);
   if (xr->tick)
      scond_free(xr->tick);
   if (xr->lock)
      slock_free(xr->lock);
   free(xr);
}

void vulkan_openxr_drop(vulkan_openxr_t *xr)
{
   RARCH_WARN("[OpenXR] Continuing without headset output.\n");
   vulkan_openxr_notify(MSG_OPENXR_FAILED);
   vulkan_openxr_free(xr);
}

void vulkan_openxr_needs_reload(void)
{
   RARCH_WARN("[OpenXR] The content keeps its Vulkan context; headset output starts when it is loaded again.\n");
   vulkan_openxr_notify(MSG_OPENXR_NEEDS_RELOAD);
}

bool vulkan_openxr_uses_enable2(const vulkan_openxr_t *xr)
{
   return xr->enable2;
}

unsigned vulkan_openxr_instance_extensions(const vulkan_openxr_t *xr,
      const char **out, unsigned cap)
{
   unsigned i;
   for (i = 0; i < xr->num_inst_exts && i < cap; i++)
      out[i] = xr->inst_exts[i];
   return i;
}

unsigned vulkan_openxr_device_extensions(const vulkan_openxr_t *xr,
      const char **out, unsigned cap)
{
   unsigned i;
   for (i = 0; i < xr->num_dev_exts && i < cap; i++)
      out[i] = xr->dev_exts[i];
   return i;
}

VkResult vulkan_openxr_create_instance(vulkan_openxr_t *xr,
      PFN_vkGetInstanceProcAddr gipa, const VkInstanceCreateInfo *info,
      VkInstance *instance)
{
   XrResult res;
   XrVulkanInstanceCreateInfoKHR ci;
   VkResult vk_res = VK_ERROR_INITIALIZATION_FAILED;

   memset(&ci, 0, sizeof(ci));
   ci.type                   = XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR;
   ci.systemId               = xr->system;
   ci.pfnGetInstanceProcAddr = gipa;
   ci.vulkanCreateInfo       = info;
   res = xr->CreateVulkanInstanceKHR(xr->instance, &ci, instance, &vk_res);
   if (XR_FAILED(res) || vk_res != VK_SUCCESS)
   {
      RARCH_WARN("[OpenXR] The runtime could not create the Vulkan instance (%d, %d).\n",
            (int)res, (int)vk_res);
      return (vk_res != VK_SUCCESS) ? vk_res : VK_ERROR_INITIALIZATION_FAILED;
   }
   RARCH_LOG("[OpenXR] Vulkan instance created through the runtime.\n");
   return VK_SUCCESS;
}

VkResult vulkan_openxr_create_device(vulkan_openxr_t *xr,
      PFN_vkGetInstanceProcAddr gipa, VkPhysicalDevice gpu,
      const VkDeviceCreateInfo *info, VkDevice *device)
{
   XrResult res;
   XrVulkanDeviceCreateInfoKHR ci;
   VkResult vk_res = VK_ERROR_INITIALIZATION_FAILED;

   memset(&ci, 0, sizeof(ci));
   ci.type                   = XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR;
   ci.systemId               = xr->system;
   ci.pfnGetInstanceProcAddr = gipa;
   ci.vulkanPhysicalDevice   = gpu;
   ci.vulkanCreateInfo       = info;
   res = xr->CreateVulkanDeviceKHR(xr->instance, &ci, device, &vk_res);
   if (XR_FAILED(res) || vk_res != VK_SUCCESS)
   {
      RARCH_WARN("[OpenXR] The runtime could not create the Vulkan device (%d, %d).\n",
            (int)res, (int)vk_res);
      return (vk_res != VK_SUCCESS) ? vk_res : VK_ERROR_INITIALIZATION_FAILED;
   }
   RARCH_LOG("[OpenXR] Vulkan device created through the runtime.\n");
   return VK_SUCCESS;
}

VkPhysicalDevice vulkan_openxr_gpu(vulkan_openxr_t *xr, VkInstance instance)
{
   XrResult res;
   VkPhysicalDevice gpu = VK_NULL_HANDLE;

   if (xr->enable2)
   {
      XrVulkanGraphicsDeviceGetInfoKHR gi;
      memset(&gi, 0, sizeof(gi));
      gi.type           = XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR;
      gi.systemId       = xr->system;
      gi.vulkanInstance = instance;
      res = xr->GetVulkanGraphicsDevice2KHR(xr->instance, &gi, &gpu);
   }
   else
      res = xr->GetVulkanGraphicsDeviceKHR(xr->instance, xr->system,
            instance, &gpu);
   if (XR_FAILED(res) || gpu == VK_NULL_HANDLE)
   {
      RARCH_WARN("[OpenXR] The runtime named no GPU (%d).\n", (int)res);
      return VK_NULL_HANDLE;
   }
   return gpu;
}

void vulkan_openxr_set_hooks(const vulkan_openxr_hooks_t *hooks)
{
   if (hooks)
      vulkan_openxr_hooks = *hooks;
   else
      memset(&vulkan_openxr_hooks, 0, sizeof(vulkan_openxr_hooks));
}

static const char *vulkan_openxr_state_name(XrSessionState state)
{
   switch (state)
   {
      case XR_SESSION_STATE_IDLE:
         return "idle";
      case XR_SESSION_STATE_READY:
         return "ready";
      case XR_SESSION_STATE_SYNCHRONIZED:
         return "synchronized";
      case XR_SESSION_STATE_VISIBLE:
         return "visible";
      case XR_SESSION_STATE_FOCUSED:
         return "focused";
      case XR_SESSION_STATE_STOPPING:
         return "stopping";
      case XR_SESSION_STATE_LOSS_PENDING:
         return "loss pending";
      case XR_SESSION_STATE_EXITING:
         return "exiting";
      default:
         break;
   }
   return "unknown";
}

static void vulkan_openxr_ended(vulkan_openxr_t *xr, bool instance_lost)
{
   xr->running = false;
   if (instance_lost)
      xr->lost = true;
   if (xr->ended)
      return;
   xr->ended = true;
   retro_atomic_store_release_int(&xr->alive, 0);
   if (xr->lock && xr->tick)
   {
      slock_lock(xr->lock);
      scond_signal(xr->tick);
      slock_unlock(xr->lock);
   }
   RARCH_WARN("[OpenXR] The headset session ended; the window keeps the output.\n");
   vulkan_openxr_notify(MSG_OPENXR_SESSION_ENDED);
}

static void vulkan_openxr_session_state(vulkan_openxr_t *xr,
      XrSessionState state)
{
   XrResult res;
   retro_atomic_store_release_int(&xr->state, (int)state);
   RARCH_LOG("[OpenXR] Session %s.\n", vulkan_openxr_state_name(state));
   if (state == XR_SESSION_STATE_STOPPING && xr->lock && xr->tick)
   {
      slock_lock(xr->lock);
      scond_signal(xr->tick);
      slock_unlock(xr->lock);
   }
   switch (state)
   {
      case XR_SESSION_STATE_READY:
         {
            XrSessionBeginInfo bi;
            memset(&bi, 0, sizeof(bi));
            bi.type                         = XR_TYPE_SESSION_BEGIN_INFO;
            bi.primaryViewConfigurationType =
               XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            slock_lock(xr->queue_lock);
            res = xr->BeginSession(xr->session, &bi);
            slock_unlock(xr->queue_lock);
            if (XR_SUCCEEDED(res))
               xr->running = true;
            else
               RARCH_ERR("[OpenXR] xrBeginSession failed (%d).\n", (int)res);
         }
         break;
      case XR_SESSION_STATE_STOPPING:
         slock_lock(xr->queue_lock);
         xr->EndSession(xr->session);
         slock_unlock(xr->queue_lock);
         xr->running = false;
         break;
      case XR_SESSION_STATE_EXITING:
         vulkan_openxr_ended(xr, false);
         video_driver_headset_exit_request();
         break;
      case XR_SESSION_STATE_LOSS_PENDING:
         vulkan_openxr_ended(xr, false);
         break;
      default:
         break;
   }
}

/* The runtime's own recenter moves LOCAL, which the anchor is in: the
 * screens go back straight ahead, and a hotkey request with them. */
static void vulkan_openxr_space_changed(vulkan_openxr_t *xr,
      const XrEventDataReferenceSpaceChangePending *ev)
{
   if (     ev->session != xr->session
         || ev->referenceSpaceType != XR_REFERENCE_SPACE_TYPE_LOCAL)
      return;
   retro_atomic_store_release_int(&xr->recenter, 0);
   slock_lock(xr->lock);
   video_xr_pose_identity(&xr->anchor);
   slock_unlock(xr->lock);
   RARCH_LOG("[OpenXR] Recentered by the runtime.\n");
}

static void vulkan_openxr_poll(vulkan_openxr_t *xr)
{
   XrResult res;
   XrEventDataBuffer ev;
   for (;;)
   {
      memset(&ev, 0, sizeof(ev));
      ev.type = XR_TYPE_EVENT_DATA_BUFFER;
      res     = xr->PollEvent(xr->instance, &ev);
      if (res == XR_ERROR_INSTANCE_LOST)
         vulkan_openxr_ended(xr, true);
      if (res != XR_SUCCESS)
         break;
      if (ev.type == XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING)
         vulkan_openxr_space_changed(xr,
               (const XrEventDataReferenceSpaceChangePending*)&ev);
      if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED)
      {
         const XrEventDataSessionStateChanged *sc =
            (const XrEventDataSessionStateChanged*)&ev;
         /* A session destroyed for a new one may have some queued. */
         if (sc->session == xr->session)
            vulkan_openxr_session_state(xr, sc->state);
      }
      else if (ev.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING)
         vulkan_openxr_ended(xr, true);
      else if (ev.type == XR_TYPE_EVENT_DATA_DISPLAY_REFRESH_RATE_CHANGED_FB)
      {
         const XrEventDataDisplayRefreshRateChangedFB *rc =
            (const XrEventDataDisplayRefreshRateChangedFB*)&ev;
         RARCH_LOG("[OpenXR] The headset changed from %.2f to %.2f Hz.\n",
               rc->fromDisplayRefreshRate, rc->toDisplayRefreshRate);
      }
   }
}

/* Pixels per radian across one eye, from the first located views. */
static void vulkan_openxr_measure(vulkan_openxr_t *xr, XrTime time)
{
   float fov;
   uint32_t n = 0;
   XrViewLocateInfo li;
   XrViewState vs;
   XrView views[2];

   memset(&li, 0, sizeof(li));
   li.type                  = XR_TYPE_VIEW_LOCATE_INFO;
   li.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
   li.displayTime           = time;
   li.space                 = xr->local_space;
   memset(&vs, 0, sizeof(vs));
   vs.type                  = XR_TYPE_VIEW_STATE;
   memset(views, 0, sizeof(views));
   views[0].type            = XR_TYPE_VIEW;
   views[1].type            = XR_TYPE_VIEW;
   if (     XR_FAILED(xr->LocateViews(xr->session, &li, &vs, 2, &n, views))
         || !n)
      return;
   fov = views[0].fov.angleRight - views[0].fov.angleLeft;
   if (fov < 0.1f || !xr->rec_width)
      return;
   slock_lock(xr->lock);
   xr->px_per_rad = (float)xr->rec_width / fov;
   slock_unlock(xr->lock);
   RARCH_LOG("[OpenXR] %u pixels across %.0f degrees per eye.\n",
         (unsigned)xr->rec_width, fov * 57.29578f);
}

static void vulkan_openxr_frame_error(vulkan_openxr_t *xr,
      const char *fn, XrResult res)
{
   if (!xr->frame_failed)
   {
      xr->frame_failed = true;
      RARCH_ERR("[OpenXR] %s failed (%d).\n", fn, (int)res);
   }
   /* A runtime may report the loss here without an event. */
   if (res == XR_ERROR_SESSION_LOST || res == XR_ERROR_INSTANCE_LOST)
      vulkan_openxr_ended(xr, res == XR_ERROR_INSTANCE_LOST);
}

/* The published quads whose slot has released an image. The caller
 * holds the queue lock, so no slot changes under it. */
static unsigned vulkan_openxr_layers(vulkan_openxr_t *xr,
      XrCompositionLayerQuad *layers,
      const XrCompositionLayerBaseHeader **ptrs)
{
   unsigned i;
   unsigned n = 0;
   slock_lock(xr->lock);
   for (i = 0; i < xr->quads.num_quads; i++)
   {
      const video_xr_quad_t *q        = &xr->quads.quads[i];
      struct vulkan_openxr_slot *slot = &xr->slots[q->slot];
      XrCompositionLayerQuad *l       = &layers[n];
      if (     !slot->swapchains[0] || q->layer >= slot->layers
            || !retro_atomic_load_acquire_int(&slot->content))
         continue;
      memset(l, 0, sizeof(*l));
      l->type          = XR_TYPE_COMPOSITION_LAYER_QUAD;
      /* The UI is drawn over transparent black: premultiplied. */
      l->layerFlags    = (q->kind == VIDEO_XR_QUAD_MENU)
         ? XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT : 0;
      l->space         = xr->local_space;
      l->eyeVisibility = (q->eye == VIDEO_XR_EYE_LEFT)
         ? XR_EYE_VISIBILITY_LEFT
         : ((q->eye == VIDEO_XR_EYE_RIGHT)
               ? XR_EYE_VISIBILITY_RIGHT : XR_EYE_VISIBILITY_BOTH);
      l->subImage.swapchain               = slot->swapchains[q->layer];
      l->subImage.imageRect.extent.width  = (int32_t)VIDEO_SCALE_W(slot->dims);
      l->subImage.imageRect.extent.height = (int32_t)VIDEO_SCALE_H(slot->dims);
      l->pose.orientation.x = q->pose.orientation.x;
      l->pose.orientation.y = q->pose.orientation.y;
      l->pose.orientation.z = q->pose.orientation.z;
      l->pose.orientation.w = q->pose.orientation.w;
      l->pose.position.x    = q->pose.position.x;
      l->pose.position.y    = q->pose.position.y;
      l->pose.position.z    = q->pose.position.z;
      l->size.width         = q->width;
      l->size.height        = q->height;
      ptrs[n++]             = (const XrCompositionLayerBaseHeader*)l;
   }
   slock_unlock(xr->lock);
   return n;
}

/* The head's position and heading at time become the anchor. */
static void vulkan_openxr_recenter(vulkan_openxr_t *xr, XrTime time)
{
   XrSpaceLocation loc;
   video_xr_pose_t head, anchor;
   XrSpaceLocationFlags valid = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT
      | XR_SPACE_LOCATION_POSITION_VALID_BIT;

   memset(&loc, 0, sizeof(loc));
   loc.type = XR_TYPE_SPACE_LOCATION;
   if (     XR_FAILED(xr->LocateSpace(xr->view_space, xr->local_space,
               time, &loc))
         || (loc.locationFlags & valid) != valid)
   {
      RARCH_WARN("[OpenXR] Recenter: the headset is not tracked.\n");
      return;
   }
   head.orientation.x = loc.pose.orientation.x;
   head.orientation.y = loc.pose.orientation.y;
   head.orientation.z = loc.pose.orientation.z;
   head.orientation.w = loc.pose.orientation.w;
   head.position.x    = loc.pose.position.x;
   head.position.y    = loc.pose.position.y;
   head.position.z    = loc.pose.position.z;
   if (!video_xr_anchor_from_head(&head, &anchor))
      return;
   slock_lock(xr->lock);
   xr->anchor = anchor;
   slock_unlock(xr->lock);
   RARCH_LOG("[OpenXR] Recentered at %.2f, %.2f, %.2f.\n",
         anchor.position.x, anchor.position.y, anchor.position.z);
}

/* Every interval headset frames, a tick for the core. */
static void vulkan_openxr_tick(vulkan_openxr_t *xr)
{
   unsigned interval = (unsigned)retro_atomic_load_acquire_int(
         &xr->interval);
   if (interval != xr->tick_interval)
   {
      xr->tick_interval = interval;
      xr->tick_count    = 0;
   }
   if (!interval || ++xr->tick_count < interval)
      return;
   xr->tick_count = 0;
   slock_lock(xr->lock);
   xr->tick_seq++;
   scond_signal(xr->tick);
   slock_unlock(xr->lock);
}

/* The rate the video thread wants, asked once a session and value. */
static void vulkan_openxr_ask_rate(vulkan_openxr_t *xr)
{
   XrResult res;
   float hz;
   int bits = retro_atomic_load_acquire_int(&xr->want_rate);
   memcpy(&hz, &bits, sizeof(hz));
   if (!xr->refresh_ext || hz <= 0.0f || hz == xr->asked_rate)
      return;
   xr->asked_rate = hz;
   res            = xr->RequestDisplayRefreshRateFB(xr->session, hz);
   RARCH_LOG("[OpenXR] Asked the headset for %.2f Hz (%d).\n", hz,
         (int)res);
}

/* One headset frame. xrWaitFrame paces this thread at the headset's
 * rate, and the core too while the headset paces it. */
static void vulkan_openxr_frame(vulkan_openxr_t *xr)
{
   XrResult res;
   int session_state;
   const char *fn = NULL;
   XrFrameWaitInfo wait_info;
   XrFrameState state;
   XrFrameBeginInfo begin_info;
   XrFrameEndInfo end_info;
   XrCompositionLayerQuad layers[VIDEO_XR_MAX_QUADS];
   const XrCompositionLayerBaseHeader *ptrs[VIDEO_XR_MAX_QUADS
      + VULKAN_OPENXR_MAX_EXTRA_LAYERS];

   memset(&wait_info, 0, sizeof(wait_info));
   wait_info.type = XR_TYPE_FRAME_WAIT_INFO;
   memset(&state, 0, sizeof(state));
   state.type     = XR_TYPE_FRAME_STATE;
   if (XR_FAILED(res = xr->WaitFrame(xr->session, &wait_info, &state)))
   {
      vulkan_openxr_frame_error(xr, "xrWaitFrame", res);
      retro_sleep(1);
      return;
   }
   session_state = retro_atomic_load_acquire_int(&xr->state);
   /* A headset that is not worn reports periods that are not its own. */
   if (    (   session_state == XR_SESSION_STATE_VISIBLE
            || session_state == XR_SESSION_STATE_FOCUSED)
         && video_xr_period_add(&xr->period,
            (int64_t)state.predictedDisplayPeriod))
   {
      retro_atomic_store_release_int(&xr->period_ns,
            (int)xr->period.published);
      RARCH_LOG("[OpenXR] The headset runs at %.2f Hz.\n",
            1000000000.0 / (double)xr->period.published);
   }
   vulkan_openxr_tick(xr);
   vulkan_openxr_ask_rate(xr);
   slock_lock(xr->lock);
   xr->predicted_time = state.predictedDisplayTime;
   slock_unlock(xr->lock);
   if (xr->px_per_rad <= 0.0f)
      vulkan_openxr_measure(xr, state.predictedDisplayTime);
   if (retro_atomic_load_acquire_int(&xr->recenter))
   {
      retro_atomic_store_release_int(&xr->recenter, 0);
      vulkan_openxr_recenter(xr, state.predictedDisplayTime);
   }

   memset(&begin_info, 0, sizeof(begin_info));
   begin_info.type               = XR_TYPE_FRAME_BEGIN_INFO;
   memset(&end_info, 0, sizeof(end_info));
   end_info.type                 = XR_TYPE_FRAME_END_INFO;
   end_info.displayTime          = state.predictedDisplayTime;
   end_info.environmentBlendMode = xr->blend_mode;

   slock_lock(xr->queue_lock);
   if (XR_FAILED(res = xr->BeginFrame(xr->session, &begin_info)))
      fn = "xrBeginFrame";
   else
   {
      session_state = retro_atomic_load_acquire_int(&xr->state);
      /* Layers only while the headset shows the session. */
      if (     state.shouldRender
            && (   session_state == XR_SESSION_STATE_VISIBLE
                || session_state == XR_SESSION_STATE_FOCUSED))
      {
         end_info.layerCount = vulkan_openxr_layers(xr, layers, ptrs);
         if (vulkan_openxr_hooks.frame_layers)
            end_info.layerCount += vulkan_openxr_hooks.frame_layers(
                  vulkan_openxr_hooks.user, state.predictedDisplayTime,
                  ptrs + end_info.layerCount,
                  VULKAN_OPENXR_MAX_EXTRA_LAYERS);
      }
      end_info.layers = end_info.layerCount ? ptrs : NULL;
      if (XR_FAILED(res = xr->EndFrame(xr->session, &end_info)))
         fn = "xrEndFrame";
   }
   slock_unlock(xr->queue_lock);
   if (fn)
      vulkan_openxr_frame_error(xr, fn, res);
   else
      xr->frame_failed = false;
}

static void vulkan_openxr_thread(void *data)
{
   vulkan_openxr_t *xr = (vulkan_openxr_t*)data;
   while (!retro_atomic_load_acquire_int(&xr->quit))
   {
      vulkan_openxr_poll(xr);
      if (xr->running)
         vulkan_openxr_frame(xr);
      else
         retro_sleep(10);
   }
}

/* The rates XR_FB_display_refresh_rate lists for this session. */
static void vulkan_openxr_list_rates(vulkan_openxr_t *xr)
{
   char s[256];
   uint32_t i;
   size_t len    = 0;
   uint32_t n    = 0;
   xr->num_rates = 0;
   if (     !xr->refresh_ext
         || XR_FAILED(xr->EnumerateDisplayRefreshRatesFB(xr->session, 0,
               &n, NULL))
         || !n)
      return;
   if (n > VIDEO_HEADSET_MAX_RATES)
   {
      /* The runtime takes no less room than it lists. */
      uint32_t all = n;
      float *tmp   = (float*)malloc(all * sizeof(*tmp));
      if (!tmp)
         return;
      if (XR_FAILED(xr->EnumerateDisplayRefreshRatesFB(xr->session, all,
               &all, tmp)))
      {
         free(tmp);
         return;
      }
      n = (all < VIDEO_HEADSET_MAX_RATES) ? all : VIDEO_HEADSET_MAX_RATES;
      memcpy(xr->rates, tmp, n * sizeof(*tmp));
      free(tmp);
      if (all > n)
         RARCH_WARN("[OpenXR] The headset offers %u rates; %u dropped.\n",
               (unsigned)all, (unsigned)(all - n));
   }
   else if (XR_FAILED(xr->EnumerateDisplayRefreshRatesFB(xr->session, n,
            &n, xr->rates)))
      return;
   xr->num_rates = n;
   s[0]          = '\0';
   for (i = 0; i < n && len < sizeof(s); i++)
      len += snprintf(s + len, sizeof(s) - len, "%s%.2f",
            i ? ", " : "", xr->rates[i]);
   RARCH_LOG("[OpenXR] The headset offers %s Hz.\n", s);
}

static bool vulkan_openxr_create_session(vulkan_openxr_t *xr,
      VkInstance instance, VkPhysicalDevice gpu, VkDevice device,
      uint32_t queue_family)
{
   XrResult res;
   uint32_t count = 0;
   XrGraphicsBindingVulkanKHR binding;
   XrSessionCreateInfo sci;
   XrReferenceSpaceCreateInfo rci;

   if (     !VULKAN_OPENXR_FN(xr, PollEvent)
         || !VULKAN_OPENXR_FN(xr, CreateSession)
         || !VULKAN_OPENXR_FN(xr, DestroySession)
         || !VULKAN_OPENXR_FN(xr, BeginSession)
         || !VULKAN_OPENXR_FN(xr, EndSession)
         || !VULKAN_OPENXR_FN(xr, CreateReferenceSpace)
         || !VULKAN_OPENXR_FN(xr, DestroySpace)
         || !VULKAN_OPENXR_FN(xr, LocateViews)
         || !VULKAN_OPENXR_FN(xr, LocateSpace)
         || !VULKAN_OPENXR_FN(xr, WaitFrame)
         || !VULKAN_OPENXR_FN(xr, BeginFrame)
         || !VULKAN_OPENXR_FN(xr, EndFrame)
         || !VULKAN_OPENXR_FN(xr, EnumerateSwapchainFormats)
         || !VULKAN_OPENXR_FN(xr, CreateSwapchain)
         || !VULKAN_OPENXR_FN(xr, DestroySwapchain)
         || !VULKAN_OPENXR_FN(xr, EnumerateSwapchainImages)
         || !VULKAN_OPENXR_FN(xr, AcquireSwapchainImage)
         || !VULKAN_OPENXR_FN(xr, WaitSwapchainImage)
         || !VULKAN_OPENXR_FN(xr, ReleaseSwapchainImage))
   {
      RARCH_ERR("[OpenXR] The runtime lacks session functions.\n");
      return false;
   }

   memset(&binding, 0, sizeof(binding));
   binding.type             = XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR;
   binding.instance         = instance;
   binding.physicalDevice   = gpu;
   binding.device           = device;
   binding.queueFamilyIndex = queue_family;
   binding.queueIndex       = 0;
   memset(&sci, 0, sizeof(sci));
   sci.type                 = XR_TYPE_SESSION_CREATE_INFO;
   sci.next                 = &binding;
   sci.systemId             = xr->system;
   if (XR_FAILED(res = xr->CreateSession(xr->instance, &sci, &xr->session)))
   {
      RARCH_ERR("[OpenXR] xrCreateSession failed (%d).\n", (int)res);
      xr->session = XR_NULL_HANDLE;
      return false;
   }
   xr->device = device;
   video_xr_pose_identity(&xr->anchor);
   retro_atomic_store_release_int(&xr->recenter, 0);

   memset(&rci, 0, sizeof(rci));
   rci.type                               = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
   rci.referenceSpaceType                 = XR_REFERENCE_SPACE_TYPE_LOCAL;
   rci.poseInReferenceSpace.orientation.w = 1.0f;
   if (XR_FAILED(res = xr->CreateReferenceSpace(xr->session, &rci,
               &xr->local_space)))
   {
      RARCH_ERR("[OpenXR] No LOCAL space (%d).\n", (int)res);
      xr->local_space = XR_NULL_HANDLE;
      goto error;
   }
   rci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
   if (XR_FAILED(res = xr->CreateReferenceSpace(xr->session, &rci,
               &xr->view_space)))
   {
      RARCH_ERR("[OpenXR] No VIEW space (%d).\n", (int)res);
      xr->view_space = XR_NULL_HANDLE;
      goto error;
   }
   if (XR_FAILED(xr->EnumerateSwapchainFormats(xr->session,
               VULKAN_OPENXR_MAX_FORMATS, &count, xr->formats)))
      count = 0;
   xr->num_formats = count;
   vulkan_openxr_list_rates(xr);
   RARCH_LOG("[OpenXR] Session created.\n");

   if (vulkan_openxr_hooks.session_created)
   {
      vulkan_openxr_handles_t h;
      h.xr          = xr;
      h.instance    = xr->instance;
      h.session     = xr->session;
      h.local_space = xr->local_space;
      h.get_proc    = xr->GetInstanceProcAddr;
      h.frame_controller = xr->frame_controller;
      vulkan_openxr_hooks.session_created(vulkan_openxr_hooks.user, &h);
   }
   return true;

error:
   /* Freed here, so session_destroying only follows session_created. */
   if (xr->local_space)
      xr->DestroySpace(xr->local_space);
   xr->local_space = XR_NULL_HANDLE;
   xr->DestroySession(xr->session);
   xr->session     = XR_NULL_HANDLE;
   return false;
}

/* A white dot with a dark rim and a one-pixel soft edge, alpha
 * premultiplied. Grey, so RGBA and BGRA hold the same bytes. */
static void vulkan_openxr_cursor_pixels(uint8_t *px)
{
   unsigned x, y;
   const float c = (VULKAN_OPENXR_CURSOR_DIM - 1) * 0.5f;
   const float r = VULKAN_OPENXR_CURSOR_DIM * 0.5f;
   for (y = 0; y < VULKAN_OPENXR_CURSOR_DIM; y++)
   {
      for (x = 0; x < VULKAN_OPENXR_CURSOR_DIM; x++)
      {
         float dx   = (float)x - c;
         float dy   = (float)y - c;
         float d    = (float)sqrt(dx * dx + dy * dy) / r;
         float a    = (d < 1.0f - 1.0f / r) ? 1.0f
            : ((d < 1.0f) ? (1.0f - d) * r : 0.0f);
         float l    = (d < 0.6f) ? 1.0f : 0.15f;
         uint8_t *p = px + ((size_t)y * VULKAN_OPENXR_CURSOR_DIM + x) * 4;
         p[0]       = (uint8_t)(l * a * 255.0f + 0.5f);
         p[1]       = p[0];
         p[2]       = p[0];
         p[3]       = (uint8_t)(a * 255.0f + 0.5f);
      }
   }
}

/* The dot's swapchain, filled once from a staging buffer on the
 * session's queue. A failure leaves no dot and changes nothing else. */
static void vulkan_openxr_cursor_create(vulkan_openxr_t *xr,
      VkPhysicalDevice gpu, VkDevice device, uint32_t queue_family)
{
   static const VkFormat formats[4] = {
      VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_B8G8R8A8_SRGB,
      VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM };
   unsigned i;
   uint32_t n                 = 0;
   uint32_t index             = 0;
   XrResult res               = XR_SUCCESS;
   VkResult vk_res            = VK_SUCCESS;
   const char *step           = NULL;
   VkFormat format            = VK_FORMAT_UNDEFINED;
   XrSwapchain sc             = XR_NULL_HANDLE;
   VkQueue queue              = VK_NULL_HANDLE;
   VkBuffer buffer            = VK_NULL_HANDLE;
   VkDeviceMemory memory      = VK_NULL_HANDLE;
   VkCommandPool pool         = VK_NULL_HANDLE;
   VkCommandBuffer cmd        = VK_NULL_HANDLE;
   VkFence fence              = VK_NULL_HANDLE;
   void *map                  = NULL;
   VkDeviceSize size          = VULKAN_OPENXR_CURSOR_DIM
      * VULKAN_OPENXR_CURSOR_DIM * 4;
   VkMemoryPropertyFlags host = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
   XrSwapchainCreateInfo ci;
   XrSwapchainImageVulkanKHR imgs[VULKAN_OPENXR_MAX_IMAGES];
   XrSwapchainImageAcquireInfo ai;
   XrSwapchainImageWaitInfo wi;
   XrSwapchainImageReleaseInfo ri;
   VkBufferCreateInfo bi;
   VkMemoryRequirements req;
   VkPhysicalDeviceMemoryProperties props;
   VkMemoryAllocateInfo mi;
   VkCommandPoolCreateInfo pi;
   VkCommandBufferAllocateInfo cai;
   VkCommandBufferBeginInfo cbi;
   VkFenceCreateInfo fi;
   VkImageMemoryBarrier b;
   VkBufferImageCopy region;
   VkSubmitInfo si;

   for (i = 0; i < 4 && format == VK_FORMAT_UNDEFINED; i++)
      if (vulkan_openxr_supports_format(xr, formats[i]))
         format = formats[i];
   if (format == VK_FORMAT_UNDEFINED)
   {
      step = "no 8-bit RGBA format";
      goto end;
   }

   memset(&ci, 0, sizeof(ci));
   ci.type        = XR_TYPE_SWAPCHAIN_CREATE_INFO;
   ci.usageFlags  = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT
      | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
   ci.format      = (int64_t)format;
   ci.sampleCount = 1;
   ci.width       = VULKAN_OPENXR_CURSOR_DIM;
   ci.height      = VULKAN_OPENXR_CURSOR_DIM;
   ci.faceCount   = 1;
   ci.arraySize   = 1;
   ci.mipCount    = 1;
   memset(imgs, 0, sizeof(imgs));
   for (i = 0; i < VULKAN_OPENXR_MAX_IMAGES; i++)
      imgs[i].type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
   memset(&ai, 0, sizeof(ai));
   ai.type        = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO;
   memset(&wi, 0, sizeof(wi));
   wi.type        = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
   wi.timeout     = 100000000; /* 100 ms: a fresh swapchain is free */

   slock_lock(xr->queue_lock);
   res = xr->CreateSwapchain(xr->session, &ci, &sc);
   if (XR_SUCCEEDED(res))
      res = xr->EnumerateSwapchainImages(sc, 0, &n, NULL);
   if (XR_SUCCEEDED(res) && (!n || n > VULKAN_OPENXR_MAX_IMAGES))
      res = XR_ERROR_SIZE_INSUFFICIENT;
   if (XR_SUCCEEDED(res))
      res = xr->EnumerateSwapchainImages(sc, n, &n,
            (XrSwapchainImageBaseHeader*)imgs);
   if (XR_SUCCEEDED(res))
      res = xr->AcquireSwapchainImage(sc, &ai, &index);
   if (XR_SUCCEEDED(res))
      res = xr->WaitSwapchainImage(sc, &wi);
   slock_unlock(xr->queue_lock);
   /* XR_TIMEOUT_EXPIRED succeeds too, but leaves no image. */
   if (res != XR_SUCCESS)
   {
      step = "swapchain";
      goto end;
   }

   vkGetDeviceQueue(device, queue_family, 0, &queue);
   memset(&bi, 0, sizeof(bi));
   bi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   bi.size        = size;
   bi.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
   bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
   if (vkCreateBuffer(device, &bi, NULL, &buffer) != VK_SUCCESS)
   {
      step = "vkCreateBuffer";
      goto end;
   }
   vkGetBufferMemoryRequirements(device, buffer, &req);
   vkGetPhysicalDeviceMemoryProperties(gpu, &props);
   for (i = 0; i < props.memoryTypeCount; i++)
      if (     (req.memoryTypeBits & (1u << i))
            && (props.memoryTypes[i].propertyFlags & host) == host)
         break;
   memset(&mi, 0, sizeof(mi));
   mi.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   mi.allocationSize  = req.size;
   mi.memoryTypeIndex = i;
   if (     i == props.memoryTypeCount
         || vkAllocateMemory(device, &mi, NULL, &memory) != VK_SUCCESS
         || vkBindBufferMemory(device, buffer, memory, 0) != VK_SUCCESS
         || vkMapMemory(device, memory, 0, size, 0, &map) != VK_SUCCESS)
   {
      step = "staging memory";
      goto end;
   }
   vulkan_openxr_cursor_pixels((uint8_t*)map);
   vkUnmapMemory(device, memory);

   memset(&pi, 0, sizeof(pi));
   pi.sType              = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   pi.queueFamilyIndex   = queue_family;
   memset(&cai, 0, sizeof(cai));
   cai.sType             = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   cai.level             = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   cai.commandBufferCount = 1;
   memset(&fi, 0, sizeof(fi));
   fi.sType              = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   if (vkCreateCommandPool(device, &pi, NULL, &pool) != VK_SUCCESS)
   {
      step = "command pool";
      goto end;
   }
   cai.commandPool = pool;
   if (     vkAllocateCommandBuffers(device, &cai, &cmd) != VK_SUCCESS
         || vkCreateFence(device, &fi, NULL, &fence) != VK_SUCCESS)
   {
      step = "command buffer";
      goto end;
   }

   memset(&cbi, 0, sizeof(cbi));
   cbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   cbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
   vkBeginCommandBuffer(cmd, &cbi);
   memset(&b, 0, sizeof(b));
   b.sType                       = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
   b.srcAccessMask               = 0;
   b.dstAccessMask               = VK_ACCESS_TRANSFER_WRITE_BIT;
   b.oldLayout                   = VK_IMAGE_LAYOUT_UNDEFINED;
   b.newLayout                   = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
   b.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
   b.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
   b.image                       = imgs[index].image;
   b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   b.subresourceRange.levelCount = 1;
   b.subresourceRange.layerCount = 1;
   vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &b);
   memset(&region, 0, sizeof(region));
   region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   region.imageSubresource.layerCount = 1;
   region.imageExtent.width           = VULKAN_OPENXR_CURSOR_DIM;
   region.imageExtent.height          = VULKAN_OPENXR_CURSOR_DIM;
   region.imageExtent.depth           = 1;
   vkCmdCopyBufferToImage(cmd, buffer, imgs[index].image,
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
   /* The layout the runtime takes it back in. */
   b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
   b.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
   b.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
   b.newLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
   vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL,
         1, &b);
   vkEndCommandBuffer(cmd);

   memset(&si, 0, sizeof(si));
   si.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   si.commandBufferCount = 1;
   si.pCommandBuffers    = &cmd;
   slock_lock(xr->queue_lock);
   vk_res = vkQueueSubmit(queue, 1, &si, fence);
   slock_unlock(xr->queue_lock);
   if (     vk_res != VK_SUCCESS
         || vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX)
         != VK_SUCCESS)
   {
      step = "submit";
      goto end;
   }

   memset(&ri, 0, sizeof(ri));
   ri.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;
   slock_lock(xr->queue_lock);
   res = xr->ReleaseSwapchainImage(sc, &ri);
   slock_unlock(xr->queue_lock);
   if (XR_FAILED(res))
   {
      step = "xrReleaseSwapchainImage";
      goto end;
   }
   xr->cursor = sc;
   sc         = XR_NULL_HANDLE;
   RARCH_LOG("[OpenXR] Laser cursor ready.\n");

end:
   if (fence)
      vkDestroyFence(device, fence, NULL);
   if (pool)
      vkDestroyCommandPool(device, pool, NULL);
   if (memory)
      vkFreeMemory(device, memory, NULL);
   if (buffer)
      vkDestroyBuffer(device, buffer, NULL);
   if (sc)
   {
      slock_lock(xr->queue_lock);
      xr->DestroySwapchain(sc);
      slock_unlock(xr->queue_lock);
   }
   if (step)
      RARCH_WARN("[OpenXR] No laser cursor (%s, %d).\n", step,
            (res != XR_SUCCESS) ? (int)res : (int)vk_res);
}

bool vulkan_openxr_start(vulkan_openxr_t *xr, VkInstance instance,
      VkPhysicalDevice gpu, VkDevice device, uint32_t queue_family,
      slock_t *queue_lock)
{
   if (!xr->lock && !(xr->lock = slock_new()))
      return false;
   if (!xr->tick && !(xr->tick = scond_new()))
      return false;
   /* A kept device outlives a session the runtime ended: a new one. */
   if (xr->ended)
   {
      vulkan_openxr_destroy_session(xr);
      xr->ended        = false;
      xr->frame_failed = false;
      retro_atomic_store_release_int(&xr->state, XR_SESSION_STATE_UNKNOWN);
   }
   if (     !xr->session
         && !vulkan_openxr_create_session(xr, instance, gpu, device,
            queue_family))
      return false;
   if (xr->device != device)
   {
      RARCH_ERR("[OpenXR] The session belongs to another Vulkan device.\n");
      return false;
   }
   xr->queue_lock = queue_lock;
   if (!xr->cursor)
      vulkan_openxr_cursor_create(xr, gpu, device, queue_family);
   video_xr_period_init(&xr->period);
   retro_atomic_store_release_int(&xr->period_ns, 0);
   xr->tick_count    = 0;
   xr->tick_interval = 0;
   xr->asked_rate    = 0.0f;
   retro_atomic_store_release_int(&xr->quit, 0);
   retro_atomic_store_release_int(&xr->alive, 1);
   if (!(xr->thread = sthread_create(vulkan_openxr_thread, xr)))
   {
      retro_atomic_store_release_int(&xr->alive, 0);
      return false;
   }
   return true;
}

void vulkan_openxr_stop_thread(vulkan_openxr_t *xr)
{
   if (!xr || !xr->thread)
      return;
   retro_atomic_store_release_int(&xr->quit, 1);
   sthread_join(xr->thread);
   xr->thread = NULL;
   /* Nothing shows the session's frames now: a driver that presents on
    * (a staged content load keeps it up) draws none for it. */
   retro_atomic_store_release_int(&xr->alive, 0);
}

void vulkan_openxr_stop(vulkan_openxr_t *xr)
{
   unsigned s;
   if (!xr)
      return;
   vulkan_openxr_stop_thread(xr);
   /* A kept session outlives the driver's views of its images. */
   for (s = 0; s < VIDEO_XR_MAX_SLOTS; s++)
      vulkan_openxr_slot_destroy(xr, s);
   if (xr->cursor)
   {
      slock_lock(xr->queue_lock);
      xr->DestroySwapchain(xr->cursor);
      slock_unlock(xr->queue_lock);
      xr->cursor = XR_NULL_HANDLE;
   }
   if (xr->lock)
   {
      slock_lock(xr->lock);
      xr->quads.num_quads = 0;
      slock_unlock(xr->lock);
   }
   /* The context frees it next; a kept session outlives it. */
   xr->queue_lock = NULL;
}

void vulkan_openxr_drop_and_reinit(vulkan_openxr_t *xr)
{
   RARCH_ERR("[OpenXR] Rebuilding video without headset output.\n");
   vulkan_openxr_notify(MSG_OPENXR_FAILED);
   vulkan_openxr_free(xr);
   vulkan_openxr_skip_once = true;
}

bool vulkan_openxr_lost(const vulkan_openxr_t *xr)
{
   return xr->lost;
}

bool vulkan_openxr_alive(vulkan_openxr_t *xr)
{
   return retro_atomic_load_acquire_int(&xr->alive) != 0;
}

bool vulkan_openxr_focused(vulkan_openxr_t *xr)
{
   return retro_atomic_load_acquire_int(&xr->alive)
      && retro_atomic_load_acquire_int(&xr->state) == XR_SESSION_STATE_FOCUSED;
}

XrTime vulkan_openxr_predicted_time(vulkan_openxr_t *xr)
{
   XrTime t;
   slock_lock(xr->lock);
   t = xr->predicted_time;
   slock_unlock(xr->lock);
   return t;
}

bool vulkan_openxr_should_draw(vulkan_openxr_t *xr)
{
   int state = retro_atomic_load_acquire_int(&xr->state);
   return retro_atomic_load_acquire_int(&xr->alive)
      && (   state == XR_SESSION_STATE_VISIBLE
          || state == XR_SESSION_STATE_FOCUSED);
}

float vulkan_openxr_pixels_per_radian(vulkan_openxr_t *xr)
{
   float v;
   slock_lock(xr->lock);
   v = xr->px_per_rad;
   slock_unlock(xr->lock);
   return v;
}

float vulkan_openxr_refresh_rate(vulkan_openxr_t *xr)
{
   int ns = retro_atomic_load_acquire_int(&xr->period_ns);
   return (ns > 0) ? (float)(1000000000.0 / (double)ns) : 0.0f;
}

void vulkan_openxr_set_pacing(vulkan_openxr_t *xr, unsigned interval)
{
   retro_atomic_store_release_int(&xr->interval, (int)interval);
   if (!interval)
      xr->pace_mode = 0;
}

/* The next tick after the last one seen, or false after timeout_ns. */
static bool vulkan_openxr_wait_tick(vulkan_openxr_t *xr,
      int64_t timeout_ns)
{
   bool ticked;
   retro_time_t deadline = cpu_features_get_time_usec()
      + (retro_time_t)(timeout_ns / 1000);
   slock_lock(xr->lock);
   while (xr->tick_seq == xr->tick_seen && vulkan_openxr_should_draw(xr))
   {
      retro_time_t left = deadline - cpu_features_get_time_usec();
      if (left <= 0 || !scond_wait_timeout(xr->tick, xr->lock, left))
         break;
   }
   ticked        = xr->tick_seq != xr->tick_seen;
   xr->tick_seen = xr->tick_seq;
   slock_unlock(xr->lock);
   return ticked;
}

void vulkan_openxr_pace_skip(vulkan_openxr_t *xr)
{
   if (!xr->tick)
      return;
   slock_lock(xr->lock);
   xr->tick_seen = xr->tick_seq;
   slock_unlock(xr->lock);
}

void vulkan_openxr_pace_wait(vulkan_openxr_t *xr)
{
   int64_t period = (int64_t)retro_atomic_load_acquire_int(&xr->period_ns)
      * retro_atomic_load_acquire_int(&xr->interval);
   if (period <= 0 || !xr->tick)
      return;
   if (vulkan_openxr_should_draw(xr))
   {
      if (xr->pace_mode != 1)
         RARCH_LOG("[OpenXR] Pacing on the headset's frames.\n");
      xr->pace_mode = 1;
      if (vulkan_openxr_wait_tick(xr, period * 2))
         xr->tick_late = false;
      else if (vulkan_openxr_should_draw(xr) && !xr->tick_late)
      {
         xr->tick_late = true;
         RARCH_WARN("[OpenXR] No headset frame for two intervals; the core carries on.\n");
      }
      xr->pace_anchor_ns = (int64_t)cpu_features_get_time_usec() * 1000;
   }
   else
   {
      /* No ticks to wait on: keep the core's rate on the clock, from
       * the last tick on. */
      retro_time_t sleep_us;
      if (xr->pace_mode != 2)
         RARCH_LOG("[OpenXR] Pacing on the clock while the headset does not show the session.\n");
      xr->pace_mode = 2;
      vulkan_openxr_pace_skip(xr);
      sleep_us      = runloop_pace_schedule(&xr->pace_anchor_ns, period,
            cpu_features_get_time_usec());
      if (sleep_us > 0)
         retro_sleep_us((unsigned)sleep_us);
   }
}

unsigned vulkan_openxr_refresh_rates(const vulkan_openxr_t *xr,
      float *rates, unsigned cap)
{
   unsigned i;
   for (i = 0; i < xr->num_rates && i < cap; i++)
      rates[i] = xr->rates[i];
   return i;
}

void vulkan_openxr_request_rate(vulkan_openxr_t *xr, float hz)
{
   int bits;
   memcpy(&bits, &hz, sizeof(bits));
   retro_atomic_store_release_int(&xr->want_rate, bits);
}

unsigned vulkan_openxr_max_dim(const vulkan_openxr_t *xr)
{
   return xr->max_dim;
}

bool vulkan_openxr_supports_format(const vulkan_openxr_t *xr,
      VkFormat format)
{
   uint32_t i;
   for (i = 0; i < xr->num_formats; i++)
      if (xr->formats[i] == (int64_t)format)
         return true;
   return false;
}

void vulkan_openxr_slot_destroy(vulkan_openxr_t *xr, unsigned slot)
{
   unsigned l;
   XrSwapchain sc[2];
   struct vulkan_openxr_slot *s = &xr->slots[slot];
   if (!s->swapchains[0])
      return;
   slock_lock(xr->queue_lock);
   slock_lock(xr->lock);
   memcpy(sc, s->swapchains, sizeof(sc));
   memset(s->swapchains, 0, sizeof(s->swapchains));
   memset(s->acquired, 0, sizeof(s->acquired));
   memset(s->waited, 0, sizeof(s->waited));
   s->dims      = 0;
   s->layers    = 0;
   retro_atomic_store_release_int(&s->content, 0);
   slock_unlock(xr->lock);
   for (l = 0; l < 2; l++)
      if (sc[l])
         xr->DestroySwapchain(sc[l]);
   slock_unlock(xr->queue_lock);
}

bool vulkan_openxr_slot_create(vulkan_openxr_t *xr, unsigned slot,
      VkFormat format, bool mutable_format, unsigned dims, unsigned layers,
      VkImage (*images)[2], unsigned *num_images)
{
   uint32_t i, l;
   uint32_t n   = 0;
   XrResult res = XR_SUCCESS;
   XrSwapchainCreateInfo ci;
   XrSwapchainImageVulkanKHR imgs[VULKAN_OPENXR_MAX_IMAGES];
   XrSwapchain sc[2];
   struct vulkan_openxr_slot *s = &xr->slots[slot];

   vulkan_openxr_slot_destroy(xr, slot);
   memset(&ci, 0, sizeof(ci));
   ci.type        = XR_TYPE_SWAPCHAIN_CREATE_INFO;
   ci.usageFlags  = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT
      | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
   /* The chains' final passes write through a view in the window's
    * format. */
   if (mutable_format)
      ci.usageFlags |= XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT;
   ci.format      = (int64_t)format;
   ci.sampleCount = 1;
   ci.width       = VIDEO_SCALE_W(dims);
   ci.height      = VIDEO_SCALE_H(dims);
   ci.faceCount   = 1;
   ci.arraySize   = 1;
   ci.mipCount    = 1;
   memset(imgs, 0, sizeof(imgs));
   for (i = 0; i < VULKAN_OPENXR_MAX_IMAGES; i++)
      imgs[i].type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
   sc[0] = XR_NULL_HANDLE;
   sc[1] = XR_NULL_HANDLE;

   slock_lock(xr->queue_lock);
   for (l = 0; l < layers && XR_SUCCEEDED(res); l++)
   {
      uint32_t count = 0;
      res = xr->CreateSwapchain(xr->session, &ci, &sc[l]);
      if (XR_SUCCEEDED(res))
         res = xr->EnumerateSwapchainImages(sc[l], 0, &count, NULL);
      /* The driver keeps one image count per slot. */
      if (     XR_SUCCEEDED(res)
            && (  !count || count > VULKAN_OPENXR_MAX_IMAGES
               || (l && count != n)))
         res = XR_ERROR_SIZE_INSUFFICIENT;
      if (XR_SUCCEEDED(res))
         res = xr->EnumerateSwapchainImages(sc[l], count, &count,
               (XrSwapchainImageBaseHeader*)imgs);
      for (i = 0; XR_SUCCEEDED(res) && i < count; i++)
         images[i][l] = imgs[i].image;
      n = count;
   }
   if (XR_SUCCEEDED(res))
   {
      slock_lock(xr->lock);
      memcpy(s->swapchains, sc, sizeof(sc));
      memset(s->acquired, 0, sizeof(s->acquired));
      memset(s->waited, 0, sizeof(s->waited));
      s->dims      = dims;
      s->layers    = layers;
      retro_atomic_store_release_int(&s->content, 0);
      slock_unlock(xr->lock);
   }
   else
      for (l = 0; l < 2; l++)
         if (sc[l] != XR_NULL_HANDLE)
            xr->DestroySwapchain(sc[l]);
   slock_unlock(xr->queue_lock);

   if (XR_FAILED(res))
   {
      RARCH_ERR("[OpenXR] No %ux%u swapchain with %u layer(s) (%d).\n",
            VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims), layers, (int)res);
      return false;
   }
   *num_images = n;
   RARCH_LOG("[OpenXR] Slot %u: %ux%u, %u layer(s), %u images.\n", slot,
         VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims), layers, (unsigned)n);
   return true;
}

bool vulkan_openxr_slot_acquire(vulkan_openxr_t *xr, unsigned slot,
      unsigned *index)
{
   unsigned l;
   struct vulkan_openxr_slot *s = &xr->slots[slot];
   if (!s->swapchains[0])
      return false;
   for (l = 0; l < s->layers; l++)
   {
      if (!s->acquired[l])
      {
         XrSwapchainImageAcquireInfo ai;
         memset(&ai, 0, sizeof(ai));
         ai.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO;
         if (XR_FAILED(xr->AcquireSwapchainImage(s->swapchains[l], &ai,
                     &s->index[l])))
            return false;
         s->acquired[l] = true;
         s->waited[l]   = false;
      }
      if (!s->waited[l])
      {
         XrSwapchainImageWaitInfo wi;
         memset(&wi, 0, sizeof(wi));
         wi.type    = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
         /* Never stall the core, nor the XR thread behind the queue lock:
          * an image the compositor still reads is tried again next frame. */
         wi.timeout = 0;
         if (xr->WaitSwapchainImage(s->swapchains[l], &wi) != XR_SUCCESS)
            return false;
         s->waited[l] = true;
      }
      index[l] = s->index[l];
   }
   return true;
}

void vulkan_openxr_slot_release(vulkan_openxr_t *xr, unsigned slot)
{
   unsigned l;
   bool released = true;
   XrSwapchainImageReleaseInfo ri;
   struct vulkan_openxr_slot *s = &xr->slots[slot];
   if (!s->swapchains[0])
      return;
   for (l = 0; l < s->layers; l++)
      if (!s->acquired[l] || !s->waited[l])
         return;
   memset(&ri, 0, sizeof(ri));
   ri.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;
   for (l = 0; l < s->layers; l++)
   {
      if (XR_FAILED(xr->ReleaseSwapchainImage(s->swapchains[l], &ri)))
         released = false;
      s->acquired[l] = false;
      s->waited[l]   = false;
   }
   if (released)
      retro_atomic_store_release_int(&s->content, 1);
}

void vulkan_openxr_slot_forget(vulkan_openxr_t *xr, unsigned slot)
{
   retro_atomic_store_release_int(&xr->slots[slot].content, 0);
}

void vulkan_openxr_publish(vulkan_openxr_t *xr,
      const video_xr_quad_set_t *set)
{
   slock_lock(xr->lock);
   xr->quads = *set;
   slock_unlock(xr->lock);
}

void vulkan_openxr_get_anchor(vulkan_openxr_t *xr, video_xr_pose_t *anchor)
{
   slock_lock(xr->lock);
   *anchor = xr->anchor;
   slock_unlock(xr->lock);
}

bool vulkan_openxr_get_quads(vulkan_openxr_t *xr, video_xr_quad_set_t *out)
{
   if (!xr || !xr->lock)
      return false;
   slock_lock(xr->lock);
   *out = xr->quads;
   slock_unlock(xr->lock);
   return true;
}

void vulkan_openxr_request_recenter(vulkan_openxr_t *xr)
{
   retro_atomic_store_release_int(&xr->recenter, 1);
}

XrSwapchain vulkan_openxr_cursor(const vulkan_openxr_t *xr)
{
   return xr->cursor;
}
