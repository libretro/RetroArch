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

#include <assert.h>
#include <boolean.h>
#include <string/stdstring.h>
#include <file/file_path.h>
#include <formats/image.h>

#include "../font_driver.h"
#include "../common/d3d_common.h"
#include "../common/win32_common.h"
#include "../common/dxgi_common.h"
#include "../common/d3d12_common.h"
#include "../common/d3dcompiler_common.h"

#include "../../driver.h"
#include "../../verbosity.h"
#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../managers/state_manager.h"

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#ifdef HAVE_MENU_WIDGETS
#include "../../menu/widgets/menu_widgets.h"
#endif
#endif

#include "wiiu/wiiu_dbg.h"

/* Temporary workaround for d3d12 not being able to poll flags during init */
static gfx_ctx_driver_t d3d12_fake_context;
static uint32_t d3d12_get_flags(void *data);

static void d3d12_gfx_sync(d3d12_video_t* d3d12)
{
   if (D3D12GetCompletedValue(d3d12->queue.fence) < d3d12->queue.fenceValue)
   {
      D3D12SetEventOnCompletion(
            d3d12->queue.fence, d3d12->queue.fenceValue, d3d12->queue.fenceEvent);
      WaitForSingleObject(d3d12->queue.fenceEvent, INFINITE);
   }
}

#ifdef HAVE_OVERLAY
static void d3d12_free_overlays(d3d12_video_t* d3d12)
{
   unsigned i;
   for (i = 0; i < (unsigned)d3d12->overlays.count; i++)
      d3d12_release_texture(&d3d12->overlays.textures[i]);

   Release(d3d12->overlays.vbo);
}

static void
d3d12_overlay_vertex_geom(void* data, unsigned index, float x, float y, float w, float h)
{
   d3d12_sprite_t* sprites = NULL;
   D3D12_RANGE     range   = { 0, 0 };
   d3d12_video_t*  d3d12   = (d3d12_video_t*)data;

   if (!d3d12)
      return;

   D3D12Map(d3d12->overlays.vbo, 0, &range, (void**)&sprites);

   sprites[index].pos.x = x;
   sprites[index].pos.y = y;
   sprites[index].pos.w = w;
   sprites[index].pos.h = h;

   range.Begin = index * sizeof(*sprites);
   range.End   = range.Begin + sizeof(*sprites);
   D3D12Unmap(d3d12->overlays.vbo, 0, &range);
}

static void d3d12_overlay_tex_geom(void* data, unsigned index, float u, float v, float w, float h)
{
   d3d12_sprite_t* sprites = NULL;
   D3D12_RANGE     range   = { 0, 0 };
   d3d12_video_t*  d3d12   = (d3d12_video_t*)data;

   if (!d3d12)
      return;

   D3D12Map(d3d12->overlays.vbo, 0, &range, (void**)&sprites);

   sprites[index].coords.u = u;
   sprites[index].coords.v = v;
   sprites[index].coords.w = w;
   sprites[index].coords.h = h;

   range.Begin = index * sizeof(*sprites);
   range.End   = range.Begin + sizeof(*sprites);
   D3D12Unmap(d3d12->overlays.vbo, 0, &range);
}

static void d3d12_overlay_set_alpha(void* data, unsigned index, float mod)
{
   d3d12_sprite_t* sprites = NULL;
   D3D12_RANGE     range   = { 0, 0 };
   d3d12_video_t*  d3d12   = (d3d12_video_t*)data;

   if (!d3d12)
      return;

   D3D12Map(d3d12->overlays.vbo, 0, &range, (void**)&sprites);

   sprites[index].colors[0] = DXGI_COLOR_RGBA(0xFF, 0xFF, 0xFF, mod * 0xFF);
   sprites[index].colors[1] = sprites[index].colors[0];
   sprites[index].colors[2] = sprites[index].colors[0];
   sprites[index].colors[3] = sprites[index].colors[0];

   range.Begin = index * sizeof(*sprites);
   range.End   = range.Begin + sizeof(*sprites);
   D3D12Unmap(d3d12->overlays.vbo, 0, &range);
}

static bool d3d12_overlay_load(void* data, const void* image_data, unsigned num_images)
{
   unsigned                    i;
   d3d12_sprite_t*             sprites = NULL;
   D3D12_RANGE                 range   = { 0, 0 };
   d3d12_video_t*              d3d12   = (d3d12_video_t*)data;
   const struct texture_image* images  = (const struct texture_image*)image_data;

   if (!d3d12)
      return false;

   d3d12_gfx_sync(d3d12);
   d3d12_free_overlays(d3d12);
   d3d12->overlays.count    = num_images;
   d3d12->overlays.textures = (d3d12_texture_t*)calloc(num_images, sizeof(d3d12_texture_t));

   d3d12->overlays.count                   = num_images;
   d3d12->overlays.vbo_view.SizeInBytes    = sizeof(d3d12_sprite_t) * d3d12->overlays.count;
   d3d12->overlays.vbo_view.StrideInBytes  = sizeof(d3d12_sprite_t);
   d3d12->overlays.vbo_view.BufferLocation = d3d12_create_buffer(
         d3d12->device, d3d12->overlays.vbo_view.SizeInBytes, &d3d12->overlays.vbo);

   D3D12Map(d3d12->overlays.vbo, 0, &range, (void**)&sprites);

   for (i = 0; i < num_images; i++)
   {

      d3d12->overlays.textures[i].desc.Width  = images[i].width;
      d3d12->overlays.textures[i].desc.Height = images[i].height;
      d3d12->overlays.textures[i].desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
      d3d12->overlays.textures[i].srv_heap    = &d3d12->desc.srv_heap;
      d3d12_init_texture(d3d12->device, &d3d12->overlays.textures[i]);

      d3d12_update_texture(
            images[i].width, images[i].height, 0, DXGI_FORMAT_B8G8R8A8_UNORM, images[i].pixels,
            &d3d12->overlays.textures[i]);

      sprites[i].pos.x = 0.0f;
      sprites[i].pos.y = 0.0f;
      sprites[i].pos.w = 1.0f;
      sprites[i].pos.h = 1.0f;

      sprites[i].coords.u = 0.0f;
      sprites[i].coords.v = 0.0f;
      sprites[i].coords.w = 1.0f;
      sprites[i].coords.h = 1.0f;

      sprites[i].params.scaling  = 1;
      sprites[i].params.rotation = 0;

      sprites[i].colors[0] = 0xFFFFFFFF;
      sprites[i].colors[1] = sprites[i].colors[0];
      sprites[i].colors[2] = sprites[i].colors[0];
      sprites[i].colors[3] = sprites[i].colors[0];
   }
   D3D12Unmap(d3d12->overlays.vbo, 0, NULL);

   return true;
}

static void d3d12_overlay_enable(void* data, bool state)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   if (!d3d12)
      return;

   d3d12->overlays.enabled = state;
   win32_show_cursor(d3d12, state);
}

static void d3d12_overlay_full_screen(void* data, bool enable)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   if (!d3d12)
      return;

   d3d12->overlays.fullscreen = enable;
}

static void d3d12_get_overlay_interface(void* data, const video_overlay_interface_t** iface)
{
   static const video_overlay_interface_t overlay_interface = {
      d3d12_overlay_enable,      d3d12_overlay_load,        d3d12_overlay_tex_geom,
      d3d12_overlay_vertex_geom, d3d12_overlay_full_screen, d3d12_overlay_set_alpha,
   };

   *iface = &overlay_interface;
}
#endif

static void d3d12_set_filtering(void* data, unsigned index, bool smooth)
{
   int            i;
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   for (i = 0; i < RARCH_WRAP_MAX; i++)
   {
      if (smooth)
         d3d12->samplers[RARCH_FILTER_UNSPEC][i] = d3d12->samplers[RARCH_FILTER_LINEAR][i];
      else
         d3d12->samplers[RARCH_FILTER_UNSPEC][i] = d3d12->samplers[RARCH_FILTER_NEAREST][i];
   }
}

static void d3d12_gfx_set_rotation(void* data, unsigned rotation)
{
   math_matrix_4x4  rot;
   math_matrix_4x4* mvp;
   D3D12_RANGE      read_range = { 0, 0 };
   d3d12_video_t*   d3d12      = (d3d12_video_t*)data;

   if (!d3d12)
      return;

   d3d12_gfx_sync(d3d12);
   d3d12->frame.rotation = rotation;

   matrix_4x4_rotate_z(rot, d3d12->frame.rotation * (M_PI / 2.0f));
   matrix_4x4_multiply(d3d12->mvp, rot, d3d12->mvp_no_rot);

   D3D12Map(d3d12->frame.ubo, 0, &read_range, (void**)&mvp);
   *mvp = d3d12->mvp;
   D3D12Unmap(d3d12->frame.ubo, 0, NULL);
}

