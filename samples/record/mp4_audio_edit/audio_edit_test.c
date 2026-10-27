/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../record/record_mp4_audio_edit.h"
static unsigned char *read_file(const char *path, size_t *size)
{
   FILE *file = fopen(path, "rb");
   unsigned char *data;
   long bytes;
   assert(file && !fseek(file, 0, SEEK_END));
   bytes = ftell(file);
   assert(bytes > 0 && bytes <= 64 * 1024 * 1024 && !fseek(file, 0, SEEK_SET));
   *size = (size_t)bytes;
   data = (unsigned char *)malloc(*size);
   assert(data && fread(data, 1, *size, file) == *size);
   assert(!fclose(file));
   return data;
}

int main(int argc, char **argv)
{
   unsigned char *input, *expected, *output;
   size_t bytes, expected_size, size, i;
   unsigned frames;
   assert(argc == 4);
   frames = (unsigned)strtoul(argv[3], NULL, 10);
   input = read_file(argv[1], &bytes);
   expected = read_file(argv[2], &expected_size);
   assert(record_mp4_audio_edit(input, bytes, 2048, frames, 2, NULL, 0, &size));
   assert(size == expected_size);
   output = (unsigned char *)malloc(size);
   assert(output);
   assert(record_mp4_audio_edit(input, bytes, 2048, frames, 2, output, size, &size));
   assert(size == expected_size && !memcmp(output, expected, size));
   for (i = 0; i < bytes; i++)
   {
      size = 1;
      assert(!record_mp4_audio_edit(input, i, 2048, frames, 2, NULL, 0, &size));
      assert(!size);
   }
   for (i = 0; i < expected_size; i++)
   {
      size = 1;
      assert(!record_mp4_audio_edit(input, bytes, 2048, frames, 2, output, i, &size));
      assert(!size);
   }
   assert(!record_mp4_audio_edit(input, bytes, 2048, frames, 2,
            input, expected_size, &size));
   assert(!record_mp4_audio_edit(input, bytes, 2048, frames, 2,
            input + 1, expected_size, &size));
   assert(!record_mp4_audio_edit(output, expected_size, 2048, frames, 2,
            NULL, 0, &size));
   assert(!record_mp4_audio_edit(input, bytes, 50176, 1, 2, NULL, 0, &size));
   assert(!record_mp4_audio_edit(input, bytes, 2048, 48000, 0, NULL, 0, &size));
   assert(!record_mp4_audio_edit(input, bytes, 2048, 48000, 32769, NULL, 0, &size));
   free(output);
   free(expected);
   free(input);
   puts("PASS: audio edit clock, roll groups, untouched bytes, bounds and alias admission");
   return 0;
}
