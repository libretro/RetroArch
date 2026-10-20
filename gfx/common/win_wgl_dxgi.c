#if defined(_WIN32) && !defined(__WINRT__)
#include <windows.h>
#include <GL/gl.h>
#include <stdlib.h>
#include <stdint.h>
#include "win_wgl_dxgi.h"

typedef void (APIENTRY *create_objects_t)(GLsizei, GLuint *);
typedef void (APIENTRY *delete_objects_t)(GLsizei, const GLuint *);
typedef void (APIENTRY *get_luid_t)(GLenum, GLubyte *);
typedef void (APIENTRY *import_memory_t)(GLuint, uint64_t, GLenum, void *);
typedef void (APIENTRY *memory_parameter_t)(GLuint, GLenum, const GLint *);
typedef void (APIENTRY *texture_storage_t)(GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLuint, uint64_t);
typedef void (APIENTRY *import_semaphore_t)(GLuint, GLenum, void *);
typedef void (APIENTRY *semaphore_parameter_t)(GLuint, GLenum, const uint64_t *);
typedef void (APIENTRY *semaphore_op_t)(GLuint, GLuint, const GLuint *, GLuint, const GLuint *, const GLenum *);
typedef void (APIENTRY *bind_framebuffer_t)(GLenum, GLuint);
typedef void (APIENTRY *attach_texture_t)(GLenum, GLenum, GLenum, GLuint, GLint);
typedef GLenum (APIENTRY *check_framebuffer_t)(GLenum);
typedef void (APIENTRY *blit_framebuffer_t)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);

struct ra_wgl_dxgi
{
   ra_dxgi_bridge_t *bridge;
   GLuint memory[2], texture[2], framebuffer[2], ready, done;
   unsigned width, height, format, internal_format;
   uint64_t ready_value;
   create_objects_t create_memory, gen_semaphore, gen_framebuffer;
   delete_objects_t delete_memory, delete_semaphore, delete_framebuffer;
   get_luid_t get_luid;
   import_memory_t import_memory;
   memory_parameter_t memory_parameter;
   texture_storage_t texture_storage;
   import_semaphore_t import_semaphore;
   semaphore_parameter_t semaphore_parameter;
   semaphore_op_t wait_semaphore, signal_semaphore;
   bind_framebuffer_t bind_framebuffer;
   attach_texture_t attach_texture;
   check_framebuffer_t check_framebuffer;
   blit_framebuffer_t blit_framebuffer;
};

#define RA_WGL_FRAMEBUFFER_SRGB 0x8DB9
#define RA_WGL_READ_FRAMEBUFFER 0x8CA8
#define RA_WGL_DRAW_FRAMEBUFFER 0x8CA9
#define RA_WGL_READ_FRAMEBUFFER_BINDING 0x8CAA
#define RA_WGL_DRAW_FRAMEBUFFER_BINDING 0x8CA6
#define RA_WGL_COLOR_ATTACHMENT0 0x8CE0
#define RA_WGL_FRAMEBUFFER_COMPLETE 0x8CD5
#define RA_WGL_LAYOUT_GENERAL 0x958D
#define RA_WGL_DEVICE_LUID 0x9599
#define RA_WGL_D3D12_RESOURCE 0x958A
#define RA_WGL_D3D12_FENCE 0x9594
#define RA_WGL_FENCE_VALUE 0x9595
#define RA_WGL_DEDICATED_MEMORY 0x9581

int ra_wgl_dxgi_presentable(ra_wgl_dxgi_t *p)
{ return ra_dxgi_bridge_presentable(p->bridge); }

static void ra_wgl_dxgi_destroy_images(ra_wgl_dxgi_t *p)
{
   GLint old_draw;
   unsigned i;
   if ((p->framebuffer[0] || p->framebuffer[1])
         && p->bind_framebuffer && p->attach_texture)
   {
      glGetIntegerv(RA_WGL_DRAW_FRAMEBUFFER_BINDING, &old_draw);
      for (i = 0; i < 2; i++)
         if (p->framebuffer[i])
         {
            p->bind_framebuffer(RA_WGL_DRAW_FRAMEBUFFER, p->framebuffer[i]);
            p->attach_texture(RA_WGL_DRAW_FRAMEBUFFER, RA_WGL_COLOR_ATTACHMENT0,
                  GL_TEXTURE_2D, 0, 0);
         }
      p->bind_framebuffer(RA_WGL_DRAW_FRAMEBUFFER, old_draw);
   }
   glDeleteTextures(2, p->texture);
   if (p->delete_memory) p->delete_memory(2, p->memory);
   memset(p->texture, 0, sizeof(p->texture));
   memset(p->memory, 0, sizeof(p->memory));
}

