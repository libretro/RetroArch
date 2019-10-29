#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <boolean.h>
#include <lists/dir_list.h>
#include <file/file_path.h>
#include <compat/strl.h>
#include <retro_environment.h>

#include <streams/file_stream.h>

#if defined(HAVE_RPNG) || defined(HAVE_RJPEG) || defined(HAVE_RTGA) || defined(HAVE_RBMP)
#define PREFER_NON_STB_IMAGE
#endif

#if defined(HAVE_STB_IMAGE) && !defined(PREFER_NON_STB_IMAGE)
#define STB_IMAGE_IMPLEMENTATION

#if 0
#define STBI_NO_PSD
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM
#endif
#define STBI_SUPPORT_ZLIB

#ifdef RARCH_INTERNAL
#include "../../deps/stb/stb_image.h"
#else
#include <stb_image.h>
#endif
#else
#include <formats/image.h>
#endif

#include <libretro.h>

#ifdef RARCH_INTERNAL
#include "internal_cores.h"
#define IMAGE_CORE_PREFIX(s) libretro_imageviewer_##s
#else
#define IMAGE_CORE_PREFIX(s) s
#endif

static retro_log_printf_t IMAGE_CORE_PREFIX(log_cb);
static retro_video_refresh_t IMAGE_CORE_PREFIX(video_cb);
static retro_input_poll_t IMAGE_CORE_PREFIX(input_poll_cb);
static retro_input_state_t IMAGE_CORE_PREFIX(input_state_cb);
static retro_audio_sample_batch_t IMAGE_CORE_PREFIX(audio_batch_cb);
static retro_environment_t IMAGE_CORE_PREFIX(environ_cb);

static bool      process_new_image;
static uint32_t* image_buffer;
#ifndef STB_IMAGE_IMPLEMENTATION
static struct texture_image image_texture;
#endif
static int       image_width;
static int       image_height;
static bool      image_uploaded;
static bool      slideshow_enable;
static struct string_list *image_file_list;

#if 0
#define DUPE_TEST
#endif

#ifdef STB_IMAGE_IMPLEMENTATION
static const char* IMAGE_CORE_PREFIX(valid_extensions) = "jpg|jpeg|png|bmp|psd|tga|gif|hdr|pic|ppm|pgm";
#else

static const char image_formats[] =

#ifdef HAVE_RJPEG
"|jpg|jpeg"
#endif

#ifdef HAVE_RPNG
"|png"
#endif

#ifdef HAVE_RBMP
"|bmp"
#endif

#ifdef HAVE_RTGA
"|tga"
#endif

#if !defined(HAVE_RJPEG) && !defined(HAVE_RPNG) && !defined(HAVE_RBMP) && !defined(HAVE_RTGA)
#error "can't build this core with no image formats"
#endif
;

/* to remove the first |, the alternative is 25 extra lines of ifdef/etc */
static const char* IMAGE_CORE_PREFIX(valid_extensions) = image_formats + 1;

#endif

void IMAGE_CORE_PREFIX(retro_get_system_info)(struct retro_system_info *info)
{
   info->library_name     = "image display";
   info->library_version  = "v0.1";
   info->need_fullpath    = true;
   info->block_extract    = false;
   info->valid_extensions = IMAGE_CORE_PREFIX(valid_extensions);
}

void IMAGE_CORE_PREFIX(retro_get_system_av_info)(struct retro_system_av_info *info)
{
   info->geometry.base_width   = image_width;
   info->geometry.base_height  = image_height;
   info->geometry.max_width    = image_width;
   info->geometry.max_height   = image_height;
   info->geometry.aspect_ratio = 0;
   info->timing.fps            = 60.0;
   info->timing.sample_rate    = 44100.0;
}

static void imageviewer_reset(void)
{
   image_buffer = NULL;
   image_width  = 0;
   image_height = 0;
}

void IMAGE_CORE_PREFIX(retro_init)(void)
{
   struct retro_log_callback log;

   if (IMAGE_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      IMAGE_CORE_PREFIX(log_cb) = log.log;
   else
      IMAGE_CORE_PREFIX(log_cb) = NULL;

   imageviewer_reset();

}

static void imageviewer_free_image(void)
{
#ifdef STB_IMAGE_IMPLEMENTATION
   if (image_buffer)
      free(image_buffer);
#else
   image_texture_free(&image_texture);
#endif
   image_buffer = NULL;
}

void IMAGE_CORE_PREFIX(retro_deinit)(void)
{
   imageviewer_free_image();
   imageviewer_reset();
}

void IMAGE_CORE_PREFIX(retro_set_environment)(retro_environment_t cb)
{
   static const struct retro_variable vars[] = {
      { NULL, NULL },
   };
#ifndef RARCH_INTERNAL
   struct retro_vfs_interface_info vfs_iface_info = { 1, NULL };
#endif

   IMAGE_CORE_PREFIX(environ_cb) = cb;

   cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)vars);

