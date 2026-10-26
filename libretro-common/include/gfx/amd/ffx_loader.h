/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (ffx_loader.h).
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

#ifndef __LIBRETRO_SDK_GFX_AMD_FFX_LOADER_H
#define __LIBRETRO_SDK_GFX_AMD_FFX_LOADER_H

/* AMD FidelityFX Super Resolution through the FidelityFX API (ffx-api),
 * loaded at run time from the library the user supplies: nothing of
 * AMD's is built in or shipped. The library is the SDK's own, which
 * ships under the MIT license:
 *  - Direct3D 12: amd_fidelityfx_loader_dx12.dll of SDK 2 (which loads
 *    amd_fidelityfx_upscaler_dx12.dll beside it: FSR 4 on the GPUs that
 *    have it, FSR 3.1 elsewhere), or amd_fidelityfx_dx12.dll of SDK 1.1
 *    (FSR 3.1);
 *  - Vulkan: amd_fidelityfx_vk.dll of SDK 1.1 (FSR 3.1; SDK 2 has no
 *    Vulkan).
 * One entry point set serves both (ffx_api.h): five functions and
 * descriptors chained by their headers, whose layouts and type numbers
 * are the SDK's, declared here so that no SDK header is needed. The
 * structures mirror the SDK's to the byte on x86-64. */

#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef uint32_t ffx_return_t;
#define FFX_RETURN_OK                      0
#define FFX_RETURN_ERROR                   1
#define FFX_RETURN_ERROR_UNKNOWN_DESCTYPE  2
#define FFX_RETURN_ERROR_RUNTIME_ERROR     3
#define FFX_RETURN_NO_PROVIDER             4
#define FFX_RETURN_ERROR_MEMORY            5
#define FFX_RETURN_ERROR_PARAMETER         6

typedef void *ffx_context_t;

/* Every descriptor starts with this; the API is handed the header */
typedef struct ffx_header
{
   uint64_t type;
   struct ffx_header *next;
} ffx_header_t;

/* Descriptor types (the effect's id in the high word) */
#define FFX_TYPE_GET_VERSIONS            4u
#define FFX_TYPE_OVERRIDE_VERSION        5u
#define FFX_TYPE_CONFIGURE_GLOBALDEBUG1  1u
#define FFX_TYPE_BACKEND_DX12            0x00000002u
#define FFX_TYPE_BACKEND_VK              0x00000003u
#define FFX_TYPE_UPSCALE_CREATE          0x00010000u
#define FFX_TYPE_UPSCALE_DISPATCH        0x00010001u
#define FFX_TYPE_UPSCALE_RATIO_OF_MODE   0x00010002u
#define FFX_TYPE_UPSCALE_RENDER_OF_MODE  0x00010003u
#define FFX_TYPE_UPSCALE_VERSION         0x0001000bu

/* FfxApiUpscaleQualityMode */
#define FFX_UPSCALE_MODE_NATIVEAA          0
#define FFX_UPSCALE_MODE_QUALITY           1
#define FFX_UPSCALE_MODE_BALANCED          2
#define FFX_UPSCALE_MODE_PERFORMANCE       3
#define FFX_UPSCALE_MODE_ULTRA_PERFORMANCE 4

/* FfxApiCreateContextUpscaleFlags */
#define FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE        (1 << 0)
#define FFX_UPSCALE_ENABLE_DISPLAY_RESOLUTION_MVS    (1 << 1)
#define FFX_UPSCALE_ENABLE_MV_JITTER_CANCELLATION    (1 << 2)
#define FFX_UPSCALE_ENABLE_DEPTH_INVERTED            (1 << 3)
#define FFX_UPSCALE_ENABLE_DEPTH_INFINITE            (1 << 4)
#define FFX_UPSCALE_ENABLE_AUTO_EXPOSURE             (1 << 5)
#define FFX_UPSCALE_ENABLE_DYNAMIC_RESOLUTION        (1 << 6)
#define FFX_UPSCALE_ENABLE_DEBUG_CHECKING            (1 << 7)
#define FFX_UPSCALE_ENABLE_NON_LINEAR_COLORSPACE     (1 << 8)

/* FfxApiDispatchFsrUpscaleFlags */
#define FFX_UPSCALE_FLAG_NON_LINEAR_COLOR_SRGB (1 << 1)
#define FFX_UPSCALE_FLAG_NON_LINEAR_COLOR_PQ   (1 << 2)

