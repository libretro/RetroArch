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

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <compat/strl.h>
#include <retro_atomic.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <gfx/nvidia/ngx_loader.h>
#include <gfx/amd/ffx_loader.h>
#include <gfx/intel/xess_loader.h>

#include "../configuration.h"
#include "../verbosity.h"
#include "video_driver.h"
#include "video_upscaler.h"

const char *video_upscaler_name(enum video_temporal_upscaler kind)
{
   switch (kind)
   {
      case VIDEO_TEMPORAL_UPSCALER_DLSS: return "DLSS";
      case VIDEO_TEMPORAL_UPSCALER_FSR:  return "FSR";
      case VIDEO_TEMPORAL_UPSCALER_XESS: return "XeSS";
      default:                           break;
   }
   return "Off";
}

size_t video_upscaler_dir(enum video_temporal_upscaler kind, char *s, size_t len)
{
   settings_t *settings = config_get_ptr();
   const char *set      = NULL;
   const char *fallback = NULL;
   switch (kind)
   {
      case VIDEO_TEMPORAL_UPSCALER_DLSS:
         set      = settings->paths.directory_dlss;
         fallback = ":/upscalers/dlss";
         break;
      case VIDEO_TEMPORAL_UPSCALER_FSR:
         set      = settings->paths.directory_fsr;
         fallback = ":/upscalers/fsr";
         break;
      case VIDEO_TEMPORAL_UPSCALER_XESS:
         set      = settings->paths.directory_xess;
         fallback = ":/upscalers/xess";
         break;
      default:
         *s = '\0';
         return 0;
   }
   if (!string_is_empty(set))
      return strlcpy(s, set, len);
   /* the application directory's: beside shaders and system */
   return fill_pathname_expand_special(s, fallback, len);
}

/* Which API's FSR library the driver up would load: the probe looks
 * for both, and Vulkan's where there is no driver to say */
static enum ffx_api video_upscaler_ffx_api(void)
{
   const char *ident = video_driver_get_ident();
   if (string_is_equal(ident, "d3d12"))
      return FFX_API_D3D12;
   return FFX_API_VULKAN;
}

static uint32_t upscaler_dlss_version;
static char upscaler_dlss_version_dir[DIR_MAX_LENGTH];
/* UPSCALER_FLAG_* */
static unsigned upscaler_flags;
#define UPSCALER_FLAG_WANTED     0x1u
#define UPSCALER_FLAG_OPTISCALER 0x2u

uint32_t video_upscaler_dlss_version(void)
{
   char dir[DIR_MAX_LENGTH];
   video_upscaler_dir(VIDEO_TEMPORAL_UPSCALER_DLSS, dir, sizeof(dir));
   if (strcmp(dir, upscaler_dlss_version_dir))
   {
      strlcpy(upscaler_dlss_version_dir, dir, sizeof(upscaler_dlss_version_dir));
      upscaler_dlss_version = ngx_dlss_feature_version(dir);
   }
   return upscaler_dlss_version;
}

uint32_t video_upscaler_preset_min_version(enum video_temporal_upscaler_preset preset)
{
   /* A to G: the CNN models of DLSS 2 and 3; H to K came with the
    * transformer models of 310; L to O later still */
   if (preset >= VIDEO_TEMPORAL_UPSCALER_PRESET_L)
      return NGX_VERSION_PACK(310, 4, 0);
   if (preset >= VIDEO_TEMPORAL_UPSCALER_PRESET_H)
      return NGX_VERSION_PACK(310, 0, 0);
   return 0;
}

static uint32_t upscaler_dlssnr_version;
static bool upscaler_dlssnr_found;
static char upscaler_dlssnr_dir[DIR_MAX_LENGTH];
/* What a session's NGX said of Neural Rendering: UPSCALER_NR_* */
static retro_atomic_int_t upscaler_nr_state;
#define UPSCALER_NR_UNKNOWN     0
#define UPSCALER_NR_SUPPORTED   1
#define UPSCALER_NR_UNSUPPORTED 2

