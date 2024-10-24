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

#ifdef HAVE_THREADS
#include "../video_thread_wrapper.h"
#endif

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
#endif

#ifndef GL_UNSIGNED_INT_8_8_8_8_REV
#define GL_UNSIGNED_INT_8_8_8_8_REV       0x8367
#endif

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

#define GL_RASTER_FONT_EMIT(c, vx, vy) \
   font_vertex[     2 * (6 * i + c) + 0] = (x + (delta_x + off_x + vx * width) * scale) * inv_win_width; \
   font_vertex[     2 * (6 * i + c) + 1] = (y + (delta_y - off_y - vy * height) * scale) * inv_win_height; \
   font_tex_coords[ 2 * (6 * i + c) + 0] = (tex_x + vx * width) * inv_tex_size_x; \
   font_tex_coords[ 2 * (6 * i + c) + 1] = (tex_y + vy * height) * inv_tex_size_y; \
   font_color[      4 * (6 * i + c) + 0] = color[0]; \
   font_color[      4 * (6 * i + c) + 1] = color[1]; \
   font_color[      4 * (6 * i + c) + 2] = color[2]; \
   font_color[      4 * (6 * i + c) + 3] = color[3]; \
   font_lut_tex_coord[    2 * (6 * i + c) + 0] = gl->coords.lut_tex_coord[0]; \
   font_lut_tex_coord[    2 * (6 * i + c) + 1] = gl->coords.lut_tex_coord[1]

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

#elif (defined(__MACH__) && defined(MAC_OS_X_VERSION_MAX_ALLOWED) && (MAC_OS_X_VERSION_MAX_ALLOWED < 101200))
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

enum gl2_renderchain_flags
{
   GL2_CHAIN_FLAG_EGL_IMAGES           = (1 << 0),
   GL2_CHAIN_FLAG_HAS_FP_FBO           = (1 << 1),
   GL2_CHAIN_FLAG_HAS_SRGB_FBO_GLES3   = (1 << 2),
   GL2_CHAIN_FLAG_HAS_SRGB_FBO         = (1 << 3),
   GL2_CHAIN_FLAG_HW_RENDER_DEPTH_INIT = (1 << 4)
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
   unsigned tex_width, tex_height;

   const font_renderer_driver_t *font_driver;
   void *font_data;
   struct font_atlas *atlas;

   video_font_raster_block_t *block;
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
      unsigned viewport_width,
      unsigned viewport_height,
      bool force_full, bool allow_rotate);

#ifdef IOS
/* There is no default frame buffer on iOS. */
void glkitview_bind_fbo(void);
#define gl2_renderchain_bind_backbuffer() glkitview_bind_fbo()
#else
#define gl2_renderchain_bind_backbuffer() gl2_bind_fb(0)
#endif

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

static GLenum gfx_display_prim_to_gl_enum(
      enum gfx_display_prim_type type)
{
   switch (type)
   {
      case GFX_DISPLAY_PRIM_TRIANGLESTRIP:
         return GL_TRIANGLE_STRIP;
      case GFX_DISPLAY_PRIM_TRIANGLES:
         return GL_TRIANGLES;
      case GFX_DISPLAY_PRIM_NONE:
      default:
         break;
   }

   return 0;
}

static void gfx_display_gl2_blend_begin(void *data)
{
   gl2_t             *gl          = (gl2_t*)data;

   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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
      unsigned width, unsigned height)
{
   static bool mali_4xx_detected     = false;
   static bool scissor_inited        = false;
   static unsigned last_video_width  = 0;
   static unsigned last_video_height = 0;

   if (!scissor_inited)
   {
      unsigned i;
      scissor_inited                = true;
      const char *gpu_device_string = gl->device_str;

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

      last_video_width  = width;
      last_video_height = height;
   }

   /* Early out, to minimise performance impact on
    * non-mali_4xx devices */
   if (!mali_4xx_detected)
      return false;

   /* Have to update scissor_set_rectangle() if the
    * video dimensions change */
   if (   (width  != last_video_width)
       || (height != last_video_height))
   {
      scissor_set_rectangle(0,
            width - 1,
            0,
            height - 1,
            0);

      last_video_width  = width;
      last_video_height = height;
   }

   /* Discards not only out-of-bounds scissoring,
    * but also out-of-view draws.
    *
    * This is intentional.
    */
   return scissor_is_outside_rectangle(
         draw->x, draw->x + draw->width - 1,
         draw->y, draw->y + draw->height - 1);
}
#endif

static void gfx_display_gl2_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height)
{
   gl2_t             *gl  = (gl2_t*)data;

   if (!gl || !draw)
      return;

#ifdef MALI_BUG
   if (gfx_display_gl2_discard_draw_rectangle(gl, draw, video_width,
            video_height))
   {
      /*RARCH_WARN("[Menu]: discarded draw rect: %.4i %.4i %.4i %.4i\n",
        (int)draw->x, (int)draw->y, (int)draw->width, (int)draw->height);*/
      return;
   }
#endif

   if (!draw->coords->vertex)
      draw->coords->vertex        = &gl2_vertexes[0];
   if (!draw->coords->tex_coord)
      draw->coords->tex_coord     = &gl2_tex_coords[0];
   if (!draw->coords->lut_tex_coord)
      draw->coords->lut_tex_coord = &gl2_tex_coords[0];

   glViewport(draw->x, draw->y, draw->width, draw->height);
   glBindTexture(GL_TEXTURE_2D, (GLuint)draw->texture);

   gl->shader->set_coords(gl->shader_data, draw->coords);
   gl->shader->set_mvp(gl->shader_data,
         draw->matrix_data ? (math_matrix_4x4*)draw->matrix_data
      : (math_matrix_4x4*)&gl->mvp_no_rot);


   glDrawArrays(gfx_display_prim_to_gl_enum(
            draw->prim_type), 0, draw->coords->vertices);

   gl->coords.color     = gl->white_color_ptr;
}

static void gfx_display_gl2_draw_pipeline(
      gfx_display_ctx_draw_t *draw,
      gfx_display_t *p_disp,
      void *data,
      unsigned video_width,
      unsigned video_height)
{
#ifdef HAVE_SHADERPIPELINE
   struct uniform_info uniform_param;
   gl2_t             *gl            = (gl2_t*)data;
   static float t                   = 0;
   video_coord_array_t *ca          = &p_disp->dispca;

   draw->x                          = 0;
   draw->y                          = 0;
   draw->coords                     = (struct video_coords*)(&ca->coords);
   draw->matrix_data                = NULL;

   switch (draw->pipeline_id)
   {
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
         glBlendFunc(GL_DST_COLOR, GL_ONE);
         break;
      default:
         glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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

         t += 0.01;

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
         uniform_param.result.f.v0       = draw->width;
         uniform_param.result.f.v1       = draw->height;

         gl->shader->set_uniform_parameter(gl->shader_data,
               &uniform_param, NULL);
#endif
         break;
   }
#endif
}

static void gfx_display_gl2_scissor_begin(
      void *data,
      unsigned video_width,
      unsigned video_height,
      int x, int y,
      unsigned width, unsigned height)
{
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

static void gfx_display_gl2_scissor_end(
      void *data,
      unsigned video_width,
      unsigned video_height)
{
   glScissor(0, 0, video_width, video_height);
   glDisable(GL_SCISSOR_TEST);
#ifdef MALI_BUG
   scissor_set_rectangle(0, video_width - 1, 0, video_height - 1, 0);
#endif
}

gfx_display_ctx_driver_t gfx_display_ctx_gl = {
   gfx_display_gl2_draw,
   gfx_display_gl2_draw_pipeline,
   gfx_display_gl2_blend_begin,
   gfx_display_gl2_blend_end,
   gfx_display_gl2_get_default_mvp,
   gfx_display_gl2_get_default_vertices,
   gfx_display_gl2_get_default_tex_coords,
   FONT_DRIVER_RENDER_OPENGL_API,
   GFX_VIDEO_DRIVER_OPENGL,
   "gl",
   false,
   gfx_display_gl2_scissor_begin,
   gfx_display_gl2_scissor_end
};

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

static void gl2_raster_font_upload_atlas(gl2_raster_t *font)
{
   int i, j;
   GLint  gl_internal          = GL_LUMINANCE_ALPHA;
   GLenum gl_format            = GL_LUMINANCE_ALPHA;
   size_t ncomponents          = 2;
   uint8_t *tmp                = (uint8_t*)calloc(font->tex_height, font->tex_width * ncomponents);

   switch (ncomponents)
   {
      case 1:
         for (i = 0; i < (int)font->atlas->height; ++i)
         {
            const uint8_t *src = &font->atlas->buffer[i * font->atlas->width];
            uint8_t       *dst = &tmp[i * font->tex_width * ncomponents];

            memcpy(dst, src, font->atlas->width);
         }
         break;
      case 2:
         for (i = 0; i < (int)font->atlas->height; ++i)
         {
            const uint8_t *src = &font->atlas->buffer[i * font->atlas->width];
            uint8_t       *dst = &tmp[i * font->tex_width * ncomponents];

            for (j = 0; j < (int)font->atlas->width; ++j)
            {
               *dst++ = 0xff;
               *dst++ = *src++;
            }
         }
         break;
   }

   glTexImage2D(GL_TEXTURE_2D, 0, gl_internal,
         font->tex_width, font->tex_height,
         0, gl_format, GL_UNSIGNED_BYTE, tmp);

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
            &font->font_data, font_path, font_size))
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
   font->tex_width  = next_pow2(font->atlas->width);
   font->tex_height = next_pow2(font->atlas->height);

   gl2_raster_font_upload_atlas(font);

   font->atlas->dirty = false;

   if (font->gl)
      glBindTexture(GL_TEXTURE_2D, font->gl->texture[font->gl->tex_index]);

   return font;
}

static int gl2_raster_font_get_message_width(void *data, const char *msg,
      size_t msg_len, float scale)
{
   const struct font_glyph* glyph_q = NULL;
   gl2_raster_t *font               = (gl2_raster_t*)data;
   const char* msg_end              = msg + msg_len;
   int delta_x                      = 0;

   if (     !font
         || !font->font_driver
         || !font->font_data )
      return 0;

   glyph_q = font->font_driver->get_glyph(font->font_data, '?');

   while (msg < msg_end)
   {
      const struct font_glyph *glyph;
      unsigned code                  = utf8_walk(&msg);

      /* Do something smarter here ... */
      if (!(glyph = font->font_driver->get_glyph(
            font->font_data, code)))
         if (!(glyph = glyph_q))
            continue;

      delta_x += glyph->advance_x;
   }

   return delta_x * scale;
}

