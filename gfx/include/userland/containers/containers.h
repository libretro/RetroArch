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
#ifndef VC_CONTAINERS_H
#define VC_CONTAINERS_H

/** \file containers.h
 * Public API for container readers and writers
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "containers/containers_types.h"

/** \defgroup VcContainerApi Container API
 *  API for container readers and writers */
/* @{ */

/** Status codes returned by the container API */
typedef enum
{
   VC_CONTAINER_SUCCESS = 0,                        /**< No error */
   VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED,         /**< Format of container is not supported */
   VC_CONTAINER_ERROR_FORMAT_FEATURE_NOT_SUPPORTED, /**< Format of container uses unsupported features */
   VC_CONTAINER_ERROR_FORMAT_INVALID,               /**< Format of container is invalid */
   VC_CONTAINER_ERROR_CORRUPTED,                    /**< Container is corrupted */
   VC_CONTAINER_ERROR_URI_NOT_FOUND,                /**< URI could not be found */
   VC_CONTAINER_ERROR_URI_OPEN_FAILED,              /**< URI could not be opened */
   VC_CONTAINER_ERROR_OUT_OF_MEMORY,                /**< Out of memory */
   VC_CONTAINER_ERROR_OUT_OF_SPACE,                 /**< Out of disk space (used when writing) */
   VC_CONTAINER_ERROR_OUT_OF_RESOURCES,             /**< Out of resources (other than memory) */
   VC_CONTAINER_ERROR_EOS,                          /**< End of stream reached */
   VC_CONTAINER_ERROR_LIMIT_REACHED,                /**< User defined limit reached (used when writing) */
   VC_CONTAINER_ERROR_BUFFER_TOO_SMALL,             /**< Given buffer is too small for data to be copied */
   VC_CONTAINER_ERROR_INCOMPLETE_DATA,              /**< Requested data is incomplete */
   VC_CONTAINER_ERROR_NO_TRACK_AVAILABLE,           /**< Container doesn't have any track */
   VC_CONTAINER_ERROR_TRACK_FORMAT_NOT_SUPPORTED,   /**< Format of the track is not supported */
   VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION,        /**< The requested operation is not supported */
   VC_CONTAINER_ERROR_INVALID_ARGUMENT,             /**< The argument provided is invalid */
   VC_CONTAINER_ERROR_CONTINUE,                     /**< The requested operation was interrupted and needs to be tried again */
   VC_CONTAINER_ERROR_ABORTED,                      /**< The requested operation was aborted */
   VC_CONTAINER_ERROR_NOT_FOUND,                    /**< The requested data was not found */
   VC_CONTAINER_ERROR_DRM_NOT_AUTHORIZED,           /**< The DRM was not authorized */
   VC_CONTAINER_ERROR_DRM_EXPIRED,                  /**< The DRM has expired */
   VC_CONTAINER_ERROR_DRM_FAILED,                   /**< Generic DRM error */
   VC_CONTAINER_ERROR_FAILED,                       /**< Generic error */
   VC_CONTAINER_ERROR_NOT_READY                     /**< The container was not yet able to carry out the operation. */
} VC_CONTAINER_STATUS_T;

/** Four Character Code type used to identify codecs, etc. */
typedef uint32_t VC_CONTAINER_FOURCC_T;

/** Type definition for language codes.
 * Language are defined as ISO639 Alpha-3 codes (http://en.wikipedia.org/wiki/List_of_ISO_639-2_codes) */
typedef uint8_t VC_CONTAINER_LANGUAGE_T[3];

/** Enumeration of the character encodings supported. */
typedef enum {
   VC_CONTAINER_CHAR_ENCODING_UNKNOWN = 0, /**< Encoding is unknown */
   VC_CONTAINER_CHAR_ENCODING_UTF8         /**< UTF8 encoding */
} VC_CONTAINER_CHAR_ENCODING_T;

/** \name Container Capabilities
 * The following flags are exported by containers to describe their capabilities */