static void video_upscaler_dlssnr_probe(void)
{
   char dir[DIR_MAX_LENGTH];
   video_upscaler_dir(VIDEO_TEMPORAL_UPSCALER_DLSS, dir, sizeof(dir));
   if (strcmp(dir, upscaler_dlssnr_dir))
   {
      strlcpy(upscaler_dlssnr_dir, dir, sizeof(upscaler_dlssnr_dir));
      upscaler_dlssnr_found   = ngx_dlssnr_feature_in(dir);
      upscaler_dlssnr_version = upscaler_dlssnr_found
         ? ngx_dlssnr_feature_version(dir) : 0;
   }
}

uint32_t video_upscaler_dlssnr_version(void)
{
   video_upscaler_dlssnr_probe();
   return upscaler_dlssnr_version;
}

bool video_upscaler_nr_available(void)
{
   video_upscaler_dlssnr_probe();
   return upscaler_dlssnr_found
      && retro_atomic_load_relaxed_int(&upscaler_nr_state) != UPSCALER_NR_UNSUPPORTED
      && !video_upscaler_optiscaler();
}

void video_upscaler_note_nr(bool supported)
{
   retro_atomic_store_relaxed_int(&upscaler_nr_state,
         supported ? UPSCALER_NR_SUPPORTED : UPSCALER_NR_UNSUPPORTED);
}

unsigned video_upscaler_nr_mode(void)
{
   settings_t *settings = config_get_ptr();
   unsigned preset, style, intensity, structure, tone, skin;
   int skin_set;
   if (!settings->bools.video_dlss_nr_enable)
      return 0;
   preset    = settings->uints.video_dlss_nr_preset;
   style     = settings->uints.video_dlss_nr_style;
   intensity = settings->uints.video_dlss_nr_intensity / 5;
   structure = settings->uints.video_dlss_nr_local_structure / 5;
   tone      = settings->uints.video_dlss_nr_local_tone / 5;
   skin_set  = settings->ints.video_dlss_nr_skin_structure;
   skin      = skin_set < 0 ? 0 : 1 + (unsigned)skin_set / 5;
   if (preset > 3)
      preset = 0;
   if (style > 2)
      style = 0;
   if (intensity > 40)
      intensity = 40;
   if (structure > 40)
      structure = 40;
   if (tone > 40)
      tone = 40;
   if (skin > 41)
      skin = 41;
   return VIDEO_UPSCALER_NR_PACK(VIDEO_UPSCALER_NR_ON
         | (settings->bools.video_dlss_nr_auto_mask ? VIDEO_UPSCALER_NR_AUTO_MASK : 0),
         preset, style, intensity, structure, tone, skin);
}

bool video_upscaler_optiscaler(void)
{
   char dir[DIR_MAX_LENGTH];
   if (     (upscaler_flags & UPSCALER_FLAG_OPTISCALER)
         || (ngx_lib_present() & NGX_PRESENT_OPTISCALER))
      return true;
   video_upscaler_dir(VIDEO_TEMPORAL_UPSCALER_DLSS, dir, sizeof(dir));
   return ngx_stand_in_in(dir);
}

void video_upscaler_note_optiscaler(void)
{
   upscaler_flags |= UPSCALER_FLAG_OPTISCALER;
}

bool video_upscaler_wanted(void)
{
   return (upscaler_flags & UPSCALER_FLAG_WANTED) != 0;
}

void video_upscaler_note_wanted(void)
{
   char dir[DIR_MAX_LENGTH];
   if (upscaler_flags & UPSCALER_FLAG_WANTED)
      return;
   upscaler_flags |= UPSCALER_FLAG_WANTED;
   if (config_get_ptr()->uints.video_temporal_upscaler != VIDEO_TEMPORAL_UPSCALER_DLSS)
      return;
   /* ahead of NGX, which it then hooks */
   video_upscaler_dir(VIDEO_TEMPORAL_UPSCALER_DLSS, dir, sizeof(dir));
   if (ngx_lib_load_stand_in(dir))
      RARCH_LOG("[Video] Temporal Upscaler: OptiScaler loaded from \"%s\".\n", dir);
}

