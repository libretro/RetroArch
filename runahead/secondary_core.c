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
#include "../configuration.h"
#include "../dynamic.h"
#include "../paths.h"
#include "../content.h"

#include "secondary_core.h"
#include "dirty_input.h"

static int port_map[16];

static char *secondary_library_path;
static dylib_t secondary_module;
static struct retro_core_t secondary_core;
static struct retro_callbacks secondary_callbacks;

extern retro_ctx_load_content_info_t *load_content_info;
extern enum rarch_core_type last_core_type;
extern struct retro_callbacks retro_ctx;

bool secondary_core_run_no_input_polling(void);

bool secondary_core_deserialize(const void *buffer, int size);

void secondary_core_destroy(void);

void set_last_core_type(enum rarch_core_type type);

void remember_controller_port_device(long port, long device);

void clear_controller_port_map(void);

static char *get_temp_directory_alloc(void)
{
   char *path             = NULL;
#ifdef _WIN32
#ifdef LEGACY_WIN32
   DWORD pathLength       = GetTempPath(0, NULL) + 1;
   path                   = (char*)malloc(pathLength * sizeof(char));

   if (!path)
      return NULL;

   path[pathLength - 1]   = 0;
   GetTempPath(pathLength, path);
#else
   DWORD pathLength = GetTempPathW(0, NULL) + 1;
   wchar_t *wideStr = (wchar_t*)malloc(pathLength * sizeof(wchar_t));

   if (!wideStr)
      return NULL;

   wideStr[pathLength - 1] = 0;
   GetTempPathW(pathLength, wideStr);

   path = utf16_to_utf8_string_alloc(wideStr);
   free(wideStr);
#endif
#elif defined ANDROID
   {
      settings_t *settings = config_get_ptr();
      path = strcpy_alloc_force(settings->paths.directory_libretro);
   }
#else
   if (getenv("TMPDIR"))
      path = strcpy_alloc_force(getenv("TMPDIR"));
   else
      path = strcpy_alloc_force("/tmp");

#endif
   return path;
}

static bool write_file_with_random_name(char **tempDllPath,
      const char *retroarchTempPath, const void* data, ssize_t dataSize)
{
   unsigned i;
   char number_buf[32];
   bool okay                = false;
   const char *prefix       = "tmp";
   time_t time_value        = time(NULL);
   unsigned number_value    = (unsigned)time_value;
   char *ext                = strcpy_alloc_force(
         path_get_extension(*tempDllPath));
   int ext_len              = (int)strlen(ext);

   if (ext_len > 0)
   {
      strcat_alloc(&ext, ".");
      memmove(ext + 1, ext, ext_len);
      ext[0] = '.';
      ext_len++;
   }

   /* Try up to 30 'random' filenames before giving up */
   for (i = 0; i < 30; i++)
   {
      int number;
      number_value = number_value * 214013 + 2531011;
      number       = (number_value >> 14) % 100000;

      snprintf(number_buf, sizeof(number_buf), "%05d", number);

      if (*tempDllPath)
         free(*tempDllPath);
      *tempDllPath = NULL;
      strcat_alloc(tempDllPath, retroarchTempPath);
      strcat_alloc(tempDllPath, prefix);
      strcat_alloc(tempDllPath, number_buf);
      strcat_alloc(tempDllPath, ext);
      if (filestream_write_file(*tempDllPath, data, dataSize))
      {
         okay = true;
         break;
      }
   }

   if (ext)
      free(ext);
   ext = NULL;
   return okay;
}

static char *copy_core_to_temp_file(void)
{
   bool failed              = false;
   char *tempDirectory      = NULL;
   char *retroarchTempPath  = NULL;
   char *tempDllPath        = NULL;
   void *dllFileData        = NULL;
   int64_t dllFileSize      = 0;
   const char *corePath     = path_get(RARCH_PATH_CORE);
   const char *coreBaseName = path_basename(corePath);

   if (strlen(coreBaseName) == 0)
   {
      failed = true;
      goto end;
   }

   tempDirectory = get_temp_directory_alloc();
   if (!tempDirectory)
   {
      failed = true;
      goto end;
   }

   strcat_alloc(&retroarchTempPath, tempDirectory);
   strcat_alloc(&retroarchTempPath, path_default_slash());
   strcat_alloc(&retroarchTempPath, "retroarch_temp");
   strcat_alloc(&retroarchTempPath, path_default_slash());

   if (!path_mkdir(retroarchTempPath))
   {
      failed = true;
      goto end;
   }

   if (!filestream_read_file(corePath, &dllFileData, &dllFileSize))
   {
      failed = true;
      goto end;
   }

   strcat_alloc(&tempDllPath, retroarchTempPath);
   strcat_alloc(&tempDllPath, coreBaseName);

   if (!filestream_write_file(tempDllPath, dllFileData, dllFileSize))
   {
      /* try other file names */
      if (!write_file_with_random_name(&tempDllPath,
               retroarchTempPath, dllFileData, dllFileSize))
         failed = true;
   }

end:
   if (tempDirectory)
      free(tempDirectory);
   if (retroarchTempPath)
      free(retroarchTempPath);
   if (dllFileData)
      free(dllFileData);

   tempDirectory     = NULL;
   retroarchTempPath = NULL;
   dllFileData       = NULL;

   if (!failed)
      return tempDllPath;

   if (tempDllPath)
      free(tempDllPath);

   tempDllPath       = NULL;

   return NULL;
}

