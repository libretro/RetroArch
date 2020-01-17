/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2011-2017 - Higor Euripedes
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

#include <stdlib.h>
#include <string.h>

#include <retro_assert.h>
#include <gfx/video_frame.h>
#include <retro_assert.h>
#include "../../verbosity.h"

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include <SDL/SDL.h>
#include <SDL/SDL_video.h>

#include "../../configuration.h"
#include "../../retroarch.h"

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define VERBOSE 0

typedef struct sdl_dingux_video
{
   SDL_Surface *screen;
   bool rgb;
   bool menu_active;
   bool was_in_menu;
   bool quitting;
   char menu_frame[320*240*32];

} sdl_dingux_video_t;

static void sdl_dingux_gfx_free(void *data)
{
   sdl_dingux_video_t *vid = (sdl_dingux_video_t*)data;
   if (!vid)
      return;

   SDL_QuitSubSystem(SDL_INIT_VIDEO);

   free(vid);
}

static void *sdl_dingux_gfx_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   sdl_dingux_video_t *vid = NULL;
   settings_t *settings = config_get_ptr();

    FILE* f = fopen("/sys/devices/platform/jz-lcd.0/allow_downscaling", "w");
    if (f) {
        fprintf(f, "%d", 1);
        fclose(f);
    }

   if (SDL_WasInit(0) == 0)
   {
      if (SDL_Init(SDL_INIT_VIDEO) < 0)
         return NULL;
   }
   else if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
      return NULL;

   vid = (sdl_dingux_video_t*)calloc(1, sizeof(*vid));
   if (!vid)
      return NULL;

#ifdef VERBOSE
   printf("sdl_dingux_gfx_init video %dx%d rgb32 %d smooth %d input_scale %u force_aspect %d fullscreen %d\n",
           video->width, video->height, video->rgb32, video->smooth, video->input_scale, video->force_aspect, video->fullscreen);
#endif

   vid->screen = SDL_SetVideoMode(320, 240, video->rgb32 ? 32 : 16, SDL_HWSURFACE | SDL_TRIPLEBUF | SDL_FULLSCREEN);
   if (!vid->screen)
   {
      RARCH_ERR("[SDL1]: Failed to init SDL surface: %s\n", SDL_GetError());
      goto error;
   }

   vid->rgb = video->rgb32;
   vid->menu_active = false;
   vid->was_in_menu = false;

   SDL_ShowCursor(SDL_DISABLE);

   if (input && input_data)
   {
      void *sdl_input = input_sdl.init(settings->arrays.input_joypad_driver);

      if (sdl_input)
      {
         *input = &input_sdl;
         *input_data = sdl_input;
      }
      else
      {
         *input = NULL;
         *input_data = NULL;
      }
   }

   return vid;

error:
   sdl_dingux_gfx_free(vid);
   return NULL;
}

static void clear_screen(void* data)
{
    sdl_dingux_video_t* vid = (sdl_dingux_video_t*)data;
	SDL_FillRect(vid->screen, 0, 0);
	SDL_Flip(vid->screen);
	SDL_FillRect(vid->screen, 0, 0);
	SDL_Flip(vid->screen);
	SDL_FillRect(vid->screen, 0, 0);
	SDL_Flip(vid->screen);
}

static void set_output(sdl_dingux_video_t* vid, int width, int height, int pitch, bool rgb)
{
#ifdef VERBOSE
    printf("set_output current w %d h %d pitch %d new_w %d new_h %d pitch %d rgb %d\n",
            vid->screen->w, vid->screen->h, vid->screen->pitch, width, height, pitch, (int)vid->rgb);
#endif

    vid->screen = SDL_SetVideoMode(width, height, rgb ? 32 : 16, SDL_HWSURFACE | SDL_TRIPLEBUF | SDL_FULLSCREEN);
    if (!vid->screen)
        RARCH_ERR("[SDL1]: Failed to init SDL surface: %s\n", SDL_GetError());
}

static void blit(uint32_t* d, uint32_t* s, int width, int height, int pitch)
{
    int skip = pitch/4 - width;
    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
            *(d++) = *(s++);
        s += skip;
    }
}