static void gl2_raster_font_draw_vertices(gl2_t *gl,
      gl2_raster_t *font,
      const video_coords_t *coords)
{
   if (font->atlas->dirty)
   {
      gl2_raster_font_upload_atlas(font);
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

static void gl2_raster_font_render_line(gl2_t *gl,
      gl2_raster_t *font, const char *msg, size_t msg_len,
      GLfloat scale, const GLfloat color[4], GLfloat pos_x,
      GLfloat pos_y, unsigned text_align)
{
   int i;
   struct video_coords coords;
   const struct font_glyph* glyph_q = NULL;
   GLfloat font_tex_coords[2 * 6 * MAX_MSG_LEN_CHUNK];
   GLfloat font_vertex[2 * 6 * MAX_MSG_LEN_CHUNK];
   GLfloat font_color[4 * 6 * MAX_MSG_LEN_CHUNK];
   GLfloat font_lut_tex_coord[2 * 6 * MAX_MSG_LEN_CHUNK];
   const char* msg_end  = msg + msg_len;
   int x                = roundf(pos_x * gl->vp.width);
   int y                = roundf(pos_y * gl->vp.height);
   int delta_x          = 0;
   int delta_y          = 0;
   float inv_tex_size_x = 1.0f / font->tex_width;
   float inv_tex_size_y = 1.0f / font->tex_height;
   float inv_win_width  = 1.0f / gl->vp.width;
   float inv_win_height = 1.0f / gl->vp.height;

   switch (text_align)
   {
      case TEXT_ALIGN_RIGHT:
         x -= gl2_raster_font_get_message_width(font, msg, msg_len, scale);
         break;
      case TEXT_ALIGN_CENTER:
         x -= gl2_raster_font_get_message_width(font, msg, msg_len, scale) / 2.0;
         break;
   }

   glyph_q = font->font_driver->get_glyph(font->font_data, '?');

   while (msg < msg_end)
   {
      i = 0;
      while ((i < MAX_MSG_LEN_CHUNK) && (msg < msg_end))
      {
         const struct font_glyph *glyph;
         int off_x, off_y, tex_x, tex_y, width, height;
         unsigned                  code = utf8_walk(&msg);

         /* Do something smarter here ... */
         if (!(glyph = font->font_driver->get_glyph(
               font->font_data, code)))
            if (!(glyph = glyph_q))
               continue;

         off_x  = glyph->draw_offset_x;
         off_y  = glyph->draw_offset_y;
         tex_x  = glyph->atlas_offset_x;
         tex_y  = glyph->atlas_offset_y;
         width  = glyph->width;
         height = glyph->height;

         GL_RASTER_FONT_EMIT(0, 0, 1); /* Bottom-left */
         GL_RASTER_FONT_EMIT(1, 1, 1); /* Bottom-right */
         GL_RASTER_FONT_EMIT(2, 0, 0); /* Top-left */

         GL_RASTER_FONT_EMIT(3, 1, 0); /* Top-right */
         GL_RASTER_FONT_EMIT(4, 0, 0); /* Top-left */
         GL_RASTER_FONT_EMIT(5, 1, 1); /* Bottom-right */

         i++;

         delta_x += glyph->advance_x;
         delta_y -= glyph->advance_y;
      }

      coords.tex_coord     = font_tex_coords;
      coords.vertex        = font_vertex;
      coords.color         = font_color;
      coords.vertices      = i * 6;
      coords.lut_tex_coord = font_lut_tex_coord;

      if (font->block)
         video_coord_array_append(&font->block->carr,
			 &coords, coords.vertices);
      else
         gl2_raster_font_draw_vertices(gl, font, &coords);
   }
}

static void gl2_raster_font_render_message(gl2_t *gl,
      gl2_raster_t *font, const char *msg, GLfloat scale,
      const GLfloat color[4], GLfloat pos_x, GLfloat pos_y,
      unsigned text_align)
{
   float line_height;
   struct font_line_metrics *line_metrics = NULL;
   int lines                              = 0;
   font->font_driver->get_line_metrics(font->font_data, &line_metrics);
   line_height = line_metrics->height * scale / gl->vp.height;

   for (;;)
   {
      const char *delim = strchr(msg, '\n');
      size_t msg_len    = delim ? (size_t)(delim - msg) : strlen(msg);

      /* Draw the line */
      gl2_raster_font_render_line(gl, font,
            msg, msg_len, scale, color, pos_x,
            pos_y - (float)lines*line_height, text_align);

      if (!delim)
         break;

      msg += msg_len + 1;
      lines++;
   }
}

static void gl2_raster_font_setup_viewport(
      gl2_t *gl,
      gl2_raster_t *font,
      unsigned width, unsigned height,
      bool full_screen)
{
   gl2_set_viewport(gl, width, height, full_screen, true);

   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glBlendEquation(GL_FUNC_ADD);

   glBindTexture(GL_TEXTURE_2D, font->tex);

   if (gl->shader && gl->shader->use)
      gl->shader->use(gl,
            gl->shader_data, VIDEO_SHADER_STOCK_BLEND, true);
}

static void gl2_raster_font_render_msg(
      void *userdata,
      void *data,
      const char *msg,
      const struct font_params *params)
{
   GLfloat color[4];
   int drop_x, drop_y;
   GLfloat x, y, scale, drop_mod, drop_alpha;
   enum text_alignment text_align    = TEXT_ALIGN_LEFT;
   bool full_screen                  = false ;
   gl2_raster_t                *font = (gl2_raster_t*)data;
   gl2_t *gl                         = (gl2_t*)userdata;
   unsigned width                    = gl->video_width;
   unsigned height                   = gl->video_height;

   if (!font || string_is_empty(msg) || !gl)
      return;

   if (params)
   {
      x           = params->x;
      y           = params->y;
      scale       = params->scale;
      full_screen = params->full_screen;
      text_align  = params->text_align;
      drop_x      = params->drop_x;
      drop_y      = params->drop_y;
      drop_mod    = params->drop_mod;
      drop_alpha  = params->drop_alpha;

      color[0]    = FONT_COLOR_GET_RED(params->color)   / 255.0f;
      color[1]    = FONT_COLOR_GET_GREEN(params->color) / 255.0f;
      color[2]    = FONT_COLOR_GET_BLUE(params->color)  / 255.0f;
      color[3]    = FONT_COLOR_GET_ALPHA(params->color) / 255.0f;

      /* If alpha is 0.0f, turn it into default 1.0f */
      if (color[3] <= 0.0f)
         color[3] = 1.0f;
   }
   else
   {
      settings_t *settings     = config_get_ptr();
      float video_msg_pos_x    = settings->floats.video_msg_pos_x;
      float video_msg_pos_y    = settings->floats.video_msg_pos_y;
      float video_msg_color_r  = settings->floats.video_msg_color_r;
      float video_msg_color_g  = settings->floats.video_msg_color_g;
      float video_msg_color_b  = settings->floats.video_msg_color_b;
      x                        = video_msg_pos_x;
      y                        = video_msg_pos_y;
      scale                    = 1.0f;
      full_screen              = true;
      text_align               = TEXT_ALIGN_LEFT;

      color[0]                 = video_msg_color_r;
      color[1]                 = video_msg_color_g;
      color[2]                 = video_msg_color_b;
      color[3]                 = 1.0f;

      drop_x                   = -2;
      drop_y                   = -2;
      drop_mod                 = 0.3f;
      drop_alpha               = 1.0f;
   }

   if (font->block)
      font->block->fullscreen  = full_screen;
   else
      gl2_raster_font_setup_viewport(gl, font, width, height, full_screen);

   if (    !string_is_empty(msg)
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

         gl2_raster_font_render_message(gl, font, msg, scale, color_dark,
               x + scale * drop_x / gl->vp.width,
               y + scale * drop_y / gl->vp.height,
               text_align);
      }

      gl2_raster_font_render_message(gl, font, msg, scale, color,
            x, y, text_align);
   }

   if (!font->block)
   {
      /* Restore viewport */
      glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
      glDisable(GL_BLEND);
      gl2_set_viewport(gl, width, height, false, true);
   }
}

static const struct font_glyph *gl2_raster_font_get_glyph(
      void *data, uint32_t code)
{
   gl2_raster_t *font = (gl2_raster_t*)data;
   if (font && font->font_driver)
      return font->font_driver->get_glyph((void*)font->font_driver, code);
   return NULL;
}

static void gl2_raster_font_flush_block(unsigned width, unsigned height,
      void *data)
{
   gl2_raster_t          *font       = (gl2_raster_t*)data;
   video_font_raster_block_t *block  = font ? font->block : NULL;
   gl2_t *gl                         = font ? font->gl : NULL;

   if (!font || !block || !block->carr.coords.vertices || !gl)
      return;

   gl2_raster_font_setup_viewport(gl, font, width, height, block->fullscreen);
   gl2_raster_font_draw_vertices(gl, font, (video_coords_t*)&block->carr.coords);

   /* Restore viewport */
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

   glDisable(GL_BLEND);
   gl2_set_viewport(gl, width, height, block->fullscreen, true);
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

font_renderer_t gl2_raster_font = {
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
#if !defined(HAVE_PSGL) && !defined(ORBIS) && !defined(VITA) && !defined(IOS)
#ifdef HAVE_OPENGLES2
   enum gl_capability_enum cap = GL_CAPS_TEX_STORAGE_EXT;
#else
   enum gl_capability_enum cap = GL_CAPS_TEX_STORAGE;
#endif

   if (gl_check_capability(cap) && internalFormat != GL_BGRA_EXT)
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

static bool gl2_recreate_fbo(
      struct video_fbo_rect *fbo_rect,
      GLuint fbo,
      GLuint* texture
      )
{
   gl2_bind_fb(fbo);
   glDeleteTextures(1, texture);
   glGenTextures(1, texture);
   glBindTexture(GL_TEXTURE_2D, *texture);
   gl2_load_texture_image(GL_TEXTURE_2D,
         0, RARCH_GL_INTERNAL_FORMAT32,
         fbo_rect->width,
         fbo_rect->height,
         0, RARCH_GL_TEXTURE_TYPE32,
         RARCH_GL_FORMAT32, NULL);

   gl2_fb_texture_2d(RARCH_GL_FRAMEBUFFER,
         RARCH_GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
         *texture, 0);

   if (gl2_check_fb_status(RARCH_GL_FRAMEBUFFER)
         == RARCH_GL_FRAMEBUFFER_COMPLETE)
      return true;

   RARCH_WARN("[GL]: Failed to reinitialize FBO texture.\n");
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
      unsigned viewport_width,
      unsigned viewport_height,
      bool force_full, bool allow_rotate)
{
   settings_t *settings     = config_get_ptr();
   float device_aspect = (float) viewport_width / (float)viewport_height;

   if (gl->ctx_driver->translate_aspect)
      device_aspect         = gl->ctx_driver->translate_aspect(
            gl->ctx_data, viewport_width, viewport_height);

   if (settings->bools.video_scale_integer && !force_full)
   {
      video_viewport_get_scaled_integer(&gl->vp,
            viewport_width, viewport_height,
            video_driver_get_aspect_ratio(),
            (gl->flags & GL2_FLAG_KEEP_ASPECT) ? true : false,
            false);
      viewport_width  = gl->vp.width;
      viewport_height = gl->vp.height;
   }
   else if ((gl->flags & GL2_FLAG_KEEP_ASPECT) && !force_full)
   {
      gl->vp.full_height = gl->video_height;
      video_viewport_get_scaled_aspect2(&gl->vp, viewport_width, viewport_height, false, device_aspect, video_driver_get_aspect_ratio());
      viewport_width  = gl->vp.width;
      viewport_height = gl->vp.height;
   }
   else
   {
      gl->vp.x      = gl->vp.y = 0;
      gl->vp.width  = viewport_width;
      gl->vp.height = viewport_height;
   }

   glViewport(gl->vp.x, gl->vp.y, gl->vp.width, gl->vp.height);
   gl2_set_projection(gl, &default_ortho, allow_rotate);

   /* Set last backbuffer viewport. */
   if (!force_full)
   {
      gl->vp_out_width  = viewport_width;
      gl->vp_out_height = viewport_height;
   }

#if 0
   RARCH_LOG("Setting viewport @ %ux%u\n", viewport_width, viewport_height);
#endif
}

static void gl2_renderchain_render(
      gl2_t *gl,
      gl2_renderchain_data_t *chain,
      uint64_t frame_count,
      const struct video_tex_info *tex_info,
      const struct video_tex_info *feedback_info)
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
   unsigned width                         = gl->video_width;
   unsigned height                        = gl->video_height;

   /* Render the rest of our passes. */
   gl->coords.tex_coord      = fbo_tex_coords;

   /* Calculate viewports, texture coordinates etc,
    * and render all passes from FBOs, to another FBO. */
   for (i = 1; i < chain->fbo_pass; i++)
   {
      const struct video_fbo_rect *rect = &gl->fbo_rect[i];

      prev_rect = &gl->fbo_rect[i - 1];
      fbo_info  = &fbo_tex_info[i - 1];

      xamt      = (GLfloat)prev_rect->img_width / prev_rect->width;
      yamt      = (GLfloat)prev_rect->img_height / prev_rect->height;

      SET_TEXTURE_COORDS(fbo_tex_coords, xamt, yamt);

      fbo_info->tex           = chain->fbo_texture[i - 1];
      fbo_info->input_size[0] = prev_rect->img_width;
      fbo_info->input_size[1] = prev_rect->img_height;
      fbo_info->tex_size[0]   = prev_rect->width;
      fbo_info->tex_size[1]   = prev_rect->height;
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
            rect->img_width, rect->img_height, true, false);

      params.data          = gl;
      params.width         = prev_rect->img_width;
      params.height        = prev_rect->img_height;
      params.tex_width     = prev_rect->width;
      params.tex_height    = prev_rect->height;
      params.out_width     = gl->vp.width;
      params.out_height    = gl->vp.height;
      params.frame_counter = (unsigned int)frame_count;
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
   xamt      = (GLfloat)prev_rect->img_width / prev_rect->width;
   yamt      = (GLfloat)prev_rect->img_height / prev_rect->height;

   SET_TEXTURE_COORDS(fbo_tex_coords, xamt, yamt);

   /* Push final FBO to list. */
   fbo_info                = &fbo_tex_info[chain->fbo_pass - 1];

   fbo_info->tex           = chain->fbo_texture[chain->fbo_pass - 1];
   fbo_info->input_size[0] = prev_rect->img_width;
   fbo_info->input_size[1] = prev_rect->img_height;
   fbo_info->tex_size[0]   = prev_rect->width;
   fbo_info->tex_size[1]   = prev_rect->height;
   memcpy(fbo_info->coord, fbo_tex_coords, sizeof(fbo_tex_coords));
   fbo_tex_info_cnt++;

   /* Render our FBO texture to back buffer. */
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
   gl2_set_viewport(gl,
         width, height, false, true);

   params.data          = gl;
   params.width         = prev_rect->img_width;
   params.height        = prev_rect->img_height;
   params.tex_width     = prev_rect->width;
   params.tex_height    = prev_rect->height;
   params.out_width     = gl->vp.width;
   params.out_height    = gl->vp.height;
   params.frame_counter = (unsigned int)frame_count;
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

static void gl2_renderchain_deinit_hw_render(gl2_t *gl, gl2_renderchain_data_t *chain)
{
   if (gl->flags    & GL2_FLAG_SHARED_CONTEXT_USE)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, true);
   if (gl->flags    & GL2_FLAG_HW_RENDER_FBO_INIT)
      gl2_delete_fb(gl->textures, gl->hw_render_fbo);
   if (chain->flags & GL2_CHAIN_FLAG_HW_RENDER_DEPTH_INIT)
      gl2_delete_rb(gl->textures, chain->hw_render_depth);
   gl->flags &= ~GL2_FLAG_HW_RENDER_FBO_INIT;

   if (gl->flags    & GL2_FLAG_SHARED_CONTEXT_USE)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, false);
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
   RARCH_ERR("[GL]: Failed to set up frame buffer objects. Multi-pass shading will not work.\n");
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
      unsigned i, GLuint texture)
{
   GLenum mag_filter, wrap_enum;
   enum gfx_wrap_type wrap_type;
   bool fp_fbo                   = false;
   bool smooth                   = false;
   settings_t *settings          = config_get_ptr();
   bool video_smooth             = settings->bools.video_smooth;
#if HAVE_ODROIDGO2
   bool video_ctx_scaling         = settings->bools.video_ctx_scaling;
   if (video_ctx_scaling)
       video_smooth = false;
#endif
#ifndef HAVE_OPENGLES
   bool force_srgb_disable       = settings->bools.video_force_srgb_disable;
#endif
   GLuint base_filt              = video_smooth ? GL_LINEAR : GL_NEAREST;
   GLuint base_mip_filt          = video_smooth ?
      GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST;
   unsigned mip_level            = i + 2;
   bool mipmapped                = gl->shader->mipmap_input(gl->shader_data, mip_level);
   GLenum min_filter             = mipmapped ? base_mip_filt : base_filt;

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
         RARCH_ERR("[GL]: Floating-point FBO was requested, but is not supported. Falling back to UNORM. Result may band/clip/etc.!\n");
   }

#if !defined(HAVE_OPENGLES2)
   if (     fp_fbo
         && (chain->flags & GL2_CHAIN_FLAG_HAS_FP_FBO))
   {
      RARCH_LOG("[GL]: FBO pass #%d is floating-point.\n", i);
      gl2_load_texture_image(GL_TEXTURE_2D, 0, GL_RGBA32F,
         gl->fbo_rect[i].width, gl->fbo_rect[i].height,
         0, GL_RGBA, GL_FLOAT, NULL);
   }
   else
#endif
   {
#ifndef HAVE_OPENGLES
      bool srgb_fbo = (chain->fbo_scale[i].flags & FBO_SCALE_FLAG_SRGB_FBO) ? true : false;

      if (!fp_fbo && srgb_fbo)
      {
         if (!(chain->flags & GL2_CHAIN_FLAG_HAS_SRGB_FBO))
               RARCH_ERR("[GL]: sRGB FBO was requested, but it is not supported. Falling back to UNORM. Result may have banding!\n");
      }

      if (force_srgb_disable)
         srgb_fbo = false;

      if (      srgb_fbo
            && (chain->flags & GL2_CHAIN_FLAG_HAS_SRGB_FBO))
      {
         RARCH_LOG("[GL]: FBO pass #%d is sRGB.\n", i);
#ifdef HAVE_OPENGLES2
         /* EXT defines are same as core GLES3 defines,
          * but GLES3 variant requires different arguments. */
         glTexImage2D(GL_TEXTURE_2D,
               0, GL_SRGB_ALPHA_EXT,
               gl->fbo_rect[i].width, gl->fbo_rect[i].height, 0,
               (chain->flags & GL2_CHAIN_FLAG_HAS_SRGB_FBO_GLES3)
               ? GL_RGBA
               : GL_SRGB_ALPHA_EXT,
               GL_UNSIGNED_BYTE, NULL);
#else
         gl2_load_texture_image(GL_TEXTURE_2D,
            0, GL_SRGB8_ALPHA8,
            gl->fbo_rect[i].width, gl->fbo_rect[i].height, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, NULL);
#endif
      }
      else
#endif
      {
#if defined(HAVE_OPENGLES2)
         glTexImage2D(GL_TEXTURE_2D,
               0, GL_RGBA,
               gl->fbo_rect[i].width, gl->fbo_rect[i].height, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, NULL);
#elif defined(HAVE_PSGL)
         glTexImage2D(GL_TEXTURE_2D,
               0, GL_ARGB_SCE,
               gl->fbo_rect[i].width, gl->fbo_rect[i].height, 0,
               GL_ARGB_SCE, GL_UNSIGNED_BYTE, NULL);
#else
         /* Avoid potential performance
          * reductions on particular platforms. */
         gl2_load_texture_image(GL_TEXTURE_2D,
            0, RARCH_GL_INTERNAL_FORMAT32,
            gl->fbo_rect[i].width, gl->fbo_rect[i].height, 0,
            RARCH_GL_TEXTURE_TYPE32, RARCH_GL_FORMAT32, NULL);
#endif
      }
   }
}

