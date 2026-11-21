/* Production preview output negotiation and import descriptor. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "configuration.h"
#include "gfx/gfx_thumbnail.h"
#include "gfx/gfx_anim_preview.h"
#include <string/stdstring.h>
#include "gfx/gfx_surface_player.h"
static unsigned formats, shown, failures;
static bool hdr_source, requirements_ok;
static bool strided_upload = true;
static gfx_surface_t surface;
static video_frame_import_t captured;
static enum gfx_surface_submit_result result = GFX_SURFACE_SUBMIT_DONE;
static unsigned submitted, abandoned, released, recovery_requested;
static video_player_t *player;
#define CHECK(c) do { if (!(c)) { failures++; fprintf(stderr, "FAIL %d: %s\n", __LINE__, #c); } } while (0)
bool gfx_surface_query_requirements(unsigned width, gfx_surface_requirements_t *req)
{
   (void)width;
   memset(req, 0, sizeof(*req)); req->formats = formats;
   req->planar_strided = strided_upload;
   return requirements_ok;
}
bool image_transfer_anim_stream_is_hdr(const void *stream, enum image_type_enum type)
{
   (void)stream; (void)type; return hdr_source;
}
static bool planar_source, slotted;
static gfx_surface_planes_t captured_planes;
static unsigned plane_submits, held_plane_submits;
#ifdef HAVE_THREADS
static bool wrapper_active;
bool video_driver_thread_wrapper_active(void) { return wrapper_active; }
#endif
enum gfx_surface_submit_result gfx_surface_submit_planes(gfx_surface_t *s,
      const gfx_surface_planes_t *planes, bool rgba)
{
   (void)rgba;
   CHECK(s == &surface);
   captured_planes = *planes;
   plane_submits++;
   return s->inflight ? GFX_SURFACE_SUBMIT_BUSY : GFX_SURFACE_SUBMIT_DONE;
}
static unsigned yuv_set, yuv_sets, submits, submit_slot;
bool image_transfer_anim_stream_can_planar(const void *stream, enum image_type_enum type)
{
   (void)stream; (void)type; return planar_source;
}
static gfx_surface_t *make_surface(void *user, unsigned dims, unsigned slots, uint32_t pixfmt)
{
   (void)user;
   if (slotted)
   {
      /* a software decode's surface, in the format asked for */
      memset(&surface, 0, sizeof(surface));
      CHECK(slots == 1 || slots == 2);
      surface.dims = dims; surface.num_slots = slots; surface.pixfmt = pixfmt;
      surface.pitch[0] = surface.pitch[1] = (size_t)VIDEO_SCALE_W(dims)
         * IMAGE_PIXFMT_BPP(pixfmt);
      return &surface;
   }
   CHECK(!slots && pixfmt == IMAGE_PIXFMT_NV12);
   surface.dims = dims; surface.imported = true;
   return &surface;
}
void gfx_surface_set_yuv(gfx_surface_t *s, unsigned flags)
{
   CHECK(s == &surface && GFX_SURFACE_IS_PLANAR(s));
   yuv_set = flags; yuv_sets++;
}
enum gfx_surface_submit_result gfx_surface_submit_pixels(gfx_surface_t *s,
      const void *pixels, bool rgba)
{
   (void)s; (void)pixels; (void)rgba;
   CHECK(false);
   return GFX_SURFACE_SUBMIT_FAILED;
}
static void saw(void *user, gfx_surface_t *s)
{
   (void)user; CHECK(s == &surface); shown++;
}
enum gfx_surface_submit_result gfx_surface_submit_pixels_held(gfx_surface_t *s,
      const void *pixels, bool rgba)
{
   return gfx_surface_submit_pixels(s, pixels, rgba);
}

