/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdlib.h>
#include <math.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
#include "applog.h"
#include "interface/vcos/vcos.h"
#include "interface/vcos/vcos_stdbool.h"
#include "interface/khronos/include/EGL/eglext_brcm.h"
#include "vidtex.h"

/** Max number of simultaneous EGL images supported = max number of distinct video decoder
 * buffers.
 */
#define VT_MAX_IMAGES  32

/** Maximum permitted difference between the number of EGL buffer swaps and number of video
 * frames.
 */
#define VT_MAX_FRAME_DISPARITY  2

/** Mapping of MMAL opaque buffer handle to EGL image */
typedef struct VIDTEX_IMAGE_SLOT_T
{
   /* Decoded video frame, as MMAL opaque buffer handle. NULL => unused slot. */
   void *video_frame;

   /* Corresponding EGL image */
   EGLImageKHR image;
} VIDTEX_IMAGE_SLOT_T;

/**
 * Video Texture. Displays video from a URI or camera onto an EGL surface.
 */
typedef struct VIDTEX_T
{
   /** Test options; bitmask of VIDTEX_OPT_XXX values */
   uint32_t opts;

   /* Semaphore to synchronise use of decoded frame (video_frame). */
   VCOS_SEMAPHORE_T sem_decoded;

   /* Semaphore to synchronise drawing of video frame. */
   VCOS_SEMAPHORE_T sem_drawn;

   /* Mutex to guard access to quit field. */
   VCOS_MUTEX_T mutex;

   /* Signal thread to quit. */
   bool quit;

   /* Reason for quitting */
   uint32_t stop_reason;

   /* EGL display/surface/context on which to render the video */
   EGLDisplay display;
   EGLSurface surface;
   EGLContext context;

   /* EGL texture name */
   GLuint texture;

   /* Table of EGL images corresponding to MMAL opaque buffer handles */
   VIDTEX_IMAGE_SLOT_T slots[VT_MAX_IMAGES];

   /* Current decoded video frame, as MMAL opaque buffer handle.
    * NULL if no buffer currently available. */
   void *video_frame;

   /* Number of EGL buffer swaps */
   unsigned num_swaps;
} VIDTEX_T;

/* Vertex co-ordinates:
 *
 * v0----v1
 * |     |
 * |     |
 * |     |
 * v3----v2
 */

static const GLfloat vt_vertices[] =
{
#define VT_V0  -0.8,  0.8,  0.8,
#define VT_V1   0.8,  0.8,  0.8,
#define VT_V2   0.8, -0.8,  0.8,
#define VT_V3  -0.8, -0.8,  0.8,
   VT_V0 VT_V3 VT_V2 VT_V2 VT_V1 VT_V0
};

/* Texture co-ordinates:
 *
 * (0,0) b--c
 *       |  |
 *       a--d
 *
 * b,a,d d,c,b
 */
static const GLfloat vt_tex_coords[] =
{
   0, 0, 0, 1, 1, 1,
   1, 1, 1, 0, 0, 0
};

/* Local function prototypes */
static VIDTEX_T *vidtex_create(EGLNativeWindowType win);
static void vidtex_destroy(VIDTEX_T *vt);
static int vidtex_gl_init(VIDTEX_T *vt, EGLNativeWindowType win);
static void vidtex_gl_term(VIDTEX_T *vt);
static void vidtex_destroy_images(VIDTEX_T *vt);
static int vidtex_play(VIDTEX_T *vt, const VIDTEX_PARAMS_T *params);
static void vidtex_check_gl(VIDTEX_T *vt, uint32_t line);
static void vidtex_draw(VIDTEX_T *vt, void *video_frame);
static void vidtex_flush_gl(VIDTEX_T *vt);
static bool vidtex_set_quit(VIDTEX_T *vt, bool quit);
static void vidtex_video_frame_cb(void *ctx, void *ob);
static void vidtex_stop_cb(void *ctx, uint32_t stop_reason);

#define VIDTEX_CHECK_GL(VT) vidtex_check_gl(VT, __LINE__)

