/* input/common/input_openxr.c, the headset's controllers on the
 * desktop and the Quest, as it ships, against a runtime and a frontend
 * the test stands in for: the xr calls it makes through the proc
 * lookup it is given, the session's focus, the frame's time and quads,
 * the run flags and the clock are the test's. No headset can be
 * scripted; what is held to here is what it gives the RetroPad, and
 * that nothing stays held when the runtime has nothing to give.
 *
 * - Combined: one pad from both hands, sticks with Y down, analog and
 *   digital L2/R2, Analog to Digital; Separate: a pad per hand, the
 *   other set unread.
 * - Out of focus, a failed sync, the session gone, a failed setup:
 *   nothing held; the first two logged once.
 * - Touch's menu button: Start, and the RetroArch menu held a second;
 *   a failed sync starts the second over.
 * - Recenter, Laser Pointer and the RetroArch menu answer on any port.
 * - The laser: a whole frame in Always is the light gun's, the open
 *   menu's quad the menu's pointer; a window is the menu's while it
 *   is open, and in Always the gun's where gl2 draws the core in it.
 * - Rumble: strong to the left hand, weak to the right; stopped while
 *   paused; dropped when no core runs. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <math.h>

#include <boolean.h>
#include <compat/strl.h>
#include <openxr/openxr.h>

/* ---- the log ---- */
static char t_log[8192];
static void t_append(const char *fmt, va_list ap)
{
   size_t len = strlen(t_log);
   if (len < sizeof(t_log) - 1)
      vsnprintf(t_log + len, sizeof(t_log) - len, fmt, ap);
}
void RARCH_LOG(const char *fmt, ...) { va_list ap; va_start(ap, fmt); t_append(fmt, ap); va_end(ap); }
void RARCH_WARN(const char *fmt, ...) { va_list ap; va_start(ap, fmt); t_append(fmt, ap); va_end(ap); }
void RARCH_ERR(const char *fmt, ...) { va_list ap; va_start(ap, fmt); t_append(fmt, ap); va_end(ap); }
static void t_reset_log(void) { t_log[0] = '\0'; }
static unsigned t_logged(const char *s)
{
   unsigned n     = 0;
   const char *at = t_log;
   while ((at = strstr(at, s)))
   {
      n++;
      at += strlen(s);
   }
   return n;
}

static int failures;
#define CHECK(cond, ...) do { if (!(cond)) { \
   fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
   fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); failures++; } } while (0)

/* ---- the runtime ---- */
#define T_INSTANCE ((XrInstance)(uintptr_t)0x10)
#define T_SESSION  ((XrSession)(uintptr_t)0x30)

/* Paths: each string a number, from 1. */
static char     t_paths[128][XR_MAX_PATH_LENGTH];
static unsigned t_num_paths;
static XRAPI_ATTR XrResult XRAPI_CALL t_string_to_path(XrInstance i,
      const char *s, XrPath *p)
{
   unsigned k;
   for (k = 0; k < t_num_paths; k++)
      if (!strcmp(t_paths[k], s))
      {
         *p = (XrPath)(k + 1);
         return XR_SUCCESS;
      }
   if (t_num_paths >= 128)
      return XR_ERROR_PATH_COUNT_EXCEEDED;
   strlcpy(t_paths[t_num_paths], s, sizeof(t_paths[0]));
   *p = (XrPath)(++t_num_paths);
   return XR_SUCCESS;
}
static const char *t_path(XrPath p)
{
   return (p && p <= t_num_paths) ? t_paths[p - 1] : "";
}
/* 0 for no hand, 1 the left, 2 the right. */
static int t_hand(XrPath p)
{
   if (!strcmp(t_path(p), "/user/hand/left"))
      return 1;
   if (!strcmp(t_path(p), "/user/hand/right"))
      return 2;
   return 0;
}

static char     t_set_names[16][XR_MAX_ACTION_SET_NAME_SIZE];
static unsigned t_num_sets, t_sets_destroyed;
static XRAPI_ATTR XrResult XRAPI_CALL t_create_set(XrInstance i,
      const XrActionSetCreateInfo *ci, XrActionSet *s)
{
   if (t_num_sets >= 16)
      return XR_ERROR_LIMIT_REACHED;
   strlcpy(t_set_names[t_num_sets], ci->actionSetName,
         sizeof(t_set_names[0]));
   *s = (XrActionSet)(uintptr_t)(0x100 + (++t_num_sets));
   return XR_SUCCESS;
}
static XRAPI_ATTR XrResult XRAPI_CALL t_destroy_set(XrActionSet s)
{ t_sets_destroyed++; return XR_SUCCESS; }
static unsigned t_set_index(XrActionSet s)
{
   return (unsigned)((uintptr_t)s - 0x101);
}

/* Actions as "set/name", in the order they are made. */
static char     t_actions[128][XR_MAX_ACTION_SET_NAME_SIZE + XR_MAX_ACTION_NAME_SIZE];
static unsigned t_action_set[128];
static unsigned t_num_actions;
static XRAPI_ATTR XrResult XRAPI_CALL t_create_action(XrActionSet s,
      const XrActionCreateInfo *ci, XrAction *a)
{
   unsigned set = t_set_index(s);
   if (t_num_actions >= 128 || set >= t_num_sets)
      return XR_ERROR_HANDLE_INVALID;
   snprintf(t_actions[t_num_actions], sizeof(t_actions[0]), "%s/%s",
         t_set_names[set], ci->actionName);
   t_action_set[t_num_actions] = set;
   *a = (XrAction)(uintptr_t)(0x200 + (++t_num_actions));
   return XR_SUCCESS;
}
static unsigned t_action_index(XrAction a)
{
   return (unsigned)((uintptr_t)a - 0x201);
}

