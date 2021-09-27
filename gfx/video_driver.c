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

#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "video_driver.h"

#include "../ui/ui_companion_driver.h"
#include "../verbosity.h"

typedef struct
{
   struct string_list *list;
   enum gfx_ctx_api api;
} gfx_api_gpu_map;

static gfx_api_gpu_map gpu_map[] = {
   { NULL,                   GFX_CTX_VULKAN_API     },
   { NULL,                   GFX_CTX_DIRECT3D10_API },
   { NULL,                   GFX_CTX_DIRECT3D11_API },
   { NULL,                   GFX_CTX_DIRECT3D12_API }
};

video_driver_t *hw_render_context_driver(
      enum retro_hw_context_type type, int major, int minor)
{
   switch (type)
   {
      case RETRO_HW_CONTEXT_OPENGL_CORE:
#ifdef HAVE_OPENGL_CORE
         return &video_gl_core;
#else
         break;
#endif
      case RETRO_HW_CONTEXT_OPENGL:
#ifdef HAVE_OPENGL
         return &video_gl2;
#else
         break;
#endif
      case RETRO_HW_CONTEXT_DIRECT3D:
#if defined(HAVE_D3D9)
         if (major == 9)
            return &video_d3d9;
#endif
#if defined(HAVE_D3D11)
         if (major == 11)
            return &video_d3d11;
#endif
         break;
      case RETRO_HW_CONTEXT_VULKAN:
#if defined(HAVE_VULKAN)
         return &video_vulkan;
#else
         break;
#endif
      default:
      case RETRO_HW_CONTEXT_NONE:
         break;
   }

   return NULL;
}

const char *hw_render_context_name(
      enum retro_hw_context_type type, int major, int minor)
{
#ifdef HAVE_OPENGL_CORE
   if (type == RETRO_HW_CONTEXT_OPENGL_CORE)
      return "glcore";
#endif
#ifdef HAVE_OPENGL
   switch (type)
   {
      case RETRO_HW_CONTEXT_OPENGLES2:
      case RETRO_HW_CONTEXT_OPENGLES3:
      case RETRO_HW_CONTEXT_OPENGLES_VERSION:
      case RETRO_HW_CONTEXT_OPENGL:
#ifndef HAVE_OPENGL_CORE
      case RETRO_HW_CONTEXT_OPENGL_CORE:
#endif
         return "gl";
      default:
         break;
   }
#endif
#ifdef HAVE_VULKAN
   if (type == RETRO_HW_CONTEXT_VULKAN)
      return "vulkan";
#endif
#ifdef HAVE_D3D11
   if (type == RETRO_HW_CONTEXT_DIRECT3D && major == 11)
      return "d3d11";
#endif
#ifdef HAVE_D3D9
   if (type == RETRO_HW_CONTEXT_DIRECT3D && major == 9)
      return "d3d9";
#endif
   return "N/A";
}

enum retro_hw_context_type hw_render_context_type(const char *s)
{
#ifdef HAVE_OPENGL_CORE
   if (string_is_equal(s, "glcore"))
      return RETRO_HW_CONTEXT_OPENGL_CORE;
#endif
#ifdef HAVE_OPENGL
   if (string_is_equal(s, "gl"))
      return RETRO_HW_CONTEXT_OPENGL;
#endif
#ifdef HAVE_VULKAN
   if (string_is_equal(s, "vulkan"))
      return RETRO_HW_CONTEXT_VULKAN;
#endif
#ifdef HAVE_D3D11
   if (string_is_equal(s, "d3d11"))
      return RETRO_HW_CONTEXT_DIRECT3D;
#endif
#ifdef HAVE_D3D11
   if (string_is_equal(s, "d3d9"))
      return RETRO_HW_CONTEXT_DIRECT3D;
#endif
   return RETRO_HW_CONTEXT_NONE;
}

/* string list stays owned by the caller and must be available at
 * all times after the video driver is inited */
void video_driver_set_gpu_api_devices(
      enum gfx_ctx_api api, struct string_list *list)
{
   int i;

