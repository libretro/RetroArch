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

#include "mmal_util_params.h"

/** Helper function to set the value of a boolean parameter */
MMAL_STATUS_T mmal_port_parameter_set_boolean(MMAL_PORT_T *port, uint32_t id, MMAL_BOOL_T value)
{
   MMAL_PARAMETER_BOOLEAN_T param = {{id, sizeof(param)}, value};
   return mmal_port_parameter_set(port, &param.hdr);
}

/** Helper function to get the value of a boolean parameter */
MMAL_STATUS_T mmal_port_parameter_get_boolean(MMAL_PORT_T *port, uint32_t id, MMAL_BOOL_T *value)
{
   MMAL_PARAMETER_BOOLEAN_T param = {{id, sizeof(param)}, 0};
   // coverity[overrun-buffer-val] Structure accessed correctly via size field
   MMAL_STATUS_T status = mmal_port_parameter_get(port, &param.hdr);
   if (status == MMAL_SUCCESS)
      *value = param.enable;
   return status;
}

/** Helper function to set the value of a 64 bits unsigned integer parameter */
MMAL_STATUS_T mmal_port_parameter_set_uint64(MMAL_PORT_T *port, uint32_t id, uint64_t value)
{
   MMAL_PARAMETER_UINT64_T param = {{id, sizeof(param)}, value};
   return mmal_port_parameter_set(port, &param.hdr);
}

/** Helper function to get the value of a 64 bits unsigned integer parameter */
MMAL_STATUS_T mmal_port_parameter_get_uint64(MMAL_PORT_T *port, uint32_t id, uint64_t *value)
{
   MMAL_PARAMETER_UINT64_T param = {{id, sizeof(param)}, 0LL};
   // coverity[overrun-buffer-val] Structure accessed correctly via size field
   MMAL_STATUS_T status = mmal_port_parameter_get(port, &param.hdr);
   if (status == MMAL_SUCCESS)
      *value = param.value;
   return status;
}

/** Helper function to set the value of a 64 bits signed integer parameter */
MMAL_STATUS_T mmal_port_parameter_set_int64(MMAL_PORT_T *port, uint32_t id, int64_t value)
{
   MMAL_PARAMETER_INT64_T param = {{id, sizeof(param)}, value};
   return mmal_port_parameter_set(port, &param.hdr);
}

/** Helper function to get the value of a 64 bits signed integer parameter */
MMAL_STATUS_T mmal_port_parameter_get_int64(MMAL_PORT_T *port, uint32_t id, int64_t *value)
{
   MMAL_PARAMETER_INT64_T param = {{id, sizeof(param)}, 0LL};
   // coverity[overrun-buffer-val] Structure accessed correctly via size field
   MMAL_STATUS_T status = mmal_port_parameter_get(port, &param.hdr);
   if (status == MMAL_SUCCESS)
      *value = param.value;
   return status;
}

/** Helper function to set the value of a 32 bits unsigned integer parameter */
MMAL_STATUS_T mmal_port_parameter_set_uint32(MMAL_PORT_T *port, uint32_t id, uint32_t value)
{
   MMAL_PARAMETER_UINT32_T param = {{id, sizeof(param)}, value};
   return mmal_port_parameter_set(port, &param.hdr);
}

/** Helper function to get the value of a 32 bits unsigned integer parameter */
MMAL_STATUS_T mmal_port_parameter_get_uint32(MMAL_PORT_T *port, uint32_t id, uint32_t *value)
{
   MMAL_PARAMETER_UINT32_T param = {{id, sizeof(param)}, 0};
   // coverity[overrun-buffer-val] Structure accessed correctly via size field
   MMAL_STATUS_T status = mmal_port_parameter_get(port, &param.hdr);
   if (status == MMAL_SUCCESS)
      *value = param.value;
   return status;
}

/** Helper function to set the value of a 32 bits signed integer parameter */
MMAL_STATUS_T mmal_port_parameter_set_int32(MMAL_PORT_T *port, uint32_t id, int32_t value)
{
   MMAL_PARAMETER_INT32_T param = {{id, sizeof(param)}, value};
   return mmal_port_parameter_set(port, &param.hdr);
}

/** Helper function to get the value of a 32 bits signed integer parameter */
MMAL_STATUS_T mmal_port_parameter_get_int32(MMAL_PORT_T *port, uint32_t id, int32_t *value)
{
   MMAL_PARAMETER_INT32_T param = {{id, sizeof(param)}, 0};
   // coverity[overrun-buffer-val] Structure accessed correctly via size field
   MMAL_STATUS_T status = mmal_port_parameter_get(port, &param.hdr);
   if (status == MMAL_SUCCESS)
      *value = param.value;
   return status;
}