/* Profiles suggested; Touch's bindings as action and path. */
static unsigned t_suggested;
static char     t_touch[128][2][XR_MAX_PATH_LENGTH];
static unsigned t_num_touch;
static XRAPI_ATTR XrResult XRAPI_CALL t_suggest(XrInstance i,
      const XrInteractionProfileSuggestedBinding *sb)
{
   uint32_t k;
   t_suggested++;
   if (strcmp(t_path(sb->interactionProfile),
            "/interaction_profiles/oculus/touch_controller"))
      return XR_SUCCESS;
   for (k = 0; k < sb->countSuggestedBindings && t_num_touch < 128; k++)
   {
      strlcpy(t_touch[t_num_touch][0],
            t_actions[t_action_index(sb->suggestedBindings[k].action)],
            sizeof(t_touch[0][0]));
      strlcpy(t_touch[t_num_touch][1],
            t_path(sb->suggestedBindings[k].binding),
            sizeof(t_touch[0][1]));
      t_num_touch++;
   }
   return XR_SUCCESS;
}
static bool t_touch_binds(const char *action, const char *path)
{
   unsigned k;
   for (k = 0; k < t_num_touch; k++)
      if (     !strcmp(t_touch[k][0], action)
            && (!path || !strcmp(t_touch[k][1], path)))
         return true;
   return false;
}

static unsigned t_attached;
static XrResult t_attach_result = XR_SUCCESS;
static XRAPI_ATTR XrResult XRAPI_CALL t_attach(XrSession s,
      const XrSessionActionSetsAttachInfo *ai)
{
   if (t_attach_result != XR_SUCCESS)
      return t_attach_result;
   t_attached += ai->countActionSets;
   return XR_SUCCESS;
}

/* The aim spaces, a hand each. */
static int      t_space_hands[16];
static unsigned t_spaces_made, t_spaces_destroyed;
static XRAPI_ATTR XrResult XRAPI_CALL t_create_space(XrSession s,
      const XrActionSpaceCreateInfo *si, XrSpace *out)
{
   if (t_spaces_made >= 16)
      return XR_ERROR_LIMIT_REACHED;
   t_space_hands[t_spaces_made] = t_hand(si->subactionPath);
   *out = (XrSpace)(uintptr_t)(0x300 + (++t_spaces_made));
   return XR_SUCCESS;
}
static XRAPI_ATTR XrResult XRAPI_CALL t_destroy_space(XrSpace s)
{ t_spaces_destroyed++; return XR_SUCCESS; }

/* The sets the last sync made active. */
static XrResult t_sync_result = XR_SUCCESS;
static bool     t_synced[16];
static XRAPI_ATTR XrResult XRAPI_CALL t_sync(XrSession s,
      const XrActionsSyncInfo *si)
{
   uint32_t k;
   memset(t_synced, 0, sizeof(t_synced));
   if (t_sync_result != XR_SUCCESS)
      return t_sync_result;
   for (k = 0; k < si->countActiveActionSets; k++)
      t_synced[t_set_index(si->activeActionSets[k].actionSet)] = true;
   return XR_SUCCESS;
}

/* What is held: an action ("set/name"), a hand or 0 for either, and
 * its value. */
typedef struct t_held
{
   char action[96];
   int hand;
   float x;
   float y;
} t_held_t;
static t_held_t t_held[16];
static unsigned t_num_held;
static void t_hold(const char *action, int hand, float x, float y)
{
   t_held_t *h = &t_held[t_num_held++];
   strlcpy(h->action, action, sizeof(h->action));
   h->hand = hand;
   h->x    = x;
   h->y    = y;
}
static void t_let_go(void) { t_num_held = 0; }
/* An action's held value for the hand asked, only while its set is
 * synced. */
static const t_held_t *t_find(const XrActionStateGetInfo *gi, bool *active)
{
   unsigned k;
   unsigned a = t_action_index(gi->action);
   int hand   = t_hand(gi->subactionPath);
   *active    = a < t_num_actions && t_synced[t_action_set[a]];
   if (!*active)
      return NULL;
   for (k = 0; k < t_num_held; k++)
      if (     !strcmp(t_held[k].action, t_actions[a])
            && (!hand || !t_held[k].hand || t_held[k].hand == hand))
         return &t_held[k];
   return NULL;
}
static XRAPI_ATTR XrResult XRAPI_CALL t_get_bool(XrSession s,
      const XrActionStateGetInfo *gi, XrActionStateBoolean *st)
{
   bool active;
   const t_held_t *h = t_find(gi, &active);
   st->isActive     = active ? XR_TRUE : XR_FALSE;
   st->currentState = (h && h->x != 0.0f) ? XR_TRUE : XR_FALSE;
   return XR_SUCCESS;
}
static XRAPI_ATTR XrResult XRAPI_CALL t_get_float(XrSession s,
      const XrActionStateGetInfo *gi, XrActionStateFloat *st)
{
   bool active;
   const t_held_t *h = t_find(gi, &active);
   st->isActive     = active ? XR_TRUE : XR_FALSE;
   st->currentState = h ? h->x : 0.0f;
   return XR_SUCCESS;
}
static XRAPI_ATTR XrResult XRAPI_CALL t_get_vec2(XrSession s,
      const XrActionStateGetInfo *gi, XrActionStateVector2f *st)
{
   bool active;
   const t_held_t *h = t_find(gi, &active);
   st->isActive       = active ? XR_TRUE : XR_FALSE;
   st->currentState.x = h ? h->x : 0.0f;
   st->currentState.y = h ? h->y : 0.0f;
   return XR_SUCCESS;
}

