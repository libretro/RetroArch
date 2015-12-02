/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#ifndef RARCH_VIDEO_THREAD_H__
#define RARCH_VIDEO_THREAD_H__

#include "../driver.h"
#include "../general.h"
#include <boolean.h>
#include <rthreads/rthreads.h>
#include "font_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

enum thread_cmd
{
   CMD_NONE = 0,
   CMD_INIT,
   CMD_SET_SHADER,
   CMD_FREE,
   CMD_ALIVE, /* Blocking alive check. Used when paused. */
   CMD_SET_VIEWPORT,
   CMD_SET_ROTATION,
   CMD_READ_VIEWPORT,

#ifdef HAVE_OVERLAY
   CMD_OVERLAY_ENABLE,
   CMD_OVERLAY_LOAD,
   CMD_OVERLAY_TEX_GEOM,
   CMD_OVERLAY_VERTEX_GEOM,
   CMD_OVERLAY_FULL_SCREEN,
#endif

   CMD_POKE_SET_VIDEO_MODE,
   CMD_POKE_SET_FILTERING,
   CMD_POKE_GET_VIDEO_OUTPUT_SIZE,
   CMD_POKE_GET_VIDEO_OUTPUT_PREV,
   CMD_POKE_GET_VIDEO_OUTPUT_NEXT,
#ifdef HAVE_FBO
   CMD_POKE_SET_FBO_STATE,
   CMD_POKE_GET_FBO_STATE,
#endif
   CMD_POKE_SET_ASPECT_RATIO,
   CMD_POKE_SET_OSD_MSG,
   CMD_FONT_INIT,
   CMD_CUSTOM_COMMAND,

   CMD_DUMMY = INT_MAX
};


typedef struct
{
   enum thread_cmd type;
   union
   {
      bool b;
      int i;
      float f;
      const char *str;
      void *v;

      struct
      {
         enum rarch_shader_type type;
         const char *path;
      } set_shader;

      struct
      {
         unsigned width;
         unsigned height;
         bool force_full;
         bool allow_rotate;
      } set_viewport;

      struct
      {
         unsigned index;
         float x, y, w, h;
      } rect;

      struct
      {
         const struct texture_image *data;
         unsigned num;
      } image;

      struct
      {
         unsigned width;
         unsigned height;
      } output;

      struct
      {
         unsigned width;
         unsigned height;
         bool fullscreen;
      } new_mode;

      struct
      {
         unsigned index;
         bool smooth;
      } filtering;

      struct
      {
         char msg[PATH_MAX_LENGTH];
         struct font_params params;
      } osd_message;

      struct
      {
         int (*method)(void*);
         void* data;
         int return_value;
      } custom_command;

      struct
      {
         bool (*method)(const void **font_driver,
                        void **font_handle, void *video_data, const char *font_path,
                        float font_size, enum font_driver_render_api api);
         const void **font_driver;
         void **font_handle;
         void *video_data;
         const char *font_path;
         float font_size;
         bool return_value;
         enum font_driver_render_api api;
      } font_init;
   } data;
} thread_packet_t;

typedef struct thread_video
{
   slock_t *lock;
   scond_t *cond_cmd;
   scond_t *cond_thread;
   sthread_t *thread;

   video_info_t info;
   const video_driver_t *driver;

#ifdef HAVE_OVERLAY
   const video_overlay_interface_t *overlay;
#endif
   const video_poke_interface_t *poke;

   void *driver_data;
   const input_driver_t **input;
   void **input_data;

#if defined(HAVE_MENU)
   struct
   {
      void *frame;
      size_t frame_cap;
      unsigned width;
      unsigned height;
      float alpha;
      bool frame_updated;
      bool rgb32;
      bool enable;
      bool full_screen;
   } texture;
#endif
   bool apply_state_changes;

   bool alive;
   bool focus;
   bool suppress_screensaver;
   bool has_windowed;
   bool nonblock;

   retro_time_t last_time;
   unsigned hit_count;
   unsigned miss_count;

   float *alpha_mod;
   unsigned alpha_mods;
   bool alpha_update;
   slock_t *alpha_lock;

   void (*send_and_wait)(struct thread_video *, thread_packet_t*);
   enum thread_cmd send_cmd;
   enum thread_cmd reply_cmd;
   thread_packet_t cmd_data;

   struct video_viewport vp;
   struct video_viewport read_vp; /* Last viewport reported to caller. */

   struct
   {
      slock_t *lock;
      uint8_t *buffer;
      unsigned width;
      unsigned height;
      unsigned pitch;
      bool updated;
      bool within_thread;
      uint64_t count;
      char msg[PATH_MAX_LENGTH];
   } frame;

   video_driver_t video_thread;

} thread_video_t;

/**
 * rarch_threaded_video_init:
 * @out_driver                : Output video driver
 * @out_data                  : Output video data
 * @input                     : Input input driver
 * @input_data                : Input input data 
 * @driver                    : Input Video driver
 * @info                      : Video info handle.
 *
 * Creates, initializes and starts a video driver in a new thread.
 * Access to video driver will be mediated through this driver.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool rarch_threaded_video_init(
      const video_driver_t **out_driver, void **out_data,
      const input_driver_t **input, void **input_data,
      const video_driver_t *driver, const video_info_t *info);

/**
 * rarch_threaded_video_get_ptr:
 * @drv                       : Found driver.
 *
 * Gets the underlying video driver associated with the 
 * threaded video wrapper. Sets @drv to the found
 * video driver.
 *
 * Returns: Video driver data of the video driver associated
 * with the threaded wrapper (if successful). If not successful,
 * NULL.
 **/
void *rarch_threaded_video_get_ptr(const video_driver_t **drv);

const char *rarch_threaded_video_get_ident(void);

#ifdef __cplusplus
}
#endif

#endif
