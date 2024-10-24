/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018      - misson20000
 *  Copyright (C) 2018      - m4xw
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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <time.h>

#include <retro_inline.h>
#include <retro_math.h>
#include <formats/image.h>
#include <encodings/utf.h>

#include <gfx/scaler/scaler.h>
#include <gfx/scaler/pixconv.h>
#include <gfx/video_frame.h>

#include <switch.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../font_driver.h"

#include "../../configuration.h"
#include "../../command.h"
#include "../../driver.h"

#include "../../retroarch.h"
#include "../../verbosity.h"

#include "../common/switch_defines.h"

#ifndef HAVE_THREADS
#include "../../tasks/tasks_internal.h"
#endif

/*
 * DISPLAY DRIVER
 */

static void gfx_display_switch_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height) { }

static const float *gfx_display_switch_get_default_vertices(void)
{
   static float dummy[16] = {0.0f};
   return &dummy[0];
}

static const float *gfx_display_switch_get_default_tex_coords(void)
{
   static float dummy[16] = {0.0f};
   return &dummy[0];
}

gfx_display_ctx_driver_t gfx_display_ctx_switch = {
   gfx_display_switch_draw,
   NULL,                                        /* draw_pipeline   */
   NULL,                                        /* blend_begin     */
   NULL,                                        /* blend_end       */
   NULL,                                        /* get_default_mvp */
   gfx_display_switch_get_default_vertices,
   gfx_display_switch_get_default_tex_coords,
   FONT_DRIVER_RENDER_SWITCH,
   GFX_VIDEO_DRIVER_SWITCH,
   "switch",
   false,
   NULL,                                         /* scissor_begin */
   NULL                                          /* scissor_end   */
};

/*
 * FONT DRIVER
 */

#define AVG_GLPYH_LIMIT 140

typedef struct
{
   struct font_atlas *atlas;

   const font_renderer_driver_t *font_driver;
   void *font_data;
} switch_font_t;

static void *switch_font_init(void *data, const char *font_path,
      float font_size, bool is_threaded)
{
   switch_font_t *font = (switch_font_t *)calloc(1, sizeof(switch_font_t));

   if (!font)
      return NULL;

   if (!font_renderer_create_default(&font->font_driver,
            &font->font_data, font_path, font_size))
   {
      free(font);
      return NULL;
   }

   font->atlas = font->font_driver->get_atlas(font->font_data);

   return font;
}

static void switch_font_free(void *data, bool is_threaded)
{
   switch_font_t *font = (switch_font_t *)data;

   if (!font)
      return;

   if (font->font_driver && font->font_data)
      font->font_driver->free(font->font_data);

   free(font);
}

static int switch_font_get_message_width(void *data, const char *msg,
      size_t msg_len, float scale)
{
   int i;
   const struct font_glyph* glyph_q = NULL;
   int         delta_x = 0;
   switch_font_t *font = (switch_font_t *)data;

   if (!font)
      return 0;

   glyph_q = font->font_driver->get_glyph(font->font_data, '?');

   for (i = 0; i < msg_len; i++)
   {
      const struct font_glyph *glyph;
      const char *msg_tmp = &msg[i];
      unsigned       code = utf8_walk(&msg_tmp);
      unsigned       skip = msg_tmp - &msg[i];

      if (skip > 1)
         i += skip - 1;

      /* Do something smarter here ... */
      if (!(glyph =
               font->font_driver->get_glyph(font->font_data, code)))
         if (!(glyph = glyph_q))
            continue;

      delta_x += glyph->advance_x;
   }

   return delta_x * scale;
}

