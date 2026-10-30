/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

/* The native bindings' headers come first, before OpenXR's. */
#include "openxr_binding_gl.h"
#include <GL/gl.h>
#include <GL/glext.h>

#include "gl_openxr.h"
#include "openxr_runtime.h"
#include "openxr_session.h"
#include "openxr_swapchain.h"
#include "openxr_frame.h"
#include "openxr_host.h"
#include "openxr_ring.h"
#include "../video_driver.h"

#include "../../msg_hash.h"
#include "../../verbosity.h"

/* The GL calls, through the shared context's get_proc_address: the
 * same on the video thread, with the window's context, and on the XR
 * thread, with its own. */
typedef struct gl_openxr_procs
{
   void (APIENTRY *GenTextures)(GLsizei, GLuint*);
   void (APIENTRY *DeleteTextures)(GLsizei, const GLuint*);
   void (APIENTRY *BindTexture)(GLenum, GLuint);
   void (APIENTRY *TexStorage2D)(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
   void (APIENTRY *TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei,
         GLint, GLenum, GLenum, const void*);
   void (APIENTRY *TexParameteri)(GLenum, GLenum, GLint);
   void (APIENTRY *TexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei,
         GLsizei, GLenum, GLenum, const void*);
   void (APIENTRY *PixelStorei)(GLenum, GLint);
   void (APIENTRY *GetIntegerv)(GLenum, GLint*);
   const GLubyte *(APIENTRY *GetStringi)(GLenum, GLuint);
   GLenum (APIENTRY *GetError)(void);
   GLsync (APIENTRY *FenceSync)(GLenum, GLbitfield);
   void (APIENTRY *WaitSync)(GLsync, GLbitfield, GLuint64);
   void (APIENTRY *DeleteSync)(GLsync);
   void (APIENTRY *Flush)(void);
   void (APIENTRY *Finish)(void);
   void (APIENTRY *CopyImageSubData)(GLuint, GLenum, GLint, GLint, GLint,
         GLint, GLuint, GLenum, GLint, GLint, GLint, GLint, GLsizei,
         GLsizei, GLsizei);
   void (APIENTRY *GenFramebuffers)(GLsizei, GLuint*);
   void (APIENTRY *DeleteFramebuffers)(GLsizei, const GLuint*);
   void (APIENTRY *BindFramebuffer)(GLenum, GLuint);
   void (APIENTRY *FramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint,
         GLint);
   GLenum (APIENTRY *CheckFramebufferStatus)(GLenum);
   void (APIENTRY *BlitFramebuffer)(GLint, GLint, GLint, GLint, GLint,
         GLint, GLint, GLint, GLbitfield, GLenum);
   void (APIENTRY *Disable)(GLenum);
} gl_openxr_procs_t;

/* One image of a slot, in the slot's triple buffer: owned by the video
 * thread as the back, by the XR thread as the front. */
typedef struct gl_openxr_record
{
   GLuint tex[2];
   unsigned dims;          /* tex's size; 0 none */
   unsigned layers;
   GLsync drawn;           /* the video thread's */
   GLsync copied;          /* the XR thread's */
   bool empty;             /* the slot shows nothing from here */
   bool yield;             /* empty once the projection shows */
   unsigned num_quads;
   video_xr_quad_t quads[VIDEO_XR_MAX_QUADS];
   video_xr_slot_t info;
   openxr_projection_t projection;
} gl_openxr_record_t;

typedef struct gl_openxr_slot
{
   openxr_ring_t ring;
   gl_openxr_record_t rec[OPENXR_RING_SIZE];
   /* The XR thread's: the front waits to be copied; the swapchains'
    * size; the size refused, not asked again; the images; the quads
    * shown with the image the swapchains hold. */
   bool pending;
   unsigned dims;
   unsigned layers;
   unsigned refused_dims;
   unsigned refused_layers;
   uint32_t images[OPENXR_MAX_IMAGES][2];
   bool copy_failed;
   unsigned num_quads;
   video_xr_quad_t quads[VIDEO_XR_MAX_QUADS];
   video_xr_slot_t info;
   retro_atomic_int_t refused;   /* for the video thread */
} gl_openxr_slot_t;

struct gl_openxr
{
   openxr_runtime_t rt;
   openxr_session_t session;
   openxr_swapchains_t sc;
   openxr_frame_t frame;
   openxr_host_t host;
   /* The driver shows the headset nothing (HDR output). */
   retro_atomic_int_t hidden;
   const gfx_ctx_driver_t *ctx_driver;
   void *ctx_data;
   gfx_ctx_shared_context_t shared;
   bool shared_made;
   openxr_gl_binding_t binding;
   gl_openxr_procs_t gl;
   bool texture_storage;
   bool copy_image;
   bool started;
   /* The XR thread's. */
   bool bound;
   /* The projection the frame shows came from a copied record. */
   bool proj_shown;
   int64_t format;
   GLuint fbo[2];          /* the blit's read and draw */
   gl_openxr_slot_t slots[OPENXR_MAX_SLOTS];
};

static void gl_openxr_state_hook(void *user, XrSessionState state)
{
   if (state == XR_SESSION_STATE_STOPPING)
      openxr_frame_tick_notify(&((gl_openxr_t*)user)->frame);
}

static void gl_openxr_ended_hook(void *user)
{
   openxr_frame_tick_notify(&((gl_openxr_t*)user)->frame);
   RARCH_WARN("[OpenXR] The headset session ended; the window keeps the output.\n");
   openxr_runtime_notify(MSG_OPENXR_SESSION_ENDED);
}

static void gl_openxr_exiting_hook(void *user)
{
   video_driver_headset_exit_request();
}

static void gl_openxr_local_hook(void *user)
{
   openxr_frame_local_changed(&((gl_openxr_t*)user)->frame);
}

static void gl_openxr_stage_hook(void *user)
{
   openxr_host_stage_changed(&((gl_openxr_t*)user)->host);
}

static unsigned gl_openxr_host_state(void *user)
{
   gl_openxr_t *xr = (gl_openxr_t*)user;
   unsigned bits   = 0;
   bool shown      = !retro_atomic_load_acquire_int(&xr->hidden);
   if (shown && openxr_session_alive(&xr->session))
      bits |= VIDEO_XR_HEADSET_PRESENTING;
   if (openxr_session_focused(&xr->session))
      bits |= VIDEO_XR_HEADSET_FOCUSED;
   /* The driver draws a tracked map's eyes as the projection. */
   if (     shown && openxr_session_alive(&xr->session)
         && openxr_session_running(&xr->session))
      bits |= VIDEO_XR_HEADSET_TRACKING;
   return bits;
}

static bool gl_openxr_host_sample(void *user, unsigned ahead,
      uint32_t *frame_flags)
{
   return openxr_host_sample(&((gl_openxr_t*)user)->host, ahead, frame_flags);
}

static bool gl_openxr_host_eye_state(void *user,
      struct retro_vr_eye_state out[2], unsigned *token)
{
   return openxr_host_eye_state(&((gl_openxr_t*)user)->host, out, token);
}

static void gl_openxr_host_give(void *user, unsigned token)
{
   openxr_host_give(&((gl_openxr_t*)user)->host, token);
}

static bool gl_openxr_host_head_pose(void *user,
      struct retro_vr_head_pose *out)
{
   return openxr_host_head_pose(&((gl_openxr_t*)user)->host, out);
}

static bool gl_openxr_host_locate_hands(void *user)
{
   return openxr_host_locate_hands(&((gl_openxr_t*)user)->host);
}

static bool gl_openxr_host_controller_poses(void *user,
      struct retro_vr_controller_poses *out)
{
   return openxr_host_controller_poses(&((gl_openxr_t*)user)->host, out);
}

static bool gl_openxr_host_eye_size(void *user, unsigned *w, unsigned *h)
{
   return openxr_host_eye_size(&((gl_openxr_t*)user)->host, w, h);
}

static bool gl_openxr_host_set_space(void *user,
      enum retro_vr_reference_space *space, unsigned *w, unsigned *h)
{
   return openxr_host_set_space(&((gl_openxr_t*)user)->host, space, w, h);
}

static void gl_openxr_host_resume(void *user)
{
   openxr_host_resume(&((gl_openxr_t*)user)->host);
}

/* A function pointer of the table's own type, from the loader's. */
static bool gl_openxr_proc(gl_openxr_t *xr, void *field, size_t size,
      const char *name)
{
   gfx_ctx_proc_t proc = xr->shared.get_proc_address(name);
   if (size != sizeof(proc))
      return false;
   memcpy(field, &proc, sizeof(proc));
   return proc != NULL;
}

#define GL_OPENXR_PROC(xr, field, name) \
   gl_openxr_proc((xr), (void*)&(xr)->gl.field, sizeof((xr)->gl.field), \
         (name))

static bool gl_openxr_load_procs(gl_openxr_t *xr)
{
   return GL_OPENXR_PROC(xr, GenTextures, "glGenTextures")
      && GL_OPENXR_PROC(xr, DeleteTextures, "glDeleteTextures")
      && GL_OPENXR_PROC(xr, BindTexture, "glBindTexture")
      && GL_OPENXR_PROC(xr, TexImage2D, "glTexImage2D")
      && GL_OPENXR_PROC(xr, TexParameteri, "glTexParameteri")
      && GL_OPENXR_PROC(xr, TexSubImage2D, "glTexSubImage2D")
      && GL_OPENXR_PROC(xr, PixelStorei, "glPixelStorei")
      && GL_OPENXR_PROC(xr, GetIntegerv, "glGetIntegerv")
      && GL_OPENXR_PROC(xr, GetError, "glGetError")
      && GL_OPENXR_PROC(xr, FenceSync, "glFenceSync")
      && GL_OPENXR_PROC(xr, WaitSync, "glWaitSync")
      && GL_OPENXR_PROC(xr, DeleteSync, "glDeleteSync")
      && GL_OPENXR_PROC(xr, Flush, "glFlush")
      && GL_OPENXR_PROC(xr, Finish, "glFinish")
      && GL_OPENXR_PROC(xr, GenFramebuffers, "glGenFramebuffers")
      && GL_OPENXR_PROC(xr, DeleteFramebuffers, "glDeleteFramebuffers")
      && GL_OPENXR_PROC(xr, BindFramebuffer, "glBindFramebuffer")
      && GL_OPENXR_PROC(xr, FramebufferTexture2D, "glFramebufferTexture2D")
      && GL_OPENXR_PROC(xr, CheckFramebufferStatus,
            "glCheckFramebufferStatus")
      && GL_OPENXR_PROC(xr, BlitFramebuffer, "glBlitFramebuffer")
      && GL_OPENXR_PROC(xr, Disable, "glDisable");
}

static bool gl_openxr_has_extension(gl_openxr_t *xr, const char *name)
{
   GLint i, n = 0;
   if (!xr->gl.GetStringi)
      return false;
   xr->gl.GetIntegerv(GL_NUM_EXTENSIONS, &n);
   for (i = 0; i < n; i++)
   {
      const char *ext = (const char*)xr->gl.GetStringi(GL_EXTENSIONS,
            (GLuint)i);
      if (ext && string_is_equal(ext, name))
         return true;
   }
   return false;
}

/* The window's context: what both contexts can do, asked once. A
 * name the loader returns is no proof of support. */
static void gl_openxr_caps(gl_openxr_t *xr, unsigned version)
{
   bool storage = GL_OPENXR_PROC(xr, TexStorage2D, "glTexStorage2D");
   bool copy    = GL_OPENXR_PROC(xr, CopyImageSubData, "glCopyImageSubData");
   GL_OPENXR_PROC(xr, GetStringi, "glGetStringi");
   xr->texture_storage = storage && (version >= 42
         || gl_openxr_has_extension(xr, "GL_ARB_texture_storage"));
   xr->copy_image      = copy && (version >= 43
         || gl_openxr_has_extension(xr, "GL_ARB_copy_image"));
}

gl_openxr_t *gl_openxr_new(const gfx_ctx_driver_t *ctx_driver,
      void *ctx_data)
{
   unsigned s;
   GLint major = 0;
   GLint minor = 0;
   enum openxr_runtime_result rr = OPENXR_RUNTIME_FAILED;
   gl_openxr_t *xr;

   if (     !ctx_driver
         || !ctx_driver->shared_context_new
         || !ctx_driver->shared_context_bind
         || !ctx_driver->shared_context_free)
   {
      RARCH_WARN("[OpenXR] The context driver has no shared contexts; no headset output.\n");
      openxr_runtime_notify(MSG_OPENXR_FAILED);
      openxr_host_forget();
      return NULL;
   }
   if (!(xr = (gl_openxr_t*)calloc(1, sizeof(*xr))))
   {
      openxr_host_forget();
      return NULL;
   }
   xr->ctx_driver                  = ctx_driver;
   xr->ctx_data                    = ctx_data;
   xr->session.hooks.state_changed = gl_openxr_state_hook;
   xr->session.hooks.ended         = gl_openxr_ended_hook;
   xr->session.hooks.exiting       = gl_openxr_exiting_hook;
   xr->session.hooks.local_changed = gl_openxr_local_hook;
   xr->session.hooks.stage_changed = gl_openxr_stage_hook;
   /* No lock: every runtime call is on the XR thread, which alone
    * makes current the context the runtime submits with. */
   xr->session.hooks.user          = xr;
   for (s = 0; s < OPENXR_MAX_SLOTS; s++)
      openxr_ring_init(&xr->slots[s].ring);
   openxr_host_init(&xr->host, &xr->rt, &xr->session, &xr->frame);

   if (!ctx_driver->shared_context_new(ctx_data, &xr->shared))
   {
      RARCH_WARN("[OpenXR] No context could share the window's; no headset output.\n");
      goto end;
   }
   xr->shared_made = true;
   if (!gl_openxr_load_procs(xr))
   {
      RARCH_ERR("[OpenXR] The GL context lacks functions headset output needs.\n");
      goto end;
   }
   /* The XR thread's context is the window's kind and version. */
   xr->gl.GetIntegerv(GL_MAJOR_VERSION, &major);
   xr->gl.GetIntegerv(GL_MINOR_VERSION, &minor);
   gl_openxr_caps(xr, (unsigned)(major * 10 + minor));

#ifdef HAVE_EGL
   /* XR_MNDX_egl_enable's binding is an EGL context's alone. */
   if (xr->shared.kind == GFX_CTX_SHARED_EGL)
      xr->rt.binding_ext = XR_MNDX_EGL_ENABLE_EXTENSION_NAME;
#endif
   if ((rr = openxr_runtime_create_instance(&xr->rt, xrGetInstanceProcAddr,
               XR_KHR_OPENGL_ENABLE_EXTENSION_NAME)) != OPENXR_RUNTIME_OK)
      goto end;
   if ((rr = openxr_runtime_find_system(&xr->rt)) != OPENXR_RUNTIME_OK)
      goto end;
   rr = OPENXR_RUNTIME_FAILED;
   if (!openxr_binding_gl_requirements(&xr->rt, (unsigned)major,
            (unsigned)minor))
      goto end;
   if (     !openxr_session_load(&xr->session, &xr->rt)
         || !openxr_frame_load(&xr->frame, &xr->rt, &xr->session, &xr->sc)
         || !openxr_swapchains_load(&xr->sc, &xr->rt, &xr->session))
   {
      RARCH_ERR("[OpenXR] The runtime lacks session functions.\n");
      goto end;
   }
   return xr;

end:
   openxr_runtime_notify((rr == OPENXR_RUNTIME_UNAVAILABLE)
         ? MSG_OPENXR_UNAVAILABLE : MSG_OPENXR_FAILED);
   gl_openxr_free(xr);
   openxr_host_forget();
   return NULL;
}

/* The laser's dot, filled on the XR thread. */
struct gl_openxr_dot
{
   gl_openxr_t *xr;
   uint32_t images[OPENXR_MAX_IMAGES][2];
};

static bool gl_openxr_cursor_fill(void *user, uint32_t index,
      const char **step, int *code)
{
   GLenum err;
   struct gl_openxr_dot *dot = (struct gl_openxr_dot*)user;
   gl_openxr_t *xr           = dot->xr;
   unsigned w                = openxr_frame_cursor_w(&xr->frame);
   uint8_t *px               = (uint8_t*)malloc(
         (size_t)VIDEO_XR_CURSOR_W * VIDEO_XR_CURSOR_H * 4);
   if (!px)
   {
      *step = "malloc";
      *code = 0;
      return false;
   }
   openxr_frame_cursor_pixels(px, w, true);
   while (xr->gl.GetError() != GL_NO_ERROR) { }
   xr->gl.BindTexture(GL_TEXTURE_2D, dot->images[index][0]);
   xr->gl.PixelStorei(GL_UNPACK_ALIGNMENT, 4);
   xr->gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, VIDEO_XR_CURSOR_H,
         GL_RGBA, GL_UNSIGNED_BYTE, px);
   xr->gl.BindTexture(GL_TEXTURE_2D, 0);
   free(px);
   if ((err = xr->gl.GetError()) == GL_NO_ERROR)
      return true;
   *step = "glTexSubImage2D";
   *code = (int)err;
   return false;
}

