/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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
 *  This file is rewritten from sdl video by AreaScout
 */

#include <SDL/SDL.h>
#include <GLES2/gl2.h>
#include <limare.h>
#include <stdlib.h>
#include <string.h>
#include "../driver.h"
#include "../general.h"
#include "scaler/scaler.h"
#include "gfx_common.h"
#include "gfx_context.h"
#include "fonts/fonts.h"

#define LIMA_TEXEL_FORMAT_BGR_565		    0x0E
#define LIMA_TEXEL_FORMAT_RGBA_8888		    0x16

static struct limare_state *state;
static void lima_gfx_set_rotation(void *data, unsigned rotation);
static void lima_gfx_get_poke_interface(void *data, const video_poke_interface_t **iface);
static void lima_set_texture_frame(void *data, const void *frame, bool rgb32, unsigned width, unsigned height, float alpha);

typedef struct lima_video
{
   SDL_Surface *screen;
   bool quitting;

   void *font;
   const font_renderer_driver_t *font_driver;
   uint8_t font_r;
   uint8_t font_g;
   uint8_t font_b;

   struct scaler_ctx scaler;
   unsigned last_width;
   unsigned last_height;
} lima_video_t;


typedef struct _point2 {
	float x,y;
} Point2;

float vertices[4][3] =
{
	{-1, -1,  0},
	{ 1, -1,  0},
	{-1,  1,  0},
	{ 1,  1,  0}
};

Point2 coords[] =
{
	{0, 1}, //   0 deg
	{1, 1},
	{0, 0},
	{1, 0},
	{0, 0}, //  90 deg
	{0, 1},
	{1, 0},
	{1, 1},
	{1, 0}, // 180 deg
	{0, 0},
	{1, 1},
	{0, 1},
	{1, 1}, // 270 deg
	{1, 0},
	{0, 1},
	{0, 0}
};

static int rotate = 0;
static bool _rgui = false;
static void *rgui_buffer = NULL;

static void lima_gfx_free(void *data)
{
   lima_video_t *vid = (lima_video_t*)data;
   if (!vid)
      return;

   SDL_QuitSubSystem(SDL_INIT_VIDEO);

   if (vid->font)
      vid->font_driver->free(vid->font);

   scaler_ctx_gen_reset(&vid->scaler);

   free(vid);
   if(rgui_buffer)
   {
      free(rgui_buffer);
      rgui_buffer = NULL;
   }
   rotate = 0;
}

static void lima_init_font(lima_video_t *vid, const char *font_path, unsigned font_size)
{
   if (!g_settings.video.font_enable)
      return;

   if (font_renderer_create_default(&vid->font_driver, &vid->font))
   {
         int r = g_settings.video.msg_color_r * 255;
         int g = g_settings.video.msg_color_g * 255;
         int b = g_settings.video.msg_color_b * 255;

         r = r < 0 ? 0 : (r > 255 ? 255 : r);
         g = g < 0 ? 0 : (g > 255 ? 255 : g);
         b = b < 0 ? 0 : (b > 255 ? 255 : b);

         vid->font_r = r;
         vid->font_g = g;
         vid->font_b = b;
   }
   else
      RARCH_LOG("Could not initialize fonts.\n");
}