void video_upscaler_clear_wanted(void)
{
   upscaler_flags &= ~UPSCALER_FLAG_WANTED;
}

unsigned video_upscaler_probe(enum video_temporal_upscaler kind)
{
   char dir[DIR_MAX_LENGTH];
   unsigned found = 0;
   video_upscaler_dir(kind, dir, sizeof(dir));
   switch (kind)
   {
      case VIDEO_TEMPORAL_UPSCALER_DLSS:
         if (ngx_lib_present() & NGX_PRESENT_FOUND)
            found |= VIDEO_UPSCALER_FOUND_LIBRARY;
         if (ngx_dlss_feature_in(dir))
            found |= VIDEO_UPSCALER_FOUND_RUNTIME;
         /* (a probe reads the runtime's version again: it may have
          * been put there since) */
         upscaler_dlss_version_dir[0] = '\0';
         upscaler_dlssnr_dir[0]       = '\0';
         break;
      case VIDEO_TEMPORAL_UPSCALER_FSR:
         if (ffx_runtime_in(FFX_API_D3D12, dir) || ffx_runtime_in(FFX_API_VULKAN, dir))
            found |= VIDEO_UPSCALER_FOUND_RUNTIME;
         if (ffx_lib_get(video_upscaler_ffx_api(), dir))
            found |= VIDEO_UPSCALER_FOUND_LIBRARY;
         break;
      case VIDEO_TEMPORAL_UPSCALER_XESS:
         if (xess_runtime_in(dir))
            found |= VIDEO_UPSCALER_FOUND_RUNTIME;
         if (xess_lib_get(dir))
            found |= VIDEO_UPSCALER_FOUND_LIBRARY;
         break;
      default:
         break;
   }
   return found;
}

size_t video_upscaler_status_string(char *s, size_t len)
{
   unsigned want  = config_get_ptr()->uints.video_temporal_upscaler;
   unsigned found;
   size_t _len;
   if (want >= VIDEO_TEMPORAL_UPSCALER_LAST)
      want = VIDEO_TEMPORAL_UPSCALER_OFF;
   _len  = strlcpy(s, video_upscaler_name((enum video_temporal_upscaler)want), len);
   switch (want)
   {
      case VIDEO_TEMPORAL_UPSCALER_DLSS:
      {
         uint32_t version = video_upscaler_dlss_version();
         found = video_upscaler_probe(VIDEO_TEMPORAL_UPSCALER_DLSS);
         _len += strlcpy(s + _len, (found & VIDEO_UPSCALER_FOUND_LIBRARY)
               ? " (NGX: found" : " (NGX: not found", len - _len);
         if (version)
            _len += snprintf(s + _len, len - _len, ", DLSS runtime %u.%u.%u",
                  (unsigned)NGX_VERSION_MAJOR(version), (unsigned)NGX_VERSION_MINOR(version),
                  (unsigned)NGX_VERSION_PATCH(version));
         else
            _len += strlcpy(s + _len, (found & VIDEO_UPSCALER_FOUND_RUNTIME)
                  ? ", DLSS runtime: found" : ", DLSS runtime: not in DLSS Directory",
                  len - _len);
         video_upscaler_dlssnr_probe();
         if (video_upscaler_optiscaler())
            _len += strlcpy(s + _len, ", via OptiScaler", len - _len);
         else if (upscaler_dlssnr_found)
         {
            uint32_t nr = upscaler_dlssnr_version;
            if (retro_atomic_load_relaxed_int(&upscaler_nr_state) == UPSCALER_NR_UNSUPPORTED)
               _len += strlcpy(s + _len, ", Neural Rendering: needs a newer driver",
                     len - _len);
            else if (nr)
               _len += snprintf(s + _len, len - _len, ", Neural Rendering %u.%u.%u",
                     (unsigned)NGX_VERSION_MAJOR(nr), (unsigned)NGX_VERSION_MINOR(nr),
                     (unsigned)NGX_VERSION_PATCH(nr));
            else
               _len += strlcpy(s + _len, ", Neural Rendering: found", len - _len);
         }
         _len += strlcpy(s + _len, ")", len - _len);
         break;
      }
      case VIDEO_TEMPORAL_UPSCALER_FSR:
         found = video_upscaler_probe(VIDEO_TEMPORAL_UPSCALER_FSR);
         _len += strlcpy(s + _len, (found & VIDEO_UPSCALER_FOUND_RUNTIME)
               ? " (FSR library: found)" : " (FSR library: not in FSR Directory)",
               len - _len);
         break;
      case VIDEO_TEMPORAL_UPSCALER_XESS:
         found = video_upscaler_probe(VIDEO_TEMPORAL_UPSCALER_XESS);
         _len += strlcpy(s + _len, (found & VIDEO_UPSCALER_FOUND_RUNTIME)
               ? " (XeSS library: found)" : " (XeSS library: not in XeSS Directory)",
               len - _len);
         break;
      default:
         break;
   }
   return _len;
}

