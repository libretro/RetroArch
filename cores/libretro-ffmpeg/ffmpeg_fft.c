#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <glsym/glsym.h>
#include <libretro.h>

#include <retro_miscellaneous.h>
#include <retro_inline.h>
#include <filters.h>
#include <math/complex.h>
#include <gfx/math/matrix_4x4.h>
#include <gfx/math/vector_2.h>

#include "ffmpeg_fft.h"

#if GL_DEBUG
#define GL_CHECK_ERROR() do { \
   if (glGetError() != GL_NO_ERROR) \
   { \
      log_cb(RETRO_LOG_ERROR, "GL error at line: %d\n", __LINE__); \
      abort(); \
   } \
} while(0)
#else
#define GL_CHECK_ERROR()
#endif

#ifndef M_HALF_PI
#define M_HALF_PI 1.57079632679489661923132169163975144
#endif

extern retro_log_printf_t log_cb;

struct target
{
   GLuint tex;
   GLuint fbo;
};

struct Pass
{
   struct target target;
   GLuint parameter_tex;
};

struct GLFFT
{
   GLuint ms_rb_color;
   GLuint ms_rb_ds;
   GLuint ms_fbo;

   struct Pass *passes;
   unsigned passes_size;

   GLuint input_tex;
   GLuint window_tex;
   GLuint prog_real;
   GLuint prog_complex;
   GLuint prog_resolve;
   GLuint prog_blur;

   GLuint quad;
   GLuint vao;

   unsigned output_ptr;

   struct target output, resolve, blur;

   struct Block
   {
      GLuint prog;
      GLuint vao;
      GLuint vbo;
      GLuint ibo;
      unsigned elems;
   } block;

   GLuint pbo;
   GLshort *sliding;
   unsigned sliding_size;

   unsigned steps;
   unsigned size;
   unsigned block_size;
   unsigned depth;
};

#include "gl_shaders/fft_heightmap.glsl.vert.h"
#include "gl_shaders/fft_heightmap.glsl.frag.h"
#include "gl_shaders/fft_vertex_program.glsl.vert.h"
#include "gl_shaders/fft_fragment_program_resolve.glsl.frag.h"
#include "gl_shaders/fft_fragment_program_real.glsl.frag.h"
#include "gl_shaders/fft_fragment_program_complex.glsl.frag.h"
#include "gl_shaders/fft_fragment_program_blur.glsl.frag.h"

static GLuint fft_compile_shader(fft_t *fft, GLenum type, const char *source)
{
   GLint status  = 0;
   GLuint shader = glCreateShader(type);

   glShaderSource(shader, 1, (const GLchar**)&source, NULL);
   glCompileShader(shader);

   glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

   if (!status)
   {
      char log_info[8 * 1024];
      GLsizei log_len;

      log_cb(RETRO_LOG_ERROR, "Failed to compile.\n");
      glGetShaderInfoLog(shader, sizeof(log_info), &log_len, log_info);
      log_cb(RETRO_LOG_ERROR, "ERROR: %s\n", log_info);
      return 0;
   }

   return shader;
}

static GLuint fft_compile_program(fft_t *fft,
      const char *vertex_source, const char *fragment_source)
{
   GLint status = 0;
   GLuint prog  = glCreateProgram();
   GLuint vert  = fft_compile_shader(fft, GL_VERTEX_SHADER, vertex_source);
   GLuint frag  = fft_compile_shader(fft, GL_FRAGMENT_SHADER, fragment_source);

   glAttachShader(prog, vert);
   glAttachShader(prog, frag);
   glLinkProgram(prog);

   glGetProgramiv(prog, GL_LINK_STATUS, &status);

   if (!status)
   {
      char log_info[8 * 1024];
      GLsizei log_len;

      log_cb(RETRO_LOG_ERROR, "Failed to link.\n");
      glGetProgramInfoLog(prog, sizeof(log_info), &log_len, log_info);
      log_cb(RETRO_LOG_ERROR, "ERROR: %s\n", log_info);
   }

   glDeleteShader(vert);
   glDeleteShader(frag);
   return prog;
}