/* Each hand's aim, by hand, and the space the last locate was in. */
static bool    t_aim_tracked[3];
static XrPosef t_aim[3];
static XrSpace t_located_base;
static XRAPI_ATTR XrResult XRAPI_CALL t_locate(XrSpace space, XrSpace base,
      XrTime time, XrSpaceLocation *loc)
{
   unsigned n = (unsigned)((uintptr_t)space - 0x301);
   int hand   = (n < t_spaces_made) ? t_space_hands[n] : 0;
   t_located_base     = base;
   loc->locationFlags = 0;
   if (hand && t_aim_tracked[hand])
   {
      loc->pose          = t_aim[hand];
      loc->locationFlags = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT
         | XR_SPACE_LOCATION_POSITION_VALID_BIT;
   }
   return XR_SUCCESS;
}

/* Each hand's haptics: the last amplitude, 0 once stopped. */
static float t_amp[3];
static XRAPI_ATTR XrResult XRAPI_CALL t_apply(XrSession s,
      const XrHapticActionInfo *hi, const XrHapticBaseHeader *h)
{
   t_amp[t_hand(hi->subactionPath)] =
      ((const XrHapticVibration*)h)->amplitude;
   return XR_SUCCESS;
}
static XRAPI_ATTR XrResult XRAPI_CALL t_stop(XrSession s,
      const XrHapticActionInfo *hi)
{
   t_amp[t_hand(hi->subactionPath)] = 0.0f;
   return XR_SUCCESS;
}

static const struct { const char *name; PFN_xrVoidFunction fn; } t_procs[] = {
   { "xrStringToPath",                     (PFN_xrVoidFunction)t_string_to_path },
   { "xrCreateActionSet",                  (PFN_xrVoidFunction)t_create_set },
   { "xrDestroyActionSet",                 (PFN_xrVoidFunction)t_destroy_set },
   { "xrCreateAction",                     (PFN_xrVoidFunction)t_create_action },
   { "xrSuggestInteractionProfileBindings", (PFN_xrVoidFunction)t_suggest },
   { "xrAttachSessionActionSets",          (PFN_xrVoidFunction)t_attach },
   { "xrCreateActionSpace",                (PFN_xrVoidFunction)t_create_space },
   { "xrDestroySpace",                     (PFN_xrVoidFunction)t_destroy_space },
   { "xrSyncActions",                      (PFN_xrVoidFunction)t_sync },
   { "xrGetActionStateBoolean",            (PFN_xrVoidFunction)t_get_bool },
   { "xrGetActionStateFloat",              (PFN_xrVoidFunction)t_get_float },
   { "xrGetActionStateVector2f",           (PFN_xrVoidFunction)t_get_vec2 },
   { "xrLocateSpace",                      (PFN_xrVoidFunction)t_locate },
   { "xrApplyHapticFeedback",              (PFN_xrVoidFunction)t_apply },
   { "xrStopHapticFeedback",               (PFN_xrVoidFunction)t_stop },
   { NULL, NULL }
};
static XRAPI_ATTR XrResult XRAPI_CALL t_get_proc(XrInstance i,
      const char *name, PFN_xrVoidFunction *fn)
{
   unsigned k;
   *fn = NULL;
   for (k = 0; t_procs[k].name; k++)
      if (!strcmp(name, t_procs[k].name))
      {
         *fn = t_procs[k].fn;
         return XR_SUCCESS;
      }
   return XR_ERROR_FUNCTION_UNSUPPORTED;
}

/* ---- the module, as it ships ---- */
#include "../../../input/common/input_openxr.c"

/* ---- the frontend and the frame around it ---- */
static retro_time_t t_clock = 1000000;
retro_time_t cpu_features_get_time_usec(void) { return t_clock; }
static uint32_t t_run_flags = RUNLOOP_FLAG_CORE_RUNNING;
uint32_t runloop_get_flags(void) { return t_run_flags; }
static unsigned t_dpad_mode = ANALOG_DPAD_NONE;
unsigned input_driver_analog_dpad_mode(unsigned port) { return t_dpad_mode; }
/* The left stick right as the D-pad's right: what the test needs. */
uint16_t input_driver_analog_dpad_buttons(unsigned mode,
      const int16_t *analog, float axis_threshold)
{
   if (     mode == ANALOG_DPAD_LSTICK
         && analog[0] > (int16_t)(axis_threshold * 32767.0f))
      return (uint16_t)(1u << RETRO_DEVICE_ID_JOYPAD_RIGHT);
   return 0;
}
/* A whole 800x480 frame, no view map. */
bool video_driver_get_views_core(const video_views_map_t **map,
      unsigned *frame_dims)
{
   *map        = NULL;
   *frame_dims = VIDEO_SCALE_PACK(800, 480);
   return true;
}
/* gl2's viewport: the whole of an 800x480 window, unless a case puts
 * bars around it. */
