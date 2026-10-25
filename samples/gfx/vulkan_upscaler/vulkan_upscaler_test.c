/* gfx/common/vulkan_ngx.c and gfx/video_upscaler.c against a stand-in
 * NGX core library (fake_ngx.c) on a real Vulkan device (lavapipe),
 * under the validation layer where it is installed.
 *
 * What is pinned: the session comes up from the loaded library with
 * the project id and path list; the mode's render sizes reach the
 * core's request with the mip bias; a frame records a feature made for
 * the mode's full range and an evaluate whose resources carry the
 * core's images, formats and sizes, the frame's size as the render
 * subrect, the jitter, scales and reset; a depth image left in a depth
 * layout is moved and moved back; a resize makes a new feature and
 * output and retires the old ones, which go after the frames in flight;
 * frames outside the range, without inputs or after a failed evaluate
 * are refused and the core's image shown; nothing of it is reachable
 * with the setting off. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <vulkan/vulkan_symbol_wrapper.h>
#include <gfx/nvidia/ngx_loader.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

/* The loader's entry point: linked on Linux, from vulkan-1.dll on
 * Windows, where mingw has no import library for it */
#undef vkGetInstanceProcAddr
#if defined(_WIN32)
static PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
static bool load_vulkan(void)
{
   HMODULE m = LoadLibraryA("vulkan-1.dll");
   FARPROC p = m ? GetProcAddress(m, "vkGetInstanceProcAddr") : NULL;
   memcpy(&vkGetInstanceProcAddr, &p, sizeof(p));
   return vkGetInstanceProcAddr != NULL;
}
#else
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(
      VkInstance instance, const char *pName);
static bool load_vulkan(void) { return true; }
#endif

#include "../../../configuration.h"
#include "../../../gfx/video_upscaler.h"
#include "../../../gfx/common/vulkan_ngx.h"
#include "fake_ngx.h"

static int failures;
static int validation_errors;

#define CHECK(cond) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
         failures++; \
      } \
   } while (0)

typedef struct image
{
   VkImage image;
   VkImageView view;
   VkDeviceMemory memory;
   VkFormat format;
   VkImageLayout layout;
   unsigned width, height;
} image_t;

static VkInstance instance;
static VkPhysicalDevice gpu;
static VkDevice device;
static VkQueue queue;
static uint32_t queue_family;
static VkCommandPool pool;
static VkCommandBuffer cmd;
static VkPhysicalDeviceMemoryProperties mem_props;
static VkDebugUtilsMessengerEXT messenger;
static PFN_vkCreateDebugUtilsMessengerEXT create_messenger;
static PFN_vkDestroyDebugUtilsMessengerEXT destroy_messenger;

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_cb(
      VkDebugUtilsMessageSeverityFlagBitsEXT severity,
      VkDebugUtilsMessageTypeFlagsEXT types,
      const VkDebugUtilsMessengerCallbackDataEXT *data, void *user)
{
   (void)types; (void)user;
   if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
   {
      validation_errors++;
      printf("validation: %s\n", data->pMessage);
   }
   return VK_FALSE;
}

static bool have_layer(const char *name)
{
   uint32_t n = 0, i;
   VkLayerProperties *props;
   bool found = false;
   vkEnumerateInstanceLayerProperties(&n, NULL);
   if (!n)
      return false;
   props = (VkLayerProperties*)malloc(n * sizeof(*props));
   vkEnumerateInstanceLayerProperties(&n, props);
   for (i = 0; i < n; i++)
      if (!strcmp(props[i].layerName, name))
         found = true;
   free(props);
   return found;
}

