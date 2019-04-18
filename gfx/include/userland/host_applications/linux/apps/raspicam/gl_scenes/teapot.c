/*
Copyright (c) 2012, Broadcom Europe Ltd
Copyright (c) 2012, OtherCrashOverride
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

// A rotating cube rendered with OpenGL|ES. Three images used as textures on the cube faces.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <unistd.h>

#include <GLES/gl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "cube_texture_and_coords.h"
#include "models.h"
#include "teapot.h"

#include "RaspiTex.h"
#include "RaspiTexUtil.h"

#define PATH "./"

#ifndef M_PI
   #define M_PI 3.141592654
#endif

typedef struct
{
   uint32_t screen_width;
   uint32_t screen_height;
   GLuint tex;
// model rotation vector and direction
   GLfloat rot_angle_x_inc;
   GLfloat rot_angle_y_inc;
   GLfloat rot_angle_z_inc;
// current model rotation angles
   GLfloat rot_angle_x;
   GLfloat rot_angle_y;
   GLfloat rot_angle_z;
// current distance from camera
   GLfloat distance;
   GLfloat distance_inc;
   MODEL_T model;
} TEAPOT_STATE_T;

static void init_ogl(TEAPOT_STATE_T *state);
static void init_model_proj(TEAPOT_STATE_T *state);
static void reset_model(TEAPOT_STATE_T *state);
static GLfloat inc_and_wrap_angle(GLfloat angle, GLfloat angle_inc);
static GLfloat inc_and_clip_distance(GLfloat distance, GLfloat distance_inc);

/***********************************************************
 * Name: init_ogl
 *
 * Arguments:
 *       TEAPOT_STATE_T *state - holds OGLES model info
 *
 * Description: Sets the display, OpenGL|ES context and screen stuff
 *
 * Returns: void
 *
 ***********************************************************/
static void init_ogl(TEAPOT_STATE_T *state)
{
   // Set background color and clear buffers
   glClearColor((0.3922f+7*0.5f)/8, (0.1176f+7*0.5f)/8, (0.5882f+7*0.5f)/8, 1.0f);

   // Enable back face culling.
   glEnable(GL_CULL_FACE);

   glEnable(GL_DEPTH_TEST);
   glClearDepthf(1.0);
   glDepthFunc(GL_LEQUAL);

   float noAmbient[] = {1.0f, 1.0f, 1.0f, 1.0f};
   glLightfv(GL_LIGHT0, GL_AMBIENT, noAmbient);
   glEnable(GL_LIGHT0);
   glEnable(GL_LIGHTING);
}

/***********************************************************
 * Name: init_model_proj
 *
 * Arguments:
 *       TEAPOT_STATE_T *state - holds OGLES model info
 *
 * Description: Sets the OpenGL|ES model to default values
 *
 * Returns: void
 *
 ***********************************************************/
static void init_model_proj(TEAPOT_STATE_T *state)
{
   float nearp = 0.1f;
   float farp = 500.0f;
   float hht;
   float hwd;

   glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );

   glViewport(0, 0, (GLsizei)state->screen_width, (GLsizei)state->screen_height);

   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();

   hht = nearp * (float)tan(45.0 / 2.0 / 180.0 * M_PI);
   hwd = hht * (float)state->screen_width / (float)state->screen_height;

   glFrustumf(-hwd, hwd, -hht, hht, nearp, farp);

   glEnableClientState( GL_VERTEX_ARRAY );

   reset_model(state);
}

/***********************************************************
 * Name: reset_model
 *
 * Arguments:
 *       TEAPOT_STATE_T *state - holds OGLES model info
 *
 * Description: Resets the Model projection and rotation direction
 *
 * Returns: void
 *
 ***********************************************************/
static void reset_model(TEAPOT_STATE_T *state)
{
   // reset model position
   glMatrixMode(GL_MODELVIEW);

   // reset model rotation
   state->rot_angle_x = 45.f; state->rot_angle_y = 30.f; state->rot_angle_z = 0.f;
   state->rot_angle_x_inc = 0.5f; state->rot_angle_y_inc = 0.5f; state->rot_angle_z_inc = 0.f;
   state->distance = 0.8f*1.5f;
}

/***********************************************************
 * Name: teapot_update_model
 *
 * Arguments:
 *       TEAPOT_STATE_T *state - holds OGLES model info
 *
 * Description: Updates model projection to current position/rotation
 *
 * Returns: void
 *
 ***********************************************************/
