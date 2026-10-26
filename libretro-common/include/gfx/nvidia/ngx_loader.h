/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (ngx_loader.h).
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

#ifndef _LIBRETRO_NGX_LOADER_H
#define _LIBRETRO_NGX_LOADER_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* NVIDIA's NGX, the runtime behind DLSS, loaded at run time and never
 * linked: the driver installs the NGX core library (_nvngx.dll beside
 * the program, else where the driver's registry keys point, with the
 * legacy key Wine and Proton set; libnvidia-ngx.so.1 on Linux), which
 * exports the NVSDK_NGX_* entry points that the SDK's static library
 * only forwards to. The feature itself (DLSS Super Resolution,
 * nvngx_dlss.dll / libnvidia-ngx-dlss.so.*) is the user's: NGX looks
 * for it beside the program and in the directories the caller lists
 * at init. No NVIDIA header, library or binary is needed to build or
 * ship; the declarations here are the SDK's public interface (names,
 * ids, layouts) restated, only what Super Resolution takes.
 *
 * Loaded the first time it is asked for and kept, one load per module
 * that links this, frontend or core alike. 64-bit Windows and Linux,
 * the only builds NVIDIA ships NGX for; every other platform answers
 * "no NGX". Needs encodings/encoding_utf.c for
 * ngx_utf8_to_wide(). */

#if defined(_WIN32)
#define NGX_CONV __cdecl
#else
#define NGX_CONV
#endif

/* NVSDK_NGX_Result: 1 success, 0xBAD00000 | n a failure */
typedef int ngx_result_t;
#define NGX_RESULT_SUCCESS                 0x1
#define NGX_RESULT_FAIL                    ((int)0xBAD00000)
#define NGX_RESULT_FAIL_FEATURE_NOT_SUPPORTED (NGX_RESULT_FAIL | 1)
#define NGX_RESULT_FAIL_PLATFORM_ERROR        (NGX_RESULT_FAIL | 2)
#define NGX_RESULT_FAIL_FEATURE_ALREADY_EXISTS (NGX_RESULT_FAIL | 3)
#define NGX_RESULT_FAIL_FEATURE_NOT_FOUND     (NGX_RESULT_FAIL | 4)
#define NGX_RESULT_FAIL_INVALID_PARAMETER     (NGX_RESULT_FAIL | 5)
#define NGX_RESULT_FAIL_SCRATCH_TOO_SMALL     (NGX_RESULT_FAIL | 6)
#define NGX_RESULT_FAIL_NOT_INITIALIZED       (NGX_RESULT_FAIL | 7)
#define NGX_RESULT_FAIL_UNSUPPORTED_INPUT_FORMAT (NGX_RESULT_FAIL | 8)
#define NGX_RESULT_FAIL_RW_FLAG_MISSING       (NGX_RESULT_FAIL | 9)
#define NGX_RESULT_FAIL_MISSING_INPUT         (NGX_RESULT_FAIL | 10)
#define NGX_RESULT_FAIL_UNABLE_TO_INIT_FEATURE (NGX_RESULT_FAIL | 11)
#define NGX_RESULT_FAIL_OUT_OF_DATE           (NGX_RESULT_FAIL | 12)
#define NGX_RESULT_FAIL_OUT_OF_GPU_MEMORY     (NGX_RESULT_FAIL | 13)
#define NGX_RESULT_FAIL_UNSUPPORTED_FORMAT    (NGX_RESULT_FAIL | 14)
#define NGX_RESULT_FAIL_UNABLE_TO_WRITE_TO_APP_DATA_PATH (NGX_RESULT_FAIL | 15)
#define NGX_RESULT_FAIL_UNSUPPORTED_PARAMETER (NGX_RESULT_FAIL | 16)
#define NGX_RESULT_FAIL_DENIED                (NGX_RESULT_FAIL | 17)
#define NGX_RESULT_FAIL_NOT_IMPLEMENTED       (NGX_RESULT_FAIL | 18)
#define NGX_SUCCEEDED(r) ((((unsigned)(r)) & 0xFFF00000u) != 0xBAD00000u)
#define NGX_FAILED(r)    ((((unsigned)(r)) & 0xFFF00000u) == 0xBAD00000u)

