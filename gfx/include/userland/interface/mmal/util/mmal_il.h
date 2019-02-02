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

#ifndef MMAL_IL_H
#define MMAL_IL_H

/** \defgroup MmalILUtility MMAL to OMX IL conversion utilities
 * \ingroup MmalUtilities
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "interface/vmcs_host/khronos/IL/OMX_Core.h"
#include "interface/vmcs_host/khronos/IL/OMX_Component.h"
#include "interface/vmcs_host/khronos/IL/OMX_Video.h"
#include "interface/vmcs_host/khronos/IL/OMX_Audio.h"
#include "interface/vmcs_host/khronos/IL/OMX_Broadcom.h"

/** Convert MMAL status codes into OMX error codes.
 *
 * @param status MMAL status code.
 * @return OMX error code.
 */
OMX_ERRORTYPE mmalil_error_to_omx(MMAL_STATUS_T status);

/** Convert OMX error codes into MMAL status codes.
 *
 * @param error OMX error code.
 * @return MMAL status code.
 */
MMAL_STATUS_T mmalil_error_to_mmal(OMX_ERRORTYPE error);

/** Convert MMAL buffer header flags into OMX buffer header flags.
 *
 * @param flags OMX buffer header flags.
 * @return MMAL buffer header flags.
 */
uint32_t mmalil_buffer_flags_to_mmal(OMX_U32 flags);

/** Convert OMX buffer header flags into MMAL buffer header flags.
 *
 * @param flags MMAL buffer header flags.
 * @return OMX buffer header flags.
 */
OMX_U32 mmalil_buffer_flags_to_omx(uint32_t flags);

/** Convert MMAL buffer header type video specific flags into OMX buffer header
 * flags.
 *
 * @param flags OMX buffer header flags.
 * @return MMAL buffer header video specific flags.
 */
uint32_t mmalil_video_buffer_flags_to_mmal(OMX_U32 flags);

/** Convert OMX buffer header flags into MMAL buffer header type video specific
 * flags.
 *
 * @param flags MMAL buffer header video specific flags.
 * @return OMX buffer header flags.
 */
OMX_U32 mmalil_video_buffer_flags_to_omx(uint32_t flags);

/** Convert a MMAL buffer header into an OMX buffer header.
 * Note that only the fields which have a direct mapping between OMX and MMAL are converted.
 *
 * @param omx  Pointer to the destination OMX buffer header.
 * @param mmal Pointer to the source MMAL buffer header.
 */
void mmalil_buffer_header_to_omx(OMX_BUFFERHEADERTYPE *omx, MMAL_BUFFER_HEADER_T *mmal);

/** Convert an OMX buffer header into a MMAL buffer header.
 *
 * @param mmal Pointer to the destination MMAL buffer header.
 * @param omx  Pointer to the source OMX buffer header.
 */
void mmalil_buffer_header_to_mmal(MMAL_BUFFER_HEADER_T *mmal, OMX_BUFFERHEADERTYPE *omx);

OMX_PORTDOMAINTYPE mmalil_es_type_to_omx_domain(MMAL_ES_TYPE_T type);
MMAL_ES_TYPE_T mmalil_omx_domain_to_es_type(OMX_PORTDOMAINTYPE domain);
uint32_t mmalil_omx_audio_coding_to_encoding(OMX_AUDIO_CODINGTYPE coding);
OMX_AUDIO_CODINGTYPE mmalil_encoding_to_omx_audio_coding(uint32_t encoding);
uint32_t mmalil_omx_video_coding_to_encoding(OMX_VIDEO_CODINGTYPE coding);
OMX_VIDEO_CODINGTYPE mmalil_encoding_to_omx_video_coding(uint32_t encoding);
uint32_t mmalil_omx_image_coding_to_encoding(OMX_IMAGE_CODINGTYPE coding);
OMX_IMAGE_CODINGTYPE mmalil_encoding_to_omx_image_coding(uint32_t encoding);
uint32_t mmalil_omx_coding_to_encoding(uint32_t encoding, OMX_PORTDOMAINTYPE domain);
uint32_t mmalil_omx_color_format_to_encoding(OMX_COLOR_FORMATTYPE coding);
OMX_COLOR_FORMATTYPE mmalil_encoding_to_omx_color_format(uint32_t encoding);
uint32_t mmalil_omx_bayer_format_order_to_encoding(OMX_BAYERORDERTYPE bayer_order, OMX_COLOR_FORMATTYPE color_format);
OMX_BAYERORDERTYPE mmalil_encoding_to_omx_bayer_order(uint32_t encoding);
uint32_t mmalil_omx_color_space_to_mmal(OMX_COLORSPACETYPE coding);
OMX_COLORSPACETYPE mmalil_color_space_to_omx(uint32_t coding);
uint32_t mmalil_omx_video_profile_to_mmal(OMX_U32 level, OMX_VIDEO_CODINGTYPE coding);
OMX_U32 mmalil_video_profile_to_omx(uint32_t profile);
uint32_t mmalil_omx_video_level_to_mmal(OMX_U32 level, OMX_VIDEO_CODINGTYPE coding);
OMX_U32 mmalil_video_level_to_omx(uint32_t level);
MMAL_VIDEO_RATECONTROL_T mmalil_omx_video_ratecontrol_to_mmal(OMX_VIDEO_CONTROLRATETYPE omx);
OMX_VIDEO_CONTROLRATETYPE mmalil_video_ratecontrol_to_omx(MMAL_VIDEO_RATECONTROL_T mmal);
MMAL_VIDEO_INTRA_REFRESH_T mmalil_omx_video_intrarefresh_to_mmal(OMX_VIDEO_INTRAREFRESHTYPE omx);

