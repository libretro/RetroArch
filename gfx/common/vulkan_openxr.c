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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "vksym.h"
#include "vulkan_openxr.h"
#include "openxr_runtime.h"
#include "openxr_session.h"
#include "openxr_swapchain.h"
#include "openxr_frame.h"
#include "openxr_host.h"
#include "../video_driver.h"

#include "../../msg_hash.h"
#include "../../verbosity.h"

#define VULKAN_OPENXR_EXT_BUF 1024

struct vulkan_openxr
{
   openxr_runtime_t rt;
   bool enable2;
   /* The session is made on the thread that makes the device; its frame
    * loop runs on the XR thread or in-line. queue_lock guards what both
    * read. */
   openxr_session_t session;
   openxr_swapchains_t sc;
   openxr_frame_t frame;
   openxr_host_t host;
   /* The driver shows the headset nothing (HDR output). */
   retro_atomic_int_t hidden;
   VkDevice device;
   slock_t *queue_lock;
   /* A headset's own context: its hooks, and the frame it began. */
   vulkan_openxr_platform_t platform;
   openxr_frame_begun_t begun;
   bool inline_host;

   /* XR_KHR_vulkan_enable's lists, split in place. */
   char inst_ext_buf[VULKAN_OPENXR_EXT_BUF];
   char dev_ext_buf[VULKAN_OPENXR_EXT_BUF];
   const char *inst_exts[VULKAN_OPENXR_MAX_EXTS];
   const char *dev_exts[VULKAN_OPENXR_MAX_EXTS];
   unsigned num_inst_exts;
   unsigned num_dev_exts;

   PFN_xrGetVulkanGraphicsRequirements2KHR GetVulkanGraphicsRequirements2KHR;
   PFN_xrCreateVulkanInstanceKHR CreateVulkanInstanceKHR;
   PFN_xrCreateVulkanDeviceKHR CreateVulkanDeviceKHR;
   PFN_xrGetVulkanGraphicsDevice2KHR GetVulkanGraphicsDevice2KHR;
   PFN_xrGetVulkanGraphicsRequirementsKHR GetVulkanGraphicsRequirementsKHR;
   PFN_xrGetVulkanGraphicsDeviceKHR GetVulkanGraphicsDeviceKHR;
   PFN_xrGetVulkanInstanceExtensionsKHR GetVulkanInstanceExtensionsKHR;
   PFN_xrGetVulkanDeviceExtensionsKHR GetVulkanDeviceExtensionsKHR;
};

static void vulkan_openxr_lock_queue(void *user)
{
   slock_lock(((vulkan_openxr_t*)user)->queue_lock);
}

static void vulkan_openxr_unlock_queue(void *user)
{
   slock_unlock(((vulkan_openxr_t*)user)->queue_lock);
}

static void vulkan_openxr_state_hook(void *user, XrSessionState state)
{
   if (state == XR_SESSION_STATE_STOPPING)
      openxr_frame_tick_notify(&((vulkan_openxr_t*)user)->frame);
}

static void vulkan_openxr_ended_hook(void *user)
{
   vulkan_openxr_t *xr = (vulkan_openxr_t*)user;
   openxr_frame_tick_notify(&xr->frame);
   /* A headset's own context has nothing to fall back to. */
   if (xr->inline_host)
   {
      if (xr->platform.ended)
         xr->platform.ended(xr->platform.user);
      return;
   }
   RARCH_WARN("[OpenXR] The headset session ended; the window keeps the output.\n");
   openxr_runtime_notify(MSG_OPENXR_SESSION_ENDED);
}

static void vulkan_openxr_exiting_hook(void *user)
{
   if (!((vulkan_openxr_t*)user)->inline_host)
      video_driver_headset_exit_request();
}

static void vulkan_openxr_local_hook(void *user)
{
   openxr_frame_local_changed(&((vulkan_openxr_t*)user)->frame);
}

static void vulkan_openxr_stage_hook(void *user)
{
   openxr_host_stage_changed(&((vulkan_openxr_t*)user)->host);
}

