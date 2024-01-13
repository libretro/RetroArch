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

#include "include/webhooks.h"
#include "include/webhooks_client.h"
#include "include/webhooks_progress_downloader.h"
#include "include/webhooks_progress_parser.h"
#include "include/webhooks_progress_tracker.h"
#include "include/webhooks_oauth.h"

//  Keeps dependency on rcheevos to compute the hash of a ROM.
//  Keeps dependency on rcheevos to reuse the triggers.
#include "../deps/rcheevos/include/rc_api_runtime.h"
#include "../deps/rcheevos/include/rc_runtime.h"
#include "../deps/rcheevos/include/rc_runtime_types.h"
#include "../deps/rcheevos/include/rc_hash.h"

#include "../cheevos/cheevos_locals.h"

#include "../runloop.h"

struct wb_identify_game_data_t
{
  rc_hash_iterator_t iterator;
  char* path;
  uint8_t* datacopy;
  char hash[HASH_LENGTH];
};

//  Contains all the shared state for all the webhook modules.
wb_locals_t locals;

int is_game_loaded = 0;
retro_time_t last_update_time;
unsigned long frame_counter = 0;

const unsigned short LOADED = 1;
const unsigned short STARTED = 2;
const unsigned short ACHIEVEMENT = 3;
const unsigned short KEEP_ALIVE = 4;
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

static void* wb_on_hash_handle_file_open
(
  const char* path
)
{
  return intfstream_open_file(path,
        RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
}

static void wb_on_hash_handle_file_seek
(
  void* file_handle,
  int64_t offset,
  int origin
)
{
  intfstream_seek((intfstream_t*)file_handle, offset, origin);
}

static int64_t wb_on_hash_handle_file_tell
(
  void* file_handle
)
{
  return intfstream_tell((intfstream_t*)file_handle);
}

static size_t wb_on_hash_handle_file_read
(
  void* file_handle,
  void* buffer,
  size_t requested_bytes
)
{
  return intfstream_read((intfstream_t*)file_handle,
        buffer, requested_bytes);
}

static void wb_on_hash_handle_file_close(void* file_handle)
{
  intfstream_close((intfstream_t*)file_handle);
  CHEEVOS_FREE(file_handle);
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
  file_reader.open = wb_on_hash_handle_file_open;
  file_reader.seek = wb_on_hash_handle_file_seek;
  file_reader.tell = wb_on_hash_handle_file_tell;
  file_reader.read = wb_on_hash_handle_file_read;
  file_reader.close = wb_on_hash_handle_file_close;
  rc_hash_init_custom_filereader(&file_reader);

  //rc_hash_init_error_message_callback(rcheevos_handle_log_message);

  //  TODO: Ignored for now: obj-unix/release/webhooks/webhooks.o:webhooks.c:(.text+0x27f): undefined reference to `rc_hash_reset_cdreader_hooks'
  //  rc_hash_reset_cdreader_hooks();

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
static void wh_get_core_memory_info
(
  unsigned id,
  rc_libretro_core_memory_info_t* info
)
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
static int wh_init_memory
(
  wb_locals_t* locals
)
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

    wc_update_progress
    (
      locals.console_id,
      locals.hash,
      wpt_get_last_progress(),
      frame_counter,
      time
    );
  }
  else {
    if (is_game_loaded == 1) {

      retro_time_t current_time = cpu_features_get_time_usec();

      long long elapsed_time = current_time - last_update_time;

      if (elapsed_time >= 60000000) {

        WEBHOOKS_LOG(WEBHOOKS_TAG "Sending KEEP ALIVE\n");

        wc_send_keep_alive_event
        (
          locals.console_id,
          locals.hash,
          KEEP_ALIVE,
          frame_counter,
          time
        );

        last_update_time = current_time;
      }
    }
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
      wc_send_game_event(locals.console_id, locals.hash, event_id, frame_counter, time);
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
void webhooks_load_game
(
  const struct retro_game_info* info
)
{
  WEBHOOKS_LOG(WEBHOOKS_TAG "New game loaded: %s\n", info->path);

  frame_counter = 0;
  retro_time_t time = cpu_features_get_time_usec();

  wh_compute_hash(info);

  wh_init_memory(&locals);

  wpt_clear_progress();

  wc_send_game_event(locals.console_id, locals.hash, LOADED, frame_counter, time);

  wpd_download_game_progress(&locals, &wh_on_game_progress_downloaded);

  is_game_loaded = 1;
}

//  ---------------------------------------------------------------------------
//  Called when a game is being unloaded.
//  ---------------------------------------------------------------------------
void webhooks_unload_game()
{
  WEBHOOKS_LOG(WEBHOOKS_TAG "Current game has been unloaded\n");

  retro_time_t time = cpu_features_get_time_usec();

  wc_send_game_event(locals.console_id, locals.hash, UNLOADED, frame_counter, time);

  is_game_loaded = 0;
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

  wc_send_game_event(locals.console_id, locals.hash, UNLOADED, frame_counter, time);

  wc_send_game_event(locals.console_id, locals.hash, LOADED, frame_counter, time);
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

void webhooks_update_achievements()
{
  int number_of_active  = 0;
  
  int total_number      = 0;

  //  Only deals with supported & official achievements.
  const rcheevos_racheevo_t* current_achievement = locals.current_achievement;

  for (; current_achievement < locals.last_achievement; current_achievement++)
  {
    if (current_achievement->active & RCHEEVOS_ACTIVE_UNOFFICIAL)
      continue;

    if (current_achievement->active & RCHEEVOS_ACTIVE_UNSUPPORTED)
      continue;

    total_number++;

    if (current_achievement->active)
      number_of_active++;
  }

  wc_send_achievement_event
  (
    locals.console_id,
    locals.hash,
    ACHIEVEMENT,
    number_of_active,
    total_number,
    frame_counter,
    time
   );
}

void webhooks_on_achievements_loaded
(
  const rcheevos_racheevo_t* achievements,
  const unsigned int achievements_count
)
{
  locals.current_achievement = achievements;
  locals.last_achievement    = achievements + achievements_count;

  webhooks_update_achievements();
}

void webhooks_on_achievement_awarded
(
  rcheevos_racheevo_t* cheevo
)
{
  webhooks_update_achievements();
}
