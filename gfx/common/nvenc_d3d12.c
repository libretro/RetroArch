/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "nvenc_d3d12.h"
#if defined(_WIN32) && !defined(__WINRT__)
#ifndef COBJMACROS
#define COBJMACROS
#endif
#include <windows.h>
#include <d3d12.h>
#include <string.h>
#include <stdlib.h>
#include "../../deps/nvenc/nvEncodeAPI.h"

typedef struct nvenc_d3d12_slot
{
   ID3D12Resource *texture, *output;
   ID3D12Fence *done, *ready;
   HANDLE event;
   NV_ENC_REGISTERED_PTR registered_input, registered_output;
   NV_ENC_INPUT_RESOURCE_D3D12 input;
   NV_ENC_OUTPUT_RESOURCE_D3D12 bitstream;
   uint64_t value;
   bool busy;
} nvenc_d3d12_slot_t;

struct nvenc_d3d12
{
   HMODULE library;
   ID3D12Device *device;
   void *session;
   HANDLE eos;
   NV_ENCODE_API_FUNCTION_LIST api;
   NV_ENC_PRESET_CONFIG preset;
   NV_ENC_INITIALIZE_PARAMS init;
   NV_ENC_PIC_PARAMS picture;
   NV_ENC_LOCK_BITSTREAM lock;
   NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS open;
   NV_ENC_CAPS_PARAM caps;
   NV_ENC_EVENT_PARAMS event;
   NV_ENC_REGISTER_RESOURCE registration;
   NV_ENC_MAP_INPUT_RESOURCE mapping;
   NV_ENC_SEI_PAYLOAD metadata_payload[2];
   uint8_t metadata_bytes[28];
   unsigned metadata_count, metadata_gop, metadata_left;
   nvenc_d3d12_slot_t slots[NVENC_D3D12_SLOTS];
   unsigned order[NVENC_D3D12_SLOTS], read, count, frame;
   unsigned width, height, slot_count;
   NV_ENC_BUFFER_FORMAT format;
   bool locked, finishing;
   enum nvenc_d3d12_codec codec;
};

static bool nve_metadata_valid(const nvenc_d3d12_config_t *c)
{
   unsigned i;
   if (!c->mastering.present && !c->max_cll && !c->max_fall)
      return true;
   if (c->color < NVENC_D3D12_COLOR_BT2020_PQ
         || c->codec == NVENC_D3D12_H264
         || c->max_cll > 65535 || c->max_fall > 65535
         || (c->max_cll && c->max_fall > c->max_cll))
      return false;
   if (!c->mastering.present)
      return true;
   if (c->mastering.present != VIDEO_MASTERING_DISPLAY_COMPLETE
         || !c->mastering.luminance_max
         || c->mastering.luminance_max > 100000000
         || c->mastering.luminance_min > c->mastering.luminance_max)
      return false;
   for (i = 0; i < 8; i++)
      if (c->mastering.chromaticity[i] > 50000)
         return false;
   return true;
}

static uint16_t nve_chroma(uint16_t value, bool av1)
{
   uint32_t scaled;
   if (!av1)
      return value;
   scaled = ((uint32_t)value * 65536 + 25000) / 50000;
   return (uint16_t)(scaled > 65535 ? 65535 : scaled);
}

static void nve_be(uint8_t *p, uint32_t value, unsigned bytes)
{
   unsigned i;
   for (i = 0; i < bytes; i++)
      p[i] = (uint8_t)(value >> (8 * (bytes - i - 1)));
}

