/* gfx/common/vulkan_ngx.c and gfx/video_upscaler.c against a stand-in
 * NGX core library (fake_ngx.c) on a real Vulkan device (lavapipe),
 * under the validation layer where it is installed; then
 * gfx/common/vulkan_ffx.c and vulkan_xess.c behind the dispatch of
 * vulkan_upscaler.c, against the stand-in FidelityFX API and XeSS
 * libraries of libretro-common's samples planted in directories of
 * the test's.
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
#include <gfx/amd/ffx_loader.h>
#include <gfx/intel/xess_loader.h>
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
#include "../../../gfx/common/vulkan_upscaler.h"
#include <file/file_path.h>
#include "fake_ngx.h"

extern float stub_core_hz;
#include "../../../libretro-common/samples/gfx/amd/fake_ffx.h"
#include "../../../libretro-common/samples/gfx/intel/fake_xess.h"

/* The DLSS runtime in @dir: the stand-in feature library built beside
 * the test, under the runtime's name, so its version can be read */
static void plant_dlss_runtime(const char *dir)
{
   char path[256];
   char buf[4096];
   FILE *in, *out;
   size_t n;
   path_mkdir(dir);
#if defined(_WIN32)
   in = fopen("nvngx_dlss.dll", "rb");
   snprintf(path, sizeof(path), "%s/nvngx_dlss.dll", dir);
#else
   in = fopen("libnvidia-ngx-dlss.so.310.2.1", "rb");
   snprintf(path, sizeof(path), "%s/libnvidia-ngx-dlss.so.310.2.1", dir);
#endif
   if (!in)
      return;
   if ((out = fopen(path, "wb")))
   {
      while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
         fwrite(buf, 1, n, out);
      fclose(out);
   }
   fclose(in);
}

/* ...and none there (a run before may have left one) */
static void clear_dlss_runtime(const char *dir)
{
   char path[256];
#if defined(_WIN32)
   snprintf(path, sizeof(path), "%s/nvngx_dlss.dll", dir);
   remove(path);
#else
   snprintf(path, sizeof(path), "%s/libnvidia-ngx-dlss.so.1", dir);
   remove(path);
   snprintf(path, sizeof(path), "%s/libnvidia-ngx-dlss.so.310.2.1", dir);
   remove(path);
#endif
}

/* DLSS Neural Rendering's library: on Linux its version is its file
 * name's; on Windows its file version, a copy of the DLSS stand-in's */
static void plant_dlssnr_runtime(const char *dir)
{
   char path[256];
   char buf[4096];
   FILE *in, *out;
   size_t n;
   path_mkdir(dir);
#if defined(_WIN32)
   in = fopen("nvngx_dlss.dll", "rb");
   snprintf(path, sizeof(path), "%s/nvngx_dlssnr.dll", dir);
#else
   in = fopen("libnvidia-ngx-dlss.so.310.2.1", "rb");
   snprintf(path, sizeof(path), "%s/libnvidia-ngx-dlssnr.so.310.8.0", dir);
#endif
   if (!in)
      return;
   if ((out = fopen(path, "wb")))
   {
      while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
         fwrite(buf, 1, n, out);
      fclose(out);
   }
   fclose(in);
}

static void clear_dlssnr_runtime(const char *dir)
{
   char path[256];
#if defined(_WIN32)
   snprintf(path, sizeof(path), "%s/nvngx_dlssnr.dll", dir);
#else
   snprintf(path, sizeof(path), "%s/libnvidia-ngx-dlssnr.so.310.8.0", dir);
#endif
   remove(path);
}

static int failures;
static fake_ngx_record_t *record(void);
static int checks;
static int validation_errors;

