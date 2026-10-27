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
#include <gfx/intel/xess_loader.h>
#include <vulkan/vulkan_symbol_wrapper.h>

#include "../../verbosity.h"
#include "xess_common.h"
#include "vulkan_xess.h"

struct vulkan_xess
{
   const xess_lib_t *lib;
   xess_context_t context;
   VkInstance instance;
   VkPhysicalDevice gpu;
   VkDevice device;
   VkPhysicalDeviceMemoryProperties memory_properties;
   vulkan_upscaler_ring_t ring;
   vulkan_upscaler_output_t out;
   xess_common_state_t state;
};

/* A storage image cannot be sRGB: the output takes the UNORM twin, the
 * encoding the same */
static VkFormat vulkan_xess_output_format(VkFormat format)
{
   switch (format)
   {
      case VK_FORMAT_R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_UNORM;
      case VK_FORMAT_B8G8R8A8_SRGB: return VK_FORMAT_B8G8R8A8_UNORM;
      default:                      break;
   }
   return format;
}

static const xess_lib_t *vulkan_xess_lib(const char *dir)
{
   const xess_lib_t *lib = xess_lib_get(dir ? dir : "");
   if (!lib || !lib->vk.create_context)
      return NULL;
   return lib;
}

static bool vulkan_xess_in_list(const char **list, unsigned count,
      const char *name)
{
   unsigned i;
   for (i = 0; i < count; i++)
      if (!strcmp(list[i], name))
         return true;
   return false;
}

unsigned vulkan_xess_instance_extensions(const char *dir,
      const char **list, unsigned count, unsigned cap)
{
   const xess_lib_t *lib = vulkan_xess_lib(dir);
   uint32_t n = 0, min_api = 0, i;
   const char *const *exts = NULL;
   if (!lib)
      return count;
   if (XESS_FAILED(lib->vk.get_required_instance_extensions(&n, &exts, &min_api)) || !exts)
      return count;
   for (i = 0; i < n && count < cap; i++)
      if (!vulkan_xess_in_list(list, count, exts[i]))
         list[count++] = exts[i];
   return count;
}

unsigned vulkan_xess_device_extensions(const char *dir, VkPhysicalDevice gpu,
      PFN_vkGetInstanceProcAddr gipa, VkInstance instance,
      const char **list, unsigned count, unsigned cap)
{
   const xess_lib_t *lib = vulkan_xess_lib(dir);
   PFN_vkEnumerateDeviceExtensionProperties enumerate;
   VkExtensionProperties *props = NULL;
   uint32_t nprops = 0, n = 0, i, j;
   const char *const *exts = NULL;
   if (!lib)
      return count;
   if (XESS_FAILED(lib->vk.get_required_device_extensions((void*)instance,
               (void*)gpu, &n, &exts)) || !exts || !n)
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
      if (vulkan_xess_in_list(list, count, exts[i]))
         continue;
      for (j = 0; j < nprops; j++)
         if (!strcmp(props[j].extensionName, exts[i]))
         {
            list[count++] = exts[i];
            break;
         }
   }
   free(props);
   return count;
}

static bool vulkan_xess_chain_has(const void *pnext, VkStructureType type)
{
   const VkBaseInStructure *chain;
   for (chain = (const VkBaseInStructure*)pnext; chain; chain = chain->pNext)
      if (chain->sType == type)
         return true;
   return false;
}

const void *vulkan_xess_device_features(const char *dir, VkPhysicalDevice gpu,
      PFN_vkGetInstanceProcAddr gipa, VkInstance instance, const void *pnext)
{
   const xess_lib_t *lib = vulkan_xess_lib(dir);
   void *features = NULL;
   VkBaseOutStructure *node, *next;
   if (!lib)
      return pnext;
   if (XESS_FAILED(lib->vk.get_required_device_features((void*)instance,
               (void*)gpu, &features)) || !features)
      return pnext;
   /* The library's chain, its structures the device's chain lacks
    * linked ahead of it one by one (the library owns them; a type the
    * chain names already is left to the chain's) */
   for (node = (VkBaseOutStructure*)features; node; node = next)
   {
      next = node->pNext;
      if (vulkan_xess_chain_has(pnext, node->sType))
         continue;
      node->pNext = (VkBaseOutStructure*)(uintptr_t)pnext;
      pnext       = node;
   }
   return pnext;
}