static void switch_font_render_line(
      switch_video_t *sw,
      switch_font_t *font, const char *msg, size_t msg_len,
      float scale, const unsigned int color, float pos_x,
      float pos_y, unsigned text_align)
{
   int i;
   const struct font_glyph* glyph_q = NULL;
   int delta_x                      = 0;
   int delta_y                      = 0;
   unsigned fb_width                = sw->vp.full_width;
   unsigned fb_height               = sw->vp.full_height;
   int x                            = roundf(pos_x * fb_width);
   int y                            = roundf((1.0f - pos_y) * fb_height);

   switch (text_align)
   {
      case TEXT_ALIGN_RIGHT:
         x -= switch_font_get_message_width(font, msg, msg_len, scale);
         break;
      case TEXT_ALIGN_CENTER:
         x -= switch_font_get_message_width(font, msg, msg_len, scale) / 2;
         break;
   }

   glyph_q = font->font_driver->get_glyph(font->font_data, '?');

   for (i = 0; i < msg_len; i++)
   {
      const struct font_glyph *glyph;
      int off_x, off_y, tex_x, tex_y, width, height;
      const char *msg_tmp = &msg[i];
      unsigned code       = utf8_walk(&msg_tmp);
      unsigned skip       = msg_tmp - &msg[i];

      if (skip > 1)
         i               += skip - 1;

      /* Do something smarter here ... */
      if (!(glyph =
               font->font_driver->get_glyph(font->font_data, code)))
         if (!(glyph = glyph_q))
            continue;

      off_x  = x + glyph->draw_offset_x + delta_x;
      off_y  = y + glyph->draw_offset_y + delta_y;
      width  = glyph->width;
      height = glyph->height;

      tex_x = glyph->atlas_offset_x;
      tex_y = glyph->atlas_offset_y;

      for (y = tex_y; y < tex_y + height; y++)
      {
         int x;
         uint8_t *row = &font->atlas->buffer[y * font->atlas->width];
         for (x = tex_x; x < tex_x + width; x++)
         {
            int x1, y1;
            if (!row[x])
               continue;
            x1 = off_x + (x - tex_x);
            y1 = off_y + (y - tex_y);
            if (x1 < fb_width && y1 < fb_height)
               sw->out_buffer[y1 * sw->stride / sizeof(uint32_t) + x1] = color;
         }
      }

      delta_x += glyph->advance_x;
      delta_y += glyph->advance_y;
   }
}

static void switch_font_render_message(
      switch_video_t *sw,
      switch_font_t *font, const char *msg, float scale,
      const unsigned int color, float pos_x, float pos_y,
      unsigned text_align)
{
   float line_height;
   struct font_line_metrics *line_metrics = NULL;
   int lines                              = 0;
   font->font_driver->get_line_metrics(font->font_data, &line_metrics);
   line_height = scale / line_metrics->height;

   for (;;)
   {
      const char *delim = strchr(msg, '\n');
      size_t msg_len    = delim ? (delim - msg) : strlen(msg);

      /* Draw the line */
      if (msg_len <= AVG_GLPYH_LIMIT)
         switch_font_render_line(sw, font, msg, msg_len,
               scale, color, pos_x, pos_y - (float)lines * line_height,
               text_align);

      if (!delim)
         break;

      msg += msg_len + 1;
      lines++;
   }
}

static void switch_font_render_msg(
      void *userdata,
      void *data,
      const char *msg,
      const struct font_params *params)
{
   float x, y, scale;
   enum text_alignment text_align;
   unsigned color, r, g, b, alpha;
   switch_font_t *font              = (switch_font_t *)data;
   switch_video_t *sw               = (switch_video_t*)userdata;
   settings_t *settings             = config_get_ptr();
   float video_msg_color_r          = settings->floats.video_msg_color_r;
   float video_msg_color_g          = settings->floats.video_msg_color_g;
   float video_msg_color_b          = settings->floats.video_msg_color_b;

   if (!font || !msg || (msg && !*msg))
      return;
   if (!sw || !sw->out_buffer)
      return;

   if (params)
   {
      x          = params->x;
      y          = params->y;
      scale      = params->scale;
      text_align = params->text_align;

      r          = FONT_COLOR_GET_RED(params->color);
      g          = FONT_COLOR_GET_GREEN(params->color);
      b          = FONT_COLOR_GET_BLUE(params->color);
      alpha      = FONT_COLOR_GET_ALPHA(params->color);

      color      = params->color;
   }
   else
   {
      x          = 0.0f;
      y          = 0.0f;
      scale      = 1.0f;
      text_align = TEXT_ALIGN_LEFT;

      r          = (video_msg_color_r * 255);
      g          = (video_msg_color_g * 255);
      b          = (video_msg_color_b * 255);
      alpha      = 255;
      color      = COLOR_ABGR(r, g, b, alpha);

   }

   switch_font_render_message(sw, font, msg, scale,
         color, x, y, text_align);
}

