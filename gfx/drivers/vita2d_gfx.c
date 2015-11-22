/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015 - Sergi Granell (xerpi)
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

#include "../../defines/psp_defines.h"
#include "../../general.h"
#include "../../driver.h"

typedef struct vita_menu_frame
{
   bool active;
   int width;
   int height;
   vita2d_texture *texture;
} vita_menu_t;

#ifdef HAVE_OVERLAY
struct vita_overlay_data
{
   vita2d_texture *tex;
   float x;
   float y; 
   float w; 
   float h;
   float tex_x;
   float tex_y; 
   float tex_w; 
   float tex_h;
   float alpha_mod;
   float width;
   float height;
};
#endif

typedef struct vita_video
{
   vita2d_texture *texture;
   SceGxmTextureFormat format;
   int width;
   int height;
   SceGxmTextureFilter tex_filter;

   bool fullscreen;
   bool vsync;
   bool rgb32;

   video_viewport_t vp;

   unsigned rotation;
   bool vblank_not_reached;
   bool keep_aspect;
   bool should_resize;

   vita_menu_t menu;
   
#ifdef HAVE_OVERLAY
   struct vita_overlay_data *overlay;
   unsigned overlays;
   bool overlay_enable;
   bool overlay_full_screen;
#endif
   
} vita_video_t;

static void *vita2d_gfx_init(const video_info_t *video,
      const input_driver_t **input, void **input_data)
{
   vita_video_t *vita = (vita_video_t *)calloc(1, sizeof(vita_video_t));
   driver_t            *driver = driver_get_ptr();
   settings_t *settings = config_get_ptr();

   if (!vita)
      return NULL;

   *input         = NULL;
   *input_data    = NULL;

   RARCH_LOG("vita2d_gfx_init: w: %i  h: %i\n", video->width, video->height);
   RARCH_LOG("RARCH_SCALE_BASE: %i input_scale: %i = %i\n",
	RARCH_SCALE_BASE, video->input_scale, RARCH_SCALE_BASE * video->input_scale);

   vita2d_init();
   vita2d_set_clear_color(RGBA8(0x00, 0x00, 0x00, 0xFF));
   vita2d_set_vblank_wait(video->vsync);

   if (vita->rgb32)
   {
      RARCH_LOG("Format: SCE_GXM_TEXTURE_FORMAT_X8U8U8U8_1RGB\n");
      vita->format = SCE_GXM_TEXTURE_FORMAT_X8U8U8U8_1RGB;
   }
   else
   {
      RARCH_LOG("Format: SCE_GXM_TEXTURE_FORMAT_R5G6B5\n");
      vita->format = SCE_GXM_TEXTURE_FORMAT_R5G6B5;
   }

   vita->fullscreen = video->fullscreen;

   vita->texture      = NULL;
   vita->menu.texture = NULL;
   vita->menu.active  = 0;
   vita->menu.width   = 0;
   vita->menu.height  = 0;

   vita->vsync        = video->vsync;
   vita->rgb32        = video->rgb32;

   vita->tex_filter = video->smooth? SCE_GXM_TEXTURE_FILTER_LINEAR : SCE_GXM_TEXTURE_FILTER_POINT;

   if (input && input_data)
   {
      void *pspinput = input_psp.init();
      *input      = pspinput ? &input_psp : NULL;
      *input_data = pspinput;
   }

   vita->keep_aspect        = true;
   vita->should_resize      = true;
#ifdef HAVE_OVERLAY
   vita->overlay_enable           = false;
#endif
   if (!font_init_first((const void**)&driver->font_osd_driver, &driver->font_osd_data,
          vita, *settings->video.font_path 
          ? settings->video.font_path : NULL, settings->video.font_size,
          FONT_DRIVER_RENDER_VITA2D))
   {
      RARCH_ERR("Font: Failed to initialize font renderer.\n");
        return false;
   }
   return vita;
}

#ifdef HAVE_OVERLAY
static void vita2d_render_overlay(void *data);
static void vita2d_free_overlay(vita_video_t *vita)
{
   for (unsigned i = 0; i < vita->overlays; i++)
   {
      vita2d_free_texture(vita->overlay[i].tex);
   }
   free(vita->overlay);
   vita->overlay = NULL;
   vita->overlays = 0;
   //GX_InvalidateTexAll();
}
#endif

static void vita2d_gfx_update_viewport(vita_video_t* vita);

