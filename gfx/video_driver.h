/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2021 - Daniel De Matteis
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

#ifndef RARCH_VIDEO_DRIVER_H__
#define RARCH_VIDEO_DRIVER_H__

#include <stddef.h>


#include <libretro.h>
#include <retro_common_api.h>
#include <boolean.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#include <gfx/scaler/pixconv.h>
#include <gfx/scaler/scaler.h>

#include "../configuration.h"
#include "../input/input_driver.h"
#include "../input/input_types.h"

#include "video_defines.h"

#ifdef HAVE_VIDEO_LAYOUT
#include "video_layout.h"
#endif

#ifdef HAVE_CRTSWITCHRES
#include "video_crt_switch.h"
#endif

#include "video_coord_array.h"
#include "video_shader_parse.h"
#include "video_filter.h"

#define RARCH_SCALE_BASE 256

#define MEASURE_FRAME_TIME_SAMPLES_COUNT (2 * 1024)

#define VIDEO_SHADER_STOCK_BLEND (GFX_MAX_SHADERS - 1)
#define VIDEO_SHADER_MENU        (GFX_MAX_SHADERS - 2)
#define VIDEO_SHADER_MENU_2      (GFX_MAX_SHADERS - 3)
#define VIDEO_SHADER_MENU_3      (GFX_MAX_SHADERS - 4)
#define VIDEO_SHADER_MENU_4      (GFX_MAX_SHADERS - 5)
#define VIDEO_SHADER_MENU_5      (GFX_MAX_SHADERS - 6)
#define VIDEO_SHADER_MENU_6      (GFX_MAX_SHADERS - 7)
#define VIDEO_SHADER_STOCK_HDR   (GFX_MAX_SHADERS - 8)

#define VIDEO_HDR_MAX_CONTRAST 10.0f

#if defined(_XBOX360)
#define DEFAULT_SHADER_TYPE RARCH_SHADER_HLSL
#elif defined(__PSL1GHT__) || defined(HAVE_OPENGLES2) || defined(HAVE_GLSL)
#define DEFAULT_SHADER_TYPE RARCH_SHADER_GLSL
#elif defined(HAVE_CG)
#define DEFAULT_SHADER_TYPE RARCH_SHADER_CG
#else
#define DEFAULT_SHADER_TYPE RARCH_SHADER_NONE
#endif

#ifndef MAX_EGLIMAGE_TEXTURES
#define MAX_EGLIMAGE_TEXTURES 32
#endif

#define MAX_VARIABLES 64

#ifdef HAVE_THREADS
#define VIDEO_DRIVER_IS_THREADED_INTERNAL(video_st) ((!video_driver_is_hw_context() && video_st->threaded) ? true : false)
#define VIDEO_DRIVER_LOCK(video_st) \
   if (video_st->display_lock) \
      slock_lock(video_st->display_lock)

#define VIDEO_DRIVER_UNLOCK(video_st) \
   if (video_st->display_lock) \
      slock_unlock(video_st->display_lock)

#define VIDEO_DRIVER_CONTEXT_LOCK(video_st) \
   if (video_st->context_lock) \
      slock_lock(video_st->context_lock)

#define VIDEO_DRIVER_CONTEXT_UNLOCK(video_st) \
   if (video_st->context_lock) \
      slock_unlock(video_st->context_lock)

#define VIDEO_DRIVER_LOCK_FREE(video_st) \
   slock_free(video_st->display_lock); \
   slock_free(video_st->context_lock); \
   video_st->display_lock = NULL; \
   video_st->context_lock = NULL

#define VIDEO_DRIVER_THREADED_LOCK(video_st, is_threaded) \
   if (is_threaded) \
      VIDEO_DRIVER_LOCK(video_st)

#define VIDEO_DRIVER_THREADED_UNLOCK(video_st, is_threaded) \
   if (is_threaded) \
      VIDEO_DRIVER_UNLOCK(video_st)
#define VIDEO_DRIVER_GET_PTR_INTERNAL(video_st) ((VIDEO_DRIVER_IS_THREADED_INTERNAL(video_st)) ? video_thread_get_ptr(video_st) : video_st->data)
#else
#define VIDEO_DRIVER_IS_THREADED_INTERNAL(video_st) (false)
#define VIDEO_DRIVER_LOCK(video_st)            ((void)0)
#define VIDEO_DRIVER_UNLOCK(video_st)          ((void)0)
#define VIDEO_DRIVER_LOCK_FREE(video_st)       ((void)0)
#define VIDEO_DRIVER_THREADED_LOCK(video_st, is_threaded)   ((void)0)
#define VIDEO_DRIVER_THREADED_UNLOCK(video_st, is_threaded) ((void)0)
#define VIDEO_DRIVER_CONTEXT_LOCK(video_st)    ((void)0)
#define VIDEO_DRIVER_CONTEXT_UNLOCK(video_st)  ((void)0)
#define VIDEO_DRIVER_GET_PTR_INTERNAL(video_st) (video_st->data)
#endif

#define VIDEO_DRIVER_GET_HW_CONTEXT_INTERNAL(video_st) (&video_st->hw_render)

#define VIDEO_HAS_FOCUS(video_st) (video_st->current_video->focus ? (video_st->current_video->focus(video_st->data)) : true)

RETRO_BEGIN_DECLS

struct LinkInfo
{
   struct video_shader_pass *pass;
   unsigned tex_w, tex_h;
};


struct shader_program_info
{
   void *data;
   const char *vertex;
   const char *fragment;
   const char *combined;
   unsigned idx;
   bool is_file;
};

struct uniform_info
{
   bool enabled;

   int32_t location;
   int32_t count;
   unsigned type; /* shader uniform type */

   struct
   {
      enum shader_program_type type;
      const char *ident;
      uint32_t idx;
      bool add_prefix;
      bool enable;
   } lookup;

