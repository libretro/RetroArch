#include "../libretro.h"
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
static struct retro_hw_render_callback hw_render;

#define GL_GLEXT_PROTOTYPES
#if defined(GLES)
#ifdef IOS
#include <OpenGLES/ES2/gl.h>
#else
#include <GLES2/gl2.h>
#endif
#elif defined(__APPLE__)
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#else
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#define BASE_WIDTH 320
#define BASE_HEIGHT 240
#ifdef GLES
#define MAX_WIDTH 1024
#define MAX_HEIGHT 1024
#else
#define MAX_WIDTH 1920
#define MAX_HEIGHT 1600
#endif

static unsigned width = BASE_WIDTH;
static unsigned height = BASE_HEIGHT;

#if defined(GLES) || defined(__APPLE__)
#define pglCreateProgram glCreateProgram
#define pglCreateShader glCreateShader
#define pglCompileShader glCompileShader
#define pglUseProgram glUseProgram
#define pglShaderSource glShaderSource
#define pglAttachShader glAttachShader
#define pglLinkProgram glLinkProgram
#define pglBindFramebuffer glBindFramebuffer
#define pglGetUniformLocation glGetUniformLocation
#define pglUniformMatrix4fv glUniformMatrix4fv
#define pglGetAttribLocation glGetAttribLocation
#define pglVertexAttribPointer glVertexAttribPointer
#define pglEnableVertexAttribArray glEnableVertexAttribArray
#define pglDisableVertexAttribArray glDisableVertexAttribArray
#define pglGenBuffers glGenBuffers
#define pglBufferData glBufferData
#define pglBindBuffer glBindBuffer
#define init_gl_proc()
#else
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
static PFNGLGENBUFFERSPROC pglGenBuffers;
static PFNGLBUFFERDATAPROC pglBufferData;
static PFNGLBINDBUFFERPROC pglBindBuffer;

struct gl_proc_map
{
   void *proc;
   const char *sym;
};

#define PROC_BIND(name) { &(pgl##name), "gl" #name }
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
   PROC_BIND(GenBuffers),
   PROC_BIND(BufferData),
   PROC_BIND(BindBuffer),
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
#endif

static GLuint prog;
static GLuint vbo;

static const GLfloat vertex_data[] = {
   -0.5, -0.5,
    0.5, -0.5,
   -0.5,  0.5,
    0.5,  0.5,
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
   "#ifdef GL_ES\n",
   "precision mediump float;\n",
   "#endif\n",
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

static void setup_vao(void)
{
   pglUseProgram(prog);

   pglGenBuffers(1, &vbo);
   pglBindBuffer(GL_ARRAY_BUFFER, vbo);
   pglBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data, GL_STATIC_DRAW);

   pglBindBuffer(GL_ARRAY_BUFFER, 0);
   pglUseProgram(0);
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
      .base_width   = BASE_WIDTH,
      .base_height  = BASE_HEIGHT,
      .max_width    = MAX_WIDTH,
      .max_height   = MAX_HEIGHT,
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

   struct retro_variable variables[] = {
      { "resolution",
#ifdef GLES
         "Internal resolution; 320x240|360x480|480x272|512x384|512x512|640x240|640x448|640x480|720x576|800x600|960x720|1024x768" },
#else
      "Internal resolution; 320x240|360x480|480x272|512x384|512x512|640x240|640x448|640x480|720x576|800x600|960x720|1024x768|1024x1024|1280x720|1280x960|1600x1200|1920x1080|1920x1440|1920x1600" },
#endif
                         { NULL, NULL },
   };

   bool no_rom = true;
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_rom);
   cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);
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

static void update_variables(void)
{
   struct retro_variable var;

   var.key = "resolution";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var))
   {
      char *pch;
      char str[100];
      snprintf(str, sizeof(str), var.value);
      
      pch = strtok(str, "x");

      if (pch != NULL)
         width = atoi(pch);

      pch = strtok(str, "x");

      if (pch != NULL)
         height = atoi(pch);
   }
}

void retro_run(void)
{
   bool updated = false;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      update_variables();

   input_poll_cb();

   pglBindFramebuffer(GL_FRAMEBUFFER, hw_render.get_current_framebuffer());
   glClearColor(0.3, 0.4, 0.5, 1.0);
   glViewport(0, 0, width, height);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   pglUseProgram(prog);

   glEnable(GL_DEPTH_TEST);

   pglBindBuffer(GL_ARRAY_BUFFER, vbo);
   int vloc = pglGetAttribLocation(prog, "aVertex");
   pglVertexAttribPointer(vloc, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
   pglEnableVertexAttribArray(vloc);
   int cloc = pglGetAttribLocation(prog, "aColor");
   pglVertexAttribPointer(cloc, 4, GL_FLOAT, GL_FALSE, 0, (void*)(8 * sizeof(GLfloat)));
   pglEnableVertexAttribArray(cloc);
   pglBindBuffer(GL_ARRAY_BUFFER, 0);

   int loc = pglGetUniformLocation(prog, "uMVP");

   static unsigned frame_count;
   frame_count++;
   float angle = frame_count / 100.0;
   float cos_angle = cos(angle);
   float sin_angle = sin(angle);

   const GLfloat mvp[] = {
      cos_angle, -sin_angle, 0, 0,
      sin_angle, cos_angle, 0, 0,
      0, 0, 1, 0,
      0, 0, 0, 1,
   };
   pglUniformMatrix4fv(loc, 1, GL_FALSE, mvp);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   cos_angle *= 0.5;
   sin_angle *= 0.5;
   const GLfloat mvp2[] = {
      cos_angle, -sin_angle, 0, 0.0,
      sin_angle, cos_angle, 0, 0.0,
      0, 0, 1, 0,
      0.4, 0.4, 0.2, 1,
   };

   pglUniformMatrix4fv(loc, 1, GL_FALSE, mvp2);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   pglDisableVertexAttribArray(vloc);
   pglDisableVertexAttribArray(cloc);

   pglUseProgram(0);

   video_cb(RETRO_HW_FRAME_BUFFER_VALID, width, height, 0);
}

static void context_reset(void)
{
   fprintf(stderr, "Context reset!\n");
   init_gl_proc();
   compile_program();
   setup_vao();
}


bool retro_load_game(const struct retro_game_info *info)
{
   update_variables();

   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      fprintf(stderr, "XRGB8888 is not supported.\n");
      return false;
   }

#ifdef GLES
   hw_render.context_type = RETRO_HW_CONTEXT_OPENGLES2;
#else
   hw_render.context_type = RETRO_HW_CONTEXT_OPENGL;
#endif
   hw_render.context_reset = context_reset;
   hw_render.depth = true;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render))
      return false;

   fprintf(stderr, "Loaded game!\n");
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

