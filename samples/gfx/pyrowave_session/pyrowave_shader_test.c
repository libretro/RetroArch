/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../record/record_pyrowave_source.h"
#include <streams/file_stream.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
struct RFILE { int unused; };
static struct RFILE shader_file;
static unsigned shader_opens, shader_reads, shader_closes;
static int64_t shader_size = 7;
RFILE *filestream_open(const char *path, unsigned mode, unsigned hints)
{
   (void)mode; (void)hints;
   shader_opens++;
   return !strcmp(path, "override/color.dxil") ? &shader_file : NULL;
}
int64_t filestream_get_size(RFILE *file) { assert(file == &shader_file); return shader_size; }
int64_t filestream_read(RFILE *file, void *data, int64_t size)
{
   assert(file == &shader_file && size == 7);
   memcpy(data, "fixture", 7); shader_reads++;
   return size;
}
int filestream_close(RFILE *file) { assert(file == &shader_file); shader_closes++; return 0; }
int main(void)
{
   static const char *const names[] = { "color.dxil", "dwt.dxil", "packet.dxil", "color.dxbc", "dwt.dxbc", "packet.dxbc" };
   record_pyrowave_shader_t first, second;
   unsigned i;
   for (i = 0; i < 6; i++)
   {
#if defined(HAVE_D3D11) && !defined(HAVE_D3D12)
      if (i < 3)
      {
         assert(!record_pyrowave_source_shader_available(names[i]));
         continue;
      }
#endif
      assert(record_pyrowave_source_shader_available(names[i]));
      assert(record_pyrowave_source_shader(NULL, names[i], &first));
      assert(record_pyrowave_source_shader("", names[i], &second));
      assert(first.data == second.data && first.bytes == second.bytes && first.bytes > 100 && !(first.bytes & 3));
      assert(!first.owned && !second.owned);
      assert(!memcmp(first.data, "DXBC", 4) && !((uintptr_t)first.data & 3));
      record_pyrowave_source_shader_release(&first);
      record_pyrowave_source_shader_release(&second);
      assert(!first.data && !first.owned && !first.bytes);
   }
   assert(!shader_opens && !shader_reads && !shader_closes);
   assert(!record_pyrowave_source_shader_available(NULL));
   assert(!record_pyrowave_source_shader("", "unknown.hlsl", &first));
   assert(!record_pyrowave_source_shader("", NULL, &first));
   assert(!record_pyrowave_source_shader("", names[0], NULL));
   assert(!shader_opens);
   assert(!record_pyrowave_source_shader("absent", names[0], &first));
   assert(!first.data && !first.owned && !first.bytes);
   assert(record_pyrowave_source_shader("override", names[0], &first));
   assert(first.owned == first.data && first.bytes == 7 && !memcmp(first.data, "fixture", 8));
   record_pyrowave_source_shader_release(&first);
   shader_size = 1048577;
   assert(!record_pyrowave_source_shader("override", names[0], &first));
   assert(shader_opens == 3 && shader_reads == 1 && shader_closes == 2);
   record_pyrowave_source_shader_release(&first);
   record_pyrowave_source_shader_release(NULL);
   puts("PASS: embedded shaders borrow executable data without file I/O; overrides remain bounded and explicit");
   return 0;
}
