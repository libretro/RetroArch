/*  SSNES - A Super Ninteno Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010 - Hans-Kristian Arntzen
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


#ifndef __SSNES_GENERAL_H
#define __SSNES_GENERAL_H

#include <stdbool.h>
#include "driver.h"
#include <stdio.h>
#include "record/ffemu.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SRC
#include <samplerate.h>
#endif


#define MAX_PLAYERS 2
#define MAX_BINDS 14
struct settings
{
   struct 
   {
      char driver[32];
      float xscale;
      float yscale;
      bool fullscreen;
      unsigned fullscreen_x;
      unsigned fullscreen_y;
      bool vsync;
      bool smooth;
      bool force_aspect;
      float aspect_ratio;
      char cg_shader_path[256];
      char bsnes_shader_path[256];
      unsigned filter;
   } video;

   struct
   {
      char driver[32];
      bool enable;
      unsigned out_rate;
      unsigned in_rate;
      char device[256];
      unsigned latency;
      bool sync;
      int src_quality;
   } audio;

   struct
   {
      char driver[32];
      struct snes_keybind binds[MAX_PLAYERS][MAX_BINDS];
      int save_state_key;
      int load_state_key;
      int toggle_fullscreen_key;
      int exit_emulator_key;
      float axis_threshold;
   } input;

   char libsnes[256];
};

struct global
{
   bool verbose;
   SRC_STATE *source;
   bool audio_active;
   bool video_active;

   FILE *rom_file;
   char savefile_name_srm[256];
   char config_path[256];
   char basename[256];

#ifdef HAVE_FFMPEG
   ffemu_t *rec;
   char record_path[256];
   bool recording;
#endif
};

void parse_config(void);

extern struct settings g_settings;
extern struct global g_extern;

#define SSNES_LOG(msg, args...) do { \
   if (g_extern.verbose) \
      fprintf(stderr, "SSNES: " msg, ##args); \
      fflush(stderr); \
   } while(0)

#define SSNES_ERR(msg, args...) do { \
      fprintf(stderr, "SSNES [ERROR] :: " msg, ##args); \
      fflush(stderr); \
   } while(0)

#define SSNES_WARN(msg, args...) do { \
      fprintf(stderr, "SSNES [WARN] :: " msg, ##args); \
      fflush(stderr); \
   } while(0)

#endif
