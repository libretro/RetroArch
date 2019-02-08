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

/* We are targeting a minimum of OpenGL 1.1 and the Microsoft "GDI Generic" software GL implementation.
 * Any additional features added for later 1.x versions should only be enabled if they are detected at runtime. */

#include <stddef.h>
#include <retro_miscellaneous.h>
#include <formats/image.h>
#include <string/stdstring.h>
#include <retro_math.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
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

static bool is_pot(unsigned x)
{
   return (x & (x - 1)) == 0;
}

static unsigned get_pot(unsigned x)
{
   return (is_pot(x) ? x : next_pow2(x));
}

static void gl1_gfx_create(void)
{
}

static void *gl1_gfx_init(const video_info_t *video,
      const input_driver_t **input, void **input_data)
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

   gl1_gfx_create();

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

   /* Clear out potential error flags in case we use cached context. */
   glGetError();

   if (string_is_equal(ctx_driver->ident, "null"))
      goto error;

   if (!string_is_empty(version))
      sscanf(version, "%d.%d", &gl1->version_major, &gl1->version_minor);

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

   video_context_driver_swap_interval(&interval);

   if (!video_context_driver_set_video_mode(&mode))
      goto error;

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

   inp.input      = input;
   inp.input_data = input_data;

   video_context_driver_input_driver(&inp);

   if (settings->bools.video_font_enable)
      font_driver_init_osd(gl1, false,
            video->is_threaded,
            FONT_DRIVER_RENDER_OPENGL1_API);

   vendor   = (const char*)glGetString(GL_VENDOR);
   renderer = (const char*)glGetString(GL_RENDERER);
   version  = (const char*)glGetString(GL_VERSION);
   extensions = (const char*)glGetString(GL_EXTENSIONS);

   gl1->extensions = string_split(extensions, " ");

   RARCH_LOG("[GL1]: Vendor: %s, Renderer: %s.\n", vendor, renderer);
   RARCH_LOG("[GL1]: Version: %s.\n", version);
   RARCH_LOG("[GL1]: Extensions: %s\n", extensions);

   gl1->supports_bgra = string_list_find_elem(gl1->extensions, "GL_EXT_bgra");

   glDisable(GL_BLEND);
   glDisable(GL_DEPTH_TEST);
   glDisable(GL_STENCIL_TEST);
   glDisable(GL_SCISSOR_TEST);
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
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

static void draw_tex(gl1_t *gl1, int pot_width, int pot_height, int tex_width, int height, GLuint tex, const void *frame_to_copy)
{
   /* FIXME: For now, everything is uploaded as BGRA8888, I could not get 444 or 555 to work, and there is no 565 support in GL 1.1 either. */
   GLint internalFormat = GL_RGBA8;
   GLenum format = (gl1->supports_bgra ? GL_BGRA_EXT : GL_RGBA);
   GLenum type = GL_UNSIGNED_BYTE;

   glDisable(GL_DEPTH_TEST);
   glDisable(GL_STENCIL_TEST);
   glDisable(GL_SCISSOR_TEST);
   glEnable(GL_TEXTURE_2D);

   /* multi-texture not part of GL 1.1 */
   /*glActiveTexture(GL_TEXTURE0);*/

   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
   glPixelStorei(GL_UNPACK_ROW_LENGTH, pot_width);
   glBindTexture(GL_TEXTURE_2D, tex);

   /* TODO: We could implement red/blue swap if client GL does not support BGRA... but even MS GDI Generic supports it */
   glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, pot_width, pot_height, 0, format, type, NULL);
   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tex_width, height, format, type, frame_to_copy);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

   glMatrixMode(GL_PROJECTION);
   glPushMatrix();
   glLoadIdentity();
   /*glLoadMatrixf(gl1->mvp.data);*/

   glMatrixMode(GL_MODELVIEW);
   glPushMatrix();
   glLoadIdentity();

   /* stock coord set does not handle POT, disable for now */
   /*glEnableClientState(GL_COLOR_ARRAY);
   glEnableClientState(GL_VERTEX_ARRAY);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);

   glColorPointer(4, GL_FLOAT, 0, gl1->coords.color);
   glVertexPointer(2, GL_FLOAT, 0, gl1->coords.vertex);
   glTexCoordPointer(2, GL_FLOAT, 0, gl1->coords.tex_coord);

   glDrawArrays(GL_TRIANGLES, 0, gl1->coords.vertices);

   glDisableClientState(GL_TEXTURE_COORD_ARRAY);
   glDisableClientState(GL_VERTEX_ARRAY);
   glDisableClientState(GL_COLOR_ARRAY);*/

   if (gl1->rotation)
      glRotatef(gl1->rotation, 0.0f, 0.0f, 1.0f);

   glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

   glBegin(GL_QUADS);

   {
      float tex_BL[2] = {0.0f, 0.0f};
      float tex_BR[2] = {1.0f, 0.0f};
      float tex_TL[2] = {0.0f, 1.0f};
      float tex_TR[2] = {1.0f, 1.0f};
      float *tex_mirror_BL = tex_TL;
      float *tex_mirror_BR = tex_TR;
      float *tex_mirror_TL = tex_BL;
      float *tex_mirror_TR = tex_BR;
      float norm_width = (1.0f / (float)pot_width) * (float)tex_width;
      float norm_height = (1.0f / (float)pot_height) * (float)height;

      /* remove extra POT padding */
      tex_mirror_BR[0] = norm_width;
      tex_mirror_TR[0] = norm_width;

      /* normally this would be 1.0 - height, but we're drawing upside-down */
      tex_mirror_BL[1] = norm_height;
      tex_mirror_BR[1] = norm_height;

      glTexCoord2f(tex_mirror_BL[0], tex_mirror_BL[1]);
      glVertex2f(-1.0f, -1.0f);

      glTexCoord2f(tex_mirror_TL[0], tex_mirror_TL[1]);
      glVertex2f(-1.0f, 1.0f);

      glTexCoord2f(tex_mirror_TR[0], tex_mirror_TR[1]);
      glVertex2f(1.0f, 1.0f);

      glTexCoord2f(tex_mirror_BR[0], tex_mirror_BR[1]);
      glVertex2f(1.0f, -1.0f);
   }

   glEnd();

   glMatrixMode(GL_MODELVIEW);
   glPopMatrix();
   glMatrixMode(GL_PROJECTION);
   glPopMatrix();
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
   unsigned tex_width        = 0;
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

