/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

/* Direct3D 9 driver.
 *
 * Minimum version : Direct3D 9.0 (2002)
 * Minimum OS      : Windows 98, Windows 2000, Windows ME
 * Recommended OS  : Windows XP
 */

#define CINTERFACE

#ifdef _XBOX
#include <xtl.h>
#include <xgraphics.h>
#endif

#include <formats/image.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <retro_math.h>

#include <d3d9.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <defines/d3d_defines.h>
#include "../common/d3d_common.h"
#include "../common/d3d9_common.h"
#include "../video_coord_array.h"
#include "../../configuration.h"
#include "../../dynamic.h"
#include "../../ui/ui_companion_driver.h"
#include "../../frontend/frontend_driver.h"

#ifdef HAVE_THREADS
#include "../video_thread_wrapper.h"
#endif

#include "../common/win32_common.h"

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif
#ifdef HAVE_GFX_WIDGETS
#include "../gfx_widgets.h"
#endif

#include "../font_driver.h"

#include "../../core.h"
#include "../../verbosity.h"
#include "../../retroarch.h"

#ifdef __WINRT__
#error "UWP does not support D3D9"
#endif

/* Temporary workaround for d3d9 not being able to poll flags during init */
static gfx_ctx_driver_t d3d9_fake_context;

LPDIRECT3D9 g_pD3D9;
static enum rarch_shader_type supported_shader_type = RARCH_SHADER_NONE;

extern d3d9_renderchain_driver_t cg_d3d9_renderchain;
extern d3d9_renderchain_driver_t hlsl_d3d9_renderchain;

static uint32_t d3d9_get_flags(void *data)
{
   uint32_t flags = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_BLACK_FRAME_INSERTION);
   BIT32_SET(flags, GFX_CTX_FLAGS_MENU_FRAME_FILTERING);

   if (supported_shader_type == RARCH_SHADER_CG)
      BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_CG);
   else if (supported_shader_type == RARCH_SHADER_HLSL)
      BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_HLSL);

   return flags;
}

static bool d3d9_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
#if defined(HAVE_CG) || defined(HAVE_HLSL)
   d3d9_video_t *d3d = (d3d9_video_t*)data;

   if (!d3d)
      return false;

   if (!string_is_empty(d3d->shader_path))
      free(d3d->shader_path);
   d3d->shader_path = NULL;

   switch (type)
   {
      case RARCH_SHADER_CG:
      case RARCH_SHADER_HLSL:

         if (type != supported_shader_type)
         {
            RARCH_WARN("[D3D9]: Shader preset %s is using unsupported shader type %s, falling back to stock %s.\n",
               path, video_shader_type_to_str(type), video_shader_type_to_str(supported_shader_type));
            break;
         }
      
         if (!string_is_empty(path))
            d3d->shader_path = strdup(path);

         break;
      case RARCH_SHADER_NONE:
         break;
      default:
         RARCH_WARN("[D3D9]: Only Cg shaders are supported. Falling back to stock.\n");
   }

   if (!d3d9_process_shader(d3d) || !d3d9_restore(d3d))
   {
      RARCH_ERR("[D3D9]: Failed to set shader.\n");
      return false;
   }

   return true;
#else
   return false;
#endif
}


static void d3d9_deinit_chain(d3d9_video_t *d3d)
{
   if (!d3d || !d3d->renderchain_driver)
      return;

   if (d3d->renderchain_driver->chain_free)
      d3d->renderchain_driver->chain_free(d3d->renderchain_data);

   d3d->renderchain_driver = NULL;
   d3d->renderchain_data   = NULL;
}

