/* GL3's headset draw, copied out of gfx/drivers/gl3.c, with GL, the
 * runtime's textures and the chain's passes stubbed: each screen is
 * drawn straight into the texture the runtime gives, a new frame ends
 * the chain's frame, and a repeat draws a screen again only when it
 * moved or changed size, by replaying the last frame's final pass. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <boolean.h>
#include "gfx/video_views.h"
#include "gfx/video_defines.h"
#include "gfx/video_xr.h"

typedef unsigned GLuint;
typedef int GLint;
typedef unsigned GLenum;
typedef unsigned char GLboolean;
typedef float GLfloat;
typedef int GLsizei;
typedef unsigned GLbitfield;

#define GL_FRAMEBUFFER               0x8D40
#define GL_DRAW_FRAMEBUFFER_BINDING  0x8CA6
#define GL_VIEWPORT                  0x0BA2
#define GL_SCISSOR_TEST              0x0C11
#define GL_BLEND                     0x0BE2
#define GL_COLOR_ATTACHMENT0         0x8CE0
#define GL_TEXTURE_2D                0x0DE1
#define GL_COLOR_BUFFER_BIT          0x00004000

struct gl3_filter_chain_texture
{
   GLuint image;
   unsigned dims;
   unsigned padded_dims;
   unsigned origin;
   GLenum format;
};

struct gl3_viewport
{
   unsigned pos;
   unsigned dims;
};

typedef struct gl3_filter_chain gl3_filter_chain_t;
typedef struct gl_openxr gl_openxr_t;

typedef struct
{
   video_views_map_t views;
   bool frame_repeat;
} video_frame_info_t;

typedef struct gl3
{
   struct
   {
      GLuint tex[RETRO_VIDEO_VIEWS_MAX];
      unsigned tex_dims[RETRO_VIDEO_VIEWS_MAX];
      GLenum tex_format[RETRO_VIDEO_VIEWS_MAX];
   } views;
   struct
   {
      gl_openxr_t *host;
      video_xr_quad_set_t set;
      struct
      {
         video_xr_slot_t info;
         video_xr_quad_t quads[4];
         unsigned num_quads;
         bool published;
      } shown[VIDEO_XR_MAX_SLOTS];
      float mvp[16];
      GLuint fbo;
      bool proj_shown;
   } xr;
   unsigned rotation_raw;
} gl3_t;

static unsigned failures;

/* The runtime's textures are 0x100 + slot * 2 + layer; any other name
 * attached is an image of the driver's own. */
#define RUNTIME_TEX(s, l) (0x100u + (s) * 2u + (l))

static unsigned offscreen_passes, viewport_passes, replays;
static unsigned frames_ended, last_pass_dims;
static bool chain_drawn = true;
static unsigned runtime_attaches, own_attaches;
static unsigned published[VIDEO_XR_MAX_SLOTS];
static video_xr_quad_set_t planned;

static void glGetIntegerv(GLenum pname, GLint *v)
{
   (void)pname;
   v[0] = 0;
   if (pname == GL_VIEWPORT)
      v[1] = v[2] = v[3] = 0;
}
static GLboolean glIsEnabled(GLenum cap) { (void)cap; return 0; }
static void glEnable(GLenum cap) { (void)cap; }
static void glDisable(GLenum cap) { (void)cap; }
static void glGenFramebuffers(GLsizei n, GLuint *fbos)
{
   (void)n;
   fbos[0] = 1;
}
static void glBindFramebuffer(GLenum target, GLuint fbo)
{
   (void)target;
   (void)fbo;
}
static void glFramebufferTexture2D(GLenum target, GLenum att,
      GLenum textarget, GLuint tex, GLint level)
{
   (void)target;
   (void)att;
   (void)textarget;
   (void)level;
   if (!tex)
      return;
   if (tex >= RUNTIME_TEX(0, 0)
         && tex <= RUNTIME_TEX(VIDEO_XR_MAX_SLOTS - 1, 1))
      runtime_attaches++;
   else
      own_attaches++;
}
static void glViewport(GLint x, GLint y, GLsizei w, GLsizei h)
{
   (void)x;
   (void)y;
   (void)w;
   (void)h;
}
static void glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
   (void)r;
   (void)g;
   (void)b;
   (void)a;
}
static void glClear(GLbitfield mask) { (void)mask; }

