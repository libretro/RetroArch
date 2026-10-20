/* input/common/input_openxr.c, the headset's controllers on the
 * desktop and the Quest, as it ships, against a runtime and a frontend
 * the test stands in for: the xr calls it makes through the proc
 * lookup it is given, the session's focus, the frame's time and quads,
 * the run flags and the clock are the test's. No headset can be
 * scripted; what is held to here is what it gives the RetroPad, and
 * that nothing stays held when the runtime has nothing to give.
 *
 * - Combined: one pad from both hands, sticks with Y down, analog and
 *   digital L2/R2; Separate: a pad per hand, the other set unread. Each
 *   pad is a joypad device in a slot, read through the joypad driver
 *   the module wraps; other slots reach the driver under it.
 * - The devices: connected at the first free slot once a session's
 *   controllers are up, renamed and added or dropped by a mode switch,
 *   gone with the session; a pad that takes a slot moves the headset's
 *   device on, and its port with it.
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
/* A runtime whose profiles lack XR_EXT_palm_pose's path. */
static bool t_refuse_palm;
static XRAPI_ATTR XrResult XRAPI_CALL t_suggest(XrInstance i,
      const XrInteractionProfileSuggestedBinding *sb)
{
   uint32_t k;
   if (t_refuse_palm)
      for (k = 0; k < sb->countSuggestedBindings; k++)
         if (strstr(t_path(sb->suggestedBindings[k].binding), "palm_ext"))
            return XR_ERROR_PATH_UNSUPPORTED;
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
   XrSpaceVelocity *vel = (XrSpaceVelocity*)loc->next;
   t_located_base     = base;
   loc->locationFlags = 0;
   /* A runtime fills a chained velocity; none is known here. */
   if (vel && vel->type == XR_TYPE_SPACE_VELOCITY)
      vel->velocityFlags = 0;
   if (hand && t_aim_tracked[hand])
   {
      loc->pose          = t_aim[hand];
      loc->locationFlags = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT
         | XR_SPACE_LOCATION_POSITION_VALID_BIT;
   }
   return XR_SUCCESS;
}

static unsigned t_path_lookups;
static XRAPI_ATTR XrResult XRAPI_CALL t_path_to_string(XrInstance i,
      XrPath p, uint32_t cap, uint32_t *n, char *buf)
{
   t_path_lookups++;
   *n = (uint32_t)strlen(t_path(p)) + 1;
   if (cap < *n)
      return XR_ERROR_SIZE_INSUFFICIENT;
   strlcpy(buf, t_path(p), cap);
   return XR_SUCCESS;
}
static XRAPI_ATTR XrResult XRAPI_CALL t_get_pose(XrSession s,
      const XrActionStateGetInfo *gi, XrActionStatePose *st)
{
   bool active;
   t_find(gi, &active);
   st->isActive = active ? XR_TRUE : XR_FALSE;
   return XR_SUCCESS;
}
/* Touch in both hands. */
static XRAPI_ATTR XrResult XRAPI_CALL t_get_profile(XrSession s,
      XrPath user, XrInteractionProfileState *st)
{
   return t_string_to_path(T_INSTANCE,
         "/interaction_profiles/oculus/touch_controller",
         &st->interactionProfile);
}

/* The hand trackers, by hand (1 left, 2 right): made, gone, a hand
 * whose tracker is refused, and what each was asked for. */
static unsigned t_trackers_made, t_trackers_destroyed;
static int      t_tracker_refused;
static unsigned t_tracker_sources;     /* data sources the last asked */
static XRAPI_ATTR XrResult XRAPI_CALL t_create_tracker(XrSession s,
      const XrHandTrackerCreateInfoEXT *ci, XrHandTrackerEXT *out)
{
   int hand = ci->hand == XR_HAND_LEFT_EXT ? 1 : 2;
   const XrHandTrackingDataSourceInfoEXT *src =
      (const XrHandTrackingDataSourceInfoEXT*)ci->next;
   t_tracker_sources = (src
         && src->type == XR_TYPE_HAND_TRACKING_DATA_SOURCE_INFO_EXT)
      ? src->requestedDataSourceCount : 0;
   if (     hand == t_tracker_refused
         || ci->handJointSet != XR_HAND_JOINT_SET_DEFAULT_EXT)
      return XR_ERROR_FEATURE_UNSUPPORTED;
   t_trackers_made++;
   *out = (XrHandTrackerEXT)(uintptr_t)(0x500 + hand);
   return XR_SUCCESS;
}
static XRAPI_ATTR XrResult XRAPI_CALL t_destroy_tracker(XrHandTrackerEXT t)
{ t_trackers_destroyed++; return XR_SUCCESS; }

/* Each hand's joints: tracked or not, and what drives them. Joint j is
 * j cm along X with a 5 mm radius. */