enum gfx_surface_submit_result gfx_surface_submit_planes_held(gfx_surface_t *s,
      const gfx_surface_planes_t *p, bool rgba)
{
   held_plane_submits++;
   gfx_surface_submit_planes(s, p, rgba);
   return s->inflight ? GFX_SURFACE_SUBMIT_BUSY : GFX_SURFACE_SUBMIT_QUEUED;
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
   (void)rgba;
   CHECK(slotted && s == &surface);
   submits++; submit_slot = slot;
   return s->inflight ? GFX_SURFACE_SUBMIT_BUSY : GFX_SURFACE_SUBMIT_DONE;
}
bool gfx_surface_wants_rgba(void) { return false; }
void video_player_slot_released(video_player_t *vp, unsigned slot)
{
   CHECK(vp == player && slot == 3);
   released++;
}
bool video_player_request_software(video_player_t *vp)
{
   CHECK(vp == player);
   recovery_requested++;
   return true;
}
static void abandon(void *owner, uint64_t done, uint64_t value)
{
   CHECK(owner == &abandoned && done == 88 && value == 7);
   abandoned++;
}
/* Constructor dependencies are inert; capture the real sink it publishes. */
static settings_t policy_settings;
static bool policy_settings_present;
static unsigned policy_reads;
static video_player_sink_t policy_sink;
settings_t *config_get_ptr(void)
{
   policy_reads++;
   return policy_settings_present ? &policy_settings : NULL;
}
video_player_t *video_player_new(void *stream, enum image_type_enum type,
      int loop_count, const video_player_sink_t *sink, unsigned flags)
{
   CHECK(stream == &policy_settings && type == IMAGE_TYPE_MP4);
   CHECK(loop_count == 1 && flags == VIDEO_PLAYER_INLINE);
   policy_sink = *sink;
   return (video_player_t*)&policy_sink;
}
static uint32_t *gfx_surface_player_slot_begin(void *user, unsigned slot)
{
   (void)user; (void)slot;
   return NULL;
}
static void gfx_surface_player_slot_end(void *user, unsigned slot)
{
   (void)user; (void)slot;
}
static bool gfx_surface_player_feed(void *user)
{
   (void)user;
   return false;
}
static void gfx_surface_player_decoded(void *user, enum video_player_how how)
{
   (void)user; (void)how;
}
static unsigned policy_cpu_opens, policy_hw_opens;
static void gfx_thumbnail_anim_close(gfx_thumbnail_t *thumbnail)
{
   (void)thumbnail;
}
gfx_anim_preview_t *gfx_anim_preview_open(const char *path, int png_probe)
{
   CHECK(!strcmp(path, "policy.mp4") && png_probe == -1);
   policy_cpu_opens++;
   return NULL;
}
gfx_anim_preview_t *gfx_anim_preview_open_hw(const char *path, int png_probe)
{
   CHECK(!strcmp(path, "policy.mp4") && png_probe == -1);
   policy_hw_opens++;
   return NULL;
}
bool gfx_anim_preview_windowed(const gfx_anim_preview_t *preview)
{
   (void)preview;
   CHECK(false);
   return false;
}
void gfx_anim_preview_release(gfx_anim_preview_t *preview)
{
   (void)preview;
   CHECK(false);
}
static bool gfx_thumbnail_anim_install(gfx_thumbnail_t *thumbnail,
      void *stream, enum image_type_enum type, void *buf, size_t len,
      struct data_transfer *dt, int windowed, const char *path)
{
   (void)thumbnail; (void)stream; (void)type; (void)buf; (void)len;
   (void)dt; (void)windowed; (void)path;
   CHECK(false);
   return false;
}
bool image_transfer_anim_stream_stop_hw(void *stream, enum image_type_enum type)
{
   (void)stream; (void)type;
   CHECK(false);
   return false;
}
bool video_player_set_hw_fallback(video_player_t *vp,
      video_player_hw_restart_t restart, void *user)
{
   CHECK(vp == (video_player_t*)&policy_sink && restart != NULL);
   CHECK(user == policy_sink.user);
   return true;
}
#include "surface.inc"
static void policy_snapshot(void)
{
   gfx_surface_player_cbs_t cb;
   gfx_thumbnail_t thumbnail;
   unsigned mode;
   memset(&cb, 0, sizeof(cb));
   memset(&thumbnail, 0, sizeof(thumbnail));
   cb.surface = make_surface;
   cb.shown = saw;
   for (mode = 0; mode < 4; mode++)
   {
      gfx_surface_player_t *gsp;
      void *output = &surface;
      bool enabled = mode == 1 || mode == 3;
      unsigned flags = VIDEO_PLAYER_INLINE;
      policy_settings_present = mode != 3;
      policy_settings.uints.video_hw_decode = (mode == 1 || mode == 2)
         ? VIDEO_HW_DECODE_AUTO : VIDEO_HW_DECODE_OFF;
      if (mode != 2)
         flags |= GFX_SURFACE_PLAYER_GPU;
      policy_cpu_opens = policy_hw_opens = 0;
      gfx_thumbnail_anim_open_probed(&thumbnail, "policy.mp4", -1);
      CHECK(policy_hw_opens == (mode != 0 ? 1u : 0u));
      CHECK(policy_cpu_opens == (mode == 0 ? 1u : 0u));
      policy_reads = 0;
      gsp = gfx_surface_player_new(&policy_settings, IMAGE_TYPE_MP4, 1,
            &output, &cb, flags);
      CHECK(gsp != NULL && policy_reads == 1);
      CHECK((policy_sink.start_hw != NULL) == enabled);
      /* A changed setting affects the next player, not this sink. Its
       * native constructor must not query mutable frontend policy. */
      policy_settings.uints.video_hw_decode = mode == 1
         ? VIDEO_HW_DECODE_OFF : VIDEO_HW_DECODE_AUTO;
      if (policy_sink.start_hw)
         CHECK(policy_sink.start_hw(policy_sink.user, 32, 16, 2) == 0);
      CHECK((policy_sink.start_hw != NULL) == enabled && policy_reads == 1);
      free(gsp);
   }
}

