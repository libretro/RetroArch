/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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

// Loader for external API plugins.

#define RARCH_DLL_IMPORT
#include "ext/rarch_video.h"
#include "../boolean.h"
#include <stdlib.h>
#include <stdint.h>
#include "../dynamic.h"
#include "../general.h"
#include "sdlwrap.h"
#include "gfx_common.h"

#ifdef HAVE_FREETYPE
#include "fonts.h"
#endif

#ifdef HAVE_PYTHON
#define PY_STATE_OMIT_DECLARATION
#include "py_state/py_state.h"
#endif

static bool g_input_dead = true;
static bool g_video_dead = true;
static dylib_t g_lib = NULL;

/////////// Input hook

typedef struct
{
   const rarch_input_driver_t *driver;
   void *handle;
} input_ext_t;

static void *input_ext_init(void)
{
   g_input_dead = false;
   return calloc(1, sizeof(input_ext_t));
}

static void input_ext_poll(void *data)
{
   input_ext_t *ext = (input_ext_t*)data;
   ext->driver->poll(ext->handle);
}

static int16_t input_ext_input_state(void *data, const struct snes_keybind **snes_keybinds, unsigned port, unsigned device, unsigned index, unsigned id)
{
   input_ext_t *ext = (input_ext_t*)data;

   unsigned player = port + 1;

   if (id < RARCH_BIND_LIST_END)
   {
      const struct snes_keybind *rarch_bind = &snes_keybinds[player - 1][id];
      if (!rarch_bind->valid)
         return 0;

      struct rarch_keybind bind = {0};
      bind.key = rarch_bind->key;
      bind.joykey = rarch_bind->joykey;
      bind.joyaxis = rarch_bind->joyaxis;

      return ext->driver->input_state(ext->handle, &bind, player);
   }
   else
      return 0;
}

static bool input_ext_key_pressed(void *data, int key)
{
   input_ext_t *ext = (input_ext_t*)data;

   if (key >= 0 && key < RARCH_BIND_LIST_END)
   {
      const struct snes_keybind *rarch_bind = &g_settings.input.binds[0][key];
      if (!rarch_bind->valid)
         return false;

      struct rarch_keybind bind = {0};
      bind.key = rarch_bind->key;
      bind.joykey = rarch_bind->joykey;
      bind.joyaxis = rarch_bind->joyaxis;

      return ext->driver->input_state(ext->handle, &bind, 1);
   }
   else
      return false;
}

static void input_ext_free(void *data)
{
   input_ext_t *ext = (input_ext_t*)data;
   if (ext)
   {
      if (ext->driver && ext->handle)
         ext->driver->free(ext->handle);

      if (g_video_dead)
      {
         dylib_close(g_lib);
         g_lib = NULL;
      }

      g_input_dead = true;

      free(ext);
   }
}

static const input_driver_t input_ext = {
   input_ext_init,
   input_ext_poll,
   input_ext_input_state,
   input_ext_key_pressed,
   input_ext_free,
   "ext"
};

//////////// Video hook
typedef struct
{
   const rarch_video_driver_t *driver;
   void *handle;
} ext_t;

static void video_ext_free(void *data)
{
   ext_t *ext = (ext_t*)data;
   if (ext)
   {
      if (ext->driver && ext->handle)
         ext->driver->free(ext->handle);

      if (g_input_dead)
      {
         dylib_close(g_lib);
         g_lib = NULL;
      }

      g_video_dead = true;

      free(ext);
   }
}

static bool video_ext_focus(void *data)
{
   ext_t *ext = (ext_t*)data;
   return ext->driver->focus(ext->handle);
}

static bool video_ext_alive(void *data)
{
   ext_t *ext = (ext_t*)data;
   return ext->driver->alive(ext->handle);
}

static void video_ext_set_nonblock_state(void *data, bool state)
{
   ext_t *ext = (ext_t*)data;
   ext->driver->set_nonblock_state(ext->handle, state);
}

static bool video_ext_frame(void *data, const void *frame, unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   ext_t *ext = (ext_t*)data;
   return ext->driver->frame(ext->handle, frame, width, height, pitch, msg);
}

static void *setup_input(ext_t *ext, const rarch_input_driver_t *driver)
{
   // TODO: Change external API to allow more players. To be done in next major ABI break.
   int joypad_index[5];
   for (unsigned i = 0; i < 5; i++)
      joypad_index[i] = g_settings.input.joypad_map[i] < 0 ? -1 : g_settings.input.joypad_map[i];

   void *handle = driver->init(joypad_index, g_settings.input.axis_threshold);
   if (!handle)
      return NULL;

   input_ext_t *wrap_handle = (input_ext_t*)input_ext.init();
   if (!wrap_handle)
      return NULL;

   wrap_handle->handle = handle;
   wrap_handle->driver = driver;

   return wrap_handle;
}

