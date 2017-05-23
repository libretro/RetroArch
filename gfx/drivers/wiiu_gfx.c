/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Ali Bouhlel
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <string.h>
#include <wiiu/os.h>
#include <wiiu/gx2.h>
#include <formats/image.h>

#include "../../driver.h"
#include "../../configuration.h"
#include "../../verbosity.h"

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "gfx/common/gx2_common.h"
#include "system/memory.h"

#include "wiiu_dbg.h"

#include "../font_driver.h"


static const wiiu_render_mode_t wiiu_render_mode_map[] =
{
   {0},                                         /* GX2_TV_SCAN_MODE_NONE  */
   {854,  480,  GX2_TV_RENDER_MODE_WIDE_480P},  /* GX2_TV_SCAN_MODE_576I  */
   {854,  480,  GX2_TV_RENDER_MODE_WIDE_480P},  /* GX2_TV_SCAN_MODE_480I  */
   {854,  480,  GX2_TV_RENDER_MODE_WIDE_480P},  /* GX2_TV_SCAN_MODE_480P  */
   {1280, 720,  GX2_TV_RENDER_MODE_WIDE_720P},  /* GX2_TV_SCAN_MODE_720P  */
   {0},                                         /* GX2_TV_SCAN_MODE_unk   */
   {1920, 1080, GX2_TV_RENDER_MODE_WIDE_1080P}, /* GX2_TV_SCAN_MODE_1080I */
   {1920, 1080, GX2_TV_RENDER_MODE_WIDE_1080P}  /* GX2_TV_SCAN_MODE_1080P */
};

static int wiiu_set_position(position_t* position, GX2ColorBuffer* draw_buffer, float x0, float y0, float x1, float y1)
{
   position[0].x = (2.0f * x0 / draw_buffer->surface.width) - 1.0f;
   position[0].y = (2.0f * y0 / draw_buffer->surface.height) - 1.0f;
   position[1].x = (2.0f * x1 / draw_buffer->surface.width) - 1.0f;;
   position[1].y = (2.0f * y0 / draw_buffer->surface.height) - 1.0f;
   position[2].x = (2.0f * x1 / draw_buffer->surface.width) - 1.0f;;
   position[2].y = (2.0f * y1 / draw_buffer->surface.height) - 1.0f;
   position[3].x = (2.0f * x0 / draw_buffer->surface.width) - 1.0f;;
   position[3].y = (2.0f * y1 / draw_buffer->surface.height) - 1.0f;
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, position, 4 * sizeof(*position));
}

static void wiiu_set_tex_coords(tex_coord_t* tex_coord, GX2Texture* texture, float u0, float v0, float u1, float v1, unsigned rotation)
{
   tex_coord[((0 + rotation) % 4)].u = u0 / texture->surface.width;
   tex_coord[((0 + rotation) % 4)].v = (v1 / texture->surface.height);
   tex_coord[((1 + rotation) % 4)].u = u1 / texture->surface.width;
   tex_coord[((1 + rotation) % 4)].v = (v1 / texture->surface.height);
   tex_coord[((2 + rotation) % 4)].u = u1 / texture->surface.width;
   tex_coord[((2 + rotation) % 4)].v = (v0 / texture->surface.height);
   tex_coord[((3 + rotation) % 4)].u = u0 / texture->surface.width;
   tex_coord[((3 + rotation) % 4)].v = (v0 / texture->surface.height);

   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, tex_coord, 4 * sizeof(*tex_coord));
}

