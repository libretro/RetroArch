/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_VULKAN
#define VK_USE_PLATFORM_WAYLAND_KHR
#include "../common/vulkan_common.h"
#endif

#include <sys/poll.h>
#include <unistd.h>
#include <signal.h>

#include <wayland-client.h>
#ifdef HAVE_EGL
#include <wayland-egl.h>
#endif

#include <string/stdstring.h>

#include "../../driver.h"
#include "../../general.h"
#include "../../runloop.h"
#ifdef HAVE_EGL
#include "../common/egl_common.h"
#endif
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
#include "../common/gl_common.h"
#endif

static volatile sig_atomic_t g_quit = 0;

#ifdef HAVE_VULKAN
static VkInstance cached_instance;
static VkDevice cached_device;

#endif

#ifdef HAVE_VULKAN
typedef struct gfx_ctx_vulkan_data
{
   struct vulkan_context context;

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

   VkSurfaceKHR vk_surface;
   VkSwapchainKHR swapchain;
   bool need_new_swapchain;
} gfx_ctx_vulkan_data_t;
#endif

typedef struct gfx_ctx_wayland_data
{

#ifdef HAVE_EGL
   egl_ctx_data_t egl;
   struct wl_egl_window *win;
#endif
   bool resize;
   int fd;
   unsigned width;
   unsigned height;
   struct wl_display *dpy;
   struct wl_registry *registry;
   struct wl_compositor *compositor;
   struct wl_surface *surface;
   struct wl_shell_surface *shell_surf;
   struct wl_shell *shell;
   struct wl_keyboard *wl_keyboard;
   struct wl_pointer  *wl_pointer;
   unsigned swap_interval;

   unsigned buffer_scale;

#ifdef HAVE_VULKAN
   gfx_ctx_vulkan_data_t vk;
#endif
} gfx_ctx_wayland_data_t;

#ifdef HAVE_VULKAN
/* Forward declaration */
static bool vulkan_create_swapchain(gfx_ctx_vulkan_data_t *vk,
      unsigned width, unsigned height, unsigned swap_interval);
#endif

static enum gfx_ctx_api wl_api;

