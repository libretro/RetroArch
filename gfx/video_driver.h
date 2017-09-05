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

#ifndef __VIDEO_DRIVER__H
#define __VIDEO_DRIVER__H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <gfx/math/matrix_4x4.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HAVE_OVERLAY
#include "../input/input_overlay.h"
#endif

#include "video_defines.h"
#include "video_coord_array.h"
#include "video_filter.h"
#include "video_shader_parse.h"
#include "video_state_tracker.h"

#include "../input/input_driver.h"

#define RARCH_SCALE_BASE 256

#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL) || defined(HAVE_SLANG)
#ifndef HAVE_SHADER_MANAGER
#define HAVE_SHADER_MANAGER
#endif

#include "video_shader_parse.h"

#define VIDEO_SHADER_STOCK_BLEND (GFX_MAX_SHADERS - 1)
#define VIDEO_SHADER_MENU        (GFX_MAX_SHADERS - 2)
#define VIDEO_SHADER_MENU_2      (GFX_MAX_SHADERS - 3)
#define VIDEO_SHADER_MENU_3      (GFX_MAX_SHADERS - 4)
#define VIDEO_SHADER_MENU_4      (GFX_MAX_SHADERS - 5)
#define VIDEO_SHADER_MENU_5      (GFX_MAX_SHADERS - 6)

#endif

#if defined(_XBOX360)
#define DEFAULT_SHADER_TYPE RARCH_SHADER_HLSL
#elif defined(__PSL1GHT__) || defined(HAVE_OPENGLES2) || defined(HAVE_GLSL)
#define DEFAULT_SHADER_TYPE RARCH_SHADER_GLSL
#elif defined(__CELLOS_LV2__) || defined(HAVE_CG)
#define DEFAULT_SHADER_TYPE RARCH_SHADER_CG
#else
#define DEFAULT_SHADER_TYPE RARCH_SHADER_NONE
#endif

RETRO_BEGIN_DECLS

#ifndef MAX_EGLIMAGE_TEXTURES
#define MAX_EGLIMAGE_TEXTURES 32
#endif

#define MAX_VARIABLES 64

enum
{
   TEXTURES = 8,
   TEXTURESMASK = TEXTURES - 1
};

struct LinkInfo
{
   unsigned tex_w, tex_h;
   struct video_shader_pass *pass;
};

enum gfx_ctx_api
{
   GFX_CTX_NONE = 0,
   GFX_CTX_OPENGL_API,
   GFX_CTX_OPENGL_ES_API,
   GFX_CTX_DIRECT3D8_API,
   GFX_CTX_DIRECT3D9_API,
   GFX_CTX_OPENVG_API,
   GFX_CTX_VULKAN_API,
   GFX_CTX_GDI_API
};

enum display_metric_types
{
   DISPLAY_METRIC_NONE = 0,
   DISPLAY_METRIC_MM_WIDTH,
   DISPLAY_METRIC_MM_HEIGHT,
   DISPLAY_METRIC_DPI
};

enum display_flags
{
   GFX_CTX_FLAGS_NONE = 0,
   GFX_CTX_FLAGS_GL_CORE_CONTEXT,
   GFX_CTX_FLAGS_MULTISAMPLING,
   GFX_CTX_FLAGS_CUSTOMIZABLE_SWAPCHAIN_IMAGES
};

enum shader_uniform_type
{
   UNIFORM_1F = 0,
   UNIFORM_2F,
   UNIFORM_3F,
   UNIFORM_4F,
   UNIFORM_1FV,
   UNIFORM_2FV,
   UNIFORM_3FV,
   UNIFORM_4FV,
   UNIFORM_1I
};

enum shader_program_type
{
   SHADER_PROGRAM_VERTEX = 0,
   SHADER_PROGRAM_FRAGMENT,
   SHADER_PROGRAM_COMBINED
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
   unsigned type; /* shader uniform type */
   bool enabled;

   int32_t location;
   int32_t count;

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
      struct
      {
         intptr_t v0;
         intptr_t v1;
         intptr_t v2;
         intptr_t v3;
      } integer;

      intptr_t *integerv;

      struct
      {
         uintptr_t v0;
         uintptr_t v1;
         uintptr_t v2;
         uintptr_t v3;
      } unsigned_integer;

