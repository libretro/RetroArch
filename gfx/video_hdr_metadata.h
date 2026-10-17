#ifndef VIDEO_HDR_METADATA_H
#define VIDEO_HDR_METADATA_H

#include <string.h>
#include <retro_inline.h>
#include <libretro.h>

#include "video_defines.h"

typedef struct video_hdr_metadata_snapshot
{
   float paper_white_nits;
   float max_nits;
   unsigned output_mode;
   unsigned expand_gamut;
   unsigned display_dims;  /* VIDEO_SCALE_PACK pixels, 0 when unknown */
   unsigned display_mm;    /* VIDEO_SCALE_PACK millimetres */
   uint32_t source_formats;
} video_hdr_metadata_snapshot_t;

static INLINE bool video_hdr_metadata_query(unsigned cmd, void *data,
      const video_hdr_metadata_snapshot_t *snapshot)
{
   struct retro_hdr_metadata hdr;
   unsigned header[2];
   unsigned size;
   if (!data || !snapshot)
      return false;
   if (cmd == RETRO_ENVIRONMENT_GET_HDR_METADATA)
      size = sizeof(struct retro_hdr_metadata);
   else if (cmd == RETRO_ENVIRONMENT_GET_DISPLAY_METADATA)
      size = sizeof(struct retro_display_metadata);
   else
      return false;
   /* Both queries start with the same caller-owned version/size header. */
   memcpy(header, data, sizeof(header));
   if (header[0] != 1 || header[1] < size)
      return false;

   memset(&hdr, 0, sizeof(hdr));
   hdr.version           = 1;
   hdr.size              = sizeof(hdr);
   hdr.output_mode       = snapshot->output_mode <= 2
      ? snapshot->output_mode : 0;
   hdr.source_formats    = snapshot->source_formats;
   hdr.paper_white_nits  = snapshot->paper_white_nits;
   hdr.max_nits          = snapshot->max_nits;
   hdr.expand_gamut      = snapshot->expand_gamut;
   if (hdr.output_mode)
   {
      hdr.color_primaries   = hdr.output_mode == 1 ? 9 : 1;
      hdr.transfer_function = hdr.output_mode == 1 ? 16 : 8;
      hdr.passthrough       = (snapshot->source_formats
            & RETRO_HDR_SOURCE_FORMAT_HDR10_2101010) != 0;
   }
   if (cmd == RETRO_ENVIRONMENT_GET_HDR_METADATA)
      memcpy(data, &hdr, sizeof(hdr));
   else
   {
      struct retro_display_metadata display;
      memset(&display, 0, sizeof(display));
      display.version = 1;
      display.size    = sizeof(display);
      display.geo.native_width  = VIDEO_SCALE_W(snapshot->display_dims);
      display.geo.native_height = VIDEO_SCALE_H(snapshot->display_dims);
      display.geo.width_mm      = VIDEO_SCALE_W(snapshot->display_mm);
      display.geo.height_mm     = VIDEO_SCALE_H(snapshot->display_mm);
      memcpy(&display.hdr, &hdr, sizeof(hdr));
      memcpy(data, &display, sizeof(display));
   }
   return true;
}

#endif
