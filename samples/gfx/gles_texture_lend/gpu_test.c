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
