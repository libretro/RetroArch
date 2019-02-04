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
#ifndef LAUNCHER_RPI_H
#define LAUNCHER_RPI_H

#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include "applog.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Entry point function for application-specific code.
 * @param params  Application-specific parameters.
 * @param win     Native window/surface.
 * @return 0 on success; -1 on failure.
 */
typedef int (*RUN_APP_FN_T)(const void *params, EGLNativeWindowType win);

/** Create native window and runs the function.
 *
 * No need to run in a separate thread on Linux / RPI.
 *
 * @param name        Application name.
 * @param run_app_fn  Entry point function for application.
 * @param params      Application-specific parameters.
 * @param param_size  Size, in bytes, of the application parameters.
 * @return 0 on success; -1 on failure.
 */
int runApp(const char *name, RUN_APP_FN_T run_app_fn, const void *params, size_t param_size);

#ifdef __cplusplus
}
#endif

#endif
