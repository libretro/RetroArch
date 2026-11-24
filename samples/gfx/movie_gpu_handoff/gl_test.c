/* glcore and gl2 movie frames in memory: the production import, drop,
 * prepare and device code against a fake planar upload, no context. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <boolean.h>
#include <formats/image.h>
#include "gfx/video_defines.h"
#include "frame.inc"

#define RETRO_HW_FRAME_BUFFER_VALID ((void*)-1)
enum video_texture_update { VIDEO_TEXTURE_UPDATE_REFUSED,
   VIDEO_TEXTURE_UPDATE_DONE, VIDEO_TEXTURE_UPDATE_DROPPED };
typedef struct { uint32_t import_formats; } video_device_info_t;
typedef unsigned GLuint;
typedef unsigned GLenum;
typedef int GLsizei;
typedef int GLint;
#define GL_TEXTURE_2D        0x0DE1
#define GL_TEXTURE_MAX_LEVEL 0x813D
#define RARCH_LOG log_count

#define GL_PLANAR_FAILED (1 << 1)
#define GL_PLANAR_OK(p) (!((p)->flags & GL_PLANAR_FAILED))
typedef struct { uint8_t flags; } gl_planar_t;

#define GL2_FLAG_HAVE_FBO            (1 << 0)
#define GL2_FLAG_CORE_CONTEXT_IN_USE (1 << 1)

typedef struct
{
   gl_planar_t planar;
   GLuint movie_texture;
   unsigned movie_dims;
   bool movie_pending;
   unsigned flags;
   unsigned last_width[4], last_height[4];
} gl_fake_t;
typedef gl_fake_t gl2_t;
typedef gl_fake_t gl3_t;

static unsigned failures, loads, updates, forgets, deletes, released, logs;
#ifndef GL_LANE_GL2
static GLuint bound, max_level_tex;
static GLint max_level = -1;
#endif
static unsigned next_tex = 7;
static bool load_ok = true, update_ok = true, last_core;
static const struct texture_planar *seen;
static unsigned seen_w, seen_h;
static GLuint forgot, deleted;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); failures++; } } while (0)

static unsigned gl_planar_load(gl_planar_t *p, const struct texture_image *ti,
      bool linear, bool core)
{
   (void)p;
   CHECK(linear);
   loads++; last_core = core;
   seen = ti->planar; seen_w = ti->width; seen_h = ti->height;
   return load_ok ? next_tex++ : 0;
}
static bool gl_planar_update(gl_planar_t *p, unsigned tex,
      const struct texture_image *ti)
{
   (void)p; (void)tex;
   updates++; seen = ti->planar; seen_w = ti->width; seen_h = ti->height;
   return update_ok;
}
static void gl_planar_forget(gl_planar_t *p, unsigned tex)
{ (void)p; forgets++; forgot = tex; }
static void glDeleteTextures(GLsizei n, const GLuint *t)
{ CHECK(n == 1); deletes++; deleted = *t; }
static void log_count(const char *fmt, ...) { (void)fmt; logs++; }
#ifndef GL_LANE_GL2
static void glBindTexture(GLenum target, GLuint t)
{ CHECK(target == GL_TEXTURE_2D); bound = t; }
static void glTexParameteri(GLenum target, GLenum name, GLint value)
{
   CHECK(target == GL_TEXTURE_2D && name == GL_TEXTURE_MAX_LEVEL);
   max_level_tex = bound; max_level = value;
}
#endif
static void release(void *owner, uint64_t done, uint64_t value)
{
   CHECK(owner == &released && done == 5 && value == 6);
   released++;
}

#ifdef GL_LANE_GL2
#define GL_NAME "gl2"
#define GL_CLEARS_COORDS 1 /* the core's next frame sets them again */
#define GL_IMPORT gl2_import_movie_frame
#define GL_PREPARE gl2_movie_prepare
#define GL_GET_DEVICE gl2_get_device
#else
#define GL_NAME "glcore"
#define GL_CLEARS_COORDS 0
#define GL_IMPORT gl3_import_movie_frame
#define GL_PREPARE gl3_movie_prepare
#define GL_GET_DEVICE gl3_get_device
#endif
#include "gl_driver.inc"

static void frame_of(video_frame_import_t *f, unsigned w, unsigned h)
{
   static const uint8_t planes[12] = { 0 };
   memset(f, 0, sizeof(*f));
   f->planar.planes[0] = planes; f->planar.planes[1] = planes + 8;
   f->planar.planes[2] = planes + 10; f->planar.strides[0] = 4;
   f->planar.strides[1] = f->planar.strides[2] = 2;
   f->dims = VIDEO_SCALE_PACK(w, h); f->pixfmt = IMAGE_PIXFMT_I420;
   f->abandon = release; f->owner = &released; f->done = 5; f->done_value = 6;
}

