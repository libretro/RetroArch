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

#ifndef IMAGECONV_H
#define IMAGECONV_H

#include "interface/vcos/vcos.h"
#include "interface/vctypes/vc_image_types.h"
#include "vcfw/rtos/common/rtos_common_mem.h"
#include "interface/vmcs_host/vc_imageconv_defs.h"

typedef struct
{
   void *data;
   /** Width of the converted image in pixels */
   uint32_t width;
   /** Height of the converted image in pixels */
   uint32_t height;
   /** Cropped width of the converted image in pixels */
   uint32_t crop_width;
   /** Cropped height of the converted image in pixels */
   uint32_t crop_height;
   /** The pitch of the converted image */
   uint32_t pitch;
   /** The size required for the converted image */
   uint32_t size;
   /** The type of the converted image */
   VC_IMAGE_TYPE_T type;
   /** Whether the destination has been sub-sampled */
   uint32_t subsampled;
   /** Non-zero = image is V/U interleaved */
   uint32_t is_vu;
   /** Vertical pitch of the buffer */
   uint32_t vpitch;
} IMAGECONV_IMAGE_DATA_T;

typedef struct IMAGECONV_DRIVER_IMAGE_T IMAGECONV_DRIVER_IMAGE_T;

/** Converter class */
typedef struct IMAGE_CONVERT_CLASS_T IMAGE_CONVERT_CLASS_T;

/**
 * Create an image.
 * The initial reference count is 1.
 * @param data     Image data. Not all fields are used by all convert classes. 'data' must be
 *                 filled in for all classes.
 * @param handle   On success, this points to the newly created image handle.
 * @return 0 on success; -1 on failure.
 */
typedef int (*IMAGECONV_CREATE)(const IMAGE_CONVERT_CLASS_T *converter,
                                const IMAGECONV_IMAGE_DATA_T *data, MEM_HANDLE_T *handle);

typedef int (*IMAGECONV_GET_SIZE)(const IMAGE_CONVERT_CLASS_T *converter,
                                  const IMAGECONV_DRIVER_IMAGE_T *image, uint32_t *width,
                                  uint32_t *height, uint32_t *pitch, VC_IMAGE_TYPE_T *type);

/** Get an image which can be used in one of the other conversion function,
 * given a mem handle.
 */
typedef int (*IMAGECONV_GET_IMAGE)(const IMAGE_CONVERT_CLASS_T *converter, MEM_HANDLE_T src,
                                   IMAGECONV_DRIVER_IMAGE_T **image);

/** Gets the dimensions and allocation size of the destination image
 * required by IMAGECONV_CONVERT.
 *
 * Must NOT be NULL.
 *
 * @return 0 if successful
 */
typedef int (*IMAGECONV_GET_CONVERTED_SIZE)(
      const IMAGE_CONVERT_CLASS_T *converter, MEM_HANDLE_T src,
      VC_IMAGE_TYPE_T type, IMAGECONV_IMAGE_DATA_T *dest_info);

/*
 * Tell the converter that we've finished with the image for now (so it can
 * unlock it etc.).
 *
 * May be NULL if the converter does not support or require this.
 */
typedef void (*IMAGECONV_UNGET_IMAGE)(const IMAGE_CONVERT_CLASS_T *converter,
                                      MEM_HANDLE_T src,
                                      IMAGECONV_DRIVER_IMAGE_T *image);

/*
 * Add a reference count to an image, preventing it from being returned to its
 * image pool.
 *
 * May be NULL if the converter does not support or require this.
 */
typedef int (*IMAGECONV_ACQUIRE_IMAGE)(const IMAGE_CONVERT_CLASS_T *converter,
                                       IMAGECONV_DRIVER_IMAGE_T *image);

/*
 * Remove a reference count to an image, releasing it back to its
 * image pool.
 *
 * May be NULL if the converter does not support or require this.
 */
typedef void (*IMAGECONV_RELEASE_IMAGE)(const IMAGE_CONVERT_CLASS_T *converter,
                                        IMAGECONV_DRIVER_IMAGE_T *image);

/* mem-lock an image and return the data pointer(s).
 */
typedef int (*IMAGECONV_LOCK)(const IMAGE_CONVERT_CLASS_T *converter,
                              IMAGECONV_DRIVER_IMAGE_T *image, IMAGECONV_IMAGE_DATA_T *data);

/* mem-unlock an image.
 */
typedef void (*IMAGECONV_UNLOCK)(const IMAGE_CONVERT_CLASS_T *converter,
                                 IMAGECONV_DRIVER_IMAGE_T *image);