static unsigned vulkan_openxr_host_state(void *user)
{
   vulkan_openxr_t *xr = (vulkan_openxr_t*)user;
   unsigned bits       = 0;
   bool shown          = !retro_atomic_load_acquire_int(&xr->hidden);
   if (shown && openxr_session_alive(&xr->session))
      bits |= VIDEO_XR_HEADSET_PRESENTING;
   if (openxr_session_focused(&xr->session))
      bits |= VIDEO_XR_HEADSET_FOCUSED;
   /* The driver draws a tracked map's eyes as the projection. */
   if (     shown && openxr_session_alive(&xr->session)
         && openxr_session_running(&xr->session))
      bits |= VIDEO_XR_HEADSET_TRACKING;
   return bits;
}

static bool vulkan_openxr_host_sample(void *user, unsigned ahead,
      uint32_t *frame_flags)
{
   return openxr_host_sample(&((vulkan_openxr_t*)user)->host, ahead, frame_flags);
}

static bool vulkan_openxr_host_eye_state(void *user,
      struct retro_vr_eye_state out[2], unsigned *token)
{
   return openxr_host_eye_state(&((vulkan_openxr_t*)user)->host, out, token);
}

static void vulkan_openxr_host_give(void *user, unsigned token)
{
   openxr_host_give(&((vulkan_openxr_t*)user)->host, token);
}

static bool vulkan_openxr_host_head_pose(void *user,
      struct retro_vr_head_pose *out)
{
   return openxr_host_head_pose(&((vulkan_openxr_t*)user)->host, out);
}

static bool vulkan_openxr_host_locate_hands(void *user)
{
   return openxr_host_locate_hands(&((vulkan_openxr_t*)user)->host);
}

static bool vulkan_openxr_host_controller_poses(void *user,
      struct retro_vr_controller_poses *out)
{
   return openxr_host_controller_poses(&((vulkan_openxr_t*)user)->host, out);
}

static bool vulkan_openxr_host_eye_size(void *user, unsigned *w,
      unsigned *h)
{
   return openxr_host_eye_size(&((vulkan_openxr_t*)user)->host, w, h);
}

static bool vulkan_openxr_host_set_space(void *user,
      enum retro_vr_reference_space *space, unsigned *w, unsigned *h)
{
   return openxr_host_set_space(&((vulkan_openxr_t*)user)->host, space,
         w, h);
}

#define VULKAN_OPENXR_FN(xr, name) OPENXR_FN(&(xr)->rt, (xr), name)

/* Set after a session fails on a device made for it: the reinit that
 * follows builds the device without the runtime. */
static bool vulkan_openxr_skip_once;

static void vulkan_openxr_host_resume(void *user)
{
   openxr_host_resume(&((vulkan_openxr_t*)user)->host);
}

static vulkan_openxr_platform_t vulkan_openxr_platform;
static bool vulkan_openxr_has_platform;

void vulkan_openxr_set_platform(const vulkan_openxr_platform_t *p)
{
   if (p)
      vulkan_openxr_platform = *p;
   else
      memset(&vulkan_openxr_platform, 0, sizeof(vulkan_openxr_platform));
   vulkan_openxr_has_platform = (p != NULL);
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
   if (     XR_FAILED(get(xr->rt.instance, xr->rt.system, 0, &len, NULL))
         || len > VULKAN_OPENXR_EXT_BUF)
      return false;
   if (!len)
      return true;
   if (XR_FAILED(get(xr->rt.instance, xr->rt.system, len, &len, buf)))
      return false;
   buf[VULKAN_OPENXR_EXT_BUF - 1] = '\0';
   return vulkan_openxr_split(buf, names, count);
}

