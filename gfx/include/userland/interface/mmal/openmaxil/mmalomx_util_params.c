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

#include "interface/vmcs_host/khronos/IL/OMX_Broadcom.h"
#include "mmalomx.h"
#include "mmalomx_util_params.h"
#include "mmalomx_util_params_common.h"

static const MMALOMX_PARAM_TRANSLATION_T *mmalomx_param_list[] = {
   mmalomx_param_xlator_audio, mmalomx_param_xlator_video,
   mmalomx_param_xlator_camera, mmalomx_param_xlator_misc};

const MMALOMX_PARAM_TRANSLATION_T *mmalomx_find_parameter_enum(unsigned int index)
{
   unsigned int i, j;

   for (i = 0; i < MMAL_COUNTOF(mmalomx_param_list); i++)
   {
      for (j = 0; mmalomx_param_list[i][j].mmal_id != MMAL_PARAMETER_UNUSED; j++)
      {
         if (!index--)
            break;
      }
      if (mmalomx_param_list[i][j].mmal_id != MMAL_PARAMETER_UNUSED)
         break;
   }

   return i < MMAL_COUNTOF(mmalomx_param_list) ? &mmalomx_param_list[i][j] : NULL;
}

const MMALOMX_PARAM_TRANSLATION_T *mmalomx_find_parameter_from_omx_id(uint32_t id)
{
   unsigned int i, j;

   for (i = 0; i < MMAL_COUNTOF(mmalomx_param_list); i++)
   {
      for (j = 0; mmalomx_param_list[i][j].mmal_id != MMAL_PARAMETER_UNUSED; j++)
      {
         if (mmalomx_param_list[i][j].omx_id == id)
            break;
      }
      if (mmalomx_param_list[i][j].mmal_id != MMAL_PARAMETER_UNUSED)
         break;
   }

   return i < MMAL_COUNTOF(mmalomx_param_list) ? &mmalomx_param_list[i][j] : NULL;
}

const MMALOMX_PARAM_TRANSLATION_T *mmalomx_find_parameter_from_mmal_id(uint32_t id)
{
   unsigned int i, j;

   for (i = 0; i < MMAL_COUNTOF(mmalomx_param_list); i++)
   {
      for (j = 0; mmalomx_param_list[i][j].mmal_id != MMAL_PARAMETER_UNUSED; j++)
      {
         if (mmalomx_param_list[i][j].mmal_id == id)
            break;
      }
      if (mmalomx_param_list[i][j].mmal_id != MMAL_PARAMETER_UNUSED)
         break;
   }

   return i < MMAL_COUNTOF(mmalomx_param_list) ? &mmalomx_param_list[i][j] : NULL;
}

const char *mmalomx_parameter_name_omx(uint32_t id)
{
   const MMALOMX_PARAM_TRANSLATION_T *xlat = mmalomx_find_parameter_from_omx_id(id);
   return xlat ? xlat->omx_name : 0;
}

const char *mmalomx_parameter_name_mmal(uint32_t id)
{
   const MMALOMX_PARAM_TRANSLATION_T *xlat = mmalomx_find_parameter_from_mmal_id(id);
   return xlat ? xlat->mmal_name : 0;
}

