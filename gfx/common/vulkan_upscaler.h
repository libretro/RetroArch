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

#ifndef __VULKAN_UPSCALER_H
#define __VULKAN_UPSCALER_H

#include <boolean.h>
#include <retro_common_api.h>
#include <libretro.h>
#include <vulkan/vulkan.h>

#include "../video_defines.h"
#include "../video_upscaler.h"

RETRO_BEGIN_DECLS

/* The temporal upscaler on a Vulkan device: the backend of the kind
 * set (DLSS on NGX: vulkan_ngx.h; FSR on the FidelityFX API:
 * vulkan_ffx.h; XeSS: vulkan_xess.h) behind one face. Each keeps a
 * session per device, a feature per output size and mode, and an
 * output image the filter chain reads in place of the core's. Nothing
 * here blocks: images and features a frame in flight may still use are
 * retired over the next frames. */
typedef struct vulkan_upscaler vulkan_upscaler_t;

/* The frame the upscaler reads: the core's color image as set_image
 * handed it over, and what it produces: the output image, in
 * VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL once the upscale is
 * recorded. */
typedef struct vulkan_upscaler_image
{
   VkImage image;
   VkImageView view;
   VkImageLayout layout;
   VkFormat format;
   unsigned dims;             /* VIDEO_SCALE_PACK */
} vulkan_upscaler_image_t;

/* The extensions the kind's library needs on the instance and device,
 * appended to @list (of @cap) past @count, those the GPU has and the
 * list lacks; the new count. Nothing appended where the library is not
 * loaded. @dir is the kind's directory (video_upscaler_dir). */
unsigned vulkan_upscaler_instance_extensions(enum video_temporal_upscaler kind,
      const char *dir, const char **list, unsigned count, unsigned cap);
unsigned vulkan_upscaler_device_extensions(enum video_temporal_upscaler kind,
      const char *dir, VkPhysicalDevice gpu, PFN_vkGetInstanceProcAddr gipa,
      VkInstance instance, const char **list, unsigned count, unsigned cap);
#define VULKAN_UPSCALER_MAX_EXTS 8

/* The device features the kind's library needs, chained ahead of
 * @pnext (a VkDeviceCreateInfo's): the new head. Structures already in
 * the chain are left to it. @pnext when the library needs none. */
const void *vulkan_upscaler_device_features(enum video_temporal_upscaler kind,
      const char *dir, VkPhysicalDevice gpu, PFN_vkGetInstanceProcAddr gipa,
      VkInstance instance, const void *pnext);

/* The session of @kind on @device, its library looked for in @dir.
 * NULL, with the reason logged, when the library, its runtime or the
 * device cannot. */
vulkan_upscaler_t *vulkan_upscaler_init(enum video_temporal_upscaler kind,
      VkInstance instance, VkPhysicalDevice gpu, VkDevice device,
      PFN_vkGetInstanceProcAddr gipa, PFN_vkGetDeviceProcAddr gdpa,
      const VkPhysicalDeviceMemoryProperties *memory_properties,
      const char *dir);

/* The mode's render sizes for an output of @out_dims: what it wants,
 * and the range it accepts, each VIDEO_SCALE_PACK. @mode is
 * video_upscaler_mode()'s word. */
bool vulkan_upscaler_optimal(vulkan_upscaler_t *up, unsigned mode,
      unsigned out_dims, unsigned *render_dims,
      unsigned *min_dims, unsigned *max_dims);

/* Records into @cmd the upscale of @color with @inputs to @out_dims,
 * and names the output in *@out. False, nothing recorded, when the
 * frame cannot be taken: the caller shows the color image as it is.
 * Call once per frame, before anything that reads *@out. */
bool vulkan_upscaler_upscale(vulkan_upscaler_t *up, VkCommandBuffer cmd,
      const vulkan_upscaler_image_t *color,
      const struct retro_hw_upscaler_inputs *inputs,
      unsigned mode, unsigned out_dims, vulkan_upscaler_image_t *out);

/* The frame was submitted: retired images and features count down. */
void vulkan_upscaler_frame_done(vulkan_upscaler_t *up);

/* The device is idle (context teardown): everything released. */
void vulkan_upscaler_free(vulkan_upscaler_t *up);

/* What the backends share: an output image of their own, the layout
 * moves around a dispatch, and the ring that retires an image and
 * feature after the frames that may still read them. */

typedef struct vulkan_upscaler_output
{
   VkImage image;
   VkImageView view;
   VkDeviceMemory memory;
   VkFormat format;
   unsigned dims;             /* VIDEO_SCALE_PACK */
} vulkan_upscaler_output_t;

/* A storage + sampled image of @format and @dims on device memory into
 * *@out (zeroed on failure) */
bool vulkan_upscaler_output_create(VkDevice device,
      const VkPhysicalDeviceMemoryProperties *memory_properties,
      VkFormat format, unsigned dims, vulkan_upscaler_output_t *out);
void vulkan_upscaler_output_destroy(VkDevice device, vulkan_upscaler_output_t *out);

VkImageAspectFlags vulkan_upscaler_aspect(VkFormat format);

void vulkan_upscaler_barrier(VkCommandBuffer cmd, VkImage image,
      VkImageAspectFlags aspect, VkImageLayout old_layout,
      VkImageLayout new_layout, VkAccessFlags src_access,
      VkAccessFlags dst_access, VkPipelineStageFlags src_stages,
      VkPipelineStageFlags dst_stages);

/* An input in a layout a compute shader cannot sample from is moved
 * to SHADER_READ_ONLY_OPTIMAL for the dispatch (@to_read), and back
 * after */
bool vulkan_upscaler_input_needs_move(const struct retro_hw_upscaler_image *img);
void vulkan_upscaler_move_input(VkCommandBuffer cmd,
      const struct retro_hw_upscaler_image *img, bool to_read);

/* Everything written so far, by any stage, before whatever a library
 * reads or writes with: which stages it uses is its own business */
void vulkan_upscaler_full_barrier(VkCommandBuffer cmd);

/* Frames an image or feature a submitted frame may read stays alive
 * for, past the swapchain's depth */
#define VULKAN_UPSCALER_RETIRE_FRAMES 20
#define VULKAN_UPSCALER_RETIRED_MAX   8

typedef void (*vulkan_upscaler_release_t)(void *data, void *feature);

typedef struct vulkan_upscaler_retired
{
   void *feature;
   vulkan_upscaler_output_t output;
   unsigned frames;
} vulkan_upscaler_retired_t;

typedef struct vulkan_upscaler_ring
{
   vulkan_upscaler_retired_t slot[VULKAN_UPSCALER_RETIRED_MAX];
   VkDevice device;
   vulkan_upscaler_release_t release;  /* frees a feature; NULL for none */
   void *release_data;
} vulkan_upscaler_ring_t;

/* @feature and *@output (zeroed) to the ring, destroyed once every
 * frame that could use them has gone by; a full ring frees its oldest
 * now, which has had every frame of a swapchain's depth to finish */
void vulkan_upscaler_ring_retire(vulkan_upscaler_ring_t *ring,
      void *feature, vulkan_upscaler_output_t *output);
void vulkan_upscaler_ring_frame_done(vulkan_upscaler_ring_t *ring);
/* Everything in the ring freed now (the device is idle) */
void vulkan_upscaler_ring_flush(vulkan_upscaler_ring_t *ring);

RETRO_END_DECLS

#endif
