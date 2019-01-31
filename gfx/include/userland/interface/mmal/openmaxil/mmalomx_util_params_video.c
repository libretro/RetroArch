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

#include "mmalomx.h"
#include "mmalomx_util_params_common.h"
#include "mmalomx_logging.h"

static void rect_to_omx(OMX_DISPLAYRECTTYPE *dst, const MMAL_RECT_T *src)
{
   dst->x_offset  = src->x;
   dst->y_offset  = src->y;
   dst->width     = src->width;
   dst->height    = src->height;
}

static void rect_to_mmal(MMAL_RECT_T *dst, const OMX_DISPLAYRECTTYPE *src)
{
   dst->x = src->x_offset;
   dst->y = src->y_offset;
   dst->width = src->width;
   dst->height = src->height;
}

static MMAL_STATUS_T mmalomx_param_mapping_displayregion(MMALOMX_PARAM_MAPPING_DIRECTION dir,
   MMAL_PARAMETER_HEADER_T *mmal_param, OMX_PTR omx_param)
{
   OMX_CONFIG_DISPLAYREGIONTYPE *omx = (OMX_CONFIG_DISPLAYREGIONTYPE *)omx_param;
   MMAL_DISPLAYREGION_T *mmal = (MMAL_DISPLAYREGION_T *)mmal_param;

   if (dir == MMALOMX_PARAM_MAPPING_TO_MMAL)
   {
      mmal->set = omx->set;
      mmal->display_num = omx->num;
      mmal->fullscreen = omx->fullscreen;
      mmal->transform = (MMAL_DISPLAYTRANSFORM_T)omx->transform;
      rect_to_mmal(&mmal->dest_rect, &omx->dest_rect);
      rect_to_mmal(&mmal->src_rect, &omx->src_rect);
      mmal->noaspect = omx->noaspect;
      mmal->mode = (MMAL_DISPLAYMODE_T)omx->mode;
      mmal->pixel_x = omx->pixel_x;
      mmal->pixel_y = omx->pixel_y;
      mmal->layer = omx->layer;
      mmal->copyprotect_required = omx->copyprotect_required;
      mmal->alpha = omx->alpha;
   }
   else
   {
      omx->set        = mmal->set;
      omx->num        = mmal->display_num;
      omx->fullscreen = mmal->fullscreen;
      omx->transform  = (OMX_DISPLAYTRANSFORMTYPE)mmal->transform;
      rect_to_omx(&omx->dest_rect, &mmal->dest_rect);
      rect_to_omx(&omx->src_rect, &mmal->src_rect);
      omx->noaspect   = mmal->noaspect;
      omx->mode       = (OMX_DISPLAYMODETYPE)mmal->mode;
      omx->pixel_x    = mmal->pixel_x;
      omx->pixel_y    = mmal->pixel_y;
      omx->layer      = mmal->layer;
      omx->copyprotect_required = mmal->copyprotect_required;
      omx->alpha      = mmal->alpha;
   }

   return MMAL_SUCCESS;
}

static MMAL_STATUS_T mmalomx_param_list_supported_profiles(MMALOMX_PARAM_MAPPING_DIRECTION dir,
   const MMALOMX_PARAM_TRANSLATION_T *xlat, unsigned int index,
   MMAL_PARAMETER_HEADER_T *mmal_param, OMX_PTR omx_param, MMAL_PORT_T *mmal_port)
{
   OMX_VIDEO_PARAM_PROFILELEVELTYPE *omx = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)omx_param;
   MMAL_PARAMETER_VIDEO_PROFILE_T *mmal = (MMAL_PARAMETER_VIDEO_PROFILE_T *)mmal_param;
   MMAL_PARAM_UNUSED(xlat);

   if (dir == MMALOMX_PARAM_MAPPING_TO_MMAL)
   {
      OMX_VIDEO_CODINGTYPE coding = mmalil_encoding_to_omx_video_coding(mmal_port->format->encoding);
      mmal->profile[index].profile = mmalil_omx_video_profile_to_mmal(omx->eProfile, coding);
      mmal->profile[index].level = mmalil_omx_video_level_to_mmal(omx->eLevel, coding);
   }
   else
   {
      omx->eProfile = mmalil_video_profile_to_omx(mmal->profile[index].profile);
      omx->eLevel = mmalil_video_level_to_omx(mmal->profile[index].level);
   }

   return MMAL_SUCCESS;
}

