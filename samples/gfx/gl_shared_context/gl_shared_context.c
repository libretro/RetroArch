/* An OpenGL hardware core for gl_shared_context_test.py that draws on a
 * thread of its own (RETRO_ENVIRONMENT_GET_HW_SHARED_CONTEXT_INTERFACE).
 *
 * In context_reset it makes a context in its own context's share group
 * and starts a thread on it, which makes a texture, clears it to one
 * colour through a framebuffer of its own, fences that and hands the
 * texture and the fence over. retro_run, on the core's context, waits on
 * the fence on the GPU and blits the texture into the frontend's
 * framebuffer - so the colour on screen is the other thread's drawing,
 * seen through the share group. Until then it clears to black.
 *
 * Every entry point comes from the frontend's get_proc_address; it links
 * no GL library. What happened goes to stderr for the test to read. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <libretro.h>
#include <retro_atomic.h>

#define W 64
#define H 48

#define CORE_GL_VERSION               0x1F02
#define CORE_GL_TEXTURE_2D            0x0DE1
#define CORE_GL_RGBA                  0x1908
#define CORE_GL_RGBA8                 0x8058
#define CORE_GL_UNSIGNED_BYTE         0x1401
#define CORE_GL_TEXTURE_MIN_FILTER    0x2801
#define CORE_GL_NEAREST               0x2600
#define CORE_GL_FRAMEBUFFER           0x8D40
#define CORE_GL_READ_FRAMEBUFFER      0x8CA8
#define CORE_GL_DRAW_FRAMEBUFFER      0x8CA9
#define CORE_GL_COLOR_ATTACHMENT0     0x8CE0
#define CORE_GL_FRAMEBUFFER_COMPLETE  0x8CD5
#define CORE_GL_COLOR_BUFFER_BIT      0x00004000
#define CORE_GL_SYNC_GPU_COMMANDS_COMPLETE 0x9117

typedef struct core_sync *core_sync_t;

static const unsigned char *(*p_GetString)(unsigned);
static void (*p_GenTextures)(int, unsigned*);
static void (*p_DeleteTextures)(int, const unsigned*);
static void (*p_BindTexture)(unsigned, unsigned);
static void (*p_TexImage2D)(unsigned, int, int, int, int, int, unsigned, unsigned, const void*);
static void (*p_TexParameteri)(unsigned, unsigned, int);
static void (*p_GenFramebuffers)(int, unsigned*);
static void (*p_DeleteFramebuffers)(int, const unsigned*);
static void (*p_BindFramebuffer)(unsigned, unsigned);
static void (*p_FramebufferTexture2D)(unsigned, unsigned, unsigned, unsigned, int);
static unsigned (*p_CheckFramebufferStatus)(unsigned);
static void (*p_Viewport)(int, int, int, int);
static void (*p_ClearColor)(float, float, float, float);
static void (*p_Clear)(unsigned);
static core_sync_t (*p_FenceSync)(unsigned, unsigned);
static void (*p_WaitSync)(core_sync_t, unsigned, unsigned long long);
static void (*p_DeleteSync)(core_sync_t);
static void (*p_Flush)(void);
static void (*p_BlitFramebuffer)(int, int, int, int, int, int, int, int, unsigned, unsigned);

static retro_video_refresh_t video_cb;
static retro_environment_t   environ_cb;
static struct retro_hw_render_callback hw;
static const struct retro_hw_shared_context_interface *shared_iface;
static struct retro_hw_shared_context *worker_ctx;
static pthread_t worker;
static int worker_started;
static int have_context;

/* The worker's texture and fence, published with the flag after them */
static unsigned    worker_texture;
static core_sync_t worker_fence;
static retro_atomic_int_t worker_done; /* 1 drawn, -1 failed */
static unsigned    read_fbo;
static int         shown;

static int load_proc(void *dst, const char *name)
{
   retro_proc_address_t f = hw.get_proc_address(name);
   memcpy(dst, &f, sizeof(f));
   return f != NULL;
}

#define PROC(name) load_proc(&p_##name, "gl" #name)

