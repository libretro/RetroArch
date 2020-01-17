/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2018 - Ali Bouhlel
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

#define CINTERFACE
#define COBJMACROS

#include <assert.h>

#include <string/stdstring.h>
#include <file/file_path.h>
#include <encodings/utf.h>
#include <lists/string_list.h>
#include <formats/image.h>

#include <dxgi.h>

#include "../../driver.h"
#include "../../verbosity.h"
#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../managers/state_manager.h"

#include "../font_driver.h"
#include "../common/d3d_common.h"
#include "../common/win32_common.h"
#include "../common/d3d10_common.h"
#include "../common/dxgi_common.h"
#include "../common/d3dcompiler_common.h"
#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#ifdef HAVE_MENU_WIDGETS
#include "../../menu/widgets/menu_widgets.h"
#endif
#endif

#ifdef __WINRT__
#error "UWP does not support D3D10"
#endif

#define D3D10_MAX_GPU_COUNT 16

/* Temporary workaround for d3d10 not being able to poll flags during init */
static gfx_ctx_driver_t d3d10_fake_context;
static uint32_t d3d10_get_flags(void *data);

static struct string_list *d3d10_gpu_list = NULL;
static IDXGIAdapter1 *d3d10_adapters[D3D10_MAX_GPU_COUNT] = {NULL};
static IDXGIAdapter1 *d3d10_current_adapter = NULL;

static void d3d10_clear_scissor(d3d10_video_t *d3d10, video_frame_info_t *video_info)
{
   D3D10_RECT scissor_rect;

   scissor_rect.left   = 0;
   scissor_rect.top    = 0;
   scissor_rect.right  = video_info->width;
   scissor_rect.bottom = video_info->height;

   D3D10SetScissorRects(d3d10->device, 1, &scissor_rect);
}


#ifdef HAVE_OVERLAY
static void d3d10_free_overlays(d3d10_video_t* d3d10)
{
   unsigned i;
   for (i = 0; i < (unsigned)d3d10->overlays.count; i++)
      d3d10_release_texture(&d3d10->overlays.textures[i]);

   Release(d3d10->overlays.vbo);
}

   static void
d3d10_overlay_vertex_geom(void* data, unsigned index, float x, float y, float w, float h)
{
   d3d10_sprite_t* sprites = NULL;
   d3d10_video_t*  d3d10 = (d3d10_video_t*)data;

   if (!d3d10)
      return;

   D3D10MapBuffer(d3d10->overlays.vbo,
         D3D10_MAP_WRITE_NO_OVERWRITE, 0, (void**)&sprites);
   sprites[index].pos.x    = x;
   sprites[index].pos.y    = y;
   sprites[index].pos.w    = w;
   sprites[index].pos.h    = h;
   D3D10UnmapBuffer(d3d10->overlays.vbo);
}

static void d3d10_overlay_tex_geom(void* data, unsigned index, float u, float v, float w, float h)
{
   d3d10_sprite_t* sprites = NULL;
   d3d10_video_t*  d3d10 = (d3d10_video_t*)data;

   if (!d3d10)
      return;

   D3D10MapBuffer(
         d3d10->overlays.vbo,
         D3D10_MAP_WRITE_NO_OVERWRITE, 0, (void**)&sprites);
   sprites[index].coords.u = u;
   sprites[index].coords.v = v;
   sprites[index].coords.w = w;
   sprites[index].coords.h = h;
   D3D10UnmapBuffer(d3d10->overlays.vbo);
}

static void d3d10_overlay_set_alpha(void* data, unsigned index, float mod)
{
   d3d10_sprite_t* sprites = NULL;
   d3d10_video_t*  d3d10 = (d3d10_video_t*)data;

   if (!d3d10)
      return;

   D3D10MapBuffer(
         d3d10->overlays.vbo,
         D3D10_MAP_WRITE_NO_OVERWRITE, 0, (void**)&sprites);
   sprites[index].colors[0] = DXGI_COLOR_RGBA(0xFF, 0xFF, 0xFF, mod * 0xFF);
   sprites[index].colors[1] = sprites[index].colors[0];
   sprites[index].colors[2] = sprites[index].colors[0];
   sprites[index].colors[3] = sprites[index].colors[0];
   D3D10UnmapBuffer(d3d10->overlays.vbo);
}

static bool d3d10_overlay_load(void* data, const void* image_data, unsigned num_images)
{
   D3D10_BUFFER_DESC desc;
   unsigned                          i;
   d3d10_sprite_t*             sprites;
   d3d10_video_t*              d3d10  = (d3d10_video_t*)data;
   const struct texture_image* images = (const struct texture_image*)image_data;

   if (!d3d10)
      return false;

   d3d10_free_overlays(d3d10);
   d3d10->overlays.count    = num_images;
   d3d10->overlays.textures = (d3d10_texture_t*)calloc(
         num_images, sizeof(d3d10_texture_t));

   desc.ByteWidth           = sizeof(d3d10_sprite_t) * num_images;
   desc.Usage               = D3D10_USAGE_DYNAMIC;
   desc.BindFlags           = D3D10_BIND_VERTEX_BUFFER;
   desc.CPUAccessFlags      = D3D10_CPU_ACCESS_WRITE;
   desc.MiscFlags           = 0;
#if 0
   desc.StructureByteStride = 0;
#endif
   D3D10CreateBuffer(d3d10->device, &desc, NULL, &d3d10->overlays.vbo);

   D3D10MapBuffer(d3d10->overlays.vbo,
         D3D10_MAP_WRITE_DISCARD, 0, (void**)&sprites);

   for (i = 0; i < (unsigned)num_images; i++)
   {

      d3d10->overlays.textures[i].desc.Width  = images[i].width;
      d3d10->overlays.textures[i].desc.Height = images[i].height;
      d3d10->overlays.textures[i].desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;

      d3d10_init_texture(d3d10->device, &d3d10->overlays.textures[i]);

      d3d10_update_texture(
            d3d10->device,
            images[i].width,
            images[i].height, 0, DXGI_FORMAT_B8G8R8A8_UNORM,
            images[i].pixels, &d3d10->overlays.textures[i]);

      sprites[i].pos.x           = 0.0f;
      sprites[i].pos.y           = 0.0f;
      sprites[i].pos.w           = 1.0f;
      sprites[i].pos.h           = 1.0f;

      sprites[i].coords.u        = 0.0f;
      sprites[i].coords.v        = 0.0f;
      sprites[i].coords.w        = 1.0f;
      sprites[i].coords.h        = 1.0f;

      sprites[i].params.scaling  = 1;
      sprites[i].params.rotation = 0;

      sprites[i].colors[0]       = 0xFFFFFFFF;
      sprites[i].colors[1]       = sprites[i].colors[0];
      sprites[i].colors[2]       = sprites[i].colors[0];
      sprites[i].colors[3]       = sprites[i].colors[0];
   }
   D3D10UnmapBuffer(d3d10->overlays.vbo);

   return true;
}

static void d3d10_overlay_enable(void* data, bool state)
{
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;

   if (!d3d10)
      return;

   d3d10->overlays.enabled = state;
   win32_show_cursor(d3d10, state);
}

static void d3d10_overlay_full_screen(void* data, bool enable)
{
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;

   if (!d3d10)
      return;

   d3d10->overlays.fullscreen = enable;
}

static void d3d10_get_overlay_interface(void* data, const video_overlay_interface_t** iface)
{
   static const video_overlay_interface_t overlay_interface = {
      d3d10_overlay_enable,      d3d10_overlay_load,        d3d10_overlay_tex_geom,
      d3d10_overlay_vertex_geom, d3d10_overlay_full_screen, d3d10_overlay_set_alpha,
   };

   *iface = &overlay_interface;
}
#endif

static void d3d10_set_filtering(void* data, unsigned index, bool smooth)
{
   unsigned       i;
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;

   for (i = 0; i < RARCH_WRAP_MAX; i++)
   {
      if (smooth)
         d3d10->samplers[RARCH_FILTER_UNSPEC][i] = d3d10->samplers[RARCH_FILTER_LINEAR][i];
      else
         d3d10->samplers[RARCH_FILTER_UNSPEC][i] = d3d10->samplers[RARCH_FILTER_NEAREST][i];
   }
}