MMAL_STATUS_T mmalomx_param_mapping_generic(MMALOMX_PARAM_MAPPING_DIRECTION dir,
   const MMALOMX_PARAM_TRANSLATION_T *xlat,
   MMAL_PARAMETER_HEADER_T *mmal_param, OMX_PTR omx_param, MMAL_PORT_T *mmal_port)
{
   MMALOMX_PARAM_OMX_HEADER_T *omx_header = (MMALOMX_PARAM_OMX_HEADER_T *)omx_param;
   uint8_t *mmal_data = ((uint8_t *)mmal_param) + sizeof(MMAL_PARAMETER_HEADER_T);
   uint8_t *omx_data = ((uint8_t *)omx_param) + sizeof(MMALOMX_PARAM_OMX_HEADER_T);
   unsigned int size = mmal_param->size - sizeof(MMAL_PARAMETER_HEADER_T);
   MMAL_PARAM_UNUSED(mmal_port);

   if (xlat->portless)
      omx_data -= sizeof(OMX_U32);

   if (((uint8_t *)omx_param) + omx_header->nSize !=
         omx_data + size)
   {
      VCOS_ALERT("mmalomx_param_mapping_generic: mismatch between mmal and omx parameters for (%u)",
                 (unsigned int)mmal_param->id);
      return MMAL_EINVAL;
   }

   if (dir == MMALOMX_PARAM_MAPPING_TO_MMAL)
      memcpy(mmal_data, omx_data, size);
   else
      memcpy(omx_data, mmal_data, size);

   return MMAL_SUCCESS;
}

MMAL_STATUS_T mmalomx_param_enum_generic(MMALOMX_PARAM_MAPPING_DIRECTION dir,
   const MMALOMX_PARAM_TRANSLATION_T *xlat,
   MMAL_PARAMETER_HEADER_T *mmal_param, OMX_PTR omx_param, MMAL_PORT_T *mmal_port)
{
   uint32_t *mmal = (uint32_t *)(((uint8_t *)mmal_param) + sizeof(MMAL_PARAMETER_HEADER_T));
   uint32_t *omx = (uint32_t *)(((uint8_t *)omx_param) + sizeof(MMALOMX_PARAM_OMX_HEADER_T));
   unsigned int i = 0;
   MMAL_PARAM_UNUSED(mmal_port);

   if (xlat->portless)
      omx -= 1;

   /* Find translation entry in lookup table */
   if (dir == MMALOMX_PARAM_MAPPING_TO_MMAL)
      for (i = 0; i < xlat->xlat_enum_num && xlat->xlat_enum->omx != *omx; i++);
   else
      for (i = 0; i < xlat->xlat_enum_num && xlat->xlat_enum->mmal != *mmal; i++);

   if (i == xlat->xlat_enum_num)
   {
      if (dir == MMALOMX_PARAM_MAPPING_TO_MMAL)
         VCOS_ALERT("mmalomx_param_enum_generic: omx enum value %u not supported", (unsigned int)*omx);
      else
         VCOS_ALERT("mmalomx_param_enum_generic: mmal enum value %u not supported", (unsigned int)*mmal);
      return MMAL_EINVAL;
   }

   if (dir == MMALOMX_PARAM_MAPPING_TO_MMAL)
      *mmal = xlat->xlat_enum[i].mmal;
   else
      *omx = xlat->xlat_enum[i].omx;

   return MMAL_SUCCESS;
}

MMAL_STATUS_T mmalomx_param_rational_generic(MMALOMX_PARAM_MAPPING_DIRECTION dir,
   const MMALOMX_PARAM_TRANSLATION_T *xlat,
   MMAL_PARAMETER_HEADER_T *mmal_param, OMX_PTR omx_param,  MMAL_PORT_T *mmal_port)
{
   MMAL_RATIONAL_T *mmal = (MMAL_RATIONAL_T *)(((uint8_t *)mmal_param) + sizeof(MMAL_PARAMETER_HEADER_T));
   int32_t *omx = (int32_t *)(((uint8_t *)omx_param) + sizeof(MMALOMX_PARAM_OMX_HEADER_T));
   MMAL_PARAM_UNUSED(mmal_port);

   if (xlat->portless)
      omx -= 1;

   if (dir == MMALOMX_PARAM_MAPPING_TO_MMAL)
   {
      mmal->num = *omx;
      mmal->den = xlat->xlat_enum_num;
      mmal_rational_simplify(mmal);
   }
   else
   {
      mmal_rational_simplify(mmal);
      *omx = 0;
      if (mmal->den)
         *omx = mmal->num * xlat->xlat_enum_num / mmal->den;
   }

   return MMAL_SUCCESS;
}
