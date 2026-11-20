/* camera/camera_driver.c's preview against a scripted driver: who opens
 * the camera, who polls it, and who is handed its frames, with and
 * without a core holding a camera of its own.
 *
 * camera_driver.c is built with HAVE_V4L2, and camera_v4l2 here is the
 * scripted driver, so the preview finds it by name as it would the
 * real one. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <libretro.h>
#include <string/stdstring.h>

#include "../../../configuration.h"
#include "../../../runloop.h"
#include "../../../camera/camera_driver.h"
#include "../../../list_special.h"

/* ---- frontend stubs --------------------------------------------- */

static settings_t      test_settings;
static runloop_state_t test_runloop;
static unsigned        msgs_pushed;

settings_t *config_get_ptr(void) { return &test_settings; }
runloop_state_t *runloop_state_get_ptr(void) { return &test_runloop; }

void runloop_frame_work_set(unsigned bit, bool on)
{
   if (on)
      test_runloop.frame_work |= bit;
   else
      test_runloop.frame_work &= ~bit;
}

void runloop_msg_queue_push(const char *msg, size_t len,
      unsigned prio, unsigned duration, bool flush, char *title,
      enum message_queue_icon icon, enum message_queue_category category)
{
   (void)msg; (void)len; (void)prio; (void)duration; (void)flush;
   (void)title; (void)icon; (void)category;
   msgs_pushed++;
}

int driver_find_index(const char *label, const char *drv)
{
   int i;
   (void)label;
   for (i = 0; camera_drivers[i]; i++)
      if (string_is_equal(camera_drivers[i]->ident, drv))
         return i;
   return -1;
}

const char *char_list_new_special(enum string_list_type type, void *data)
{
   (void)type; (void)data;
   return "";
}

void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void RARCH_LOG_OUTPUT(const char *fmt, ...) { (void)fmt; }

/* ---- the scripted driver ---------------------------------------- */

typedef struct
{
   char device[64];
   uint64_t caps;
   bool started;
   bool core;
} fake_cam_t;

static unsigned fake_inits, fake_frees, fake_starts, fake_stops, fake_polls;
static bool     fake_fail_init, fake_fail_start;
static uint64_t fake_last_caps;
static char     fake_last_device[64];
static uint8_t  fake_plane[16 * 16];
static uint32_t fake_pixels[4 * 4];

static void *fake_init(const char *device, uint64_t caps, unsigned dims)
{
   fake_cam_t *cam;
   (void)dims;
   fake_inits++;
   fake_last_caps = caps;
   strlcpy(fake_last_device, device ? device : "(default)",
         sizeof(fake_last_device));
   if (fake_fail_init || !(cam = (fake_cam_t*)calloc(1, sizeof(*cam))))
      return NULL;
   cam->caps = caps;
   strlcpy(cam->device, fake_last_device, sizeof(cam->device));
   return cam;
}
static void fake_free(void *data) { fake_frees++; free(data); }
static bool fake_start(void *data)
{
   fake_starts++;
   if (fake_fail_start)
      return false;
   ((fake_cam_t*)data)->started = true;
   return true;
}
static void fake_stop(void *data)
{
   fake_stops++;
   ((fake_cam_t*)data)->started = false;
}
/* Planes when opened for them, else raw; GL for whoever asks. */
static bool fake_poll(void *data,
      retro_camera_frame_raw_framebuffer_t raw,
      retro_camera_frame_opengl_texture_t gl,
      retro_camera_frame_planar_t planar)
{
   fake_cam_t *cam = (fake_cam_t*)data;
   fake_polls++;
   if (gl)
      gl(7, 0, NULL);
   if (cam->caps & (UINT64_C(1) << RETRO_CAMERA_BUFFER_PLANAR))
   {
      struct retro_camera_planar_frame f;
      if (!planar)
         return false;
      memset(&f, 0, sizeof(f));
      f.planes[0] = f.planes[1] = fake_plane;
      f.planes[2] = fake_plane + 1;
      f.strides[0] = f.strides[1] = f.strides[2] = 16;
      f.chroma_step = 2;
      f.width = f.height = 16;
      planar(&f);
      return true;
   }
   if (!raw)
      return false;
   raw(fake_pixels, 4, 4, 16);
   return true;
}

camera_driver_t camera_v4l2 = {
   fake_init, fake_free, fake_start, fake_stop, fake_poll, "video4linux2",
};

/* ---- the consumers ---------------------------------------------- */

static unsigned core_raw, core_planar, core_gl, sink_raw, sink_planar;

static void RETRO_CALLCONV core_raw_cb(const uint32_t *b, unsigned w,
      unsigned h, size_t p) { (void)b; (void)w; (void)h; (void)p; core_raw++; }
static void RETRO_CALLCONV core_gl_cb(unsigned id, unsigned target,
      const float *affine) { (void)id; (void)target; (void)affine; core_gl++; }
