/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2017 - Sergi Granell (xerpi)
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

#include <vita2d.h>

#include <retro_inline.h>
#include <encodings/utf.h>
#include <string/stdstring.h>
#include <formats/image.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif
#ifdef HAVE_GFX_WIDGETS
#include "../gfx_widgets.h"
#endif

#include "../font_driver.h"
#include "../video_driver.h"

#include "../common/vita2d_defines.h"
#include "../../driver.h"
#include "../../verbosity.h"
#include "../../configuration.h"

#include <defines/psp_defines.h>
#include <psp2/kernel/sysmem.h>

typedef struct
{
   vita_video_t *vita;
   vita2d_texture *texture;
   const font_renderer_driver_t *font_driver;
   void *font_data;
   struct font_atlas *atlas;
} vita_font_t;

static const float vita2d_vertexes[8]   = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const float vita2d_tex_coords[8] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

static const float vita2d_colors[16]    = {
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
};


/*
 * FORWARD DECLARATIONS
 */

extern void *memcpy_neon(void *dst, const void *src, size_t n);
static void vita2d_update_viewport(vita_video_t* vita,
      video_frame_info_t *video_info);
static void vita2d_set_viewport_wrapper(void *data, unsigned viewport_width,
      unsigned viewport_height, bool force_full, bool allow_rotate);

/*
 * DISPLAY DRIVER
 */

static const float *gfx_display_vita2d_get_default_vertices(void)
{
   return &vita2d_vertexes[0];
}

static const float *gfx_display_vita2d_get_default_tex_coords(void)
{
   return &vita2d_tex_coords[0];
}

static void *gfx_display_vita2d_get_default_mvp(void *data)
{
   vita_video_t *vita2d = (vita_video_t*)data;

   if (!vita2d)
      return NULL;

   return &vita2d->mvp_no_rot;
}

static void gfx_display_vita2d_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height)
{
   unsigned i;
   struct vita2d_texture *texture   = NULL;
   const float *vertex              = NULL;
   const float *tex_coord           = NULL;
   const float *color               = NULL;
   vita_video_t             *vita2d = (vita_video_t*)data;

   if (!vita2d || !draw)
      return;

   texture            = (struct vita2d_texture*)draw->texture;
   vertex             = draw->coords->vertex;
   tex_coord          = draw->coords->tex_coord;
   color              = draw->coords->color;

   if (!vertex)
      vertex          = &vita2d_vertexes[0];
   if (!tex_coord)
      tex_coord       = &vita2d_tex_coords[0];
   if (!draw->coords->lut_tex_coord)
      draw->coords->lut_tex_coord = &vita2d_tex_coords[0];
   if (!texture)
      return;
   if (!color)
      color           = &vita2d_colors[0];

   vita2d_set_viewport(draw->x, draw->y, draw->width, draw->height);
   vita2d_texture_tint_vertex *vertices = (vita2d_texture_tint_vertex *)vita2d_pool_memalign(
         draw->coords->vertices * sizeof(vita2d_texture_tint_vertex),
         sizeof(vita2d_texture_tint_vertex));

   for (i = 0; i < draw->coords->vertices; i++)
   {
      vertices[i].x = *vertex++;
      vertices[i].y = *vertex++;
      vertices[i].z = 1.0f;
      vertices[i].u = *tex_coord++;
      vertices[i].v = *tex_coord++;
      vertices[i].r = *color++;
      vertices[i].g = *color++;
      vertices[i].b = *color++;
      vertices[i].a = *color++;
   }

   vita2d_draw_array_textured_mat(texture, vertices, draw->coords->vertices, &vita2d->mvp_no_rot);
}

static void gfx_display_vita2d_scissor_begin(void *data,
      unsigned video_width,
      unsigned video_height,
      int x, int y,
      unsigned width, unsigned height)
{
   vita2d_set_clip_rectangle(x, y, x + width, y + height);
   vita2d_set_region_clip(SCE_GXM_REGION_CLIP_OUTSIDE, x, y, x + width, y + height);
}

static void gfx_display_vita2d_scissor_end(
      void *data,
      unsigned video_width,
      unsigned video_height)
{
   vita2d_set_region_clip(SCE_GXM_REGION_CLIP_NONE, 0, 0,
         video_width, video_height);
   vita2d_disable_clipping();
}

gfx_display_ctx_driver_t gfx_display_ctx_vita2d = {
   gfx_display_vita2d_draw,
   NULL,                                        /* draw_pipeline */
   NULL,                                        /* blend_begin   */
   NULL,                                        /* blend_end     */
   gfx_display_vita2d_get_default_mvp,
   gfx_display_vita2d_get_default_vertices,
   gfx_display_vita2d_get_default_tex_coords,
   FONT_DRIVER_RENDER_VITA2D,
   GFX_VIDEO_DRIVER_VITA2D,
   "vita2d",
   true,
   gfx_display_vita2d_scissor_begin,
   gfx_display_vita2d_scissor_end
};

/*
 * FONT DRIVER
 */

