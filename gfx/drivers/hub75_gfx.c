/*  RetroArch - A frontend for libretro.
 *
 *  Raspberry Pi HUB75 RGB LED matrix video driver.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation, either version 3 of the License, or (at your
 *  option) any later version.
 */

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <led-matrix-c.h>
#include <retro_miscellaneous.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../video_driver.h"
#include "../../configuration.h"
#include "../../driver.h"
#include "../../frontend/frontend_driver.h"
#include "../../verbosity.h"

enum hub75_scaling_mode
{
   HUB75_SCALING_FIT = 0,
   HUB75_SCALING_FILL,
   HUB75_SCALING_STRETCH,
   HUB75_SCALING_INTEGER
};

#ifdef HAVE_OVERLAY
typedef struct hub75_overlay
{
   uint32_t *pixels;
   unsigned width;
   unsigned height;
   float tex_x;
   float tex_y;
   float tex_w;
   float tex_h;
   float vert_x;
   float vert_y;
   float vert_w;
   float vert_h;
   float alpha;
   bool supports_rgba;
   bool fullscreen;
} hub75_overlay_t;
#endif

typedef struct hub75
{
   struct RGBLedMatrix *matrix;
   struct LedCanvas *canvas;
   struct Color *pixels;
   unsigned char *menu_frame;
   size_t menu_frame_cap;
   unsigned canvas_width;
   unsigned canvas_height;
   unsigned frame_width;
   unsigned frame_height;
   unsigned frame_pitch;
   unsigned menu_width;
   unsigned menu_height;
   unsigned menu_pitch;
   unsigned menu_bits;
   unsigned rotation;
   unsigned viewport_x;
   unsigned viewport_y;
   unsigned viewport_width;
   unsigned viewport_height;
   enum hub75_scaling_mode scaling;
#ifdef HAVE_OVERLAY
   hub75_overlay_t *overlays;
   unsigned overlays_size;
   bool overlays_enabled;
#endif
   bool rgb32;
   bool menu_enabled;
   bool warned_hw_frame;
} hub75_t;

#ifdef HAVE_OVERLAY
static void hub75_overlay_free(hub75_t *hub75)
{
   unsigned i;

   if (!hub75 || !hub75->overlays)
      return;

   for (i = 0; i < hub75->overlays_size; i++)
      free(hub75->overlays[i].pixels);
   free(hub75->overlays);
   hub75->overlays      = NULL;
   hub75->overlays_size = 0;
}
#endif

static enum hub75_scaling_mode hub75_get_scaling_mode(void)
{
   const char *value = getenv("HUB75_SCALING");

   if (!value || !*value || strcmp(value, "fit") == 0)
      return HUB75_SCALING_FIT;
   if (strcmp(value, "fill") == 0)
      return HUB75_SCALING_FILL;
   if (strcmp(value, "stretch") == 0)
      return HUB75_SCALING_STRETCH;
   if (strcmp(value, "integer") == 0)
      return HUB75_SCALING_INTEGER;

   RARCH_WARN("[HUB75] Ignoring invalid HUB75_SCALING=%s "
         "(expected fit, fill, stretch, or integer).\n", value);
   return HUB75_SCALING_FIT;
}

static const char *hub75_scaling_name(enum hub75_scaling_mode scaling)
{
   switch (scaling)
   {
      case HUB75_SCALING_FILL:
         return "fill";
      case HUB75_SCALING_STRETCH:
         return "stretch";
      case HUB75_SCALING_INTEGER:
         return "integer";
      default:
         return "fit";
   }
}

static int hub75_env_int(const char *name, int min_value, int max_value)
{
   const char *value = getenv(name);
   char *end         = NULL;
   long parsed;

   if (!value || !*value)
      return 0;

   errno  = 0;
   parsed = strtol(value, &end, 10);
   if (errno || end == value || *end || parsed < min_value || parsed > max_value)
   {
      RARCH_WARN("[HUB75] Ignoring invalid %s=%s (expected %d..%d).\n",
            name, value, min_value, max_value);
      return 0;
   }

   return (int)parsed;
}

