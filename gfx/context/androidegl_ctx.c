/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#include "../../driver.h"
#include "../gfx_common.h"
#include "../gl_common.h"

#include <EGL/egl.h> /* Requires NDK r5 or newer */
#include <GLES/gl.h>

#include "../../android/native/jni/android_general.h"
#include "../image.h"

#include "../fonts/gl_font.h"
#include <stdint.h>

#if defined(HAVE_RMENU)
GLuint menu_texture_id;
static struct texture_image menu_texture;
#endif

#ifdef HAVE_GLSL
#include "../shader_glsl.h"
#endif

static EGLContext g_egl_ctx;
static EGLSurface g_egl_surf;
static EGLDisplay g_egl_dpy;
static EGLConfig g_config;

GLfloat _angle;

static enum gfx_ctx_api g_api;

static float gfx_ctx_get_aspect_ratio(void)
{
   return 4.0f / 3.0f;
}

static void gfx_ctx_set_swap_interval(unsigned interval)
{
   RARCH_LOG("gfx_ctx_set_swap_interval(%d).\n", interval);
   eglSwapInterval(g_egl_dpy, interval);
}

static void gfx_ctx_destroy(void)
{
   RARCH_LOG("gfx_ctx_destroy().\n");
   eglMakeCurrent(g_egl_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
   eglDestroyContext(g_egl_dpy, g_egl_ctx);
   eglDestroySurface(g_egl_dpy, g_egl_surf);
   eglTerminate(g_egl_dpy);

   g_egl_dpy = EGL_NO_DISPLAY;
   g_egl_surf = EGL_NO_SURFACE;
   g_egl_ctx = EGL_NO_CONTEXT;
   g_config   = 0;
}

static void gfx_ctx_get_video_size(unsigned *width, unsigned *height)
{
   (void)width;
   (void)height;

   if (g_egl_dpy)
   {
      EGLint gl_width, gl_height;
      eglQuerySurface(g_egl_dpy, g_egl_surf, EGL_WIDTH, &gl_width);
      eglQuerySurface(g_egl_dpy, g_egl_surf, EGL_HEIGHT, &gl_height);
      *width = gl_width;
      *height = gl_height;
   }
}

static bool gfx_ctx_init(void)
{
   RARCH_LOG("gfx_ctx_init().\n");
   const EGLint attribs[] = {
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_BLUE_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_RED_SIZE, 8,
      EGL_NONE
   };
   EGLint num_config;
   EGLint egl_version_major, egl_version_minor;
   EGLint format;
   EGLint width;
   EGLint height;
   GLfloat ratio;

   EGLint context_attributes[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE
   };

   RARCH_LOG("Initializing context\n");

   if ((g_egl_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY)) == EGL_NO_DISPLAY)
   {
      RARCH_ERR("eglGetDisplay failed.\n");
      goto error;
   }

   if (!eglInitialize(g_egl_dpy, &egl_version_major, &egl_version_minor))
   {
      RARCH_ERR("eglInitialize failed.\n");
      goto error;
   }

   RARCH_LOG("[ANDROID/EGL]: EGL version: %d.%d\n", egl_version_major, egl_version_minor);

   if (!eglChooseConfig(g_egl_dpy, attribs, &g_config, 1, &num_config))
   {
      RARCH_ERR("eglChooseConfig failed.\n");
      goto error;
   }

   int var = eglGetConfigAttrib(g_egl_dpy, g_config, EGL_NATIVE_VISUAL_ID, &format);

   if (!var)
   {
      RARCH_ERR("eglGetConfigAttrib failed: %d.\n", var);
      goto error;
   }

   ANativeWindow_setBuffersGeometry(g_android.app->window, 0, 0, format);

   if (!(g_egl_surf = eglCreateWindowSurface(g_egl_dpy, g_config, g_android.app->window, 0)))
   {
      RARCH_ERR("eglCreateWindowSurface failed.\n");
      goto error;
   }

   if (!(g_egl_ctx = eglCreateContext(g_egl_dpy, g_config, 0, context_attributes)))
   {
      RARCH_ERR("eglCreateContext failed.\n");
      goto error;
   }

   if (!eglMakeCurrent(g_egl_dpy, g_egl_surf, g_egl_surf, g_egl_ctx))
   {
      RARCH_ERR("eglMakeCurrent failed.\n");
      goto error;
   }

   if (!eglQuerySurface(g_egl_dpy, g_egl_surf, EGL_WIDTH, &width) ||
         !eglQuerySurface(g_egl_dpy, g_egl_surf, EGL_HEIGHT, &height))
   {
      RARCH_ERR("eglQuerySurface failed.\n");
      goto error;
   }

   if (g_extern.lifecycle_state & (1ULL << RARCH_REENTRANT))
   {
      RARCH_LOG("[ANDROID/EGL]: Setting up reentrant state.\n");

      gl_t *gl = (gl_t*)driver.video_data;

      // Get real known video size, which might have been altered by context.
      gfx_ctx_get_video_size(&gl->win_width, &gl->win_height);
      RARCH_LOG("GL: Using resolution %ux%u\n", gl->win_width, gl->win_height);

      if (gl->full_x || gl->full_y) // We got bogus from gfx_ctx_get_video_size. Replace.
      {
         gl->full_x = gl->win_width;
         gl->full_y = gl->win_height;
      }

#ifdef HAVE_GLSL
      gl_glsl_use(0);
#endif
      gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);
#ifdef HAVE_GLSL
      gl_glsl_use(1);
#endif
      gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);
   }

   return true;

