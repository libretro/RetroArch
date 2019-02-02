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
 * MMAL test application which plays back video files
 * Note: this is test code. Do not use this in your app. It *will* change or even be removed without notice.
 */

#include "interface/mmal/mmal.h"

#define VCOS_LOG_CATEGORY (&mmalplay_log_category)
#include "interface/vcos/vcos.h"

extern VCOS_LOG_CAT_T mmalplay_log_category;

/** MMALPLAY instance id.
 */
typedef struct MMALPLAY_T MMALPLAY_T;

/** MMALPLAY configuration options.
 */
typedef struct {
   uint32_t output_format;
   MMAL_RECT_T output_rect;
   uint32_t render_format;
   MMAL_RECT_T render_rect;
   uint32_t render_layer;

   int copy_input;
   int copy_output;

   int tunnelling;
   unsigned int output_num;

   int disable_playback;
   int disable_video;
   int disable_audio;
   int enable_scheduling;
   int disable_video_decode;

   const char *component_container_reader;
   const char *component_video_decoder;
   const char *component_splitter;
   const char *component_video_render;
   const char *component_video_converter;
   const char *component_video_scheduler;

   const char *component_audio_decoder;
   const char *component_audio_render;

   const char *audio_destination;
   unsigned int video_destination;

   const char *output_uri;

   int window;

   float seeking;
   int stepping;

   int audio_passthrough;
} MMALPLAY_OPTIONS_T;

/** Create an instance of mmalplay.
 *
 * @param uri URI for the video stream to play
 * @param opts configuration options for the instance to be created
 * @param status status of the operation
 *
 * @return a MMALPLAY_T instance id on success
 */
MMALPLAY_T *mmalplay_create(const char *uri, MMALPLAY_OPTIONS_T *opts, MMAL_STATUS_T *status);

/** Start playback on an instance of mmalplay.
 *
 * @param MMALPLAY instance id
 *
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmalplay_play(MMALPLAY_T *ctx);

/** Stop the playback on an instance of mmalplay.
 *
 * @param MMALPLAY instance id
 */
void mmalplay_stop(MMALPLAY_T *ctx);

/** Destroys an instance of mmalplay.
 *
 * @param MMALPLAY instance id
 */
void mmalplay_destroy(MMALPLAY_T *ctx);