static void hub75_input_driver(const char *joypad_driver,
      input_driver_t **input, void **input_data)
{
#ifdef HAVE_UDEV
   *input_data = input_driver_init_wrap(&input_udev, joypad_driver);
   if (*input_data)
   {
      *input = &input_udev;
      return;
   }
#else
   (void)joypad_driver;
#endif
   *input      = NULL;
   *input_data = NULL;
}

static void hub75_free(void *data)
{
   hub75_t *hub75 = (hub75_t*)data;

   if (!hub75)
      return;

   if (hub75->canvas)
      led_canvas_clear(hub75->canvas);
   if (hub75->matrix)
      led_matrix_delete(hub75->matrix);

#ifdef HAVE_OVERLAY
   hub75_overlay_free(hub75);
#endif
   free(hub75->menu_frame);
   free(hub75->pixels);
   free(hub75);
}

static void *hub75_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   settings_t *settings = config_get_ptr();
   hub75_t *hub75 = (hub75_t*)calloc(1, sizeof(*hub75));
   struct RGBLedMatrixOptions options;
   struct RGBLedRuntimeOptions runtime;
   int width = 0;
   int height = 0;

   *input      = NULL;
   *input_data = NULL;
   if (!hub75)
      return NULL;

   memset(&options, 0, sizeof(options));
   memset(&runtime, 0, sizeof(runtime));

   options.hardware_mapping     = getenv("HUB75_GPIO_MAPPING");
   options.rows                 = hub75_env_int("HUB75_ROWS", 8, 128);
   options.cols                 = hub75_env_int("HUB75_COLS", 8, 256);
   options.chain_length         = hub75_env_int("HUB75_CHAIN", 1, 64);
   options.parallel             = hub75_env_int("HUB75_PARALLEL", 1, 6);
   options.pwm_bits             = hub75_env_int("HUB75_PWM_BITS", 1, 11);
   options.brightness           = hub75_env_int("HUB75_BRIGHTNESS", 1, 100);
   options.multiplexing         = hub75_env_int("HUB75_MULTIPLEXING", 0, 32);
   options.row_address_type     = hub75_env_int("HUB75_ROW_ADDR_TYPE", 0, 5);
   options.led_rgb_sequence     = getenv("HUB75_RGB_SEQUENCE");
   options.pixel_mapper_config = getenv("HUB75_PIXEL_MAPPER");
   options.panel_type           = getenv("HUB75_PANEL_TYPE");
   runtime.gpio_slowdown        = hub75_env_int("HUB75_GPIO_SLOWDOWN", 0, 10);
   runtime.rp1_pio              = hub75_env_int("HUB75_RP1_PIO", 0, 1);
   runtime.daemon               = 0;
   runtime.drop_privileges      = 0;
   runtime.do_gpio_init         = true;

   hub75->matrix = led_matrix_create_from_options_and_rt_options(
         &options, &runtime);
   if (!hub75->matrix)
   {
      RARCH_ERR("[HUB75] Failed to initialize the RGB LED matrix.\n");
      hub75_free(hub75);
      return NULL;
   }

   hub75->canvas = led_matrix_create_offscreen_canvas(hub75->matrix);
   if (!hub75->canvas)
   {
      RARCH_ERR("[HUB75] Failed to create an offscreen canvas.\n");
      hub75_free(hub75);
      return NULL;
   }

   led_canvas_get_size(hub75->canvas, &width, &height);
   if (width <= 0 || height <= 0 ||
       (size_t)width > SIZE_MAX / (size_t)height / sizeof(*hub75->pixels))
   {
      RARCH_ERR("[HUB75] Invalid matrix geometry %dx%d.\n", width, height);
      hub75_free(hub75);
      return NULL;
   }

   hub75->pixels = (struct Color*)calloc(
         (size_t)width * (size_t)height, sizeof(*hub75->pixels));
   if (!hub75->pixels)
   {
      hub75_free(hub75);
      return NULL;
   }

   hub75->canvas_width    = (unsigned)width;
   hub75->canvas_height   = (unsigned)height;
   hub75->viewport_width  = hub75->canvas_width;
   hub75->viewport_height = hub75->canvas_height;
   hub75->frame_width     = video->width;
   hub75->frame_height    = video->height;
   hub75->frame_pitch     = video->width * (video->rgb32 ? 4 : 2);
   hub75->rgb32           = video->rgb32;
   hub75->menu_enabled    = true;
   hub75->scaling         = hub75_get_scaling_mode();

   hub75_input_driver(settings->arrays.input_joypad_driver, input, input_data);
   frontend_driver_install_signal_handler();
   led_canvas_clear(hub75->canvas);
   hub75->canvas = led_matrix_swap_on_vsync(hub75->matrix, hub75->canvas);

   RARCH_LOG("[HUB75] Initialized %ux%u RGB LED matrix (scaling: %s).\n",
         hub75->canvas_width, hub75->canvas_height,
         hub75_scaling_name(hub75->scaling));
   return hub75;
}

