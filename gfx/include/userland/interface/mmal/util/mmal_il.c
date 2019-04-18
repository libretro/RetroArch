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

#include "mmal.h"
#include "util/mmal_il.h"
#include "interface/vmcs_host/khronos/IL/OMX_Broadcom.h"

/*****************************************************************************/
static struct {
   MMAL_STATUS_T mmal;
   OMX_ERRORTYPE omx;
} mmal_omx_error[] =
{
   {MMAL_SUCCESS, OMX_ErrorNone},
   {MMAL_ENOMEM, OMX_ErrorInsufficientResources},
   {MMAL_ENOSPC, OMX_ErrorInsufficientResources},
   {MMAL_EINVAL, OMX_ErrorBadParameter},
   {MMAL_ENOSYS, OMX_ErrorNotImplemented},
   {(MMAL_STATUS_T)-1, OMX_ErrorUndefined},
};

OMX_ERRORTYPE mmalil_error_to_omx(MMAL_STATUS_T status)
{
   unsigned int i;
   for(i = 0; mmal_omx_error[i].mmal != (MMAL_STATUS_T)-1; i++)
      if(mmal_omx_error[i].mmal == status) break;
   return mmal_omx_error[i].omx;
}

MMAL_STATUS_T mmalil_error_to_mmal(OMX_ERRORTYPE error)
{
   unsigned int i;
   for(i = 0; mmal_omx_error[i].mmal != (MMAL_STATUS_T)-1; i++)
      if(mmal_omx_error[i].omx == error) break;
   return mmal_omx_error[i].mmal;
}

/*****************************************************************************/
OMX_U32 mmalil_buffer_flags_to_omx(uint32_t flags)
{
   OMX_U32 omx_flags = 0;

   if(flags & MMAL_BUFFER_HEADER_FLAG_KEYFRAME)
      omx_flags |= OMX_BUFFERFLAG_SYNCFRAME;
   if(flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END)
      omx_flags |= OMX_BUFFERFLAG_ENDOFFRAME;
   if(flags & MMAL_BUFFER_HEADER_FLAG_EOS)
      omx_flags |= OMX_BUFFERFLAG_EOS;
   if(flags & MMAL_BUFFER_HEADER_FLAG_CONFIG)
      omx_flags |= OMX_BUFFERFLAG_CODECCONFIG;
   if(flags & MMAL_BUFFER_HEADER_FLAG_DISCONTINUITY)
      omx_flags |= OMX_BUFFERFLAG_DISCONTINUITY;
   if (flags & MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO)
      omx_flags |= OMX_BUFFERFLAG_CODECSIDEINFO;
   if (flags & MMAL_BUFFER_HEADER_FLAGS_SNAPSHOT)
      omx_flags |= OMX_BUFFERFLAG_CAPTURE_PREVIEW;
   if (flags & MMAL_BUFFER_HEADER_FLAG_CORRUPTED)
      omx_flags |= OMX_BUFFERFLAG_DATACORRUPT;
   if (flags & MMAL_BUFFER_HEADER_FLAG_DECODEONLY)
      omx_flags |= OMX_BUFFERFLAG_DECODEONLY;
   if (flags & MMAL_BUFFER_HEADER_VIDEO_FLAG_INTERLACED)
      omx_flags |= OMX_BUFFERFLAG_INTERLACED;
   if (flags & MMAL_BUFFER_HEADER_VIDEO_FLAG_TOP_FIELD_FIRST)
     omx_flags |= OMX_BUFFERFLAG_TOP_FIELD_FIRST;
   if (flags & MMAL_BUFFER_HEADER_FLAG_NAL_END)
     omx_flags |= OMX_BUFFERFLAG_ENDOFNAL;

   if (flags & MMAL_BUFFER_HEADER_FLAG_USER0)
      omx_flags |= OMX_BUFFERFLAG_USR0;
   if (flags & MMAL_BUFFER_HEADER_FLAG_USER1)
      omx_flags |= OMX_BUFFERFLAG_USR1;
   if (flags & MMAL_BUFFER_HEADER_FLAG_USER2)
      omx_flags |= OMX_BUFFERFLAG_USR2;
   if (flags & MMAL_BUFFER_HEADER_FLAG_USER3)
      omx_flags |= OMX_BUFFERFLAG_USR3;

   return omx_flags;
}

uint32_t mmalil_buffer_flags_to_mmal(OMX_U32 flags)
{
   uint32_t mmal_flags = 0;

   if (flags & OMX_BUFFERFLAG_SYNCFRAME)
      mmal_flags |= MMAL_BUFFER_HEADER_FLAG_KEYFRAME;
   if (flags & OMX_BUFFERFLAG_ENDOFFRAME)
      mmal_flags |= MMAL_BUFFER_HEADER_FLAG_FRAME_END;
   if (flags & OMX_BUFFERFLAG_EOS)
      mmal_flags |= MMAL_BUFFER_HEADER_FLAG_EOS;
   if (flags & OMX_BUFFERFLAG_CODECCONFIG)
      mmal_flags |= MMAL_BUFFER_HEADER_FLAG_CONFIG;
   if (flags & OMX_BUFFERFLAG_DISCONTINUITY)
      mmal_flags |= MMAL_BUFFER_HEADER_FLAG_DISCONTINUITY;
   if (flags & OMX_BUFFERFLAG_CODECSIDEINFO)
      mmal_flags |= MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO;
   if (flags & OMX_BUFFERFLAG_CAPTURE_PREVIEW)
      mmal_flags |= MMAL_BUFFER_HEADER_FLAGS_SNAPSHOT;
   if (flags & OMX_BUFFERFLAG_DATACORRUPT)
      mmal_flags |= MMAL_BUFFER_HEADER_FLAG_CORRUPTED;
   if (flags & OMX_BUFFERFLAG_DECODEONLY)
      mmal_flags |= MMAL_BUFFER_HEADER_FLAG_DECODEONLY;
   if (flags & OMX_BUFFERFLAG_INTERLACED)
      mmal_flags |= MMAL_BUFFER_HEADER_VIDEO_FLAG_INTERLACED;
   if (flags & OMX_BUFFERFLAG_TOP_FIELD_FIRST)
      mmal_flags |= MMAL_BUFFER_HEADER_VIDEO_FLAG_TOP_FIELD_FIRST;
   if (flags & OMX_BUFFERFLAG_ENDOFNAL)
      mmal_flags |= MMAL_BUFFER_HEADER_FLAG_NAL_END;

   if (flags & OMX_BUFFERFLAG_USR0)
      mmal_flags |= MMAL_BUFFER_HEADER_FLAG_USER0;
   if (flags & OMX_BUFFERFLAG_USR1)
      mmal_flags |= MMAL_BUFFER_HEADER_FLAG_USER1;
   if (flags & OMX_BUFFERFLAG_USR2)
      mmal_flags |= MMAL_BUFFER_HEADER_FLAG_USER2;
   if (flags & OMX_BUFFERFLAG_USR3)
      mmal_flags |= MMAL_BUFFER_HEADER_FLAG_USER3;

   return mmal_flags;
}

OMX_U32 mmalil_video_buffer_flags_to_omx(uint32_t flags)
{
   OMX_U32 omx_flags = 0;

   if (flags & MMAL_BUFFER_HEADER_VIDEO_FLAG_INTERLACED)
      omx_flags |= OMX_BUFFERFLAG_INTERLACED;
   if (flags & MMAL_BUFFER_HEADER_VIDEO_FLAG_TOP_FIELD_FIRST)
     omx_flags |= OMX_BUFFERFLAG_TOP_FIELD_FIRST;

  return omx_flags;
}

uint32_t mmalil_video_buffer_flags_to_mmal(OMX_U32 flags)
{
   uint32_t mmal_flags = 0;

   if (flags & OMX_BUFFERFLAG_INTERLACED)
      mmal_flags |= MMAL_BUFFER_HEADER_VIDEO_FLAG_INTERLACED;
   if (flags & OMX_BUFFERFLAG_TOP_FIELD_FIRST)
      mmal_flags |= MMAL_BUFFER_HEADER_VIDEO_FLAG_TOP_FIELD_FIRST;

   return mmal_flags;
}

