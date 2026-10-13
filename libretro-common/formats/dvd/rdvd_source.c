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
   source->pending = false; source->pending_bytes = 0;
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
