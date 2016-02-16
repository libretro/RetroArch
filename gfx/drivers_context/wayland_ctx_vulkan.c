/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2016 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#define VK_USE_PLATFORM_WAYLAND_KHR
#include "../common/vulkan_common.h"
#include "../video_context_driver.h"

#include <sys/poll.h>
#include <unistd.h>
#include <signal.h>

#include <wayland-client.h>

#include "../../driver.h"
#include "../../general.h"
#include "../../runloop.h"

static volatile sig_atomic_t g_quit = 0;

static VkInstance cached_instance;
static VkDevice cached_device;

static void sighandler(int sig)
{
   (void)sig;
   g_quit = 1;
}

static void install_sighandlers(void)
{
   struct sigaction sa;

   sa.sa_sigaction = NULL;
   sa.sa_handler   = sighandler;
   sa.sa_flags     = SA_RESTART;
   sigemptyset(&sa.sa_mask);
   sigaction(SIGINT, &sa, NULL);
   sigaction(SIGTERM, &sa, NULL);
}

#define GET_INSTANCE_PROC_ADDR(inst, entrypoint) do {                                      \
   wl->fp##entrypoint = (PFN_vk##entrypoint) vkGetInstanceProcAddr(inst, "vk"#entrypoint); \
   if (wl->fp##entrypoint == NULL) {                                                       \
      RARCH_ERR("vkGetInstanceProcAddr failed to find vk%s\n", #entrypoint);               \
      goto error;                                                                          \
   }                                                                                       \
} while(0)

#define GET_DEVICE_PROC_ADDR(dev, entrypoint) do {                                       \
   wl->fp##entrypoint = (PFN_vk##entrypoint) vkGetDeviceProcAddr(dev, "vk" #entrypoint); \
   if (wl->fp##entrypoint == NULL) {                                                     \
      RARCH_ERR("vkGetDeviceProcAddr failed to find vk%s\n", #entrypoint);               \
      goto error;                                                                        \
   }                                                                                     \
} while(0)

typedef struct gfx_ctx_wayland_data
{
   struct vulkan_context context;

   bool resize;
   int fd;
   unsigned width;
   unsigned height;
   struct wl_display *dpy;
   struct wl_registry *registry;
   struct wl_compositor *compositor;
   struct wl_surface *surf;
   struct wl_shell_surface *shell_surf;
   struct wl_shell *shell;
   unsigned swap_interval;
   bool need_new_swapchain;

   unsigned buffer_scale;

   PFN_vkGetPhysicalDeviceSurfaceSupportKHR fpGetPhysicalDeviceSurfaceSupportKHR;
   PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR fpGetPhysicalDeviceSurfaceCapabilitiesKHR;
   PFN_vkGetPhysicalDeviceSurfaceFormatsKHR fpGetPhysicalDeviceSurfaceFormatsKHR;
   PFN_vkGetPhysicalDeviceSurfacePresentModesKHR fpGetPhysicalDeviceSurfacePresentModesKHR;
   PFN_vkCreateSwapchainKHR fpCreateSwapchainKHR;
   PFN_vkDestroySwapchainKHR fpDestroySwapchainKHR;
   PFN_vkGetSwapchainImagesKHR fpGetSwapchainImagesKHR;
   PFN_vkAcquireNextImageKHR fpAcquireNextImageKHR;
   PFN_vkQueuePresentKHR fpQueuePresentKHR;
   PFN_vkCreateWaylandSurfaceKHR fpCreateWaylandSurfaceKHR;
   PFN_vkDestroySurfaceKHR fpDestroySurfaceKHR;

   VkSurfaceKHR surface;
   VkSwapchainKHR swapchain;
} gfx_ctx_wayland_data_t;

static bool vulkan_create_swapchain(gfx_ctx_wayland_data_t *wl);

/* Shell surface callbacks. */
static void shell_surface_handle_ping(void *data,
      struct wl_shell_surface *shell_surface,
      uint32_t serial)
{
   (void)data;
   wl_shell_surface_pong(shell_surface, serial);
}

