/* audio/drivers/pipewire.c's write, against its own ring.
 *
 * A write longer than the ring's high-water mark used to log an error and
 * take nothing, on every call: at a low latency setting a frame of audio
 * is longer than the mark, so the stream went silent and the log filled
 * at the frame rate. A write now takes what the ring has room for, in
 * whole frames, and blocking waits for the rest in parts. The process
 * callbacks, on the graph's thread, count a cycle with no buffer to
 * take rather than logging it.
 *
 * The driver is this translation unit, built against the system's
 * PipeWire headers; the pw_* calls are stand-ins, and the loop wait is
 * the graph: each wait consumes a quantum from the ring. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "../../../audio/drivers/pipewire.c"

static unsigned failures  = 0;
static unsigned log_lines = 0;

#define CHECK(cond, msg, a, b) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: %s (%ld, %ld)\n", __FILE__, __LINE__, \
               msg, (long)(a), (long)(b)); \
         failures++; \
      } \
   } while (0)

/* The audio the graph takes a wait, and the stream it plays from. */
static pipewire_audio_t *g_audio;
static uint32_t          g_quantum = 1024 * 8;   /* 1024 float stereo frames */
static unsigned          g_waits;

/* The frontend's logging, counted: the write path is to say nothing. */
void RARCH_LOG(const char *fmt, ...)  { (void)fmt; log_lines++; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; log_lines++; }
void RARCH_ERR(const char *fmt, ...)  { (void)fmt; log_lines++; }
void RARCH_DBG(const char *fmt, ...)  { (void)fmt; }

/* The graph: a wait plays one quantum out of the ring. */
bool pipewire_loop_wait_ms(struct pw_thread_loop *loop, unsigned ms)
{
   uint32_t idx;
   int32_t  avail;
   (void)loop;
   (void)ms;
   g_waits++;
   avail = spa_ringbuffer_get_read_index(&g_audio->ring, &idx);
   if (avail > 0)
   {
      uint32_t n = (uint32_t)avail < g_quantum ? (uint32_t)avail : g_quantum;
      spa_ringbuffer_read_update(&g_audio->ring, idx + n);
   }
   return true;
}

enum pw_stream_state pw_stream_get_state(struct pw_stream *stream, const char **error)
{
   (void)stream;
   if (error)
      *error = NULL;
   return PW_STREAM_STATE_STREAMING;
}
void pw_thread_loop_lock(struct pw_thread_loop *loop)   { (void)loop; }
void pw_thread_loop_unlock(struct pw_thread_loop *loop) { (void)loop; }
void pw_thread_loop_signal(struct pw_thread_loop *loop, bool wait) { (void)loop; (void)wait; }

/* The rest of the surface the driver refers to; the write path reaches
 * none of it. */
settings_t *config_get_ptr(void) { static settings_t s; return &s; }
uint32_t audio_driver_requested_layout(void) { return 0x3; }
void audio_driver_set_device_latency(size_t frames) { (void)frames; }
unsigned audio_layout_channels(uint32_t layout) { (void)layout; return 2; }
bool pipewire_core_init(pipewire_core_t **pw, const char *n, const struct pw_registry_events *e) { (void)pw; (void)n; (void)e; return false; }
void pipewire_core_deinit(pipewire_core_t *pw) { (void)pw; }
bool pipewire_core_wait_resync(pipewire_core_t *pw) { (void)pw; return true; }
bool pipewire_stream_set_active(struct pw_thread_loop *l, struct pw_stream *s, bool a) { (void)l; (void)s; (void)a; return true; }
struct pw_properties *pw_properties_new(const char *key, ...) { (void)key; return NULL; }
int pw_properties_set(struct pw_properties *p, const char *k, const char *v) { (void)p; (void)k; (void)v; return 0; }
int pw_properties_setf(struct pw_properties *p, const char *k, const char *f, ...) { (void)p; (void)k; (void)f; return 0; }
void pw_stream_add_listener(struct pw_stream *s, struct spa_hook *l, const struct pw_stream_events *e, void *d) { (void)s; (void)l; (void)e; (void)d; }
int pw_stream_connect(struct pw_stream *s, enum spa_direction d, uint32_t t, enum pw_stream_flags f, const struct spa_pod **p, uint32_t n) { (void)s; (void)d; (void)t; (void)f; (void)p; (void)n; return -1; }
struct pw_buffer *pw_stream_dequeue_buffer(struct pw_stream *s) { (void)s; return NULL; }
void pw_stream_destroy(struct pw_stream *s) { (void)s; }
int pw_stream_get_time_n(struct pw_stream *s, struct pw_time *t, size_t z) { (void)s; (void)t; (void)z; return -1; }
struct pw_stream *pw_stream_new(struct pw_core *c, const char *n, struct pw_properties *p) { (void)c; (void)n; (void)p; return NULL; }
int pw_stream_queue_buffer(struct pw_stream *s, struct pw_buffer *b) { (void)s; (void)b; return 0; }
const char *pw_stream_state_as_string(enum pw_stream_state st) { (void)st; return ""; }