static bool renderchain_d3d_init_first(
      enum gfx_ctx_api api,
      const d3d9_renderchain_driver_t **renderchain_driver,
      void **renderchain_handle)
{
   switch (api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
            static const d3d9_renderchain_driver_t *renderchain_d3d_drivers[] =
            {
#if defined(_WIN32) && defined(HAVE_CG)
               &cg_d3d9_renderchain,
#endif
#if defined(_WIN32) && defined(HAVE_HLSL)
               &hlsl_d3d9_renderchain,
#endif
               NULL
            };
            unsigned i;

            for (i = 0; renderchain_d3d_drivers[i]; i++)
            {
               void *data = renderchain_d3d_drivers[i]->chain_new();

               if (!data)
                  continue;

               *renderchain_driver = renderchain_d3d_drivers[i];
               *renderchain_handle = data;

               if (string_is_equal(renderchain_d3d_drivers[i]->ident, "cg_d3d9"))
                  supported_shader_type = RARCH_SHADER_CG;
               else if (string_is_equal(renderchain_d3d_drivers[i]->ident, "hlsl_d3d9"))
                  supported_shader_type = RARCH_SHADER_HLSL;

               return true;
            }
         }
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   return false;
}


static bool d3d9_init_chain(d3d9_video_t *d3d,
      unsigned input_scale,
      bool rgb32)
{
   unsigned i = 0;
   struct LinkInfo link_info;
#ifndef _XBOX
   unsigned current_width, current_height, out_width, out_height;
#endif

   /* Setup information for first pass. */
   link_info.pass  = NULL;
   link_info.tex_w = input_scale * RARCH_SCALE_BASE;
   link_info.tex_h = input_scale * RARCH_SCALE_BASE;
   link_info.pass  = &d3d->shader.pass[0];

   if (!renderchain_d3d_init_first(GFX_CTX_DIRECT3D9_API,
            &d3d->renderchain_driver,
            &d3d->renderchain_data))
   {
      RARCH_ERR("[D3D9]: Renderchain could not be initialized.\n");
      return false;
   }

   if (!d3d->renderchain_driver || !d3d->renderchain_data)
      return false;

   if (
         !d3d->renderchain_driver->init(
            d3d,
            d3d->dev, &d3d->final_viewport, &link_info,
            rgb32)
      )
   {
      RARCH_ERR("[D3D9]: Failed to init render chain.\n");
      return false;
   }

   RARCH_LOG("[D3D9]: Renderchain driver: \"%s\".\n", d3d->renderchain_driver->ident);
   d3d9_log_info(&link_info);

#ifndef _XBOX
   current_width  = link_info.tex_w;
   current_height = link_info.tex_h;
   out_width      = 0;
   out_height     = 0;

   for (i = 1; i < d3d->shader.passes; i++)
   {
      d3d9_convert_geometry(
            &link_info,
            &out_width, &out_height,
            current_width, current_height, &d3d->final_viewport);

      link_info.pass  = &d3d->shader.pass[i];
      link_info.tex_w = next_pow2(out_width);
      link_info.tex_h = next_pow2(out_height);

      current_width   = out_width;
      current_height  = out_height;

      if (!d3d->renderchain_driver->add_pass(
               d3d->renderchain_data, &link_info))
      {
         RARCH_ERR("[D3D9]: Failed to add pass.\n");
         return false;
      }
      d3d9_log_info(&link_info);
   }
#endif

   if (d3d->renderchain_driver)
   {
      if (d3d->renderchain_driver->add_lut)
      {
         unsigned i;
         settings_t *settings = config_get_ptr();
         bool video_smooth    = settings->bools.video_smooth;

         for (i = 0; i < d3d->shader.luts; i++)
         {
            if (!d3d->renderchain_driver->add_lut(
                     d3d->renderchain_data,
                     d3d->shader.lut[i].id, d3d->shader.lut[i].path,
                     d3d->shader.lut[i].filter == RARCH_FILTER_UNSPEC 
                     ? video_smooth 
                     : (d3d->shader.lut[i].filter == RARCH_FILTER_LINEAR)))
            {
               RARCH_ERR("[D3D9]: Failed to init LUTs.\n");
               return false;
            }
         }
      }
   }

   return true;
}

static void d3d9_deinitialize(d3d9_video_t *d3d)
{
   if (!d3d)
      return;

   font_driver_free_osd();

   d3d9_deinit_chain(d3d);
   d3d9_vertex_buffer_free(d3d->menu_display.buffer,
         d3d->menu_display.decl);

   d3d->menu_display.buffer = NULL;
   d3d->menu_display.decl   = NULL;
}

