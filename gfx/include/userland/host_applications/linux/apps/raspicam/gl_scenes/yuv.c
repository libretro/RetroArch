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

#include "yuv.h"
#include "RaspiTex.h"
#include "RaspiTexUtil.h"
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

/* Draw a scaled quad showing the the entire texture with the
 * origin defined as an attribute */
static RASPITEXUTIL_SHADER_PROGRAM_T yuv_shader =
{
    .vertex_source =
    "attribute vec2 vertex;\n"
    "attribute vec2 top_left;\n"
    "varying vec2 texcoord;\n"
    "void main(void) {\n"
    "   texcoord = vertex + vec2(0.0, 1.0);\n"
    "   gl_Position = vec4(top_left + vertex, 0.0, 1.0);\n"
    "}\n",

    .fragment_source =
    "#extension GL_OES_EGL_image_external : require\n"
    "uniform samplerExternalOES tex;\n"
    "varying vec2 texcoord;\n"
    "void main(void) {\n"
    "    gl_FragColor = texture2D(tex, texcoord);\n"
    "}\n",
    .uniform_names = {"tex"},
    .attribute_names = {"vertex", "top_left"},
};

static GLfloat varray[] =
{
   0.0f, 0.0f, 0.0f, -1.0f, 1.0f, -1.0f,
   1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f,
};

static const EGLint yuv_egl_config_attribs[] =
{
   EGL_RED_SIZE,   8,
   EGL_GREEN_SIZE, 8,
   EGL_BLUE_SIZE,  8,
   EGL_ALPHA_SIZE, 8,
   EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
   EGL_NONE
};

/**
 * Creates the OpenGL ES 2.X context and builds the shaders.
 * @param raspitex_state A pointer to the GL preview state.
 * @return Zero if successful.
 */
static int yuv_init(RASPITEX_STATE *state)
{
    int rc;
    state->egl_config_attribs = yuv_egl_config_attribs;
    rc = raspitexutil_gl_init_2_0(state);
    if (rc != 0)
       goto end;

    rc = raspitexutil_build_shader_program(&yuv_shader);
    GLCHK(glUseProgram(yuv_shader.program));
    GLCHK(glUniform1i(yuv_shader.uniform_locations[0], 0)); // tex unit
end:
    return rc;
}

/**
 * Draws a 2x2 grid with each shell showing the entire MMAL buffer from a
 * different EGL image target.
 */
static int yuv_redraw(RASPITEX_STATE *raspitex_state)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    GLCHK(glUseProgram(yuv_shader.program));
    GLCHK(glActiveTexture(GL_TEXTURE0));
    GLCHK(glEnableVertexAttribArray(yuv_shader.attribute_locations[0]));
    GLCHK(glVertexAttribPointer(yuv_shader.attribute_locations[0],
             2, GL_FLOAT, GL_FALSE, 0, varray));

    // Y plane
    GLCHK(glBindTexture(GL_TEXTURE_EXTERNAL_OES, raspitex_state->y_texture));
    GLCHK(glVertexAttrib2f(yuv_shader.attribute_locations[1], -1.0f, 1.0f));
    GLCHK(glDrawArrays(GL_TRIANGLES, 0, 6));

    // U plane
    GLCHK(glBindTexture(GL_TEXTURE_EXTERNAL_OES, raspitex_state->u_texture));
    GLCHK(glVertexAttrib2f(yuv_shader.attribute_locations[1], 0.0f, 1.0f));
    GLCHK(glDrawArrays(GL_TRIANGLES, 0, 6));

    // V plane
    GLCHK(glBindTexture(GL_TEXTURE_EXTERNAL_OES, raspitex_state->v_texture));
    GLCHK(glVertexAttrib2f(yuv_shader.attribute_locations[1], 0.0f, 0.0f));
    GLCHK(glDrawArrays(GL_TRIANGLES, 0, 6));

    // RGB plane
    GLCHK(glBindTexture(GL_TEXTURE_EXTERNAL_OES, raspitex_state->texture));
    GLCHK(glVertexAttrib2f(yuv_shader.attribute_locations[1], -1.0f, 0.0f));
    GLCHK(glDrawArrays(GL_TRIANGLES, 0, 6));

    GLCHK(glDisableVertexAttribArray(yuv_shader.attribute_locations[0]));
    GLCHK(glUseProgram(0));
    return 0;
}

int yuv_open(RASPITEX_STATE *state)
{
   state->ops.gl_init = yuv_init;
   state->ops.redraw = yuv_redraw;
   state->ops.update_texture = raspitexutil_update_texture;
   state->ops.update_y_texture = raspitexutil_update_y_texture;
   state->ops.update_u_texture = raspitexutil_update_u_texture;
   state->ops.update_v_texture = raspitexutil_update_v_texture;
   return 0;
}