/** Union of all the OMX_VIDEO/AUDIO_PARAM types */
typedef union OMX_FORMAT_PARAM_TYPE {
   OMX_PARAM_U32TYPE common;

   /* Video */
   OMX_VIDEO_PARAM_AVCTYPE avc;
   OMX_VIDEO_PARAM_H263TYPE h263;
   OMX_VIDEO_PARAM_MPEG2TYPE mpeg2;
   OMX_VIDEO_PARAM_MPEG4TYPE mpeg4;
   OMX_VIDEO_PARAM_WMVTYPE wmv;
   OMX_VIDEO_PARAM_RVTYPE rv;

   /* Audio */
   OMX_AUDIO_PARAM_PCMMODETYPE pcm;
   OMX_AUDIO_PARAM_MP3TYPE mp3;
   OMX_AUDIO_PARAM_AACPROFILETYPE aac;
   OMX_AUDIO_PARAM_VORBISTYPE vorbis;
   OMX_AUDIO_PARAM_WMATYPE wma;
   OMX_AUDIO_PARAM_RATYPE ra;
   OMX_AUDIO_PARAM_SBCTYPE sbc;
   OMX_AUDIO_PARAM_ADPCMTYPE adpcm;
   OMX_AUDIO_PARAM_G723TYPE g723;
   OMX_AUDIO_PARAM_G726TYPE g726;
   OMX_AUDIO_PARAM_G729TYPE g729;
   OMX_AUDIO_PARAM_AMRTYPE amr;
   OMX_AUDIO_PARAM_GSMFRTYPE gsmfr;
   OMX_AUDIO_PARAM_GSMHRTYPE gsmhr;
   OMX_AUDIO_PARAM_GSMEFRTYPE gsmefr;
   OMX_AUDIO_PARAM_TDMAFRTYPE tdmafr;
   OMX_AUDIO_PARAM_TDMAEFRTYPE tdmaefr;
   OMX_AUDIO_PARAM_PDCFRTYPE pdcfr;
   OMX_AUDIO_PARAM_PDCEFRTYPE pdcefr;
   OMX_AUDIO_PARAM_PDCHRTYPE pdchr;
   OMX_AUDIO_PARAM_QCELP8TYPE qcelp8;
   OMX_AUDIO_PARAM_QCELP13TYPE qcelp13;
   OMX_AUDIO_PARAM_EVRCTYPE evrc;
   OMX_AUDIO_PARAM_SMVTYPE smv;
   OMX_AUDIO_PARAM_MIDITYPE midi;
#ifdef OMX_AUDIO_CodingDDP_Supported
   OMX_AUDIO_PARAM_DDPTYPE ddp;
#endif
#ifdef OMX_AUDIO_CodingDTS_Supported
   OMX_AUDIO_PARAM_DTSTYPE dts;
#endif

} OMX_FORMAT_PARAM_TYPE;

/** Get the OMX_IndexParamAudio index corresponding to a specified audio coding type.
 *
 * @param coding Audio coding type.
 * @param size  Pointer used to return the size of the parameter.
 *
 * @return OMX index or 0 if no match was found.
 */
OMX_INDEXTYPE mmalil_omx_audio_param_index(OMX_AUDIO_CODINGTYPE coding, OMX_U32 *size);

/** Get the audio coding corresponding to a specified OMX_IndexParamAudio index.
 *
 * @param index Audio coding type.
 *
 * @return Audio coding type.
 */
OMX_AUDIO_CODINGTYPE mmalil_omx_audio_param_index_to_coding(OMX_INDEXTYPE index);

/** Setup a default channel mapping based on the number of channels
 * @param channel_mapping The output channel mapping
 * @param nchannels Number of channels
 *
 * @return MMAL_SUCCESS if we managed to produce a channel mapping
 */
MMAL_STATUS_T mmalil_omx_default_channel_mapping(OMX_AUDIO_CHANNELTYPE *channel_mapping, unsigned int nchannels);

/** Convert an OMX_IndexParamAudio into a MMAL elementary stream format.
 *
 * @param format Format structure to update.
 * @param coding Audio coding type.
 * @param param  Source OMX_IndexParamAudio structure.
 *
 * @return The MMAL encoding if a match was found or MMAL_ENCODING_UNKNOWN otherwise.
 */
MMAL_FOURCC_T mmalil_omx_audio_param_to_format(MMAL_ES_FORMAT_T *format,
   OMX_AUDIO_CODINGTYPE coding, OMX_FORMAT_PARAM_TYPE *param);

/** Convert a MMAL elementary stream format into a OMX_IndexParamAudio structure.
 *
 * @param param  OMX_IndexParamAudio structure to update.
 * @param param_index returns the OMX_IndexParamAudio index corresponding to the format.
 * @param format Source format structure.
 *
 * @return The OMX aduio coding type if a match was found or OMX_AUDIO_CodingUnused otherwise.
 */
OMX_AUDIO_CODINGTYPE mmalil_format_to_omx_audio_param(OMX_FORMAT_PARAM_TYPE *param,
   OMX_INDEXTYPE *param_index, MMAL_ES_FORMAT_T *format);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* MMAL_IL_H */