static bool d3d9_init_base(void *data, const video_info_t *info)
{
   D3DPRESENT_PARAMETERS d3dpp;
   d3d9_video_t *d3d  = (d3d9_video_t*)data;
#ifndef _XBOX
   HWND focus_window  = win32_get_window();
#endif

   memset(&d3dpp, 0, sizeof(d3dpp));

   g_pD3D9            = (LPDIRECT3D9)d3d9_create();

   /* this needs g_pD3D9 created first */
   d3d9_make_d3dpp(d3d, info, &d3dpp);

   if (!g_pD3D9)
   {
      RARCH_ERR("[D3D9]: Failed to create D3D interface.\n");
      return false;
   }

   if (!d3d9_create_device(&d3d->dev, &d3dpp,
            g_pD3D9,
            focus_window,
            d3d->cur_mon_id)
      )
   {
      RARCH_ERR("[D3D9]: Failed to initialize device.\n");
      return false;
   }

   return true;
}

static void d3d9_set_viewport(void *data,
      unsigned width, unsigned height,
      bool force_full,
      bool allow_rotate)
{
   int x               = 0;
   int y               = 0;
   d3d9_video_t *d3d   = (d3d9_video_t*)data;

   d3d9_calculate_rect(data, &width, &height, &x, &y,
         force_full, allow_rotate);

   /* D3D doesn't support negative X/Y viewports ... */
   if (x < 0)
      x = 0;
   if (y < 0)
      y = 0;

   d3d->final_viewport.X      = x;
   d3d->final_viewport.Y      = y;
   d3d->final_viewport.Width  = width;
   d3d->final_viewport.Height = height;
   d3d->final_viewport.MinZ   = 0.0f;
   d3d->final_viewport.MaxZ   = 1.0f;

   d3d9_set_font_rect(d3d, NULL);
}



static bool d3d9_initialize(d3d9_video_t *d3d, const video_info_t *info)
{
   unsigned width, height;
   bool ret             = true;
   settings_t *settings = config_get_ptr();

   if (!d3d)
      return false;

   if (!g_pD3D9)
      ret = d3d9_init_base(d3d, info);
   else if (d3d->needs_restore)
   {
      D3DPRESENT_PARAMETERS d3dpp;

      d3d9_make_d3dpp(d3d, info, &d3dpp);

      /* the D3DX font driver uses POOL_DEFAULT resources
       * and will prevent a clean reset here
       * another approach would be to keep track of all created D3D
       * font objects and free/realloc them around the d3d_reset call  */
#ifdef HAVE_MENU
      menu_driver_ctl(RARCH_MENU_CTL_DEINIT, NULL);
#endif

      if (!d3d9_reset(d3d->dev, &d3dpp))
      {
         d3d9_deinitialize(d3d);
         d3d9_device_free(NULL, g_pD3D9);
         g_pD3D9 = NULL;

         ret = d3d9_init_base(d3d, info);
         if (ret)
            RARCH_LOG("[D3D9]: Recovered from dead state.\n");
      }

#ifdef HAVE_MENU
      menu_driver_init(info->is_threaded);
#endif
   }

   if (!ret)
      return ret;

   if (!d3d9_init_chain(d3d, info->input_scale, info->rgb32))
   {
      RARCH_ERR("[D3D9]: Failed to initialize render chain.\n");
      return false;
   }

   video_driver_get_size(&width, &height);
   d3d9_set_viewport(d3d,
      width, height, false, true);

   font_driver_init_osd(d3d, info,
         false,
         info->is_threaded,
         FONT_DRIVER_RENDER_D3D9_API);

   {
      static const D3DVERTEXELEMENT9 VertexElements[4] = {
         {0, offsetof(Vertex, x),  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,
            D3DDECLUSAGE_POSITION, 0},
         {0, offsetof(Vertex, u), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,
            D3DDECLUSAGE_TEXCOORD, 0},
         {0, offsetof(Vertex, color), D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT,
            D3DDECLUSAGE_COLOR, 0},
         D3DDECL_END()
      };
      if (!d3d9_vertex_declaration_new(d3d->dev,
               (void*)VertexElements, (void**)&d3d->menu_display.decl))
         return false;
   }

   d3d->menu_display.offset = 0;
   d3d->menu_display.size   = 1024;
   d3d->menu_display.buffer = d3d9_vertex_buffer_new(
         d3d->dev, d3d->menu_display.size * sizeof(Vertex),
         D3DUSAGE_WRITEONLY,
#ifdef _XBOX
         0,
#else
         D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1,
#endif
         D3DPOOL_DEFAULT,
         NULL);

   if (!d3d->menu_display.buffer)
      return false;

   d3d_matrix_ortho_off_center_lh(&d3d->mvp_transposed, 0, 1, 0, 1, 0, 1);
   d3d_matrix_transpose(&d3d->mvp, &d3d->mvp_transposed);

   d3d9_set_render_state(d3d->dev, D3DRS_CULLMODE, D3DCULL_NONE);
   d3d9_set_render_state(d3d->dev, D3DRS_SCISSORTESTENABLE, TRUE);

   return true;
}

