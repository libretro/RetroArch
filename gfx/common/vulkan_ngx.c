/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - Libretro team
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

#include <retro_miscellaneous.h>
#include <compat/strl.h>
#include <features/features_cpu.h>
#include <gfx/nvidia/ngx_loader.h>
#include <vulkan/vulkan_symbol_wrapper.h>

#include "../../verbosity.h"
#include "../video_upscaler.h"
#include "vulkan_ngx.h"

/* RetroArch's NGX project: an id of its own, as NGX asks of applications
 * without an NVIDIA application id */
#define VULKAN_NGX_PROJECT_ID     "3fe3a1ab-9a3c-4b57-a9d7-26c5a2a3b0d1"
#define VULKAN_NGX_ENGINE_VERSION "1.0"

/* Frames an image or feature a submitted frame may read stays alive
 * for, past the swapchain's depth */
#define VULKAN_NGX_RETIRE_FRAMES 20
#define VULKAN_NGX_RETIRED_MAX   8
/* NGX_PERF_QUALITY_MAX_PERF .. DLAA */
#define VULKAN_NGX_MODES         6
/* Frames a changed Neural Rendering setting holds before the feature is
 * made again with it: the model reads its tuning only at creation, and
 * a feature made every frame of a slider's travel exhausts the driver */
#define VULKAN_NGX_NR_SETTLE     30

typedef struct vulkan_ngx_retired
{
   ngx_handle_t *feature;
   VkImage image;
   VkImageView view;
   VkDeviceMemory memory;
   unsigned frames;
} vulkan_ngx_retired_t;

struct vulkan_ngx
{
   const ngx_lib_t *lib;
   ngx_parameter_t *caps;
   ngx_parameter_t *params;
   ngx_handle_t *feature;
   VkDevice device;
   /* What Init was given, alive until after Shutdown: NGX keeps
    * reading it */
   wchar_t *wdir;
   const wchar_t *paths[1];
   ngx_feature_common_info_t info;
   VkPhysicalDeviceMemoryProperties memory_properties;
   vulkan_ngx_retired_t retired[VULKAN_NGX_RETIRED_MAX];
   /* The output image, the feature's size */
   VkImage out_image;
   VkImageView out_view;
   VkDeviceMemory out_memory;
   VkFormat out_format;
   /* What the feature was made for; sizes are VIDEO_SCALE_PACK */
   unsigned feature_out_dims;
   unsigned feature_max_dims;
   unsigned feature_flags;
   unsigned feature_quality;
   /* The last optimal query, kept for the request each frame */
   /* DLSS's sizes for opt_out_dims, per mode (NGX_PERF_QUALITY_*):
    * optimal, and the range of each; opt_valid has a bit per mode
    * asked, opt_ok one per mode that answered */
   unsigned opt_out_dims;
   unsigned opt_valid;
   unsigned opt_ok;
   unsigned opt_render_dims[VULKAN_NGX_MODES];
   unsigned opt_min_dims[VULKAN_NGX_MODES];
   unsigned opt_max_dims[VULKAN_NGX_MODES];
   /* A frame no mode takes, said once */
   unsigned unfit_dims;
   /* What failed last, create or evaluate: not tried again until the
    * output size or mode changes, the frames shown as they are */
   unsigned failed_out_dims;
   unsigned failed_quality;
   /* DLSS Neural Rendering over the output: its own parameters,
    * feature and output image, made for nr_dims with the settings word
    * nr_word (VIDEO_UPSCALER_NR_*); a changed word waits in nr_pending */
   ngx_parameter_t *nr_params;
   ngx_handle_t *nr_feature;
   VkImage nr_image;
   VkImageView nr_view;
   VkDeviceMemory nr_memory;
   VkFormat nr_format;
   unsigned nr_dims;
   unsigned nr_word;
   unsigned nr_pending;
   unsigned nr_pending_frames;
   unsigned nr_failed_word;
   unsigned nr_failed_dims;
   bool nr_reset;
   /* turned off, the feature kept for when it is turned on again */
   bool nr_idle;
   bool nr_unsupported;
   bool reset_pending;
};

static VkImageAspectFlags vulkan_ngx_aspect(VkFormat format)
{
   switch (format)
   {
      case VK_FORMAT_D16_UNORM:
      case VK_FORMAT_X8_D24_UNORM_PACK32:
      case VK_FORMAT_D32_SFLOAT:
         return VK_IMAGE_ASPECT_DEPTH_BIT;
      case VK_FORMAT_D16_UNORM_S8_UINT:
      case VK_FORMAT_D24_UNORM_S8_UINT:
      case VK_FORMAT_D32_SFLOAT_S8_UINT:
         return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
      case VK_FORMAT_S8_UINT:
         return VK_IMAGE_ASPECT_STENCIL_BIT;
      default:
         return VK_IMAGE_ASPECT_COLOR_BIT;
   }
}

static void vulkan_ngx_barrier(VkCommandBuffer cmd, VkImage image,
      VkImageAspectFlags aspect, VkImageLayout old_layout,
      VkImageLayout new_layout, VkAccessFlags src_access,
      VkAccessFlags dst_access, VkPipelineStageFlags src_stages,
      VkPipelineStageFlags dst_stages)
{
   VkImageMemoryBarrier barrier;
   barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
   barrier.pNext                           = NULL;
   barrier.srcAccessMask                   = src_access;
   barrier.dstAccessMask                   = dst_access;
   barrier.oldLayout                       = old_layout;
   barrier.newLayout                       = new_layout;
   barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
   barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
   barrier.image                           = image;
   barrier.subresourceRange.aspectMask     = aspect;
   barrier.subresourceRange.baseMipLevel   = 0;
   barrier.subresourceRange.levelCount     = 1;
   barrier.subresourceRange.baseArrayLayer = 0;
   barrier.subresourceRange.layerCount     = 1;
   vkCmdPipelineBarrier(cmd, src_stages, dst_stages, 0, 0, NULL, 0, NULL,
         1, &barrier);
}

/* Everything written so far, by any stage, before whatever DLSS reads
 * or writes with: which stages it uses is its own business */
static void vulkan_ngx_full_barrier(VkCommandBuffer cmd)
{
   VkMemoryBarrier barrier;
   barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
   barrier.pNext         = NULL;
   barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
   barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
   vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &barrier, 0, NULL, 0, NULL);
}

static bool vulkan_ngx_in_list(const char **list, unsigned count,
      const char *name)
{
   unsigned i;
   for (i = 0; i < count; i++)
      if (!strcmp(list[i], name))
         return true;
   return false;
}

#define VULKAN_NGX_NAMES 32

/* The extension names NGX needs, the instance's or (@gpu set) the
 * device's, into @names: from NVSDK_NGX_VULKAN_RequiredExtensions where
 * the library has it, else from the feature requirements API that
 * replaced it (OptiScaler has only that). The names are NGX's own. */
static unsigned vulkan_ngx_feature_names(const ngx_lib_t *lib,
      VkInstance instance, VkPhysicalDevice gpu, int feature,
      const char **names, unsigned out)
{
   ngx_feature_discovery_info_t disc;
   ngx_feature_common_info_t info;
   const wchar_t *paths[1];
   VkExtensionProperties *props = NULL;
   wchar_t *wdir;
   char dir[DIR_MAX_LENGTH];
   uint32_t n = 0;
   unsigned i;
   ngx_result_t res = NGX_RESULT_FAIL;

   if (feature == NGX_FEATURE_SUPER_SAMPLING && lib->vulkan.required_extensions)
   {
      unsigned ic = 0, dc = 0;
      const char **ie = NULL, **de = NULL;
      if (!NGX_FAILED(lib->vulkan.required_extensions(&ic, &ie, &dc, &de)))
      {
         const char **from = gpu ? de : ie;
         unsigned count    = gpu ? dc : ic;
         for (i = 0; i < count && out < VULKAN_NGX_NAMES; i++)
            names[out++] = from[i];
         return out;
      }
   }
   /* OptiScaler adds NGX's extensions in its own vkCreateInstance and
    * vkCreateDevice hooks, and asking it recurses through them until
    * the stack runs out */
   if (ngx_lib_is_optiscaler())
      return out;
   if (gpu ? !lib->vulkan.get_feature_device_extension_requirements
           : !lib->vulkan.get_feature_instance_extension_requirements)
      return out;
   if (!(wdir = (wchar_t*)calloc(DIR_MAX_LENGTH, sizeof(wchar_t))))
      return out;
   video_upscaler_dir(VIDEO_TEMPORAL_UPSCALER_DLSS, dir, sizeof(dir));
   ngx_utf8_to_wide(wdir, DIR_MAX_LENGTH, dir);
   memset(&info, 0, sizeof(info));
   paths[0]                    = wdir;
   info.path_list.path         = paths;
   info.path_list.length       = 1;
   memset(&disc, 0, sizeof(disc));
   disc.sdk_version            = NGX_VERSION_API;
   disc.feature_id             = feature;
   disc.identifier_type        = 1;
   disc.v.project.project_id   = VULKAN_NGX_PROJECT_ID;
   disc.v.project.engine_type  = NGX_ENGINE_TYPE_CUSTOM;
   disc.v.project.engine_version = VULKAN_NGX_ENGINE_VERSION;
   disc.application_data_path  = wdir;
   disc.feature_info           = &info;
   if (gpu)
      res = lib->vulkan.get_feature_device_extension_requirements(
            (void*)instance, (void*)gpu, &disc, &n, (void**)&props);
   else
      res = lib->vulkan.get_feature_instance_extension_requirements(
            &disc, &n, (void**)&props);
   if (!NGX_FAILED(res) && props)
      for (i = 0; i < n && out < VULKAN_NGX_NAMES; i++)
         names[out++] = props[i].extensionName;
   free(wdir);
   return out;
}

