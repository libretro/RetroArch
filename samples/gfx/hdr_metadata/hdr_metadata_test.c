#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../gfx/video_hdr_metadata.h"

#if defined(RETRO_ENVIRONMENT_GET_HDR_OUTPUT_MODE) \
   || defined(RETRO_ENVIRONMENT_GET_HDR_PAPER_WHITE_NITS) \
   || defined(RETRO_ENVIRONMENT_GET_HDR_MAX_NITS) \
   || defined(RETRO_ENVIRONMENT_GET_HDR_EXPAND_GAMUT) \
   || defined(RETRO_ENVIRONMENT_GET_SCREEN_10BPC_CAPABLE)
#error Retired HDR queries must not remain public
#endif

#define CHECK(x) do { if (!(x)) { \
   fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

static void test_query(unsigned cmd)
{
   union
   {
      struct retro_display_metadata display;
      struct retro_hdr_metadata hdr;
      unsigned char bytes[sizeof(struct retro_display_metadata) + 32];
   } buffer, saved;
   video_hdr_metadata_snapshot_t snapshot;
   struct retro_hdr_metadata *hdr;
   unsigned size = cmd == RETRO_ENVIRONMENT_GET_HDR_METADATA
      ? sizeof(struct retro_hdr_metadata) : sizeof(struct retro_display_metadata);
   unsigned mode, i;
   unsigned header[2];
   memset(&snapshot, 0, sizeof(snapshot));
   snapshot.paper_white_nits = 203.0f;
   snapshot.max_nits         = 600.0f;
   snapshot.expand_gamut     = 3;
   snapshot.source_formats   = RETRO_HDR_SOURCE_FORMAT_HDR10_2101010;
   snapshot.display_dims     = (2560u << 16) | 1440u;
   snapshot.display_mm       = (597u << 16) | 336u;
   CHECK(!video_hdr_metadata_query(cmd, NULL, &snapshot));
   for (i = 0; i < 4; i++)
   {
      memset(&buffer, 0x5a, sizeof(buffer));
      header[0] = i == 0 ? 0 : (i == 1 ? 2 : 1);
      header[1] = i == 2 ? 0 : (i == 3 ? size - 1 : size);
      memcpy(buffer.bytes, header, sizeof(header));
      saved = buffer;
      CHECK(!video_hdr_metadata_query(cmd, &buffer, &snapshot));
      CHECK(!memcmp(&buffer, &saved, sizeof(buffer)));
   }
   for (mode = 0; mode <= 3; mode++)
   {
      memset(&buffer, 0x5a, sizeof(buffer));
      header[0] = 1;
      header[1] = sizeof(buffer);
      memcpy(buffer.bytes, header, sizeof(header));
      snapshot.output_mode = mode;
      CHECK(video_hdr_metadata_query(cmd, &buffer, &snapshot));
      for (i = size; i < sizeof(buffer); i++)
         CHECK(buffer.bytes[i] == 0x5a);
      hdr = cmd == RETRO_ENVIRONMENT_GET_HDR_METADATA
         ? &buffer.hdr : &buffer.display.hdr;
      CHECK(hdr->version == 1 && hdr->size == sizeof(*hdr));
      CHECK(hdr->output_mode == (mode <= 2 ? mode : 0));
      CHECK(hdr->passthrough == (mode == 1 || mode == 2));
      CHECK(hdr->source_formats == RETRO_HDR_SOURCE_FORMAT_HDR10_2101010);
      CHECK(hdr->paper_white_nits == 203.0f && hdr->max_nits == 600.0f);
      CHECK(hdr->expand_gamut == 3);
      CHECK(hdr->color_primaries == (mode == 1 ? 9u : (mode == 2 ? 1u : 0u)));
      CHECK(hdr->transfer_function == (mode == 1 ? 16u : (mode == 2 ? 8u : 0u)));
      CHECK(hdr->min_nits == 0 && hdr->max_cll == 0 && hdr->max_fall == 0);
      CHECK(hdr->tonemap_operator == 0);
      for (i = 0; i < 8; i++)
         CHECK(hdr->reserved[i] == 0);
      if (cmd == RETRO_ENVIRONMENT_GET_DISPLAY_METADATA)
      {
         struct retro_display_color_caps caps;
         memset(&caps, 0, sizeof(caps));
         CHECK(buffer.display.version == 1 && buffer.display.size == size);
         CHECK(buffer.display.geo.native_width == 2560
               && buffer.display.geo.native_height == 1440);
         CHECK(buffer.display.geo.width_mm == 597
               && buffer.display.geo.height_mm == 336);
         CHECK(!buffer.display.geo.refresh_min_mHz
               && !buffer.display.geo.refresh_max_mHz);
         CHECK(!memcmp(&caps, &buffer.display.caps, sizeof(caps)));
      }
   }
   if (cmd == RETRO_ENVIRONMENT_GET_DISPLAY_METADATA)
   {
      struct retro_display_geometry geo;
      memset(&geo, 0, sizeof(geo));
      snapshot.display_dims = snapshot.display_mm = 0;
      header[0] = 1;
      header[1] = size;
      memcpy(buffer.bytes, header, sizeof(header));
      CHECK(video_hdr_metadata_query(cmd, &buffer, &snapshot));
      CHECK(!memcmp(&geo, &buffer.display.geo, sizeof(geo)));
   }
   /* Input precision/encoding is independent of output and passthrough. */
   for (i = 0; i < 4; i++)
   {
      snapshot.source_formats = i | (1u << 31);
      snapshot.output_mode = 2;
      header[0] = 1;
      header[1] = size;
      memcpy(buffer.bytes, header, sizeof(header));
      CHECK(video_hdr_metadata_query(cmd, &buffer, &snapshot));
      hdr = cmd == RETRO_ENVIRONMENT_GET_HDR_METADATA
         ? &buffer.hdr : &buffer.display.hdr;
      CHECK(hdr->source_formats == snapshot.source_formats);
      CHECK(hdr->passthrough == ((i
               & RETRO_HDR_SOURCE_FORMAT_HDR10_2101010) != 0));
   }
   CHECK(!video_hdr_metadata_query(cmd, &buffer, NULL));
   for (i = 88; i <= 92; i++)
      CHECK(!video_hdr_metadata_query(i | RETRO_ENVIRONMENT_EXPERIMENTAL,
               &buffer, &snapshot));
   CHECK(!video_hdr_metadata_query(RETRO_ENVIRONMENT_GET_VFS_AUTHORIZED_LOCATIONS,
            &buffer, &snapshot));
   CHECK(!video_hdr_metadata_query(RETRO_ENVIRONMENT_GET_AUDIO_SAMPLE_BATCH_MULTI,
            &buffer, &snapshot));
}

int main(void)
{
   CHECK(RETRO_ENVIRONMENT_GET_HDR_METADATA != RETRO_ENVIRONMENT_GET_DISPLAY_METADATA);
   test_query(RETRO_ENVIRONMENT_GET_HDR_METADATA);
   test_query(RETRO_ENVIRONMENT_GET_DISPLAY_METADATA);
   puts("PASS: bounded HDR/display queries, retired IDs, PQ/scRGB contracts, display geometry");
   return 0;
}
