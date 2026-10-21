/* SPDX-License-Identifier: MIT */
#ifndef __LIBRETRO_SDK_FORMAT_RAV1_HDR_H__
#define __LIBRETRO_SDK_FORMAT_RAV1_HDR_H__

#include <formats/rav1.h>
#include <formats/video_color.h>

/* AV1 metadata_obu: https://aomediacodec.github.io/av1-spec/ */
typedef struct rav1_hdr_metadata
{
   video_mastering_display_t mastering;
   unsigned max_cll, max_fall, present; /* bit 0 CLL, bit 1 MDCV */
} rav1_hdr_metadata_t;

static INLINE uint32_t rav1_hdr_be32(const uint8_t *p)
{
   return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
      (uint32_t)p[2] << 8 | p[3];
}

/* 1 HDR update, 0 unknown metadata, -1 malformed. On 0/-1 state is
 * unchanged. Metadata is copied; the caller may retire its OBU storage.
 * AV1 xy is 0.16, maximum luminance 24.8, minimum 18.14. Convert to the
 * existing frame ABI with nearest rounding. A maximum beyond that ABI's
 * uint32_t range is absent, rather than wrapped or falsely saturated. */
static INLINE int rav1_hdr_metadata_parse(rav1_hdr_metadata_t *out,
      const uint8_t *payload, size_t size)
{
   rav1_hdr_metadata_t next;
   uint64_t type = 0, value;
   size_t pos = 0, required, i;
   unsigned byte = 128;
   if (!out || !payload) return -1;
   for (i = 0; i < 8 && pos < size; i++)
   {
      byte = payload[pos++];
      type |= (uint64_t)(byte & 127) << (7 * i);
      if (!(byte & 128)) break;
   }
   if (byte & 128) return -1;
   if (type != 1 && type != 2) return 0;
   required = type == 1 ? 4 : 24;
   if (required >= size - pos || payload[pos + required] != 128)
      return -1;
   for (i = pos + required + 1; i < size; i++)
      if (payload[i]) return -1;
   next = *out;
   if (type == 1)
   {
      next.max_cll = (unsigned)payload[pos] << 8 | payload[pos + 1];
      next.max_fall = (unsigned)payload[pos + 2] << 8 | payload[pos + 3];
      next.present |= 1;
   }
   else
   {
      memset(&next.mastering, 0, sizeof(next.mastering));
      next.mastering.present = VIDEO_MASTERING_DISPLAY_COMPLETE;
      for (i = 0; i < 8; i++)
      {
         unsigned xy = (unsigned)payload[pos + i * 2] << 8 |
            payload[pos + i * 2 + 1];
         next.mastering.chromaticity[i] = (uint16_t)
            (((uint32_t)xy * 50000u + 32768u) / 65536u);
      }
      value = ((uint64_t)rav1_hdr_be32(payload + pos + 16) * 10000u + 128u) / 256u;
      if (value <= UINT32_MAX)
         next.mastering.luminance_max = (uint32_t)value;
      else
         next.mastering.present &= ~0x100u;
      next.mastering.luminance_min = (uint32_t)
         (((uint64_t)rav1_hdr_be32(payload + pos + 20) * 10000u + 8192u) / 16384u);
      next.present |= 2;
   }
   *out = next;
   return 1;
}

/* CLL/MDCV are not layer-specific and may not carry an OBU extension. */
static INLINE int rav1_hdr_metadata_obu(rav1_hdr_metadata_t *out,
      const rav1_obu_t *obu)
{
   rav1_hdr_metadata_t next;
   int ret;
   if (!out || !obu || obu->type != 5 || !obu->data) return -1;
   next = *out;
   ret = rav1_hdr_metadata_parse(&next, obu->payload, obu->payload_size);
   if (ret > 0)
   {
      if (obu->data[0] & 4) return -1;
      *out = next;
   }
   return ret;
}

/* Snapshot source HDR metadata without changing colour code points. */
static INLINE void rav1_hdr_metadata_apply(video_color_metadata_t *color,
      const rav1_hdr_metadata_t *metadata)
{
   if (metadata->present & 1)
   {
      color->max_cll = metadata->max_cll;
      color->max_fall = metadata->max_fall;
   }
   if (metadata->present & 2) color->mastering = metadata->mastering;
}

/* The AV1 ISO binding requires matching explicit container/OBU values,
 * allowing precision differences. Tolerances include one AV1 quantum and
 * normalization rounding: xy 1, maximum 40, minimum 1 in frame ABI units.
 * Conflicts leave the destination unchanged. Zero light levels are absent
 * in the container ABI; a coded CLL record still explicitly replaces them. */
static INLINE bool rav1_hdr_metadata_merge(video_color_metadata_t *color,
      const rav1_hdr_metadata_t *metadata)
{
   unsigned i, shared;
   uint32_t a, b;
   if (metadata->present & 1)
   {
      if ((color->max_cll && color->max_cll != metadata->max_cll) ||
          (color->max_fall && color->max_fall != metadata->max_fall))
         return false;
   }
   if (metadata->present & 2)
   {
      shared = color->mastering.present & metadata->mastering.present;
      for (i = 0; i < 8; i++)
         if (shared & (1u << i))
         {
            a = color->mastering.chromaticity[i];
            b = metadata->mastering.chromaticity[i];
            if ((a > b ? a - b : b - a) > 1) return false;
         }
      a = color->mastering.luminance_max;
      b = metadata->mastering.luminance_max;
      if ((shared & 0x100u) && (a > b ? a - b : b - a) > 40) return false;
      a = color->mastering.luminance_min;
      b = metadata->mastering.luminance_min;
      if ((shared & 0x200u) && (a > b ? a - b : b - a) > 1) return false;
   }
   rav1_hdr_metadata_apply(color, metadata);
   return true;
}

/* Configuration OBUs apply to every associated sample. Atomic on error. */
static INLINE bool rav1_hdr_metadata_config(rav1_hdr_metadata_t *out,
      const rav1_config_t *config)
{
   rav1_hdr_metadata_t next;
   rav1_obu_t obu;
   size_t offset = 0;
   int ret;
   if (!out || !config) return false;
   next = *out;
   while ((ret = rav1_obu_next(&obu, config->obus, config->obus_size,
                              &offset, false)) == 1)
      if (obu.type == 5 && rav1_hdr_metadata_obu(&next, &obu) < 0)
         return false;
   if (ret < 0) return false;
   *out = next;
   return true;
}
#endif
