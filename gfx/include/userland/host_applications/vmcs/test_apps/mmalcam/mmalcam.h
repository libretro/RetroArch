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
#ifndef MMALCAM_MMALCAM_H_
#define MMALCAM_MMALCAM_H_

#define VCOS_LOG_CATEGORY (&mmalcam_log_category)
#include "interface/vcos/vcos.h"
#include "interface/mmal/mmal.h"

extern VCOS_LOG_CAT_T mmalcam_log_category;

typedef enum
{
   MMALCAM_CHANGE_NONE,
   MMALCAM_CHANGE_IMAGE_EFFECT,
   MMALCAM_CHANGE_ROTATION,
   MMALCAM_CHANGE_ZOOM,
   MMALCAM_CHANGE_FOCUS,
   MMALCAM_CHANGE_DRC,
   MMALCAM_CHANGE_HDR,
   MMALCAM_CHANGE_CONTRAST,
   MMALCAM_CHANGE_BRIGHTNESS,
   MMALCAM_CHANGE_SATURATION,
   MMALCAM_CHANGE_SHARPNESS,
} MMALCAM_CHANGE_T;

typedef enum
{
   MMALCAM_INIT_SUCCESS,
   MMALCAM_INIT_ERROR_EVENT_FLAGS,
   MMALCAM_INIT_ERROR_VCSM_INIT,
   MMALCAM_INIT_ERROR_CAMERA,
   MMALCAM_INIT_ERROR_RENDER,
   MMALCAM_INIT_ERROR_VIEWFINDER,
   MMALCAM_INIT_ERROR_ENCODER,
   MMALCAM_INIT_ERROR_ENCODER_IN,
   MMALCAM_INIT_ERROR_ENCODER_OUT,
   MMALCAM_INIT_ERROR_WRITER,
   MMALCAM_INIT_ERROR_CAMERA_CAPTURE,
} MMALCAM_INIT_STATUS_T;

typedef struct MMALCAM_BEHAVIOUR_T
{
   const char *uri;                             /**< Output URI for recording */
   const char *vformat;                         /**< Video resolution and encoding format */
   MMAL_RECT_T display_area;                    /**< Size and position of viewfinder on screen */
   uint32_t layer;                              /**< Layer number of the viewfinder */
   MMALCAM_CHANGE_T change;                     /**< Camera change to make, if any */
   uint32_t seconds_per_change;                 /**< Number of seconds between changes */
   MMAL_RATIONAL_T frame_rate;                  /**< Frame rate, or zero for variable */
   MMAL_BOOL_T zero_copy;                       /**< Enable zero copy if set */
   MMAL_BOOL_T tunneling;                       /**< Enable port tunneling if set */
   MMAL_BOOL_T opaque;                          /**< Enable opaque image support */
   VCOS_SEMAPHORE_T init_sem;                   /**< Semaphore signalled once initialisation is complete */
   MMALCAM_INIT_STATUS_T init_result;           /**< Result of initialisation */
   MMAL_PARAMETER_STATISTICS_T render_stats;    /**< Video render stats */
   MMAL_PARAMETER_STATISTICS_T encoder_stats;   /**< Video encoder output stats */
   uint32_t bit_rate;                           /**< Video encoder bit rate */
   MMAL_PARAM_FOCUS_T focus_test;               /**< Set to given focus, MMAL_PARAM_FOCUS_MAX to disable */
   uint32_t camera_num;                         /**< camera number */
} MMALCAM_BEHAVIOUR_T;

/** Start the camcorder.
 *
 * Starts a viewfinder/preview on screen and optionally encodes the camera
 * output to a URI.
 *
 * @param stop When this is set to 1 externally, the camcorder will be stopped.
 * @param behaviour Defines the behaviour of the camcorder, for automation
 *    purposes.
 */
int test_mmal_start_camcorder(volatile int *stop, MMALCAM_BEHAVIOUR_T *behaviour);

#endif /* MMALCAM_MMALCAM_H_ */
