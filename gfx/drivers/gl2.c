/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 *  Copyright (C) 2016-2019 - Brad Parker
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

/* Middle of the road OpenGL 2.x driver.
 *
 * Minimum version (desktop): OpenGL   2.0+ (2004)
 * Minimum version (mobile) : OpenGLES 2.0+ (2007)
 */

#ifdef _MSC_VER
#if defined(HAVE_OPENGLES)
#pragma comment(lib, "libGLESv2")
#else
#pragma comment(lib, "opengl32")
#endif
#endif

#include "../gfx_instrument.h"
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <encodings/utf.h>
#include <compat/strl.h>
#include <gfx/scaler/scaler.h>
#include <gfx/math/matrix_4x4.h>
#include <formats/image.h>
#include <retro_inline.h>
#include <retro_miscellaneous.h>
#include <retro_math.h>
#include <string/stdstring.h>
#include <libretro.h>

#include <gfx/gl_capabilities.h>
#include <gfx/video_frame.h>
#include <glsym/glsym.h>

#include "../../configuration.h"
#include "../../dynamic.h"

#include "../../retroarch.h"
#include "../../runloop.h"
#include "../../record/record_driver.h"
#include "../../verbosity.h"
#include "../common/gl2_common.h"
#ifdef GL2_KMS_FRAMES
#include "../common/drm_common.h"
#endif

#ifdef HAVE_THREADS
#include "../video_thread_wrapper.h"
#include "../video_thread_hw.h"
#endif

static bool gl2_core_context_is_mains(gl2_t *gl);

#include "../font_driver.h"

#ifdef HAVE_GLSL
#include "../drivers_shader/shader_glsl.h"
#endif

#ifdef GL_DEBUG
#include <lists/string_list.h>

#if defined(HAVE_OPENGLES2) || defined(HAVE_OPENGLES3) || defined(HAVE_OPENGLES_3_1) || defined(HAVE_OPENGLES_3_2)
#define HAVE_GL_DEBUG_ES
#endif
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif
#ifdef HAVE_GFX_WIDGETS
#include "../gfx_widgets.h"
#ifdef __MACH__
#include <TargetConditionals.h>
#endif
#endif

#ifdef HAVE_OPENXR
#include "../drivers_context/gl_android_openxr.h"
#endif

/* Views are drawn with programs of their own */
#ifdef HAVE_GLSL
#define GL2_VIEWS
#endif
/* Desktop headset output, never built with the Quest's. */
#if defined(HAVE_OPENXR_GL) && !defined(ANDROID)
#define HAVE_GL2_XR
#include "../common/gl_openxr.h"
#include "../video_xr.h"
#endif

#ifndef GL_UNSIGNED_INT_8_8_8_8_REV
#define GL_UNSIGNED_INT_8_8_8_8_REV       0x8367
#endif
/* 10-bit source support (desktop only; tokens may be absent from GLES2
 * headers but the code paths using them are compiled out there). */
#ifndef GL_RGB10_A2
#define GL_RGB10_A2                       0x8059
#endif
#ifndef GL_UNSIGNED_INT_2_10_10_10_REV
#define GL_UNSIGNED_INT_2_10_10_10_REV    0x8368
#endif
#ifndef GL_RGBA16F
#define GL_RGBA16F                        0x881A
#endif
#ifndef GL_HALF_FLOAT
#define GL_HALF_FLOAT                     0x140B
#endif

#if !defined(HAVE_OPENGLES) && !defined(HAVE_PSGL)
/* The names of the RGBA16F textures gl2_load_texture made: those are
 * linear scRGB and drawn through gl2_linear_program. Loads, unloads and
 * draws all run on the thread that owns the context. */
static GLuint  *gl2_fp16_tex;
static unsigned gl2_fp16_count;
static unsigned gl2_fp16_cap;
/* One was loaded since init: the UI's layer turns RGBA16F for good,
 * so a session with no linear texture keeps the 8-bit layer */
static bool     gl2_fp16_loaded;

/* Room for one more name, made before the texture is: a half-float
 * texture the set could not hold would be drawn as an SDR one, so the
 * load is refused instead */
static bool gl2_fp16_reserve(void)
{
   unsigned cap;
   GLuint *grown;
   if (gl2_fp16_count < gl2_fp16_cap)
      return true;
   cap = gl2_fp16_cap ? gl2_fp16_cap * 2 : 32;
   if (!(grown = (GLuint*)realloc(gl2_fp16_tex, cap * sizeof(*grown))))
      return false;
   gl2_fp16_tex = grown;
   gl2_fp16_cap = cap;
   return true;
}

static void gl2_fp16_remember(GLuint id)
{
   gl2_fp16_tex[gl2_fp16_count++] = id;
   gl2_fp16_loaded                = true;
}

static void gl2_fp16_forget(GLuint id)
{
   unsigned i;
   for (i = 0; i < gl2_fp16_count; i++)
      if (gl2_fp16_tex[i] == id)
      {
         gl2_fp16_tex[i] = gl2_fp16_tex[--gl2_fp16_count];
         return;
      }
}

static void gl2_fp16_forget_all(void)
{
   free(gl2_fp16_tex);
   gl2_fp16_tex    = NULL;
   gl2_fp16_count  = 0;
   gl2_fp16_cap    = 0;
   gl2_fp16_loaded = false;
}

static bool gl2_fp16_is(GLuint id)
{
   unsigned i;
   for (i = 0; i < gl2_fp16_count; i++)
      if (gl2_fp16_tex[i] == id)
         return true;
   return false;
}

static GLuint gl2_linear_program(gl2_t *gl);
#endif

#if defined(HAVE_OPENGLES2)
#define GL2_DEFAULT_SHADER_TYPE RARCH_SHADER_GLSL
#elif defined(HAVE_GLSL)
#define GL2_DEFAULT_SHADER_TYPE RARCH_SHADER_GLSL
#elif defined(HAVE_CG)
#define GL2_DEFAULT_SHADER_TYPE RARCH_SHADER_CG
#else
#define GL2_DEFAULT_SHADER_TYPE RARCH_SHADER_NONE
#endif

/* Desktop OpenGL against a macOS SDK before 10.12, whose framebuffer
 * names carry the EXT suffix. An iOS SDK defines the macOS version
 * macro as well - below 10.12 up to the iOS 10 SDKs - so GLES builds
 * are ruled out by name. */
#if defined(__MACH__) && !defined(HAVE_OPENGLES) \
   && defined(MAC_OS_X_VERSION_MAX_ALLOWED) \
   && (MAC_OS_X_VERSION_MAX_ALLOWED < 101200)
#define GL2_MAC_EXT_FBO 1
#endif

#if defined(HAVE_PSGL)
#define RARCH_GL_FRAMEBUFFER GL_FRAMEBUFFER_OES
#define RARCH_GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE_OES
#define RARCH_GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0_EXT
#elif defined(GL2_MAC_EXT_FBO)
#define RARCH_GL_FRAMEBUFFER GL_FRAMEBUFFER_EXT
#define RARCH_GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE_EXT
#define RARCH_GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0_EXT
#else
#define RARCH_GL_FRAMEBUFFER GL_FRAMEBUFFER
#define RARCH_GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE
#define RARCH_GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0
#endif

#if defined(HAVE_OPENGLES2) || defined(HAVE_OPENGLES3) || defined(HAVE_OPENGLES_3_1) || defined(HAVE_OPENGLES_3_2)
#define RARCH_GL_RENDERBUFFER GL_RENDERBUFFER
#if defined(HAVE_OPENGLES2)
#define RARCH_GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8_OES
#else
#define RARCH_GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8
#endif
#define RARCH_GL_DEPTH_ATTACHMENT GL_DEPTH_ATTACHMENT
#define RARCH_GL_STENCIL_ATTACHMENT GL_STENCIL_ATTACHMENT
#elif defined(GL2_MAC_EXT_FBO)
#define RARCH_GL_RENDERBUFFER GL_RENDERBUFFER_EXT
#define RARCH_GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8_EXT
#define RARCH_GL_DEPTH_ATTACHMENT GL_DEPTH_ATTACHMENT_EXT
#define RARCH_GL_STENCIL_ATTACHMENT GL_STENCIL_ATTACHMENT_EXT
#elif defined(HAVE_PSGL)
#define RARCH_GL_RENDERBUFFER GL_RENDERBUFFER_OES
#define RARCH_GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8_SCE
#define RARCH_GL_DEPTH_ATTACHMENT GL_DEPTH_ATTACHMENT_OES
#define RARCH_GL_STENCIL_ATTACHMENT GL_STENCIL_ATTACHMENT_OES
#else
#define RARCH_GL_RENDERBUFFER GL_RENDERBUFFER
#define RARCH_GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8
#define RARCH_GL_DEPTH_ATTACHMENT GL_DEPTH_ATTACHMENT
#define RARCH_GL_STENCIL_ATTACHMENT GL_STENCIL_ATTACHMENT
#endif

#if defined(GL2_MAC_EXT_FBO)
#define RARCH_GL_MAX_RENDERBUFFER_SIZE GL_MAX_RENDERBUFFER_SIZE_EXT
#elif defined(HAVE_PSGL)
#define RARCH_GL_MAX_RENDERBUFFER_SIZE GL_MAX_RENDERBUFFER_SIZE_OES
#else
#define RARCH_GL_MAX_RENDERBUFFER_SIZE GL_MAX_RENDERBUFFER_SIZE
#endif

#if defined(HAVE_PSGL)
#define glGenerateMipmap glGenerateMipmapOES
#endif

#if defined(__APPLE__) || defined(HAVE_PSGL)
#ifndef GL_RGBA32F
#define GL_RGBA32F GL_RGBA32F_ARB
#endif
#endif

#if defined(HAVE_PSGL)
#define RARCH_GL_INTERNAL_FORMAT32 GL_ARGB_SCE
#define RARCH_GL_INTERNAL_FORMAT16 GL_RGB5 /* TODO: Verify if this is really 565 or just 555. */
#define RARCH_GL_TEXTURE_TYPE32 GL_BGRA
#define RARCH_GL_TEXTURE_TYPE16 GL_BGRA
#define RARCH_GL_FORMAT32 GL_UNSIGNED_INT_8_8_8_8_REV
#define RARCH_GL_FORMAT16 GL_RGB5
#elif defined(HAVE_OPENGLES)
/* Imgtec/SGX headers have this missing. */
#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif
#ifndef GL_BGRA8_EXT
#define GL_BGRA8_EXT 0x93A1
#endif
#if TARGET_OS_IPHONE
/* Stupid Apple */
#define RARCH_GL_INTERNAL_FORMAT32 GL_RGBA
#else
#define RARCH_GL_INTERNAL_FORMAT32 GL_BGRA_EXT
#endif
#define RARCH_GL_INTERNAL_FORMAT16 GL_RGB
#define RARCH_GL_TEXTURE_TYPE32 GL_BGRA_EXT
#define RARCH_GL_TEXTURE_TYPE16 GL_RGB
#define RARCH_GL_FORMAT32 GL_UNSIGNED_BYTE
#define RARCH_GL_FORMAT16 GL_UNSIGNED_SHORT_5_6_5
#else
/* On desktop, we always use 32-bit. */
#define RARCH_GL_INTERNAL_FORMAT32 GL_RGBA8
#define RARCH_GL_INTERNAL_FORMAT16 GL_RGBA8
#define RARCH_GL_TEXTURE_TYPE32 GL_BGRA
#define RARCH_GL_TEXTURE_TYPE16 GL_BGRA
#define RARCH_GL_FORMAT32 GL_UNSIGNED_INT_8_8_8_8_REV
#define RARCH_GL_FORMAT16 GL_UNSIGNED_INT_8_8_8_8_REV

/* GL_RGB565 internal format isn't in desktop GL
 * until 4.1 core (ARB_ES2_compatibility).
 * Check for this. */
#ifndef GL_RGB565
#define GL_RGB565 0x8D62
#endif
#define RARCH_GL_INTERNAL_FORMAT16_565 GL_RGB565
#define RARCH_GL_TEXTURE_TYPE16_565 GL_RGB
#define RARCH_GL_FORMAT16_565 GL_UNSIGNED_SHORT_5_6_5
#endif

#if defined(HAVE_OPENGLES2) /* TODO: Figure out exactly what. */
#define NO_GL_CLAMP_TO_BORDER
#endif

#if defined(HAVE_OPENGLES)
#ifndef GL_UNPACK_ROW_LENGTH
#define GL_UNPACK_ROW_LENGTH  0x0CF2
#endif

#ifndef GL_SRGB_ALPHA_EXT
#define GL_SRGB_ALPHA_EXT 0x8C42
#endif
#endif

#define GL2_BIND_TEXTURE(id, wrap_mode, mag_filter, min_filter) \
   glBindTexture(GL_TEXTURE_2D, id); \
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_mode); \
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_mode); \
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter); \
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter)

#define SET_TEXTURE_COORDS(coords, xamt, yamt) \
   coords[2] = xamt; \
   coords[6] = xamt; \
   coords[5] = yamt; \
   coords[7] = yamt

#define MAX_FENCES 4

#if !defined(HAVE_PSGL)

#ifndef HAVE_GL_SYNC
#define HAVE_GL_SYNC
#endif

#endif

/* Sync objects for the threaded wrapper's hardware ring and the PBO
 * readback. Desktop GL has them through ARB_sync, GLES from 3.0 in
 * core (GL_CAPS_SYNC checks the version at runtime); GLES 2 builds
 * finish instead. Without them every frame from a hardware core costs
 * a glFinish on the core's thread - the whole of the GPU's frame,
 * serialised behind the core's own work. */
#if defined(HAVE_GL_SYNC) && (!defined(HAVE_OPENGLES) || defined(HAVE_OPENGLES3))
#define GL2_HW_RING_SYNC 1
#endif

#define GL_RASTER_FONT_EMIT(c, vx, vy) \
   font_vertex[     2 * (6 * i + c) + 0] = (x + (delta_x + off_x + vx * width) * scale) * inv_win_width; \
   font_vertex[     2 * (6 * i + c) + 1] = (y + (delta_y - off_y - vy * height) * scale) * inv_win_height; \
   font_tex_coords[ 2 * (6 * i + c) + 0] = (tex_x + vx * width) * inv_tex_size_x; \
   font_tex_coords[ 2 * (6 * i + c) + 1] = (tex_y + vy * height) * inv_tex_size_y

#define MAX_MSG_LEN_CHUNK 64

#ifdef HAVE_GL_SYNC
#if defined(HAVE_OPENGLES2)
typedef struct __GLsync *GLsync;
#endif
#endif

#if (!defined(HAVE_OPENGLES) || defined(HAVE_OPENGLES3))
#ifdef GL_PIXEL_PACK_BUFFER
#define HAVE_GL_ASYNC_READBACK
#endif
#endif

/* Forward declaration for lazy init in read_viewport */
static bool gl2_init_pbo_readback(gl2_t *gl);
static void gl2_free_pbo_readback(gl2_t *gl);

#if defined(HAVE_PSGL)
#define gl2_fb_texture_2d(a, b, c, d, e) glFramebufferTexture2DOES(a, b, c, d, e)
#define gl2_check_fb_status(target) glCheckFramebufferStatusOES(target)
#define gl2_gen_fb(n, ids)   glGenFramebuffersOES(n, ids)
#define gl2_delete_fb(n, fb) glDeleteFramebuffersOES(n, fb)
#define gl2_bind_fb(id)      glBindFramebufferOES(RARCH_GL_FRAMEBUFFER, id)
#define gl2_gen_rb           glGenRenderbuffersOES
#define gl2_bind_rb          glBindRenderbufferOES
#define gl2_fb_rb            glFramebufferRenderbufferOES
#define gl2_rb_storage       glRenderbufferStorageOES
#define gl2_delete_rb        glDeleteRenderbuffersOES

#elif defined(GL2_MAC_EXT_FBO)
#define gl2_fb_texture_2d(a, b, c, d, e) glFramebufferTexture2DEXT(a, b, c, d, e)
#define gl2_check_fb_status(target) glCheckFramebufferStatusEXT(target)
#define gl2_gen_fb(n, ids)   glGenFramebuffersEXT(n, ids)
#define gl2_delete_fb(n, fb) glDeleteFramebuffersEXT(n, fb)
#define gl2_bind_fb(id)      glBindFramebufferEXT(RARCH_GL_FRAMEBUFFER, id)
#define gl2_gen_rb           glGenRenderbuffersEXT
#define gl2_bind_rb          glBindRenderbufferEXT
#define gl2_fb_rb            glFramebufferRenderbufferEXT
#define gl2_rb_storage       glRenderbufferStorageEXT
#define gl2_delete_rb        glDeleteRenderbuffersEXT

#else

#define gl2_fb_texture_2d(a, b, c, d, e) glFramebufferTexture2D(a, b, c, d, e)
#define gl2_check_fb_status(target) glCheckFramebufferStatus(target)
#define gl2_gen_fb(n, ids)   glGenFramebuffers(n, ids)
#define gl2_delete_fb(n, fb) glDeleteFramebuffers(n, fb)
#define gl2_bind_fb(id)      glBindFramebuffer(RARCH_GL_FRAMEBUFFER, id)
#define gl2_gen_rb           glGenRenderbuffers
#define gl2_bind_rb          glBindRenderbuffer
#define gl2_fb_rb            glFramebufferRenderbuffer
#define gl2_rb_storage       glRenderbufferStorage
#define gl2_delete_rb        glDeleteRenderbuffers

#endif

#ifndef GL_SYNC_GPU_COMMANDS_COMPLETE
#define GL_SYNC_GPU_COMMANDS_COMPLETE     0x9117
#endif

#ifndef GL_SYNC_FLUSH_COMMANDS_BIT
#define GL_SYNC_FLUSH_COMMANDS_BIT        0x00000001
#endif
/* The rest of the ARB_sync names the hardware ring uses; the PowerPC
 * macOS headers have none of them. */
#ifndef GL_TIMEOUT_IGNORED
#define GL_TIMEOUT_IGNORED                0xFFFFFFFFFFFFFFFFull
#endif
#ifndef GL_TIMEOUT_EXPIRED
#define GL_TIMEOUT_EXPIRED                0x911B
#endif
#ifndef GL_ALREADY_SIGNALED
#define GL_ALREADY_SIGNALED               0x911A
#endif
#ifndef GL_CONDITION_SATISFIED
#define GL_CONDITION_SATISFIED            0x911C
#endif

enum gl2_renderchain_flags
{
   GL2_CHAIN_FLAG_EGL_IMAGES           = (1 << 0),
   GL2_CHAIN_FLAG_HAS_FP_FBO           = (1 << 1),
   GL2_CHAIN_FLAG_HAS_SRGB_FBO         = (1 << 2),
   GL2_CHAIN_FLAG_HW_RENDER_DEPTH_INIT = (1 << 3)
};

typedef struct video_shader_ctx_scale
{
   struct gfx_fbo_scale *scale;
} video_shader_ctx_scale_t;

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

typedef struct gl2_renderchain_data
{
   int fbo_pass;

#ifdef HAVE_GL_SYNC
   GLsync fences[MAX_FENCES];
#endif

   GLuint vao;
   GLuint fbo[GFX_MAX_SHADERS];
   GLuint fbo_texture[GFX_MAX_SHADERS];
   GLuint hw_render_depth[GFX_MAX_TEXTURES];

   unsigned fence_count;

   struct gfx_fbo_scale fbo_scale[GFX_MAX_SHADERS];
   uint8_t flags;
} gl2_renderchain_data_t;

/* TODO: Move viewport side effects to the caller: it's a source of bugs. */

typedef struct
{
   gl2_t *gl;
   GLuint tex;
   unsigned tex_dims;            /* VIDEO_SCALE_PACK, the atlas texture */
   bool respecify;               /* The atlas grew: remake the texture */

   const font_renderer_driver_t *font_driver;
   void *font_data;
   struct font_atlas *atlas;

   video_font_raster_block_t *block;

   /* The chunk a line is built into before it is handed over. Here
    * rather than on the stack of the function that fills it: three
    * arrays of MAX_MSG_LEN_CHUNK glyphs are twelve kilobytes, and a
    * frame that size is three times what this tree allows. One font
    * renders at a time on the thread that draws, so one is enough. */
   GLfloat font_vertex[2 * 6 * MAX_MSG_LEN_CHUNK];
   GLfloat font_tex_coords[2 * 6 * MAX_MSG_LEN_CHUNK];
   GLfloat font_color[4 * 6 * MAX_MSG_LEN_CHUNK];
} gl2_raster_t;

#if defined(__arm__) || defined(__aarch64__)
static int scx0, scx1, scy0, scy1;

/* This array contains problematic GPU drivers
 * that have problems when we draw outside the
 * bounds of the framebuffer */
static const struct
{
   const char *str;
   int len;
} scissor_device_strings[] = {
   { "ARM Mali-4xx", 10 },
   { 0, 0 }
};
#endif

static const shader_backend_t *gl2_shader_ctx_drivers[] = {
#ifdef HAVE_GLSL
   &gl_glsl_backend,
#endif
#ifdef HAVE_CG
   &gl_cg_backend,
#endif
   NULL
};

static struct video_ortho default_ortho = {0, 1, 0, 1, -1, 1};

/* Used for the last pass when rendering to the back buffer. */
static const GLfloat vertexes_flipped[8] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

/* Used when rendering to an FBO.
 * Texture coords have to be aligned
 * with vertex coordinates. */
static const GLfloat vertexes[8] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const GLfloat gl2_vertexes[8] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const GLfloat gl2_tex_coords[8] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

static const GLfloat tex_coords[8] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const GLfloat white_color[16] = {
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1,
};

/**
 * FORWARD DECLARATIONS
 */
static void gl2_set_viewport(gl2_t *gl,
      unsigned dims,
      bool force_full, bool allow_rotate);

#if TARGET_OS_IPHONE
/* There is no default frame buffer on iOS. */
void glkitview_bind_fbo(void);
#define gl2_renderchain_bind_backbuffer() glkitview_bind_fbo()
#else
#if defined(HAVE_OPENXR) && defined(ANDROID)
#define gl2_renderchain_bind_backbuffer() gl2_bind_fb(gl_android_openxr_get_framebuffer())
#else
#define gl2_renderchain_bind_backbuffer() gl2_bind_fb(0)
#endif
#endif

/* Defined with the scRGB helpers further down; referenced from the
 * renderchain's final-target bind above them. */
static GLuint gl2_frame_target_fbo(gl2_t *gl);
static bool gl2_needs_pq_downconvert(gl2_t *gl);

/**
 * DISPLAY DRIVER
 */

#if defined(__arm__) || defined(__aarch64__)
static void scissor_set_rectangle(
      int x0, int x1, int y0, int y1, int sc)
{
   const int dx = sc ? 10 : 2;
   const int dy = dx;
   scx0         = x0 + dx;
   scx1         = x1 - dx;
   scy0         = y0 + dy;
   scy1         = y1 - dy;
}

static bool scissor_is_outside_rectangle(
      int x0, int x1, int y0, int y1)
{
   if (x1 < scx0)
      return true;
   if (scx1 < x0)
      return true;
   if (y1 < scy0)
      return true;
   if (scy1 < y0)
      return true;
   return false;
}

#define MALI_BUG
#endif

static const float *gfx_display_gl2_get_default_vertices(void)
{
   return &gl2_vertexes[0];
}

static const float *gfx_display_gl2_get_default_tex_coords(void)
{
   return &gl2_tex_coords[0];
}

static void *gfx_display_gl2_get_default_mvp(void *data)
{
   gl2_t *gl = (gl2_t*)data;

   if (!gl)
      return NULL;

   return &gl->mvp_no_rot;
}

/* Straight alpha. Into the transparent UI layer, alpha accumulates as
 * coverage, so the layer composites as the UI drawn directly would. */
static void gl2_blend_alpha(const gl2_t *gl)
{
#ifdef GL2_VIEWS
   if (gl->views.ui_pass)
   {
      glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
            GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      return;
   }
#endif
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

static void gfx_display_gl2_blend_begin(void *data)
{
   gl2_t             *gl          = (gl2_t*)data;

   glEnable(GL_BLEND);
   gl2_blend_alpha(gl);

   gl->shader->use(gl, gl->shader_data, VIDEO_SHADER_STOCK_BLEND,
         true);
}

static void gfx_display_gl2_blend_end(void *data)
{
   glDisable(GL_BLEND);
}

#ifdef MALI_BUG
static bool
gfx_display_gl2_discard_draw_rectangle(gl2_t *gl,
      gfx_display_ctx_draw_t *draw,
      unsigned video_dims)
{
   static bool mali_4xx_detected     = false;
   static bool scissor_inited        = false;
   static unsigned last_video_dims   = 0;
   unsigned width                    = VIDEO_SCALE_W(video_dims);
   unsigned height                   = VIDEO_SCALE_H(video_dims);

   if (!scissor_inited)
   {
      unsigned i;
      const char *gpu_device_string = gl->device_str;

      scissor_inited                = true;

      scissor_set_rectangle(0,
            width - 1,
            0,
            height - 1,
            0);

      if (gpu_device_string)
      {
         for (i = 0; scissor_device_strings[i].len; ++i)
         {
            if (strncmp(gpu_device_string,
                     scissor_device_strings[i].str,
                     scissor_device_strings[i].len) == 0)
            {
               mali_4xx_detected = true;
               break;
            }
         }
      }

      last_video_dims   = video_dims;
   }

   /* Early out, to minimise performance impact on
    * non-mali_4xx devices */
   if (!mali_4xx_detected)
      return false;

   /* Have to update scissor_set_rectangle() if the
    * video dimensions change */
   if (video_dims != last_video_dims)
   {
      scissor_set_rectangle(0,
            width - 1,
            0,
            height - 1,
            0);

      last_video_dims = video_dims;
   }

   /* Discards not only out-of-bounds scissoring,
    * but also out-of-view draws.
    *
    * This is intentional.
    */
   return scissor_is_outside_rectangle(
         VIDEO_POS_X(draw->pos), VIDEO_POS_X(draw->pos) + VIDEO_SCALE_W(draw->dims) - 1,
         VIDEO_POS_Y(draw->pos), VIDEO_POS_Y(draw->pos) + VIDEO_SCALE_H(draw->dims) - 1);
}
#endif

static void gfx_display_gl2_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_dims)
{
   video_coords_t     coords;
   gl2_t             *gl  = (gl2_t*)data;

   if (!gl || !draw)
      return;

#ifdef MALI_BUG
   if (gfx_display_gl2_discard_draw_rectangle(gl, draw, video_dims))
   {
      /*RARCH_WARN("discarded draw rect: %.4i %.4i %.4i %.4i\n",
        VIDEO_POS_X(draw->pos), VIDEO_POS_Y(draw->pos),
        (int)VIDEO_SCALE_W(draw->dims), (int)VIDEO_SCALE_H(draw->dims));*/
      return;
   }
#endif

   /* Default the absent streams into a local copy rather than back
    * into the caller's struct. For the effect pipelines draw->coords
    * is gfx_display's effect coordinates, which point into the effect
    * mesh and are shared by every effect draw; a default written into
    * them would stay there for the next one. */
   coords = *draw->coords;

   if (!coords.vertex)
      coords.vertex        = &gl2_vertexes[0];
   if (!coords.tex_coord)
      coords.tex_coord     = &gl2_tex_coords[0];
   if (!coords.lut_tex_coord)
      coords.lut_tex_coord = &gl2_tex_coords[0];

   glViewport(VIDEO_POS_X(draw->pos), VIDEO_POS_Y(draw->pos),
         VIDEO_SCALE_W(draw->dims), VIDEO_SCALE_H(draw->dims));
   glBindTexture(GL_TEXTURE_2D, draw->texture
         ? (GLuint)draw->texture : gl->white_texture);

#if !defined(HAVE_OPENGLES) && !defined(HAVE_PSGL)
   /* A half-float texture is linear scRGB: into an RGBA16F layer,
    * through the program that inverts its encode */
   if (     gl->scrgb.tex_fp16
         && gl2_fp16_count
         && draw->texture
         && gl2_fp16_is((GLuint)draw->texture))
   {
      GLuint prog = gl2_linear_program(gl);
      if (prog)
      {
         const math_matrix_4x4 *mat = draw->matrix_data
            ? (const math_matrix_4x4*)draw->matrix_data
            : (const math_matrix_4x4*)&gl->mvp_no_rot;
         glUseProgram(prog);
         glUniformMatrix4fv(gl->scrgb.lin_loc_mvp, 1, GL_FALSE, mat->data);
         glUniform1i(gl->scrgb.lin_loc_tex, 0);
         glUniform2f(gl->scrgb.lin_loc_params, gl->scrgb.menu_nits,
               (float)gl->scrgb.expand_gamut);
         glBindBuffer(GL_ARRAY_BUFFER, 0);
         glEnableVertexAttribArray(0);
         glEnableVertexAttribArray(1);
         glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, coords.vertex);
         glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, coords.tex_coord);
         if (coords.color)
         {
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 0, coords.color);
         }
         else
            glVertexAttrib4f(2, 1.0f, 1.0f, 1.0f, 1.0f);
         glDrawArrays(GL_TRIANGLE_STRIP, 0, coords.vertices);
         glDisableVertexAttribArray(0);
         glDisableVertexAttribArray(1);
         glDisableVertexAttribArray(2);
         glUseProgram(0);
         gl->shader->use(gl, gl->shader_data, VIDEO_SHADER_STOCK_BLEND,
               true);
         gl->coords.color = gl->white_color_ptr;
         return;
      }
   }
#endif

   gl->shader->set_coords(gl->shader_data, &coords);
   gl->shader->set_mvp(gl->shader_data,
         draw->matrix_data ? (math_matrix_4x4*)draw->matrix_data
      : (math_matrix_4x4*)&gl->mvp_no_rot);


   /* Menu draws use a triangle-strip layout. */
   glDrawArrays(GL_TRIANGLE_STRIP, 0, coords.vertices);

   gl->coords.color     = gl->white_color_ptr;
}

static void gfx_display_gl2_draw_pipeline(
      gfx_display_ctx_draw_t *draw,
      gfx_display_t *p_disp,
      void *data,
      unsigned video_dims)
{
#ifdef HAVE_SHADERPIPELINE
   struct uniform_info uniform_param;
   gl2_t             *gl            = (gl2_t*)data;
   /* The effects' clock, one step ahead, as this driver has always
    * drawn them */
   float t                          = p_disp->effect_time + 0.01f;

   draw->pos                        = VIDEO_POS_PACK(0, 0);
   draw->coords                     = gfx_display_effect_coords(p_disp);
   if (!draw->coords)
      return;
   draw->matrix_data                = NULL;

   switch (draw->pipeline_id)
   {
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
         glBlendFunc(GL_DST_COLOR, GL_ONE);
         break;
      default:
         gl2_blend_alpha(gl);
         break;
   }

   switch (draw->pipeline_id)
   {
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
      case VIDEO_SHADER_MENU_6:
         gl->shader->use(gl, gl->shader_data, draw->pipeline_id,
               true);

         if (t > 65536.0f)
            t -= 65536.0f;

         uniform_param.type              = UNIFORM_1F;
         uniform_param.enabled           = true;
         uniform_param.location          = 0;
         uniform_param.count             = 0;

         uniform_param.lookup.type       = SHADER_PROGRAM_VERTEX;
         uniform_param.lookup.ident      = "time";
         uniform_param.lookup.idx        = draw->pipeline_id;
         uniform_param.lookup.add_prefix = true;
         uniform_param.lookup.enable     = true;

         uniform_param.result.f.v0       = t;

         gl->shader->set_uniform_parameter(gl->shader_data,
               &uniform_param, NULL);
         break;
   }

   switch (draw->pipeline_id)
   {
      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
      case VIDEO_SHADER_MENU_6:
#ifndef HAVE_PSGL
         uniform_param.type              = UNIFORM_2F;
         uniform_param.lookup.ident      = "OutputSize";
         uniform_param.result.f.v0       = VIDEO_SCALE_W(draw->dims);
         uniform_param.result.f.v1       = VIDEO_SCALE_H(draw->dims);

         gl->shader->set_uniform_parameter(gl->shader_data,
               &uniform_param, NULL);
#endif
         break;
   }
#endif
}

#ifdef HAVE_GLSL
static void gl2_free_meshes(gl2_t *gl)
{
   unsigned i;
   for (i = 0; i < ARRAY_SIZE(gl->meshes); i++)
   {
      if (gl->meshes[i].vbo)
         glDeleteBuffers(1, &gl->meshes[i].vbo);
      if (gl->meshes[i].ibo)
         glDeleteBuffers(1, &gl->meshes[i].ibo);
      gl->meshes[i].vbo = 0;
      gl->meshes[i].ibo = 0;
      gl->meshes[i].id  = 0;
   }
}

/* The slot whose buffers hold @mesh, uploaded the first time it is
 * drawn; -1 when they cannot be had, and the mesh is streamed */
static int gl2_mesh_slot(gl2_t *gl, const gfx_display_mesh_t *mesh)
{
   uint64_t oldest = (uint64_t)-1;
   int slot        = -1;
   unsigned i;

   for (i = 0; i < ARRAY_SIZE(gl->meshes); i++)
   {
      if (gl->meshes[i].vbo && gl->meshes[i].id == mesh->id)
         return (int)i;
      if (!gl->meshes[i].vbo)
      {
         if (slot < 0 || gl->meshes[slot].vbo)
            slot = (int)i;
      }
      else if ((slot < 0 || gl->meshes[slot].vbo)
            && gl->meshes[i].last_draw < oldest)
      {
         oldest = gl->meshes[i].last_draw;
         slot   = (int)i;
      }
   }
   if (slot < 0)
      return -1;
   if (gl->meshes[slot].vbo)
      glDeleteBuffers(1, &gl->meshes[slot].vbo);
   if (gl->meshes[slot].ibo)
      glDeleteBuffers(1, &gl->meshes[slot].ibo);
   gl->meshes[slot].vbo = 0;
   gl->meshes[slot].ibo = 0;
   gl->meshes[slot].id  = 0;

   glGenBuffers(1, &gl->meshes[slot].vbo);
   if (!gl->meshes[slot].vbo)
      return -1;
   glBindBuffer(GL_ARRAY_BUFFER, gl->meshes[slot].vbo);
   glBufferData(GL_ARRAY_BUFFER,
         (GLsizeiptr)mesh->vertex_count * sizeof(gfx_display_mesh_vertex_t),
         mesh->vertices, GL_STATIC_DRAW);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   if (mesh->indices)
   {
      glGenBuffers(1, &gl->meshes[slot].ibo);
      if (!gl->meshes[slot].ibo)
      {
         glDeleteBuffers(1, &gl->meshes[slot].vbo);
         gl->meshes[slot].vbo = 0;
         return -1;
      }
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl->meshes[slot].ibo);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            (GLsizeiptr)mesh->index_count * sizeof(uint16_t),
            mesh->indices, GL_STATIC_DRAW);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
   }
   gl->meshes[slot].id = mesh->id;
   return slot;
}

static bool gfx_display_gl2_mesh_draw(void *data, unsigned video_dims,
      const gfx_display_mesh_t *mesh, const float *mvp,
      uintptr_t texture, const float *tint)
{
   math_matrix_4x4 user, out;
   gl2_t *gl = (gl2_t*)data;
   GLboolean blend;
   bool drawn;
   int slot;

   if (     !gl || !mesh || !gl->shader
         || gl->shader->type != RARCH_SHADER_GLSL)
      return false;
   if ((slot = gl2_mesh_slot(gl, mesh)) < 0)
      return false;
   gl->meshes[slot].last_draw = ++gl->mesh_draws;

   memcpy(user.data, mvp, sizeof(user.data));
   matrix_4x4_multiply(out, gl->mvp_no_rot, user);

   /* The whole display, as draw() sets it for a strip at the origin */
   glViewport(0, 0, VIDEO_SCALE_W(video_dims), VIDEO_SCALE_H(video_dims));
   glBindTexture(GL_TEXTURE_2D, texture ? (GLuint)texture : gl->white_texture);
   /* Meshes are drawn blended, as every driver draws them */
   blend = glIsEnabled(GL_BLEND);
   glEnable(GL_BLEND);
   gl2_blend_alpha(gl);

   drawn = gl_glsl_draw_mesh(gl->shader_data,
         gl->meshes[slot].vbo, gl->meshes[slot].ibo,
         mesh->indices ? mesh->index_count : mesh->vertex_count,
         mesh->topology == GFX_MESH_TRIANGLE_STRIP, out.data, tint);

   if (!blend)
      glDisable(GL_BLEND);
   return drawn;
}
#endif

static void gfx_display_gl2_scissor_begin(void *data, unsigned video_dims,
      int x, int y, unsigned dims)
{
   unsigned video_height = VIDEO_SCALE_H(video_dims);
   unsigned width        = VIDEO_SCALE_W(dims);
   unsigned height       = VIDEO_SCALE_H(dims);
   glScissor(x, video_height - y - height, width, height);
   glEnable(GL_SCISSOR_TEST);
#ifdef MALI_BUG
   /* TODO/FIXME: If video width/height changes between
    * a call of gfx_display_gl2_scissor_begin() and the
    * next call of gfx_display_gl2_draw() (or if
    * gfx_display_gl2_scissor_begin() is called before the
    * first call of gfx_display_gl2_draw()), the scissor
    * rectangle set here will be overwritten by the initialisation
    * procedure inside gfx_display_gl2_discard_draw_rectangle(),
    * causing the next frame to render glitched content */
   scissor_set_rectangle(x, x + width - 1, y, y + height - 1, 1);
#endif
}

static void gfx_display_gl2_scissor_end(void *data, unsigned video_dims)
{
   unsigned video_width  = VIDEO_SCALE_W(video_dims);
   unsigned video_height = VIDEO_SCALE_H(video_dims);
   glScissor(0, 0, video_width, video_height);
   glDisable(GL_SCISSOR_TEST);
#ifdef MALI_BUG
   scissor_set_rectangle(0, video_width - 1, 0, video_height - 1, 0);
#endif
}

/**
 * FONT DRIVER
 */
static void gl2_raster_font_free(void *data,
      bool is_threaded)
{
   gl2_raster_t *font = (gl2_raster_t*)data;
   if (!font)
      return;

   if (font->font_driver && font->font_data)
      font->font_driver->free(font->font_data);

   if (is_threaded)
   {
      if (
               font->gl
            && font->gl->ctx_driver
            && font->gl->ctx_driver->make_current)
         font->gl->ctx_driver->make_current(true);
   }

   if (font->tex)
   {
      glDeleteTextures(1, &font->tex);
      font->tex = 0;
   }

   free(font);
}

/* Converts atlas texels to LUMINANCE_ALPHA, opaque white with the
 * coverage as alpha inside the atlas and clear past it, and uploads
 * them: the whole texture, its storage made anew, when @respecify is
 * set, otherwise the atlas region @xy0..@xy1 in place. The region is
 * widened to whole groups of four texels, so each row is a multiple of
 * eight bytes and reads the same under any GL_UNPACK_ALIGNMENT, and is
 * packed tightly, which OpenGL ES 2 needs for want of
 * GL_UNPACK_ROW_LENGTH. */
static void gl2_raster_font_upload_atlas(gl2_raster_t *font,
      unsigned xy0, unsigned xy1, bool respecify)
{
   unsigned x, y, x0, y0, x1, y1, xe;
   unsigned tex_w     = VIDEO_SCALE_W(font->tex_dims);
   unsigned tex_h     = VIDEO_SCALE_H(font->tex_dims);
   unsigned aw        = font->atlas->width;
   unsigned ah        = font->atlas->height;
   GLint  gl_internal = GL_LUMINANCE_ALPHA;
   GLenum gl_format   = GL_LUMINANCE_ALPHA;
   uint8_t *tmp, *dst;

   if (respecify)
   {
      x0 = 0;
      y0 = 0;
      x1 = tex_w;
      y1 = tex_h;
   }
   else
   {
      x0 = VIDEO_SCALE_W(xy0);
      y0 = VIDEO_SCALE_H(xy0);
      x1 = VIDEO_SCALE_W(xy1);
      y1 = VIDEO_SCALE_H(xy1);
      if (x1 <= x0 || y1 <= y0 || x1 > aw || y1 > ah)
         return;
      x0 &= ~3u;
      x1  = (x1 + 3) & ~3u;
      if (x1 > tex_w)
         x1 = tex_w;
   }

   if (!(tmp = (uint8_t*)malloc((size_t)(x1 - x0) * (y1 - y0) * 2)))
      return;

   dst = tmp;
   for (y = y0; y < y1; y++)
   {
      xe = (y < ah) ? (x1 < aw ? x1 : aw) : x0;
      if (xe > x0)
      {
         const uint8_t *src = font->atlas->buffer + (size_t)y * aw + x0;
         for (x = x0; x < xe; x++)
         {
            *dst++ = 0xff;
            *dst++ = *src++;
         }
      }
      memset(dst, 0, (size_t)(x1 - xe) * 2);
      dst += (size_t)(x1 - xe) * 2;
   }

   if (respecify)
      glTexImage2D(GL_TEXTURE_2D, 0, gl_internal,
            tex_w, tex_h,
            0, gl_format, GL_UNSIGNED_BYTE, tmp);
   else
      glTexSubImage2D(GL_TEXTURE_2D, 0, (GLint)x0, (GLint)y0,
            (GLsizei)(x1 - x0), (GLsizei)(y1 - y0),
            gl_format, GL_UNSIGNED_BYTE, tmp);

   free(tmp);
}

static void *gl2_raster_font_init(void *data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   gl2_raster_t   *font  = (gl2_raster_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   font->gl              = (gl2_t*)data;

   if (!font_renderer_create_default(
            &font->font_driver,
            &font->font_data, font_path, font_size, FONT_ATLAS_FORMAT_A8))
   {
      free(font);
      return NULL;
   }

   if (is_threaded)
      if (
               font->gl
            && font->gl->ctx_driver
            && font->gl->ctx_driver->make_current)
         font->gl->ctx_driver->make_current(false);

   glGenTextures(1, &font->tex);

   GL2_BIND_TEXTURE(font->tex, GL_CLAMP_TO_EDGE, GL_LINEAR, GL_LINEAR);

   font->atlas      = font->font_driver->get_atlas(font->font_data);
   font->tex_dims   = VIDEO_SCALE_PACK(next_pow2(font->atlas->width),
         next_pow2(font->atlas->height));
   /* The atlas may grow, up to the largest texture there is */
   {
      GLint max_tex = 0;
      glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex);
      if (max_tex > 0)
         font->atlas->max_dims = VIDEO_SCALE_PACK(
               (unsigned)max_tex, (unsigned)max_tex);
   }

   gl2_raster_font_upload_atlas(font, 0, 0, true);

   font->atlas->dirty = false;

   if (font->gl)
      glBindTexture(GL_TEXTURE_2D, font->gl->texture[font->gl->tex_index]);

   return font;
}

static int gl2_raster_font_get_message_width(void *data, const char *msg,
      size_t msg_len, float scale)
{
   gl2_raster_t *font = (gl2_raster_t*)data;
   if (!font)
      return 0;
   return font_renderer_get_message_width(font->font_driver,
         font->font_data, msg, msg_len, scale);
}

static void gl2_raster_font_draw_vertices(gl2_t *gl,
      gl2_raster_t *font,
      const video_coords_t *coords)
{
   if (font->respecify)
   {
      gl2_raster_font_upload_atlas(font, 0, 0, true);
      font->respecify      = false;
      font->atlas->dirty   = false;
   }
   else if (font->atlas->dirty)
   {
      unsigned r, xy0, xy1;
      for (r = 0; r < font_atlas_dirty_rects(font->atlas); r++)
      {
         font_atlas_dirty_rect(font->atlas, r, &xy0, &xy1);
         gl2_raster_font_upload_atlas(font, xy0, xy1, false);
      }
      font->atlas->dirty   = false;
   }

   if (gl->shader)
   {
      gl->shader->set_coords(gl->shader_data, coords);
      gl->shader->set_mvp(gl->shader_data,
            &gl->mvp_no_rot);
   }

   glDrawArrays(GL_TRIANGLES, 0, coords->vertices);
}

#define GL2_RASTER_FONT_FLUSH() \
   do \
   { \
      coords.tex_coord     = font_tex_coords; \
      coords.vertex        = font_vertex; \
      coords.color         = font_color; \
      coords.vertices      = i * 6; \
      coords.lut_tex_coord = NULL; \
      if (font->block) \
         video_coord_array_append(&font->block->carr, \
               &coords, coords.vertices); \
      else \
         gl2_raster_font_draw_vertices(gl, font, &coords); \
      i = 0; \
   } while (0)

static void gl2_raster_font_render_message(gl2_t *gl,
      gl2_raster_t *font, const char *msg, size_t msg_len,
      GLfloat scale, const GLfloat color[4], GLfloat pos_x,
      GLfloat pos_y, unsigned text_align)
{
   struct video_coords coords;
   GLfloat color_block[4 * 6];
   int n;
   float line_height;
   struct font_line_metrics *line_metrics = NULL;
   const struct font_glyph *glyph_q       = NULL;
   int i                                  = 0;
   int x                                  = 0;
   int y                                  = 0;
   GLfloat *font_tex_coords               = font->font_tex_coords;
   GLfloat *font_vertex                   = font->font_vertex;
   GLfloat *font_color                    = font->font_color;
   float inv_tex_size_x                   = 1.0f / VIDEO_SCALE_W(font->tex_dims);
   float inv_tex_size_y                   = 1.0f / VIDEO_SCALE_H(font->tex_dims);
   float inv_win_width                    = 1.0f / VIDEO_SCALE_W(gl->vp.dims);
   float inv_win_height                   = 1.0f / VIDEO_SCALE_H(gl->vp.dims);
   const struct font_glyph* (*get_glyph)(void*, uint32_t)
                                          = font->font_driver->get_glyph;
   void *font_data                        = font->font_data;
   bool aligned                           = (text_align == TEXT_ALIGN_RIGHT
                                         || text_align == TEXT_ALIGN_CENTER);

   font->font_driver->get_line_metrics(font->font_data, &line_metrics);
   line_height = line_metrics->height * scale / VIDEO_SCALE_H(gl->vp.dims);
   glyph_q     = get_glyph(font_data, '?');

   for (n = 0; n < 6; n++)
   {
      color_block[4 * n + 0] = color[0];
      color_block[4 * n + 1] = color[1];
      color_block[4 * n + 2] = color[2];
      color_block[4 * n + 3] = color[3];
   }

#define FONT_LAYOUT_ALIGNED aligned
#define FONT_LAYOUT_LINE(line, line_width, count, bytes) \
   do \
   { \
      (void)(count); \
      x = roundf(pos_x * VIDEO_SCALE_W(gl->vp.dims)); \
      y = roundf((pos_y - (float)(line) * line_height) \
            * VIDEO_SCALE_H(gl->vp.dims)); \
      if (text_align == TEXT_ALIGN_RIGHT) \
         x -= (int)((line_width) * scale); \
      else if (text_align == TEXT_ALIGN_CENTER) \
         x -= (int)((line_width) * scale) / 2; \
   } while (0)
#define FONT_LAYOUT_GLYPH(glyph, pen_x, pen_y) \
   do \
   { \
      int off_x   = (glyph)->draw_offset_x; \
      int off_y   = (glyph)->draw_offset_y; \
      int tex_x   = VIDEO_SCALE_W((glyph)->atlas_pos); \
      int tex_y   = VIDEO_SCALE_H((glyph)->atlas_pos); \
      int width   = VIDEO_SCALE_W((glyph)->dims); \
      int height  = VIDEO_SCALE_H((glyph)->dims); \
      int delta_x = (pen_x); \
      int delta_y = -(pen_y); \
      GL_RASTER_FONT_EMIT(0, 0, 1); /* Bottom-left */ \
      GL_RASTER_FONT_EMIT(1, 1, 1); /* Bottom-right */ \
      GL_RASTER_FONT_EMIT(2, 0, 0); /* Top-left */ \
      GL_RASTER_FONT_EMIT(3, 1, 0); /* Top-right */ \
      GL_RASTER_FONT_EMIT(4, 0, 0); /* Top-left */ \
      GL_RASTER_FONT_EMIT(5, 1, 1); /* Bottom-right */ \
      memcpy(&font_color[4 * 6 * i], color_block, sizeof(color_block)); \
      if (++i == MAX_MSG_LEN_CHUNK) \
         GL2_RASTER_FONT_FLUSH(); \
   } while (0)
#define FONT_LAYOUT_LINE_END() \
   do \
   { \
      if (i) \
         GL2_RASTER_FONT_FLUSH(); \
   } while (0)
#include "../font_layout.h"
}

#undef GL2_RASTER_FONT_FLUSH

static void gl2_raster_font_setup_viewport(
      gl2_t *gl,
      gl2_raster_t *font,
      unsigned dims,
      bool full_screen,
      bool video_scale_integer)
{
   gl2_set_viewport(gl, dims, full_screen, true);

   glEnable(GL_BLEND);
   gl2_blend_alpha(gl);
   glBlendEquation(GL_FUNC_ADD);

   glBindTexture(GL_TEXTURE_2D, font->tex);

   if (gl->shader && gl->shader->use)
      gl->shader->use(gl,
            gl->shader_data, VIDEO_SHADER_STOCK_BLEND, true);
}

static void gl2_raster_font_render_msg(
      void *userdata,
      void *data,
      const char *msg, size_t msg_len,
      const struct font_params *params)
{
   font_params_resolved_t rp;
   GLfloat color[4];
   int drop_x, drop_y;
   GLfloat x, y, scale, drop_mod, drop_alpha;
   enum text_alignment text_align    = TEXT_ALIGN_LEFT;
   bool full_screen                  = false ;
   gl2_raster_t                *font = (gl2_raster_t*)data;
   gl2_t *gl                         = (gl2_t*)userdata;
   unsigned dims                      = gl->video_dims;
   bool video_scale_integer          = config_get_ptr()->bools.video_scale_integer;

   if (!font || !msg || !*msg || !gl)
      return;

   /* Asked for before anything is laid out: it may have grown, and the
    * texture coordinates are taken from the texture's size */
   font->atlas = font->font_driver->get_atlas(font->font_data);
   {
      unsigned tex_dims = VIDEO_SCALE_PACK(next_pow2(font->atlas->width),
            next_pow2(font->atlas->height));
      if (tex_dims != font->tex_dims)
      {
         font->tex_dims  = tex_dims;
         font->respecify = true;
      }
   }

   font_driver_resolve_params(params, &rp);
   x           = rp.x;
   y           = rp.y;
   scale       = rp.scale;
   full_screen = rp.full_screen;
   text_align  = rp.text_align;
   drop_x      = rp.drop_x;
   drop_y      = rp.drop_y;
   drop_mod    = rp.drop_mod;
   drop_alpha  = rp.drop_alpha;
   if (rp.color_hp)
   {
      color[0] = rp.color_hp[0];
      color[1] = rp.color_hp[1];
      color[2] = rp.color_hp[2];
      color[3] = rp.color_hp[3];
   }
   else
   {
      color[0] = rp.color[0];
      color[1] = rp.color[1];
      color[2] = rp.color[2];
      color[3] = rp.color[3];
   }
   /* If alpha is 0.0f, turn it into default 1.0f */
   if (color[3] <= 0.0f)
      color[3] = 1.0f;


   if (font->block)
      font->block->fullscreen  = full_screen;
   else
      gl2_raster_font_setup_viewport(gl, font, dims, full_screen,
            video_scale_integer);

   if (    (msg && *msg)
         && font->font_data
         && font->font_driver)
   {
      if (drop_x || drop_y)
      {
         GLfloat color_dark[4];
         color_dark[0] = color[0] * drop_mod;
         color_dark[1] = color[1] * drop_mod;
         color_dark[2] = color[2] * drop_mod;
         color_dark[3] = color[3] * drop_alpha;

         gl2_raster_font_render_message(gl, font, msg, msg_len, scale,
               color_dark,
               x + scale * drop_x / VIDEO_SCALE_W(gl->vp.dims),
               y + scale * drop_y / VIDEO_SCALE_H(gl->vp.dims),
               text_align);
      }

      gl2_raster_font_render_message(gl, font, msg, msg_len, scale,
            color, x, y, text_align);
   }

   if (!font->block)
   {
      /* Restore viewport */
      glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
      glDisable(GL_BLEND);
      gl2_set_viewport(gl, dims, false, true);
   }
}

static const struct font_glyph *gl2_raster_font_get_glyph(
      void *data, uint32_t code)
{
   gl2_raster_t *font = (gl2_raster_t*)data;
   if (font && font->font_driver)
      return font->font_driver->get_glyph((void*)font->font_data, code);
   return NULL;
}

static void gl2_raster_font_flush_block(unsigned dims, void *data)
{
   gl2_raster_t          *font       = (gl2_raster_t*)data;
   video_font_raster_block_t *block  = font ? font->block : NULL;
   gl2_t *gl                         = font ? font->gl : NULL;
   bool video_scale_integer          = config_get_ptr()->bools.video_scale_integer;

   if (!font || !block || !block->carr.coords.vertices || !gl)
      return;

   gl2_raster_font_setup_viewport(gl, font, dims, block->fullscreen,
         video_scale_integer);
   gl2_raster_font_draw_vertices(gl, font, (video_coords_t*)&block->carr.coords);

   /* Restore viewport */
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

   glDisable(GL_BLEND);
   gl2_set_viewport(gl, dims, block->fullscreen, true);
}

static void gl2_raster_font_bind_block(void *data, void *userdata)
{
   gl2_raster_t                *font = (gl2_raster_t*)data;
   video_font_raster_block_t *block = (video_font_raster_block_t*)userdata;

   if (font)
      font->block = block;
}

static bool gl2_raster_font_get_line_metrics(void* data, struct font_line_metrics **metrics)
{
   gl2_raster_t *font   = (gl2_raster_t*)data;
   if (font && font->font_driver && font->font_data)
   {
      font->font_driver->get_line_metrics(font->font_data, metrics);
      return true;
   }
   return false;
}

/*
 * VIDEO DRIVER
 */
static unsigned gl2_get_alignment(unsigned pitch)
{
   if (pitch & 1)
      return 1;
   if (pitch & 2)
      return 2;
   if (pitch & 4)
      return 4;
   return 8;
}

static void gl2_shader_scale(gl2_t *gl,
      video_shader_ctx_scale_t *scaler, unsigned idx)
{
   if (scaler->scale)
   {
      scaler->scale->flags &= ~FBO_SCALE_FLAG_VALID;
      gl->shader->shader_scale(gl->shader_data,
            idx, scaler->scale);
   }
}

static void gl2_size_format(GLint* internalFormat)
{
#ifndef HAVE_PSGL
   switch (*internalFormat)
   {
      case GL_RGB:
         /* FIXME: PS3 does not support this, neither does it have GL_RGB565_OES. */
         *internalFormat = GL_RGB565;
         break;
      case GL_RGBA:
#ifdef HAVE_OPENGLES2
         *internalFormat = GL_RGBA8_OES;
#else
         *internalFormat = GL_RGBA8;
#endif
         break;
   }
#endif
}

#if !defined(HAVE_PSGL) && !defined(ORBIS) && !defined(VITA) && !TARGET_OS_IPHONE
static bool gl2_tex_storage_allowed(void)
{
   static int allowed = -1;

   if (allowed < 0)
   {
      const char *vendor       = (const char*)glGetString(GL_VENDOR);
      const char *renderer     = (const char*)glGetString(GL_RENDERER);
      const char *model        = NULL;
      unsigned long model_id   = 0;

      allowed = 1;

      if (vendor && renderer
            && strstr(vendor, "Qualcomm")
            && strstr(renderer, "Adreno"))
      {
         /* Handle both "Adreno (TM) 830" and "Adreno X1-xx" styles. */
         model = strstr(renderer, "Adreno");

         while (model && *model && (*model < '0' || *model > '9'))
            model++;

         if (model && *model)
            model_id = strtoul(model, NULL, 10);

         if (model_id >= 800 || strstr(renderer, "X1"))
         {
            allowed = 0;
            RARCH_WARN("[GL] Disabling glTexStorage on %s to avoid black-screen regressions on Qualcomm Adreno 8xx/X1 Android drivers.\n",
                  renderer);
         }
      }
   }

   return allowed == 1;
}
#endif

/* This function should only be used without mipmaps
   and when data == NULL */
static void gl2_load_texture_image(GLenum target,
      GLint level,
      GLint internalFormat,
      GLsizei width,
      GLsizei height,
      GLint border,
      GLenum format,
      GLenum type,
      const GLvoid * data)
{
#if !defined(HAVE_PSGL) && !defined(ORBIS) && !defined(VITA) && !TARGET_OS_IPHONE
#ifdef HAVE_OPENGLES2
   enum gl_capability_enum cap = GL_CAPS_TEX_STORAGE_EXT;
#else
   enum gl_capability_enum cap = GL_CAPS_TEX_STORAGE;
#endif

   if (     gl2_tex_storage_allowed()
         && gl_check_capability(cap)
         && internalFormat != GL_BGRA_EXT)
   {
      gl2_size_format(&internalFormat);
#ifdef HAVE_OPENGLES2
      glTexStorage2DEXT(target, 1, internalFormat, width, height);
#else
      glTexStorage2D   (target, 1, internalFormat, width, height);
#endif
   }
   else
#endif
   {
#ifdef HAVE_OPENGLES
      if (gl_check_capability(GL_CAPS_GLES3_SUPPORTED))
#endif
         gl2_size_format(&internalFormat);
      glTexImage2D(target, level, internalFormat, width,
            height, border, format, type, data);
   }
}

static void gl2_create_fbo_texture(gl2_t *gl,
      gl2_renderchain_data_t *chain,
      unsigned i, GLuint texture,
      bool video_smooth, bool force_srgb_disable);

static bool gl2_recreate_fbo(
      gl2_t *gl,
      gl2_renderchain_data_t *chain,
      unsigned i,
      GLuint fbo,
      GLuint* texture
      )
{
   gl2_bind_fb(fbo);
   glDeleteTextures(1, texture);
   glGenTextures(1, texture);
   gl2_create_fbo_texture(gl, chain, i, *texture,
         (gl->flags & GL2_FLAG_FBO_SMOOTH) != 0,
         (gl->flags & GL2_FLAG_FBO_SRGB_DISABLE) != 0);

   gl2_fb_texture_2d(RARCH_GL_FRAMEBUFFER,
         RARCH_GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
         *texture, 0);

   if (gl2_check_fb_status(RARCH_GL_FRAMEBUFFER)
         == RARCH_GL_FRAMEBUFFER_COMPLETE)
      return true;

   RARCH_WARN("[GL] Failed to reinitialize FBO texture.\n");
   return false;
}

static void gl2_set_projection(gl2_t *gl,
      struct video_ortho *ortho, bool allow_rotate)
{
   static math_matrix_4x4 rot     = {
      { 0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    1.0f }
   };
   float radians, cosine, sine;

   /* Calculate projection. */
   matrix_4x4_ortho(gl->mvp_no_rot, ortho->left, ortho->right,
         ortho->bottom, ortho->top, ortho->znear, ortho->zfar);

   if (!allow_rotate)
   {
      gl->mvp = gl->mvp_no_rot;
      return;
   }

   radians                 = M_PI * gl->rotation / 180.0f;
   cosine                  = cosf(radians);
   sine                    = sinf(radians);
   MAT_ELEM_4X4(rot, 0, 0) = cosine;
   MAT_ELEM_4X4(rot, 0, 1) = -sine;
   MAT_ELEM_4X4(rot, 1, 0) = sine;
   MAT_ELEM_4X4(rot, 1, 1) = cosine;
   matrix_4x4_multiply(gl->mvp, rot, gl->mvp_no_rot);
}

static void gl2_set_viewport(gl2_t *gl,
      unsigned dims,
      bool force_full, bool allow_rotate)
{
   gl->vp.full_dims   = dims;
   /* Views place themselves in the whole window, which a GPU
    * screenshot then reads. */
   video_driver_update_viewport(&gl->vp, force_full || gl->views.active,
         (gl->flags & GL2_FLAG_KEEP_ASPECT) ? true : false, false);

   glViewport(VIDEO_POS_X(gl->vp.pos), VIDEO_POS_Y(gl->vp.pos), VIDEO_SCALE_W(gl->vp.dims), VIDEO_SCALE_H(gl->vp.dims));
   gl2_set_projection(gl, &default_ortho, allow_rotate);

   /* Set last backbuffer viewport. */
   if (!force_full)
   {
      gl->out_vp_dims   = gl->vp.dims;
   }
}

static void gl2_renderchain_render(
      gl2_t *gl,
      gl2_renderchain_data_t *chain,
      uint64_t frame_count,
      const struct video_tex_info *tex_info,
      const struct video_tex_info *feedback_info,
      bool video_scale_integer)
{
   int i;
   video_shader_ctx_params_t params;
   static GLfloat fbo_tex_coords[8]       = {0.0f};
   struct video_tex_info fbo_tex_info[GFX_MAX_SHADERS];
   struct video_tex_info *fbo_info        = NULL;
   const struct video_fbo_rect *prev_rect = NULL;
   GLfloat xamt                           = 0.0f;
   GLfloat yamt                           = 0.0f;
   unsigned mip_level                     = 0;
   unsigned fbo_tex_info_cnt              = 0;
   unsigned width                         = VIDEO_SCALE_W(gl->video_dims);
   unsigned height                        = VIDEO_SCALE_H(gl->video_dims);

   /* Render the rest of our passes. */
   gl->coords.tex_coord      = fbo_tex_coords;

   /* Calculate viewports, texture coordinates etc,
    * and render all passes from FBOs, to another FBO. */
   for (i = 1; i < chain->fbo_pass; i++)
   {
      const struct video_fbo_rect *rect = &gl->fbo_rect[i];

      prev_rect = &gl->fbo_rect[i - 1];
      fbo_info  = &fbo_tex_info[i - 1];

      xamt      = (GLfloat)VIDEO_SCALE_W(prev_rect->img_dims)
            / VIDEO_SCALE_W(prev_rect->dims);
      yamt      = (GLfloat)VIDEO_SCALE_H(prev_rect->img_dims)
            / VIDEO_SCALE_H(prev_rect->dims);

      SET_TEXTURE_COORDS(fbo_tex_coords, xamt, yamt);

      fbo_info->tex           = chain->fbo_texture[i - 1];
      fbo_info->input_size[0] = VIDEO_SCALE_W(prev_rect->img_dims);
      fbo_info->input_size[1] = VIDEO_SCALE_H(prev_rect->img_dims);
      fbo_info->tex_size[0]   = VIDEO_SCALE_W(prev_rect->dims);
      fbo_info->tex_size[1]   = VIDEO_SCALE_H(prev_rect->dims);
      memcpy(fbo_info->coord, fbo_tex_coords, sizeof(fbo_tex_coords));
      fbo_tex_info_cnt++;

      gl2_bind_fb(chain->fbo[i]);

      gl->shader->use(gl, gl->shader_data,
            i + 1, true);

      glBindTexture(GL_TEXTURE_2D, chain->fbo_texture[i - 1]);

      mip_level = i + 1;

      if (gl->shader->mipmap_input(gl->shader_data, mip_level)
            && (gl->flags & GL2_FLAG_HAVE_MIPMAP))
         glGenerateMipmap(GL_TEXTURE_2D);

      glClear(GL_COLOR_BUFFER_BIT);

      /* Render to FBO with certain size. */
      gl2_set_viewport(gl,
            rect->img_dims, true, false);

      params.vp_dims       = gl->out_vp_dims;
      params.dims          = prev_rect->img_dims;
      params.tex_dims      = prev_rect->dims;
      params.out_dims      = gl->vp.dims;
      params.frame_counter = (unsigned int)frame_count;
      /* Intermediate passes of the same present: the outer frame's
       * count, read from the shared state since video_info does not
       * reach this far in. */
      params.swap_counter  = (unsigned int)video_thread_swap_count();
      params.info          = tex_info;
      params.prev_info     = gl->prev_info;
      params.feedback_info = feedback_info;
      params.fbo_info      = fbo_tex_info;
      params.fbo_info_cnt  = fbo_tex_info_cnt;

      gl->shader->set_params(&params, gl->shader_data);

      gl->coords.vertices = 4;

      gl->shader->set_coords(gl->shader_data, &gl->coords);
      gl->shader->set_mvp(gl->shader_data, &gl->mvp);

      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   }

#if defined(GL_FRAMEBUFFER_SRGB) && !defined(HAVE_OPENGLES)
   if (chain->flags & GL2_CHAIN_FLAG_HAS_SRGB_FBO)
      glDisable(GL_FRAMEBUFFER_SRGB);
#endif

   /* Render our last FBO texture directly to screen. */
   prev_rect = &gl->fbo_rect[chain->fbo_pass - 1];
   xamt      = (GLfloat)VIDEO_SCALE_W(prev_rect->img_dims)
         / VIDEO_SCALE_W(prev_rect->dims);
   yamt      = (GLfloat)VIDEO_SCALE_H(prev_rect->img_dims)
         / VIDEO_SCALE_H(prev_rect->dims);

   SET_TEXTURE_COORDS(fbo_tex_coords, xamt, yamt);

   /* Push final FBO to list. */
   fbo_info                = &fbo_tex_info[chain->fbo_pass - 1];

   fbo_info->tex           = chain->fbo_texture[chain->fbo_pass - 1];
   fbo_info->input_size[0] = VIDEO_SCALE_W(prev_rect->img_dims);
   fbo_info->input_size[1] = VIDEO_SCALE_H(prev_rect->img_dims);
   fbo_info->tex_size[0]   = VIDEO_SCALE_W(prev_rect->dims);
   fbo_info->tex_size[1]   = VIDEO_SCALE_H(prev_rect->dims);
   memcpy(fbo_info->coord, fbo_tex_coords, sizeof(fbo_tex_coords));
   fbo_tex_info_cnt++;

   /* Render our FBO texture to back buffer (or, under scRGB output,
    * into the SDR offscreen the end-of-frame encode consumes).
    * The macro is kept for the SDR case: on iOS the "backbuffer" is
    * GLKit's FBO, not 0. */
   if (gl->scrgb.active || gl2_needs_pq_downconvert(gl))
      gl2_bind_fb(gl2_frame_target_fbo(gl));
   else
      gl2_renderchain_bind_backbuffer();

   gl->shader->use(gl, gl->shader_data,
         chain->fbo_pass + 1, true);

   glBindTexture(GL_TEXTURE_2D, chain->fbo_texture[chain->fbo_pass - 1]);

   mip_level = chain->fbo_pass + 1;

   if (
            gl->shader->mipmap_input(gl->shader_data, mip_level)
         && (gl->flags & GL2_FLAG_HAVE_MIPMAP))
      glGenerateMipmap(GL_TEXTURE_2D);

   glClear(GL_COLOR_BUFFER_BIT);
   gl2_set_viewport(gl, VIDEO_SCALE_PACK(width, height), false, true);

   params.vp_dims       = gl->out_vp_dims;
   params.dims          = prev_rect->img_dims;
   params.tex_dims      = prev_rect->dims;
   params.out_dims      = gl->vp.dims;
   params.frame_counter = (unsigned int)frame_count;
   /* Last pass of the same present; see above. */
   params.swap_counter  = (unsigned int)video_thread_swap_count();
   params.info          = tex_info;
   params.prev_info     = gl->prev_info;
   params.feedback_info = feedback_info;
   params.fbo_info      = fbo_tex_info;
   params.fbo_info_cnt  = fbo_tex_info_cnt;

   gl->shader->set_params(&params, gl->shader_data);

   gl->coords.vertex    = gl->vertex_ptr;

   gl->coords.vertices  = 4;

   gl->shader->set_coords(gl->shader_data, &gl->coords);
   gl->shader->set_mvp(gl->shader_data, &gl->mvp);

   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   gl->coords.tex_coord = gl->tex_info.coord;
}

static void gl2_renderchain_deinit_fbo(gl2_t *gl,
      gl2_renderchain_data_t *chain)
{
   if (gl)
   {
      if (gl->fbo_feedback)
         gl2_delete_fb(1, &gl->fbo_feedback);
      if (gl->fbo_feedback_texture)
         glDeleteTextures(1, &gl->fbo_feedback_texture);

      gl->flags               &= ~(GL2_FLAG_FBO_INITED
                               |   GL2_FLAG_FBO_FEEDBACK_ENABLE
                                  );
      gl->fbo_feedback_pass    = 0;
      gl->fbo_feedback_texture = 0;
      gl->fbo_feedback         = 0;
   }

   if (chain)
   {
      gl2_delete_fb(chain->fbo_pass, chain->fbo);
      glDeleteTextures(chain->fbo_pass, chain->fbo_texture);

      memset(chain->fbo_texture, 0, sizeof(chain->fbo_texture));
      memset(chain->fbo,         0, sizeof(chain->fbo));

      chain->fbo_pass          = 0;
   }
}

/* Take or give back the core's context around work on its objects -
 * a bind-true / bind-false pair that leaves this thread holding what
 * it held before. Once the ring has taken the context (the flag) that
 * work is marshalled to the main thread, where the context already is
 * current: nothing to bind, and binding would move it. Before the ring
 * has taken it, this thread builds the objects in the core's context
 * and gives it back, ring expected or not; this asks the flag alone,
 * unlike the sites that would leave the context current here. */
static void gl2_bind_core_context(gl2_t *gl, bool enable)
{
   if (     (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
         && !retro_atomic_load_acquire_int(&gl->hw_ring))
      gl->ctx_driver->bind_hw_render(gl->ctx_data, enable);
}

static void gl2_renderchain_deinit_hw_render(gl2_t *gl, gl2_renderchain_data_t *chain)
{
   gl2_bind_core_context(gl, true);
   if (gl->flags    & GL2_FLAG_HW_RENDER_FBO_INIT)
      gl2_delete_fb(gl->textures, gl->hw_render_fbo);
   if (chain->flags & GL2_CHAIN_FLAG_HW_RENDER_DEPTH_INIT)
      gl2_delete_rb(gl->textures, chain->hw_render_depth);
   gl->flags &= ~GL2_FLAG_HW_RENDER_FBO_INIT;

   gl2_bind_core_context(gl, false);
}

static bool gl2_create_fbo_targets(gl2_t *gl, gl2_renderchain_data_t *chain)
{
   size_t i;

   glBindTexture(GL_TEXTURE_2D, 0);
   gl2_gen_fb(chain->fbo_pass, chain->fbo);

   for (i = 0; i < (size_t)chain->fbo_pass; i++)
   {
      gl2_bind_fb(chain->fbo[i]);
      gl2_fb_texture_2d(RARCH_GL_FRAMEBUFFER,
            RARCH_GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, chain->fbo_texture[i], 0);
      if (gl2_check_fb_status(RARCH_GL_FRAMEBUFFER) != RARCH_GL_FRAMEBUFFER_COMPLETE)
         goto error;
   }

   if (gl->fbo_feedback_texture)
   {
      gl2_gen_fb(1, &gl->fbo_feedback);
      gl2_bind_fb(gl->fbo_feedback);
      gl2_fb_texture_2d(RARCH_GL_FRAMEBUFFER,
            RARCH_GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
            gl->fbo_feedback_texture, 0);

      if (gl2_check_fb_status(RARCH_GL_FRAMEBUFFER) != RARCH_GL_FRAMEBUFFER_COMPLETE)
         goto error;

      /* Make sure the feedback textures are cleared
       * so we don't feedback noise. */
      glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
      glClear(GL_COLOR_BUFFER_BIT);
   }

   return true;

error:
   gl2_delete_fb(chain->fbo_pass, chain->fbo);
   if (gl->fbo_feedback)
      gl2_delete_fb(1, &gl->fbo_feedback);
   RARCH_ERR("[GL] Failed to set up frame buffer objects. Multi-pass shading will not work.\n");
   return false;
}

static unsigned gl2_wrap_type_to_enum(enum gfx_wrap_type type)
{
   switch (type)
   {
#ifndef HAVE_OPENGLES
      case RARCH_WRAP_BORDER: /* GL_CLAMP_TO_BORDER: Available since GL 1.3 */
         return GL_CLAMP_TO_BORDER;
#else
      case RARCH_WRAP_BORDER:
#endif
      case RARCH_WRAP_EDGE:
         return GL_CLAMP_TO_EDGE;
      case RARCH_WRAP_REPEAT:
         return GL_REPEAT;
      case RARCH_WRAP_MIRRORED_REPEAT:
         return GL_MIRRORED_REPEAT;
      default:
         break;
   }

   return 0;
}

static GLenum gl2_min_filter_to_mag(GLenum type)
{
   switch (type)
   {
      case GL_LINEAR_MIPMAP_LINEAR:
         return GL_LINEAR;
      case GL_NEAREST_MIPMAP_NEAREST:
         return GL_NEAREST;
      default:
         break;
   }

   return type;
}

static void gl2_create_fbo_texture(gl2_t *gl,
      gl2_renderchain_data_t *chain,
      unsigned i, GLuint texture,
      bool video_smooth, bool force_srgb_disable)
{
   GLenum mag_filter, wrap_enum;
   enum gfx_wrap_type wrap_type;
   bool fp_fbo                   = false;
   bool smooth                   = false;
   GLuint base_filt              = video_smooth ? GL_LINEAR : GL_NEAREST;
   GLuint base_mip_filt          = video_smooth ?
      GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST;
   unsigned mip_level            = i + 2;
   bool mipmapped                = gl->shader->mipmap_input(gl->shader_data, mip_level);
   GLenum min_filter             = mipmapped ? base_mip_filt : base_filt;
   unsigned tex_w                = VIDEO_SCALE_W(gl->fbo_rect[i].dims);
   unsigned tex_h                = VIDEO_SCALE_H(gl->fbo_rect[i].dims);

   if (gl->shader->filter_type(gl->shader_data,
            i + 2, &smooth))
   {
      min_filter = mipmapped ? (smooth ?
            GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST)
         : (smooth ? GL_LINEAR : GL_NEAREST);
   }

   mag_filter = gl2_min_filter_to_mag(min_filter);

   wrap_type  = gl->shader->wrap_type(gl->shader_data, i + 2);

   wrap_enum  = gl2_wrap_type_to_enum(wrap_type);

   GL2_BIND_TEXTURE(texture, wrap_enum, mag_filter, min_filter);

   fp_fbo     = (chain->fbo_scale[i].flags & FBO_SCALE_FLAG_FP_FBO) ? true : false;

   if (fp_fbo)
   {
      if (!(chain->flags & GL2_CHAIN_FLAG_HAS_FP_FBO))
         RARCH_ERR("[GL] Floating-point FBO was requested, but is not supported. Falling back to UNORM. Result may band/clip/etc.!\n");
   }

   if (     fp_fbo
         && (chain->flags & GL2_CHAIN_FLAG_HAS_FP_FBO))
   {
      RARCH_LOG("[GL] FBO pass #%d is floating-point.\n", i);
      gl2_load_texture_image(GL_TEXTURE_2D, 0,
#ifdef HAVE_OPENGLES2
         GL_RGBA,
#else
         GL_RGBA32F,
#endif
         tex_w, tex_h,
         0, GL_RGBA, GL_FLOAT, NULL);
   }
   else
   {
      bool srgb_fbo = (chain->fbo_scale[i].flags & FBO_SCALE_FLAG_SRGB_FBO) ? true : false;

      if (!fp_fbo && srgb_fbo)
      {
         if (!(chain->flags & GL2_CHAIN_FLAG_HAS_SRGB_FBO))
               RARCH_ERR("[GL] sRGB FBO was requested, but it is not supported. Falling back to UNORM. Result may have banding!\n");
      }

      if (force_srgb_disable)
         srgb_fbo = false;

      if (      srgb_fbo
            && (chain->flags & GL2_CHAIN_FLAG_HAS_SRGB_FBO))
      {
         RARCH_LOG("[GL] FBO pass #%d is sRGB.\n", i);
         gl2_load_texture_image(GL_TEXTURE_2D, 0,
#ifdef HAVE_OPENGLES2
            GL_SRGB_ALPHA_EXT,
#else
            GL_SRGB8_ALPHA8,
#endif
            tex_w, tex_h, 0,
#ifdef HAVE_OPENGLES2
            GL_SRGB_ALPHA_EXT,
#else
            GL_RGBA,
#endif
            GL_UNSIGNED_BYTE, NULL);
      }
      else
      {
#if defined(HAVE_OPENGLES)
         glTexImage2D(GL_TEXTURE_2D,
               0, GL_RGBA,
               tex_w, tex_h, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, NULL);
#elif defined(HAVE_PSGL)
         glTexImage2D(GL_TEXTURE_2D,
               0, GL_ARGB_SCE,
               tex_w, tex_h, 0,
               GL_ARGB_SCE, GL_UNSIGNED_BYTE, NULL);
#else
         /* Avoid potential performance
          * reductions on particular platforms. */
         gl2_load_texture_image(GL_TEXTURE_2D,
            0, RARCH_GL_INTERNAL_FORMAT32,
            tex_w, tex_h, 0,
            RARCH_GL_TEXTURE_TYPE32, RARCH_GL_FORMAT32, NULL);
#endif
      }
   }
}

/* Takes how the passes' textures are to be made from the settings.
 * Not for the video thread. */
static void gl2_fbo_settings_latch(gl2_t *gl)
{
   settings_t *settings          = config_get_ptr();
#if HAVE_ODROIDGO2
   bool video_smooth             = settings->bools.video_ctx_scaling ? false : settings->bools.video_smooth;
#else
   bool video_smooth             = settings->bools.video_smooth;
#endif
   gl->flags &= ~(GL2_FLAG_FBO_SMOOTH | GL2_FLAG_FBO_SRGB_DISABLE);
   if (video_smooth)
      gl->flags |= GL2_FLAG_FBO_SMOOTH;
   if (settings->bools.video_force_srgb_disable)
      gl->flags |= GL2_FLAG_FBO_SRGB_DISABLE;
}

static void gl2_create_fbo_textures(gl2_t *gl,
      gl2_renderchain_data_t *chain)
{
   int i;
   bool video_smooth             = (gl->flags & GL2_FLAG_FBO_SMOOTH) != 0;
   bool force_srgb_disable       = (gl->flags & GL2_FLAG_FBO_SRGB_DISABLE) != 0;

   glGenTextures(chain->fbo_pass, chain->fbo_texture);

   for (i = 0; i < chain->fbo_pass; i++)
      gl2_create_fbo_texture(gl,
            (gl2_renderchain_data_t*)gl->renderchain_data,
            i, chain->fbo_texture[i],
            video_smooth, force_srgb_disable);

   if (gl->flags & GL2_FLAG_FBO_FEEDBACK_ENABLE)
   {
      glGenTextures(1, &gl->fbo_feedback_texture);
      gl2_create_fbo_texture(gl,
            (gl2_renderchain_data_t*)gl->renderchain_data,
            gl->fbo_feedback_pass, gl->fbo_feedback_texture,
            video_smooth, force_srgb_disable);
   }

   glBindTexture(GL_TEXTURE_2D, 0);
}

/* Compute FBO geometry.
 * When width/height changes or window sizes change,
 * we have to recalculate geometry of our FBO. */

static void gl2_renderchain_recompute_pass_sizes(
      gl2_t *gl,
      gl2_renderchain_data_t *chain,
      unsigned last_dims, unsigned vp_dims)
{
   size_t i;
   bool size_modified      = false;
   GLint max_size          = 0;
   unsigned last_max_dims  = gl->tex_dims;

   glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);

   /* Calculate viewports for FBOs. */
   for (i = 0; i < (size_t)chain->fbo_pass; i++)
   {
      struct video_fbo_rect  *fbo_rect   = &gl->fbo_rect[i];
      struct gfx_fbo_scale *fbo_scale    = &chain->fbo_scale[i];
      /* The axes scale apart from each other -- a pass can be absolute
       * on one and relative on the other -- so each is found on its own
       * here and the two are joined only when they are stored, which is
       * what stops a pass ever holding half an update. */
      unsigned img_w                     = VIDEO_SCALE_W(fbo_rect->img_dims);
      unsigned img_h                     = VIDEO_SCALE_H(fbo_rect->img_dims);
      unsigned max_img_w                 =
            VIDEO_SCALE_W(fbo_rect->max_img_dims);
      unsigned max_img_h                 =
            VIDEO_SCALE_H(fbo_rect->max_img_dims);

      switch (fbo_scale->type_x)
      {
         case RARCH_SCALE_INPUT:
            img_w     = fbo_scale->scale_x * VIDEO_SCALE_W(last_dims);
            max_img_w = VIDEO_SCALE_W(last_max_dims) * fbo_scale->scale_x;
            break;

         case RARCH_SCALE_ABSOLUTE:
            img_w     = max_img_w = fbo_scale->abs_x;
            break;

         case RARCH_SCALE_VIEWPORT:
            if (gl->rotation % 180 == 90)
               img_w  = max_img_w = fbo_scale->scale_x * VIDEO_SCALE_H(vp_dims);
            else
               img_w  = max_img_w = fbo_scale->scale_x * VIDEO_SCALE_W(vp_dims);
            break;
      }

      switch (fbo_scale->type_y)
      {
         case RARCH_SCALE_INPUT:
            img_h     = VIDEO_SCALE_H(last_dims) * fbo_scale->scale_y;
            max_img_h = VIDEO_SCALE_H(last_max_dims) * fbo_scale->scale_y;
            break;

         case RARCH_SCALE_ABSOLUTE:
            img_h     = max_img_h = fbo_scale->abs_y;
            break;

         case RARCH_SCALE_VIEWPORT:
            if (gl->rotation % 180 == 90)
               img_h  = max_img_h = fbo_scale->scale_y * VIDEO_SCALE_W(vp_dims);
            else
               img_h  = max_img_h = fbo_scale->scale_y * VIDEO_SCALE_H(vp_dims);
            break;
      }

      if (img_w > (unsigned)max_size)
      {
         size_modified = true;
         img_w         = max_size;
      }

      if (img_h > (unsigned)max_size)
      {
         size_modified = true;
         img_h         = max_size;
      }

      if (max_img_w > (unsigned)max_size)
      {
         size_modified = true;
         max_img_w     = max_size;
      }

      if (max_img_h > (unsigned)max_size)
      {
         size_modified = true;
         max_img_h     = max_size;
      }

      if (size_modified)
         RARCH_WARN("[GL] FBO textures exceeded maximum size of GPU (%dx%d). Resizing to fit.\n", max_size, max_size);

      fbo_rect->img_dims     = VIDEO_SCALE_PACK(img_w, img_h);
      fbo_rect->max_img_dims = VIDEO_SCALE_PACK(max_img_w, max_img_h);

      last_dims              = fbo_rect->img_dims;
      last_max_dims          = fbo_rect->max_img_dims;
   }
}

/* Each pass's target remade larger where its pass, as
 * gl2_renderchain_recompute_pass_sizes() last sized them, draws past
 * it. Never smaller, so outputs of different sizes take turns without
 * remaking anything. */
static void gl2_renderchain_grow_fbos(gl2_t *gl,
      gl2_renderchain_data_t *chain)
{
   size_t i;
   for (i = 0; i < (size_t)chain->fbo_pass; i++)
   {
      struct video_fbo_rect *fbo_rect = &gl->fbo_rect[i];
      if (fbo_rect)
      {
         unsigned img_width   = VIDEO_SCALE_W(fbo_rect->max_img_dims);
         unsigned img_height  = VIDEO_SCALE_H(fbo_rect->max_img_dims);

         if (     (img_width  > VIDEO_SCALE_W(fbo_rect->dims))
               || (img_height > VIDEO_SCALE_H(fbo_rect->dims)))
         {
            /* Check proactively since we might suddenly
             * get sizes of tex_w width or tex_h height. */
            unsigned max                    = img_width > img_height ? img_width : img_height;
            unsigned pow2_size              = next_pow2(max);
            bool update_feedback            = (gl->flags & GL2_FLAG_FBO_FEEDBACK_ENABLE)
               && (unsigned)i == gl->fbo_feedback_pass;

            fbo_rect->dims                  = VIDEO_SCALE_PACK(
                  pow2_size, pow2_size);

            gl2_recreate_fbo(gl, chain, (unsigned)i,
                  chain->fbo[i], &chain->fbo_texture[i]);

            /* Update feedback texture in-place so we avoid having to
             * juggle two different fbo_rect structs since they get updated here. */
            if (update_feedback)
            {
               if (gl2_recreate_fbo(gl, chain, (unsigned)i,
                        gl->fbo_feedback, &gl->fbo_feedback_texture))
               {
                  /* Make sure the feedback textures are cleared
                   * so we don't feedback noise. */
                  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
                  glClear(GL_COLOR_BUFFER_BIT);
               }
            }

            RARCH_LOG("[GL] Recreating FBO texture #%d: %ux%u.\n",
                  i, VIDEO_SCALE_W(fbo_rect->dims),
                  VIDEO_SCALE_H(fbo_rect->dims));
         }
      }
   }
}

static void gl2_renderchain_start_render(gl2_t *gl,
      gl2_renderchain_data_t *chain, bool video_scale_integer)
{
   /* Used when rendering to an FBO.
    * Texture coords have to be aligned
    * with vertex coordinates. */
   static const GLfloat fbo_vertexes[] = {
      0, 0,
      1, 0,
      0, 1,
      1, 1
   };
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
   gl2_bind_fb(chain->fbo[0]);

   gl2_set_viewport(gl,
         gl->fbo_rect[0].img_dims, true, false);

   /* Need to preserve the "flipped" state when in FBO
    * as well to have consistent texture coordinates.
    *
    * We will "flip" it in place on last pass. */
   gl->coords.vertex = fbo_vertexes;

#if defined(GL_FRAMEBUFFER_SRGB) && !defined(HAVE_OPENGLES)
   if (chain->flags & GL2_CHAIN_FLAG_HAS_SRGB_FBO)
      glEnable(GL_FRAMEBUFFER_SRGB);
#endif
}

/* Set up render to texture. */
static void gl2_renderchain_init(
      gl2_t *gl,
      gl2_renderchain_data_t *chain)
{
   int i;
   video_shader_ctx_scale_t scaler;
   unsigned shader_info_num;
   struct gfx_fbo_scale scale, scale_last;

   shader_info_num = gl->shader->num_shaders(gl->shader_data);

   if (!gl || shader_info_num == 0)
      return;

   scale.flags         = 0;
   scaler.scale        = &scale;

   gl2_shader_scale(gl, &scaler, 1);

   scale_last.flags    = 0;
   scaler.scale        = &scale_last;

   gl2_shader_scale(gl, &scaler, shader_info_num);

   /* we always want FBO to be at least initialized on startup for consoles */
   if (      shader_info_num == 1
         && (!(scale.flags & FBO_SCALE_FLAG_VALID)))
      return;

   if (!(gl->flags & GL2_FLAG_HAVE_FBO))
   {
      RARCH_ERR("[GL] Failed to locate FBO functions. Won't be able to use render-to-texture.\n");
      return;
   }

   /* The last pass gets an FBO of its own, drawn to the backbuffer by
    * the stock pass after it, only when the image would differ drawn
    * straight there: a scale other than the viewport at 1.0, an FBO
    * format, or the feedback the preset asks of it. */
   chain->fbo_pass = shader_info_num - 1;
   if (scale_last.flags & FBO_SCALE_FLAG_VALID)
   {
      unsigned feedback_pass;
      if (     !GFX_FBO_SCALE_IS_VIEWPORT(&scale_last)
            || (  gl->shader->get_feedback_pass(gl->shader_data,
                     &feedback_pass)
               && feedback_pass == shader_info_num - 1))
         chain->fbo_pass++;
   }

   if (!chain->fbo_pass)
      return;

   if (!(scale.flags & FBO_SCALE_FLAG_VALID))
   {
      scale.scale_x    = 1.0f;
      scale.scale_y    = 1.0f;
      scale.type_x     = RARCH_SCALE_INPUT;
      scale.type_y     = RARCH_SCALE_INPUT;
      scale.flags     |= FBO_SCALE_FLAG_VALID;
   }

   chain->fbo_scale[0] = scale;

   for (i = 1; i < chain->fbo_pass; i++)
   {
      scaler.scale = &chain->fbo_scale[i];

      gl2_shader_scale(gl, &scaler, i + 1);

      if (!(chain->fbo_scale[i].flags & FBO_SCALE_FLAG_VALID))
      {
         chain->fbo_scale[i].scale_x = chain->fbo_scale[i].scale_y = 1.0f;
         chain->fbo_scale[i].type_x  = chain->fbo_scale[i].type_y  =
            RARCH_SCALE_INPUT;
         chain->fbo_scale[i].flags  |= FBO_SCALE_FLAG_VALID;
      }
   }

   gl2_renderchain_recompute_pass_sizes(gl, chain, gl->tex_dims,
         gl->video_dims);

   for (i = 0; i < chain->fbo_pass; i++)
   {
      unsigned img_dims        = gl->fbo_rect[i].img_dims;

      gl->fbo_rect[i].dims     = VIDEO_SCALE_PACK(
            next_pow2(VIDEO_SCALE_W(img_dims)),
            next_pow2(VIDEO_SCALE_H(img_dims)));
      RARCH_LOG("[GL] Creating FBO %d @ %ux%u.\n", i,
            VIDEO_SCALE_W(gl->fbo_rect[i].dims),
            VIDEO_SCALE_H(gl->fbo_rect[i].dims));
   }

   if (gl->shader->get_feedback_pass(gl->shader_data, &gl->fbo_feedback_pass))
   {
      if (gl->fbo_feedback_pass < (unsigned)chain->fbo_pass)
      {
         RARCH_LOG("[GL] Creating feedback FBO %d @ %ux%u.\n", i,
               VIDEO_SCALE_W(gl->fbo_rect[gl->fbo_feedback_pass].dims),
               VIDEO_SCALE_H(gl->fbo_rect[gl->fbo_feedback_pass].dims));
         gl->flags |=  GL2_FLAG_FBO_FEEDBACK_ENABLE;
      }
      else
      {
         RARCH_WARN("[GL] Tried to create feedback FBO of pass #%u, but there are only %d FBO passes. Will use input texture as feedback texture.\n",
               gl->fbo_feedback_pass, chain->fbo_pass);
         gl->flags &= ~GL2_FLAG_FBO_FEEDBACK_ENABLE;
      }
   }
   else
      gl->flags    &= ~GL2_FLAG_FBO_FEEDBACK_ENABLE;


   gl2_create_fbo_textures(gl, chain);
   if (!gl || !gl2_create_fbo_targets(gl, chain))
   {
      glDeleteTextures(chain->fbo_pass, chain->fbo_texture);
      RARCH_ERR("[GL] Failed to create FBO targets. Will continue without FBO.\n");
      return;
   }

   gl->flags |= GL2_FLAG_FBO_INITED;
}

static bool gl2_renderchain_init_hw_render(
      gl2_t *gl,
      gl2_renderchain_data_t *chain,
      unsigned width, unsigned height)
{
   unsigned i;
   bool depth                           = false;
   bool stencil                         = false;
   GLint max_fbo_size                   = 0;
   GLint max_renderbuffer_size          = 0;
   struct retro_hw_render_callback *hwr =
      video_driver_get_hw_context();

   /* We can only share texture objects through contexts.
    * FBOs are "abstract" objects and are not shared. */
   gl2_bind_core_context(gl, true);

   RARCH_LOG("[GL] Initializing HW render (%ux%u).\n", width, height);
   glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_fbo_size);
   glGetIntegerv(RARCH_GL_MAX_RENDERBUFFER_SIZE, &max_renderbuffer_size);
   RARCH_LOG("[GL] Max texture size: %d px, renderbuffer size: %d px.\n",
         max_fbo_size, max_renderbuffer_size);

   if (!(gl->flags & GL2_FLAG_HAVE_FBO))
      return false;

   RARCH_LOG("[GL] Supports FBO (render-to-texture).\n");

   glBindTexture(GL_TEXTURE_2D, 0);
   gl2_gen_fb(gl->textures, gl->hw_render_fbo);

   depth   = hwr->depth;
   stencil = hwr->stencil;

   if (depth)
   {
      gl2_gen_rb(gl->textures, chain->hw_render_depth);
      chain->flags |= GL2_CHAIN_FLAG_HW_RENDER_DEPTH_INIT;
   }

   for (i = 0; i < gl->textures; i++)
   {
      GLenum status;
      gl2_bind_fb(gl->hw_render_fbo[i]);
      gl2_fb_texture_2d(RARCH_GL_FRAMEBUFFER,
            RARCH_GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl->texture[i], 0);

      if (depth)
      {
         gl2_bind_rb(RARCH_GL_RENDERBUFFER, chain->hw_render_depth[i]);
         gl2_rb_storage(RARCH_GL_RENDERBUFFER,
               stencil ? RARCH_GL_DEPTH24_STENCIL8 : GL_DEPTH_COMPONENT16,
               width, height);
         gl2_bind_rb(RARCH_GL_RENDERBUFFER, 0);

         if (stencil)
         {
#if defined(HAVE_OPENGLES2) || defined(HAVE_OPENGLES1) || defined(GL2_MAC_EXT_FBO)
            /* GLES2 is a bit weird, as always.
             * There's no GL_DEPTH_STENCIL_ATTACHMENT like in desktop GL. */
            gl2_fb_rb(RARCH_GL_FRAMEBUFFER,
                  RARCH_GL_DEPTH_ATTACHMENT,
                  RARCH_GL_RENDERBUFFER,
                  chain->hw_render_depth[i]);
            gl2_fb_rb(RARCH_GL_FRAMEBUFFER,
                  RARCH_GL_STENCIL_ATTACHMENT,
                  RARCH_GL_RENDERBUFFER,
                  chain->hw_render_depth[i]);
#else
            /* We use ARB FBO extensions, no need to check. */
            gl2_fb_rb(RARCH_GL_FRAMEBUFFER,
                  GL_DEPTH_STENCIL_ATTACHMENT,
                  RARCH_GL_RENDERBUFFER,
                  chain->hw_render_depth[i]);
#endif
         }
         else
         {
            gl2_fb_rb(RARCH_GL_FRAMEBUFFER,
                  RARCH_GL_DEPTH_ATTACHMENT,
                  RARCH_GL_RENDERBUFFER,
                  chain->hw_render_depth[i]);
         }
      }

      status = gl2_check_fb_status(RARCH_GL_FRAMEBUFFER);
      if (status != RARCH_GL_FRAMEBUFFER_COMPLETE)
      {
         RARCH_ERR("[GL] Failed to create HW render FBO #%u, error: 0x%04x.\n",
               i, status);
         return false;
      }
   }

   gl2_renderchain_bind_backbuffer();
   gl->flags |= GL2_FLAG_HW_RENDER_FBO_INIT;

   gl2_bind_core_context(gl, false);
   return true;
}

static void gl2_renderchain_bind_prev_texture(
      gl2_t *gl,
      gl2_renderchain_data_t *chain,
      const struct video_tex_info *tex_info)
{
   memmove(gl->prev_info + 1, gl->prev_info,
         sizeof(*tex_info) * (gl->textures - 1));
   memcpy(&gl->prev_info[0], tex_info,
         sizeof(*tex_info));

   /* Implement feedback by swapping out FBO/textures
    * for FBO pass #N and feedbacks. */
   if (gl->flags & GL2_FLAG_FBO_FEEDBACK_ENABLE)
   {
      GLuint tmp_fbo                 = gl->fbo_feedback;
      GLuint tmp_tex                 = gl->fbo_feedback_texture;
      gl->fbo_feedback               = chain->fbo[gl->fbo_feedback_pass];
      gl->fbo_feedback_texture       = chain->fbo_texture[gl->fbo_feedback_pass];
      chain->fbo[gl->fbo_feedback_pass]         = tmp_fbo;
      chain->fbo_texture[gl->fbo_feedback_pass] = tmp_tex;
   }
}

#ifdef HAVE_GL_ASYNC_READBACK
/* bgrx: write 32-bit BGRX (read_viewport_bgrx) instead of BGR24. */
static bool gl2_read_pbo(gl2_t *gl, uint8_t *buffer, bool bgrx)
{
   const uint8_t *ptr = NULL;
#ifdef HAVE_OPENGLES3
   size_t num_pixels = VIDEO_SCALE_AREA(gl->vp.dims);
#endif

   /* Don't readback if we're in menu mode.
    * We haven't buffered up enough frames yet, come back later. */
   if (!gl->pbo_readback_valid[gl->pbo_readback_index])
      return false;

#ifdef GL2_HW_RING_SYNC
   /* Four frames behind is usually done; when the GPU is further back
    * than that the frame is left for the next readback rather than
    * waited on in the map. */
   if (gl->pbo_readback_sync[gl->pbo_readback_index])
   {
      GLsync sync = (GLsync)gl->pbo_readback_sync[gl->pbo_readback_index];
      GLenum status = glClientWaitSync(sync, 0, 0);
      if (status != GL_ALREADY_SIGNALED && status != GL_CONDITION_SATISFIED)
         return false;
      glDeleteSync(sync);
      gl->pbo_readback_sync[gl->pbo_readback_index] = NULL;
   }
#endif

   gl->pbo_readback_valid[gl->pbo_readback_index] = false;
   glBindBuffer(GL_PIXEL_PACK_BUFFER,
         gl->pbo_readback[gl->pbo_readback_index]);

#ifdef HAVE_OPENGLES3
   /* Slower path, but should work on all implementations at least. */
   ptr        = (const uint8_t*)glMapBufferRange(GL_PIXEL_PACK_BUFFER,
         0, num_pixels * sizeof(uint32_t), GL_MAP_READ_BIT);

   if (ptr)
   {
      /* Clamp to the region glReadPixels actually wrote
       * (see gl2_renderchain_readback). */
      unsigned rb_w = MIN(VIDEO_SCALE_W(gl->vp.dims),
            VIDEO_SCALE_W(gl->video_dims));
      unsigned rb_h = MIN(VIDEO_SCALE_H(gl->vp.dims),
            VIDEO_SCALE_H(gl->video_dims));
      if (bgrx)
         video_frame_copy_to_bgrx(buffer, VIDEO_SCALE_W(gl->vp.dims),
               VIDEO_SCALE_H(gl->vp.dims), ptr, rb_w * 4, rb_w, rb_h, true);
      else
         video_frame_convert_rgba_to_bgr(
               (const void*)ptr,
               buffer,
               rb_w * sizeof(uint32_t),
               rb_w * 3,
               rb_w,
               rb_h);
   }
#else
   ptr = (const uint8_t*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
   if (ptr && bgrx)
   {
      /* Already BGRA (GL_BGRA readback): rows are copied as-is. */
      unsigned rb_w = MIN(VIDEO_SCALE_W(gl->vp.dims),
            VIDEO_SCALE_W(gl->video_dims));
      unsigned rb_h = MIN(VIDEO_SCALE_H(gl->vp.dims),
            VIDEO_SCALE_H(gl->video_dims));
      video_frame_copy_to_bgrx(buffer, VIDEO_SCALE_W(gl->vp.dims),
            VIDEO_SCALE_H(gl->vp.dims), ptr, rb_w * 4, rb_w, rb_h, false);
   }
   else if (ptr)
   {
      struct scaler_ctx *ctx = &gl->pbo_readback_scaler;
      scaler_ctx_scale_direct(ctx, buffer, ptr);
   }
#endif

   if (!ptr)
   {
      RARCH_ERR("[GL] Failed to map pixel unpack buffer.\n");
      glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
      return false;
   }

   if (!glUnmapBuffer(GL_PIXEL_PACK_BUFFER))
   {
      glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
      return false;
   }
   glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
   return true;
}
#endif

static bool gl2_renderchain_read_viewport(
      gl2_t *gl,
      uint8_t *buffer, bool is_idle, bool bgrx)
{
   size_t num_pixels      = 0;

   if (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, false);

#ifdef HAVE_GL_ASYNC_READBACK
   /* Lazy init / reinit: (re)initialize PBO readback when recording
    * starts after driver init, or when viewport dimensions change. */
   if (  !(gl->flags & GL2_FLAG_PBO_READBACK_ENABLE)
       || (unsigned)gl->pbo_readback_scaler.in_width  != VIDEO_SCALE_W(gl->vp.dims)
       || (unsigned)gl->pbo_readback_scaler.in_height != VIDEO_SCALE_H(gl->vp.dims))
   {
      if (gl->flags & GL2_FLAG_GPU_RECORDING)
      {
         /* Tear down old PBO resources before reinitializing */
         if (gl->flags & GL2_FLAG_PBO_READBACK_ENABLE)
         {
            gl2_free_pbo_readback(gl);
         }
         gl->flags |= GL2_FLAG_PBO_READBACK_ENABLE;
         if (gl2_init_pbo_readback(gl))
            RARCH_LOG("[GL] (Re)initialized async PBO readback for recording.\n");
      }
   }
#endif

   num_pixels             = VIDEO_SCALE_AREA(gl->vp.dims);

#ifdef HAVE_GL_ASYNC_READBACK
   if (gl->flags & GL2_FLAG_PBO_READBACK_ENABLE)
   {
      if (!gl2_read_pbo(gl, buffer, bgrx))
         goto error;
   }
   else
#endif
   {
      /* Use slow synchronous readbacks. Use this with plain screenshots
         as we don't really care about performance in this case. */

      /* GLES2 only guarantees GL_RGBA/GL_UNSIGNED_BYTE
       * readbacks so do just that.
       * GLES2 also doesn't support reading back data
       * from front buffer, so render a cached frame
       * and have gl_frame() do the readback while it's
       * in the back buffer.
       *
       * Keep codepath similar for GLES and desktop GL.
       */
      gl->readback_buffer_screenshot = malloc(num_pixels * sizeof(uint32_t));

      if (!gl->readback_buffer_screenshot)
         goto error;

      if (!is_idle)
         video_driver_cached_frame();

      {
         /* Clamp to the region glReadPixels actually wrote
          * (see gl2_renderchain_readback). */
         unsigned rb_w = MIN(VIDEO_SCALE_W(gl->vp.dims),
               VIDEO_SCALE_W(gl->video_dims));
         unsigned rb_h = MIN(VIDEO_SCALE_H(gl->vp.dims),
               VIDEO_SCALE_H(gl->video_dims));
         if (bgrx)
            video_frame_copy_to_bgrx(buffer, VIDEO_SCALE_W(gl->vp.dims),
                  VIDEO_SCALE_H(gl->vp.dims),
                  (const uint8_t*)gl->readback_buffer_screenshot,
                  rb_w * 4, rb_w, rb_h, true);
         else
            video_frame_convert_rgba_to_bgr(
                  (const void*)gl->readback_buffer_screenshot,
                  buffer,
                  rb_w * sizeof(uint32_t),
                  rb_w * 3,
                  rb_w,
                  rb_h);
      }

      free(gl->readback_buffer_screenshot);
      gl->readback_buffer_screenshot = NULL;
   }

   if (     (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
         && !gl2_core_context_is_mains(gl))
      gl->ctx_driver->bind_hw_render(gl->ctx_data, true);
   return true;

error:
   if (     (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
         && !gl2_core_context_is_mains(gl))
      gl->ctx_driver->bind_hw_render(gl->ctx_data, true);

   return false;
}

#ifdef HAVE_OPENGLES
#define gl2_renderchain_restore_default_state(gl) \
   glDisable(GL_DEPTH_TEST); \
   glDisable(GL_CULL_FACE); \
   glDisable(GL_DITHER)
#else
#define gl2_renderchain_restore_default_state(gl) \
   if (!(gl->flags & GL2_FLAG_CORE_CONTEXT_IN_USE)) \
      glEnable(GL_TEXTURE_2D); \
   glDisable(GL_DEPTH_TEST); \
   glDisable(GL_CULL_FACE); \
   glDisable(GL_DITHER)
#endif

#ifdef GL2_KMS_FRAMES
/* The frame textures go: their buffers with them */
static void gl2_kms_frames_drop(gl2_t *gl)
{
   if (!gl->kms_frames)
      return;
   /* The cached frame may name one of them */
   video_driver_cached_frame_retire();
   egl_gbm_frames_free(&gl->kms_frames);
}
#endif

static void gl2_renderchain_copy_frame(
      gl2_t *gl,
      gl2_renderchain_data_t *chain,
      bool use_rgba,
      const void *frame,
      unsigned width, unsigned height, unsigned pitch)
{
#ifdef GL2_KMS_FRAMES
   if (gl->kms_frames)
   {
      /* Lent and written: the texture takes the buffer, nothing moves */
      if (egl_gbm_frames_attach(gl->kms_frames, frame, gl->tex_index))
         return;
      /* Any other frame goes up into storage of the texture's own */
      if (egl_gbm_frames_detach(gl->kms_frames, gl->tex_index))
         gl2_load_texture_image(GL_TEXTURE_2D, 0, gl->internal_fmt,
               VIDEO_SCALE_W(gl->tex_dims), VIDEO_SCALE_H(gl->tex_dims), 0,
               gl->texture_type, gl->texture_fmt, gl->empty_buf);
   }
#endif
#if defined(HAVE_PSGL)
   {
      int h;
      size_t buffer_addr        = VIDEO_SCALE_AREA(gl->tex_dims) *
         gl->tex_index * gl->base_size;
      size_t buffer_stride      = VIDEO_SCALE_W(gl->tex_dims) * gl->base_size;
      const uint8_t *frame_copy = frame;
      size_t frame_copy_size    = width * gl->base_size;
      uint8_t           *buffer = (uint8_t*)glMapBuffer(
            GL_TEXTURE_REFERENCE_BUFFER_SCE, GL_READ_WRITE) + buffer_addr;
      for (h = 0; h < height; h++, buffer += buffer_stride, frame_copy += pitch)
         memcpy(buffer, frame_copy, frame_copy_size);

      glUnmapBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE);
   }
#elif defined(HAVE_OPENGLES)
#if defined(HAVE_EGL)
   if (chain->flags & GL2_CHAIN_FLAG_EGL_IMAGES)
   {
      bool new_egl    = false;
      EGLImageKHR img = 0;

      if (gl->ctx_driver->image_buffer_write)
         new_egl      =  gl->ctx_driver->image_buffer_write(
               gl->ctx_data,
               frame, width, height, pitch,
               (gl->base_size == 4),
               gl->tex_index,
               &img);

      if (img == EGL_NO_IMAGE_KHR)
      {
         RARCH_ERR("[GL] Failed to create EGL image.\n");
         return;
      }

      if (new_egl)
         glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)img);
   }
   else
#endif
   {
      glPixelStorei(GL_UNPACK_ALIGNMENT,
            gl2_get_alignment(width * gl->base_size));

      /* Fallback for GLES devices without GL_BGRA_EXT. */
      if (gl->base_size == 4 && use_rgba)
      {
         video_frame_convert_argb8888_to_abgr8888(
               &gl->scaler,
               gl->conv_buffer,
               frame, width, height, pitch);
         glTexSubImage2D(GL_TEXTURE_2D,
               0, 0, 0, width, height, gl->texture_type,
               gl->texture_fmt, gl->conv_buffer);
      }
      else if (gl->flags & GL2_FLAG_HAVE_UNPACK_ROW_LENGTH)
      {
         const GLvoid *data_buf = frame;
#ifdef HAVE_GL_FRAME_MEM
         /* As on the desktop: a frame in frame memory is read out of
          * it by the GL */
         int fm_slot            = -1;
         size_t off             = 0;
         size_t extent          = height ? (size_t)(height - 1) * pitch
            + (size_t)width * gl->base_size : 0;
         if ((fm_slot = gl_frame_mem_bind(gl->frame_mem, frame,
                     extent, &off)) >= 0)
            data_buf            = (const GLvoid*)off;
#endif
         /* Rows are pitch bytes apart, whatever the width */
         glPixelStorei(GL_UNPACK_ALIGNMENT, gl2_get_alignment(pitch));
         glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / gl->base_size);
         glTexSubImage2D(GL_TEXTURE_2D,
               0, 0, 0, width, height, gl->texture_type,
               gl->texture_fmt, data_buf);

         glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#ifdef HAVE_GL_FRAME_MEM
         gl_sw_fb_pushed(&gl->sw_fb, fm_slot);
         if (fm_slot >= 0)
         {
            GFX_INSTR_INC(GFX_INSTR_FRAME_MEM);
            gl_frame_mem_unbind(gl->frame_mem, fm_slot);
         }
#endif
      }
      else
      {
         /* No GL_UNPACK_ROW_LENGTH. */

         const GLvoid *data_buf = frame;
         unsigned pitch_width   = pitch / gl->base_size;

         if (width != pitch_width)
         {
            /* Slow path - conv_buffer is preallocated
             * just in case we hit this path. */
            size_t h;
            const unsigned line_bytes = width * gl->base_size;
            uint8_t *dst              = (uint8_t*)gl->conv_buffer;
            const uint8_t *src        = (const uint8_t*)frame;

            for (h = 0; h < height; h++, src += pitch, dst += line_bytes)
               memcpy(dst, src, line_bytes);

            data_buf                  = gl->conv_buffer;
         }

         glTexSubImage2D(GL_TEXTURE_2D,
               0, 0, 0, width, height, gl->texture_type,
               gl->texture_fmt, data_buf);
      }
   }
#else
   {
      const GLvoid *data_buf = frame;
#ifdef HAVE_GL_FRAME_MEM
      int fm_slot            = -1;
#endif
      glPixelStorei(GL_UNPACK_ALIGNMENT, gl2_get_alignment(pitch));

      if (gl->base_size == 2 && (!(gl->flags & GL2_FLAG_HAVE_ES2_COMPAT)))
      {
         /* Convert to 32-bit textures on desktop GL.
          *
          * It is *much* faster (order of magnitude on my setup)
          * to use a custom SIMD-optimized conversion routine
          * than letting GL do it. */
         video_frame_convert_rgb16_to_rgb32(
               &gl->scaler,
               gl->conv_buffer,
               frame,
               width,
               height,
               pitch);
         data_buf = gl->conv_buffer;
      }
      else
      {
         glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / gl->base_size);
#ifdef HAVE_GL_FRAME_MEM
         /* A frame in one of the threaded wrapper's buffers is read out
          * of it by the GL, the upload queued rather than copied here */
         {
            size_t off    = 0;
            size_t extent = height ? (size_t)(height - 1) * pitch
               + (size_t)width * gl->base_size : 0;
            fm_slot       = gl_frame_mem_bind(gl->frame_mem, frame,
                  extent, &off);
            if (fm_slot >= 0)
               data_buf   = (const GLvoid*)off;
         }
#endif
      }

      glTexSubImage2D(GL_TEXTURE_2D,
            0, 0, 0, width, height, gl->texture_type,
            gl->texture_fmt, data_buf);

      glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#ifdef HAVE_GL_FRAME_MEM
      gl_sw_fb_pushed(&gl->sw_fb, fm_slot);
      if (fm_slot >= 0)
      {
         GFX_INSTR_INC(GFX_INSTR_FRAME_MEM);
         gl_frame_mem_unbind(gl->frame_mem, fm_slot);
      }
#endif
   }
#endif
}

#if !defined(HAVE_OPENGLES2) && !defined(HAVE_PSGL)
#define gl2_renderchain_bind_pbo(idx) glBindBuffer(GL_PIXEL_PACK_BUFFER, (GLuint)idx)
#define gl2_renderchain_unbind_pbo()  glBindBuffer(GL_PIXEL_PACK_BUFFER, 0)
#define gl2_renderchain_init_pbo(size, data) glBufferData(GL_PIXEL_PACK_BUFFER, size, (const GLvoid*)data, GL_STREAM_READ)
#else
#define gl2_renderchain_bind_pbo(idx)
#define gl2_renderchain_unbind_pbo()
#define gl2_renderchain_init_pbo(size, data)
#endif

static void gl2_renderchain_readback(
      gl2_t *gl,
      void *chain_data,
      unsigned alignment,
      unsigned fmt, unsigned type,
      void *src)
{
   glPixelStorei(GL_PACK_ALIGNMENT, alignment);
#ifndef HAVE_OPENGLES
   glPixelStorei(GL_PACK_ROW_LENGTH, 0);
#endif
   /* Under scRGB, read the pre-encode SDR offscreen -- roundtrip-free
    * SDR capture, same as the glcore driver. Not under PQ content:
    * the offscreen then holds PQ, not a displayable SDR image, so
    * fall through to the backbuffer. */
   if (gl->scrgb.active && gl->scrgb.fbo
         && !gl->video_info.source_hdr10)
   {
      gl2_bind_fb(gl->scrgb.fbo);
#ifndef HAVE_OPENGLES
      glReadBuffer(RARCH_GL_COLOR_ATTACHMENT0);
#endif
   }
#ifndef HAVE_OPENGLES
   else
      glReadBuffer(GL_BACK);
#endif

   glReadPixels(
         (VIDEO_POS_X(gl->vp.pos) > 0) ? VIDEO_POS_X(gl->vp.pos) : 0,
         (VIDEO_POS_Y(gl->vp.pos) > 0) ? VIDEO_POS_Y(gl->vp.pos) : 0,
         MIN(VIDEO_SCALE_W(gl->vp.dims), VIDEO_SCALE_W(gl->video_dims)),
         MIN(VIDEO_SCALE_H(gl->vp.dims), VIDEO_SCALE_H(gl->video_dims)),
         (GLenum)fmt, (GLenum)type, (GLvoid*)src);

   if (gl->scrgb.active && gl->scrgb.fbo
         && !gl->video_info.source_hdr10)
      gl2_bind_fb(0);
}

static void gl2_renderchain_fence_iterate(
      void *data,
      gl2_renderchain_data_t *chain,
      unsigned hard_sync_frames)
{
#ifndef HAVE_OPENGLES
#ifdef HAVE_GL_SYNC
   chain->fences[chain->fence_count++] =
      glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

   while (chain->fence_count > hard_sync_frames)
   {
      glClientWaitSync(chain->fences[0],
            GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000);
      glDeleteSync(chain->fences[0]);

      chain->fence_count--;
      memmove(chain->fences, chain->fences + 1,
            chain->fence_count * sizeof(void*));
   }
#endif
#endif
}

static void gl2_renderchain_fence_free(void *data,
      gl2_renderchain_data_t *chain)
{
#ifndef HAVE_OPENGLES
#ifdef HAVE_GL_SYNC
   size_t i;
   for (i = 0; i < chain->fence_count; i++)
   {
      glClientWaitSync(chain->fences[i],
            GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000);
      glDeleteSync(chain->fences[i]);
   }
   chain->fence_count = 0;
#endif
#endif
}

static void gl2_renderchain_init_texture_reference(
      gl2_t *gl,
      gl2_renderchain_data_t *chain,
      unsigned i,
      unsigned internal_fmt, unsigned texture_fmt,
      unsigned texture_type)
{
#ifdef HAVE_PSGL
   glTextureReferenceSCE(GL_TEXTURE_2D, 1,
         VIDEO_SCALE_W(gl->tex_dims), VIDEO_SCALE_H(gl->tex_dims), 0,
         (GLenum)internal_fmt,
         VIDEO_SCALE_W(gl->tex_dims) * gl->base_size,
         VIDEO_SCALE_AREA(gl->tex_dims) * i * gl->base_size);
#else
   if (chain->flags & GL2_CHAIN_FLAG_EGL_IMAGES)
      return;

   gl2_load_texture_image(GL_TEXTURE_2D,
      0,
      (GLenum)internal_fmt,
      VIDEO_SCALE_W(gl->tex_dims), VIDEO_SCALE_H(gl->tex_dims), 0,
      (GLenum)texture_type,
      (GLenum)texture_fmt,
      gl->empty_buf ? gl->empty_buf : NULL);
#endif
}

static void gl2_renderchain_resolve_extensions(gl2_t *gl,
      gl2_renderchain_data_t *chain,
      const char *context_ident,
      const video_info_t *video)
{
   settings_t *settings             = config_get_ptr();
   bool force_srgb_disable          = settings->bools.video_force_srgb_disable;

   if (!chain)
      return;

   chain->flags    &= ~GL2_CHAIN_FLAG_HAS_SRGB_FBO;
   if (gl_check_capability(GL_CAPS_FP_FBO))
      chain->flags |=  GL2_CHAIN_FLAG_HAS_FP_FBO;
   else
      chain->flags &= ~GL2_CHAIN_FLAG_HAS_FP_FBO;

   if (!force_srgb_disable)
   {
      if (gl_check_capability(GL_CAPS_SRGB_FBO))
         chain->flags |=  GL2_CHAIN_FLAG_HAS_SRGB_FBO;
      else
         chain->flags &= ~GL2_CHAIN_FLAG_HAS_SRGB_FBO;
   }

   /* Use regular textures if we use HW render. */
   if ( (!(gl->flags & GL2_FLAG_HW_RENDER_USE))
      &&  (gl_check_capability(GL_CAPS_EGLIMAGE))
      &&  (gl->ctx_driver->image_buffer_init)
      &&  (gl->ctx_driver->image_buffer_init(gl->ctx_data, video)))
      chain->flags |=  GL2_CHAIN_FLAG_EGL_IMAGES;
   else
      chain->flags &= ~GL2_CHAIN_FLAG_EGL_IMAGES;
}

static void gl_load_texture_data(
      GLuint id,
      enum gfx_wrap_type wrap_type,
      enum texture_filter_type filter_type,
      unsigned alignment,
      unsigned width, unsigned height,
      const void *frame, unsigned base_size, bool pix10)
{
   GLint mag_filter, min_filter;
   bool want_mipmap = false;
   bool use_rgba    = (video_driver_get_disp_flags() & VIDEO_FLAG_USE_RGBA);
   bool rgb32       = (base_size == (sizeof(uint32_t)));
   GLenum wrap      = gl2_wrap_type_to_enum(wrap_type);
   bool have_mipmap = gl_check_capability(GL_CAPS_MIPMAP);

   if (!have_mipmap)
   {
      /* Assume no mipmapping support. */
      switch (filter_type)
      {
         case TEXTURE_FILTER_MIPMAP_LINEAR:
            filter_type = TEXTURE_FILTER_LINEAR;
            break;
         case TEXTURE_FILTER_MIPMAP_NEAREST:
            filter_type = TEXTURE_FILTER_NEAREST;
            break;
         default:
            break;
      }
   }

   switch (filter_type)
   {
      case TEXTURE_FILTER_MIPMAP_LINEAR:
         min_filter = GL_LINEAR_MIPMAP_NEAREST;
         mag_filter = GL_LINEAR;
         want_mipmap = true;
         break;
      case TEXTURE_FILTER_MIPMAP_NEAREST:
         min_filter = GL_NEAREST_MIPMAP_NEAREST;
         mag_filter = GL_NEAREST;
         want_mipmap = true;
         break;
      case TEXTURE_FILTER_NEAREST:
         min_filter = GL_NEAREST;
         mag_filter = GL_NEAREST;
         break;
      case TEXTURE_FILTER_LINEAR:
      default:
         min_filter = GL_LINEAR;
         mag_filter = GL_LINEAR;
         break;
   }

   GL2_BIND_TEXTURE(id, wrap, mag_filter, min_filter);

   glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
#if !defined(HAVE_OPENGLES) && !defined(HAVE_PSGL)
   /* XRGB2101010, the core frames' layout and upload: GL_BGRA reads
    * A from 31:30 and R from 29:20, so no swizzle. */
   if (pix10)
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB10_A2, width, height, 0,
            GL_BGRA, GL_UNSIGNED_INT_2_10_10_10_REV, frame);
   else
#endif
   glTexImage2D(GL_TEXTURE_2D,
         0,
         (use_rgba || !rgb32)
         ? GL_RGBA
         : RARCH_GL_INTERNAL_FORMAT32,
         width, height, 0,
         (use_rgba || !rgb32)
         ? GL_RGBA
         : RARCH_GL_TEXTURE_TYPE32,
         (rgb32)
         ? RARCH_GL_FORMAT32
         : GL_UNSIGNED_SHORT_4_4_4_4,
         frame);

   if (want_mipmap && have_mipmap)
      glGenerateMipmap(GL_TEXTURE_2D);
}

static bool gl2_add_lut(
      const char *lut_path,
      bool lut_mipmap,
      unsigned lut_filter,
      enum gfx_wrap_type lut_wrap_type,
      size_t i, GLuint *textures_lut)
{
   struct texture_image img;
   enum texture_filter_type filter_type = TEXTURE_FILTER_LINEAR;

   memset(&img, 0, sizeof(img));
   img.supports_rgba = (video_driver_get_disp_flags() & VIDEO_FLAG_USE_RGBA);

   if (!image_texture_load(&img, lut_path))
   {
      RARCH_ERR("[GL] Failed to load texture image from: \"%s\".\n",
            lut_path);
      return false;
   }

   RARCH_LOG("[GL] Loaded texture image from: \"%s\".\n",
         lut_path);

   if (lut_filter == RARCH_FILTER_NEAREST)
      filter_type = TEXTURE_FILTER_NEAREST;

   if (lut_mipmap)
   {
      if (filter_type == TEXTURE_FILTER_NEAREST)
         filter_type = TEXTURE_FILTER_MIPMAP_NEAREST;
      else
         filter_type = TEXTURE_FILTER_MIPMAP_LINEAR;
   }

   gl_load_texture_data(
         textures_lut[i],
         lut_wrap_type,
         filter_type, 4,
         img.width, img.height,
         img.pixels, sizeof(uint32_t), false);
   image_texture_free(&img);

   return true;
}

bool gl2_load_luts(
      const void *shader_data,
      GLuint *textures_lut)
{
   size_t i;
   const struct video_shader *shader =
      (const struct video_shader*)shader_data;
   unsigned num_luts                 = MIN(shader->luts, GFX_MAX_TEXTURES);

   if (!shader->luts)
      return true;

   glGenTextures(num_luts, textures_lut);

   for (i = 0; i < num_luts; i++)
   {
      if (!gl2_add_lut(
               shader->lut[i].path,
               shader->lut[i].mipmap,
               shader->lut[i].filter,
               shader->lut[i].wrap,
               i, textures_lut))
         return false;
   }

   glBindTexture(GL_TEXTURE_2D, 0);
   return true;
}

#ifdef HAVE_OVERLAY
static void gl2_free_overlay(gl2_t *gl)
{
   /* A page shown through load_textures holds the pack's names, which
    * are the pack's to delete (input_overlay_release_textures) and are
    * on the pack's other pages as well. */
   if (gl->overlay_tex && !(gl->flags & GL2_FLAG_OVERLAY_BORROWED))
      glDeleteTextures(gl->overlays, gl->overlay_tex);
   gl->flags &= ~GL2_FLAG_OVERLAY_BORROWED;

   /* The three coordinate arrays are views into the overlay_tex block. */
   free(gl->overlay_tex);
   gl->overlay_tex          = NULL;
   gl->overlay_vertex_coord = NULL;
   gl->overlay_tex_coord    = NULL;
   gl->overlay_color_coord  = NULL;
   gl->overlays             = 0;
}

static void gl2_overlay_vertex_geom(void *data,
      unsigned image,
      float x, float y,
      float w, float h)
{
   GLfloat *vertex = NULL;
   gl2_t *gl       = (gl2_t*)data;

   if (!gl || !gl->overlay_vertex_coord)
      return;

   if (image >= gl->overlays)
   {
      RARCH_ERR("[GL] Invalid overlay id: %u\n", image);
      return;
   }

   vertex          = (GLfloat*)&gl->overlay_vertex_coord[image * 8];

   /* Flipped, so we preserve top-down semantics. */
   y               = 1.0f - y;
   h               = -h;

   vertex[0]       = x;
   vertex[1]       = y;
   vertex[2]       = x + w;
   vertex[3]       = y;
   vertex[4]       = x;
   vertex[5]       = y + h;
   vertex[6]       = x + w;
   vertex[7]       = y + h;
}

static void gl2_overlay_tex_geom(void *data,
      unsigned image,
      GLfloat x, GLfloat y,
      GLfloat w, GLfloat h)
{
   GLfloat *tex = NULL;
   gl2_t *gl    = (gl2_t*)data;

   if (!gl || !gl->overlay_tex_coord)
      return;

   if (image >= gl->overlays)
      return;

   tex          = (GLfloat*)&gl->overlay_tex_coord[image * 8];

   tex[0]       = x;
   tex[1]       = y;
   tex[2]       = x + w;
   tex[3]       = y;
   tex[4]       = x;
   tex[5]       = y + h;
   tex[6]       = x + w;
   tex[7]       = y + h;
}

static void gl2_render_overlay(gl2_t *gl)
{
   unsigned i;
   unsigned width                      = VIDEO_SCALE_W(gl->video_dims);
   unsigned height                     = VIDEO_SCALE_H(gl->video_dims);

   glEnable(GL_BLEND);

   /* Views place themselves in the whole window, and so does the
    * overlay, as the input side hit-tests it. */
   if ((gl->flags & GL2_FLAG_OVERLAY_FULLSCREEN) || gl->views.active)
      glViewport(0, 0, width, height);

   /* Ensure that we reset the attrib array. */
   gl->shader->use(gl, gl->shader_data,
         VIDEO_SHADER_STOCK_BLEND, true);

   gl->coords.vertex    = gl->overlay_vertex_coord;
   gl->coords.tex_coord = gl->overlay_tex_coord;
   gl->coords.color     = gl->overlay_color_coord;
   gl->coords.vertices  = 4 * gl->overlays;

   gl->shader->set_coords(gl->shader_data, &gl->coords);
   gl->shader->set_mvp(gl->shader_data, &gl->mvp_no_rot);

   for (i = 0; i < gl->overlays; i++)
   {
      glBindTexture(GL_TEXTURE_2D, gl->overlay_tex[i]);
      glDrawArrays(GL_TRIANGLE_STRIP, 4 * i, 4);
   }

   glDisable(GL_BLEND);
   gl->coords.vertex    = gl->vertex_ptr;
   gl->coords.tex_coord = gl->tex_info.coord;
   gl->coords.color     = gl->white_color_ptr;
   gl->coords.vertices  = 4;
   if ((gl->flags & GL2_FLAG_OVERLAY_FULLSCREEN) || gl->views.active)
      glViewport(VIDEO_POS_X(gl->vp.pos), VIDEO_POS_Y(gl->vp.pos), VIDEO_SCALE_W(gl->vp.dims), VIDEO_SCALE_H(gl->vp.dims));
}
#endif

static void gl2_set_viewport_wrapper(void *data, unsigned dims,
      bool force_full, bool allow_rotate)
{
   gl2_t              *gl = (gl2_t*)data;
   gl2_set_viewport(gl, dims, force_full, allow_rotate);
}

/* Shaders */

/**
 * gl2_get_fallback_shader_type:
 * @type                      : shader type which should be used if possible
 *
 * Returns a supported fallback shader type in case the given one is not supported.
 * For gl2, shader support is completely defined by the context driver shader flags.
 *
 * gl2_get_fallback_shader_type(RARCH_SHADER_NONE) returns a default shader type.
 * if gl2_get_fallback_shader_type(type) != type, type was not supported.
 *
 * Returns: A supported shader type.
 *  If RARCH_SHADER_NONE is returned, no shader backend is supported.
 **/
static enum rarch_shader_type gl2_get_fallback_shader_type(enum rarch_shader_type type)
{
#if defined(HAVE_GLSL) || defined(HAVE_CG)
   int i;
   gfx_ctx_flags_t flags;
   flags.flags     = 0;
   video_context_driver_get_flags(&flags);

   if (type != RARCH_SHADER_CG && type != RARCH_SHADER_GLSL)
   {
      type = GL2_DEFAULT_SHADER_TYPE;

      if (type != RARCH_SHADER_CG && type != RARCH_SHADER_GLSL)
         type = RARCH_SHADER_GLSL;
   }

   for (i = 0; i < 2; i++)
   {
      switch (type)
      {
         case RARCH_SHADER_CG:
#ifdef HAVE_CG
            if (BIT32_GET(flags.flags, GFX_CTX_FLAGS_SHADERS_CG))
               return type;
#endif
            type = RARCH_SHADER_GLSL;
            break;

         case RARCH_SHADER_GLSL:
#ifdef HAVE_GLSL
            if (BIT32_GET(flags.flags, GFX_CTX_FLAGS_SHADERS_GLSL))
               return type;
#endif
            type = RARCH_SHADER_CG;
            break;

         default:
            return RARCH_SHADER_NONE;
      }
   }
#endif
   return RARCH_SHADER_NONE;
}

static const shader_backend_t *gl_shader_driver_set_backend(
      enum rarch_shader_type type)
{
   enum rarch_shader_type fallback = gl2_get_fallback_shader_type(type);
   if (fallback != type)
      RARCH_ERR("[GL] Shader backend %d not supported, falling back to %d.\n", type, fallback);

   switch (fallback)
   {
#ifdef HAVE_CG
      case RARCH_SHADER_CG:
         RARCH_LOG("[GL] Using Cg shader backend.\n");
         return &gl_cg_backend;
#endif
#ifdef HAVE_GLSL
      case RARCH_SHADER_GLSL:
         RARCH_LOG("[GL] Using GLSL shader backend.\n");
         return &gl_glsl_backend;
#endif
      default:
         RARCH_LOG("[GL] No supported shader backend.\n");
         return NULL;
   }
}

static bool gl_shader_driver_init(video_shader_ctx_init_t *init)
{
   void            *tmp = NULL;
   settings_t *settings = config_get_ptr();

   if (!init->shader || !init->shader->init)
   {
      init->shader = gl_shader_driver_set_backend(init->shader_type);

      if (!init->shader)
         return false;
   }

   tmp = init->shader->init(init->data, init->path);

   if (!tmp)
      return false;

   if (string_is_equal(settings->arrays.menu_driver, "xmb")
         && init->shader->init_menu_shaders)
   {
      RARCH_LOG("[GL] Setting up menu pipeline shaders for XMB...\n");
      init->shader->init_menu_shaders(tmp);
   }

   init->shader_data = tmp;

   return true;
}

static bool gl2_shader_init(gl2_t *gl, const gfx_ctx_driver_t *ctx_driver,
      struct retro_hw_render_callback *hwr
      )
{
   video_shader_ctx_init_t init_data;
   bool ret                          = false;
   const char *shader_path           = video_shader_get_current_shader_preset();
   enum rarch_shader_type parse_type = video_shader_parse_type(shader_path);
   enum rarch_shader_type type;

   type = gl2_get_fallback_shader_type(parse_type);

   if (type == RARCH_SHADER_NONE)
   {
      RARCH_ERR("[GL] Couldn't find any supported shader backend! Continuing without shaders.\n");
      return true;
   }

   if (type != parse_type)
   {
      if (shader_path && *shader_path)
         RARCH_WARN("[GL] Shader preset %s is using unsupported shader type %s, falling back to stock %s.\n",
            shader_path, video_shader_type_to_str(parse_type), video_shader_type_to_str(type));

      shader_path = NULL;
   }

#ifdef HAVE_GLSL
   if (type == RARCH_SHADER_GLSL)
      gl_glsl_set_context_type(gl->flags & GL2_FLAG_CORE_CONTEXT_IN_USE,
            hwr->version_major, hwr->version_minor);
#endif

   init_data.gl.core_context_enabled = (gl->flags & GL2_FLAG_CORE_CONTEXT_IN_USE) ? true : false;
   init_data.shader_type             = type;
   init_data.shader                  = NULL;
   init_data.shader_data             = NULL;
   init_data.data                    = gl;
   init_data.path                    = shader_path;

   if (gl_shader_driver_init(&init_data))
   {
      gl->shader                     = init_data.shader;
      gl->shader_data                = init_data.shader_data;
      gl->views.preset               = shader_path && *shader_path;
      return true;
   }

   RARCH_ERR("[GL] Failed to initialize shader, falling back to stock.\n");

   init_data.shader                  = NULL;
   init_data.shader_data             = NULL;
   init_data.path                    = NULL;

   ret                               = gl_shader_driver_init(&init_data);

   gl->shader                        = init_data.shader;
   gl->shader_data                   = init_data.shader_data;

   return ret;
}

/* Whether the threaded wrapper's hardware ring will drive this driver:
 * decided at init, when the wrapper is already up, from the core's
 * context type and the setting. */
/* Whether the core's context belongs to the main thread: it does once
 * the wrapper's ring has taken it (the flag), and it will as soon as
 * the ring is set up (expected, from init on). Every place this
 * thread would take that context for itself asks this, and only this.
 * Asking only the flag let a bind during init - the stock shader's
 * load is one - take the context here after the ring was decided but
 * before it was set up, and the main thread's own bind then failed:
 * a core with no current context, and no GL function resolved. */
static bool gl2_core_context_is_mains(gl2_t *gl)
{
   /* Both bits are set on the video thread inside blocking command
    * handlers (init, ring bring-up) while the main thread is parked
    * in the wrapper's send-and-wait, and read here from the frame
    * path. The live-settings consultation this replaces read
    * settings->arrays.video_driver every frame from the video
    * thread with the main thread running free. */
   return retro_atomic_load_acquire_int(&gl->hw_ring)
      || (gl->flags & GL2_FLAG_HW_RING_EXPECTED);
}


/* --- the threaded wrapper's hardware ring ------------------------------ */

/* The core's context, current on the caller - the main thread. The
 * context driver created it shared with this thread's at init, with
 * the HW-render FBOs already made inside it. From here on the frame
 * never takes it back. */
/* True on a context that cannot share: the ring then hands this
 * driver's one context between the threads (video_thread_hw.c), and
 * this driver neither makes a shared context nor binds one. Read from
 * the context driver, whose answer is fixed once it is up. */
static bool gl2_ctx_no_shared_context(gl2_t *gl)
{
   return gl->ctx_driver && gl->ctx_driver->get_flags
      && BIT32_GET(gl->ctx_driver->get_flags(gl->ctx_data),
            GFX_CTX_FLAGS_NO_SHARED_CONTEXT);
}

static bool gl2_hw_ring_context_new(void *data, void **ctx)
{
   gl2_t *gl = (gl2_t*)data;
   if (!gl || !ctx || !gl->ctx_driver || !gl->ctx_driver->bind_hw_render)
      return false;
   /* Asked now, with the context up: a driver that could not make the
    * shared context says so only once it has tried. One context for
    * both threads: the ring binds it on whichever thread holds its
    * token, so nothing is bound here. */
   if (     (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
         && gl2_ctx_no_shared_context(gl))
   {
      gl->flags &= ~GL2_FLAG_SHARED_CONTEXT_USE;
      RARCH_LOG("[GL] No shared context on \"%s\": the core shares this driver's.\n",
            gl->ctx_driver->ident);
   }
   if (!(gl->flags & GL2_FLAG_SHARED_CONTEXT_USE))
   {
      if (!gl2_ctx_no_shared_context(gl))
         return false;
   }
   else
      gl->ctx_driver->bind_hw_render(gl->ctx_data, true);
   retro_atomic_store_release_int(&gl->hw_ring, 1);
   *ctx = gl;
   return true;
}

/* Called on the thread that holds the core's context - the main
 * thread - before the driver is freed. The context is given up here,
 * where it is current; the driver's own bind would make the video
 * thread's context current here instead. Without the hook the context
 * driver's teardown copes with a still-current context, which GLX and
 * WGL define. */
static void gl2_hw_ring_context_free(void *data, void *ctx)
{
   gl2_t *gl = (gl2_t*)data;
   (void)ctx;
   if (!gl)
      return;
   retro_atomic_store_release_int(&gl->hw_ring, 0);
   if (gl->ctx_driver && gl->ctx_driver->release_current)
      gl->ctx_driver->release_current(gl->ctx_data);
}

/* Main thread, the core's context: the FBO for the ring slot, which is
 * this driver's HW-render FBO of the same index. */
static uintptr_t gl2_hw_ring_framebuffer(void *data, unsigned slot)
{
   gl2_t *gl = (gl2_t*)data;
   if (!gl || slot >= gl->textures)
      return 0;
   return gl->hw_render_fbo[slot];
}

/* Main thread, the core's context: place a fence after the core's
 * rendering and flush, so the frame on the other thread can wait it.
 * Without sync objects, finish: correct, slower. */
static bool gl2_hw_ring_capture(void *data, unsigned slot,
      const void *source, unsigned format)
{
   gl2_t *gl = (gl2_t*)data;
   (void)source; (void)format;
   if (!gl || slot >= 3)
      return false;
#ifdef GL2_HW_RING_SYNC
   /* The core's thread: flags is the video thread's word. */
   if (gl->hw_ring_have_sync)
   {
      /* A sync the video thread never took belongs to a frame it
       * dropped; deleting it does not disturb a wait already placed. */
      void *old;
      GL_HW_RING_SYNC_SWAP(&gl->hw_ring_sync[slot],
            (void*)glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0), old);
      glFlush();
      if (old)
         glDeleteSync((GLsync)old);
      return true;
   }
#endif
   glFinish();
   return true;
}

/* Video thread: wait the core's fence for the slot on the server, and
 * read the slot's texture. */
static bool gl2_hw_ring_present_slot(void *data, unsigned slot)
{
   gl2_t *gl = (gl2_t*)data;
   if (!gl || slot >= gl->textures)
      return false;
#ifdef GL2_HW_RING_SYNC
   {
      void *sync;
      GL_HW_RING_SYNC_SWAP(&gl->hw_ring_sync[slot], NULL, sync);
      if (sync)
      {
         glWaitSync((GLsync)sync, 0, GL_TIMEOUT_IGNORED);
         glDeleteSync((GLsync)sync);
      }
   }
#endif
   /* The frame advances tex_index itself before reading; land on the
    * slot after that advance. */
   gl->tex_index = (slot + gl->textures - 1) % gl->textures;
   return true;
}

/* Ring fences: a sync object placed in this thread's context after the
 * frame, waited by the core's thread before it renders into the slot
 * again. */
typedef struct { void *sync; } gl2_ring_fence_t;

/* The ring's fence for this frame, taken ahead of the swap: a driver
 * that queues swaps completes a fence behind one only once the swap is
 * released, at the display's rate. */
static void gl2_hw_ring_drawn(gl2_t *gl)
{
#ifdef GL2_HW_RING_SYNC
   if (     !gl2_core_context_is_mains(gl)
         || !(gl->flags & GL2_FLAG_HAVE_SYNC))
      return;
   if (gl->hw_ring_done_sync)
      glDeleteSync((GLsync)gl->hw_ring_done_sync);
   gl->hw_ring_done_sync = (void*)glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
#else
   (void)gl;
#endif
}

static bool gl2_hw_ring_fence_new(void *data, void **fence)
{
   gl2_ring_fence_t *f;
   (void)data;
   if (!fence || !(f = (gl2_ring_fence_t*)calloc(1, sizeof(*f))))
      return false;
   *fence = f;
   return true;
}

static void gl2_hw_ring_fence_free(void *data, void *fence)
{
   gl2_ring_fence_t *f = (gl2_ring_fence_t*)fence;
   (void)data;
   if (!f)
      return;
#ifdef GL2_HW_RING_SYNC
   if (f->sync)
      glDeleteSync((GLsync)f->sync);
#endif
   free(f);
}

static void gl2_hw_ring_fence_signal(void *data, void *fence)
{
   gl2_t *gl = (gl2_t*)data;
   gl2_ring_fence_t *f = (gl2_ring_fence_t*)fence;
   if (!gl || !f)
      return;
#ifdef GL2_HW_RING_SYNC
   if (gl->flags & GL2_FLAG_HAVE_SYNC)
   {
      if (f->sync)
         glDeleteSync((GLsync)f->sync);
      if (gl->hw_ring_done_sync)
      {
         f->sync               = gl->hw_ring_done_sync;
         gl->hw_ring_done_sync = NULL;
      }
      else
         f->sync = (void*)glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
      glFlush();
      return;
   }
#endif
   glFinish();
}

static bool gl2_hw_ring_fence_wait(void *data, void *fence, unsigned timeout_us)
{
   gl2_ring_fence_t *f = (gl2_ring_fence_t*)fence;
   (void)data;
   if (!f)
      return true;
#ifdef GL2_HW_RING_SYNC
   if (f->sync)
   {
      GLenum r = glClientWaitSync((GLsync)f->sync, GL_SYNC_FLUSH_COMMANDS_BIT,
            timeout_us == HW_RING_WAIT_FOREVER
               ? GL_TIMEOUT_IGNORED : (GLuint64)timeout_us * 1000);
      if (r == GL_TIMEOUT_EXPIRED)
         return false;
      /* Not deleted here: the video thread made it and may still be
       * in the swap that follows it. It goes when the cell is
       * signalled again */
   }
#else
   (void)timeout_us;
#endif
   return true;
}

static uintptr_t gl2_get_current_framebuffer(void *data)
{
   gl2_t *gl = (gl2_t*)data;
   if (!gl || (!(gl->flags & GL2_FLAG_HAVE_FBO)))
      return 0;
   return gl->hw_render_fbo[(gl->tex_index + 1) % gl->textures];
}

static void gl2_set_rotation(void *data, unsigned rotation)
{
   gl2_t               *gl = (gl2_t*)data;

   if (!gl)
      return;

   gl->rotation = 90 * rotation;
   gl2_set_projection(gl, &default_ortho, true);
}

static void gl2_set_video_mode(void *data, unsigned dims,
      bool fullscreen)
{
   gl2_t               *gl = (gl2_t*)data;
   if (     !gl->ctx_driver->set_video_mode
         || !gl->ctx_driver->set_video_mode(gl->ctx_data,
            dims, fullscreen))
      video_driver_modify_disp_flags(VIDEO_FLAG_DRIVER_REINIT, 0);
}

static void gl2_update_input_size(gl2_t *gl, unsigned width,
      unsigned height, unsigned pitch, bool clear)
{
   float xamt, yamt;

   if ((width != gl->last_width[gl->tex_index] ||
            height != gl->last_height[gl->tex_index]) && gl->empty_buf)
   {
      /* Resolution change.
       *
       * Both rectangles are clamped to the texture before anything is
       * derived from them: a core is free to hand over a frame wider
       * or taller than the geometry it declared, nothing above tex_w
       * or tex_h ever reached the texture, and such a width must not
       * end up describing a read out of empty_buf. */
      unsigned old_w                 = MIN(gl->last_width[gl->tex_index],
            VIDEO_SCALE_W(gl->tex_dims));
      unsigned old_h                 = MIN(gl->last_height[gl->tex_index],
            VIDEO_SCALE_H(gl->tex_dims));
      unsigned new_w                 = MIN(width,  VIDEO_SCALE_W(gl->tex_dims));
      unsigned new_h                 = MIN(height, VIDEO_SCALE_H(gl->tex_dims));

      gl->last_width[gl->tex_index]  = width;
      gl->last_height[gl->tex_index] = height;

      /* Whatever sits outside the rectangle the core last wrote is
       * already blank - the textures come up that way and every
       * shrink blanks them again - so only a shrink can leave pixels
       * of the old frame close enough for the edge filtering and the
       * clamp to reach them. A larger frame covers the difference in
       * the copy that follows this call. */
      if (clear && (new_w < old_w || new_h < old_h))
      {
#if defined(HAVE_PSGL)
         glPixelStorei(GL_UNPACK_ALIGNMENT,
               gl2_get_alignment(VIDEO_SCALE_W(gl->tex_dims) * gl->base_size));
         glBufferSubData(GL_TEXTURE_REFERENCE_BUFFER_SCE,
               VIDEO_SCALE_AREA(gl->tex_dims) * gl->tex_index * gl->base_size,
               VIDEO_SCALE_AREA(gl->tex_dims) * gl->base_size,
               gl->empty_buf);
#else
         /* Only the part of the old rectangle the new one stops
          * covering can be holding a stale pixel, so a strip down the
          * right and a strip along the bottom leave the same texture
          * behind as blanking all of it, for a fraction of the bytes.
          * The rows are packed tight and the alignment describes the
          * strip, so each upload reads exactly as much of empty_buf
          * as its own width and height ask for and no row of it is
          * ever wider than tex_w. */
         if (gl->flags & GL2_FLAG_HAVE_UNPACK_ROW_LENGTH)
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

         if (new_w < old_w)
         {
            glPixelStorei(GL_UNPACK_ALIGNMENT,
                  gl2_get_alignment((old_w - new_w) * gl->base_size));
            glTexSubImage2D(GL_TEXTURE_2D, 0,
                  new_w, 0, old_w - new_w, old_h,
                  gl->texture_type, gl->texture_fmt, gl->empty_buf);
         }

         if (new_h < old_h)
         {
            unsigned span = MIN(new_w, old_w);
            if (span > 0)
            {
               glPixelStorei(GL_UNPACK_ALIGNMENT,
                     gl2_get_alignment(span * gl->base_size));
               glTexSubImage2D(GL_TEXTURE_2D, 0,
                     0, new_h, span, old_h - new_h,
                     gl->texture_type, gl->texture_fmt, gl->empty_buf);
            }
         }
#endif
      }
   }
   /* We might have used different texture coordinates
    * last frame. Edge case if resolution changes very rapidly. */
   else if ((width !=
            gl->last_width[(gl->tex_index + gl->textures - 1) % gl->textures]) ||
         (height !=
          gl->last_height[(gl->tex_index + gl->textures - 1) % gl->textures])) { }
   else
      return;

   xamt = (float)width  / VIDEO_SCALE_W(gl->tex_dims);
   yamt = (float)height / VIDEO_SCALE_H(gl->tex_dims);
   SET_TEXTURE_COORDS(gl->tex_info.coord, xamt, yamt);
}

static void gl2_init_textures_data(gl2_t *gl)
{
   size_t i;
   for (i = 0; i < gl->textures; i++)
   {
      /* Nothing has been written into these yet - gl2_init_textures
       * has just blanked them - so the first frame to land in each
       * one grows into empty space and needs no clear of its own. */
      gl->last_width[i]  = 0;
      gl->last_height[i] = 0;
   }

   for (i = 0; i < gl->textures; i++)
   {
      gl->prev_info[i].tex           = gl->texture[0];
      gl->prev_info[i].input_size[0] = VIDEO_SCALE_W(gl->tex_dims);
      gl->prev_info[i].tex_size[0]   = VIDEO_SCALE_W(gl->tex_dims);
      gl->prev_info[i].input_size[1] = VIDEO_SCALE_H(gl->tex_dims);
      gl->prev_info[i].tex_size[1]   = VIDEO_SCALE_H(gl->tex_dims);
      memcpy(gl->prev_info[i].coord, tex_coords, sizeof(tex_coords));
   }
}

static void gl2_init_textures(gl2_t *gl)
{
   unsigned i;
   GLenum internal_fmt = gl->internal_fmt;
   GLenum texture_type = gl->texture_type;
   GLenum texture_fmt  = gl->texture_fmt;

#ifdef HAVE_PSGL
   if (!gl->pbo)
      glGenBuffers(1, &gl->pbo);

   glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, gl->pbo);
   glBufferData(GL_TEXTURE_REFERENCE_BUFFER_SCE,
         VIDEO_SCALE_AREA(gl->tex_dims) * gl->base_size * gl->textures,
         NULL, GL_STREAM_DRAW);
#endif

#if defined(HAVE_OPENGLES) && !defined(HAVE_PSGL)
   /* GLES is picky about which format we use here.
    * Without extensions, we can *only* render to 16-bit FBOs. */

   if (     (gl->flags & GL2_FLAG_HW_RENDER_USE)
         && (gl->base_size == sizeof(uint32_t)))
   {
      if (gl_check_capability(GL_CAPS_ARGB8))
      {
         internal_fmt = GL_RGBA;
         texture_type = GL_RGBA;
         texture_fmt  = GL_UNSIGNED_BYTE;
      }
      else
      {
         RARCH_WARN("[GL] 32-bit FBO not supported. Falling back to 16-bit.\n");
         internal_fmt = GL_RGB;
         texture_type = GL_RGB;
         texture_fmt  = GL_UNSIGNED_SHORT_5_6_5;
      }
   }
#endif

   glGenTextures(gl->textures, gl->texture);

   for (i = 0; i < gl->textures; i++)
   {
      GL2_BIND_TEXTURE(gl->texture[i], gl->wrap_mode, gl->tex_mag_filter,
            gl->tex_min_filter);

      gl2_renderchain_init_texture_reference(
            gl, (gl2_renderchain_data_t*)gl->renderchain_data,
            i, internal_fmt,
            texture_fmt, texture_type);
   }

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
}

static INLINE void gl2_set_shader_viewports(gl2_t *gl, bool video_scale_integer)
{
   int i;
   unsigned width                = VIDEO_SCALE_W(gl->video_dims);
   unsigned height               = VIDEO_SCALE_H(gl->video_dims);

   for (i = 0; i < 2; i++)
   {
      gl->shader->use(gl, gl->shader_data, i, true);
      gl2_set_viewport(gl, VIDEO_SCALE_PACK(width, height), false, true);
   }
}

static void gl2_set_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned dims,
      float alpha)
{
   enum texture_filter_type menu_filter;
   unsigned base_size              = rgb32 ? sizeof(uint32_t) : sizeof(uint16_t);
   gl2_t *gl                       = (gl2_t*)data;
   if (!gl)
      return;

   /* What the last frame carried, not what the setting says now: the
    * video thread applies this in thread_update_driver_state(). */
   menu_filter                     = (gl->flags & GL2_FLAG_MENU_LINEAR_FILTER)
      ? TEXTURE_FILTER_LINEAR
      : TEXTURE_FILTER_NEAREST;

   if (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, false);

   if (!gl->menu_texture)
      glGenTextures(1, &gl->menu_texture);

   gl_load_texture_data(gl->menu_texture,
         RARCH_WRAP_EDGE, menu_filter,
         gl2_get_alignment(VIDEO_SCALE_W(dims) * base_size),
         VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims), frame,
         base_size, false);

   gl->menu_texture_alpha = alpha;
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

   if (     (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
         && !gl2_core_context_is_mains(gl))
      gl->ctx_driver->bind_hw_render(gl->ctx_data, true);
}

static void gl2_set_texture_enable(void *data, bool state, bool full_screen)
{
   gl2_t *gl                    = (gl2_t*)data;

   if (!gl)
      return;

   if (state)
      gl->flags                |=  GL2_FLAG_MENU_TEXTURE_ENABLE;
   else
      gl->flags                &= ~GL2_FLAG_MENU_TEXTURE_ENABLE;
   if (full_screen)
      gl->flags                |=  GL2_FLAG_MENU_TEXTURE_FULLSCREEN;
   else
      gl->flags                &= ~GL2_FLAG_MENU_TEXTURE_FULLSCREEN;
}

static void gl2_show_mouse(void *data, bool state)
{
   gl2_t                            *gl = (gl2_t*)data;

   if (gl && gl->ctx_driver->show_mouse)
      gl->ctx_driver->show_mouse(gl->ctx_data, state);
}

static struct video_shader *gl2_get_current_shader(void *data)
{
   gl2_t                            *gl = (gl2_t*)data;

   if (!gl)
      return NULL;

   return gl->shader->get_current_shader(gl->shader_data);
}

#if defined(HAVE_MENU)
static INLINE void gl2_draw_texture(gl2_t *gl)
{
   GLfloat color[16];
   unsigned width         = VIDEO_SCALE_W(gl->video_dims);
   unsigned height        = VIDEO_SCALE_H(gl->video_dims);

   color[ 0]              = 1.0f;
   color[ 1]              = 1.0f;
   color[ 2]              = 1.0f;
   color[ 3]              = gl->menu_texture_alpha;
   color[ 4]              = 1.0f;
   color[ 5]              = 1.0f;
   color[ 6]              = 1.0f;
   color[ 7]              = gl->menu_texture_alpha;
   color[ 8]              = 1.0f;
   color[ 9]              = 1.0f;
   color[10]              = 1.0f;
   color[11]              = gl->menu_texture_alpha;
   color[12]              = 1.0f;
   color[13]              = 1.0f;
   color[14]              = 1.0f;
   color[15]              = gl->menu_texture_alpha;

   gl->coords.vertex      = vertexes_flipped;
   gl->coords.tex_coord   = tex_coords;
   gl->coords.color       = color;

   glBindTexture(GL_TEXTURE_2D, gl->menu_texture);

   gl->shader->use(gl,
         gl->shader_data, VIDEO_SHADER_STOCK_BLEND, true);

   gl->coords.vertices    = 4;

   gl->shader->set_coords(gl->shader_data, &gl->coords);
   gl->shader->set_mvp(gl->shader_data, &gl->mvp_no_rot);

   glEnable(GL_BLEND);
   if (gl->views.ui_pass)
      gl2_blend_alpha(gl);

   if (gl->flags & GL2_FLAG_MENU_TEXTURE_FULLSCREEN)
   {
      glViewport(0, 0, width, height);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      glViewport(VIDEO_POS_X(gl->vp.pos), VIDEO_POS_Y(gl->vp.pos), VIDEO_SCALE_W(gl->vp.dims), VIDEO_SCALE_H(gl->vp.dims));
   }
   else if (gl->views.active)
   {
      /* gl->vp is the whole window while views draw, so fit the menu
       * to this pass's target here. */
      struct video_viewport vp;
      vp.full_dims = gl->video_dims;
      video_driver_update_viewport(&vp, false,
            (gl->flags & GL2_FLAG_KEEP_ASPECT) ? true : false, false);
      glViewport(VIDEO_POS_X(vp.pos), VIDEO_POS_Y(vp.pos),
            VIDEO_SCALE_W(vp.dims), VIDEO_SCALE_H(vp.dims));
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      glViewport(VIDEO_POS_X(gl->vp.pos), VIDEO_POS_Y(gl->vp.pos), VIDEO_SCALE_W(gl->vp.dims), VIDEO_SCALE_H(gl->vp.dims));
   }
   else
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   glDisable(GL_BLEND);

   gl->coords.vertex      = gl->vertex_ptr;
   gl->coords.tex_coord   = gl->tex_info.coord;
   gl->coords.color       = gl->white_color_ptr;
}
#endif

static void gl2_pbo_async_readback(gl2_t *gl);

/* The recording's per-frame async readback, or its teardown once
 * recording has stopped. skip_in_menu: don't read back while the menu
 * is drawn. */
static void gl2_record_readback(gl2_t *gl, bool skip_in_menu)
{
   if (!(gl->flags & GL2_FLAG_PBO_READBACK_ENABLE))
      return;

   if (!(gl->flags & GL2_FLAG_GPU_RECORDING))
   {
      gl2_free_pbo_readback(gl);
      gl->flags &= ~GL2_FLAG_PBO_READBACK_ENABLE;
   }
#ifdef HAVE_MENU
   else if (skip_in_menu && (gl->flags & GL2_FLAG_MENU_TEXTURE_ENABLE))
      ;
#endif
   else
      gl2_pbo_async_readback(gl);
}

static void gl2_pbo_async_readback(gl2_t *gl)
{
#ifdef HAVE_OPENGLES
   GLenum fmt  = GL_RGBA;
   GLenum type = GL_UNSIGNED_BYTE;
#else
   GLenum fmt  = GL_BGRA;
   GLenum type = GL_UNSIGNED_INT_8_8_8_8_REV;
#endif

#if !defined(HAVE_OPENGLES) && !defined(HAVE_PSGL)
   /* Resize the destination before issuing a copy at the new extent. */
   if ((unsigned)gl->pbo_readback_scaler.in_width != VIDEO_SCALE_W(gl->vp.dims)
         || (unsigned)gl->pbo_readback_scaler.in_height != VIDEO_SCALE_H(gl->vp.dims))
   {
      gl2_free_pbo_readback(gl);
      if (!gl2_init_pbo_readback(gl))
      {
         gl->flags &= ~GL2_FLAG_PBO_READBACK_ENABLE;
         return;
      }
   }
#endif

   gl2_renderchain_bind_pbo(
         gl->pbo_readback[gl->pbo_readback_index]);
   gl->pbo_readback_valid[gl->pbo_readback_index] = true;

   gl2_renderchain_readback(gl, gl->renderchain_data,
         gl2_get_alignment(VIDEO_SCALE_W(gl->vp.dims) * sizeof(uint32_t)),
         fmt, type, NULL);
   gl2_renderchain_unbind_pbo();

#ifdef GL2_HW_RING_SYNC
   if (gl->flags & GL2_FLAG_HAVE_SYNC)
   {
      if (gl->pbo_readback_sync[gl->pbo_readback_index])
         glDeleteSync((GLsync)gl->pbo_readback_sync[gl->pbo_readback_index]);
      gl->pbo_readback_sync[gl->pbo_readback_index] =
            (void*)glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
   }
#endif
   gl->pbo_readback_index = (gl->pbo_readback_index + 1) & 3;
}

/* GLSL 1.20 scRGB encode: mirror of the glcore / hdr_sm5 SDR branch
 * (gamma 2.4 linearize, expand-gamut matrix select, gamut round-trip,
 * paper-white / 80 scale). The matrices are the shared constants
 * transposed for GLSL column-major construction, identical to the
 * (column-by-column verified) glcore versions. */
/* Sources are string arrays fed to glShaderSource as multiple
 * segments: C90 only guarantees 509 characters per literal, and the
 * fragment shader exceeds that as one string
 * (-Werror=overlength-strings on the strict CI targets). */
static const char *gl2_scrgb_vert_src[] = {
   "attribute vec2 Pos;\n"
   "attribute vec2 Tex;\n"
   "varying vec2 vTex;\n"
   "void main()\n"
   "{\n"
   "   gl_Position = vec4(Pos * 2.0 - 1.0, 0.0, 1.0);\n"
   "   vTex = Tex;\n"
   "}\n"
};

static const char *gl2_scrgb_frag_src[] = {
   "uniform sampler2D uTex;\n"
   "uniform sampler2D uUITex;\n"
   "uniform float uNits;\n"
   "uniform float uExpand;\n"
   /* 0 = SDR -> scRGB, 1 = PQ -> scRGB, 2 = PQ -> SDR fallback. */
   "uniform float uMode;\n"
   /* <= 0 disables the separate UI composite. */
   "uniform float uUINits;\n"
   /* > 0.5: HDR10 output (Rec.2020 PQ, a 10-bit scanout) rather than
    * scRGB. Unset, it reads 0 and the output stays scRGB. */
   "uniform float uOutPQ;\n"
   "varying vec2 vTex;\n"
   "const mat3 k709to2020 = mat3(\n"
   "   0.6274040, 0.0690970, 0.0163916,\n"
   "   0.3292820, 0.9195400, 0.0880132,\n"
   "   0.0433136, 0.0113612, 0.8955950);\n"
   "const mat3 kExpanded709to2020 = mat3(\n"
   "   0.6274040, 0.0457456, -0.00121055,\n"
   "   0.3292820, 0.9417770,  0.0176041,\n"
   "   0.0433136, 0.0124772,  0.9836070);\n",
   "const mat3 kP3to2020 = mat3(\n"
   "   0.753833,  0.045744, -0.001210,\n"
   "   0.198597,  0.941777,  0.017602,\n"
   "   0.047570,  0.012479,  0.983609);\n"
   "const mat3 k2020to709 = mat3(\n"
   "    1.6604910, -0.1245505, -0.0181508,\n"
   "   -0.5876411,  1.1328999, -0.1005789,\n"
   "   -0.0728499, -0.0083494,  1.1187297);\n",
   /* ST.2084 (PQ) -> normalized linear. */
   "vec3 pqToLinear(vec3 e)\n"
   "{\n"
   "   vec3 p = pow(abs(e), vec3(1.0 / 78.84375));\n"
   "   vec3 n = max(p - 0.8359375, vec3(0.0));\n"
   "   vec3 d = 18.8515625 - 18.6875 * p;\n"
   "   return pow(abs(n / d), vec3(1.0 / 0.1593017578));\n"
   "}\n",
   /* Normalized linear (1.0 = 10,000 nits) -> ST.2084 (PQ). */
   "vec3 linearToPq(vec3 l)\n"
   "{\n"
   "   vec3 p = pow(max(l, vec3(0.0)), vec3(0.1593017578125));\n"
   "   return pow((0.8359375 + 18.8515625 * p) / (1.0 + 18.6875 * p),\n"
   "         vec3(78.84375));\n"
   "}\n",
   /* This is the old main() body verbatim: SDR gamma 2.4 -> linear
    * scRGB at the given paper white. */
   "vec3 sdrToScrgb(vec3 c, float nits)\n"
   "{\n"
   "   vec3 lin = pow(abs(c), vec3(2.4));\n"
   "   if (uExpand < 0.5)\n"
   "      lin = k709to2020 * lin;\n"
   "   else if (uExpand < 1.5)\n"
   "      lin = kExpanded709to2020 * lin;\n"
   "   else if (uExpand < 2.5)\n"
   "      lin = kP3to2020 * lin;\n"
   "   lin = max(lin, vec3(0.0));\n"
   "   lin = k2020to709 * lin;\n"
   "   return lin * (nits / 80.0);\n"
   "}\n",
   /* All rotations below are matrix * vector: this file stores its
    * matrices column-major for that order. hdr_common.glsl stores the
    * SAME matrices row-major for vector * matrix - the idioms cannot
    * be copied between the files without transposing the result,
    * which reads as a strong red cast (whites scale 1.52/0.44/1.04). */
   "void main()\n"
   "{\n"
   "   vec4 src = texture2D(uTex, vTex);\n"
   "   vec3 lin;\n"
   "   if (uMode > 1.5)\n"
   "   {\n"
   /*    PQ content on an SDR output: decode to nits, normalize to
    *    paper white, roll the overshoot off, re-encode to gamma. */
   "      vec3 nits = pqToLinear(src.rgb) * 10000.0;\n"
   "      vec3 sdr  = (k2020to709 * nits) / max(uNits, 1.0);\n"
   "      float pk  = max(sdr.r, max(sdr.g, sdr.b));\n"
   "      if (pk > 1.0)\n"
   "         sdr /= pk;\n"
   "      gl_FragColor = vec4(pow(max(sdr, vec3(0.0)), vec3(1.0 / 2.4)), src.a);\n"
   "      return;\n"
   "   }\n",
   "   if (uMode > 0.5)\n"
   /*    HDR10 PQ Rec.2020 at absolute luminance; 10000/80 = 125. */
   "      lin = (k2020to709 * pqToLinear(src.rgb)) * 125.0;\n"
   "   else\n"
   "      lin = sdrToScrgb(src.rgb, uNits);\n"
   /* SDR UI over PQ content, in linear light at its own brightness.
    * The layer accumulated over a transparent clear with src-alpha
    * blending, so it is premultiplied: un-premultiply before the
    * transfer function, re-apply after. */
   "   if (uUINits > 0.0)\n"
   "   {\n"
   "      vec4 ui = texture2D(uUITex, vTex);\n"
   "      if (ui.a > 0.0)\n"
   "      {\n"
   "         vec3 uil = sdrToScrgb(ui.rgb / ui.a, uUINits) * ui.a;\n"
   "         lin      = uil + lin * (1.0 - ui.a);\n"
   "      }\n"
   "   }\n"
   /* HDR10 output: the composited scRGB rotated to Rec.2020 and
    * PQ-encoded at absolute luminance; scanout is opaque. */
   "   if (uOutPQ > 0.5)\n"
   "   {\n"
   "      gl_FragColor = vec4(linearToPq(k709to2020 * lin * (80.0 / 10000.0)), 1.0);\n"
   "      return;\n"
   "   }\n"
   "   gl_FragColor = vec4(lin, src.a);\n"
   "}\n"
};

static GLuint gl2_scrgb_compile_stage(GLenum stage,
      const char **src, GLsizei count)
{
   GLint status = 0;
   GLuint sh;
   if (!(sh = glCreateShader(stage)))
      return 0;
   glShaderSource(sh, count, src, NULL);
   glCompileShader(sh);
   glGetShaderiv(sh, GL_COMPILE_STATUS, &status);
   if (!status)
   {
      glDeleteShader(sh);
      return 0;
   }
   return sh;
}

static bool gl2_scrgb_init_program(gl2_t *gl)
{
   GLint status = 0;
   GLuint vs, fs, prog;

   /* On a GL 1.x context the GLSL entry points do not resolve;
    * without them the encode cannot exist and stage-1 dim output is
    * the (documented) best available behavior. The check only makes
    * sense where glsym remaps the names to loadable pointers (the
    * remap is a macro, hence the defined() test); where they are
    * directly linked functions the address is never null and clang
    * (Orbis) rejects the comparison outright. */
#if defined(glCreateShader) && defined(glCreateProgram)
   if (!glCreateShader || !glCreateProgram)
      return false;
#endif

   if (!(vs = gl2_scrgb_compile_stage(GL_VERTEX_SHADER,
               gl2_scrgb_vert_src,
               (GLsizei)ARRAY_SIZE(gl2_scrgb_vert_src))))
      return false;
   if (!(fs = gl2_scrgb_compile_stage(GL_FRAGMENT_SHADER,
               gl2_scrgb_frag_src,
               (GLsizei)ARRAY_SIZE(gl2_scrgb_frag_src))))
   {
      glDeleteShader(vs);
      return false;
   }
   prog = glCreateProgram();
   glAttachShader(prog, vs);
   glAttachShader(prog, fs);
   glBindAttribLocation(prog, 0, "Pos");
   glBindAttribLocation(prog, 1, "Tex");
   glLinkProgram(prog);
   glDeleteShader(vs);
   glDeleteShader(fs);
   glGetProgramiv(prog, GL_LINK_STATUS, &status);
   if (!status)
   {
      glDeleteProgram(prog);
      return false;
   }
   gl->scrgb.program     = prog;
   gl->scrgb.loc_tex     = glGetUniformLocation(prog, "uTex");
   gl->scrgb.loc_nits    = glGetUniformLocation(prog, "uNits");
   gl->scrgb.loc_expand  = glGetUniformLocation(prog, "uExpand");
   gl->scrgb.loc_ui_tex  = glGetUniformLocation(prog, "uUITex");
   gl->scrgb.loc_mode    = glGetUniformLocation(prog, "uMode");
   gl->scrgb.loc_ui_nits = glGetUniformLocation(prog, "uUINits");
   gl->scrgb.loc_out_pq  = glGetUniformLocation(prog, "uOutPQ");
   return true;
}

#if !defined(HAVE_OPENGLES) && !defined(HAVE_PSGL)
/* An RGBA16F texture is linear scRGB: 1.0 is 80 nits, the 709
 * primaries. Drawn into the UI's layer, which the encode (sdrToScrgb
 * above) shows as k2020to709 * M * v^2.4 at the menu nits, M the
 * expand-gamut matrix. This writes the exact inverse, so the encode
 * shows the texture at the luminance it was graded at; above menu
 * white v is above 1.0, which the RGBA16F layer keeps. The inverses
 * are of the encode's own matrices, column-major likewise. */
static const char *gl2_linear_vert_src[] = {
   "attribute vec2 Pos;\n"
   "attribute vec2 Tex;\n"
   "attribute vec4 Col;\n"
   "uniform mat4 uMVP;\n"
   "varying vec2 vTex;\n"
   "varying vec4 vCol;\n"
   "void main()\n"
   "{\n"
   "   gl_Position = uMVP * vec4(Pos, 0.0, 1.0);\n"
   "   vTex = Tex;\n"
   "   vCol = Col;\n"
   "}\n"
};

static const char *gl2_linear_frag_src[] = {
   "uniform sampler2D uTex;\n"
   /* x: menu nits, y: the encode's uExpand */
   "uniform vec2 uParams;\n"
   "varying vec2 vTex;\n"
   "varying vec4 vCol;\n"
   "const mat3 k709to2020 = mat3(\n"
   "   0.6274040, 0.0690970, 0.0163916,\n"
   "   0.3292820, 0.9195400, 0.0880132,\n"
   "   0.0433136, 0.0113612, 0.8955950);\n"
   "const mat3 kInvExpanded709to2020 = mat3(\n"
   "    1.6353460, -0.0794803,  0.0034352,\n"
   "   -0.5705700,  1.0898049, -0.0202070,\n"
   "   -0.0647755, -0.0103244,  1.0167713);\n",
   "const mat3 kInvP3to2020 = mat3(\n"
   "    1.3435784, -0.0652977,  0.0028213,\n"
   "   -0.2821792,  1.0757882, -0.0195987,\n"
   "   -0.0613991, -0.0104905,  1.0167763);\n"
   "void main()\n"
   "{\n"
   "   vec4 s = texture2D(uTex, vTex);\n"
   "   vec3 t = vCol.rgb * s.rgb * (80.0 / max(uParams.x, 1.0));\n"
   "   vec3 x;\n"
   "   if (uParams.y < 0.5)\n"
   "      x = t;\n"
   "   else if (uParams.y < 1.5)\n"
   "      x = kInvExpanded709to2020 * (k709to2020 * t);\n"
   "   else if (uParams.y < 2.5)\n"
   "      x = kInvP3to2020 * (k709to2020 * t);\n"
   "   else\n"
   "      x = k709to2020 * t;\n"
   "   gl_FragColor = vec4(pow(max(x, vec3(0.0)), vec3(1.0 / 2.4)),\n"
   "         vCol.a * s.a);\n"
   "}\n"
};

/* Made the first time a half-float texture is drawn; 0 where it
 * cannot be, and the texture is drawn as any other */
static GLuint gl2_linear_program(gl2_t *gl)
{
   GLint status = 0;
   GLuint vs, fs, prog;

   if (gl->scrgb.lin_program || gl->scrgb.lin_tried)
      return gl->scrgb.lin_program;
   gl->scrgb.lin_tried = true;

   if (!(vs = gl2_scrgb_compile_stage(GL_VERTEX_SHADER,
               gl2_linear_vert_src,
               (GLsizei)ARRAY_SIZE(gl2_linear_vert_src))))
      return 0;
   if (!(fs = gl2_scrgb_compile_stage(GL_FRAGMENT_SHADER,
               gl2_linear_frag_src,
               (GLsizei)ARRAY_SIZE(gl2_linear_frag_src))))
   {
      glDeleteShader(vs);
      return 0;
   }
   prog = glCreateProgram();
   glAttachShader(prog, vs);
   glAttachShader(prog, fs);
   glBindAttribLocation(prog, 0, "Pos");
   glBindAttribLocation(prog, 1, "Tex");
   glBindAttribLocation(prog, 2, "Col");
   glLinkProgram(prog);
   glDeleteShader(vs);
   glDeleteShader(fs);
   glGetProgramiv(prog, GL_LINK_STATUS, &status);
   if (!status)
   {
      glDeleteProgram(prog);
      return 0;
   }
   gl->scrgb.lin_program    = prog;
   gl->scrgb.lin_loc_mvp    = glGetUniformLocation(prog, "uMVP");
   gl->scrgb.lin_loc_tex    = glGetUniformLocation(prog, "uTex");
   gl->scrgb.lin_loc_params = glGetUniformLocation(prog, "uParams");
   return prog;
}

/* Whether an RGBA16F texture can be the UI layer's target, tried once
 * at init on the context that will draw it */
static bool gl2_fp16_target_ok(void)
{
   GLuint tex = 0, fbo = 0;
   bool   ok  = false;
   glGenTextures(1, &tex);
   glBindTexture(GL_TEXTURE_2D, tex);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 4, 4, 0,
         GL_RGBA, GL_HALF_FLOAT, NULL);
   gl2_gen_fb(1, &fbo);
   gl2_bind_fb(fbo);
   gl2_fb_texture_2d(RARCH_GL_FRAMEBUFFER, RARCH_GL_COLOR_ATTACHMENT0,
         GL_TEXTURE_2D, tex, 0);
   ok = gl2_check_fb_status(RARCH_GL_FRAMEBUFFER)
      == RARCH_GL_FRAMEBUFFER_COMPLETE;
   gl2_bind_fb(0);
   gl2_delete_fb(1, &fbo);
   glBindTexture(GL_TEXTURE_2D, 0);
   glDeleteTextures(1, &tex);
   return ok;
}
#endif

/* True when the frame must route through the offscreen + encode even
 * with no scRGB backbuffer: the core supplied PQ, which cannot go to an
 * SDR presentation path as-is. The frontend accepts HDR10 by configured
 * driver ident (it cannot know this early whether the context will be
 * scRGB), and the driver reconciles here - same contract as glcore. */
static bool gl2_needs_pq_downconvert(gl2_t *gl)
{
   return gl->video_info.source_hdr10 && !gl->scrgb.active;
}

/* (Re)create the offscreen; returns the FBO the frame's final targets
 * should bind: the offscreen under scRGB or PQ content, 0 otherwise. */
static GLuint gl2_frame_target_fbo(gl2_t *gl)
{
   /* The layer the UI draws into - the frame's for SDR content, its
    * own for PQ - is RGBA16F once a linear texture has been loaded, so
    * it keeps what is above menu white; but not while recording reads
    * the layer back each frame, which RGBA8 hands over without
    * converting */
   bool want_fp16 = false;
#if !defined(HAVE_OPENGLES) && !defined(HAVE_PSGL)
   want_fp16      =    gl->scrgb.fp16_ok
                    && gl->scrgb.active
                    && gl2_fp16_loaded
                    && !(gl->flags & GL2_FLAG_PBO_READBACK_ENABLE);
#endif

   if (!gl->scrgb.active && !gl2_needs_pq_downconvert(gl))
      return 0;

   if (     !gl->scrgb.fbo
         || gl->scrgb.dims != gl->video_dims
         || gl->scrgb.fp16_made != want_fp16)
   {
      if (gl->scrgb.fbo)
         gl2_delete_fb(1, &gl->scrgb.fbo);
      if (gl->scrgb.tex)
         glDeleteTextures(1, &gl->scrgb.tex);
      glGenTextures(1, &gl->scrgb.tex);
      glBindTexture(GL_TEXTURE_2D, gl->scrgb.tex);
      /* PQ content needs 10 bits here for the same reason the source
       * textures do: this holds the frame between the chain's final
       * pass and the encode, and 8-bit PQ bands in the darks. */
      gl->scrgb.tex_fp16 = false;
#if !defined(HAVE_OPENGLES) && !defined(HAVE_PSGL)
      if (gl->video_info.source_hdr10)
         glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB10_A2,
               VIDEO_SCALE_W(gl->video_dims), VIDEO_SCALE_H(gl->video_dims), 0,
               GL_BGRA, GL_UNSIGNED_INT_2_10_10_10_REV, NULL);
      else if ((gl->scrgb.tex_fp16 = want_fp16))
         glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
               VIDEO_SCALE_W(gl->video_dims), VIDEO_SCALE_H(gl->video_dims), 0,
               GL_RGBA, GL_HALF_FLOAT, NULL);
      else
#endif
         glTexImage2D(GL_TEXTURE_2D, 0, RARCH_GL_INTERNAL_FORMAT32,
               VIDEO_SCALE_W(gl->video_dims), VIDEO_SCALE_H(gl->video_dims), 0,
               RARCH_GL_TEXTURE_TYPE32, RARCH_GL_FORMAT32, NULL);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      gl2_gen_fb(1, &gl->scrgb.fbo);
      gl2_bind_fb(gl->scrgb.fbo);
      gl2_fb_texture_2d(RARCH_GL_FRAMEBUFFER, RARCH_GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, gl->scrgb.tex, 0);
      if (gl2_check_fb_status(RARCH_GL_FRAMEBUFFER)
            != RARCH_GL_FRAMEBUFFER_COMPLETE)
      {
         RARCH_ERR("[GL] scRGB offscreen FBO incomplete; falling back to direct rendering.\n");
         gl2_bind_fb(0);
         gl2_delete_fb(1, &gl->scrgb.fbo);
         glDeleteTextures(1, &gl->scrgb.tex);
         gl->scrgb.fbo    = 0;
         gl->scrgb.tex    = 0;
         gl->scrgb.active = false;
         return 0;
      }
      gl2_bind_fb(0);
      gl->scrgb.dims      = gl->video_dims;
      gl->scrgb.fp16_made = want_fp16;

      /* UI layer, sized and lifetimed with the content offscreen.
       * Only the scRGB PQ composite needs it; the downconvert path
       * draws the UI straight over the converted backbuffer. */
      if (gl->scrgb.ui_fbo)
      {
         gl2_delete_fb(1, &gl->scrgb.ui_fbo);
         gl->scrgb.ui_fbo = 0;
      }
      if (gl->scrgb.ui_tex)
      {
         glDeleteTextures(1, &gl->scrgb.ui_tex);
         gl->scrgb.ui_tex = 0;
      }
      if (gl->video_info.source_hdr10 && gl->scrgb.active)
      {
         glGenTextures(1, &gl->scrgb.ui_tex);
         glBindTexture(GL_TEXTURE_2D, gl->scrgb.ui_tex);
#if !defined(HAVE_OPENGLES) && !defined(HAVE_PSGL)
         if ((gl->scrgb.tex_fp16 = want_fp16))
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
                  VIDEO_SCALE_W(gl->video_dims), VIDEO_SCALE_H(gl->video_dims), 0,
                  GL_RGBA, GL_HALF_FLOAT, NULL);
         else
#endif
         glTexImage2D(GL_TEXTURE_2D, 0, RARCH_GL_INTERNAL_FORMAT32,
               VIDEO_SCALE_W(gl->video_dims), VIDEO_SCALE_H(gl->video_dims), 0,
               RARCH_GL_TEXTURE_TYPE32, RARCH_GL_FORMAT32, NULL);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
         gl2_gen_fb(1, &gl->scrgb.ui_fbo);
         gl2_bind_fb(gl->scrgb.ui_fbo);
         gl2_fb_texture_2d(RARCH_GL_FRAMEBUFFER, RARCH_GL_COLOR_ATTACHMENT0,
               GL_TEXTURE_2D, gl->scrgb.ui_tex, 0);
         if (gl2_check_fb_status(RARCH_GL_FRAMEBUFFER)
               != RARCH_GL_FRAMEBUFFER_COMPLETE)
         {
            RARCH_ERR("[GL] scRGB UI layer FBO incomplete; UI will be composited with the frame.\n");
            gl2_delete_fb(1, &gl->scrgb.ui_fbo);
            glDeleteTextures(1, &gl->scrgb.ui_tex);
            gl->scrgb.ui_fbo   = 0;
            gl->scrgb.ui_tex   = 0;
            gl->scrgb.tex_fp16 = false;
         }
         gl2_bind_fb(0);
      }
   }
   return gl->scrgb.fbo;
}

/* Target for the UI draws. Under scRGB PQ content that is the separate
 * UI layer, cleared to transparent once per frame here; the encode
 * composites it over the decoded content at Menu HDR Brightness. */
static GLuint gl2_ui_target_fbo(gl2_t *gl)
{
   GLuint fbo = gl2_frame_target_fbo(gl);

   if (     gl->video_info.source_hdr10
         && gl->scrgb.active
         && gl->scrgb.ui_fbo)
   {
      gl2_bind_fb(gl->scrgb.ui_fbo);
      glDisable(GL_SCISSOR_TEST);
      glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
      glClear(GL_COLOR_BUFFER_BIT);
      return gl->scrgb.ui_fbo;
   }
   return fbo;
}

/* Tonemap a PQ frame from the offscreen into the backbuffer. Only used
 * when the core supplied HDR10 but this context has no scRGB
 * backbuffer; runs at the pre-UI choke point so the UI then draws over
 * a displayable SDR image in the ordinary way. */
static void gl2_encode_pq_to_sdr(gl2_t *gl)
{
   static const float quad_pos[8] = {
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f
   };
   static const float quad_tex[8] = {
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f
   };
   static bool warned = false;

   if (!gl->scrgb.fbo || !gl->scrgb.program)
      return;

   if (!warned)
   {
      warned = true;
      RARCH_WARN("[GL] Core supplied HDR10 PQ but this context has no scRGB backbuffer; tonemapping to SDR. Turn on HDR output, or set the core to a 24-bit colour format.\n");
   }

   gl2_renderchain_bind_backbuffer();
   glViewport(0, 0,
         VIDEO_SCALE_W(gl->video_dims),
         VIDEO_SCALE_H(gl->video_dims));
   glDisable(GL_BLEND);
   glUseProgram(gl->scrgb.program);
   if (gl->scrgb.loc_tex >= 0)
      glUniform1i(gl->scrgb.loc_tex, 0);
   if (gl->scrgb.loc_ui_tex >= 0)
      glUniform1i(gl->scrgb.loc_ui_tex, 0);
   if (gl->scrgb.loc_nits >= 0)
      /* Driver-owned, latched by the HDR poke path: no live
       * settings on the frame path. */
      glUniform1f(gl->scrgb.loc_nits, gl->scrgb.paper_white_nits);
   if (gl->scrgb.loc_expand >= 0)
      glUniform1f(gl->scrgb.loc_expand, 0.0f);
   if (gl->scrgb.loc_mode >= 0)
      glUniform1f(gl->scrgb.loc_mode, 2.0f);
   if (gl->scrgb.loc_ui_nits >= 0)
      glUniform1f(gl->scrgb.loc_ui_nits, 0.0f);
   /* An SDR output: the program keeps its uniforms between draws */
   if (gl->scrgb.loc_out_pq >= 0)
      glUniform1f(gl->scrgb.loc_out_pq, 0.0f);

   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, gl->scrgb.tex);

   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glEnableVertexAttribArray(0);
   glEnableVertexAttribArray(1);
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, quad_pos);
   glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, quad_tex);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glDisableVertexAttribArray(0);
   glDisableVertexAttribArray(1);
   glUseProgram(0);

   /* Restore the aspect viewport - context state this driver only
    * re-establishes on resize (see the matching restore in the
    * end-of-frame encode). */
   glViewport(VIDEO_POS_X(gl->vp.pos), VIDEO_POS_Y(gl->vp.pos), VIDEO_SCALE_W(gl->vp.dims), VIDEO_SCALE_H(gl->vp.dims));
}

/* Copies the backbuffer into the retained texture, sizing that texture
 * to the window when it does not match. */
static void gl2_retain_backbuffer(gl2_t *gl)
{
   unsigned width  = VIDEO_SCALE_W(gl->video_dims);
   unsigned height = VIDEO_SCALE_H(gl->video_dims);

   if (!width || !height)
      return;

   gl2_renderchain_bind_backbuffer();
   if (!gl->retained_texture)
      glGenTextures(1, &gl->retained_texture);
   glBindTexture(GL_TEXTURE_2D, gl->retained_texture);
   if (gl->retained_dims != VIDEO_SCALE_PACK(width, height))
   {
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, NULL);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      gl->retained_dims   = VIDEO_SCALE_PACK(width, height);
   }
   glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
}

/* Replays the group the retaining frame made: the retained texture
 * drawn over the whole window with the stock shader for each light
 * present, a clear for each dark one, a swap after each. The copy was
 * taken from the backbuffer, so it is already the right way up and is
 * drawn with the unflipped vertices. Returns swaps made. */
static unsigned gl2_present_last(void *data)
{
   unsigned i;
   unsigned done = 0;
   gl2_t *gl     = (gl2_t*)data;

   if (     !gl || !gl->retained_texture
         || gl->retained_dims   != gl->video_dims)
      return 0;

   for (i = 0; i < gl->retained_light; i++)
   {
      gl2_renderchain_bind_backbuffer();
      glViewport(0, 0,
         VIDEO_SCALE_W(gl->video_dims),
         VIDEO_SCALE_H(gl->video_dims));
      glBindTexture(GL_TEXTURE_2D, gl->retained_texture);

      gl->coords.vertex    = vertexes;
      gl->coords.tex_coord = tex_coords;
      gl->coords.color     = gl->white_color_ptr;
      gl->coords.vertices  = 4;

      gl->shader->use(gl, gl->shader_data, VIDEO_SHADER_STOCK_BLEND, true);
      gl->shader->set_coords(gl->shader_data, &gl->coords);
      gl->shader->set_mvp(gl->shader_data, &gl->mvp_no_rot);

      glDisable(GL_BLEND);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

      gl->coords.vertex    = gl->vertex_ptr;
      gl->coords.tex_coord = gl->tex_info.coord;
      glViewport(VIDEO_POS_X(gl->vp.pos), VIDEO_POS_Y(gl->vp.pos), VIDEO_SCALE_W(gl->vp.dims), VIDEO_SCALE_H(gl->vp.dims));
      glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

      if (gl->ctx_driver->swap_buffers)
         gl->ctx_driver->swap_buffers(gl->ctx_data);
      done++;
   }

   for (i = 0; i < gl->retained_dark; i++)
   {
      gl2_renderchain_bind_backbuffer();
      glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT);
      if (gl->ctx_driver->swap_buffers)
         gl->ctx_driver->swap_buffers(gl->ctx_data);
      done++;
   }
   return done;
}

/* The frame's swap: how long it took, and whether it was one that
 * waits for the vertical blank (a swap interval set). With a swap
 * interval the swap returns when the frame is out, so that time is
 * waiting, not drawing, and the frame is shown: what the threaded
 * presenter needs to hold the core to the drawing time alone and to
 * count the frame to the vblank it met. As glcore says it. */
static retro_time_t gl2_present_wait;
static bool gl2_present_vsynced;

static retro_time_t gl2_get_last_present_wait(void *data, bool *shown)
{
   *shown = false;
   if (!data || !gl2_present_vsynced)
      return 0;
   *shown = true;
   return gl2_present_wait;
}

static retro_time_t gl2_get_last_present_time(void *data)
{
   gl2_t *gl = (gl2_t*)data;
   if (gl && gl->ctx_driver && gl->ctx_driver->last_present_time)
      return gl->ctx_driver->last_present_time(gl->ctx_data);
   return 0;
}

#if (defined(HAVE_OPENXR) && defined(ANDROID) && defined(HAVE_OPENGLES3)) \
   || defined(HAVE_GL2_XR)
/* A tracked map's eyes, each its rectangle of the core's frame drawn
 * with the stock shader and no chain into dst's images from their
 * origin, at the left eye's size within dims, shrunk if larger, as
 * Vulkan draws them. Sampled, so any format gl2 draws will do; each
 * image is cleared whole, as the compositor may read past the
 * rectangle. The framebuffer to draw through is bound. */
static void gl2_xr_draw_eye_images(gl2_t *gl,
      const video_frame_info_t *video_info, const uint32_t *dst,
      unsigned dims, unsigned *width, unsigned *height)
{
   unsigned e;
   GLfloat coords[8];
   const video_views_map_t *map = &video_info->views;
   int l                        = video_info->headset_eye[0];
   int r                        = video_info->headset_eye[1];
   float tex_w                  = gl->tex_info.tex_size[0];
   float tex_h                  = gl->tex_info.tex_size[1];
   float frame_h                = gl->tex_info.input_size[1];
   bool bottom_left             = (gl->vertex_ptr == vertexes);
   unsigned w = MIN(map->views[l].width,  VIDEO_SCALE_W(dims));
   unsigned h = MIN(map->views[l].height, VIDEO_SCALE_H(dims));

   glBindTexture(GL_TEXTURE_2D, gl->tex_info.tex);
   glDisable(GL_BLEND);
   glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
   glViewport(0, 0, w, h);

   gl->coords.vertex    = vertexes;
   gl->coords.tex_coord = coords;
   gl->coords.color     = gl->white_color_ptr;
   gl->coords.vertices  = 4;
   gl->shader->use(gl, gl->shader_data, VIDEO_SHADER_STOCK_BLEND, true);
   gl->shader->set_mvp(gl->shader_data, &gl->mvp_no_rot);

   for (e = 0; e < 2; e++)
   {
      const struct retro_video_view *v = &map->views[e ? r : l];
      bool scaled = (v->width != w || v->height != h);
      /* Scaled, the filter stays off the next rectangle's texels. */
      float in    = scaled ? 0.5f : 0.0f;
      float s0    = ((float)v->x + in) / tex_w;
      float s1    = ((float)(v->x + v->width) - in) / tex_w;
      float t0, t1;   /* the rows shown at the image's bottom and top */
      GLint filter = scaled ? GL_LINEAR : GL_NEAREST;

      if (bottom_left)
      {
         t0 = (frame_h - (float)(v->y + v->height) + in) / tex_h;
         t1 = (frame_h - (float)v->y - in) / tex_h;
      }
      else
      {
         t0 = ((float)(v->y + v->height) - in) / tex_h;
         t1 = ((float)v->y + in) / tex_h;
      }
      coords[0] = s0;
      coords[1] = t0;
      coords[2] = s1;
      coords[3] = t0;
      coords[4] = s0;
      coords[5] = t1;
      coords[6] = s1;
      coords[7] = t1;

      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, (GLuint)dst[e], 0);
      glClear(GL_COLOR_BUFFER_BIT);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
      gl->shader->set_coords(gl->shader_data, &gl->coords);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   }

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl->tex_min_filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl->tex_mag_filter);
   gl->coords.vertex    = gl->vertex_ptr;
   gl->coords.tex_coord = gl->tex_info.coord;
   glViewport(VIDEO_POS_X(gl->vp.pos), VIDEO_POS_Y(gl->vp.pos), VIDEO_SCALE_W(gl->vp.dims), VIDEO_SCALE_H(gl->vp.dims));
   *width  = w;
   *height = h;
}
#endif

#if defined(HAVE_OPENXR) && defined(ANDROID) && defined(HAVE_OPENGLES3)
static void gl2_viewport_info(void *data, struct video_viewport *vp);

static void gl2_xr_draw_eyes(gl2_t *gl, const video_frame_info_t *video_info)
{
   unsigned dims, w, h;
   uint32_t dst[2];
   const video_views_map_t *map = &video_info->views;
   int l                        = video_info->headset_eye[0];
   int r                        = video_info->headset_eye[1];

   if (     l < 0 || r < 0
         || l >= (int)map->num_views || r >= (int)map->num_views
         || !gl_android_openxr_eye_targets(dst, &dims))
      return;
   if (!gl->xr_fbo)
      glGenFramebuffers(1, &gl->xr_fbo);
   glBindFramebuffer(GL_FRAMEBUFFER, gl->xr_fbo);
   glDisable(GL_SCISSOR_TEST);
   gl2_xr_draw_eye_images(gl, video_info, dst, dims, &w, &h);
   glBindFramebuffer(GL_FRAMEBUFFER, 0);
   gl_android_openxr_eyes_drawn(w, h);
}

static float gl2_get_headset_refresh(void *data, float *rates,
      unsigned cap, unsigned *count)
{
   (void)data;
   return gl_android_openxr_refresh(rates, cap, count);
}
#endif

#ifdef HAVE_GL2_XR
static void gl2_viewport_info(void *data, struct video_viewport *vp);

/* Desktop headset output: the window as a screen in the room, or an
 * untracked map's screens cut from it, and a tracked map's eyes as a
 * projection, through gl_openxr's shared textures. The Quest's group
 * above is its own: neither is built with the other. */
typedef struct gl2_desktop_xr
{
   gl_openxr_t *host;
   video_xr_quad_set_t set;
   /* Where each slot's layers lie in the window, from its top left. */
   video_views_rect_t rects[VIDEO_XR_MAX_SLOTS][2];
   /* The size of each slot's image the headset has, 0 for none. */
   unsigned published[VIDEO_XR_MAX_SLOTS];
   GLuint fbo;
   /* The core's last frame size: a repeat brings none. */
   unsigned frame_dims;
   unsigned recenter;
   /* The headset paces frames, so the window presents without
    * waiting; window_interval is what the window was asked for. */
   int window_interval;
   bool paced;
   /* The headset was given a projection to show. */
   bool proj_shown;
   bool recenter_seen;
   bool hdr_warned;
} gl2_desktop_xr_t;

/* Whether the map names two tracked eyes. */
static bool gl2_desktop_xr_tracked(const video_frame_info_t *video_info)
{
   int l = video_info->headset_eye[0];
   int r = video_info->headset_eye[1];
   return l >= 0 && r >= 0
      && l < (int)video_info->views.num_views
      && r < (int)video_info->views.num_views;
}

/* Whether the projection can take the tracked eyes, as glcore's: at a
 * size the runtime has not refused, and on a repeat only once it has
 * them, or on a software repeat while none is shown, if the views the
 * frame was drawn from still hold. Otherwise the window stays. */
static bool gl2_desktop_xr_projection_ok(const gl2_t *gl, bool new_frame,
      bool kept_frame)
{
   const gl2_desktop_xr_t *xr = gl->desktop_xr;
   unsigned dims              = gl_openxr_eye_dims(xr->host);
   return dims
      && gl_openxr_slot_refused(xr->host, OPENXR_PROJECTION_SLOT) != dims
      && (   new_frame || xr->proj_shown
          || (kept_frame && gl_openxr_has_given(xr->host)));
}

/* A tracked map's eyes into the projection slot, a layer each, with
 * the poses the core was given. */
static bool gl2_desktop_xr_draw_eyes(gl2_t *gl,
      const video_frame_info_t *video_info)
{
   unsigned w, h;
   uint32_t dst[2];
   openxr_projection_t p;
   gl2_desktop_xr_t *xr = gl->desktop_xr;
   unsigned dims        = gl_openxr_eye_dims(xr->host);

   if (     !dims
         || gl->tex_info.tex_size[0] <= 0.0f
         || gl->tex_info.tex_size[1] <= 0.0f
         || !gl_openxr_slot_target(xr->host, OPENXR_PROJECTION_SLOT, dims,
            2, dst))
      return false;
   glBindFramebuffer(GL_FRAMEBUFFER, xr->fbo);
   gl2_xr_draw_eye_images(gl, video_info, dst, dims, &w, &h);
   gl_openxr_projection(xr->host, w, h, &p);
   gl_openxr_slot_publish(xr->host, OPENXR_PROJECTION_SLOT, NULL, &p);
   xr->proj_shown = true;
   return true;
}

/* This frame's quads and where each is cut from the window: the window
 * whole while the menu is open, without a map or while a tracked map's
 * eyes are not the projection's, an untracked map's screens otherwise,
 * and nothing while the eyes fill the headset. vp: where the window
 * drew the frame; map: the frame has one that fits it. */
static void gl2_desktop_xr_plan(gl2_t *gl,
      const video_frame_info_t *video_info, const video_viewport_t *vp,
      bool map, bool eyes)
{
   unsigned s;
   video_xr_params_t p;
   gl2_desktop_xr_t *xr = gl->desktop_xr;
   bool menu            = false;
#ifdef HAVE_MENU
   menu = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
#endif

   memset(&p, 0, sizeof(p));
   if (!eyes || menu)
   {
      p.map           = map ? &video_info->views : NULL;
      p.frame_dims    = gl->video_dims;
      video_xr_pose_identity(&p.anchor);
      /* A window shown whole despite a map is not screen 1. */
      p.screens       = (map || !video_info->views.num_views)
         ? video_info->headset_screens : NULL;
      p.edit_seq      = video_info->headset_edit_seq;
      p.distance      = video_info->headset_distance;
      p.width         = video_info->headset_width;
      p.px_per_rad    = gl_openxr_pixels_per_radian(xr->host);
      p.max_dim       = gl_openxr_max_dim(xr->host);
      p.screen_layout = video_info->screen_layout;
      p.rotation      = gl->rotation / 90;
      p.swap_eyes     = video_info->stereo_swap_eyes;
      p.window        = true;
   }
   /* With no width, an empty set: the eyes are all there is. */
   video_xr_place_window(&p, menu,
         map && gl2_desktop_xr_tracked(video_info), vp, xr->frame_dims,
         &xr->set, xr->rects);
   for (s = 0; s < VIDEO_XR_MAX_SLOTS; s++)
      xr->set.slots[s].dims = video_xr_keep_dims(xr->set.slots[s].dims,
            xr->published[s], video_info->headset_editing);
}

/* The plan's rectangles out of the window's back buffer, which is
 * bottom up, into each slot's textures, scaled to the slot's size; what
 * lies past the window stays black. A slot the plan leaves out shows
 * nothing more: at once, or with eyes once the projection shows. */
static void gl2_desktop_xr_draw(gl2_t *gl, bool eyes)
{
   unsigned s, l;
   gl2_desktop_xr_t *xr = gl->desktop_xr;
   GLint full_h         = (GLint)VIDEO_SCALE_H(gl->video_dims);

   glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
   for (s = 0; s < VIDEO_XR_MAX_SLOTS; s++)
   {
      uint32_t tex[2];
      const video_xr_slot_t *plan = &xr->set.slots[s];
      if (!plan->dims)
      {
         if (xr->published[s])
         {
            if (eyes)
               gl_openxr_slot_yield(xr->host, s);
            else
               gl_openxr_slot_forget(xr->host, s);
         }
         xr->published[s] = 0;
         continue;
      }
      if (     gl_openxr_slot_refused(xr->host, s) == plan->dims
            || !gl_openxr_slot_target(xr->host, s, plan->dims, plan->layers,
               tex))
         continue;
      gl2_renderchain_bind_backbuffer();
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, xr->fbo);
      for (l = 0; l < plan->layers; l++)
      {
         const video_views_rect_t *r = &xr->rects[s][l];
         GLint x = VIDEO_POS_X(r->pos);
         GLint y = full_h - VIDEO_POS_Y(r->pos) - (GLint)VIDEO_SCALE_H(r->dims);
         glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
               GL_TEXTURE_2D, (GLuint)tex[l], 0);
         glClear(GL_COLOR_BUFFER_BIT);
         glBlitFramebuffer(x, y, x + (GLint)VIDEO_SCALE_W(r->dims),
               y + (GLint)VIDEO_SCALE_H(r->dims), 0, 0,
               (GLint)VIDEO_SCALE_W(plan->dims),
               (GLint)VIDEO_SCALE_H(plan->dims),
               GL_COLOR_BUFFER_BIT, GL_LINEAR);
      }
      gl_openxr_slot_publish(xr->host, s, &xr->set, NULL);
      xr->published[s] = plan->dims;
   }
}

/* Whether the headset shows this frame: a session it can see, its pixel
 * density measured, and no HDR output, whose window is not what the
 * headset's images hold. */
static bool gl2_desktop_xr_ready(gl2_t *gl)
{
   unsigned s;
   gl2_desktop_xr_t *xr = gl->desktop_xr;
   if (     !gl_openxr_should_draw(xr->host)
         || gl_openxr_pixels_per_radian(xr->host) <= 0.0f)
      return false;
   if (!gl->scrgb.active)
      return true;
   if (!xr->hdr_warned)
   {
      RARCH_WARN("[OpenXR] The headset shows nothing while HDR output is on.\n");
      xr->hdr_warned = true;
   }
   /* Nothing, rather than its last frame. */
   for (s = 0; s < VIDEO_XR_MAX_SLOTS; s++)
   {
      if (xr->published[s])
         gl_openxr_slot_forget(xr->host, s);
      xr->published[s] = 0;
   }
   if (xr->proj_shown)
      gl_openxr_slot_forget(xr->host, OPENXR_PROJECTION_SLOT);
   xr->proj_shown = false;
   return false;
}

/* While the headset paces the core, the window presents without
 * waiting, and each core frame waits for the headset instead. */
static void gl2_desktop_xr_pace_setup(gl2_t *gl,
      const video_frame_info_t *video_info)
{
   gl2_desktop_xr_t *xr = gl->desktop_xr;
   bool paced           = video_info->headset_interval != 0;
   gl_openxr_set_pacing(xr->host, video_info->headset_interval);
   gl_openxr_request_rate(xr->host, video_info->headset_request_hz);
   if (paced == xr->paced)
      return;
   xr->paced = paced;
   if (gl->ctx_driver->swap_interval)
   {
      int interval = paced ? 0 : xr->window_interval;
      gl->ctx_driver->swap_interval(gl->ctx_data, interval);
      gl2_present_vsynced = (interval != 0);
   }
   RARCH_LOG("[GL] Headset pacing %s.\n", paced
         ? "on: the window presents without waiting" : "off");
}

/* The window this frame drew, before it is swapped, to the headset:
 * what it shows of it, and a tracked map's eyes from the core's frame.
 * frame: a core frame came, of dims; vp: where the window drew it. The
 * window's framebuffer, scissor and blending are as they were. */
static void gl2_desktop_xr_frame(gl2_t *gl,
      const video_frame_info_t *video_info, bool frame, unsigned dims,
      const video_viewport_t *vp)
{
   bool map, eyes;
   GLboolean scissor, blend;
   gl2_desktop_xr_t *xr = gl->desktop_xr;
   bool new_frame       = frame && !video_info->frame_repeat;

   /* A recenter request since the last frame, and where the screens are
    * kept, go to the headset's frame. */
   if (xr->recenter_seen && xr->recenter != video_info->headset_recenter)
      gl_openxr_request_recenter(xr->host);
   xr->recenter      = video_info->headset_recenter;
   xr->recenter_seen = true;
   gl_openxr_set_placement(xr->host, video_info->headset_placement);
   gl2_desktop_xr_pace_setup(gl, video_info);
   if (frame && dims)
      xr->frame_dims = dims;
   /* Views kept across a reinit drew only the frame a repeat shows. */
   if (new_frame)
      gl_openxr_new_frame(xr->host);
   else
      gl_openxr_place_kept(xr->host);
   if (!gl2_desktop_xr_ready(gl))
      return;

   /* The map was checked against the core's frame; threaded video can
    * crop it, and then the window is all there is to show. */
   map  = video_views_fit_frame(&video_info->views, xr->frame_dims);
   eyes = map && gl2_desktop_xr_tracked(video_info)
      && gl2_desktop_xr_projection_ok(gl, new_frame, frame);
   if (!xr->fbo)
      glGenFramebuffers(1, &xr->fbo);
   /* Neither the blits nor the clears are to be clipped. */
   scissor = glIsEnabled(GL_SCISSOR_TEST);
   blend   = glIsEnabled(GL_BLEND);
   glDisable(GL_SCISSOR_TEST);
   gl2_desktop_xr_plan(gl, video_info, vp, map, eyes);
   gl2_desktop_xr_draw(gl, eyes);
   /* A repeat draws the eyes only for a projection not shown yet, with
    * the poses that drew them. Without eyes the projection goes, in
    * order behind the window that takes its place; a repeat may be a
    * video-thread redraw, which names no eyes. */
   if (eyes && (new_frame || !xr->proj_shown))
      gl2_desktop_xr_draw_eyes(gl, video_info);
   else if (xr->proj_shown && !eyes && !video_info->frame_repeat)
   {
      gl_openxr_slot_forget(xr->host, OPENXR_PROJECTION_SLOT);
      xr->proj_shown = false;
   }

   /* An attachment held would keep a replaced texture alive. */
   glBindFramebuffer(GL_FRAMEBUFFER, xr->fbo);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
         GL_TEXTURE_2D, 0, 0);
   gl2_renderchain_bind_backbuffer();
   if (scissor)
      glEnable(GL_SCISSOR_TEST);
   if (blend)
      glEnable(GL_BLEND);
}

/* Headset output from a context in the window's share group, when the
 * setting is on and the context can do what it takes; the window as
 * ever otherwise. The window's context is current. */
static void gl2_desktop_xr_start(gl2_t *gl)
{
   gl2_desktop_xr_t *xr;
   settings_t *settings = config_get_ptr();
   if (!settings->bools.video_openxr_enable)
      return;
   /* The window's rectangles are scaled into the headset's images. */
   if (     !(gl->flags & GL2_FLAG_HAVE_FBO)
         || !(   gl->version_major >= 3
              || gl_query_extension("ARB_framebuffer_object")))
   {
      RARCH_WARN("[OpenXR] This GL context has no framebuffer blits; no headset output.\n");
      return;
   }
   if (!(xr = (gl2_desktop_xr_t*)calloc(1, sizeof(*xr))))
      return;
   if (     !(xr->host = gl_openxr_new(gl->ctx_driver, gl->ctx_data))
         || !gl_openxr_start(xr->host))
   {
      gl_openxr_free(xr->host);
      free(xr);
      return;
   }
   /* With HDR output the headset is shown nothing: no HMD either. */
   gl_openxr_set_hidden(xr->host, gl->scrgb.active);
   gl->desktop_xr = xr;
}

/* Before the core's context and the window's: the XR thread stops and
 * its context goes. The window's context is current. */
static void gl2_desktop_xr_free(gl2_t *gl)
{
   gl2_desktop_xr_t *xr = gl->desktop_xr;
   if (!xr)
      return;
   if (xr->fbo)
      glDeleteFramebuffers(1, &xr->fbo);
   gl_openxr_free(xr->host);
   free(xr);
   gl->desktop_xr = NULL;
}

/* The XR thread stops before the core destroys its context. */
static void gl2_desktop_xr_context_destroying(void *data)
{
   gl2_t *gl = (gl2_t*)data;
   if (gl && gl->desktop_xr)
      gl_openxr_stop_thread(gl->desktop_xr->host);
}

static float gl2_desktop_xr_refresh(void *data, float *rates, unsigned cap,
      unsigned *count)
{
   gl2_t *gl = (gl2_t*)data;
   *count    = 0;
   if (!gl || !gl->desktop_xr || !gl_openxr_alive(gl->desktop_xr->host))
      return 0.0f;
   *count = gl_openxr_refresh_rates(gl->desktop_xr->host, rates, cap);
   return gl_openxr_refresh_rate(gl->desktop_xr->host);
}
#endif /* HAVE_GL2_XR */

/* The buffers the frame textures are sized by, at side x side: as init
 * sizes them, with an extra row on the clear's source */
static bool gl2_frame_buffers_grow(gl2_t *gl, unsigned side)
{
   void *empty_buf = calloc((size_t)side * (side + 1), sizeof(uint32_t));
   void *conv_buf  = calloc((size_t)side * side, sizeof(uint32_t));
   if (!empty_buf || !conv_buf)
   {
      free(empty_buf);
      free(conv_buf);
      return false;
   }
   free(gl->empty_buf);
   free(gl->conv_buffer);
   gl->empty_buf   = empty_buf;
   gl->conv_buffer = conv_buf;
   gl->tex_dims    = VIDEO_SCALE_PACK(side, side);
   return true;
}

/* The frame textures made again at tex_dims, with the render chain's
 * passes around them */
static void gl2_frame_textures_remake(gl2_t *gl)
{
   if (gl->flags & GL2_FLAG_FBO_INITED)
      gl2_renderchain_deinit_fbo(gl,
            (gl2_renderchain_data_t*)gl->renderchain_data);
#ifdef GL2_KMS_FRAMES
   gl2_kms_frames_drop(gl);
#endif
   glDeleteTextures(gl->textures, gl->texture);
   gl->tex_index = 0;
   gl2_init_textures(gl);
   gl2_init_textures_data(gl);
   gl2_renderchain_init(gl,
         (gl2_renderchain_data_t*)gl->renderchain_data);
}

/* A software frame past the textures - a core past the geometry it
 * declared - has them made again large enough, as glcore does: an
 * upload past a texture's edge fills none of it. */
static void gl2_frame_fit_textures(gl2_t *gl, unsigned width,
      unsigned height)
{
   GLint max_size = 0;
   unsigned side;
   if (     (width  <= VIDEO_SCALE_W(gl->tex_dims)
         &&  height <= VIDEO_SCALE_H(gl->tex_dims))
         || (gl->flags & (GL2_FLAG_HW_RENDER_USE
               | GL2_FLAG_SHARED_CONTEXT_USE)))
      return;
   side = next_pow2(MAX(width, height));
   glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);
   if (side > (unsigned)max_size || !gl2_frame_buffers_grow(gl, side))
      return;
   gl2_frame_textures_remake(gl);
   RARCH_LOG("[GL] A %ux%u frame is past the textures: made again at "
         "%ux%u.\n", width, height, side, side);
}

#ifdef GL2_VIEWS
/* What a frame's placement draws read */
struct gl2_views_draw
{
   const video_frame_info_t *video_info;
   GLuint target;
   unsigned target_height;
   unsigned frame_count;
};

static void gl2_views_draw_direct(gl2_t *gl,
      const struct gl2_views_draw *d, unsigned view,
      const video_views_rect_t *dst, const video_views_rect_t *cut);
static void gl2_views_draw_pass(gl2_t *gl, const struct gl2_views_draw *d,
      const video_views_rect_t *dst, const video_views_rect_t *cut);
static bool gl2_views_target_make(gl2_t *gl, struct gl2_views_fbo *t,
      unsigned dims);
static bool gl2_views_target_retire(gl2_t *gl, struct gl2_views_fbo *t);

/* The views compositor, with these calls bound in. A view's chain is
 * the driver: with a preset its passes are drawn for one view at a
 * time, from that view's copy. */
#define VV_FN(name)                     gl2_vv_##name
#define VV_DRV                          gl2_t
#define VV_CHAIN                        void
#define VV_CREATE_STOCK(drv)            ((void*)(drv))
#define VV_SYNC_PARAMS(drv, c)          ((void)0)
#define VV_DRAW                         struct gl2_views_draw
#define VV_DIRECT(p, d, v, r, c)        gl2_views_draw_direct(p, d, v, r, c)
#define VV_PASS(p, d, f, r, c, a)       gl2_views_draw_pass(p, d, r, c)
#define VV_TARGET                       struct gl2_views_fbo
#define VV_TARGET_MAKE(drv, t, dims)    gl2_views_target_make(drv, t, dims)
#define VV_TARGET_RETIRE(drv, t)        gl2_views_target_retire(drv, t)
#define VV_ROWS_UP
#include "../video_views_draw.h"

/* Views: not with HDR output or an HDR10 source, nor while a headset
 * shows the frame, or without their programs */
static bool gl2_views_allowed(const gl2_t *gl)
{
   return !gl->views.failed && !gl->scrgb.active
      && !gl->video_info.source_hdr10
#ifdef HAVE_GL2_XR
      && !(gl->desktop_xr && gl_openxr_alive(gl->desktop_xr->host))
#endif
#ifdef HAVE_OPENXR
      && !gl_android_openxr_active()
#endif
      ;
}

static void gl2_views_publish(gl2_t *gl)
{
   retro_atomic_store_release_int(&gl->views.allowed,
         gl2_views_allowed(gl) ? 1 : 0);
}

/* Rect: the part of the texture drawn. Clamp: its first and last texel
 * centres, so filtering stops at its edge as at a texture's. The canvas
 * holds the left eye on its left half, the right eye on its right;
 * anaglyph is Dubois' least-squares red/cyan, interlaced takes even rows
 * from the left half, odd from the right. The UI layer is
 * premultiplied. */
static const char gl2_views_vert_src[] =
   "attribute vec2 Pos;\n"
   "attribute vec2 Tex;\n"
   "uniform mat4 uMVP;\n"
   "varying vec2 vTex;\n"
   "void main()\n"
   "{\n"
   "   gl_Position = uMVP * vec4(Pos, 0.0, 1.0);\n"
   "   vTex = Tex;\n"
   "}\n";

static const char gl2_views_frag_head[] =
   "#ifdef GL_ES\n"
   "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
   "precision highp float;\n"
   "#else\n"
   "precision mediump float;\n"
   "#endif\n"
   "#endif\n"
   "uniform sampler2D uTex;\n"
   "varying vec2 vTex;\n";

static const char gl2_views_rect_src[] =
   "uniform vec4 uClamp;\n"
   "void main()\n"
   "{\n"
   "   gl_FragColor = vec4(texture2D(uTex,\n"
   "         clamp(vTex, uClamp.xy, uClamp.zw)).rgb, 1.0);\n"
   "}\n";

static const char gl2_views_layer_src[] =
   "void main()\n"
   "{\n"
   "   gl_FragColor = texture2D(uTex, vTex);\n"
   "}\n";

/* In two: C89 only guarantees 509 characters a literal. */
static const char gl2_views_anaglyph_src[] =
   "void main()\n"
   "{\n"
   "   vec2 uv = vec2(vTex.x * 0.5, vTex.y);\n"
   "   vec3 l  = texture2D(uTex, uv).rgb;\n"
   "   vec3 r  = texture2D(uTex, uv + vec2(0.5, 0.0)).rgb;\n";

static const char gl2_views_anaglyph_src2[] =
   "   vec3 c  = vec3(\n"
   "         dot(l, vec3( 0.456100,  0.500484,  0.176381))\n"
   "       + dot(r, vec3(-0.0434706, -0.0879388, -0.00155529)),\n"
   "         dot(l, vec3(-0.0400822, -0.0378246, -0.0157589))\n"
   "       + dot(r, vec3( 0.378476,   0.73364,   -0.0184503)),\n"
   "         dot(l, vec3(-0.0152161, -0.0205971, -0.00546856))\n"
   "       + dot(r, vec3(-0.0721527, -0.112961,   1.2264)));\n"
   "   gl_FragColor = vec4(clamp(c, 0.0, 1.0), 1.0);\n"
   "}\n";

static const char gl2_views_interlaced_src[] =
   "uniform float uHeight;\n"
   "void main()\n"
   "{\n"
   "   float row = floor(vTex.y * uHeight);\n"
   "   gl_FragColor = vec4(texture2D(uTex, vec2(vTex.x * 0.5\n"
   "         + 0.5 * mod(row, 2.0), vTex.y)).rgb, 1.0);\n"
   "}\n";

/* A core context takes GLSL 1.50 */
static const char gl2_views_vert_core[] =
   "#version 150\n"
   "#define attribute in\n"
   "#define varying out\n";

static const char gl2_views_frag_core[] =
   "#version 150\n"
   "#define varying in\n"
   "#define texture2D texture\n"
   "#define gl_FragColor FragColor\n"
   "out vec4 FragColor;\n";

static bool gl2_views_program(gl2_t *gl, struct gl2_views_prog *p,
      const char *frag, const char *frag2)
{
   GLint status = 0;
   GLuint vs, fs;
   const char *src[4];
   bool core = (gl->flags & GL2_FLAG_CORE_CONTEXT_IN_USE) ? true : false;

   src[0] = core ? gl2_views_vert_core : "";
   src[1] = gl2_views_vert_src;
   if (!(vs = gl2_scrgb_compile_stage(GL_VERTEX_SHADER, src, 2)))
      return false;
   src[0] = core ? gl2_views_frag_core : "";
   src[1] = gl2_views_frag_head;
   src[2] = frag;
   src[3] = frag2;
   if (!(fs = gl2_scrgb_compile_stage(GL_FRAGMENT_SHADER, src, 4)))
   {
      glDeleteShader(vs);
      return false;
   }
   p->prog = glCreateProgram();
   glAttachShader(p->prog, vs);
   glAttachShader(p->prog, fs);
   glBindAttribLocation(p->prog, 0, "Pos");
   glBindAttribLocation(p->prog, 1, "Tex");
   glLinkProgram(p->prog);
   glDeleteShader(vs);
   glDeleteShader(fs);
   glGetProgramiv(p->prog, GL_LINK_STATUS, &status);
   if (!status)
   {
      glDeleteProgram(p->prog);
      p->prog = 0;
      return false;
   }
   p->loc_mvp    = glGetUniformLocation(p->prog, "uMVP");
   p->loc_tex    = glGetUniformLocation(p->prog, "uTex");
   p->loc_clamp  = glGetUniformLocation(p->prog, "uClamp");
   p->loc_height = glGetUniformLocation(p->prog, "uHeight");
   return true;
}

static bool gl2_views_target_retire(gl2_t *gl, struct gl2_views_fbo *t)
{
   (void)gl;
   if (t->fbo)
      gl2_delete_fb(1, &t->fbo);
   if (t->tex)
      glDeleteTextures(1, &t->tex);
   t->fbo = 0;
   t->tex = 0;
   return true;
}

/* The views' copies and feedback let go */
static void gl2_views_free_copies(gl2_t *gl)
{
   unsigned i, k;
   if (!gl->views.copies)
      return;
   for (i = 0; i < RETRO_VIDEO_VIEWS_MAX; i++)
   {
      for (k = 0; k < GL2_VIEWS_HISTORY; k++)
      {
         if (gl->views.tex[i][k])
            glDeleteTextures(1, &gl->views.tex[i][k]);
         gl->views.tex[i][k]         = 0;
         gl->views.tex_dims[i][k]    = 0;
         gl->views.tex_written[i][k] = false;
      }
      gl2_views_target_retire(gl, &gl->views.feedback[i]);
      gl->views.feedback_dims[i] = 0;
      gl->views.head[i]          = 0;
      gl->views.prev_live[i]     = false;
   }
   if (gl->views.copy_fbo)
      gl2_delete_fb(1, &gl->views.copy_fbo);
   gl->views.copy_fbo = 0;
   gl->views.copies   = false;
}

static void gl2_views_release(gl2_t *gl)
{
   if (gl->views.rect.prog)
      glDeleteProgram(gl->views.rect.prog);
   if (gl->views.layer.prog)
      glDeleteProgram(gl->views.layer.prog);
   if (gl->views.anaglyph.prog)
      glDeleteProgram(gl->views.anaglyph.prog);
   if (gl->views.interlaced.prog)
      glDeleteProgram(gl->views.interlaced.prog);
   if (gl->views.vbo)
      glDeleteBuffers(1, &gl->views.vbo);
   gl2_views_free_copies(gl);
   gl->views.rect.prog       = 0;
   gl->views.layer.prog      = 0;
   gl->views.anaglyph.prog   = 0;
   gl->views.interlaced.prog = 0;
   gl->views.vbo             = 0;
   gl2_vv_target_drop(gl, &gl->views.canvas_target, &gl->views.canvas);
   gl2_vv_target_drop(gl, &gl->views.ui_target, &gl->views.ui);
}

/* The programs views draw with, made on first use: most sessions never
 * draw views. Without them the frame is drawn whole. */
static void gl2_views_init(gl2_t *gl)
{
   gl->views.tried = true;
#if defined(glCreateShader) && defined(glCreateProgram)
   if (!glCreateShader || !glCreateProgram)
   {
      gl->views.failed = true;
      return;
   }
#endif
   glGenBuffers(1, &gl->views.vbo);
   if (     gl->views.vbo
         && gl2_views_program(gl, &gl->views.rect, gl2_views_rect_src, "")
         && gl2_views_program(gl, &gl->views.layer, gl2_views_layer_src,
            "")
         && gl2_views_program(gl, &gl->views.anaglyph,
            gl2_views_anaglyph_src, gl2_views_anaglyph_src2)
         && gl2_views_program(gl, &gl->views.interlaced,
            gl2_views_interlaced_src, ""))
      return;
   RARCH_WARN("[GL] Views can't be drawn: their shaders failed.\n");
   gl2_views_release(gl);
   gl->views.failed = true;
}

/* An FBO with an RGBA texture sized dims */
static bool gl2_views_target_make(gl2_t *gl, struct gl2_views_fbo *t,
      unsigned dims)
{
   GLint max_size = 0;
   bool ok        = false;
   unsigned w     = VIDEO_SCALE_W(dims);
   unsigned h     = VIDEO_SCALE_H(dims);
   glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);
   if (     (gl->flags & GL2_FLAG_HAVE_FBO)
         && w <= (unsigned)max_size && h <= (unsigned)max_size)
   {
      gl2_gen_fb(1, &t->fbo);
      glGenTextures(1, &t->tex);
      glBindTexture(GL_TEXTURE_2D, t->tex);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
            GL_UNSIGNED_BYTE, NULL);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glBindTexture(GL_TEXTURE_2D, 0);
      gl2_bind_fb(t->fbo);
      gl2_fb_texture_2d(RARCH_GL_FRAMEBUFFER, RARCH_GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, t->tex, 0);
      ok = gl2_check_fb_status(RARCH_GL_FRAMEBUFFER)
         == RARCH_GL_FRAMEBUFFER_COMPLETE;
      gl2_renderchain_bind_backbuffer();
   }
   if (!ok)
   {
      RARCH_WARN("[GL] Views can't draw into a %ux%u image. "
            "Drawing without it.\n", w, h);
      gl2_views_target_retire(gl, t);
   }
   return ok;
}

/* A top-left rectangle as GL's x, y, width and height in a
 * target_height-high target */
static void gl2_views_gl_rect(const video_views_rect_t *r,
      unsigned target_height, GLint *out)
{
   out[0] = VIDEO_POS_X(r->pos);
   out[1] = (GLint)target_height
      - (VIDEO_POS_Y(r->pos) + (GLint)VIDEO_SCALE_H(r->dims));
   out[2] = (GLint)VIDEO_SCALE_W(r->dims);
   out[3] = (GLint)VIDEO_SCALE_H(r->dims);
}

/* Draws the unit quad with p and mvp, sampling tex over tex_coords */
static void gl2_views_quad(gl2_t *gl, const struct gl2_views_prog *p,
      const math_matrix_4x4 *mvp, const GLfloat *tex_coords)
{
   unsigned k;
   GLfloat data[16];
   for (k = 0; k < 4; k++)
   {
      data[k * 4 + 0] = vertexes[k * 2 + 0];
      data[k * 4 + 1] = vertexes[k * 2 + 1];
      data[k * 4 + 2] = tex_coords[k * 2 + 0];
      data[k * 4 + 3] = tex_coords[k * 2 + 1];
   }
   glUseProgram(p->prog);
   if (p->loc_mvp >= 0)
      glUniformMatrix4fv(p->loc_mvp, 1, GL_FALSE, mvp->data);
   if (p->loc_tex >= 0)
      glUniform1i(p->loc_tex, 0);
   glBindBuffer(GL_ARRAY_BUFFER, gl->views.vbo);
   glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STREAM_DRAW);
   glEnableVertexAttribArray(0);
   glEnableVertexAttribArray(1);
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
         (const GLvoid*)0);
   glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
         (const GLvoid*)(2 * sizeof(GLfloat)));
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glDisableVertexAttribArray(0);
   glDisableVertexAttribArray(1);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/* View @view of the frame over @dst, cut to @cut, as the stock shader
 * draws the frame: the same texels, the same filter, stopping at the
 * view's edge as at a texture's. */
static void gl2_views_draw_direct(gl2_t *gl,
      const struct gl2_views_draw *d, unsigned view,
      const video_views_rect_t *dst, const video_views_rect_t *cut)
{
   GLint vp[4], sc[4];
   GLfloat coords[8];
   const struct retro_video_view *v = &d->video_info->views.views[view];
   float tw      = gl->tex_info.tex_size[0];
   float th      = gl->tex_info.tex_size[1];
   float frame_h = gl->tex_info.input_size[1];
   float s0      = (float)v->x / tw;
   float s1      = (float)(v->x + v->width) / tw;
   float t0, t1; /* the rows at the view's bottom and top */

   /* A hardware frame's rows count from its bottom */
   if (gl->vertex_ptr == vertexes)
   {
      t0 = (frame_h - (float)(v->y + v->height)) / th;
      t1 = (frame_h - (float)v->y) / th;
   }
   else
   {
      t0 = (float)(v->y + v->height) / th;
      t1 = (float)v->y / th;
   }
   coords[0] = s0;
   coords[1] = t0;
   coords[2] = s1;
   coords[3] = t0;
   coords[4] = s0;
   coords[5] = t1;
   coords[6] = s1;
   coords[7] = t1;

   gl2_views_gl_rect(dst, d->target_height, vp);
   gl2_views_gl_rect(cut, d->target_height, sc);
   glViewport(vp[0], vp[1], vp[2], vp[3]);
   glEnable(GL_SCISSOR_TEST);
   glScissor(sc[0], sc[1], sc[2], sc[3]);
   glUseProgram(gl->views.rect.prog);
   if (gl->views.rect.loc_clamp >= 0)
      glUniform4f(gl->views.rect.loc_clamp,
            s0 + 0.5f / tw, MIN(t0, t1) + 0.5f / th,
            s1 - 0.5f / tw, MAX(t0, t1) - 0.5f / th);
   gl2_views_quad(gl, &gl->views.rect, &gl->mvp, coords);
   glDisable(GL_SCISSOR_TEST);
}

/* The frame's rows of view v as texture coordinates: those at its top
 * and at its bottom */
static void gl2_views_rows(const gl2_t *gl, const struct retro_video_view *v,
      float *top, float *bottom)
{
   float th      = gl->tex_info.tex_size[1];
   float frame_h = gl->tex_info.input_size[1];
   /* A hardware frame's rows count from its bottom */
   if (gl->vertex_ptr == vertexes)
   {
      *top    = (frame_h - (float)v->y) / th;
      *bottom = (frame_h - (float)(v->y + v->height)) / th;
   }
   else
   {
      *top    = (float)v->y / th;
      *bottom = (float)(v->y + v->height) / th;
   }
}

/* View i's texture for this draw, of the @depth it keeps: a new
 * frame's the next, the oldest, a dupe's the newest again. Made at the
 * view's size, as the frame's textures are when a software frame's view
 * is @written straight in, else for a copy drawn into it; bound. */
static GLuint gl2_views_slot(gl2_t *gl, unsigned i,
      const struct retro_video_view *v, unsigned depth, bool fresh,
      bool written)
{
   GLuint *tex;
   unsigned dims = VIDEO_SCALE_PACK(v->width, v->height);
   unsigned k    = gl->views.head[i] % depth;

   if (fresh && depth > 1)
      k = (k + 1) % depth;
   gl->views.head[i] = k;
   gl->views.copies  = true;
   tex               = &gl->views.tex[i][k];
   if (     !*tex || gl->views.tex_dims[i][k] != dims
         || gl->views.tex_written[i][k] != written)
   {
      /* A new name: the storage may be immutable */
      if (*tex)
         glDeleteTextures(1, tex);
      glGenTextures(1, tex);
      glBindTexture(GL_TEXTURE_2D, *tex);
      if (written)
         gl2_load_texture_image(GL_TEXTURE_2D, 0, gl->internal_fmt,
               v->width, v->height, 0, gl->texture_type, gl->texture_fmt,
               NULL);
      else
         glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, v->width, v->height, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, NULL);
      gl->views.tex_dims[i][k]    = dims;
      gl->views.tex_written[i][k] = written;
   }
   /* As the frame's textures are, for the preset's first pass */
   glBindTexture(GL_TEXTURE_2D, *tex);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl->tex_min_filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl->tex_mag_filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl->wrap_mode);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl->wrap_mode);
   return *tex;
}

/* Whether software frames a preset may draw as views have each view
 * written straight into a texture of its own, the frame itself once
 * that is known: where it goes in as it is, unconverted, and not through
 * EGL images. */
static bool gl2_views_write_wanted(const gl2_t *gl,
      const gl2_renderchain_data_t *chain,
      const video_frame_info_t *video_info, bool use_rgba)
{
   return video_info->views.num_views
      && gl->views.preset && video_info->shader_active
      /* A screen's own window copies its views from the frame too */
      && !(video_info->windows && video_info->windows->plan.separate_mask)
      && !(gl->flags & GL2_FLAG_TEXTURE_MIPMAP)
      && !(chain->flags & GL2_CHAIN_FLAG_EGL_IMAGES)
#ifdef HAVE_OPENGLES
      && !(gl->base_size == 4 && use_rgba)
#else
      && !(gl->base_size == 2 && !(gl->flags & GL2_FLAG_HAVE_ES2_COMPAT))
#endif
      ;
}

/* View i of software frame @frame written straight into its texture,
 * its top row first as the frame's: no copy */
static void gl2_views_write(gl2_t *gl, unsigned i,
      const struct retro_video_view *v, unsigned depth,
      const void *frame, unsigned pitch, bool use_rgba)
{
   gl2_views_slot(gl, i, v, depth, true, true);
   gl2_renderchain_copy_frame(gl,
         (gl2_renderchain_data_t*)gl->renderchain_data, use_rgba,
         (const uint8_t*)frame + (size_t)v->y * pitch
         + (size_t)v->x * gl->base_size, v->width, v->height, pitch);
}

/* Whether a dupe finds view i's newest texture at its size: the frame
 * it was taken from again, nothing to copy */
static bool gl2_views_kept(gl2_t *gl, unsigned i,
      const struct retro_video_view *v, unsigned depth)
{
   unsigned k        = gl->views.head[i] % depth;
   gl->views.head[i] = k;
   return gl->views.tex[i][k]
      && gl->views.tex_dims[i][k] == VIDEO_SCALE_PACK(v->width, v->height);
}

/* View i copied out of the frame into a texture of its size, its top
 * row first as a software frame's: the preset reads it as it would a
 * frame of that size. False when it can't be. */
static bool gl2_views_copy(gl2_t *gl, unsigned i,
      const struct retro_video_view *v, unsigned depth, bool fresh)
{
   GLfloat coords[8];
   float top, bottom;
   bool ok;
   float tw   = gl->tex_info.tex_size[0];
   float th   = gl->tex_info.tex_size[1];
   float s0   = (float)v->x / tw;
   float s1   = (float)(v->x + v->width) / tw;
   GLuint tex = gl2_views_slot(gl, i, v, depth, fresh, false);

   if (!gl->views.copy_fbo)
      gl2_gen_fb(1, &gl->views.copy_fbo);
   gl2_bind_fb(gl->views.copy_fbo);
   gl2_fb_texture_2d(RARCH_GL_FRAMEBUFFER, RARCH_GL_COLOR_ATTACHMENT0,
         GL_TEXTURE_2D, tex, 0);
   ok = gl2_check_fb_status(RARCH_GL_FRAMEBUFFER)
      == RARCH_GL_FRAMEBUFFER_COMPLETE;
   if (ok)
   {
      /* One texel a pixel: each sample lands on a texel's centre. The
       * target's bottom row, GL's first, takes the view's top. */
      gl2_views_rows(gl, v, &top, &bottom);
      coords[0] = s0;
      coords[1] = top;
      coords[2] = s1;
      coords[3] = top;
      coords[4] = s0;
      coords[5] = bottom;
      coords[6] = s1;
      coords[7] = bottom;
      glViewport(0, 0, v->width, v->height);
      glBindTexture(GL_TEXTURE_2D, gl->tex_info.tex);
      glUseProgram(gl->views.rect.prog);
      if (gl->views.rect.loc_clamp >= 0)
         glUniform4f(gl->views.rect.loc_clamp, 0.0f, 0.0f, 1.0f, 1.0f);
      gl2_views_quad(gl, &gl->views.rect, &gl->mvp_no_rot, coords);
   }
   gl2_fb_texture_2d(RARCH_GL_FRAMEBUFFER, RARCH_GL_COLOR_ATTACHMENT0,
         GL_TEXTURE_2D, 0, 0);
   return ok;
}

/* Whether the preset can draw each view through its passes, one view
 * after another, each with earlier frames and feedback of its own:
 * its first pass takes no mipmaps, and it reads no further back than
 * the views keep. */
static bool gl2_views_preset_ok(const gl2_t *gl)
{
   return (gl->flags & GL2_FLAG_HAVE_FBO)
      && !(gl->flags & GL2_FLAG_TEXTURE_MIPMAP)
      && gl->shader->get_prev_textures(gl->shader_data)
         < GL2_VIEWS_HISTORY;
}

/* View i's copy just drawn, as the preset reads earlier frames: as the
 * frame's are, pushed every draw, dupes too */
static void gl2_views_push_prev(gl2_t *gl, unsigned i)
{
   struct video_tex_info *prev = gl->views.prev[i];
   memmove(prev + 1, prev, sizeof(*prev) * (GL2_VIEWS_HISTORY - 1));
   memcpy(&prev[0], &gl->tex_info, sizeof(*prev));
}

/* What view i's feedback pass drew is kept for its next draw, as the
 * frame's is (gl2_renderchain_bind_prev_texture) */
static void gl2_views_swap_feedback(gl2_t *gl, unsigned i)
{
   gl2_renderchain_data_t *chain = (gl2_renderchain_data_t*)
      gl->renderchain_data;
   struct gl2_views_fbo *f       = &gl->views.feedback[i];
   unsigned fp                   = gl->fbo_feedback_pass;
   GLuint fbo                    = f->fbo;
   GLuint tex                    = f->tex;
   f->fbo                        = chain->fbo[fp];
   f->tex                        = chain->fbo_texture[fp];
   chain->fbo[fp]                = fbo;
   chain->fbo_texture[fp]        = tex;
}

/* View i's feedback target at the feedback pass's size, made cleared,
 * then what the passes read as feedback */
static void gl2_views_feedback(gl2_t *gl, unsigned i)
{
   struct gl2_views_fbo *f        = &gl->views.feedback[i];
   const struct video_fbo_rect *r = &gl->fbo_rect[gl->fbo_feedback_pass];
   struct video_tex_info *info    = &gl->views.feedback_cur;

   if (!f->fbo || gl->views.feedback_dims[i] != r->dims)
   {
      /* Made as the chain's own target for that pass is, whose place it
       * takes every draw */
      gl2_views_target_retire(gl, f);
      glGenTextures(1, &f->tex);
      gl2_create_fbo_texture(gl,
            (gl2_renderchain_data_t*)gl->renderchain_data,
            gl->fbo_feedback_pass, f->tex,
            (gl->flags & GL2_FLAG_FBO_SMOOTH) != 0,
            (gl->flags & GL2_FLAG_FBO_SRGB_DISABLE) != 0);
      gl2_gen_fb(1, &f->fbo);
      gl2_bind_fb(f->fbo);
      gl2_fb_texture_2d(RARCH_GL_FRAMEBUFFER, RARCH_GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, f->tex, 0);
      glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
      glClear(GL_COLOR_BUFFER_BIT);
      gl->views.feedback_dims[i] = r->dims;
   }
   info->tex           = f->tex;
   info->input_size[0] = VIDEO_SCALE_W(r->img_dims);
   info->input_size[1] = VIDEO_SCALE_H(r->img_dims);
   info->tex_size[0]   = VIDEO_SCALE_W(r->dims);
   info->tex_size[1]   = VIDEO_SCALE_H(r->dims);
   memset(info->coord, 0, sizeof(info->coord));
   SET_TEXTURE_COORDS(info->coord,
         (GLfloat)VIDEO_SCALE_W(r->img_dims) / VIDEO_SCALE_W(r->dims),
         (GLfloat)VIDEO_SCALE_H(r->img_dims) / VIDEO_SCALE_H(r->dims));
}

/* The preset's last pass, or its only one, over @dst, cut to @cut, for
 * the view whose passes were drawn last (gl2_views_preset_view) */
static void gl2_views_draw_pass(gl2_t *gl, const struct gl2_views_draw *d,
      const video_views_rect_t *dst, const video_views_rect_t *cut)
{
   GLint vp[4], sc[4];
   video_shader_ctx_params_t params;
   static GLfloat fbo_coords[8] = { 0.0f, 0.0f, 0.0f, 0.0f,
                                    0.0f, 0.0f, 0.0f, 0.0f };
   struct video_tex_info fbo_info[GFX_MAX_SHADERS];
   gl2_renderchain_data_t *chain = (gl2_renderchain_data_t*)
      gl->renderchain_data;
   unsigned pass                 = 1;
   unsigned i;

   if (d->target)
      gl2_bind_fb(d->target);
   else
      gl2_renderchain_bind_backbuffer();
   gl2_views_gl_rect(dst, d->target_height, vp);
   gl2_views_gl_rect(cut, d->target_height, sc);

   params.info          = &gl->tex_info;
   params.dims          = VIDEO_SCALE_PACK(
         (unsigned)gl->tex_info.input_size[0],
         (unsigned)gl->tex_info.input_size[1]);
   params.tex_dims      = VIDEO_SCALE_PACK(
         (unsigned)gl->tex_info.tex_size[0],
         (unsigned)gl->tex_info.tex_size[1]);
   params.fbo_info      = NULL;
   params.fbo_info_cnt  = 0;
   gl->coords.tex_coord = gl->tex_info.coord;
   glBindTexture(GL_TEXTURE_2D, gl->tex_info.tex);
   if (gl->flags & GL2_FLAG_FBO_INITED)
   {
      const struct video_fbo_rect *r;
      pass = chain->fbo_pass + 1;
      for (i = 0; i < (unsigned)chain->fbo_pass; i++)
      {
         r                      = &gl->fbo_rect[i];
         fbo_info[i].tex        = chain->fbo_texture[i];
         fbo_info[i].input_size[0] = VIDEO_SCALE_W(r->img_dims);
         fbo_info[i].input_size[1] = VIDEO_SCALE_H(r->img_dims);
         fbo_info[i].tex_size[0]   = VIDEO_SCALE_W(r->dims);
         fbo_info[i].tex_size[1]   = VIDEO_SCALE_H(r->dims);
         SET_TEXTURE_COORDS(fbo_info[i].coord,
               (GLfloat)VIDEO_SCALE_W(r->img_dims) / VIDEO_SCALE_W(r->dims),
               (GLfloat)VIDEO_SCALE_H(r->img_dims) / VIDEO_SCALE_H(r->dims));
         fbo_info[i].coord[0] = 0.0f;
         fbo_info[i].coord[1] = 0.0f;
         fbo_info[i].coord[3] = 0.0f;
         fbo_info[i].coord[4] = 0.0f;
      }
      r                    = &gl->fbo_rect[chain->fbo_pass - 1];
      memcpy(fbo_coords, fbo_info[chain->fbo_pass - 1].coord,
            sizeof(fbo_coords));
      params.dims          = r->img_dims;
      params.tex_dims      = r->dims;
      params.fbo_info      = fbo_info;
      params.fbo_info_cnt  = chain->fbo_pass;
      gl->coords.tex_coord = fbo_coords;
      glBindTexture(GL_TEXTURE_2D, chain->fbo_texture[chain->fbo_pass - 1]);
   }

   gl->shader->use(gl, gl->shader_data, pass, true);
   glViewport(vp[0], vp[1], vp[2], vp[3]);
   glEnable(GL_SCISSOR_TEST);
   glScissor(sc[0], sc[1], sc[2], sc[3]);
   params.vp_dims       = dst->dims;
   params.out_dims      = dst->dims;
   params.frame_counter = d->frame_count;
   params.swap_counter  = (unsigned int)d->video_info->swap_count;
   params.prev_info     = gl->views.prev_cur;
   params.feedback_info = &gl->views.feedback_cur;
   gl->shader->set_params(&params, gl->shader_data);
   /* The copy's top row first, as a software frame's */
   gl->coords.vertex    = vertexes_flipped;
   gl->coords.color     = gl->white_color_ptr;
   gl->coords.vertices  = 4;
   gl->shader->set_coords(gl->shader_data, &gl->coords);
   gl->shader->set_mvp(gl->shader_data, &gl->mvp);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glDisable(GL_SCISSOR_TEST);
}

/* The preset's passes but its last drawn for view i, from its copy, at
 * the size of @vp_dims, its first placement: gl->tex_info is the
 * copy's until the caller puts the frame's back. */
static void gl2_views_preset_view(gl2_t *gl,
      const video_frame_info_t *video_info, unsigned i, unsigned vp_dims,
      unsigned frame_count)
{
   static const GLfloat fbo_vertexes[8] = { 0, 0, 1, 0, 0, 1, 1, 1 };
   static GLfloat fbo_coords[8]         = { 0.0f, 0.0f, 0.0f, 0.0f,
                                            0.0f, 0.0f, 0.0f, 0.0f };
   video_shader_ctx_params_t params;
   struct video_tex_info fbo_info[GFX_MAX_SHADERS];
   const struct retro_video_view *v = &video_info->views.views[i];
   gl2_renderchain_data_t *chain    = (gl2_renderchain_data_t*)
      gl->renderchain_data;
   unsigned dims                    = VIDEO_SCALE_PACK(v->width, v->height);
   int p;

   gl->tex_info.tex           = gl->views.tex[i][gl->views.head[i]];
   gl->tex_info.input_size[0] = v->width;
   gl->tex_info.input_size[1] = v->height;
   gl->tex_info.tex_size[0]   = v->width;
   gl->tex_info.tex_size[1]   = v->height;
   SET_TEXTURE_COORDS(gl->tex_info.coord, 1.0f, 1.0f);
   gl->tex_info.coord[0]      = 0.0f;
   gl->tex_info.coord[1]      = 0.0f;
   gl->tex_info.coord[3]      = 0.0f;
   gl->tex_info.coord[4]      = 0.0f;

   /* Earlier frames are this view's own: before its first draw, all
    * this one, as the frame's are before its first */
   if (!gl->views.prev_live[i])
   {
      unsigned k;
      for (k = 0; k < GL2_VIEWS_HISTORY; k++)
         gl->views.prev[i][k] = gl->tex_info;
      gl->views.prev_live[i] = true;
   }
   gl->views.prev_cur     = gl->views.prev[i];
   gl->views.feedback_cur = gl->tex_info;

   if (!(gl->flags & GL2_FLAG_FBO_INITED))
      return;

   gl2_renderchain_recompute_pass_sizes(gl, chain, dims, vp_dims);
   if (gl->flags & GL2_FLAG_FBO_FEEDBACK_ENABLE)
      gl2_views_feedback(gl, i);
#if defined(GL_FRAMEBUFFER_SRGB) && !defined(HAVE_OPENGLES)
   if (chain->flags & GL2_CHAIN_FLAG_HAS_SRGB_FBO)
      glEnable(GL_FRAMEBUFFER_SRGB);
#endif
   gl->coords.vertex   = fbo_vertexes;
   gl->coords.color    = gl->white_color_ptr;
   gl->coords.vertices = 4;
   params.vp_dims       = vp_dims;
   params.frame_counter = frame_count;
   params.swap_counter  = (unsigned int)video_info->swap_count;
   params.info          = &gl->tex_info;
   params.prev_info     = gl->views.prev_cur;
   params.feedback_info = &gl->views.feedback_cur;
   for (p = 0; p < chain->fbo_pass; p++)
   {
      const struct video_fbo_rect *r = &gl->fbo_rect[p];
      gl2_bind_fb(chain->fbo[p]);
      gl->shader->use(gl, gl->shader_data, p + 1, true);
      if (p == 0)
      {
         glBindTexture(GL_TEXTURE_2D, gl->tex_info.tex);
         params.dims          = dims;
         params.tex_dims      = dims;
         gl->coords.tex_coord = gl->tex_info.coord;
      }
      else
      {
         const struct video_fbo_rect *prev = &gl->fbo_rect[p - 1];
         struct video_tex_info *info       = &fbo_info[p - 1];
         info->tex           = chain->fbo_texture[p - 1];
         info->input_size[0] = VIDEO_SCALE_W(prev->img_dims);
         info->input_size[1] = VIDEO_SCALE_H(prev->img_dims);
         info->tex_size[0]   = VIDEO_SCALE_W(prev->dims);
         info->tex_size[1]   = VIDEO_SCALE_H(prev->dims);
         memset(info->coord, 0, sizeof(info->coord));
         SET_TEXTURE_COORDS(info->coord,
               (GLfloat)VIDEO_SCALE_W(prev->img_dims)
               / VIDEO_SCALE_W(prev->dims),
               (GLfloat)VIDEO_SCALE_H(prev->img_dims)
               / VIDEO_SCALE_H(prev->dims));
         memcpy(fbo_coords, info->coord, sizeof(fbo_coords));
         glBindTexture(GL_TEXTURE_2D, chain->fbo_texture[p - 1]);
         if (     gl->shader->mipmap_input(gl->shader_data, p + 1)
               && (gl->flags & GL2_FLAG_HAVE_MIPMAP))
            glGenerateMipmap(GL_TEXTURE_2D);
         params.dims          = prev->img_dims;
         params.tex_dims      = prev->dims;
         gl->coords.tex_coord = fbo_coords;
      }
      glViewport(0, 0, VIDEO_SCALE_W(r->img_dims), VIDEO_SCALE_H(r->img_dims));
      glClear(GL_COLOR_BUFFER_BIT);
      params.out_dims      = r->img_dims;
      params.fbo_info      = p ? fbo_info : NULL;
      params.fbo_info_cnt  = p;
      gl->shader->set_params(&params, gl->shader_data);
      gl->shader->set_coords(gl->shader_data, &gl->coords);
      gl->shader->set_mvp(gl->shader_data, &gl->mvp_no_rot);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   }
   if (     gl->shader->mipmap_input(gl->shader_data, chain->fbo_pass + 1)
         && (gl->flags & GL2_FLAG_HAVE_MIPMAP))
   {
      glBindTexture(GL_TEXTURE_2D, chain->fbo_texture[chain->fbo_pass - 1]);
      glGenerateMipmap(GL_TEXTURE_2D);
   }
#if defined(GL_FRAMEBUFFER_SRGB) && !defined(HAVE_OPENGLES)
   if (chain->flags & GL2_CHAIN_FLAG_HAS_SRGB_FBO)
      glDisable(GL_FRAMEBUFFER_SRGB);
#endif
}

/* Whether this frame draws its views, with each drawn view's chain
 * picked and the rectangle its first placement takes; in *blend
 * whether they draw into the canvas, in *direct whether straight from
 * the frame, else through the preset. */
static bool gl2_views_setup(gl2_t *gl, const video_frame_info_t *video_info,
      const void *frame, bool *blend, bool *direct, void **chains,
      video_views_rect_t *rects)
{
   unsigned i;
   bool views;
   const video_views_layout_t *layout = &video_info->views_layout;
   bool offscreen = video_info->views.num_views && layout->offscreen;
   bool stock     = !video_info->shader_active || !gl->views.preset;

   if (video_info->views.num_views && !gl->views.tried)
      gl2_views_init(gl);
   /* Anaglyph and interlaced draw both eyes into the canvas, or without
    * one only the left eye's area into the window. */
   *blend = offscreen && !gl->views.failed
      && gl2_vv_target_ensure(gl, &gl->views.canvas_target,
            &gl->views.canvas, layout->canvas_dims);
   /* A real frame without the blend modes frees the canvas. */
   if (frame && !offscreen)
      gl2_vv_target_drop(gl, &gl->views.canvas_target, &gl->views.canvas);
   /* A preset that keeps frames from one draw to the next draws the
    * frame whole. */
   views = video_info->views.num_views
      && gl2_views_allowed(gl)
      && (stock ? !(gl->flags & GL2_FLAG_FBO_INITED)
                : gl2_views_preset_ok(gl))
      && video_views_fit_frame(&video_info->views, VIDEO_SCALE_PACK(
               (unsigned)gl->tex_info.input_size[0],
               (unsigned)gl->tex_info.input_size[1]));
   for (i = 0; i < gl->views.chains.count; i++)
      gl->views.chains.preset[i] = gl;
   views = views
      && gl2_vv_pick_chains(gl, &gl->views.chains, &video_info->views,
            layout, stock, *blend || !layout->offscreen,
            *blend ? layout->canvas_dims : video_info->dims, chains, rects);
   *blend  = views && *blend;
   *direct = views && stock;
   if (*direct && !gl->views.direct_seen)
   {
      gl->views.direct_seen = true;
      RARCH_LOG("[GL] Views drawn straight from the frame.\n");
   }
   if (views && !stock && !gl->views.preset_seen)
   {
      gl->views.preset_seen = true;
      RARCH_LOG("[GL] Views drawn through the preset, each from a "
            "copy.\n");
   }
   if (!views || stock)
      gl2_views_free_copies(gl);
   return views;
}

/* Draws the frame's views as @layout places them into the back
 * buffer, sized @dims, or with blend into @canvas, whose halves one
 * pass then blends into the back buffer: with direct straight from the
 * frame, else each through the preset, one view after another, from
 * its view of software frame @sw written straight in or else a copy of
 * it. */
static void gl2_views_render(gl2_t *gl,
      const video_frame_info_t *video_info,
      const video_views_layout_t *layout, unsigned dims,
      const struct gl2_views_fbo *canvas, void *const *chains,
      const video_views_rect_t *rects, bool blend, bool direct,
      bool fresh, const void *sw, unsigned pitch, bool use_rgba,
      unsigned frame_count)
{
   struct gl2_views_draw d;
   unsigned target_dims               = blend ? layout->canvas_dims : dims;

   /* The window's viewport and projection, which the preset's first
    * framebuffer took (gl2_renderchain_start_render), as its last pass
    * gives them back */
   if (gl->flags & GL2_FLAG_FBO_INITED)
      gl2_set_viewport(gl, gl->video_dims, false, true);

   d.video_info    = video_info;
   d.target        = blend ? canvas->fbo : 0;
   d.target_height = VIDEO_SCALE_H(target_dims);
   d.frame_count   = frame_count;

   if (blend)
      gl2_bind_fb(canvas->fbo);
   else
      gl2_renderchain_bind_backbuffer();
   glDisable(GL_BLEND);
   glDisable(GL_SCISSOR_TEST);
   glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
   glClear(GL_COLOR_BUFFER_BIT);
   glActiveTexture(GL_TEXTURE0);

   if (direct)
   {
      glBindTexture(GL_TEXTURE_2D, gl->tex_info.tex);
      gl2_vv_draw_placements(gl, &d, layout, chains,
            blend || !layout->offscreen, target_dims, true, blend);
   }
   else
   {
      unsigned i, j;
      void *one[RETRO_VIDEO_VIEWS_MAX];
      struct video_tex_info frame = gl->tex_info;
      /* The copies a preset reading earlier frames keeps: the newest
       * and as many as it reads before it */
      unsigned depth = gl->shader->get_prev_textures(gl->shader_data) + 1;
      bool feedback  = (gl->flags & GL2_FLAG_FBO_INITED)
         && (gl->flags & GL2_FLAG_FBO_FEEDBACK_ENABLE);
      for (i = 0; i < video_info->views.num_views; i++)
      {
         if (!chains[i])
            continue;
         if (sw)
            gl2_views_write(gl, i, &video_info->views.views[i], depth,
                  sw, pitch, use_rgba);
         else if (     !(!fresh && gl2_views_kept(gl, i,
                        &video_info->views.views[i], depth))
               && !gl2_views_copy(gl, i, &video_info->views.views[i],
                  depth, fresh))
            continue;
         gl2_views_preset_view(gl, video_info, i, rects[i].dims,
               frame_count);
         for (j = 0; j < RETRO_VIDEO_VIEWS_MAX; j++)
            one[j] = (j == i) ? chains[i] : NULL;
         gl2_vv_draw_placements(gl, &d, layout, one,
               blend || !layout->offscreen, target_dims, false, blend);
         if (depth > 1)
            gl2_views_push_prev(gl, i);
         if (feedback)
            gl2_views_swap_feedback(gl, i);
         gl->tex_info = frame;
      }
      gl->coords.vertex    = gl->vertex_ptr;
      gl->coords.tex_coord = gl->tex_info.coord;
      gl->coords.color     = gl->white_color_ptr;
      gl->coords.vertices  = 4;
   }

   if (blend)
   {
      const struct gl2_views_prog *p =
         (layout->stereo_mode == VIDEO_STEREO_MODE_INTERLACED)
         ? &gl->views.interlaced : &gl->views.anaglyph;
      gl2_renderchain_bind_backbuffer();
      glViewport(0, 0, VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims));
      glBindTexture(GL_TEXTURE_2D, canvas->tex);
      glUseProgram(p->prog);
      if (p->loc_height >= 0)
         glUniform1f(p->loc_height,
               (float)VIDEO_SCALE_H(layout->canvas_dims));
      gl2_views_quad(gl, p, &gl->mvp_no_rot, tex_coords);
   }

   /* The frame's program, as the frame's own draw leaves it */
   gl->shader->use(gl, gl->shader_data, 1, true);
   glBindTexture(GL_TEXTURE_2D, gl->tex_info.tex);
   /* What follows, a non-fullscreen overlay for one, draws in the
    * current viewport. */
   glViewport(VIDEO_POS_X(gl->vp.pos), VIDEO_POS_Y(gl->vp.pos),
         VIDEO_SCALE_W(gl->vp.dims), VIDEO_SCALE_H(gl->vp.dims));
}

/* Moves UI drawing into the UI layer, laid out at one eye's size */
static void gl2_views_ui_begin(gl2_t *gl, video_frame_info_t *video_info)
{
   const video_views_layout_t *layout = &video_info->views_layout;
   /* Only full side by side places it unscaled. */
   GLint filter = (layout->stereo_mode == VIDEO_STEREO_MODE_SBS_FULL)
      ? GL_NEAREST : GL_LINEAR;

   glBindTexture(GL_TEXTURE_2D, gl->views.ui.tex);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
   glBindTexture(GL_TEXTURE_2D, 0);

   gl2_bind_fb(gl->views.ui.fbo);
   glDisable(GL_SCISSOR_TEST);
   glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
   glClear(GL_COLOR_BUFFER_BIT);

   /* Menus and widgets size from video_info->dims, fonts from
    * video_dims. */
   video_views_ui_enter(&gl->views.saved, &video_info->dims,
         &gl->video_dims, layout->ui_dims);
   gl->views.ui_pass = true;
   gl2_set_viewport(gl, video_info->dims, true, true);
}

/* Back onto the back buffer, and the UI layer into each eye's area. It
 * was drawn over transparent black, so its colour is premultiplied. */
static void gl2_views_ui_end(gl2_t *gl, video_frame_info_t *video_info)
{
   unsigned a;
   const video_views_layout_t *layout = &video_info->views_layout;

   gl->views.ui_pass = false;
   video_views_ui_leave(&gl->views.saved, &video_info->dims,
         &gl->video_dims);

   gl2_renderchain_bind_backbuffer();
   glDisable(GL_SCISSOR_TEST);
   glEnable(GL_BLEND);
   glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
   glBlendEquation(GL_FUNC_ADD);
   glBindTexture(GL_TEXTURE_2D, gl->views.ui.tex);
   for (a = 0; a < layout->num_areas; a++)
   {
      GLint vp[4];
      gl2_views_gl_rect(&layout->areas[a],
            VIDEO_SCALE_H(video_info->dims), vp);
      glViewport(vp[0], vp[1], vp[2], vp[3]);
      gl2_views_quad(gl, &gl->views.layer, &gl->mvp_no_rot, tex_coords);
   }
   glUseProgram(0);
   glDisable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glBindTexture(GL_TEXTURE_2D, gl->tex_info.tex);
   /* gl->vp is the window again, for the readbacks after. */
   gl2_set_viewport(gl, video_info->dims, false, true);
}

#ifdef HAVE_VIDEO_WINDOWS
/* Events a frame takes from the context's windows */
#define GL2_WINDOW_EVENTS 8

/* Window @w let go: its GPU objects, then the context's window, and
 * input stops mapping into it */
static void gl2_window_retire(gl2_t *gl, unsigned w)
{
   struct gl2_window *win = &gl->windows.win[w];
   gl2_vv_target_drop(gl, &win->canvas_target, &win->canvas);
   gl->ctx_driver->windows->destroy(gl->ctx_data, &win->native);
   video_driver_publish_window_layout(w, 0, NULL, NULL, 0, 0);
   memset(win, 0, sizeof(*win));
   gl->windows.held.live &= ~(1u << w);
}

static void gl2_windows_retire_all(gl2_t *gl)
{
   unsigned w;
   for (w = 1; w < RETRO_VIDEO_VIEWS_MAX; w++)
      if (gl->windows.held.live & (1u << w))
         gl2_window_retire(gl, w);
}

/* Window @w's screens back in the main window, until the next restore
 * (video_driver_restore_windows()) */
static void gl2_window_attach(gl2_t *gl, const video_windows_frame_t *f,
      unsigned w)
{
   if (f)
      video_driver_attach_windows(f->window_screens[w], f->restore);
   gl->windows.held.failed |= 1u << w;
   if (gl->windows.held.live & (1u << w))
      gl2_window_retire(gl, w);
}

/* Window @w for @f's screens there, on @f's display */
static bool gl2_window_make(gl2_t *gl, const video_windows_frame_t *f,
      unsigned w)
{
   video_window_request_t request;
   struct gl2_window *win = &gl->windows.win[w];

   memset(&request, 0, sizeof(request));
   memset(win, 0, sizeof(*win));
   request.screen     = w;
   request.generation = video_driver_window_generation();
   request.settings   = f->settings[w];
   request.display    = f->display;
   if (     !request.generation
         || !gl->ctx_driver->windows->create(gl->ctx_data, &request,
            &win->native))
      return false;
   gl->windows.held.live       |= 1u << w;
   gl->windows.held.display[w]  = f->display;
   return true;
}

/* The preset's targets made to hold its passes for each of @chains'
 * views at its rectangle in @rects, which a window larger than the main
 * one draws past */
static void gl2_windows_fit_chain(gl2_t *gl,
      const video_frame_info_t *video_info, void *const *chains,
      const video_views_rect_t *rects)
{
   unsigned i;
   gl2_renderchain_data_t *chain = (gl2_renderchain_data_t*)
      gl->renderchain_data;
   if (!(gl->flags & GL2_FLAG_FBO_INITED))
      return;
   for (i = 0; i < video_info->views.num_views; i++)
   {
      const struct retro_video_view *v = &video_info->views.views[i];
      if (!chains[i])
         continue;
      gl2_renderchain_recompute_pass_sizes(gl, chain,
            VIDEO_SCALE_PACK(v->width, v->height), rects[i].dims);
      gl2_renderchain_grow_fbos(gl, chain);
   }
}

/* Window @w's screens drawn as the main window's views were this
 * frame, and shown. Without a surface it can show, it keeps its last
 * image. */
static bool gl2_window_draw(gl2_t *gl,
      const video_frame_info_t *video_info, unsigned w, bool new_frame)
{
   void *chains[RETRO_VIDEO_VIEWS_MAX];
   video_views_rect_t rects[RETRO_VIDEO_VIEWS_MAX];
   bool blend;
   const video_windows_frame_t *f     = video_info->windows;
   const gfx_ctx_window_driver_t *ctx = gl->ctx_driver->windows;
   struct gl2_window *win             = &gl->windows.win[w];
   video_views_layout_t *layout       = &win->layout;
   unsigned dims                      = win->native.dims;
   bool stock                         = !video_info->shader_active
      || !gl->views.preset;

   if (     !win->native.configured || !win->native.presentable
         || !VIDEO_SCALE_W(dims) || !VIDEO_SCALE_H(dims))
      return false;
   video_driver_window_layout_screens(f, f->window_screens[w], dims,
         layout);
   blend = layout->offscreen
      && gl2_vv_target_ensure(gl, &win->canvas_target, &win->canvas,
            layout->canvas_dims);
   if (!layout->offscreen)
      gl2_vv_target_drop(gl, &win->canvas_target, &win->canvas);
   if (!gl2_vv_pick_chains(gl, &gl->views.chains, &video_info->views,
            layout, stock, blend || !layout->offscreen,
            blend ? layout->canvas_dims : dims, chains, rects))
      return false;
   if (!ctx->bind_gl(gl->ctx_data, &win->native))
   {
      ctx->bind_gl(gl->ctx_data, NULL);
      RARCH_WARN("[GL] A screen window can't be drawn. "
            "Its screens go back to the main window.\n");
      gl2_window_attach(gl, f, w);
      return false;
   }
   if (!stock)
      gl2_windows_fit_chain(gl, video_info, chains, rects);
   gl2_views_render(gl, video_info, layout, dims, &win->canvas, chains,
         rects, blend, stock, new_frame, NULL, 0, false,
         gl->windows.frame_count);
   /* False: not shown this time, its last image stays */
   ctx->present_gl(gl->ctx_data, &win->native);
   ctx->bind_gl(gl->ctx_data, NULL);
   video_driver_publish_window_layout(w, win->native.generation, layout,
         &f->core, f->core_dims, dims);
   return true;
}

/* The screens' own windows for this frame: made, let go of, and drawn
 * after the main window's frame and before its swap, which stays the
 * only one that waits; a window swaps at interval 0. Views only: the
 * menu, OSD, widgets and overlay stay in the main window. True when a
 * window was drawn. */
static bool gl2_windows_draw(gl2_t *gl,
      const video_frame_info_t *video_info, bool new_frame)
{
   unsigned w, retire, create;
   unsigned closed                    = 0;
   const video_windows_frame_t *f     = video_info->windows;
   const gfx_ctx_window_driver_t *ctx = gl->ctx_driver->windows;
   bool drawn                         = false;

   if (!ctx)
      return false;
   if (gl->windows.held.live)
   {
      unsigned i, count;
      video_window_event_t events[GL2_WINDOW_EVENTS];
      count = ctx->poll(gl->ctx_data, events, GL2_WINDOW_EVENTS);
      for (i = 0; i < count; i++)
      {
         unsigned id = events[i].screen;
         if (     events[i].kind == VIDEO_WINDOW_EVENT_CLOSE
               && id && id < RETRO_VIDEO_VIEWS_MAX
               && (gl->windows.held.live & (1u << id))
               && gl->windows.win[id].native.generation
                  == events[i].generation)
            closed |= 1u << id;
      }
   }
   video_windows_steps(&gl->windows.held, f, &retire, &create);
   for (w = 1; w < RETRO_VIDEO_VIEWS_MAX; w++)
      if (retire & (1u << w))
         gl2_window_retire(gl, w);
   if (!f)
      return false;
   /* Marked after steps(), which clears the marks when this frame's
    * restore count moved, so a closed window waits for the next one */
   for (w = 1; w < RETRO_VIDEO_VIEWS_MAX; w++)
      if ((closed & (1u << w)) && f->window_screens[w])
         gl2_window_attach(gl, f, w);
   create &= ~closed;
   for (w = 1; w < RETRO_VIDEO_VIEWS_MAX; w++)
   {
      if (!(create & (1u << w)) || gl2_window_make(gl, f, w))
         continue;
      RARCH_WARN("[GL] A screen window can't be made. "
            "Its screens stay in the main window.\n");
      gl2_window_attach(gl, f, w);
   }
   /* Views the main window drew whole are not drawn here either */
   if (!gl->views.active)
      return false;
   for (w = 1; w < RETRO_VIDEO_VIEWS_MAX; w++)
      if (     (gl->windows.held.live & (1u << w))
            && gl2_window_draw(gl, video_info, w, new_frame))
         drawn = true;
   return drawn;
}
#endif
#endif

static bool gl2_frame(void *data, const void *frame,
      unsigned dims,
      uint64_t frame_count,
      unsigned pitch, const char *msg,
      video_frame_info_t *video_info)
{
   unsigned frame_width = VIDEO_SCALE_W(dims);
   unsigned frame_height = VIDEO_SCALE_H(dims);
   video_shader_ctx_params_t params;
   struct video_tex_info feedback_info;
   gl2_t                            *gl = (gl2_t*)data;
   gl2_renderchain_data_t       *chain = NULL;
   unsigned width                      = 0;
   unsigned height                     = 0;
   bool use_rgba                       = (video_info->video_st_flags & VIDEO_FLAG_USE_RGBA) ? true : false;
   bool statistics_show                = video_info->statistics_show;
   bool input_driver_nonblock_state    = video_info->input_driver_nonblock_state;
   bool hard_sync                      = video_info->hard_sync;
   unsigned hard_sync_frames           = video_info->hard_sync_frames;
   struct font_params *osd_params      = (struct font_params*)
      &video_info->osd_stat_params;
   const char *stat_text               = video_info->stat_text;
#ifdef HAVE_MENU
   bool menu_is_alive                  = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
#endif
#ifdef HAVE_GFX_WIDGETS
   bool widgets_active                 = video_info->widgets_active;
#endif
   bool overlay_behind_menu            = video_info->overlay_behind_menu;
   bool video_scale_integer            = video_info->scale_integer;
   bool views_per_eye                  = false;
#ifdef GL2_VIEWS
   void *view_chains[RETRO_VIDEO_VIEWS_MAX];
   video_views_rect_t view_rects[RETRO_VIDEO_VIEWS_MAX];
   bool views                          = false;
   bool views_blend                    = false;
   bool views_direct                   = false;
   /* A software frame put off until the views are known */
   const void *views_sw                = NULL;
   bool views_ui                       = false;
#endif
#if (defined(HAVE_OPENXR) && defined(ANDROID) && defined(HAVE_OPENGLES3)) \
   || defined(HAVE_GL2_XR)
   video_viewport_t xr_vp;
#endif

   if (!gl)
      return false;

#ifdef GL2_HW_RING_SYNC
   /* One a frame without the ring's hand-over left behind */
   if (gl->hw_ring_done_sync)
   {
      glDeleteSync((GLsync)gl->hw_ring_done_sync);
      gl->hw_ring_done_sync = NULL;
   }
#endif

   /* These travel with the frame, so this thread does not read what the
    * main thread writes: the scRGB encode below and gl2_encode_pq_to_sdr()
    * read the latched copies. */
   gl->scrgb.menu_nits        = video_info->hdr_menu_nits;
   gl->scrgb.paper_white_nits = video_info->hdr_paper_white_nits;
   gl->scrgb.expand_gamut     = video_info->hdr_expand_gamut;

   /* Travels with the frame, for gl2_set_aspect_ratio() to read rather
    * than the setting the menu writes */
   if (video_info->ctx_scaling)
      gl->flags |=  GL2_FLAG_CTX_SCALING;
   else
      gl->flags &= ~GL2_FLAG_CTX_SCALING;

   /* Travels with the frame, for set_texture_frame() to read rather
    * than the setting the menu writes */
   if (video_info->menu_linear_filter)
      gl->flags |=  GL2_FLAG_MENU_LINEAR_FILTER;
   else
      gl->flags &= ~GL2_FLAG_MENU_LINEAR_FILTER;

   /* Whether to read frames back travels with the frame, so this thread
    * does not read the recording state the main thread writes. */
   if (video_info->gpu_recording)
      gl->flags |=  GL2_FLAG_GPU_RECORDING;
   else
      gl->flags &= ~GL2_FLAG_GPU_RECORDING;

   /* Resolved only after the guard above: initialising these at
    * declaration dereferenced 'data' before the NULL check ever ran,
    * so a frame call issued with no driver instance (e.g. the
    * cached-frame replay in command_event_reinit) faulted at
    * function entry instead of returning false. gl3_frame already
    * orders it this way. */
   chain  = (gl2_renderchain_data_t*)gl->renderchain_data;
   width  = VIDEO_SCALE_W(gl->video_dims);
   height = VIDEO_SCALE_H(gl->video_dims);

   if (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, false);

#ifndef HAVE_OPENGLES
   if (gl->flags & GL2_FLAG_CORE_CONTEXT_IN_USE)
      glBindVertexArray(chain->vao);
#endif

   gl->shader->use(gl, gl->shader_data, 1, true);

#ifdef HAVE_OPENXR
   if (gl_android_openxr_active())
   {
      gl_android_openxr_begin_frame();
      gl2_bind_fb(gl_android_openxr_get_framebuffer());
   }
#endif

#if TARGET_OS_IPHONE
   /* Apparently the viewport is lost each frame, thanks Apple. */
   gl2_set_viewport(gl, VIDEO_SCALE_PACK(width, height), false, true);
#endif

   if (frame && !(gl->flags & GL2_FLAG_HW_RENDER_FBO_INIT))
      gl2_frame_fit_textures(gl, frame_width, frame_height);

   /* Render to texture in first pass. */
   if (gl->flags & GL2_FLAG_FBO_INITED)
   {
      gl2_renderchain_recompute_pass_sizes(gl, chain, dims,
            gl->out_vp_dims);

      gl2_renderchain_start_render(gl, chain, video_scale_integer);
   }

   if (gl->flags & GL2_FLAG_SHOULD_RESIZE)
   {
      if (gl->ctx_driver->set_resize)
         gl->ctx_driver->set_resize(gl->ctx_data, gl->video_dims);
      gl->flags &= ~GL2_FLAG_SHOULD_RESIZE;

      if (gl->flags & GL2_FLAG_FBO_INITED)
      {
         /* On resize, we might have to recreate our FBOs
          * due to "Viewport" scale, and set a new viewport. */
         gl2_renderchain_grow_fbos(gl, chain);

         /* Go back to what we're supposed to do,
          * render to FBO #0. */
         gl2_renderchain_start_render(gl, chain, video_scale_integer);
      }
      else
         gl2_set_viewport(gl, VIDEO_SCALE_PACK(width, height), false, true);
   }

   if (frame)
      gl->tex_index = ((gl->tex_index + 1) % gl->textures);

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

   /* Can be NULL for frame dupe / NULL render. */
   if (frame)
   {
      if (!(gl->flags & GL2_FLAG_HW_RENDER_FBO_INIT))
      {
         gl2_update_input_size(gl, frame_width, frame_height, pitch, true);

#ifdef GL2_VIEWS
         if (gl2_views_write_wanted(gl, chain, video_info, use_rgba))
            views_sw = frame;
         else
#endif
         gl2_renderchain_copy_frame(gl, chain, use_rgba,
               frame, frame_width, frame_height, pitch);
      }

      /* No point regenerating mipmaps
       * if there are no new frames. */
      if (     (gl->flags & GL2_FLAG_TEXTURE_MIPMAP)
            && (gl->flags & GL2_FLAG_HAVE_MIPMAP))
         glGenerateMipmap(GL_TEXTURE_2D);
   }

   /* scRGB or PQ content: route the whole frame into the offscreen;
    * the encode (end-of-frame under scRGB, pre-UI choke point for the
    * PQ -> SDR fallback) then writes the backbuffer. This early bind
    * covers the direct (no-FBO-chain) draw path; the chain's own
    * back-buffer binds below are redirected the same way. */
   if (gl->scrgb.active || gl2_needs_pq_downconvert(gl))
      gl2_bind_fb(gl2_frame_target_fbo(gl));

   /* Have to reset rendering state which libretro core
    * could easily have overridden. */
   if (gl->flags & GL2_FLAG_HW_RENDER_FBO_INIT)
   {
      gl2_update_input_size(gl, frame_width, frame_height, pitch, false);
      if (!(gl->flags & GL2_FLAG_FBO_INITED))
      {
         if (gl->scrgb.active || gl2_needs_pq_downconvert(gl))
            gl2_bind_fb(gl2_frame_target_fbo(gl));
         else
            gl2_renderchain_bind_backbuffer();
         gl2_set_viewport(gl, VIDEO_SCALE_PACK(width, height), false, true);
      }

      gl2_renderchain_restore_default_state(gl);

      /* GL_SCISSOR_TEST is global context state, so a core that
       * leaves it enabled clips everything we draw afterwards -
       * including the menu and the widgets - to whatever rectangle
       * the core last set. Menu drivers that use
       * gfx_display_scissor_begin()/scissor_end() (Ozone) reset it
       * by accident on their first clipped draw; the ones that never
       * touch scissor (XMB, RGUI) stay clipped indefinitely. */
      glDisable(GL_SCISSOR_TEST);
      glDisable(GL_STENCIL_TEST);
      glDisable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glBlendEquation(GL_FUNC_ADD);
      glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
   }

   gl->tex_info.tex           = gl->texture[gl->tex_index];
   gl->tex_info.input_size[0] = frame_width;
   gl->tex_info.input_size[1] = frame_height;
   gl->tex_info.tex_size[0]   = VIDEO_SCALE_W(gl->tex_dims);
   gl->tex_info.tex_size[1]   = VIDEO_SCALE_H(gl->tex_dims);

   feedback_info              = gl->tex_info;

   if (gl->flags & GL2_FLAG_FBO_FEEDBACK_ENABLE)
   {
      const struct video_fbo_rect
         *rect                        = &gl->fbo_rect[gl->fbo_feedback_pass];
      GLfloat xamt                    = (GLfloat)VIDEO_SCALE_W(rect->img_dims)
            / VIDEO_SCALE_W(rect->dims);
      GLfloat yamt                    = (GLfloat)VIDEO_SCALE_H(rect->img_dims)
            / VIDEO_SCALE_H(rect->dims);

      feedback_info.tex               = gl->fbo_feedback_texture;
      feedback_info.input_size[0]     = VIDEO_SCALE_W(rect->img_dims);
      feedback_info.input_size[1]     = VIDEO_SCALE_H(rect->img_dims);
      feedback_info.tex_size[0]       = VIDEO_SCALE_W(rect->dims);
      feedback_info.tex_size[1]       = VIDEO_SCALE_H(rect->dims);

      SET_TEXTURE_COORDS(feedback_info.coord, xamt, yamt);
   }

#ifdef GL2_VIEWS
   views = gl2_views_setup(gl, video_info, frame, &views_blend,
         &views_direct, view_chains, view_rects);
   /* Views through the preset have theirs written straight in; drawn
    * any other way the frame is written now. */
   if (views_sw && (!views || views_direct))
   {
      glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
      gl2_renderchain_copy_frame(gl, chain, use_rgba,
            views_sw, frame_width, frame_height, pitch);
      views_sw = NULL;
   }
   else if (views_sw && !gl->views.written_seen)
   {
      gl->views.written_seen = true;
      RARCH_LOG("[GL] Views' presets have each view of a software frame "
            "written straight in.\n");
   }
   /* gl->vp is the whole window while views draw. */
   if (views != gl->views.active)
   {
      gl->views.active = views;
      gl2_set_viewport(gl, VIDEO_SCALE_PACK(width, height), false, true);
   }
   retro_atomic_store_release_int(&gl->views.fallback,
         (video_info->views.num_views && !views) ? 1 : 0);
   gl2_views_publish(gl);
   if (views)
      gl2_views_render(gl, video_info, &video_info->views_layout,
            video_info->dims, &gl->views.canvas, view_chains, view_rects,
            views_blend, views_direct, frame != NULL, views_sw, pitch,
            use_rgba, (unsigned)frame_count);
   else
#endif
   {
   glClear(GL_COLOR_BUFFER_BIT);

   params.vp_dims          = gl->out_vp_dims;
   params.dims             = dims;
   params.tex_dims         = gl->tex_dims;
   params.out_dims         = gl->vp.dims;
   params.frame_counter    = (unsigned int)frame_count;
   params.swap_counter    = (unsigned int)video_info->swap_count;
   params.info             = &gl->tex_info;
   params.prev_info        = gl->prev_info;
   params.feedback_info    = &feedback_info;
   params.fbo_info         = NULL;
   params.fbo_info_cnt     = 0;

   gl->shader->set_params(&params, gl->shader_data);

   gl->coords.vertices  = 4;

   gl->shader->set_coords(gl->shader_data, &gl->coords);
   gl->shader->set_mvp(gl->shader_data, &gl->mvp);

   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   if (gl->flags & GL2_FLAG_FBO_INITED)
      gl2_renderchain_render(gl,
            chain,
            frame_count, &gl->tex_info, &feedback_info,
            video_scale_integer);
   }

#if (defined(HAVE_OPENXR) && defined(ANDROID) && defined(HAVE_OPENGLES3)) \
   || defined(HAVE_GL2_XR)
   /* Where the frame is, before the UI sets viewports of its own. */
   gl2_viewport_info(gl, &xr_vp);
#endif

#ifdef __EMSCRIPTEN__
   /* Workaround for a chromium-specific bug */
   glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
   glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
   glClear(GL_COLOR_BUFFER_BIT);
   glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
#endif

   /* Set prev textures. */
   gl2_renderchain_bind_prev_texture(gl,
         chain, &gl->tex_info);

   /* scRGB: re-assert the frame target before UI draws (chain
    * internals may have restored FBO 0), routing them into the
    * separate UI layer when the content is PQ. On an SDR output with
    * PQ content, convert here instead so the UI below draws over a
    * displayable image. Same choke-point pattern as glcore. */
   if (gl->scrgb.active)
      gl2_bind_fb(gl2_ui_target_fbo(gl));
   else
   {
      if (gl2_needs_pq_downconvert(gl))
         gl2_encode_pq_to_sdr(gl);
      /* Record Game Only: read back before any UI is drawn. Under
       * scRGB the image only lands at the final composite. */
      if (video_info->record_game_only)
         gl2_record_readback(gl, false);
   }

#ifdef GL2_VIEWS
   /* As the main thread laid the UI out, also in a frame drawn whole:
    * it lays out later frames whole once it reads the fallback. */
   views_per_eye = video_views_ui_per_eye(&video_info->views,
         &video_info->views_layout, gl2_views_allowed(gl));
   /* A real frame without it frees the UI layer. */
   if (frame && !views_per_eye)
      gl2_vv_target_drop(gl, &gl->views.ui_target, &gl->views.ui);
   /* Drawn and composited only when there is UI; overlays are hidden. */
   views_ui = VIDEO_VIEWS_UI_WANTED(views_per_eye,
         (gl->flags & GL2_FLAG_MENU_TEXTURE_ENABLE), msg, statistics_show,
         video_info->widgets_visible)
      && gl2_vv_target_ensure(gl, &gl->views.ui_target, &gl->views.ui,
            video_info->views_layout.ui_dims);
#endif

   /* Touch overlays would straddle both eyes. */
#ifdef HAVE_OVERLAY
   if (     (gl->flags & GL2_FLAG_OVERLAY_ENABLE) && overlay_behind_menu
         && !views_per_eye)
      gl2_render_overlay(gl);
#endif

#ifdef GL2_VIEWS
   if (views_ui)
      gl2_views_ui_begin(gl, video_info);
#endif

#if defined(HAVE_MENU)
   if (gl->flags & GL2_FLAG_MENU_TEXTURE_ENABLE)
   {
      menu_driver_frame(menu_is_alive, video_info);

      if (gl->menu_texture)
         gl2_draw_texture(gl);
   }
   else if (statistics_show)
   {
      if (osd_params)
         font_driver_render_msg(gl, stat_text, video_info->stat_text_len,
               (const struct font_params*)osd_params, NULL);
   }
#endif

#ifdef HAVE_OVERLAY
   if (     (gl->flags & GL2_FLAG_OVERLAY_ENABLE) && !overlay_behind_menu
         && !views_per_eye)
      gl2_render_overlay(gl);
#endif

#ifdef HAVE_GFX_WIDGETS
   if (widgets_active)
      gfx_widgets_frame(video_info);
#endif

   if (msg && *msg)
      font_driver_render_msg(gl, msg, strlen(msg), NULL, NULL);

#ifdef GL2_VIEWS
   if (views_ui)
      gl2_views_ui_end(gl, video_info);
#endif

   if (gl->ctx_driver->update_window_title)
      gl->ctx_driver->update_window_title(gl->ctx_data);

   /* Reset state which could easily mess up libretro core. */
   if (gl->flags & GL2_FLAG_HW_RENDER_FBO_INIT)
   {
      gl->shader->use(gl, gl->shader_data, 0, true);
      glBindTexture(GL_TEXTURE_2D, 0);
   }

   /* scRGB: encode the SDR offscreen into the FP16 backbuffer.
    * Runs before the read-back blocks below: SDR capture reads the
    * offscreen (roundtrip-free), the native HDR capture reads the
    * encoded backbuffer. Menu HDR Brightness semantics match the
    * other five HDR paths: menu_nits when any UI is composited this
    * frame, paper white otherwise. */
   if (gl->scrgb.active && gl->scrgb.fbo && gl->scrgb.program)
   {
      float nits           = 200.0f;
      bool ui_visible      = false;
      bool pq              = gl->video_info.source_hdr10
                          && gl->scrgb.ui_fbo != 0;
      static const float quad_pos[8] = {
         0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f
      };
      static const float quad_tex[8] = {
         0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f
      };

#ifdef HAVE_MENU
      if (gl->flags & GL2_FLAG_MENU_TEXTURE_ENABLE)
         ui_visible = true;
#endif
#ifdef HAVE_OVERLAY
      if (gl->flags & GL2_FLAG_OVERLAY_ENABLE)
         ui_visible = true;
#endif
      if ((msg && *msg) || statistics_show)
         ui_visible = true;
      if (video_info->widgets_visible)
         ui_visible = true;

      /* SDR content keeps the existing behaviour, including scaling
       * the whole frame by Menu HDR Brightness when any UI is up. PQ
       * content carries its own absolute luminance - paper white does
       * not apply to it - and the UI is composited separately at the
       * menu setting instead, which is what that setting means on the
       * other HDR paths. */
      nits = (!pq && ui_visible)
            ? gl->scrgb.menu_nits
            : gl->scrgb.paper_white_nits;

      gl2_bind_fb(0);
      glViewport(0, 0,
         VIDEO_SCALE_W(gl->video_dims),
         VIDEO_SCALE_H(gl->video_dims));
      glDisable(GL_BLEND);
      glUseProgram(gl->scrgb.program);
      if (gl->scrgb.loc_tex >= 0)
         glUniform1i(gl->scrgb.loc_tex, 0);
      if (gl->scrgb.loc_ui_tex >= 0)
         glUniform1i(gl->scrgb.loc_ui_tex, 1);
      if (gl->scrgb.loc_nits >= 0)
         glUniform1f(gl->scrgb.loc_nits, nits);
      if (gl->scrgb.loc_expand >= 0)
         glUniform1f(gl->scrgb.loc_expand,
               (float)gl->scrgb.expand_gamut);
      if (gl->scrgb.loc_mode >= 0)
         glUniform1f(gl->scrgb.loc_mode, pq ? 1.0f : 0.0f);
      if (gl->scrgb.loc_ui_nits >= 0)
         glUniform1f(gl->scrgb.loc_ui_nits,
               pq ? gl->scrgb.menu_nits : 0.0f);
      if (gl->scrgb.loc_out_pq >= 0)
         glUniform1f(gl->scrgb.loc_out_pq, gl->scrgb.pq_out ? 1.0f : 0.0f);

      /* Unit 1 must hold something valid even when the shader will
       * not sample it (uUINits == 0): a stale binding on the unit is
       * undefined sampling on some drivers. */
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, pq ? gl->scrgb.ui_tex : gl->scrgb.tex);
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, gl->scrgb.tex);

      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glEnableVertexAttribArray(0);
      glEnableVertexAttribArray(1);
      glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, quad_pos);
      glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, quad_tex);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      glDisableVertexAttribArray(0);
      glDisableVertexAttribArray(1);
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, 0);
      glActiveTexture(GL_TEXTURE0);
      glUseProgram(0);

      /* Restore the aspect-correct viewport the composite just
       * clobbered. glViewport is context state, not per-FBO, and this
       * driver only re-establishes it on resize (and, for hardware
       * cores, in the HW_RENDER_FBO_INIT block above) -- so without
       * this the next frame draws into the offscreen with the full
       * window viewport still latched and the image is stretched to
       * fill, ignoring the aspect ratio. Same save/restore convention
       * as the fullscreen branch of gl2_draw_texture.
       *
       * The glcore driver has the same shape here but is unaffected:
       * its filter chain re-runs glViewport on the final pass every
       * frame (shader_gl3.c), so the leak never survives to a draw.
       * The gl2 GLSL path has no equivalent choke point. */
      glViewport(VIDEO_POS_X(gl->vp.pos), VIDEO_POS_Y(gl->vp.pos), VIDEO_SCALE_W(gl->vp.dims), VIDEO_SCALE_H(gl->vp.dims));
   }

   /* Screenshots. */
   if (gl->readback_buffer_screenshot)
      gl2_renderchain_readback(gl,
            chain,
            4, GL_RGBA, GL_UNSIGNED_BYTE,
            gl->readback_buffer_screenshot);

   else if (!video_info->record_game_only || gl->scrgb.active)
      gl2_record_readback(gl, !video_info->record_game_only);

   /* The backbuffer is what it is until the swap, so the copy is
    * taken now. Not from the BFI light dupes, which recurse in here
    * with the dupe lock held and would only copy the same image. */
   if (     video_info->retain_output
         && !(gl->flags & GL2_FLAG_FRAME_DUPE_LOCK))
   {
      gl2_retain_backbuffer(gl);
      gl->retained_light = 1;
      gl->retained_dark  = 0;
   }

#if defined(HAVE_OPENXR) && defined(ANDROID) && defined(HAVE_OPENGLES3)
   gl_android_openxr_recenter(video_info->headset_recenter);
   gl_android_openxr_set_placement(video_info->headset_placement,
         video_info->headset_distance, video_info->headset_width);
   gl_android_openxr_set_pacing(video_info->input_driver_nonblock_state
         ? 0 : video_info->headset_interval, video_info->headset_request_hz);
   gl_android_openxr_set_views(&video_info->views, dims, &xr_vp,
         gl->rotation / 90, video_info->screen_layout,
         video_info->stereo_swap_eyes, video_info->headset_screens,
         video_info->headset_edit_seq, video_info->headset_editing);
   if (gl_android_openxr_active())
   {
      bool new_frame = frame && !video_info->frame_repeat;
      /* Views kept across a reinit drew only the frame a repeat shows. */
      if (new_frame)
         gl_android_openxr_new_frame();
      else
         gl_android_openxr_place_kept();
      /* A dupe's image was drawn from older poses than the core's last:
       * a repeat that carries a software frame draws its eyes only while
       * none are shown, with the views that frame was drawn from. */
      if (new_frame || (frame && gl_android_openxr_repeat_eyes()))
         gl2_xr_draw_eyes(gl, video_info);
   }
#endif
#ifdef HAVE_GL2_XR
   /* The back buffer is what the window shows until the swap. Not from
    * the BFI light dupes, which would only copy the same image. */
   if (gl->desktop_xr && !(gl->flags & GL2_FLAG_FRAME_DUPE_LOCK))
      gl2_desktop_xr_frame(gl, video_info, frame != NULL, dims, &xr_vp);
#endif

#if defined(GL2_VIEWS) && defined(HAVE_VIDEO_WINDOWS)
   /* Before the ring's fence: the windows read the frame too. Not from
    * the BFI light dupes. */
   if (     (video_info->windows || gl->windows.held.live
            || gl->windows.held.failed)
         && !(gl->flags & GL2_FLAG_FRAME_DUPE_LOCK))
   {
      gl->windows.frame_count = (unsigned)frame_count;
      if (     gl2_windows_draw(gl, video_info, frame != NULL)
            && (gl->flags & GL2_FLAG_HW_RENDER_FBO_INIT))
      {
         /* As the frame leaves it for the core */
         gl->shader->use(gl, gl->shader_data, 0, true);
         glBindTexture(GL_TEXTURE_2D, 0);
      }
   }
#endif

    gl2_hw_ring_drawn(gl);
    gl2_present_wait = 0;
    if (gl->ctx_driver->swap_buffers)
    {
       /* timed: see gl2_get_last_present_wait() */
       retro_time_t swap_at = cpu_features_get_time_usec();
       gl->ctx_driver->swap_buffers(gl->ctx_data);
       gl2_present_wait = cpu_features_get_time_usec() - swap_at;
    }

 /* Emscripten has to do black frame insertion in its main loop */
#ifndef __EMSCRIPTEN__
   /* Disable BFI during fast forward, slow-motion,
    * and pause to prevent flicker. */
   if (
         video_info->black_frame_insertion
         && !video_info->input_driver_nonblock_state
         && !video_info->runloop_is_slowmotion
         && !video_info->runloop_is_paused
#ifdef HAVE_OPENXR
         /* Each would end another headset frame. */
         && !gl_android_openxr_active()
#endif
         && !(gl->flags & GL2_FLAG_MENU_TEXTURE_ENABLE))
   {
      unsigned n;
      int bfi_light_frames;

      if (video_info->bfi_dark_frames > video_info->black_frame_insertion)
      video_info->bfi_dark_frames = video_info->black_frame_insertion;

      /* BFI now handles variable strobe strength, like on-off-off, vs on-on-off for 180hz.
         This needs to be done with duping frames instead of increased swap intervals for
         a couple reasons. Swap interval caps out at 4 in most all apis as of coding,
         and seems to be flat ignored >1 at least in modern Windows for some older APIs. */
      bfi_light_frames = video_info->black_frame_insertion - video_info->bfi_dark_frames;
      if (bfi_light_frames > 0 && !(gl->flags & GL2_FLAG_FRAME_DUPE_LOCK))
      {
         gl->flags |= GL2_FLAG_FRAME_DUPE_LOCK;

         while (bfi_light_frames > 0)
         {
            if (!(gl2_frame(gl, NULL, 0, frame_count, 0, msg, video_info)))
            {
               gl->flags &= ~GL2_FLAG_FRAME_DUPE_LOCK;
               return false;
            }
            --bfi_light_frames;
         }
         gl->flags &= ~GL2_FLAG_FRAME_DUPE_LOCK;
      }

      for (n = 0; n < video_info->bfi_dark_frames; ++n)
      {
         if (!(gl->flags & GL2_FLAG_FRAME_DUPE_LOCK))
         {
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            if (gl->ctx_driver->swap_buffers)
               gl->ctx_driver->swap_buffers(gl->ctx_data);
         }
      }

      /* The group this frame made, for present_last() to replay. */
      if (     video_info->retain_output
            && !(gl->flags & GL2_FLAG_FRAME_DUPE_LOCK))
      {
         gl->retained_light = 1 + (video_info->black_frame_insertion
               - video_info->bfi_dark_frames);
         gl->retained_dark  = video_info->bfi_dark_frames;
      }
   }
#endif

   /* check if we are fast forwarding or in menu,
    * if we are ignore hard sync */
   if (     (gl->flags & GL2_FLAG_HAVE_SYNC)
         &&  hard_sync
         &&  !input_driver_nonblock_state
         )
   {
      glClear(GL_COLOR_BUFFER_BIT);

      gl2_renderchain_fence_iterate(gl, chain,
            hard_sync_frames);
   }

#ifdef HAVE_GL2_XR
   if (gl->desktop_xr)
      gl_openxr_host_poll(gl->desktop_xr->host);
   /* In place of the window's vsync: once a core frame, never under
    * fast-forward. */
   if (     gl->desktop_xr && gl->desktop_xr->paced
         && !(gl->flags & GL2_FLAG_FRAME_DUPE_LOCK))
   {
      if (input_driver_nonblock_state)
         gl_openxr_pace_skip(gl->desktop_xr->host);
      else
         gl_openxr_pace_wait(gl->desktop_xr->host);
   }
#endif

#ifndef HAVE_OPENGLES
   if (gl->flags & GL2_FLAG_CORE_CONTEXT_IN_USE)
      glBindVertexArray(0);
#endif
   /* Not under the ring: the core's context is current on the main
    * thread, and this one has no business taking it. */
   if (     (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
         && !gl2_core_context_is_mains(gl))
      gl->ctx_driver->bind_hw_render(gl->ctx_data, true);
   return true;
}

static void gl2_destroy_resources(gl2_t *gl)
{
   if (gl)
   {
      if (gl->empty_buf)
         free(gl->empty_buf);
      if (gl->conv_buffer)
         free(gl->conv_buffer);
      free(gl);
   }

   gl_query_core_context_unset();
}

static void gl2_scrgb_deinit(gl2_t *gl)
{
   if (gl->scrgb.program)
   {
      glDeleteProgram(gl->scrgb.program);
      gl->scrgb.program = 0;
   }
   if (gl->scrgb.lin_program)
   {
      glDeleteProgram(gl->scrgb.lin_program);
      gl->scrgb.lin_program = 0;
   }
   gl->scrgb.lin_tried = false;
   gl->scrgb.tex_fp16  = false;
   gl->scrgb.fp16_made = false;
   if (gl->scrgb.fbo)
   {
      gl2_delete_fb(1, &gl->scrgb.fbo);
      gl->scrgb.fbo = 0;
   }
   if (gl->scrgb.tex)
   {
      glDeleteTextures(1, &gl->scrgb.tex);
      gl->scrgb.tex = 0;
   }
   if (gl->scrgb.ui_fbo)
   {
      gl2_delete_fb(1, &gl->scrgb.ui_fbo);
      gl->scrgb.ui_fbo = 0;
   }
   if (gl->scrgb.ui_tex)
   {
      glDeleteTextures(1, &gl->scrgb.ui_tex);
      gl->scrgb.ui_tex = 0;
   }
   gl->scrgb.active = false;
}

static void gl2_deinit_chain(gl2_t *gl)
{
   if (!gl)
      return;

   if (gl->renderchain_data)
      free(gl->renderchain_data);
   gl->renderchain_data   = NULL;
}

static void gl2_free(void *data)
{
   gl2_t *gl = (gl2_t*)data;
   if (!gl)
      return;

   if (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, false);
#ifdef HAVE_GL2_XR
   gl2_desktop_xr_free(gl);
#endif

   if (gl->flags & GL2_FLAG_HAVE_SYNC)
      gl2_renderchain_fence_free(gl,
            (gl2_renderchain_data_t*)
            gl->renderchain_data);


   gl->shader->deinit(gl->shader_data);

   gl2_scrgb_deinit(gl);
#ifdef GL2_VIEWS
#ifdef HAVE_VIDEO_WINDOWS
   /* Before the context, which sweeps what is left */
   gl2_windows_retire_all(gl);
#endif
   gl2_views_release(gl);
#endif
#ifdef HAVE_GL_PLANAR
   gl_planar_free(&gl->planar);
#endif
#if !defined(HAVE_OPENGLES) && !defined(HAVE_PSGL)
   gl2_fp16_forget_all();
#endif
#ifdef HAVE_GL_TEXTURE_LEND
   gl_texture_lend_free(&gl->lend);
#endif
#ifdef HAVE_GL_FRAME_MEM
   gl_sw_fb_free(gl->frame_mem, &gl->sw_fb);
   gl_frame_mem_destroy(&gl->frame_mem);
#endif
#ifdef GL2_KMS_FRAMES
   gl2_kms_frames_drop(gl);
#endif

   glDeleteTextures(gl->textures, gl->texture);
#if defined(HAVE_OPENXR) && defined(ANDROID) && defined(HAVE_OPENGLES3)
   if (gl->xr_fbo)
      glDeleteFramebuffers(1, &gl->xr_fbo);
#endif

#if defined(HAVE_MENU)
   if (gl->menu_texture)
      glDeleteTextures(1, &gl->menu_texture);
#endif
   if (gl->white_texture)
      glDeleteTextures(1, &gl->white_texture);
   if (gl->retained_texture)
      glDeleteTextures(1, &gl->retained_texture);

#ifdef HAVE_OVERLAY
   gl2_free_overlay(gl);
#endif
#ifdef HAVE_GLSL
   gl2_free_meshes(gl);
#endif

#if defined(HAVE_PSGL)
   glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, 0);
   glDeleteBuffers(1, &gl->pbo);
#endif

   scaler_ctx_gen_reset(&gl->scaler);

   if (gl->flags & GL2_FLAG_PBO_READBACK_ENABLE)
   {
      gl2_free_pbo_readback(gl);
   }

#ifndef HAVE_OPENGLES
   if (gl->flags & GL2_FLAG_CORE_CONTEXT_IN_USE)
   {
      gl2_renderchain_data_t *chain = (gl2_renderchain_data_t*)
         gl->renderchain_data;

      glBindVertexArray(0);
      glDeleteVertexArrays(1, &chain->vao);
   }
#endif

   gl2_renderchain_deinit_fbo(gl, (gl2_renderchain_data_t*)gl->renderchain_data);
   gl2_renderchain_deinit_hw_render(gl, (gl2_renderchain_data_t*)gl->renderchain_data);
   gl2_deinit_chain(gl);

   if (gl->ctx_driver && gl->ctx_driver->destroy)
      gl->ctx_driver->destroy(gl->ctx_data);
   video_context_driver_free();

   gl2_destroy_resources(gl);
}

static void gl2_set_nonblock_state(
      void *data, bool state,
      bool adaptive_vsync_enabled,
      unsigned swap_interval)
{
   gl2_t             *gl        = (gl2_t*)data;

   if (!gl)
      return;

   if (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, false);

   if (gl->ctx_driver->swap_interval)
   {
      int interval              = 0;
      if (!state)
         interval               = swap_interval;
      if (interval == 1 && adaptive_vsync_enabled)
         interval = -1;
#ifdef HAVE_GL2_XR
      /* Kept for when the headset stops pacing. */
      if (gl->desktop_xr)
      {
         gl->desktop_xr->window_interval = interval;
         if (gl->desktop_xr->paced)
            interval = 0;
      }
#endif
      gl->ctx_driver->swap_interval(gl->ctx_data, interval);
      gl2_present_vsynced = (interval != 0);
   }

   if (     (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
         && !gl2_core_context_is_mains(gl))
      gl->ctx_driver->bind_hw_render(gl->ctx_data, true);
}

static bool gl2_resolve_extensions(gl2_t *gl, const char *context_ident, const video_info_t *video)
{
   /* GL2_FLAG_HAVE_ES2_COMPAT  - GL_RGB565 internal format support.
    *                             Even though ES2 support is claimed, the format
    *                             is not supported on older ATI catalyst drivers.
    *
    *                             The speed gain from using GL_RGB565 is worth
    *                             adding some workarounds for.
    *
    * GL2_FLAG_HAVE_SYNC         - Use ARB_sync to reduce latency.
    */
   if (gl_check_capability(GL_CAPS_MIPMAP))
      gl->flags                 |=  GL2_FLAG_HAVE_MIPMAP;
   if (gl_check_capability(GL_CAPS_ES2_COMPAT))
      gl->flags                 |=  GL2_FLAG_HAVE_ES2_COMPAT;
   else
      gl->flags                 &= ~GL2_FLAG_HAVE_ES2_COMPAT;

   if (gl_check_capability(GL_CAPS_UNPACK_ROW_LENGTH))
      gl->flags                 |=  GL2_FLAG_HAVE_UNPACK_ROW_LENGTH;
   else
      gl->flags                 &= ~GL2_FLAG_HAVE_UNPACK_ROW_LENGTH;

   if (gl_check_capability(GL_CAPS_SYNC))
   {
      settings_t *settings       = config_get_ptr();
      bool video_hard_sync       = settings->bools.video_hard_sync;

      gl->flags                 |=  GL2_FLAG_HAVE_SYNC;
      gl->hw_ring_have_sync      = true;
      if (video_hard_sync)
         RARCH_LOG("[GL] Using ARB_sync to reduce latency.\n");
   }
   else
      gl->flags                 &= ~GL2_FLAG_HAVE_SYNC;

   video_driver_modify_disp_flags(0, VIDEO_FLAG_USE_RGBA);

   gl2_renderchain_resolve_extensions(gl,
         (gl2_renderchain_data_t*)gl->renderchain_data,
         context_ident, video);

#if defined(HAVE_OPENGLES) && !defined(HAVE_PSGL)
   if (!gl_check_capability(GL_CAPS_BGRA8888))
   {
      video_driver_modify_disp_flags(VIDEO_FLAG_USE_RGBA, 0);
      RARCH_WARN("[GL] GLES implementation does not have BGRA8888 extension.\n"
                 "[GL] 32-bit path will require conversion.\n");
   }
#endif

#ifdef GL_DEBUG
   /* Useful for debugging, but kinda obnoxious otherwise. */
   RARCH_LOG("[GL] Supported extensions:\n");

   if (gl->flags & GL2_FLAG_CORE_CONTEXT_IN_USE)
   {
#ifdef GL_NUM_EXTENSIONS
      GLint i;
      GLint exts = 0;
      glGetIntegerv(GL_NUM_EXTENSIONS, &exts);
      for (i = 0; i < exts; i++)
      {
         const char *ext = (const char*)glGetStringi(GL_EXTENSIONS, i);
         if (ext)
            RARCH_LOG("\t%s\n", ext);
      }
#endif
   }
   else
   {
      const char *ext = (const char*)glGetString(GL_EXTENSIONS);

      if (ext)
      {
         size_t i;
         struct string_list  list = {0};

         string_list_initialize(&list);
         string_split_noalloc(&list, ext, " ");

         for (i = 0; i < list.size; i++)
            RARCH_LOG("\t%s\n", list.elems[i].data);
         string_list_deinitialize(&list);
      }
   }
#endif

   return true;
}

static INLINE void gl2_set_texture_fmts(gl2_t *gl, bool rgb32)
{
   gl->internal_fmt       = RARCH_GL_INTERNAL_FORMAT16;
   gl->texture_type       = RARCH_GL_TEXTURE_TYPE16;
   gl->texture_fmt        = RARCH_GL_FORMAT16;
   gl->base_size          = sizeof(uint16_t);

   if (rgb32)
   {
      bool use_rgba       = (video_driver_get_disp_flags() & VIDEO_FLAG_USE_RGBA);

      gl->internal_fmt    = RARCH_GL_INTERNAL_FORMAT32;
      gl->texture_type    = RARCH_GL_TEXTURE_TYPE32;
      gl->texture_fmt     = RARCH_GL_FORMAT32;
      gl->base_size       = sizeof(uint32_t);

      if (use_rgba)
      {
         gl->internal_fmt = GL_RGBA;
         gl->texture_type = GL_RGBA;
      }
#if !defined(HAVE_OPENGLES) && !defined(HAVE_PSGL)
      /* Native 10-bit sources (XRGB2101010 / HDR10_2101010) arrive as
       * packed 2-10-10-10 words. GL_BGRA + UNSIGNED_INT_2_10_10_10_REV
       * reads A from bits 31:30 and R from 29:20 - exactly the
       * A2R10G10B10 layout of both formats - so unlike the glcore
       * driver (GLES-shared, RGBA order + view swizzle) no swizzle is
       * needed, which is fortunate as GL_TEXTURE_SWIZZLE_* is 3.3+.
       * This one site also widens the hardware render target: the HW
       * FBOs attach gl->texture[i] as their colour buffer, and PQ
       * narrowed to 8 bits bands heavily in the darks. Same 32 bits
       * per pixel, so no cost and no option. */
      if (gl->video_info.source_10bit)
      {
         gl->internal_fmt = GL_RGB10_A2;
         gl->texture_type = GL_BGRA;
         gl->texture_fmt  = GL_UNSIGNED_INT_2_10_10_10_REV;
      }
#endif
   }
#ifndef HAVE_OPENGLES
   else if (gl->flags & GL2_FLAG_HAVE_ES2_COMPAT)
   {
      RARCH_LOG("[GL] Using GL_RGB565 for texture uploads.\n");
      gl->internal_fmt    = RARCH_GL_INTERNAL_FORMAT16_565;
      gl->texture_type    = RARCH_GL_TEXTURE_TYPE16_565;
      gl->texture_fmt     = RARCH_GL_FORMAT16_565;
   }
#endif
}

static void gl2_pbo_readback_free_syncs(gl2_t *gl)
{
#ifdef GL2_HW_RING_SYNC
   int i;
   for (i = 0; i < 4; i++)
   {
      if (gl->pbo_readback_sync[i])
         glDeleteSync((GLsync)gl->pbo_readback_sync[i]);
      gl->pbo_readback_sync[i] = NULL;
   }
#endif
}

static void gl2_free_pbo_readback(gl2_t *gl)
{
   gl2_pbo_readback_free_syncs(gl);
   glDeleteBuffers(4, gl->pbo_readback);
   scaler_ctx_gen_reset(&gl->pbo_readback_scaler);
}

static bool gl2_init_pbo_readback(gl2_t *gl)
{
#if !defined(HAVE_OPENGLES2) && !defined(HAVE_PSGL)
   int i;

   gl->pbo_readback_index = 0;
   memset(gl->pbo_readback_valid, 0, sizeof(gl->pbo_readback_valid));
   gl2_pbo_readback_free_syncs(gl);
   glGenBuffers(4, gl->pbo_readback);

   for (i = 0; i < 4; i++)
   {
      gl2_renderchain_bind_pbo(gl->pbo_readback[i]);
      gl2_renderchain_init_pbo(VIDEO_SCALE_AREA(gl->vp.dims) * sizeof(uint32_t), NULL);
   }
   gl2_renderchain_unbind_pbo();

   gl->pbo_readback_scaler.in_width  = VIDEO_SCALE_W(gl->vp.dims);
   gl->pbo_readback_scaler.in_height = VIDEO_SCALE_H(gl->vp.dims);

#ifndef HAVE_OPENGLES3
   {
      struct scaler_ctx *scaler = &gl->pbo_readback_scaler;
      scaler->in_width          = VIDEO_SCALE_W(gl->vp.dims);
      scaler->in_height         = VIDEO_SCALE_H(gl->vp.dims);
      scaler->out_width         = VIDEO_SCALE_W(gl->vp.dims);
      scaler->out_height        = VIDEO_SCALE_H(gl->vp.dims);
      scaler->in_stride         = VIDEO_SCALE_W(gl->vp.dims) * sizeof(uint32_t);
      scaler->out_stride        = VIDEO_SCALE_W(gl->vp.dims) * 3;
      scaler->in_fmt            = SCALER_FMT_ARGB8888;
      scaler->out_fmt           = SCALER_FMT_BGR24;
      scaler->scaler_type       = SCALER_TYPE_POINT;

      if (!scaler_ctx_gen_filter(scaler))
      {
         gl->flags             &= ~GL2_FLAG_PBO_READBACK_ENABLE;
         RARCH_ERR("[GL] Failed to initialize pixel conversion for PBO.\n");
         glDeleteBuffers(4, gl->pbo_readback);
         return false;
      }
   }
#endif

   return true;
#else
   /* If none of these are bound, we have to assume
    * we are not going to use PBOs */
   return false;
#endif
}

static const gfx_ctx_driver_t *gl2_get_context(gl2_t *gl)
{
   const gfx_ctx_driver_t *gfx_ctx      = NULL;
   void                      *ctx_data  = NULL;
   settings_t                 *settings = config_get_ptr();
   struct retro_hw_render_callback *hwr = video_driver_get_hw_context();
   unsigned major                       = hwr->version_major;
   unsigned minor                       = hwr->version_minor;
   bool video_shared_context            = settings->bools.video_shared_context;
#ifdef HAVE_OPENGLES
   enum gfx_ctx_api api                 = GFX_CTX_OPENGL_ES_API;
   switch(hwr->context_type)
   {
      case RETRO_HW_CONTEXT_OPENGLES3:
         major = 3;
         minor = 0;
         break;
      case RETRO_HW_CONTEXT_OPENGLES_VERSION:
         /* Passthrough version_major / version_minor unchanged. */
         break;
      default:
         major = 2;
         minor = 0;
   }
#else
   enum gfx_ctx_api api                 = GFX_CTX_OPENGL_API;
#endif
   if (video_shared_context
      && (hwr->context_type != RETRO_HW_CONTEXT_NONE))
      gl->flags                        |=  GL2_FLAG_SHARED_CONTEXT_USE;
   else
      gl->flags                        &= ~GL2_FLAG_SHARED_CONTEXT_USE;

   if (     (runloop_get_flags() & RUNLOOP_FLAG_CORE_SET_SHARED_CONTEXT)
         && (hwr->context_type != RETRO_HW_CONTEXT_NONE))
      gl->flags                        |=  GL2_FLAG_SHARED_CONTEXT_USE;

#ifdef HAVE_THREADS
   /* Under the threaded wrapper a hardware core renders on the main
    * thread while this driver draws on the video thread, so the core
    * cannot borrow this driver's context the way it does unthreaded:
    * it needs one of its own, shared with this one. That is the
    * shared context, and gl2_hw_ring_context_new() refuses without
    * it. It was only ever made when the setting or the core asked,
    * so a core that did neither - the ffmpeg core is one - was sent
    * into context_reset with no context at all: every GL function it
    * looked up came back NULL on WGL, and it called the first one.
    * glcore has always forced it for hardware cores. */
   if (     (hwr->context_type != RETRO_HW_CONTEXT_NONE)
         && video_driver_thread_wrapper_active()
         && video_thread_hw_allowed())
      gl->flags                        |=  GL2_FLAG_SHARED_CONTEXT_USE;
#endif

   gfx_ctx = video_context_driver_init_first(gl,
         settings->arrays.video_context_driver,
         api, major, minor,
         (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE) ? true : false,
         &ctx_data);

   if (ctx_data)
      gl->ctx_data = ctx_data;

   /* A context that cannot share (one GL context per process) made
    * none: the ring hands this one between the threads, and this
    * driver must not bind a shared context it does not have. */
   if (     gfx_ctx
         && (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
         && gfx_ctx->get_flags
         && BIT32_GET(gfx_ctx->get_flags(ctx_data), GFX_CTX_FLAGS_NO_SHARED_CONTEXT))
   {
      gl->flags &= ~GL2_FLAG_SHARED_CONTEXT_USE;
      RARCH_LOG("[GL] No shared context on \"%s\": the core shares this driver's.\n",
            gfx_ctx->ident);
   }

   return gfx_ctx;
}

#ifdef GL_DEBUG
#ifdef HAVE_GL_DEBUG_ES
#define DEBUG_CALLBACK_TYPE GL_APIENTRY

#define GL_DEBUG_SOURCE_API GL_DEBUG_SOURCE_API_KHR
#define GL_DEBUG_SOURCE_WINDOW_SYSTEM GL_DEBUG_SOURCE_WINDOW_SYSTEM_KHR
#define GL_DEBUG_SOURCE_SHADER_COMPILER GL_DEBUG_SOURCE_SHADER_COMPILER_KHR
#define GL_DEBUG_SOURCE_THIRD_PARTY GL_DEBUG_SOURCE_THIRD_PARTY_KHR
#define GL_DEBUG_SOURCE_APPLICATION GL_DEBUG_SOURCE_APPLICATION_KHR
#define GL_DEBUG_SOURCE_OTHER GL_DEBUG_SOURCE_OTHER_KHR
#define GL_DEBUG_TYPE_ERROR GL_DEBUG_TYPE_ERROR_KHR
#define GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_KHR
#define GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_KHR
#define GL_DEBUG_TYPE_PORTABILITY GL_DEBUG_TYPE_PORTABILITY_KHR
#define GL_DEBUG_TYPE_PERFORMANCE GL_DEBUG_TYPE_PERFORMANCE_KHR
#define GL_DEBUG_TYPE_MARKER GL_DEBUG_TYPE_MARKER_KHR
#define GL_DEBUG_TYPE_PUSH_GROUP GL_DEBUG_TYPE_PUSH_GROUP_KHR
#define GL_DEBUG_TYPE_POP_GROUP GL_DEBUG_TYPE_POP_GROUP_KHR
#define GL_DEBUG_TYPE_OTHER GL_DEBUG_TYPE_OTHER_KHR
#define GL_DEBUG_SEVERITY_HIGH GL_DEBUG_SEVERITY_HIGH_KHR
#define GL_DEBUG_SEVERITY_MEDIUM GL_DEBUG_SEVERITY_MEDIUM_KHR
#define GL_DEBUG_SEVERITY_LOW GL_DEBUG_SEVERITY_LOW_KHR
#else
#define DEBUG_CALLBACK_TYPE APIENTRY
#endif

static void DEBUG_CALLBACK_TYPE gl2_debug_cb(GLenum source, GLenum type,
      GLuint id, GLenum severity, GLsizei length,
      const GLchar *message, void *userParam)
{
   const char      *src = NULL;
   const char *typestr  = NULL;

   switch (source)
   {
      case GL_DEBUG_SOURCE_API:
         src = "API";
         break;
      case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
         src = "Window system";
         break;
      case GL_DEBUG_SOURCE_SHADER_COMPILER:
         src = "Shader compiler";
         break;
      case GL_DEBUG_SOURCE_THIRD_PARTY:
         src = "3rd party";
         break;
      case GL_DEBUG_SOURCE_APPLICATION:
         src = "Application";
         break;
      case GL_DEBUG_SOURCE_OTHER:
         src = "Other";
         break;
      default:
         src = "Unknown";
         break;
   }

   switch (type)
   {
      case GL_DEBUG_TYPE_ERROR:
         typestr = "Error";
         break;
      case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
         typestr = "Deprecated behavior";
         break;
      case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
         typestr = "Undefined behavior";
         break;
      case GL_DEBUG_TYPE_PORTABILITY:
         typestr = "Portability";
         break;
      case GL_DEBUG_TYPE_PERFORMANCE:
         typestr = "Performance";
         break;
      case GL_DEBUG_TYPE_MARKER:
         typestr = "Marker";
         break;
      case GL_DEBUG_TYPE_PUSH_GROUP:
         typestr = "Push group";
         break;
      case GL_DEBUG_TYPE_POP_GROUP:
        typestr = "Pop group";
        break;
      case GL_DEBUG_TYPE_OTHER:
        typestr = "Other";
        break;
      default:
        typestr = "Unknown";
        break;
   }

   switch (severity)
   {
      case GL_DEBUG_SEVERITY_HIGH:
         RARCH_ERR("[GL debug (High, %s, %s)] %s\n", src, typestr, message);
         break;
      case GL_DEBUG_SEVERITY_MEDIUM:
         RARCH_WARN("[GL debug (Medium, %s, %s)] %s\n", src, typestr, message);
         break;
      case GL_DEBUG_SEVERITY_LOW:
         RARCH_LOG("[GL debug (Low, %s, %s)] %s\n", src, typestr, message);
         break;
   }
}

static void gl2_begin_debug(gl2_t *gl)
{
   if (gl_check_capability(GL_CAPS_DEBUG))
   {
#ifdef HAVE_GL_DEBUG_ES
      glDebugMessageCallbackKHR(gl2_debug_cb, gl);
      glDebugMessageControlKHR(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
      glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_KHR);
#else
      glDebugMessageCallback(gl2_debug_cb, gl);
      glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
      glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif
   }
   else
      RARCH_ERR("[GL] Neither GL_KHR_debug nor GL_ARB_debug_output are implemented. Cannot start GL debugging.\n");
}
#endif

static bool renderchain_gl2_init_first(void **renderchain_handle)
{
   gl2_renderchain_data_t *data = (gl2_renderchain_data_t *)calloc(1, sizeof(*data));

   if (!data)
      return false;

   *renderchain_handle = data;
   return true;
}

static void *gl2_init(const video_info_t *video)
{
   enum gfx_wrap_type wrap_type;
   unsigned full_x, full_y;
   unsigned shader_info_num;
   settings_t *settings                 = config_get_ptr();
   bool video_gpu_record                = settings->bools.video_gpu_record;
   bool video_scale_integer             = settings->bools.video_scale_integer;
   int interval                         = 0;
   unsigned mip_level                   = 0;
   unsigned mode_dims                   = 0;
   unsigned win_dims                    = 0;
   unsigned temp_dims                   = 0;
   bool force_smooth                    = false;
   bool force_fullscreen                = false;
   const char *vendor                   = NULL;
   const char *renderer                 = NULL;
   const char *version                  = NULL;
   struct retro_hw_render_callback *hwr = NULL;
   char *error_string                   = NULL;
   recording_state_t *recording_st      = recording_state_get_ptr();
   gl2_t *gl                            = (gl2_t*)calloc(1, sizeof(gl2_t));
   const gfx_ctx_driver_t *ctx_driver   = NULL;

   /* gl must exist before the context lookup writes its flags. */
   if (!gl)
      goto error;
   retro_atomic_int_init(&gl->hw_ring, 0);
   if (!(ctx_driver = gl2_get_context(gl)))
      goto error;

   /* Latched here, inside the wrapper's blocking CMD_INIT (the main
    * thread is parked in send-and-wait, so the settings read is
    * race-free), for every later gl2_core_context_is_mains() -
    * including the frame path, where main runs free. */
#ifdef HAVE_THREADS
   if (video_driver_thread_wrapper_active() && video_thread_hw_allowed())
      gl->flags |= GL2_FLAG_HW_RING_EXPECTED;
#endif

   video_context_driver_set((const gfx_ctx_driver_t*)ctx_driver);

   gl->ctx_driver                       = ctx_driver;
   gl->video_info                       = *video;

   RARCH_LOG("[GL] Found GL context: \"%s\".\n", ctx_driver->ident);

#if !defined(RARCH_CONSOLE) || defined(HAVE_LIBNX)
   if (!ctx_driver->get_proc_address)
   {
      RARCH_ERR("[GL] The \"%s\" context has no OpenGL.\n",
            ctx_driver->ident);
      goto error;
   }
#endif

   if (gl->ctx_driver->get_video_size)
      gl->ctx_driver->get_video_size(gl->ctx_data,
               &mode_dims);

   if (!video->fullscreen && !gl->ctx_driver->has_windowed)
   {
      RARCH_DBG("[GL] Config requires windowed mode, but context driver does not support it. "
                "Forcing fullscreen for this session.\n");
      force_fullscreen = true;
   }

#if defined(DINGUX)
   mode_dims   = VIDEO_SCALE_PACK(320, 240);
#endif
   full_x      = VIDEO_SCALE_W(mode_dims);
   full_y      = VIDEO_SCALE_H(mode_dims);
   interval    = 0;

   RARCH_LOG("[GL] Detecting screen resolution: %ux%u.\n", full_x, full_y);

   if (video->vsync)
      interval = video->swap_interval;

   if (gl->ctx_driver->swap_interval)
   {
      bool adaptive_vsync_enabled            = video_driver_test_all_flags(
            GFX_CTX_FLAGS_ADAPTIVE_VSYNC) && video->adaptive_vsync;
      if (adaptive_vsync_enabled && interval == 1)
         interval = -1;
      gl->ctx_driver->swap_interval(gl->ctx_data, interval);
      gl2_present_vsynced = (interval != 0);
   }

   win_dims    = video->dims;

   /* Neither axis set is the whole word clear */
   if (video->fullscreen && (win_dims == 0))
      win_dims = VIDEO_SCALE_PACK(full_x, full_y);
   /* If fullscreen had to be forced, VIDEO_SCALE_W(video->dims)/height is incorrect */
   else if (force_fullscreen)
      win_dims = VIDEO_SCALE_PACK(settings->uints.video_fullscreen_x,
            settings->uints.video_fullscreen_y);

   if (     !gl->ctx_driver->set_video_mode
         || !gl->ctx_driver->set_video_mode(gl->ctx_data, win_dims,
            (video->fullscreen || force_fullscreen)))
      goto error;
#if !defined(RARCH_CONSOLE) || defined(HAVE_LIBNX)
   rglgen_resolve_symbols(ctx_driver->get_proc_address);
#endif

   /* Clear out potential error flags in case we use cached context. */
   glGetError();

   vendor   = (const char*)glGetString(GL_VENDOR);
   renderer = (const char*)glGetString(GL_RENDERER);
   version  = (const char*)glGetString(GL_VERSION);

   RARCH_LOG("[GL] Vendor: %s, Renderer: %s.\n", vendor, renderer);
   RARCH_LOG("[GL] Version: %s.\n", version);

   {
      gfx_ctx_flags_t ctx_flags;
      ctx_flags.flags = 0;
      video_context_driver_get_flags(&ctx_flags);
      if (BIT32_GET(ctx_flags.flags, GFX_CTX_FLAGS_HDR10_FRAMEBUFFER))
      {
         /* The same offscreen and encode as scRGB, finished in PQ */
         if (gl2_scrgb_init_program(gl))
         {
            gl->scrgb.active = true;
            gl->scrgb.pq_out = true;
            RARCH_LOG("[GL] HDR10 backbuffer active; the frame will be encoded to Rec.2020 PQ.\n");
         }
      }
      else if (BIT32_GET(ctx_flags.flags, GFX_CTX_FLAGS_SCRGB_FRAMEBUFFER))
      {
         if (gl2_scrgb_init_program(gl))
         {
            gl->scrgb.active = true;
            RARCH_LOG("[GL] scRGB backbuffer active; SDR content will be encoded for HDR output.\n");
         }
         else
            RARCH_WARN("[GL] scRGB backbuffer present but the encode program could not be built; output will be dim (paper-white mapped).\n");
      }
   }

   /* Whether the source is PQ decides every later composition choice,
    * it arrives only through video_info, and getting it wrong is
    * silent. State it once, as the glcore and Vulkan drivers do. */
   RARCH_LOG("[GL] Source is %s (%s), output %s.\n",
         video->source_hdr10 ? "HDR10 PQ Rec.2020"
                             : (video->source_10bit ? "10-bit SDR" : "SDR"),
         video->rgb32 ? "32-bit" : "16-bit",
         gl->scrgb.active ? "scRGB" : "SDR");

   if (string_is_equal(ctx_driver->ident, "null"))
      goto error;

   if (version && *version)
   {
      const char *v = version;
      char *end     = NULL;
      if (string_starts_with(v, "OpenGL ES "))
         v += STRLEN_CONST("OpenGL ES ");
      else if (string_starts_with(v, "OpenGL "))
         v += STRLEN_CONST("OpenGL ");
      gl->version_major = (int)strtol(v, &end, 10);
      if (end && *end == '.')
         gl->version_minor = (int)strtol(end + 1, NULL, 10);
   }

   {
      size_t _len = 0;

      if (vendor && *vendor)
      {
         strlcpy_append(gl->device_str, sizeof(gl->device_str), &_len, vendor);
         strlcpy_append(gl->device_str, sizeof(gl->device_str), &_len, " ");
      }

      if (renderer && *renderer)
         strlcpy_append(gl->device_str, sizeof(gl->device_str), &_len, renderer);

      if (version && *version)
         video_driver_set_gpu_api_version_string(version);
   }

#ifdef _WIN32
   if (   string_is_equal(vendor,   "Microsoft Corporation"))
      if (string_is_equal(renderer, "GDI Generic"))
#ifdef HAVE_OPENGL1
         video_driver_force_fallback("gl1");
#else
         video_driver_force_fallback("gdi");
#endif
#endif
#if defined(__APPLE__) && defined(__ppc__)
   if (gl->version_major == 1)
         video_driver_force_fallback("gl1");
#endif

   hwr = video_driver_get_hw_context();

   if (hwr->context_type == RETRO_HW_CONTEXT_OPENGL_CORE)
   {
      gl_query_core_context_set(true);
      gl->flags |= GL2_FLAG_CORE_CONTEXT_IN_USE;

      if (hwr->context_type == RETRO_HW_CONTEXT_OPENGL_CORE)
      {
         /* Ensure that the rest of the frontend knows we have a core context */
         gfx_ctx_flags_t flags;
         flags.flags = 0;
         BIT32_SET(flags.flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT);

         video_context_driver_set_flags(&flags);
      }

      RARCH_LOG("[GL] Using Core GL context, setting up VAO...\n");
      if (!gl_check_capability(GL_CAPS_VAO))
      {
         RARCH_ERR("[GL] Failed to initialize VAOs.\n");
         goto error;
      }
   }

   if (!renderchain_gl2_init_first(&gl->renderchain_data))
   {
      RARCH_ERR("[GL] Renderchain could not be initialized.\n");
      goto error;
   }

   gl2_renderchain_restore_default_state(gl);

#ifndef HAVE_OPENGLES
   if (hwr->context_type == RETRO_HW_CONTEXT_OPENGL_CORE)
   {
      gl2_renderchain_data_t *chain = (gl2_renderchain_data_t*)
         gl->renderchain_data;

      glGenVertexArrays(1, &chain->vao);
   }
#endif

   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glBlendEquation(GL_FUNC_ADD);

   gl->flags              &= ~GL2_FLAG_HW_RENDER_USE;
   if (gl_check_capability(GL_CAPS_FBO))
   {
      gl->flags           |=  GL2_FLAG_HAVE_FBO;
      if (hwr->context_type != RETRO_HW_CONTEXT_NONE)
         gl->flags        |=  GL2_FLAG_HW_RENDER_USE;
   }
   else
      gl->flags           &= ~GL2_FLAG_HAVE_FBO;

   if (!gl2_resolve_extensions(gl, ctx_driver->ident, video))
      goto error;

#if !defined(HAVE_OPENGLES) && !defined(HAVE_PSGL)
   /* Half-float textures are core in GL 3.0, extensions before it */
   gl->fp16_textures =
            (version && version[0] >= '3' && version[0] <= '9'
             && version[1] == '.')
         || gl_query_core_context_in_use()
         || (     gl_query_extension("ARB_texture_float")
               && gl_query_extension("ARB_half_float_pixel"));
   gl->scrgb.fp16_ok = gl->fp16_textures
         && (gl->flags & GL2_FLAG_HAVE_FBO)
         && gl->scrgb.active
         && gl2_fp16_target_ok();
#endif

#ifdef GL_DEBUG
   gl2_begin_debug(gl);
#endif

   if (video->fullscreen || force_fullscreen)
      gl->flags  |=  GL2_FLAG_FULLSCREEN;

   mode_dims      = 0;

   if (gl->ctx_driver->get_video_size)
      gl->ctx_driver->get_video_size(gl->ctx_data,
            &mode_dims);

#if defined(DINGUX)
   mode_dims      = VIDEO_SCALE_PACK(320, 240);
#endif
   temp_dims      = mode_dims;

   /* Get real known video size, which might have been altered by context.
    * One axis alone is not a size, so both have to be set. */

   if (VIDEO_SCALE_W(temp_dims) != 0 && VIDEO_SCALE_H(temp_dims) != 0)
      video_driver_set_output_dims(temp_dims);
   else
      temp_dims = video_driver_get_output_dims();

   gl->video_dims        = temp_dims;

   RARCH_LOG("[GL] Using resolution %ux%u.\n",
         VIDEO_SCALE_W(temp_dims), VIDEO_SCALE_H(temp_dims));

   gl->vertex_ptr        = hwr->bottom_left_origin
      ? vertexes : vertexes_flipped;

   /* Better pipelining with GPU due to synchronous glSubTexImage.
    * Multiple async PBOs would be an alternative,
    * but still need multiple textures with PREV.
    */
   gl->textures         = 4;

   if (gl->flags & GL2_FLAG_HW_RENDER_USE)
   {
      /* All on GPU, no need to excessively
       * create textures. Under the threaded wrapper's ring the core
       * renders on another thread into one while this thread reads
       * another, so there are as many as the ring has slots. */
      gl->textures = 1;
#ifdef HAVE_THREADS
      if (video_driver_thread_wrapper_active() && video_thread_hw_allowed())
         gl->textures = VIDEO_THREAD_HW_RING;
#endif
#ifdef GL_DEBUG
      if (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
      {
         gl->ctx_driver->bind_hw_render(gl->ctx_data, true);
         gl2_begin_debug(gl);
         gl->ctx_driver->bind_hw_render(gl->ctx_data, false);
      }
      else
         gl2_begin_debug(gl);
#endif
   }

   gl->white_color_ptr = white_color;

   gl->shader          = (shader_backend_t*)gl2_shader_ctx_drivers[0];

   if (!gl->shader)
   {
      RARCH_ERR("[GL] Shader driver initialization failed.\n");
      goto error;
   }

   RARCH_LOG("[GL] Default shader backend found: %s.\n", gl->shader->ident);

   if (!gl2_shader_init(gl, ctx_driver, hwr))
   {
      RARCH_ERR("[GL] Shader initialization failed.\n");
      goto error;
   }

   {
      unsigned texture_info_id = gl->shader->get_prev_textures(gl->shader_data);
      unsigned minimum         = texture_info_id;
      gl->textures             = MAX(minimum + 1, gl->textures);
   }

   shader_info_num = gl->shader->num_shaders(gl->shader_data);

   RARCH_LOG("[GL] Using %u textures.\n", gl->textures);
   RARCH_LOG("[GL] Loaded %u program(s).\n",
         shader_info_num);

   gl->tex_dims = VIDEO_SCALE_PACK(RARCH_SCALE_BASE * video->input_scale,
         RARCH_SCALE_BASE * video->input_scale);
   if (video->force_aspect)
      gl->flags       |=  GL2_FLAG_KEEP_ASPECT;
   else
      gl->flags       &= ~GL2_FLAG_KEEP_ASPECT;

#if defined(HAVE_ODROIDGO2)
   if (settings->bools.video_ctx_scaling)
      gl->flags &= ~GL2_FLAG_KEEP_ASPECT;
   else
#endif
   /* Apparently need to set viewport for passes
    * when we aren't using FBOs. */
      gl2_set_shader_viewports(gl, video_scale_integer);

   mip_level            = 1;
   if (gl->shader->mipmap_input(gl->shader_data, mip_level))
      gl->flags        |=  GL2_FLAG_TEXTURE_MIPMAP;
   else
      gl->flags        &= ~GL2_FLAG_TEXTURE_MIPMAP;

   if (gl->shader->filter_type(gl->shader_data,
            1, &force_smooth))
      gl->tex_min_filter = (gl->flags & GL2_FLAG_TEXTURE_MIPMAP)
         ? (force_smooth ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST)
         : (force_smooth ? GL_LINEAR : GL_NEAREST);
   else
      gl->tex_min_filter = (gl->flags & GL2_FLAG_TEXTURE_MIPMAP)
         ? (video->smooth ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST)
         : (video->smooth ? GL_LINEAR : GL_NEAREST);

   gl->tex_mag_filter = gl2_min_filter_to_mag(gl->tex_min_filter);

   wrap_type     = gl->shader->wrap_type(
         gl->shader_data, 1);

   gl->wrap_mode      = gl2_wrap_type_to_enum(wrap_type);

   gl2_set_texture_fmts(gl, video->rgb32);

   memcpy(gl->tex_info.coord, tex_coords, sizeof(gl->tex_info.coord));
   gl->coords.vertex         = gl->vertex_ptr;
   gl->coords.tex_coord      = gl->tex_info.coord;
   gl->coords.color          = gl->white_color_ptr;
   gl->coords.lut_tex_coord  = tex_coords;
   gl->coords.vertices       = 4;

   /* Empty buffer that we use to clear out
    * the texture with on res change.
    *
    * That clear reads the full tex_w * tex_h rectangle, which at 32bpp
    * is every byte of the allocation, so a driver copying the source
    * in blocks has nothing to spare past the final row. Large mappings
    * are guarded on modern allocators, so those few bytes are a fault
    * rather than a harmless read. Carry an extra row. */
   gl->empty_buf             = calloc(
         (size_t)VIDEO_SCALE_W(gl->tex_dims)
               * (VIDEO_SCALE_H(gl->tex_dims) + 1),
         sizeof(uint32_t));

   gl->conv_buffer           = calloc(VIDEO_SCALE_AREA(gl->tex_dims),
         sizeof(uint32_t));

   if (!gl->conv_buffer)
      goto error;

   gl2_init_textures(gl);
   gl2_init_textures_data(gl);

   {
      static const uint32_t white = 0xffffffff;
      glGenTextures(1, &gl->white_texture);
      gl_load_texture_data(gl->white_texture, RARCH_WRAP_EDGE,
            TEXTURE_FILTER_NEAREST, 4, 1, 1, &white,
            sizeof(uint32_t), false);
   }

   gl2_fbo_settings_latch(gl);
   gl2_renderchain_init(gl,
         (gl2_renderchain_data_t*)gl->renderchain_data);

   if (gl->flags & GL2_FLAG_HAVE_FBO)
   {
      if (     (gl->flags & GL2_FLAG_HW_RENDER_USE)
            && !gl2_renderchain_init_hw_render(gl,
                  (gl2_renderchain_data_t*)gl->renderchain_data,
                  VIDEO_SCALE_W(gl->tex_dims),
                  VIDEO_SCALE_H(gl->tex_dims)))
      {
         RARCH_ERR("[GL] Hardware rendering context initialization failed.\n");
         goto error;
      }
   }

   if (gl->ctx_driver->input_driver)
   {
      const char *joypad_name = settings->arrays.input_joypad_driver;
      gl->ctx_driver->input_driver(
            gl->ctx_data, joypad_name);
   }


   /* Only bother with PBO readback if we're doing GPU recording.
    * Check recording_st->enable and not
    * driver.recording_data, because recording is
    * not initialized yet.
    */
   if (  video_gpu_record
      && recording_st->enable)
   {
      gl->flags |=  GL2_FLAG_PBO_READBACK_ENABLE;
      if (gl2_init_pbo_readback(gl))
      {
         RARCH_LOG("[GL] Async PBO readback enabled.\n");
      }
   }
   else
      gl->flags &= ~GL2_FLAG_PBO_READBACK_ENABLE;

   if (!gl_check_error(&error_string))
   {
      RARCH_ERR("[GL] %s\n", error_string);
      free(error_string);
      goto error;
   }

#ifdef HAVE_GL2_XR
   gl2_desktop_xr_start(gl);
   if (gl->desktop_xr)
      gl->desktop_xr->window_interval = interval;
#endif

   /* Init leaves the core's context current for context_reset on this
    * thread; when the wrapper's ring will drive the core, the main
    * thread takes that context itself and this one must not hold it. */
   if (     (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
         && !gl2_core_context_is_mains(gl))
      gl->ctx_driver->bind_hw_render(gl->ctx_data, true);

   return gl;

error:
   video_context_driver_free();
   gl2_destroy_resources(gl);
   return NULL;
}

static bool gl2_alive(void *data)
{
   bool ret             = false;
   bool quit            = false;
   bool resize          = false;
   gl2_t         *gl    = (gl2_t*)data;
   unsigned temp_dims  = gl->video_dims;

   gl->ctx_driver->check_window(gl->ctx_data,
         &quit, &resize, &temp_dims);

#ifdef __WINRT__
   if (is_running_on_xbox())
   {
      /* Match the output res to the display resolution */
      temp_dims  = VIDEO_SCALE_PACK(uwp_get_width(),
            uwp_get_height());
   }
#endif
   if (quit)
      gl->flags   |= GL2_FLAG_QUITTING;
   else if (resize)
      gl->flags   |= GL2_FLAG_SHOULD_RESIZE;

   ret             = !(gl->flags & GL2_FLAG_QUITTING);

   if (VIDEO_SCALE_W(temp_dims) != 0 && VIDEO_SCALE_H(temp_dims) != 0)
   {
      video_driver_set_output_dims(temp_dims);
      gl->video_dims   = temp_dims;
   }

   return ret;
}

static bool gl2_suppress_screensaver(void *data, bool enable)
{
   bool enabled = enable;
   gl2_t *gl     = (gl2_t*)data;

   if (gl->ctx_data && gl->ctx_driver->suppress_screensaver)
      return gl->ctx_driver->suppress_screensaver(gl->ctx_data, enabled);
   return false;
}

static void gl2_update_tex_filter_frame(gl2_t *gl, bool video_smooth)
{
   unsigned i, mip_level;
   GLenum wrap_mode;
   GLuint new_filt;
   enum gfx_wrap_type wrap_type;
   bool smooth                       = false;

   if (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, false);

   if (!gl->shader->filter_type(gl->shader_data,
            1, &smooth))
      smooth             = video_smooth;

   mip_level             = 1;
   wrap_type             = gl->shader->wrap_type(gl->shader_data, 1);
   wrap_mode             = gl2_wrap_type_to_enum(wrap_type);
   if (gl->shader->mipmap_input(gl->shader_data, mip_level))
      gl->flags         |=  GL2_FLAG_TEXTURE_MIPMAP;
   else
      gl->flags         &= ~GL2_FLAG_TEXTURE_MIPMAP;
   gl->video_info.smooth = smooth;
   new_filt              = (gl->flags & GL2_FLAG_TEXTURE_MIPMAP)
      ? (smooth ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST)
      : (smooth ? GL_LINEAR : GL_NEAREST);

   if (new_filt == gl->tex_min_filter && wrap_mode == gl->wrap_mode)
      return;

   gl->tex_min_filter    = new_filt;
   gl->tex_mag_filter    = gl2_min_filter_to_mag(gl->tex_min_filter);
   gl->wrap_mode         = wrap_mode;

   for (i = 0; i < gl->textures; i++)
   {
      if (!gl->texture[i])
         continue;

      GL2_BIND_TEXTURE(gl->texture[i], gl->wrap_mode, gl->tex_mag_filter,
            gl->tex_min_filter);
   }

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
   if (     (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
         && !gl2_core_context_is_mains(gl))
      gl->ctx_driver->bind_hw_render(gl->ctx_data, true);
}

/* The HW-render rebuild on a shader change, as a call the wrapper can
 * run on the core's thread; see gl2_set_shader. */
static void gl2_deinit_hw_render_cb(void *data)
{
   gl2_t *gl = (gl2_t*)data;
   gl2_renderchain_deinit_hw_render(gl, (gl2_renderchain_data_t*)gl->renderchain_data);
}

static void gl2_init_hw_render_cb(void *data)
{
   gl2_t *gl = (gl2_t*)data;
   gl2_renderchain_init_hw_render(gl,
         (gl2_renderchain_data_t*)gl->renderchain_data,
         VIDEO_SCALE_W(gl->tex_dims), VIDEO_SCALE_H(gl->tex_dims));
}

static bool gl2_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
#if defined(HAVE_GLSL) || defined(HAVE_CG)
   unsigned textures;
   video_shader_ctx_init_t init_data;
   enum rarch_shader_type fallback;
   gl2_t *gl                = (gl2_t*)data;
   settings_t *settings     = config_get_ptr();
   bool video_scale_integer = settings->bools.video_scale_integer;
#ifdef HAVE_ODROIDGO2
   bool video_smooth        = settings->bools.video_ctx_scaling ? false : settings->bools.video_smooth;
#else
   bool video_smooth        = settings->bools.video_smooth;
#endif

   if (!gl)
      return false;

   if (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, false);

   fallback = gl2_get_fallback_shader_type(type);

   if (fallback == RARCH_SHADER_NONE)
   {
      RARCH_ERR("[GL] No supported shader backend found.\n");
      goto error;
   }

   gl->shader->deinit(gl->shader_data);
   gl->shader_data = NULL;

   if (type != fallback)
   {
      RARCH_ERR("[GL] %s shader not supported, falling back to stock %s.\n",
            video_shader_type_to_str(type), video_shader_type_to_str(fallback));
      path = NULL;
   }

   if (gl->flags & GL2_FLAG_FBO_INITED)
   {
      gl2_renderchain_deinit_fbo(gl,
            (gl2_renderchain_data_t*)gl->renderchain_data);

      glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
   }

   init_data.shader_type             = fallback;
   init_data.path                    = path;
   init_data.shader                  = NULL;
   init_data.data                    = gl;
   init_data.shader_data             = NULL;
   init_data.gl.core_context_enabled = false;

   if (!gl_shader_driver_init(&init_data))
   {
      init_data.path   = NULL;

      gl_shader_driver_init(&init_data);

      gl->shader       = init_data.shader;
      gl->shader_data  = init_data.shader_data;
      gl->views.preset = false;

      RARCH_WARN("[GL] Failed to set multipass shader. Falling back to stock.\n");

      goto error;
   }

   gl->shader       = init_data.shader;
   gl->shader_data  = init_data.shader_data;
   gl->views.preset = path && *path;
#ifdef GL2_VIEWS
   /* The views' earlier frames and feedback were the last preset's */
   gl2_views_free_copies(gl);
#endif

   gl2_update_tex_filter_frame(gl, video_smooth);

   {
      unsigned texture_info_id = gl->shader->get_prev_textures(gl->shader_data);
      textures                 = texture_info_id + 1;
   }

   if (textures > gl->textures) /* Have to reinit a bit. */
   {
      /* The HW-render framebuffers live in the core's context. Under
       * the wrapper's ring that context is current on the core's
       * thread, which is waiting for this command: the teardown and
       * rebuild go there. Textures and renderbuffers are shared and
       * can be made here; framebuffers are not. */
      if ((gl->flags & GL2_FLAG_HW_RENDER_USE) && (gl->flags & GL2_FLAG_FBO_INITED))
         video_thread_call_on_waiter(gl2_deinit_hw_render_cb, gl);

#ifdef GL2_KMS_FRAMES
      gl2_kms_frames_drop(gl);
#endif
      glDeleteTextures(gl->textures, gl->texture);
#if defined(HAVE_PSGL)
      /* gl2_init_textures generates the buffer again */
      glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, 0);
      glDeleteBuffers(1, &gl->pbo);
      gl->pbo       = 0;
#endif
      gl->textures  = textures;
      gl->tex_index = 0;
      RARCH_LOG("[GL] Using %u textures.\n", gl->textures);
      gl2_init_textures(gl);
      gl2_init_textures_data(gl);
      /* The new textures must be visible to the other context before
       * its framebuffers attach them. */
      glFlush();

      if (gl->flags & GL2_FLAG_HW_RENDER_USE)
         video_thread_call_on_waiter(gl2_init_hw_render_cb, gl);
   }

   gl2_fbo_settings_latch(gl);
   gl2_renderchain_init(gl,
         (gl2_renderchain_data_t*)gl->renderchain_data);

   /* Apparently need to set viewport for passes when we aren't using FBOs. */
   gl2_set_shader_viewports(gl, video_scale_integer);
   gl2_bind_core_context(gl, true);

   return true;

error:
   if (     (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
         && !gl2_core_context_is_mains(gl))
      gl->ctx_driver->bind_hw_render(gl->ctx_data, true);
#endif
   return false;
}

static void gl2_viewport_info(void *data, struct video_viewport *vp)
{
   unsigned top_y, top_dist;
   gl2_t *gl       = (gl2_t*)data;
   unsigned width  = VIDEO_SCALE_W(gl->video_dims);
   unsigned height = VIDEO_SCALE_H(gl->video_dims);

   *vp             = gl->vp;
   vp->full_dims   = VIDEO_SCALE_PACK(width, height);

   /* Adjust as GL viewport is bottom-up. */
   top_y           = VIDEO_POS_Y(vp->pos) + VIDEO_SCALE_H(vp->dims);
   top_dist        = height - top_y;
   VIDEO_POS_PUT_Y(vp->pos, top_dist);
}

static bool gl2_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   gl2_t *gl             = (gl2_t*)data;

   if (!gl)
      return false;

   return gl2_renderchain_read_viewport(gl, buffer, is_idle, false);
}

static bool gl2_read_viewport_bgrx(void *data, uint8_t *buffer,
      bool is_idle, bool *bottom_up)
{
   gl2_t *gl = (gl2_t*)data;

   if (!gl)
      return false;

   /* glReadPixels returns the bottom row first. */
   *bottom_up = true;
   return gl2_renderchain_read_viewport(gl, buffer, is_idle, true);
}

#if defined(HAVE_GL_ASYNC_READBACK) && !defined(HAVE_OPENGLES) && !defined(HAVE_PSGL)
static bool gl2_record_read(void *data, uint8_t *buffer, bool bgrx,
      bool *bottom_up)
{
   gl2_t *gl = (gl2_t*)data;
   if (!gl || !VIDEO_SCALE_W(gl->vp.dims) || !VIDEO_SCALE_H(gl->vp.dims))
      return false;
   if (     !(gl->flags & GL2_FLAG_PBO_READBACK_ENABLE)
         || (unsigned)gl->pbo_readback_scaler.in_width != VIDEO_SCALE_W(gl->vp.dims)
         || (unsigned)gl->pbo_readback_scaler.in_height != VIDEO_SCALE_H(gl->vp.dims))
   {
      if (gl->flags & GL2_FLAG_PBO_READBACK_ENABLE)
      {
         gl2_free_pbo_readback(gl);
      }
      gl->flags |= GL2_FLAG_PBO_READBACK_ENABLE;
      if (!gl2_init_pbo_readback(gl))
         gl->flags &= ~GL2_FLAG_PBO_READBACK_ENABLE;
      return false;
   }
   /* glReadPixels returns the bottom row first, in either format. */
   *bottom_up = true;
   return gl2_read_pbo(gl, buffer, bgrx);
}
#endif

#ifdef HAVE_OVERLAY
/* The texture names and the vertex, texture and colour coordinate
 * arrays of a page's images come out of one zeroed block, each region
 * starting on a 64-byte boundary; overlay_tex owns it. Geometry starts
 * as the whole screen, colour as opaque white. Names come from
 * @textures when the page shows the pack's textures, else are made
 * here for the upload that follows. */
static bool gl2_overlay_alloc(gl2_t *gl, unsigned num_images,
      const uintptr_t *textures)
{
   size_t o_vertex, o_tex, o_color;
   unsigned i, j;

   gl2_free_overlay(gl);
   o_vertex = ((num_images * sizeof(GLuint)) + 63) & ~(size_t)63;
   o_tex    = o_vertex + ((2 * 4 * num_images * sizeof(GLfloat) + 63) & ~(size_t)63);
   o_color  = o_tex    + ((2 * 4 * num_images * sizeof(GLfloat) + 63) & ~(size_t)63);
   gl->overlay_tex = (GLuint*)
      calloc(1, o_color + 4 * 4 * num_images * sizeof(GLfloat));

   if (!gl->overlay_tex)
      return false;

   gl->overlay_vertex_coord = (GLfloat*)((uint8_t*)gl->overlay_tex + o_vertex);
   gl->overlay_tex_coord    = (GLfloat*)((uint8_t*)gl->overlay_tex + o_tex);
   gl->overlay_color_coord  = (GLfloat*)((uint8_t*)gl->overlay_tex + o_color);

   gl->overlays = num_images;
   if (textures)
   {
      for (i = 0; i < num_images; i++)
         gl->overlay_tex[i] = (GLuint)textures[i];
      gl->flags |= GL2_FLAG_OVERLAY_BORROWED;
   }
   else
      glGenTextures(num_images, gl->overlay_tex);

   for (i = 0; i < num_images; i++)
   {
      gl2_overlay_tex_geom(gl, i, 0, 0, 1, 1);
      gl2_overlay_vertex_geom(gl, i, 0, 0, 1, 1);
      for (j = 0; j < 16; j++)
         gl->overlay_color_coord[16 * i + j] = 1.0f;
   }
   return true;
}

static bool gl2_overlay_load(void *data,
      const void *image_data, unsigned num_images)
{
   unsigned i;
   bool ok;
   gl2_t *gl = (gl2_t*)data;
   const struct texture_image *images =
      (const struct texture_image*)image_data;

   if (!gl)
      return false;

   if (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, false);

   if ((ok = gl2_overlay_alloc(gl, num_images, NULL)))
   {
      for (i = 0; i < num_images; i++)
      {
         unsigned alignment = gl2_get_alignment(images[i].width
               * sizeof(uint32_t));

         gl_load_texture_data(gl->overlay_tex[i],
               RARCH_WRAP_EDGE, TEXTURE_FILTER_LINEAR,
               alignment,
               images[i].width, images[i].height, images[i].pixels,
               sizeof(uint32_t), images[i].pix10);
      }
   }

   if (     (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
         && !gl2_core_context_is_mains(gl))
      gl->ctx_driver->bind_hw_render(gl->ctx_data, true);
   return ok;
}

/* A page of the pack's textures: no upload, no GL call but the
 * geometry setup. The names are gl2_load_texture's, valid on this
 * context. */
static bool gl2_overlay_load_textures(void *data,
      const uintptr_t *textures, unsigned num_textures)
{
   gl2_t *gl = (gl2_t*)data;
   if (!gl)
      return false;
   return gl2_overlay_alloc(gl, num_textures, textures);
}

static void gl2_overlay_enable(void *data, bool state)
{
   gl2_t *gl          = (gl2_t*)data;
   if (!gl)
      return;

   if (state)
      gl->flags      |=  GL2_FLAG_OVERLAY_ENABLE;
   else
      gl->flags      &= ~GL2_FLAG_OVERLAY_ENABLE;

   if ((gl->flags & GL2_FLAG_FULLSCREEN) && gl->ctx_driver->show_mouse)
      gl->ctx_driver->show_mouse(gl->ctx_data, state);
}

static void gl2_overlay_full_screen(void *data, bool enable)
{
   gl2_t *gl = (gl2_t*)data;
   if (!gl)
      return;

   if (enable)
      gl->flags |=  GL2_FLAG_OVERLAY_FULLSCREEN;
   else
      gl->flags &= ~GL2_FLAG_OVERLAY_FULLSCREEN;
}

static void gl2_overlay_set_alpha(void *data, unsigned image, float mod)
{
   GLfloat *color;
   gl2_t *gl = (gl2_t*)data;

   /* As the geometry setters: no page loaded is a NULL array, and an
    * index off the end of the page is the neighbouring block. */
   if (!gl || !gl->overlay_color_coord || image >= gl->overlays)
      return;

   color         = (GLfloat*)&gl->overlay_color_coord[image * 16];
   color[ 0 + 3] = mod;
   color[ 4 + 3] = mod;
   color[ 8 + 3] = mod;
   color[12 + 3] = mod;
}

static const video_overlay_interface_t gl2_overlay_interface = {
   gl2_overlay_enable,
   gl2_overlay_load,
   gl2_overlay_load_textures,
   gl2_overlay_tex_geom,
   gl2_overlay_vertex_geom,
   gl2_overlay_full_screen,
   gl2_overlay_set_alpha,
};

static void gl2_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface) {*iface = &gl2_overlay_interface;}
#endif

static retro_proc_address_t gl2_get_proc_address(void *data, const char *sym)
{
   gl2_t *gl = (gl2_t*)data;

   if (gl && gl->ctx_driver->get_proc_address)
      return gl->ctx_driver->get_proc_address(sym);

   return NULL;
}

static void gl2_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   gl2_t *gl = (gl2_t*)data;

   if (!gl)
      return;

   gl->flags        |= (GL2_FLAG_KEEP_ASPECT
                     |  GL2_FLAG_SHOULD_RESIZE);
#if defined(HAVE_ODROIDGO2)
   /* What the last frame carried, not what the setting says now: this
    * runs on the video thread under the threaded wrapper. */
   if (gl->flags & GL2_FLAG_CTX_SCALING)
      gl->flags     &= ~GL2_FLAG_KEEP_ASPECT;
#endif
}

static void gl2_apply_state_changes(void *data)
{
   gl2_t *gl            = (gl2_t*)data;
   if (gl)
      gl->flags        |= GL2_FLAG_SHOULD_RESIZE;
}

#if !defined(HAVE_OPENGLES) && !defined(HAVE_PSGL)
/* Half floats, R,G,B,A as sampled: RGBA16F with no swizzle and no
 * mipmaps, which half-float storage need not generate */
static void gl2_load_texture_fp16(GLuint id,
      enum texture_filter_type filter_type, const struct texture_image *ti)
{
   GLint filter = (   filter_type == TEXTURE_FILTER_NEAREST
                   || filter_type == TEXTURE_FILTER_MIPMAP_NEAREST)
      ? GL_NEAREST : GL_LINEAR;
   GL2_BIND_TEXTURE(id, GL_CLAMP_TO_EDGE, filter, filter);
   glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, ti->width, ti->height, 0,
         GL_RGBA, GL_HALF_FLOAT, ti->pixels);
   gl2_fp16_remember(id);
}
#endif

static void video_texture_load_gl2(
      const struct texture_image *ti,
      enum texture_filter_type filter_type,
      uintptr_t *idptr)
{
   GLuint id;
   unsigned width     = 0;
   unsigned height    = 0;
   const void *pixels = NULL;
#if !defined(HAVE_OPENGLES) && !defined(HAVE_PSGL)
   if (ti && ti->fp16)
   {
      *idptr = 0;
      if (!gl2_fp16_reserve())
         return;
      glGenTextures(1, &id);
      *idptr = id;
      gl2_load_texture_fp16(id, filter_type, ti);
      return;
   }
#endif
   /* Generate the OpenGL texture object */
   glGenTextures(1, &id);
   *idptr             = id;

   if (ti)
   {
      width           = ti->width;
      height          = ti->height;
      pixels          = ti->pixels;
   }

   gl_load_texture_data(id,
         RARCH_WRAP_EDGE, filter_type,
         4 /* TODO/FIXME - dehardcode */,
         width, height, pixels,
         sizeof(uint32_t) /* TODO/FIXME - dehardcode */,
         ti ? ti->pix10 : false);
}

/* A texture of @ti: a planar frame converted on the GPU, anything else
 * uploaded as it is */
static void gl2_texture_load(gl2_t *gl, const struct texture_image *ti,
      enum texture_filter_type filter_type, uintptr_t *id)
{
   *id = 0;
   if (ti->planar)
   {
#ifdef HAVE_GL_PLANAR
      if (gl)
         *id = gl_planar_load(&gl->planar, ti,
               filter_type != TEXTURE_FILTER_NEAREST
               && filter_type != TEXTURE_FILTER_MIPMAP_NEAREST,
               (gl->flags & GL2_FLAG_CORE_CONTEXT_IN_USE) != 0);
#endif
      return;
   }
   video_texture_load_gl2(ti, filter_type, id);
}

#ifdef HAVE_THREADS
typedef struct
{
   gl2_t     *gl;
   void      *payload;
   uintptr_t  handle;
} gl2_texture_cmd_t;

static uintptr_t video_texture_load_wrap_gl2_mipmap(void *data)
{
   uintptr_t id            = 0;
   gl2_texture_cmd_t *cmd  = (gl2_texture_cmd_t*)data;
   gl2_t             *gl   = cmd->gl;
   void              *image = cmd->payload;

   if (gl && gl->ctx_driver->make_current)
      gl->ctx_driver->make_current(false);

   if (image)
      gl2_texture_load(gl, (struct texture_image*)image,
            TEXTURE_FILTER_MIPMAP_LINEAR, &id);
   return (int)id;
}

static uintptr_t video_texture_load_wrap_gl2(void *data)
{
   uintptr_t id            = 0;
   gl2_texture_cmd_t *cmd  = (gl2_texture_cmd_t*)data;
   gl2_t             *gl   = cmd->gl;
   void              *image = cmd->payload;

   if (gl && gl->ctx_driver->make_current)
      gl->ctx_driver->make_current(false);

   if (image)
      gl2_texture_load(gl, (struct texture_image*)image,
            TEXTURE_FILTER_LINEAR, &id);
   return (int)id;
}

static uintptr_t video_texture_unload_wrap_gl2(void *data)
{
   GLuint  glid;
   gl2_texture_cmd_t *cmd = (gl2_texture_cmd_t*)data;
   gl2_t             *gl  = cmd->gl;
   uintptr_t          id  = (uintptr_t)cmd->payload;

   if (gl && gl->ctx_driver->make_current)
      gl->ctx_driver->make_current(false);

   glid = (GLuint)id;
#if !defined(HAVE_OPENGLES) && !defined(HAVE_PSGL)
   gl2_fp16_forget(glid);
#endif
#ifdef HAVE_GL_TEXTURE_LEND
   if (gl)
      gl_texture_lend_forget(&gl->lend, glid);
#endif
#ifdef HAVE_GL_PLANAR
   if (gl)
      gl_planar_forget(&gl->planar, glid);
#endif
   glDeleteTextures(1, &glid);
   return 0;
}
#endif

static uintptr_t gl2_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   uintptr_t id = 0;

#ifdef HAVE_THREADS
   if (threaded)
   {
      gl2_texture_cmd_t cmd;
      custom_command_method_t func = video_texture_load_wrap_gl2;

      cmd.gl      = (gl2_t*)video_data;
      cmd.payload = data;

      switch (filter_type)
      {
         case TEXTURE_FILTER_MIPMAP_LINEAR:
         case TEXTURE_FILTER_MIPMAP_NEAREST:
            func = video_texture_load_wrap_gl2_mipmap;
            break;
         default:
            break;
      }
      return video_thread_texture_handle(&cmd, func);
   }
#endif

   gl2_texture_load((gl2_t*)video_data, (struct texture_image*)data,
         filter_type, &id);
   return id;
}

static void gl2_unload_texture(void *data,
      bool threaded, uintptr_t id)
{
   GLuint glid;
   if (!id)
      return;

#ifdef HAVE_THREADS
   if (threaded)
   {
      gl2_texture_cmd_t cmd;
      custom_command_method_t func = video_texture_unload_wrap_gl2;

      cmd.gl      = (gl2_t*)data;
      cmd.payload = (void*)id;

      video_thread_texture_handle(&cmd, func);
      return;
   }
#endif

   glid = (GLuint)id;
#if !defined(HAVE_OPENGLES) && !defined(HAVE_PSGL)
   gl2_fp16_forget(glid);
#endif
#ifdef HAVE_GL_TEXTURE_LEND
   if (data)
      gl_texture_lend_forget(&((gl2_t*)data)->lend, glid);
#endif
#ifdef HAVE_GL_PLANAR
   if (data)
      gl_planar_forget(&((gl2_t*)data)->planar, glid);
#endif
   glDeleteTextures(1, &glid);
}

/* Same-size, same-order contents into a texture gl2_load_texture made:
 * the storage stays, glTexSubImage2D rewrites it. The pixel format is
 * the one gl_load_texture_data chose from the driver's RGBA flag, so
 * the caller's order is the order the texture was created with. */
static enum video_texture_update gl2_update_texture_internal(gl2_t *gl,
      uintptr_t id, const struct texture_image *ti)
{
   bool use_rgba      = (video_driver_get_disp_flags() & VIDEO_FLAG_USE_RGBA);
   const void *pixels = ti->pixels;
#ifdef HAVE_GL_TEXTURE_LEND
   int lent           = -1;
#endif
   if (ti->planar)
   {
#ifdef HAVE_GL_PLANAR
      if (gl && gl_planar_update(&gl->planar, (unsigned)id, ti))
         return VIDEO_TEXTURE_UPDATE_DONE;
#endif
      return VIDEO_TEXTURE_UPDATE_REFUSED;
   }
#ifdef HAVE_GL_TEXTURE_LEND
   /* A lent slot holding the frame: uploaded from its buffer */
   if (gl && gl->lend
         && (lent = gl_texture_lend_bind(gl->lend, (unsigned)id, pixels)) != -1)
   {
      if (lent < 0)
         return VIDEO_TEXTURE_UPDATE_DROPPED;
      pixels = NULL;
   }
#endif
   glBindTexture(GL_TEXTURE_2D, (GLuint)id);
   glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
#if !defined(HAVE_OPENGLES) && !defined(HAVE_PSGL)
   if (ti->fp16)
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ti->width, ti->height,
            GL_RGBA, GL_HALF_FLOAT, pixels);
   else if (ti->pix10)
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ti->width, ti->height,
            GL_BGRA, GL_UNSIGNED_INT_2_10_10_10_REV, pixels);
   else
#endif
   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ti->width, ti->height,
         use_rgba ? GL_RGBA : RARCH_GL_TEXTURE_TYPE32,
         RARCH_GL_FORMAT32, pixels);
#ifdef HAVE_GL_TEXTURE_LEND
   if (lent >= 0)
      gl_texture_lend_unbind(gl->lend, (unsigned)id, lent);
#endif
   return VIDEO_TEXTURE_UPDATE_DONE;
}

#ifdef HAVE_THREADS
static uintptr_t video_texture_update_wrap_gl2(void *data)
{
   gl2_texture_cmd_t *cmd  = (gl2_texture_cmd_t*)data;
   gl2_t             *gl   = cmd->gl;

   if (gl && gl->ctx_driver->make_current)
      gl->ctx_driver->make_current(false);

   gl2_update_texture_internal(gl, (uintptr_t)cmd->handle,
         (const struct texture_image*)cmd->payload);
   return 1;
}
#endif

static enum video_texture_update gl2_update_texture(void *video_data,
      uintptr_t id, const struct texture_image *ti, bool threaded)
{
   if (!id || !ti || (!ti->pixels && !ti->planar))
      return VIDEO_TEXTURE_UPDATE_REFUSED;
#if !defined(HAVE_OPENGLES) && !defined(HAVE_PSGL)
   /* Half floats only into half-float storage, and only there: the
    * set is the video thread's, so the threaded check is the format */
   if (ti->fp16 && !((gl2_t*)video_data)->fp16_textures)
      return VIDEO_TEXTURE_UPDATE_REFUSED;
   if (!threaded && ti->fp16 != gl2_fp16_is((GLuint)id))
      return VIDEO_TEXTURE_UPDATE_REFUSED;
#else
   if (ti->fp16)
      return VIDEO_TEXTURE_UPDATE_REFUSED;
#endif

#ifdef HAVE_THREADS
   if (threaded)
   {
      gl2_texture_cmd_t cmd;
      cmd.gl      = (gl2_t*)video_data;
      cmd.payload = (void*)ti;
      cmd.handle  = id;
      video_thread_texture_handle(&cmd, video_texture_update_wrap_gl2);
      return VIDEO_TEXTURE_UPDATE_DONE;
   }
#endif

   return gl2_update_texture_internal((gl2_t*)video_data, id, ti);
}

#ifdef HAVE_GL_TEXTURE_LEND
/* On the context's thread (see texture_lend in video_driver.h) */
static void *gl2_texture_lend(void *data, uintptr_t id, unsigned slot,
      size_t pitch, size_t *out_pitch)
{
   gl2_t *gl = (gl2_t*)data;
   void *mem = gl ? gl_texture_lend(&gl->lend, (unsigned)id, slot, pitch) : NULL;
   if (mem && out_pitch)
      *out_pitch = pitch;
   return mem;
}

static bool gl2_texture_lend_ready(void *data, uintptr_t id,
      unsigned slot)
{
   gl2_t *gl = (gl2_t*)data;
   return !gl || gl_texture_lend_ready(gl->lend, (unsigned)id, slot);
}
#endif

#ifdef HAVE_GL_FRAME_MEM
/* Whether gl2_renderchain_copy_frame uploads a software frame out of
 * frame memory as it is, rather than the CPU reading it first (to
 * convert it, repack it, or hand it to an EGL image): memory the GL
 * may map write-combined is for the GL to read. */
static bool gl2_frame_mem_uploads(gl2_t *gl)
{
   if (gl->flags & GL2_FLAG_HW_RENDER_USE)
      return false;
#if defined(HAVE_OPENGLES)
   if (     (video_driver_get_disp_flags() & VIDEO_FLAG_USE_RGBA)
         && gl->base_size == 4)
      return false;
   if (!(gl->flags & GL2_FLAG_HAVE_UNPACK_ROW_LENGTH))
      return false;
#if defined(HAVE_EGL)
   if (     gl->renderchain_data
         && (((gl2_renderchain_data_t*)gl->renderchain_data)->flags
            & GL2_CHAIN_FLAG_EGL_IMAGES))
      return false;
#endif
#else
   if (gl->base_size == 2 && !(gl->flags & GL2_FLAG_HAVE_ES2_COMPAT))
      return false;
#endif
   return true;
}

/* The threaded wrapper's frame buffers, on the context's thread: a
 * frame pushed from one is uploaded out of it by the GL
 * (gl2_renderchain_copy_frame). Not for a hardware core, nor where
 * the upload does not read it as it is. */
static void *gl2_frame_mem_alloc(void *data, size_t size, uintptr_t *id,
      bool *cached, unsigned *pitch_align)
{
   gl2_t *gl = (gl2_t*)data;
   if (!gl || !gl2_frame_mem_uploads(gl))
      return NULL;
   /* Where the GL puts a persistent mapping is its own choice */
   *cached      = false;
   *pitch_align = 4;
   return gl_frame_mem_alloc(&gl->frame_mem, size, id);
}

/* The frame lent to the core, unthreaded: frame memory the GL uploads
 * the pushed frame out of (gl2_renderchain_copy_frame), where that
 * upload reads it as it is (gl2_frame_mem_uploads). */
static bool gl2_frame_mem_lend_fb(gl2_t *gl, struct retro_framebuffer *fb)
{
   void    *p;
   if (!gl2_frame_mem_uploads(gl))
      return false;
   /* Remade at a new size: the cached frame may name the old ones */
   if (!gl_sw_fb_fits(&gl->sw_fb, fb->width, fb->height, gl->base_size))
      video_driver_cached_frame_retire();
   if (!(p = gl_sw_fb_lend(&gl->frame_mem, &gl->sw_fb,
               fb->width, fb->height, gl->base_size)))
      return false;
   fb->data         = p;
   fb->pitch        = (size_t)fb->width * gl->base_size;
   fb->format       = gl->video_info.source_hdr10
      ? RETRO_PIXEL_FORMAT_HDR10_2101010 : (gl->video_info.source_10bit
      ? RETRO_PIXEL_FORMAT_XRGB2101010 : ((gl->base_size == 4)
      ? RETRO_PIXEL_FORMAT_XRGB8888 : RETRO_PIXEL_FORMAT_RGB565));
   fb->memory_flags = 0;
   return true;
}

static bool gl2_frame_mem_ready(void *data, uintptr_t id)
{
   gl2_t *gl = (gl2_t*)data;
   return !gl || gl_frame_mem_ready(gl->frame_mem, id);
}

static void gl2_frame_mem_free(void *data, uintptr_t id)
{
   gl2_t *gl = (gl2_t*)data;
   if (gl)
      gl_frame_mem_free(gl->frame_mem, id);
}
#endif

#ifdef GL2_KMS_FRAMES
/* A buffer the GPU samples as the core writes it, the frame texture
 * taking it as storage: KMS only, a plain 8-bit frame in one texture
 * level, as large as the frame textures; one per texture, and three
 * more for the core to write into while those and the frames before
 * them are read */
static void *gl2_kms_frames_lend(gl2_t *gl, unsigned width,
      unsigned height, size_t *pitch)
{
   unsigned tw    = VIDEO_SCALE_W(gl->tex_dims);
   unsigned th    = VIDEO_SCALE_H(gl->tex_dims);
   unsigned count = gl->textures + 3;
   gl2_renderchain_data_t *chain = (gl2_renderchain_data_t*)
      gl->renderchain_data;

   if (     gl->kms_frames_off
         || video_driver_display_type_get() != RARCH_DISPLAY_KMS
         || (gl->flags & (GL2_FLAG_HW_RENDER_USE | GL2_FLAG_TEXTURE_MIPMAP))
         || (chain && (chain->flags & GL2_CHAIN_FLAG_EGL_IMAGES))
         || gl->video_info.source_10bit || gl->video_info.source_hdr10
         || count > EGL_GBM_FRAMES_MAX
         || !width || !height || width > tw || height > th)
      return NULL;
   if (!egl_gbm_frames_fits(gl->kms_frames, tw, th, gl->base_size, count))
   {
      gl2_kms_frames_drop(gl);
      if (!(gl->kms_frames = egl_gbm_frames_new(g_drm_fd, tw, th,
                  gl->base_size, count)))
      {
         gl->kms_frames_off = true;
         return NULL;
      }
   }
   return egl_gbm_frames_lend(gl->kms_frames, width, height, pitch);
}
#endif

#if defined(HAVE_GL_FRAME_MEM) || defined(GL2_KMS_FRAMES)
/* Not to a core that reads its frame: the memory may be write-combined */
static bool gl2_get_current_software_framebuffer(void *data,
      struct retro_framebuffer *fb)
{
   gl2_t *gl = (gl2_t*)data;
#ifdef GL2_KMS_FRAMES
   size_t pitch = 0;
   void *p;
#endif
   if (!gl || !fb || (fb->access_flags & RETRO_MEMORY_ACCESS_READ))
      return false;
#ifdef GL2_KMS_FRAMES
   if ((p = gl2_kms_frames_lend(gl, fb->width, fb->height, &pitch)))
   {
      fb->data         = p;
      fb->pitch        = pitch;
      fb->format       = (gl->base_size == 4)
         ? RETRO_PIXEL_FORMAT_XRGB8888 : RETRO_PIXEL_FORMAT_RGB565;
      fb->memory_flags = 0;
      return true;
   }
#endif
#ifdef HAVE_GL_FRAME_MEM
   return gl2_frame_mem_lend_fb(gl, fb);
#else
   return false;
#endif
}
#endif


#ifdef GL2_VIEWS
static void gl2_set_view_count(void *data, unsigned count)
{
   unsigned i;
   gl2_t *gl = (gl2_t*)data;
   if (!gl)
      return;
   gl->views.chains.count = MIN(count, RETRO_VIDEO_VIEWS_MAX);
   for (i = gl->views.chains.count; i < RETRO_VIDEO_VIEWS_MAX; i++)
      gl->views.chains.stock[i] = NULL;
}
#endif

static uint32_t gl2_get_flags(void *data)
{
   uint32_t flags = 0;
#ifdef GL2_VIEWS
   gl2_t *gl2     = (gl2_t*)data;
#endif
#if !defined(HAVE_OPENGLES) && !defined(HAVE_PSGL)
   gl2_t *gl      = (gl2_t*)data;

   /* Advertise a 10-bit source path only when there is genuinely
    * somewhere to present it: the scRGB FP16 backbuffer. Same gate and
    * same rationale as gl3_get_flags; scrgb.active is cached at init,
    * and querying the context here would recurse through
    * video_context_driver_get_flags(). NULL-safe because the frontend
    * clears the poke at teardown, but a freed instance must still not
    * claim capabilities. */
   if (gl && gl->scrgb.active)
      BIT32_SET(flags, GFX_CTX_FLAGS_SCREEN_10BPC_SOURCE);
#endif
#ifdef HAVE_GL2_XR
   /* While a headset shows it, as the Quest's context says: gl2 draws
    * the frame whole in the window, so touch, overlays and the UI follow
    * it; the headset shows a tracked map's eyes from the frame's views,
    * and an untracked map's screens from the window. */
   if (     gl && gl->desktop_xr && !gl->scrgb.active
         && gl_openxr_alive(gl->desktop_xr->host))
   {
      BIT32_SET(flags, GFX_CTX_FLAGS_VIDEO_VIEWS);
      BIT32_SET(flags, GFX_CTX_FLAGS_VIDEO_VIEWS_FALLBACK);
   }
#endif
#ifdef GL2_VIEWS
   if (gl2 && retro_atomic_load_acquire_int(&gl2->views.allowed))
   {
      BIT32_SET(flags, GFX_CTX_FLAGS_VIDEO_VIEWS);
#if defined(HAVE_VIDEO_WINDOWS) && !(defined(HAVE_OPENXR) && defined(ANDROID))
      /* The context can make screens windows of their own. Not in the
       * Quest's build, whose back buffer is the headset's framebuffer. */
      if (gl2->ctx_driver && gl2->ctx_driver->windows)
         BIT32_SET(flags, GFX_CTX_FLAGS_VIDEO_WINDOWS);
#endif
   }
   if (gl2 && retro_atomic_load_acquire_int(&gl2->views.fallback))
      BIT32_SET(flags, GFX_CTX_FLAGS_VIDEO_VIEWS_FALLBACK);
#endif

   BIT32_SET(flags, GFX_CTX_FLAGS_HARD_SYNC);
   BIT32_SET(flags, GFX_CTX_FLAGS_BLACK_FRAME_INSERTION);
   BIT32_SET(flags, GFX_CTX_FLAGS_MENU_FRAME_FILTERING);
   BIT32_SET(flags, GFX_CTX_FLAGS_SCREENSHOTS_SUPPORTED);
   BIT32_SET(flags, GFX_CTX_FLAGS_OVERLAY_BEHIND_MENU_SUPPORTED);
   /* The PBO readback, issued before the UI; under scRGB the image
    * only lands at the final composite */
#ifdef HAVE_GL_ASYNC_READBACK
#if !defined(HAVE_OPENGLES) && !defined(HAVE_PSGL)
   if (gl && !gl->scrgb.active)
#endif
      BIT32_SET(flags, GFX_CTX_FLAGS_RECORD_GAME_ONLY);
#endif

   return flags;
}

#ifndef GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#endif
#ifndef GL_COMPRESSED_RGBA_BPTC_UNORM
#define GL_COMPRESSED_RGBA_BPTC_UNORM 0x8E8C
#endif

static GLenum gl2_gpu_format(enum texture_gpu_format fmt)
{
   switch (fmt)
   {
      case TEXTURE_GPU_FORMAT_BC1: return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
      case TEXTURE_GPU_FORMAT_BC2: return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
      case TEXTURE_GPU_FORMAT_BC3: return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
      case TEXTURE_GPU_FORMAT_BC7: return GL_COMPRESSED_RGBA_BPTC_UNORM;
      default:                     break;
   }
   return 0;
}

static bool gl2_supports_texture_format(void *data,
      enum texture_gpu_format fmt)
{
   const gl2_t *gl = (const gl2_t*)data;
   switch (fmt)
   {
      /* Converted by a draw into a framebuffer (gl_planar_load) */
      case TEXTURE_GPU_FORMAT_YUV420:
#ifdef HAVE_GL_PLANAR
         return gl && (gl->flags & GL2_FLAG_HAVE_FBO)
            && GL_PLANAR_OK(&gl->planar);
#else
         return false;
#endif
      /* Both answers are fixed at init */
      case TEXTURE_GPU_FORMAT_RGBA16F:
         return gl && gl->fp16_textures;
      /* The linear program shows such a texture as linear scRGB while
       * the backbuffer is HDR, into an RGBA16F layer that keeps its
       * highlights; in SDR there is no linear light. */
      case TEXTURE_GPU_FORMAT_SCRGB:
         return gl && gl->scrgb.fp16_ok;
      /* Desktop GL takes RGB10_A2 from BGRA 2_10_10_10_REV words in
       * gl_load_texture_data and gl2_update_texture_internal; GLES2
       * has neither, and gets the image narrowed. */
      case TEXTURE_GPU_FORMAT_RGB10A2:
#if !defined(HAVE_OPENGLES) && !defined(HAVE_PSGL)
         return true;
#else
         return false;
#endif
      case TEXTURE_GPU_FORMAT_BC1:
      case TEXTURE_GPU_FORMAT_BC2:
      case TEXTURE_GPU_FORMAT_BC3:
         return gl_query_extension("EXT_texture_compression_s3tc")
             || gl_query_extension("ANGLE_texture_compression_dxt5")
             || gl_query_extension("EXT_texture_compression_dxt1");
      case TEXTURE_GPU_FORMAT_BC7:
         return gl_query_extension("ARB_texture_compression_bptc")
             || gl_query_extension("EXT_texture_compression_bptc");
      default:
         break;
   }
   return false;
}

static uintptr_t gl2_upload_texture_compressed(
      const struct texture_compressed *tc,
      enum texture_filter_type filter_type)
{
   GLuint   id   = 0;
   GLenum   ifmt;
   unsigned i;
   GLint    minf;
   GLint    magf;
   bool     nearest;

   if (!tc || tc->num_mips == 0)
      return 0;
   ifmt = gl2_gpu_format(tc->format);
   if (!ifmt)
      return 0;

   nearest = (filter_type == TEXTURE_FILTER_NEAREST
           || filter_type == TEXTURE_FILTER_MIPMAP_NEAREST);
   magf    = nearest ? GL_NEAREST : GL_LINEAR;
   if (tc->num_mips > 1)
      minf = nearest ? GL_NEAREST_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR;
   else
      minf = magf;

   glGenTextures(1, &id);
   glBindTexture(GL_TEXTURE_2D, id);
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

   for (i = 0; i < tc->num_mips; i++)
      glCompressedTexImage2D(GL_TEXTURE_2D, (GLint)i, ifmt,
            (GLsizei)tc->mips[i].width, (GLsizei)tc->mips[i].height, 0,
            (GLsizei)tc->mips[i].size, tc->mips[i].data);

#ifdef GL_TEXTURE_MAX_LEVEL
   /* Not defined by GLES2 / Orbis (PS4) GL headers; only meaningful when
    * the file actually carries a mip chain. */
   if (tc->num_mips > 1)
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL,
            (GLint)(tc->num_mips - 1));
#endif
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minf);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magf);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   glBindTexture(GL_TEXTURE_2D, 0);

   return (uintptr_t)id;
}

#ifdef HAVE_THREADS
typedef struct
{
   gl2_t                           *gl;
   const struct texture_compressed *tc;
   enum texture_filter_type         filter;
} gl2_texture_compressed_cmd_t;

static uintptr_t video_texture_load_wrap_gl2_compressed(void *data)
{
   gl2_texture_compressed_cmd_t *cmd = (gl2_texture_compressed_cmd_t*)data;
   gl2_t                        *gl  = cmd->gl;
   if (gl && gl->ctx_driver->make_current)
      gl->ctx_driver->make_current(false);
   return gl2_upload_texture_compressed(cmd->tc, cmd->filter);
}
#endif

static uintptr_t gl2_load_texture_compressed(void *video_data,
      const struct texture_compressed *tc, bool threaded,
      enum texture_filter_type filter_type)
{
#ifdef HAVE_THREADS
   if (threaded)
   {
      gl2_texture_compressed_cmd_t cmd;
      cmd.gl     = (gl2_t*)video_data;
      cmd.tc     = tc;
      cmd.filter = filter_type;
      return video_thread_texture_handle(&cmd,
            video_texture_load_wrap_gl2_compressed);
   }
#else
   (void)video_data;
   (void)threaded;
#endif
   return gl2_upload_texture_compressed(tc, filter_type);
}

/* The frame textures are made again in the new format, and at the
 * new size when it is larger than theirs, with the render chain's
 * passes around them; the frames' own size follows per frame. On PSGL
 * gl2_init_textures sizes the texture reference buffer again. */
static bool gl2_set_frame_format(void *data, bool rgb32,
      unsigned input_scale)
{
   unsigned side;
   gl2_t *gl = (gl2_t*)data;

   if (     !gl
         || (gl->flags & (GL2_FLAG_HW_RENDER_USE | GL2_FLAG_SHARED_CONTEXT_USE))
         || gl->video_info.source_10bit)
      return false;

   side = RARCH_SCALE_BASE * input_scale;
   if (side > VIDEO_SCALE_W(gl->tex_dims))
   {
      if (!gl2_frame_buffers_grow(gl, side))
         return false;
      gl->video_info.input_scale  = input_scale;
   }
   else if (rgb32 == gl->video_info.rgb32)
      return true;

   gl->video_info.rgb32 = rgb32;
   gl2_set_texture_fmts(gl, rgb32);
   gl2_frame_textures_remake(gl);
   return true;
}

static const video_poke_interface_t gl2_poke_interface = {
   gl2_get_flags,
   gl2_load_texture,
   gl2_unload_texture,
   gl2_set_video_mode,
   NULL, /* refresh_rate - handled by display server */
   NULL, /* set_filtering */
   NULL, /* video_output_size - handled by display server */
   NULL, /* video_output_prev - handled by display server */
   NULL, /* video_output_next - handled by display server */
   gl2_get_current_framebuffer,
   gl2_get_proc_address,
   gl2_set_aspect_ratio,
   gl2_apply_state_changes,
   gl2_set_texture_frame,
   gl2_set_texture_enable,
   font_driver_render_msg,
   gl2_show_mouse,
   NULL, /* grab_mouse_toggle */
   gl2_get_current_shader,
#if defined(HAVE_GL_FRAME_MEM) || defined(GL2_KMS_FRAMES)
   gl2_get_current_software_framebuffer,
#else
   NULL, /* get_current_software_framebuffer */
#endif
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_menu_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_expand_gamut */
   NULL, /* set_hdr_scanlines */
   NULL, /* set_hdr_subpixel_layout */
   gl2_supports_texture_format,
   gl2_load_texture_compressed,
   gl2_present_last,
   gl2_get_last_present_time,
   NULL, /* hw_ring_install: Vulkan-shaped */
   gl2_hw_ring_fence_new,
   gl2_hw_ring_fence_free,
   gl2_hw_ring_fence_signal,
   gl2_hw_ring_fence_wait,
   gl2_hw_ring_capture,
   gl2_hw_ring_present_slot,
   gl2_hw_ring_context_new,
   gl2_hw_ring_context_free,
   gl2_hw_ring_framebuffer,
   gl2_update_texture,
   NULL, /* get_swap_interval_cap */
#ifdef HAVE_GL_TEXTURE_LEND
   gl2_texture_lend,
   gl2_texture_lend_ready,
#else
   NULL, /* texture_lend */
   NULL, /* texture_lend_ready */
#endif
   gl2_get_last_present_wait,
#ifdef GL2_VIEWS
   gl2_set_view_count,
#else
   NULL, /* set_view_count */
#endif
#ifdef HAVE_GL2_XR
   gl2_desktop_xr_context_destroying,
   gl2_desktop_xr_refresh,
#else
   NULL, /* hw_context_destroying */
#if defined(HAVE_OPENXR) && defined(ANDROID) && defined(HAVE_OPENGLES3)
   gl2_get_headset_refresh,
#else
   NULL, /* get_headset_refresh */
#endif
#endif
   gl2_set_frame_format,
#ifdef HAVE_GL_FRAME_MEM
   gl2_frame_mem_alloc,
   gl2_frame_mem_ready,
   gl2_frame_mem_free,
#else
   NULL, /* frame_mem_alloc */
   NULL, /* frame_mem_ready */
   NULL, /* frame_mem_free */
#endif
   NULL, /* get_device */
   NULL, /* import_texture */
   NULL, /* get_last_present_sync */
#if defined(HAVE_GL_ASYNC_READBACK) && !defined(HAVE_OPENGLES) && !defined(HAVE_PSGL)
   gl2_record_read,
#else
   NULL, /* record_read */
#endif
   NULL, /* import_movie_frame */
   NULL, /* get_menu_framebuffer */
   NULL, /* menu_mem_alloc */
   0,    /* menu_mem_pitch_align */
   false, /* texture_lend_per_write */
   NULL, /* driver_pacing_set */
   NULL, /* driver_pacing_wait */
   NULL  /* driver_pacing_mark */
};

static void gl2_get_poke_interface(void *data,
      const video_poke_interface_t **iface)     { *iface = &gl2_poke_interface; }
#ifdef HAVE_GFX_WIDGETS
static bool gl2_gfx_widgets_enabled(void *data) { return true; }
#endif

static bool gl2_has_windowed(void *data)
{
   gl2_t *gl = (gl2_t*)data;
   if (gl && gl->ctx_driver)
      return gl->ctx_driver->has_windowed;
   return false;
}

static bool gl2_focus(void *data)
{
   gl2_t *gl = (gl2_t*)data;
   if (gl && gl->ctx_driver && gl->ctx_driver->has_focus)
      return gl->ctx_driver->has_focus(gl->ctx_data);
   return true;
}

/* CPU-side scRGB -> PQ helpers; same (vulkan-verbatim) math as the
 * other drivers' native HDR read-backs. */
static float gl2_hdr_pq_encode(float v)
{
   const float m1 = 0.1593017578125f, m2 = 78.84375f;
   const float c1 = 0.8359375f, c2 = 18.8515625f, c3 = 18.6875f;
   float yp;
   if (v < 0.0f) v = 0.0f;
   else if (v > 1.0f) v = 1.0f;
   yp = powf(v, m1);
   return powf((c1 + c2 * yp) / (1.0f + c3 * yp), m2);
}

static uint16_t gl2_hdr_scrgb_to_pq16(float scrgb)
{
   float nits = scrgb * 80.0f;
   float pq;
   if (nits < 0.0f) nits = 0.0f;
   else if (nits > 10000.0f) nits = 10000.0f;
   pq = gl2_hdr_pq_encode(nits / 10000.0f);
   if (pq < 0.0f) pq = 0.0f;
   else if (pq > 1.0f) pq = 1.0f;
   return (uint16_t)(pq * 65535.0f + 0.5f);
}

/* Native (no tone-map) HDR read-back of the encoded FP16 scRGB
 * backbuffer, row by row as floats; GL rows arrive bottom-up matching
 * the 48-bit buffer convention. Metadata matches the shared scRGB
 * tagging (PQ transfer, BT.709 primaries, D65, measured cLLI,
 * 1000 / 0.001 nit mastering defaults). */
static bool gl2_read_viewport_hdr(void *data, uint16_t *buffer,
      bool is_idle, struct rpng_hdr_metadata *out_meta)
{
   gl2_t *gl = (gl2_t*)data;
   int      vp_x, vp_y;
   unsigned w, h, x;
   size_t   y;
   float   *row;
   float    max_cll  = 0.0f;
   double   sum_fall = 0.0;

   /* Reads the backbuffer as FP16 scRGB; a PQ one is not */
   if (!gl || !(gl->scrgb.active) || gl->scrgb.pq_out || !buffer)
      return false;

   if (!is_idle)
      video_driver_cached_frame();

   vp_x = (VIDEO_POS_X(gl->vp.pos) > 0) ? VIDEO_POS_X(gl->vp.pos) : 0;
   vp_y = (VIDEO_POS_Y(gl->vp.pos) > 0) ? VIDEO_POS_Y(gl->vp.pos) : 0;
   w    = MIN(VIDEO_SCALE_W(gl->vp.dims), VIDEO_SCALE_W(gl->video_dims));
   h    = MIN(VIDEO_SCALE_H(gl->vp.dims), VIDEO_SCALE_H(gl->video_dims));
   if (!w || !h)
      return false;

   row = (float*)malloc((size_t)w * 4 * sizeof(float));
   if (!row)
      return false;

   gl2_bind_fb(0);
   glPixelStorei(GL_PACK_ALIGNMENT, 4);
#ifndef HAVE_OPENGLES
   glPixelStorei(GL_PACK_ROW_LENGTH, 0);
   glReadBuffer(GL_BACK);
#endif

   for (y = 0; y < h; y++)
   {
      uint16_t *dst = buffer + y * (size_t)w * 3;
      glReadPixels(vp_x, vp_y + (int)y, w, 1, GL_RGBA, GL_FLOAT, row);
      for (x = 0; x < w; x++)
      {
         float r        = row[4 * x + 0];
         float g        = row[4 * x + 1];
         float b        = row[4 * x + 2];
         float lvl;
         dst[3 * x + 0] = gl2_hdr_scrgb_to_pq16(r);
         dst[3 * x + 1] = gl2_hdr_scrgb_to_pq16(g);
         dst[3 * x + 2] = gl2_hdr_scrgb_to_pq16(b);
         lvl = r;
         if (g > lvl)
            lvl = g;
         if (b > lvl)
            lvl = b;
         lvl *= 80.0f;
         if (lvl < 0.0f)
            lvl = 0.0f;
         else if (lvl > 10000.0f)
            lvl = 10000.0f;
         if (lvl > max_cll)
            max_cll = lvl;
         sum_fall += lvl;
      }
   }

   free(row);

   if (out_meta)
   {
      memset(out_meta, 0, sizeof(*out_meta));
      out_meta->colour_primaries      = 1;  /* BT.709 (scRGB) */
      out_meta->transfer_function     = 16; /* SMPTE ST 2084 (PQ) */
      out_meta->matrix_coefficients   = 0;  /* RGB */
      out_meta->video_full_range_flag = 1;
      out_meta->max_cll               = max_cll;
      out_meta->max_fall              = (float)(sum_fall
            / ((double)w * (double)h));
      out_meta->write_mdcv            = 1;
      out_meta->primary_chromaticity[0][0] = 0.640f;
      out_meta->primary_chromaticity[0][1] = 0.330f;
      out_meta->primary_chromaticity[1][0] = 0.300f;
      out_meta->primary_chromaticity[1][1] = 0.600f;
      out_meta->primary_chromaticity[2][0] = 0.150f;
      out_meta->primary_chromaticity[2][1] = 0.060f;
      out_meta->white_point[0] = 0.3127f;
      out_meta->white_point[1] = 0.3290f; /* D65 */
      out_meta->max_luminance  = 1000.0f;
      out_meta->min_luminance  = 0.001f;
   }
   return true;
}

#ifdef HAVE_OPENXR
static bool gl2_set_vr_content_info(void *data,
      const video_vr_content_info_t *info)
{
#ifdef HAVE_GL2_XR
   /* Tracked content is drawn from the map by the headset's host. */
   gl2_t *gl = (gl2_t*)data;
   (void)info;
   return gl && gl->desktop_xr;
#else
   /* VR is only available while the OpenXR context owns a live session;
    * on the flat fallback context it must be reported unavailable. */
   bool live = gl_android_openxr_active();
   (void)data;
   gl_android_openxr_set_stereo(live && info && info->stereo_native);
   return live;
#endif
}
#endif

static font_renderer_t gl2_raster_font = {
   gl2_raster_font_init,
   gl2_raster_font_free,
   gl2_raster_font_render_msg,
   "gl",
   gl2_raster_font_get_glyph,
   gl2_raster_font_bind_block,
   gl2_raster_font_flush_block,
   gl2_raster_font_get_message_width,
   gl2_raster_font_get_line_metrics
};

video_driver_t video_gl2 = {
   gl2_init,
   gl2_frame,
   gl2_set_nonblock_state,
   gl2_alive,
   gl2_focus,
   gl2_suppress_screensaver,
   gl2_has_windowed,
   gl2_set_shader,
   gl2_free,
   "gl",
   gl2_set_viewport_wrapper,
   gl2_set_rotation,
   gl2_viewport_info,
   gl2_read_viewport,
#ifdef HAVE_OVERLAY
   gl2_get_overlay_interface,
#endif
   gl2_get_poke_interface,
   gl2_wrap_type_to_enum,
   NULL, /* shader_load_begin */
   NULL, /* shader_load_step */
#ifdef HAVE_GFX_WIDGETS
   gl2_gfx_widgets_enabled,
#endif
   NULL, /* invalidate_hw_render_cache */
   gl2_read_viewport_hdr,
   &gl2_raster_font,
#ifdef HAVE_OPENXR
   gl2_set_vr_content_info,
#endif
   gl2_read_viewport_bgrx
};

gfx_display_ctx_driver_t gfx_display_ctx_gl = {
   gfx_display_gl2_draw,
   gfx_display_gl2_draw_pipeline,
   gfx_display_gl2_blend_begin,
   gfx_display_gl2_blend_end,
   gfx_display_gl2_get_default_mvp,
   gfx_display_gl2_get_default_vertices,
   gfx_display_gl2_get_default_tex_coords,
   &gl2_raster_font,
   GFX_VIDEO_DRIVER_OPENGL,
   "gl",
   false,
   true,
   gfx_display_gl2_scissor_begin,
   gfx_display_gl2_scissor_end,
#ifdef HAVE_GLSL
   gfx_display_gl2_mesh_draw
#else
   NULL
#endif
};