vulkan_openxr_t *vulkan_openxr_new(bool enable1, uint32_t api_version)
{
   XrResult res;
   XrVersion api;
   XrGraphicsRequirementsVulkanKHR reqs;
   enum openxr_runtime_result rr;
   const char *ext;
   vulkan_openxr_t *xr;

   if (vulkan_openxr_skip_once)
   {
      vulkan_openxr_skip_once = false;
      openxr_host_forget();
      RARCH_LOG("[OpenXR] Starting once without headset output after a failure.\n");
      return NULL;
   }
   if (!(xr = (vulkan_openxr_t*)calloc(1, sizeof(*xr))))
   {
      openxr_host_forget();
      return NULL;
   }
   xr->session.hooks.state_changed = vulkan_openxr_state_hook;
   xr->session.hooks.ended         = vulkan_openxr_ended_hook;
   xr->session.hooks.exiting       = vulkan_openxr_exiting_hook;
   xr->session.hooks.local_changed = vulkan_openxr_local_hook;
   xr->session.hooks.stage_changed = vulkan_openxr_stage_hook;
   xr->session.hooks.lock          = vulkan_openxr_lock_queue;
   xr->session.hooks.unlock        = vulkan_openxr_unlock_queue;
   xr->session.hooks.user          = xr;
   if (vulkan_openxr_has_platform)
   {
      xr->platform    = vulkan_openxr_platform;
      xr->rt.platform = vulkan_openxr_platform.runtime;
      xr->inline_host = true;
   }
   openxr_host_init(&xr->host, &xr->rt, &xr->session, &xr->frame);
   xr->enable2 = !enable1;
   ext         = enable1 ? XR_KHR_VULKAN_ENABLE_EXTENSION_NAME
                         : XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME;

   if ((rr = openxr_runtime_create_instance(&xr->rt, xrGetInstanceProcAddr,
               ext)) != OPENXR_RUNTIME_OK)
      goto end;
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
   if ((rr = openxr_runtime_find_system(&xr->rt)) != OPENXR_RUNTIME_OK)
      goto end;

   /* The runtime must be asked before any Vulkan object is made. */
   reqs.type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR;
   reqs.next = NULL;
   res       = enable1
      ? xr->GetVulkanGraphicsRequirementsKHR(xr->rt.instance, xr->rt.system, &reqs)
      : xr->GetVulkanGraphicsRequirements2KHR(xr->rt.instance, xr->rt.system, &reqs);
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

missing:
   RARCH_ERR("[OpenXR] The runtime lacks functions headset output needs.\n");
   rr = OPENXR_RUNTIME_FAILED;
   goto end;
failed:
   rr = OPENXR_RUNTIME_FAILED;
end:
   openxr_runtime_notify((rr == OPENXR_RUNTIME_UNAVAILABLE)
         ? MSG_OPENXR_UNAVAILABLE : MSG_OPENXR_FAILED);
   vulkan_openxr_free(xr);
   openxr_host_forget();
   return NULL;
}

/* After vulkan_openxr_stop(). */
static void vulkan_openxr_destroy_session(vulkan_openxr_t *xr)
{
   if (xr->session.session)
      openxr_frame_input_detach(&xr->frame);
   openxr_session_destroy(&xr->session);
}

void vulkan_openxr_free(vulkan_openxr_t *xr)
{
   if (!xr)
      return;
   vulkan_openxr_stop(xr);
   vulkan_openxr_destroy_session(xr);
   openxr_runtime_deinit(&xr->rt);
   openxr_frame_deinit(&xr->frame);
   free(xr);
}

void vulkan_openxr_drop(vulkan_openxr_t *xr)
{
   RARCH_WARN("[OpenXR] Continuing without headset output.\n");
   openxr_runtime_notify(MSG_OPENXR_FAILED);
   vulkan_openxr_free(xr);
   openxr_host_forget();
}

