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

#include "../../defines/d3d_defines.h"
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

#ifdef _XBOX
#define D3D9_PRESENTATIONINTERVAL D3DRS_PRESENTINTERVAL
#endif

#define FS_PRESENTINTERVAL(pp) ((pp)->PresentationInterval)

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#ifdef HAVE_MENU_WIDGETS
#include "../../menu/widgets/menu_widgets.h"
#endif
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
static uint32_t d3d9_get_flags(void *data);
static bool d3d9_set_shader(void *data, enum rarch_shader_type type, const char *path);

static LPDIRECT3D9 g_pD3D9;
static enum rarch_shader_type supported_shader_type = RARCH_SHADER_NONE;

void *dinput;

#ifdef _XBOX
static bool d3d9_widescreen_mode = false;
#endif

static bool d3d9_set_resize(d3d9_video_t *d3d,
      unsigned new_width, unsigned new_height)
{
   /* No changes? */
   if (     (new_width  == d3d->video_info.width)
         && (new_height == d3d->video_info.height))
      return false;

   RARCH_LOG("[D3D9]: Resize %ux%u.\n", new_width, new_height);
   d3d->video_info.width  = new_width;
   d3d->video_info.height = new_height;
   video_driver_set_size(&new_width, &new_height);

   return true;
}

extern d3d9_renderchain_driver_t cg_d3d9_renderchain;
extern d3d9_renderchain_driver_t hlsl_d3d9_renderchain;

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

static void d3d9_log_info(const struct LinkInfo *info)
{
   RARCH_LOG("[D3D9]: Render pass info:\n");
   RARCH_LOG("\tTexture width: %u\n", info->tex_w);
   RARCH_LOG("\tTexture height: %u\n", info->tex_h);

   RARCH_LOG("\tScale type (X): ");

   switch (info->pass->fbo.type_x)
   {
      case RARCH_SCALE_INPUT:
         RARCH_LOG("Relative @ %fx\n", info->pass->fbo.scale_x);
         break;

      case RARCH_SCALE_VIEWPORT:
         RARCH_LOG("Viewport @ %fx\n", info->pass->fbo.scale_x);
         break;

      case RARCH_SCALE_ABSOLUTE:
         RARCH_LOG("Absolute @ %u px\n", info->pass->fbo.abs_x);
         break;
   }

   RARCH_LOG("\tScale type (Y): ");

   switch (info->pass->fbo.type_y)
   {
      case RARCH_SCALE_INPUT:
         RARCH_LOG("Relative @ %fx\n", info->pass->fbo.scale_y);
         break;

      case RARCH_SCALE_VIEWPORT:
         RARCH_LOG("Viewport @ %fx\n", info->pass->fbo.scale_y);
         break;

      case RARCH_SCALE_ABSOLUTE:
         RARCH_LOG("Absolute @ %u px\n", info->pass->fbo.abs_y);
         break;
   }

   RARCH_LOG("\tBilinear filter: %s\n",
         info->pass->filter == RARCH_FILTER_LINEAR ? "true" : "false");
}

static bool d3d9_init_chain(d3d9_video_t *d3d, const video_info_t *video_info)
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
      RARCH_ERR("[D3D9]: Renderchain could not be initialized.\n");
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
      RARCH_ERR("[D3D9]: Failed to init render chain.\n");
      return false;
   }

   RARCH_LOG("[D3D9]: Renderchain driver: %s\n", d3d->renderchain_driver->ident);
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

      current_width = out_width;
      current_height = out_height;

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

         for (i = 0; i < d3d->shader.luts; i++)
         {
            if (!d3d->renderchain_driver->add_lut(
                     d3d->renderchain_data,
                     d3d->shader.lut[i].id, d3d->shader.lut[i].path,
                     d3d->shader.lut[i].filter == RARCH_FILTER_UNSPEC ?
                     settings->bools.video_smooth :
                     (d3d->shader.lut[i].filter == RARCH_FILTER_LINEAR)))
            {
               RARCH_ERR("[D3D9]: Failed to init LUTs.\n");
               return false;
            }
         }
      }
   }

   return true;
}