/*****************************************************************************/
void mmalil_buffer_header_to_omx(OMX_BUFFERHEADERTYPE *omx, MMAL_BUFFER_HEADER_T *mmal)
{
   omx->pBuffer = mmal->data;
   omx->nAllocLen = mmal->alloc_size;
   omx->nFilledLen = mmal->length;
   omx->nOffset = mmal->offset;
   omx->nFlags = mmalil_buffer_flags_to_omx(mmal->flags);
   omx->nTimeStamp = omx_ticks_from_s64(mmal->pts);
   if (mmal->pts == MMAL_TIME_UNKNOWN)
   {
      if (mmal->dts == MMAL_TIME_UNKNOWN)
      {
         omx->nTimeStamp = omx_ticks_from_s64(0);
         omx->nFlags |= OMX_BUFFERFLAG_TIME_UNKNOWN;
      }
      else
      {
        omx->nTimeStamp = omx_ticks_from_s64(mmal->dts);
        omx->nFlags |= OMX_BUFFERFLAG_TIME_IS_DTS;
      }
   }
}

void mmalil_buffer_header_to_mmal(MMAL_BUFFER_HEADER_T *mmal, OMX_BUFFERHEADERTYPE *omx)
{
   mmal->cmd = 0;
   mmal->data = omx->pBuffer;
   mmal->alloc_size = omx->nAllocLen;
   mmal->length = omx->nFilledLen;
   mmal->offset = omx->nOffset;
   if (omx->nFlags & OMX_BUFFERFLAG_TIME_IS_DTS)
   {
     mmal->dts = omx_ticks_to_s64(omx->nTimeStamp);
     mmal->pts = MMAL_TIME_UNKNOWN;
   }
   else if (omx->nFlags & OMX_BUFFERFLAG_TIME_UNKNOWN)
   {
     mmal->dts = MMAL_TIME_UNKNOWN;
     mmal->pts = MMAL_TIME_UNKNOWN;
   }
   else
   {
     mmal->dts = MMAL_TIME_UNKNOWN;
     mmal->pts = omx_ticks_to_s64(omx->nTimeStamp);
   }
   mmal->flags = mmalil_buffer_flags_to_mmal(omx->nFlags);
}

/*****************************************************************************/
static struct {
   MMAL_ES_TYPE_T type;
   OMX_PORTDOMAINTYPE domain;
} mmal_omx_es_type_table[] =
{
   {MMAL_ES_TYPE_VIDEO,           OMX_PortDomainVideo},
   {MMAL_ES_TYPE_VIDEO,           OMX_PortDomainImage},
   {MMAL_ES_TYPE_AUDIO,           OMX_PortDomainAudio},
   {MMAL_ES_TYPE_UNKNOWN,         OMX_PortDomainMax}
};

OMX_PORTDOMAINTYPE mmalil_es_type_to_omx_domain(MMAL_ES_TYPE_T type)
{
   unsigned int i;
   for(i = 0; mmal_omx_es_type_table[i].type != MMAL_ES_TYPE_UNKNOWN; i++)
      if(mmal_omx_es_type_table[i].type == type) break;
   return mmal_omx_es_type_table[i].domain;
}

MMAL_ES_TYPE_T mmalil_omx_domain_to_es_type(OMX_PORTDOMAINTYPE domain)
{
   unsigned int i;
   for(i = 0; mmal_omx_es_type_table[i].type != MMAL_ES_TYPE_UNKNOWN; i++)
      if(mmal_omx_es_type_table[i].domain == domain) break;
   return mmal_omx_es_type_table[i].type;
}

/*****************************************************************************/
static struct {
   uint32_t encoding;
   OMX_AUDIO_CODINGTYPE coding;
} mmal_omx_audio_coding_table[] =
{
   {MMAL_ENCODING_MP4A,           OMX_AUDIO_CodingAAC},
   {MMAL_ENCODING_MPGA,           OMX_AUDIO_CodingMP3},
   {MMAL_ENCODING_WMA2,           OMX_AUDIO_CodingWMA},
   {MMAL_ENCODING_WMA1,           OMX_AUDIO_CodingWMA},
   {MMAL_ENCODING_AMRNB,          OMX_AUDIO_CodingAMR},
   {MMAL_ENCODING_AMRWB,          OMX_AUDIO_CodingAMR},
   {MMAL_ENCODING_AMRWBP,         OMX_AUDIO_CodingAMR},
   {MMAL_ENCODING_VORBIS,         OMX_AUDIO_CodingVORBIS},
   {MMAL_ENCODING_ALAW,           OMX_AUDIO_CodingPCM},
   {MMAL_ENCODING_MULAW,          OMX_AUDIO_CodingPCM},
   {MMAL_ENCODING_PCM_SIGNED_LE,  OMX_AUDIO_CodingPCM},
   {MMAL_ENCODING_PCM_UNSIGNED_LE,OMX_AUDIO_CodingPCM},
   {MMAL_ENCODING_PCM_SIGNED_BE,  OMX_AUDIO_CodingPCM},
   {MMAL_ENCODING_PCM_UNSIGNED_BE,OMX_AUDIO_CodingPCM},
   {MMAL_ENCODING_AC3,            OMX_AUDIO_CodingDDP},
   {MMAL_ENCODING_EAC3,           OMX_AUDIO_CodingDDP},
   {MMAL_ENCODING_DTS,            OMX_AUDIO_CodingDTS},
   {MMAL_ENCODING_UNKNOWN,        OMX_AUDIO_CodingUnused}
};

uint32_t mmalil_omx_audio_coding_to_encoding(OMX_AUDIO_CODINGTYPE coding)
{
   unsigned int i;
   for(i = 0; mmal_omx_audio_coding_table[i].encoding != MMAL_ENCODING_UNKNOWN; i++)
      if(mmal_omx_audio_coding_table[i].coding == coding) break;
   return mmal_omx_audio_coding_table[i].encoding;
}

OMX_AUDIO_CODINGTYPE mmalil_encoding_to_omx_audio_coding(uint32_t encoding)
{
   unsigned int i;
   for(i = 0; mmal_omx_audio_coding_table[i].encoding != MMAL_ENCODING_UNKNOWN; i++)
      if(mmal_omx_audio_coding_table[i].encoding == encoding) break;
   return mmal_omx_audio_coding_table[i].coding;
}

