/* The gl2 driver's PBO readback ring against a scripted GL: fences are
 * handed out and signalled by the test, and the map is counted, so a
 * read that waits on the GPU, or maps a slot whose fence has not
 * signalled, is a failure. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../gfx/drivers/gl2.c"

#define W 64
#define H 32

/* The scripted GL. */
#define MAX_SYNCS 64
static int      sync_signalled[MAX_SYNCS];
static int      sync_live[MAX_SYNCS];
static unsigned syncs_created;
static unsigned syncs_deleted;
static unsigned maps;
static uint32_t pixels[W * H];

static GLsync fake_fence_sync(GLenum condition, GLbitfield flags)
{
   unsigned id = ++syncs_created;
   (void)condition; (void)flags;
   if (id >= MAX_SYNCS)
      abort();
   sync_live[id]      = 1;
   sync_signalled[id] = 0;
   return (GLsync)(uintptr_t)id;
}

static GLenum fake_client_wait_sync(GLsync sync, GLbitfield flags,
      GLuint64 timeout)
{
   unsigned id = (unsigned)(uintptr_t)sync;
   (void)flags;
   if (timeout != 0)
   {
      printf("  [FAIL] glClientWaitSync asked to wait\n");
      exit(1);
   }
   if (!sync_live[id])
   {
      printf("  [FAIL] glClientWaitSync on a deleted sync\n");
      exit(1);
   }
   return sync_signalled[id] ? GL_ALREADY_SIGNALED : GL_TIMEOUT_EXPIRED;
}

static void fake_delete_sync(GLsync sync)
{
   unsigned id = (unsigned)(uintptr_t)sync;
   if (!sync_live[id])
   {
      printf("  [FAIL] glDeleteSync twice\n");
      exit(1);
   }
   sync_live[id] = 0;
   syncs_deleted++;
}

static void fake_gen_buffers(GLsizei n, GLuint *ids)
{
   static GLuint next = 1;
   GLsizei i;
   for (i = 0; i < n; i++)
      ids[i] = next++;
}

static void fake_delete_buffers(GLsizei n, const GLuint *ids)
{
   (void)n; (void)ids;
}

static void fake_bind_buffer(GLenum target, GLuint id)
{
   (void)target; (void)id;
}

static void fake_buffer_data(GLenum target, GLsizeiptr size,
      const void *data, GLenum usage)
{
   (void)target; (void)size; (void)data; (void)usage;
}

static void *fake_map_buffer(GLenum target, GLenum access)
{
   (void)target; (void)access;
   maps++;
   return pixels;
}

static GLboolean fake_unmap_buffer(GLenum target)
{
   (void)target;
   return GL_TRUE;
}

