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

#ifndef MMAL_PARAMETERS_CLOCK_H
#define MMAL_PARAMETERS_CLOCK_H

#include "mmal_clock.h"
#include "mmal_parameters_common.h"

/*************************************************
 * ALWAYS ADD NEW ENUMS AT THE END OF THIS LIST! *
 ************************************************/

/** Clock-specific MMAL parameter IDs.
 * @ingroup MMAL_PARAMETER_IDS
 */
enum
{
   MMAL_PARAMETER_CLOCK_REFERENCE           /**< Takes a MMAL_PARAMETER_BOOLEAN_T */
      = MMAL_PARAMETER_GROUP_CLOCK,
   MMAL_PARAMETER_CLOCK_ACTIVE,             /**< Takes a MMAL_PARAMETER_BOOLEAN_T */
   MMAL_PARAMETER_CLOCK_SCALE,              /**< Takes a MMAL_PARAMETER_RATIONAL_T */
   MMAL_PARAMETER_CLOCK_TIME,               /**< Takes a MMAL_PARAMETER_INT64_T */
   MMAL_PARAMETER_CLOCK_UPDATE_THRESHOLD,   /**< Takes a MMAL_PARAMETER_CLOCK_UPDATE_THRESHOLD_T */
   MMAL_PARAMETER_CLOCK_DISCONT_THRESHOLD,  /**< Takes a MMAL_PARAMETER_CLOCK_DISCONT_THRESHOLD_T */
   MMAL_PARAMETER_CLOCK_REQUEST_THRESHOLD,  /**< Takes a MMAL_PARAMETER_CLOCK_REQUEST_THRESHOLD_T */
   MMAL_PARAMETER_CLOCK_ENABLE_BUFFER_INFO, /**< Takes a MMAL_PARAMETER_BOOLEAN_T */
   MMAL_PARAMETER_CLOCK_FRAME_RATE,         /**< Takes a MMAL_PARAMETER_RATIONAL_T */
   MMAL_PARAMETER_CLOCK_LATENCY,            /**< Takes a MMAL_PARAMETER_CLOCK_LATENCY_T */
};

/** Media-time update thresholds */
typedef struct MMAL_PARAMETER_CLOCK_UPDATE_THRESHOLD_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_CLOCK_UPDATE_THRESHOLD_T value;
} MMAL_PARAMETER_CLOCK_UPDATE_THRESHOLD_T;

/** Media-time discontinuity settings */
typedef struct MMAL_PARAMETER_CLOCK_DISCONT_THRESHOLD_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_CLOCK_DISCONT_THRESHOLD_T value;
} MMAL_PARAMETER_CLOCK_DISCONT_THRESHOLD_T;

/** Media-time future frame drop settings */
typedef struct MMAL_PARAMETER_CLOCK_REQUEST_THRESHOLD_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_CLOCK_REQUEST_THRESHOLD_T value;
} MMAL_PARAMETER_CLOCK_REQUEST_THRESHOLD_T;

/** Clock latency parameter */
typedef struct MMAL_PARAMETER_CLOCK_LATENCY_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_CLOCK_LATENCY_T value;
} MMAL_PARAMETER_CLOCK_LATENCY_T;

#endif /* MMAL_PARAMETERS_CLOCK_H */