static void d3d12_update_viewport(void* data, bool force_full)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   video_driver_update_viewport(&d3d12->vp, force_full, d3d12->keep_aspect);

   d3d12->frame.viewport.TopLeftX = d3d12->vp.x;
   d3d12->frame.viewport.TopLeftY = d3d12->vp.y;
   d3d12->frame.viewport.Width    = d3d12->vp.width;
   d3d12->frame.viewport.Height   = d3d12->vp.height;
   d3d12->frame.viewport.MaxDepth = 0.0f;
   d3d12->frame.viewport.MaxDepth = 1.0f;

   /* having to add vp.x and vp.y here doesn't make any sense */
   d3d12->frame.scissorRect.top    = 0;
   d3d12->frame.scissorRect.left   = 0;
   d3d12->frame.scissorRect.right  = d3d12->vp.x + d3d12->vp.width;
   d3d12->frame.scissorRect.bottom = d3d12->vp.y + d3d12->vp.height;

   if (d3d12->shader_preset && (d3d12->frame.output_size.x != d3d12->vp.width ||
                                d3d12->frame.output_size.y != d3d12->vp.height))
      d3d12->resize_render_targets = true;

   d3d12->frame.output_size.x = d3d12->vp.width;
   d3d12->frame.output_size.y = d3d12->vp.height;
   d3d12->frame.output_size.z = 1.0f / d3d12->vp.width;
   d3d12->frame.output_size.w = 1.0f / d3d12->vp.height;

   d3d12->resize_viewport = false;
}

static void d3d12_free_shader_preset(d3d12_video_t* d3d12)
{
   unsigned i;
   if (!d3d12->shader_preset)
      return;

   for (i = 0; i < d3d12->shader_preset->passes; i++)
   {
      unsigned j;

      free(d3d12->shader_preset->pass[i].source.string.vertex);
      free(d3d12->shader_preset->pass[i].source.string.fragment);
      free(d3d12->pass[i].semantics.textures);
      d3d12_release_texture(&d3d12->pass[i].rt);
      d3d12_release_texture(&d3d12->pass[i].feedback);

      for (j = 0; j < SLANG_CBUFFER_MAX; j++)
      {
         free(d3d12->pass[i].semantics.cbuffers[j].uniforms);
         Release(d3d12->pass[i].buffers[j]);
      }

      Release(d3d12->pass[i].pipe);
   }

   memset(d3d12->pass, 0, sizeof(d3d12->pass));

   /* only free the history textures here */
   for (i = 1; i <= (unsigned)d3d12->shader_preset->history_size; i++)
      d3d12_release_texture(&d3d12->frame.texture[i]);

   memset(
         &d3d12->frame.texture[1], 0,
         sizeof(d3d12->frame.texture[1]) * d3d12->shader_preset->history_size);

   for (i = 0; i < d3d12->shader_preset->luts; i++)
      d3d12_release_texture(&d3d12->luts[i]);

   memset(d3d12->luts, 0, sizeof(d3d12->luts));

   free(d3d12->shader_preset);
   d3d12->shader_preset         = NULL;
   d3d12->init_history          = false;
   d3d12->resize_render_targets = false;
}

static bool d3d12_gfx_set_shader(void* data, enum rarch_shader_type type, const char* path)
{
#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
   unsigned         i;
   config_file_t* conf     = NULL;
   d3d12_texture_t* source = NULL;
   d3d12_video_t*   d3d12  = (d3d12_video_t*)data;

   if (!d3d12)
      return false;

   d3d12_gfx_sync(d3d12);
   d3d12_free_shader_preset(d3d12);

   if (string_is_empty(path))
      return true;

   if (type != RARCH_SHADER_SLANG)
   {
      RARCH_WARN("[D3D12] Only Slang shaders are supported. Falling back to stock.\n");
      return false;
   }

   if (!(conf = video_shader_read_preset(path)))
      return false;

   d3d12->shader_preset = (struct video_shader*)calloc(1, sizeof(*d3d12->shader_preset));

   if (!video_shader_read_conf_preset(conf, d3d12->shader_preset))
      goto error;

   source = &d3d12->frame.texture[0];
   for (i = 0; i < d3d12->shader_preset->passes; source = &d3d12->pass[i++].rt)
   {
      unsigned j;
      /* clang-format off */
      semantics_map_t semantics_map = {
         {
            /* Original */
            { &d3d12->frame.texture[0], 0,
               &d3d12->frame.texture[0].size_data, 0},

            /* Source */
            { source, 0,
               &source->size_data, 0},

            /* OriginalHistory */
            { &d3d12->frame.texture[0], sizeof(*d3d12->frame.texture),
               &d3d12->frame.texture[0].size_data, sizeof(*d3d12->frame.texture)},

            /* PassOutput */
            { &d3d12->pass[0].rt, sizeof(*d3d12->pass),
               &d3d12->pass[0].rt.size_data, sizeof(*d3d12->pass)},

            /* PassFeedback */
            { &d3d12->pass[0].feedback, sizeof(*d3d12->pass),
               &d3d12->pass[0].feedback.size_data, sizeof(*d3d12->pass)},

            /* User */
            { &d3d12->luts[0], sizeof(*d3d12->luts),
               &d3d12->luts[0].size_data, sizeof(*d3d12->luts)},
         },
         {
            &d3d12->mvp,                     /* MVP */
            &d3d12->pass[i].rt.size_data,    /* OutputSize */
            &d3d12->frame.output_size,       /* FinalViewportSize */
            &d3d12->pass[i].frame_count,     /* FrameCount */
            &d3d12->pass[i].frame_direction, /* FrameDirection */
         }
      };
      /* clang-format on */

      if (!slang_process(
               d3d12->shader_preset, i, RARCH_SHADER_HLSL, 50, &semantics_map,
               &d3d12->pass[i].semantics))
         goto error;

      {
         D3DBlob                            vs_code = NULL;
         D3DBlob                            ps_code = NULL;
         D3D12_GRAPHICS_PIPELINE_STATE_DESC desc    = { d3d12->desc.sl_rootSignature };

         static const D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d12_vertex_t, position),
              D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d12_vertex_t, texcoord),
              D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
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
         const char*       slang_path = d3d12->shader_preset->pass[i].source.path;
         const char*       vs_src     = d3d12->shader_preset->pass[i].source.string.vertex;
         const char*       ps_src     = d3d12->shader_preset->pass[i].source.string.fragment;

         strlcpy(vs_path, slang_path, sizeof(vs_path));
         strlcpy(ps_path, slang_path, sizeof(ps_path));
         strlcat(vs_path, vs_ext, sizeof(vs_path));
         strlcat(ps_path, ps_ext, sizeof(ps_path));

         if (!d3d_compile(vs_src, 0, vs_path, "main", "vs_5_0", &vs_code))
            save_hlsl = true;
         if (!d3d_compile(ps_src, 0, ps_path, "main", "ps_5_0", &ps_code))
            save_hlsl = true;

         desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
         if (i == d3d12->shader_preset->passes - 1)
            desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
         else
            desc.RTVFormats[0] = glslang_format_to_dxgi(d3d12->pass[i].semantics.format);

         desc.PrimitiveTopologyType          = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
         desc.InputLayout.pInputElementDescs = inputElementDesc;
         desc.InputLayout.NumElements        = countof(inputElementDesc);

         if (!d3d12_init_pipeline(
                   d3d12->device, vs_code, ps_code, NULL, &desc, &d3d12->pass[i].pipe))
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

         free(d3d12->shader_preset->pass[i].source.string.vertex);
         free(d3d12->shader_preset->pass[i].source.string.fragment);

         d3d12->shader_preset->pass[i].source.string.vertex   = NULL;
         d3d12->shader_preset->pass[i].source.string.fragment = NULL;

         Release(vs_code);
         Release(ps_code);

         if (!d3d12->pass[i].pipe)
            goto error;

         d3d12->pass[i].rt.rt_view.ptr =
               d3d12->desc.rtv_heap.cpu.ptr +
               (countof(d3d12->chain.renderTargets) + i) * d3d12->desc.rtv_heap.stride;

         d3d12->pass[i].textures.ptr =
               d3d12->desc.srv_heap.gpu.ptr + i * SLANG_NUM_SEMANTICS * d3d12->desc.srv_heap.stride;
         d3d12->pass[i].samplers.ptr = d3d12->desc.sampler_heap.gpu.ptr +
                                       i * SLANG_NUM_SEMANTICS * d3d12->desc.sampler_heap.stride;
      }

      for (j = 0; j < SLANG_CBUFFER_MAX; j++)
      {
         if (!d3d12->pass[i].semantics.cbuffers[j].size)
            continue;

         d3d12->pass[i].buffer_view[j].SizeInBytes    = d3d12->pass[i].semantics.cbuffers[j].size;
         d3d12->pass[i].buffer_view[j].BufferLocation = d3d12_create_buffer(
               d3d12->device, d3d12->pass[i].buffer_view[j].SizeInBytes,
               &d3d12->pass[i].buffers[j]);
      }
   }

   for (i = 0; i < d3d12->shader_preset->luts; i++)
   {
      struct texture_image image = { 0 };
      image.supports_rgba        = true;

      if (!image_texture_load(&image, d3d12->shader_preset->lut[i].path))
         goto error;

      d3d12->luts[i].desc.Width  = image.width;
      d3d12->luts[i].desc.Height = image.height;
      d3d12->luts[i].desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      d3d12->luts[i].srv_heap    = &d3d12->desc.srv_heap;

      if (d3d12->shader_preset->lut[i].mipmap)
         d3d12->luts[i].desc.MipLevels = UINT16_MAX;

      d3d12_init_texture(d3d12->device, &d3d12->luts[i]);

      d3d12_update_texture(
            image.width, image.height, 0, DXGI_FORMAT_R8G8B8A8_UNORM, image.pixels,
            &d3d12->luts[i]);

      image_texture_free(&image);
   }

   video_shader_resolve_current_parameters(conf, d3d12->shader_preset);
   config_file_free(conf);

   d3d12->resize_render_targets = true;
   d3d12->init_history          = true;

   return true;