/* NVSDK_NGX_Version_API this interface restates */
#define NGX_VERSION_API 0x0000015

/* NVSDK_NGX_Feature: DLSS Super Resolution, and DLSS Neural Rendering
 * (DLSS 5, nvngx_dlssnr.dll), which NGX cores older than its driver
 * answer with NGX_RESULT_FAIL_OUT_OF_DATE */
#define NGX_FEATURE_SUPER_SAMPLING 1
#define NGX_FEATURE_DLSS_NR        18

/* NVSDK_NGX_EngineType */
#define NGX_ENGINE_TYPE_CUSTOM 0

/* NVSDK_NGX_PerfQuality_Value */
enum ngx_perf_quality
{
   NGX_PERF_QUALITY_MAX_PERF = 0,
   NGX_PERF_QUALITY_BALANCED,
   NGX_PERF_QUALITY_MAX_QUALITY,
   NGX_PERF_QUALITY_ULTRA_PERFORMANCE,
   NGX_PERF_QUALITY_ULTRA_QUALITY,
   NGX_PERF_QUALITY_DLAA
};

/* NVSDK_NGX_DLSS_Hint_Render_Preset: the letters as the SDK numbers
 * them. A letter the loaded feature library does not know reverts to
 * its default; A to G are the CNN models (DLSS 3), J and K the
 * transformer models (DLSS 310.x), the rest the runtime's to define. */
enum ngx_dlss_render_preset
{
   NGX_DLSS_PRESET_DEFAULT = 0,
   NGX_DLSS_PRESET_A       = 1,
   NGX_DLSS_PRESET_B       = 2,
   NGX_DLSS_PRESET_C       = 3,
   NGX_DLSS_PRESET_D       = 4,
   NGX_DLSS_PRESET_E       = 5,
   NGX_DLSS_PRESET_F       = 6,
   NGX_DLSS_PRESET_G       = 7,
   NGX_DLSS_PRESET_H       = 8,
   NGX_DLSS_PRESET_I       = 9,
   NGX_DLSS_PRESET_J       = 10,   /* transformer: less ghosting, some flicker */
   NGX_DLSS_PRESET_K       = 11,   /* transformer: the default of DLAA, Quality, Balanced */
   NGX_DLSS_PRESET_L       = 12,
   NGX_DLSS_PRESET_M       = 13,
   NGX_DLSS_PRESET_N       = 14,
   NGX_DLSS_PRESET_O       = 15,
   NGX_DLSS_PRESET_LAST
};

/* A mode as the frontend passes it, one word: the quality (an
 * ngx_perf_quality), the render preset, and the sharpness in percent
 * (the Sharpness parameter: honoured by DLSS 2.5.0 and older, and by
 * OptiScaler's RCAS; later DLSS ignores it) */
#define NGX_MODE_PACK(quality, preset) ((unsigned)(quality) | ((unsigned)(preset) << 8))
#define NGX_MODE_PACK_SHARP(quality, preset, sharpness) \
   (NGX_MODE_PACK(quality, preset) | ((unsigned)(sharpness) << 16))
#define NGX_MODE_QUALITY(mode)         ((mode) & 0xffu)
#define NGX_MODE_PRESET(mode)          (((mode) >> 8) & 0xffu)
#define NGX_MODE_SHARPNESS(mode)       (((mode) >> 16) & 0xffu)
#define NGX_MODE_FEATURE(mode)         ((mode) & 0xffffu)   /* what a feature is made for */

/* A feature library's version, major.minor.patch, as
 * ngx_dlss_feature_version returns it */
#define NGX_VERSION_PACK(major, minor, patch) \
   (((uint32_t)(major) << 16) | ((uint32_t)(minor) << 8) | (uint32_t)(patch))
#define NGX_VERSION_MAJOR(v) ((v) >> 16)
#define NGX_VERSION_MINOR(v) (((v) >> 8) & 0xffu)
#define NGX_VERSION_PATCH(v) ((v) & 0xffu)

