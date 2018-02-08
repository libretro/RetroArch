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

#include "d3d.h"
#include "../../defines/d3d_defines.h"
#include "../common/d3d_common.h"
#include "../video_coord_array.h"
#include "../../configuration.h"
#include "../../dynamic.h"
#include "../video_driver.h"

#ifdef HAVE_THREADS
#include "../video_thread_wrapper.h"
#endif

#include "../common/win32_common.h"

#ifndef _XBOX
#define HAVE_MONITOR
#define HAVE_WINDOW
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../font_driver.h"

#include "../../core.h"
#include "../../verbosity.h"
#include "../../retroarch.h"

static LPDIRECT3D9 g_pD3D9;

static bool d3d9_init_imports(d3d_video_t *d3d)
{
   retro_ctx_memory_info_t    mem_info;
   state_tracker_t *state_tracker         = NULL;
   struct state_tracker_info tracker_info = {0};

   if (!d3d->shader.variables)
      return true;

   mem_info.id                    = RETRO_MEMORY_SYSTEM_RAM;

   core_get_memory(&mem_info);

   tracker_info.script_class      = NULL;
   tracker_info.wram              = (uint8_t*)mem_info.data;
   tracker_info.info              = d3d->shader.variable;
   tracker_info.info_elem         = d3d->shader.variables;
   tracker_info.script            = NULL;
   tracker_info.script_is_file    = false;

#ifdef HAVE_PYTHON
   if (*d3d->shader.script_path)
   {
      tracker_info.script         = d3d->shader.script_path;
      tracker_info.script_is_file = true;
   }

   if (*d3d->shader.script_class)
      tracker_info.script_class   = d3d->shader.script_class;
#endif

   state_tracker                  = 
      state_tracker_init(&tracker_info);

   if (!state_tracker)
   {
      RARCH_ERR("[D3D]: Failed to initialize state tracker.\n");
      return false;
   }

   d3d->renderchain_driver->add_state_tracker(
         d3d->renderchain_data, state_tracker);

   return true;
}

static bool d3d9_init_chain(d3d_video_t *d3d, const video_info_t *video_info)
{
   struct LinkInfo link_info;
   unsigned current_width, current_height, out_width, out_height;
   unsigned i                   = 0;

   (void)i;
   (void)current_width;
   (void)current_height;
   (void)out_width;
   (void)out_height;

   /* Setup information for first pass. */
   link_info.pass  = NULL;
   link_info.tex_w = video_info->input_scale * RARCH_SCALE_BASE;
   link_info.tex_h = video_info->input_scale * RARCH_SCALE_BASE;
   link_info.pass  = &d3d->shader.pass[0];

   if (!renderchain_d3d_init_first(GFX_CTX_DIRECT3D9_API,
            &d3d->renderchain_driver,
            &d3d->renderchain_data))
   {
	   RARCH_ERR("[D3D]: Renderchain could not be initialized.\n");
	   return false;
   }

   if (!d3d->renderchain_driver || !d3d->renderchain_data)
	   return false;

   if (
         !d3d->renderchain_driver->init(
            d3d,
            &d3d->video_info,
            d3d->dev, &d3d->final_viewport, &link_info,
            d3d->video_info.rgb32)
      )
   {
      RARCH_ERR("[D3D]: Failed to init render chain.\n");
      return false;
   }

   RARCH_LOG("[D3D]: Renderchain driver: %s\n", d3d->renderchain_driver->ident);

#ifndef _XBOX
   current_width  = link_info.tex_w;
   current_height = link_info.tex_h;
   out_width      = 0;
   out_height     = 0;

   for (i = 1; i < d3d->shader.passes; i++)
   {
      d3d->renderchain_driver->convert_geometry(d3d->renderchain_data,
		    &link_info,
            &out_width, &out_height,
            current_width, current_height, &d3d->final_viewport);

      link_info.pass  = &d3d->shader.pass[i];
      link_info.tex_w = next_pow2(out_width);
      link_info.tex_h = next_pow2(out_height);

      current_width = out_width;
      current_height = out_height;

      if (!d3d->renderchain_driver->add_pass(
               d3d->renderchain_data, &link_info))
      {
         RARCH_ERR("[D3D]: Failed to add pass.\n");
         return false;
      }
   }
#endif

   if (d3d->renderchain_driver)
   {
      if (d3d->renderchain_driver->add_lut)
      {
         unsigned i;
         settings_t *settings = config_get_ptr();

         for (i = 0; i < d3d->shader.luts; i++)
         {
            if (!d3d->renderchain_driver->add_lut(
                     d3d->renderchain_data,
                     d3d->shader.lut[i].id, d3d->shader.lut[i].path,
                     d3d->shader.lut[i].filter == RARCH_FILTER_UNSPEC ?
                     settings->bools.video_smooth :
                     (d3d->shader.lut[i].filter == RARCH_FILTER_LINEAR)))
            {
               RARCH_ERR("[D3D]: Failed to init LUTs.\n");
               return false;
            }
         }
      }

      if (d3d->renderchain_driver->add_state_tracker)
      {
         if (!d3d9_init_imports(d3d))
         {
            RARCH_ERR("[D3D]: Failed to init imports.\n");
            return false;
         }
      }
   }

   return true;
}

static bool d3d9_init_singlepass(d3d_video_t *d3d)
{
   struct video_shader_pass *pass = NULL;

   if (!d3d)
      return false;

   memset(&d3d->shader, 0, sizeof(d3d->shader));
   d3d->shader.passes                    = 1;

   pass                                  = (struct video_shader_pass*)
      &d3d->shader.pass[0];

   pass->fbo.valid                       = true;
   pass->fbo.scale_y                     = 1.0;
   pass->fbo.type_y                      = RARCH_SCALE_VIEWPORT;
   pass->fbo.scale_x                     = pass->fbo.scale_y;
   pass->fbo.type_x                      = pass->fbo.type_y;

   if (!string_is_empty(d3d->shader_path))
      strlcpy(pass->source.path, d3d->shader_path,
            sizeof(pass->source.path));

   return true;
}

