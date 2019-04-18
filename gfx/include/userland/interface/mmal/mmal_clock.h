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

#ifndef MMAL_CLOCK_H
#define MMAL_CLOCK_H

#include "interface/vcos/vcos.h"
#include "mmal_types.h"
#include "mmal_common.h"

/** \defgroup MmalClock Clock Framework
 * The MMAL clock framework provides scheduling facilities to the rest of
 * MMAL.
 *
 * The framework consists mainly of clock ports and a clock module. Client
 * applications and components interact directly with clock ports, while
 * the clock module is only used internally by clock ports.
 *
 * Clock ports ensure that the local media-time for each component is
 * synchronised across all components. This is done by passing buffers between
 * clock ports which contain clock-specific data.
 *
 * One clock port will normally act as the reference clock for the rest of the
 * system. This is usually chosen to be the clock port of the audio render
 * component, but is configurable by the client and could potentially be any
 * other clock port (or even the client application itself).
 *
 * Components that are responsible for timed delivery of frames, do so by
 * registering callback requests for a particular time-stamp with the clock
 * port. These requests are scheduled using the clock module which maintains
 * an internal media-time.
 *
 * The clock framework also provides the ability to perform playback at different
 * speeds. This is achieved with a clock scale factor which determines the speed
 * at which the media-time advances relative to real-time, with:
 *   scale = 1.0 -> normal playback speed
 *   scale = 0   -> playback paused
 *   scale > 1.0 -> fast-forward
 *   scale < 1.0 -> slow motion
 */

/** Clock event magic */
#define MMAL_CLOCK_EVENT_MAGIC               MMAL_FOURCC('C','K','L','M')

/** Clock reference update */
#define MMAL_CLOCK_EVENT_REFERENCE           MMAL_FOURCC('C','R','E','F')

/** Clock state update */
#define MMAL_CLOCK_EVENT_ACTIVE              MMAL_FOURCC('C','A','C','T')

/** Clock scale update */
#define MMAL_CLOCK_EVENT_SCALE               MMAL_FOURCC('C','S','C','A')

/** Clock media-time update */
#define MMAL_CLOCK_EVENT_TIME                MMAL_FOURCC('C','T','I','M')

/** Clock update threshold */
#define MMAL_CLOCK_EVENT_UPDATE_THRESHOLD    MMAL_FOURCC('C','U','T','H')

/** Clock discontinuity threshold */
#define MMAL_CLOCK_EVENT_DISCONT_THRESHOLD   MMAL_FOURCC('C','D','T','H')

/** Clock request threshold */
#define MMAL_CLOCK_EVENT_REQUEST_THRESHOLD   MMAL_FOURCC('C','R','T','H')

/** Buffer statistics */
#define MMAL_CLOCK_EVENT_INPUT_BUFFER_INFO   MMAL_FOURCC('C','I','B','I')
#define MMAL_CLOCK_EVENT_OUTPUT_BUFFER_INFO  MMAL_FOURCC('C','O','B','I')

/** Clock latency setting */
#define MMAL_CLOCK_EVENT_LATENCY             MMAL_FOURCC('C','L','A','T')

/** Clock event not valid */
#define MMAL_CLOCK_EVENT_INVALID   0

/** Thresholds used when updating a clock's media-time */
typedef struct MMAL_CLOCK_UPDATE_THRESHOLD_T
{
   /** Time differences below this threshold are ignored (microseconds) */
   int64_t threshold_lower;

   /** Time differences above this threshold reset media-time (microseconds) */
   int64_t threshold_upper;
} MMAL_CLOCK_UPDATE_THRESHOLD_T;

/** Threshold for detecting a discontinuity in media-time */
typedef struct MMAL_CLOCK_DISCONT_THRESHOLD_T
{
   /** Threshold after which backward jumps in media-time are treated as a
    * discontinuity (microseconds) */
   int64_t threshold;

   /** Duration in microseconds for which a discontinuity applies (wall-time) */
   int64_t duration;
} MMAL_CLOCK_DISCONT_THRESHOLD_T;

/** Threshold applied to client callback requests */
typedef struct MMAL_CLOCK_REQUEST_THRESHOLD_T
{
   /** Frames with a media-time difference (compared to current media-time)
    * above this threshold are dropped (microseconds) */
   int64_t threshold;

   /** Enable/disable the request threshold */
   MMAL_BOOL_T threshold_enable;
} MMAL_CLOCK_REQUEST_THRESHOLD_T;

/** Structure for passing buffer information to a clock port */
typedef struct MMAL_CLOCK_BUFFER_INFO_T
{
   int64_t time_stamp;
   uint32_t arrival_time;
} MMAL_CLOCK_BUFFER_INFO_T;

/** Clock latency settings used by the clock component */
typedef struct MMAL_CLOCK_LATENCY_T
{
   int64_t target;            /**< target latency (microseconds) */
   int64_t attack_period;     /**< duration of one attack period (microseconds) */
   int64_t attack_rate;       /**< amount by which media-time will be adjusted
                                   every attack_period (microseconds) */
} MMAL_CLOCK_LATENCY_T;

/** Clock event used to pass data between clock ports and a client. */
typedef struct MMAL_CLOCK_EVENT_T
{
   /** 4cc event id */
   uint32_t id;

   /** 4cc event magic */
   uint32_t magic;

   /** buffer associated with this event (can be NULL) */
   struct MMAL_BUFFER_HEADER_T *buffer;

   /** pad to 64-bit boundary */
   uint32_t padding0;

   /** additional event data (type-specific) */
   union
   {
      /** used either for clock reference or clock state */
      MMAL_BOOL_T enable;

      /** new clock scale */
      MMAL_RATIONAL_T scale;

      /** new media-time */
      int64_t media_time;

      /** media-time update threshold */
      MMAL_CLOCK_UPDATE_THRESHOLD_T update_threshold;

      /** media-time discontinuity threshold */
      MMAL_CLOCK_DISCONT_THRESHOLD_T discont_threshold;

      /** client callback request threshold */
      MMAL_CLOCK_REQUEST_THRESHOLD_T request_threshold;

      /** input/output buffer information */
      MMAL_CLOCK_BUFFER_INFO_T buffer;

      /** clock latency setting */
      MMAL_CLOCK_LATENCY_T latency;
   } data;

   /** pad to 64-bit boundary */
   uint64_t padding1;
} MMAL_CLOCK_EVENT_T;

/* Make sure MMAL_CLOCK_EVENT_T will preserve 64-bit alignment */
vcos_static_assert(!(sizeof(MMAL_CLOCK_EVENT_T) & 0x7));

#define MMAL_CLOCK_EVENT_INIT(id) { id, MMAL_CLOCK_EVENT_MAGIC, NULL, 0, {0}, 0 }

#endif /* MMAL_CLOCK_H */