static bool d3d9_init_internal(d3d9_video_t *d3d,
      const video_info_t *info, input_driver_t **input,
      void **input_data)
{
#ifdef HAVE_MONITOR
   bool windowed_full;
   RECT mon_rect;
   MONITORINFOEX current_mon;
   HMONITOR hm_to_use;
#endif
#ifdef HAVE_WINDOW
   DWORD style;
   unsigned win_width        = 0;
   unsigned win_height       = 0;
   RECT rect                 = {0};
#endif
   unsigned full_x           = 0;
   unsigned full_y           = 0;
   settings_t    *settings   = config_get_ptr();
   overlay_t *menu           = (overlay_t*)calloc(1, sizeof(*menu));

   if (!menu)
      return false;

   d3d->menu                 = menu;
   d3d->cur_mon_id           = 0;
   d3d->menu->tex_coords[0]  = 0;
   d3d->menu->tex_coords[1]  = 0;
   d3d->menu->tex_coords[2]  = 1;
   d3d->menu->tex_coords[3]  = 1;
   d3d->menu->vert_coords[0] = 0;
   d3d->menu->vert_coords[1] = 1;
   d3d->menu->vert_coords[2] = 1;
   d3d->menu->vert_coords[3] = -1;

#ifdef HAVE_WINDOW
   memset(&d3d->windowClass, 0, sizeof(d3d->windowClass));
   d3d->windowClass.lpfnWndProc = wnd_proc_d3d_common;
#ifdef HAVE_DINPUT
   if (string_is_equal(settings->arrays.input_driver, "dinput"))
      d3d->windowClass.lpfnWndProc = wnd_proc_d3d_dinput;
#endif
#ifdef HAVE_WINRAWINPUT
   if (string_is_equal(settings->arrays.input_driver, "raw"))
      d3d->windowClass.lpfnWndProc = wnd_proc_d3d_winraw;
#endif
   win32_window_init(&d3d->windowClass, true, NULL);
#endif

#ifdef HAVE_MONITOR
   win32_monitor_info(&current_mon, &hm_to_use, &d3d->cur_mon_id);

   mon_rect              = current_mon.rcMonitor;
   g_win32_resize_width  = info->width;
   g_win32_resize_height = info->height;

   windowed_full         = settings->bools.video_windowed_fullscreen;

   full_x                = (windowed_full || info->width  == 0) ?
      (mon_rect.right  - mon_rect.left) : info->width;
   full_y                = (windowed_full || info->height == 0) ?
      (mon_rect.bottom - mon_rect.top)  : info->height;

   RARCH_LOG("[D3D9]: Monitor size: %dx%d.\n",
         (int)(mon_rect.right  - mon_rect.left),
         (int)(mon_rect.bottom - mon_rect.top));
#else
   {
      d3d9_get_video_size(d3d, &full_x, &full_y);
   }
#endif
   {
      unsigned new_width  = info->fullscreen ? full_x : info->width;
      unsigned new_height = info->fullscreen ? full_y : info->height;
      video_driver_set_size(new_width, new_height);
   }

#ifdef HAVE_WINDOW
   video_driver_get_size(&win_width, &win_height);

   win32_set_style(&current_mon, &hm_to_use, &win_width, &win_height,
         info->fullscreen, windowed_full, &rect, &mon_rect, &style);

   win32_window_create(d3d, style, &mon_rect, win_width,
         win_height, info->fullscreen);

   win32_set_window(&win_width, &win_height, info->fullscreen,
      windowed_full, &rect);
#endif

   d3d->video_info = *info;

   if (!d3d9_initialize(d3d, &d3d->video_info))
      return false;

   d3d9_fake_context.get_flags   = d3d9_get_flags;
#ifndef _XBOX_
   d3d9_fake_context.get_metrics = win32_get_metrics;
#endif
   video_context_driver_set(&d3d9_fake_context); 
#if defined(HAVE_CG) || defined(HAVE_HLSL)
   {
      const char *shader_preset   = retroarch_get_shader_preset();
      enum rarch_shader_type type = video_shader_parse_type(shader_preset);

      d3d9_set_shader(d3d, type, shader_preset);
   }
#endif

   d3d_input_driver(settings->arrays.input_joypad_driver,
      settings->arrays.input_joypad_driver, input, input_data);

   {
      char version_str[128];
      D3DADAPTER_IDENTIFIER9 ident = {0};

      IDirect3D9_GetAdapterIdentifier(g_pD3D9, 0, 0, &ident);

      version_str[0] = '\0';

      snprintf(version_str, sizeof(version_str), "%u.%u.%u.%u", HIWORD(ident.DriverVersion.HighPart), LOWORD(ident.DriverVersion.HighPart), HIWORD(ident.DriverVersion.LowPart), LOWORD(ident.DriverVersion.LowPart));

      RARCH_LOG("[D3D9]: Using GPU: \"%s\".\n", ident.Description);
      RARCH_LOG("[D3D9]: GPU API Version: %s\n", version_str);

      video_driver_set_gpu_device_string(ident.Description);
      video_driver_set_gpu_api_version_string(version_str);
   }

   RARCH_LOG("[D3D9]: Init complete.\n");
   return true;
}