static bool d3d9_init_singlepass(d3d9_video_t *d3d)
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

static bool d3d9_init_multipass(d3d9_video_t *d3d, const char *shader_path)
{
   unsigned i;
   bool            use_extra_pass = false;
   struct video_shader_pass *pass = NULL;
   config_file_t            *conf = video_shader_read_preset(shader_path);

   if (!conf)
   {
      RARCH_ERR("[D3D9]: Failed to load preset.\n");
      return false;
   }

   memset(&d3d->shader, 0, sizeof(d3d->shader));

   if (!video_shader_read_conf_preset(conf, &d3d->shader))
   {
      config_file_free(conf);
      RARCH_ERR("[D3D9]: Failed to parse shader preset.\n");
      return false;
   }

   config_file_free(conf);

   RARCH_LOG("[D3D9]: Found %u shaders.\n", d3d->shader.passes);

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

static bool d3d9_process_shader(d3d9_video_t *d3d)
{
   const char *shader_path = d3d->shader_path;
   if (d3d && !string_is_empty(shader_path))
      return d3d9_init_multipass(d3d, shader_path);

   return d3d9_init_singlepass(d3d);
}

static void d3d9_viewport_info(void *data, struct video_viewport *vp)
{
   unsigned width, height;
   d3d9_video_t *d3d   = (d3d9_video_t*)data;

   if (!vp)
      return;

   video_driver_get_size(&width, &height);

   vp->x            = d3d->final_viewport.X;
   vp->y            = d3d->final_viewport.Y;
   vp->width        = d3d->final_viewport.Width;
   vp->height       = d3d->final_viewport.Height;

   vp->full_width   = width;
   vp->full_height  = height;
}

void d3d9_set_mvp(void *data, const void *mat_data)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)data;
   d3d9_set_vertex_shader_constantf(dev, 0, (const float*)mat_data, 4);
}

