#ifndef __eglplatform_h_
#define __eglplatform_h_

/*
** Copyright (c) 2007-2009 The Khronos Group Inc.
**
** Permission is hereby granted, free of charge, to any person obtaining a
** copy of this software and/or associated documentation files (the
** "Materials"), to deal in the Materials without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Materials, and to
** permit persons to whom the Materials are furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be included
** in all copies or substantial portions of the Materials.
**
** THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
*/

/* Platform-specific types and definitions for egl.h
 * $Revision: 12306 $ on $Date: 2010-08-25 09:51:28 -0700 (Wed, 25 Aug 2010) $
 *
 * Adopters may modify khrplatform.h and this file to suit their platform.
 * You are encouraged to submit all modifications to the Khronos group so that
 * they can be included in future versions of this file.  Please submit changes
 * by sending them to the public Khronos Bugzilla (http://khronos.org/bugzilla)
 * by filing a bug against product "EGL" component "Registry".
 */

#include "../KHR/khrplatform.h"

#ifdef ABSTRACT_PLATFORM
#include "begl_memplatform.h"
#include "begl_hwplatform.h"
#include "begl_dispplatform.h"
#endif /* ABSTRACT_PLATFORM */

/* Macros used in EGL function prototype declarations.
 *
 * EGL functions should be prototyped as:
 *
 * EGLAPI return-type EGLAPIENTRY eglFunction(arguments);
 * typedef return-type (EXPAPIENTRYP PFNEGLFUNCTIONPROC) (arguments);
 *
 * KHRONOS_APICALL and KHRONOS_APIENTRY are defined in KHR/khrplatform.h
 */

#ifndef EGLAPI
#define EGLAPI KHRONOS_APICALL
#endif

#ifndef EGLAPIENTRY
#define EGLAPIENTRY  KHRONOS_APIENTRY
#endif
#ifndef EGLAPIENTRYP
#define EGLAPIENTRYP EGLAPIENTRY *
#endif
#ifndef EGLAPI
#ifdef KHAPI
#define EGLAPI KHAPI
#else
#define EGLAPI extern
#endif
#endif

/* The types NativeDisplayType, NativeWindowType, and NativePixmapType
 * are aliases of window-system-dependent types, such as X Display * or
 * Windows Device Context. They must be defined in platform-specific
 * code below. The EGL-prefixed versions of Native*Type are the same
 * types, renamed in EGL 1.3 so all types in the API start with "EGL".
 *
 * Khronos STRONGLY RECOMMENDS that you use the default definitions
 * provided below, since these changes affect both binary and source
 * portability of applications using EGL running on different EGL
 * implementations.
 */

/* Unix (tentative)
    #include <X headers>
    typedef Display *NativeDisplayType;
      - or maybe, if encoding "hostname:display.head"
    typedef const char *NativeWindowType;
        etc.
 */
#if (defined (__ANDROID__) || defined(ANDROID)) && defined(KHRN_BCG_ANDROID)

struct android_native_window_t;
struct egl_native_pixmap_t;

typedef struct android_native_window_t* EGLNativeWindowType;
typedef struct egl_native_pixmap_t*     EGLNativePixmapType;
typedef void *EGLNativeDisplayType;

#else

typedef void *EGLNativeDisplayType;
typedef void *EGLNativePixmapType;
typedef void *EGLNativeWindowType;
#endif

#ifndef EGL_SERVER_SMALLINT

#include "interface/vmcs_host/vc_dispmanx.h"
/* TODO: EGLNativeWindowType is really one of these but I'm leaving it
 * as void* for now, in case changing it would cause problems
 */
typedef struct {
   DISPMANX_ELEMENT_HANDLE_T element;
   int width;   /* This is necessary because dispmanx elements are not queriable. */
   int height;
} EGL_DISPMANX_WINDOW_T;
#elif defined (ABSTRACT_PLATFORM)

#else

