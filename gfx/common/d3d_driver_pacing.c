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
#include <string.h>

#include "../../verbosity.h"
#include "../video_driver.h"

#include "d3d_driver_pacing.h"

#if defined(_WIN32) && !defined(__WINRT__) && !defined(_XBOX)
#include <gfx/nvidia/nvapi_loader.h>
#endif

#if defined(_WIN32) && !defined(__WINRT__) && !defined(_XBOX)

/* NVAPI's Reflex entry points by their published interface ids
 * (nvapi_interface.h, MIT), through libretro-common's loader. The
 * structs are the header's, declared here so no SDK is needed; their
 * version word is sizeof | (revision << 16), as MAKE_NVAPI_VERSION
 * makes it. */
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
   if (nvapi_probed)
      return nvapi_set_sleep_mode != NULL;
   nvapi_probed = true;
   nvapi_set_sleep_mode     = (nvapi_set_sleep_mode_t)
      nvapi_query(NVAPI_ID_D3D_SET_SLEEP_MODE);
   nvapi_sleep              = (nvapi_sleep_t)nvapi_query(NVAPI_ID_D3D_SLEEP);
   nvapi_set_latency_marker = (nvapi_set_latency_marker_t)
      nvapi_query(NVAPI_ID_D3D_SET_LATENCY_MARKER);
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
      retro_atomic_store_release_int(&s->mode_pub, 0);
      return false;
   }
   s->mode            = (uint8_t)mode;
   s->min_interval_us = min_interval_us;
   s->device          = mode ? device : NULL;
   retro_atomic_store_release_int(&s->mode_pub, (int)mode);
   return true;
}

unsigned d3d_driver_pacing_set(d3d_driver_pacing_t *s, void *device,
      unsigned mode)
{
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
      /* Asked for again is the way back from a failure */
      s->failed = false;
      return VIDEO_DRIVER_PACING_REASON_OFF;
   }
   if (s->failed)
      return VIDEO_DRIVER_PACING_REASON_FAILED;
   if (!d3d_driver_pacing_probe())
      return VIDEO_DRIVER_PACING_REASON_DEVICE;
   if (mode == s->mode)
      return VIDEO_DRIVER_PACING_REASON_NONE;
   if (!d3d_driver_pacing_apply(s, device, mode, s->min_interval_us))
      return VIDEO_DRIVER_PACING_REASON_FAILED;
   return VIDEO_DRIVER_PACING_REASON_NONE;
}

bool d3d_driver_pacing_wait(d3d_driver_pacing_t *s, void *device,
      uint64_t frame_id, unsigned max_wait_us)
{
   /* NvAPI_D3D_Sleep holds the thread itself, for as long as the
    * driver's own schedule says and no longer; it takes no bound and
    * names no frame. */
   (void)frame_id;
   (void)max_wait_us;
   if (!s->mode || s->failed || s->device != device)
      return false;
   if (nvapi_sleep(device) != 0)
   {
      s->failed = true;
      return false;
   }
   return true;
}

static void d3d_driver_pacing_marker(void *device, uint64_t frame_id,
      unsigned marker)
{
   nvapi_latency_marker_params_t params;
   memset(&params, 0, sizeof(params));
   params.version     = NVAPI_LATENCY_MARKER_PARAMS_VER;
   params.frame_id    = frame_id;
   params.marker_type = marker;
   nvapi_set_latency_marker(device, &params);
}

void d3d_driver_pacing_mark(d3d_driver_pacing_t *s, void *device,
      uint64_t frame_id, unsigned marker)
{
   if (!s->mode || s->failed || s->device != device || !frame_id)
      return;
   /* NV_LATENCY_MARKER_TYPE matches VkLatencyMarkerNV through
    * INPUT_SAMPLE, which is as far as the pokes go */
   d3d_driver_pacing_marker(device, frame_id, marker);
}

/* The presenter's view of the mode: what the frame loop last put on
 * this device. */
static bool d3d_driver_pacing_presenting(d3d_driver_pacing_t *s,
      void *device)
{
   return retro_atomic_load_acquire_int(&s->mode_pub) != 0
      && s->frame_id != 0 && device != NULL;
}

