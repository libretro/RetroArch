/*
Copyright (c) 2013, Broadcom Europe Ltd
Copyright (c) 2013, Tim Gover
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

#ifndef RASPITEX_H_
#define RASPITEX_H_

#include <stdio.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
#include "interface/khronos/include/EGL/eglext_brcm.h"
#include "interface/mmal/mmal.h"

#define RASPITEX_VERSION_MAJOR 1
#define RASPITEX_VERSION_MINOR 0

typedef enum
{
   RASPITEX_SCENE_SQUARE = 0,
   RASPITEX_SCENE_MIRROR,
   RASPITEX_SCENE_TEAPOT,
   RASPITEX_SCENE_YUV,
   RASPITEX_SCENE_SOBEL,
   RASPITEX_SCENE_VCSM_SQUARE,

} RASPITEX_SCENE_T;

struct RASPITEX_STATE;

typedef struct RASPITEX_SCENE_OPS
{
   /// Creates a native window that will be used by egl_init
   /// to create a window surface.
   int (*create_native_window)(struct RASPITEX_STATE *state);

   /// Creates EGL surface for native window
   int (*gl_init)(struct RASPITEX_STATE *state);

   /// Updates the RGBX texture from the next MMAL buffer
   /// Set to null if this texture type is not required
   int (*update_texture)(struct RASPITEX_STATE *state, EGLClientBuffer mm_buf);

   /// Updates the Y' plane texture from the next MMAL buffer
   /// Set to null if this texture type is not required
   int (*update_y_texture)(struct RASPITEX_STATE *state, EGLClientBuffer mm_buf);

   /// Updates the U plane texture from the next MMAL buffer
   /// Set to null if this texture type is not required
   int (*update_u_texture)(struct RASPITEX_STATE *state, EGLClientBuffer mm_buf);

   /// Updates the V plane texture from the next MMAL buffer
   /// Set to null if this texture type is not required
   int (*update_v_texture)(struct RASPITEX_STATE *state, EGLClientBuffer mm_buf);

   /// Advance to the next animation step
   int (*update_model)(struct RASPITEX_STATE *state);

   /// Draw the scene - called after update_model
   int (*redraw)(struct RASPITEX_STATE *state);

   /// Allocates a buffer and copies the pixels from the current
   /// frame-buffer into it.
   int (*capture)(struct RASPITEX_STATE *state,
                  uint8_t **buffer, size_t *buffer_size);

   /// Creates EGL surface for native window
   void (*gl_term)(struct RASPITEX_STATE *state);

   /// Destroys the native window
   void (*destroy_native_window)(struct RASPITEX_STATE *state);

   /// Called when the scene is unloaded
   void (*close)(struct RASPITEX_STATE *state);
} RASPITEX_SCENE_OPS;

typedef struct RASPITEX_CAPTURE
{
   /// Wait for previous capture to complete
   VCOS_SEMAPHORE_T start_sem;

   /// Posted once the capture is complete
   VCOS_SEMAPHORE_T completed_sem;

   /// The RGB capture buffer
   uint8_t *buffer;

   /// Size of the captured buffer in bytes
   size_t size;

   /// Frame-buffer capture has been requested. Could use
   /// a queue instead here to allow multiple capture requests.
   int request;
} RASPITEX_CAPTURE;

/**
 * Contains the internal state and configuration for the GL rendered
 * preview window.
 */
typedef struct RASPITEX_STATE
{
   int version_major;                  /// For binary compatibility
   int version_minor;                  /// Incremented for new features
   MMAL_PORT_T *preview_port;          /// Source port for preview opaque buffers
   MMAL_POOL_T *preview_pool;          /// Pool for storing opaque buffer handles
   MMAL_QUEUE_T *preview_queue;        /// Queue preview buffers to display in order
   VCOS_THREAD_T preview_thread;       /// Preview worker / GL rendering thread
   uint32_t preview_stop;              /// If zero the worker can continue

   /* Copy of preview window params */
   int32_t preview_x;                  /// x-offset of preview window
   int32_t preview_y;                  /// y-offset of preview window
   int32_t preview_width;              /// preview y-plane width in pixels
   int32_t preview_height;             /// preview y-plane height in pixels

   /* Display rectangle for the native window */
   int32_t x;                          /// x-offset in pixels
   int32_t y;                          /// y-offset in pixels
   int32_t width;                      /// width in pixels
   int32_t height;                     /// height in pixels
   int opacity;                        /// Alpha value for display element
   int gl_win_defined;                 /// Use rect from --glwin instead of preview

   /* DispmanX info. This might be unused if a custom create_native_window
    * does something else. */
   DISPMANX_DISPLAY_HANDLE_T disp;     /// Dispmanx display for GL preview
   EGL_DISPMANX_WINDOW_T win;          /// Dispmanx handle for preview surface

   EGLNativeWindowType* native_window; /// Native window used for EGL surface
   EGLDisplay display;                 /// The current EGL display
   EGLSurface surface;                 /// The current EGL surface
   EGLContext context;                 /// The current EGL context
   const EGLint *egl_config_attribs;   /// GL scenes preferred EGL configuration

   GLuint texture;                     /// Name for the preview texture
   EGLImageKHR egl_image;              /// The current preview EGL image

   GLuint y_texture;                   /// The Y plane texture
   EGLImageKHR y_egl_image;            /// EGL image for Y plane texture
   GLuint u_texture;                   /// The U plane texture
   EGLImageKHR u_egl_image;            /// EGL image for U plane texture
   GLuint v_texture;                   /// The V plane texture
   EGLImageKHR v_egl_image;            /// EGL image for V plane texture

   MMAL_BUFFER_HEADER_T *preview_buf;  /// MMAL buffer currently bound to texture(s)

   RASPITEX_SCENE_T scene_id;          /// Id of the scene to load
   RASPITEX_SCENE_OPS ops;             /// The interface for the current scene
   void *scene_state;                  /// Pointer to scene specific data
   int verbose;                        /// Log FPS

   RASPITEX_CAPTURE capture;           /// Frame-buffer capture state

} RASPITEX_STATE;

int raspitex_init(RASPITEX_STATE *state);
void raspitex_destroy(RASPITEX_STATE *state);
int raspitex_start(RASPITEX_STATE *state);
void raspitex_stop(RASPITEX_STATE *state);
void raspitex_set_defaults(RASPITEX_STATE *state);
int raspitex_configure_preview_port(RASPITEX_STATE *state,
                                    MMAL_PORT_T *preview_port);
void raspitex_display_help();
int raspitex_parse_cmdline(RASPITEX_STATE *state,
                           const char *arg1, const char *arg2);
int raspitex_capture(RASPITEX_STATE *state, FILE* output_file);

#endif /* RASPITEX_H_ */
