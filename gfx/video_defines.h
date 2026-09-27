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

#ifndef __VIDEO_DEFINES__H
#define __VIDEO_DEFINES__H

#include <stddef.h>

#include <retro_common_api.h>

/* One-cycle alias: builds that still pass the old switch get the
 * in-tree modeline engine. */
#if defined(HAVE_CRTSWITCHRES) && !defined(HAVE_MODELINE)
#define HAVE_MODELINE
#endif

RETRO_BEGIN_DECLS

enum
{
   TEXTURES = 8,
   TEXTURESMASK = TEXTURES - 1
};

enum texture_filter_type
{
   TEXTURE_FILTER_LINEAR = 0,
   TEXTURE_FILTER_NEAREST,
   TEXTURE_FILTER_MIPMAP_LINEAR,
   TEXTURE_FILTER_MIPMAP_NEAREST
};

enum aspect_ratio
{
   ASPECT_RATIO_4_3 = 0,
   ASPECT_RATIO_16_9,
   ASPECT_RATIO_16_10,
   ASPECT_RATIO_16_15,
   ASPECT_RATIO_21_9,
   ASPECT_RATIO_1_1,
   ASPECT_RATIO_2_1,
   ASPECT_RATIO_3_2,
   ASPECT_RATIO_3_4,
   ASPECT_RATIO_4_1,
   ASPECT_RATIO_4_4,
   ASPECT_RATIO_5_4,
   ASPECT_RATIO_6_5,
   ASPECT_RATIO_7_9,
   ASPECT_RATIO_8_3,
   ASPECT_RATIO_8_7,
   ASPECT_RATIO_19_12,
   ASPECT_RATIO_19_14,
   ASPECT_RATIO_30_17,
   ASPECT_RATIO_32_9,
   ASPECT_RATIO_CONFIG,
   ASPECT_RATIO_SQUARE,
   ASPECT_RATIO_CORE,
   ASPECT_RATIO_CUSTOM,
   ASPECT_RATIO_FULL,

   ASPECT_RATIO_END
};

enum video_scale_integer_axis
{
   VIDEO_SCALE_INTEGER_AXIS_Y = 0,
   VIDEO_SCALE_INTEGER_AXIS_Y_X,
   VIDEO_SCALE_INTEGER_AXIS_Y_XHALF,
   VIDEO_SCALE_INTEGER_AXIS_YHALF_XHALF,
   VIDEO_SCALE_INTEGER_AXIS_X,
   VIDEO_SCALE_INTEGER_AXIS_XHALF,
   VIDEO_SCALE_INTEGER_AXIS_LAST
};

enum video_scale_integer_scaling
{
   VIDEO_SCALE_INTEGER_SCALING_UNDERSCALE = 0,
   VIDEO_SCALE_INTEGER_SCALING_OVERSCALE,
   VIDEO_SCALE_INTEGER_SCALING_SMART,
   VIDEO_SCALE_INTEGER_SCALING_LAST
};

enum rotation
{
   ORIENTATION_NORMAL = 0,
   ORIENTATION_VERTICAL,
   ORIENTATION_FLIPPED,
   ORIENTATION_FLIPPED_ROTATED,
   ORIENTATION_END
};

enum video_rotation_type
{
   VIDEO_ROTATION_NORMAL = 0,
   VIDEO_ROTATION_90_DEG,
   VIDEO_ROTATION_180_DEG,
   VIDEO_ROTATION_270_DEG
};

/* How hard to push for exclusive fullscreen where the platform lets the
 * application decide (VK_EXT_full_screen_exclusive on Windows Vulkan). */
enum video_fse_negotiation
{
   VIDEO_FSE_RELAXED = 0, /* hint only; the driver may decline */
   VIDEO_FSE_FORCED,      /* take it explicitly and hold it    */
   VIDEO_FSE_LAST
};

enum autoswitch_refresh_rate
{
   AUTOSWITCH_REFRESH_RATE_EXCLUSIVE_FULLSCREEN = 0,
   AUTOSWITCH_REFRESH_RATE_WINDOWED_FULLSCREEN,
   AUTOSWITCH_REFRESH_RATE_ALL_FULLSCREEN,
   AUTOSWITCH_REFRESH_RATE_OFF,
   AUTOSWITCH_REFRESH_RATE_LAST
};

