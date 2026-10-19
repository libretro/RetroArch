/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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
#include <string.h>
#include <string/stdstring.h>

#if defined(_WIN32) && !defined(__WINRT__) && !defined(_XBOX)
#include <windows.h>
#endif

#include "../../configuration.h"
#include "../../input/input_driver.h"
#include "../../verbosity.h"

#include "d3d_common.h"
#include "../video_driver.h"

#define D3D_TEXTURE_FILTER_LINEAR 2
#define D3D_TEXTURE_FILTER_POINT  1

int32_t d3d_translate_filter(unsigned type)
{
   switch (type)
   {
      case RARCH_FILTER_UNSPEC:
         {
            settings_t *settings = config_get_ptr();
            bool video_smooth    = settings->bools.video_smooth;
            if (!video_smooth)
               break;
         }
         /* fall-through */
      case RARCH_FILTER_LINEAR:
         return (int32_t)D3D_TEXTURE_FILTER_LINEAR;
      case RARCH_FILTER_NEAREST:
         break;
   }

   return (int32_t)D3D_TEXTURE_FILTER_POINT;
}


#if defined(_WIN32) && !defined(__WINRT__) && !defined(_XBOX)

/* NVAPI, by its published interface ids (nvapi_interface.h, MIT):
 * resolved once for the process through nvapi_QueryInterface, never
 * linked, so a machine without the driver's nvapi loses nothing. The
 * structs are the header's, declared here so no SDK is needed; their
 * version word is sizeof | (revision << 16), as MAKE_NVAPI_VERSION
 * makes it. */
#define NVAPI_ID_INITIALIZE          0x0150e828
#define NVAPI_ID_D3D_SET_SLEEP_MODE  0xac1ca9e0
#define NVAPI_ID_D3D_SLEEP           0x852cd1d2
#define NVAPI_ID_D3D_SET_LATENCY_MARKER 0xd9984c05

#pragma pack(push, 8)
typedef struct
{
   uint32_t version;
   uint8_t  low_latency_mode;
   uint8_t  low_latency_boost;
   uint32_t minimum_interval_us;
   uint8_t  use_markers_to_optimize;
   uint8_t  use_min_queue_time;
   uint8_t  rsvd[30];
} nvapi_sleep_mode_params_t;

typedef struct
{
   uint32_t version;
   uint64_t frame_id;
   uint32_t marker_type;
   uint64_t rsvd0;
   uint8_t  rsvd[56];
} nvapi_latency_marker_params_t;
#pragma pack(pop)

#define NVAPI_SLEEP_MODE_PARAMS_VER \
   ((uint32_t)(sizeof(nvapi_sleep_mode_params_t) | (1u << 16)))
#define NVAPI_LATENCY_MARKER_PARAMS_VER \
   ((uint32_t)(sizeof(nvapi_latency_marker_params_t) | (1u << 16)))

typedef void  (*nvapi_proc_t)(void);
typedef nvapi_proc_t (__cdecl *nvapi_query_interface_t)(unsigned int id);
typedef int   (__cdecl *nvapi_initialize_t)(void);
typedef int   (__cdecl *nvapi_set_sleep_mode_t)(void *device,
      nvapi_sleep_mode_params_t *params);
typedef int   (__cdecl *nvapi_sleep_t)(void *device);
typedef int   (__cdecl *nvapi_set_latency_marker_t)(void *device,
      nvapi_latency_marker_params_t *params);

static nvapi_set_sleep_mode_t     nvapi_set_sleep_mode;
static nvapi_sleep_t              nvapi_sleep;
static nvapi_set_latency_marker_t nvapi_set_latency_marker;
static bool                       nvapi_probed;

static bool d3d_driver_pacing_probe(void)
{
   HMODULE dll;
   nvapi_query_interface_t query;
   nvapi_initialize_t init;
   if (nvapi_probed)
      return nvapi_set_sleep_mode != NULL;
   nvapi_probed = true;
#ifdef _WIN64
   dll   = LoadLibraryA("nvapi64.dll");
#else
   dll   = LoadLibraryA("nvapi.dll");
#endif
   if (!dll)
      return false;
   query = (nvapi_query_interface_t)GetProcAddress(dll, "nvapi_QueryInterface");
   if (!query)
      return false;
   init  = (nvapi_initialize_t)query(NVAPI_ID_INITIALIZE);
   if (!init || init() != 0)
      return false;
   nvapi_set_sleep_mode     = (nvapi_set_sleep_mode_t)
      query(NVAPI_ID_D3D_SET_SLEEP_MODE);
   nvapi_sleep              = (nvapi_sleep_t)query(NVAPI_ID_D3D_SLEEP);
   nvapi_set_latency_marker = (nvapi_set_latency_marker_t)
      query(NVAPI_ID_D3D_SET_LATENCY_MARKER);
   if (!nvapi_set_sleep_mode || !nvapi_sleep || !nvapi_set_latency_marker)
   {
      nvapi_set_sleep_mode = NULL;
      return false;
   }
   RARCH_LOG("[D3D] Driver Pacing available through NVAPI.\n");
   return true;
}

