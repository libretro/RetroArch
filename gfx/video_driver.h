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

#ifndef __VIDEO_DRIVER__H
#define __VIDEO_DRIVER__H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <boolean.h>
#include <gfx/scaler/scaler.h>
#include "video_filter.h"
#include "video_shader_parse.h"
#include "video_viewport.h"

#include "../libretro.h"
#include "../input/input_driver.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef struct video_info
{
   unsigned width;
   unsigned height;
   bool fullscreen;
   bool vsync;
   bool force_aspect;
#ifdef GEKKO
   /* TODO - we can't really have driver system-specific
    * variables in here. There should be some
    * kind of publicly accessible driver implementation
    * video struct for specific things like this.
    */
   unsigned viwidth;
#endif
   bool vfilter;
   bool smooth;
   /* Maximum input size: RARCH_SCALE_BASE * input_scale */
   unsigned input_scale;
   /* Use 32bit RGBA rather than native RGB565/XBGR1555. */
   bool rgb32;
} video_info_t;

  enum text_alignment
  {
     TEXT_ALIGN_LEFT = 0,
     TEXT_ALIGN_RIGHT,
     TEXT_ALIGN_CENTER
  };

struct font_params
{
   float x;
   float y;
   float scale;
   /* Drop shadow color multiplier. */
   float drop_mod;
   /* Drop shadow offset.
    * If both are 0, no drop shadow will be rendered. */
   int drop_x, drop_y;
   /* ABGR. Use the macros. */
   uint32_t color;
   bool full_screen;
   enum text_alignment text_align;
};

enum texture_filter_type
{
   TEXTURE_FILTER_LINEAR = 0,
   TEXTURE_FILTER_NEAREST,
   TEXTURE_FILTER_MIPMAP_LINEAR,
   TEXTURE_FILTER_MIPMAP_NEAREST
};

#define FONT_COLOR_RGBA(r, g, b, a) (((r) << 24) | ((g) << 16) | ((b) << 8) | ((a) << 0))
#define FONT_COLOR_GET_RED(col)   (((col) >> 24) & 0xff)
#define FONT_COLOR_GET_GREEN(col) (((col) >> 16) & 0xff)
#define FONT_COLOR_GET_BLUE(col)  (((col) >>  8) & 0xff)
#define FONT_COLOR_GET_ALPHA(col) (((col) >>  0) & 0xff)
#define FONT_COLOR_ARGB_TO_RGBA(col) ( (((col) >> 24) & 0xff) | (((col) << 8) & 0xffffff00) )

/* Optionally implemented interface to poke more
 * deeply into video driver. */

typedef struct video_poke_interface
{
   void (*set_video_mode)(void *data, unsigned width, unsigned height, bool fullscreen);
   void (*set_filtering)(void *data, unsigned index, bool smooth);
   void (*get_video_output_size)(void *data, unsigned *width, unsigned *height);
   void (*get_video_output_prev)(void *data);
   void (*get_video_output_next)(void *data);
   uintptr_t (*get_current_framebuffer)(void *data);
   retro_proc_address_t (*get_proc_address)(void *data, const char *sym);
   void (*set_aspect_ratio)(void *data, unsigned aspectratio_index);
   void (*apply_state_changes)(void *data);

#ifdef HAVE_MENU
   /* Update texture. */
   void (*set_texture_frame)(void *data, const void *frame, bool rgb32,
         unsigned width, unsigned height, float alpha);
#endif
   /* Enable or disable rendering. */
   void (*set_texture_enable)(void *data, bool enable, bool full_screen);
   void (*set_osd_msg)(void *data, const char *msg,
         const struct font_params *params, void *font);

   void (*show_mouse)(void *data, bool state);
   void (*grab_mouse_toggle)(void *data);

   struct video_shader *(*get_current_shader)(void *data);
} video_poke_interface_t;

