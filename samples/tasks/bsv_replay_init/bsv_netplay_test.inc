/* RetroArch - GPLv3 or later. See COPYING.
 * The shipping BSV recorder, eligibility check and post-frame rollback loop
 * run against a deterministic core. No ROMs or network services are needed.
 * Socket transport, input resolution and rollback serialization are fixtures;
 * movie input staging, confirmation, writing and offline reading are real. */
#include <features/features_cpu.h>
#include <math/float_minmax.h>
#include "../../../network/netplay/netplay_private.h"
#include "../../../tasks/tasks_internal.h"

static net_driver_state_t networking_driver_st;
static uint16_t bsv_remote[8];
static uint32_t bsv_first_frame = 500;
static unsigned bsv_core_runs;
static bool bsv_replaying_offline;
static bool bsv_mask_reads;
#define NETPLAY_ASSERT_MODUS(m) ((void)0)

#define input_driver_st input_st
#include "netplay_bsv_frontend.inc"
#undef input_driver_st
#include "netplay_bsv_recording.inc"
#include "netplay_bsv_eligibility.inc"

static uint16_t bsv_remote_buttons(uint32_t count)
{
   unsigned frame = count - bsv_first_frame;
   /* A player leaves and rejoins: the absent port reads zero. */
   return frame >= 8 && frame < 13 ? 0 :
      (uint16_t)(1 | ((frame & 1) ? 0x8000 : 0));
}

static int16_t bsv_query(netplay_t *netplay, unsigned port, unsigned device,
      unsigned idx, unsigned id, int16_t resolved)
{
   if (bsv_replaying_offline)
      return bsv_movie_read_state(&input_st, port, device, idx, id);
   netplay_bsv_input(netplay, port, device, idx, id, resolved);
   return resolved;
}

static void bsv_emulate(netplay_t *netplay, uint32_t count, uint16_t remote)
{
   unsigned i;
   int16_t p1, p2, axis;
   p1 = bsv_query(netplay, 0, RETRO_DEVICE_JOYPAD, 0,
         RETRO_DEVICE_ID_JOYPAD_MASK, (int16_t)(count & 15));
   if (bsv_mask_reads)
      p2 = bsv_query(netplay, 1, RETRO_DEVICE_JOYPAD, 0,
            RETRO_DEVICE_ID_JOYPAD_MASK, (int16_t)remote);
   else
   {
      p2 = bsv_query(netplay, 1, RETRO_DEVICE_JOYPAD, 0,
            RETRO_DEVICE_ID_JOYPAD_B, (remote & 1) != 0);
      p2 |= (uint16_t)bsv_query(netplay, 1, RETRO_DEVICE_JOYPAD, 0,
            RETRO_DEVICE_ID_JOYPAD_R3, (remote & 0x8000) != 0) << 15;
   }
   axis = bsv_query(netplay, 2, RETRO_DEVICE_ANALOG,
         RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y,
         (int16_t)(-20000 + 17 * (count - bsv_first_frame)));
   CHECK(bsv_query(netplay, 3, RETRO_DEVICE_JOYPAD, 0,
            RETRO_DEVICE_ID_JOYPAD_MASK, 0) == 0, "absent port");
   /* A core may poll a button hundreds of times in one retro_run. */
   for (i = 0; i < 600; i++)
      CHECK(bsv_query(netplay, 0, RETRO_DEVICE_JOYPAD, 0,
               RETRO_DEVICE_ID_JOYPAD_MASK, (int16_t)(count & 15)) == p1,
            "repeated poll");
   core_state[0] += (uint8_t)(p1 + (uint16_t)p2 * 3);
   core_state[1] ^= (uint8_t)(axis >> 8);
   core_state[2] += (p2 != 0) ? 3 : 1;
   core_state[3]++;
}