static bool d3d_driver_pacing_apply(d3d_driver_pacing_t *s, void *device,
      unsigned mode, unsigned min_interval_us)
{
   nvapi_sleep_mode_params_t params;
   memset(&params, 0, sizeof(params));
   params.version                 = NVAPI_SLEEP_MODE_PARAMS_VER;
   params.low_latency_mode        = mode ? 1 : 0;
   params.low_latency_boost       = (mode == VIDEO_DRIVER_PACING_BOOST) ? 1 : 0;
   params.minimum_interval_us     = mode ? min_interval_us : 0;
   params.use_markers_to_optimize = mode ? 1 : 0;
   if (nvapi_set_sleep_mode(device, &params) != 0)
   {
      s->failed = true;
      s->mode   = 0;
      s->device = NULL;
      return false;
   }
   s->mode            = (uint8_t)mode;
   s->min_interval_us = min_interval_us;
   s->device          = mode ? device : NULL;
   return true;
}

unsigned d3d_driver_pacing_set(d3d_driver_pacing_t *s, void *device,
      unsigned mode)
{
   if (s->failed)
      return VIDEO_DRIVER_PACING_REASON_FAILED;
   if (!device)
      return VIDEO_DRIVER_PACING_REASON_SWAPCHAIN;
   if (s->device != device)
   {
      s->device = NULL;
      s->mode   = 0;
   }
   if (!mode)
   {
      if (s->mode && !d3d_driver_pacing_apply(s, device, 0, 0))
         return VIDEO_DRIVER_PACING_REASON_FAILED;
      return VIDEO_DRIVER_PACING_REASON_OFF;
   }
   if (!d3d_driver_pacing_probe())
      return VIDEO_DRIVER_PACING_REASON_DEVICE;
   if (mode == s->mode)
      return VIDEO_DRIVER_PACING_REASON_NONE;
   if (!d3d_driver_pacing_apply(s, device, mode, s->min_interval_us))
      return VIDEO_DRIVER_PACING_REASON_FAILED;
   return VIDEO_DRIVER_PACING_REASON_NONE;
}

uint64_t d3d_driver_pacing_wait(d3d_driver_pacing_t *s, void *device,
      unsigned min_interval_us, unsigned max_wait_us)
{
   /* NvAPI_D3D_Sleep holds the thread itself, for as long as the
    * driver's own schedule says and no longer; it takes no bound. */
   (void)max_wait_us;
   if (!s->mode || s->failed || s->device != device)
      return 0;
   if (     min_interval_us != s->min_interval_us
         && !d3d_driver_pacing_apply(s, device, s->mode, min_interval_us))
      return 0;
   if (nvapi_sleep(device) != 0)
   {
      s->failed = true;
      return 0;
   }
   s->stage = 0;
   return ++s->frame_id;
}

void d3d_driver_pacing_mark(d3d_driver_pacing_t *s, void *device,
      uint64_t frame_id, unsigned marker)
{
   nvapi_latency_marker_params_t params;
   if (!s->mode || s->failed || s->device != device || !frame_id)
      return;
   memset(&params, 0, sizeof(params));
   params.version     = NVAPI_LATENCY_MARKER_PARAMS_VER;
   params.frame_id    = frame_id;
   /* NV_LATENCY_MARKER_TYPE matches VkLatencyMarkerNV through
    * INPUT_SAMPLE, which is as far as the pokes go */
   params.marker_type = marker;
   nvapi_set_latency_marker(device, &params);
}

void d3d_driver_pacing_render_begin(d3d_driver_pacing_t *s, void *device)
{
   if (s->stage != 0 || !s->mode)
      return;
   s->stage = 1;
   d3d_driver_pacing_mark(s, device, s->frame_id, 1); /* SIMULATION_END */
   d3d_driver_pacing_mark(s, device, s->frame_id, 2); /* RENDERSUBMIT_START */
}

void d3d_driver_pacing_present_begin(d3d_driver_pacing_t *s, void *device)
{
   if (s->stage != 1)
      return;
   s->stage = 2;
   d3d_driver_pacing_mark(s, device, s->frame_id, 3); /* RENDERSUBMIT_END */
   d3d_driver_pacing_mark(s, device, s->frame_id, 4); /* PRESENT_START */
}

void d3d_driver_pacing_present_end(d3d_driver_pacing_t *s, void *device)
{
   if (s->stage != 2)
      return;
   s->stage = 3;
   d3d_driver_pacing_mark(s, device, s->frame_id, 5); /* PRESENT_END */
}

void d3d_driver_pacing_free(d3d_driver_pacing_t *s, void *device)
{
   if (s->mode && !s->failed && s->device == device && device)
      d3d_driver_pacing_apply(s, device, 0, 0);
   s->mode   = 0;
   s->device = NULL;
}

#else

unsigned d3d_driver_pacing_set(d3d_driver_pacing_t *s, void *device,
      unsigned mode)
{
   (void)s; (void)device; (void)mode;
   return VIDEO_DRIVER_PACING_REASON_DEVICE;
}
uint64_t d3d_driver_pacing_wait(d3d_driver_pacing_t *s, void *device,
      unsigned min_interval_us, unsigned max_wait_us)
{
   (void)s; (void)device; (void)min_interval_us; (void)max_wait_us;
   return 0;
}
void d3d_driver_pacing_mark(d3d_driver_pacing_t *s, void *device,
      uint64_t frame_id, unsigned marker)
{
   (void)s; (void)device; (void)frame_id; (void)marker;
}
void d3d_driver_pacing_render_begin(d3d_driver_pacing_t *s, void *device)
{
   (void)s; (void)device;
}
void d3d_driver_pacing_present_begin(d3d_driver_pacing_t *s, void *device)
{
   (void)s; (void)device;
}
void d3d_driver_pacing_present_end(d3d_driver_pacing_t *s, void *device)
{
   (void)s; (void)device;
}
void d3d_driver_pacing_free(d3d_driver_pacing_t *s, void *device)
{
   (void)s; (void)device;
}

#endif