enum time_show_type
{
   TIME_SHOW_OFF = 0,
   TIME_SHOW_HM,
   TIME_SHOW_HMS,
   TIME_SHOW_HM_AMPM,
   TIME_SHOW_HMS_AMPM,
   TIME_SHOW_LAST
};

enum rarch_display_type
{
   /* Non-bindable types like consoles, KMS, VideoCore, etc. */
   RARCH_DISPLAY_NONE = 0,
   /* video_display => Display*, video_window => Window */
   RARCH_DISPLAY_X11,
   /* video_display => N/A, video_window => HWND */
   RARCH_DISPLAY_WIN32,
   RARCH_DISPLAY_WAYLAND,
   RARCH_DISPLAY_OSX,
   RARCH_DISPLAY_KMS,
   /* Legacy Raspberry Pi firmware stack: no window, modes through
    * the firmware's gencmd interface */
   RARCH_DISPLAY_VIDEOCORE
};


enum text_alignment
{
   TEXT_ALIGN_LEFT = 0,
   TEXT_ALIGN_RIGHT,
   TEXT_ALIGN_CENTER
};

#ifndef COLOR_ABGR
#define COLOR_ABGR(r, g, b, a) (((unsigned)(a) << 24) | ((b) << 16) | ((g) << 8) | ((r) << 0))
#endif

#define LAST_ASPECT_RATIO ASPECT_RATIO_FULL

/* ABGR color format defines */

#define WHITE		  0xffffffffu
#define RED         0xff0000ffu
#define GREEN		  0xff00ff00u
#define BLUE        0xffff0000u
#define YELLOW      0xff00ffffu
#define PURPLE      0xffff00ffu
#define CYAN        0xffffff00u
#define ORANGE      0xff0063ffu
#define SILVER      0xff8c848cu
#define LIGHTBLUE   0xFFFFE0E0U
#define LIGHTORANGE 0xFFE0EEFFu

#define FONT_COLOR_RGBA(r, g, b, a) (((unsigned)(r) << 24) | ((g) << 16) | ((b) << 8) | ((a) << 0))
#define FONT_COLOR_GET_RED(col)   (((col) >> 24) & 0xff)
#define FONT_COLOR_GET_GREEN(col) (((col) >> 16) & 0xff)
#define FONT_COLOR_GET_BLUE(col)  (((col) >>  8) & 0xff)
#define FONT_COLOR_GET_ALPHA(col) (((col) >>  0) & 0xff)
#define FONT_COLOR_ARGB_TO_RGBA(col) ( (((col) >> 24) & 0xff) | (((unsigned)(col) << 8) & 0xffffff00) )

/* The statistics overlay's text, the buffer it is built in and the
 * copy the threaded wrapper draws from alike. */
#define VIDEO_STAT_TEXT_SIZE 1664

/* A size pair in one word: width in the high half, height in the low,
 * clamped so neither axis can write over the other. Anything past
 * 65535 an axis is beyond what a driver here allocates. */
#define VIDEO_SCALE_DIM_MAX 0xffffu
/* Each axis becomes unsigned before it is compared, so a caller holding
 * its sizes in int or float packs without a cast of its own. */
#define VIDEO_SCALE_CLAMP(v) \
   ((unsigned)(v) > VIDEO_SCALE_DIM_MAX ? VIDEO_SCALE_DIM_MAX : (unsigned)(v))
#define VIDEO_SCALE_PACK(w, h) \
   ((VIDEO_SCALE_CLAMP(w) << 16) | VIDEO_SCALE_CLAMP(h))
#define VIDEO_SCALE_W(d) (((unsigned)(d) >> 16) & VIDEO_SCALE_DIM_MAX)
#define VIDEO_SCALE_H(d)  ((unsigned)(d)        & VIDEO_SCALE_DIM_MAX)
/* Whether a size packs without clamping. Anything sized for storage
 * from a word has to ask first: a clamped axis would allocate less
 * than the source it holds. */
#define VIDEO_SCALE_FITS(w, h) \
   ((unsigned)(w) <= VIDEO_SCALE_DIM_MAX && (unsigned)(h) <= VIDEO_SCALE_DIM_MAX)

/* The pixel count, as size_t. Both axes are at most 65535, so their
 * product reaches 0xfffe0001 -- inside 32-bit unsigned, but a buffer
 * size is that times the bytes per pixel, and 32-bit arithmetic gives
 * a too-small allocation rather than a failure. Widening here means a
 * caller writing VIDEO_SCALE_AREA(d) * 4 gets the wide multiply
 * without having to remember the cast. */