static bool     t_joints_active[3];
static XrHandTrackingDataSourceEXT t_joints_source[3];
static XrTime   t_joints_at;
static XRAPI_ATTR XrResult XRAPI_CALL t_locate_joints(XrHandTrackerEXT t,
      const XrHandJointsLocateInfoEXT *li, XrHandJointLocationsEXT *locs)
{
   unsigned j;
   int hand = (int)((uintptr_t)t - 0x500);
   const XrBaseOutStructure *next = (const XrBaseOutStructure*)locs->next;
   XrHandJointVelocitiesEXT *vels  = NULL;
   XrHandTrackingDataSourceStateEXT *src = NULL;
   for (; next; next = next->next)
      if (next->type == XR_TYPE_HAND_JOINT_VELOCITIES_EXT)
         vels = (XrHandJointVelocitiesEXT*)next;
      else if (next->type == XR_TYPE_HAND_TRACKING_DATA_SOURCE_STATE_EXT)
         src  = (XrHandTrackingDataSourceStateEXT*)next;
   t_located_base = li->baseSpace;
   t_joints_at    = li->time;
   locs->isActive = t_joints_active[hand] ? XR_TRUE : XR_FALSE;
   if (src)
   {
      src->isActive   = locs->isActive;
      src->dataSource = t_joints_source[hand];
   }
   if (!locs->isActive)
      return XR_SUCCESS;
   for (j = 0; j < locs->jointCount; j++)
   {
      memset(&locs->jointLocations[j].pose, 0,
            sizeof(locs->jointLocations[j].pose));
      locs->jointLocations[j].pose.orientation.w = 1.0f;
      locs->jointLocations[j].pose.position.x    = (float)j * 0.01f;
      locs->jointLocations[j].radius             = 0.005f;
      locs->jointLocations[j].locationFlags      =
           XR_SPACE_LOCATION_ORIENTATION_VALID_BIT
         | XR_SPACE_LOCATION_POSITION_VALID_BIT
         | XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
      if (vels && j < vels->jointCount)
      {
         memset(&vels->jointVelocities[j], 0,
               sizeof(vels->jointVelocities[j]));
         vels->jointVelocities[j].velocityFlags =
            XR_SPACE_VELOCITY_LINEAR_VALID_BIT;
         vels->jointVelocities[j].linearVelocity.y = 0.5f;
      }
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
   { "xrPathToString",                     (PFN_xrVoidFunction)t_path_to_string },
   { "xrGetActionStatePose",               (PFN_xrVoidFunction)t_get_pose },
   { "xrGetCurrentInteractionProfile",     (PFN_xrVoidFunction)t_get_profile },
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
   { "xrCreateHandTrackerEXT",             (PFN_xrVoidFunction)t_create_tracker },
   { "xrDestroyHandTrackerEXT",            (PFN_xrVoidFunction)t_destroy_tracker },
   { "xrLocateHandJointsEXT",              (PFN_xrVoidFunction)t_locate_joints },
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

/* The device table and the autoconfig tasks: each connect and
 * disconnect is queued, and applied when a case lands them, as the
 * task's callback does. */
typedef struct t_op
{
   bool connect;
   unsigned slot;
   uint8_t flags;
   char name[64];
   char driver[16];
} t_op_t;
static t_op_t   t_ops[256];
static unsigned t_num_ops, t_num_landed;
static char     t_dev_name[MAX_USERS][64];
static bool     t_dev_auto[MAX_USERS];
static bool     t_pending;
bool input_autoconfigure_connect_ex(const char *name,
      const char *display_name, const char *phys, const char *driver,
      unsigned port, unsigned vid, unsigned pid, uint8_t flags)
{
   t_op_t *op = &t_ops[t_num_ops++ & 255];
   op->connect = true;
   op->slot    = port;
   op->flags   = flags;
   strlcpy(op->name, name ? name : "", sizeof(op->name));
   strlcpy(op->driver, driver ? driver : "", sizeof(op->driver));
   return true;
}
bool input_autoconfigure_disconnect(unsigned port, const char *name)
{
   t_op_t *op = &t_ops[t_num_ops++ & 255];
   op->connect = false;
   op->slot    = port;
   strlcpy(op->name, name ? name : "", sizeof(op->name));
   op->driver[0] = '\0';
   return true;
}
bool input_autoconfigure_pending(void)
{
   return t_pending || t_num_landed < t_num_ops;
}
const char *input_config_get_device_name(unsigned port)
{
   return (port < MAX_USERS && t_dev_name[port][0]) ? t_dev_name[port]
      : NULL;
}
bool input_config_get_device_autoconfigured(unsigned port)
{
   return port < MAX_USERS && t_dev_auto[port];
}
/* Port n reads slot t_index[n]. */
static unsigned t_index[MAX_USERS];
static unsigned t_exchanges;
void input_driver_exchange_ports(unsigned a, unsigned b)
{
   unsigned i;
   t_exchanges++;
   for (i = 0; i < MAX_USERS; i++)
   {
      if (t_index[i] == a)
         t_index[i] = b;
      else if (t_index[i] == b)
         t_index[i] = a;
   }
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
static bool                 t_ended;
static XrTime               t_time    = 1000000000;
static video_xr_quad_set_t  t_quads;
static openxr_input_hooks_t t_hooks;
bool openxr_session_focused(openxr_session_t *s)
{
   return s == &t_xr_session && t_focused;
}
bool openxr_session_ended(openxr_session_t *s)
{
   return s == &t_xr_session && t_ended;
}
XrTime openxr_frame_predicted_time(openxr_frame_t *f) { return t_time; }
/* The quads as shown, and their space: LOCAL unless a case sets it. */
static XrSpace t_space;
void openxr_frame_get_shown_quads(openxr_frame_t *f,
      video_xr_quad_set_t *out, XrSpace *space)
{
   *out   = t_quads;
   *space = t_space ? t_space : t_xr_session.local_space;
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
static bool t_palm_pose;
static bool t_hand_tracking;
static bool t_hand_data_source;
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
   h.palm_pose        = t_palm_pose;
   h.hand_tracking    = t_hand_tracking;
   h.hand_data_source = t_hand_data_source;
   t_hooks.session_created(t_hooks.user, &h);
}

/* The joypad driver under the headset's: slot 5 has a pad, with
 * button 3 down and every axis at 1234. */
static unsigned t_inner_calls;
static bool t_inner_query(unsigned pad) { t_inner_calls++; return pad == 5; }
static int32_t t_inner_button(unsigned port, uint16_t joykey)
{
   t_inner_calls++;
   return port == 5 && joykey == 3;
}
static int16_t t_inner_state(rarch_joypad_info_t *joypad_info,
      const uint16_t *joykeys, const uint32_t *joyaxes, unsigned port)
{
   t_inner_calls++;
   return 0x55;
}
static void t_inner_get_buttons(unsigned port, input_bits_t *state)
{
   t_inner_calls++;
   BIT256_CLEAR_ALL_PTR(state);
   BIT256_SET_PTR(state, 7);
}
static int16_t t_inner_axis(unsigned port, uint32_t joyaxis)
{
   t_inner_calls++;
   return port == 5 ? 1234 : 0;
}
static void t_inner_poll(void) { }
static bool t_inner_rumble(unsigned pad, enum retro_rumble_effect effect,
      uint16_t strength)
{
   t_inner_calls++;
   return true;
}
static const char *t_inner_name(unsigned pad)
{
   t_inner_calls++;
   return pad == 5 ? "Pad" : NULL;
}
static input_device_driver_t t_joypad = {
   NULL, t_inner_query, NULL, t_inner_button, t_inner_state,
   t_inner_get_buttons, t_inner_axis, t_inner_poll, t_inner_rumble,
   NULL, NULL, NULL, t_inner_name, "test" };

/* The queued tasks applied, as their callbacks apply them: the newest
 * on a slot wins. */
static void t_land(void)
{
   for (; t_num_landed < t_num_ops; t_num_landed++)
   {
      unsigned k;
      bool newer      = false;
      const t_op_t *op = &t_ops[t_num_landed & 255];
      for (k = t_num_landed + 1; k < t_num_ops; k++)
         if (t_ops[k & 255].slot == op->slot)
            newer = true;
      if (newer)
         continue;
      if (op->connect)
      {
         strlcpy(t_dev_name[op->slot], op->name, sizeof(t_dev_name[0]));
         t_dev_auto[op->slot] = (op->flags
               & AUTOCONF_FLAG_AUTOCONFIG_ENABLED) != 0;
         input_openxr_slot_connected(op->slot, op->driver);
      }
      else
      {
         t_dev_name[op->slot][0] = '\0';
         t_dev_auto[op->slot]    = false;
      }
   }
}

/* A pad the joypad driver connects to @slot. */
static void t_pad_lands(unsigned slot, const char *name)
{
   strlcpy(t_dev_name[slot], name, sizeof(t_dev_name[0]));
   t_dev_auto[slot] = true;
   input_openxr_slot_connected(slot, "udev");
}

static void t_forget_devices(void)
{
   unsigned i;
   for (i = 0; i < MAX_USERS; i++)
   {
      t_dev_name[i][0] = '\0';
      t_dev_auto[i]    = false;
      t_index[i]       = i;
   }
}

/* Unless a case holds them, the tasks a poll queues have run by the
 * next. */
static bool t_hold_tasks;

static void t_poll(unsigned controllers, unsigned laser)
{
   input_openxr_poll(4, controllers, laser, 0.5f);
   if (!t_hold_tasks)
      t_land();
}

/* User port's pad: the headset's device in its slot. */
static int t_slot(unsigned port)
{
   return port < INPUT_OPENXR_PADS ? input_openxr_devs.slot[port] : -1;
}

static bool t_btn(unsigned port, unsigned id)
{
   const input_device_driver_t *j = input_openxr_joypad(&t_joypad);
   if (id >= RARCH_FIRST_CUSTOM_BIND)
      return input_openxr_hotkey(id);
   return t_slot(port) >= 0
      && j->button((unsigned)t_slot(port), (uint16_t)id);
}

/* A stick axis, or L2 or R2's (axes 4 and 5) for ANALOG_BUTTON. */
static int16_t t_axis(unsigned port, unsigned idx, unsigned id)
{
   unsigned a;
   const input_device_driver_t *j = input_openxr_joypad(&t_joypad);
   int s                          = t_slot(port);
   if (s < 0)
      return 0;
   if (idx == RETRO_DEVICE_INDEX_ANALOG_BUTTON)
      return j->axis((unsigned)s,
            AXIS_POS(id == RETRO_DEVICE_ID_JOYPAD_L2 ? 4 : 5));
   a = idx * 2 + id;
   return j->axis((unsigned)s, AXIS_POS(a))
      + j->axis((unsigned)s, AXIS_NEG(a));
}

static bool t_rumble(unsigned port, enum retro_rumble_effect effect,
      uint16_t strength)
{
   const input_device_driver_t *j = input_openxr_joypad(&t_joypad);
   return t_slot(port) >= 0
      && j->set_rumble((unsigned)t_slot(port), effect, strength);
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
      && !t_btn(0, RARCH_LASER_POINTER_TOGGLE)
      && !t_btn(0, RARCH_HEADSET_VIEW_TOGGLE);
}

/* ---- the tests ---- */
static void test_before_session(void)
{
   input_openxr_register();
   CHECK(t_hooks.session_created && t_hooks.session_destroying
         && t_hooks.frame_layers && t_hooks.locate_hands,
         "the hooks were not registered");
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_AUTO);
   CHECK(t_nothing_held(), "something is held before there is a session");
   CHECK(!input_openxr_can_rumble(), "rumble before there is a session");
}

static void test_session_made(void)
{
   t_reset_log();
   t_session(false);
   CHECK(t_num_sets == 3 && t_num_actions == 37,
         "%u sets and %u actions made, want 3 and 37", t_num_sets,
         t_num_actions);
   CHECK(t_suggested == 4,
         "%u profiles suggested without the Frame's extension, want 4",
         t_suggested);
   CHECK(t_attached == 3 && t_spaces_made == 4,
         "%u sets attached and %u aim and grip spaces", t_attached,
         t_spaces_made);
   CHECK(t_touch_binds("pointer/grip", "/user/hand/right/input/grip/pose")
         && !t_touch_binds("pointer/palm", NULL),
         "Touch's grip is not bound, or its palm is without the extension");
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

   /* A stick is a stick: Analog to Digital is the port's, by its binds. */
   t_let_go();
   t_hold("combined/left_stick", 0, 0.75f, 0.0f);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(!t_btn(0, RETRO_DEVICE_ID_JOYPAD_RIGHT)
         && t_axis(0, RETRO_DEVICE_INDEX_ANALOG_LEFT,
            RETRO_DEVICE_ID_ANALOG_X) == 24575,
         "the stick pushed right: D-pad right %d, stick %d",
         t_btn(0, RETRO_DEVICE_ID_JOYPAD_RIGHT),
         t_axis(0, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X));
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
   t_hold("combined/view", 0, 1.0f, 0.0f);
   t_hold("combined/menu", 0, 1.0f, 0.0f);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_btn(3, RARCH_HEADSET_RECENTER)
         && t_btn(3, RARCH_LASER_POINTER_TOGGLE)
         && t_btn(3, RARCH_HEADSET_VIEW_TOGGLE)
         && t_btn(3, RARCH_MENU_TOGGLE),
         "the hotkeys do not answer on any port: %d %d %d %d",
         t_btn(3, RARCH_HEADSET_RECENTER),
         t_btn(3, RARCH_LASER_POINTER_TOGGLE),
         t_btn(3, RARCH_HEADSET_VIEW_TOGGLE), t_btn(3, RARCH_MENU_TOGGLE));

   t_let_go();
   t_hold("separate/view", 0, 1.0f, 0.0f);
   t_poll(VIDEO_OPENXR_CONTROLLERS_SEPARATE, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_btn(3, RARCH_HEADSET_VIEW_TOGGLE),
         "the Separate set's Headset View does not answer");
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
   /* Screens that follow the head are in VIEW, and so is the aim. */
   t_space = t_xr_session.view_space;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_ALWAYS);
   CHECK(t_located_base == t_xr_session.view_space,
         "the aim was not located where the screens are");
   t_space = XR_NULL_HANDLE;
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

/* A core's hands: located with the head, in its space, then marked
 * held and named from the actions on the main thread. */
static void test_hands(void)
{
   struct retro_vr_controller_poses poses;
   const struct retro_vr_hand_state *l = &poses.hands[RETRO_VR_HAND_LEFT];
   const struct retro_vr_hand_state *r = &poses.hands[RETRO_VR_HAND_RIGHT];
   XrSpace base       = (XrSpace)(uintptr_t)0x43;
   const uint32_t loc = RETRO_VR_POSE_ORIENTATION_VALID
      | RETRO_VR_POSE_POSITION_VALID;

   memset(t_aim, 0, sizeof(t_aim));
   t_aim[2].orientation.w = 1.0f;
   t_aim[2].position.x    = 0.25f;
   t_aim_tracked[1]       = false;
   t_aim_tracked[2]       = true;
   t_let_go();
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);

   memset(&poses, 0xff, sizeof(poses));
   CHECK(t_hooks.locate_hands(t_hooks.user, base, t_time, &poses),
         "the hands were not located");
   CHECK(t_located_base == base, "the hands were not located in the host's space");
   CHECK(     r->grip.flags == loc && r->grip.position[0] == 0.25f
         && r->grip.orientation[3] == 1.0f && r->aim.flags == loc,
         "the right hand's grip and aim: flags 0x%x 0x%x", r->grip.flags,
         r->aim.flags);
   CHECK(r->palm.flags == 0 && !(r->flags & RETRO_VR_HAND_JOINTS),
         "a palm or joints without their extensions");
   CHECK(l->grip.flags == 0 && l->grip.position[0] == 0.0f,
         "the untracked left hand has a pose");

   input_openxr_controller_poses(&poses);
   CHECK(r->flags == RETRO_VR_HAND_ACTIVE && l->flags == RETRO_VR_HAND_ACTIVE,
         "both controllers' poses are bound: flags 0x%x 0x%x", l->flags,
         r->flags);
   CHECK(     r->interaction_profile && !strcmp(r->interaction_profile,
            "/interaction_profiles/oculus/touch_controller"),
         "the right hand's profile: %s",
         r->interaction_profile ? r->interaction_profile : "none");

   /* Out of focus, no hand is the core's. */
   t_focused = false;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_hooks.locate_hands(t_hooks.user, base, t_time, &poses);
   input_openxr_controller_poses(&poses);
   CHECK(     r->flags == 0 && r->grip.flags == 0
         && !r->interaction_profile && l->flags == 0,
         "a hand out of focus: flags 0x%x", r->flags);
   t_focused        = true;
   t_aim_tracked[2] = false;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
}