static struct Color hub75_read_pixel(const void *frame, unsigned pitch,
      unsigned x, unsigned y, unsigned bits, bool menu)
{
   struct Color color;

   if (bits == 32)
   {
      const uint32_t *row = (const uint32_t*)((const uint8_t*)frame +
            (size_t)y * pitch);
      uint32_t pixel = row[x];
      color.r = (uint8_t)(pixel >> 16);
      color.g = (uint8_t)(pixel >> 8);
      color.b = (uint8_t)pixel;
   }
   else
   {
      const uint16_t *row = (const uint16_t*)((const uint8_t*)frame +
            (size_t)y * pitch);
      uint16_t pixel = row[x];
      if (menu)
      {
         color.r = (uint8_t)(((pixel >> 12) & 0xf) * 17);
         color.g = (uint8_t)(((pixel >> 8) & 0xf) * 17);
         color.b = (uint8_t)(((pixel >> 4) & 0xf) * 17);
      }
      else
      {
         unsigned r = (pixel >> 11) & 0x1f;
         unsigned g = (pixel >> 5) & 0x3f;
         unsigned b = pixel & 0x1f;
         color.r = (uint8_t)((r << 3) | (r >> 2));
         color.g = (uint8_t)((g << 2) | (g >> 4));
         color.b = (uint8_t)((b << 3) | (b >> 2));
      }
   }

   return color;
}

#ifdef HAVE_OVERLAY
static void hub75_render_overlays(hub75_t *hub75)
{
   unsigned i;

   if (!hub75 || !hub75->overlays_enabled || !hub75->overlays)
      return;

   for (i = 0; i < hub75->overlays_size; i++)
   {
      hub75_overlay_t *overlay = &hub75->overlays[i];
      unsigned base_x;
      unsigned base_y;
      unsigned base_width;
      unsigned base_height;
      int dst_x;
      int dst_y;
      int dst_width;
      int dst_height;
      int start_x;
      int start_y;
      int end_x;
      int end_y;
      int x;
      int y;

      if (!overlay->pixels || !overlay->width || !overlay->height ||
          overlay->alpha <= 0.0f || overlay->tex_w <= 0.0f ||
          overlay->tex_h <= 0.0f)
         continue;

      if (overlay->fullscreen)
      {
         base_x      = 0;
         base_y      = 0;
         base_width  = hub75->canvas_width;
         base_height = hub75->canvas_height;
      }
      else
      {
         base_x      = hub75->viewport_x;
         base_y      = hub75->viewport_y;
         base_width  = hub75->viewport_width;
         base_height = hub75->viewport_height;
      }

      dst_x      = (int)base_x + (int)(overlay->vert_x * base_width);
      dst_y      = (int)base_y + (int)(overlay->vert_y * base_height);
      dst_width  = (int)(overlay->vert_w * base_width + 0.5f);
      dst_height = (int)(overlay->vert_h * base_height + 0.5f);
      if (dst_width <= 0 || dst_height <= 0)
         continue;

      start_x = dst_x < 0 ? 0 : dst_x;
      start_y = dst_y < 0 ? 0 : dst_y;
      end_x   = dst_x + dst_width;
      end_y   = dst_y + dst_height;
      if (end_x > (int)hub75->canvas_width)
         end_x = (int)hub75->canvas_width;
      if (end_y > (int)hub75->canvas_height)
         end_y = (int)hub75->canvas_height;

      for (y = start_y; y < end_y; y++)
      {
         float v = overlay->tex_y +
               ((float)(y - dst_y) / dst_height) * overlay->tex_h;
         int source_y = (int)(v * overlay->height);

         if (source_y < 0)
            source_y = 0;
         else if (source_y >= (int)overlay->height)
            source_y = (int)overlay->height - 1;

         for (x = start_x; x < end_x; x++)
         {
            float u = overlay->tex_x +
                  ((float)(x - dst_x) / dst_width) * overlay->tex_w;
            int source_x = (int)(u * overlay->width);
            uint32_t pixel;
            unsigned alpha;
            unsigned red;
            unsigned green;
            unsigned blue;
            struct Color *destination;

            if (source_x < 0)
               source_x = 0;
            else if (source_x >= (int)overlay->width)
               source_x = (int)overlay->width - 1;

            pixel = overlay->pixels[(size_t)source_y * overlay->width +
                  (unsigned)source_x];
            alpha = (unsigned)(((pixel >> 24) & 0xff) * overlay->alpha);
            if (!alpha)
               continue;
            if (alpha > 255)
               alpha = 255;

            green = (pixel >> 8) & 0xff;
            if (overlay->supports_rgba)
            {
               red  = pixel & 0xff;
               blue = (pixel >> 16) & 0xff;
            }
            else
            {
               red  = (pixel >> 16) & 0xff;
               blue = pixel & 0xff;
            }

            destination = &hub75->pixels[(size_t)y * hub75->canvas_width +
                  (unsigned)x];
            destination->r = (uint8_t)((red * alpha +
                  destination->r * (255 - alpha) + 127) / 255);
            destination->g = (uint8_t)((green * alpha +
                  destination->g * (255 - alpha) + 127) / 255);
            destination->b = (uint8_t)((blue * alpha +
                  destination->b * (255 - alpha) + 127) / 255);
         }
      }
   }
}
#endif