   struct
   {
      float *floatv;
      intptr_t *integerv;
      uintptr_t *unsigned_integerv;

      struct
      {
         intptr_t v0;
         intptr_t v1;
         intptr_t v2;
         intptr_t v3;
      } integer;

      struct
      {
         uintptr_t v0;
         uintptr_t v1;
         uintptr_t v2;
         uintptr_t v3;
      } unsigned_integer;

      struct
      {
         float v0;
         float v1;
         float v2;
         float v3;
      } f;

   } result;
};

typedef struct shader_backend
{
   void *(*init)(void *data, const char *path);
   void (*init_menu_shaders)(void *data);
   void (*deinit)(void *data);

   /* Set shader parameters. */
   void (*set_params)(void *data, void *shader_data);

   void (*set_uniform_parameter)(void *data, struct uniform_info *param,
         void *uniform_data);

   /* Compile a shader program. */
   bool (*compile_program)(void *data, unsigned idx,
         void *program_data, struct shader_program_info *program_info);

   /* Use a shader program specified by variable 'index'. */
   void (*use)(void *data, void *shader_data, unsigned index, bool set_active);

   /* Returns the number of currently loaded shaders. */
   unsigned (*num_shaders)(void *data);

   bool (*filter_type)(void *data, unsigned index, bool *smooth);
   enum gfx_wrap_type (*wrap_type)(void *data, unsigned index);
   void (*shader_scale)(void *data,
         unsigned index, struct gfx_fbo_scale *scale);
   bool (*set_coords)(void *shader_data, const struct video_coords *coords);
   bool (*set_mvp)(void *shader_data, const void *mat_data);
   unsigned (*get_prev_textures)(void *data);
   bool (*get_feedback_pass)(void *data, unsigned *pass);
   bool (*mipmap_input)(void *data, unsigned index);

   struct video_shader *(*get_current_shader)(void *data);

   void (*get_flags)(uint32_t*);

   enum rarch_shader_type type;

   /* Human readable string. */
   const char *ident;
} shader_backend_t;

typedef struct video_shader_ctx_init
{
   const char *path;
   const shader_backend_t *shader;
   void *data;
   void *shader_data;
   enum rarch_shader_type shader_type;
   struct
   {
      bool core_context_enabled;
   } gl;
} video_shader_ctx_init_t;

typedef struct video_shader_ctx_params
{
   void *data;
   const void *info;
   const void *prev_info;
   const void *feedback_info;
   const void *fbo_info;
   unsigned width;
   unsigned height;
   unsigned tex_width;
   unsigned tex_height;
   unsigned out_width;
   unsigned out_height;
   unsigned frame_counter;
   unsigned fbo_info_cnt;
} video_shader_ctx_params_t;

typedef struct video_shader_ctx_coords
{
   void *handle_data;
   const void *data;
} video_shader_ctx_coords_t;

typedef struct video_shader_ctx_scale
{
   struct gfx_fbo_scale *scale;
   unsigned idx;
} video_shader_ctx_scale_t;

typedef struct video_shader_ctx_info
{
   void *data;
   unsigned num;
   unsigned idx;
   bool set_active;
} video_shader_ctx_info_t;

typedef struct video_shader_ctx_mvp
{
   void *data;
   const void *matrix;
} video_shader_ctx_mvp_t;

typedef struct video_shader_ctx_filter
{
   bool *smooth;
   unsigned index;
} video_shader_ctx_filter_t;

typedef struct video_shader_ctx
{
   struct video_shader *data;
} video_shader_ctx_t;

typedef struct video_shader_ctx_texture
{
   unsigned id;
} video_shader_ctx_texture_t;

typedef struct video_pixel_scaler
{
   struct scaler_ctx *scaler;
   void *scaler_out;
} video_pixel_scaler_t;

typedef void (*gfx_ctx_proc_t)(void);

typedef struct video_info
{
   const char *path_font;

   uintptr_t parent;

   int swap_interval;


   /* Width of window.
    * If fullscreen mode is requested,
    * a width of 0 means the resolution of the
    * desktop should be used. */
   unsigned width;

   /* Height of window.
    * If fullscreen mode is requested,
    * a height of 0 means the resolutiof the desktop should be used.
    */
   unsigned height;

#ifdef GEKKO
   /* TODO - we can't really have driver system-specific
    * variables in here. There should be some
    * kind of publicly accessible driver implementation
    * video struct for specific things like this.
    */

   /* Wii-specific settings. Ignored for everything else. */
   unsigned viwidth;
#endif

   /*
    * input_scale defines the maximum size of the picture that will
    * ever be used with the frame callback.
    *
    * The maximum resolution is a multiple of 256x256 size (RARCH_SCALE_BASE),
    * so an input scale of 2 means you should allocate a texture or of 512x512.
    *
    * Maximum input size: RARCH_SCALE_BASE * input_scale
    */
   unsigned input_scale;

   float font_size;

   bool adaptive_vsync;

#ifdef GEKKO
   bool vfilter;
#endif

   /* If true, applies bilinear filtering to the image,
    * otherwise nearest filtering. */
   bool smooth;

   bool ctx_scaling;

   bool is_threaded;

   /* Use 32bit RGBA rather than native RGB565/XBGR1555.
    *
    * XRGB1555 format is 16-bit and has byte ordering: 0RRRRRGGGGGBBBBB,
    * in native endian.
    *
    * ARGB8888 is AAAAAAAARRRRRRRRGGGGGGGGBBBBBBBB, native endian.
    * Alpha channel should be disregarded.
    * */
   bool rgb32;

   /* Launch in fullscreen mode instead of windowed mode. */
   bool fullscreen;

   /* Start with V-Sync enabled. */
   bool vsync;

   /* If true, the output image should have the aspect ratio
    * as set in aspect_ratio. */
   bool force_aspect;

   bool font_enable;
} video_info_t;

