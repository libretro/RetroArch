/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <string.h>
#include <formats/rdvd_source.h>

bool rdvd_source_init(rdvd_source_t *source, const rdvd_vob_map_t *map,
      uint64_t offset, uint64_t bytes)
{
   rdvd_vob_map_t validated;
   if (!source) return false;
   memset(source, 0, sizeof(*source));
   source->failed = true;
   if (!map || !rdvd_vob_map_read(map->sizes, map->segments, &validated)
         || offset > validated.total_bytes || bytes > validated.total_bytes - offset)
      return false;
   source->map = validated; source->offset = offset; source->remaining = bytes;
   source->failed = false;
   return true;
}

bool rdvd_source_prepare(rdvd_source_t *source, void *destination,
      size_t capacity, rdvd_source_request_t *out)
{
   rdvd_vob_span_t span;
   uint64_t bytes;
   if (!out) return false;
   memset(out, 0, sizeof(*out));
   if (!source || !destination || !capacity || source->pending
         || source->failed || source->cancelled || !source->remaining) return false;
   bytes = source->remaining < capacity ? source->remaining : capacity;
   if (bytes > INT64_MAX) bytes = INT64_MAX;
   if (source->ticket == UINT64_MAX
         || !rdvd_vob_span_read(&source->map, source->offset, bytes, &span))
   {
      source->failed = true;
      return false;
   }
   source->ticket++;
   source->pending_bytes = (size_t)span.bytes; source->pending = true;
   out->destination = destination; out->file_offset = span.file_offset;
   out->ticket = source->ticket; out->bytes = source->pending_bytes;
   out->segment = span.segment;
   return true;
}


bool rdvd_source_complete(rdvd_source_t *source, uint64_t ticket, int64_t bytes)
{
   size_t requested;
   if (!source || !source->pending || ticket != source->ticket) return false;
   requested = source->pending_bytes;
   if (source->pending_backend && source->pending_backend->owner == source)
      source->pending_backend->owner = NULL;
   source->pending = false; source->pending_bytes = 0;
   source->pending_backend = NULL;
   if (source->cancelled) return true;
   if (bytes <= 0 || (uint64_t)bytes > requested)
   {
      source->failed = true;
      return true;
   }
   source->offset += (uint64_t)bytes;
   source->remaining -= (uint64_t)bytes;
   source->completed += (uint64_t)bytes;
   return true;
}

void rdvd_source_cancel(rdvd_source_t *source)
{
   if (source) source->cancelled = true;
}


bool rdvd_source_init_title(rdvd_source_t *source, const rdvd_ifo_t *ifo,
      const rdvd_vob_map_t *map, unsigned title, unsigned chapter,
      unsigned cell_number, uint32_t target)
{
   rdvd_title_target_t resolved;
   if (!source) return false;
   memset(source, 0, sizeof(*source));
   source->failed = true;
   if (!rdvd_ifo_title_target(ifo, map, title, chapter, cell_number, target, &resolved))
      return false;
   return rdvd_source_init(source, map, resolved.remaining.offset, resolved.remaining.bytes);
}


bool rdvd_source_backend_init(rdvd_source_backend_t *backend,
      const rdvd_backend_ops_t *ops, void *context, void *const *files,
      const uint64_t *sizes, unsigned count)
{
   rdvd_vob_map_t map;
   unsigned i;
   if (!backend) return false;
   memset(backend, 0, sizeof(*backend));
   if (!ops || !ops->submit || !ops->poll || !files
         || !rdvd_vob_map_read(sizes, count, &map)) return false;
   for (i = 0; i < count; i++)
      if (!files[i]) return false;
   backend->ops = *ops; backend->context = context;
   backend->files = files; backend->map = map;
   return true;
}


bool rdvd_source_submit(rdvd_source_t *source, rdvd_source_backend_t *backend,
      void *destination, size_t capacity)
{
   rdvd_source_request_t request;
   unsigned i;
   if (!source || !backend || backend->owner || !backend->ops.submit || !backend->ops.poll
         || !backend->files || source->map.sizes != backend->map.sizes
         || source->map.segments != backend->map.segments) return false;
   for (i = 0; i < source->map.segments; i++)
      if (!backend->files[i]) return false;
   if (!rdvd_source_prepare(source, destination, capacity, &request)) return false;
   source->pending_backend = backend; backend->owner = source;
   if (!backend->ops.submit(backend->context, backend->files[request.segment - 1],
            request.file_offset, request.destination, request.bytes, request.ticket))
   {
      rdvd_source_complete(source, request.ticket, -1);
      return false;
   }
   return true;
}


bool rdvd_source_poll(rdvd_source_t *source, rdvd_source_backend_t *backend)
{
   uint64_t ticket = 0;
   int64_t bytes = 0;
   enum rdvd_backend_status status;
   if (!source || !backend || !source->pending || source->pending_backend != backend
         || backend->owner != source || !backend->ops.poll) return false;
   status = backend->ops.poll(backend->context, &ticket, &bytes);
   if (status == RDVD_BACKEND_PENDING) return false;
   if (status != RDVD_BACKEND_COMPLETE && status != RDVD_BACKEND_ERROR)
   {
      source->failed = true;
      return false;
   }
   return rdvd_source_complete(source, ticket, status == RDVD_BACKEND_ERROR ? -1 : bytes);
}