static void hub75_render(hub75_t *hub75, const void *frame,
      unsigned width, unsigned height, unsigned pitch, unsigned bits,
      bool menu, bool render_overlays)
{
   unsigned rotated_width  = (hub75->rotation & 1) ? height : width;
   unsigned rotated_height = (hub75->rotation & 1) ? width : height;
   unsigned output_width   = hub75->canvas_width;
   unsigned output_height  = hub75->canvas_height;
   unsigned draw_width;
   unsigned draw_height;
   unsigned offset_x;
   unsigned offset_y;
   unsigned crop_x      = 0;
   unsigned crop_y      = 0;
   unsigned crop_width  = rotated_width;
   unsigned crop_height = rotated_height;
   unsigned integer_factor = 1;
   bool integer_downscale = false;
   unsigned x;
   unsigned y;

   memset(hub75->pixels, 0, (size_t)output_width * output_height *
         sizeof(*hub75->pixels));

   draw_width  = output_width;
   draw_height = output_height;
   offset_x    = 0;
   offset_y    = 0;

   if (hub75->scaling == HUB75_SCALING_FIT)
   {
      if ((uint64_t)output_width * rotated_height <=
          (uint64_t)output_height * rotated_width)
      {
         draw_width  = output_width;
         draw_height = (unsigned)((uint64_t)rotated_height * output_width /
               rotated_width);
      }
      else
      {
         draw_height = output_height;
         draw_width  = (unsigned)((uint64_t)rotated_width * output_height /
               rotated_height);
      }

      if (!draw_width)
         draw_width = 1;
      if (!draw_height)
         draw_height = 1;
      offset_x = (output_width - draw_width) / 2;
      offset_y = (output_height - draw_height) / 2;
   }
   else if (hub75->scaling == HUB75_SCALING_FILL)
   {
      if ((uint64_t)output_width * rotated_height >=
          (uint64_t)output_height * rotated_width)
      {
         crop_height = (unsigned)((uint64_t)rotated_width * output_height /
               output_width);
         if (!crop_height)
            crop_height = 1;
         crop_y = (rotated_height - crop_height) / 2;
      }
      else
      {
         crop_width = (unsigned)((uint64_t)rotated_height * output_width /
               output_height);
         if (!crop_width)
            crop_width = 1;
         crop_x = (rotated_width - crop_width) / 2;
      }
   }
   else if (hub75->scaling == HUB75_SCALING_INTEGER)
   {
      if (rotated_width <= output_width && rotated_height <= output_height)
      {
         unsigned scale_x = output_width / rotated_width;
         unsigned scale_y = output_height / rotated_height;
         integer_factor = scale_x < scale_y ? scale_x : scale_y;
         if (!integer_factor)
            integer_factor = 1;
         draw_width  = rotated_width * integer_factor;
         draw_height = rotated_height * integer_factor;
      }
      else
      {
         unsigned divisor_x = (rotated_width + output_width - 1) /
               output_width;
         unsigned divisor_y = (rotated_height + output_height - 1) /
               output_height;
         integer_factor = divisor_x > divisor_y ? divisor_x : divisor_y;
         integer_downscale = true;
         draw_width  = (rotated_width + integer_factor - 1) / integer_factor;
         draw_height = (rotated_height + integer_factor - 1) / integer_factor;
      }
      offset_x = (output_width - draw_width) / 2;
      offset_y = (output_height - draw_height) / 2;
   }

   if (!menu)
   {
      hub75->viewport_x      = offset_x;
      hub75->viewport_y      = offset_y;
      hub75->viewport_width  = draw_width;
      hub75->viewport_height = draw_height;
   }

   for (y = 0; y < draw_height; y++)
   {
      unsigned ry = hub75->scaling == HUB75_SCALING_INTEGER
            ? (integer_downscale ? y * integer_factor : y / integer_factor)
            : crop_y + (unsigned)((uint64_t)y * crop_height / draw_height);
      for (x = 0; x < draw_width; x++)
      {
         unsigned rx = hub75->scaling == HUB75_SCALING_INTEGER
               ? (integer_downscale ? x * integer_factor : x / integer_factor)
               : crop_x + (unsigned)((uint64_t)x * crop_width / draw_width);
         unsigned sx;
         unsigned sy;

         switch (hub75->rotation & 3)
         {
            case 1:
               sx = ry;
               sy = height - 1 - rx;
               break;
            case 2:
               sx = width - 1 - rx;
               sy = height - 1 - ry;
               break;
            case 3:
               sx = width - 1 - ry;
               sy = rx;
               break;
            default:
               sx = rx;
               sy = ry;
               break;
         }

         hub75->pixels[(size_t)(offset_y + y) * output_width + offset_x + x] =
               hub75_read_pixel(frame, pitch, sx, sy, bits, menu);
      }
   }

#ifdef HAVE_OVERLAY
   if (render_overlays)
      hub75_render_overlays(hub75);
#else
   (void)render_overlays;
#endif

   led_canvas_set_pixels(hub75->canvas, 0, 0,
         (int)output_width, (int)output_height, hub75->pixels);
   hub75->canvas = led_matrix_swap_on_vsync(hub75->matrix, hub75->canvas);
}