static const struct font_glyph *switch_font_get_glyph(
    void *data, uint32_t code)
{
   switch_font_t *font = (switch_font_t *)data;
   if (font && font->font_driver)
      return font->font_driver->get_glyph((void *)font->font_driver, code);
   return NULL;
}

static bool switch_font_get_line_metrics(void* data, struct font_line_metrics **metrics)
{
   switch_font_t *font = (switch_font_t *)data;
   if (font && font->font_driver && font->font_data)
   {
      font->font_driver->get_line_metrics(font->font_data, metrics);
      return true;
   }
   return false;
}

font_renderer_t switch_font =
{
   switch_font_init,
   switch_font_free,
   switch_font_render_msg,
   "switch",
   switch_font_get_glyph,
   NULL, /* bind_block  */
   NULL, /* flush_block */
   switch_font_get_message_width,
   switch_font_get_line_metrics
};

/*
 * VIDEO DRIVER
 */

#if 0
/* (C) libtransistor */
static int pdep(uint32_t mask, uint32_t value)
{
   int shift;
   uint32_t out = 0;
   for (shift = 0; shift < 32; shift++)
   {
      uint32_t bit = 1u << shift;
      if (mask & bit)
      {
         if (value & 1)
            out |= bit;
         value >>= 1;
      }
   }
   return out;
}

static uint32_t swizzle_x(uint32_t v) { return pdep(~0x7B4u, v); }
static uint32_t swizzle_y(uint32_t v) { return pdep(0x7B4, v); }

static void gfx_slow_swizzling_blit(uint32_t *buffer, uint32_t *image, int w, int h, int tx, int ty, bool blend)
{
    uint32_t *dest = buffer;
    uint32_t *src = image;
    int x0 = tx;
    int y0 = ty;
    int x1 = x0 + w;
    int y1 = y0 + h;
    const uint32_t tile_height = 128;
    const uint32_t padded_width = 128 * 10;

    /* we're doing this in pixels - should just shift the swizzles instead */
    uint32_t offs_x0 = swizzle_x(x0);
    uint32_t offs_y = swizzle_y(y0);
    uint32_t x_mask = swizzle_x(~0u);
    uint32_t y_mask = swizzle_y(~0u);
    uint32_t incr_y = swizzle_x(padded_width);

    /* step offs_x0 to the right row of tiles */
    offs_x0 += incr_y * (y0 / tile_height);

    uint32_t x, y;
    for (y = y0; y < y1; y++)
    {
        uint32_t *dest_line = dest + offs_y;
        uint32_t offs_x = offs_x0;

        for (x = x0; x < x1; x++)
        {
            uint32_t pixel = *src++;
            if (blend) /* supercheap masking */
            {
                uint32_t dst = dest_line[offs_x];
                uint8_t src_a = ((pixel & 0xFF000000) >> 24);

                if (src_a > 0)
                    pixel &= 0x00FFFFFF;
                else
                    pixel = dst;
            }

            dest_line[offs_x] = pixel;

            offs_x = (offs_x - x_mask) & x_mask;
        }

        offs_y = (offs_y - y_mask) & y_mask;
        if (!offs_y)
            offs_x0 += incr_y; /* wrap into next tile row */
    }
}
#endif

static void gfx_cpy_dsp_buf(uint32_t *buffer, uint32_t *image, int w, int h, uint32_t stride, bool blend)
{
    uint32_t *dest = buffer;
    uint32_t *src = image;
    for (uint32_t y = 0; y < h; y ++)
    {
        for (uint32_t x = 0; x < w; x ++)
        {
            uint32_t pos = y * stride / sizeof(uint32_t) + x;
            uint32_t pixel = *src;

            if (blend) /* supercheap masking */
            {
                uint32_t dst = dest[pos];
                uint8_t src_a = ((pixel & 0xFF000000) >> 24);

                if (src_a > 0)
                    pixel &= 0x00FFFFFF;
                else
                    pixel = dst;
            }

            dest[pos] = pixel;

            src++;
        }
    }
}

