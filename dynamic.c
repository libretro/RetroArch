/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dynamic.h"
#include "general.h"
#include "compat/strl.h"
#include <string.h>

#ifdef RARCH_CONSOLE
#include "console/console_ext.h"
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "boolean.h"
#include "libretro.h"

#ifdef NEED_DYNAMIC
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#endif

#ifdef HAVE_DYNAMIC
#define SYM(x) do { \
   function_t func = dylib_proc(lib_handle, #x); \
   memcpy(&p##x, &func, sizeof(func)); \
   if (p##x == NULL) { RARCH_ERR("Failed to load symbol: \"%s\"\n", #x); rarch_fail(1, "init_libretro_sym()"); } \
} while (0)

static dylib_t lib_handle = NULL;
#else
#define SYM(x) p##x = x
#endif

void (*pretro_init)(void);
void (*pretro_deinit)(void);

unsigned (*pretro_api_version)(void);

void (*pretro_get_system_info)(struct retro_system_info*);
void (*pretro_get_system_av_info)(struct retro_system_av_info*);

void (*pretro_set_environment)(retro_environment_t);
void (*pretro_set_video_refresh)(retro_video_refresh_t);
void (*pretro_set_audio_sample)(retro_audio_sample_t);
void (*pretro_set_audio_sample_batch)(retro_audio_sample_batch_t);
void (*pretro_set_input_poll)(retro_input_poll_t);
void (*pretro_set_input_state)(retro_input_state_t);

void (*pretro_set_controller_port_device)(unsigned, unsigned);

void (*pretro_reset)(void);
void (*pretro_run)(void);

size_t (*pretro_serialize_size)(void);
bool (*pretro_serialize)(void*, size_t);
bool (*pretro_unserialize)(const void*, size_t);

void (*pretro_cheat_reset)(void);
void (*pretro_cheat_set)(unsigned, bool, const char*);

bool (*pretro_load_game)(const struct retro_game_info*);
bool (*pretro_load_game_special)(unsigned, const struct retro_game_info*, size_t);

void (*pretro_unload_game)(void);

unsigned (*pretro_get_region)(void);

void *(*pretro_get_memory_data)(unsigned);
size_t (*pretro_get_memory_size)(unsigned);

static void set_environment(void);
static void set_environment_defaults(void);

static void load_symbols(void)
{
#ifdef HAVE_DYNAMIC
   RARCH_LOG("Loading dynamic libsnes from: \"%s\"\n", g_settings.libretro);
   lib_handle = dylib_load(g_settings.libretro);
   if (!lib_handle)
   {
      RARCH_ERR("Failed to open dynamic library: \"%s\"\n", g_settings.libretro);
      rarch_fail(1, "load_dynamic()");
   }
#endif

   SYM(retro_init);
   SYM(retro_deinit);

   SYM(retro_api_version);
   SYM(retro_get_system_info);
   SYM(retro_get_system_av_info);

   SYM(retro_set_environment);
   SYM(retro_set_video_refresh);
   SYM(retro_set_audio_sample);
   SYM(retro_set_audio_sample_batch);
   SYM(retro_set_input_poll);
   SYM(retro_set_input_state);

   SYM(retro_set_controller_port_device);

   SYM(retro_reset);
   SYM(retro_run);

   SYM(retro_serialize_size);
   SYM(retro_serialize);
   SYM(retro_unserialize);

   SYM(retro_cheat_reset);
   SYM(retro_cheat_set);

   SYM(retro_load_game);
   SYM(retro_load_game_special);

   SYM(retro_unload_game);
   SYM(retro_get_region);
   SYM(retro_get_memory_data);
   SYM(retro_get_memory_size);
}

void init_libretro_sym(void)
{
   // Guarantee that we can do "dirty" casting.
   // Every OS that this program supports should pass this ...
   rarch_assert(sizeof(void*) == sizeof(void (*)(void)));

#ifdef HAVE_DYNAMIC
   // Try to verify that -lsnes was not linked in from other modules
   // since loading it dynamically and with -l will fail hard.
   function_t sym = dylib_proc(NULL, "retro_init");
   if (sym)
   {
      RARCH_ERR("Serious problem. RetroArch wants to load libsnes dyamically, but it is already linked.\n"); 
      RARCH_ERR("This could happen if other modules RetroArch depends on link against libsnes directly.\n");
      RARCH_ERR("Proceeding could cause a crash. Aborting ...\n");
      rarch_fail(1, "init_libretro_sym()");
   }

   if (!*g_settings.libretro)
   {
#if defined(_WIN32)
      const char *libretro_path = "retro.dll";
#elif defined(__APPLE__)
      const char *libretro_path = "libretro.dylib";
#else
      const char *libretro_path = "libretro.so";
#endif
      strlcpy(g_settings.libretro, libretro_path, sizeof(g_settings.libretro));
   }
#endif

   load_symbols();

   set_environment_defaults();
   set_environment();
}

void uninit_libretro_sym(void)
{
#ifdef HAVE_DYNAMIC
   if (lib_handle)
      dylib_close(lib_handle);
#endif
}

#ifdef NEED_DYNAMIC
// Platform independent dylib loading.
dylib_t dylib_load(const char *path)
{
#ifdef _WIN32
   dylib_t lib = LoadLibrary(path);
   if (!lib)
      RARCH_ERR("Failed to load library, error code: 0x%x\n", (unsigned)GetLastError());
   return lib;
#else
   return dlopen(path, RTLD_LAZY);
#endif
}

function_t dylib_proc(dylib_t lib, const char *proc)
{
#ifdef _WIN32
   function_t sym = (function_t)GetProcAddress(lib ? (HMODULE)lib : GetModuleHandle(NULL), proc);
#else
   void *ptr_sym = NULL;
   if (lib)
      ptr_sym = dlsym(lib, proc); 
   else
   {
      void *handle = dlopen(NULL, RTLD_LAZY);
      if (handle)
      {
         ptr_sym = dlsym(handle, proc);
         dlclose(handle);
      }
   }

   // Dirty hack to workaround the non-legality of (void*) -> fn-pointer casts.
   function_t sym;
   memcpy(&sym, &ptr_sym, sizeof(void*));
#endif

   return sym;
}

void dylib_close(dylib_t lib)
{
#ifdef _WIN32
   FreeLibrary((HMODULE)lib);
#else
   dlclose(lib);
#endif
}
#endif

static bool environment_cb(unsigned cmd, void *data)
{
   switch (cmd)
   {
      case RETRO_ENVIRONMENT_GET_OVERSCAN:
         *(bool*)data = !g_settings.video.crop_overscan;
         RARCH_LOG("Environ GET_OVERSCAN: %u\n", (unsigned)!g_settings.video.crop_overscan);
         break;

      case RETRO_ENVIRONMENT_GET_CAN_DUPE:
#ifdef HAVE_FFMPEG
         *(bool*)data = true;
         RARCH_LOG("Environ GET_CAN_DUPE: true\n");
#else
         *(bool*)data = false;
         RARCH_LOG("Environ GET_CAN_DUPE: false\n");
#endif
         break;

      case RETRO_ENVIRONMENT_GET_VARIABLE:
      {
         struct retro_variable *var = (struct retro_variable*)data;
         if (var->key)
         {
            // Split string has '\0' delimiters so we have to find the position in original string,
            // then pass the corresponding offset into the split string.
            const char *key = strstr(g_extern.system.environment, var->key);
            size_t key_len = strlen(var->key);
            if (key && key[key_len] == '=')
            {
               ptrdiff_t offset = key - g_extern.system.environment;
               var->value = &g_extern.system.environment_split[offset + key_len + 1];
            }
            else
               var->value = NULL;
         }
         else
            var->value = g_extern.system.environment;

         RARCH_LOG("Environ GET_VARIABLE: %s=%s\n",
               var->key ? var->key : "null",
               var->value ? var->value : "null");

         break;
      }

      case RETRO_ENVIRONMENT_SET_VARIABLES:
      {
         RARCH_LOG("Environ SET_VARIABLES:\n");
         RARCH_LOG("=======================\n");
         const struct retro_variable *vars = (const struct retro_variable*)data;
         while (vars->key)
         {
            RARCH_LOG("\t%s :: %s\n",
                  vars->key,
                  vars->value ? vars->value : "N/A");

            vars++;
         }
         RARCH_LOG("=======================\n");
         break;
      }

      case RETRO_ENVIRONMENT_SET_MESSAGE:
      {
         const struct retro_message *msg = (const struct retro_message*)data;
         RARCH_LOG("Environ SET_MESSAGE: %s\n", msg->msg);
         if (g_extern.msg_queue)
            msg_queue_push(g_extern.msg_queue, msg->msg, 1, msg->frames);
         break;
      }

      case RETRO_ENVIRONMENT_SET_ROTATION:
      {
         unsigned rotation = *(const unsigned*)data;
         RARCH_LOG("Environ SET_ROTATION: %u\n", rotation);
         if (!g_settings.video.allow_rotate)
            break;

         g_extern.system.rotation = rotation;

         if (driver.video && driver.video->set_rotation)
         {
            if (driver.video_data)
               video_set_rotation_func(rotation);
         }
         else
            return false;
         break;
      }

      default:
         RARCH_LOG("Environ UNSUPPORTED (#%u).\n", cmd);
         return false;
   }

   return true;
}

static void set_environment(void)
{
   pretro_set_environment(environment_cb);
}

// Assume SNES as defaults.
static void set_environment_defaults(void)
{
   // Split up environment variables beforehand.
   if (g_extern.system.environment_split && strtok(g_extern.system.environment_split, ";"))
      while (strtok(NULL, ";"));
}