      uintptr_t *unsigned_integerv;

      struct
      {
         float v0;
         float v1;
         float v2;
         float v3;
      } f;

      float *floatv;
   } result;
};

typedef struct shader_backend
{
   void *(*init)(void *data, const char *path);
   void (*deinit)(void *data);

   /* Set shader parameters. */
   void (*set_params)(void *data, void *shader_data,
         unsigned width, unsigned height, 
         unsigned tex_width, unsigned tex_height, 
         unsigned out_width, unsigned out_height,
         unsigned frame_counter,
         const void *info, 
         const void *prev_info,
         const void *feedback_info,
         const void *fbo_info, unsigned fbo_info_cnt);

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
   bool (*set_coords)(void *handle_data,
         void *shader_data, const struct video_coords *coords);
   bool (*set_coords_fallback)(void *handle_data,
         void *shader_data, const struct video_coords *coords);
   bool (*set_mvp)(void *data, void *shader_data,
         const math_matrix_4x4 *mat);
   unsigned (*get_prev_textures)(void *data);
   bool (*get_feedback_pass)(void *data, unsigned *pass);
   bool (*mipmap_input)(void *data, unsigned index);

   struct video_shader *(*get_current_shader)(void *data);

   enum rarch_shader_type type;

   /* Human readable string. */
   const char *ident;
} shader_backend_t;

typedef struct video_shader_ctx_init
{
   enum rarch_shader_type shader_type;
   const shader_backend_t *shader;
   struct
   {
      bool core_context_enabled;
   } gl;
   void *data;
   const char *path;
} video_shader_ctx_init_t;

typedef struct video_shader_ctx_params
{
   void *data;
   unsigned width;
   unsigned height;
   unsigned tex_width;
   unsigned tex_height;
   unsigned out_width;
   unsigned out_height;
   unsigned frame_counter;
   const void *info;
   const void *prev_info;
   const void *feedback_info;
   const void *fbo_info;
   unsigned fbo_info_cnt;
} video_shader_ctx_params_t;

typedef struct video_shader_ctx_coords
{
   void *handle_data;
   const void *data;
} video_shader_ctx_coords_t;

typedef struct video_shader_ctx_scale
{
   unsigned idx;
   struct gfx_fbo_scale *scale;
} video_shader_ctx_scale_t;

typedef struct video_shader_ctx_info
{
   bool set_active;
   unsigned num;
   unsigned idx;
   void *data;
} video_shader_ctx_info_t;

typedef struct video_shader_ctx_mvp
{
   void *data;
   const math_matrix_4x4 *matrix;
} video_shader_ctx_mvp_t;

typedef struct video_shader_ctx_filter
{
   unsigned index;
   bool *smooth;
} video_shader_ctx_filter_t;

typedef struct video_shader_ctx_wrap
{
   unsigned idx;
   enum gfx_wrap_type type;
} video_shader_ctx_wrap_t;

typedef struct video_shader_ctx
{
   struct video_shader *data;
} video_shader_ctx_t;

typedef struct video_shader_ctx_ident
{
   const char *ident;
} video_shader_ctx_ident_t;

typedef struct video_shader_ctx_texture
{
   unsigned id;
} video_shader_ctx_texture_t;

typedef void (*gfx_ctx_proc_t)(void);

typedef struct video_info
{
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

   /* Launch in fullscreen mode instead of windowed mode. */
   bool fullscreen;

   /* Start with V-Sync enabled. */
   bool vsync;

   /* If true, the output image should have the aspect ratio 
    * as set in aspect_ratio. */
   bool force_aspect;

   unsigned swap_interval;

   bool font_enable;

#ifdef GEKKO
   /* TODO - we can't really have driver system-specific
    * variables in here. There should be some
    * kind of publicly accessible driver implementation
    * video struct for specific things like this.
    */

   /* Wii-specific settings. Ignored for everything else. */
   unsigned viwidth;
   bool vfilter;
#endif

   /* If true, applies bilinear filtering to the image,
    * otherwise nearest filtering. */
   bool smooth;

   bool is_threaded;

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

   /* Use 32bit RGBA rather than native RGB565/XBGR1555. 
    *
    * XRGB1555 format is 16-bit and has byte ordering: 0RRRRRGGGGGBBBBB,
    * in native endian.
    *
    * ARGB8888 is AAAAAAAARRRRRRRRGGGGGGGGBBBBBBBB, native endian.
    * Alpha channel should be disregarded.
    * */
   bool rgb32;
   
#ifndef RARCH_INTERNAL
   uintptr_t parent;
#endif
} video_info_t;

