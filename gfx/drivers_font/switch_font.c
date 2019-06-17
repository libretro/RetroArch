/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018      - lifajucejo
 *  Copyright (C) 2018      - m4xw
 *  Copyright (C) 2018      - natinusala
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
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <encodings/utf.h>

#include <retro_math.h>

#include "../font_driver.h"

#include "../../retroarch.h"
#include "../../verbosity.h"

#include "../common/switch_common.h"

typedef struct
{
      struct font_atlas *atlas;

      const font_renderer_driver_t *font_driver;
      void *font_data;
} switch_font_t;

static void *switch_font_init_font(void *data, const char *font_path,
                                   float font_size, bool is_threaded)
{
      switch_font_t *font = (switch_font_t *)calloc(1, sizeof(switch_font_t));

      if (!font)
            return NULL;

      if (!font_renderer_create_default((const void **)&font->font_driver,
                                        &font->font_data, font_path, font_size))
      {
            RARCH_WARN("Couldn't initialize font renderer.\n");
            free(font);
            return NULL;
      }

      font->atlas = font->font_driver->get_atlas(font->font_data);

      RARCH_LOG("Switch font driver initialized with backend %s\n", font->font_driver->ident);

      return font;
}

static void switch_font_free_font(void *data, bool is_threaded)
{
      switch_font_t *font = (switch_font_t *)data;

      if (!font)
            return;

      if (font->font_driver && font->font_data)
            font->font_driver->free(font->font_data);

      free(font);
}

static int switch_font_get_message_width(void *data, const char *msg,
                                         unsigned msg_len, float scale)
{
      switch_font_t *font = (switch_font_t *)data;

      unsigned i;
      int delta_x = 0;

      if (!font)
            return 0;

      for (i = 0; i < msg_len; i++)
      {
            const char *msg_tmp = &msg[i];
            unsigned code = utf8_walk(&msg_tmp);
            unsigned skip = msg_tmp - &msg[i];

            if (skip > 1)
                  i += skip - 1;

            const struct font_glyph *glyph =
                font->font_driver->get_glyph(font->font_data, code);

            if (!glyph) /* Do something smarter here ... */
                  glyph = font->font_driver->get_glyph(font->font_data, '?');

            if (!glyph)
                  continue;

            delta_x += glyph->advance_x;
      }

      return delta_x * scale;
}

static void switch_font_render_line(
    video_frame_info_t *video_info,
    switch_font_t *font, const char *msg, unsigned msg_len,
    float scale, const unsigned int color, float pos_x,
    float pos_y, unsigned text_align)
{
      switch_video_t* sw = (switch_video_t*)video_info->userdata;

      if(!sw)
         return;

      int delta_x = 0;
      int delta_y = 0;

      unsigned fbWidth = sw->vp.full_width;
      unsigned fbHeight = sw->vp.full_height;

      if (sw->out_buffer) {

            int x = roundf(pos_x * fbWidth);
            int y = roundf((1.0f - pos_y) * fbHeight);

            switch (text_align)
            {
            case TEXT_ALIGN_RIGHT:
                  x -= switch_font_get_message_width(font, msg, msg_len, scale);
                  break;
            case TEXT_ALIGN_CENTER:
                  x -= switch_font_get_message_width(font, msg, msg_len, scale) / 2;
                  break;
            }

            for (int i = 0; i < msg_len; i++)
            {
                  int off_x, off_y, tex_x, tex_y, width, height;
                  const char *msg_tmp = &msg[i];
                  unsigned code = utf8_walk(&msg_tmp);
                  unsigned skip = msg_tmp - &msg[i];

                  if (skip > 1)
                        i += skip - 1;

                  const struct font_glyph *glyph =
                        font->font_driver->get_glyph(font->font_data, code);

                  if (!glyph) /* Do something smarter here ... */
                        glyph = font->font_driver->get_glyph(font->font_data, '?');

                  if (!glyph)
                        continue;

                  off_x = x + glyph->draw_offset_x + delta_x;
                  off_y = y + glyph->draw_offset_y + delta_y;
                  width = glyph->width;
                  height = glyph->height;

                  tex_x = glyph->atlas_offset_x;
                  tex_y = glyph->atlas_offset_y;

                  for (int y = tex_y; y < tex_y + height; y++)
                  {
                        uint8_t *row = &font->atlas->buffer[y * font->atlas->width];
                        for (int x = tex_x; x < tex_x + width; x++)
                        {
                              if (!row[x])
                                 continue;
                              int x1 = off_x + (x - tex_x);
                              int y1 = off_y + (y - tex_y);
                              if (x1 < fbWidth && y1 < fbHeight)
                                    sw->out_buffer[y1 * sw->stride / sizeof(uint32_t) + x1] = color;
                        }
                  }

                  delta_x += glyph->advance_x;
                  delta_y += glyph->advance_y;
            }
      }
}