static bool d3d9_init_multipass(d3d_video_t *d3d, const char *shader_path)
{
   unsigned i;
   bool            use_extra_pass = false;
   struct video_shader_pass *pass = NULL;
   config_file_t            *conf = config_file_new(shader_path);

   if (!conf)
   {
      RARCH_ERR("[D3D]: Failed to load preset.\n");
      return false;
   }

   memset(&d3d->shader, 0, sizeof(d3d->shader));

   if (!video_shader_read_conf_cgp(conf, &d3d->shader))
   {
      config_file_free(conf);
      RARCH_ERR("[D3D]: Failed to parse CGP file.\n");
      return false;
   }

   config_file_free(conf);

   if (!string_is_empty(shader_path))
      video_shader_resolve_relative(&d3d->shader, shader_path);
   RARCH_LOG("[D3D]: Found %u shaders.\n", d3d->shader.passes);

   for (i = 0; i < d3d->shader.passes; i++)
   {
      if (d3d->shader.pass[i].fbo.valid)
         continue;

      d3d->shader.pass[i].fbo.scale_y = 1.0f;
      d3d->shader.pass[i].fbo.scale_x = 1.0f;
      d3d->shader.pass[i].fbo.type_x  = RARCH_SCALE_INPUT;
      d3d->shader.pass[i].fbo.type_y  = RARCH_SCALE_INPUT;
   }

   use_extra_pass       = d3d->shader.passes < GFX_MAX_SHADERS &&
      d3d->shader.pass[d3d->shader.passes - 1].fbo.valid;

   if (use_extra_pass)
   {
      d3d->shader.passes++;
      pass              = (struct video_shader_pass*)
         &d3d->shader.pass[d3d->shader.passes - 1];

      pass->fbo.scale_x = 1.0f;
      pass->fbo.scale_y = 1.0f;
      pass->fbo.type_x  = RARCH_SCALE_VIEWPORT;
      pass->fbo.type_y  = RARCH_SCALE_VIEWPORT;
      pass->filter      = RARCH_FILTER_UNSPEC;
   }
   else
   {
      pass              = (struct video_shader_pass*)
         &d3d->shader.pass[d3d->shader.passes - 1];

      pass->fbo.scale_x = 1.0f;
      pass->fbo.scale_y = 1.0f;
      pass->fbo.type_x  = RARCH_SCALE_VIEWPORT;
      pass->fbo.type_y  = RARCH_SCALE_VIEWPORT;
   }

   return true;
}

static bool d3d9_process_shader(d3d_video_t *d3d)
{
   const char *shader_path = d3d->shader_path;
   if (d3d && !string_is_empty(shader_path) &&
         string_is_equal(path_get_extension(shader_path), "cgp"))
      return d3d9_init_multipass(d3d, shader_path);

   return d3d9_init_singlepass(d3d);
}

static void d3d9_viewport_info(void *data, struct video_viewport *vp)
{
   d3d_video_t *d3d   = (d3d_video_t*)data;

   if (  d3d &&
         d3d->renderchain_driver &&
         d3d->renderchain_driver->viewport_info)
      d3d->renderchain_driver->viewport_info(d3d, vp);
}

static void d3d9_set_mvp(void *data,
      void *shader_data,
      const void *mat_data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

   if (  d3d &&
         d3d->renderchain_driver &&
         d3d->renderchain_driver->set_mvp)
      d3d->renderchain_driver->set_mvp(d3d, d3d->renderchain_data, shader_data, mat_data);
}

static void d3d9_overlay_render(d3d_video_t *d3d,
      video_frame_info_t *video_info,
      overlay_t *overlay)
{
   LPDIRECT3DVERTEXDECLARATION9 vertex_decl;
   struct video_viewport vp;
   void *verts;
   unsigned i;
   Vertex vert[4];
   D3DVERTEXELEMENT9 vElems[4] = {
      {0, offsetof(Vertex, x),  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,
         D3DDECLUSAGE_POSITION, 0},
      {0, offsetof(Vertex, u), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,
         D3DDECLUSAGE_TEXCOORD, 0},
      {0, offsetof(Vertex, color), D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT,
         D3DDECLUSAGE_COLOR, 0},
      D3DDECL_END()
   };
   unsigned width      = video_info->width;
   unsigned height     = video_info->height;

   if (!d3d || !overlay || !overlay->tex)
      return;

   if (!overlay->vert_buf)
   {
      overlay->vert_buf = d3d_vertex_buffer_new(
      d3d->dev, sizeof(vert), D3DUSAGE_WRITEONLY,
#ifdef _XBOX
	  0,
#else
      D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1,
#endif
      D3DPOOL_MANAGED, NULL);

	  if (!overlay->vert_buf)
		  return;
   }

   for (i = 0; i < 4; i++)
   {
      vert[i].z    = 0.5f;
      vert[i].color   = (((uint32_t)(overlay->alpha_mod * 0xFF)) << 24) | 0xFFFFFF;
   }

   d3d9_viewport_info(d3d, &vp);

   vert[0].x      = overlay->vert_coords[0];
   vert[1].x      = overlay->vert_coords[0] + overlay->vert_coords[2];
   vert[2].x      = overlay->vert_coords[0];
   vert[3].x      = overlay->vert_coords[0] + overlay->vert_coords[2];
   vert[0].y      = overlay->vert_coords[1];
   vert[1].y      = overlay->vert_coords[1];
   vert[2].y      = overlay->vert_coords[1] + overlay->vert_coords[3];
   vert[3].y      = overlay->vert_coords[1] + overlay->vert_coords[3];

   vert[0].u      = overlay->tex_coords[0];
   vert[1].u      = overlay->tex_coords[0] + overlay->tex_coords[2];
   vert[2].u      = overlay->tex_coords[0];
   vert[3].u      = overlay->tex_coords[0] + overlay->tex_coords[2];
   vert[0].v      = overlay->tex_coords[1];
   vert[1].v      = overlay->tex_coords[1];
   vert[2].v      = overlay->tex_coords[1] + overlay->tex_coords[3];
   vert[3].v      = overlay->tex_coords[1] + overlay->tex_coords[3];

   verts = d3d_vertex_buffer_lock(overlay->vert_buf);
   memcpy(verts, vert, sizeof(vert));
   d3d_vertex_buffer_unlock(overlay->vert_buf);

   d3d_enable_blend_func(d3d->dev);

   /* set vertex declaration for overlay. */
   d3d_vertex_declaration_new(d3d->dev, &vElems, (void**)&vertex_decl);
   d3d_set_vertex_declaration(d3d->dev, vertex_decl);
   d3d_vertex_declaration_free(vertex_decl);

   d3d_set_stream_source(d3d->dev, 0, overlay->vert_buf,
         0, sizeof(*vert));

   if (overlay->fullscreen)
   {
      D3DVIEWPORT9 vp_full;

      vp_full.X      = 0;
      vp_full.Y      = 0;
      vp_full.Width  = width;
      vp_full.Height = height;
      vp_full.MinZ   = 0.0f;
      vp_full.MaxZ   = 1.0f;
      d3d_set_viewports(d3d->dev, &vp_full);
   }

   /* Render overlay. */
   d3d_set_texture(d3d->dev, 0, overlay->tex);
   d3d_set_sampler_address_u(d3d->dev, 0, D3DTADDRESS_BORDER);
   d3d_set_sampler_address_v(d3d->dev, 0, D3DTADDRESS_BORDER);
   d3d_set_sampler_minfilter(d3d->dev, 0, D3DTEXF_LINEAR);
   d3d_set_sampler_magfilter(d3d->dev, 0, D3DTEXF_LINEAR);
   d3d_draw_primitive(d3d->dev, D3DPT_TRIANGLESTRIP, 0, 2);

   /* Restore previous state. */
   d3d_disable_blend_func(d3d->dev);
   d3d_set_viewports(d3d->dev, &d3d->final_viewport);
}