error:
   d3d12_free_shader_preset(d3d12);
#endif
   return false;
}

static bool d3d12_gfx_init_pipelines(d3d12_video_t* d3d12)
{
   D3DBlob                            vs_code = NULL;
   D3DBlob                            ps_code = NULL;
   D3DBlob                            gs_code = NULL;
   D3DBlob                            cs_code = NULL;
   settings_t                  *     settings = config_get_ptr();
   D3D12_GRAPHICS_PIPELINE_STATE_DESC desc    = { d3d12->desc.rootSignature };

   desc.BlendState.RenderTarget[0] = d3d12_blend_enable_desc;
   desc.RTVFormats[0]              = DXGI_FORMAT_R8G8B8A8_UNORM;

   {
      static const char shader[] =
#include "../drivers/d3d_shaders/opaque_sm5.hlsl.h"
            ;

      static const D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
         { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d12_vertex_t, position),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d12_vertex_t, texcoord),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(d3d12_vertex_t, color),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
      };

      if (!d3d_compile(shader, sizeof(shader), NULL, "VSMain", "vs_5_0", &vs_code))
         goto error;
      if (!d3d_compile(shader, sizeof(shader), NULL, "PSMain", "ps_5_0", &ps_code))
         goto error;

      desc.PrimitiveTopologyType          = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
      desc.InputLayout.pInputElementDescs = inputElementDesc;
      desc.InputLayout.NumElements        = countof(inputElementDesc);

      if (!d3d12_init_pipeline(
               d3d12->device, vs_code, ps_code, NULL, &desc,
               &d3d12->pipes[VIDEO_SHADER_STOCK_BLEND]))
         goto error;

      Release(vs_code);
      Release(ps_code);
      vs_code = NULL;
      ps_code = NULL;
   }
   {
      static const char shader[] =
#include "d3d_shaders/sprite_sm4.hlsl.h"
            ;

      D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
         { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(d3d12_sprite_t, pos),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(d3d12_sprite_t, coords),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(d3d12_sprite_t, colors[0]),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "COLOR", 1, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(d3d12_sprite_t, colors[1]),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "COLOR", 2, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(d3d12_sprite_t, colors[2]),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "COLOR", 3, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(d3d12_sprite_t, colors[3]),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "PARAMS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d12_sprite_t, params),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
      };

      if (!d3d_compile(shader, sizeof(shader), NULL, "VSMain", "vs_5_0", &vs_code))
         goto error;
      if (!d3d_compile(shader, sizeof(shader), NULL, "PSMain", "ps_5_0", &ps_code))
         goto error;
      if (!d3d_compile(shader, sizeof(shader), NULL, "GSMain", "gs_5_0", &gs_code))
         goto error;

      desc.BlendState.RenderTarget[0].BlendEnable = false;
      desc.PrimitiveTopologyType                  = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
      desc.InputLayout.pInputElementDescs         = inputElementDesc;
      desc.InputLayout.NumElements                = countof(inputElementDesc);

      if (!d3d12_init_pipeline(
                d3d12->device, vs_code, ps_code, gs_code, &desc, &d3d12->sprites.pipe_noblend))
         goto error;

      desc.BlendState.RenderTarget[0].BlendEnable = true;
      if (!d3d12_init_pipeline(
                d3d12->device, vs_code, ps_code, gs_code, &desc, &d3d12->sprites.pipe_blend))
         goto error;

      Release(ps_code);
      ps_code = NULL;

      if (!d3d_compile(shader, sizeof(shader), NULL, "PSMainA8", "ps_5_0", &ps_code))
         goto error;

      if (!d3d12_init_pipeline(
                d3d12->device, vs_code, ps_code, gs_code, &desc, &d3d12->sprites.pipe_font))
         goto error;

      Release(vs_code);
      Release(ps_code);
      Release(gs_code);
      vs_code = NULL;
      ps_code = NULL;
      gs_code = NULL;
   }

   if (string_is_equal(settings->arrays.menu_driver, "xmb"))
   {
      {
         static const char ribbon[] =
#include "d3d_shaders/ribbon_sm4.hlsl.h"
            ;
         static const char ribbon_simple[] =
#include "d3d_shaders/ribbon_simple_sm4.hlsl.h"
            ;

         D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
               D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         };

         desc.BlendState.RenderTarget[0].SrcBlend  = D3D12_BLEND_ONE;
         desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
         desc.PrimitiveTopologyType                = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
         desc.InputLayout.pInputElementDescs       = inputElementDesc;
         desc.InputLayout.NumElements              = countof(inputElementDesc);

         if (!d3d_compile(ribbon, sizeof(ribbon), NULL, "VSMain", "vs_5_0", &vs_code))
            goto error;
         if (!d3d_compile(ribbon, sizeof(ribbon), NULL, "PSMain", "ps_5_0", &ps_code))
            goto error;

         if (!d3d12_init_pipeline(
                  d3d12->device, vs_code, ps_code, NULL, &desc, &d3d12->pipes[VIDEO_SHADER_MENU]))
            goto error;

         Release(vs_code);
         Release(ps_code);
         vs_code = NULL;
         ps_code = NULL;

         if (!d3d_compile(ribbon_simple, sizeof(ribbon_simple), NULL, "VSMain", "vs_5_0", &vs_code))
            goto error;
         if (!d3d_compile(ribbon_simple, sizeof(ribbon_simple), NULL, "PSMain", "ps_5_0", &ps_code))
            goto error;

         if (!d3d12_init_pipeline(
                  d3d12->device, vs_code, ps_code, NULL, &desc, &d3d12->pipes[VIDEO_SHADER_MENU_2]))
            goto error;

         Release(vs_code);
         Release(ps_code);
         vs_code = NULL;
         ps_code = NULL;
      }

      {
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

         D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d12_vertex_t, position),
               D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d12_vertex_t, texcoord),
               D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         };

         desc.PrimitiveTopologyType          = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
         desc.InputLayout.pInputElementDescs = inputElementDesc;
         desc.InputLayout.NumElements        = countof(inputElementDesc);

         if (!d3d_compile(simple_snow, sizeof(simple_snow), NULL, "VSMain", "vs_5_0", &vs_code))
            goto error;
         if (!d3d_compile(simple_snow, sizeof(simple_snow), NULL, "PSMain", "ps_5_0", &ps_code))
            goto error;

         if (!d3d12_init_pipeline(
                  d3d12->device, vs_code, ps_code, NULL, &desc, &d3d12->pipes[VIDEO_SHADER_MENU_3]))
            goto error;

         Release(vs_code);
         Release(ps_code);
         vs_code = NULL;
         ps_code = NULL;

         if (!d3d_compile(snow, sizeof(snow), NULL, "VSMain", "vs_5_0", &vs_code))
            goto error;
         if (!d3d_compile(snow, sizeof(snow), NULL, "PSMain", "ps_5_0", &ps_code))
            goto error;

         if (!d3d12_init_pipeline(
                  d3d12->device, vs_code, ps_code, NULL, &desc, &d3d12->pipes[VIDEO_SHADER_MENU_4]))
            goto error;

         Release(vs_code);
         Release(ps_code);
         vs_code = NULL;
         ps_code = NULL;

         if (!d3d_compile(bokeh, sizeof(bokeh), NULL, "VSMain", "vs_5_0", &vs_code))
            goto error;
         if (!d3d_compile(bokeh, sizeof(bokeh), NULL, "PSMain", "ps_5_0", &ps_code))
            goto error;

         if (!d3d12_init_pipeline(
                  d3d12->device, vs_code, ps_code, NULL, &desc, &d3d12->pipes[VIDEO_SHADER_MENU_5]))
            goto error;

         Release(vs_code);
         Release(ps_code);
         vs_code = NULL;
         ps_code = NULL;

         if (!d3d_compile(snowflake, sizeof(snowflake), NULL, "VSMain", "vs_5_0", &vs_code))
            goto error;
         if (!d3d_compile(snowflake, sizeof(snowflake), NULL, "PSMain", "ps_5_0", &ps_code))
            goto error;

         if (!d3d12_init_pipeline(
                  d3d12->device, vs_code, ps_code, NULL, &desc, &d3d12->pipes[VIDEO_SHADER_MENU_6]))
            goto error;

         Release(vs_code);
         Release(ps_code);
         vs_code = NULL;
         ps_code = NULL;
      }
   }

   {
      static const char shader[] =
#include "d3d_shaders/mimpapgen_sm5.h"
            ;
      D3D12_COMPUTE_PIPELINE_STATE_DESC desc = { d3d12->desc.cs_rootSignature };
      if (!d3d_compile(shader, sizeof(shader), NULL, "CSMain", "cs_5_0", &cs_code))
         goto error;

      desc.CS.pShaderBytecode = D3DGetBufferPointer(cs_code);
      desc.CS.BytecodeLength  = D3DGetBufferSize(cs_code);
      if (!D3D12CreateComputePipelineState(d3d12->device, &desc, &d3d12->mipmapgen_pipe))

         Release(cs_code);
      cs_code = NULL;
   }

   return true;

