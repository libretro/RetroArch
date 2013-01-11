/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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
#include "../../ps3/sdk_defines.h"

#ifndef __PSL1GHT__
#include <sys/spu_initialize.h>
#endif

#include <stdint.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../gl_common.h"
#include "../image.h"

#include "../gfx_context.h"
#include "../fonts/gl_font.h"

#ifdef HAVE_GLSL
#include "../shader_glsl.h"
#endif

#ifdef HAVE_CG
#include "../shader_cg.h"
#endif

#if defined(HAVE_RMENU)
GLuint menu_texture_id;
static struct texture_image menu_texture;
#endif

#if defined(HAVE_PSGL)
static PSGLdevice* gl_device;
static PSGLcontext* gl_context;
#endif


#define HARDCODE_FONT_SIZE 0.91f
#define POSITION_X 0.09f
#define POSITION_X_CENTER 0.5f
#define POSITION_Y_START 0.17f
#define POSITION_Y_INCREMENT 0.035f
#define POSITION_Y_BEGIN (POSITION_Y_START + POSITION_Y_INCREMENT)
#define COMMENT_TWO_Y_POSITION 0.91f
#define COMMENT_Y_POSITION 0.82f

#define MSG_QUEUE_X_POSITION g_settings.video.msg_pos_x
#define MSG_QUEUE_Y_POSITION 0.76f
#define MSG_QUEUE_FONT_SIZE 1.03f

#define MSG_PREV_NEXT_Y_POSITION 0.03f
#define CURRENT_PATH_Y_POSITION 0.15f

#define NUM_ENTRY_PER_PAGE 15

#define DRIVE_MAPPING_SIZE 4

const char drive_mappings[DRIVE_MAPPING_SIZE][32] = {
   "/app_home/",
   "/dev_hdd0/",
   "/dev_hdd1/",
   "/host_root/"
};

unsigned char drive_mapping_idx = 1;

static int gfx_ctx_check_resolution(unsigned resolution_id)
{
   return cellVideoOutGetResolutionAvailability(CELL_VIDEO_OUT_PRIMARY, resolution_id, CELL_VIDEO_OUT_ASPECT_AUTO, 0);
}

static unsigned gfx_ctx_get_resolution_width(unsigned resolution_id)
{
   CellVideoOutResolution resolution;
   cellVideoOutGetResolution(resolution_id, &resolution);

   return resolution.width;
}

static unsigned gfx_ctx_get_resolution_height(unsigned resolution_id)
{
   CellVideoOutResolution resolution;
   cellVideoOutGetResolution(resolution_id, &resolution);

   return resolution.height;
}

static float gfx_ctx_get_aspect_ratio(void)
{
   CellVideoOutState videoState;
   cellVideoOutGetState(CELL_VIDEO_OUT_PRIMARY, 0, &videoState);

   switch (videoState.displayMode.aspect)
   {
      case CELL_VIDEO_OUT_ASPECT_4_3:
         return 4.0f/3.0f;
      case CELL_VIDEO_OUT_ASPECT_16_9:
         return 16.0f/9.0f;
   }

   return 16.0f/9.0f;
}

static void gfx_ctx_ps3_set_default_pos(rmenu_default_positions_t *position)
{
   position->x_position = POSITION_X;
   position->x_position_center = POSITION_X_CENTER;
   position->y_position = POSITION_Y_BEGIN;
   position->comment_y_position = COMMENT_Y_POSITION;
   position->y_position_increment = POSITION_Y_INCREMENT;
   position->starting_y_position = POSITION_Y_START;
   position->comment_two_y_position = COMMENT_TWO_Y_POSITION;
   position->font_size = HARDCODE_FONT_SIZE;
   position->msg_queue_x_position = MSG_QUEUE_X_POSITION;
   position->msg_queue_y_position = MSG_QUEUE_Y_POSITION;
   position->msg_queue_font_size= MSG_QUEUE_FONT_SIZE;
   position->msg_prev_next_y_position = MSG_PREV_NEXT_Y_POSITION;
   position->current_path_font_size = g_settings.video.font_size;
   position->current_path_y_position = CURRENT_PATH_Y_POSITION;
   position->variable_font_size = g_settings.video.font_size;
   position->entries_per_page = NUM_ENTRY_PER_PAGE;
   position->core_msg_x_position = 0.3f;
   position->core_msg_y_position = 0.06f;
   position->core_msg_font_size = COMMENT_Y_POSITION;
}