static void wiiu_gfx_update_viewport(wiiu_video_t* wiiu)
{
   int x                = 0;
   int y                = 0;
   float width          = wiiu->vp.full_width;
   float height         = wiiu->vp.full_height;
   settings_t *settings = config_get_ptr();
   float desired_aspect = video_driver_get_aspect_ratio();

   if(wiiu->rotation & 0x1)
      desired_aspect = 1.0 / desired_aspect;

   if (settings->bools.video_scale_integer)
   {
      video_viewport_get_scaled_integer(&wiiu->vp, wiiu->vp.full_width,
            wiiu->vp.full_height, desired_aspect, wiiu->keep_aspect);
   }
   else if (wiiu->keep_aspect)
   {
#if defined(HAVE_MENU)
      if (settings->uints.video_aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         struct video_viewport *custom = video_viewport_get_custom();

         x      = custom->x;
         y      = custom->y;
         width  = custom->width;
         height = custom->height;
      }
      else
#endif
      {
         float delta;
         float device_aspect  = ((float)wiiu->vp.full_width) / wiiu->vp.full_height;

         if (fabsf(device_aspect - desired_aspect) < 0.0001f)
         {
            /* If the aspect ratios of screen and desired aspect
             * ratio are sufficiently equal (floating point stuff),
             * assume they are actually equal.
             */
         }
         else if (device_aspect > desired_aspect)
         {
            delta = (desired_aspect / device_aspect - 1.0f)
               / 2.0f + 0.5f;
            x     = (int)roundf(width * (0.5f - delta));
            width = (unsigned)roundf(2.0f * width * delta);
         }
         else
         {
            delta  = (device_aspect / desired_aspect - 1.0f)
               / 2.0f + 0.5f;
            y      = (int)roundf(height * (0.5f - delta));
            height = (unsigned)roundf(2.0f * height * delta);
         }
      }

      wiiu->vp.x      = x;
      wiiu->vp.y      = y;
      wiiu->vp.width  = width;
      wiiu->vp.height = height;
   }
   else
   {
      wiiu->vp.x = wiiu->vp.y = 0;
      wiiu->vp.width = width;
      wiiu->vp.height = height;
   }


   float scale_w = wiiu->color_buffer.surface.width / wiiu->render_mode.width;
   float scale_h = wiiu->color_buffer.surface.height / wiiu->render_mode.height;
   wiiu_set_position(wiiu->position, &wiiu->color_buffer,
                     wiiu->vp.x * scale_w,
                     wiiu->vp.y * scale_h,
                    (wiiu->vp.x + wiiu->vp.width) * scale_w,
                    (wiiu->vp.y + wiiu->vp.height) * scale_h);

   wiiu->should_resize = false;
}

static void wiiu_gfx_set_aspect_ratio(void* data, unsigned aspect_ratio_idx)
{
   wiiu_video_t *wiiu = (wiiu_video_t*)data;

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

   video_driver_set_aspect_ratio_value(aspectratio_lut[aspect_ratio_idx].value);

   if(!wiiu)
      return;

   wiiu->keep_aspect = true;
   wiiu->should_resize = true;
}

static void* wiiu_gfx_init(const video_info_t* video,
      const input_driver_t** input, void** input_data)
{
   int i;
   float refresh_rate = 60.0f / 1.001f;
   u32 size           = 0;
   u32 tmp            = 0;
   void* wiiuinput    = NULL;
   wiiu_video_t* wiiu = calloc(1, sizeof(*wiiu));

   if (!wiiu)
      return NULL;

   *input             = NULL;
   *input_data        = NULL;

   if (input && input_data)
   {
      settings_t *settings = config_get_ptr();
      wiiuinput            = input_wiiu.init(settings->arrays.input_joypad_driver);
      *input               = wiiuinput ? &input_wiiu : NULL;
      *input_data          = wiiuinput;
   }

   /* video init */
   wiiu->cmd_buffer = MEM2_alloc(0x400000, 0x40);
   u32 init_attributes[] =
   {
      GX2_INIT_CMD_BUF_BASE, (u32)wiiu->cmd_buffer,
      GX2_INIT_CMD_BUF_POOL_SIZE, 0x400000,
      GX2_INIT_ARGC, 0,
      GX2_INIT_ARGV, 0,
      GX2_INIT_END
   };
   GX2Init(init_attributes);

   /* setup scanbuffers */
   wiiu->render_mode = wiiu_render_mode_map[GX2GetSystemTVScanMode()];
   GX2CalcTVSize(wiiu->render_mode.mode, GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8,
         GX2_BUFFERING_MODE_DOUBLE, &size, &tmp);

   wiiu->tv_scan_buffer = MEMBucket_alloc(size, GX2_SCAN_BUFFER_ALIGNMENT);
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU, wiiu->tv_scan_buffer, size);
   GX2SetTVBuffer(wiiu->tv_scan_buffer, size, wiiu->render_mode.mode,
         GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8,
         GX2_BUFFERING_MODE_DOUBLE);

   GX2CalcDRCSize(GX2_DRC_RENDER_MODE_SINGLE, GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8,
         GX2_BUFFERING_MODE_DOUBLE, &size,
         &tmp);

   wiiu->drc_scan_buffer = MEMBucket_alloc(size, GX2_SCAN_BUFFER_ALIGNMENT);
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU, wiiu->drc_scan_buffer, size);
   GX2SetDRCBuffer(wiiu->drc_scan_buffer, size, GX2_DRC_RENDER_MODE_SINGLE,
         GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8,
         GX2_BUFFERING_MODE_DOUBLE);

   memset(&wiiu->color_buffer, 0, sizeof(GX2ColorBuffer));

   wiiu->color_buffer.surface.dim       = GX2_SURFACE_DIM_TEXTURE_2D;
   wiiu->color_buffer.surface.width     = wiiu->render_mode.width;
   wiiu->color_buffer.surface.height    = wiiu->render_mode.height;
   wiiu->color_buffer.surface.depth     = 1;
   wiiu->color_buffer.surface.mipLevels = 1;
   wiiu->color_buffer.surface.format    = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
   wiiu->color_buffer.surface.use       = GX2_SURFACE_USE_TEXTURE_COLOR_BUFFER_TV;
   wiiu->color_buffer.viewNumSlices     = 1;

   GX2CalcSurfaceSizeAndAlignment(&wiiu->color_buffer.surface);
   GX2InitColorBufferRegs(&wiiu->color_buffer);

   wiiu->color_buffer.surface.image = MEM1_alloc(wiiu->color_buffer.surface.imageSize,
                                      wiiu->color_buffer.surface.alignment);
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU, wiiu->color_buffer.surface.image,
         wiiu->color_buffer.surface.imageSize);

   wiiu->ctx_state = (GX2ContextState*)MEM2_alloc(sizeof(GX2ContextState), GX2_CONTEXT_STATE_ALIGNMENT);
   GX2SetupContextStateEx(wiiu->ctx_state, GX2_TRUE);

   GX2SetContextState(wiiu->ctx_state);
   GX2SetColorBuffer(&wiiu->color_buffer, GX2_RENDER_TARGET_0);
   GX2SetViewport(0.0f, 0.0f, wiiu->color_buffer.surface.width, wiiu->color_buffer.surface.height, 0.0f, 1.0f);
   GX2SetScissor(0, 0, wiiu->color_buffer.surface.width, wiiu->color_buffer.surface.height);
   GX2SetDepthOnlyControl(GX2_DISABLE, GX2_DISABLE, GX2_COMPARE_FUNC_ALWAYS);
   GX2SetColorControl(GX2_LOGIC_OP_COPY, 1, GX2_DISABLE, GX2_ENABLE);
