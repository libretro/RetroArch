/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
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

#ifdef SSNES_CONSOLE
#include "console/console_ext.h"
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "boolean.h"
#include "libsnes.hpp"

#ifdef NEED_DYNAMIC
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#endif

#ifdef NEED_DYNAMIC
#define DLSYM(lib, x) dylib_proc(lib, #x)

#define SYM(type, x) do { \
   p##x = (type)DLSYM(lib_handle, x); \
   if (p##x == NULL) { SSNES_ERR("Failed to load symbol: \"%s\"\n", #x); ssnes_fail(1, "init_libsnes_sym()"); } \
} while (0)

#define OPT_SYM(type, x) do { \
   p##x = (type)DLSYM(lib_handle, x); \
} while (0)

#endif

#ifdef HAVE_DYNAMIC
static dylib_t lib_handle = NULL;
#endif

void (*psnes_init)(void);

void (*psnes_set_video_refresh)(snes_video_refresh_t);
void (*psnes_set_audio_sample)(snes_audio_sample_t);
void (*psnes_set_input_poll)(snes_input_poll_t);
void (*psnes_set_input_state)(snes_input_state_t);

void (*psnes_reset)(void);
void (*psnes_run)(void);

void (*psnes_cheat_reset)(void);
void (*psnes_cheat_set)(unsigned, bool, const char*);

const char *(*psnes_library_id)(void) = NULL;
unsigned (*psnes_library_revision_minor)(void);
unsigned (*psnes_library_revision_major)(void);

bool (*psnes_load_cartridge_normal)(const char*, const uint8_t*, unsigned);
bool (*psnes_load_cartridge_super_game_boy)(
         const char*, const uint8_t*, unsigned, 
         const char*, const uint8_t*, unsigned);
bool (*psnes_load_cartridge_bsx)(
         const char*, const uint8_t*, unsigned, 
         const char*, const uint8_t*, unsigned);
bool (*psnes_load_cartridge_bsx_slotted)(
         const char*, const uint8_t*, unsigned, 
         const char*, const uint8_t*, unsigned);
bool (*psnes_load_cartridge_sufami_turbo)(
         const char*, const uint8_t*, unsigned, 
         const char*, const uint8_t*, unsigned, 
         const char*, const uint8_t*, unsigned);

void (*psnes_set_controller_port_device)(bool, unsigned);

bool (*psnes_get_region)(void);

unsigned (*psnes_serialize_size)(void);
bool (*psnes_serialize)(uint8_t*, unsigned);
bool (*psnes_unserialize)(const uint8_t*, unsigned);

void (*psnes_set_cartridge_basename)(const char*);

uint8_t* (*psnes_get_memory_data)(unsigned);
unsigned (*psnes_get_memory_size)(unsigned);

void (*psnes_unload_cartridge)(void);
void (*psnes_term)(void);

#if defined(NEED_DYNAMIC) || defined(SSNES_CONSOLE)
static void set_environment(void);
#endif
static void set_environment_defaults(void);

#ifdef HAVE_DYNAMIC
static void load_dynamic(void)
{
   SSNES_LOG("Loading dynamic libsnes from: \"%s\"\n", g_settings.libsnes);
   lib_handle = dylib_load(g_settings.libsnes);
   if (!lib_handle)
   {
      SSNES_ERR("Failed to open dynamic library: \"%s\"\n", g_settings.libsnes);
      ssnes_fail(1, "load_dynamic()");
   }

   SYM(void (*)(void), snes_init);
   SYM(void (*)(snes_video_refresh_t), snes_set_video_refresh);
   SYM(void (*)(snes_audio_sample_t), snes_set_audio_sample);
   SYM(void (*)(snes_input_poll_t), snes_set_input_poll);
   SYM(void (*)(snes_input_state_t), snes_set_input_state);
   SYM(const char *(*)(void), snes_library_id);
   SYM(unsigned (*)(void), snes_library_revision_minor);
   SYM(unsigned (*)(void), snes_library_revision_major);
   SYM(void (*)(void), snes_cheat_reset);
   SYM(void (*)(unsigned, bool, const char*), snes_cheat_set);
   SYM(void (*)(void), snes_reset);
   SYM(void (*)(void), snes_run);
   SYM(bool (*)(void), snes_get_region);
   SYM(bool (*)(const char*, const uint8_t*, unsigned), snes_load_cartridge_normal);
   SYM(bool (*)(const char*, const uint8_t*, unsigned, 
            const char*, const uint8_t*, unsigned), snes_load_cartridge_super_game_boy);
   SYM(bool (*)(const char*, const uint8_t*, unsigned, 
            const char*, const uint8_t*, unsigned), snes_load_cartridge_bsx);
   SYM(bool (*)(const char*, const uint8_t*, unsigned, 
            const char*, const uint8_t*, unsigned), snes_load_cartridge_bsx_slotted);
   SYM(bool (*)(const char*, const uint8_t*, unsigned, 
            const char*, const uint8_t*, unsigned, 
            const char*, const uint8_t*, unsigned), snes_load_cartridge_sufami_turbo);
   SYM(void (*)(bool, unsigned), snes_set_controller_port_device);
   SYM(unsigned (*)(void), snes_serialize_size);
   SYM(bool (*)(uint8_t*, unsigned), snes_serialize);
   SYM(bool (*)(const uint8_t*, unsigned), snes_unserialize);
   SYM(void (*)(const char*), snes_set_cartridge_basename);
   SYM(uint8_t *(*)(unsigned), snes_get_memory_data);
   SYM(unsigned (*)(unsigned), snes_get_memory_size);
   SYM(void (*)(void), snes_unload_cartridge);
   SYM(void (*)(void), snes_term);
}
#else
#define SSYM(x) do { \
   p##x = x; \
} while (0)