static void test_rumble(void)
{
   t_let_go();
   t_run_flags = RUNLOOP_FLAG_CORE_RUNNING;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_rumble(0, RETRO_RUMBLE_STRONG, 65535)
         && t_rumble(0, RETRO_RUMBLE_WEAK, 32768),
         "rumble refused with controllers ready");
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_amp[1] == 1.0f && fabs(t_amp[2] - 32768.0f / 65535.0f) < 1e-4f,
         "Combined: left %.3f right %.3f, want strong left, weak right",
         t_amp[1], t_amp[2]);
   /* Separate: each hand its player's stronger motor, by its slot. */
   t_poll(VIDEO_OPENXR_CONTROLLERS_SEPARATE, VIDEO_OPENXR_LASER_OFF);
   t_rumble(1, RETRO_RUMBLE_WEAK, 16384);
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
   CHECK(t_sets_destroyed == 3 && t_spaces_destroyed == 4,
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

/* A session of its own: the runtime's tables start over, so any number
 * of cases can make one. */
static void t_fresh_session(void)
{
   t_num_sets    = 0;
   t_num_actions = 0;
   t_num_touch   = 0;
   t_spaces_made = 0;
   t_session(false);
}

/* A trigger held across a session's restart is no new pull: none until
 * it is let go, as after a loss. */
static void test_session_restart_trigger(void)
{
   float u      = 0.0f;
   float v      = 0.0f;
   bool pressed = true;

   memset(&t_quads, 0, sizeof(t_quads));
   t_quads.num_quads = 1;
   video_xr_pose_identity(&t_quads.quads[0].pose);
   t_quads.quads[0].pose.position.z = -1.7f;
   t_quads.quads[0].width           = 1.6f;
   t_quads.quads[0].height          = 0.96f;
   t_quads.quads[0].kind            = VIDEO_XR_QUAD_MENU;
   memset(t_aim, 0, sizeof(t_aim));
   t_aim[2].orientation.w = 1.0f;
   t_aim_tracked[2]       = true;
   t_menu_open            = true;
   t_let_go();
   t_fresh_session();
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_AUTO);
   t_hold("combined/r2", 0, 1.0f, 0.0f);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_AUTO);
   CHECK(input_openxr_menu_pointer(&u, &v, &pressed) && pressed,
         "restart: a pull did not press on the menu");

   /* the session goes and comes back with the trigger still held */
   t_hooks.session_destroying(t_hooks.user);
   t_fresh_session();
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_AUTO);
   CHECK(input_openxr_menu_pointer(&u, &v, &pressed) && !pressed,
         "a trigger held across a session restart pressed the menu");

   /* let go and pulled again: that one presses */
   t_let_go();
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_AUTO);
   t_hold("combined/r2", 0, 1.0f, 0.0f);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_AUTO);
   CHECK(input_openxr_menu_pointer(&u, &v, &pressed) && pressed,
         "after a session restart a new pull did not press on the menu");
   t_let_go();
   t_menu_open = false;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_AUTO);
   t_hooks.session_destroying(t_hooks.user);
   memset(&t_quads, 0, sizeof(t_quads));
}

