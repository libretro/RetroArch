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
      void *data, unsigned video_dims) { }

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
            &font->font_data, font_path, font_size, FONT_ATLAS_FORMAT_A8))
   {
      free(font);
      return NULL;
   }

   font->atlas = font->font_driver->get_atlas(font->font_data);
   /* The atlas may grow; glyphs are blitted from it in memory, so
    * there is no texture to make again */
   font->atlas->max_dims = VIDEO_SCALE_PACK(2048, 2048);

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
   switch_font_t *font = (switch_font_t*)data;
   if (!font)
      return 0;
   return font_renderer_get_message_width(font->font_driver,
         font->font_data, msg, msg_len, scale);
}

static void switch_font_render_message(
      switch_video_t *sw,
      switch_font_t *font, const char *msg, size_t msg_len, float scale,
      const unsigned int color, float pos_x, float pos_y,
      unsigned text_align)
{
   float line_height;
   struct font_line_metrics *line_metrics = NULL;
   bool line_ok                           = false;
   int x                                  = 0;
   int y                                  = 0;
   unsigned fb_width                      = VIDEO_SCALE_W(sw->vp.full_dims);
   unsigned fb_height                     = VIDEO_SCALE_H(sw->vp.full_dims);
   const struct font_glyph* (*get_glyph)(void*, uint32_t)
                                          = font->font_driver->get_glyph;
   void *font_data                        = font->font_data;
   const struct font_glyph* glyph_q       = get_glyph(font_data, '?');
   bool aligned                           = (text_align == TEXT_ALIGN_RIGHT
                                         || text_align == TEXT_ALIGN_CENTER);

   font->font_driver->get_line_metrics(font->font_data, &line_metrics);
   line_height = scale / line_metrics->height;

#define FONT_LAYOUT_ALIGNED aligned
   /* A line over the limit is not looked up either */
#define FONT_LAYOUT_SKIP(line, bytes) ((bytes) > AVG_GLPYH_LIMIT)
#define FONT_LAYOUT_LINE(line, line_width, count, bytes) \
   do \
   { \
      (void)(count); \
      line_ok = ((bytes) <= AVG_GLPYH_LIMIT); \
      x       = roundf(pos_x * fb_width); \
      y       = roundf((1.0f - (pos_y - (float)(line) * line_height)) \
            * fb_height); \
      if (text_align == TEXT_ALIGN_RIGHT) \
         x -= (int)((line_width) * scale); \
      else if (text_align == TEXT_ALIGN_CENTER) \
         x -= (int)((line_width) * scale) / 2; \
   } while (0)
#define FONT_LAYOUT_GLYPH(glyph, pen_x, pen_y) \
   do \
   { \
      int ty; \
      int off_x  = x + (glyph)->draw_offset_x + (pen_x); \
      int off_y  = y + (glyph)->draw_offset_y + (pen_y); \
      int g_w    = VIDEO_SCALE_W((glyph)->dims); \
      int g_h    = VIDEO_SCALE_H((glyph)->dims); \
      int tex_x  = VIDEO_SCALE_W((glyph)->atlas_pos); \
      int tex_y  = VIDEO_SCALE_H((glyph)->atlas_pos); \
      if (!line_ok) \
         break; \
      for (ty = tex_y; ty < tex_y + g_h; ty++) \
      { \
         int tx; \
         uint8_t *row = &font->atlas->buffer[ty * font->atlas->width]; \
         for (tx = tex_x; tx < tex_x + g_w; tx++) \
         { \
            int x1, y1; \
            if (!row[tx]) \
               continue; \
            x1 = off_x + (tx - tex_x); \
            y1 = off_y + (ty - tex_y); \
            if (x1 < fb_width && y1 < fb_height) \
               sw->out_buffer[y1 * sw->stride / sizeof(uint32_t) + x1] = color; \
         } \
      } \
   } while (0)
#include "../font_layout.h"
}