/* NVSDK_NGX_DLSS_Feature_Flags */
#define NGX_DLSS_FLAG_IS_HDR          (1 << 0)
#define NGX_DLSS_FLAG_MV_LOW_RES      (1 << 1)
#define NGX_DLSS_FLAG_MV_JITTERED     (1 << 2)
#define NGX_DLSS_FLAG_DEPTH_INVERTED  (1 << 3)
#define NGX_DLSS_FLAG_AUTO_EXPOSURE   (1 << 6)
#define NGX_DLSS_FLAG_ALPHA_UPSCALING (1 << 7)

/* NVSDK_NGX_Feature_Support_Result bits, 0 supported */
#define NGX_SUPPORT_CHECK_NOT_PRESENT   1
#define NGX_SUPPORT_DRIVER_UNSUPPORTED  2
#define NGX_SUPPORT_ADAPTER_UNSUPPORTED 4
#define NGX_SUPPORT_OS_BELOW_MINIMUM    8
#define NGX_SUPPORT_NOT_IMPLEMENTED    16

/* NVSDK_NGX_Parameter names */
#define NGX_PARAM_SUPER_SAMPLING_AVAILABLE "SuperSampling.Available"
#define NGX_PARAM_SUPER_SAMPLING_NEEDS_UPDATED_DRIVER "SuperSampling.NeedsUpdatedDriver"
#define NGX_PARAM_SUPER_SAMPLING_MIN_DRIVER_MAJOR "SuperSampling.MinDriverVersionMajor"
#define NGX_PARAM_SUPER_SAMPLING_MIN_DRIVER_MINOR "SuperSampling.MinDriverVersionMinor"
#define NGX_PARAM_SUPER_SAMPLING_FEATURE_INIT_RESULT "SuperSampling.FeatureInitResult"
#define NGX_PARAM_WIDTH          "Width"
#define NGX_PARAM_HEIGHT         "Height"
#define NGX_PARAM_OUT_WIDTH      "OutWidth"
#define NGX_PARAM_OUT_HEIGHT     "OutHeight"
#define NGX_PARAM_SHARPNESS      "Sharpness"
#define NGX_PARAM_COLOR          "Color"
#define NGX_PARAM_OUTPUT         "Output"
#define NGX_PARAM_RESET          "Reset"
#define NGX_PARAM_MOTION_VECTORS "MotionVectors"
#define NGX_PARAM_MV_SCALE_X     "MV.Scale.X"
#define NGX_PARAM_MV_SCALE_Y     "MV.Scale.Y"
#define NGX_PARAM_DEPTH          "Depth"
#define NGX_PARAM_PERF_QUALITY   "PerfQualityValue"
#define NGX_PARAM_JITTER_OFFSET_X "Jitter.Offset.X"
#define NGX_PARAM_JITTER_OFFSET_Y "Jitter.Offset.Y"
/* The mask of pixels to take from this frame over the history
 * (particles, translucency): "TransparencyMask" is reserved, unread */