#ifndef EGL_OPENGL_ES3_BIT_KHR
#define EGL_OPENGL_ES3_BIT_KHR 0x0040
#endif

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

   wl->width  = wl->buffer_scale * width;
   wl->height = wl->buffer_scale * height;

   RARCH_LOG("[Wayland/EGL]: Surface configure: %u x %u.\n",
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

   if (string_is_equal(interface, "wl_compositor"))
   {
      unsigned num = 1;
      switch (wl_api)
      {
         case GFX_CTX_OPENGL_API:
         case GFX_CTX_OPENGL_ES_API:
         case GFX_CTX_OPENVG_API:
            break;
         case GFX_CTX_VULKAN_API:
            num = 3;
            break;
         case GFX_CTX_NONE:
         default:
            break;
      }

      wl->compositor = (struct wl_compositor*)wl_registry_bind(reg,
            id, &wl_compositor_interface, num);
   }
   else if (string_is_equal(interface, "wl_shell"))
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

   (void)i;

   switch (wl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
      case GFX_CTX_OPENVG_API:
#ifdef HAVE_EGL
         egl_destroy(wl);

         if (wl->win)
            wl_egl_window_destroy(wl->win);
#endif
         break;
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         if (wl->vk.context.queue)
            vkQueueWaitIdle(wl->vk.context.queue);
         if (wl->vk.swapchain)
            wl->vk.fpDestroySwapchainKHR(wl->vk.context.device,
                  wl->vk.swapchain, NULL);

         if (wl->surface)
            wl->vk.fpDestroySurfaceKHR(wl->vk.context.instance,
                  wl->vk.vk_surface, NULL);

         for (i = 0; i < VULKAN_MAX_SWAPCHAIN_IMAGES; i++)
         {
            if (wl->vk.context.swapchain_semaphores[i] != VK_NULL_HANDLE)
               vkDestroySemaphore(wl->vk.context.device,
                     wl->vk.context.swapchain_semaphores[i], NULL);
            if (wl->vk.context.swapchain_fences[i] != VK_NULL_HANDLE)
               vkDestroyFence(wl->vk.context.device,
                     wl->vk.context.swapchain_fences[i], NULL);
         }

         if (video_driver_ctl(RARCH_DISPLAY_CTL_IS_VIDEO_CACHE_CONTEXT, NULL))
         {
            cached_device   = wl->vk.context.device;
            cached_instance = wl->vk.context.instance;
         }
         else
         {
            if (wl->vk.context.device)
               vkDestroyDevice(wl->vk.context.device, NULL);
            if (wl->vk.context.instance)
               vkDestroyInstance(wl->vk.context.instance, NULL);
         }

         if (wl->fd >= 0)
            close(wl->fd);
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   if (wl->shell)
      wl_shell_destroy(wl->shell);
   if (wl->compositor)
      wl_compositor_destroy(wl->compositor);
   if (wl->registry)
      wl_registry_destroy(wl->registry);
   if (wl->shell_surf)
      wl_shell_surface_destroy(wl->shell_surf);
   if (wl->surface)
      wl_surface_destroy(wl->surface);

   if (wl->dpy)
   {
      wl_display_flush(wl->dpy);
      wl_display_disconnect(wl->dpy);
   }

   wl->win        = NULL;
   wl->shell      = NULL;
   wl->compositor = NULL;
   wl->registry   = NULL;
   wl->dpy        = NULL;
   wl->shell_surf = NULL;
   wl->surface    = NULL;

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

   gfx_ctx_wl_get_video_size(data, &new_width, &new_height);

   switch (wl_api)
   {
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         /* Swapchains are recreated in set_resize as a 
          * central place, so use that to trigger swapchain reinit. */
         *resize = wl->vk.need_new_swapchain;
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

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

   switch (wl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
      case GFX_CTX_OPENVG_API:
#ifdef HAVE_EGL
         wl_egl_window_resize(wl->win, width, height, 0, 0);
#endif
         break;
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         wl->width  = width;
         wl->height = height;

         if (vulkan_create_swapchain(&wl->vk, width, height, wl->swap_interval))
            wl->vk.context.invalid_swapchain = true;
         else
         {
            RARCH_ERR("[Wayland/Vulkan]: Failed to update swapchain.\n");
            return false;
         }

         wl->vk.need_new_swapchain = false;
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

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

#ifdef HAVE_EGL
#define WL_EGL_ATTRIBS_BASE \
   EGL_SURFACE_TYPE,    EGL_WINDOW_BIT, \
   EGL_RED_SIZE,        1, \
   EGL_GREEN_SIZE,      1, \
   EGL_BLUE_SIZE,       1, \
   EGL_ALPHA_SIZE,      0, \
   EGL_DEPTH_SIZE,      0
#endif

#ifdef HAVE_VULKAN

#define VK_GET_INSTANCE_PROC_ADDR(vk, inst, entrypoint) do {                               \
   vk->fp##entrypoint = (PFN_vk##entrypoint) vkGetInstanceProcAddr(inst, "vk"#entrypoint); \
   if (vk->fp##entrypoint == NULL) {                                                       \
      RARCH_ERR("vkGetInstanceProcAddr failed to find vk%s\n", #entrypoint);               \
      return false;                                                                        \
   }                                                                                       \
} while(0)

#define VK_GET_DEVICE_PROC_ADDR(vk, dev, entrypoint) do {                                \
   vk->fp##entrypoint = (PFN_vk##entrypoint) vkGetDeviceProcAddr(dev, "vk" #entrypoint); \
   if (vk->fp##entrypoint == NULL) {                                                     \
      RARCH_ERR("vkGetDeviceProcAddr failed to find vk%s\n", #entrypoint);               \
      return false;                                                                      \
   }                                                                                     \
} while(0)

static void vulkan_destroy_context(gfx_ctx_vulkan_data_t *vk)
{
   if (vk->context.queue_lock)
      slock_free(vk->context.queue_lock);
}

static bool vulkan_init_context(gfx_ctx_vulkan_data_t *vk)
{
   unsigned i;
   uint32_t queue_count;
   VkQueueFamilyProperties queue_properties[32];
   VkInstanceCreateInfo info          = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
   VkDeviceQueueCreateInfo queue_info = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
   VkDeviceCreateInfo device_info     = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
   uint32_t gpu_count                 = 1;
   bool found_queue                   = false;
   static const float one             = 1.0f;

   static const char *instance_extensions[] = {
      "VK_KHR_surface",
      "VK_KHR_wayland_surface",
   };

   static const char *device_extensions[] = {
      "VK_KHR_swapchain",
   };
   VkApplicationInfo app             = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
   VkPhysicalDeviceFeatures features = { false };

   app.pApplicationName              = "RetroArch";
   app.applicationVersion            = 0;
   app.pEngineName                   = "RetroArch";
   app.engineVersion                 = 0;
   app.apiVersion                    = VK_API_VERSION;

   info.pApplicationInfo             = &app;
   info.enabledExtensionCount        = ARRAY_SIZE(instance_extensions);
   info.ppEnabledExtensionNames      = instance_extensions;

   if (cached_instance)
   {
      vk->context.instance           = cached_instance;
      cached_instance                = NULL;
   }
   else if (vkCreateInstance(&info, NULL, &vk->context.instance) != VK_SUCCESS)
      return false;

   if (vkEnumeratePhysicalDevices(vk->context.instance,
            &gpu_count, &vk->context.gpu) != VK_SUCCESS)
      return false;

   if (gpu_count != 1)
   {
      RARCH_ERR("[Wayland/Vulkan]: Failed to enumerate Vulkan physical device.\n");
      return false;
   }

   vkGetPhysicalDeviceProperties(vk->context.gpu,
         &vk->context.gpu_properties);
   vkGetPhysicalDeviceMemoryProperties(vk->context.gpu,
         &vk->context.memory_properties);
   vkGetPhysicalDeviceQueueFamilyProperties(vk->context.gpu,
         &queue_count, NULL);

   if (queue_count < 1 || queue_count > 32)
      return false;

   vkGetPhysicalDeviceQueueFamilyProperties(vk->context.gpu,
         &queue_count, queue_properties);

   for (i = 0; i < queue_count; i++)
   {
      if (queue_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
      {
         vk->context.graphics_queue_index = i;
         RARCH_LOG("[Wayland/Vulkan]: Device supports %u sub-queues.\n",
               queue_properties[i].queueCount);
         found_queue = true;
         break;
      }
   }

   if (!found_queue)
   {
      RARCH_ERR("[Wayland/Vulkan]: Did not find suitable graphics queue.\n");
      return false;
   }

   queue_info.queueFamilyIndex         = vk->context.graphics_queue_index;
   queue_info.queueCount               = 1;
   queue_info.pQueuePriorities         = &one;

   device_info.queueCreateInfoCount    = 1;
   device_info.pQueueCreateInfos       = &queue_info;
   device_info.enabledExtensionCount   = ARRAY_SIZE(device_extensions);
   device_info.ppEnabledExtensionNames = device_extensions;
   device_info.pEnabledFeatures        = &features;

   if (cached_device)
   {
      vk->context.device = cached_device;
      cached_device = NULL;
      video_driver_ctl(RARCH_DISPLAY_CTL_SET_VIDEO_CACHE_CONTEXT_ACK, NULL);
      RARCH_LOG("[Vulkan]: Using cached Vulkan context.\n");
   }
   else if (vkCreateDevice(vk->context.gpu, &device_info,
            NULL, &vk->context.device) != VK_SUCCESS)
      return false;

   vkGetDeviceQueue(vk->context.device,
         vk->context.graphics_queue_index, 0, &vk->context.queue);

   VK_GET_INSTANCE_PROC_ADDR(vk,
         vk->context.instance, GetPhysicalDeviceSurfaceSupportKHR);
   VK_GET_INSTANCE_PROC_ADDR(vk,
         vk->context.instance, GetPhysicalDeviceSurfaceCapabilitiesKHR);
   VK_GET_INSTANCE_PROC_ADDR(vk,
         vk->context.instance, GetPhysicalDeviceSurfaceFormatsKHR);
   VK_GET_INSTANCE_PROC_ADDR(vk,
         vk->context.instance, GetPhysicalDeviceSurfacePresentModesKHR);
   VK_GET_INSTANCE_PROC_ADDR(vk,
         vk->context.instance, CreateWaylandSurfaceKHR);
   VK_GET_INSTANCE_PROC_ADDR(vk,
         vk->context.instance, DestroySurfaceKHR);
   VK_GET_DEVICE_PROC_ADDR(vk,
         vk->context.device, CreateSwapchainKHR);
   VK_GET_DEVICE_PROC_ADDR(vk,
         vk->context.device, DestroySwapchainKHR);
   VK_GET_DEVICE_PROC_ADDR(vk,
         vk->context.device, GetSwapchainImagesKHR);
   VK_GET_DEVICE_PROC_ADDR(vk,
         vk->context.device, AcquireNextImageKHR);
   VK_GET_DEVICE_PROC_ADDR(vk,
         vk->context.device, QueuePresentKHR);

   vk->context.queue_lock = slock_new();
   if (!vk->context.queue_lock)
      return false;

   return true;
}
#endif

static void *gfx_ctx_wl_init(void *video_driver)
{

#ifdef HAVE_OPENGL
   static const EGLint egl_attribs_gl[] = {
      WL_EGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
      EGL_NONE,
   };
#endif

#ifdef HAVE_OPENGLES
#ifdef HAVE_OPENGLES2
   static const EGLint egl_attribs_gles[] = {
      WL_EGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_NONE,
   };
#endif

#ifdef HAVE_OPENGLES3
#ifdef EGL_KHR_create_context
   static const EGLint egl_attribs_gles3[] = {
      WL_EGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
      EGL_NONE,
   };
#endif
#endif

#endif

#ifdef HAVE_EGL
   static const EGLint egl_attribs_vg[] = {
      WL_EGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENVG_BIT,
      EGL_NONE,
   };

   EGLint major = 0, minor = 0;
   EGLint n;
   const EGLint *attrib_ptr = NULL;
#endif
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)
      calloc(1, sizeof(gfx_ctx_wayland_data_t));

   if (!wl)
      return NULL;

   (void)video_driver;

   switch (wl->egl.api)
   {
      case GFX_CTX_OPENGL_API:
#ifdef HAVE_OPENGL
         attrib_ptr = egl_attribs_gl;
#endif
         break;
      case GFX_CTX_OPENGL_ES_API:
#ifdef HAVE_OPENGLES
#ifdef HAVE_OPENGLES3
#ifdef EGL_KHR_create_context
         if (g_egl_major >= 3)
            attrib_ptr = egl_attribs_gles3;
         else
#endif
#endif
            attrib_ptr = egl_attribs_gles;
#endif
         break;
      case GFX_CTX_OPENVG_API:
#ifdef HAVE_VG
         attrib_ptr = egl_attribs_vg;
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   g_quit = 0;

   wl->dpy = wl_display_connect(NULL);
   if (!wl->dpy)
   {
      RARCH_ERR("Failed to connect to Wayland server.\n");
      goto error;
   }

   install_sighandlers();

   wl->registry = wl_display_get_registry(wl->dpy);
   wl_registry_add_listener(wl->registry, &registry_listener, wl);

   switch (wl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
      case GFX_CTX_OPENVG_API:
#ifdef HAVE_EGL
         wl_display_dispatch(wl->dpy);
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

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

   switch (wl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
      case GFX_CTX_OPENVG_API:
#ifdef HAVE_EGL
         if (!egl_init_context(wl, (EGLNativeDisplayType)wl->dpy,
                  &major, &minor, &n, attrib_ptr))
         {
            egl_report_error();
            goto error;
         }

         if (n == 0 || !egl_has_config(wl))
            goto error;
#endif
         break;
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         if (!vulkan_init_context(&wl->vk))
            goto error;
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   return wl;

error:
   gfx_ctx_wl_destroy_resources(wl);

   if (wl)
      free(wl);

   return NULL;
}

#ifdef HAVE_EGL
static EGLint *egl_fill_attribs(gfx_ctx_wayland_data_t *wl, EGLint *attr)
{
   switch (wl->egl.api)
   {
#ifdef EGL_KHR_create_context
      case GFX_CTX_OPENGL_API:
      {
#ifdef HAVE_OPENGL
         unsigned version = wl->egl.major * 1000 + wl->egl.minor;
         bool core = version >= 3001;
#ifdef GL_DEBUG
         bool debug = true;
#else
         const struct retro_hw_render_callback *hw_render =
            (const struct retro_hw_render_callback*)video_driver_callback();
         bool debug = hw_render->debug_context;
#endif

         if (core)
         {
            *attr++ = EGL_CONTEXT_MAJOR_VERSION_KHR;
            *attr++ = wl->egl.major;
            *attr++ = EGL_CONTEXT_MINOR_VERSION_KHR;
            *attr++ = wl->egl.minor;
            /* Technically, we don't have core/compat until 3.2.
             * Version 3.1 is either compat or not depending on GL_ARB_compatibility. */
            if (version >= 3002)
            {
               *attr++ = EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR;
               *attr++ = EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR;
            }
         }

         if (debug)
         {
            *attr++ = EGL_CONTEXT_FLAGS_KHR;
            *attr++ = EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;
         }
#endif

         break;
      }
#endif

      case GFX_CTX_OPENGL_ES_API:
#ifdef HAVE_OPENGLES
         *attr++ = EGL_CONTEXT_CLIENT_VERSION; /* Same as EGL_CONTEXT_MAJOR_VERSION */
         *attr++ = wl->egl.major ? (EGLint)wl->egl.major : 2;
#ifdef EGL_KHR_create_context
         if (wl->egl.minor > 0)
         {
            *attr++ = EGL_CONTEXT_MINOR_VERSION_KHR;
            *attr++ = wl->egl.minor;
         }
#endif
#endif
         break;

      case GFX_CTX_NONE:
      default:
         break;
   }

   *attr = EGL_NONE;
   return attr;
}
#endif

static void gfx_ctx_wl_destroy(void *data)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   if (!wl)
      return;

   gfx_ctx_wl_destroy_resources(wl);

   switch (wl_api)
   {
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         vulkan_destroy_context(&wl->vk);
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   free(wl);
}

static void gfx_ctx_wl_set_swap_interval(void *data, unsigned swap_interval)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   switch (wl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
      case GFX_CTX_OPENVG_API:
#ifdef HAVE_EGL
         egl_set_swap_interval(data, swap_interval);
#endif
         break;
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         if (wl->swap_interval != swap_interval)
         {
            wl->swap_interval = swap_interval;
            if (wl->vk.swapchain)
               wl->vk.need_new_swapchain = true;
         }
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

static bool gfx_ctx_wl_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
#ifdef HAVE_EGL
   EGLint egl_attribs[16];
   EGLint *attr              = egl_fill_attribs(
         (gfx_ctx_wayland_data_t*)data, egl_attribs);
#endif
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   wl->width        = width  ? width  : DEFAULT_WINDOWED_WIDTH;
   wl->height       = height ? height : DEFAULT_WINDOWED_HEIGHT;

   /* TODO: Use wl_output::scale to obtain correct value. */
   wl->buffer_scale = 1;

   wl->surface    = wl_compositor_create_surface(wl->compositor);

   switch (wl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
      case GFX_CTX_OPENVG_API:
#ifdef HAVE_EGL
         wl->win        = wl_egl_window_create(wl->surface, wl->width, wl->height);
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
   wl->shell_surf = wl_shell_get_shell_surface(wl->shell, wl->surface);

   wl_shell_surface_add_listener(wl->shell_surf, &shell_surface_listener, wl);
   wl_shell_surface_set_toplevel(wl->shell_surf);
   wl_shell_surface_set_class(wl->shell_surf, "RetroArch");
   wl_shell_surface_set_title(wl->shell_surf, "RetroArch");

   switch (wl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
      case GFX_CTX_OPENVG_API:
#ifdef HAVE_EGL

         if (!egl_create_context(wl, (attr != egl_attribs) ? egl_attribs : NULL))
         {
            egl_report_error();
            goto error;
         }

         if (!egl_create_surface(wl, (EGLNativeWindowType)wl->win))
            goto error;
         egl_set_swap_interval(wl, wl->egl.interval);
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   if (fullscreen)
      wl_shell_surface_set_fullscreen(wl->shell_surf,
            WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT, 0, NULL);

   flush_wayland_fd(wl);

   switch (wl_api)
   {
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         {
            VkWaylandSurfaceCreateInfoKHR wl_info = 
            { VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR };
            wl_display_roundtrip(wl->dpy);

            wl_info.display = wl->dpy;
            wl_info.surface = wl->surface;

            wl->vk.fpCreateWaylandSurfaceKHR(wl->vk.context.instance,
                  &wl_info, NULL, &wl->vk.vk_surface);

            if (!vulkan_create_swapchain(
                     &wl->vk, wl->width, wl->height, wl->swap_interval))
               goto error;
         }
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

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
   g_egl_major = major;
   g_egl_minor = minor;
   g_egl_api   = api;

   switch (api)
   {
      case GFX_CTX_OPENGL_API:
#ifdef HAVE_OPENGL
#ifndef EGL_KHR_create_context
         if ((major * 1000 + minor) >= 3001)
            return false;
#endif
         wl_api = api;
         return eglBindAPI(EGL_OPENGL_API);
#else
         break;
#endif
      case GFX_CTX_OPENGL_ES_API:
#ifdef HAVE_OPENGLES
#ifndef EGL_KHR_create_context
         if (major >= 3)
            return false;
#endif
         wl_api = api;
         return eglBindAPI(EGL_OPENGL_ES_API);
#else
         break;
#endif
      case GFX_CTX_OPENVG_API:
#ifdef HAVE_VG
         wl_api = api;
         return eglBindAPI(EGL_OPENVG_API);
#else
         break;
#endif
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         wl_api = api;
         return true;
#else
         break;
#endif
      case GFX_CTX_NONE:
      default:
         break;
   }

   return false;
}

static void keyboard_handle_keymap(void* data,
struct wl_keyboard* keyboard,
uint32_t format,
int fd,
uint32_t size)
{
   /* TODO */
}

static void keyboard_handle_enter(void* data,
struct wl_keyboard* keyboard,
uint32_t serial,
struct wl_surface* surface,
struct wl_array* keys)
{
   /* TODO */
}

static void keyboard_handle_leave(void* data,
struct wl_keyboard* keyboard,
uint32_t serial,
struct wl_surface* surface)
{
   /* TODO */
}

static void keyboard_handle_key(void* data,
struct wl_keyboard* keyboard,
uint32_t serial,
uint32_t time,
uint32_t key,
uint32_t state)
{
   /* TODO */
}

static void keyboard_handle_modifiers(void* data,
struct wl_keyboard* keyboard,
uint32_t serial,
uint32_t modsDepressed,
uint32_t modsLatched,
uint32_t modsLocked,
uint32_t group)
{
   /* TODO */
}

static const struct wl_keyboard_listener keyboard_listener = {
   keyboard_handle_keymap,
   keyboard_handle_enter,
   keyboard_handle_leave,
   keyboard_handle_key,
   keyboard_handle_modifiers,
};

static void pointer_handle_enter(void* data,
struct wl_pointer* pointer,
uint32_t serial,
struct wl_surface* surface,
wl_fixed_t sx,
wl_fixed_t sy)
{
   /* TODO */
}

static void pointer_handle_leave(void* data,
struct wl_pointer* pointer,
uint32_t serial,
struct wl_surface* surface)
{
   /* TODO */
}

static void pointer_handle_motion(void* data,
struct wl_pointer* pointer,
uint32_t time,
wl_fixed_t sx,
wl_fixed_t sy)
{
   /* TODO */
}

static void pointer_handle_button(void* data,
struct wl_pointer* wl_pointer,
uint32_t serial,
uint32_t time,
uint32_t button,
uint32_t state)
{
   /* TODO */
}

static void pointer_handle_axis(void* data,
struct wl_pointer* wl_pointer,
uint32_t time,
uint32_t axis,
wl_fixed_t value)
{
   /* TODO */
}


static const struct wl_pointer_listener pointer_listener = {
   pointer_handle_enter,
   pointer_handle_leave,
   pointer_handle_motion,
   pointer_handle_button,
   pointer_handle_axis,
};

static void seat_handle_capabilities(void *data,
struct wl_seat *seat, unsigned caps)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !wl->wl_keyboard)
   {
      wl->wl_keyboard = wl_seat_get_keyboard(seat);
      wl_keyboard_add_listener(wl->wl_keyboard, &keyboard_listener, NULL);
   }
   else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && wl->wl_keyboard)
   {
      wl_keyboard_destroy(wl->wl_keyboard);
      wl->wl_keyboard = NULL;
   }
   if ((caps & WL_SEAT_CAPABILITY_POINTER) && !wl->wl_pointer)
   {
      wl->wl_pointer = wl_seat_get_pointer(seat);
      wl_pointer_add_listener(wl->wl_pointer, &pointer_listener, NULL);
   }
   else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && wl->wl_pointer)
   {
      wl_pointer_destroy(wl->wl_pointer);
      wl->wl_pointer = NULL;
   }
}

/* Seat callbacks - TODO/FIXME */
static const struct wl_seat_listener seat_listener = {
   seat_handle_capabilities,
};


#ifdef HAVE_VULKAN
static void *gfx_ctx_wl_get_context_data(void *data)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   return &wl->vk.context;
}

static void vulkan_acquire_next_image(gfx_ctx_vulkan_data_t *vk)
{
   unsigned index;
   VkResult err;
   VkFence fence;
   VkFence *next_fence;
   VkSemaphoreCreateInfo sem_info = 
   { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
   VkFenceCreateInfo info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };

   vkCreateFence(vk->context.device, &info, NULL, &fence);

   err = vk->fpAcquireNextImageKHR(vk->context.device,
         vk->swapchain, UINT64_MAX,
         VK_NULL_HANDLE, fence, &vk->context.current_swapchain_index);

   index = vk->context.current_swapchain_index;
   if (vk->context.swapchain_semaphores[index] == VK_NULL_HANDLE)
      vkCreateSemaphore(vk->context.device, &sem_info,
            NULL, &vk->context.swapchain_semaphores[index]);

   vkWaitForFences(vk->context.device, 1, &fence, true, UINT64_MAX);
   vkDestroyFence(vk->context.device, fence, NULL);

   next_fence = &vk->context.swapchain_fences[index];
   if (*next_fence != VK_NULL_HANDLE)
   {
      vkWaitForFences(vk->context.device, 1, next_fence, true, UINT64_MAX);
      vkResetFences(vk->context.device, 1, next_fence);
   }
   else
      vkCreateFence(vk->context.device, &info, NULL, next_fence);

   if (err != VK_SUCCESS)
   {
      RARCH_LOG("[Wayland/Vulkan]: AcquireNextImage failed, invalidating swapchain.\n");
      vk->context.invalid_swapchain = true;
   }
}

static bool vulkan_create_swapchain(gfx_ctx_vulkan_data_t *vk,
      unsigned width, unsigned height,
      unsigned swap_interval)
{
   unsigned i;
   uint32_t format_count;
   uint32_t desired_swapchain_images;
   VkSurfaceCapabilitiesKHR surface_properties;
   VkSurfaceFormatKHR formats[256];
   VkSurfaceFormatKHR format;
   VkExtent2D swapchain_size;
   VkSwapchainKHR old_swapchain;
   VkSurfaceTransformFlagBitsKHR pre_transform;
   VkPresentModeKHR swapchain_present_mode = swap_interval 
      ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR;
   VkSwapchainCreateInfoKHR info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };

   RARCH_LOG("[Wayland/Vulkan]: Creating swapchain with present mode: %u\n",
         (unsigned)swapchain_present_mode);

   vk->fpGetPhysicalDeviceSurfaceCapabilitiesKHR(vk->context.gpu,
         vk->vk_surface, &surface_properties);
   vk->fpGetPhysicalDeviceSurfaceFormatsKHR(vk->context.gpu,
         vk->vk_surface, &format_count, NULL);
   vk->fpGetPhysicalDeviceSurfaceFormatsKHR(vk->context.gpu,
         vk->vk_surface, &format_count, formats);

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
      swapchain_size.width     = width;
      swapchain_size.height    = height;
   }
   else
      swapchain_size           = surface_properties.currentExtent;

   desired_swapchain_images    = surface_properties.minImageCount + 1;
   if ((surface_properties.maxImageCount > 0) 
         && (desired_swapchain_images > surface_properties.maxImageCount))
      desired_swapchain_images = surface_properties.maxImageCount;

   if (surface_properties.supportedTransforms 
         & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
      pre_transform            = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
   else
      pre_transform            = surface_properties.currentTransform;

   old_swapchain               = vk->swapchain;

   info.surface                = vk->vk_surface;
   info.minImageCount          = desired_swapchain_images;
   info.imageFormat            = format.format;
   info.imageColorSpace        = format.colorSpace;
   info.imageExtent.width      = swapchain_size.width;
   info.imageExtent.height     = swapchain_size.height;
   info.imageArrayLayers       = 1;
   info.imageUsage             = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT 
      | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
   info.imageSharingMode       = VK_SHARING_MODE_EXCLUSIVE;
   info.preTransform           = pre_transform;
   info.compositeAlpha         = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
   info.presentMode            = swapchain_present_mode;
   info.clipped                = true;
   info.oldSwapchain           = old_swapchain;

   vk->fpCreateSwapchainKHR(vk->context.device, &info, NULL, &vk->swapchain);
   if (old_swapchain != VK_NULL_HANDLE)
      vk->fpDestroySwapchainKHR(vk->context.device, old_swapchain, NULL);

   vk->context.swapchain_width  = swapchain_size.width;
   vk->context.swapchain_height = swapchain_size.height;

   /* Make sure we create a backbuffer format that is as we expect. */
   switch (format.format)
   {
      case VK_FORMAT_B8G8R8A8_SRGB:
         vk->context.swapchain_format  = VK_FORMAT_B8G8R8A8_UNORM;
         vk->context.swapchain_is_srgb = true;
         break;

      case VK_FORMAT_R8G8B8A8_SRGB:
         vk->context.swapchain_format  = VK_FORMAT_R8G8B8A8_UNORM;
         vk->context.swapchain_is_srgb = true;
         break;

      case VK_FORMAT_R8G8B8_SRGB:
         vk->context.swapchain_format  = VK_FORMAT_R8G8B8_UNORM;
         vk->context.swapchain_is_srgb = true;
         break;

      case VK_FORMAT_B8G8R8_SRGB:
         vk->context.swapchain_format  = VK_FORMAT_B8G8R8_UNORM;
         vk->context.swapchain_is_srgb = true;
         break;

      default:
         vk->context.swapchain_format  = format.format;
         break;
   }

   vk->fpGetSwapchainImagesKHR(vk->context.device, vk->swapchain,
         &vk->context.num_swapchain_images, NULL);
   vk->fpGetSwapchainImagesKHR(vk->context.device, vk->swapchain,
         &vk->context.num_swapchain_images, vk->context.swapchain_images);

   for (i = 0; i < vk->context.num_swapchain_images; i++)
   {
      if (vk->context.swapchain_fences[i])
      {
         vkDestroyFence(vk->context.device,
               vk->context.swapchain_fences[i], NULL);
         vk->context.swapchain_fences[i] = VK_NULL_HANDLE;
      }
   }

   vulkan_acquire_next_image(vk);

   return true;
}

static void vulkan_present(gfx_ctx_vulkan_data_t *vk, unsigned index)
{
   VkResult result            = VK_SUCCESS;
   VkResult err               = VK_SUCCESS;
   VkPresentInfoKHR present   = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
   present.swapchainCount     = 1;
   present.pSwapchains        = &vk->swapchain;
   present.pImageIndices      = &index;
   present.pResults           = &result;
   present.waitSemaphoreCount = 1;
   present.pWaitSemaphores    = &vk->context.swapchain_semaphores[index];

   /* Better hope QueuePresent doesn't block D: */
   slock_lock(vk->context.queue_lock);
   err = vk->fpQueuePresentKHR(vk->context.queue, &present);

   if (err != VK_SUCCESS || result != VK_SUCCESS)
   {
      RARCH_LOG("[Wayland/Vulkan]: QueuePresent failed, invalidating swapchain.\n");
      vk->context.invalid_swapchain = true;
   }
   slock_unlock(vk->context.queue_lock);
}
#endif

static void gfx_ctx_wl_swap_buffers(void *data)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   switch (wl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
      case GFX_CTX_OPENVG_API:
#ifdef HAVE_EGL
         egl_swap_buffers(data);
#endif
         break;
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         vulkan_present(&wl->vk, wl->vk.context.current_swapchain_index);
         vulkan_acquire_next_image(&wl->vk);
         flush_wayland_fd(wl);
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

static gfx_ctx_proc_t gfx_ctx_wl_get_proc_address(const char *symbol)
{
   switch (wl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
      case GFX_CTX_OPENVG_API:
#ifdef HAVE_EGL
         return egl_get_proc_address(symbol);
#else
         break;
#endif
      case GFX_CTX_NONE:
      default:
         break;
   }

   return NULL;
}

static void gfx_ctx_wl_bind_hw_render(void *data, bool enable)
{
   switch (wl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
      case GFX_CTX_OPENVG_API:
#ifdef HAVE_EGL
         egl_bind_hw_render(data, enable);
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

const gfx_ctx_driver_t gfx_ctx_wayland = {
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
   gfx_ctx_wl_get_proc_address,
   NULL,
   NULL,
   NULL,
   "wayland",
   gfx_ctx_wl_bind_hw_render,
#ifdef HAVE_VULKAN
   gfx_ctx_wl_get_context_data
#else
   NULL
#endif
};
