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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "vksym.h"
#include "vulkan_openxr.h"
#include "openxr_runtime.h"
#include "openxr_session.h"
#include "openxr_swapchain.h"
#include "openxr_frame.h"
#include "../video_driver.h"

#include "../../msg_hash.h"
#include "../../verbosity.h"

#define VULKAN_OPENXR_EXT_BUF 1024

struct vulkan_openxr
{
   openxr_runtime_t rt;
   bool enable2;
   /* The session is made on the thread that makes the device; its frame
    * loop runs on the XR thread. lock guards what both read. */
   openxr_session_t session;
   openxr_swapchains_t sc;
   openxr_frame_t frame;
   VkDevice device;
   slock_t *queue_lock;
   XrSwapchain cursor;             /* start to stop; the XR thread reads it */

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
   openxr_frame_tick_notify(&((vulkan_openxr_t*)user)->frame);
   RARCH_WARN("[OpenXR] The headset session ended; the window keeps the output.\n");
   openxr_runtime_notify(MSG_OPENXR_SESSION_ENDED);
}

static void vulkan_openxr_exiting_hook(void *user)
{
   video_driver_headset_exit_request();
}

static void vulkan_openxr_local_hook(void *user)
{
   openxr_frame_local_changed(&((vulkan_openxr_t*)user)->frame);
}

#define VULKAN_OPENXR_FN(xr, name) OPENXR_FN(&(xr)->rt, (xr), name)

static vulkan_openxr_hooks_t vulkan_openxr_hooks;
/* Set after a session fails on a device made for it: the reinit that
 * follows builds the device without the runtime. */
static bool vulkan_openxr_skip_once;

static unsigned vulkan_openxr_extra_layers(void *user, XrTime time,
      const XrCompositionLayerBaseHeader **layers, unsigned cap)
{
   if (!vulkan_openxr_hooks.frame_layers)
      return 0;
   return vulkan_openxr_hooks.frame_layers(vulkan_openxr_hooks.user, time,
         layers, cap);
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
      RARCH_LOG("[OpenXR] Starting once without headset output after a failure.\n");
      return NULL;
   }
   if (!(xr = (vulkan_openxr_t*)calloc(1, sizeof(*xr))))
      return NULL;
   xr->session.hooks.state_changed = vulkan_openxr_state_hook;
   xr->session.hooks.ended         = vulkan_openxr_ended_hook;
   xr->session.hooks.exiting       = vulkan_openxr_exiting_hook;
   xr->session.hooks.local_changed = vulkan_openxr_local_hook;
   xr->session.hooks.lock          = vulkan_openxr_lock_queue;
   xr->session.hooks.unlock        = vulkan_openxr_unlock_queue;
   xr->session.hooks.user          = xr;
   xr->frame.extra_layers          = vulkan_openxr_extra_layers;
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
   memset(&reqs, 0, sizeof(reqs));
   reqs.type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR;
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
   return NULL;
}