static const video_viewport_t t_vp_whole = {
   0, VIDEO_SCALE_PACK(800, 480), VIDEO_SCALE_PACK(800, 480) };
static video_viewport_t t_vp = {
   0, VIDEO_SCALE_PACK(800, 480), VIDEO_SCALE_PACK(800, 480) };
bool video_driver_get_viewport_info(struct video_viewport *vp)
{
   *vp = t_vp;
   return true;
}
static bool t_menu_open;
bool menu_driver_alive(void) { return t_menu_open; }

static openxr_session_t     t_xr_session;
static openxr_frame_t       t_xr_frame;
static bool                 t_focused = true;
static XrTime               t_time    = 1000000000;
static video_xr_quad_set_t  t_quads;
static openxr_input_hooks_t t_hooks;
bool openxr_session_focused(openxr_session_t *s)
{
   return s == &t_xr_session && t_focused;
}
XrTime openxr_frame_predicted_time(openxr_frame_t *f) { return t_time; }
void openxr_frame_get_quads(openxr_frame_t *f, video_xr_quad_set_t *out)
{
   *out = t_quads;
}
XrSwapchain openxr_frame_cursor(const openxr_frame_t *f)
{
   return XR_NULL_HANDLE;
}
void openxr_frame_set_input_hooks(const openxr_input_hooks_t *hooks)
{
   if (hooks)
      t_hooks = *hooks;
   else
      memset(&t_hooks, 0, sizeof(t_hooks));
}

/* ---- helpers ---- */
static void t_session(bool frame_controller)
{
   openxr_input_handles_t h;
   memset(&t_xr_session, 0, sizeof(t_xr_session));
   memset(&t_xr_frame, 0, sizeof(t_xr_frame));
   t_xr_session.session     = T_SESSION;
   t_xr_session.local_space = (XrSpace)(uintptr_t)0x41;
   t_xr_session.view_space  = (XrSpace)(uintptr_t)0x42;
   t_xr_frame.session       = &t_xr_session;
   memset(&h, 0, sizeof(h));
   h.frame            = &t_xr_frame;
   h.instance         = T_INSTANCE;
   h.session          = T_SESSION;
   h.get_proc         = t_get_proc;
   h.frame_controller = frame_controller;
   t_hooks.session_created(t_hooks.user, &h);
}

static void t_poll(unsigned controllers, unsigned laser)
{
   input_openxr_poll(controllers, laser, 0.5f);
}

static bool t_btn(unsigned port, unsigned id)
{
   return input_openxr_button(port, id);
}

static int16_t t_axis(unsigned port, unsigned idx, unsigned id)
{
   return input_openxr_analog(port, idx, id, 0);
}

static bool t_nothing_held(void)
{
   unsigned p, id;
   for (p = 0; p < INPUT_OPENXR_PADS; p++)
   {
      for (id = 0; id < RARCH_FIRST_CUSTOM_BIND; id++)
         if (t_btn(p, id))
            return false;
      for (id = 0; id < 2; id++)
         if (     t_axis(p, id, RETRO_DEVICE_ID_ANALOG_X)
               || t_axis(p, id, RETRO_DEVICE_ID_ANALOG_Y))
            return false;
      if (     t_axis(p, RETRO_DEVICE_INDEX_ANALOG_BUTTON,
                  RETRO_DEVICE_ID_JOYPAD_L2)
            || t_axis(p, RETRO_DEVICE_INDEX_ANALOG_BUTTON,
                  RETRO_DEVICE_ID_JOYPAD_R2))
         return false;
   }
   return !t_btn(0, RARCH_MENU_TOGGLE) && !t_btn(0, RARCH_HEADSET_RECENTER)
      && !t_btn(0, RARCH_LASER_POINTER_TOGGLE);
}

/* ---- the tests ---- */
static void test_before_session(void)
{
   input_openxr_register();
   CHECK(t_hooks.session_created && t_hooks.session_destroying
         && t_hooks.frame_layers, "the hooks were not registered");
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_AUTO);
   CHECK(t_nothing_held(), "something is held before there is a session");
   CHECK(!input_openxr_can_rumble(), "rumble before there is a session");
}

static void test_session_made(void)
{
   t_reset_log();
   t_session(false);
   CHECK(t_num_sets == 3 && t_num_actions == 33,
         "%u sets and %u actions made, want 3 and 33", t_num_sets,
         t_num_actions);
   CHECK(t_suggested == 4,
         "%u profiles suggested without the Frame's extension, want 4",
         t_suggested);
   CHECK(t_attached == 3 && t_spaces_made == 2,
         "%u sets attached and %u aim spaces", t_attached, t_spaces_made);
   CHECK(t_touch_binds("combined/start_menu",
            "/user/hand/left/input/menu/click")
         && !t_touch_binds("combined/start", NULL),
         "Touch's menu button is not Start and the menu");
   CHECK(input_openxr_can_rumble(), "rumble does not reach ready controllers");
   CHECK(t_logged("[OpenXR] Headset controllers ready.\n") == 1,
         "log: %s", t_log);
}

