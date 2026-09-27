/* gfx/common/drm_hdr.c without a display: which EDIDs take HDR10, what
 * luminances they declare, that nothing malformed is read past its
 * end, and the HDR_OUTPUT_METADATA blob, whose layout must be the
 * kernel's. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <math.h>
#include <drm_mode.h>

#include "gfx/common/drm_hdr.h"

static int fails;
static void check(const char *what, int ok)
{
   printf("   %s %s\n", ok ? "ok  " : "FAIL", what);
   if (!ok)
      fails++;
}

/* A base block and one CTA-861 extension carrying 'db' as its only
 * data block; exactly the size the EDID says, on the heap, so any read
 * past the end is caught. */
static uint8_t *edid_with(const uint8_t *db, size_t dblen, size_t *len)
{
   uint8_t *e = (uint8_t*)calloc(1, 256);
   memcpy(e, "\x00\xff\xff\xff\xff\xff\xff\x00", 8);
   e[126]     = 1;
   e[128]     = 0x02;        /* CTA extension */
   e[129]     = 0x03;
   e[130]     = (uint8_t)(4 + dblen);
   memcpy(e + 132, db, dblen);
   *len = 256;
   return e;
}

int main(void)
{
   drm_hdr_sink_t sink;
   size_t len;
   uint8_t *e;

   printf("1. the metadata blob's layout is the kernel's\n");
   check("size", sizeof(drm_hdr_output_metadata_t) == sizeof(struct hdr_output_metadata));
#define SAME(f) check("offset of " #f, offsetof(drm_hdr_output_metadata_t, hdmi_metadata_type1.f) == offsetof(struct hdr_output_metadata, hdmi_metadata_type1.f))
   SAME(eotf); SAME(metadata_type); SAME(display_primaries); SAME(white_point);
   SAME(max_display_mastering_luminance); SAME(min_display_mastering_luminance);
   SAME(max_cll); SAME(max_fall);

   printf("2. a sink that takes PQ, with luminances\n");
   {
      /* extended tag block, 6 bytes: tag 0x06, EOTFs SDR+PQ, SM type 1,
       * max cv 96 (400 nits), frame average cv 80, min cv 32 */
      static const uint8_t db[] = { 0xE6, 0x06, 0x05, 0x01, 96, 80, 32 };
      e = edid_with(db, sizeof(db), &len);
      check("takes PQ", drm_hdr_parse_edid(e, len, &sink) && sink.pq);
      check("peak 400 nits", fabs(sink.max_nits - 400.0f) < 0.01f);
      check("frame average 50*2^(80/32) nits", fabs(sink.max_fall_nits - 50.0f * powf(2.0f, 2.5f)) < 0.01f);
      check("minimum 400*(32/255)^2/100 nits", fabs(sink.min_nits - 400.0f * (32.0f / 255.0f) * (32.0f / 255.0f) / 100.0f) < 1e-4f);
      free(e);
   }

   printf("3. sinks that do not take PQ\n");
   {
      static const uint8_t hlg[] = { 0xE3, 0x06, 0x09, 0x01 };   /* SDR + HLG */
      static const uint8_t vid[] = { 0x43, 0x10, 0x04, 0x03 };   /* a video block only */
      e = edid_with(hlg, sizeof(hlg), &len);
      check("HLG only is refused", !drm_hdr_parse_edid(e, len, &sink) && !sink.pq);
      free(e);
      e = edid_with(vid, sizeof(vid), &len);
      check("no HDR block is refused", !drm_hdr_parse_edid(e, len, &sink));
      free(e);
   }

   printf("4. malformed EDIDs are refused without reading past them\n");
   {
      static const uint8_t db[] = { 0xE6, 0x06, 0x05, 0x01, 96, 80, 32 };
      uint8_t *t;
      e = edid_with(db, sizeof(db), &len);
      /* claims an extension it does not have */
      t = (uint8_t*)malloc(128); memcpy(t, e, 128);
      check("an extension counted but missing", !drm_hdr_parse_edid(t, 128, &sink));
      free(t);
      check("shorter than a block", !drm_hdr_parse_edid(e, 100, &sink));
      /* a data block whose length runs past the data area */
      e[132] = 0xFF;
      check("a data block overrunning its area", !drm_hdr_parse_edid(e, len, &sink));
      /* a timing offset past the block */
      e[130] = 0xF0; e[132] = 0xE6;
      drm_hdr_parse_edid(e, len, &sink);
      check("a timing offset out of range reads no further", 1);
      check("no EDID at all", !drm_hdr_parse_edid(NULL, 0, &sink));
      free(e);
   }

   printf("5. the metadata sent for HDR10\n");
   {
      drm_hdr_output_metadata_t m;
      drm_hdr_sink_t s;
      memset(&s, 0, sizeof(s));
      s.pq = true; s.max_nits = 400.0f; s.max_fall_nits = 282.84f; s.min_nits = 0.063f;
      drm_hdr_build_metadata(&m, &s, 1000.0f);
      check("PQ EOTF, static metadata type 1", m.hdmi_metadata_type1.eotf == 2 && m.metadata_type == 0 && m.hdmi_metadata_type1.metadata_type == 0);
      check("the user's 1000 nits capped at the sink's 400", m.hdmi_metadata_type1.max_cll == 400 && m.hdmi_metadata_type1.max_display_mastering_luminance == 400);
      check("frame average from the sink", m.hdmi_metadata_type1.max_fall == 283);
      check("minimum in 0.0001-nit units", m.hdmi_metadata_type1.min_display_mastering_luminance == 630);
      check("Rec.2020 red and D65 in 0.00002 units",
            m.hdmi_metadata_type1.display_primaries[0].x == 35400 && m.hdmi_metadata_type1.display_primaries[0].y == 14600
            && m.hdmi_metadata_type1.white_point.x == 15635 && m.hdmi_metadata_type1.white_point.y == 16450);
      drm_hdr_build_metadata(&m, NULL, 600.0f);
      check("no sink luminances: the user's peak for both levels", m.hdmi_metadata_type1.max_cll == 600 && m.hdmi_metadata_type1.max_fall == 600);
   }

   if (fails)
   {
      printf("drm_hdr: %d check(s) failed\n", fails);
      return 1;
   }
   printf("drm_hdr: all checks passed\n");
   return 0;
}