#define NGX_PARAM_BIAS_CURRENT_COLOR_MASK   "DLSS.Input.Bias.Current.Color.Mask"
#define NGX_PARAM_BIAS_CURRENT_COLOR_BASE_X "DLSS.Input.Bias.Current.Color.Subrect.Base.X"
#define NGX_PARAM_BIAS_CURRENT_COLOR_BASE_Y "DLSS.Input.Bias.Current.Color.Subrect.Base.Y"
#define NGX_PARAM_EXPOSURE_TEXTURE  "ExposureTexture"
#define NGX_PARAM_DLSS_CREATE_FLAGS "DLSS.Feature.Create.Flags"
/* The render preset a mode uses (ngx_dlss_render_preset), one hint a mode */
#define NGX_PARAM_DLSS_PRESET_DLAA        "DLSS.Hint.Render.Preset.DLAA"
#define NGX_PARAM_DLSS_PRESET_QUALITY     "DLSS.Hint.Render.Preset.Quality"
#define NGX_PARAM_DLSS_PRESET_BALANCED    "DLSS.Hint.Render.Preset.Balanced"
#define NGX_PARAM_DLSS_PRESET_PERFORMANCE "DLSS.Hint.Render.Preset.Performance"
#define NGX_PARAM_DLSS_PRESET_ULTRA_PERFORMANCE "DLSS.Hint.Render.Preset.UltraPerformance"
#define NGX_PARAM_DLSS_PRESET_ULTRA_QUALITY "DLSS.Hint.Render.Preset.UltraQuality"
#define NGX_PARAM_FRAME_TIME_DELTA_MS "FrameTimeDeltaInMsec"
#define NGX_PARAM_DLSS_ENABLE_OUTPUT_SUBRECTS "DLSS.Enable.Output.Subrects"
#define NGX_PARAM_DLSS_RENDER_SUBRECT_WIDTH  "DLSS.Render.Subrect.Dimensions.Width"
#define NGX_PARAM_DLSS_RENDER_SUBRECT_HEIGHT "DLSS.Render.Subrect.Dimensions.Height"
#define NGX_PARAM_DLSS_PRE_EXPOSURE   "DLSS.Pre.Exposure"
#define NGX_PARAM_DLSS_EXPOSURE_SCALE "DLSS.Exposure.Scale"
#define NGX_PARAM_DLSS_INDICATOR_INVERT_Y "DLSS.Indicator.Invert.Y.Axis"
#define NGX_PARAM_DLSS_INDICATOR_INVERT_X "DLSS.Indicator.Invert.X.Axis"
#define NGX_PARAM_DLSS_OPTIMAL_SETTINGS_CALLBACK "DLSSOptimalSettingsCallback"
#define NGX_PARAM_RTX_VALUE      "RTXValue"
#define NGX_PARAM_CREATION_NODE_MASK   "CreationNodeMask"
#define NGX_PARAM_VISIBILITY_NODE_MASK "VisibilityNodeMask"
#define NGX_PARAM_DLSS_INPUT_COLOR_SUBRECT_X "DLSS.Input.Color.Subrect.Base.X"
#define NGX_PARAM_DLSS_INPUT_COLOR_SUBRECT_Y "DLSS.Input.Color.Subrect.Base.Y"
#define NGX_PARAM_DLSS_INPUT_DEPTH_SUBRECT_X "DLSS.Input.Depth.Subrect.Base.X"
#define NGX_PARAM_DLSS_INPUT_DEPTH_SUBRECT_Y "DLSS.Input.Depth.Subrect.Base.Y"
#define NGX_PARAM_DLSS_INPUT_MV_SUBRECT_X    "DLSS.Input.MV.Subrect.Base.X"
#define NGX_PARAM_DLSS_INPUT_MV_SUBRECT_Y    "DLSS.Input.MV.Subrect.Base.Y"
#define NGX_PARAM_DLSS_OUTPUT_SUBRECT_X      "DLSS.Output.Subrect.Base.X"
#define NGX_PARAM_DLSS_OUTPUT_SUBRECT_Y      "DLSS.Output.Subrect.Base.Y"
#define NGX_PARAM_DLSS_DYNAMIC_MAX_RENDER_WIDTH  "DLSS.Get.Dynamic.Max.Render.Width"
#define NGX_PARAM_DLSS_DYNAMIC_MAX_RENDER_HEIGHT "DLSS.Get.Dynamic.Max.Render.Height"
#define NGX_PARAM_DLSS_DYNAMIC_MIN_RENDER_WIDTH  "DLSS.Get.Dynamic.Min.Render.Width"
#define NGX_PARAM_DLSS_DYNAMIC_MIN_RENDER_HEIGHT "DLSS.Get.Dynamic.Min.Render.Height"
#define NGX_PARAM_DLSS_HINT_PRESET_DLAA        "DLSS.Hint.Render.Preset.DLAA"
#define NGX_PARAM_DLSS_HINT_PRESET_QUALITY     "DLSS.Hint.Render.Preset.Quality"
#define NGX_PARAM_DLSS_HINT_PRESET_BALANCED    "DLSS.Hint.Render.Preset.Balanced"
#define NGX_PARAM_DLSS_HINT_PRESET_PERFORMANCE "DLSS.Hint.Render.Preset.Performance"
#define NGX_PARAM_DLSS_HINT_PRESET_ULTRA_PERFORMANCE "DLSS.Hint.Render.Preset.UltraPerformance"

/* DLSS Neural Rendering's parameters. The capability set says whether
 * it is there (a core without it has no such key); the model's tuning
 * is read when the feature is created, the rest at every evaluate. */