static struct {
   OMX_AUDIO_CODINGTYPE coding;
   OMX_INDEXTYPE index;
   unsigned int size;
} mmal_omx_audio_format_table[] =
{
   {OMX_AUDIO_CodingPCM, OMX_IndexParamAudioPcm, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE)},
   {OMX_AUDIO_CodingADPCM, OMX_IndexParamAudioAdpcm, sizeof(OMX_AUDIO_PARAM_ADPCMTYPE)},
   {OMX_AUDIO_CodingAMR, OMX_IndexParamAudioAmr, sizeof(OMX_AUDIO_PARAM_AMRTYPE)},
   {OMX_AUDIO_CodingGSMFR, OMX_IndexParamAudioGsm_FR, sizeof(OMX_AUDIO_PARAM_GSMFRTYPE)},
   {OMX_AUDIO_CodingGSMEFR, OMX_IndexParamAudioGsm_EFR, sizeof(OMX_AUDIO_PARAM_GSMEFRTYPE)},
   {OMX_AUDIO_CodingGSMHR, OMX_IndexParamAudioGsm_HR, sizeof(OMX_AUDIO_PARAM_GSMHRTYPE)},
   {OMX_AUDIO_CodingPDCFR, OMX_IndexParamAudioPdc_FR, sizeof(OMX_AUDIO_PARAM_PDCFRTYPE)},
   {OMX_AUDIO_CodingPDCEFR, OMX_IndexParamAudioPdc_EFR, sizeof(OMX_AUDIO_PARAM_PDCEFRTYPE)},
   {OMX_AUDIO_CodingPDCHR, OMX_IndexParamAudioPdc_HR, sizeof(OMX_AUDIO_PARAM_PDCHRTYPE)},
   {OMX_AUDIO_CodingTDMAFR, OMX_IndexParamAudioTdma_FR, sizeof(OMX_AUDIO_PARAM_TDMAFRTYPE)},
   {OMX_AUDIO_CodingTDMAEFR, OMX_IndexParamAudioTdma_EFR, sizeof(OMX_AUDIO_PARAM_TDMAEFRTYPE)},
   {OMX_AUDIO_CodingQCELP8, OMX_IndexParamAudioQcelp8, sizeof(OMX_AUDIO_PARAM_QCELP8TYPE)},
   {OMX_AUDIO_CodingQCELP13, OMX_IndexParamAudioQcelp13, sizeof(OMX_AUDIO_PARAM_QCELP13TYPE)},
   {OMX_AUDIO_CodingEVRC, OMX_IndexParamAudioEvrc, sizeof(OMX_AUDIO_PARAM_EVRCTYPE)},
   {OMX_AUDIO_CodingSMV, OMX_IndexParamAudioSmv, sizeof(OMX_AUDIO_PARAM_SMVTYPE)},
   {OMX_AUDIO_CodingG723, OMX_IndexParamAudioG723, sizeof(OMX_AUDIO_PARAM_G723TYPE)},
   {OMX_AUDIO_CodingG726, OMX_IndexParamAudioG726, sizeof(OMX_AUDIO_PARAM_G726TYPE)},
   {OMX_AUDIO_CodingG729, OMX_IndexParamAudioG729, sizeof(OMX_AUDIO_PARAM_G729TYPE)},
   {OMX_AUDIO_CodingAAC, OMX_IndexParamAudioAac, sizeof(OMX_AUDIO_PARAM_AACPROFILETYPE)},
   {OMX_AUDIO_CodingMP3, OMX_IndexParamAudioMp3, sizeof(OMX_AUDIO_PARAM_MP3TYPE)},
   {OMX_AUDIO_CodingSBC, OMX_IndexParamAudioSbc, sizeof(OMX_AUDIO_PARAM_SBCTYPE)},
   {OMX_AUDIO_CodingVORBIS, OMX_IndexParamAudioVorbis, sizeof(OMX_AUDIO_PARAM_VORBISTYPE)},
   {OMX_AUDIO_CodingWMA, OMX_IndexParamAudioWma, sizeof(OMX_AUDIO_PARAM_WMATYPE)},
   {OMX_AUDIO_CodingRA, OMX_IndexParamAudioRa, sizeof(OMX_AUDIO_PARAM_RATYPE)},
   {OMX_AUDIO_CodingMIDI, OMX_IndexParamAudioMidi, sizeof(OMX_AUDIO_PARAM_MIDITYPE)},
   {OMX_AUDIO_CodingDDP, OMX_IndexParamAudioDdp, sizeof(OMX_AUDIO_PARAM_DDPTYPE)},
   {OMX_AUDIO_CodingDTS, OMX_IndexParamAudioDts, sizeof(OMX_AUDIO_PARAM_DTSTYPE)},
   {OMX_AUDIO_CodingUnused, 0, 0}
};

OMX_AUDIO_CODINGTYPE mmalil_omx_audio_param_index_to_coding(OMX_INDEXTYPE index)
{
   unsigned int i;
   for(i = 0; mmal_omx_audio_format_table[i].coding != OMX_AUDIO_CodingUnused; i++)
      if(mmal_omx_audio_format_table[i].index == index) break;

   return mmal_omx_audio_format_table[i].coding;
}

OMX_INDEXTYPE mmalil_omx_audio_param_index(OMX_AUDIO_CODINGTYPE coding, OMX_U32 *size)
{
   unsigned int i;
   for(i = 0; mmal_omx_audio_format_table[i].coding != OMX_AUDIO_CodingUnused; i++)
      if(mmal_omx_audio_format_table[i].coding == coding) break;

   if(size) *size = mmal_omx_audio_format_table[i].size;
   return mmal_omx_audio_format_table[i].index;
}

MMAL_STATUS_T mmalil_omx_default_channel_mapping(OMX_AUDIO_CHANNELTYPE *channel_mapping, unsigned int nchannels)
{
   static const OMX_AUDIO_CHANNELTYPE default_mapping[][8] = {
      {OMX_AUDIO_ChannelNone},
      {OMX_AUDIO_ChannelCF},
      {OMX_AUDIO_ChannelLF, OMX_AUDIO_ChannelRF},
      {OMX_AUDIO_ChannelLF, OMX_AUDIO_ChannelRF, OMX_AUDIO_ChannelCF},
      {OMX_AUDIO_ChannelLF, OMX_AUDIO_ChannelRF, OMX_AUDIO_ChannelCF,
         OMX_AUDIO_ChannelCS},
      {OMX_AUDIO_ChannelLF, OMX_AUDIO_ChannelRF, OMX_AUDIO_ChannelCF,
         OMX_AUDIO_ChannelLR, OMX_AUDIO_ChannelRR},
      {OMX_AUDIO_ChannelLF, OMX_AUDIO_ChannelRF, OMX_AUDIO_ChannelCF,
         OMX_AUDIO_ChannelLFE, OMX_AUDIO_ChannelLR, OMX_AUDIO_ChannelRR},
      {OMX_AUDIO_ChannelLF, OMX_AUDIO_ChannelRF, OMX_AUDIO_ChannelCF,
         OMX_AUDIO_ChannelLFE, OMX_AUDIO_ChannelLR, OMX_AUDIO_ChannelRR,
         OMX_AUDIO_ChannelCS},
      {OMX_AUDIO_ChannelLF, OMX_AUDIO_ChannelRF, OMX_AUDIO_ChannelCF,
         OMX_AUDIO_ChannelLFE, OMX_AUDIO_ChannelLR, OMX_AUDIO_ChannelRR,
         OMX_AUDIO_ChannelLS, OMX_AUDIO_ChannelRS}
   };

   if (!nchannels || nchannels >= MMAL_COUNTOF(default_mapping))
      return MMAL_EINVAL;

   memcpy(channel_mapping, default_mapping[nchannels],
      sizeof(default_mapping[0][0]) * nchannels);
   return MMAL_SUCCESS;
}

