/*  RetroArch - A frontend for libretro.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __RARCH_DRM_HDR_H
#define __RARCH_DRM_HDR_H

#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* HDR10 on a KMS connector, as far as it is pure data: what the sink's
 * EDID says it takes, and the HDR_OUTPUT_METADATA blob that tells it
 * what is coming. No libdrm here, so it can be checked without a
 * display. */

typedef struct drm_hdr_sink
{
   /* Luminances the sink declares, 0 where it declares none */
   float max_nits;
   float max_fall_nits;
   float min_nits;
   bool  pq;
} drm_hdr_sink_t;

/* The kernel's struct hdr_metadata_infoframe and hdr_output_metadata,
 * mirrored because older libdrm headers lack them; the layout is ABI. */
typedef struct drm_hdr_infoframe
{
   uint8_t  eotf;
   uint8_t  metadata_type;
   struct { uint16_t x, y; } display_primaries[3];
   struct { uint16_t x, y; } white_point;
   uint16_t max_display_mastering_luminance;
   uint16_t min_display_mastering_luminance;
   uint16_t max_cll;
   uint16_t max_fall;
} drm_hdr_infoframe_t;

typedef struct drm_hdr_output_metadata
{
   uint32_t            metadata_type;
   drm_hdr_infoframe_t hdmi_metadata_type1;
} drm_hdr_output_metadata_t;

/**
 * Reads a sink's EDID (base block and extensions, as the connector's
 * EDID property holds it) for the CTA-861 HDR static metadata block.
 * Returns true, with 'sink' filled, when the sink takes SMPTE ST.2084
 * (PQ); false for anything else, including a malformed EDID.
 */
bool drm_hdr_parse_edid(const uint8_t *edid, size_t len,
      drm_hdr_sink_t *sink);

/**
 * The static metadata for HDR10 output: Rec.2020 primaries, D65, and
 * luminances from the user's peak, capped at what the sink declares.
 */
void drm_hdr_build_metadata(drm_hdr_output_metadata_t *out,
      const drm_hdr_sink_t *sink, float max_nits);

RETRO_END_DECLS

#endif
