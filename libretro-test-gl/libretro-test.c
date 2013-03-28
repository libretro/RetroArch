#include "../libretro.h"
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <GL/gl.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/glext.h>

static PFNGLCREATEPROGRAMPROC pglCreateProgram;
static PFNGLCREATESHADERPROC pglCreateShader;
static PFNGLCREATESHADERPROC pglCompileShader;
static PFNGLCREATESHADERPROC pglUseProgram;
static PFNGLSHADERSOURCEPROC pglShaderSource;
static PFNGLATTACHSHADERPROC pglAttachShader;
static PFNGLLINKPROGRAMPROC pglLinkProgram;
static PFNGLBINDFRAMEBUFFERPROC pglBindFramebuffer;
static PFNGLGETUNIFORMLOCATIONPROC pglGetUniformLocation;
static PFNGLUNIFORMMATRIX4FVPROC pglUniformMatrix4fv;
static PFNGLGETATTRIBLOCATIONPROC pglGetAttribLocation;
static PFNGLVERTEXATTRIBPOINTERPROC pglVertexAttribPointer;
static PFNGLENABLEVERTEXATTRIBARRAYPROC pglEnableVertexAttribArray;
static PFNGLDISABLEVERTEXATTRIBARRAYPROC pglDisableVertexAttribArray;

static struct retro_hw_render_callback hw_render;

struct gl_proc_map
{
   void *proc;
   const char *sym;
};

#define PROC_BIND(name) { &(pgl##name), "gl" #name }
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
static const struct gl_proc_map proc_map[] = {
   PROC_BIND(CreateProgram),
   PROC_BIND(CreateShader),
   PROC_BIND(CompileShader),
   PROC_BIND(UseProgram),
   PROC_BIND(ShaderSource),
   PROC_BIND(AttachShader),
   PROC_BIND(LinkProgram),
   PROC_BIND(BindFramebuffer),
   PROC_BIND(GetUniformLocation),
   PROC_BIND(GetAttribLocation),
   PROC_BIND(UniformMatrix4fv),
   PROC_BIND(VertexAttribPointer),
   PROC_BIND(EnableVertexAttribArray),
   PROC_BIND(DisableVertexAttribArray),
};

static void init_gl_proc(void)
{
   for (unsigned i = 0; i < ARRAY_SIZE(proc_map); i++)
   {
      retro_proc_address_t proc = hw_render.get_proc_address(proc_map[i].sym);
      if (!proc)
         fprintf(stderr, "Symbol %s not found!\n", proc_map[i].sym);
      memcpy(proc_map[i].proc, &proc, sizeof(proc));
   }
}

static GLuint prog;

static const GLfloat vertex[] = {
   0.0, 0.0,
   1.0, 0.0,
   0.0, 1.0,
   1.0, 1.0,
};

static const GLfloat color[] = {
   1.0, 1.0, 1.0, 1.0,
   1.0, 1.0, 0.0, 1.0,
   0.0, 1.0, 1.0, 1.0,
   1.0, 0.0, 1.0, 1.0,
};


static const char *vertex_shader[] = {
   "uniform mat4 uMVP;",
   "attribute vec2 aVertex;",
   "attribute vec4 aColor;",
   "varying vec4 color;",
   "void main() {",
   "  gl_Position = uMVP * vec4(aVertex, 0.0, 1.0);",
   "  color = aColor;",
   "}",
};

static const char *fragment_shader[] = {
   "varying vec4 color;",
   "void main() {",
   "  gl_FragColor = color;",
   "}",
};

static void compile_program(void)
{
   prog = pglCreateProgram();
   GLuint vert = pglCreateShader(GL_VERTEX_SHADER);
   GLuint frag = pglCreateShader(GL_FRAGMENT_SHADER);

   pglShaderSource(vert, ARRAY_SIZE(vertex_shader), vertex_shader, 0);
   pglShaderSource(frag, ARRAY_SIZE(fragment_shader), fragment_shader, 0);
   pglCompileShader(vert);
   pglCompileShader(frag);

   pglAttachShader(prog, vert);
   pglAttachShader(prog, frag);
   pglLinkProgram(prog);
}

void retro_init(void)
{}

void retro_deinit(void)
{}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   (void)port;
   (void)device;
}

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "TestCore GL";
   info->library_version  = "v1";
   info->need_fullpath    = false;
   info->valid_extensions = NULL; // Anything is fine, we don't care.
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   info->timing = (struct retro_system_timing) {
      .fps = 60.0,
      .sample_rate = 30000.0,
   };

   info->geometry = (struct retro_game_geometry) {
      .base_width   = 320,
      .base_height  = 240,
      .max_width    = 320,
      .max_height   = 240,
      .aspect_ratio = 4.0 / 3.0,
   };
}

static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
   audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
   input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
   input_state_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

void retro_run(void)
{
   input_poll_cb();

   pglBindFramebuffer(GL_FRAMEBUFFER, hw_render.get_current_framebuffer());
   glClearColor(0.3, 0.4, 0.5, 1.0);
   glViewport(0, 0, 320, 240);
   glClear(GL_COLOR_BUFFER_BIT);

   pglUseProgram(prog);

   int loc = pglGetUniformLocation(prog, "uMVP");
   static const GLfloat identity[] = {
      1, 0, 0, 0,
      0, 1, 0, 0,
      0, 0, 1, 0,
      0, 0, 0, 1,
   };

   pglUniformMatrix4fv(loc, 1, GL_FALSE, identity);

   int vloc = pglGetAttribLocation(prog, "aVertex");
   pglVertexAttribPointer(vloc, 2, GL_FLOAT, GL_FALSE, 0, vertex);
   pglEnableVertexAttribArray(vloc);
   int cloc = pglGetAttribLocation(prog, "aColor");
   pglVertexAttribPointer(cloc, 4, GL_FLOAT, GL_FALSE, 0, color);
   pglEnableVertexAttribArray(cloc);

   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   pglUseProgram(0);
   pglDisableVertexAttribArray(vloc);
   pglDisableVertexAttribArray(cloc);

   video_cb(RETRO_HW_FRAME_BUFFER_VALID, 320, 240, 0);
}

static void context_reset(void)
{
   fprintf(stderr, "Context reset!\n");
   init_gl_proc();
   compile_program();
}

bool retro_load_game(const struct retro_game_info *info)
{
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      fprintf(stderr, "XRGB8888 is not supported.\n");
      return false;
   }

   hw_render.context_type = RETRO_HW_CONTEXT_OPENGL;
   hw_render.context_reset = context_reset;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render))
      return false;

   (void)info;
   return true;
}

void retro_unload_game(void)
{}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
   (void)type;
   (void)info;
   (void)num;
   return false;
}

size_t retro_serialize_size(void)
{
   return 0;
}

bool retro_serialize(void *data, size_t size)
{
   (void)data;
   (void)size;
   return false;
}

bool retro_unserialize(const void *data, size_t size)
{
   (void)data;
   (void)size;
   return false;
}

void *retro_get_memory_data(unsigned id)
{
   (void)id;
   return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
   (void)id;
   return 0;
}

void retro_reset(void)
{}

void retro_cheat_reset(void)
{}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}

