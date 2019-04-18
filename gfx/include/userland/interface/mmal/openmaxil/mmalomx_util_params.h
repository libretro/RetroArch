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

#ifndef MMALOMX_UTIL_PARAMS_H
#define MMALOMX_UTIL_PARAMS_H

#ifdef __cplusplus
extern "C" {
#endif

/** The structure that all OMX parameters containing a port start with */
typedef struct MMALOMX_PARAM_OMX_HEADER_T
{
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
} MMALOMX_PARAM_OMX_HEADER_T;

/** The structure that all OMX parameters without a port start with */
typedef struct MMALOMX_PARAM_OMX_HEADER_PORTLESS_T
{
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
} MMALOMX_PARAM_OMX_HEADER_PORTLESS_T;

typedef enum {
   MMALOMX_PARAM_MAPPING_TO_MMAL,
   MMALOMX_PARAM_MAPPING_TO_OMX
} MMALOMX_PARAM_MAPPING_DIRECTION;

typedef struct MMALOMX_PARAM_ENUM_TRANSLATE_T {
   uint32_t mmal;
   uint32_t omx;
} MMALOMX_PARAM_ENUM_TRANSLATE_T;

typedef enum MMALOMX_PARAM_TRANSLATION_TYPE_T {
   MMALOMX_PARAM_TRANSLATION_TYPE_NONE = 0,
   MMALOMX_PARAM_TRANSLATION_TYPE_SIMPLE,
   MMALOMX_PARAM_TRANSLATION_TYPE_CUSTOM,
   MMALOMX_PARAM_TRANSLATION_TYPE_DIRECT,
} MMALOMX_PARAM_TRANSLATION_TYPE_T;

/** MMAL <-> OMX parameter translation information */
typedef struct MMALOMX_PARAM_TRANSLATION_T
{
   uint32_t mmal_id;               /**< MMAL parameter id */
   uint32_t omx_id;                /**< OpenMAX IL parameter index */
   unsigned int mmal_size:16;
   unsigned int omx_size:16;
   unsigned int portless:1;
   unsigned int double_translation:1;
   MMALOMX_PARAM_TRANSLATION_TYPE_T type:4;

   struct {
      MMAL_STATUS_T (*simple)(MMALOMX_PARAM_MAPPING_DIRECTION dir,
         MMAL_PARAMETER_HEADER_T *mmal_param, OMX_PTR omx_param);
      MMAL_STATUS_T (*generic)(MMALOMX_PARAM_MAPPING_DIRECTION dir,
         const struct MMALOMX_PARAM_TRANSLATION_T *xlat,
         MMAL_PARAMETER_HEADER_T *mmal_param, OMX_PTR omx_param, MMAL_PORT_T *mmal_port);
      MMAL_STATUS_T (*list)(MMALOMX_PARAM_MAPPING_DIRECTION dir,
         const struct MMALOMX_PARAM_TRANSLATION_T *xlat, unsigned int index,
         MMAL_PARAMETER_HEADER_T *mmal_param, OMX_PTR omx_param, MMAL_PORT_T *mmal_port);
      MMAL_STATUS_T (*custom)(MMALOMX_PARAM_MAPPING_DIRECTION dir,
         const struct MMALOMX_PARAM_TRANSLATION_T *xlat,
         MMAL_PARAMETER_HEADER_T *mmal_param, OMX_PTR omx_param, MMAL_PORT_T *mmal_port);
   } fn;

   const struct MMALOMX_PARAM_ENUM_TRANSLATE_T *xlat_enum;
   unsigned int xlat_enum_num;

   const char *mmal_name; /**< MMAL parameter name */
   const char *omx_name;  /**< OMX parameter name */

} MMALOMX_PARAM_TRANSLATION_T;

const char *mmalomx_parameter_name_omx(uint32_t id);
const char *mmalomx_parameter_name_mmal(uint32_t id);
const MMALOMX_PARAM_TRANSLATION_T *mmalomx_find_parameter_from_omx_id(uint32_t id);
const MMALOMX_PARAM_TRANSLATION_T *mmalomx_find_parameter_from_mmal_id(uint32_t id);
const MMALOMX_PARAM_TRANSLATION_T *mmalomx_find_parameter_enum(unsigned int index);

#ifdef __cplusplus
}
#endif

#endif /* MMALOMX_UTIL_PARAMS_H */
