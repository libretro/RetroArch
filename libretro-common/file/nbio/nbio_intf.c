/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (nbio_intf.c).
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

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <file/nbio.h>

extern nbio_intf_t nbio_linux;
extern nbio_intf_t nbio_mmap_unix;
extern nbio_intf_t nbio_mmap_win32;
extern nbio_intf_t nbio_stdio;

#ifndef _XBOX
#if defined(_WIN32)
#if defined(_MSC_VER) && _MSC_VER >= 1500

#ifndef HAVE_MMAP_WIN32
#define HAVE_MMAP_WIN32
#endif

#elif !defined(_MSC_VER)

#ifndef HAVE_MMAP_WIN32
#define HAVE_MMAP_WIN32
#endif
#endif
#endif

#endif

#if defined(__linux__)
static nbio_intf_t *internal_nbio = &nbio_linux;
#elif defined(HAVE_MMAP) && defined(BSD)
static nbio_intf_t *internal_nbio = &nbio_mmap_unix;
#elif defined(HAVE_MMAP_WIN32)
static nbio_intf_t *internal_nbio = &nbio_mmap_win32;
#else
static nbio_intf_t *internal_nbio = &nbio_stdio;
#endif

void *nbio_open(const char * filename, unsigned mode)
{
   return internal_nbio->open(filename, mode);
}

void nbio_begin_read(void *data)
{
   internal_nbio->begin_read(data);
}

void nbio_begin_write(void *data)
{
   internal_nbio->begin_write(data);
}

bool nbio_iterate(void *data)
{
   return internal_nbio->iterate(data);
}

void nbio_resize(void *data, size_t len)
{
   internal_nbio->resize(data, len);
}

void *nbio_get_ptr(void *data, size_t* len)
{
   return internal_nbio->get_ptr(data, len);
}

void nbio_cancel(void *data)
{
   internal_nbio->cancel(data);
}

void nbio_free(void *data)
{
   internal_nbio->free(data);
}

void nbio_set_chunk_size(void *data, size_t chunk_size)
{
   if (internal_nbio->set_chunk_size)
      internal_nbio->set_chunk_size(data, chunk_size);
}

int nbio_get_fd(void *data)
{
   if (internal_nbio->get_fd)
      return internal_nbio->get_fd(data);
   return -1;
}

bool nbio_get_progress(void *data, size_t *completed, size_t *total)
{
   if (internal_nbio->get_progress)
      return internal_nbio->get_progress(data, completed, total);
   if (completed) *completed = 0;
   if (total)     *total     = 0;
   return false;
}

void *nbio_load_entire(void *data, size_t *len)
{
   /* Fast path: backend provides a direct implementation */
   if (internal_nbio->load_entire)
      return internal_nbio->load_entire(data, len);

   /* Fallback: use the iterate loop */
   internal_nbio->begin_read(data);
   while (!internal_nbio->iterate(data));
   return internal_nbio->get_ptr(data, len);
}
