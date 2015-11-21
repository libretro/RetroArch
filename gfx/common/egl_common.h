/*  RetroArch - A frontend for libretro.
 *  copyright (c) 2011-2015 - Daniel De Matteis
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

volatile sig_atomic_t g_egl_quit;

extern EGLContext g_egl_ctx;
extern EGLContext g_egl_hw_ctx;
extern EGLSurface g_egl_surf;
extern EGLDisplay g_egl_dpy;
extern EGLConfig g_egl_config;
extern enum gfx_ctx_api g_egl_api;
extern bool g_egl_inited;
extern unsigned g_interval;

void egl_report_error(void);

void egl_destroy(void *data);

gfx_ctx_proc_t egl_get_proc_address(const char *symbol);

void egl_bind_hw_render(void *data, bool enable);

void egl_swap_buffers(void *data);

void egl_set_swap_interval(void *data, unsigned interval);

void egl_get_video_size(void *data, unsigned *width, unsigned *height);

void egl_install_sighandlers(void);

bool egl_init_context(NativeDisplayType display,
      EGLint *major, EGLint *minor,
     EGLint *n, const EGLint *attrib_ptr);

bool egl_create_context(EGLint *egl_attribs);

bool egl_create_surface(NativeWindowType native_window);

#endif