/* @{ */
/** Type definition for container capabilities */
typedef uint32_t VC_CONTAINER_CAPABILITIES_T;
/** The container can seek */
#define VC_CONTAINER_CAPS_CAN_SEEK               0x1
/** Seeking is fast. The absence of this flag can be used as a hint to avoid seeking as much as possible */
#define VC_CONTAINER_CAPS_SEEK_IS_FAST           0x2
/** The container controls the pace at which it is reading the data */
#define VC_CONTAINER_CAPS_CAN_CONTROL_PACE       0x4
/** The container provides an index. This basically means that seeking will be precise and fast */
#define VC_CONTAINER_CAPS_HAS_INDEX              0x8
/** The container provides keyframe information */
#define VC_CONTAINER_CAPS_DATA_HAS_KEYFRAME_FLAG 0x10
/** The container supports adding tracks after TRACK_ADD_DONE control message has been sent */
#define VC_CONTAINER_CAPS_DYNAMIC_TRACK_ADD      0x20
/** The container supports forcing reading of a given track */
#define VC_CONTAINER_CAPS_FORCE_TRACK            0x40
/* @} */

/** \defgroup VcContainerMetadata Container Metadata
 * Container metadata contains descriptive information which is associated with the multimedia data */
/* @{ */

/** Enumeration of the different metadata keys available. */
typedef enum {
   /* Metadata of global scope */
   VC_CONTAINER_METADATA_KEY_TITLE       = VC_FOURCC('t','i','t','l'),
   VC_CONTAINER_METADATA_KEY_ARTIST      = VC_FOURCC('a','r','t','i'),
   VC_CONTAINER_METADATA_KEY_ALBUM       = VC_FOURCC('a','l','b','m'),
   VC_CONTAINER_METADATA_KEY_DESCRIPTION = VC_FOURCC('d','e','s','c'),
   VC_CONTAINER_METADATA_KEY_YEAR        = VC_FOURCC('y','e','a','r'),
   VC_CONTAINER_METADATA_KEY_GENRE       = VC_FOURCC('g','e','n','r'),
   VC_CONTAINER_METADATA_KEY_TRACK       = VC_FOURCC('t','r','a','k'),
   VC_CONTAINER_METADATA_KEY_LYRICS      = VC_FOURCC('l','y','r','x'),

   VC_CONTAINER_METADATA_KEY_UNKNOWN     = 0

} VC_CONTAINER_METADATA_KEY_T;

/** Definition of the metadata type.
 * This type is used to store one element of metadata */
typedef struct VC_CONTAINER_METADATA_T
{
   /** Identifier for the type of metadata the value refers to.
    * Using an enum for the id will mean that a list of possible values will have to be
    * defined and maintained. This might limit extensibility and customisation.\n
    * Maybe it would be better to use a FOURCC or even a string here. */
   VC_CONTAINER_METADATA_KEY_T key;

   VC_CONTAINER_LANGUAGE_T language; /**< Language code for the metadata */
   VC_CONTAINER_CHAR_ENCODING_T encoding; /**< Encoding of the metadata */

   /** Metadata value. This value is defined as null-terminated UTF-8 string.\n
    * Do we want to support other types than strings (e.g. integer) ?\n
    * We need an encoding conversion library! */
   char *value;

   /** Size of the memory area reserved for metadata value (including any
    * terminating characters). */
   unsigned int size;
} VC_CONTAINER_METADATA_T;
/* @} */

/** \defgroup VcContainerESFormat Container Elementary Stream Format
 * This describes the format of an elementary stream associated with a track */
/* @{ */

/** Enumeration of the different types of elementary streams.
 * This divides elementary streams into 4 big categories. */
typedef enum
{
   VC_CONTAINER_ES_TYPE_UNKNOWN,     /**< Unknown elementary stream type */
   VC_CONTAINER_ES_TYPE_AUDIO,       /**< Audio elementary stream */
   VC_CONTAINER_ES_TYPE_VIDEO,       /**< Video elementary stream */
   VC_CONTAINER_ES_TYPE_SUBPICTURE   /**< Sub-picture elementary stream (e.g. subtitles, overlays) */

} VC_CONTAINER_ES_TYPE_T;

/** Definition of a video format.
 * This describes the properties specific to a video stream */
typedef struct VC_CONTAINER_VIDEO_FORMAT_T
{
   uint32_t width;           /**< Width of the frame */
   uint32_t height;          /**< Height of the frame */
   uint32_t visible_width;   /**< Width of the visible area of the frame */
   uint32_t visible_height;  /**< Height of the visible area of the frame */
   uint32_t x_offset;        /**< Offset to the start of the visible width */
   uint32_t y_offset;        /**< Offset to the start of the visible height */
   uint32_t frame_rate_num;  /**< Frame rate numerator */
   uint32_t frame_rate_den;  /**< Frame rate denominator */
   uint32_t par_num;         /**< Pixel aspect ratio numerator */
   uint32_t par_den;         /**< Pixel aspect ratio denominator */
} VC_CONTAINER_VIDEO_FORMAT_T;