static bool has_variable_update = false;

static bool rarch_environment_secondary_core_hook(unsigned cmd, void *data)
{
   bool result = rarch_environment_cb(cmd, data);
   if (has_variable_update)
   {
      if (cmd == RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE)
      {
         bool *bool_p        = (bool*)data;
         *bool_p             = true;
         has_variable_update = false;
         return true;
      }
      else if (cmd == RETRO_ENVIRONMENT_GET_VARIABLE)
         has_variable_update = false;
   }
   return result;
}

static bool secondary_core_create(void)
{
   long port, device;
   bool contentless       = false;
   bool is_inited         = false;

   if (  last_core_type != CORE_TYPE_PLAIN ||
         !load_content_info                ||
         load_content_info->special)
      return false;

   if (secondary_library_path)
      free(secondary_library_path);
   secondary_library_path = NULL;
   secondary_library_path = copy_core_to_temp_file();

   if (!secondary_library_path)
      return false;

   /* Load Core */
   if (init_libretro_sym_custom(
            CORE_TYPE_PLAIN, &secondary_core,
            secondary_library_path, &secondary_module))
   {
      secondary_core.symbols_inited = true;
      secondary_core.retro_set_environment(
            rarch_environment_secondary_core_hook);
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
         secondary_core.game_loaded = false;

      if (!secondary_core.inited)
      {
         secondary_core_destroy();
         return false;
      }

      core_set_default_callbacks(&secondary_callbacks);
      secondary_core.retro_set_video_refresh(secondary_callbacks.frame_cb);
      secondary_core.retro_set_audio_sample(secondary_callbacks.sample_cb);
      secondary_core.retro_set_audio_sample_batch(secondary_callbacks.sample_batch_cb);
      secondary_core.retro_set_input_state(secondary_callbacks.state_cb);
      secondary_core.retro_set_input_poll(secondary_callbacks.poll_cb);

      for (port = 0; port < 16; port++)
      {
         device = port_map[port];
         if (device >= 0)
            secondary_core.retro_set_controller_port_device(
                  (unsigned)port, (unsigned)device);
      }
      clear_controller_port_map();
   }
   else
      return false;

   return true;
}

void secondary_core_set_variable_update(void)
{
   has_variable_update = true;
}

static void secondary_core_input_poll_null(void) { }

bool secondary_core_run_use_last_input(void)
{
   if (!secondary_core_ensure_exists())
      return false;

   retro_input_poll_t old_poll_function   = secondary_callbacks.poll_cb;
   retro_input_state_t old_input_function = secondary_callbacks.state_cb;

   secondary_callbacks.poll_cb            = secondary_core_input_poll_null;
   secondary_callbacks.state_cb           = input_state_get_last;

   secondary_core.retro_set_input_poll(secondary_callbacks.poll_cb);
   secondary_core.retro_set_input_state(secondary_callbacks.state_cb);

   secondary_core.retro_run();

   secondary_callbacks.poll_cb            = old_poll_function;
   secondary_callbacks.state_cb           = old_input_function;

   secondary_core.retro_set_input_poll(secondary_callbacks.poll_cb);
   secondary_core.retro_set_input_state(secondary_callbacks.state_cb);

   return true;
}

bool secondary_core_deserialize(const void *buffer, int size)
{
   if (secondary_core_ensure_exists())
      return secondary_core.retro_unserialize(buffer, size);
   return false;
}

bool secondary_core_ensure_exists(void)
{
   if (!secondary_module)
   {
      if (!secondary_core_create())
      {
         secondary_core_destroy();
         return false;
      }
   }
   return true;
}

void secondary_core_destroy(void)
{
   if (!secondary_module)
      return;

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
   if (secondary_library_path)
      free(secondary_library_path);
   secondary_library_path = NULL;
}

void remember_controller_port_device(long port, long device)
{
   if (port >= 0 && port < 16)
      port_map[port] = (int)device;
   if (secondary_module && secondary_core.retro_set_controller_port_device)
      secondary_core.retro_set_controller_port_device((unsigned)port, (unsigned)device);
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

void secondary_core_destroy(void) { }
void remember_controller_port_device(long port, long device) { }
void secondary_core_set_variable_update(void) { }
void clear_controller_port_map(void) { }

#endif