static void *vita2d_font_init(void *data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   unsigned int stride, pitch, j, k;
   const uint8_t         *frame32 = NULL;
   uint8_t                 *tex32 = NULL;
   const struct font_atlas *atlas = NULL;
   vita_font_t              *font = (vita_font_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   font->vita                     = (vita_video_t*)data;

   if (!font_renderer_create_default(
            &font->font_driver,
            &font->font_data, font_path, font_size))
      goto error;

   font->atlas   = font->font_driver->get_atlas(font->font_data);
   atlas         = font->atlas;

   if (!atlas)
      goto error;

   font->texture = vita2d_create_empty_texture_format(
         atlas->width,
         atlas->height,
         SCE_GXM_TEXTURE_FORMAT_U8_R111);

   if (!font->texture)
      goto error;

   vita2d_texture_set_filters(font->texture,
         SCE_GXM_TEXTURE_FILTER_POINT,
         SCE_GXM_TEXTURE_FILTER_LINEAR);

   stride  = vita2d_texture_get_stride(font->texture);
   tex32   = vita2d_texture_get_datap(font->texture);
   frame32 = atlas->buffer;
   pitch   = atlas->width;

   for (j = 0; j < atlas->height; j++)
      for (k = 0; k < atlas->width; k++)
         tex32[k + j * stride] = frame32[k + j*pitch];

   font->atlas->dirty = false;

   return font;

error:
   free(font);
   return NULL;
}

static void vita2d_font_free(void *data, bool is_threaded)
{
   vita_font_t *font = (vita_font_t*)data;
   if (!font)
      return;

   if (font->font_driver && font->font_data)
      font->font_driver->free(font->font_data);

   vita2d_wait_rendering_done();
   vita2d_free_texture(font->texture);

   free(font);
}

static int vita2d_font_get_message_width(void *data, const char *msg,
      size_t msg_len, float scale)
{
   int i;
   const struct font_glyph* glyph_q = NULL;
   int delta_x       = 0;
   vita_font_t *font = (vita_font_t*)data;

   if (!font)
      return 0;

   glyph_q = font->font_driver->get_glyph(font->font_data, '?');

   for (i = 0; i < msg_len; i++)
   {
      const struct font_glyph *glyph = NULL;
      const char *msg_tmp            = &msg[i];
      unsigned code                  = utf8_walk(&msg_tmp);
      unsigned skip                  = msg_tmp - &msg[i];

      if (skip > 1)
         i += skip - 1;

      /* Do something smarter here ... */
      if (!(glyph = font->font_driver->get_glyph(font->font_data, code)))
         if (!(glyph = glyph_q))
            continue;

      delta_x += glyph->advance_x;
   }

   return delta_x * scale;
}

static void vita2d_font_render_line(
      vita_video_t *vita,
      vita_font_t *font,
      const struct font_glyph* glyph_q,
      const char *msg, size_t msg_len,
      float scale, const unsigned int color, float pos_x,
      float pos_y,
      unsigned width,
      unsigned height,
      int pre_x,
      unsigned text_align)
{
   int i;
   int x           = pre_x;
   int y           = roundf((1.0f - pos_y) * height);
   int delta_x     = 0;
   int delta_y     = 0;

   switch (text_align)
   {
      case TEXT_ALIGN_RIGHT:
         x -= vita2d_font_get_message_width(font, msg, msg_len, scale);
         break;
      case TEXT_ALIGN_CENTER:
         x -= vita2d_font_get_message_width(font, msg, msg_len, scale) / 2;
         break;
   }

   for (i = 0; i < msg_len; i++)
   {
      int j, k;
      int off_x, off_y, tex_x, tex_y, width, height;
      const struct font_glyph *glyph = NULL;
      const char *msg_tmp            = &msg[i];
      unsigned code                  = utf8_walk(&msg_tmp);
      unsigned skip                  = msg_tmp - &msg[i];

      if (skip > 1)
         i += skip - 1;

      /* Do something smarter here ... */
      if (!(glyph = font->font_driver->get_glyph(font->font_data, code)))
         if (!(glyph = glyph_q))
            continue;

      off_x  = glyph->draw_offset_x;
      off_y  = glyph->draw_offset_y;
      tex_x  = glyph->atlas_offset_x;
      tex_y  = glyph->atlas_offset_y;
      width  = glyph->width;
      height = glyph->height;

      if (font->atlas->dirty)
      {
        unsigned int stride    = vita2d_texture_get_stride(font->texture);
        uint8_t *tex32         = vita2d_texture_get_datap(font->texture);
        const uint8_t *frame32 = font->atlas->buffer;
        unsigned int pitch     = font->atlas->width;

        for (j = 0; j < font->atlas->height; j++)
           for (k = 0; k < font->atlas->width; k++)
              tex32[k + j*stride] = frame32[k + j * pitch];

         font->atlas->dirty = false;
      }

      vita2d_draw_texture_tint_part_scale(font->texture,
            x + (off_x + delta_x) * scale,
            y + (off_y + delta_y) * scale,
            tex_x, tex_y, width, height,
            scale,
            scale,
            color);

      delta_x += glyph->advance_x;
      delta_y += glyph->advance_y;
   }
}

static void vita2d_font_render_message(
      vita_video_t *vita,
      vita_font_t *font, const char *msg, float scale,
      const unsigned int color, float pos_x, float pos_y,
      unsigned width, unsigned height, unsigned text_align)
{
   float line_height;
   struct font_line_metrics *line_metrics = NULL;
   int lines                              = 0;
   int x                                  = roundf(pos_x * width);
   const struct font_glyph* glyph_q       = font->font_driver->get_glyph(font->font_data, '?');
   font->font_driver->get_line_metrics(font->font_data, &line_metrics);
   line_height = line_metrics->height * scale / vita->vp.height;

   for (;;)
   {
      const char *delim = strchr(msg, '\n');
      size_t msg_len    = (delim) ? (delim - msg) : strlen(msg);

      /* Draw the line */
      vita2d_font_render_line(vita, font, glyph_q, msg, msg_len,
            scale, color, pos_x, pos_y - (float)lines * line_height,
            width, height, x, text_align);

      if (!delim)
         break;

      msg += msg_len + 1;
      lines++;
   }
}

static void vita2d_font_render_msg(
      void *userdata,
      void *data,
      const char *msg,
      const struct font_params *params)
{
   int drop_x, drop_y;
   unsigned color, r, g, b, alpha;
   enum text_alignment text_align;
   float x, y, scale, drop_mod, drop_alpha;
   bool full_screen                 = false;
   vita_video_t *vita               = (vita_video_t *)userdata;
   vita_font_t *font                = (vita_font_t *)data;
   unsigned width                   = vita->video_width;
   unsigned height                  = vita->video_height;

   if (!font || !msg || !*msg)
      return;

   if (params)
   {
      x                       = params->x;
      y                       = params->y;
      scale                   = params->scale;
      full_screen             = params->full_screen;
      text_align              = params->text_align;
      drop_x                  = params->drop_x;
      drop_y                  = params->drop_y;
      drop_mod                = params->drop_mod;
      drop_alpha              = params->drop_alpha;
      r    				         = FONT_COLOR_GET_RED(params->color);
      g    				         = FONT_COLOR_GET_GREEN(params->color);
      b    				         = FONT_COLOR_GET_BLUE(params->color);
      alpha    		         = FONT_COLOR_GET_ALPHA(params->color);
      color    		         = RGBA8(r,g,b,alpha);
   }
   else
   {
      settings_t *settings    = config_get_ptr();
      float video_msg_pos_x   = settings->floats.video_msg_pos_x;
      float video_msg_pos_y   = settings->floats.video_msg_pos_y;
      float video_msg_color_r = settings->floats.video_msg_color_r;
      float video_msg_color_g = settings->floats.video_msg_color_g;
      float video_msg_color_b = settings->floats.video_msg_color_b;
      x                       = video_msg_pos_x;
      y                       = video_msg_pos_y;
      scale                   = 1.0f;
      full_screen             = true;
      text_align              = TEXT_ALIGN_LEFT;

      r                       = (video_msg_color_r * 255);
      g                       = (video_msg_color_g * 255);
      b                       = (video_msg_color_b * 255);
      alpha			            = 255;
      color 		            = RGBA8(r,g,b,alpha);

      drop_x                  = -2;
      drop_y                  = -2;
      drop_mod                = 0.3f;
      drop_alpha              = 1.0f;
   }

   vita2d_set_viewport_wrapper(vita, width, height, full_screen, false);

   if (drop_x || drop_y)
   {
      unsigned r_dark         = r * drop_mod;
      unsigned g_dark         = g * drop_mod;
      unsigned b_dark         = b * drop_mod;
      unsigned alpha_dark     = alpha * drop_alpha;
      unsigned color_dark     = RGBA8(r_dark,g_dark,b_dark,alpha_dark);

      vita2d_font_render_message(vita, font, msg, scale, color_dark,
            x + scale * drop_x / width, y +
            scale * drop_y / height, width, height, text_align);
   }

   vita2d_font_render_message(vita, font, msg, scale,
         color, x, y, width, height, text_align);
}

static const struct font_glyph *vita2d_font_get_glyph(
      void *data, uint32_t code)
{
   vita_font_t *font = (vita_font_t*)data;
   if (font && font->font_driver)
      return font->font_driver->get_glyph((void*)font->font_driver, code);
   return NULL;
}

static bool vita2d_font_get_line_metrics(void* data, struct font_line_metrics **metrics)
{
   vita_font_t *font = (vita_font_t*)data;
   if (font && font->font_driver && font->font_data)
   {
      font->font_driver->get_line_metrics(font->font_data, metrics);
      return true;
   }
   return false;
}

font_renderer_t vita2d_vita_font = {
   vita2d_font_init,
   vita2d_font_free,
   vita2d_font_render_msg,
   "vita2d",
   vita2d_font_get_glyph,
   NULL,                      /* bind_block */
   NULL,                      /* flush */
   vita2d_font_get_message_width,
   vita2d_font_get_line_metrics
};

/*
 * VIDEO DRIVER
 */

static void *vita2d_gfx_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   unsigned temp_width                    = PSP_FB_WIDTH;
   unsigned temp_height                   = PSP_FB_HEIGHT;
   vita2d_video_mode_data video_mode_data = {0};
   vita_video_t *vita                     = (vita_video_t *)
	   calloc(1, sizeof(vita_video_t));

   if (!vita)
      return NULL;

   *input             = NULL;
   *input_data        = NULL;

   vita2d_init_advanced_with_msaa((1 * 1024 * 1024), SCE_GXM_MULTISAMPLE_4X,
    sceKernelGetModelForCDialog() == SCE_KERNEL_MODEL_VITATV? VITA2D_VIDEO_MODE_1280x720 : VITA2D_VIDEO_MODE_960x544 );
   vita2d_set_clear_color(RGBA8(0x00, 0x00, 0x00, 0xFF));
   vita2d_set_vblank_wait(video->vsync);

   if (video->rgb32)
      vita->format    = SCE_GXM_TEXTURE_FORMAT_X8U8U8U8_1RGB;
   else
      vita->format    = SCE_GXM_TEXTURE_FORMAT_R5G6B5;

   video_mode_data    = vita2d_get_video_mode_data();
   temp_width         = video_mode_data.width;
   temp_height        = video_mode_data.height;

   vita->fullscreen   = video->fullscreen;

   vita->texture      = NULL;
   vita->menu.texture = NULL;
   vita->menu.active  = 0;
   vita->menu.width   = 0;
   vita->menu.height  = 0;

   vita->vsync        = video->vsync;
   vita->rgb32        = video->rgb32;

   vita->tex_filter   = video->smooth
      ? SCE_GXM_TEXTURE_FILTER_LINEAR : SCE_GXM_TEXTURE_FILTER_POINT;

   vita->video_width  = temp_width;
   vita->video_height = temp_height;

   video_driver_set_size(temp_width, temp_height);
   vita2d_set_viewport_wrapper(vita, temp_width, temp_height, false, true);

   if (input && input_data)
   {
      settings_t *settings = config_get_ptr();
      void *pspinput       = input_driver_init_wrap(&input_psp,
            settings->arrays.input_joypad_driver);
      *input               = pspinput ? &input_psp : NULL;
      *input_data          = pspinput;
   }

   vita->keep_aspect        = true;
   vita->should_resize      = true;
#ifdef HAVE_OVERLAY
   vita->overlay_enable     = false;
#endif
   font_driver_init_osd(vita,
         video,
         false,
         video->is_threaded,
         FONT_DRIVER_RENDER_VITA2D);

   return vita;
}

