/* The shipping desktop haptic path with scripted runtime results. */
#include <stdio.h>
#include <string.h>
#include "input/common/input_openxr.c"

static retro_time_t test_now;
static bool test_focused;
static unsigned applies[2], stops[2];
static float amplitudes[2];
static XrResult apply_result[2], stop_result[2];
static unsigned failures;

#define CHECK(c, msg) do { if (!(c)) { \
   fprintf(stderr, "FAIL %s\n", msg); failures++; } } while (0)

retro_time_t cpu_features_get_time_usec(void) { return test_now; }
uint32_t runloop_get_flags(void) { return RUNLOOP_FLAG_CORE_RUNNING; }
bool vulkan_openxr_focused(vulkan_openxr_t *xr)
{ (void)xr; return test_focused; }
XrSwapchain vulkan_openxr_cursor(const vulkan_openxr_t *xr)
{ (void)xr; return XR_NULL_HANDLE; }
void vulkan_openxr_set_hooks(const vulkan_openxr_hooks_t *hooks)
{ (void)hooks; }
XrTime vulkan_openxr_predicted_time(vulkan_openxr_t *xr)
{ (void)xr; return 0; }
bool vulkan_openxr_get_quads(vulkan_openxr_t *xr, video_xr_quad_set_t *out)
{ (void)xr; (void)out; return false; }
bool video_driver_get_views_core(const video_views_map_t **map, unsigned *dims)
{ (void)map; (void)dims; return false; }
unsigned input_driver_analog_dpad_mode(unsigned port)
{ (void)port; return 0; }
uint16_t input_driver_analog_dpad_buttons(unsigned mode,
      const int16_t *analog, float threshold)
{ (void)mode; (void)analog; (void)threshold; return 0; }
void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void video_xr_rotate(const video_xr_quat_t *q, const video_xr_vec3_t *v,
      video_xr_vec3_t *out) { (void)q; *out = *v; }
bool video_xr_quad_to_frame(const video_xr_quad_t *q, float u, float v,
      const video_views_map_t *map, unsigned dims, int16_t *x, int16_t *y)
{ (void)q; (void)u; (void)v; (void)map; (void)dims; (void)x; (void)y; return false; }
bool video_xr_quad_live(const video_xr_quad_t *q, unsigned laser, bool menu)
{ (void)q; (void)laser; (void)menu; return false; }
int video_xr_pick(const video_xr_quad_set_t *set, unsigned laser, bool menu,
      const video_xr_vec3_t *o, const video_xr_vec3_t *d,
      float *u, float *v, float *t)
{ (void)set; (void)laser; (void)menu; (void)o; (void)d; (void)u; (void)v; (void)t; return -1; }

static XrResult XRAPI_CALL apply(XrSession session,
      const XrHapticActionInfo *info, const XrHapticBaseHeader *haptic)
{
   unsigned hand = (unsigned)info->subactionPath - 1;
   const XrHapticVibration *vib = (const XrHapticVibration*)haptic;
   (void)session;
   CHECK(hand < 2, "invalid haptic hand");
   if (hand >= 2)
      return XR_ERROR_PATH_INVALID;
   CHECK(vib->type == XR_TYPE_HAPTIC_VIBRATION, "wrong haptic type");
   CHECK(vib->duration == INPUT_OPENXR_HAPTIC_DURATION, "wrong haptic duration");
   applies[hand]++;
   amplitudes[hand] = vib->amplitude;
   return apply_result[hand];
}
static XrResult XRAPI_CALL stop(XrSession session, const XrHapticActionInfo *info)
{
   unsigned hand = (unsigned)info->subactionPath - 1;
   (void)session;
   CHECK(hand < 2, "invalid stop hand");
   if (hand >= 2)
      return XR_ERROR_PATH_INVALID;
   stops[hand]++;
   return stop_result[hand];
}
static void setup(void)
{
   unsigned p, h;
   input_openxr_clear(&input_openxr_st);
   input_openxr_st.ApplyHapticFeedback = apply;
   input_openxr_st.StopHapticFeedback = stop;
   input_openxr_st.hands[0] = 1;
   input_openxr_st.hands[1] = 2;
   input_openxr_st.xr = (vulkan_openxr_t*)(uintptr_t)1;
   memset(applies, 0, sizeof(applies));
   memset(stops, 0, sizeof(stops));
   for (h = 0; h < 2; h++)
      apply_result[h] = stop_result[h] = XR_SUCCESS;
   for (p = 0; p < INPUT_OPENXR_PADS; p++)
      for (h = 0; h < 2; h++)
         retro_atomic_store_release_int(&input_openxr_rumble[p][h], 0);
   retro_atomic_store_release_int(&input_openxr_ready, 1);
   test_now = 1000000;
   test_focused = true;
}
static void service(bool separate, bool paused)
{ input_openxr_haptics(&input_openxr_st, separate, paused); }

