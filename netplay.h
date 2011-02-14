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


#ifndef __SSNES_NETPLAY_H
#define __SSNES_NETPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include <libsnes.hpp>

void input_poll_net(void);
int16_t input_state_net(bool port, unsigned device, unsigned index, unsigned id);
void video_frame_net(const uint16_t *data, unsigned width, unsigned height);
void audio_sample_net(uint16_t left, uint16_t right);

typedef struct netplay netplay_t;

struct snes_callbacks
{
   snes_video_refresh_t frame_cb;
   snes_audio_sample_t sample_cb;
   snes_input_poll_t poll_cb;
   snes_input_state_t state_cb;
};

// Creates a new netplay handle. A NULL host means we're hosting (player 1). :)
netplay_t *netplay_new(const char *server, uint16_t port, unsigned frames, const struct snes_callbacks *cb);
void netplay_free(netplay_t *handle);

// Call this before running snes_run()
void netplay_pre_frame(netplay_t *handle);
// Call this after running snes_run()
void netplay_post_frame(netplay_t *handle);

// Checks if input port/index is controlled by netplay or not.
bool netplay_is_alive(netplay_t *handle);

bool netplay_poll(netplay_t *handle);
int16_t netplay_input_state(netplay_t *handle, bool port, unsigned device, unsigned index, unsigned id);

// If we're fast-forward replaying to resync, check if we should actually show frame.
bool netplay_should_skip(netplay_t *handle);
bool netplay_can_poll(netplay_t *handle);
const struct snes_callbacks* netplay_callbacks(netplay_t *handle);

#endif
