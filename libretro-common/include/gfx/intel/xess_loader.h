/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (xess_loader.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_GFX_INTEL_XESS_LOADER_H
#define __LIBRETRO_SDK_GFX_INTEL_XESS_LOADER_H

/* Intel XeSS Super Resolution (XeSS-SR), loaded at run time from the
 * library the user supplies, libxess.dll of the XeSS SDK (Direct3D 12
 * and Vulkan in the one library): nothing of Intel's is built in or
 * shipped. The entry points of xess.h, xess_d3d12.h and xess_vk.h, with
 * their structures declared here so that no SDK header is needed; the
 * SDK packs them to 8, which is their natural layout on x86-64. */

#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct xess_context *xess_context_t;

typedef int xess_result_t;
#define XESS_RESULT_WARNING_OLD_DRIVER         2
#define XESS_RESULT_SUCCESS                    0
#define XESS_RESULT_ERROR_UNSUPPORTED_DEVICE  -1
#define XESS_RESULT_ERROR_UNSUPPORTED_DRIVER  -2
#define XESS_RESULT_ERROR_UNINITIALIZED       -3
#define XESS_RESULT_ERROR_INVALID_ARGUMENT    -4
#define XESS_RESULT_ERROR_DEVICE_OUT_OF_MEMORY -5
#define XESS_RESULT_ERROR_DEVICE              -6
#define XESS_RESULT_ERROR_NOT_IMPLEMENTED     -7
#define XESS_RESULT_ERROR_INVALID_CONTEXT     -8
#define XESS_RESULT_ERROR_OPERATION_IN_PROGRESS -9
#define XESS_RESULT_ERROR_UNSUPPORTED         -10
#define XESS_RESULT_ERROR_CANT_LOAD_LIBRARY   -11
#define XESS_RESULT_ERROR_WRONG_CALL_ORDER    -12
#define XESS_FAILED(r) ((r) < 0)

typedef struct xess_version
{
   uint16_t major;
   uint16_t minor;
   uint16_t patch;
   uint16_t reserved;
} xess_version_t;

typedef struct xess_2d
{
   uint32_t x;
   uint32_t y;
} xess_2d_t;

/* xess_quality_settings_t */
#define XESS_QUALITY_ULTRA_PERFORMANCE  100
#define XESS_QUALITY_PERFORMANCE        101
#define XESS_QUALITY_BALANCED           102
#define XESS_QUALITY_QUALITY            103
#define XESS_QUALITY_ULTRA_QUALITY      104
#define XESS_QUALITY_ULTRA_QUALITY_PLUS 105
#define XESS_QUALITY_AA                 106

/* xess_init_flags_t */
#define XESS_INIT_FLAG_HIGH_RES_MV              (1 << 0)
#define XESS_INIT_FLAG_INVERTED_DEPTH           (1 << 1)
#define XESS_INIT_FLAG_EXPOSURE_SCALE_TEXTURE   (1 << 2)
#define XESS_INIT_FLAG_RESPONSIVE_PIXEL_MASK    (1 << 3)
#define XESS_INIT_FLAG_USE_NDC_VELOCITY         (1 << 4)
#define XESS_INIT_FLAG_EXTERNAL_DESCRIPTOR_HEAP (1 << 5)
#define XESS_INIT_FLAG_LDR_INPUT_COLOR          (1 << 6)
#define XESS_INIT_FLAG_JITTERED_MV              (1 << 7)
#define XESS_INIT_FLAG_ENABLE_AUTOEXPOSURE      (1 << 8)

/* xess_logging_level_t */
#define XESS_LOGGING_LEVEL_DEBUG   0
#define XESS_LOGGING_LEVEL_INFO    1
#define XESS_LOGGING_LEVEL_WARNING 2
#define XESS_LOGGING_LEVEL_ERROR   3
typedef void (*xess_log_t)(const char *message, int level);

/* xess_d3d12_init_params_t */
typedef struct xess_d3d12_init_params
{
   xess_2d_t output_resolution;
   int quality;                /* XESS_QUALITY_ */
   uint32_t init_flags;        /* XESS_INIT_FLAG_ */
   uint32_t creation_node_mask;
   uint32_t visible_node_mask;
   void *temp_buffer_heap;     /* ID3D12Heap *, or NULL for the library's own */
   uint64_t buffer_heap_offset;
   void *temp_texture_heap;
   uint64_t texture_heap_offset;
   void *pipeline_library;     /* ID3D12PipelineLibrary *, or NULL */
} xess_d3d12_init_params_t;

/* xess_d3d12_execute_params_t: the inputs in NON_PIXEL_SHADER_RESOURCE,
 * the output in UNORDERED_ACCESS, left there */
typedef struct xess_d3d12_execute_params
{
   void *color;                /* ID3D12Resource * each */
   void *velocity;
   void *depth;
   void *exposure_scale;
   void *responsive_pixel_mask;
   void *output;
   float jitter_offset_x;
   float jitter_offset_y;
   float exposure_scale_value;
   uint32_t reset_history;
   uint32_t input_width;
   uint32_t input_height;
   xess_2d_t input_color_base;
   xess_2d_t input_motion_vector_base;
   xess_2d_t input_depth_base;
   xess_2d_t input_responsive_mask_base;
   xess_2d_t reserved0;
   xess_2d_t output_color_base;
   void *descriptor_heap;      /* ID3D12DescriptorHeap *, or NULL */
   uint32_t descriptor_heap_offset;
} xess_d3d12_execute_params_t;

/* xess_vk_image_view_info (the non-dispatchable handles as the 64-bit
 * ABI has them) */
typedef struct xess_vk_image_view_info
{
   uint64_t image_view;        /* VkImageView */
   uint64_t image;             /* VkImage */
   struct
   {
      uint32_t aspect_mask;
      uint32_t base_mip_level;
      uint32_t level_count;
      uint32_t base_array_layer;
      uint32_t layer_count;
   } subresource_range;
   int format;                 /* VkFormat */
   uint32_t width;
   uint32_t height;
} xess_vk_image_view_info_t;

/* xess_vk_execute_params_t: the inputs in SHADER_READ_ONLY_OPTIMAL,
 * the output in GENERAL, left there */
typedef struct xess_vk_execute_params
{
   xess_vk_image_view_info_t color;
   xess_vk_image_view_info_t velocity;
   xess_vk_image_view_info_t depth;
   xess_vk_image_view_info_t exposure_scale;
   xess_vk_image_view_info_t responsive_pixel_mask;
   xess_vk_image_view_info_t output;
   float jitter_offset_x;
   float jitter_offset_y;
   float exposure_scale_value;
   uint32_t reset_history;
   uint32_t input_width;
   uint32_t input_height;
   xess_2d_t input_color_base;
   xess_2d_t input_motion_vector_base;
   xess_2d_t input_depth_base;
   xess_2d_t input_responsive_mask_base;
   xess_2d_t reserved0;
   xess_2d_t output_color_base;
} xess_vk_execute_params_t;

/* xess_vk_init_params_t */
typedef struct xess_vk_init_params
{
   xess_2d_t output_resolution;
   int quality;
   uint32_t init_flags;
   uint32_t creation_node_mask;
   uint32_t visible_node_mask;
   uint64_t temp_buffer_heap;  /* VkDeviceMemory, or 0 for the library's own */
   uint64_t buffer_heap_offset;
   uint64_t temp_texture_heap;
   uint64_t texture_heap_offset;
   uint64_t pipeline_cache;    /* VkPipelineCache, or 0 */
} xess_vk_init_params_t;

/* The entry points of xess.h the frontend uses */
typedef struct xess_common_api
{
   xess_result_t (*get_version)(xess_version_t *version);
   xess_result_t (*get_optimal_input_resolution)(xess_context_t ctx,
         const xess_2d_t *output, int quality, xess_2d_t *optimal,
         xess_2d_t *min, xess_2d_t *max);
   xess_result_t (*destroy_context)(xess_context_t ctx);
   xess_result_t (*set_jitter_scale)(xess_context_t ctx, float x, float y);
   xess_result_t (*set_velocity_scale)(xess_context_t ctx, float x, float y);
   xess_result_t (*set_exposure_multiplier)(xess_context_t ctx, float scale);
   xess_result_t (*set_logging_callback)(xess_context_t ctx, int level,
         xess_log_t callback);
   /* After a non-blocking build_pipelines, before init: SUCCESS once
    * built, XESS_RESULT_ERROR_OPERATION_IN_PROGRESS while building.
    * NULL in a library without it: init builds them, blocking. */
   xess_result_t (*get_pipeline_build_status)(xess_context_t ctx);
} xess_common_api_t;

/* xess_d3d12.h; device, command list and pipeline library are Direct3D's */
typedef struct xess_d3d12_api
{
   xess_result_t (*create_context)(void *device, xess_context_t *ctx);
   xess_result_t (*build_pipelines)(xess_context_t ctx,
         void *pipeline_library, bool blocking, uint32_t init_flags);
   xess_result_t (*init)(xess_context_t ctx, const xess_d3d12_init_params_t *params);
   xess_result_t (*execute)(xess_context_t ctx, void *command_list,
         const xess_d3d12_execute_params_t *params);
} xess_d3d12_api_t;

/* xess_vk.h; instance, physical device, device and command buffer are
 * Vulkan's dispatchable handles */
typedef struct xess_vk_api
{
   xess_result_t (*get_required_instance_extensions)(uint32_t *count,
         const char *const **extensions, uint32_t *min_api_version);
   xess_result_t (*get_required_device_extensions)(void *instance,
         void *physical_device, uint32_t *count, const char *const **extensions);
   /* a chain of VkPhysicalDevice*Features structures to put on the
    * device's pNext, the library's own */
   xess_result_t (*get_required_device_features)(void *instance,
         void *physical_device, void **features);
   xess_result_t (*create_context)(void *instance, void *physical_device,
         void *device, xess_context_t *ctx);
   xess_result_t (*build_pipelines)(xess_context_t ctx,
         uint64_t pipeline_cache, bool blocking, uint32_t init_flags);
   xess_result_t (*init)(xess_context_t ctx, const xess_vk_init_params_t *params);
   xess_result_t (*execute)(xess_context_t ctx, void *command_buffer,
         const xess_vk_execute_params_t *params);
} xess_vk_api_t;

typedef struct xess_lib
{
   void *handle;
   xess_common_api_t common;
   xess_d3d12_api_t d3d12;     /* d3d12.create_context NULL: no Direct3D 12 */
   xess_vk_api_t vk;           /* vk.create_context NULL: no Vulkan */
   xess_version_t version;
} xess_lib_t;

/* The library in @dir (libxess.dll; libxess.so elsewhere), loaded the
 * first time it is found and kept: NULL where there is none (looked for
 * again next call, as the directory is a setting), or one that lacks
 * the common entry points or both APIs. Another directory takes a
 * restart. */
const xess_lib_t *xess_lib_get(const char *dir);

/* Whether @dir holds the library, the file the user supplies. Loads
 * nothing. */
bool xess_runtime_in(const char *dir);

/* What the probe found, for a log. Valid after xess_lib_get(); before,
 * "not probed". */
const char *xess_lib_status(void);

RETRO_END_DECLS

#endif