static void *d3d9_init(const video_info_t *info,
      input_driver_t **input, void **input_data)
{
   d3d9_video_t *d3d = (d3d9_video_t*)calloc(1, sizeof(*d3d));

   if (!d3d)
      return NULL;

   if (!d3d9_initialize_symbols(GFX_CTX_DIRECT3D9_API))
   {
      free(d3d);
      return NULL;
   }

#ifndef _XBOX
   win32_window_reset();
   win32_monitor_init();
#endif

   /* Default values */
   d3d->dev                  = NULL;
   d3d->dev_rotation         = 0;
   d3d->needs_restore        = false;
#ifdef HAVE_OVERLAY
   d3d->overlays_enabled     = false;
#endif
   d3d->should_resize        = false;
   d3d->menu                 = NULL;

   if (!d3d9_init_internal(d3d, info, input, input_data))
   {
      RARCH_ERR("[D3D9]: Failed to init D3D.\n");
      free(d3d);
      return NULL;
   }

   d3d->keep_aspect       = info->force_aspect;

   return d3d;
}

static void d3d9_free(void *data)
{
   d3d9_video_t   *d3d = (d3d9_video_t*)data;

   if (!d3d)
      return;

#ifdef HAVE_OVERLAY
   d3d9_free_overlays(d3d);
   if (d3d->overlays)
      free(d3d->overlays);
   d3d->overlays      = NULL;
   d3d->overlays_size = 0;
#endif

   d3d9_free_overlay(d3d, d3d->menu);
   if (d3d->menu)
      free(d3d->menu);
   d3d->menu          = NULL;

   d3d9_deinitialize(d3d);

   if (!string_is_empty(d3d->shader_path))
      free(d3d->shader_path);

   d3d->shader_path = NULL;
   d3d9_device_free(d3d->dev, g_pD3D9);
   d3d->dev         = NULL;
   g_pD3D9          = NULL;

   d3d9_deinitialize_symbols();

#ifndef _XBOX
   win32_monitor_from_window();
   win32_destroy_window();
#endif
   free(d3d);
}