static bool setup_vulkan(void)
{
   VkApplicationInfo app;
   VkInstanceCreateInfo iinfo;
   VkDeviceQueueCreateInfo qinfo;
   VkDeviceCreateInfo dinfo;
   VkCommandPoolCreateInfo pinfo;
   VkCommandBufferAllocateInfo ainfo;
   VkDebugUtilsMessengerCreateInfoEXT minfo;
   const char *layers[1]  = { "VK_LAYER_KHRONOS_validation" };
   const char *exts[1]    = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };
   const char *dev_exts[VULKAN_NGX_MAX_EXTS];
   unsigned num_dev_exts  = 0;
   bool validation;
   static const float prio = 1.0f;
   uint32_t count = 0, i;
   VkPhysicalDevice gpus[8];
   VkQueueFamilyProperties families[16];

   if (!load_vulkan())
   {
      printf("no Vulkan loader\n");
      return false;
   }
   vulkan_symbol_wrapper_init(vkGetInstanceProcAddr);
   vulkan_symbol_wrapper_load_global_symbols();
   validation = have_layer(layers[0]);

   app.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
   app.pNext              = NULL;
   app.pApplicationName   = "vulkan_upscaler_test";
   app.applicationVersion = 0;
   app.pEngineName        = "RetroArch";
   app.engineVersion      = 0;
   app.apiVersion         = VK_API_VERSION_1_1;
   iinfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
   iinfo.pNext                   = NULL;
   iinfo.flags                   = 0;
   iinfo.pApplicationInfo        = &app;
   iinfo.enabledLayerCount       = validation ? 1 : 0;
   iinfo.ppEnabledLayerNames     = layers;
   iinfo.enabledExtensionCount   = validation ? 1 : 0;
   iinfo.ppEnabledExtensionNames = exts;
   if (vkCreateInstance(&iinfo, NULL, &instance) != VK_SUCCESS)
   {
      printf("no Vulkan instance\n");
      return false;
   }
   vulkan_symbol_wrapper_load_core_instance_symbols(instance);
   if (validation)
   {
      create_messenger  = (PFN_vkCreateDebugUtilsMessengerEXT)
         vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
      destroy_messenger = (PFN_vkDestroyDebugUtilsMessengerEXT)
         vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
      memset(&minfo, 0, sizeof(minfo));
      minfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
      minfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
      minfo.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
      minfo.pfnUserCallback = debug_cb;
      if (create_messenger)
         create_messenger(instance, &minfo, NULL, &messenger);
      printf("validation layer: on\n");
   }
   else
      printf("validation layer: not installed\n");

   count = 8;
   if (vkEnumeratePhysicalDevices(instance, &count, gpus) < VK_SUCCESS || !count)
   {
      printf("no Vulkan device\n");
      return false;
   }
   gpu   = gpus[0];
   count = 16;
   vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, families);
   queue_family = count;
   for (i = 0; i < count; i++)
      if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
      {
         queue_family = i;
         break;
      }
   if (queue_family == count)
      return false;
   vkGetPhysicalDeviceMemoryProperties(gpu, &mem_props);

   /* The upscaler's extensions, the device's among them: the fake asks
    * for one lavapipe has and one it has not */
   num_dev_exts = vulkan_ngx_device_extensions(gpu, vkGetInstanceProcAddr,
         instance, dev_exts, 0, VULKAN_NGX_MAX_EXTS);
   CHECK(num_dev_exts == 1 && !strcmp(dev_exts[0], "VK_KHR_maintenance1"));
   /* already there: not twice */
   dev_exts[0]  = "VK_KHR_maintenance1";
   num_dev_exts = vulkan_ngx_device_extensions(gpu, vkGetInstanceProcAddr,
         instance, dev_exts, 1, VULKAN_NGX_MAX_EXTS);
   CHECK(num_dev_exts == 1);

   qinfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
   qinfo.pNext            = NULL;
   qinfo.flags            = 0;
   qinfo.queueFamilyIndex = queue_family;
   qinfo.queueCount       = 1;
   qinfo.pQueuePriorities = &prio;
   dinfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
   dinfo.pNext                   = NULL;
   dinfo.flags                   = 0;
   dinfo.queueCreateInfoCount    = 1;
   dinfo.pQueueCreateInfos       = &qinfo;
   dinfo.enabledLayerCount       = 0;
   dinfo.ppEnabledLayerNames     = NULL;
   dinfo.enabledExtensionCount   = num_dev_exts;
   dinfo.ppEnabledExtensionNames = dev_exts;
   dinfo.pEnabledFeatures        = NULL;
   if (vkCreateDevice(gpu, &dinfo, NULL, &device) != VK_SUCCESS)
      return false;
   vulkan_symbol_wrapper_load_core_device_symbols(device);
   vkGetDeviceQueue(device, queue_family, 0, &queue);

   pinfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   pinfo.pNext            = NULL;
   pinfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
   pinfo.queueFamilyIndex = queue_family;
   vkCreateCommandPool(device, &pinfo, NULL, &pool);
   ainfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   ainfo.pNext              = NULL;
   ainfo.commandPool        = pool;
   ainfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   ainfo.commandBufferCount = 1;
   vkAllocateCommandBuffers(device, &ainfo, &cmd);
   return true;
}

