/* Real GLES storage ownership, retained pixels and bounded reuse. */
#include <EGL/egl.h>
#include <EGL/eglext.h>
#ifdef HAVE_OPENGLES3
#include <GLES3/gl3.h>
#else
#include <GLES2/gl2.h>
#endif
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define INLINE inline
#define GFX_MAX_FRAME_HISTORY 8
#include "../../../gfx/common/gl_media_pool.h"
static GLuint retained[2];
static bool blocked;
static bool referenced(void *unused, GLuint texture)
{
   (void)unused;
   return blocked || texture == retained[0] || texture == retained[1];
}
static void upload(GLuint texture, unsigned value)
{
   unsigned char pixels[16];
   memset(pixels, value, sizeof(pixels));
   glBindTexture(GL_TEXTURE_2D, texture);
   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}
static void pixel(GLuint framebuffer, GLuint texture, unsigned expected)
{
   unsigned char pixels[16];
   unsigned i;
   glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
   assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
   glReadPixels(0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
   for (i = 0; i < sizeof(pixels); i++) assert(pixels[i] == expected);
}
int main(void)
{
   PFNEGLGETPLATFORMDISPLAYEXTPROC platform = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
   EGLDisplay display = platform(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
   EGLConfig config;
   EGLint count;
   EGLint attributes[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE};
   EGLint dimensions[] = {EGL_WIDTH, 2, EGL_HEIGHT, 2, EGL_NONE};
#ifdef HAVE_OPENGLES3
   EGLint version[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
   unsigned capabilities = GL_MEDIA_POOL_SWIZZLE | GL_MEDIA_POOL_UNPACK;
   GLint format = GL_RGBA8;
#else
   EGLint version[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
   unsigned capabilities = 0;
   GLint format = GL_RGBA;
#endif
   EGLSurface surface;
   EGLContext context;
   gl_media_pool_t *pool = NULL;
   GLuint texture, framebuffer, sentinel, destination;
#ifdef HAVE_OPENGLES3
   GLuint read_framebuffer;
#endif
   GLint bound, filters[4];
   unsigned frame;
   assert(eglInitialize(display, NULL, NULL));
   assert(eglBindAPI(EGL_OPENGL_ES_API));
   assert(eglChooseConfig(display, attributes, &config, 1, &count) && count);
   surface = eglCreatePbufferSurface(display, config, dimensions);
   context = eglCreateContext(display, config, EGL_NO_CONTEXT, version);
   assert(eglMakeCurrent(display, surface, surface, context));
   glGenTextures(1, &texture);
   glBindTexture(GL_TEXTURE_2D, texture);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   glTexImage2D(GL_TEXTURE_2D, 0, format, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
   upload(texture, 17);
   retained[0] = texture;
   /* Unknown producers must retain the established snapshot fallback. */
   assert(gl_media_pool_prepare_ex(&pool, texture, 2, 2, referenced, NULL, capabilities) == texture);
   glGenTextures(1, &sentinel);
   glBindTexture(GL_TEXTURE_2D, sentinel);
   gl_media_pool_register(&pool, texture, 2, 2, format, GL_RGBA, GL_UNSIGNED_BYTE, true, true, capabilities);
   glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
   assert((GLuint)bound == sentinel);
   glGenFramebuffers(1, &framebuffer);
#ifdef HAVE_OPENGLES3
   glGenFramebuffers(1, &read_framebuffer);
#endif
   for (frame = 1; frame < 100; frame++)
   {
      glBindTexture(GL_TEXTURE_2D, sentinel);
      glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
#ifdef HAVE_OPENGLES3
      glBindFramebuffer(GL_READ_FRAMEBUFFER, read_framebuffer);
#endif
      destination = gl_media_pool_prepare_ex(&pool, texture, 2, 2, referenced, NULL, capabilities);
      assert(destination && destination != retained[0] && destination != retained[1]);
      glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
      assert((GLuint)bound == sentinel);
      glGetIntegerv(GL_FRAMEBUFFER_BINDING, &bound);
      assert((GLuint)bound == framebuffer);
#ifdef HAVE_OPENGLES3
      glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &bound);
      assert((GLuint)bound == read_framebuffer);
#endif
      assert(gl_media_pool_resolve(pool, texture) == retained[0]);
      upload(destination, 17 + frame);
      pixel(framebuffer, retained[0], 16 + frame);
      if (frame > 1) pixel(framebuffer, retained[1], 15 + frame);
      gl_media_pool_publish(pool, texture, destination);
      retained[1] = retained[0]; retained[0] = destination;
      assert(pool->count <= 3);
   }
#ifdef HAVE_OPENGLES3
   /* Preserve HDR values outside normalized RGBA8, without conversion. */
   {
      GLuint hdr, next;
      const unsigned short values[4] = {0xb800, 0x3e00, 0x3800, 0x3c00};
      unsigned short result[4];
      glGenTextures(1, &hdr);
      glBindTexture(GL_TEXTURE_2D, hdr);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 1, 1, 0, GL_RGBA, GL_HALF_FLOAT, values);
      gl_media_pool_register(&pool, hdr, 1, 1, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, true, false, capabilities);
      retained[0] = hdr;
      next = gl_media_pool_prepare_ex(&pool, hdr, 1, 1, referenced, NULL, capabilities);
      assert(next && next != hdr);
      glBindTexture(GL_TEXTURE_2D, next);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_HALF_FLOAT, values);
      glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, next, 0);
      assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
      glReadPixels(0, 0, 1, 1, GL_RGBA, GL_HALF_FLOAT, result);
      assert(!memcmp(values, result, sizeof(values)));
      assert(gl_media_pool_forget(&pool, hdr, NULL, NULL));
   }
#endif
   blocked = true;
   while (pool->count < GFX_MAX_FRAME_HISTORY + 2)
      assert(gl_media_pool_prepare_ex(&pool, texture, 2, 2, referenced, NULL, capabilities));
   assert(!gl_media_pool_prepare_ex(&pool, texture, 2, 2, referenced, NULL, capabilities));
   assert(glGetError() == GL_NO_ERROR);
   memcpy(filters, pool->filter, sizeof(filters));
   gl_media_pool_sync_sampler(pool, filters);
   gl_media_pool_free(&pool);
   glDeleteTextures(1, &sentinel);
   glDeleteFramebuffers(1, &framebuffer);
#ifdef HAVE_OPENGLES3
   glDeleteFramebuffers(1, &read_framebuffer);
#endif
   eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
   eglDestroyContext(display, context); eglDestroySurface(display, surface); eglTerminate(display);
   puts("GLES pool: retained pixels, bounded warm reuse, backpressure and state PASS");
   return 0;
}
