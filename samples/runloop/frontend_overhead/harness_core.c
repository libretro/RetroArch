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

/* The run in which pad 1's B was first seen pressed, then released,
 * since the last harness_core_input_measure() (-1: not yet). */
static long in_press_run  = -1;
static long in_release_run = -1;
static int  in_b_last;

/* Set while the core is inside input_poll / input_state, so the
 * harness can count heap calls in those alone. */
volatile int harness_core_in_input;

/* A probe the harness may set. While measuring, retro_run() calls it at
 * four points - 0 on entry, 1 after input_poll, 2 after the frame's
 * first input_state, 3 before it returns - so the harness can tell
 * where in the frame the frontend polled. Kept out of the timings. */
static void (*in_probe)(int point);
void harness_core_set_probe(void (*probe)(int point)) { in_probe = probe; }

/* Trace: what the core sees of pad 1 each frame. @mode 1 reads the
 * sixteen buttons one by one, the way older cores do; 2 reads them as
 * one bitmask; 0 stops. With @analog the two sticks are read too. */
static int      trace_mode;
static int      trace_analog;
static unsigned trace_buttons;
static int      trace_axes[4];

void harness_core_trace(int mode, int analog)
{
   trace_mode   = mode;
   trace_analog = analog;
}

/* What the last frame saw. */
void harness_core_trace_last(unsigned *buttons, int *axes)
{
   *buttons = trace_buttons;
   memcpy(axes, trace_axes, sizeof(trace_axes));
}

/* Rumble: @calls set_rumble_state calls a frame on port 0's strong
 * motor, the last with strength @calls * 100, and one on the weak
 * motor with strength 50. 0 stops. */
static struct retro_rumble_interface rumble;
static unsigned      rumble_calls;
static unsigned long rumble_made;
static unsigned long rumble_true;

void harness_core_rumble(unsigned calls)
{
   rumble_calls = calls;
   rumble_made  = 0;
   rumble_true  = 0;
}

/* The calls made since harness_core_rumble(), and how many of them the
 * frontend answered true. */
void harness_core_rumble_stats(unsigned long *made, unsigned long *answered_true)
{
   *made          = rumble_made;
   *answered_true = rumble_true;
}

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
   in_press_run   = -1;
   in_release_run = -1;
   in_b_last      = 0;
   in_poll_ns  = 0;
   in_first_ns = 0;
   in_state_ns = 0;
   in_frames   = 0;
}

/* The runs so far, and the runs in which pad 1's B changed since the
 * last harness_core_input_measure(). */
long harness_core_runs(void) { return (long)runs; }
void harness_core_input_edges(long *press_run, long *release_run)
{
   *press_run   = in_press_run;
   *release_run = in_release_run;
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
   /* two controller ports, so the frontend tells the core what is
    * plugged into each */
   static const struct retro_controller_description pads[] = {
      { "RetroPad", RETRO_DEVICE_JOYPAD }
   };
   static const struct retro_controller_info ports[] = {
      { pads, 1 }, { pads, 1 }, { NULL, 0 }
   };
   bool no_content = true;
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
   environ_cb = cb;
   cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);
   cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);
}
void retro_set_video_refresh(retro_video_refresh_t cb)      { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb)        { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb)            { poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb)          { state_cb = cb; }
void retro_init(void)
{
   if (environ_cb)
      environ_cb(RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE, &rumble);
}
void retro_deinit(void) { }

/* For the lane that closes content with a remap in use: the first
 * port's buttons are described, as a core describes them. */
void harness_core_describe(void)
{
   static const struct retro_input_descriptor desc[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,  "Jump" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,  "Fire" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,  "Strafe Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "Previous Weapon" },
      { 0, 0, 0, 0, NULL }
   };
   if (environ_cb)
      environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, (void*)desc);
}
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
/* The device the frontend last gave each port, how often it has given
 * one, and how often it did so from inside retro_run(). */
static unsigned port_device[8];
static long     port_device_calls, port_device_calls_in_run;
static int      in_run;

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   if (port < 8)
      port_device[port] = device;
   port_device_calls++;
   if (in_run)
      port_device_calls_in_run++;
}