error:
   Release(vs_code);
   Release(ps_code);
   Release(gs_code);
   Release(cs_code);
   return false;
}

static void d3d12_gfx_free(void* data)
{
   unsigned       i;
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   if (!d3d12)
      return;

   d3d12_gfx_sync(d3d12);

#ifdef HAVE_OVERLAY
   d3d12_free_overlays(d3d12);
#endif

   d3d12_free_shader_preset(d3d12);

   font_driver_free_osd();

   Release(d3d12->sprites.vbo);
   Release(d3d12->menu_pipeline_vbo);

   Release(d3d12->frame.ubo);
   Release(d3d12->frame.vbo);
   Release(d3d12->frame.texture[0].handle);
   Release(d3d12->frame.texture[0].upload_buffer);
   Release(d3d12->menu.vbo);
   Release(d3d12->menu.texture.handle);
   Release(d3d12->menu.texture.upload_buffer);

   free(d3d12->desc.sampler_heap.map);
   free(d3d12->desc.srv_heap.map);
   free(d3d12->desc.rtv_heap.map);
   Release(d3d12->desc.sampler_heap.handle);
   Release(d3d12->desc.srv_heap.handle);
   Release(d3d12->desc.rtv_heap.handle);

   Release(d3d12->desc.cs_rootSignature);
   Release(d3d12->desc.sl_rootSignature);
   Release(d3d12->desc.rootSignature);

   Release(d3d12->ubo);

   for (i = 0; i < GFX_MAX_SHADERS; i++)
      Release(d3d12->pipes[i]);

   Release(d3d12->mipmapgen_pipe);
   Release(d3d12->sprites.pipe_blend);
   Release(d3d12->sprites.pipe_noblend);
   Release(d3d12->sprites.pipe_font);

   Release(d3d12->queue.fence);
   Release(d3d12->chain.renderTargets[0]);
   Release(d3d12->chain.renderTargets[1]);
   Release(d3d12->chain.handle);

   Release(d3d12->queue.cmd);
   Release(d3d12->queue.allocator);
   Release(d3d12->queue.handle);

   Release(d3d12->factory);
   Release(d3d12->device);
   Release(d3d12->adapter);

   for (i = 0; i < D3D12_MAX_GPU_COUNT; i++)
   {
      if (d3d12->adapters[i])
      {
         Release(d3d12->adapters[i]);
         d3d12->adapters[i] = NULL;
      }
   }

#ifdef HAVE_MONITOR
   win32_monitor_from_window();
#endif
#ifdef HAVE_WINDOW
   win32_destroy_window();
#endif

   free(d3d12);
}

static void *d3d12_gfx_init(const video_info_t* video, 
      input_driver_t** input, void** input_data)
{
#ifdef HAVE_MONITOR
   MONITORINFOEX  current_mon;
   HMONITOR       hm_to_use;
   WNDCLASSEX     wndclass = { 0 };
#endif
   settings_t*    settings = config_get_ptr();
   d3d12_video_t* d3d12    = (d3d12_video_t*)calloc(1, sizeof(*d3d12));

   if (!d3d12)
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

   win32_monitor_info(&current_mon, &hm_to_use, &d3d12->cur_mon_id);
#endif

   d3d12->vp.full_width  = video->width;
   d3d12->vp.full_height = video->height;

#ifdef HAVE_MONITOR
   if (!d3d12->vp.full_width)
      d3d12->vp.full_width = current_mon.rcMonitor.right - current_mon.rcMonitor.left;
   if (!d3d12->vp.full_height)
      d3d12->vp.full_height = current_mon.rcMonitor.bottom - current_mon.rcMonitor.top;
#endif

   if (!win32_set_video_mode(d3d12, d3d12->vp.full_width, d3d12->vp.full_height, video->fullscreen))
   {
      RARCH_ERR("[D3D12]: win32_set_video_mode failed.\n");
      goto error;
   }

   d3d_input_driver(settings->arrays.input_driver, settings->arrays.input_joypad_driver, input, input_data);

   if (!d3d12_init_base(d3d12))
      goto error;

   if (!d3d12_init_descriptors(d3d12))
      goto error;

   if (!d3d12_gfx_init_pipelines(d3d12))
      goto error;

   if (!d3d12_init_queue(d3d12))
      goto error;

#ifdef __WINRT__
   if (!d3d12_init_swapchain(d3d12, d3d12->vp.full_width, d3d12->vp.full_height, uwp_get_corewindow()))
      goto error;
#else
   if (!d3d12_init_swapchain(d3d12, d3d12->vp.full_width, d3d12->vp.full_height, main_window.hwnd))
      goto error;
#endif

   d3d12_init_samplers(d3d12);
   d3d12_set_filtering(d3d12, 0, video->smooth);

   d3d12_create_fullscreen_quad_vbo(d3d12->device, &d3d12->frame.vbo_view, &d3d12->frame.vbo);
   d3d12_create_fullscreen_quad_vbo(d3d12->device, &d3d12->menu.vbo_view, &d3d12->menu.vbo);

   d3d12->sprites.capacity                = 4096;
   d3d12->sprites.vbo_view.SizeInBytes    = sizeof(d3d12_sprite_t) * d3d12->sprites.capacity;
   d3d12->sprites.vbo_view.StrideInBytes  = sizeof(d3d12_sprite_t);
   d3d12->sprites.vbo_view.BufferLocation = d3d12_create_buffer(
         d3d12->device, d3d12->sprites.vbo_view.SizeInBytes, &d3d12->sprites.vbo);

   d3d12->ubo_view.SizeInBytes = sizeof(d3d12_uniform_t);
   d3d12->ubo_view.BufferLocation =
         d3d12_create_buffer(d3d12->device, d3d12->ubo_view.SizeInBytes, &d3d12->ubo);

   d3d12->frame.ubo_view.SizeInBytes = sizeof(d3d12_uniform_t);
   d3d12->frame.ubo_view.BufferLocation =
         d3d12_create_buffer(d3d12->device, d3d12->frame.ubo_view.SizeInBytes, &d3d12->frame.ubo);

   matrix_4x4_ortho(d3d12->mvp_no_rot, 0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);

   d3d12->ubo_values.mvp               = d3d12->mvp_no_rot;
   d3d12->ubo_values.OutputSize.width  = d3d12->chain.viewport.Width;
   d3d12->ubo_values.OutputSize.height = d3d12->chain.viewport.Height;

   {
      math_matrix_4x4* mvp;
      D3D12_RANGE      read_range = { 0, 0 };
      D3D12Map(d3d12->ubo, 0, &read_range, (void**)&mvp);
      *mvp = d3d12->mvp_no_rot;
      D3D12Unmap(d3d12->ubo, 0, NULL);
   }

   d3d12_gfx_set_rotation(d3d12, 0);
   video_driver_set_size(&d3d12->vp.full_width, &d3d12->vp.full_height);
   d3d12->chain.viewport.Width  = d3d12->vp.full_width;
   d3d12->chain.viewport.Height = d3d12->vp.full_height;
   d3d12->resize_viewport       = true;
   d3d12->keep_aspect           = video->force_aspect;
   d3d12->chain.vsync           = video->vsync;
   d3d12->format = video->rgb32 ? DXGI_FORMAT_B8G8R8X8_UNORM : DXGI_FORMAT_B5G6R5_UNORM;
   d3d12->frame.texture[0].desc.Format = d3d12->format;
   d3d12->frame.texture[0].desc.Width  = 4;
   d3d12->frame.texture[0].desc.Height = 4;
   d3d12->frame.texture[0].srv_heap    = &d3d12->desc.srv_heap;
   d3d12_init_texture(d3d12->device, &d3d12->frame.texture[0]);

   font_driver_init_osd(d3d12, false, video->is_threaded, FONT_DRIVER_RENDER_D3D12_API);

   {
      d3d12_fake_context.get_flags = d3d12_get_flags;
      d3d12_fake_context.get_metrics = win32_get_metrics;
      video_context_driver_set(&d3d12_fake_context); 
      const char *shader_preset   = retroarch_get_shader_preset();
      enum rarch_shader_type type = video_shader_parse_type(shader_preset);
      d3d12_gfx_set_shader(d3d12, type, shader_preset);
   }

   return d3d12;

error:
   RARCH_ERR("[D3D12]: failed to init video driver.\n");
   d3d12_gfx_free(d3d12);
   return NULL;
}

