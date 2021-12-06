/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef RARCH_VIDEO_THREAD_H__
#define RARCH_VIDEO_THREAD_H__

#include <limits.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <rthreads/rthreads.h>
#include <retro_miscellaneous.h>

#include "font_driver.h"

RETRO_BEGIN_DECLS

enum thread_cmd
{
   CMD_VIDEO_NONE = 0,
   CMD_INIT,
   CMD_SET_SHADER,
   CMD_FREE,
   CMD_ALIVE, /* Blocking alive check. Used when paused. */
   CMD_SET_VIEWPORT,
   CMD_SET_ROTATION,
   CMD_READ_VIEWPORT,

   CMD_OVERLAY_ENABLE,
   CMD_OVERLAY_LOAD,
   CMD_OVERLAY_TEX_GEOM,
   CMD_OVERLAY_VERTEX_GEOM,
   CMD_OVERLAY_FULL_SCREEN,

   CMD_POKE_SET_VIDEO_MODE,
   CMD_POKE_SET_FILTERING,

   CMD_POKE_SET_FBO_STATE,
   CMD_POKE_GET_FBO_STATE,

   CMD_POKE_SET_ASPECT_RATIO,
   CMD_FONT_INIT,
   CMD_CUSTOM_COMMAND,

   CMD_POKE_SHOW_MOUSE,
   CMD_POKE_GRAB_MOUSE_TOGGLE,

   CMD_POKE_SET_HDR_MAX_NITS,
   CMD_POKE_SET_HDR_PAPER_WHITE_NITS,
   CMD_POKE_SET_HDR_CONTRAST,
   CMD_POKE_SET_HDR_EXPAND_GAMUT,   

   CMD_DUMMY = INT_MAX
};

typedef int (*custom_command_method_t)(void*);

typedef bool (*custom_font_command_method_t)(const void **font_driver,
      void **font_handle, void *video_data, const char *font_path,
      float font_size, enum font_driver_render_api api,
      bool is_threaded);

typedef struct thread_packet thread_packet_t;

struct thread_packet
{
   union
   {
      const char *str;
      void *v;
      int i;
      float f;
      bool b;

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
         bool ctx_scaling;
      } filtering;

      struct
      {
         char msg[128];
         struct font_params params;
      } osd_message;

      struct
      {
         custom_command_method_t method;
         void* data;
         int return_value;
      } custom_command;

      struct
      {
         custom_font_command_method_t method;
         const void **font_driver;
         void **font_handle;
         void *video_data;
         const char *font_path;
         float font_size;
         bool return_value;
         bool is_threaded;
         enum font_driver_render_api api;
      } font_init;

      struct
      {
         float max_nits;
         float paper_white_nits;
         float contrast;
         bool expand_gamut;
      } hdr;
   } data;
   enum thread_cmd type;
};

typedef struct thread_video
{
   retro_time_t last_time;

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
   input_driver_t **input;
   void **input_data;

   float *alpha_mod;
   slock_t *alpha_lock;
   void (*send_and_wait)(struct thread_video *, thread_packet_t*);

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

   unsigned hit_count;
   unsigned miss_count;
   unsigned alpha_mods;

   struct video_viewport vp;
   struct video_viewport read_vp; /* Last viewport reported to caller. */

   thread_packet_t cmd_data;
   video_driver_t video_thread;


   enum thread_cmd send_cmd;
   enum thread_cmd reply_cmd;

   bool alpha_update;

   struct
   {
      uint64_t count;
      slock_t *lock;
      uint8_t *buffer;
      unsigned width;
      unsigned height;
      unsigned pitch;
      char msg[NAME_MAX_LENGTH];
      bool updated;
      bool within_thread;
   } frame;


   bool apply_state_changes;

   bool alive;
   bool focus;
   bool suppress_screensaver;
   bool has_windowed;
   bool nonblock;
   bool is_idle;
} thread_video_t;

/**
 * video_init_thread:
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
bool video_init_thread(
      const video_driver_t **out_driver, void **out_data,
      input_driver_t **input, void **input_data,
      const video_driver_t *driver, const video_info_t info);

bool video_thread_font_init(
      const void **font_driver,
      void **font_handle,
      void *data,
      const char *font_path,
      float font_size,
      enum font_driver_render_api api,
      custom_font_command_method_t func,
      bool is_threaded);

unsigned video_thread_texture_load(void *data,
      custom_command_method_t func);

RETRO_END_DECLS

#endif