/* FfxApiSurfaceFormat, the ones a frame brings */
#define FFX_FORMAT_UNKNOWN              0
#define FFX_FORMAT_R32G32B32A32_FLOAT   3
#define FFX_FORMAT_R16G16B16A16_FLOAT   4
#define FFX_FORMAT_R32G32_FLOAT         6
#define FFX_FORMAT_R8_UINT              7
#define FFX_FORMAT_R32_UINT             8
#define FFX_FORMAT_R8G8B8A8_UNORM       10
#define FFX_FORMAT_R8G8B8A8_SRGB        12
#define FFX_FORMAT_B8G8R8A8_UNORM       14
#define FFX_FORMAT_B8G8R8A8_SRGB        15
#define FFX_FORMAT_R11G11B10_FLOAT      16
#define FFX_FORMAT_R10G10B10A2_UNORM    17
#define FFX_FORMAT_R16G16_FLOAT         18
#define FFX_FORMAT_R16_FLOAT            21
#define FFX_FORMAT_R16_UNORM            23
#define FFX_FORMAT_R8_UNORM             25
#define FFX_FORMAT_R32_FLOAT            28

/* FfxApiResourceUsage */
#define FFX_USAGE_READ_ONLY     0
#define FFX_USAGE_RENDERTARGET  (1 << 0)
#define FFX_USAGE_UAV           (1 << 1)
#define FFX_USAGE_DEPTHTARGET   (1 << 2)
#define FFX_USAGE_STENCILTARGET (1 << 5)

/* FfxApiResourceState */
#define FFX_STATE_COMMON            (1 << 0)
#define FFX_STATE_UNORDERED_ACCESS  (1 << 1)
#define FFX_STATE_COMPUTE_READ      (1 << 2)
#define FFX_STATE_PIXEL_READ        (1 << 3)
#define FFX_STATE_COPY_SRC          (1 << 4)
#define FFX_STATE_COPY_DEST         (1 << 5)
#define FFX_STATE_RENDER_TARGET     (1 << 8)
#define FFX_STATE_DEPTH_ATTACHMENT  (1 << 9)

/* FfxApiResourceType */
#define FFX_RESOURCE_TEXTURE2D 2

typedef struct ffx_dims
{
   uint32_t width;
   uint32_t height;
} ffx_dims_t;

typedef struct ffx_coords
{
   float x;
   float y;
} ffx_coords_t;

/* FfxApiResourceDescription (its unions laid out as their first member) */
typedef struct ffx_resource_desc
{
   uint32_t type;        /* FFX_RESOURCE_ */
   uint32_t format;      /* FFX_FORMAT_ */
   uint32_t width;
   uint32_t height;
   uint32_t depth;
   uint32_t mip_count;
   uint32_t flags;
   uint32_t usage;       /* FFX_USAGE_ */
} ffx_resource_desc_t;

/* FfxApiResource: the API's handle (ID3D12Resource *, or the VkImage
 * as a pointer), and the state it is in, which the dispatch leaves it in */
typedef struct ffx_resource
{
   void *resource;
   ffx_resource_desc_t description;
   uint32_t state;       /* FFX_STATE_ */
} ffx_resource_t;

/* A message from the runtime: type 0 error, 1 warning */
typedef void (*ffx_message_t)(uint32_t type, const wchar_t *message);

/* ffxConfigureDescGlobalDebug1, with a NULL context */
typedef struct ffx_configure_debug
{
   ffx_header_t header;
   ffx_message_t message;
   uint32_t debug_level;    /* 0 silence, 1 errors, 2 warnings */
} ffx_configure_debug_t;

/* ffxQueryDescGetVersions, with a NULL context: count first (ids and
 * names NULL), then both arrays of that many */
typedef struct ffx_query_versions
{
   ffx_header_t header;
   uint64_t create_desc_type;  /* FFX_TYPE_UPSCALE_CREATE */
   void *device;               /* Direct3D 12: the ID3D12Device *; Vulkan: NULL */
   uint64_t *output_count;
   uint64_t *version_ids;
   const char **version_names;
} ffx_query_versions_t;

/* ffxOverrideVersion: chained after the backend's descriptor */
typedef struct ffx_override_version
{
   ffx_header_t header;
   uint64_t version_id;
} ffx_override_version_t;

/* ffxCreateBackendDX12Desc */
typedef struct ffx_backend_dx12
{
   ffx_header_t header;
   void *device;               /* ID3D12Device * */
} ffx_backend_dx12_t;