static void d3d10_gfx_set_rotation(void* data, unsigned rotation)
{
   math_matrix_4x4 rot;
   void* mapped_ubo      = NULL;
   d3d10_video_t*  d3d10 = (d3d10_video_t*)data;

   if (!d3d10)
      return;

   matrix_4x4_rotate_z(rot, rotation * (M_PI / 2.0f));
   matrix_4x4_multiply(d3d10->mvp, rot, d3d10->ubo_values.mvp);

   D3D10MapBuffer(d3d10->frame.ubo, D3D10_MAP_WRITE_DISCARD, 0, &mapped_ubo);
   *(math_matrix_4x4*)mapped_ubo = d3d10->mvp;
   D3D10UnmapBuffer(d3d10->frame.ubo);
}

static void d3d10_update_viewport(void* data, bool force_full)
{
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;

   video_driver_update_viewport(&d3d10->vp, force_full, d3d10->keep_aspect);

   d3d10->frame.viewport.TopLeftX = d3d10->vp.x;
   d3d10->frame.viewport.TopLeftY = d3d10->vp.y;
   d3d10->frame.viewport.Width    = d3d10->vp.width;
   d3d10->frame.viewport.Height   = d3d10->vp.height;
   d3d10->frame.viewport.MaxDepth = 0.0f;
   d3d10->frame.viewport.MaxDepth = 1.0f;

   if (d3d10->shader_preset && (d3d10->frame.output_size.x != d3d10->vp.width ||
            d3d10->frame.output_size.y != d3d10->vp.height))
      d3d10->resize_render_targets = true;

   d3d10->frame.output_size.x = d3d10->vp.width;
   d3d10->frame.output_size.y = d3d10->vp.height;
   d3d10->frame.output_size.z = 1.0f / d3d10->vp.width;
   d3d10->frame.output_size.w = 1.0f / d3d10->vp.height;

   d3d10->resize_viewport = false;
}

static void d3d10_free_shader_preset(d3d10_video_t* d3d10)
{
   unsigned i;
   if (!d3d10->shader_preset)
      return;

   for (i = 0; i < d3d10->shader_preset->passes; i++)
   {
      unsigned j;

      free(d3d10->shader_preset->pass[i].source.string.vertex);
      free(d3d10->shader_preset->pass[i].source.string.fragment);
      free(d3d10->pass[i].semantics.textures);
      d3d10_release_shader(&d3d10->pass[i].shader);
      d3d10_release_texture(&d3d10->pass[i].rt);
      d3d10_release_texture(&d3d10->pass[i].feedback);

      for (j = 0; j < SLANG_CBUFFER_MAX; j++)
      {
         free(d3d10->pass[i].semantics.cbuffers[j].uniforms);
         Release(d3d10->pass[i].buffers[j]);
      }
   }

   memset(d3d10->pass, 0, sizeof(d3d10->pass));

   /* only free the history textures here */
   for (i = 1; i <= (unsigned)d3d10->shader_preset->history_size; i++)
      d3d10_release_texture(&d3d10->frame.texture[i]);

   memset(
         &d3d10->frame.texture[1], 0,
         sizeof(d3d10->frame.texture[1]) * d3d10->shader_preset->history_size);

   for (i = 0; i < d3d10->shader_preset->luts; i++)
      d3d10_release_texture(&d3d10->luts[i]);

   memset(d3d10->luts, 0, sizeof(d3d10->luts));

   free(d3d10->shader_preset);
   d3d10->shader_preset         = NULL;
   d3d10->init_history          = false;
   d3d10->resize_render_targets = false;
}

static bool d3d10_gfx_set_shader(void* data, enum rarch_shader_type type, const char* path)
{
#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
   unsigned         i;
   config_file_t* conf     = NULL;
   d3d10_texture_t* source = NULL;
   d3d10_video_t*   d3d10  = (d3d10_video_t*)data;

   if (!d3d10)
      return false;

   D3D10Flush(d3d10->device);
   d3d10_free_shader_preset(d3d10);

   if (string_is_empty(path))
      return true;

   if (type != RARCH_SHADER_SLANG)
   {
      RARCH_WARN("[D3D10] Only Slang shaders are supported. Falling back to stock.\n");
      return false;
   }

   if (!(conf = video_shader_read_preset(path)))
      return false;

   d3d10->shader_preset = (struct video_shader*)calloc(1, sizeof(*d3d10->shader_preset));

   if (!video_shader_read_conf_preset(conf, d3d10->shader_preset))
      goto error;

   source = &d3d10->frame.texture[0];
   for (i = 0; i < d3d10->shader_preset->passes; source = &d3d10->pass[i++].rt)
   {
      unsigned j;
      /* clang-format off */
      semantics_map_t semantics_map = {
         {
            /* Original */
            { &d3d10->frame.texture[0].view, 0,
               &d3d10->frame.texture[0].size_data, 0},

            /* Source */
            { &source->view, 0,
               &source->size_data, 0},

            /* OriginalHistory */
            { &d3d10->frame.texture[0].view, sizeof(*d3d10->frame.texture),
               &d3d10->frame.texture[0].size_data, sizeof(*d3d10->frame.texture)},

            /* PassOutput */
            { &d3d10->pass[0].rt.view, sizeof(*d3d10->pass),
               &d3d10->pass[0].rt.size_data, sizeof(*d3d10->pass)},

            /* PassFeedback */
            { &d3d10->pass[0].feedback.view, sizeof(*d3d10->pass),
               &d3d10->pass[0].feedback.size_data, sizeof(*d3d10->pass)},

            /* User */
            { &d3d10->luts[0].view, sizeof(*d3d10->luts),
               &d3d10->luts[0].size_data, sizeof(*d3d10->luts)},
         },
         {
            &d3d10->mvp,                     /* MVP */
            &d3d10->pass[i].rt.size_data,    /* OutputSize */
            &d3d10->frame.output_size,       /* FinalViewportSize */
            &d3d10->pass[i].frame_count,     /* FrameCount */
            &d3d10->pass[i].frame_direction, /* FrameDirection */
         }
      };
      /* clang-format on */

      if (!slang_process(
               d3d10->shader_preset, i, RARCH_SHADER_HLSL, 40, &semantics_map,
               &d3d10->pass[i].semantics))
         goto error;

      {
         static const D3D10_INPUT_ELEMENT_DESC desc[] = {
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d10_vertex_t, position),
               D3D10_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d10_vertex_t, texcoord),
               D3D10_INPUT_PER_VERTEX_DATA, 0 },
         };
#ifdef DEBUG
         bool save_hlsl = true;
#else
         bool save_hlsl = false;
