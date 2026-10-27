/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../../../record/record_mediacodec_file.h"
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#include <winioctl.h>
#define file_write _write
#define file_read _read
#define file_seek _lseeki64
#else
#include <unistd.h>
#define file_write write
#define file_read read
#define file_seek lseek
#endif

static unsigned char *load(const char *path, size_t *bytes)
{
   FILE *f = fopen(path, "rb");
   unsigned char *data;
   long size;
   assert(f && !fseek(f, 0, SEEK_END));
   size = ftell(f);
   assert(size > 0 && size < 65536 && !fseek(f, 0, SEEK_SET));
   data = (unsigned char *)malloc((size_t)size);
   assert(data && fread(data, 1, (size_t)size, f) == (size_t)size);
   assert(!fclose(f));
   *bytes = (size_t)size;
   return data;
}

int main(int argc, char **argv)
{
   static const unsigned char prefix[] = {
      0, 0, 0, 8, 'f', 't', 'y', 'p',
      0, 0, 0, 1, 'm', 'd', 'a', 't',
      0, 0, 0, 0, 0, 0, 0, 24,
      1, 2, 3, 4, 5, 6, 7, 8};
   record_mediacodec_file_t *f;
   unsigned char *input, *expected, *output;
   unsigned char large_prefix[sizeof(prefix)];
   int64_t large_offset = (int64_t)((uint64_t)1 << 32) + sizeof(prefix);
   size_t bytes, expected_size;
   char path[1024];
   unsigned i;
   int fd, error;
   unsigned frames;
   assert(argc == 4 && strlen(argv[1]) < sizeof(path) - 16);
   frames = (unsigned)strtoul(argv[3], NULL, 10);
   input = load(argv[1], &bytes);
   expected = load(argv[2], &expected_size);
   output = (unsigned char *)malloc(expected_size + sizeof(prefix));
   assert(output);
   sprintf(path, "%s.mp4", argv[1]);
   f = record_mediacodec_file_new(path, &error);
   assert(f && !error);
   fd = record_mediacodec_file_descriptor(f);
   assert(file_write(fd, prefix, sizeof(prefix)) == sizeof(prefix));
   assert(file_write(fd, input, (unsigned)bytes) == (int)bytes);
   assert(record_mediacodec_file_audio_edit(f, 2048, frames, 2, &error));
   assert(!error);
   assert(file_seek(fd, 0, SEEK_END)
         == (int64_t)(sizeof(prefix) + expected_size));
   assert(file_seek(fd, 0, SEEK_SET) == 0);
   assert(file_read(fd, output, (unsigned)(expected_size + sizeof(prefix)))
         == (int)(expected_size + sizeof(prefix)));
   assert(!memcmp(output, prefix, sizeof(prefix)));
   assert(!memcmp(output + sizeof(prefix), expected, expected_size));
   assert(!record_mediacodec_file_audio_edit(f, 2048, frames, 2, &error));
   assert(error && record_mediacodec_file_close(f, true, &error));
   assert(!remove(path));
   sprintf(path, "%s-large.mp4", argv[1]);
   f = record_mediacodec_file_new(path, &error);
   assert(f);
   fd = record_mediacodec_file_descriptor(f);
#ifdef _WIN32
   {
      DWORD returned = 0;
      assert(DeviceIoControl((HANDLE)_get_osfhandle(fd), FSCTL_SET_SPARSE,
               NULL, 0, NULL, 0, &returned, NULL));
   }
#endif
   memcpy(large_prefix, prefix, sizeof(prefix));
   large_prefix[19] = 1;
   assert(file_write(fd, large_prefix, sizeof(large_prefix))
         == sizeof(large_prefix));
   assert(file_seek(fd, large_offset, SEEK_SET) == large_offset);
   assert(file_write(fd, input, (unsigned)bytes) == (int)bytes);
   assert(record_mediacodec_file_audio_edit(f, 2048, frames, 2, &error));
   assert(!error);
   assert(file_seek(fd, 0, SEEK_END) == large_offset + (int64_t)expected_size);
   assert(file_seek(fd, 0, SEEK_SET) == 0);
   assert(file_read(fd, output, sizeof(large_prefix)) == sizeof(large_prefix));
   assert(!memcmp(output, large_prefix, sizeof(large_prefix)));
   assert(file_seek(fd, large_offset, SEEK_SET) == large_offset);
   assert(file_read(fd, output, (unsigned)expected_size) == (int)expected_size);
   assert(!memcmp(output, expected, expected_size));
   assert(record_mediacodec_file_close(f, true, &error) && !error);
   assert(!remove(path));
   for (i = 0; i < 5; i++)
   {
      unsigned char header[16] = {0, 0, 0, 8, 'f', 'r', 'e', 'e'};
      sprintf(path, "%s-%u.mp4", argv[1], i);
      f = record_mediacodec_file_new(path, &error);
      assert(f);
      fd = record_mediacodec_file_descriptor(f);
      if (i == 1)
         header[3] = 0;
      else if (i == 2)
         header[3] = 255;
      else if (i == 3)
         header[3] = 1;
      else if (i == 4)
      {
         memcpy(header + 4, "moov", 4);
         header[3] = 8;
      }
      assert(file_write(fd, header, 8) == 8);
      assert(!record_mediacodec_file_audio_edit(f, 2048, frames, 2, &error));
      assert(error && record_mediacodec_file_close(f, false, &error));
      assert(!remove(path));
   }
   assert(!record_mediacodec_file_audio_edit(NULL, 2048, frames, 2, &error));
   free(output);
   free(expected);
   free(input);
   puts("PASS: cold MP4 finalization preserves media, edits only moov and "
        "rejects malformed boxes and repeated edits");
   return 0;
}