typedef struct video_frame_info
{
   void *userdata;
   void *widgets_userdata;
   void *disp_userdata;

   int custom_vp_x;
   int custom_vp_y;
   int crt_switch_center_adjust;
   int crt_switch_porch_adjust;

   unsigned hard_sync_frames;
   unsigned aspect_ratio_idx;
   unsigned max_swapchain_images;
   unsigned monitor_index;
   unsigned crt_switch_resolution;
   unsigned crt_switch_resolution_super;
   unsigned width;
   unsigned height;
   unsigned xmb_theme;
   unsigned xmb_color_theme;
   unsigned menu_shader_pipeline;
   unsigned materialui_color_theme;
   unsigned ozone_color_theme;
   unsigned custom_vp_width;
   unsigned custom_vp_height;
   unsigned custom_vp_full_width;
   unsigned custom_vp_full_height;
   unsigned black_frame_insertion;
   unsigned fps_update_interval;
   unsigned memory_update_interval;

   float menu_wallpaper_opacity;
   float menu_framebuffer_opacity;
   float menu_header_opacity;
   float menu_footer_opacity;
   float refresh_rate;
   float font_msg_pos_x;
   float font_msg_pos_y;
   float font_msg_color_r;
   float font_msg_color_g;
   float font_msg_color_b;
   float xmb_alpha_factor;


   struct
   {
      /* Drop shadow offset.
       * If both are 0, no drop shadow will be rendered. */
      int drop_x, drop_y;
      /* ABGR. Use the macros. */
      uint32_t color;
      float x;
      float y;
      float scale;
      /* Drop shadow color multiplier. */
      float drop_mod;
      /* Drop shadow alpha */
      float drop_alpha;
      enum text_alignment text_align;
      bool full_screen;
   } osd_stat_params;

   char stat_text[512];

   bool widgets_active;
   bool notifications_hidden;
   bool menu_mouse_enable;
   bool widgets_is_paused;
   bool widgets_is_fast_forwarding;
   bool widgets_is_rewinding;
   bool input_menu_swap_ok_cancel_buttons;
   bool input_driver_nonblock_state;
   bool input_driver_grab_mouse_state;
   bool hard_sync;
   bool fps_show;
   bool memory_show;
   bool statistics_show;
   bool framecount_show;
   bool core_status_msg_show;
   bool post_filter_record;
   bool windowed_fullscreen;
   bool fullscreen;
   bool font_enable;
   bool use_rgba;
   bool hdr_support;
   bool libretro_running;
   bool xmb_shadows_enable;
   bool battery_level_enable;
   bool timedate_enable;
   bool runloop_is_slowmotion;
   bool runloop_is_paused;
   bool fastforward_frameskip;
   bool menu_is_alive;
   bool menu_screensaver_active;
   bool msg_bgcolor_enable;
   bool crt_switch_hires_menu;
   bool hdr_enable;
   bool overlay_behind_menu;
} video_frame_info_t;

typedef void (*update_window_title_cb)(void*);
typedef bool (*get_metrics_cb)(void *data, enum display_metric_types type,
      float *value);
typedef bool (*set_resize_cb)(void*, unsigned, unsigned);

typedef struct gfx_ctx_driver
{
   /* The opaque pointer is the underlying video driver data (e.g. gl_t for
    * OpenGL contexts). Although not advised, the context driver is allowed
    * to hold a pointer to it as the context never outlives the video driver.
    *
    * The context driver is responsible for it's own data.*/
   void* (*init)(void *video_driver);
   void (*destroy)(void *data);

   enum gfx_ctx_api (*get_api)(void *data);

   /* Which API to bind to. */
   bool (*bind_api)(void *video_driver, enum gfx_ctx_api,
         unsigned major, unsigned minor);

   /* Sets the swap interval. */
   void (*swap_interval)(void *data, int);

   /* Sets video mode. Creates a window, etc. */
   bool (*set_video_mode)(void*, unsigned, unsigned, bool);

   /* Gets current window size.
    * If not initialized yet, it returns current screen size. */
   void (*get_video_size)(void*, unsigned*, unsigned*);

   float (*get_refresh_rate)(void*);

   void (*get_video_output_size)(void*, unsigned*, unsigned*, char *, size_t);

   void (*get_video_output_prev)(void*);

   void (*get_video_output_next)(void*);

   get_metrics_cb get_metrics;

   /* Translates a window size to an aspect ratio.
    * In most cases this will be just width / height, but
    * some contexts will better know which actual aspect ratio is used.
    * This can be NULL to assume the default behavior.
    */
   float (*translate_aspect)(void*, unsigned, unsigned);

   /* Asks driver to update window title (FPS, etc). */
   update_window_title_cb update_window_title;

   /* Queries for resize and quit events.
    * Also processes events. */
   void (*check_window)(void*, bool*, bool*,
         unsigned*, unsigned*);

   /* Acknowledge a resize event. This is needed for some APIs.
    * Most backends will ignore this. */
   set_resize_cb set_resize;

   /* Checks if window has input focus. */
   bool (*has_focus)(void*);

   /* Should the screensaver be suppressed? */
   bool (*suppress_screensaver)(void *data, bool enable);

   /* Checks if context driver has windowed support. */
   bool has_windowed;

   /* Swaps buffers. VBlank sync depends on
    * earlier calls to swap_interval. */
   void (*swap_buffers)(void*);

   /* Most video backends will want to use a certain input driver.
    * Checks for it here. */
   void (*input_driver)(void*, const char *, input_driver_t**, void**);

   /* Wraps whatever gl_proc_address() there is.
    * Does not take opaque, to avoid lots of ugly wrapper code. */
   gfx_ctx_proc_t (*get_proc_address)(const char*);

   /* Returns true if this context supports EGLImage buffers for
    * screen drawing and was initalized correctly. */
   bool (*image_buffer_init)(void*, const video_info_t*);

   /* Writes the frame to the EGLImage and sets image_handle to it.
    * Returns true if a new image handle is created.
    * Always returns true the first time it's called for a new index.
    * The graphics core must handle a change in the handle correctly. */
   bool (*image_buffer_write)(void*, const void *frame, unsigned width,
         unsigned height, unsigned pitch, bool rgb32,
         unsigned index, void **image_handle);

   /* Shows or hides mouse. Can be NULL if context doesn't
    * have a concept of mouse pointer. */
   void (*show_mouse)(void *data, bool state);

   /* Human readable string. */
   const char *ident;

   uint32_t (*get_flags)(void *data);

   void     (*set_flags)(void *data, uint32_t flags);

   /* Optional. Binds HW-render offscreen context. */
   void (*bind_hw_render)(void *data, bool enable);

   /* Optional. Gets base data for the context which is used by the driver.
    * This is mostly relevant for graphics APIs such as Vulkan
    * which do not have global context state. */
   void *(*get_context_data)(void *data);

   /* Optional. Makes driver context (only GL right now)
    * active for this thread. */
   void (*make_current)(bool release);
} gfx_ctx_driver_t;

