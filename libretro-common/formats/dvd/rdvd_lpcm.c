/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <string.h>
#include <formats/rdvd_lpcm.h>

bool rdvd_lpcm_init(rdvd_lpcm_t *audio, unsigned index)
{
   if (!audio) return false;
   memset(audio, 0, sizeof(*audio));
   if (index > 7) return false;
   audio->index = index;
   return true;
}

void rdvd_lpcm_reset(rdvd_lpcm_t *audio)
{
   unsigned index;
   if (!audio) return;
   index = audio->index;
   memset(audio, 0, sizeof(*audio));
   audio->index = index;
}

bool rdvd_lpcm_feed(rdvd_lpcm_t *audio, const rdvd_packet_t *packet)
{
   rlpcm_format_t format;
   size_t bytes, offset = 0;
   unsigned frames;
   if (!audio || !packet || audio->borrowing || audio->index > 7
         || packet->kind != RDVD_PACKET_LPCM || packet->index != audio->index
         || (!packet->data && packet->size)
         || (packet->sample_rate != 48000 && packet->sample_rate != 96000))
      return false;
   memset(&format, 0, sizeof(format));
   format.bits = packet->sample_bits;
   format.channels = packet->channels;
   format.sample_rate = packet->sample_rate;
   format.big_endian = true;
   if (rlpcm_parse_format(RLPCM_KIND_RAW, NULL, 0, &format) != RLPCM_OK)
      return false;
   format.kind = RLPCM_KIND_DVD;
   if (!rlpcm_dvd_block_shape(&format, &bytes, &frames)) return false;
   if (audio->configured && (audio->format.bits != format.bits
         || audio->format.channels != format.channels
         || audio->format.sample_rate != format.sample_rate))
      return false;
   if (!audio->synchronized)
   {
      if (!packet->first_access_unit) return true;
      if (packet->first_access_unit < 4) return false;
      offset = packet->first_access_unit - 4;
      if (offset > packet->size) return false;
   }
   audio->format = format;
   audio->block_bytes = bytes;
   audio->block_frames = frames;
   audio->configured = audio->synchronized = true;
   audio->input = packet->data ? packet->data + offset : NULL;
   audio->remaining = packet->size - offset;
   audio->borrowing = true;
   return true;
}

enum rdvd_lpcm_result rdvd_lpcm_next(rdvd_lpcm_t *audio,
      size_t max_frames, rdvd_lpcm_block_t *out)
{
   size_t blocks, take;
   if (out) memset(out, 0, sizeof(*out));
   if (!audio || !out) return RDVD_LPCM_ERROR;
   if (!audio->borrowing) return RDVD_LPCM_MORE;
   if (max_frames < audio->block_frames) return RDVD_LPCM_ERROR;
   if (!audio->remaining)
   {
      audio->input = NULL;
      audio->borrowing = false;
      return RDVD_LPCM_MORE;
   }
   if (audio->used)
   {
      take = audio->block_bytes - audio->used;
      if (take > audio->remaining) take = audio->remaining;
      memcpy(audio->buffer + audio->used, audio->input, take);
      audio->used += take;
      audio->input += take;
      audio->remaining -= take;
      if (audio->used == audio->block_bytes)
      {
         out->data = audio->buffer;
         out->size = audio->block_bytes;
         out->frames = audio->block_frames;
         out->format = audio->format;
         audio->used = 0;
         return RDVD_LPCM_BLOCK;
      }
   }
   else if (audio->remaining >= audio->block_bytes)
   {
      blocks = audio->remaining / audio->block_bytes;
      if (blocks > max_frames / audio->block_frames)
         blocks = max_frames / audio->block_frames;
      out->data = audio->input;
      out->size = blocks * audio->block_bytes;
      out->frames = blocks * audio->block_frames;
      out->format = audio->format;
      audio->input += out->size;
      audio->remaining -= out->size;
      return RDVD_LPCM_BLOCK;
   }
   else
   {
      memcpy(audio->buffer, audio->input, audio->remaining);
      audio->used = audio->remaining;
      audio->remaining = 0;
   }
   audio->input = NULL;
   audio->borrowing = false;
   return RDVD_LPCM_MORE;
}