#ifdef HAVE_MENU
   if (gl1->menu_texture_enable)
      menu_driver_frame(video_info);
#endif

   if (  gl1_video_width  != frame_width  ||
         gl1_video_height != frame_height ||
         gl1_video_pitch  != pitch)
   {
      if (frame_width > 4 && frame_height > 4)
      {
         gl1_video_width  = frame_width;
         gl1_video_height = frame_height;
         gl1_video_pitch  = pitch;

         tex_width = pitch / (bits / 8);
         pot_width = get_pot(tex_width);
         pot_height = get_pot(frame_height);

         if (gl1_video_buf)
            free(gl1_video_buf);

         gl1_video_buf = (unsigned char*)malloc(pot_width * pot_height * 4);
      }
   }

   width         = gl1_video_width;
   height        = gl1_video_height;
   pitch         = gl1_video_pitch;

   tex_width = pitch / (bits / 8);
   pot_width = get_pot(tex_width);
   pot_height = get_pot(height);

   if (  frame_width  == 4 &&
         frame_height == 4 &&
         (frame_width < width && frame_height < height)
      )
      draw = false;

   if (draw && gl1_video_buf)
   {
      unsigned x, y;

      if (bits == 32)
      {
         for (y = 0; y < height; y++)
         {
            /* copy lines into top-left portion of larger (power-of-two) buffer */
            memcpy(gl1_video_buf + ((pot_width * (bits / 8)) * y), frame + (pitch * y), pitch);
         }
      }
      else if (bits == 16)
      {
         /* change bit depth from 565 to 8888 */
         for (y = 0; y < height; y++)
         {
            for (x = 0; x < tex_width; x++)
            {
               unsigned pixel = ((unsigned short*)frame)[tex_width * y + x];
               unsigned *new_pixel = (unsigned*)gl1_video_buf + (pot_width * y + x);
               unsigned r = (255.0f / 31.0f) * ((pixel & 0xF800) >> 11);
               unsigned g = (255.0f / 63.0f) * ((pixel & 0x7E0) >> 5);
               unsigned b = (255.0f / 31.0f) * ((pixel & 0x1F) >> 0);
               /* copy pixels into top-left portion of larger (power-of-two) buffer */
               *new_pixel = 0xFF000000 | (r << 16) | (g << 8) | b;
            }
         }
      }

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

   glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
   glClear(GL_COLOR_BUFFER_BIT);

   if (draw)
   {
      if (frame_to_copy)
         draw_tex(gl1, pot_width, pot_height, tex_width, height, gl1->tex, frame_to_copy);
   }

   if (gl1_menu_frame && video_info->menu_is_alive)
   {
      unsigned x, y;
      frame_to_copy = NULL;
      width         = gl1_menu_width;
      height        = gl1_menu_height;
      pitch         = gl1_menu_pitch;
      bits          = gl1_menu_bits;

      tex_width = pitch / (bits / 8);
      pot_width = get_pot(tex_width);
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
         /* change bit depth from 4444 to 8888 */
         for (y = 0; y < height; y++)
         {
            for (x = 0; x < tex_width; x++)
            {
               unsigned pixel = ((unsigned short*)gl1_menu_frame)[tex_width * y + x];
               unsigned *new_pixel = (unsigned*)gl1_menu_video_buf + (pot_width * y + x);
               unsigned r = (255.0f / 15.0f) * ((pixel & 0xF000) >> 12);
               unsigned g = (255.0f / 15.0f) * ((pixel & 0xF00) >> 8);
               unsigned b = (255.0f / 15.0f) * ((pixel & 0xF0) >> 4);
               unsigned a = (255.0f / 15.0f) * ((pixel & 0xF) >> 0);
               /* copy pixels into top-left portion of larger (power-of-two) buffer */
               *new_pixel = (a << 24) | (r << 16) | (g << 8) | b;
            }
         }

         frame_to_copy = gl1_menu_video_buf;

         glEnable(GL_BLEND);

         if (gl1->menu_texture_full_screen)
         {
            glViewport(0, 0, video_info->width, video_info->height);
            draw_tex(gl1, pot_width, pot_height, tex_width, height, gl1->menu_tex, frame_to_copy);
            glViewport(gl1->vp.x, gl1->vp.y, gl1->vp.width, gl1->vp.height);
         }
         else
            draw_tex(gl1, pot_width, pot_height, tex_width, height, gl1->menu_tex, frame_to_copy);

         glDisable(GL_BLEND);
      }
   }

   if (msg)
      font_driver_render_msg(video_info, NULL, msg, NULL);

   video_info->cb_update_window_title(
         video_info->context_data, video_info);

   video_info->cb_swap_buffers(video_info->context_data, video_info);

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

   video_context_driver_swap_interval(&interval);
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

