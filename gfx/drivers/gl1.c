/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

/* OpenGL driver.
 *
 * We are targeting a minimum of OpenGL 1.1 and the Microsoft "GDI Generic" software GL implementation.
 * Any additional features added for later 1.x versions should only be enabled if they are detected at runtime. */

#include <stddef.h>
#include <retro_miscellaneous.h>
#include <formats/image.h>
#include <string/stdstring.h>
#include <retro_math.h>
#include <gfx/video_frame.h>
#include <gfx/scaler/pixconv.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#ifdef HAVE_MENU_WIDGETS
#include "../../menu/widgets/menu_widgets.h"
#endif
#endif

#include "../font_driver.h"

#include "../../driver.h"
#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../frontend/frontend_driver.h"
#include "../common/gl1_common.h"

#if defined(_WIN32) && !defined(_XBOX)
#include "../common/win32_common.h"
#endif

#ifdef HAVE_THREADS
#include "../video_thread_wrapper.h"
#endif

#ifdef VITA
static bool vgl_inited = false;
#endif

static unsigned char *gl1_menu_frame = NULL;
static unsigned gl1_menu_width       = 0;
static unsigned gl1_menu_height      = 0;
static unsigned gl1_menu_pitch       = 0;
static unsigned gl1_video_width      = 0;
static unsigned gl1_video_height     = 0;
static unsigned gl1_video_pitch      = 0;
static unsigned gl1_video_bits       = 0;
static unsigned gl1_menu_bits        = 0;
static bool gl1_rgb32                = false;
static bool gl1_menu_rgb32           = false;
static bool gl1_menu_size_changed    = false;
static unsigned char *gl1_video_buf  = NULL;
static unsigned char *gl1_menu_video_buf = NULL;

static bool gl1_shared_context_use = false;

static struct video_ortho gl1_default_ortho = {0, 1, 0, 1, -1, 1};

/* Used for the last pass when rendering to the back buffer. */
static const GLfloat gl1_vertexes_flipped[] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

static const GLfloat gl1_vertexes[] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const GLfloat gl1_tex_coords[] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const GLfloat gl1_white_color[] = {
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1,
};

#define gl1_context_bind_hw_render(gl1, enable) \
   if (gl1_shared_context_use) \
      gl1->ctx_driver->bind_hw_render(gl1->ctx_data, enable)

#ifdef HAVE_OVERLAY
static void gl1_render_overlay(gl1_t *gl, video_frame_info_t *video_info)
{
   unsigned i;
   unsigned width                      = video_info->width;
   unsigned height                     = video_info->height;

   glEnable(GL_BLEND);

   if (gl->overlay_full_screen)
      glViewport(0, 0, width, height);

   gl->coords.vertex    = gl->overlay_vertex_coord;
   gl->coords.tex_coord = gl->overlay_tex_coord;
   gl->coords.color     = gl->overlay_color_coord;
   gl->coords.vertices  = 4 * gl->overlays;

   glMatrixMode(GL_PROJECTION);
   glPushMatrix();
   glLoadIdentity();

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
   if (gl->overlay_full_screen)
      glViewport(gl->vp.x, gl->vp.y, gl->vp.width, gl->vp.height);
}

static void gl1_free_overlay(gl1_t *gl)
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