typedef struct gfx_ctx_size
{
   bool *quit;
   bool *resize;
   unsigned *width;
   unsigned *height;
} gfx_ctx_size_t;

typedef struct gfx_ctx_mode
{
   unsigned width;
   unsigned height;
   bool fullscreen;
} gfx_ctx_mode_t;

typedef struct gfx_ctx_metrics
{
   float *value;
   enum display_metric_types type;
} gfx_ctx_metrics_t;

typedef struct gfx_ctx_aspect
{
   float *aspect;
   unsigned width;
   unsigned height;
} gfx_ctx_aspect_t;

typedef struct gfx_ctx_input
{
   input_driver_t **input;
   void **input_data;
} gfx_ctx_input_t;

typedef struct gfx_ctx_ident
{
   const char *ident;
} gfx_ctx_ident_t;

struct aspect_ratio_elem
{
   float value;
   char name[64];
};

/* Optionally implemented interface to poke more
 * deeply into video driver. */

typedef struct video_poke_interface
{
   uint32_t (*get_flags)(void *data);
   uintptr_t (*load_texture)(void *video_data, void *data,
         bool threaded, enum texture_filter_type filter_type);
   void (*unload_texture)(void *data, bool threaded, uintptr_t id);
   void (*set_video_mode)(void *data, unsigned width,
         unsigned height, bool fullscreen);
   float (*get_refresh_rate)(void *data);
   void (*set_filtering)(void *data, unsigned index, bool smooth, bool ctx_scaling);
   void (*get_video_output_size)(void *data,
         unsigned *width, unsigned *height, char *desc, size_t desc_len);

   /* Move index to previous resolution */
   void (*get_video_output_prev)(void *data);

   /* Move index to next resolution */
   void (*get_video_output_next)(void *data);

   uintptr_t (*get_current_framebuffer)(void *data);
   retro_proc_address_t (*get_proc_address)(void *data, const char *sym);
   void (*set_aspect_ratio)(void *data, unsigned aspectratio_index);
   void (*apply_state_changes)(void *data);

   /* Update texture. */
   void (*set_texture_frame)(void *data, const void *frame, bool rgb32,
         unsigned width, unsigned height, float alpha);
   /* Enable or disable rendering. */
   void (*set_texture_enable)(void *data, bool enable, bool full_screen);
   void (*set_osd_msg)(void *data, 
         const char *msg,
         const void *params, void *font);

   void (*show_mouse)(void *data, bool state);
   void (*grab_mouse_toggle)(void *data);

   struct video_shader *(*get_current_shader)(void *data);
   bool (*get_current_software_framebuffer)(void *data,
         struct retro_framebuffer *framebuffer);
   bool (*get_hw_render_interface)(void *data,
         const struct retro_hw_render_interface **iface);

   /* hdr settings */ 
   void (*set_hdr_max_nits)(void *data, float max_nits);
   void (*set_hdr_paper_white_nits)(void *data, float paper_white_nits);
   void (*set_hdr_contrast)(void *data, float contrast);
   void (*set_hdr_expand_gamut)(void *data, bool expand_gamut);         
} video_poke_interface_t;

/* msg is for showing a message on the screen
 * along with the video frame. */
typedef bool (*video_driver_frame_t)(void *data,
      const void *frame, unsigned width,
      unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info);

typedef struct video_driver
{
   /* Should the video driver act as an input driver as well?
    * The video initialization might preinitialize an input driver
    * to override the settings in case the video driver relies on
    * input driver for event handling. */
   void *(*init)(const video_info_t *video,
         input_driver_t **input,
         void **input_data);

   /* Updates frame on the screen.
    * Frame can be either XRGB1555, RGB565 or ARGB32 format
    * depending on rgb32 setting in video_info_t.
    * Pitch is the distance in bytes between two scanlines in memory.
    *
    * When msg is non-NULL,
    * it's a message that should be displayed to the user. */
   video_driver_frame_t frame;

   /* Should we care about syncing to vblank? Fast forwarding.
    *
    * Requests nonblocking operation.
    *
    * True = VSync is turned off.
    * False = VSync is turned on.
    * */
   void (*set_nonblock_state)(void *data, bool toggle,
         bool adaptive_vsync_enabled,
         unsigned swap_interval);

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
   bool (*read_viewport)(void *data, uint8_t *buffer, bool is_idle);

   /* Returns a pointer to a newly allocated buffer that can
    * (and must) be passed to free() by the caller, containing a
    * copy of the current raw frame in the active pixel format
    * and sets width, height and pitch to the correct values. */
   void* (*read_frame_raw)(void *data, unsigned *width,
   unsigned *height, size_t *pitch);

#ifdef HAVE_OVERLAY
   void (*overlay_interface)(void *data,
         const video_overlay_interface_t **iface);
#endif
#ifdef HAVE_VIDEO_LAYOUT
   const video_layout_render_interface_t *(*video_layout_render_interface)(void *data);
#endif
   void (*poke_interface)(void *data, const video_poke_interface_t **iface);
   unsigned (*wrap_type_to_enum)(enum gfx_wrap_type type);

#if defined(HAVE_GFX_WIDGETS)
   /* if set to true, will use display widgets when applicable
    * if set to false, will use OSD as a fallback */
   bool (*gfx_widgets_enabled)(void *data);
#endif
} video_driver_t;