static bool hub75_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   hub75_t *hub75 = (hub75_t*)data;
   const void *source = frame;
   unsigned width;
   unsigned height;
   unsigned bits;
   bool menu = false;
   bool render_overlays = true;
#ifdef HAVE_MENU
   bool menu_alive = video_info &&
         (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE);
   menu_driver_frame(menu_alive, video_info);
#else
   (void)video_info;
#endif
   (void)frame_count;
   (void)msg;

   if (!hub75 || !frame || !frame_width || !frame_height)
      return true;

   if (frame == RETRO_HW_FRAME_BUFFER_VALID)
   {
      if (!hub75->warned_hw_frame)
      {
         RARCH_WARN("[HUB75] Hardware-rendered frames are not supported.\n");
         hub75->warned_hw_frame = true;
      }
      return true;
   }

   if (frame_width > 4 && frame_height > 4)
   {
      hub75->frame_width  = frame_width;
      hub75->frame_height = frame_height;
      hub75->frame_pitch  = pitch;
   }

#ifdef HAVE_MENU
   if (hub75->menu_enabled && menu_alive && hub75->menu_frame)
   {
      source = hub75->menu_frame;
      width  = hub75->menu_width;
      height = hub75->menu_height;
      pitch  = hub75->menu_pitch;
      bits   = hub75->menu_bits;
      menu   = true;
      if (video_info && video_info->overlay_behind_menu)
         render_overlays = false;
   }
   else
#endif
   {
      if (frame_width == 4 && frame_height == 4)
         return true;
      width  = hub75->frame_width;
      height = hub75->frame_height;
      pitch  = hub75->frame_pitch;
      bits   = hub75->rgb32 ? 32 : 16;
   }

   if (source && width && height && pitch)
      hub75_render(hub75, source, width, height, pitch, bits, menu,
            render_overlays);
   return true;
}

