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

#ifndef MMAL_FORMAT_H
#define MMAL_FORMAT_H

#ifdef __cplusplus
extern "C" {
#endif

/** \defgroup MmalFormat Elementary stream format
 * Definition of an elementary stream format and its associated API */
/* @{ */

#include "mmal_types.h"
#include "mmal_encodings.h"

/** Enumeration of the different types of elementary streams.
 * This divides elementary streams into 4 big categories, plus an invalid type. */
typedef enum {
   MMAL_ES_TYPE_UNKNOWN,     /**< Unknown elementary stream type */
   MMAL_ES_TYPE_CONTROL,     /**< Elementary stream of control commands */
   MMAL_ES_TYPE_AUDIO,       /**< Audio elementary stream */
   MMAL_ES_TYPE_VIDEO,       /**< Video elementary stream */
   MMAL_ES_TYPE_SUBPICTURE   /**< Sub-picture elementary stream (e.g. subtitles, overlays) */
} MMAL_ES_TYPE_T;

/** Definition of a video format.
 * This describes the properties specific to a video stream */
typedef struct
{
   uint32_t        width;        /**< Width of frame in pixels */
   uint32_t        height;       /**< Height of frame in rows of pixels */
   MMAL_RECT_T     crop;         /**< Visible region of the frame */
   MMAL_RATIONAL_T frame_rate;   /**< Frame rate */
   MMAL_RATIONAL_T par;          /**< Pixel aspect ratio */

   MMAL_FOURCC_T   color_space;  /**< FourCC specifying the color space of the
                                   * video stream. See the \ref MmalColorSpace
                                   * "pre-defined color spaces" for some examples.
                                   */

} MMAL_VIDEO_FORMAT_T;

/** Definition of an audio format.
 * This describes the properties specific to an audio stream */
typedef struct MMAL_AUDIO_FORMAT_T
{
   uint32_t channels;           /**< Number of audio channels */
   uint32_t sample_rate;        /**< Sample rate */

   uint32_t bits_per_sample;    /**< Bits per sample */
   uint32_t block_align;        /**< Size of a block of data */

   /** \todo add channel mapping, gapless and replay-gain support */

} MMAL_AUDIO_FORMAT_T;

/** Definition of a subpicture format.
 * This describes the properties specific to a subpicture stream */
typedef struct
{
   uint32_t x_offset;        /**< Width offset to the start of the subpicture */
   uint32_t y_offset;        /**< Height offset to the start of the subpicture */

   /** \todo surely more things are needed here */

} MMAL_SUBPICTURE_FORMAT_T;

/** Definition of the type specific format.
 * This describes the type specific information of the elementary stream. */
typedef union
{
   MMAL_AUDIO_FORMAT_T      audio;      /**< Audio specific information */
   MMAL_VIDEO_FORMAT_T      video;      /**< Video specific information */
   MMAL_SUBPICTURE_FORMAT_T subpicture; /**< Subpicture specific information */
} MMAL_ES_SPECIFIC_FORMAT_T;

/** \name Elementary stream flags
 * \anchor elementarystreamflags
 * The following flags describe properties of an elementary stream */
/* @{ */
#define MMAL_ES_FORMAT_FLAG_FRAMED       0x1 /**< The elementary stream will already be framed */
/* @} */

/** \name Undefined encoding value.
 * This value indicates an unknown encoding
 */
/* @{ */
#define MMAL_ENCODING_UNKNOWN            0
/* @} */

/** \name Default encoding variant value.
 * This value indicates the default encoding variant is used
 */
/* @{ */
#define MMAL_ENCODING_VARIANT_DEFAULT    0
/* @} */

/** Definition of an elementary stream format */
typedef struct MMAL_ES_FORMAT_T
{
   MMAL_ES_TYPE_T type;           /**< Type of the elementary stream */

   MMAL_FOURCC_T encoding;        /**< FourCC specifying the encoding of the elementary stream.
                                    * See the \ref MmalEncodings "pre-defined encodings" for some
                                    * examples.
                                    */
   MMAL_FOURCC_T encoding_variant;/**< FourCC specifying the specific encoding variant of
                                    * the elementary stream. See the \ref MmalEncodingVariants
                                    * "pre-defined encoding variants" for some examples.
                                    */

   MMAL_ES_SPECIFIC_FORMAT_T *es; /**< Type specific information for the elementary stream */

   uint32_t bitrate;              /**< Bitrate in bits per second */
   uint32_t flags;                /**< Flags describing properties of the elementary stream.
                                    * See \ref elementarystreamflags "Elementary stream flags".
                                    */

   uint32_t extradata_size;       /**< Size of the codec specific data */
   uint8_t  *extradata;           /**< Codec specific data */

} MMAL_ES_FORMAT_T;

/** Allocate and initialise a \ref MMAL_ES_FORMAT_T structure.
 *
 * @return a \ref MMAL_ES_FORMAT_T structure
 */
MMAL_ES_FORMAT_T *mmal_format_alloc(void);

/** Free a \ref MMAL_ES_FORMAT_T structure allocated by \ref mmal_format_alloc.
 *
 * @param format the \ref MMAL_ES_FORMAT_T structure to free
 */
void mmal_format_free(MMAL_ES_FORMAT_T *format);

/** Allocate the extradata buffer in \ref MMAL_ES_FORMAT_T.
 * This buffer will be freed automatically when the format is destroyed or
 * another allocation is done.
 *
 * @param format format structure for which the extradata buffer will be allocated
 * @param size size of the extradata buffer to allocate
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_format_extradata_alloc(MMAL_ES_FORMAT_T *format, unsigned int size);

/** Shallow copy a format structure.
 * It is worth noting that the extradata buffer will not be copied in the new format.
 *
 * @param format_dest destination \ref MMAL_ES_FORMAT_T for the copy
 * @param format_src source \ref MMAL_ES_FORMAT_T for the copy
 */
void mmal_format_copy(MMAL_ES_FORMAT_T *format_dest, MMAL_ES_FORMAT_T *format_src);

/** Fully copy a format structure, including the extradata buffer.
 *
 * @param format_dest destination \ref MMAL_ES_FORMAT_T for the copy
 * @param format_src source \ref MMAL_ES_FORMAT_T for the copy
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_format_full_copy(MMAL_ES_FORMAT_T *format_dest, MMAL_ES_FORMAT_T *format_src);

/** \name Comparison flags
 * \anchor comparisonflags
 * The following flags describe the differences between 2 format structures */
/* @{ */
#define MMAL_ES_FORMAT_COMPARE_FLAG_TYPE              0x01 /**< The type is different */
#define MMAL_ES_FORMAT_COMPARE_FLAG_ENCODING          0x02 /**< The encoding is different */
#define MMAL_ES_FORMAT_COMPARE_FLAG_BITRATE           0x04 /**< The bitrate is different */
#define MMAL_ES_FORMAT_COMPARE_FLAG_FLAGS             0x08 /**< The flags are different */
#define MMAL_ES_FORMAT_COMPARE_FLAG_EXTRADATA         0x10 /**< The extradata is different */

#define MMAL_ES_FORMAT_COMPARE_FLAG_VIDEO_RESOLUTION   0x0100 /**< The video resolution is different */
#define MMAL_ES_FORMAT_COMPARE_FLAG_VIDEO_CROPPING     0x0200 /**< The video cropping is different */
#define MMAL_ES_FORMAT_COMPARE_FLAG_VIDEO_FRAME_RATE   0x0400 /**< The video frame rate is different */
#define MMAL_ES_FORMAT_COMPARE_FLAG_VIDEO_ASPECT_RATIO 0x0800 /**< The video aspect ratio is different */
#define MMAL_ES_FORMAT_COMPARE_FLAG_VIDEO_COLOR_SPACE  0x1000 /**< The video color space is different */

#define MMAL_ES_FORMAT_COMPARE_FLAG_ES_OTHER  0x10000000 /**< Other ES specific parameters are different */
/* @} */

/** Compare 2 format structures and returns a set of flags describing the differences.
 * The result will be zero if the structures are the same, or a combination of
 * one or more of the \ref comparisonflags "Comparison flags" if different.
 *
 * @param format_1 first \ref MMAL_ES_FORMAT_T to compare
 * @param format_2 second \ref MMAL_ES_FORMAT_T to compare
 * @return set of flags describing the differences
 */
uint32_t mmal_format_compare(MMAL_ES_FORMAT_T *format_1, MMAL_ES_FORMAT_T *format_2);

/* @} */

#ifdef __cplusplus
}
#endif

#endif /* MMAL_FORMAT_H */