#if 1
   GX2SetBlendControl(GX2_RENDER_TARGET_0, GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_INV_SRC_ALPHA, GX2_BLEND_COMBINE_MODE_ADD,
                      GX2_ENABLE,          GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_INV_SRC_ALPHA, GX2_BLEND_COMBINE_MODE_ADD);
#else
   GX2SetBlendControl(GX2_RENDER_TARGET_0, GX2_BLEND_MODE_ONE, GX2_BLEND_MODE_ZERO, GX2_BLEND_COMBINE_MODE_ADD,
                      GX2_DISABLE,         GX2_BLEND_MODE_ONE, GX2_BLEND_MODE_ZERO, GX2_BLEND_COMBINE_MODE_ADD);
#endif
   GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, GX2_DISABLE, GX2_DISABLE);
#ifdef GX2_CAN_ACCESS_DATA_SECTION
   wiiu->shader = &tex_shader;
#else

   /* init shader */
   wiiu->shader = MEM2_alloc(sizeof(tex_shader), 0x1000);
   memcpy(wiiu->shader, &tex_shader, sizeof(tex_shader));
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU, wiiu->shader, sizeof(tex_shader));

   wiiu->shader->vs.program = MEM2_alloc(wiiu->shader->vs.size, GX2_SHADER_ALIGNMENT);
   memcpy(wiiu->shader->vs.program, tex_shader.vs.program, wiiu->shader->vs.size);
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, wiiu->shader->vs.program, wiiu->shader->vs.size);
   wiiu->shader->vs.attribVars = MEM2_alloc(wiiu->shader->vs.attribVarCount * sizeof(GX2AttribVar),
         GX2_SHADER_ALIGNMENT);
   memcpy(wiiu->shader->vs.attribVars, tex_shader.vs.attribVars ,
          wiiu->shader->vs.attribVarCount * sizeof(GX2AttribVar));

   wiiu->shader->ps.program = MEM2_alloc(wiiu->shader->ps.size, GX2_SHADER_ALIGNMENT);
   memcpy(wiiu->shader->ps.program, tex_shader.ps.program, wiiu->shader->ps.size);
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, wiiu->shader->ps.program, wiiu->shader->ps.size);
   wiiu->shader->ps.samplerVars = MEM2_alloc(wiiu->shader->ps.samplerVarCount * sizeof(GX2SamplerVar),
         GX2_SHADER_ALIGNMENT);
   memcpy(wiiu->shader->ps.samplerVars, tex_shader.ps.samplerVars,
          wiiu->shader->ps.samplerVarCount * sizeof(GX2SamplerVar));