void vulkan_openxr_needs_reload(void)
{
   RARCH_WARN("[OpenXR] The content keeps its Vulkan context; headset output starts when it is loaded again.\n");
   openxr_runtime_notify(MSG_OPENXR_NEEDS_RELOAD);
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

   ci.type                   = XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR;
   ci.next                   = NULL;
   ci.createFlags            = 0;
   ci.vulkanAllocator        = NULL;
   ci.systemId               = xr->rt.system;
   ci.pfnGetInstanceProcAddr = gipa;
   ci.vulkanCreateInfo       = info;
   res = xr->CreateVulkanInstanceKHR(xr->rt.instance, &ci, instance, &vk_res);
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

   ci.type                   = XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR;
   ci.next                   = NULL;
   ci.createFlags            = 0;
   ci.vulkanAllocator        = NULL;
   ci.systemId               = xr->rt.system;
   ci.pfnGetInstanceProcAddr = gipa;
   ci.vulkanPhysicalDevice   = gpu;
   ci.vulkanCreateInfo       = info;
   res = xr->CreateVulkanDeviceKHR(xr->rt.instance, &ci, device, &vk_res);
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
      gi.type           = XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR;
      gi.next           = NULL;
      gi.systemId       = xr->rt.system;
      gi.vulkanInstance = instance;
      res = xr->GetVulkanGraphicsDevice2KHR(xr->rt.instance, &gi, &gpu);
   }
   else
      res = xr->GetVulkanGraphicsDeviceKHR(xr->rt.instance, xr->rt.system,
            instance, &gpu);
   if (XR_FAILED(res) || gpu == VK_NULL_HANDLE)
   {
      RARCH_WARN("[OpenXR] The runtime named no GPU (%d).\n", (int)res);
      return VK_NULL_HANDLE;
   }
   return gpu;
}

static bool vulkan_openxr_create_session(vulkan_openxr_t *xr,
      VkInstance instance, VkPhysicalDevice gpu, VkDevice device,
      uint32_t queue_family)
{
   XrGraphicsBindingVulkanKHR binding;

   if (     !openxr_session_load(&xr->session, &xr->rt)
         || !openxr_frame_load(&xr->frame, &xr->rt, &xr->session, &xr->sc)
         || !openxr_swapchains_load(&xr->sc, &xr->rt, &xr->session))
   {
      RARCH_ERR("[OpenXR] The runtime lacks session functions.\n");
      return false;
   }

   binding.type             = XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR;
   binding.next             = NULL;
   binding.instance         = instance;
   binding.physicalDevice   = gpu;
   binding.device           = device;
   binding.queueFamilyIndex = queue_family;
   binding.queueIndex       = 0;
   if (!openxr_session_create(&xr->session, &binding))
      return false;
   xr->device = device;
   openxr_swapchains_list_formats(&xr->sc);
   openxr_frame_session_created(&xr->frame);
   openxr_host_session_created(&xr->host);
   RARCH_LOG("[OpenXR] Session created.\n");

   openxr_frame_input_attach(&xr->frame);
   return true;
}

struct vulkan_openxr_images
{
   vulkan_openxr_t *xr;
   VkImage (*images)[2];
};

static XrResult vulkan_openxr_list_images(void *user, XrSwapchain swapchain,
      unsigned layer, uint32_t *count)
{
   uint32_t i;
   XrResult res;
   XrSwapchainImageVulkanKHR imgs[OPENXR_MAX_IMAGES];
   struct vulkan_openxr_images *li = (struct vulkan_openxr_images*)user;
   for (i = 0; i < OPENXR_MAX_IMAGES; i++)
   {
      imgs[i].type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
      imgs[i].next = NULL;
   }
   res = li->xr->sc.EnumerateSwapchainImages(swapchain, *count, count,
         (XrSwapchainImageBaseHeader*)imgs);
   for (i = 0; XR_SUCCEEDED(res) && i < *count; i++)
      li->images[i][layer] = imgs[i].image;
   return res;
}

struct vulkan_openxr_dot
{
   vulkan_openxr_t *xr;
   VkPhysicalDevice gpu;
   VkDevice device;
   uint32_t queue_family;
   VkImage images[OPENXR_MAX_IMAGES][2];
};