MMAL_FOURCC_T mmalil_omx_audio_param_to_format(MMAL_ES_FORMAT_T *format,
   OMX_AUDIO_CODINGTYPE coding, OMX_FORMAT_PARAM_TYPE *param)
{
   MMAL_AUDIO_FORMAT_T *audio = &format->es->audio;
   format->encoding = mmalil_omx_audio_coding_to_encoding(coding);
   format->encoding_variant = 0;

   switch(coding)
   {
   case OMX_AUDIO_CodingPCM:
      audio->channels = param->pcm.nChannels;
      audio->sample_rate = param->pcm.nSamplingRate;
      audio->bits_per_sample = param->pcm.nBitPerSample;
      if(param->pcm.ePCMMode == OMX_AUDIO_PCMModeLinear && param->pcm.bInterleaved)
      {
         if(param->pcm.eEndian == OMX_EndianBig &&
            param->pcm.eNumData == OMX_NumericalDataSigned)
            format->encoding = MMAL_ENCODING_PCM_SIGNED_BE;
         else if(param->pcm.eEndian == OMX_EndianLittle &&
            param->pcm.eNumData == OMX_NumericalDataSigned)
            format->encoding = MMAL_ENCODING_PCM_SIGNED_LE;
         if(param->pcm.eEndian == OMX_EndianBig &&
            param->pcm.eNumData == OMX_NumericalDataUnsigned)
            format->encoding = MMAL_ENCODING_PCM_UNSIGNED_BE;
         if(param->pcm.eEndian == OMX_EndianLittle &&
            param->pcm.eNumData == OMX_NumericalDataUnsigned)
            format->encoding = MMAL_ENCODING_PCM_UNSIGNED_LE;
      }
      else if(param->pcm.ePCMMode == OMX_AUDIO_PCMModeALaw)
         format->encoding = MMAL_ENCODING_ALAW;
      else if(param->pcm.ePCMMode == OMX_AUDIO_PCMModeMULaw)
         format->encoding = MMAL_ENCODING_MULAW;
      break;
   case OMX_AUDIO_CodingAAC:
      audio->channels = param->aac.nChannels;
      audio->sample_rate = param->aac.nSampleRate;
      format->bitrate = param->aac.nBitRate;
      switch(param->aac.eAACStreamFormat)
      {
      case OMX_AUDIO_AACStreamFormatMP2ADTS:
      case OMX_AUDIO_AACStreamFormatMP4ADTS:
         format->encoding = MMAL_ENCODING_MP4A;
         format->encoding_variant = MMAL_ENCODING_VARIANT_MP4A_ADTS;
         break;
      case OMX_AUDIO_AACStreamFormatMP4FF:
      case OMX_AUDIO_AACStreamFormatRAW:
         format->encoding = MMAL_ENCODING_MP4A;
         format->encoding_variant = MMAL_ENCODING_VARIANT_MP4A_DEFAULT;
         break;
      default: break;
      }
      break;
   case OMX_AUDIO_CodingMP3:
      format->encoding = MMAL_ENCODING_MPGA;
      audio->channels = param->mp3.nChannels;
      audio->sample_rate = param->mp3.nSampleRate;
      format->bitrate = param->mp3.nBitRate;
      break;
   case OMX_AUDIO_CodingWMA:
      audio->channels = param->wma.nChannels;
      audio->sample_rate = param->wma.nSamplingRate;
      audio->block_align = param->wma.nBlockAlign;
      format->bitrate = param->wma.nBitRate;
      switch(param->wma.eFormat)
      {
      case OMX_AUDIO_WMAFormat7:
         format->encoding = MMAL_ENCODING_WMA1;
         break;
      case OMX_AUDIO_WMAFormat8:
      case OMX_AUDIO_WMAFormat9:
         format->encoding = MMAL_ENCODING_WMA2;
         break;
      default: break;
      }
      break;
   case OMX_AUDIO_CodingVORBIS:
      audio->channels = param->vorbis.nChannels;
      audio->sample_rate = param->vorbis.nSampleRate;
      format->bitrate = param->vorbis.nBitRate;
      break;
   case OMX_AUDIO_CodingAMR:
      audio->channels = param->amr.nChannels;
      audio->sample_rate = 8000;
      format->bitrate = param->amr.nBitRate;
      if(param->amr.eAMRBandMode >= OMX_AUDIO_AMRBandModeNB0 &&
         param->amr.eAMRBandMode <= OMX_AUDIO_AMRBandModeNB7)
         format->encoding = MMAL_ENCODING_AMRNB;
      if(param->amr.eAMRBandMode >= OMX_AUDIO_AMRBandModeWB0 &&
         param->amr.eAMRBandMode <= OMX_AUDIO_AMRBandModeWB8)
         format->encoding = MMAL_ENCODING_AMRWB;
      break;
   case OMX_AUDIO_CodingDDP:
      audio->channels = param->ddp.nChannels;
      audio->sample_rate = param->ddp.nSampleRate;
      if(param->ddp.eBitStreamId > OMX_AUDIO_DDPBitStreamIdAC3)
         format->encoding = MMAL_ENCODING_EAC3;
      break;
   case OMX_AUDIO_CodingDTS:
      audio->channels = param->dts.nChannels;
      audio->sample_rate = param->dts.nSampleRate;
      audio->block_align = param->dts.nDtsFrameSizeBytes;
      break;

   case OMX_AUDIO_CodingADPCM:
   case OMX_AUDIO_CodingGSMFR:
   case OMX_AUDIO_CodingGSMEFR:
   case OMX_AUDIO_CodingGSMHR:
   case OMX_AUDIO_CodingPDCFR:
   case OMX_AUDIO_CodingPDCEFR:
   case OMX_AUDIO_CodingPDCHR:
   case OMX_AUDIO_CodingTDMAFR:
   case OMX_AUDIO_CodingTDMAEFR:
   case OMX_AUDIO_CodingQCELP8:
   case OMX_AUDIO_CodingQCELP13:
   case OMX_AUDIO_CodingEVRC:
   case OMX_AUDIO_CodingSMV:
   case OMX_AUDIO_CodingG711:
   case OMX_AUDIO_CodingG723:
   case OMX_AUDIO_CodingG726:
   case OMX_AUDIO_CodingG729:
   case OMX_AUDIO_CodingSBC:
   case OMX_AUDIO_CodingRA:
   case OMX_AUDIO_CodingMIDI:
   default:
      vcos_assert(0);
      break;
   }

   return format->encoding;
}

OMX_AUDIO_CODINGTYPE mmalil_format_to_omx_audio_param(OMX_FORMAT_PARAM_TYPE *param,
   OMX_INDEXTYPE *param_index, MMAL_ES_FORMAT_T *format)
{
   MMAL_AUDIO_FORMAT_T *audio = &format->es->audio;
   OMX_AUDIO_CODINGTYPE coding = mmalil_encoding_to_omx_audio_coding(format->encoding);
   OMX_U32 size = 0;
   OMX_INDEXTYPE index = mmalil_omx_audio_param_index(coding, &size);

   if(param_index) *param_index = index;
   memset(param, 0, size);
   param->common.nSize = size;

   switch(coding)
   {
   case OMX_AUDIO_CodingPCM:
      param->pcm.nChannels = audio->channels;
      param->pcm.nSamplingRate = audio->sample_rate;
      param->pcm.nBitPerSample = audio->bits_per_sample;
      mmalil_omx_default_channel_mapping(param->pcm.eChannelMapping, audio->channels);
      if(format->encoding == MMAL_ENCODING_PCM_SIGNED_BE ||
         format->encoding == MMAL_ENCODING_PCM_SIGNED_LE ||
         format->encoding == MMAL_ENCODING_PCM_UNSIGNED_BE ||
         format->encoding == MMAL_ENCODING_PCM_UNSIGNED_LE)
      {
         param->pcm.ePCMMode = OMX_AUDIO_PCMModeLinear;
         param->pcm.bInterleaved = OMX_TRUE;
         param->pcm.eEndian = OMX_EndianLittle;
         param->pcm.eNumData = OMX_NumericalDataSigned;
         if(format->encoding == MMAL_ENCODING_PCM_SIGNED_BE ||
            format->encoding == MMAL_ENCODING_PCM_UNSIGNED_BE)
            param->pcm.eEndian = OMX_EndianBig;
         if(format->encoding == MMAL_ENCODING_PCM_UNSIGNED_LE ||
            format->encoding == MMAL_ENCODING_PCM_UNSIGNED_BE)
            param->pcm.eNumData = OMX_NumericalDataUnsigned;
      }
      else if(format->encoding == MMAL_ENCODING_ALAW)
         param->pcm.ePCMMode = OMX_AUDIO_PCMModeALaw;
      else if(format->encoding == MMAL_ENCODING_MULAW)
         param->pcm.ePCMMode = OMX_AUDIO_PCMModeMULaw;
      break;
   case OMX_AUDIO_CodingAAC:
      param->aac.nChannels = audio->channels;
      param->aac.nSampleRate = audio->sample_rate;
      param->aac.nBitRate = format->bitrate;
      switch(format->encoding_variant)
      {
      case MMAL_ENCODING_VARIANT_MP4A_ADTS:
         param->aac.eAACStreamFormat = OMX_AUDIO_AACStreamFormatMP4ADTS;
         break;
      case MMAL_ENCODING_VARIANT_MP4A_DEFAULT:
         param->aac.eAACStreamFormat = OMX_AUDIO_AACStreamFormatRAW;
         break;
      default: break;
      }
      break;
   case OMX_AUDIO_CodingMP3:
      param->mp3.nChannels = audio->channels;
      param->mp3.nSampleRate = audio->sample_rate;
      param->mp3.nBitRate = format->bitrate;
      break;
   case OMX_AUDIO_CodingWMA:
      param->wma.nChannels = audio->channels;
      param->wma.nSamplingRate = audio->sample_rate;
      param->wma.nBlockAlign = audio->block_align;
      param->wma.nBitRate = format->bitrate;
      switch(format->encoding)
      {
      case MMAL_ENCODING_WMA1:
         param->wma.eFormat = OMX_AUDIO_WMAFormat7;
         break;
      case MMAL_ENCODING_WMA2:
         param->wma.eFormat = OMX_AUDIO_WMAFormat8;
         break;
      default: break;
      }
      break;
   case OMX_AUDIO_CodingVORBIS:
      param->vorbis.nChannels = audio->channels;
      param->vorbis.nSampleRate = audio->sample_rate;
      param->vorbis.nBitRate = format->bitrate;
      break;
   case OMX_AUDIO_CodingAMR:
      param->amr.nChannels = audio->channels;
      param->amr.nBitRate = format->bitrate;
      if(format->encoding == MMAL_ENCODING_AMRNB)
         param->amr.eAMRBandMode = OMX_AUDIO_AMRBandModeNB0;
      if(format->encoding == MMAL_ENCODING_AMRWB)
         param->amr.eAMRBandMode = OMX_AUDIO_AMRBandModeWB0;
      break;
   case OMX_AUDIO_CodingDDP:
      param->ddp.nChannels = audio->channels;
      param->ddp.nSampleRate = audio->sample_rate;
      param->ddp.eBitStreamId = OMX_AUDIO_DDPBitStreamIdAC3;
      if(format->encoding == MMAL_ENCODING_EAC3)
         param->ddp.eBitStreamId = OMX_AUDIO_DDPBitStreamIdEAC3;
      param->ddp.eBitStreamMode = 0;
      param->ddp.eDolbySurroundMode = 0;
      mmalil_omx_default_channel_mapping(param->ddp.eChannelMapping, audio->channels);
      break;
   case OMX_AUDIO_CodingDTS:
      param->dts.nChannels = audio->channels;
      param->dts.nSampleRate = audio->sample_rate;
      param->dts.nDtsFrameSizeBytes = audio->block_align;
      param->dts.nDtsType = 1;
      param->dts.nFormat = 0;
      mmalil_omx_default_channel_mapping(param->dts.eChannelMapping, audio->channels);
      break;
   case OMX_AUDIO_CodingADPCM:
   case OMX_AUDIO_CodingGSMFR:
   case OMX_AUDIO_CodingGSMEFR:
   case OMX_AUDIO_CodingGSMHR:
   case OMX_AUDIO_CodingPDCFR:
   case OMX_AUDIO_CodingPDCEFR:
   case OMX_AUDIO_CodingPDCHR:
   case OMX_AUDIO_CodingTDMAFR:
   case OMX_AUDIO_CodingTDMAEFR:
   case OMX_AUDIO_CodingQCELP8:
   case OMX_AUDIO_CodingQCELP13:
   case OMX_AUDIO_CodingEVRC:
   case OMX_AUDIO_CodingSMV:
   case OMX_AUDIO_CodingG711:
   case OMX_AUDIO_CodingG723:
   case OMX_AUDIO_CodingG726:
   case OMX_AUDIO_CodingG729:
   case OMX_AUDIO_CodingSBC:
   case OMX_AUDIO_CodingRA:
   case OMX_AUDIO_CodingMIDI:
   default:
      vcos_assert(0);
      break;
   }

   return coding;
}