static void test_combined(void)
{
   t_let_go();
   t_hold("combined/a", 0, 1.0f, 0.0f);
   t_hold("combined/left_stick", 0, 0.5f, 0.5f);
   t_hold("combined/l2", 0, 0.25f, 0.0f);
   t_hold("combined/r2", 0, 0.75f, 0.0f);
   t_hold("separate/b", 1, 1.0f, 0.0f);
   t_reset_log();
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_btn(0, RETRO_DEVICE_ID_JOYPAD_A)
         && !t_btn(0, RETRO_DEVICE_ID_JOYPAD_B),
         "A held reads %d, B not held reads %d",
         t_btn(0, RETRO_DEVICE_ID_JOYPAD_A),
         t_btn(0, RETRO_DEVICE_ID_JOYPAD_B));
   CHECK(t_axis(0, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X)
            == 16383
         && t_axis(0, RETRO_DEVICE_INDEX_ANALOG_LEFT,
            RETRO_DEVICE_ID_ANALOG_Y) == -16383,
         "the left stick up and right reads %d,%d, want 16383,-16383",
         t_axis(0, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X),
         t_axis(0, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y));
   CHECK(t_axis(0, RETRO_DEVICE_INDEX_ANALOG_BUTTON,
            RETRO_DEVICE_ID_JOYPAD_L2) == 8191
         && t_axis(0, RETRO_DEVICE_INDEX_ANALOG_BUTTON,
            RETRO_DEVICE_ID_JOYPAD_R2) == 24575,
         "the triggers at 0.25,0.75 read %d,%d",
         t_axis(0, RETRO_DEVICE_INDEX_ANALOG_BUTTON,
            RETRO_DEVICE_ID_JOYPAD_L2),
         t_axis(0, RETRO_DEVICE_INDEX_ANALOG_BUTTON,
            RETRO_DEVICE_ID_JOYPAD_R2));
   CHECK(t_btn(0, RETRO_DEVICE_ID_JOYPAD_R2)
         && !t_btn(0, RETRO_DEVICE_ID_JOYPAD_L2),
         "digital triggers past half: L2 %d R2 %d",
         t_btn(0, RETRO_DEVICE_ID_JOYPAD_L2),
         t_btn(0, RETRO_DEVICE_ID_JOYPAD_R2));
   CHECK(!t_btn(1, RETRO_DEVICE_ID_JOYPAD_B),
         "player 2 in Combined, or the Separate set read");
   CHECK(t_logged("[OpenXR] Controllers: combined.\n") == 1,
         "log: %s", t_log);
   /* A source further from centre keeps its value, as the overlay's. */
   CHECK(input_openxr_analog(0, RETRO_DEVICE_INDEX_ANALOG_LEFT,
            RETRO_DEVICE_ID_ANALOG_X, -20000) == -20000,
         "the larger value from elsewhere was lost");

   /* Analog to Digital: the left stick is the D-pad, and centred. */
   t_dpad_mode = ANALOG_DPAD_LSTICK;
   t_let_go();
   t_hold("combined/left_stick", 0, 0.75f, 0.0f);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_btn(0, RETRO_DEVICE_ID_JOYPAD_RIGHT)
         && !t_axis(0, RETRO_DEVICE_INDEX_ANALOG_LEFT,
            RETRO_DEVICE_ID_ANALOG_X),
         "Analog to Digital: right %d, stick %d",
         t_btn(0, RETRO_DEVICE_ID_JOYPAD_RIGHT),
         t_axis(0, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X));
   t_dpad_mode = ANALOG_DPAD_NONE;
}

static void test_separate(void)
{
   t_let_go();
   t_hold("separate/b", 1, 1.0f, 0.0f);
   t_hold("separate/a", 2, 1.0f, 0.0f);
   t_hold("separate/r2", 2, 0.6f, 0.0f);
   t_hold("combined/x", 0, 1.0f, 0.0f);
   t_poll(VIDEO_OPENXR_CONTROLLERS_SEPARATE, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_btn(0, RETRO_DEVICE_ID_JOYPAD_B)
         && !t_btn(0, RETRO_DEVICE_ID_JOYPAD_A),
         "player 1 is not the left hand");
   CHECK(t_btn(1, RETRO_DEVICE_ID_JOYPAD_A)
         && t_btn(1, RETRO_DEVICE_ID_JOYPAD_R2)
         && t_axis(1, RETRO_DEVICE_INDEX_ANALOG_BUTTON,
            RETRO_DEVICE_ID_JOYPAD_R2) == 19660,
         "player 2 is not the right hand: A %d R2 %d (%d)",
         t_btn(1, RETRO_DEVICE_ID_JOYPAD_A),
         t_btn(1, RETRO_DEVICE_ID_JOYPAD_R2),
         t_axis(1, RETRO_DEVICE_INDEX_ANALOG_BUTTON,
            RETRO_DEVICE_ID_JOYPAD_R2));
   CHECK(!t_btn(0, RETRO_DEVICE_ID_JOYPAD_X)
         && !t_btn(1, RETRO_DEVICE_ID_JOYPAD_X),
         "the Combined set was read in Separate");
}