static void shell_surface_handle_configure(void *data,
      struct wl_shell_surface *shell_surface,
      uint32_t edges, int32_t width, int32_t height)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   (void)shell_surface;
   (void)edges;

   wl->width = wl->buffer_scale * width;
   wl->height = wl->buffer_scale * height;

   RARCH_LOG("[Wayland/Vulkan]: Surface configure: %u x %u.\n",
         wl->width, wl->height);
}

static void shell_surface_handle_popup_done(void *data,
      struct wl_shell_surface *shell_surface)
{
   (void)data;
   (void)shell_surface;
}

static const struct wl_shell_surface_listener shell_surface_listener = {
   shell_surface_handle_ping,
   shell_surface_handle_configure,
   shell_surface_handle_popup_done,
};

/* Registry callbacks. */
static void registry_handle_global(void *data, struct wl_registry *reg,
      uint32_t id, const char *interface, uint32_t version)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   (void)version;

   if (!strcmp(interface, "wl_compositor"))
      wl->compositor = (struct wl_compositor*)wl_registry_bind(reg, id, &wl_compositor_interface, 3);
   else if (!strcmp(interface, "wl_shell"))
      wl->shell = (struct wl_shell*)wl_registry_bind(reg, id, &wl_shell_interface, 1);
}

static void registry_handle_global_remove(void *data,
      struct wl_registry *registry, uint32_t id)
{
   (void)data;
   (void)registry;
   (void)id;
}

static const struct wl_registry_listener registry_listener = {
   registry_handle_global,
   registry_handle_global_remove,
};

static void gfx_ctx_wl_get_video_size(void *data,
      unsigned *width, unsigned *height);

static void gfx_ctx_wl_destroy_resources(gfx_ctx_wayland_data_t *wl)
{
   unsigned i;

   if (!wl)
      return;

   if (wl->context.queue)
      vkQueueWaitIdle(wl->context.queue);

   if (wl->swapchain)
      wl->fpDestroySwapchainKHR(wl->context.device, wl->swapchain, NULL);

   if (wl->surface)
      wl->fpDestroySurfaceKHR(wl->context.instance, wl->surface, NULL);

   for (i = 0; i < VULKAN_MAX_SWAPCHAIN_IMAGES; i++)
   {
      if (wl->context.swapchain_semaphores[i] != VK_NULL_HANDLE)
         vkDestroySemaphore(wl->context.device, wl->context.swapchain_semaphores[i], NULL);
      if (wl->context.swapchain_fences[i] != VK_NULL_HANDLE)
         vkDestroyFence(wl->context.device, wl->context.swapchain_fences[i], NULL);
   }

   if (video_driver_ctl(RARCH_DISPLAY_CTL_IS_VIDEO_CACHE_CONTEXT, NULL))
   {
      cached_device = wl->context.device;
      cached_instance = wl->context.instance;
   }
   else
   {
      if (wl->context.device)
         vkDestroyDevice(wl->context.device, NULL);
      if (wl->context.instance)
         vkDestroyInstance(wl->context.instance, NULL);
   }

   if (wl->fd >= 0)
      close(wl->fd);

   if (wl->shell_surf)
      wl_shell_surface_destroy(wl->shell_surf);
   if (wl->surf)
      wl_surface_destroy(wl->surf);
   if (wl->compositor)
      wl_compositor_destroy(wl->compositor);
   if (wl->registry)
      wl_registry_destroy(wl->registry);
   if (wl->dpy)
      wl_display_disconnect(wl->dpy);

   wl->dpy        = NULL;
   wl->shell      = NULL;
   wl->compositor = NULL;
   wl->registry   = NULL;
   wl->dpy        = NULL;
   wl->shell_surf = NULL;
   wl->surf       = NULL;

   wl->width  = 0;
   wl->height = 0;
}

