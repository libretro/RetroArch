/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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

#ifndef __OPENXR_SWAPCHAIN_H
#define __OPENXR_SWAPCHAIN_H

#include <stdint.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_common_api.h>

#include <openxr/openxr.h>

#include "openxr_runtime.h"
#include "openxr_session.h"
#include "../video_xr.h"

RETRO_BEGIN_DECLS

#define OPENXR_MAX_IMAGES  8
#define OPENXR_MAX_FORMATS 64

/* A swapchain per layer, not one array: SteamVR on the Steam Frame
 * ignores a quad's imageArrayIndex and shows layer 0. */
typedef struct openxr_slot
{
   XrSwapchain swapchains[2];   /* changed under the graphics lock */
   unsigned dims;
   unsigned layers;
   uint32_t index[2];           /* the drawing thread */
   bool acquired[2];
   bool waited[2];
   retro_atomic_int_t content;  /* every layer has released an image */
} openxr_slot_t;

/* The binding lists at most *count of swapchain's images where it keeps
 * them for layer, and sets *count to the number it got. */
typedef XrResult (*openxr_images_cb)(void *user, XrSwapchain swapchain,
      unsigned layer, uint32_t *count);

typedef struct openxr_swapchains
{
   openxr_session_t *session;
   int64_t formats[OPENXR_MAX_FORMATS];
   uint32_t num_formats;
   openxr_slot_t slots[VIDEO_XR_MAX_SLOTS];
   PFN_xrEnumerateSwapchainFormats EnumerateSwapchainFormats;
   PFN_xrCreateSwapchain CreateSwapchain;
   PFN_xrDestroySwapchain DestroySwapchain;
   PFN_xrEnumerateSwapchainImages EnumerateSwapchainImages;
   PFN_xrAcquireSwapchainImage AcquireSwapchainImage;
   PFN_xrWaitSwapchainImage WaitSwapchainImage;
   PFN_xrReleaseSwapchainImage ReleaseSwapchainImage;
} openxr_swapchains_t;

bool openxr_swapchains_load(openxr_swapchains_t *sc,
      const openxr_runtime_t *rt, openxr_session_t *session);
/* After the session is made. */
void openxr_swapchains_list_formats(openxr_swapchains_t *sc);
bool openxr_swapchains_supports(const openxr_swapchains_t *sc,
      int64_t format);
bool openxr_slot_create(openxr_swapchains_t *sc, unsigned slot,
      int64_t format, bool mutable_format, unsigned dims, unsigned layers,
      openxr_images_cb images, void *images_user, unsigned *num_images);
void openxr_slot_destroy(openxr_swapchains_t *sc, unsigned slot);
bool openxr_slot_acquire(openxr_swapchains_t *sc, unsigned slot,
      unsigned *index);
void openxr_slot_release(openxr_swapchains_t *sc, unsigned slot);
void openxr_slot_forget(openxr_swapchains_t *sc, unsigned slot);

RETRO_END_DECLS

#endif