/* GL 1.1 entry points the driver links directly. */
void glPixelStorei(GLenum pname, GLint param) { (void)pname; (void)param; }
void glReadBuffer(GLenum mode) { (void)mode; }
void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
      GLenum format, GLenum type, GLvoid *data)
{ (void)x; (void)y; (void)width; (void)height; (void)format; (void)type; (void)data; }
void glBindTexture(GLenum t, GLuint i) { (void)t; (void)i; }
void glBlendFunc(GLenum a, GLenum b) { (void)a; (void)b; }
void glClear(GLbitfield m) { (void)m; }
void glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a) { (void)r; (void)g; (void)b; (void)a; }
void glCopyTexSubImage2D(GLenum a, GLint b, GLint c, GLint d, GLint e, GLint f, GLsizei g, GLsizei h)
{ (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; (void)h; }
void glDeleteTextures(GLsizei n, const GLuint *t) { (void)n; (void)t; }
void glDisable(GLenum c) { (void)c; }
void glDrawArrays(GLenum m, GLint f, GLsizei c) { (void)m; (void)f; (void)c; }
void glEnable(GLenum c) { (void)c; }
void glFinish(void) { printf("  [FAIL] glFinish\n"); exit(1); }
void glFlush(void) { }
void glGenTextures(GLsizei n, GLuint *t) { (void)n; (void)t; }
GLenum glGetError(void) { return GL_NO_ERROR; }
void glGetIntegerv(GLenum p, GLint *v) { (void)p; *v = 0; }
const GLubyte *glGetString(GLenum n) { (void)n; return (const GLubyte*)""; }
GLboolean glIsEnabled(GLenum c) { (void)c; return GL_FALSE; }
void glScissor(GLint a, GLint b, GLsizei c, GLsizei d) { (void)a; (void)b; (void)c; (void)d; }
void glTexImage2D(GLenum a, GLint b, GLint c, GLsizei d, GLsizei e, GLint f, GLenum g, GLenum h, const GLvoid *i)
{ (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; (void)h; (void)i; }
void glTexParameteri(GLenum a, GLenum b, GLint c) { (void)a; (void)b; (void)c; }
void glTexSubImage2D(GLenum a, GLint b, GLint c, GLint d, GLsizei e, GLsizei f, GLenum g, GLenum h, const GLvoid *i)
{ (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; (void)h; (void)i; }
void glViewport(GLint a, GLint b, GLsizei c, GLsizei d) { (void)a; (void)b; (void)c; (void)d; }
void glGetTexLevelParameteriv(GLenum a, GLint b, GLenum c, GLint *d) { (void)a; (void)b; (void)c; *d = 0; }

static int fails;

#define CHECK(cond, what) do { \
   if (cond) printf("  [ok]   %s\n", what); \
   else { printf("  [FAIL] %s\n", what); fails++; } \
} while (0)

int main(void)
{
   gl2_t    gl;
   uint8_t *out = (uint8_t*)malloc(W * H * 4);
   unsigned i, created;

   __rglgen_glFenceSync      = fake_fence_sync;
   __rglgen_glClientWaitSync = fake_client_wait_sync;
   __rglgen_glDeleteSync     = fake_delete_sync;
   __rglgen_glGenBuffers     = fake_gen_buffers;
   __rglgen_glDeleteBuffers  = fake_delete_buffers;
   __rglgen_glBindBuffer     = fake_bind_buffer;
   __rglgen_glBufferData     = fake_buffer_data;
   __rglgen_glMapBuffer      = fake_map_buffer;
   __rglgen_glUnmapBuffer    = fake_unmap_buffer;

   memset(&gl, 0, sizeof(gl));
   gl.vp.dims    = VIDEO_SCALE_PACK(W, H);
   gl.video_dims = VIDEO_SCALE_PACK(W, H);
   gl.flags      = GL2_FLAG_HAVE_SYNC | GL2_FLAG_PBO_READBACK_ENABLE
                 | GL2_FLAG_GPU_RECORDING;

   printf("ring with fences:\n");
   CHECK(gl2_init_pbo_readback(&gl), "ring initialised");
   CHECK(!gl2_read_pbo(&gl, out, false) && maps == 0,
         "empty ring: no frame, no map");

   for (i = 0; i < 4; i++)
      gl2_pbo_async_readback(&gl);
   CHECK(syncs_created == 4, "one fence per issued readback");
   for (i = 0; i < 4; i++)
      if (!gl.pbo_readback_sync[i])
         fails++;

   CHECK(!gl2_read_pbo(&gl, out, false) && maps == 0,
         "oldest slot's fence pending: no frame, no map");
   CHECK(gl.pbo_readback_valid[0] && gl.pbo_readback_sync[0],
         "slot kept for the next readback");

   sync_signalled[1] = 1;
   CHECK(gl2_read_pbo(&gl, out, false) && maps == 1,
         "fence signalled: frame mapped");
   CHECK(!gl.pbo_readback_sync[0] && syncs_deleted == 1,
         "read slot's fence deleted");
   CHECK(!gl.pbo_readback_valid[0], "read slot no longer valid");

   /* Four more without a read: slot 0 gets a fence, the other three
    * have theirs replaced. */
   created = syncs_created;
   for (i = 0; i < 4; i++)
      gl2_pbo_async_readback(&gl);
   CHECK(syncs_created == created + 4 && syncs_deleted == 4,
         "reissued slots replace their pending fence");

   gl2_free_pbo_readback(&gl);
   CHECK(syncs_deleted == syncs_created, "teardown deletes every fence");
   for (i = 0; i < 4; i++)
      if (gl.pbo_readback_sync[i])
         fails++;

   printf("ring without sync objects:\n");
   gl.flags &= ~GL2_FLAG_HAVE_SYNC;
   created = syncs_created;
   maps    = 0;
   CHECK(gl2_init_pbo_readback(&gl), "ring initialised");
   for (i = 0; i < 4; i++)
      gl2_pbo_async_readback(&gl);
   CHECK(syncs_created == created, "no fences issued");
   CHECK(gl2_read_pbo(&gl, out, false) && maps == 1,
         "frame mapped as before");
   gl2_free_pbo_readback(&gl);

   /* The recording reader the threaded wrapper calls, through the poke:
    * B,G,R,X rows as the PBO holds them, or BGR24, bottom row first
    * either way, as glReadPixels leaves them. */
   printf("recording reader:\n");
   {
      unsigned x, y, bad;
      bool bottom_up = false;
      for (y = 0; y < H; y++)
         for (x = 0; x < W; x++)
            pixels[y * W + x] = 0xff000000u | (0x5au << 16)
               | ((uint32_t)y << 8) | (uint32_t)x;
      gl.flags |= GL2_FLAG_HAVE_SYNC;
      gl.flags &= ~GL2_FLAG_PBO_READBACK_ENABLE;
      CHECK(gl2_poke_interface.record_read != NULL,
            "the poke carries the reader");
      CHECK(!gl2_poke_interface.record_read(&gl, out, true, &bottom_up)
            && (gl.flags & GL2_FLAG_PBO_READBACK_ENABLE),
            "first call starts the readbacks, no frame");

      /* A full ring, every fence signalled: the oldest slot is read */
      for (i = 0; i < 4; i++)
         gl2_pbo_async_readback(&gl);
      for (i = 1; i <= syncs_created; i++)
         sync_signalled[i] = 1;
      memset(out, 0, W * H * 4);
      CHECK(gl2_poke_interface.record_read(&gl, out, true, &bottom_up)
            && bottom_up, "BGRX frame read, bottom row first");
      for (bad = 0, y = 0; y < H; y++)
         for (x = 0; x < W; x++)
         {
            const uint8_t *p = out + ((size_t)y * W + x) * 4;
            if (p[0] != x || p[1] != y || p[2] != 0x5a)
               bad++;
         }
      CHECK(!bad, "BGRX rows are the PBO's, as read");

      /* A full ring, every fence signalled: the oldest slot is read */
      for (i = 0; i < 4; i++)
         gl2_pbo_async_readback(&gl);
      for (i = 1; i <= syncs_created; i++)
         sync_signalled[i] = 1;
      memset(out, 0, W * H * 4);
      bottom_up = false;
      CHECK(gl2_poke_interface.record_read(&gl, out, false, &bottom_up)
            && bottom_up, "BGR24 frame read, bottom row first");
      for (bad = 0, y = 0; y < H; y++)
         for (x = 0; x < W; x++)
         {
            const uint8_t *p = out + ((size_t)y * W + x) * 3;
            if (p[0] != x || p[1] != y || p[2] != 0x5a)
               bad++;
         }
      CHECK(!bad, "BGR24 rows are the PBO's, converted");
      gl2_free_pbo_readback(&gl);
   }

   free(out);
   if (fails)
   {
      printf("%d check(s) failed\n", fails);
      return 1;
   }
   printf("all checks passed\n");
   return 0;
}