vulkan_xess_t *vulkan_xess_init(VkInstance instance, VkPhysicalDevice gpu,
      VkDevice device,
      const VkPhysicalDeviceMemoryProperties *memory_properties,
      const char *dir)
{
   vulkan_xess_t *xess;
   const xess_lib_t *lib = xess_lib_get(dir ? dir : "");
   if (!lib || !lib->vk.create_context)
   {
      RARCH_LOG("[Vulkan] Temporal Upscaler: %s, upscaler off.\n", xess_lib_status());
      return NULL;
   }
   if (!(xess = (vulkan_xess_t*)calloc(1, sizeof(*xess))))
      return NULL;
   xess->lib               = lib;
   xess->instance          = instance;
   xess->gpu               = gpu;
   xess->device            = device;
   xess->memory_properties = *memory_properties;
   xess->ring.device       = device;
   /* (a context of its own answers the size queries before a frame
    * brings the first) */
   if (XESS_FAILED(lib->vk.create_context((void*)instance, (void*)gpu,
               (void*)device, &xess->state.query)))
   {
      RARCH_LOG("[Vulkan] Temporal Upscaler: XeSS does not run on this device, upscaler off.\n");
      free(xess);
      return NULL;
   }
   RARCH_LOG("[Vulkan] Temporal Upscaler: %s; XeSS ready.\n", xess_lib_status());
   return xess;
}

bool vulkan_xess_optimal(vulkan_xess_t *xess, unsigned mode,
      unsigned out_dims, unsigned *render_dims,
      unsigned *min_dims, unsigned *max_dims)
{
   if (!xess)
      return false;
   return xess_common_optimal(xess->lib, &xess->state, mode, out_dims,
         render_dims, min_dims, max_dims);
}

static void vulkan_xess_release(void *data, void *feature)
{
   vulkan_xess_t *xess = (vulkan_xess_t*)data;
   xess->lib->common.destroy_context((xess_context_t)feature);
}

static void vulkan_xess_retire(vulkan_xess_t *xess)
{
   xess->ring.release      = vulkan_xess_release;
   xess->ring.release_data = xess;
   vulkan_upscaler_ring_retire(&xess->ring, xess->context, &xess->out);
   xess->context = NULL;
}

void vulkan_xess_frame_done(vulkan_xess_t *xess)
{
   if (!xess)
      return;
   vulkan_upscaler_ring_frame_done(&xess->ring);
}

/* A new context for the output size, mode and flags: its pipelines
 * start building; the frames until they are built are shown as they
 * are */
static bool vulkan_xess_create(vulkan_xess_t *xess, VkFormat format,
      unsigned mode, unsigned flags, unsigned out_dims)
{
   xess_result_t res;
   vulkan_xess_retire(xess);
   if (!vulkan_upscaler_output_create(xess->device, &xess->memory_properties,
            vulkan_xess_output_format(format), out_dims, &xess->out))
   {
      RARCH_ERR("[Vulkan] Temporal Upscaler: no output image of %ux%u.\n",
            VIDEO_SCALE_W(out_dims), VIDEO_SCALE_H(out_dims));
      return false;
   }
   res = xess->lib->vk.create_context((void*)xess->instance, (void*)xess->gpu,
         (void*)xess->device, &xess->context);
   if (XESS_FAILED(res))
   {
      RARCH_ERR("[Vulkan] Temporal Upscaler: XeSS context failed (%d).\n", (int)res);
      xess->context = NULL;
      vulkan_xess_retire(xess);
      xess_common_failed(&xess->state, out_dims, mode, flags);
      return false;
   }
   xess_common_created(xess->lib, &xess->state, xess->context, mode, flags, out_dims);
   res = xess->lib->vk.build_pipelines(xess->context, 0,
         xess->lib->common.get_pipeline_build_status ? false : true, flags);
   if (XESS_FAILED(res))
   {
      RARCH_ERR("[Vulkan] Temporal Upscaler: XeSS pipelines failed (%d).\n", (int)res);
      vulkan_xess_retire(xess);
      xess_common_failed(&xess->state, out_dims, mode, flags);
      return false;
   }
   return true;
}

