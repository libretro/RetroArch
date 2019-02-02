/* $Revision: 6810 $ on $Date:: 2008-10-29 14:31:37 +0000 #$ */

/*------------------------------------------------------------------------
 *
 * VG platform specific header Reference Implementation
 * ----------------------------------------------------
 *
 * Copyright (c) 2008 The Khronos Group Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and /or associated documentation files
 * (the "Materials "), to deal in the Materials without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Materials,
 * and to permit persons to whom the Materials are furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE MATERIALS OR
 * THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//**
 * \file
 * \brief VG platform specific header
 *//*-------------------------------------------------------------------*/

#ifndef _VGPLATFORM_H
#define _VGPLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef VG_API_CALL
   #ifdef KHAPI
      #define VG_API_CALL KHAPI
   #else
      #define VG_API_CALL extern
   #endif
#endif

#ifndef VGU_API_CALL
   #define VGU_API_CALL VG_API_CALL
#endif

#ifndef VG_API_ENTRY
#define VG_API_ENTRY
#endif

#ifndef VG_API_EXIT
#define VG_API_EXIT
#endif

#ifndef VGU_API_ENTRY
#define VGU_API_ENTRY
#endif

#ifndef VGU_API_EXIT
#define VGU_API_EXIT
#endif

#include "../KHR/khrplatform.h"
typedef khronos_float_t  VGfloat;
typedef khronos_int8_t   VGbyte;
typedef khronos_uint8_t  VGubyte;
typedef khronos_int16_t  VGshort;
typedef khronos_int32_t  VGint;
typedef khronos_uint32_t VGuint;
typedef khronos_uint32_t VGbitfield;

#ifndef VG_VGEXT_PROTOTYPES
#define VG_VGEXT_PROTOTYPES
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _VGPLATFORM_H */
