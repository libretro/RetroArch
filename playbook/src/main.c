/*
 * Copyright (c) 2011-2012 Research In Motion Limited.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <assert.h>
#include <screen/screen.h>
#include <bps/navigator.h>
#include <bps/screen.h>
#include <bps/bps.h>
#include <bps/event.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include <math.h>

#include "bbutil.h"

static GLfloat vertices[8];
static GLfloat colors[16];

static GLuint program;

static GLuint transformLoc;
static GLuint positionLoc;
static GLuint colorLoc;

static GLfloat matrix[16] = {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};

static GLuint vertexID;
static GLuint colorID;

static screen_context_t screen_cxt;


void handleScreenEvent(bps_event_t *event) {
    screen_event_t screen_event = screen_event_get_event(event);

    int screen_val;
    screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_TYPE, &screen_val);

    switch (screen_val) {
    case SCREEN_EVENT_MTOUCH_TOUCH:
    case SCREEN_EVENT_MTOUCH_MOVE:
    case SCREEN_EVENT_MTOUCH_RELEASE:
        break;
    }
}

int initialize() {
    //Initialize vertex and color data
    vertices[0] = -0.25f;
    vertices[1] = -0.25f;

    vertices[2] = 0.25f;
    vertices[3] = -0.25f;

    vertices[4] = -0.25f;
    vertices[5] = 0.25f;

    vertices[6] = 0.25f;
    vertices[7] = 0.25f;

    colors[0] = 1.0f;
    colors[1] = 0.0f;
    colors[2] = 1.0f;
    colors[3] = 1.0f;

    colors[4] = 1.0f;
    colors[5] = 1.0f;
    colors[6] = 0.0f;
    colors[7] = 1.0f;

    colors[8] = 0.0f;
    colors[9] = 1.0f;
    colors[10] = 1.0f;
    colors[11] = 1.0f;

    colors[12] = 0.0f;
    colors[13] = 1.0f;
    colors[14] = 1.0f;
    colors[15] = 1.0f;

    //Query width and height of the window surface created by utility code
    EGLint surface_width, surface_height;

    eglQuerySurface(egl_disp, egl_surf, EGL_WIDTH, &surface_width);
    eglQuerySurface(egl_disp, egl_surf, EGL_HEIGHT, &surface_height);

    // Create shaders
    const char* vSource =
            "precision mediump float;"
            "uniform mat4 u_projection;"
            "uniform mat4 u_transform;"
            "attribute vec4 a_position;"
            "attribute vec4 a_color;"
            "varying vec4 v_color;"
            "void main()"
            "{"
            "    gl_Position = u_projection * u_transform * a_position;"
            "    v_color = a_color;"
            "}";

    const char* fSource =
            "varying lowp vec4 v_color;"
            "void main()"
            "{"
            "    gl_FragColor = v_color;"
            "}";

    GLint status;

    // Compile the vertex shader
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    if (!vs) {
        fprintf(stderr, "Failed to create vertex shader: %d\n", glGetError());
        return EXIT_FAILURE;
    } else {
        glShaderSource(vs, 1, &vSource, 0);
        glCompileShader(vs);
        glGetShaderiv(vs, GL_COMPILE_STATUS, &status);
        if (GL_FALSE == status) {
            GLchar log[256];
            glGetShaderInfoLog(vs, 256, NULL, log);

            fprintf(stderr, "Failed to compile vertex shader: %s\n", log);

            glDeleteShader(vs);
        }
    }

    // Compile the fragment shader
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    if (!fs) {
        fprintf(stderr, "Failed to create fragment shader: %d\n", glGetError());
        return EXIT_FAILURE;
    } else {
        glShaderSource(fs, 1, &fSource, 0);
        glCompileShader(fs);
        glGetShaderiv(fs, GL_COMPILE_STATUS, &status);
        if (GL_FALSE == status) {
            GLchar log[256];
            glGetShaderInfoLog(fs, 256, NULL, log);

            fprintf(stderr, "Failed to compile fragment shader: %s\n", log);

            glDeleteShader(vs);
            glDeleteShader(fs);

            return EXIT_FAILURE;
        }
    }

    // Create and link the program
    program = glCreateProgram();
    if (program)
    {
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);

        glGetProgramiv(program, GL_LINK_STATUS, &status);
        if (status == GL_FALSE)    {
            GLchar log[256];
            glGetProgramInfoLog(fs, 256, NULL, log);

            fprintf(stderr, "Failed to link shader program: %s\n", log);

            glDeleteProgram(program);
            program = 0;

            return EXIT_FAILURE;
        }
    } else {
        fprintf(stderr, "Failed to create a shader program\n");

        glDeleteShader(vs);
        glDeleteShader(fs);
        return EXIT_FAILURE;
    }

    glUseProgram(program);

    // Set up the orthographic projection - equivalent to glOrtho in GLES1
    GLuint projectionLoc = glGetUniformLocation(program, "u_projection");
    {
        GLfloat left = 0.0f;
        GLfloat right = (float)(surface_width) / (float)(surface_height);
        GLfloat bottom = 0.0f;
        GLfloat top = 1.0f;
        GLfloat zNear = -1.0f;
        GLfloat zFar = 1.0f;
        GLfloat ortho[16] = {2.0 / (right-left), 0, 0, 0,
                            0, 2.0 / (top-bottom), 0, 0,
                            0, 0, -2.0 / (zFar - zNear), 0,
                            -(right+left)/(right-left), -(top+bottom)/(top-bottom), -(zFar+zNear)/(zFar-zNear), 1};
        glUniformMatrix4fv(projectionLoc, 1, false, ortho);
    }

    // Store the locations of the shader variables we need later
    transformLoc = glGetUniformLocation(program, "u_transform");
    positionLoc = glGetAttribLocation(program, "a_position");
    colorLoc = glGetAttribLocation(program, "a_color");

    // We don't need the shaders anymore - the program is enough
    glDeleteShader(fs);
    glDeleteShader(vs);

    // Generate vertex and color buffers and fill with data
    glGenBuffers(1, &vertexID);
    glBindBuffer(GL_ARRAY_BUFFER, vertexID);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glGenBuffers(1, &colorID);
    glBindBuffer(GL_ARRAY_BUFFER, colorID);
    glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW);

    // Perform the same translation as the GLES1 version
    matrix[12] = (float)(surface_width) / (float)(surface_height) / 2;
    matrix[13] = 0.5;
    glUniformMatrix4fv(transformLoc, 1, false, matrix);

    return EXIT_SUCCESS;
}

void render() {
    // Increment the angle by 0.5 degrees
    static float angle = 0.0f;
    angle += 0.5f * M_PI / 180.0f;

    //Typical render pass
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Enable and bind the vertex information
    glEnableVertexAttribArray(positionLoc);
    glBindBuffer(GL_ARRAY_BUFFER, vertexID);
    glVertexAttribPointer(positionLoc, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), 0);

    // Enable and bind the color information
    glEnableVertexAttribArray(colorLoc);
    glBindBuffer(GL_ARRAY_BUFFER, colorID);
    glVertexAttribPointer(colorLoc, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);

    // Effectively apply a rotation of angle about the y-axis.
    matrix[0] = cos(angle);
    matrix[2] = -sin(angle);
    matrix[8] = sin(angle);
    matrix[10] = cos(angle);
    glUniformMatrix4fv(transformLoc, 1, false, matrix);

    // Same draw call as in GLES1.
    glDrawArrays(GL_TRIANGLE_STRIP, 0 , 4);

    // Disable attribute arrays
    glDisableVertexAttribArray(positionLoc);
    glDisableVertexAttribArray(colorLoc);

    bbutil_swap();
}

int main(int argc, char *argv[]) {
    int rc;
    int exit_application = 0;

    //Create a screen context that will be used to create an EGL surface to to receive libscreen events
    screen_create_context(&screen_cxt, 0);

    //Initialize BPS library
    bps_initialize();

    //Use utility code to initialize EGL for rendering with GL ES 2.0
    if (EXIT_SUCCESS != bbutil_init_egl(screen_cxt)) {
        fprintf(stderr, "bbutil_init_egl failed\n");
        bbutil_terminate();
        screen_destroy_context(screen_cxt);
        return 0;
    }

    //Initialize application logic
    if (EXIT_SUCCESS != initialize()) {
        fprintf(stderr, "initialize failed\n");
        bbutil_terminate();
        screen_destroy_context(screen_cxt);
        bps_shutdown();
        return 0;
    }

    //Signal BPS library that navigator and screen events will be requested
    if (BPS_SUCCESS != screen_request_events(screen_cxt)) {
        fprintf(stderr, "screen_request_events failed\n");
        bbutil_terminate();
        screen_destroy_context(screen_cxt);
        bps_shutdown();
        return 0;
    }

    if (BPS_SUCCESS != navigator_request_events(0)) {
        fprintf(stderr, "navigator_request_events failed\n");
        bbutil_terminate();
        screen_destroy_context(screen_cxt);
        bps_shutdown();
        return 0;
    }

    //Signal BPS library that navigator orientation is not to be locked
    if (BPS_SUCCESS != navigator_rotation_lock(false)) {
        fprintf(stderr, "navigator_rotation_lock failed\n");
        bbutil_terminate();
        screen_destroy_context(screen_cxt);
        bps_shutdown();
        return 0;
    }

    while (!exit_application) {
        //Request and process all available BPS events
        bps_event_t *event = NULL;

        for(;;) {
            rc = bps_get_event(&event, 0);
            assert(rc == BPS_SUCCESS);

            if (event) {
                int domain = bps_event_get_domain(event);

                if (domain == screen_get_domain()) {
                    handleScreenEvent(event);
                } else if ((domain == navigator_get_domain())
                        && (NAVIGATOR_EXIT == bps_event_get_code(event))) {
                    exit_application = 1;
                }
            } else {
                break;
            }
        }
        render();
    }

    //Stop requesting events from libscreen
    screen_stop_events(screen_cxt);

    //Shut down BPS library for this process
    bps_shutdown();

    //Use utility code to terminate EGL setup
    bbutil_terminate();

    //Destroy libscreen context
    screen_destroy_context(screen_cxt);
    return 0;
}