static void flush_wayland_fd(gfx_ctx_wayland_data_t *wl)
{
   struct pollfd fd = {0};

   wl_display_dispatch_pending(wl->dpy);
   wl_display_flush(wl->dpy);

   fd.fd = wl->fd;
   fd.events = POLLIN | POLLOUT | POLLERR | POLLHUP;

   if (poll(&fd, 1, 0) > 0)
   {
      if (fd.revents & (POLLERR | POLLHUP))
      {
         close(wl->fd);
         g_quit = true;
      }

      if (fd.revents & POLLIN)
         wl_display_dispatch(wl->dpy);
      if (fd.revents & POLLOUT)
         wl_display_flush(wl->dpy);
   }
}

static void gfx_ctx_wl_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height,
      unsigned frame_count)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   unsigned new_width, new_height;

   (void)frame_count;

   flush_wayland_fd(wl);

   new_width = *width;
   new_height = *height;

   gfx_ctx_wl_get_video_size(wl, &new_width, &new_height);

   /* Swapchains are recreated in set_resize as a central place, so use that to trigger swapchain reinit. */
   *resize = wl->need_new_swapchain;

   if (new_width != *width || new_height != *height)
   {
      *resize = true;
      *width  = new_width;
      *height = new_height;
   }

   *quit = g_quit;
}

static bool gfx_ctx_wl_set_resize(void *data, unsigned width, unsigned height)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   wl->width = width;
   wl->height = height;
   if (!vulkan_create_swapchain(wl))
   {
      RARCH_ERR("[Wayland/Vulkan]: Failed to update swapchain.\n");
      return false;
   }
   else
      wl->context.invalid_swapchain = true;

   wl->need_new_swapchain = false;
   return true;
}

static void gfx_ctx_wl_update_window_title(void *data)
{
   char buf[128]              = {0};
   char buf_fps[128]          = {0};
   settings_t *settings       = config_get_ptr();
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   if (video_monitor_get_fps(buf, sizeof(buf),  
            buf_fps, sizeof(buf_fps)))
      wl_shell_surface_set_title(wl->shell_surf, buf);

   if (settings->fps_show)
      runloop_msg_queue_push(buf_fps, 1, 1, false);
}

static void gfx_ctx_wl_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   *width  = wl->width;
   *height = wl->height;
}

#define DEFAULT_WINDOWED_WIDTH 640
#define DEFAULT_WINDOWED_HEIGHT 480