/* needed to clear surface completely as hw scaling doesn't always scale to full resolution perflectly */
static void clear_screen(switch_video_t *sw)
{
    nwindowSetDimensions(sw->win, sw->vp.full_width, sw->vp.full_height);

    uint32_t stride;

    uint32_t *out_buffer = (uint32_t*)framebufferBegin(&sw->fb, &stride);

    memset(out_buffer, 0, stride * 720);

    framebufferEnd(&sw->fb);
}

static void *switch_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
    void  *switchinput = NULL;
    switch_video_t *sw = (switch_video_t *)calloc(1, sizeof(*sw));
    if (!sw)
        return NULL;

   sw->win = nwindowGetDefault();

   framebufferCreate(&sw->fb, sw->win, 1280, 720, PIXEL_FORMAT_RGBA_8888, 2);
   framebufferMakeLinear(&sw->fb);

    sw->vp.x            = 0;
    sw->vp.y            = 0;
    sw->vp.width        = sw->o_width = video->width;
    sw->vp.height       = sw->o_height = video->height;
    sw->overlay_enabled = false;
    sw->overlay         = NULL;
#ifdef HAVE_MENU
    sw->in_menu         = false;
#endif

    sw->vp.full_width   = 1280;
    sw->vp.full_height  = 720;

    /* Sanity check */
    sw->vp.width = MIN(sw->vp.width, sw->vp.full_width);
    sw->vp.height = MIN(sw->vp.height, sw->vp.full_height);

    sw->vsync = video->vsync;
    sw->rgb32 = video->rgb32;
    sw->keep_aspect = true;
    sw->should_resize = true;
    sw->o_size = true;
    sw->is_threaded = video->is_threaded;
    sw->smooth = video->smooth;
    sw->menu_texture.enable = false;

    /* Autoselect driver */
    if (input && input_data)
    {
        settings_t *settings = config_get_ptr();
        switchinput          = input_driver_init_wrap(&input_switch,
              settings->arrays.input_joypad_driver);
        *input               = switchinput ? &input_switch : NULL;
        *input_data          = switchinput;
    }

    font_driver_init_osd(sw,
          video,
          false,
          video->is_threaded,
          FONT_DRIVER_RENDER_SWITCH);

    clear_screen(sw);

    return sw;
}

static void switch_update_viewport(switch_video_t *sw,
            video_frame_info_t *video_info)
{
    settings_t *settings = config_get_ptr();
    float desired_aspect = 0.0f;
    float width          = sw->vp.full_width;
    float height         = sw->vp.full_height;

    if (sw->o_size)
    {
        width         = sw->o_width;
        height        = sw->o_height;
        sw->vp.x      = (int)(((float)sw->vp.full_width - width)) / 2;
        sw->vp.y      = (int)(((float)sw->vp.full_height - height)) / 2;

        sw->vp.width  = width;
        sw->vp.height = height;

        return;
    }

    desired_aspect = video_driver_get_aspect_ratio();

    /* TODO/FIXME: Does nx use top-left or bottom-left origin?  I'm assuming top left. */
    if (settings->bools.video_scale_integer)
       video_viewport_get_scaled_integer(&sw->vp, sw->vp.full_width, sw->vp.full_height,
             desired_aspect, sw->keep_aspect, true);
    else if (sw->keep_aspect)
       video_viewport_get_scaled_aspect(&sw->vp, width, height, true);
    else
    {
        sw->vp.x      = sw->vp.y = 0;
        sw->vp.width  = width;
        sw->vp.height = height;
    }
}

