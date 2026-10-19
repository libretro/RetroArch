/* Production preview output negotiation and import descriptor. */
#include <stdio.h>
#include <string.h>
#include "gfx/gfx_surface_player.h"
static unsigned formats, shown, failures;
static bool hdr_source, requirements_ok;
static gfx_surface_t surface;
static video_frame_import_t captured;
static enum gfx_surface_submit_result result = GFX_SURFACE_SUBMIT_DONE;
static unsigned submitted, abandoned, released;
static video_player_t *player;
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
   CHECK(s == &surface); captured = *frame; submitted++;
   return result;
}
enum gfx_surface_submit_result gfx_surface_submit(gfx_surface_t *s,
      unsigned slot, bool rgba)
{
   (void)s; (void)slot; (void)rgba;
   CHECK(false);
   return GFX_SURFACE_SUBMIT_FAILED;
}
bool gfx_surface_wants_rgba(void) { return false; }
void video_player_slot_released(video_player_t *vp, unsigned slot)
{
   CHECK(vp == player && slot == 3);
   released++;
}
static void abandon(void *owner, uint64_t done, uint64_t value)
{
   CHECK(owner == &abandoned && done == 88 && value == 7);
   abandoned++;
}
#include "surface.inc"
static void retry_ownership(void)
{
   static const enum gfx_surface_submit_result results[] = {
      GFX_SURFACE_SUBMIT_DONE, GFX_SURFACE_SUBMIT_QUEUED,
      GFX_SURFACE_SUBMIT_BUSY, GFX_SURFACE_SUBMIT_DROPPED,
      GFX_SURFACE_SUBMIT_FAILED
   };
   gfx_surface_player_t gsp;
   unsigned i, mode;
   for (mode = 0; mode < 3; mode++)
      for (i = 0; i < sizeof(results) / sizeof(results[0]); i++)
      {
         bool queued;
         bool retry = mode == 0;
         memset(&gsp, 0, sizeof(gsp));
         memset(&surface, 0, sizeof(surface));
         player = (video_player_t*)&gsp;
         gsp.vp = player;
         gsp.hw_slot = 3;
         gsp.hw_resent = mode == 1;
         surface.imported = surface.dropped = true;
         surface.import.owner = &abandoned;
         surface.import.abandon = abandon;
         surface.import.done = 88;
         surface.import.done_value = 7;
         result = results[i];
         submitted = abandoned = released = 0;
         queued = gfx_surface_player_release(mode == 2 ? NULL : &gsp,
               &surface, 0);
         CHECK(submitted == (retry ? 1u : 0u));
         CHECK(queued == (retry && result == GFX_SURFACE_SUBMIT_QUEUED));
         CHECK(abandoned == ((!retry || (result != GFX_SURFACE_SUBMIT_DONE
                     && result != GFX_SURFACE_SUBMIT_QUEUED)) ? 1u : 0u));
         CHECK(released == (mode != 2 && !queued ? 1u : 0u));
         if (retry && result == GFX_SURFACE_SUBMIT_DONE)
            CHECK(!surface.dropped);
         if (queued)
         {
            CHECK(gsp.hw_resent);
            /* The accepted retry is still pending; the first release
             * must not return its decoder slot or signal abandonment. */
            surface.dropped = false;
            CHECK(!gfx_surface_player_release(&gsp, &surface, 0));
            CHECK(!abandoned && released == 1 && submitted == 1);
         }
      }
}

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
   retry_ownership();
   puts(failures ? "Surface import output: FAIL" : "Surface import output: PASS");
   return failures ? 1 : 0;
}