error:
   RARCH_ERR("EGL error: %d.\n", eglGetError());
   gfx_ctx_destroy();
   return false;
}

static void gfx_ctx_swap_buffers(void)
{
   eglSwapBuffers(g_egl_dpy, g_egl_surf);
}

static void gfx_ctx_check_window(bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   (void)width;
   (void)height;
   (void)frame_count;

   int id;
   struct android_app* android_app = g_android.app;

   *quit = false;
   *resize = false;

   RARCH_PERFORMANCE_INIT(alooper_pollonce);
   RARCH_PERFORMANCE_START(alooper_pollonce);
   
   id = ALooper_pollOnce(0, NULL, 0, NULL);

   if(id == LOOPER_ID_MAIN)
   {
      int8_t cmd;

      if (read(android_app->msgread, &cmd, sizeof(cmd)) == sizeof(cmd))
      {
         if(cmd == APP_CMD_SAVE_STATE)
            free_saved_state(android_app);
      }
      else
         cmd = -1;

      engine_handle_cmd(android_app, cmd);
   }

   RARCH_PERFORMANCE_STOP(alooper_pollonce);

   int32_t new_orient = AConfiguration_getOrientation(g_android.app->config);

   if (new_orient != g_android.last_orient)
   {
      *resize = true;
      g_android.last_orient = new_orient;
      // reinit video driver for new window dimensions
      driver.video->free(driver.video_data);
      init_video_input();
   }

   // Check if we are exiting.
   if (g_extern.lifecycle_state & (1ULL << RARCH_QUIT_KEY))
      *quit = true;
}

static void gfx_ctx_set_blend(bool enable)
{
   if(enable)
   {
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      glEnable(GL_BLEND);
   }
   else
      glDisable(GL_BLEND);
}

static void gfx_ctx_set_resize(unsigned width, unsigned height)
{
   (void)width;
   (void)height;
}

static void gfx_ctx_update_window_title(bool reset)
{
   (void)reset;
#if 0
   if (reset)
      gfx_window_title_reset();

   char buf[128];

   if (gfx_get_fps(buf, sizeof(buf), false))
      RARCH_LOG("%s.\n", buf);
#endif
}



static bool gfx_ctx_set_video_mode(
      unsigned width, unsigned height,
      bool fullscreen)
{
   (void)width;
   (void)height;
   (void)fullscreen;
   return true;
}


static void gfx_ctx_input_driver(const input_driver_t **input, void **input_data)
{
   *input = NULL;
   *input_data = NULL;
}

static void gfx_ctx_set_filtering(unsigned index, bool set_smooth)
{
   gl_t *gl = driver.video_data;

   if (!gl)
      return;

   if (index == 1)
   {
      // Apply to all PREV textures.
      for (unsigned i = 0; i < TEXTURES; i++)
      {
         glBindTexture(GL_TEXTURE_2D, gl->texture[i]);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, set_smooth ? GL_LINEAR : GL_NEAREST);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, set_smooth ? GL_LINEAR : GL_NEAREST);
      }
   }
#ifdef HAVE_FBO
   else if (index >= 2 && gl->fbo_inited)
   {
      glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[index - 2]);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, set_smooth ? GL_LINEAR : GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, set_smooth ? GL_LINEAR : GL_NEAREST);
   }
#endif

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
}

static void gfx_ctx_set_fbo(unsigned mode)
{
   gl_t *gl = driver.video_data;

#ifdef HAVE_FBO
   switch(mode)
   {
      case FBO_DEINIT:
         gl_deinit_fbo(gl);
         break;
      case FBO_REINIT:
         gl_deinit_fbo(gl);
         /* fall-through */
      case FBO_INIT:
         gl_init_fbo(gl, gl->tex_w, gl->tex_h);
         break;
   }
#endif
}

static void gfx_ctx_get_available_resolutions (void)
{
   /* TODO */
}

#ifdef HAVE_RMENU
#define DRIVE_MAPPING_SIZE 3
const char drive_mappings[DRIVE_MAPPING_SIZE][32] = {
   "/",
   "/mnt/",
   "/mnt/sdcard"
};
unsigned char drive_mapping_idx = 1;
bool rmenu_inited = false;

static bool gfx_ctx_rmenu_init(void)
{
   gl_t *gl = driver.video_data;

   if (!gl)
      return false;

   if (rmenu_inited)
      return true;

#ifdef HAVE_RMENU
   glGenTextures(1, &menu_texture_id);

   RARCH_LOG("Loading texture image for menu...\n");
   if (!texture_image_load(default_paths.menu_border_file, &menu_texture))
   {
      RARCH_ERR("Failed to load texture image for menu.\n");
      return false;
   }

   glBindTexture(GL_TEXTURE_2D, menu_texture_id);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl->border_type);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl->border_type);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

   glTexImage2D(GL_TEXTURE_2D, 0, RARCH_GL_INTERNAL_FORMAT32,
         menu_texture.width, menu_texture.height, 0,
         RARCH_GL_TEXTURE_TYPE32, RARCH_GL_FORMAT32, menu_texture.pixels);

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

   free(menu_texture.pixels);