static void gl_openxr_cursor_create(gl_openxr_t *xr)
{
   static const int64_t formats[2] = {
      OPENXR_GL_SRGB8_ALPHA8, OPENXR_GL_RGBA8 };
   struct gl_openxr_dot dot;
   openxr_gl_images_t li;
   const char *step = NULL;
   int code         = 0;
   dot.xr           = xr;
   li.sc            = &xr->sc;
   li.tex           = dot.images;
   xr->frame.cursor = openxr_swapchains_create_filled(&xr->sc, formats, 2,
         VIDEO_SCALE_PACK(openxr_frame_cursor_w(&xr->frame),
            VIDEO_XR_CURSOR_H),
         openxr_binding_gl_images, &li, gl_openxr_cursor_fill, &dot,
         &step, &code);
   if (xr->frame.cursor && xr->frame.rt->max_width)
      RARCH_LOG("[OpenXR] Laser cursor ready: %u wide, with %s (the "
            "headset's widest %u).\n", openxr_frame_cursor_w(&xr->frame),
            openxr_frame_cursor_parts(&xr->frame),
            xr->frame.rt->max_width);
   else if (xr->frame.cursor)
      RARCH_LOG("[OpenXR] Laser cursor ready: %u wide, with %s (the "
            "headset does not say its widest).\n",
            openxr_frame_cursor_w(&xr->frame),
            openxr_frame_cursor_parts(&xr->frame));
   else
      RARCH_WARN("[OpenXR] No laser cursor (%s, %d).\n", step, code);
}

