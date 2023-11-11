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
#include <limits.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "webhooks.h"
#include "webhooks_client.h"
#include "webhooks_progress_downloader.h"
#include "webhooks_progress_parser.h"
#include "webhooks_progress_tracker.h"
#include "webhooks_oauth.h"

#include "webhooks_client.h"

//  Keeps dependency on rcheevos to compute the hash of a ROM.
#include "../deps/rcheevos/include/rc_api_runtime.h"
//  Keeps dependency on rcheevos to reuse the triggers.
#include "../deps/rcheevos/include/rc_runtime.h"

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

const unsigned short LOADED = 1;
const unsigned short UNLOADED = USHRT_MAX;
const unsigned long PROGRESS_UPDATE_FRAME_FREQUENCY = 30;

//  ---------------------------------------------------------------------------
//
//  ---------------------------------------------------------------------------
unsigned wb_peek
(
  unsigned address,
  unsigned num_bytes,
  void* ud
)
{
  unsigned avail;
  uint8_t* data = rc_libretro_memory_find_avail(&locals.memory, address, &avail);

  if (data && avail >= num_bytes)
  {
    switch (num_bytes)
    {
      case 4:
        return (data[3] << 24) | (data[2] << 16) | (data[1] <<  8) | (data[0]);
      case 3:
        return (data[2] << 16) | (data[1] << 8) | (data[0]);
      case 2:
        return (data[1] << 8)  | (data[0]);
      case 1:
        return data[0];
    }
  }

  return 0;
}

//  ---------------------------------------------------------------------------
//
//  ---------------------------------------------------------------------------
static void wh_compute_hash(const struct retro_game_info* info)
{
  struct wb_identify_game_data_t* data;
  struct rc_hash_filereader file_reader;
  
  data = (struct wb_identify_game_data_t*) malloc(sizeof(struct wb_identify_game_data_t));
  if (!data)
  {
    WEBHOOKS_LOG(WEBHOOKS_TAG "allocation failed\n");
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

  WEBHOOKS_LOG(WEBHOOKS_TAG "Current game's hash is '%s'\n", locals.hash);
}

//  ---------------------------------------------------------------------------
//  Called when the game's progress has been received from the backend server.
//  ---------------------------------------------------------------------------
static void wh_on_game_progress_downloaded
(
  wb_locals_t* locals,
  const char* game_progress,
  size_t length
)
{
  wpp_parse_game_progress(game_progress, &locals->runtime);
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
//  Initialize wb_locals_t's memory field.
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
static void wb_check_progress
(
  unsigned long frame_counter,
  retro_time_t time
)
{
  //  No need to update the progress at every frame since
  //  it can the ability to submerge the server with many
  //  requests. For instance, when a boss is beaten in Castlevania 1,
  //  the score is changed very rapidly.
  //if (frame_counter % PROGRESS_UPDATE_FRAME_FREQUENCY != 0)
  //  return;

  int result = wpt_process_frame(&locals.runtime);

  if (result != PROGRESS_UNCHANGED) {
    wc_update_progress(locals.console_id, locals.hash, wpt_get_last_progress(), frame_counter, time);
  }
}

//  ---------------------------------------------------------------------------
//
//  ---------------------------------------------------------------------------
static void wb_check_game_events
(
  unsigned long frame_counter,
  retro_time_t time
)
{
  rc_runtime_trigger_t* triggers = locals.runtime.triggers;
  for (int trigger_num = 0; trigger_num < locals.runtime.trigger_count; ++trigger_num, ++triggers) {
    struct rc_trigger_t* trigger = triggers->trigger;
    int result = rc_evaluate_trigger(trigger, &wb_peek, NULL, 0);

    if (result == RC_TRIGGER_STATE_TRIGGERED) {
      int event_id = locals.runtime.triggers[trigger_num].id;
      wc_send_event(locals.console_id, locals.hash, event_id, frame_counter, time);
    }
  }
}

//  ---------------------------------------------------------------------------
//
//  ---------------------------------------------------------------------------
static void wb_reset_game_events()
{
  rc_runtime_trigger_t* triggers = locals.runtime.triggers;
  for (int trigger_num = 0; trigger_num < locals.runtime.trigger_count; ++trigger_num, ++triggers) {
    struct rc_trigger_t* trigger = triggers->trigger;
    rc_reset_trigger(trigger);
  }
}

//  ---------------------------------------------------------------------------
//
//  ---------------------------------------------------------------------------
void webhooks_initialize()
{
  if (locals.initialized) {
    return;
  }

  WEBHOOKS_LOG(WEBHOOKS_TAG "Initializing\n");

  //  -----------------------------------------------------------------------------
  //  IT IS DONE HERE NOW BUT I SHOULD TRY AND DO THIS ONLY ONCE IN wb_initialize().
  //  I NEED TO FIND THE PROPER PLACE TO HOOK IT UP.
  rc_runtime_init(&locals.runtime);

  woauth_get_accesstoken();

  locals.initialized = true;
}

//  ---------------------------------------------------------------------------
//  Called when a new game is loaded in the emulator.
//  ---------------------------------------------------------------------------
void webhooks_load_game(const struct retro_game_info* info)
{
  WEBHOOKS_LOG(WEBHOOKS_TAG "New game loaded: %s\n", info->path);

  frame_counter = 0;
  retro_time_t time = cpu_features_get_time_usec();

  wh_compute_hash(info);

  wh_init_memory(&locals);

  wpt_clear_progress();

  wc_send_event(locals.console_id, locals.hash, LOADED, frame_counter, time);

  wpd_download_game_progress(&locals, &wh_on_game_progress_downloaded);
}

//  ---------------------------------------------------------------------------
//  Called when a game is being unloaded.
//  ---------------------------------------------------------------------------
void webhooks_unload_game()
{
  WEBHOOKS_LOG(WEBHOOKS_TAG "Current game has been unloaded\n");

  retro_time_t time = cpu_features_get_time_usec();

  wc_send_event(locals.console_id, locals.hash, UNLOADED, frame_counter, time);
}

//  ---------------------------------------------------------------------------
//  Called when the game is being reset.
//  ---------------------------------------------------------------------------
void webhooks_reset_game()
{
  WEBHOOKS_LOG(WEBHOOKS_TAG "Current game has been reset\n");

  frame_counter = 0;
  retro_time_t time = cpu_features_get_time_usec();

  wb_reset_game_events();

  wpt_clear_progress();

  wc_send_event(locals.console_id, locals.hash, UNLOADED, frame_counter, time);

  wc_send_event(locals.console_id, locals.hash, LOADED, frame_counter, time);
}

//  ---------------------------------------------------------------------------
//  Called for each frame.
//  ---------------------------------------------------------------------------
void webhooks_process_frame()
{
  frame_counter++;
  retro_time_t time = cpu_features_get_time_usec();

  wb_check_game_events(frame_counter, time);

  wb_check_progress(frame_counter, time);
}