typedef float stub_matrix4x4[4][4];

static INLINE unsigned log2i(unsigned x)
{
   unsigned res;

   for (res = 0; x; x >>= 1)
      res++;
   return res - 1;
}

static INLINE unsigned bitinverse(unsigned x, unsigned size)
{
   unsigned i;
   unsigned size_log2 = (size == 0) ? 0 : log2i(size);
   unsigned ret       = 0;

   for (i = 0; i < size_log2; i++)
      ret |= ((x >> i) & 0x1) << (size_log2 - 1 - i);
   return ret;
}

static fft_complex_t exp_imag(float phase)
{
   fft_complex_t out;
   out.real = cosf(phase);
   out.imag = sinf(phase);
   return out;
}

void fft_build_params(fft_t *fft, GLuint *buffer,
      unsigned step, unsigned size)
{
   unsigned i, j;
   unsigned step_size = 1 << step;

   for (i = 0; i < size; i += step_size << 1)
   {
      for (j = i; j < i + step_size; j++)
      {
         vec2_t tmp;
         int s                 = j - i;
         float phase           = -1.0f * (float)s / step_size;
         unsigned a            = j;
         unsigned b            = j + step_size;
         fft_complex_t twiddle = exp_imag(M_PI * phase);

         unsigned read_a       = (step == 0) ? bitinverse(a, size) : a;
         unsigned read_b       = (step == 0) ? bitinverse(b, size) : b;

         tmp[0]                = twiddle.real;
         tmp[1]                = twiddle.imag;

         buffer[2 * a + 0]     = (read_a << 16) | read_b;
         buffer[2 * a + 1]     = vec2_packHalf2x16(tmp[0], tmp[1]);
         buffer[2 * b + 0]     = (read_a << 16) | read_b;
         buffer[2 * b + 1]     = vec2_packHalf2x16(-tmp[0], -tmp[1]);
      }
   }
}

static void fft_init_quad_vao(fft_t *fft)
{
   static const GLbyte quad_buffer[] = {
      -1, -1, 1, -1, -1, 1, 1, 1,
       0,  0, 1,  0,  0, 1, 1, 1,
   };
   glGenBuffers(1, &fft->quad);
   glBindBuffer(GL_ARRAY_BUFFER, fft->quad);
   glBufferData(GL_ARRAY_BUFFER,
         sizeof(quad_buffer), quad_buffer, GL_STATIC_DRAW);
   glBindBuffer(GL_ARRAY_BUFFER, 0);

   glGenVertexArrays(1, &fft->vao);
   glBindVertexArray(fft->vao);
   glBindBuffer(GL_ARRAY_BUFFER, fft->quad);
   glEnableVertexAttribArray(0);
   glEnableVertexAttribArray(1);
   glVertexAttribPointer(0, 2, GL_BYTE, GL_FALSE, 0, 0);
   glVertexAttribPointer(1, 2, GL_BYTE, GL_FALSE, 0,
         (const GLvoid*)((uintptr_t)(8)));
   glBindVertexArray(0);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void fft_init_texture(fft_t *fft, GLuint *tex, GLenum format,
      unsigned width, unsigned height, unsigned levels, GLenum mag, GLenum min)
{
   glGenTextures(1, tex);
   glBindTexture(GL_TEXTURE_2D, *tex);
   glTexStorage2D(GL_TEXTURE_2D, levels, format, width, height);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min);
   glBindTexture(GL_TEXTURE_2D, 0);
}

static void fft_init_target(fft_t *fft, struct target *target, GLenum format,
      unsigned width, unsigned height, unsigned levels, GLenum mag, GLenum min)
{
   fft_init_texture(fft, &target->tex, format, width, height, levels, mag, min);
   glGenFramebuffers(1, &target->fbo);
   glBindFramebuffer(GL_FRAMEBUFFER, target->fbo);

   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
      target->tex, 0);

   if (format == GL_RGBA8)
   {
      glClearColor(0, 0, 0, 0);
      glClear(GL_COLOR_BUFFER_BIT);
   }
   else if (format == GL_RG16I)
   {
      static const GLint v[] = { 0, 0, 0, 0 };
      glClearBufferiv(GL_COLOR, 0, v);
   }
   else
   {
      static const GLuint v[] = { 0, 0, 0, 0 };
      glClearBufferuiv(GL_COLOR, 0, v);
   }
   glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