/* The dot's image from a staging buffer, on the session's queue. */
static bool vulkan_openxr_cursor_fill(void *user, uint32_t index,
      const char **step, int *code)
{
   unsigned i;
   struct vulkan_openxr_dot *dot = (struct vulkan_openxr_dot*)user;
   VkDevice device            = dot->device;
   VkResult vk_res            = VK_SUCCESS;
   const char *failed         = NULL;
   VkQueue queue              = VK_NULL_HANDLE;
   VkBuffer buffer            = VK_NULL_HANDLE;
   VkDeviceMemory memory      = VK_NULL_HANDLE;
   VkCommandPool pool         = VK_NULL_HANDLE;
   VkCommandBuffer cmd        = VK_NULL_HANDLE;
   VkFence fence              = VK_NULL_HANDLE;
   void *map                  = NULL;
   VkDeviceSize size          = VIDEO_XR_CURSOR_DIM
      * VIDEO_XR_CURSOR_DIM * 4;
   VkMemoryPropertyFlags host = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
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

   vkGetDeviceQueue(device, dot->queue_family, 0, &queue);
   bi.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   bi.pNext                 = NULL;
   bi.flags                 = 0;
   bi.queueFamilyIndexCount = 0;
   bi.pQueueFamilyIndices   = NULL;
   bi.size                  = size;
   bi.usage                 = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
   bi.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
   if (vkCreateBuffer(device, &bi, NULL, &buffer) != VK_SUCCESS)
   {
      failed = "vkCreateBuffer";
      goto end;
   }
   vkGetBufferMemoryRequirements(device, buffer, &req);
   vkGetPhysicalDeviceMemoryProperties(dot->gpu, &props);
   for (i = 0; i < props.memoryTypeCount; i++)
      if (     (req.memoryTypeBits & (1u << i))
            && (props.memoryTypes[i].propertyFlags & host) == host)
         break;
   mi.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   mi.pNext           = NULL;
   mi.allocationSize  = req.size;
   mi.memoryTypeIndex = i;
   if (     i == props.memoryTypeCount
         || vkAllocateMemory(device, &mi, NULL, &memory) != VK_SUCCESS
         || vkBindBufferMemory(device, buffer, memory, 0) != VK_SUCCESS
         || vkMapMemory(device, memory, 0, size, 0, &map) != VK_SUCCESS)
   {
      failed = "staging memory";
      goto end;
   }
   video_xr_cursor_pixels((uint8_t*)map);
   vkUnmapMemory(device, memory);

   pi.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   pi.pNext            = NULL;
   pi.flags            = 0;
   pi.queueFamilyIndex = dot->queue_family;
   cai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   cai.pNext              = NULL;
   cai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   cai.commandBufferCount = 1;
   fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   fi.pNext = NULL;
   fi.flags = 0;
   if (vkCreateCommandPool(device, &pi, NULL, &pool) != VK_SUCCESS)
   {
      failed = "command pool";
      goto end;
   }
   cai.commandPool = pool;
   if (     vkAllocateCommandBuffers(device, &cai, &cmd) != VK_SUCCESS
         || vkCreateFence(device, &fi, NULL, &fence) != VK_SUCCESS)
   {
      failed = "command buffer";
      goto end;
   }

   cbi.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   cbi.pNext            = NULL;
   cbi.pInheritanceInfo = NULL;
   cbi.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
   vkBeginCommandBuffer(cmd, &cbi);
   b.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
   b.pNext                           = NULL;
   b.subresourceRange.baseMipLevel   = 0;
   b.subresourceRange.baseArrayLayer = 0;
   b.srcAccessMask                   = 0;
   b.dstAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
   b.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
   b.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
   b.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
   b.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
   b.image                           = dot->images[index][0];
   b.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
   b.subresourceRange.levelCount     = 1;
   b.subresourceRange.layerCount     = 1;
   vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &b);
   region.bufferOffset                    = 0;
   region.bufferRowLength                 = 0;
   region.bufferImageHeight               = 0;
   region.imageSubresource.mipLevel       = 0;
   region.imageSubresource.baseArrayLayer = 0;
   region.imageOffset.x                   = 0;
   region.imageOffset.y                   = 0;
   region.imageOffset.z                   = 0;
   region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
   region.imageSubresource.layerCount     = 1;
   region.imageExtent.width               = VIDEO_XR_CURSOR_DIM;
   region.imageExtent.height              = VIDEO_XR_CURSOR_DIM;
   region.imageExtent.depth               = 1;
   vkCmdCopyBufferToImage(cmd, buffer, dot->images[index][0],
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

   si.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   si.pNext                = NULL;
   si.waitSemaphoreCount   = 0;
   si.pWaitSemaphores      = NULL;
   si.pWaitDstStageMask    = NULL;
   si.signalSemaphoreCount = 0;
   si.pSignalSemaphores    = NULL;
   si.commandBufferCount   = 1;
   si.pCommandBuffers      = &cmd;
   slock_lock(dot->xr->queue_lock);
   vk_res = vkQueueSubmit(queue, 1, &si, fence);
   slock_unlock(dot->xr->queue_lock);
   if (     vk_res != VK_SUCCESS
         || vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX)
         != VK_SUCCESS)
   {
      failed = "submit";
      goto end;
   }

