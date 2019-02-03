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

#ifndef MMAL_PARAMETERS_COMMON_H
#define MMAL_PARAMETERS_COMMON_H

/** @defgroup MMAL_PARAMETER_IDS Pre-defined MMAL parameter IDs
 * @ingroup MmalParameters
 * @{
 */

/** @name Parameter groups
 * Parameters are divided into groups, and then allocated sequentially within
 * a group using an enum.
 * @{
 */

/** Common parameter ID group, used with many types of component. */
#define MMAL_PARAMETER_GROUP_COMMON            (0<<16)
/** Camera-specific parameter ID group. */
#define MMAL_PARAMETER_GROUP_CAMERA            (1<<16)
/** Video-specific parameter ID group. */
#define MMAL_PARAMETER_GROUP_VIDEO             (2<<16)
/** Audio-specific parameter ID group. */
#define MMAL_PARAMETER_GROUP_AUDIO             (3<<16)
/** Clock-specific parameter ID group. */
#define MMAL_PARAMETER_GROUP_CLOCK             (4<<16)
/** Miracast-specific parameter ID group. */
#define MMAL_PARAMETER_GROUP_MIRACAST       (5<<16)

/**@}*/

/** Common MMAL parameter IDs.
 */
enum {
   MMAL_PARAMETER_UNUSED                  /**< Never a valid parameter ID */
         = MMAL_PARAMETER_GROUP_COMMON,
   MMAL_PARAMETER_SUPPORTED_ENCODINGS,    /**< Takes a MMAL_PARAMETER_ENCODING_T */
   MMAL_PARAMETER_URI,                    /**< Takes a MMAL_PARAMETER_URI_T */
   MMAL_PARAMETER_CHANGE_EVENT_REQUEST,   /**< Takes a MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T */
   MMAL_PARAMETER_ZERO_COPY,              /**< Takes a MMAL_PARAMETER_BOOLEAN_T */
   MMAL_PARAMETER_BUFFER_REQUIREMENTS,    /**< Takes a MMAL_PARAMETER_BUFFER_REQUIREMENTS_T */
   MMAL_PARAMETER_STATISTICS,             /**< Takes a MMAL_PARAMETER_STATISTICS_T */
   MMAL_PARAMETER_CORE_STATISTICS,        /**< Takes a MMAL_PARAMETER_CORE_STATISTICS_T */
   MMAL_PARAMETER_MEM_USAGE,              /**< Takes a MMAL_PARAMETER_MEM_USAGE_T */
   MMAL_PARAMETER_BUFFER_FLAG_FILTER,     /**< Takes a MMAL_PARAMETER_UINT32_T */
   MMAL_PARAMETER_SEEK,                   /**< Takes a MMAL_PARAMETER_SEEK_T */
   MMAL_PARAMETER_POWERMON_ENABLE,        /**< Takes a MMAL_PARAMETER_BOOLEAN_T */
   MMAL_PARAMETER_LOGGING,                /**< Takes a MMAL_PARAMETER_LOGGING_T */
   MMAL_PARAMETER_SYSTEM_TIME,            /**< Takes a MMAL_PARAMETER_UINT64_T */
   MMAL_PARAMETER_NO_IMAGE_PADDING,       /**< Takes a MMAL_PARAMETER_BOOLEAN_T */
   MMAL_PARAMETER_LOCKSTEP_ENABLE         /**< Takes a MMAL_PARAMETER_BOOLEAN_T */
};

/**@}*/

/** Parameter header type. All parameter structures need to begin with this type.
 * The \ref id field must be set to a parameter ID, such as one of those listed on
 * the \ref MMAL_PARAMETER_IDS "Pre-defined MMAL parameter IDs" page.
 */
typedef struct MMAL_PARAMETER_HEADER_T
{
   uint32_t id;      /**< Parameter ID. */
   uint32_t size;    /**< Size in bytes of the parameter (including the header) */
} MMAL_PARAMETER_HEADER_T;

/** Change event request parameter type.
 * This is used to control whether a \ref MMAL_EVENT_PARAMETER_CHANGED_T event
 * is issued should a given parameter change.
 */