#ifdef HAVE_OVERLAY
static void vita2d_render_overlay(void *data);
static void vita2d_free_overlay(vita_video_t *vita)
{
   unsigned i;

   vita2d_wait_rendering_done();

   for (i = 0; i < vita->overlays; i++)
      vita2d_free_texture(vita->overlay[i].tex);
   free(vita->overlay);
   vita->overlay = NULL;
   vita->overlays = 0;
}
#endif

static bool vita2d_frame(void *data, const void *frame,
      unsigned width, unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   void *tex_p;
   vita_video_t *vita                     = (vita_video_t *)data;
   unsigned temp_width                    = PSP_FB_WIDTH;
   unsigned temp_height                   = PSP_FB_HEIGHT;
   vita2d_video_mode_data video_mode_data = {0};
#ifdef HAVE_MENU
   bool menu_is_alive                     = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
#endif
#ifdef HAVE_GFX_WIDGETS
   bool widgets_active                    = video_info->widgets_active;
#endif
   bool statistics_show                   = video_info->statistics_show;
   struct font_params *osd_params         = (struct font_params*)&video_info->osd_stat_params;

   if (frame)
   {
      if (!(vita->texture&&vita2d_texture_get_datap(vita->texture)==frame))
      {
         unsigned i;
         unsigned int stride;

         if ((width != vita->width || height != vita->height) && vita->texture)
         {
            vita2d_wait_rendering_done();
            vita2d_free_texture(vita->texture);
            vita->texture = NULL;
         }

         if (!vita->texture)
         {
            vita->width   = width;
            vita->height  = height;
            vita->texture = vita2d_create_empty_texture_format(width, height, vita->format);
            vita2d_texture_set_filters(vita->texture,vita->tex_filter,vita->tex_filter);
         }
         tex_p = vita2d_texture_get_datap(vita->texture);
         stride = vita2d_texture_get_stride(vita->texture);

         if (vita->format == SCE_GXM_TEXTURE_FORMAT_X8U8U8U8_1RGB)
         {
            stride                     /= 4;
            pitch                      /= 4;
            uint32_t             *tex32 = tex_p;
            const uint32_t     *frame32 = frame;

            for (i = 0; i < height; i++)
               memcpy_neon(&tex32[i*stride],&frame32[i*pitch],pitch*sizeof(uint32_t));
         }
         else
         {
            stride                 /= 2;
            pitch                  /= 2;
            uint16_t *tex16         = tex_p;
            const uint16_t *frame16 = frame;

            for (i = 0; i < height; i++)
               memcpy_neon(&tex16[i*stride],&frame16[i*pitch],width*sizeof(uint16_t));
         }
      }
   }

   if (vita->should_resize)
      vita2d_update_viewport(vita, video_info);

   video_mode_data = vita2d_get_video_mode_data();
   temp_width = video_mode_data.width;
   temp_height = video_mode_data.height;

   vita2d_start_drawing();

   vita2d_draw_rectangle(0,0,temp_width,temp_height,vita2d_get_clear_color());

   if (vita->texture)
   {
      if (vita->fullscreen)
         vita2d_draw_texture_scale(vita->texture,
               0, 0,
               temp_width  / (float)vita->width,
               temp_height / (float)vita->height);
      else
      {
         const float radian = 270 * 0.0174532925f;
         const float rad = vita->rotation * radian;
         float scalex = vita->vp.width / (float)vita->width;
         float scaley = vita->vp.height / (float)vita->height;
         vita2d_draw_texture_scale_rotate(vita->texture,vita->vp.x,
               vita->vp.y, scalex, scaley, rad);
      }
   }

   if (vita->menu.active)
   {
#ifdef HAVE_MENU
      menu_driver_frame(menu_is_alive, video_info);
#endif

      if (vita->menu.texture)
      {
         if (vita->fullscreen)
            vita2d_draw_texture_scale(vita->menu.texture,
                  0, 0,
                  temp_width  / (float)vita->menu.width,
                  temp_height / (float)vita->menu.height);
         else
         {
            if (vita->menu.width > vita->menu.height)
            {
               float scale = temp_height / (float)vita->menu.height;
               float w = vita->menu.width * scale;
               vita2d_draw_texture_scale(vita->menu.texture,
                     temp_width / 2.0f - w/2.0f, 0.0f,
                     scale, scale);
            }
            else
            {
               float scale = temp_width / (float)vita->menu.width;
               float h = vita->menu.height * scale;
               vita2d_draw_texture_scale(vita->menu.texture,
                     0.0f, temp_height / 2.0f - h/2.0f,
                     scale, scale);
            }
         }
      }
   }
   else if (statistics_show)
   {
      if (osd_params)
         font_driver_render_msg(vita, video_info->stat_text,
               osd_params, NULL);
   }

#ifdef HAVE_OVERLAY
   if (vita->overlay_enable)
      vita2d_render_overlay(vita);
#endif

#ifdef HAVE_GFX_WIDGETS
   if (widgets_active)
      gfx_widgets_frame(video_info);
#endif

   if (!string_is_empty(msg))
      font_driver_render_msg(vita, msg, NULL, NULL);

   vita2d_end_drawing();
   vita2d_swap_buffers();

   return true;
}