/** Enumeration for the different channel locations */
typedef enum
{
   VC_CONTAINER_AUDIO_CHANNEL_LEFT = 0,            /**< Left channel */
   VC_CONTAINER_AUDIO_CHANNEL_RIGHT,               /**< Right channel */
   VC_CONTAINER_AUDIO_CHANNEL_CENTER,              /**< Center channel */
   VC_CONTAINER_AUDIO_CHANNEL_LOW_FREQUENCY,       /**< Low frequency channel */
   VC_CONTAINER_AUDIO_CHANNEL_BACK_LEFT,           /**< Back left channel */
   VC_CONTAINER_AUDIO_CHANNEL_BACK_RIGHT,          /**< Back right channel */
   VC_CONTAINER_AUDIO_CHANNEL_BACK_CENTER,         /**< Back center channel */
   VC_CONTAINER_AUDIO_CHANNEL_SIDE_LEFT,           /**< Side left channel */
   VC_CONTAINER_AUDIO_CHANNEL_SIDE_RIGHT,          /**< Side right channel */

   VC_CONTAINER_AUDIO_CHANNELS_MAX = 32            /**< Maximum number of channels supported */

} VC_CONTAINER_AUDIO_CHANNEL_T;

/** \name Audio format flags
 * \anchor audioformatflags
 * The following flags describe properties of an audio stream */
/* @{ */
#define VC_CONTAINER_AUDIO_FORMAT_FLAG_CHANNEL_MAPPING 0x1 /**< Channel mapping available */
/* @} */

/** Definition of an audio format.
 * This describes the properties specific to an audio stream */
typedef struct VC_CONTAINER_AUDIO_FORMAT_T
{
   uint32_t channels;           /**< Number of audio channels */
   uint32_t sample_rate;        /**< Sample rate */

   uint32_t bits_per_sample;    /**< Bits per sample */
   uint32_t block_align;        /**< Size of a block of data */

   uint32_t flags;              /**< Flags describing the audio format.
                                 * See \ref audioformatflags "Audio format flags". */

   /** Mapping of the channels in order of appearance */
   VC_CONTAINER_AUDIO_CHANNEL_T channel_mapping[VC_CONTAINER_AUDIO_CHANNELS_MAX];

   uint16_t gap_delay;   /**< Delay introduced by the encoder. Used for gapless playback */
   uint16_t gap_padding; /**< Padding introduced by the encoder. Used for gapless playback */

   /** Replay gain information. First element is the track information and the second
    * is the album information. */
   struct {
      float peak; /**< Peak value (full range is 1.0) */
      float gain; /**< Gain value in dB */
   } replay_gain[2];

} VC_CONTAINER_AUDIO_FORMAT_T;

/** Definition of a subpicture format.
 * This describes the properties specific to a subpicture stream */
typedef struct VC_CONTAINER_SUBPICTURE_FORMAT_T
{
   VC_CONTAINER_CHAR_ENCODING_T encoding; /**< Encoding for text based subpicture formats */
   uint32_t x_offset;        /**< Width offset to the start of the subpicture */
   uint32_t y_offset;        /**< Height offset to the start of the subpicture */
} VC_CONTAINER_SUBPICTURE_FORMAT_T;

/** \name Elementary stream format flags
 * \anchor esformatflags
 * The following flags describe properties of an elementary stream */
/* @{ */
#define VC_CONTAINER_ES_FORMAT_FLAG_FRAMED 0x1 /**< Elementary stream is framed */
/* @} */

/** Definition of the type specific format.
 * This describes the type specific information of the elementary stream. */
typedef union
{
   VC_CONTAINER_AUDIO_FORMAT_T      audio;      /**< Audio specific information */
   VC_CONTAINER_VIDEO_FORMAT_T      video;      /**< Video specific information */
   VC_CONTAINER_SUBPICTURE_FORMAT_T subpicture; /**< Subpicture specific information */
} VC_CONTAINER_ES_SPECIFIC_FORMAT_T;

