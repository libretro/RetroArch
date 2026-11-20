/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef COBJMACROS
#define COBJMACROS
#endif
#include "record_pyrowave_source.h"
#if defined(_WIN32) && defined(HAVE_D3D12) && !defined(__WINRT__) && !defined(_XBOX)
#include "record_pyrowave_player_d3d12.h"
#include "../record/record_pyrowave_receiver.h"
#include "../gfx/common/d3dcompiler_common.h"
#include <stdlib.h>
#include <string.h>

static bool record_pyrowave_source_d12_poll(void *user) { return record_pyrowave_d3d12_poll(user); }
static bool record_pyrowave_source_d12_idle(void *user) { return record_pyrowave_d3d12_idle(user); }
static uint64_t record_pyrowave_source_d12_retired(void *user) { return record_pyrowave_d3d12_source_retired(user); }
static bool record_pyrowave_source_d12_free(void *user) { return record_pyrowave_d3d12_free(user); }

#if defined(HAVE_PYROWAVE) && defined(HAVE_THREADS) && defined(HAVE_CONFIGFILE)
static bool record_pyrowave_source_d12_quiesce(void *user, void *renderer)
{ return video_d3d12_pyrowave_quiesce(renderer, user) > 0; }
static bool record_pyrowave_source_d12_abandon(void *user)
{ return record_pyrowave_d3d12_abandon(user); }

#endif

static record_pyrowave_d3d12_t *record_pyrowave_source_d12_create(
      const record_pyrowave_source_config_t *config, bool decode)
{
   static const char *const names[3] = { "color.hlsl", "dwt.hlsl", "packet.hlsl" };
   record_pyrowave_d3d12_config_t cfg;
   record_pyrowave_shader_t shaders[3];
   ID3DBlob *blobs[3] = { NULL, NULL, NULL };
   record_pyrowave_d3d12_t *codec = NULL;
   unsigned i;
   if (!config) return NULL;
   memset(&cfg, 0, sizeof(cfg));
   memset(shaders, 0, sizeof(shaders));
   if (config->directory && *config->directory)
   {
      for (i = 0; i < 3; i++)
      {
         record_pyrowave_shader_t text;
         if (!record_pyrowave_source_shader(config->directory, names[i], &text)) goto done;
         if (!d3d_compile((const char*)text.data, text.bytes, names[i], "main", "cs_5_0", &blobs[i]))
         { record_pyrowave_source_shader_release(&text); goto done; }
         record_pyrowave_source_shader_release(&text);
         cfg.shaders[i] = ID3D10Blob_GetBufferPointer(blobs[i]);
         cfg.shader_bytes[i] = ID3D10Blob_GetBufferSize(blobs[i]);
      }
   }
   else
   {
      static const char *const dxil[3] = { "color.dxil", "dwt.dxil", "packet.dxil" };
      static const char *const dxbc[3] = { "color.dxbc", "dwt.dxbc", "packet.dxbc" };
      const char *const *compiled = dxbc;
      D3D12_FEATURE_DATA_SHADER_MODEL model;
      if (!config->device) goto done;
      model.HighestShaderModel = D3D_SHADER_MODEL_6_0;
      if (SUCCEEDED(ID3D12Device_CheckFeatureSupport((ID3D12Device*)config->device,
                  D3D12_FEATURE_SHADER_MODEL, &model, sizeof(model)))
            && model.HighestShaderModel >= D3D_SHADER_MODEL_6_0) compiled = dxil;
      for (i = 0; i < 3; i++)
      {
         if (!record_pyrowave_source_shader(NULL, compiled[i], &shaders[i])) goto done;
         cfg.shaders[i] = shaders[i].data; cfg.shader_bytes[i] = shaders[i].bytes;
      }
   }
   cfg.device = (ID3D12Device*)config->device;
   cfg.serialize = D3D12SerializeRootSignature;
   cfg.scale_source = config->scale_source;
   cfg.sequence = config->stream.sequence;
   cfg.maximum_bytes = config->stream.maximum_frame_bytes;
   cfg.slots = config->slots; cfg.bit_depth = config->stream.bit_depth;
   cfg.texture_mode = config->texture_mode;
   cfg.texture_width = config->full_width; cfg.texture_height = config->full_height;
   cfg.x = config->x; cfg.y = config->y;
   cfg.queue_type = D3D12_COMMAND_LIST_TYPE_DIRECT;
   cfg.source_format = (DXGI_FORMAT)config->format; cfg.decode = decode;
   codec = record_pyrowave_d3d12_new(&cfg);

done:
   for (i = 0; i < 3; i++)
   {
      if (blobs[i]) ID3D10Blob_Release(blobs[i]);
      record_pyrowave_source_shader_release(&shaders[i]);
   }
   return codec;
}

record_pyrowave_d3d12_t *record_pyrowave_player_d3d12_new(
      const record_pyrowave_source_config_t *config)
{
   return record_pyrowave_source_d12_create(config, true);
}

bool record_pyrowave_source_d3d12_new(record_pyrowave_source_t *source,
      const record_pyrowave_source_config_t *config)
{
   record_pyrowave_d3d12_t *codec;
   if (!source) return false;
   codec = record_pyrowave_source_d12_create(config, false);
   if (!codec) return false;
   memset(source, 0, sizeof(*source)); source->user = codec;
   record_pyrowave_d3d12_session_video(codec, &source->video);
   source->poll = record_pyrowave_source_d12_poll;
   source->idle = record_pyrowave_source_d12_idle;
   source->retired = record_pyrowave_source_d12_retired;
   source->free = record_pyrowave_source_d12_free;
#if defined(HAVE_PYROWAVE) && defined(HAVE_THREADS) && defined(HAVE_CONFIGFILE)
   source->quiesce = record_pyrowave_source_d12_quiesce;
   source->abandon = record_pyrowave_source_d12_abandon;
#endif
   return true;
}

int record_pyrowave_source_d3d12_push(record_pyrowave_source_t *source,
      void *commands, void *fence, uint64_t value, void *texture,
      uint64_t timestamp, uint64_t duration, uint64_t *ticket)
{
   D3D12_RESOURCE_BARRIER barrier;
   int result;
   memset(&barrier, 0, sizeof(barrier)); barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
   barrier.Transition.pResource = (ID3D12Resource*)texture;
   barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
   barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
   barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
   ID3D12GraphicsCommandList_ResourceBarrier((ID3D12GraphicsCommandList*)commands, 1, &barrier);
   result = record_pyrowave_d3d12_push(source->user, (ID3D12GraphicsCommandList*)commands,
         (ID3D12Fence*)fence, value, (ID3D12Resource*)texture, timestamp, duration, ticket);
   barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
   barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
   ID3D12GraphicsCommandList_ResourceBarrier((ID3D12GraphicsCommandList*)commands, 1, &barrier);
   return result;
}
#else
bool record_pyrowave_source_d3d12_new(record_pyrowave_source_t *source,
      const record_pyrowave_source_config_t *config)
{ (void)source; (void)config; return false; }
int record_pyrowave_source_d3d12_push(record_pyrowave_source_t *source,
      void *commands, void *fence, uint64_t value, void *texture,
      uint64_t timestamp, uint64_t duration, uint64_t *ticket)
{ (void)source; (void)commands; (void)fence; (void)value; (void)texture;
  (void)timestamp; (void)duration; (void)ticket; return RECORD_GPU_QUEUE_ERROR; }
#endif