/*****************************************************************************/
static struct {
   uint32_t encoding;
   OMX_VIDEO_CODINGTYPE coding;
} mmal_omx_video_coding_table[] =
{
   {MMAL_ENCODING_H264,           OMX_VIDEO_CodingAVC},
   {MMAL_ENCODING_MVC,            OMX_VIDEO_CodingMVC},
   {MMAL_ENCODING_MP4V,           OMX_VIDEO_CodingMPEG4},
   {MMAL_ENCODING_MP2V,           OMX_VIDEO_CodingMPEG2},
   {MMAL_ENCODING_MP1V,           OMX_VIDEO_CodingMPEG2},
   {MMAL_ENCODING_H263,           OMX_VIDEO_CodingH263},
   {MMAL_ENCODING_WVC1,           OMX_VIDEO_CodingWMV},
   {MMAL_ENCODING_WMV3,           OMX_VIDEO_CodingWMV},
   {MMAL_ENCODING_WMV2,           OMX_VIDEO_CodingWMV},
   {MMAL_ENCODING_WMV1,           OMX_VIDEO_CodingWMV},
   {MMAL_ENCODING_VP6,            OMX_VIDEO_CodingVP6},
   {MMAL_ENCODING_VP7,            OMX_VIDEO_CodingVP7},
   {MMAL_ENCODING_VP8,            OMX_VIDEO_CodingVP8},
   {MMAL_ENCODING_SPARK,          OMX_VIDEO_CodingSorenson},
   {MMAL_ENCODING_THEORA,         OMX_VIDEO_CodingTheora},
   {MMAL_ENCODING_MJPEG,          OMX_VIDEO_CodingMJPEG},
   {MMAL_ENCODING_UNKNOWN,        OMX_VIDEO_CodingUnused}
};

uint32_t mmalil_omx_video_coding_to_encoding(OMX_VIDEO_CODINGTYPE coding)
{
   unsigned int i;
   for(i = 0; mmal_omx_video_coding_table[i].encoding != MMAL_ENCODING_UNKNOWN; i++)
      if(mmal_omx_video_coding_table[i].coding == coding) break;
   return mmal_omx_video_coding_table[i].encoding;
}

OMX_VIDEO_CODINGTYPE mmalil_encoding_to_omx_video_coding(uint32_t encoding)
{
   unsigned int i;
   for(i = 0; mmal_omx_video_coding_table[i].encoding != MMAL_ENCODING_UNKNOWN; i++)
      if(mmal_omx_video_coding_table[i].encoding == encoding) break;
   return mmal_omx_video_coding_table[i].coding;
}

/*****************************************************************************/
static struct {
   uint32_t encoding;
   OMX_IMAGE_CODINGTYPE coding;
} mmal_omx_image_coding_table[] =
{
   {MMAL_ENCODING_JPEG,           OMX_IMAGE_CodingJPEG},
   {MMAL_ENCODING_GIF,            OMX_IMAGE_CodingGIF},
   {MMAL_ENCODING_PNG,            OMX_IMAGE_CodingPNG},
   {MMAL_ENCODING_BMP,            OMX_IMAGE_CodingBMP},
   {MMAL_ENCODING_TGA,            OMX_IMAGE_CodingTGA},
   {MMAL_ENCODING_PPM,            OMX_IMAGE_CodingPPM},
   {MMAL_ENCODING_UNKNOWN,        OMX_IMAGE_CodingUnused}
};

uint32_t mmalil_omx_image_coding_to_encoding(OMX_IMAGE_CODINGTYPE coding)
{
   unsigned int i;
   for(i = 0; mmal_omx_image_coding_table[i].encoding != MMAL_ENCODING_UNKNOWN; i++)
      if(mmal_omx_image_coding_table[i].coding == coding) break;
   return mmal_omx_image_coding_table[i].encoding;
}

OMX_IMAGE_CODINGTYPE mmalil_encoding_to_omx_image_coding(uint32_t encoding)
{
   unsigned int i;
   for(i = 0; mmal_omx_image_coding_table[i].encoding != MMAL_ENCODING_UNKNOWN; i++)
      if(mmal_omx_image_coding_table[i].encoding == encoding) break;
   return mmal_omx_image_coding_table[i].coding;
}

uint32_t mmalil_omx_coding_to_encoding(uint32_t encoding, OMX_PORTDOMAINTYPE domain)
{
   if(domain == OMX_PortDomainVideo)
      return mmalil_omx_video_coding_to_encoding((OMX_VIDEO_CODINGTYPE)encoding);
   else if(domain == OMX_PortDomainAudio)
      return mmalil_omx_audio_coding_to_encoding((OMX_AUDIO_CODINGTYPE)encoding);
   else if(domain == OMX_PortDomainImage)
      return mmalil_omx_image_coding_to_encoding((OMX_IMAGE_CODINGTYPE)encoding);
   else
      return MMAL_ENCODING_UNKNOWN;
}