/** Definition of an elementary stream format */
typedef struct VC_CONTAINER_ES_FORMAT_T
{
   VC_CONTAINER_ES_TYPE_T es_type;    /**< Type of the elementary stream */
   VC_CONTAINER_FOURCC_T  codec;      /**< Coding of the elementary stream */
   VC_CONTAINER_FOURCC_T  codec_variant;  /**< If set, indicates a variant of the coding */

   VC_CONTAINER_ES_SPECIFIC_FORMAT_T *type; /**< Type specific information for the elementary stream */

   uint32_t bitrate;         /**< Bitrate */

   VC_CONTAINER_LANGUAGE_T language; /**< Language code for the elementary stream */
   uint32_t group_id;                /**< ID of the group this elementary stream belongs to */

   uint32_t flags;         /**< Flags describing the properties of an elementary stream.
                            * See \ref esformatflags "Elementary stream format flags". */

   unsigned int extradata_size; /**< Size of the codec specific data */
   uint8_t *extradata;     /**< Codec specific data */

} VC_CONTAINER_ES_FORMAT_T;
/* @} */

/** \defgroup VcContainerPacket Container Packet
 * A container packet is the unit of data that is being read from or written to a container */
/* @{ */

/** Structure describing a data packet */
typedef struct VC_CONTAINER_PACKET_T
{
   struct VC_CONTAINER_PACKET_T *next; /**< Used to build lists of packets */
   uint8_t *data;              /**< Pointer to the buffer containing the actual data for the packet */
   unsigned int buffer_size;   /**< Size of the p_data buffer. This is used to indicate how much data can be read in p_data */
   unsigned int size;          /**< Size of the data contained in p_data */
   unsigned int frame_size;    /**< If set, indicates the size of the frame this packet belongs to */
   int64_t pts;                /**< Presentation Timestamp of the packet */
   int64_t dts;                /**< Decoding Timestamp of the packet */
   uint64_t num;               /**< Number of this packet */
   uint32_t track;             /**< Track associated with this packet */
   uint32_t flags;             /**< Flags associated with this packet */

   void *user_data;            /**< Field reserved for use by the client */
   void *framework_data;       /**< Field reserved for use by the framework */

} VC_CONTAINER_PACKET_T;

/** \name Container Packet Flags
 * The following flags describe properties of the data packet */
/* @{ */
#define VC_CONTAINER_PACKET_FLAG_KEYFRAME       0x01   /**< Packet is a keyframe */
#define VC_CONTAINER_PACKET_FLAG_FRAME_START    0x02   /**< Packet starts a frame */
#define VC_CONTAINER_PACKET_FLAG_FRAME_END      0x04   /**< Packet ends a frame */
#define VC_CONTAINER_PACKET_FLAG_FRAME          0x06   /**< Packet contains only complete frames */
#define VC_CONTAINER_PACKET_FLAG_DISCONTINUITY  0x08   /**< Packet comes after a discontinuity in the stream. Decoders might have to be flushed */
#define VC_CONTAINER_PACKET_FLAG_ENCRYPTED      0x10   /**< Packet contains DRM encrypted data */
#define VC_CONTAINER_PACKET_FLAG_CONFIG         0x20   /**< Packet contains stream specific config data */
/* @} */

/** \name Special Unknown Time Value
 * This is the special value used to signal that a timestamp is not known */
/* @{ */
#define VC_CONTAINER_TIME_UNKNOWN (INT64_C(1)<<63)     /**< Special value for signalling that time is not known */
/* @} */

/* @} */

/** \name Track flags
 * \anchor trackflags
 * The following flags describe properties of a track */
/* @{ */
#define VC_CONTAINER_TRACK_FLAG_CHANGED 0x1 /**< Track definition has changed */
/* @} */

/** Definition of the track type */
typedef struct VC_CONTAINER_TRACK_T
{
   struct VC_CONTAINER_TRACK_PRIVATE_T *priv; /**< Private member used by the implementation */
   uint32_t is_enabled;               /**< Flag to specify if the track is enabled */
   uint32_t flags;                    /**< Flags describing the properties of a track.
                                       * See \ref trackflags "Track flags". */

   VC_CONTAINER_ES_FORMAT_T *format;  /**< Format of the elementary stream contained in the track */

   unsigned int meta_num;             /**< Number of metadata elements associated with the track */
   VC_CONTAINER_METADATA_T **meta;    /**< Array of metadata elements associated with the track */

} VC_CONTAINER_TRACK_T;

/** Definition of the DRM type */
typedef struct VC_CONTAINER_DRM_T
{
   VC_CONTAINER_FOURCC_T format;    /**< Four character code describing the format of the DRM in use */
   unsigned int views_max;          /**< Maximum number of views allowed */
   unsigned int views_current;      /**< Current number of views */

} VC_CONTAINER_DRM_T;

/** Type definition for the progress reporting function. This function will be called regularly
 * by the container during a call which blocks for too long and will report the progress of the
 * operation as an estimated total length in microseconds and a percentage done.
 * Returning anything else than VC_CONTAINER_SUCCESS in this function will abort the current
 * operation. */
