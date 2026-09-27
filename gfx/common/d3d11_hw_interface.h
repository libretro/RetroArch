/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - libretroadmin
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

#ifndef RARCH_D3D11_HW_INTERFACE_H__
#define RARCH_D3D11_HW_INTERFACE_H__

#include <retro_inline.h>
#include <libretro.h>
#include <libretro_d3d11.h>

#include "../video_driver.h"

/* The highest retro_hw_render_interface_d3d11 version this frontend
 * implements. */
#define RARCH_D3D11_HW_INTERFACE_VERSION RETRO_HW_RENDER_INTERFACE_D3D11_VERSION_2

/* The version to hand the running core: 1 unless it asked for more
 * through retro_hw_render_context_negotiation_interface_d3d11, and never
 * more than it asked for. The driver and the threaded wrapper's hardware
 * ring in front of it both ask here, so they cannot disagree. */
static INLINE unsigned d3d11_hw_interface_negotiated_version(void)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   const struct retro_hw_render_context_negotiation_interface_d3d11 *n =
      (const struct retro_hw_render_context_negotiation_interface_d3d11*)
      video_st->hw_render_context_negotiation;

   if (     !n
         || n->interface_type    != RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_D3D11
         || n->interface_version <  RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_D3D11_VERSION
         || n->max_render_interface_version < RETRO_HW_RENDER_INTERFACE_D3D11_VERSION_2)
      return RETRO_HW_RENDER_INTERFACE_D3D11_VERSION;
   if (n->max_render_interface_version > RARCH_D3D11_HW_INTERFACE_VERSION)
      return RARCH_D3D11_HW_INTERFACE_VERSION;
   return n->max_render_interface_version;
}

#endif