/* The names asked last, the instance's and the device's: what NGX
 * answers does not change while RetroArch runs, and asking loads and
 * checks the feature libraries, every time a context is made */
typedef struct vulkan_ngx_name_cache
{
   char names[VULKAN_NGX_NAMES][VK_MAX_EXTENSION_NAME_SIZE];
   unsigned count;
   /* what they were asked for: the GPU, and Neural Rendering's too */
   uint32_t vendor_id;
   uint32_t device_id;
   uint32_t driver_version;
   bool nr;
   bool valid;
} vulkan_ngx_name_cache_t;

static vulkan_ngx_name_cache_t vulkan_ngx_instance_cache;
static vulkan_ngx_name_cache_t vulkan_ngx_device_cache;

/* Super Resolution's, and Neural Rendering's while its library is in
 * the DLSS directory, so that it can be turned on at any time */
static unsigned vulkan_ngx_names(const ngx_lib_t *lib, VkInstance instance,
      VkPhysicalDevice gpu, PFN_vkGetInstanceProcAddr gipa,
      const char **names)
{
   char dir[DIR_MAX_LENGTH];
   VkPhysicalDeviceProperties props;
   PFN_vkGetPhysicalDeviceProperties get_props = NULL;
   vulkan_ngx_name_cache_t *cache = gpu
      ? &vulkan_ngx_device_cache : &vulkan_ngx_instance_cache;
   unsigned out, i;
   bool nr = false;

   video_upscaler_dir(VIDEO_TEMPORAL_UPSCALER_DLSS, dir, sizeof(dir));
   nr = ngx_dlssnr_feature_in(dir);
   memset(&props, 0, sizeof(props));
   if (gpu && gipa)
      get_props = (PFN_vkGetPhysicalDeviceProperties)
         gipa(instance, "vkGetPhysicalDeviceProperties");
   if (get_props)
      get_props(gpu, &props);
   if (     cache->valid
         && cache->nr             == nr
         && cache->vendor_id      == props.vendorID
         && cache->device_id      == props.deviceID
         && cache->driver_version == props.driverVersion)
   {
      for (i = 0; i < cache->count; i++)
         names[i] = cache->names[i];
      return cache->count;
   }

   out = vulkan_ngx_feature_names(lib, instance, gpu,
         NGX_FEATURE_SUPER_SAMPLING, names, 0);
   if (nr)
      out = vulkan_ngx_feature_names(lib, instance, gpu,
            NGX_FEATURE_DLSS_NR, names, out);

   /* (the names are NGX's: copies go in the cache, and out) */
   for (i = 0; i < out; i++)
   {
      strlcpy(cache->names[i], names[i], sizeof(cache->names[i]));
      names[i] = cache->names[i];
   }
   cache->count          = out;
   cache->vendor_id      = props.vendorID;
   cache->device_id      = props.deviceID;
   cache->driver_version = props.driverVersion;
   cache->nr             = nr;
   cache->valid          = true;
   return out;
}

unsigned vulkan_ngx_instance_extensions(const char **list,
      unsigned count, unsigned cap)
{
   const ngx_lib_t *lib = ngx_lib_get();
   const char *names[VULKAN_NGX_NAMES];
   unsigned n, i;
   if (!lib)
      return count;
   n = vulkan_ngx_names(lib, VK_NULL_HANDLE, VK_NULL_HANDLE, NULL, names);
   for (i = 0; i < n && count < cap; i++)
      if (!vulkan_ngx_in_list(list, count, names[i]))
         list[count++] = names[i];
   return count;
}

unsigned vulkan_ngx_device_extensions(VkPhysicalDevice gpu,
      PFN_vkGetInstanceProcAddr gipa, VkInstance instance,
      const char **list, unsigned count, unsigned cap)
{
   const ngx_lib_t *lib = ngx_lib_get();
   const char *names[VULKAN_NGX_NAMES];
   PFN_vkEnumerateDeviceExtensionProperties enumerate;
   VkExtensionProperties *props = NULL;
   uint32_t nprops = 0;
   unsigned n, i, j;
   if (!lib || !gpu)
      return count;
   if (!(n = vulkan_ngx_names(lib, instance, gpu, gipa, names)))
      return count;
   enumerate = (PFN_vkEnumerateDeviceExtensionProperties)
      gipa(instance, "vkEnumerateDeviceExtensionProperties");
   if (!enumerate || enumerate(gpu, NULL, &nprops, NULL) != VK_SUCCESS || !nprops)
      return count;
   if (!(props = (VkExtensionProperties*)malloc(nprops * sizeof(*props))))
      return count;
   if (enumerate(gpu, NULL, &nprops, props) != VK_SUCCESS)
   {
      free(props);
      return count;
   }
   for (i = 0; i < n && count < cap; i++)
   {
      if (vulkan_ngx_in_list(list, count, names[i]))
         continue;
      for (j = 0; j < nprops; j++)
         if (!strcmp(props[j].extensionName, names[i]))
         {
            list[count++] = names[i];
            break;
         }
   }
   free(props);
   return count;
}

static void vulkan_ngx_nr_unsupported(vulkan_ngx_t *ngx, ngx_result_t res);

