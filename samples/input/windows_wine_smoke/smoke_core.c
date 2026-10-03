/* A core for the Windows raw input run under Wine: it shows what
 * reached it.
 *
 * It needs no content. Each frame it polls and reads port 1's B and A
 * and writes a line to the log when either changes; it registers a
 * keyboard callback and writes a line for every key event it is
 * given. On A it asks the frontend for another frame rate, which makes
 * the frontend restart its drivers - the restart a content load makes,
 * asked for from here because a test can press a key but cannot easily
 * drive the menu to load content.
 *
 * The frontend's default keyboard binds put B on Z and A on X. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* the thread retro_run() is on: a key event that comes on another has
 * been handed to the core behind its back */
#ifdef _WIN32
#include <windows.h>
static DWORD run_thread;
#define THREAD_NOW()      GetCurrentThreadId()
#define THREAD_SAME(a, b) ((a) == (b))
#else
#include <pthread.h>
static pthread_t run_thread;
#define THREAD_NOW()      pthread_self()
#define THREAD_SAME(a, b) pthread_equal((a), (b))
#endif
static int run_thread_known;

#include <libretro.h>

static retro_environment_t   environ_cb;
static retro_video_refresh_t video_cb;
static retro_input_poll_t    poll_cb;
static retro_input_state_t   state_cb;
static retro_log_printf_t    log_cb;

static uint32_t frame[320 * 240];
static int      was_b, was_a;
static double   fps = 60.0;

static void say(const char *fmt, ...)
{
   char buf[160];
   va_list ap;
   va_start(ap, fmt);
   vsnprintf(buf, sizeof(buf), fmt, ap);
   va_end(ap);
   if (log_cb)
      log_cb(RETRO_LOG_INFO, "[smoke core] %s\n", buf);
}

static void RETRO_CALLCONV on_key(bool down, unsigned keycode,
      uint32_t character, uint16_t mods)
{
   (void)character;
   say("key event: %s keycode %u mods %u", down ? "down" : "up",
         keycode, (unsigned)mods);
   if (run_thread_known && !THREAD_SAME(THREAD_NOW(), run_thread))
      say("key event on a thread that is not the core's");
}

void retro_set_environment(retro_environment_t cb)
{
   bool no_content = true;
   struct retro_log_callback log;
   environ_cb = cb;
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);
   if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
}

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { (void)cb; }
void retro_set_input_poll(retro_input_poll_t cb) { poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { state_cb = cb; }

void retro_init(void) { }
void retro_deinit(void) { }
unsigned retro_api_version(void) { return RETRO_API_VERSION; }

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "Smoke";
   info->library_version  = "1";
   info->valid_extensions = "";
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   memset(info, 0, sizeof(*info));
   info->geometry.base_width   = 320;
   info->geometry.base_height  = 240;
   info->geometry.max_width    = 320;
   info->geometry.max_height   = 240;
   info->geometry.aspect_ratio = 4.0f / 3.0f;
   info->timing.fps            = fps;
   info->timing.sample_rate    = 48000.0;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{ (void)port; (void)device; }
void retro_reset(void) { }

void retro_run(void)
{
   int b, a;

   run_thread       = THREAD_NOW();
   run_thread_known = 1;
   poll_cb();
   b = state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B) ? 1 : 0;
   a = state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A) ? 1 : 0;

   if (b != was_b)
      say("joypad B %s", b ? "pressed" : "released");
   if (a && !was_a)
   {
      struct retro_system_av_info info;
      /* another frame rate: the frontend restarts its drivers */
      fps = (fps == 60.0) ? 50.0 : 60.0;
      retro_get_system_av_info(&info);
      say("joypad A pressed: asking for %d fps", (int)fps);
      environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &info);
   }
   was_b = b;
   was_a = a;

   video_cb(frame, 320, 240, 320 * sizeof(uint32_t));
}

bool retro_load_game(const struct retro_game_info *game)
{
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
   struct retro_keyboard_callback kb;
   unsigned i;
   (void)game;
   environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
   kb.callback = on_key;
   environ_cb(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &kb);
   for (i = 0; i < 320 * 240; i++)
      frame[i] = 0x00203040;
   say("running");
   return true;
}

void retro_unload_game(void) { }
unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{ (void)type; (void)info; (void)num; return false; }
size_t retro_serialize_size(void) { return 0; }
bool retro_serialize(void *data, size_t len) { (void)data; (void)len; return false; }
bool retro_unserialize(const void *data, size_t len) { (void)data; (void)len; return false; }
void *retro_get_memory_data(unsigned id) { (void)id; return NULL; }
size_t retro_get_memory_size(unsigned id) { (void)id; return 0; }
void retro_cheat_reset(void) { }
void retro_cheat_set(unsigned index, bool enabled, const char *code)
{ (void)index; (void)enabled; (void)code; }