static void gl2_create_fbo_textures(gl2_t *gl,
      gl2_renderchain_data_t *chain)
{
   int i;

   glGenTextures(chain->fbo_pass, chain->fbo_texture);

   for (i = 0; i < chain->fbo_pass; i++)
      gl2_create_fbo_texture(gl,
            (gl2_renderchain_data_t*)gl->renderchain_data,
            i, chain->fbo_texture[i]);

   if (gl->flags & GL2_FLAG_FBO_FEEDBACK_ENABLE)
   {
      glGenTextures(1, &gl->fbo_feedback_texture);
      gl2_create_fbo_texture(gl,
            (gl2_renderchain_data_t*)gl->renderchain_data,
            gl->fbo_feedback_pass, gl->fbo_feedback_texture);
   }

   glBindTexture(GL_TEXTURE_2D, 0);
}

/* Compute FBO geometry.
 * When width/height changes or window sizes change,
 * we have to recalculate geometry of our FBO. */

static void gl2_renderchain_recompute_pass_sizes(
      gl2_t *gl,
      gl2_renderchain_data_t *chain,
      unsigned width, unsigned height,
      unsigned vp_width, unsigned vp_height)
{
   size_t i;
   bool size_modified       = false;
   GLint max_size           = 0;
   unsigned last_width      = width;
   unsigned last_height     = height;
   unsigned last_max_width  = gl->tex_w;
   unsigned last_max_height = gl->tex_h;

   glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);

   /* Calculate viewports for FBOs. */
   for (i = 0; i < (size_t)chain->fbo_pass; i++)
   {
      struct video_fbo_rect  *fbo_rect   = &gl->fbo_rect[i];
      struct gfx_fbo_scale *fbo_scale    = &chain->fbo_scale[i];

      switch (fbo_scale->type_x)
      {
         case RARCH_SCALE_INPUT:
            fbo_rect->img_width      = fbo_scale->scale_x * last_width;
            fbo_rect->max_img_width  = last_max_width     * fbo_scale->scale_x;
            break;

         case RARCH_SCALE_ABSOLUTE:
            fbo_rect->img_width      = fbo_rect->max_img_width =
               fbo_scale->abs_x;
            break;

         case RARCH_SCALE_VIEWPORT:
            if (gl->rotation % 180 == 90)
            {
               fbo_rect->img_width      = fbo_rect->max_img_width =
               fbo_scale->scale_x * vp_height;
            } else {
               fbo_rect->img_width      = fbo_rect->max_img_width =
               fbo_scale->scale_x * vp_width;
            }
            break;
      }

      switch (fbo_scale->type_y)
      {
         case RARCH_SCALE_INPUT:
            fbo_rect->img_height     = last_height * fbo_scale->scale_y;
            fbo_rect->max_img_height = last_max_height * fbo_scale->scale_y;
            break;

         case RARCH_SCALE_ABSOLUTE:
            fbo_rect->img_height     = fbo_scale->abs_y;
            fbo_rect->max_img_height = fbo_scale->abs_y;
            break;

         case RARCH_SCALE_VIEWPORT:
            if (gl->rotation % 180 == 90)
            {
               fbo_rect->img_height      = fbo_rect->max_img_height =
               fbo_scale->scale_y * vp_width;
            } else {
            fbo_rect->img_height     = fbo_rect->max_img_height =
               fbo_scale->scale_y * vp_height;
            }
            break;
      }

      if (fbo_rect->img_width > (unsigned)max_size)
      {
         size_modified            = true;
         fbo_rect->img_width      = max_size;
      }

      if (fbo_rect->img_height > (unsigned)max_size)
      {
         size_modified            = true;
         fbo_rect->img_height     = max_size;
      }

      if (fbo_rect->max_img_width > (unsigned)max_size)
      {
         size_modified            = true;
         fbo_rect->max_img_width  = max_size;
      }

      if (fbo_rect->max_img_height > (unsigned)max_size)
      {
         size_modified            = true;
         fbo_rect->max_img_height = max_size;
      }

      if (size_modified)
         RARCH_WARN("[GL]: FBO textures exceeded maximum size of GPU (%dx%d). Resizing to fit.\n", max_size, max_size);

      last_width      = fbo_rect->img_width;
      last_height     = fbo_rect->img_height;
      last_max_width  = fbo_rect->max_img_width;
      last_max_height = fbo_rect->max_img_height;
   }
}