static void test_hotkeys(void)
{
   t_let_go();
   t_hold("combined/recenter", 0, 1.0f, 0.0f);
   t_hold("combined/laser", 0, 1.0f, 0.0f);
   t_hold("combined/menu", 0, 1.0f, 0.0f);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_btn(3, RARCH_HEADSET_RECENTER)
         && t_btn(3, RARCH_LASER_POINTER_TOGGLE)
         && t_btn(3, RARCH_MENU_TOGGLE),
         "the hotkeys do not answer on any port: %d %d %d",
         t_btn(3, RARCH_HEADSET_RECENTER),
         t_btn(3, RARCH_LASER_POINTER_TOGGLE), t_btn(3, RARCH_MENU_TOGGLE));
}

static void test_nothing_held(void)
{
   t_let_go();
   t_hold("combined/a", 0, 1.0f, 0.0f);
   t_hold("combined/right_stick", 0, -1.0f, 0.0f);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_btn(0, RETRO_DEVICE_ID_JOYPAD_A), "fixture: A not held");

   /* Out of focus. */
   t_reset_log();
   t_focused = false;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_nothing_held(), "something held out of focus");
   CHECK(t_logged("[OpenXR] Controllers released: the headset is not focused.\n") == 1,
         "log: %s", t_log);
   t_focused = true;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_btn(0, RETRO_DEVICE_ID_JOYPAD_A),
         "back in focus, what is held does not read");

   /* A failed sync: nothing, warned once. Not focused: nothing, not
    * warned. */
   t_reset_log();
   t_sync_result = XR_ERROR_RUNTIME_FAILURE;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_nothing_held()
         && t_logged("[OpenXR] Controllers not read: xrSyncActions -2.\n") == 1,
         "after a failed sync: held %d, log: %s", !t_nothing_held(), t_log);
   t_reset_log();
   t_sync_result = XR_SESSION_NOT_FOCUSED;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_nothing_held() && !t_log[0],
         "not focused: held %d, log: %s", !t_nothing_held(), t_log);
   t_sync_result = XR_SUCCESS;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_btn(0, RETRO_DEVICE_ID_JOYPAD_A),
         "with the sync working again what is held does not read");
}

static void test_start_menu(void)
{
   t_let_go();
   t_hold("combined/start_menu", 0, 1.0f, 0.0f);
   t_clock = 50000000;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_btn(0, RETRO_DEVICE_ID_JOYPAD_START) && !t_btn(0, RARCH_MENU_TOGGLE),
         "the press: Start %d, menu %d",
         t_btn(0, RETRO_DEVICE_ID_JOYPAD_START), t_btn(0, RARCH_MENU_TOGGLE));
   t_clock += 999000;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(!t_btn(0, RARCH_MENU_TOGGLE), "the menu before the second was up");
   t_clock += 1000;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_btn(0, RARCH_MENU_TOGGLE) && t_btn(0, RETRO_DEVICE_ID_JOYPAD_START),
         "held a second: menu %d, Start %d", t_btn(0, RARCH_MENU_TOGGLE),
         t_btn(0, RETRO_DEVICE_ID_JOYPAD_START));
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_btn(0, RARCH_MENU_TOGGLE), "the menu is not held on every read");
   t_let_go();
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(!t_btn(0, RARCH_MENU_TOGGLE)
         && !t_btn(0, RETRO_DEVICE_ID_JOYPAD_START),
         "let go: menu %d, Start %d", t_btn(0, RARCH_MENU_TOGGLE),
         t_btn(0, RETRO_DEVICE_ID_JOYPAD_START));

   /* A failed sync starts the second over. */
   t_hold("combined/start_menu", 0, 1.0f, 0.0f);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_clock      += 600000;
   t_sync_result = XR_ERROR_RUNTIME_FAILURE;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_sync_result = XR_SUCCESS;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_clock += 600000;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(!t_btn(0, RARCH_MENU_TOGGLE), "the hold went on through a failed sync");
   t_clock += 400000;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_btn(0, RARCH_MENU_TOGGLE),
         "a second's hold after the failed sync did not open the menu");
   t_let_go();
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
}