int main(void)
{
   setup();
   input_openxr_set_rumble(0, RETRO_RUMBLE_STRONG, 65535);
   input_openxr_set_rumble(0, RETRO_RUMBLE_WEAK, 32768);
   service(false, false);
   CHECK(applies[0] == 1 && applies[1] == 1, "combined motors did not reach both hands");
   CHECK(amplitudes[0] == 1.0f && amplitudes[1] > 0.49f
         && amplitudes[1] < 0.51f, "combined motor amplitudes changed");
   service(false, false);
   CHECK(applies[0] == 1, "unchanged amplitude applied before refresh");
   test_now += INPUT_OPENXR_HAPTIC_REFRESH;
   service(false, false);
   CHECK(applies[0] == 2 && applies[1] == 2, "unchanged amplitudes did not refresh");
   service(false, true);
   service(false, true);
   CHECK(stops[0] == 1 && stops[1] == 1, "pause did not stop once per hand");

   setup();
   input_openxr_set_rumble(0, RETRO_RUMBLE_WEAK, 65535);
   input_openxr_set_rumble(1, RETRO_RUMBLE_STRONG, 32768);
   service(true, false);
   CHECK(amplitudes[0] == 1.0f && amplitudes[1] > 0.49f
         && amplitudes[1] < 0.51f, "separate players lost their stronger motor");

   setup();
   apply_result[0] = XR_ERROR_RUNTIME_FAILURE;
   input_openxr_set_rumble(0, RETRO_RUMBLE_STRONG, 65535);
   service(false, false);
   CHECK(input_openxr_st.haptic[0] == 0.0f, "failed apply cached as successful");
   service(false, false);
   CHECK(applies[0] == 1, "failed apply retried without backoff");
   apply_result[0] = XR_SUCCESS;
   test_now += INPUT_OPENXR_HAPTIC_REFRESH;
   service(false, false);
   CHECK(applies[0] == 2 && input_openxr_st.haptic[0] == 1.0f,
         "unchanged failed apply did not recover");
   stop_result[0] = XR_ERROR_RUNTIME_FAILURE;
   service(false, true);
   CHECK(input_openxr_st.haptic[0] == 1.0f, "failed stop cached as successful");
   service(false, true);
   CHECK(stops[0] == 1, "failed stop retried without backoff");
   stop_result[0] = XR_SUCCESS;
   test_now += INPUT_OPENXR_HAPTIC_REFRESH;
   service(false, true);
   CHECK(stops[0] == 2 && input_openxr_st.haptic[0] == 0.0f,
         "unchanged failed stop did not recover");
   service(false, true);
   CHECK(stops[0] == 2, "successful idle stop was repeated");

   setup();
   apply_result[0] = XR_SESSION_NOT_FOCUSED;
   input_openxr_set_rumble(0, RETRO_RUMBLE_STRONG, 65535);
   service(false, false);
   CHECK(input_openxr_st.haptic[0] == 0.0f, "unfocused result cached as applied");
   test_focused = false;
   input_openxr_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF, 0.5f);
   test_focused = true;
   apply_result[0] = XR_SUCCESS;
   service(false, false);
   CHECK(applies[0] == 2, "focus return retained old retry backoff");

   stop_result[0] = XR_SESSION_NOT_FOCUSED;
   service(false, true);
   CHECK(input_openxr_st.haptic[0] == 1.0f,
         "unfocused stop cached as accepted");
   service(false, true);
   CHECK(stops[0] == 1, "unfocused stop retried without backoff");
   stop_result[0] = XR_SUCCESS;
   test_now += INPUT_OPENXR_HAPTIC_REFRESH;
   service(false, true);
   CHECK(stops[0] == 2 && input_openxr_st.haptic[0] == 0.0f,
         "unfocused stop did not recover");

   setup();
   input_openxr_set_rumble(0, RETRO_RUMBLE_STRONG, 65535);
   service(false, false);
   test_now += INPUT_OPENXR_HAPTIC_REFRESH;
   apply_result[0] = XR_ERROR_RUNTIME_FAILURE;
   service(false, false);
   service(false, false);
   CHECK(applies[0] == 2, "failed refresh retried without backoff");
   apply_result[0] = XR_SUCCESS;
   test_now += INPUT_OPENXR_HAPTIC_REFRESH;
   service(false, false);
   CHECK(applies[0] == 3, "failed refresh did not recover");

   setup();
   apply_result[0] = XR_ERROR_RUNTIME_FAILURE;
   input_openxr_set_rumble(0, RETRO_RUMBLE_STRONG, 65535);
   service(false, false);
   apply_result[0] = XR_SUCCESS;
   input_openxr_set_rumble(0, RETRO_RUMBLE_STRONG, 32768);
   service(false, false);
   CHECK(applies[0] == 2, "new desired amplitude waited for the failed one");
   apply_result[0] = XR_ERROR_RUNTIME_FAILURE;
   input_openxr_set_rumble(0, RETRO_RUMBLE_STRONG, 65535);
   service(false, false);
   input_openxr_session_destroying(NULL, input_openxr_st.xr);
   CHECK(input_openxr_st.haptic[0] == 0.0f && !input_openxr_st.xr,
         "session retirement retained haptic state");
   CHECK(!input_openxr_can_rumble(), "session retirement retained readiness");
   setup();
   input_openxr_set_rumble(0, RETRO_RUMBLE_STRONG, 65535);
   service(false, false);
   CHECK(applies[0] == 1, "new session retained retired retry backoff");

   setup();
   apply_result[0] = XR_SESSION_LOSS_PENDING;
   input_openxr_set_rumble(0, RETRO_RUMBLE_STRONG, 65535);
   service(false, false);
   CHECK(input_openxr_st.haptic[0] == 1.0f, "successful loss-pending result rejected");
   puts(failures ? "FAIL openxr_haptics_test" : "PASS openxr_haptics_test");
   return failures ? 1 : 0;
}