vulkan_ngx_t *vulkan_ngx_init(VkInstance instance, VkPhysicalDevice gpu,
      VkDevice device, PFN_vkGetInstanceProcAddr gipa,
      PFN_vkGetDeviceProcAddr gdpa,
      const VkPhysicalDeviceMemoryProperties *memory_properties,
      const char *dlss_dir)
{
   /* The path list and the data path, wide as NGX takes them */
   vulkan_ngx_t *ngx;
   ngx_result_t res;
   int available = 0;
   int in_use = 0;
   const ngx_lib_t *lib;
   retro_time_t start = cpu_features_get_time_usec();

   if (!ngx_dlss_feature_in(dlss_dir ? dlss_dir : ""))
   {
      RARCH_LOG("[Vulkan] Temporal Upscaler: no DLSS runtime in \"%s\", upscaler off.\n",
            dlss_dir ? dlss_dir : "");
      return NULL;
   }
   lib = ngx_lib_get();
   if (!lib || !lib->vulkan.init_project_id)
   {
      RARCH_LOG("[Vulkan] Temporal Upscaler: no NGX core library.\n");
      return NULL;
   }
   if (!(ngx = (vulkan_ngx_t*)calloc(1, sizeof(*ngx))))
      return NULL;
   if (!(ngx->wdir = (wchar_t*)calloc(DIR_MAX_LENGTH, sizeof(wchar_t))))
   {
      free(ngx);
      return NULL;
   }
   ngx_utf8_to_wide(ngx->wdir, DIR_MAX_LENGTH, dlss_dir ? dlss_dir : "");
   ngx->paths[0]              = ngx->wdir;
   ngx->info.path_list.path   = ngx->paths;
   ngx->info.path_list.length = 1;
   if (lib->vulkan.init_project_id_ext)
      res = lib->vulkan.init_project_id_ext(VULKAN_NGX_PROJECT_ID,
            NGX_ENGINE_TYPE_CUSTOM, VULKAN_NGX_ENGINE_VERSION, ngx->wdir,
            (void*)instance, (void*)gpu, (void*)device,
            (ngx_vk_proc_t)gipa, (ngx_vk_proc_t)gdpa,
            NGX_VERSION_API, &ngx->info);
   else
      res = lib->vulkan.init_project_id(VULKAN_NGX_PROJECT_ID,
            NGX_ENGINE_TYPE_CUSTOM, VULKAN_NGX_ENGINE_VERSION, ngx->wdir,
            (void*)instance, (void*)gpu, (void*)device,
            NGX_VERSION_API, &ngx->info);
   if (NGX_FAILED(res))
   {
      RARCH_WARN("[Vulkan] Temporal Upscaler: NGX init failed (0x%08x).\n",
            (unsigned)res);
      free(ngx->wdir);
      free(ngx);
      return NULL;
   }
   RARCH_LOG("[Vulkan] Temporal Upscaler: NGX initialised (%s) in %u ms.\n",
         lib->vulkan.init_project_id_ext ? "Init_ProjectID_Ext" : "Init_ProjectID",
         (unsigned)((cpu_features_get_time_usec() - start) / 1000));
   ngx->lib    = lib;
   ngx->device = device;
   ngx->memory_properties = *memory_properties;
   if (   NGX_FAILED(lib->vulkan.get_capability_parameters(&ngx->caps))
       || NGX_FAILED(lib->param.get_i(ngx->caps,
             NGX_PARAM_SUPER_SAMPLING_AVAILABLE, &available))
       || !available)
   {
      int needs_driver = 0, major = 0, minor = 0, init_result = 0;
      if (ngx->caps)
      {
         lib->param.get_i(ngx->caps, NGX_PARAM_SUPER_SAMPLING_NEEDS_UPDATED_DRIVER, &needs_driver);
         lib->param.get_i(ngx->caps, NGX_PARAM_SUPER_SAMPLING_MIN_DRIVER_MAJOR, &major);
         lib->param.get_i(ngx->caps, NGX_PARAM_SUPER_SAMPLING_MIN_DRIVER_MINOR, &minor);
         lib->param.get_i(ngx->caps, NGX_PARAM_SUPER_SAMPLING_FEATURE_INIT_RESULT, &init_result);
      }
      if (needs_driver)
         RARCH_WARN("[Vulkan] Temporal Upscaler: DLSS needs driver %d.%d or newer.\n",
               major, minor);
      else
         RARCH_WARN("[Vulkan] Temporal Upscaler: DLSS Super Resolution not available on this device (0x%08x).\n",
               (unsigned)init_result);
      lib->vulkan.shutdown1((void*)device, &in_use);
      free(ngx->wdir);
      free(ngx);
      return NULL;
   }
   if (NGX_FAILED(lib->vulkan.allocate_parameters(&ngx->params)))
   {
      lib->vulkan.shutdown1((void*)device, &in_use);
      free(ngx->wdir);
      free(ngx);
      return NULL;
   }
   if (ngx_params_from_optiscaler(lib, ngx->caps))
   {
      video_upscaler_note_optiscaler();
      RARCH_LOG("[Vulkan] Temporal Upscaler: NGX answered by OptiScaler.\n");
   }
   else if (ngx_dlssnr_feature_in(dlss_dir ? dlss_dir : ""))
   {
      /* What the core says of Neural Rendering: a core without it has
       * no such capability */
      int nr = 0;
      if (     NGX_FAILED(lib->param.get_i(ngx->caps, NGX_PARAM_DLSSNR_AVAILABLE, &nr))
            || !nr)
      {
         int init_result = 0;
         lib->param.get_i(ngx->caps, NGX_PARAM_DLSSNR_FEATURE_INIT_RESULT, &init_result);
         vulkan_ngx_nr_unsupported(ngx, (ngx_result_t)init_result);
      }
   }
   RARCH_LOG("[Vulkan] Temporal Upscaler: DLSS Super Resolution ready in %u ms.\n",
         (unsigned)((cpu_features_get_time_usec() - start) / 1000));
   return ngx;
}

/* The mode's name for the log: the quality, and the preset when one is asked for */
static const char *vulkan_ngx_quality_name(unsigned mode)
{
   switch (NGX_MODE_QUALITY(mode))
   {
      case NGX_PERF_QUALITY_MAX_PERF:          return "Performance";
      case NGX_PERF_QUALITY_BALANCED:          return "Balanced";
      case NGX_PERF_QUALITY_MAX_QUALITY:       return "Quality";
      case NGX_PERF_QUALITY_ULTRA_PERFORMANCE: return "Ultra Performance";
      case NGX_PERF_QUALITY_ULTRA_QUALITY:     return "Ultra Quality";
      case NGX_PERF_QUALITY_DLAA:              return "DLAA";
      default:                                 break;
   }
   return "?";
}

static const char *vulkan_ngx_preset_name(unsigned mode)
{
   switch (NGX_MODE_PRESET(mode))
   {
      case NGX_DLSS_PRESET_J: return ", preset J";
      case NGX_DLSS_PRESET_K: return ", preset K";
      default:                break;
   }
   return "";
}

/* The render preset hints, one a quality: the one asked for, or the library's default */
static void vulkan_ngx_set_preset(const ngx_lib_t *lib, ngx_parameter_t *p, unsigned mode)
{
   const unsigned preset = NGX_MODE_PRESET(mode);
   lib->param.set_ui(p, NGX_PARAM_DLSS_PRESET_DLAA, preset);
   lib->param.set_ui(p, NGX_PARAM_DLSS_PRESET_QUALITY, preset);
   lib->param.set_ui(p, NGX_PARAM_DLSS_PRESET_BALANCED, preset);
   lib->param.set_ui(p, NGX_PARAM_DLSS_PRESET_PERFORMANCE, preset);
   lib->param.set_ui(p, NGX_PARAM_DLSS_PRESET_ULTRA_PERFORMANCE, preset);
   lib->param.set_ui(p, NGX_PARAM_DLSS_PRESET_ULTRA_QUALITY, preset);
}

/* DLSS's sizes for mode @q (NGX_PERF_QUALITY_*) and @out_dims,
 * asked of NGX once per output size */
static bool vulkan_ngx_mode_sizes(vulkan_ngx_t *ngx, unsigned q, unsigned out_dims)
{
   const ngx_lib_t *lib = ngx->lib;
   ngx_parameter_t *p   = ngx->caps;
   void *callback       = NULL;
   unsigned rw = 0, rh = 0, mnw, mnh, mxw, mxh;
   ngx_result_t (NGX_CONV *optimal)(ngx_parameter_t *p);
   if (q >= VULKAN_NGX_MODES)
      return false;
   if (ngx->opt_out_dims != out_dims)
   {
      ngx->opt_out_dims = out_dims;
      ngx->opt_valid    = 0;
      ngx->opt_ok       = 0;
   }
   if (ngx->opt_valid & (1u << q))
      return (ngx->opt_ok & (1u << q)) != 0;
   ngx->opt_valid |= 1u << q;
   lib->param.get_void_pointer(p, NGX_PARAM_DLSS_OPTIMAL_SETTINGS_CALLBACK,
         &callback);
   if (!callback)
      return false;
   memcpy(&optimal, &callback, sizeof(optimal));
   lib->param.set_ui(p, NGX_PARAM_WIDTH, VIDEO_SCALE_W(out_dims));
   lib->param.set_ui(p, NGX_PARAM_HEIGHT, VIDEO_SCALE_H(out_dims));
   lib->param.set_i(p, NGX_PARAM_PERF_QUALITY, (int)q);
   lib->param.set_i(p, NGX_PARAM_RTX_VALUE, 0);
   if (NGX_FAILED(optimal(p)))
      return false;
   lib->param.get_ui(p, NGX_PARAM_OUT_WIDTH, &rw);
   lib->param.get_ui(p, NGX_PARAM_OUT_HEIGHT, &rh);
   if (!rw || !rh)
      return false;
   mxw = mnw = rw;
   mxh = mnh = rh;
   lib->param.get_ui(p, NGX_PARAM_DLSS_DYNAMIC_MAX_RENDER_WIDTH, &mxw);
   lib->param.get_ui(p, NGX_PARAM_DLSS_DYNAMIC_MAX_RENDER_HEIGHT, &mxh);
   lib->param.get_ui(p, NGX_PARAM_DLSS_DYNAMIC_MIN_RENDER_WIDTH, &mnw);
   lib->param.get_ui(p, NGX_PARAM_DLSS_DYNAMIC_MIN_RENDER_HEIGHT, &mnh);
   if (mxw < rw || mxh < rh)
   {
      mxw = rw;
      mxh = rh;
   }
   if (!mnw || !mnh || mnw > rw || mnh > rh)
   {
      mnw = rw;
      mnh = rh;
   }
   ngx->opt_render_dims[q] = VIDEO_SCALE_PACK(rw, rh);
   ngx->opt_min_dims[q]    = VIDEO_SCALE_PACK(mnw, mnh);
   ngx->opt_max_dims[q]    = VIDEO_SCALE_PACK(mxw, mxh);
   ngx->opt_ok            |= 1u << q;
   return true;
}