typedef struct video_frame_info
{
   bool input_driver_nonblock_state;
   bool shared_context;
   bool black_frame_insertion;
   bool hard_sync;
   bool fps_show;
   bool scale_integer;
   bool post_filter_record;
   bool windowed_fullscreen;
   bool fullscreen;
   bool font_enable;
   bool use_rgba;
   bool libretro_running;
   bool xmb_shadows_enable;
   bool battery_level_enable;
   bool timedate_enable;
   bool runloop_is_slowmotion;
   bool runloop_is_idle;
   bool runloop_is_paused;
   bool is_perfcnt_enable;
   bool menu_is_alive;

   int custom_vp_x;
   int custom_vp_y;

   unsigned hard_sync_frames;
   unsigned aspect_ratio_idx;
   unsigned max_swapchain_images;
   unsigned monitor_index;
   unsigned width;
   unsigned height;
   unsigned xmb_theme;
   unsigned xmb_color_theme;
   unsigned menu_shader_pipeline;
   unsigned materialui_color_theme;
   unsigned custom_vp_width;
   unsigned custom_vp_height;
   unsigned custom_vp_full_width;
   unsigned custom_vp_full_height;

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

   char fps_text[128];
   void (*cb_update_window_title)(void*, void *);
   void (*cb_swap_buffers)(void*, void *);
   bool (*cb_get_metrics)(void *data, enum display_metric_types type,
      float *value);
   bool (*cb_set_resize)(void*, unsigned, unsigned);

   void (*cb_shader_use)(void *data, void *shader_data, unsigned index, bool set_active);
   bool (*cb_shader_set_mvp)(void *data, void *shader_data,
         const math_matrix_4x4 *mat);

   void *context_data;
   void *shader_data;
} video_frame_info_t;

typedef void (*update_window_title_cb)(void*, void*);
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
   void* (*init)(video_frame_info_t *video_info, void *video_driver);
   void (*destroy)(void *data);

   /* Which API to bind to. */
   bool (*bind_api)(void *video_driver, enum gfx_ctx_api,
         unsigned major, unsigned minor);

   /* Sets the swap interval. */
   void (*swap_interval)(void *data, unsigned);

   /* Sets video mode. Creates a window, etc. */
   bool (*set_video_mode)(void*, video_frame_info_t *video_info, unsigned, unsigned, bool);

   /* Gets current window size.
    * If not initialized yet, it returns current screen size. */
   void (*get_video_size)(void*, unsigned*, unsigned*);

   void (*get_video_output_size)(void*, unsigned*, unsigned*);

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
         unsigned*, unsigned*, bool);

   /* Acknowledge a resize event. This is needed for some APIs.
    * Most backends will ignore this. */
   set_resize_cb set_resize;

   /* Checks if window has input focus. */
   bool (*has_focus)(void*);

   /* Should the screensaver be suppressed? */
   bool (*suppress_screensaver)(void *data, bool enable);

   /* Checks if context driver has windowed support. */
   bool (*has_windowed)(void*);

   /* Swaps buffers. VBlank sync depends on
    * earlier calls to swap_interval. */
   void (*swap_buffers)(void*, void *);

   /* Most video backends will want to use a certain input driver.
    * Checks for it here. */
   void (*input_driver)(void*, const char *, const input_driver_t**, void**);

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

   /* Optional. Makes driver context (only GLX right now)
    * active for this thread. */
   void (*make_current)(bool release);
} gfx_ctx_driver_t;

typedef struct gfx_ctx_flags
{
   uint32_t flags;
} gfx_ctx_flags_t;

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
   enum display_metric_types type;
   float *value;
} gfx_ctx_metrics_t;

typedef struct gfx_ctx_aspect
{
   float *aspect;
   unsigned width;
   unsigned height;
} gfx_ctx_aspect_t;

