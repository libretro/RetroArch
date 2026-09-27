/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (bsv_replay_init_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Regression oracle for replay (.replay / .bsv) start-up and shutdown
 * in tasks/task_movie.c and input/bsv/bsvmovie.c - issue #19622.
 *
 * The units under test are the shipping tasks/task_movie.c and
 * input/bsv/bsvmovie.c translation units, compiled from the tree and
 * driven through the real task queue, intfstream and VFS against a
 * stub core.  The build matches the iOS one where the bug was
 * reported: HAVE_RZSTD on, HAVE_STATESTREAM off, so a checkpoint is a
 * RAW-encoded state under Zstandard compression.
 *
 * The runloop is not linked.  What it does with a replay is two
 * lines, mirrored verbatim in frame() below:
 *
 *    bsv_movie_dequeue_next(input_st);           runloop.c:8312
 *    ...core runs...
 *    bsv_movie_next_frame(input_st);
 *    if (flags & BSV_FLAG_MOVIE_END)
 *    {
 *       movie_stop(input_st);
 *       command_event(CMD_EVENT_PAUSE, NULL);    runloop.c:8615
 *    }
 *
 * and Close Content is movie_stop() again (runloop.c:4968).  If
 * runloop.c changes that protocol, frame() must follow.
 *
 * The bug
 * -------
 * bsv_movie_init_playback() reads the first checkpoint and the first
 * frame's events before the handle is installed.  Every short-read
 * branch on that path sets BSV_FLAG_MOVIE_END on the global input
 * state.  When init then failed, the handle was freed and never
 * enqueued, so MOVIE_END was left set with no PLAYBACK flag beside
 * it.  The runloop line above then fired movie_stop() + PAUSE after
 * every frame, and movie_stop() only clears MOVIE_END on the
 * PLAYBACK/RECORDING branches, so the flag was permanent: a pause
 * toggle ran exactly one frame and re-paused, and Close Content
 * carried the flag into the next content.  Only a process restart
 * cleared it.
 *
 * Two files reach that path: a replay truncated inside its first
 * checkpoint, and a replay the recorder itself produces when
 * recording is halted before a single frame has run - header plus
 * checkpoint, no frames.  The second is not a corrupt file; the
 * player just has to treat "no frames" as a replay that ends on
 * frame one, the way it ends any other replay.
 *
 * Lanes
 * -----
 *  roundtrip  record N frames, halt, play back, run to the end:
 *             playback ends through movie_stop_playback(), pausing
 *             exactly once, the flags are clear afterwards, and the
 *             core is back at the state it had when recording began
 *             (the checkpoint restore).  Run compressed (zstd) and
 *             uncompressed.  Guards the fixture: the other lanes are
 *             only meaningful if a good replay plays.
 *  truncated  same file cut inside the first checkpoint: playback
 *             start fails, the user is told, MOVIE_END is not left
 *             set, no frame pauses, Close Content leaves it clear.
 *  empty      record, halt before any frame, play: starts, ends on
 *             the first frame through the normal end-of-replay path,
 *             pauses once, flags clear.
 *  stray      MOVIE_END set with no movie active (any future init
 *             path that forgets to clear it): movie_stop() clears it.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <boolean.h>
#include <libretro.h>
#include <streams/file_stream.h>
#include <file/file_path.h>
#include <queues/task_queue.h>

#include "../../../configuration.h"
#include "../../../runloop.h"
#include "../../../core.h"
#include "../../../msg_hash.h"
#include "../../../input/input_driver.h"
#include "../../../input/bsv/bsvmovie.h"

/* ---- stub frontend ------------------------------------------------ */

#define STATE_SIZE (48 * 1024)   /* NES-sized */

static uint8_t core_state[STATE_SIZE];
static settings_t settings;
static runloop_state_t runloop_st;
static input_driver_state_t input_st;

static unsigned msgs_failed_load = 0;
static unsigned msgs_ended       = 0;
static unsigned n_pause          = 0;
static unsigned n_fail           = 0;

#define CHECK(cond, ...) do { if (!(cond)) { \
   fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
   fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); n_fail++; } } while (0)