static void gl2_renderchain_start_render(
      gl2_t *gl,
      gl2_renderchain_data_t *chain)
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
         gl->fbo_rect[0].img_width,
         gl->fbo_rect[0].img_height, true, false);

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
      gl2_renderchain_data_t *chain,
      unsigned fbo_width, unsigned fbo_height)
{
   int i;
   unsigned width, height;
   video_shader_ctx_scale_t scaler;
   unsigned shader_info_num;
   struct gfx_fbo_scale scale, scale_last;

   shader_info_num = gl->shader->num_shaders(gl->shader_data);

   if (!gl || shader_info_num == 0)
      return;

   width        = gl->video_width;
   height       = gl->video_height;

   scaler.scale = &scale;

   gl2_shader_scale(gl, &scaler, 1);

   scaler.scale = &scale_last;

   gl2_shader_scale(gl, &scaler, shader_info_num);

   /* we always want FBO to be at least initialized on startup for consoles */
   if (      shader_info_num == 1
         && (!(scale.flags & FBO_SCALE_FLAG_VALID)))
      return;

   if (!(gl->flags & GL2_FLAG_HAVE_FBO))
   {
      RARCH_ERR("[GL]: Failed to locate FBO functions. Won't be able to use render-to-texture.\n");
      return;
   }

   chain->fbo_pass = shader_info_num - 1;
   if (scale_last.flags & FBO_SCALE_FLAG_VALID)
      chain->fbo_pass++;

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

   gl2_renderchain_recompute_pass_sizes(gl,
         chain, fbo_width, fbo_height, width, height);

   for (i = 0; i < chain->fbo_pass; i++)
   {
      gl->fbo_rect[i].width  = next_pow2(gl->fbo_rect[i].img_width);
      gl->fbo_rect[i].height = next_pow2(gl->fbo_rect[i].img_height);
      RARCH_LOG("[GL]: Creating FBO %d @ %ux%u.\n", i,
            gl->fbo_rect[i].width, gl->fbo_rect[i].height);
   }

   if (gl->shader->get_feedback_pass(gl->shader_data, &gl->fbo_feedback_pass))
   {
      if (gl->fbo_feedback_pass < (unsigned)chain->fbo_pass)
      {
         RARCH_LOG("[GL]: Creating feedback FBO %d @ %ux%u.\n", i,
               gl->fbo_rect[gl->fbo_feedback_pass].width,
               gl->fbo_rect[gl->fbo_feedback_pass].height);
         gl->flags |=  GL2_FLAG_FBO_FEEDBACK_ENABLE;
      }
      else
      {
         RARCH_WARN("[GL]: Tried to create feedback FBO of pass #%u, but there are only %d FBO passes. Will use input texture as feedback texture.\n",
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
      RARCH_ERR("[GL]: Failed to create FBO targets. Will continue without FBO.\n");
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
   if (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, true);

   RARCH_LOG("[GL]: Initializing HW render (%ux%u).\n", width, height);
   glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_fbo_size);
   glGetIntegerv(RARCH_GL_MAX_RENDERBUFFER_SIZE, &max_renderbuffer_size);
   RARCH_LOG("[GL]: Max texture size: %d px, renderbuffer size: %d px.\n",
         max_fbo_size, max_renderbuffer_size);

   if (!(gl->flags & GL2_FLAG_HAVE_FBO))
      return false;

   RARCH_LOG("[GL]: Supports FBO (render-to-texture).\n");

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
#if defined(HAVE_OPENGLES2) || defined(HAVE_OPENGLES1) || (defined(__MACH__) && defined(MAC_OS_X_VERSION_MAX_ALLOWED) && (MAC_OS_X_VERSION_MAX_ALLOWED < 101200))
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
         RARCH_ERR("[GL]: Failed to create HW render FBO #%u, error: 0x%04x.\n",
               i, status);
         return false;
      }
   }

   gl2_renderchain_bind_backbuffer();
   gl->flags |= GL2_FLAG_HW_RENDER_FBO_INIT;

   if (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, false);
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

static bool gl2_renderchain_read_viewport(
      gl2_t *gl,
      uint8_t *buffer, bool is_idle)
{
   unsigned num_pixels    = 0;

   if (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, false);

   num_pixels             = gl->vp.width * gl->vp.height;

#ifdef HAVE_GL_ASYNC_READBACK
   if (gl->flags & GL2_FLAG_PBO_READBACK_ENABLE)
   {
      const uint8_t *ptr  = NULL;

      /* Don't readback if we're in menu mode.
       * We haven't buffered up enough frames yet, come back later. */
      if (!gl->pbo_readback_valid[gl->pbo_readback_index])
         goto error;

      gl->pbo_readback_valid[gl->pbo_readback_index] = false;
      glBindBuffer(GL_PIXEL_PACK_BUFFER,
            gl->pbo_readback[gl->pbo_readback_index]);

#ifdef HAVE_OPENGLES3
      /* Slower path, but should work on all implementations at least. */
      ptr        = (const uint8_t*)glMapBufferRange(GL_PIXEL_PACK_BUFFER,
            0, num_pixels * sizeof(uint32_t), GL_MAP_READ_BIT);

      if (ptr)
      {
         int y;
         for (y = 0; y < gl->vp.height; y++)
         {
            video_frame_convert_rgba_to_bgr(
                  (const void*)ptr,
                  buffer,
                  gl->vp.width);
         }
      }
#else
      ptr = (const uint8_t*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
      if (ptr)
      {
         struct scaler_ctx *ctx = &gl->pbo_readback_scaler;
         scaler_ctx_scale_direct(ctx, buffer, ptr);
      }
#endif

      if (!ptr)
      {
         RARCH_ERR("[GL]: Failed to map pixel unpack buffer.\n");
         goto error;
      }

      glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
      glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
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

      video_frame_convert_rgba_to_bgr(
            (const void*)gl->readback_buffer_screenshot,
            buffer,
            num_pixels);

      free(gl->readback_buffer_screenshot);
      gl->readback_buffer_screenshot = NULL;
   }

   if (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, true);
   return true;

error:
   if (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
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

static void gl2_renderchain_copy_frame(
      gl2_t *gl,
      gl2_renderchain_data_t *chain,
      bool use_rgba,
      const void *frame,
      unsigned width, unsigned height, unsigned pitch)
{
#if defined(HAVE_PSGL)
   {
      int h;
      size_t buffer_addr        = gl->tex_w * gl->tex_h *
         gl->tex_index * gl->base_size;
      size_t buffer_stride      = gl->tex_w * gl->base_size;
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
         RARCH_ERR("[GL]: Failed to create EGL image.\n");
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
         glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / gl->base_size);
         glTexSubImage2D(GL_TEXTURE_2D,
               0, 0, 0, width, height, gl->texture_type,
               gl->texture_fmt, frame);

         glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
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
            int h;
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
         glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / gl->base_size);

      glTexSubImage2D(GL_TEXTURE_2D,
            0, 0, 0, width, height, gl->texture_type,
            gl->texture_fmt, data_buf);

      glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
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
   glReadBuffer(GL_BACK);
#endif

   glReadPixels(gl->vp.x, gl->vp.y,
         gl->vp.width, gl->vp.height,
         (GLenum)fmt, (GLenum)type, (GLvoid*)src);
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
         gl->tex_w, gl->tex_h, 0,
         (GLenum)internal_fmt,
         gl->tex_w * gl->base_size,
         gl->tex_w * gl->tex_h * i * gl->base_size);
#else
   if (chain->flags & GL2_CHAIN_FLAG_EGL_IMAGES)
      return;

   gl2_load_texture_image(GL_TEXTURE_2D,
      0,
      (GLenum)internal_fmt,
      gl->tex_w, gl->tex_h, 0,
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
   /* GLES3 has unpack_subimage and sRGB in core. */
   if (gl_check_capability(GL_CAPS_SRGB_FBO_ES3))
      chain->flags |=  GL2_CHAIN_FLAG_HAS_SRGB_FBO_GLES3;
   else
      chain->flags &= ~GL2_CHAIN_FLAG_HAS_SRGB_FBO_GLES3;

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
      const void *frame, unsigned base_size)
{
   GLint mag_filter, min_filter;
   bool want_mipmap = false;
   bool use_rgba    = video_driver_supports_rgba();
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

   img.width         = 0;
   img.height        = 0;
   img.pixels        = NULL;
   img.supports_rgba = video_driver_supports_rgba();

   if (!image_texture_load(&img, lut_path))
   {
      RARCH_ERR("[GL]: Failed to load texture image from: \"%s\".\n",
            lut_path);
      return false;
   }

   RARCH_LOG("[GL]: Loaded texture image from: \"%s\" ...\n",
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
         img.pixels, sizeof(uint32_t));
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
   glDeleteTextures(gl->overlays, gl->overlay_tex);

   free(gl->overlay_tex);
   free(gl->overlay_vertex_coord);
   free(gl->overlay_tex_coord);
   free(gl->overlay_color_coord);
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

   if (!gl)
      return;

   if (image > gl->overlays)
   {
      RARCH_ERR("[GL]: Invalid overlay id: %u\n", image);
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

   if (!gl)
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
   unsigned width                      = gl->video_width;
   unsigned height                     = gl->video_height;

   glEnable(GL_BLEND);

   if (gl->flags & GL2_FLAG_OVERLAY_FULLSCREEN)
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
   if (gl->flags & GL2_FLAG_OVERLAY_FULLSCREEN)
      glViewport(gl->vp.x, gl->vp.y, gl->vp.width, gl->vp.height);
}
#endif

static void gl2_set_viewport_wrapper(void *data, unsigned viewport_width,
      unsigned viewport_height, bool force_full, bool allow_rotate)
{
   gl2_t              *gl = (gl2_t*)data;
   gl2_set_viewport(gl,
         viewport_width, viewport_height, force_full, allow_rotate);
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

   if (type != RARCH_SHADER_CG && type != RARCH_SHADER_GLSL)
   {
      type = DEFAULT_SHADER_TYPE;

      if (type != RARCH_SHADER_CG && type != RARCH_SHADER_GLSL)
         type = RARCH_SHADER_GLSL;
   }

   for (i = 0; i < 2; i++)
   {
      switch (type)
      {
         case RARCH_SHADER_CG:
#ifdef HAVE_CG
            if (video_shader_is_supported(type))
               return type;
#endif
            type = RARCH_SHADER_GLSL;
            break;

         case RARCH_SHADER_GLSL:
#ifdef HAVE_GLSL
            if (video_shader_is_supported(type))
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
      RARCH_ERR("[Shader driver]: Shader backend %d not supported, falling back to %d.\n", type, fallback);

   switch (fallback)
   {
#ifdef HAVE_CG
      case RARCH_SHADER_CG:
         RARCH_LOG("[Shader driver]: Using Cg shader backend.\n");
         return &gl_cg_backend;
#endif
#ifdef HAVE_GLSL
      case RARCH_SHADER_GLSL:
         RARCH_LOG("[Shader driver]: Using GLSL shader backend.\n");
         return &gl_glsl_backend;
#endif
      default:
         RARCH_LOG("[Shader driver]: No supported shader backend.\n");
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
      RARCH_LOG("Setting up menu pipeline shaders for XMB ...\n");
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
      RARCH_ERR("[GL]: Couldn't find any supported shader backend! Continuing without shaders.\n");
      return true;
   }

   if (type != parse_type)
   {
      if (!string_is_empty(shader_path))
         RARCH_WARN("[GL]: Shader preset %s is using unsupported shader type %s, falling back to stock %s.\n",
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
      return true;
   }

   RARCH_ERR("[GL]: Failed to initialize shader, falling back to stock.\n");

   init_data.shader                  = NULL;
   init_data.shader_data             = NULL;
   init_data.path                    = NULL;

   ret                               = gl_shader_driver_init(&init_data);

   gl->shader                        = init_data.shader;
   gl->shader_data                   = init_data.shader_data;

   return ret;
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

static void gl2_set_video_mode(void *data, unsigned width, unsigned height,
      bool fullscreen)
{
   gl2_t               *gl = (gl2_t*)data;
   if (gl->ctx_driver->set_video_mode)
      gl->ctx_driver->set_video_mode(gl->ctx_data,
            width, height, fullscreen);
}

static void gl2_update_input_size(gl2_t *gl, unsigned width,
      unsigned height, unsigned pitch, bool clear)
{
   float xamt, yamt;

   if ((width != gl->last_width[gl->tex_index] ||
            height != gl->last_height[gl->tex_index]) && gl->empty_buf)
   {
      /* Resolution change. Need to clear out texture. */

      gl->last_width[gl->tex_index]  = width;
      gl->last_height[gl->tex_index] = height;

      if (clear)
      {
         glPixelStorei(GL_UNPACK_ALIGNMENT,
               gl2_get_alignment(width * sizeof(uint32_t)));
#if defined(HAVE_PSGL)
         glBufferSubData(GL_TEXTURE_REFERENCE_BUFFER_SCE,
               gl->tex_w * gl->tex_h * gl->tex_index * gl->base_size,
               gl->tex_w * gl->tex_h * gl->base_size,
               gl->empty_buf);
#else
         glTexSubImage2D(GL_TEXTURE_2D,
               0, 0, 0, gl->tex_w, gl->tex_h, gl->texture_type,
               gl->texture_fmt, gl->empty_buf);
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

   xamt = (float)width  / gl->tex_w;
   yamt = (float)height / gl->tex_h;
   SET_TEXTURE_COORDS(gl->tex_info.coord, xamt, yamt);
}

static void gl2_init_textures_data(gl2_t *gl)
{
   size_t i;
   for (i = 0; i < gl->textures; i++)
   {
      gl->last_width[i]  = gl->tex_w;
      gl->last_height[i] = gl->tex_h;
   }

   for (i = 0; i < gl->textures; i++)
   {
      gl->prev_info[i].tex           = gl->texture[0];
      gl->prev_info[i].input_size[0] = gl->tex_w;
      gl->prev_info[i].tex_size[0]   = gl->tex_w;
      gl->prev_info[i].input_size[1] = gl->tex_h;
      gl->prev_info[i].tex_size[1]   = gl->tex_h;
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
         gl->tex_w * gl->tex_h * gl->base_size * gl->textures,
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
         RARCH_WARN("[GL]: 32-bit FBO not supported. Falling back to 16-bit.\n");
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

static INLINE void gl2_set_shader_viewports(gl2_t *gl)
{
   int i;
   unsigned width                = gl->video_width;
   unsigned height               = gl->video_height;

   for (i = 0; i < 2; i++)
   {
      gl->shader->use(gl, gl->shader_data, i, true);
      gl2_set_viewport(gl, width, height, false, true);
   }
}

static void gl2_set_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   settings_t *settings            = config_get_ptr();
   enum texture_filter_type
      menu_filter                  = settings->bools.menu_linear_filter
      ? TEXTURE_FILTER_LINEAR
      : TEXTURE_FILTER_NEAREST;
   unsigned base_size              = rgb32 ? sizeof(uint32_t) : sizeof(uint16_t);
   gl2_t *gl                       = (gl2_t*)data;
   if (!gl)
      return;

   if (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, false);

   if (!gl->menu_texture)
      glGenTextures(1, &gl->menu_texture);

   gl_load_texture_data(gl->menu_texture,
         RARCH_WRAP_EDGE, menu_filter,
         gl2_get_alignment(width * base_size),
         width, height, frame,
         base_size);

   gl->menu_texture_alpha = alpha;
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

   if (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
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

static void gl2_render_osd_background(gl2_t *gl, const char *msg)
{
   video_coords_t coords;
   struct uniform_info uniform_param;
   float colors[4];
   const unsigned
      vertices_total       = 6;
   float *dummy            = (float*)calloc(4 * vertices_total, sizeof(float));
   float *verts            = (float*)malloc(2 * vertices_total * sizeof(float));
   settings_t *settings    = config_get_ptr();
   float video_font_size   = settings->floats.video_font_size;
   int msg_width           =
      font_driver_get_message_width(NULL, msg, strlen(msg), 1.0f);

   /* shader driver expects vertex coords as 0..1 */
   float x                 = settings->floats.video_msg_pos_x;
   float y                 = settings->floats.video_msg_pos_y;
   float width             = msg_width / (float)gl->video_width;
   float height            = video_font_size / (float)gl->video_height;
   float x2                = 0.005f; /* extend background around text */
   float y2                = 0.005f;

   x                      -= x2;
   y                      -= y2;
   width                  += x2;
   height                 += y2;

   colors[0]               = settings->uints.video_msg_bgcolor_red / 255.0f;
   colors[1]               = settings->uints.video_msg_bgcolor_green / 255.0f;
   colors[2]               = settings->uints.video_msg_bgcolor_blue / 255.0f;
   colors[3]               = settings->floats.video_msg_bgcolor_opacity;

   /* triangle 1 */
   verts[0]                = x;
   verts[1]                = y; /* bottom-left */

   verts[2]                = x;
   verts[3]                = y + height; /* top-left */

   verts[4]                = x + width;
   verts[5]                = y + height; /* top-right */

   /* triangle 2 */
   verts[6]                = x;
   verts[7]                = y; /* bottom-left */

   verts[8]                = x + width;
   verts[9]                = y + height; /* top-right */

   verts[10]               = x + width;
   verts[11]               = y; /* bottom-right */

   coords.color            = dummy;
   coords.vertex           = verts;
   coords.tex_coord        = dummy;
   coords.lut_tex_coord    = dummy;
   coords.vertices         = vertices_total;

   gl2_set_viewport(gl,
         gl->video_width,
         gl->video_height, true, false);

   gl->shader->use(gl, gl->shader_data,
         VIDEO_SHADER_STOCK_BLEND, true);

   gl->shader->set_coords(gl->shader_data, &coords);

   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glBlendEquation(GL_FUNC_ADD);

   gl->shader->set_mvp(gl->shader_data, &gl->mvp_no_rot);

   uniform_param.type              = UNIFORM_4F;
   uniform_param.enabled           = true;
   uniform_param.location          = 0;
   uniform_param.count             = 0;

   uniform_param.lookup.type       = SHADER_PROGRAM_FRAGMENT;
   uniform_param.lookup.ident      = "bgcolor";
   uniform_param.lookup.idx        = VIDEO_SHADER_STOCK_BLEND;
   uniform_param.lookup.add_prefix = true;
   uniform_param.lookup.enable     = true;

   uniform_param.result.f.v0       = colors[0];
   uniform_param.result.f.v1       = colors[1];
   uniform_param.result.f.v2       = colors[2];
   uniform_param.result.f.v3       = colors[3];

   gl->shader->set_uniform_parameter(gl->shader_data,
         &uniform_param, NULL);

   glDrawArrays(GL_TRIANGLES, 0, coords.vertices);

   /* reset uniform back to zero so it is not used for anything else */
   uniform_param.result.f.v0       = 0.0f;
   uniform_param.result.f.v1       = 0.0f;
   uniform_param.result.f.v2       = 0.0f;
   uniform_param.result.f.v3       = 0.0f;

   gl->shader->set_uniform_parameter(gl->shader_data,
         &uniform_param, NULL);

   free(dummy);
   free(verts);

   gl2_set_viewport(gl,
         gl->video_width,
         gl->video_height, false, true);
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
   unsigned width         = gl->video_width;
   unsigned height        = gl->video_height;

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

   if (gl->flags & GL2_FLAG_MENU_TEXTURE_FULLSCREEN)
   {
      glViewport(0, 0, width, height);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      glViewport(gl->vp.x, gl->vp.y, gl->vp.width, gl->vp.height);
   }
   else
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   glDisable(GL_BLEND);

   gl->coords.vertex      = gl->vertex_ptr;
   gl->coords.tex_coord   = gl->tex_info.coord;
   gl->coords.color       = gl->white_color_ptr;
}
#endif

static void gl2_pbo_async_readback(gl2_t *gl)
{
#ifdef HAVE_OPENGLES
   GLenum fmt  = GL_RGBA;
   GLenum type = GL_UNSIGNED_BYTE;
#else
   GLenum fmt  = GL_BGRA;
   GLenum type = GL_UNSIGNED_INT_8_8_8_8_REV;
#endif

   gl2_renderchain_bind_pbo(
         gl->pbo_readback[gl->pbo_readback_index++]);
   gl->pbo_readback_index &= 3;

   /* 4 frames back, we can readback. */
   gl->pbo_readback_valid[gl->pbo_readback_index] = true;

   gl2_renderchain_readback(gl, gl->renderchain_data,
         gl2_get_alignment(gl->vp.width * sizeof(uint32_t)),
         fmt, type, NULL);
   gl2_renderchain_unbind_pbo();
}

static bool gl2_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height,
      uint64_t frame_count,
      unsigned pitch, const char *msg,
      video_frame_info_t *video_info)
{
   video_shader_ctx_params_t params;
   struct video_tex_info feedback_info;
   gl2_t                            *gl = (gl2_t*)data;
   gl2_renderchain_data_t       *chain = (gl2_renderchain_data_t*)gl->renderchain_data;
   unsigned width                      = gl->video_width;
   unsigned height                     = gl->video_height;
   bool use_rgba                       = (video_info->video_st_flags & VIDEO_FLAG_USE_RGBA) ? true : false;
   bool statistics_show                = video_info->statistics_show;
   bool msg_bgcolor_enable             = video_info->msg_bgcolor_enable;
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

   if (!gl)
      return false;

   if (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, false);

#ifndef HAVE_OPENGLES
   if (gl->flags & GL2_FLAG_CORE_CONTEXT_IN_USE)
      glBindVertexArray(chain->vao);
#endif

   gl->shader->use(gl, gl->shader_data, 1, true);

#ifdef IOS
   /* Apparently the viewport is lost each frame, thanks Apple. */
   gl2_set_viewport(gl, width, height, false, true);
#endif

   /* Render to texture in first pass. */
   if (gl->flags & GL2_FLAG_FBO_INITED)
   {
      gl2_renderchain_recompute_pass_sizes(
            gl, chain,
            frame_width, frame_height,
            gl->vp_out_width, gl->vp_out_height);

      gl2_renderchain_start_render(gl, chain);
   }

   if (gl->flags & GL2_FLAG_SHOULD_RESIZE)
   {
      if (gl->ctx_driver->set_resize)
         gl->ctx_driver->set_resize(gl->ctx_data,
            width, height);
      gl->flags &= ~GL2_FLAG_SHOULD_RESIZE;

      if (gl->flags & GL2_FLAG_FBO_INITED)
      {
         /* On resize, we might have to recreate our FBOs
          * due to "Viewport" scale, and set a new viewport. */
         size_t i;
         /* Check if we have to recreate our FBO textures. */
         for (i = 0; i < (size_t)chain->fbo_pass; i++)
         {
            struct video_fbo_rect *fbo_rect = &gl->fbo_rect[i];
            if (fbo_rect)
            {
               unsigned img_width   = fbo_rect->max_img_width;
               unsigned img_height  = fbo_rect->max_img_height;

               if (     (img_width  > fbo_rect->width)
                     || (img_height > fbo_rect->height))
               {
                  /* Check proactively since we might suddenly
                   * get sizes of tex_w width or tex_h height. */
                  unsigned max                    = img_width > img_height ? img_width : img_height;
                  unsigned pow2_size              = next_pow2(max);
                  bool update_feedback            = (gl->flags & GL2_FLAG_FBO_FEEDBACK_ENABLE)
                     && (unsigned)i == gl->fbo_feedback_pass;

                  fbo_rect->width                 = pow2_size;
                  fbo_rect->height                = pow2_size;

                  gl2_recreate_fbo(fbo_rect, chain->fbo[i], &chain->fbo_texture[i]);

                  /* Update feedback texture in-place so we avoid having to
                   * juggle two different fbo_rect structs since they get updated here. */
                  if (update_feedback)
                  {
                     if (gl2_recreate_fbo(fbo_rect, gl->fbo_feedback,
                              &gl->fbo_feedback_texture))
                     {
                        /* Make sure the feedback textures are cleared
                         * so we don't feedback noise. */
                        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
                        glClear(GL_COLOR_BUFFER_BIT);
                     }
                  }

                  RARCH_LOG("[GL]: Recreating FBO texture #%d: %ux%u.\n",
                        i, fbo_rect->width, fbo_rect->height);
               }
            }
         }

         /* Go back to what we're supposed to do,
          * render to FBO #0. */
         gl2_renderchain_start_render(gl, chain);
      }
      else
         gl2_set_viewport(gl, width, height, false, true);
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

         gl2_renderchain_copy_frame(gl, chain, use_rgba,
               frame, frame_width, frame_height, pitch);
      }

      /* No point regenerating mipmaps
       * if there are no new frames. */
      if (     (gl->flags & GL2_FLAG_TEXTURE_MIPMAP)
            && (gl->flags & GL2_FLAG_HAVE_MIPMAP))
         glGenerateMipmap(GL_TEXTURE_2D);
   }

   /* Have to reset rendering state which libretro core
    * could easily have overridden. */
   if (gl->flags & GL2_FLAG_HW_RENDER_FBO_INIT)
   {
      gl2_update_input_size(gl, frame_width, frame_height, pitch, false);
      if (!(gl->flags & GL2_FLAG_FBO_INITED))
      {
         gl2_renderchain_bind_backbuffer();
         gl2_set_viewport(gl, width, height, false, true);
      }

      gl2_renderchain_restore_default_state(gl);

      glDisable(GL_STENCIL_TEST);
      glDisable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glBlendEquation(GL_FUNC_ADD);
      glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
   }

   gl->tex_info.tex           = gl->texture[gl->tex_index];
   gl->tex_info.input_size[0] = frame_width;
   gl->tex_info.input_size[1] = frame_height;
   gl->tex_info.tex_size[0]   = gl->tex_w;
   gl->tex_info.tex_size[1]   = gl->tex_h;

   feedback_info              = gl->tex_info;

   if (gl->flags & GL2_FLAG_FBO_FEEDBACK_ENABLE)
   {
      const struct video_fbo_rect
         *rect                        = &gl->fbo_rect[gl->fbo_feedback_pass];
      GLfloat xamt                    = (GLfloat)rect->img_width / rect->width;
      GLfloat yamt                    = (GLfloat)rect->img_height / rect->height;

      feedback_info.tex               = gl->fbo_feedback_texture;
      feedback_info.input_size[0]     = rect->img_width;
      feedback_info.input_size[1]     = rect->img_height;
      feedback_info.tex_size[0]       = rect->width;
      feedback_info.tex_size[1]       = rect->height;

      SET_TEXTURE_COORDS(feedback_info.coord, xamt, yamt);
   }

   glClear(GL_COLOR_BUFFER_BIT);

   params.data          = gl;
   params.width         = frame_width;
   params.height        = frame_height;
   params.tex_width     = gl->tex_w;
   params.tex_height    = gl->tex_h;
   params.out_width     = gl->vp.width;
   params.out_height    = gl->vp.height;
   params.frame_counter = (unsigned int)frame_count;
   params.info          = &gl->tex_info;
   params.prev_info     = gl->prev_info;
   params.feedback_info = &feedback_info;
   params.fbo_info      = NULL;
   params.fbo_info_cnt  = 0;

   gl->shader->set_params(&params, gl->shader_data);

   gl->coords.vertices  = 4;

   gl->shader->set_coords(gl->shader_data, &gl->coords);
   gl->shader->set_mvp(gl->shader_data, &gl->mvp);

   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   if (gl->flags & GL2_FLAG_FBO_INITED)
      gl2_renderchain_render(gl,
            chain,
            frame_count, &gl->tex_info, &feedback_info);

   /* Set prev textures. */
   gl2_renderchain_bind_prev_texture(gl,
         chain, &gl->tex_info);

#ifdef HAVE_OVERLAY
   if ((gl->flags & GL2_FLAG_OVERLAY_ENABLE) && overlay_behind_menu)
      gl2_render_overlay(gl);
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
         font_driver_render_msg(gl, stat_text,
               (const struct font_params*)osd_params, NULL);
   }
#endif

#ifdef HAVE_OVERLAY
   if ((gl->flags & GL2_FLAG_OVERLAY_ENABLE) && !overlay_behind_menu)
      gl2_render_overlay(gl);
#endif

#ifdef HAVE_GFX_WIDGETS
   if (widgets_active)
      gfx_widgets_frame(video_info);
#endif

   if (!string_is_empty(msg))
   {
      if (msg_bgcolor_enable)
         gl2_render_osd_background(gl, msg);
      font_driver_render_msg(gl, msg, NULL, NULL);
   }

   if (gl->ctx_driver->update_window_title)
      gl->ctx_driver->update_window_title(gl->ctx_data);

   /* Reset state which could easily mess up libretro core. */
   if (gl->flags & GL2_FLAG_HW_RENDER_FBO_INIT)
   {
      gl->shader->use(gl, gl->shader_data, 0, true);
      glBindTexture(GL_TEXTURE_2D, 0);
   }

   /* Screenshots. */
   if (gl->readback_buffer_screenshot)
      gl2_renderchain_readback(gl,
            chain,
            4, GL_RGBA, GL_UNSIGNED_BYTE,
            gl->readback_buffer_screenshot);

   /* Don't readback if we're in menu mode. */
   else if (gl->flags & GL2_FLAG_PBO_READBACK_ENABLE)
#ifdef HAVE_MENU
         /* Don't readback if we're in menu mode. */
         if (!(gl->flags & GL2_FLAG_MENU_TEXTURE_ENABLE))
#endif
            gl2_pbo_async_readback(gl);

    if (gl->ctx_driver->swap_buffers)
        gl->ctx_driver->swap_buffers(gl->ctx_data);

 /* Emscripten has to do black frame insertion in its main loop */
#ifndef EMSCRIPTEN
   /* Disable BFI during fast forward, slow-motion,
    * and pause to prevent flicker. */
   if (
         video_info->black_frame_insertion
         && !video_info->input_driver_nonblock_state
         && !video_info->runloop_is_slowmotion
         && !video_info->runloop_is_paused
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
            if (!(gl2_frame(gl, NULL, 0, 0, frame_count, 0, msg, video_info)))
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

#ifndef HAVE_OPENGLES
   if (gl->flags & GL2_FLAG_CORE_CONTEXT_IN_USE)
      glBindVertexArray(0);
#endif
   if (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
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

   if (gl->flags & GL2_FLAG_HAVE_SYNC)
      gl2_renderchain_fence_free(gl,
            (gl2_renderchain_data_t*)
            gl->renderchain_data);

   font_driver_free_osd();

   gl->shader->deinit(gl->shader_data);

   glDeleteTextures(gl->textures, gl->texture);

#if defined(HAVE_MENU)
   if (gl->menu_texture)
      glDeleteTextures(1, &gl->menu_texture);
#endif

#ifdef HAVE_OVERLAY
   gl2_free_overlay(gl);
#endif

#if defined(HAVE_PSGL)
   glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, 0);
   glDeleteBuffers(1, &gl->pbo);
#endif

   scaler_ctx_gen_reset(&gl->scaler);

   if (gl->flags & GL2_FLAG_PBO_READBACK_ENABLE)
   {
      glDeleteBuffers(4, gl->pbo_readback);
      scaler_ctx_gen_reset(&gl->pbo_readback_scaler);
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
      gl->ctx_driver->swap_interval(gl->ctx_data, interval);
   }

   if (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
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
      if (video_hard_sync)
         RARCH_LOG("[GL]: Using ARB_sync to reduce latency.\n");
   }
   else
      gl->flags                 &= ~GL2_FLAG_HAVE_SYNC;

   video_driver_unset_rgba();

   gl2_renderchain_resolve_extensions(gl,
         (gl2_renderchain_data_t*)gl->renderchain_data,
         context_ident, video);

#if defined(HAVE_OPENGLES) && !defined(HAVE_PSGL)
   if (!gl_check_capability(GL_CAPS_BGRA8888))
   {
      video_driver_set_rgba();
      RARCH_WARN("[GL]: GLES implementation does not have BGRA8888 extension.\n"
                 "[GL]: 32-bit path will require conversion.\n");
   }
   /* TODO/FIXME - No extensions for float FBO currently. */
#endif

#ifdef GL_DEBUG
   /* Useful for debugging, but kinda obnoxious otherwise. */
   RARCH_LOG("[GL]: Supported extensions:\n");

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
      bool use_rgba       = video_driver_supports_rgba();

      gl->internal_fmt    = RARCH_GL_INTERNAL_FORMAT32;
      gl->texture_type    = RARCH_GL_TEXTURE_TYPE32;
      gl->texture_fmt     = RARCH_GL_FORMAT32;
      gl->base_size       = sizeof(uint32_t);

      if (use_rgba)
      {
         gl->internal_fmt = GL_RGBA;
         gl->texture_type = GL_RGBA;
      }
   }
#ifndef HAVE_OPENGLES
   else if (gl->flags & GL2_FLAG_HAVE_ES2_COMPAT)
   {
      RARCH_LOG("[GL]: Using GL_RGB565 for texture uploads.\n");
      gl->internal_fmt    = RARCH_GL_INTERNAL_FORMAT16_565;
      gl->texture_type    = RARCH_GL_TEXTURE_TYPE16_565;
      gl->texture_fmt     = RARCH_GL_FORMAT16_565;
   }
#endif
}

static bool gl2_init_pbo_readback(gl2_t *gl)
{
#if !defined(HAVE_OPENGLES2) && !defined(HAVE_PSGL)
   int i;

   glGenBuffers(4, gl->pbo_readback);

   for (i = 0; i < 4; i++)
   {
      gl2_renderchain_bind_pbo(gl->pbo_readback[i]);
      gl2_renderchain_init_pbo(gl->vp.width *
            gl->vp.height * sizeof(uint32_t), NULL);
   }
   gl2_renderchain_unbind_pbo();

#ifndef HAVE_OPENGLES3
   {
      struct scaler_ctx *scaler = &gl->pbo_readback_scaler;
      scaler->in_width          = gl->vp.width;
      scaler->in_height         = gl->vp.height;
      scaler->out_width         = gl->vp.width;
      scaler->out_height        = gl->vp.height;
      scaler->in_stride         = gl->vp.width * sizeof(uint32_t);
      scaler->out_stride        = gl->vp.width * 3;
      scaler->in_fmt            = SCALER_FMT_ARGB8888;
      scaler->out_fmt           = SCALER_FMT_BGR24;
      scaler->scaler_type       = SCALER_TYPE_POINT;

      if (!scaler_ctx_gen_filter(scaler))
      {
         gl->flags             &= ~GL2_FLAG_PBO_READBACK_ENABLE;
         RARCH_ERR("[GL]: Failed to initialize pixel conversion for PBO.\n");
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
   if (hwr->context_type == RETRO_HW_CONTEXT_OPENGLES3)
   {
      major                             = 3;
      minor                             = 0;
   }
   else
   {
      major                             = 2;
      minor                             = 0;
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

   gfx_ctx = video_context_driver_init_first(gl,
         settings->arrays.video_context_driver,
         api, major, minor,
         (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE) ? true : false,
         &ctx_data);

   if (ctx_data)
      gl->ctx_data = ctx_data;

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
         RARCH_ERR("[GL debug (High, %s, %s)]: %s\n", src, typestr, message);
         break;
      case GL_DEBUG_SEVERITY_MEDIUM:
         RARCH_WARN("[GL debug (Medium, %s, %s)]: %s\n", src, typestr, message);
         break;
      case GL_DEBUG_SEVERITY_LOW:
         RARCH_LOG("[GL debug (Low, %s, %s)]: %s\n", src, typestr, message);
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
      RARCH_ERR("[GL]: Neither GL_KHR_debug nor GL_ARB_debug_output are implemented. Cannot start GL debugging.\n");
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

static void *gl2_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   enum gfx_wrap_type wrap_type;
   unsigned full_x, full_y;
   unsigned shader_info_num;
   settings_t *settings                 = config_get_ptr();
   bool video_gpu_record                = settings->bools.video_gpu_record;
   int interval                         = 0;
   unsigned mip_level                   = 0;
   unsigned mode_width                  = 0;
   unsigned mode_height                 = 0;
   unsigned win_width                   = 0;
   unsigned win_height                  = 0;
   unsigned temp_width                  = 0;
   unsigned temp_height                 = 0;
   bool force_smooth                    = false;
   bool force_fullscreen                = false;
   const char *vendor                   = NULL;
   const char *renderer                 = NULL;
   const char *version                  = NULL;
   struct retro_hw_render_callback *hwr = NULL;
   char *error_string                   = NULL;
   recording_state_t *recording_st      = recording_state_get_ptr();
   gl2_t *gl                            = (gl2_t*)calloc(1, sizeof(gl2_t));
   const gfx_ctx_driver_t *ctx_driver   = gl2_get_context(gl);

   if (!gl || !ctx_driver)
      goto error;

   video_context_driver_set((const gfx_ctx_driver_t*)ctx_driver);

   gl->ctx_driver                       = ctx_driver;
   gl->video_info                       = *video;

   RARCH_LOG("[GL]: Found GL context: \"%s\".\n", ctx_driver->ident);

   if (gl->ctx_driver->get_video_size)
      gl->ctx_driver->get_video_size(gl->ctx_data,
               &mode_width, &mode_height);

   if (!video->fullscreen && !gl->ctx_driver->has_windowed)
   {
      RARCH_DBG("[GL]: Config requires windowed mode, but context driver does not support it. "
                "Forcing fullscreen for this session.\n");
      force_fullscreen = true;
   }

#if defined(DINGUX)
   mode_width  = 320;
   mode_height = 240;
#endif
   full_x      = mode_width;
   full_y      = mode_height;
   interval    = 0;

   RARCH_LOG("[GL]: Detecting screen resolution: %ux%u.\n", full_x, full_y);

   if (video->vsync)
      interval = video->swap_interval;

   if (gl->ctx_driver->swap_interval)
   {
      bool adaptive_vsync_enabled            = video_driver_test_all_flags(
            GFX_CTX_FLAGS_ADAPTIVE_VSYNC) && video->adaptive_vsync;
      if (adaptive_vsync_enabled && interval == 1)
         interval = -1;
      gl->ctx_driver->swap_interval(gl->ctx_data, interval);
   }

   win_width   = video->width;
   win_height  = video->height;

   if (video->fullscreen && (win_width == 0) && (win_height == 0))
   {
      win_width  = full_x;
      win_height = full_y;
   }
   /* If fullscreen had to be forced, video->width/height is incorrect */
   else if (force_fullscreen)
   {
      win_width  = settings->uints.video_fullscreen_x;
      win_height = settings->uints.video_fullscreen_y;
   }

   if (     !gl->ctx_driver->set_video_mode
         || !gl->ctx_driver->set_video_mode(gl->ctx_data,
            win_width, win_height, (video->fullscreen || force_fullscreen)))
      goto error;
#if defined(__APPLE__) && !defined(IOS) && !defined(HAVE_COCOA_METAL)
   /* This is a hack for now to work around a very annoying
    * issue that currently eludes us. */
   if (     !gl->ctx_driver->set_video_mode
         || !gl->ctx_driver->set_video_mode(gl->ctx_data,
            win_width, win_height, (video->fullscreen || force_fullscreen)))
      goto error;
#endif

#if !defined(RARCH_CONSOLE) || defined(HAVE_LIBNX)
   rglgen_resolve_symbols(ctx_driver->get_proc_address);
#endif

   /* Clear out potential error flags in case we use cached context. */
   glGetError();

   vendor   = (const char*)glGetString(GL_VENDOR);
   renderer = (const char*)glGetString(GL_RENDERER);
   version  = (const char*)glGetString(GL_VERSION);

   RARCH_LOG("[GL]: Vendor: %s, Renderer: %s.\n", vendor, renderer);
   RARCH_LOG("[GL]: Version: %s.\n", version);

   if (string_is_equal(ctx_driver->ident, "null"))
      goto error;

   if (!string_is_empty(version))
   {
      if (string_starts_with(version, "OpenGL ES "))
         sscanf(version, "OpenGL ES %d.%d", &gl->version_major, &gl->version_minor);
      else if (string_starts_with(version, "OpenGL "))
         sscanf(version, "OpenGL %d.%d", &gl->version_major, &gl->version_minor);
      else
         sscanf(version, "%d.%d", &gl->version_major, &gl->version_minor);
   }

   {
      size_t len    = 0;

      if (!string_is_empty(vendor))
      {
        len                    = strlcpy(gl->device_str, vendor, sizeof(gl->device_str));
        gl->device_str[  len]  = ' ';
        gl->device_str[++len]  = '\0';
      }

      if (!string_is_empty(renderer))
        strlcpy(gl->device_str + len, renderer, sizeof(gl->device_str) - len);

      if (!string_is_empty(version))
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

      RARCH_LOG("[GL]: Using Core GL context, setting up VAO...\n");
      if (!gl_check_capability(GL_CAPS_VAO))
      {
         RARCH_ERR("[GL]: Failed to initialize VAOs.\n");
         goto error;
      }
   }

   if (!renderchain_gl2_init_first(&gl->renderchain_data))
   {
      RARCH_ERR("[GL]: Renderchain could not be initialized.\n");
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

#ifdef GL_DEBUG
   gl2_begin_debug(gl);
#endif

   if (video->fullscreen || force_fullscreen)
      gl->flags  |=  GL2_FLAG_FULLSCREEN;

   mode_width     = 0;
   mode_height    = 0;

   if (gl->ctx_driver->get_video_size)
      gl->ctx_driver->get_video_size(gl->ctx_data,
            &mode_width, &mode_height);

#if defined(DINGUX)
   mode_width     = 320;
   mode_height    = 240;
#endif
   temp_width     = mode_width;
   temp_height    = mode_height;

   /* Get real known video size, which might have been altered by context. */

   if (temp_width != 0 && temp_height != 0)
      video_driver_set_size(temp_width, temp_height);

   video_driver_get_size(&temp_width, &temp_height);
   gl->video_width       = temp_width;
   gl->video_height      = temp_height;

   RARCH_LOG("[GL]: Using resolution %ux%u.\n", temp_width, temp_height);

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
       * create textures. */
      gl->textures = 1;
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
      RARCH_ERR("[GL:]: Shader driver initialization failed.\n");
      goto error;
   }

   RARCH_LOG("[GL]: Default shader backend found: %s.\n", gl->shader->ident);

   if (!gl2_shader_init(gl, ctx_driver, hwr))
   {
      RARCH_ERR("[GL]: Shader initialization failed.\n");
      goto error;
   }

   {
      unsigned texture_info_id = gl->shader->get_prev_textures(gl->shader_data);
      unsigned minimum         = texture_info_id;
      gl->textures             = MAX(minimum + 1, gl->textures);
   }

   shader_info_num = gl->shader->num_shaders(gl->shader_data);

   RARCH_LOG("[GL]: Using %u textures.\n", gl->textures);
   RARCH_LOG("[GL]: Loaded %u program(s).\n",
         shader_info_num);

   gl->tex_w = gl->tex_h = (RARCH_SCALE_BASE * video->input_scale);
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
      gl2_set_shader_viewports(gl);

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
    * the texture with on res change. */
   gl->empty_buf             = calloc(sizeof(uint32_t), gl->tex_w * gl->tex_h);

   gl->conv_buffer           = calloc(sizeof(uint32_t), gl->tex_w * gl->tex_h);

   if (!gl->conv_buffer)
      goto error;

   gl2_init_textures(gl);
   gl2_init_textures_data(gl);

   gl2_renderchain_init(gl,
         (gl2_renderchain_data_t*)gl->renderchain_data,
         gl->tex_w, gl->tex_h);

   if (gl->flags & GL2_FLAG_HAVE_FBO)
   {
      if (     (gl->flags & GL2_FLAG_HW_RENDER_USE)
            && !gl2_renderchain_init_hw_render(gl, (gl2_renderchain_data_t*)gl->renderchain_data, gl->tex_w, gl->tex_h))
      {
         RARCH_ERR("[GL]: Hardware rendering context initialization failed.\n");
         goto error;
      }
   }

   if (gl->ctx_driver->input_driver)
   {
      const char *joypad_name = settings->arrays.input_joypad_driver;
      gl->ctx_driver->input_driver(
            gl->ctx_data, joypad_name,
            input, input_data);
   }

   if (video->font_enable)
      font_driver_init_osd(gl, video,
            false,
            video->is_threaded,
            FONT_DRIVER_RENDER_OPENGL_API);

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
         RARCH_LOG("[GL]: Async PBO readback enabled.\n");
      }
   }
   else
      gl->flags &= ~GL2_FLAG_PBO_READBACK_ENABLE;

   if (!gl_check_error(&error_string))
   {
      RARCH_ERR("[GL]: %s\n", error_string);
      free(error_string);
      goto error;
   }

   if (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
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
   unsigned temp_width  = gl->video_width;
   unsigned temp_height = gl->video_height;

   gl->ctx_driver->check_window(gl->ctx_data,
         &quit, &resize, &temp_width, &temp_height);

#ifdef __WINRT__
   if (is_running_on_xbox())
   {
      /* Match the output res to the display resolution */
      temp_width  = uwp_get_width();
      temp_height = uwp_get_height();
   }
#endif
   if (quit)
      gl->flags   |= GL2_FLAG_QUITTING;
   else if (resize)
      gl->flags   |= GL2_FLAG_SHOULD_RESIZE;

   ret             = !(gl->flags & GL2_FLAG_QUITTING);

   if (temp_width != 0 && temp_height != 0)
   {
      video_driver_set_size(temp_width, temp_height);
      gl->video_width  = temp_width;
      gl->video_height = temp_height;
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

static void gl2_update_tex_filter_frame(gl2_t *gl)
{
   unsigned i, mip_level;
   GLenum wrap_mode;
   GLuint new_filt;
   enum gfx_wrap_type wrap_type;
   bool smooth                       = false;
   settings_t *settings              = config_get_ptr();
   bool video_smooth                 = settings->bools.video_smooth;
#ifdef HAVE_ODROIDGO2
   bool video_ctx_scaling            = settings->bools.video_ctx_scaling;
   if (video_ctx_scaling)
       video_smooth = false;
#endif

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
   if (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, true);
}

static bool gl2_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
#if defined(HAVE_GLSL) || defined(HAVE_CG)
   unsigned textures;
   video_shader_ctx_init_t init_data;
   enum rarch_shader_type fallback;
   gl2_t *gl = (gl2_t*)data;

   if (!gl)
      return false;

   if (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, false);

   fallback = gl2_get_fallback_shader_type(type);

   if (fallback == RARCH_SHADER_NONE)
   {
      RARCH_ERR("[GL]: No supported shader backend found!\n");
      goto error;
   }

   gl->shader->deinit(gl->shader_data);
   gl->shader_data = NULL;

   if (type != fallback)
   {
      RARCH_ERR("[GL]: %s shader not supported, falling back to stock %s\n",
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

      RARCH_WARN("[GL]: Failed to set multipass shader. Falling back to stock.\n");

      goto error;
   }

   gl->shader       = init_data.shader;
   gl->shader_data  = init_data.shader_data;

   gl2_update_tex_filter_frame(gl);

   {
      unsigned texture_info_id = gl->shader->get_prev_textures(gl->shader_data);
      textures                 = texture_info_id + 1;
   }

   if (textures > gl->textures) /* Have to reinit a bit. */
   {
      if ((gl->flags & GL2_FLAG_HW_RENDER_USE) && (gl->flags & GL2_FLAG_FBO_INITED))
         gl2_renderchain_deinit_hw_render(gl, (gl2_renderchain_data_t*)
               gl->renderchain_data);

      glDeleteTextures(gl->textures, gl->texture);
#if defined(HAVE_PSGL)
      glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, 0);
      glDeleteBuffers(1, &gl->pbo);
#endif
      gl->textures  = textures;
      gl->tex_index = 0;
      RARCH_LOG("[GL]: Using %u textures.\n", gl->textures);
      gl2_init_textures(gl);
      gl2_init_textures_data(gl);

      if (gl->flags & GL2_FLAG_HW_RENDER_USE)
         gl2_renderchain_init_hw_render(gl,
               (gl2_renderchain_data_t*)gl->renderchain_data,
               gl->tex_w, gl->tex_h);
   }

   gl2_renderchain_init(gl,
         (gl2_renderchain_data_t*)gl->renderchain_data,
         gl->tex_w, gl->tex_h);

   /* Apparently need to set viewport for passes when we aren't using FBOs. */
   gl2_set_shader_viewports(gl);
   if (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, true);

   return true;

error:
   if (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, true);
#endif
   return false;
}

static void gl2_viewport_info(void *data, struct video_viewport *vp)
{
   unsigned top_y, top_dist;
   gl2_t *gl       = (gl2_t*)data;
   unsigned width  = gl->video_width;
   unsigned height = gl->video_height;

   *vp             = gl->vp;
   vp->full_width  = width;
   vp->full_height = height;

   /* Adjust as GL viewport is bottom-up. */
   top_y           = vp->y + vp->height;
   top_dist        = height - top_y;
   vp->y           = top_dist;
}

static bool gl2_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   gl2_t *gl             = (gl2_t*)data;

   if (!gl)
      return false;

   return gl2_renderchain_read_viewport(gl, buffer, is_idle);
}

#if 0
#define READ_RAW_GL_FRAME_TEST
#endif

#if defined(READ_RAW_GL_FRAME_TEST)
static void* gl2_read_frame_raw(void *data, unsigned *width_p,
unsigned *height_p, size_t *pitch_p)
{
   gl2_t *gl             = (gl2_t*)data;
   unsigned width       = gl->last_width[gl->tex_index];
   unsigned height      = gl->last_height[gl->tex_index];
   size_t pitch         = gl->tex_w * gl->base_size;
   void* buffer         = NULL;
   void* buffer_texture = NULL;

   if (gl->flags & GL2_FLAG_HW_RENDER_USE)
   {
      buffer = malloc(pitch * height);
      if (!buffer)
         return NULL;
   }

   buffer_texture = malloc(pitch * gl->tex_h);

   if (!buffer_texture)
   {
      if (buffer)
         free(buffer);
      return NULL;
   }

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
   glGetTexImage(GL_TEXTURE_2D, 0,
         gl->texture_type, gl->texture_fmt, buffer_texture);

   *width_p  = width;
   *height_p = height;
   *pitch_p  = pitch;

   if (gl->flags & GL2_FLAG_HW_RENDER_USE)
   {
      int i;
      for (i = 0; i < height ; i++)
         memcpy((uint8_t*)buffer + i * pitch,
            (uint8_t*)buffer_texture + (height - 1 - i) * pitch, pitch);

      free(buffer_texture);
      return buffer;
   }

   return buffer_texture;
}
#endif

#ifdef HAVE_OVERLAY
static bool gl2_overlay_load(void *data,
      const void *image_data, unsigned num_images)
{
   unsigned i, j;
   gl2_t *gl = (gl2_t*)data;
   const struct texture_image *images =
      (const struct texture_image*)image_data;

   if (!gl)
      return false;

   if (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, false);

   gl2_free_overlay(gl);
   gl->overlay_tex = (GLuint*)
      calloc(num_images, sizeof(*gl->overlay_tex));

   if (!gl->overlay_tex)
   {
      if (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
         gl->ctx_driver->bind_hw_render(gl->ctx_data, true);
      return false;
   }

   gl->overlay_vertex_coord = (GLfloat*)
      calloc(2 * 4 * num_images, sizeof(GLfloat));
   gl->overlay_tex_coord    = (GLfloat*)
      calloc(2 * 4 * num_images, sizeof(GLfloat));
   gl->overlay_color_coord  = (GLfloat*)
      calloc(4 * 4 * num_images, sizeof(GLfloat));

   if (     !gl->overlay_vertex_coord
         || !gl->overlay_tex_coord
         || !gl->overlay_color_coord)
      return false;

   gl->overlays = num_images;
   glGenTextures(num_images, gl->overlay_tex);

   for (i = 0; i < num_images; i++)
   {
      unsigned alignment = gl2_get_alignment(images[i].width
            * sizeof(uint32_t));

      gl_load_texture_data(gl->overlay_tex[i],
            RARCH_WRAP_EDGE, TEXTURE_FILTER_LINEAR,
            alignment,
            images[i].width, images[i].height, images[i].pixels,
            sizeof(uint32_t));

      /* Default. Stretch to whole screen. */
      gl2_overlay_tex_geom(gl, i, 0, 0, 1, 1);
      gl2_overlay_vertex_geom(gl, i, 0, 0, 1, 1);

      for (j = 0; j < 16; j++)
         gl->overlay_color_coord[16 * i + j] = 1.0f;
   }

   if (gl->flags & GL2_FLAG_SHARED_CONTEXT_USE)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, true);
   return true;
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

   if (!gl)
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
   if (config_get_ptr()->bools.video_ctx_scaling)
      gl->flags     &= ~GL2_FLAG_KEEP_ASPECT;
#endif
}

static void gl2_apply_state_changes(void *data)
{
   gl2_t *gl            = (gl2_t*)data;
   if (gl)
      gl->flags        |= GL2_FLAG_SHOULD_RESIZE;
}

static void gl2_get_video_output_size(void *data,
      unsigned *width, unsigned *height, char *desc, size_t desc_len)
{
   gl2_t *gl         = (gl2_t*)data;
   if (gl && gl->ctx_driver && gl->ctx_driver->get_video_output_size)
      gl->ctx_driver->get_video_output_size(
            gl->ctx_data,
            width, height, desc, desc_len);
}

static void gl2_get_video_output_prev(void *data)
{
   gl2_t *gl = (gl2_t*)data;
   if (gl && gl->ctx_driver && gl->ctx_driver->get_video_output_prev)
      gl->ctx_driver->get_video_output_prev(gl->ctx_data);
}

static void gl2_get_video_output_next(void *data)
{
   gl2_t *gl = (gl2_t*)data;
   if (gl && gl->ctx_driver && gl->ctx_driver->get_video_output_next)
      gl->ctx_driver->get_video_output_next(gl->ctx_data);
}

static void video_texture_load_gl2(
      struct texture_image *ti,
      enum texture_filter_type filter_type,
      uintptr_t *idptr)
{
   GLuint id;
   unsigned width     = 0;
   unsigned height    = 0;
   const void *pixels = NULL;
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
         sizeof(uint32_t) /* TODO/FIXME - dehardcode */
         );
}

#ifdef HAVE_THREADS
static int video_texture_load_wrap_gl2_mipmap(void *data)
{
   uintptr_t id = 0;
   gl2_t    *gl = (gl2_t*)video_driver_get_ptr();

   if (gl && gl->ctx_driver->make_current)
      gl->ctx_driver->make_current(false);

   if (data)
      video_texture_load_gl2((struct texture_image*)data,
            TEXTURE_FILTER_MIPMAP_LINEAR, &id);
   return (int)id;
}

static int video_texture_load_wrap_gl2(void *data)
{
   uintptr_t id = 0;
   gl2_t    *gl = (gl2_t*)video_driver_get_ptr();

   if (gl && gl->ctx_driver->make_current)
      gl->ctx_driver->make_current(false);

   if (data)
      video_texture_load_gl2((struct texture_image*)data,
            TEXTURE_FILTER_LINEAR, &id);
   return (int)id;
}

static int video_texture_unload_wrap_gl2(void *data)
{
   GLuint  glid;
   uintptr_t id = (uintptr_t)data;
#if 0
   /*FIXME: crash on reinit*/
   gl2_t    *gl = (gl2_t*)video_driver_get_ptr();

   if (gl && gl->ctx_driver->make_current)
      gl->ctx_driver->make_current(false);
#endif
   glid = (GLuint)id;
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
      custom_command_method_t func = video_texture_load_wrap_gl2;
      switch (filter_type)
      {
         case TEXTURE_FILTER_MIPMAP_LINEAR:
         case TEXTURE_FILTER_MIPMAP_NEAREST:
            func = video_texture_load_wrap_gl2_mipmap;
            break;
         default:
            break;
      }
      return video_thread_texture_handle(data, func);
   }
#endif

   video_texture_load_gl2((struct texture_image*)data, filter_type, &id);
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
      custom_command_method_t func = video_texture_unload_wrap_gl2;
      video_thread_texture_handle((void *)id, func);
      return;
   }
#endif

   glid = (GLuint)id;
   glDeleteTextures(1, &glid);
}

static float gl2_get_refresh_rate(void *data)
{
   float refresh_rate = 0.0f;
   if (video_context_driver_get_refresh_rate(&refresh_rate))
      return refresh_rate;
   return 0.0f;
}

static uint32_t gl2_get_flags(void *data)
{
   uint32_t flags = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_HARD_SYNC);
   BIT32_SET(flags, GFX_CTX_FLAGS_BLACK_FRAME_INSERTION);
   BIT32_SET(flags, GFX_CTX_FLAGS_MENU_FRAME_FILTERING);
   BIT32_SET(flags, GFX_CTX_FLAGS_SCREENSHOTS_SUPPORTED);
   BIT32_SET(flags, GFX_CTX_FLAGS_OVERLAY_BEHIND_MENU_SUPPORTED);

   return flags;
}

static const video_poke_interface_t gl2_poke_interface = {
   gl2_get_flags,
   gl2_load_texture,
   gl2_unload_texture,
   gl2_set_video_mode,
   gl2_get_refresh_rate,
   NULL, /* set_filtering */
   gl2_get_video_output_size,
   gl2_get_video_output_prev,
   gl2_get_video_output_next,
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
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_max_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_contrast */
   NULL  /* set_hdr_expand_gamut */
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
#if defined(READ_RAW_GL_FRAME_TEST)
   gl2_read_frame_raw,
#else
   NULL,
#endif
#ifdef HAVE_OVERLAY
   gl2_get_overlay_interface,
#endif
   gl2_get_poke_interface,
   gl2_wrap_type_to_enum,
#ifdef HAVE_GFX_WIDGETS
   gl2_gfx_widgets_enabled
#endif
};