static void d3d9_free_overlay(d3d_video_t *d3d, overlay_t *overlay)
{
   if (!d3d)
      return;

   d3d_texture_free(overlay->tex);
   d3d_vertex_buffer_free(overlay->vert_buf, NULL);
}

static void d3d_deinit_chain(d3d_video_t *d3d)
{
   if (!d3d || !d3d->renderchain_driver)
      return;

   if (d3d->renderchain_driver->chain_free)
      d3d->renderchain_driver->chain_free(d3d->renderchain_data);

   d3d->renderchain_driver = NULL;
   d3d->renderchain_data   = NULL;
}

static void d3d_deinitialize(d3d_video_t *d3d)
{
   if (!d3d)
      return;

   font_driver_free_osd();

   d3d_deinit_chain(d3d);
   d3d_vertex_buffer_free(d3d->menu_display.buffer, d3d->menu_display.decl);
   d3d->menu_display.buffer = NULL;
   d3d->menu_display.decl = NULL;
}

#define FS_PRESENTINTERVAL(pp) ((pp)->PresentationInterval)

static D3DFORMAT d3d_get_color_format_backbuffer(bool rgb32, bool windowed)
{
   D3DFORMAT fmt = D3DFMT_X8R8G8B8;
#ifdef _XBOX
   if (!rgb32)
      fmt        = d3d_get_rgb565_format();
#else
   if (windowed)
   {
      D3DDISPLAYMODE display_mode;
      if (d3d_get_adapter_display_mode(g_pD3D9, 0, &display_mode))
         fmt = display_mode.Format;
   }
#endif
   return fmt;
}

#ifdef _XBOX
static D3DFORMAT d3d_get_color_format_front_buffer(void)
{
   return D3DFMT_LE_X8R8G8B8;
}
#endif

static bool d3d_is_windowed_enable(bool info_fullscreen)
{
#ifndef _XBOX
   settings_t *settings = config_get_ptr();
   if (!info_fullscreen)
      return true;
   if (settings)
      return settings->bools.video_windowed_fullscreen;
#endif
   return false;
}

void d3d_make_d3dpp(void *data,
      const video_info_t *info, void *_d3dpp)
{
   d3d_video_t *d3d               = (d3d_video_t*)data;
   D3DPRESENT_PARAMETERS *d3dpp   = (D3DPRESENT_PARAMETERS*)_d3dpp;
#ifdef _XBOX
   /* TODO/FIXME - get rid of global state dependencies. */
   global_t *global               = global_get_ptr();
   bool gamma_enable              = global ? 
      global->console.screen.gamma_correction : false;
#endif
   bool windowed_enable           = d3d_is_windowed_enable(info->fullscreen);

   memset(d3dpp, 0, sizeof(*d3dpp));

   d3dpp->Windowed                = windowed_enable;
   FS_PRESENTINTERVAL(d3dpp)      = D3DPRESENT_INTERVAL_IMMEDIATE;

   if (info->vsync)
   {
      settings_t *settings        = config_get_ptr();

      switch (settings->uints.video_swap_interval)
      {
         default:
         case 1:
            FS_PRESENTINTERVAL(d3dpp) = D3DPRESENT_INTERVAL_ONE;
            break;
         case 2:
            FS_PRESENTINTERVAL(d3dpp) = D3DPRESENT_INTERVAL_TWO;
            break;
         case 3:
            FS_PRESENTINTERVAL(d3dpp) = D3DPRESENT_INTERVAL_THREE;
            break;
         case 4:
            FS_PRESENTINTERVAL(d3dpp) = D3DPRESENT_INTERVAL_FOUR;
            break;
      }
   }

   d3dpp->SwapEffect              = D3DSWAPEFFECT_DISCARD;
   d3dpp->BackBufferCount         = 2;
   d3dpp->BackBufferFormat        = d3d_get_color_format_backbuffer(
         info->rgb32, windowed_enable);

#ifdef _XBOX
   d3dpp->FrontBufferFormat       = d3d_get_color_format_front_buffer();

   if (gamma_enable)
   {
      d3dpp->BackBufferFormat     = (D3DFORMAT)MAKESRGBFMT(
            d3dpp->BackBufferFormat);
      d3dpp->FrontBufferFormat    = (D3DFORMAT)MAKESRGBFMT(
            d3dpp->FrontBufferFormat);
   }
#else
   d3dpp->hDeviceWindow           = win32_get_window();
#endif

   if (!windowed_enable)
   {
#ifdef _XBOX
      gfx_ctx_mode_t mode;
      unsigned width              = 0;
      unsigned height             = 0;

      video_context_driver_get_video_size(&mode);

      width                       = mode.width;
      height                      = mode.height;
      mode.width                  = 0;
      mode.height                 = 0;
      video_driver_set_size(&width, &height);
#endif
      video_driver_get_size(&d3dpp->BackBufferWidth,
            &d3dpp->BackBufferHeight);
   }

#ifdef _XBOX
   d3dpp->MultiSampleType         = D3DMULTISAMPLE_NONE;
   d3dpp->EnableAutoDepthStencil  = FALSE;
#if 0
   if (!widescreen_mode)
      d3dpp->Flags |= D3DPRESENTFLAG_NO_LETTERBOX;
#endif
   d3dpp->MultiSampleQuality      = 0;
#endif
}

