/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "record_android_capture_gl.h"
#include <stdlib.h>
#include <string.h>

typedef struct record_capture_gl_slot
{
   EGLImageKHR image;
   EGLSyncKHR ready;
   GLsync done;
   GLuint texture;
   bool reading, ended;
} record_capture_gl_slot_t;
struct record_android_capture_gl
{
   record_android_capture_gl_ops_t ops;
   record_capture_gl_slot_t slots[16];
   GLsync recovery;
   EGLDisplay display;
   unsigned count;
   bool failed, recovered;
};
static bool record_capture_gl_extension(const char *list, const char *name)
{
   const char *p;
   size_t n = strlen(name);
   if (!list)
      return false;
   for (p = list; (p = strstr(p, name)) != NULL; p += n)
      if ((p == list || p[-1] == ' ') && (!p[n] || p[n] == ' '))
         return true;
   return false;
}
bool record_android_capture_gl_free(record_android_capture_gl_t *c)
{
   unsigned i;
   if (!c)
      return true;
   if (c->recovery)
      return false;
   for (i = 0; i < c->count; i++)
      if (c->slots[i].reading || c->slots[i].done)
         return false;
   c->failed = true;
   for (i = 0; i < c->count; i++)
   {
      record_capture_gl_slot_t *s = &c->slots[i];
      if (s->ready)
      {
         if (!c->ops.destroy_sync(c->display, s->ready))
            return false;
         s->ready = EGL_NO_SYNC_KHR;
      }
      if (s->texture)
      {
         c->ops.delete_textures(1, &s->texture);
         s->texture = 0;
      }
      if (s->image)
      {
         if (!c->ops.destroy_image(c->display, s->image))
            return false;
         s->image = EGL_NO_IMAGE_KHR;
      }
   }
   free(c);
   return true;
}
bool record_android_capture_gl_new(EGLDisplay display,
      const record_android_capture_gl_ops_t *ops,
      struct AHardwareBuffer *const *buffers, unsigned count,
      record_android_capture_gl_t **output)
{
   record_android_capture_gl_t *c;
   const char *extensions;
   unsigned i, j;
   if (!output)
      return false;
   *output = NULL;
   if (display == EGL_NO_DISPLAY || !ops || !buffers || !count || count > 16
         || !ops->query_string || !ops->native_buffer || !ops->create_image
         || !ops->destroy_image || !ops->create_sync || !ops->destroy_sync
         || !ops->wait_sync || !ops->gen_textures || !ops->delete_textures
         || !ops->bind_texture || !ops->texture_parameter || !ops->image_target
         || !ops->error || !ops->string || !ops->fence || !ops->poll
         || !ops->delete_fence || !ops->flush)
      return false;
   extensions = ops->query_string(display, EGL_EXTENSIONS);
   if (!record_capture_gl_extension(
             extensions, "EGL_ANDROID_get_native_client_buffer")
         || !record_capture_gl_extension(
               extensions, "EGL_ANDROID_image_native_buffer")
         || !record_capture_gl_extension(extensions, "EGL_KHR_image_base")
         || !record_capture_gl_extension(
               extensions, "EGL_ANDROID_native_fence_sync")
         || !record_capture_gl_extension(extensions, "EGL_KHR_wait_sync")
         || !record_capture_gl_extension(extensions, "EGL_KHR_fence_sync")
         || !record_capture_gl_extension(
               (const char *)ops->string(GL_EXTENSIONS), "GL_OES_EGL_image")
         || !record_capture_gl_extension(
               (const char *)ops->string(GL_EXTENSIONS), "GL_OES_EGL_sync")
         || ops->error() != GL_NO_ERROR)
      return false;
   for (i = 0; i < count; i++)
   {
      if (!buffers[i])
         return false;
      for (j = 0; j < i; j++)
         if (buffers[i] == buffers[j])
            return false;
   }
   c = (record_android_capture_gl_t *)calloc(1, sizeof(*c));
   if (!c)
      return false;
   *output = c;
   c->ops = *ops;
   c->display = display;
   c->count = count;
   for (i = 0; i < count; i++)
   {
      const EGLint attributes[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
      EGLClientBuffer buffer = ops->native_buffer(buffers[i]);
      record_capture_gl_slot_t *s = &c->slots[i];
      if (!buffer)
         break;
      s->image = ops->create_image(display, EGL_NO_CONTEXT,
            EGL_NATIVE_BUFFER_ANDROID, buffer, attributes);
      if (!s->image)
         break;
      ops->gen_textures(1, &s->texture);
      if (!s->texture)
         break;
      ops->bind_texture(GL_TEXTURE_2D, s->texture);
      ops->texture_parameter(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      ops->texture_parameter(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      ops->texture_parameter(
            GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      ops->texture_parameter(
            GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      ops->image_target(GL_TEXTURE_2D, (void *)s->image);
      if (ops->error() != GL_NO_ERROR)
         break;
   }
   ops->bind_texture(GL_TEXTURE_2D, 0);
   if (i == count)
      return true;
   if (record_android_capture_gl_free(c))
      *output = NULL;
   return false;
}
bool record_android_capture_gl_begin(record_android_capture_gl_t *c,
      unsigned slot, int *owned_fd, GLuint *texture)
{
   record_capture_gl_slot_t *s;
   EGLint attributes[3];
   int fd;
   if (!c || c->failed || slot >= c->count || !owned_fd || !texture
         || *owned_fd < -1)
      return false;
   s = &c->slots[slot];
   if (s->reading || s->done || s->ready)
      return false;
   fd = *owned_fd;
   *owned_fd = -2;
   if (fd >= 0)
   {
      attributes[0] = EGL_SYNC_NATIVE_FENCE_FD_ANDROID;
      attributes[1] = fd;
      attributes[2] = EGL_NONE;
      s->ready = c->ops.create_sync(
            c->display, EGL_SYNC_NATIVE_FENCE_ANDROID, attributes);
      if (!s->ready || !c->ops.wait_sync(c->display, s->ready, 0))
      {
         c->failed = true;
         return false;
      }
      if (!c->ops.destroy_sync(c->display, s->ready))
      {
         c->failed = true;
         return false;
      }
      s->ready = EGL_NO_SYNC_KHR;
   }
   s->reading = true;
   *texture = s->texture;
   return true;
}
bool record_android_capture_gl_end(
      record_android_capture_gl_t *c, unsigned slot)
{
   record_capture_gl_slot_t *s;
   if (!c || slot >= c->count)
      return false;
   s = &c->slots[slot];
   if (!s->reading || s->ended)
      return false;
   s->ended = true;
   s->done = c->ops.fence(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
   c->ops.flush();
   if (!s->done)
   {
      c->failed = true;
      return false;
   }
   return true;
}
int record_android_capture_gl_poll(
      record_android_capture_gl_t *c, unsigned slot)
{
   record_capture_gl_slot_t *s;
   GLenum status;
   if (!c || slot >= c->count)
      return -1;
   s = &c->slots[slot];
   if (!s->reading)
      return 1;
   if (!s->done)
      return c->failed ? -1 : 0;
   status = c->ops.poll(s->done, 0, 0);
   if (status == GL_TIMEOUT_EXPIRED)
      return 0;
   if (status != GL_ALREADY_SIGNALED && status != GL_CONDITION_SATISFIED)
   {
      c->failed = true;
      return -1;
   }
   c->ops.delete_fence(s->done);
   s->done = NULL;
   s->reading = false;
   s->ended = false;
   return 1;
}
int record_android_capture_gl_recover_step(record_android_capture_gl_t *c)
{
   unsigned i;
   GLenum status;
   if (!c)
      return -1;
   c->failed = true;
   if (c->recovered)
      return 1;
   if (!c->recovery)
   {
      c->recovery = c->ops.fence(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
      if (!c->recovery)
         return -1;
      c->ops.flush();
      return 0;
   }
   status = c->ops.poll(c->recovery, 0, 0);
   if (status == GL_TIMEOUT_EXPIRED)
      return 0;
   if (status != GL_ALREADY_SIGNALED && status != GL_CONDITION_SATISFIED)
      return -1;
   c->ops.delete_fence(c->recovery);
   c->recovery = NULL;
   for (i = 0; i < c->count; i++)
   {
      record_capture_gl_slot_t *s = &c->slots[i];
      if (s->done)
         c->ops.delete_fence(s->done);
      s->done = NULL;
      s->reading = s->ended = false;
   }
   c->recovered = true;
   return 1;
}
void record_android_capture_gl_after_context_retired(
      record_android_capture_gl_t *c)
{
   unsigned i;
   if (!c)
      return;
   c->failed = true;
   c->recovery = NULL;
   c->recovered = true;
   for (i = 0; i < c->count; i++)
   {
      c->slots[i].texture = 0;
      c->slots[i].done = NULL;
      c->slots[i].reading = false;
   }
}
