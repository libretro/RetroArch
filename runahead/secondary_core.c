#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)

#include <string.h>
#include <time.h>

#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0500 || defined(_XBOX)
#ifndef LEGACY_WIN32
#define LEGACY_WIN32
#endif
#endif

#include <boolean.h>
#include <encodings/utf.h>
#include <dynamic/dylib.h>
#include <file/file_path.h>
#include <streams/file_stream.h>

#include "mem_util.h"

#include "../core.h"
#include "../dynamic.h"
#include "../paths.h"
#include "../content.h"

#include "secondary_core.h"

static int port_map[16];

static char *secondary_library_path;
static dylib_t secondary_module;
static struct retro_core_t secondary_core;
static struct retro_callbacks secondary_callbacks;

extern retro_ctx_load_content_info_t *load_content_info;
extern enum rarch_core_type last_core_type;
extern struct retro_callbacks retro_ctx;

static char* get_temp_directory_alloc(void);

static char* copy_core_to_temp_file(void);

static bool write_file_with_random_name(char **tempDllPath,
      const char *retroarchTempPath, const void* data, ssize_t dataSize);

static bool secondary_core_create(void);

bool secondary_core_run_no_input_polling(void);

bool secondary_core_deserialize(const void *buffer, int size);

static bool rarch_environment_secondary_core_hook(unsigned cmd, void *data);

void secondary_core_destroy(void);

void set_last_core_type(enum rarch_core_type type);

void remember_controller_port_device(long port, long device);

void clear_controller_port_map(void);

char* get_temp_directory_alloc(void)
{
#ifdef _WIN32
#ifdef LEGACY_WIN32
   DWORD pathLength = GetTempPath(0, NULL) + 1;
   char *path       = (char*)malloc(pathLength * sizeof(char));

   path[pathLength - 1] = 0;
   GetTempPath(pathLength, path);
   return path;
#else
   char *path;
   DWORD pathLength = GetTempPathW(0, NULL) + 1;
   wchar_t *wideStr = (wchar_t*)malloc(pathLength * sizeof(wchar_t));
   wideStr[pathLength - 1] = 0;
   GetTempPathW(pathLength, wideStr);

   path = utf16_to_utf8_string_alloc(wideStr);
   free(wideStr);
   return path;
#endif
#else
   char *path = strcpy_alloc_force(getenv("TMPDIR"));
   return path;
#endif
}

char* copy_core_to_temp_file(void)
{
   bool okay                = false;
   char *tempDirectory      = NULL;
   char *retroarchTempPath  = NULL;
   char *tempDllPath        = NULL;
   void *dllFileData        = NULL;
   ssize_t dllFileSize      = 0;
   const char *corePath     = path_get(RARCH_PATH_CORE);
   const char *coreBaseName = path_basename(corePath);

   if (strlen(coreBaseName) == 0)
      goto failed;

   tempDirectory = get_temp_directory_alloc();
   if (!tempDirectory)
      goto failed;

   strcat_alloc(&retroarchTempPath, tempDirectory);
   strcat_alloc(&retroarchTempPath, path_default_slash());
   strcat_alloc(&retroarchTempPath, "retroarch_temp");
   strcat_alloc(&retroarchTempPath, path_default_slash());

   okay = path_mkdir(retroarchTempPath);

   if (!okay)
      goto failed;

   if (!filestream_read_file(corePath, &dllFileData, &dllFileSize))
      goto failed;

   strcat_alloc(&tempDllPath, retroarchTempPath);
   strcat_alloc(&tempDllPath, coreBaseName);
   okay = filestream_write_file(tempDllPath, dllFileData, dllFileSize);

   if (!okay)
   {
      /* try other file names */
      okay = write_file_with_random_name(&tempDllPath, retroarchTempPath, dllFileData, dllFileSize);
      if (!okay)
         goto failed;
   }

   FREE(tempDirectory);
   FREE(retroarchTempPath);
   FREE(dllFileData);
   return tempDllPath;

failed:
   FREE(tempDirectory);
   FREE(retroarchTempPath);
   FREE(tempDllPath);
   FREE(dllFileData);
   return NULL;
}

bool write_file_with_random_name(char **tempDllPath,
      const char *retroarchTempPath, const void* data, ssize_t dataSize)
{
   unsigned i;
   char numberBuf[32];
   bool okay                = false;
   const char *prefix       = "tmp";
   time_t timeValue         = time(NULL);
   unsigned int numberValue = (unsigned int)timeValue;
   int number               = 0;
   char *ext                = strcpy_alloc_force(path_get_extension(*tempDllPath));
   int extLen               = strlen(ext);

   if (extLen > 0)
   {
      strcat_alloc(&ext, ".");
      memmove(ext + 1, ext, extLen);
      ext[0] = '.';
      extLen++;
   }

   /* try up to 30 'random' filenames before giving up */
   for (i = 0; i < 30; i++)
   {
      numberValue = numberValue * 214013 + 2531011;
      number = (numberValue >> 14) % 100000;
      sprintf(numberBuf, "%05d", number);
      FREE(*tempDllPath);
      strcat_alloc(tempDllPath, retroarchTempPath);
      strcat_alloc(tempDllPath, prefix);
      strcat_alloc(tempDllPath, numberBuf);
      strcat_alloc(tempDllPath, ext);
      okay = filestream_write_file(*tempDllPath, data, dataSize);
      if (okay)
         break;
   }

   FREE(ext);
   return true;
}