static void lima_render_msg(lima_video_t *vid, SDL_Surface *buffer,
      const char *msg, unsigned width, unsigned height, const SDL_PixelFormat *fmt)
{
   int x, y;
   if (!vid->font)
      return;

   struct font_output_list out;
   vid->font_driver->render_msg(vid->font, msg, &out);
   struct font_output *head = out.head;

   int msg_base_x = g_settings.video.msg_pos_x * width;
   int msg_base_y = (1.0 - g_settings.video.msg_pos_y) * height;

   unsigned rshift = fmt->Rshift;
   unsigned gshift = fmt->Gshift;
   unsigned bshift = fmt->Bshift;

   for (; head; head = head->next)
   {
      int base_x = msg_base_x + head->off_x;
      int base_y = msg_base_y - head->off_y - head->height;

      int glyph_width  = head->width;
      int glyph_height = head->height;

      const uint8_t *src = head->output;

      if (base_x < 0)
      {
         src -= base_x;
         glyph_width += base_x;
         base_x = 0;
      }

      if (base_y < 0)
      {
         src -= base_y * (int)head->pitch;
         glyph_height += base_y;
         base_y = 0;
      }

      int max_width  = width - base_x;
      int max_height = height - base_y;

      if (max_width <= 0 || max_height <= 0)
         continue;

      if (glyph_width > max_width)
         glyph_width = max_width;
      if (glyph_height > max_height)
         glyph_height = max_height;

      if (vid->scaler.in_fmt == SCALER_FMT_ARGB8888)
      {
    	 uint32_t *out = (uint32_t*)buffer->pixels + base_y * (buffer->pitch >> 2) + base_x;

         for (y = 0; y < glyph_height; y++, src += head->pitch, out += buffer->pitch >> 2)
         {
            for (x = 0; x < glyph_width; x++)
            {
               unsigned blend = src[x];
               unsigned out_pix = out[x];
               unsigned r = (out_pix >> rshift) & 0xff;
               unsigned g = (out_pix >> gshift) & 0xff;
               unsigned b = (out_pix >> bshift) & 0xff;

               unsigned out_r = (r * (256 - blend) + vid->font_b * blend) >> 8;
               unsigned out_g = (g * (256 - blend) + vid->font_g * blend) >> 8;
               unsigned out_b = (b * (256 - blend) + vid->font_r * blend) >> 8;
               out[x] = (out_r << rshift) | (out_g << gshift) | (out_b << bshift);
            }
         }
      }
      else
      {
    	 uint16_t *out = (uint16_t*)buffer->pixels + base_y * (buffer->pitch >> 1) + base_x;

         for (y = 0; y < glyph_height; y++, src += head->pitch, out += buffer->pitch >> 1)
         {
            for (x = 0; x < glyph_width; x++)
            {
               uint8_t blend = src[x];
               uint16_t out_pix = out[x];

               uint8_t b = (out_pix >>  0) & 0x1f;
               uint8_t g = (out_pix >>  5) & 0x3f;
               uint8_t r = (out_pix >> 11) & 0x1f;
               b = (b << 3) | (b >> 2);
               g = (g << 2) | (g >> 4);
               r = (r << 3) | (r >> 2);

               uint8_t out_r = (r * (256 - blend) + vid->font_r * blend) >> 8;
               uint8_t out_g = (g * (256 - blend) + vid->font_g * blend) >> 8;
               uint8_t out_b = (b * (256 - blend) + vid->font_b * blend) >> 8;

               out[x] = (((out_r) << 8) & fmt->Rmask) | (((out_g) << 3) & fmt->Gmask) | (((out_b) >> 3) & fmt->Bmask);
            }
         }
      }
   }

   vid->font_driver->free_output(vid->font, &out);
}

