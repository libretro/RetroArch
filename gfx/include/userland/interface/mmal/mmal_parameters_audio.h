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

#ifndef MMAL_PARAMETERS_AUDIO_H
#define MMAL_PARAMETERS_AUDIO_H

#include "mmal_parameters_common.h"

/*************************************************
 * ALWAYS ADD NEW ENUMS AT THE END OF THIS LIST! *
 ************************************************/

/** Audio-specific MMAL parameter IDs.
 * @ingroup MMAL_PARAMETER_IDS
 */
enum
{
   MMAL_PARAMETER_AUDIO_DESTINATION      /**< Takes a MMAL_PARAMETER_STRING_T */
         = MMAL_PARAMETER_GROUP_AUDIO,
   MMAL_PARAMETER_AUDIO_LATENCY_TARGET,  /**< Takes a MMAL_PARAMETER_AUDIO_LATENCY_TARGET_T */
   MMAL_PARAMETER_AUDIO_SOURCE,
   MMAL_PARAMETER_AUDIO_PASSTHROUGH,     /**< Takes a MMAL_PARAMETER_BOOLEAN_T */
};

/** Audio latency target to maintain.
 * These settings are used to adjust the clock speed in order
 * to match the measured audio latency to a specified value. */
typedef struct MMAL_PARAMETER_AUDIO_LATENCY_TARGET_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_BOOL_T enable;   /**< whether this mode is enabled */
   uint32_t filter;      /**< number of latency samples to filter on, good value: 1 */
   uint32_t target;      /**< target latency (microseconds) */
   uint32_t shift;       /**< shift for storing latency values, good value: 7 */
   int32_t speed_factor; /**< multiplier for speed changes, in 24.8 format, good value: 256-512 */
   int32_t inter_factor; /**< divider for comparing latency versus gradiant, good value: 300 */
   int32_t adj_cap;      /**< limit for speed change before nSpeedFactor is applied, good value: 100 */
} MMAL_PARAMETER_AUDIO_LATENCY_TARGET_T;

#endif /* MMAL_PARAMETERS_AUDIO_H */