/** Create a new vidtex instance */
static VIDTEX_T *vidtex_create(EGLNativeWindowType win)
{
   VIDTEX_T *vt;
   VCOS_STATUS_T st;

   vt = vcos_calloc(1, sizeof(*vt), "vidtex");
   if (vt == NULL)
   {
      vcos_log_trace("Memory allocation failure");
      return NULL;
   }

   st = vcos_semaphore_create(&vt->sem_decoded, "vidtex-dec", 0);
   if (st != VCOS_SUCCESS)
   {
      vcos_log_trace("Error creating semaphore");
      goto error_ctx;
   }

   st = vcos_semaphore_create(&vt->sem_drawn, "vidtex-drw", 0);
   if (st != VCOS_SUCCESS)
   {
      vcos_log_trace("Error creating semaphore");
      goto error_sem1;
   }

   st = vcos_mutex_create(&vt->mutex, "vidtex");
   if (st != VCOS_SUCCESS)
   {
      vcos_log_trace("Error creating semaphore");
      goto error_sem2;
   }

   if (vidtex_gl_init(vt, win) != 0)
   {
      vcos_log_trace("Error initialising EGL");
      goto error_mutex;
   }

   vt->quit = false;
   vt->stop_reason = 0;

   return vt;

error_mutex:
   vcos_mutex_delete(&vt->mutex);
error_sem2:
   vcos_semaphore_delete(&vt->sem_drawn);
error_sem1:
   vcos_semaphore_delete(&vt->sem_decoded);
error_ctx:
   vcos_free(vt);
   return NULL;
}

/** Destroy a vidtex instance */
static void vidtex_destroy(VIDTEX_T *vt)
{
   vidtex_gl_term(vt);
   vcos_mutex_delete(&vt->mutex);
   vcos_semaphore_delete(&vt->sem_drawn);
   vcos_semaphore_delete(&vt->sem_decoded);
   vcos_free(vt);
}

/** Init GL using a native window */
static int vidtex_gl_init(VIDTEX_T *vt, EGLNativeWindowType win)
{
   const EGLint attribs[] =
   {
      EGL_RED_SIZE,   8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE,  8,
      EGL_DEPTH_SIZE, 0,
      EGL_NONE
   };
   EGLConfig config;
   EGLint num_configs;

   vt->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   eglInitialize(vt->display, 0, 0);

   eglChooseConfig(vt->display, attribs, &config, 1, &num_configs);

   vt->surface = eglCreateWindowSurface(vt->display, config, win, NULL);
   vt->context = eglCreateContext(vt->display, config, NULL, NULL);

   if (!eglMakeCurrent(vt->display, vt->surface, vt->surface, vt->context))
   {
      vidtex_gl_term(vt);
      return -1;
   }

   glGenTextures(1, &vt->texture);

   glShadeModel(GL_FLAT);
   glDisable(GL_DITHER);
   glDisable(GL_SCISSOR_TEST);
   glEnable(GL_TEXTURE_EXTERNAL_OES);
   glDisable(GL_TEXTURE_2D);

   return 0;
}

