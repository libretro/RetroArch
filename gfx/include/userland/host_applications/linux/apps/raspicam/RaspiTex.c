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

#include "RaspiTex.h"
#include "RaspiCLI.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
#include "RaspiTexUtil.h"
#include "interface/vcos/vcos.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "tga.h"

#include "gl_scenes/mirror.h"
#include "gl_scenes/sobel.h"
#include "gl_scenes/square.h"
#include "gl_scenes/teapot.h"
#include "gl_scenes/vcsm_square.h"
#include "gl_scenes/yuv.h"

/**
 * \file RaspiTex.c
 *
 * A simple framework for extending a MMAL application to render buffers via
 * OpenGL.
 *
 * MMAL buffers are often in YUV colour space and in either a planar or
 * tile format which is not supported directly by V3D. Instead of copying
 * the buffer from the GPU and doing a colour space / pixel format conversion
 * the GL_OES_EGL_image_external is used. This allows an EGL image to be
 * created from GPU buffer handle (MMAL opaque buffer handle). The EGL image
 * may then be used to create a texture (glEGLImageTargetTexture2DOES) and
 * drawn by either OpenGL ES 1.0 or 2.0 contexts.
 *
 * Notes:
 * 1) GL_OES_EGL_image_external textures always return pixels in RGBA format.
 *    This is also the case when used from a fragment shader.
 *
 * 2) The driver implementation creates a new RGB_565 buffer and does the color
 *    space conversion from YUV. This happens in GPU memory using the vector
 *    processor.
 *
 * 3) Each EGL external image in use will consume GPU memory for the RGB 565
 *    buffer. In addition, the GL pipeline might require more than one EGL image
 *    to be retained in GPU memory until the drawing commands are flushed.
 *
 *    Typically 128 MB of GPU memory is sufficient for 720p viewfinder and 720p
 *    GL surface. If both the viewfinder and the GL surface are 1080p then
 *    256MB of GPU memory is recommended, otherwise, for non-trivial scenes
 *    the system can run out of GPU memory whilst the camera is running.
 *
 * 4) It is important to make sure that the MMAL opaque buffer is not returned
 *    to MMAL before the GL driver has completed the asynchronous call to
 *    glEGLImageTargetTexture2DOES. Deferring destruction of the EGL image and
 *    the buffer return to MMAL until after eglSwapBuffers is the recommended.
 *
 * See also: http://www.khronos.org/registry/gles/extensions/OES/OES_EGL_image_external.txt
 */

#define DEFAULT_WIDTH   640
#define DEFAULT_HEIGHT  480

enum
{
   CommandGLScene,
   CommandGLWin
};

static COMMAND_LIST cmdline_commands[] =
{
   { CommandGLScene, "-glscene",  "gs",  "GL scene square,teapot,mirror,yuv,sobel,vcsm_square", 1 },
   { CommandGLWin,   "-glwin",    "gw",  "GL window settings <'x,y,w,h'>", 1 },
};

static int cmdline_commands_size = sizeof(cmdline_commands) / sizeof(cmdline_commands[0]);

/**
 * Parse a possible command pair - command and parameter
 * @param arg1 Command
 * @param arg2 Parameter (could be NULL)
 * @return How many parameters were used, 0,1,2
 */
int raspitex_parse_cmdline(RASPITEX_STATE *state,
                           const char *arg1, const char *arg2)
{
   int command_id, used = 0, num_parameters;

   if (!arg1)
      return 0;

   command_id = raspicli_get_command_id(cmdline_commands,
                                        cmdline_commands_size, arg1, &num_parameters);

   // If invalid command, or we are missing a parameter, drop out
   if (command_id==-1 || (command_id != -1 && num_parameters > 0 && arg2 == NULL))
      return 0;

   switch (command_id)
   {
   case CommandGLWin: // Allows a GL window to be different to preview-res
   {
      int tmp;
      tmp = sscanf(arg2, "%d,%d,%d,%d",
                   &state->x, &state->y, &state->width, &state->height);
      if (tmp != 4)
      {
         // Default to safe size on parse error
         state->x = state->y = 0;
         state->width = DEFAULT_WIDTH;
         state->height = DEFAULT_HEIGHT;
      }
      else
      {
         state->gl_win_defined = 1;
      }

      used = 2;
      break;
   }

   case CommandGLScene: // Selects the GL scene
   {
      if (strcmp(arg2, "square") == 0)
         state->scene_id = RASPITEX_SCENE_SQUARE;
      else if (strcmp(arg2, "teapot") == 0)
         state->scene_id = RASPITEX_SCENE_TEAPOT;
      else if (strcmp(arg2, "mirror") == 0)
         state->scene_id = RASPITEX_SCENE_MIRROR;
      else if (strcmp(arg2, "yuv") == 0)
         state->scene_id = RASPITEX_SCENE_YUV;
      else if (strcmp(arg2, "sobel") == 0)
         state->scene_id = RASPITEX_SCENE_SOBEL;
      else if (strcmp(arg2, "vcsm_square") == 0)
         state->scene_id = RASPITEX_SCENE_VCSM_SQUARE;
      else
         vcos_log_error("Unknown scene %s", arg2);

      used = 2;
      break;
   }
   }
   return used;
}