bool core_run(void)
{
   netplay_t *netplay = networking_driver_st.data;
   bool replay = (netplay->flags & NETPLAY_FLAG_IS_REPLAY) != 0;
   size_t ptr = replay ? netplay->replay_ptr : netplay->run_ptr;
   uint32_t count = replay ? netplay->replay_frame_count : netplay->run_frame_count;
   bsv_emulate(netplay, count, bsv_remote[ptr]);
   bsv_core_runs++;
   return true;
}

/* External effects of post-frame; all movie work stays in the real code. */
void autosave_lock(void) {}
void autosave_unlock(void) {}
void input_driver_set_nonblocking(bool value) { (void)value; }
void driver_set_nonblock_state(void) {}
void core_reset(void) {}
bool netplay_wait_and_init_serialization(netplay_t *netplay)
{ (void)netplay; return true; }
static bool netplay_cmd_stall(netplay_t *netplay,
      struct netplay_connection *connection, uint32_t frames)
{ (void)netplay; (void)connection; (void)frames; return true; }
static void netplay_handle_frame_hash(netplay_t *netplay, struct delta_frame *delta)
{ (void)netplay; (void)delta; }
static bool netplay_lockstep_wants_crc(netplay_t *netplay, struct delta_frame *delta)
{ (void)netplay; (void)delta; return false; }
static bool netplay_lockstep_take_crc(netplay_t *netplay,
      struct delta_frame *delta, bool have_state)
{ (void)netplay; (void)delta; return have_state; }
static bool netplay_build_savestate(netplay_t *netplay,
      retro_ctx_serialize_info_t *info, bool achievements)
{ (void)netplay; (void)achievements; return core_serialize(info); }
static bool netplay_process_savestate(netplay_t *netplay, retro_ctx_serialize_info_t *info)
{ (void)netplay; return core_unserialize(info); }
static bool netplay_resolve_input(netplay_t *netplay, size_t ptr, bool resim)
{
   uint32_t frame = netplay->buffer[ptr].frame;
   uint16_t next = frame < netplay->unread_frame_count
      ? bsv_remote_buttons(frame) : bsv_remote[ptr];
   bool changed = next != bsv_remote[ptr];
   (void)resim;
   bsv_remote[ptr] = next;
   return changed;
}

#include "netplay_bsv_post.inc"

static void bsv_net_setup(netplay_t *netplay, unsigned role)
{
   unsigned i;
   memset(netplay, 0, sizeof(*netplay));
   memset(&networking_driver_st, 0, sizeof(networking_driver_st));
   memset(bsv_remote, 0, sizeof(bsv_remote));
   netplay->buffer_size = 8;
   netplay->buffer = (struct delta_frame*)calloc(8, sizeof(*netplay->buffer));
   for (i = 0; i < 8; i++)
      netplay->buffer[i].state = calloc(1, STATE_SIZE);
   netplay->state_size  = STATE_SIZE;
   netplay->modus       = NETPLAY_MODUS_INPUT_FRAME_SYNC;
   netplay->flags       = role == 0 ? NETPLAY_FLAG_IS_SERVER : 0;
   netplay->self_mode   = role == 2 ? NETPLAY_CONNECTION_SPECTATING
                                  : NETPLAY_CONNECTION_PLAYING;
   netplay->self_devices = role == 2 ? 0 : (1U << role);
   netplay->connected_players = 3;
   netplay->run_frame_count = netplay->other_frame_count =
      netplay->unread_frame_count = bsv_first_frame;
   netplay->self_frame_count = bsv_first_frame + 4;
   netplay->input_latency_frames = 4;
   netplay->run_ptr = netplay->other_ptr = netplay->unread_ptr = 5;
   netplay->self_ptr = 1;
   networking_driver_st.flags = NET_DRIVER_ST_FLAG_NETPLAY_ENABLED;
   networking_driver_st.data = netplay;
   bsv_core_runs = 0;
   bsv_replaying_offline = false;
   fill_state(123);
}