void video_upscaler_log_probe(enum video_temporal_upscaler kind)
{
   char dir[DIR_MAX_LENGTH];
   if (!video_upscaler_wanted())
   {
      RARCH_LOG("[Video] Temporal Upscaler: not loaded, the core does not ask for it.\n");
      return;
   }
   video_upscaler_dir(kind, dir, sizeof(dir));
   switch (kind)
   {
      case VIDEO_TEMPORAL_UPSCALER_DLSS:
      {
         uint32_t version = video_upscaler_dlss_version();
         ngx_lib_get();
         RARCH_LOG("[Video] Temporal Upscaler: %s.\n", ngx_lib_status());
         if (version)
            RARCH_LOG("[Video] Temporal Upscaler: DLSS runtime %u.%u.%u in \"%s\".\n",
                  (unsigned)NGX_VERSION_MAJOR(version), (unsigned)NGX_VERSION_MINOR(version),
                  (unsigned)NGX_VERSION_PATCH(version), dir);
         else
            RARCH_LOG("[Video] Temporal Upscaler: DLSS runtime (nvngx_dlss.dll / libnvidia-ngx-dlss.so) %s in \"%s\".\n",
                  ngx_dlss_feature_in(dir) ? "found" : "NOT found", dir);
         if (ngx_dlssnr_feature_in(dir))
         {
            uint32_t nr = video_upscaler_dlssnr_version();
            RARCH_LOG("[Video] Temporal Upscaler: DLSS Neural Rendering runtime %u.%u.%u in \"%s\".\n",
                  (unsigned)NGX_VERSION_MAJOR(nr), (unsigned)NGX_VERSION_MINOR(nr),
                  (unsigned)NGX_VERSION_PATCH(nr), dir);
         }
         break;
      }
      case VIDEO_TEMPORAL_UPSCALER_FSR:
      {
         enum ffx_api api = video_upscaler_ffx_api();
         ffx_lib_get(api, dir);
         RARCH_LOG("[Video] Temporal Upscaler: %s.\n", ffx_lib_status(api));
         break;
      }
      case VIDEO_TEMPORAL_UPSCALER_XESS:
         xess_lib_get(dir);
         RARCH_LOG("[Video] Temporal Upscaler: %s.\n", xess_lib_status());
         break;
      default:
         break;
   }
}

static const video_upscaler_backend_t *upscaler_backend;
static const struct retro_hw_upscaler_inputs *upscaler_inputs;
/* Under the threaded video wrapper the driver takes on the video
 * thread what the wrapper carried there for the frame, never the
 * pointer the core set on the main thread for a newer one */
static const struct retro_hw_upscaler_inputs *upscaler_carried;
static bool upscaler_carrying;
static unsigned upscaler_out_dims;

static unsigned video_upscaler_quality(unsigned out_dims)
{
   unsigned q = config_get_ptr()->uints.video_temporal_upscaler_quality;
   if (q > VIDEO_TEMPORAL_UPSCALER_QUALITY_AUTO && q < VIDEO_TEMPORAL_UPSCALER_QUALITY_LAST)
      return q;
   /* Auto: the sharper mode up to 1440 lines, Balanced from there */
   return VIDEO_SCALE_H(out_dims) > 1440
      ? VIDEO_TEMPORAL_UPSCALER_QUALITY_BALANCED : VIDEO_TEMPORAL_UPSCALER_QUALITY_QUALITY;
}

