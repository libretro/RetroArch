/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifdef NDEBUG
#error Capture retirement acceptance requires assertions
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../../record/record_android_capture_gl.h"
#define D ((EGLDisplay)(uintptr_t)1)
static unsigned images, textures, syncs, fences, waits, flushes, imported;
static unsigned image_calls, bad_image;
static bool bad_destroy, bad_wait, bad_fence, bad_sync;
static GLsync live_fences[32], newest, expected_poll;
static unsigned fence_serial;
static GLenum completion;
static const char *EGLAPIENTRY query(EGLDisplay d, EGLint name)
{
   assert(d == D && name == EGL_EXTENSIONS);
   return "EGL_ANDROID_get_native_client_buffer "
          "EGL_ANDROID_image_native_buffer "
          "EGL_KHR_image_base EGL_ANDROID_native_fence_sync EGL_KHR_wait_sync "
          "EGL_KHR_fence_sync";
}
static EGLClientBuffer EGLAPIENTRY native(const struct AHardwareBuffer *b)
{
   assert(b);
   return (EGLClientBuffer)b;
}
static EGLImageKHR EGLAPIENTRY image(EGLDisplay d, EGLContext ctx, EGLenum type,
      EGLClientBuffer buffer, const EGLint *a)
{
   assert(d == D && ctx == EGL_NO_CONTEXT && buffer);
   assert(type == EGL_NATIVE_BUFFER_ANDROID);
   assert(a[0] == EGL_IMAGE_PRESERVED_KHR && a[1] == EGL_TRUE
         && a[2] == EGL_NONE);
   image_calls++;
   if (image_calls == bad_image)
      return EGL_NO_IMAGE_KHR;
   images++;
   return (EGLImageKHR)(uintptr_t)(100 + image_calls);
}
static EGLBoolean EGLAPIENTRY destroy_image(EGLDisplay d, EGLImageKHR i)
{
   assert(d == D && i && images);
   if (bad_destroy)
      return EGL_FALSE;
   images--;
   return EGL_TRUE;
}
static EGLSyncKHR EGLAPIENTRY sync_new(
      EGLDisplay d, EGLenum type, const EGLint *a)
{
   assert(d == D && type == EGL_SYNC_NATIVE_FENCE_ANDROID);
   assert(a[0] == EGL_SYNC_NATIVE_FENCE_FD_ANDROID && a[1] == 7
         && a[2] == EGL_NONE);
   imported++;
   if (bad_sync)
      return EGL_NO_SYNC_KHR;
   syncs++;
   return (EGLSyncKHR)(uintptr_t)300;
}
static EGLBoolean EGLAPIENTRY sync_delete(EGLDisplay d, EGLSyncKHR s)
{
   assert(d == D && s && syncs);
   if (bad_destroy)
      return EGL_FALSE;
   syncs--;
   return EGL_TRUE;
}
static EGLint EGLAPIENTRY wait_sync(EGLDisplay d, EGLSyncKHR s, EGLint flags)
{
   assert(d == D && s && !flags);
   waits++;
   return bad_wait ? EGL_FALSE : EGL_TRUE;
}
static void GL_APIENTRY gen(GLsizei n, GLuint *t)
{
   assert(n == 1);
   *t = ++textures;
}
static void GL_APIENTRY del(GLsizei n, const GLuint *t)
{
   assert(n == 1 && *t && textures);
   textures--;
}
static void GL_APIENTRY bind(GLenum target, GLuint t)
{
   assert(target == GL_TEXTURE_2D);
   (void)t;
}
static void GL_APIENTRY parameter(GLenum target, GLenum name, GLint v)
{
   assert(target == GL_TEXTURE_2D);
   assert(((name == GL_TEXTURE_MIN_FILTER || name == GL_TEXTURE_MAG_FILTER)
                && v == GL_LINEAR)
         || ((name == GL_TEXTURE_WRAP_S || name == GL_TEXTURE_WRAP_T)
               && v == GL_CLAMP_TO_EDGE));
}
static void GL_APIENTRY target_image(GLenum target, void *i)
{
   assert(target == GL_TEXTURE_2D && i);
}
static GLenum GL_APIENTRY error(void)
{
   return GL_NO_ERROR;
}
static const GLubyte *GL_APIENTRY string(GLenum name)
{
   assert(name == GL_EXTENSIONS);
   return (const GLubyte *)"GL_OES_EGL_image GL_OES_EGL_sync";
}
static GLsync GL_APIENTRY fence(GLenum type, GLbitfield flags)
{
   unsigned i;
   assert(type == GL_SYNC_GPU_COMMANDS_COMPLETE && !flags);
   if (bad_fence)
      return NULL;
   fences++;
   newest = (GLsync)(uintptr_t)(400 + ++fence_serial);
   for (i = 0; i < 32; i++)
      if (!live_fences[i])
      {
         live_fences[i] = newest;
         return newest;
      }
   assert(0);
   return NULL;
}
static GLenum GL_APIENTRY poll(GLsync s, GLbitfield flags, GLuint64 timeout)
{
   assert(s && !flags && !timeout);
   assert(!expected_poll || s == expected_poll);
   return completion;
}
static void GL_APIENTRY delete_fence(GLsync s)
{
   unsigned i;
   assert(s && fences);
   for (i = 0; i < 32; i++)
      if (live_fences[i] == s)
         break;
   assert(i < 32);
   live_fences[i] = NULL;
   fences--;
}
static void GL_APIENTRY flush(void)
{
   flushes++;
}
int main(void)
{
   record_android_capture_gl_ops_t ops;
   record_android_capture_gl_t *c;
   struct AHardwareBuffer *buffers[16];
   unsigned i, j;
   int fd;
   GLuint texture;
   memset(&ops, 0, sizeof(ops));
   ops.query_string = query;
   ops.native_buffer = native;
   ops.create_image = image;
   ops.destroy_image = destroy_image;
   ops.create_sync = sync_new;
   ops.destroy_sync = sync_delete;
   ops.wait_sync = wait_sync;
   ops.gen_textures = gen;
   ops.delete_textures = del;
   ops.bind_texture = bind;
   ops.texture_parameter = parameter;
   ops.image_target = target_image;
   ops.error = error;
   ops.string = string;
   ops.fence = fence;
   ops.poll = poll;
   ops.delete_fence = delete_fence;
   ops.flush = flush;
   for (i = 0; i < 16; i++)
      buffers[i] = (struct AHardwareBuffer *)(uintptr_t)(i + 1);
   assert(record_android_capture_gl_new(D, &ops, buffers, 16, &c));
   assert(images == 16 && textures == 16);
   for (i = 0; i < 1000; i++)
      for (j = 0; j < 16; j++)
      {
         fd = i & 1 ? -1 : 7;
         assert(record_android_capture_gl_begin(c, j, &fd, &texture));
         assert(fd == -2 && texture);
         assert(!record_android_capture_gl_free(c));
         assert(!record_android_capture_gl_begin(c, j, &fd, &texture));
         assert(record_android_capture_gl_end(c, j));
         assert(!record_android_capture_gl_end(c, j));
         completion = GL_TIMEOUT_EXPIRED;
         assert(record_android_capture_gl_poll(c, j) == 0);
         completion = i & 1 ? GL_ALREADY_SIGNALED : GL_CONDITION_SATISFIED;
         assert(record_android_capture_gl_poll(c, j) == 1);
      }
   assert(flushes == 16000 && imported == 8000 && waits == 8000);
   assert(record_android_capture_gl_free(c));
   assert(!images && !textures && !syncs && !fences);
   for (i = 1; i <= 16; i++)
   {
      image_calls = 0;
      bad_image = i;
      assert(!record_android_capture_gl_new(D, &ops, buffers, 16, &c));
      assert(!c && !images && !textures);
   }
   image_calls = 0;
   bad_image = 2;
   bad_destroy = true;
   assert(!record_android_capture_gl_new(D, &ops, buffers, 16, &c));
   assert(c && images == 1);
   bad_destroy = false;
   assert(record_android_capture_gl_free(c));
   bad_image = 0;
   buffers[1] = buffers[0];
   assert(!record_android_capture_gl_new(D, &ops, buffers, 2, &c) && !c);
   buffers[1] = (struct AHardwareBuffer *)(uintptr_t)2;
   assert(!record_android_capture_gl_new(D, &ops, buffers, 17, &c) && !c);
   assert(record_android_capture_gl_new(D, &ops, buffers, 1, &c));
   fd = 7;
   assert(!record_android_capture_gl_begin(c, 1, &fd, &texture) && fd == 7);
   assert(!record_android_capture_gl_begin(c, 0, &fd, NULL) && fd == 7);
   assert(record_android_capture_gl_begin(c, 0, &fd, &texture));
   assert(record_android_capture_gl_end(c, 0));
   completion = GL_WAIT_FAILED;
   assert(record_android_capture_gl_poll(c, 0) == -1);
   assert(!record_android_capture_gl_free(c));
   completion = GL_CONDITION_SATISFIED;
   assert(record_android_capture_gl_poll(c, 0) == 1);
   fd = -1;
   assert(!record_android_capture_gl_begin(c, 0, &fd, &texture) && fd == -1);
   assert(record_android_capture_gl_free(c));
   for (i = 0; i < 4; i++)
   {
      assert(record_android_capture_gl_new(D, &ops, buffers, 1, &c));
      fd = 7;
      bad_sync = i == 0;
      bad_wait = i == 1;
      bad_destroy = i == 2;
      bad_fence = i == 3;
      if (i < 3)
         assert(!record_android_capture_gl_begin(c, 0, &fd, &texture));
      else
      {
         assert(record_android_capture_gl_begin(c, 0, &fd, &texture));
         assert(!record_android_capture_gl_end(c, 0));
         bad_fence = false;
         assert(!record_android_capture_gl_end(c, 0));
         assert(record_android_capture_gl_poll(c, 0) == -1);
         assert(!record_android_capture_gl_free(c));
         textures = 0;
         record_android_capture_gl_after_context_retired(c);
      }
      assert(fd == -2);
      bad_sync = bad_wait = bad_destroy = bad_fence = false;
      assert(record_android_capture_gl_free(c));
      assert(!images && !textures && !syncs && !fences);
   }
   assert(record_android_capture_gl_new(D, &ops, buffers, 2, &c));
   for (i = 0; i < 2; i++)
   {
      fd = -1;
      assert(record_android_capture_gl_begin(c, i, &fd, &texture));
      bad_fence = i == 1;
      assert(record_android_capture_gl_end(c, i) == !bad_fence);
   }
   assert(fences == 1);
   bad_fence = true;
   assert(record_android_capture_gl_recover_step(c) == -1 && fences == 1);
   assert(!record_android_capture_gl_free(c));
   bad_fence = false;
   assert(record_android_capture_gl_recover_step(c) == 0 && fences == 2);
   expected_poll = newest;
   completion = GL_WAIT_FAILED;
   assert(record_android_capture_gl_recover_step(c) == -1 && fences == 2);
   assert(!record_android_capture_gl_free(c));
   completion = GL_TIMEOUT_EXPIRED;
   assert(record_android_capture_gl_recover_step(c) == 0 && fences == 2);
   completion = GL_CONDITION_SATISFIED;
   assert(record_android_capture_gl_recover_step(c) == 1 && !fences);
   expected_poll = NULL;
   assert(record_android_capture_gl_recover_step(c) == 1);
   assert(record_android_capture_gl_poll(c, 0) == 1);
   assert(record_android_capture_gl_poll(c, 1) == 1);
   fd = -1;
   assert(!record_android_capture_gl_begin(c, 0, &fd, &texture));
   assert(record_android_capture_gl_free(c));
   assert(!images && !textures && !syncs && !fences);
   puts("PASS: fixed GLES imports, 16000 GPU-read retirements, terminal "
        "recovery and "
        "partial-fault ownership");
   return 0;
}