#endif
   wiiu->shader->fs.size = GX2CalcFetchShaderSizeEx(2, GX2_FETCH_SHADER_TESSELLATION_NONE,
         GX2_TESSELLATION_MODE_DISCRETE);
   wiiu->shader->fs.program = MEM2_alloc(wiiu->shader->fs.size, GX2_SHADER_ALIGNMENT);
   GX2InitFetchShaderEx(&wiiu->shader->fs, (uint8_t*)wiiu->shader->fs.program,
                        sizeof(wiiu->shader->attribute_stream) /  sizeof(GX2AttribStream),
                        (GX2AttribStream*)&wiiu->shader->attribute_stream,
                        GX2_FETCH_SHADER_TESSELLATION_NONE, GX2_TESSELLATION_MODE_DISCRETE);
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, wiiu->shader->fs.program, wiiu->shader->fs.size);
   GX2SetVertexShader(&wiiu->shader->vs);
   GX2SetPixelShader(&wiiu->shader->ps);
   GX2SetFetchShader(&wiiu->shader->fs);

   wiiu->position = MEM2_alloc(4 * sizeof(*wiiu->position), GX2_VERTEX_BUFFER_ALIGNMENT);
   wiiu_set_position(wiiu->position, &wiiu->color_buffer, 0, 0,
         wiiu->color_buffer.surface.width, wiiu->color_buffer.surface.height);

   wiiu->tex_coord = MEM2_alloc(4 * sizeof(*wiiu->tex_coord), GX2_VERTEX_BUFFER_ALIGNMENT);
   wiiu_set_tex_coords(wiiu->tex_coord, &wiiu->texture, 0, 0,
         wiiu->texture.surface.width, wiiu->texture.surface.height, wiiu->rotation);

   GX2SetAttribBuffer(0, 4 * sizeof(*wiiu->position), sizeof(*wiiu->position), wiiu->position);
   GX2SetAttribBuffer(1, 4 * sizeof(*wiiu->tex_coord), sizeof(*wiiu->tex_coord), wiiu->tex_coord);

   wiiu->menu.position = MEM2_alloc(4 * sizeof(*wiiu->menu.position), GX2_VERTEX_BUFFER_ALIGNMENT);
   wiiu_set_position(wiiu->menu.position, &wiiu->color_buffer, 0, 0,
         wiiu->color_buffer.surface.width, wiiu->color_buffer.surface.height);

   wiiu->menu.tex_coord = MEM2_alloc(4 * sizeof(*wiiu->menu.tex_coord), GX2_VERTEX_BUFFER_ALIGNMENT);
   wiiu_set_tex_coords(wiiu->menu.tex_coord, &wiiu->menu.texture, 0, 0,
         wiiu->menu.texture.surface.width, wiiu->menu.texture.surface.height, 0);

   /* init frame texture */
   memset(&wiiu->texture, 0, sizeof(GX2Texture));
   wiiu->texture.surface.width       = video->input_scale * RARCH_SCALE_BASE;
   wiiu->texture.surface.height      = video->input_scale * RARCH_SCALE_BASE;
   wiiu->texture.surface.depth       = 1;
   wiiu->texture.surface.dim         = GX2_SURFACE_DIM_TEXTURE_2D;
   wiiu->texture.surface.tileMode    = GX2_TILE_MODE_LINEAR_ALIGNED;
   wiiu->texture.viewNumSlices       = 1;
   wiiu->rgb32                       = video->rgb32;

   if(wiiu->rgb32)
   {
      wiiu->texture.surface.format   = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
      wiiu->texture.compMap          = GX2_COMP_SEL(_G, _B, _A, _1);
   }
   else
   {
      wiiu->texture.surface.format   = GX2_SURFACE_FORMAT_UNORM_R5_G6_B5;
      wiiu->texture.compMap          = GX2_COMP_SEL(_B, _G, _R, _1);
   }
   GX2CalcSurfaceSizeAndAlignment(&wiiu->texture.surface);
   GX2InitTextureRegs(&wiiu->texture);

   wiiu->texture.surface.image = MEM2_alloc(wiiu->texture.surface.imageSize,
                                 wiiu->texture.surface.alignment);
   memset(wiiu->texture.surface.image, 0x0, wiiu->texture.surface.imageSize);
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, wiiu->texture.surface.image,
                 wiiu->texture.surface.imageSize);

   /* init menu texture */
   memset(&wiiu->menu.texture, 0, sizeof(GX2Texture));
   wiiu->menu.texture.surface.width    = 512;
   wiiu->menu.texture.surface.height   = 512;
   wiiu->menu.texture.surface.depth    = 1;
   wiiu->menu.texture.surface.dim      = GX2_SURFACE_DIM_TEXTURE_2D;
   wiiu->menu.texture.surface.format   = GX2_SURFACE_FORMAT_UNORM_R4_G4_B4_A4;
   wiiu->menu.texture.surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
   wiiu->menu.texture.viewNumSlices    = 1;
   wiiu->menu.texture.compMap          = GX2_COMP_SEL(_A, _R, _G, _B);
   GX2CalcSurfaceSizeAndAlignment(&wiiu->menu.texture.surface);
   GX2InitTextureRegs(&wiiu->menu.texture);

   wiiu->menu.texture.surface.image = MEM2_alloc(wiiu->menu.texture.surface.imageSize,
                                      wiiu->menu.texture.surface.alignment);

   memset(wiiu->menu.texture.surface.image, 0x0, wiiu->menu.texture.surface.imageSize);
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, wiiu->menu.texture.surface.image,
                 wiiu->menu.texture.surface.imageSize);

   wiiu->vertex_cache.size = 0x1000;
   wiiu->vertex_cache.current = 0;
   wiiu->vertex_cache.positions = MEM2_alloc(wiiu->vertex_cache.size * sizeof(position_t), GX2_VERTEX_BUFFER_ALIGNMENT);
   wiiu->vertex_cache.tex_coords = MEM2_alloc(wiiu->vertex_cache.size * sizeof(tex_coord_t), GX2_VERTEX_BUFFER_ALIGNMENT);

   /* init samplers */
   GX2InitSampler(&wiiu->sampler_nearest, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_POINT);
   GX2InitSampler(&wiiu->sampler_linear, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);

   /* set Texture and Sampler */
   GX2SetPixelTexture(&wiiu->texture, wiiu->shader->sampler.location);
   GX2SetPixelSampler(&wiiu->sampler_linear, wiiu->shader->sampler.location);

   /* clear leftover image */
   GX2ClearColor(&wiiu->color_buffer, 0.0f, 0.0f, 0.0f, 1.0f);
   GX2CopyColorBufferToScanBuffer(&wiiu->color_buffer, GX2_SCAN_TARGET_DRC);
   GX2CopyColorBufferToScanBuffer(&wiiu->color_buffer, GX2_SCAN_TARGET_TV);

   GX2SwapScanBuffers();
   GX2Flush();
   GX2WaitForVsync();

   GX2SetTVEnable(GX2_ENABLE);
   GX2SetDRCEnable(GX2_ENABLE);

   wiiu->keep_aspect   = true;
   wiiu->should_resize = true;
   wiiu->smooth        = video->smooth;
   wiiu->vsync         = video->vsync;
   GX2SetSwapInterval(!!video->vsync);

   wiiu->vp.x           = 0;
   wiiu->vp.y           = 0;
   wiiu->vp.width       = wiiu->render_mode.width;
   wiiu->vp.height      = wiiu->render_mode.height;
   wiiu->vp.full_width  = wiiu->render_mode.width;
   wiiu->vp.full_height = wiiu->render_mode.height;
   video_driver_set_size(&wiiu->vp.width, &wiiu->vp.height);

   driver_ctl(RARCH_DRIVER_CTL_SET_REFRESH_RATE, &refresh_rate);

   font_driver_init_osd(wiiu, false,
         video->is_threaded,
         FONT_DRIVER_RENDER_WIIU);

   return wiiu;
}

