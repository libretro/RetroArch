/*  RetroArch - A frontend for libretro.
 *  Copyright (c) 2011-2016 - Daniel De Matteis
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

#include <signal.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <boolean.h>

#include "../video_context_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

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
   unsigned interval;

   unsigned major;
   unsigned minor;

   /* egl "private" */
   bool use_hw_ctx;
} egl_ctx_data_t;

extern volatile sig_atomic_t g_egl_quit;
extern bool g_egl_inited;

/* bind_api is called before init so we need these, please
 * try no to use them outside of bind_api() and init() */
extern unsigned g_egl_major;
extern unsigned g_egl_minor;

void egl_report_error(void);

void egl_destroy(void *data);

gfx_ctx_proc_t egl_get_proc_address(const char *symbol);

void egl_bind_hw_render(void *data, bool enable);

void egl_swap_buffers(void *data);

void egl_set_swap_interval(void *data, unsigned interval);

void egl_get_video_size(void *data, unsigned *width, unsigned *height);

void egl_install_sighandlers(void);

bool egl_init_context(void *data, NativeDisplayType display,
      EGLint *major, EGLint *minor,
     EGLint *n, const EGLint *attrib_ptr);

bool egl_create_context(void *data, const EGLint *egl_attribs);

bool egl_create_surface(void *data, NativeWindowType native_window);

bool egl_get_native_visual_id(void *data, EGLint *value);

bool egl_has_config(void *data);

#ifdef __cplusplus
}
#endif

#endif
