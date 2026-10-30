/* The shipping desktop haptic path with scripted runtime results. */
#include <stdio.h>
#include <string.h>
#include <retro_atomic.h>

static void (*after_load)(void);
static int test_load(const retro_atomic_int_t *value)
{
   int result = retro_atomic_load_acquire_int(value);
   if (after_load)
   {
      void (*callback)(void) = after_load;
      after_load = NULL;
      callback();
   }
   return result;
}
#undef retro_atomic_load_acquire_int
#define retro_atomic_load_acquire_int test_load
#include "input/common/input_openxr.c"

static retro_time_t test_now;
static bool test_focused;
static bool test_menu_open, test_frame, test_navigation;
static unsigned picked_laser, picks;
static bool picked_menu;
static unsigned applies[2], stops[2];
static float amplitudes[2];
static XrResult apply_result[2], stop_result[2];
static unsigned failures;
static openxr_session_t test_session;
static openxr_frame_t test_host_frame;
static input_device_driver_t test_pad_driver;
static char test_device_name[MAX_USERS][64];
static bool test_device_auto[MAX_USERS];

#define CHECK(c, msg) do { if (!(c)) { \
   fprintf(stderr, "FAIL %s\n", msg); failures++; } } while (0)

retro_time_t cpu_features_get_time_usec(void) { return test_now; }
uint32_t runloop_get_flags(void) { return RUNLOOP_FLAG_CORE_RUNNING; }
bool openxr_session_focused(openxr_session_t *session)
{ (void)session; return test_focused; }
bool openxr_session_ended(openxr_session_t *session)
{ (void)session; return false; }
XrSwapchain openxr_frame_cursor(const openxr_frame_t *frame)
{ (void)frame; return test_frame ? (XrSwapchain)(uintptr_t)1 : XR_NULL_HANDLE; }
unsigned openxr_frame_cursor_w(const openxr_frame_t *frame)
{ (void)frame; return VIDEO_XR_CURSOR_W; }
void openxr_frame_set_input_hooks(const openxr_input_hooks_t *hooks)
{ (void)hooks; }
XrTime openxr_frame_predicted_time(openxr_frame_t *frame)
{ (void)frame; return 0; }
XrDuration openxr_frame_period(openxr_frame_t *frame)
{ (void)frame; return 0; }
void openxr_frame_get_shown_quads(openxr_frame_t *frame,
      video_xr_quad_set_t *out, XrSpace *space)
{ (void)frame; memset(out, 0, sizeof(*out)); *space = XR_NULL_HANDLE; }
bool menu_driver_alive(void) { return test_menu_open; }
bool video_driver_get_views_core(const video_views_map_t **map, unsigned *dims)
{ (void)map; (void)dims; return false; }
bool video_driver_get_viewport_info(struct video_viewport *vp)
{ (void)vp; return false; }
void input_driver_platform_request(enum input_platform_request req)
{ (void)req; }
void runloop_msg_queue_push(const char *msg, size_t len, unsigned prio,
      unsigned duration, bool flush, char *title,
      enum message_queue_icon icon, enum message_queue_category category)
{ (void)msg; (void)len; (void)prio; (void)duration; (void)flush;
  (void)title; (void)icon; (void)category; }
const char *msg_hash_to_str(enum msg_hash_enums msg)
{ (void)msg; return ""; }
void video_driver_headset_edit(bool editing, unsigned seq)
{ (void)editing; (void)seq; }
bool input_autoconfigure_connect_ex(const char *name,
      const char *display_name, const char *phys, const char *driver,
      unsigned port, unsigned vid, unsigned pid, uint8_t flags)
{
   (void)display_name; (void)phys; (void)vid; (void)pid;
   strlcpy(test_device_name[port], name, sizeof(test_device_name[port]));
   test_device_auto[port] = (flags & AUTOCONF_FLAG_AUTOCONFIG_ENABLED) != 0;
   input_openxr_slot_connected(port, driver);
   return true;
}
bool input_autoconfigure_disconnect(unsigned port, const char *name)
{
   (void)name;
   test_device_name[port][0] = '\0';
   test_device_auto[port] = false;
   return true;
}
bool input_autoconfigure_pending(void) { return false; }
const char *input_config_get_device_name(unsigned port)
{ return test_device_name[port][0] ? test_device_name[port] : NULL; }
bool input_config_get_device_autoconfigured(unsigned port)
{ return test_device_auto[port]; }
void input_driver_exchange_ports(unsigned a, unsigned b)
{ (void)a; (void)b; }
void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void video_xr_rotate(const video_xr_quat_t *q, const video_xr_vec3_t *v,
      video_xr_vec3_t *out) { (void)q; *out = *v; }