static void hub75_set_nonblock_state(void *data, bool toggle,
      bool adaptive_vsync_enabled, unsigned swap_interval)
{
   (void)data;
   (void)toggle;
   (void)adaptive_vsync_enabled;
   (void)swap_interval;
}

static bool hub75_alive(void *data)
{
   (void)data;
   return !frontend_driver_get_signal_handler_state();
}

static bool hub75_focus(void *data) { (void)data; return true; }
static bool hub75_suppress_screensaver(void *data, bool enable)
{ (void)data; (void)enable; return false; }
static bool hub75_has_windowed(void *data) { (void)data; return false; }
static bool hub75_set_shader(void *data, enum rarch_shader_type type,
      const char *path)
{ (void)data; (void)type; (void)path; return false; }
static void hub75_set_viewport(void *data, unsigned width, unsigned height,
      bool force_full, bool allow_rotate)
{ (void)data; (void)width; (void)height; (void)force_full; (void)allow_rotate; }

static void hub75_set_rotation(void *data, unsigned rotation)
{
   hub75_t *hub75 = (hub75_t*)data;
   if (hub75)
      hub75->rotation = rotation & 3;
}

static void hub75_viewport_info(void *data, struct video_viewport *vp)
{
   hub75_t *hub75 = (hub75_t*)data;
   if (!hub75 || !vp)
      return;
   vp->x           = (int)hub75->viewport_x;
   vp->y           = (int)hub75->viewport_y;
   vp->width       = hub75->viewport_width;
   vp->height      = hub75->viewport_height;
   vp->full_width  = hub75->canvas_width;
   vp->full_height = hub75->canvas_height;
}

#ifdef HAVE_OVERLAY
static bool hub75_overlay_load(void *data, const void *image_data,
      unsigned num_images)
{
   hub75_t *hub75 = (hub75_t*)data;
   const struct texture_image *images =
         (const struct texture_image*)image_data;
   unsigned i;

   if (!hub75)
      return false;

   hub75_overlay_free(hub75);
   if (!num_images || !images)
      return true;

   hub75->overlays = (hub75_overlay_t*)calloc(num_images,
         sizeof(*hub75->overlays));
   if (!hub75->overlays)
      return false;
   hub75->overlays_size = num_images;

   for (i = 0; i < num_images; i++)
   {
      hub75_overlay_t *overlay = &hub75->overlays[i];
      size_t pixel_count;

      overlay->alpha  = 1.0f;
      overlay->tex_w  = 1.0f;
      overlay->tex_h  = 1.0f;
      overlay->vert_w = 1.0f;
      overlay->vert_h = 1.0f;
      if (!images[i].pixels || !images[i].width || !images[i].height)
         continue;
      if ((size_t)images[i].width >
          SIZE_MAX / (size_t)images[i].height / sizeof(uint32_t))
      {
         hub75_overlay_free(hub75);
         return false;
      }

      pixel_count = (size_t)images[i].width * images[i].height;
      overlay->pixels = (uint32_t*)malloc(pixel_count * sizeof(uint32_t));
      if (!overlay->pixels)
      {
         hub75_overlay_free(hub75);
         return false;
      }
      memcpy(overlay->pixels, images[i].pixels,
            pixel_count * sizeof(uint32_t));
      overlay->width         = images[i].width;
      overlay->height        = images[i].height;
      overlay->supports_rgba = images[i].supports_rgba;
   }

   return true;
}

static void hub75_overlay_enable(void *data, bool state)
{
   hub75_t *hub75 = (hub75_t*)data;
   if (hub75)
      hub75->overlays_enabled = state;
}

static void hub75_overlay_tex_geom(void *data, unsigned index,
      float x, float y, float width, float height)
{
   hub75_t *hub75 = (hub75_t*)data;
   hub75_overlay_t *overlay;

   if (!hub75 || index >= hub75->overlays_size)
      return;
   overlay        = &hub75->overlays[index];
   overlay->tex_x = x;
   overlay->tex_y = y;
   overlay->tex_w = width;
   overlay->tex_h = height;
}