bool d3d9_restore(d3d9_video_t *d3d)
{
   d3d9_deinitialize(d3d);

   if (!d3d9_initialize(d3d, &d3d->video_info))
   {
      RARCH_ERR("[D3D9]: Restore error.\n");
      return false;
   }

   d3d->needs_restore = false;

   return true;
}


static bool d3d9_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height,
      uint64_t frame_count, unsigned pitch,
      const char *msg, video_frame_info_t *video_info)
{
   D3DVIEWPORT9 screen_vp;
   unsigned i                          = 0;
   d3d9_video_t *d3d                   = (d3d9_video_t*)data;
   unsigned width                      = video_info->width;
   unsigned height                     = video_info->height;
   bool statistics_show                = video_info->statistics_show;
   unsigned black_frame_insertion      = video_info->black_frame_insertion;
   struct font_params *osd_params      = (struct font_params*)
      &video_info->osd_stat_params;
   const char *stat_text               = video_info->stat_text;
   bool menu_is_alive                  = video_info->menu_is_alive;
   bool overlay_behind_menu            = video_info->overlay_behind_menu;
#ifdef HAVE_GFX_WIDGETS
   bool widgets_active                 = video_info->widgets_active;
#endif

   if (!frame)
      return true;

   /* We cannot recover in fullscreen. */
   if (d3d->needs_restore)
   {
#ifndef _XBOX
      HWND window = win32_get_window();
      if (IsIconic(window))
         return true;
#endif

      if (!d3d9_restore(d3d))
      {
         RARCH_ERR("[D3D9]: Failed to restore.\n");
         return false;
      }
   }

   if (d3d->should_resize)
   {
      d3d9_set_viewport(d3d, width, height, false, true);
      if (d3d->renderchain_driver->set_final_viewport)
         d3d->renderchain_driver->set_final_viewport(d3d,
               d3d->renderchain_data, &d3d->final_viewport);

      d3d->should_resize = false;
   }

   /* render_chain() only clears out viewport,
    * clear out everything. */
   screen_vp.X      = 0;
   screen_vp.Y      = 0;
   screen_vp.MinZ   = 0;
   screen_vp.MaxZ   = 1;
   screen_vp.Width  = width;
   screen_vp.Height = height;
   d3d9_set_viewports(d3d->dev, &screen_vp);
   d3d9_clear(d3d->dev, 0, 0, D3DCLEAR_TARGET, 0, 1, 0);

   d3d9_set_vertex_shader_constantf(d3d->dev, 0, (const float*)&d3d->mvp_transposed, 4);
   if (!d3d->renderchain_driver->render(
            d3d, frame, frame_width, frame_height,
            pitch, d3d->dev_rotation))
   {
      RARCH_ERR("[D3D9]: Failed to render scene.\n");
      return false;
   }
   
   if (black_frame_insertion && !d3d->menu->enabled)
   {
      unsigned n;
      for (n = 0; n < video_info->black_frame_insertion; ++n) 
      {   
        if (!d3d9_swap(d3d, d3d->dev) || d3d->needs_restore)
          return true;
        d3d9_clear(d3d->dev, 0, 0, D3DCLEAR_TARGET, 0, 1, 0);
      }
   }   

#ifdef HAVE_OVERLAY
   if (d3d->overlays_enabled && overlay_behind_menu)
   {
      d3d9_set_vertex_shader_constantf(d3d->dev, 0, (const float*)&d3d->mvp_transposed, 4);
      for (i = 0; i < d3d->overlays_size; i++)
         d3d9_overlay_render(d3d, width, height, &d3d->overlays[i], true);
   }
#endif

#ifdef HAVE_MENU
   if (d3d->menu && d3d->menu->enabled)
   {
      d3d9_set_vertex_shader_constantf(d3d->dev, 0, (const
               float*)&d3d->mvp_transposed, 4);
      d3d9_overlay_render(d3d, width, height, d3d->menu, false);

      d3d->menu_display.offset = 0;
      d3d9_set_vertex_declaration(d3d->dev, (LPDIRECT3DVERTEXDECLARATION9)d3d->menu_display.decl);
      d3d9_set_stream_source(d3d->dev, 0, (LPDIRECT3DVERTEXBUFFER9)d3d->menu_display.buffer, 0, sizeof(Vertex));

      d3d9_set_viewports(d3d->dev, &screen_vp);
      menu_driver_frame(menu_is_alive, video_info);
   }
   else if (statistics_show)
   {
      if (osd_params)
      {
         d3d9_set_viewports(d3d->dev, &screen_vp);
         d3d9_begin_scene(d3d->dev);
         font_driver_render_msg(d3d, stat_text,
               (const struct font_params*)osd_params, NULL);
         d3d9_end_scene(d3d->dev);
      }
   }
#endif

#ifdef HAVE_OVERLAY
   if (d3d->overlays_enabled && !overlay_behind_menu)
   {
      d3d9_set_vertex_shader_constantf(d3d->dev, 0, (const
               float*)&d3d->mvp_transposed, 4);
      for (i = 0; i < d3d->overlays_size; i++)
         d3d9_overlay_render(d3d, width, height, &d3d->overlays[i], true);
   }
#endif

#ifdef HAVE_GFX_WIDGETS
   if (widgets_active)
      gfx_widgets_frame(video_info);
#endif

   if (msg && *msg)
   {
      d3d9_set_viewports(d3d->dev, &screen_vp);
      d3d9_begin_scene(d3d->dev);
      font_driver_render_msg(d3d, msg, NULL, NULL);
      d3d9_end_scene(d3d->dev);
   }

   win32_update_title();
   d3d9_swap(d3d, d3d->dev);

   return true;
}