static void nve_metadata(nvenc_d3d12_t *e, const nvenc_d3d12_config_t *c)
{
   bool av1 = c->codec == NVENC_D3D12_AV1;
   e->codec = c->codec;
   if (c->mastering.present)
   {
      static const unsigned hevc_order[8] = {2, 3, 4, 5, 0, 1, 6, 7};
      NV_ENC_SEI_PAYLOAD *p = &e->metadata_payload[e->metadata_count++];
      uint8_t *b = e->metadata_bytes;
      unsigned i;
      uint32_t maximum = c->mastering.luminance_max;
      uint32_t minimum = c->mastering.luminance_min;
      p->payloadType = av1 ? 2 : 137;
      p->payloadSize = 24;
      p->payload = b;
      for (i = 0; i < 8; i++)
         nve_be(b + i * 2, nve_chroma(c->mastering.chromaticity[
                  av1 ? i : hevc_order[i]], av1), 2);
      if (av1)
      {
         maximum = (uint32_t)(((uint64_t)maximum * 256 + 5000) / 10000);
         minimum = (uint32_t)(((uint64_t)minimum * 16384 + 5000) / 10000);
      }
      nve_be(b + 16, maximum, 4);
      nve_be(b + 20, minimum, 4);
   }
   if (c->max_cll || c->max_fall)
   {
      NV_ENC_SEI_PAYLOAD *p = &e->metadata_payload[e->metadata_count++];
      p->payloadType = av1 ? 1 : 144;
      p->payloadSize = 4;
      p->payload = e->metadata_bytes + 24;
      nve_be(e->metadata_bytes + 24, c->max_cll, 2);
      nve_be(e->metadata_bytes + 26, c->max_fall, 2);
   }
   /* Use explicit payloads rather than driver-generated defaults. */
   if (av1)
   {
      e->preset.presetCfg.encodeCodecConfig.av1Config.outputMasteringDisplay = 0;
      e->preset.presetCfg.encodeCodecConfig.av1Config.outputMaxCll = 0;
   }
   else if (c->codec == NVENC_D3D12_HEVC)
   {
      e->preset.presetCfg.encodeCodecConfig.hevcConfig.outputMasteringDisplay = 0;
      e->preset.presetCfg.encodeCodecConfig.hevcConfig.outputMaxCll = 0;
   }
}

static bool nve_event(nvenc_d3d12_t *e, HANDLE *event)
{
   NV_ENC_EVENT_PARAMS *params = &e->event;
   *event = CreateEvent(NULL, TRUE, FALSE, NULL);
   if (!*event) return false;
   memset(params, 0, sizeof(*params));
   params->version = NV_ENC_EVENT_PARAMS_VER;
   params->completionEvent = *event;
   if (e->api.nvEncRegisterAsyncEvent(e->session, params) == NV_ENC_SUCCESS)
      return true;
   CloseHandle(*event);
   *event = NULL;
   return false;
}

static void nve_event_free(nvenc_d3d12_t *e, HANDLE event)
{
   NV_ENC_EVENT_PARAMS *params = &e->event;
   if (!event) return;
   memset(params, 0, sizeof(*params));
   params->version = NV_ENC_EVENT_PARAMS_VER;
   params->completionEvent = event;
   e->api.nvEncUnregisterAsyncEvent(e->session, params);
   CloseHandle(event);
}

static void nve_destroy(nvenc_d3d12_t *e)
{
   unsigned i;
   if (e->locked)
      e->api.nvEncUnlockBitstream(e->session, &e->slots[e->order[e->read]].bitstream);
   for (i = 0; i < e->slot_count; i++)
   {
      nvenc_d3d12_slot_t *s = &e->slots[i];
      if (s->input.pInputBuffer) e->api.nvEncUnmapInputResource(e->session, s->input.pInputBuffer);
      if (s->bitstream.pOutputBuffer) e->api.nvEncUnmapInputResource(e->session, s->bitstream.pOutputBuffer);
      if (s->registered_input) e->api.nvEncUnregisterResource(e->session, s->registered_input);
      if (s->registered_output) e->api.nvEncUnregisterResource(e->session, s->registered_output);
      nve_event_free(e, s->event);
      if (s->ready) ID3D12Fence_Release(s->ready);
      if (s->done) ID3D12Fence_Release(s->done);
      if (s->texture) ID3D12Resource_Release(s->texture);
      if (s->output) ID3D12Resource_Release(s->output);
   }
   nve_event_free(e, e->eos);
   if (e->session) e->api.nvEncDestroyEncoder(e->session);
   if (e->device) ID3D12Device_Release(e->device);
   if (e->library) FreeLibrary(e->library);
   free(e);
}

