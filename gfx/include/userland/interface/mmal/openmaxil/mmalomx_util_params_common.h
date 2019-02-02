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

/** \file
 * OpenMAX IL adaptation layer for MMAL - Parameters related functions
 */

#include "interface/vmcs_host/khronos/IL/OMX_Broadcom.h"
#include "mmalomx_util_params.h"
#include "util/mmal_util_rational.h"

/* Sanity check that OMX is defining the right int32 types */
vcos_static_assert(sizeof(OMX_U32) == 4);

MMAL_STATUS_T mmalomx_param_enum_generic(MMALOMX_PARAM_MAPPING_DIRECTION dir,
   const struct MMALOMX_PARAM_TRANSLATION_T *xlat,
   MMAL_PARAMETER_HEADER_T *mmal, OMX_PTR omx, MMAL_PORT_T *mmal_port);
MMAL_STATUS_T mmalomx_param_rational_generic(MMALOMX_PARAM_MAPPING_DIRECTION dir,
   const struct MMALOMX_PARAM_TRANSLATION_T *xlat,
   MMAL_PARAMETER_HEADER_T *mmal, OMX_PTR omx, MMAL_PORT_T *mmal_port);
MMAL_STATUS_T mmalomx_param_mapping_generic(MMALOMX_PARAM_MAPPING_DIRECTION dir,
   const struct MMALOMX_PARAM_TRANSLATION_T *xlat,
   MMAL_PARAMETER_HEADER_T *mmal, OMX_PTR omx, MMAL_PORT_T *mmal_port);

extern const MMALOMX_PARAM_TRANSLATION_T mmalomx_param_xlator_audio[];
extern const MMALOMX_PARAM_TRANSLATION_T mmalomx_param_xlator_video[];
extern const MMALOMX_PARAM_TRANSLATION_T mmalomx_param_xlator_camera[];
extern const MMALOMX_PARAM_TRANSLATION_T mmalomx_param_xlator_misc[];