#endif
         static const char vs_ext[] = ".vs.hlsl";
         static const char ps_ext[] = ".ps.hlsl";
         char              vs_path[PATH_MAX_LENGTH] = {0};
         char              ps_path[PATH_MAX_LENGTH] = {0};
         const char*       slang_path = d3d10->shader_preset->pass[i].source.path;
         const char*       vs_src     = d3d10->shader_preset->pass[i].source.string.vertex;
         const char*       ps_src     = d3d10->shader_preset->pass[i].source.string.fragment;

         strlcpy(vs_path, slang_path, sizeof(vs_path));
         strlcpy(ps_path, slang_path, sizeof(ps_path));
         strlcat(vs_path, vs_ext, sizeof(vs_path));
         strlcat(ps_path, ps_ext, sizeof(ps_path));

         if (!d3d10_init_shader(
                  d3d10->device, vs_src, 0, vs_path, "main", NULL, NULL, desc, countof(desc),
                  &d3d10->pass[i].shader))
            save_hlsl = true;

         if (!d3d10_init_shader(
                  d3d10->device, ps_src, 0, ps_path, NULL, "main", NULL, NULL, 0,
                  &d3d10->pass[i].shader))
            save_hlsl = true;

         if (save_hlsl)
         {
            FILE* fp = fopen(vs_path, "w");
            fwrite(vs_src, 1, strlen(vs_src), fp);
            fclose(fp);

            fp = fopen(ps_path, "w");
            fwrite(ps_src, 1, strlen(ps_src), fp);
            fclose(fp);
         }

         free(d3d10->shader_preset->pass[i].source.string.vertex);
         free(d3d10->shader_preset->pass[i].source.string.fragment);

         d3d10->shader_preset->pass[i].source.string.vertex   = NULL;
         d3d10->shader_preset->pass[i].source.string.fragment = NULL;

         if (!d3d10->pass[i].shader.vs || !d3d10->pass[i].shader.ps)
            goto error;
      }

      for (j = 0; j < SLANG_CBUFFER_MAX; j++)
      {
         D3D10_BUFFER_DESC desc;
         desc.ByteWidth           = d3d10->pass[i].semantics.cbuffers[j].size;
         desc.Usage               = D3D10_USAGE_DYNAMIC;
         desc.BindFlags           = D3D10_BIND_CONSTANT_BUFFER;
         desc.CPUAccessFlags      = D3D10_CPU_ACCESS_WRITE;
         desc.MiscFlags           = 0;

         if (!desc.ByteWidth)
            continue;

         D3D10CreateBuffer(d3d10->device, &desc,
               NULL, &d3d10->pass[i].buffers[j]);
      }
   }

   for (i = 0; i < d3d10->shader_preset->luts; i++)
   {
      struct texture_image image = { 0 };
      image.supports_rgba        = true;

      if (!image_texture_load(&image, d3d10->shader_preset->lut[i].path))
         goto error;

      d3d10->luts[i].desc.Width  = image.width;
      d3d10->luts[i].desc.Height = image.height;
      d3d10->luts[i].desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

      if (d3d10->shader_preset->lut[i].mipmap)
         d3d10->luts[i].desc.MiscFlags = D3D10_RESOURCE_MISC_GENERATE_MIPS;

      d3d10_init_texture(d3d10->device, &d3d10->luts[i]);

      d3d10_update_texture(
            d3d10->device,
            image.width, image.height, 0,
            DXGI_FORMAT_R8G8B8A8_UNORM, image.pixels,
            &d3d10->luts[i]);

      image_texture_free(&image);
   }

   video_shader_resolve_current_parameters(conf, d3d10->shader_preset);
   config_file_free(conf);

   d3d10->resize_render_targets = true;
   d3d10->init_history          = true;

   return true;

error:
   d3d10_free_shader_preset(d3d10);
#endif

   return false;
}

static void d3d10_gfx_free(void* data)
{
   unsigned       i;
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;

   if (!d3d10)
      return;

#ifdef HAVE_OVERLAY
   d3d10_free_overlays(d3d10);
#endif

   d3d10_free_shader_preset(d3d10);

   d3d10_release_texture(&d3d10->frame.texture[0]);
   Release(d3d10->frame.ubo);
   Release(d3d10->frame.vbo);

   d3d10_release_texture(&d3d10->menu.texture);
   Release(d3d10->menu.vbo);

   d3d10_release_shader(&d3d10->sprites.shader);
   d3d10_release_shader(&d3d10->sprites.shader_font);
   Release(d3d10->sprites.vbo);

   for (i = 0; i < GFX_MAX_SHADERS; i++)
      d3d10_release_shader(&d3d10->shaders[i]);

   Release(d3d10->menu_pipeline_vbo);
   Release(d3d10->blend_pipeline);

   Release(d3d10->ubo);

   Release(d3d10->blend_enable);
   Release(d3d10->blend_disable);

   for (i = 0; i < RARCH_WRAP_MAX; i++)
   {
      Release(d3d10->samplers[RARCH_FILTER_LINEAR][i]);
      Release(d3d10->samplers[RARCH_FILTER_NEAREST][i]);
   }

   Release(d3d10->state);
   Release(d3d10->renderTargetView);
   Release(d3d10->swapChain);

   font_driver_free_osd();

#if 0
   if (video_driver_is_video_cache_context())
   {
      cached_device_d3d10 = d3d10->device;
      cached_context = d3d10->context;
   }
   else
#endif
   {
      Release(d3d10->device);
   }

   for (i = 0; i < D3D10_MAX_GPU_COUNT; i++)
   {
      if (d3d10_adapters[i])
      {
         Release(d3d10_adapters[i]);
         d3d10_adapters[i] = NULL;
      }
   }

#ifdef HAVE_MONITOR
   win32_monitor_from_window();
#endif
#ifdef HAVE_WINDOW
   win32_destroy_window();
#endif
   free(d3d10);
}