void d3d_driver_pacing_frame_begin(d3d_driver_pacing_t *s, void *device,
      uint64_t frame_id, unsigned min_interval_us)
{
   if (!retro_atomic_load_acquire_int(&s->mode_pub) || !device)
      return;
   if (!frame_id || frame_id == s->frame_id)
      return;
   /* The interval is a sleep-mode parameter: set from here, where
    * the frame that wants it arrives, which NVAPI allows from any
    * thread. */
   if (min_interval_us != s->min_interval_us)
   {
      nvapi_sleep_mode_params_t params;
      memset(&params, 0, sizeof(params));
      params.version                 = NVAPI_SLEEP_MODE_PARAMS_VER;
      params.low_latency_mode        = 1;
      params.low_latency_boost       =
         (retro_atomic_load_acquire_int(&s->mode_pub) == VIDEO_DRIVER_PACING_BOOST) ? 1 : 0;
      params.minimum_interval_us     = min_interval_us;
      params.use_markers_to_optimize = 1;
      if (nvapi_set_sleep_mode(device, &params) == 0)
         s->min_interval_us = min_interval_us;
   }
   s->frame_id = frame_id;
   s->stage    = 1;
   d3d_driver_pacing_marker(device, frame_id, 1); /* SIMULATION_END */
   d3d_driver_pacing_marker(device, frame_id, 2); /* RENDERSUBMIT_START */
}

void d3d_driver_pacing_present_begin(d3d_driver_pacing_t *s, void *device)
{
   if (!d3d_driver_pacing_presenting(s, device))
      return;
   if (s->stage == 1)
   {
      d3d_driver_pacing_marker(device, s->frame_id, 3);  /* RENDERSUBMIT_END */
      d3d_driver_pacing_marker(device, s->frame_id, 4);  /* PRESENT_START */
   }
   else if (s->stage == 2)
      d3d_driver_pacing_marker(device, s->frame_id, 11); /* OUT_OF_BAND_PRESENT_START */
}

void d3d_driver_pacing_present_end(d3d_driver_pacing_t *s, void *device)
{
   if (!d3d_driver_pacing_presenting(s, device))
      return;
   if (s->stage == 1)
   {
      s->stage = 2;
      d3d_driver_pacing_marker(device, s->frame_id, 5);  /* PRESENT_END */
   }
   else if (s->stage == 2)
      d3d_driver_pacing_marker(device, s->frame_id, 12); /* OUT_OF_BAND_PRESENT_END */
}

bool d3d_driver_pacing_paced(d3d_driver_pacing_t *s, uint64_t frame_id)
{
   return frame_id != 0
      && retro_atomic_load_acquire_int(&s->mode_pub) != 0;
}

bool d3d_driver_pacing_sleep_on(void *device)
{
   return device && nvapi_sleep && nvapi_sleep(device) == 0;
}

void d3d_driver_pacing_mark_on(void *device, uint64_t frame_id,
      unsigned marker)
{
   if (device && nvapi_set_latency_marker && frame_id)
      d3d_driver_pacing_marker(device, frame_id, marker);
}

void d3d_driver_pacing_free(d3d_driver_pacing_t *s, void *device)
{
   if (s->mode && !s->failed && s->device == device && device)
      d3d_driver_pacing_apply(s, device, 0, 0);
   s->mode   = 0;
   s->device = NULL;
   retro_atomic_store_release_int(&s->mode_pub, 0);
}

#else

unsigned d3d_driver_pacing_set(d3d_driver_pacing_t *s, void *device,
      unsigned mode)
{
   (void)s; (void)device; (void)mode;
   return VIDEO_DRIVER_PACING_REASON_DEVICE;
}
bool d3d_driver_pacing_wait(d3d_driver_pacing_t *s, void *device,
      uint64_t frame_id, unsigned max_wait_us)
{
   (void)s; (void)device; (void)frame_id; (void)max_wait_us;
   return false;
}
void d3d_driver_pacing_mark(d3d_driver_pacing_t *s, void *device,
      uint64_t frame_id, unsigned marker)
{
   (void)s; (void)device; (void)frame_id; (void)marker;
}
void d3d_driver_pacing_frame_begin(d3d_driver_pacing_t *s, void *device,
      uint64_t frame_id, unsigned min_interval_us)
{
   (void)s; (void)device; (void)frame_id; (void)min_interval_us;
}
void d3d_driver_pacing_present_begin(d3d_driver_pacing_t *s, void *device)
{
   (void)s; (void)device;
}
void d3d_driver_pacing_present_end(d3d_driver_pacing_t *s, void *device)
{
   (void)s; (void)device;
}
bool d3d_driver_pacing_paced(d3d_driver_pacing_t *s, uint64_t frame_id)
{
   (void)s; (void)frame_id;
   return false;
}
bool d3d_driver_pacing_sleep_on(void *device)
{
   (void)device;
   return false;
}
void d3d_driver_pacing_mark_on(void *device, uint64_t frame_id,
      unsigned marker)
{
   (void)device; (void)frame_id; (void)marker;
}
void d3d_driver_pacing_free(d3d_driver_pacing_t *s, void *device)
{
   (void)s; (void)device;
}

#endif
