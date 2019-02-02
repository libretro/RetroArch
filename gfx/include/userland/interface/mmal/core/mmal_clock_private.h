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

#ifndef MMAL_CLOCK_PRIVATE_H
#define MMAL_CLOCK_PRIVATE_H

#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_clock.h"

#ifdef __cplusplus
extern "C" {
#endif


/** Handle to a clock. */
typedef struct MMAL_CLOCK_T
{
   void *user_data;   /**< Client-supplied data (not used by the clock). */
} MMAL_CLOCK_T;

/** Create a new instance of a clock.
 *
 * @param clock Returned clock
 *
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_clock_create(MMAL_CLOCK_T **clock);

/** Destroy a previously created clock.
 *
 * @param clock The clock to destroy
 *
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_clock_destroy(MMAL_CLOCK_T *clock);

/** Definition of a clock request callback.
 * This is invoked when the media-time requested by the client is reached.
 *
 * @param clock      The clock which serviced the request
 * @param media_time The current media-time
 * @param cb_data    Client-supplied data
 * @param priv       Function pointer used by the framework
 */
typedef void (*MMAL_CLOCK_VOID_FP)(void);
typedef void (*MMAL_CLOCK_REQUEST_CB)(MMAL_CLOCK_T *clock, int64_t media_time, void *cb_data, MMAL_CLOCK_VOID_FP priv);

/** Register a request with the clock.
 * When the specified media-time is reached, the clock will invoke the supplied callback.
 *
 * @param clock      The clock
 * @param media_time The media-time at which the callback should be invoked (microseconds)
 * @param cb         Callback to invoke
 * @param cb_data    Client-supplied callback data
 * @param priv       Function pointer used by the framework
 *
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_clock_request_add(MMAL_CLOCK_T *clock, int64_t media_time,
                                     MMAL_CLOCK_REQUEST_CB cb, void *cb_data, MMAL_CLOCK_VOID_FP priv);

/** Remove all previously registered clock requests.
 *
 * @param clock      The clock
 *
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_clock_request_flush(MMAL_CLOCK_T *clock);

/** Update the clock's media-time.
 *
 * @param clock      The clock to update
 * @param media_time New media-time to be applied (microseconds)
 *
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_clock_media_time_set(MMAL_CLOCK_T *clock, int64_t media_time);

/** Set the clock's scale.
 *
 * @param clock      The clock
 * @param scale      Scale factor
 *
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_clock_scale_set(MMAL_CLOCK_T *clock, MMAL_RATIONAL_T scale);

/** Set the clock state.
 *
 * @param clock      The clock
 * @param active     TRUE -> clock is active and media-time is advancing
 *
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_clock_active_set(MMAL_CLOCK_T *clock, MMAL_BOOL_T active);

/** Get the clock's scale.
 *
 * @param clock      The clock
 *
 * @return Current clock scale
 */
MMAL_RATIONAL_T mmal_clock_scale_get(MMAL_CLOCK_T *clock);

/** Get the clock's current media-time.
 * This takes the clock scale and media-time offset into account.
 *
 * @param clock      The clock to query
 *
 * @return Current media-time in microseconds
 */
int64_t mmal_clock_media_time_get(MMAL_CLOCK_T *clock);

/** Get the clock's state.
 *
 * @param clock      The clock to query
 *
 * @return TRUE if clock is running (i.e. local media-time is advancing)
 */
MMAL_BOOL_T mmal_clock_is_active(MMAL_CLOCK_T *clock);

/** Get the clock's media-time update threshold values.
 *
 * @param clock             The clock
 * @param update_threshold  Pointer to clock update threshold values to fill
 *
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_clock_update_threshold_get(MMAL_CLOCK_T *clock, MMAL_CLOCK_UPDATE_THRESHOLD_T *update_threshold);

/** Set the clock's media-time update threshold values.
 *
 * @param clock             The clock
 * @param update_threshold  Pointer to new clock update threshold values
 *
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_clock_update_threshold_set(MMAL_CLOCK_T *clock, const MMAL_CLOCK_UPDATE_THRESHOLD_T *update_threshold);

/** Get the clock's discontinuity threshold values.
 *
 * @param clock      The clock
 * @param discont    Pointer to clock discontinuity threshold values to fill
 *
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_clock_discont_threshold_get(MMAL_CLOCK_T *clock, MMAL_CLOCK_DISCONT_THRESHOLD_T *discont);

/** Set the clock's discontinuity threshold values.
 *
 * @param clock      The clock
 * @param discont    Pointer to new clock discontinuity threshold values
 *
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_clock_discont_threshold_set(MMAL_CLOCK_T *clock, const MMAL_CLOCK_DISCONT_THRESHOLD_T *discont);

/** Get the clock's request threshold values.
 *
 * @param clock      The clock
 * @param future     Pointer to clock request threshold values to fill
 *
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_clock_request_threshold_get(MMAL_CLOCK_T *clock, MMAL_CLOCK_REQUEST_THRESHOLD_T *req);

/** Set the clock's request threshold values.
 *
 * @param clock      The clock
 * @param discont    Pointer to new clock request threshold values
 *
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_clock_request_threshold_set(MMAL_CLOCK_T *clock, const MMAL_CLOCK_REQUEST_THRESHOLD_T *req);

#ifdef __cplusplus
}
#endif

#endif /* MMAL_CLOCK_PRIVATE_H */
