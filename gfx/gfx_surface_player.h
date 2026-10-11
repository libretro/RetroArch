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

#ifndef __GFX_SURFACE_PLAYER_H
#define __GFX_SURFACE_PLAYER_H

/* A video_player whose frames go to a gfx_surface: decoded into the
 * surface's slots and submitted from there, a dropped frame sent again
 * when the surface gives its slot back. The surface stays the
 * consumer's - it makes it, draws it and frees it - and the player
 * finds it through the consumer's pointer, so a replaced surface is
 * picked up. Main thread. */

#include <retro_common_api.h>
#include <boolean.h>
#include <formats/image.h>
#include <formats/video_player.h>

#include "gfx_surface.h"

RETRO_BEGIN_DECLS

typedef struct gfx_surface_player_cbs
{
   void *user;
   /* The surface for @num_slots slots of @dims in @pixfmt, made or
    * reused; NULL: asked again on a later frame. */
   gfx_surface_t *(*surface)(void *user, unsigned dims,
         unsigned num_slots, uint32_t pixfmt);
   /* A frame has reached the texture. */
   void (*shown)(void *user, gfx_surface_t *s);
   /* Optional, on the decoding thread before each decode; false ends
    * the playback. */
   bool (*feed)(void *user);
} gfx_surface_player_cbs_t;

typedef struct gfx_surface_player
{
   video_player_t *vp;
   void *stream;              /* borrowed, as the player's */
   void **surface;            /* the consumer's gfx_surface_t pointer */
   gfx_surface_player_cbs_t cb;
   enum image_type_enum type;
   bool rgba;
   bool threaded;             /* two slots, decoded on the worker */
} gfx_surface_player_t;

/* Plays @stream (borrowed) into the surface *@surface names. */
gfx_surface_player_t *gfx_surface_player_new(void *stream,
      enum image_type_enum type, int loop_count, void **surface,
      const gfx_surface_player_cbs_t *cb, unsigned flags);
void gfx_surface_player_free(gfx_surface_player_t *gsp);

/* The surface's release hook forwards here (@gsp NULL once the player
 * is gone). A frame the driver dropped is sent again, so the texture
 * is not left a frame behind; returns true then, and the slot stays
 * the surface's. */
bool gfx_surface_player_release(gfx_surface_player_t *gsp,
      gfx_surface_t *s, unsigned slot);

RETRO_END_DECLS

#endif