static void test_laser(void)
{
   int16_t x       = 0;
   int16_t y       = 0;
   int16_t trigger = 0;
   int16_t off     = 0;
   float u         = 0.0f;
   float v         = 0.0f;
   bool pressed    = true;

   /* A whole frame 1.8 m ahead, the right hand at the eyes aiming at
    * its middle. */
   memset(&t_quads, 0, sizeof(t_quads));
   t_quads.num_quads = 1;
   video_xr_pose_identity(&t_quads.quads[0].pose);
   t_quads.quads[0].pose.position.z = -1.8f;
   t_quads.quads[0].width           = 1.6f;
   t_quads.quads[0].height          = 0.96f;
   t_quads.quads[0].kind            = VIDEO_XR_QUAD_FRAME;
   memset(t_aim, 0, sizeof(t_aim));
   t_aim[2].orientation.w = 1.0f;
   t_aim_tracked[2]       = true;
   t_let_go();

   /* Auto leaves a whole frame to the mouse and the overlay. */
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_AUTO);
   CHECK(!input_openxr_pointer(0, RETRO_DEVICE_LIGHTGUN, 0,
            RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X, &x),
         "Auto took a whole frame's gun");

   /* Always: the gun at the middle, aimed in LOCAL. Off the middle
    * first, so a read that writes nothing fails. */
   x = 12345;
   y = 12345;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_ALWAYS);
   CHECK(input_openxr_pointer(0, RETRO_DEVICE_LIGHTGUN, 0,
            RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X, &x)
         && input_openxr_pointer(0, RETRO_DEVICE_LIGHTGUN, 0,
            RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y, &y)
         && abs(x) < 100 && abs(y) < 100,
         "the gun at %d,%d, want the middle", x, y);
   CHECK(t_located_base == t_xr_session.local_space,
         "the aim was located in another space");
   /* Its trigger fires the gun and is no R2. */
   t_hold("combined/r2", 0, 1.0f, 0.0f);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_ALWAYS);
   CHECK(input_openxr_pointer(0, RETRO_DEVICE_LIGHTGUN, 0,
            RETRO_DEVICE_ID_LIGHTGUN_TRIGGER, &trigger)
         && trigger == 1 && !t_btn(0, RETRO_DEVICE_ID_JOYPAD_R2),
         "trigger %d, R2 %d", trigger, t_btn(0, RETRO_DEVICE_ID_JOYPAD_R2));
   t_let_go();
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_ALWAYS);

   /* The open menu's quad: the menu's pointer, pressed by a pull. */
   t_quads.quads[0].kind            = VIDEO_XR_QUAD_MENU;
   t_quads.quads[0].pose.position.z = -1.7f;
   t_menu_open = true;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_AUTO);
   CHECK(input_openxr_menu_pointer(&u, &v, &pressed)
         && fabs(u - 0.5f) < 0.01f && fabs(v - 0.5f) < 0.01f && !pressed,
         "the menu pointer at %.2f,%.2f pressed %d, want the middle",
         u, v, (int)pressed);
   t_hold("combined/r2", 0, 1.0f, 0.0f);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_AUTO);
   CHECK(input_openxr_menu_pointer(&u, &v, &pressed) && pressed,
         "a pull did not press on the menu");
   t_let_go();
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_AUTO);

   /* A window, the menu drawn over it: the menu's pointer while the
    * menu is open, the gun's in Always once it is closed. */
   t_quads.quads[0].kind            = VIDEO_XR_QUAD_WINDOW;
   t_quads.quads[0].pose.position.z = -1.8f;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_AUTO);
   CHECK(input_openxr_menu_pointer(&u, &v, &pressed)
         && fabs(u - 0.5f) < 0.01f && fabs(v - 0.5f) < 0.01f,
         "the window with the menu open is not the menu's pointer");
   t_hold("combined/r2", 0, 1.0f, 0.0f);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_AUTO);
   CHECK(input_openxr_menu_pointer(&u, &v, &pressed) && pressed
         && !t_btn(0, RETRO_DEVICE_ID_JOYPAD_R2),
         "a pull on the window did not press the menu");
   t_let_go();
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_AUTO);
   t_menu_open = false;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_AUTO);
   CHECK(!input_openxr_menu_pointer(&u, &v, &pressed)
         && !input_openxr_pointer(0, RETRO_DEVICE_LIGHTGUN, 0,
            RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X, &x),
         "a window took the laser in Auto with the menu closed");
   x = 12345;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_ALWAYS);
   CHECK(input_openxr_pointer(0, RETRO_DEVICE_LIGHTGUN, 0,
            RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X, &x) && abs(x) < 100
         && !input_openxr_menu_pointer(&u, &v, &pressed),
         "a window in Always is not the gun's, or took the menu "
         "pointer: %d", x);
   /* Always, with the menu open over it: the menu's, no gun. */
   t_menu_open = true;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_ALWAYS);
   CHECK(input_openxr_menu_pointer(&u, &v, &pressed)
         && !input_openxr_pointer(0, RETRO_DEVICE_LIGHTGUN, 0,
            RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X, &x),
         "a window in Always with the menu open is not the menu's");
   t_menu_open = false;

   /* gl2 draws the core in its viewport, bars around it: the frame at
    * 1x in a 1280x720 window. A bar is off the frame. */
   t_quads.quads[0].height = 0.9f;
   t_vp.pos                = VIDEO_POS_PACK(240, 120);
   t_vp.dims               = VIDEO_SCALE_PACK(800, 480);
   t_vp.full_dims          = VIDEO_SCALE_PACK(1280, 720);
   t_aim[2].position.x     = -0.64f;
   x                       = 12345;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_ALWAYS);
   CHECK(input_openxr_pointer(0, RETRO_DEVICE_LIGHTGUN, 0,
            RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X, &x) && x == -0x8000
         && input_openxr_pointer(0, RETRO_DEVICE_LIGHTGUN, 0,
            RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN, &off) && off == 1,
         "a hit on the side bar is at %d, offscreen %d", x, off);
   t_aim[2].position.x     = 0.0f;
   t_aim[2].position.y     = 0.42f;
   x                       = 12345;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_ALWAYS);
   CHECK(input_openxr_pointer(0, RETRO_DEVICE_LIGHTGUN, 0,
            RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X, &x) && x == -0x8000,
         "a hit on the top bar is at %d", x);
   /* Window pixel 400,360 is frame pixel 160,240. */
   t_aim[2].position.x     = -0.299375f;
   t_aim[2].position.y     = 0.0f;
   x                       = 12345;
   y                       = 12345;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_ALWAYS);
   CHECK(input_openxr_pointer(0, RETRO_DEVICE_LIGHTGUN, 0,
            RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X, &x)
         && input_openxr_pointer(0, RETRO_DEVICE_LIGHTGUN, 0,
            RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y, &y)
         && abs(x + 19645) < 100 && abs(y - 67) < 100,
         "the gun inside the bars at %d,%d, want -19645,67", x, y);
   t_aim[2].position.x     = 0.0f;
   t_vp                    = t_vp_whole;

   t_menu_open = false;
   memset(&t_quads, 0, sizeof(t_quads));
   t_aim_tracked[2] = false;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_AUTO);
}