static bool gl1_gfx_has_windowed(void *data)
{
   (void)data;
   return true;
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
   (void)data;
   (void)vp;
}

static bool gl1_gfx_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   (void)data;
   (void)buffer;

   return true;
}

static void gl1_set_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   unsigned pitch = width * 2;
   gl1_t *gl1 = (gl1_t*)data;

   if (!gl1)
      return;

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

static void gl1_set_osd_msg(void *data,
      video_frame_info_t *video_info,
      const char *msg,
      const void *params, void *font)
{
   font_driver_render_msg(video_info, font,
         msg, (const struct font_params *)params);
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

static uintptr_t gl1_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   void *tmpdata               = NULL;
   gl1_texture_t *texture      = NULL;
   struct texture_image *image = (struct texture_image*)data;
   int size                    = image->width *
      image->height * sizeof(uint32_t);

   if (!image || image->width > 2048 || image->height > 2048)
      return 0;

   texture                     = (gl1_texture_t*)calloc(1, sizeof(*texture));

   if (!texture)
      return 0;

   texture->width              = image->width;
   texture->height             = image->height;
   texture->active_width       = image->width;
   texture->active_height      = image->height;
   texture->data               = calloc(1,
         texture->width * texture->height * sizeof(uint32_t));
   texture->type               = filter_type;

   if (!texture->data)
   {
      free(texture);
      return 0;
   }

   memcpy(texture->data, image->pixels,
         texture->width * texture->height * sizeof(uint32_t));

   return (uintptr_t)texture;
}

static void gl1_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   gl1_t *gl1         = (gl1_t*)data;

   switch (aspect_ratio_idx)
   {
      case ASPECT_RATIO_SQUARE:
         video_driver_set_viewport_square_pixel();
         break;

      case ASPECT_RATIO_CORE:
         video_driver_set_viewport_core();
         break;

      case ASPECT_RATIO_CONFIG:
         video_driver_set_viewport_config();
         break;

      default:
         break;
   }

   video_driver_set_aspect_ratio_value(
         aspectratio_lut[aspect_ratio_idx].value);

   if (!gl1)
      return;

   gl1->keep_aspect = true;
   gl1->should_resize = true;
}

static void gl1_unload_texture(void *data, uintptr_t handle)
{
   struct gl1_texture *texture = (struct gl1_texture*)handle;

   if (!texture)
      return;

   if (texture->data)
      free(texture->data);

   free(texture);
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

static const video_poke_interface_t gl1_poke_interface = {
   NULL, /* get_flags */
   NULL,                      /* set_coords */
   NULL,                      /* set_mvp */
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
   gl1_set_osd_msg,
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

static unsigned gl1_wrap_type_to_enum(enum gfx_wrap_type type)
{
   switch (type)
   {
      case RARCH_WRAP_REPEAT:
      case RARCH_WRAP_MIRRORED_REPEAT: /* mirrored not actually supported */
         return GL_REPEAT;
      default:
         return GL_CLAMP;
	 break;
   }

   return 0;
}

video_driver_t video_gl1 = {
   gl1_gfx_init,
   gl1_gfx_frame,
   gl1_gfx_set_nonblock_state,
   gl1_gfx_alive,
   gl1_gfx_focus,
   gl1_gfx_suppress_screensaver,
   gl1_gfx_has_windowed,
   gl1_gfx_set_shader,
   gl1_gfx_free,
   "gl1",
   gl1_gfx_set_viewport_wrapper,
   gl1_gfx_set_rotation,
   gl1_gfx_viewport_info,
   gl1_gfx_read_viewport,
   NULL, /* read_frame_raw */

#ifdef HAVE_OVERLAY
  NULL, /* overlay_interface */
#endif
  gl1_gfx_get_poke_interface,
  gl1_wrap_type_to_enum,
#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
   NULL
#endif
};