static void gl1_overlay_vertex_geom(void *data,
      unsigned image,
      float x, float y,
      float w, float h)
{
   GLfloat *vertex = NULL;
   gl1_t *gl        = (gl1_t*)data;

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

static void gl1_overlay_tex_geom(void *data,
      unsigned image,
      GLfloat x, GLfloat y,
      GLfloat w, GLfloat h)
{
   GLfloat *tex = NULL;
   gl1_t *gl     = (gl1_t*)data;

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

#endif

static bool is_pot(unsigned x)
{
   return (x & (x - 1)) == 0;
}

static unsigned get_pot(unsigned x)
{
   return (is_pot(x) ? x : next_pow2(x));
}

static void *gl1_gfx_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   unsigned full_x, full_y;
   gfx_ctx_input_t inp;
   gfx_ctx_mode_t mode;
   void *ctx_data                       = NULL;
   const gfx_ctx_driver_t *ctx_driver   = NULL;
   unsigned win_width = 0, win_height   = 0;
   unsigned temp_width = 0, temp_height = 0;
   settings_t *settings                 = config_get_ptr();
   gl1_t *gl1                           = (gl1_t*)calloc(1, sizeof(*gl1));
   const char *vendor                   = NULL;
   const char *renderer                 = NULL;
   const char *version                  = NULL;
   const char *extensions               = NULL;
   int interval                         = 0;
   struct retro_hw_render_callback *hwr = NULL;

   if (!gl1)
      return NULL;

   *input                               = NULL;
   *input_data                          = NULL;

   gl1_video_width                      = video->width;
   gl1_video_height                     = video->height;
   gl1_rgb32                            = video->rgb32;

   gl1_video_bits                       = video->rgb32 ? 32 : 16;

   if (video->rgb32)
      gl1_video_pitch = video->width * 4;
   else
      gl1_video_pitch = video->width * 2;

   ctx_driver = video_context_driver_init_first(gl1,
         settings->arrays.video_context_driver,
         GFX_CTX_OPENGL_API, 1, 1, false, &ctx_data);

   if (!ctx_driver)
      goto error;

   if (ctx_data)
      gl1->ctx_data = ctx_data;

   gl1->ctx_driver  = ctx_driver;

   video_context_driver_set((const gfx_ctx_driver_t*)ctx_driver);

   RARCH_LOG("[GL1]: Found GL1 context: %s\n", ctx_driver->ident);

   video_context_driver_get_video_size(&mode);

   full_x      = mode.width;
   full_y      = mode.height;
   mode.width  = 0;
   mode.height = 0;
#ifdef VITA
   if (!vgl_inited) {
      vglInitExtended(0x1400000, full_x, full_y, 0x100000, SCE_GXM_MULTISAMPLE_4X);
      vglUseVram(GL_TRUE);
      vglStartRendering();
      vgl_inited = true;
   }
#endif
   /* Clear out potential error flags in case we use cached context. */
   glGetError();

   if (string_is_equal(ctx_driver->ident, "null"))
      goto error;

   RARCH_LOG("[GL1]: Detecting screen resolution %ux%u.\n", full_x, full_y);

   win_width   = video->width;
   win_height  = video->height;

   if (video->fullscreen && (win_width == 0) && (win_height == 0))
   {
      win_width  = full_x;
      win_height = full_y;
   }

   mode.width      = win_width;
   mode.height     = win_height;
   mode.fullscreen = video->fullscreen;

   interval = video->swap_interval;

   if (ctx_driver->swap_interval)
   {
      bool adaptive_vsync_enabled            = video_driver_test_all_flags(
            GFX_CTX_FLAGS_ADAPTIVE_VSYNC) && video->adaptive_vsync;
      if (adaptive_vsync_enabled && interval == 1)
         interval = -1;
      ctx_driver->swap_interval(gl1->ctx_data, interval);
   }

   if (!video_context_driver_set_video_mode(&mode))
      goto error;

   gl1->fullscreen = video->fullscreen;

   mode.width     = 0;
   mode.height    = 0;

   video_context_driver_get_video_size(&mode);

   temp_width     = mode.width;
   temp_height    = mode.height;
   mode.width     = 0;
   mode.height    = 0;

   /* Get real known video size, which might have been altered by context. */

   if (temp_width != 0 && temp_height != 0)
      video_driver_set_size(&temp_width, &temp_height);

   video_driver_get_size(&temp_width, &temp_height);

   RARCH_LOG("[GL1]: Using resolution %ux%u\n", temp_width, temp_height);

   vendor   = (const char*)glGetString(GL_VENDOR);
   renderer = (const char*)glGetString(GL_RENDERER);
   version  = (const char*)glGetString(GL_VERSION);
   extensions = (const char*)glGetString(GL_EXTENSIONS);

   if (!string_is_empty(version))
      sscanf(version, "%d.%d", &gl1->version_major, &gl1->version_minor);

   if (!string_is_empty(extensions))
      gl1->extensions = string_split(extensions, " ");

   RARCH_LOG("[GL1]: Vendor: %s, Renderer: %s.\n", vendor, renderer);
   RARCH_LOG("[GL1]: Version: %s.\n", version);
   RARCH_LOG("[GL1]: Extensions: %s\n", extensions);

   {
      char device_str[128];

      device_str[0] = '\0';

      if (!string_is_empty(vendor))
      {
         strlcpy(device_str, vendor, sizeof(device_str));
         strlcat(device_str, " ", sizeof(device_str));
      }

      if (!string_is_empty(renderer))
         strlcat(device_str, renderer, sizeof(device_str));

      video_driver_set_gpu_device_string(device_str);

      if (!string_is_empty(version))
         video_driver_set_gpu_api_version_string(version);
   }

   inp.input      = input;
   inp.input_data = input_data;

   video_context_driver_input_driver(&inp);

   if (settings->bools.video_font_enable)
      font_driver_init_osd(gl1, false,
            video->is_threaded,
            FONT_DRIVER_RENDER_OPENGL1_API);

   gl1->smooth        = settings->bools.video_smooth;
   gl1->supports_bgra = string_list_find_elem(gl1->extensions, "GL_EXT_bgra");

   glDisable(GL_BLEND);
   glDisable(GL_DEPTH_TEST);
   glDisable(GL_CULL_FACE);
   glDisable(GL_STENCIL_TEST);
   glDisable(GL_SCISSOR_TEST);
#ifndef VITA
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
#endif
   glGenTextures(1, &gl1->tex);
   glGenTextures(1, &gl1->menu_tex);

   hwr = video_driver_get_hw_context();

   memcpy(gl1->tex_info.coord, gl1_tex_coords, sizeof(gl1->tex_info.coord));
   gl1->vertex_ptr        = hwr->bottom_left_origin
      ? gl1_vertexes : gl1_vertexes_flipped;
   gl1->textures              = 4;
   gl1->white_color_ptr       = gl1_white_color;
   gl1->coords.vertex         = gl1->vertex_ptr;
   gl1->coords.tex_coord      = gl1->tex_info.coord;
   gl1->coords.color          = gl1->white_color_ptr;
   gl1->coords.lut_tex_coord  = gl1_tex_coords;
   gl1->coords.vertices       = 4;

   RARCH_LOG("[GL1]: Init complete.\n");

   return gl1;

error:
   video_context_driver_destroy();
   if (gl1)
   {
      if (gl1->extensions)
         string_list_free(gl1->extensions);
      free(gl1);
   }
   return NULL;
}

static void gl1_set_projection(gl1_t *gl1,
      struct video_ortho *ortho, bool allow_rotate)
{
   math_matrix_4x4 rot;

   /* Calculate projection. */
   matrix_4x4_ortho(gl1->mvp_no_rot, ortho->left, ortho->right,
         ortho->bottom, ortho->top, ortho->znear, ortho->zfar);

   if (!allow_rotate)
   {
      gl1->mvp = gl1->mvp_no_rot;
      return;
   }

   matrix_4x4_rotate_z(rot, M_PI * gl1->rotation / 180.0f);
   matrix_4x4_multiply(gl1->mvp, rot, gl1->mvp_no_rot);
}

void gl1_gfx_set_viewport(gl1_t *gl1,
      video_frame_info_t *video_info,
      unsigned viewport_width,
      unsigned viewport_height,
      bool force_full, bool allow_rotate)
{
   gfx_ctx_aspect_t aspect_data;
   int x                    = 0;
   int y                    = 0;
   float device_aspect      = (float)viewport_width / viewport_height;
   unsigned height          = video_info->height;

   aspect_data.aspect       = &device_aspect;
   aspect_data.width        = viewport_width;
   aspect_data.height       = viewport_height;

   video_context_driver_translate_aspect(&aspect_data);

   if (video_info->scale_integer && !force_full)
   {
      video_viewport_get_scaled_integer(&gl1->vp,
            viewport_width, viewport_height,
            video_driver_get_aspect_ratio(), gl1->keep_aspect);
      viewport_width  = gl1->vp.width;
      viewport_height = gl1->vp.height;
   }
   else if (gl1->keep_aspect && !force_full)
   {
      float desired_aspect = video_driver_get_aspect_ratio();

#if defined(HAVE_MENU)
      if (video_info->aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         /* GL has bottom-left origin viewport. */
         x      = video_info->custom_vp_x;
         y      = height - video_info->custom_vp_y - video_info->custom_vp_height;
         viewport_width  = video_info->custom_vp_width;
         viewport_height = video_info->custom_vp_height;
      }
      else
#endif
      {
         float delta;

         if (fabsf(device_aspect - desired_aspect) < 0.0001f)
         {
            /* If the aspect ratios of screen and desired aspect
             * ratio are sufficiently equal (floating point stuff),
             * assume they are actually equal.
             */
         }
         else if (device_aspect > desired_aspect)
         {
            delta = (desired_aspect / device_aspect - 1.0f) / 2.0f + 0.5f;
            x     = (int)roundf(viewport_width * (0.5f - delta));
            viewport_width = (unsigned)roundf(2.0f * viewport_width * delta);
         }
         else
         {
            delta  = (device_aspect / desired_aspect - 1.0f) / 2.0f + 0.5f;
            y      = (int)roundf(viewport_height * (0.5f - delta));
            viewport_height = (unsigned)roundf(2.0f * viewport_height * delta);
         }
      }

      gl1->vp.x      = x;
      gl1->vp.y      = y;
      gl1->vp.width  = viewport_width;
      gl1->vp.height = viewport_height;
   }
   else
   {
      gl1->vp.x      = gl1->vp.y = 0;
      gl1->vp.width  = viewport_width;
      gl1->vp.height = viewport_height;
   }

#if defined(RARCH_MOBILE)
   /* In portrait mode, we want viewport to gravitate to top of screen. */
   if (device_aspect < 1.0f)
      gl1->vp.y *= 2;
#endif

   glViewport(gl1->vp.x, gl1->vp.y, gl1->vp.width, gl1->vp.height);
   gl1_set_projection(gl1, &gl1_default_ortho, allow_rotate);

   /* Set last backbuffer viewport. */
   if (!force_full)
   {
      gl1->vp_out_width  = viewport_width;
      gl1->vp_out_height = viewport_height;
   }

#if 0
   RARCH_LOG("Setting viewport @ %ux%u\n", viewport_width, viewport_height);
#endif
}

static void draw_tex(gl1_t *gl1, int pot_width, int pot_height, int width, int height, GLuint tex, const void *frame_to_copy)
{
   uint8_t *frame       = NULL;
   uint8_t *frame_rgba  = NULL;
   /* FIXME: For now, everything is uploaded as BGRA8888, I could not get 444 or 555 to work, and there is no 565 support in GL 1.1 either. */
   GLint internalFormat = GL_RGB8;
   GLenum format        = gl1->supports_bgra ? GL_BGRA_EXT : GL_RGBA;
   GLenum type          = GL_UNSIGNED_BYTE;

   float vertices[] = {
	   -1.0f, -1.0f, 0.0f,
	   -1.0f, 1.0f, 0.0f,
	   1.0f, -1.0f, 0.0f,
	   1.0f, 1.0f, 0.0f,
   };

   float colors[] = {
      1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f
   };

   float norm_width     = (1.0f / (float)pot_width) * (float)width;
   float norm_height    = (1.0f / (float)pot_height) * (float)height;
   
   float texcoords[] = {
      0.0f, 0.0f,
      0.0f, 0.0f,
      0.0f, 0.0f,
      0.0f, 0.0f
   };
   
   texcoords[1] = texcoords[5] = norm_height;
   texcoords[4] = texcoords[6] = norm_width;

   glDisable(GL_DEPTH_TEST);
   glDisable(GL_CULL_FACE);
   glDisable(GL_STENCIL_TEST);
   glDisable(GL_SCISSOR_TEST);
   glEnable(GL_TEXTURE_2D);

   /* Multi-texture not part of GL 1.1 */
   /*glActiveTexture(GL_TEXTURE0);*/

#ifndef VITA
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
   glPixelStorei(GL_UNPACK_ROW_LENGTH, pot_width);
#endif
   glBindTexture(GL_TEXTURE_2D, tex);

   frame = (uint8_t*)frame_to_copy;
   if (!gl1->supports_bgra)
   {
      frame_rgba = (uint8_t*)malloc(pot_width * pot_height * 4);
      if (frame_rgba)
      {
         int x, y;
         for (y = 0; y < pot_height; y++)
         {
            for (x = 0; x < pot_width; x++)
            {
               int index             = (y * pot_width + x) * 4;
               frame_rgba[index + 2] = frame[index + 0];
               frame_rgba[index + 1] = frame[index + 1];
               frame_rgba[index + 0] = frame[index + 2];
               frame_rgba[index + 3] = frame[index + 3];
            }
         }
         frame = frame_rgba;
      }
   }

   glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, pot_width, pot_height, 0, format, type, frame);
   if (frame_rgba)
       free(frame_rgba);

   if (tex == gl1->tex)
   {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (gl1->smooth ? GL_LINEAR : GL_NEAREST));
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (gl1->smooth ? GL_LINEAR : GL_NEAREST));
   }
   else if (tex == gl1->menu_tex)
   {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (gl1->menu_smooth ? GL_LINEAR : GL_NEAREST));
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (gl1->menu_smooth ? GL_LINEAR : GL_NEAREST));
   }

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

   glMatrixMode(GL_PROJECTION);
   glPushMatrix();
   glLoadIdentity();

   glMatrixMode(GL_MODELVIEW);
   glPushMatrix();
   glLoadIdentity();

   if (gl1->rotation && tex == gl1->tex)
      glRotatef(gl1->rotation, 0.0f, 0.0f, 1.0f);
   
   glEnableClientState(GL_COLOR_ARRAY);
   glEnableClientState(GL_VERTEX_ARRAY);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
   
   glColorPointer(4, GL_FLOAT, 0, colors);
   glVertexPointer(3, GL_FLOAT, 0, vertices);
   glTexCoordPointer(2, GL_FLOAT, 0, texcoords);

   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   
   glDisableClientState(GL_TEXTURE_COORD_ARRAY);
   glDisableClientState(GL_VERTEX_ARRAY);
   glDisableClientState(GL_COLOR_ARRAY);

   glMatrixMode(GL_MODELVIEW);
   glPopMatrix();
   glMatrixMode(GL_PROJECTION);
   glPopMatrix();
}

