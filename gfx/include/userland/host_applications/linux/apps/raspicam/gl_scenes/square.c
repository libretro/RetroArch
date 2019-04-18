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

#include <GLES/gl.h>
#include <GLES/glext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "RaspiTexUtil.h"

/* Vertex co-ordinates:
 *
 * v0----v1
 * |     |
 * |     |
 * |     |
 * v3----v2
 */

static const GLfloat vertices[] =
{
#define V0  -0.8,  0.8,  0.8,
#define V1   0.8,  0.8,  0.8,
#define V2   0.8, -0.8,  0.8,
#define V3  -0.8, -0.8,  0.8,
   V0 V3 V2 V2 V1 V0
};

/* Texture co-ordinates:
 *
 * (0,0) b--c
 *       |  |
 *       a--d
 *
 * b,a,d d,c,b
 */
static const GLfloat tex_coords[] =
{
   0, 0, 0, 1, 1, 1,
   1, 1, 1, 0, 0, 0
};

static GLfloat angle;
static uint32_t anim_step;

static int square_init(RASPITEX_STATE *state)
{
   int rc = raspitexutil_gl_init_1_0(state);

   if (rc != 0)
      goto end;

   angle = 0.0f;
   anim_step = 0;

   glClearColor(0, 0, 0, 0);
   glClearDepthf(1);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   glLoadIdentity();

end:
   return rc;
}

static int square_update_model(RASPITEX_STATE *state)
{
   int frames_per_rev = 30 * 15;
   (void) state;
   angle = (anim_step * 360) / (GLfloat) frames_per_rev;
   anim_step = (anim_step + 1) % frames_per_rev;

   return 0;
}

static int square_redraw(RASPITEX_STATE *state)
{
   /* Bind the OES texture which is used to render the camera preview */
   GLCHK(glBindTexture(GL_TEXTURE_EXTERNAL_OES, state->texture));
   glLoadIdentity();
   glRotatef(angle, 0.0, 0.0, 1.0);
   glEnableClientState(GL_VERTEX_ARRAY);
   glVertexPointer(3, GL_FLOAT, 0, vertices);
   glDisableClientState(GL_COLOR_ARRAY);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
   glTexCoordPointer(2, GL_FLOAT, 0, tex_coords);
   GLCHK(glDrawArrays(GL_TRIANGLES, 0, vcos_countof(tex_coords) / 2));
   return 0;
}

int square_open(RASPITEX_STATE *state)
{
   state->ops.gl_init = square_init;
   state->ops.update_model = square_update_model;
   state->ops.redraw = square_redraw;
   state->ops.update_texture = raspitexutil_update_texture;
   return 0;
}