static void set_statics(void)
{
   SSYM(snes_init);
   SSYM(snes_set_video_refresh);
   SSYM(snes_set_audio_sample);
   SSYM(snes_set_input_poll);
   SSYM(snes_set_input_state);
   SSYM(snes_library_revision_minor);
   SSYM(snes_library_revision_major);
   SSYM(snes_library_id);
   SSYM(snes_cheat_reset);
   SSYM(snes_cheat_set);
   SSYM(snes_reset);
   SSYM(snes_run);
   SSYM(snes_get_region);
   SSYM(snes_load_cartridge_normal);
   SSYM(snes_load_cartridge_super_game_boy);
   SSYM(snes_load_cartridge_bsx);
   SSYM(snes_load_cartridge_bsx_slotted);
   SSYM(snes_load_cartridge_sufami_turbo);
   SSYM(snes_set_controller_port_device);
   SSYM(snes_serialize_size);
   SSYM(snes_serialize);
   SSYM(snes_unserialize);
   SSYM(snes_set_cartridge_basename);
   SSYM(snes_get_memory_data);
   SSYM(snes_get_memory_size);
   SSYM(snes_unload_cartridge);
   SSYM(snes_term);
}
#endif

void init_libsnes_sym(void)
{
   // Guarantee that we can do "dirty" casting.
   // Every OS that this program supports should pass this ...
   ssnes_assert(sizeof(void*) == sizeof(void (*)(void)));

#ifdef HAVE_DYNAMIC
   // Try to verify that -lsnes was not linked in from other modules
   // since loading it dynamically and with -l will fail hard.
   function_t sym = dylib_proc(NULL, "snes_init");
   if (sym)
   {
      SSNES_ERR("Serious problem. SSNES wants to load libsnes dyamically, but it is already linked.\n"); 
      SSNES_ERR("This could happen if other modules SSNES depends on link against libsnes directly.\n");
      SSNES_ERR("Proceeding could cause a crash. Aborting ...\n");
      ssnes_fail(1, "init_libsnes_sym()");
   }

   if (!*g_settings.libsnes)
   {
#if defined(_WIN32)
      strlcpy(g_settings.libsnes, "snes.dll", sizeof(g_settings.libsnes));
#elif defined(__APPLE__)
      strlcpy(g_settings.libsnes, "libsnes.dylib", sizeof(g_settings.libsnes));
#else
      strlcpy(g_settings.libsnes, "libsnes.so", sizeof(g_settings.libsnes));
#endif
   }

   load_dynamic();
#else
   set_statics();
#endif

   set_environment_defaults();
#if defined(NEED_DYNAMIC) || defined(SSNES_CONSOLE)
   set_environment();
#endif
}

void uninit_libsnes_sym(void)
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
      SSNES_ERR("Failed to load library, error code: 0x%x\n", (unsigned)GetLastError());
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