static void d3d12_init_history(d3d12_video_t* d3d12, unsigned width, unsigned height)
{
   unsigned i;

   /* todo: should we init history to max_width/max_height instead ?
    * to prevent out of memory errors happening several frames later
    * and to reduce memory fragmentation */

   assert(d3d12->shader_preset);
   for (i = 0; i < (unsigned)d3d12->shader_preset->history_size + 1; i++)
   {
      d3d12->frame.texture[i].desc.Width     = width;
      d3d12->frame.texture[i].desc.Height    = height;
      d3d12->frame.texture[i].desc.Format    = d3d12->frame.texture[0].desc.Format;
      d3d12->frame.texture[i].desc.MipLevels = d3d12->frame.texture[0].desc.MipLevels;
      d3d12->frame.texture[i].srv_heap       = &d3d12->desc.srv_heap;
      d3d12_init_texture(d3d12->device, &d3d12->frame.texture[i]);
      /* todo: clear texture ?  */
   }
   d3d12->init_history = false;
}
static void d3d12_init_render_targets(d3d12_video_t* d3d12, unsigned width, unsigned height)
{
   unsigned i;

   assert(d3d12->shader_preset);

   for (i = 0; i < d3d12->shader_preset->passes; i++)
   {
      struct video_shader_pass* pass = &d3d12->shader_preset->pass[i];

      if (pass->fbo.valid)
      {

         switch (pass->fbo.type_x)
         {
            case RARCH_SCALE_INPUT:
               width *= pass->fbo.scale_x;
               break;

            case RARCH_SCALE_VIEWPORT:
               width = d3d12->vp.width * pass->fbo.scale_x;
               break;

            case RARCH_SCALE_ABSOLUTE:
               width = pass->fbo.abs_x;
               break;

            default:
               break;
         }

         if (!width)
            width = d3d12->vp.width;

         switch (pass->fbo.type_y)
         {
            case RARCH_SCALE_INPUT:
               height *= pass->fbo.scale_y;
               break;

            case RARCH_SCALE_VIEWPORT:
               height = d3d12->vp.height * pass->fbo.scale_y;
               break;

            case RARCH_SCALE_ABSOLUTE:
               height = pass->fbo.abs_y;
               break;

            default:
               break;
         }

         if (!height)
            height = d3d12->vp.height;
      }
      else if (i == (d3d12->shader_preset->passes - 1))
      {
         width  = d3d12->vp.width;
         height = d3d12->vp.height;
      }

      RARCH_LOG("[d3d12]: Updating framebuffer size %u x %u.\n", width, height);

      if ((i != (d3d12->shader_preset->passes - 1)) || (width != d3d12->vp.width) ||
          (height != d3d12->vp.height))
      {
         d3d12->pass[i].viewport.Width     = width;
         d3d12->pass[i].viewport.Height    = height;
         d3d12->pass[i].viewport.MaxDepth  = 1.0;
         d3d12->pass[i].scissorRect.right  = width;
         d3d12->pass[i].scissorRect.bottom = height;
         d3d12->pass[i].rt.desc.Width      = width;
         d3d12->pass[i].rt.desc.Height     = height;
         d3d12->pass[i].rt.desc.Flags      = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
         d3d12->pass[i].rt.srv_heap        = &d3d12->desc.srv_heap;
         d3d12->pass[i].rt.desc.Format = glslang_format_to_dxgi(d3d12->pass[i].semantics.format);
         d3d12_init_texture(d3d12->device, &d3d12->pass[i].rt);

         if (pass->feedback)
         {
            d3d12->pass[i].feedback.desc     = d3d12->pass[i].rt.desc;
            d3d12->pass[i].feedback.srv_heap = &d3d12->desc.srv_heap;
            d3d12_init_texture(d3d12->device, &d3d12->pass[i].feedback);
            /* todo: do we need to clear it to black here ? */
         }
      }
      else
      {
         d3d12->pass[i].rt.size_data.x = width;
         d3d12->pass[i].rt.size_data.y = height;
         d3d12->pass[i].rt.size_data.z = 1.0f / width;
         d3d12->pass[i].rt.size_data.w = 1.0f / height;
      }

      d3d12->pass[i].sampler = d3d12->samplers[pass->filter][pass->wrap];
   }

   d3d12->resize_render_targets = false;

#if 0
error:
   d3d12_free_shader_preset(d3d12);
   return false;
#endif
}