#define AVG_GLPYH_LIMIT 140
static void switch_font_render_message(
    video_frame_info_t *video_info,
    switch_font_t *font, const char *msg, float scale,
    const unsigned int color, float pos_x, float pos_y,
    unsigned text_align)
{
      int lines = 0;
      float line_height;

      if (!msg || !*msg)
            return;

      /* If the font height is not supported just draw as usual */
      if (!font->font_driver->get_line_height)
      {
            int msgLen = strlen(msg);
            if (msgLen <= AVG_GLPYH_LIMIT)
            {
                  switch_font_render_line(video_info, font, msg, strlen(msg),
                                          scale, color, pos_x, pos_y, text_align);
            }
            return;
      }
      line_height = scale / font->font_driver->get_line_height(font->font_data);

      for (;;)
      {
            const char *delim = strchr(msg, '\n');

            /* Draw the line */
            if (delim)
            {
                  unsigned msg_len = delim - msg;
                  if (msg_len <= AVG_GLPYH_LIMIT)
                  {
                        switch_font_render_line(video_info, font, msg, msg_len,
                                                scale, color, pos_x, pos_y - (float)lines * line_height,
                                                text_align);
                  }
                  msg += msg_len + 1;
                  lines++;
            }
            else
            {
                  unsigned msg_len = strlen(msg);
                  if (msg_len <= AVG_GLPYH_LIMIT)
                  {
                        switch_font_render_line(video_info, font, msg, msg_len,
                                                scale, color, pos_x, pos_y - (float)lines * line_height,
                                                text_align);
                  }
                  break;
            }
      }
}

static void switch_font_render_msg(
    video_frame_info_t *video_info,
    void *data, const char *msg,
    const struct font_params *params)
{
      float x, y, scale, drop_mod, drop_alpha;
      int drop_x, drop_y;
      unsigned max_glyphs;
      enum text_alignment text_align;
      unsigned color, color_dark, r, g, b,
          alpha, r_dark, g_dark, b_dark, alpha_dark;
      switch_font_t *font = (switch_font_t *)data;
      unsigned width = video_info->width;
      unsigned height = video_info->height;

      if (!font || !msg || msg && !*msg)
            return;

      if (params)
      {
            x = params->x;
            y = params->y;
            scale = params->scale;
            text_align = params->text_align;
            drop_x = params->drop_x;
            drop_y = params->drop_y;
            drop_mod = params->drop_mod;
            drop_alpha = params->drop_alpha;

            r = FONT_COLOR_GET_RED(params->color);
            g = FONT_COLOR_GET_GREEN(params->color);
            b = FONT_COLOR_GET_BLUE(params->color);
            alpha = FONT_COLOR_GET_ALPHA(params->color);

            color = params->color;
      }
      else
      {
            x = 0.0f;
            y = 0.0f;
            scale = 1.0f;
            text_align = TEXT_ALIGN_LEFT;

            r = (video_info->font_msg_color_r * 255);
            g = (video_info->font_msg_color_g * 255);
            b = (video_info->font_msg_color_b * 255);
            alpha = 255;
            color = COLOR_ABGR(r, g, b, alpha);

            drop_x = -2;
            drop_y = -2;
            drop_mod = 0.3f;
            drop_alpha = 1.0f;
      }

      max_glyphs = strlen(msg);

      /*if (drop_x || drop_y)
      max_glyphs    *= 2;

   if (drop_x || drop_y)
   {
      r_dark         = r * drop_mod;
      g_dark         = g * drop_mod;
      b_dark         = b * drop_mod;
      alpha_dark     = alpha * drop_alpha;
      color_dark     = COLOR_ABGR(r_dark, g_dark, b_dark, alpha_dark);

      switch_font_render_message(video_info, font, msg, scale, color_dark,
                              x + scale * drop_x / width, y +
                              scale * drop_y / height, text_align);
   }*/

      switch_font_render_message(video_info, font, msg, scale,
                                 color, x, y, text_align);
}

static const struct font_glyph *switch_font_get_glyph(
    void *data, uint32_t code)
{
      switch_font_t *font = (switch_font_t *)data;

      if (!font || !font->font_driver)
            return NULL;

      if (!font->font_driver->ident)
            return NULL;

      return font->font_driver->get_glyph((void *)font->font_driver, code);
}

static void switch_font_bind_block(void *data, void *userdata)
{
      (void)data;
}

static int switch_font_get_line_height(void *data)
{
      switch_font_t *font = (switch_font_t *)data;
      if (!font || !font->font_driver || !font->font_data)
      return -1;

      return font->font_driver->get_line_height(font->font_data);
}

font_renderer_t switch_font =
    {
        switch_font_init_font,
        switch_font_free_font,
        switch_font_render_msg,
        "switchfont",
        switch_font_get_glyph,
        switch_font_bind_block,
        NULL, /* flush_block */
        switch_font_get_message_width,
        switch_font_get_line_height
};
