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
#include "webhooks_client.h"
#include "webhooks_macro_manager.h"
#include "webhooks_macro_parser.h"
#include "webhooks_progress_tracker.h"

#include "webhooks_client.h"
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

typedef struct async_http_request_t async_http_request_t;

typedef void (*async_http_handler)
(
  struct async_http_request_t *request,
  http_transfer_data_t *data,
  char buffer[],
  size_t buffer_size
);

typedef void (*async_client_callback)(void* userdata);

struct async_http_request_t
{
  rc_api_request_t request;
  async_http_handler handler;
  async_client_callback callback;
  void* callback_data;

  //  Not used yet.
  int id;
  int attempt_count;
  const char* success_message;
  const char* failure_message;
  const char* headers;
  char type;
};

//  Contains all the shared state for all the webhook modules.
wb_locals_t locals;

unsigned long frame_counter = 0;

//  ---------------------------------------------------------------------------
//
//  ---------------------------------------------------------------------------
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
  locals.console_id = data->iterator.consoles[0];
}

//  ---------------------------------------------------------------------------
//
//  ---------------------------------------------------------------------------
static void wh_on_macro_downloaded(wb_locals_t* locals, const char* macro, size_t length)
{
  wmp_parse_macro(macro, &locals->runtime);
}

//  ---------------------------------------------------------------------------
//
//  ---------------------------------------------------------------------------
static void wh_get_core_memory_info(unsigned id, rc_libretro_core_memory_info_t* info)
{
  retro_ctx_memory_info_t ctx_info;

  if (!info)
    return;

  ctx_info.id = id;
  if (core_get_memory(&ctx_info))
  {
    info->data = (unsigned char*)ctx_info.data;
    info->size = ctx_info.size;
  }
  else
  {
    info->data = NULL;
    info->size = 0;
  }
}

//  ---------------------------------------------------------------------------
//
//  ---------------------------------------------------------------------------
static int wh_init_memory(wb_locals_t* locals)
{
  unsigned i;
  int result;
  struct retro_memory_map mmap;
  rarch_system_info_t *sys_info               = &runloop_state_get_ptr()->system;
  rarch_memory_map_t *mmaps                   = &sys_info->mmaps;
  struct retro_memory_descriptor *descriptors = (struct retro_memory_descriptor*)malloc(mmaps->num_descriptors * sizeof(*descriptors));

  if (!descriptors)
    return 0;

  mmap.descriptors = &descriptors[0];
  mmap.num_descriptors = mmaps->num_descriptors;

  /* RetroArch wraps the retro_memory_descriptor's
   * in rarch_memory_descriptor_t's, pull them back out */
  for (i = 0; i < mmap.num_descriptors; ++i)
    memcpy(&descriptors[i], &mmaps->descriptors[i].core, sizeof(descriptors[0]));

  //rc_libretro_init_verbose_message_callback(rcheevos_handle_log_message);
  result = rc_libretro_memory_init
  (
    &locals->memory,
    &mmap,
    wh_get_core_memory_info,
    locals->console_id
  );

  free(descriptors);
  return result;
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
  //  -----------------------------------------------------------------------------
  //  IT IS DONE HERE NOW BUT I SHOULD TRY AND DO THIS ONLY ONCE IN wb_initialize().
  //  I NEED TO FIND THE PROPER PLACE TO HOOK IT UP.
  rc_runtime_init(&locals.runtime);

  //  -----------------------------------------------------------------------------

  frame_counter = 0;

  wh_compute_hash(info);
  
  wh_init_memory(&locals);

  wmm_download_macro(&locals, &wh_on_macro_downloaded);
}

//  ---------------------------------------------------------------------------
//  Called for each frame.
//  ---------------------------------------------------------------------------
void webhooks_game_unloaded()
{
}

//  ---------------------------------------------------------------------------
//  Called for each frame.
//  ---------------------------------------------------------------------------
void webhooks_process_frame()
{
  frame_counter++;

  int result = wpt_process_frame(&locals.runtime);

  if (result == PROGRESS_UNCHANGED) {
    return;
  }

  wc_update_progress(locals.hash, wpt_get_last_progress(), frame_counter);
}

void webhooks_send_game_event(int game_id, enum game_event_t game_event)
{
  //wg_update_game(game_id, game_event);
}