typedef VC_CONTAINER_STATUS_T (*VC_CONTAINER_PROGRESS_REPORT_FUNC_T)(void *userdata,
   int64_t length, unsigned int percentage_done);

/** \name Container Events
 * The following flags are exported by containers to notify the application of events */
/* @{ */
/** Type definition for container events */
typedef uint32_t VC_CONTAINER_EVENTS_T;
#define VC_CONTAINER_EVENT_TRACKS_CHANGE   1    /**< Track information has changed */
#define VC_CONTAINER_EVENT_METADATA_CHANGE 2    /**< Metadata has changed */
/* @} */

/** Definition of the container context */
typedef struct VC_CONTAINER_T
{
   struct VC_CONTAINER_PRIVATE_T *priv; /**< Private member used by the implementation */

   VC_CONTAINER_EVENTS_T events; /**< Events generated by the container */
   VC_CONTAINER_CAPABILITIES_T capabilities; /**< Capabilities exported by the container */

   VC_CONTAINER_PROGRESS_REPORT_FUNC_T pf_progress; /**< Progress report function pointer */
   void *progress_userdata;                          /**< Progress report user data */

   int64_t duration;                  /**< Duration of the media in microseconds */
   int64_t position;                  /**< Current time position into the media */
   int64_t size;                      /**< Size of the media in bytes */

   unsigned int tracks_num;           /**< Number of tracks available */
   /** Pointer to an array of pointers to track elements.
    * The reasoning for using a pointer to pointers here is to allow us to extend
    * VC_CONTAINER_TRACK_T without losing binary backward compatibility. */
   VC_CONTAINER_TRACK_T **tracks;

   unsigned int meta_num;             /**< Number of metadata elements associated with the container */
   VC_CONTAINER_METADATA_T **meta;    /**< Array of metadata elements associated with the container */

   VC_CONTAINER_DRM_T *drm;           /**< Description used for DRM protected content */

} VC_CONTAINER_T;

/** Forward declaration of a container input / output context.
 * This structure defines the context for a container io instance */
typedef struct VC_CONTAINER_IO_T VC_CONTAINER_IO_T;

/** Opens the media container pointed to by the URI for reading.
 * This will create an an instance of a container reader and its associated context.
 * The context returned will also be filled with the information retrieved from the media.
 *
 * If the media isn't accessible or recognized, this will return a null pointer as well as
 * an error code indicating why this failed.
 *
 * \param  psz_uri      Unified Resource Identifier pointing to the media container
 * \param  status       Returns the status of the operation
 * \param  pf_progress  User provided function pointer to a progress report function. Can be set to
 *                      null if no progress report is wanted. This function will be used during
 *                      the whole lifetime of the instance (i.e. it will be used during
 *                      open / seek / close)
 * \param  progress_userdata User provided pointer that will be passed during the progress report
 *                      function call.
 * \return              A pointer to the context of the new instance of the
 *                      container reader. Returns NULL on failure.
 */
VC_CONTAINER_T *vc_container_open_reader( const char *psz_uri, VC_CONTAINER_STATUS_T *status,
   VC_CONTAINER_PROGRESS_REPORT_FUNC_T pf_progress, void *progress_userdata);

/** Opens for reading the media container pointed to by the container i/o.
 * This will create an an instance of a container reader and its associated context.
 * The context returned will also be filled with the information retrieved from the media.
 *
 * If the media isn't accessible or recognized, this will return a null pointer as well as
 * an error code indicating why this failed.
 *
 * \param  p_io         Instance of the container i/o to use
 * \param  psz_uri      Unified Resource Identifier pointing to the media container (optional)
 * \param  status       Returns the status of the operation
 * \param  pf_progress  User provided function pointer to a progress report function. Can be set to
 *                      null if no progress report is wanted. This function will be used during
 *                      the whole lifetime of the instance (i.e. it will be used during
 *                      open / seek / close)
 * \param  progress_userdata User provided pointer that will be passed during the progress report
 *                      function call.
 * \return              A pointer to the context of the new instance of the
 *                      container reader. Returns NULL on failure.
 */
VC_CONTAINER_T *vc_container_open_reader_with_io( VC_CONTAINER_IO_T *p_io,
   const char *psz_uri, VC_CONTAINER_STATUS_T *status,
   VC_CONTAINER_PROGRESS_REPORT_FUNC_T pf_progress, void *progress_userdata);