static void vita2d_set_nonblock_state(void *data, bool toggle, bool c, unsigned d)
{
   vita_video_t *vita = (vita_video_t *)data;

   if (vita)
   {
      vita->vsync = !toggle;
      vita2d_set_vblank_wait(vita->vsync);
   }
}

static bool vita2d_alive(void *data)     { return true; }
static bool vita2d_focus(void *data) { return true; }
static bool vita2d_suppress_screensaver(void *a, bool b) { return false; }

static void vita2d_free(void *data)
{
   vita_video_t *vita = (vita_video_t *)data;

   vita2d_fini();

   if (vita->menu.texture)
   {
      vita2d_free_texture(vita->menu.texture);
      vita->menu.texture = NULL;
   }

   if (vita->texture)
   {
      vita2d_free_texture(vita->texture);
      vita->texture = NULL;
   }

   font_driver_free_osd();
}

static bool vita2d_set_shader(void *data,
      enum rarch_shader_type type, const char *path) { return false; }

static void vita2d_set_projection(vita_video_t *vita,
      struct video_ortho *ortho, bool allow_rotate)
{
   static math_matrix_4x4 rot     = {
      { 0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    1.0f }
   };
   float radians, cosine, sine;

   /* Calculate projection. */
   matrix_4x4_ortho(vita->mvp_no_rot, ortho->left, ortho->right,
         ortho->bottom, ortho->top, ortho->znear, ortho->zfar);