#define KAISER_BETA 12.0

static void fft_init(fft_t *fft)
{
   unsigned i;
   double window_mod;
   GLushort *window             = NULL;
   static const GLfloat unity[] = { 0.0f, 0.0f, 1.0f, 1.0f };

   fft->prog_real    = fft_compile_program(fft, fft_vertex_program, fft_fragment_program_real);
   fft->prog_complex = fft_compile_program(fft, fft_vertex_program, fft_fragment_program_complex);
   fft->prog_resolve = fft_compile_program(fft, fft_vertex_program, fft_fragment_program_resolve);
   fft->prog_blur    = fft_compile_program(fft, fft_vertex_program, fft_fragment_program_blur);
   GL_CHECK_ERROR();

   glUseProgram(fft->prog_real);
   glUniform1i(glGetUniformLocation(fft->prog_real, "sTexture"), 0);
   glUniform1i(glGetUniformLocation(fft->prog_real, "sParameterTexture"), 1);
   glUniform1i(glGetUniformLocation(fft->prog_real, "sWindow"), 2);
   glUniform4fv(glGetUniformLocation(fft->prog_real, "uOffsetScale"), 1, unity);

   glUseProgram(fft->prog_complex);
   glUniform1i(glGetUniformLocation(fft->prog_complex, "sTexture"), 0);
   glUniform1i(glGetUniformLocation(fft->prog_complex, "sParameterTexture"), 1);
   glUniform4fv(glGetUniformLocation(fft->prog_complex, "uOffsetScale"), 1, unity);

   glUseProgram(fft->prog_resolve);
   glUniform1i(glGetUniformLocation(fft->prog_resolve, "sFFT"), 0);
   glUniform4fv(glGetUniformLocation(fft->prog_resolve, "uOffsetScale"), 1, unity);

   glUseProgram(fft->prog_blur);
   glUniform1i(glGetUniformLocation(fft->prog_blur, "sHeight"), 0);
   glUniform4fv(glGetUniformLocation(fft->prog_blur, "uOffsetScale"), 1, unity);

   GL_CHECK_ERROR();

   fft_init_texture(fft, &fft->window_tex, GL_R16UI,
         fft->size, 1, 1, GL_NEAREST, GL_NEAREST);
   GL_CHECK_ERROR();

   window = (GLushort*)calloc(fft->size, sizeof(GLushort));

   window_mod = 1.0 / kaiser_window_function(0.0, KAISER_BETA);

   for (i = 0; i < fft->size; i++)
   {
      double phase = (double)(i - (int)(fft->size) / 2) / ((int)(fft->size) / 2);
      double     w = kaiser_window_function(phase, KAISER_BETA);
      window[i]    = round(0xffff * w * window_mod);
   }
   glBindTexture(GL_TEXTURE_2D, fft->window_tex);
   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
         fft->size, 1, GL_RED_INTEGER, GL_UNSIGNED_SHORT, &window[0]);
   glBindTexture(GL_TEXTURE_2D, 0);

   GL_CHECK_ERROR();
   fft_init_texture(fft, &fft->input_tex, GL_RG16I,
         fft->size, 1, 1, GL_NEAREST, GL_NEAREST);
   fft_init_target(fft, &fft->output, GL_RG32UI,
         fft->size, fft->depth, 1, GL_NEAREST, GL_NEAREST);
   fft_init_target(fft, &fft->resolve, GL_RGBA8,
         fft->size, fft->depth, 1, GL_NEAREST, GL_NEAREST);
   fft_init_target(fft, &fft->blur, GL_RGBA8,
         fft->size, fft->depth,
         log2i(MAX(fft->size, fft->depth)) + 1,
         GL_NEAREST, GL_LINEAR_MIPMAP_LINEAR);

   GL_CHECK_ERROR();

   for (i = 0; i < fft->steps; i++)
   {
      GLuint *param_buffer = NULL;
      fft_init_target(fft, &fft->passes[i].target,
            GL_RG32UI, fft->size, 1, 1, GL_NEAREST, GL_NEAREST);
      fft_init_texture(fft, &fft->passes[i].parameter_tex,
            GL_RG32UI, fft->size, 1, 1, GL_NEAREST, GL_NEAREST);

      param_buffer = (GLuint*)calloc(2 * fft->size, sizeof(GLuint));

      fft_build_params(fft, &param_buffer[0], i, fft->size);

      glBindTexture(GL_TEXTURE_2D, fft->passes[i].parameter_tex);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
            fft->size, 1, GL_RG_INTEGER, GL_UNSIGNED_INT, &param_buffer[0]);
      glBindTexture(GL_TEXTURE_2D, 0);

      free(param_buffer);
   }

   GL_CHECK_ERROR();
   glGenBuffers(1, &fft->pbo);
   glBindBuffer(GL_PIXEL_UNPACK_BUFFER, fft->pbo);
   glBufferData(GL_PIXEL_UNPACK_BUFFER,
         fft->size * 2 * sizeof(GLshort), 0, GL_DYNAMIC_DRAW);
   glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

   free(window);
}