static void rmenu_ctx_ps3_screenshot_enable(bool enable)
{
#if(CELL_SDK_VERSION > 0x340000)
   if(enable)
   {
      cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_SCREENSHOT);
      CellScreenShotSetParam screenshot_param = {0, 0, 0, 0};

      screenshot_param.photo_title = "RetroArch PS3";
      screenshot_param.game_title = "RetroArch PS3";
      cellScreenShotSetParameter (&screenshot_param);
      cellScreenShotEnable();
   }
   else
   {
      cellScreenShotDisable();
      cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_SCREENSHOT);
   }
#endif
}

static void rmenu_ctx_ps3_screenshot_dump(void *data)
{
   (void)data;
}

static const char * rmenu_ctx_ps3_drive_mapping_previous(void)
{
   if(drive_mapping_idx > 0)
      drive_mapping_idx--;
   return drive_mappings[drive_mapping_idx];
}

static const char * rmenu_ctx_ps3_drive_mapping_next(void)
{
   if((drive_mapping_idx + 1) < DRIVE_MAPPING_SIZE)
      drive_mapping_idx++;
   return drive_mappings[drive_mapping_idx];
}

static void gfx_ctx_get_available_resolutions (void)
{
   bool defaultresolution;
   uint32_t resolution_count;
   uint16_t num_videomodes;

   if (g_extern.console.screen.resolutions.check)
      return;

   defaultresolution = true;

   uint32_t videomode[] = {
      CELL_VIDEO_OUT_RESOLUTION_480,
      CELL_VIDEO_OUT_RESOLUTION_576,
      CELL_VIDEO_OUT_RESOLUTION_960x1080,
      CELL_VIDEO_OUT_RESOLUTION_720,
      CELL_VIDEO_OUT_RESOLUTION_1280x1080,
      CELL_VIDEO_OUT_RESOLUTION_1440x1080,
      CELL_VIDEO_OUT_RESOLUTION_1600x1080,
      CELL_VIDEO_OUT_RESOLUTION_1080
   };

   num_videomodes = sizeof(videomode) / sizeof(uint32_t);

   resolution_count = 0;
   for (unsigned i = 0; i < num_videomodes; i++)
   {
      if(gfx_ctx_check_resolution(videomode[i]))
         resolution_count++;
   }

   g_extern.console.screen.resolutions.list = malloc(resolution_count * sizeof(uint32_t));
   g_extern.console.screen.resolutions.count = 0;

   for (unsigned i = 0; i < num_videomodes; i++)
   {
      if(gfx_ctx_check_resolution(videomode[i]))
      {
         g_extern.console.screen.resolutions.list[g_extern.console.screen.resolutions.count++] = videomode[i];
         g_extern.console.screen.resolutions.initial.id = videomode[i];

         if (g_extern.console.screen.resolutions.current.id == videomode[i])
         {
            defaultresolution = false;
            g_extern.console.screen.resolutions.current.idx = g_extern.console.screen.resolutions.count-1;
         }
      }
   }

   /* In case we didn't specify a resolution - make the last resolution
      that was added to the list (the highest resolution) the default resolution */
   if (g_extern.console.screen.resolutions.current.id > num_videomodes || defaultresolution)
      g_extern.console.screen.resolutions.current.idx = g_extern.console.screen.resolutions.count - 1;

   g_extern.console.screen.resolutions.check = true;
}

static void gfx_ctx_set_swap_interval(unsigned interval)
{
#if defined(HAVE_PSGL)
   if (gl_context)
   {
      if (interval)
         glEnable(GL_VSYNC_SCE);
      else
         glDisable(GL_VSYNC_SCE);
   }
#endif
}

static void gfx_ctx_check_window(bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   gl_t *gl = driver.video_data;
   *quit = false;
   *resize = false;

#ifdef HAVE_SYSUTILS
   cellSysutilCheckCallback();
#endif

   if (gl->quitting)
      *quit = true;

   if (gl->should_resize)
      *resize = true;
}

static bool gfx_ctx_has_focus(void)
{
   return true;
}

