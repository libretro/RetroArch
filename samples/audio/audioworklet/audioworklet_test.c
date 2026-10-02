/* audio/drivers/audioworklet.c's write, against its own ring.
 *
 * write() returns the bytes it took - the frontend's threaded pipeline
 * advances its pending pointer by that much, and the inline path writes
 * the rest from there - so a count in frames had seven eighths of every
 * write (float stereo, eight bytes a frame) sent again. And a write that
 * gives frames up says so once, as a total when the driver is freed:
 * a line from the write is a line a frame, on the audio path.
 *
 * The driver is this translation unit, built against the tree's
 * emscripten stand-ins; the harness makes its state - a ring of a known
 * size, a running context - and calls the write directly. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "../../../audio/drivers/audioworklet.c"

static unsigned failures = 0;
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

/* The frontend's logging, counted: the write path is to say nothing. */
void RARCH_LOG(const char *fmt, ...)  { (void)fmt; log_lines++; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; log_lines++; }
void RARCH_ERR(const char *fmt, ...)  { (void)fmt; log_lines++; }

/* The emscripten surface the rest of the driver refers to. The write
 * path reaches none of it. */
EMSCRIPTEN_WEBAUDIO_T emscripten_create_audio_context(const EmscriptenWebAudioCreateAttributes *o) { (void)o; return 1; }
void emscripten_start_wasm_audio_worklet_thread_async(EMSCRIPTEN_WEBAUDIO_T c, void *s, uint32_t n, EmscriptenStartWebAudioWorkletCallback cb, void *u) { (void)c; (void)s; (void)n; (void)cb; (void)u; }
void emscripten_create_wasm_audio_worklet_processor_async(EMSCRIPTEN_WEBAUDIO_T c, const WebAudioWorkletProcessorCreateOptions *o, EmscriptenWorkletProcessorCreatedCallback cb, void *u) { (void)c; (void)o; (void)cb; (void)u; }
int emscripten_audio_context_quantum_size(EMSCRIPTEN_WEBAUDIO_T c) { (void)c; return 128; }
EMSCRIPTEN_WEBAUDIO_T emscripten_create_wasm_audio_worklet_node(EMSCRIPTEN_WEBAUDIO_T c, const char *n, const EmscriptenAudioWorkletNodeCreateOptions *o, EmscriptenWorkletNodeProcessCallback cb, void *u) { (void)c; (void)n; (void)o; (void)cb; (void)u; return 2; }
void emscripten_audio_node_connect(EMSCRIPTEN_WEBAUDIO_T s, EMSCRIPTEN_WEBAUDIO_T d, int o, int i) { (void)s; (void)d; (void)o; (void)i; }
void emscripten_lock_init(emscripten_lock_t *lock) { *lock = 0; }
bool emscripten_lock_busyspin_wait_acquire(emscripten_lock_t *lock, double ms) { (void)ms; *lock = 1; return true; }
void emscripten_lock_release(emscripten_lock_t *lock) { *lock = 0; }
void platform_emscripten_run_on_browser_thread_sync(void (*func)(void*), void *arg) { func(arg); }

#define RING_BYTES 4096u
#define FRAME      (2 * sizeof(float))

static float samples[4096];

static void setup(audioworklet_data_t *aw, bool nonblock)
{
   memset(aw, 0, sizeof(*aw));
   aw->ring_init       = retro_spsc_init(&aw->ring, RING_BYTES);
   aw->ring_size       = RING_BYTES;
   aw->driver_running  = true;
   aw->context_running = true;
   aw->nonblock        = nonblock;
   retro_atomic_size_init(&aw->dropped, 0);
}

int main(void)
{
   audioworklet_data_t aw;
   ssize_t w;
   size_t  queued;

   /* Non-blocking, room enough: every byte taken, and said in bytes. */
   setup(&aw, true);
   w = audioworklet_write(&aw, samples, 1000 * FRAME / 8);   /* 125 frames */
   queued = retro_spsc_read_avail(&aw.ring);
   CHECK(w == (ssize_t)(125 * FRAME), "the write returns the bytes it took", w, 125 * FRAME);
   CHECK((size_t)w == queued, "what it says it took is what the ring holds", w, queued);

   /* More than the room: the room, in bytes, and nothing else. */
   w = audioworklet_write(&aw, samples, sizeof(samples));
   queued = retro_spsc_read_avail(&aw.ring);
   CHECK(queued == RING_BYTES, "the ring is filled to its size", queued, RING_BYTES);
   CHECK((size_t)w == RING_BYTES - 125 * FRAME, "a short write returns the bytes it took",
         w, RING_BYTES - 125 * FRAME);
   CHECK(audioworklet_write_avail(&aw) == 0, "no room after the ring is full",
         audioworklet_write_avail(&aw), 0);
   retro_spsc_free(&aw.ring);

   /* Blocking, and nothing draining the ring: the write gives up after
    * its bound, returns what went, and counts the rest - no line. */
   setup(&aw, false);
   log_lines = 0;
   w = audioworklet_write(&aw, samples, sizeof(samples));
   CHECK(w == (ssize_t)RING_BYTES, "a blocking write that cannot finish returns the bytes it took",
         w, RING_BYTES);
   CHECK(retro_atomic_load_acquire_size(&aw.dropped) == (sizeof(samples) - RING_BYTES) / FRAME,
         "the frames given up are counted",
         retro_atomic_load_acquire_size(&aw.dropped), (sizeof(samples) - RING_BYTES) / FRAME);
   CHECK(log_lines == 0, "the write path logs nothing", log_lines, 0);
   retro_spsc_free(&aw.ring);

   if (failures)
   {
      printf("[fail] audioworklet_test: %u check(s) failed\n", failures);
      return 1;
   }
   printf("[pass] audioworklet_test\n");
   return 0;
}
