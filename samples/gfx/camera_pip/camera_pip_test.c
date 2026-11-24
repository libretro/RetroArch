/* The camera's picture-in-picture over running content.
 *
 * The main thread opens the camera preview into a live thumbnail
 * while content runs, the menu is closed and the settings ask for it,
 * and stops it otherwise; the widget's frame draws the thumbnail in
 * the corner and at the size asked for. The preview, the thumbnail and
 * the draw are stood in for, recording what the widget asks of them.
 *
 * The widget's source is included, for its state. */
#include <stdio.h>
#include <string.h>

#include "../../../gfx/widgets/gfx_widget_camera_pip.c"

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %d: %s\n", __LINE__, #c); fails++; } } while (0)

/* --- the frontend the widget reads --- */
static settings_t       test_settings;
static runloop_state_t  test_runloop;
static struct menu_state test_menu;

settings_t *config_get_ptr(void) { return &test_settings; }
runloop_state_t *runloop_state_get_ptr(void) { return &test_runloop; }
struct menu_state *menu_state_get_ptr(void) { return &test_menu; }

/* --- the video driver: whether it draws a widget pass at all --- */
static bool widget_pass = true;
bool video_driver_widgets_supported(void) { return widget_pass; }

/* --- the camera preview --- */
static camera_preview_sink_t sink;
static bool sink_open;
static unsigned opens, closes, polls;
static enum camera_preview_state poll_answer = CAMERA_PREVIEW_SHARED;

void camera_driver_preview_open(const camera_preview_sink_t *s)
{
   sink = *s;
   sink_open = true;
   opens++;
}

void camera_driver_preview_close(void)
{
   sink_open = false;
   closes++;
}

enum camera_preview_state camera_driver_preview_poll(void)
{
   polls++;
   return sink_open ? poll_answer : CAMERA_PREVIEW_OFF;
}

/* --- the live thumbnail --- */
static gfx_thumbnail_t *live;
static gfx_thumbnail_live_end_t live_end;
static void *live_user;
static unsigned planes_taken, pixels_taken, last_yuv;

void gfx_thumbnail_reset(gfx_thumbnail_t *t)
{
   if (t == live)
   {
      gfx_thumbnail_live_end_t end = live_end;
      live = NULL;
      if (end)
         end(live_user);
   }
   t->dims = 0;
   retro_atomic_store_release_int(&t->status, GFX_THUMBNAIL_STATUS_UNKNOWN);
}

void gfx_thumbnail_live_begin(gfx_thumbnail_t *t,
      gfx_thumbnail_live_end_t end, void *user)
{
   gfx_thumbnail_reset(t);
   if (live)
      gfx_thumbnail_reset(live);
   live      = t;
   live_end  = end;
   live_user = user;
   retro_atomic_store_release_int(&t->status, GFX_THUMBNAIL_STATUS_PENDING);
}

static void shown(gfx_thumbnail_t *t, unsigned dims)
{
   t->dims = dims;
   retro_atomic_store_release_int(&t->status, GFX_THUMBNAIL_STATUS_AVAILABLE);
}

bool gfx_thumbnail_live_planes(gfx_thumbnail_t *t,
      const uint8_t *const planes[3], const size_t strides[3],
      unsigned chroma_step, unsigned dims, unsigned yuv)
{
   (void)planes; (void)strides; (void)chroma_step;
   if (t != live)
      return false;
   planes_taken++;
   last_yuv = yuv;
   shown(t, dims);
   return true;
}

bool gfx_thumbnail_live_pixels(gfx_thumbnail_t *t, const uint32_t *pixels,
      unsigned dims, size_t pitch)
{
   (void)pixels; (void)pitch;
   if (t != live)
      return false;
   pixels_taken++;
   shown(t, dims);
   return true;
}

/* --- the draw --- */
static unsigned draws;
static float draw_x, draw_y;
static unsigned draw_dims;

void gfx_thumbnail_draw(void *userdata, unsigned video_dims,
      gfx_thumbnail_t *t, float x, float y, unsigned dims,
      enum gfx_thumbnail_alignment alignment, float alpha,
      float scale_factor, gfx_thumbnail_shadow_t *shadow)
{
   (void)userdata; (void)video_dims; (void)alignment; (void)alpha;
   (void)scale_factor; (void)shadow;
   CHECK(t == &p_w_camera_pip_st.thumbnail);
   draws++;
   draw_x    = x;
   draw_y    = y;
   draw_dims = dims;
}

