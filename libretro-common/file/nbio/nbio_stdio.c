/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (nbio_stdio.c).
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
#if defined(WIIU)
#include <malloc.h>
#endif

/* posix_fadvise for sequential readahead hints.
 * Available on Linux and most BSDs, but NOT on macOS/iOS (Darwin),
 * Wii U, 3DS, Vita, Switch, PS2, PS3, PS4/Orbis, or Windows. */
#if !defined(_WIN32) && !defined(__APPLE__) && !defined(WIIU) \
 && !defined(_3DS) && !defined(VITA) && !defined(HAVE_LIBNX) \
 && !defined(SWITCH) && !defined(GEKKO) && !defined(__CELLOS_LV2__) \
 && !defined(__PSL1GHT__) && !defined(PSP) && !defined(PS2) \
 && !defined(ORBIS) \
 && (defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) \
     || defined(__OpenBSD__) || defined(__DragonFly__) \
     || (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L))
#include <fcntl.h>
#define NBIO_HAVE_FADVISE 1
#endif

#include <file/nbio.h>
#include <encodings/utf.h>

/* Assume W-functions do not work below Win2K and Xbox platforms */
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0500 || defined(_XBOX)

#ifndef LEGACY_WIN32
#define LEGACY_WIN32
#endif

#endif

#if defined(_WIN32)
#if defined(_MSC_VER) && _MSC_VER >= 1400
#define ATLEAST_VC2005
#endif
#endif

#if (defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE - 0) >= 200112) || (defined(__POSIX_VISIBLE) && __POSIX_VISIBLE >= 200112) || (defined(_POSIX_VERSION) && _POSIX_VERSION >= 200112) || __USE_LARGEFILE || (defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS == 64)
#ifndef HAVE_64BIT_OFFSETS
#define HAVE_64BIT_OFFSETS
#endif
#endif

/* Default chunk size for non-blocking I/O iterations (bytes).
 * Callers may override per-handle via nbio_set_chunk_size(). */
#define NBIO_DEFAULT_CHUNK_SIZE 65536

struct nbio_stdio_t
{
   FILE* f;
   void* data;
   size_t progress;
   size_t len;
   size_t chunk_size;
   /*
    * possible values:
    * NBIO_READ, NBIO_WRITE - obvious
    * -1 - currently doing nothing
    * -2 - the pointer was reallocated since the last operation
    */
   signed char op;
   signed char mode;
};

#if !defined(_WIN32) || defined(LEGACY_WIN32)
static const char    *stdio_modes[] = { "rb", "wb", "r+b", "rb", "wb", "r+b" };
#else
static const wchar_t *stdio_modes[] = { L"rb", L"wb", L"r+b", L"rb", L"wb", L"r+b" };
#endif

static int64_t fseek_wrap(FILE *f, int64_t offset, int origin)
{
#ifdef ATLEAST_VC2005
   /* VC2005 and up have a special 64-bit fseek */
   return _fseeki64(f, offset, origin);
#elif defined(HAVE_64BIT_OFFSETS)
   return fseeko(f, (off_t)offset, origin);
#else
   return fseek(f, (long)offset, origin);
#endif
}

static int64_t ftell_wrap(FILE *f)
{
#ifdef ATLEAST_VC2005
   /* VC2005 and up have a special 64-bit ftell */
   return _ftelli64(f);
#elif defined(HAVE_64BIT_OFFSETS)
   return ftello(f);
#else
   return ftell(f);
#endif
}

static void *nbio_stdio_open(const char * filename, unsigned mode)
{
   void *buf                   = NULL;
   struct nbio_stdio_t* handle = NULL;
   int64_t len                 = 0;
#if !defined(_WIN32) || defined(LEGACY_WIN32)
   FILE* f                     = fopen(filename, stdio_modes[mode]);
#else
   wchar_t *filename_wide      = utf8_to_utf16_string_alloc(filename);
   FILE* f                     = _wfopen(filename_wide, stdio_modes[mode]);

   if (filename_wide)
      free(filename_wide);
#endif
   if (!f)
      return NULL;

   handle = (struct nbio_stdio_t*)malloc(sizeof(struct nbio_stdio_t));

   if (!handle)
   {
      fclose(f);
      return NULL;
   }

   handle->f = f;

   switch (mode)
   {
      case NBIO_WRITE:
      case BIO_WRITE:
         break;
      default:
         fseek_wrap(handle->f, 0, SEEK_END);
         len = ftell_wrap(handle->f);
         break;
   }

   handle->mode          = mode;

#if defined(WIIU)
   /* hit the aligned-buffer fast path on Wii U */
   if (len)
      buf                = memalign(0x40, (size_t)len);
#else
   if (len)
      buf                = malloc((size_t)len);
#endif

   if (len && !buf)
   {
      free(handle);
      fclose(f);
      return NULL;
   }

   handle->data          = buf;
   handle->len           = len;
   handle->progress      = handle->len;
   handle->chunk_size    = NBIO_DEFAULT_CHUNK_SIZE;
   handle->op            = -2;

   return handle;
}

static void nbio_stdio_begin_read(void *data)
{
   struct nbio_stdio_t *handle = (struct nbio_stdio_t*)data;
   if (!handle)
      return;

   if (handle->op >= 0)
      abort();

   fseek_wrap(handle->f, 0, SEEK_SET);

#ifdef NBIO_HAVE_FADVISE
   /* Hint sequential access so the kernel enlarges its readahead window */
   {
      int fd = fileno(handle->f);
      if (fd >= 0)
         posix_fadvise(fd, 0, (off_t)handle->len, POSIX_FADV_SEQUENTIAL);
   }
#endif

   handle->op       = NBIO_READ;
   handle->progress = 0;
}