static int teapot_update_model(RASPITEX_STATE *raspitex_state)
{
   TEAPOT_STATE_T *state = (TEAPOT_STATE_T *) raspitex_state->scene_state;

   // update position
   state->rot_angle_x = inc_and_wrap_angle(state->rot_angle_x, state->rot_angle_x_inc);
   state->rot_angle_y = inc_and_wrap_angle(state->rot_angle_y, state->rot_angle_y_inc);
   state->rot_angle_z = inc_and_wrap_angle(state->rot_angle_z, state->rot_angle_z_inc);
   state->distance    = inc_and_clip_distance(state->distance, state->distance_inc);

   glLoadIdentity();
   // move camera back to see the cube
   glTranslatef(0.f, 0.f, -state->distance);

   // Rotate model to new position
   glRotatef(state->rot_angle_x, 1.f, 0.f, 0.f);
   glRotatef(state->rot_angle_y, 0.f, 1.f, 0.f);
   glRotatef(state->rot_angle_z, 0.f, 0.f, 1.f);

   return 0;
}

/***********************************************************
 * Name: inc_and_wrap_angle
 *
 * Arguments:
 *       GLfloat angle     current angle
 *       GLfloat angle_inc angle increment
 *
 * Description:   Increments or decrements angle by angle_inc degrees
 *                Wraps to 0 at 360 deg.
 *
 * Returns: new value of angle
 *
 ***********************************************************/
static GLfloat inc_and_wrap_angle(GLfloat angle, GLfloat angle_inc)
{
   angle += angle_inc;

   if (angle >= 360.0)
      angle -= 360.f;
   else if (angle <=0)
      angle += 360.f;

   return angle;
}

/***********************************************************
 * Name: inc_and_clip_distance
 *
 * Arguments:
 *       GLfloat distance     current distance
 *       GLfloat distance_inc distance increment
 *
 * Description:   Increments or decrements distance by distance_inc units
 *                Clips to range
 *
 * Returns: new value of angle
 *
 ***********************************************************/
static GLfloat inc_and_clip_distance(GLfloat distance, GLfloat distance_inc)
{
   distance += distance_inc;

   if (distance >= 10.0f)
      distance = 10.f;
   else if (distance <= 1.0f)
      distance = 1.0f;

   return distance;
}

/***********************************************************
 * Name: teapot_redraw
 *
 * Arguments:
 *       RASPITEX_STATE_T *state - holds OGLES model info
 *
 * Description:   Draws the model
 *
 * Returns: void
 *
 ***********************************************************/
static int teapot_redraw(RASPITEX_STATE *raspitex_state)
{
   TEAPOT_STATE_T *state = (TEAPOT_STATE_T *) raspitex_state->scene_state;

   // Start with a clear screen
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   /* Bind the OES texture which is used to render the camera preview */
   glBindTexture(GL_TEXTURE_EXTERNAL_OES, raspitex_state->texture);
   draw_wavefront(state->model, raspitex_state->texture);
   return 0;
}

//==============================================================================

static int teapot_gl_init(RASPITEX_STATE *raspitex_state)
{
   const char *model_path = "/opt/vc/src/hello_pi/hello_teapot/teapot.obj.dat";
   TEAPOT_STATE_T *state = NULL;
   int rc = 0;

   // Clear scene state
   state = calloc(1, sizeof(TEAPOT_STATE_T));
   raspitex_state->scene_state = state;
   state->screen_width = raspitex_state->width;
   state->screen_height = raspitex_state->height;

   rc = raspitexutil_gl_init_1_0(raspitex_state);
   if (rc != 0)
      goto end;

   // Start OGLES
   init_ogl(state);

   // Setup the model world
   init_model_proj(state);
   state->model = load_wavefront(model_path, NULL);

   if (! state->model)
   {
      vcos_log_error("Failed to load model from %s\n", model_path);
      rc = -1;
   }

end:
   return rc;
}

static void teapot_gl_term(RASPITEX_STATE *raspitex_state)
{
   vcos_log_trace("%s:", VCOS_FUNCTION);

   TEAPOT_STATE_T *state = raspitex_state->scene_state;
   if (state)
   {
      if (state->model)
         unload_wavefront(state->model);
      raspitexutil_gl_term(raspitex_state);
      free(raspitex_state->scene_state);
      raspitex_state->scene_state = NULL;
   }
}

int teapot_open(RASPITEX_STATE *raspitex_state)
{
   raspitex_state->ops.gl_init = teapot_gl_init;
   raspitex_state->ops.update_model = teapot_update_model;
   raspitex_state->ops.redraw = teapot_redraw;
   raspitex_state->ops.gl_term = teapot_gl_term;
   raspitex_state->ops.update_texture = raspitexutil_update_texture;
   return 0;
}