static void *lima_gfx_init(const video_info_t *video, const input_driver_t **input, void **input_data)
{

   const char* vertex_shader_source =
	"attribute vec4 in_vertex;\n"
	"attribute vec2 in_coord;\n"
	"\n"
	"varying vec2 coord;\n"
	"\n"
	"void main()\n"
	"{\n"
	"    gl_Position = in_vertex;\n"
	"    coord = in_coord;\n"
	"}\n";
   const char* fragment_shader_source =
	"precision highp float;\n"
	"\n"
	"varying vec2 coord;\n"
	"\n"
	"uniform sampler2D in_texture;\n"

	"void main()\n"
	"{\n"
	"    gl_FragColor = texture2D(in_texture, coord);\n"
	"}\n";

   if(!state)
   {
      state = limare_init();
      if (!state)
	     return NULL;

      limare_buffer_clear(state);

      if(limare_state_setup(state, 0, 0, 0xFF000000))
	     return NULL;

      limare_enable(state, GL_DEPTH_TEST);
      limare_enable(state, GL_CULL_FACE);
      limare_depth_mask(state, 1);

      int program = limare_program_new(state);
      vertex_shader_attach(state, program, vertex_shader_source);
      fragment_shader_attach(state, program, fragment_shader_source);

      limare_link(state);
   }

   SDL_InitSubSystem(SDL_INIT_VIDEO);

   lima_video_t *vid = (lima_video_t*)calloc(1, sizeof(*vid));
   if (!vid)
      return NULL;

   const SDL_VideoInfo *video_info = SDL_GetVideoInfo();
   rarch_assert(video_info);
   unsigned full_x = video_info->current_w;
   unsigned full_y = video_info->current_h;
   RARCH_LOG("Detecting desktop resolution %ux%u.\n", full_x, full_y);

   void *lima_input = NULL;

   if (!video->fullscreen)
      RARCH_LOG("Creating window @ %ux%u\n", video->width, video->height);

   SDL_Surface *dummy = SDL_SetVideoMode(1, 1, 0, SDL_ANYFORMAT | SDL_FULLSCREEN);

   vid->screen = SDL_CreateRGBSurface(SDL_HWSURFACE | SDL_HWACCEL | SDL_DOUBLEBUF, g_extern.system.av_info.geometry.base_width, g_extern.system.av_info.geometry.base_height, 32, 0, 0, 0, 0);

   RARCH_LOG("New game texture size w = %d h = %d\n", g_extern.system.av_info.geometry.base_width, g_extern.system.av_info.geometry.base_height);

   if (!vid->screen)
   {
      RARCH_ERR("Failed to init SDL surface: %s\n", SDL_GetError());
      lima_gfx_free(vid);
      return NULL;
   }

   SDL_ShowCursor(SDL_DISABLE);

   if (input && input_data)
   {
      lima_input = input_sdl.init();
      if (lima_input)
      {
         *input = &input_sdl;
         *input_data = lima_input;
      }
      else
      {
         *input = NULL;
         *input_data = NULL;
      }
   }

   lima_init_font(vid, g_settings.video.font_path, g_settings.video.font_size);

   vid->scaler.scaler_type = video->smooth ? SCALER_TYPE_BILINEAR : SCALER_TYPE_POINT;
   vid->scaler.in_fmt  = video->rgb32 ? SCALER_FMT_ARGB8888 : SCALER_FMT_RGB565;
   vid->scaler.out_fmt = SCALER_FMT_ABGR8888;

   return vid;
}

static void check_window(lima_video_t *vid)
{
   SDL_Event event;
   while (SDL_PollEvent(&event))
   {
      switch (event.type)
      {
         case SDL_QUIT:
            vid->quitting = true;
            break;

         default:
            break;
      }
   }
}

