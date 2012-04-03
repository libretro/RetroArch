/*  SSNES - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *

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


#ifndef __SSNES_NETPLAY_H
#define __SSNES_NETPLAY_H

#include <stdint.h>
#include "boolean.h"
#include "libsnes.hpp"

void input_poll_net(void);
int16_t input_state_net(bool port, unsigned device, unsigned index, unsigned id);
void video_frame_net(const uint16_t *data, unsigned width, unsigned height);
void audio_sample_net(uint16_t left, uint16_t right);

int16_t input_state_spectate(bool port, unsigned device, unsigned index, unsigned id);
int16_t input_state_spectate_client(bool port, unsigned device, unsigned index, unsigned id);

typedef struct netplay netplay_t;

struct snes_callbacks
{
   snes_video_refresh_t frame_cb;
   snes_audio_sample_t sample_cb;
   snes_input_state_t state_cb;
};

// Creates a new netplay handle. A NULL host means we're hosting (player 1). :)
netplay_t *netplay_new(const char *server,
      uint16_t port, unsigned frames,
      const struct snes_callbacks *cb, bool spectate,
      const char *nick);
void netplay_free(netplay_t *handle);

// On regular netplay, flip who controls player 1 and 2.
void netplay_flip_players(netplay_t *handle);

// Call this before running snes_run()
void netplay_pre_frame(netplay_t *handle);
// Call this after running snes_run()
void netplay_post_frame(netplay_t *handle);

#endif