/* The XR thread's start: its context for good, then the session. */
static bool gl_openxr_thread_start(void *user)
{
   const void *chain;
   gl_openxr_t *xr = (gl_openxr_t*)user;

   if (!xr->ctx_driver->shared_context_bind(&xr->shared, true))
   {
      RARCH_ERR("[OpenXR] The headset's GL context could not be made current.\n");
      return false;
   }
   xr->bound = true;
   if (     !(chain = openxr_binding_gl_chain(&xr->binding, &xr->shared))
         || !openxr_session_create(&xr->session, chain))
      return false;
   openxr_swapchains_list_formats(&xr->sc);
   if (!(xr->format = openxr_binding_gl_format(&xr->sc)))
      return false;
   openxr_frame_session_created(&xr->frame);
   openxr_host_session_created(&xr->host);
   RARCH_LOG("[OpenXR] Session created.\n");
   openxr_frame_input_attach(&xr->frame);
   gl_openxr_cursor_create(xr);
   /* The copy keeps the stored values; the compositor decodes them. */
   xr->gl.Disable(GL_FRAMEBUFFER_SRGB);
   if (!xr->copy_image)
      xr->gl.GenFramebuffers(2, xr->fbo);
   return true;
}

/* The XR thread's end, after a failed start too: what it made goes,
 * on the thread whose context made it. */