static void gfx_ctx_swap_buffers(void)
{
#if defined(HAVE_PSGL)
   psglSwap();
#endif
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

static void gfx_ctx_set_resize(unsigned width, unsigned height) { }

bool menu_bg_inited = false;

static bool gfx_ctx_rmenu_init(void)
{
   gl_t *gl = driver.video_data;

   if (!gl)
      return false;

   if (menu_bg_inited)
      return false;

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

   menu_bg_inited = true;
#endif

   return true;
}

#if defined(HAVE_RMENU)
static void gfx_ctx_rmenu_free(void)
{
   menu_bg_inited = false;
}

static void gfx_ctx_rmenu_frame(void *data)
{
   gl_t *gl = (gl_t*)data;

   gl_shader_use(gl, RARCH_CG_MENU_SHADER_INDEX);
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
#endif

static void gfx_ctx_update_window_title(bool reset) { }

static void gfx_ctx_get_video_size(unsigned *width, unsigned *height)
{
#if defined(HAVE_PSGL)
   psglGetDeviceDimensions(gl_device, width, height); 
#endif
}

static bool gfx_ctx_init(void)
{
#if defined(HAVE_PSGL)
   PSGLinitOptions options = {
      .enable = PSGL_INIT_MAX_SPUS | PSGL_INIT_INITIALIZE_SPUS,
      .maxSPUs = 1,
      .initializeSPUs = GL_FALSE,
   };
#if CELL_SDK_VERSION < 0x340000
   options.enable |=	PSGL_INIT_HOST_MEMORY_SIZE;
#endif

   // Initialize 6 SPUs but reserve 1 SPU as a raw SPU for PSGL
   sys_spu_initialize(6, 1);
   psglInit(&options);

   PSGLdeviceParameters params;

   params.enable = PSGL_DEVICE_PARAMETERS_COLOR_FORMAT |
      PSGL_DEVICE_PARAMETERS_DEPTH_FORMAT |
      PSGL_DEVICE_PARAMETERS_MULTISAMPLING_MODE;
   params.colorFormat = GL_ARGB_SCE;
   params.depthFormat = GL_NONE;
   params.multisamplingMode = GL_MULTISAMPLING_NONE_SCE;

   if (g_extern.lifecycle_menu_state & (1 << MODE_VIDEO_TRIPLE_BUFFERING_ENABLE))
   {
      params.enable |= PSGL_DEVICE_PARAMETERS_BUFFERING_MODE;
      params.bufferingMode = PSGL_BUFFERING_MODE_TRIPLE;
   }

   if (g_extern.console.screen.resolutions.current.id)
   {
      params.enable |= PSGL_DEVICE_PARAMETERS_WIDTH_HEIGHT;
      params.width = gfx_ctx_get_resolution_width(g_extern.console.screen.resolutions.current.id);
      params.height = gfx_ctx_get_resolution_height(g_extern.console.screen.resolutions.current.id);
   }

   gl_device = psglCreateDeviceExtended(&params);
   gl_context = psglCreateContext();

   psglMakeCurrent(gl_context, gl_device);
   psglResetCurrentContext();
#endif

   return true;
}

static bool gfx_ctx_set_video_mode(
      unsigned width, unsigned height,
      bool fullscreen)
{
   return true;
}

static void gfx_ctx_destroy(void)
{
#if defined(HAVE_PSGL)
   psglDestroyContext(gl_context);
   psglDestroyDevice(gl_device);

   psglExit();
#endif
}

static void gfx_ctx_input_driver(const input_driver_t **input, void **input_data) { }

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
   else if (index >= 2 && gl->fbo_inited)
   {
      glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[index - 2]);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, set_smooth ? GL_LINEAR : GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, set_smooth ? GL_LINEAR : GL_NEAREST);
   }

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
}

static void gfx_ctx_set_fbo(unsigned mode)
{
   gl_t *gl = driver.video_data;

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
}

static void gfx_ctx_set_overscan(void)
{
   gl_t *gl = driver.video_data;
   if (!gl)
      return;

   gl->should_resize = true;
}

static bool gfx_ctx_bind_api(enum gfx_ctx_api api)
{
   return api == GFX_CTX_OPENGL_API || GFX_CTX_OPENGL_ES_API;
}

static bool gfx_ctx_init_egl_image_buffer(const video_info_t *video)
{
   return false;
}

static bool gfx_ctx_write_egl_image(const void *frame, unsigned width, unsigned height, unsigned pitch, bool rgb32, unsigned index, void **image_handle)
{
   return false;
}

const gfx_ctx_driver_t gfx_ctx_ps3 = {
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
   "ps3",
#ifdef HAVE_RMENU
   gfx_ctx_set_blend,
   gfx_ctx_set_filtering,
   gfx_ctx_get_available_resolutions,
   gfx_ctx_check_resolution,
   gfx_ctx_set_fbo,
   gfx_ctx_rmenu_init,
   gfx_ctx_rmenu_frame,
   gfx_ctx_rmenu_free,
   gfx_ctx_menu_draw_bg,
   gfx_ctx_menu_draw_panel,
   gfx_ctx_ps3_set_default_pos,
   rmenu_ctx_ps3_screenshot_enable,
   rmenu_ctx_ps3_screenshot_dump,
   rmenu_ctx_ps3_drive_mapping_previous,
   rmenu_ctx_ps3_drive_mapping_next,
#endif
};