typedef struct gfx_ctx_image
{
   const void *frame;
   unsigned width;
   unsigned height;
   unsigned pitch;
   unsigned index;
   bool rgb32;
   void **handle;
} gfx_ctx_image_t;

typedef struct gfx_ctx_input
{
   const input_driver_t **input;
   void **input_data;
} gfx_ctx_input_t;

typedef struct gfx_ctx_proc_address
{
   const char *sym;
   retro_proc_address_t addr;
} gfx_ctx_proc_address_t;

typedef struct gfx_ctx_ident
{
   const char *ident;
} gfx_ctx_ident_t;

typedef struct video_viewport
{
   int x;
   int y;
   unsigned width;
   unsigned height;
   unsigned full_width;
   unsigned full_height;
} video_viewport_t;

struct aspect_ratio_elem
{
   char name[64];
   float value;
};

/* Optionally implemented interface to poke more
 * deeply into video driver. */

typedef struct video_poke_interface
{
   uintptr_t (*load_texture)(void *video_data, void *data,
         bool threaded, enum texture_filter_type filter_type);
   void (*unload_texture)(void *data, uintptr_t id);
   void (*set_video_mode)(void *data, unsigned width,
         unsigned height, bool fullscreen);
   void (*set_filtering)(void *data, unsigned index, bool smooth);
   void (*get_video_output_size)(void *data,
         unsigned *width, unsigned *height);

   /* Move index to previous resolution */
   void (*get_video_output_prev)(void *data);

   /* Move index to next resolution */
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
   void (*set_osd_msg)(void *data, video_frame_info_t *video_info,
         const char *msg,
         const void *params, void *font);

   void (*show_mouse)(void *data, bool state);
   void (*grab_mouse_toggle)(void *data);

   struct video_shader *(*get_current_shader)(void *data);
   bool (*get_current_software_framebuffer)(void *data,
         struct retro_framebuffer *framebuffer);
   bool (*get_hw_render_interface)(void *data,
         const struct retro_hw_render_interface **iface);
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
         const input_driver_t **input,
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
   void (*poke_interface)(void *data, const video_poke_interface_t **iface);
   unsigned (*wrap_type_to_enum)(enum gfx_wrap_type type);
} video_driver_t;

typedef struct d3d_renderchain_driver
{
   void (*chain_free)(void *data);
   void *(*chain_new)(void);
   bool (*reinit)(void *data, const void *info_data);
   bool (*init)(void *data,
         const void *video_info_data,
         void *dev_data,
         const void *final_viewport_data,
         const void *info_data,
         bool rgb32);
   void (*set_final_viewport)(void *data,
         void *renderchain_data, const void *viewport_data);
   bool (*add_pass)(void *data, const void *info_data);
   bool (*add_lut)(void *data,
         const char *id, const char *path,
         bool smooth);
   void (*add_state_tracker)(void *data, void *tracker_data);
   bool (*render)(void *chain_data, const void *data,
         unsigned width, unsigned height, unsigned pitch, unsigned rotation);
   void (*convert_geometry)(void *data, const void *info_data,
         unsigned *out_width, unsigned *out_height,
         unsigned width, unsigned height,
         void *final_viewport);
   void (*set_font_rect)(void *data, const void *param_data);
   bool (*read_viewport)(void *data, uint8_t *buffer, bool is_idle);
   void (*viewport_info)(void *data, struct video_viewport *vp);
   const char *ident;
} d3d_renderchain_driver_t;

typedef struct gl_renderchain_driver
{
   void (*init)(void *data, unsigned fbo_width, unsigned fbo_height);
   bool (*init_hw_render)(void *data, unsigned width, unsigned height);
   void (*free)(void *data);
   void (*deinit_hw_render)(void *data);
   void (*start_render)(void *data, video_frame_info_t *video_info);
   void (*check_fbo_dimensions)(void *data);
   void (*recompute_pass_sizes)(void *data,
         unsigned width, unsigned height,
         unsigned vp_width, unsigned vp_height);
   void (*renderchain_render)(void *data,
         video_frame_info_t *video_info,
         uint64_t frame_count,
         const struct video_tex_info *tex_info,
         const struct video_tex_info *feedback_info);
   const char *ident;
} gl_renderchain_driver_t;

