/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#ifndef LIBRETRO_RDVD_SOURCE_H
#define LIBRETRO_RDVD_SOURCE_H
#include <formats/rdvd_ifo.h>
RETRO_BEGIN_DECLS

typedef struct rdvd_source
{
   rdvd_vob_map_t map;
   uint64_t offset, remaining, completed, ticket;
   size_t pending_bytes;
   bool pending, failed, cancelled;
} rdvd_source_t;
typedef struct rdvd_source_request
{
   void *destination;
   uint64_t file_offset, ticket;
   size_t bytes;
   unsigned segment;
} rdvd_source_request_t;

/* Single-owner request state; serialize calls through the owning task.
 * Initialize fresh/drained state only. Sizes remain immutable and alive.
 * The requested logical range must fit the entire title source. */
bool rdvd_source_init(rdvd_source_t *source, const rdvd_vob_map_t *map,
      uint64_t offset, uint64_t bytes);
/* Reserves one file-local read into consumer-owned storage. No I/O occurs.
 * Caller keeps destination alive until completion, even after cancellation.
 * A pending read prevents another reservation. Failure clears request. */
bool rdvd_source_prepare(rdvd_source_t *source, void *destination,
      size_t capacity, rdvd_source_request_t *out);
RETRO_END_DECLS
#endif
