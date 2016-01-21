#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <boolean.h>
#include <file/dir_list.h>
#include <file/file_path.h>
#include <compat/strl.h>

#define STB_IMAGE_IMPLEMENTATION

#ifdef RARCH_INTERNAL
#define STBI_NO_PSD
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_SUPPORT_ZLIB
#endif

#include "../../deps/stb/stb_image.h"

#ifdef RARCH_INTERNAL
#include "internal_cores.h"
#include "../../libretro.h"
#define IMAGE_CORE_PREFIX(s) libretro_imageviewer_##s
#else
#include "libretro.h"
#define IMAGE_CORE_PREFIX(s) s
#endif

static retro_log_printf_t IMAGE_CORE_PREFIX(log_cb);
static retro_video_refresh_t IMAGE_CORE_PREFIX(video_cb);
static retro_input_poll_t IMAGE_CORE_PREFIX(input_poll_cb);
static retro_input_state_t IMAGE_CORE_PREFIX(input_state_cb);
static retro_audio_sample_batch_t IMAGE_CORE_PREFIX(audio_batch_cb);
static retro_environment_t IMAGE_CORE_PREFIX(environ_cb);

static uint32_t* image_buffer;
static int       image_width;
static int       image_height;
static bool      image_uploaded;
static bool      slideshow_enable;
struct string_list *file_list;

#if 0
#define DUPE_TEST
#endif

#ifdef RARCH_INTERNAL
static const char* IMAGE_CORE_PREFIX(valid_extensions) = "jpg|jpeg|png|bmp|tga";
#else
static const char* IMAGE_CORE_PREFIX(valid_extensions) = "jpg|jpeg|png|bmp|psd|tga|gif|hdr|pic|ppm|pgm";
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

void IMAGE_CORE_PREFIX(retro_init)(void)
{
   struct retro_log_callback log;

   if (IMAGE_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      IMAGE_CORE_PREFIX(log_cb) = log.log;
   else
      IMAGE_CORE_PREFIX(log_cb) = NULL;

   image_buffer = NULL;
   image_width  = 0;
   image_height = 0;

}

void IMAGE_CORE_PREFIX(retro_deinit)(void)
{
   if (image_buffer)
      free(image_buffer);
   image_buffer = NULL;
   image_width  = 0;
   image_height = 0;
}

void IMAGE_CORE_PREFIX(retro_set_environment)(retro_environment_t cb)
{
   static const struct retro_variable vars[] = {
      { NULL, NULL },
   };

   IMAGE_CORE_PREFIX(environ_cb) = cb;

   cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)vars);
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

bool IMAGE_CORE_PREFIX(retro_load_game)(const struct retro_game_info *info)
{
   int comp;
   uint32_t *buf               = NULL;
   uint32_t *end               = NULL;
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
   char *dir                   = strdup(info->path);

   slideshow_enable            = false;

   path_basedir(dir);

   file_list = dir_list_new(dir, IMAGE_CORE_PREFIX(valid_extensions),
         false,false);
   dir_list_sort(file_list, false);
   free(dir);

   image_buffer = (uint32_t*)stbi_load (info->path,&image_width, &image_height, &comp, 4);

   /* RGBA > XRGB8888 */
   buf          = &image_buffer[0];
   end          = buf + (image_width*image_height*sizeof(uint32_t))/4;

   while(buf < end)
   {
      uint32_t pixel = *buf;
      *buf           = (pixel & 0xff00ff00) | ((pixel << 16) & 0x00ff0000) | ((pixel >> 16) & 0xff);
      buf++;
   }
  
   if (!IMAGE_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      if (IMAGE_CORE_PREFIX(log_cb))
         IMAGE_CORE_PREFIX(log_cb)(RETRO_LOG_INFO, "XRGB8888 is not supported.\n");
      return false;
   }
   return true;
}

bool IMAGE_CORE_PREFIX(retro_load_game_special)(unsigned a, const struct retro_game_info *b, size_t c)
{
   return false;
}

void IMAGE_CORE_PREFIX(retro_unload_game)(void)
{
   if (image_buffer)
      free(image_buffer);
   image_buffer = NULL;
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

static bool imageviewer_load(uint32_t *buf, int image_index)
{
   int comp;
   uint32_t *end          = NULL;
   image_buffer           = (uint32_t*)stbi_load(
         file_list->elems[image_index].data,
         &image_width, &image_height, &comp, 4);

   /* RGBA > XRGB8888 */
   buf = &image_buffer[0];
   end = buf + (image_width*image_height*sizeof(uint32_t))/4;

   while(buf < end)
   {
      uint32_t pixel = *buf;
      *buf = (pixel & 0xff00ff00) | ((pixel << 16) & 0x00ff0000) | ((pixel >> 16) & 0xff);
      buf++;
   }

   return true;
}

void IMAGE_CORE_PREFIX(retro_run)(void)
{
   uint32_t *buf          = NULL;
   bool load_image        = false;
   bool next_image        = false;
   bool prev_image        = false;
   static int frames      = 0;
   static int image_index = 0;

   IMAGE_CORE_PREFIX(input_poll_cb)();

   if (slideshow_enable)
   {
      if ((frames % 120 == 0) && image_index < file_list->size)
         next_image = true;
   }

   if (IMAGE_CORE_PREFIX(input_state_cb)(0, RETRO_DEVICE_JOYPAD, 0,
            RETRO_DEVICE_ID_JOYPAD_LEFT))
   {
      if (image_index > 0)
         prev_image = true;
   }
   if (IMAGE_CORE_PREFIX(input_state_cb)(0, RETRO_DEVICE_JOYPAD, 0,
            RETRO_DEVICE_ID_JOYPAD_RIGHT))
   {
      if (image_index < file_list->size)
         next_image = true;
   }

   if (IMAGE_CORE_PREFIX(input_state_cb)(0, RETRO_DEVICE_JOYPAD, 0,
            RETRO_DEVICE_ID_JOYPAD_Y))
   {
      slideshow_enable = !slideshow_enable;
   }

   if (prev_image)
   {
      image_index--;
      load_image = true;
   }

   if (next_image)
   {
      image_index++;
      load_image = true;
   }

   if (load_image)
      imageviewer_load(buf, image_index);

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