static void gl1_readback(
      gl1_t *gl1,
      unsigned alignment,
      unsigned fmt, unsigned type,
      void *src)
{
#ifndef VITA
   glPixelStorei(GL_PACK_ALIGNMENT, alignment);
   glPixelStorei(GL_PACK_ROW_LENGTH, 0);
   glReadBuffer(GL_BACK);
#endif
   glReadPixels(gl1->vp.x, gl1->vp.y,
         gl1->vp.width, gl1->vp.height,
         (GLenum)fmt, (GLenum)type, (GLvoid*)src);
}

static bool gl1_gfx_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   gfx_ctx_mode_t mode;
   const void *frame_to_copy = NULL;
   unsigned width            = 0;
   unsigned height           = 0;
   unsigned bits             = gl1_video_bits;
   bool draw                 = true;
   gl1_t *gl1                = (gl1_t*)data;
   unsigned pot_width        = 0;
   unsigned pot_height       = 0;

   gl1_context_bind_hw_render(gl1, false);
   
   /* FIXME: Force these settings off as they interfere with the rendering */
   video_info->xmb_shadows_enable   = false;
   video_info->menu_shader_pipeline = 0;

   if (!frame || !frame_width || !frame_height)
      return true;

   if (gl1->should_resize)
   {
      gfx_ctx_mode_t mode;

      gl1->should_resize = false;

      mode.width        = width;
      mode.height       = height;

      video_info->cb_set_resize(video_info->context_data,
            mode.width, mode.height);

      gl1_gfx_set_viewport(gl1, video_info, video_info->width, video_info->height, false, true);
   }

   glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
   glClear(GL_COLOR_BUFFER_BIT);

   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   if (  gl1_video_width  != frame_width  ||
         gl1_video_height != frame_height ||
         gl1_video_pitch  != pitch)
   {
      if (frame_width > 4 && frame_height > 4)
      {
         gl1_video_width  = frame_width;
         gl1_video_height = frame_height;
         gl1_video_pitch  = pitch;

         pot_width = get_pot(frame_width);
         pot_height = get_pot(frame_height);

         if (gl1_video_buf)
            free(gl1_video_buf);

         gl1_video_buf = (unsigned char*)malloc(pot_width * pot_height * 4);
      }
   }

   width         = gl1_video_width;
   height        = gl1_video_height;
   pitch         = gl1_video_pitch;

   pot_width = get_pot(width);
   pot_height = get_pot(height);

   if (  frame_width  == 4 &&
         frame_height == 4 &&
         (frame_width < width && frame_height < height)
      )
      draw = false;

   if (draw && gl1_video_buf)
   {
      if (bits == 32)
      {
         unsigned y;
         /* copy lines into top-left portion of larger (power-of-two) buffer */
         for (y = 0; y < height; y++)
            memcpy(gl1_video_buf + ((pot_width * (bits / 8)) * y), (const unsigned char*)frame + (pitch * y), width * (bits / 8));
      }
      else if (bits == 16)
         conv_rgb565_argb8888(gl1_video_buf, frame, width, height, pot_width * sizeof(unsigned), pitch);

      frame_to_copy = gl1_video_buf;
   }

   if (gl1->video_width != width || gl1->video_height != height)
   {
      gl1->video_width  = width;
      gl1->video_height = height;
   }

   video_context_driver_get_video_size(&mode);

   gl1->screen_width           = mode.width;
   gl1->screen_height          = mode.height;

   if (draw)
   {
      if (frame_to_copy)
      {
         draw_tex(gl1, pot_width, pot_height, width, height, gl1->tex, frame_to_copy);
      }
   }

   if (gl1_menu_frame && video_info->menu_is_alive)
   {
      frame_to_copy = NULL;
      width         = gl1_menu_width;
      height        = gl1_menu_height;
      pitch         = gl1_menu_pitch;
      bits          = gl1_menu_bits;

      pot_width = get_pot(width);
      pot_height = get_pot(height);

      if (gl1_menu_size_changed)
      {
         gl1_menu_size_changed = false;

         if (gl1_menu_video_buf)
         {
            free(gl1_menu_video_buf);
            gl1_menu_video_buf = NULL;
         }
      }

      if (!gl1_menu_video_buf)
         gl1_menu_video_buf = (unsigned char*)malloc(pot_width * pot_height * 4);

      if (bits == 16 && gl1_menu_video_buf)
      {
         conv_rgba4444_argb8888(gl1_menu_video_buf, gl1_menu_frame, width, height, pot_width * sizeof(unsigned), pitch);

         frame_to_copy = gl1_menu_video_buf;

         if (gl1->menu_texture_full_screen)
         {
            glViewport(0, 0, video_info->width, video_info->height);
            draw_tex(gl1, pot_width, pot_height, width, height, gl1->menu_tex, frame_to_copy);
            glViewport(gl1->vp.x, gl1->vp.y, gl1->vp.width, gl1->vp.height);
         }
         else
            draw_tex(gl1, pot_width, pot_height, width, height, gl1->menu_tex, frame_to_copy);
      }
   }