typedef struct
{
#ifdef HAVE_CRTSWITCHRES
   videocrt_switch_t crt_switch_st;     /* double alignment */
#endif
   struct retro_system_av_info av_info; /* double alignment */
   retro_time_t frame_time_samples[MEASURE_FRAME_TIME_SAMPLES_COUNT];
   retro_time_t core_frame_time;
   uint64_t frame_time_count;
   uint64_t frame_count;
   uint8_t *record_gpu_buffer;
#ifdef HAVE_VIDEO_FILTER
   rarch_softfilter_t *state_filter;
   void               *state_buffer;
#endif
   void *data;
   video_driver_t *current_video;
   /* Interface for "poking". */
   const video_poke_interface_t *poke;
   gfx_ctx_driver_t current_video_context;               /* ptr alignment */
   struct retro_hw_render_callback hw_render;            /* ptr alignment */
   struct rarch_dir_shader_list dir_shader_list;         /* ptr alignment */
#ifdef HAVE_THREADS
   slock_t *display_lock;
   slock_t *context_lock;
#endif

   /* Used for 15-bit -> 16-bit conversions that take place before
    * being passed to video driver. */
   video_pixel_scaler_t *scaler_ptr;
   video_driver_frame_t frame_bak;  /* ptr alignment */

   void *current_display_server_data;

   const void *frame_cache_data;

   const struct
      retro_hw_render_context_negotiation_interface *
      hw_render_context_negotiation;

#ifdef HAVE_MENU
   struct video_shader *menu_driver_shader;
#endif

   void *context_data;

   /* Opaque handles to currently running window.
    * Used by e.g. input drivers which bind to a window.
    * Drivers are responsible for setting these if an input driver
    * could potentially make use of this. */
   uintptr_t display_userdata;
   uintptr_t display;
   uintptr_t window;

   size_t frame_cache_pitch;
   size_t window_title_len;

#ifdef HAVE_VIDEO_FILTER
   unsigned state_scale;
   unsigned state_out_bpp;
#endif
   unsigned frame_delay_target;
   unsigned frame_delay_effective;
   unsigned frame_cache_width;
   unsigned frame_cache_height;
   unsigned width;
   unsigned height;

   float core_hz;
   float aspect_ratio;
   float video_refresh_rate_original;

   enum retro_pixel_format pix_fmt;
   enum rarch_display_type display_type;
   enum rotation initial_screen_orientation;
   enum rotation current_screen_orientation;

   /**
    * dynamic.c:dynamic_request_hw_context will try to set flag data when the context
    * is in the middle of being rebuilt; in these cases we will save flag
    * data and set this to true.
    * When the context is reinit, it checks this, reads from
    * deferred_flag_data and cleans it.
    *
    * TODO - Dirty hack, fix it better
    */
   gfx_ctx_flags_t deferred_flag_data;          /* uint32_t alignment */

   char cli_shader_path[PATH_MAX_LENGTH];
   char window_title[512];
   char gpu_device_string[128];
   char gpu_api_version_string[128];
   char title_buf[64];
   char cached_driver_id[32];

   /**
    * dynamic.c:dynamic_request_hw_context will try to set
    * flag data when the context
    * is in the middle of being rebuilt; in these cases we will save flag
    * data and set this to true.
    * When the context is reinit, it checks this, reads from
    * deferred_flag_data and cleans it.
    *
    * TODO - Dirty hack, fix it better
    */
   bool deferred_video_context_driver_set_flags;
   bool window_title_update;
#ifdef HAVE_GFX_WIDGETS
   bool widgets_paused;
   bool widgets_fast_forward;
   bool widgets_rewinding;
#endif
   bool started_fullscreen;

   /* Graphics driver requires RGBA byte order data (ABGR on little-endian)
    * for 32-bit.
    * This takes effect for overlay and shader cores that wants to load
    * data into graphics driver. Kinda hackish to place it here, it is only
    * used for GLES.
    * TODO: Refactor this better. */
   bool use_rgba;

   /* Graphics driver supports HDR displays
    * Currently only D3D11/D3D12 supports HDR displays and 
    * whether we've enabled it */
   bool hdr_support;

   /* If set during context deinit, the driver should keep
    * graphics context alive to avoid having to reset all
    * context state. */
   bool cache_context;

   /* Set to true by driver if context caching succeeded. */
   bool cache_context_ack;

   bool active;
#ifdef HAVE_VIDEO_FILTER
   bool state_out_rgb32;
#endif
   bool crt_switching_active;
   bool force_fullscreen;
   bool threaded;
   bool is_switching_display_mode;
   bool shader_presets_need_reload;
   bool cli_shader_disable;
#ifdef HAVE_RUNAHEAD
   bool runahead_is_active;
#endif
} video_driver_state_t;

typedef struct video_frame_delay_auto {
   float refresh_rate;
   unsigned frame_time_interval;
   unsigned decrease;
   unsigned target;
   unsigned time;
} video_frame_delay_auto_t;

extern struct aspect_ratio_elem aspectratio_lut[ASPECT_RATIO_END];