static void bsv_net_free(netplay_t *netplay)
{
   unsigned i;
   for (i = 0; i < 8; i++)
   {
      netplay_bsv_frame_free(netplay->buffer[i].bsv);
      free(netplay->buffer[i].state);
   }
   free(netplay->buffer);
   memset(&networking_driver_st, 0, sizeof(networking_driver_st));
}

/* Committed frames keep no state, and a stopped recording keeps no
 * staging at all, so a long session cannot pile up savestates. */
static void bsv_net_check_released(netplay_t *netplay, bool all,
      const char *what)
{
   unsigned i;
   for (i = 0; i < netplay->buffer_size; i++)
   {
      struct netplay_bsv_frame *frame = netplay->buffer[i].bsv;
      if (all)
         CHECK(!frame, "%s: slot %u released", what, i);
      else if (frame && !(frame->flags & NETPLAY_BSV_FRAME_VALID))
         CHECK(!frame->checkpoint && !frame->initial_state,
               "%s: committed slot %u keeps no state", what, i);
   }
}

static void bsv_net_record(const char *path, unsigned role, unsigned start)
{
   unsigned i;
   netplay_t *netplay = (netplay_t*)calloc(1, sizeof(*netplay));
   bsv_net_setup(netplay, role);
   settings.uints.replay_checkpoint_interval = 1;
   settings.bools.savestate_file_compression = true;
   for (i = 0; i < start + 64; i++)
   {
      struct delta_frame *delta = &netplay->buffer[netplay->run_ptr];
      if (i == start)
      {
         unsigned serialized = n_serialize;
         CHECK(movie_start_record(&input_st, (char*)path), "start netplay recording");
         task_queue_check();
         bsv_movie_dequeue_next(&input_st);
         CHECK(input_st.bsv_movie_state_handle
               && BSV_MOVIE_IS_NETPLAY(input_st.bsv_movie_state_handle),
               "netplay owns the recorder");
         CHECK(n_serialize == serialized,
               "no speculative state serialized at record start");
         CHECK(replay_get_serialize_size(&input_st) == 0,
               "netplay movie stays out of savestates");
      }
      delta->used = true;
      delta->frame = netplay->run_frame_count;
      if (delta->bsv)
         delta->bsv->flags &= ~NETPLAY_BSV_FRAME_FORCED;
      memcpy(delta->state, core_state, STATE_SIZE);
      bsv_remote[netplay->run_ptr] = 0; /* initially predict no remote input */
      if (i == start + 7)
         input_st.bsv_movie_state.flags |= BSV_FLAG_MOVIE_FORCE_CHECKPOINT;
      netplay_bsv_begin_frame(netplay, netplay->run_ptr, netplay->run_frame_count);
      core_run();
      netplay->unread_frame_count = bsv_first_frame + (i > 2 ? i - 2 : 0);
      netplay->unread_ptr = (5 + netplay->unread_frame_count - bsv_first_frame) % 8;
      netplay->read_frame_count[0] = netplay->read_frame_count[1] = netplay->unread_frame_count;
      netplay->server_frame_count = netplay->unread_frame_count;
      netplay_sync_input_post_frame(netplay, false);
      if (i >= start)
      {
         CHECK(input_st.bsv_movie_state_handle != NULL, "recording still active at %u", i);
         if (!input_st.bsv_movie_state_handle)
            break;
         CHECK(input_st.bsv_movie_state_handle->frame_counter ==
               (i > start + 2 ? i - start - 2 : 0),
               "only confirmed frames written at %u", i);
      }
      netplay_sync_input_post_frame(netplay, true);
      CHECK(netplay->run_frame_count == bsv_first_frame + i + 1, "stall adds no frame");
   }
   netplay->unread_frame_count = netplay->run_frame_count;
   netplay->unread_ptr = netplay->run_ptr;
   netplay_sync_input_post_frame(netplay, true);
   CHECK(input_st.bsv_movie_state_handle->frame_counter == 64, "64 confirmed frames");
   CHECK(bsv_core_runs > 64, "rollback actually ran");
   bsv_net_check_released(netplay, false, "after commit");
   CHECK(movie_stop_record(&input_st), "stop netplay recording");
   /* Stopped from the menu, not by netplay: the next frame drops staging. */
   CHECK(netplay->buffer[netplay->run_ptr].bsv != NULL, "staging left to drop");
   netplay_bsv_begin_frame(netplay, netplay->run_ptr, netplay->run_frame_count);
   bsv_net_check_released(netplay, true, "after a menu stop");
   bsv_net_free(netplay);
   free(netplay);
}