static void nbio_stdio_begin_write(void *data)
{
   struct nbio_stdio_t *handle = (struct nbio_stdio_t*)data;
   if (!handle)
      return;

   if (handle->op >= 0)
      abort();

   fseek_wrap(handle->f, 0, SEEK_SET);
   handle->op = NBIO_WRITE;
   handle->progress = 0;
}

static bool nbio_stdio_iterate(void *data)
{
   size_t amount               = 0;
   struct nbio_stdio_t *handle = (struct nbio_stdio_t*)data;

   if (!handle)
      return false;

   amount = handle->chunk_size;

   if (amount > handle->len - handle->progress)
      amount = handle->len - handle->progress;

   switch (handle->op)
   {
      case NBIO_READ:
         if (handle->mode == BIO_READ)
         {
            amount = handle->len;
            fread((char*)handle->data, 1, amount, handle->f);
         }
         else
            fread((char*)handle->data + handle->progress, 1, amount, handle->f);
         break;
      case NBIO_WRITE:
         if (handle->mode == BIO_WRITE)
         {
            size_t written = 0;
            amount = handle->len;
            written = fwrite((char*)handle->data, 1, amount, handle->f);
            if (written != amount)
               return false;
         }
         else
            fwrite((char*)handle->data + handle->progress, 1, amount, handle->f);
         break;
   }

   handle->progress += amount;

   if (handle->progress == handle->len)
      handle->op = -1;
   return (handle->op < 0);
}

static void nbio_stdio_resize(void *data, size_t len)
{
   void *new_data              = NULL;
   struct nbio_stdio_t *handle = (struct nbio_stdio_t*)data;
   if (!handle)
      return;

   if (handle->op >= 0)
      abort();
   if (len < handle->len)
      abort();

   handle->len      = len;
   handle->progress = len;
   handle->op       = -1;

   new_data         = realloc(handle->data, handle->len);

   if (new_data)
      handle->data  = new_data;
}

static void *nbio_stdio_get_ptr(void *data, size_t* len)
{
   struct nbio_stdio_t *handle = (struct nbio_stdio_t*)data;
   if (!handle)
      return NULL;
   if (len)
      *len = handle->len;
   if (handle->op == -1)
      return handle->data;
   return NULL;
}

static void nbio_stdio_cancel(void *data)
{
   struct nbio_stdio_t *handle = (struct nbio_stdio_t*)data;
   if (!handle)
      return;

   handle->op = -1;
   handle->progress = handle->len;
}

static void nbio_stdio_free(void *data)
{
   struct nbio_stdio_t *handle = (struct nbio_stdio_t*)data;
   if (!handle)
      return;
   if (handle->op >= 0)
      abort();
   fclose(handle->f);
   free(handle->data);

   handle->f    = NULL;
   handle->data = NULL;
   free(handle);
}

static void nbio_stdio_set_chunk_size(void *data, size_t chunk_size)
{
   struct nbio_stdio_t *handle = (struct nbio_stdio_t*)data;
   if (!handle)
      return;
   handle->chunk_size = (chunk_size > 0) ? chunk_size : NBIO_DEFAULT_CHUNK_SIZE;
}

static int nbio_stdio_get_fd(void *data)
{
#if !defined(_WIN32) || defined(LEGACY_WIN32)
   struct nbio_stdio_t *handle = (struct nbio_stdio_t*)data;
   if (handle && handle->f)
      return fileno(handle->f);
#endif
   return -1;
}

static bool nbio_stdio_get_progress(void *data,
      size_t *completed, size_t *total)
{
   struct nbio_stdio_t *handle = (struct nbio_stdio_t*)data;
   if (!handle)
   {
      if (completed) *completed = 0;
      if (total)     *total     = 0;
      return false;
   }
   if (completed) *completed = handle->progress;
   if (total)     *total     = handle->len;
   return (handle->op >= 0);
}

static void *nbio_stdio_load_entire(void *data, size_t *len)
{
   struct nbio_stdio_t *handle = (struct nbio_stdio_t*)data;
   if (!handle || !handle->f)
      return NULL;

   fseek_wrap(handle->f, 0, SEEK_SET);

#ifdef NBIO_HAVE_FADVISE
   {
      int fd = fileno(handle->f);
      if (fd >= 0)
         posix_fadvise(fd, 0, (off_t)handle->len, POSIX_FADV_SEQUENTIAL);
   }
#endif

   /* Single fread of the entire file */
   if (handle->len > 0)
      fread((char*)handle->data, 1, handle->len, handle->f);

   handle->progress = handle->len;
   handle->op       = -1;

   if (len)
      *len = handle->len;
   return handle->data;
}

nbio_intf_t nbio_stdio = {
   nbio_stdio_open,
   nbio_stdio_begin_read,
   nbio_stdio_begin_write,
   nbio_stdio_iterate,
   nbio_stdio_resize,
   nbio_stdio_get_ptr,
   nbio_stdio_cancel,
   nbio_stdio_free,
   nbio_stdio_set_chunk_size,
   nbio_stdio_get_fd,
   nbio_stdio_get_progress,
   nbio_stdio_load_entire,
   "nbio_stdio",
};