static void switch_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
    settings_t *settings = config_get_ptr();
    switch_video_t *sw   = (switch_video_t *)data;

    if (!sw)
        return;

    sw->keep_aspect      = true;
    sw->o_size           = false;

    switch (aspect_ratio_idx)
    {
       case ASPECT_RATIO_CORE:
          sw->o_size      = true;
          sw->keep_aspect = false;
          break;

       case ASPECT_RATIO_CUSTOM:
          if (settings->bools.video_scale_integer)
          {
             video_driver_set_viewport_core();
             sw->o_size      = true;
             sw->keep_aspect = false;
          }
          break;

       default:
          break;
    }


    sw->should_resize = true;
}

static bool switch_frame(void *data, const void *frame,
      unsigned width, unsigned height,
      uint64_t frame_count, unsigned pitch,
      const char *msg, video_frame_info_t *video_info)
{
   uint32_t stride;
   switch_video_t   *sw = data;
   uint32_t *out_buffer = NULL;
   bool       ffwd_mode = video_info->input_driver_nonblock_state;
#ifdef HAVE_MENU
   bool menu_is_alive   = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
#endif
   struct font_params
      *osd_params       = (struct font_params *)&video_info->osd_stat_params;
   bool statistics_show = video_info->statistics_show;

   if (!frame)
      return true;

   if (ffwd_mode && !sw->is_threaded)
   {
      /* render every 4th frame when in ffwd mode and not threaded */
      if ((frame_count % 4) != 0)
         return true;
   }

   if (     sw->should_resize
         || (width  != sw->last_width)
         || (height != sw->last_height))
   {
      switch_update_viewport(sw, video_info);

      /* Sanity check */
      sw->vp.width  = MIN(sw->vp.width, sw->vp.full_width);
      sw->vp.height = MIN(sw->vp.height, sw->vp.full_height);

      scaler_ctx_gen_reset(&sw->scaler);

      sw->scaler.in_width  = width;
      sw->scaler.in_height = height;
      sw->scaler.in_stride = pitch;
      sw->scaler.in_fmt    = sw->rgb32
         ? SCALER_FMT_ARGB8888
         : SCALER_FMT_RGB565;

      if (!sw->smooth)
      {
         sw->scaler.out_width  = sw->vp.width;
         sw->scaler.out_height = sw->vp.height;
         sw->scaler.out_stride = sw->vp.full_width * sizeof(uint32_t);
      }
      else
      {
         sw->scaler.out_width  = width;
         sw->scaler.out_height = height;
         sw->scaler.out_stride = width * sizeof(uint32_t);

         float screen_ratio    = (float)sw->vp.full_width / sw->vp.full_height;
         float tgt_ratio       = (float)sw->vp.width / sw->vp.height;

         sw->hw_scale.width    = ceil(screen_ratio / tgt_ratio * sw->scaler.out_width);
         sw->hw_scale.height   = sw->scaler.out_height;
         sw->hw_scale.x_offset = ceil((sw->hw_scale.width - sw->scaler.out_width) / 2.0);
#ifdef HAVE_MENU
         if (!menu_is_alive)
#endif
         {
            clear_screen(sw);
            nwindowSetDimensions(sw->win, sw->hw_scale.width, sw->hw_scale.height);
         }
      }

      sw->scaler.out_fmt     = SCALER_FMT_ABGR8888;
      sw->scaler.scaler_type = SCALER_TYPE_POINT;

      if (!scaler_ctx_gen_filter(&sw->scaler))
         return false;

      sw->last_width         = width;
      sw->last_height        = height;

      sw->should_resize      = false;
   }

   out_buffer     = (uint32_t*)framebufferBegin(&sw->fb, &stride);
   sw->out_buffer = out_buffer;
   sw->stride     = stride;

#ifdef HAVE_MENU
   if (sw->in_menu && !menu_is_alive && sw->smooth)
   {
      memset(out_buffer, 0, stride * sw->vp.full_height);
      nwindowSetDimensions(sw->win, sw->hw_scale.width, sw->hw_scale.height);
   }

   sw->in_menu = menu_is_alive;

   if (sw->menu_texture.enable)
   {
      menu_driver_frame(menu_is_alive, video_info);

      if (sw->menu_texture.pixels)
      {
         memset(out_buffer, 0, stride * sw->vp.full_height);
         scaler_ctx_scale(&sw->menu_texture.scaler,
                 sw->tmp_image     + ((sw->vp.full_height - sw->menu_texture.tgth) / 2)
               * sw->vp.full_width + ((sw->vp.full_width  - sw->menu_texture.tgtw) / 2),
               sw->menu_texture.pixels);
         gfx_cpy_dsp_buf(out_buffer, sw->tmp_image, sw->vp.full_width, sw->vp.full_height, stride, true);
      }
   }
   else
#endif
      if (sw->smooth) /* bilinear */
      {
         int w, h;
         unsigned x, y;
         struct scaler_ctx *ctx = &sw->scaler;
         scaler_ctx_scale_direct(ctx, sw->image, frame);
         w = sw->scaler.out_width;
         h = sw->scaler.out_height;

         for (y = 0; y < h; y++)
            for (x = 0; x < w; x++)
               out_buffer[y * stride / sizeof(uint32_t) + (x + sw->hw_scale.x_offset)] = sw->image[y * w + x];
      }
      else
      {
         struct scaler_ctx *ctx = &sw->scaler;
         scaler_ctx_scale(ctx, sw->image + (sw->vp.y * sw->vp.full_width) + sw->vp.x, frame);
         gfx_cpy_dsp_buf(out_buffer, sw->image, sw->vp.full_width, sw->vp.full_height, stride, false);
      }

   if (statistics_show && !sw->smooth)
   {
      if (osd_params)
         font_driver_render_msg(sw, video_info->stat_text,
               osd_params, NULL);
   }

   if (msg)
      font_driver_render_msg(sw, msg, NULL, NULL);

   framebufferEnd(&sw->fb);

   return true;
}