static void retry_ownership(void)
{
   static const enum gfx_surface_submit_result results[] = {
      GFX_SURFACE_SUBMIT_DONE, GFX_SURFACE_SUBMIT_QUEUED,
      GFX_SURFACE_SUBMIT_BUSY, GFX_SURFACE_SUBMIT_DROPPED,
      GFX_SURFACE_SUBMIT_FAILED
   };
   gfx_surface_player_t gsp;
   unsigned i, mode, transient;
   for (transient = 0; transient < 2; transient++)
      for (mode = 0; mode < 4; mode++)
         for (i = 0; i < sizeof(results) / sizeof(results[0]); i++)
         {
            bool queued;
            bool refused = true;
            bool retry = mode == 0 || mode == 3;
            memset(&gsp, 0, sizeof(gsp));
            memset(&surface, 0, sizeof(surface));
            player = (video_player_t*)&gsp;
            gsp.vp = player;
            gsp.hw_slot = 3;
            gsp.hw_resent = mode == 1;
            gsp.hw_confirmed = mode == 3;
            surface.imported = surface.dropped = true;
#ifdef HAVE_THREADS
            surface.node.kind = VIDEO_THREAD_ASYNC_IMPORT;
            surface.node.dropped = transient != 0;
            refused = !transient;
#else
            (void)transient;
#endif
            if (retry && results[i] == GFX_SURFACE_SUBMIT_FAILED)
               refused = true;
            else if (retry && results[i] == GFX_SURFACE_SUBMIT_DROPPED)
               refused = false;
            surface.import.owner = &abandoned;
            surface.import.abandon = abandon;
            surface.import.done = 88;
            surface.import.done_value = 7;
            result = results[i];
            submitted = abandoned = released = recovery_requested = 0;
            queued = gfx_surface_player_release(mode == 2 ? NULL : &gsp,
                  &surface, 0);
            CHECK(submitted == (retry ? 1u : 0u));
            CHECK(queued == (retry && result == GFX_SURFACE_SUBMIT_QUEUED));
            CHECK(abandoned == ((!retry || (result != GFX_SURFACE_SUBMIT_DONE
                        && result != GFX_SURFACE_SUBMIT_QUEUED)) ? 1u : 0u));
            CHECK(released == (mode != 2 && !queued ? 1u : 0u));
            CHECK(recovery_requested == ((mode != 2 && !queued && surface.dropped
                     && refused && !gsp.hw_confirmed) ? 1u : 0u));
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

/* A software decode's surface format by what the driver samples and
 * what the source is: planes where the driver converts them and the
 * source can hand them out, half floats ahead of them for an HDR
 * source, 8888 otherwise; a planar frame's colour flags reach the
 * surface ahead of the submit that reads them, and not while an
 * earlier frame is still in flight. */
static void planar_start(void)
{
   gfx_surface_player_t gsp;
   video_player_frame_t frame;
   void *current = NULL;
   unsigned i;
   slotted = true;
   requirements_ok = true;
   for (i = 0; i < 8; i++)
   {
      int flags;
      bool driver_planar = (i & 1) != 0;
      uint32_t expected;
      planar_source = (i & 2) != 0;
      hdr_source    = (i & 4) != 0;
      formats       = IMAGE_PIXFMT_8888 | IMAGE_PIXFMT_FP16
         | (driver_planar ? (IMAGE_PIXFMT_I420 | IMAGE_PIXFMT_NV12) : 0);
      expected      = hdr_source ? IMAGE_PIXFMT_FP16
         : (driver_planar && planar_source) ? IMAGE_PIXFMT_I420
         : IMAGE_PIXFMT_8888;
      memset(&gsp, 0, sizeof(gsp));
      gsp.cb.surface = make_surface; gsp.cb.shown = saw;
      gsp.surface    = &current;
      flags = gfx_surface_player_start(&gsp, 32, 16, 2);
      CHECK(flags >= 0 && surface.pixfmt == expected);
      CHECK(!(flags & VIDEO_PLAYER_BORROW_PLANES));
      CHECK(((flags & VIDEO_PLAYER_I420) != 0)
            == (expected == IMAGE_PIXFMT_I420));
      CHECK(((flags & VIDEO_PLAYER_FP16) != 0)
            == (expected == IMAGE_PIXFMT_FP16));
      /* a planar slot is a packed I420 frame: rows of luma bytes */
      current = &surface;
      if (expected == IMAGE_PIXFMT_I420)
         CHECK(gfx_surface_player_slot_stride(&gsp, 1) == 32);
      current = NULL;
   }

   /* planar_source and driver_planar, SDR: the last start above made an
    * I420 surface only if i == 3 came last; make it again */
   planar_source = true; hdr_source = false;
   formats = IMAGE_PIXFMT_8888 | IMAGE_PIXFMT_I420 | IMAGE_PIXFMT_NV12;
   memset(&gsp, 0, sizeof(gsp));
   gsp.cb.surface = make_surface; gsp.cb.shown = saw;
   gsp.surface    = &current;
   CHECK(gfx_surface_player_start(&gsp, 32, 16, 2) & VIDEO_PLAYER_I420);
   current = &surface;
   memset(&frame, 0, sizeof(frame));
   frame.slot = 1; frame.yuv = 6; frame.how = VIDEO_PLAYER_DIRECT;
   yuv_sets = submits = 0;
   CHECK(gfx_surface_player_present(&gsp, &frame) == VIDEO_PLAYER_PRESENT_DONE);
   CHECK(yuv_sets == 1 && yuv_set == 6 && submits == 1 && submit_slot == 1);
   surface.inflight = 1;
   frame.slot = 0; frame.yuv = 4;
   CHECK(gfx_surface_player_present(&gsp, &frame) == VIDEO_PLAYER_PRESENT_BUSY);
   CHECK(yuv_sets == 1 && yuv_set == 6 && submits == 2);
   surface.inflight = 0;
   CHECK(gfx_surface_player_present(&gsp, &frame) == VIDEO_PLAYER_PRESENT_DONE);
   CHECK(yuv_sets == 2 && yuv_set == 4 && submit_slot == 0);
   /* A row-at-a-time backend keeps its efficient packed upload. */
   strided_upload = false;
   CHECK(gfx_surface_player_start(&gsp, 32, 16, 1) & VIDEO_PLAYER_BORROW_PLANES);
   CHECK(!(gfx_surface_player_start(&gsp, 32, 16, 1)
            & VIDEO_PLAYER_BORROW_STRIDED));
   strided_upload = true;
   /* Only inline, direct video negotiates a decoder-plane borrow. */
   CHECK(gfx_surface_player_start(&gsp, 32, 16, 1)
         & VIDEO_PLAYER_BORROW_PLANES);
   CHECK(gfx_surface_player_start(&gsp, 32, 16, 1)
         & VIDEO_PLAYER_BORROW_STRIDED);
   {
      struct texture_planar view;
      uint8_t y[32 * 16], u[16 * 8], v[16 * 8];
      memset(&view, 0, sizeof(view));
      view.planes[0] = y; view.planes[1] = u; view.planes[2] = v;
      view.strides[0] = 32; view.strides[1] = view.strides[2] = 16;
      view.chroma_step = 1;
      frame.planar = &view;
      plane_submits = 0;
      CHECK(gfx_surface_player_present(&gsp, &frame) == VIDEO_PLAYER_PRESENT_DONE);
      CHECK(plane_submits == 1 && captured_planes.planes[0] == y
            && captured_planes.planes[1] == u && captured_planes.planes[2] == v
            && captured_planes.strides[0] == 32 && captured_planes.strides[1] == 16
            && captured_planes.chroma_step == 1);
      surface.inflight = 1;
      CHECK(gfx_surface_player_present(&gsp, &frame) == VIDEO_PLAYER_PRESENT_BUSY);
      surface.inflight = 0;
      frame.planar = NULL;
   }
#ifdef HAVE_THREADS
   wrapper_active = true;
   CHECK(!(gfx_surface_player_start(&gsp, 32, 16, 1)
            & VIDEO_PLAYER_BORROW_PLANES));
   gsp.hold_canvas = true;
   {
      struct texture_planar view;
      uint8_t bytes[3] = {0};
      unsigned flags = gfx_surface_player_start(&gsp, 32, 16, 1);
      CHECK((flags & (VIDEO_PLAYER_HOLD_CANVAS | VIDEO_PLAYER_BORROW_PLANES
                  | VIDEO_PLAYER_BORROW_STRIDED))
            == (VIDEO_PLAYER_HOLD_CANVAS | VIDEO_PLAYER_BORROW_PLANES
                  | VIDEO_PLAYER_BORROW_STRIDED));
      CHECK(!(gfx_surface_player_start(&gsp, 32, 16, 2)
               & (VIDEO_PLAYER_HOLD_CANVAS | VIDEO_PLAYER_BORROW_PLANES)));
      gfx_surface_player_start(&gsp, 32, 16, 1);
      memset(&view, 0, sizeof(view));
      view.planes[0] = bytes; view.planes[1] = bytes + 1; view.planes[2] = bytes + 2;
      view.strides[0] = 64; view.strides[1] = view.strides[2] = 32; view.chroma_step = 1;
      frame.planar = &view; held_plane_submits = 0;
      CHECK(gfx_surface_player_present(&gsp, &frame) == VIDEO_PLAYER_PRESENT_QUEUED);
      CHECK(held_plane_submits == 1 && captured_planes.planes[0] == bytes
            && captured_planes.strides[0] == 64);
      surface.inflight = 1;
      CHECK(gfx_surface_player_present(&gsp, &frame) == VIDEO_PLAYER_PRESENT_BUSY);
      surface.inflight = 0; frame.planar = NULL;
   }
   wrapper_active = false;
#endif
   slotted = false;
   memset(&surface, 0, sizeof(surface));
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
   result = GFX_SURFACE_SUBMIT_DROPPED;
   gsp.threaded = false;
   CHECK(gfx_surface_player_present_hw(&gsp, &surface, &frame) == VIDEO_PLAYER_PRESENT_BUSY);
   CHECK(shown == 8);
   retry_ownership();
   policy_snapshot();
   planar_start();
   puts(failures ? "Surface import output: FAIL" : "Surface import output: PASS");
   return failures ? 1 : 0;
}