#ifdef HAVE_MENU
   if (gl1->menu_texture_enable)
      menu_driver_frame(video_info);
   else if (video_info->statistics_show)
   {
      struct font_params *osd_params = (struct font_params*)
         &video_info->osd_stat_params;

      if (osd_params)
      {
         font_driver_render_msg(gl1, video_info, video_info->stat_text,
               (const struct font_params*)&video_info->osd_stat_params, NULL);
#if 0
         osd_params->y               = 0.350f;
         osd_params->scale           = 0.75f;
         font_driver_render_msg(gl1, video_info, video_info->chat_text,
               (const struct font_params*)&video_info->osd_stat_params, NULL);
#endif
      }
   }

#ifdef HAVE_MENU_WIDGETS
   if (video_info->widgets_inited)
      menu_widgets_frame(video_info);
#endif
#endif

#ifdef HAVE_OVERLAY
   if (gl1->overlay_enable)
      gl1_render_overlay(gl1, video_info);
#endif

   if (msg)
      font_driver_render_msg(gl1, video_info, msg, NULL, NULL);

   video_info->cb_update_window_title(
         video_info->context_data, video_info);

   /* Screenshots. */
   if (gl1->readback_buffer_screenshot)
      gl1_readback(gl1,
            4, GL_RGBA, GL_UNSIGNED_BYTE,
            gl1->readback_buffer_screenshot);

   /* emscripten has to do black frame insertion in its main loop */