static bool nve_register(nvenc_d3d12_t *e, ID3D12Resource *resource,
      unsigned width, unsigned height, NV_ENC_BUFFER_FORMAT format,
      NV_ENC_BUFFER_USAGE usage, NV_ENC_REGISTERED_PTR *registered,
      NV_ENC_INPUT_PTR *mapped)
{
   NV_ENC_REGISTER_RESOURCE *params = &e->registration;
   NV_ENC_MAP_INPUT_RESOURCE *map = &e->mapping;
   memset(params, 0, sizeof(*params));
   params->version = NV_ENC_REGISTER_RESOURCE_VER;
   params->resourceType = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;
   params->width = width; params->height = height;
   params->resourceToRegister = resource;
   params->bufferFormat = format; params->bufferUsage = usage;
   if (e->api.nvEncRegisterResource(e->session, params) != NV_ENC_SUCCESS)
      return false;
   *registered = params->registeredResource;
   memset(map, 0, sizeof(*map));
   map->version = NV_ENC_MAP_INPUT_RESOURCE_VER;
   map->registeredResource = *registered;
   if (e->api.nvEncMapInputResource(e->session, map) != NV_ENC_SUCCESS)
      return false;
   *mapped = map->mappedResource;
   return true;
}

static bool nve_slot(nvenc_d3d12_t *e, unsigned i, ID3D12Resource *texture)
{
   nvenc_d3d12_slot_t *s = &e->slots[i];
   D3D12_HEAP_PROPERTIES heap;
   D3D12_RESOURCE_DESC desc;
   size_t bytes = (size_t)e->width * e->height * 4 + 65536;
   memset(&heap, 0, sizeof(heap));
   heap.Type = D3D12_HEAP_TYPE_READBACK;
   memset(&desc, 0, sizeof(desc));
   desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
   desc.Width = bytes; desc.Height = desc.DepthOrArraySize = desc.MipLevels = 1;
   desc.SampleDesc.Count = 1; desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
   if (FAILED(ID3D12Device_CreateCommittedResource(e->device, &heap,
            D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST,
            NULL, &IID_ID3D12Resource, (void**)&s->output))
         || FAILED(ID3D12Device_CreateFence(e->device, 0, D3D12_FENCE_FLAG_NONE,
            &IID_ID3D12Fence, (void**)&s->done))) return false;
   s->texture = texture;
   ID3D12Resource_AddRef(texture);
   s->input.version = NV_ENC_INPUT_RESOURCE_D3D12_VER;
   s->input.inputFencePoint.version = NV_ENC_FENCE_POINT_D3D12_VER;
   s->bitstream.version = NV_ENC_OUTPUT_RESOURCE_D3D12_VER;
   s->bitstream.outputFencePoint.version = NV_ENC_FENCE_POINT_D3D12_VER;
   s->bitstream.outputFencePoint.pFence = s->done;
   s->bitstream.outputFencePoint.bSignal = 1;
   return nve_event(e, &s->event)
      && nve_register(e, texture, e->width, e->height, e->format,
            NV_ENC_INPUT_IMAGE, &s->registered_input, &s->input.pInputBuffer)
      && nve_register(e, s->output, (unsigned)bytes, 1, NV_ENC_BUFFER_FORMAT_U8,
            NV_ENC_OUTPUT_BITSTREAM, &s->registered_output, &s->bitstream.pOutputBuffer);
}

