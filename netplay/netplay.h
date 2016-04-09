/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef __RARCH_NETPLAY_H
#define __RARCH_NETPLAY_H

#include <stdint.h>
#include <stddef.h>

#include <boolean.h>

#include "../libretro.h"
#include "../libretro_version_1.h"

typedef struct netplay netplay_t;

enum rarch_netplay_ctl_state
{
   RARCH_NETPLAY_CTL_NONE = 0,
   RARCH_NETPLAY_CTL_FLIP_PLAYERS,
   RARCH_NETPLAY_CTL_FULLSCREEN_TOGGLE,
   RARCH_NETPLAY_CTL_POST_FRAME,
   RARCH_NETPLAY_CTL_PRE_FRAME,
   RARCH_NETPLAY_CTL_IS_DATA_INITED
};

enum netplay_cmd
{
   /* Miscellaneous commands */

   /* Acknowlegement response */
   NETPLAY_CMD_ACK            = 0x0000, 

   /* Failed acknowlegement response */
   NETPLAY_CMD_NAK            = 0x0001, 

   /* Swap inputs between player 1 and player 2 */
   NETPLAY_CMD_FLIP_PLAYERS   = 0x0002,

   /* Toggle spectate/join mode */
   NETPLAY_CMD_SPECTATE       = 0x0003, 

   /* Gracefully disconnects from host */
   NETPLAY_CMD_DISCONNECT     = 0x0004, 

   /* Sends multiple config requests over, 
    * See enum netplay_cmd_cfg */
   NETPLAY_CMD_CFG            = 0x0005, 

   /* CMD_CFG streamlines sending multiple
      configurations. This acknowledges
      each one individually */
   NETPLAY_CMD_CFG_ACK        = 0x0006, 

   /* Loading and synchronization */

   /* Send a savestate for the client to load */
   NETPLAY_CMD_LOAD_SAVESTATE = 0x0012, 
   /* Sends over cheats enabled on client */
   NETPLAY_CMD_CHEATS         = 0x0013, 

   /* Controlling game playback */

   /* Pauses the game, takes no args  */
   NETPLAY_CMD_PAUSE          = 0x0030, 

   /* Resumes the game, takes no args */
   NETPLAY_CMD_RESUME         = 0x0031  
};

/* These are the configurations sent by NETPLAY_CMD_CFG. */
enum netplay_cmd_cfg
{
   /* Nickname */
   NETPLAY_CFG_NICK           = 0x0001, 

   /* input.netplay_client_swap_input */
   NETPLAY_CFG_SWAP_INPUT     = 0x0002, 

   /* netplay.sync_frames */
   NETPLAY_CFG_DELAY_FRAMES   = 0x0004, 

   /* For more than 2 players */
   NETPLAY_CFG_PLAYER_SLOT    = 0x0008  
};

void input_poll_net(void);

int16_t input_state_net(unsigned port, unsigned device,
      unsigned idx, unsigned id);

void video_frame_net(const void *data, unsigned width,
      unsigned height, size_t pitch);

void audio_sample_net(int16_t left, int16_t right);

size_t audio_sample_batch_net(const int16_t *data, size_t frames);

int16_t input_state_spectate(unsigned port, unsigned device,
      unsigned idx, unsigned id);

int16_t input_state_spectate_client(unsigned port, unsigned device,
      unsigned idx, unsigned id);

/**
 * netplay_new:
 * @server               : IP address of server.
 * @port                 : Port of server.
 * @frames               : Amount of lag frames.
 * @cb                   : Libretro callbacks.
 * @spectate             : If true, enable spectator mode.
 * @nick                 : Nickname of user.
 *
 * Creates a new netplay handle. A NULL host means we're 
 * hosting (user 1).
 *
 * Returns: new netplay handle.
 **/
netplay_t *netplay_new(const char *server,
      uint16_t port, unsigned frames,
      const struct retro_callbacks *cb, bool spectate,
      const char *nick);

/**
 * netplay_free:
 * @netplay              : pointer to netplay object
 *
 * Frees netplay handle.
 **/
void netplay_free(netplay_t *handle);

/**
 * netplay_pre_frame:   
 * @netplay              : pointer to netplay object
 *
 * Pre-frame for Netplay.
 * Call this before running retro_run().
 **/
void netplay_pre_frame(netplay_t *handle);

/**
 * netplay_post_frame:   
 * @netplay              : pointer to netplay object
 *
 * Post-frame for Netplay.
 * We check if we have new input and replay from recorded input.
 * Call this after running retro_run().
 **/
void netplay_post_frame(netplay_t *handle);

/**
 * init_netplay:
 *
 * Initializes netplay.
 *
 * If netplay is already initialized, will return false (0).
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool init_netplay(void);

void deinit_netplay(void);

bool netplay_driver_ctl(enum rarch_netplay_ctl_state state, void *data);

#endif