/**
 * Display help for command line options
 */
void raspitex_display_help()
{
   fprintf(stdout, "\nGL parameter commands\n\n");
   raspicli_display_help(cmdline_commands, cmdline_commands_size);
}

static void update_fps()
{
   static int frame_count = 0;
   static long long time_start = 0;
   long long time_now;
   struct timeval te;
   float fps;

   frame_count++;

   gettimeofday(&te, NULL);
   time_now = te.tv_sec * 1000LL + te.tv_usec / 1000;

   if (time_start == 0)
   {
      time_start = time_now;
   }
   else if (time_now - time_start > 5000)
   {
      fps = (float) frame_count / ((time_now - time_start) / 1000.0);
      frame_count = 0;
      time_start = time_now;
      vcos_log_error("%3.2f FPS", fps);
   }
}

/**
 * Captures the frame-buffer if requested.
 * @param state RASPITEX STATE
 * @return Zero if successful.
 */
static void raspitex_do_capture(RASPITEX_STATE *state)
{
   uint8_t *buffer = NULL;
   size_t size = 0;

   if (state->capture.request)
   {
      if (state->ops.capture(state, &buffer, &size) == 0)
      {
         /* Pass ownership of buffer to main thread via capture state */
         state->capture.buffer = buffer;
         state->capture.size = size;
      }
      else
      {
         state->capture.buffer = NULL; // Null indicates an error
         state->capture.size = 0;
      }

      state->capture.request = 0; // Always clear request and post sem
      vcos_semaphore_post(&state->capture.completed_sem);
   }
}

/**
 * Checks if there is at least one valid EGL image.
 * @param state RASPITEX STATE
 * @return Zero if successful.
 */
static int check_egl_image(RASPITEX_STATE *state)
{
   if (state->egl_image == EGL_NO_IMAGE_KHR &&
         state->y_egl_image == EGL_NO_IMAGE_KHR &&
         state->u_egl_image == EGL_NO_IMAGE_KHR &&
         state->v_egl_image == EGL_NO_IMAGE_KHR)
      return -1;
   else
      return 0;
}

/**
 * Draws the next preview frame. If a new preview buffer is available then the
 * preview texture is updated first.
 *
 * @param state RASPITEX STATE
 * @param video_frame MMAL buffer header containing the opaque buffer handle.
 * @return Zero if successful.
 */
static int raspitex_draw(RASPITEX_STATE *state, MMAL_BUFFER_HEADER_T *buf)
{
   int rc = 0;

   /* If buf is non-NULL then there is a new viewfinder frame available
    * from the camera so the texture should be updated.
    *
    * Although it's possible to have multiple textures mapped to different
    * viewfinder frames this can consume a lot of GPU memory for high-resolution
    * viewfinders.
    */
   if (buf)
   {
      /* Update the texture to the new viewfinder image which should */
      if (state->ops.update_texture)
      {
         rc = state->ops.update_texture(state, (EGLClientBuffer) buf->data);
         if (rc != 0)
         {
            vcos_log_error("%s: Failed to update RGBX texture %d",
                           VCOS_FUNCTION, rc);
            goto end;
         }
      }

      if (state->ops.update_y_texture)
      {
         rc = state->ops.update_y_texture(state, (EGLClientBuffer) buf->data);
         if (rc != 0)
         {
            vcos_log_error("%s: Failed to update Y' plane texture %d", VCOS_FUNCTION, rc);
            goto end;
         }
      }

      if (state->ops.update_u_texture)
      {
         rc = state->ops.update_u_texture(state, (EGLClientBuffer) buf->data);
         if (rc != 0)
         {
            vcos_log_error("%s: Failed to update U plane texture %d", VCOS_FUNCTION, rc);
            goto end;
         }
      }

      if (state->ops.update_v_texture)
      {
         rc = state->ops.update_v_texture(state, (EGLClientBuffer) buf->data);
         if (rc != 0)
         {
            vcos_log_error("%s: Failed to update V texture %d", VCOS_FUNCTION, rc);
            goto end;
         }
      }

      /* Now return the PREVIOUS MMAL buffer header back to the camera preview. */
      if (state->preview_buf)
         mmal_buffer_header_release(state->preview_buf);

      state->preview_buf = buf;
   }

   /*  Do the drawing */
   if (check_egl_image(state) == 0)
   {
      rc = state->ops.update_model(state);
      if (rc != 0)
         goto end;

      rc = state->ops.redraw(state);
      if (rc != 0)
         goto end;

      raspitex_do_capture(state);

      eglSwapBuffers(state->display, state->surface);
      update_fps();
   }
   else
   {
      // vcos_log_trace("%s: No preview image", VCOS_FUNCTION);
   }

end:
   return rc;
}

