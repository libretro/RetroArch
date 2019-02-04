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

#ifndef MMAL_PARAMETERS_H
#define MMAL_PARAMETERS_H

#include "mmal_common.h"
#include "mmal_parameters_camera.h"
#include "mmal_parameters_video.h"
#include "mmal_parameters_audio.h"
#include "mmal_parameters_clock.h"

/** \defgroup MmalParameters List of pre-defined parameters
 * This defines a list of standard parameters. Components can define proprietary
 * parameters by creating a new group and defining their own structures. */
/* @{ */

/** Generic unsigned 64-bit integer parameter type. */
typedef struct MMAL_PARAMETER_UINT64_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   uint64_t value; /**< Parameter value */
} MMAL_PARAMETER_UINT64_T;

/** Generic signed 64-bit integer parameter type. */
typedef struct MMAL_PARAMETER_INT64_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   int64_t value; /**< Parameter value */
} MMAL_PARAMETER_INT64_T;

/** Generic unsigned 32-bit integer parameter type. */
typedef struct MMAL_PARAMETER_UINT32_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   uint32_t value; /**< Parameter value */
} MMAL_PARAMETER_UINT32_T;

/** Generic signed 32-bit integer parameter type. */
typedef struct MMAL_PARAMETER_INT32_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   int32_t value; /**< Parameter value */
} MMAL_PARAMETER_INT32_T;

/** Generic rational parameter type. */
typedef struct MMAL_PARAMETER_RATIONAL_T {
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_RATIONAL_T value; /**< Parameter value */
} MMAL_PARAMETER_RATIONAL_T;

/** Generic boolean parameter type. */
typedef struct MMAL_PARAMETER_BOOLEAN_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_BOOL_T enable; /**< Parameter value */
} MMAL_PARAMETER_BOOLEAN_T;

/** Generic string parameter type. */
typedef struct MMAL_PARAMETER_STRING_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   char str[1];        /**< Null-terminated string */
} MMAL_PARAMETER_STRING_T;

/** Generic array of bytes parameter type. */
typedef struct MMAL_PARAMETER_BYTES_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   uint8_t data[1];   /**< Array of bytes */
} MMAL_PARAMETER_BYTES_T;

/** The value 1 in 16.16 fixed point form */
#define MMAL_FIXED_16_16_ONE  (1 << 16)

/** Generic two-dimensional scaling factor type. */
typedef struct MMAL_PARAMETER_SCALEFACTOR_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_FIXED_16_16_T scale_x;  /**< Scaling factor in X-axis */
   MMAL_FIXED_16_16_T scale_y;  /**< Scaling factor in Y-axis */
} MMAL_PARAMETER_SCALEFACTOR_T;

/** Valid mirror modes */
typedef enum MMAL_PARAM_MIRROR_T
{
   MMAL_PARAM_MIRROR_NONE,
   MMAL_PARAM_MIRROR_VERTICAL,
   MMAL_PARAM_MIRROR_HORIZONTAL,
   MMAL_PARAM_MIRROR_BOTH,
} MMAL_PARAM_MIRROR_T;

/** Generic mirror parameter type */
typedef struct MMAL_PARAMETER_MIRROR_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_PARAM_MIRROR_T value;   /**< Mirror mode */
} MMAL_PARAMETER_MIRROR_T;

/** URI parameter type.
 * The parameter may hold an arbitrary length, nul-terminated string as long
 * as the size is set appropriately.
 */
typedef struct MMAL_PARAMETER_URI_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   char uri[1];    /**< URI string (null-terminated) */
} MMAL_PARAMETER_URI_T;

/** Generic encoding parameter type.
 * The parameter may hold more than one encoding by overriding the size to
 * include a bigger array.
 */
typedef struct MMAL_PARAMETER_ENCODING_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   uint32_t encoding[1];   /**< Array of FourCC encodings, see \ref MmalEncodings */
} MMAL_PARAMETER_ENCODING_T;

/** Generic frame-rate parameter type.
 * Frame rates are specified as a rational number, using a pair of integers.
 * Since there can be many valid pairs for the same ratio, a frame-rate may
 * not contain exactly the same pairs of values when read back as it was
 * when set.
 */
typedef struct MMAL_PARAMETER_FRAME_RATE_T {
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_RATIONAL_T frame_rate;   /**< Frame-rate value */
} MMAL_PARAMETER_FRAME_RATE_T;

/** Generic configuration-file setup type.
 * Configuration files are transferred in small chunks. The component can
 * save all the chunks into a buffer, then process the entire file later.
 * This parameter initialises a config file to have the given size.
 */
typedef struct MMAL_PARAMETER_CONFIGFILE_T {
   MMAL_PARAMETER_HEADER_T hdr;

   uint32_t file_size;           /**< Size of complete file data */
} MMAL_PARAMETER_CONFIGFILE_T;

/** Generic configuration-file chunk data type.
 * Once a config file has been initialised, this parameter can be used to
 * write an arbitrary chunk of the file data (limited by the maximum MMAL
 * message size).
 */
typedef struct MMAL_PARAMETER_CONFIGFILE_CHUNK_T {
   MMAL_PARAMETER_HEADER_T hdr;

   uint32_t size;                /**< Number of bytes being transferred in this chunk */
   uint32_t offset;              /**< Offset of this chunk in the file */
   char data[1];                 /**< Chunk data */
} MMAL_PARAMETER_CONFIGFILE_CHUNK_T;

/* @} */

#endif /* MMAL_PARAMETERS_H */
