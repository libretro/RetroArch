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
#ifndef VC_CONTAINERS_IO_H
#define VC_CONTAINERS_IO_H

/** \file containers_io.h
 * Interface definition for the input / output abstraction layer used by container
 * readers and writers */
#include "containers/containers.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \defgroup VcContainerIoApi Container I/O API */
/* @{ */

/** Container io opening mode.
 * This is used to specify whether a reader or writer is requested.
 */
typedef enum {
   VC_CONTAINER_IO_MODE_READ = 0,  /**< Container io opened in reading mode */
   VC_CONTAINER_IO_MODE_WRITE = 1  /**< Container io opened in writing mode */
} VC_CONTAINER_IO_MODE_T;

/** \name Container I/O Capabilities
 * The following flags are exported by container i/o modules to describe their capabilities */
/* @{ */
/** Type definition for container I/O capabilities */
typedef uint32_t VC_CONTAINER_IO_CAPABILITIES_T;
/** Seeking is not supported */
#define VC_CONTAINER_IO_CAPS_CANT_SEEK    0x1
/** Seeking is slow and should be avoided */
#define VC_CONTAINER_IO_CAPS_SEEK_SLOW    0x2
/** The I/O doesn't provide any caching of the data */
#define VC_CONTAINER_IO_CAPS_NO_CACHING   0x4
/* @} */

/** Container Input / Output Context.
 * This structure defines the context for a container io instance */
struct VC_CONTAINER_IO_T
{
   /** Pointer to information private to the container io instance */
   struct VC_CONTAINER_IO_PRIVATE_T *priv;

   /** Pointer to information private to the container io module */
   struct VC_CONTAINER_IO_MODULE_T *module;

   /** Uniform Resource Identifier for the stream to open.
    * This is a string encoded in UTF-8 which follows the syntax defined in
    * RFC2396 (http://tools.ietf.org/html/rfc2396). */
   char *uri;

   /** Pre-parsed URI */
   struct VC_URI_PARTS_T *uri_parts;

   /** Current offset into the i/o stream */
   int64_t offset;

   /** Current size of the i/o stream (0 if unknown). This size might grow during the
    * lifetime of the i/o instance (for instance when doing progressive download). */
   int64_t size;

   /** Capabilities of the i/o stream */
   VC_CONTAINER_IO_CAPABILITIES_T capabilities;

   /** Status of the i/o stream */
   VC_CONTAINER_STATUS_T status;

   /** Maximum size allowed for this i/o stream (0 if unknown). This is used during writing
    * to limit the size of the stream to below this value. */
   int64_t max_size;

   /** \note the following list of function pointers should not be used directly.
    * They defines the interface for implementing container io modules and are filled in
    * by the container modules themselves. */

   /** \private
    * Function pointer to close and free all resources allocated by a
    * container io module */
   VC_CONTAINER_STATUS_T (*pf_close)(struct VC_CONTAINER_IO_T *io);

   /** \private
    * Function pointer to read or skip data from container io module */
   size_t (*pf_read)(struct VC_CONTAINER_IO_T *io, void *buffer, size_t size);

   /** \private
    * Function pointer to write data to a container io module */
   size_t (*pf_write)(struct VC_CONTAINER_IO_T *io, const void *buffer, size_t size);

   /** \private
    * Function pointer to seek into a container io module */
   VC_CONTAINER_STATUS_T (*pf_seek)(struct VC_CONTAINER_IO_T *io, int64_t offset);

   /** \private
    * Function pointer to perform a control operation on a container io module */
   VC_CONTAINER_STATUS_T (*pf_control)(struct VC_CONTAINER_IO_T *io,
                                       VC_CONTAINER_CONTROL_T operation, va_list args);

};

/** Opens an i/o stream pointed to by a URI.
 * This will create an instance of the container i/o module.
 *
 * \param  uri         Uniform Resource Identifier pointing to the multimedia container
 * \param  mode        Mode in which the i/o stream will be opened
 * \param  status      Returns the status of the operation
 * \return             If successful, this returns a pointer to the new instance
 *                     of the i/o module. Returns NULL on failure.
 */
VC_CONTAINER_IO_T *vc_container_io_open( const char *uri, VC_CONTAINER_IO_MODE_T mode,
                                         VC_CONTAINER_STATUS_T *status );

