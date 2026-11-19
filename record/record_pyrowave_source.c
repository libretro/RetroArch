/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_pyrowave_source.h"
#include <stdlib.h>
#include <string.h>
#include <streams/file_stream.h>

bool record_pyrowave_source_shader(const char *directory, const char *name,
      void **data, size_t *bytes)
{
   char path[1200];
   int64_t size = 0;
   size_t length;
   void *memory = NULL;
   RFILE *file;
   if (!directory || !name || !data || !bytes) return false;
   *data = NULL; *bytes = 0;
   length = strlen(directory);
   if (length + strlen(name) + 2 > sizeof(path)) return false;
   memcpy(path, directory, length); path[length++] = '/';
   memcpy(path + length, name, strlen(name) + 1);
   file = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!file) return false;
   size = filestream_get_size(file);
   if (size > 0 && size <= 1048576) memory = malloc((size_t)size + 1);
   if (!memory || filestream_read(file, memory, size) != size)
   { filestream_close(file); free(memory); return false; }
   filestream_close(file);
   ((char*)memory)[size] = '\0';
   *data = memory; *bytes = (size_t)size;
   return true;
}