bool video_driver_has_windowed(void);

bool video_driver_has_focus(void);

bool video_driver_cached_frame_has_valid_framebuffer(void);

void video_driver_set_cached_frame_ptr(const void *data);

void video_driver_set_stub_frame(void);

void video_driver_unset_stub_frame(void);

bool video_driver_supports_viewport_read(void);

bool video_driver_prefer_viewport_read(void);

bool video_driver_supports_read_frame_raw(void);

void video_driver_set_viewport_core(void);

void video_driver_reset_custom_viewport(settings_t *settings);

void video_driver_set_rgba(void);

void video_driver_unset_rgba(void);

bool video_driver_supports_rgba(void);

void video_driver_set_hdr_support(void);

void video_driver_unset_hdr_support(void);

bool video_driver_supports_hdr(void);

unsigned video_driver_get_hdr_color(unsigned color);

float video_driver_get_hdr_luminance(float nits);

unsigned video_driver_get_hdr_paper_white(void);

float* video_driver_get_hdr_paper_white_float(void);

bool video_driver_get_next_video_out(void);

bool video_driver_get_prev_video_out(void);

void video_driver_monitor_reset(void);

void video_driver_set_aspect_ratio(void);

void video_driver_update_viewport(struct video_viewport* vp, bool force_full, bool keep_aspect);

void video_driver_show_mouse(void);

void video_driver_hide_mouse(void);

void video_driver_apply_state_changes(void);

bool video_driver_read_viewport(uint8_t *buffer, bool is_idle);

void video_driver_cached_frame(void);

bool video_driver_is_hw_context(void);

struct retro_hw_render_callback *video_driver_get_hw_context(void);

const struct retro_hw_render_context_negotiation_interface
*video_driver_get_context_negotiation_interface(void);

bool video_driver_is_video_cache_context(void);

void video_driver_set_video_cache_context_ack(void);

bool video_driver_get_viewport_info(struct video_viewport *viewport);

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

video_driver_state_t *video_state_get_ptr(void);

bool video_driver_set_rotation(unsigned rotation);

bool video_driver_set_video_mode(unsigned width,
      unsigned height, bool fullscreen);

bool video_driver_get_video_output_size(
      unsigned *width, unsigned *height, char *desc, size_t desc_len);

void video_driver_set_texture_enable(bool enable, bool full_screen);

void video_driver_set_texture_frame(const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha);

#ifdef HAVE_VIDEO_LAYOUT
const video_layout_render_interface_t *video_driver_layout_render_interface(void);
#endif

void * video_driver_read_frame_raw(unsigned *width,
   unsigned *height, size_t *pitch);

void video_driver_set_filtering(unsigned index, bool smooth, bool ctx_scaling);

void video_driver_set_hdr_max_nits(float max_nits);
void video_driver_set_hdr_paper_white_nits(float paper_white_nits);
void video_driver_set_hdr_contrast(float contrast);
void video_driver_set_hdr_expand_gamut(bool expand_gamut);

const char *video_driver_get_ident(void);

void video_driver_set_viewport(unsigned width, unsigned height,
      bool force_fullscreen, bool allow_rotate);

void video_driver_get_size(unsigned *width, unsigned *height);

void video_driver_set_size(unsigned width, unsigned height);

float video_driver_get_aspect_ratio(void);

void video_driver_set_aspect_ratio_value(float value);

enum retro_pixel_format video_driver_get_pixel_format(void);

void video_driver_cached_frame_set(const void *data, unsigned width,
      unsigned height, size_t pitch);

void video_driver_cached_frame_get(const void **data, unsigned *width,
      unsigned *height, size_t *pitch);

void video_driver_menu_settings(void **list_data, void *list_info_data,
      void *group_data, void *subgroup_data, const char *parent_group);

/**
 * video_viewport_get_scaled_integer:
 * @vp            : Viewport handle
 * @width         : Width.
 * @height        : Height.
 * @aspect_ratio  : Aspect ratio (in float).
 * @keep_aspect   : Preserve aspect ratio?
 *
 * Gets viewport scaling dimensions based on
 * scaled integer aspect ratio.
 **/
void video_viewport_get_scaled_integer(struct video_viewport *vp,
      unsigned width, unsigned height,
      float aspect_ratio, bool keep_aspect);

struct retro_system_av_info *video_viewport_get_system_av_info(void);

struct video_viewport *video_viewport_get_custom(void);

/**
 * video_monitor_set_refresh_rate:
 * @hz                 : New refresh rate for monitor.
 *
 * Sets monitor refresh rate to new value.
 **/
void video_monitor_set_refresh_rate(float hz);

/**
 * video_monitor_compute_fps_statistics:
 *
 * Computes monitor FPS statistics.
 **/
void video_monitor_compute_fps_statistics(uint64_t
      frame_time_count);

/**
 * video_monitor_fps_statistics
 * @refresh_rate       : Monitor refresh rate.
 * @deviation          : Deviation from measured refresh rate.
 * @sample_points      : Amount of sampled points.
 *
 * Gets the monitor FPS statistics based on the current
 * runtime.
 *
 * Returns: true (1) on success.
 * false (0) if:
 * a) threaded video mode is enabled
 * b) less than 2 frame time samples.
 * c) FPS monitor enable is off.
 **/
bool video_monitor_fps_statistics(double *refresh_rate,
      double *deviation, unsigned *sample_points);

bool video_driver_monitor_adjust_system_rates(
      float timing_skew_hz,
      float video_refresh_rate,
      bool vrr_runloop_enable,
      float audio_max_timing_skew,
      unsigned video_swap_interval,
      double input_fps);

void crt_switch_driver_refresh(void);

char* crt_switch_core_name(void);