const video_poke_interface_t d3d9_poke_interface = {
   d3d9_get_flags,
   d3d9_load_texture,
   d3d9_unload_texture,
   d3d9_set_video_mode,
#if defined(_XBOX) || defined(__WINRT__)
   NULL,
#else
   /* UWP does not expose this information easily */
   win32_get_refresh_rate,
#endif
   NULL,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   d3d9_set_aspect_ratio,
   d3d9_apply_state_changes,
   d3d9_set_menu_texture_frame,
   d3d9_set_menu_texture_enable,
   d3d9_set_osd_msg,

   win32_show_cursor,
   NULL,                         /* grab_mouse_toggle */
   NULL,                         /* get_current_shader */
   NULL,                         /* get_current_software_framebuffer */
   NULL,                         /* get_hw_render_interface */
   NULL,                         /* set_hdr_max_nits */
   NULL,                         /* set_hdr_paper_white_nits */
   NULL,                         /* set_hdr_contrast */
   NULL                          /* set_hdr_expand_gamut */
};

video_driver_t video_d3d9 = {
   d3d9_init,
   d3d9_frame,
   d3d9_set_nonblock_state,
   d3d9_alive,
   NULL,                      /* focus */
   d3d9_suppress_screensaver,
   d3d9_has_windowed,
   d3d9_set_shader,
   d3d9_free,
   "d3d9",
   d3d9_set_viewport,
   d3d9_set_rotation,
   d3d9_viewport_info,
   d3d9_read_viewport,
   NULL,                      /* read_frame_raw */
#ifdef HAVE_OVERLAY
   d3d9_get_overlay_interface,
#endif
#ifdef HAVE_VIDEO_LAYOUT
   NULL,
#endif
   d3d9_get_poke_interface,
   NULL, /* wrap_type_to_enum */
#ifdef HAVE_GFX_WIDGETS
   d3d9_gfx_widgets_enabled
#endif
};
