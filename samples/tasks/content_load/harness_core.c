/* Minimal libretro core for the content load harness: runs without
 * content, pushes a frame per retro_run, and counts its own
 * retro_init calls behind an export, so the harness can tell exactly
 * which frame the frontend brought it up on. */
#include <string.h>
#include <stdint.h>
#include <libretro.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

/* The thread the library was opened on (constructor) and the one
 * retro_init ran on: the harness checks the open happened on a worker,
 * not the frame-loop thread. */
static unsigned long harness_load_tid;
static unsigned long harness_init_tid;
static unsigned long harness_self_tid(void)
{
#ifdef _WIN32
   return (unsigned long)GetCurrentThreadId();
#else
   return (unsigned long)(uintptr_t)pthread_self();
#endif
}
RETRO_API unsigned long harness_core_load_tid(void) { return harness_load_tid; }
RETRO_API unsigned long harness_core_init_tid(void) { return harness_init_tid; }
#if defined(__GNUC__) || defined(__clang__)
__attribute__((constructor))
static void harness_on_load(void) { harness_load_tid = harness_self_tid(); }
#endif

#define W 320
#define H 240

static retro_video_refresh_t video_cb;
static retro_environment_t   environ_cb;
static uint16_t frame[W * H];
static unsigned runs;
static unsigned inits;
RETRO_API unsigned harness_core_runs(void) { return runs; }
static unsigned env_sets;
RETRO_API unsigned harness_core_env_sets(void) { return env_sets; }
static unsigned key_events;
RETRO_API unsigned harness_core_key_events(void) { return key_events; }

static void harness_key_event(bool down, unsigned keycode,
      uint32_t character, uint16_t key_modifiers)
{
   (void)down; (void)keycode; (void)character; (void)key_modifiers;
   key_events++;
}
static unsigned hw_resets;
static unsigned hw_destroys;

/* RETRO_API, like the core's own entry points: a Windows DLL exports
 * only what is marked, and the harness looks these up by name. */
RETRO_API unsigned harness_core_inits(void)       { return inits; }
RETRO_API unsigned harness_core_hw_resets(void)   { return hw_resets; }
RETRO_API unsigned harness_core_hw_destroys(void) { return hw_destroys; }

static void hw_context_reset(void)   { hw_resets++; }
static void hw_context_destroy(void) { hw_destroys++; }

/* Declared to the frontend, which keeps pointers to these rather than
 * copies: the harness checks whose it ends up holding. */
static const struct retro_controller_description port_types[] = {
   { "Harness pad", RETRO_DEVICE_JOYPAD }
};
static const struct retro_controller_info ports[] = {
   { port_types, 1 },
   { NULL, 0 }
};
RETRO_API retro_hw_context_reset_t harness_core_hw_destroy_fn(void)
{ return hw_context_destroy; }
RETRO_API const struct retro_controller_description *harness_core_port_types(void)
{ return port_types; }

/* Content of extension "fpath" is taken as a path.  The software
 * build declares no override, so the two cores answer differently. */
#ifndef HARNESS_CORE_NO_HW
static const struct retro_system_content_info_override overrides[] = {
   { "fpath", true, false },
   { NULL, false, false }
};
#endif

void retro_set_environment(retro_environment_t cb)
{
   bool no_content = true;
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
   env_sets++;
   environ_cb = cb;
   cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);
   cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);
   {
      struct retro_keyboard_callback kb;
      kb.callback = harness_key_event;
      cb(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &kb);
   }
#ifndef HARNESS_CORE_NO_HW
   cb(RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE, (void*)overrides);
#endif
}
void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { (void)cb; }
void retro_set_input_poll(retro_input_poll_t cb) { (void)cb; }
void retro_set_input_state(retro_input_state_t cb) { (void)cb; }
/* HARNESS_CORE_NO_INIT builds a library that opens and exports the
 * rest of the API but not retro_init: not a core, found out only once
 * the load has committed. */
#ifndef HARNESS_CORE_NO_INIT
void retro_init(void) { inits++; harness_init_tid = harness_self_tid(); }
#endif
void retro_deinit(void) { }
unsigned retro_api_version(void) { return RETRO_API_VERSION; }
void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "content_load_harness";
   info->library_version  = "1";
   info->valid_extensions = "";
}
void retro_get_system_av_info(struct retro_system_av_info *info)
{
   memset(info, 0, sizeof(*info));
   info->timing.fps            = 60.0;
   info->timing.sample_rate    = 48000.0;
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
   unsigned i;
   runs++;
   for (i = 0; i < W * H; i++)
      frame[i] = (uint16_t)(runs + i);
   video_cb(frame, W, H, W * 2);
}
/* A state large enough that the save task writes it over several
 * checks (task_save writes one chunk per tick at least, and a chunk
 * is 16 MiB on desktop), so a close can find the save in flight. */
#define STATE_SIZE (64u * 1024 * 1024)
size_t retro_serialize_size(void) { return STATE_SIZE; }
bool retro_serialize(void *data, size_t size)
{
   if (size < STATE_SIZE)
      return false;
   memset(data, 0x5a, STATE_SIZE);
   return true;
}
bool retro_unserialize(const void *data, size_t size) { (void)data; return size >= STATE_SIZE; }
void retro_cheat_reset(void) { }
void retro_cheat_set(unsigned index, bool enabled, const char *code) { (void)index; (void)enabled; (void)code; }
bool retro_load_game(const struct retro_game_info *game)
{
   /* A hardware-render request, as a hardware core makes at load: the
    * frontend must hold it until the drivers it builds for this core
    * reset the context, and must not destroy a context that was
    * never reset.  RETRO_HW_CONTEXT_NONE keeps the request off any
    * real driver so the lanes run under the null driver; the
    * callbacks are what the frontend holds and calls. */
   /* HARNESS_CORE_NO_HW builds a software core that makes no such
    * request. */
#ifndef HARNESS_CORE_NO_HW
   struct retro_hw_render_callback hw;
   memset(&hw, 0, sizeof(hw));
   hw.context_type    = RETRO_HW_CONTEXT_NONE;
   hw.context_reset   = hw_context_reset;
   hw.context_destroy = hw_context_destroy;
   environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw);
#endif
   (void)game;
   return true;
}
bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num) { (void)type; (void)info; (void)num; return false; }
void retro_unload_game(void) { }
unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
void *retro_get_memory_data(unsigned id) { (void)id; return NULL; }
size_t retro_get_memory_size(unsigned id) { (void)id; return 0; }
