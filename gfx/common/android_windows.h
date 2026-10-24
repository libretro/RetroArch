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

#ifndef __ANDROID_WINDOWS_H
#define __ANDROID_WINDOWS_H

#include <boolean.h>
#include <retro_common_api.h>

#include "../video_windows.h"

RETRO_BEGIN_DECLS

/* The second display's window, on the video thread */
typedef struct android_window
{
   video_window_common_t common;      /* first */
   struct android_second_surface *node; /* the surface shown, or NULL */
   void *gl_surface;                  /* EGLSurface, android_ctx.c's */
   /* The ANativeWindow gl_surface was made for */
   void *gl_window;
} android_window_t;

bool android_windows_create(void *owner,
      const video_window_request_t *request, video_window_native_t *window);
void android_windows_update(void *owner, video_window_native_t *window,
      const video_window_request_t *request);
unsigned android_windows_poll(void *owner, video_window_event_t *events,
      unsigned capacity);
void android_windows_destroy(void *owner, video_window_native_t *window);
/* Window @screen if @owner made it, else NULL */
android_window_t *android_windows_find(void *owner, unsigned screen);

/* Context teardown: hands each window @owner still has to @destroy */
void android_windows_free(void *owner,
      void (*destroy)(void *owner, video_window_native_t *window));

RETRO_END_DECLS

#endif