static void switch_set_nonblock_state(void *data, bool toggle, bool c, unsigned d)
{
    switch_video_t *sw = data;
    sw->vsync = !toggle;
}

static bool switch_alive(void *data) { return true; }
static bool switch_focus(void *data) { return true; }
static bool switch_suppress_screensaver(void *data, bool enable) { return false; }
static bool switch_has_windowed(void *data) { return false; }

static void switch_free(void *data)
{
    switch_video_t *sw = data;

    framebufferClose(&sw->fb);

    if (sw->menu_texture.pixels)
        free(sw->menu_texture.pixels);

    free(sw);
}

static bool switch_set_shader(void *data,
            enum rarch_shader_type type, const char *path)
{
    (void)data;
    (void)type;
    (void)path;

    return false;
}

static void switch_set_rotation(void *data, unsigned rotation)
{
    switch_video_t *sw = data;
    if (!sw)
        return;
    sw->rotation = rotation;
}

static void switch_viewport_info(void *data, struct video_viewport *vp)
{
    switch_video_t *sw = data;
    *vp = sw->vp;
}

static void switch_set_texture_frame(
    void *data, const void *frame, bool rgb32,
    unsigned width, unsigned height, float alpha)
{
    switch_video_t *sw = data;
    size_t sz = width * height * (rgb32 ? 4 : 2);

    if (   !sw->menu_texture.pixels
        || (sw->menu_texture.width  != width)
        || (sw->menu_texture.height != height))
    {
        int xsf, ysf, sf;
        struct scaler_ctx *sctx = NULL;

        if (sw->menu_texture.pixels)
            sw->menu_texture.pixels = realloc(sw->menu_texture.pixels, sz);
        else
            sw->menu_texture.pixels = malloc(sz);

        if (!sw->menu_texture.pixels)
            return;

        xsf = 1280 / width;
        ysf = 720 / height;
        sf  = xsf;

        if (ysf < sf)
            sf = ysf;

        sw->menu_texture.width  = width;
        sw->menu_texture.height = height;
        sw->menu_texture.tgtw   = width * sf;
        sw->menu_texture.tgth   = height * sf;

        sctx                    = &sw->menu_texture.scaler;
        scaler_ctx_gen_reset(sctx);

        sctx->in_width          = width;
        sctx->in_height         = height;
        sctx->in_stride         = width * (rgb32 ? 4 : 2);
        sctx->in_fmt            = rgb32 ? SCALER_FMT_ARGB8888 : SCALER_FMT_RGB565;
        sctx->out_width         = sw->menu_texture.tgtw;
        sctx->out_height        = sw->menu_texture.tgth;
        sctx->out_stride        = 1280 * 4;
        sctx->out_fmt           = SCALER_FMT_ABGR8888;

        sctx->scaler_type       = SCALER_TYPE_POINT;

        if (!scaler_ctx_gen_filter(sctx))
            return;
    }

    memcpy(sw->menu_texture.pixels, frame, sz);
}