   if (!allow_rotate)
   {
      vita->mvp = vita->mvp_no_rot;
      return;
   }

   radians                 = M_PI * vita->rotation / 180.0f;
   cosine                  = cosf(radians);
   sine                    = sinf(radians);
   MAT_ELEM_4X4(rot, 0, 0) = cosine;
   MAT_ELEM_4X4(rot, 0, 1) = -sine;
   MAT_ELEM_4X4(rot, 1, 0) = sine;
   MAT_ELEM_4X4(rot, 1, 1) = cosine;
   matrix_4x4_multiply(vita->mvp, rot, vita->mvp_no_rot);
}

static void vita2d_update_viewport(vita_video_t* vita,
      video_frame_info_t *video_info)
{
   vita2d_video_mode_data
      video_mode_data        = vita2d_get_video_mode_data();
   unsigned temp_width       = video_mode_data.width;
   unsigned temp_height      = video_mode_data.height;
   int x                     = 0;
   int y                     = 0;
   float device_aspect       = ((float)temp_width) / temp_height;
   float width               = temp_width;
   float height              = temp_height;
   settings_t *settings      = config_get_ptr();
   bool video_scale_integer  = settings->bools.video_scale_integer;
   unsigned aspect_ratio_idx = settings->uints.video_aspect_ratio_idx;

   if (video_scale_integer)
   {
      /* TODO: Does Vita use top-left or bottom-left origin?  I'm assuming top left. */
      video_viewport_get_scaled_integer(&vita->vp, temp_width,
           temp_height, video_driver_get_aspect_ratio(), vita->keep_aspect, true);
      width  = vita->vp.width;
      height = vita->vp.height;
   }
   else if (vita->keep_aspect)
   {
      float desired_aspect = video_driver_get_aspect_ratio();
      if ( (vita->rotation == ORIENTATION_VERTICAL) ||
           (vita->rotation == ORIENTATION_FLIPPED_ROTATED))
      {
         device_aspect = 1.0 / device_aspect;
         width = temp_height;
         height = temp_width;
      }
      video_viewport_get_scaled_aspect(&vita->vp, width, height, true);
      if ( (vita->rotation == ORIENTATION_VERTICAL) ||
           (vita->rotation == ORIENTATION_FLIPPED_ROTATED)
         )
      {
         // swap x and y
         unsigned tmp = vita->vp.x;
         vita->vp.x = vita->vp.y;
         vita->vp.y = tmp;
      }
   }
   else
   {
      vita->vp.x      = 0;
      vita->vp.y      = 0;
      vita->vp.width  = width;
      vita->vp.height = height;
   }

   vita->vp.width      += vita->vp.width&0x1;
   vita->vp.height     += vita->vp.height&0x1;

   vita->should_resize  = false;

}