/* With XR_EXT_palm_pose, the palm too; a runtime that refuses it keeps
 * the rest of each profile. */
static void test_palm(void)
{
   t_palm_pose = true;
   t_fresh_session();
   CHECK(t_touch_binds("pointer/palm", "/user/hand/left/input/palm_ext/pose")
         && t_spaces_made == 6,
         "the palm is not bound with its extension (%u spaces)",
         t_spaces_made);
   t_hooks.session_destroying(t_hooks.user);
   t_refuse_palm = true;
   t_suggested   = 0;
   t_fresh_session();
   CHECK(t_suggested == 4 && t_touch_binds("pointer/grip", NULL)
         && !t_touch_binds("pointer/palm", NULL),
         "%u profiles suggested, want 4 without the refused palm",
         t_suggested);
   t_hooks.session_destroying(t_hooks.user);
   t_refuse_palm = false;
   t_palm_pose   = false;
}

/* The joints: a tracker per hand with XR_EXT_hand_tracking, asking for
 * seen and controller-driven hands with the data-source extension;
 * located with the poses in the host's space and time, by OpenXR's
 * joint order, each hand's source on its flags; a tracked hand stays
 * the core's without its controller; the trackers go with the session,
 * and one refused takes the other with it. The profile's string is
 * looked up once per profile. */