static bool lima_gfx_frame(void *data, const void *frame, unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   RARCH_PERFORMANCE_INIT(frame_run);
   RARCH_PERFORMANCE_START(frame_run);
   if (!frame)
   {
	  RARCH_PERFORMANCE_STOP(frame_run);
      return true;
   }
   int texture = 0;
   lima_video_t *vid = (lima_video_t*)data;

   vid->scaler.in_stride = pitch;

   if (width != vid->last_width || height != vid->last_height)
   {
      RARCH_LOG("New game texture size w = %d h = %d\n", width, height);
      SDL_FreeSurface(vid->screen);
      if (vid->scaler.in_fmt == SCALER_FMT_RGB565)
         vid->screen = SDL_CreateRGBSurface(SDL_HWSURFACE | SDL_HWACCEL | SDL_DOUBLEBUF, width, height, 16, 0, 0, 0, 0);
      else
    	 vid->screen = SDL_CreateRGBSurface(SDL_HWSURFACE | SDL_HWACCEL | SDL_DOUBLEBUF, width, height, 32, 0, 0, 0, 0);

      vid->scaler.in_width  = width;
      vid->scaler.in_height = height;

      vid->scaler.out_width  = width;
      vid->scaler.out_height = height; // We do HW Scale so we use the same w and h, scaler is used only for color conversation
      vid->scaler.out_stride = vid->screen->pitch;
      if(vid->scaler.in_fmt == SCALER_FMT_RGB565)
         vid->scaler.out_fmt = SCALER_FMT_RGB565;

      scaler_ctx_gen_filter(&vid->scaler);

      vid->last_width  = width;
      vid->last_height = height;
   }

   if(_rgui)
      return true;

   if (SDL_MUSTLOCK(vid->screen))
      SDL_LockSurface(vid->screen);

   scaler_ctx_scale(&vid->scaler, vid->screen->pixels, frame);
   if (msg)
      lima_render_msg(vid, vid->screen, msg, vid->screen->w, vid->screen->h, vid->screen->format);

   if (SDL_MUSTLOCK(vid->screen))
      SDL_UnlockSurface(vid->screen);

   RARCH_PERFORMANCE_INIT(copy_frame);
   RARCH_PERFORMANCE_START(copy_frame);
   if (vid->scaler.in_fmt == SCALER_FMT_RGB565)
      texture = limare_texture_upload(state, vid->screen->pixels, width, height, LIMA_TEXEL_FORMAT_BGR_565, 0);
   else
      texture = limare_texture_upload(state, vid->screen->pixels, width, height, LIMA_TEXEL_FORMAT_RGBA_8888, 0);
   RARCH_PERFORMANCE_STOP(copy_frame);

   limare_frame_new(state);

   limare_attribute_pointer(state, "in_vertex", LIMARE_ATTRIB_FLOAT,
				 3, 0, 4, vertices);
   limare_attribute_pointer(state, "in_coord", LIMARE_ATTRIB_FLOAT,
				 2, 0, 4, coords+rotate*4);

   limare_texture_attach(state, "in_texture", texture);

   if (limare_draw_arrays(state, GL_TRIANGLE_STRIP, 0, 4))
	  return false;

   if (limare_frame_flush(state))
 	  return false;

   limare_buffer_swap(state);

   g_extern.frame_count++;

   state->textures[0] = 0;
   state->texture_handles = 0;
   state->aux_mem_used = 0;

   RARCH_PERFORMANCE_STOP(frame_run);

   return true;
}

static void lima_gfx_set_nonblock_state(void *data, bool state)
{
   (void)data; // Can SDL even do this?
   (void)state;
}

static bool lima_gfx_alive(void *data)
{
   lima_video_t *vid = (lima_video_t*)data;
   check_window(vid);
   return true;
}

static bool lima_gfx_focus(void *data)
{
   (void)data;
   return (SDL_GetAppState() & (SDL_APPINPUTFOCUS | SDL_APPACTIVE)) == (SDL_APPINPUTFOCUS | SDL_APPACTIVE);
}

static void lima_gfx_set_rotation(void *data, unsigned rotation)
{
   (void)data;
   rotate = rotation;
}

static void lima_gfx_viewport_info(void *data, struct rarch_viewport *vp)
{
   lima_video_t *vid = (lima_video_t*)data;
   vp->x = vp->y = 0;
   vp->width  = vp->full_width  = vid->screen->w;
   vp->height = vp->full_height = vid->screen->h;
}