static void gl_openxr_thread_stop(void *user)
{
   unsigned s, r;
   gl_openxr_t *xr = (gl_openxr_t*)user;
   if (!xr->bound)
      return;
   xr->gl.Finish();
   for (s = 0; s < OPENXR_MAX_SLOTS; s++)
   {
      gl_openxr_slot_t *slot = &xr->slots[s];
      for (r = 0; r < OPENXR_RING_SIZE; r++)
         if (slot->rec[r].copied)
         {
            xr->gl.DeleteSync(slot->rec[r].copied);
            slot->rec[r].copied = NULL;
         }
      openxr_slot_destroy(&xr->sc, s);
      slot->pending        = false;
      slot->dims           = 0;
      slot->layers         = 0;
      slot->refused_dims   = 0;
      slot->refused_layers = 0;
      slot->num_quads      = 0;
      retro_atomic_store_release_int(&slot->refused, 0);
   }
   xr->proj_shown = false;
   if (xr->fbo[0])
   {
      xr->gl.DeleteFramebuffers(2, xr->fbo);
      xr->fbo[0] = xr->fbo[1] = 0;
   }
   if (xr->frame.cursor)
   {
      xr->sc.DestroySwapchain(xr->frame.cursor);
      xr->frame.cursor = XR_NULL_HANDLE;
   }
   if (xr->session.session)
   {
      openxr_frame_input_detach(&xr->frame);
      openxr_session_destroy(&xr->session);
   }
   xr->ctx_driver->shared_context_bind(&xr->shared, false);
   xr->bound = false;
}