void ra_wgl_dxgi_destroy(ra_wgl_dxgi_t *p)
{
   if (!p) return;
   glFinish();
   if (p->delete_framebuffer) p->delete_framebuffer(2, p->framebuffer);
   memset(p->framebuffer, 0, sizeof(p->framebuffer));
   ra_wgl_dxgi_destroy_images(p);
   if (p->delete_semaphore)
   {
      p->delete_semaphore(1, &p->ready);
      p->delete_semaphore(1, &p->done);
   }
   ra_dxgi_bridge_destroy(p->bridge);
   free(p);
}

static int ra_wgl_dxgi_create_images(ra_wgl_dxgi_t *p,
      unsigned width, unsigned height, unsigned internal_format)
{
   GLint old_texture, old_draw, dedicated = GL_TRUE;
   unsigned i;
   p->create_memory(2, p->memory);
   glGenTextures(2, p->texture);
   glGetIntegerv(GL_TEXTURE_BINDING_2D, &old_texture);
   glGetIntegerv(RA_WGL_DRAW_FRAMEBUFFER_BINDING, &old_draw);
   for (i = 0; i < 2; i++)
   {
      p->memory_parameter(p->memory[i], RA_WGL_DEDICATED_MEMORY, &dedicated);
      p->import_memory(p->memory[i], 0, RA_WGL_D3D12_RESOURCE,
            ra_dxgi_bridge_resource(p->bridge, i));
      glBindTexture(GL_TEXTURE_2D, p->texture[i]);
      p->texture_storage(GL_TEXTURE_2D, 1, internal_format, width, height,
            p->memory[i], 0);
      if (!p->framebuffer[i]) p->gen_framebuffer(1, &p->framebuffer[i]);
      p->bind_framebuffer(RA_WGL_DRAW_FRAMEBUFFER, p->framebuffer[i]);
      p->attach_texture(RA_WGL_DRAW_FRAMEBUFFER, RA_WGL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
            p->texture[i], 0);
      if (p->check_framebuffer(RA_WGL_DRAW_FRAMEBUFFER) != RA_WGL_FRAMEBUFFER_COMPLETE) break;
   }
   glBindTexture(GL_TEXTURE_2D, old_texture);
   p->bind_framebuffer(RA_WGL_DRAW_FRAMEBUFFER, old_draw);
   if (i != 2 || glGetError() != GL_NO_ERROR) return 0;
   p->width = width; p->height = height; p->internal_format = internal_format;
   return 1;
}

int ra_wgl_dxgi_resize(ra_wgl_dxgi_t *p, unsigned width, unsigned height,
      unsigned format, unsigned internal_format)
{
   if (!p || !width || !height || format != p->format
         || internal_format != p->internal_format) return 0;
   if (p->width == width && p->height == height)
      return !ra_dxgi_bridge_failed(p->bridge);
   glFinish();
   ra_wgl_dxgi_destroy_images(p);
   if (!ra_dxgi_bridge_resize(p->bridge, width, height, format)) return 0;
   return ra_wgl_dxgi_create_images(p, width, height, internal_format);
}

ra_wgl_dxgi_t *ra_wgl_dxgi_create(HWND window, unsigned width,
      unsigned height, unsigned format, unsigned internal_format)
{
   ra_wgl_dxgi_t *p = (ra_wgl_dxgi_t *)calloc(1, sizeof(*p));
   GLubyte luid[8];
   if (!p) return NULL;
#define LOAD(field, type, name) do { \
   PROC address = wglGetProcAddress(name); \
   if (!address || address == (PROC)1 || address == (PROC)2 \
         || address == (PROC)3 || address == (PROC)-1) goto error; \
   p->field = (type)address; \
} while (0)
   LOAD(create_memory, create_objects_t, "glCreateMemoryObjectsEXT");
   LOAD(delete_memory, delete_objects_t, "glDeleteMemoryObjectsEXT");
   LOAD(get_luid, get_luid_t, "glGetUnsignedBytevEXT");
   LOAD(import_memory, import_memory_t, "glImportMemoryWin32HandleEXT");
   LOAD(memory_parameter, memory_parameter_t, "glMemoryObjectParameterivEXT");
   LOAD(texture_storage, texture_storage_t, "glTexStorageMem2DEXT");
   LOAD(gen_semaphore, create_objects_t, "glGenSemaphoresEXT");
   LOAD(delete_semaphore, delete_objects_t, "glDeleteSemaphoresEXT");
   LOAD(import_semaphore, import_semaphore_t, "glImportSemaphoreWin32HandleEXT");
   LOAD(semaphore_parameter, semaphore_parameter_t, "glSemaphoreParameterui64vEXT");
   LOAD(wait_semaphore, semaphore_op_t, "glWaitSemaphoreEXT");
   LOAD(signal_semaphore, semaphore_op_t, "glSignalSemaphoreEXT");
   LOAD(gen_framebuffer, create_objects_t, "glGenFramebuffers");
   LOAD(delete_framebuffer, delete_objects_t, "glDeleteFramebuffers");
   LOAD(bind_framebuffer, bind_framebuffer_t, "glBindFramebuffer");
   LOAD(attach_texture, attach_texture_t, "glFramebufferTexture2D");
   LOAD(check_framebuffer, check_framebuffer_t, "glCheckFramebufferStatus");
   LOAD(blit_framebuffer, blit_framebuffer_t, "glBlitFramebuffer");
