/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "record_mediacodec_file.h"
#include "record_mp4_audio_edit.h"
#include <errno.h>
#include <stdlib.h>
#if defined(__ANDROID__) || defined(RECORD_MEDIACODEC_FILE_TEST)
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#define record_mcfe_seek _lseeki64
#define record_mcfe_read _read
#define record_mcfe_write _write
#else
#include <unistd.h>
#ifdef __ANDROID__
#define record_mcfe_seek lseek64
#else
#define record_mcfe_seek lseek
#endif
#define record_mcfe_read read
#define record_mcfe_write write
#endif

static uint64_t record_mcfe_uint(const unsigned char *p, unsigned bytes)
{
   uint64_t value = 0;
   unsigned i;
   for (i = 0; i < bytes; i++)
      value = (value << 8) | p[i];
   return value;
}

static bool record_mcfe_transfer(int fd, unsigned char *buffer,
      size_t bytes, bool writing)
{
   while (bytes)
   {
      int64_t count = writing ? record_mcfe_write(fd, buffer, (unsigned)bytes)
                            : record_mcfe_read(fd, buffer, (unsigned)bytes);
      if (count <= 0 || (uint64_t)count > bytes)
         return false;
      buffer += (size_t)count;
      bytes -= (size_t)count;
   }
   return true;
}

bool record_mediacodec_file_audio_edit(record_mediacodec_file_t *file,
      uint64_t skip, uint64_t frames, unsigned roll, int *error)
{
   unsigned char header[16];
   unsigned char *source = NULL, *output = NULL;
   uint64_t at = 0, size;
   int64_t end;
   size_t written = 0;
   unsigned boxes, head;
   int fd = record_mediacodec_file_descriptor(file);
   int saved = EINVAL;
   bool complete = false;
   if (error)
      *error = 0;
   if (fd < 0 || !skip || !roll)
      goto done;
   end = record_mcfe_seek(fd, 0, SEEK_END);
   if (end < 8)
      goto done;
   for (boxes = 0; boxes < 4096 && at < (uint64_t)end; boxes++)
   {
      head = 8;
      if ((uint64_t)end - at < 8
            || record_mcfe_seek(fd, (int64_t)at, SEEK_SET) != (int64_t)at
            || !record_mcfe_transfer(fd, header, 8, false))
         goto done;
      size = record_mcfe_uint(header, 4);
      if (size == 1)
      {
         if ((uint64_t)end - at < 16
               || !record_mcfe_transfer(fd, header + 8, 8, false))
            goto done;
         head = 16;
         size = record_mcfe_uint(header + 8, 8);
      }
      if (size < head || size > (uint64_t)end - at)
         goto done;
      if (!memcmp(header + 4, "moov", 4))
         break;
      at += size;
   }
   if (boxes == 4096 || at == (uint64_t)end
         || size != (uint64_t)end - at || size > 64 * 1024 * 1024)
      goto done;
   source = (unsigned char *)malloc((size_t)size);
   if (!source)
   {
      saved = ENOMEM;
      goto done;
   }
   if (record_mcfe_seek(fd, (int64_t)at, SEEK_SET) != (int64_t)at
         || !record_mcfe_transfer(fd, source, (size_t)size, false)
         || !record_mp4_audio_edit(source, (size_t)size, skip, frames,
               roll, NULL, 0, &written)
         || written < size || written > 64 * 1024 * 1024
         || (uint64_t)written > (((uint64_t)1 << 63) - 1) - at)
      goto done;
   output = (unsigned char *)malloc(written);
   if (!output)
   {
      saved = ENOMEM;
      goto done;
   }
   if (!record_mp4_audio_edit(source, (size_t)size, skip, frames,
            roll, output, written, &written))
      goto done;
   saved = EIO;
   if (record_mcfe_seek(fd, (int64_t)at, SEEK_SET) != (int64_t)at
         || !record_mcfe_transfer(fd, output, written, true))
      goto done;
   complete = true;
done:
   free(output);
   free(source);
   if (error)
      *error = complete ? 0 : saved;
   return complete;
}
#else
bool record_mediacodec_file_audio_edit(record_mediacodec_file_t *file,
      uint64_t skip, uint64_t frames, unsigned roll, int *error)
{
   (void)file;
   (void)skip;
   (void)frames;
   (void)roll;
   if (error)
      *error = ENOSYS;
   return false;
}
#endif