/*****************************************************************************/
static struct {
   uint32_t encoding;
   OMX_COLOR_FORMATTYPE coding;
} mmal_omx_colorformat_coding_table[] =
{
   {MMAL_ENCODING_I420,           OMX_COLOR_FormatYUV420PackedPlanar},
   {MMAL_ENCODING_I422,           OMX_COLOR_FormatYUV422PackedPlanar},
   {MMAL_ENCODING_I420_SLICE,     OMX_COLOR_FormatYUV420PackedPlanar},
   {MMAL_ENCODING_I422_SLICE,     OMX_COLOR_FormatYUV422PackedPlanar},
   {MMAL_ENCODING_I420,           OMX_COLOR_FormatYUV420Planar},
   {MMAL_ENCODING_YV12,           OMX_COLOR_FormatYVU420PackedPlanar},
   {MMAL_ENCODING_NV12,           OMX_COLOR_FormatYUV420PackedSemiPlanar},
   {MMAL_ENCODING_NV12,           OMX_COLOR_FormatYUV420SemiPlanar},
   {MMAL_ENCODING_NV21,           OMX_COLOR_FormatYVU420PackedSemiPlanar},
   {MMAL_ENCODING_YUVUV128,       OMX_COLOR_FormatYUVUV128},
   {MMAL_ENCODING_YUYV,           OMX_COLOR_FormatYCbYCr},
   {MMAL_ENCODING_YVYU,           OMX_COLOR_FormatYCrYCb},
   {MMAL_ENCODING_UYVY,           OMX_COLOR_FormatCbYCrY},
   {MMAL_ENCODING_VYUY,           OMX_COLOR_FormatCrYCbY},
   {MMAL_ENCODING_RGB16,          OMX_COLOR_Format16bitRGB565},
   {MMAL_ENCODING_BGR24,          OMX_COLOR_Format24bitRGB888},
   {MMAL_ENCODING_BGRA,           OMX_COLOR_Format32bitARGB8888},
   {MMAL_ENCODING_BGR16,          OMX_COLOR_Format16bitBGR565},
   {MMAL_ENCODING_RGB24,          OMX_COLOR_Format24bitBGR888},
   {MMAL_ENCODING_ARGB,           OMX_COLOR_Format32bitBGRA8888},
   {MMAL_ENCODING_RGBA,           OMX_COLOR_Format32bitABGR8888},
   {MMAL_ENCODING_RGB16_SLICE,    OMX_COLOR_Format16bitRGB565},
   {MMAL_ENCODING_BGR24_SLICE,    OMX_COLOR_Format24bitRGB888},
   {MMAL_ENCODING_BGRA_SLICE,     OMX_COLOR_Format32bitARGB8888},
   {MMAL_ENCODING_BGR16_SLICE,    OMX_COLOR_Format16bitBGR565},
   {MMAL_ENCODING_RGB24_SLICE,    OMX_COLOR_Format24bitBGR888},
   {MMAL_ENCODING_ARGB_SLICE,     OMX_COLOR_Format32bitBGRA8888},
   {MMAL_ENCODING_RGBA_SLICE,     OMX_COLOR_Format32bitABGR8888},
   {MMAL_ENCODING_EGL_IMAGE,      OMX_COLOR_FormatBRCMEGL},
   {MMAL_ENCODING_BAYER_SBGGR8,   OMX_COLOR_FormatRawBayer8bit},
   {MMAL_ENCODING_BAYER_SGRBG8,   OMX_COLOR_FormatRawBayer8bit},
   {MMAL_ENCODING_BAYER_SGBRG8,   OMX_COLOR_FormatRawBayer8bit},
   {MMAL_ENCODING_BAYER_SRGGB8,   OMX_COLOR_FormatRawBayer8bit},
   {MMAL_ENCODING_BAYER_SBGGR10P, OMX_COLOR_FormatRawBayer10bit},
   {MMAL_ENCODING_BAYER_SGRBG10P, OMX_COLOR_FormatRawBayer10bit},
   {MMAL_ENCODING_BAYER_SGBRG10P, OMX_COLOR_FormatRawBayer10bit},
   {MMAL_ENCODING_BAYER_SRGGB10P, OMX_COLOR_FormatRawBayer10bit},
   {MMAL_ENCODING_BAYER_SBGGR12P, OMX_COLOR_FormatRawBayer12bit},
   {MMAL_ENCODING_BAYER_SGRBG12P, OMX_COLOR_FormatRawBayer12bit},
   {MMAL_ENCODING_BAYER_SGBRG12P, OMX_COLOR_FormatRawBayer12bit},
   {MMAL_ENCODING_BAYER_SRGGB12P, OMX_COLOR_FormatRawBayer12bit},
   {MMAL_ENCODING_BAYER_SBGGR16,  OMX_COLOR_FormatRawBayer16bit},
   {MMAL_ENCODING_BAYER_SGBRG16,  OMX_COLOR_FormatRawBayer16bit},
   {MMAL_ENCODING_BAYER_SGRBG16,  OMX_COLOR_FormatRawBayer16bit},
   {MMAL_ENCODING_BAYER_SRGGB16,  OMX_COLOR_FormatRawBayer16bit},
   {MMAL_ENCODING_BAYER_SBGGR10DPCM8,OMX_COLOR_FormatRawBayer8bitcompressed},
   {MMAL_ENCODING_OPAQUE,         OMX_COLOR_FormatBRCMOpaque},
   {MMAL_ENCODING_I420_16,        OMX_COLOR_FormatYUV420_16PackedPlanar},
   {MMAL_ENCODING_I420_S,         OMX_COLOR_FormatYUV420_UVSideBySide},
   {MMAL_ENCODING_YUVUV64_16,     OMX_COLOR_FormatYUVUV64_16},
   {MMAL_ENCODING_I420_10,        OMX_COLOR_FormatYUV420_10PackedPlanar},
   {MMAL_ENCODING_YUVUV64_10,     OMX_COLOR_FormatYUVUV64_10},
   {MMAL_ENCODING_UNKNOWN,        OMX_COLOR_FormatUnused}
};

uint32_t mmalil_omx_color_format_to_encoding(OMX_COLOR_FORMATTYPE coding)
{
   unsigned int i;
   for(i = 0; mmal_omx_colorformat_coding_table[i].encoding != MMAL_ENCODING_UNKNOWN; i++)
      if(mmal_omx_colorformat_coding_table[i].coding == coding) break;
   return mmal_omx_colorformat_coding_table[i].encoding;
}

OMX_COLOR_FORMATTYPE mmalil_encoding_to_omx_color_format(uint32_t encoding)
{
   unsigned int i;
   for(i = 0; mmal_omx_colorformat_coding_table[i].encoding != MMAL_ENCODING_UNKNOWN; i++)
      if(mmal_omx_colorformat_coding_table[i].encoding == encoding) break;
   return mmal_omx_colorformat_coding_table[i].coding;
}

static struct {
   uint32_t encoding;
   OMX_COLOR_FORMATTYPE color_format;
   OMX_BAYERORDERTYPE bayer_order;
} mmal_omx_bayer_order_coding_table[] =
{
   //Colour format required for conversion from OMX to MMAL.
   //Not used for MMAL encoding to OMX color format.
   {MMAL_ENCODING_BAYER_SBGGR8, OMX_COLOR_FormatRawBayer8bit, OMX_BayerOrderBGGR},
   {MMAL_ENCODING_BAYER_SGBRG8, OMX_COLOR_FormatRawBayer8bit, OMX_BayerOrderGBRG},
   {MMAL_ENCODING_BAYER_SGRBG8, OMX_COLOR_FormatRawBayer8bit, OMX_BayerOrderGRBG},
   {MMAL_ENCODING_BAYER_SRGGB8, OMX_COLOR_FormatRawBayer8bit, OMX_BayerOrderRGGB},

   {MMAL_ENCODING_BAYER_SBGGR10P, OMX_COLOR_FormatRawBayer10bit, OMX_BayerOrderBGGR},
   {MMAL_ENCODING_BAYER_SGRBG10P, OMX_COLOR_FormatRawBayer10bit, OMX_BayerOrderGRBG},
   {MMAL_ENCODING_BAYER_SGBRG10P, OMX_COLOR_FormatRawBayer10bit, OMX_BayerOrderGBRG},
   {MMAL_ENCODING_BAYER_SRGGB10P, OMX_COLOR_FormatRawBayer10bit, OMX_BayerOrderRGGB},

   {MMAL_ENCODING_BAYER_SBGGR12P, OMX_COLOR_FormatRawBayer12bit, OMX_BayerOrderBGGR},
   {MMAL_ENCODING_BAYER_SGRBG12P, OMX_COLOR_FormatRawBayer12bit, OMX_BayerOrderGRBG},
   {MMAL_ENCODING_BAYER_SGBRG12P, OMX_COLOR_FormatRawBayer12bit, OMX_BayerOrderGBRG},
   {MMAL_ENCODING_BAYER_SRGGB12P, OMX_COLOR_FormatRawBayer12bit, OMX_BayerOrderRGGB},

   {MMAL_ENCODING_BAYER_SBGGR16,  OMX_COLOR_FormatRawBayer16bit, OMX_BayerOrderBGGR},
   {MMAL_ENCODING_BAYER_SGRBG16,  OMX_COLOR_FormatRawBayer16bit, OMX_BayerOrderGRBG},
   {MMAL_ENCODING_BAYER_SGBRG16,  OMX_COLOR_FormatRawBayer16bit, OMX_BayerOrderGBRG},
   {MMAL_ENCODING_BAYER_SRGGB16,  OMX_COLOR_FormatRawBayer16bit, OMX_BayerOrderRGGB},

   {MMAL_ENCODING_BAYER_SBGGR10DPCM8,OMX_COLOR_FormatRawBayer8bitcompressed, OMX_BayerOrderBGGR},
   {MMAL_ENCODING_BAYER_SGRBG10DPCM8,OMX_COLOR_FormatRawBayer8bitcompressed, OMX_BayerOrderGRBG},
   {MMAL_ENCODING_BAYER_SGBRG10DPCM8,OMX_COLOR_FormatRawBayer8bitcompressed, OMX_BayerOrderGBRG},
   {MMAL_ENCODING_BAYER_SRGGB10DPCM8,OMX_COLOR_FormatRawBayer8bitcompressed, OMX_BayerOrderRGGB},

   {MMAL_ENCODING_UNKNOWN,        OMX_COLOR_FormatMax,            OMX_BayerOrderMax}
};