/* Convert src to dest */
typedef int (*IMAGECONV_CONVERT)(const IMAGE_CONVERT_CLASS_T *converter,
      MEM_HANDLE_T dest, uint32_t dest_offset,
      MEM_HANDLE_T src, uint32_t src_offset, VC_IMAGE_TYPE_T dest_type);

/** Set the description of the source image data. Optional function to aid memory debugging.
 * This envisages the source image data is in the relocatable heap, so will call mem_set_desc_vprintf() to
 * set the description.
 *
 * May be NULL if the converter does not support or require this.
 */
typedef void (*IMAGECONV_SET_SRC_DESC)(const IMAGE_CONVERT_CLASS_T *converter, MEM_HANDLE_T src,
                                       const char *fmt, va_list ap);

/** Get the description of the source image data. Optional function to aid memory debugging.
 * This envisages the source image data is in the relocatable heap, so will call mem_get_desc() to
 * get the description.
 *
 * May be NULL if the converter does not support or require this.
 */
typedef const char *(*IMAGECONV_GET_SRC_DESC)(const IMAGE_CONVERT_CLASS_T *converter, MEM_HANDLE_T src);

/* Callback made in response to IMAGECONV_NOTIFY
 *
 * A status of 0 indicates that a reference to the image can now be acquired
 * via imageconv_acquire(_image).
 *
 * Any other value indicates that the request timed out.
 */
typedef void (*IMAGECONV_NOTIFY_CALLBACK)(const IMAGE_CONVERT_CLASS_T *converter,
                                          IMAGECONV_DRIVER_IMAGE_T *image,
                                          void *context,
                                          int status);

/* Request notification when a reference to the image can be acquired.
 *
 * May be NULL if the converter does not support or require this.
 */
typedef int (*IMAGECONV_NOTIFY)(const IMAGE_CONVERT_CLASS_T *converter,
                                IMAGECONV_DRIVER_IMAGE_T *image,
                                uint32_t timeout,
                                IMAGECONV_NOTIFY_CALLBACK callback,
                                void *context);

/* Cancel an outstanding IMAGECONV_NOTIFY request.
*/
typedef void (*IMAGECONV_CANCEL_NOTIFY)(const IMAGE_CONVERT_CLASS_T *converter,
                                        IMAGECONV_DRIVER_IMAGE_T *image);

typedef enum IMAGECONV_ID_T {
    IMAGECONV_ID_MMAL,        /**< MMAL opaque buffer to Khronos */
    IMAGECONV_ID_KHRONOS,     /**< YV12 to Khronos */
    IMAGECONV_ID_EGL_VC,      /**< EGL image to VC_IMAGE_T */
    IMAGECONV_ID_GENERIC,
    IMAGECONV_ID_KHRONOS_VC,  /**< Khronos to VC_IMAGE_T */
    IMAGECONV_ID_MAX
} IMAGECONV_ID_T;

/* Do not call these directly! */
struct IMAGE_CONVERT_CLASS_T {
    IMAGECONV_CREATE                create;
    IMAGECONV_GET_SIZE              get_size;
    IMAGECONV_GET_IMAGE             get;
    IMAGECONV_GET_CONVERTED_SIZE    get_converted_size;
    IMAGECONV_UNGET_IMAGE           unget;
    IMAGECONV_ACQUIRE_IMAGE         acquire;
    IMAGECONV_RELEASE_IMAGE         release;
    IMAGECONV_LOCK                  lock;
    IMAGECONV_UNLOCK                unlock;
    IMAGECONV_CONVERT               convert;
    IMAGECONV_SET_SRC_DESC          set_src_desc;
    IMAGECONV_GET_SRC_DESC          get_src_desc;
    IMAGECONV_NOTIFY                notify;
    IMAGECONV_CANCEL_NOTIFY         cancel_notify;
    IMAGECONV_ID_T                  id;
};

typedef enum IMAGECONV_ERR_T {
    IMAGECONV_ERR_NONE = 0,
    IMAGECONV_ERR_GENERAL = -1,
    IMAGECONV_ERR_NOT_SUPPORTED = -2,
    IMAGECONV_ERR_NOT_READY = -3,
    IMAGECONV_ERR_ALREADY = -4,
} IMAGECONV_ERR_T;

/** Initialise the library
 */
void imageconv_init(void);

/** Retrieve the conversion function routines for a given type of
 * image.
 *
 * @param converter_id           type of image to support
 * @param converter              function pointers for this image type
 *
 * @return IMAGECONV_ERR_NONE on success
 * @return IMAGECONV_ERR_NOT_SUPPORTED if image type unknown.
 */