static void wiiu_gfx_free(void* data)
{
   wiiu_video_t* wiiu = (wiiu_video_t*) data;

   if (!wiiu)
      return;

   /* clear leftover image */
   GX2ClearColor(&wiiu->color_buffer, 0.0f, 0.0f, 0.0f, 1.0f);
   GX2CopyColorBufferToScanBuffer(&wiiu->color_buffer, GX2_SCAN_TARGET_DRC);
   GX2CopyColorBufferToScanBuffer(&wiiu->color_buffer, GX2_SCAN_TARGET_TV);

   GX2SwapScanBuffers();
   GX2Flush();
   GX2DrawDone();
   GX2WaitForVsync();
   GX2Shutdown();

   GX2SetTVEnable(GX2_DISABLE);
   GX2SetDRCEnable(GX2_DISABLE);

   MEM2_free(wiiu->ctx_state);
   MEM2_free(wiiu->cmd_buffer);
   MEM2_free(wiiu->texture.surface.image);
   MEM2_free(wiiu->menu.texture.surface.image);
   MEM2_free(wiiu->vertex_cache.positions);
   MEM2_free(wiiu->vertex_cache.tex_coords);

   MEM1_free(wiiu->color_buffer.surface.image);

   MEMBucket_free(wiiu->tv_scan_buffer);
   MEMBucket_free(wiiu->drc_scan_buffer);

   MEM2_free(wiiu->shader->fs.program);
#ifndef GX2_CAN_ACCESS_DATA_SECTION
   MEM2_free(wiiu->shader->vs.program);
   MEM2_free(wiiu->shader->vs.attribVars);

   MEM2_free(wiiu->shader->ps.program);
   MEM2_free(wiiu->shader->ps.samplerVars);


   MEM2_free(wiiu->shader);
#endif
   MEM2_free(wiiu->position);
   MEM2_free(wiiu->tex_coord);
   MEM2_free(wiiu->menu.position);
   MEM2_free(wiiu->menu.tex_coord);


   free(wiiu);
}

