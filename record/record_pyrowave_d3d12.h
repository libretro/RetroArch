/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_PYROWAVE_D3D12_H
#define RECORD_PYROWAVE_D3D12_H
#include <d3d12.h>
#include <formats/rpyrowave_gpu.h>
#include "record_gpu_queue.h"
#include "record_pyrowave.h"
#include "record_pyrowave_session.h"

RETRO_BEGIN_DECLS
typedef struct record_pyrowave_d3d12 record_pyrowave_d3d12_t;
void record_pyrowave_d3d12_session_video(record_pyrowave_d3d12_t *codec, record_pyrowave_video_t *video);
typedef HRESULT (WINAPI *record_pyrowave_d3d12_serialize_t)(
      const D3D12_ROOT_SIGNATURE_DESC*, D3D_ROOT_SIGNATURE_VERSION, ID3DBlob**, ID3DBlob**);
typedef struct record_pyrowave_d3d12_config
{
   ID3D12Device *device;
   record_pyrowave_d3d12_serialize_t serialize;
   const void *shaders[3];
   size_t shader_bytes[3];
   rpyrowave_sequence_t sequence;
   D3D12_COMMAND_LIST_TYPE queue_type;
   DXGI_FORMAT source_format;
   uint32_t maximum_bytes;
   unsigned slots, bit_depth, texture_mode;
   unsigned texture_width, texture_height, x, y;
   bool flip_y, decode;
   bool scale_source;
} record_pyrowave_d3d12_config_t;

typedef struct record_pyrowave_d3d12_image
{
   record_gpu_frame_t frame;
   ID3D12Resource *resource;
   D3D12_RESOURCE_STATES state;
   unsigned texture_mode;
} record_pyrowave_d3d12_image_t;

/* Cold setup borrows the device until free. Bytecode/serializer are used
 * only during construction. Render methods have one queue owner; encoder
 * take/packet/release and decoder receive methods have one I/O owner.
 * Compressed decoder input uses a cached CPU pool, then an upload copy;
 * encoder packets are lent directly from cached readback memory. */
record_pyrowave_d3d12_t *record_pyrowave_d3d12_new(const record_pyrowave_d3d12_config_t *config);
/* The caller records outside render passes, executes each list exactly once
 * in call order, then signals completion to value. Fences are borrowed until
 * poll/discard; values must be nonzero, increasing and not UINT64_MAX.
 * The adapter never submits or waits. Source must already be visible in
 * NON_PIXEL_SHADER_RESOURCE, with the configured non-sRGB typed view/extent,
 * and retained until source_retired >= ticket. BUSY consumes nothing.
 * Descriptor heaps/root state are clobbered and must be rebound by callers. */
int record_pyrowave_d3d12_push(record_pyrowave_d3d12_t *codec,
      ID3D12GraphicsCommandList *commands, ID3D12Fence *completion, uint64_t value,
      ID3D12Resource *source, uint64_t timestamp, uint64_t duration, uint64_t *ticket);
uint64_t record_pyrowave_d3d12_source_retired(const record_pyrowave_d3d12_t *codec);
int record_pyrowave_d3d12_take(record_pyrowave_d3d12_t *codec, record_gpu_frame_t *frame);
int record_pyrowave_d3d12_packet(record_pyrowave_d3d12_t *codec, unsigned slot, record_pyrowave_packet_t *packet);
bool record_pyrowave_d3d12_release(record_pyrowave_d3d12_t *codec, unsigned slot);
int record_pyrowave_d3d12_receive_acquire(record_pyrowave_d3d12_t *codec, void **data, size_t *capacity);
int record_pyrowave_d3d12_receive_submit(record_pyrowave_d3d12_t *codec,
      size_t bytes, uint64_t timestamp, uint64_t duration);
bool record_pyrowave_d3d12_receive_cancel(record_pyrowave_d3d12_t *codec);
int record_pyrowave_d3d12_receive_drop(record_pyrowave_d3d12_t *codec, record_gpu_frame_t *frame);
int record_pyrowave_d3d12_receive_peek(record_pyrowave_d3d12_t *codec, record_gpu_frame_t *frame);
int record_pyrowave_d3d12_decode(record_pyrowave_d3d12_t *codec,
      ID3D12GraphicsCommandList *commands, ID3D12Fence *completion, uint64_t value);
int record_pyrowave_d3d12_image(record_pyrowave_d3d12_t *codec, record_pyrowave_d3d12_image_t *image);
bool record_pyrowave_d3d12_image_drop(record_pyrowave_d3d12_t *codec, unsigned slot);
/* Return an image after recording its FINAL display use. Its published state
 * must be restored before the supplied timeline label completes. The label
 * may already have completed when a held image is replaced; the fence must
 * remain alive and monotonic until poll retires the loan. Cross-queue
 * dependencies are the caller's responsibility; no CPU wait is hidden here. */
bool record_pyrowave_d3d12_present(record_pyrowave_d3d12_t *codec, unsigned slot,
      ID3D12Fence *completion, uint64_t value);
/* Only after resetting/freeing a NEVER-executed list, ensuring it cannot
 * execute. A submitted list must retire normally or through cold abandon. */
bool record_pyrowave_d3d12_discard(record_pyrowave_d3d12_t *codec, ID3D12Fence *completion, uint64_t value);
bool record_pyrowave_d3d12_poll(record_pyrowave_d3d12_t *codec);
/* Render owner: no outstanding completion fence; queued/borrowed input and
 * output loans still need their normal release before destruction. */
bool record_pyrowave_d3d12_idle(const record_pyrowave_d3d12_t *codec);
/* Cold descriptor setup only. Borrowed identity survives until codec free;
 * GPU sampling is allowed only while the matching image loan is held. */
ID3D12Resource *record_pyrowave_d3d12_texture(const record_pyrowave_d3d12_t *codec, unsigned slot);
bool record_pyrowave_d3d12_finish(record_pyrowave_d3d12_t *codec);
bool record_pyrowave_d3d12_free(record_pyrowave_d3d12_t *codec);
/* Both owners stopped; device loss or external quiescence established,
 * all associated command lists reset/freed. Call before destroying device. */
bool record_pyrowave_d3d12_abandon(record_pyrowave_d3d12_t *codec);
RETRO_END_DECLS
#endif