static bool d3d12_gfx_frame(
      void*               data,
      const void*         frame,
      unsigned            width,
      unsigned            height,
      uint64_t            frame_count,
      unsigned            pitch,
      const char*         msg,
      video_frame_info_t* video_info)
{
   unsigned         i;
   d3d12_texture_t* texture = NULL;
   d3d12_video_t*   d3d12   = (d3d12_video_t*)data;

   d3d12_gfx_sync(d3d12);
   PERF_START();

   if (d3d12->resize_chain)
   {
      unsigned i;

      for (i = 0; i < countof(d3d12->chain.renderTargets); i++)
         Release(d3d12->chain.renderTargets[i]);

      DXGIResizeBuffers(d3d12->chain.handle, 0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);

      for (i = 0; i < countof(d3d12->chain.renderTargets); i++)
      {
         DXGIGetSwapChainBuffer(d3d12->chain.handle, i, &d3d12->chain.renderTargets[i]);
         D3D12CreateRenderTargetView(
               d3d12->device, d3d12->chain.renderTargets[i], NULL, d3d12->chain.desc_handles[i]);
      }

      d3d12->chain.viewport.Width     = video_info->width;
      d3d12->chain.viewport.Height    = video_info->height;
      d3d12->chain.scissorRect.right  = video_info->width;
      d3d12->chain.scissorRect.bottom = video_info->height;
      d3d12->resize_chain             = false;
      d3d12->resize_viewport          = true;

      d3d12->ubo_values.OutputSize.width  = d3d12->chain.viewport.Width;
      d3d12->ubo_values.OutputSize.height = d3d12->chain.viewport.Height;

      video_driver_set_size(&video_info->width, &video_info->height);
   }

   D3D12ResetCommandAllocator(d3d12->queue.allocator);

   D3D12ResetGraphicsCommandList(
         d3d12->queue.cmd, d3d12->queue.allocator, d3d12->pipes[VIDEO_SHADER_STOCK_BLEND]);

   {
      D3D12DescriptorHeap desc_heaps[] = { d3d12->desc.srv_heap.handle,
                                           d3d12->desc.sampler_heap.handle };
      D3D12SetDescriptorHeaps(d3d12->queue.cmd, countof(desc_heaps), desc_heaps);
   }

#if 0 /* custom viewport doesn't call apply_state_changes, so we can't rely on this for now */
   if (d3d12->resize_viewport)
#endif
   d3d12_update_viewport(d3d12, false);

   D3D12IASetPrimitiveTopology(d3d12->queue.cmd, D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

   if (frame && width && height)
   {
      if (d3d12->shader_preset)
      {
         if (d3d12->shader_preset->luts && d3d12->luts[0].dirty)
            for (i = 0; i < d3d12->shader_preset->luts; i++)
               d3d12_upload_texture(d3d12->queue.cmd, &d3d12->luts[i],
                     video_info->userdata);

         if (d3d12->frame.texture[0].desc.Width != width ||
             d3d12->frame.texture[0].desc.Height != height)
            d3d12->resize_render_targets = true;

         if (d3d12->resize_render_targets)
         {
            /* release all render targets first to avoid memory fragmentation */
            for (i = 0; i < d3d12->shader_preset->passes; i++)
            {
               d3d12_release_texture(&d3d12->pass[i].rt);
               d3d12->pass[i].rt.handle = NULL;
               d3d12_release_texture(&d3d12->pass[i].feedback);
               d3d12->pass[i].feedback.handle = NULL;
            }
         }

         if (d3d12->shader_preset->history_size)
         {
            if (d3d12->init_history)
               d3d12_init_history(d3d12, width, height);
            else
            {
               int k;
               /* todo: what about frame-duping ?
                * maybe clone d3d12_texture_t with AddRef */
               d3d12_texture_t tmp = d3d12->frame.texture[d3d12->shader_preset->history_size];
               for (k = d3d12->shader_preset->history_size; k > 0; k--)
                  d3d12->frame.texture[k] = d3d12->frame.texture[k - 1];
               d3d12->frame.texture[0] = tmp;
            }
         }
      }

      /* either no history, or we moved a texture of a different size in the front slot */
      if (d3d12->frame.texture[0].desc.Width != width ||
          d3d12->frame.texture[0].desc.Height != height)
      {
         d3d12->frame.texture[0].desc.Width  = width;
         d3d12->frame.texture[0].desc.Height = height;
         d3d12->frame.texture[0].srv_heap    = &d3d12->desc.srv_heap;
         d3d12_init_texture(d3d12->device, &d3d12->frame.texture[0]);
      }

      if (d3d12->resize_render_targets)
         d3d12_init_render_targets(d3d12, width, height);

      d3d12_update_texture(width, height, pitch, d3d12->format, frame, &d3d12->frame.texture[0]);

      d3d12_upload_texture(d3d12->queue.cmd, &d3d12->frame.texture[0],
            video_info->userdata);
   }
   D3D12IASetVertexBuffers(d3d12->queue.cmd, 0, 1, &d3d12->frame.vbo_view);

   texture = d3d12->frame.texture;

   if (d3d12->shader_preset)
   {
      D3D12SetGraphicsRootSignature(d3d12->queue.cmd, d3d12->desc.sl_rootSignature);

      for (i = 0; i < d3d12->shader_preset->passes; i++)
      {
         if (d3d12->shader_preset->pass[i].feedback)
         {
            d3d12_texture_t tmp     = d3d12->pass[i].feedback;
            d3d12->pass[i].feedback = d3d12->pass[i].rt;
            d3d12->pass[i].rt       = tmp;
         }
      }

      for (i = 0; i < d3d12->shader_preset->passes; i++)
      {
         unsigned j;

         D3D12SetPipelineState(d3d12->queue.cmd, d3d12->pass[i].pipe);

         if (d3d12->shader_preset->pass[i].frame_count_mod)
            d3d12->pass[i].frame_count =
                  frame_count % d3d12->shader_preset->pass[i].frame_count_mod;
         else
            d3d12->pass[i].frame_count = frame_count;

         d3d12->pass[i].frame_direction = state_manager_frame_is_reversed() ? -1 : 1;

         for (j = 0; j < SLANG_CBUFFER_MAX; j++)
         {
            cbuffer_sem_t* buffer_sem = &d3d12->pass[i].semantics.cbuffers[j];

            if (buffer_sem->stage_mask && buffer_sem->uniforms)
            {
               D3D12_RANGE    range       = { 0, 0 };
               uint8_t*       mapped_data = NULL;
               uniform_sem_t* uniform     = buffer_sem->uniforms;

               D3D12Map(d3d12->pass[i].buffers[j], 0, &range, (void**)&mapped_data);
               while (uniform->size)
               {
                  if (uniform->data)
                     memcpy(mapped_data + uniform->offset, uniform->data, uniform->size);
                  uniform++;
               }
               D3D12Unmap(d3d12->pass[i].buffers[j], 0, NULL);

               D3D12SetGraphicsRootConstantBufferView(
                     d3d12->queue.cmd, j == SLANG_CBUFFER_UBO ? ROOT_ID_UBO : ROOT_ID_PC,
                     d3d12->pass[i].buffer_view[j].BufferLocation);
            }
         }
#if 0
         D3D12OMSetRenderTargets(d3d12->queue.cmd, 1, NULL, FALSE, NULL);
#endif

         {
            texture_sem_t* texture_sem = d3d12->pass[i].semantics.textures;
            while (texture_sem->stage_mask)
            {
               {
                  D3D12_CPU_DESCRIPTOR_HANDLE handle   = {
                     d3d12->pass[i].textures.ptr - d3d12->desc.srv_heap.gpu.ptr +
                        d3d12->desc.srv_heap.cpu.ptr +
                        texture_sem->binding * d3d12->desc.srv_heap.stride
                  };
                  d3d12_texture_t*                tex  = (d3d12_texture_t*)texture_sem->texture_data;
                  D3D12_SHADER_RESOURCE_VIEW_DESC desc = { tex->desc.Format };

                  desc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                  desc.ViewDimension                   = D3D12_SRV_DIMENSION_TEXTURE2D;
                  desc.Texture2D.MipLevels             = tex->desc.MipLevels;

                  D3D12CreateShaderResourceView(d3d12->device,
                        tex->handle, &desc, handle);
               }

               {
                  D3D12_CPU_DESCRIPTOR_HANDLE handle = {
                     d3d12->pass[i].samplers.ptr - d3d12->desc.sampler_heap.gpu.ptr +
                     d3d12->desc.sampler_heap.cpu.ptr +
                     texture_sem->binding * d3d12->desc.sampler_heap.stride
                  };
                  D3D12_SAMPLER_DESC desc = { D3D12_FILTER_MIN_MAG_MIP_LINEAR };

                  if (texture_sem->filter == RARCH_FILTER_NEAREST)
                     desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;

                  switch (texture_sem->wrap)
                  {
                     default:
                     case RARCH_WRAP_BORDER:
                        desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
                        break;

                     case RARCH_WRAP_EDGE:
                        desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                        break;

                     case RARCH_WRAP_REPEAT:
                        desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
                        break;

                     case RARCH_WRAP_MIRRORED_REPEAT:
                        desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
                        break;
                  }

                  desc.AddressV       = desc.AddressU;
                  desc.AddressW       = desc.AddressU;
                  desc.MaxAnisotropy  = 1;
                  desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
                  desc.MinLOD         = -D3D12_FLOAT32_MAX;
                  desc.MaxLOD         = D3D12_FLOAT32_MAX;

                  D3D12CreateSampler(d3d12->device, &desc, handle);
               }

               texture_sem++;
            }

            D3D12SetGraphicsRootDescriptorTable(
                  d3d12->queue.cmd, ROOT_ID_TEXTURE_T, d3d12->pass[i].textures);
            D3D12SetGraphicsRootDescriptorTable(
                  d3d12->queue.cmd, ROOT_ID_SAMPLER_T, d3d12->pass[i].samplers);
         }

         if (d3d12->pass[i].rt.handle)
         {
            d3d12_resource_transition(
                  d3d12->queue.cmd, d3d12->pass[i].rt.handle,
                  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

            D3D12OMSetRenderTargets(d3d12->queue.cmd, 1, &d3d12->pass[i].rt.rt_view, FALSE, NULL);
#if 0
            D3D12ClearRenderTargetView(
                  d3d12->queue.cmd, d3d12->pass[i].rt.rt_view, d3d12->chain.clearcolor, 0, NULL);
#endif
            D3D12RSSetViewports(d3d12->queue.cmd, 1, &d3d12->pass[i].viewport);
            D3D12RSSetScissorRects(d3d12->queue.cmd, 1, &d3d12->pass[i].scissorRect);

            D3D12DrawInstanced(d3d12->queue.cmd, 4, 1, 0, 0);

            d3d12_resource_transition(
                  d3d12->queue.cmd, d3d12->pass[i].rt.handle, D3D12_RESOURCE_STATE_RENDER_TARGET,
                  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            texture = &d3d12->pass[i].rt;
         }
         else
         {
            texture = NULL;
            break;
         }
      }
   }

   if (texture)
   {
      D3D12SetPipelineState(d3d12->queue.cmd, d3d12->pipes[VIDEO_SHADER_STOCK_BLEND]);
      D3D12SetGraphicsRootSignature(d3d12->queue.cmd, d3d12->desc.rootSignature);
      d3d12_set_texture(d3d12->queue.cmd, &d3d12->frame.texture[0]);
      d3d12_set_sampler(d3d12->queue.cmd, d3d12->samplers[RARCH_FILTER_UNSPEC][RARCH_WRAP_DEFAULT]);
      D3D12SetGraphicsRootConstantBufferView(
            d3d12->queue.cmd, ROOT_ID_UBO, d3d12->frame.ubo_view.BufferLocation);
   }

   d3d12->chain.frame_index = DXGIGetCurrentBackBufferIndex(d3d12->chain.handle);
   d3d12_resource_transition(
         d3d12->queue.cmd, d3d12->chain.renderTargets[d3d12->chain.frame_index],
         D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

   D3D12OMSetRenderTargets(
         d3d12->queue.cmd, 1, &d3d12->chain.desc_handles[d3d12->chain.frame_index], FALSE, NULL);
   D3D12ClearRenderTargetView(
         d3d12->queue.cmd, d3d12->chain.desc_handles[d3d12->chain.frame_index],
         d3d12->chain.clearcolor, 0, NULL);

   D3D12RSSetViewports(d3d12->queue.cmd, 1, &d3d12->frame.viewport);
   D3D12RSSetScissorRects(d3d12->queue.cmd, 1, &d3d12->frame.scissorRect);

   D3D12DrawInstanced(d3d12->queue.cmd, 4, 1, 0, 0);

   D3D12SetPipelineState(d3d12->queue.cmd, d3d12->pipes[VIDEO_SHADER_STOCK_BLEND]);
   D3D12SetGraphicsRootSignature(d3d12->queue.cmd, d3d12->desc.rootSignature);

   if (d3d12->menu.enabled && d3d12->menu.texture.handle)
   {
      if (d3d12->menu.texture.dirty)
         d3d12_upload_texture(d3d12->queue.cmd, &d3d12->menu.texture,
               video_info->userdata);

      D3D12SetGraphicsRootConstantBufferView(
            d3d12->queue.cmd, ROOT_ID_UBO, d3d12->ubo_view.BufferLocation);

      if (d3d12->menu.fullscreen)
      {
         D3D12RSSetViewports(d3d12->queue.cmd, 1, &d3d12->chain.viewport);
         D3D12RSSetScissorRects(d3d12->queue.cmd, 1, &d3d12->chain.scissorRect);
      }

      d3d12_set_texture_and_sampler(d3d12->queue.cmd, &d3d12->menu.texture);
      D3D12IASetVertexBuffers(d3d12->queue.cmd, 0, 1, &d3d12->menu.vbo_view);
      D3D12DrawInstanced(d3d12->queue.cmd, 4, 1, 0, 0);
   }

   d3d12->sprites.pipe = d3d12->sprites.pipe_noblend;
   D3D12SetPipelineState(d3d12->queue.cmd, d3d12->sprites.pipe);
   D3D12IASetPrimitiveTopology(d3d12->queue.cmd, D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

   d3d12->sprites.enabled = true;

#ifdef HAVE_MENU
#ifndef HAVE_MENU_WIDGETS
   if (d3d12->menu.enabled)
#endif
   {
      D3D12RSSetViewports(d3d12->queue.cmd, 1, &d3d12->chain.viewport);
      D3D12RSSetScissorRects(d3d12->queue.cmd, 1, &d3d12->chain.scissorRect);
      D3D12IASetVertexBuffers(d3d12->queue.cmd, 0, 1, &d3d12->sprites.vbo_view);
   }
#endif

#ifdef HAVE_MENU
   if (d3d12->menu.enabled)
      menu_driver_frame(video_info);
   else
#endif
      if (video_info->statistics_show)
   {
      struct font_params *osd_params = (struct font_params*)
         &video_info->osd_stat_params;

      if (osd_params)
      {
         D3D12SetPipelineState(d3d12->queue.cmd, d3d12->sprites.pipe_blend);
         D3D12RSSetViewports(d3d12->queue.cmd, 1, &d3d12->chain.viewport);
         D3D12RSSetScissorRects(d3d12->queue.cmd, 1, &d3d12->chain.scissorRect);
         D3D12IASetVertexBuffers(d3d12->queue.cmd, 0, 1, &d3d12->sprites.vbo_view);
         font_driver_render_msg(d3d12, video_info, video_info->stat_text,
               (const struct font_params*)&video_info->osd_stat_params, NULL);
      }
   }
#ifdef HAVE_OVERLAY
   if (d3d12->overlays.enabled)
   {
      if (d3d12->overlays.fullscreen)
      {
         D3D12RSSetViewports(d3d12->queue.cmd, 1, &d3d12->chain.viewport);
         D3D12RSSetScissorRects(d3d12->queue.cmd, 1, &d3d12->chain.scissorRect);
      }
      else
      {
         D3D12RSSetViewports(d3d12->queue.cmd, 1, &d3d12->frame.viewport);
         D3D12RSSetScissorRects(d3d12->queue.cmd, 1, &d3d12->frame.scissorRect);
      }

      D3D12IASetVertexBuffers(d3d12->queue.cmd, 0, 1, &d3d12->overlays.vbo_view);

      D3D12SetPipelineState(d3d12->queue.cmd, d3d12->sprites.pipe_blend);

      D3D12SetGraphicsRootDescriptorTable(
            d3d12->queue.cmd, ROOT_ID_SAMPLER_T,
            d3d12->samplers[RARCH_FILTER_UNSPEC][RARCH_WRAP_DEFAULT]);

      for (i = 0; i < (unsigned)d3d12->overlays.count; i++)
      {
         if (d3d12->overlays.textures[i].dirty)
            d3d12_upload_texture(d3d12->queue.cmd,
                  &d3d12->overlays.textures[i],
                  video_info->userdata);

         D3D12SetGraphicsRootDescriptorTable(
               d3d12->queue.cmd, ROOT_ID_TEXTURE_T, d3d12->overlays.textures[i].gpu_descriptor[0]);
         D3D12DrawInstanced(d3d12->queue.cmd, 1, 1, i, 0);
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
      D3D12SetPipelineState(d3d12->queue.cmd, d3d12->sprites.pipe_blend);
      D3D12RSSetViewports(d3d12->queue.cmd, 1, &d3d12->chain.viewport);
      D3D12RSSetScissorRects(d3d12->queue.cmd, 1, &d3d12->chain.scissorRect);
      D3D12IASetVertexBuffers(d3d12->queue.cmd, 0, 1, &d3d12->sprites.vbo_view);

      font_driver_render_msg(d3d12, video_info, msg, NULL, NULL);
      dxgi_update_title(video_info);
   }
   d3d12->sprites.enabled = false;

   d3d12_resource_transition(
         d3d12->queue.cmd, d3d12->chain.renderTargets[d3d12->chain.frame_index],
         D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
   D3D12CloseGraphicsCommandList(d3d12->queue.cmd);

   D3D12ExecuteGraphicsCommandLists(d3d12->queue.handle, 1, &d3d12->queue.cmd);
   D3D12SignalCommandQueue(d3d12->queue.handle, d3d12->queue.fence, ++d3d12->queue.fenceValue);

   PERF_STOP();
#if 1
   DXGIPresent(d3d12->chain.handle, !!d3d12->chain.vsync, 0);
#else
   DXGI_PRESENT_PARAMETERS pp = { 0 };
   DXGIPresent1(d3d12->swapchain, 0, 0, &pp);
#endif

   return true;
}

static void d3d12_gfx_set_nonblock_state(void* data, bool toggle)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;
   d3d12->chain.vsync   = !toggle;
}

static bool d3d12_gfx_alive(void* data)
{
   bool           quit;
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   win32_check_window(&quit, &d3d12->resize_chain, &d3d12->vp.full_width, &d3d12->vp.full_height);

   if (d3d12->resize_chain && d3d12->vp.full_width != 0 && d3d12->vp.full_height != 0)
      video_driver_set_size(&d3d12->vp.full_width, &d3d12->vp.full_height);

   return !quit;
}

static bool d3d12_gfx_suppress_screensaver(void* data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static bool d3d12_gfx_has_windowed(void* data)
{
   (void)data;
   return true;
}

static struct video_shader* d3d12_gfx_get_current_shader(void* data)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   if (!d3d12)
      return NULL;

   return d3d12->shader_preset;
}

static void d3d12_gfx_viewport_info(void* data, struct video_viewport* vp)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   *vp = d3d12->vp;
}

static void d3d12_set_menu_texture_frame(
      void* data, const void* frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   d3d12_video_t* d3d12    = (d3d12_video_t*)data;
   settings_t*    settings = config_get_ptr();
   int            pitch    = width *
      (rgb32 ? sizeof(uint32_t) : sizeof(uint16_t));
   DXGI_FORMAT    format   = rgb32 ? DXGI_FORMAT_B8G8R8A8_UNORM
      : (DXGI_FORMAT)DXGI_FORMAT_EX_A4R4G4B4_UNORM;

   if (
         d3d12->menu.texture.desc.Width  != width ||
         d3d12->menu.texture.desc.Height != height)
   {
      d3d12->menu.texture.desc.Width  = width;
      d3d12->menu.texture.desc.Height = height;
      d3d12->menu.texture.desc.Format = format;
      d3d12->menu.texture.srv_heap    = &d3d12->desc.srv_heap;
      d3d12_init_texture(d3d12->device, &d3d12->menu.texture);
   }

   d3d12_update_texture(width, height, pitch,
         format, frame, &d3d12->menu.texture);

   d3d12->menu.alpha = alpha;

   {
      D3D12_RANGE     read_range = { 0, 0 };
      d3d12_vertex_t* v          = NULL;

      D3D12Map(d3d12->menu.vbo, 0, &read_range, (void**)&v);
      v[0].color[3] = alpha;
      v[1].color[3] = alpha;
      v[2].color[3] = alpha;
      v[3].color[3] = alpha;
      D3D12Unmap(d3d12->menu.vbo, 0, NULL);
   }

   d3d12->menu.texture.sampler = settings->bools.menu_linear_filter
      ? d3d12->samplers[RARCH_FILTER_LINEAR][RARCH_WRAP_DEFAULT]
      : d3d12->samplers[RARCH_FILTER_NEAREST][RARCH_WRAP_DEFAULT];
}

static void d3d12_set_menu_texture_enable(void* data,
      bool state, bool full_screen)
{
   d3d12_video_t* d3d12   = (d3d12_video_t*)data;

   if (!d3d12)
      return;

   d3d12->menu.enabled    = state;
   d3d12->menu.fullscreen = full_screen;
}

static void d3d12_gfx_set_aspect_ratio(void* data, unsigned aspect_ratio_idx)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   if (!d3d12)
      return;

   d3d12->keep_aspect     = true;
   d3d12->resize_viewport = true;
}

static void d3d12_gfx_apply_state_changes(void* data)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   if (d3d12)
      d3d12->resize_viewport = true;
}

static void d3d12_gfx_set_osd_msg(
      void* data, video_frame_info_t* video_info,
      const char* msg, const void* params, void* font)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   if (!d3d12 || !d3d12->sprites.enabled)
      return;

   font_driver_render_msg(d3d12, video_info, msg,
         (const struct font_params*)params, font);
}

static uintptr_t d3d12_gfx_load_texture(
      void* video_data, void* data, bool threaded,
      enum texture_filter_type filter_type)
{
   d3d12_texture_t*      texture = NULL;
   d3d12_video_t*        d3d12   = (d3d12_video_t*)video_data;
   struct texture_image* image   = (struct texture_image*)data;

   if (!d3d12)
      return 0;

   texture = (d3d12_texture_t*)calloc(1, sizeof(*texture));

   if (!texture)
      return 0;

   switch (filter_type)
   {
      case TEXTURE_FILTER_MIPMAP_LINEAR:
         texture->desc.MipLevels = UINT16_MAX;
      case TEXTURE_FILTER_LINEAR:
         texture->sampler = d3d12->samplers[
            RARCH_FILTER_LINEAR][RARCH_WRAP_EDGE];
         break;
      case TEXTURE_FILTER_MIPMAP_NEAREST:
         texture->desc.MipLevels = UINT16_MAX;
      case TEXTURE_FILTER_NEAREST:
         texture->sampler = d3d12->samplers[
            RARCH_FILTER_NEAREST][RARCH_WRAP_EDGE];
         break;
   }

   texture->desc.Width  = image->width;
   texture->desc.Height = image->height;
   texture->desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
   texture->srv_heap    = &d3d12->desc.srv_heap;

   d3d12_init_texture(d3d12->device, texture);

   d3d12_update_texture(
         image->width, image->height, 0,
         DXGI_FORMAT_B8G8R8A8_UNORM, image->pixels, texture);

   return (uintptr_t)texture;
}
static void d3d12_gfx_unload_texture(void* data, uintptr_t handle)
{
   d3d12_texture_t* texture = (d3d12_texture_t*)handle;

   if (!texture)
      return;

   d3d12_gfx_sync((d3d12_video_t*)data);
   d3d12_release_texture(texture);
   free(texture);
}

static uint32_t d3d12_get_flags(void *data)
{
   uint32_t flags = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_MENU_FRAME_FILTERING);
#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
#endif

   return flags;
}

static const video_poke_interface_t d3d12_poke_interface = {
   d3d12_get_flags,
   d3d12_gfx_load_texture,
   d3d12_gfx_unload_texture,
   NULL, /* set_video_mode */
#ifndef __WINRT__
   win32_get_refresh_rate,
#else
   /* UWP does not expose this information easily */
   NULL,
#endif
   d3d12_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   d3d12_gfx_set_aspect_ratio,
   d3d12_gfx_apply_state_changes,
   d3d12_set_menu_texture_frame,
   d3d12_set_menu_texture_enable,
   d3d12_gfx_set_osd_msg,
   win32_show_cursor,
   NULL, /* grab_mouse_toggle */
   d3d12_gfx_get_current_shader,
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
};

static void d3d12_gfx_get_poke_interface(void* data, const video_poke_interface_t** iface)
{
   *iface = &d3d12_poke_interface;
}

#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
static bool d3d12_menu_widgets_enabled(void *data)
{
   (void)data;
   return true;
}
#endif

video_driver_t video_d3d12 = {
   d3d12_gfx_init,
   d3d12_gfx_frame,
   d3d12_gfx_set_nonblock_state,
   d3d12_gfx_alive,
   win32_has_focus,
   d3d12_gfx_suppress_screensaver,
   d3d12_gfx_has_windowed,
   d3d12_gfx_set_shader,
   d3d12_gfx_free,
   "d3d12",
   NULL, /* set_viewport */
   d3d12_gfx_set_rotation,
   d3d12_gfx_viewport_info,
   NULL, /* read_viewport  */
   NULL, /* read_frame_raw */

#ifdef HAVE_OVERLAY
   d3d12_get_overlay_interface,
#endif
#ifdef HAVE_VIDEO_LAYOUT
   NULL,
#endif
   d3d12_gfx_get_poke_interface,
   NULL, /* d3d12_wrap_type_to_enum */
#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
   d3d12_menu_widgets_enabled
#endif
};