static bool wiiu_gfx_frame(void* data, const void* frame,
      unsigned width, unsigned height, uint64_t frame_count,
      unsigned pitch, const char* msg, video_frame_info_t *video_info)
{
   (void)msg;
   int i;

   wiiu_video_t* wiiu = (wiiu_video_t*) data;

   if(wiiu->vsync)
   {
      uint32_t swap_count;
      uint32_t flip_count;
      OSTime last_flip;
      OSTime last_vsync;

      GX2GetSwapStatus(&swap_count, &flip_count, &last_flip, &last_vsync);

      if(wiiu->last_vsync >= last_vsync)
      {
         GX2WaitForVsync();
         wiiu->last_vsync = last_vsync + ms_to_ticks(17);
      }
      else
         wiiu->last_vsync = last_vsync;
   }
   GX2WaitForFlip();


   if (!width || !height)
      return true;

   static u32 lastTick , currentTick;
   currentTick = OSGetSystemTick();
   u32 diff = currentTick - lastTick;
   static float fps;
   static u32 frames;
   frames++;
   if(diff > wiiu_timer_clock)
   {
      fps = (float)frames * ((float) wiiu_timer_clock / (float) diff);
      lastTick = currentTick;
      frames = 0;
   }
#if 0
   static u32 last_frame_tick;
   if (!(wiiu->menu.enable))
      printf("frame time : %10.6f ms            \r", (float)(currentTick - last_frame_tick) * 1000.0f / (float)wiiu_timer_clock);
   last_frame_tick = currentTick;
#endif
   printf("fps: %8.8f frames : %5i\r", fps, wiiu->frames++);
   fflush(stdout);

   if (wiiu->should_resize)
      wiiu_gfx_update_viewport(wiiu);

   GX2ClearColor(&wiiu->color_buffer, 0.0f, 0.0f, 0.0f, 1.0f);
   /* can't call GX2ClearColor after GX2SetContextState for whatever reason */
   GX2SetContextState(wiiu->ctx_state);

   if(frame)
   {
      if (width > wiiu->texture.surface.width)
         width = wiiu->texture.surface.width;

      if (height > wiiu->texture.surface.height)
         height = wiiu->texture.surface.height;

      wiiu->width = width;
      wiiu->height = height;
      if(wiiu->rgb32)
      {
         const uint32_t* src = frame;
         uint32_t* dst = (uint32_t*)wiiu->texture.surface.image;

         for (i = 0; i < height; i++)
         {
            int j;
            for(j = 0; j < width; j++)
               dst[j] = src[j];
            dst += wiiu->texture.surface.pitch;
            src += pitch / 4;
         }
      }
      else
      {
         const uint16_t* src = frame;
         uint16_t* dst = (uint16_t*)wiiu->texture.surface.image;

         for (i = 0; i < height; i++)
         {
            int j;
            for(j = 0; j < width; j++)
               dst[j] = __builtin_bswap16(src[j]);
            dst += wiiu->texture.surface.pitch;
            src += pitch / 2;
         }
      }


      GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, wiiu->texture.surface.image,
                    wiiu->texture.surface.imageSize);
      wiiu_set_tex_coords(wiiu->tex_coord, &wiiu->texture, 0, 0, width, height, wiiu->rotation);
   }

   GX2SetAttribBuffer(0, 4 * sizeof(*wiiu->position), sizeof(*wiiu->position), wiiu->position);
   GX2SetAttribBuffer(1, 4 * sizeof(*wiiu->tex_coord), sizeof(*wiiu->tex_coord), wiiu->tex_coord);

   GX2SetPixelTexture(&wiiu->texture, wiiu->shader->sampler.location);
   GX2SetPixelSampler(wiiu->smooth? &wiiu->sampler_linear : &wiiu->sampler_nearest,
                      wiiu->shader->sampler.location);

   GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);

   if (wiiu->menu.enable)
   {
      GX2SetAttribBuffer(0, 4 * sizeof(*wiiu->menu.position), sizeof(*wiiu->menu.position), wiiu->menu.position);
      GX2SetAttribBuffer(1, 4 * sizeof(*wiiu->menu.tex_coord), sizeof(*wiiu->menu.tex_coord), wiiu->menu.tex_coord);

      GX2SetPixelTexture(&wiiu->menu.texture, wiiu->shader->sampler.location);
      GX2SetPixelSampler(&wiiu->sampler_linear, wiiu->shader->sampler.location);

      GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);
   }

   wiiu->vertex_cache.current = 0;
   GX2SetAttribBuffer(0, wiiu->vertex_cache.size * sizeof(position_t), sizeof(position_t), wiiu->vertex_cache.positions);
   GX2SetAttribBuffer(1, wiiu->vertex_cache.size * sizeof(tex_coord_t), sizeof(tex_coord_t), wiiu->vertex_cache.tex_coords);
   GX2SetPixelSampler(&wiiu->sampler_linear, wiiu->shader->sampler.location);

   wiiu->render_msg_enabled = true;

   if (wiiu->menu.enable)
      menu_driver_frame(video_info);

   if (msg)
      font_driver_render_msg(video_info, NULL, msg, NULL);

   wiiu->render_msg_enabled = false;

   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, wiiu->vertex_cache.positions, wiiu->vertex_cache.current * sizeof(position_t));
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, wiiu->vertex_cache.tex_coords, wiiu->vertex_cache.current * sizeof(tex_coord_t));

   if (wiiu->menu.enable)
      GX2DrawDone();

   GX2CopyColorBufferToScanBuffer(&wiiu->color_buffer, GX2_SCAN_TARGET_DRC);
   GX2CopyColorBufferToScanBuffer(&wiiu->color_buffer, GX2_SCAN_TARGET_TV);

   GX2SwapScanBuffers();
   GX2Flush();

   return true;
}