#ifndef EMSCRIPTEN
   /* Disable BFI during fast forward, slow-motion,
    * and pause to prevent flicker. */
   if (
         video_info->black_frame_insertion
         && !video_info->input_driver_nonblock_state
         && !video_info->runloop_is_slowmotion
         && !video_info->runloop_is_paused)
   {
      video_info->cb_swap_buffers(video_info->context_data, video_info);
      glClear(GL_COLOR_BUFFER_BIT);
   }
#endif

   video_info->cb_swap_buffers(video_info->context_data, video_info);

   /* check if we are fast forwarding or in menu, if we are ignore hard sync */
   if (video_info->hard_sync
         && !video_info->input_driver_nonblock_state
         && !gl1->menu_texture_enable)
   {
      glClear(GL_COLOR_BUFFER_BIT);
      glFinish();
   }
 
   gl1_context_bind_hw_render(gl1, true);

   return true;
}

static void gl1_gfx_set_nonblock_state(void *data, bool state)
{
   int interval                = 0;
   gl1_t             *gl1      = (gl1_t*)data;
   settings_t        *settings = config_get_ptr();

   if (!gl1)
      return;

   RARCH_LOG("[GL1]: VSync => %s\n", state ? "off" : "on");

   gl1_context_bind_hw_render(gl1, false);

   if (!state)
      interval = settings->uints.video_swap_interval;

   if (gl1->ctx_driver->swap_interval)
   {
      bool adaptive_vsync_enabled            = video_driver_test_all_flags(
            GFX_CTX_FLAGS_ADAPTIVE_VSYNC) && settings->bools.video_adaptive_vsync;
      if (adaptive_vsync_enabled && interval == 1)
         interval = -1;
      gl1->ctx_driver->swap_interval(gl1->ctx_data, interval);
   }
   gl1_context_bind_hw_render(gl1, true);
}