static void RETRO_CALLCONV core_planar_cb(
      const struct retro_camera_planar_frame *f) { (void)f; core_planar++; }
static void sink_planar_cb(void *user,
      const struct retro_camera_planar_frame *f)
{ (void)f; (*(unsigned*)user)++; sink_planar++; }
static void sink_raw_cb(void *user, const uint32_t *b, unsigned w,
      unsigned h, size_t p)
{ (void)b; (void)w; (void)h; (void)p; (*(unsigned*)user)++; sink_raw++; }

static int fails;

#define CHECK(cond, what) do { \
   if (cond) printf("  [ok]   %s\n", what); \
   else { printf("  [FAIL] %s\n", what); fails++; } \
} while (0)

static unsigned sink_user;

static void counters_reset(void)
{
   fake_inits = fake_frees = fake_starts = fake_stops = fake_polls = 0;
   core_raw = core_planar = core_gl = sink_raw = sink_planar = 0;
   sink_user = 0;
}

static void open_preview(void)
{
   camera_preview_sink_t sink;
   sink.planar = sink_planar_cb;
   sink.raw    = sink_raw_cb;
   sink.user   = &sink_user;
   camera_driver_preview_open(&sink);
}

/* A core's camera as drivers_init makes one. */
static void core_camera_init(uint64_t caps)
{
   camera_driver_state_t *st = camera_state_get_ptr();
   memset(&st->cb, 0, sizeof(st->cb));
   st->cb.caps                 = caps;
   st->cb.frame_raw_framebuffer = core_raw_cb;
   st->cb.frame_opengl_texture  = core_gl_cb;
   st->cb.frame_planar          = (caps & (UINT64_C(1)
         << RETRO_CAMERA_BUFFER_PLANAR)) ? core_planar_cb : NULL;
   st->active = true;
   camera_driver_preview_release();
   camera_driver_find_driver("camera driver", false);
   st->data = st->driver->init(NULL, caps, 0);
}

static void core_camera_deinit(void)
{
   camera_driver_state_t *st = camera_state_get_ptr();
   st->driver->free(st->data);
   st->data   = NULL;
   st->active = false;
   memset(&st->cb, 0, sizeof(st->cb));
   camera_driver_preview_release();
   runloop_frame_work_set(RUNLOOP_WORK_CAMERA, false);
}

