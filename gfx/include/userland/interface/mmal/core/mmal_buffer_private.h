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

#ifndef MMAL_BUFFER_PRIVATE_H
#define MMAL_BUFFER_PRIVATE_H

/** Typedef for the private area the framework reserves for the driver / communication layer */
typedef struct MMAL_DRIVER_BUFFER_T MMAL_DRIVER_BUFFER_T;

/** Size of the private area the framework reserves for the driver / communication layer */
#define MMAL_DRIVER_BUFFER_SIZE 32

/** Typedef for the framework's private area in the buffer header */
typedef struct MMAL_BUFFER_HEADER_PRIVATE_T
{
   /** Callback invoked just prior to actually releasing the buffer header. Returns TRUE if
    * release should be delayed. */
   MMAL_BH_PRE_RELEASE_CB_T pf_pre_release;
   void *pre_release_userdata;

   /** Callback used to release / recycle the buffer header. This needs to be set by
    * whoever allocates the buffer header. */
   void (*pf_release)(struct MMAL_BUFFER_HEADER_T *header);
   void *owner;               /**< Context set by the allocator of the buffer header and passed
                                   during the release callback */

   int32_t refcount;          /**< Reference count of the buffer header. When it reaches 0,
                                   the release callback will be called. */

   MMAL_BUFFER_HEADER_T *reference; /**< Reference to another acquired buffer header. */

   /** Callback used to free the payload associated with this buffer header. This is only
    * used if the buffer header was created by MMAL with a payload associated with it. */
   void   (*pf_payload_free)(void *payload_context, void *payload);
   void    *payload;          /**< Pointer / handle to the allocated payload buffer */
   void    *payload_context;  /**< Pointer to the context of the payload allocator */
   uint32_t payload_size;     /**< Allocated size in bytes of payload buffer */

   void *component_data;      /**< Field reserved for use by the component */
   void *payload_handle;      /**< Field reserved for mmal_buffer_header_mem_lock */

   uint8_t driver_area[MMAL_DRIVER_BUFFER_SIZE];

} MMAL_BUFFER_HEADER_PRIVATE_T;

/** Get the size in bytes of a fully initialised MMAL_BUFFER_HEADER_T */
unsigned int mmal_buffer_header_size(MMAL_BUFFER_HEADER_T *header);

/** Initialise a MMAL_BUFFER_HEADER_T */
MMAL_BUFFER_HEADER_T *mmal_buffer_header_initialise(void *mem, unsigned int length);

/** Return a pointer to the area reserved for the driver.
  */
MMAL_DRIVER_BUFFER_T *mmal_buffer_header_driver_data(MMAL_BUFFER_HEADER_T *);

/** Return a pointer to a referenced buffer header.
 * It is the caller's responsibility to ensure that the reference is still
 * valid when using it.
 */
MMAL_BUFFER_HEADER_T *mmal_buffer_header_reference(MMAL_BUFFER_HEADER_T *header);

#endif /* MMAL_BUFFER_PRIVATE_H */