static bool d3d9_init_base(void *data, const video_info_t *info)
{
   D3DPRESENT_PARAMETERS d3dpp;
   HWND focus_window = NULL;
   d3d_video_t *d3d  = (d3d_video_t*)data;

#ifndef _XBOX
   focus_window      = win32_get_window();
#endif

   memset(&d3dpp, 0, sizeof(d3dpp));

   g_pD3D9            = (LPDIRECT3D9)d3d_create();

   /* this needs g_pD3D9 created first */
   d3d_make_d3dpp(d3d, info, &d3dpp);

   if (!g_pD3D9)
   {
      RARCH_ERR("[D3D]: Failed to create D3D interface.\n");
      return false;
   }

   if (!d3d_create_device(&d3d->dev, &d3dpp,
            g_pD3D9,
            focus_window,
            d3d->cur_mon_id)
      )
   {
      RARCH_ERR("[D3D]: Failed to initialize device.\n");
      return false;
   }

   return true;
}

static void d3d_calculate_rect(void *data,
      unsigned *width, unsigned *height,
      int *x, int *y,
      bool force_full,
      bool allow_rotate)
{
   gfx_ctx_aspect_t aspect_data;
   float device_aspect  = (float)*width / *height;
   d3d_video_t *d3d     = (d3d_video_t*)data;
   settings_t *settings = config_get_ptr();

   video_driver_get_size(width, height);

   aspect_data.aspect   = &device_aspect;
   aspect_data.width    = *width;
   aspect_data.height   = *height;

   video_context_driver_translate_aspect(&aspect_data);

   *x = 0;
   *y = 0;

   if (settings->bools.video_scale_integer && !force_full)
   {
      struct video_viewport vp;

      vp.x                        = 0;
      vp.y                        = 0;
      vp.width                    = 0;
      vp.height                   = 0;
      vp.full_width               = 0;
      vp.full_height              = 0;

      video_viewport_get_scaled_integer(&vp,
            *width,
            *height,
            video_driver_get_aspect_ratio(),
            d3d->keep_aspect);

      *x                          = vp.x;
      *y                          = vp.y;
      *width                      = vp.width;
      *height                     = vp.height;
   }
   else if (d3d->keep_aspect && !force_full)
   {
      float desired_aspect = video_driver_get_aspect_ratio();

#if defined(HAVE_MENU)
      if (settings->uints.video_aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         video_viewport_t *custom = video_viewport_get_custom();

         *x          = custom->x;
         *y          = custom->y;
         *width      = custom->width;
         *height     = custom->height;
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
            delta        = (desired_aspect / device_aspect - 1.0f) / 2.0f + 0.5f;
            *x           = (int)(roundf(*width * (0.5f - delta)));
            *width       = (unsigned)(roundf(2.0f * (*width) * delta));
         }
         else
         {
            delta        = (device_aspect / desired_aspect - 1.0f) / 2.0f + 0.5f;
            *y           = (int)(roundf(*height * (0.5f - delta)));
            *height      = (unsigned)(roundf(2.0f * (*height) * delta));
         }
      }
   }
}

static void d3d9_set_viewport(void *data,
      unsigned width, unsigned height,
      bool force_full,
      bool allow_rotate)
{
   int x               = 0;
   int y               = 0;
   d3d_video_t *d3d = (d3d_video_t*)data;

   d3d_calculate_rect(data, &width, &height, &x, &y,
         force_full, allow_rotate);

   /* D3D doesn't support negative X/Y viewports ... */
   if (x < 0)
      x = 0;
   if (y < 0)
      y = 0;

   d3d->final_viewport.x      = x;
   d3d->final_viewport.y      = y;
   d3d->final_viewport.width  = width;
   d3d->final_viewport.height = height;
   d3d->final_viewport.min_z  = 0.0f;
   d3d->final_viewport.max_z  = 1.0f;

   if (d3d->renderchain_driver && d3d->renderchain_driver->set_font_rect)
      d3d->renderchain_driver->set_font_rect(d3d, NULL);
}

static bool d3d9_initialize(d3d_video_t *d3d, const video_info_t *info)
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

      d3d_make_d3dpp(d3d, info, &d3dpp);

      /* the D3DX font driver uses POOL_DEFAULT resources
       * and will prevent a clean reset here
       * another approach would be to keep track of all created D3D
       * font objects and free/realloc them around the d3d_reset call  */

      menu_driver_ctl(RARCH_MENU_CTL_DEINIT, NULL);
      if (!d3d_reset(d3d->dev, &d3dpp))
      {
         d3d_deinitialize(d3d);
         d3d_device_free(NULL, g_pD3D9);
         g_pD3D9 = NULL;

         ret = d3d9_init_base(d3d, info);
         if (ret)
            RARCH_LOG("[D3D]: Recovered from dead state.\n");
      }
      menu_driver_init(info->is_threaded);
   }

   if (!ret)
      return ret;

   if (!d3d9_init_chain(d3d, info))
   {
      RARCH_ERR("[D3D]: Failed to initialize render chain.\n");
      return false;
   }

   video_driver_get_size(&width, &height);
   d3d9_set_viewport(d3d,
	   width, height, false, true);