#define NGX_PARAM_DLSSNR_AVAILABLE       "DLSSNR.Available"
#define NGX_PARAM_DLSSNR_FEATURE_INIT_RESULT "DLSSNR.FeatureInitResult"
#define NGX_PARAM_DLSSNR_ENABLED         "DLSSNR.Enabled"
#define NGX_PARAM_DLSSNR_WIDTH           "DLSSNR.Width"
#define NGX_PARAM_DLSSNR_HEIGHT          "DLSSNR.Height"
#define NGX_PARAM_DLSSNR_PRESET          "DLSSNR.Hint.Render.Preset"
#define NGX_PARAM_DLSSNR_STYLE           "DLSSNR.Style"
#define NGX_PARAM_DLSSNR_INTENSITY       "DLSSNR.Intensity"
#define NGX_PARAM_DLSSNR_LOCAL_STRUCTURE "DLSSNR.LocalStructureStrength"
#define NGX_PARAM_DLSSNR_LOCAL_TONE      "DLSSNR.LocalToneStrength"
#define NGX_PARAM_DLSSNR_SKIN_STRUCTURE  "DLSSNR.SkinStructureStrength"
#define NGX_PARAM_DLSSNR_AUTO_MASK       "DLSSNR.UseAutoMask"
#define NGX_PARAM_DLSSNR_COLOR           "DLSSNR.Color"
#define NGX_PARAM_DLSSNR_DEPTH           "DLSSNR.Depth"
#define NGX_PARAM_DLSSNR_MVEC            "DLSSNR.MVec"
#define NGX_PARAM_DLSSNR_OUTPUT          "DLSSNR.Output"
#define NGX_PARAM_DLSSNR_DEPTH_INVERTED  "DLSSNR.DepthInverted"
#define NGX_PARAM_DLSSNR_RESET           "DLSSNR.Reset"
#define NGX_PARAM_DLSSNR_MVEC_SCALE_X    "DLSSNR.MVecScaleX"
#define NGX_PARAM_DLSSNR_MVEC_SCALE_Y    "DLSSNR.MVecScaleY"
/* Each resource's subrect: base x and y, width and height */
#define NGX_PARAM_DLSSNR_COLOR_SUBRECT   "DLSSNR.ColorSubrect"
#define NGX_PARAM_DLSSNR_DEPTH_SUBRECT   "DLSSNR.DepthSubrect"
#define NGX_PARAM_DLSSNR_MVEC_SUBRECT    "DLSSNR.MVecSubrect"
#define NGX_PARAM_DLSSNR_OUTPUT_SUBRECT  "DLSSNR.OutputSubrect"

/* A Vulkan proc-address getter (PFN_vkGetInstanceProcAddr,
 * PFN_vkGetDeviceProcAddr), cast to this */
typedef void (*ngx_vk_proc_t)(void);

/* Opaque to the caller: NVSDK_NGX_Handle, NVSDK_NGX_Parameter */
typedef struct ngx_handle ngx_handle_t;
typedef struct ngx_parameter ngx_parameter_t;

/* NVSDK_NGX_FeatureCommonInfo, as the SDK lays it out */
typedef struct ngx_path_list_info
{
   const wchar_t *const *path;
   unsigned length;
} ngx_path_list_info_t;

typedef void (NGX_CONV *ngx_log_callback_t)(const char *message,
      int level, int source_feature);

typedef struct ngx_logging_info
{
   ngx_log_callback_t callback;
   int minimum_level;          /* 0 off, 1 on, 2 verbose */
   unsigned char disable_other_sinks;
} ngx_logging_info_t;

typedef struct ngx_feature_common_info
{
   ngx_path_list_info_t path_list;
   void *internal;
   ngx_logging_info_t logging;
} ngx_feature_common_info_t;

/* NVSDK_NGX_FeatureDiscoveryInfo with a project id */
typedef struct ngx_project_id_desc
{
   const char *project_id;
   int engine_type;
   const char *engine_version;
} ngx_project_id_desc_t;

typedef struct ngx_feature_discovery_info
{
   int sdk_version;
   int feature_id;
   int identifier_type;     /* 1: project id */
   union
   {
      ngx_project_id_desc_t project;
      uint64_t application_id;
   } v;
   const wchar_t *application_data_path;
   const ngx_feature_common_info_t *feature_info;
} ngx_feature_discovery_info_t;