void harness_core_port_device(unsigned port, unsigned *device,
      long *calls, long *calls_in_run)
{
   *device       = (port < 8) ? port_device[port] : 0;
   *calls        = port_device_calls;
   *calls_in_run = port_device_calls_in_run;
}
void retro_reset(void) { }

void retro_run(void)
{
   /* One pixel changes per frame so nothing upstream can treat the
    * frame as a duplicate. */
   frame[runs % (W * H)] ^= 0xffff;
   runs++;
   in_run = 1;
   if (trace_mode)
   {
      unsigned i;
      unsigned b = 0;
      poll_cb();
      if (trace_mode == 2)
         b = (unsigned)state_cb(0, RETRO_DEVICE_JOYPAD, 0,
               RETRO_DEVICE_ID_JOYPAD_MASK) & 0xffff;
      else
         for (i = 0; i < 16; i++)
            if (state_cb(0, RETRO_DEVICE_JOYPAD, 0, i))
               b |= 1u << i;
      trace_buttons = b;
      memset(trace_axes, 0, sizeof(trace_axes));
      if (trace_analog)
      {
         trace_axes[0] = state_cb(0, RETRO_DEVICE_ANALOG,
               RETRO_DEVICE_INDEX_ANALOG_LEFT,  RETRO_DEVICE_ID_ANALOG_X);
         trace_axes[1] = state_cb(0, RETRO_DEVICE_ANALOG,
               RETRO_DEVICE_INDEX_ANALOG_LEFT,  RETRO_DEVICE_ID_ANALOG_Y);
         trace_axes[2] = state_cb(0, RETRO_DEVICE_ANALOG,
               RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X);
         trace_axes[3] = state_cb(0, RETRO_DEVICE_ANALOG,
               RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);
      }
   }
   else if (!in_queries)
   {
      poll_cb();
      state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
   }
   else
   {
      unsigned q;
      int16_t  acc = 0;
      unsigned long long t0, t1, tf, t2;
      unsigned long long first_ns;
      if (in_probe)
         in_probe(0);
      harness_core_in_input = 1;
      t0 = now_ns();
      poll_cb();
      t1 = now_ns();
      if (in_probe)
      {
         unsigned long long t;
         in_probe(1);
         t   = now_ns();
         acc = state_cb(0, RETRO_DEVICE_JOYPAD, 0, 0);
         first_ns = now_ns() - t;
         in_probe(2);
         tf  = now_ns();
      }
      else
      {
         acc = state_cb(0, RETRO_DEVICE_JOYPAD, 0, 0);
         tf  = now_ns();
         first_ns = tf - t1;
      }
      /* pad 1's B (the first query): the run its edges arrive in */
      if (acc && !in_b_last && in_press_run < 0)
         in_press_run = (long)runs;
      if (!acc && in_b_last && in_release_run < 0)
         in_release_run = (long)runs;
      in_b_last = acc != 0;
      /* the way a core reads a pad: button by button, port by port */
      for (q = 1; q < in_queries; q++)
         acc |= state_cb(q % in_ports, RETRO_DEVICE_JOYPAD, 0,
               (q / in_ports) % 16);
      t2 = now_ns();
      harness_core_in_input = 0;
      in_sink      = acc;
      in_poll_ns  += t1 - t0;
      in_first_ns += first_ns;
      in_state_ns += t2 - tf;
      in_frames++;
      if (rumble_calls && rumble.set_rumble_state)
      {
         unsigned i;
         for (i = 1; i <= rumble_calls; i++)
         {
            rumble_made++;
            if (rumble.set_rumble_state(0, RETRO_RUMBLE_STRONG, (uint16_t)(i * 100)))
               rumble_true++;
         }
         rumble_made++;
         if (rumble.set_rumble_state(0, RETRO_RUMBLE_WEAK, 50))
            rumble_true++;
      }
      if (in_probe)
         in_probe(3);
   }
   video_cb(frame, W, H, W * 2);
   audio_cb(audio, AUDIO_FRAMES);
   in_run = 0;
}

void   harness_core_describe(void);
/* the buttons are described at load, as a core describes them */
bool   retro_load_game(const struct retro_game_info *game) { (void)game; harness_core_describe(); return true; }
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
