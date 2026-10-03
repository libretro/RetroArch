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

#include <string.h>
#include <math.h>

#include "drm_hdr.h"

#define DRM_HDR_EDID_BLOCK       128
#define DRM_HDR_CTA_TAG          0x02
#define DRM_HDR_DB_EXTENDED      7
#define DRM_HDR_EXT_STATIC_META  0x06
#define DRM_HDR_EOTF_ST2084      (1 << 2)

/* CTA-861.3: code values for the desired content luminances */
static float drm_hdr_cv_to_nits(unsigned cv)
{
   return 50.0f * (float)pow(2.0, (double)cv / 32.0);
}

bool drm_hdr_parse_edid(const uint8_t *edid, size_t len,
      drm_hdr_sink_t *sink)
{
   size_t block, exts;

   memset(sink, 0, sizeof(*sink));
   if (!edid || len < DRM_HDR_EDID_BLOCK)
      return false;
   exts = edid[126];

   for (block = 1; block <= exts; block++)
   {
      size_t i, end;
      const uint8_t *b = edid + block * DRM_HDR_EDID_BLOCK;

      if ((block + 1) * DRM_HDR_EDID_BLOCK > len)
         break;
      if (b[0] != DRM_HDR_CTA_TAG)
         continue;
      /* Data blocks run from byte 4 to where the timings begin */
      end = b[2];
      if (end < 4 || end > DRM_HDR_EDID_BLOCK)
         end = DRM_HDR_EDID_BLOCK;

      for (i = 4; i < end; )
      {
         unsigned tag  = b[i] >> 5;
         size_t   blen = b[i] & 0x1f;
         const uint8_t *p = b + i + 1;

         if (i + 1 + blen > end)
            break;
         if (     tag == DRM_HDR_DB_EXTENDED && blen >= 3
               && p[0] == DRM_HDR_EXT_STATIC_META)
         {
            if (!(p[1] & DRM_HDR_EOTF_ST2084))
               return false;
            sink->pq = true;
            if (blen >= 4 && p[3])
               sink->max_nits      = drm_hdr_cv_to_nits(p[3]);
            if (blen >= 5 && p[4])
               sink->max_fall_nits = drm_hdr_cv_to_nits(p[4]);
            if (blen >= 6 && sink->max_nits > 0.0f)
               sink->min_nits      = sink->max_nits
                  * ((float)p[5] / 255.0f) * ((float)p[5] / 255.0f)
                  / 100.0f;
            return true;
         }
         i += 1 + blen;
      }
   }
   return false;
}

/* Chromaticities in units of 0.00002 */
static uint16_t drm_hdr_chroma(double v)
{
   return (uint16_t)(v / 0.00002 + 0.5);
}

static uint16_t drm_hdr_nits(float v)
{
   if (v <= 0.0f)
      return 0;
   if (v >= 65535.0f)
      return 65535;
   return (uint16_t)(v + 0.5f);
}

void drm_hdr_build_metadata(drm_hdr_output_metadata_t *out,
      const drm_hdr_sink_t *sink, float max_nits)
{
   drm_hdr_infoframe_t *f = &out->hdmi_metadata_type1;
   float peak             = max_nits > 0.0f ? max_nits : 1000.0f;
   float fall;

   /* No more than the sink says it can show */
   if (sink && sink->max_nits > 0.0f && peak > sink->max_nits)
      peak = sink->max_nits;
   /* A core's own HDR frames may average far above paper white, so
    * the frame average is the sink's, or else the peak */
   fall = (sink && sink->max_fall_nits > 0.0f && sink->max_fall_nits < peak)
      ? sink->max_fall_nits : peak;

   memset(out, 0, sizeof(*out));
   out->metadata_type = 0;  /* Static Metadata Type 1 */
   f->eotf            = 2;  /* SMPTE ST.2084 */
   f->metadata_type   = 0;
   /* Rec.2020 red, green, blue, as the compositors that set this send
    * them, and the D65 white point */
   f->display_primaries[0].x = drm_hdr_chroma(0.708);
   f->display_primaries[0].y = drm_hdr_chroma(0.292);
   f->display_primaries[1].x = drm_hdr_chroma(0.170);
   f->display_primaries[1].y = drm_hdr_chroma(0.797);
   f->display_primaries[2].x = drm_hdr_chroma(0.131);
   f->display_primaries[2].y = drm_hdr_chroma(0.046);
   f->white_point.x          = drm_hdr_chroma(0.3127);
   f->white_point.y          = drm_hdr_chroma(0.3290);
   f->max_display_mastering_luminance = drm_hdr_nits(peak);
   /* In units of 0.0001 nits */
   f->min_display_mastering_luminance = (uint16_t)(
         (sink && sink->min_nits > 0.0f && sink->min_nits < 6.5f)
         ? sink->min_nits * 10000.0f + 0.5f : 50.0f);
   f->max_cll  = drm_hdr_nits(peak);
   f->max_fall = drm_hdr_nits(fall);
}
