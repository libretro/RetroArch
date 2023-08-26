/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2016 - Andre Leiradella
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

#include <file/file_path.h>
#include <string/stdstring.h>
#include <streams/interface_stream.h>
#include <streams/file_stream.h>
#include <net/net_http.h>
#include <libretro.h>
#include <lrc_hash.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "webhooks.h"
#include "webhooks_macro_manager.h"
#include "webhooks_macro_parser.h"
#include "webhooks_progress_tracker.h"

#include "webhooks_rich_presence.h"
#include "webhooks_game.h"

//  Keeps dependency on rcheevos to compute the hash of a ROM.
#include "../deps/rcheevos/include/rc_api_runtime.h"

struct wb_identify_game_data_t
{
  struct rc_hash_iterator iterator;
  char* path;
  uint8_t* datacopy;
  char hash[HASH_LENGTH];
};

//  Contains all the shared state for all the webhook modules.
wb_locals_t locals;

void webhooks_send_game_event(int game_id, enum game_event_t game_event)
{
  wg_update_game(game_id, game_event);
}

void webhooks_game_unloaded()
{
}

static void wh_compute_hash(const struct retro_game_info* info)
{
  struct wb_identify_game_data_t* data;
  struct rc_hash_filereader file_reader;
  size_t len;

  data = (struct wb_identify_game_data_t*) calloc(1, sizeof(struct wb_identify_game_data_t));
  if (!data)
  {
    //RARCH_LOG(RCHEEVOS_TAG "allocation failed\n");return false;
    return;
  }

  /* provide hooks for reading files */
  memset(&file_reader, 0, sizeof(file_reader));
  file_reader.open = rc_hash_handle_file_open;
  file_reader.seek = rc_hash_handle_file_seek;
  file_reader.tell = rc_hash_handle_file_tell;
  file_reader.read = rc_hash_handle_file_read;
  file_reader.close = rc_hash_handle_file_close;
  rc_hash_init_custom_filereader(&file_reader);

  //rc_hash_init_error_message_callback(rcheevos_handle_log_message);

  rc_hash_reset_cdreader_hooks();

  /* fetch the first hash */
  rc_hash_initialize_iterator(&data->iterator, info->path, (uint8_t*)info->data, info->size);
  if (!rc_hash_iterate(data->hash, &data->iterator))
  {
    //CHEEVOS_LOG(RCHEEVOS_TAG "no hashes generated\n");
    rc_hash_destroy_iterator(&data->iterator);
    free(data);
    return;
  }

  memcpy(locals.hash, data->hash, HASH_LENGTH);
}

static void wh_on_macro_downloaded(wb_locals_t* locals, const char* macro, size_t length)
{
  wmp_parse_macro(macro, &locals->runtime);
}

//  ---------------------------------------------------------------------------
//
//  ---------------------------------------------------------------------------
void wb_initialize()
{
  //rc_runtime_init(&locals.runtime);
}

//  ---------------------------------------------------------------------------
//  Called when a new game is loaded in the emulator.
//  ---------------------------------------------------------------------------
void webhooks_game_loaded(const struct retro_game_info* info)
{
  //  IT IS DONE HERE NOW BUT I SHOULD TRY AND DO THIS ONLY ONCE IN wb_initialize().
  //  I NEED TO FIND THE PROPER PLACE TO HOOK IT UP.
  rc_runtime_init(&locals.runtime);

  wh_compute_hash(info);

  wmm_download_macro(&locals, &wh_on_macro_downloaded);
}

//  ---------------------------------------------------------------------------
//  Called for each frame.
//  ---------------------------------------------------------------------------
void webhooks_process_frame()
{
  wpt_process_frame(&locals.runtime);
}