#endif

   rmenu_inited = true;

   return true;
}

static void gfx_ctx_rmenu_free(void)
{
   gl_t *gl = driver.video_data;
#ifdef HAVE_RMENU
   gl->draw_rmenu = false;
#endif
}

static void gfx_ctx_rmenu_frame(void *data)
{
   gl_t *gl = (gl_t*)data;

   gl_shader_use(gl, RARCH_GLSL_MENU_SHADER_INDEX);
   gl_set_viewport(gl, gl->win_width, gl->win_height, true, false);

   if (gl->shader)
   {
      gl->shader->set_params(gl->win_width, gl->win_height, 
            gl->win_width, gl->win_height, 
            gl->win_width, gl->win_height, 
            g_extern.frame_count, NULL, NULL, NULL, 0);
   }

   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, menu_texture_id);

   gl->coords.vertex = vertexes_flipped;

   gl_shader_set_coords(gl, &gl->coords, &gl->mvp);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); 

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
   gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);
}

static void gfx_ctx_menu_draw_panel(rarch_position_t *position)
{
   (void)position;
}

static void gfx_ctx_menu_draw_bg(rarch_position_t *position)
{
   (void)position;
}

static const char * rmenu_ctx_drive_mapping_previous(void)
{
   if(drive_mapping_idx > 0)
      drive_mapping_idx--;
   return drive_mappings[drive_mapping_idx];
}

static const char * rmenu_ctx_drive_mapping_next(void)
{
   if((drive_mapping_idx + 1) < DRIVE_MAPPING_SIZE)
      drive_mapping_idx++;
   return drive_mappings[drive_mapping_idx];
}
#endif

static void rmenu_ctx_screenshot_enable(bool enable)
{
   /* TODO */
   (void)enable;
}


static void gfx_ctx_set_overscan(void)
{
   gl_t *gl = driver.video_data;
   if (!gl)
      return;

   gl->should_resize = true;
}

static int gfx_ctx_check_resolution(unsigned resolution_id)
{
   /* TODO */
   return 0;
}

static unsigned gfx_ctx_get_resolution_width(unsigned resolution_id)
{
   int gl_width;
   eglQuerySurface(g_egl_dpy, g_egl_surf, EGL_WIDTH, &gl_width);

   return gl_width;
}

static gfx_ctx_proc_t gfx_ctx_get_proc_address(const char *symbol)
{
   rarch_assert(sizeof(void*) == sizeof(void (*)(void)));
   gfx_ctx_proc_t ret;

   void *sym__ = eglGetProcAddress(symbol);
   memcpy(&ret, &sym__, sizeof(void*));

   return ret;
}

static bool gfx_ctx_bind_api(enum gfx_ctx_api api)
{
   g_api = api;
   return api == GFX_CTX_OPENGL_ES_API;
}

static bool gfx_ctx_has_focus(void)
{
   return true;
}

static bool gfx_ctx_init_egl_image_buffer(const video_info_t *video)
{
   return false;
}

static bool gfx_ctx_write_egl_image(const void *frame, unsigned width, unsigned height, unsigned pitch, bool rgb32, unsigned index, void **image_handle)
{
   return false;
}

const gfx_ctx_driver_t gfx_ctx_android = {
   gfx_ctx_init,
   gfx_ctx_destroy,
   gfx_ctx_bind_api,
   gfx_ctx_set_swap_interval,
   gfx_ctx_set_video_mode,
   gfx_ctx_get_video_size,
   NULL,
   gfx_ctx_update_window_title,
   gfx_ctx_check_window,
   gfx_ctx_set_resize,
   gfx_ctx_has_focus,
   gfx_ctx_swap_buffers,
   gfx_ctx_input_driver,
   NULL,
   gfx_ctx_init_egl_image_buffer,
   gfx_ctx_write_egl_image,
   NULL,
   "android",
#ifdef HAVE_RMENU
   gfx_ctx_set_blend,
   gfx_ctx_set_filtering,
   gfx_ctx_get_available_resolutions,
   gfx_ctx_check_resolution,
   gfx_ctx_set_fbo,
   gfx_ctx_rmenu_init,
   gfx_ctx_rmenu_frame,
   gfx_ctx_rmenu_free,
   gfx_ctx_menu_enable,
   gfx_ctx_menu_draw_bg,
   gfx_ctx_menu_draw_panel,
   gfx_ctx_ps3_set_default_pos,
   rmenu_ctx_render_msg,
   rmenu_ctx_screenshot_enable,
   rmenu_ctx_screenshot_dump,
   rmenu_ctx_drive_mapping_previous,
   rmenu_ctx_drive_mapping_next,
#endif
};
