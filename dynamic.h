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

#ifndef __DYNAMIC_H
#define __DYNAMIC_H

#include <stdbool.h>
#include <libsnes.hpp>

void init_dlsym(void);
void uninit_dlsym(void);

typedef void *dylib_t;
typedef void (*function_t)(void);

dylib_t dylib_load(const char *path);
void dylib_close(dylib_t lib);
function_t dylib_proc(dylib_t lib, const char *proc);

extern void (*psnes_init)(void);

extern void (*psnes_set_video_refresh)(snes_video_refresh_t);
extern void (*psnes_set_audio_sample)(snes_audio_sample_t);
extern void (*psnes_set_input_poll)(snes_input_poll_t);
extern void (*psnes_set_input_state)(snes_input_state_t);

extern void (*psnes_cheat_reset)(void);
extern void (*psnes_cheat_set)(unsigned, bool, const char*);

extern const char* (*psnes_library_id)(void);
extern unsigned (*psnes_library_revision_minor)(void);
extern unsigned (*psnes_library_revision_major)(void);

extern bool (*psnes_load_cartridge_normal)(const char*, const uint8_t*, unsigned);
extern bool (*psnes_load_cartridge_super_game_boy)(
         const char*, const uint8_t*, unsigned, 
         const char*, const uint8_t*, unsigned);
extern bool (*psnes_load_cartridge_bsx)(
         const char*, const uint8_t*, unsigned, 
         const char*, const uint8_t*, unsigned);
extern bool (*psnes_load_cartridge_bsx_slotted)(
         const char*, const uint8_t*, unsigned, 
         const char*, const uint8_t*, unsigned);
extern bool (*psnes_load_cartridge_sufami_turbo)(
         const char*, const uint8_t*, unsigned, 
         const char*, const uint8_t*, unsigned, 
         const char*, const uint8_t*, unsigned);

extern void (*psnes_set_controller_port_device)(bool, unsigned);

extern bool (*psnes_get_region)(void);

extern unsigned (*psnes_serialize_size)(void);
extern bool (*psnes_serialize)(uint8_t*, unsigned);
extern bool (*psnes_unserialize)(const uint8_t*, unsigned);

extern void (*psnes_reset)(void);
extern void (*psnes_run)(void);

extern void (*psnes_set_cartridge_basename)(const char*);

extern uint8_t* (*psnes_get_memory_data)(unsigned);
extern unsigned (*psnes_get_memory_size)(unsigned);

extern void (*psnes_unload_cartridge)(void);
extern void (*psnes_term)(void);

#endif

