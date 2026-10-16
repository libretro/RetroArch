/* Production preview output negotiation and import descriptor. */
#include <stdio.h>
#include <string.h>
#include "gfx/gfx_surface_player.h"
static unsigned formats, shown, failures;
static bool hdr_source, requirements_ok;
static gfx_surface_t surface;
static video_frame_import_t captured;
#define CHECK(c) do { if (!(c)) { failures++; fprintf(stderr, "FAIL %d: %s\n", __LINE__, #c); } } while (0)
bool gfx_surface_query_requirements(unsigned width, gfx_surface_requirements_t *req)
{
   (void)width;
   memset(req, 0, sizeof(*req)); req->formats = formats;
   return requirements_ok;
}
bool image_transfer_anim_stream_is_hdr(const void *stream, enum image_type_enum type)
{
   (void)stream; (void)type; return hdr_source;
}
static gfx_surface_t *make_surface(void *user, unsigned dims, unsigned slots, uint32_t pixfmt)
{
   (void)user;
   CHECK(!slots && pixfmt == IMAGE_PIXFMT_NV12);
   surface.dims = dims; surface.imported = true;
   return &surface;
}
static void saw(void *user, gfx_surface_t *s)
{
   (void)user; CHECK(s == &surface); shown++;
}
enum gfx_surface_submit_result gfx_surface_submit_import(gfx_surface_t *s,
      const video_frame_import_t *frame)
{
   CHECK(s == &surface); captured = *frame; return GFX_SURFACE_SUBMIT_DONE;
}
#include "surface.inc"
int main(void)
{
   gfx_surface_player_t gsp;
   video_player_frame_t frame;
   video_hw_frame_t hw;
   unsigned i;
   memset(&gsp, 0, sizeof(gsp)); memset(&frame, 0, sizeof(frame));
   memset(&hw, 0, sizeof(hw));
   gsp.cb.surface = make_surface; gsp.cb.shown = saw;
   hw.pixfmt = IMAGE_PIXFMT_P010; hw.width = 32; hw.height = 16;
   hw.image = 17; hw.color.primaries = hw.color.matrix = 9; hw.color.transfer = 16;
   frame.hw = &hw;
   for (i = 0; i < 8; i++)
   {
      uint32_t expected;
      requirements_ok = (i & 1) != 0; hdr_source = (i & 2) != 0;
      formats = IMAGE_PIXFMT_8888 | ((i & 4) ? IMAGE_PIXFMT_FP16 : 0);
      expected = i == 7 ? IMAGE_PIXFMT_FP16 : IMAGE_PIXFMT_8888;
      CHECK(gfx_surface_player_start_hw(&gsp, 32, 16, 2) == 0);
      CHECK(gfx_surface_player_present_hw(&gsp, &surface, &frame) == VIDEO_PLAYER_PRESENT_DONE);
      CHECK(captured.output_pixfmt == expected && captured.pixfmt == IMAGE_PIXFMT_P010);
      CHECK(captured.image == 17 && captured.color.transfer == 16);
   }
   CHECK(shown == 8);
   puts(failures ? "Surface import output: FAIL" : "Surface import output: PASS");
   return failures ? 1 : 0;
}