bool video_xr_window_point(const video_viewport_t *vp, float *u, float *v)
{ (void)vp; (void)u; (void)v; return false; }
bool video_xr_quad_to_frame(const video_xr_quad_t *q, float u, float v,
      const video_views_map_t *map, unsigned dims, int16_t *x, int16_t *y)
{ (void)q; (void)u; (void)v; (void)map; (void)dims; (void)x; (void)y; return false; }
bool video_xr_quad_live(const video_xr_quad_t *q, unsigned laser, bool menu)
{ (void)q; (void)laser; (void)menu; return false; }
int video_xr_pick(const video_xr_quad_set_t *set, unsigned laser, bool menu,
      const video_xr_vec3_t *o, const video_xr_vec3_t *d,
      float *u, float *v, float *t)
{
   (void)set; (void)o; (void)d; (void)u; (void)v; (void)t;
   picked_laser = laser; picked_menu = menu; picks++;
   return -1;
}
int video_xr_pick_edit(const video_xr_quad_set_t *set, bool stuck,
      const video_xr_vec3_t *o, const video_xr_vec3_t *d,
      float *u, float *v, float *t, bool *stick)
{
   (void)set; (void)stuck; (void)o; (void)d; (void)u; (void)v; (void)t;
   if (stick)
      *stick = false;
   picked_laser = VIDEO_XR_LIVE_EDIT; picked_menu = false; picks++;
   return -1;
}
float video_xr_cursor(const video_xr_quad_t *q, float u, float v,
      float t, video_xr_pose_t *pose)
{ (void)q; (void)u; (void)v; (void)t; (void)pose; return 0.01f; }
void video_xr_beam(const video_xr_vec3_t *o, const video_xr_vec3_t *d,
      float len, const video_xr_vec3_t *eye, video_xr_pose_t *pose)
{ (void)o; (void)d; (void)len; (void)eye; memset(pose, 0, sizeof(*pose)); }
void video_xr_tag(const video_xr_vec3_t *hand, const video_xr_vec3_t *eye,
      video_xr_pose_t *pose)
{ (void)hand; (void)eye; memset(pose, 0, sizeof(*pose)); }
/* What edit mode reaches, never run here. */
void openxr_frame_get_quads(openxr_frame_t *f, video_xr_quad_set_t *out)
{ (void)f; memset(out, 0, sizeof(*out)); }
void openxr_frame_get_screens(openxr_frame_t *f, video_xr_pose_t *pose,
      XrSpace *space)
{ (void)f; memset(pose, 0, sizeof(*pose)); *space = XR_NULL_HANDLE; }
void config_get_video_screen_xr(unsigned screen, video_screen_xr_t *out)
{ (void)screen; memset(out, 0, sizeof(*out)); }
bool config_get_video_menu_xr(video_screen_xr_t *out)
{ memset(out, 0, sizeof(*out)); return false; }
void config_set_video_screen_xr(unsigned screen,
      const video_screen_xr_t *values)
{ (void)screen; (void)values; }
void config_set_video_menu_xr(const video_screen_xr_t *values)
{ (void)values; }
void video_views_xr_reset(video_screen_xr_t *screens, unsigned count)
{ memset(screens, 0, count * sizeof(*screens)); }
void video_views_xr_normalize(video_screen_xr_t *screen) { (void)screen; }
void video_xr_compose(const video_xr_pose_t *a, const video_xr_pose_t *b,
      video_xr_pose_t *out)
{ (void)a; *out = *b; }
void video_xr_pose_invert(const video_xr_pose_t *a, video_xr_pose_t *out)
{ *out = *a; }
void video_xr_grab_hold(video_xr_grab_t *g, const video_xr_pose_t *aim,
      const video_xr_quat_t *q)
{ (void)aim; g->turn = *q; }
bool video_xr_grab_pose(const video_xr_grab_t *g, const video_xr_pose_t *aim,
      const video_xr_quad_set_t *targets, unsigned skip,
      const video_xr_pose_t *start, video_xr_pose_t *pose,
      video_screen_xr_t *out, unsigned *snapped)
{
   (void)g; (void)targets; (void)skip; (void)start; (void)out;
   if (snapped)
      *snapped = 0;
   *pose = *aim;
   return false;
}
void video_xr_grab_stick(video_xr_grab_t *g, float x, float y, double dt,
      float *scale)
{ (void)g; (void)x; (void)y; (void)dt; (void)scale; }
unsigned video_xr_grab_off_step(const video_xr_grab_t *g,
      const video_xr_pose_t *aim, const video_xr_vec3_t *head, float step,
      const float *rest)
{ (void)g; (void)aim; (void)head; (void)step; (void)rest; return 0; }
unsigned video_xr_grab_step(video_xr_grab_t *g, const video_xr_pose_t *aim,
      const video_xr_vec3_t *head, float step, unsigned axes, float *rest,
      const video_xr_quad_set_t *targets, unsigned skip,
      const video_xr_pose_t *start, const video_xr_hinge_t *lid)
{
   (void)g; (void)aim; (void)head; (void)step; (void)axes; (void)rest;
   (void)targets; (void)skip; (void)start; (void)lid;
   return 0;
}
unsigned video_xr_grab_turn_lid(video_xr_grab_t *g,
      const video_xr_pose_t *aim, const video_xr_hinge_t *lid, float x,
      float y, double dt)
{ (void)g; (void)aim; (void)lid; (void)x; (void)y; (void)dt; return 0; }
bool video_xr_grab_near_hinge(const video_xr_grab_t *g,
      const video_xr_pose_t *aim, const video_xr_quad_set_t *targets,
      const video_xr_hinge_t *h, float reach)
{
   (void)g; (void)aim; (void)targets; (void)h; (void)reach;
   return false;
}
void video_xr_rest_take(const video_xr_quad_set_t *set, unsigned slot,
      float width, float height, float gap, const video_xr_hinge_t *h,
      video_xr_pose_t *pose)
{
   (void)set; (void)slot; (void)width; (void)height; (void)gap; (void)h;
   (void)pose;
}
bool video_xr_grab_through(const video_xr_grab_t *g,
      const video_xr_pose_t *aim, const video_xr_quad_set_t *targets,
      const video_xr_hinge_t *h)
{ (void)g; (void)aim; (void)targets; (void)h; return false; }
unsigned video_xr_grab_hinge(const video_xr_grab_t *g,
      const video_xr_pose_t *aim, const video_xr_quad_set_t *targets,
      unsigned skip, const video_xr_pose_t *start, video_xr_pose_t *pose,
      video_xr_hinge_t *hinge)
{
   (void)g; (void)targets; (void)skip; (void)start; (void)hinge;
   *pose = *aim;
   return 0;
}
void video_xr_grab_put(video_xr_grab_t *g, const video_xr_pose_t *aim,
      const video_xr_pose_t *pose)
{ (void)g; (void)aim; (void)pose; }
void video_xr_grab_put_hinge(video_xr_grab_t *g, const video_xr_pose_t *aim,
      const video_xr_hinge_t *h, const video_xr_pose_t *pose, bool turned)
{ (void)g; (void)aim; (void)h; (void)pose; (void)turned; }
void video_xr_grab_put_line(video_xr_grab_t *g, const video_xr_pose_t *aim,
      const video_xr_hinge_t *h)
{ (void)g; (void)aim; (void)h; }
unsigned video_xr_grab_turn_hinge(video_xr_grab_t *g,
      const video_xr_pose_t *aim, const video_xr_hinge_t *h, float x,
      float y, double dt)
{ (void)g; (void)aim; (void)h; (void)x; (void)y; (void)dt; return 0; }
unsigned video_xr_grab_step_hinge(video_xr_grab_t *g,
      const video_xr_pose_t *aim, const video_xr_hinge_t *h, float size,
      unsigned axes, float *rest, const video_xr_quad_set_t *targets,
      unsigned skip, const video_xr_pose_t *start)
{
   (void)g; (void)aim; (void)h; (void)size; (void)axes; (void)rest;
   (void)targets; (void)skip; (void)start;
   return 0;
}
unsigned video_xr_grab_off_hinge(const video_xr_grab_t *g,
      const video_xr_pose_t *aim, const video_xr_hinge_t *h, float size,
      const float *rest)
{ (void)g; (void)aim; (void)h; (void)size; (void)rest; return 0; }
void video_xr_turn_back(video_xr_pose_t *pose) { (void)pose; }
void video_xr_menu_handle(const video_xr_quad_t *menu, video_xr_quad_t *out)
{ *out = *menu; }
void video_xr_outline(const video_xr_quad_t *q, video_xr_quad_t *out)
{ *out = *q; }
void video_xr_outline_crop(video_xr_quad_t *o, unsigned hide, bool back,
      unsigned *x, unsigned *w)
{ (void)o; (void)hide; (void)back; *x = 0; *w = VIDEO_XR_OUTLINE_DIM; }
unsigned video_xr_outline_cell(unsigned hide, bool back, unsigned lit)
{ (void)hide; (void)back; return VIDEO_XR_OUTLINE_X(lit); }
bool video_xr_hinge_line(const video_xr_hinge_t *h, float width,
      float height, const video_xr_quad_t *target, video_xr_vec3_t *a,
      video_xr_vec3_t *b)
{
   (void)h; (void)width; (void)height; (void)target; (void)a; (void)b;
   return false;
}
void video_xr_hinge_place(video_xr_hinge_t *h, const video_xr_pose_t *pose,
      float width, float height, const video_xr_quad_t *target, bool over)
{
   (void)h; (void)pose; (void)width; (void)height; (void)target;
   (void)over;
}
unsigned video_xr_rest_hinges(const video_xr_quad_set_t *set,
      unsigned skip, float gap, video_xr_rest_t *rest, unsigned max)
{
   (void)set; (void)skip; (void)gap; (void)rest; (void)max;
   return 0;
}
bool video_xr_rest_hinge(const video_xr_quad_set_t *set, unsigned slot,
      unsigned skip, float width, float height, float gap,
      const video_xr_pose_t *pose, video_xr_hinge_t *hinge)
{
   (void)set; (void)slot; (void)skip; (void)width; (void)height;
   (void)gap; (void)pose; (void)hinge;
   return false;
}
bool video_xr_hinge_settle(const video_xr_quad_set_t *set, unsigned slot,
      unsigned skip, float width, float height, float gap,
      const video_xr_hinge_t *h, const video_xr_pose_t *hand,
      video_xr_pose_t *pose)
{
   (void)set; (void)slot; (void)skip; (void)width; (void)height;
   (void)gap; (void)h; (void)hand; (void)pose;
   return false;
}
bool video_xr_grab_settle(const video_xr_grab_t *g,
      const video_xr_pose_t *aim, const video_xr_quad_set_t *targets,
      unsigned skip, const video_xr_hinge_t *h, video_xr_pose_t *pose)
{
   (void)g; (void)aim; (void)targets; (void)skip; (void)h; (void)pose;
   return false;
}
bool video_xr_grab_kept(const video_xr_grab_t *g,
      const video_xr_pose_t *aim, const video_xr_quad_set_t *targets,
      const video_xr_pose_t *start, video_xr_hinge_t *kept)
{
   (void)g; (void)aim; (void)targets; (void)start; (void)kept;
   return false;
}
void openxr_frame_publish_grabs(openxr_frame_t *f, const openxr_grabs_t *g)
{ (void)f; (void)g; }
void video_xr_smooth_step(video_xr_smooth_t *s, unsigned level,
      const video_xr_pose_t *raw, double dt, video_xr_pose_t *out)
{ (void)s; (void)level; (void)dt; *out = *raw; }
void openxr_frame_publish_aims(openxr_frame_t *f, const openxr_aims_t *a)
{ (void)f; (void)a; }
void openxr_frame_get_aims(openxr_frame_t *f, openxr_aims_t *out)
{ (void)f; memset(out, 0, sizeof(*out)); }
int openxr_frame_recenters(openxr_frame_t *f)
{ (void)f; return 0; }
static video_xr_quad_set_t test_shown;
static unsigned test_under;

