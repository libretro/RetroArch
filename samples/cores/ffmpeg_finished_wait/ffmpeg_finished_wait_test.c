/* The ffmpeg core's waits between its decode thread and the main
 * thread, driven directly.
 *
 * - The main thread, waiting for a finished video slot at the end of a
 *   clip, is let go when the decode thread ends.
 * - A decode thread with no open video slot, while the main thread
 *   waits for audio it can therefore never send, gets one: the main
 *   thread drops the finished frames.
 * - A decode thread with no room for its audio, while the main thread
 *   waits for more than is buffered, gets room: the main thread drops
 *   the buffered audio.
 * - A seek ends the decode thread's waits for a slot or for room, and
 *   the main thread's seek request returns once the decode thread has
 *   done it.
 * - The time of the last audio decoded reaches the main thread whole.
 * - Unloading stops a decode thread in either of its waits.
 *
 * The core is included rather than linked, for its statics: the video
 * buffer, the context and the waits are driven with no media involved.
 * A wait that never ends shows as the watchdog. */

#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#include "../../../cores/libretro-ffmpeg/ffmpeg_core.c"

static unsigned failures;
#define CHECK(cond, ...) \
   do { if (!(cond)) { printf("FAIL line %d: ", __LINE__); \
        printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

#define SLOTS     4
#define FIFO_CAP  4096

static const char *stage = "setup";

static void watchdog(int sig)
{
   (void)sig;
   printf("FAIL: hung in \"%s\"\n", stage);
   fflush(stdout);
   _exit(1);
}

static void sleep_ms(long ms)
{
   struct timespec d;
   d.tv_sec  = ms / 1000;
   d.tv_nsec = (ms % 1000) * 1000000L;
   nanosleep(&d, NULL);
}

static void test_log(enum retro_log_level level, const char *fmt, ...)
{
   (void)level;
   (void)fmt;
}

static bool test_environ(unsigned cmd, void *data)
{
   (void)cmd;
   (void)data;
   return false;
}

/* Both ends quiet between cases. */
static void reset(void)
{
   video_buffer_clear(VIDEO_BUFFER_STR);
   retro_spsc_clear(&AUDIO_DECODE_FIFO_STR);
   DECODE_THREAD_DEAD_SET(0);
   DO_SEEK_SET(0);
   retro_atomic_store_release_int(&g_ctx.audio_starved, 0);
   retro_atomic_store_release_int(&g_ctx.video_starved, 0);
   g_ctx.held_slot = NULL;
}

/* The decode thread's side of a full video buffer: every slot decoded. */
static void fill_video(void)
{
   unsigned i;
   for (i = 0; i < SLOTS; i++)
   {
      video_decoder_context_t *ctx = NULL;
      video_buffer_get_open_slot(VIDEO_BUFFER_STR, &ctx);
      if (ctx)
         video_buffer_finish_slot(VIDEO_BUFFER_STR, ctx);
   }
}

static void push_audio(uint8_t fill, size_t bytes)
{
   uint8_t buf[512];
   while (bytes)
   {
      size_t n = bytes < sizeof(buf) ? bytes : sizeof(buf);
      memset(buf, fill, n);
      retro_spsc_write(&AUDIO_DECODE_FIFO_STR, buf, n);
      bytes -= n;
   }
   retro_eventcount_notify(&g_ctx.main_ec);
}

/* ---- 1. the end of the decode thread ends a wait for a frame ---- */

static retro_atomic_int_t waited_out;
static bool               wait_result;

static void finished_waiter(void *unused)
{
   (void)unused;
   wait_result = video_buffer_wait_for_finished_slot(VIDEO_BUFFER_STR,
         &g_ctx.decode_thread_dead);
   retro_atomic_store_release_int(&waited_out, 1);
}

static void case_finished_wait(void)
{
   sthread_t *t;
   int i;

   stage = "finished wait";
   reset();
   retro_atomic_store_release_int(&waited_out, 0);
   t = sthread_create(finished_waiter, NULL);
   sleep_ms(50);
   CHECK(!retro_atomic_load_acquire_int(&waited_out),
         "the wait returned with the thread alive and no slot finished");

   decode_thread_mark_dead();
   for (i = 0; i < 1000 && !retro_atomic_load_acquire_int(&waited_out); i++)
      sleep_ms(1);
   CHECK(retro_atomic_load_acquire_int(&waited_out),
         "the wait for a finished slot outlived the decode thread");
   sthread_join(t);
   CHECK(!wait_result, "the wait reported a slot finished that was not");

   retro_atomic_store_release_int(&waited_out, 0);
   finished_waiter(NULL);
   CHECK(retro_atomic_load_acquire_int(&waited_out) && !wait_result,
         "a wait after the thread ended did not return at once");
}

/* ---- 2. no video slot while the main thread waits for audio ---- */

static bool decoder_result;

static void video_starved_decoder(void *unused)
{
   (void)unused;
   decoder_result = decode_video_frames_slot_wait();
   /* with a slot it gets on to the audio behind it */
   if (decoder_result)
      push_audio(0x55, 256);
}

static void case_video_starved(void)
{
   sthread_t *t;

   stage = "video starved";
   reset();
   fill_video();
   g_ctx.held_slot = video_buffer_hold_finished_slot(VIDEO_BUFFER_STR,
         &g_ctx.held_generation);
   decoder_result  = false;

   t = sthread_create(video_starved_decoder, NULL);
   main_wait_for_audio(256);
   sthread_join(t);

   CHECK(decoder_result, "the decode thread got no slot");
   CHECK(retro_spsc_read_avail(&AUDIO_DECODE_FIFO_STR) == 256,
         "the audio behind the frames did not arrive");
   CHECK(!g_ctx.held_slot, "the frame on screen was dropped but still held");
   CHECK(!retro_atomic_load_acquire_int(&g_ctx.video_starved),
         "the decode thread left its starved flag up");
   CHECK(VB_STATUS(VIDEO_BUFFER_STR, VB_TAIL(VIDEO_BUFFER_STR))
         != KB_FINISHED, "finished frames are still waiting at the tail");
}

/* ---- 3. no room for audio while the main thread waits for more ---- */

static retro_atomic_int_t decoder_stop;
static retro_atomic_int_t first_room;

static void audio_starved_decoder(void *unused)
{
   (void)unused;
   while (!retro_atomic_load_acquire_int(&decoder_stop))
   {
      if (!decode_wait_for_audio_room(200))
         break;
      retro_atomic_store_release_int(&first_room, 1);
      push_audio(0x55, 200);
   }
}

static void case_audio_starved(void)
{
   sthread_t *t;
   static uint8_t buf[FIFO_CAP];
   size_t  want = FIFO_CAP - 146;

   stage = "audio starved";
   reset();
   /* 196 bytes free: the decode thread's 200 do not fit, and the main
    * thread wants more than is there - though no more than 200-byte
    * writes can fill the ring to. */
   push_audio(0xAA, FIFO_CAP - 196);
   retro_atomic_store_release_int(&decoder_stop, 0);
   retro_atomic_store_release_int(&first_room, 0);

   t = sthread_create(audio_starved_decoder, NULL);
   main_wait_for_audio(want);
   retro_spsc_read(&AUDIO_DECODE_FIFO_STR, buf, want);

   retro_atomic_store_release_int(&decoder_stop, 1);
   DECODE_THREAD_DEAD_SET(1);
   retro_eventcount_notify(&g_ctx.decode_ec);
   sthread_join(t);

   CHECK(retro_atomic_load_acquire_int(&first_room),
         "the decode thread never got room");
   CHECK(buf[0] == 0x55 && buf[want - 1] == 0x55,
         "the audio read was not the audio decoded after the drop");
   CHECK(!retro_atomic_load_acquire_int(&g_ctx.audio_starved),
         "the decode thread left its starved flag up");
}

/* ---- 4. a seek ends the decode thread's waits ---- */

static int seek_wait_kind;   /* 0: room for audio, 1: a video slot */
static retro_atomic_int_t seek_wait_result;

static void seeking_decoder(void *unused)
{
   bool r;
   (void)unused;
   if (seek_wait_kind)
      r = decode_video_frames_slot_wait();
   else
      r = decode_wait_for_audio_room(FIFO_CAP - 50);
   retro_atomic_store_release_int(&seek_wait_result, r ? 1 : 2);

   /* the top of the decode loop: do the seek */
   while (!DO_SEEK_STR)
      sleep_ms(1);
   video_buffer_clear(VIDEO_BUFFER_STR);
   retro_spsc_clear(&AUDIO_DECODE_FIFO_STR);
   DO_SEEK_SET(0);
   retro_eventcount_notify(&g_ctx.main_ec);
}

static void case_seek(int kind)
{
   sthread_t *t;

   stage = kind ? "seek over a video wait" : "seek over an audio wait";
   reset();
   if (kind)
      fill_video();
   else
      push_audio(0xAA, FIFO_CAP - 100);
   seek_wait_kind = kind;
   retro_atomic_store_release_int(&seek_wait_result, 0);

   t = sthread_create(seeking_decoder, NULL);
   sleep_ms(30);
   CHECK(!retro_atomic_load_acquire_int(&seek_wait_result),
         "the decode thread's wait (%d) ended with nothing to end it", kind);

   g_ctx.decoded_frame_cnt = 300;
   seek_frame(30);
   sthread_join(t);

   CHECK(!DO_SEEK_STR, "seek_frame() returned with the seek not done");
   if (kind)
      CHECK(retro_atomic_load_acquire_int(&seek_wait_result) == 2,
            "a video slot wait went on through a seek request");
   else
      CHECK(retro_atomic_load_acquire_int(&seek_wait_result) != 0,
            "an audio room wait went on through a seek request");
}

/* ---- 5. the audio time arrives whole ---- */

#define TIME_WRITES 200000u
#define TIME_HI(i)  (0x40000000u | (((i) * 2654435761u) >> 12))

static void time_writer(void *unused)
{
   uint32_t i;
   (void)unused;
   for (i = 1; i <= TIME_WRITES; i++)
   {
      /* both halves carry i, so a torn read shows */
      uint32_t w[2];
      double   t;
      w[0] = i;
      w[1] = TIME_HI(i);
      memcpy(&t, w, sizeof(t));
      decode_audio_time_set(t);
   }
}

static void case_audio_time(void)
{
   sthread_t *t;
   unsigned   torn = 0, back = 0, reads = 0;
   uint32_t   last = 0;

   stage = "audio time";
   decode_audio_time_set(0.0);
   t = sthread_create(time_writer, NULL);
   for (;;)
   {
      uint32_t w[2];
      double   v = decode_audio_time();
      memcpy(w, &v, sizeof(w));
      reads++;
      if (!w[0] && !w[1])
         continue;
      if (w[1] != TIME_HI(w[0]))
      {
         torn++;
         continue;
      }
      if (w[0] < last)
         back++;
      last = w[0];
      if (w[0] == TIME_WRITES)
         break;
   }
   sthread_join(t);
   CHECK(!torn, "%u of %u reads of the audio time were torn", torn, reads);
   CHECK(!back, "%u reads of the audio time went backwards", back);
}

/* ---- 6. unloading stops a decode thread in its waits ---- */

static void unloading_decoder(void *unused)
{
   (void)unused;
   if (seek_wait_kind)
      decode_video_frames_slot_wait();
   else
      decode_wait_for_audio_room(FIFO_CAP - 50);
}

static void case_unload(int kind)
{
   stage = kind ? "unload over a video wait" : "unload over an audio wait";
   reset();
   if (kind)
      fill_video();
   else
      push_audio(0xAA, FIFO_CAP - 100);
   seek_wait_kind           = kind;
   DECODE_THREAD_HANDLE_STR = sthread_create(unloading_decoder, NULL);
   sleep_ms(30);
   retro_unload_game();
}

static bool fixture(void)
{
   log_cb                    = test_log;
   CORE_PREFIX(environ_cb)   = test_environ;
   MEDIA_STR.interpolate_fps = 30.0;
   MEDIA_STR.duration.time   = 100.0;
   MEDIA_STR.sample_rate     = 48000;
   if (!retro_eventcount_init(&g_ctx.main_ec))
      return false;
   if (!retro_eventcount_init(&g_ctx.decode_ec))
      return false;
   g_ctx.ecs_inited = true;
   g_ctx.audio_decode_fifo_init = retro_spsc_init(&AUDIO_DECODE_FIFO_STR,
         FIFO_CAP);
   if (!VIDEO_BUFFER_STR)
      VIDEO_BUFFER_STR = video_buffer_create(SLOTS, 64 * 64 * 4, 64, 64);
   return g_ctx.audio_decode_fifo_init && VIDEO_BUFFER_STR
      && AUDIO_DECODE_FIFO_STR.capacity == FIFO_CAP;
}

int main(void)
{
   signal(SIGALRM, watchdog);
   alarm(60);

   if (!fixture())
   {
      printf("FAIL: fixture\n");
      return 1;
   }

   case_finished_wait();
   case_video_starved();
   case_audio_starved();
   case_seek(0);
   case_seek(1);
   case_audio_time();
   case_unload(0);
   /* unload freed the eventcounts and the ring */
   if (!fixture())
   {
      printf("FAIL: fixture\n");
      return 1;
   }
   case_unload(1);
   alarm(0);

   video_buffer_destroy(VIDEO_BUFFER_STR);

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("[pass] ffmpeg_finished_wait_test\n");
   return 0;
}