int imageconv_get_convert_class(
        IMAGECONV_ID_T converter_id,
        const IMAGE_CONVERT_CLASS_T **converter);

/** Set the image conversion functions to use for a given type of
 * image.
 *
 * @param converter_class        type of image to support
 * @param converter              function pointers for this image type
 */
void imageconv_set_convert_class(IMAGECONV_ID_T converter_id,
      const IMAGE_CONVERT_CLASS_T *converter);

/**
 * Create an image.
 *
 * The initial reference count is 1.
 *
 * @param converter  Convert class.
 * @param data       Image data. Not all fields are used by all convert classes. 'data' must be
 *                   filled in for all classes.
 * @param handle     On success, this points to the newly created image handle. The actual type
 *                   depends on the convert class, but is probably a MEM_HANDLE_T in most cases.
 *
 * @return IMAGECONV_ERR_NOT_SUPPORTED if not implemented by the converter class
 * @return IMAGECONV_ERR_NONE on success
 * @return IMAGECONV_ERR_GENERAL on failure
 */
int imageconv_create(const IMAGE_CONVERT_CLASS_T *converter, const IMAGECONV_IMAGE_DATA_T *data,
                     MEM_HANDLE_T *handle);

/** Get the size of an image.
 *
 * @param converter    converter functions for this image
 * @param self   MEM_HANDLE to image structure.
 * @param width  width of image
 * @param height height of image
 * @param pitch  pitch of image in bytes
 * @param type   type of image
 *
 * @return IMAGECONV_ERR_NOT_SUPPORTED if not implemented by the converter class
 * @return IMAGECONV_ERR_NONE on success
 * @return IMAGECONV_ERR_GENERAL if image handle is invalid.
 */
int imageconv_get_size(const IMAGE_CONVERT_CLASS_T *converter, MEM_HANDLE_T self,
     uint32_t *width, uint32_t *height, uint32_t *pitch, VC_IMAGE_TYPE_T *type);

/** Converts the image writing the result to the memory specified at
 * dst, dst_offset.
 *
 * @return IMAGECONV_ERR_NOT_SUPPORTED if not implemented by the converter class
 * @return IMAGECONV_ERR_NONE on success
 * @return IMAGECONV_ERR_GENERAL on failure
 */
int imageconv_convert(const IMAGE_CONVERT_CLASS_T *converter,
                      MEM_HANDLE_T dest, uint32_t dest_offset,
                      MEM_HANDLE_T src, uint32_t src_offset,
                      VC_IMAGE_TYPE_T dest_type);

/** Convert a MEM_HANDLE_T handle to an actual image pointer (typically by
 * calling mem_lock() but possibly involving other operations). The image
 * must be unlocked later with imageconv_unget().
 *
 * @param converter    converter functions for this image
 * @param src          MEM_HANDLE_T to image structure
 * @param image        Return image pointer.
 *
 * @return IMAGECONV_ERR_NONE on success
 * @return IMAGECONV_ERR_GENERAL if image handle is invalid.
 */
int imageconv_get(const IMAGE_CONVERT_CLASS_T *converter,
                  MEM_HANDLE_T src,
                  IMAGECONV_DRIVER_IMAGE_T **image);

/* Gets the dimensions of the image that will be created if the image
 * is converted.
 *
 * @param converter    converter functions for this image
 * @param src          MEM_HANDLE_T to image structure
 * @param type         The desired type of the destination image.
 * @param dest_info    A pointer to the image data structure which will be
 *                     updated with the image metadata. The data pointer to
 *                     the image storage is not modified; all other fields
 *                     will be filled in.
 *
 * @return IMAGECONV_ERR_NOT_SUPPORTED if not implemented by the converter class
 * @return IMAGECONV_ERR_NONE on success
 * @return IMAGECONV_ERR_GENERAL if image handle is invalid.
 */
int imageconv_get_converted_size(const IMAGE_CONVERT_CLASS_T *converter,
      MEM_HANDLE_T src, VC_IMAGE_TYPE_T type, IMAGECONV_IMAGE_DATA_T *dest_info);

/** Release an image that was previously taken with imageconv_get().
 *
 * @param converter    converter functions for this image
 * @param src          MEM_HANDLE_T to image structure
 * @param image        image obtained from imageconv_get
 */
void imageconv_unget(const IMAGE_CONVERT_CLASS_T *converter,
                     MEM_HANDLE_T src,
                     IMAGECONV_DRIVER_IMAGE_T *image);