float video_upscaler_frame_time_ms(const struct retro_hw_upscaler_inputs *inputs)
{
   float hz;
   if (inputs->frame_time_ms > 0.0f)
      return inputs->frame_time_ms;
   hz = video_driver_get_core_hz();
   if (hz > 0.0f)
      return 1000.0f / hz;
   return 1000.0f / 60.0f;
}

unsigned video_upscaler_mode(unsigned out_dims)
{
   settings_t *settings = config_get_ptr();
   unsigned preset    = settings->uints.video_temporal_upscaler_preset;
   unsigned sharpness = settings->uints.video_temporal_upscaler_sharpness;
   if (preset >= VIDEO_TEMPORAL_UPSCALER_PRESET_LAST)
      preset = VIDEO_TEMPORAL_UPSCALER_PRESET_DEFAULT;
   if (sharpness > 100)
      sharpness = 100;
   return VIDEO_UPSCALER_MODE_PACK_SHARP(video_upscaler_quality(out_dims),
         preset, sharpness);
}

unsigned video_upscaler_mode_ngx(unsigned mode)
{
   unsigned quality = NGX_PERF_QUALITY_MAX_QUALITY;
   /* (the preset is numbered as NGX numbers it) */
   unsigned preset  = VIDEO_UPSCALER_MODE_PRESET(mode);
   switch (VIDEO_UPSCALER_MODE_QUALITY(mode))
   {
      case VIDEO_TEMPORAL_UPSCALER_QUALITY_NATIVE:
         quality = NGX_PERF_QUALITY_DLAA;
         break;
      case VIDEO_TEMPORAL_UPSCALER_QUALITY_BALANCED:
         quality = NGX_PERF_QUALITY_BALANCED;
         break;
      case VIDEO_TEMPORAL_UPSCALER_QUALITY_PERFORMANCE:
         quality = NGX_PERF_QUALITY_MAX_PERF;
         break;
      case VIDEO_TEMPORAL_UPSCALER_QUALITY_ULTRA_PERFORMANCE:
         quality = NGX_PERF_QUALITY_ULTRA_PERFORMANCE;
         break;
      default:
         break;
   }
   if (preset >= NGX_DLSS_PRESET_LAST)
      preset = NGX_DLSS_PRESET_DEFAULT;
   return NGX_MODE_PACK_SHARP(quality, preset, VIDEO_UPSCALER_MODE_SHARPNESS(mode));
}

unsigned video_upscaler_mode_ffx(unsigned mode)
{
   unsigned quality = FFX_UPSCALE_MODE_QUALITY;
   switch (VIDEO_UPSCALER_MODE_QUALITY(mode))
   {
      case VIDEO_TEMPORAL_UPSCALER_QUALITY_NATIVE:
         quality = FFX_UPSCALE_MODE_NATIVEAA;
         break;
      case VIDEO_TEMPORAL_UPSCALER_QUALITY_BALANCED:
         quality = FFX_UPSCALE_MODE_BALANCED;
         break;
      case VIDEO_TEMPORAL_UPSCALER_QUALITY_PERFORMANCE:
         quality = FFX_UPSCALE_MODE_PERFORMANCE;
         break;
      case VIDEO_TEMPORAL_UPSCALER_QUALITY_ULTRA_PERFORMANCE:
         quality = FFX_UPSCALE_MODE_ULTRA_PERFORMANCE;
         break;
      default:
         break;
   }
   return quality | (VIDEO_UPSCALER_MODE_SHARPNESS(mode) << 16);
}