static void fft_init_block(fft_t *fft)
{
   unsigned x, y;
   unsigned block_vertices_size = 0;
   unsigned block_indices_size  = 0;
   int pos                      = 0;
   GLuint *bp                   = NULL;
   GLushort *block_vertices     = NULL;
   GLuint   *block_indices      = NULL;

   fft->block.prog              = fft_compile_program(fft,
         fft_vertex_program_heightmap, fft_fragment_program_heightmap);

   glUseProgram(fft->block.prog);
   glUniform1i(glGetUniformLocation(fft->block.prog, "sHeight"), 0);

   block_vertices_size = 2 * fft->block_size * fft->depth;
   block_vertices      = (GLushort*)calloc(block_vertices_size, sizeof(GLushort));

   for (y = 0; y < fft->depth; y++)
   {
      for (x = 0; x < fft->block_size; x++)
      {
         block_vertices[2 * (y * fft->block_size + x) + 0] = x;
         block_vertices[2 * (y * fft->block_size + x) + 1] = y;
      }
   }
   glGenBuffers(1, &fft->block.vbo);
   glBindBuffer(GL_ARRAY_BUFFER, fft->block.vbo);
   glBufferData(GL_ARRAY_BUFFER,
         block_vertices_size * sizeof(GLushort),
         &block_vertices[0], GL_STATIC_DRAW);

   fft->block.elems = (2 * fft->block_size - 1) * (fft->depth - 1) + 1;

   block_indices_size = fft->block.elems;
   block_indices = (GLuint*)calloc(block_indices_size, sizeof(GLuint));

   bp = &block_indices[0];

   for (y = 0; y < fft->depth - 1; y++)
   {
      int x;
      int step_odd  = (-(int)(fft->block_size)) + ((y & 1) ? -1 : 1);
      int step_even = fft->block_size;

      for (x = 0; x < 2 * (int)(fft->block_size) - 1; x++)
      {
         *bp++ = pos;
         pos += (x & 1) ? step_odd : step_even;
      }
   }
   *bp++ = pos;

   glGenVertexArrays(1, &fft->block.vao);
   glBindVertexArray(fft->block.vao);

   glGenBuffers(1, &fft->block.ibo);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, fft->block.ibo);
   glBufferData(GL_ELEMENT_ARRAY_BUFFER,
         block_indices_size * sizeof(GLuint),
         &block_indices[0], GL_STATIC_DRAW);

   glEnableVertexAttribArray(0);
   glVertexAttribPointer(0, 2, GL_UNSIGNED_SHORT, GL_FALSE, 0, 0);
   glBindVertexArray(0);

   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

   free(block_vertices);
   free(block_indices);
}

