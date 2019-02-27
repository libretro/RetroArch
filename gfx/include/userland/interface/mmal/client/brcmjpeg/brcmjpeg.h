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

/** \file
 *  Jpeg encoder and decoder library using the hardware jpeg codec
 */

#ifndef BRCM_JPEG_H
#define BRCM_JPEG_H

#ifdef __cplusplus
extern "C" {
#endif

/** Status return codes from the API */
typedef enum
{
   BRCMJPEG_SUCCESS = 0,
   BRCMJPEG_ERROR_NOMEM,
   BRCMJPEG_ERROR_INIT,
   BRCMJPEG_ERROR_INPUT_FORMAT,
   BRCMJPEG_ERROR_OUTPUT_FORMAT,
   BRCMJPEG_ERROR_INPUT_BUFFER,
   BRCMJPEG_ERROR_OUTPUT_BUFFER,
   BRCMJPEG_ERROR_EXECUTE,
   BRCMJPEG_ERROR_REQUEST,
} BRCMJPEG_STATUS_T;

/** Type of the codec instance to create */
typedef enum
{
   BRCMJPEG_TYPE_ENCODER = 0,
   BRCMJPEG_TYPE_DECODER
} BRCMJPEG_TYPE_T;

/** Pixel formats supported by the codec */
typedef enum
{
   PIXEL_FORMAT_UNKNOWN = 0,
   PIXEL_FORMAT_I420, /* planar YUV 4:2:0 */
   PIXEL_FORMAT_YV12, /* planar YVU 4:2:0 */
   PIXEL_FORMAT_I422, /* planar YUV 4:2:2 */
   PIXEL_FORMAT_YV16, /* planar YVU 4:2:2 */
   PIXEL_FORMAT_YUYV, /* interleaved YUV 4:2:2 */
   PIXEL_FORMAT_RGBA, /* interleaved RGBA */
} BRCMJPEG_PIXEL_FORMAT_T;

/** Definition of a codec request  */
typedef struct
{
   /** Pointer to the buffer containing the input data
    * A client should set input OR input_handle, but not both. */
   const unsigned char *input;
   /** Actual size of the input data */
   unsigned int input_size;
   /** Handle to input buffer containing input data */
   unsigned int input_handle;

   /** Pointer to the buffer used for the output data
     * A client should set output OR output_handle, but not both. */
   unsigned char *output;
   /** Total size of the output buffer */
   unsigned int output_alloc_size;
   /** Actual size of the output data (this is an output parameter) */
   unsigned int output_size;
   /** Handle to the buffer used for the output data */
   unsigned int output_handle;

   /** Width of the raw frame (this is an input parameter for encode) */
   unsigned int width;
   /** Height of the raw frame (this is an input parameter for encode) */
   unsigned int height;
   /** Pixel format of the raw frame (this is an input parameter) */
   BRCMJPEG_PIXEL_FORMAT_T pixel_format;

   /** Width of the buffer containing the raw frame (input parameter).
     * This is optional but if set, is used to specify the actual width
     * of the buffer containing the raw frame */
   unsigned int buffer_width;
   /** Height of the buffer containing the raw frame (input parameter).
     * This is optional but if set, is used to specify the actual height
     * of the buffer containing the raw frame */
   unsigned int buffer_height;

   /** Encode quality - 0 to 100 */
   unsigned int quality;
} BRCMJPEG_REQUEST_T;

/** Type of the codec instance */
typedef struct BRCMJPEG_T BRCMJPEG_T;

/** Create an instance of the jpeg codec
 * This will actually re-use an existing instance if one is
 * available.
 *
 * @param type type of codec instance required
 * @param ctx will point to the newly created instance
 * @return BRCMJPEG_SUCCESS on success
 */
BRCMJPEG_STATUS_T brcmjpeg_create(BRCMJPEG_TYPE_T type, BRCMJPEG_T **ctx);

/** Acquire a new reference on a codec instance
 *
 * @param ctx instance to acquire a reference on
 */
void brcmjpeg_acquire(BRCMJPEG_T *ctx);

/** Release an instance of the jpeg codec
 * This will only trigger the destruction of the codec instance when
 * the last reference to it is being released.
 *
 * @param ctx instance to release
 */
void brcmjpeg_release(BRCMJPEG_T *ctx);

/** Process a jpeg codec request
 *
 * @param ctx instance of codec to use
 * @param request codec request to execute
 * @return BRCMJPEG_SUCCESS on success
 */
BRCMJPEG_STATUS_T brcmjpeg_process(BRCMJPEG_T *ctx, BRCMJPEG_REQUEST_T *request);

#ifdef __cplusplus
}
#endif

#endif /* BRCM_JPEG_H */