/** Opens the media container pointed to by the URI for writing.
 * This will create an an instance of a container writer and its associated context.
 * The context returned will be initialised to sensible values.
 *
 * The application will need to add all the media tracks using \ref vc_container_control before
 * it starts writing data using \ref vc_container_write.
 *
 * If the media isn't accessible or recognized, this will return a null pointer as well as
 * an error code indicating why this failed.
 *
 * \param  psz_uri      Unified Resource Identifier pointing to the media container
 * \param  status       Returns the status of the operation
 * \param  pf_progress User provided function pointer to a progess report function. Can be set to
 *                      null if no progress report is wanted.
 * \param  progress_userdata User provided pointer that will be passed during the progress report
 *                      function call.
 * \return              A pointer to the context of the new instance of the
 *                      container writer. Returns NULL on failure.
 */
VC_CONTAINER_T *vc_container_open_writer( const char *psz_uri, VC_CONTAINER_STATUS_T *status,
   VC_CONTAINER_PROGRESS_REPORT_FUNC_T pf_progress, void *progress_userdata);

/** Closes an instance of a container reader / writer.
 * This will free all the resources associated with the context.
 *
 * \param  context   Pointer to the context of the instance to close
 * \return           the status of the operation
 */
VC_CONTAINER_STATUS_T vc_container_close( VC_CONTAINER_T *context );

/** \name Container read flags
 * The following flags can be passed during a read call */
/* @{ */
/** Type definition for the read flags */
typedef uint32_t VC_CONTAINER_READ_FLAGS_T;
/** Ask the container to only return information on the next packet without reading it */
#define VC_CONTAINER_READ_FLAG_INFO   1
/** Ask the container to skip the next packet */
#define VC_CONTAINER_READ_FLAG_SKIP   2
/** Force the container to read data from the specified track */
#define VC_CONTAINER_READ_FLAG_FORCE_TRACK 4
/* @} */

/** Reads a data packet from a container reader.
 * By default, the reader will read whatever packet comes next in the container and update the
 * given \ref VC_CONTAINER_PACKET_T structure with this packet's information.
 * This behaviour can be changed using the \ref VC_CONTAINER_READ_FLAGS_T.\n
 * \ref VC_CONTAINER_READ_FLAG_INFO will instruct the reader to only return information on the
 * following packet but not its actual data. The data can be retreived later by issuing another
 * read request.\n
 * \ref VC_CONTAINER_READ_FLAG_FORCE_TRACK will force the reader to read the next packet for the
 * selected track (as present in the \ref VC_CONTAINER_PACKET_T structure) instead of defaulting
 * to reading the packet which comes next in the container.\n
 * \ref VC_CONTAINER_READ_FLAG_SKIP will instruct the reader to skip the next packet. In this case
 * it isn't necessary for the caller to pass a pointer to a \ref VC_CONTAINER_PACKET_T structure
 * unless the \ref VC_CONTAINER_READ_FLAG_INFO is also given.\n
 * A combination of all these flags can be used.
 *
 * \param  context   Pointer to the context of the reader to use
 * \param  packet    Pointer to the VC_CONTAINER_PACKET_T structure describing the data packet
 *                   This needs to be partially filled before the call (buffer, buffer_size)
 * \param  flags     Flags controlling the read operation
 * \return           the status of the operation
 */
VC_CONTAINER_STATUS_T vc_container_read( VC_CONTAINER_T *context,
   VC_CONTAINER_PACKET_T *packet, VC_CONTAINER_READ_FLAGS_T flags );

/** Writes a data packet to a container writer.
 *
 * \param  context   Pointer to the context of the writer to use
 * \param  packet    Pointer to the VC_CONTAINER_PACKET_T structure describing the data packet
 * \return           the status of the operation
 */
VC_CONTAINER_STATUS_T vc_container_write( VC_CONTAINER_T *context,
   VC_CONTAINER_PACKET_T *packet );

/** Definition of the different seek modes */
typedef enum
{
   /** The offset provided for seeking is an absolute time offset in microseconds */
   VC_CONTAINER_SEEK_MODE_TIME = 0,
   /** The offset provided for seeking is a percentage (Q32 ?) */
   VC_CONTAINER_SEEK_MODE_PERCENT

} VC_CONTAINER_SEEK_MODE_T;

/** \name Container Seek Flags
 * The following flags control seek operations */