static bool fft_context_reset(fft_t *fft, unsigned fft_steps,
      rglgen_proc_address_t proc, unsigned fft_depth)
{
   rglgen_resolve_symbols(proc);

   fft->steps       = fft_steps;
   fft->depth       = fft_depth;
   fft->size        = 1 << fft_steps;
   fft->block_size  = fft->size / 4 + 1;

   fft->passes_size = fft_steps;
   fft->passes      = (struct Pass*)calloc(fft->passes_size, sizeof(struct Pass));

   if (!fft->passes)
      return false;

   fft->sliding_size = 2 * fft->size;
   fft->sliding      = (GLshort*)calloc(fft->sliding_size, sizeof(GLshort));

   if (!fft->sliding)
      return false;

   GL_CHECK_ERROR();
   fft_init_quad_vao(fft);
   GL_CHECK_ERROR();
   fft_init(fft);
   GL_CHECK_ERROR();
   fft_init_block(fft);
   GL_CHECK_ERROR();

   return true;
}

/* GLFFT requires either GLES3 or
 * desktop GL with ES3_compat (supported by MESA on Linux) extension. */
fft_t *fft_new(unsigned fft_steps, rglgen_proc_address_t proc)
{
   fft_t *fft    = NULL;
#ifdef HAVE_OPENGLES3
   const char *ver = (const char*)(glGetString(GL_VERSION));
   if (ver)
   {
      unsigned major, minor;
      if (sscanf(ver, "OpenGL ES %u.%u", &major, &minor) != 2 || major < 3)
         return NULL;
   }
   else
      return NULL;
#else
   const char *exts = (const char*)(glGetString(GL_EXTENSIONS));
   if (!exts || !strstr(exts, "ARB_ES3_compatibility"))
      return NULL;
#endif
   fft = (fft_t*)calloc(1, sizeof(*fft));

   if (!fft)
      return NULL;

   if (!fft_context_reset(fft, fft_steps, proc, 256))
      goto error;

   return fft;

error:
   if (fft)
      free(fft);
   return NULL;
}

void fft_init_multisample(fft_t *fft, unsigned width, unsigned height, unsigned samples)
{
   if (fft->ms_rb_color)
      glDeleteRenderbuffers(1, &fft->ms_rb_color);
   fft->ms_rb_color = 0;
   if (fft->ms_rb_ds)
      glDeleteRenderbuffers(1, &fft->ms_rb_ds);
   fft->ms_rb_ds    = 0;
   if (fft->ms_fbo)
      glDeleteFramebuffers(1, &fft->ms_fbo);
   fft->ms_fbo      = 0;

   if (samples > 1)
   {
      glGenRenderbuffers(1, &fft->ms_rb_color);
      glGenRenderbuffers(1, &fft->ms_rb_ds);
      glGenFramebuffers (1, &fft->ms_fbo);

      glBindRenderbuffer(GL_RENDERBUFFER, fft->ms_rb_color);
      glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples,
            GL_RGBA8, width, height);
      glBindRenderbuffer(GL_RENDERBUFFER, fft->ms_rb_ds);
      glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples,
            GL_DEPTH24_STENCIL8, width, height);
      glBindRenderbuffer(GL_RENDERBUFFER, 0);

      glBindFramebuffer(GL_FRAMEBUFFER, fft->ms_fbo);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_RENDERBUFFER, fft->ms_rb_color);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
            GL_RENDERBUFFER, fft->ms_rb_ds);
      if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
         fft_init_multisample(fft, 0, 0, 0);
   }

   glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void fft_context_destroy(fft_t *fft)
{
   fft_init_multisample(fft, 0, 0, 0);
   if (fft->passes)
      free(fft->passes);
   fft->passes = NULL;
   if (fft->sliding)
      free(fft->sliding);
   fft->sliding = NULL;
}

void fft_free(fft_t *fft)
{
   if (!fft)
      return;

   fft_context_destroy(fft);
   if (fft)
      free(fft);
   fft = NULL;
}