/** Creates an empty i/o stream. The i/o function pointers will have to be set
 * by the caller before the i/o gets used.
 * This will create an instance of the container i/o module.
 *
 * \param  uri         Uniform Resource Identifier pointing to the multimedia container
 * \param  mode        Mode in which the i/o stream will be opened
 * \param  capabilities Flags indicating the capabilities of the i/o
 * \param  status      Returns the status of the operation
 * \return             If successful, this returns a pointer to the new instance
 *                     of the i/o module. Returns NULL on failure.
 */
VC_CONTAINER_IO_T *vc_container_io_create( const char *uri, VC_CONTAINER_IO_MODE_T mode,
                                           VC_CONTAINER_IO_CAPABILITIES_T capabilities,
                                           VC_CONTAINER_STATUS_T *p_status );

/** Closes an instance of a container i/o module.
 * \param  context     Pointer to the VC_CONTAINER_IO_T context of the instance to close
 * \return             VC_CONTAINER_SUCCESS on success.
 */
VC_CONTAINER_STATUS_T vc_container_io_close( VC_CONTAINER_IO_T *context );

/** Read data from an i/o stream without advancing the read position within the stream.
 * \param  context     Pointer to the VC_CONTAINER_IO_T instance to use
 * \param  buffer      Pointer to the buffer where the data will be read
 * \param  size        Number of bytes to read
 * \return             The size of the data actually read.
 */
size_t vc_container_io_peek(VC_CONTAINER_IO_T *context, void *buffer, size_t size);

/** Read data from an i/o stream.
 * \param  context     Pointer to the VC_CONTAINER_IO_T instance to use
 * \param  buffer      Pointer to the buffer where the data will be read
 * \param  size        Number of bytes to read
 * \return             The size of the data actually read.
 */
size_t vc_container_io_read(VC_CONTAINER_IO_T *context, void *buffer, size_t size);

/** Skip data in an i/o stream without reading it.
 * \param  context     Pointer to the VC_CONTAINER_IO_T instance to use
 * \param  size        Number of bytes to skip
 * \return             The size of the data actually skipped.
 */
size_t vc_container_io_skip(VC_CONTAINER_IO_T *context, size_t size);

/** Write data to an i/o stream.
 * \param  context     Pointer to the VC_CONTAINER_IO_T instance to use
 * \param  buffer      Pointer to the buffer containing the data to write
 * \param  size        Number of bytes to write
 * \return             The size of the data actually written.
 */
size_t vc_container_io_write(VC_CONTAINER_IO_T *context, const void *buffer, size_t size);

/** Seek into an i/o stream.
 * \param  context     Pointer to the VC_CONTAINER_IO_T instance to use
 * \param  offset      Absolute file offset to seek to
 * \return             Status of the operation
 */
VC_CONTAINER_STATUS_T vc_container_io_seek(VC_CONTAINER_IO_T *context, int64_t offset);

/** Perform control operation on an i/o stream (va_list).
 * \param  context     Pointer to the VC_CONTAINER_IO_T instance to use
 * \param  operation   Control operation to be performed
 * \param  args        Additional arguments for the operation
 * \return             Status of the operation
 */
VC_CONTAINER_STATUS_T vc_container_io_control_list(VC_CONTAINER_IO_T *context,
                                                   VC_CONTAINER_CONTROL_T operation, va_list args);

/** Perform control operation on an i/o stream (varargs).
 * \param  context     Pointer to the VC_CONTAINER_IO_T instance to use
 * \param  operation   Control operation to be performed
 * \param  ...         Additional arguments for the operation
 * \return             Status of the operation
 */
VC_CONTAINER_STATUS_T vc_container_io_control(VC_CONTAINER_IO_T *context,
                                                VC_CONTAINER_CONTROL_T operation, ...);

/** Cache the pointed region of the i/o stream (from current position).
 * This will allow future seeking into the specified region even on non-seekable streams.
 * \param  context     Pointer to the VC_CONTAINER_IO_T instance to use
 * \param  size        Size of the region to cache
 * \return             Status of the operation
 */
size_t vc_container_io_cache(VC_CONTAINER_IO_T *context, size_t size);

/* @} */

#ifdef __cplusplus
}
#endif

#endif /* VC_CONTAINERS_HELPERS_H */
