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

#ifndef MMAL_DEFAULT_COMPONENTS_H
#define MMAL_DEFAULT_COMPONENTS_H

/** \defgroup MmalDefaultComponents List of default components
 * This provides a list of default components on a per platform basis.
 * @{
 */

#define MMAL_COMPONENT_DEFAULT_CONTAINER_READER "container_reader"
#define MMAL_COMPONENT_DEFAULT_CONTAINER_WRITER "container_writer"

#if defined(ENABLE_MMAL_STANDALONE)
# define MMAL_COMPONENT_DEFAULT_VIDEO_DECODER    "avcodec.video_decode"
# define MMAL_COMPONENT_DEFAULT_VIDEO_ENCODER    "avcodec.video_encode"
# define MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER   "sdl.video_render"
# define MMAL_COMPONENT_DEFAULT_IMAGE_DECODER    "avcodec.video_decode"
# define MMAL_COMPONENT_DEFAULT_IMAGE_ENCODER    "avcodec.video_encode"
# define MMAL_COMPONENT_DEFAULT_CAMERA           "artificial_camera"
# define MMAL_COMPONENT_DEFAULT_VIDEO_CONVERTER  "avcodec.video_convert"
# define MMAL_COMPONENT_DEFAULT_SPLITTER         "splitter"
# define MMAL_COMPONENT_DEFAULT_SCHEDULER        "scheduler"
# define MMAL_COMPONENT_DEFAULT_VIDEO_INJECTER   "video_inject"
# define MMAL_COMPONENT_DEFAULT_AUDIO_DECODER    "avcodec.audio_decode"
# define MMAL_COMPONENT_DEFAULT_AUDIO_RENDERER   "sdl.audio_render"
# define MMAL_COMPONENT_DEFAULT_MIRACAST         "miracast"
# define MMAL_COMPONENT_DEFAULT_CLOCK            "clock"
#elif defined(__VIDEOCORE__)
# define MMAL_COMPONENT_DEFAULT_VIDEO_DECODER    "ril.video_decode"
# define MMAL_COMPONENT_DEFAULT_VIDEO_ENCODER    "ril.video_encode"
# define MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER   "ril.video_render"
# define MMAL_COMPONENT_DEFAULT_IMAGE_DECODER    "ril.image_decode"
# define MMAL_COMPONENT_DEFAULT_IMAGE_ENCODER    "ril.image_encode"
# define MMAL_COMPONENT_DEFAULT_CAMERA           "ril.camera"
# define MMAL_COMPONENT_DEFAULT_VIDEO_CONVERTER  "video_convert"
# define MMAL_COMPONENT_DEFAULT_SPLITTER         "splitter"
# define MMAL_COMPONENT_DEFAULT_SCHEDULER        "scheduler"
# define MMAL_COMPONENT_DEFAULT_VIDEO_INJECTER   "video_inject"
# define MMAL_COMPONENT_DEFAULT_VIDEO_SPLITTER   "ril.video_splitter"
# define MMAL_COMPONENT_DEFAULT_AUDIO_DECODER    "none"
# define MMAL_COMPONENT_DEFAULT_AUDIO_RENDERER   "ril.audio_render"
# define MMAL_COMPONENT_DEFAULT_MIRACAST         "miracast"
# define MMAL_COMPONENT_DEFAULT_CLOCK            "clock"
# define MMAL_COMPONENT_DEFAULT_CAMERA_INFO      "camera_info"
#else
# define MMAL_COMPONENT_DEFAULT_VIDEO_DECODER    "vc.ril.video_decode"
# define MMAL_COMPONENT_DEFAULT_VIDEO_ENCODER    "vc.ril.video_encode"
# define MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER   "vc.ril.video_render"
# define MMAL_COMPONENT_DEFAULT_IMAGE_DECODER    "vc.ril.image_decode"
# define MMAL_COMPONENT_DEFAULT_IMAGE_ENCODER    "vc.ril.image_encode"
# define MMAL_COMPONENT_DEFAULT_CAMERA           "vc.ril.camera"
# define MMAL_COMPONENT_DEFAULT_VIDEO_CONVERTER  "vc.video_convert"
# define MMAL_COMPONENT_DEFAULT_SPLITTER         "vc.splitter"
# define MMAL_COMPONENT_DEFAULT_SCHEDULER        "vc.scheduler"
# define MMAL_COMPONENT_DEFAULT_VIDEO_INJECTER   "vc.video_inject"
# define MMAL_COMPONENT_DEFAULT_VIDEO_SPLITTER   "vc.ril.video_splitter"
# define MMAL_COMPONENT_DEFAULT_AUDIO_DECODER    "none"
# define MMAL_COMPONENT_DEFAULT_AUDIO_RENDERER   "vc.ril.audio_render"
# define MMAL_COMPONENT_DEFAULT_MIRACAST         "vc.miracast"
# define MMAL_COMPONENT_DEFAULT_CLOCK            "vc.clock"
# define MMAL_COMPONENT_DEFAULT_CAMERA_INFO      "vc.camera_info"
#endif

/** @} */

#endif /* MMAL_DEFAULT_COMPONENTS_H */