end:
   if (fence)
      vkDestroyFence(device, fence, NULL);
   if (pool)
      vkDestroyCommandPool(device, pool, NULL);
   if (memory)
      vkFreeMemory(device, memory, NULL);
   if (buffer)
      vkDestroyBuffer(device, buffer, NULL);
   if (!failed)
      return true;
   *step = failed;
   *code = (int)vk_res;
   return false;
}

/* The laser's dot. A failure leaves none and changes nothing else. */
static void vulkan_openxr_cursor_create(vulkan_openxr_t *xr,
      VkPhysicalDevice gpu, VkDevice device, uint32_t queue_family)
{
   static const int64_t formats[4] = {
      VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_B8G8R8A8_SRGB,
      VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM };
   struct vulkan_openxr_images li;
   struct vulkan_openxr_dot dot;
   const char *step = NULL;
   int code         = 0;

   dot.xr           = xr;
   dot.gpu          = gpu;
   dot.device       = device;
   dot.queue_family = queue_family;
   li.xr            = xr;
   li.images        = dot.images;
   xr->frame.cursor = openxr_swapchains_create_filled(&xr->sc, formats, 4,
         VIDEO_SCALE_PACK(VIDEO_XR_CURSOR_DIM, VIDEO_XR_CURSOR_DIM),
         vulkan_openxr_list_images, &li, vulkan_openxr_cursor_fill, &dot,
         &step, &code);
   if (xr->frame.cursor)
      RARCH_LOG("[OpenXR] Laser cursor ready.\n");
   else
      RARCH_WARN("[OpenXR] No laser cursor (%s, %d).\n", step, code);
}

bool vulkan_openxr_start(vulkan_openxr_t *xr, VkInstance instance,
      VkPhysicalDevice gpu, VkDevice device, uint32_t queue_family,
      slock_t *queue_lock)
{
   /* A kept device outlives a session the runtime ended: a new one. */
   if (openxr_session_ended(&xr->session))
   {
      vulkan_openxr_destroy_session(xr);
      openxr_session_reset(&xr->session);
   }
   if (     !xr->session.session
         && !vulkan_openxr_create_session(xr, instance, gpu, device,
            queue_family))
      return false;
   if (xr->device != device)
   {
      RARCH_ERR("[OpenXR] The session belongs to another Vulkan device.\n");
      return false;
   }
   /* Also marks the session started for vulkan_openxr_stop's keep. */
   xr->queue_lock = queue_lock;
   if (!xr->frame.cursor)
      vulkan_openxr_cursor_create(xr, gpu, device, queue_family);
   if (xr->inline_host ? !openxr_frame_start_inline(&xr->frame)
                       : !openxr_frame_start(&xr->frame))
      return false;
   vulkan_openxr_host_register(xr);
   return true;
}