/* After vulkan_openxr_stop(). */
static void vulkan_openxr_destroy_session(vulkan_openxr_t *xr)
{
   if (xr->session.session && vulkan_openxr_hooks.session_destroying)
      vulkan_openxr_hooks.session_destroying(vulkan_openxr_hooks.user, xr);
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

   memset(&ci, 0, sizeof(ci));
   ci.type                   = XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR;
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

   memset(&ci, 0, sizeof(ci));
   ci.type                   = XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR;
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
      memset(&gi, 0, sizeof(gi));
      gi.type           = XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR;
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

void vulkan_openxr_set_hooks(const vulkan_openxr_hooks_t *hooks)
{
   if (hooks)
      vulkan_openxr_hooks = *hooks;
   else
      memset(&vulkan_openxr_hooks, 0, sizeof(vulkan_openxr_hooks));
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

   memset(&binding, 0, sizeof(binding));
   binding.type             = XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR;
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
   RARCH_LOG("[OpenXR] Session created.\n");

   if (vulkan_openxr_hooks.session_created)
   {
      vulkan_openxr_handles_t h;
      h.xr          = xr;
      h.instance    = xr->rt.instance;
      h.session     = xr->session.session;
      h.local_space = xr->session.local_space;
      h.get_proc    = xr->rt.GetInstanceProcAddr;
      h.frame_controller = xr->rt.frame_controller;
      vulkan_openxr_hooks.session_created(vulkan_openxr_hooks.user, &h);
   }
   return true;
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
   XrSwapchain chain          = XR_NULL_HANDLE;
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
   XrSwapchainImageVulkanKHR imgs[OPENXR_MAX_IMAGES];
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
   for (i = 0; i < OPENXR_MAX_IMAGES; i++)
      imgs[i].type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
   memset(&ai, 0, sizeof(ai));
   ai.type        = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO;
   memset(&wi, 0, sizeof(wi));
   wi.type        = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
   wi.timeout     = 100000000; /* 100 ms: a fresh swapchain is free */

   slock_lock(xr->queue_lock);
   res = xr->sc.CreateSwapchain(xr->session.session, &ci, &chain);
   if (XR_SUCCEEDED(res))
      res = xr->sc.EnumerateSwapchainImages(chain, 0, &n, NULL);
   if (XR_SUCCEEDED(res) && (!n || n > OPENXR_MAX_IMAGES))
      res = XR_ERROR_SIZE_INSUFFICIENT;
   if (XR_SUCCEEDED(res))
      res = xr->sc.EnumerateSwapchainImages(chain, n, &n,
            (XrSwapchainImageBaseHeader*)imgs);
   if (XR_SUCCEEDED(res))
      res = xr->sc.AcquireSwapchainImage(chain, &ai, &index);
   if (XR_SUCCEEDED(res))
      res = xr->sc.WaitSwapchainImage(chain, &wi);
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
   res = xr->sc.ReleaseSwapchainImage(chain, &ri);
   slock_unlock(xr->queue_lock);
   if (XR_FAILED(res))
   {
      step = "xrReleaseSwapchainImage";
      goto end;
   }
   xr->cursor = chain;
   chain      = XR_NULL_HANDLE;
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
   if (chain)
   {
      slock_lock(xr->queue_lock);
      xr->sc.DestroySwapchain(chain);
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
   /* A kept device outlives a session the runtime ended: a new one. */
   if (xr->session.ended)
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
   xr->queue_lock = queue_lock;
   if (!xr->cursor)
      vulkan_openxr_cursor_create(xr, gpu, device, queue_family);
   return openxr_frame_start(&xr->frame);
}

void vulkan_openxr_stop_thread(vulkan_openxr_t *xr)
{
   if (xr)
      openxr_frame_stop(&xr->frame);
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
      xr->sc.DestroySwapchain(xr->cursor);
      slock_unlock(xr->queue_lock);
      xr->cursor = XR_NULL_HANDLE;
   }
   openxr_frame_clear_quads(&xr->frame);
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

XrTime vulkan_openxr_predicted_time(vulkan_openxr_t *xr)
{
   return openxr_frame_predicted_time(&xr->frame);
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
   memset(imgs, 0, sizeof(imgs));
   for (i = 0; i < OPENXR_MAX_IMAGES; i++)
      imgs[i].type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
   res = li->xr->sc.EnumerateSwapchainImages(swapchain, *count, count,
         (XrSwapchainImageBaseHeader*)imgs);
   for (i = 0; XR_SUCCEEDED(res) && i < *count; i++)
      li->images[i][layer] = imgs[i].image;
   return res;
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

void vulkan_openxr_get_anchor(vulkan_openxr_t *xr, video_xr_pose_t *anchor)
{
   openxr_frame_get_anchor(&xr->frame, anchor);
}

bool vulkan_openxr_get_quads(vulkan_openxr_t *xr, video_xr_quad_set_t *out)
{
   if (!xr)
      return false;
   openxr_frame_get_quads(&xr->frame, out);
   return true;
}

void vulkan_openxr_request_recenter(vulkan_openxr_t *xr)
{
   openxr_frame_request_recenter(&xr->frame);
}

XrSwapchain vulkan_openxr_cursor(const vulkan_openxr_t *xr)
{
   return xr->cursor;
}