/* @{ */
/** Type definition for the seek flags */
typedef uint32_t VC_CONTAINER_SEEK_FLAGS_T;
/** Choose precise seeking even if slower */
#define VC_CONTAINER_SEEK_FLAG_PRECISE     0x1
/** By default a seek will always seek to the keyframe which comes just before the requested
 * position. This flag allows the caller to force the container to seek to the keyframe which
 * comes just after the requested position. */
#define VC_CONTAINER_SEEK_FLAG_FORWARD     0x2
/* @} */

/** Seek into a container reader.
 *
 * \param  context   Pointer to the context of the reader to use
 * \param  offset    Offset to seek to. Used as an input as well as output value.
 * \param  mode      Seeking mode requested.
 * \param  flags     Flags affecting the seeking operation.
 * \return           the status of the operation
 */
VC_CONTAINER_STATUS_T vc_container_seek( VC_CONTAINER_T *context, int64_t *offset,
      VC_CONTAINER_SEEK_MODE_T mode, VC_CONTAINER_SEEK_FLAGS_T flags);

/** Performance statistics.
 */
/** The maximum number of bins a statistics value is held in */
#define VC_CONTAINER_STATS_BINS 10

/** This type is used to represent multiple values of a statistic.
 */
typedef struct VC_CONTAINER_STATS_T
{
   /** The number of places to right shift count before using.  Resulting values
    * of zero are rounded to 1. */
   uint32_t shift;

   /** We store VC_CONTAINER_STATS_BINS+1 records, in sorted order of numpc.
    * At least one will be invalid and all zero.  We combine adjacent records
    * as necessary. */
   struct {
      /** Sum of count.  For single value statistics this is the freqency, for paired statistics
       * this is the number of bytes written. */
      uint32_t count;
      /** Number of count. For single value statistics this is the total value, for paired statistics
       * this is the total length of time.  */
      uint32_t num;
      /** Number>>shift per count.  Stored to save recalculation. */
      uint32_t numpc;
   } record[VC_CONTAINER_STATS_BINS+1];
} VC_CONTAINER_STATS_T;

/** This type represents the statistics saved by the io layer. */
typedef struct VC_CONTAINER_WRITE_STATS_T
{
   /** This logs the number of bytes written in count, and the microseconds taken to write
    * in num. */
   VC_CONTAINER_STATS_T write;
   /** This logs the length of time the write function has to wait for the asynchronous task. */
   VC_CONTAINER_STATS_T wait;
   /** This logs the length of time that we wait for a flush command to complete. */
   VC_CONTAINER_STATS_T flush;
} VC_CONTAINER_WRITE_STATS_T;