/** Acquire a lock on an image to prevent it being recycled into its
 * image pool.
 *
 * @param converter    converter functions for this image
 * @param src          MEM_HANDLE_T to image structure
 *
 * @return IMAGECONV_ERR_NOT_SUPPORTED if not implemented by the converter class
 * @return IMAGECONV_ERR_NONE on success
 * @return IMAGECONV_ERR_GENERAL if image handle is invalid
 * @return IMAGECONV_ERR_NOT_READY if reference cannot be currently acquired
 */
int imageconv_acquire(const IMAGE_CONVERT_CLASS_T *converter,
                      MEM_HANDLE_T src);

/** Acquire a lock on an image to prevent it being recycled into its
 * image pool.
 *
 * @param converter    converter functions for this image
 * @param image        image pointer from imageconv_get
 *
 * @return IMAGECONV_ERR_NOT_SUPPORTED if not implemented by the converter class
 * @return IMAGECONV_ERR_NONE on success
 * @return IMAGECONV_ERR_GENERAL if image handle is invalid
 * @return IMAGECONV_ERR_NOT_READY if reference cannot be currently acquired
 */
int imageconv_acquire_image(const IMAGE_CONVERT_CLASS_T *converter,
                      IMAGECONV_DRIVER_IMAGE_T *image);

/** Release a lock on an image to allow it to be recycled back
 * to its image pool.
 *
 * @param converter    converter functions for this image
 * @param image        image pointer from imageconv_get
 *
 * @return IMAGECONV_ERR_NOT_SUPPORTED if not implemented by the converter class
 * @return IMAGECONV_ERR_NONE on success
 * @return IMAGECONV_ERR_GENERAL if image handle is invalid
 */
int imageconv_release_image(const IMAGE_CONVERT_CLASS_T *converter,
                      IMAGECONV_DRIVER_IMAGE_T *image);

/** Set the description of the source image data. Function to aid memory debugging; may not be
 * supported by every converter class (in which case it becomes a no-op).
 *
 * @param converter    converter functions for this image
 * @param src          MEM_HANDLE_T to source image
 * @param desc         Memory block description
 */
void imageconv_set_src_desc(const IMAGE_CONVERT_CLASS_T *converter, MEM_HANDLE_T src,
                            const char *desc);

/** Set the description of the source image data. Function to aid memory debugging; may not be
 * supported by every converter class (in which case it becomes a no-op).
 *
 * @param converter    converter functions for this image
 * @param src          MEM_HANDLE_T to source image
 * @param fmt,...      Memory block description
 */
void imageconv_set_src_desc_printf(const IMAGE_CONVERT_CLASS_T *converter, MEM_HANDLE_T src,
                                   const char *fmt, ...);

/** Get the description of the source image data. Function to aid memory debugging; may not be
 * supported by every converter class (in which case it returns NULL).
 * The returned string will be valid for the lifetime of the src handle, or
 * until a new description is set.
 *
 * @param converter    converter functions for this image
 * @param src          MEM_HANDLE_T to source image
 *
 * @return Memory block description, or NULL if unavailable
 */
const char *imageconv_get_src_desc(const IMAGE_CONVERT_CLASS_T *converter, MEM_HANDLE_T src);

/* Request notification when a reference to the image can be acquired.
 *
 * If the reference can already be acquired at the time when the request is made,
 * this is indicated via the return code, and no callback will be made.
 *
 * Otherwise the callback is made when the reference can be acquired, or when
 * the specified time period has elapsed.
 *
 * The thread context in which callbacks are executed depends upon the converter
 * class implementation.  The callback function should therefore do as little
 * work as possible, deferring to a known thread context for any further
 * processing.
 *
 * @param converter    converter functions for this image
 * @param image        image pointer
 * @param timeout      time to wait in milliseconds; 0 means wait indefinitely
 * @param callback     callback to be invoked when image is valid, or timeout expires
 *
 * @return IMAGECONV_ERR_NOT_SUPPORTED if not implemented by the converter class
 * @return IMAGECONV_ERR_ALREADY if reference can be acquired immediately
 * @return IMAGECONV_ERR_NONE if reference cannot be acquire immediately; callback will be made
 */
int imageconv_notify(const IMAGE_CONVERT_CLASS_T *converter,
                     IMAGECONV_DRIVER_IMAGE_T *image,
                     uint32_t timeout,
                     IMAGECONV_NOTIFY_CALLBACK callback,
                     void *context);

/* Cancel an outstanding IMAGECONV_NOTIFY request.
 *
 * @param converter    converter functions for this image
 * @param image        return image pointer
 */
void imageconv_cancel_notify(const IMAGE_CONVERT_CLASS_T *converter,
                             IMAGECONV_DRIVER_IMAGE_T *image);

#endif
