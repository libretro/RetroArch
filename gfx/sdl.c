/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "SDL.h"
#include "driver.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "general.h"
#include "input/ssnes_sdl_input.h"

typedef struct sdl_video sdl_video_t;
struct sdl_video
{
   SDL_Surface *screen, *buffer;
   bool quitting;
};

static void sdl_gfx_free(void *data)
{
   sdl_video_t *vid = data;
   if (!vid)
      return;

   if (vid->buffer)
      SDL_FreeSurface(vid->screen);

   SDL_QuitSubSystem(SDL_INIT_VIDEO);

   free(vid);
}

static void* sdl_gfx_init(const video_info_t *video, const input_driver_t **input, void **input_data)
{
   SDL_InitSubSystem(SDL_INIT_VIDEO);

   sdl_video_t *vid = calloc(1, sizeof(*vid));
   if (!vid)
      return NULL;

   const SDL_VideoInfo *video_info = SDL_GetVideoInfo();
   assert(video_info);
   unsigned full_x = video_info->current_w;
   unsigned full_y = video_info->current_h;
   SSNES_LOG("Detecting desktop resolution %ux%u.\n", full_x, full_y);

   vid->screen = SDL_SetVideoMode(video->width, video->height, 15, SDL_HWSURFACE | (video->fullscreen ? SDL_FULLSCREEN : 0));
   if (!vid->screen)
   {
      SSNES_ERR("Failed to init SDL surface.\n");
      goto error;
   }

   SDL_ShowCursor(SDL_DISABLE);
   vid->buffer = SDL_CreateRGBSurface(SDL_SWSURFACE, 256 * video->input_scale, 256 * video->input_scale, 15,
      0x7c00, 0x03e0, 0x001f, 0);
   if (!vid->buffer)
   {
      SSNES_ERR("SDL_CreateRGBSurface failed: %s\n", SDL_GetError());
      goto error;
   }

   sdl_input_t *sdl_input = input_sdl.init();
   if (sdl_input)
   {
      sdl_input->quitting = &vid->quitting;
      *input = &input_sdl;
      *input_data = sdl_input;
   }
   else
      *input = NULL;

   return vid;

error:
   sdl_gfx_free(vid);
   return NULL;
}

static bool sdl_gfx_frame(void *data, const void* frame, unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   (void)msg;
   sdl_video_t *vid = data;

   if (SDL_MUSTLOCK(vid->buffer))
      SDL_LockSurface(vid->buffer);

   for (unsigned y = 0; y < height; y++)
   {
      uint16_t *dest = (uint16_t*)vid->buffer->pixels + ((y * vid->buffer->pitch) >> 1);
      const uint16_t *src = (const uint16_t*)frame + ((y * pitch) >> 1);
      memcpy(dest, src, width * sizeof(uint16_t));
   }

   if (SDL_MUSTLOCK(vid->buffer))
      SDL_UnlockSurface(vid->buffer);

   SDL_Rect src = {
      .x = 0,
      .y = 0,
      .w = width,
      .h = height
   };

   SDL_Rect dest = {
      .x = 0,
      .y = 0,
      .w = vid->screen->w,
      .h = vid->screen->h
   };

   SDL_SoftStretch(vid->buffer, &src, vid->screen, &dest);
   SDL_UpdateRect(vid->screen, dest.x, dest.y, dest.w, dest.h);

   return true;
}

static void sdl_gfx_set_nonblock_state(void *data, bool state)
{
   (void)data; // Can SDL even do this?
   (void)state;
}

static bool sdl_gfx_alive(void *data)
{
   sdl_video_t *vid = data;
   return !vid->quitting;
}

static bool sdl_gfx_focus(void *data)
{
   (void)data;
   return (SDL_GetAppState() & (SDL_APPINPUTFOCUS | SDL_APPACTIVE)) == (SDL_APPINPUTFOCUS | SDL_APPACTIVE);
}


const video_driver_t video_sdl = {
   .init = sdl_gfx_init,
   .frame = sdl_gfx_frame,
   .alive = sdl_gfx_alive,
   .set_nonblock_state = sdl_gfx_set_nonblock_state,
   .focus = sdl_gfx_focus,
   .free = sdl_gfx_free,
   .ident = "sdl"
};