/** Control operations which can be done on containers. */
typedef enum
{
   /** Adds a new track to the list of tracks. This should be used by writers to create
    * their list of tracks.\n
    * Arguments:\n
    *   arg1= VC_CONTAINER_ES_FORMAT_T *: format of the track to add\n
    *   return=  VC_CONTAINER_ERROR_TRACK_FORMAT_NOT_SUPPORTED if the format is not supported */
   VC_CONTAINER_CONTROL_TRACK_ADD = 0,

   /** Specifies that we're done adding new tracks. This is optional but can be used by writers
    * to trigger the writing of the container header early. If this isn't used, the header will be
    * written when the first data packet is received.\n
    * No arguments.\n
    *   return=  VC_CONTAINER_ERROR_TRACK_FORMAT_NOT_SUPPORTED if the format is not supported */
   VC_CONTAINER_CONTROL_TRACK_ADD_DONE,

   /** Change the format of a track in the list of tracks. This should be used by writers to modify
    * the format of a track at run-time.\n
    * Arguments:\n
    *   arg1= unsigned int: index of track to change\n
    *   arg2= VC_CONTAINER_ES_FORMAT_T *: format of the track to add\n
    *   return=  VC_CONTAINER_ERROR_TRACK_FORMAT_NOT_SUPPORTED if the format is not supported */
   VC_CONTAINER_CONTROL_TRACK_CHANGE,

   /** Deletes a track from the list of tracks. This should be used by writers to delete tracks
    * during run-time. Note that vc_container_close will automatically delete all track so it
    * isn't necessary to call this before closing a writer.\n
    * Arguments:\n
    *   arg1= index of the track to delete  */
   VC_CONTAINER_CONTROL_TRACK_DEL,

   /** Activate the playback of DRM protected content.\n
    * No arguments.\n
    *   return=  one of the VC_CONTAINER_ERROR_DRM error codes if content can't be played */
   VC_CONTAINER_CONTROL_DRM_PLAY,

   /** TBD */
   VC_CONTAINER_CONTROL_METADATA_ADD,
   /** TBD */
   VC_CONTAINER_CONTROL_METADATA_CHANGE,
   /** TBD */
   VC_CONTAINER_CONTROL_METADATA_DEL,

   /** TBD */
   VC_CONTAINER_CONTROL_CHAPTER_ADD,
   /** TBD */
   VC_CONTAINER_CONTROL_CHAPTER_DEL,

   /** Set a maximum size for files generated by writers.\n
    * Arguments:\n
    *   arg1= uint64_t: maximum size */
   VC_CONTAINER_CONTROL_SET_MAXIMUM_SIZE,

   /** Enables/disabled performance statistic gathering.\n
    * Arguments:\n
    *   arg1= bool: enable or disable */
   VC_CONTAINER_CONTROL_SET_IO_PERF_STATS,

   /** Collects performance statistics.\n
    * Arguments:\n
    *   arg1= VC_CONTAINER_WRITE_STATS_T *: */
   VC_CONTAINER_CONTROL_GET_IO_PERF_STATS,

   /** HACK.\n
    * Arguments:\n
    *   arg1= void (*)(void *): callback function
    *   arg1= void *: opaque pointer to pass during the callback */
   VC_CONTAINER_CONTROL_SET_IO_BUFFER_FULL_CALLBACK,

   /** Set the I/O read buffer size to be used.\n
    * Arguments:\n
    *   arg1= uint32_t: New buffer size in bytes*/
   VC_CONTAINER_CONTROL_IO_SET_READ_BUFFER_SIZE,

   /** Set the timeout on I/O read operations, if applicable.\n
    * Arguments:\n
    *   arg1= uint32_t: New timeout in milliseconds, or VC_CONTAINER_READ_TIMEOUT_BLOCK */
   VC_CONTAINER_CONTROL_IO_SET_READ_TIMEOUT_MS,

   /** Set the timestamp base.\n
    * The timestamp passed equates to time zero for the stream.\n
    * Arguments:\n
    *   arg1= uint32_t: Timestamp base in stream clock units. */
   VC_CONTAINER_CONTROL_SET_TIMESTAMP_BASE,

   /** Set the next expected sequence number for the stream.\n
    * Arguments:\n
    *   arg1= uint32_t: Next expected sequence number. */
   VC_CONTAINER_CONTROL_SET_NEXT_SEQUENCE_NUMBER,

   /** Set the source ID for the container.\n
    * Arguments:\n
    *   arg1= uint32_t: Source identifier. */
   VC_CONTAINER_CONTROL_SET_SOURCE_ID,

   /** Arguments:\n
    *   arg1= void *: metadata buffer
    *   arg2= unsigned long: length of metadata in bytes */
   VC_CONTAINER_CONTROL_GET_DRM_METADATA,

   /** Arguments:\n
    *   arg1= unsigned long: track number
    *   arg2= VC_CONTAINER_FOURCC_T : drm type
    *   arg3= void *: encryption configuration parameters.
    *   arg4= unsigned long: configuration data length */
   VC_CONTAINER_CONTROL_ENCRYPT_TRACK,

   /** Causes the io to be flushed.\n
    * Arguments: none */
   VC_CONTAINER_CONTROL_IO_FLUSH,

   /** Request the container reader to packetize data for the specified track.
    * Arguments:\n
    *   arg1= unsigned long: track number
    *   arg2= VC_CONTAINER_FOURCC_T: codec variant to output */
   VC_CONTAINER_CONTROL_TRACK_PACKETIZE,

   /** Private user extensions must be above this number */
   VC_CONTAINER_CONTROL_USER_EXTENSIONS = 0x1000

} VC_CONTAINER_CONTROL_T;

/** Used with the VC_CONTAINER_CONTROL_IO_SET_READ_TIMEOUT_MS control to indicate the read shall
 * block until either data is available, or an error occurs.
 */
#define VC_CONTAINER_READ_TIMEOUT_BLOCK   (uint32_t)(-1)

/** Extensible control function for container readers and writers.
 * This function takes a variable number of arguments which will depend on the specific operation.
 *
 * \param  context   Pointer to the VC_CONTAINER_T context to use
 * \param  operation The requested operation
 * \return           the status of the operation. Can be \ref VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION
 *                   if the operation is not supported or implemented by the container.
 */
VC_CONTAINER_STATUS_T vc_container_control( VC_CONTAINER_T *context, VC_CONTAINER_CONTROL_T operation, ... );

/* @} */

#ifdef __cplusplus
}
#endif

#endif /* VC_CONTAINERS_H */
