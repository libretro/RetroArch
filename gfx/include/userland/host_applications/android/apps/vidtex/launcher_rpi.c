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
#include "launcher_rpi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include  "bcm_host.h"

static int create_native_window(EGL_DISPMANX_WINDOW_T *native_win)
{
   int32_t status = 0;
   DISPMANX_DISPLAY_HANDLE_T disp;
   DISPMANX_ELEMENT_HANDLE_T elem;
   DISPMANX_UPDATE_HANDLE_T update;
   VC_DISPMANX_ALPHA_T alpha = {DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS, 255, 0};
   VC_RECT_T src_rect = {0};
   VC_RECT_T dest_rect = {0};
   uint32_t display_width, display_height;
   uint32_t disp_num = 0; // Primary
   uint32_t layer_num = 0;

   status = graphics_get_display_size(0, &display_width, &display_height);
   if (status != 0)
      return status;

   /* Fullscreen */
   display_width = 1280;
   display_height = 720;
   dest_rect.width = display_width;
   dest_rect.height = display_height;
   src_rect.width = display_width << 16;
   src_rect.height = display_height << 16;

   disp = vc_dispmanx_display_open(disp_num);
   update = vc_dispmanx_update_start(0);
   elem = vc_dispmanx_element_add(update, disp, layer_num, &dest_rect, 0,
         &src_rect, DISPMANX_PROTECTION_NONE, &alpha, NULL, DISPMANX_NO_ROTATE);

   native_win->element = elem;
   native_win->width = display_width;
   native_win->height = display_height;
   vc_dispmanx_update_submit_sync(update);

   return 0;
}

int runApp(const char *name, RUN_APP_FN_T run_app_fn, const void *params, size_t param_size)
{
   EGL_DISPMANX_WINDOW_T win;
   (void) param_size;


   vcos_log_trace("Initialsing BCM HOST");
   bcm_host_init();

   vcos_log_trace("Starting '%s'", name);
   if (create_native_window(&win) != 0)
      return -1;

   return run_app_fn(params, (EGLNativeWindowType *) &win);
}

