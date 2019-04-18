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
 * Support for setting/getting parameters as string values.
 */

#ifndef MMAL_PARAM_CONVERT_H
#define MMAL_PARAM_CONVERT_H

#include "interface/mmal/mmal.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Parse a video size. e.g. "1080p" gives 1920x1080.
 *
 * @param w width result
 * @param h height result
 * @param str string to convert
 * @return MMAL_SUCCESS or error code
 */
MMAL_STATUS_T mmal_parse_video_size(uint32_t *w, uint32_t *h, const char *str);

/** Parse a rational number. e.g. "30000/1001", "30", etc.
 * @param dest filled in with result
 * @param str string to convert
 * @return MMAL_SUCCESS or error code
 */
MMAL_STATUS_T mmal_parse_rational(MMAL_RATIONAL_T *dest, const char *str);

/** Parse an integer, e.g. -10, 0x1A, etc.
 * @param dest filled in with result
 * @param str string to convert
 * @return MMAL_SUCCESS or error code
 */
MMAL_STATUS_T mmal_parse_int(int *dest, const char *str);

/** Parse an unsigned integer, e.g. 10, 0x1A, etc.
 * @param dest filled in with result
 * @param str string to convert
 * @return MMAL_SUCCESS or error code
 */
MMAL_STATUS_T mmal_parse_uint(unsigned int *dest, const char *str);

/** Parse a geometry for a rectangle
 *
 * e.g. 100*100+50+75
 * or   200*150
 * @param dest filled in with result
 * @param str string to convert
 * @return MMAL_SUCCESS or error code
 */
MMAL_STATUS_T mmal_parse_geometry(MMAL_RECT_T *dest, const char *str);

/** Parse a video codec name (something that can be encoded/decoded)
 * @param str string to convert
 * @param dest filled in with result
 * @return MMAL_SUCCESS or error code
 */
MMAL_STATUS_T mmal_parse_video_codec(uint32_t *dest, const char *str);

#ifdef __cplusplus
}
#endif

#endif