   for (i = 0; i < ARRAY_SIZE(gpu_map); i++)
   {
      if (api == gpu_map[i].api)
      {
         gpu_map[i].list = list;
         break;
      }
   }
}

struct string_list* video_driver_get_gpu_api_devices(enum gfx_ctx_api api)
{
   int i;

   for (i = 0; i < ARRAY_SIZE(gpu_map); i++)
   {
      if (api == gpu_map[i].api)
         return gpu_map[i].list;
   }

   return NULL;
}


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
      int mouse_x,           int mouse_y,
      int16_t *res_x,        int16_t *res_y,
      int16_t *res_screen_x, int16_t *res_screen_y)
{
   int norm_vp_width         = (int)vp->width;
   int norm_vp_height        = (int)vp->height;
   int norm_full_vp_width    = (int)vp->full_width;
   int norm_full_vp_height   = (int)vp->full_height;
   int scaled_screen_x       = -0x8000; /* OOB */
   int scaled_screen_y       = -0x8000; /* OOB */
   int scaled_x              = -0x8000; /* OOB */
   int scaled_y              = -0x8000; /* OOB */
   if (norm_vp_width       <= 0 ||
       norm_vp_height      <= 0 ||
       norm_full_vp_width  <= 0 ||
       norm_full_vp_height <= 0)
      return false;

   if (mouse_x >= 0 && mouse_x <= norm_full_vp_width)
      scaled_screen_x = ((2 * mouse_x * 0x7fff)
            / norm_full_vp_width)  - 0x7fff;

   if (mouse_y >= 0 && mouse_y <= norm_full_vp_height)
      scaled_screen_y = ((2 * mouse_y * 0x7fff)
            / norm_full_vp_height) - 0x7fff;

   mouse_x           -= vp->x;
   mouse_y           -= vp->y;

   if (mouse_x >= 0 && mouse_x <= norm_vp_width)
      scaled_x        = ((2 * mouse_x * 0x7fff)
            / norm_vp_width) - 0x7fff;
   else
      scaled_x        = -0x8000; /* OOB */

   if (mouse_y >= 0 && mouse_y <= norm_vp_height)
      scaled_y        = ((2 * mouse_y * 0x7fff)
            / norm_vp_height) - 0x7fff;

   *res_x             = scaled_x;
   *res_y             = scaled_y;
   *res_screen_x      = scaled_screen_x;
   *res_screen_y      = scaled_screen_y;
   return true;
}

/**
 * video_monitor_set_refresh_rate:
 * @hz                 : New refresh rate for monitor.
 *
 * Sets monitor refresh rate to new value.
 **/
void video_monitor_set_refresh_rate(float hz)
{
   char msg[128];
   settings_t        *settings = config_get_ptr();

   snprintf(msg, sizeof(msg),
         "Setting refresh rate to: %.3f Hz.", hz);
   if (settings->bools.notification_show_refresh_rate)
      runloop_msg_queue_push(msg, 1, 180, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   RARCH_LOG("[Video]: %s\n", msg);

   configuration_set_float(settings,
         settings->floats.video_refresh_rate,
         hz);
}

void video_driver_force_fallback(const char *driver)
{
   settings_t *settings        = config_get_ptr();
   ui_msg_window_t *msg_window = NULL;

   configuration_set_string(settings,
         settings->arrays.video_driver,
         driver);

   command_event(CMD_EVENT_MENU_SAVE_CURRENT_CONFIG, NULL);

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__) && !defined(WINAPI_FAMILY)
   /* UI companion driver is not inited yet, just call into it directly */
   msg_window = &ui_msg_window_win32;
#endif

   if (msg_window)
   {
      char text[PATH_MAX_LENGTH];
      ui_msg_window_state window_state;
      char *title          = strdup(msg_hash_to_str(MSG_ERROR));

      text[0]              = '\0';

      snprintf(text, sizeof(text),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER_FALLBACK),
            driver);

      window_state.buttons = UI_MSG_WINDOW_OK;
      window_state.text    = strdup(text);
      window_state.title   = title;
      window_state.window  = NULL;

      msg_window->error(&window_state);

      free(title);
   }
   exit(1);
}