static bool gl1_gfx_alive(void *data)
{
   unsigned temp_width  = 0;
   unsigned temp_height = 0;
   bool quit            = false;
   bool resize          = false;
   bool ret             = false;
   bool is_shutdown     = rarch_ctl(RARCH_CTL_IS_SHUTDOWN, NULL);
   gl1_t *gl1           = (gl1_t*)data;

   /* Needed because some context drivers don't track their sizes */
   video_driver_get_size(&temp_width, &temp_height);

   gl1->ctx_driver->check_window(gl1->ctx_data,
            &quit, &resize, &temp_width, &temp_height, is_shutdown);

   if (resize)
      gl1->should_resize = true;

   ret = !quit;

   if (temp_width != 0 && temp_height != 0)
      video_driver_set_size(&temp_width, &temp_height);

   return ret;
}

static bool gl1_gfx_focus(void *data)
{
   (void)data;
   return true;
}

static bool gl1_gfx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static void gl1_gfx_free(void *data)
{
   gl1_t *gl1 = (gl1_t*)data;

   gl1_context_bind_hw_render(gl1, false);

   if (gl1_menu_frame)
   {
      free(gl1_menu_frame);
      gl1_menu_frame = NULL;
   }

   if (gl1_video_buf)
   {
      free(gl1_video_buf);
      gl1_video_buf = NULL;
   }

   if (gl1_menu_video_buf)
   {
      free(gl1_menu_video_buf);
      gl1_menu_video_buf = NULL;
   }

   if (!gl1)
      return;

   if (gl1->tex)
   {
      glDeleteTextures(1, &gl1->tex);
      gl1->tex = 0;
   }

   if (gl1->menu_tex)
   {
      glDeleteTextures(1, &gl1->menu_tex);
      gl1->menu_tex = 0;
   }

#ifdef HAVE_OVERLAY
   gl1_free_overlay(gl1);
#endif

   if (gl1->extensions)
   {
      string_list_free(gl1->extensions);
      gl1->extensions = NULL;
   }

   font_driver_free_osd();
   video_context_driver_free();
   free(gl1);
}

static bool gl1_gfx_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

static void gl1_gfx_set_rotation(void *data,
      unsigned rotation)
{
   gl1_t *gl1 = (gl1_t*)data;

   if (!gl1)
      return;

   gl1->rotation = 90 * rotation;
   gl1_set_projection(gl1, &gl1_default_ortho, true);
}

static void gl1_gfx_viewport_info(void *data,
      struct video_viewport *vp)
{
   unsigned width, height;
   unsigned top_y, top_dist;
   gl1_t *gl1             = (gl1_t*)data;

   video_driver_get_size(&width, &height);

   *vp             = gl1->vp;
   vp->full_width  = width;
   vp->full_height = height;

   /* Adjust as GL viewport is bottom-up. */
   top_y           = vp->y + vp->height;
   top_dist        = height - top_y;
   vp->y           = top_dist;
}

static bool gl1_gfx_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   unsigned num_pixels = 0;
   gl1_t *gl1 = (gl1_t*)data;

   if (!gl1)
      return false;

   gl1_context_bind_hw_render(gl1, false);

   num_pixels = gl1->vp.width * gl1->vp.height;

   gl1->readback_buffer_screenshot = malloc(num_pixels * sizeof(uint32_t));

   if (!gl1->readback_buffer_screenshot)
      goto error;

   if (!is_idle)
      video_driver_cached_frame();

   video_frame_convert_rgba_to_bgr(
         (const void*)gl1->readback_buffer_screenshot,
         buffer,
         num_pixels);

   free(gl1->readback_buffer_screenshot);
   gl1->readback_buffer_screenshot = NULL;

   gl1_context_bind_hw_render(gl1, true);
   return true;

error:
   gl1_context_bind_hw_render(gl1, true);

   return false;
}

static void gl1_set_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   settings_t *settings = config_get_ptr();
   unsigned pitch = width * 2;
   gl1_t *gl1 = (gl1_t*)data;

   if (!gl1)
      return;

   gl1->menu_smooth = settings->bools.menu_linear_filter;

   gl1_context_bind_hw_render(gl1, false);

   if (rgb32)
      pitch = width * 4;

   if (gl1_menu_frame)
   {
      free(gl1_menu_frame);
      gl1_menu_frame = NULL;
   }

   if ( !gl1_menu_frame            ||
         gl1_menu_width != width   ||
         gl1_menu_height != height ||
         gl1_menu_pitch != pitch)
   {
      if (pitch && height)
      {
         if (gl1_menu_frame)
            free(gl1_menu_frame);

         /* FIXME? We have to assume the pitch has no extra padding in it because that will mess up the POT calculation when we don't know how many bpp there are. */
         gl1_menu_frame = (unsigned char*)malloc(pitch * height);
      }
   }

   if (gl1_menu_frame && frame && pitch && height)
   {
      memcpy(gl1_menu_frame, frame, pitch * height);
      gl1_menu_width  = width;
      gl1_menu_height = height;
      gl1_menu_pitch  = pitch;
      gl1_menu_bits   = rgb32 ? 32 : 16;
      gl1_menu_size_changed = true;
   }

   gl1_context_bind_hw_render(gl1, true);
}