#if defined(NEED_DYNAMIC) || defined(SSNES_CONSOLE)
static bool environment_cb(unsigned cmd, void *data)
{
   switch (cmd)
   {
      case SNES_ENVIRONMENT_GET_FULLPATH:
         *(const char**)data = g_extern.system.fullpath;
         SSNES_LOG("Environ FULLPATH: \"%s\"\n", g_extern.system.fullpath);
         break;

      case SNES_ENVIRONMENT_SET_GEOMETRY:
         g_extern.system.geom = *(const struct snes_geometry*)data;
         g_extern.system.geom.max_width = next_pow2(g_extern.system.geom.max_width);
         g_extern.system.geom.max_height = next_pow2(g_extern.system.geom.max_height);
         SSNES_LOG("Environ SET_GEOMETRY: (%ux%u) / (%ux%u)\n",
               g_extern.system.geom.base_width,
               g_extern.system.geom.base_height,
               g_extern.system.geom.max_width,
               g_extern.system.geom.max_height);
         break;

      case SNES_ENVIRONMENT_SET_PITCH:
         g_extern.system.pitch = *(const unsigned*)data;
         SSNES_LOG("Environ SET_PITCH: %u\n", g_extern.system.pitch);
         break;

      case SNES_ENVIRONMENT_GET_OVERSCAN:
         *(bool*)data = !g_settings.video.crop_overscan;
         SSNES_LOG("Environ GET_OVERSCAN: %u\n", (unsigned)!g_settings.video.crop_overscan);
         break;

      case SNES_ENVIRONMENT_SET_TIMING:
         g_extern.system.timing = *(const struct snes_system_timing*)data;
         g_extern.system.timing_set = true;
         SSNES_LOG("Environ SET_TIMING: %.3f Hz/ %.3f Hz\n",
               (float)g_extern.system.timing.fps, (float)g_extern.system.timing.sample_rate);
         break;

      case SNES_ENVIRONMENT_GET_CAN_DUPE:
#ifdef HAVE_FFMPEG
         *(bool*)data = true;
         SSNES_LOG("Environ GET_CAN_DUPE: true\n");
#else
         *(bool*)data = false;
         SSNES_LOG("Environ GET_CAN_DUPE: false\n");
#endif
         break;

      case SNES_ENVIRONMENT_SET_NEED_FULLPATH:
         g_extern.system.need_fullpath = *(const bool*)data;
         SSNES_LOG("Environ SET_NEED_FULLPATH: %s\n", g_extern.system.need_fullpath ? "true" : "false");
         break;

      case SNES_ENVIRONMENT_GET_CAN_REWIND:
         *(bool*)data = g_settings.rewind_enable;
         SSNES_LOG("Environ GET_CAN_REWIND: %s\n", g_settings.rewind_enable ? "true" : "false");
         break;

      case SNES_ENVIRONMENT_GET_VARIABLE:
      {
         struct snes_variable *var = (struct snes_variable*)data;
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

         SSNES_LOG("Environ GET_VARIABLE: %s=%s\n",
               var->key ? var->key : "null",
               var->value ? var->value : "null");

         break;
      }

      case SNES_ENVIRONMENT_SET_VARIABLES:
      {
         SSNES_LOG("Environ SET_VARIABLES:\n");
         SSNES_LOG("=======================\n");
         const struct snes_variable *vars = (const struct snes_variable*)data;
         while (vars->key)
         {
            SSNES_LOG("\t%s :: %s\n",
                  vars->key,
                  vars->value ? vars->value : "N/A");

            vars++;
         }
         SSNES_LOG("=======================\n");
         break;
      }

#ifdef SSNES_CONSOLE
      case SNES_ENVIRONMENT_SET_BATCH_LOAD:
         g_console.block_zip_extract = *(const bool*)data;
         break;

      case SNES_ENVIRONMENT_SET_ROM_FORMATS:
         ssnes_console_set_rom_ext((const char*)data);
         break;
#endif

      case SNES_ENVIRONMENT_SET_MESSAGE:
      {
         const struct snes_message *msg = (const struct snes_message*)data;
         SSNES_LOG("Environ SET_MESSAGE: %s\n", msg->msg);
         if (g_extern.msg_queue)
            msg_queue_push(g_extern.msg_queue, msg->msg, 1, msg->frames);
         break;
      }

      default:
         SSNES_LOG("Environ UNSUPPORTED (#%u).\n", cmd);
         return false;
   }

   return true;
}
#endif

#ifdef NEED_DYNAMIC
// SSNES extension hooks. Totally optional 'n shizz :)
static void set_environment(void)
{
#ifdef HAVE_DYNAMIC
   dylib_t lib = lib_handle;
#else
   dylib_t lib = NULL;
#endif

   void (*psnes_set_environment)(snes_environment_t) = 
      (void (*)(snes_environment_t))dylib_proc(lib, "snes_set_environment");

   if (psnes_set_environment)
      psnes_set_environment(environment_cb);
}
#elif defined(SSNES_CONSOLE)
// Not very optional when we're running on a console without dynamic loading support.
static void set_environment(void)
{
   snes_set_environment(environment_cb);
}
#endif

// Assume SNES as defaults.
static void set_environment_defaults(void)
{
   SSNES_LOG("Setting environment defaults (SNES)\n");
   g_extern.system.pitch = 0; // 0 is classic libsnes semantics.
   g_extern.system.geom.base_width = 256;
   g_extern.system.geom.base_height = 224;
   g_extern.system.geom.max_width = 512;
   g_extern.system.geom.max_height = 512;

   // Split up environment variables beforehand.
   if (g_extern.system.environment_split && strtok(g_extern.system.environment_split, ";"))
      while (strtok(NULL, ";"));
}