int main(void)
{
   camera_driver_state_t *st = camera_state_get_ptr();

   strlcpy(test_settings.arrays.camera_driver, "video4linux2",
         sizeof(test_settings.arrays.camera_driver));
   test_settings.arrays.camera_device[0] = '\0';
   camera_driver_find_driver("camera driver", false);

   printf("closed preview:\n");
   counters_reset();
   CHECK(camera_driver_preview_poll() == CAMERA_PREVIEW_OFF
         && fake_inits == 0, "nothing opens without a preview");

   printf("camera not allowed:\n");
   test_settings.bools.camera_allow = false;
   open_preview();
   CHECK(fake_inits == 0, "opening the preview opens no camera");
   CHECK(camera_driver_preview_poll() == CAMERA_PREVIEW_DISALLOWED
         && fake_inits == 0, "disallowed: no camera opened");

   printf("no core camera:\n");
   test_settings.bools.camera_allow = true;
   CHECK(camera_driver_preview_poll() == CAMERA_PREVIEW_OWN,
         "the preview opens its own camera");
   CHECK(fake_inits == 1 && fake_starts == 1 && fake_polls == 1,
         "opened, started and polled once");
   CHECK(fake_last_caps == ((UINT64_C(1) << RETRO_CAMERA_BUFFER_PLANAR)
            | (UINT64_C(1) << RETRO_CAMERA_BUFFER_RAW_FRAMEBUFFER)),
         "asked for planes, raw as the fallback");
   CHECK(string_is_equal(fake_last_device, "(default)"),
         "an empty device setting is the driver's default");
   CHECK(sink_planar == 1 && sink_user == 1, "the frame reached the sink");
   CHECK(camera_driver_preview_poll() == CAMERA_PREVIEW_OWN
         && fake_inits == 1 && fake_polls == 2 && sink_planar == 2,
         "the next poll reuses the camera");

   printf("device and driver changes:\n");
   strlcpy(test_settings.arrays.camera_device, "/dev/video3",
         sizeof(test_settings.arrays.camera_device));
   camera_driver_preview_poll();
   CHECK(fake_frees == 1 && fake_stops == 1 && fake_inits == 2
         && string_is_equal(fake_last_device, "/dev/video3"),
         "a new device reopens on it");
   strlcpy(test_settings.arrays.camera_driver, "null",
         sizeof(test_settings.arrays.camera_driver));
   CHECK(camera_driver_preview_poll() == CAMERA_PREVIEW_OWN
         && fake_frees == 2 && st->preview_driver
         && string_is_equal(st->preview_driver->ident, "null"),
         "a new driver choice reopens with it");
   strlcpy(test_settings.arrays.camera_driver, "video4linux2",
         sizeof(test_settings.arrays.camera_driver));
   camera_driver_preview_poll();
   CHECK(fake_inits == 3, "and back");

   printf("open failures:\n");
   counters_reset();
   camera_driver_preview_release();
   fake_fail_init = true;
   CHECK(camera_driver_preview_poll() == CAMERA_PREVIEW_FAILED
         && fake_inits == 1, "a failed open reports it");
   CHECK(camera_driver_preview_poll() == CAMERA_PREVIEW_FAILED
         && fake_inits == 1, "and is not retried every frame");
   fake_fail_init = false;
   camera_driver_preview_close();
   open_preview();
   fake_fail_start = true;
   counters_reset();
   CHECK(camera_driver_preview_poll() == CAMERA_PREVIEW_FAILED
         && fake_inits == 1 && fake_frees == 1 && fake_stops == 0
         && !st->preview_data,
         "reopening retries; a camera that would not start is freed unstopped");
   fake_fail_start = false;
   strlcpy(test_settings.arrays.camera_device, "/dev/video4",
         sizeof(test_settings.arrays.camera_device));
   CHECK(camera_driver_preview_poll() == CAMERA_PREVIEW_FAILED,
         "a failure stands until the preview reopens");
   camera_driver_preview_close();
   open_preview();
   CHECK(camera_driver_preview_poll() == CAMERA_PREVIEW_OWN,
         "reopened after the failure");

   printf("a core takes the camera:\n");
   counters_reset();
   core_camera_init((UINT64_C(1) << RETRO_CAMERA_BUFFER_RAW_FRAMEBUFFER)
         | (UINT64_C(1) << RETRO_CAMERA_BUFFER_OPENGL_TEXTURE));
   CHECK(fake_stops == 1 && fake_frees == 1 && fake_inits == 1
         && !st->preview_data, "the preview's camera gives way to the core's");
   CHECK(camera_driver_preview_poll() == CAMERA_PREVIEW_CORE_IDLE
         && fake_polls == 0 && fake_inits == 1,
         "a core camera not started is not started or opened over");
   CHECK(driver_camera_start(), "the core starts its camera");
   CHECK(camera_driver_preview_poll() == CAMERA_PREVIEW_SHARED
         && fake_polls == 1 && sink_raw == 1 && core_raw == 0
         && core_gl == 0, "a paused core's camera polled for the sink only");
   camera_driver_poll();
   CHECK(fake_polls == 2 && core_raw == 1 && core_gl == 1 && sink_raw == 2,
         "the core's poll reaches the core and the sink");
   CHECK(camera_driver_preview_poll() == CAMERA_PREVIEW_SHARED
         && fake_polls == 2, "no second poll in a frame the core polled");
   CHECK(camera_driver_preview_poll() == CAMERA_PREVIEW_SHARED
         && fake_polls == 3 && core_raw == 1,
         "the next frame without the core's poll polls for the sink");

   printf("the preview closes under a core:\n");
   camera_driver_preview_close();
   counters_reset();
   camera_driver_poll();
   CHECK(fake_polls == 1 && core_raw == 1 && sink_raw == 0 && sink_user == 0,
         "the core's poll reaches the core alone");
   CHECK(fake_frees == 0 && fake_stops == 0,
         "the core's camera is left as it was");

   printf("allow turned off with a core camera:\n");
   open_preview();
   test_settings.bools.camera_allow = false;
   CHECK(camera_driver_preview_poll() == CAMERA_PREVIEW_DISALLOWED,
         "disallowed shows nothing");
   test_settings.bools.camera_allow = true;

   printf("the core lets the camera go:\n");
   core_camera_deinit();
   counters_reset();
   CHECK(camera_driver_preview_poll() == CAMERA_PREVIEW_OWN
         && fake_inits == 1 && sink_planar == 1,
         "the preview opens its own again");

   printf("released, not closed:\n");
   camera_driver_preview_release();
   CHECK(fake_frees == 1 && st->preview_on, "release frees the camera only");
   CHECK(camera_driver_preview_poll() == CAMERA_PREVIEW_OWN
         && fake_inits == 2, "the next poll opens it again");
   camera_driver_preview_close();
   CHECK(fake_frees == 2 && !st->preview_on && !st->preview_data,
         "close frees the camera and the sink");
   CHECK(camera_driver_preview_poll() == CAMERA_PREVIEW_OFF
         && fake_inits == 2, "a closed preview opens nothing");

   printf(fails ? "\n%d FAILED\n" : "\nall passed\n", fails);
   return fails ? 1 : 0;
}