static XrResult XRAPI_CALL locate(XrSpace space, XrSpace base,
      XrTime time, XrSpaceLocation *out)
{
   (void)space; (void)base; (void)time;
   out->locationFlags = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT
      | XR_SPACE_LOCATION_POSITION_VALID_BIT;
   out->pose.orientation.w = 1.0f;
   return XR_SUCCESS;
}
static XrResult XRAPI_CALL sync_actions(XrSession session,
      const XrActionsSyncInfo *info)
{ (void)session; (void)info; return XR_SUCCESS; }
static XrResult XRAPI_CALL read_bool(XrSession session,
      const XrActionStateGetInfo *info, XrActionStateBoolean *out)
{
   (void)session; (void)info;
   out->isActive = out->currentState = test_navigation ? XR_TRUE : XR_FALSE;
   return XR_SUCCESS;
}
static XrResult XRAPI_CALL read_float(XrSession session,
      const XrActionStateGetInfo *info, XrActionStateFloat *out)
{ (void)session; (void)info; out->isActive = XR_FALSE; return XR_SUCCESS; }
static XrResult XRAPI_CALL read_vector(XrSession session,
      const XrActionStateGetInfo *info, XrActionStateVector2f *out)
{ (void)session; (void)info; out->isActive = XR_FALSE; return XR_SUCCESS; }

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
   memset(&test_host_frame, 0, sizeof(test_host_frame));
   test_host_frame.session = &test_session;
   input_openxr_st.frame = &test_host_frame;
   memset(&test_pad_driver, 0, sizeof(test_pad_driver));
   memset(test_device_name, 0, sizeof(test_device_name));
   memset(test_device_auto, 0, sizeof(test_device_auto));
   input_openxr_devs.count = INPUT_OPENXR_PADS;
   for (p = 0; p < INPUT_OPENXR_PADS; p++)
   {
      input_openxr_devs.slot[p] = (int)p;
      input_openxr_devs.lost[p] = -1;
      input_openxr_devs.landed[p] = true;
      strlcpy(test_device_name[p], input_openxr_device_name(p),
            sizeof(test_device_name[p]));
      test_device_auto[p] = true;
   }
   memset(applies, 0, sizeof(applies));
   memset(stops, 0, sizeof(stops));
   for (h = 0; h < 2; h++)
      apply_result[h] = stop_result[h] = XR_SUCCESS;
   input_openxr_slots_publish();
   for (p = 0; p < INPUT_OPENXR_PADS; p++)
      retro_atomic_store_release_int(&input_openxr_rumble[p], 0);
   retro_atomic_store_release_int(&input_openxr_ready, 1);
   test_now = 1000000;
   test_focused = true;
   test_menu_open = test_frame = false;
   test_navigation = false;
   after_load = NULL;
}
static void service(bool separate, bool paused)
{ input_openxr_haptics(&input_openxr_st, separate, paused); }

