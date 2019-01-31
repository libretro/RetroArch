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

#ifndef EGL_DISP_H
#define EGL_DISP_H

#include "interface/khronos/common/khrn_int_common.h"
#include "middleware/khronos/common/khrn_mem.h"

extern bool egl_disp_init(void);
extern void egl_disp_term(void);

typedef enum {
   EGL_DISP_HANDLE_INVALID = -1,
   EGL_DISP_HANDLE_FORCE_32BIT = (int)0x80000000
} EGL_DISP_HANDLE_T;

/* there are only a fixed number of handles available; EGL_DISP_HANDLE_INVALID
 * may be returned even when there is plenty of free memory. todo: fix this?
 *
 * khdispatch_send_async(ASYNC_COMMAND_POST, pid, sem) is called every time an
 * image comes off the display
 *
 * call from master task */
extern EGL_DISP_HANDLE_T egl_disp_alloc(
   uint64_t pid, uint32_t sem,
   uint32_t n, const MEM_HANDLE_T *images);

/* this will not return until all images are off the display
 *
 * call from master task */
extern void egl_disp_free(EGL_DISP_HANDLE_T disp_handle);

/* wait until we're ready to display the next image, ie we're idle. this implies
 * we won't call egl_server_platform_display until the next image comes along */
extern void egl_disp_finish(EGL_DISP_HANDLE_T disp_handle);

/* equivalent to calling egl_disp_finish on all valid handles */
extern void egl_disp_finish_all(void);

/* display the next image in the chain. image_handle is optional -- if provided,
 * it is merely checked against the handle of the image to be displayed
 *
 * we will wait (asynchronously) until the image is actually ready to be
 * displayed (ie there are no outstanding writes) before calling
 * egl_server_platform_display
 *
 * images are displayed in sequence (even if they become ready out of sequence)
 *
 * swap_interval is the minimum display time of the image in v-sync periods. a
 * swap_interval of 0 is special: in addition to not requiring any display time
 * at all, we won't hold back rendering to wait for the (previous) image to
 * come off the display. this can result in tearing
 *
 * the image parameters (size/stride) are read before returning from this
 * function to make resizing (in the case where the storage handle and size
 * aren't changed) safe. this won't prevent visual glitches with swap interval
 * 0
 *
 * call from master task */
extern void egl_disp_next(EGL_DISP_HANDLE_T disp_handle,
   MEM_HANDLE_T image_handle, uint32_t win, uint32_t swap_interval);

typedef enum {
   EGL_DISP_SLOT_HANDLE_INVALID = -1,
   EGL_DISP_SLOT_HANDLE_FORCE_32BIT = (int)0x80000000
} EGL_DISP_SLOT_HANDLE_T;

/* mark the slot as ready. if skip, the swap interval of the image is forced to
 * 0 and we won't actually display the image
 *
 * call from any task */
extern void egl_disp_ready(EGL_DISP_SLOT_HANDLE_T slot_handle, bool skip);

typedef enum {
   EGL_DISP_IMAGE_HANDLE_INVALID = -1,
   EGL_DISP_IMAGE_HANDLE_FORCE_32BIT = (int)0x80000000
} EGL_DISP_IMAGE_HANDLE_T;

/* wait until the image is not on the display. an image is considered to be on
 * the display between an egl_disp_next call that queues the image for display
 * and the image coming off the display
 *
 * call from master task */
extern void egl_disp_wait(EGL_DISP_IMAGE_HANDLE_T image_handle);

/* post a wait-for-display message to fifo. this function will filter out
 * unnecessary waits, but otherwise just forwards (with transformed arguments)
 * to khrn_delayed_wait_for_display(). the wait-for-display message itself
 * should wait until egl_disp_on() returns false
 *
 * call egl_disp_post_wait() from master task. call egl_disp_on() from any
 * task */
extern void egl_disp_post_wait(uint32_t fifo, EGL_DISP_IMAGE_HANDLE_T image_handle);
extern bool egl_disp_on(EGL_DISP_HANDLE_T disp_handle, uint32_t pos);

extern void egl_disp_callback(uint32_t cb_arg);

#endif