static void gl1_get_video_output_size(void *data,
      unsigned *width, unsigned *height)
{
   gfx_ctx_size_t size_data;
   size_data.width  = width;
   size_data.height = height;
   video_context_driver_get_video_output_size(&size_data);
}

static void gl1_get_video_output_prev(void *data)
{
   video_context_driver_get_video_output_prev();
}

static void gl1_get_video_output_next(void *data)
{
   video_context_driver_get_video_output_next();
}

static void gl1_set_video_mode(void *data, unsigned width, unsigned height,
      bool fullscreen)
{
   gfx_ctx_mode_t mode;

   mode.width      = width;
   mode.height     = height;
   mode.fullscreen = fullscreen;

   video_context_driver_set_video_mode(&mode);
}

static unsigned gl1_wrap_type_to_enum(enum gfx_wrap_type type)
{
   switch (type)
   {
      case RARCH_WRAP_REPEAT:
      case RARCH_WRAP_MIRRORED_REPEAT: /* mirrored not actually supported */
         return GL_REPEAT;
      default:
         return GL_CLAMP;
   }

   return 0;
}

static void gl1_load_texture_data(
      GLuint id,
      enum gfx_wrap_type wrap_type,
      enum texture_filter_type filter_type,
      unsigned alignment,
      unsigned width, unsigned height,
      const void *frame, unsigned base_size)
{
   GLint mag_filter, min_filter;
   bool use_rgba    = video_driver_supports_rgba();
   bool rgb32       = (base_size == (sizeof(uint32_t)));
   GLenum wrap      = gl1_wrap_type_to_enum(wrap_type);

   /* GL1.x does not have mipmapping support. */
   switch (filter_type)
   {
      case TEXTURE_FILTER_MIPMAP_NEAREST:
      case TEXTURE_FILTER_NEAREST:
         min_filter = GL_NEAREST;
         mag_filter = GL_NEAREST;
         break;
      case TEXTURE_FILTER_MIPMAP_LINEAR:
      case TEXTURE_FILTER_LINEAR:
      default:
         min_filter = GL_LINEAR;
         mag_filter = GL_LINEAR;
         break;
   }

   gl1_bind_texture(id, wrap, mag_filter, min_filter);

#ifndef VITA
   glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
#endif

   glTexImage2D(GL_TEXTURE_2D,
         0,
         (use_rgba || !rgb32) ? GL_RGBA : RARCH_GL1_INTERNAL_FORMAT32,
         width, height, 0,
         (use_rgba || !rgb32) ? GL_RGBA : RARCH_GL1_TEXTURE_TYPE32,
         (rgb32) ? RARCH_GL1_FORMAT32 : GL_UNSIGNED_BYTE, frame);
}

static void video_texture_load_gl1(
      struct texture_image *ti,
      enum texture_filter_type filter_type,
      uintptr_t *id)
{
   /* Generate the OpenGL texture object */
   glGenTextures(1, (GLuint*)id);
   gl1_load_texture_data((GLuint)*id,
         RARCH_WRAP_EDGE, filter_type,
         4 /* TODO/FIXME - dehardcode */,
         ti->width, ti->height, ti->pixels,
         sizeof(uint32_t) /* TODO/FIXME - dehardcode */
         );
}

#ifdef HAVE_THREADS
static int video_texture_load_wrap_gl1(void *data)
{
   uintptr_t id = 0;

   if (!data)
      return 0;
   video_texture_load_gl1((struct texture_image*)data,
         TEXTURE_FILTER_NEAREST, &id);
   return (int)id;
}
#endif

static uintptr_t gl1_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   uintptr_t id = 0;

#ifdef HAVE_THREADS
   if (threaded)
   {
      custom_command_method_t func = video_texture_load_wrap_gl1;
      return video_thread_texture_load(data, func);
   }
#endif

   video_texture_load_gl1((struct texture_image*)data, filter_type, &id);
   return id;
}

static void gl1_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   gl1_t *gl1         = (gl1_t*)data;

   if (!gl1)
      return;

   gl1->keep_aspect   = true;
   gl1->should_resize = true;
}

static void gl1_unload_texture(void *data, uintptr_t id)
{
   GLuint glid;
   if (!id)
      return;

   glid = (GLuint)id;
   glDeleteTextures(1, &glid);
}

static float gl1_get_refresh_rate(void *data)
{
   float refresh_rate = 0.0f;
   if (video_context_driver_get_refresh_rate(&refresh_rate))
      return refresh_rate;
   return 0.0f;
}

static void gl1_set_texture_enable(void *data, bool state, bool full_screen)
{
   gl1_t *gl1                    = (gl1_t*)data;

   if (!gl1)
      return;

   gl1->menu_texture_enable      = state;
   gl1->menu_texture_full_screen = full_screen;
}

static uint32_t gl1_get_flags(void *data)
{
   uint32_t             flags = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_HARD_SYNC);
   BIT32_SET(flags, GFX_CTX_FLAGS_BLACK_FRAME_INSERTION);
   BIT32_SET(flags, GFX_CTX_FLAGS_MENU_FRAME_FILTERING);

   return flags;
}