#undef LOAD
   p->get_luid(RA_WGL_DEVICE_LUID, luid);
   if (glGetError() != GL_NO_ERROR) goto error;
   p->bridge = ra_dxgi_bridge_create(window, width, height, format, luid);
   if (!p->bridge) goto error;
   p->format = format;
   p->gen_semaphore(1, &p->ready);
   p->gen_semaphore(1, &p->done);
   p->import_semaphore(p->ready, RA_WGL_D3D12_FENCE, ra_dxgi_bridge_ready_fence(p->bridge));
   p->import_semaphore(p->done, RA_WGL_D3D12_FENCE, ra_dxgi_bridge_done_fence(p->bridge));
   if (!ra_wgl_dxgi_create_images(p, width, height, internal_format)) goto error;
   return p;
error:
   ra_wgl_dxgi_destroy(p);
   return NULL;
}

int ra_wgl_dxgi_present(ra_wgl_dxgi_t *p, unsigned interval)
{
   unsigned index = ra_dxgi_bridge_index(p->bridge, interval);
   uint64_t last_use;
   GLenum layout = RA_WGL_LAYOUT_GENERAL;
   GLint old_read, old_draw, old_buffer;
   GLboolean scissor, srgb;
   if (index >= 2 || !ra_dxgi_bridge_wait_target(p->bridge, index))
      return ra_dxgi_bridge_failed(p->bridge) ? 0 : -1;
   last_use = ra_dxgi_bridge_last_use(p->bridge, index);
   p->semaphore_parameter(p->done, RA_WGL_FENCE_VALUE, &last_use);
   p->wait_semaphore(p->done, 0, NULL, 1, &p->texture[index], &layout);
   glGetIntegerv(RA_WGL_READ_FRAMEBUFFER_BINDING, &old_read);
   glGetIntegerv(RA_WGL_DRAW_FRAMEBUFFER_BINDING, &old_draw);
   p->bind_framebuffer(RA_WGL_READ_FRAMEBUFFER, 0);
   glGetIntegerv(GL_READ_BUFFER, &old_buffer);
   glReadBuffer(GL_BACK);
   p->bind_framebuffer(RA_WGL_DRAW_FRAMEBUFFER, p->framebuffer[index]);
   scissor = glIsEnabled(GL_SCISSOR_TEST);
   glDisable(GL_SCISSOR_TEST);
   srgb = glIsEnabled(RA_WGL_FRAMEBUFFER_SRGB);
   glDisable(RA_WGL_FRAMEBUFFER_SRGB);
   p->blit_framebuffer(0, 0, p->width, p->height, 0, p->height, p->width, 0,
         GL_COLOR_BUFFER_BIT, GL_NEAREST);
   if (scissor) glEnable(GL_SCISSOR_TEST);
   if (srgb) glEnable(RA_WGL_FRAMEBUFFER_SRGB);
   glReadBuffer(old_buffer);
   p->bind_framebuffer(RA_WGL_READ_FRAMEBUFFER, old_read);
   p->bind_framebuffer(RA_WGL_DRAW_FRAMEBUFFER, old_draw);
   ++p->ready_value;
   p->semaphore_parameter(p->ready, RA_WGL_FENCE_VALUE, &p->ready_value);
   p->signal_semaphore(p->ready, 0, NULL, 1, &p->texture[index], &layout);
   glFlush();
   if (glGetError() != GL_NO_ERROR) return 0;
   return ra_dxgi_bridge_present(p->bridge, p->ready_value, interval);
}

#endif