static bool vita2d_gfx_frame(void *data, const void *frame,
      unsigned width, unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg)
{
   void *tex_p;
   vita_video_t *vita = (vita_video_t *)data;

   (void)frame;
   (void)width;
   (void)height;
   (void)pitch;
   (void)msg;

   if (frame)
   {
      unsigned i, j;
      unsigned int stride;

      if ((width != vita->width || height != vita->height) && vita->texture)
      {
         vita2d_free_texture(vita->texture);
         vita->texture = NULL;
      }

      if (!vita->texture)
      {
         RARCH_LOG("Creating texture: %ix%i\n", width, height);
         vita->width = width;
         vita->height = height;
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
            for (j = 0; j < width; j++)
               tex32[j + i*stride] = frame32[j + i*pitch];
      }
      else
      {
         stride                 /= 2;
         pitch                  /= 2;
         uint16_t *tex16         = tex_p;
         const uint16_t *frame16 = frame;

         for (i = 0; i < height; i++)
            for (j = 0; j < width; j++)
               tex16[j + i*stride] = frame16[j + i*pitch];
      }
   }

   if (vita->should_resize)
      vita2d_gfx_update_viewport(vita);

   vita2d_start_drawing();
   vita2d_clear_screen();

   if (vita->texture)
   {
      if (vita->fullscreen)
         vita2d_draw_texture_scale(vita->texture,
               0, 0,
               PSP_FB_WIDTH  / (float)vita->width,
               PSP_FB_HEIGHT / (float)vita->height);
      else
      {
         const float radian = 90 * 0.0174532925f;
         const float rad = vita->rotation * radian;
         float scalex = vita->vp.width / (float)vita->width;
         float scaley = vita->vp.height / (float)vita->height;
         vita2d_draw_texture_scale_rotate(vita->texture, vita->vp.x,
               vita->vp.y, scalex, scaley, rad);
      }
   }

#ifdef HAVE_OVERLAY
   if (vita->overlay_enable)
      vita2d_render_overlay(vita);
#endif

   if (vita->menu.active && vita->menu.texture)
   {
      if (vita->fullscreen)
         vita2d_draw_texture_scale(vita->menu.texture,
               0, 0,
               PSP_FB_WIDTH  / (float)vita->menu.width,
               PSP_FB_HEIGHT / (float)vita->menu.height);
      else
      {
         if (vita->menu.width > vita->menu.height)
         {
            float scale = PSP_FB_HEIGHT / (float)vita->menu.height;
            float w = vita->menu.width * scale;
            vita2d_draw_texture_scale(vita->menu.texture,
                  PSP_FB_WIDTH / 2.0f - w/2.0f, 0.0f,
                  scale, scale);
         }
         else
         {
            float scale = PSP_FB_WIDTH / (float)vita->menu.width;
            float h = vita->menu.height * scale;
            vita2d_draw_texture_scale(vita->menu.texture,
                  0.0f, PSP_FB_HEIGHT / 2.0f - h/2.0f,
                  scale, scale);
         }
      }
   }
   
   if(msg&&strcmp(msg,"")){
     driver_t          *driver = driver_get_ptr();
     const font_renderer_t *font_ctx = driver->font_osd_driver;
     
     if (font_ctx->render_msg)
       font_ctx->render_msg(driver->font_osd_data, msg, NULL);
   }
   
   vita2d_end_drawing();
   vita2d_swap_buffers();

   return true;
}

static void vita2d_gfx_set_nonblock_state(void *data, bool toggle)
{
   vita_video_t *vita = (vita_video_t *)data;

   if (vita)
   {
      vita->vsync = !toggle;
      vita2d_set_vblank_wait(vita->vsync);
   }
}

static bool vita2d_gfx_alive(void *data)
{
   (void)data;
   return true;
}

static bool vita2d_gfx_focus(void *data)
{
   (void)data;
   return true;
}

static bool vita2d_gfx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static bool vita2d_gfx_has_windowed(void *data)
{
   (void)data;
   return true;
}

static void vita2d_gfx_free(void *data)
{
   vita_video_t *vita = (vita_video_t *)data;

   RARCH_LOG("vita2d_gfx_free()\n");

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

   RARCH_LOG("vita2d_gfx_free() done\n");
}

static bool vita2d_gfx_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