static void *gfx_ctx_wl_init(void *video_driver)
{
   VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
   VkInstanceCreateInfo info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
   VkDeviceQueueCreateInfo queue_info = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
   VkPhysicalDeviceFeatures features = { false };
   VkDeviceCreateInfo device_info = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
   uint32_t gpu_count = 1;
   uint32_t queue_count;
   bool found_queue = false;
   unsigned i;
   VkQueueFamilyProperties queue_properties[32];
   static const float one = 1.0f;

   static const char *instance_extensions[] = {
      "VK_KHR_surface",
      "VK_KHR_wayland_surface",
   };

   static const char *device_extensions[] = {
      "VK_KHR_swapchain",
   };

   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)
      calloc(1, sizeof(gfx_ctx_wayland_data_t));

   if (!wl)
      return NULL;

   wl->dpy = wl_display_connect(NULL);
   if (!wl->dpy)
   {
      RARCH_ERR("Failed to connect to Wayland server.\n");
      goto error;
   }

   install_sighandlers();

   wl->registry = wl_display_get_registry(wl->dpy);
   wl_registry_add_listener(wl->registry, &registry_listener, wl);
   wl_display_roundtrip(wl->dpy);

   if (!wl->compositor)
   {
      RARCH_ERR("Failed to create compositor.\n");
      goto error;
   }

   if (!wl->shell)
   {
      RARCH_ERR("Failed to create shell.\n");
      goto error;
   }

   wl->fd = wl_display_get_fd(wl->dpy);

   app.pApplicationName = "RetroArch";
   app.applicationVersion = 0;
   app.pEngineName = "RetroArch";
   app.engineVersion = 0;
   app.apiVersion = VK_API_VERSION;

   info.pApplicationInfo = &app;
   info.enabledExtensionCount = ARRAY_SIZE(instance_extensions);
   info.ppEnabledExtensionNames = instance_extensions;

   if (cached_instance)
   {
      wl->context.instance = cached_instance;
      cached_instance = NULL;
   }
   else if (vkCreateInstance(&info, NULL, &wl->context.instance) != VK_SUCCESS)
      goto error;

   if (vkEnumeratePhysicalDevices(wl->context.instance, &gpu_count, &wl->context.gpu) != VK_SUCCESS)
      goto error;

   if (gpu_count != 1)
   {
      RARCH_ERR("[Wayland/Vulkan]: Failed to enumerate Vulkan physical device.\n");
      goto error;
   }

   vkGetPhysicalDeviceProperties(wl->context.gpu, &wl->context.gpu_properties);
   vkGetPhysicalDeviceMemoryProperties(wl->context.gpu, &wl->context.memory_properties);
   vkGetPhysicalDeviceQueueFamilyProperties(wl->context.gpu, &queue_count, NULL);
   if (queue_count < 1 || queue_count > 32)
      goto error;
   vkGetPhysicalDeviceQueueFamilyProperties(wl->context.gpu, &queue_count, queue_properties);

   for (i = 0; i < queue_count; i++)
   {
      if (queue_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
      {
         wl->context.graphics_queue_index = i;
         RARCH_LOG("[Wayland/Vulkan]: Device supports %u sub-queues.\n",
               queue_properties[i].queueCount);
         found_queue = true;
         break;
      }
   }

   if (!found_queue)
   {
      RARCH_ERR("[Wayland/Vulkan]: Did not find suitable graphics queue.\n");
      goto error;
   }

   queue_info.queueFamilyIndex = wl->context.graphics_queue_index;
   queue_info.queueCount = 1;
   queue_info.pQueuePriorities = &one;

   device_info.queueCreateInfoCount = 1;
   device_info.pQueueCreateInfos = &queue_info;
   device_info.enabledExtensionCount = ARRAY_SIZE(device_extensions);
   device_info.ppEnabledExtensionNames = device_extensions;
   device_info.pEnabledFeatures = &features;

   if (cached_device)
   {
      wl->context.device = cached_device;
      cached_device = NULL;
      video_driver_ctl(RARCH_DISPLAY_CTL_SET_VIDEO_CACHE_CONTEXT_ACK, NULL);
      RARCH_LOG("[Vulkan]: Using cached Vulkan context.\n");
   }
   else if (vkCreateDevice(wl->context.gpu, &device_info, NULL, &wl->context.device) != VK_SUCCESS)
      goto error;

   vkGetDeviceQueue(wl->context.device, wl->context.graphics_queue_index, 0, &wl->context.queue);

   GET_INSTANCE_PROC_ADDR(wl->context.instance, GetPhysicalDeviceSurfaceSupportKHR);
   GET_INSTANCE_PROC_ADDR(wl->context.instance, GetPhysicalDeviceSurfaceCapabilitiesKHR);
   GET_INSTANCE_PROC_ADDR(wl->context.instance, GetPhysicalDeviceSurfaceFormatsKHR);
   GET_INSTANCE_PROC_ADDR(wl->context.instance, GetPhysicalDeviceSurfacePresentModesKHR);
   GET_INSTANCE_PROC_ADDR(wl->context.instance, CreateWaylandSurfaceKHR);
   GET_INSTANCE_PROC_ADDR(wl->context.instance, DestroySurfaceKHR);
   GET_DEVICE_PROC_ADDR(wl->context.device, CreateSwapchainKHR);
   GET_DEVICE_PROC_ADDR(wl->context.device, DestroySwapchainKHR);
   GET_DEVICE_PROC_ADDR(wl->context.device, GetSwapchainImagesKHR);
   GET_DEVICE_PROC_ADDR(wl->context.device, AcquireNextImageKHR);
   GET_DEVICE_PROC_ADDR(wl->context.device, QueuePresentKHR);

   wl->context.queue_lock = slock_new();
   if (!wl->context.queue_lock)
      goto error;

   return wl;

error:
   gfx_ctx_wl_destroy_resources(wl);
   free(wl);
   return NULL;
}