#ifndef RARCH_INTERNAL
   /* I don't trust filestream_vfs_init to work inside rarch */
   if (environ_cb(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &vfs_iface_info))
      filestream_vfs_init(&vfs_iface_info);
#endif
}

void IMAGE_CORE_PREFIX(retro_set_video_refresh)(retro_video_refresh_t cb)
{
   IMAGE_CORE_PREFIX(video_cb) = cb;
}

void IMAGE_CORE_PREFIX(retro_set_audio_sample)(retro_audio_sample_t unused)
{
}

void IMAGE_CORE_PREFIX(retro_set_audio_sample_batch)(retro_audio_sample_batch_t cb)
{
   IMAGE_CORE_PREFIX(audio_batch_cb) = cb;
}

void IMAGE_CORE_PREFIX(retro_set_input_poll)(retro_input_poll_t cb)
{
   IMAGE_CORE_PREFIX(input_poll_cb) = cb;
}

void IMAGE_CORE_PREFIX(retro_set_input_state)(retro_input_state_t cb)
{
   IMAGE_CORE_PREFIX(input_state_cb) = cb;
}

void IMAGE_CORE_PREFIX(retro_set_controller_port_device)(unsigned a, unsigned b)
{
}

void IMAGE_CORE_PREFIX(retro_reset)(void)
{
   image_uploaded = false;
}

size_t IMAGE_CORE_PREFIX(retro_serialize_size)(void)
{
   return 0;
}

bool IMAGE_CORE_PREFIX(retro_serialize)(void *data, size_t size)
{
   (void)data;
   (void)size;
   return false;
}

bool IMAGE_CORE_PREFIX(retro_unserialize)(const void *data, size_t size)
{
   (void)data;
   (void)size;
   return false;
}

void IMAGE_CORE_PREFIX(retro_cheat_reset)(void)
{
}

void IMAGE_CORE_PREFIX(retro_cheat_set)(unsigned a, bool b, const char * c)
{
}

static bool imageviewer_load(const char *path, int image_index)
{
#ifdef STB_IMAGE_IMPLEMENTATION
   int comp;
   RFILE* f;
   size_t len;
   void* buf;
#endif
#ifdef RARCH_INTERNAL
   extern bool video_driver_supports_rgba(void);
#endif

   imageviewer_free_image();

#ifdef STB_IMAGE_IMPLEMENTATION
   f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   len = filestream_get_size(f);
   buf = malloc(len);
   filestream_read(f, buf, len);
   filestream_close(f);

   image_buffer           = (uint32_t*)stbi_load_from_memory(
         buf, len,
         &image_width, &image_height,
         &comp, 4);
   free(buf);
#else
#ifdef RARCH_INTERNAL
   image_texture.supports_rgba = video_driver_supports_rgba();
#endif
   if (!image_texture_load(&image_texture, path))
      return false;
   image_buffer = (uint32_t*)image_texture.pixels;
   image_width  = image_texture.width;
   image_height = image_texture.height;
#endif
   if (!image_buffer)
      return false;

   process_new_image = true;

   return true;
}

bool IMAGE_CORE_PREFIX(retro_load_game)(const struct retro_game_info *info)
{
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
   char *dir                   = strdup(info->path);

   slideshow_enable            = false;

   path_basedir(dir);

   image_file_list = dir_list_new(dir, IMAGE_CORE_PREFIX(valid_extensions),
         false,true,false,false);
   dir_list_sort(image_file_list, false);
   free(dir);

   if (!IMAGE_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      if (IMAGE_CORE_PREFIX(log_cb))
         IMAGE_CORE_PREFIX(log_cb)(RETRO_LOG_INFO, "XRGB8888 is not supported.\n");
      return false;
   }

   if (!imageviewer_load(info->path, 0))
      return false;

   return true;
}

bool IMAGE_CORE_PREFIX(retro_load_game_special)(unsigned a, const struct retro_game_info *b, size_t c)
{
   return false;
}

void IMAGE_CORE_PREFIX(retro_unload_game)(void)
{
   imageviewer_free_image();
   image_width  = 0;
   image_height = 0;
}

unsigned IMAGE_CORE_PREFIX(retro_get_region)(void)
{
   return RETRO_REGION_NTSC;
}

void *IMAGE_CORE_PREFIX(retro_get_memory_data)(unsigned id)
{
   return NULL;
}