static void test_joints(void)
{
   struct retro_vr_controller_poses poses;
   const struct retro_vr_hand_state *l = &poses.hands[RETRO_VR_HAND_LEFT];
   const struct retro_vr_hand_state *r = &poses.hands[RETRO_VR_HAND_RIGHT];
   const struct retro_vr_hand_joint_state *tip =
      &r->joints[RETRO_VR_HAND_JOINT_INDEX_TIP];
   XrSpace base = (XrSpace)(uintptr_t)0x43;
   unsigned made, gone, lookups;

   t_hand_tracking    = true;
   t_hand_data_source = true;
   t_trackers_made    = 0;
   t_trackers_destroyed = 0;
   t_reset_log();
   t_fresh_session();
   CHECK(t_trackers_made == 2 && t_tracker_sources == 2
         && t_logged("Hand joints tracked"),
         "%u trackers made asking %u sources, want 2 asking 2",
         t_trackers_made, t_tracker_sources);

   t_joints_active[1] = true;
   t_joints_source[1] = XR_HAND_TRACKING_DATA_SOURCE_CONTROLLER_EXT;
   t_joints_active[2] = true;
   t_joints_source[2] = XR_HAND_TRACKING_DATA_SOURCE_UNOBSTRUCTED_EXT;
   t_let_go();
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   memset(&poses, 0xff, sizeof(poses));
   CHECK(t_hooks.locate_hands(t_hooks.user, base, t_time, &poses),
         "the hands were not located");
   CHECK(t_located_base == base && t_joints_at == t_time,
         "the joints were not located in the host's space and time");
   CHECK(     (r->flags & (RETRO_VR_HAND_JOINTS | RETRO_VR_HAND_JOINTS_OPTICAL))
               == (RETRO_VR_HAND_JOINTS | RETRO_VR_HAND_JOINTS_OPTICAL)
         && !(r->flags & RETRO_VR_HAND_JOINTS_CONTROLLER)
         && (l->flags & RETRO_VR_HAND_JOINTS_CONTROLLER)
         && !(l->flags & RETRO_VR_HAND_JOINTS_OPTICAL),
         "the joints' sources: flags 0x%x 0x%x", l->flags, r->flags);
   CHECK(     tip->pose.position[0] > 0.0999f && tip->pose.position[0] < 0.1001f
         && tip->radius == 0.005f
         && tip->pose.flags == (RETRO_VR_POSE_ORIENTATION_VALID
            | RETRO_VR_POSE_POSITION_VALID | RETRO_VR_POSE_POSITION_TRACKED
            | RETRO_VR_POSE_LINEAR_VELOCITY_VALID)
         && tip->pose.linear_velocity[1] == 0.5f,
         "the index tip: x %f radius %f flags 0x%x",
         (double)tip->pose.position[0], (double)tip->radius,
         tip->pose.flags);

   /* Looked up once, then kept while the profile is the same. */
   lookups = t_path_lookups;
   input_openxr_controller_poses(&poses);
   input_openxr_controller_poses(&poses);
   CHECK(t_path_lookups - lookups == 2,
         "the profile's string was looked up %u times for two hands",
         t_path_lookups - lookups);

   /* Out of focus no controller is the core's; the seen hand is. */
   t_focused = false;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_hooks.locate_hands(t_hooks.user, base, t_time, &poses);
   input_openxr_controller_poses(&poses);
   CHECK(     (r->flags & (RETRO_VR_HAND_ACTIVE | RETRO_VR_HAND_JOINTS))
               == (RETRO_VR_HAND_ACTIVE | RETRO_VR_HAND_JOINTS)
         && r->grip.flags == 0 && r->aim.flags == 0
         && !r->interaction_profile && tip->radius == 0.005f,
         "a tracked hand without its controller: flags 0x%x grip 0x%x",
         r->flags, r->grip.flags);
   t_focused = true;

   /* Not tracked: no joints. */
   t_joints_active[2] = false;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_hooks.locate_hands(t_hooks.user, base, t_time, &poses);
   CHECK(!(r->flags & RETRO_VR_HAND_JOINTS) && tip->pose.flags == 0,
         "an untracked hand has joints: flags 0x%x", r->flags);

   gone = t_trackers_destroyed;
   t_hooks.session_destroying(t_hooks.user);
   CHECK(t_trackers_destroyed - gone == 2,
         "%u trackers went with the session, want 2",
         t_trackers_destroyed - gone);

   /* Without the data source: joints of no known source. */
   t_hand_data_source = false;
   t_joints_active[2] = true;
   t_fresh_session();
   CHECK(t_tracker_sources == 0, "data sources asked without the extension");
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_hooks.locate_hands(t_hooks.user, base, t_time, &poses);
   CHECK(     (r->flags & RETRO_VR_HAND_JOINTS)
         && !(r->flags & (RETRO_VR_HAND_JOINTS_OPTICAL
               | RETRO_VR_HAND_JOINTS_CONTROLLER)),
         "joints without a data source: flags 0x%x", r->flags);
   t_hooks.session_destroying(t_hooks.user);

   /* The right hand's tracker refused: the left's goes, the
    * controllers stay. */
   t_tracker_refused = 2;
   made = t_trackers_made;
   gone = t_trackers_destroyed;
   t_reset_log();
   t_fresh_session();
   CHECK(     t_trackers_made - made == 1 && t_trackers_destroyed - gone == 1
         && t_logged("Headset controllers ready"),
         "a refused tracker: %u made, %u destroyed",
         t_trackers_made - made, t_trackers_destroyed - gone);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_hooks.locate_hands(t_hooks.user, base, t_time, &poses);
   CHECK(     !(l->flags & RETRO_VR_HAND_JOINTS)
         && !(r->flags & RETRO_VR_HAND_JOINTS),
         "joints from a refused tracker: flags 0x%x 0x%x",
         l->flags, r->flags);
   gone = t_trackers_destroyed;
   t_hooks.session_destroying(t_hooks.user);
   CHECK(t_trackers_destroyed == gone,
         "a refused tracker's pair destroyed again");

   t_tracker_refused  = 0;
   t_joints_active[1] = false;
   t_joints_active[2] = false;
   t_hand_tracking    = false;
}