extern struct aspect_ratio_elem aspectratio_lut[ASPECT_RATIO_END];

bool video_driver_has_windowed(void);

bool video_driver_cached_frame_has_valid_framebuffer(void);

void video_driver_destroy(void);
void video_driver_set_cached_frame_ptr(const void *data);
void video_driver_set_stub_frame(void);
void video_driver_unset_stub_frame(void);
bool video_driver_supports_recording(void);
bool video_driver_supports_viewport_read(void);
bool video_driver_supports_read_frame_raw(void);
void video_driver_set_viewport_config(void);
void video_driver_set_viewport_square_pixel(void);
void video_driver_set_viewport_core(void);
void video_driver_reset_custom_viewport(void);
void video_driver_set_rgba(void);
void video_driver_unset_rgba(void);
bool video_driver_supports_rgba(void);
bool video_driver_get_next_video_out(void);
bool video_driver_get_prev_video_out(void);
bool video_driver_init(bool *video_is_threaded);
void video_driver_destroy_data(void);
void video_driver_free(void);
void video_driver_free_hw_context(void);
void video_driver_monitor_reset(void);
void video_driver_set_aspect_ratio(void);
void video_driver_show_mouse(void);
void video_driver_hide_mouse(void);
void video_driver_set_nonblock_state(bool toggle);
bool video_driver_find_driver(void);
void video_driver_apply_state_changes(void);
bool video_driver_read_viewport(uint8_t *buffer, bool is_idle);
bool video_driver_cached_frame(void);
bool video_driver_frame_filter_alive(void);
bool video_driver_frame_filter_is_32bit(void);
void video_driver_default_settings(void);
void video_driver_load_settings(config_file_t *conf);
void video_driver_save_settings(config_file_t *conf);
void video_driver_set_own_driver(void);
void video_driver_unset_own_driver(void);
bool video_driver_owns_driver(void);
bool video_driver_is_hw_context(void);
struct retro_hw_render_callback *video_driver_get_hw_context(void);
const struct retro_hw_render_context_negotiation_interface 
*video_driver_get_context_negotiation_interface(void);
void video_driver_set_context_negotiation_interface(const struct 
      retro_hw_render_context_negotiation_interface *iface);
bool video_driver_is_video_cache_context(void);
void video_driver_set_video_cache_context_ack(void);
bool video_driver_is_video_cache_context_ack(void);
void video_driver_set_active(void);
bool video_driver_is_active(void);
bool video_driver_gpu_record_init(unsigned size);
void video_driver_gpu_record_deinit(void);
bool video_driver_get_current_software_framebuffer(struct 
      retro_framebuffer *fb);
bool video_driver_get_hw_render_interface(const struct 
      retro_hw_render_interface **iface);
bool video_driver_get_viewport_info(struct video_viewport *viewport);
void video_driver_set_title_buf(void);
void video_driver_monitor_adjust_system_rates(void);

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
 * Returns: Human-readable identifier of video driver at index. 
 * Can be NULL if nothing found.
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
void *video_driver_get_ptr(bool force_nonthreaded_data);

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

bool video_driver_set_rotation(unsigned rotation);

bool video_driver_set_video_mode(unsigned width,
      unsigned height, bool fullscreen);

bool video_driver_get_video_output_size(
      unsigned *width, unsigned *height);

void video_driver_set_osd_msg(const char *msg,
      const void *params, void *font);

void video_driver_set_texture_enable(bool enable, bool full_screen);

void video_driver_set_texture_frame(const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha);

#ifdef HAVE_OVERLAY
bool video_driver_overlay_interface(
      const video_overlay_interface_t **iface);
#endif

void * video_driver_read_frame_raw(unsigned *width,
   unsigned *height, size_t *pitch);

void video_driver_set_filtering(unsigned index, bool smooth);

const char *video_driver_get_ident(void);

bool video_driver_set_viewport(unsigned width, unsigned height,
      bool force_fullscreen, bool allow_rotate);

void video_driver_get_size(unsigned *width, unsigned *height);

void video_driver_set_size(unsigned *width, unsigned *height);

void video_driver_unset_video_cache_context_ack(void);

float video_driver_get_aspect_ratio(void);

void video_driver_set_aspect_ratio_value(float value);