size_t IMAGE_CORE_PREFIX(retro_get_memory_size)(unsigned id)
{
   return 0;
}

void IMAGE_CORE_PREFIX(retro_run)(void)
{
   bool first_image       = false;
   bool last_image        = false;
   bool backwards_image   = false;
   bool forward_image     = false;
   bool load_image        = false;
   bool next_image        = false;
   bool prev_image        = false;
   static int frames      = 0;
   static int image_index = 0;
   uint16_t input         = 0;
   static uint16_t previnput;
   uint16_t realinput     = 0;
   int i;

   IMAGE_CORE_PREFIX(input_poll_cb)();

   if (slideshow_enable)
   {
      if ((frames % 120 == 0) && image_index < (signed)(image_file_list->size - 1))
         next_image = true;
   }

   for (i=0;i<16;i++)
   {
      if (IMAGE_CORE_PREFIX(input_state_cb)(0, RETRO_DEVICE_JOYPAD, 0, i))
         realinput |= 1<<i;
   }

   input     = realinput & ~previnput;
   previnput = realinput;

   if (input & (1<<RETRO_DEVICE_ID_JOYPAD_UP))
   {
      if ((image_index + 5) < (signed)(image_file_list->size - 1))
         forward_image   = true;
      else
         last_image      = true;
   }

   if (input & (1<<RETRO_DEVICE_ID_JOYPAD_DOWN))
   {
      if ((image_index - 5) > 0)
         backwards_image = true;
      else
         first_image     = true;
   }

   if (input & (1<<RETRO_DEVICE_ID_JOYPAD_LEFT))
   {
      if (image_index > 0)
         prev_image = true;
   }
   if (input & (1<<RETRO_DEVICE_ID_JOYPAD_RIGHT))
   {
      if (image_index < (signed)(image_file_list->size - 1))
         next_image = true;
   }

   if (input & (1<<RETRO_DEVICE_ID_JOYPAD_Y))
   {
      slideshow_enable = !slideshow_enable;
   }

   if (prev_image)
   {
      image_index--;
      load_image = true;
   }
   else if (next_image)
   {
      image_index++;
      load_image = true;
   }
   else if (backwards_image)
   {
      image_index -= 5;
      load_image = true;
   }
   else if (forward_image)
   {
      image_index += 5;
      load_image = true;
   }
   else if (first_image)
   {
      image_index = 0;
      load_image = true;
   }
   else if (last_image)
   {
      image_index = (int)(image_file_list->size - 1);
      load_image  = true;
   }

   if (load_image)
   {
      if (!imageviewer_load(image_file_list->elems[image_index].data, image_index))
      {
         IMAGE_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
      }
   }

   if (process_new_image)
   {
      /* RGBA > XRGB8888 */
      struct retro_system_av_info info;

#ifdef STB_IMAGE_IMPLEMENTATION
      int x, y;
      uint32_t *buf = &image_buffer[0];

      for (y = 0; y < image_height; y++)
      {
         for (x = 0; x < image_width; x++, buf++)
         {
            uint32_t pixel = *buf;
            uint32_t a = pixel >> 24;

            if (a == 255)
               *buf = (pixel & 0x0000ff00) | ((pixel << 16) & 0x00ff0000) | ((pixel >> 16) & 0x000000ff);
            else
            {
               uint32_t r = pixel & 0x0000ff;
               uint32_t g = (pixel & 0x00ff00) >> 8;
               uint32_t b = (pixel & 0xff0000) >> 16;
               uint32_t bg = ((x & 8) ^ (y & 8)) ? 0x66 : 0x99;

               r = a * r / 255 + (255 - a) * bg / 255;
               g = a * g / 255 + (255 - a) * bg / 255;
               b = a * b / 255 + (255 - a) * bg / 255;

               *buf = r << 16 | g << 8 | b;
            }
         }
      }
#endif

      IMAGE_CORE_PREFIX(retro_get_system_av_info)(&info);

      IMAGE_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_GEOMETRY, &info.geometry);

      process_new_image = false;
   }

#ifdef DUPE_TEST
   if (!image_uploaded)
   {
      IMAGE_CORE_PREFIX(video_cb)(image_buffer, image_width, image_height, image_width * sizeof(uint32_t));
      image_uploaded = true;
   }
   else
      IMAGE_CORE_PREFIX(video_cb)(NULL, image_width, image_height, image_width * sizeof(uint32_t));
#else
   IMAGE_CORE_PREFIX(video_cb)(image_buffer, image_width, image_height, image_width * sizeof(uint32_t));
#endif
   frames++;
}

unsigned IMAGE_CORE_PREFIX(retro_api_version)(void)
{
   return RETRO_API_VERSION;
}
