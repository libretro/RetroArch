/* A minimal OpenGL hardware core for egl_hw_core_test.py.
 *
 * It asks for a hardware context, and from then on does what any such
 * core does: reads the GL version in context_reset, and every frame
 * binds the frontend's framebuffer, clears it to one colour and hands
 * it over.  It links no GL library - every entry point comes from the
 * frontend's get_proc_address - and it does not crash when it is given
 * no context: it says so on stderr, which is what the test looks for.
 *
 * A real core is less polite.  Its first glGetString returns NULL and
 * it dereferences that, which is how this was found: on EGL the
 * threaded wrapper's ring bound the core's context to the window
 * surface the video thread already held, eglMakeCurrent refused, and
 * every hardware core on Android died in its renderer's init. */
#include <stdio.h>
#include <string.h>
#include <libretro.h>

#define W 64
#define H 48

#define CORE_GL_VERSION           0x1F02
#define CORE_GL_FRAMEBUFFER       0x8D40
#define CORE_GL_COLOR_BUFFER_BIT  0x00004000

typedef const unsigned char *(*core_get_string_t)(unsigned name);
typedef void (*core_bind_framebuffer_t)(unsigned target, unsigned fbo);
typedef void (*core_viewport_t)(int x, int y, int w, int h);
typedef void (*core_clear_color_t)(float r, float g, float b, float a);
typedef void (*core_clear_t)(unsigned mask);

static retro_video_refresh_t video_cb;
static retro_environment_t   environ_cb;
static struct retro_hw_render_callback hw;
static int have_context;

static core_bind_framebuffer_t core_bind_framebuffer;
static core_viewport_t         core_viewport;
static core_clear_color_t      core_clear_color;
static core_clear_t            core_clear;

static void context_reset(void)
{
   core_get_string_t get_string =
      (core_get_string_t)hw.get_proc_address("glGetString");
   const unsigned char *version = get_string
      ? get_string(CORE_GL_VERSION) : NULL;

   core_bind_framebuffer = (core_bind_framebuffer_t)
      hw.get_proc_address("glBindFramebuffer");
   core_viewport         = (core_viewport_t)
      hw.get_proc_address("glViewport");
   core_clear_color      = (core_clear_color_t)
      hw.get_proc_address("glClearColor");
   core_clear            = (core_clear_t)
      hw.get_proc_address("glClear");

   have_context = version && core_bind_framebuffer && core_viewport
      && core_clear_color && core_clear;
   if (have_context)
      fprintf(stderr, "[egl_hw_core] context_reset: GL_VERSION \"%s\"\n",
            (const char*)version);
   else
      fprintf(stderr, "[egl_hw_core] context_reset: NO CONTEXT\n");
   fflush(stderr);
}

static void context_destroy(void)
{
   have_context = 0;
}

void retro_set_environment(retro_environment_t cb)
{
   bool no_content = true;
   environ_cb = cb;
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);
}

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { (void)cb; }
void retro_set_input_poll(retro_input_poll_t cb) { (void)cb; }
void retro_set_input_state(retro_input_state_t cb) { (void)cb; }
void retro_init(void) { }
void retro_deinit(void) { }
unsigned retro_api_version(void) { return RETRO_API_VERSION; }

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "egl_hw_core";
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
   info->geometry.aspect_ratio = (float)W / (float)H;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   (void)port;
   (void)device;
}

void retro_reset(void) { }

void retro_run(void)
{
   if (!have_context)
   {
      video_cb(NULL, W, H, 0);
      return;
   }
   core_bind_framebuffer(CORE_GL_FRAMEBUFFER,
         (unsigned)hw.get_current_framebuffer());
   core_viewport(0, 0, W, H);
   /* 51, 153, 204 in a screenshot. */
   core_clear_color(0.2f, 0.6f, 0.8f, 1.0f);
   core_clear(CORE_GL_COLOR_BUFFER_BIT);
   video_cb(RETRO_HW_FRAME_BUFFER_VALID, W, H, 0);
}

bool retro_load_game(const struct retro_game_info *game)
{
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
   (void)game;
   environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
   memset(&hw, 0, sizeof(hw));
   hw.context_type    = RETRO_HW_CONTEXT_OPENGL;
   hw.context_reset   = context_reset;
   hw.context_destroy = context_destroy;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw))
   {
      fprintf(stderr, "[egl_hw_core] SET_HW_RENDER refused\n");
      return false;
   }
   return true;
}

bool retro_load_game_special(unsigned type,
      const struct retro_game_info *info, size_t num)
{
   (void)type;
   (void)info;
   (void)num;
   return false;
}

void retro_unload_game(void) { }
unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
size_t retro_serialize_size(void) { return 0; }
bool retro_serialize(void *data, size_t size) { (void)data; (void)size; return false; }
bool retro_unserialize(const void *data, size_t size) { (void)data; (void)size; return false; }
void retro_cheat_reset(void) { }
void retro_cheat_set(unsigned index, bool enabled, const char *code) { (void)index; (void)enabled; (void)code; }
void *retro_get_memory_data(unsigned id) { (void)id; return NULL; }
size_t retro_get_memory_size(unsigned id) { (void)id; return 0; }