/* The slot shows nothing until an image drawn after now is copied. */
static void gl_openxr_hide(gl_openxr_t *xr, unsigned slot)
{
   xr->slots[slot].num_quads = 0;
   openxr_slot_forget(&xr->sc, slot);
   if (slot == OPENXR_PROJECTION_SLOT)
   {
      openxr_frame_clear_projection(&xr->frame);
      xr->proj_shown = false;
   }
}

/* The slot's swapchains at a record's size, on the XR thread. A size
 * refused is not asked again. */
static bool gl_openxr_make_slot(gl_openxr_t *xr, unsigned slot,
      unsigned dims, unsigned layers)
{
   unsigned n             = 0;
   gl_openxr_slot_t *s    = &xr->slots[slot];
   openxr_gl_images_t li;
   if (dims == s->refused_dims && layers == s->refused_layers)
      return false;
   li.sc     = &xr->sc;
   li.tex    = s->images;
   s->dims   = 0;
   s->layers = 0;
   if (!openxr_slot_create(&xr->sc, slot, xr->format, false, dims, layers,
            openxr_binding_gl_images, &li, &n))
   {
      s->refused_dims   = dims;
      s->refused_layers = layers;
      retro_atomic_store_release_int(&s->refused, (int)dims);
      return false;
   }
   s->dims           = dims;
   s->layers         = layers;
   s->refused_dims   = 0;
   s->refused_layers = 0;
   retro_atomic_store_release_int(&s->refused, 0);
   return true;
}

/* A record's textures into the acquired images: as they are stored,
 * with no sRGB conversion. False on a GL error or an incomplete
 * framebuffer. */
static bool gl_openxr_copy(gl_openxr_t *xr, const gl_openxr_slot_t *s,
      const gl_openxr_record_t *r, const unsigned *index)
{
   unsigned l;
   bool ok      = true;
   GLsizei w    = (GLsizei)VIDEO_SCALE_W(r->dims);
   GLsizei h    = (GLsizei)VIDEO_SCALE_H(r->dims);
   while (xr->gl.GetError() != GL_NO_ERROR) { }
   for (l = 0; l < r->layers && ok; l++)
   {
      GLuint image = s->images[index[l]][l];
      if (xr->copy_image)
      {
         xr->gl.CopyImageSubData(r->tex[l], GL_TEXTURE_2D, 0, 0, 0, 0,
               image, GL_TEXTURE_2D, 0, 0, 0, 0, w, h, 1);
         continue;
      }
      /* Framebuffers are each context's own: these are the XR thread's. */
      xr->gl.BindFramebuffer(GL_READ_FRAMEBUFFER, xr->fbo[0]);
      xr->gl.FramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, r->tex[l], 0);
      xr->gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, xr->fbo[1]);
      xr->gl.FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, image, 0);
      if (     xr->gl.CheckFramebufferStatus(GL_READ_FRAMEBUFFER)
               != GL_FRAMEBUFFER_COMPLETE
            || xr->gl.CheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)
               != GL_FRAMEBUFFER_COMPLETE)
         ok = false;
      else
         xr->gl.BlitFramebuffer(0, 0, w, h, 0, 0, w, h,
               GL_COLOR_BUFFER_BIT, GL_NEAREST);
      xr->gl.FramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, 0, 0);
      xr->gl.FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, 0, 0);
      xr->gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
   }
   return ok && xr->gl.GetError() == GL_NO_ERROR;
}

