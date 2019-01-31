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

#ifndef MMAL_BUFFER_H
#define MMAL_BUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

/** \defgroup MmalBufferHeader Buffer headers
 * Definition of a buffer header and its associated API.
 * Buffer headers are the basic element used to pass data and information between different
 * parts of the system. They are passed to components via ports and sent back to the client
 * using a callback mechanism.
 */
/* @{ */

/** Specific data associated with video frames */
typedef struct {
   uint32_t planes;     /**< Number of planes composing the video frame */
   uint32_t offset[4];  /**< Offsets to the different planes. These must point within the
                             payload buffer */
   uint32_t pitch[4];   /**< Pitch (size in bytes of a line of a plane) of the different
                             planes */
   uint32_t flags;      /**< Flags describing video specific properties of a buffer header
                             (see \ref videobufferheaderflags "Video buffer header flags") */
   /* TBD stereoscopic support */
} MMAL_BUFFER_HEADER_VIDEO_SPECIFIC_T;

/** Type specific data that's associated with a payload buffer */
typedef union
{
   /** Specific data associated with video frames */
   MMAL_BUFFER_HEADER_VIDEO_SPECIFIC_T video;

} MMAL_BUFFER_HEADER_TYPE_SPECIFIC_T;

/** Definition of the buffer header structure.
 * A buffer header does not directly carry the data to be passed to a component but instead
 * it references the actual data using a pointer (and an associated length).
 * It also contains an internal area which can be used to store command to be associated
 * with the external data.
 */
typedef struct MMAL_BUFFER_HEADER_T
{
   struct MMAL_BUFFER_HEADER_T *next; /**< Used to link several buffer headers together */

   struct MMAL_BUFFER_HEADER_PRIVATE_T *priv; /**< Data private to the framework */

   uint32_t cmd;              /**< Defines what the buffer header contains. This is a FourCC
                                   with 0 as a special value meaning stream data */

   uint8_t  *data;            /**< Pointer to the start of the payload buffer (should not be
                                   changed by component) */
   uint32_t alloc_size;       /**< Allocated size in bytes of payload buffer */
   uint32_t length;           /**< Number of bytes currently used in the payload buffer (starting
                                   from offset) */
   uint32_t offset;           /**< Offset in bytes to the start of valid data in the payload buffer */

   uint32_t flags;            /**< Flags describing properties of a buffer header (see
                                   \ref bufferheaderflags "Buffer header flags") */

   int64_t  pts;              /**< Presentation timestamp in microseconds. \ref MMAL_TIME_UNKNOWN
                                   is used when the pts is unknown. */
   int64_t  dts;              /**< Decode timestamp in microseconds (dts = pts, except in the case
                                   of video streams with B frames). \ref MMAL_TIME_UNKNOWN
                                   is used when the dts is unknown. */

   /** Type specific data that's associated with a payload buffer */
   MMAL_BUFFER_HEADER_TYPE_SPECIFIC_T *type;

   void *user_data;           /**< Field reserved for use by the client */

} MMAL_BUFFER_HEADER_T;

/** \name Buffer header flags
 * \anchor bufferheaderflags
 * The following flags describe properties of a buffer header */
/* @{ */
/** Signals that the current payload is the end of the stream of data */
#define MMAL_BUFFER_HEADER_FLAG_EOS                    (1<<0)
/** Signals that the start of the current payload starts a frame */
#define MMAL_BUFFER_HEADER_FLAG_FRAME_START            (1<<1)
/** Signals that the end of the current payload ends a frame */
#define MMAL_BUFFER_HEADER_FLAG_FRAME_END              (1<<2)
/** Signals that the current payload contains only complete frames (1 or more) */
#define MMAL_BUFFER_HEADER_FLAG_FRAME                  (MMAL_BUFFER_HEADER_FLAG_FRAME_START|MMAL_BUFFER_HEADER_FLAG_FRAME_END)
/** Signals that the current payload is a keyframe (i.e. self decodable) */
#define MMAL_BUFFER_HEADER_FLAG_KEYFRAME               (1<<3)
/** Signals a discontinuity in the stream of data (e.g. after a seek).
 * Can be used for instance by a decoder to reset its state */