static bool set_rumble(unsigned slot, enum retro_rumble_effect effect,
      uint16_t strength)
{ return input_openxr_joypad(&test_pad_driver)->set_rumble(slot, effect, strength); }

static void publish_pointer(unsigned laser, bool menu)
{
   openxr_frame_t *frame = input_openxr_st.frame;
   test_menu_open = menu;
   input_openxr_st.frame = NULL;
   input_openxr_poll(4, VIDEO_OPENXR_CONTROLLERS_COMBINED, laser,
         VIDEO_OPENXR_LASER_BEAM_EDIT, 0.5f);
   input_openxr_st.frame = frame;
}
static void replace_pointer(void)
{ publish_pointer(VIDEO_OPENXR_LASER_ALWAYS, true); }
static void pointer_tests(void)
{
   const XrCompositionLayerBaseHeader *layers[2];
   unsigned laser, menu;
   setup();
   test_frame = true;
   input_openxr_st.aim[0] = (XrSpace)(uintptr_t)1;
   input_openxr_st.aim[1] = (XrSpace)(uintptr_t)2;
   input_openxr_st.LocateSpace = locate;
   for (laser = 0; laser < VIDEO_OPENXR_LASER_LAST; laser++)
      for (menu = 0; menu < 2; menu++)
      {
         publish_pointer(laser, menu != 0);
         picks = 0;
         input_openxr_frame_layers(NULL, 1, &test_shown, XR_NULL_HANDLE,
               layers, 2, &test_under);
         if (laser == VIDEO_OPENXR_LASER_OFF)
            CHECK(picks == 0, "disabled pointer picked a quad");
         else
            CHECK(picks == 2 && picked_laser == laser
                  && picked_menu == (menu != 0), "pointer pair changed");
      }
   publish_pointer(VIDEO_OPENXR_LASER_LAST, false);
   picks = 0;
   input_openxr_frame_layers(NULL, 1, &test_shown, XR_NULL_HANDLE,
         layers, 2, &test_under);
   CHECK(picks == 2 && picked_laser == VIDEO_OPENXR_LASER_AUTO
         && !picked_menu, "a laser past the choices was not Auto");
   publish_pointer(VIDEO_OPENXR_LASER_AUTO, false);
   after_load = replace_pointer;
   picks = 0;
   input_openxr_frame_layers(NULL, 1, &test_shown, XR_NULL_HANDLE,
         layers, 2, &test_under);
   CHECK(picks == 2 && picked_laser == VIDEO_OPENXR_LASER_AUTO
         && !picked_menu, "frame mixed two published pointer configurations");
   input_openxr_frame_layers(NULL, 1, &test_shown, XR_NULL_HANDLE,
         layers, 2, &test_under);
   CHECK(picked_laser == VIDEO_OPENXR_LASER_ALWAYS && picked_menu,
         "next frame did not observe replacement pointer pair");
   input_openxr_st.yield = true;
   publish_pointer(VIDEO_OPENXR_LASER_ALWAYS, true);
   picks = 0;
   input_openxr_frame_layers(NULL, 1, &test_shown, XR_NULL_HANDLE,
         layers, 2, &test_under);
   CHECK(picks == 0, "yielding pointer remained visible");
   publish_pointer(VIDEO_OPENXR_LASER_ALWAYS, false);
   input_openxr_frame_layers(NULL, 1, &test_shown, XR_NULL_HANDLE,
         layers, 2, &test_under);
   CHECK(picks == 2 && !picked_menu, "closed menu did not end pointer yield");
   input_openxr_st.SyncActions = sync_actions;
   input_openxr_st.GetActionStateBoolean = read_bool;
   input_openxr_st.GetActionStateFloat = read_float;
   input_openxr_st.GetActionStateVector2f = read_vector;
   test_navigation = test_menu_open = true;
   input_openxr_poll(4, VIDEO_OPENXR_CONTROLLERS_COMBINED,
         VIDEO_OPENXR_LASER_ALWAYS, VIDEO_OPENXR_LASER_BEAM_EDIT, 0.5f);
   picks = 0;
   input_openxr_frame_layers(NULL, 1, &test_shown, XR_NULL_HANDLE,
         layers, 2, &test_under);
   CHECK(picks == 0, "post-navigation publication did not hide pointer");
   test_navigation = test_menu_open = false;
   input_openxr_poll(4, VIDEO_OPENXR_CONTROLLERS_COMBINED,
         VIDEO_OPENXR_LASER_ALWAYS, VIDEO_OPENXR_LASER_BEAM_EDIT, 0.5f);
   input_openxr_frame_layers(NULL, 1, &test_shown, XR_NULL_HANDLE,
         layers, 2, &test_under);
   CHECK(picks == 2 && picked_laser == VIDEO_OPENXR_LASER_ALWAYS
         && !picked_menu, "completed poll published the wrong pointer pair");
}

