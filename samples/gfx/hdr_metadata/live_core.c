#define retro_run fixture_run
#include "../../record/nvenc_frontend/core.c"
#undef retro_run

#define CHECK_HDR(x) do { if (!(x)) { \
   fprintf(stderr, "[hdr_metadata] FAIL line %d: %s\n", __LINE__, #x); abort(); } } while (0)

RETRO_API void retro_run(void)
{
   struct retro_hdr_metadata hdr;
   struct retro_display_metadata display;
   struct { unsigned version, size, canary; } short_buffer;
   unsigned i;
   const char *requested = getenv("HDR_QUERY_REQUESTED_MODE");
   unsigned allowed = requested ? (unsigned)strtoul(requested, NULL, 10) : 0;
   memset(&hdr, 0, sizeof(hdr));
   memset(&display, 0, sizeof(display));
   hdr.version = display.version = 1;
   hdr.size = sizeof(hdr);
   display.size = sizeof(display);
   CHECK_HDR(environment(RETRO_ENVIRONMENT_GET_HDR_METADATA, &hdr));
   CHECK_HDR(environment(RETRO_ENVIRONMENT_GET_DISPLAY_METADATA, &display));
   CHECK_HDR(!memcmp(&hdr, &display.hdr, sizeof(hdr)));
   CHECK_HDR(hdr.output_mode == 0 || hdr.output_mode == allowed);
   CHECK_HDR(hdr.passthrough == (hdr.output_mode != 0
            && (hdr.source_formats & RETRO_HDR_SOURCE_FORMAT_HDR10_2101010)));
   CHECK_HDR(hdr.color_primaries == (hdr.output_mode == 1 ? 9u
            : (hdr.output_mode == 2 ? 1u : 0u)));
   CHECK_HDR(hdr.transfer_function == (hdr.output_mode == 1 ? 16u
            : (hdr.output_mode == 2 ? 8u : 0u)));
   CHECK_HDR(hdr.paper_white_nits == 203.0f && hdr.max_nits == 600.0f);
   CHECK_HDR(hdr.expand_gamut == 2);
   for (i = 88; i <= 92; i++)
      CHECK_HDR(!environment(i | RETRO_ENVIRONMENT_EXPERIMENTAL, &hdr));
   CHECK_HDR(!environment(RETRO_ENVIRONMENT_GET_HDR_METADATA, NULL));
   short_buffer.version = 1;
   short_buffer.size = sizeof(short_buffer);
   short_buffer.canary = 0xdeadbeef;
   CHECK_HDR(!environment(RETRO_ENVIRONMENT_GET_HDR_METADATA, &short_buffer));
   CHECK_HDR(short_buffer.version == 1 && short_buffer.canary == 0xdeadbeef);
   hdr.version = 2;
   CHECK_HDR(!environment(RETRO_ENVIRONMENT_GET_HDR_METADATA, &hdr));
   CHECK_HDR(hdr.version == 2);
   if (!frames)
      fprintf(stderr, "[hdr_metadata] PASS: live structured queries; mode=%u native10=%u\n",
            display.hdr.output_mode, (display.hdr.source_formats
               & RETRO_HDR_SOURCE_FORMAT_HDR10_2101010) ? 1 : 0);
   fixture_run();
}