#define VIDEO_SCALE_AREA(d) ((size_t)VIDEO_SCALE_W(d) * VIDEO_SCALE_H(d))

/* An alpha modulation as an 8-bit channel, saturated. An overlay's
 * alpha is its opacity times a per-desc alpha_mod, which packs set
 * above 1 to brighten a pressed button; packed straight into a byte
 * that product wraps, so 1.4 comes out as 0x65. */
#define VIDEO_ALPHA_BYTE(a) \
   ((a) <= 0.0f ? 0u : (a) >= 1.0f ? 0xFFu : (unsigned)((a) * 0xFF))

/* One axis of a packed pair, leaving the other half as it stands.
 * A viewport whose axes are set apart from each other reads back
 * through VIDEO_SCALE_W/H either way. */
#define VIDEO_SCALE_PUT_W(d, w) \
   ((d) = VIDEO_SCALE_PACK((w), VIDEO_SCALE_H(d)))
#define VIDEO_SCALE_PUT_H(d, h) \
   ((d) = VIDEO_SCALE_PACK(VIDEO_SCALE_W(d), (h)))

/* An origin in one word: x in the high half, y in the low, each a
 * signed 16-bit offset. A viewport's origin goes negative wherever
 * integer scaling overscans the window, so both halves sign-extend on
 * the way back out. An offset past +-32767 is further off a display
 * than any of these drivers places one. */
#define VIDEO_POS_MAX    32767
#define VIDEO_POS_MIN  (-32768)
#define VIDEO_POS_CLAMP(v) \
   ((int)(v) > VIDEO_POS_MAX ? VIDEO_POS_MAX \
    : ((int)(v) < VIDEO_POS_MIN ? VIDEO_POS_MIN : (int)(v)))
#define VIDEO_POS_PACK(x, y) \
   ((((unsigned)VIDEO_POS_CLAMP(x) & 0xffffu) << 16) \
    | ((unsigned)VIDEO_POS_CLAMP(y) & 0xffffu))
#define VIDEO_POS_X(p) ((int)(int16_t)(((unsigned)(p) >> 16) & 0xffffu))
#define VIDEO_POS_Y(p) ((int)(int16_t)( (unsigned)(p)        & 0xffffu))

/* One axis of an origin, leaving the other half as it stands. */
#define VIDEO_POS_PUT_X(p, x) \
   ((p) = VIDEO_POS_PACK((x), VIDEO_POS_Y(p)))
#define VIDEO_POS_PUT_Y(p, y) \
   ((p) = VIDEO_POS_PACK(VIDEO_POS_X(p), (y)))

/* A float length or position as the whole pixels a display can show
 * it in. Every menu metric is a constant times a DPI scale, and every
 * animated position is a tween between two of those, so the value
 * arriving here is nearly always fractional and something has to
 * decide which pixel it means.
 *
 * It rounds. Truncating is what the plain conversion does, and it
 * loses up to a pixel off every metric in the same direction: at the
 * 1.3333 scale a 50px row became 66 rather than 67, which is two
 * thirds of a pixel per row and thirteen down a twenty-row list.
 * Rounding halves the worst case and stops it accumulating in one
 * direction.
 *
 * It also bounds the conversion. Converting a float past INT_MAX is
 * undefined, and widget layout has produced such a value in the
 * frames before an icon's metrics are known: sdl2_gfx and sdl3_gfx
 * each carry a hand-written range test against the NaN vertices it
 * turned into downstream. A value out of range - NaN included, since
 * neither comparison holds for it - lands at the far edge instead,
 * which draws off-screen and is over the following frame.
 *
 * v is evaluated more than once, as it is in VIDEO_POS_CLAMP above;
 * callers pass a variable or a plain arithmetic expression. */
#define VIDEO_PX(v) \
   (((v) >= (float)VIDEO_POS_MIN && (v) <= (float)VIDEO_POS_MAX) \
    ? (int)((v) + ((v) < 0.0f ? -0.5f : 0.5f)) \
    : (((v) > 0.0f) ? VIDEO_POS_MAX : VIDEO_POS_MIN))