static bool setup_video(ext_t *ext, const video_info_t *video, const input_driver_t **input, void **input_data)
{
   RARCH_LOG("Loaded driver: \"%s\"\n", ext->driver->ident ? ext->driver->ident : "Unknown");

   if (RARCH_GRAPHICS_API_VERSION != ext->driver->api_version)
   {
      RARCH_ERR("API version mismatch detected.\n");
      RARCH_ERR("Required API version: %d, Library version: %d\n", RARCH_GRAPHICS_API_VERSION, ext->driver->api_version);
      return false;
   }

   const char *cg_shader = NULL;
   const char *xml_shader = NULL;
   enum rarch_shader_type type = g_settings.video.shader_type;
   if ((type == RARCH_SHADER_CG || type == RARCH_SHADER_AUTO) && *g_settings.video.cg_shader_path)
      cg_shader = g_settings.video.cg_shader_path;
   else if ((type == RARCH_SHADER_BSNES || type == RARCH_SHADER_AUTO) && *g_settings.video.bsnes_shader_path)
      xml_shader = g_settings.video.bsnes_shader_path;

   int font_color_r = g_settings.video.msg_color_r * 255;
   int font_color_g = g_settings.video.msg_color_g * 255;
   int font_color_b = g_settings.video.msg_color_b * 255;
   font_color_r = font_color_r > 255 ? 255 : (font_color_r < 0 ? 0 : font_color_r);
   font_color_g = font_color_g > 255 ? 255 : (font_color_g < 0 ? 0 : font_color_g);
   font_color_b = font_color_b > 255 ? 255 : (font_color_b < 0 ? 0 : font_color_b);

   const char *font = NULL;
   if (g_settings.video.font_enable)
   {
#ifdef HAVE_FREETYPE
      if (*g_settings.video.font_path)
         font = g_settings.video.font_path;
      else
         font = font_renderer_get_default_font();
#else
      font = *g_settings.video.font_path ?
         g_settings.video.font_path : NULL;
#endif
   }

   char title_buf[128];
   gfx_window_title_reset();
   gfx_window_title(title_buf, sizeof(title_buf));

   rarch_video_info_t info = {0};
   info.width = video->width;
   info.height = video->height;
   info.fullscreen = video->fullscreen;
   info.vsync = video->vsync;
   info.force_aspect = video->force_aspect;
   info.aspect_ratio = g_settings.video.aspect_ratio;
   info.smooth = video->smooth;
   info.input_scale = video->input_scale;
   info.color_format = video->rgb32 ? RARCH_COLOR_FORMAT_ARGB8888 : RARCH_COLOR_FORMAT_XRGB1555;
   info.xml_shader = xml_shader;
   info.cg_shader = cg_shader;
   info.ttf_font = font;
   info.ttf_font_size = g_settings.video.font_size;
   info.ttf_font_color = (font_color_r << 16) | (font_color_g << 8) | (font_color_b << 0);
   info.title_hint = title_buf;

#ifdef HAVE_PYTHON
   info.python_state_new = py_state_new;
   info.python_state_get = py_state_get;
   info.python_state_free = py_state_free;
#endif

   const rarch_input_driver_t *input_driver = NULL;
   ext->handle = ext->driver->init(&info, &input_driver);
   if (!ext->handle)
      return false;

   *input = input_driver ? &input_ext : NULL;
   if (input_driver)
      *input_data = setup_input(ext, input_driver);
   else
      *input_data = NULL;

   return true;
}

static void *video_ext_init(const video_info_t *video, const input_driver_t **input, void **input_data)
{
   ext_t *ext = (ext_t*)calloc(1, sizeof(*ext));
   if (!ext)
      return NULL;

   const rarch_video_driver_t *(*video_init)(void) = NULL;

   if (!(*g_settings.video.external_driver))
   {
      RARCH_ERR("External driver needs video_external_driver path to be set.\n");
      goto error;
   }

   g_lib = dylib_load(g_settings.video.external_driver);
   if (!g_lib)
   {
      RARCH_ERR("Failed to open library: \"%s\"\n", g_settings.video.external_driver);
      goto error;
   }

   video_init = (const rarch_video_driver_t *(*)(void))dylib_proc(g_lib, "rarch_video_init");
   if (!video_init)
   {
      RARCH_ERR("Couldn't find function rarch_video_init in library ...\n");
      goto error;
   }

   ext->driver = video_init();
   if (!ext->driver)
   {
      RARCH_ERR("External driver returned invalid driver handle.\n");
      goto error;
   }

   if (!setup_video(ext, video, input, input_data))
   {
      RARCH_ERR("Failed to start driver.\n");
      goto error;
   }

   g_video_dead = false;
   return ext;

error:
   video_ext_free(ext);
   return NULL;
}

const video_driver_t video_ext = {
   video_ext_init,
   video_ext_frame,
   video_ext_set_nonblock_state,
   video_ext_alive,
   video_ext_focus,
   NULL,
   video_ext_free,
   "ext"
};