static bool vulkan_xess_ready(vulkan_xess_t *xess, unsigned out_dims)
{
   xess_vk_init_params_t init;
   xess_result_t res;
   if (xess->state.inited)
      return true;
   if (xess->lib->common.get_pipeline_build_status)
   {
      res = xess->lib->common.get_pipeline_build_status(xess->context);
      if (res == XESS_RESULT_ERROR_OPERATION_IN_PROGRESS)
         return false;
      if (XESS_FAILED(res))
      {
         RARCH_ERR("[Vulkan] Temporal Upscaler: XeSS pipeline build failed (%d).\n", (int)res);
         xess_common_failed(&xess->state, out_dims, xess->state.mode, xess->state.flags);
         vulkan_xess_retire(xess);
         return false;
      }
   }
   memset(&init, 0, sizeof(init));
   init.output_resolution.x = VIDEO_SCALE_W(out_dims);
   init.output_resolution.y = VIDEO_SCALE_H(out_dims);
   init.quality             = (int)xess->state.mode;
   init.init_flags          = xess->state.flags;
   init.creation_node_mask  = 1;
   init.visible_node_mask   = 1;
   res = xess->lib->vk.init(xess->context, &init);
   if (XESS_FAILED(res))
   {
      RARCH_ERR("[Vulkan] Temporal Upscaler: XeSS init %ux%u failed (%d).\n",
            VIDEO_SCALE_W(out_dims), VIDEO_SCALE_H(out_dims), (int)res);
      xess_common_failed(&xess->state, out_dims, xess->state.mode, xess->state.flags);
      vulkan_xess_retire(xess);
      return false;
   }
   xess_common_inited(xess->lib, &xess->state, xess->context);
   RARCH_LOG("[Vulkan] Temporal Upscaler: XeSS -> %ux%u (%s), flags 0x%x.\n",
         VIDEO_SCALE_W(out_dims), VIDEO_SCALE_H(out_dims),
         xess_common_mode_name(xess->state.mode), xess->state.flags);
   return true;
}

static void vulkan_xess_view(xess_vk_image_view_info_t *v,
      const struct retro_hw_upscaler_image *img)
{
   memset(v, 0, sizeof(*v));
   v->image_view                    = img->view;
   v->image                         = img->image;
   /* A depth-stencil image is sampled through its depth alone */
   v->subresource_range.aspect_mask =
      vulkan_upscaler_aspect((VkFormat)img->format) & ~VK_IMAGE_ASPECT_STENCIL_BIT;
   v->subresource_range.level_count = 1;
   v->subresource_range.layer_count = 1;
   v->format                        = (int)img->format;
   v->width                         = img->width;
   v->height                        = img->height;
}