static bool t_op_is(unsigned i, bool connect, unsigned slot,
      const char *name)
{
   const t_op_t *op = &t_ops[i & 255];
   return i < t_num_ops && op->connect == connect && op->slot == slot
      && (!connect || (!strcmp(op->name, name)
            && !strcmp(op->driver, INPUT_OPENXR_JOYPAD)
            && (op->flags & AUTOCONF_FLAG_AUTOCONFIG_ENABLED)));
}

/* The devices' lives: connected when a session's controllers are up,
 * at the lowest free slot, renamed in place and joined or left by the
 * other hand on a mode switch, kept through a session remade before a
 * poll or out of focus, announced again when forgotten or after a
 * driver restart, and disconnected when the session ends or goes once
 * their connect has landed. */
static void test_devices(void)
{
   unsigned n;
   t_let_go();
   t_forget_devices();
   t_land();
   n = t_num_ops;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_num_ops == n, "a device without a session");

   /* A pad in slot 0: the headset is the next controller. */
   t_pad_lands(0, "Pad");
   t_fresh_session();
   t_pending = true;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_num_ops == n, "a slot was taken while another connect was "
         "pending");
   t_pending = false;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_num_ops == n + 1
         && t_op_is(n, true, 1, INPUT_OPENXR_JOYPAD_COMBINED),
         "%u ops, the first to slot %u as \"%s\" by \"%s\" (flags %u), "
         "want \"%s\" by \"openxr\" in slot 1", t_num_ops - n,
         t_ops[n & 255].slot, t_ops[n & 255].name, t_ops[n & 255].driver,
         (unsigned)t_ops[n & 255].flags, INPUT_OPENXR_JOYPAD_COMBINED);
   n = t_num_ops;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_num_ops == n, "connected again");

   /* Separate: the device is the left hand where it is, the right hand
    * the next free slot. */
   t_poll(VIDEO_OPENXR_CONTROLLERS_SEPARATE, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_num_ops == n + 2
         && t_op_is(n, true, 1, INPUT_OPENXR_JOYPAD_LEFT)
         && t_op_is(n + 1, true, 2, INPUT_OPENXR_JOYPAD_RIGHT),
         "Separate: %u ops (%s in %u, %s in %u)", t_num_ops - n,
         t_ops[n & 255].name, t_ops[n & 255].slot,
         t_ops[(n + 1) & 255].name, t_ops[(n + 1) & 255].slot);
   /* And back: the right hand goes, the left is the pair again. */
   n = t_num_ops;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_num_ops == n + 2 && t_op_is(n, false, 2, NULL)
         && t_op_is(n + 1, true, 1, INPUT_OPENXR_JOYPAD_COMBINED),
         "back to Combined: %u ops", t_num_ops - n);

   /* A session remade before a poll changes nothing. */
   n = t_num_ops;
   t_hooks.session_destroying(t_hooks.user);
   t_fresh_session();
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_num_ops == n, "a remade session moved the device");

   /* Forgotten, as a config load forgets: announced again in place. */
   t_dev_name[1][0] = '\0';
   t_dev_auto[1]    = false;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_num_ops == n + 1
         && t_op_is(n, true, 1, INPUT_OPENXR_JOYPAD_COMBINED),
         "a forgotten device was not announced again");

   /* Out of focus it stays; once the session has ended the next poll
    * disconnects it, and a new session connects it again. */
   n = t_num_ops;
   t_focused = false;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_num_ops == n, "an unfocused session dropped the device");
   t_ended = true;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_num_ops == n + 1 && t_op_is(n, false, 1, NULL),
         "an ended session's device was not disconnected: %u ops",
         t_num_ops - n);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_num_ops == n + 1, "an ended session's device came back");
   t_hooks.session_destroying(t_hooks.user);
   t_ended   = false;
   t_focused = true;
   t_fresh_session();
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_num_ops == n + 2
         && t_op_is(n + 1, true, 1, INPUT_OPENXR_JOYPAD_COMBINED),
         "a new session did not connect the device again");

   /* A driver restart: reported again in place; the disconnect that
    * follows waits for that connect to land. */
   t_hold_tasks = true;
   n = t_num_ops;
   input_openxr_reannounce();
   CHECK(t_num_ops == n + 1
         && t_op_is(n, true, 1, INPUT_OPENXR_JOYPAD_COMBINED),
         "not reported again after a restart");
   t_hooks.session_destroying(t_hooks.user);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_num_ops == n + 1, "disconnected ahead of its own connect");
   t_land();
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_num_ops == n + 2 && t_op_is(n + 1, false, 1, NULL),
         "not disconnected with the session");
   t_hold_tasks = false;
   t_land();
   CHECK(input_openxr_joypad(&t_joypad) == &t_joypad,
         "the driver is wrapped with no device");
}