static void vita2d_set_viewport_wrapper(void *data, unsigned viewport_width,
      unsigned viewport_height, bool force_full, bool allow_rotate)
{
   int x                     = 0;
   int y                     = 0;
   float device_aspect       = (float)viewport_width / viewport_height;
   struct video_ortho ortho  = {0, 1, 0, 1, -1, 1};
   settings_t *settings      = config_get_ptr();
   vita_video_t *vita        = (vita_video_t*)data;
   bool video_scale_integer  = settings->bools.video_scale_integer;
   unsigned aspect_ratio_idx = settings->uints.video_aspect_ratio_idx;

   if (video_scale_integer && !force_full)
   {
      /* TODO: Does Vita use top-left or bottom-left origin?  I'm assuming top left. */
      video_viewport_get_scaled_integer(&vita->vp,
            viewport_width, viewport_height,
            video_driver_get_aspect_ratio(), vita->keep_aspect, true);
      viewport_width  = vita->vp.width;
      viewport_height = vita->vp.height;
   }
   else if (vita->keep_aspect && !force_full)
   {
      float desired_aspect = video_driver_get_aspect_ratio();

#if defined(HAVE_MENU)
      if (aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         video_viewport_t *custom_vp = &settings->video_viewport_custom;
         x                           = custom_vp->x;
         y                           = custom_vp->y;
         viewport_width              = custom_vp->width;
         viewport_height             = custom_vp->height;
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
            float viewport_bias = settings->floats.video_viewport_bias_x;
            delta          = (desired_aspect / device_aspect - 1.0f)
               / 2.0f + 0.5f;
            x              = (int)roundf(viewport_width * ((0.5f - delta) * (viewport_bias * 2.0f)));
            viewport_width = (unsigned)roundf(2.0f * viewport_width * delta);
         }
         else
         {
            /* TODO: Does Vita use top-left or bottom-left origin?  I'm assuming top left. */
            float viewport_bias = settings->floats.video_viewport_bias_y;
            delta           = (device_aspect / desired_aspect - 1.0f)
               / 2.0f + 0.5f;
            y               = (int)roundf(viewport_height * ((0.5f - delta) * (viewport_bias * 2.0f)));
            viewport_height = (unsigned)roundf(2.0f * viewport_height * delta);
         }
      }

      vita->vp.x      = x;
      vita->vp.y      = y;
      vita->vp.width  = viewport_width;
      vita->vp.height = viewport_height;
   }
   else
   {
      vita->vp.x      = 0;
      vita->vp.y      = 0;
      vita->vp.width  = viewport_width;
      vita->vp.height = viewport_height;
   }

   vita2d_set_viewport(vita->vp.x, vita->vp.y, vita->vp.width, vita->vp.height);
   vita2d_set_projection(vita, &ortho, allow_rotate);

   /* Set last backbuffer viewport. */
   if (!force_full)
   {
      vita->vp.width  = viewport_width;
      vita->vp.height = viewport_height;
   }
}

static void vita2d_set_rotation(void *data,
      unsigned rotation)
{
  vita_video_t *vita       = (vita_video_t*)data;
  struct video_ortho ortho = {0, 1, 0, 1, -1, 1};

  if (!vita)
     return;

  vita->rotation           = rotation;
  vita->should_resize      = true;
  vita2d_set_projection(vita, &ortho, true);

}

static void vita2d_viewport_info(void *data,
      struct video_viewport *vp)
{
    vita_video_t *vita = (vita_video_t*)data;

    if (vita)
       *vp = vita->vp;
}

static void vita2d_set_filtering(void *data, unsigned index, bool smooth, bool ctx_scaling)
{
   vita_video_t *vita = (vita_video_t *)data;

   if (vita)
   {
      vita->tex_filter = smooth
         ? SCE_GXM_TEXTURE_FILTER_LINEAR
         : SCE_GXM_TEXTURE_FILTER_POINT;
      vita2d_texture_set_filters(vita->texture,vita->tex_filter,
            vita->tex_filter);
   }
}

static void vita2d_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   vita_video_t *vita  = (vita_video_t*)data;

   if (!vita)
      return;
   vita->keep_aspect   = true;
   vita->should_resize = true;
}

static void vita2d_apply_state_changes(void *data)
{
  vita_video_t *vita = (vita_video_t*)data;

  if (vita)
     vita->should_resize = true;
}