void RARCH_LOG(const char *fmt, ...)  { va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap); }
void RARCH_WARN(const char *fmt, ...) { va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap); }
void RARCH_ERR(const char *fmt, ...)  { va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap); }

settings_t *config_get_ptr(void) { return &settings; }
runloop_state_t *runloop_state_get_ptr(void) { return &runloop_st; }
input_driver_state_t *input_state_get_ptr(void) { return &input_st; }
bool content_load_state_in_progress(void *data) { (void)data; return false; }
void input_keyboard_event(bool down, unsigned code, uint32_t character,
      uint16_t mod, unsigned device) { (void)down; (void)code; (void)character; (void)mod; (void)device; }

const char *msg_hash_to_str(enum msg_hash_enums msg)
{
   switch (msg)
   {
      case MSG_FAILED_TO_LOAD_MOVIE_FILE: return "failed-to-load";
      case MSG_MOVIE_PLAYBACK_ENDED:      return "playback-ended";
      default:                            return "msg";
   }
}

void runloop_msg_queue_push(const char *msg, size_t len,
      unsigned prio, unsigned duration, bool flush, char *title,
      enum message_queue_icon icon, enum message_queue_category category)
{
   (void)len; (void)prio; (void)duration; (void)flush; (void)title; (void)icon; (void)category;
   if (!strcmp(msg, "failed-to-load"))
      msgs_failed_load++;
   else if (!strcmp(msg, "playback-ended"))
      msgs_ended++;
}

size_t core_serialize_size(void) { return STATE_SIZE; }
bool core_serialize(retro_ctx_serialize_info_t *info)
{
   memcpy(info->data, core_state, STATE_SIZE);
   return true;
}
bool core_unserialize(retro_ctx_serialize_info_t *info)
{
   if (info->size != STATE_SIZE)
      return false;
   memcpy(core_state, info->data_const, STATE_SIZE);
   return true;
}

/* The core: a state that changes every frame, and is compressible
 * the way a real one is (mostly RAM with a few bytes moving). */
static void stub_core_run(unsigned frame)
{
   core_state[frame % STATE_SIZE] ^= (uint8_t)(0x5a + frame);
   core_state[(frame * 7919) % STATE_SIZE] += 1;
}

/* Mirror of the runloop's per-frame replay protocol; see the header. */
static void frame(unsigned n)
{
   bsv_movie_dequeue_next(&input_st);
   stub_core_run(n);
   bsv_movie_next_frame(&input_st);
   if (input_st.bsv_movie_state.flags & BSV_FLAG_MOVIE_END)
   {
      movie_stop(&input_st);
      runloop_st.flags |= RUNLOOP_FLAG_PAUSED;
      n_pause++;
   }
}

/* The user hits pause toggle; the runloop runs frames again. */
static void unpause(void) { runloop_st.flags &= ~RUNLOOP_FLAG_PAUSED; }
static bool paused(void)  { return !!(runloop_st.flags & RUNLOOP_FLAG_PAUSED); }

/* Close Content, as far as replays are concerned (runloop.c:4968). */
static void close_content(void) { movie_stop(&input_st); }

static unsigned n_fail_at_lane_start = 0;

static void reset_counters(void)
{
   msgs_failed_load = msgs_ended = n_pause = 0;
   runloop_st.flags = 0;
   n_fail_at_lane_start = n_fail;
}

static void lane_done(const char *name, const char *variant)
{
   printf("[%s] %s%s%s\n", n_fail == n_fail_at_lane_start ? "ok" : "FAIL",
         name, variant ? " " : "", variant ? variant : "");
}

/* ---- fixtures ------------------------------------------------------ */

static void fill_state(unsigned seed)
{
   unsigned i;
   uint32_t x = 0x9e3779b9u ^ seed;
   /* mostly-quiet RAM with a noisy stripe, so zstd has something to do */
   memset(core_state, 0, STATE_SIZE);
   for (i = 0; i < STATE_SIZE / 8; i++)
   {
      x ^= x << 13; x ^= x >> 17; x ^= x << 5;
      core_state[i] = (uint8_t)x;
   }
}

