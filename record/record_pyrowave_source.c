/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif
#include "record_pyrowave_source.h"
#include <stdlib.h>
#include <string.h>
#include <streams/file_stream.h>

/* Committed executable data; no build-time generator or runtime files. */
#if defined(HAVE_OPENGL_CORE)
#define RPYROWAVE_SHADER_GLSL
#endif
#if defined(HAVE_VULKAN)
#define RPYROWAVE_SHADER_SPIRV
#endif
#if defined(HAVE_D3D12) || defined(HAVE_D3D11)
#define RPYROWAVE_SHADER_D3D
#ifndef HAVE_D3D12
#define RPYROWAVE_SHADER_DXBC_ONLY
#endif
#endif
#if defined(HAVE_METAL)
#define RPYROWAVE_SHADER_METAL
#endif
#include "../libretro-common/formats/pyrowave/shaders/rpyrowave_shaders.h"

typedef struct record_pyrowave_builtin_shader
{
   const char *name;
   const void *data;
   size_t bytes;
} record_pyrowave_builtin_shader_t;
#define PYROWAVE_TEXT(name, suffix, format) \
   { #name suffix, rpyrowave_shader_##name##_##format, sizeof(rpyrowave_shader_##name##_##format) - 1 }
#define PYROWAVE_BINARY(name, suffix, format) \
   { #name suffix, rpyrowave_shader_##name##_##format, sizeof(rpyrowave_shader_##name##_##format) }
static const record_pyrowave_builtin_shader_t record_pyrowave_builtin_shaders[] = {
#ifdef RPYROWAVE_SHADER_GLSL
   PYROWAVE_TEXT(color, ".comp", glsl), PYROWAVE_TEXT(dwt, ".comp", glsl), PYROWAVE_TEXT(packet, ".comp", glsl),
#endif
#ifdef RPYROWAVE_SHADER_SPIRV
   PYROWAVE_BINARY(color, ".spv", spirv), PYROWAVE_BINARY(dwt, ".spv", spirv), PYROWAVE_BINARY(packet, ".spv", spirv),
#endif
#ifdef RPYROWAVE_SHADER_D3D
#ifndef RPYROWAVE_SHADER_DXBC_ONLY
   PYROWAVE_BINARY(color, ".dxil", dxil), PYROWAVE_BINARY(dwt, ".dxil", dxil), PYROWAVE_BINARY(packet, ".dxil", dxil),
#endif
   PYROWAVE_BINARY(color, ".dxbc", dxbc), PYROWAVE_BINARY(dwt, ".dxbc", dxbc), PYROWAVE_BINARY(packet, ".dxbc", dxbc),
#endif
#ifdef RPYROWAVE_SHADER_METAL
   PYROWAVE_TEXT(color, ".metal", metal), PYROWAVE_TEXT(dwt, ".metal", metal), PYROWAVE_TEXT(packet, ".metal", metal),
#endif
   { NULL, NULL, 0 }
};
#undef PYROWAVE_TEXT
#undef PYROWAVE_BINARY
#undef RPYROWAVE_SHADER_GLSL
#undef RPYROWAVE_SHADER_SPIRV
#undef RPYROWAVE_SHADER_D3D
#undef RPYROWAVE_SHADER_DXBC_ONLY
#undef RPYROWAVE_SHADER_METAL

static const record_pyrowave_builtin_shader_t *record_pyrowave_builtin_find(const char *name)
{
   size_t i;
   if (!name) return NULL;
   for (i = 0; record_pyrowave_builtin_shaders[i].name; i++)
      if (!strcmp(name, record_pyrowave_builtin_shaders[i].name))
         return &record_pyrowave_builtin_shaders[i];
   return NULL;
}
bool record_pyrowave_source_shader_available(const char *name)
{ return record_pyrowave_builtin_find(name) != NULL; }

bool record_pyrowave_source_shader(const char *directory, const char *name,
      record_pyrowave_shader_t *shader)
{
   char path[1200];
   int64_t size = 0;
   size_t length;
   void *memory = NULL;
   RFILE *file;
   if (!shader) return false;
   memset(shader, 0, sizeof(*shader));
   if (!name) return false;
   if (!directory || !*directory)
   {
      const record_pyrowave_builtin_shader_t *builtin = record_pyrowave_builtin_find(name);
      if (!builtin) return false;
      shader->data = builtin->data; shader->bytes = builtin->bytes;
      return true;
   }
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
   shader->owned = memory; shader->data = memory; shader->bytes = (size_t)size;
   return true;
}
void record_pyrowave_source_shader_release(record_pyrowave_shader_t *shader)
{
   if (!shader) return;
   free(shader->owned); memset(shader, 0, sizeof(*shader));
}