static uint32_t memory_type(uint32_t bits)
{
   uint32_t i;
   for (i = 0; i < mem_props.memoryTypeCount; i++)
      if (bits & (1u << i))
         return i;
   return 0;
}

static bool make_image(image_t *img, VkFormat format, unsigned w, unsigned h,
      VkImageUsageFlags usage, VkImageAspectFlags aspect)
{
   VkImageCreateInfo info;
   VkImageViewCreateInfo view;
   VkMemoryRequirements reqs;
   VkMemoryAllocateInfo alloc;
   memset(img, 0, sizeof(*img));
   info.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   info.pNext                 = NULL;
   info.flags                 = 0;
   info.imageType             = VK_IMAGE_TYPE_2D;
   info.format                = format;
   info.extent.width          = w;
   info.extent.height         = h;
   info.extent.depth          = 1;
   info.mipLevels             = 1;
   info.arrayLayers           = 1;
   info.samples               = VK_SAMPLE_COUNT_1_BIT;
   info.tiling                = VK_IMAGE_TILING_OPTIMAL;
   info.usage                 = usage;
   info.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
   info.queueFamilyIndexCount = 0;
   info.pQueueFamilyIndices   = NULL;
   info.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
   if (vkCreateImage(device, &info, NULL, &img->image) != VK_SUCCESS)
      return false;
   vkGetImageMemoryRequirements(device, img->image, &reqs);
   alloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   alloc.pNext           = NULL;
   alloc.allocationSize  = reqs.size;
   alloc.memoryTypeIndex = memory_type(reqs.memoryTypeBits);
   if (vkAllocateMemory(device, &alloc, NULL, &img->memory) != VK_SUCCESS)
      return false;
   vkBindImageMemory(device, img->image, img->memory, 0);
   view.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   view.pNext                           = NULL;
   view.flags                           = 0;
   view.image                           = img->image;
   view.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
   view.format                          = format;
   view.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
   view.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
   view.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
   view.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
   view.subresourceRange.aspectMask     = aspect;
   view.subresourceRange.baseMipLevel   = 0;
   view.subresourceRange.levelCount     = 1;
   view.subresourceRange.baseArrayLayer = 0;
   view.subresourceRange.layerCount     = 1;
   if (vkCreateImageView(device, &view, NULL, &img->view) != VK_SUCCESS)
      return false;
   img->format = format;
   img->width  = w;
   img->height = h;
   return true;
}

static void free_image(image_t *img)
{
   if (img->view)
      vkDestroyImageView(device, img->view, NULL);
   if (img->image)
      vkDestroyImage(device, img->image, NULL);
   if (img->memory)
      vkFreeMemory(device, img->memory, NULL);
   memset(img, 0, sizeof(*img));
}

/* UNDEFINED -> @layout, the way the core leaves its images */
static void place_image(image_t *img, VkImageLayout layout,
      VkImageAspectFlags aspect)
{
   VkImageMemoryBarrier b;
   b.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
   b.pNext                           = NULL;
   b.srcAccessMask                   = 0;
   b.dstAccessMask                   = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
   b.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
   b.newLayout                       = layout;
   b.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
   b.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
   b.image                           = img->image;
   b.subresourceRange.aspectMask     = aspect;
   b.subresourceRange.baseMipLevel   = 0;
   b.subresourceRange.levelCount     = 1;
   b.subresourceRange.baseArrayLayer = 0;
   b.subresourceRange.layerCount     = 1;
   vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1, &b);
   img->layout = layout;
}

static void begin_frame(void)
{
   VkCommandBufferBeginInfo begin;
   begin.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   begin.pNext            = NULL;
   begin.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
   begin.pInheritanceInfo = NULL;
   vkResetCommandBuffer(cmd, 0);
   vkBeginCommandBuffer(cmd, &begin);
}