/* ffxCreateBackendVKDesc */
typedef struct ffx_backend_vk
{
   ffx_header_t header;
   void *device;               /* VkDevice */
   void *physical_device;      /* VkPhysicalDevice */
   void *get_device_proc_addr; /* PFN_vkGetDeviceProcAddr */
} ffx_backend_vk_t;

/* ffxCreateContextDescUpscale */
typedef struct ffx_upscale_create
{
   ffx_header_t header;
   uint32_t flags;             /* FFX_UPSCALE_ENABLE_ */
   ffx_dims_t max_render_size;
   ffx_dims_t max_upscale_size;
   ffx_message_t message;
} ffx_upscale_create_t;

/* ffxCreateContextDescUpscaleVersion: the API version built against */
typedef struct ffx_upscale_version
{
   ffx_header_t header;
   uint32_t version;
} ffx_upscale_version_t;
#define FFX_UPSCALER_VERSION(major, minor, patch) (((major) << 22) | ((minor) << 12) | (patch))

/* ffxDispatchDescUpscale (its bools a byte each, as the SDK's compilers
 * have them) */
typedef struct ffx_upscale_dispatch
{
   ffx_header_t header;
   void *command_list;         /* ID3D12GraphicsCommandList *, or VkCommandBuffer */
   ffx_resource_t color;
   ffx_resource_t depth;
   ffx_resource_t motion_vectors;
   ffx_resource_t exposure;
   ffx_resource_t reactive;
   ffx_resource_t transparency_and_composition;
   ffx_resource_t output;
   ffx_coords_t jitter_offset;
   ffx_coords_t motion_vector_scale;
   ffx_dims_t render_size;
   ffx_dims_t upscale_size;
   unsigned char enable_sharpening;
   float sharpness;
   float frame_time_delta;     /* ms */
   float pre_exposure;
   unsigned char reset;
   float camera_near;
   float camera_far;
   float camera_fov_angle_vertical;  /* radians */
   float view_space_to_meters_factor;
   uint32_t flags;             /* FFX_UPSCALE_FLAG_ */
} ffx_upscale_dispatch_t;

/* ffxQueryDescUpscaleGetRenderResolutionFromQualityMode, with a NULL
 * context (or one) */
typedef struct ffx_query_render_of_mode
{
   ffx_header_t header;
   uint32_t display_width;
   uint32_t display_height;
   uint32_t quality_mode;      /* FFX_UPSCALE_MODE_ */
   uint32_t *out_render_width;
   uint32_t *out_render_height;
} ffx_query_render_of_mode_t;

/* The five entry points of ffx_api.h */
typedef struct ffx_lib
{
   void *handle;
   ffx_return_t (*create_context)(ffx_context_t *context,
         ffx_header_t *desc, const void *mem_cb);
   ffx_return_t (*destroy_context)(ffx_context_t *context, const void *mem_cb);
   ffx_return_t (*configure)(ffx_context_t *context, const ffx_header_t *desc);
   ffx_return_t (*query)(ffx_context_t *context, ffx_header_t *desc);
   ffx_return_t (*dispatch)(ffx_context_t *context, const ffx_header_t *desc);
} ffx_lib_t;

/* Which API's library */
enum ffx_api
{
   FFX_API_D3D12 = 0,
   FFX_API_VULKAN,
   FFX_API_LAST
};

/* The library for @api in @dir, loaded the first time it is found and
 * kept: NULL where there is none (looked for again next call, as the
 * directory is a setting), or one that lacks an entry point. A library
 * loaded from one directory is the one there is after; another
 * directory takes a restart. */
const ffx_lib_t *ffx_lib_get(enum ffx_api api, const char *dir);

/* Whether @dir holds the library for @api, the file the user supplies.
 * Loads nothing. */
bool ffx_runtime_in(enum ffx_api api, const char *dir);

/* What the probe found, for a log: the file loaded, or why not. Valid
 * after ffx_lib_get(); before, "not probed". */
const char *ffx_lib_status(enum ffx_api api);

/* The upscaler versions @lib offers (@device: Direct3D 12's, NULL for
 * Vulkan), their ids into @ids and names into @names, up to @cap: how
 * many there are (0: none, or the query failed). Names are copied
 * out by the caller before the next query. The first is the newest. */
unsigned ffx_lib_versions(const ffx_lib_t *lib, void *device,
      uint64_t *ids, const char **names, unsigned cap);

RETRO_END_DECLS

#endif
