/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <string.h>
#include <formats/rdvd_ac3.h>

bool rdvd_ac3_init(rdvd_ac3_t *audio, unsigned index,
      uint8_t *buffer, size_t capacity)
{
   if (!audio)
      return false;
   memset(audio, 0, sizeof(*audio));
   if (!buffer || capacity < RDVD_AC3_BUFFER_BYTES || index > 7)
      return false;
   audio->buffer = buffer;
   audio->index = index;
   return true;
}

void rdvd_ac3_reset(rdvd_ac3_t *audio)
{
   if (!audio)
      return;
   audio->input = NULL;
   audio->remaining = audio->used = 0;
   audio->synchronized = audio->borrowing = audio->failed = false;
}

bool rdvd_ac3_feed(rdvd_ac3_t *audio, const rdvd_packet_t *packet)
{
   size_t offset = 0;
   if (!audio || !audio->buffer || audio->failed || audio->borrowing
         || !packet || packet->kind != RDVD_PACKET_AC3
         || packet->index != audio->index || (!packet->data && packet->size))
      return false;
   if (!audio->synchronized)
   {
      if (!packet->frame_count)
         return true;
      if (!packet->first_access_unit)
         return false;
      offset = packet->first_access_unit - 1;
      if (offset > packet->size)
         return false;
      audio->synchronized = true;
   }
   audio->input = packet->data ? packet->data + offset : NULL;
   audio->remaining = packet->size - offset;
   audio->borrowing = true;
   return true;
}

static enum rdvd_ac3_result rdvd_ac3_fail(rdvd_ac3_t *audio)
{
   audio->failed = true;
   audio->borrowing = false;
   audio->input = NULL;
   audio->remaining = audio->used = 0;
   return RDVD_AC3_ERROR;
}

enum rdvd_ac3_result rdvd_ac3_next(rdvd_ac3_t *audio,
      rdvd_ac3_frame_t *frame)
{
   rac3_frame_info_t info;
   const uint8_t *data;
   size_t available, take;
   enum rac3_status status;
   if (frame)
      memset(frame, 0, sizeof(*frame));
   if (!audio || !audio->buffer || !frame)
      return RDVD_AC3_ERROR;
   if (audio->failed)
      return RDVD_AC3_ERROR;
   if (!audio->borrowing)
      return RDVD_AC3_MORE;
   if (!audio->remaining)
   {
      audio->borrowing = false;
      audio->input = NULL;
      return RDVD_AC3_MORE;
   }
   if (audio->used && audio->used < 8)
   {
      take = 8 - audio->used;
      if (take > audio->remaining)
         take = audio->remaining;
      memcpy(audio->buffer + audio->used, audio->input, take);
      audio->used += take;
      audio->input += take;
      audio->remaining -= take;
   }
   data = audio->used ? audio->buffer : audio->input;
   available = audio->used ? audio->used : audio->remaining;
   status = rac3_parse_frame_info(data, available, &info);
   if (status == RAC3_NEED_MORE)
   {
      if (!audio->used)
      {
         memcpy(audio->buffer, audio->input, audio->remaining);
         audio->used = audio->remaining;
         audio->remaining = 0;
      }
      audio->borrowing = false;
      audio->input = NULL;
      return RDVD_AC3_MORE;
   }
   if (status != RAC3_OK || info.kind != RAC3_KIND_AC3
         || info.frame_bytes > RDVD_AC3_BUFFER_BYTES || !info.frame_bytes)
      return rdvd_ac3_fail(audio);
   if (audio->used)
   {
      if (audio->used > info.frame_bytes)
         return rdvd_ac3_fail(audio);
      take = info.frame_bytes - audio->used;
      if (take > audio->remaining)
         take = audio->remaining;
      memcpy(audio->buffer + audio->used, audio->input, take);
      audio->used += take;
      audio->input += take;
      audio->remaining -= take;
      if (audio->used < info.frame_bytes)
      {
         audio->borrowing = false;
         audio->input = NULL;
         return RDVD_AC3_MORE;
      }
      frame->data = audio->buffer;
      audio->used = 0;
   }
   else if (audio->remaining < info.frame_bytes)
   {
      memcpy(audio->buffer, audio->input, audio->remaining);
      audio->used = audio->remaining;
      audio->remaining = 0;
      audio->input = NULL;
      audio->borrowing = false;
      return RDVD_AC3_MORE;
   }
   else
   {
      frame->data = audio->input;
      audio->input += info.frame_bytes;
      audio->remaining -= info.frame_bytes;
   }
   frame->size = info.frame_bytes;
   frame->info = info;
   return RDVD_AC3_FRAME;
}