/* A pad the joypad driver puts in the headset's slot: the slot is the
 * pad's at once, and the headset's device moves to the next free slot
 * with its port. */
static void test_device_taken(void)
{
   unsigned x;
   t_let_go();
   t_forget_devices();
   t_fresh_session();
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_slot(0) == 0, "the device is in slot %d, want 0", t_slot(0));
   t_hold("combined/a", 0, 1.0f, 0.0f);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_pad_lands(0, "Pad");
   CHECK(input_openxr_joypad(&t_joypad) == &t_joypad,
         "slot 0 is still answered for the headset");
   x = t_exchanges;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_slot(0) == 1 && t_exchanges == x + 1
         && t_index[0] == 1 && t_index[1] == 0,
         "moved to slot %d, ports read %u and %u, want the headset's 1 on "
         "port 1", t_slot(0), t_index[0], t_index[1]);
   CHECK(t_btn(0, RETRO_DEVICE_ID_JOYPAD_A),
         "A is not answered in the new slot");
   t_hooks.session_destroying(t_hooks.user);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_let_go();
   t_forget_devices();
}

/* The joypad driver readers get: the device's slot answered from its
 * pad with the RetroPad's ids as buttons and sticks, L2 and R2 as axes
 * 0 to 5; every other slot from the driver under it. */