/* The slot's newest record, if one came since: it waits to be shown. */
static void gl_openxr_take(gl_openxr_t *xr, unsigned slot)
{
   int front;
   gl_openxr_slot_t *s = &xr->slots[slot];
   gl_openxr_record_t *r;
   if (!openxr_ring_take(&s->ring, &front))
      return;
   r = &s->rec[front];
   /* Made the last time this record was copied: the video thread issued
    * its wait on it before publishing the record again. */
   if (r->copied)
   {
      xr->gl.DeleteSync(r->copied);
      r->copied = NULL;
   }
   s->pending = true;
}

static bool gl_openxr_waiting(const gl_openxr_t *xr, unsigned slot)
{
   const gl_openxr_slot_t *s = &xr->slots[slot];
   return s->pending && !s->rec[openxr_ring_front(&s->ring)].empty;
}

/* One slot's waiting record, each headset frame: copied, or kept for the
 * next frame while the runtime still reads the image. True when what the
 * slot shows changed. */
static bool gl_openxr_fill_slot(gl_openxr_t *xr, unsigned slot)
{
   bool ok;
   unsigned index[2];
   gl_openxr_slot_t *s = &xr->slots[slot];
   gl_openxr_record_t *r;

   if (!s->pending)
      return false;
   r = &s->rec[openxr_ring_front(&s->ring)];
   if (r->empty)
   {
      /* A screen yielding to the tracked eyes shows until they do. */
      if (r->yield && !xr->proj_shown)
         return false;
      s->pending = false;
      gl_openxr_hide(xr, slot);
      return true;
   }
   if (     (r->dims != s->dims || r->layers != s->layers)
         && !gl_openxr_make_slot(xr, slot, r->dims, r->layers))
   {
      s->pending = false;
      gl_openxr_hide(xr, slot);
      return true;
   }

   /* The draw finishes before the copy reads it. */
   xr->gl.WaitSync(r->drawn, 0, GL_TIMEOUT_IGNORED);
   if (!openxr_slot_acquire(&xr->sc, slot, index))
      return false;
   ok        = gl_openxr_copy(xr, s, r, index);
   r->copied = xr->gl.FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
   xr->gl.Flush();
   openxr_slot_release(&xr->sc, slot);
   /* The eyes go out with the poses that drew them; the frame carries
    * them to a recenter's new track space. */
   if (slot == OPENXR_PROJECTION_SLOT)
   {
      if (ok && r->projection.active)
         openxr_frame_publish_projection(&xr->frame, &r->projection);
      else
         openxr_frame_clear_projection(&xr->frame);
      xr->proj_shown = ok && r->projection.active;
   }
   s->pending = false;

   if (!ok)
   {
      if (!s->copy_failed)
         RARCH_ERR("[OpenXR] Slot %u's image could not be copied for the headset.\n",
               slot);
      s->copy_failed = true;
      s->num_quads   = 0;
      return true;
   }
   s->num_quads = r->num_quads;
   memcpy(s->quads, r->quads, r->num_quads * sizeof(r->quads[0]));
   s->info      = r->info;
   return true;
}

/* The quads of every slot, each with the image it was drawn with. */
static void gl_openxr_publish_quads(gl_openxr_t *xr)
{
   unsigned s, i;
   video_xr_quad_set_t set;
   memset(&set, 0, sizeof(set));
   for (s = 0; s < OPENXR_MAX_SLOTS; s++)
   {
      const gl_openxr_slot_t *slot = &xr->slots[s];
      if (s < VIDEO_XR_MAX_SLOTS && slot->num_quads)
         set.slots[s] = slot->info;
      for (i = 0; i < slot->num_quads && set.num_quads < VIDEO_XR_MAX_QUADS;
            i++)
         set.quads[set.num_quads++] = slot->quads[i];
   }
   openxr_frame_publish(&xr->frame, &set);
}

/* The XR thread, after each xrWaitFrame, outside the graphics lock. */
static void gl_openxr_thread_fill(void *user, XrTime display_time)
{
   unsigned s;
   bool changed    = false;
   bool screens    = false;
   gl_openxr_t *xr = (gl_openxr_t*)user;
   const unsigned p = OPENXR_PROJECTION_SLOT;
   for (s = 0; s < OPENXR_MAX_SLOTS; s++)
      gl_openxr_take(xr, s);
   /* The tracked eyes before the screens that yield to them; their
    * clearing after the screens that come back, once those show. */
   if (gl_openxr_waiting(xr, p) && gl_openxr_fill_slot(xr, p))
      changed = true;
   for (s = 0; s < VIDEO_XR_MAX_SLOTS; s++)
   {
      if (gl_openxr_fill_slot(xr, s))
         changed = true;
      if (gl_openxr_waiting(xr, s))
         screens = true;
   }
   if (     !screens && xr->slots[p].pending && !gl_openxr_waiting(xr, p)
         && gl_openxr_fill_slot(xr, p))
      changed = true;
   if (changed)
      gl_openxr_publish_quads(xr);
}