static void gl3_filter_chain_set_input_texture(gl3_filter_chain_t *chain,
      const struct gl3_filter_chain_texture *in)
{
   (void)chain;
   (void)in;
}
static void gl3_filter_chain_build_offscreen_passes(
      gl3_filter_chain_t *chain, const struct gl3_viewport *vp)
{
   (void)chain;
   (void)vp;
   offscreen_passes++;
}
static bool gl3_filter_chain_replay_ready(const gl3_filter_chain_t *chain)
{
   (void)chain;
   return chain_drawn;
}
static void gl3_filter_chain_build_viewport_pass_replay(
      gl3_filter_chain_t *chain, const struct gl3_viewport *vp,
      const float *mvp)
{
   (void)chain;
   (void)mvp;
   replays++;
   last_pass_dims = vp->dims;
}
static void gl3_filter_chain_build_viewport_pass(gl3_filter_chain_t *chain,
      const struct gl3_viewport *vp, const float *mvp)
{
   (void)chain;
   (void)mvp;
   viewport_passes++;
   last_pass_dims = vp->dims;
}
static void gl3_filter_chain_end_frame(gl3_filter_chain_t *chain)
{
   (void)chain;
   frames_ended++;
}

static void gl3_xr_plan(gl3_t *gl, const video_frame_info_t *video_info,
      bool views, bool eyes, unsigned frame_dims, unsigned src_dims,
      unsigned ui_dims)
{
   (void)video_info;
   (void)views;
   (void)eyes;
   (void)frame_dims;
   (void)src_dims;
   (void)ui_dims;
   gl->xr.set = planned;
}
bool video_xr_shrinks(unsigned image_dims, unsigned source_dims,
      unsigned rotation)
{
   (void)image_dims;
   (void)source_dims;
   (void)rotation;
   return false;
}
static void gl3_xr_forget(gl3_t *gl, unsigned s, bool yield)
{
   (void)yield;
   gl->xr.shown[s].published = false;
}
static unsigned gl3_xr_eye_screens(const video_frame_info_t *video_info)
{
   (void)video_info;
   return 0;
}
unsigned gl_openxr_slot_refused(gl_openxr_t *xr, unsigned slot)
{
   (void)xr;
   (void)slot;
   return 0;
}
bool gl_openxr_slot_target(gl_openxr_t *xr, unsigned slot,
      unsigned dims, unsigned layers, uint32_t textures[2])
{
   (void)xr;
   (void)dims;
   (void)layers;
   textures[0] = RUNTIME_TEX(slot, 0);
   textures[1] = RUNTIME_TEX(slot, 1);
   return true;
}
void gl_openxr_slot_publish(gl_openxr_t *xr, unsigned slot,
      const video_xr_quad_set_t *quads, const void *projection)
{
   (void)xr;
   (void)quads;
   (void)projection;
   published[slot]++;
}
void gl_openxr_slot_forget(gl_openxr_t *xr, unsigned slot)
{
   (void)xr;
   (void)slot;
}
static void gl3_xr_copy_ui(gl3_t *gl, unsigned s, GLuint tex,
      unsigned ui_dims)
{
   (void)gl;
   (void)s;
   (void)tex;
   (void)ui_dims;
}
static bool gl3_xr_draw_projection(gl3_t *gl,
      const video_frame_info_t *video_info)
{
   (void)gl;
   (void)video_info;
   return true;
}

#define OPENXR_PROJECTION_SLOT VIDEO_XR_MAX_SLOTS

#include "driver_code.inc"

#define CHECK(cond) do { if (!(cond)) { \
   printf("FAIL line %u: %s\n", (unsigned)__LINE__, #cond); \
   failures++; } } while (0)

/* Any chain: the pass steps are stubbed. */
#define FRAME_CHAIN ((gl3_filter_chain_t*)(uintptr_t)1)

/* @screens screens drawn whole, no menu. */
static void plan(unsigned dims, unsigned screens, float x)
{
   unsigned s;
   memset(&planned, 0, sizeof(planned));
   for (s = 0; s < screens; s++)
   {
      planned.slots[s].dims         = dims;
      planned.slots[s].layers       = 1;
      planned.slots[s].view[0]      = -1;
      planned.quads[s].slot         = s;
      planned.quads[s].pose.position.x = x;
   }
   planned.num_quads = screens;
}