#ifdef _XBOX
   strlcpy(settings->paths.path_font, "game:\\media\\Arial_12.xpr",
         sizeof(settings->paths.path_font));
#endif
   font_driver_init_osd(d3d, false,
         info->is_threaded,
         FONT_DRIVER_RENDER_DIRECT3D_API);

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
      if (!d3d_vertex_declaration_new(d3d->dev,
               (void*)VertexElements, (void**)&d3d->menu_display.decl))
         return false;
   }

   d3d->menu_display.offset = 0;
   d3d->menu_display.size   = 1024;
   d3d->menu_display.buffer = d3d_vertex_buffer_new(
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

   d3d_set_render_state(d3d->dev, D3DRS_CULLMODE, D3DCULL_NONE);

   return true;
}

static bool d3d9_restore(void *data)
{
   d3d_video_t            *d3d = (d3d_video_t*)data;

   if (!d3d)
      return false;

   d3d_deinitialize(d3d);

   if (!d3d9_initialize(d3d, &d3d->video_info))
   {
      RARCH_ERR("[D3D]: Restore error.\n");
      return false;
   }

   d3d->needs_restore = false;

   return true;
}


static void d3d9_set_nonblock_state(void *data, bool state)
{
   unsigned interval            = state ? 0 : 1;
   d3d_video_t            *d3d = (d3d_video_t*)data;

   if (!d3d)
      return;

   d3d->video_info.vsync = !state;

   video_context_driver_swap_interval(&interval);
#ifndef _XBOX
   d3d->needs_restore = true;
   d3d9_restore(d3d);
#endif
}

static bool d3d9_alive(void *data)
{
   gfx_ctx_size_t size_data;
   unsigned temp_width  = 0;
   unsigned temp_height = 0;
   bool ret             = false;
   d3d_video_t *d3d     = (d3d_video_t*)data;
   bool        quit     = false;
   bool        resize   = false;

   /* Needed because some context drivers don't track their sizes */
   video_driver_get_size(&temp_width, &temp_height);

   size_data.quit       = &quit;
   size_data.resize     = &resize;
   size_data.width      = &temp_width;
   size_data.height     = &temp_height;

   if (video_context_driver_check_window(&size_data))
   {
      if (quit)
         d3d->quitting = quit;

      if (resize)
      {
         d3d->should_resize = true;
         video_driver_set_resize(temp_width, temp_height);
         d3d9_restore(d3d);
      }

      ret = !quit;
   }

   if (temp_width != 0 && temp_height != 0)
      video_driver_set_size(&temp_width, &temp_height);

   return ret;
}

static bool d3d9_suppress_screensaver(void *data, bool enable)
{
   bool enabled = enable;
   return video_context_driver_suppress_screensaver(&enabled);
}

static void d3d9_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

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

   if (!d3d)
      return;

   d3d->keep_aspect   = true;
   d3d->should_resize = true;
}

static void d3d9_apply_state_changes(void *data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   if (d3d)
      d3d->should_resize = true;
}

static void d3d9_set_osd_msg(void *data,
      video_frame_info_t *video_info,
      const char *msg,
      const void *params, void *font)
{
   d3d_video_t          *d3d = (d3d_video_t*)data;

   if (d3d->renderchain_driver->set_font_rect && params)
      d3d->renderchain_driver->set_font_rect(d3d, params);

   font_driver_render_msg(video_info, font, msg, params);
}

static bool d3d9_init_internal(d3d_video_t *d3d,
      const video_info_t *info, const input_driver_t **input,
      void **input_data)
{
   gfx_ctx_input_t inp;
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

   memset(&d3d->windowClass, 0, sizeof(d3d->windowClass));

#ifdef HAVE_WINDOW
   d3d->windowClass.lpfnWndProc = WndProcD3D;
   win32_window_init(&d3d->windowClass, true, NULL);
#endif

#ifdef HAVE_MONITOR
   win32_monitor_info(&current_mon, &hm_to_use, &d3d->cur_mon_id);

   mon_rect        = current_mon.rcMonitor;
   g_resize_width  = info->width;
   g_resize_height = info->height;

   windowed_full   = settings->bools.video_windowed_fullscreen;

   full_x          = (windowed_full || info->width  == 0) ?
      (mon_rect.right  - mon_rect.left) : info->width;
   full_y          = (windowed_full || info->height == 0) ?
      (mon_rect.bottom - mon_rect.top)  : info->height;

   RARCH_LOG("[D3D]: Monitor size: %dx%d.\n",
         (int)(mon_rect.right  - mon_rect.left),
         (int)(mon_rect.bottom - mon_rect.top));
#else
   {
      gfx_ctx_mode_t mode;

      video_context_driver_get_video_size(&mode);

      full_x   = mode.width;
      full_y   = mode.height;
   }
#endif
   {
      unsigned new_width  = info->fullscreen ? full_x : info->width;
      unsigned new_height = info->fullscreen ? full_y : info->height;
      video_driver_set_size(&new_width, &new_height);
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

   /* This should only be done once here
    * to avoid set_shader() to be overridden
    * later. */
   if (settings->bools.video_shader_enable)
   {
      enum rarch_shader_type type =
         video_shader_parse_type(retroarch_get_shader_preset(),
               RARCH_SHADER_NONE);

      switch (type)
      {
         case RARCH_SHADER_CG:
            if (!string_is_empty(d3d->shader_path))
               free(d3d->shader_path);
            if (!string_is_empty(retroarch_get_shader_preset()))
               d3d->shader_path = strdup(retroarch_get_shader_preset());
            break;
         default:
            break;
      }
   }

   if (!d3d9_process_shader(d3d))
      return false;

   d3d->video_info = *info;
   if (!d3d9_initialize(d3d, &d3d->video_info))
      return false;

   inp.input      = input;
   inp.input_data = input_data;

   video_context_driver_input_driver(&inp);

   RARCH_LOG("[D3D]: Init complete.\n");
   return true;
}

static void d3d9_set_rotation(void *data, unsigned rot)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   struct video_ortho ortho = {0, 1, 0, 1, -1, 1};

   if (!d3d)
      return;

   d3d->dev_rotation = rot;
}

