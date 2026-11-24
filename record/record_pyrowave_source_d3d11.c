/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef COBJMACROS
#define COBJMACROS
#endif
#include "record_pyrowave_source.h"
#if defined(_WIN32) && defined(HAVE_D3D11) && !defined(__WINRT__) && !defined(_XBOX)
#include "record_pyrowave_player_d3d11.h"
#include "../gfx/common/d3dcompiler_common.h"
#include <string.h>
static bool record_pyrowave_source_d11_poll(void *p) { return record_pyrowave_d3d11_poll(p); }
static bool record_pyrowave_source_d11_idle(void *p) { return record_pyrowave_d3d11_idle(p); }
static bool record_pyrowave_source_d11_free(void *p) { return record_pyrowave_d3d11_free(p); }
static uint64_t record_pyrowave_source_d11_retired(void *p) { return record_pyrowave_d3d11_source_retired(p); }
static bool record_pyrowave_source_d11_quiesce(void *p, void *renderer)
{ (void)renderer; return record_pyrowave_d3d11_quiesce(p); }
static bool record_pyrowave_source_d11_abandon(void *p) { return record_pyrowave_d3d11_abandon(p); }
static record_pyrowave_d3d11_t *record_pyrowave_source_d11_create(const record_pyrowave_source_config_t *config, bool decode)
{
   static const char *const names[] = {"color", "dwt", "packet"};
   record_pyrowave_d3d11_config_t cfg;
   record_pyrowave_shader_t shaders[3];
   ID3DBlob *blobs[3] = {NULL, NULL, NULL};
   record_pyrowave_d3d11_t *codec = NULL;
   unsigned i;
   if (!config) return NULL;
   memset(&cfg, 0, sizeof(cfg)); memset(shaders, 0, sizeof(shaders));
   for (i = 0; i < 3; i++)
   {
      char name[24];
      bool text = config->directory && *config->directory;
      strcpy(name, names[i]); strcat(name, text ? ".hlsl" : ".dxbc");
      if (!record_pyrowave_source_shader(config->directory, name, &shaders[i])) goto done;
      if (text)
      {
         if (!d3d_compile((const char*)shaders[i].data, shaders[i].bytes, name, "main", "cs_5_0", &blobs[i])) goto done;
         cfg.shaders[i] = ID3D10Blob_GetBufferPointer(blobs[i]); cfg.shader_bytes[i] = ID3D10Blob_GetBufferSize(blobs[i]);
      }
      else { cfg.shaders[i] = shaders[i].data; cfg.shader_bytes[i] = shaders[i].bytes; }
   }
   cfg.device = (ID3D11Device*)config->device; cfg.context = (ID3D11DeviceContext*)config->queue;
   cfg.sequence = config->stream.sequence; cfg.maximum_bytes = config->stream.maximum_frame_bytes;
   cfg.slots = config->slots; cfg.bit_depth = config->stream.bit_depth; cfg.texture_mode = config->texture_mode;
   cfg.texture_width = config->full_width; cfg.texture_height = config->full_height;
   cfg.x = config->x; cfg.y = config->y; cfg.decode = decode; cfg.scale_source = config->scale_source;
   codec = record_pyrowave_d3d11_new(&cfg);
done:
   for (i = 0; i < 3; i++)
   {
      if (blobs[i]) ID3D10Blob_Release(blobs[i]);
      record_pyrowave_source_shader_release(&shaders[i]);
   }
   return codec;
}
record_pyrowave_d3d11_t *record_pyrowave_player_d3d11_new(const record_pyrowave_source_config_t *config)
{ return record_pyrowave_source_d11_create(config, true); }
bool record_pyrowave_source_d3d11_new(record_pyrowave_source_t *source, const record_pyrowave_source_config_t *config)
{
   record_pyrowave_d3d11_t *codec;
   if (!source) return false;
   codec = record_pyrowave_source_d11_create(config, false);
   if (!codec) return false;
   memset(source, 0, sizeof(*source)); source->user = codec;
   record_pyrowave_d3d11_session_video(codec, &source->video);
   source->poll = record_pyrowave_source_d11_poll; source->idle = record_pyrowave_source_d11_idle;
   source->retired = record_pyrowave_source_d11_retired; source->free = record_pyrowave_source_d11_free;
   source->quiesce = record_pyrowave_source_d11_quiesce; source->abandon = record_pyrowave_source_d11_abandon;
   return true;
}
int record_pyrowave_source_d3d11_push(record_pyrowave_source_t *source, void *view, uint64_t pts, uint64_t duration, uint64_t *ticket)
{ return record_pyrowave_d3d11_push(source->user, (ID3D11ShaderResourceView*)view, pts, duration, ticket); }
#else
bool record_pyrowave_source_d3d11_new(record_pyrowave_source_t *source, const record_pyrowave_source_config_t *config)
{ (void)source; (void)config; return false; }
int record_pyrowave_source_d3d11_push(record_pyrowave_source_t *source, void *view, uint64_t pts, uint64_t duration, uint64_t *ticket)
{ (void)source; (void)view; (void)pts; (void)duration; (void)ticket; return RECORD_GPU_QUEUE_ERROR; }
#endif