static void draw(gl3_t *gl, bool new_frame)
{
   video_frame_info_t info;
   struct gl3_filter_chain_texture frame;
   memset(&info, 0, sizeof(info));
   memset(&frame, 0, sizeof(frame));
   frame.image = 0x10;
   frame.dims  = VIDEO_SCALE_PACK(64, 32);
   info.frame_repeat = !new_frame;
   gl3_xr_draw(gl, &info, new_frame, true, false, NULL, FRAME_CHAIN, NULL,
         &frame, frame.dims, 0);
}

static void reset(gl3_t *gl)
{
   memset(gl, 0, sizeof(*gl));
   offscreen_passes = viewport_passes = replays = 0;
   frames_ended = last_pass_dims = 0;
   chain_drawn = true;
   runtime_attaches = own_attaches = 0;
   memset(published, 0, sizeof(published));
}

/* A new frame: each screen straight into the runtime's texture, one
 * final pass each, and the chain's frame ended. */
static void test_direct(void)
{
   gl3_t gl;
   unsigned dims = VIDEO_SCALE_PACK(64, 32);
   reset(&gl);
   plan(dims, 2, 0.0f);
   draw(&gl, true);
   CHECK(offscreen_passes == 2 && viewport_passes == 2);
   CHECK(frames_ended == 2 && replays == 0);
   CHECK(runtime_attaches == 2 && own_attaches == 0);
   CHECK(published[0] == 1 && published[1] == 1);
}

/* A repeat of what was shown draws nothing. */
static void test_repeat_same(void)
{
   gl3_t gl;
   unsigned dims = VIDEO_SCALE_PACK(64, 32);
   reset(&gl);
   plan(dims, 1, 0.0f);
   draw(&gl, true);
   viewport_passes = frames_ended = runtime_attaches = 0;
   draw(&gl, false);
   CHECK(viewport_passes == 0 && replays == 0 && frames_ended == 0);
   CHECK(published[0] == 1);
}

/* Resized or moved while paused: the last frame's final pass replayed
 * at the new size straight into the runtime's texture. No other pass
 * runs and no frame ends, so shader feedback and history stay where
 * that frame left them. */
static void test_repeat_changed(void)
{
   gl3_t gl;
   unsigned dims    = VIDEO_SCALE_PACK(64, 32);
   unsigned changed = VIDEO_SCALE_PACK(80, 40);
   reset(&gl);
   plan(dims, 1, 0.0f);
   draw(&gl, true);
   offscreen_passes = viewport_passes = frames_ended = 0;
   runtime_attaches = own_attaches = 0;
   plan(changed, 1, 0.0f);
   draw(&gl, false);
   CHECK(replays == 1 && offscreen_passes == 0 && viewport_passes == 0);
   CHECK(frames_ended == 0 && last_pass_dims == changed);
   CHECK(runtime_attaches == 1 && own_attaches == 0);
   replays = 0;
   plan(changed, 1, 0.5f);
   draw(&gl, false);
   CHECK(replays == 1 && offscreen_passes == 0 && frames_ended == 0);
   CHECK(published[0] == 3);
}

/* A chain with no frame drawn since it was built has nothing to replay:
 * its passes run, and no frame ends. */
static void test_repeat_unready(void)
{
   gl3_t gl;
   unsigned dims    = VIDEO_SCALE_PACK(64, 32);
   unsigned changed = VIDEO_SCALE_PACK(80, 40);
   reset(&gl);
   plan(dims, 1, 0.0f);
   draw(&gl, true);
   offscreen_passes = viewport_passes = frames_ended = 0;
   chain_drawn = false;
   plan(changed, 1, 0.0f);
   draw(&gl, false);
   CHECK(offscreen_passes == 1 && viewport_passes == 1);
   CHECK(replays == 0 && frames_ended == 0);
}

int main(void)
{
   test_direct();
   test_repeat_same();
   test_repeat_changed();
   test_repeat_unready();
   printf("%u failed (4 GL3 headset draw cases)\n", failures);
   return failures ? 1 : 0;
}