static MMAL_STATUS_T mmalomx_param_custom_profile(MMALOMX_PARAM_MAPPING_DIRECTION dir,
   const MMALOMX_PARAM_TRANSLATION_T *xlat,
   MMAL_PARAMETER_HEADER_T *mmal_param, OMX_PTR omx_param, MMAL_PORT_T *mmal_port)
{
   OMX_VIDEO_PARAM_PROFILELEVELTYPE *omx = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)omx_param;
   MMAL_PARAMETER_VIDEO_PROFILE_T *mmal = (MMAL_PARAMETER_VIDEO_PROFILE_T *)mmal_param;
   MMAL_PARAM_UNUSED(xlat);

   if (dir == MMALOMX_PARAM_MAPPING_TO_MMAL)
   {
      OMX_VIDEO_CODINGTYPE coding = mmalil_encoding_to_omx_video_coding(mmal_port->format->encoding);
      mmal->profile[0].profile = mmalil_omx_video_profile_to_mmal(omx->eProfile, coding);
      mmal->profile[0].level = mmalil_omx_video_level_to_mmal(omx->eLevel, coding);
   }
   else
   {
      omx->eProfile = mmalil_video_profile_to_omx(mmal->profile[0].profile);
      omx->eLevel = mmalil_video_level_to_omx(mmal->profile[0].level);
   }

   return MMAL_SUCCESS;
}

static MMAL_STATUS_T mmalomx_param_custom_ratecontrol(MMALOMX_PARAM_MAPPING_DIRECTION dir,
   const MMALOMX_PARAM_TRANSLATION_T *xlat,
   MMAL_PARAMETER_HEADER_T *mmal_param, OMX_PTR omx_param, MMAL_PORT_T *mmal_port)
{
   OMX_VIDEO_PARAM_BITRATETYPE *omx = (OMX_VIDEO_PARAM_BITRATETYPE *)omx_param;
   MMAL_PARAMETER_VIDEO_RATECONTROL_T *mmal = (MMAL_PARAMETER_VIDEO_RATECONTROL_T *)mmal_param;
   MMAL_PARAM_UNUSED(xlat);

   if (dir == MMALOMX_PARAM_MAPPING_TO_MMAL)
   {
      mmal->control = mmalil_omx_video_ratecontrol_to_mmal(omx->eControlRate);
      /* This does not apply nTargetBitrate but should not be necessary */
   }
   else
   {
      omx->eControlRate   = mmalil_video_ratecontrol_to_omx(mmal->control);
      omx->nTargetBitrate = mmal_port->format->bitrate; /* Should not really be necessary */
   }

   return MMAL_SUCCESS;
}

static const MMALOMX_PARAM_ENUM_TRANSLATE_T mmalomx_param_enum_nalunitformat[] = {
   {MMAL_VIDEO_NALUNITFORMAT_STARTCODES,               OMX_NaluFormatStartCodes},
   {MMAL_VIDEO_NALUNITFORMAT_NALUNITPERBUFFER,         OMX_NaluFormatOneNaluPerBuffer},
   {MMAL_VIDEO_NALUNITFORMAT_ONEBYTEINTERLEAVELENGTH,  OMX_NaluFormatOneByteInterleaveLength},
   {MMAL_VIDEO_NALUNITFORMAT_TWOBYTEINTERLEAVELENGTH,  OMX_NaluFormatTwoByteInterleaveLength},
   {MMAL_VIDEO_NALUNITFORMAT_FOURBYTEINTERLEAVELENGTH, OMX_NaluFormatFourByteInterleaveLength},
};

static MMAL_STATUS_T mmalomx_param_mapping_frame_rate(MMALOMX_PARAM_MAPPING_DIRECTION dir,
   MMAL_PARAMETER_HEADER_T *mmal_param, OMX_PTR omx_param)
{
   OMX_CONFIG_FRAMERATETYPE *omx = (OMX_CONFIG_FRAMERATETYPE *)omx_param;
   MMAL_PARAMETER_FRAME_RATE_T *mmal = (MMAL_PARAMETER_FRAME_RATE_T *)mmal_param;

   if (dir == MMALOMX_PARAM_MAPPING_TO_MMAL)
   {
      mmal->frame_rate.num = omx->xEncodeFramerate;
      mmal->frame_rate.den = (1<<16);
   }
   else
   {
      omx->xEncodeFramerate = 0;
      if (mmal->frame_rate.den)
         omx->xEncodeFramerate = (((int64_t)mmal->frame_rate.num)<<16)/mmal->frame_rate.den;
   }

   return MMAL_SUCCESS;
}

