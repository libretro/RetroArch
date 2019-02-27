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
#ifndef MP4_COMMON_H
#define MP4_COMMON_H

/******************************************************************************
Type definitions.
******************************************************************************/
typedef enum {
   MP4_BOX_TYPE_UNKNOWN           = 0,
   MP4_BOX_TYPE_ROOT              = VC_FOURCC('r','o','o','t'),
   MP4_BOX_TYPE_FTYP              = VC_FOURCC('f','t','y','p'),
   MP4_BOX_TYPE_MDAT              = VC_FOURCC('m','d','a','t'),
   MP4_BOX_TYPE_MOOV              = VC_FOURCC('m','o','o','v'),
   MP4_BOX_TYPE_MVHD              = VC_FOURCC('m','v','h','d'),
   MP4_BOX_TYPE_TRAK              = VC_FOURCC('t','r','a','k'),
   MP4_BOX_TYPE_TKHD              = VC_FOURCC('t','k','h','d'),
   MP4_BOX_TYPE_MDIA              = VC_FOURCC('m','d','i','a'),
   MP4_BOX_TYPE_MDHD              = VC_FOURCC('m','d','h','d'),
   MP4_BOX_TYPE_HDLR              = VC_FOURCC('h','d','l','r'),
   MP4_BOX_TYPE_MINF              = VC_FOURCC('m','i','n','f'),
   MP4_BOX_TYPE_VMHD              = VC_FOURCC('v','m','h','d'),
   MP4_BOX_TYPE_SMHD              = VC_FOURCC('s','m','h','d'),
   MP4_BOX_TYPE_DINF              = VC_FOURCC('d','i','n','f'),
   MP4_BOX_TYPE_DREF              = VC_FOURCC('d','r','e','f'),
   MP4_BOX_TYPE_STBL              = VC_FOURCC('s','t','b','l'),
   MP4_BOX_TYPE_STSD              = VC_FOURCC('s','t','s','d'),
   MP4_BOX_TYPE_STTS              = VC_FOURCC('s','t','t','s'),
   MP4_BOX_TYPE_CTTS              = VC_FOURCC('c','t','t','s'),
   MP4_BOX_TYPE_STSC              = VC_FOURCC('s','t','s','c'),
   MP4_BOX_TYPE_STSZ              = VC_FOURCC('s','t','s','z'),
   MP4_BOX_TYPE_STCO              = VC_FOURCC('s','t','c','o'),
   MP4_BOX_TYPE_CO64              = VC_FOURCC('c','o','6','4'),
   MP4_BOX_TYPE_STSS              = VC_FOURCC('s','t','s','s'),
   MP4_BOX_TYPE_VIDE              = VC_FOURCC('v','i','d','e'),
   MP4_BOX_TYPE_SOUN              = VC_FOURCC('s','o','u','n'),
   MP4_BOX_TYPE_TEXT              = VC_FOURCC('t','e','x','t'),
   MP4_BOX_TYPE_FREE              = VC_FOURCC('f','r','e','e'),
   MP4_BOX_TYPE_SKIP              = VC_FOURCC('s','k','i','p'),
   MP4_BOX_TYPE_WIDE              = VC_FOURCC('w','i','d','e'),
   MP4_BOX_TYPE_PNOT              = VC_FOURCC('p','m','o','t'),
   MP4_BOX_TYPE_PICT              = VC_FOURCC('P','I','C','T'),
   MP4_BOX_TYPE_UDTA              = VC_FOURCC('u','d','t','a'),
   MP4_BOX_TYPE_UUID              = VC_FOURCC('u','u','i','d'),
   MP4_BOX_TYPE_ESDS              = VC_FOURCC('e','s','d','s'),
   MP4_BOX_TYPE_AVCC              = VC_FOURCC('a','v','c','C'),
   MP4_BOX_TYPE_D263              = VC_FOURCC('d','2','6','3'),
   MP4_BOX_TYPE_DAMR              = VC_FOURCC('d','a','m','r'),
   MP4_BOX_TYPE_DAWP              = VC_FOURCC('d','a','w','p'),
   MP4_BOX_TYPE_DEVC              = VC_FOURCC('d','e','v','c'),
   MP4_BOX_TYPE_WAVE              = VC_FOURCC('w','a','v','e'),
   MP4_BOX_TYPE_ZERO              = 0
} MP4_BOX_TYPE_T;