/** Terminate GL */
static void vidtex_gl_term(VIDTEX_T *vt)
{
   vidtex_destroy_images(vt);

   /* Delete texture name */
   glDeleteTextures(1, &vt->texture);

   /* Terminate EGL */
   eglMakeCurrent(vt->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
   eglDestroyContext(vt->display, vt->context);
   eglDestroySurface(vt->display, vt->surface);
   eglTerminate(vt->display);
}

/** Destroy all EGL images */
static void vidtex_destroy_images(VIDTEX_T *vt)
{
   VIDTEX_IMAGE_SLOT_T *slot;

   for (slot = vt->slots; slot < vt->slots + vcos_countof(vt->slots); slot++)
   {
      slot->video_frame = NULL;

      if (slot->image)
      {
         vcos_log_trace("Destroying EGL image %p", slot->image);
         eglDestroyImageKHR(vt->display, slot->image);
         slot->image = NULL;
      }
   }
}

/** Play video - from URI or camera - on EGL surface. */
static int vidtex_play(VIDTEX_T *vt, const VIDTEX_PARAMS_T *params)
{
   const char *uri;
   SVP_CALLBACKS_T callbacks;
   SVP_T *svp;
   SVP_OPTS_T opts;
   SVP_STATS_T stats;
   int rv = -1;

   uri = (params->uri[0] == '\0') ? NULL : params->uri;
   vt->opts = params->opts;
   callbacks.ctx = vt;
   callbacks.video_frame_cb = vidtex_video_frame_cb;
   callbacks.stop_cb = vidtex_stop_cb;
   opts.duration_ms = params->duration_ms;

   svp = svp_create(uri, &callbacks, &opts);
   if (svp)
   {
      /* Reset stats */
      vt->num_swaps = 0;

      /* Run video player until receive quit notification */
      if (svp_start(svp) == 0)
      {
         while (!vidtex_set_quit(vt, false))
         {
            vcos_semaphore_wait(&vt->sem_decoded);

            if (vt->video_frame)
            {
               vidtex_draw(vt, vt->video_frame);
               vcos_semaphore_post(&vt->sem_drawn);
            }
         }

         vcos_semaphore_post(&vt->sem_drawn);

         /* Dump stats */
         svp_get_stats(svp, &stats);
         vcos_log_info("video frames decoded: %6u", stats.video_frame_count);
         vcos_log_info("EGL buffer swaps:     %6u", vt->num_swaps);

         /* Determine status of operation and log errors */
         if (vt->stop_reason & SVP_STOP_ERROR)
         {
            vcos_log_error("vidtex exiting on error");
         }
         else if (vt->num_swaps == 0)
         {
            vcos_log_error("vidtex completed with no EGL buffer swaps");
         }
         else if (abs((int)vt->num_swaps - (int)stats.video_frame_count) > VT_MAX_FRAME_DISPARITY)
         {
            vcos_log_error("vidtex completed with %u EGL buffer swaps, but %u video frames",
                           vt->num_swaps, (int)stats.video_frame_count);
         }
         else
         {
            rv = 0;
         }
      }

      svp_destroy(svp);
   }

   vidtex_flush_gl(vt);

   return rv;
}

/** Check for OpenGL errors - logs any errors and sets quit flag */
static void vidtex_check_gl(VIDTEX_T *vt, uint32_t line)
{
   GLenum error = glGetError();
   int abort = 0;
   while (error != GL_NO_ERROR)
   {
      vcos_log_error("GL error: line %d error 0x%04x", line, error);
      abort = 1;
      error = glGetError();
   }
   if (abort)
      vidtex_stop_cb(vt, SVP_STOP_ERROR);
}

/* Draw one video frame onto EGL surface.
 * @param vt           vidtex instance.
 * @param video_frame  MMAL opaque buffer handle for decoded video frame. Can't be NULL.
 */
static void vidtex_draw(VIDTEX_T *vt, void *video_frame)
{
   EGLImageKHR image;
   VIDTEX_IMAGE_SLOT_T *slot;
   static uint32_t frame_num = 0;

   vcos_assert(video_frame);

   glClearColor(0, 0, 0, 0);
   glClearDepthf(1);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   glLoadIdentity();

   glBindTexture(GL_TEXTURE_EXTERNAL_OES, vt->texture);
   VIDTEX_CHECK_GL(vt);

   /* Lookup or create EGL image corresponding to supplied buffer handle.
    * N.B. Slot array is filled in sequentially, with the images all destroyed together on
    *      vidtex termination; it never has holes. */
   image = EGL_NO_IMAGE_KHR;

   for (slot = vt->slots; slot < vt->slots + vcos_countof(vt->slots); slot++)
   {
      if (slot->video_frame == video_frame)
      {
         vcos_assert(slot->image);
         image = slot->image;
         break;
      }

      if (slot->video_frame == NULL)
      {
         EGLenum target;
         vcos_assert(slot->image == NULL);

         if (vt->opts & VIDTEX_OPT_Y_TEXTURE)
            target = EGL_IMAGE_BRCM_MULTIMEDIA_Y;
         else if (vt->opts & VIDTEX_OPT_U_TEXTURE)
            target = EGL_IMAGE_BRCM_MULTIMEDIA_U;
         else if (vt->opts & VIDTEX_OPT_V_TEXTURE)
            target = EGL_IMAGE_BRCM_MULTIMEDIA_V;
         else
            target = EGL_IMAGE_BRCM_MULTIMEDIA;

         image = eglCreateImageKHR(vt->display, EGL_NO_CONTEXT, target,
               (EGLClientBuffer)video_frame, NULL);
         if (image == EGL_NO_IMAGE_KHR)
         {
            vcos_log_error("EGL image conversion error");
         }
         else
         {
            vcos_log_trace("Created EGL image %p for buf %p", image, video_frame);
            slot->video_frame = video_frame;
            slot->image = image;
         }
         VIDTEX_CHECK_GL(vt);

         break;
      }
   }

   if (slot == vt->slots + vcos_countof(vt->slots))
   {
      vcos_log_error("Exceeded configured max number of EGL images");
   }

   /* Draw the EGL image */
   if (image != EGL_NO_IMAGE_KHR)
   {
      /* Assume 30fps */
      int frames_per_rev = 30 * 15;
      GLfloat angle = (frame_num * 360) / (GLfloat) frames_per_rev;
      frame_num = (frame_num + 1) % frames_per_rev;

      glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, image);
      VIDTEX_CHECK_GL(vt);

      glRotatef(angle, 0.0, 0.0, 1.0);
      glEnableClientState(GL_VERTEX_ARRAY);
      glVertexPointer(3, GL_FLOAT, 0, vt_vertices);
      glDisableClientState(GL_COLOR_ARRAY);
      glEnableClientState(GL_TEXTURE_COORD_ARRAY);
      glTexCoordPointer(2, GL_FLOAT, 0, vt_tex_coords);

      glDrawArrays(GL_TRIANGLES, 0, vcos_countof(vt_tex_coords) / 2);

      eglSwapBuffers(vt->display, vt->surface);

      if (vt->opts & VIDTEX_OPT_IMG_PER_FRAME)
      {
         vidtex_destroy_images(vt);
      }

      vt->num_swaps++;
   }

   VIDTEX_CHECK_GL(vt);
}