static void switch_apply_state_changes(void *data) { }

static void switch_set_texture_enable(void *data, bool enable, bool full_screen)
{
    switch_video_t *sw = data;
    if (!sw->menu_texture.enable && enable)
        nwindowSetDimensions(sw->win, sw->vp.full_width, sw->vp.full_height);
    else if (!enable && sw->menu_texture.enable && sw->smooth)
    {
        clear_screen(sw);
        nwindowSetDimensions(sw->win, sw->hw_scale.width, sw->hw_scale.height);
    }
    sw->menu_texture.enable = enable;
    sw->menu_texture.fullscreen = full_screen;
}

#ifdef HAVE_OVERLAY
static void switch_overlay_enable(void *data, bool state)
{
    switch_video_t *swa = (switch_video_t *)data;

    if (!swa)
        return;

    swa->overlay_enabled = state;
}

static bool switch_overlay_load(void *data,
            const void *image_data, unsigned num_images)
{
    switch_video_t          *swa = (switch_video_t *)data;
    struct texture_image *images = (struct texture_image *)image_data;

    if (!swa)
        return false;

    swa->overlay         = images;
    swa->overlay_enabled = true;

    return true;
}

static void switch_overlay_tex_geom(void *data,
            unsigned idx, float x, float y, float w, float h) { }
static void switch_overlay_vertex_geom(void *data,
            unsigned idx, float x, float y, float w, float h) { }
static void switch_overlay_full_screen(void *data, bool enable) { }
static void switch_overlay_set_alpha(void *data, unsigned idx, float mod) { }

static const video_overlay_interface_t switch_overlay = {
    switch_overlay_enable,
    switch_overlay_load,
    switch_overlay_tex_geom,
    switch_overlay_vertex_geom,
    switch_overlay_full_screen,
    switch_overlay_set_alpha,
};

static void switch_get_overlay_interface(void *data, const video_overlay_interface_t **iface)
{
    switch_video_t *swa = (switch_video_t *)data;
    if (!swa)
        return;
    *iface = &switch_overlay;
}

#endif

static const video_poke_interface_t switch_poke_interface = {
   NULL, /* get_flags */
   NULL, /* load_texture */
   NULL, /* unload_texture */
   NULL, /* set_video_mode */
   NULL, /* get_refresh_rate */
   NULL, /* set_filtering */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   switch_set_aspect_ratio,
   switch_apply_state_changes,
   switch_set_texture_frame,
   switch_set_texture_enable,
   font_driver_render_msg,
   NULL, /* show_mouse */
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_max_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_contrast */
   NULL  /* set_hdr_expand_gamut */
};

static void switch_get_poke_interface(void *data,
            const video_poke_interface_t **iface)
{
    (void)data;
    *iface = &switch_poke_interface;
}

video_driver_t video_switch = {
   switch_init,
   switch_frame,
   switch_set_nonblock_state,
   switch_alive,
   switch_focus,
   switch_suppress_screensaver,
   switch_has_windowed,
   switch_set_shader,
   switch_free,
   "switch",
   NULL, /* set_viewport */
   switch_set_rotation,
   switch_viewport_info,
   NULL, /* read_viewport  */
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   switch_get_overlay_interface,
#endif
   switch_get_poke_interface,
   NULL, /* wrap_type_to_enum */
#ifdef HAVE_GFX_WIDGETS
   NULL  /* gfx_widgets_enabled */
#endif
};

/* vim: set ts=3 sw=3 */