static void hub75_overlay_vertex_geom(void *data, unsigned index,
      float x, float y, float width, float height)
{
   hub75_t *hub75 = (hub75_t*)data;
   hub75_overlay_t *overlay;

   if (!hub75 || index >= hub75->overlays_size)
      return;
   overlay         = &hub75->overlays[index];
   overlay->vert_x = x;
   overlay->vert_y = y;
   overlay->vert_w = width;
   overlay->vert_h = height;
}

static void hub75_overlay_full_screen(void *data, bool enable)
{
   hub75_t *hub75 = (hub75_t*)data;
   unsigned i;

   if (!hub75)
      return;
   for (i = 0; i < hub75->overlays_size; i++)
      hub75->overlays[i].fullscreen = enable;
}

static void hub75_overlay_set_alpha(void *data, unsigned index, float mod)
{
   hub75_t *hub75 = (hub75_t*)data;
   if (!hub75 || index >= hub75->overlays_size)
      return;
   if (mod < 0.0f)
      mod = 0.0f;
   else if (mod > 1.0f)
      mod = 1.0f;
   hub75->overlays[index].alpha = mod;
}

static const video_overlay_interface_t hub75_overlay_interface = {
   hub75_overlay_enable,
   hub75_overlay_load,
   hub75_overlay_tex_geom,
   hub75_overlay_vertex_geom,
   hub75_overlay_full_screen,
   hub75_overlay_set_alpha
};

static void hub75_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface)
{
   (void)data;
   *iface = &hub75_overlay_interface;
}
#endif

#ifdef HAVE_MENU
static void hub75_set_texture_frame(void *data, const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   hub75_t *hub75 = (hub75_t*)data;
   unsigned pitch = width * (rgb32 ? 4 : 2);
   size_t required;
   unsigned char *new_frame;
   (void)alpha;

   if (!hub75 || !frame || !width || !height)
      return;
   if ((size_t)height > SIZE_MAX / pitch)
      return;
   required = (size_t)pitch * height;
   if (required > hub75->menu_frame_cap)
   {
      new_frame = (unsigned char*)realloc(hub75->menu_frame, required);
      if (!new_frame)
         return;
      hub75->menu_frame     = new_frame;
      hub75->menu_frame_cap = required;
   }
   memcpy(hub75->menu_frame, frame, required);
   hub75->menu_width  = width;
   hub75->menu_height = height;
   hub75->menu_pitch  = pitch;
   hub75->menu_bits   = rgb32 ? 32 : 16;
}

static void hub75_set_texture_enable(void *data, bool enable, bool full_screen)
{
   hub75_t *hub75 = (hub75_t*)data;
   (void)full_screen;
   if (hub75)
      hub75->menu_enabled = enable;
}
#endif

static const video_poke_interface_t hub75_poke_interface = {
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
   NULL, /* set_aspect_ratio */
   NULL, /* apply_state_changes */
#ifdef HAVE_MENU
   hub75_set_texture_frame,
   hub75_set_texture_enable,
#else
   NULL, /* set_texture_frame */
   NULL, /* set_texture_enable */
#endif
   NULL, /* set_osd_msg */
   NULL, /* show_mouse */
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_menu_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_expand_gamut */
   NULL, /* set_hdr_scanlines */
   NULL, /* set_hdr_subpixel_layout */
   NULL, /* supports_texture_format */
   NULL  /* load_texture_compressed */
};

static void hub75_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &hub75_poke_interface;
}

video_driver_t video_hub75 = {
   hub75_init,
   hub75_frame,
   hub75_set_nonblock_state,
   hub75_alive,
   hub75_focus,
   hub75_suppress_screensaver,
   hub75_has_windowed,
   hub75_set_shader,
   hub75_free,
   "hub75",
   hub75_set_viewport,
   hub75_set_rotation,
   hub75_viewport_info,
   NULL, /* read_viewport */
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   hub75_get_overlay_interface,
#endif
   hub75_get_poke_interface,
   NULL, /* wrap_type_to_enum */
   NULL, /* shader_load_begin */
   NULL, /* shader_load_step */
#ifdef HAVE_GFX_WIDGETS
   NULL, /* gfx_widgets_enabled */
#endif
   NULL, /* invalidate_hw_render_cache */
   NULL  /* read_viewport_hdr */
};