void fft_step_fft(fft_t *fft, const GLshort *audio_buffer, unsigned frames)
{
   unsigned i;
   GLfloat resolve_offset[4];
   GLshort *buffer = NULL;
   GLshort *slide  = (GLshort*)&fft->sliding[0];

   glEnable(GL_DEPTH_TEST);
   glEnable(GL_CULL_FACE);
   glBindVertexArray(fft->vao);

   glActiveTexture(GL_TEXTURE2);
   glBindTexture(GL_TEXTURE_2D, fft->window_tex);

   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, fft->input_tex);
   glUseProgram(fft->prog_real);

   memmove(slide, slide + frames * 2,
         (fft->sliding_size - 2 * frames) * sizeof(GLshort));
   memcpy(slide + fft->sliding_size - frames * 2, audio_buffer,
         2 * frames * sizeof(GLshort));

   /* Upload audio data to GPU. */
   glBindBuffer(GL_PIXEL_UNPACK_BUFFER, fft->pbo);

   buffer = (GLshort*)(glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0,
            2 * fft->size * sizeof(GLshort),
            GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));

   if (buffer)
   {
      memcpy(buffer, slide, fft->sliding_size * sizeof(GLshort));
      glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
   }
   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, fft->size, 1,
         GL_RG_INTEGER, GL_SHORT, NULL);
   glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

   /* Perform FFT of new block. */
   glViewport(0, 0, fft->size, 1);

   for (i = 0; i < fft->steps; i++)
   {
      if (i == fft->steps - 1)
      {
         glBindFramebuffer(GL_FRAMEBUFFER, fft->output.fbo);
         glUniform1i(glGetUniformLocation(i == 0
                  ? fft->prog_real : fft->prog_complex, "uViewportOffset"),
               fft->output_ptr);
         glViewport(0, fft->output_ptr, fft->size, 1);
      }
      else
      {
         glUniform1i(glGetUniformLocation(i == 0
                  ? fft->prog_real : fft->prog_complex, "uViewportOffset"), 0);
         glBindFramebuffer(GL_FRAMEBUFFER, fft->passes[i].target.fbo);
         glClear(GL_COLOR_BUFFER_BIT);
      }

      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, fft->passes[i].parameter_tex);

      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, fft->passes[i].target.tex);

      if (i == 0)
         glUseProgram(fft->prog_complex);
   }
   glActiveTexture(GL_TEXTURE0);

   /* Resolve new chunk to heightmap. */
   glViewport(0, fft->output_ptr, fft->size, 1);
   glUseProgram(fft->prog_resolve);
   glBindFramebuffer(GL_FRAMEBUFFER, fft->resolve.fbo);

   resolve_offset[0] = 0.0f;
   resolve_offset[1] = (float)(fft->output_ptr) / fft->depth;
   resolve_offset[2] = 1.0f;
   resolve_offset[3] = 1.0f / fft->depth;

   glUniform4fv(glGetUniformLocation(fft->prog_resolve, "uOffsetScale"),
         1, resolve_offset);
   glBindTexture(GL_TEXTURE_2D, fft->output.tex);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   /* Re-blur damaged regions of heightmap. */
   glUseProgram(fft->prog_blur);
   glBindTexture(GL_TEXTURE_2D, fft->resolve.tex);
   glBindFramebuffer(GL_FRAMEBUFFER, fft->blur.fbo);
   glUniform4fv(glGetUniformLocation(fft->prog_blur, "uOffsetScale"),
         1, resolve_offset);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   /* Mipmap the heightmap. */
   glBindTexture(GL_TEXTURE_2D, fft->blur.tex);
   glGenerateMipmap(GL_TEXTURE_2D);
   glBindTexture(GL_TEXTURE_2D, 0);

   fft->output_ptr++;
   fft->output_ptr &= fft->depth - 1;

   glBindVertexArray(0);
   glUseProgram(0);
   GL_CHECK_ERROR();
}