nvenc_d3d12_t *nvenc_d3d12_open(const nvenc_d3d12_config_t *c)
{
   static const NV_ENC_BUFFER_FORMAT formats[] = {NV_ENC_BUFFER_FORMAT_ABGR,
      NV_ENC_BUFFER_FORMAT_ARGB, NV_ENC_BUFFER_FORMAT_NV12, NV_ENC_BUFFER_FORMAT_YUV420_10BIT};
   static const DXGI_FORMAT dxgi[] = {DXGI_FORMAT_R8G8B8A8_UNORM,
      DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_NV12, DXGI_FORMAT_P010};
   NVENCSTATUS (NVENCAPI *create)(NV_ENCODE_API_FUNCTION_LIST*);
   NVENCSTATUS (NVENCAPI *version)(uint32_t*);
   NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS *open;
   NV_ENC_CAPS_PARAM *caps;
   NV_ENC_BUFFER_FORMAT supported[32];
   GUID guids[16], codec;
   nvenc_d3d12_t *e;
   uint32_t maximum, count, i, j;
   int async = 0;
   NV_ENC_CONFIG_H264_VUI_PARAMETERS *vui;
   if (!c || !c->device || c->slots < 4 || c->slots > NVENC_D3D12_SLOTS
         || !c->width || !c->height || ((c->width | c->height) & 1)
         || c->width > 16384 || c->height > 16384
         || !c->fps_num || !c->fps_den || !c->bitrate
         || (unsigned)c->codec > NVENC_D3D12_AV1 || (unsigned)c->format > NVENC_D3D12_P010
         || (c->format == NVENC_D3D12_P010 && c->codec == NVENC_D3D12_H264)
         || (unsigned)c->color > NVENC_D3D12_COLOR_BT2020_HLG
         || (c->color >= NVENC_D3D12_COLOR_BT2020_PQ && c->format != NVENC_D3D12_P010)
         || (c->color == NVENC_D3D12_COLOR_DEFAULT && c->full_range)
         || !nve_metadata_valid(c)) return NULL;
   for (i = 0; i < c->slots; i++)
   {
      D3D12_RESOURCE_DESC desc;
      ID3D12Device *device = NULL;
      D3D12_RESOURCE_DESC *(STDMETHODCALLTYPE *get_desc)(ID3D12Resource*, D3D12_RESOURCE_DESC*);
      if (!c->textures[i]) return NULL;
      for (j = 0; j < i; j++) if (c->textures[j] == c->textures[i]) return NULL;
      /* Use COM's explicit return buffer across SDK header versions. */
      memcpy(&get_desc, &((ID3D12Resource*)c->textures[i])->lpVtbl->GetDesc, sizeof(get_desc));
      get_desc((ID3D12Resource*)c->textures[i], &desc);
      if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || desc.Width != c->width
            || desc.Height != c->height || desc.DepthOrArraySize != 1 || desc.MipLevels != 1
            || desc.SampleDesc.Count != 1 || desc.Format != dxgi[c->format]) return NULL;
      if (FAILED(ID3D12Resource_GetDevice((ID3D12Resource*)c->textures[i],
               &IID_ID3D12Device, (void**)&device))) return NULL;
      ID3D12Device_Release(device);
      if (device != c->device) return NULL;
   }
   e = (nvenc_d3d12_t*)calloc(1, sizeof(*e));
   if (!e) return NULL;
   open = &e->open; caps = &e->caps;
   e->slot_count = c->slots; e->width = c->width; e->height = c->height;
   e->format = formats[c->format];
   e->device = (ID3D12Device*)c->device;
   ID3D12Device_AddRef(e->device);