unsigned video_upscaler_mode_xess(unsigned mode)
{
   switch (VIDEO_UPSCALER_MODE_QUALITY(mode))
   {
      case VIDEO_TEMPORAL_UPSCALER_QUALITY_NATIVE:
         return XESS_QUALITY_AA;
      case VIDEO_TEMPORAL_UPSCALER_QUALITY_BALANCED:
         return XESS_QUALITY_BALANCED;
      case VIDEO_TEMPORAL_UPSCALER_QUALITY_PERFORMANCE:
         return XESS_QUALITY_PERFORMANCE;
      case VIDEO_TEMPORAL_UPSCALER_QUALITY_ULTRA_PERFORMANCE:
         return XESS_QUALITY_ULTRA_PERFORMANCE;
      default:
         break;
   }
   return XESS_QUALITY_QUALITY;
}

void video_upscaler_attach(const video_upscaler_backend_t *backend)
{
   upscaler_backend = backend;
}

void video_upscaler_detach(void)
{
   upscaler_backend = NULL;
   upscaler_inputs  = NULL;
   upscaler_carried = NULL;
   upscaler_carrying = false;
}

void video_upscaler_set_output(unsigned dims)
{
   upscaler_out_dims = dims;
}

static bool video_upscaler_get_request(void *handle,
      struct retro_hw_upscaler_request *request)
{
   unsigned mode;
   unsigned render_dims, min_dims, max_dims;
   const video_upscaler_backend_t *backend = upscaler_backend;
   (void)handle;
   if (!backend || !VIDEO_SCALE_W(upscaler_out_dims)
         || !VIDEO_SCALE_H(upscaler_out_dims)
         || !config_get_ptr()->uints.video_temporal_upscaler)
      return false;
   mode = video_upscaler_mode(upscaler_out_dims);
   if (!backend->optimal(backend->data, mode, upscaler_out_dims,
            &render_dims, &min_dims, &max_dims))
      return false;
   request->output_width      = VIDEO_SCALE_W(upscaler_out_dims);
   request->output_height     = VIDEO_SCALE_H(upscaler_out_dims);
   request->render_width      = VIDEO_SCALE_W(render_dims);
   request->render_height     = VIDEO_SCALE_H(render_dims);
   request->min_render_width  = VIDEO_SCALE_W(min_dims);
   request->min_render_height = VIDEO_SCALE_H(min_dims);
   request->max_render_width  = VIDEO_SCALE_W(max_dims);
   request->max_render_height = VIDEO_SCALE_H(max_dims);
   request->flags             = 0;
   request->mip_bias          = (float)(log((double)request->render_width
            / (double)request->output_width) / log(2.0)) - 1.0f;
   return true;
}

static void video_upscaler_set_inputs(void *handle,
      const struct retro_hw_upscaler_inputs *inputs)
{
   (void)handle;
   upscaler_inputs = inputs;
}

const struct retro_hw_upscaler_inputs *video_upscaler_take_inputs(void)
{
   const struct retro_hw_upscaler_inputs *inputs;
   if (upscaler_carrying)
   {
      upscaler_carrying = false;
      inputs            = upscaler_carried;
      upscaler_carried  = NULL;
      return inputs;
   }
   inputs          = upscaler_inputs;
   upscaler_inputs = NULL;
   return inputs;
}

const struct retro_hw_upscaler_inputs *video_upscaler_take_set_inputs(void)
{
   const struct retro_hw_upscaler_inputs *inputs = upscaler_inputs;
   upscaler_inputs = NULL;
   return inputs;
}

void video_upscaler_carry_inputs(const struct retro_hw_upscaler_inputs *inputs)
{
   upscaler_carried  = inputs;
   upscaler_carrying = true;
}

void video_upscaler_drop_carry(void)
{
   upscaler_carried  = NULL;
   upscaler_carrying = false;
}

static const struct retro_hw_render_upscaler_interface upscaler_iface = {
   RETRO_HW_RENDER_UPSCALER_INTERFACE_VERSION,
   NULL,
   video_upscaler_get_request,
   video_upscaler_set_inputs
};

const struct retro_hw_render_upscaler_interface *video_upscaler_get_interface(void)
{
   if (!upscaler_backend)
      return NULL;
   return &upscaler_iface;
}