static void vita2d_gfx_update_viewport(vita_video_t* vita)
{
   int x                = 0;
   int y                = 0;
   float device_aspect  = ((float)PSP_FB_WIDTH) / PSP_FB_HEIGHT;
   float width          = PSP_FB_WIDTH;
   float height         = PSP_FB_HEIGHT;
   settings_t *settings = config_get_ptr();

   if (settings->video.scale_integer)
   {
      video_viewport_get_scaled_integer(&vita->vp, PSP_FB_WIDTH,
            PSP_FB_HEIGHT, video_driver_get_aspect_ratio(), vita->keep_aspect);
      width  = vita->vp.width;
      height = vita->vp.height;
   }
   else if (vita->keep_aspect)
   {
      float desired_aspect = video_driver_get_aspect_ratio();
      if (vita->rotation == ORIENTATION_VERTICAL ||
            vita->rotation == ORIENTATION_FLIPPED_ROTATED){
              device_aspect = 1.0 / device_aspect;
              width = PSP_FB_HEIGHT;
              height = PSP_FB_WIDTH;
            }
#if defined(HAVE_MENU)
      if (settings->video.aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         struct video_viewport *custom = video_viewport_get_custom();

         if (custom)
         {
            x      = custom->x;
            y      = custom->y;
            width  = custom->width;
            height = custom->height;
         }
      }
      else
#endif
      {
         float delta;

         if ((fabsf(device_aspect - desired_aspect) < 0.0001f))
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

         if (vita->rotation == ORIENTATION_VERTICAL ||
               vita->rotation == ORIENTATION_FLIPPED_ROTATED){
                 x = (PSP_FB_WIDTH - width) * 0.5f;
                 y = (PSP_FB_HEIGHT - height) * 0.5f;
               }
      }

      vita->vp.x      = x;
      vita->vp.y      = y;
      vita->vp.width  = width;
      vita->vp.height = height;
   }
   else
   {
      vita->vp.x = vita->vp.y = 0;
      vita->vp.width = width;
      vita->vp.height = height;
   }

   vita->vp.width += vita->vp.width&0x1;
   vita->vp.height += vita->vp.height&0x1;

   vita->should_resize = false;

}

static void vita2d_gfx_set_rotation(void *data,
      unsigned rotation)
{
  vita_video_t *vita = (vita_video_t*)data;

  if (!vita)
     return;

  vita->rotation = rotation;
  vita->should_resize = true;
}

static void vita2d_gfx_viewport_info(void *data,
      struct video_viewport *vp)
{
    vita_video_t *vita = (vita_video_t*)data;

    if (vita)
       *vp = vita->vp;
}

static bool vita2d_gfx_read_viewport(void *data, uint8_t *buffer)
{
   (void)data;
   (void)buffer;

   return true;
}

static void vita_set_filtering(void *data, unsigned index, bool smooth)
{
   vita_video_t *vita = (vita_video_t *)data;

   if (vita)
   {
      vita->tex_filter = smooth? SCE_GXM_TEXTURE_FILTER_LINEAR : SCE_GXM_TEXTURE_FILTER_POINT;
      vita2d_texture_set_filters(vita->texture,vita->tex_filter,vita->tex_filter);
   }
}

static void vita_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   vita_video_t *vita = (vita_video_t*)data;
   enum rarch_display_ctl_state cmd = RARCH_DISPLAY_CTL_NONE;

   switch (aspect_ratio_idx)
   {
      case ASPECT_RATIO_SQUARE:
         cmd = RARCH_DISPLAY_CTL_SET_VIEWPORT_SQUARE_PIXEL;
         break;

      case ASPECT_RATIO_CORE:
         cmd = RARCH_DISPLAY_CTL_SET_VIEWPORT_CORE;
         break;

      case ASPECT_RATIO_CONFIG:
         cmd = RARCH_DISPLAY_CTL_SET_VIEWPORT_CONFIG;
         break;

      default:
         break;
   }

   if (cmd != RARCH_DISPLAY_CTL_NONE)
      video_driver_ctl(cmd, NULL);

   video_driver_set_aspect_ratio_value(aspectratio_lut[aspect_ratio_idx].value);

   vita->keep_aspect = true;
   vita->should_resize = true;
}

static void vita_apply_state_changes(void *data)
{
  vita_video_t *vita = (vita_video_t*)data;

  if (vita)
     vita->should_resize = true;
}