/** Helper function to set the value of a rational parameter */
MMAL_STATUS_T mmal_port_parameter_set_rational(MMAL_PORT_T *port, uint32_t id, MMAL_RATIONAL_T value)
{
   MMAL_PARAMETER_RATIONAL_T param = {{id, sizeof(param)}, {value.num, value.den}};
   return mmal_port_parameter_set(port, &param.hdr);
}

/** Helper function to get the value of a rational parameter */
MMAL_STATUS_T mmal_port_parameter_get_rational(MMAL_PORT_T *port, uint32_t id, MMAL_RATIONAL_T *value)
{
   MMAL_PARAMETER_RATIONAL_T param = {{id, sizeof(param)}, {0,0}};
   // coverity[overrun-buffer-val] Structure accessed correctly via size field
   MMAL_STATUS_T status = mmal_port_parameter_get(port, &param.hdr);
   if (status == MMAL_SUCCESS)
      *value = param.value;
   return status;
}

/** Helper function to set the value of a string parameter */
MMAL_STATUS_T mmal_port_parameter_set_string(MMAL_PORT_T *port, uint32_t id, const char *value)
{
   MMAL_PARAMETER_STRING_T *param = 0;
   MMAL_STATUS_T status;
   size_t param_size = sizeof(param->hdr) + strlen(value) + 1;

   param = calloc(1, param_size);
   if (!param)
      return MMAL_ENOMEM;

   param->hdr.id = id;
   param->hdr.size = param_size;
   memcpy(param->str, value, strlen(value)+1);
   status = mmal_port_parameter_set(port, &param->hdr);
   free(param);
   return status;
}

/** Helper function to set a MMAL_PARAMETER_URI_T parameter on a port */
MMAL_STATUS_T mmal_util_port_set_uri(MMAL_PORT_T *port, const char *uri)
{
   return mmal_port_parameter_set_string(port, MMAL_PARAMETER_URI, uri);
}

/** Helper function to set the value of an array of bytes parameter */
MMAL_STATUS_T mmal_port_parameter_set_bytes(MMAL_PORT_T *port, uint32_t id,
   const uint8_t *data, unsigned int size)
{
   MMAL_PARAMETER_BYTES_T *param = 0;
   MMAL_STATUS_T status;
   size_t param_size = sizeof(param->hdr) + size;

   param = calloc(1, param_size);
   if (!param)
      return MMAL_ENOMEM;

   param->hdr.id = id;
   param->hdr.size = param_size;
   memcpy(param->data, data, size);
   status = mmal_port_parameter_set(port, &param->hdr);
   free(param);
   return status;
}

/** Set the display region.
 * @param port   port to configure
 * @param region region
 *
 * @return MMAL_SUCCESS or error
 */

MMAL_STATUS_T mmal_util_set_display_region(MMAL_PORT_T *port,
                                           MMAL_DISPLAYREGION_T *region)
{
   region->hdr.id = MMAL_PARAMETER_DISPLAYREGION;
   region->hdr.size = sizeof(*region);
   return mmal_port_parameter_set(port, &region->hdr);
}

MMAL_STATUS_T mmal_util_camera_use_stc_timestamp(MMAL_PORT_T *port, MMAL_CAMERA_STC_MODE_T mode)
{
   MMAL_PARAMETER_CAMERA_STC_MODE_T param =
      {{MMAL_PARAMETER_USE_STC, sizeof(MMAL_PARAMETER_CAMERA_STC_MODE_T)},mode};
   return mmal_port_parameter_set(port, &param.hdr);
}

MMAL_STATUS_T mmal_util_get_core_port_stats(MMAL_PORT_T *port,
                                            MMAL_CORE_STATS_DIR dir,
                                            MMAL_BOOL_T reset,
                                            MMAL_CORE_STATISTICS_T *stats)
{
   MMAL_PARAMETER_CORE_STATISTICS_T param;
   MMAL_STATUS_T ret;

   memset(&param, 0, sizeof(param));
   param.hdr.id = MMAL_PARAMETER_CORE_STATISTICS;
   param.hdr.size = sizeof(param);
   param.dir = dir;
   param.reset = reset;
   // coverity[overrun-buffer-val] Structure accessed correctly via size field
   ret = mmal_port_parameter_get(port, &param.hdr);
   if (ret == MMAL_SUCCESS)
      *stats = param.stats;
   return ret;
}