typedef struct video_viewport
{
   /* The origin, one signed pair in VIDEO_POS_PACK's layout. */
   unsigned pos;
   /* The drawn area and the window that holds it, each a size pair
    * in one word, VIDEO_SCALE_PACK's layout. */
   unsigned dims;
   unsigned full_dims;
} video_viewport_t;

/* The custom viewport as the settings hold it: an origin in
 * VIDEO_POS_PACK's layout and a size in VIDEO_SCALE_PACK's, the same
 * two words video_viewport_t carries.
 *
 * The config file still keeps custom_viewport_x/_y/_width/_height as
 * four keys and the menu still shows four rows. Neither binds a half
 * by address any more: a config row carries which half of the word it
 * is (CFG_HALF_HI / CFG_HALF_LO in configuration.c) and a menu row
 * carries the same in its SD_FREE_FLAG_PACKED_HI / _LO bit. */
typedef struct video_viewport_settings
{
   unsigned pos;
   unsigned dims;
} video_viewport_settings_t;

typedef struct gfx_ctx_flags
{
   uint32_t flags;
} gfx_ctx_flags_t;

enum gfx_ctx_api
{
   GFX_CTX_NONE = 0,
   GFX_CTX_OPENGL_API,
   GFX_CTX_OPENGL_ES_API,
   GFX_CTX_DIRECT3D8_API,
   GFX_CTX_DIRECT3D9_API,
   GFX_CTX_DIRECT3D10_API,
   GFX_CTX_DIRECT3D11_API,
   GFX_CTX_DIRECT3D12_API,
   GFX_CTX_OPENVG_API,
   GFX_CTX_VULKAN_API,
   GFX_CTX_METAL_API,
   GFX_CTX_RSX_API
};

enum display_metric_types
{
   DISPLAY_METRIC_NONE = 0,
   DISPLAY_METRIC_MM_WIDTH,
   DISPLAY_METRIC_MM_HEIGHT,
   DISPLAY_METRIC_DPI,
   DISPLAY_METRIC_PIXEL_WIDTH,
   DISPLAY_METRIC_PIXEL_HEIGHT
};

enum display_flags
{
   GFX_CTX_FLAGS_NONE            = 0,
   GFX_CTX_FLAGS_GL_CORE_CONTEXT,
   GFX_CTX_FLAGS_MULTISAMPLING,
   GFX_CTX_FLAGS_CUSTOMIZABLE_SWAPCHAIN_IMAGES,
   GFX_CTX_FLAGS_CUSTOMIZABLE_FRAME_LATENCY,
   GFX_CTX_FLAGS_HARD_SYNC,
   GFX_CTX_FLAGS_BLACK_FRAME_INSERTION,
   GFX_CTX_FLAGS_MENU_FRAME_FILTERING,
   GFX_CTX_FLAGS_ADAPTIVE_VSYNC,
   GFX_CTX_FLAGS_SHADERS_GLSL,
   GFX_CTX_FLAGS_SHADERS_CG,
   GFX_CTX_FLAGS_SHADERS_HLSL,
   GFX_CTX_FLAGS_SHADERS_SLANG,
   GFX_CTX_FLAGS_SCREENSHOTS_SUPPORTED,
   GFX_CTX_FLAGS_OVERLAY_BEHIND_MENU_SUPPORTED,
   GFX_CTX_FLAGS_CRT_SWITCHRES,
   GFX_CTX_FLAGS_SUBFRAME_SHADERS,
   GFX_CTX_FLAGS_FAST_TOGGLE_SHADERS,
   /* Set by a video driver that can present a native XRGB2101010 (10-bit
    * per channel) source frame without the frontend down-converting it to
    * XRGB8888 first. */
   GFX_CTX_FLAGS_SCREEN_10BPC_SOURCE,
   /* Set by a context driver whose default framebuffer is FP16 scRGB
    * (linear, 1.0 = 80 nits): the video driver must encode SDR content
    * for HDR output itself (paper-white scaling etc.). */
   GFX_CTX_FLAGS_SCRGB_FRAMEBUFFER,
   /* Set by a context driver whose default framebuffer is 10-bit
    * Rec.2020 PQ (HDR10, e.g. a KMS scanout with HDR metadata): the
    * video driver encodes its frame to PQ instead of scRGB. */
   GFX_CTX_FLAGS_HDR10_FRAMEBUFFER
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

/* All coordinates and offsets are top-left oriented.
 *
 * This is a texture-atlas approach which allows text to
 * be drawn in a single draw call.
 *
 * It is up to the code using this interface to actually
 * generate proper vertex buffers and upload the atlas texture to GPU. */

struct font_glyph
{
   unsigned width;
   unsigned height;