static void *d3d10_gfx_init(const video_info_t* video,
      input_driver_t** input, void** input_data)
{
   unsigned i;
#ifdef HAVE_MONITOR
   MONITORINFOEX   current_mon;
   HMONITOR        hm_to_use;
   WNDCLASSEX      wndclass = { 0 };
#endif
   settings_t*     settings = config_get_ptr();
   d3d10_video_t*  d3d10    = (d3d10_video_t*)calloc(1, sizeof(*d3d10));

   if (!d3d10)
      return NULL;

#ifdef HAVE_WINDOW
   win32_window_reset();
#endif
#ifdef HAVE_MONITOR
   win32_monitor_init();
   wndclass.lpfnWndProc = WndProcD3D;
#ifdef HAVE_WINDOW
   win32_window_init(&wndclass, true, NULL);
#endif

   win32_monitor_info(&current_mon, &hm_to_use, &d3d10->cur_mon_id);
#endif

   d3d10->vp.full_width  = video->width;
   d3d10->vp.full_height = video->height;

#ifdef HAVE_MONITOR
   if (!d3d10->vp.full_width)
      d3d10->vp.full_width =
         current_mon.rcMonitor.right - current_mon.rcMonitor.left;
   if (!d3d10->vp.full_height)
      d3d10->vp.full_height =
         current_mon.rcMonitor.bottom - current_mon.rcMonitor.top;
#endif

   if (!win32_set_video_mode(d3d10,
            d3d10->vp.full_width, d3d10->vp.full_height, video->fullscreen))
   {
      RARCH_ERR("[D3D10]: win32_set_video_mode failed.\n");
      goto error;
   }

   d3d_input_driver(settings->arrays.input_driver, settings->arrays.input_joypad_driver, input, input_data);

   {
      UINT                 flags = 0;
      DXGI_SWAP_CHAIN_DESC desc  = {{0}};

      desc.BufferCount           = 1;
      desc.BufferDesc.Width      = d3d10->vp.full_width;
      desc.BufferDesc.Height     = d3d10->vp.full_height;
      desc.BufferDesc.Format     = DXGI_FORMAT_R8G8B8A8_UNORM;
      desc.BufferDesc.RefreshRate.Numerator   = 60;
      desc.BufferDesc.RefreshRate.Denominator = 1;
      desc.BufferUsage           = DXGI_USAGE_RENDER_TARGET_OUTPUT;
#ifdef HAVE_WINDOW
      desc.OutputWindow          = main_window.hwnd;
#endif
      desc.SampleDesc.Count      = 1;
      desc.SampleDesc.Quality    = 0;
      desc.Windowed              = TRUE;
      desc.SwapEffect            = DXGI_SWAP_EFFECT_SEQUENTIAL;
#if 0
      desc.SwapEffect            = DXGI_SWAP_EFFECT_DISCARD;
      desc.SwapEffect            = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
      desc.SwapEffect            = DXGI_SWAP_EFFECT_FLIP_DISCARD;
#endif

#ifdef DEBUG
      flags |= D3D10_CREATE_DEVICE_DEBUG;
#endif

      if (FAILED(D3D10CreateDeviceAndSwapChain(
                  (IDXGIAdapter*)d3d10->adapter, D3D10_DRIVER_TYPE_HARDWARE,
                  NULL, flags, D3D10_SDK_VERSION, &desc,
                  (IDXGISwapChain**)&d3d10->swapChain, &d3d10->device)))
         goto error;
   }

   {
      D3D10Texture2D backBuffer;
      DXGIGetSwapChainBufferD3D10(d3d10->swapChain, 0, &backBuffer);
      D3D10CreateTexture2DRenderTargetView(
            d3d10->device, backBuffer, NULL, &d3d10->renderTargetView);
      Release(backBuffer);
   }

   D3D10SetRenderTargets(d3d10->device, 1, &d3d10->renderTargetView, NULL);

   video_driver_set_size(&d3d10->vp.full_width, &d3d10->vp.full_height);
   d3d10->viewport.Width  = d3d10->vp.full_width;
   d3d10->viewport.Height = d3d10->vp.full_height;
   d3d10->resize_viewport = true;
   d3d10->keep_aspect     = video->force_aspect;
   d3d10->vsync           = video->vsync;
   d3d10->format          = video->rgb32 ?
      DXGI_FORMAT_B8G8R8X8_UNORM : DXGI_FORMAT_B5G6R5_UNORM;

   d3d10->frame.texture[0].desc.Format = d3d10->format;
   d3d10->frame.texture[0].desc.Usage  = D3D10_USAGE_DEFAULT;
   d3d10->frame.texture[0].desc.Width  = 4;
   d3d10->frame.texture[0].desc.Height = 4;

   d3d10_init_texture(d3d10->device, &d3d10->frame.texture[0]);

   d3d10->menu.texture.desc.Usage  = D3D10_USAGE_DEFAULT;

   matrix_4x4_ortho(d3d10->ubo_values.mvp, 0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);

   d3d10->ubo_values.OutputSize.width  = d3d10->viewport.Width;
   d3d10->ubo_values.OutputSize.height = d3d10->viewport.Height;

   {
      D3D10_SUBRESOURCE_DATA ubo_data;
      D3D10_BUFFER_DESC desc;

      desc.ByteWidth            = sizeof(d3d10->ubo_values);
      desc.Usage                = D3D10_USAGE_DYNAMIC;
      desc.BindFlags            = D3D10_BIND_CONSTANT_BUFFER;
      desc.CPUAccessFlags       = D3D10_CPU_ACCESS_WRITE;
      desc.MiscFlags            = 0;

      ubo_data.pSysMem          = &d3d10->ubo_values.mvp;
      ubo_data.SysMemPitch      = 0;
      ubo_data.SysMemSlicePitch = 0;

      D3D10CreateBuffer(d3d10->device, &desc, &ubo_data, &d3d10->ubo);
      D3D10CreateBuffer(d3d10->device, &desc, NULL, &d3d10->frame.ubo);
   }

   d3d10_gfx_set_rotation(d3d10, 0);

   {
      D3D10_SAMPLER_DESC desc = { D3D10_FILTER_MIN_MAG_MIP_POINT };
      desc.MaxAnisotropy      = 1;
      desc.ComparisonFunc     = D3D10_COMPARISON_NEVER;
      desc.MinLOD             = -D3D10_FLOAT32_MAX;
      desc.MaxLOD             = D3D10_FLOAT32_MAX;

      /* Initialize samplers */
      for (i = 0; i < RARCH_WRAP_MAX; i++)
      {
         switch (i)
         {
            case RARCH_WRAP_BORDER:
               desc.AddressU = D3D10_TEXTURE_ADDRESS_BORDER;
               break;

            case RARCH_WRAP_EDGE:
               desc.AddressU = D3D10_TEXTURE_ADDRESS_CLAMP;
               break;

            case RARCH_WRAP_REPEAT:
               desc.AddressU = D3D10_TEXTURE_ADDRESS_WRAP;
               break;

            case RARCH_WRAP_MIRRORED_REPEAT:
               desc.AddressU = D3D10_TEXTURE_ADDRESS_MIRROR;
               break;
         }
         desc.AddressV = desc.AddressU;
         desc.AddressW = desc.AddressU;

         desc.Filter = D3D10_FILTER_MIN_MAG_MIP_LINEAR;
         D3D10CreateSamplerState(d3d10->device, &desc,
               &d3d10->samplers[RARCH_FILTER_LINEAR][i]);

         desc.Filter = D3D10_FILTER_MIN_MAG_MIP_POINT;
         D3D10CreateSamplerState(d3d10->device, &desc,
               &d3d10->samplers[RARCH_FILTER_NEAREST][i]);
      }
   }

   d3d10_set_filtering(d3d10, 0, video->smooth);

   {
      D3D10_BUFFER_DESC desc;
      d3d10_vertex_t vertices[] = {
         { { 0.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
         { { 0.0f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
         { { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
         { { 1.0f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
      };
      D3D10_SUBRESOURCE_DATA
         vertexData               = { vertices };

      desc.ByteWidth              = sizeof(vertices);
      desc.Usage                  = D3D10_USAGE_IMMUTABLE;
      desc.BindFlags              = D3D10_BIND_VERTEX_BUFFER;
      desc.CPUAccessFlags         = 0;
      desc.MiscFlags              = 0;

      D3D10CreateBuffer(d3d10->device, &desc, &vertexData, &d3d10->frame.vbo);
      desc.Usage          = D3D10_USAGE_DYNAMIC;
      desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
      D3D10CreateBuffer(d3d10->device, &desc, &vertexData, &d3d10->menu.vbo);

      d3d10->sprites.capacity  = 4096;
      desc.ByteWidth           = sizeof(d3d10_sprite_t) * d3d10->sprites.capacity;
      D3D10CreateBuffer(d3d10->device, &desc, NULL, &d3d10->sprites.vbo);
   }

   {
      D3D10_INPUT_ELEMENT_DESC desc[] = {
         { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d10_vertex_t, position),
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
         { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d10_vertex_t, texcoord),
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
         { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(d3d10_vertex_t, color),
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
      };

      static const char shader[] =
#include "d3d_shaders/opaque_sm5.hlsl.h"
         ;

      if (!d3d10_init_shader(
               d3d10->device, shader, sizeof(shader), NULL, "VSMain", "PSMain", NULL, desc,
               countof(desc), &d3d10->shaders[VIDEO_SHADER_STOCK_BLEND]))
         goto error;
   }

   {
      D3D10_INPUT_ELEMENT_DESC desc[] = {
         { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(d3d10_sprite_t, pos),
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
         { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(d3d10_sprite_t, coords),
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
         { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(d3d10_sprite_t, colors[0]),
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
         { "COLOR", 1, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(d3d10_sprite_t, colors[1]),
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
         { "COLOR", 2, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(d3d10_sprite_t, colors[2]),
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
         { "COLOR", 3, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(d3d10_sprite_t, colors[3]),
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
         { "PARAMS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d10_sprite_t, params),
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
      };

      static const char shader[] =
#include "d3d_shaders/sprite_sm4.hlsl.h"
         ;

      if (!d3d10_init_shader(
               d3d10->device, shader, sizeof(shader), NULL, "VSMain", "PSMain", "GSMain", desc,
               countof(desc), &d3d10->sprites.shader))
         goto error;
      if (!d3d10_init_shader(
               d3d10->device, shader, sizeof(shader), NULL, "VSMain", "PSMainA8", "GSMain", desc,
               countof(desc), &d3d10->sprites.shader_font))
         goto error;
   }

   if (string_is_equal(settings->arrays.menu_driver, "xmb"))
   {
      {
         D3D10_INPUT_ELEMENT_DESC desc[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
         };

         static const char ribbon[] =
#include "d3d_shaders/ribbon_sm4.hlsl.h"
            ;
         static const char ribbon_simple[] =
#include "d3d_shaders/ribbon_simple_sm4.hlsl.h"
            ;

         if (!d3d10_init_shader(
                  d3d10->device, ribbon, sizeof(ribbon), NULL, "VSMain", "PSMain", NULL, desc,
                  countof(desc), &d3d10->shaders[VIDEO_SHADER_MENU]))
            goto error;

         if (!d3d10_init_shader(
                  d3d10->device, ribbon_simple, sizeof(ribbon_simple), NULL, "VSMain", "PSMain", NULL,
                  desc, countof(desc), &d3d10->shaders[VIDEO_SHADER_MENU_2]))
            goto error;
      }

      {
         D3D10_INPUT_ELEMENT_DESC desc[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d10_vertex_t, position),
               D3D10_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d10_vertex_t, texcoord),
               D3D10_INPUT_PER_VERTEX_DATA, 0 },
         };

         static const char simple_snow[] =
#include "d3d_shaders/simple_snow_sm4.hlsl.h"
            ;
         static const char snow[] =
#include "d3d_shaders/snow_sm4.hlsl.h"
            ;
         static const char bokeh[] =
#include "d3d_shaders/bokeh_sm4.hlsl.h"
            ;
         static const char snowflake[] =
#include "d3d_shaders/snowflake_sm4.hlsl.h"
            ;

         if (!d3d10_init_shader(
                  d3d10->device, simple_snow, sizeof(simple_snow), NULL, "VSMain", "PSMain", NULL,
                  desc, countof(desc), &d3d10->shaders[VIDEO_SHADER_MENU_3]))
            goto error;
         if (!d3d10_init_shader(
                  d3d10->device, snow, sizeof(snow), NULL, "VSMain", "PSMain", NULL, desc,
                  countof(desc), &d3d10->shaders[VIDEO_SHADER_MENU_4]))
            goto error;

         if (!d3d10_init_shader(
                  d3d10->device, bokeh, sizeof(bokeh), NULL, "VSMain", "PSMain", NULL, desc,
                  countof(desc), &d3d10->shaders[VIDEO_SHADER_MENU_5]))
            goto error;

         if (!d3d10_init_shader(
                  d3d10->device, snowflake, sizeof(snowflake), NULL, "VSMain", "PSMain", NULL, desc,
                  countof(desc), &d3d10->shaders[VIDEO_SHADER_MENU_6]))
            goto error;
      }
   }

   {
      D3D10_BLEND_DESC blend_desc = { 0 };

      blend_desc.AlphaToCoverageEnable    = FALSE;
      blend_desc.BlendEnable[0]           = TRUE;
      blend_desc.SrcBlend                 = D3D10_BLEND_SRC_ALPHA;
      blend_desc.DestBlend                = D3D10_BLEND_INV_SRC_ALPHA;
      blend_desc.BlendOp                  = D3D10_BLEND_OP_ADD;
      blend_desc.SrcBlendAlpha            = D3D10_BLEND_SRC_ALPHA;
      blend_desc.DestBlendAlpha           = D3D10_BLEND_INV_SRC_ALPHA;
      blend_desc.BlendOpAlpha             = D3D10_BLEND_OP_ADD;
      blend_desc.RenderTargetWriteMask[0] = D3D10_COLOR_WRITE_ENABLE_ALL;
      D3D10CreateBlendState(d3d10->device, &blend_desc, &d3d10->blend_enable);

      blend_desc.SrcBlend  = D3D10_BLEND_ONE;
      blend_desc.DestBlend = D3D10_BLEND_ONE;
      D3D10CreateBlendState(d3d10->device, &blend_desc, &d3d10->blend_pipeline);

      blend_desc.BlendEnable[0] = FALSE;
      D3D10CreateBlendState(d3d10->device, &blend_desc, &d3d10->blend_disable);
   }

   {
      D3D10_RASTERIZER_DESC desc = { (D3D10_FILL_MODE)0 };

      desc.FillMode = D3D10_FILL_SOLID;
      desc.CullMode = D3D10_CULL_NONE;
      desc.ScissorEnable = TRUE;

      D3D10CreateRasterizerState(d3d10->device, &desc, &d3d10->state);
   }

   D3D10SetState(d3d10->device, d3d10->state);

   font_driver_init_osd(d3d10, false, video->is_threaded, FONT_DRIVER_RENDER_D3D10_API);

   {
      d3d10_fake_context.get_flags = d3d10_get_flags;
      d3d10_fake_context.get_metrics = win32_get_metrics;
      video_context_driver_set(&d3d10_fake_context); 
#ifdef HAVE_SLANG
      const char *shader_preset   = retroarch_get_shader_preset();
      enum rarch_shader_type type = video_shader_parse_type(shader_preset);
      d3d10_gfx_set_shader(d3d10, type, shader_preset);
#endif
   }

#if 0
   if (video_driver_get_hw_context()->context_type == RETRO_HW_CONTEXT_DIRECT3D &&
         video_driver_get_hw_context()->version_major == 11)
   {
      d3d10->hw.enable                  = true;
      d3d10->hw.iface.interface_type    = RETRO_HW_RENDER_INTERFACE_D3D10;
      d3d10->hw.iface.interface_version = RETRO_HW_RENDER_INTERFACE_D3D10_VERSION;
      d3d10->hw.iface.handle            = d3d10;
      d3d10->hw.iface.device            = d3d10->device;
      d3d10->hw.iface.context           = d3d10->context;
      d3d10->hw.iface.featureLevel      = d3d10->supportedFeatureLevel;
      d3d10->hw.iface.D3DCompile        = D3DCompile;
   }
#endif

#ifdef __WINRT__
   DXGICreateFactory2(&d3d10->factory);
#else
   DXGICreateFactory(&d3d10->factory);
#endif

   {
      int i = 0;

      if (d3d10_gpu_list)
         string_list_free(d3d10_gpu_list);

      d3d10_gpu_list = string_list_new();

      while (true)
      {
         DXGI_ADAPTER_DESC desc = {0};
         union string_list_elem_attr attr = {0};
         char str[128];

         str[0] = '\0';

#ifdef __WINRT__
         if (FAILED(DXGIEnumAdapters2(d3d10->factory, i, &d3d10->adapter)))
            break;
#else
         if (FAILED(DXGIEnumAdapters(d3d10->factory, i, &d3d10->adapter)))
            break;
#endif

         IDXGIAdapter_GetDesc(d3d10->adapter, &desc);

         utf16_to_char_string((const uint16_t*)
               desc.Description, str, sizeof(str));

         RARCH_LOG("[D3D10]: Found GPU at index %d: %s\n", i, str);

         string_list_append(d3d10_gpu_list, str, attr);

         if (i < D3D10_MAX_GPU_COUNT)
            d3d10_adapters[i] = d3d10->adapter;

         i++;
      }

      video_driver_set_gpu_api_devices(GFX_CTX_DIRECT3D10_API, d3d10_gpu_list);

      if (0 <= settings->ints.d3d10_gpu_index && settings->ints.d3d10_gpu_index <= i && settings->ints.d3d10_gpu_index < D3D10_MAX_GPU_COUNT)
      {
         d3d10_current_adapter = d3d10_adapters[settings->ints.d3d10_gpu_index];
         d3d10->adapter = d3d10_current_adapter;
         RARCH_LOG("[D3D10]: Using GPU index %d.\n", settings->ints.d3d10_gpu_index);
         video_driver_set_gpu_device_string(d3d10_gpu_list->elems[settings->ints.d3d10_gpu_index].data);
      }
      else
      {
         RARCH_WARN("[D3D10]: Invalid GPU index %d, using first device found.\n", settings->ints.d3d10_gpu_index);
         d3d10_current_adapter = d3d10_adapters[0];
         d3d10->adapter = d3d10_current_adapter;
      }
   }

   return d3d10;

error:
   d3d10_gfx_free(d3d10);
   return NULL;
}

static void d3d10_init_history(d3d10_video_t* d3d10, unsigned width, unsigned height)
{
   unsigned i;

   /* todo: should we init history to max_width/max_height instead ?
    * to prevent out of memory errors happening several frames later
    * and to reduce memory fragmentation */

   assert(d3d10->shader_preset);
   for (i = 0; i < (unsigned)d3d10->shader_preset->history_size + 1; i++)
   {
      d3d10->frame.texture[i].desc.Width  = width;
      d3d10->frame.texture[i].desc.Height = height;
      d3d10->frame.texture[i].desc.Format = d3d10->frame.texture[0].desc.Format;
      d3d10->frame.texture[i].desc.Usage  = d3d10->frame.texture[0].desc.Usage;
      d3d10_init_texture(d3d10->device, &d3d10->frame.texture[i]);
      /* todo: clear texture ?  */
   }
   d3d10->init_history = false;
}

static void d3d10_init_render_targets(d3d10_video_t* d3d10,
      unsigned width, unsigned height)
{
   unsigned i;

   assert(d3d10->shader_preset);

   for (i = 0; i < d3d10->shader_preset->passes; i++)
   {
      struct video_shader_pass* pass = &d3d10->shader_preset->pass[i];

      if (pass->fbo.valid)
      {

         switch (pass->fbo.type_x)
         {
            case RARCH_SCALE_INPUT:
               width *= pass->fbo.scale_x;
               break;

            case RARCH_SCALE_VIEWPORT:
               width = d3d10->vp.width * pass->fbo.scale_x;
               break;

            case RARCH_SCALE_ABSOLUTE:
               width = pass->fbo.abs_x;
               break;

            default:
               break;
         }

         if (!width)
            width = d3d10->vp.width;

         switch (pass->fbo.type_y)
         {
            case RARCH_SCALE_INPUT:
               height *= pass->fbo.scale_y;
               break;

            case RARCH_SCALE_VIEWPORT:
               height = d3d10->vp.height * pass->fbo.scale_y;
               break;

            case RARCH_SCALE_ABSOLUTE:
               height = pass->fbo.abs_y;
               break;

            default:
               break;
         }

         if (!height)
            height = d3d10->vp.height;
      }
      else if (i == (d3d10->shader_preset->passes - 1))
      {
         width  = d3d10->vp.width;
         height = d3d10->vp.height;
      }

      RARCH_LOG("[D3D10]: Updating framebuffer size %u x %u.\n", width, height);

      if ((i != (d3d10->shader_preset->passes - 1)) || (width != d3d10->vp.width) ||
            (height != d3d10->vp.height))
      {
         d3d10->pass[i].viewport.Width    = width;
         d3d10->pass[i].viewport.Height   = height;
         d3d10->pass[i].viewport.MaxDepth = 1.0;
         d3d10->pass[i].rt.desc.Width     = width;
         d3d10->pass[i].rt.desc.Height    = height;
         d3d10->pass[i].rt.desc.BindFlags = D3D10_BIND_RENDER_TARGET;
         d3d10->pass[i].rt.desc.Format    = glslang_format_to_dxgi(d3d10->pass[i].semantics.format);
         d3d10_init_texture(d3d10->device, &d3d10->pass[i].rt);

         if (pass->feedback)
         {
            d3d10->pass[i].feedback.desc = d3d10->pass[i].rt.desc;
            d3d10_init_texture(d3d10->device, &d3d10->pass[i].feedback);
            /* todo: do we need to clear it to black here ? */
         }
      }
      else
      {
         d3d10->pass[i].rt.size_data.x = width;
         d3d10->pass[i].rt.size_data.y = height;
         d3d10->pass[i].rt.size_data.z = 1.0f / width;
         d3d10->pass[i].rt.size_data.w = 1.0f / height;
      }
   }

   d3d10->resize_render_targets = false;

#if 0
error:
   d3d10_free_shader_preset(d3d10);
   return false;
#endif
}

static bool d3d10_gfx_frame(
      void*               data,
      const void*         frame,
      unsigned            width,
      unsigned            height,
      uint64_t            frame_count,
      unsigned            pitch,
      const char*         msg,
      video_frame_info_t* video_info)
{
   unsigned           i;
   d3d10_texture_t*   texture = NULL;
   d3d10_video_t      * d3d10 = (d3d10_video_t*)data;
   D3D10Device       context  = d3d10->device;

   if (d3d10->resize_chain)
   {
      D3D10Texture2D backBuffer;

      Release(d3d10->renderTargetView);
      DXGIResizeBuffers(d3d10->swapChain, 0, 0, 0, (DXGI_FORMAT)0, 0);

      DXGIGetSwapChainBufferD3D10(d3d10->swapChain, 0, &backBuffer);
      D3D10CreateTexture2DRenderTargetView(
            d3d10->device, backBuffer, NULL, &d3d10->renderTargetView);
      Release(backBuffer);

      D3D10SetRenderTargets(d3d10->device, 1, &d3d10->renderTargetView, NULL);
      d3d10->viewport.Width  = video_info->width;
      d3d10->viewport.Height = video_info->height;

      d3d10->ubo_values.OutputSize.width  = d3d10->viewport.Width;
      d3d10->ubo_values.OutputSize.height = d3d10->viewport.Height;

      d3d10->resize_chain    = false;
      d3d10->resize_viewport = true;

      video_driver_set_size(&video_info->width, &video_info->height);
   }

   PERF_START();

#if 0 /* custom viewport doesn't call apply_state_changes, so we can't rely on this for now */
   if (d3d10->resize_viewport)
#endif
      d3d10_update_viewport(d3d10, false);

   D3D10SetPrimitiveTopology(context, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

#if 0
   if (d3d10->hw.enable)
   {
      D3D10SetRenderTargets(context, 1, &d3d10->renderTargetView, NULL);
      D3D10SetState(context, d3d10->state);
   }
#endif

   if (frame && width && height)
   {
      if (d3d10->shader_preset)
      {
         if (d3d10->frame.texture[0].desc.Width != width ||
               d3d10->frame.texture[0].desc.Height != height)
            d3d10->resize_render_targets = true;

         if (d3d10->resize_render_targets)
         {
            unsigned i;

            /* release all render targets first to avoid memory fragmentation */
            for (i = 0; i < d3d10->shader_preset->passes; i++)
            {
               d3d10_release_texture(&d3d10->pass[i].rt);
               d3d10_release_texture(&d3d10->pass[i].feedback);
               memset(&d3d10->pass[i].rt, 0, sizeof(d3d10->pass[i].rt));
               memset(&d3d10->pass[i].feedback, 0, sizeof(d3d10->pass[i].feedback));
            }
         }

         if (d3d10->shader_preset->history_size)
         {
            if (d3d10->init_history)
               d3d10_init_history(d3d10, width, height);
            else
            {
               int k;
               /* todo: what about frame-duping ?
                * maybe clone d3d10_texture_t with AddRef */
               d3d10_texture_t tmp = d3d10->frame.texture[d3d10->shader_preset->history_size];
               for (k = d3d10->shader_preset->history_size; k > 0; k--)
                  d3d10->frame.texture[k] = d3d10->frame.texture[k - 1];
               d3d10->frame.texture[0] = tmp;
            }
         }
      }

      /* either no history, or we moved a texture of a different size in the front slot */
      if (d3d10->frame.texture[0].desc.Width != width ||
            d3d10->frame.texture[0].desc.Height != height)
      {
         d3d10->frame.texture[0].desc.Width  = width;
         d3d10->frame.texture[0].desc.Height = height;
         d3d10_init_texture(d3d10->device, &d3d10->frame.texture[0]);
      }

      if (d3d10->resize_render_targets)
         d3d10_init_render_targets(d3d10, width, height);

      if (frame != RETRO_HW_FRAME_BUFFER_VALID)
         d3d10_update_texture(
               d3d10->device,
               width, height, pitch, d3d10->format, frame, &d3d10->frame.texture[0]);
   }

   D3D10SetVertexBuffer(context, 0, d3d10->frame.vbo, sizeof(d3d10_vertex_t), 0);
   D3D10SetBlendState(context, d3d10->blend_disable, NULL, D3D10_DEFAULT_SAMPLE_MASK);

   texture = d3d10->frame.texture;

   if (d3d10->shader_preset)
   {
      for (i = 0; i < d3d10->shader_preset->passes; i++)
      {
         if (d3d10->shader_preset->pass[i].feedback)
         {
            d3d10_texture_t tmp     = d3d10->pass[i].feedback;
            d3d10->pass[i].feedback = d3d10->pass[i].rt;
            d3d10->pass[i].rt       = tmp;
         }
      }

      for (i = 0; i < d3d10->shader_preset->passes; i++)
      {
         unsigned j;

         d3d10_set_shader(context, &d3d10->pass[i].shader);

         if (d3d10->shader_preset->pass[i].frame_count_mod)
            d3d10->pass[i].frame_count =
               frame_count % d3d10->shader_preset->pass[i].frame_count_mod;
         else
            d3d10->pass[i].frame_count = frame_count;

         d3d10->pass[i].frame_direction = state_manager_frame_is_reversed() ? -1 : 1;

         for (j = 0; j < SLANG_CBUFFER_MAX; j++)
         {
            D3D10Buffer    buffer     = d3d10->pass[i].buffers[j];
            cbuffer_sem_t* buffer_sem = &d3d10->pass[i].semantics.cbuffers[j];

            if (buffer_sem->stage_mask && buffer_sem->uniforms)
            {
               void*               uniform_data = NULL;
               uniform_sem_t*           uniform = buffer_sem->uniforms;

               D3D10MapBuffer(buffer, D3D10_MAP_WRITE_DISCARD,
                     0, (void**)&uniform_data);

               while (uniform->size)
               {
                  if (uniform->data)
                     memcpy((uint8_t*)uniform_data + uniform->offset,
                           uniform->data, uniform->size);
                  uniform++;
               }
               D3D10UnmapBuffer(buffer);

               if (buffer_sem->stage_mask & SLANG_STAGE_VERTEX_MASK)
                  D3D10SetVShaderConstantBuffers(context, buffer_sem->binding, 1, &buffer);

               if (buffer_sem->stage_mask & SLANG_STAGE_FRAGMENT_MASK)
                  D3D10SetPShaderConstantBuffers(context, buffer_sem->binding, 1, &buffer);
            }
         }

         {
            D3D10RenderTargetView null_rt = NULL;
            D3D10SetRenderTargets(context, 1, &null_rt, NULL);
         }

         {
            D3D10ShaderResourceView textures[SLANG_NUM_BINDINGS] = { NULL };
            D3D10SamplerState       samplers[SLANG_NUM_BINDINGS] = { NULL };

            texture_sem_t* texture_sem = d3d10->pass[i].semantics.textures;
            while (texture_sem->stage_mask)
            {
               int binding       = texture_sem->binding;
               textures[binding] = *(D3D10ShaderResourceView*)texture_sem->texture_data;
               samplers[binding] = d3d10->samplers[texture_sem->filter][texture_sem->wrap];
               texture_sem++;
            }

#if 0
            if (d3d10->hw.enable && (i == 0))
               D3D10SetPShaderResources(context, 1, SLANG_NUM_BINDINGS - 1, textures + 1);
            else
#endif
               D3D10SetPShaderResources(context, 0, SLANG_NUM_BINDINGS, textures);

            D3D10SetPShaderSamplers(context, 0, SLANG_NUM_BINDINGS, samplers);
         }

         if (d3d10->pass[i].rt.handle)
         {
            D3D10SetRenderTargets(context, 1, &d3d10->pass[i].rt.rt_view, NULL);
#if 0
            D3D10ClearRenderTargetView(context, d3d10->pass[i].rt.rt_view, d3d10->clearcolor);
#endif
            D3D10SetViewports(context, 1, &d3d10->pass[i].viewport);

            D3D10Draw(context, 4, 0);
            texture = &d3d10->pass[i].rt;
         }
         else
         {
            texture = NULL;
            break;
         }
      }
      D3D10SetRenderTargets(context, 1, &d3d10->renderTargetView, NULL);
   }

   if (texture)
   {
      d3d10_set_shader(context, &d3d10->shaders[VIDEO_SHADER_STOCK_BLEND]);
#if 0
      /* TODO/FIXME */
      if (!d3d10->hw.enable || d3d10->shader_preset)
#endif
         D3D10SetPShaderResources(context, 0, 1, &texture->view);
      D3D10SetPShaderSamplers(
            context, 0, 1, &d3d10->samplers[RARCH_FILTER_UNSPEC][RARCH_WRAP_DEFAULT]);
      D3D10SetVShaderConstantBuffers(context, 0, 1, &d3d10->frame.ubo);
   }

   D3D10ClearRenderTargetView(context, d3d10->renderTargetView, d3d10->clearcolor);
   D3D10SetViewports(context, 1, &d3d10->frame.viewport);

   d3d10_clear_scissor(d3d10, video_info);

   D3D10Draw(context, 4, 0);

   D3D10SetBlendState(context, d3d10->blend_enable, NULL, D3D10_DEFAULT_SAMPLE_MASK);

   if (d3d10->menu.enabled && d3d10->menu.texture.handle)
   {
      if (d3d10->menu.fullscreen)
         D3D10SetViewports(context, 1, &d3d10->viewport);

      d3d10_set_shader(context, &d3d10->shaders[VIDEO_SHADER_STOCK_BLEND]);
      D3D10SetVertexBuffer(context, 0, d3d10->menu.vbo, sizeof(d3d10_vertex_t), 0);
      D3D10SetVShaderConstantBuffers(context, 0, 1, &d3d10->ubo);
      d3d10_set_texture_and_sampler(context, 0, &d3d10->menu.texture);
      D3D10Draw(context, 4, 0);
   }

   d3d10_set_shader(context, &d3d10->sprites.shader);
   D3D10SetPrimitiveTopology(context, D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
   D3D10SetVShaderConstantBuffer(context, 0, d3d10->ubo);
   D3D10SetPShaderConstantBuffer(context, 0, d3d10->ubo);

   d3d10->sprites.enabled = true;

#ifdef HAVE_MENU
#ifndef HAVE_MENU_WIDGETS
   if (d3d10->menu.enabled)
#endif
   {
      D3D10SetViewports(context, 1, &d3d10->viewport);
      D3D10SetVertexBuffer(context, 0, d3d10->sprites.vbo, sizeof(d3d10_sprite_t), 0);
   }
#endif

#ifdef HAVE_MENU
   if (d3d10->menu.enabled)
      menu_driver_frame(video_info);
   else
#endif
      if (video_info->statistics_show)
      {
         struct font_params* osd_params = (struct font_params*)&video_info->osd_stat_params;

         if (osd_params)
         {
            D3D10SetViewports(context, 1, &d3d10->viewport);
            D3D10SetBlendState(d3d10->device, d3d10->blend_enable, NULL, D3D10_DEFAULT_SAMPLE_MASK);
            D3D10SetVertexBuffer(context, 0, d3d10->sprites.vbo, sizeof(d3d10_sprite_t), 0);
            font_driver_render_msg(d3d10,
                  video_info, video_info->stat_text,
                  (const struct font_params*)&video_info->osd_stat_params, NULL);
         }
      }

#ifdef HAVE_OVERLAY
   if (d3d10->overlays.enabled)
   {
      if (d3d10->overlays.fullscreen)
         D3D10SetViewports(context, 1, &d3d10->viewport);
      else
         D3D10SetViewports(context, 1, &d3d10->frame.viewport);

      D3D10SetBlendState(d3d10->device, d3d10->blend_enable, NULL, D3D10_DEFAULT_SAMPLE_MASK);
      D3D10SetVertexBuffer(context, 0, d3d10->overlays.vbo, sizeof(d3d10_sprite_t), 0);
      D3D10SetPShaderSamplers(
            context, 0, 1, &d3d10->samplers[RARCH_FILTER_UNSPEC][RARCH_WRAP_DEFAULT]);

      for (i = 0; i < (unsigned)d3d10->overlays.count; i++)
      {
         D3D10SetPShaderResources(context, 0, 1, &d3d10->overlays.textures[i].view);
         D3D10Draw(d3d10->device, 1, i);
      }
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
      D3D10SetViewports(d3d10->device, 1, &d3d10->viewport);
      D3D10SetBlendState(d3d10->device, d3d10->blend_enable, NULL, D3D10_DEFAULT_SAMPLE_MASK);
      D3D10SetVertexBuffer(d3d10->device, 0, d3d10->sprites.vbo, sizeof(d3d10_sprite_t), 0);
      font_driver_render_msg(d3d10, video_info, msg, NULL, NULL);
      dxgi_update_title(video_info);
   }
   d3d10->sprites.enabled = false;

   PERF_STOP();
   DXGIPresent(d3d10->swapChain, !!d3d10->vsync, 0);

   return true;
}

static void d3d10_gfx_set_nonblock_state(void* data, bool toggle)
{
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;

   if (!d3d10)
      return;

   d3d10->vsync         = !toggle;
}

static bool d3d10_gfx_alive(void* data)
{
   bool           quit;
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;

   win32_check_window(&quit, &d3d10->resize_chain, &d3d10->vp.full_width, &d3d10->vp.full_height);

   if (d3d10->resize_chain && d3d10->vp.full_width != 0 && d3d10->vp.full_height != 0)
      video_driver_set_size(&d3d10->vp.full_width, &d3d10->vp.full_height);

   return !quit;
}

static bool d3d10_gfx_suppress_screensaver(void* data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static bool d3d10_gfx_has_windowed(void* data)
{
   (void)data;
   return true;
}

static struct video_shader* d3d10_gfx_get_current_shader(void* data)
{
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;

   if (!d3d10)
      return NULL;

   return d3d10->shader_preset;
}

static void d3d10_gfx_viewport_info(void* data, struct video_viewport* vp)
{
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;

   *vp = d3d10->vp;
}

static void d3d10_set_menu_texture_frame(
      void* data, const void* frame, bool rgb32, unsigned width, unsigned height, float alpha)
{
   d3d10_video_t* d3d10    = (d3d10_video_t*)data;
   settings_t*    settings = config_get_ptr();
   DXGI_FORMAT    format   = rgb32 ? DXGI_FORMAT_B8G8R8A8_UNORM :
      (DXGI_FORMAT)DXGI_FORMAT_EX_A4R4G4B4_UNORM;

   if (
         d3d10->menu.texture.desc.Width  != width ||
         d3d10->menu.texture.desc.Height != height)
   {
      d3d10->menu.texture.desc.Format = format;
      d3d10->menu.texture.desc.Width  = width;
      d3d10->menu.texture.desc.Height = height;
      d3d10_init_texture(d3d10->device, &d3d10->menu.texture);
   }

   d3d10_update_texture(d3d10->device, width, height, 0,
         format, frame, &d3d10->menu.texture);
   d3d10->menu.texture.sampler = d3d10->samplers
      [settings->bools.menu_linear_filter
      ? RARCH_FILTER_LINEAR
         : RARCH_FILTER_NEAREST][RARCH_WRAP_DEFAULT];
}
static void d3d10_set_menu_texture_enable(void* data, bool state, bool full_screen)
{
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;

   if (!d3d10)
      return;

   d3d10->menu.enabled    = state;
   d3d10->menu.fullscreen = full_screen;
}

static void d3d10_gfx_set_aspect_ratio(void* data, unsigned aspect_ratio_idx)
{
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;

   if (!d3d10)
      return;

   d3d10->keep_aspect     = true;
   d3d10->resize_viewport = true;
}

static void d3d10_gfx_apply_state_changes(void* data)
{
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;

   if (d3d10)
      d3d10->resize_viewport = true;
}

static void d3d10_gfx_set_osd_msg(
      void* data, video_frame_info_t* video_info, const char* msg, const void* params, void* font)
{
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;

   if (d3d10)
   {
      if (d3d10->sprites.enabled)
         font_driver_render_msg(d3d10, video_info, msg, (const struct font_params*)params, font);
      else
         printf("OSD msg: %s\n", msg);
   }
}

static uintptr_t d3d10_gfx_load_texture(
      void* video_data, void* data, bool threaded, enum texture_filter_type filter_type)
{
   d3d10_texture_t*      texture = NULL;
   d3d10_video_t*        d3d10   = (d3d10_video_t*)video_data;
   struct texture_image* image   = (struct texture_image*)data;

   if (!d3d10)
      return 0;

   texture = (d3d10_texture_t*)calloc(1, sizeof(*texture));

   if (!texture)
      return 0;

   switch (filter_type)
   {
      case TEXTURE_FILTER_MIPMAP_LINEAR:
         texture->desc.MiscFlags = D3D10_RESOURCE_MISC_GENERATE_MIPS;
         /* fallthrough */
      case TEXTURE_FILTER_LINEAR:
         texture->sampler = d3d10->samplers[RARCH_FILTER_LINEAR][RARCH_WRAP_EDGE];
         break;
      case TEXTURE_FILTER_MIPMAP_NEAREST:
         texture->desc.MiscFlags = D3D10_RESOURCE_MISC_GENERATE_MIPS;
         /* fallthrough */
      case TEXTURE_FILTER_NEAREST:
         texture->sampler = d3d10->samplers[RARCH_FILTER_NEAREST][RARCH_WRAP_EDGE];
         break;
   }

   texture->desc.Width  = image->width;
   texture->desc.Height = image->height;
   texture->desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;

   d3d10_init_texture(d3d10->device, texture);

   d3d10_update_texture(
         d3d10->device,
         image->width, image->height, 0, DXGI_FORMAT_B8G8R8A8_UNORM, image->pixels,
         texture);

   return (uintptr_t)texture;
}
static void d3d10_gfx_unload_texture(void* data, uintptr_t handle)
{
   d3d10_texture_t* texture = (d3d10_texture_t*)handle;

   if (!texture)
      return;

   Release(texture->view);
   Release(texture->staging);
   Release(texture->handle);
   free(texture);
}

#if 0
   static bool
d3d10_get_hw_render_interface(void* data, const struct retro_hw_render_interface** iface)
{
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;
   *iface               = (const struct retro_hw_render_interface*)&d3d10->hw.iface;
   return d3d10->hw.enable;
}
#endif

static uint32_t d3d10_get_flags(void *data)
{
   uint32_t flags = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_MENU_FRAME_FILTERING);
#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
#endif

   return flags;
}

static const video_poke_interface_t d3d10_poke_interface = {
   d3d10_get_flags,
   d3d10_gfx_load_texture,
   d3d10_gfx_unload_texture,
   NULL, /* set_video_mode */
#ifndef __WINRT__
   win32_get_refresh_rate,
#else
   /* UWP does not expose this information easily */
   NULL,
#endif
   d3d10_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   d3d10_gfx_set_aspect_ratio,
   d3d10_gfx_apply_state_changes,
   d3d10_set_menu_texture_frame,
   d3d10_set_menu_texture_enable,
   d3d10_gfx_set_osd_msg,
   win32_show_cursor,
   NULL, /* grab_mouse_toggle */
   d3d10_gfx_get_current_shader,
   NULL, /* get_current_software_framebuffer */
#if 0
   d3d10_get_hw_render_interface,
#else
   NULL, /* get_hw_render_interface */
#endif
};

static void d3d10_gfx_get_poke_interface(void* data, const video_poke_interface_t** iface)
{
   *iface = &d3d10_poke_interface;
}

#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
static bool d3d10_menu_widgets_enabled(void *data)
{
   (void)data;
   return true;
}
#endif

video_driver_t video_d3d10 = {
   d3d10_gfx_init,
   d3d10_gfx_frame,
   d3d10_gfx_set_nonblock_state,
   d3d10_gfx_alive,
   win32_has_focus,
   d3d10_gfx_suppress_screensaver,
   d3d10_gfx_has_windowed,
   d3d10_gfx_set_shader,
   d3d10_gfx_free,
   "d3d10",
   NULL, /* set_viewport */
   d3d10_gfx_set_rotation,
   d3d10_gfx_viewport_info,
   NULL, /* read_viewport  */
   NULL, /* read_frame_raw */

#ifdef HAVE_OVERLAY
   d3d10_get_overlay_interface,
#endif
#ifdef HAVE_VIDEO_LAYOUT
   NULL,
#endif
   d3d10_gfx_get_poke_interface,
   NULL, /* d3d10_wrap_type_to_enum */
#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
   d3d10_menu_widgets_enabled
#endif
};