typedef struct video_driver
{
   /* Should the video driver act as an input driver as well?
    * The video initialization might preinitialize an input driver
    * to override the settings in case the video driver relies on
    * input driver for event handling. */
   void *(*init)(const video_info_t *video, const input_driver_t **input,
         void **input_data);

   /* msg is for showing a message on the screen along with the video frame. */
   bool (*frame)(void *data, const void *frame, unsigned width,
         unsigned height, uint64_t frame_count, unsigned pitch, const char *msg);

   /* Should we care about syncing to vblank? Fast forwarding. */
   void (*set_nonblock_state)(void *data, bool toggle);

   /* Is the window still active? */
   bool (*alive)(void *data);

   /* Does the window have focus? */
   bool (*focus)(void *data);

   /* Should the screensaver be suppressed? */
   bool (*suppress_screensaver)(void *data, bool enable);

   /* Does the graphics context support windowed mode? */
   bool (*has_windowed)(void *data);

   /* Sets shader. Might not be implemented. Will be moved to
    * poke_interface later. */
   bool (*set_shader)(void *data, enum rarch_shader_type type,
         const char *path);

   /* Frees driver. */
   void (*free)(void *data);

   /* Human-readable identifier. */
   const char *ident;

   void (*set_viewport)(void *data, unsigned width, unsigned height,
         bool force_full, bool allow_rotate);

   void (*set_rotation)(void *data, unsigned rotation);
   void (*viewport_info)(void *data, struct video_viewport *vp);

   /* Reads out in BGR byte order (24bpp). */
   bool (*read_viewport)(void *data, uint8_t *buffer);

   /* Returns a pointer to a newly allocated buffer that can
    * (and must) be passed to free() by the caller, containing a
    * copy of the current raw frame in the active pixel format
    * and sets width, height and pitch to the correct values. */
   void* (*read_frame_raw)(void *data, unsigned *width,
   unsigned *height, size_t *pitch);

#ifdef HAVE_OVERLAY
   void (*overlay_interface)(void *data, const video_overlay_interface_t **iface);
#endif
   void (*poke_interface)(void *data, const video_poke_interface_t **iface);
   unsigned (*wrap_type_to_enum)(enum gfx_wrap_type type);
} video_driver_t;

extern video_driver_t video_gl;
extern video_driver_t video_psp1;
extern video_driver_t video_vita2d;
extern video_driver_t video_ctr;
extern video_driver_t video_d3d;
extern video_driver_t video_gx;
extern video_driver_t video_xenon360;
extern video_driver_t video_xvideo;
extern video_driver_t video_xdk_d3d;
extern video_driver_t video_sdl;
extern video_driver_t video_sdl2;
extern video_driver_t video_vg;
extern video_driver_t video_omap;
extern video_driver_t video_exynos;
extern video_driver_t video_dispmanx;
extern video_driver_t video_sunxi;
extern video_driver_t video_xshm;
extern video_driver_t video_null;

enum rarch_display_type
{
   /* Non-bindable types like consoles, KMS, VideoCore, etc. */
   RARCH_DISPLAY_NONE = 0,
   /* video_display => Display*, video_window => Window */
   RARCH_DISPLAY_X11,
   /* video_display => N/A, video_window => HWND */
   RARCH_DISPLAY_WIN32,
   RARCH_DISPLAY_OSX
};

enum rarch_display_ctl_state
{
   RARCH_DISPLAY_CTL_NONE = 0,
   RARCH_DISPLAY_CTL_APPLY_STATE_CHANGES,
   RARCH_DISPLAY_CTL_FIND_DRIVER,
   RARCH_DISPLAY_CTL_FRAME_FILTER_ALIVE,
   RARCH_DISPLAY_CTL_FRAME_FILTER_IS_32BIT,
   RARCH_DISPLAY_CTL_HAS_WINDOWED,
   RARCH_DISPLAY_CTL_IS_FOCUSED,
   RARCH_DISPLAY_CTL_SET_ASPECT_RATIO,
   RARCH_DISPLAY_CTL_READ_VIEWPORT,
   RARCH_DISPLAY_CTL_SET_NONBLOCK_STATE,
   /* Renders the current video frame. */
   RARCH_DISPLAY_CTL_CACHED_FRAME_RENDER,
   RARCH_DISPLAY_CTL_CACHED_FRAME_HAS_VALID_FB,
   RARCH_DISPLAY_CTL_SHOW_MOUSE,
   RARCH_DISPLAY_CTL_GET_FRAME_COUNT
};