static void d3d9_show_mouse(void *data, bool state)
{
   video_context_driver_show_mouse(&state);
}

static const gfx_ctx_driver_t *d3d9_get_context(void *data)
{
   /* TODO: GL core contexts through ANGLE? */
   unsigned minor       = 0;
   unsigned major       = 9;
   enum gfx_ctx_api api = GFX_CTX_DIRECT3D9_API;
   settings_t *settings = config_get_ptr();

   return video_context_driver_init_first(data,
         settings->arrays.video_context_driver,
         api, major, minor, false);
}

static void *d3d9_init(const video_info_t *info,
      const input_driver_t **input, void **input_data)
{
   d3d_video_t            *d3d        = NULL;
   const gfx_ctx_driver_t *ctx_driver = NULL;

   if (!d3d_initialize_symbols(GFX_CTX_DIRECT3D9_API))
      return NULL;

   d3d = (d3d_video_t*)calloc(1, sizeof(*d3d));
   if (!d3d)
      goto error;

   ctx_driver = d3d9_get_context(d3d);
   if (!ctx_driver)
      goto error;

   /* Default values */
   d3d->dev                  = NULL;
   d3d->dev_rotation         = 0;
   d3d->needs_restore        = false;
#ifdef HAVE_OVERLAY
   d3d->overlays_enabled     = false;
#endif
   d3d->should_resize        = false;
   d3d->menu                 = NULL;

   video_context_driver_set((const gfx_ctx_driver_t*)ctx_driver);

   if (!d3d9_init_internal(d3d, info, input, input_data))
   {
      RARCH_ERR("[D3D]: Failed to init D3D.\n");
      goto error;
   }

   d3d->keep_aspect       = info->force_aspect;

   return d3d;

error:
   video_context_driver_destroy();
   if (d3d)
      free(d3d);
   return NULL;
}

#ifdef HAVE_OVERLAY
static void d3d9_free_overlays(d3d_video_t *d3d)
{
   unsigned i;

   if (!d3d)
      return;

   for (i = 0; i < d3d->overlays_size; i++)
      d3d9_free_overlay(d3d, &d3d->overlays[i]);
   free(d3d->overlays);
   d3d->overlays      = NULL;
   d3d->overlays_size = 0;
}
#endif

static void d3d9_free(void *data)
{
   d3d_video_t   *d3d = (d3d_video_t*)data;

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

   d3d_deinitialize(d3d);

   video_context_driver_free();

   if (!string_is_empty(d3d->shader_path))
      free(d3d->shader_path);

   d3d->shader_path = NULL;
   d3d_device_free(d3d->dev, g_pD3D9);
   d3d->dev         = NULL;
   g_pD3D9          = NULL;

#ifndef _XBOX
   win32_monitor_from_window();
#endif

   if (d3d)
      free(d3d);

   d3d_deinitialize_symbols();

#ifndef _XBOX
   win32_destroy_window();
#endif
}

#ifdef HAVE_OVERLAY
static void d3d9_overlay_tex_geom(
      void *data,
      unsigned index,
      float x, float y,
      float w, float h)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   if (!d3d)
      return;

   d3d->overlays[index].tex_coords[0] = x;
   d3d->overlays[index].tex_coords[1] = y;
   d3d->overlays[index].tex_coords[2] = w;
   d3d->overlays[index].tex_coords[3] = h;
}

static void d3d9_overlay_vertex_geom(
      void *data,
      unsigned index,
      float x, float y,
      float w, float h)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   if (!d3d)
      return;

   y                                   = 1.0f - y;
   h                                   = -h;
   d3d->overlays[index].vert_coords[0] = x;
   d3d->overlays[index].vert_coords[1] = y;
   d3d->overlays[index].vert_coords[2] = w;
   d3d->overlays[index].vert_coords[3] = h;
}

static bool d3d9_overlay_load(void *data,
      const void *image_data, unsigned num_images)
{
   unsigned i, y;
   overlay_t *new_overlays            = NULL;
   d3d_video_t *d3d                   = (d3d_video_t*)data;
   const struct texture_image *images = (const struct texture_image*)
      image_data;

   if (!d3d)
	   return false;

   d3d9_free_overlays(d3d);
   d3d->overlays      = (overlay_t*)calloc(num_images, sizeof(*d3d->overlays));
   d3d->overlays_size = num_images;

   for (i = 0; i < num_images; i++)
   {
      D3DLOCKED_RECT d3dlr;
      unsigned width     = images[i].width;
      unsigned height    = images[i].height;
      overlay_t *overlay = (overlay_t*)&d3d->overlays[i];

      overlay->tex       = d3d_texture_new(d3d->dev, NULL,
                  width, height, 1,
                  0,
                  d3d_get_argb8888_format(),
                  D3DPOOL_MANAGED, 0, 0, 0,
                  NULL, NULL, false);

      if (!overlay->tex)
      {
         RARCH_ERR("[D3D]: Failed to create overlay texture\n");
         return false;
      }

      if (d3d_lock_rectangle(overlay->tex, 0, &d3dlr,
               NULL, 0, D3DLOCK_NOSYSLOCK))
      {
         uint32_t       *dst = (uint32_t*)(d3dlr.pBits);
         const uint32_t *src = images[i].pixels;
         unsigned      pitch = d3dlr.Pitch >> 2;

         for (y = 0; y < height; y++, dst += pitch, src += width)
            memcpy(dst, src, width << 2);
         d3d_unlock_rectangle(overlay->tex);
      }

      overlay->tex_w         = width;
      overlay->tex_h         = height;

      /* Default. Stretch to whole screen. */
      d3d9_overlay_tex_geom(d3d, i, 0, 0, 1, 1);
      d3d9_overlay_vertex_geom(d3d, i, 0, 0, 1, 1);
   }

   return true;
}