void vulkan_openxr_host_register(vulkan_openxr_t *xr)
{
   video_xr_host_t host;
   memset(&host, 0, sizeof(host));
   host.state     = vulkan_openxr_host_state;
   host.sample    = vulkan_openxr_host_sample;
   host.eye_state = vulkan_openxr_host_eye_state;
   host.give      = vulkan_openxr_host_give;
   host.set_space = vulkan_openxr_host_set_space;
   host.head_pose = vulkan_openxr_host_head_pose;
   host.locate_hands = vulkan_openxr_host_locate_hands;
   host.controller_poses = vulkan_openxr_host_controller_poses;
   host.eye_size  = vulkan_openxr_host_eye_size;
   host.resume    = vulkan_openxr_host_resume;
   host.user      = xr;
   video_driver_set_xr_host(&host);
}

void vulkan_openxr_stop_thread(vulkan_openxr_t *xr)
{
   if (!xr)
      return;
   if (xr->inline_host)
      openxr_frame_stop_inline(&xr->frame);
   else
      openxr_frame_stop(&xr->frame);
}

void vulkan_openxr_stop(vulkan_openxr_t *xr)
{
   unsigned s;
   if (!xr)
      return;
   /* Once a start, while the session still shows what the core drew:
    * free's second stop keeps what the first did. */
   if (xr->queue_lock)
      openxr_host_keep(&xr->host);
   vulkan_openxr_stop_thread(xr);
   video_driver_set_xr_host(NULL);
   /* A kept session outlives the driver's views of its images. */
   for (s = 0; s < OPENXR_MAX_SLOTS; s++)
      vulkan_openxr_slot_destroy(xr, s);
   if (xr->frame.cursor)
   {
      slock_lock(xr->queue_lock);
      xr->sc.DestroySwapchain(xr->frame.cursor);
      slock_unlock(xr->queue_lock);
      xr->frame.cursor = XR_NULL_HANDLE;
   }
   openxr_frame_clear_quads(&xr->frame);
   openxr_frame_clear_projection(&xr->frame);
   /* The context frees it next; a kept session outlives it. */
   xr->queue_lock = NULL;
}

void vulkan_openxr_drop_and_reinit(vulkan_openxr_t *xr)
{
   RARCH_ERR("[OpenXR] Rebuilding video without headset output.\n");
   openxr_runtime_notify(MSG_OPENXR_FAILED);
   vulkan_openxr_free(xr);
   vulkan_openxr_skip_once = true;
}

bool vulkan_openxr_lost(const vulkan_openxr_t *xr)
{
   return xr->session.lost;
}

bool vulkan_openxr_alive(vulkan_openxr_t *xr)
{
   return openxr_session_alive(&xr->session);
}

bool vulkan_openxr_focused(vulkan_openxr_t *xr)
{
   return openxr_session_focused(&xr->session);
}

bool vulkan_openxr_should_draw(vulkan_openxr_t *xr)
{
   return openxr_session_visible(&xr->session);
}

float vulkan_openxr_pixels_per_radian(vulkan_openxr_t *xr)
{
   return openxr_frame_pixels_per_radian(&xr->frame);
}

float vulkan_openxr_refresh_rate(vulkan_openxr_t *xr)
{
   return openxr_frame_refresh_rate(&xr->frame);
}

void vulkan_openxr_set_pacing(vulkan_openxr_t *xr, unsigned interval)
{
   openxr_frame_set_pacing(&xr->frame, interval);
}

void vulkan_openxr_pace_skip(vulkan_openxr_t *xr)
{
   openxr_frame_pace_skip(&xr->frame);
}

void vulkan_openxr_pace_wait(vulkan_openxr_t *xr)
{
   openxr_frame_pace_wait(&xr->frame);
}

unsigned vulkan_openxr_refresh_rates(const vulkan_openxr_t *xr,
      float *rates, unsigned cap)
{
   return openxr_frame_refresh_rates(&xr->frame, rates, cap);
}

void vulkan_openxr_request_rate(vulkan_openxr_t *xr, float hz)
{
   openxr_frame_request_rate(&xr->frame, hz);
}

unsigned vulkan_openxr_max_dim(const vulkan_openxr_t *xr)
{
   return xr->rt.max_dim;
}