#if defined(HAVE_MENU) || defined(HAVE_OVERLAY)
static void d3d9_overlay_render(d3d9_video_t *d3d,
      video_frame_info_t *video_info,
      overlay_t *overlay, bool force_linear)
{
   D3DTEXTUREFILTERTYPE filter_type;
   LPDIRECT3DVERTEXDECLARATION9 vertex_decl;
   LPDIRECT3DDEVICE9 dev;
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

   dev                 = d3d->dev;

   if (!overlay->vert_buf)
   {
      overlay->vert_buf = d3d9_vertex_buffer_new(
      dev, sizeof(vert), D3DUSAGE_WRITEONLY,
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

   verts = d3d9_vertex_buffer_lock((LPDIRECT3DVERTEXBUFFER9)overlay->vert_buf);
   memcpy(verts, vert, sizeof(vert));
   d3d9_vertex_buffer_unlock((LPDIRECT3DVERTEXBUFFER9)overlay->vert_buf);

   d3d9_enable_blend_func(d3d->dev);

   /* set vertex declaration for overlay. */
   d3d9_vertex_declaration_new(dev, &vElems, (void**)&vertex_decl);
   d3d9_set_vertex_declaration(dev, vertex_decl);
   d3d9_vertex_declaration_free(vertex_decl);

   d3d9_set_stream_source(dev, 0, (LPDIRECT3DVERTEXBUFFER9)overlay->vert_buf,
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
      d3d9_set_viewports(dev, &vp_full);
   }

   filter_type = D3DTEXF_LINEAR;

   if (!force_linear)
   {
      settings_t *settings = config_get_ptr();
      if (!settings->bools.menu_linear_filter)
         filter_type       = D3DTEXF_POINT;
   }

   /* Render overlay. */
   d3d9_set_texture(dev, 0, (LPDIRECT3DTEXTURE9)overlay->tex);
   d3d9_set_sampler_address_u(dev, 0, D3DTADDRESS_BORDER);
   d3d9_set_sampler_address_v(dev, 0, D3DTADDRESS_BORDER);
   d3d9_set_sampler_minfilter(dev, 0, filter_type);
   d3d9_set_sampler_magfilter(dev, 0, filter_type);
   d3d9_draw_primitive(dev, D3DPT_TRIANGLESTRIP, 0, 2);

   /* Restore previous state. */
   d3d9_disable_blend_func(dev);
   d3d9_set_viewports(dev, &d3d->final_viewport);
}
#endif

static void d3d9_free_overlay(d3d9_video_t *d3d, overlay_t *overlay)
{
   if (!d3d)
      return;

   d3d9_texture_free((LPDIRECT3DTEXTURE9)overlay->tex);
   d3d9_vertex_buffer_free(overlay->vert_buf, NULL);
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

static void d3d9_deinitialize(d3d9_video_t *d3d)
{
   if (!d3d)
      return;

   font_driver_free_osd();

   d3d9_deinit_chain(d3d);
   d3d9_vertex_buffer_free(d3d->menu_display.buffer, d3d->menu_display.decl);

   d3d->menu_display.buffer = NULL;
   d3d->menu_display.decl = NULL;
}

static D3DFORMAT d3d9_get_color_format_backbuffer(bool rgb32, bool windowed)
{
   D3DFORMAT fmt = D3DFMT_X8R8G8B8;
#ifdef _XBOX
   if (!rgb32)
      fmt        = d3d9_get_rgb565_format();
#else
   if (windowed)
   {
      D3DDISPLAYMODE display_mode;
      if (d3d9_get_adapter_display_mode(g_pD3D9, 0, &display_mode))
         fmt = display_mode.Format;
   }
#endif
   return fmt;
}

#ifdef _XBOX
static D3DFORMAT d3d9_get_color_format_front_buffer(void)
{
   return D3DFMT_LE_X8R8G8B8;
}
#endif

static bool d3d9_is_windowed_enable(bool info_fullscreen)
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

#ifdef _XBOX
static void d3d9_get_video_size(d3d9_video_t *d3d,
      unsigned *width, unsigned *height)
{
   XVIDEO_MODE video_mode;

   XGetVideoMode(&video_mode);

   *width  = video_mode.dwDisplayWidth;
   *height = video_mode.dwDisplayHeight;

   d3d->resolution_hd_enable = false;

   if(video_mode.fIsHiDef)
   {
      *width = 1280;
      *height = 720;
      d3d->resolution_hd_enable = true;
   }
   else
   {
      *width = 640;
      *height = 480;
   }

   d3d9_widescreen_mode = video_mode.fIsWideScreen;
}
#endif

void d3d9_make_d3dpp(void *data,
      const video_info_t *info, void *_d3dpp)
{
   d3d9_video_t *d3d              = (d3d9_video_t*)data;
   D3DPRESENT_PARAMETERS *d3dpp   = (D3DPRESENT_PARAMETERS*)_d3dpp;
#ifdef _XBOX
   /* TODO/FIXME - get rid of global state dependencies. */
   global_t *global               = global_get_ptr();
   bool gamma_enable              = global ?
      global->console.screen.gamma_correction : false;
#endif
   bool windowed_enable           = d3d9_is_windowed_enable(info->fullscreen);

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
   d3dpp->BackBufferFormat        = d3d9_get_color_format_backbuffer(
         info->rgb32, windowed_enable);

#ifdef _XBOX
   d3dpp->FrontBufferFormat       = d3d9_get_color_format_front_buffer();

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
      unsigned width  = 0;
      unsigned height = 0;
      d3d9_get_video_size(d3d, &width, &height);
      video_driver_set_size(&width, &height);
#endif
      video_driver_get_size(&d3dpp->BackBufferWidth,
            &d3dpp->BackBufferHeight);
   }

#ifdef _XBOX
   d3dpp->MultiSampleType         = D3DMULTISAMPLE_NONE;
   d3dpp->EnableAutoDepthStencil  = FALSE;
   if (!d3d9_widescreen_mode)
      d3dpp->Flags |= D3DPRESENTFLAG_NO_LETTERBOX;
   d3dpp->MultiSampleQuality      = 0;
#endif
}

static bool d3d9_init_base(void *data, const video_info_t *info)
{
   D3DPRESENT_PARAMETERS d3dpp;
   HWND focus_window  = NULL;
   d3d9_video_t *d3d  = (d3d9_video_t*)data;

#ifndef _XBOX
   focus_window       = win32_get_window();
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

static void d3d9_calculate_rect(void *data,
      unsigned *width, unsigned *height,
      int *x, int *y,
      bool force_full,
      bool allow_rotate)
{
   float device_aspect   = (float)*width / *height;
   d3d9_video_t *d3d     = (d3d9_video_t*)data;
   settings_t *settings  = config_get_ptr();

   video_driver_get_size(width, height);

   *x                   = 0;
   *y                   = 0;

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

static void d3d9_set_font_rect(
      d3d9_video_t *d3d,
      const struct font_params *params)
{
   settings_t *settings             = config_get_ptr();
   float pos_x                      = settings->floats.video_msg_pos_x;
   float pos_y                      = settings->floats.video_msg_pos_y;
   float font_size                  = settings->floats.video_font_size;

   if (params)
   {
      pos_x                       = params->x;
      pos_y                       = params->y;
      font_size                  *= params->scale;
   }

   if (!d3d)
      return;

   d3d->font_rect.left            = d3d->video_info.width * pos_x;
   d3d->font_rect.right           = d3d->video_info.width;
   d3d->font_rect.top             = (1.0f - pos_y) * d3d->video_info.height - font_size;
   d3d->font_rect.bottom          = d3d->video_info.height;

   d3d->font_rect_shifted         = d3d->font_rect;
   d3d->font_rect_shifted.left   -= 2;
   d3d->font_rect_shifted.right  -= 2;
   d3d->font_rect_shifted.top    += 2;
   d3d->font_rect_shifted.bottom += 2;
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

   if (!d3d9_init_chain(d3d, info))
   {
      RARCH_ERR("[D3D9]: Failed to initialize render chain.\n");
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

static bool d3d9_restore(void *data)
{
   d3d9_video_t            *d3d = (d3d9_video_t*)data;

   if (!d3d)
      return false;

   d3d9_deinitialize(d3d);

   if (!d3d9_initialize(d3d, &d3d->video_info))
   {
      RARCH_ERR("[D3D9]: Restore error.\n");
      return false;
   }

   d3d->needs_restore = false;

   return true;
}

static void d3d9_set_nonblock_state(void *data, bool state)
{
   int interval                 = 0;
   d3d9_video_t            *d3d = (d3d9_video_t*)data;

   if (!d3d)
      return;

   if (!state)
      interval           = 1;

   d3d->video_info.vsync = !state;

   (void)interval;

#ifdef _XBOX
   d3d9_set_render_state(d3d->dev,
         D3D9_PRESENTATIONINTERVAL,
         interval ?
         D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE
         );
#else
   d3d->needs_restore = true;
   d3d9_restore(d3d);
#endif
}

static bool d3d9_alive(void *data)
{
   unsigned temp_width   = 0;
   unsigned temp_height  = 0;
   bool ret              = false;
   bool        quit      = false;
   bool        resize    = false;
   d3d9_video_t *d3d     = (d3d9_video_t*)data;

   /* Needed because some context drivers don't track their sizes */
   video_driver_get_size(&temp_width, &temp_height);

#ifndef _XBOX
   win32_check_window(&quit, &resize, &temp_width, &temp_height);
#endif

   if (quit)
      d3d->quitting      = quit;

   if (resize)
   {
      d3d->should_resize = true;
      d3d9_set_resize(d3d, temp_width, temp_height);
      d3d9_restore(d3d);
   }

   ret = !quit;

   if (  temp_width  != 0 &&
         temp_height != 0)
      video_driver_set_size(&temp_width, &temp_height);

   return ret;
}

static bool d3d9_suppress_screensaver(void *data, bool enable)
{
#ifdef _XBOX
   return true;
#else
   return win32_suppress_screensaver(data, enable);
#endif
}

static void d3d9_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;

   if (!d3d)
      return;

   d3d->keep_aspect   = true;
   d3d->should_resize = true;
}

static void d3d9_apply_state_changes(void *data)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;
   if (d3d)
      d3d->should_resize = true;
}

static void d3d9_set_osd_msg(void *data,
      video_frame_info_t *video_info,
      const char *msg,
      const void *params, void *font)
{
   d3d9_video_t          *d3d = (d3d9_video_t*)data;
   LPDIRECT3DDEVICE9     dev  = d3d->dev;
   const struct font_params *d3d_font_params = (const
         struct font_params*)params;

   d3d9_set_font_rect(d3d, d3d_font_params);
   d3d9_begin_scene(dev);
   font_driver_render_msg(d3d, video_info,
         msg, d3d_font_params, font);
   d3d9_end_scene(dev);
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
   d3d->windowClass.lpfnWndProc = WndProcD3D;
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

   d3d->video_info = *info;

   if (!d3d9_initialize(d3d, &d3d->video_info))
      return false;

   {

      d3d9_fake_context.get_flags = d3d9_get_flags;
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
   }

   d3d_input_driver(settings->arrays.input_joypad_driver,
      settings->arrays.input_joypad_driver, input, input_data);

   {
      char version_str[128];
      D3DADAPTER_IDENTIFIER9 ident = {0};

      IDirect3D9_GetAdapterIdentifier(g_pD3D9, 0, 0, &ident);

      version_str[0] = '\0';

      snprintf(version_str, sizeof(version_str), "%u.%u.%u.%u", HIWORD(ident.DriverVersion.HighPart), LOWORD(ident.DriverVersion.HighPart), HIWORD(ident.DriverVersion.LowPart), LOWORD(ident.DriverVersion.LowPart));

      RARCH_LOG("[D3D9]: Using GPU: %s\n", ident.Description);
      RARCH_LOG("[D3D9]: GPU API Version: %s\n", version_str);

      video_driver_set_gpu_device_string(ident.Description);
      video_driver_set_gpu_api_version_string(version_str);
   }

   RARCH_LOG("[D3D9]: Init complete.\n");
   return true;
}

static void d3d9_set_rotation(void *data, unsigned rot)
{
   d3d9_video_t        *d3d = (d3d9_video_t*)data;
   struct video_ortho ortho = {0, 1, 0, 1, -1, 1};

   if (!d3d)
      return;

   d3d->dev_rotation = rot;
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

#ifdef HAVE_OVERLAY
static void d3d9_free_overlays(d3d9_video_t *d3d)
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

#ifdef HAVE_OVERLAY
static void d3d9_overlay_tex_geom(
      void *data,
      unsigned index,
      float x, float y,
      float w, float h)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;

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
   d3d9_video_t *d3d = (d3d9_video_t*)data;

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
   d3d9_video_t *d3d                  = (d3d9_video_t*)data;
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

      overlay->tex       = d3d9_texture_new(d3d->dev, NULL,
                  width, height, 1,
                  0,
                  d3d9_get_argb8888_format(),
                  D3DPOOL_MANAGED, 0, 0, 0,
                  NULL, NULL, false);

      if (!overlay->tex)
      {
         RARCH_ERR("[D3D9]: Failed to create overlay texture\n");
         return false;
      }

      if (d3d9_lock_rectangle((LPDIRECT3DTEXTURE9)overlay->tex, 0, &d3dlr,
               NULL, 0, D3DLOCK_NOSYSLOCK))
      {
         uint32_t       *dst = (uint32_t*)(d3dlr.pBits);
         const uint32_t *src = images[i].pixels;
         unsigned      pitch = d3dlr.Pitch >> 2;

         for (y = 0; y < height; y++, dst += pitch, src += width)
            memcpy(dst, src, width << 2);
         d3d9_unlock_rectangle((LPDIRECT3DTEXTURE9)overlay->tex);
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
   d3d9_video_t            *d3d = (d3d9_video_t*)data;

   if (!d3d)
      return;

   for (i = 0; i < d3d->overlays_size; i++)
      d3d->overlays_enabled = state;

#ifndef XBOX
   win32_show_cursor(d3d, state);
#endif
}

static void d3d9_overlay_full_screen(void *data, bool enable)
{
   unsigned i;
   d3d9_video_t *d3d = (d3d9_video_t*)data;

   for (i = 0; i < d3d->overlays_size; i++)
      d3d->overlays[i].fullscreen = enable;
}

static void d3d9_overlay_set_alpha(void *data, unsigned index, float mod)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;
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

static void d3d9_update_title(video_frame_info_t *video_info)
{
#ifndef _XBOX
   const ui_window_t *window      = ui_companion_driver_get_window_ptr();

   if (window)
   {
      char title[128];

      title[0] = '\0';

      video_driver_get_window_title(title, sizeof(title));

      if (title[0])
         window->set_title(&main_window, title);
   }
#endif
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

   /* Insert black frame first, so we
    * can screenshot, etc. */
   if (video_info->black_frame_insertion)
   {
      if (!d3d9_swap(d3d, d3d->dev) || d3d->needs_restore)
         return true;
      d3d9_clear(d3d->dev, 0, 0, D3DCLEAR_TARGET, 0, 1, 0);
   }

   if (!d3d->renderchain_driver->render(
            d3d, video_info,
            frame, frame_width, frame_height,
            pitch, d3d->dev_rotation))
   {
      RARCH_ERR("[D3D9]: Failed to render scene.\n");
      return false;
   }

#ifdef HAVE_MENU
   if (d3d->menu && d3d->menu->enabled)
   {
      d3d9_set_mvp(d3d->dev, &d3d->mvp);
      d3d9_overlay_render(d3d, video_info, d3d->menu, false);

      d3d->menu_display.offset = 0;
      d3d9_set_vertex_declaration(d3d->dev, (LPDIRECT3DVERTEXDECLARATION9)d3d->menu_display.decl);
      d3d9_set_stream_source(d3d->dev, 0, (LPDIRECT3DVERTEXBUFFER9)d3d->menu_display.buffer, 0, sizeof(Vertex));

      d3d9_set_viewports(d3d->dev, &screen_vp);
      menu_driver_frame(video_info);
   }
   else if (video_info->statistics_show)
   {
      struct font_params *osd_params = (struct font_params*)
         &video_info->osd_stat_params;

      if (osd_params)
      {
         d3d9_set_viewports(d3d->dev, &screen_vp);
         d3d9_begin_scene(d3d->dev);
         font_driver_render_msg(d3d, video_info, video_info->stat_text,
               (const struct font_params*)&video_info->osd_stat_params, NULL);
         d3d9_end_scene(d3d->dev);
      }
   }
#endif

#ifdef HAVE_OVERLAY
   if (d3d->overlays_enabled)
   {
      d3d9_set_mvp(d3d->dev, &d3d->mvp);
      for (i = 0; i < d3d->overlays_size; i++)
         d3d9_overlay_render(d3d, video_info, &d3d->overlays[i], true);
   }
#endif

#ifdef HAVE_MENU
#ifdef HAVE_MENU_WIDGETS
   if (video_info->widgets_inited)
      menu_widgets_frame(video_info);
#endif
#endif

   if (msg && *msg)
   {
      d3d9_set_viewports(d3d->dev, &screen_vp);
      d3d9_begin_scene(d3d->dev);
      font_driver_render_msg(d3d, video_info, msg, NULL, NULL);
      d3d9_end_scene(d3d->dev);
   }

   d3d9_update_title(video_info);
   d3d9_swap(d3d, d3d->dev);

   return true;
}

static bool d3d9_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   unsigned width, height;
   D3DLOCKED_RECT rect;
   LPDIRECT3DSURFACE9 target = NULL;
   LPDIRECT3DSURFACE9 dest   = NULL;
   bool ret                  = true;
   d3d9_video_t *d3d         = (d3d9_video_t*)data;
   LPDIRECT3DDEVICE9 d3dr    = d3d->dev;

   video_driver_get_size(&width, &height);

   if (
         !d3d9_device_get_render_target(d3dr, 0, (void**)&target)     ||
         !d3d9_device_create_offscreen_plain_surface(d3dr, width, height,
            d3d9_get_xrgb8888_format(),
            D3DPOOL_SYSTEMMEM, (void**)&dest, NULL) ||
         !d3d9_device_get_render_target_data(d3dr, target, dest)
         )
   {
      ret = false;
      goto end;
   }

   if (d3d9_surface_lock_rect(dest, &rect))
   {
      unsigned x, y;
      unsigned pitchpix       = rect.Pitch / 4;
      const uint32_t *pixels  = (const uint32_t*)rect.pBits;

      pixels                 += d3d->final_viewport.X;
      pixels                 += (d3d->final_viewport.Height - 1) * pitchpix;
      pixels                 -= d3d->final_viewport.Y * pitchpix;

      for (y = 0; y < d3d->final_viewport.Height; y++, pixels -= pitchpix)
      {
         for (x = 0; x < d3d->final_viewport.Width; x++)
         {
            *buffer++ = (pixels[x] >>  0) & 0xff;
            *buffer++ = (pixels[x] >>  8) & 0xff;
            *buffer++ = (pixels[x] >> 16) & 0xff;
         }
      }

      d3d9_surface_unlock_rect(dest);
   }
   else
      ret = false;

end:
   if (target)
      d3d9_surface_free(target);
   if (dest)
      d3d9_surface_free(dest);
   return ret;
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
               path, video_shader_to_str(type), video_shader_to_str(supported_shader_type));
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

static void d3d9_set_menu_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   D3DLOCKED_RECT d3dlr;
   d3d9_video_t *d3d = (d3d9_video_t*)data;

   (void)d3dlr;
   (void)frame;
   (void)rgb32;
   (void)width;
   (void)height;
   (void)alpha;

   if (!d3d || !d3d->menu)
      return;

   if (    !d3d->menu->tex            ||
            d3d->menu->tex_w != width ||
            d3d->menu->tex_h != height)
   {
      d3d9_texture_free((LPDIRECT3DTEXTURE9)d3d->menu->tex);

      d3d->menu->tex = d3d9_texture_new(d3d->dev, NULL,
            width, height, 1,
            0, d3d9_get_argb8888_format(),
            D3DPOOL_MANAGED, 0, 0, 0, NULL, NULL, false);

      if (!d3d->menu->tex)
      {
         RARCH_ERR("[D3D9]: Failed to create menu texture.\n");
         return;
      }

      d3d->menu->tex_w          = width;
      d3d->menu->tex_h          = height;
   }

   d3d->menu->alpha_mod = alpha;

   if (d3d9_lock_rectangle((LPDIRECT3DTEXTURE9)d3d->menu->tex, 0, &d3dlr,
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
         d3d9_unlock_rectangle((LPDIRECT3DTEXTURE9)d3d->menu->tex);
   }
}

static void d3d9_set_menu_texture_enable(void *data,
      bool state, bool full_screen)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;

   if (!d3d || !d3d->menu)
      return;

   d3d->menu->enabled            = state;
   d3d->menu->fullscreen         = full_screen;
}

struct d3d9_texture_info
{
   void *userdata;
   void *data;
   enum texture_filter_type type;
};

static void d3d9_video_texture_load_d3d(
      struct d3d9_texture_info *info,
      uintptr_t *id)
{
   D3DLOCKED_RECT d3dlr;
   LPDIRECT3DTEXTURE9 tex   = NULL;
   unsigned usage           = 0;
   bool want_mipmap         = false;
   d3d9_video_t *d3d        = (d3d9_video_t*)info->userdata;
   struct texture_image *ti = (struct texture_image*)info->data;

   if (!ti)
      return;

   if((info->type == TEXTURE_FILTER_MIPMAP_LINEAR) ||
      (info->type == TEXTURE_FILTER_MIPMAP_NEAREST))
      want_mipmap        = true;

   tex = (LPDIRECT3DTEXTURE9)d3d9_texture_new(d3d->dev, NULL,
               ti->width, ti->height, 0,
               usage, d3d9_get_argb8888_format(),
               D3DPOOL_MANAGED, 0, 0, 0,
               NULL, NULL, want_mipmap);

   if (!tex)
   {
      RARCH_ERR("[D3D9]: Failed to create texture\n");
      return;
   }

   if (d3d9_lock_rectangle(tex, 0, &d3dlr,
            NULL, 0, D3DLOCK_NOSYSLOCK))
   {
      unsigned i;
      uint32_t       *dst = (uint32_t*)(d3dlr.pBits);
      const uint32_t *src = ti->pixels;
      unsigned      pitch = d3dlr.Pitch >> 2;

      for (i = 0; i < ti->height; i++, dst += pitch, src += ti->width)
         memcpy(dst, src, ti->width << 2);
      d3d9_unlock_rectangle(tex);
   }

   *id = (uintptr_t)tex;
}

#ifdef HAVE_THREADS
static int d3d9_video_texture_load_wrap_d3d(void *data)
{
   uintptr_t id = 0;
   struct d3d9_texture_info *info = (struct d3d9_texture_info*)data;
   if (!info)
      return 0;
   d3d9_video_texture_load_d3d(info, &id);
   return id;
}
#endif

static uintptr_t d3d9_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   uintptr_t id = 0;
   struct d3d9_texture_info info;

   info.userdata = video_data;
   info.data     = data;
   info.type     = filter_type;

#ifdef HAVE_THREADS
   if (threaded)
      return video_thread_texture_load(&info,
            d3d9_video_texture_load_wrap_d3d);
#endif

   d3d9_video_texture_load_d3d(&info, &id);
   return id;
}

static void d3d9_unload_texture(void *data, uintptr_t id)
{
   LPDIRECT3DTEXTURE9 texid;
   if (!id)
      return;

   texid = (LPDIRECT3DTEXTURE9)id;
   d3d9_texture_free(texid);
}

static void d3d9_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
#ifndef _XBOX
   win32_show_cursor(data, !fullscreen);
#endif
}

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

static const video_poke_interface_t d3d9_poke_interface = {
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
   NULL                          /* get_hw_render_interface */
};

static void d3d9_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &d3d9_poke_interface;
}

static bool d3d9_has_windowed(void *data)
{
#ifdef _XBOX
   return false;
#else
   return true;
#endif
}

#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
static bool d3d9_menu_widgets_enabled(void *data)
{
   (void)data;
   return false; /* currently disabled due to memory issues */
}
#endif

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
#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
   d3d9_menu_widgets_enabled
#endif
};