static void d3d9_overlay_enable(void *data, bool state)
{
   unsigned i;
   d3d_video_t            *d3d = (d3d_video_t*)data;

   if (!d3d)
      return;

   for (i = 0; i < d3d->overlays_size; i++)
      d3d->overlays_enabled = state;

   video_context_driver_show_mouse(&state);
}

static void d3d9_overlay_full_screen(void *data, bool enable)
{
   unsigned i;
   d3d_video_t *d3d = (d3d_video_t*)data;

   for (i = 0; i < d3d->overlays_size; i++)
      d3d->overlays[i].fullscreen = enable;
}

static void d3d9_overlay_set_alpha(void *data, unsigned index, float mod)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   if (d3d)
      d3d->overlays[index].alpha_mod = mod;
}

static const video_overlay_interface_t d3d9_overlay_interface = {
   d3d9_overlay_enable,
   d3d9_overlay_load,
   d3d9_overlay_tex_geom,
   d3d9_overlay_vertex_geom,
   d3d9_overlay_full_screen,
   d3d9_overlay_set_alpha,
};

static void d3d9_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface)
{
   (void)data;
   *iface = &d3d9_overlay_interface;
}
#endif

static bool d3d9_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height,
      uint64_t frame_count, unsigned pitch,
      const char *msg, video_frame_info_t *video_info)
{
   D3DVIEWPORT9 screen_vp;
   unsigned i                          = 0;
   d3d_video_t *d3d                    = (d3d_video_t*)data;
   unsigned width                      = video_info->width;
   unsigned height                     = video_info->height;
   (void)i;

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
         RARCH_ERR("[D3D]: Failed to restore.\n");
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
   d3d_set_viewports(d3d->dev, &screen_vp);
   d3d_clear(d3d->dev, 0, 0, D3DCLEAR_TARGET, 0, 1, 0);

   /* Insert black frame first, so we
    * can screenshot, etc. */
   if (video_info->black_frame_insertion)
   {
      if (!d3d_swap(d3d, d3d->dev) || d3d->needs_restore)
         return true;
      d3d_clear(d3d->dev, 0, 0, D3DCLEAR_TARGET, 0, 1, 0);
   }

   if (!d3d->renderchain_driver->render(
            d3d,
            frame, frame_width, frame_height,
            pitch, d3d->dev_rotation))
   {
      RARCH_ERR("[D3D]: Failed to render scene.\n");
      return false;
   }


#ifdef HAVE_MENU
   if (d3d->menu && d3d->menu->enabled)
   {
      d3d9_set_mvp(d3d, NULL, &d3d->mvp);
      d3d9_overlay_render(d3d, video_info, d3d->menu);

      d3d->menu_display.offset = 0;
      d3d_set_vertex_declaration(d3d->dev, d3d->menu_display.decl);
      d3d_set_stream_source(d3d->dev, 0, d3d->menu_display.buffer, 0, sizeof(Vertex));

      d3d_set_viewports(d3d->dev, &screen_vp);
      menu_driver_frame(video_info);
   }
#endif

#ifdef HAVE_OVERLAY
   if (d3d->overlays_enabled)
   {
      d3d9_set_mvp(d3d, NULL, &d3d->mvp);
      for (i = 0; i < d3d->overlays_size; i++)
         d3d9_overlay_render(d3d, video_info, &d3d->overlays[i]);
   }
#endif

   if (msg && *msg)
   {
      d3d_set_viewports(d3d->dev, &screen_vp);
      font_driver_render_msg(video_info, NULL, msg, NULL);
   }

   video_info->cb_update_window_title(
         video_info->context_data, video_info);

   video_info->cb_swap_buffers(
         video_info->context_data, video_info);

   return true;
}

static bool d3d9_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   d3d_video_t *d3d   = (d3d_video_t*)data;

   if (  !d3d ||
         !d3d->renderchain_driver ||
         !d3d->renderchain_driver->read_viewport)
      return false;

   return d3d->renderchain_driver->read_viewport(d3d, buffer, false);
}

static bool d3d9_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   d3d_video_t *d3d       = (d3d_video_t*)data;
   char *old_shader       = (d3d && !string_is_empty(d3d->shader_path)) ? strdup(d3d->shader_path) : NULL;

   if (!string_is_empty(d3d->shader_path))
      free(d3d->shader_path);
   d3d->shader_path = NULL;

   switch (type)
   {
      case RARCH_SHADER_CG:
      case RARCH_SHADER_HLSL:
         if (!string_is_empty(path))
            d3d->shader_path = strdup(path);
         break;
      default:
         break;
   }

   if (!d3d9_process_shader(d3d) || !d3d9_restore(d3d))
   {
      RARCH_ERR("[D3D]: Setting shader failed.\n");
      if (!string_is_empty(old_shader))
      {
         d3d->shader_path = strdup(old_shader);
         d3d9_process_shader(d3d);
         d3d9_restore(d3d);
      }
      free(old_shader);
      return false;
   }

   return true;
}

