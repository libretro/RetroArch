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

#ifndef __OPENXR_BINDING_GLES_H
#define __OPENXR_BINDING_GLES_H

#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>

#include <openxr/openxr.h>

#include "openxr_runtime.h"
#include "openxr_swapchain.h"

RETRO_BEGIN_DECLS

#define OPENXR_GLES_RGBA8        0x8058  /* GL_RGBA8 */
#define OPENXR_GLES_SRGB8_ALPHA8 0x8C43  /* GL_SRGB8_ALPHA8 */
#define OPENXR_GLES_FRAMEBUFFER_SRGB 0x8DB9  /* GL_FRAMEBUFFER_SRGB_EXT */

/* Where a slot's images go as textures: [image][layer]. */
typedef struct openxr_gles_images
{
   openxr_swapchains_t *sc;
   uint32_t (*tex)[2];
} openxr_gles_images_t;

/* Asked of the runtime before a GLES session, as it must be. */
bool openxr_binding_gles_requirements(const openxr_runtime_t *rt);

/* gl2 writes display-referred values: sRGB when the copy can leave
 * them as they are (srgb_write_control: EXT_sRGB_write_control), else
 * RGBA8, else sRGB with a warning, else 0. */
int64_t openxr_binding_gles_format(const openxr_swapchains_t *sc,
      bool srgb_write_control);

/* An openxr_images_cb; user is an openxr_gles_images_t. */
XrResult openxr_binding_gles_images(void *user, XrSwapchain swapchain,
      unsigned layer, uint32_t *count);

RETRO_END_DECLS

#endif