/* Record @frames frames to @path through the real task path.
 * Returns the core state as of the first recorded frame in @start. */
static void record(const char *path, unsigned frames, bool compress,
      uint8_t *start)
{
   unsigned i;
   settings.bools.savestate_file_compression = compress;
   fill_state(frames);
   memcpy(start, core_state, STATE_SIZE);

   CHECK(movie_start_record(&input_st, (char*)path), "start_record");
   task_queue_check();   /* runs the record task and its callback */
   CHECK(input_st.bsv_movie_state.flags & BSV_FLAG_MOVIE_RECORDING,
         "recording flag not set after task");
   for (i = 0; i < frames; i++)
      frame(i);
   CHECK(movie_stop(&input_st), "halt");
   CHECK(!(input_st.bsv_movie_state.flags
            & (BSV_FLAG_MOVIE_RECORDING | BSV_FLAG_MOVIE_END)),
         "flags after halt: %x", input_st.bsv_movie_state.flags);
   CHECK(!input_st.bsv_movie_state_handle && !input_st.bsv_movie_state_next_handle,
         "handle left after halt");
}

/* Play Replay from the menu: the command pushes the task; the task
 * queue runs it and its callback installs (or fails to install) the
 * handle. */
static bool play(const char *path)
{
   bool ret = movie_start_playback(&input_st, (char*)path);
   task_queue_check();
   return ret;
}

static void truncate_file(const char *path, int64_t len)
{
   RFILE *f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ_WRITE
         | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   CHECK(f != NULL, "open for truncate");
   if (f)
   {
      CHECK(filestream_truncate(f, len) == 0, "truncate");
      filestream_close(f);
   }
}

/* ---- lanes ----------------------------------------------------------- */

static void lane_roundtrip(const char *path, bool compress)
{
   uint8_t start[STATE_SIZE];
   unsigned i;
   const unsigned frames = 40;

   reset_counters();
   record(path, frames, compress, start);

   /* Run on a bit so the core is somewhere else when playback starts;
    * the checkpoint restore must bring it back. */
   for (i = 0; i < 10; i++)
      stub_core_run(1000 + i);
   CHECK(memcmp(core_state, start, STATE_SIZE) != 0, "core did not move");

   CHECK(play(path), "play (%s)", compress ? "zstd" : "raw");
   CHECK(msgs_failed_load == 0, "failed-to-load message on a good replay");
   CHECK(input_st.bsv_movie_state.flags & BSV_FLAG_MOVIE_PLAYBACK,
         "playback flag not set");
   /* First frame promotes the handle and restores the checkpoint. */
   frame(0);
   /* The recorded frame 0 input... we do not replay input here; what
    * the checkpoint restore gives us is the state at record start with
    * frame 0 applied on top by stub_core_run(0) - same as the recorder. */
   for (i = 1; i <= frames + 2 && !paused(); i++)
      frame(i);
   CHECK(paused(), "playback did not end");
   CHECK(n_pause == 1, "paused %u times, want 1", n_pause);
   CHECK(msgs_ended == 1, "playback-ended messages: %u", msgs_ended);
   CHECK(!(input_st.bsv_movie_state.flags
            & (BSV_FLAG_MOVIE_PLAYBACK | BSV_FLAG_MOVIE_END)),
         "flags after end: %x", input_st.bsv_movie_state.flags);
   CHECK(!input_st.bsv_movie_state_handle, "handle left after end");

   /* And the user regains control: unpause, run, nothing re-pauses. */
   unpause();
   for (i = 0; i < 5; i++)
      frame(2000 + i);
   CHECK(!paused(), "re-paused after playback ended");
   close_content();
   CHECK(input_st.bsv_movie_state.flags == 0, "flags after close: %x",
         input_st.bsv_movie_state.flags);
   lane_done("roundtrip", compress ? "zstd" : "raw");
}