typedef enum {
   MP4_BRAND_ISOM                 = VC_FOURCC('i','s','o','m'),
   MP4_BRAND_MP42                 = VC_FOURCC('m','p','4','2'),
   MP4_BRAND_3GP4                 = VC_FOURCC('3','g','p','4'),
   MP4_BRAND_3GP5                 = VC_FOURCC('3','g','p','5'),
   MP4_BRAND_3GP6                 = VC_FOURCC('3','g','p','6'),
   MP4_BRAND_SKM2                 = VC_FOURCC('s','k','m','2'),
   MP4_BRAND_SKM3                 = VC_FOURCC('s','k','m','3'),
   MP4_BRAND_QT                   = VC_FOURCC('q','t',' ',' '),
   MP4_BRAND_NUM
} MP4_BRAND_T;

typedef enum
{
   MP4_SAMPLE_TABLE_STTS = 0,  /* decoding time to sample */
   MP4_SAMPLE_TABLE_STSZ = 1,  /* sample size */
   MP4_SAMPLE_TABLE_STSC = 2,  /* sample to chunk */
   MP4_SAMPLE_TABLE_STCO = 3,  /* sample to chunk-offset */
   MP4_SAMPLE_TABLE_STSS = 4,  /* sync sample */
   MP4_SAMPLE_TABLE_CO64 = 5,  /* sample to chunk-offset */
   MP4_SAMPLE_TABLE_CTTS = 6,  /* composite time to sample */
   MP4_SAMPLE_TABLE_NUM
} MP4_SAMPLE_TABLE_T;

/* Values for object_type_indication (mp4_decoder_config_descriptor)
 * see ISO/IEC 14496-1:2001(E) section 8.6.6.2 table 8 p. 30
 * see ISO/IEC 14496-15:2003 (draft) section 4.2.2 table 3 p. 11
 * see SKT Spec 8.2.3 p. 107
 * see 3GPP2 Spec v1.0 p. 22 */
#define MP4_MPEG4_VISUAL_OBJECT_TYPE    0x20  /* visual ISO/IEC 14496-2 */
#define MP4_MPEG4_H264_OBJECT_TYPE      0x21  /* visual ISO/IEC 14496-10 */
#define MP4_MPEG4_H264_PS_OBJECT_TYPE   0x22  /* visual ISO/IEC 14496-10 (used for parameter ES) */
#define MP4_MPEG4_AAC_LC_OBJECT_TYPE    0x40  /* audio ISO/IEC 14496-3 */
#define MP4_MPEG2_SP_OBJECT_TYPE        0x60  /* visual ISO/IEC 13818-2 Simple Profile */
#define MP4_MPEG2_MP_OBJECT_TYPE        0x61  /* visual ISO/IEC 13818-2 Main Profile */
#define MP4_MPEG2_SNR_OBJECT_TYPE       0x62  /* visual ISO/IEC 13818-2 SNR Profile */
#define MP4_MPEG2_AAC_LC_OBJECT_TYPE    0x67  /* audio ISO/IEC 13818-7 LowComplexity Profile */
#define MP4_MP3_OBJECT_TYPE             0x69  /* audio ISO/IEC 13818-3 */
#define MP4_MPEG1_VISUAL_OBJECT_TYPE    0x6A  /* visual ISO/IEC 11172-2 */
#define MP4_MPEG1_AUDIO_OBJECT_TYPE     0x6B  /* audio ISO/IEC 11172-3 */
#define MP4_JPEG_OBJECT_TYPE            0x6C  /* visual ISO/IEC 10918-1 */
#define MP4_SKT_EVRC_2V1_OBJECT_TYPE    0x82  /* SKT spec V2.1 for EVRC */
#define MP4_KTF_EVRC_OBJECT_TYPE        0xC2  /* KTF spec V1.2 for EVRC */
#define MP4_KTF_AMR_OBJECT_TYPE         0xC4  /* KTF spec V1.2 for AMR */
#define MP4_KTF_MP3_OBJECT_TYPE         0xC5  /* KTF spec V1.2 for MP3 */
#define MP4_SKT_TEXT_OBJECT_TYPE        0xD0  /* SKT spec V2.2 for Text */
#define MP4_SKT_EVRC_OBJECT_TYPE        0xD1  /* SKT spec V2.2 for EVRC */
#define MP4_3GPP2_QCELP_OBJECT_TYPE     0xE1  /* 3GPP2 spec V1.0 for QCELP13K */

#endif /* MP4_COMMON_H */