static void end_frame(void)
{
   VkSubmitInfo submit;
   vkEndCommandBuffer(cmd);
   memset(&submit, 0, sizeof(submit));
   submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submit.commandBufferCount = 1;
   submit.pCommandBuffers    = &cmd;
   vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
   /* A test: the frame is over before the next is recorded */
   vkQueueWaitIdle(queue);
}

static fake_ngx_record_t *record(void)
{
   const ngx_lib_t *lib = ngx_lib_get();
   fake_ngx_record_t *(*get)(void) = NULL;
   if (!lib)
      return NULL;
#if defined(_WIN32)
   {
      FARPROC p = GetProcAddress((HMODULE)lib->handle, "fake_ngx_record");
      memcpy(&get, &p, sizeof(get));
   }
#else
   {
      void *p = dlsym(lib->handle, "fake_ngx_record");
      memcpy(&get, &p, sizeof(get));
   }
#endif
   return get ? get() : NULL;
}

static void set_input(struct retro_hw_upscaler_image *dst, const image_t *src)
{
   dst->image  = (uint64_t)(uintptr_t)src->image;
   dst->view   = (uint64_t)(uintptr_t)src->view;
   dst->format = (unsigned)src->format;
   dst->state  = (unsigned)src->layout;
   dst->width  = src->width;
   dst->height = src->height;
}

static bool optimal_cb(void *data, unsigned mode, unsigned out_w,
      unsigned out_h, unsigned *rw, unsigned *rh, unsigned *mnw,
      unsigned *mnh, unsigned *mxw, unsigned *mxh)
{
   return vulkan_ngx_optimal((vulkan_ngx_t*)data, mode, out_w, out_h,
         rw, rh, mnw, mnh, mxw, mxh);
}