static void bsv_net_play(const char *path, unsigned start)
{
   unsigned i;
   uint8_t expected[69][4];
   fill_state(123);
   memcpy(expected[0], core_state, 4);
   /* Independent state oracle, not the recorder or core_run fixture. */
   for (i = 0; i < start + 64; i++)
   {
      uint16_t buttons = bsv_remote_buttons(bsv_first_frame + i);
      int16_t axis = (int16_t)(-20000 + 17 * i);
      expected[i+1][0] = (uint8_t)(expected[i][0] + ((bsv_first_frame+i) & 15) + buttons * 3);
      expected[i+1][1] = expected[i][1] ^ (uint8_t)(axis >> 8);
      expected[i+1][2] = expected[i][2] + (buttons ? 3 : 1);
      expected[i+1][3] = expected[i][3] + 1;
   }
   memset(core_state, 0xcc, STATE_SIZE);
   CHECK(play(path), "offline start");
   bsv_movie_dequeue_next(&input_st);
   CHECK(memcmp(core_state, expected[start], 4) == 0, "common initial state");
   bsv_replaying_offline = true;
   for (i = start; i < start + 64; i++)
   {
      bsv_movie_t *movie = input_st.bsv_movie_state_handle;
      CHECK(movie->input_event_count <= 6, "repeated reads deduplicated");
      bsv_emulate(NULL, bsv_first_frame + i, 0);
      CHECK(memcmp(core_state, expected[i+1], 4) == 0, "offline state at frame %u", i);
      /* Check snapshot timing before the reader could restore it.
       * Request decoding but clear checkpoint_ready before advancing,
       * so checkpoints never conceal an input desync. */
      if (i == start + 7 || i == start + 59)
         CHECK(memcmp(movie->cur_save, core_state, STATE_SIZE) == 0,
               "checkpoint is the confirmed post-frame state at %u", i);
      movie->checkpoint_ready = false;
      bsv_movie_next_frame(&input_st, 0, true);
   }
   CHECK(input_st.bsv_movie_state.flags & BSV_FLAG_MOVIE_END, "exact offline EOF");
   movie_stop(&input_st);
   bsv_replaying_offline = false;
}

/* The existing BSV header identifies recordings by wall-clock time. */
static void bsv_net_compare(const char *left, const char *right)
{
   FILE *a = fopen(left, "rb");
   FILE *b = fopen(right, "rb");
   size_t offset = 0;
   int ca, cb;
   CHECK(a && b, "open role recordings");
   if (a && b)
      do
      {
         ca = fgetc(a);
         cb = fgetc(b);
         if (offset < REPLAY_HEADER_IDENTIFIER_INDEX * sizeof(uint32_t)
               || offset >= REPLAY_HEADER_IDENTIFIER_INDEX * sizeof(uint32_t) + 8)
            CHECK(ca == cb, "identical role recordings at byte %lu", (unsigned long)offset);
         offset++;
      } while (ca != EOF && cb != EOF);
   if (a)
      fclose(a);
   if (b)
      fclose(b);
}