typedef struct ngx_feature_requirement
{
   int feature_supported;   /* NGX_SUPPORT_* bits, 0 supported */
   unsigned min_hw_architecture;
   char min_os_version[255];
} ngx_feature_requirement_t;

/* NVSDK_NGX_Resource_VK; the Vulkan handles are void pointers here so
 * nothing Vulkan is included, cast at the call. SubresourceRange is
 * VkImageSubresourceRange's five words. */
typedef struct ngx_vk_image_view_info
{
   void *image_view;
   void *image;
   struct
   {
      unsigned aspect_mask;
      unsigned base_mip_level;
      unsigned level_count;
      unsigned base_array_layer;
      unsigned layer_count;
   } subresource_range;
   int format;
   unsigned width;
   unsigned height;
} ngx_vk_image_view_info_t;

typedef struct ngx_vk_buffer_info
{
   void *buffer;
   unsigned size_in_bytes;
} ngx_vk_buffer_info_t;

typedef struct ngx_vk_resource
{
   union
   {
      ngx_vk_image_view_info_t image_view;
      ngx_vk_buffer_info_t buffer;
   } resource;
   int type;                /* 0 image view, 1 buffer */
   unsigned char read_write;
} ngx_vk_resource_t;

/* NVSDK_NGX_Parameter's accessors: the object's own virtual methods,
 * called through its vtable (the library exports no C accessors) */
typedef struct ngx_param_api
{
   void (NGX_CONV *set_ull)(ngx_parameter_t *p, const char *name, uint64_t v);
   void (NGX_CONV *set_f)(ngx_parameter_t *p, const char *name, float v);
   void (NGX_CONV *set_d)(ngx_parameter_t *p, const char *name, double v);
   void (NGX_CONV *set_ui)(ngx_parameter_t *p, const char *name, unsigned v);
   void (NGX_CONV *set_i)(ngx_parameter_t *p, const char *name, int v);
   void (NGX_CONV *set_d3d11_resource)(ngx_parameter_t *p, const char *name, void *v);
   void (NGX_CONV *set_d3d12_resource)(ngx_parameter_t *p, const char *name, void *v);
   void (NGX_CONV *set_void_pointer)(ngx_parameter_t *p, const char *name, void *v);
   ngx_result_t (NGX_CONV *get_ull)(ngx_parameter_t *p, const char *name, uint64_t *v);
   ngx_result_t (NGX_CONV *get_f)(ngx_parameter_t *p, const char *name, float *v);
   ngx_result_t (NGX_CONV *get_d)(ngx_parameter_t *p, const char *name, double *v);
   ngx_result_t (NGX_CONV *get_ui)(ngx_parameter_t *p, const char *name, unsigned *v);
   ngx_result_t (NGX_CONV *get_i)(ngx_parameter_t *p, const char *name, int *v);
   ngx_result_t (NGX_CONV *get_d3d11_resource)(ngx_parameter_t *p, const char *name, void **v);
   ngx_result_t (NGX_CONV *get_d3d12_resource)(ngx_parameter_t *p, const char *name, void **v);
   ngx_result_t (NGX_CONV *get_void_pointer)(ngx_parameter_t *p, const char *name, void **v);
} ngx_param_api_t;

/* The Direct3D 12 entry points; device, command list and adapter are
 * the COM interfaces. All NULL where the library has no D3D12 (Linux). */