#define video_driver_translate_coord_viewport_wrap(vp, mouse_x, mouse_y, res_x, res_y, res_screen_x, res_screen_y) \
   (video_driver_get_viewport_info(vp) ? video_driver_translate_coord_viewport(vp, mouse_x, mouse_y, res_x, res_y, res_screen_x, res_screen_y) : false)

/**
 * video_driver_translate_coord_viewport:
 * @mouse_x                        : Pointer X coordinate.
 * @mouse_y                        : Pointer Y coordinate.
 * @res_x                          : Scaled  X coordinate.
 * @res_y                          : Scaled  Y coordinate.
 * @res_screen_x                   : Scaled screen X coordinate.
 * @res_screen_y                   : Scaled screen Y coordinate.
 *
 * Translates pointer [X,Y] coordinates into scaled screen
 * coordinates based on viewport info.
 *
 * Returns: true (1) if successful, false if video driver doesn't support
 * viewport info.
 **/
bool video_driver_translate_coord_viewport(
      struct video_viewport *vp,
      int mouse_x, int mouse_y,
      int16_t *res_x, int16_t *res_y, int16_t *res_screen_x,
      int16_t *res_screen_y);

uintptr_t video_driver_display_userdata_get(void);

uintptr_t video_driver_display_get(void);

enum rarch_display_type video_driver_display_type_get(void);

uintptr_t video_driver_window_get(void);

void video_driver_display_type_set(enum rarch_display_type type);

void video_driver_display_set(uintptr_t idx);

void video_driver_display_userdata_set(uintptr_t idx);

void video_driver_window_set(uintptr_t idx);

bool video_driver_texture_load(void *data,
      enum texture_filter_type  filter_type,
      uintptr_t *id);

bool video_driver_texture_unload(uintptr_t *id);

void video_driver_build_info(video_frame_info_t *video_info);

void video_driver_reinit(int flags);

size_t video_driver_get_window_title(char *buf, unsigned len);

bool *video_driver_get_threaded(void);

void video_driver_set_threaded(bool val);

void video_frame_delay_auto(video_driver_state_t *video_st, video_frame_delay_auto_t *vfda);

/**
 * video_context_driver_init:
 * @core_set_shared_context : Boolean value that tells us whether shared context
 *                            is set.
 * @ctx                     : Graphics context driver to initialize.
 * @ident                   : Identifier of graphics context driver to find.
 * @api                     : API of higher-level graphics API.
 * @major                   : Major version number of higher-level graphics API.
 * @minor                   : Minor version number of higher-level graphics API.
 * @hw_render_ctx           : Request a graphics context driver capable of
 *                            hardware rendering?
 *
 * Initialize graphics context driver.
 *
 * Returns: graphics context driver if successfully initialized,
 * otherwise NULL.
 **/
const gfx_ctx_driver_t *video_context_driver_init(
      bool core_set_shared_context,
      settings_t *settings,
      void *data,
      const gfx_ctx_driver_t *ctx,
      const char *ident,
      enum gfx_ctx_api api, unsigned major,
      unsigned minor, bool hw_render_ctx,
      void **ctx_data);

/**
 * video_context_driver_init_first:
 * @data                    : Input data.
 * @ident                   : Identifier of graphics context driver to find.
 * @api                     : API of higher-level graphics API.
 * @major                   : Major version number of higher-level graphics API.
 * @minor                   : Minor version number of higher-level graphics API.
 * @hw_render_ctx           : Request a graphics context driver capable of
 *                            hardware rendering?
 *
 * Finds first suitable graphics context driver and initializes.
 *
 * Returns: graphics context driver if found, otherwise NULL.
 **/
const gfx_ctx_driver_t *video_context_driver_init_first(
      void *data, const char *ident,
      enum gfx_ctx_api api, unsigned major, unsigned minor,
      bool hw_render_ctx, void **ctx_data);

bool video_context_driver_set(const gfx_ctx_driver_t *data);

bool video_context_driver_get_ident(gfx_ctx_ident_t *ident);

bool video_context_driver_get_refresh_rate(float *refresh_rate);

bool video_context_driver_set_flags(gfx_ctx_flags_t *flags);

bool video_context_driver_get_metrics(gfx_ctx_metrics_t *metrics);

void video_context_driver_destroy(gfx_ctx_driver_t *ctx_driver);

enum gfx_ctx_api video_context_driver_get_api(void);

void video_context_driver_free(void);

bool video_shader_driver_get_current_shader(video_shader_ctx_t *shader);

float video_driver_get_refresh_rate(void);

#if defined(HAVE_GFX_WIDGETS)
bool video_driver_has_widgets(void);
#endif

bool video_driver_is_threaded(void);

bool video_context_driver_get_flags(gfx_ctx_flags_t *flags);

bool video_driver_test_all_flags(enum display_flags testflag);

gfx_ctx_flags_t video_driver_get_flags_wrapper(void);

void video_driver_set_gpu_device_string(const char *str);

const char* video_driver_get_gpu_device_string(void);

void video_driver_set_gpu_api_version_string(const char *str);

const char* video_driver_get_gpu_api_version_string(void);

void video_driver_force_fallback(const char *driver);

/* string list stays owned by the caller and must be available at all times after the video driver is inited */
void video_driver_set_gpu_api_devices(enum gfx_ctx_api api, struct string_list *list);

struct string_list* video_driver_get_gpu_api_devices(enum gfx_ctx_api api);

enum retro_hw_context_type hw_render_context_type(const char *s);

const char *hw_render_context_name(
      enum retro_hw_context_type type, int major, int minor);

video_driver_t *hw_render_context_driver(
      enum retro_hw_context_type type, int major, int minor);

void video_driver_pixel_converter_free(
      video_pixel_scaler_t *scalr);

video_pixel_scaler_t *video_driver_pixel_converter_init(
      const enum retro_pixel_format video_driver_pix_fmt,
      struct retro_hw_render_callback *hwr,
      unsigned size);