static void gl_openxr_host_register(gl_openxr_t *xr)
{
   video_xr_host_t host;
   memset(&host, 0, sizeof(host));
   host.state     = gl_openxr_host_state;
   host.sample    = gl_openxr_host_sample;
   host.eye_state = gl_openxr_host_eye_state;
   host.give      = gl_openxr_host_give;
   host.set_space = gl_openxr_host_set_space;
   host.head_pose = gl_openxr_host_head_pose;
   host.locate_hands = gl_openxr_host_locate_hands;
   host.controller_poses = gl_openxr_host_controller_poses;
   host.eye_size  = gl_openxr_host_eye_size;
   host.resume    = gl_openxr_host_resume;
   host.user      = xr;
   video_driver_set_xr_host(&host);
}

bool gl_openxr_start(gl_openxr_t *xr)
{
   if (xr->started)
      return true;
   xr->frame.thread_hooks.start = gl_openxr_thread_start;
   xr->frame.thread_hooks.fill  = gl_openxr_thread_fill;
   xr->frame.thread_hooks.stop  = gl_openxr_thread_stop;
   xr->frame.thread_hooks.user  = xr;
   if (!openxr_frame_start(&xr->frame))
   {
      RARCH_WARN("[OpenXR] Continuing without headset output.\n");
      openxr_runtime_notify(MSG_OPENXR_FAILED);
      return false;
   }
   xr->started = true;
   gl_openxr_host_register(xr);
   return true;
}

void gl_openxr_stop_thread(gl_openxr_t *xr)
{
   if (!xr || !xr->started)
      return;
   /* While the session still shows what the core drew. */
   openxr_host_keep(&xr->host);
   openxr_frame_stop(&xr->frame);
   video_driver_set_xr_host(NULL);
   openxr_frame_clear_quads(&xr->frame);
   openxr_frame_clear_projection(&xr->frame);
   xr->started = false;
}

void gl_openxr_free(gl_openxr_t *xr)
{
   unsigned s, r;
   if (!xr)
      return;
   gl_openxr_stop_thread(xr);
   /* The video thread's: the textures and the draws' fences. */
   if (xr->gl.DeleteTextures)
      for (s = 0; s < OPENXR_MAX_SLOTS; s++)
         for (r = 0; r < OPENXR_RING_SIZE; r++)
         {
            gl_openxr_record_t *rec = &xr->slots[s].rec[r];
            if (rec->layers)
               xr->gl.DeleteTextures((GLsizei)rec->layers, rec->tex);
            if (rec->drawn)
               xr->gl.DeleteSync(rec->drawn);
         }
   if (xr->shared_made)
      xr->ctx_driver->shared_context_free(&xr->shared);
   openxr_runtime_deinit(&xr->rt);
   openxr_frame_deinit(&xr->frame);
   free(xr);
}

bool gl_openxr_slot_target(gl_openxr_t *xr, unsigned slot, unsigned dims,
      unsigned layers, uint32_t textures[2])
{
   unsigned l;
   gl_openxr_slot_t *s;
   gl_openxr_record_t *r;

   if (     slot >= OPENXR_MAX_SLOTS || !VIDEO_SCALE_W(dims)
         || !VIDEO_SCALE_H(dims) || layers < 1 || layers > 2)
      return false;
   s = &xr->slots[slot];
   r = &s->rec[openxr_ring_back(&s->ring)];
   /* The XR thread's last copy from these textures finishes first. Its
    * fence stays its own to delete. */
   if (r->copied)
      xr->gl.WaitSync(r->copied, 0, GL_TIMEOUT_IGNORED);
   if (r->dims != dims || r->layers != layers)
   {
      GLint bound = 0;
      GLsizei w   = (GLsizei)VIDEO_SCALE_W(dims);
      GLsizei h   = (GLsizei)VIDEO_SCALE_H(dims);
      xr->gl.GetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
      if (r->layers)
         xr->gl.DeleteTextures((GLsizei)r->layers, r->tex);
      r->tex[0] = r->tex[1] = 0;
      r->dims   = 0;
      r->layers = 0;
      xr->gl.GenTextures((GLsizei)layers, r->tex);
      /* One level, so complete: copies refuse incomplete textures. */
      for (l = 0; l < layers; l++)
      {
         xr->gl.BindTexture(GL_TEXTURE_2D, r->tex[l]);
         if (xr->texture_storage)
            xr->gl.TexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, w, h);
         else
         {
            xr->gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA,
                  GL_UNSIGNED_BYTE, NULL);
            xr->gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
         }
         xr->gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
               GL_LINEAR);
      }
      xr->gl.BindTexture(GL_TEXTURE_2D, (GLuint)bound);
      r->dims   = dims;
      r->layers = layers;
   }
   textures[0] = r->tex[0];
   textures[1] = (layers > 1) ? r->tex[1] : 0;
   return true;
}