static void *worker_main(void *arg)
{
   unsigned fbo = 0;
   int ok;
   (void)arg;

   if (!shared_iface->make_current(worker_ctx, true))
   {
      fprintf(stderr, "[gl_shared_context] worker: make_current failed\n");
      retro_atomic_store_release_int(&worker_done, -1);
      return NULL;
   }
   fprintf(stderr, "[gl_shared_context] worker: GL_VERSION \"%s\"\n",
         (const char*)p_GetString(CORE_GL_VERSION));

   p_GenTextures(1, &worker_texture);
   p_BindTexture(CORE_GL_TEXTURE_2D, worker_texture);
   p_TexParameteri(CORE_GL_TEXTURE_2D, CORE_GL_TEXTURE_MIN_FILTER, CORE_GL_NEAREST);
   p_TexImage2D(CORE_GL_TEXTURE_2D, 0, CORE_GL_RGBA8, W, H, 0,
         CORE_GL_RGBA, CORE_GL_UNSIGNED_BYTE, NULL);
   p_BindTexture(CORE_GL_TEXTURE_2D, 0);

   /* Framebuffers are not shared: this one is the worker's */
   p_GenFramebuffers(1, &fbo);
   p_BindFramebuffer(CORE_GL_FRAMEBUFFER, fbo);
   p_FramebufferTexture2D(CORE_GL_FRAMEBUFFER, CORE_GL_COLOR_ATTACHMENT0,
         CORE_GL_TEXTURE_2D, worker_texture, 0);
   ok = p_CheckFramebufferStatus(CORE_GL_FRAMEBUFFER) == CORE_GL_FRAMEBUFFER_COMPLETE;
   if (ok)
   {
      p_Viewport(0, 0, W, H);
      /* 204, 102, 51 in a screenshot */
      p_ClearColor(0.8f, 0.4f, 0.2f, 1.0f);
      p_Clear(CORE_GL_COLOR_BUFFER_BIT);
   }
   p_BindFramebuffer(CORE_GL_FRAMEBUFFER, 0);
   p_DeleteFramebuffers(1, &fbo);
   worker_fence = p_FenceSync(CORE_GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
   p_Flush();

   shared_iface->make_current(worker_ctx, false);
   fprintf(stderr, "[gl_shared_context] worker: %s\n",
         ok ? "drew the texture" : "framebuffer incomplete");
   fflush(stderr);
   retro_atomic_store_release_int(&worker_done, ok ? 1 : -1);
   return NULL;
}

static void context_reset(void)
{
   const unsigned char *version;

   have_context = PROC(GetString) && PROC(GenTextures) && PROC(DeleteTextures)
      && PROC(BindTexture) && PROC(TexImage2D) && PROC(TexParameteri)
      && PROC(GenFramebuffers) && PROC(DeleteFramebuffers)
      && PROC(BindFramebuffer) && PROC(FramebufferTexture2D)
      && PROC(CheckFramebufferStatus) && PROC(Viewport)
      && PROC(ClearColor) && PROC(Clear) && PROC(FenceSync)
      && PROC(WaitSync) && PROC(DeleteSync) && PROC(Flush)
      && PROC(BlitFramebuffer);
   version = have_context ? p_GetString(CORE_GL_VERSION) : NULL;
   if (!version)
   {
      have_context = 0;
      fprintf(stderr, "[gl_shared_context] context_reset: NO CONTEXT\n");
      fflush(stderr);
      return;
   }
   fprintf(stderr, "[gl_shared_context] context_reset: GL_VERSION \"%s\"\n",
         (const char*)version);

   shared_iface = NULL;
   if (     !environ_cb(RETRO_ENVIRONMENT_GET_HW_SHARED_CONTEXT_INTERFACE, &shared_iface)
         || !shared_iface
         || shared_iface->interface_version < RETRO_HW_SHARED_CONTEXT_INTERFACE_VERSION)
   {
      fprintf(stderr, "[gl_shared_context] context_reset: NO INTERFACE\n");
      fflush(stderr);
      return;
   }
   if (!(worker_ctx = shared_iface->create()))
   {
      fprintf(stderr, "[gl_shared_context] context_reset: create failed\n");
      fflush(stderr);
      return;
   }
   fprintf(stderr, "[gl_shared_context] context_reset: shared context made\n");
   fflush(stderr);
   retro_atomic_store_release_int(&worker_done, 0);
   worker_started = pthread_create(&worker, NULL, worker_main, NULL) == 0;
}

static void context_destroy(void)
{
   if (worker_started)
      pthread_join(worker, NULL);
   worker_started = 0;
   if (have_context)
   {
      if (read_fbo)
         p_DeleteFramebuffers(1, &read_fbo);
      if (worker_texture)
         p_DeleteTextures(1, &worker_texture);
      if (worker_fence)
         p_DeleteSync(worker_fence);
   }
   read_fbo = worker_texture = 0;
   worker_fence = NULL;
   if (worker_ctx)
      shared_iface->destroy(worker_ctx);
   worker_ctx   = NULL;
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
   info->library_name     = "gl_shared_context";
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
   unsigned fb;
   if (!have_context)
   {
      video_cb(NULL, W, H, 0);
      return;
   }
   fb = (unsigned)hw.get_current_framebuffer();
   if (retro_atomic_load_acquire_int(&worker_done) == 1)
   {
      if (!read_fbo)
      {
         /* The GPU, not this thread, waits for the worker's drawing */
         p_WaitSync(worker_fence, 0, ~0ULL);
         p_GenFramebuffers(1, &read_fbo);
         p_BindFramebuffer(CORE_GL_READ_FRAMEBUFFER, read_fbo);
         p_FramebufferTexture2D(CORE_GL_READ_FRAMEBUFFER, CORE_GL_COLOR_ATTACHMENT0,
               CORE_GL_TEXTURE_2D, worker_texture, 0);
      }
      p_BindFramebuffer(CORE_GL_READ_FRAMEBUFFER, read_fbo);
      p_BindFramebuffer(CORE_GL_DRAW_FRAMEBUFFER, fb);
      p_BlitFramebuffer(0, 0, W, H, 0, 0, W, H, CORE_GL_COLOR_BUFFER_BIT, CORE_GL_NEAREST);
      p_BindFramebuffer(CORE_GL_FRAMEBUFFER, fb);
      if (!shown++)
      {
         fprintf(stderr, "[gl_shared_context] retro_run: showing the worker's texture\n");
         fflush(stderr);
      }
   }
   else
   {
      p_BindFramebuffer(CORE_GL_FRAMEBUFFER, fb);
      p_Viewport(0, 0, W, H);
      p_ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
      p_Clear(CORE_GL_COLOR_BUFFER_BIT);
   }
   video_cb(RETRO_HW_FRAME_BUFFER_VALID, W, H, 0);
}

bool retro_load_game(const struct retro_game_info *game)
{
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
   (void)game;
   environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
   memset(&hw, 0, sizeof(hw));
   if (getenv("GL_SHARED_CONTEXT_CORE"))
   {
      hw.context_type  = RETRO_HW_CONTEXT_OPENGL_CORE;
      hw.version_major = 3;
      hw.version_minor = 3;
   }
   else
      hw.context_type  = RETRO_HW_CONTEXT_OPENGL;
   hw.context_reset   = context_reset;
   hw.context_destroy = context_destroy;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw))
   {
      fprintf(stderr, "[gl_shared_context] SET_HW_RENDER refused\n");
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
