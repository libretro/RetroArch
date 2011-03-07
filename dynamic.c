/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
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
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <libsnes.hpp>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#ifdef HAVE_DYNAMIC
#define DLSYM(lib, x) dylib_proc(lib, #x)

#define SYM(x) do { \
   p##x = DLSYM(lib_handle, x); \
   if (p##x == NULL) { SSNES_ERR("Failed to load symbol: \"%s\"\n", #x); exit(1); } \
} while(0)

static dylib_t lib_handle = NULL;
#endif


void (*psnes_init)(void);

void (*psnes_set_video_refresh)(snes_video_refresh_t);
void (*psnes_set_audio_sample)(snes_audio_sample_t);
void (*psnes_set_input_poll)(snes_input_poll_t);
void (*psnes_set_input_state)(snes_input_state_t);

void (*psnes_run)(void);

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

#ifdef HAVE_DYNAMIC
static void load_dynamic(void)
{
   SSNES_LOG("Loading dynamic libsnes from: \"%s\"\n", g_settings.libsnes);
   lib_handle = dylib_load(g_settings.libsnes);
   if (!lib_handle)
   {
      SSNES_ERR("Failed to open dynamic library: \"%s\"\n", g_settings.libsnes);
      exit(1);
   }

   SYM(snes_init);
   SYM(snes_set_video_refresh);
   SYM(snes_set_audio_sample);
   SYM(snes_set_input_poll);
   SYM(snes_set_input_state);
   SYM(snes_library_revision_minor);
   SYM(snes_library_revision_major);
   SYM(snes_run);
   SYM(snes_get_region);
   SYM(snes_load_cartridge_normal);
   SYM(snes_load_cartridge_super_game_boy);
   SYM(snes_load_cartridge_bsx);
   SYM(snes_load_cartridge_bsx_slotted);
   SYM(snes_load_cartridge_sufami_turbo);
   SYM(snes_set_controller_port_device);
   SYM(snes_serialize_size);
   SYM(snes_serialize);
   SYM(snes_unserialize);
   SYM(snes_set_cartridge_basename);
   SYM(snes_get_memory_data);
   SYM(snes_get_memory_size);
   SYM(snes_unload_cartridge);
   SYM(snes_term);
}
#endif

#define SSYM(x) do { \
   p##x = x; \
} while(0)

#ifndef HAVE_DYNAMIC
static void set_statics(void)
{
   SSYM(snes_init);
   SSYM(snes_set_video_refresh);
   SSYM(snes_set_audio_sample);
   SSYM(snes_set_input_poll);
   SSYM(snes_set_input_state);
   SSYM(snes_library_revision_minor);
   SSYM(snes_library_revision_major);
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

void init_dlsym(void)
{
#ifdef HAVE_DYNAMIC
   if (strlen(g_settings.libsnes) > 0)
      load_dynamic();
   else
   {
      SSNES_ERR("This binary is built to use runtime dynamic binding of libsnes. Set libsnes_path in config to load a libsnes library dynamically.\n");
      exit(1);
   }
#else
      set_statics();
#endif
}

void uninit_dlsym(void)
{
#ifdef HAVE_DYNAMIC
   if (lib_handle)
      dylib_close(lib_handle);
#endif
}

// Platform independent dylib loading.
dylib_t dylib_load(const char *path)
{
#ifdef _WIN32
   return LoadLibrary(path);
#else
   return dlopen(path, RTLD_LAZY);
#endif
}

void* dylib_proc(dylib_t lib, const char *proc)
{
#ifdef _WIN32
   return GetProcAddress(lib, proc);
#else
   return dlsym(lib, proc);
#endif
}

void dylib_close(dylib_t lib)
{
#ifdef _WIN32
   FreeLibrary(lib);
#else
   dlclose(lib);
#endif
}

