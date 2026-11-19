/* Real GLES texture uploads through the shared persistent loan path. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glsym/glsym.h>
#include <gfx/gl_capabilities.h>
#include "../../../gfx/common/gl_common.h"

bool gl_query_extension(const char *name)
{
   GLint count = 0, i;
   glGetIntegerv(GL_NUM_EXTENSIONS, &count);
   for (i = 0; i < count; i++)
   {
      const char *ext = (const char*)glGetStringi(GL_EXTENSIONS, i);
      if (ext && !strncmp(ext, "GL_", 3) && !strcmp(ext + 3, name))
         return true;
   }
   return false;
}

static rglgen_func_t get_proc(const char *name)
{
   return (rglgen_func_t)eglGetProcAddress(name);
}

/* A software core's BGRX frame on GLES 3 without BGRA8888: uploaded as
 * it is into an RGBA texture that swaps R and B as it is sampled
 * (gl2's GL2_CHAIN_FLAG_FRAME_SWIZZLE), at a pitch wider than the frame,
 * samples as the frame - what the CPU conversion it replaces made. */
static GLuint swizzle_shader(GLenum type, const char *src)
{
   GLint ok = 0;
   GLuint sh = glCreateShader(type);
   glShaderSource(sh, 1, &src, NULL);
   glCompileShader(sh);
   glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
   assert(ok);
   return sh;
}

static void swizzle_check(void)
{
   static const char *vs =
      "#version 300 es\n"
      "void main() {\n"
      "   vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));\n"
      "   gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
      "}\n";
   static const char *fs =
      "#version 300 es\n"
      "precision mediump float;\n"
      "uniform mediump sampler2D tex;\n"
      "out vec4 color;\n"
      "void main() { color = texelFetch(tex, ivec2(gl_FragCoord.xy), 0); }\n";
   enum { W = 4, H = 2, PITCH_PX = 6 };
   unsigned char frame[PITCH_PX * H * 4], want[W * H * 4], result[W * H * 4];
   GLuint src, dst, fbo, prog, vao;
   GLint ok = 0;
   unsigned x, y, c;

   for (y = 0; y < H; y++)
      for (x = 0; x < PITCH_PX; x++)
         for (c = 0; c < 4; c++)
            frame[(y * PITCH_PX + x) * 4 + c] =
               (unsigned char)(17 + y * 61 + x * 29 + c * 7);
   /* B, G, R, X in memory; R, G, B, X sampled */
   for (y = 0; y < H; y++)
      for (x = 0; x < W; x++)
      {
         const unsigned char *in = frame + (y * PITCH_PX + x) * 4;
         unsigned char *out      = want  + (y * W + x) * 4;
         out[0] = in[2]; out[1] = in[1]; out[2] = in[0]; out[3] = in[3];
      }

   glGenTextures(1, &src);
   glBindTexture(GL_TEXTURE_2D, src);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0,
         GL_RGBA, GL_UNSIGNED_BYTE, NULL);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
   glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
   glPixelStorei(GL_UNPACK_ROW_LENGTH, PITCH_PX);
   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, W, H,
         GL_RGBA, GL_UNSIGNED_BYTE, frame);
   glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

   glGenTextures(1, &dst);
   glBindTexture(GL_TEXTURE_2D, dst);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0,
         GL_RGBA, GL_UNSIGNED_BYTE, NULL);
   glGenFramebuffers(1, &fbo);
   glBindFramebuffer(GL_FRAMEBUFFER, fbo);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
         GL_TEXTURE_2D, dst, 0);
   assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

   prog = glCreateProgram();
   glAttachShader(prog, swizzle_shader(GL_VERTEX_SHADER, vs));
   glAttachShader(prog, swizzle_shader(GL_FRAGMENT_SHADER, fs));
   glLinkProgram(prog);
   glGetProgramiv(prog, GL_LINK_STATUS, &ok);
   assert(ok);
   glUseProgram(prog);
   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, src);
   glUniform1i(glGetUniformLocation(prog, "tex"), 0);
   glGenVertexArrays(1, &vao);
   glBindVertexArray(vao);
   glViewport(0, 0, W, H);
   glDrawArrays(GL_TRIANGLES, 0, 3);
   /* Synchronization and readback belong only to this pixel oracle. */
   glFinish();
   glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, result);
   assert(glGetError() == GL_NO_ERROR);
   assert(!memcmp(result, want, sizeof(want)));

   glDeleteVertexArrays(1, &vao);
   glDeleteProgram(prog);
   glDeleteFramebuffers(1, &fbo);
   glDeleteTextures(1, &dst);
   glDeleteTextures(1, &src);
   glBindFramebuffer(GL_FRAMEBUFFER, 0);
   puts("PASS GLES swizzled frame texture: BGRX uploaded as it is samples as RGBX");
}

