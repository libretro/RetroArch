/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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
#include "../boolean.h"
#include "../rthreads/rthreads.h"
#include "../general.h"

/* Starts a video driver in a new thread.
 * Access to video driver will be mediated through this driver. */
bool rarch_threaded_video_init(
      const video_driver_t **out_driver, void **out_data,
      const input_driver_t **input, void **input_data,
      const video_driver_t *driver, const video_info_t *info);

void *rarch_threaded_video_resolve(const video_driver_t **drv);

enum thread_cmd
{
   CMD_NONE = 0,
   CMD_INIT,
   CMD_SET_SHADER,
   CMD_FREE,
   CMD_ALIVE, /* Blocking alive check. Used when paused. */
   CMD_SET_ROTATION,
   CMD_READ_VIEWPORT,

#ifdef HAVE_OVERLAY
   CMD_OVERLAY_ENABLE,
   CMD_OVERLAY_LOAD,
   CMD_OVERLAY_TEX_GEOM,
   CMD_OVERLAY_VERTEX_GEOM,
   CMD_OVERLAY_FULL_SCREEN,
#endif

   CMD_POKE_SET_FILTERING,
#ifdef HAVE_FBO
   CMD_POKE_SET_FBO_STATE,
   CMD_POKE_GET_FBO_STATE,
#endif
   CMD_POKE_SET_ASPECT_RATIO,
   CMD_POKE_SET_OSD_MSG,
   CMD_CUSTOM_COMMAND,

   CMD_DUMMY = INT_MAX
};

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
   bool has_windowed;
   bool nonblock;

   retro_time_t last_time;
   unsigned hit_count;
   unsigned miss_count;

   float *alpha_mod;
   unsigned alpha_mods;
   bool alpha_update;
   slock_t *alpha_lock;

   void (*send_cmd_func)(struct thread_video *, enum thread_cmd);
   void (*wait_reply_func)(struct thread_video *, enum thread_cmd);
   enum thread_cmd send_cmd;
   enum thread_cmd reply_cmd;
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
         unsigned index;
         bool smooth;
      } filtering;

      struct
      {
         char msg[PATH_MAX];
         struct font_params params;
      } osd_message;

      struct
      {
         int (*method)(void*);
         void* data;
         int return_value;
      } custom_command;


   } cmd_data;

   struct rarch_viewport vp;
   struct rarch_viewport read_vp; /* Last viewport reported to caller. */

   struct
   {
      slock_t *lock;
      uint8_t *buffer;
      unsigned width;
      unsigned height;
      unsigned pitch;
      bool updated;
      bool within_thread;
      char msg[PATH_MAX];
   } frame;

   video_driver_t video_thread;

} thread_video_t;

#endif

