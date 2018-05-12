/* Copyright  (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (nbio.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_NBIO_H
#define __LIBRETRO_SDK_NBIO_H

#include <stddef.h>
#include <boolean.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#ifndef NBIO_READ
#define NBIO_READ   0
#endif

#ifndef NBIO_WRITE
#define NBIO_WRITE  1
#endif

#ifndef NBIO_UPDATE
#define NBIO_UPDATE 2
#endif

/* these two are blocking; nbio_iterate always returns true, but that operation (or something earlier) may take arbitrarily long */
#ifndef BIO_READ
#define BIO_READ    3
#endif

#ifndef BIO_WRITE
#define BIO_WRITE   4
#endif

typedef struct nbio_intf
{
   void *(*open)(const char * filename, unsigned mode);

   void (*begin_read)(void *data);

   void (*begin_write)(void *data);

   bool (*iterate)(void *data);

   void (*resize)(void *data, size_t len);

   void *(*get_ptr)(void *data, size_t* len);

   void (*cancel)(void *data);

   void (*free)(void *data);

   /* Human readable string. */
   const char *ident;
} nbio_intf_t;

/*
 * Creates an nbio structure for performing the
 * given operation on the given file.
 */
void *nbio_open(const char * filename, unsigned mode);

/*
 * Starts reading the given file. When done, it will be available in nbio_get_ptr.
 * Can not be done if the structure was created with {N,}BIO_WRITE.
 */
void nbio_begin_read(void *data);

/*
 * Starts writing to the given file. Before this, you should've copied the data to nbio_get_ptr.
 * Can not be done if the structure was created with {N,}BIO_READ.
 */
void nbio_begin_write(void *data);

/*
 * Performs part of the requested operation, or checks how it's going.
 * When it returns true, it's done.
 */
bool nbio_iterate(void *data);

/*
 * Resizes the file up to the given size; cannot shrink.
 * Can not be done if the structure was created with {N,}BIO_READ.
 */
void nbio_resize(void *data, size_t len);

/*
 * Returns a pointer to the file data. Writable only if structure was not created with {N,}BIO_READ.
 * If any operation is in progress, the pointer will be NULL, but len will still be correct.
 */
void* nbio_get_ptr(void *data, size_t* len);

/*
 * Stops any pending operation, allowing the object to be freed.
 */
void nbio_cancel(void *data);

/*
 * Deletes the nbio structure and its associated pointer.
 */
void nbio_free(void *data);

RETRO_END_DECLS

#endif