static void vita2d_set_texture_frame(void *data, const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   int i, j;
   void *tex_p;
   unsigned int stride;
   vita_video_t *vita = (vita_video_t*)data;

   if (     (width  != vita->menu.width)
         && (height != vita->menu.height)
         && vita->menu.texture)
   {
      vita2d_wait_rendering_done();
      vita2d_free_texture(vita->menu.texture);
      vita->menu.texture = NULL;
   }

   if (!vita->menu.texture)
   {
      if (rgb32)
         vita->menu.texture = vita2d_create_empty_texture(width, height);
      else
         vita->menu.texture = vita2d_create_empty_texture_format(
               width, height, SCE_GXM_TEXTURE_FORMAT_U4U4U4U4_RGBA);
      vita->menu.width      = width;
      vita->menu.height     = height;
   }

   vita2d_texture_set_filters(vita->menu.texture,
         SCE_GXM_TEXTURE_FILTER_LINEAR,
         SCE_GXM_TEXTURE_FILTER_LINEAR);

   tex_p  = vita2d_texture_get_datap(vita->menu.texture);
   stride = vita2d_texture_get_stride(vita->menu.texture);

   if (rgb32)
   {
      uint32_t         *tex32 = tex_p;
      const uint32_t *frame32 = frame;

      stride                 /= 4;

      for (i = 0; i < height; i++)
         for (j = 0; j < width; j++)
            tex32[j + i*stride] = frame32[j + i*width];
   }
   else
   {
      uint16_t               *tex16 = tex_p;
      const uint16_t       *frame16 = frame;

      stride                       /= 2;

      for (i = 0; i < height; i++)
         for (j = 0; j < width; j++)
            tex16[j + i*stride] = frame16[j + i*width];
   }
}

static void vita2d_set_texture_enable(void *data, bool state, bool full_screen)
{
   vita_video_t *vita = (vita_video_t*)data;
   vita->menu.active  = state;
}

static uintptr_t vita2d_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   unsigned int stride, pitch, j;
   uint32_t             *tex32    = NULL;
   const uint32_t *frame32        = NULL;
   struct texture_image *image    = (struct texture_image*)data;
   struct vita2d_texture *texture = vita2d_create_empty_texture_format(image->width,
     image->height,SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ARGB);

   if (!texture)
      return 0;

   if (    (filter_type == TEXTURE_FILTER_MIPMAP_LINEAR)
        || (filter_type == TEXTURE_FILTER_LINEAR))
      vita2d_texture_set_filters(texture,
            SCE_GXM_TEXTURE_FILTER_LINEAR,
            SCE_GXM_TEXTURE_FILTER_LINEAR);

   stride                      = vita2d_texture_get_stride(texture);
   stride                     /= 4;

   tex32                       = vita2d_texture_get_datap(texture);
   frame32                     = image->pixels;
   pitch                       = image->width;

   for (j = 0; j < image->height; j++)
         memcpy_neon(
               &tex32[j*stride],
               &frame32[j*pitch],
               pitch * sizeof(uint32_t));

   return (uintptr_t)texture;
}

static void vita2d_unload_texture(void *data,
      bool threaded, uintptr_t handle)
{
   struct vita2d_texture *texture = (struct vita2d_texture*)handle;
   if (!texture)
      return;

   /* TODO: We really want to defer this deletion instead,
    * but this will do for now. */
   vita2d_wait_rendering_done();
   vita2d_free_texture(texture);

#if 0
   free(texture);
#endif
}

static bool vita2d_get_current_sw_framebuffer(void *data,
      struct retro_framebuffer *framebuffer)
{
   vita_video_t *vita = (vita_video_t*)data;

   if (     !vita->texture
         || (vita->width  != framebuffer->width)
         || (vita->height != framebuffer->height))
   {
      if (vita->texture)
      {
         vita2d_wait_rendering_done();
         vita2d_free_texture(vita->texture);
         vita->texture = NULL;
      }

      vita->width   = framebuffer->width;
      vita->height  = framebuffer->height;
      vita->texture = vita2d_create_empty_texture_format(
            vita->width, vita->height, vita->format);
      vita2d_texture_set_filters(vita->texture,
            vita->tex_filter,vita->tex_filter);
   }

   framebuffer->data         = vita2d_texture_get_datap(vita->texture);
   framebuffer->pitch        = vita2d_texture_get_stride(vita->texture);
   framebuffer->format       = vita->rgb32
      ? RETRO_PIXEL_FORMAT_XRGB8888
      : RETRO_PIXEL_FORMAT_RGB565;
   framebuffer->memory_flags = 0;

   return true;
}

static uint32_t vita2d_get_flags(void *data)
{
   uint32_t             flags   = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_SCREENSHOTS_SUPPORTED);

   return flags;
}

static const video_poke_interface_t vita_poke_interface = {
   vita2d_get_flags,
   vita2d_load_texture,
   vita2d_unload_texture,
   NULL, /* set_video_mode */
   NULL, /* get_refresh_rate */
   vita2d_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   vita2d_set_aspect_ratio,
   vita2d_apply_state_changes,
   vita2d_set_texture_frame,
   vita2d_set_texture_enable,
   font_driver_render_msg,
   NULL, /* show_mouse */
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   vita2d_get_current_sw_framebuffer,
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_max_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_contrast */
   NULL  /* set_hdr_expand_gamut */
 };

static void vita2d_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   *iface = &vita_poke_interface;
}

#ifdef HAVE_GFX_WIDGETS
static bool vita2d_widgets_enabled(void *data) { return true; }
#endif

#ifdef HAVE_OVERLAY
static void vita2d_overlay_tex_geom(void *data, unsigned image, float x, float y, float w, float h);
static void vita2d_overlay_vertex_geom(void *data, unsigned image, float x, float y, float w, float h);