static void vita_set_texture_frame(void *data, const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   int i, j;
   void *tex_p;
   unsigned int stride;
   vita_video_t *vita = (vita_video_t*)data;

   (void)alpha;

   if (width != vita->menu.width && height != vita->menu.height && vita->menu.texture)
   {
      vita2d_free_texture(vita->menu.texture);
      vita->menu.texture = NULL;
   }

   if (!vita->menu.texture)
   {
      if (rgb32)
      {
         vita->menu.texture = vita2d_create_empty_texture(width, height);
         RARCH_LOG("Creating Frame RGBA8 texture: w: %i  h: %i\n", width, height);
      }
      else
      {
         vita->menu.texture = vita2d_create_empty_texture_format(width, height, SCE_GXM_TEXTURE_FORMAT_U4U4U4U4_RGBA);
         RARCH_LOG("Creating Frame R5G6B5 texture: w: %i  h: %i\n", width, height);
      }
      vita->menu.width = width;
      vita->menu.height = height;
   }
   vita2d_texture_set_filters(vita->menu.texture,SCE_GXM_TEXTURE_FILTER_LINEAR,SCE_GXM_TEXTURE_FILTER_LINEAR);
   tex_p = vita2d_texture_get_datap(vita->menu.texture);
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

static void vita_set_texture_enable(void *data, bool state, bool full_screen)
{
   vita_video_t *vita = (vita_video_t*)data;
   (void)full_screen;

   vita->menu.active = state;
}

static const video_poke_interface_t vita_poke_interface = {
   NULL,
   vita_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   vita_set_aspect_ratio,
   vita_apply_state_changes,
#ifdef HAVE_MENU
   vita_set_texture_frame,
   vita_set_texture_enable,
#endif
   NULL,
   NULL,
   NULL
 };

static void vita2d_gfx_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &vita_poke_interface;
}

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
      struct vita_overlay_data *o = (struct vita_overlay_data*)&vita->overlay[i];
      o->width = images[i].width;
      o->height = images[i].height;
      o->tex = vita2d_create_empty_texture_format(o->width , o->height, SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ARGB);
      vita2d_texture_set_filters(o->tex,SCE_GXM_TEXTURE_FILTER_LINEAR,SCE_GXM_TEXTURE_FILTER_LINEAR);
      stride = vita2d_texture_get_stride(o->tex);
      stride                     /= 4;
      uint32_t             *tex32 = vita2d_texture_get_datap(o->tex);
      const uint32_t     *frame32 = images[i].pixels;
      pitch = o->width;
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
   vita_video_t *vita = (vita_video_t*)data;
   struct vita_overlay_data *o;
   
   o = NULL;

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
   vita_video_t *vita = (vita_video_t*)data;
   struct vita_overlay_data *o;
   
   o = NULL;
   
   /* Flipped, so we preserve top-down semantics. */
   /*y = 1.0f - y;
   h = -h;*/

   if (vita)
      o = (struct vita_overlay_data*)&vita->overlay[image];

   if (o)
   {
      
      o->w = w*PSP_FB_WIDTH/o->width;
      o->h = h*PSP_FB_HEIGHT/o->height;
      o->x = PSP_FB_WIDTH*(1-w)/2+x;
      o->y = PSP_FB_HEIGHT*(1-h)/2+y;
   }
}

static void vita2d_overlay_enable(void *data, bool state)
{
   vita_video_t *vita = (vita_video_t*)data;
   vita->overlay_enable = state;
}

static void vita2d_overlay_full_screen(void *data, bool enable)
{
   vita_video_t *vita = (vita_video_t*)data;
   vita->overlay_full_screen = enable;
}

static void vita2d_overlay_set_alpha(void *data, unsigned image, float mod)
{
   vita_video_t *vita = (vita_video_t*)data;
   vita->overlay[image].alpha_mod = mod;
}

static void vita2d_render_overlay(void *data)
{
   vita_video_t *vita = (vita_video_t*)data;
   for (unsigned i = 0; i < vita->overlays; i++)
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
                                              //RGBA8(0x00, 0x00, 0x00, (uint8_t)(vita->overlay[i].alpha_mod * 255.0f)));
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
   (void)data;
   *iface = &vita2d_overlay_interface;
}
#endif

video_driver_t video_vita2d = {
   vita2d_gfx_init,
   vita2d_gfx_frame,
   vita2d_gfx_set_nonblock_state,
   vita2d_gfx_alive,
   vita2d_gfx_focus,
   vita2d_gfx_suppress_screensaver,
   vita2d_gfx_has_windowed,
   vita2d_gfx_set_shader,
   vita2d_gfx_free,
   "vita2d",
   NULL, /* set_viewport */
   vita2d_gfx_set_rotation,
   vita2d_gfx_viewport_info,
   vita2d_gfx_read_viewport,
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   vita2d_get_overlay_interface,
#endif
   vita2d_gfx_get_poke_interface,
};
