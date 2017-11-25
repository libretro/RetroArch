/* Copyright  (C) 2010-2017 The RetroArch team
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

#if defined(HAVE_MMAP) && defined(_linux__)
static nbio_intf_t *internal_nbio = &nbio_linux;
#elif defined(HAVE_MMAP) && defined(BSD) && !defined(__MACH__)
static nbio_intf_t *internal_nbio = &nbio_mmap_unix;
#elif defined(_WIN32) && !defined(_XBOX)
static nbio_intf_t *internal_nbio = &nbio_mmap_win32;
#else
static nbio_intf_t *internal_nbio = &nbio_stdio;
#endif

struct nbio_t* nbio_open(const char * filename, unsigned mode)
{
   return internal_nbio->open(filename, mode);
}

void nbio_begin_read(struct nbio_t* handle)
{
   internal_nbio->begin_read(handle);
}

void nbio_begin_write(struct nbio_t* handle)
{
   internal_nbio->begin_write(handle);
}

bool nbio_iterate(struct nbio_t* handle)
{
   return internal_nbio->iterate(handle);
}

void nbio_resize(struct nbio_t* handle, size_t len)
{
   internal_nbio->resize(handle, len);
}

void *nbio_get_ptr(struct nbio_t* handle, size_t* len)
{
   return internal_nbio->get_ptr(handle, len);
}

void nbio_cancel(struct nbio_t* handle)
{
   internal_nbio->cancel(handle);
}

void nbio_free(struct nbio_t* handle)
{
   internal_nbio->free(handle);
}