static void wiiu_gfx_set_nonblock_state(void* data, bool toggle)
{
   wiiu_video_t* wiiu = (wiiu_video_t*) data;

   if (!wiiu)
      return;

   wiiu->vsync = !toggle;
   GX2SetSwapInterval(!toggle);  /* do we need this ? */
}

static bool wiiu_gfx_alive(void* data)
{
   (void)data;
   return true;
}

static bool wiiu_gfx_focus(void* data)
{
   (void)data;
   return true;
}

static bool wiiu_gfx_suppress_screensaver(void* data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static bool wiiu_gfx_set_shader(void* data,
                                enum rarch_shader_type type, const char* path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

static void wiiu_gfx_set_rotation(void* data,
                                  unsigned rotation)
{
   wiiu_video_t* wiiu = (wiiu_video_t*) data;
   if(wiiu)
      wiiu->rotation = rotation;
}

static void wiiu_gfx_viewport_info(void* data,
                                   struct video_viewport* vp)
{
   wiiu_video_t* wiiu = (wiiu_video_t*) data;
   if(wiiu)
      *vp = wiiu->vp;
}

static bool wiiu_gfx_read_viewport(void* data, uint8_t* buffer, bool is_idle)
{
   (void)data;
   (void)buffer;

   return true;
}

static uintptr_t wiiu_gfx_load_texture(void* video_data, void* data,
      bool threaded, enum texture_filter_type filter_type)
{
   int i;
   wiiu_video_t* wiiu = (wiiu_video_t*) video_data;
   struct texture_image *image = (struct texture_image*)data;

   if (!wiiu)
      return 0;

   GX2Texture* texture = calloc(1, sizeof(GX2Texture));

   texture->surface.width       = image->width;
   texture->surface.height      = image->height;
   texture->surface.depth       = 1;
   texture->surface.dim         = GX2_SURFACE_DIM_TEXTURE_2D;
   texture->surface.tileMode    = GX2_TILE_MODE_LINEAR_ALIGNED;
   texture->viewNumSlices       = 1;

   texture->surface.format   = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
   texture->compMap          = GX2_COMP_SEL(_G, _B, _A, _R);

   GX2CalcSurfaceSizeAndAlignment(&texture->surface);
   GX2InitTextureRegs(texture);
   texture->surface.image = MEM2_alloc(texture->surface.imageSize, texture->surface.alignment);

   for (i = 0; (i < image->height) && (i < texture->surface.height); i++)
      memcpy((uint32_t*)texture->surface.image + (i * texture->surface.pitch),
             image->pixels + (i * image->width), image->width * sizeof(image->pixels));

   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, texture->surface.image, texture->surface.imageSize);

   return (uintptr_t)texture;
}
static void wiiu_gfx_unload_texture(void* data, uintptr_t handle)
{
   GX2Texture* texture = (GX2Texture*)handle;

   if(!texture)
      return;

   MEM2_free(texture->surface.image);
   free(texture);
}
static void wiiu_gfx_set_filtering(void* data, unsigned index, bool smooth)
{
   wiiu_video_t* wiiu = (wiiu_video_t*) data;
   if(wiiu)
      wiiu->smooth = smooth;
}


static void wiiu_gfx_apply_state_changes(void* data)
{
   wiiu_video_t* wiiu = (wiiu_video_t*)data;

   if (wiiu)
      wiiu->should_resize = true;


}

static void wiiu_gfx_set_texture_frame(void* data, const void* frame, bool rgb32,
                                   unsigned width, unsigned height, float alpha)
{
   int i;
   wiiu_video_t* wiiu = (wiiu_video_t*) data;

   if (!wiiu)
      return;

   if (!frame || !width || !height)
      return;

   if (width > wiiu->menu.texture.surface.width)
      width = wiiu->menu.texture.surface.width;

   if (height > wiiu->menu.texture.surface.height)
      height = wiiu->menu.texture.surface.height;

   wiiu->menu.width = width;
   wiiu->menu.height = height;

   const uint16_t* src = frame;
   uint16_t* dst = (uint16_t*)wiiu->menu.texture.surface.image;

   for (i = 0; i < height; i++)
   {
      memcpy(dst, src, width * sizeof(uint16_t));
      dst += wiiu->menu.texture.surface.pitch;
      src += width;
   }

   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, wiiu->menu.texture.surface.image,
                 wiiu->menu.texture.surface.imageSize);

   wiiu_set_tex_coords(wiiu->menu.tex_coord, &wiiu->menu.texture, 0, 0, width, height, 0);
//   wiiu_set_tex_coords(wiiu->menu.tex_coord, &wiiu->menu.texture, 0, 0, wiiu->menu.texture.surface.width, wiiu->menu.texture.surface.height, 0);
}