bool vulkan_openxr_supports_format(const vulkan_openxr_t *xr,
      VkFormat format)
{
   return openxr_swapchains_supports(&xr->sc, (int64_t)format);
}

void vulkan_openxr_slot_destroy(vulkan_openxr_t *xr, unsigned slot)
{
   openxr_slot_destroy(&xr->sc, slot);
}

bool vulkan_openxr_slot_create(vulkan_openxr_t *xr, unsigned slot,
      VkFormat format, bool mutable_format, unsigned dims, unsigned layers,
      VkImage (*images)[2], unsigned *num_images)
{
   struct vulkan_openxr_images li;
   li.xr     = xr;
   li.images = images;
   return openxr_slot_create(&xr->sc, slot, (int64_t)format, mutable_format,
         dims, layers, vulkan_openxr_list_images, &li, num_images);
}

bool vulkan_openxr_slot_acquire(vulkan_openxr_t *xr, unsigned slot,
      unsigned *index)
{
   return openxr_slot_acquire(&xr->sc, slot, index);
}

void vulkan_openxr_slot_release(vulkan_openxr_t *xr, unsigned slot)
{
   openxr_slot_release(&xr->sc, slot);
}

void vulkan_openxr_slot_forget(vulkan_openxr_t *xr, unsigned slot)
{
   openxr_slot_forget(&xr->sc, slot);
}

void vulkan_openxr_publish(vulkan_openxr_t *xr,
      const video_xr_quad_set_t *set)
{
   openxr_frame_publish(&xr->frame, set);
}

void vulkan_openxr_set_hidden(vulkan_openxr_t *xr, bool hidden)
{
   retro_atomic_store_release_int(&xr->hidden, hidden ? 1 : 0);
}

void vulkan_openxr_publish_projection(vulkan_openxr_t *xr,
      unsigned width, unsigned height)
{
   openxr_projection_t p;
   openxr_host_projection(&xr->host, width, height, &p);
   openxr_frame_publish_projection(&xr->frame, &p);
}

void vulkan_openxr_clear_projection(vulkan_openxr_t *xr)
{
   openxr_frame_clear_projection(&xr->frame);
}

bool vulkan_openxr_has_given(const vulkan_openxr_t *xr)
{
   return openxr_host_has_given(&xr->host);
}

void vulkan_openxr_place_kept(vulkan_openxr_t *xr)
{
   openxr_host_place_kept(&xr->host);
}

void vulkan_openxr_new_frame(vulkan_openxr_t *xr)
{
   openxr_host_new_frame(&xr->host);
}

unsigned vulkan_openxr_eye_dims(const vulkan_openxr_t *xr)
{
   unsigned w = xr->rt.rec_width;
   unsigned h = xr->rt.rec_height;
   if (!w || !h)
      return 0;
   return VIDEO_SCALE_PACK(MIN(w, xr->rt.max_dim), MIN(h, xr->rt.max_dim));
}

void vulkan_openxr_request_recenter(vulkan_openxr_t *xr)
{
   openxr_frame_request_recenter(&xr->frame);
}

void vulkan_openxr_set_placement(vulkan_openxr_t *xr, unsigned placement)
{
   openxr_frame_set_placement(&xr->frame, placement);
}

bool vulkan_openxr_inline(const vulkan_openxr_t *xr)
{
   return xr->inline_host;
}

void vulkan_openxr_begin_frame(vulkan_openxr_t *xr)
{
   /* A second wait costs a whole headset period. */
   if (xr->begun.waited)
      return;
   openxr_frame_begin(&xr->frame, &xr->begun);
}

void vulkan_openxr_end_frame(vulkan_openxr_t *xr, unsigned interval)
{
   openxr_frame_end(&xr->frame, &xr->begun);
   memset(&xr->begun, 0, sizeof(xr->begun));
   if (interval > 1)
      openxr_frame_repeat(&xr->frame, interval - 1);
}

void vulkan_openxr_eye_size(const vulkan_openxr_t *xr, unsigned *width,
      unsigned *height)
{
   *width  = xr->rt.rec_width;
   *height = xr->rt.rec_height;
}