const MMALOMX_PARAM_TRANSLATION_T mmalomx_param_xlator_video[] = {
   MMALOMX_PARAM_STRAIGHT_MAPPING(MMAL_PARAMETER_DISPLAYREGION, MMAL_DISPLAYREGION_T,
      OMX_IndexConfigDisplayRegion, OMX_CONFIG_DISPLAYREGIONTYPE,
      mmalomx_param_mapping_displayregion),
   MMALOMX_PARAM_LIST(MMAL_PARAMETER_SUPPORTED_PROFILES, MMAL_PARAMETER_VIDEO_PROFILE_T,
      OMX_IndexParamVideoProfileLevelQuerySupported, OMX_VIDEO_PARAM_PROFILELEVELTYPE,
      nProfileIndex, mmalomx_param_list_supported_profiles),
   MMALOMX_PARAM_CUSTOM(MMAL_PARAMETER_PROFILE, MMAL_PARAMETER_VIDEO_PROFILE_T,
      OMX_IndexParamVideoProfileLevelCurrent, OMX_VIDEO_PARAM_PROFILELEVELTYPE,
      mmalomx_param_custom_profile),
   MMALOMX_PARAM_PASSTHROUGH(MMAL_PARAMETER_INTRAPERIOD, MMAL_PARAMETER_UINT32_T,
      OMX_IndexConfigBrcmVideoIntraPeriod, OMX_PARAM_U32TYPE),
   MMALOMX_PARAM_CUSTOM(MMAL_PARAMETER_RATECONTROL, MMAL_PARAMETER_VIDEO_RATECONTROL_T,
      OMX_IndexParamVideoBitrate, OMX_VIDEO_PARAM_BITRATETYPE,
      mmalomx_param_custom_ratecontrol),
   MMALOMX_PARAM_ENUM(MMAL_PARAMETER_NALUNITFORMAT, MMAL_PARAMETER_VIDEO_NALUNITFORMAT_T,
      OMX_IndexParamNalStreamFormatSelect, OMX_NALSTREAMFORMATTYPE, mmalomx_param_enum_nalunitformat),
   MMALOMX_PARAM_BOOLEAN_PORTLESS(MMAL_PARAMETER_MINIMISE_FRAGMENTATION,
      OMX_IndexConfigMinimiseFragmentation),
   MMALOMX_PARAM_PASSTHROUGH(MMAL_PARAMETER_MB_ROWS_PER_SLICE, MMAL_PARAMETER_UINT32_T,
      OMX_IndexConfigBrcmVideoEncoderMBRowsPerSlice, OMX_PARAM_U32TYPE),
   MMALOMX_PARAM_PASSTHROUGH(MMAL_PARAMETER_VIDEO_LEVEL_EXTENSION, MMAL_PARAMETER_VIDEO_LEVEL_EXTENSION_T,
      OMX_IndexConfigEncLevelExtension, OMX_VIDEO_CONFIG_LEVEL_EXTEND),
   MMALOMX_PARAM_PASSTHROUGH(MMAL_PARAMETER_VIDEO_INTRA_REFRESH, MMAL_PARAMETER_VIDEO_INTRA_REFRESH_T,
      OMX_IndexConfigBrcmVideoIntraRefresh, OMX_VIDEO_PARAM_INTRAREFRESHTYPE),
   MMALOMX_PARAM_PASSTHROUGH(MMAL_PARAMETER_VIDEO_INTRA_REFRESH, MMAL_PARAMETER_VIDEO_INTRA_REFRESH_T,
      OMX_IndexParamVideoIntraRefresh, OMX_VIDEO_PARAM_INTRAREFRESHTYPE),
   MMALOMX_PARAM_PASSTHROUGH(MMAL_PARAMETER_VIDEO_EEDE_ENABLE, MMAL_PARAMETER_VIDEO_EEDE_ENABLE_T,
      OMX_IndexParamBrcmEEDEEnable, OMX_VIDEO_EEDE_ENABLE),
   MMALOMX_PARAM_PASSTHROUGH(MMAL_PARAMETER_VIDEO_EEDE_LOSSRATE, MMAL_PARAMETER_VIDEO_EEDE_LOSSRATE_T,
      OMX_IndexParamBrcmEEDELossRate, OMX_VIDEO_EEDE_LOSSRATE),
   MMALOMX_PARAM_BOOLEAN_PORTLESS(MMAL_PARAMETER_VIDEO_REQUEST_I_FRAME,
      OMX_IndexConfigBrcmVideoRequestIFrame),
   MMALOMX_PARAM_BOOLEAN_PORTLESS(MMAL_PARAMETER_VIDEO_IMMUTABLE_INPUT,
      OMX_IndexParamBrcmImmutableInput),
   MMALOMX_PARAM_PASSTHROUGH(MMAL_PARAMETER_VIDEO_BIT_RATE, MMAL_PARAMETER_UINT32_T,
      OMX_IndexConfigVideoBitrate, OMX_VIDEO_CONFIG_BITRATETYPE),
   MMALOMX_PARAM_STRAIGHT_MAPPING(MMAL_PARAMETER_VIDEO_FRAME_RATE, MMAL_PARAMETER_FRAME_RATE_T,
      OMX_IndexConfigVideoFramerate, OMX_CONFIG_FRAMERATETYPE, mmalomx_param_mapping_frame_rate),
   MMALOMX_PARAM_STRAIGHT_MAPPING(MMAL_PARAMETER_FRAME_RATE, MMAL_PARAMETER_FRAME_RATE_T,
      OMX_IndexConfigVideoFramerate, OMX_CONFIG_FRAMERATETYPE, mmalomx_param_mapping_frame_rate),
   MMALOMX_PARAM_PASSTHROUGH(MMAL_PARAMETER_VIDEO_ENCODE_MIN_QUANT, MMAL_PARAMETER_UINT32_T,
      OMX_IndexParamBrcmVideoEncodeMinQuant, OMX_PARAM_U32TYPE),
   MMALOMX_PARAM_PASSTHROUGH(MMAL_PARAMETER_VIDEO_ENCODE_MAX_QUANT, MMAL_PARAMETER_UINT32_T,
      OMX_IndexParamBrcmVideoEncodeMaxQuant, OMX_PARAM_U32TYPE),
   MMALOMX_PARAM_PASSTHROUGH(MMAL_PARAMETER_VIDEO_ENCODE_RC_MODEL, MMAL_PARAMETER_VIDEO_ENCODE_RC_MODEL_T,
      OMX_IndexParamRateControlModel, OMX_PARAM_U32TYPE),
   MMALOMX_PARAM_PASSTHROUGH(MMAL_PARAMETER_EXTRA_BUFFERS, MMAL_PARAMETER_UINT32_T,
      OMX_IndexParamBrcmExtraBuffers, OMX_PARAM_U32TYPE),
   MMALOMX_PARAM_PASSTHROUGH(MMAL_PARAMETER_VIDEO_ALIGN_HORIZ, MMAL_PARAMETER_UINT32_T,
      OMX_IndexParamBrcmAlignHoriz, OMX_PARAM_U32TYPE),
   MMALOMX_PARAM_PASSTHROUGH(MMAL_PARAMETER_VIDEO_ALIGN_VERT, MMAL_PARAMETER_UINT32_T,
      OMX_IndexParamBrcmAlignVert, OMX_PARAM_U32TYPE),
   MMALOMX_PARAM_BOOLEAN_PORTLESS(MMAL_PARAMETER_VIDEO_DROPPABLE_PFRAMES,
      OMX_IndexParamBrcmDroppablePFrames),
   MMALOMX_PARAM_PASSTHROUGH(MMAL_PARAMETER_VIDEO_ENCODE_INITIAL_QUANT, MMAL_PARAMETER_UINT32_T,
      OMX_IndexParamBrcmVideoInitialQuant, OMX_PARAM_U32TYPE),
   MMALOMX_PARAM_PASSTHROUGH(MMAL_PARAMETER_VIDEO_ENCODE_QP_P, MMAL_PARAMETER_UINT32_T,
      OMX_IndexParamBrcmVideoInitialQuant, OMX_PARAM_U32TYPE),
   MMALOMX_PARAM_PASSTHROUGH(MMAL_PARAMETER_VIDEO_ENCODE_RC_SLICE_DQUANT, MMAL_PARAMETER_UINT32_T,
      OMX_IndexParamBrcmVideoRCSliceDQuant, OMX_PARAM_U32TYPE),
   MMALOMX_PARAM_PASSTHROUGH(MMAL_PARAMETER_VIDEO_ENCODE_FRAME_LIMIT_BITS, MMAL_PARAMETER_UINT32_T,
      OMX_IndexParamBrcmVideoFrameLimitBits, OMX_PARAM_U32TYPE),
   MMALOMX_PARAM_PASSTHROUGH(MMAL_PARAMETER_VIDEO_ENCODE_PEAK_RATE, MMAL_PARAMETER_UINT32_T,
      OMX_IndexParamBrcmVideoPeakRate, OMX_PARAM_U32TYPE),
   MMALOMX_PARAM_BOOLEAN_PORTLESS(MMAL_PARAMETER_VIDEO_ENCODE_H264_DISABLE_CABAC,
      OMX_IndexConfigBrcmVideoH264DisableCABAC),
   MMALOMX_PARAM_BOOLEAN_PORTLESS(MMAL_PARAMETER_VIDEO_ENCODE_H264_LOW_LATENCY,
      OMX_IndexConfigBrcmVideoH264LowLatency),
   MMALOMX_PARAM_BOOLEAN_PORTLESS(MMAL_PARAMETER_VIDEO_ENCODE_H264_AU_DELIMITERS,
      OMX_IndexConfigBrcmVideoH264AUDelimiters),
   MMALOMX_PARAM_PASSTHROUGH(MMAL_PARAMETER_VIDEO_ENCODE_H264_DEBLOCK_IDC, MMAL_PARAMETER_UINT32_T,
      OMX_IndexConfigBrcmVideoH264DeblockIDC, OMX_PARAM_U32TYPE),
   MMALOMX_PARAM_PASSTHROUGH(MMAL_PARAMETER_VIDEO_ENCODE_H264_MB_INTRA_MODE, MMAL_PARAMETER_VIDEO_ENCODER_H264_MB_INTRA_MODES_T,
      OMX_IndexConfigBrcmVideoH264IntraMBMode, OMX_PARAM_U32TYPE),
   MMALOMX_PARAM_BOOLEAN_PORTLESS(MMAL_PARAMETER_VIDEO_ENCODE_HEADER_ON_OPEN,
      OMX_IndexParamBrcmHeaderOnOpen),
   MMALOMX_PARAM_BOOLEAN_PORTLESS(MMAL_PARAMETER_VIDEO_ENCODE_PRECODE_FOR_QP,
      OMX_IndexParamBrcmVideoPrecodeForQP),
   MMALOMX_PARAM_BOOLEAN_PORTLESS(MMAL_PARAMETER_VIDEO_TIMESTAMP_FIFO,
      OMX_IndexParamBrcmVideoTimestampFifo),
   MMALOMX_PARAM_BOOLEAN_PORTLESS(MMAL_PARAMETER_VIDEO_DECODE_ERROR_CONCEALMENT,
      OMX_IndexParamBrcmVideoDecodeErrorConcealment),
   MMALOMX_PARAM_PASSTHROUGH_PORTLESS_DOUBLE_TRANSLATION(MMAL_PARAMETER_VIDEO_DRM_PROTECT_BUFFER, MMAL_PARAMETER_VIDEO_DRM_PROTECT_BUFFER_T,
      OMX_IndexParamBrcmVideoDrmProtectBuffer, OMX_PARAM_BRCMVIDEODRMPROTECTBUFFERTYPE),
   MMALOMX_PARAM_PASSTHROUGH_PORTLESS(MMAL_PARAMETER_VIDEO_DECODE_CONFIG_VD3, MMAL_PARAMETER_BYTES_T,
      OMX_IndexParamBrcmVideoDecodeConfigVD3, OMX_PARAM_BRCMVIDEODECODECONFIGVD3TYPE),
   MMALOMX_PARAM_BOOLEAN(MMAL_PARAMETER_VIDEO_ENCODE_INLINE_HEADER,
      OMX_IndexParamBrcmVideoAVCInlineHeaderEnable),
   MMALOMX_PARAM_TERMINATE()
};