/** Do some GL stuff in order to ensure that any multimedia-related GL buffers have been released
 * if they are going to be released.
 */
static void vidtex_flush_gl(VIDTEX_T *vt)
{
   int i;

   glFlush();
   glClearColor(0, 0, 0, 0);

   for (i = 0; i < 10; i++)
   {
      glClear(GL_COLOR_BUFFER_BIT);
      eglSwapBuffers(vt->display, vt->surface);
      VIDTEX_CHECK_GL(vt);
   }

   glFlush();
   VIDTEX_CHECK_GL(vt);
}

/** Set quit flag, with locking.
 * @param quit  New value of the quit flag: true - command thread to quit; false - command thread
 *              to continue.
 * @return Old value of the quit flag.
 */
static bool vidtex_set_quit(VIDTEX_T *vt, bool quit)
{
   vcos_mutex_lock(&vt->mutex);
   bool old_quit = vt->quit;
   vt->quit = quit;
   vcos_mutex_unlock(&vt->mutex);

   return old_quit;
}

/** Callback to receive decoded video frame */
static void vidtex_video_frame_cb(void *ctx, void *ob)
{
   if (ob)
   {
      VIDTEX_T *vt = ctx;
      /* coverity[missing_lock] Coverity gets confused by the semaphore locking scheme */
      vt->video_frame = ob;
      vcos_semaphore_post(&vt->sem_decoded);
      vcos_semaphore_wait(&vt->sem_drawn);
      vt->video_frame = NULL;
   }
}

/** Callback to receive stop notification. Sets quit flag and posts semaphore.
 * @param ctx          VIDTEX_T instance. Declared as void * in order to use as SVP callback.
 * @param stop_reason  SVP stop reason.
 */
static void vidtex_stop_cb(void *ctx, uint32_t stop_reason)
{
   VIDTEX_T *vt = ctx;
   vt->stop_reason = stop_reason;
   vidtex_set_quit(vt, true);
   vcos_semaphore_post(&vt->sem_decoded);
}

/* Convenience function to create/play/destroy */
int vidtex_run(const VIDTEX_PARAMS_T *params, EGLNativeWindowType win)
{
   VIDTEX_T *vt;
   int rv = -1;

   vt = vidtex_create(win);
   if (vt)
   {
      rv = vidtex_play(vt, params);
      vidtex_destroy(vt);
   }

   return rv;
}