/**
 * Process preview buffers.
 *
 * Dequeue each available preview buffer in order and call current redraw
 * function. If no new buffers are available then the render function is
 * invoked anyway.
 * @param   state The GL preview window state.
 * @return Zero if successful.
 */
static int preview_process_returned_bufs(RASPITEX_STATE* state)
{
   MMAL_BUFFER_HEADER_T *buf;
   int new_frame = 0;
   int rc = 0;

   while ((buf = mmal_queue_get(state->preview_queue)) != NULL)
   {
      if (state->preview_stop == 0)
      {
         new_frame = 1;
         rc = raspitex_draw(state, buf);
         if (rc != 0)
         {
            vcos_log_error("%s: Error drawing frame. Stopping.", VCOS_FUNCTION);
            state->preview_stop = 1;
            return rc;
         }
      }
   }

   /* If there were no new frames then redraw the scene again with the previous
    * texture. Otherwise, go round the loop again to see if any new buffers
    * are returned.
    */
   if (! new_frame)
      rc = raspitex_draw(state, NULL);
   return rc;
}

/** Preview worker thread.
 * Ensures camera preview is supplied with buffers and sends preview frames to GL.
 * @param arg  Pointer to state.
 * @return NULL always.
 */
static void *preview_worker(void *arg)
{
   RASPITEX_STATE* state = arg;
   MMAL_PORT_T *preview_port = state->preview_port;
   MMAL_BUFFER_HEADER_T *buf;
   MMAL_STATUS_T st;
   int rc;

   vcos_log_trace("%s: port %p", VCOS_FUNCTION, preview_port);

   rc = state->ops.create_native_window(state);
   if (rc != 0)
      goto end;

   rc = state->ops.gl_init(state);
   if (rc != 0)
      goto end;

   while (state->preview_stop == 0)
   {
      /* Send empty buffers to camera preview port */
      while ((buf = mmal_queue_get(state->preview_pool->queue)) != NULL)
      {
         st = mmal_port_send_buffer(preview_port, buf);
         if (st != MMAL_SUCCESS)
         {
            vcos_log_error("Failed to send buffer to %s", preview_port->name);
         }
      }
      /* Process returned buffers */
      if (preview_process_returned_bufs(state) != 0)
      {
         vcos_log_error("Preview error. Exiting.");
         state->preview_stop = 1;
      }
   }

end:
   /* Make sure all buffers are returned on exit */
   while ((buf = mmal_queue_get(state->preview_queue)) != NULL)
      mmal_buffer_header_release(buf);

   /* Tear down GL */
   state->ops.gl_term(state);
   vcos_log_trace("Exiting preview worker");
   return NULL;
}

/**
 * MMAL Callback from camera preview output port.
 * @param port The camera preview port.
 * @param buf The new preview buffer.
 **/
static void preview_output_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buf)
{
   RASPITEX_STATE *state = (RASPITEX_STATE*) port->userdata;

   if (buf->length == 0)
   {
      vcos_log_trace("%s: zero-length buffer => EOS", port->name);
      state->preview_stop = 1;
      mmal_buffer_header_release(buf);
   }
   else if (buf->data == NULL)
   {
      vcos_log_trace("%s: zero buffer handle", port->name);
      mmal_buffer_header_release(buf);
   }
   else
   {
      /* Enqueue the preview frame for rendering and return to
       * avoid blocking MMAL core.
       */
      mmal_queue_put(state->preview_queue, buf);
   }
}

