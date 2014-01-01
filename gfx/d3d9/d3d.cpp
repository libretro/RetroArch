/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 *  Copyright (C) 2012-2014 - OV2
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

#include "d3d9.hpp"

#include "../gfx_common.h"
#include "../../compat/posix_string.h"
#include "../../performance.h"

static bool d3d_frame(void *data, const void *frame,
      unsigned width, unsigned height, unsigned pitch,
      const char *msg)
{
   return reinterpret_cast<D3DVideo*>(data)->frame(frame,
         width, height, pitch, msg);
}

static void d3d_set_nonblock_state(void *data, bool state)
{
   reinterpret_cast<D3DVideo*>(data)->set_nonblock_state(state);
}

static bool d3d_alive(void *data)
{
   return reinterpret_cast<D3DVideo*>(data)->alive();
}

static bool d3d_focus(void *data)
{
   return reinterpret_cast<D3DVideo*>(data)->focus();
}

static void d3d_set_rotation(void *data, unsigned rot)
{
   reinterpret_cast<D3DVideo*>(data)->set_rotation(rot);
}

static void d3d_free(void *data)
{
   delete reinterpret_cast<D3DVideo*>(data);
}

static void d3d_viewport_info(void *data, struct rarch_viewport *vp)
{
   reinterpret_cast<D3DVideo*>(data)->viewport_info(*vp);
}

static bool d3d_read_viewport(void *data, uint8_t *buffer)
{
   return reinterpret_cast<D3DVideo*>(data)->read_viewport(buffer);
}

static bool d3d_set_shader(void *data, enum rarch_shader_type type, const char *path)
{
   std::string shader = "";
   if (path && type == RARCH_SHADER_CG)
      shader = path;

   return reinterpret_cast<D3DVideo*>(data)->set_shader(shader);
}

#ifdef HAVE_MENU
static void d3d_get_poke_interface(void *data, const video_poke_interface_t **iface);
#endif

#ifdef HAVE_OVERLAY
static bool d3d_overlay_load(void *data, const texture_image *images, unsigned num_images)
{
   return reinterpret_cast<D3DVideo*>(data)->overlay_load(images, num_images);
}

static void d3d_overlay_tex_geom(void *data,
      unsigned index,
      float x, float y,
      float w, float h)
{
   return reinterpret_cast<D3DVideo*>(data)->overlay_tex_geom(index, x, y, w, h);
}

static void d3d_overlay_vertex_geom(void *data,
      unsigned index,
      float x, float y,
      float w, float h)
{
   return reinterpret_cast<D3DVideo*>(data)->overlay_vertex_geom(index, x, y, w, h);
}

static void d3d_overlay_enable(void *data, bool state)
{
   return reinterpret_cast<D3DVideo*>(data)->overlay_enable(state);
}

static void d3d_overlay_full_screen(void *data, bool enable)
{
   return reinterpret_cast<D3DVideo*>(data)->overlay_full_screen(enable);
}

static void d3d_overlay_set_alpha(void *data, unsigned index, float mod)
{
   return reinterpret_cast<D3DVideo*>(data)->overlay_set_alpha(index, mod);
}

static const video_overlay_interface_t d3d_overlay_interface = {
   d3d_overlay_enable,
   d3d_overlay_load,
   d3d_overlay_tex_geom,
   d3d_overlay_vertex_geom,
   d3d_overlay_full_screen,
   d3d_overlay_set_alpha,
};

static void d3d_get_overlay_interface(void *data, const video_overlay_interface_t **iface)
{
   (void)data;
   *iface = &d3d_overlay_interface;
}
#endif

static void d3d_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);

   switch (aspect_ratio_idx)
   {
      case ASPECT_RATIO_SQUARE:
         gfx_set_square_pixel_viewport(g_extern.system.av_info.geometry.base_width, g_extern.system.av_info.geometry.base_height);
         break;

      case ASPECT_RATIO_CORE:
         gfx_set_core_viewport();
         break;

      case ASPECT_RATIO_CONFIG:
         gfx_set_config_viewport();
         break;

      default:
         break;
   }

   g_extern.system.aspect_ratio = aspectratio_lut[aspect_ratio_idx].value;
   d3d->info().force_aspect = true;
   d3d->should_resize = true;
   return;
}

static void d3d_apply_state_changes(void *data)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);
   d3d->should_resize = true;
}

static void d3d_set_osd_msg(void *data, const char *msg, void *userdata)
{
   font_params_t *params = (font_params_t*)userdata;
   reinterpret_cast<D3DVideo*>(data)->render_msg(msg, params);
}

static void d3d_show_mouse(void *data, bool state)
{
   reinterpret_cast<D3DVideo*>(data)->show_cursor(state);
}

#ifdef HAVE_MENU
static void d3d_set_rgui_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   reinterpret_cast<D3DVideo*>(data)->set_rgui_texture_frame(frame, rgb32, width, height, alpha);
}

static void d3d_set_rgui_texture_enable(void *data, bool state, bool full_screen)
{
   reinterpret_cast<D3DVideo*>(data)->set_rgui_texture_enable(state, full_screen);
}
#endif

static const video_poke_interface_t d3d_poke_interface = {
   NULL,
#ifdef HAVE_FBO
   NULL,
   NULL,
#endif
   d3d_set_aspect_ratio,
   d3d_apply_state_changes,
#ifdef HAVE_MENU
   d3d_set_rgui_texture_frame,
   d3d_set_rgui_texture_enable,
#endif
   d3d_set_osd_msg,

   d3d_show_mouse,
};

static void d3d_get_poke_interface(void *data, const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &d3d_poke_interface;
}

static void *d3d_init(const video_info_t *info, const input_driver_t **input,
      void **input_data)
{
   D3DVideo *vid = new D3DVideo(info, input, input_data);

   if (!vid)
   {
      RARCH_ERR("[D3D]: Failed to init D3D.\n");
      return NULL;
   }

   return vid;
}

const video_driver_t video_d3d = {
   d3d_init,
   d3d_frame,
   d3d_set_nonblock_state,
   d3d_alive,
   d3d_focus,
   d3d_set_shader,
   d3d_free,
   "d3d9",
#ifdef HAVE_MENU
   NULL,
#endif
   d3d_set_rotation,
   d3d_viewport_info,
   d3d_read_viewport,
#ifdef HAVE_OVERLAY
   d3d_get_overlay_interface,
#endif
   d3d_get_poke_interface
};