static void lane_truncated(const char *path)
{
   uint8_t start[STATE_SIZE];
   unsigned i;
   int64_t size;

   reset_counters();
   record(path, 40, true, start);
   size = path_get_size(path);
   CHECK(size > 64, "replay size %lld", (long long)size);
   /* Cut inside the first checkpoint's compressed data: past the
    * header, the compression/encoding bytes and the three size words,
    * so the reader gets a declared size it cannot fill.  That is the
    * branch that raised MOVIE_END (bsvmovie.c "Truncated checkpoint,
    * terminating movie"). */
   CHECK(size > REPLAY_HEADER_LEN_BYTES + 2 + 12 + 64, "replay too small to cut");
   truncate_file(path, REPLAY_HEADER_LEN_BYTES + 2 + 12 + 16);

   play(path);
   CHECK(msgs_failed_load == 1, "failed-to-load messages: %u", msgs_failed_load);
   CHECK(!(input_st.bsv_movie_state.flags & BSV_FLAG_MOVIE_END),
         "MOVIE_END left set by failed playback init");
   CHECK(!(input_st.bsv_movie_state.flags & BSV_FLAG_MOVIE_PLAYBACK),
         "PLAYBACK set without a handle");
   CHECK(!input_st.bsv_movie_state_handle && !input_st.bsv_movie_state_next_handle,
         "handle installed for a truncated replay");

   for (i = 0; i < 10; i++)
      frame(i);
   CHECK(!paused(), "paused after failed playback start");
   CHECK(n_pause == 0, "paused %u times", n_pause);

   close_content();
   for (i = 0; i < 10; i++)
      frame(100 + i);
   CHECK(!paused(), "pause carried across Close Content");
   CHECK(input_st.bsv_movie_state.flags == 0, "flags after close: %x",
         input_st.bsv_movie_state.flags);
   lane_done("truncated", NULL);
}

static void lane_empty(const char *path)
{
   uint8_t start[STATE_SIZE];
   unsigned i;

   reset_counters();
   /* Record and halt before a single frame runs: the recorder writes
    * header + checkpoint and nothing else.  That is a valid replay of
    * zero frames. */
   record(path, 0, true, start);

   CHECK(play(path), "play empty");
   CHECK(msgs_failed_load == 0, "empty replay reported as failed to load");
   CHECK(input_st.bsv_movie_state.flags & BSV_FLAG_MOVIE_PLAYBACK,
         "playback flag not set for empty replay");

   for (i = 0; i < 5 && !paused(); i++)
      frame(i);
   CHECK(paused(), "empty replay did not end");
   CHECK(n_pause == 1, "paused %u times, want 1", n_pause);
   CHECK(msgs_ended == 1, "playback-ended messages: %u", msgs_ended);
   CHECK(!(input_st.bsv_movie_state.flags
            & (BSV_FLAG_MOVIE_PLAYBACK | BSV_FLAG_MOVIE_END)),
         "flags after empty end: %x", input_st.bsv_movie_state.flags);

   unpause();
   for (i = 0; i < 5; i++)
      frame(100 + i);
   CHECK(!paused(), "re-paused after empty replay ended");
   close_content();
   lane_done("empty", NULL);
}

static void lane_stray(void)
{
   unsigned i;
   reset_counters();
   input_st.bsv_movie_state.flags = BSV_FLAG_MOVIE_END;
   close_content();
   CHECK(input_st.bsv_movie_state.flags == 0,
         "movie_stop left stray MOVIE_END: %x", input_st.bsv_movie_state.flags);
   for (i = 0; i < 5; i++)
      frame(i);
   CHECK(!paused(), "stray MOVIE_END paused the runloop");
   lane_done("stray", NULL);
}

int main(int argc, char **argv)
{
   const char *path = "bsv_replay_init_test.replay";
   (void)argc; (void)argv;

   task_queue_init(false, NULL);
   settings.uints.replay_checkpoint_interval   = 0;
   settings.bools.replay_checkpoint_deserialize = true;

   lane_roundtrip(path, false);
   lane_roundtrip(path, true);
   lane_truncated(path);
   lane_empty(path);
   lane_stray();

   filestream_delete(path);
   task_queue_deinit();

   if (n_fail)
   {
      fprintf(stderr, "%u failure(s)\n", n_fail);
      return 1;
   }
   puts("bsv_replay_init_test: all lanes pass");
   return 0;
}