static void switch_font_render_msg(
      void *userdata,
      void *data,
      const char *msg, size_t msg_len,
      const struct font_params *params)
{
   font_params_resolved_t rp;
   float x, y, scale;
   enum text_alignment text_align;
   unsigned color, r, g, b, alpha;
   switch_font_t *font              = (switch_font_t *)data;
   switch_video_t *sw               = (switch_video_t*)userdata;

   if (!font || !msg || (msg && !*msg))
      return;
   if (!sw || !sw->out_buffer)
      return;

   /* Asked for before anything is laid out: it may have grown */
   if (font->font_driver && font->font_data)
      font->atlas = font->font_driver->get_atlas(font->font_data);

   font_driver_resolve_params(params, &rp);
   x          = rp.x;
   y          = rp.y;
   scale      = rp.scale;
   text_align = rp.text_align;
   r          = rp.rgba[0];
   g          = rp.rgba[1];
   b          = rp.rgba[2];
   alpha      = rp.rgba[3];
   /* The caller's colour as packed; the message colour in this
    * driver's own order */
   color      = params ? params->color : COLOR_ABGR(r, g, b, alpha);


   switch_font_render_message(sw, font, msg, msg_len, scale,
         color, x, y, text_align);
}

static const struct font_glyph *switch_font_get_glyph(
    void *data, uint32_t code)
{
   switch_font_t *font = (switch_font_t *)data;
   if (font && font->font_driver)
      return font->font_driver->get_glyph((void *)font->font_data, code);
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

/*
 * VIDEO DRIVER
 */

static void gfx_cpy_dsp_buf(uint32_t *buffer, uint32_t *image, int w, int h, uint32_t stride, bool blend)
{
    uint32_t *dest = buffer;
    uint32_t *src = image;
    uint32_t y, x;
    for (y = 0; y < (uint32_t)h; y ++)
    {
        for (x = 0; x < (uint32_t)w; x ++)
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
    uint32_t stride;
    uint32_t *out_buffer;

    nwindowSetDimensions(sw->win, VIDEO_SCALE_W(sw->vp.full_dims), VIDEO_SCALE_H(sw->vp.full_dims));

    out_buffer = (uint32_t*)framebufferBegin(&sw->fb, &stride);

    memset(out_buffer, 0, stride * 720);

    framebufferEnd(&sw->fb);
}

#ifdef HAVE_OVERLAY
static void switch_overlay_free(switch_video_t *sw)
{
   unsigned i;
   for (i = 0; i < sw->overlays_size; i++)
      free(sw->overlays[i].pixels);
   free(sw->overlays);
   sw->overlays      = NULL;
   sw->overlays_size = 0;
}

static void switch_overlay_enable(void *data, bool state)
{
   switch_video_t *sw = (switch_video_t *)data;
   if (sw)
      sw->overlay_enabled = state;
}

/* The page is copied once, into the framebuffer's channel order: the
 * pack's pixels are not this driver's to keep, and a frame blends
 * without a swizzle. */
static bool switch_overlay_load(void *data,
            const void *image_data, unsigned num_images)
{
   unsigned i;
   switch_video_t                   *sw = (switch_video_t *)data;
   const struct texture_image   *images = (const struct texture_image *)image_data;

   if (!sw)
      return false;

   switch_overlay_free(sw);
   if (!num_images || !images)
      return true;

   if (!(sw->overlays = (switch_overlay_t*)calloc(num_images,
               sizeof(*sw->overlays))))
      return false;
   sw->overlays_size = num_images;

   for (i = 0; i < num_images; i++)
   {
      size_t p, n;
      unsigned r;
      const uint32_t *src;
      uint32_t *dst;
      switch_overlay_t *o = &sw->overlays[i];

      o->tex[2]  = o->tex[3]  = 1.0f;
      o->vert[2] = o->vert[3] = 1.0f;
      o->alpha   = 1.0f;
      if (     !images[i].pixels || !images[i].width || !images[i].height
            || !VIDEO_SCALE_FITS(images[i].width, images[i].height))
         continue;
      n = (size_t)images[i].width * images[i].height;
      /* the pixels, then the row spans after them */
      if (!(o->pixels = (uint32_t*)malloc(n * sizeof(uint32_t)
                  + (size_t)images[i].height * 2 * sizeof(uint16_t))))
      {
         switch_overlay_free(sw);
         return false;
      }
      o->spans = (uint16_t*)(o->pixels + n);
      o->dims  = VIDEO_SCALE_PACK(images[i].width, images[i].height);
      src      = images[i].pixels;
      dst      = o->pixels;
      /* R in the low byte, as the framebuffer and its scaler output */
      if (images[i].supports_rgba)
         memcpy(dst, src, n * sizeof(uint32_t));
      else
         for (p = 0; p < n; p++)
            dst[p] =  (src[p] & 0xff00ff00u)
                   | ((src[p] >> 16) & 0xffu)
                   | ((src[p] & 0xffu) << 16);
      for (r = 0; r < images[i].height; r++)
      {
         const uint32_t *row = dst + (size_t)r * images[i].width;
         unsigned first      = 0;
         unsigned last       = images[i].width;
         while (first < last && !(row[first] >> 24))
            first++;
         while (last > first && !(row[last - 1] >> 24))
            last--;
         o->spans[2 * r]     = (uint16_t)first;
         o->spans[2 * r + 1] = (uint16_t)last;
      }
   }

   return true;
}

static void switch_overlay_tex_geom(void *data,
            unsigned index, float x, float y, float w, float h)
{
   switch_video_t *sw = (switch_video_t *)data;
   if (!sw || index >= sw->overlays_size)
      return;
   sw->overlays[index].tex[0] = x;
   sw->overlays[index].tex[1] = y;
   sw->overlays[index].tex[2] = w;
   sw->overlays[index].tex[3] = h;
}

/* y-down from the top left, as the frontend gives it and the
 * framebuffer is laid out */
static void switch_overlay_vertex_geom(void *data,
            unsigned index, float x, float y, float w, float h)
{
   switch_video_t *sw = (switch_video_t *)data;
   if (!sw || index >= sw->overlays_size)
      return;
   sw->overlays[index].vert[0] = x;
   sw->overlays[index].vert[1] = y;
   sw->overlays[index].vert[2] = w;
   sw->overlays[index].vert[3] = h;
}

static void switch_overlay_full_screen(void *data, bool enable)
{
   switch_video_t *sw = (switch_video_t *)data;
   if (sw)
      sw->overlay_full_screen = enable;
}

static void switch_overlay_set_alpha(void *data, unsigned index, float mod)
{
   switch_video_t *sw = (switch_video_t *)data;
   if (!sw || index >= sw->overlays_size)
      return;
   sw->overlays[index].alpha = mod;
}

/* Blend the page over @buf, @pitch pixels a row: across @screen_dims,
 * or the frame at @frame_pos, @frame_dims unless full screen. Nearest
 * sampling at pixel centres, stepped in 16.16 once per row and once
 * per image for the columns; opaque texels are stored, transparent
 * ones skipped. */
static void switch_overlay_render(switch_video_t *sw, uint32_t *buf,
      unsigned pitch, unsigned screen_dims, unsigned frame_pos,
      unsigned frame_dims)
{
   unsigned i;
   int screen_w = (int)VIDEO_SCALE_W(screen_dims);
   int screen_h = (int)VIDEO_SCALE_H(screen_dims);
   int base_x   = sw->overlay_full_screen ? 0 : VIDEO_POS_X(frame_pos);
   int base_y   = sw->overlay_full_screen ? 0 : VIDEO_POS_Y(frame_pos);
   int base_w   = sw->overlay_full_screen ? screen_w
      : (int)VIDEO_SCALE_W(frame_dims);
   int base_h   = sw->overlay_full_screen ? screen_h
      : (int)VIDEO_SCALE_H(frame_dims);

   for (i = 0; i < sw->overlays_size; i++)
   {
      int x, y, dst_x, dst_y, dst_w, dst_h, x0, y0, x1, y1, x_first, x_last;
      int64_t sx, sx_step, sy_step, sy;
      unsigned mod;
      const switch_overlay_t *o = &sw->overlays[i];
      int ol_w                  = (int)VIDEO_SCALE_W(o->dims);
      int ol_h                  = (int)VIDEO_SCALE_H(o->dims);
      uint16_t *cols            = sw->overlay_cols;

      if (     !o->pixels || o->alpha <= 0.0f
            || o->tex[2] <= 0.0f || o->tex[3] <= 0.0f)
         continue;
      dst_x = base_x + (int)floor(o->vert[0] * (double)base_w + 0.5);
      dst_y = base_y + (int)floor(o->vert[1] * (double)base_h + 0.5);
      dst_w = (int)(o->vert[2] * (float)base_w + 0.5f);
      dst_h = (int)(o->vert[3] * (float)base_h + 0.5f);
      if (dst_w <= 0 || dst_h <= 0)
         continue;
      x0 = dst_x < 0 ? 0 : dst_x;
      y0 = dst_y < 0 ? 0 : dst_y;
      x1 = dst_x + dst_w > screen_w ? screen_w : dst_x + dst_w;
      y1 = dst_y + dst_h > screen_h ? screen_h : dst_y + dst_h;
      if (x1 > (int)ARRAY_SIZE(sw->overlay_cols))
         x1 = (int)ARRAY_SIZE(sw->overlay_cols);
      if (x0 >= x1 || y0 >= y1)
         continue;
      mod      = o->alpha >= 1.0f ? 255u
         : (unsigned)(o->alpha * 255.0f + 0.5f);

      sx_step  = (int64_t)((double)o->tex[2] * ol_w * 65536.0 / dst_w);
      sy_step  = (int64_t)((double)o->tex[3] * ol_h * 65536.0 / dst_h);
      sx       = (int64_t)((double)o->tex[0] * ol_w * 65536.0)
         + sx_step / 2 + (int64_t)(x0 - dst_x) * sx_step;
      sy       = (int64_t)((double)o->tex[1] * ol_h * 65536.0)
         + sy_step / 2 + (int64_t)(y0 - dst_y) * sy_step;
      for (x = x0; x < x1; x++, sx += sx_step)
      {
         int col = (int)(sx >> 16);
         cols[x] = (uint16_t)(col < 0 ? 0 : col >= ol_w ? ol_w - 1 : col);
      }

      for (y = y0; y < y1; y++, sy += sy_step)
      {
         int lo, hi, row    = (int)(sy >> 16);
         unsigned first, last;
         const uint32_t *in;
         uint32_t *out      = buf + (size_t)y * pitch;
         if (row < 0)
            row = 0;
         else if (row >= ol_h)
            row = ol_h - 1;
         first = o->spans[2 * row];
         last  = o->spans[2 * row + 1];
         if (first >= last)
            continue;
         /* The screen columns the row's span maps to: cols[] only
          * ever grows */
         lo = x0;
         hi = x1;
         while (lo < hi)
         {
            int mid = (lo + hi) >> 1;
            if (cols[mid] < first)
               lo = mid + 1;
            else
               hi = mid;
         }
         x_first = lo;
         hi      = x1;
         while (lo < hi)
         {
            int mid = (lo + hi) >> 1;
            if (cols[mid] < last)
               lo = mid + 1;
            else
               hi = mid;
         }
         x_last = lo;
         in     = o->pixels + (size_t)row * ol_w;

         for (x = x_first; x < x_last; x++)
         {
            uint32_t s = in[cols[x]];
            uint32_t d, rb, g, t;
            unsigned a = s >> 24, ia;
            if (!a)
               continue;
            if (mod != 255u)
            {
               /* exact a * mod / 255, rounded */
               t = a * mod + 128u;
               a = (t + (t >> 8)) >> 8;
               if (!a)
                  continue;
            }
            d = out[x];
            if (a == 255u)
            {
               out[x] = (s & 0x00ffffffu) | (d & 0xff000000u);
               continue;
            }
            /* exact x / 255, two channels at once */
            ia = 255u - a;
            rb = (s & 0x00ff00ffu) * a + (d & 0x00ff00ffu) * ia + 0x00800080u;
            rb = ((rb + ((rb >> 8) & 0x00ff00ffu)) >> 8) & 0x00ff00ffu;
            g  = ((s >> 8) & 0xffu) * a + ((d >> 8) & 0xffu) * ia + 0x80u;
            g  = ((g + (g >> 8)) >> 8) & 0xffu;
            out[x] = rb | (g << 8) | (d & 0xff000000u);
         }
      }
   }
}
#endif

static void *switch_init(const video_info_t *video)
{
    switch_video_t *sw = (switch_video_t *)calloc(1, sizeof(*sw));
    if (!sw)
        return NULL;

   sw->win = nwindowGetDefault();

   framebufferCreate(&sw->fb, sw->win, 1280, 720, PIXEL_FORMAT_RGBA_8888, 2);
   framebufferMakeLinear(&sw->fb);

    sw->vp.pos          = VIDEO_POS_PACK(0, 0);
    sw->o_dims          = video->dims;
    sw->vp.dims         = video->dims;
    sw->overlay_enabled = false;
#ifdef HAVE_MENU
    sw->in_menu         = false;
#endif

    sw->vp.full_dims    = VIDEO_SCALE_PACK(1280, 720);

    /* Sanity check */
    sw->vp.dims  = VIDEO_SCALE_PACK(MIN(VIDEO_SCALE_W(sw->vp.dims), VIDEO_SCALE_W(sw->vp.full_dims)),
          MIN(VIDEO_SCALE_H(sw->vp.dims), VIDEO_SCALE_H(sw->vp.full_dims)));

    sw->vsync = video->vsync;
    sw->rgb32 = video->rgb32;
    sw->keep_aspect = true;
    sw->should_resize = true;
    sw->o_size = true;
    sw->is_threaded = video->is_threaded;
    sw->smooth = video->smooth;
    sw->menu_texture.enable = false;

    /* Autoselect driver */
    /* no input driver of this driver's own: the frontend starts the
     * platform's */
    input_driver_video_window(INPUT_WINDOW_PLATFORM, NULL);


    clear_screen(sw);

    return sw;
}

static void switch_update_viewport(switch_video_t *sw)
{
    /* Handle o_size mode (original size) specially */
    if (sw->o_size)
    {
        sw->vp.pos    = VIDEO_POS_PACK((int)(((float)VIDEO_SCALE_W(sw->vp.full_dims) - VIDEO_SCALE_W(sw->o_dims))) / 2,
              (int)(((float)VIDEO_SCALE_H(sw->vp.full_dims) - VIDEO_SCALE_H(sw->o_dims))) / 2);
        sw->vp.dims   = sw->o_dims;
        return;
    }

    video_driver_update_viewport(&sw->vp, false, sw->keep_aspect, true);
}

static void switch_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
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
          /* What the last frame carried, not what the setting says
           * now: this runs on the video thread under the threaded
           * wrapper, which is the default here. */
          if (sw->frame_scale_integer)
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
      unsigned dims,
      uint64_t frame_count, unsigned pitch,
      const char *msg, video_frame_info_t *video_info)
{
   unsigned width = VIDEO_SCALE_W(dims);
   unsigned height = VIDEO_SCALE_H(dims);
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

#ifdef HAVE_MENU
   /* Travels with the frame, for set_aspect_ratio() to read rather
    * than the setting the menu writes */
   if (sw)
      sw->frame_scale_integer = video_info->scale_integer;
#endif

   if (!frame)
      return true;

   if (ffwd_mode && !sw->is_threaded)
   {
      /* render every 4th frame when in ffwd mode and not threaded */
      if ((frame_count % 4) != 0)
         return true;
   }

   if (     sw->should_resize
         || (sw->last_dims != dims))
   {
      switch_update_viewport(sw);

      /* Sanity check */
      sw->vp.dims   = VIDEO_SCALE_PACK(MIN(VIDEO_SCALE_W(sw->vp.dims), VIDEO_SCALE_W(sw->vp.full_dims)),
            MIN(VIDEO_SCALE_H(sw->vp.dims), VIDEO_SCALE_H(sw->vp.full_dims)));

      scaler_ctx_gen_reset(&sw->scaler);

      sw->scaler.in_width  = width;
      sw->scaler.in_height = height;
      sw->scaler.in_stride = pitch;
      sw->scaler.in_fmt    = sw->rgb32
         ? SCALER_FMT_ARGB8888
         : SCALER_FMT_RGB565;

      if (!sw->smooth)
      {
         sw->scaler.out_width  = VIDEO_SCALE_W(sw->vp.dims);
         sw->scaler.out_height = VIDEO_SCALE_H(sw->vp.dims);
         sw->scaler.out_stride = VIDEO_SCALE_W(sw->vp.full_dims) * sizeof(uint32_t);
      }
      else
      {
         float screen_ratio    = (float)VIDEO_SCALE_W(sw->vp.full_dims) / VIDEO_SCALE_H(sw->vp.full_dims);
         float tgt_ratio       = (float)VIDEO_SCALE_W(sw->vp.dims) / VIDEO_SCALE_H(sw->vp.dims);

         sw->scaler.out_width  = width;
         sw->scaler.out_height = height;
         sw->scaler.out_stride = width * sizeof(uint32_t);

         sw->hw_scale.dims     = VIDEO_SCALE_PACK(
               ceil(screen_ratio / tgt_ratio * sw->scaler.out_width),
               sw->scaler.out_height);
         sw->hw_scale.x_offset = ceil(
               (VIDEO_SCALE_W(sw->hw_scale.dims)
                - sw->scaler.out_width) / 2.0);
#ifdef HAVE_MENU
         if (!menu_is_alive)
#endif
         {
            clear_screen(sw);
            nwindowSetDimensions(sw->win,
                  VIDEO_SCALE_W(sw->hw_scale.dims),
                  VIDEO_SCALE_H(sw->hw_scale.dims));
         }
      }

      sw->scaler.out_fmt     = SCALER_FMT_ABGR8888;
      sw->scaler.scaler_type = SCALER_TYPE_POINT;

      if (!scaler_ctx_gen_filter(&sw->scaler))
         return false;

      sw->last_dims          = dims;

      sw->should_resize      = false;
   }

   out_buffer     = (uint32_t*)framebufferBegin(&sw->fb, &stride);
   sw->out_buffer = out_buffer;
   sw->stride     = stride;

#ifdef HAVE_MENU
   if (sw->in_menu && !menu_is_alive && sw->smooth)
   {
      memset(out_buffer, 0, stride * VIDEO_SCALE_H(sw->vp.full_dims));
      nwindowSetDimensions(sw->win,
            VIDEO_SCALE_W(sw->hw_scale.dims),
            VIDEO_SCALE_H(sw->hw_scale.dims));
   }

   sw->in_menu = menu_is_alive;

   if (sw->menu_texture.enable)
   {
      menu_driver_frame(menu_is_alive, video_info);

      if (sw->menu_texture.pixels)
      {
         memset(out_buffer, 0, stride * VIDEO_SCALE_H(sw->vp.full_dims));
         scaler_ctx_scale(&sw->menu_texture.scaler,
                 sw->tmp_image
               + ((VIDEO_SCALE_H(sw->vp.full_dims)
                     - VIDEO_SCALE_H(sw->menu_texture.tgt_dims)) / 2)
               * VIDEO_SCALE_W(sw->vp.full_dims)
               + ((VIDEO_SCALE_W(sw->vp.full_dims)
                     - VIDEO_SCALE_W(sw->menu_texture.tgt_dims)) / 2),
               sw->menu_texture.pixels);
         gfx_cpy_dsp_buf(out_buffer, sw->tmp_image, VIDEO_SCALE_W(sw->vp.full_dims), VIDEO_SCALE_H(sw->vp.full_dims), stride, true);
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
         scaler_ctx_scale(ctx, sw->image + (VIDEO_POS_Y(sw->vp.pos) * VIDEO_SCALE_W(sw->vp.full_dims)) + VIDEO_POS_X(sw->vp.pos), frame);
         gfx_cpy_dsp_buf(out_buffer, sw->image, VIDEO_SCALE_W(sw->vp.full_dims), VIDEO_SCALE_H(sw->vp.full_dims), stride, false);
      }

#ifdef HAVE_OVERLAY
   if (sw->overlay_enabled && sw->overlays)
   {
      bool on_menu = false;
#ifdef HAVE_MENU
      on_menu      = sw->menu_texture.enable;
#endif
      /* The scaled-up frame is the framebuffer's top left, the window
       * cropped to it; the menu and the unscaled frame fill it */
      if (!(on_menu && video_info->overlay_behind_menu))
      {
         if (on_menu || !sw->smooth)
            switch_overlay_render(sw, out_buffer, stride / sizeof(uint32_t),
                  sw->vp.full_dims, sw->vp.pos, sw->vp.dims);
         else
            switch_overlay_render(sw, out_buffer, stride / sizeof(uint32_t),
                  sw->hw_scale.dims,
                  VIDEO_POS_PACK(sw->hw_scale.x_offset, 0),
                  VIDEO_SCALE_PACK(sw->scaler.out_width,
                     sw->scaler.out_height));
      }
   }
#endif

   if (statistics_show && !sw->smooth)
   {
      if (osd_params)
         font_driver_render_msg(sw, video_info->stat_text, video_info->stat_text_len,
               osd_params, NULL);
   }

   if (msg)
      font_driver_render_msg(sw, msg, strlen(msg), NULL, NULL);

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
    scaler_ctx_gen_reset(&sw->scaler);
    scaler_ctx_gen_reset(&sw->menu_texture.scaler);

#ifdef HAVE_OVERLAY
    switch_overlay_free(sw);
#endif
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
    unsigned dims, float alpha)
{
    switch_video_t *sw = data;
    size_t sz = VIDEO_SCALE_AREA(dims) * (rgb32 ? 4 : 2);

    if (   !sw->menu_texture.pixels
        || (sw->menu_texture.dims != dims))
    {
        int xsf, ysf, sf;
        struct scaler_ctx *sctx = NULL;

        /* realloc-to-tmp to avoid the classic realloc-assign-self
         * leak: the pre-patch form 'pixels = realloc(pixels, sz)'
         * overwrites the only pointer to the old menu-texture
         * buffer with NULL on OOM, leaking it.  The subsequent
         * 'if (!pixels) return' catches the crash but not the
         * leak. */
        if (sw->menu_texture.pixels)
        {
            void *tmp = realloc(sw->menu_texture.pixels, sz);
            if (!tmp)
                return;
            sw->menu_texture.pixels = tmp;
        }
        else
            sw->menu_texture.pixels = malloc(sz);

        if (!sw->menu_texture.pixels)
            return;

        xsf = 1280 / VIDEO_SCALE_W(dims);
        ysf = 720 / VIDEO_SCALE_H(dims);
        sf  = xsf;

        if (ysf < sf)
            sf = ysf;

        sw->menu_texture.dims     = dims;
        sw->menu_texture.tgt_dims = VIDEO_SCALE_PACK(
              VIDEO_SCALE_W(dims) * sf, VIDEO_SCALE_H(dims) * sf);

        sctx                    = &sw->menu_texture.scaler;
        scaler_ctx_gen_reset(sctx);

        sctx->in_width          = VIDEO_SCALE_W(dims);
        sctx->in_height         = VIDEO_SCALE_H(dims);
        sctx->in_stride         = VIDEO_SCALE_W(dims) * (rgb32 ? 4 : 2);
        sctx->in_fmt            = rgb32 ? SCALER_FMT_ARGB8888 : SCALER_FMT_RGB565;
        sctx->out_width         = VIDEO_SCALE_W(sw->menu_texture.tgt_dims);
        sctx->out_height        = VIDEO_SCALE_H(sw->menu_texture.tgt_dims);
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
        nwindowSetDimensions(sw->win, VIDEO_SCALE_W(sw->vp.full_dims), VIDEO_SCALE_H(sw->vp.full_dims));
    else if (!enable && sw->menu_texture.enable && sw->smooth)
    {
        clear_screen(sw);
        nwindowSetDimensions(sw->win,
              VIDEO_SCALE_W(sw->hw_scale.dims),
              VIDEO_SCALE_H(sw->hw_scale.dims));
    }
    sw->menu_texture.enable = enable;
    sw->menu_texture.fullscreen = full_screen;
}

#ifdef HAVE_OVERLAY
static const video_overlay_interface_t switch_overlay = {
    switch_overlay_enable,
    switch_overlay_load,
    NULL, /* load_textures */
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
   NULL, /* set_hdr_menu_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_expand_gamut */
   NULL, /* set_hdr_scanlines */
   NULL  /* set_hdr_subpixel_layout */
};

static void switch_get_poke_interface(void *data,
            const video_poke_interface_t **iface)
{
    (void)data;
    *iface = &switch_poke_interface;
}

static font_renderer_t switch_font =
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
#ifdef HAVE_OVERLAY
   switch_get_overlay_interface,
#endif
   switch_get_poke_interface,
   NULL, /* wrap_type_to_enum */
   NULL, /* shader_load_begin */
   NULL, /* shader_load_step */
#ifdef HAVE_GFX_WIDGETS
   NULL  /* gfx_widgets_enabled */,
#endif
   NULL, /* invalidate_hw_render_cache */
   NULL, /* read_viewport_hdr */
   &switch_font
};

gfx_display_ctx_driver_t gfx_display_ctx_switch = {
   gfx_display_switch_draw,
   NULL,                                        /* draw_pipeline   */
   NULL,                                        /* blend_begin     */
   NULL,                                        /* blend_end       */
   NULL,                                        /* get_default_mvp */
   gfx_display_switch_get_default_vertices,
   gfx_display_switch_get_default_tex_coords,
   &switch_font,
   GFX_VIDEO_DRIVER_SWITCH,
   "switch",
   false,
   false,
   NULL,                                         /* scissor_begin */
   NULL                                          /* scissor_end   */
};


/* vim: set ts=3 sw=3 */