#define MMAL_BUFFER_HEADER_FLAG_DISCONTINUITY          (1<<4)
/** Signals a buffer containing some kind of config data for the component
 * (e.g. codec config data) */
#define MMAL_BUFFER_HEADER_FLAG_CONFIG                 (1<<5)
/** Signals an encrypted payload */
#define MMAL_BUFFER_HEADER_FLAG_ENCRYPTED              (1<<6)
/** Signals a buffer containing side information */
#define MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO          (1<<7)
/** Signals a buffer which is the snapshot/postview image from a stills capture */
#define MMAL_BUFFER_HEADER_FLAGS_SNAPSHOT              (1<<8)
/** Signals a buffer which contains data known to be corrupted */
#define MMAL_BUFFER_HEADER_FLAG_CORRUPTED              (1<<9)
/** Signals that a buffer failed to be transmitted */
#define MMAL_BUFFER_HEADER_FLAG_TRANSMISSION_FAILED    (1<<10)
/** Signals the output buffer won't be used, just update reference frames */
#define MMAL_BUFFER_HEADER_FLAG_DECODEONLY             (1<<11)
/** Signals that the end of the current payload ends a NAL */
#define MMAL_BUFFER_HEADER_FLAG_NAL_END                (1<<12)
/** User flags - can be passed in and will get returned */
#define MMAL_BUFFER_HEADER_FLAG_USER0                  (1<<28)
#define MMAL_BUFFER_HEADER_FLAG_USER1                  (1<<29)
#define MMAL_BUFFER_HEADER_FLAG_USER2                  (1<<30)
#define MMAL_BUFFER_HEADER_FLAG_USER3                  (1<<31)
/* @} */

/** \name Video buffer header flags
 * \anchor videobufferheaderflags
 * The following flags describe properties of a video buffer header.
 * As there is no collision with the MMAL_BUFFER_HEADER_FLAGS_ defines, these
 * flags will also be present in the MMAL_BUFFER_HEADER_T flags field.
 */
#define MMAL_BUFFER_HEADER_FLAG_FORMAT_SPECIFIC_START_BIT 16
#define MMAL_BUFFER_HEADER_FLAG_FORMAT_SPECIFIC_START (1<<MMAL_BUFFER_HEADER_FLAG_FORMAT_SPECIFIC_START_BIT)
/* @{ */
/** 16: Signals an interlaced video frame */
#define MMAL_BUFFER_HEADER_VIDEO_FLAG_INTERLACED       (MMAL_BUFFER_HEADER_FLAG_FORMAT_SPECIFIC_START<<0)
/** 17: Signals that the top field of the current interlaced frame should be displayed first */
#define MMAL_BUFFER_HEADER_VIDEO_FLAG_TOP_FIELD_FIRST  (MMAL_BUFFER_HEADER_FLAG_FORMAT_SPECIFIC_START<<1)
/** 19: Signals that the buffer should be displayed on external display if attached. */
#define MMAL_BUFFER_HEADER_VIDEO_FLAG_DISPLAY_EXTERNAL (MMAL_BUFFER_HEADER_FLAG_FORMAT_SPECIFIC_START<<3)
/** 20: Signals that contents of the buffer requires copy protection. */
#define MMAL_BUFFER_HEADER_VIDEO_FLAG_PROTECTED        (MMAL_BUFFER_HEADER_FLAG_FORMAT_SPECIFIC_START<<4)
/** 27-24: If non-zero it signals the video frame is encoded in column mode,
 * with a column width equal to 2^<masked value>.
 * Zero is raster order. */
#define MMAL_BUFFER_HEADER_VIDEO_FLAG_COLUMN_LOG2_SHIFT (MMAL_BUFFER_HEADER_FLAG_FORMAT_SPECIFIC_START_BIT+8)
#define MMAL_BUFFER_HEADER_VIDEO_FLAG_COLUMN_LOG2_MASK (0xF<<MMAL_BUFFER_HEADER_VIDEO_FLAG_COLUMN_LOG2_SHIFT)
/* @} */

