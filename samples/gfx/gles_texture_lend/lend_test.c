/* Scripted GLES context: unsupported contexts never query a texture or
 * allocate a buffer, and outstanding uploads never grant a writable slot. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HAVE_OPENGLES
#define HAVE_OPENGLES3
#include "gl_common.h"
#ifndef HAVE_GL_TEXTURE_LEND
#error GLES3 must compile texture lending
#endif

typedef unsigned GLenum;
typedef unsigned GLuint;
typedef unsigned GLbitfield;
typedef int GLint;
typedef int GLsizei;
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;
typedef uint64_t GLuint64;
typedef void *GLsync;
#define GL_VERSION 1
#define GL_TEXTURE_2D 2
#define GL_TIMEOUT_EXPIRED 3
#define GL_WAIT_FAILED 4

static const char *version = "OpenGL ES 3.1 mock";
static bool extension = true;
static unsigned queries, allocations, deletions, maps, fence_deletions;
static unsigned bound, next_buffer;
static unsigned fail_map;
static bool fail_fence;
static GLenum wait_result;
static GLint width = 4, height = 2, format = 0x8058;
static unsigned char storage[16][64];

static const unsigned char *glGetString(GLenum name)
{
   assert(name == GL_VERSION);
   return (const unsigned char*)version;
}

bool gl_query_extension(const char *name)
{
   assert(!strcmp(name, "EXT_buffer_storage"));
   return extension;
}

static void query(GLenum target, GLint level, GLenum name, GLint *value)
{
   assert(target == GL_TEXTURE_2D && level == 0);
   queries++;
   switch (name)
   {
      case 0x1000: *value = width; break;
      case 0x1001: *value = height; break;
      case 0x1003: *value = format; break;
      default: abort();
   }
}
static void (*glGetTexLevelParameteriv)(GLenum, GLint, GLenum, GLint*) = query;

static void glBindTexture(GLenum target, GLuint id)
{
   assert(target == GL_TEXTURE_2D);
   (void)id;
}
static void glGenBuffers(GLsizei count, GLuint *ids)
{
   GLsizei i;
   allocations += count;
   for (i = 0; i < count; i++)
      ids[i] = ++next_buffer;
   assert(next_buffer < 16);
}
static void glDeleteBuffers(GLsizei count, const GLuint *ids)
{
   GLsizei i;
   deletions += count;
   for (i = 0; i < count; i++)
      memset(storage[ids[i]], 0xdd, sizeof(storage[0]));
}
static void glBindBuffer(GLenum target, GLuint id)
{
   assert(target == 0x88ec);
   bound = id;
}
static void buffer_storage(GLenum target, GLsizeiptr size,
      const void *data, GLbitfield flags)
{
   assert(target == 0x88ec && bound && size <= 64 && !data);
   assert((flags & 0xc3) == 0xc3);
}
static void (*glBufferStorageEXT)(GLenum, GLsizeiptr, const void*, GLbitfield)
   = buffer_storage;
static void *glMapBufferRange(GLenum target, GLintptr offset,
      GLsizeiptr size, GLbitfield flags)
{
   assert(target == 0x88ec && bound && !offset && size <= 64);
   assert((flags & 0xc3) == 0xc3);
   maps++;
   return maps == fail_map ? NULL : storage[bound];
}
static GLsync glFenceSync(GLenum condition, GLbitfield flags)
{
   assert(condition == 0x9117 && !flags);
   return fail_fence ? NULL : (GLsync)(uintptr_t)1;
}
static GLenum glClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)
{
   assert(sync && !flags && !timeout);
   return wait_result;
}
static void glDeleteSync(GLsync sync)
{
   assert(sync);
   fence_deletions++;
}

#include "lend.inc"

static void refused(gl_texture_lend_t **list)
{
   unsigned q = queries, a = allocations;
   assert(!gl_texture_lend(list, 7, 0, 16));
   assert(!*list && queries == q && allocations == a);
}

int main(void)
{
   gl_texture_lend_t *list = NULL;
   void *a, *b;
   const char *unsupported[] = {NULL, "", "OpenGL ES 2.0", "OpenGL ES 3.0",
      "OpenGL ES 3", "OpenGL ES 3.", "4.6 desktop"};
   unsigned i;
   for (i = 0; i < sizeof(unsupported) / sizeof(unsupported[0]); i++)
   {
      version = unsupported[i];
      refused(&list);
   }
   version = "OpenGL ES 3.2 mock";
   extension = false;
   refused(&list);
   extension = true;
   glBufferStorageEXT = NULL;
   refused(&list);
   glBufferStorageEXT = buffer_storage;
   glGetTexLevelParameteriv = NULL;
   refused(&list);
   glGetTexLevelParameteriv = query;
   assert(!gl_texture_lend(&list, 7, 0, 12));
   assert(!list && !allocations);
   fail_map = 2;
   assert(!gl_texture_lend(&list, 7, 0, 16));
   assert(!list && allocations == 2 && deletions == 2 && !bound);
   fail_map = 0;
   a = gl_texture_lend(&list, 7, 0, 16);
   b = gl_texture_lend(&list, 7, 1, 16);
   assert(a && b && a != b && allocations == 4 && maps == 4);
   assert(gl_texture_lend(&list, 7, 0, 16) == a);
   assert(!gl_texture_lend(&list, 7, 0, 12));
   assert(!gl_texture_lend(&list, 7, 2, 16));
   assert(gl_texture_lend_bind(list, 7, a) == 0 && bound);
   memset(a, 0x5a, 32);
   gl_texture_lend_unbind(list, 7, 0);
   assert(!bound);
   wait_result = GL_TIMEOUT_EXPIRED;
   assert(!gl_texture_lend_ready(list, 7, 0));
   assert(gl_texture_lend_bind(list, 7, a) == -2 && !bound);
   assert(gl_texture_lend_ready(list, 7, 1));
   wait_result = GL_WAIT_FAILED;
   assert(!gl_texture_lend_ready(list, 7, 0));
   wait_result = GL_ALREADY_SIGNALED;
   assert(gl_texture_lend_ready(list, 7, 0) && fence_deletions == 1);
   assert(gl_texture_lend(&list, 7, 0, 16) == a);
   assert(gl_texture_lend_bind(list, 7, b) == 1);
   fail_fence = true;
   gl_texture_lend_unbind(list, 7, 1);
   assert(!gl_texture_lend_ready(list, 7, 1));
   fail_fence = false;
   wait_result = GL_TIMEOUT_EXPIRED;
   assert(!gl_texture_lend_ready(list, 7, 1));
   gl_texture_lend_forget(&list, 7);
   assert(!list && allocations == deletions && fence_deletions == 2);
   assert(gl_texture_lend_ready(list, 7, 0));
   assert(gl_texture_lend_bind(list, 7, a) == -1);
   assert(gl_texture_lend(&list, 9, 0, 16));
   gl_texture_lend_free(&list);
   assert(!list && allocations == deletions);
   puts("PASS GLES loan capability, fallback, partial allocation, busy and retirement");
   return 0;
}