bool secondary_core_create(void)
{
   long port, device;
   bool contentless, is_inited;

   if (  last_core_type != CORE_TYPE_PLAIN || 
         !load_content_info                ||
         load_content_info->special)
      return false;

   FREE(secondary_library_path);
   secondary_library_path = copy_core_to_temp_file();

   if (!secondary_library_path)
      return false;

   /* Load Core */
   if (init_libretro_sym_custom(CORE_TYPE_PLAIN, &secondary_core, secondary_library_path, &secondary_module))
   {
      secondary_core.symbols_inited = true;

      core_set_default_callbacks(&secondary_callbacks);
      secondary_core.retro_set_video_refresh(secondary_callbacks.frame_cb);
      secondary_core.retro_set_audio_sample(secondary_callbacks.sample_cb);
      secondary_core.retro_set_audio_sample_batch(secondary_callbacks.sample_batch_cb);
      secondary_core.retro_set_input_state(secondary_callbacks.state_cb);
      secondary_core.retro_set_input_poll(secondary_callbacks.poll_cb);
      secondary_core.retro_set_environment(rarch_environment_secondary_core_hook);
      secondary_core_set_variable_update();

      secondary_core.retro_init();

      content_get_status(&contentless, &is_inited);
      secondary_core.inited = is_inited;

      /* Load Content */
      if (!load_content_info || load_content_info->special)
      {
         /* disabled due to crashes */
         return false;
#if 0
         secondary_core.game_loaded = secondary_core.retro_load_game_special(
               loadContentInfo.special->id, loadContentInfo.info, loadContentInfo.content->size);
         if (!secondary_core.game_loaded)
         {
            secondary_core_destroy();
            return false;
         }
#endif
      }
      else if (load_content_info->content->size > 0 && load_content_info->content->elems[0].data)
      {
         secondary_core.game_loaded = secondary_core.retro_load_game(load_content_info->info);
         if (!secondary_core.game_loaded)
         {
            secondary_core_destroy();
            return false;
         }
      }
      else if (contentless)
      {
         secondary_core.game_loaded = secondary_core.retro_load_game(NULL);
         if (!secondary_core.game_loaded)
         {
            secondary_core_destroy();
            return false;
         }
      }
      else
      {
         secondary_core.game_loaded = false;
      }
      if (!secondary_core.inited)
      {
         secondary_core_destroy();
         return false;
      }

      for (port = 0; port < 16; port++)
      {
         device = port_map[port];
         if (device >= 0)
            secondary_core.retro_set_controller_port_device(port, device);
      }
      clear_controller_port_map();
   }
   else
      return false;

   return true;
}

static bool has_variable_update;

static bool rarch_environment_secondary_core_hook(unsigned cmd, void *data)
{
   bool result = rarch_environment_cb(cmd, data);
   if (cmd == RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE && has_variable_update)
   {
      has_variable_update = false;
      bool *bool_p = (bool*)data;
      *bool_p = true;
      return true;
   }
   return result;
}

void secondary_core_set_variable_update(void)
{
   has_variable_update = true;
}

bool secondary_core_run_no_input_polling(void)
{
   if (!secondary_module)
   {
      bool okay = secondary_core_create();
      if (!okay)
         return false;
   }
   secondary_core.retro_run();
   return true;
}

bool secondary_core_deserialize(const void *buffer, int size)
{
   if (!secondary_module)
   {
      bool okay = secondary_core_create();
      if (!okay)
      {
         secondary_core_destroy();
         return false;
      }
   }
   return secondary_core.retro_unserialize(buffer, size);
}

void secondary_core_destroy(void)
{
   if (secondary_module)
   {
      /* unload game from core */
      if (secondary_core.retro_unload_game)
         secondary_core.retro_unload_game();
      /* deinit */
      if (secondary_core.retro_deinit)
         secondary_core.retro_deinit();
      memset(&secondary_core, 0, sizeof(struct retro_core_t));

      dylib_close(secondary_module);
      secondary_module = NULL;
      filestream_delete(secondary_library_path);
      FREE(secondary_library_path);
   }
}

void remember_controller_port_device(long port, long device)
{
   if (port >= 0 && port < 16)
      port_map[port] = device;
   if (secondary_module && secondary_core.retro_set_controller_port_device)
      secondary_core.retro_set_controller_port_device(port, device);
}

void clear_controller_port_map(void)
{
   unsigned port;
   for (port = 0; port < 16; port++)
      port_map[port] = -1;
}

#else
#include <boolean.h>

#include "../core.h"

bool secondary_core_run_no_input_polling(void)
{
   return false;
}
bool secondary_core_deserialize(const void *buffer, int size)
{
   return false;
}
void secondary_core_destroy(void)
{
   /* do nothing */
}
void remember_controller_port_device(long port, long device)
{
   /* do nothing */
}
void secondary_core_set_variable_update(void)
{
   /* do nothing */
}

#endif