/** Acquire a buffer header.
 * Acquiring a buffer header increases a reference counter on it and makes sure that the
 * buffer header won't be recycled until all the references to it are gone.
 * This is useful for instance if a component needs to return a buffer header but still needs
 * access to it for some internal processing (e.g. reference frames in video codecs).
 *
 * @param header buffer header to acquire
 */
void mmal_buffer_header_acquire(MMAL_BUFFER_HEADER_T *header);

/** Reset a buffer header.
 * Resets all header variables to default values.
 *
 * @param header buffer header to reset
 */
void mmal_buffer_header_reset(MMAL_BUFFER_HEADER_T *header);

/** Release a buffer header.
 * Releasing a buffer header will decrease its reference counter and when no more references
 * are left, the buffer header will be recycled by calling its 'release' callback function.
 *
 * If a pre-release callback is set (\ref MMAL_BH_PRE_RELEASE_CB_T), this will be invoked
 * before calling the buffer's release callback and potentially postpone buffer recycling.
 * Once pre-release is complete the buffer header is recycled with
 * \ref mmal_buffer_header_release_continue.
 *
 * @param header buffer header to release
 */
void mmal_buffer_header_release(MMAL_BUFFER_HEADER_T *header);

/** Continue the buffer header release process.
 * This should be called to complete buffer header recycling once all pre-release activity
 * has been completed.
 *
 * @param header buffer header to release
 */
void mmal_buffer_header_release_continue(MMAL_BUFFER_HEADER_T *header);

/** Buffer header pre-release callback.
 * The callback is invoked just before a buffer is released back into a
 * pool. This is used by clients who need to trigger additional actions
 * before the buffer can finally be released (e.g. wait for a bulk transfer
 * to complete).
 *
 * The callback should return TRUE if the buffer release need to be post-poned.
 *
 * @param header   buffer header about to be released
 * @param userdata user-specific data
 *
 * @return TRUE if the buffer should not be released
 */
typedef MMAL_BOOL_T (*MMAL_BH_PRE_RELEASE_CB_T)(MMAL_BUFFER_HEADER_T *header, void *userdata);

/** Set a buffer header pre-release callback.
 * If the callback is NULL, the buffer will be released back into the pool
 * immediately as usual.
 *
 * @param header   buffer header to associate callback with
 * @param cb       pre-release callback to invoke
 * @param userdata user-specific data
 */
void mmal_buffer_header_pre_release_cb_set(MMAL_BUFFER_HEADER_T *header, MMAL_BH_PRE_RELEASE_CB_T cb, void *userdata);

/** Replicate a buffer header into another one.
 * Replicating a buffer header will not only do an exact copy of all the public fields of the
 * buffer header (including data and alloc_size), but it will also acquire a reference to the
 * source buffer header which will only be released once the replicate has been released.
 *
 * @param dest buffer header into which to replicate
 * @param src buffer header to use as the source for the replication
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_buffer_header_replicate(MMAL_BUFFER_HEADER_T *dest, MMAL_BUFFER_HEADER_T *src);

/** Lock the data buffer contained in the buffer header in memory.
 * This call does nothing on all platforms except VideoCore where it is needed to pin a
 * buffer in memory before any access to it.
 *
 * @param header buffer header to lock
 */
MMAL_STATUS_T mmal_buffer_header_mem_lock(MMAL_BUFFER_HEADER_T *header);

/** Unlock the data buffer contained in the buffer header.
 * This call does nothing on all platforms except VideoCore where it is needed to un-pin a
 * buffer in memory after any access to it.
 *
 * @param header buffer header to unlock
 */
void mmal_buffer_header_mem_unlock(MMAL_BUFFER_HEADER_T *header);

/* @} */

#ifdef __cplusplus
}
#endif

#endif /* MMAL_BUFFER_H */