rarch_softfilter_t *video_driver_frame_filter_get_ptr(void);

enum retro_pixel_format video_driver_get_pixel_format(void);

void video_driver_set_pixel_format(enum retro_pixel_format fmt);

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

unsigned video_pixel_get_alignment(unsigned pitch);

const video_poke_interface_t *video_driver_get_poke(void);

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
      void *data,
      int mouse_x, int mouse_y,
      int16_t *res_x, int16_t *res_y, int16_t *res_screen_x,
      int16_t *res_screen_y);

uintptr_t video_driver_display_get(void);

enum rarch_display_type video_driver_display_type_get(void);

uintptr_t video_driver_window_get(void);

void video_driver_display_type_set(enum rarch_display_type type);

void video_driver_display_set(uintptr_t idx);

void video_driver_window_set(uintptr_t idx);

bool video_driver_texture_load(void *data,
      enum texture_filter_type  filter_type,
      uintptr_t *id);

bool video_driver_texture_unload(uintptr_t *id);

void video_driver_build_info(video_frame_info_t *video_info);

void video_driver_reinit(void);

void video_driver_get_window_title(char *buf, unsigned len);

void video_driver_get_record_status(
      bool *has_gpu_record, 
      uint8_t **gpu_buf);

bool *video_driver_get_threaded(void);

void video_driver_set_threaded(bool val);

void video_driver_get_status(uint64_t *frame_count, bool * is_alive,
      bool *is_focused);

void video_driver_set_resize(unsigned width, unsigned height);

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
const gfx_ctx_driver_t *video_context_driver_init_first(void *data, const char *ident,
      enum gfx_ctx_api api, unsigned major, unsigned minor, bool hw_render_ctx);

bool video_context_driver_check_window(gfx_ctx_size_t *size_data);

bool video_context_driver_find_prev_driver(void);

bool video_context_driver_find_next_driver(void);

bool video_context_driver_init_image_buffer(const video_info_t *data);

bool video_context_driver_write_to_image_buffer(gfx_ctx_image_t *img);

bool video_context_driver_get_video_output_prev(void);

bool video_context_driver_get_video_output_next(void);

bool video_context_driver_bind_hw_render(bool *enable);

void video_context_driver_make_current(bool restore);

bool video_context_driver_set(const gfx_ctx_driver_t *data);

void video_context_driver_destroy(void);

bool video_context_driver_get_video_output_size(gfx_ctx_size_t *size_data);

bool video_context_driver_swap_interval(unsigned *interval);

bool video_context_driver_get_proc_address(gfx_ctx_proc_address_t *proc);

bool video_context_driver_suppress_screensaver(bool *bool_data);

bool video_context_driver_get_ident(gfx_ctx_ident_t *ident);

bool video_context_driver_set_video_mode(gfx_ctx_mode_t *mode_info);

bool video_context_driver_get_video_size(gfx_ctx_mode_t *mode_info);

bool video_context_driver_get_context_data(void *data);

bool video_context_driver_show_mouse(bool *bool_data);

void video_context_driver_set_data(void *data);

bool video_context_driver_get_flags(gfx_ctx_flags_t *flags);

bool video_context_driver_set_flags(gfx_ctx_flags_t *flags);

bool video_context_driver_get_metrics(gfx_ctx_metrics_t *metrics);

bool video_context_driver_translate_aspect(gfx_ctx_aspect_t *aspect);

bool video_context_driver_input_driver(gfx_ctx_input_t *inp);

void video_context_driver_free(void);

bool video_shader_driver_get_prev_textures(video_shader_ctx_texture_t *texture);

bool video_shader_driver_get_ident(video_shader_ctx_ident_t *ident);

bool video_shader_driver_get_current_shader(video_shader_ctx_t *shader);

bool video_shader_driver_direct_get_current_shader(video_shader_ctx_t *shader);

bool video_shader_driver_deinit(void);

#define video_shader_driver_set_parameter(param) \
   if (current_shader && current_shader->set_uniform_parameter) \
      current_shader->set_uniform_parameter(shader_data, &param, NULL)