int main(void)
{
   gl_fake_t gl;
   video_frame_import_t f;
   video_device_info_t info;
   const void *frame;
   unsigned dims, tex;
   memset(&gl, 0, sizeof(gl));
   gl.flags = GL2_FLAG_HAVE_FBO;

   /* The device lists the planes it converts, and only those */
   CHECK(!GL_GET_DEVICE(NULL, &info));
   CHECK(GL_GET_DEVICE(&gl, &info) && info.import_formats == IMAGE_PIXFMT_I420);
   gl.planar.flags = GL_PLANAR_FAILED;
   CHECK(!GL_GET_DEVICE(&gl, &info) && !info.import_formats);

   /* Refused: no driver, no planes, no converter, no size */
   frame_of(&f, 4, 2);
   CHECK(GL_IMPORT(NULL, &f) == VIDEO_TEXTURE_UPDATE_REFUSED);
   CHECK(GL_IMPORT(&gl, &f) == VIDEO_TEXTURE_UPDATE_REFUSED && !loads && !released);
   gl.planar.flags = 0;
   f.planar.planes[0] = NULL;
   CHECK(GL_IMPORT(&gl, &f) == VIDEO_TEXTURE_UPDATE_REFUSED && !loads);
   frame_of(&f, 0, 2);
   CHECK(GL_IMPORT(&gl, &f) == VIDEO_TEXTURE_UPDATE_REFUSED && !loads);
   frame_of(&f, 4, 2);
   load_ok = false;
   CHECK(GL_IMPORT(&gl, &f) == VIDEO_TEXTURE_UPDATE_REFUSED && loads == 1
         && !released && !gl.movie_texture && !gl.movie_pending);
   load_ok = true;

   /* The first frame makes the texture, the next ones rewrite it */
   CHECK(GL_IMPORT(&gl, &f) == VIDEO_TEXTURE_UPDATE_DONE && loads == 2
         && seen == &f.planar && seen_w == 4 && seen_h == 2
         && released == 1 && gl.movie_pending && gl.movie_dims == f.dims);
   tex = gl.movie_texture;
   CHECK(tex && logs == 1);
#ifndef GL_LANE_GL2
   /* Sampled with the chain's mip filter: one level, unbound after */
   CHECK(max_level_tex == tex && !max_level && !bound);
#endif
   CHECK(GL_IMPORT(&gl, &f) == VIDEO_TEXTURE_UPDATE_DONE && loads == 2
         && updates == 1 && released == 2 && gl.movie_texture == tex);
   update_ok = false; gl.movie_pending = false;
   CHECK(GL_IMPORT(&gl, &f) == VIDEO_TEXTURE_UPDATE_REFUSED && released == 2
         && !gl.movie_pending && gl.movie_texture == tex);
   update_ok = true;

   /* The movie stands in for a NULL or hardware refresh, at its size */
   gl.movie_pending = true;
   frame = NULL; dims = VIDEO_SCALE_PACK(320, 240);
   CHECK(GL_PREPARE(&gl, &frame, &dims) && !frame && dims == f.dims
         && !gl.movie_pending);
   frame = NULL;
   CHECK(!GL_PREPARE(&gl, &frame, &dims) && dims == f.dims);
   frame = RETRO_HW_FRAME_BUFFER_VALID;
   CHECK(!GL_PREPARE(&gl, &frame, &dims) && !frame && gl.movie_texture == tex);

   /* A new size makes a new texture */
   gl.last_width[1] = 320;
   frame_of(&f, 8, 4);
   CHECK(GL_IMPORT(&gl, &f) == VIDEO_TEXTURE_UPDATE_DONE && forgets == 1
         && forgot == tex && deletes == 1 && deleted == tex && loads == 3
         && gl.movie_texture != tex && seen_w == 8 && logs == 2);
   CHECK(GL_CLEARS_COORDS == !gl.last_width[1]);
   tex = gl.movie_texture;

   /* A core frame ends the movie and is drawn itself */
   gl.last_width[1] = 320;
   frame = &f; dims = VIDEO_SCALE_PACK(320, 240);
   CHECK(!GL_PREPARE(&gl, &frame, &dims) && frame == &f
         && dims == VIDEO_SCALE_PACK(320, 240) && !gl.movie_texture
         && !gl.movie_pending && deleted == tex && deletes == 2);
   CHECK(GL_CLEARS_COORDS == !gl.last_width[1]);
   CHECK(!GL_PREPARE(&gl, &frame, &dims) && frame == &f);

   /* NULL clears, more than once */
   CHECK(GL_IMPORT(&gl, &f) == VIDEO_TEXTURE_UPDATE_DONE);
   CHECK(GL_IMPORT(&gl, NULL) == VIDEO_TEXTURE_UPDATE_DONE
         && !gl.movie_texture && !gl.movie_dims && deletes == 3);
   CHECK(GL_IMPORT(&gl, NULL) == VIDEO_TEXTURE_UPDATE_DONE && deletes == 3);
#ifdef GL_LANE_GL2
   /* Converted by a draw: no framebuffers, no planes; the context's
    * profile picks the converter's */
   CHECK(!last_core);
   gl.flags = GL2_FLAG_CORE_CONTEXT_IN_USE;
   CHECK(!GL_GET_DEVICE(&gl, &info));
   CHECK(GL_IMPORT(&gl, &f) == VIDEO_TEXTURE_UPDATE_REFUSED && loads == 4);
   gl.flags |= GL2_FLAG_HAVE_FBO;
   CHECK(GL_IMPORT(&gl, &f) == VIDEO_TEXTURE_UPDATE_DONE && last_core);
#else
   /* Always a core profile */
   CHECK(last_core);
#endif

   printf("GL movie planes (%s): %u failures\n", GL_NAME, failures);
   return failures ? 1 : 0;
}