static const video_poke_interface_t gl1_poke_interface = {
   gl1_get_flags,
   gl1_load_texture,
   gl1_unload_texture,
   gl1_set_video_mode,
   gl1_get_refresh_rate,
   NULL,
   gl1_get_video_output_size,
   gl1_get_video_output_prev,
   gl1_get_video_output_next,
   NULL,
   NULL,
   gl1_set_aspect_ratio,
   NULL,
   gl1_set_texture_frame,
   gl1_set_texture_enable,
   font_driver_render_msg,
   NULL,
   NULL,                         /* grab_mouse_toggle */
   NULL,                         /* get_current_shader */
   NULL,                         /* get_current_software_framebuffer */
   NULL                          /* get_hw_render_interface */
};

static void gl1_gfx_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &gl1_poke_interface;
}

#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
static bool gl1_menu_widgets_enabled(void *data)
{
   (void)data;
   return true;
}
#endif

static void gl1_gfx_set_viewport_wrapper(void *data, unsigned viewport_width,
      unsigned viewport_height, bool force_full, bool allow_rotate)
{
   video_frame_info_t video_info;
   gl1_t               *gl1 = (gl1_t*)data;

   video_driver_build_info(&video_info);

   gl1_gfx_set_viewport(gl1, &video_info,
         viewport_width, viewport_height, force_full, allow_rotate);
}

bool gl1_has_menu_frame(void)
{
   return (gl1_menu_frame != NULL);
}

#ifdef HAVE_OVERLAY
static bool gl1_overlay_load(void *data,
      const void *image_data, unsigned num_images)
{
   unsigned i, j;
   gl1_t *gl = (gl1_t*)data;
   const struct texture_image *images =
      (const struct texture_image*)image_data;

   if (!gl)
      return false;

   gl1_context_bind_hw_render(gl, false);

   gl1_free_overlay(gl);
   gl->overlay_tex = (GLuint*)
      calloc(num_images, sizeof(*gl->overlay_tex));

   if (!gl->overlay_tex)
   {
      gl1_context_bind_hw_render(gl, true);
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

   gl->overlays             = num_images;
   glGenTextures(num_images, gl->overlay_tex);

   for (i = 0; i < num_images; i++)
   {
      unsigned alignment = video_pixel_get_alignment(images[i].width
            * sizeof(uint32_t));

      gl1_load_texture_data(gl->overlay_tex[i],
            RARCH_WRAP_EDGE, TEXTURE_FILTER_LINEAR,
            alignment,
            images[i].width, images[i].height, images[i].pixels,
            sizeof(uint32_t));

      /* Default. Stretch to whole screen. */
      gl1_overlay_tex_geom(gl, i, 0, 0, 1, 1);
      gl1_overlay_vertex_geom(gl, i, 0, 0, 1, 1);

      for (j = 0; j < 16; j++)
         gl->overlay_color_coord[16 * i + j] = 1.0f;
   }

   gl1_context_bind_hw_render(gl, true);
   return true;
}

static void gl1_overlay_enable(void *data, bool state)
{
   gl1_t *gl           = (gl1_t*)data;

   if (!gl)
      return;

   gl->overlay_enable = state;

   if (gl->fullscreen && gl->ctx_driver->show_mouse)
      gl->ctx_driver->show_mouse(gl->ctx_data, state);
}

static void gl1_overlay_full_screen(void *data, bool enable)
{
   gl1_t *gl = (gl1_t*)data;

   if (gl)
      gl->overlay_full_screen = enable;
}

static void gl1_overlay_set_alpha(void *data, unsigned image, float mod)
{
   GLfloat *color = NULL;
   gl1_t *gl      = (gl1_t*)data;
   if (!gl)
      return;

   color          = (GLfloat*)&gl->overlay_color_coord[image * 16];

   color[ 0 + 3]  = mod;
   color[ 4 + 3]  = mod;
   color[ 8 + 3]  = mod;
   color[12 + 3]  = mod;
}

static const video_overlay_interface_t gl1_overlay_interface = {
   gl1_overlay_enable,
   gl1_overlay_load,
   gl1_overlay_tex_geom,
   gl1_overlay_vertex_geom,
   gl1_overlay_full_screen,
   gl1_overlay_set_alpha,
};

static void gl1_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface)
{
   (void)data;
   *iface = &gl1_overlay_interface;
}

#endif

video_driver_t video_gl1 = {
   gl1_gfx_init,
   gl1_gfx_frame,
   gl1_gfx_set_nonblock_state,
   gl1_gfx_alive,
   gl1_gfx_focus,
   gl1_gfx_suppress_screensaver,
   NULL, /* has_windowed */
   gl1_gfx_set_shader,
   gl1_gfx_free,
   "gl1",
   gl1_gfx_set_viewport_wrapper,
   gl1_gfx_set_rotation,
   gl1_gfx_viewport_info,
   gl1_gfx_read_viewport,
   NULL, /* read_frame_raw */

#ifdef HAVE_OVERLAY
   gl1_get_overlay_interface,
#endif
#ifdef HAVE_VIDEO_LAYOUT
  NULL,
#endif
  gl1_gfx_get_poke_interface,
  gl1_wrap_type_to_enum,
#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
  gl1_menu_widgets_enabled
#endif
};