#define FRAME     8u            /* float stereo */
#define HIGHWATER (768u * FRAME) /* 16 ms at 48 kHz */

static float samples[2 * 8192];

int main(void)
{
   pipewire_core_t core;
   ssize_t  w;
   uint32_t idx;
   int32_t  filled;

   memset(&core, 0, sizeof(core));
   g_audio = (pipewire_audio_t*)calloc(1, sizeof(*g_audio));
   if (!g_audio)
      return 1;
   g_audio->pw             = &core;
   g_audio->frame_size     = FRAME;
   g_audio->highwater_mark = HIGHWATER;
   spa_ringbuffer_init(&g_audio->ring);

   /* Non-blocking, a frame of 60 Hz audio - longer than the mark: what
    * fits goes in, said in bytes, and nothing is logged. */
   core.nonblock = true;
   w      = pwire_write(g_audio, samples, 800 * FRAME);
   filled = spa_ringbuffer_get_write_index(&g_audio->ring, &idx);
   CHECK(w == (ssize_t)HIGHWATER, "a write past the mark takes the mark", w, HIGHWATER);
   CHECK(filled == (int32_t)HIGHWATER, "the ring holds what the write took", filled, HIGHWATER);
   CHECK(log_lines == 0, "the write path logs nothing", log_lines, 0);

   /* Full: a non-blocking write takes nothing, and says so. */
   w = pwire_write(g_audio, samples, 64 * FRAME);
   CHECK(w == 0, "a full ring takes nothing", w, 0);

   /* Blocking, four times the mark: it goes in parts as the graph
    * plays quanta out, all of it. */
   core.nonblock = false;
   g_waits       = 0;
   w = pwire_write(g_audio, samples, 4 * HIGHWATER);
   CHECK(w == (ssize_t)(4 * HIGHWATER), "a blocking write past the mark takes it all",
         w, 4 * HIGHWATER);
   CHECK(g_waits > 0, "and waited on the graph for it", g_waits, 1);
   CHECK(log_lines == 0, "the write path logs nothing", log_lines, 0);

   /* Whole frames only. */
   core.nonblock = true;
   pipewire_loop_wait_ms(NULL, 0);
   w = pwire_write(g_audio, samples, 1027);
   CHECK(w % FRAME == 0, "a write takes whole frames", w, 1024);

   /* The capture callback with no buffer to take: counted, not logged
    * - it runs on the graph's thread. */
   {
      pipewire_microphone_t *mic = (pipewire_microphone_t*)calloc(1, sizeof(*mic));
      if (mic)
      {
         mic->pw   = &core;
         log_lines = 0;
         pwire_capture_process_cb(mic);
         CHECK(retro_atomic_load_acquire_size(&mic->xruns) == 1,
               "a capture cycle with no buffer is counted",
               retro_atomic_load_acquire_size(&mic->xruns), 1);
         CHECK(log_lines == 0, "the capture callback logs nothing", log_lines, 0);
         free(mic);
      }
   }

   /* The playback callback with no buffer to take: counted too. */
   log_lines = 0;
   pwire_playback_process_cb(g_audio);
   CHECK(retro_atomic_load_acquire_size(&g_audio->xruns) == 1,
         "a playback cycle with no buffer is counted",
         retro_atomic_load_acquire_size(&g_audio->xruns), 1);
   CHECK(log_lines == 0, "the playback callback logs nothing", log_lines, 0);

   free(g_audio);

   if (failures)
   {
      printf("[fail] pipewire_write_test: %u check(s) failed\n", failures);
      return 1;
   }
   printf("[pass] pipewire_write_test\n");
   return 0;
}