int main(void)
{
   static const EGLint config_attrs[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR, EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE};
   static const EGLint context_attrs[] = {EGL_CONTEXT_CLIENT_VERSION, 3,
      EGL_CONTEXT_MINOR_VERSION_KHR, 1, EGL_NONE};
   static const EGLint surface_attrs[] = {EGL_WIDTH, 4, EGL_HEIGHT, 2, EGL_NONE};
   EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   EGLContext context;
   EGLSurface surface;
   EGLConfig config;
   EGLint count;
   GLuint texture, fbo;
   gl_texture_lend_t *list = NULL;
   unsigned char *pixels[2], result[32];
   unsigned i, slot, j;
   assert(display != EGL_NO_DISPLAY && eglInitialize(display, NULL, NULL));
   assert(eglBindAPI(EGL_OPENGL_ES_API));
   assert(eglChooseConfig(display, config_attrs, &config, 1, &count) && count);
   context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attrs);
   assert(context != EGL_NO_CONTEXT);
   surface = eglCreatePbufferSurface(display, config, surface_attrs);
   assert(surface != EGL_NO_SURFACE);
   assert(eglMakeCurrent(display, surface, surface, context));
   rglgen_resolve_symbols(get_proc);
   printf("GLES context: %s\n", glGetString(GL_VERSION));
   assert(gl_query_extension("EXT_buffer_storage"));
   glGenTextures(1, &texture);
   glBindTexture(GL_TEXTURE_2D, texture);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 2, 0,
         GL_RGBA, GL_UNSIGNED_BYTE, NULL);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glGenFramebuffers(1, &fbo);
   glBindFramebuffer(GL_FRAMEBUFFER, fbo);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
         GL_TEXTURE_2D, texture, 0);
   assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
   for (slot = 0; slot < 2; slot++)
   {
      pixels[slot] = (unsigned char*)gl_texture_lend(&list, texture, slot, 16);
      assert(pixels[slot]);
   }
   assert(pixels[0] != pixels[1]);
   for (i = 0; i < 64; i++)
   {
      int loan;
      slot = i % 2;
      assert(gl_texture_lend_ready(list, texture, slot));
      for (j = 0; j < 32; j++)
         pixels[slot][j] = (unsigned char)(i * 7 + j * 13);
      loan = gl_texture_lend_bind(list, texture, pixels[slot]);
      assert(loan == (int)slot);
      glBindTexture(GL_TEXTURE_2D, texture);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 2,
            GL_RGBA, GL_UNSIGNED_BYTE, NULL);
      gl_texture_lend_unbind(list, texture, loan);
      /* Synchronization and readback belong only to this pixel oracle. */
      glFinish();
      glReadPixels(0, 0, 4, 2, GL_RGBA, GL_UNSIGNED_BYTE, result);
      assert(glGetError() == GL_NO_ERROR);
      assert(!memcmp(result, pixels[slot], 32));
   }
   gl_texture_lend_free(&list);
   swizzle_check();
   glDeleteFramebuffers(1, &fbo);
   glDeleteTextures(1, &texture);
   assert(glGetError() == GL_NO_ERROR);
   assert(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
   assert(eglDestroySurface(display, surface));
   assert(eglDestroyContext(display, context));
   assert(eglTerminate(display));
   puts("PASS GLES persistent loans: 64 alternating exact RGBA8 uploads");
   return 0;
}