int main(void)
{
   pointer_tests();
   setup();
   set_rumble(0, RETRO_RUMBLE_STRONG, 10000);
   set_rumble(0, RETRO_RUMBLE_WEAK, 20000);
   CHECK(!set_rumble(0, (enum retro_rumble_effect)2, 65535),
         "invalid effect accepted");
   CHECK(!set_rumble(0, (enum retro_rumble_effect)-1, 65535),
         "negative effect accepted");
   CHECK(!set_rumble(INPUT_OPENXR_PADS,
         RETRO_RUMBLE_STRONG, 65535), "unclaimed slot accepted");
   CHECK(!set_rumble(MAX_USERS, RETRO_RUMBLE_STRONG, 65535),
         "invalid slot accepted");
   CHECK(!set_rumble((unsigned)-1, RETRO_RUMBLE_STRONG, 65535),
         "negative slot accepted");
   CHECK((unsigned)retro_atomic_load_acquire_int(&input_openxr_rumble[0])
         == ((10000u << 16) | 20000u),
         "invalid effect altered a valid motor, or the two motors are not"
         " kept together");
   setup();
   set_rumble(0, RETRO_RUMBLE_STRONG, 65535);
   set_rumble(0, RETRO_RUMBLE_WEAK, 32768);
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
   set_rumble(0, RETRO_RUMBLE_WEAK, 65535);
   set_rumble(1, RETRO_RUMBLE_STRONG, 32768);
   service(true, false);
   CHECK(amplitudes[0] == 1.0f && amplitudes[1] > 0.49f
         && amplitudes[1] < 0.51f, "separate players lost their stronger motor");

   setup();
   apply_result[0] = XR_ERROR_RUNTIME_FAILURE;
   set_rumble(0, RETRO_RUMBLE_STRONG, 65535);
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
   set_rumble(0, RETRO_RUMBLE_STRONG, 65535);
   service(false, false);
   CHECK(input_openxr_st.haptic[0] == 0.0f, "unfocused result cached as applied");
   test_focused = false;
   input_openxr_poll(4, VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF,
         VIDEO_OPENXR_LASER_BEAM_EDIT, 0.5f);
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
   set_rumble(0, RETRO_RUMBLE_STRONG, 65535);
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
   set_rumble(0, RETRO_RUMBLE_STRONG, 65535);
   service(false, false);
   apply_result[0] = XR_SUCCESS;
   set_rumble(0, RETRO_RUMBLE_STRONG, 32768);
   service(false, false);
   CHECK(applies[0] == 2, "new desired amplitude waited for the failed one");
   apply_result[0] = XR_ERROR_RUNTIME_FAILURE;
   set_rumble(0, RETRO_RUMBLE_STRONG, 65535);
   service(false, false);
   input_openxr_session_destroying(NULL);
   CHECK(input_openxr_st.haptic[0] == 0.0f && !input_openxr_st.frame,
         "session retirement retained haptic state");
   CHECK(!input_openxr_can_rumble(), "session retirement retained readiness");
   setup();
   set_rumble(0, RETRO_RUMBLE_STRONG, 65535);
   service(false, false);
   CHECK(applies[0] == 1, "new session retained retired retry backoff");

   setup();
   apply_result[0] = XR_SESSION_LOSS_PENDING;
   set_rumble(0, RETRO_RUMBLE_STRONG, 65535);
   service(false, false);
   CHECK(input_openxr_st.haptic[0] == 1.0f, "successful loss-pending result rejected");
   puts(failures ? "FAIL openxr_haptics_test" : "PASS openxr_haptics_test");
   return failures ? 1 : 0;
}