int main(void)
{
   settings_t *settings = (settings_t*)config_get_ptr();
   fake_ngx_record_t *rec;
   vulkan_ngx_t *ngx;
   image_t color, mv, depth, color_big;
   vulkan_ngx_image_t in, out;
   struct retro_hw_upscaler_inputs inputs;
   struct retro_hw_upscaler_request req;
   video_upscaler_backend_t backend;
   const struct retro_hw_render_upscaler_interface *iface;
   unsigned rw, rh, mnw, mnh, mxw, mxh;
   unsigned i;

   if (!setup_vulkan())
   {
      printf("vulkan_upscaler_test: no Vulkan device, skipped\n");
      return 0;
   }
   rec = record();
   CHECK(rec != NULL);
   if (!rec)
      return 1;

   /* Off: no interface for the core, whatever the backend */
   settings->uints.video_temporal_upscaler = 0;
   CHECK(video_upscaler_get_interface() == NULL);
   strcpy(settings->paths.directory_dlss, "Zdir");

   /* The session: init with the project, the path list carrying the
    * DLSS directory, the capability set read */
   settings->uints.video_temporal_upscaler = VIDEO_TEMPORAL_UPSCALER_DLSS;
   rec->unavailable = 1;
   CHECK(vulkan_ngx_init(instance, gpu, device, vkGetInstanceProcAddr,
            vkGetDeviceProcAddr, &mem_props, "Zdir") == NULL);
   CHECK(rec->init_calls == 1 && rec->shutdown_calls == 1);
   rec->unavailable = 0;
   ngx = vulkan_ngx_init(instance, gpu, device, vkGetInstanceProcAddr,
         vkGetDeviceProcAddr, &mem_props, "Zdir");
   CHECK(ngx != NULL);
   if (!ngx)
      return 1;
   CHECK(rec->init_calls == 2 && rec->init_device == (void*)device);
   CHECK(rec->init_path0 == 'Z');

   /* The mode's sizes, through the capability callback, cached */
   CHECK(vulkan_ngx_optimal(ngx, NGX_PERF_QUALITY_MAX_QUALITY, 1920, 1080,
            &rw, &rh, &mnw, &mnh, &mxw, &mxh));
   CHECK(rw == 1280 && rh == 720 && mxw == 1920 && mxh == 1080 && mnw == 640 && mnh == 360);
   CHECK(vulkan_ngx_optimal(ngx, NGX_PERF_QUALITY_MAX_QUALITY, 1920, 1080,
            &rw, &rh, &mnw, &mnh, &mxw, &mxh));
   CHECK(rec->optimal_calls == 1);
   CHECK(!vulkan_ngx_optimal(ngx, NGX_PERF_QUALITY_ULTRA_QUALITY, 1920, 1080,
            &rw, &rh, &mnw, &mnh, &mxw, &mxh));
   CHECK(!vulkan_ngx_optimal(ngx, NGX_PERF_QUALITY_MAX_QUALITY, 0, 1080,
            &rw, &rh, &mnw, &mnh, &mxw, &mxh));

   /* The core's request, through the frontend module */
   backend.data    = ngx;
   backend.optimal = optimal_cb;
   video_upscaler_attach(&backend);
   iface = video_upscaler_get_interface();
   CHECK(iface && iface->interface_version == RETRO_HW_RENDER_UPSCALER_INTERFACE_VERSION);
   memset(&req, 0, sizeof(req));
   CHECK(!iface->get_request(iface->handle, &req));   /* no output size yet */
   video_upscaler_set_output(1920, 1080);
   settings->uints.video_temporal_upscaler_quality = VIDEO_TEMPORAL_UPSCALER_QUALITY_AUTO;
   CHECK(iface->get_request(iface->handle, &req));
   CHECK(req.output_width == 1920 && req.output_height == 1080);
   CHECK(req.render_width == 1280 && req.render_height == 720);
   CHECK(req.min_render_width == 640 && req.max_render_width == 1920);
   CHECK(fabs(req.mip_bias - (log(1280.0 / 1920.0) / log(2.0) - 1.0)) < 1e-5);
   settings->uints.video_temporal_upscaler_quality = VIDEO_TEMPORAL_UPSCALER_QUALITY_PERFORMANCE;
   CHECK(iface->get_request(iface->handle, &req) && req.render_width == 960);
   settings->uints.video_temporal_upscaler_quality = VIDEO_TEMPORAL_UPSCALER_QUALITY_NATIVE;
   CHECK(iface->get_request(iface->handle, &req) && req.render_width == 1920
         && fabs(req.mip_bias + 1.0) < 1e-5);
   video_upscaler_set_output(3840, 2160);
   settings->uints.video_temporal_upscaler_quality = VIDEO_TEMPORAL_UPSCALER_QUALITY_AUTO;
   CHECK(iface->get_request(iface->handle, &req) && req.render_width == (unsigned)(3840 * 0.58));
   settings->uints.video_temporal_upscaler = 0;
   CHECK(!iface->get_request(iface->handle, &req));
   settings->uints.video_temporal_upscaler = VIDEO_TEMPORAL_UPSCALER_DLSS;
   settings->uints.video_temporal_upscaler_quality = VIDEO_TEMPORAL_UPSCALER_QUALITY_QUALITY;
   video_upscaler_set_output(1920, 1080);

   /* The inputs: set before the frame, taken in it, once */
   memset(&inputs, 0, sizeof(inputs));
   CHECK(video_upscaler_take_inputs() == NULL);
   iface->set_inputs(iface->handle, &inputs);
   CHECK(video_upscaler_take_inputs() == &inputs);
   CHECK(video_upscaler_take_inputs() == NULL);

   /* A frame: color at the mode's render size, the depth left in a
    * depth layout */
   CHECK(make_image(&color, VK_FORMAT_R8G8B8A8_UNORM, 1280, 720,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT));
   CHECK(make_image(&mv, VK_FORMAT_R16G16_SFLOAT, 1280, 720,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT));
   CHECK(make_image(&depth, VK_FORMAT_D32_SFLOAT, 1280, 720,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT));
   CHECK(make_image(&color_big, VK_FORMAT_R8G8B8A8_UNORM, 2048, 1152,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT));
   begin_frame();
   place_image(&color, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
   place_image(&mv, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
   place_image(&depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
   place_image(&color_big, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
   end_frame();

   in.image  = color.image;
   in.view   = color.view;
   in.layout = color.layout;
   in.format = color.format;
   in.width  = color.width;
   in.height = color.height;
   set_input(&inputs.motion_vectors, &mv);
   set_input(&inputs.depth, &depth);
   inputs.flags      = RETRO_HW_UPSCALER_DEPTH_INVERTED | RETRO_HW_UPSCALER_MV_JITTERED;
   inputs.jitter_x   = 0.25f;
   inputs.jitter_y   = -0.125f;
   inputs.mv_scale_x = 0.0f;       /* reads as 1 */
   inputs.mv_scale_y = -1.0f;
   inputs.frame_time_ms = 16.6f;

   begin_frame();
   memset(&out, 0, sizeof(out));
   CHECK(vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY,
            1920, 1080, &out));
   end_frame();
   vulkan_ngx_frame_done(ngx);
   CHECK(rec->creates == 1 && rec->evals == 1);
   /* The feature: the mode's full range in, the output size out, the
    * flags from the inputs with low-res MVs and automatic exposure */
   CHECK(rec->create[0].width == 1920 && rec->create[0].height == 1080);
   CHECK(rec->create[0].out_width == 1920 && rec->create[0].out_height == 1080);
   CHECK(rec->create[0].quality == NGX_PERF_QUALITY_MAX_QUALITY);
   CHECK(rec->create[0].flags == (NGX_DLSS_FLAG_MV_LOW_RES | NGX_DLSS_FLAG_MV_JITTERED
            | NGX_DLSS_FLAG_DEPTH_INVERTED | NGX_DLSS_FLAG_AUTO_EXPOSURE));
   CHECK(rec->create[0].node_mask == 0x101);
   /* The evaluate: the images as given, the output ours */
   CHECK(rec->eval[0].color.present && rec->eval[0].color.image == (uint64_t)(uintptr_t)color.image
         && rec->eval[0].color.view == (uint64_t)(uintptr_t)color.view
         && rec->eval[0].color.format == (int)VK_FORMAT_R8G8B8A8_UNORM
         && rec->eval[0].color.width == 1280 && rec->eval[0].color.height == 720
         && !rec->eval[0].color.read_write);
   CHECK(rec->eval[0].depth.image == (uint64_t)(uintptr_t)depth.image
         && rec->eval[0].depth.aspect == VK_IMAGE_ASPECT_DEPTH_BIT
         && rec->eval[0].depth.format == (int)VK_FORMAT_D32_SFLOAT);
   CHECK(rec->eval[0].motion_vectors.image == (uint64_t)(uintptr_t)mv.image
         && rec->eval[0].motion_vectors.format == (int)VK_FORMAT_R16G16_SFLOAT);
   CHECK(rec->eval[0].output.present && rec->eval[0].output.read_write
         && rec->eval[0].output.image == (uint64_t)(uintptr_t)out.image
         && rec->eval[0].output.view == (uint64_t)(uintptr_t)out.view
         && rec->eval[0].output.width == 1920 && rec->eval[0].output.height == 1080
         && rec->eval[0].output.format == (int)VK_FORMAT_R8G8B8A8_UNORM);
   CHECK(!rec->eval[0].exposure.present && !rec->eval[0].transparency.present);
   CHECK(rec->eval[0].jitter_x == 0.25f && rec->eval[0].jitter_y == -0.125f);
   CHECK(rec->eval[0].mv_scale_x == 1.0f && rec->eval[0].mv_scale_y == -1.0f);
   CHECK(rec->eval[0].pre_exposure == 1.0f && rec->eval[0].exposure_scale == 1.0f);
   CHECK(rec->eval[0].frame_time == 16.6f);
   CHECK(rec->eval[0].reset == 1);   /* a new feature starts over */
   CHECK(rec->eval[0].subrect_w == 1280 && rec->eval[0].subrect_h == 720);
   CHECK(out.width == 1920 && out.height == 1080 && out.format == VK_FORMAT_R8G8B8A8_UNORM
         && out.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

   /* The next frame: the same feature, no reset, the core's reset
    * honoured when asked */
   begin_frame();
   CHECK(vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY,
            1920, 1080, &out));
   end_frame();
   vulkan_ngx_frame_done(ngx);
   CHECK(rec->creates == 1 && rec->evals == 2 && rec->eval[1].reset == 0);
   inputs.flags |= RETRO_HW_UPSCALER_RESET;
   begin_frame();
   CHECK(vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY,
            1920, 1080, &out));
   end_frame();
   vulkan_ngx_frame_done(ngx);
   CHECK(rec->evals == 3 && rec->eval[2].reset == 1 && rec->creates == 1);
   inputs.flags &= ~RETRO_HW_UPSCALER_RESET;

   /* A smaller frame within the range: the same feature, the subrect
    * says the size; one outside: refused */
   in.width  = 960;
   in.height = 540;
   begin_frame();
   CHECK(vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY,
            1920, 1080, &out));
   end_frame();
   vulkan_ngx_frame_done(ngx);
   CHECK(rec->creates == 1 && rec->evals == 4
         && rec->eval[3].subrect_w == 960 && rec->eval[3].subrect_h == 540);
   in.image  = color_big.image;
   in.view   = color_big.view;
   in.width  = color_big.width;
   in.height = color_big.height;
   begin_frame();
   CHECK(!vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY,
            1920, 1080, &out));
   end_frame();
   vulkan_ngx_frame_done(ngx);
   CHECK(rec->evals == 4);
   in.image  = color.image;
   in.view   = color.view;
   in.width  = color.width;
   in.height = color.height;

   /* No inputs, no motion vectors: refused */
   CHECK(!vulkan_ngx_upscale(ngx, cmd, &in, NULL, NGX_PERF_QUALITY_MAX_QUALITY,
            1920, 1080, &out));
   inputs.motion_vectors.image = 0;
   CHECK(!vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY,
            1920, 1080, &out));
   set_input(&inputs.motion_vectors, &mv);

   /* A resize: a new feature and output, the old retired and released
    * once the frames that could read them are through */
   begin_frame();
   CHECK(vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY,
            2560, 1440, &out));
   end_frame();
   vulkan_ngx_frame_done(ngx);
   CHECK(rec->creates == 2 && rec->create[1].width == 2560 && rec->create[1].out_width == 2560);
   CHECK(out.width == 2560 && out.height == 1440);
   CHECK(rec->releases == 0);
   CHECK(rec->eval[4].reset == 1);
   for (i = 0; i < 30 && rec->releases == 0; i++)
      vulkan_ngx_frame_done(ngx);
   CHECK(rec->releases == 1 && rec->last_released == 1);
   CHECK(i >= 15);

   /* A mode change makes a new feature too */
   begin_frame();
   CHECK(vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_BALANCED,
            2560, 1440, &out));
   end_frame();
   vulkan_ngx_frame_done(ngx);
   CHECK(rec->creates == 3 && rec->create[2].quality == NGX_PERF_QUALITY_BALANCED);

   /* An evaluate that fails: the frame shown as it is, the feature left
    * alone until something new is asked of it */
   rec->eval_fails = 1;
   begin_frame();
   CHECK(!vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_BALANCED,
            2560, 1440, &out));
   end_frame();
   vulkan_ngx_frame_done(ngx);
   rec->eval_fails = 0;
   begin_frame();
   CHECK(!vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_BALANCED,
            2560, 1440, &out));
   end_frame();
   vulkan_ngx_frame_done(ngx);
   begin_frame();
   CHECK(vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY,
            2560, 1440, &out));
   end_frame();
   vulkan_ngx_frame_done(ngx);
   CHECK(rec->creates == 4);

   /* A feature that will not create: refused, nothing leaks */
   rec->create_fails = 1;
   begin_frame();
   CHECK(!vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_PERF,
            2560, 1440, &out));
   end_frame();
   vulkan_ngx_frame_done(ngx);
   rec->create_fails = 0;

   /* Teardown releases everything, retired or current */
   vkDeviceWaitIdle(device);
   {
      unsigned releases = rec->releases;
      vulkan_ngx_free(ngx);
      CHECK(rec->releases == releases + 3);  /* the three retired */
      CHECK(rec->shutdown_calls == 2 && rec->params_destroyed == 1);
   }
   video_upscaler_detach();
   CHECK(video_upscaler_get_interface() == NULL);

   free_image(&color);
   free_image(&mv);
   free_image(&depth);
   free_image(&color_big);
   vkDestroyCommandPool(device, pool, NULL);
   vkDestroyDevice(device, NULL);
   if (messenger && destroy_messenger)
      destroy_messenger(instance, messenger, NULL);
   vkDestroyInstance(instance, NULL);

   CHECK(validation_errors == 0);
   if (failures)
   {
      printf("vulkan_upscaler_test: %d failure(s)\n", failures);
      return 1;
   }
   printf("vulkan_upscaler_test: ok\n");
   return 0;
}