typedef struct ngx_d3d12_api
{
   /* NVSDK_NGX_D3D12_Init_ProjectID: the SDK version before the
    * feature info, as the library has it (the SDK's static library
    * shows applications the other order under Init_with_ProjectID) */
   ngx_result_t (NGX_CONV *init_project_id)(const char *project_id,
         int engine_type, const char *engine_version,
         const wchar_t *application_data_path, void *device,
         int sdk_version, const ngx_feature_common_info_t *info);
   /* NVSDK_NGX_*_Shutdown1 as the library exports it: the SDK's static
    * library hands it an int it writes (whether the library is still
    * in use), which the SDK's own prototype leaves out */
   ngx_result_t (NGX_CONV *shutdown1)(void *device, int *in_use);
   ngx_result_t (NGX_CONV *get_capability_parameters)(ngx_parameter_t **out);
   ngx_result_t (NGX_CONV *allocate_parameters)(ngx_parameter_t **out);
   ngx_result_t (NGX_CONV *destroy_parameters)(ngx_parameter_t *p);
   ngx_result_t (NGX_CONV *get_feature_requirements)(void *adapter,
         const ngx_feature_discovery_info_t *info,
         ngx_feature_requirement_t *out);
   ngx_result_t (NGX_CONV *create_feature)(void *cmd_list, int feature_id,
         ngx_parameter_t *p, ngx_handle_t **out);
   ngx_result_t (NGX_CONV *release_feature)(ngx_handle_t *h);
   ngx_result_t (NGX_CONV *evaluate_feature)(void *cmd_list,
         const ngx_handle_t *h, ngx_parameter_t *p, void *progress_callback);
} ngx_d3d12_api_t;

/* The Vulkan entry points; instance, physical device, device, command
 * buffer and the two proc-address getters are Vulkan's own handles. */
typedef struct ngx_vulkan_api
{
   /* NULL where the library has only the two feature requirement
    * calls after it, which then are both set */
   ngx_result_t (NGX_CONV *required_extensions)(unsigned *instance_count,
         const char ***instance_exts, unsigned *device_count,
         const char ***device_exts);
   ngx_result_t (NGX_CONV *get_feature_instance_extension_requirements)(
         const ngx_feature_discovery_info_t *info, uint32_t *count,
         void **extension_properties);
   ngx_result_t (NGX_CONV *get_feature_device_extension_requirements)(
         void *instance, void *physical_device,
         const ngx_feature_discovery_info_t *info, uint32_t *count,
         void **extension_properties);
   ngx_result_t (NGX_CONV *get_feature_requirements)(void *instance,
         void *physical_device, const ngx_feature_discovery_info_t *info,
         ngx_feature_requirement_t *out);
   /* NVSDK_NGX_VULKAN_Init_ProjectID (as the D3D12 one: the SDK
    * version before the feature info); the library resolves the
    * Vulkan entry points itself */
   ngx_result_t (NGX_CONV *init_project_id)(const char *project_id,
         int engine_type, const char *engine_version,
         const wchar_t *application_data_path, void *instance,
         void *physical_device, void *device,
         int sdk_version, const ngx_feature_common_info_t *info);
   /* NVSDK_NGX_VULKAN_Init_ProjectID_Ext: the same, with the
    * application's proc-address getters; NULL in older libraries */
   ngx_result_t (NGX_CONV *init_project_id_ext)(const char *project_id,
         int engine_type, const char *engine_version,
         const wchar_t *application_data_path, void *instance,
         void *physical_device, void *device,
         ngx_vk_proc_t get_instance_proc_addr,
         ngx_vk_proc_t get_device_proc_addr,
         int sdk_version, const ngx_feature_common_info_t *info);
   /* NVSDK_NGX_*_Shutdown1 as the library exports it: the SDK's static
    * library hands it an int it writes (whether the library is still
    * in use), which the SDK's own prototype leaves out */
   ngx_result_t (NGX_CONV *shutdown1)(void *device, int *in_use);
   ngx_result_t (NGX_CONV *get_capability_parameters)(ngx_parameter_t **out);
   ngx_result_t (NGX_CONV *allocate_parameters)(ngx_parameter_t **out);
   ngx_result_t (NGX_CONV *destroy_parameters)(ngx_parameter_t *p);
   ngx_result_t (NGX_CONV *create_feature1)(void *device, void *cmd_buffer,
         int feature_id, ngx_parameter_t *p, ngx_handle_t **out);
   ngx_result_t (NGX_CONV *release_feature)(ngx_handle_t *h);
   ngx_result_t (NGX_CONV *evaluate_feature)(void *cmd_buffer,
         const ngx_handle_t *h, ngx_parameter_t *p, void *progress_callback);
} ngx_vulkan_api_t;