   /* Texel coordinate offset for top-left pixel of this glyph. */
   unsigned atlas_offset_x;
   unsigned atlas_offset_y;

   /* When drawing this glyph, apply an offset to
    * current X/Y draw coordinate. */
   int draw_offset_x;
   int draw_offset_y;

   /* Advance X/Y draw coordinates after drawing this glyph. */
   int advance_x;
   int advance_y;
};

/* Coverage bit depth of a font atlas. A8 is the default; A16 is
 * produced when a higher-precision atlas was requested (HDR output)
 * and the renderer supports it, with samples stored as native-endian
 * uint16_t in 'buffer'. Consumers must check 'format' before
 * interpreting the buffer or sizing uploads. */
enum font_atlas_format
{
   FONT_ATLAS_FORMAT_A8 = 0,
   FONT_ATLAS_FORMAT_A16
};

struct font_atlas
{
   uint8_t *buffer; /* Coverage samples; layout per 'format'. */
   unsigned width;
   unsigned height;
   /* Dirty region in pixels, covering every glyph cell updated since
    * the consumer last cleared the dirty flag; x1/y1 are exclusive
    * and the values are only meaningful while dirty is set.
    * Consumers may upload just this region (or any superset of it,
    * such as the full-width row band) instead of the whole atlas. */
   unsigned dirty_x0;
   unsigned dirty_y0;
   unsigned dirty_x1;
   unsigned dirty_y1;
   enum font_atlas_format format;
   bool dirty;
};

struct font_params
{
   /* Drop shadow offset.
    * If both are 0, no drop shadow will be rendered. */
   int drop_x, drop_y;

   /* ABGR. Use the macros. */
   uint32_t color;

   /* Optional full-precision colour. When non-NULL it points to 4 floats
    * (R, G, B, A, each 0..1) that take precedence over the 8-bit 'color'
    * above, letting a caller drive text at more than 8 bits per channel on a
    * deep-colour (e.g. 10-bit) framebuffer. NULL means "use 'color'".
    *
    * Only honoured by font backends that opt in; the rest ignore it and use
    * 'color', so it is always safe to leave set or unset. Because most
    * font_params are built field by field, a producer that wants to use this
    * MUST set it explicitly (to NULL or to a valid array) - do not assume it
    * is zero-initialised. It is only ever read by backends fed from the
    * central builders that initialise it (the menu text path and the OSD
    * stat params), so an uninitialised value at other sites is never
    * dereferenced. */
   const float *color_hp;

   float x;
   float y;
   float scale;
   /* Drop shadow color multiplier. */
   float drop_mod;
   /* Drop shadow alpha */
   float drop_alpha;

   enum text_alignment text_align;

   bool full_screen;
};

struct font_line_metrics
{
   float height;
   float ascender;
   float descender;
};

/* A pass in the FBO chain, its three sizes each in VIDEO_SCALE_PACK's
 * layout: what the pass renders this frame, the largest it renders at
 * any input size, and the texture that holds it. */
struct video_fbo_rect
{
   unsigned img_dims;
   unsigned max_img_dims;
   unsigned dims;
};

struct video_ortho
{
   float left;
   float right;
   float bottom;
   float top;
   float znear;
   float zfar;
};

struct video_tex_info
{
   unsigned int tex;
   float input_size[2];
   float tex_size[2];
   float coord[8];
};

typedef struct video_coords
{
   const float *vertex;
   const float *color;
   const float *tex_coord;
   const float *lut_tex_coord;
   const unsigned *index;
   unsigned vertices;
   unsigned indexes;
} video_coords_t;

typedef struct video_mut_coords
{
   float *vertex;
   float *color;
   float *tex_coord;
   float *lut_tex_coord;
   unsigned *index;
   unsigned vertices;
   unsigned indexes;
} video_mut_coords_t;

typedef struct video_coord_array
{
   video_mut_coords_t coords; /* ptr alignment */
   unsigned allocated;
} video_coord_array_t;

typedef struct video_font_raster_block
{
   video_coord_array_t carr; /* ptr alignment */
   bool fullscreen;
} video_font_raster_block_t;


RETRO_END_DECLS

#endif
