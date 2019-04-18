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

#ifndef MMAL_UTIL_PARAMS_H
#define MMAL_UTIL_PARAMS_H

#include "interface/mmal/mmal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * Utility functions to set some common parameters.
 */

/** Helper function to set the value of a boolean parameter.
 * @param port   port on which to set the parameter
 * @param id     parameter id
 * @param value  value to set the parameter to
 *
 * @return MMAL_SUCCESS or error
 */
MMAL_STATUS_T mmal_port_parameter_set_boolean(MMAL_PORT_T *port, uint32_t id, MMAL_BOOL_T value);

/** Helper function to get the value of a boolean parameter.
 * @param port   port on which to get the parameter
 * @param id     parameter id
 * @param value  pointer to where the value will be returned
 *
 * @return MMAL_SUCCESS or error
 */
MMAL_STATUS_T mmal_port_parameter_get_boolean(MMAL_PORT_T *port, uint32_t id, MMAL_BOOL_T *value);

/** Helper function to set the value of a 64 bits unsigned integer parameter.
 * @param port   port on which to set the parameter
 * @param id     parameter id
 * @param value  value to set the parameter to
 *
 * @return MMAL_SUCCESS or error
 */
MMAL_STATUS_T mmal_port_parameter_set_uint64(MMAL_PORT_T *port, uint32_t id, uint64_t value);

/** Helper function to get the value of a 64 bits unsigned integer parameter.
 * @param port   port on which to get the parameter
 * @param id     parameter id
 * @param value  pointer to where the value will be returned
 *
 * @return MMAL_SUCCESS or error
 */
MMAL_STATUS_T mmal_port_parameter_get_uint64(MMAL_PORT_T *port, uint32_t id, uint64_t *value);

/** Helper function to set the value of a 64 bits signed integer parameter.
 * @param port   port on which to set the parameter
 * @param id     parameter id
 * @param value  value to set the parameter to
 *
 * @return MMAL_SUCCESS or error
 */
MMAL_STATUS_T mmal_port_parameter_set_int64(MMAL_PORT_T *port, uint32_t id, int64_t value);

/** Helper function to get the value of a 64 bits signed integer parameter.
 * @param port   port on which to get the parameter
 * @param id     parameter id
 * @param value  pointer to where the value will be returned
 *
 * @return MMAL_SUCCESS or error
 */
MMAL_STATUS_T mmal_port_parameter_get_int64(MMAL_PORT_T *port, uint32_t id, int64_t *value);

/** Helper function to set the value of a 32 bits unsigned integer parameter.
 * @param port   port on which to set the parameter
 * @param id     parameter id
 * @param value  value to set the parameter to
 *
 * @return MMAL_SUCCESS or error
 */
MMAL_STATUS_T mmal_port_parameter_set_uint32(MMAL_PORT_T *port, uint32_t id, uint32_t value);

/** Helper function to get the value of a 32 bits unsigned integer parameter.
 * @param port   port on which to get the parameter
 * @param id     parameter id
 * @param value  pointer to where the value will be returned
 *
 * @return MMAL_SUCCESS or error
 */
MMAL_STATUS_T mmal_port_parameter_get_uint32(MMAL_PORT_T *port, uint32_t id, uint32_t *value);

/** Helper function to set the value of a 32 bits signed integer parameter.
 * @param port   port on which to set the parameter
 * @param id     parameter id
 * @param value  value to set the parameter to
 *
 * @return MMAL_SUCCESS or error
 */
MMAL_STATUS_T mmal_port_parameter_set_int32(MMAL_PORT_T *port, uint32_t id, int32_t value);

/** Helper function to get the value of a 32 bits signed integer parameter.
 * @param port   port on which to get the parameter
 * @param id     parameter id
 * @param value  pointer to where the value will be returned
 *
 * @return MMAL_SUCCESS or error
 */
MMAL_STATUS_T mmal_port_parameter_get_int32(MMAL_PORT_T *port, uint32_t id, int32_t *value);

/** Helper function to set the value of a rational parameter.
 * @param port   port on which to set the parameter
 * @param id     parameter id
 * @param value  value to set the parameter to
 *
 * @return MMAL_SUCCESS or error
 */
MMAL_STATUS_T mmal_port_parameter_set_rational(MMAL_PORT_T *port, uint32_t id, MMAL_RATIONAL_T value);

/** Helper function to get the value of a rational parameter.
 * @param port   port on which to get the parameter
 * @param id     parameter id
 * @param value  pointer to where the value will be returned
 *
 * @return MMAL_SUCCESS or error
 */
MMAL_STATUS_T mmal_port_parameter_get_rational(MMAL_PORT_T *port, uint32_t id, MMAL_RATIONAL_T *value);

/** Helper function to set the value of a string parameter.
 * @param port   port on which to set the parameter
 * @param id     parameter id
 * @param value  null-terminated string value
 *
 * @return MMAL_SUCCESS or error
 */
MMAL_STATUS_T mmal_port_parameter_set_string(MMAL_PORT_T *port, uint32_t id, const char *value);

/** Helper function to set the value of an array of bytes parameter.
 * @param port   port on which to set the parameter
 * @param id     parameter id
 * @param data   pointer to the array of bytes
 * @param size   size of the array of bytes
 *
 * @return MMAL_SUCCESS or error
 */
MMAL_STATUS_T mmal_port_parameter_set_bytes(MMAL_PORT_T *port, uint32_t id,
   const uint8_t *data, unsigned int size);

/** Helper function to set a MMAL_PARAMETER_URI_T parameter on a port.
 * @param port   port on which to set the parameter
 * @param uri    URI string
 *
 * @return MMAL_SUCCESS or error
 */
MMAL_STATUS_T mmal_util_port_set_uri(MMAL_PORT_T *port, const char *uri);

/** Set the display region.
 * @param port   port to configure
 * @param region region
 *
 * @return MMAL_SUCCESS or error
 */
MMAL_STATUS_T mmal_util_set_display_region(MMAL_PORT_T *port,
                                           MMAL_DISPLAYREGION_T *region);

/** Tell the camera to use the STC for timestamps rather than the clock.
 *
 * @param port   port to configure
 * @param mode   STC mode to use
 * @return MMAL_SUCCESS or error
 */
MMAL_STATUS_T mmal_util_camera_use_stc_timestamp(MMAL_PORT_T *port, MMAL_CAMERA_STC_MODE_T mode);

/** Get the MMAL core statistics for a given port.
 *
 * @param port  port to query
 * @param dir   port direction
 * @param reset reset the stats as well
 * @param stats filled in with results
 * @return MMAL_SUCCESS or error
 */
MMAL_STATUS_T mmal_util_get_core_port_stats(MMAL_PORT_T *port, MMAL_CORE_STATS_DIR dir, MMAL_BOOL_T reset,
                                            MMAL_CORE_STATISTICS_T *stats);

#ifdef __cplusplus
}
#endif

#endif