typedef struct ngx_lib
{
   void *handle;
   ngx_param_api_t param;
   ngx_d3d12_api_t d3d12;      /* d3d12.init_project_id NULL: no D3D12 */
   ngx_vulkan_api_t vulkan;    /* vulkan.init_project_id NULL: no Vulkan */
} ngx_lib_t;

/* The NGX core library, found and loaded the first time, with every
 * entry point above resolved: NULL where there is none (no NVIDIA
 * driver, an older one without NGX, another platform) or one that
 * lacks the parameter accessors. A found library is kept loaded. */
const ngx_lib_t *ngx_lib_get(void);

/* What the probe found, for a log: where the library was looked for
 * and loaded from, and which entry points it lacked. Valid after
 * ngx_lib_get(); before, "not probed". */
const char *ngx_lib_status(void);

/* Whether @dir holds a DLSS Super Resolution feature library
 * (nvngx_dlss.dll on Windows, libnvidia-ngx-dlss.so.* on Linux), the
 * file the user supplies. Needs no NGX. */
bool ngx_dlss_feature_in(const char *dir);

/* The version of the feature library in @dir (NGX_VERSION_PACK): on
 * Windows its file version, on Linux its file name's; else what its
 * NVSDK_NGX_GetSnippetVersion says. 0 where there is none, or it does
 * not say. Loading the library for the export keeps it loaded (NGX
 * loads the same file from the directory later). */
uint32_t ngx_dlss_feature_version(const char *dir);

/* Whether @dir holds the DLSS Neural Rendering feature library
 * (nvngx_dlssnr.dll; libnvidia-ngx-dlssnr.so.* on Linux), and its
 * version (its file version, on Linux its file name's; 0 where there
 * is none or it does not say). The library is never loaded for it. */
bool ngx_dlssnr_feature_in(const char *dir);
uint32_t ngx_dlssnr_feature_version(const char *dir);

/* NVSDK_NGX_GetSnippetVersion's word as NGX_VERSION_PACK: major in the
 * high half, minor and patch a byte each below it */
uint32_t ngx_snippet_version(uint32_t snippet);

/* Whether the NGX core library the probe found is OptiScaler standing
 * in for it (its module says so, on Windows); the status names its
 * version. Valid after ngx_lib_get(). */
bool ngx_lib_is_optiscaler(void);

/* OptiScaler's own file name. Put in the DLSS directory it is loaded
 * ahead of the NGX core library, which it then hooks and answers for,
 * instead of standing beside the executable under a system DLL's name.
 * Windows only. */
#define NGX_STAND_IN_NAME "OptiScaler.dll"

/* Whether @dir holds NGX_STAND_IN_NAME; needs no NGX, loads nothing */
bool ngx_stand_in_in(const char *dir);

/* Loads NGX_STAND_IN_NAME from @dir, kept loaded: only before
 * ngx_lib_get() has loaded the core library. True once it is in. */
bool ngx_lib_load_stand_in(const char *dir);

/* Keeps the feature libraries NGX has loaded from @dir (DLSS's, Neural
 * Rendering's) loaded until the process ends, so that NGX made again on
 * another device finds them in instead of loading them again. Call
 * after a feature is made; loads nothing itself. */
void ngx_lib_keep_features(const char *dir);

/* Whether there is an NGX core library to load (NGX_PRESENT_FOUND)
 * and whether it is OptiScaler (NGX_PRESENT_OPTISCALER), as a status
 * shows it. On Windows the file is found and read without loading it:
 * a stand-in hooks the process once loaded. */
#define NGX_PRESENT_FOUND      0x1u
#define NGX_PRESENT_OPTISCALER 0x2u
unsigned ngx_lib_present(void);

/* Whether the parameter table @p (the capability parameters, or any
 * allocated) came from OptiScaler: its tables carry the "OptiScaler"
 * key. Catches OptiScaler reached through a proxy of another name
 * too. */
bool ngx_params_from_optiscaler(const ngx_lib_t *lib, ngx_parameter_t *p);

/* @utf8 as the wide string NGX takes for its paths, into @out of
 * @out_chars (wchar_t), always terminated; false, and @out empty, when
 * it does not fit. */
bool ngx_utf8_to_wide(wchar_t *out, size_t out_chars, const char *utf8);

RETRO_END_DECLS

#endif