static void gfx_ctx_wl_destroy(void *data)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   if (!wl)
      return;

   gfx_ctx_wl_destroy_resources(wl);
   if (wl->context.queue_lock)
      slock_free(wl->context.queue_lock);
   free(wl);
}

static void vulkan_acquire_next_image(gfx_ctx_wayland_data_t *wl)
{
   VkResult err;
   VkSemaphoreCreateInfo sem_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
   VkFenceCreateInfo info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
   VkFence fence;
   VkFence *next_fence;
   unsigned index;

   vkCreateFence(wl->context.device, &info, NULL, &fence);

   err = wl->fpAcquireNextImageKHR(wl->context.device, wl->swapchain, UINT64_MAX,
         VK_NULL_HANDLE, fence, &wl->context.current_swapchain_index);

   index = wl->context.current_swapchain_index;
   if (wl->context.swapchain_semaphores[index] == VK_NULL_HANDLE)
      vkCreateSemaphore(wl->context.device, &sem_info, NULL, &wl->context.swapchain_semaphores[index]);

   vkWaitForFences(wl->context.device, 1, &fence, true, UINT64_MAX);
   vkDestroyFence(wl->context.device, fence, NULL);

   next_fence = &wl->context.swapchain_fences[index];
   if (*next_fence != VK_NULL_HANDLE)
   {
      vkWaitForFences(wl->context.device, 1, next_fence, true, UINT64_MAX);
      vkResetFences(wl->context.device, 1, next_fence);
   }
   else
      vkCreateFence(wl->context.device, &info, NULL, next_fence);

   if (err != VK_SUCCESS)
   {
      RARCH_LOG("[Wayland/Vulkan]: AcquireNextImage failed, invalidating swapchain.\n");
      wl->context.invalid_swapchain = true;
   }
}

