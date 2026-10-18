#define retro_run fixture_run
#define retro_load_game fixture_load_game
#include "../../record/nvenc_frontend/core.c"
#undef retro_run
#undef retro_load_game

#if defined(_WIN32)
#define GL_CALL __stdcall
#else
#define GL_CALL
#endif
static struct retro_hw_render_callback hardware;
static void (GL_CALL *bind_framebuffer)(unsigned, unsigned);
static void (GL_CALL *clear_buffer)(unsigned, int, const float*);
static void (GL_CALL *attachment_parameter)(unsigned, unsigned, unsigned, int*);
static void (GL_CALL *flush)(void);

static void reset_context(void)
{
   bind_framebuffer = (void (GL_CALL*)(unsigned, unsigned))
      hardware.get_proc_address("glBindFramebuffer");
   clear_buffer = (void (GL_CALL*)(unsigned, int, const float*))
      hardware.get_proc_address("glClearBufferfv");
   attachment_parameter = (void (GL_CALL*)(unsigned, unsigned, unsigned, int*))
      hardware.get_proc_address("glGetFramebufferAttachmentParameteriv");
   flush = (void (GL_CALL*)(void))hardware.get_proc_address("glFlush");
   if (!bind_framebuffer || !clear_buffer || !attachment_parameter || !flush)
      abort();
}

RETRO_API bool retro_load_game(const struct retro_game_info *info)
{
   enum retro_pixel_format format = RETRO_PIXEL_FORMAT_SCRGB_RGBA16F;
   (void)info;
   if (!environment(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &format))
      return false;
   memset(&hardware, 0, sizeof(hardware));
   hardware.context_type = RETRO_HW_CONTEXT_OPENGL_CORE;
   hardware.version_major = 3;
   hardware.version_minor = 3;
   hardware.context_reset = reset_context;
   return environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hardware);
}

RETRO_API void retro_run(void)
{
   static const float color[4] = { -0.25f, 0.5f, 2.0f, 1.0f };
   int component = 0;
   bind_framebuffer(0x8d40, (unsigned)hardware.get_current_framebuffer());
   attachment_parameter(0x8d40, 0x8ce0, 0x8211, &component);
   if (component != 0x1406)
      abort();
   clear_buffer(0x1800, 0, color);
   flush();
   if (!frames)
      fprintf(stderr, "[fp16] PASS: native floating-point core framebuffer\n");
   frames++;
   video(RETRO_HW_FRAME_BUFFER_VALID, WIDTH, HEIGHT, 0);
}