static void test_rumble(void)
{
   t_let_go();
   t_run_flags = RUNLOOP_FLAG_CORE_RUNNING;
   CHECK(input_openxr_set_rumble(0, RETRO_RUMBLE_STRONG, 65535)
         && input_openxr_set_rumble(0, RETRO_RUMBLE_WEAK, 32768),
         "rumble refused with controllers ready");
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_amp[1] == 1.0f && fabs(t_amp[2] - 32768.0f / 65535.0f) < 1e-4f,
         "Combined: left %.3f right %.3f, want strong left, weak right",
         t_amp[1], t_amp[2]);
   /* Separate: each hand its player's stronger motor. */
   input_openxr_set_rumble(1, RETRO_RUMBLE_WEAK, 16384);
   t_poll(VIDEO_OPENXR_CONTROLLERS_SEPARATE, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_amp[1] == 1.0f && fabs(t_amp[2] - 16384.0f / 65535.0f) < 1e-4f,
         "Separate: left %.3f right %.3f", t_amp[1], t_amp[2]);
   /* Paused: stopped until resumed. */
   t_run_flags |= RUNLOOP_FLAG_PAUSED;
   t_poll(VIDEO_OPENXR_CONTROLLERS_SEPARATE, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_amp[1] == 0.0f && t_amp[2] == 0.0f, "rumble went on while paused");
   t_run_flags &= ~RUNLOOP_FLAG_PAUSED;
   t_poll(VIDEO_OPENXR_CONTROLLERS_SEPARATE, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_amp[1] == 1.0f, "rumble did not come back after the pause");
   /* No core: dropped, and the next one does not inherit it. */
   t_run_flags = 0;
   t_poll(VIDEO_OPENXR_CONTROLLERS_SEPARATE, VIDEO_OPENXR_LASER_OFF);
   t_run_flags = RUNLOOP_FLAG_CORE_RUNNING;
   t_poll(VIDEO_OPENXR_CONTROLLERS_SEPARATE, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_amp[1] == 0.0f && t_amp[2] == 0.0f,
         "a closed core's rumble went on: %.3f %.3f", t_amp[1], t_amp[2]);
}

static void test_session_gone(void)
{
   t_let_go();
   t_hold("combined/a", 0, 1.0f, 0.0f);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_hooks.session_destroying(t_hooks.user);
   CHECK(t_sets_destroyed == 3 && t_spaces_destroyed == 2,
         "%u sets and %u spaces destroyed", t_sets_destroyed,
         t_spaces_destroyed);
   CHECK(!input_openxr_can_rumble(), "rumble with the session gone");
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_nothing_held(), "something held with the session gone");

   /* With the Frame's extension, its profile too. */
   t_suggested = 0;
   t_session(true);
   CHECK(t_suggested == 5, "%u profiles with the Frame's extension, want 5",
         t_suggested);
   t_hooks.session_destroying(t_hooks.user);
}

/* Controllers that could not be set up: none, and the session goes on
 * without them. */
static void test_session_failed(void)
{
   unsigned sets      = t_num_sets;
   unsigned sets_gone = t_sets_destroyed;
   unsigned spaces    = t_spaces_made;
   unsigned gone      = t_spaces_destroyed;
   t_reset_log();
   t_attach_result = XR_ERROR_RUNTIME_FAILURE;
   t_session(false);
   t_attach_result = XR_SUCCESS;
   CHECK(!input_openxr_active(), "active after a failed setup");
   CHECK(     t_num_sets - sets == 3
         && t_sets_destroyed - sets_gone == 3
         && t_spaces_destroyed - gone == t_spaces_made - spaces,
         "a failed setup left %u of %u sets and %u spaces",
         (t_num_sets - sets) - (t_sets_destroyed - sets_gone),
         t_num_sets - sets,
         (t_spaces_made - spaces) - (t_spaces_destroyed - gone));
   CHECK(t_logged("[OpenXR] Headset controllers unavailable (xrAttachSessionActionSets: -2).\n") == 1,
         "log: %s", t_log);
   t_let_go();
   t_hold("combined/a", 0, 1.0f, 0.0f);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_nothing_held(), "something held without controllers");
   t_let_go();
}

int main(void)
{
   test_before_session();
   test_session_made();
   test_combined();
   test_separate();
   test_hotkeys();
   test_nothing_held();
   test_start_menu();
   test_laser();
   test_rumble();
   test_session_gone();
   test_session_failed();
   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("PASS openxr_input_test\n");
   return 0;
}