typedef struct MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   uint32_t change_id;  /**< ID of parameter that may change, see \ref MMAL_PARAMETER_IDS */
   MMAL_BOOL_T enable;  /**< True if the event is enabled, false if disabled */
} MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T;

/** Buffer requirements parameter.
 * This is mainly used to increase the requirements of a component. */
typedef struct MMAL_PARAMETER_BUFFER_REQUIREMENTS_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   uint32_t buffer_num_min;          /**< Minimum number of buffers the port requires */
   uint32_t buffer_size_min;         /**< Minimum size of buffers the port requires */
   uint32_t buffer_alignment_min;    /**< Minimum alignment requirement for the buffers.
                                          A value of zero means no special alignment requirements. */
   uint32_t buffer_num_recommended;  /**< Number of buffers the port recommends for optimal performance.
                                          A value of zero means no special recommendation. */
   uint32_t buffer_size_recommended; /**< Size of buffers the port recommends for optimal performance.
                                          A value of zero means no special recommendation. */
} MMAL_PARAMETER_BUFFER_REQUIREMENTS_T;

/** Seek request parameter type.
 * This is used to issue a seek request to a source component.
 */
typedef struct MMAL_PARAMETER_SEEK_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   int64_t offset;  /**< Offset (in microseconds) to seek to */
   uint32_t flags;  /**< Seeking flags */

#define MMAL_PARAM_SEEK_FLAG_PRECISE 0x1 /**< Choose precise seeking even if slower */
#define MMAL_PARAM_SEEK_FLAG_FORWARD 0x2 /**< Seek to the next keyframe following the specified offset */

} MMAL_PARAMETER_SEEK_T;

/** Port statistics for debugging/test purposes.
 * Ports may support query of this parameter to return statistics for debugging or
 * test purposes. Not all values may be relevant for a given port.
 */
typedef struct MMAL_PARAMETER_STATISTICS_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   uint32_t buffer_count;           /**< Total number of buffers processed */
   uint32_t frame_count;            /**< Total number of frames processed */
   uint32_t frames_skipped;         /**< Number of frames without expected PTS based on frame rate */
   uint32_t frames_discarded;       /**< Number of frames discarded */
   uint32_t eos_seen;               /**< Set if the end of stream has been reached */
   uint32_t maximum_frame_bytes;    /**< Maximum frame size in bytes */
   int64_t  total_bytes;            /**< Total number of bytes processed */
   uint32_t corrupt_macroblocks;    /**< Number of corrupt macroblocks in the stream */
} MMAL_PARAMETER_STATISTICS_T;

typedef enum
{
   MMAL_CORE_STATS_RX,
   MMAL_CORE_STATS_TX,
   MMAL_CORE_STATS_MAX = 0x7fffffff /* Force 32 bit size for this enum */
} MMAL_CORE_STATS_DIR;

/** MMAL core statistics. These are collected by the core itself.
 */
typedef struct MMAL_PARAMETER_CORE_STATISTICS_T
{
   MMAL_PARAMETER_HEADER_T hdr;
   MMAL_CORE_STATS_DIR dir;
   MMAL_BOOL_T reset;               /**< Reset to zero after reading */
   MMAL_CORE_STATISTICS_T stats;    /**< The statistics */
} MMAL_PARAMETER_CORE_STATISTICS_T;

/**
 * Component memory usage statistics.
 */
typedef struct MMAL_PARAMETER_MEM_USAGE_T
{
   MMAL_PARAMETER_HEADER_T hdr;
   /**< The amount of memory allocated in image pools by the component */
   uint32_t pool_mem_alloc_size;
} MMAL_PARAMETER_MEM_USAGE_T;

/**
 * Logging control.
 */
typedef struct MMAL_PARAMETER_LOGGING_T
{
   MMAL_PARAMETER_HEADER_T hdr;
   uint32_t set;     /**< Logging bits to set */
   uint32_t clear;   /**< Logging bits to clear */
} MMAL_PARAMETER_LOGGING_T;

#endif /* MMAL_PARAMETERS_COMMON_H */
