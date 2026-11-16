/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VULKAN_ENCODE_HEVC_H
#define RARCH_VULKAN_ENCODE_HEVC_H
#include "vulkan_encode_pool.h"
#include "vulkan_encode_packet.h"
#include "vulkan_encode_hevc_submit.h"
RETRO_BEGIN_DECLS
typedef struct vulkan_encode_hevc vulkan_encode_hevc_t;
typedef struct vulkan_encode_hevc_ops
{
   vulkan_encode_hevc_session_ops_t session;
   vulkan_encode_pool_ops_t pool;
   vulkan_encode_hevc_submit_ops_t submit;
} vulkan_encode_hevc_ops_t;
typedef struct vulkan_encode_hevc_open
{
   vulkan_encode_hevc_session_config_t session;
   VkQueue queue;
   size_t packet_size;
   unsigned slots, producer_family, gop, qp;
   bool storage_image_extended_formats;
} vulkan_encode_hevc_open_t;
/* One worker owns encoder and queue. Device/dispatch remain borrowed until
 * free. Producer borrows fixed pool images and timelines through retirement.
 * No B frames, frame-path allocations or waits. */
vulkan_encode_hevc_t *vulkan_encode_hevc_open(
      const vulkan_encode_hevc_open_t *config,
      const vulkan_encode_hevc_ops_t *ops);
const vulkan_encode_pool_t *vulkan_encode_hevc_pool(
      const vulkan_encode_hevc_t *encoder);
/* Startup, before the first accepted input. */
bool vulkan_encode_hevc_headers(vulkan_encode_hevc_t *encoder,
      void *data, size_t capacity, size_t *size);
/* Input is GENERAL after producer's strictly increasing ready value. A slot
 * remains immutable until packet release. Done signals the same value after
 * encoder use; producer reads/writes must also retire before pool destruction.
 * BUSY/invalid input consume nothing. Native errors stop admission and may
 * retain uncertain work. */
int vulkan_encode_hevc_submit(vulkan_encode_hevc_t *encoder, unsigned slot,
      uint64_t timestamp, uint64_t duration, uint64_t ready_value, bool keyframe);
/* One oldest-frame check, or repeated borrow until release. */
int vulkan_encode_hevc_poll(vulkan_encode_hevc_t *encoder,
      vulkan_encode_packet_t *packet);
bool vulkan_encode_hevc_release(vulkan_encode_hevc_t *encoder);
int vulkan_encode_hevc_finish(vulkan_encode_hevc_t *encoder);
/* Error/cancel drain: one bounded check per active transaction, discards
 * unborrowed output, refuses a packet still borrowed by the caller. Stops
 * admission. A successful drain says nothing about producer GPU completion. */
bool vulkan_encode_hevc_drain(vulkan_encode_hevc_t *encoder);
/* Producer stopped, its GPU commands retired, packet readers quiesced.
 * Refuses outstanding encoder work; never waits or destroys the device. */
bool vulkan_encode_hevc_free(vulkan_encode_hevc_t *encoder);
RETRO_END_DECLS
#endif