#define CHECK(cond) \
   do { \
      checks++; \
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
   VkValidationFeaturesEXT vfeat;
   VkValidationFeatureEnableEXT venable[1] = {
      VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT };
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
   /* (with the layer's hazard tracking: a barrier a library's own
    * stages fall outside of is an error here, not a frame now and then
    * on a GPU that overlaps them) */
   vfeat.sType                          = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
   vfeat.pNext                          = NULL;
   vfeat.enabledValidationFeatureCount  = 1;
   vfeat.pEnabledValidationFeatures     = venable;
   vfeat.disabledValidationFeatureCount = 0;
   vfeat.pDisabledValidationFeatures    = NULL;
   iinfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
   iinfo.pNext                   = validation ? &vfeat : NULL;
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
   /* asked through the feature requirement calls, for DLSS: the stand-in,
    * as OptiScaler, has no NVSDK_NGX_VULKAN_RequiredExtensions */
   CHECK(record() && record()->ext_feature_id == NGX_FEATURE_SUPER_SAMPLING);
   {
      const char *inst[4];
      CHECK(vulkan_ngx_instance_extensions(inst, 0, 4) == 1
            && !strcmp(inst[0], "VK_KHR_get_physical_device_properties2"));
   }
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

/* The frontend's mode word, as the driver hands it to the session */
static bool optimal_cb(void *data, unsigned mode, unsigned out_dims,
      unsigned *render_dims, unsigned *min_dims, unsigned *max_dims)
{
   return vulkan_ngx_optimal((vulkan_ngx_t*)data, video_upscaler_mode_ngx(mode),
         out_dims, render_dims, min_dims, max_dims);
}

static bool dispatch_cb(void *data, unsigned mode, unsigned out_dims,
      unsigned *render_dims, unsigned *min_dims, unsigned *max_dims)
{
   return vulkan_upscaler_optimal((vulkan_upscaler_t*)data, mode, out_dims,
         render_dims, min_dims, max_dims);
}

static void *symbol_of(void *handle, const char *name)
{
   void *fn = NULL;
#if defined(_WIN32)
   FARPROC p = GetProcAddress((HMODULE)handle, name);
#else
   void *p = dlsym(handle, name);
#endif
   memcpy(&fn, &p, sizeof(fn));
   return fn;
}

static unsigned chain_length(const void *pnext, VkStructureType *types, unsigned cap)
{
   const VkBaseInStructure *c;
   unsigned n = 0;
   for (c = (const VkBaseInStructure*)pnext; c; c = c->pNext)
      if (n < cap)
         types[n++] = c->sType;
   return n;
}

#define QUALITY(q) VIDEO_UPSCALER_MODE_PACK(VIDEO_TEMPORAL_UPSCALER_QUALITY_##q, 0)

/* FSR behind the dispatch: the library from the FSR directory, the
 * device's 16-bit extensions and features, the sizes by the library's
 * query, a frame's context and dispatch descriptor as the SDK reads
 * them, a resize retiring the context, a failed dispatch */
static void test_ffx(settings_t *settings, image_t *color, image_t *mv,
      image_t *depth)
{
   vulkan_upscaler_t *up;
   fake_ffx_record_t *(*get)(void);
   fake_ffx_record_t *rec;
   const ffx_lib_t *lib;
   vulkan_upscaler_image_t in, out;
   struct retro_hw_upscaler_inputs inputs;
   struct retro_hw_upscaler_request req;
   video_upscaler_backend_t backend;
   const struct retro_hw_render_upscaler_interface *iface;
   const char *ext[8];
   VkStructureType types[8];
   VkPhysicalDeviceShaderFloat16Int8FeaturesKHR f16;
   const void *chain;
   unsigned n, i, render_dims, min_dims, max_dims;
   char status[256];

   settings->uints.video_temporal_upscaler = VIDEO_TEMPORAL_UPSCALER_FSR;
   strcpy(settings->paths.directory_fsr, "Zdir_none");
   CHECK(video_upscaler_probe(VIDEO_TEMPORAL_UPSCALER_FSR) == 0);
   video_upscaler_status_string(status, sizeof(status));
   CHECK(strstr(status, "FSR (FSR library: not in FSR Directory)") == status);
   CHECK(vulkan_upscaler_init(VIDEO_TEMPORAL_UPSCALER_FSR, instance, gpu, device,
            vkGetInstanceProcAddr, vkGetDeviceProcAddr, &mem_props, "Zdir_none") == NULL);
   CHECK(vulkan_upscaler_device_extensions(VIDEO_TEMPORAL_UPSCALER_FSR, "Zdir_none",
            gpu, vkGetInstanceProcAddr, instance, ext, 0, 8) == 0);
   strcpy(settings->paths.directory_fsr, "Zdir_fsr");
   CHECK(video_upscaler_probe(VIDEO_TEMPORAL_UPSCALER_FSR) == VIDEO_UPSCALER_READY);
   video_upscaler_status_string(status, sizeof(status));
   CHECK(strstr(status, "FSR (FSR library: found)") == status);
   video_upscaler_log_probe(VIDEO_TEMPORAL_UPSCALER_FSR);
   lib = ffx_lib_get(FFX_API_VULKAN, "Zdir_fsr");
   CHECK(lib != NULL);
   if (!lib)
      return;
   get = (fake_ffx_record_t *(*)(void))symbol_of(lib->handle, "fake_ffx_record");
   CHECK(get != NULL);
   if (!get)
      return;
   rec = get();

   /* What the device gets: the 16-bit extensions the GPU has, once */
   n = vulkan_upscaler_device_extensions(VIDEO_TEMPORAL_UPSCALER_FSR, "Zdir_fsr",
         gpu, vkGetInstanceProcAddr, instance, ext, 0, 8);
   CHECK(n <= 2);
   for (i = 0; i < n; i++)
      CHECK(!strcmp(ext[i], "VK_KHR_shader_float16_int8") || !strcmp(ext[i], "VK_KHR_16bit_storage"));
   CHECK(vulkan_upscaler_device_extensions(VIDEO_TEMPORAL_UPSCALER_FSR, "Zdir_fsr",
            gpu, vkGetInstanceProcAddr, instance, ext, n, 8) == n);
   CHECK(vulkan_upscaler_instance_extensions(VIDEO_TEMPORAL_UPSCALER_FSR, "Zdir_fsr",
            ext, 0, 8) == 0);
   chain = vulkan_upscaler_device_features(VIDEO_TEMPORAL_UPSCALER_FSR, "Zdir_fsr",
         gpu, vkGetInstanceProcAddr, instance, NULL);
   n = chain_length(chain, types, 8);
   CHECK(n <= 2);
   for (i = 0; i < n; i++)
      CHECK(types[i] == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR
            || types[i] == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR);
   /* (a chain naming the features already keeps its own) */
   memset(&f16, 0, sizeof(f16));
   f16.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR;
   chain = vulkan_upscaler_device_features(VIDEO_TEMPORAL_UPSCALER_FSR, "Zdir_fsr",
         gpu, vkGetInstanceProcAddr, instance, &f16);
   {
      const VkBaseInStructure *c;
      unsigned f16_nodes = 0;
      for (c = (const VkBaseInStructure*)chain; c; c = c->pNext)
         if (c->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR)
         {
            f16_nodes++;
            CHECK((const void*)c == (const void*)&f16);
         }
      CHECK(f16_nodes == 1);
   }

   /* The session: the newest version the library offers */
   up = vulkan_upscaler_init(VIDEO_TEMPORAL_UPSCALER_FSR, instance, gpu, device,
         vkGetInstanceProcAddr, vkGetDeviceProcAddr, &mem_props, "Zdir_fsr");
   CHECK(up != NULL);
   if (!up)
      return;
   CHECK(rec->creates == 0 && rec->queries >= 2);

   /* The sizes: the library's render size of the mode, a third of the
    * output up to the output */
   CHECK(vulkan_upscaler_optimal(up, QUALITY(QUALITY), VIDEO_SCALE_PACK(1920, 1080),
            &render_dims, &min_dims, &max_dims));
   CHECK(render_dims == VIDEO_SCALE_PACK(1280, 720)
         && min_dims == VIDEO_SCALE_PACK(640, 360)
         && max_dims == VIDEO_SCALE_PACK(1920, 1080));
   CHECK(vulkan_upscaler_optimal(up, QUALITY(BALANCED), VIDEO_SCALE_PACK(1920, 1080),
            &render_dims, &min_dims, &max_dims) && render_dims == VIDEO_SCALE_PACK(1129, 635));
   CHECK(vulkan_upscaler_optimal(up, QUALITY(PERFORMANCE), VIDEO_SCALE_PACK(1920, 1080),
            &render_dims, &min_dims, &max_dims) && render_dims == VIDEO_SCALE_PACK(960, 540));
   CHECK(vulkan_upscaler_optimal(up, QUALITY(ULTRA_PERFORMANCE), VIDEO_SCALE_PACK(1920, 1080),
            &render_dims, &min_dims, &max_dims) && render_dims == VIDEO_SCALE_PACK(640, 360));
   CHECK(vulkan_upscaler_optimal(up, QUALITY(NATIVE), VIDEO_SCALE_PACK(1920, 1080),
            &render_dims, &min_dims, &max_dims) && render_dims == VIDEO_SCALE_PACK(1920, 1080));
   CHECK(!vulkan_upscaler_optimal(up, QUALITY(NATIVE), VIDEO_SCALE_PACK(0, 1080),
            &render_dims, &min_dims, &max_dims));

   /* The core's request through the frontend module */
   backend.data    = up;
   backend.optimal = dispatch_cb;
   video_upscaler_attach(&backend);
   iface = video_upscaler_get_interface();
   CHECK(iface != NULL);
   video_upscaler_set_output(VIDEO_SCALE_PACK(1920, 1080));
   settings->uints.video_temporal_upscaler_quality = VIDEO_TEMPORAL_UPSCALER_QUALITY_AUTO;
   CHECK(iface->get_request(iface->handle, &req));
   CHECK(req.render_width == 1280 && req.render_height == 720
         && req.min_render_width == 640 && req.max_render_width == 1920);
   settings->uints.video_temporal_upscaler_quality = VIDEO_TEMPORAL_UPSCALER_QUALITY_PERFORMANCE;
   CHECK(iface->get_request(iface->handle, &req) && req.render_width == 960);
   settings->uints.video_temporal_upscaler_quality = VIDEO_TEMPORAL_UPSCALER_QUALITY_QUALITY;

   /* A frame: the context for the output size and the inputs' flags,
    * the dispatch with the images as the SDK reads them */
   memset(&inputs, 0, sizeof(inputs));
   set_input(&inputs.motion_vectors, mv);
   set_input(&inputs.depth, depth);
   inputs.flags      = RETRO_HW_UPSCALER_DEPTH_INVERTED | RETRO_HW_UPSCALER_MV_JITTERED;
   inputs.jitter_x   = 0.25f;
   inputs.jitter_y   = -0.125f;
   inputs.mv_scale_x = 0.0f;
   inputs.mv_scale_y = -1.0f;
   inputs.frame_time_ms = 16.6f;
   in.image  = color->image;
   in.view   = color->view;
   in.layout = color->layout;
   in.format = color->format;
   in.dims   = VIDEO_SCALE_PACK(color->width, color->height);
   begin_frame();
   memset(&out, 0, sizeof(out));
   CHECK(vulkan_upscaler_upscale(up, cmd, &in, &inputs, QUALITY(QUALITY),
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   vulkan_upscaler_frame_done(up);
   CHECK(rec->creates == 1 && rec->dispatches == 1);
   CHECK(rec->create[0].flags == (FFX_UPSCALE_ENABLE_DYNAMIC_RESOLUTION
            | FFX_UPSCALE_ENABLE_MV_JITTER_CANCELLATION | FFX_UPSCALE_ENABLE_DEPTH_INVERTED
            | FFX_UPSCALE_ENABLE_NON_LINEAR_COLORSPACE | FFX_UPSCALE_ENABLE_AUTO_EXPOSURE));
   CHECK(rec->create[0].max_render_w == 1920 && rec->create[0].max_render_h == 1080
         && rec->create[0].max_upscale_w == 1920 && rec->create[0].max_upscale_h == 1080);
   CHECK(rec->create[0].backend_type == FFX_TYPE_BACKEND_VK
         && rec->create[0].device == (void*)device
         && rec->create[0].physical_device == (void*)gpu
         && rec->create[0].get_device_proc_addr == (void*)vkGetDeviceProcAddr);
   CHECK(rec->create[0].version_id == 0x4002 && rec->create[0].has_message);
   CHECK(rec->dispatch[0].command_list == (void*)cmd);
   CHECK(rec->dispatch[0].color.present && rec->dispatch[0].color.resource == (void*)color->image
         && rec->dispatch[0].color.format == FFX_FORMAT_R8G8B8A8_UNORM
         && rec->dispatch[0].color.width == 1280 && rec->dispatch[0].color.height == 720
         && rec->dispatch[0].color.usage == FFX_USAGE_READ_ONLY
         && rec->dispatch[0].color.state == FFX_STATE_COMPUTE_READ);
   /* (the depth moved out of its depth layout for the read, the motion
    * vectors read where they are, in GENERAL) */
   CHECK(rec->dispatch[0].depth.resource == (void*)depth->image
         && rec->dispatch[0].depth.format == FFX_FORMAT_R32_FLOAT
         && (rec->dispatch[0].depth.usage & FFX_USAGE_DEPTHTARGET)
         && rec->dispatch[0].depth.state == FFX_STATE_COMPUTE_READ);
   CHECK(rec->dispatch[0].motion_vectors.resource == (void*)mv->image
         && rec->dispatch[0].motion_vectors.format == FFX_FORMAT_R16G16_FLOAT
         && rec->dispatch[0].motion_vectors.state == FFX_STATE_UNORDERED_ACCESS);
   CHECK(rec->dispatch[0].output.present && rec->dispatch[0].output.resource == (void*)out.image
         && rec->dispatch[0].output.usage == FFX_USAGE_UAV
         && rec->dispatch[0].output.state == FFX_STATE_UNORDERED_ACCESS
         && rec->dispatch[0].output.width == 1920 && rec->dispatch[0].output.height == 1080);
   CHECK(!rec->dispatch[0].exposure.present && !rec->dispatch[0].transparency.present
         && !rec->dispatch[0].reactive.present);
   CHECK(rec->dispatch[0].jitter_x == 0.25f && rec->dispatch[0].jitter_y == -0.125f);
   CHECK(rec->dispatch[0].mv_scale_x == 1.0f && rec->dispatch[0].mv_scale_y == -1.0f);
   CHECK(rec->dispatch[0].render_w == 1280 && rec->dispatch[0].render_h == 720
         && rec->dispatch[0].upscale_w == 1920 && rec->dispatch[0].upscale_h == 1080);
   CHECK(!rec->dispatch[0].sharpening && rec->dispatch[0].reset == 1);
   CHECK(rec->dispatch[0].frame_time == 16.6f && rec->dispatch[0].pre_exposure == 1.0f);
   CHECK(rec->dispatch[0].camera_near == 0.1f && rec->dispatch[0].camera_far == 100000.0f);
   CHECK(rec->dispatch[0].flags == FFX_UPSCALE_FLAG_NON_LINEAR_COLOR_SRGB);
   CHECK(out.dims == VIDEO_SCALE_PACK(1920, 1080) && out.format == VK_FORMAT_R8G8B8A8_UNORM
         && out.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && out.view != VK_NULL_HANDLE);

   /* The next frames: the same context, no reset but the core's; a
    * mode change needs no new context, only the render size changes */
   begin_frame();
   CHECK(vulkan_upscaler_upscale(up, cmd, &in, &inputs, QUALITY(QUALITY),
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   vulkan_upscaler_frame_done(up);
   CHECK(rec->creates == 1 && rec->dispatches == 2 && rec->dispatch[1].reset == 0);
   inputs.flags |= RETRO_HW_UPSCALER_RESET;
   in.dims = VIDEO_SCALE_PACK(960, 540);
   begin_frame();
   CHECK(vulkan_upscaler_upscale(up, cmd, &in, &inputs, QUALITY(PERFORMANCE),
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   vulkan_upscaler_frame_done(up);
   CHECK(rec->creates == 1 && rec->dispatches == 3 && rec->dispatch[2].reset == 1
         && rec->dispatch[2].render_w == 960 && rec->dispatch[2].render_h == 540);
   inputs.flags &= ~RETRO_HW_UPSCALER_RESET;
   in.dims = VIDEO_SCALE_PACK(1280, 720);

   /* HDR inputs: another context, linear color; the sharpness in the
    * mode word is FSR's own sharpening */
   inputs.flags |= RETRO_HW_UPSCALER_HDR;
   begin_frame();
   CHECK(vulkan_upscaler_upscale(up, cmd, &in, &inputs,
            VIDEO_UPSCALER_MODE_PACK_SHARP(VIDEO_TEMPORAL_UPSCALER_QUALITY_QUALITY, 0, 30),
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   vulkan_upscaler_frame_done(up);
   CHECK(rec->creates == 2 && rec->dispatches == 4
         && (rec->create[1].flags & FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE)
         && !(rec->create[1].flags & FFX_UPSCALE_ENABLE_NON_LINEAR_COLORSPACE)
         && rec->dispatch[3].flags == 0);
   CHECK(rec->dispatch[3].sharpening && rec->dispatch[3].sharpness == 0.3f
         && rec->dispatch[3].render_w == 1280);
   inputs.flags &= ~RETRO_HW_UPSCALER_HDR;

   /* A resize: a new context, the old retired after the frames in flight */
   begin_frame();
   CHECK(vulkan_upscaler_upscale(up, cmd, &in, &inputs, QUALITY(QUALITY),
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   vulkan_upscaler_frame_done(up);
   CHECK(rec->creates == 3 && rec->create[2].max_upscale_w == 2560
         && out.dims == VIDEO_SCALE_PACK(2560, 1440));
   CHECK(rec->destroys == 0);
   for (i = 0; i < 30 && rec->destroys < 2; i++)
      vulkan_upscaler_frame_done(up);
   CHECK(rec->destroys == 2 && i >= 15);

   /* A dispatch that fails: the frame shown as it is, nothing tried
    * again until the output or flags change */
   rec->dispatch_fails = 1;
   begin_frame();
   CHECK(!vulkan_upscaler_upscale(up, cmd, &in, &inputs, QUALITY(QUALITY),
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   vulkan_upscaler_frame_done(up);
   rec->dispatch_fails = 0;
   begin_frame();
   CHECK(!vulkan_upscaler_upscale(up, cmd, &in, &inputs, QUALITY(QUALITY),
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   vulkan_upscaler_frame_done(up);
   CHECK(rec->dispatches == 6);
   begin_frame();
   CHECK(vulkan_upscaler_upscale(up, cmd, &in, &inputs, QUALITY(QUALITY),
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   vulkan_upscaler_frame_done(up);
   CHECK(rec->creates == 4 && rec->dispatches == 7);

   /* A context that will not create: refused, nothing leaks */
   rec->create_fails = 1;
   begin_frame();
   CHECK(!vulkan_upscaler_upscale(up, cmd, &in, &inputs, QUALITY(QUALITY),
            VIDEO_SCALE_PACK(1280, 720), &out));
   end_frame();
   vulkan_upscaler_frame_done(up);
   rec->create_fails = 0;
   CHECK(rec->creates == 5);

   /* No inputs, no depth: refused */
   CHECK(!vulkan_upscaler_upscale(up, cmd, &in, NULL, QUALITY(QUALITY),
            VIDEO_SCALE_PACK(1920, 1080), &out));
   inputs.depth.image = 0;
   CHECK(!vulkan_upscaler_upscale(up, cmd, &in, &inputs, QUALITY(QUALITY),
            VIDEO_SCALE_PACK(1920, 1080), &out));

   vkDeviceWaitIdle(device);
   vulkan_upscaler_free(up);
   CHECK(rec->destroys == 4);
   video_upscaler_detach();
}

/* XeSS behind the dispatch: the library from the XeSS directory, the
 * instance and device extensions and the feature chain it asks for,
 * the sizes by its query, a context whose pipelines build in the
 * background before init, the execute parameters as the SDK reads
 * them, a resize retiring the context, a failed execute */
static void test_xess(settings_t *settings, image_t *color, image_t *mv,
      image_t *depth)
{
   vulkan_upscaler_t *up;
   fake_xess_record_t *(*get)(void);
   fake_xess_record_t *rec;
   const xess_lib_t *lib;
   vulkan_upscaler_image_t in, out;
   struct retro_hw_upscaler_inputs inputs;
   struct retro_hw_upscaler_request req;
   video_upscaler_backend_t backend;
   const struct retro_hw_render_upscaler_interface *iface;
   const char *ext[8];
   VkStructureType types[8];
   struct { VkStructureType sType; void *pNext; uint32_t a, b; } named;
   const void *chain;
   unsigned n, i, render_dims, min_dims, max_dims;
   char status[256];

   settings->uints.video_temporal_upscaler = VIDEO_TEMPORAL_UPSCALER_XESS;
   strcpy(settings->paths.directory_xess, "Zdir_none");
   CHECK(video_upscaler_probe(VIDEO_TEMPORAL_UPSCALER_XESS) == 0);
   video_upscaler_status_string(status, sizeof(status));
   CHECK(strstr(status, "XeSS (XeSS library: not in XeSS Directory)") == status);
   CHECK(vulkan_upscaler_init(VIDEO_TEMPORAL_UPSCALER_XESS, instance, gpu, device,
            vkGetInstanceProcAddr, vkGetDeviceProcAddr, &mem_props, "Zdir_none") == NULL);
   CHECK(vulkan_upscaler_instance_extensions(VIDEO_TEMPORAL_UPSCALER_XESS, "Zdir_none",
            ext, 0, 8) == 0);
   strcpy(settings->paths.directory_xess, "Zdir_xess");
   CHECK(video_upscaler_probe(VIDEO_TEMPORAL_UPSCALER_XESS) == VIDEO_UPSCALER_READY);
   video_upscaler_status_string(status, sizeof(status));
   CHECK(strstr(status, "XeSS (XeSS library: found)") == status);
   video_upscaler_log_probe(VIDEO_TEMPORAL_UPSCALER_XESS);
   lib = xess_lib_get("Zdir_xess");
   CHECK(lib != NULL);
   if (!lib)
      return;
   get = (fake_xess_record_t *(*)(void))symbol_of(lib->handle, "fake_xess_record");
   CHECK(get != NULL);
   if (!get)
      return;
   rec = get();

   /* What the instance and device get: the library's lists, those the
    * GPU has, none twice; its feature chain ahead of the device's */
   ext[0] = "VK_KHR_surface";
   n = vulkan_upscaler_instance_extensions(VIDEO_TEMPORAL_UPSCALER_XESS, "Zdir_xess", ext, 1, 8);
   CHECK(n == 2 && !strcmp(ext[1], "VK_KHR_get_physical_device_properties2"));
   CHECK(vulkan_upscaler_instance_extensions(VIDEO_TEMPORAL_UPSCALER_XESS, "Zdir_xess", ext, n, 8) == n);
   n = vulkan_upscaler_device_extensions(VIDEO_TEMPORAL_UPSCALER_XESS, "Zdir_xess",
         gpu, vkGetInstanceProcAddr, instance, ext, 0, 8);
   CHECK(n <= 2);
   for (i = 0; i < n; i++)
      CHECK(!strcmp(ext[i], "VK_KHR_shader_float16_int8") || !strcmp(ext[i], "VK_KHR_16bit_storage"));
   CHECK(vulkan_upscaler_device_extensions(VIDEO_TEMPORAL_UPSCALER_XESS, "Zdir_xess",
            gpu, vkGetInstanceProcAddr, instance, ext, n, 8) == n);
   chain = vulkan_upscaler_device_features(VIDEO_TEMPORAL_UPSCALER_XESS, "Zdir_xess",
         gpu, vkGetInstanceProcAddr, instance, NULL);
   n = chain_length(chain, types, 8);
   CHECK(n == 1 && types[0] == (VkStructureType)1000082000);
   memset(&named, 0, sizeof(named));
   named.sType = (VkStructureType)1000082000;
   chain = vulkan_upscaler_device_features(VIDEO_TEMPORAL_UPSCALER_XESS, "Zdir_xess",
         gpu, vkGetInstanceProcAddr, instance, &named);
   CHECK(chain == (const void*)&named);

   /* The session: a context of its own for the queries */
   up = vulkan_upscaler_init(VIDEO_TEMPORAL_UPSCALER_XESS, instance, gpu, device,
         vkGetInstanceProcAddr, vkGetDeviceProcAddr, &mem_props, "Zdir_xess");
   CHECK(up != NULL);
   if (!up)
      return;
   CHECK(rec->creates == 1 && rec->create_instance == (void*)instance
         && rec->create_physical_device == (void*)gpu && rec->create_device == (void*)device);
   CHECK(rec->inits == 0);

   /* The sizes: the library's, the range it names */
   CHECK(vulkan_upscaler_optimal(up, QUALITY(QUALITY), VIDEO_SCALE_PACK(1920, 1080),
            &render_dims, &min_dims, &max_dims));
   CHECK(render_dims == VIDEO_SCALE_PACK(1129, 635)
         && min_dims == VIDEO_SCALE_PACK(640, 360)
         && max_dims == VIDEO_SCALE_PACK(1920, 1080));
   CHECK(vulkan_upscaler_optimal(up, QUALITY(BALANCED), VIDEO_SCALE_PACK(1920, 1080),
            &render_dims, &min_dims, &max_dims) && render_dims == VIDEO_SCALE_PACK(960, 540));
   CHECK(vulkan_upscaler_optimal(up, QUALITY(PERFORMANCE), VIDEO_SCALE_PACK(1920, 1080),
            &render_dims, &min_dims, &max_dims) && render_dims == VIDEO_SCALE_PACK(834, 469));
   CHECK(vulkan_upscaler_optimal(up, QUALITY(ULTRA_PERFORMANCE), VIDEO_SCALE_PACK(1920, 1080),
            &render_dims, &min_dims, &max_dims) && render_dims == VIDEO_SCALE_PACK(640, 360));
   CHECK(vulkan_upscaler_optimal(up, QUALITY(NATIVE), VIDEO_SCALE_PACK(1920, 1080),
            &render_dims, &min_dims, &max_dims) && render_dims == VIDEO_SCALE_PACK(1920, 1080));

   backend.data    = up;
   backend.optimal = dispatch_cb;
   video_upscaler_attach(&backend);
   iface = video_upscaler_get_interface();
   CHECK(iface != NULL);
   video_upscaler_set_output(VIDEO_SCALE_PACK(1920, 1080));
   settings->uints.video_temporal_upscaler_quality = VIDEO_TEMPORAL_UPSCALER_QUALITY_BALANCED;
   CHECK(iface->get_request(iface->handle, &req));
   CHECK(req.render_width == 960 && req.render_height == 540
         && req.min_render_width == 640 && req.max_render_width == 1920);

   /* A frame: a context for the output size, quality and flags, its
    * pipelines building; nothing shown until they are built, then the
    * init and the execute */
   memset(&inputs, 0, sizeof(inputs));
   set_input(&inputs.motion_vectors, mv);
   set_input(&inputs.depth, depth);
   inputs.flags      = RETRO_HW_UPSCALER_DEPTH_INVERTED | RETRO_HW_UPSCALER_MV_JITTERED;
   inputs.jitter_x   = 0.25f;
   inputs.jitter_y   = -0.125f;
   inputs.mv_scale_x = 0.0f;
   inputs.mv_scale_y = -1.0f;
   inputs.frame_time_ms = 16.6f;
   in.image  = color->image;
   in.view   = color->view;
   in.layout = color->layout;
   in.format = color->format;
   in.dims   = VIDEO_SCALE_PACK(color->width, color->height);
   rec->build_pending = 2;
   begin_frame();
   memset(&out, 0, sizeof(out));
   CHECK(!vulkan_upscaler_upscale(up, cmd, &in, &inputs, QUALITY(BALANCED),
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   vulkan_upscaler_frame_done(up);
   CHECK(rec->creates == 2 && rec->pipelines == 1 && !rec->build_blocking
         && rec->inits == 0 && rec->executes == 0);
   CHECK(rec->log_level == XESS_LOGGING_LEVEL_WARNING);
   begin_frame();
   CHECK(!vulkan_upscaler_upscale(up, cmd, &in, &inputs, QUALITY(BALANCED),
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   vulkan_upscaler_frame_done(up);
   CHECK(rec->creates == 2 && rec->pipelines == 1 && rec->inits == 0);
   begin_frame();
   CHECK(vulkan_upscaler_upscale(up, cmd, &in, &inputs, QUALITY(BALANCED),
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   vulkan_upscaler_frame_done(up);
   CHECK(rec->creates == 2 && rec->inits == 1 && rec->executes == 1);
   CHECK(rec->init[0].vk && rec->init[0].out_w == 1920 && rec->init[0].out_h == 1080
         && rec->init[0].quality == XESS_QUALITY_BALANCED
         && rec->init[0].flags == (XESS_INIT_FLAG_LDR_INPUT_COLOR | XESS_INIT_FLAG_JITTERED_MV
            | XESS_INIT_FLAG_INVERTED_DEPTH | XESS_INIT_FLAG_ENABLE_AUTOEXPOSURE)
         && rec->init[0].node_mask == 1);
   CHECK(rec->jitter_scale_x == 1.0f && rec->jitter_scale_y == 1.0f);
   CHECK(rec->velocity_scale_x == 1.0f && rec->velocity_scale_y == -1.0f);
   CHECK(rec->execute[0].vk && rec->execute[0].context == rec->init[0].context
         && rec->execute[0].command_list == (void*)cmd);
   CHECK(rec->execute[0].color.present && rec->execute[0].color.image == (uint64_t)(uintptr_t)color->image
         && rec->execute[0].color.view == (uint64_t)(uintptr_t)color->view
         && rec->execute[0].color.format == (int)VK_FORMAT_R8G8B8A8_UNORM
         && rec->execute[0].color.aspect == VK_IMAGE_ASPECT_COLOR_BIT
         && rec->execute[0].color.width == 1280 && rec->execute[0].color.height == 720);
   CHECK(rec->execute[0].depth.image == (uint64_t)(uintptr_t)depth->image
         && rec->execute[0].depth.aspect == VK_IMAGE_ASPECT_DEPTH_BIT
         && rec->execute[0].depth.format == (int)VK_FORMAT_D32_SFLOAT);
   CHECK(rec->execute[0].velocity.image == (uint64_t)(uintptr_t)mv->image
         && rec->execute[0].velocity.format == (int)VK_FORMAT_R16G16_SFLOAT);
   CHECK(rec->execute[0].output.present && rec->execute[0].output.image == (uint64_t)(uintptr_t)out.image
         && rec->execute[0].output.view == (uint64_t)(uintptr_t)out.view
         && rec->execute[0].output.width == 1920 && rec->execute[0].output.height == 1080);
   CHECK(!rec->execute[0].exposure.present && !rec->execute[0].mask.present);
   CHECK(rec->execute[0].jitter_x == 0.25f && rec->execute[0].jitter_y == -0.125f);
   CHECK(rec->execute[0].exposure_scale == 1.0f && rec->execute[0].reset == 1);
   CHECK(rec->execute[0].input_w == 1280 && rec->execute[0].input_h == 720);
   CHECK(out.dims == VIDEO_SCALE_PACK(1920, 1080) && out.format == VK_FORMAT_R8G8B8A8_UNORM
         && out.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

   /* The next frame: the same context, no reset; the scale set once */
   begin_frame();
   CHECK(vulkan_upscaler_upscale(up, cmd, &in, &inputs, QUALITY(BALANCED),
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   vulkan_upscaler_frame_done(up);
   CHECK(rec->creates == 2 && rec->inits == 1 && rec->executes == 2 && rec->execute[1].reset == 0);

   /* A mode change: a new context (the old one's pipelines are its own) */
   begin_frame();
   CHECK(vulkan_upscaler_upscale(up, cmd, &in, &inputs, QUALITY(QUALITY),
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   vulkan_upscaler_frame_done(up);
   CHECK(rec->creates == 3 && rec->inits == 2 && rec->init[1].quality == XESS_QUALITY_QUALITY
         && rec->executes == 3 && rec->execute[2].reset == 1);
   CHECK(rec->destroys == 0);
   for (i = 0; i < 30 && rec->destroys < 1; i++)
      vulkan_upscaler_frame_done(up);
   CHECK(rec->destroys == 1 && i >= 15);

   /* An execute that fails: the frame shown as it is until the output
    * or mode changes */
   rec->execute_fails = 1;
   begin_frame();
   CHECK(!vulkan_upscaler_upscale(up, cmd, &in, &inputs, QUALITY(QUALITY),
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   vulkan_upscaler_frame_done(up);
   rec->execute_fails = 0;
   begin_frame();
   CHECK(!vulkan_upscaler_upscale(up, cmd, &in, &inputs, QUALITY(QUALITY),
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   vulkan_upscaler_frame_done(up);
   CHECK(rec->executes == 4);
   begin_frame();
   CHECK(vulkan_upscaler_upscale(up, cmd, &in, &inputs, QUALITY(QUALITY),
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   vulkan_upscaler_frame_done(up);
   CHECK(rec->creates == 4 && rec->inits == 3 && rec->executes == 5
         && out.dims == VIDEO_SCALE_PACK(2560, 1440));

   /* An init that fails: refused, the context gone */
   rec->init_fails = 1;
   begin_frame();
   CHECK(!vulkan_upscaler_upscale(up, cmd, &in, &inputs, QUALITY(BALANCED),
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   vulkan_upscaler_frame_done(up);
   rec->init_fails = 0;
   CHECK(rec->creates == 5 && rec->executes == 5);

   vkDeviceWaitIdle(device);
   vulkan_upscaler_free(up);
   CHECK(rec->destroys == 5);   /* every context, the query one too */
   video_upscaler_detach();
}

/* A feature library NGX loaded stays loaded past NGX's own unload of
 * it, so that NGX made again finds it in (a directory of its own: a
 * loaded library cannot be removed on Windows) */
static void test_keep_features(void)
{
#if defined(_WIN32)
   HMODULE m;
   plant_dlss_runtime("Zdir_keep");
   CHECK(GetModuleHandleA("nvngx_dlss.dll") == NULL);
   m = LoadLibraryA("Zdir_keep\\nvngx_dlss.dll");
   CHECK(m != NULL);
   ngx_lib_keep_features("Zdir_keep");
   if (m)
      FreeLibrary(m);
   CHECK(GetModuleHandleA("nvngx_dlss.dll") != NULL);
#else
   void *m;
   const char *path = "Zdir_keep/libnvidia-ngx-dlss.so.310.2.1";
   plant_dlss_runtime("Zdir_keep");
   CHECK(dlopen(path, RTLD_NOW | RTLD_LOCAL | RTLD_NOLOAD) == NULL);
   m = dlopen(path, RTLD_NOW | RTLD_LOCAL);
   CHECK(m != NULL);
   ngx_lib_keep_features("Zdir_keep");
   if (m)
      dlclose(m);
   CHECK(dlopen(path, RTLD_NOW | RTLD_LOCAL | RTLD_NOLOAD) != NULL);
#endif
}

/* A frame below its mode's range but in another's runs in that mode,
 * the nearest taking it, and back; one no mode takes is refused */
static void test_mode_for(image_t *color, image_t *mv, image_t *depth)
{
   fake_ngx_record_t *rec = record();
   vulkan_ngx_t *ngx;
   vulkan_ngx_image_t in, out;
   struct retro_hw_upscaler_inputs inputs;
   unsigned creates, evals;

   ngx = vulkan_ngx_init(instance, gpu, device, vkGetInstanceProcAddr,
         vkGetDeviceProcAddr, &mem_props, "Zdir");
   CHECK(ngx != NULL);
   if (!ngx)
      return;
   memset(&inputs, 0, sizeof(inputs));
   set_input(&inputs.motion_vectors, mv);
   set_input(&inputs.depth, depth);
   in.image  = color->image;
   in.view   = color->view;
   in.layout = color->layout;
   in.format = color->format;
   creates   = rec->creates;
   evals     = rec->evals;

   in.dims   = VIDEO_SCALE_PACK(400, 225);
   begin_frame();
   CHECK(vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, 0,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   vulkan_ngx_frame_done(ngx);
   CHECK(rec->creates == creates + 1 && rec->evals == evals + 1);
   CHECK(rec->create[creates % FAKE_NGX_MAX].quality == NGX_PERF_QUALITY_ULTRA_PERFORMANCE
         && rec->create[creates % FAKE_NGX_MAX].width == 1920);
   CHECK(rec->eval[evals % FAKE_NGX_MAX].subrect_w == 400
         && rec->eval[evals % FAKE_NGX_MAX].subrect_h == 225);

   in.dims   = VIDEO_SCALE_PACK(1280, 720);
   begin_frame();
   CHECK(vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, 0,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   vulkan_ngx_frame_done(ngx);
   CHECK(rec->creates == creates + 2
         && rec->create[(creates + 1) % FAKE_NGX_MAX].quality == NGX_PERF_QUALITY_MAX_QUALITY);

   in.dims   = VIDEO_SCALE_PACK(200, 112);
   begin_frame();
   CHECK(!vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, 0,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   vulkan_ngx_frame_done(ngx);
   CHECK(rec->creates == creates + 2 && rec->evals == evals + 2);

   vkDeviceWaitIdle(device);
   vulkan_ngx_free(ngx);
}

/* DLSS Neural Rendering over Super Resolution's output: offered only
 * with its library in the DLSS directory, the settings as one word,
 * the feature made with the model's tuning and evaluated on Super
 * Resolution's output with the core's guides, made again only once a
 * changed setting has held, dropped when turned off, and a core
 * without it (an older driver) leaving Super Resolution alone */
/* Frames follow each other on the queue with nothing waited between
 * them; the library writes its output with whatever stage it likes (the
 * stand-in clears it through the transfer stage). Two in one command
 * buffer, as the layer tracks hazards: the barriers around an evaluate
 * must order the library's work against the frame before, however it
 * was done */
static void test_barriers(image_t *color, image_t *mv, image_t *depth)
{
   fake_ngx_record_t *rec = record();
   vulkan_ngx_t *ngx;
   vulkan_ngx_image_t in, out;
   struct retro_hw_upscaler_inputs inputs;
   unsigned evals = rec->evals;
   int errors     = validation_errors;

   ngx = vulkan_ngx_init(instance, gpu, device, vkGetInstanceProcAddr,
         vkGetDeviceProcAddr, &mem_props, "Zdir");
   CHECK(ngx != NULL);
   if (!ngx)
      return;
   memset(&inputs, 0, sizeof(inputs));
   set_input(&inputs.motion_vectors, mv);
   set_input(&inputs.depth, depth);
   in.image  = color->image;
   in.view   = color->view;
   in.layout = color->layout;
   in.format = color->format;
   in.dims   = VIDEO_SCALE_PACK(1280, 720);

   begin_frame();
   CHECK(vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, 0,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   CHECK(vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, 0,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   vulkan_ngx_frame_done(ngx);
   CHECK(rec->evals == evals + 2);
   CHECK(validation_errors == errors);

   vkDeviceWaitIdle(device);
   vulkan_ngx_free(ngx);
}

static void test_nr(settings_t *settings, image_t *color, image_t *mv,
      image_t *depth)
{
   fake_ngx_record_t *rec = record();
   vulkan_ngx_t *ngx;
   vulkan_ngx_image_t in, out;
   struct retro_hw_upscaler_inputs inputs;
   const char *list[8];
   char status[256];
   unsigned nr, nr2, creates, evals, releases, i, sr, calls, made;
#if defined(_WIN32)
   const uint32_t version = NGX_VERSION_PACK(310, 2, 1);
   const char *expect = "DLSS (NGX: found, DLSS runtime 310.2.1, Neural Rendering 310.2.1)";
#else
   const uint32_t version = NGX_VERSION_PACK(310, 8, 0);
   const char *expect = "DLSS (NGX: found, DLSS runtime 310.2.1, Neural Rendering 310.8.0)";
#endif

   settings->uints.video_temporal_upscaler = VIDEO_TEMPORAL_UPSCALER_DLSS;
   clear_dlssnr_runtime("Zdir");
   video_upscaler_probe(VIDEO_TEMPORAL_UPSCALER_DLSS);
   CHECK(!video_upscaler_nr_available());
   CHECK(video_upscaler_dlssnr_version() == 0);
   plant_dlssnr_runtime("Zdir");
   video_upscaler_probe(VIDEO_TEMPORAL_UPSCALER_DLSS);
   CHECK(video_upscaler_nr_available());
   CHECK(video_upscaler_dlssnr_version() == version);
   video_upscaler_status_string(status, sizeof(status));
   CHECK(!strcmp(status, expect));

   settings->bools.video_dlss_nr_enable          = false;
   CHECK(video_upscaler_nr_mode() == 0);
   settings->bools.video_dlss_nr_enable          = true;
   settings->bools.video_dlss_nr_auto_mask       = true;
   settings->uints.video_dlss_nr_preset          = 1;
   settings->uints.video_dlss_nr_style           = 2;
   settings->uints.video_dlss_nr_intensity       = 100;
   settings->uints.video_dlss_nr_local_structure = 150;
   settings->uints.video_dlss_nr_local_tone      = 50;
   settings->ints.video_dlss_nr_skin_structure   = -5;
   nr = video_upscaler_nr_mode();
   CHECK(nr == VIDEO_UPSCALER_NR_PACK(VIDEO_UPSCALER_NR_ON | VIDEO_UPSCALER_NR_AUTO_MASK,
            1, 2, 20, 30, 10, 0));
   settings->ints.video_dlss_nr_skin_structure   = 50;
   CHECK(VIDEO_UPSCALER_NR_SKIN(video_upscaler_nr_mode()) == 11);
   settings->ints.video_dlss_nr_skin_structure   = -5;

   /* Its device extensions are asked for too, on or off, so that
    * turning it on needs no new context; the answers kept for the next
    * one */
   calls = rec->ext_calls;
   CHECK(vulkan_ngx_device_extensions(gpu, vkGetInstanceProcAddr, instance,
            list, 0, 8) >= 1);
   CHECK(rec->ext_feature_id == 18 && rec->ext_calls == calls + 2);
   CHECK(vulkan_ngx_device_extensions(gpu, vkGetInstanceProcAddr, instance,
            list, 0, 8) >= 1);
   CHECK(rec->ext_calls == calls + 2);
   settings->bools.video_dlss_nr_enable          = false;
   CHECK(vulkan_ngx_device_extensions(gpu, vkGetInstanceProcAddr, instance,
            list, 0, 8) >= 1);
   CHECK(rec->ext_calls == calls + 2);
   settings->bools.video_dlss_nr_enable          = true;

   ngx = vulkan_ngx_init(instance, gpu, device, vkGetInstanceProcAddr,
         vkGetDeviceProcAddr, &mem_props, "Zdir");
   CHECK(ngx != NULL);
   if (!ngx)
      return;
   memset(&inputs, 0, sizeof(inputs));
   set_input(&inputs.motion_vectors, mv);
   set_input(&inputs.depth, depth);
   inputs.flags      = RETRO_HW_UPSCALER_DEPTH_INVERTED;
   inputs.mv_scale_x = -2.0f;
   inputs.mv_scale_y = 3.0f;
   in.image  = color->image;
   in.view   = color->view;
   in.layout = color->layout;
   in.format = color->format;
   in.dims   = VIDEO_SCALE_PACK(color->width, color->height);

   /* The first frame: Super Resolution, then the model on its output */
   creates = rec->creates;
   evals   = rec->evals;
   begin_frame();
   CHECK(vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, nr,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   vulkan_ngx_frame_done(ngx);
   CHECK(rec->creates == creates + 2 && rec->evals == evals + 2);
   CHECK(rec->create[creates % FAKE_NGX_MAX].feature == 1
         && rec->create[(creates + 1) % FAKE_NGX_MAX].feature == 18);
   i = (creates + 1) % FAKE_NGX_MAX;
   CHECK(rec->create[i].out_width == 1920 && rec->create[i].out_height == 1080
         && rec->create[i].node_mask == 0x101);
   CHECK(rec->create[i].nr_preset == 1 && rec->create[i].nr_style == 2
         && rec->create[i].nr_auto_mask == 1);
   CHECK(rec->create[i].nr_intensity == 1.0f && rec->create[i].nr_structure == 1.5f
         && rec->create[i].nr_tone == 0.5f && rec->create[i].nr_skin == -1.0f);
   sr = evals % FAKE_NGX_MAX;
   i  = (evals + 1) % FAKE_NGX_MAX;
   CHECK(!rec->eval[sr].nr && rec->eval[i].nr);
   CHECK(rec->eval[i].color.present && !rec->eval[i].color.read_write
         && rec->eval[i].color.image == rec->eval[sr].output.image
         && rec->eval[i].color.width == 1920);
   CHECK(rec->eval[i].output.present && rec->eval[i].output.read_write
         && rec->eval[i].output.image == (uint64_t)(uintptr_t)out.image
         && rec->eval[i].output.image != rec->eval[sr].output.image
         && rec->eval[i].output.width == 1920);
   CHECK(rec->eval[i].depth.image == (uint64_t)(uintptr_t)depth->image
         && rec->eval[i].motion_vectors.image == (uint64_t)(uintptr_t)mv->image);
   CHECK(rec->eval[i].subrect_w == 1920 && rec->eval[i].subrect_h == 1080);
   CHECK(rec->eval[i].depth_w == 1280 && rec->eval[i].depth_h == 720
         && rec->eval[i].mv_w == 1280 && rec->eval[i].mv_h == 720);
   CHECK(rec->eval[i].depth_inverted == 1 && rec->eval[i].reset == 1);
   CHECK(rec->eval[i].mv_scale_x == -2.0f && rec->eval[i].mv_scale_y == 3.0f);
   CHECK(out.dims == VIDEO_SCALE_PACK(1920, 1080)
         && out.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

   /* The next frame: the same feature, no reset */
   begin_frame();
   CHECK(vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, nr,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   vulkan_ngx_frame_done(ngx);
   CHECK(rec->creates == creates + 2 && rec->eval[(rec->evals - 1) % FAKE_NGX_MAX].nr
         && rec->eval[(rec->evals - 1) % FAKE_NGX_MAX].reset == 0);

   /* A changed setting: the model made again only once it has held */
   nr2 = VIDEO_UPSCALER_NR_PACK(VIDEO_UPSCALER_NR_ON, 1, 2, 30, 30, 10, 0);
   for (i = 0; i < 40 && rec->creates == creates + 2; i++)
   {
      begin_frame();
      CHECK(vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, nr2,
               VIDEO_SCALE_PACK(1920, 1080), &out));
      end_frame();
      vulkan_ngx_frame_done(ngx);
   }
   CHECK(rec->creates == creates + 3 && i >= 25);
   i = (rec->creates - 1) % FAKE_NGX_MAX;
   CHECK(rec->create[i].feature == 18 && rec->create[i].nr_intensity == 1.5f
         && rec->create[i].nr_auto_mask == 0);

   /* Off: Super Resolution's frame, the model kept; on again, the same
    * model, its history reset once */
   for (i = 0; i < 30; i++)
      vulkan_ngx_frame_done(ngx);
   made     = rec->creates;
   releases = rec->releases;
   evals    = rec->evals;
   begin_frame();
   CHECK(vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, 0,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   CHECK(rec->evals == evals + 1 && !rec->eval[evals % FAKE_NGX_MAX].nr
         && out.image && rec->eval[evals % FAKE_NGX_MAX].output.image
         == (uint64_t)(uintptr_t)out.image);
   for (i = 0; i < 30; i++)
      vulkan_ngx_frame_done(ngx);
   CHECK(rec->releases == releases);
   for (i = 0; i < 2; i++)
   {
      evals = rec->evals;
      begin_frame();
      CHECK(vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, nr2,
               VIDEO_SCALE_PACK(1920, 1080), &out));
      end_frame();
      vulkan_ngx_frame_done(ngx);
      CHECK(rec->creates == made && rec->evals == evals + 2
            && rec->eval[(evals + 1) % FAKE_NGX_MAX].nr
            && rec->eval[(evals + 1) % FAKE_NGX_MAX].reset == (i == 0));
   }
   vkDeviceWaitIdle(device);
   vulkan_ngx_free(ngx);

   /* Off too, the core is asked about the model, so that turning it on
    * needs nothing made again: a core without it is found out */
   settings->bools.video_dlss_nr_enable = false;
   video_upscaler_note_nr(true);
   rec->nr_missing = 1;
   ngx = vulkan_ngx_init(instance, gpu, device, vkGetInstanceProcAddr,
         vkGetDeviceProcAddr, &mem_props, "Zdir");
   CHECK(ngx != NULL);
   if (ngx)
   {
      CHECK(!video_upscaler_nr_available());
      vulkan_ngx_free(ngx);
   }

   /* A core without the model, as before its driver: not offered, said
    * so, and Super Resolution goes on */
   settings->bools.video_dlss_nr_enable = true;
   ngx = vulkan_ngx_init(instance, gpu, device, vkGetInstanceProcAddr,
         vkGetDeviceProcAddr, &mem_props, "Zdir");
   CHECK(ngx != NULL);
   CHECK(!video_upscaler_nr_available());
   video_upscaler_status_string(status, sizeof(status));
   CHECK(strstr(status, "Neural Rendering: needs a newer driver") != NULL);
   if (ngx)
   {
      creates = rec->creates;
      begin_frame();
      CHECK(vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, nr,
               VIDEO_SCALE_PACK(1920, 1080), &out));
      end_frame();
      vulkan_ngx_frame_done(ngx);
      CHECK(rec->creates == creates + 1
            && rec->create[creates % FAKE_NGX_MAX].feature == 1);
      vkDeviceWaitIdle(device);
      vulkan_ngx_free(ngx);
   }
   settings->bools.video_dlss_nr_enable = false;
   rec->nr_missing = 0;
   video_upscaler_note_nr(true);
   clear_dlssnr_runtime("Zdir");
   video_upscaler_probe(VIDEO_TEMPORAL_UPSCALER_DLSS);
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
   unsigned render_dims, min_dims, max_dims;
   unsigned i;

   /* Until a core asks for it, nothing loads the NGX library: not the
    * log of what the upscaler would find, nor (on Windows, where a
    * stand-in hooks the process) the status */
   settings->uints.video_temporal_upscaler = VIDEO_TEMPORAL_UPSCALER_DLSS;
   CHECK(!video_upscaler_wanted());
   video_upscaler_log_probe(VIDEO_TEMPORAL_UPSCALER_DLSS);
   CHECK(video_upscaler_probe(VIDEO_TEMPORAL_UPSCALER_DLSS) & VIDEO_UPSCALER_FOUND_LIBRARY);
#if defined(_WIN32)
   CHECK(GetModuleHandleW(L"_nvngx.dll") == NULL);
#endif
   video_upscaler_note_wanted();
   CHECK(video_upscaler_wanted());
   video_upscaler_clear_wanted();
   CHECK(!video_upscaler_wanted());
   video_upscaler_note_wanted();
   settings->uints.video_temporal_upscaler = 0;
   test_keep_features();

   if (!setup_vulkan())
   {
      printf("vulkan_upscaler_test: no Vulkan device, skipped\n");
      return failures ? 1 : 0;
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
   /* no runtime in the directory: the driver library is never called */
   clear_dlss_runtime("Zdir");
   CHECK(video_upscaler_probe(VIDEO_TEMPORAL_UPSCALER_DLSS) == VIDEO_UPSCALER_FOUND_LIBRARY);
   CHECK(video_upscaler_dlss_version() == 0);
   CHECK(vulkan_ngx_init(instance, gpu, device, vkGetInstanceProcAddr,
            vkGetDeviceProcAddr, &mem_props, "Zdir") == NULL);
   CHECK(rec->init_calls == 0);
   plant_dlss_runtime("Zdir");
   /* The runtime's version, read from its own export before NGX
    * loads it; the status names it */
   CHECK(video_upscaler_probe(VIDEO_TEMPORAL_UPSCALER_DLSS) == VIDEO_UPSCALER_READY);
   CHECK(video_upscaler_dlss_version() == NGX_VERSION_PACK(310, 2, 1));
   CHECK(video_upscaler_preset_min_version(VIDEO_TEMPORAL_UPSCALER_PRESET_K) <= NGX_VERSION_PACK(310, 2, 1));
   CHECK(video_upscaler_preset_min_version(VIDEO_TEMPORAL_UPSCALER_PRESET_O) > NGX_VERSION_PACK(310, 2, 1));
   CHECK(video_upscaler_preset_min_version(VIDEO_TEMPORAL_UPSCALER_PRESET_A) == 0);
   {
      char status[256];
      video_upscaler_status_string(status, sizeof(status));
      CHECK(!strcmp(status, "DLSS (NGX: found, DLSS runtime 310.2.1)"));
   }
   CHECK(!video_upscaler_optiscaler());
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

   /* The mode's size and the range over all modes (Ultra Performance's
    * half of 640x360 the floor), through the capability callback,
    * asked once a mode */
   CHECK(vulkan_ngx_optimal(ngx, NGX_PERF_QUALITY_MAX_QUALITY,
            VIDEO_SCALE_PACK(1920, 1080), &render_dims, &min_dims, &max_dims));
   CHECK(render_dims == VIDEO_SCALE_PACK(1280, 720)
         && max_dims == VIDEO_SCALE_PACK(1920, 1080)
         && min_dims == VIDEO_SCALE_PACK(320, 180));
   CHECK(rec->optimal_calls == 6);
   CHECK(vulkan_ngx_optimal(ngx, NGX_PERF_QUALITY_MAX_QUALITY,
            VIDEO_SCALE_PACK(1920, 1080), &render_dims, &min_dims, &max_dims));
   CHECK(rec->optimal_calls == 6);
   CHECK(!vulkan_ngx_optimal(ngx, NGX_PERF_QUALITY_ULTRA_QUALITY,
            VIDEO_SCALE_PACK(1920, 1080), &render_dims, &min_dims, &max_dims));
   CHECK(!vulkan_ngx_optimal(ngx, NGX_PERF_QUALITY_MAX_QUALITY,
            VIDEO_SCALE_PACK(0, 1080), &render_dims, &min_dims, &max_dims));

   /* The core's request, through the frontend module */
   backend.data    = ngx;
   backend.optimal = optimal_cb;
   video_upscaler_attach(&backend);
   iface = video_upscaler_get_interface();
   CHECK(iface && iface->interface_version == RETRO_HW_RENDER_UPSCALER_INTERFACE_VERSION);
   memset(&req, 0, sizeof(req));
   CHECK(!iface->get_request(iface->handle, &req));   /* no output size yet */
   video_upscaler_set_output(VIDEO_SCALE_PACK(1920, 1080));
   settings->uints.video_temporal_upscaler_quality = VIDEO_TEMPORAL_UPSCALER_QUALITY_AUTO;
   CHECK(iface->get_request(iface->handle, &req));
   CHECK(req.output_width == 1920 && req.output_height == 1080);
   CHECK(req.render_width == 1280 && req.render_height == 720);
   CHECK(req.min_render_width == 320 && req.max_render_width == 1920);
   CHECK(fabs(req.mip_bias - (log(1280.0 / 1920.0) / log(2.0) - 1.0)) < 1e-5);
   settings->uints.video_temporal_upscaler_quality = VIDEO_TEMPORAL_UPSCALER_QUALITY_PERFORMANCE;
   CHECK(iface->get_request(iface->handle, &req) && req.render_width == 960);
   settings->uints.video_temporal_upscaler_quality = VIDEO_TEMPORAL_UPSCALER_QUALITY_NATIVE;
   CHECK(iface->get_request(iface->handle, &req) && req.render_width == 1920
         && fabs(req.mip_bias + 1.0) < 1e-5);
   video_upscaler_set_output(VIDEO_SCALE_PACK(3840, 2160));
   settings->uints.video_temporal_upscaler_quality = VIDEO_TEMPORAL_UPSCALER_QUALITY_AUTO;
   CHECK(iface->get_request(iface->handle, &req) && req.render_width == (unsigned)(3840 * 0.58));
   settings->uints.video_temporal_upscaler = 0;
   CHECK(!iface->get_request(iface->handle, &req));
   settings->uints.video_temporal_upscaler = VIDEO_TEMPORAL_UPSCALER_DLSS;
   settings->uints.video_temporal_upscaler_quality = VIDEO_TEMPORAL_UPSCALER_QUALITY_QUALITY;
   video_upscaler_set_output(VIDEO_SCALE_PACK(1920, 1080));

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
   in.dims   = VIDEO_SCALE_PACK(color.width, color.height);
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
   CHECK(vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, 0,
            VIDEO_SCALE_PACK(1920, 1080), &out));
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
   CHECK(out.dims == VIDEO_SCALE_PACK(1920, 1080) && out.format == VK_FORMAT_R8G8B8A8_UNORM
         && out.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

   /* The next frame: the same feature, no reset, the core's reset
    * honoured when asked; a frame time the core left at 0 is the
    * period of its frame (DLSS weighs its history by it, 0 is not a
    * frame) */
   inputs.frame_time_ms = 0.0f;
   stub_core_hz         = 50.0f;
   begin_frame();
   CHECK(vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, 0,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   vulkan_ngx_frame_done(ngx);
   CHECK(rec->creates == 1 && rec->evals == 2 && rec->eval[1].reset == 0);
   CHECK(rec->eval[1].frame_time == 20.0f);
   inputs.frame_time_ms = 16.6f;
   stub_core_hz         = 0.0f;
   /* (and the core's transparency mask reaches DLSS as the mask it
    * reads, the current color's bias; a stand-in image) */
   inputs.flags |= RETRO_HW_UPSCALER_RESET;
   set_input(&inputs.transparency_mask, &mv);
   begin_frame();
   CHECK(vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, 0,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   vulkan_ngx_frame_done(ngx);
   CHECK(rec->evals == 3 && rec->eval[2].reset == 1 && rec->creates == 1);
   CHECK(rec->eval[2].transparency.present
         && rec->eval[2].transparency.image == (uint64_t)(uintptr_t)mv.image);
   inputs.flags &= ~RETRO_HW_UPSCALER_RESET;
   memset(&inputs.transparency_mask, 0, sizeof(inputs.transparency_mask));

   /* A smaller frame within the range: the same feature, the subrect
    * says the size; one outside: refused */
   in.dims   = VIDEO_SCALE_PACK(960, 540);
   begin_frame();
   CHECK(vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, 0,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   vulkan_ngx_frame_done(ngx);
   CHECK(rec->creates == 1 && rec->evals == 4
         && rec->eval[3].subrect_w == 960 && rec->eval[3].subrect_h == 540);
   in.image  = color_big.image;
   in.view   = color_big.view;
   in.dims   = VIDEO_SCALE_PACK(color_big.width, color_big.height);
   begin_frame();
   CHECK(!vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, 0,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   vulkan_ngx_frame_done(ngx);
   CHECK(rec->evals == 4);
   in.image  = color.image;
   in.view   = color.view;
   in.dims   = VIDEO_SCALE_PACK(color.width, color.height);

   /* No inputs, no motion vectors: refused */
   CHECK(!vulkan_ngx_upscale(ngx, cmd, &in, NULL, NGX_PERF_QUALITY_MAX_QUALITY, 0,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   inputs.motion_vectors.image = 0;
   CHECK(!vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, 0,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   set_input(&inputs.motion_vectors, &mv);

   /* A resize: a new feature and output, the old retired and released
    * once the frames that could read them are through */
   begin_frame();
   CHECK(vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, 0,
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   vulkan_ngx_frame_done(ngx);
   CHECK(rec->creates == 2 && rec->create[1].width == 2560 && rec->create[1].out_width == 2560);
   CHECK(out.dims == VIDEO_SCALE_PACK(2560, 1440));
   CHECK(rec->releases == 0);
   CHECK(rec->eval[4].reset == 1);
   for (i = 0; i < 30 && rec->releases == 0; i++)
      vulkan_ngx_frame_done(ngx);
   CHECK(rec->releases == 1 && rec->last_released == 1);
   CHECK(i >= 15);

   /* A mode change makes a new feature too */
   begin_frame();
   CHECK(vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_BALANCED, 0,
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   vulkan_ngx_frame_done(ngx);
   CHECK(rec->creates == 3 && rec->create[2].quality == NGX_PERF_QUALITY_BALANCED);
   CHECK(rec->create[2].preset == NGX_DLSS_PRESET_DEFAULT);

   /* The render preset in the mode word: a new feature with the hint set */
   begin_frame();
   CHECK(vulkan_ngx_upscale(ngx, cmd, &in, &inputs, (NGX_MODE_PACK(NGX_PERF_QUALITY_BALANCED, NGX_DLSS_PRESET_K)), 0,
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   vulkan_ngx_frame_done(ngx);
   CHECK(rec->creates == 4 && rec->create[3].quality == NGX_PERF_QUALITY_BALANCED
         && rec->create[3].preset == NGX_DLSS_PRESET_K);
   begin_frame();
   CHECK(vulkan_ngx_upscale(ngx, cmd, &in, &inputs, (NGX_MODE_PACK(NGX_PERF_QUALITY_BALANCED, NGX_DLSS_PRESET_K)), 0,
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   vulkan_ngx_frame_done(ngx);
   CHECK(rec->creates == 4);
   begin_frame();
   CHECK(vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_BALANCED, 0,
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   vulkan_ngx_frame_done(ngx);
   CHECK(rec->creates == 5 && rec->create[4].preset == NGX_DLSS_PRESET_DEFAULT);

   /* An evaluate that fails: the frame shown as it is, the feature left
    * alone until something new is asked of it */
   rec->eval_fails = 1;
   begin_frame();
   CHECK(!vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_BALANCED, 0,
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   vulkan_ngx_frame_done(ngx);
   rec->eval_fails = 0;
   begin_frame();
   CHECK(!vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_BALANCED, 0,
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   vulkan_ngx_frame_done(ngx);
   begin_frame();
   CHECK(vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, 0,
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   vulkan_ngx_frame_done(ngx);
   CHECK(rec->creates == 6);

   /* A feature that will not create: refused, nothing leaks */
   rec->create_fails = 1;
   begin_frame();
   CHECK(!vulkan_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_PERF, 0,
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   vulkan_ngx_frame_done(ngx);
   rec->create_fails = 0;

   /* Teardown releases everything, retired or current */
   vkDeviceWaitIdle(device);
   {
      unsigned releases = rec->releases;
      vulkan_ngx_free(ngx);
      CHECK(rec->releases == releases + 5);  /* the five retired */
      CHECK(rec->shutdown_calls == 2 && rec->params_destroyed == 2);
      CHECK(rec->shutdown_path0 == 'Z');
   }
   video_upscaler_detach();
   CHECK(video_upscaler_get_interface() == NULL);

   test_mode_for(&color, &mv, &depth);
   test_barriers(&color, &mv, &depth);
   test_nr(settings, &color, &mv, &depth);

   /* DLSS behind the dispatch: the frontend's mode word translated to
    * NGX's quality, preset and sharpness; the session's tables name
    * OptiScaler, so the status does */
   rec->optiscaler = 1;
   {
      vulkan_upscaler_t *up = vulkan_upscaler_init(VIDEO_TEMPORAL_UPSCALER_DLSS,
            instance, gpu, device, vkGetInstanceProcAddr, vkGetDeviceProcAddr,
            &mem_props, "Zdir");
      vulkan_upscaler_image_t din, dout;
      unsigned creates = rec->creates;
      char status[256];
      CHECK(up != NULL);
      CHECK(video_upscaler_optiscaler());
      video_upscaler_status_string(status, sizeof(status));
      CHECK(!strcmp(status, "DLSS (NGX: found, DLSS runtime 310.2.1, via OptiScaler)"));
      if (up)
      {
         CHECK(vulkan_upscaler_optimal(up,
                  VIDEO_UPSCALER_MODE_PACK(VIDEO_TEMPORAL_UPSCALER_QUALITY_PERFORMANCE, 0),
                  VIDEO_SCALE_PACK(1920, 1080), &render_dims, &min_dims, &max_dims)
               && render_dims == VIDEO_SCALE_PACK(960, 540));
         din.image  = color.image;
         din.view   = color.view;
         din.layout = color.layout;
         din.format = color.format;
         din.dims   = VIDEO_SCALE_PACK(color.width, color.height);
         begin_frame();
         CHECK(vulkan_upscaler_upscale(up, cmd, &din, &inputs,
                  VIDEO_UPSCALER_MODE_PACK(VIDEO_TEMPORAL_UPSCALER_QUALITY_QUALITY,
                     VIDEO_TEMPORAL_UPSCALER_PRESET_K),
                  VIDEO_SCALE_PACK(1920, 1080), &dout));
         end_frame();
         vulkan_upscaler_frame_done(up);
         CHECK(rec->creates == creates + 1
               && rec->create[creates].quality == NGX_PERF_QUALITY_MAX_QUALITY
               && rec->create[creates].preset == NGX_DLSS_PRESET_K
               && rec->create[creates].out_width == 1920);
         CHECK(dout.dims == VIDEO_SCALE_PACK(1920, 1080)
               && dout.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
         CHECK(rec->eval[(rec->evals - 1) % FAKE_NGX_MAX].sharpness == 0.0f);
         /* The sharpness rides in the mode word: the same feature,
          * the parameter set */
         begin_frame();
         CHECK(vulkan_upscaler_upscale(up, cmd, &din, &inputs,
                  VIDEO_UPSCALER_MODE_PACK_SHARP(VIDEO_TEMPORAL_UPSCALER_QUALITY_QUALITY,
                     VIDEO_TEMPORAL_UPSCALER_PRESET_K, 50),
                  VIDEO_SCALE_PACK(1920, 1080), &dout));
         end_frame();
         vulkan_upscaler_frame_done(up);
         CHECK(rec->creates == creates + 1 && rec->eval[(rec->evals - 1) % FAKE_NGX_MAX].sharpness == 0.5f);
         vkDeviceWaitIdle(device);
         vulkan_upscaler_free(up);
      }
   }

   test_ffx(settings, &color, &mv, &depth);
   test_xess(settings, &color, &mv, &depth);

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
   printf("vulkan_upscaler_test: ok (%d checks)\n", checks);
   return 0;
}