static void test_joypad_wrapped(void)
{
   static const char *const names[RARCH_FIRST_CUSTOM_BIND] = {
      "b", "y", "select", "start", "dpad_up", "dpad_down", "dpad_left",
      "dpad_right", "a", "x", "l", "r", "l2", "r2", "l3", "r3" };
   unsigned id, s, calls;
   input_bits_t bits;
   rarch_joypad_info_t info;
   uint16_t keys[RARCH_FIRST_CUSTOM_BIND];
   uint32_t axes[RARCH_FIRST_CUSTOM_BIND];
   const input_device_driver_t *j;

   t_let_go();
   t_forget_devices();
   t_fresh_session();
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   s = (unsigned)t_slot(0);
   j = input_openxr_joypad(&t_joypad);
   CHECK(j != &t_joypad && j->query_pad(s) && input_openxr_has_slot(s)
         && !input_openxr_has_slot(5)
         && !strcmp(j->name(s), INPUT_OPENXR_JOYPAD_COMBINED)
         && !strcmp(j->ident, "test"),
         "the device's slot %u is not answered as a device", s);

   for (id = 0; id < RARCH_FIRST_CUSTOM_BIND; id++)
   {
      char action[32];
      unsigned other, mask = 0;
      snprintf(action, sizeof(action), "combined/%s", names[id]);
      t_let_go();
      t_hold(action, 0, 1.0f, 0.0f);
      t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
      for (other = 0; other < RARCH_FIRST_CUSTOM_BIND; other++)
         if (j->button(s, (uint16_t)other))
            mask |= 1u << other;
      j->get_buttons(s, &bits);
      CHECK(mask == (1u << id) && bits.data[0] == mask,
            "%s held: buttons 0x%x, get_buttons 0x%x", names[id], mask,
            (unsigned)bits.data[0]);
   }

   t_let_go();
   t_hold("combined/b", 0, 1.0f, 0.0f);
   t_hold("combined/left_stick", 0, 0.5f, 0.5f);
   t_hold("combined/right_stick", 0, -1.0f, -0.25f);
   t_hold("combined/l2", 0, 0.25f, 0.0f);
   t_hold("combined/r2", 0, 0.75f, 0.0f);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(j->axis(s, AXIS_POS(0)) == 16383 && j->axis(s, AXIS_NEG(0)) == 0
         && j->axis(s, AXIS_NEG(1)) == -16383 && j->axis(s, AXIS_POS(1)) == 0
         && j->axis(s, AXIS_NEG(2)) == -32767 && j->axis(s, AXIS_POS(3)) == 8191
         && j->axis(s, AXIS_POS(4)) == 8191 && j->axis(s, AXIS_POS(5)) == 24575
         && j->axis(s, AXIS_NEG(4)) == 0 && j->axis(s, AXIS_POS(6)) == 0
         && j->axis(s, AXIS_NONE) == 0,
         "axes %d %d %d %d %d %d", j->axis(s, AXIS_POS(0)),
         j->axis(s, AXIS_NEG(1)), j->axis(s, AXIS_NEG(2)),
         j->axis(s, AXIS_POS(3)), j->axis(s, AXIS_POS(4)),
         j->axis(s, AXIS_POS(5)));
   /* The common loop over a port's joykeys and joyaxes. */
   for (id = 0; id < RARCH_FIRST_CUSTOM_BIND; id++)
   {
      keys[id] = NO_BTN;
      axes[id] = AXIS_NONE;
   }
   keys[RETRO_DEVICE_ID_JOYPAD_B]  = RETRO_DEVICE_ID_JOYPAD_B;
   keys[RETRO_DEVICE_ID_JOYPAD_A]  = RETRO_DEVICE_ID_JOYPAD_A;
   axes[RETRO_DEVICE_ID_JOYPAD_UP] = AXIS_NEG(1);
   axes[RETRO_DEVICE_ID_JOYPAD_L2] = AXIS_POS(4);
   axes[RETRO_DEVICE_ID_JOYPAD_R2] = AXIS_POS(5);
   info.joy_idx        = s;
   info.auto_binds     = NULL;
   info.axis_threshold = 0.4f;
   CHECK(j->state(&info, keys, axes, 0)
         == ((1 << RETRO_DEVICE_ID_JOYPAD_B) | (1 << RETRO_DEVICE_ID_JOYPAD_UP)
            | (1 << RETRO_DEVICE_ID_JOYPAD_R2)),
         "state 0x%x", (unsigned)j->state(&info, keys, axes, 0));

   /* Other slots are the driver's. */
   calls        = t_inner_calls;
   info.joy_idx = 5;
   j->get_buttons(5, &bits);
   CHECK(j->button(5, 3) && j->axis(5, AXIS_POS(0)) == 1234
         && j->query_pad(5) && !strcmp(j->name(5), "Pad")
         && j->state(&info, keys, axes, 5) == 0x55 && BIT256_GET(bits, 7)
         && j->set_rumble(5, RETRO_RUMBLE_STRONG, 1)
         && t_inner_calls == calls + 7,
         "slot 5 did not reach the driver under it");

   /* While the laser owns a trigger, its axis is 0 too. */
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
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_ALWAYS);
   t_hold("combined/r2", 0, 1.0f, 0.0f);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_ALWAYS);
   CHECK(!j->axis(s, AXIS_POS(5))
         && !j->button(s, RETRO_DEVICE_ID_JOYPAD_R2),
         "the laser's trigger reads axis %d, R2 %d",
         j->axis(s, AXIS_POS(5)), j->button(s, RETRO_DEVICE_ID_JOYPAD_R2));
   memset(&t_quads, 0, sizeof(t_quads));
   t_aim_tracked[2] = false;
   t_let_go();

   /* Save Controller Profile looks the device's profile up again, as
    * the headset's. */
   calls = t_num_ops;
   CHECK(input_openxr_autoconfigure(s) && !input_openxr_autoconfigure(5)
         && t_num_ops == calls + 1
         && t_op_is(calls, true, s, INPUT_OPENXR_JOYPAD_COMBINED),
         "the device's profile was not looked up again as the headset's");

   t_hooks.session_destroying(t_hooks.user);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(input_openxr_joypad(&t_joypad) == &t_joypad,
         "the driver is wrapped after the session");
   t_forget_devices();
}

int main(void)
{
   t_forget_devices();
   test_before_session();
   test_session_made();
   test_combined();
   test_separate();
   test_hotkeys();
   test_nothing_held();
   test_start_menu();
   test_laser();
   test_hands();
   test_rumble();
   test_session_gone();
   test_session_failed();
   test_palm();
   test_devices();
   test_device_taken();
   test_joypad_wrapped();
   test_joints();
   test_session_restart_trigger();
   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("PASS openxr_input_test\n");
   return 0;
}