static bool vita2d_overlay_load(void *data, const void *image_data, unsigned num_images)
{
   unsigned i,j,k;
   unsigned int stride, pitch;
   vita_video_t *vita = (vita_video_t*)data;
   const struct texture_image *images = (const struct texture_image*)image_data;

   vita2d_free_overlay(vita);
   vita->overlay = (struct vita_overlay_data*)calloc(num_images, sizeof(*vita->overlay));
   if (!vita->overlay)
      return false;

   vita->overlays = num_images;

   for (i = 0; i < num_images; i++)
   {
      uint32_t *tex32;
      const uint32_t *frame32;
      struct vita_overlay_data *o = (struct vita_overlay_data*)&vita->overlay[i];
      o->width   = images[i].width;
      o->height  = images[i].height;
      o->tex     = vita2d_create_empty_texture_format(o->width , o->height, SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ARGB);
      vita2d_texture_set_filters(o->tex,SCE_GXM_TEXTURE_FILTER_LINEAR,SCE_GXM_TEXTURE_FILTER_LINEAR);
      stride     = vita2d_texture_get_stride(o->tex);
      stride    /= 4;
      tex32      = vita2d_texture_get_datap(o->tex);
      frame32    = images[i].pixels;
      pitch      = o->width;
      for (j = 0; j < o->height; j++)
         for (k = 0; k < o->width; k++)
            tex32[k + j*stride] = frame32[k + j*pitch];

      vita2d_overlay_tex_geom(vita, i, 0, 0, 1, 1); /* Default. Stretch to whole screen. */
      vita2d_overlay_vertex_geom(vita, i, 0, 0, 1, 1);
      vita->overlay[i].alpha_mod = 1.0f;
   }

   return true;
}

static void vita2d_overlay_tex_geom(void *data, unsigned image,
      float x, float y, float w, float h)
{
   vita_video_t          *vita = (vita_video_t*)data;
   struct vita_overlay_data *o = NULL;

   if (vita)
      o = (struct vita_overlay_data*)&vita->overlay[image];

   if (o)
   {
      o->tex_x = x;
      o->tex_y = y;
      o->tex_w = w*o->width;
      o->tex_h = h*o->height;
   }
}

static void vita2d_overlay_vertex_geom(void *data, unsigned image,
         float x, float y, float w, float h)
{
   vita_video_t          *vita = (vita_video_t*)data;
   struct vita_overlay_data *o = NULL;

   /* Flipped, so we preserve top-down semantics. */
   /* y = 1.0f - y;
      h = -h;
    */

   if (vita)
      o = (struct vita_overlay_data*)&vita->overlay[image];

   if (o)
   {
      vita2d_video_mode_data video_mode_data = vita2d_get_video_mode_data();
      o->w = w * video_mode_data.width  / o->width;
      o->h = h * video_mode_data.height / o->height;
      o->x = video_mode_data.width  * (1 - w) / 2 + x;
      o->y = video_mode_data.height * (1 - h) / 2 + y;
   }
}

static void vita2d_overlay_enable(void *data, bool state)
{
   vita_video_t *vita   = (vita_video_t*)data;
   vita->overlay_enable = state;
}

static void vita2d_overlay_full_screen(void *data, bool enable)
{
   vita_video_t *vita        = (vita_video_t*)data;
   vita->overlay_full_screen = enable;
}

static void vita2d_overlay_set_alpha(void *data, unsigned image, float mod)
{
   vita_video_t *vita             = (vita_video_t*)data;
   vita->overlay[image].alpha_mod = mod;
}

static void vita2d_render_overlay(void *data)
{
   unsigned i;
   vita_video_t *vita = (vita_video_t*)data;

   for (i = 0; i < vita->overlays; i++)
   {
      vita2d_draw_texture_tint_part_scale(vita->overlay[i].tex,
            vita->overlay[i].x,
            vita->overlay[i].y,
            vita->overlay[i].tex_x,
            vita->overlay[i].tex_y,
            vita->overlay[i].tex_w,
            vita->overlay[i].tex_h,
            vita->overlay[i].w,
            vita->overlay[i].h,
            RGBA8(0xFF,0xFF,0xFF,(uint8_t)(vita->overlay[i].alpha_mod * 255.0f)));
   }
}

static const video_overlay_interface_t vita2d_overlay_interface = {
   vita2d_overlay_enable,
   vita2d_overlay_load,
   vita2d_overlay_tex_geom,
   vita2d_overlay_vertex_geom,
   vita2d_overlay_full_screen,
   vita2d_overlay_set_alpha,
};

static void vita2d_get_overlay_interface(void *data, const video_overlay_interface_t **iface)
{
   *iface = &vita2d_overlay_interface;
}
#endif

video_driver_t video_vita2d = {
   vita2d_gfx_init,
   vita2d_frame,
   vita2d_set_nonblock_state,
   vita2d_alive,
   vita2d_focus,
   vita2d_suppress_screensaver,
   NULL, /* has_windowed */
   vita2d_set_shader,
   vita2d_free,
   "vita2d",
   vita2d_set_viewport_wrapper,
   vita2d_set_rotation,
   vita2d_viewport_info,
   NULL, /* read_viewport */
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   vita2d_get_overlay_interface,
#endif
   vita2d_get_poke_interface,
   NULL, /* wrap_type_to_enum */
#ifdef HAVE_GFX_WIDGETS
   vita2d_widgets_enabled
#endif
};