static bool vulkan_ngx_mode_takes(const vulkan_ngx_t *ngx, unsigned q, unsigned dims)
{
   return  VIDEO_SCALE_W(dims) >= VIDEO_SCALE_W(ngx->opt_min_dims[q])
        && VIDEO_SCALE_W(dims) <= VIDEO_SCALE_W(ngx->opt_max_dims[q])
        && VIDEO_SCALE_H(dims) >= VIDEO_SCALE_H(ngx->opt_min_dims[q])
        && VIDEO_SCALE_H(dims) <= VIDEO_SCALE_H(ngx->opt_max_dims[q]);
}

/* The size @quality's mode wants, and the range DLSS takes over all its
 * modes: a core that renders in coarse steps (a factor of the console's
 * resolution) may land outside the one mode's range, and then gets the
 * mode whose range it is in (vulkan_ngx_mode_for) */
bool vulkan_ngx_optimal(vulkan_ngx_t *ngx, unsigned quality,
      unsigned out_dims, unsigned *render_dims,
      unsigned *min_dims, unsigned *max_dims)
{
   unsigned q, mnw = 0, mnh = 0, mxw = 0, mxh = 0;
   if (!ngx || !VIDEO_SCALE_W(out_dims) || !VIDEO_SCALE_H(out_dims))
      return false;
   if (!vulkan_ngx_mode_sizes(ngx, NGX_MODE_QUALITY(quality), out_dims))
      return false;
   for (q = 0; q < VULKAN_NGX_MODES; q++)
   {
      unsigned mn, mx;
      if (!vulkan_ngx_mode_sizes(ngx, q, out_dims))
         continue;
      mn = ngx->opt_min_dims[q];
      mx = ngx->opt_max_dims[q];
      if (!mnw || VIDEO_SCALE_W(mn) < mnw)
         mnw = VIDEO_SCALE_W(mn);
      if (!mnh || VIDEO_SCALE_H(mn) < mnh)
         mnh = VIDEO_SCALE_H(mn);
      if (VIDEO_SCALE_W(mx) > mxw)
         mxw = VIDEO_SCALE_W(mx);
      if (VIDEO_SCALE_H(mx) > mxh)
         mxh = VIDEO_SCALE_H(mx);
   }
   *render_dims = ngx->opt_render_dims[NGX_MODE_QUALITY(quality)];
   *min_dims    = VIDEO_SCALE_PACK(mnw, mnh);
   *max_dims    = VIDEO_SCALE_PACK(mxw, mxh);
   return true;
}

/* The mode word for a frame of @dims: @quality's when its mode takes
 * it, else the mode taking it whose wanted size is nearest; ~0u when
 * none does. *@max_dims is that mode's largest size. */
static unsigned vulkan_ngx_mode_for(vulkan_ngx_t *ngx, unsigned quality,
      unsigned out_dims, unsigned dims, unsigned *max_dims)
{
   unsigned q, best = ~0u, best_distance = 0;
   unsigned want = NGX_MODE_QUALITY(quality);
   if (     vulkan_ngx_mode_sizes(ngx, want, out_dims)
         && vulkan_ngx_mode_takes(ngx, want, dims))
      best = want;
   else
      for (q = 0; q < VULKAN_NGX_MODES; q++)
      {
         unsigned w, distance;
         if (     !vulkan_ngx_mode_sizes(ngx, q, out_dims)
               || !vulkan_ngx_mode_takes(ngx, q, dims))
            continue;
         w        = VIDEO_SCALE_W(ngx->opt_render_dims[q]);
         distance = w > VIDEO_SCALE_W(dims) ? w - VIDEO_SCALE_W(dims)
                                            : VIDEO_SCALE_W(dims) - w;
         if (best == ~0u || distance < best_distance)
         {
            best          = q;
            best_distance = distance;
         }
      }
   if (best == ~0u)
      return ~0u;
   *max_dims = ngx->opt_max_dims[best];
   return NGX_MODE_PACK_SHARP(best, NGX_MODE_PRESET(quality),
         NGX_MODE_SHARPNESS(quality));
}

/* A feature and its output image go to the retired ring, destroyed
 * once every frame that could use them has gone by; the handles are
 * cleared */
static void vulkan_ngx_retire_one(vulkan_ngx_t *ngx, ngx_handle_t **feature,
      VkImage *image, VkImageView *view, VkDeviceMemory *memory)
{
   unsigned i;
   vulkan_ngx_retired_t *slot = NULL;
   if (!*feature && !*image)
      return;
   for (i = 0; i < VULKAN_NGX_RETIRED_MAX; i++)
      if (!ngx->retired[i].frames)
      {
         slot = &ngx->retired[i];
         break;
      }
   if (!slot)
   {
      /* The ring is full of retirees from a run of resizes: the oldest
       * has had every frame of a swapchain's depth to finish */
      slot = &ngx->retired[0];
      for (i = 1; i < VULKAN_NGX_RETIRED_MAX; i++)
         if (ngx->retired[i].frames < slot->frames)
            slot = &ngx->retired[i];
      if (slot->feature)
         ngx->lib->vulkan.release_feature(slot->feature);
      if (slot->view)
         vkDestroyImageView(ngx->device, slot->view, NULL);
      if (slot->image)
         vkDestroyImage(ngx->device, slot->image, NULL);
      if (slot->memory)
         vkFreeMemory(ngx->device, slot->memory, NULL);
   }
   slot->feature = *feature;
   slot->image   = *image;
   slot->view    = *view;
   slot->memory  = *memory;
   slot->frames  = VULKAN_NGX_RETIRE_FRAMES;
   *feature      = NULL;
   *image        = VK_NULL_HANDLE;
   *view         = VK_NULL_HANDLE;
   *memory       = VK_NULL_HANDLE;
}

static void vulkan_ngx_retire(vulkan_ngx_t *ngx)
{
   vulkan_ngx_retire_one(ngx, &ngx->feature, &ngx->out_image,
         &ngx->out_view, &ngx->out_memory);
}

static void vulkan_ngx_nr_retire(vulkan_ngx_t *ngx)
{
   vulkan_ngx_retire_one(ngx, &ngx->nr_feature, &ngx->nr_image,
         &ngx->nr_view, &ngx->nr_memory);
   ngx->nr_dims = 0;
}

void vulkan_ngx_frame_done(vulkan_ngx_t *ngx)
{
   unsigned i;
   int in_use = 0;
   if (!ngx)
      return;
   for (i = 0; i < VULKAN_NGX_RETIRED_MAX; i++)
   {
      vulkan_ngx_retired_t *r = &ngx->retired[i];
      if (!r->frames || --r->frames)
         continue;
      if (r->feature)
         ngx->lib->vulkan.release_feature(r->feature);
      if (r->view)
         vkDestroyImageView(ngx->device, r->view, NULL);
      if (r->image)
         vkDestroyImage(ngx->device, r->image, NULL);
      if (r->memory)
         vkFreeMemory(ngx->device, r->memory, NULL);
      memset(r, 0, sizeof(*r));
   }
}

/* NGX unloads its feature libraries at every shutdown: kept, a context
 * made again (content loaded, the driver restarted) finds them in */
static void vulkan_ngx_keep_features(void)
{
   char dir[DIR_MAX_LENGTH];
   video_upscaler_dir(VIDEO_TEMPORAL_UPSCALER_DLSS, dir, sizeof(dir));
   ngx_lib_keep_features(dir);
}

