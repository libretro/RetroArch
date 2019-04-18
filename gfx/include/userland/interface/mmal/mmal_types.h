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

#ifndef MMAL_TYPES_H
#define MMAL_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

/** \defgroup MmalTypes Common types
 * Definition for common types */
/* @{ */

#include "mmal_common.h"

/** Status return codes from the API.
 *
 * \internal Please try to keep this similar to the standard POSIX codes
 * rather than making up new ones!
 */
typedef enum
{
   MMAL_SUCCESS = 0,                 /**< Success */
   MMAL_ENOMEM,                      /**< Out of memory */
   MMAL_ENOSPC,                      /**< Out of resources (other than memory) */
   MMAL_EINVAL,                      /**< Argument is invalid */
   MMAL_ENOSYS,                      /**< Function not implemented */
   MMAL_ENOENT,                      /**< No such file or directory */
   MMAL_ENXIO,                       /**< No such device or address */
   MMAL_EIO,                         /**< I/O error */
   MMAL_ESPIPE,                      /**< Illegal seek */
   MMAL_ECORRUPT,                    /**< Data is corrupt \attention FIXME: not POSIX */
   MMAL_ENOTREADY,                   /**< Component is not ready \attention FIXME: not POSIX */
   MMAL_ECONFIG,                     /**< Component is not configured \attention FIXME: not POSIX */
   MMAL_EISCONN,                     /**< Port is already connected */
   MMAL_ENOTCONN,                    /**< Port is disconnected */
   MMAL_EAGAIN,                      /**< Resource temporarily unavailable. Try again later*/
   MMAL_EFAULT,                      /**< Bad address */
   /* Do not add new codes here unless they match something from POSIX */
   MMAL_STATUS_MAX = 0x7FFFFFFF      /**< Force to 32 bit */
} MMAL_STATUS_T;

/** Describes a rectangle */
typedef struct
{
   int32_t x;      /**< x coordinate (from left) */
   int32_t y;      /**< y coordinate (from top) */
   int32_t width;  /**< width */
   int32_t height; /**< height */
} MMAL_RECT_T;

/** Describes a rational number */
typedef struct
{
   int32_t num;    /**< Numerator */
   int32_t den;    /**< Denominator */
} MMAL_RATIONAL_T;

/** \name Special Unknown Time Value
 * Timestamps in MMAL are defined as signed 64 bits integer values representing microseconds.
 * However a pre-defined special value is used to signal that a timestamp is not known. */
/* @{ */
#define MMAL_TIME_UNKNOWN (INT64_C(1)<<63)  /**< Special value signalling that time is not known */
/* @} */

/** Four Character Code type */
typedef uint32_t MMAL_FOURCC_T;

/* @} */

#ifdef __cplusplus
}
#endif

#endif /* MMAL_TYPES_H */