#ifdef _WIN64
   e->library = LoadLibraryExA("nvEncodeAPI64.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
#else
   e->library = LoadLibraryExA("nvEncodeAPI.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
#endif
   if (!e->library) goto fail;
   create = (NVENCSTATUS (NVENCAPI*)(NV_ENCODE_API_FUNCTION_LIST*))GetProcAddress(e->library, "NvEncodeAPICreateInstance");
   version = (NVENCSTATUS (NVENCAPI*)(uint32_t*))GetProcAddress(e->library, "NvEncodeAPIGetMaxSupportedVersion");
   if (!create || !version || version(&maximum) != NV_ENC_SUCCESS
         || maximum < ((NVENCAPI_MAJOR_VERSION << 4) | NVENCAPI_MINOR_VERSION)) goto fail;
   e->api.version = NV_ENCODE_API_FUNCTION_LIST_VER;
   if (create(&e->api) != NV_ENC_SUCCESS) goto fail;
   memset(open, 0, sizeof(*open));
   open->version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
   open->apiVersion = NVENCAPI_VERSION; open->device = e->device;
   open->deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
   if (e->api.nvEncOpenEncodeSessionEx(open, &e->session) != NV_ENC_SUCCESS) goto fail;
   codec = c->codec == NVENC_D3D12_H264 ? NV_ENC_CODEC_H264_GUID
      : c->codec == NVENC_D3D12_HEVC ? NV_ENC_CODEC_HEVC_GUID : NV_ENC_CODEC_AV1_GUID;
   if (e->api.nvEncGetEncodeGUIDs(e->session, guids, 16, &count) != NV_ENC_SUCCESS) goto fail;
   for (i = 0; i < count && i < 16; i++) if (!memcmp(&codec, &guids[i], sizeof(codec))) break;
   if (i == count || i == 16) goto fail;
   if (e->api.nvEncGetInputFormats(e->session, codec, supported, 32, &count) != NV_ENC_SUCCESS) goto fail;
   for (i = 0; i < count && i < 32; i++) if (supported[i] == e->format) break;
   if (i == count || i == 32) goto fail;
   memset(caps, 0, sizeof(*caps)); caps->version = NV_ENC_CAPS_PARAM_VER;
   caps->capsToQuery = NV_ENC_CAPS_ASYNC_ENCODE_SUPPORT;
   if (e->api.nvEncGetEncodeCaps(e->session, codec, caps, &async) != NV_ENC_SUCCESS || !async) goto fail;
   if (c->format == NVENC_D3D12_P010)
   {
      caps->capsToQuery = NV_ENC_CAPS_SUPPORT_10BIT_ENCODE;
      if (e->api.nvEncGetEncodeCaps(e->session, codec, caps, &async) != NV_ENC_SUCCESS || !async) goto fail;
   }
   e->preset.version = NV_ENC_PRESET_CONFIG_VER;
   e->preset.presetCfg.version = NV_ENC_CONFIG_VER;
   if (e->api.nvEncGetEncodePresetConfigEx(e->session, codec, NV_ENC_PRESET_P1_GUID,
            NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY, &e->preset) != NV_ENC_SUCCESS) goto fail;
   e->preset.presetCfg.frameIntervalP = 1;
   e->preset.presetCfg.gopLength = c->fps_num / c->fps_den;
   if (!e->preset.presetCfg.gopLength) e->preset.presetCfg.gopLength = 1;
   e->preset.presetCfg.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR;
   e->preset.presetCfg.rcParams.averageBitRate = c->bitrate;
   e->preset.presetCfg.rcParams.maxBitRate = c->bitrate;
   e->preset.presetCfg.rcParams.enableLookahead = 0;
   if (c->codec == NVENC_D3D12_HEVC)
   {
      e->preset.presetCfg.encodeCodecConfig.hevcConfig.inputBitDepth = c->format == NVENC_D3D12_P010 ? NV_ENC_BIT_DEPTH_10 : NV_ENC_BIT_DEPTH_8;
      e->preset.presetCfg.encodeCodecConfig.hevcConfig.outputBitDepth = e->preset.presetCfg.encodeCodecConfig.hevcConfig.inputBitDepth;
      e->preset.presetCfg.encodeCodecConfig.hevcConfig.repeatSPSPPS = 1;
   }
   else if (c->codec == NVENC_D3D12_AV1)
   {
      e->preset.presetCfg.encodeCodecConfig.av1Config.inputBitDepth = c->format == NVENC_D3D12_P010 ? NV_ENC_BIT_DEPTH_10 : NV_ENC_BIT_DEPTH_8;
      e->preset.presetCfg.encodeCodecConfig.av1Config.outputBitDepth = e->preset.presetCfg.encodeCodecConfig.av1Config.inputBitDepth;
      e->preset.presetCfg.encodeCodecConfig.av1Config.repeatSeqHdr = 1;
   }
   else e->preset.presetCfg.encodeCodecConfig.h264Config.repeatSPSPPS = 1;
   if (c->color != NVENC_D3D12_COLOR_DEFAULT)
   {
      NV_ENC_VUI_COLOR_PRIMARIES primaries = c->color == NVENC_D3D12_COLOR_BT709
         ? NV_ENC_VUI_COLOR_PRIMARIES_BT709 : NV_ENC_VUI_COLOR_PRIMARIES_BT2020;
      NV_ENC_VUI_TRANSFER_CHARACTERISTIC transfer = c->color == NVENC_D3D12_COLOR_BT709
         ? NV_ENC_VUI_TRANSFER_CHARACTERISTIC_BT709
         : c->color == NVENC_D3D12_COLOR_BT2020_PQ
         ? NV_ENC_VUI_TRANSFER_CHARACTERISTIC_SMPTE2084 : NV_ENC_VUI_TRANSFER_CHARACTERISTIC_ARIB_STD_B67;
      NV_ENC_VUI_MATRIX_COEFFS matrix = c->color == NVENC_D3D12_COLOR_BT709
         ? NV_ENC_VUI_MATRIX_COEFFS_BT709 : NV_ENC_VUI_MATRIX_COEFFS_BT2020_NCL;
      if (c->codec == NVENC_D3D12_AV1)
      {
         e->preset.presetCfg.encodeCodecConfig.av1Config.colorPrimaries = primaries;
         e->preset.presetCfg.encodeCodecConfig.av1Config.transferCharacteristics = transfer;
         e->preset.presetCfg.encodeCodecConfig.av1Config.matrixCoefficients = matrix;
         e->preset.presetCfg.encodeCodecConfig.av1Config.colorRange = c->full_range;
      }
      else
      {
         vui = c->codec == NVENC_D3D12_H264
            ? &e->preset.presetCfg.encodeCodecConfig.h264Config.h264VUIParameters
            : &e->preset.presetCfg.encodeCodecConfig.hevcConfig.hevcVUIParameters;
         vui->videoSignalTypePresentFlag = vui->colourDescriptionPresentFlag = 1;
         vui->videoFormat = NV_ENC_VUI_VIDEO_FORMAT_UNSPECIFIED;
         vui->videoFullRangeFlag = c->full_range;
         vui->colourPrimaries = primaries;
         vui->transferCharacteristics = transfer;
         vui->colourMatrix = matrix;
      }
   }
   nve_metadata(e, c);
   /* Own the cadence so each random-access frame carries metadata. */
   if (e->metadata_count)
   {
      e->metadata_gop = e->preset.presetCfg.gopLength;
      e->preset.presetCfg.gopLength = NVENC_INFINITE_GOPLENGTH;
      if (c->codec == NVENC_D3D12_AV1)
         e->preset.presetCfg.encodeCodecConfig.av1Config.idrPeriod = NVENC_INFINITE_GOPLENGTH;
      else
         e->preset.presetCfg.encodeCodecConfig.hevcConfig.idrPeriod = NVENC_INFINITE_GOPLENGTH;
   }
   e->init.version = NV_ENC_INITIALIZE_PARAMS_VER;
   e->init.encodeGUID = codec; e->init.presetGUID = NV_ENC_PRESET_P1_GUID;
   e->init.encodeWidth = e->init.darWidth = e->init.maxEncodeWidth = c->width;
   e->init.encodeHeight = e->init.darHeight = e->init.maxEncodeHeight = c->height;
   e->init.frameRateNum = c->fps_num; e->init.frameRateDen = c->fps_den;
   e->init.enablePTD = e->init.enableEncodeAsync = 1;
   e->init.bufferFormat = e->format;
   e->init.encodeConfig = &e->preset.presetCfg;
   e->init.tuningInfo = NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY;
   if (e->api.nvEncInitializeEncoder(e->session, &e->init) != NV_ENC_SUCCESS || !nve_event(e, &e->eos)) goto fail;
   for (i = 0; i < c->slots; i++) if (!nve_slot(e, i, (ID3D12Resource*)c->textures[i])) goto fail;
   return e;
fail:
   nve_destroy(e);
   return NULL;
}

bool nvenc_d3d12_headers(nvenc_d3d12_t *e, void *data, size_t capacity, size_t *size)
{
   NV_ENC_SEQUENCE_PARAM_PAYLOAD payload;
   uint32_t written = 0;
   if (size) *size = 0;
   if (!e || !data || !size || !capacity || capacity > UINT32_MAX
         || e->frame || e->finishing || !e->api.nvEncGetSequenceParams) return false;
   memset(&payload, 0, sizeof(payload));
   payload.version = NV_ENC_SEQUENCE_PARAM_PAYLOAD_VER;
   payload.inBufferSize = (uint32_t)capacity;
   payload.spsppsBuffer = data;
   payload.outSPSPPSPayloadSize = &written;
   if (e->api.nvEncGetSequenceParams(e->session, &payload) != NV_ENC_SUCCESS
         || !written || written > capacity) return false;
   *size = written;
   return true;
}

int nvenc_d3d12_submit(nvenc_d3d12_t *e, unsigned index, uint64_t timestamp,
      uint64_t duration, void *ready, uint64_t value, bool keyframe)
{
   nvenc_d3d12_slot_t *s;
   NVENCSTATUS status;
   if (!e || e->finishing || index >= e->slot_count || !duration || (!ready && value)) return NVENC_D3D12_ERROR;
   s = &e->slots[index];
   if (s->busy || e->count == e->slot_count) return NVENC_D3D12_BUSY;
   if (ready)
   {
      ID3D12Device *device = NULL;
      if (FAILED(ID3D12Fence_GetDevice((ID3D12Fence*)ready, &IID_ID3D12Device, (void**)&device)))
         return NVENC_D3D12_ERROR;
      ID3D12Device_Release(device);
      if (device != e->device) return NVENC_D3D12_ERROR;
   }
   s->input.inputFencePoint.pFence = ready;
   s->input.inputFencePoint.waitValue = value;
   s->input.inputFencePoint.bWait = ready ? 1 : 0;
   s->bitstream.outputFencePoint.signalValue = s->value + 1;
   memset(&e->picture, 0, sizeof(e->picture));
   e->picture.version = NV_ENC_PIC_PARAMS_VER;
   e->picture.inputBuffer = &s->input; e->picture.outputBitstream = &s->bitstream;
   e->picture.inputWidth = e->width; e->picture.inputHeight = e->height;
   e->picture.bufferFmt = e->format;
   e->picture.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
   e->picture.inputTimeStamp = timestamp; e->picture.inputDuration = duration;
   e->picture.frameIdx = e->frame;
   e->picture.completionEvent = s->event;
   if (e->metadata_count && !e->metadata_left)
      keyframe = true;
   if (keyframe) e->picture.encodePicFlags = NV_ENC_PIC_FLAG_FORCEIDR | NV_ENC_PIC_FLAG_OUTPUT_SPSPPS;
   if (e->metadata_count && keyframe)
   {
      if (e->codec == NVENC_D3D12_HEVC)
      {
         e->picture.codecPicParams.hevcPicParams.seiPayloadArray = e->metadata_payload;
         e->picture.codecPicParams.hevcPicParams.seiPayloadArrayCnt = e->metadata_count;
      }
      else if (e->codec == NVENC_D3D12_AV1)
      {
         e->picture.codecPicParams.av1PicParams.obuPayloadArray = e->metadata_payload;
         e->picture.codecPicParams.av1PicParams.obuPayloadArrayCnt = e->metadata_count;
      }
   }
   ResetEvent(s->event);
   status = e->api.nvEncEncodePicture(e->session, &e->picture);
   if (status == NV_ENC_ERR_ENCODER_BUSY) return NVENC_D3D12_BUSY;
   if (status != NV_ENC_SUCCESS && status != NV_ENC_ERR_NEED_MORE_INPUT) return NVENC_D3D12_ERROR;
   s->value++; s->busy = true;
   s->ready = (ID3D12Fence*)ready;
   if (s->ready) ID3D12Fence_AddRef(s->ready);
   e->order[(e->read + e->count) % e->slot_count] = index;
   e->count++; e->frame++;
   if (e->metadata_count)
      e->metadata_left = keyframe ? e->metadata_gop - 1 : e->metadata_left - 1;
   return NVENC_D3D12_OK;
}

int nvenc_d3d12_poll(nvenc_d3d12_t *e, nvenc_d3d12_packet_t *packet)
{
   nvenc_d3d12_slot_t *s;
   NVENCSTATUS status;
   uint64_t done;
   if (!e || !packet || e->locked) return NVENC_D3D12_ERROR;
   if (!e->count)
      return e->finishing && WaitForSingleObject(e->eos, 0) == WAIT_OBJECT_0 ? NVENC_D3D12_END : NVENC_D3D12_BUSY;
   s = &e->slots[e->order[e->read]];
   done = ID3D12Fence_GetCompletedValue(s->done);
   if (done == UINT64_MAX) return NVENC_D3D12_ERROR;
   if (done < s->value) return NVENC_D3D12_BUSY;
   memset(&e->lock, 0, sizeof(e->lock));
   e->lock.version = NV_ENC_LOCK_BITSTREAM_VER;
   e->lock.outputBitstream = &s->bitstream; e->lock.doNotWait = 1;
   status = e->api.nvEncLockBitstream(e->session, &e->lock);
   if (status == NV_ENC_ERR_LOCK_BUSY) return NVENC_D3D12_BUSY;
   if (status != NV_ENC_SUCCESS) return NVENC_D3D12_ERROR;
   e->locked = true;
   packet->data = e->lock.bitstreamBufferPtr;
   packet->size = e->lock.bitstreamSizeInBytes;
   packet->timestamp = e->lock.outputTimeStamp;
   packet->duration = e->lock.outputDuration;
   packet->slot = e->order[e->read];
   packet->keyframe = e->lock.pictureType == NV_ENC_PIC_TYPE_IDR || e->lock.pictureType == NV_ENC_PIC_TYPE_I;
   return NVENC_D3D12_OK;
}

void *nvenc_d3d12_wait_handle(const nvenc_d3d12_t *e)
{
   if (!e || e->locked) return NULL;
   if (e->count) return e->slots[e->order[e->read]].event;
   return e->finishing ? e->eos : NULL;
}

int nvenc_d3d12_release(nvenc_d3d12_t *e)
{
   nvenc_d3d12_slot_t *s;
   if (!e || !e->locked) return NVENC_D3D12_ERROR;
   s = &e->slots[e->order[e->read]];
   if (e->api.nvEncUnlockBitstream(e->session, &s->bitstream) != NV_ENC_SUCCESS) return NVENC_D3D12_ERROR;
   if (s->ready) ID3D12Fence_Release(s->ready);
   s->ready = NULL; s->busy = false;
   e->locked = false; e->count--; e->read = (e->read + 1) % e->slot_count;
   return NVENC_D3D12_OK;
}

int nvenc_d3d12_finish(nvenc_d3d12_t *e)
{
   NVENCSTATUS status;
   if (!e) return NVENC_D3D12_ERROR;
   if (e->finishing) return NVENC_D3D12_OK;
   memset(&e->picture, 0, sizeof(e->picture));
   e->picture.version = NV_ENC_PIC_PARAMS_VER;
   e->picture.encodePicFlags = NV_ENC_PIC_FLAG_EOS;
   e->picture.completionEvent = e->eos;
   status = e->api.nvEncEncodePicture(e->session, &e->picture);
   if (status == NV_ENC_ERR_ENCODER_BUSY) return NVENC_D3D12_BUSY;
   if (status != NV_ENC_SUCCESS) return NVENC_D3D12_ERROR;
   e->finishing = true;
   return NVENC_D3D12_OK;
}

bool nvenc_d3d12_free(nvenc_d3d12_t *e)
{
   if (!e) return true;
   if (FAILED(ID3D12Device_GetDeviceRemovedReason(e->device)))
   {
      nve_destroy(e);
      return true;
   }
   if (e->count || e->locked) return false;
   if (e->finishing && WaitForSingleObject(e->eos, 0) != WAIT_OBJECT_0) return false;
   nve_destroy(e);
   return true;
}
#else
nvenc_d3d12_t *nvenc_d3d12_open(const nvenc_d3d12_config_t *config) { (void)config; return NULL; }
bool nvenc_d3d12_headers(nvenc_d3d12_t *e, void *data, size_t capacity, size_t *size)
{
   (void)e; (void)data; (void)capacity; if (size) *size = 0; return false;
}
int nvenc_d3d12_submit(nvenc_d3d12_t *e, unsigned slot, uint64_t timestamp,
      uint64_t duration, void *fence, uint64_t value, bool keyframe)
{
   (void)e; (void)slot; (void)timestamp; (void)duration; (void)fence; (void)value; (void)keyframe;
   return NVENC_D3D12_ERROR;
}
int nvenc_d3d12_poll(nvenc_d3d12_t *e, nvenc_d3d12_packet_t *p) { (void)e; (void)p; return NVENC_D3D12_ERROR; }
void *nvenc_d3d12_wait_handle(const nvenc_d3d12_t *e)
{
   (void)e; return NULL;
}

int nvenc_d3d12_release(nvenc_d3d12_t *e) { (void)e; return NVENC_D3D12_ERROR; }
int nvenc_d3d12_finish(nvenc_d3d12_t *e) { (void)e; return NVENC_D3D12_ERROR; }
bool nvenc_d3d12_free(nvenc_d3d12_t *e) { return e == NULL; }
#endif