bool video_driver_ctl(enum rarch_display_ctl_state state, void *data);

/**
 * video_driver_find_handle:
 * @index              : index of driver to get handle to.
 *
 * Returns: handle to video driver at index. Can be NULL
 * if nothing found.
 **/
const void *video_driver_find_handle(int index);

/**
 * video_driver_find_ident:
 * @index              : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of video driver at index. Can be NULL
 * if nothing found.
 **/
const char *video_driver_find_ident(int index);

/**
 * config_get_video_driver_options:
 *
 * Get an enumerated list of all video driver names, separated by '|'.
 *
 * Returns: string listing of all video driver names, separated by '|'.
 **/
const char* config_get_video_driver_options(void);

/**
 * video_driver_get_ptr:
 *
 * Use this if you need the real video driver
 * and driver data pointers.
 *
 * Returns: video driver's userdata.
 **/
void *video_driver_get_ptr(void);

/**
 * video_driver_get_current_framebuffer:
 *
 * Gets pointer to current hardware renderer framebuffer object.
 * Used by RETRO_ENVIRONMENT_SET_HW_RENDER.
 *
 * Returns: pointer to hardware framebuffer object, otherwise 0.
 **/
uintptr_t video_driver_get_current_framebuffer(void);

retro_proc_address_t video_driver_get_proc_address(const char *sym);

bool video_driver_set_shader(enum rarch_shader_type type,
      const char *shader);

void uninit_video_input(void);

bool init_video(void);

bool video_driver_set_rotation(unsigned rotation);

void video_driver_set_video_mode(unsigned width,
      unsigned height, bool fullscreen);

bool video_driver_get_video_output_size(
      unsigned *width, unsigned *height);

void video_driver_set_osd_msg(const char *msg,
      const struct font_params *params, void *font);

void video_driver_set_texture_enable(bool enable, bool full_screen);

void video_driver_set_texture_frame(const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha);

bool video_driver_viewport_info(struct video_viewport *vp);

bool video_driver_set_shader(enum rarch_shader_type type,
      const char *path);

#ifdef HAVE_OVERLAY
bool video_driver_overlay_interface(const video_overlay_interface_t **iface);
#endif

void * video_driver_read_frame_raw(unsigned *width,
   unsigned *height, size_t *pitch);

void video_driver_set_filtering(unsigned index, bool smooth);

void video_driver_get_video_output_next(void);

void video_driver_get_video_output_prev(void);

bool video_driver_suppress_screensaver(bool enable);

const char *video_driver_get_ident(void);

bool video_driver_set_viewport(unsigned width, unsigned height,
      bool force_fullscreen, bool allow_rotate);

void video_driver_get_size(unsigned *width, unsigned *height);

void video_driver_set_size_width(unsigned width);

void video_driver_set_size_height(unsigned width);

float video_driver_get_aspect_ratio(void);

void video_driver_set_aspect_ratio_value(float value);

struct retro_hw_render_callback *video_driver_callback(void);

void video_driver_unset_callback(void);

bool video_driver_frame_filter(const void *data,
      unsigned width, unsigned height,
      size_t pitch,
      unsigned *output_width, unsigned *output_height,
      unsigned *output_pitch);

rarch_softfilter_t *video_driver_frame_filter_get_ptr(void);

void *video_driver_frame_filter_get_buf_ptr(void);

enum retro_pixel_format video_driver_get_pixel_format(void);

void video_driver_set_pixel_format(enum retro_pixel_format fmt);

void video_driver_cached_frame_set(const void *data, unsigned width,
      unsigned height, size_t pitch);

void video_driver_cached_frame_set_ptr(const void *data);

void video_driver_cached_frame_get(const void **data, unsigned *width,
      unsigned *height, size_t *pitch);

#ifdef __cplusplus
}
#endif

#endif