static void lima_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   (void)data;

   float val = aspectratio_lut[aspect_ratio_idx].value;

   switch (aspect_ratio_idx)
   {
      case ASPECT_RATIO_4_3:
         vertices[0][0] = -0.75;
         vertices[1][0] =  0.75;
         vertices[2][0] = -0.75;
         vertices[3][0] =  0.75;
      break;

      case ASPECT_RATIO_16_9:
      case ASPECT_RATIO_16_10:
      case ASPECT_RATIO_16_15:
         vertices[0][0] = -1;
         vertices[1][0] =  1;
         vertices[2][0] = -1;
         vertices[3][0] =  1;
      break;

      case ASPECT_RATIO_CONFIG:
    	  if(val == 0.0f)
             break;
    	  else
    	  {
             vertices[0][0] = 1/(val/-1);
             vertices[1][0] = 1/val;
             vertices[2][0] = 1/(val/-1);
             vertices[3][0] = 1/val;
    	  }
      break;

      default:
         break;
   }

   g_extern.system.aspect_ratio = aspectratio_lut[aspect_ratio_idx].value;
}

static void lima_apply_state_changes(void *data)
{
   (void)data;
}

static void lima_set_texture_frame(void *data, const void *frame, bool rgb32, unsigned width, unsigned height, float alpha)
{
   int texture;

   if(!rgui_buffer)
      rgui_buffer = (uint32_t*)malloc(width * height * sizeof(uint32_t));

   lima_video_t *vid = (lima_video_t*)data;

   uint32_t *dst = (uint32_t*)rgui_buffer;
   const uint16_t *src = (const uint16_t*)frame;
   for (unsigned h = 0; h < height; h++, dst += width * sizeof(uint32_t) >> 2, src += width)
   {
      for (unsigned w = 0; w < width; w++)
      {
         uint16_t c = src[w];
         uint32_t r = (c >> 12) & 0xf;
         uint32_t g = (c >>  8) & 0xf;
         uint32_t b = (c >>  4) & 0xf;
         uint32_t a = (c >>  0) & 0xf;
         r = ((r << 4) | r) << 16;
         g = ((g << 4) | g) <<  8;
         b = ((b << 4) | b) <<  0;
         a = ((a << 4) | a) << 24;
         dst[w] = r | g | b | a;
      }
   }

   texture = limare_texture_upload(state, rgui_buffer, width, height, LIMA_TEXEL_FORMAT_RGBA_8888, 0);

   limare_frame_new(state);

   limare_attribute_pointer(state, "in_vertex", LIMARE_ATTRIB_FLOAT,
					 3, 0, 4, vertices);
   limare_attribute_pointer(state, "in_coord", LIMARE_ATTRIB_FLOAT,
					 2, 0, 4, coords);

   limare_texture_attach(state, "in_texture", texture);

   if (limare_draw_arrays(state, GL_TRIANGLE_STRIP, 0, 4))
      return;

   if (limare_frame_flush(state))
      return;

   limare_buffer_swap(state);

   state->textures[0] = 0;
   state->texture_handles = 0;
   state->aux_mem_used = 0;
}

static void lima_set_texture_enable(void *data, bool state, bool full_screen)
{
   (void)data;
   _rgui = state;
}

static void lima_set_osd_msg(void *data, const char *msg, void *userdata)
{
   (void)data;
   (void)userdata;
}

static void lima_show_mouse(void *data, bool state)
{
   (void)data;
}

static const video_poke_interface_t lima_poke_interface = {
   NULL,
#ifdef HAVE_FBO
   NULL,
   NULL,
#endif
   lima_set_aspect_ratio,
   lima_apply_state_changes,
#if defined(HAVE_RGUI) || defined(HAVE_RMENU)
   lima_set_texture_frame,
   lima_set_texture_enable,
#endif
   lima_set_osd_msg,

   lima_show_mouse,
};

static void lima_gfx_get_poke_interface(void *data, const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &lima_poke_interface;
}

const video_driver_t video_lima = {
   lima_gfx_init,
   lima_gfx_frame,
   lima_gfx_set_nonblock_state,
   lima_gfx_alive,
   lima_gfx_focus,
   NULL,
   lima_gfx_free,
   "lima",

#ifdef HAVE_MENU
   NULL,
#endif

   lima_gfx_set_rotation,
   lima_gfx_viewport_info,
   NULL,

#ifdef HAVE_OVERLAY
   NULL,
#endif
   lima_gfx_get_poke_interface,
};

