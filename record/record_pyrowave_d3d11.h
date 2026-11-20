/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_PYROWAVE_D3D11_H
#define RECORD_PYROWAVE_D3D11_H
#include <d3d11.h>
#include <formats/rpyrowave_gpu.h>
#include "record_gpu_queue.h"
#include "record_pyrowave_session.h"
RETRO_BEGIN_DECLS
typedef struct record_pyrowave_d3d11 record_pyrowave_d3d11_t;
typedef struct record_pyrowave_d3d11_config
{
   ID3D11Device *device;
   ID3D11DeviceContext *context;
   const void *shaders[3];
   size_t shader_bytes[3];
   rpyrowave_sequence_t sequence;
   uint32_t maximum_bytes;
   unsigned slots, bit_depth, texture_mode;
   unsigned texture_width, texture_height, x, y;
   bool flip_y, decode, scale_source;
} record_pyrowave_d3d11_config_t;
typedef struct record_pyrowave_d3d11_image
{
   record_gpu_frame_t frame;
   ID3D11ShaderResourceView *view;
   unsigned texture_mode;
} record_pyrowave_d3d11_image_t;
/* All GPU methods, including poll/free, belong to the immediate-context
 * owner. Only session packet/input methods belong to I/O. No API waits.
 * push borrows a non-sRGB view until source_retired reaches its ticket.
 * The caller unbinds graphics targets before push and submits through its
 * normal Present/Flush. Unbind display SRVs before decode can reuse images.
 * Compute state is clobbered and unbound afterwards. */
record_pyrowave_d3d11_t *record_pyrowave_d3d11_new(const record_pyrowave_d3d11_config_t *config);
int record_pyrowave_d3d11_push(record_pyrowave_d3d11_t *codec,
      ID3D11ShaderResourceView *source, uint64_t pts, uint64_t duration, uint64_t *ticket);
uint64_t record_pyrowave_d3d11_source_retired(const record_pyrowave_d3d11_t *codec);
int record_pyrowave_d3d11_take(record_pyrowave_d3d11_t*, record_gpu_frame_t*);
int record_pyrowave_d3d11_packet(record_pyrowave_d3d11_t*, unsigned, record_pyrowave_packet_t*);
bool record_pyrowave_d3d11_release(record_pyrowave_d3d11_t*, unsigned);
int record_pyrowave_d3d11_receive_acquire(record_pyrowave_d3d11_t*, void**, size_t*);
int record_pyrowave_d3d11_receive_submit(record_pyrowave_d3d11_t*, size_t, uint64_t, uint64_t);
bool record_pyrowave_d3d11_receive_cancel(record_pyrowave_d3d11_t*);
int record_pyrowave_d3d11_receive_peek(record_pyrowave_d3d11_t*, record_gpu_frame_t*);
int record_pyrowave_d3d11_receive_drop(record_pyrowave_d3d11_t*, record_gpu_frame_t*);
int record_pyrowave_d3d11_decode(record_pyrowave_d3d11_t*);
int record_pyrowave_d3d11_image(record_pyrowave_d3d11_t*, record_pyrowave_d3d11_image_t*);
bool record_pyrowave_d3d11_image_drop(record_pyrowave_d3d11_t*, unsigned);
/* Render owner, after the image's final draw. A query protects that use. */
bool record_pyrowave_d3d11_present(record_pyrowave_d3d11_t*, unsigned);
bool record_pyrowave_d3d11_poll(record_pyrowave_d3d11_t*);
bool record_pyrowave_d3d11_idle(const record_pyrowave_d3d11_t*);
bool record_pyrowave_d3d11_finish(record_pyrowave_d3d11_t*);
bool record_pyrowave_d3d11_free(record_pyrowave_d3d11_t*);
/* Cold, both owners stopped, only after device removal or idle proof. */
bool record_pyrowave_d3d11_abandon(record_pyrowave_d3d11_t*);
bool record_pyrowave_d3d11_quiesce(record_pyrowave_d3d11_t*);
void record_pyrowave_d3d11_session_video(record_pyrowave_d3d11_t*, record_pyrowave_video_t*);
RETRO_END_DECLS
#endif