static bool vulkan_create_swapchain(gfx_ctx_wayland_data_t *wl)
{
   VkSurfaceCapabilitiesKHR surface_properties;
   VkSurfaceFormatKHR formats[256];
   VkSurfaceFormatKHR format;
   VkExtent2D swapchain_size;
   VkPresentModeKHR swapchain_present_mode = wl->swap_interval ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR;
   VkSurfaceTransformFlagBitsKHR pre_transform;
   VkSwapchainKHR old_swapchain;
   VkSwapchainCreateInfoKHR info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
   uint32_t format_count;
   uint32_t desired_swapchain_images;
   unsigned i;

   RARCH_LOG("[Wayland/Vulkan]: Creating swapchain with present mode: %u\n", (unsigned)swapchain_present_mode);

   wl->fpGetPhysicalDeviceSurfaceCapabilitiesKHR(wl->context.gpu, wl->surface, &surface_properties);
   wl->fpGetPhysicalDeviceSurfaceFormatsKHR(wl->context.gpu, wl->surface, &format_count, NULL);
   wl->fpGetPhysicalDeviceSurfaceFormatsKHR(wl->context.gpu, wl->surface, &format_count, formats);

   if (format_count == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
   {
      format = formats[0];
      format.format = VK_FORMAT_B8G8R8A8_UNORM;
   }
   else
   {
      if (format_count == 0)
      {
         RARCH_ERR("[Wayland Vulkan]: Surface has no formats.\n");
         return false;
      }

      format = formats[0];
   }

   if (surface_properties.currentExtent.width == -1)
   {
      swapchain_size.width = wl->width;
      swapchain_size.height = wl->height;
   }
   else
      swapchain_size = surface_properties.currentExtent;

   desired_swapchain_images = surface_properties.minImageCount + 1;
   if ((surface_properties.maxImageCount > 0) && (desired_swapchain_images > surface_properties.maxImageCount))
      desired_swapchain_images = surface_properties.maxImageCount;

   if (surface_properties.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
      pre_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
   else
      pre_transform = surface_properties.currentTransform;

   old_swapchain = wl->swapchain;

   info.surface = wl->surface;
   info.minImageCount = desired_swapchain_images;
   info.imageFormat = format.format;
   info.imageColorSpace = format.colorSpace;
   info.imageExtent.width = swapchain_size.width;
   info.imageExtent.height = swapchain_size.height;
   info.imageArrayLayers = 1;
   info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
   info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
   info.preTransform = pre_transform;
   info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
   info.presentMode = swapchain_present_mode;
   info.clipped = true;
   info.oldSwapchain = old_swapchain;

   wl->fpCreateSwapchainKHR(wl->context.device, &info, NULL, &wl->swapchain);
   if (old_swapchain != VK_NULL_HANDLE)
      wl->fpDestroySwapchainKHR(wl->context.device, old_swapchain, NULL);

   wl->context.swapchain_width = swapchain_size.width;
   wl->context.swapchain_height = swapchain_size.height;

   /* Make sure we create a backbuffer format that is as we expect. */
   switch (format.format)
   {
      case VK_FORMAT_B8G8R8A8_SRGB:
         wl->context.swapchain_format = VK_FORMAT_B8G8R8A8_UNORM;
         wl->context.swapchain_is_srgb = true;
         break;

      case VK_FORMAT_R8G8B8A8_SRGB:
         wl->context.swapchain_format = VK_FORMAT_R8G8B8A8_UNORM;
         wl->context.swapchain_is_srgb = true;
         break;

      case VK_FORMAT_R8G8B8_SRGB:
         wl->context.swapchain_format = VK_FORMAT_R8G8B8_UNORM;
         wl->context.swapchain_is_srgb = true;
         break;

      case VK_FORMAT_B8G8R8_SRGB:
         wl->context.swapchain_format = VK_FORMAT_B8G8R8_UNORM;
         wl->context.swapchain_is_srgb = true;
         break;

      default:
         wl->context.swapchain_format = format.format;
         break;
   }

   wl->fpGetSwapchainImagesKHR(wl->context.device, wl->swapchain,
         &wl->context.num_swapchain_images, NULL);
   wl->fpGetSwapchainImagesKHR(wl->context.device, wl->swapchain,
         &wl->context.num_swapchain_images, wl->context.swapchain_images);

   for (i = 0; i < wl->context.num_swapchain_images; i++)
   {
      if (wl->context.swapchain_fences[i])
      {
         vkDestroyFence(wl->context.device, wl->context.swapchain_fences[i], NULL);
         wl->context.swapchain_fences[i] = VK_NULL_HANDLE;
      }
   }

   vulkan_acquire_next_image(wl);

   return true;
}

static void vulkan_present(gfx_ctx_wayland_data_t *wl, unsigned index)
{
   VkResult result = VK_SUCCESS, err = VK_SUCCESS;
   VkPresentInfoKHR present = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
   present.swapchainCount = 1;
   present.pSwapchains = &wl->swapchain;
   present.pImageIndices = &index;
   present.pResults = &result;
   present.waitSemaphoreCount = 1;
   present.pWaitSemaphores = &wl->context.swapchain_semaphores[index];

   /* Better hope QueuePresent doesn't block D: */
   slock_lock(wl->context.queue_lock);
   err = wl->fpQueuePresentKHR(wl->context.queue, &present);
   if (err != VK_SUCCESS || result != VK_SUCCESS)
   {
      RARCH_LOG("[Wayland/Vulkan]: QueuePresent failed, invalidating swapchain.\n");
      wl->context.invalid_swapchain = true;
   }
   slock_unlock(wl->context.queue_lock);
}

static void gfx_ctx_wl_swap_buffers(void *data)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   vulkan_present(wl, wl->context.current_swapchain_index);
   vulkan_acquire_next_image(wl);
   flush_wayland_fd(wl);
}

static void gfx_ctx_wl_set_swap_interval(void *data, unsigned swap_interval)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   if (wl->swap_interval != swap_interval)
   {
      wl->swap_interval = swap_interval;
      if (wl->swapchain)
         wl->need_new_swapchain = true;
   }
}