static bool sdl_dingux_gfx_frame(void *data, const void *frame, unsigned width,
        unsigned height, uint64_t frame_count,
        unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
//    printf("sdl_gfx_frame width %d height %d pitch %d frame_count %lu\n", width, height, pitch, frame_count); 
    sdl_dingux_video_t* vid = (sdl_dingux_video_t*)data;

    if (unlikely(!frame))
        return true;

    if (unlikely((vid->screen->w != width || vid->screen->h != height) && !vid->menu_active))
    {
        set_output(vid, width, height, pitch, vid->rgb);
    }

    menu_driver_frame(video_info);

    if (likely(!vid->menu_active))
    {
        blit((uint32_t*)vid->screen->pixels, (uint32_t*)frame, vid->rgb ? width : width/2, height, pitch);
        if (unlikely(vid->was_in_menu))
            vid->was_in_menu = false;
    }
    else
    {
        if (!vid->was_in_menu)
        {
            set_output(vid, 320, 240, 320*2, false);
            vid->was_in_menu = true;
        }
        memcpy(vid->screen->pixels, vid->menu_frame, 320*240*2);
    }

    SDL_Flip(vid->screen);

    return true;
}

static void sdl_dingux_set_texture_enable(void *data, bool state, bool full_screen)
{
    sdl_dingux_video_t *vid = (sdl_dingux_video_t*)data;
    (void)full_screen;

    if (vid->menu_active != state)
    {
        vid->menu_active = state;
    }
}

static void sdl_dingux_set_texture_frame(void *data, const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
    sdl_dingux_video_t *vid = (sdl_dingux_video_t*)data;

    int len = width * height * 2;
    memcpy(vid->menu_frame, frame, len);
}


static void sdl_dingux_gfx_set_nonblock_state(void *data, bool state)
{
   (void)data; /* Can SDL even do this? */
   (void)state;
}

static void sdl_dingux_gfx_check_window(sdl_dingux_video_t *vid)
{
   SDL_Event event;

   SDL_PumpEvents();
   while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_QUITMASK))
   {
      if (event.type != SDL_QUIT)
         continue;

      vid->quitting = true;
      break;
   }
}

static bool sdl_dingux_gfx_alive(void *data)
{
   sdl_dingux_video_t *vid = (sdl_dingux_video_t*)data;
   sdl_dingux_gfx_check_window(vid);
   return !vid->quitting;
}

static bool sdl_dingux_gfx_focus(void *data)
{
    (void)data;
    return true;
}

static bool sdl_dingux_gfx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static bool sdl_dingux_gfx_has_windowed(void *data)
{
   (void)data;
   return false;
}

static void sdl_dingux_gfx_viewport_info(void *data, struct video_viewport *vp)
{
   sdl_dingux_video_t *vid = (sdl_dingux_video_t*)data;
   vp->x      = 0;
   vp->y      = 0;
   vp->width  = vp->full_width  = vid->screen->w;
   vp->height = vp->full_height = vid->screen->h;
}

static void sdl_dingux_set_filtering(void *data, unsigned index, bool smooth)
{
    (void)data;
}

static void sdl_dingux_apply_state_changes(void *data)
{
   (void)data;
}

static uint32_t sdl_dingux_get_flags(void *data)
{
    (void)data;
    return 0;
}

static const video_poke_interface_t sdl_dingux_poke_interface = {
   sdl_dingux_get_flags,
   NULL,
   NULL,
   NULL,
   NULL, /* get_refresh_rate */
   sdl_dingux_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   NULL,
   sdl_dingux_apply_state_changes,
   sdl_dingux_set_texture_frame,
   sdl_dingux_set_texture_enable,
   NULL,
   NULL,//sdl_show_mouse,
   NULL,//sdl_grab_mouse_toggle,
   NULL,                         /* get_current_shader */
   NULL,                         /* get_current_software_framebuffer */
   NULL                          /* get_hw_render_interface */
};

static void sdl_dingux_get_poke_interface(void *data, const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &sdl_dingux_poke_interface;
}

static bool sdl_dingux_gfx_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   (void)data;
   (void)type;
   (void)path;
   return false;
}

video_driver_t video_sdl_dingux = {
   sdl_dingux_gfx_init,
   sdl_dingux_gfx_frame,
   sdl_dingux_gfx_set_nonblock_state,
   sdl_dingux_gfx_alive,
   sdl_dingux_gfx_focus,
   sdl_dingux_gfx_suppress_screensaver,
   sdl_dingux_gfx_has_windowed,
   sdl_dingux_gfx_set_shader,
   sdl_dingux_gfx_free,
   "sdl_dingux",
   NULL,
   NULL, /* set_rotation */
   sdl_dingux_gfx_viewport_info,
   NULL, /* read_viewport  */
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   NULL,
#endif
#ifdef HAVE_VIDEO_LAYOUT
  NULL,
#endif
   sdl_dingux_get_poke_interface
};