/* window I of a horizontal strip of N WxH windows */
#define PACK_NATIVE_WINDOW(W, H, I, N) ((NativeWindowType)((W) | ((H) << 12) | ((I) << 24) | ((N) << 28)))
#define UNPACK_NATIVE_WINDOW_W(WIN) ((unsigned int)(WIN) & 0xfff)
#define UNPACK_NATIVE_WINDOW_H(WIN) (((unsigned int)(WIN) >> 12) & 0xfff)
#define UNPACK_NATIVE_WINDOW_I(WIN) (((unsigned int)(WIN) >> 24) & 0xf)
#define UNPACK_NATIVE_WINDOW_N(WIN) ((unsigned int)(WIN) >> 28)

/* todo: can we change these to use PACK_NATIVE_WINDOW and get rid of platform_canonical_win from platform.h? */
#define NATIVE_WINDOW_800_480    ((NativeWindowType)0)
#define NATIVE_WINDOW_640_480    ((NativeWindowType)1)
#define NATIVE_WINDOW_320_240    ((NativeWindowType)2)
#define NATIVE_WINDOW_240_320    ((NativeWindowType)3)
#define NATIVE_WINDOW_64_64      ((NativeWindowType)4)
#define NATIVE_WINDOW_400_480_A  ((NativeWindowType)5)
#define NATIVE_WINDOW_400_480_B  ((NativeWindowType)6)
#define NATIVE_WINDOW_512_512    ((NativeWindowType)7)
#define NATIVE_WINDOW_360_640    ((NativeWindowType)8)
#define NATIVE_WINDOW_640_360    ((NativeWindowType)9)
#define NATIVE_WINDOW_1280_720   ((NativeWindowType)10)
#define NATIVE_WINDOW_1920_1080  ((NativeWindowType)11)
#define NATIVE_WINDOW_480_320    ((NativeWindowType)12)
#define NATIVE_WINDOW_1680_1050  ((NativeWindowType)13)
#endif

/* EGL 1.2 types, renamed for consistency in EGL 1.3 */
typedef EGLNativeDisplayType NativeDisplayType;
typedef EGLNativePixmapType  NativePixmapType;
typedef EGLNativeWindowType  NativeWindowType;


/* Define EGLint. This must be a signed integral type large enough to contain
 * all legal attribute names and values passed into and out of EGL, whether
 * their type is boolean, bitmask, enumerant (symbolic constant), integer,
 * handle, or other.  While in general a 32-bit integer will suffice, if
 * handles are 64 bit types, then EGLint should be defined as a signed 64-bit
 * integer type.
 */
typedef khronos_int32_t EGLint;

#ifdef ABSTRACT_PLATFORM

#ifdef __cplusplus
extern "C"
{
#endif

/*
The client application, or default platform library must register valid versions of each of these
interfaces before any EGL or GL functions are invoked, using the following functions provided by the 3D driver.
*/
typedef struct
{
   BEGL_MemoryInterface  *memInterface;     /* Memory interface which will called by the 3d driver */
   BEGL_HWInterface      *hwInterface;      /* Hardware interface which will be called by the driver */
   BEGL_DisplayInterface *displayInterface; /* Display interface which will be called by the driver */

   BEGL_DisplayCallbacks displayCallbacks; /* Callback pointers set by BEGL_GetDefaultDriverInterfaces, for client to call into driver */
   int hwInterfaceCloned;
   int memInterfaceCloned;
   void *memInterfaceFn;
   void *hwInterfaceFn;
} BEGL_DriverInterfaces;

/* Register application level overrides for any or all of the abstract API calls made by the 3D driver. */
EGLAPI void EGLAPIENTRY BEGL_RegisterDriverInterfaces(BEGL_DriverInterfaces *iface);

/* Get a pointer to the registered driver interfaces, can be used to override partial defaults - see android platform layer(s) for example */
EGLAPI BEGL_DriverInterfaces * BEGL_GetDriverInterfaces(void);

/* Initializes all interfaces in the structure to NULL, fills out Callbacks with appropriate function pointers */
EGLAPI void EGLAPIENTRY BEGL_GetDefaultDriverInterfaces(BEGL_DriverInterfaces *iface);

#ifdef __cplusplus
}
#endif

#endif /* ABSTRACT_PLATFORM */

#if 0
#include "interface/khronos/common/khrn_client_mangle.h"
#endif

#endif /* __eglplatform_h */