static bool gfx_ctx_wl_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   VkWaylandSurfaceCreateInfoKHR wl_info = { VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR };

   wl->width = width ? width : DEFAULT_WINDOWED_WIDTH;
   wl->height = height ? height : DEFAULT_WINDOWED_HEIGHT;

   /* TODO: Use wl_output::scale to obtain correct value. */
   wl->buffer_scale = 1;

   wl->surf = wl_compositor_create_surface(wl->compositor);
   wl_surface_set_buffer_scale(wl->surf, wl->buffer_scale);

   wl->shell_surf = wl_shell_get_shell_surface(wl->shell, wl->surf);

   wl_shell_surface_add_listener(wl->shell_surf, &shell_surface_listener, wl);
   wl_shell_surface_set_toplevel(wl->shell_surf);
   wl_shell_surface_set_class(wl->shell_surf, "RetroArch");
   wl_shell_surface_set_title(wl->shell_surf, "RetroArch");

   if (fullscreen)
      wl_shell_surface_set_fullscreen(wl->shell_surf, WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT, 0, NULL);

   flush_wayland_fd(wl);
   wl_display_roundtrip(wl->dpy);

   wl_info.display = wl->dpy;
   wl_info.surface = wl->surf;

   wl->fpCreateWaylandSurfaceKHR(wl->context.instance, &wl_info, NULL, &wl->surface);

   if (!vulkan_create_swapchain(wl))
      goto error;

   return true;

error:
   gfx_ctx_wl_destroy(data);
   return false;
}

static void gfx_ctx_wl_input_driver(void *data,
      const input_driver_t **input, void **input_data)
{
   (void)data;
#if 0
   void *wl    = input_wayland.init();
   *input      = wl ? &input_wayland : NULL;
   *input_data = wl;
#endif
   *input = NULL;
   *input_data = NULL;
}

static bool gfx_ctx_wl_has_focus(void *data)
{
   (void)data;
   return true;
}

static bool gfx_ctx_wl_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return true;
}

static bool gfx_ctx_wl_has_windowed(void *data)
{
   (void)data;
   return true;
}

static bool gfx_ctx_wl_bind_api(void *video_driver,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)video_driver;
   (void)api;
   (void)major;
   (void)minor;
   return api == GFX_CTX_VULKAN_API;
}

static void *gfx_ctx_wl_get_context_data(void *data)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   return &wl->context;
}

const gfx_ctx_driver_t gfx_ctx_wayland_vulkan = {
   gfx_ctx_wl_init,
   gfx_ctx_wl_destroy,
   gfx_ctx_wl_bind_api,
   gfx_ctx_wl_set_swap_interval,
   gfx_ctx_wl_set_video_mode,
   gfx_ctx_wl_get_video_size,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   NULL,
   gfx_ctx_wl_update_window_title,
   gfx_ctx_wl_check_window,
   gfx_ctx_wl_set_resize,
   gfx_ctx_wl_has_focus,
   gfx_ctx_wl_suppress_screensaver,
   gfx_ctx_wl_has_windowed,
   gfx_ctx_wl_swap_buffers,
   gfx_ctx_wl_input_driver,
   NULL,
   NULL,
   NULL,
   NULL,
   "wayland-vulkan",
   NULL,
   gfx_ctx_wl_get_context_data,
};