uint32_t mmalil_omx_bayer_format_order_to_encoding(OMX_BAYERORDERTYPE bayer_order, OMX_COLOR_FORMATTYPE color_format)
{
   unsigned int i;
   for(i = 0; mmal_omx_bayer_order_coding_table[i].encoding != MMAL_ENCODING_UNKNOWN; i++)
      if(mmal_omx_bayer_order_coding_table[i].bayer_order == bayer_order &&
         mmal_omx_bayer_order_coding_table[i].color_format == color_format)
         break;
   return mmal_omx_bayer_order_coding_table[i].encoding;
}

OMX_BAYERORDERTYPE mmalil_encoding_to_omx_bayer_order(uint32_t encoding)
{
   unsigned int i;
   for(i = 0; mmal_omx_bayer_order_coding_table[i].encoding != MMAL_ENCODING_UNKNOWN; i++)
      if(mmal_omx_bayer_order_coding_table[i].encoding == encoding) break;
   return mmal_omx_bayer_order_coding_table[i].bayer_order;
}

/*****************************************************************************/
static struct {
   uint32_t mmal;
   OMX_COLORSPACETYPE omx;
} mmal_omx_colorspace_coding_table[] =
{
   {MMAL_COLOR_SPACE_ITUR_BT601,    OMX_COLORSPACE_ITU_R_BT601},
   {MMAL_COLOR_SPACE_ITUR_BT709,    OMX_COLORSPACE_ITU_R_BT709},
   {MMAL_COLOR_SPACE_JPEG_JFIF,     OMX_COLORSPACE_JPEG_JFIF},
   {MMAL_COLOR_SPACE_FCC,           OMX_COLORSPACE_FCC},
   {MMAL_COLOR_SPACE_SMPTE240M,     OMX_COLORSPACE_SMPTE240M},
   {MMAL_COLOR_SPACE_BT470_2_M,     OMX_COLORSPACE_BT470_2_M},
   {MMAL_COLOR_SPACE_BT470_2_BG,    OMX_COLORSPACE_BT470_2_BG},
   {MMAL_COLOR_SPACE_JFIF_Y16_255,  OMX_COLORSPACE_JFIF_Y16_255},
   {MMAL_COLOR_SPACE_UNKNOWN,       OMX_COLORSPACE_UNKNOWN}
};

uint32_t mmalil_omx_color_space_to_mmal(OMX_COLORSPACETYPE coding)
{
   unsigned int i;
   for(i = 0; mmal_omx_colorspace_coding_table[i].mmal != MMAL_COLOR_SPACE_UNKNOWN; i++)
      if(mmal_omx_colorspace_coding_table[i].omx == coding) break;
   return mmal_omx_colorspace_coding_table[i].mmal;
}

OMX_COLORSPACETYPE mmalil_color_space_to_omx(uint32_t coding)
{
   unsigned int i;
   for(i = 0; mmal_omx_colorspace_coding_table[i].mmal != MMAL_COLOR_SPACE_UNKNOWN; i++)
      if(mmal_omx_colorspace_coding_table[i].mmal == coding) break;
   return mmal_omx_colorspace_coding_table[i].omx;
}

/*****************************************************************************/
static struct {
   uint32_t mmal;
   OMX_U32 omx;
   OMX_VIDEO_CODINGTYPE omx_coding;
} mmal_omx_video_profile_table[] =
{
   { MMAL_VIDEO_PROFILE_H263_BASELINE,           OMX_VIDEO_H263ProfileBaseline,           OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_PROFILE_H263_H320CODING,         OMX_VIDEO_H263ProfileH320Coding,         OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_PROFILE_H263_BACKWARDCOMPATIBLE, OMX_VIDEO_H263ProfileBackwardCompatible, OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_PROFILE_H263_ISWV2,              OMX_VIDEO_H263ProfileISWV2,              OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_PROFILE_H263_ISWV3,              OMX_VIDEO_H263ProfileISWV3,              OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_PROFILE_H263_HIGHCOMPRESSION,    OMX_VIDEO_H263ProfileHighCompression,    OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_PROFILE_H263_INTERNET,           OMX_VIDEO_H263ProfileInternet,           OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_PROFILE_H263_INTERLACE,          OMX_VIDEO_H263ProfileInterlace,          OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_PROFILE_H263_HIGHLATENCY,        OMX_VIDEO_H263ProfileHighLatency,        OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_PROFILE_MP4V_SIMPLE,             OMX_VIDEO_MPEG4ProfileSimple,            OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_MP4V_SIMPLESCALABLE,     OMX_VIDEO_MPEG4ProfileSimpleScalable,    OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_MP4V_CORE,               OMX_VIDEO_MPEG4ProfileCore,              OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_MP4V_MAIN,               OMX_VIDEO_MPEG4ProfileMain,              OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_MP4V_NBIT,               OMX_VIDEO_MPEG4ProfileNbit,              OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_MP4V_SCALABLETEXTURE,    OMX_VIDEO_MPEG4ProfileScalableTexture,   OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_MP4V_SIMPLEFACE,         OMX_VIDEO_MPEG4ProfileSimpleFace,        OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_MP4V_SIMPLEFBA,          OMX_VIDEO_MPEG4ProfileSimpleFBA,         OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_MP4V_BASICANIMATED,      OMX_VIDEO_MPEG4ProfileBasicAnimated,     OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_MP4V_HYBRID,             OMX_VIDEO_MPEG4ProfileHybrid,            OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_MP4V_ADVANCEDREALTIME,   OMX_VIDEO_MPEG4ProfileAdvancedRealTime,  OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_MP4V_CORESCALABLE,       OMX_VIDEO_MPEG4ProfileCoreScalable,      OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_MP4V_ADVANCEDCODING,     OMX_VIDEO_MPEG4ProfileAdvancedCoding,    OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_MP4V_ADVANCEDCORE,       OMX_VIDEO_MPEG4ProfileAdvancedCore,      OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_MP4V_ADVANCEDSCALABLE,   OMX_VIDEO_MPEG4ProfileAdvancedScalable,  OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_MP4V_ADVANCEDSIMPLE,     OMX_VIDEO_MPEG4ProfileAdvancedSimple,    OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_H264_BASELINE,           OMX_VIDEO_AVCProfileBaseline,            OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_PROFILE_H264_MAIN,               OMX_VIDEO_AVCProfileMain,                OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_PROFILE_H264_EXTENDED,           OMX_VIDEO_AVCProfileExtended,            OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_PROFILE_H264_HIGH,               OMX_VIDEO_AVCProfileHigh,                OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_PROFILE_H264_HIGH10,             OMX_VIDEO_AVCProfileHigh10,              OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_PROFILE_H264_HIGH422,            OMX_VIDEO_AVCProfileHigh422,             OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_PROFILE_H264_HIGH444,            OMX_VIDEO_AVCProfileHigh444,             OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_PROFILE_H264_CONSTRAINED_BASELINE, OMX_VIDEO_AVCProfileConstrainedBaseline,             OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_PROFILE_DUMMY,                   OMX_VIDEO_AVCProfileMax,                 OMX_VIDEO_CodingAVC},
};