/* XeSS reads its inputs in SHADER_READ_ONLY_OPTIMAL alone */
static bool vulkan_xess_needs_move(const struct retro_hw_upscaler_image *img)
{
   return img->image
      && img->state != (unsigned)VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

static void vulkan_xess_move(VkCommandBuffer cmd,
      const struct retro_hw_upscaler_image *img, bool to_read)
{
   if (vulkan_xess_needs_move(img))
      vulkan_upscaler_move_input(cmd, img, to_read);
}

bool vulkan_xess_upscale(vulkan_xess_t *xess, VkCommandBuffer cmd,
      const vulkan_upscaler_image_t *color,
      const struct retro_hw_upscaler_inputs *inputs,
      unsigned mode, unsigned out_dims, vulkan_upscaler_image_t *out)
{
   xess_vk_execute_params_t e;
   struct retro_hw_upscaler_image img;
   unsigned render_dims, min_dims, max_dims;
   unsigned flags;
   xess_result_t res;

   if (!xess || !inputs || !color->image || !inputs->motion_vectors.image
         || !inputs->depth.image
         || VIDEO_SCALE_W(out_dims) < 32 || VIDEO_SCALE_H(out_dims) < 32)
      return false;
   flags = xess_common_init_flags(inputs);
   if (xess_common_has_failed(&xess->state, out_dims, mode, flags))
      return false;
   if (!vulkan_xess_optimal(xess, mode, out_dims, &render_dims, &min_dims,
            &max_dims))
      return false;
   if (     VIDEO_SCALE_W(color->dims) < VIDEO_SCALE_W(min_dims)
         || VIDEO_SCALE_W(color->dims) > VIDEO_SCALE_W(max_dims)
         || VIDEO_SCALE_H(color->dims) < VIDEO_SCALE_H(min_dims)
         || VIDEO_SCALE_H(color->dims) > VIDEO_SCALE_H(max_dims))
      return false;

   if (   !xess->context
       || xess->state.out_dims != out_dims
       || xess->state.mode != mode
       || xess->state.flags != flags
       || xess->out.format != vulkan_xess_output_format(color->format))
   {
      if (!vulkan_xess_create(xess, color->format, mode, flags, out_dims))
         return false;
   }
   if (!vulkan_xess_ready(xess, out_dims))
      return false;
   xess_common_scales(xess->lib, &xess->state, xess->context, inputs);

   vulkan_upscaler_barrier(cmd, xess->out.image, VK_IMAGE_ASPECT_COLOR_BIT,
         xess->state.reset_pending ? VK_IMAGE_LAYOUT_UNDEFINED
                                   : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
         VK_IMAGE_LAYOUT_GENERAL,
         VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
   img.image  = (uint64_t)(uintptr_t)color->image;
   img.view   = (uint64_t)(uintptr_t)color->view;
   img.format = (unsigned)color->format;
   img.state  = (unsigned)color->layout;
   img.width  = VIDEO_SCALE_W(color->dims);
   img.height = VIDEO_SCALE_H(color->dims);
   vulkan_xess_move(cmd, &img, true);
   vulkan_xess_move(cmd, &inputs->motion_vectors, true);
   vulkan_xess_move(cmd, &inputs->depth, true);
   vulkan_xess_move(cmd, &inputs->exposure, true);
   vulkan_xess_move(cmd, &inputs->transparency_mask, true);

   memset(&e, 0, sizeof(e));
   vulkan_xess_view(&e.color, &img);
   vulkan_xess_view(&e.velocity, &inputs->motion_vectors);
   vulkan_xess_view(&e.depth, &inputs->depth);
   if (inputs->exposure.image)
      vulkan_xess_view(&e.exposure_scale, &inputs->exposure);
   if (inputs->transparency_mask.image)
      vulkan_xess_view(&e.responsive_pixel_mask, &inputs->transparency_mask);
   img.image  = (uint64_t)(uintptr_t)xess->out.image;
   img.view   = (uint64_t)(uintptr_t)xess->out.view;
   img.format = (unsigned)xess->out.format;
   img.width  = VIDEO_SCALE_W(out_dims);
   img.height = VIDEO_SCALE_H(out_dims);
   vulkan_xess_view(&e.output, &img);
   e.jitter_offset_x      = inputs->jitter_x;
   e.jitter_offset_y      = inputs->jitter_y;
   e.exposure_scale_value = inputs->exposure_scale == 0.0f ? 1.0f : inputs->exposure_scale;
   e.reset_history        = (xess->state.reset_pending || (inputs->flags & RETRO_HW_UPSCALER_RESET)) ? 1 : 0;
   e.input_width          = VIDEO_SCALE_W(color->dims);
   e.input_height         = VIDEO_SCALE_H(color->dims);
   vulkan_upscaler_full_barrier(cmd);
   res = xess->lib->vk.execute(xess->context, (void*)cmd, &e);
   xess->state.reset_pending = false;

   img.image  = (uint64_t)(uintptr_t)color->image;
   img.format = (unsigned)color->format;
   img.state  = (unsigned)color->layout;
   vulkan_xess_move(cmd, &img, false);
   vulkan_xess_move(cmd, &inputs->motion_vectors, false);
   vulkan_xess_move(cmd, &inputs->depth, false);
   vulkan_xess_move(cmd, &inputs->exposure, false);
   vulkan_xess_move(cmd, &inputs->transparency_mask, false);
   vulkan_upscaler_barrier(cmd, xess->out.image, VK_IMAGE_ASPECT_COLOR_BIT,
         VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
         VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
   if (XESS_FAILED(res))
   {
      RARCH_ERR("[Vulkan] Temporal Upscaler: XeSS execute failed (%d).\n", (int)res);
      xess_common_failed(&xess->state, out_dims, mode, flags);
      return false;
   }
   out->image  = xess->out.image;
   out->view   = xess->out.view;
   out->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   out->format = xess->out.format;
   out->dims   = out_dims;
   return true;
}

void vulkan_xess_free(vulkan_xess_t *xess)
{
   if (!xess)
      return;
   vulkan_xess_retire(xess);
   vulkan_upscaler_ring_flush(&xess->ring);
   if (xess->state.query)
      xess->lib->common.destroy_context(xess->state.query);
   RARCH_LOG("[Vulkan] Temporal Upscaler: XeSS context released.\n");
   free(xess);
}
