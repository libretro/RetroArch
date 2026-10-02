/* The core for the frontend-overhead harness: the cheapest thing a
 * real core does every frame, and nothing else. One 320x240 RGB565
 * frame, one 735-frame stereo audio batch (NTSC at 44.1 kHz), one
 * input poll, one input read. Every other cost in the run is the
 * frontend's.
 *
 * For the input lanes the harness switches it to measuring: each frame
 * it times its own input_poll and a set number of input_state queries
 * spread over the joypad buttons of the ports it is told to read - the
 * frontend's input cost as a core sees it, from the core's side of the
 * libretro calls. */
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <libretro.h>

#define W 320
#define H 240
#define AUDIO_FRAMES 735

static retro_video_refresh_t       video_cb;
static retro_audio_sample_batch_t  audio_cb;
static retro_input_poll_t          poll_cb;
static retro_input_state_t         state_cb;
static retro_environment_t         environ_cb;
static uint16_t frame[W * H];
static int16_t  audio[AUDIO_FRAMES * 2];
static unsigned runs;

/* input measurement, driven by harness_core_input_measure() */
static unsigned           in_queries;     /* 0: the one read of normal mode */
static unsigned           in_ports = 1;
static unsigned long long in_poll_ns;
static unsigned long long in_first_ns;    /* the frame's first query */
static unsigned long long in_state_ns;    /* the rest */
static unsigned long      in_frames;
static volatile int16_t   in_sink;

/* Set while the core is inside input_poll / input_state, so the
 * harness can count heap calls in those alone. */
volatile int harness_core_in_input;

static unsigned long long now_ns(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return (unsigned long long)ts.tv_sec * 1000000000ULL + (unsigned long long)ts.tv_nsec;
}

/* Start measuring: @queries input_state calls a frame over @ports
 * ports (0 queries goes back to the normal single read), counters
 * cleared. */
void harness_core_input_measure(unsigned queries, unsigned ports)
{
   in_queries  = queries;
   in_ports    = ports ? ports : 1;
   in_poll_ns  = 0;
   in_first_ns = 0;
   in_state_ns = 0;
   in_frames   = 0;
}

/* What was measured since: frames, and nanoseconds in input_poll, in
 * each frame's first input_state call (which carries the poll when the
 * frontend polls late, its default), and in the other calls. */
void harness_core_input_stats(unsigned long *frames,
      unsigned long long *poll_ns, unsigned long long *first_ns,
      unsigned long long *state_ns)
{
   *frames   = in_frames;
   *poll_ns  = in_poll_ns;
   *first_ns = in_first_ns;
   *state_ns = in_state_ns;
}

void retro_set_environment(retro_environment_t cb)
{
   bool no_content = true;
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
   environ_cb = cb;
   cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);
}
void retro_set_video_refresh(retro_video_refresh_t cb)      { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb)        { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb)            { poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb)          { state_cb = cb; }
void retro_init(void) { }
void retro_deinit(void) { }
unsigned retro_api_version(void) { return RETRO_API_VERSION; }
void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "frontend_overhead_harness";
   info->library_version  = "1";
   info->valid_extensions = "";
}
void retro_get_system_av_info(struct retro_system_av_info *info)
{
   memset(info, 0, sizeof(*info));
   info->timing.fps            = 60.0;
   info->timing.sample_rate    = 44100.0;
   info->geometry.base_width   = W;
   info->geometry.base_height  = H;
   info->geometry.max_width    = W;
   info->geometry.max_height   = H;
   info->geometry.aspect_ratio = 4.0f / 3.0f;
}
void retro_set_controller_port_device(unsigned port, unsigned device) { (void)port; (void)device; }
void retro_reset(void) { }

void retro_run(void)
{
   /* One pixel changes per frame so nothing upstream can treat the
    * frame as a duplicate. */
   frame[runs % (W * H)] ^= 0xffff;
   runs++;
   if (!in_queries)
   {
      poll_cb();
      state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
   }
   else
   {
      unsigned q;
      int16_t  acc = 0;
      unsigned long long t0, t1, tf, t2;
      harness_core_in_input = 1;
      t0 = now_ns();
      poll_cb();
      t1 = now_ns();
      acc = state_cb(0, RETRO_DEVICE_JOYPAD, 0, 0);
      tf = now_ns();
      /* the way a core reads a pad: button by button, port by port */
      for (q = 1; q < in_queries; q++)
         acc |= state_cb(q % in_ports, RETRO_DEVICE_JOYPAD, 0,
               (q / in_ports) % 16);
      t2 = now_ns();
      harness_core_in_input = 0;
      in_sink      = acc;
      in_poll_ns  += t1 - t0;
      in_first_ns += tf - t1;
      in_state_ns += t2 - tf;
      in_frames++;
   }
   video_cb(frame, W, H, W * 2);
   audio_cb(audio, AUDIO_FRAMES);
}

bool   retro_load_game(const struct retro_game_info *game) { (void)game; return true; }
void   retro_unload_game(void) { }
unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
bool   retro_load_game_special(unsigned t, const struct retro_game_info *i, size_t n) { (void)t; (void)i; (void)n; return false; }
size_t retro_serialize_size(void) { return sizeof(frame); }
bool   retro_serialize(void *d, size_t n)   { if (n < sizeof(frame)) return false; memcpy(d, frame, sizeof(frame)); return true; }
bool   retro_unserialize(const void *d, size_t n) { if (n < sizeof(frame)) return false; memcpy(frame, d, sizeof(frame)); return true; }
void   retro_cheat_reset(void) { }
void   retro_cheat_set(unsigned i, bool e, const char *c) { (void)i; (void)e; (void)c; }
void  *retro_get_memory_data(unsigned id) { (void)id; return NULL; }
size_t retro_get_memory_size(unsigned id) { (void)id; return 0; }
