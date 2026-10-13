/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#ifndef LIBRETRO_RDVD_SOURCE_H
#define LIBRETRO_RDVD_SOURCE_H
#include <formats/rdvd_ifo.h>
RETRO_BEGIN_DECLS

struct rdvd_source_backend;
typedef struct rdvd_source
{
   rdvd_vob_map_t map;
   struct rdvd_source_backend *pending_backend;
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
 * Discard all prior completions before reinitializing: tickets are scoped
 * to one initialization. The logical range must fit the entire source. */
bool rdvd_source_init(rdvd_source_t *source, const rdvd_vob_map_t *map,
      uint64_t offset, uint64_t bytes);
/* Reserves one file-local read into consumer-owned storage. No I/O occurs.
 * Caller keeps destination alive until completion, even after cancellation.
 * A pending read prevents another reservation. Failure clears request. */
bool rdvd_source_prepare(rdvd_source_t *source, void *destination,
      size_t capacity, rdvd_source_request_t *out);
/* Accepts the matching completion exactly once. Positive short reads
 * advance by actual bytes; zero/negative/oversized results fail the source.
 * Returns true when consumed, including errors. Stale tickets do nothing. */
bool rdvd_source_complete(rdvd_source_t *source, uint64_t ticket, int64_t bytes);
/* Stops new requests without waiting. Pending storage remains owned by
 * the outstanding reader until its matching completion is consumed. */
void rdvd_source_cancel(rdvd_source_t *source);
/* Starts at a checked title NAV target and reads through that cell's end.
 * Source sizes must belong to the supplied VTS. Fresh/drained state only;
 * angle/interleaved cells are rejected by the target resolver. */
bool rdvd_source_init_title(rdvd_source_t *source, const rdvd_ifo_t *ifo,
      const rdvd_vob_map_t *map, unsigned title, unsigned chapter,
      unsigned cell_number, uint32_t target);
enum rdvd_backend_status
{
   RDVD_BACKEND_PENDING = 0, RDVD_BACKEND_COMPLETE, RDVD_BACKEND_ERROR
};
typedef struct rdvd_backend_ops
{
   /* Submit must return immediately. False means no reader retains storage. */
   bool (*submit)(void *context, void *file, uint64_t offset,
         void *destination, size_t bytes, uint64_t ticket);
   /* Terminal results mean the reader has relinquished destination storage. */
   enum rdvd_backend_status (*poll)(void *context, uint64_t *ticket, int64_t *bytes);
} rdvd_backend_ops_t;
typedef struct rdvd_source_backend
{
   rdvd_backend_ops_t ops;
   rdvd_source_t *owner;
   void *context;
   void *const *files;
   rdvd_vob_map_t map;
} rdvd_source_backend_t;

/* Borrows immutable file-handle and size arrays in title segment order.
 * Handles/context stay alive through every pending completion. Operations
 * must not wait or perform blocking I/O on the caller's thread. Initialize
 * fresh/drained backend state only. */
bool rdvd_source_backend_init(rdvd_source_backend_t *backend,
      const rdvd_backend_ops_t *ops, void *context, void *const *files,
      const uint64_t *sizes, unsigned count);
/* Reserves and submits one read. Source/backend share the same immutable
 * size array. False submission drains the reservation as a source error.
 * Accepted backend/context remains alive until completion is polled.
 * Backend callbacks must not reenter source operations. */
bool rdvd_source_submit(rdvd_source_t *source, rdvd_source_backend_t *backend,
      void *destination, size_t capacity);
/* Polls once without waiting. True means a matching terminal completion
 * was consumed, including read errors. Pending/stale results return false.
 * A backend permits one outstanding source; cancellation still drains it. */
bool rdvd_source_poll(rdvd_source_t *source, rdvd_source_backend_t *backend);
RETRO_END_DECLS
#endif