static bool vulkan_ngx_create_output(vulkan_ngx_t *ngx, VkFormat format,
      unsigned dims, VkImage *out_image, VkImageView *out_view,
      VkDeviceMemory *out_memory)
{
   VkImageCreateInfo image;
   VkImageViewCreateInfo view;
   VkMemoryRequirements reqs;
   VkMemoryAllocateInfo alloc;
   uint32_t type;
   unsigned i;

   image.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   image.pNext                 = NULL;
   image.flags                 = 0;
   image.imageType             = VK_IMAGE_TYPE_2D;
   image.format                = format;
   image.extent.width          = VIDEO_SCALE_W(dims);
   image.extent.height         = VIDEO_SCALE_H(dims);
   image.extent.depth          = 1;
   image.mipLevels             = 1;
   image.arrayLayers           = 1;
   image.samples               = VK_SAMPLE_COUNT_1_BIT;
   image.tiling                = VK_IMAGE_TILING_OPTIMAL;
   /* (written by the library however it likes to: a transfer too) */
   image.usage                 = VK_IMAGE_USAGE_STORAGE_BIT
                               | VK_IMAGE_USAGE_SAMPLED_BIT
                               | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                               | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
   image.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
   image.queueFamilyIndexCount = 0;
   image.pQueueFamilyIndices   = NULL;
   image.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
   if (vkCreateImage(ngx->device, &image, NULL, out_image) != VK_SUCCESS)
      return false;

   vkGetImageMemoryRequirements(ngx->device, *out_image, &reqs);
   type = ngx->memory_properties.memoryTypeCount;
   for (i = 0; i < ngx->memory_properties.memoryTypeCount; i++)
      if (   (reqs.memoryTypeBits & (1u << i))
          && (ngx->memory_properties.memoryTypes[i].propertyFlags
             & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
      {
         type = i;
         break;
      }
   if (type == ngx->memory_properties.memoryTypeCount)
      for (i = 0; i < ngx->memory_properties.memoryTypeCount; i++)
         if (reqs.memoryTypeBits & (1u << i))
         {
            type = i;
            break;
         }
   alloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   alloc.pNext           = NULL;
   alloc.allocationSize  = reqs.size;
   alloc.memoryTypeIndex = type;
   if (   type == ngx->memory_properties.memoryTypeCount
       || vkAllocateMemory(ngx->device, &alloc, NULL, out_memory) != VK_SUCCESS
       || vkBindImageMemory(ngx->device, *out_image, *out_memory, 0) != VK_SUCCESS)
      goto fail;

   view.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   view.pNext                           = NULL;
   view.flags                           = 0;
   view.image                           = *out_image;
   view.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
   view.format                          = format;
   view.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
   view.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
   view.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
   view.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
   view.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
   view.subresourceRange.baseMipLevel   = 0;
   view.subresourceRange.levelCount     = 1;
   view.subresourceRange.baseArrayLayer = 0;
   view.subresourceRange.layerCount     = 1;
   if (vkCreateImageView(ngx->device, &view, NULL, out_view) != VK_SUCCESS)
      goto fail;
   return true;

fail:
   if (*out_memory)
      vkFreeMemory(ngx->device, *out_memory, NULL);
   vkDestroyImage(ngx->device, *out_image, NULL);
   *out_image  = VK_NULL_HANDLE;
   *out_memory = VK_NULL_HANDLE;
   return false;
}

static bool vulkan_ngx_create_feature(vulkan_ngx_t *ngx,
      VkCommandBuffer cmd, VkFormat format, unsigned quality,
      unsigned flags, unsigned out_dims, unsigned max_dims)
{
   const ngx_lib_t *lib = ngx->lib;
   ngx_parameter_t *p   = ngx->params;
   ngx_result_t res;
   retro_time_t start;

   vulkan_ngx_retire(ngx);
   if (!vulkan_ngx_create_output(ngx, format, out_dims, &ngx->out_image,
            &ngx->out_view, &ngx->out_memory))
   {
      RARCH_ERR("[Vulkan] Temporal Upscaler: no output image of %ux%u.\n",
            VIDEO_SCALE_W(out_dims), VIDEO_SCALE_H(out_dims));
      return false;
   }
   ngx->out_format = format;
   lib->param.set_ui(p, NGX_PARAM_CREATION_NODE_MASK, 1);
   lib->param.set_ui(p, NGX_PARAM_VISIBILITY_NODE_MASK, 1);
   lib->param.set_ui(p, NGX_PARAM_WIDTH, VIDEO_SCALE_W(max_dims));
   lib->param.set_ui(p, NGX_PARAM_HEIGHT, VIDEO_SCALE_H(max_dims));
   lib->param.set_ui(p, NGX_PARAM_OUT_WIDTH, VIDEO_SCALE_W(out_dims));
   lib->param.set_ui(p, NGX_PARAM_OUT_HEIGHT, VIDEO_SCALE_H(out_dims));
   lib->param.set_i(p, NGX_PARAM_PERF_QUALITY, (int)NGX_MODE_QUALITY(quality));
   vulkan_ngx_set_preset(lib, p, quality);
   lib->param.set_i(p, NGX_PARAM_DLSS_CREATE_FLAGS, (int)flags);
   lib->param.set_i(p, NGX_PARAM_DLSS_ENABLE_OUTPUT_SUBRECTS, 0);
   start = cpu_features_get_time_usec();
   res   = lib->vulkan.create_feature1((void*)ngx->device, (void*)cmd,
         NGX_FEATURE_SUPER_SAMPLING, p, &ngx->feature);
   if (NGX_FAILED(res))
   {
      RARCH_ERR("[Vulkan] Temporal Upscaler: DLSS feature %ux%u -> %ux%u failed (0x%08x).\n",
            VIDEO_SCALE_W(max_dims), VIDEO_SCALE_H(max_dims),
            VIDEO_SCALE_W(out_dims), VIDEO_SCALE_H(out_dims), (unsigned)res);
      ngx->feature = NULL;
      vulkan_ngx_retire(ngx);
      ngx->failed_out_dims = out_dims;
      ngx->failed_quality  = quality;
      return false;
   }
   ngx->feature_out_dims = out_dims;
   ngx->feature_max_dims = max_dims;
   ngx->feature_flags    = flags;
   ngx->feature_quality  = NGX_MODE_FEATURE(quality);
   ngx->reset_pending    = true;
   vulkan_ngx_keep_features();
   RARCH_LOG("[Vulkan] Temporal Upscaler: DLSS feature up to %ux%u -> %ux%u (%s%s), flags 0x%x, in %u ms.\n",
         VIDEO_SCALE_W(max_dims), VIDEO_SCALE_H(max_dims),
         VIDEO_SCALE_W(out_dims), VIDEO_SCALE_H(out_dims),
         vulkan_ngx_quality_name(quality), vulkan_ngx_preset_name(quality), flags,
         (unsigned)((cpu_features_get_time_usec() - start) / 1000));
   return true;
}

static void vulkan_ngx_vk_resource(ngx_vk_resource_t *r,
      const struct retro_hw_upscaler_image *img, bool read_write)
{
   memset(r, 0, sizeof(*r));
   r->resource.image_view.image_view = (void*)(uintptr_t)img->view;
   r->resource.image_view.image      = (void*)(uintptr_t)img->image;
   /* A depth-stencil image is sampled through its depth alone */
   r->resource.image_view.subresource_range.aspect_mask =
      vulkan_ngx_aspect((VkFormat)img->format) & ~VK_IMAGE_ASPECT_STENCIL_BIT;
   r->resource.image_view.subresource_range.level_count = 1;
   r->resource.image_view.subresource_range.layer_count = 1;
   r->resource.image_view.format = (int)img->format;
   r->resource.image_view.width  = img->width;
   r->resource.image_view.height = img->height;
   r->type       = 0;
   r->read_write = read_write ? 1 : 0;
}

/* An input in a layout a compute shader cannot sample from is moved
 * for the evaluate, and moved back after */
static bool vulkan_ngx_input_needs_move(const struct retro_hw_upscaler_image *img)
{
   return img->image
      && img->state != (unsigned)VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
      && img->state != (unsigned)VK_IMAGE_LAYOUT_GENERAL;
}

static void vulkan_ngx_move_input(VkCommandBuffer cmd,
      const struct retro_hw_upscaler_image *img, bool to_read)
{
   VkImageLayout layout = (VkImageLayout)img->state;
   vulkan_ngx_barrier(cmd, (VkImage)(uintptr_t)img->image,
         vulkan_ngx_aspect((VkFormat)img->format),
         to_read ? layout : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
         to_read ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : layout,
         to_read ? VK_ACCESS_MEMORY_WRITE_BIT : VK_ACCESS_MEMORY_READ_BIT,
         to_read ? VK_ACCESS_MEMORY_READ_BIT : VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
}

/* Whether a create's result says NGX's core has no Neural Rendering */
static bool vulkan_ngx_nr_missing(ngx_result_t res)
{
   return res == NGX_RESULT_FAIL_OUT_OF_DATE
       || res == NGX_RESULT_FAIL_FEATURE_NOT_SUPPORTED
       || res == NGX_RESULT_FAIL_NOT_IMPLEMENTED;
}

static void vulkan_ngx_nr_unsupported(vulkan_ngx_t *ngx, ngx_result_t res)
{
   ngx->nr_unsupported = true;
   video_upscaler_note_nr(false);
   RARCH_WARN("[Vulkan] Temporal Upscaler: DLSS Neural Rendering needs a newer NVIDIA driver (0x%08x).\n",
         (unsigned)res);
}

static bool vulkan_ngx_nr_create(vulkan_ngx_t *ngx, VkCommandBuffer cmd,
      VkFormat format, unsigned dims, unsigned nr)
{
   const ngx_lib_t *lib = ngx->lib;
   ngx_parameter_t *p;
   unsigned skin;
   ngx_result_t res;
   retro_time_t start;

   vulkan_ngx_nr_retire(ngx);
   if (!ngx->nr_params && NGX_FAILED(lib->vulkan.allocate_parameters(&ngx->nr_params)))
   {
      ngx->nr_params = NULL;
      return false;
   }
   p = ngx->nr_params;
   if (!vulkan_ngx_create_output(ngx, format, dims, &ngx->nr_image,
            &ngx->nr_view, &ngx->nr_memory))
   {
      RARCH_ERR("[Vulkan] Temporal Upscaler: no Neural Rendering image of %ux%u.\n",
            VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims));
      return false;
   }
   skin = VIDEO_UPSCALER_NR_SKIN(nr);
   lib->param.set_ui(p, NGX_PARAM_CREATION_NODE_MASK, 1);
   lib->param.set_ui(p, NGX_PARAM_VISIBILITY_NODE_MASK, 1);
   lib->param.set_ui(p, NGX_PARAM_DLSSNR_ENABLED, 1);
   lib->param.set_ui(p, NGX_PARAM_DLSSNR_WIDTH, VIDEO_SCALE_W(dims));
   lib->param.set_ui(p, NGX_PARAM_DLSSNR_HEIGHT, VIDEO_SCALE_H(dims));
   lib->param.set_ui(p, NGX_PARAM_DLSSNR_PRESET, VIDEO_UPSCALER_NR_PRESET(nr));
   lib->param.set_ui(p, NGX_PARAM_DLSSNR_STYLE, VIDEO_UPSCALER_NR_STYLE(nr));
   lib->param.set_f(p, NGX_PARAM_DLSSNR_INTENSITY,
         VIDEO_UPSCALER_NR_INTENSITY(nr) * VIDEO_UPSCALER_NR_STEP);
   lib->param.set_f(p, NGX_PARAM_DLSSNR_LOCAL_STRUCTURE,
         VIDEO_UPSCALER_NR_STRUCTURE(nr) * VIDEO_UPSCALER_NR_STEP);
   lib->param.set_f(p, NGX_PARAM_DLSSNR_LOCAL_TONE,
         VIDEO_UPSCALER_NR_TONE(nr) * VIDEO_UPSCALER_NR_STEP);
   lib->param.set_f(p, NGX_PARAM_DLSSNR_SKIN_STRUCTURE,
         skin ? (skin - 1) * VIDEO_UPSCALER_NR_STEP : -1.0f);
   lib->param.set_ui(p, NGX_PARAM_DLSSNR_AUTO_MASK,
         (nr & VIDEO_UPSCALER_NR_AUTO_MASK) ? 1 : 0);
   start = cpu_features_get_time_usec();
   res   = lib->vulkan.create_feature1((void*)ngx->device, (void*)cmd,
         NGX_FEATURE_DLSS_NR, p, &ngx->nr_feature);
   if (NGX_FAILED(res))
   {
      ngx->nr_feature = NULL;
      vulkan_ngx_nr_retire(ngx);
      if (vulkan_ngx_nr_missing(res))
         vulkan_ngx_nr_unsupported(ngx, res);
      else
         RARCH_ERR("[Vulkan] Temporal Upscaler: DLSS Neural Rendering feature %ux%u failed (0x%08x).\n",
               VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims), (unsigned)res);
      ngx->nr_failed_word = nr;
      ngx->nr_failed_dims = dims;
      return false;
   }
   video_upscaler_note_nr(true);
   ngx->nr_dims           = dims;
   ngx->nr_word           = nr;
   ngx->nr_pending        = nr;
   ngx->nr_pending_frames = 0;
   ngx->nr_format         = format;
   ngx->nr_reset          = true;
   vulkan_ngx_keep_features();
   RARCH_LOG("[Vulkan] Temporal Upscaler: DLSS Neural Rendering feature at %ux%u, in %u ms.\n",
         VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims),
         (unsigned)((cpu_features_get_time_usec() - start) / 1000));
   return true;
}