void fft_render(fft_t *fft, GLuint backbuffer, unsigned width, unsigned height)
{
   vec3_t eye, center, up;
   stub_matrix4x4 mvp_real;
   math_matrix_4x4 mvp_lookat, mvp, mvp_persp;

   eye[0]               = 0.0f;
   eye[1]               = 80.0f;
   eye[2]               = -60.0f;

   up[0]                = 0.0f;
   up[1]                = 1.0f;
   up[2]                = 0.0f;

   center[0]            = 0.0f;
   center[1]            = 0.0f;
   center[2]            = 1.0f;

   vec3_add(center, eye);

   matrix_4x4_projection(mvp_persp, (float)M_HALF_PI, (float)width / height, 1.0f, 500.0f);
   matrix_4x4_lookat(mvp_lookat, eye, center, up);
   matrix_4x4_multiply(mvp, mvp_lookat, mvp_persp);

   /* Render scene. */
   glBindFramebuffer(GL_FRAMEBUFFER, fft->ms_fbo ? fft->ms_fbo : backbuffer);
   glViewport(0, 0, width, height);
   glClearColor(0.1f, 0.15f, 0.1f, 1.0f);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

   glUseProgram(fft->block.prog);

   mvp_real[0][0] = MAT_ELEM_4X4(mvp, 0, 0);
   mvp_real[0][1] = MAT_ELEM_4X4(mvp, 0, 1);
   mvp_real[0][2] = MAT_ELEM_4X4(mvp, 0, 2);
   mvp_real[0][3] = MAT_ELEM_4X4(mvp, 0, 3);
   mvp_real[1][0] = MAT_ELEM_4X4(mvp, 1, 0);
   mvp_real[1][1] = MAT_ELEM_4X4(mvp, 1, 1);
   mvp_real[1][2] = MAT_ELEM_4X4(mvp, 1, 2);
   mvp_real[1][3] = MAT_ELEM_4X4(mvp, 1, 3);
   mvp_real[2][0] = MAT_ELEM_4X4(mvp, 2, 0);
   mvp_real[2][1] = MAT_ELEM_4X4(mvp, 2, 1);
   mvp_real[2][2] = MAT_ELEM_4X4(mvp, 2, 2);
   mvp_real[2][3] = MAT_ELEM_4X4(mvp, 2, 3);
   mvp_real[3][0] = MAT_ELEM_4X4(mvp, 3, 0);
   mvp_real[3][1] = MAT_ELEM_4X4(mvp, 3, 1);
   mvp_real[3][2] = MAT_ELEM_4X4(mvp, 3, 2);
   mvp_real[3][3] = MAT_ELEM_4X4(mvp, 3, 3);

   glUniformMatrix4fv(glGetUniformLocation(fft->block.prog, "uMVP"),
         1, GL_FALSE, (&mvp_real[0][0]));

   glUniform2i(glGetUniformLocation(fft->block.prog, "uOffset"),
         ((-(int)(fft->block_size)) + 1) / 2, fft->output_ptr);
   glUniform4f(glGetUniformLocation(fft->block.prog, "uHeightmapParams"),
         -(fft->block_size - 1.0f) / 2.0f, 0.0f, 3.0f, 2.0f);
   glUniform1f(glGetUniformLocation(fft->block.prog, "uAngleScale"),
         M_PI / ((fft->block_size - 1) / 2));

   glBindVertexArray(fft->block.vao);
   glBindTexture(GL_TEXTURE_2D, fft->blur.tex);
   glDrawElements(GL_TRIANGLE_STRIP, fft->block.elems, GL_UNSIGNED_INT, NULL);
   glBindVertexArray(0);
   glUseProgram(0);

   if (fft->ms_fbo)
   {
      static const GLenum attachments[] = { GL_COLOR_ATTACHMENT0, GL_DEPTH_STENCIL_ATTACHMENT };

      glBindFramebuffer(GL_READ_FRAMEBUFFER, fft->ms_fbo);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, backbuffer);
      glBlitFramebuffer(0, 0, width, height, 0, 0, width, height,
            GL_COLOR_BUFFER_BIT, GL_NEAREST);

      glBindFramebuffer(GL_FRAMEBUFFER, fft->ms_fbo);
      glInvalidateFramebuffer(GL_FRAMEBUFFER, 2, attachments);
      GL_CHECK_ERROR();
   }

   glBindFramebuffer(GL_FRAMEBUFFER, 0);
   GL_CHECK_ERROR();
}