static void wiiu_gfx_set_texture_enable(void* data, bool state, bool full_screen)
{
   (void) full_screen;
   wiiu_video_t* wiiu = (wiiu_video_t*) data;
   if(wiiu)
      wiiu->menu.enable = state;

}

static void wiiu_gfx_set_osd_msg(void* data,
      video_frame_info_t *video_info,
      const char* msg,
      const void* params, void* font)
{
   wiiu_video_t* wiiu = (wiiu_video_t*)data;

   if (wiiu)
   {
      if (wiiu->render_msg_enabled)
         font_driver_render_msg(video_info, font, msg, params);
      else
         printf("OSD msg: %s\n", msg);
   }

}

static const video_poke_interface_t wiiu_poke_interface =
{
   wiiu_gfx_load_texture,
   wiiu_gfx_unload_texture,
   NULL,
   wiiu_gfx_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL,
   wiiu_gfx_set_aspect_ratio,
   wiiu_gfx_apply_state_changes,
#ifdef HAVE_MENU
   wiiu_gfx_set_texture_frame,
#endif
   wiiu_gfx_set_texture_enable,
   wiiu_gfx_set_osd_msg,
   NULL,
   NULL,
   NULL
};

static void wiiu_gfx_get_poke_interface(void* data,
                                        const video_poke_interface_t** iface)
{
   (void)data;
   *iface = &wiiu_poke_interface;
}

video_driver_t video_wiiu =
{
   wiiu_gfx_init,
   wiiu_gfx_frame,
   wiiu_gfx_set_nonblock_state,
   wiiu_gfx_alive,
   wiiu_gfx_focus,
   wiiu_gfx_suppress_screensaver,
   NULL, /* has_windowed */
   wiiu_gfx_set_shader,
   wiiu_gfx_free,
   "gx2",
   NULL, /* set_viewport */
   wiiu_gfx_set_rotation,
   wiiu_gfx_viewport_info,
   wiiu_gfx_read_viewport,
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   NULL, /* overlay_interface */
#endif
   wiiu_gfx_get_poke_interface,
};
