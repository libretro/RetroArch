/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_file_posix.h"
#include "record_mediacodec_file.h"
#include <stdlib.h>
#include <errno.h>
#if defined(__linux__) || defined(RECORD_FILE_POSIX_TEST)
#ifdef _WIN32
#include <io.h>
#ifndef RECORD_FP_WRITE
#define RECORD_FP_WRITE _write
#endif
#else
#include <unistd.h>
#ifndef RECORD_FP_WRITE
#define RECORD_FP_WRITE write
#endif
#endif
struct record_file_posix
{
   record_mediacodec_file_t *file;
   const uint8_t *data;
   size_t size, offset;
   uint64_t completed;
   int error;
   bool finished;
};
record_file_posix_t *record_file_posix_new(const char *path)
{
   record_file_posix_t *f = (record_file_posix_t *)calloc(1, sizeof(*f));
   if (!f)
      return NULL;
   f->file = record_mediacodec_file_new(path, &f->error);
   if (!f->file)
   {
      free(f);
      return NULL;
   }
   return f;
}
int record_file_posix_push(record_file_posix_t *f, const void *data, size_t size)
{
   if (!f || f->error || f->finished || !data || !size ||
       size > UINT64_MAX - f->completed)
      return RECORD_FILE_POSIX_ERROR;
   if (f->data)
      return RECORD_FILE_POSIX_BUSY;
   f->data = (const uint8_t *)data;
   f->size = size;
   f->offset = 0;
   return RECORD_FILE_POSIX_OK;
}
int record_file_posix_step(record_file_posix_t *f)
{
   size_t bytes;
   long written;
   if (!f || f->error)
      return RECORD_FILE_POSIX_ERROR;
   if (!f->data)
      return RECORD_FILE_POSIX_OK;
   bytes = f->size - f->offset;
   if (bytes > 65536)
      bytes = 65536;
   written = (long)RECORD_FP_WRITE(record_mediacodec_file_descriptor(f->file),
                                   f->data + f->offset, bytes);
   if (written < 0 && (errno == EINTR || errno == EAGAIN))
      return RECORD_FILE_POSIX_BUSY;
   if (written <= 0 || (size_t)written > bytes)
   {
      f->error = written < 0 && errno ? errno : EIO;
      f->data = NULL;
      return RECORD_FILE_POSIX_ERROR;
   }
   f->offset += (size_t)written;
   f->completed += (size_t)written;
   if (f->offset != f->size)
      return RECORD_FILE_POSIX_BUSY;
   f->data = NULL;
   return RECORD_FILE_POSIX_OK;
}
bool record_file_posix_cancel(record_file_posix_t *f)
{
   if (!f || f->finished)
      return false;
   if (!f->error)
      f->error = ECANCELED;
   f->data = NULL;
   return true;
}
uint64_t record_file_posix_bytes(const record_file_posix_t *f)
{
   return f ? f->completed : 0;
}
int record_file_posix_error(const record_file_posix_t *f)
{
   return f ? f->error : EINVAL;
}
bool record_file_posix_finish(record_file_posix_t *f)
{
   int error = 0;
   bool ok;
   if (!f || f->data || f->error)
      return false;
   if (f->finished)
      return true;
   ok = record_mediacodec_file_close(f->file, true, &error);
   f->file = NULL;
   if (!ok)
   {
      f->error = error ? error : EIO;
      return false;
   }
   f->finished = true;
   return true;
}
bool record_file_posix_free(record_file_posix_t *f)
{
   if (!f)
      return true;
   if (f->data)
      return false;
   record_mediacodec_file_close(f->file, false, NULL);
   free(f);
   return true;
}
#else
record_file_posix_t *record_file_posix_new(const char *path)
{
   (void)path;
   return NULL;
}
int record_file_posix_push(record_file_posix_t *f, const void *data, size_t size)
{
   (void)f;
   (void)data;
   (void)size;
   return RECORD_FILE_POSIX_ERROR;
}
int record_file_posix_step(record_file_posix_t *f)
{
   (void)f;
   return RECORD_FILE_POSIX_ERROR;
}
bool record_file_posix_cancel(record_file_posix_t *f)
{
   (void)f;
   return false;
}
uint64_t record_file_posix_bytes(const record_file_posix_t *f)
{
   (void)f;
   return 0;
}
int record_file_posix_error(const record_file_posix_t *f)
{
   (void)f;
   return ENOSYS;
}
bool record_file_posix_finish(record_file_posix_t *f)
{
   (void)f;
   return false;
}
bool record_file_posix_free(record_file_posix_t *f)
{
   return f == NULL;
}
#endif