static void d3d9_set_menu_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   D3DLOCKED_RECT d3dlr;
   d3d_video_t *d3d = (d3d_video_t*)data;

   (void)d3dlr;
   (void)frame;
   (void)rgb32;
   (void)width;
   (void)height;
   (void)alpha;

   if (    !d3d->menu->tex            || 
            d3d->menu->tex_w != width ||
            d3d->menu->tex_h != height)
   {
      if (d3d->menu)
	     d3d_texture_free(d3d->menu->tex);

      d3d->menu->tex = d3d_texture_new(d3d->dev, NULL,
            width, height, 1,
            0, d3d_get_argb8888_format(),
            D3DPOOL_MANAGED, 0, 0, 0, NULL, NULL, false);

      if (!d3d->menu->tex)
      {
         RARCH_ERR("[D3D]: Failed to create menu texture.\n");
         return;
      }

      d3d->menu->tex_w          = width;
      d3d->menu->tex_h          = height;
   }

   d3d->menu->alpha_mod = alpha;

   if (d3d_lock_rectangle(d3d->menu->tex, 0, &d3dlr,
            NULL, 0, D3DLOCK_NOSYSLOCK))
   {
      unsigned h, w;
      if (rgb32)
      {
         uint8_t        *dst = (uint8_t*)d3dlr.pBits;
         const uint32_t *src = (const uint32_t*)frame;

         for (h = 0; h < height; h++, dst += d3dlr.Pitch, src += width)
         {
            memcpy(dst, src, width * sizeof(uint32_t));
            memset(dst + width * sizeof(uint32_t), 0,
                  d3dlr.Pitch - width * sizeof(uint32_t));
         }
      }
      else
      {
         uint32_t       *dst = (uint32_t*)d3dlr.pBits;
         const uint16_t *src = (const uint16_t*)frame;

         for (h = 0; h < height; h++, dst += d3dlr.Pitch >> 2, src += width)
         {
            for (w = 0; w < width; w++)
            {
               uint16_t c = src[w];
               uint32_t r = (c >> 12) & 0xf;
               uint32_t g = (c >>  8) & 0xf;
               uint32_t b = (c >>  4) & 0xf;
               uint32_t a = (c >>  0) & 0xf;
               r          = ((r << 4) | r) << 16;
               g          = ((g << 4) | g) <<  8;
               b          = ((b << 4) | b) <<  0;
               a          = ((a << 4) | a) << 24;
               dst[w]     = r | g | b | a;
            }
         }
      }


      if (d3d->menu)
         d3d_unlock_rectangle(d3d->menu->tex);
   }
}

static void d3d9_set_menu_texture_enable(void *data,
      bool state, bool full_screen)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

   if (!d3d || !d3d->menu)
      return;

   d3d->menu->enabled            = state;
   d3d->menu->fullscreen         = full_screen;
}

static void d3d9_video_texture_load_d3d(d3d_video_t *d3d,
      struct texture_image *ti,
      enum texture_filter_type filter_type,
      uintptr_t *id)
{
   D3DLOCKED_RECT d3dlr;
   LPDIRECT3DTEXTURE9 tex = NULL;
   unsigned usage         = 0;
   bool want_mipmap       = false;

   if((filter_type == TEXTURE_FILTER_MIPMAP_LINEAR) ||
      (filter_type == TEXTURE_FILTER_MIPMAP_NEAREST))
      want_mipmap        = true;

   tex = (LPDIRECT3DTEXTURE9)d3d_texture_new(d3d->dev, NULL,
               ti->width, ti->height, 0,
               usage, d3d_get_argb8888_format(),
               D3DPOOL_MANAGED, 0, 0, 0,
               NULL, NULL, want_mipmap);

   if (!tex)
   {
      RARCH_ERR("[D3D]: Failed to create texture\n");
      return;
   }

   if (d3d_lock_rectangle(tex, 0, &d3dlr,
            NULL, 0, D3DLOCK_NOSYSLOCK))
   {
      unsigned i;
      uint32_t       *dst = (uint32_t*)(d3dlr.pBits);
      const uint32_t *src = ti->pixels;
      unsigned      pitch = d3dlr.Pitch >> 2;

      for (i = 0; i < ti->height; i++, dst += pitch, src += ti->width)
         memcpy(dst, src, ti->width << 2);
      d3d_unlock_rectangle(tex);
   }

   *id = (uintptr_t)tex;
}

static int d3d9_video_texture_load_wrap_d3d_mipmap(void *data)
{
   uintptr_t id = 0;
   d3d9_video_texture_load_d3d((d3d_video_t*)video_driver_get_ptr(true),
         (struct texture_image*)data, TEXTURE_FILTER_MIPMAP_LINEAR, &id);
   return id;
}

static int d3d9_video_texture_load_wrap_d3d(void *data)
{
   uintptr_t id = 0;
   d3d9_video_texture_load_d3d((d3d_video_t*)video_driver_get_ptr(true),
         (struct texture_image*)data, TEXTURE_FILTER_LINEAR, &id);
   return id;
}

static uintptr_t d3d9_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   uintptr_t id = 0;

   if (threaded)
   {
      custom_command_method_t func = d3d9_video_texture_load_wrap_d3d;

      switch (filter_type)
      {
         case TEXTURE_FILTER_MIPMAP_LINEAR:
         case TEXTURE_FILTER_MIPMAP_NEAREST:
            func = d3d9_video_texture_load_wrap_d3d_mipmap;
            break;
         default:
            func = d3d9_video_texture_load_wrap_d3d;
            break;
      }

      return video_thread_texture_load(data, func);
   }

   d3d9_video_texture_load_d3d((d3d_video_t*)video_driver_get_ptr(false),
         (struct texture_image*)data, filter_type, &id);
   return id;
}

static void d3d9_unload_texture(void *data, uintptr_t id)
{
   LPDIRECT3DTEXTURE9 texid;
   if (!id)
	   return;

   texid = (LPDIRECT3DTEXTURE9)id;
   d3d_texture_free(texid);
}

static const video_poke_interface_t d3d9_poke_interface = {
   NULL,                            /* set_coords */
   d3d9_set_mvp,
   d3d9_load_texture,
   d3d9_unload_texture,
   NULL,
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

   d3d9_show_mouse,
   NULL,                         /* grab_mouse_toggle */
   NULL,                         /* get_current_shader */
   NULL,                         /* get_current_software_framebuffer */
   NULL                          /* get_hw_render_interface */
};

static void d3d9_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &d3d9_poke_interface;
}

video_driver_t video_d3d9 = {
   d3d9_init,
   d3d9_frame,
   d3d9_set_nonblock_state,
   d3d9_alive,
   NULL,                      /* focus */
   d3d9_suppress_screensaver,
   NULL,                      /* has_windowed */
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
   d3d9_get_poke_interface
};