/* Registers a callback on the camera preview port to receive
 * notifications of new frames.
 * This must be called before rapitex_start and may not be called again
 * without calling raspitex_destroy first.
 *
 * @param state Pointer to the GL preview state.
 * @param port  Pointer to the camera preview port
 * @return Zero if successful.
 */
int raspitex_configure_preview_port(RASPITEX_STATE *state,
                                    MMAL_PORT_T *preview_port)
{
   MMAL_STATUS_T status;
   vcos_log_trace("%s port %p", VCOS_FUNCTION, preview_port);

   /* Enable ZERO_COPY mode on the preview port which instructs MMAL to only
    * pass the 4-byte opaque buffer handle instead of the contents of the opaque
    * buffer.
    * The opaque handle is resolved on VideoCore by the GL driver when the EGL
    * image is created.
    */
   status = mmal_port_parameter_set_boolean(preview_port,
            MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE);
   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Failed to enable zero copy on camera preview port");
      goto end;
   }

   status = mmal_port_format_commit(preview_port);
   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("camera viewfinder format couldn't be set");
      goto end;
   }

   /* For GL a pool of opaque buffer handles must be allocated in the client.
    * These buffers are used to create the EGL images.
    */
   state->preview_port = preview_port;
   preview_port->buffer_num = preview_port->buffer_num_recommended;
   preview_port->buffer_size = preview_port->buffer_size_recommended;

   vcos_log_trace("Creating buffer pool for GL renderer num %d size %d",
                  preview_port->buffer_num, preview_port->buffer_size);

   /* Pool + queue to hold preview frames */
   state->preview_pool = mmal_port_pool_create(preview_port,
                         preview_port->buffer_num, preview_port->buffer_size);

   if (! state->preview_pool)
   {
      vcos_log_error("Error allocating pool");
      status = MMAL_ENOMEM;
      goto end;
   }

   /* Place filled buffers from the preview port in a queue to render */
   state->preview_queue = mmal_queue_create();
   if (! state->preview_queue)
   {
      vcos_log_error("Error allocating queue");
      status = MMAL_ENOMEM;
      goto end;
   }

   /* Enable preview port callback */
   preview_port->userdata = (struct MMAL_PORT_USERDATA_T*) state;
   status = mmal_port_enable(preview_port, preview_output_cb);
   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Failed to camera preview port");
      goto end;
   }
end:
   return (status == MMAL_SUCCESS ? 0 : -1);
}

/* Initialises GL preview state and creates the dispmanx native window.
 * @param state Pointer to the GL preview state.
 * @return Zero if successful.
 */
int raspitex_init(RASPITEX_STATE *state)
{
   VCOS_STATUS_T status;
   int rc;
   vcos_init();

   vcos_log_register("RaspiTex", VCOS_LOG_CATEGORY);
   vcos_log_set_level(VCOS_LOG_CATEGORY,
                      state->verbose ? VCOS_LOG_INFO : VCOS_LOG_WARN);
   vcos_log_trace("%s", VCOS_FUNCTION);

   status = vcos_semaphore_create(&state->capture.start_sem,
                                  "glcap_start_sem", 1);
   if (status != VCOS_SUCCESS)
      goto error;

   status = vcos_semaphore_create(&state->capture.completed_sem,
                                  "glcap_completed_sem", 0);
   if (status != VCOS_SUCCESS)
      goto error;

   switch (state->scene_id)
   {
   case RASPITEX_SCENE_SQUARE:
      rc = square_open(state);
      break;
   case RASPITEX_SCENE_MIRROR:
      rc = mirror_open(state);
      break;
   case RASPITEX_SCENE_TEAPOT:
      rc = teapot_open(state);
      break;
   case RASPITEX_SCENE_YUV:
      rc = yuv_open(state);
      break;
   case RASPITEX_SCENE_SOBEL:
      rc = sobel_open(state);
      break;
   case RASPITEX_SCENE_VCSM_SQUARE:
      rc = vcsm_square_open(state);
      break;
   default:
      rc = -1;
      break;
   }
   if (rc != 0)
      goto error;

   return 0;

error:
   vcos_log_error("%s: failed", VCOS_FUNCTION);
   return -1;
}

/* Destroys the pools of buffers used by the GL renderer.
 * @param  state Pointer to the GL preview state.
 */