static dispgfx_widget_t widgets_st;

static void frame_in(unsigned vw, unsigned vh, dispgfx_widget_t *widgets)
{
   video_frame_info_t info;
   memset(&info, 0, sizeof(info));
   info.dims = VIDEO_SCALE_PACK(vw, vh);
   draws     = 0;
   gfx_widget_camera_pip.frame(&info, widgets);
}

static void frame(unsigned vw, unsigned vh)
{
   frame_in(vw, vh, &widgets_st);
}

static void camera_frame(unsigned w, unsigned h)
{
   static const uint8_t plane[16] = { 0 };
   struct retro_camera_planar_frame f;
   memset(&f, 0, sizeof(f));
   f.planes[0] = f.planes[1] = f.planes[2] = plane;
   f.strides[0] = f.strides[1] = f.strides[2] = 4;
   f.chroma_step = 2;
   f.width = w; f.height = h;
   f.flags = RETRO_CAMERA_PLANAR_FULL_RANGE | RETRO_CAMERA_PLANAR_BT709;
   CHECK(sink.planar);
   sink.planar(sink.user, &f);
}

int main(void)
{
   uint32_t xrgb[4] = { 0 };
   memset(&test_settings, 0, sizeof(test_settings));
   test_settings.bools.camera_allow    = true;
   test_settings.uints.camera_pip_size = 25;
   test_settings.uints.camera_pip_corner = CAMERA_PIP_BOTTOM_RIGHT;
   test_runloop.flags = RUNLOOP_FLAG_CORE_RUNNING;
   widgets_st.simple_widget_padding = 10;

   /* Off: no camera */
   gfx_widget_camera_pip_update(true);
   CHECK(!opens && !polls && !gfx_widget_camera_pip.visible());
   frame(1280, 720);
   CHECK(!draws);

   /* On: the preview opened into the thumbnail, nothing drawn until a
    * frame reaches it */
   test_settings.bools.camera_pip = true;
   gfx_widget_camera_pip_update(true);
   CHECK(opens == 1 && sink_open && polls == 1 && live == &p_w_camera_pip_st.thumbnail);
   CHECK(sink.user == &p_w_camera_pip_st.thumbnail);
   CHECK(gfx_widget_camera_pip.visible());
   frame(1280, 720);
   CHECK(!draws);

   /* A 4:3 frame: a quarter of the height, bottom right inside the
    * padding; range and matrix carried */
   camera_frame(640, 480);
   CHECK(planes_taken == 1
         && last_yuv == (IMAGE_YUV_FLAG_FULL_RANGE | IMAGE_YUV_FLAG_BT709));
   gfx_widget_camera_pip_update(true);
   CHECK(opens == 1 && polls == 2);
   frame(1280, 720);
   CHECK(draws == 1 && draw_dims == VIDEO_SCALE_PACK(240, 180));
   CHECK(draw_x == 1280 - 240 - 10 && draw_y == 720 - 180 - 10);

   /* Each corner */
   test_settings.uints.camera_pip_corner = CAMERA_PIP_TOP_LEFT;
   gfx_widget_camera_pip_update(true);
   frame(1280, 720);
   CHECK(draws == 1 && draw_x == 10 && draw_y == 10);
   test_settings.uints.camera_pip_corner = CAMERA_PIP_TOP_RIGHT;
   gfx_widget_camera_pip_update(true);
   frame(1280, 720);
   CHECK(draws == 1 && draw_x == 1280 - 240 - 10 && draw_y == 10);
   test_settings.uints.camera_pip_corner = CAMERA_PIP_BOTTOM_LEFT;
   gfx_widget_camera_pip_update(true);
   frame(1280, 720);
   CHECK(draws == 1 && draw_x == 10 && draw_y == 720 - 180 - 10);

   /* A wide frame at half the height: no wider than half the screen */
   test_settings.uints.camera_pip_size = 50;
   pixels_taken = 0;
   sink.raw(sink.user, xrgb, 32, 4, 32 * 4);
   CHECK(pixels_taken == 1);
   gfx_widget_camera_pip_update(true);
   frame(1280, 720);
   CHECK(draws == 1 && draw_dims == VIDEO_SCALE_PACK(640, 80));
   test_settings.uints.camera_pip_size = 25;
   camera_frame(640, 480);

   /* The core's camera not started: kept open, nothing drawn */
   poll_answer = CAMERA_PREVIEW_CORE_IDLE;
   gfx_widget_camera_pip_update(true);
   CHECK(sink_open && opens == 1 && !gfx_widget_camera_pip.visible());
   frame(1280, 720);
   CHECK(!draws);
   poll_answer = CAMERA_PREVIEW_SHARED;
   gfx_widget_camera_pip_update(true);
   frame(1280, 720);
   CHECK(draws == 1);

   /* The menu open: the camera closed, nothing drawn; open again as it
    * closes */
   test_menu.flags = MENU_ST_FLAG_ALIVE;
   gfx_widget_camera_pip_update(true);
   CHECK(!sink_open && closes == 1 && !live && !gfx_widget_camera_pip.visible());
   frame(1280, 720);
   CHECK(!draws);
   test_menu.flags = 0;
   gfx_widget_camera_pip_update(true);
   CHECK(sink_open && opens == 2 && live == &p_w_camera_pip_st.thumbnail);

   /* Closed under it (the menu's own stop): opened again next frame */
   camera_driver_preview_close();
   gfx_widget_camera_pip_update(true);
   CHECK(!live && !gfx_widget_camera_pip.visible());
   gfx_widget_camera_pip_update(true);
   CHECK(sink_open && opens == 3);

   /* Another live thumbnail beginning ends the picture's camera */
   {
      gfx_thumbnail_t other;
      unsigned before = closes;
      memset(&other, 0, sizeof(other));
      gfx_thumbnail_live_begin(&other, NULL, NULL);
      CHECK(closes == before + 1 && !p_w_camera_pip_st.live);
      CHECK(!gfx_widget_camera_pip.visible());
      gfx_thumbnail_reset(&other);
      gfx_widget_camera_pip_update(true);
      CHECK(sink_open && p_w_camera_pip_st.live);
   }

   /* Camera not allowed, no content, widgets off: each stops it */
   test_settings.bools.camera_allow = false;
   gfx_widget_camera_pip_update(true);
   CHECK(!sink_open && !live);
   test_settings.bools.camera_allow = true;
   gfx_widget_camera_pip_update(true);
   CHECK(sink_open);
   test_runloop.flags = 0;
   gfx_widget_camera_pip_update(true);
   CHECK(!sink_open);
   test_runloop.flags = RUNLOOP_FLAG_CORE_RUNNING;
   gfx_widget_camera_pip_update(true);
   CHECK(sink_open);

   /* The widgets off (notifications off): shown in the pass the driver
    * draws for it alone, padded without the widgets' metrics */
   gfx_widget_camera_pip_update(false);
   CHECK(sink_open && gfx_widget_camera_pip_showing());
   camera_frame(640, 480);
   frame_in(1280, 720, NULL);
   CHECK(draws == 1 && draw_dims == VIDEO_SCALE_PACK(240, 180));
   CHECK(draw_x == 720 / 54 && draw_y == 720 - 180 - 720 / 54); /* bottom left */
   /* ... and stopped where the driver draws no such pass */
   widget_pass = false;
   gfx_widget_camera_pip_update(false);
   CHECK(!sink_open && !gfx_widget_camera_pip_showing());
   gfx_widget_camera_pip_update(true);
   CHECK(sink_open);
   widget_pass = true;

   /* The video driver going: stopped, and nothing to stop twice */
   gfx_widget_camera_pip_update(true);
   CHECK(sink_open);
   {
      unsigned before = closes;
      gfx_widget_camera_pip_stop();
      CHECK(!sink_open && closes == before + 1);
      gfx_widget_camera_pip_stop();
      CHECK(closes == before + 1);
   }

   /* Turned off: stays off */
   test_settings.bools.camera_pip = false;
   {
      unsigned before = opens;
      gfx_widget_camera_pip_update(true);
      CHECK(opens == before && !sink_open);
   }

   printf("Camera picture-in-picture: %d failures\n", fails);
   return fails ? 1 : 0;
}
