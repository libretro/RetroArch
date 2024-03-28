/*
 * Copyright © 2022 Google LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef MESA_INTERFACE_H
#define MESA_INTERFACE_H

#include "dri_interface.h"

/* Mesa-internal interface between the GLX, GBM, and EGL DRI driver loaders, and
 * the gallium dri_util.c code.
 */

typedef struct __DRImesaCoreExtensionRec __DRImesaCoreExtension;

#define __DRI_MESA "DRI_Mesa"
#define __DRI_MESA_VERSION 1

struct dri_screen;

/**  Core struct that appears alongside __DRI_CORE for Mesa-internal usage.
 * Implemented in the top-level dri/drisw/kopper extension list.
 */
struct __DRImesaCoreExtensionRec {
   __DRIextension base;

   /* Version string for verifying that the DRI driver is from the same build as
    * the loader.
    */
#define MESA_INTERFACE_VERSION_STRING PACKAGE_VERSION MESA_GIT_SHA1
   const char *version_string;

   /* Screen creation function regardless of DRI2, image, or swrast backend.
    * (Nothing uses the old __DRI_CORE screen create).
    *
    * If not associated with a DRM fd (non-swkms swrast), the fd argument should
    * be -1.
    */
   __DRIcreateNewScreen2Func createNewScreen;

   __DRIcreateContextAttribsFunc createContext;

   /* driver function for finishing initialization inside createNewScreen(). */
   const __DRIconfig **(*initScreen)(struct dri_screen *screen);

   int (*queryCompatibleRenderOnlyDeviceFd)(int kms_only_fd);
};

#endif /* MESA_INTERFACE_H */
