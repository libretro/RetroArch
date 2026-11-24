/* Shipping GL2/GL3 menu callbacks against real desktop and GLES uploads. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <glsym/glsym.h>
#include <gfx/gl_capabilities.h>
#include "../../../gfx/common/gl_common.h"
#include "../../../gfx/video_defines.h"

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
static bool no_storage;
static rglgen_func_t get_proc(const char *name)
{
   if (no_storage && (!strcmp(name, "glBufferStorage")
            || !strcmp(name, "glBufferStorageEXT")))
      return NULL;
#ifdef _WIN32
   {
      PROC proc = wglGetProcAddress(name);
      if (!proc || (uintptr_t)proc <= 3 || (intptr_t)proc == -1)
         proc = GetProcAddress(GetModuleHandleA("opengl32.dll"), name);
      return (rglgen_func_t)proc;
   }
#else
   return (rglgen_func_t)eglGetProcAddress(name);
#endif
}

typedef struct menu_context
{
   gl_frame_mem_t *frame_mem;
   struct { void (*bind_hw_render)(void*, bool); } *ctx_driver;
   void *ctx_data;
   GLuint menu_texture, texture[1];
   unsigned flags, tex_index, menu_texture_dims;
   float menu_texture_alpha;
   bool menu_texture_rgb32, frame_menu_linear_filter;
} gl2_t;
typedef gl2_t gl3_t;
#define GL2_FLAG_SHARED_CONTEXT_USE 1
#define GL2_FLAG_MENU_LINEAR_FILTER 2
#define GL3_FLAG_USE_SHARED_CONTEXT 1
#if BACKEND == 2
static bool gl2_core_context_is_mains(gl2_t *gl) { (void)gl; return true; }
enum gfx_wrap_type { RARCH_WRAP_EDGE };
#define VIDEO_FLAG_USE_RGBA 1
static unsigned video_driver_get_disp_flags(void) { return VIDEO_FLAG_USE_RGBA; }
static GLenum gl2_wrap_type_to_enum(enum gfx_wrap_type wrap)
{ (void)wrap; return GL_CLAMP_TO_EDGE; }
bool gl_check_capability(enum gl_capability_enum cap)
{ (void)cap; return false; }
#define GL2_BIND_TEXTURE(id, wrap, mag, min) do { \
   glBindTexture(GL_TEXTURE_2D, id); \
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap); \
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap); \
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag); \
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min); \
} while (0)
#define RARCH_GL_INTERNAL_FORMAT32 GL_RGBA
#define RARCH_GL_TEXTURE_TYPE32 GL_RGBA
#define RARCH_GL_FORMAT32 GL_UNSIGNED_BYTE
#define menu_alloc gl2_menu_mem_alloc
#define menu_push gl2_set_texture_frame
#define menu_ready gl2_frame_mem_ready
#define menu_free gl2_frame_mem_free
#else
static bool gl3_core_context_is_mains(gl3_t *gl) { (void)gl; return true; }
#define menu_alloc gl3_menu_mem_alloc
#define menu_push gl3_set_texture_frame
#define menu_ready gl3_frame_mem_ready
#define menu_free gl3_frame_mem_free
#endif
static bool expect_mapped;
static unsigned mapped_uploads, cpu_uploads;
static void check_source(const void *pixels)
{
   GLint unpack = 0;
   glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack);
   assert((unpack != 0) == expect_mapped);
   if (expect_mapped)
   {
      assert(!pixels);
      mapped_uploads++;
   }
   else
   {
      assert(pixels);
      cpu_uploads++;
   }
}
#if BACKEND == 2
static void upload_image(GLenum target, GLint level, GLint internal,
      GLsizei width, GLsizei height, GLint border, GLenum format,
      GLenum type, const void *pixels)
{
   check_source(pixels);
   glTexImage2D(target, level, internal, width, height, border, format,
         type, pixels);
}
#define glTexImage2D upload_image
#else
static void upload_subimage(GLenum target, GLint level, GLint x, GLint y,
      GLsizei width, GLsizei height, GLenum format, GLenum type,
      const void *pixels)
{
   check_source(pixels);
   glTexSubImage2D(target, level, x, y, width, height, format, type, pixels);
}
#define glTexSubImage2D upload_subimage
#endif
#include "menu.inc"

int main(void)
{
#ifdef _WIN32
   HWND window;
   HDC dc;
   HGLRC context;
   PIXELFORMATDESCRIPTOR format;
#else
   EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   EGLConfig config;
   EGLContext context;
   EGLSurface surface;
   EGLint count;
#ifdef HAVE_OPENGLES
   const EGLint api = EGL_OPENGL_ES_API, renderable = EGL_OPENGL_ES3_BIT;
   const EGLint context_attrs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
#else
   const EGLint api = EGL_OPENGL_API, renderable = EGL_OPENGL_BIT;
   const EGLint context_attrs[] = { EGL_NONE };
#endif
   EGLint attrs[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
      EGL_RENDERABLE_TYPE, 0, EGL_NONE };
   const EGLint size[] = { EGL_WIDTH, 8, EGL_HEIGHT, 8, EGL_NONE };
#endif
   gl2_t gl;
   uintptr_t ids[2];
   unsigned char *pixels[2], raw[80], result[80];
   unsigned rgb, frame, i;
   GLuint fbo;
   GLint unpack;
   memset(&gl, 0, sizeof(gl));
#ifdef _WIN32
   window = CreateWindowExA(0, "STATIC", "GL menu loan test", WS_POPUP,
         0, 0, 8, 8, NULL, NULL, GetModuleHandle(NULL), NULL);
   assert(window);
   dc = GetDC(window);
   memset(&format, 0, sizeof(format));
   format.nSize = sizeof(format);
   format.nVersion = 1;
   format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
   format.iPixelType = PFD_TYPE_RGBA;
   format.cColorBits = 32;
   assert(SetPixelFormat(dc, ChoosePixelFormat(dc, &format), &format));
   context = wglCreateContext(dc);
   assert(context && wglMakeCurrent(dc, context));
#else
   attrs[3] = renderable;
   assert(eglInitialize(display, NULL, NULL) && eglBindAPI(api));
   assert(eglChooseConfig(display, attrs, &config, 1, &count) && count);
   context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attrs);
   surface = eglCreatePbufferSurface(display, config, size);
   assert(eglMakeCurrent(display, surface, surface, context));
#endif
   rglgen_resolve_symbols(get_proc);
   printf("GL%d menu: %s\n", BACKEND, glGetString(GL_VERSION));
   no_storage = true;
   rglgen_resolve_symbols(get_proc);
   ids[0] = 0;
   assert(!menu_alloc(&gl, sizeof(raw), &ids[0]) && !gl.frame_mem);
   no_storage = false;
   rglgen_resolve_symbols(get_proc);
   glGenFramebuffers(1, &fbo);
   for (i = 0; i < 2; i++)
   {
      pixels[i] = (unsigned char*)menu_alloc(&gl, sizeof(raw), &ids[i]);
      assert(pixels[i] && menu_ready(&gl, ids[i]));
   }
   for (rgb = 0; rgb < 2; rgb++)
      for (frame = 0; frame < 24; frame++)
      {
         unsigned slot = frame & 1, w = frame & 2 ? 5 : 3;
         unsigned bytes = w * 2 * (rgb ? 4 : 2);
         assert(menu_ready(&gl, ids[slot]));
         for (i = 0; i < bytes; i++)
            pixels[slot][i] = (unsigned char)(frame * 7 + i * 13);
         memcpy(raw, pixels[slot], bytes);
         expect_mapped = true;
         menu_push(&gl, pixels[slot], rgb != 0, VIDEO_SCALE_PACK(w, 2), 1);
         glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack);
         assert(!unpack);
         glBindFramebuffer(GL_FRAMEBUFFER, fbo);
         glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
               GL_TEXTURE_2D, gl.menu_texture, 0);
         assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
         glReadPixels(0, 0, w, 2, GL_RGBA, GL_UNSIGNED_BYTE, result);
         for (i = 0; i < w * 2; i++)
         {
            if (rgb)
               assert(!memcmp(result + i * 4, raw + i * 4, 4));
            else
            {
               uint16_t value;
               memcpy(&value, raw + i * 2, 2);
               assert(result[i*4] == ((value >> 12) & 15)*17);
               assert(result[i*4+1] == ((value >> 8) & 15)*17);
               assert(result[i*4+2] == ((value >> 4) & 15)*17);
               assert(result[i*4+3] == (value & 15)*17);
            }
         }
         assert(menu_ready(&gl, ids[slot]));
         /* Unregistered pointers still upload from ordinary CPU memory. */
         expect_mapped = false;
         menu_push(&gl, raw, rgb != 0, VIDEO_SCALE_PACK(w, 2), .5f);
         assert(glGetError() == GL_NO_ERROR);
      }
   assert(mapped_uploads == 48 && cpu_uploads == 48);
   for (i = 0; i < 2; i++)
      menu_free(&gl, ids[i]);
   gl_frame_mem_destroy(&gl.frame_mem);
   glDeleteTextures(1, &gl.menu_texture);
   glDeleteFramebuffers(1, &fbo);
   assert(glGetError() == GL_NO_ERROR);
#ifdef _WIN32
   wglMakeCurrent(NULL, NULL);
   wglDeleteContext(context);
   ReleaseDC(window, dc);
   DestroyWindow(window);
#else
   assert(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
   eglDestroySurface(display, surface);
   eglDestroyContext(display, context);
   eglTerminate(display);
#endif
   puts("PASS menu persistent uploads: 4444, RGBA8, resizing, retirement and raw fallback");
   return 0;
}
