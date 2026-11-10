/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_mf_audio.h"
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) && defined(HAVE_MF_AUDIO)
#ifndef COBJMACROS
#define COBJMACROS
#endif
#include <windows.h>
#include <mfapi.h>
#include <mftransform.h>
#include <mferror.h>
/* Private IDs avoid bulk GUID libraries and SDK target gating. */
static const GUID record_mf_major =
{ 0x48eba18e, 0xf8c9, 0x4687, { 0xbf, 0x11, 0x0a, 0x74, 0xc9, 0xf9, 0x6a, 0x8f } };
static const GUID record_mf_subtype =
{ 0xf7e34c9a, 0x42e8, 0x4714, { 0xb7, 0x4b, 0xcb, 0x29, 0xd7, 0x2c, 0x35, 0xe5 } };
static const GUID record_mf_bits =
{ 0xf2deb57f, 0x40fa, 0x4764, { 0xaa, 0x33, 0xed, 0x4f, 0x2d, 0x1f, 0xf6, 0x69 } };
static const GUID record_mf_rate =
{ 0x5faeeae7, 0x0290, 0x4c31, { 0x9e, 0x8a, 0xc5, 0x34, 0xf6, 0x8d, 0x9d, 0xba } };
static const GUID record_mf_channels =
{ 0x37e48bf5, 0x645e, 0x4c5b, { 0x89, 0xde, 0xad, 0xa9, 0xe2, 0x9b, 0x69, 0x6a } };
static const GUID record_mf_average =
{ 0x1aab75c8, 0xcfef, 0x451c, { 0xab, 0x95, 0xac, 0x03, 0x4b, 0x8e, 0x17, 0x31 } };
static const GUID record_mf_user_data =
{ 0xb6bc765f, 0x4c3b, 0x40a4, { 0xbd, 0x51, 0x25, 0x35, 0xb6, 0x6f, 0xe0, 0x9d } };
static const GUID record_mf_audio_type_id =
{ 0x73647561, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
static const GUID record_mf_pcm =
{ 0x00000001, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
static const GUID record_mf_aac =
{ 0x00001610, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
static const GUID record_mf_transform =
{ 0xbf94c121, 0x5b05, 0x4e6f, { 0x80, 0x00, 0xba, 0x59, 0x89, 0x61, 0x41, 0x4d } };
static const GUID record_mf_encoder =
{ 0x93af0c51, 0x2275, 0x45d2, { 0xa3, 0x5b, 0xf2, 0xba, 0x21, 0xca, 0xed, 0x00 } };
static const GUID record_mf_payload_type =
{ 0xbfbabe79, 0x7434, 0x4d1c, { 0x94, 0xf0, 0x72, 0xa3, 0xb9, 0xe1, 0x71, 0x88 } };

typedef struct record_mf_audio_api
{
   HRESULT (WINAPI *startup)(ULONG, DWORD);
   HRESULT (WINAPI *shutdown)(void);
   HRESULT (WINAPI *type)(IMFMediaType **);
   HRESULT (WINAPI *sample)(IMFSample **);
   HRESULT (WINAPI *buffer)(DWORD, DWORD, IMFMediaBuffer **);
   HRESULT (WINAPI *initialize)(void *, DWORD);
   void (WINAPI *uninitialize)(void);
   HRESULT (WINAPI *instance)(REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID *);
} record_mf_audio_api_t;

struct record_mf_audio
{
   uint64_t next, prepared_index;
   record_mf_audio_packet_t packet;
   record_mf_audio_api_t api;
   IMFTransform *transform;
   IMFSample *input, *output, *packet_sample;
   IMFMediaBuffer *input_buffer, *output_buffer, *packet_buffer;
   HMODULE mf, ole;
   BYTE *input_data;
   size_t prepared_frames, config_size;
   DWORD owner, input_alignment;
   HRESULT error;
   unsigned rate, channels;
   uint8_t config[64];
   bool initialized, started, input_locked, packet_locked, provides_samples;
   bool have_input, needs_output, draining, ended, cancelled, cancel_ok;
};

static HMODULE record_mf_audio_library(const char *name)
{
   char path[MAX_PATH + 32];
   UINT size = GetSystemDirectoryA(path, MAX_PATH);
   size_t length = strlen(name);
   if (!size || size >= MAX_PATH || length + size + 2 > sizeof(path))
      return NULL;
   path[size++] = '\\';
   memcpy(path + size, name, length + 1);
   return LoadLibraryA(path);
}

static bool record_mf_audio_symbol(HMODULE module, const char *name, void *target, size_t size)
{
   FARPROC proc = GetProcAddress(module, name);
   if (!proc || size != sizeof(proc))
      return false;
   memcpy(target, &proc, size);
   return true;
}

static bool record_mf_audio_owner(const record_mf_audio_t *e)
{
   return e && e->owner == GetCurrentThreadId();
}

static int record_mf_audio_fail(record_mf_audio_t *e, HRESULT error)
{
   if (!e->error)
      e->error = FAILED(error) ? error : E_UNEXPECTED;
   return RECORD_MF_AUDIO_ERROR;
}

static void record_mf_audio_drop_input(record_mf_audio_t *e)
{
   if (e->input_locked)
      IMFMediaBuffer_Unlock(e->input_buffer);
   e->input_locked = false;
   e->input_data = NULL;
   if (e->input)
      IMFSample_Release(e->input);
   if (e->input_buffer)
      IMFMediaBuffer_Release(e->input_buffer);
   e->input = NULL;
   e->input_buffer = NULL;
   e->prepared_frames = 0;
}

static HRESULT record_mf_audio_drop_packet(record_mf_audio_t *e)
{
   HRESULT hr = S_OK;
   if (e->packet_locked)
      hr = IMFMediaBuffer_Unlock(e->packet_buffer);
   e->packet_locked = false;
   if (e->packet_buffer)
      IMFMediaBuffer_Release(e->packet_buffer);
   if (e->packet_sample && e->packet_sample != e->output)
      IMFSample_Release(e->packet_sample);
   e->packet_buffer = NULL;
   e->packet_sample = NULL;
   memset(&e->packet, 0, sizeof(e->packet));
   return hr;
}

static void record_mf_audio_destroy(record_mf_audio_t *e)
{
   record_mf_audio_drop_input(e);
   record_mf_audio_drop_packet(e);
   if (e->transform)
      IMFTransform_Release(e->transform);
   if (e->output)
      IMFSample_Release(e->output);
   if (e->output_buffer)
      IMFMediaBuffer_Release(e->output_buffer);
   if (e->started)
      e->api.shutdown();
   if (e->initialized)
      e->api.uninitialize();
   if (e->mf)
      FreeLibrary(e->mf);
   if (e->ole)
      FreeLibrary(e->ole);
   free(e);
}

static bool record_mf_audio_clock(uint64_t index, unsigned rate, LONGLONG *time)
{
   uint64_t maximum = (~(uint64_t)0) >> 1;
   uint64_t seconds = index / rate;
   uint64_t fraction = (index % rate) * 10000000 / rate;
   if (seconds > (maximum - fraction) / 10000000)
      return false;
   *time = (LONGLONG)(seconds * 10000000 + fraction);
   return true;
}

static HRESULT record_mf_audio_type(record_mf_audio_t *e, bool output, IMFMediaType **type)
{
   HRESULT hr = e->api.type(type);
   if (SUCCEEDED(hr)) hr = IMFMediaType_SetGUID(*type, &record_mf_major, &record_mf_audio_type_id);
   if (SUCCEEDED(hr)) hr = IMFMediaType_SetGUID(*type, &record_mf_subtype,
         output ? &record_mf_aac : &record_mf_pcm);
   if (SUCCEEDED(hr)) hr = IMFMediaType_SetUINT32(*type, &record_mf_bits, 16);
   if (SUCCEEDED(hr)) hr = IMFMediaType_SetUINT32(*type, &record_mf_rate, e->rate);
   if (SUCCEEDED(hr)) hr = IMFMediaType_SetUINT32(*type, &record_mf_channels, e->channels);
   return hr;
}

record_mf_audio_t *record_mf_audio_new(unsigned rate, unsigned channels, unsigned bitrate)
{
   record_mf_audio_t *e;
   IMFMediaType *type = NULL;
   MFT_OUTPUT_STREAM_INFO output;
   MFT_INPUT_STREAM_INFO input;
   BYTE data[76];
   UINT32 size = 0;
   DWORD capacity, alignment;
   HRESULT hr;
   if ((rate != 44100 && rate != 48000) || !channels || channels > 2
         || (bitrate != 96000 && bitrate != 128000 && bitrate != 160000 && bitrate != 192000))
      return NULL;
   e = (record_mf_audio_t*)calloc(1, sizeof(*e));
   if (!e)
      return NULL;
   e->owner = GetCurrentThreadId();
   e->rate = rate;
   e->channels = channels;
   e->mf = record_mf_audio_library("mfplat.dll");
   e->ole = record_mf_audio_library("ole32.dll");
   if (!e->mf || !e->ole)
      goto failed;
#define RECORD_MF_LOAD(module, field, name) \
   if (!record_mf_audio_symbol(e->module, name, &e->api.field, sizeof(e->api.field))) goto failed
   RECORD_MF_LOAD(mf, startup, "MFStartup");
   RECORD_MF_LOAD(mf, shutdown, "MFShutdown");
   RECORD_MF_LOAD(mf, type, "MFCreateMediaType");
   RECORD_MF_LOAD(mf, sample, "MFCreateSample");
   RECORD_MF_LOAD(mf, buffer, "MFCreateAlignedMemoryBuffer");
   RECORD_MF_LOAD(ole, initialize, "CoInitializeEx");
   RECORD_MF_LOAD(ole, uninitialize, "CoUninitialize");
   RECORD_MF_LOAD(ole, instance, "CoCreateInstance");
#undef RECORD_MF_LOAD
   hr = e->api.initialize(NULL, COINIT_MULTITHREADED);
   if (FAILED(hr)) goto failed;
   e->initialized = true;
   hr = e->api.startup(MF_VERSION, MFSTARTUP_LITE);
   if (FAILED(hr)) goto failed;
   e->started = true;
   hr = e->api.instance(&record_mf_encoder, NULL, CLSCTX_INPROC_SERVER,
         &record_mf_transform, (void**)&e->transform);
   if (FAILED(hr)) goto failed;
   hr = record_mf_audio_type(e, true, &type);
   if (SUCCEEDED(hr)) hr = IMFMediaType_SetUINT32(type, &record_mf_average, bitrate / 8);
   if (SUCCEEDED(hr)) hr = IMFMediaType_SetUINT32(type, &record_mf_payload_type, 0);
   if (SUCCEEDED(hr)) hr = IMFTransform_SetOutputType(e->transform, 0, type, 0);
   if (type) IMFMediaType_Release(type);
   type = NULL;
   if (FAILED(hr)) goto failed;
   hr = IMFTransform_GetOutputCurrentType(e->transform, 0, &type);
   if (SUCCEEDED(hr)) hr = IMFMediaType_GetBlob(type, &record_mf_user_data, data, sizeof(data), &size);
   if (type) IMFMediaType_Release(type);
   type = NULL;
   if (FAILED(hr) || size < 14 || size > sizeof(data) || data[0] || data[1]
         || (data[12] >> 3) != 2 || ((data[13] >> 3) & 15) != channels
         || (((data[12] & 7) << 1) | (data[13] >> 7)) != (rate == 48000 ? 3u : 4u)
         || (data[13] & 7)) goto failed;
   e->config_size = size - 12;
   memcpy(e->config, data + 12, e->config_size);
   hr = record_mf_audio_type(e, false, &type);
   if (SUCCEEDED(hr)) hr = IMFTransform_SetInputType(e->transform, 0, type, 0);
   if (type) IMFMediaType_Release(type);
   type = NULL;
   if (FAILED(hr)) goto failed;
   hr = IMFTransform_GetInputStreamInfo(e->transform, 0, &input);
   if (FAILED(hr) || input.cbAlignment > 65536
         || (input.cbAlignment && (input.cbAlignment & (input.cbAlignment - 1)))) goto failed;
   e->input_alignment = input.cbAlignment > 2 ? input.cbAlignment - 1 : 1;
   hr = IMFTransform_GetOutputStreamInfo(e->transform, 0, &output);
   if (FAILED(hr) || output.cbAlignment > 65536
         || (output.cbAlignment && (output.cbAlignment & (output.cbAlignment - 1)))) goto failed;
   e->provides_samples = (output.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) != 0;
   if (!e->provides_samples)
   {
      capacity = output.cbSize ? output.cbSize : 65536;
      alignment = output.cbAlignment > 1 ? output.cbAlignment - 1 : 0;
      if (capacity > 1048576) goto failed;
      hr = e->api.buffer(capacity, alignment, &e->output_buffer);
      if (SUCCEEDED(hr)) hr = e->api.sample(&e->output);
      if (SUCCEEDED(hr)) hr = IMFSample_AddBuffer(e->output, e->output_buffer);
      if (FAILED(hr)) goto failed;
   }
   hr = IMFTransform_ProcessMessage(e->transform, MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
   if (SUCCEEDED(hr)) hr = IMFTransform_ProcessMessage(e->transform, MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
   if (FAILED(hr)) goto failed;
   return e;
failed:
   if (type) IMFMediaType_Release(type);
   record_mf_audio_destroy(e);
   return NULL;
}

int record_mf_audio_input(record_mf_audio_t *e, int16_t **samples, size_t *frames)
{
   DWORD capacity = 0;
   HRESULT hr;
   if (samples) *samples = NULL;
   if (frames) *frames = 0;
   if (!record_mf_audio_owner(e) || !samples || !frames)
      return RECORD_MF_AUDIO_ERROR;
   if (e->error || e->draining || e->cancelled)
      return RECORD_MF_AUDIO_ERROR;
   if (e->packet_locked || (e->input && !e->input_locked)
         || (e->needs_output && !e->input))
      return RECORD_MF_AUDIO_BUSY;
   if (!e->input)
   {
      hr = e->api.buffer(1024 * e->channels * 2, e->input_alignment, &e->input_buffer);
      if (SUCCEEDED(hr)) hr = e->api.sample(&e->input);
      if (SUCCEEDED(hr)) hr = IMFSample_AddBuffer(e->input, e->input_buffer);
      if (SUCCEEDED(hr)) hr = IMFMediaBuffer_Lock(e->input_buffer, &e->input_data, &capacity, NULL);
      if (FAILED(hr))
      {
         record_mf_audio_drop_input(e);
         return record_mf_audio_fail(e, hr);
      }
      e->input_locked = true;
      if (!e->input_data || capacity < 1024 * e->channels * 2)
         return record_mf_audio_fail(e, E_UNEXPECTED);
   }
   *samples = (int16_t*)e->input_data;
   *frames = 1024;
   return RECORD_MF_AUDIO_PROGRESS;
}

int record_mf_audio_submit(record_mf_audio_t *e, size_t frames, uint64_t index)
{
   LONGLONG start, end;
   HRESULT hr;
   if (!record_mf_audio_owner(e) || !e->input || e->error || e->draining || e->cancelled)
      return RECORD_MF_AUDIO_ERROR;
   if (!frames || frames > 1024 || index > ~(uint64_t)0 - frames
         || (e->have_input && index != e->next)
         || !record_mf_audio_clock(index, e->rate, &start)
         || !record_mf_audio_clock(index + frames, e->rate, &end))
      return RECORD_MF_AUDIO_ERROR;
   if (e->input_locked)
   {
      hr = IMFMediaBuffer_SetCurrentLength(e->input_buffer, (DWORD)(frames * e->channels * 2));
      if (SUCCEEDED(hr)) hr = IMFSample_SetSampleTime(e->input, start);
      if (SUCCEEDED(hr)) hr = IMFSample_SetSampleDuration(e->input, end - start);
      if (FAILED(hr)) return record_mf_audio_fail(e, hr);
      hr = IMFMediaBuffer_Unlock(e->input_buffer);
      e->input_data = NULL;
      if (FAILED(hr)) return record_mf_audio_fail(e, hr);
      e->input_locked = false;
      e->prepared_frames = frames;
      e->prepared_index = index;
   }
   if (e->prepared_frames != frames || e->prepared_index != index)
      return RECORD_MF_AUDIO_ERROR;
   if (e->packet_locked)
      return RECORD_MF_AUDIO_BUSY;
   hr = IMFTransform_ProcessInput(e->transform, 0, e->input, 0);
   if (hr == MF_E_NOTACCEPTING)
      return RECORD_MF_AUDIO_BUSY;
   if (FAILED(hr)) return record_mf_audio_fail(e, hr);
   e->next = index + frames;
   e->have_input = true;
   e->needs_output = true;
   record_mf_audio_drop_input(e);
   return RECORD_MF_AUDIO_PROGRESS;
}

int record_mf_audio_pump(record_mf_audio_t *e, record_mf_audio_packet_t *packet)
{
   MFT_OUTPUT_DATA_BUFFER output;
   DWORD status = 0, count = 0, size = 0;
   BYTE *data = NULL;
   LONGLONG timestamp = 0, duration = 0;
   HRESULT hr;
   if (packet) memset(packet, 0, sizeof(*packet));
   if (!record_mf_audio_owner(e) || !packet || e->error || e->cancelled)
      return RECORD_MF_AUDIO_ERROR;
   if (e->packet_locked)
   {
      *packet = e->packet;
      return RECORD_MF_AUDIO_PACKET;
   }
   if (e->ended)
      return RECORD_MF_AUDIO_END;
   if (e->output_buffer)
   {
      hr = IMFMediaBuffer_SetCurrentLength(e->output_buffer, 0);
      if (FAILED(hr)) return record_mf_audio_fail(e, hr);
   }
   output.dwStreamID = 0;
   output.pSample = e->output;
   output.dwStatus = 0;
   output.pEvents = NULL;
   hr = IMFTransform_ProcessOutput(e->transform, 0, 1, &output, &status);
   if (output.pEvents) IMFCollection_Release(output.pEvents);
   if (FAILED(hr))
   {
      if (output.pSample && output.pSample != e->output) IMFSample_Release(output.pSample);
      if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT)
      {
         e->needs_output = false;
         if (e->draining) e->ended = true;
         return e->ended ? RECORD_MF_AUDIO_END : RECORD_MF_AUDIO_BUSY;
      }
      return record_mf_audio_fail(e, hr);
   }
   e->packet_sample = output.pSample;
   if (!e->packet_sample)
      return record_mf_audio_fail(e, E_UNEXPECTED);
   hr = IMFSample_GetBufferCount(e->packet_sample, &count);
   if (SUCCEEDED(hr) && count == 1)
      hr = IMFSample_GetBufferByIndex(e->packet_sample, 0, &e->packet_buffer);
   else if (SUCCEEDED(hr))
      hr = IMFSample_ConvertToContiguousBuffer(e->packet_sample, &e->packet_buffer);
   if (SUCCEEDED(hr)) hr = IMFMediaBuffer_Lock(e->packet_buffer, &data, NULL, &size);
   if (FAILED(hr))
   {
      record_mf_audio_drop_packet(e);
      return record_mf_audio_fail(e, hr);
   }
   e->packet_locked = true;
   hr = IMFSample_GetSampleTime(e->packet_sample, &timestamp);
   if (SUCCEEDED(hr)) hr = IMFSample_GetSampleDuration(e->packet_sample, &duration);
   if (FAILED(hr) || !data || !size || duration <= 0)
   {
      record_mf_audio_drop_packet(e);
      return record_mf_audio_fail(e, FAILED(hr) ? hr : E_UNEXPECTED);
   }
   e->packet.timestamp = timestamp;
   e->packet.duration = duration;
   e->packet.data = data;
   e->packet.size = size;
   *packet = e->packet;
   return RECORD_MF_AUDIO_PACKET;
}

bool record_mf_audio_release(record_mf_audio_t *e)
{
   HRESULT hr;
   if (!record_mf_audio_owner(e) || !e->packet_locked)
      return false;
   hr = record_mf_audio_drop_packet(e);
   if (FAILED(hr))
   {
      record_mf_audio_fail(e, hr);
      return false;
   }
   return true;
}

bool record_mf_audio_config(record_mf_audio_t *e, const void **data, size_t *size)
{
   if (data) *data = NULL;
   if (size) *size = 0;
   if (!record_mf_audio_owner(e) || !data || !size || e->error || e->cancelled)
      return false;
   *data = e->config;
   *size = e->config_size;
   return true;
}

int record_mf_audio_finish(record_mf_audio_t *e)
{
   HRESULT hr;
   if (!record_mf_audio_owner(e) || e->error || e->cancelled)
      return RECORD_MF_AUDIO_ERROR;
   if (e->ended) return RECORD_MF_AUDIO_END;
   if (e->draining) return RECORD_MF_AUDIO_PROGRESS;
   if (e->input || e->packet_locked) return RECORD_MF_AUDIO_BUSY;
   hr = IMFTransform_ProcessMessage(e->transform, MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
   if (SUCCEEDED(hr)) hr = IMFTransform_ProcessMessage(e->transform, MFT_MESSAGE_COMMAND_DRAIN, 0);
   if (FAILED(hr)) return record_mf_audio_fail(e, hr);
   e->draining = true;
   return RECORD_MF_AUDIO_PROGRESS;
}

bool record_mf_audio_cancel(record_mf_audio_t *e)
{
   HRESULT hr;
   if (!record_mf_audio_owner(e) || e->packet_locked)
      return false;
   if (e->cancelled) return e->cancel_ok;
   record_mf_audio_drop_input(e);
   hr = IMFTransform_ProcessMessage(e->transform, MFT_MESSAGE_COMMAND_FLUSH, 0);
   e->cancelled = true;
   e->cancel_ok = SUCCEEDED(hr);
   if (FAILED(hr)) record_mf_audio_fail(e, hr);
   return SUCCEEDED(hr);
}

bool record_mf_audio_free(record_mf_audio_t *e)
{
   if (!e) return true;
   if (!record_mf_audio_owner(e) || e->input || e->packet_locked
         || (!e->ended && !e->cancelled && !e->error))
      return false;
   record_mf_audio_destroy(e);
   return true;
}

uint32_t record_mf_audio_error(const record_mf_audio_t *e)
{
   return record_mf_audio_owner(e) ? (uint32_t)e->error : 0;
}
#else
record_mf_audio_t *record_mf_audio_new(unsigned rate, unsigned channels, unsigned bitrate)
{
   (void)rate;
   (void)channels;
   (void)bitrate;
   return NULL;
}
int record_mf_audio_input(record_mf_audio_t *e, int16_t **samples, size_t *frames)
{
   (void)e;
   if (samples) *samples = NULL;
   if (frames) *frames = 0;
   return RECORD_MF_AUDIO_ERROR;
}
int record_mf_audio_submit(record_mf_audio_t *e, size_t frames, uint64_t index)
{
   (void)e;
   (void)frames;
   (void)index;
   return RECORD_MF_AUDIO_ERROR;
}
int record_mf_audio_pump(record_mf_audio_t *e, record_mf_audio_packet_t *packet)
{
   (void)e;
   if (packet) memset(packet, 0, sizeof(*packet));
   return RECORD_MF_AUDIO_ERROR;
}
bool record_mf_audio_release(record_mf_audio_t *e)
{
   (void)e;
   return false;
}
bool record_mf_audio_config(record_mf_audio_t *e, const void **data, size_t *size)
{
   (void)e;
   if (data) *data = NULL;
   if (size) *size = 0;
   return false;
}
int record_mf_audio_finish(record_mf_audio_t *e)
{
   (void)e;
   return RECORD_MF_AUDIO_ERROR;
}
bool record_mf_audio_cancel(record_mf_audio_t *e)
{
   (void)e;
   return false;
}
bool record_mf_audio_free(record_mf_audio_t *e)
{
   return e == NULL;
}
uint32_t record_mf_audio_error(const record_mf_audio_t *e)
{
   (void)e;
   return 0;
}
#endif