#define MMALOMX_PARAM_ENUM_FIND(TYPE, VAR, TABLE, DIR, MMAL, OMX)  \
   const TYPE *VAR = TABLE; \
   const TYPE *VAR##_end = VAR + MMAL_COUNTOF(TABLE); \
   if (DIR == MMALOMX_PARAM_MAPPING_TO_MMAL) \
      while (VAR < VAR##_end && VAR->omx != OMX) VAR++; \
   else \
      while (VAR < VAR##_end && VAR->mmal != MMAL) VAR++; \
   do { if (VAR == VAR##_end) { \
      VAR = 0; \
      if (DIR == MMALOMX_PARAM_MAPPING_TO_MMAL) \
         VCOS_ALERT("omx enum value %u not supported", (unsigned int)OMX); \
      else \
         VCOS_ALERT("mmal enum value %u not supported", (unsigned int)MMAL); \
   } } while(0)

#define mmalomx_ct_assert(e) (sizeof(char[1 - 2*!(e)]))

/** List of macros used to define parameters mapping */
#define MMALOMX_PARAM_PASSTHROUGH(a,b,c,d) \
   {a, (uint32_t)c, sizeof(b), sizeof(d), \
   !(offsetof(d, nPortIndex) | mmalomx_ct_assert(sizeof(b)+4==sizeof(d))), \
   0, MMALOMX_PARAM_TRANSLATION_TYPE_CUSTOM, {0, mmalomx_param_mapping_generic, 0, 0}, 0, 0, \
   MMAL_TO_STRING(a), MMAL_TO_STRING(c)}
#define MMALOMX_PARAM_BOOLEAN(a, b) \
   MMALOMX_PARAM_PASSTHROUGH(a, MMAL_PARAMETER_BOOLEAN_T, b, OMX_CONFIG_PORTBOOLEANTYPE)
#define MMALOMX_PARAM_PASSTHROUGH_PORTLESS(a,b,c,d) \
   {a, (uint32_t)c, sizeof(b), sizeof(d), \
   !!(mmalomx_ct_assert(sizeof(b)==sizeof(d))), \
   0, MMALOMX_PARAM_TRANSLATION_TYPE_CUSTOM, {0, mmalomx_param_mapping_generic, 0, 0}, 0, 0, \
   MMAL_TO_STRING(a), MMAL_TO_STRING(c)}
#define MMALOMX_PARAM_BOOLEAN_PORTLESS(a, b) \
   MMALOMX_PARAM_PASSTHROUGH_PORTLESS(a, MMAL_PARAMETER_BOOLEAN_T, b, OMX_CONFIG_BOOLEANTYPE)
#define MMALOMX_PARAM_PASSTHROUGH_PORTLESS_DOUBLE_TRANSLATION(a,b,c,d) \
   {a, (uint32_t)c, sizeof(b), sizeof(d), \
   !!(mmalomx_ct_assert(sizeof(b)==sizeof(d))), \
   1, MMALOMX_PARAM_TRANSLATION_TYPE_CUSTOM, {0, mmalomx_param_mapping_generic, 0, 0}, 0, 0, \
   MMAL_TO_STRING(a), MMAL_TO_STRING(c)}
#define MMALOMX_PARAM_DIRECT_PORTLESS(a,b,c,d) \
   {a, (uint32_t)c, sizeof(b), sizeof(d), 1, \
   0, MMALOMX_PARAM_TRANSLATION_TYPE_DIRECT, {0, 0, 0, 0}, 0, 0, \
   MMAL_TO_STRING(a), MMAL_TO_STRING(c)}
#define MMALOMX_PARAM_STRAIGHT_MAPPING(a,b,c,d,e) \
   {a, (uint32_t)c, sizeof(b), sizeof(d), \
   !offsetof(d, nPortIndex), \
   0, MMALOMX_PARAM_TRANSLATION_TYPE_SIMPLE, {e, 0, 0, 0}, 0, 0, \
   MMAL_TO_STRING(a), MMAL_TO_STRING(c)}
#define MMALOMX_PARAM_STRAIGHT_MAPPING_PORTLESS(a,b,c,d,e) \
   {a, (uint32_t)c, sizeof(b), sizeof(d), 1, \
   0, MMALOMX_PARAM_TRANSLATION_TYPE_SIMPLE, {e, 0, 0, 0}, 0, 0, \
   MMAL_TO_STRING(a), MMAL_TO_STRING(c)}
#define MMALOMX_PARAM_STRAIGHT_MAPPING_DOUBLE_TRANSLATION(a,b,c,d,e) \
   {a, (uint32_t)c, sizeof(b), sizeof(d), 0, \
   1, MMALOMX_PARAM_TRANSLATION_TYPE_SIMPLE, {e, 0, 0, 0}, 0, 0, \
   MMAL_TO_STRING(a), MMAL_TO_STRING(c)}
#define MMALOMX_PARAM_ENUM(a,b,c,d,e) \
   {a, (uint32_t)c, sizeof(b), sizeof(d), 0, \
   0, MMALOMX_PARAM_TRANSLATION_TYPE_CUSTOM, {0, mmalomx_param_enum_generic, 0, 0}, e, MMAL_COUNTOF(e), \
   MMAL_TO_STRING(a), MMAL_TO_STRING(c)}
#define MMALOMX_PARAM_ENUM_PORTLESS(a,b,c,d,e) \
   {a, (uint32_t)c, sizeof(b), sizeof(d), 1, \
   0, MMALOMX_PARAM_TRANSLATION_TYPE_CUSTOM, {0, mmalomx_param_enum_generic, 0, 0}, e, MMAL_COUNTOF(e), \
   MMAL_TO_STRING(a), MMAL_TO_STRING(c)}
#define MMALOMX_PARAM_RATIONAL(a,b,c,d,e) \
   {a, (uint32_t)c, sizeof(b), sizeof(d), 0, \
   0, MMALOMX_PARAM_TRANSLATION_TYPE_CUSTOM, {0, mmalomx_param_rational_generic, 0, 0}, 0, e, \
   MMAL_TO_STRING(a), MMAL_TO_STRING(c)}
#define MMALOMX_PARAM_RATIONAL_PORTLESS(a,b,c,d,e) \
   {a, (uint32_t)c, sizeof(b), sizeof(d), 1, \
   0, MMALOMX_PARAM_TRANSLATION_TYPE_CUSTOM, {0, mmalomx_param_rational_generic, 0, 0}, 0, e, \
   MMAL_TO_STRING(a), MMAL_TO_STRING(c)}
#define MMALOMX_PARAM_CUSTOM(a,b,c,d,e) \
   {a, (uint32_t)c, sizeof(b), sizeof(d), 0, \
   0, MMALOMX_PARAM_TRANSLATION_TYPE_CUSTOM, {0, e, 0, 0}, 0, 0, \
   MMAL_TO_STRING(a), MMAL_TO_STRING(c)}
#define MMALOMX_PARAM_LIST(a,b,c,d,e,f) \
   {a, (uint32_t)c, sizeof(b), sizeof(d), 0, \
   0, MMALOMX_PARAM_TRANSLATION_TYPE_CUSTOM, {0, 0, f, 0}, 0, offsetof(d,e), \
   MMAL_TO_STRING(a), MMAL_TO_STRING(c)}
#define MMALOMX_PARAM_TERMINATE() \
   {MMAL_PARAMETER_UNUSED, 0, 0, 0, 0, 0, 0, {0, 0, 0, 0}, 0, 0, 0, 0}