void recording_dump_frame(
      const void *data, unsigned width,
      unsigned height, size_t pitch, bool is_idle);

void video_driver_gpu_record_deinit(void);

void video_driver_init_filter(enum retro_pixel_format colfmt_int,
      settings_t *settings);

void video_context_driver_reset(void);

void video_driver_free_internal(void);

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

void video_driver_free_hw_context(void);

#ifdef HAVE_VIDEO_FILTER
void video_driver_filter_free(void);
#endif

#ifdef HAVE_THREADS
/**
 * video_thread_get_ptr:
 *
 * Gets the underlying video driver associated with the
 * threaded video wrapper. Sets @drv to the found
 * video driver.
 *
 * Returns: Video driver data of the video driver associated
 * with the threaded wrapper (if successful). If not successful,
 * NULL.
 **/
void *video_thread_get_ptr(video_driver_state_t *video_st);
#endif

void video_driver_lock_new(void);

bool video_driver_find_driver(
      void *settings_data,
      const char *prefix, bool verbosity_enabled);

void video_driver_restore_cached(void *settings_data);

void video_driver_set_viewport_config(
      struct retro_game_geometry *geom,
      float video_aspect_ratio,
      bool video_aspect_ratio_auto);

void video_driver_set_viewport_square_pixel(struct retro_game_geometry *geom);

bool video_driver_init_internal(bool *video_is_threaded, bool verbosity_enabled);

/**
 * video_driver_frame:
 * @data                 : pointer to data of the video frame.
 * @width                : width of the video frame.
 * @height               : height of the video frame.
 * @pitch                : pitch of the video frame.
 *
 * Video frame render callback function.
 **/
void video_driver_frame(const void *data, unsigned width,
      unsigned height, size_t pitch);

extern const video_driver_t *video_drivers[];

extern video_driver_t video_gl3;
extern video_driver_t video_gl2;
extern video_driver_t video_gl1;
extern video_driver_t video_vulkan;
extern video_driver_t video_metal;
extern video_driver_t video_psp1;
extern video_driver_t video_vita2d;
extern video_driver_t video_ps2;
extern video_driver_t video_ctr;
extern video_driver_t video_gcm;
extern video_driver_t video_switch;
extern video_driver_t video_d3d8;
extern video_driver_t video_d3d9_cg;
extern video_driver_t video_d3d9_hlsl;
extern video_driver_t video_d3d10;
extern video_driver_t video_d3d11;
extern video_driver_t video_d3d12;
extern video_driver_t video_gx;
extern video_driver_t video_wiiu;
extern video_driver_t video_xenon360;
extern video_driver_t video_xvideo;
extern video_driver_t video_sdl;
extern video_driver_t video_sdl2;
extern video_driver_t video_sdl_dingux;
extern video_driver_t video_sdl_rs90;
extern video_driver_t video_vg;
extern video_driver_t video_omap;
extern video_driver_t video_exynos;
extern video_driver_t video_dispmanx;
extern video_driver_t video_sunxi;
extern video_driver_t video_drm;
extern video_driver_t video_xshm;
extern video_driver_t video_caca;
extern video_driver_t video_gdi;
extern video_driver_t video_vga;
extern video_driver_t video_fpga;
extern video_driver_t video_sixel;
extern video_driver_t video_network;
extern video_driver_t video_oga;
extern video_driver_t video_null;

extern const gfx_ctx_driver_t gfx_ctx_osmesa;
extern const gfx_ctx_driver_t gfx_ctx_sdl_gl;
extern const gfx_ctx_driver_t gfx_ctx_x_egl;
extern const gfx_ctx_driver_t gfx_ctx_uwp;
extern const gfx_ctx_driver_t gfx_ctx_vk_wayland;
extern const gfx_ctx_driver_t gfx_ctx_wayland;
extern const gfx_ctx_driver_t gfx_ctx_x;
extern const gfx_ctx_driver_t gfx_ctx_vk_x;
extern const gfx_ctx_driver_t gfx_ctx_drm;
extern const gfx_ctx_driver_t gfx_ctx_go2_drm;
extern const gfx_ctx_driver_t gfx_ctx_mali_fbdev;
extern const gfx_ctx_driver_t gfx_ctx_vivante_fbdev;
extern const gfx_ctx_driver_t gfx_ctx_android;
extern const gfx_ctx_driver_t gfx_ctx_vk_android;
extern const gfx_ctx_driver_t gfx_ctx_ps3;
extern const gfx_ctx_driver_t gfx_ctx_w_vk;
extern const gfx_ctx_driver_t gfx_ctx_wgl;
extern const gfx_ctx_driver_t gfx_ctx_videocore;
extern const gfx_ctx_driver_t gfx_ctx_qnx;
extern const gfx_ctx_driver_t gfx_ctx_cgl;
extern const gfx_ctx_driver_t gfx_ctx_cocoagl;
extern const gfx_ctx_driver_t gfx_ctx_cocoavk;
extern const gfx_ctx_driver_t gfx_ctx_emscripten;
extern const gfx_ctx_driver_t gfx_ctx_opendingux_fbdev;
extern const gfx_ctx_driver_t gfx_ctx_khr_display;
extern const gfx_ctx_driver_t gfx_ctx_gdi;
extern const gfx_ctx_driver_t gfx_ctx_fpga;
extern const gfx_ctx_driver_t gfx_ctx_sixel;
extern const gfx_ctx_driver_t gfx_ctx_network;
extern const gfx_ctx_driver_t switch_ctx;
extern const gfx_ctx_driver_t orbis_ctx;
extern const gfx_ctx_driver_t vita_ctx;
extern const gfx_ctx_driver_t gfx_ctx_null;

extern const shader_backend_t gl_glsl_backend;
extern const shader_backend_t gl_cg_backend;

RETRO_END_DECLS

#endif