uint32_t mmalil_omx_video_profile_to_mmal(OMX_U32 profile, OMX_VIDEO_CODINGTYPE coding)
{
   unsigned int i;
   for(i = 0; mmal_omx_video_profile_table[i].mmal != MMAL_VIDEO_PROFILE_DUMMY; i++)
      if(mmal_omx_video_profile_table[i].omx == profile
         && mmal_omx_video_profile_table[i].omx_coding == coding) break;
   return mmal_omx_video_profile_table[i].mmal;
}

OMX_U32 mmalil_video_profile_to_omx(uint32_t profile)
{
   unsigned int i;
   for(i = 0; mmal_omx_video_profile_table[i].mmal != MMAL_VIDEO_PROFILE_DUMMY; i++)
      if(mmal_omx_video_profile_table[i].mmal == profile) break;
   return mmal_omx_video_profile_table[i].omx;
}

/*****************************************************************************/
static struct {
   uint32_t mmal;
   OMX_U32 omx;
   OMX_VIDEO_CODINGTYPE omx_coding;
} mmal_omx_video_level_table[] =
{
   { MMAL_VIDEO_LEVEL_H263_10, OMX_VIDEO_H263Level10,  OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_LEVEL_H263_20, OMX_VIDEO_H263Level20,  OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_LEVEL_H263_30, OMX_VIDEO_H263Level30,  OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_LEVEL_H263_40, OMX_VIDEO_H263Level40,  OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_LEVEL_H263_45, OMX_VIDEO_H263Level45,  OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_LEVEL_H263_50, OMX_VIDEO_H263Level50,  OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_LEVEL_H263_60, OMX_VIDEO_H263Level60,  OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_LEVEL_H263_70, OMX_VIDEO_H263Level70,  OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_LEVEL_MP4V_0,  OMX_VIDEO_MPEG4Level0,  OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_LEVEL_MP4V_0b, OMX_VIDEO_MPEG4Level0b, OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_LEVEL_MP4V_1,  OMX_VIDEO_MPEG4Level1,  OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_LEVEL_MP4V_2,  OMX_VIDEO_MPEG4Level2,  OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_LEVEL_MP4V_3,  OMX_VIDEO_MPEG4Level3,  OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_LEVEL_MP4V_4,  OMX_VIDEO_MPEG4Level4,  OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_LEVEL_MP4V_4a, OMX_VIDEO_MPEG4Level4a, OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_LEVEL_MP4V_5,  OMX_VIDEO_MPEG4Level5,  OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_LEVEL_MP4V_6,  OMX_VIDEO_MPEG4Level6,  OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_LEVEL_H264_1,  OMX_VIDEO_AVCLevel1,    OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_H264_1b, OMX_VIDEO_AVCLevel1b,   OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_H264_11, OMX_VIDEO_AVCLevel11,   OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_H264_12, OMX_VIDEO_AVCLevel12,   OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_H264_13, OMX_VIDEO_AVCLevel13,   OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_H264_2,  OMX_VIDEO_AVCLevel2,    OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_H264_21, OMX_VIDEO_AVCLevel21,   OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_H264_22, OMX_VIDEO_AVCLevel22,   OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_H264_3,  OMX_VIDEO_AVCLevel3,    OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_H264_31, OMX_VIDEO_AVCLevel31,   OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_H264_32, OMX_VIDEO_AVCLevel32,   OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_H264_4,  OMX_VIDEO_AVCLevel4,    OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_H264_41, OMX_VIDEO_AVCLevel41,   OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_H264_42, OMX_VIDEO_AVCLevel42,   OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_H264_5,  OMX_VIDEO_AVCLevel5,    OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_H264_51, OMX_VIDEO_AVCLevel51,   OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_DUMMY,   OMX_VIDEO_AVCLevelMax,  OMX_VIDEO_CodingMax},
};

uint32_t mmalil_omx_video_level_to_mmal(OMX_U32 level, OMX_VIDEO_CODINGTYPE coding)
{
   unsigned int i;
   for(i = 0; mmal_omx_video_level_table[i].mmal != MMAL_VIDEO_LEVEL_DUMMY; i++)
      if(mmal_omx_video_level_table[i].omx == level
         && mmal_omx_video_level_table[i].omx_coding == coding) break;
   return mmal_omx_video_level_table[i].mmal;
}

OMX_U32 mmalil_video_level_to_omx(uint32_t level)
{
   unsigned int i;
   for(i = 0; mmal_omx_video_level_table[i].mmal != MMAL_VIDEO_LEVEL_DUMMY; i++)
      if(mmal_omx_video_level_table[i].mmal == level) break;
   return mmal_omx_video_level_table[i].omx;
}

/*****************************************************************************/
static struct {
   MMAL_VIDEO_RATECONTROL_T mmal;
   OMX_VIDEO_CONTROLRATETYPE omx;
} mmal_omx_video_ratecontrol_table[] =
{
   { MMAL_VIDEO_RATECONTROL_DEFAULT,              OMX_Video_ControlRateDisable},
   { MMAL_VIDEO_RATECONTROL_VARIABLE,             OMX_Video_ControlRateVariable},
   { MMAL_VIDEO_RATECONTROL_CONSTANT,             OMX_Video_ControlRateConstant},
   { MMAL_VIDEO_RATECONTROL_VARIABLE_SKIP_FRAMES, OMX_Video_ControlRateVariableSkipFrames},
   { MMAL_VIDEO_RATECONTROL_CONSTANT_SKIP_FRAMES, OMX_Video_ControlRateConstantSkipFrames},
   { MMAL_VIDEO_RATECONTROL_DUMMY,                OMX_Video_ControlRateMax},
};

MMAL_VIDEO_RATECONTROL_T mmalil_omx_video_ratecontrol_to_mmal(OMX_VIDEO_CONTROLRATETYPE omx)
{
   unsigned int i;
   for(i = 0; mmal_omx_video_ratecontrol_table[i].mmal != MMAL_VIDEO_RATECONTROL_DUMMY; i++)
      if(mmal_omx_video_ratecontrol_table[i].omx == omx) break;
   return mmal_omx_video_ratecontrol_table[i].mmal;
}

OMX_VIDEO_CONTROLRATETYPE mmalil_video_ratecontrol_to_omx(MMAL_VIDEO_RATECONTROL_T mmal)
{
   unsigned int i;
   for(i = 0; mmal_omx_video_ratecontrol_table[i].mmal != MMAL_VIDEO_RATECONTROL_DUMMY; i++)
      if(mmal_omx_video_ratecontrol_table[i].mmal == mmal) break;
   return mmal_omx_video_ratecontrol_table[i].omx;
}

/*****************************************************************************/
static struct {
   MMAL_VIDEO_INTRA_REFRESH_T mmal;
   OMX_VIDEO_INTRAREFRESHTYPE omx;
} mmal_omx_video_intrarefresh_table[] =
{
   { MMAL_VIDEO_INTRA_REFRESH_CYCLIC,             OMX_VIDEO_IntraRefreshCyclic},
   { MMAL_VIDEO_INTRA_REFRESH_ADAPTIVE,           OMX_VIDEO_IntraRefreshAdaptive},
   { MMAL_VIDEO_INTRA_REFRESH_BOTH,               OMX_VIDEO_IntraRefreshBoth},
   { MMAL_VIDEO_INTRA_REFRESH_KHRONOSEXTENSIONS,  OMX_VIDEO_IntraRefreshKhronosExtensions},
   { MMAL_VIDEO_INTRA_REFRESH_VENDORSTARTUNUSED,  OMX_VIDEO_IntraRefreshVendorStartUnused},
   { MMAL_VIDEO_INTRA_REFRESH_DUMMY,              OMX_VIDEO_IntraRefreshMax},
};

MMAL_VIDEO_INTRA_REFRESH_T mmalil_omx_video_intrarefresh_to_mmal(OMX_VIDEO_INTRAREFRESHTYPE omx)
{
   unsigned int i;
   for(i = 0; mmal_omx_video_intrarefresh_table[i].mmal != MMAL_VIDEO_INTRA_REFRESH_DUMMY; i++)
      if(mmal_omx_video_intrarefresh_table[i].omx == omx) break;
   return mmal_omx_video_intrarefresh_table[i].mmal;
}
