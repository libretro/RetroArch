/*  RetroArch - A frontend for libretro.
 *  Copyright (c) 2011-2017 - Daniel De Matteis
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

#ifndef __EGL_COMMON_H
#define __EGL_COMMON_H

#ifdef HAVE_GBM
/* presense or absense of this include makes egl.h change NativeWindowType between gbm_device* and _XDisplay* */
#include <gbm.h>
#endif
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "../../retroarch.h"

#ifndef EGL_CONTEXT_FLAGS_KHR
#define EGL_CONTEXT_FLAGS_KHR 0x30FC
#endif

#ifndef EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR
#define EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR 0x00000001
#endif

RETRO_BEGIN_DECLS

/* Put this structure as the first member of egl-based contexts
 * like this:
 *    typedef struct
 *    {
 *       egl_ctx_data_t egl;
 *       int member0;
 *       char member1;
 *     ....
 *    } my_ctx_data_t;
 *
 * You can call egl functions passing the data pointer you receive
 * or using &ctx_data->egl. It's up to you.
 */
typedef struct
{
   EGLContext ctx;
   EGLContext hw_ctx;
   EGLSurface surf;
   EGLDisplay dpy;
   EGLConfig config;
   int interval;

   unsigned major;
   unsigned minor;

   /* egl "private" */
   bool use_hw_ctx;
} egl_ctx_data_t;

extern bool g_egl_inited;

/* bind_api is called before init so we need these, please
 * try no to use them outside of bind_api() and init() */
extern unsigned g_egl_major;
extern unsigned g_egl_minor;

void egl_report_error(void);

void egl_destroy(egl_ctx_data_t *egl);

gfx_ctx_proc_t egl_get_proc_address(const char *symbol);

void egl_terminate(EGLDisplay dpy);

void egl_bind_hw_render(egl_ctx_data_t *egl, bool enable);

void egl_swap_buffers(void *data);

void egl_set_swap_interval(egl_ctx_data_t *egl, int interval);

void egl_get_video_size(egl_ctx_data_t *egl, unsigned *width, unsigned *height);

typedef bool (*egl_accept_config_cb_t)(void *display_data, EGLDisplay dpy, EGLConfig config);
bool egl_default_accept_config_cb(void *display_data, EGLDisplay dpy, EGLConfig config);

bool egl_initialize(EGLDisplay dpy, EGLint *major, EGLint *minor);

bool egl_init_dll(void);

bool egl_init_context_common(
      egl_ctx_data_t *egl, EGLint *count,
      const EGLint *attrib_ptr,
      egl_accept_config_cb_t cb,
      void *display_data);

bool egl_init_context(egl_ctx_data_t *egl,
      EGLenum platform,
      void *display_data,
      EGLint *major,
      EGLint *minor,
      EGLint *n,
      const EGLint *attrib_ptr,
      egl_accept_config_cb_t cb);

bool egl_bind_api(EGLenum egl_api);

bool egl_create_context(egl_ctx_data_t *egl, const EGLint *egl_attribs);

bool egl_create_surface(egl_ctx_data_t *egl, void *native_window);

bool egl_get_native_visual_id(egl_ctx_data_t *egl, EGLint *value);

bool egl_get_config_attrib(EGLDisplay dpy, EGLConfig config,
      EGLint attribute, EGLint *value);

bool egl_has_config(egl_ctx_data_t *egl);

RETRO_END_DECLS

#endif