void raspitex_destroy(RASPITEX_STATE *state)
{
   vcos_log_trace("%s", VCOS_FUNCTION);
   if (state->preview_pool)
   {
      mmal_pool_destroy(state->preview_pool);
      state->preview_pool = NULL;
   }

   if (state->preview_queue)
   {
      mmal_queue_destroy(state->preview_queue);
      state->preview_queue = NULL;
   }

   if (state->ops.destroy_native_window)
      state->ops.destroy_native_window(state);

   if (state->ops.close)
      state->ops.close(state);

   vcos_semaphore_delete(&state->capture.start_sem);
   vcos_semaphore_delete(&state->capture.completed_sem);
}

/* Initialise the GL / window state to sensible defaults.
 * Also initialise any rendering parameters e.g. the scene
 *
 * @param state Pointer to the GL preview state.
 * @return Zero if successful.
 */
void raspitex_set_defaults(RASPITEX_STATE *state)
{
   memset(state, 0, sizeof(*state));
   state->version_major = RASPITEX_VERSION_MAJOR;
   state->version_minor = RASPITEX_VERSION_MINOR;
   state->display = EGL_NO_DISPLAY;
   state->surface = EGL_NO_SURFACE;
   state->context = EGL_NO_CONTEXT;
   state->egl_image = EGL_NO_IMAGE_KHR;
   state->y_egl_image = EGL_NO_IMAGE_KHR;
   state->u_egl_image = EGL_NO_IMAGE_KHR;
   state->v_egl_image = EGL_NO_IMAGE_KHR;
   state->opacity = 255;
   state->width = DEFAULT_WIDTH;
   state->height = DEFAULT_HEIGHT;
   state->scene_id = RASPITEX_SCENE_SQUARE;

   state->ops.create_native_window = raspitexutil_create_native_window;
   state->ops.gl_init = raspitexutil_gl_init_1_0;
   state->ops.update_model = raspitexutil_update_model;
   state->ops.redraw = raspitexutil_redraw;
   state->ops.capture = raspitexutil_capture_bgra;
   state->ops.gl_term = raspitexutil_gl_term;
   state->ops.destroy_native_window = raspitexutil_destroy_native_window;
   state->ops.close = raspitexutil_close;
}

/* Stops the rendering loop and destroys MMAL resources
 * @param state  Pointer to the GL preview state.
 */
void raspitex_stop(RASPITEX_STATE *state)
{
   if (! state->preview_stop)
   {
      vcos_log_trace("Stopping GL preview");
      state->preview_stop = 1;
      vcos_thread_join(&state->preview_thread, NULL);
   }
}

/**
 * Starts the worker / GL renderer thread.
 * @pre raspitex_init was successful
 * @pre raspitex_configure_preview_port was successful
 * @param state Pointer to the GL preview state.
 * @return Zero on success, otherwise, -1 is returned
 * */
int raspitex_start(RASPITEX_STATE *state)
{
   VCOS_STATUS_T status;

   vcos_log_trace("%s", VCOS_FUNCTION);
   status = vcos_thread_create(&state->preview_thread, "preview-worker",
                               NULL, preview_worker, state);

   if (status != VCOS_SUCCESS)
      vcos_log_error("%s: Failed to start worker thread %d",
                     VCOS_FUNCTION, status);

   return (status == VCOS_SUCCESS ? 0 : -1);
}

/**
 * Writes the next GL frame-buffer to a RAW .ppm formatted file
 * using the specified file-handle.
 * @param state Pointer to the GL preview state.
 * @param outpt_file Output file handle for the ppm image.
 * @return Zero on success.
 */
int raspitex_capture(RASPITEX_STATE *state, FILE *output_file)
{
   int rc = 0;
   uint8_t *buffer = NULL;
   size_t size = 0;

   vcos_log_trace("%s: state %p file %p", VCOS_FUNCTION,
                  state, output_file);

   if (state && output_file)
   {
      /* Only request one capture at a time */
      vcos_semaphore_wait(&state->capture.start_sem);
      state->capture.request = 1;

      /* Wait for capture to start */
      vcos_semaphore_wait(&state->capture.completed_sem);

      /* Take ownership of the captured buffer */
      buffer = state->capture.buffer;
      size = state->capture.size;

      state->capture.request = 0;
      state->capture.buffer = 0;
      state->capture.size = 0;

      /* Allow another capture to be requested */
      vcos_semaphore_post(&state->capture.start_sem);
   }
   if (size == 0 || ! buffer)
   {
      vcos_log_error("%s: capture failed", VCOS_FUNCTION);
      rc = -1;
      goto end;
   }

   raspitexutil_brga_to_rgba(buffer, size);
   rc = write_tga(output_file, state->width, state->height, buffer, size);
   fflush(output_file);

end:
   free(buffer);
   return rc;
}