#define video_shader_driver_set_parameters(params) \
   current_shader->set_params(params.data, shader_data, params.width, params.height, params.tex_width, params.tex_height, params.out_width, params.out_height, params.frame_counter, params.info, params.prev_info, params.feedback_info, params.fbo_info, params.fbo_info_cnt)

bool video_shader_driver_init_first(void);

bool video_shader_driver_init(video_shader_ctx_init_t *init);

bool video_shader_driver_get_feedback_pass(unsigned *data);

bool video_shader_driver_mipmap_input(unsigned *index);

#define video_shader_driver_set_coords(coords) \
   if (!current_shader->set_coords(coords.handle_data, shader_data, (const struct video_coords*)coords.data) && current_shader->set_coords_fallback) \
      current_shader->set_coords_fallback(coords.handle_data, shader_data, (const struct video_coords*)coords.data)

bool video_shader_driver_scale(video_shader_ctx_scale_t *scaler);

bool video_shader_driver_info(video_shader_ctx_info_t *shader_info);

#define video_shader_driver_set_mvp(mvp) \
   if (mvp.matrix) \
      current_shader->set_mvp(mvp.data, shader_data, mvp.matrix) \

bool video_shader_driver_filter_type(video_shader_ctx_filter_t *filter);

bool video_shader_driver_compile_program(struct shader_program_info *program_info);

#define video_shader_driver_use(shader_info) \
   current_shader->use(shader_info.data, shader_data, shader_info.idx, shader_info.set_active)

bool video_shader_driver_wrap_type(video_shader_ctx_wrap_t *wrap);

bool renderchain_init_first(const d3d_renderchain_driver_t **renderchain_driver,
	void **renderchain_handle);

extern bool (*video_driver_cb_has_focus)(void);

extern shader_backend_t *current_shader;
extern void *shader_data;

extern video_driver_t video_gl;
extern video_driver_t video_vulkan;
extern video_driver_t video_psp1;
extern video_driver_t video_vita2d;
extern video_driver_t video_ctr;
extern video_driver_t video_d3d;
extern video_driver_t video_gx;
extern video_driver_t video_wiiu;
extern video_driver_t video_xenon360;
extern video_driver_t video_xvideo;
extern video_driver_t video_sdl;
extern video_driver_t video_sdl2;
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
extern video_driver_t video_null;

extern const gfx_ctx_driver_t gfx_ctx_osmesa;
extern const gfx_ctx_driver_t gfx_ctx_sdl_gl;
extern const gfx_ctx_driver_t gfx_ctx_x_egl;
extern const gfx_ctx_driver_t gfx_ctx_wayland;
extern const gfx_ctx_driver_t gfx_ctx_x;
extern const gfx_ctx_driver_t gfx_ctx_d3d;
extern const gfx_ctx_driver_t gfx_ctx_drm;
extern const gfx_ctx_driver_t gfx_ctx_mali_fbdev;
extern const gfx_ctx_driver_t gfx_ctx_vivante_fbdev;
extern const gfx_ctx_driver_t gfx_ctx_android;
extern const gfx_ctx_driver_t gfx_ctx_ps3;
extern const gfx_ctx_driver_t gfx_ctx_wgl;
extern const gfx_ctx_driver_t gfx_ctx_videocore;
extern const gfx_ctx_driver_t gfx_ctx_qnx;
extern const gfx_ctx_driver_t gfx_ctx_cgl;
extern const gfx_ctx_driver_t gfx_ctx_cocoagl;
extern const gfx_ctx_driver_t gfx_ctx_emscripten;
extern const gfx_ctx_driver_t gfx_ctx_opendingux_fbdev;
extern const gfx_ctx_driver_t gfx_ctx_khr_display;
extern const gfx_ctx_driver_t gfx_ctx_gdi;
extern const gfx_ctx_driver_t gfx_ctx_null;


extern const shader_backend_t gl_glsl_backend;
extern const shader_backend_t hlsl_backend;
extern const shader_backend_t gl_cg_backend;
extern const shader_backend_t shader_null_backend;

extern d3d_renderchain_driver_t d3d8_renderchain;
extern d3d_renderchain_driver_t cg_d3d9_renderchain;
extern d3d_renderchain_driver_t hlsl_d3d9_renderchain;
extern d3d_renderchain_driver_t null_renderchain;

RETRO_END_DECLS

#endif