void gl_openxr_slot_publish(gl_openxr_t *xr, unsigned slot,
      const video_xr_quad_set_t *quads,
      const openxr_projection_t *projection)
{
   unsigned i;
   gl_openxr_slot_t *s;
   gl_openxr_record_t *r;
   GLsync drawn;
   if (slot >= OPENXR_MAX_SLOTS)
      return;
   s = &xr->slots[slot];
   r = &s->rec[openxr_ring_back(&s->ring)];
   if (!r->dims || !(drawn = xr->gl.FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,
               0)))
      return;
   /* Flushed, so the XR thread's wait on it can end. */
   xr->gl.Flush();
   if (r->drawn)
      xr->gl.DeleteSync(r->drawn);
   r->drawn     = drawn;
   r->empty     = false;
   r->yield     = false;
   r->num_quads = 0;
   memset(&r->info, 0, sizeof(r->info));
   if (quads)
   {
      for (i = 0; i < quads->num_quads; i++)
         if (quads->quads[i].slot == slot)
            r->quads[r->num_quads++] = quads->quads[i];
      if (slot < VIDEO_XR_MAX_SLOTS)
         r->info = quads->slots[slot];
   }
   memset(&r->projection, 0, sizeof(r->projection));
   if (projection && slot == OPENXR_PROJECTION_SLOT)
      r->projection = *projection;
   openxr_ring_publish(&s->ring);
}

void gl_openxr_slot_forget(gl_openxr_t *xr, unsigned slot)
{
   gl_openxr_slot_t *s;
   gl_openxr_record_t *r;
   if (slot >= OPENXR_MAX_SLOTS)
      return;
   s            = &xr->slots[slot];
   r            = &s->rec[openxr_ring_back(&s->ring)];
   r->empty     = true;
   r->yield     = false;
   r->num_quads = 0;
   memset(&r->projection, 0, sizeof(r->projection));
   openxr_ring_publish(&s->ring);
}

void gl_openxr_slot_yield(gl_openxr_t *xr, unsigned slot)
{
   gl_openxr_slot_t *s;
   gl_openxr_record_t *r;
   if (slot >= VIDEO_XR_MAX_SLOTS)
      return;
   s            = &xr->slots[slot];
   r            = &s->rec[openxr_ring_back(&s->ring)];
   r->empty     = true;
   r->yield     = true;
   r->num_quads = 0;
   memset(&r->projection, 0, sizeof(r->projection));
   openxr_ring_publish(&s->ring);
}

unsigned gl_openxr_slot_refused(gl_openxr_t *xr, unsigned slot)
{
   if (slot >= OPENXR_MAX_SLOTS)
      return 0;
   return (unsigned)retro_atomic_load_acquire_int(&xr->slots[slot].refused);
}

void gl_openxr_projection(gl_openxr_t *xr, unsigned width,
      unsigned height, openxr_projection_t *out)
{
   openxr_host_projection(&xr->host, width, height, out);
}

bool gl_openxr_alive(gl_openxr_t *xr)
{
   return openxr_session_alive(&xr->session);
}

bool gl_openxr_should_draw(gl_openxr_t *xr)
{
   return openxr_session_visible(&xr->session);
}

float gl_openxr_pixels_per_radian(gl_openxr_t *xr)
{
   return openxr_frame_pixels_per_radian(&xr->frame);
}

float gl_openxr_refresh_rate(gl_openxr_t *xr)
{
   return openxr_frame_refresh_rate(&xr->frame);
}

void gl_openxr_set_pacing(gl_openxr_t *xr, unsigned interval)
{
   openxr_frame_set_pacing(&xr->frame, interval);
}

void gl_openxr_pace_wait(gl_openxr_t *xr)
{
   openxr_frame_pace_wait(&xr->frame);
}

void gl_openxr_host_poll(gl_openxr_t *xr)
{
   openxr_frame_host_poll(&xr->frame);
}

void gl_openxr_pace_skip(gl_openxr_t *xr)
{
   openxr_frame_pace_skip(&xr->frame);
}

unsigned gl_openxr_refresh_rates(const gl_openxr_t *xr, float *rates,
      unsigned cap)
{
   return openxr_frame_refresh_rates(&xr->frame, rates, cap);
}

void gl_openxr_request_rate(gl_openxr_t *xr, float hz)
{
   openxr_frame_request_rate(&xr->frame, hz);
}

unsigned gl_openxr_max_dim(const gl_openxr_t *xr)
{
   return xr->rt.max_dim;
}

void gl_openxr_set_hidden(gl_openxr_t *xr, bool hidden)
{
   retro_atomic_store_release_int(&xr->hidden, hidden ? 1 : 0);
}

bool gl_openxr_has_given(const gl_openxr_t *xr)
{
   return openxr_host_has_given(&xr->host);
}

void gl_openxr_place_kept(gl_openxr_t *xr)
{
   openxr_host_place_kept(&xr->host);
}

void gl_openxr_new_frame(gl_openxr_t *xr)
{
   openxr_host_new_frame(&xr->host);
}

unsigned gl_openxr_eye_dims(const gl_openxr_t *xr)
{
   unsigned w = xr->rt.rec_width;
   unsigned h = xr->rt.rec_height;
   if (!w || !h)
      return 0;
   return VIDEO_SCALE_PACK(MIN(w, xr->rt.max_dim), MIN(h, xr->rt.max_dim));
}

void gl_openxr_request_recenter(gl_openxr_t *xr)
{
   openxr_frame_request_recenter(&xr->frame);
}

void gl_openxr_set_placement(gl_openxr_t *xr, unsigned placement)
{
   openxr_frame_set_placement(&xr->frame, placement);
}