static void lane_netplay_lifecycle(void)
{
   netplay_t *netplay = (netplay_t*)calloc(1, sizeof(*netplay));
   bsv_movie_t *movie = (bsv_movie_t*)calloc(1, sizeof(*movie));
   struct retro_netpacket_callback packet = {0};
   struct delta_frame *delta;
   reset_counters();
   bsv_net_setup(netplay, 1);
   netplay->self_mode = NETPLAY_CONNECTION_NONE;
   CHECK(!netplay_bsv_movie_init(movie), "reject unsynchronized client");
   netplay->self_mode = NETPLAY_CONNECTION_PLAYING;
   netplay->flags |= NETPLAY_FLAG_FORCE_REWIND;
   CHECK(!netplay_bsv_movie_init(movie), "reject pending resynchronization");
   netplay->flags &= ~NETPLAY_FLAG_FORCE_REWIND;
   networking_driver_st.core_netpacket_interface = &packet;
   CHECK(netplay_bsv_movie_init(movie) && !BSV_MOVIE_IS_NETPLAY(movie), "packet interface unchanged");
   networking_driver_st.core_netpacket_interface = NULL;
   networking_driver_st.flags = 0;
   CHECK(netplay_bsv_movie_init(movie) && !BSV_MOVIE_IS_NETPLAY(movie), "solo unchanged");
   networking_driver_st.flags = NET_DRIVER_ST_FLAG_NETPLAY_ENABLED;
   netplay->flags |= NETPLAY_FLAG_IS_SERVER;
   netplay->connected_players = 1;
   CHECK(movie_start_record(&input_st, "bsv_netplay_lifecycle.replay"), "start host alone");
   task_queue_check();
   bsv_movie_dequeue_next(&input_st);
   delta = &netplay->buffer[netplay->run_ptr];
   delta->used = true;
   delta->frame = netplay->run_frame_count;
   netplay_bsv_begin_frame(netplay, netplay->run_ptr, netplay->run_frame_count);
   core_run();
   netplay_sync_input_post_frame(netplay, false);
   CHECK(input_st.bsv_movie_state_handle->frame_counter == 1,
         "confirmation clamped to emulation despite local input latency");
   CHECK(!movie_seek_to_frame(&input_st, 0), "do not seek a live netplay recording");
   netplay_sync_input_post_frame(netplay, true);
   CHECK(input_st.bsv_movie_state_handle->frame_counter == 1, "host stall adds no frame");
   netplay_bsv_stop(netplay, "test resynchronization");
   CHECK(input_st.bsv_movie_state.flags & BSV_FLAG_MOVIE_END, "deferred stop requested");
   CHECK(!input_driver_netplay_bsv_recording(), "stopped recorder cannot capture more input");
   bsv_net_check_released(netplay, true, "after a netplay stop");
   CHECK(movie_stop(&input_st), "finish stopped movie outside netplay");
   CHECK(play("bsv_netplay_lifecycle.replay"), "confirmed prefix still playable");
   bsv_movie_dequeue_next(&input_st);
   bsv_movie_next_frame(&input_st, 0, false);
   CHECK(input_st.bsv_movie_state.flags & BSV_FLAG_MOVIE_END, "confirmed prefix has one frame");
   movie_stop(&input_st);
   filestream_delete("bsv_netplay_lifecycle.replay");
   free(movie);
   bsv_net_free(netplay);
   free(netplay);
   lane_done("netplay_lifecycle", "admission, latency without peers, deferred stop");
}

static void lane_netplay(void)
{
   unsigned role, start;
   const char *paths[3] = {"bsv_netplay_host.replay", "bsv_netplay_client.replay",
      "bsv_netplay_spectator.replay"};
   lane_netplay_lifecycle();
   reset_counters();
   for (bsv_mask_reads = false; ; bsv_mask_reads = true)
   {
      for (start = 0; start <= 4; start += 4)
      {
         for (role = 0; role < 3; role++)
         {
            bsv_net_record(paths[role], role, start);
            bsv_net_play(paths[role], start);
            if (role)
               bsv_net_compare(paths[0], paths[role]);
         }
         for (role = 0; role < 3; role++)
            filestream_delete(paths[role]);
      }
      if (bsv_mask_reads)
         break;
   }
   lane_done("netplay_confirmed_roundtrip", "host/client/spectator, buttons/masks, mid-session start, rollback, stalls, latency, checkpoints");
}