/* Neural Rendering over @io, Super Resolution's output, recorded into
 * @cmd: @io names its output on success. The core's guides are its
 * render size, @render_dims. */
static bool vulkan_ngx_nr(vulkan_ngx_t *ngx, VkCommandBuffer cmd,
      const struct retro_hw_upscaler_inputs *inputs, unsigned render_dims,
      unsigned nr, bool reset, vulkan_ngx_image_t *io)
{
   const ngx_lib_t *lib = ngx->lib;
   ngx_parameter_t *p;
   ngx_vk_resource_t r_color, r_output, r_depth, r_mv;
   struct retro_hw_upscaler_image img;
   unsigned mv_dims;
   ngx_result_t res;

   if (!(nr & VIDEO_UPSCALER_NR_ON))
   {
      ngx->nr_idle = ngx->nr_feature != NULL;
      return false;
   }
   if (ngx->nr_idle)
   {
      /* (its history is from before it was turned off) */
      ngx->nr_idle = false;
      reset        = true;
   }
   if (ngx->nr_unsupported
         || (ngx->nr_failed_word == nr && ngx->nr_failed_dims == io->dims))
      return false;
   if (!ngx->nr_feature || ngx->nr_dims != io->dims || ngx->nr_format != io->format)
   {
      if (!vulkan_ngx_nr_create(ngx, cmd, io->format, io->dims, nr))
         return false;
   }
   else if (nr != ngx->nr_word)
   {
      /* (made again once the change has held) */
      if (nr != ngx->nr_pending)
      {
         ngx->nr_pending        = nr;
         ngx->nr_pending_frames = 0;
      }
      else if (++ngx->nr_pending_frames >= VULKAN_NGX_NR_SETTLE
            && !vulkan_ngx_nr_create(ngx, cmd, io->format, io->dims, nr))
         return false;
   }
   p = ngx->nr_params;

   vulkan_ngx_barrier(cmd, ngx->nr_image, VK_IMAGE_ASPECT_COLOR_BIT,
         ngx->nr_reset ? VK_IMAGE_LAYOUT_UNDEFINED
                       : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
         VK_IMAGE_LAYOUT_GENERAL,
         VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
   if (vulkan_ngx_input_needs_move(&inputs->motion_vectors))
      vulkan_ngx_move_input(cmd, &inputs->motion_vectors, true);
   if (vulkan_ngx_input_needs_move(&inputs->depth))
      vulkan_ngx_move_input(cmd, &inputs->depth, true);

   img.image  = (uint64_t)(uintptr_t)io->image;
   img.view   = (uint64_t)(uintptr_t)io->view;
   img.format = (unsigned)io->format;
   img.state  = (unsigned)io->layout;
   img.width  = VIDEO_SCALE_W(io->dims);
   img.height = VIDEO_SCALE_H(io->dims);
   vulkan_ngx_vk_resource(&r_color, &img, false);
   img.image  = (uint64_t)(uintptr_t)ngx->nr_image;
   img.view   = (uint64_t)(uintptr_t)ngx->nr_view;
   vulkan_ngx_vk_resource(&r_output, &img, true);
   vulkan_ngx_vk_resource(&r_depth, &inputs->depth, false);
   vulkan_ngx_vk_resource(&r_mv, &inputs->motion_vectors, false);
   mv_dims = (inputs->flags & RETRO_HW_UPSCALER_MV_OUTPUT_RES)
      ? io->dims : render_dims;

   lib->param.set_void_pointer(p, NGX_PARAM_DLSSNR_COLOR, &r_color);
   lib->param.set_void_pointer(p, NGX_PARAM_DLSSNR_OUTPUT, &r_output);
   lib->param.set_void_pointer(p, NGX_PARAM_DLSSNR_DEPTH, &r_depth);
   lib->param.set_void_pointer(p, NGX_PARAM_DLSSNR_MVEC, &r_mv);
   lib->param.set_ui(p, NGX_PARAM_DLSSNR_ENABLED, 1);
   lib->param.set_ui(p, NGX_PARAM_DLSSNR_WIDTH, VIDEO_SCALE_W(io->dims));
   lib->param.set_ui(p, NGX_PARAM_DLSSNR_HEIGHT, VIDEO_SCALE_H(io->dims));
   lib->param.set_ui(p, NGX_PARAM_DLSSNR_DEPTH_INVERTED,
         (inputs->flags & RETRO_HW_UPSCALER_DEPTH_INVERTED) ? 1 : 0);
   lib->param.set_ui(p, NGX_PARAM_DLSSNR_RESET,
         (ngx->nr_reset || reset) ? 1 : 0);
   lib->param.set_f(p, NGX_PARAM_DLSSNR_MVEC_SCALE_X,
         inputs->mv_scale_x == 0.0f ? 1.0f : inputs->mv_scale_x);
   lib->param.set_f(p, NGX_PARAM_DLSSNR_MVEC_SCALE_Y,
         inputs->mv_scale_y == 0.0f ? 1.0f : inputs->mv_scale_y);
   lib->param.set_ui(p, NGX_PARAM_DLSSNR_COLOR_SUBRECT "BaseX", 0);
   lib->param.set_ui(p, NGX_PARAM_DLSSNR_COLOR_SUBRECT "BaseY", 0);
   lib->param.set_ui(p, NGX_PARAM_DLSSNR_COLOR_SUBRECT "Width", VIDEO_SCALE_W(io->dims));
   lib->param.set_ui(p, NGX_PARAM_DLSSNR_COLOR_SUBRECT "Height", VIDEO_SCALE_H(io->dims));
   lib->param.set_ui(p, NGX_PARAM_DLSSNR_OUTPUT_SUBRECT "BaseX", 0);
   lib->param.set_ui(p, NGX_PARAM_DLSSNR_OUTPUT_SUBRECT "BaseY", 0);
   lib->param.set_ui(p, NGX_PARAM_DLSSNR_OUTPUT_SUBRECT "Width", VIDEO_SCALE_W(io->dims));
   lib->param.set_ui(p, NGX_PARAM_DLSSNR_OUTPUT_SUBRECT "Height", VIDEO_SCALE_H(io->dims));
   lib->param.set_ui(p, NGX_PARAM_DLSSNR_DEPTH_SUBRECT "BaseX", 0);
   lib->param.set_ui(p, NGX_PARAM_DLSSNR_DEPTH_SUBRECT "BaseY", 0);
   lib->param.set_ui(p, NGX_PARAM_DLSSNR_DEPTH_SUBRECT "Width", VIDEO_SCALE_W(render_dims));
   lib->param.set_ui(p, NGX_PARAM_DLSSNR_DEPTH_SUBRECT "Height", VIDEO_SCALE_H(render_dims));
   lib->param.set_ui(p, NGX_PARAM_DLSSNR_MVEC_SUBRECT "BaseX", 0);
   lib->param.set_ui(p, NGX_PARAM_DLSSNR_MVEC_SUBRECT "BaseY", 0);
   lib->param.set_ui(p, NGX_PARAM_DLSSNR_MVEC_SUBRECT "Width", VIDEO_SCALE_W(mv_dims));
   lib->param.set_ui(p, NGX_PARAM_DLSSNR_MVEC_SUBRECT "Height", VIDEO_SCALE_H(mv_dims));
   vulkan_ngx_full_barrier(cmd);
   res = lib->vulkan.evaluate_feature((void*)cmd, ngx->nr_feature, p, NULL);
   ngx->nr_reset = false;

   if (vulkan_ngx_input_needs_move(&inputs->motion_vectors))
      vulkan_ngx_move_input(cmd, &inputs->motion_vectors, false);
   if (vulkan_ngx_input_needs_move(&inputs->depth))
      vulkan_ngx_move_input(cmd, &inputs->depth, false);
   vulkan_ngx_barrier(cmd, ngx->nr_image, VK_IMAGE_ASPECT_COLOR_BIT,
         VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
         VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
   if (NGX_FAILED(res))
   {
      RARCH_ERR("[Vulkan] Temporal Upscaler: DLSS Neural Rendering evaluate failed (0x%08x).\n",
            (unsigned)res);
      ngx->nr_failed_word = nr;
      ngx->nr_failed_dims = io->dims;
      return false;
   }
   io->image  = ngx->nr_image;
   io->view   = ngx->nr_view;
   io->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   io->format = ngx->nr_format;
   return true;
}

bool vulkan_ngx_upscale(vulkan_ngx_t *ngx, VkCommandBuffer cmd,
      const vulkan_ngx_image_t *color,
      const struct retro_hw_upscaler_inputs *inputs,
      unsigned quality, unsigned nr, unsigned out_dims,
      vulkan_ngx_image_t *out)
{
   const ngx_lib_t *lib;
   ngx_parameter_t *p;
   ngx_vk_resource_t r_color, r_output, r_depth, r_mv, r_exposure, r_mask;
   struct retro_hw_upscaler_image color_img;
   unsigned render_dims, min_dims, max_dims;
   unsigned flags;
   ngx_result_t res;
   bool reset;

   if (!ngx || !inputs || !color->image || !inputs->motion_vectors.image
         || !inputs->depth.image
         || VIDEO_SCALE_W(out_dims) < 32 || VIDEO_SCALE_H(out_dims) < 32)
      return false;
   lib = ngx->lib;
   p   = ngx->params;
   if (!vulkan_ngx_optimal(ngx, quality, out_dims, &render_dims, &min_dims,
            &max_dims))
      return false;
   if ((quality = vulkan_ngx_mode_for(ngx, quality, out_dims, color->dims,
               &max_dims)) == ~0u)
   {
      unsigned unfit = color->dims ^ (out_dims * 31u);
      if (ngx->unfit_dims != unfit)
      {
         ngx->unfit_dims = unfit;
         RARCH_WARN("[Vulkan] Temporal Upscaler: a %ux%u frame is outside DLSS's %ux%u to %ux%u for %ux%u; shown without upscaling.\n",
               VIDEO_SCALE_W(color->dims), VIDEO_SCALE_H(color->dims),
               VIDEO_SCALE_W(min_dims), VIDEO_SCALE_H(min_dims),
               VIDEO_SCALE_W(max_dims), VIDEO_SCALE_H(max_dims),
               VIDEO_SCALE_W(out_dims), VIDEO_SCALE_H(out_dims));
      }
      return false;
   }
   if (ngx->failed_out_dims == out_dims && ngx->failed_quality == quality)
      return false;

   flags = 0;
   if (!(inputs->flags & RETRO_HW_UPSCALER_MV_OUTPUT_RES))
      flags |= NGX_DLSS_FLAG_MV_LOW_RES;
   if (inputs->flags & RETRO_HW_UPSCALER_MV_JITTERED)
      flags |= NGX_DLSS_FLAG_MV_JITTERED;
   if (inputs->flags & RETRO_HW_UPSCALER_DEPTH_INVERTED)
      flags |= NGX_DLSS_FLAG_DEPTH_INVERTED;
   if (inputs->flags & RETRO_HW_UPSCALER_HDR)
      flags |= NGX_DLSS_FLAG_IS_HDR;
   if (!inputs->exposure.image)
      flags |= NGX_DLSS_FLAG_AUTO_EXPOSURE;

   if (   !ngx->feature
       || ngx->feature_out_dims != out_dims
       || ngx->feature_max_dims != max_dims
       || ngx->feature_flags != flags
       || ngx->feature_quality != NGX_MODE_FEATURE(quality)
       || ngx->out_format != color->format)
   {
      if (!vulkan_ngx_create_feature(ngx, cmd, color->format, quality,
               flags, out_dims, max_dims))
         return false;
   }

   /* The output: the last frame's read is over before this write; its
    * first frame has nothing to wait for */
   vulkan_ngx_barrier(cmd, ngx->out_image, VK_IMAGE_ASPECT_COLOR_BIT,
         ngx->reset_pending ? VK_IMAGE_LAYOUT_UNDEFINED
                            : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
         VK_IMAGE_LAYOUT_GENERAL,
         VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
   if (vulkan_ngx_input_needs_move(&inputs->motion_vectors))
      vulkan_ngx_move_input(cmd, &inputs->motion_vectors, true);
   if (vulkan_ngx_input_needs_move(&inputs->depth))
      vulkan_ngx_move_input(cmd, &inputs->depth, true);

   color_img.image  = (uint64_t)(uintptr_t)color->image;
   color_img.view   = (uint64_t)(uintptr_t)color->view;
   color_img.format = (unsigned)color->format;
   color_img.state  = (unsigned)color->layout;
   color_img.width  = VIDEO_SCALE_W(color->dims);
   color_img.height = VIDEO_SCALE_H(color->dims);
   vulkan_ngx_vk_resource(&r_color, &color_img, false);
   vulkan_ngx_vk_resource(&r_depth, &inputs->depth, false);
   vulkan_ngx_vk_resource(&r_mv, &inputs->motion_vectors, false);
   color_img.image  = (uint64_t)(uintptr_t)ngx->out_image;
   color_img.view   = (uint64_t)(uintptr_t)ngx->out_view;
   color_img.width  = VIDEO_SCALE_W(out_dims);
   color_img.height = VIDEO_SCALE_H(out_dims);
   vulkan_ngx_vk_resource(&r_output, &color_img, true);
   if (inputs->exposure.image)
      vulkan_ngx_vk_resource(&r_exposure, &inputs->exposure, false);
   if (inputs->transparency_mask.image)
      vulkan_ngx_vk_resource(&r_mask, &inputs->transparency_mask, false);

   lib->param.set_void_pointer(p, NGX_PARAM_COLOR, &r_color);
   lib->param.set_void_pointer(p, NGX_PARAM_OUTPUT, &r_output);
   lib->param.set_void_pointer(p, NGX_PARAM_DEPTH, &r_depth);
   lib->param.set_void_pointer(p, NGX_PARAM_MOTION_VECTORS, &r_mv);
   lib->param.set_void_pointer(p, NGX_PARAM_EXPOSURE_TEXTURE,
         inputs->exposure.image ? (void*)&r_exposure : NULL);
   lib->param.set_void_pointer(p, NGX_PARAM_BIAS_CURRENT_COLOR_MASK,
         inputs->transparency_mask.image ? (void*)&r_mask : NULL);
   lib->param.set_ui(p, NGX_PARAM_BIAS_CURRENT_COLOR_BASE_X, 0);
   lib->param.set_ui(p, NGX_PARAM_BIAS_CURRENT_COLOR_BASE_Y, 0);
   lib->param.set_f(p, NGX_PARAM_JITTER_OFFSET_X, inputs->jitter_x);
   lib->param.set_f(p, NGX_PARAM_JITTER_OFFSET_Y, inputs->jitter_y);
   lib->param.set_f(p, NGX_PARAM_SHARPNESS, (float)NGX_MODE_SHARPNESS(quality) / 100.0f);
   lib->param.set_i(p, NGX_PARAM_RESET,
         (ngx->reset_pending || (inputs->flags & RETRO_HW_UPSCALER_RESET)) ? 1 : 0);
   lib->param.set_f(p, NGX_PARAM_MV_SCALE_X,
         inputs->mv_scale_x == 0.0f ? 1.0f : inputs->mv_scale_x);
   lib->param.set_f(p, NGX_PARAM_MV_SCALE_Y,
         inputs->mv_scale_y == 0.0f ? 1.0f : inputs->mv_scale_y);
   lib->param.set_f(p, NGX_PARAM_DLSS_PRE_EXPOSURE,
         inputs->pre_exposure == 0.0f ? 1.0f : inputs->pre_exposure);
   lib->param.set_f(p, NGX_PARAM_DLSS_EXPOSURE_SCALE,
         inputs->exposure_scale == 0.0f ? 1.0f : inputs->exposure_scale);
   lib->param.set_f(p, NGX_PARAM_FRAME_TIME_DELTA_MS, video_upscaler_frame_time_ms(inputs));
   lib->param.set_ui(p, NGX_PARAM_DLSS_RENDER_SUBRECT_WIDTH, VIDEO_SCALE_W(color->dims));
   lib->param.set_ui(p, NGX_PARAM_DLSS_RENDER_SUBRECT_HEIGHT, VIDEO_SCALE_H(color->dims));
   lib->param.set_ui(p, NGX_PARAM_DLSS_INPUT_COLOR_SUBRECT_X, 0);
   lib->param.set_ui(p, NGX_PARAM_DLSS_INPUT_COLOR_SUBRECT_Y, 0);
   lib->param.set_ui(p, NGX_PARAM_DLSS_INPUT_DEPTH_SUBRECT_X, 0);
   lib->param.set_ui(p, NGX_PARAM_DLSS_INPUT_DEPTH_SUBRECT_Y, 0);
   lib->param.set_ui(p, NGX_PARAM_DLSS_INPUT_MV_SUBRECT_X, 0);
   lib->param.set_ui(p, NGX_PARAM_DLSS_INPUT_MV_SUBRECT_Y, 0);
   lib->param.set_ui(p, NGX_PARAM_DLSS_OUTPUT_SUBRECT_X, 0);
   lib->param.set_ui(p, NGX_PARAM_DLSS_OUTPUT_SUBRECT_Y, 0);
   lib->param.set_i(p, NGX_PARAM_DLSS_INDICATOR_INVERT_X, 0);
   lib->param.set_i(p, NGX_PARAM_DLSS_INDICATOR_INVERT_Y, 0);
   reset = ngx->reset_pending || (inputs->flags & RETRO_HW_UPSCALER_RESET);
   vulkan_ngx_full_barrier(cmd);
   res = lib->vulkan.evaluate_feature((void*)cmd, ngx->feature, p, NULL);
   ngx->reset_pending = false;

   if (vulkan_ngx_input_needs_move(&inputs->motion_vectors))
      vulkan_ngx_move_input(cmd, &inputs->motion_vectors, false);
   if (vulkan_ngx_input_needs_move(&inputs->depth))
      vulkan_ngx_move_input(cmd, &inputs->depth, false);
   vulkan_ngx_barrier(cmd, ngx->out_image, VK_IMAGE_ASPECT_COLOR_BIT,
         VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
         VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
   if (NGX_FAILED(res))
   {
      RARCH_ERR("[Vulkan] Temporal Upscaler: evaluate failed (0x%08x).\n",
            (unsigned)res);
      ngx->failed_out_dims = out_dims;
      ngx->failed_quality  = quality;
      return false;
   }
   out->image  = ngx->out_image;
   out->view   = ngx->out_view;
   out->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   out->format = ngx->out_format;
   out->dims   = out_dims;
   /* (Super Resolution's frame stands where Neural Rendering does not
    * run) */
   vulkan_ngx_nr(ngx, cmd, inputs, color->dims, nr, reset, out);
   return true;
}

void vulkan_ngx_free(vulkan_ngx_t *ngx)
{
   unsigned i;
   int in_use = 0;
   if (!ngx)
      return;
   vulkan_ngx_retire(ngx);
   vulkan_ngx_nr_retire(ngx);
   for (i = 0; i < VULKAN_NGX_RETIRED_MAX; i++)
      ngx->retired[i].frames = ngx->retired[i].frames ? 1 : 0;
   vulkan_ngx_frame_done(ngx);
   RARCH_LOG("[Vulkan] Temporal Upscaler: DLSS feature released.\n");
   if (ngx->nr_params)
      ngx->lib->vulkan.destroy_parameters(ngx->nr_params);
   if (ngx->params)
      ngx->lib->vulkan.destroy_parameters(ngx->params);
   if (ngx->caps)
      ngx->lib->vulkan.destroy_parameters(ngx->caps);
   RARCH_LOG("[Vulkan] Temporal Upscaler: NGX parameters destroyed.\n");
   ngx->lib->vulkan.shutdown1((void*)ngx->device, &in_use);
   RARCH_LOG("[Vulkan] Temporal Upscaler: NGX shut down.\n");
   free(ngx->wdir);
   free(ngx);
}
