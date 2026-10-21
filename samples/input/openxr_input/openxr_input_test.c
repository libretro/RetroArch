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
static char     t_actions[256][XR_MAX_ACTION_SET_NAME_SIZE + XR_MAX_ACTION_NAME_SIZE];
static unsigned t_action_set[256];
static unsigned t_num_actions;
static XRAPI_ATTR XrResult XRAPI_CALL t_create_action(XrActionSet s,
      const XrActionCreateInfo *ci, XrAction *a)
{
   unsigned set = t_set_index(s);
   if (t_num_actions >= 256 || set >= t_num_sets)
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

/* Profiles suggested; each binding as profile, action and path. A
 * runtime that refuses edit mode's bindings refuses any suggestion that
 * holds one: for every profile, or for t_refuse_edit_for alone. */
static unsigned    t_suggested;
static bool        t_refuse_edit;
static const char *t_refuse_edit_for;
static char     t_bound[256][3][XR_MAX_PATH_LENGTH];
static unsigned t_num_bound;
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
   if (     t_refuse_edit
         || (t_refuse_edit_for
            && !strcmp(t_path(sb->interactionProfile), t_refuse_edit_for)))
      for (k = 0; k < sb->countSuggestedBindings; k++)
         if (!strncmp(t_actions[t_action_index(
                     sb->suggestedBindings[k].action)], "edit/", 5))
            return XR_ERROR_PATH_UNSUPPORTED;
   t_suggested++;
   for (k = 0; k < sb->countSuggestedBindings && t_num_bound < 256; k++)
   {
      strlcpy(t_bound[t_num_bound][0], t_path(sb->interactionProfile),
            sizeof(t_bound[0][0]));
      strlcpy(t_bound[t_num_bound][1],
            t_actions[t_action_index(sb->suggestedBindings[k].action)],
            sizeof(t_bound[0][1]));
      strlcpy(t_bound[t_num_bound][2],
            t_path(sb->suggestedBindings[k].binding),
            sizeof(t_bound[0][2]));
      t_num_bound++;
   }
   return XR_SUCCESS;
}
static bool t_binds(const char *profile, const char *action,
      const char *path)
{
   unsigned k;
   for (k = 0; k < t_num_bound; k++)
      if (     !strcmp(t_bound[k][0], profile)
            && !strcmp(t_bound[k][1], action)
            && (!path || !strcmp(t_bound[k][2], path)))
         return true;
   return false;
}
static bool t_touch_binds(const char *action, const char *path)
{
   return t_binds("/interaction_profiles/oculus/touch_controller",
         action, path);
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
/* Actions a binding leaves out, by their names' start ("edit/": a
 * user's binding saved before edit mode's actions existed). */
static const char *t_unbound;
/* An action's held value for the hand asked, only while its set is
 * synced and it is bound. */
static const t_held_t *t_find(const XrActionStateGetInfo *gi, bool *active)
{
   unsigned k;
   unsigned a = t_action_index(gi->action);
   int hand   = t_hand(gi->subactionPath);
   *active    = a < t_num_actions && t_synced[t_action_set[a]]
      && !(t_unbound && !strncmp(t_actions[a], t_unbound,
               strlen(t_unbound)));
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

/* Each hand's aim, by hand; the head, when a case tracks it; and the
 * space the last locate was in. */
#define T_VIEW ((XrSpace)(uintptr_t)0x42)
static bool    t_aim_tracked[3];
static XrPosef t_aim[3];
static bool    t_head_tracked;
static XrPosef t_head = { { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f } };
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
   if (     (hand && t_aim_tracked[hand])
         || (space == T_VIEW && t_head_tracked))
   {
      loc->pose          = hand ? t_aim[hand] : t_head;
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
/* Each hand's controller's profile, by hand: Touch unless a case says. */
static const char *t_current[3];
static XRAPI_ATTR XrResult XRAPI_CALL t_get_profile(XrSession s,
      XrPath user, XrInteractionProfileState *st)
{
   const char *profile = t_current[t_hand(user)];
   return t_string_to_path(T_INSTANCE, profile ? profile
         : "/interaction_profiles/oculus/touch_controller",
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

/* Each hand's haptics: the last amplitude, 0 once stopped, and how
 * many were applied. */
static float    t_amp[3];
static unsigned t_applies[3];
static XRAPI_ATTR XrResult XRAPI_CALL t_apply(XrSession s,
      const XrHapticActionInfo *hi, const XrHapticBaseHeader *h)
{
   t_amp[t_hand(hi->subactionPath)] =
      ((const XrHapticVibration*)h)->amplitude;
   t_applies[t_hand(hi->subactionPath)]++;
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

/* The video state, the frontend's pause and notices edit mode uses. A
 * pause asked from inside a poll, which can be inside the core's frame,
 * or before video was told, is counted. */
static bool     t_editing;
static unsigned t_edit_seq;
void video_driver_headset_edit(bool editing, unsigned seq)
{
   t_editing  = editing;
   t_edit_seq = seq;
}
/* The settings edit mode reads and writes, kept as configuration.c
 * keeps them: a write normalised and marked modified, Visible left
 * alone; NULL resets a screen as Reset Screen does, or puts the menu
 * back in front of screen 1. */
static video_screen_xr_t t_xr[RETRO_VIDEO_VIEWS_MAX];
static video_screen_xr_t t_menu_xr;
static bool              t_menu_placed;
static bool              t_modified;
void config_get_video_screen_xr(unsigned screen, video_screen_xr_t *out)
{
   *out = t_xr[screen];
}
bool config_get_video_menu_xr(video_screen_xr_t *out)
{
   *out = t_menu_xr;
   return t_menu_placed;
}
void config_set_video_screen_xr(unsigned screen,
      const video_screen_xr_t *values)
{
   video_screen_xr_t s;
   if (screen >= RETRO_VIDEO_VIEWS_MAX)
      return;
   video_views_xr_reset(&s, 1);
   if (values)
   {
      s         = *values;
      s.visible = t_xr[screen].visible;
      video_views_xr_normalize(&s);
   }
   t_xr[screen] = s;
   t_modified   = true;
}
void config_set_video_menu_xr(const video_screen_xr_t *values)
{
   video_views_xr_reset(&t_menu_xr, 1);
   t_menu_placed = values != NULL;
   if (values)
   {
      t_menu_xr = *values;
      video_views_xr_normalize(&t_menu_xr);
   }
   t_modified = true;
}
static bool     t_in_poll;
static unsigned t_pauses, t_unpauses, t_bad_pauses;
/* A pause the frontend refuses (hardcore, netplay) when a case says. */
static bool     t_pause_refused;
void input_driver_platform_request(enum input_platform_request req)
{
   if (req == INPUT_PLATFORM_PAUSE)
   {
      t_pauses++;
      if (t_in_poll || !t_editing)
         t_bad_pauses++;
      if (!t_pause_refused)
         t_run_flags |= RUNLOOP_FLAG_PAUSED;
   }
   else if (req == INPUT_PLATFORM_UNPAUSE)
   {
      t_unpauses++;
      t_run_flags &= ~RUNLOOP_FLAG_PAUSED;
   }
}
static char     t_notice[256];
static unsigned t_notices;
void runloop_msg_queue_push(const char *msg, size_t len,
      unsigned prio, unsigned duration, bool flush, char *title,
      enum message_queue_icon icon, enum message_queue_category category)
{
   strlcpy(t_notice, msg, sizeof(t_notice));
   t_notices++;
}
const char *msg_hash_to_str(enum msg_hash_enums msg)
{
   switch (msg)
   {
      case MSG_OPENXR_EDIT_ON:
         return "edit on";
      case MSG_OPENXR_EDIT_OFF:
         return "edit off";
      case MSG_OPENXR_EDIT_UNSAVED:
         return "edit unsaved";
      case MSG_OPENXR_EDIT_UNAVAILABLE:
         return "edit unavailable";
      case MSG_OPENXR_EDIT_NO_CONTENT:
         return "edit no content";
      default:
         break;
   }
   return "";
}

static openxr_session_t     t_xr_session;
static openxr_frame_t       t_xr_frame;
static bool                 t_focused = true;
static bool                 t_ended;
static XrTime               t_time    = 1000000000;
static video_xr_quad_set_t  t_quads;
static openxr_input_hooks_t t_hooks;
static unsigned             t_under;
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
/* The same quads from an identity anchor, planned at video's edit count
 * or, while a case holds them back (the drawing thread behind), at the
 * count it holds them at; and the screens' pose: the identity unless a
 * case turns it, in the shown quads' space. */
static video_xr_pose_t t_screens = { { 0.0f, 0.0f, 0.0f, 1.0f },
   { 0.0f, 0.0f, 0.0f } };
static bool     t_quads_behind;
static unsigned t_quads_seq;
void openxr_frame_get_quads(openxr_frame_t *f, video_xr_quad_set_t *out)
{
   unsigned s;
   *out = t_quads;
   for (s = 0; s < VIDEO_XR_MAX_SLOTS; s++)
      out->slots[s].edit_seq = t_quads_behind ? t_quads_seq : t_edit_seq;
}
void openxr_frame_get_screens(openxr_frame_t *f, video_xr_pose_t *pose,
      XrSpace *space)
{
   *pose  = t_screens;
   *space = t_space ? t_space : t_xr_session.local_space;
}
/* The dots' image: none unless a case gives one. */
static bool t_cursor;
XrSwapchain openxr_frame_cursor(const openxr_frame_t *f)
{
   return t_cursor ? (XrSwapchain)(uintptr_t)0x51 : XR_NULL_HANDLE;
}
/* What the hands hold, as last given to the frame loop. */
static openxr_grabs_t t_grabs;
static unsigned       t_grabs_published;
void openxr_frame_publish_grabs(openxr_frame_t *f, const openxr_grabs_t *g)
{
   t_grabs = *g;
   t_grabs_published++;
}
/* Edit Smoothing's aims as the frame loop last published them, and the
 * recenters it has seen. */
static openxr_aims_t t_aims;
static unsigned t_aims_published;
static int t_recenters;
void openxr_frame_publish_aims(openxr_frame_t *f, const openxr_aims_t *a)
{
   (void)f;
   t_aims = *a;
   t_aims_published++;
}
void openxr_frame_get_aims(openxr_frame_t *f, openxr_aims_t *out)
{
   (void)f;
   *out = t_aims;
}
int openxr_frame_recenters(openxr_frame_t *f)
{
   (void)f;
   return t_recenters;
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
   t_xr_session.view_space  = T_VIEW;
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

/* The Laser Beam setting each poll passes. */
static unsigned t_beam = VIDEO_OPENXR_LASER_BEAM_EDIT;

static void t_poll(unsigned controllers, unsigned laser)
{
   t_in_poll = true;
   input_openxr_poll(4, controllers, laser, t_beam, 0.5f);
   t_in_poll = false;
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
      && !t_btn(0, RARCH_HEADSET_VIEW_TOGGLE)
      && !t_btn(0, RARCH_HEADSET_EDIT_SCREENS);
}

/* ---- the tests ---- */
static void test_before_session(void)
{
   input_openxr_register();
   CHECK(t_hooks.session_created && t_hooks.session_destroying
         && t_hooks.frame_layers && t_hooks.locate_hands && t_hooks.aim,
         "the hooks were not registered");
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_AUTO);
   CHECK(t_nothing_held(), "something is held before there is a session");
   CHECK(!input_openxr_can_rumble(), "rumble before there is a session");
}

static void test_session_made(void)
{
   t_reset_log();
   t_session(false);
   CHECK(t_num_sets == 4 && t_num_actions == 47,
         "%u sets and %u actions made, want 4 and 47", t_num_sets,
         t_num_actions);
   CHECK(t_suggested == 4,
         "%u profiles suggested without the Frame's extension, want 4",
         t_suggested);
   CHECK(t_attached == 4 && t_spaces_made == 4,
         "%u sets attached and %u aim and grip spaces", t_attached,
         t_spaces_made);
   CHECK(t_touch_binds("pointer/grip", "/user/hand/right/input/grip/pose")
         && !t_touch_binds("pointer/palm", NULL),
         "Touch's grip is not bound, or its palm is without the extension");
   CHECK(t_touch_binds("combined/start_menu",
            "/user/hand/left/input/menu/click")
         && !t_touch_binds("combined/start", NULL),
         "Touch's menu button is not Start and the menu");
   CHECK(t_touch_binds("edit/grab", "/user/hand/left/input/squeeze/value")
         && t_touch_binds("edit/grab", "/user/hand/right/input/squeeze/value")
         && t_touch_binds("edit/tilt", "/user/hand/right/input/trigger/value")
         && t_touch_binds("edit/stick", "/user/hand/left/input/thumbstick")
         && t_touch_binds("edit/reset", "/user/hand/left/input/x/click")
         && t_touch_binds("edit/reset", "/user/hand/right/input/a/click")
         && t_touch_binds("edit/menu", "/user/hand/left/input/menu/click"),
         "Touch's edit bindings are not grip, trigger, stick, X/A, menu");
   CHECK(!t_touch_binds("combined/edit_screens", NULL)
         && !t_touch_binds("separate/edit_screens", NULL)
         && !t_touch_binds("edit/edit_screens", NULL),
         "Edit Screens suggested: the user binds it");
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
   t_hold("combined/edit_screens", 0, 1.0f, 0.0f);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_btn(3, RARCH_HEADSET_RECENTER)
         && t_btn(3, RARCH_LASER_POINTER_TOGGLE)
         && t_btn(3, RARCH_HEADSET_VIEW_TOGGLE)
         && t_btn(3, RARCH_MENU_TOGGLE)
         && t_btn(3, RARCH_HEADSET_EDIT_SCREENS),
         "the hotkeys do not answer on any port: %d %d %d %d %d",
         t_btn(3, RARCH_HEADSET_RECENTER),
         t_btn(3, RARCH_LASER_POINTER_TOGGLE),
         t_btn(3, RARCH_HEADSET_VIEW_TOGGLE), t_btn(3, RARCH_MENU_TOGGLE),
         t_btn(3, RARCH_HEADSET_EDIT_SCREENS));

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
   /* So does a config's value past the choices: Auto, not edit mode's. */
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_LAST);
   CHECK(!input_openxr_pointer(0, RETRO_DEVICE_LIGHTGUN, 0,
            RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X, &x),
         "a Laser Pointer past the choices took a whole frame's gun");

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
   CHECK(t_sets_destroyed == 4 && t_spaces_destroyed == 4,
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
   CHECK(     t_num_sets - sets == 4
         && t_sets_destroyed - sets_gone == 4
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
   t_num_bound   = 0;
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

/* Snap to Screens, as edit mode starts; off unless a case says. */
static bool t_snap;
static unsigned t_smooth;

/* As the frontend's command does it, between frames, with the menu
 * closed. */
static void t_edit_on(bool pause)
{
   if (input_openxr_edit_toggle())
      input_openxr_edit_begin(pause, t_snap, t_smooth);
}

/* Edit Screens: the frontend's command starts it at once, between
 * frames, and the headset's action only raises that command; its own
 * set replaces the pads' and their buttons reach nothing; the laser
 * points at every item whatever its setting; the menu, the command
 * again, a video reinit and the session's end stop it. Without content
 * or with the session ended, nothing starts. */
static void test_edit_mode(void)
{
   int config;
   const XrCompositionLayerBaseHeader *layers[4];
   t_forget_devices();
   t_fresh_session();
   t_menu_open = false;
   t_reset_log();
   t_notices = 0;
   t_let_go();
   t_hold("combined/edit_screens", 0, 1.0f, 0.0f);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_btn(3, RARCH_HEADSET_EDIT_SCREENS) && !t_editing,
         "the action is no hotkey, or started edit mode in the poll");
   t_let_go();
   CHECK(input_openxr_edit_toggle(), "a start was refused");
   CHECK(!t_editing, "the toggle alone started it");
   input_openxr_edit_begin(false, false, 0);
   CHECK(t_editing, "did not start at once");
   t_hold("combined/a", 0, 1.0f, 0.0f);
   t_hold("edit/edit_screens", 0, 1.0f, 0.0f);
   t_hold("edit/menu", 0, 1.0f, 0.0f);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_editing && t_synced[3] && t_synced[2] && !t_synced[0]
         && !t_synced[1], "synced %d %d %d %d, want the edit and pointer sets",
         t_synced[0], t_synced[1], t_synced[2], t_synced[3]);
   CHECK(!t_btn(0, RETRO_DEVICE_ID_JOYPAD_A),
         "a pad button reached the core while editing");
   CHECK(t_btn(3, RARCH_HEADSET_EDIT_SCREENS),
         "the edit set's Edit Screens is no hotkey");
   CHECK(t_btn(3, RARCH_MENU_TOGGLE),
         "the edit set's menu button opens no menu");
   config = retro_atomic_load_acquire_int(&input_openxr_pointer_config);
   CHECK((config & INPUT_OPENXR_LASER_MASK) == VIDEO_XR_LIVE_EDIT,
         "the laser is %d while editing", config);
   CHECK(!strcmp(t_notice, "edit on")
         && t_logged("[OpenXR] Edit Screens on.\n") == 1,
         "notice %s, log: %s", t_notice, t_log);
   /* A whole frame, which Off leaves alone: a dot from each hand. */
   memset(&t_quads, 0, sizeof(t_quads));
   t_quads.num_quads = 1;
   video_xr_pose_identity(&t_quads.quads[0].pose);
   t_quads.quads[0].pose.position.z = -1.8f;
   t_quads.quads[0].width           = 1.6f;
   t_quads.quads[0].height          = 0.96f;
   t_quads.quads[0].kind            = VIDEO_XR_QUAD_FRAME;
   memset(t_aim, 0, sizeof(t_aim));
   t_aim[1].orientation.w = 1.0f;
   t_aim[2].orientation.w = 1.0f;
   t_aim_tracked[1]       = true;
   t_aim_tracked[2]       = true;
   t_cursor               = true;
   CHECK(t_hooks.frame_layers(t_hooks.user, t_time, &t_quads,
            t_xr_session.local_space, layers, 4, &t_under) == 3,
         "both hands do not point while editing, after its outline");

   /* The menu opens: off, and the laser is the setting's again. */
   t_let_go();
   t_menu_open = true;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   config = retro_atomic_load_acquire_int(&input_openxr_pointer_config);
   CHECK(!t_editing && t_synced[0] && !t_synced[3],
         "still editing with the menu open");
   CHECK((config & INPUT_OPENXR_LASER_MASK) == VIDEO_OPENXR_LASER_OFF,
         "the laser is %d after editing", config);
   CHECK(!strcmp(t_notice, "edit off"), "notice %s", t_notice);
   CHECK(t_hooks.frame_layers(t_hooks.user, t_time, &t_quads,
            t_xr_session.local_space, layers, 4, &t_under) == 0,
         "the laser points after editing with the setting Off");
   t_cursor         = false;
   memset(&t_quads, 0, sizeof(t_quads));
   t_aim_tracked[1] = false;
   t_aim_tracked[2] = false;
   t_menu_open = false;

   /* The menu would not close, which only happens without content: the
    * desktop is told, and nothing starts. */
   t_menu_open = true;
   t_notices   = 0;
   CHECK(input_openxr_edit_toggle(), "a start was refused with the menu open");
   input_openxr_edit_begin(false, false, 0);
   CHECK(!t_editing && t_notices == 1 && !strcmp(t_notice, "edit no content"),
         "%u notices, the last %s, editing %d", t_notices, t_notice,
         (int)t_editing);
   t_menu_open = false;

   /* An ended session cannot start it. */
   t_ended   = true;
   t_notices = 0;
   CHECK(!input_openxr_edit_toggle() && !t_editing && t_notices == 1
         && !strcmp(t_notice, "edit unavailable"),
         "an ended session: %u notices, the last %s", t_notices, t_notice);
   t_ended   = false;

   /* The command while on: off. */
   t_edit_on(false);
   CHECK(t_editing && !input_openxr_edit_toggle() && !t_editing,
         "the command did not stop it");

   /* A video reinit ends it. */
   t_edit_on(false);
   input_openxr_edit_end();
   CHECK(!t_editing, "a video reinit left it on");

   /* So does the session's end. */
   t_edit_on(false);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_editing, "fixture: not editing");
   t_hooks.session_destroying(t_hooks.user);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(!t_editing, "the session's end left it on");
   t_forget_devices();
}

/* The pads' triggers go unread while editing: one held as the menu opens
 * and ends it is no pull on the menu. */
static void test_edit_trigger_held(void)
{
   float u      = 0.0f;
   float v      = 0.0f;
   bool pressed = true;
   t_forget_devices();
   t_fresh_session();
   t_menu_open = false;
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
   t_let_go();
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_AUTO);
   t_edit_on(false);
   t_hold("combined/r2", 0, 1.0f, 0.0f);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_AUTO);
   CHECK(t_editing, "fixture: not editing");
   t_menu_open = true;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_AUTO);
   CHECK(!t_editing && input_openxr_menu_pointer(&u, &v, &pressed)
         && !pressed, "a trigger held through editing pressed the menu");
   t_let_go();
   t_menu_open = false;
   memset(&t_quads, 0, sizeof(t_quads));
   t_aim_tracked[2] = false;
   t_hooks.session_destroying(t_hooks.user);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_AUTO);
   t_forget_devices();
}

/* A runtime that refuses edit mode's bindings keeps the rest of each
 * profile; edit mode is then unavailable: told every time, logged once. */
static void test_edit_unavailable(void)
{
   t_forget_devices();
   t_reset_log();
   t_refuse_edit = true;
   t_fresh_session();
   t_refuse_edit = false;
   CHECK(t_touch_binds("combined/a", NULL)
         && !t_touch_binds("edit/grab", NULL),
         "Touch lost its game bindings or kept edit mode's");
   CHECK(t_logged("[OpenXR] Edit Screens bindings refused for ") == 4,
         "log: %s", t_log);
   t_reset_log();
   t_notices = 0;
   CHECK(!input_openxr_edit_toggle() && !input_openxr_edit_toggle(),
         "edit mode started without its bindings");
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(!t_editing && !t_synced[3], "the edit set was synced");
   CHECK(t_notices == 2 && !strcmp(t_notice, "edit unavailable"),
         "%u notices, the last %s", t_notices, t_notice);
   CHECK(t_logged("[OpenXR] Edit Screens unavailable") == 1,
         "log: %s", t_log);
   t_hooks.session_destroying(t_hooks.user);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_forget_devices();
}

/* Refused for Touch alone: a hand on a Touch controller cannot start
 * edit mode, which it could not leave; Index controllers can. */
static void test_edit_profile_refused(void)
{
   t_forget_devices();
   t_reset_log();
   t_refuse_edit_for = "/interaction_profiles/oculus/touch_controller";
   t_fresh_session();
   t_refuse_edit_for = NULL;
   CHECK(t_logged("[OpenXR] Edit Screens bindings refused for ") == 1
         && t_binds("/interaction_profiles/valve/index_controller",
            "edit/grab", NULL),
         "log: %s", t_log);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_current[1] = "/interaction_profiles/valve/index_controller";
   t_current[2] = "/interaction_profiles/oculus/touch_controller";
   t_notices    = 0;
   CHECK(!input_openxr_edit_toggle() && !t_editing && t_notices == 1
         && !strcmp(t_notice, "edit unavailable"),
         "a Touch controller: editing %d, %u notices, the last %s",
         (int)t_editing, t_notices, t_notice);
   t_current[2] = "/interaction_profiles/valve/index_controller";
   t_edit_on(false);
   CHECK(t_editing, "Index controllers could not start it");
   input_openxr_edit_toggle();
   t_current[1] = NULL;
   t_current[2] = NULL;
   t_hooks.session_destroying(t_hooks.user);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_forget_devices();
}

/* A pause the frontend refuses (hardcore allows one only so often):
 * edit mode runs on and claims no pause, so leaving it resumes nothing
 * the user pauses meanwhile. */
static void test_edit_pause_refused(void)
{
   t_forget_devices();
   t_fresh_session();
   t_run_flags     = RUNLOOP_FLAG_CORE_RUNNING;
   t_pauses        = 0;
   t_unpauses      = 0;
   t_pause_refused = true;
   t_edit_on(true);
   t_pause_refused = false;
   CHECK(t_editing && t_pauses == 1 && !(t_run_flags & RUNLOOP_FLAG_PAUSED)
         && !input_openxr_edit.paused,
         "a refused pause: editing %d, %u pauses, claimed %d",
         (int)t_editing, t_pauses, (int)input_openxr_edit.paused);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_run_flags |= RUNLOOP_FLAG_PAUSED;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   input_openxr_edit_toggle();
   CHECK(!t_editing && t_unpauses == 0 && (t_run_flags & RUNLOOP_FLAG_PAUSED),
         "the user's pause undone: %u resumes", t_unpauses);
   t_run_flags = RUNLOOP_FLAG_CORE_RUNNING;
   t_hooks.session_destroying(t_hooks.user);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_forget_devices();
}

/* Edit mode pauses; the user resumes, then pauses again: that pause is
 * the user's, and leaving edit mode keeps it. */
static void test_edit_repause(void)
{
   t_forget_devices();
   t_fresh_session();
   t_run_flags = RUNLOOP_FLAG_CORE_RUNNING;
   t_pauses    = 0;
   t_unpauses  = 0;
   t_edit_on(true);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_editing && t_pauses == 1 && input_openxr_edit.paused,
         "fixture: edit mode did not pause");
   t_run_flags &= ~RUNLOOP_FLAG_PAUSED;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_run_flags |= RUNLOOP_FLAG_PAUSED;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   input_openxr_edit_toggle();
   CHECK(!t_editing && t_unpauses == 0 && (t_run_flags & RUNLOOP_FLAG_PAUSED),
         "the user's pause undone: %u resumes", t_unpauses);
   t_run_flags = RUNLOOP_FLAG_CORE_RUNNING;
   t_hooks.session_destroying(t_hooks.user);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_forget_devices();
}

/* A runtime that takes edit mode's bindings and binds none of them, as
 * for a binding the user saved before they existed: nothing could grab
 * or leave. A few polls in a row with neither grab nor the menu bound
 * end edit mode, resume what it paused and say it is unavailable,
 * logged once; fewer do not, nor does the menu bound alone. */
static void test_edit_unbound(void)
{
   unsigned i;
   t_forget_devices();
   t_fresh_session();
   t_reset_log();
   t_run_flags = RUNLOOP_FLAG_CORE_RUNNING;
   t_pauses    = 0;
   t_unpauses  = 0;
   t_edit_on(true);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_editing && (t_run_flags & RUNLOOP_FLAG_PAUSED),
         "fixture: not editing, paused");
   t_unbound = "edit/grab";
   for (i = 0; i <= INPUT_OPENXR_EDIT_UNBOUND; i++)
      t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_editing, "ended with its menu bound");
   t_unbound = "edit/";
   for (i = 1; i < INPUT_OPENXR_EDIT_UNBOUND; i++)
      t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_unbound = NULL;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_unbound = "edit/";
   for (i = 1; i < INPUT_OPENXR_EDIT_UNBOUND; i++)
      t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_editing, "ended before %u unbound polls in a row",
         INPUT_OPENXR_EDIT_UNBOUND);
   t_notices = 0;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(!t_editing && t_unpauses == 1
         && !(t_run_flags & RUNLOOP_FLAG_PAUSED),
         "unbound: editing %d, %u resumes", (int)t_editing, t_unpauses);
   CHECK(t_notices && !strcmp(t_notice, "edit unavailable"),
         "%u notices, the last %s", t_notices, t_notice);
   CHECK((retro_atomic_load_acquire_int(&input_openxr_pointer_config)
            & INPUT_OPENXR_LASER_MASK) == VIDEO_OPENXR_LASER_OFF,
         "the laser kept edit mode's");
   /* Again: told, not logged again. */
   t_edit_on(false);
   for (i = 0; i < INPUT_OPENXR_EDIT_UNBOUND; i++)
      t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(!t_editing && !strcmp(t_notice, "edit unavailable"),
         "a second start: editing %d, notice %s", (int)t_editing, t_notice);
   CHECK(t_logged("[OpenXR] Edit Screens unavailable") == 1,
         "log: %s", t_log);
   t_unbound   = NULL;
   t_run_flags = RUNLOOP_FLAG_CORE_RUNNING;
   t_hooks.session_destroying(t_hooks.user);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_forget_devices();
}

/* A profile with more bindings than one suggestion holds keeps its
 * game's, edit mode's refused for it: a hand on it cannot start it. */
static void test_edit_oversized(void)
{
   static input_openxr_bind_t game[INPUT_OPENXR_MAX_BINDS - 1];
   static const input_openxr_bind_t edit[] = {
      IXB_BOTH(IXA_E_GRAB, "squeeze/value") };
   input_openxr_profile_t p;
   unsigned i;
   const char *path = "/interaction_profiles/test/oversized";
   t_forget_devices();
   t_fresh_session();
   for (i = 0; i < ARRAY_SIZE(game); i++)
   {
      game[i].action = IXA_C_A;
      game[i].path   = "/user/hand/right/input/a/click";
   }
   memset(&p, 0, sizeof(p));
   p.path       = path;
   p.binds      = game;
   p.edit       = edit;
   p.count      = ARRAY_SIZE(game);
   p.edit_count = ARRAY_SIZE(edit);
   t_reset_log();
   CHECK(!input_openxr_suggest(&input_openxr_st, &p),
         "edit mode's bindings taken past the limit");
   CHECK(t_binds(path, "combined/a", NULL) && !t_binds(path, "edit/grab",
            NULL), "the game's not suggested, or edit mode's were");
   CHECK(t_logged("[OpenXR] Edit Screens bindings refused for "
            "/interaction_profiles/test/oversized") == 1
         && t_logged("[OpenXR] Bindings suggested for "
            "/interaction_profiles/test/oversized") == 1, "log: %s", t_log);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_current[2] = path;
   CHECK(!input_openxr_edit_toggle() && !t_editing,
         "a hand on it started edit mode");
   t_current[2] = NULL;
   t_hooks.session_destroying(t_hooks.user);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_forget_devices();
}

/* While Editing Screens: Pause pauses the content, once video knows and
 * never from inside a poll, which can be inside the core's frame; it
 * resumes only what it paused. */
static void test_edit_pause(void)
{
   t_forget_devices();
   t_fresh_session();
   t_run_flags  = RUNLOOP_FLAG_CORE_RUNNING;
   t_pauses     = 0;
   t_unpauses   = 0;
   t_bad_pauses = 0;
   t_edit_on(true);
   CHECK(t_editing && t_pauses == 1 && (t_run_flags & RUNLOOP_FLAG_PAUSED),
         "entering did not pause: %u", t_pauses);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   input_openxr_edit_toggle();
   CHECK(!t_editing && t_unpauses == 1
         && !(t_run_flags & RUNLOOP_FLAG_PAUSED), "leaving did not resume");
   /* Paused already: left paused. */
   t_run_flags |= RUNLOOP_FLAG_PAUSED;
   t_edit_on(true);
   input_openxr_edit_toggle();
   CHECK(t_pauses == 1 && t_unpauses == 1
         && (t_run_flags & RUNLOOP_FLAG_PAUSED),
         "a pause edit mode did not make was undone");
   /* Resumed by hand while editing: nothing to resume. */
   t_run_flags = RUNLOOP_FLAG_CORE_RUNNING;
   t_edit_on(true);
   t_run_flags &= ~RUNLOOP_FLAG_PAUSED;
   input_openxr_edit_toggle();
   CHECK(t_pauses == 2 && t_unpauses == 1, "%u pauses %u resumes",
         t_pauses, t_unpauses);
   /* Ended in a poll, by the menu opening: resumed. */
   t_run_flags = RUNLOOP_FLAG_CORE_RUNNING;
   t_edit_on(true);
   t_menu_open = true;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_menu_open = false;
   CHECK(!t_editing && t_pauses == 3 && t_unpauses == 2
         && !(t_run_flags & RUNLOOP_FLAG_PAUSED),
         "the menu ended it: %u pauses %u resumes", t_pauses, t_unpauses);
   /* Keep Running: never paused. */
   t_edit_on(false);
   CHECK(t_pauses == 3 && !(t_run_flags & RUNLOOP_FLAG_PAUSED),
         "Keep Running paused");
   input_openxr_edit_toggle();
   /* The headset's action held through polls only raises the command. */
   t_let_go();
   t_hold("combined/edit_screens", 0, 1.0f, 0.0f);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_let_go();
   CHECK(!t_editing && t_pauses == 3, "the action started it in a poll");
   CHECK(t_bad_pauses == 0,
         "%u pauses from inside a poll or before video knew", t_bad_pauses);
   t_run_flags = RUNLOOP_FLAG_CORE_RUNNING;
   t_hooks.session_destroying(t_hooks.user);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_forget_devices();
}

static bool t_near(float a, float b)
{
   return fabs((double)a - (double)b) < 1e-3;
}

/* An outline's size round an item s metres across. */
static float t_outline(float s)
{
   return (s + 2.0f * VIDEO_XR_OUTLINE_GAP) / (1.0f - 2.0f
         * (VIDEO_XR_OUTLINE_EDGE + 0.5f) / VIDEO_XR_OUTLINE_DIM);
}

/* The menu's handle bar's middle, for a menu h metres tall centred at
 * y 0: 5 mm below its outline. */
static float t_handle_y(float h)
{
   return -(t_outline(h) + VIDEO_XR_HANDLE_HEIGHT) * 0.5f - 0.005f;
}

/* Screen 1 1.8 m ahead and the menu placed 2 m to its right, from an
 * identity anchor; both hands at the eyes, looking ahead. */
static void t_edit_fixture(void)
{
   memset(&t_quads, 0, sizeof(t_quads));
   t_quads.num_quads = 2;
   video_xr_pose_identity(&t_quads.quads[0].pose);
   t_quads.quads[0].pose.position.z = -1.8f;
   t_quads.quads[0].width           = 1.6f;
   t_quads.quads[0].height          = 0.96f;
   t_quads.quads[0].kind            = VIDEO_XR_QUAD_SCREEN;
   t_quads.slots[0].base.z          = -1.8f;
   video_xr_pose_identity(&t_quads.quads[1].pose);
   t_quads.quads[1].pose.position.x = 2.0f;
   t_quads.quads[1].pose.position.z = -1.7f;
   t_quads.quads[1].width           = 1.0f;
   t_quads.quads[1].height          = 0.6f;
   t_quads.quads[1].kind            = VIDEO_XR_QUAD_MENU;
   t_quads.quads[1].slot            = VIDEO_XR_MENU_SLOT;
   t_quads.slots[VIDEO_XR_MENU_SLOT].base.z = -1.7f;
   memset(t_aim, 0, sizeof(t_aim));
   t_aim[1].orientation.w = 1.0f;
   t_aim[2].orientation.w = 1.0f;
   t_aim_tracked[1]       = true;
   t_aim_tracked[2]       = true;
   video_views_xr_reset(t_xr, RETRO_VIDEO_VIEWS_MAX);
   video_views_xr_reset(&t_menu_xr, 1);
   t_menu_xr.x            = 2.0f;
   t_menu_placed          = true;
   t_modified             = false;
   video_xr_pose_identity(&t_screens);
   t_space                = XR_NULL_HANDLE;
   t_menu_open            = false;
   t_let_go();
}

/* A fresh session in edit mode, started as the frontend's command
 * starts it. */
static void t_edit_start(void)
{
   t_forget_devices();
   t_fresh_session();
   t_edit_fixture();
   t_edit_on(false);
   CHECK(t_editing, "fixture: not editing");
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
}

static void t_edit_end(void)
{
   t_let_go();
   if (t_editing)
      input_openxr_edit_toggle();
   t_hooks.session_destroying(t_hooks.user);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   memset(&t_quads, 0, sizeof(t_quads));
   t_aim_tracked[1] = t_aim_tracked[2] = false;
   video_xr_pose_identity(&t_screens);
   t_space          = XR_NULL_HANDLE;
   t_forget_devices();
}

static void t_edit_poll(void)
{
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
}

/* A grip on a screen grabs it; it follows the hand, level; the trigger
 * lets its roll follow; the stick pushes and resizes, by how far past
 * its deadzone; letting go leaves it there, counted for the frame
 * snapshot. */
static void test_edit_grab(void)
{
   const video_xr_grab_t *g = &input_openxr_edit.hand[1].g;
   unsigned seq;
   t_edit_start();
   seq = t_edit_seq;
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(g->state == VIDEO_XR_GRAB_HELD && g->slot == 0,
         "the grip grabbed %u in state %u", g->slot, g->state);
   CHECK(t_near(t_xr[0].x, 0.0f) && t_near(t_xr[0].z, 0.0f),
         "a grab moved the screen");

   t_aim[2].position.x = 0.2f;
   t_edit_poll();
   CHECK(t_near(t_xr[0].x, 0.2f) && t_near(t_xr[0].y, 0.0f)
         && t_near(t_xr[0].z, 0.0f) && t_near(t_xr[0].yaw, 0.0f),
         "moved 20 cm: %.3f %.3f %.3f", t_xr[0].x, t_xr[0].y, t_xr[0].z);

   /* A 30 degree roll of the wrist: level. */
   t_aim[2].orientation.z = (float)sin(M_PI / 12.0);
   t_aim[2].orientation.w = (float)cos(M_PI / 12.0);
   t_edit_poll();
   CHECK(t_near(t_xr[0].roll, 0.0f), "rolled %.2f while level",
         t_xr[0].roll);
   /* The trigger: no jump, then the roll follows. */
   t_hold("edit/tilt", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(t_near(t_xr[0].roll, 0.0f), "the trigger rolled it at once");
   t_aim[2].orientation.z = 0.5f;
   t_aim[2].orientation.w = 0.8660254f;
   t_edit_poll();
   CHECK(t_near(t_xr[0].roll, 30.0f), "rolled %.2f tilting", t_xr[0].roll);
   /* Let go of the trigger: the roll stays. */
   t_let_go();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_aim[2].orientation.z = 0.0f;
   t_aim[2].orientation.w = 1.0f;
   t_edit_poll();
   CHECK(t_near(t_xr[0].roll, 30.0f), "the roll went back to %.2f",
         t_xr[0].roll);

   /* The stick for 0.1 s: up pushes 15 cm, right grows 2^0.1 times. */
   t_hold("edit/stick", 2, 0.0f, 1.0f);
   t_clock += 100000;
   t_edit_poll();
   CHECK(t_near(t_xr[0].z, -0.15f), "pushed to %.3f", t_xr[0].z);
   t_let_go();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_hold("edit/stick", 2, 1.0f, 0.0f);
   t_clock += 100000;
   t_edit_poll();
   CHECK(t_near(t_xr[0].scale, 1.07177f), "scaled to %.4f", t_xr[0].scale);
   CHECK(t_near(g->width, 1.6f * 1.07177f), "width %.3f", g->width);
   /* Inside the deadzone, nothing; half past it, half the rate. */
   t_let_go();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_hold("edit/stick", 2, 0.15f, 0.15f);
   t_clock += 100000;
   t_edit_poll();
   CHECK(t_near(t_xr[0].z, -0.15f) && t_near(t_xr[0].scale, 1.07177f),
         "the deadzone moved it to %.3f, %.4f", t_xr[0].z, t_xr[0].scale);
   t_let_go();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_hold("edit/stick", 2, 0.0f, 0.6f);
   t_clock += 100000;
   t_edit_poll();
   CHECK(t_near(t_xr[0].z, -0.225f), "half past the deadzone: %.3f",
         t_xr[0].z);
   /* A stalled poll steps no more than 0.1 s. */
   t_let_go();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_hold("edit/stick", 2, 0.0f, 1.0f);
   t_clock += 2000000;
   t_edit_poll();
   CHECK(t_near(t_xr[0].z, -0.375f), "a 2 s stall pushed to %.3f",
         t_xr[0].z);

   /* Let go: left there, the change counted. */
   t_let_go();
   t_edit_poll();
   CHECK(g->state == VIDEO_XR_GRAB_RELEASED && g->release_seq == seq + 1
         && t_edit_seq == seq + 1, "state %u seq %u/%u", g->state,
         g->release_seq, t_edit_seq);
   CHECK(t_near(t_xr[0].x, 0.2f), "letting go moved it");
   CHECK(t_modified, "not marked modified");
   input_openxr_edit_toggle();
   CHECK(!strcmp(t_notice, "edit unsaved"), "notice %s", t_notice);
   t_edit_end();
}

/* A carried screen keeps its pitch as the ray rises; the trigger lets
 * the pitch follow from where it is, and letting go keeps it. The free
 * hand's stick turns what the other hand holds, about its middle, as
 * fast as it is pushed; the holding hand's own stick still resizes;
 * with both hands holding, each stick works its own item. */
static void test_edit_turn(void)
{
   const video_xr_grab_t *r = &input_openxr_edit.hand[1].g;
   const video_xr_grab_t *l = &input_openxr_edit.hand[0].g;
   t_edit_start();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(r->state == VIDEO_XR_GRAB_HELD && r->slot == 0,
         "took slot %u in state %u", r->slot, r->state);
   /* The ray 20 degrees up: pitch held. */
   t_aim[2].orientation.x = (float)sin(M_PI / 18.0);
   t_aim[2].orientation.w = (float)cos(M_PI / 18.0);
   t_edit_poll();
   CHECK(t_near(t_xr[0].pitch, 0.0f) && t_xr[0].y > 0.5f,
         "carried up to %.3f, pitched %.2f", t_xr[0].y, t_xr[0].pitch);
   /* The trigger, then 10 degrees more: it follows from there. */
   t_hold("edit/tilt", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(t_near(t_xr[0].pitch, 0.0f), "the trigger pitched it at once");
   t_aim[2].orientation.x = (float)sin(M_PI / 12.0);
   t_aim[2].orientation.w = (float)cos(M_PI / 12.0);
   t_edit_poll();
   CHECK(t_near(t_xr[0].pitch, 10.0f), "pitched %.2f tilting",
         t_xr[0].pitch);
   /* Let go of the trigger, the ray level: the pitch stays. */
   t_let_go();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_aim[2].orientation.x = 0.0f;
   t_aim[2].orientation.w = 1.0f;
   t_edit_poll();
   CHECK(t_near(t_xr[0].pitch, 10.0f), "the pitch went back to %.2f",
         t_xr[0].pitch);
   t_let_go();
   t_edit_poll();
   t_edit_end();

   /* The left stick right for 0.1 s turns screen 1 nine degrees, in
    * place. */
   t_edit_start();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   t_hold("edit/stick", 1, 1.0f, 0.0f);
   t_clock += 100000;
   t_edit_poll();
   CHECK(t_near(t_xr[0].yaw, 9.0f) && t_near(t_xr[0].x, 0.0f)
         && t_near(t_xr[0].z, 0.0f) && t_near(t_xr[0].pitch, 0.0f),
         "turned to %.2f at %.3f,%.3f", t_xr[0].yaw, t_xr[0].x,
         t_xr[0].z);
   /* The holding hand's own stick right resizes it and turns nothing. */
   t_let_go();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_hold("edit/stick", 2, 1.0f, 0.0f);
   t_clock += 100000;
   t_edit_poll();
   CHECK(t_near(t_xr[0].yaw, 9.0f) && t_near(t_xr[0].scale, 1.07177f),
         "own stick: yaw %.2f, scale %.4f", t_xr[0].yaw, t_xr[0].scale);
   /* The left hand takes the menu by its bar: its stick is the menu's. */
   t_let_go();
   t_aim[1].position.x = 2.0f;
   t_aim[1].position.y = t_handle_y(0.6f);
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_hold("edit/grab", 1, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(l->state == VIDEO_XR_GRAB_HELD && l->slot == VIDEO_XR_MENU_SLOT,
         "the left took slot %u", l->slot);
   t_hold("edit/stick", 1, 1.0f, 0.0f);
   t_clock += 100000;
   t_edit_poll();
   CHECK(t_near(t_xr[0].yaw, 9.0f) && t_near(t_menu_xr.scale, 1.07177f),
         "both holding: screen 1 yaw %.2f, menu scale %.4f", t_xr[0].yaw,
         t_menu_xr.scale);
   t_let_go();
   t_edit_poll();
   t_edit_end();

   /* Taken pitched 15 degrees, the ray then 20 up: the grab took that
    * pitch, and it keeps it. */
   t_edit_start();
   t_quads.quads[0].pose.orientation.x = (float)sin(M_PI / 24.0);
   t_quads.quads[0].pose.orientation.w = (float)cos(M_PI / 24.0);
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   t_aim[2].position.x    = 0.2f;
   t_aim[2].orientation.x = (float)sin(M_PI / 18.0);
   t_aim[2].orientation.w = (float)cos(M_PI / 18.0);
   t_edit_poll();
   CHECK(t_near(t_xr[0].pitch, 15.0f), "taken at 15, carried at %.2f",
         t_xr[0].pitch);
   t_let_go();
   t_edit_poll();
   t_edit_end();
}

/* A grab with the trigger already held tilts from the start: the roll
 * follows at the next poll. */
static void test_edit_tilt_from_grab(void)
{
   t_edit_start();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_hold("edit/tilt", 2, 1.0f, 0.0f);
   t_edit_poll();
   t_aim[2].orientation.z = (float)sin(M_PI / 12.0);
   t_aim[2].orientation.w = (float)cos(M_PI / 12.0);
   t_edit_poll();
   CHECK(t_near(t_xr[0].roll, 30.0f), "grabbed tilting, rolled %.2f",
         t_xr[0].roll);
   t_edit_end();
}

/* Offsets stop at 10 m and resume from there; a hand out of tracking
 * holds its item still; an item gone from the map ends the grab. */
static void test_edit_limits(void)
{
   const video_xr_grab_t *g = &input_openxr_edit.hand[1].g;
   unsigned seq;
   t_edit_start();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   t_aim[2].position.x = 10.3f;
   t_edit_poll();
   CHECK(t_near(t_xr[0].x, 10.0f), "past the limit: %.3f", t_xr[0].x);
   t_aim[2].position.x = 10.2f;
   t_edit_poll();
   CHECK(t_near(t_xr[0].x, 9.9f), "back from the limit: %.3f", t_xr[0].x);
   t_aim_tracked[2]    = false;
   t_aim[2].position.x = 5.0f;
   t_edit_poll();
   CHECK(g->state == VIDEO_XR_GRAB_HELD && t_near(t_xr[0].x, 9.9f),
         "an untracked hand moved it");
   t_aim_tracked[2]    = true;
   t_aim[2].position.x = 10.1f;
   t_edit_poll();
   CHECK(t_near(t_xr[0].x, 9.8f), "tracked again: %.3f", t_xr[0].x);
   seq               = t_edit_seq;
   t_quads.num_quads = 0;
   t_edit_poll();
   CHECK(g->state == VIDEO_XR_GRAB_NONE && t_edit_seq == seq,
         "a screen gone: state %u, seq %u/%u", g->state, t_edit_seq, seq);
   t_edit_end();
}

/* The core's layout moves the held screen's spot under the hand: the
 * grab ends there, as for a screen gone from the map, and writes
 * nothing from the old spot. */
static void test_edit_spot_moved(void)
{
   const video_xr_grab_t *g = &input_openxr_edit.hand[1].g;
   unsigned seq;
   t_edit_start();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   t_aim[2].position.x = 0.2f;
   t_edit_poll();
   CHECK(g->state == VIDEO_XR_GRAB_HELD && t_near(t_xr[0].x, 0.2f),
         "fixture: not held at 0.2");
   seq                              = t_edit_seq;
   t_quads.slots[0].base.y          = -0.5f;
   t_quads.quads[0].pose.position.y = -0.5f;
   t_aim[2].position.x              = 0.4f;
   t_edit_poll();
   CHECK(g->state == VIDEO_XR_GRAB_NONE && t_edit_seq == seq,
         "a moved spot: state %u, seq %u/%u", g->state, t_edit_seq, seq);
   CHECK(t_near(t_xr[0].x, 0.2f) && t_near(t_xr[0].y, 0.0f),
         "written from the old spot: %.3f %.3f", t_xr[0].x, t_xr[0].y);
   t_edit_end();
}

/* One item per hand: the second hand on the same item does nothing;
 * on another item it holds that one. The menu, by its handle, has a
 * place of its own. */
static void test_edit_two_hands(void)
{
   const video_xr_grab_t *l = &input_openxr_edit.hand[0].g;
   const video_xr_grab_t *r = &input_openxr_edit.hand[1].g;
   t_edit_start();
   t_hold("edit/grab", 1, 1.0f, 0.0f);
   t_edit_poll();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(l->state == VIDEO_XR_GRAB_HELD && l->slot == 0
         && r->state != VIDEO_XR_GRAB_HELD,
         "the second hand took screen 1 too");
   t_let_go();
   t_hold("edit/grab", 1, 1.0f, 0.0f);
   t_aim[2].position.x = 2.0f;
   t_aim[2].position.y = t_handle_y(0.6f);
   t_edit_poll();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(r->state == VIDEO_XR_GRAB_HELD && r->slot == VIDEO_XR_MENU_SLOT,
         "the right hand did not take the menu");
   /* A hand holds one item: the left on the menu's handle takes
    * nothing more. */
   t_aim[1].position.x = 2.0f;
   t_aim[1].position.y = t_handle_y(0.6f);
   t_aim[2].position.x = 2.3f;
   t_edit_poll();
   CHECK(l->state == VIDEO_XR_GRAB_HELD && l->slot == 0
         && t_menu_placed && t_near(t_menu_xr.x, 2.3f)
         && t_near(t_xr[0].x, 2.0f),
         "menu at %.3f placed %d, screen 1 at %.3f", t_menu_xr.x,
         (int)t_menu_placed, t_xr[0].x);
   t_edit_end();
}

/* The menu is taken by its handle alone: through the menu, the laser
 * takes screen 1 behind it. A menu that follows screen 1 is placed
 * where and as large as it shows. */
static void test_edit_menu_handle(void)
{
   const video_xr_grab_t *g = &input_openxr_edit.hand[1].g;
   /* 10 cm in front of a screen 1 pulled 0.9 m nearer: 0.8 m away, and
    * sized by that from its 1.7 m spot. */
   float k = 0.8f / 1.7f;
   t_edit_start();
   t_quads.quads[0].pose.position.z = -0.9f;
   t_xr[0].z                        = 0.9f;
   t_quads.quads[1].pose.position.x = 0.0f;
   t_quads.quads[1].pose.position.z = -0.8f;
   t_quads.quads[1].width           = 1.0f * k;
   t_quads.quads[1].height          = 0.6f * k;
   video_views_xr_reset(&t_menu_xr, 1);
   t_menu_placed                    = false;
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(g->state == VIDEO_XR_GRAB_HELD && g->slot == 0,
         "through the menu: slot %u state %u", g->slot, g->state);
   t_let_go();
   t_edit_poll();
   t_aim[2].position.y = t_handle_y(0.6f * k);
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(g->state == VIDEO_XR_GRAB_HELD && g->slot == VIDEO_XR_MENU_SLOT,
         "its handle: slot %u state %u", g->slot, g->state);
   t_edit_poll();
   CHECK(t_menu_placed && t_near(t_menu_xr.x, 0.0f)
         && t_near(t_menu_xr.y, 0.0f) && t_near(t_menu_xr.z, 0.9f)
         && t_near(t_menu_xr.scale, k) && t_near(g->width, k),
         "placed %d at %.3f %.3f %.3f, scale %.4f, width %.4f",
         (int)t_menu_placed, t_menu_xr.x, t_menu_xr.y, t_menu_xr.z,
         t_menu_xr.scale, g->width);
   t_edit_end();
}

/* The main button on a free item resets it as Reset Screen does, and
 * puts the menu back in front of screen 1; on a held item, nothing. */
static void test_edit_reset(void)
{
   t_edit_start();
   t_xr[0].x                        = 0.5f;
   t_xr[0].scale                    = 2.0f;
   t_xr[0].visible                  = false;
   t_quads.quads[0].pose.position.x = 0.5f;
   t_aim[2].position.x              = 0.5f;
   t_hold("edit/reset", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(t_xr[0].x == 0.0f && t_xr[0].scale == 1.0f && t_xr[0].visible
         && t_modified, "Reset left screen 1 at %.2f", t_xr[0].x);
   /* Held on to the menu's handle: no new press, no reset. */
   t_aim[2].position.x = 2.0f;
   t_aim[2].position.y = t_handle_y(0.6f);
   t_edit_poll();
   CHECK(t_menu_placed, "a held button reset the menu too");
   t_let_go();
   t_edit_poll();
   t_aim[2].position.x = 2.0f;
   t_aim[2].position.y = t_handle_y(0.6f);
   t_hold("edit/reset", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(!t_menu_placed && t_menu_xr.x == 0.0f, "Reset left the menu placed");
   t_let_go();
   t_edit_poll();
   /* Held by the other hand: no reset. */
   t_xr[0].x           = 0.5f;
   t_aim[1].position.x = 0.5f;
   t_aim[2].position.x = 0.5f;
   t_aim[2].position.y = 0.0f;
   t_hold("edit/grab", 1, 1.0f, 0.0f);
   t_edit_poll();
   t_hold("edit/reset", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(t_near(t_xr[0].x, 0.5f), "a held screen was reset");
   t_edit_end();
}

/* A button held as edit mode starts (the menu's OK on A, which is
 * Reset) counts once released and pressed again; a grip too. */
static void test_edit_held_at_start(void)
{
   const video_xr_grab_t *g = &input_openxr_edit.hand[1].g;
   t_forget_devices();
   t_fresh_session();
   t_edit_fixture();
   t_xr[0].x                        = 0.5f;
   t_quads.quads[0].pose.position.x = 0.5f;
   t_aim[2].position.x              = 0.5f;
   t_hold("combined/a", 0, 1.0f, 0.0f);
   t_hold("edit/reset", 2, 1.0f, 0.0f);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_edit_on(false);
   CHECK(t_editing, "fixture: not editing");
   t_edit_poll();
   CHECK(t_near(t_xr[0].x, 0.5f) && !t_modified,
         "Reset held from the start reset screen 1 to %.2f", t_xr[0].x);
   t_let_go();
   t_edit_poll();
   t_hold("edit/reset", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(t_xr[0].x == 0.0f && t_modified,
         "Reset pressed again left screen 1 at %.2f", t_xr[0].x);
   t_edit_end();

   t_forget_devices();
   t_fresh_session();
   t_edit_fixture();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_on(false);
   CHECK(t_editing, "fixture: not editing");
   t_edit_poll();
   CHECK(g->state != VIDEO_XR_GRAB_HELD,
         "a grip held from the start grabbed %u", g->slot);
   t_let_go();
   t_edit_poll();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(g->state == VIDEO_XR_GRAB_HELD && g->slot == 0,
         "a grip pressed again grabbed %u in state %u", g->slot, g->state);
   t_edit_end();
}

/* A recenter lets go first; edit mode stays on, and a grip still held
 * grabs nothing until pressed again. */
static void test_edit_recenter(void)
{
   const video_xr_grab_t *g = &input_openxr_edit.hand[1].g;
   unsigned seq;
   t_edit_start();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   seq = t_edit_seq;
   input_openxr_edit_release();
   CHECK(g->state == VIDEO_XR_GRAB_RELEASED && t_edit_seq == seq + 1
         && t_editing, "a recenter did not let go: %u", g->state);
   t_edit_poll();
   CHECK(g->state == VIDEO_XR_GRAB_RELEASED, "a held grip grabbed again");
   t_let_go();
   t_edit_poll();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(g->state == VIDEO_XR_GRAB_HELD, "a new grip did not grab");
   /* Leaving edit mode lets go too. */
   seq = t_edit_seq;
   input_openxr_edit_toggle();
   CHECK(g->state == VIDEO_XR_GRAB_RELEASED && g->release_seq == seq + 1
         && !t_editing, "leaving held on: state %u", g->state);
   t_edit_end();
}

/* A hand points at items where they show: one another hand holds is
 * where that hand put it, not where the last quads had it. Screen 1
 * lifted 2 m, the laser through its old place takes screen 2 behind. */
static void test_edit_held_moves(void)
{
   const video_xr_grab_t *l = &input_openxr_edit.hand[0].g;
   const video_xr_grab_t *r = &input_openxr_edit.hand[1].g;
   t_edit_start();
   t_quads.num_quads                = 3;
   t_quads.quads[2]                 = t_quads.quads[0];
   t_quads.quads[2].pose.position.z = -3.0f;
   t_quads.quads[2].screen          = 1;
   t_quads.quads[2].slot            = 1;
   t_quads.slots[1].base.z          = -3.0f;
   t_hold("edit/grab", 1, 1.0f, 0.0f);
   t_edit_poll();
   t_aim[1].position.y = 2.0f;
   t_edit_poll();
   CHECK(l->state == VIDEO_XR_GRAB_HELD && l->slot == 0
         && t_near(t_xr[0].y, 2.0f), "the left hand did not lift screen 1");
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(r->state == VIDEO_XR_GRAB_HELD && r->slot == 1,
         "the right hand took slot %u in state %u", r->slot, r->state);
   t_edit_end();
}

/* A stereo screen is one item: a grab moves both eyes' quads, and a
 * second hand on the pair is refused. */
static void test_edit_stereo(void)
{
   const video_xr_grab_t *l = &input_openxr_edit.hand[0].g;
   const video_xr_grab_t *r = &input_openxr_edit.hand[1].g;
   const video_xr_quad_t *q = input_openxr_st.quads.quads;
   t_edit_start();
   t_quads.num_quads       = 3;
   t_quads.quads[2]        = t_quads.quads[1];
   t_quads.quads[1]        = t_quads.quads[0];
   t_quads.quads[0].eye    = VIDEO_XR_EYE_LEFT;
   t_quads.quads[1].eye    = VIDEO_XR_EYE_RIGHT;
   t_quads.quads[1].layer  = 1;
   t_quads.slots[0].layers = 2;
   t_hold("edit/grab", 1, 1.0f, 0.0f);
   t_edit_poll();
   t_aim[1].position.x = 0.3f;
   t_edit_poll();
   t_edit_poll();
   CHECK(l->state == VIDEO_XR_GRAB_HELD && l->slot == 0
         && t_near(t_xr[0].x, 0.3f) && t_near(q[0].pose.position.x, 0.3f)
         && t_near(q[1].pose.position.x, 0.3f),
         "the pair: setting %.3f, eyes at %.3f and %.3f", t_xr[0].x,
         q[0].pose.position.x, q[1].pose.position.x);
   t_aim[2].position.x = 0.3f;
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(r->state != VIDEO_XR_GRAB_HELD, "a second hand took the pair");
   t_edit_end();
}

/* A views core's frame shown whole without the screens' settings is no
 * item: a grip and the main button on it do nothing and write nothing,
 * no dot shows on it, and a screen held as it turns into one is let go
 * as gone from the map. */
static void test_edit_fallback_frame(void)
{
   const XrCompositionLayerBaseHeader *layers[4];
   const video_xr_grab_t *g = &input_openxr_edit.hand[1].g;
   video_xr_quad_set_t frame;
   unsigned seq;
   t_edit_start();
   t_quads.quads[0].kind  = VIDEO_XR_QUAD_FRAME;
   t_quads.slots[0].fixed = true;
   t_xr[0].x              = 0.5f;
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   t_aim[2].position.x = 0.2f;
   t_edit_poll();
   CHECK(g->state != VIDEO_XR_GRAB_HELD && !t_modified
         && t_xr[0].x == 0.5f, "the fallback frame grabbed: state %u, "
         "screen 1 at %.3f", g->state, t_xr[0].x);
   t_let_go();
   t_edit_poll();
   t_hold("edit/reset", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(!t_modified && t_xr[0].x == 0.5f,
         "the main button on the fallback frame reset screen 1");
   t_let_go();
   t_cursor        = true;
   frame           = t_quads;
   frame.num_quads = 1;
   CHECK(t_hooks.frame_layers(t_hooks.user, t_time, &frame,
            t_xr_session.local_space, layers, 4, &t_under) == 0,
         "a dot or an outline on the fallback frame");
   t_cursor = false;
   t_quads.quads[0].kind  = VIDEO_XR_QUAD_SCREEN;
   t_quads.slots[0].fixed = false;
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(g->state == VIDEO_XR_GRAB_HELD, "fixture: screen 1 not held");
   seq                    = t_edit_seq;
   t_quads.quads[0].kind  = VIDEO_XR_QUAD_FRAME;
   t_quads.slots[0].fixed = true;
   t_edit_poll();
   CHECK(g->state == VIDEO_XR_GRAB_NONE && t_edit_seq == seq,
         "held into the fallback: state %u, seq %u/%u", g->state,
         t_edit_seq, seq);
   t_edit_end();
}

/* What the hands hold reaches the frame loop each poll, let go with its
 * count; a hand that lets go and takes another item at its next poll
 * still hands the first over, where it let go of it; leaving edit mode
 * lets go for the frame loop too; a new session starts with nothing
 * held. */
static void test_edit_hold_record(void)
{
   unsigned seq, n;
   t_edit_start();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(t_grabs.hand[1].state == VIDEO_XR_GRAB_HELD
         && t_grabs.hand[1].slot == 0
         && t_near(t_grabs.hand[1].width, 1.6f)
         && t_near(t_grabs.hand[1].base.z, -1.8f),
         "held: state %u slot %u", t_grabs.hand[1].state,
         t_grabs.hand[1].slot);
   t_aim[2].position.x = 0.3f;
   t_edit_poll();
   CHECK(t_near(t_grabs.hand[1].last.position.x, 0.3f),
         "the hold's place not given: %.3f",
         t_grabs.hand[1].last.position.x);
   t_let_go();
   t_edit_poll();
   seq = t_edit_seq;
   CHECK(t_grabs.hand[1].state == VIDEO_XR_GRAB_RELEASED
         && t_grabs.hand[1].release_seq == seq
         && t_grabs.let_go[0].state == VIDEO_XR_GRAB_RELEASED
         && t_grabs.let_go[0].release_seq == seq
         && t_near(t_grabs.let_go[0].last.position.x, 0.3f),
         "let go: state %u seq %u/%u, slot 0's %u at %.3f",
         t_grabs.hand[1].state, t_grabs.hand[1].release_seq, seq,
         t_grabs.let_go[0].state, t_grabs.let_go[0].last.position.x);
   /* The same hand on the menu's handle at its next poll. */
   t_aim[2].position.x = 2.0f;
   t_aim[2].position.y = t_handle_y(0.6f);
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(t_grabs.hand[1].state == VIDEO_XR_GRAB_HELD
         && t_grabs.hand[1].slot == VIDEO_XR_MENU_SLOT
         && t_grabs.let_go[0].state == VIDEO_XR_GRAB_RELEASED
         && t_grabs.let_go[0].release_seq == seq
         && t_near(t_grabs.let_go[0].last.position.x, 0.3f)
         && t_near(t_grabs.let_go[0].width, 1.6f),
         "screen 1's hand-over lost with its hold: slot 0's %u seq %u",
         t_grabs.let_go[0].state, t_grabs.let_go[0].release_seq);
   input_openxr_edit_toggle();
   CHECK(!t_editing && t_grabs.hand[1].state == VIDEO_XR_GRAB_RELEASED
         && t_grabs.let_go[VIDEO_XR_MENU_SLOT].state
            == VIDEO_XR_GRAB_RELEASED
         && t_grabs.let_go[VIDEO_XR_MENU_SLOT].release_seq == t_edit_seq
         && t_grabs.let_go[0].release_seq == seq,
         "leaving edit mode: the frame loop still sees state %u",
         t_grabs.hand[1].state);
   t_edit_end();
   /* A session's start is the frame's to clear; a recenter with nothing
    * held outside edit mode publishes nothing either. */
   n = t_grabs_published;
   t_fresh_session();
   CHECK(t_grabs_published == n, "the session's start published a record");
   input_openxr_edit_release();
   CHECK(t_grabs_published == n,
         "a recenter outside edit mode published a record");
   t_hooks.session_destroying(t_hooks.user);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_forget_devices();
}

/* The XR thread's aim: each hand's own, in the space asked; nothing
 * for a hand that is not tracked or does not exist. */
static void test_edit_frame_aim(void)
{
   video_xr_pose_t pose;
   t_edit_start();
   t_aim[1].position.x = -0.25f;
   CHECK(     t_hooks.aim(t_hooks.user, 0, t_xr_session.view_space, t_time,
               &pose)
         && t_near(pose.position.x, -0.25f)
         && t_located_base == t_xr_session.view_space,
         "the left hand's aim: %.3f", pose.position.x);
   t_aim_tracked[1] = false;
   CHECK(!t_hooks.aim(t_hooks.user, 0, t_xr_session.local_space, t_time,
            &pose), "an untracked hand aimed");
   CHECK(!t_hooks.aim(t_hooks.user, 2, t_xr_session.local_space, t_time,
            &pose), "a third hand aimed");
   t_edit_end();
}

/* Edit mode's layers: a bright outline round what a hand points at, a
 * faint one round every other item, the menu's handle bar after its
 * outline, then the dot; each where the frame shows its item; outlines
 * only in the layers the dots leave; none round a fixed slot. The
 * screens' outlines go first, under the screens; the menu's and its
 * handle over them. */
static void test_edit_outlines(void)
{
   unsigned n;
   video_xr_quad_set_t shown;
   const XrCompositionLayerQuad *q;
   const XrCompositionLayerBaseHeader *layers[OPENXR_MAX_EXTRA_LAYERS];
   XrSpace local = XR_NULL_HANDLE;
   t_edit_start();
   local            = t_xr_session.local_space;
   t_aim_tracked[1] = false;
   t_cursor         = true;
   shown            = t_quads;
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   CHECK(n == 4, "%u layers, want two outlines, a handle and a dot", n);
   CHECK(t_under == 1, "%u layers under the screens, want screen 1's "
         "outline alone", t_under);
   q = (const XrCompositionLayerQuad*)layers[0];
   CHECK(     q->subImage.imageRect.offset.x == VIDEO_XR_OUTLINE_X(1)
         && q->subImage.imageRect.offset.y == 0
         && q->subImage.imageRect.extent.width == VIDEO_XR_OUTLINE_DIM
         && q->subImage.imageRect.extent.height == VIDEO_XR_OUTLINE_DIM
         && q->subImage.swapchain == (XrSwapchain)(uintptr_t)0x51
         && q->space == local
         && (q->layerFlags
            & XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT)
         && t_near(q->size.width, t_outline(1.6f))
         && t_near(q->size.height, t_outline(0.96f))
         && t_near(q->pose.position.z, -1.8f + VIDEO_XR_CURSOR_LIFT),
         "screen 1's outline: offset %d, %.3f x %.3f at z %.4f",
         (int)q->subImage.imageRect.offset.x, q->size.width,
         q->size.height, q->pose.position.z);
   q = (const XrCompositionLayerQuad*)layers[1];
   CHECK(q->subImage.imageRect.offset.x == VIDEO_XR_OUTLINE_X(0)
         && t_near(q->pose.position.x, 2.0f)
         && t_near(q->size.width, t_outline(1.0f)),
         "the menu's outline is not the faint one round it");
   q = (const XrCompositionLayerQuad*)layers[2];
   CHECK(     q->subImage.imageRect.offset.x == VIDEO_XR_HANDLE_X(0)
         && q->subImage.imageRect.offset.y == VIDEO_XR_HANDLE_Y
         && q->subImage.imageRect.extent.width == VIDEO_XR_HANDLE_COLS
         && q->subImage.imageRect.extent.height == VIDEO_XR_HANDLE_ROWS
         && t_near(q->size.width, 1.0f * VIDEO_XR_HANDLE_WIDTH)
         && t_near(q->size.height, VIDEO_XR_HANDLE_HEIGHT)
         && t_near(q->pose.position.x, 2.0f)
         && t_near(q->pose.position.y, t_handle_y(0.6f)),
         "the menu's handle: offset %d, %.3f x %.3f at %.3f,%.3f",
         (int)q->subImage.imageRect.offset.x, q->size.width, q->size.height,
         q->pose.position.x, q->pose.position.y);
   q = (const XrCompositionLayerQuad*)layers[3];
   CHECK(q->subImage.imageRect.extent.width == VIDEO_XR_CURSOR_DIM
         && q->subImage.imageRect.offset.x == 0
         && q->subImage.imageRect.offset.y == VIDEO_XR_CURSOR_Y,
         "the dot is not last, or not its part of the image");

   /* The frame shows screen 1 elsewhere: its outline goes there. */
   shown.quads[0].pose.position.x = 0.5f;
   t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   q = (const XrCompositionLayerQuad*)layers[0];
   CHECK(t_near(q->pose.position.x, 0.5f),
         "the outline at %.3f, not where the frame shows screen 1",
         q->pose.position.x);
   shown = t_quads;

   /* A laser on the handle lights the menu's outline and handle. */
   t_aim[2].position.x = 2.0f;
   t_aim[2].position.y = t_handle_y(0.6f);
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   CHECK(n == 4
         && ((const XrCompositionLayerQuad*)layers[0])
            ->subImage.imageRect.offset.x == VIDEO_XR_OUTLINE_X(0)
         && ((const XrCompositionLayerQuad*)layers[1])
            ->subImage.imageRect.offset.x == VIDEO_XR_OUTLINE_X(1)
         && ((const XrCompositionLayerQuad*)layers[2])
            ->subImage.imageRect.offset.x == VIDEO_XR_HANDLE_X(1),
         "the handle under the laser: %u layers", n);
   t_aim[2].position.x = 0.0f;
   t_aim[2].position.y = 0.0f;

   /* The menu first among the quads: its outline still over. */
   shown.quads[0] = t_quads.quads[1];
   shown.quads[1] = t_quads.quads[0];
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   CHECK(n == 4 && t_under == 1
         && ((const XrCompositionLayerQuad*)layers[0])
            ->subImage.imageRect.offset.x == VIDEO_XR_OUTLINE_X(1)
         && t_near(((const XrCompositionLayerQuad*)layers[1])
            ->pose.position.x, 2.0f)
         && ((const XrCompositionLayerQuad*)layers[2])
            ->subImage.imageRect.offset.x == VIDEO_XR_HANDLE_X(0),
         "the menu first: %u layers, %u under", n, t_under);
   shown = t_quads;

   /* With the dots' two layers alone: the dot, no outline. */
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_KEPT_EXTRA_LAYERS, &t_under);
   q = (const XrCompositionLayerQuad*)layers[0];
   CHECK(n == 1 && t_under == 0
         && q->subImage.imageRect.extent.width == VIDEO_XR_CURSOR_DIM
         && q->subImage.imageRect.offset.x == 0,
         "with two layers: %u, the dot dropped or an outline kept", n);
   /* One more: screen 1's outline first. */
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_KEPT_EXTRA_LAYERS + 1, &t_under);
   q = (const XrCompositionLayerQuad*)layers[0];
   CHECK(n == 2 && t_under == 1
         && q->subImage.imageRect.offset.x == VIDEO_XR_OUTLINE_X(1),
         "with three layers: %u", n);

   /* A frame shown whole without the screens' settings: no outline. */
   shown.slots[0].fixed = true;
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   q = (const XrCompositionLayerQuad*)layers[0];
   CHECK(n == 2 && t_under == 0
         && q->subImage.imageRect.offset.x == VIDEO_XR_OUTLINE_X(0)
         && t_near(q->pose.position.x, 2.0f),
         "%u layers round a fixed slot and the menu", n);

   /* A stereo screen is one item: one outline round its pair. */
   shown                  = t_quads;
   shown.num_quads        = 3;
   shown.quads[2]         = shown.quads[1];
   shown.quads[1]         = shown.quads[0];
   shown.quads[0].eye     = VIDEO_XR_EYE_LEFT;
   shown.quads[1].eye     = VIDEO_XR_EYE_RIGHT;
   shown.quads[1].layer   = 1;
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   CHECK(n == 4 && t_under == 1,
         "%u layers round a stereo screen and the menu", n);

   /* Out of edit mode, Always: the dot alone. */
   input_openxr_edit_toggle();
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_ALWAYS);
   n = t_hooks.frame_layers(t_hooks.user, t_time, &t_quads, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   q = (const XrCompositionLayerQuad*)layers[0];
   CHECK(n == 1 && t_under == 0 && q->subImage.imageRect.offset.x == 0,
         "%u layers out of edit mode", n);
   t_cursor = false;
   t_edit_end();
}

/* Screen 2, 0.8 x 0.5, 20 cm nearer than the menu and under its
 * handle bar, its top edge 7 mm above the bar's middle. */
static void t_screen_under_handle(void)
{
   video_xr_quad_t *q = &t_quads.quads[t_quads.num_quads++];
   memset(q, 0, sizeof(*q));
   video_xr_pose_identity(&q->pose);
   q->pose.position.x = 2.0f;
   q->pose.position.y = t_handle_y(0.6f) + 0.007f - 0.25f;
   q->pose.position.z = -1.5f;
   q->width           = 0.8f;
   q->height          = 0.5f;
   q->kind            = VIDEO_XR_QUAD_SCREEN;
   q->screen          = 1;
   q->slot            = 1;
   t_quads.slots[1].base = q->pose.position;
}

/* Whether the bright outlines in the frame loop's layers are only one
 * w wide (the menu's 1.0, screen 2's 0.8). */
static bool t_lit_only(const XrCompositionLayerBaseHeader **layers,
      unsigned n, float w)
{
   unsigned i;
   unsigned lit = 0;
   for (i = 0; i < n; i++)
   {
      const XrCompositionLayerQuad *q =
         (const XrCompositionLayerQuad*)layers[i];
      if (     q->subImage.imageRect.offset.x != VIDEO_XR_OUTLINE_X(1)
            || q->subImage.imageRect.extent.width != VIDEO_XR_OUTLINE_DIM)
         continue;
      if (!t_near(q->size.width, t_outline(w)))
         return false;
      lit++;
   }
   return lit == 1;
}

/* The menu's handle sticks to a laser that was on its bar: in the 3 cm
 * margin over screen 2 the grip still takes the menu; arriving there
 * from screen 2, screen 2. Losing tracking and edit mode's end let go;
 * the frame loop is told, and lights what is stuck. */
static void test_edit_handle_sticky(void)
{
   unsigned n;
   video_xr_quad_set_t shown;
   const XrCompositionLayerBaseHeader *layers[OPENXR_MAX_EXTRA_LAYERS];
   XrSpace local      = XR_NULL_HANDLE;
   const float hy     = t_handle_y(0.6f);
   input_openxr_edit_t *e = &input_openxr_edit;
   t_edit_start();
   t_screen_under_handle();
   t_aim_tracked[1]    = false;
   t_aim[2].position.x = 2.0f;
   t_aim[2].position.y = hy;
   t_edit_poll();
   CHECK(e->stuck == 2u && t_grabs.stuck == 2u
         && input_openxr_st.hit[1] == 1,
         "on the bar: stuck %#x, published %#x, hit %d", e->stuck,
         t_grabs.stuck, input_openxr_st.hit[1]);
   t_aim[2].position.y = hy - 0.03f;
   t_edit_poll();
   CHECK(e->stuck == 2u && input_openxr_st.hit[1] == 1,
         "in the margin over screen 2: stuck %#x, hit %d", e->stuck,
         input_openxr_st.hit[1]);
   /* The frame loop lights the menu while stuck, screen 2 when not. */
   local    = t_xr_session.local_space;
   t_cursor = true;
   shown    = t_quads;
   t_xr_frame.grabs = t_grabs;
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   CHECK(t_lit_only(layers, n, 1.0f), "stuck: the menu not alone lit");
   t_xr_frame.grabs.stuck = 0;
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   CHECK(t_lit_only(layers, n, 0.8f), "unstuck: screen 2 not alone lit");
   memset(&t_xr_frame.grabs, 0, sizeof(t_xr_frame.grabs));
   t_cursor = false;
   /* The grip takes the menu. */
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(e->hand[1].g.state == VIDEO_XR_GRAB_HELD
         && e->hand[1].g.slot == VIDEO_XR_MENU_SLOT,
         "stuck, the grip took slot %u", e->hand[1].g.slot);
   t_let_go();
   t_edit_poll();
   /* From screen 2 past the margin into it: screen 2's. */
   t_aim[2].position.y = hy - 0.06f;
   t_edit_poll();
   CHECK(!e->stuck && input_openxr_st.hit[1] == 2,
         "past the margin: stuck %#x, hit %d", e->stuck,
         input_openxr_st.hit[1]);
   t_aim[2].position.y = hy - 0.03f;
   t_edit_poll();
   CHECK(!e->stuck && input_openxr_st.hit[1] == 2,
         "arriving from screen 2: stuck %#x, hit %d", e->stuck,
         input_openxr_st.hit[1]);
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(e->hand[1].g.slot == 1, "not stuck, the grip took slot %u",
         e->hand[1].g.slot);
   t_let_go();
   t_edit_poll();
   /* Untracked, it lets go; edit mode's end clears it. */
   t_aim[2].position.y = hy;
   t_edit_poll();
   t_aim_tracked[2] = false;
   t_edit_poll();
   CHECK(!e->stuck && !t_grabs.stuck, "untracked, still stuck %#x",
         e->stuck);
   t_aim_tracked[2] = true;
   t_edit_poll();
   CHECK(e->stuck == 2u, "back on the bar: %#x", e->stuck);
   /* Per hand: the left hand in the margin, only the right stuck. */
   t_aim_tracked[1]    = true;
   t_aim[1].position.x = 2.0f;
   t_aim[1].position.y = hy - 0.03f;
   t_edit_poll();
   CHECK(e->stuck == 2u && input_openxr_st.hit[0] == 2,
         "the left hand in the margin: stuck %#x, hit %d", e->stuck,
         input_openxr_st.hit[0]);
   t_aim_tracked[2]       = false;
   t_cursor               = true;
   shown                  = t_quads;
   t_xr_frame.grabs.stuck = 2u;
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   CHECK(t_lit_only(layers, n, 0.8f), "the left hand lit the menu");
   memset(&t_xr_frame.grabs, 0, sizeof(t_xr_frame.grabs));
   t_cursor = false;
   t_edit_end();
   CHECK(!e->stuck, "edit mode ended, still stuck %#x", e->stuck);
}

/* A layer's own axis (x, y, z), in its space. */
static void t_layer_axis(const XrCompositionLayerQuad *l, float x, float y,
      float z, video_xr_vec3_t *out)
{
   video_xr_quat_t o;
   video_xr_vec3_t v;
   o.x = l->pose.orientation.x;
   o.y = l->pose.orientation.y;
   o.z = l->pose.orientation.z;
   o.w = l->pose.orientation.w;
   v.x = x;
   v.y = y;
   v.z = z;
   video_xr_rotate(&o, &v, out);
}

/* The beams: one a tracked hand, from its aim to its dot, or
 * VIDEO_XR_BEAM_REACH into nothing; along the ray, facing the head;
 * after the outlines and before the dots, and the first to go when the
 * layers run short. Off: none; Edit Mode Only: in edit mode alone;
 * Always: wherever the laser can point. No head, no beam. */
static void test_edit_beams(void)
{
   unsigned n;
   video_xr_vec3_t ax, az;
   const XrCompositionLayerQuad *q;
   const XrCompositionLayerBaseHeader *layers[OPENXR_MAX_EXTRA_LAYERS];
   XrSpace local = XR_NULL_HANDLE;
   t_edit_start();
   local               = t_xr_session.local_space;
   t_aim_tracked[1]    = false;
   t_aim[2].position.x = 0.2f;
   t_aim[2].position.y = -0.3f;
   t_head_tracked      = true;
   t_cursor            = true;

   /* Straight ahead onto screen 1, 1.8 m away. */
   n = t_hooks.frame_layers(t_hooks.user, t_time, &t_quads, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   CHECK(n == 5, "%u layers, want three outlines, a beam and a dot", n);
   q = (const XrCompositionLayerQuad*)layers[3];
   CHECK(     q->subImage.imageRect.offset.x == VIDEO_XR_BEAM_X(0)
         && q->subImage.imageRect.offset.y == VIDEO_XR_BEAM_Y
         && q->subImage.imageRect.extent.width == VIDEO_XR_BEAM_LEN
         && q->subImage.imageRect.extent.height == VIDEO_XR_BEAM_ROWS
         && q->subImage.swapchain == (XrSwapchain)(uintptr_t)0x51
         && q->space == local
         && (q->layerFlags
            & XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT)
         && t_near(q->size.width, 1.8f)
         && t_near(q->size.height, VIDEO_XR_BEAM_WIDTH)
         && t_near(q->pose.position.x, 0.2f)
         && t_near(q->pose.position.y, -0.3f)
         && t_near(q->pose.position.z, -0.9f),
         "the beam: offset %d, %.3f x %.4f at %.3f,%.3f,%.3f",
         (int)q->subImage.imageRect.offset.x, q->size.width, q->size.height,
         q->pose.position.x, q->pose.position.y, q->pose.position.z);
   t_layer_axis(q, 1.0f, 0.0f, 0.0f, &ax);
   t_layer_axis(q, 0.0f, 0.0f, 1.0f, &az);
   CHECK(t_near(ax.z, -1.0f), "the beam's x is not down the ray");
   CHECK(t_near(az.x, -0.5547f) && t_near(az.y, 0.8321f) && t_near(az.z, 0.0f),
         "the beam faces %.3f,%.3f,%.3f, not the head", az.x, az.y, az.z);
   q = (const XrCompositionLayerQuad*)layers[4];
   CHECK(q->subImage.imageRect.offset.x == 0
         && q->subImage.imageRect.extent.width == VIDEO_XR_CURSOR_DIM,
         "the dot is not last");

   /* Straight up, into nothing: 1.5 m, the strip that fades out. */
   t_aim[2].orientation.x = 0.70711f;
   t_aim[2].orientation.w = 0.70711f;
   n = t_hooks.frame_layers(t_hooks.user, t_time, &t_quads, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   q = (const XrCompositionLayerQuad*)layers[3];
   CHECK(n == 4 && q->subImage.imageRect.offset.x == VIDEO_XR_BEAM_X(1)
         && t_near(q->size.width, VIDEO_XR_BEAM_REACH)
         && t_near(q->pose.position.y, -0.3f + 0.5f * VIDEO_XR_BEAM_REACH)
         && t_near(q->pose.position.z, 0.0f),
         "%u layers; the missing beam: offset %d, %.3f long at y %.3f", n,
         (int)q->subImage.imageRect.offset.x, q->size.width,
         q->pose.position.y);
   t_layer_axis(q, 0.0f, 0.0f, 1.0f, &az);
   CHECK(t_near(az.x, -1.0f), "the missing beam does not face the head");
   t_aim[2].orientation.x = 0.0f;
   t_aim[2].orientation.w = 1.0f;

   /* Short of layers: the beam goes before any outline; the dot stays. */
   n = t_hooks.frame_layers(t_hooks.user, t_time, &t_quads, local, layers,
         OPENXR_KEPT_EXTRA_LAYERS + 3, &t_under);
   q = (const XrCompositionLayerQuad*)layers[3];
   CHECK(n == 4 && q->subImage.imageRect.offset.x == 0,
         "with room for the outlines and dots: %u layers", n);
   n = t_hooks.frame_layers(t_hooks.user, t_time, &t_quads, local, layers,
         OPENXR_KEPT_EXTRA_LAYERS + 2, &t_under);
   CHECK(n == 3, "with room for two outlines: %u layers", n);
   /* Two hands on screen 1, room for one beam: the first hand's. */
   t_aim_tracked[1]    = true;
   t_aim[1].position.x = -0.2f;
   t_aim[1].position.y = -0.3f;
   n = t_hooks.frame_layers(t_hooks.user, t_time, &t_quads, local, layers,
         OPENXR_KEPT_EXTRA_LAYERS + 4, &t_under);
   q = (const XrCompositionLayerQuad*)layers[3];
   CHECK(n == 6 && q->subImage.imageRect.offset.x == VIDEO_XR_BEAM_X(0)
         && t_near(q->pose.position.x, -0.2f),
         "two hands, one beam's room: %u layers", n);
   n = t_hooks.frame_layers(t_hooks.user, t_time, &t_quads, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   CHECK(n == 7, "two hands with room: %u layers, want 3 + 2 + 2", n);
   t_aim_tracked[1] = false;

   /* No head: no beam. */
   t_head_tracked = false;
   n = t_hooks.frame_layers(t_hooks.user, t_time, &t_quads, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   CHECK(n == 4, "%u layers with the head lost", n);
   t_head_tracked = true;

   /* Off: none. A value past the choices is Edit Mode Only. */
   t_beam = VIDEO_OPENXR_LASER_BEAM_OFF;
   t_edit_poll();
   n = t_hooks.frame_layers(t_hooks.user, t_time, &t_quads, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   CHECK(n == 4, "%u layers with Laser Beam Off", n);
   t_beam = 7;
   t_edit_poll();
   n = t_hooks.frame_layers(t_hooks.user, t_time, &t_quads, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   CHECK(n == 5, "%u layers with Laser Beam 7", n);

   /* Out of edit mode, Laser Pointer Always: Edit Mode Only draws the
    * dot alone, Always the beam too; Laser Pointer Off neither. */
   t_beam = VIDEO_OPENXR_LASER_BEAM_EDIT;
   input_openxr_edit_toggle();
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_ALWAYS);
   n = t_hooks.frame_layers(t_hooks.user, t_time, &t_quads, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   CHECK(n == 1, "%u layers out of edit mode, Edit Mode Only", n);
   t_beam = VIDEO_OPENXR_LASER_BEAM_ALWAYS;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_ALWAYS);
   n = t_hooks.frame_layers(t_hooks.user, t_time, &t_quads, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   q = (const XrCompositionLayerQuad*)layers[0];
   CHECK(n == 2 && q->subImage.imageRect.offset.x == VIDEO_XR_BEAM_X(0),
         "%u layers out of edit mode, Always", n);
   /* Auto with the menu closed: screen 1 is not the laser's, so there is
    * nothing to point at and no beam. */
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_AUTO);
   n = t_hooks.frame_layers(t_hooks.user, t_time, &t_quads, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   CHECK(n == 0, "%u layers with nothing the laser can point at", n);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   n = t_hooks.frame_layers(t_hooks.user, t_time, &t_quads, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   CHECK(n == 0, "%u layers with Laser Pointer Off", n);

   t_beam              = VIDEO_OPENXR_LASER_BEAM_EDIT;
   t_head_tracked      = false;
   t_cursor            = false;
   t_aim[2].position.x = 0.0f;
   t_aim[2].position.y = 0.0f;
   t_edit_end();
}

/* Screen 2 hidden behind screen 1: 1.2 x 0.9 at 2.1 m, its spot. */
static void t_edit_buried(void)
{
   video_xr_quad_t *q = &t_quads.quads[t_quads.num_quads++];
   memset(q, 0, sizeof(*q));
   video_xr_pose_identity(&q->pose);
   q->pose.position.z      = -2.1f;
   q->width                = 1.2f;
   q->height               = 0.9f;
   q->kind                 = VIDEO_XR_QUAD_SCREEN;
   q->screen               = 1;
   q->slot                 = 1;
   t_quads.slots[1].base.z = -2.1f;
}

/* Select Next (step 1) or Select Previous (-1) pressed and let go;
 * whatever else was held is let go too. */
static void t_edit_select(int step)
{
   t_hold(step > 0 ? "edit/select_next" : "edit/select_previous", 0,
         1.0f, 0.0f);
   t_edit_poll();
   t_let_go();
   t_edit_poll();
}

/* The slots whose outline is bright in the frame loop's layers: a
 * bright outline shows the image's VIDEO_XR_OUTLINE_X(1) square, as
 * test_edit_outlines tells it (handles, beams and dots show other
 * parts); its item told by where it sits: the menu at x 2, screen 2
 * beyond 2 m, screen 1 else. */
static unsigned t_lit(const XrCompositionLayerBaseHeader **layers,
      unsigned n)
{
   unsigned i;
   unsigned lit = 0;
   for (i = 0; i < n; i++)
   {
      const XrCompositionLayerQuad *q =
         (const XrCompositionLayerQuad*)layers[i];
      if (     q->subImage.imageRect.offset.x != VIDEO_XR_OUTLINE_X(1)
            || q->subImage.imageRect.extent.width != VIDEO_XR_OUTLINE_DIM)
         continue;
      if (t_near(q->pose.position.x, 2.0f))
         lit |= 1u << VIDEO_XR_MENU_SLOT;
      else if (q->pose.position.z < -2.0f)
         lit |= 1u << 1;
      else
         lit |= 1u;
   }
   return lit;
}

/* Select Next and Select Previous: suggested per profile, the Frame's
 * on the left D-pad beside its Reset, the Vive's next alone, the simple
 * controller's none. */
static void test_edit_select_bindings(void)
{
   const char *index  = "/interaction_profiles/valve/index_controller";
   const char *frame  = "/interaction_profiles/valve/frame_controller_valve";
   const char *vive   = "/interaction_profiles/htc/vive_controller";
   const char *simple = "/interaction_profiles/khr/simple_controller";
   t_forget_devices();
   t_num_sets    = 0;
   t_num_actions = 0;
   t_num_bound   = 0;
   t_spaces_made = 0;
   t_session(true);
   CHECK(t_binds(frame, "edit/select_next",
            "/user/hand/left/input/dpad_right/click")
         && t_binds(frame, "edit/select_previous",
            "/user/hand/left/input/dpad_left/click")
         && t_binds(frame, "edit/reset",
            "/user/hand/left/input/dpad_down/click"),
         "the Frame's choice is not the left D-pad's right and left");
   CHECK(t_touch_binds("edit/select_next", "/user/hand/right/input/b/click")
         && t_touch_binds("edit/select_previous",
            "/user/hand/left/input/y/click"),
         "Touch's choice is not B and Y");
   CHECK(t_binds(index, "edit/select_next", "/user/hand/right/input/b/click")
         && t_binds(index, "edit/select_previous",
            "/user/hand/left/input/b/click"),
         "Index's choice is not the right and the left B");
   CHECK(t_binds(vive, "edit/select_next", "/user/hand/right/input/menu/click")
         && !t_binds(vive, "edit/select_previous", NULL),
         "the Vive's choice is not the right menu button alone");
   CHECK(!t_binds(simple, "edit/select_next", NULL)
         && !t_binds(simple, "edit/select_previous", NULL),
         "the simple controller has a choice");
   t_hooks.session_destroying(t_hooks.user);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_forget_devices();
}

/* Select Next steps Laser, the shown screens by number (a stereo pair
 * one, a hidden or fixed screen none), the menu, Laser again; Select
 * Previous the other way; a held button steps once. Without the menu's
 * quad (gl) there is no menu step. */
static void test_edit_select_cycle(void)
{
   input_openxr_edit_t *e = &input_openxr_edit;
   t_edit_start();
   CHECK(e->chosen == -1, "edit mode starts on %d, not the laser",
         e->chosen);
   /* Screen 1 a stereo pair; screen 2 hidden (no quad in slot 1);
    * screen 3 shown (slot 2); screen 4 fixed (slot 3). */
   t_quads.quads[0].eye             = VIDEO_XR_EYE_LEFT;
   t_quads.quads[2]                 = t_quads.quads[0];
   t_quads.quads[2].eye             = VIDEO_XR_EYE_RIGHT;
   t_quads.quads[3]                 = t_quads.quads[0];
   t_quads.quads[3].eye             = VIDEO_XR_EYE_BOTH;
   t_quads.quads[3].slot            = 2;
   t_quads.quads[3].pose.position.x = -2.0f;
   t_quads.quads[4]                 = t_quads.quads[3];
   t_quads.quads[4].slot            = 3;
   t_quads.quads[4].pose.position.x = -4.0f;
   t_quads.slots[3].fixed           = true;
   t_quads.num_quads                = 5;
   t_edit_select(1);
   CHECK(e->chosen == 0, "next from the laser: %d", e->chosen);
   t_edit_select(1);
   CHECK(e->chosen == 2, "next from screen 1: %d", e->chosen);
   t_edit_select(1);
   CHECK(e->chosen == VIDEO_XR_MENU_SLOT, "next from screen 3: %d",
         e->chosen);
   t_edit_select(1);
   CHECK(e->chosen == -1, "next from the menu: %d", e->chosen);
   t_edit_select(-1);
   CHECK(e->chosen == VIDEO_XR_MENU_SLOT, "previous from the laser: %d",
         e->chosen);
   t_edit_select(-1);
   CHECK(e->chosen == 2, "previous from the menu: %d", e->chosen);
   t_edit_select(-1);
   CHECK(e->chosen == 0, "previous from screen 3: %d", e->chosen);
   t_edit_select(-1);
   CHECK(e->chosen == -1, "previous from screen 1: %d", e->chosen);
   t_hold("edit/select_next", 0, 1.0f, 0.0f);
   t_edit_poll();
   t_edit_poll();
   CHECK(e->chosen == 0, "a held Select Next stepped to %d", e->chosen);
   t_let_go();
   t_edit_poll();
   /* No menu quad: screen 3 steps back to the laser. */
   t_quads.quads[1] = t_quads.quads[2];
   t_edit_select(1);
   CHECK(e->chosen == 2, "next from screen 1 without a menu: %d",
         e->chosen);
   t_edit_select(1);
   CHECK(e->chosen == -1, "next from screen 3 without a menu: %d",
         e->chosen);
   t_edit_end();
}

/* A chosen item gone from the cycle falls back to the laser; edit mode
 * ending forgets the choice, for the frame loop too. */
static void test_edit_select_fallback(void)
{
   input_openxr_edit_t *e = &input_openxr_edit;
   t_edit_start();
   t_edit_buried();
   t_edit_select(1);
   t_edit_select(1);
   CHECK(e->chosen == 1 && t_grabs.chosen == 2,
         "screen 2 not chosen: %d, published %u", e->chosen,
         t_grabs.chosen);
   t_quads.num_quads--;
   t_edit_poll();
   CHECK(e->chosen == -1 && t_grabs.chosen == 0,
         "hidden, screen 2 stayed chosen");
   t_edit_buried();
   t_edit_select(-1);
   CHECK(e->chosen == VIDEO_XR_MENU_SLOT, "previous chose %d", e->chosen);
   input_openxr_edit_toggle();
   CHECK(!t_editing && t_grabs.chosen == 0,
         "edit mode ended with %u published", t_grabs.chosen);
   t_edit_on(false);
   t_edit_poll();
   CHECK(e->chosen == -1, "a new edit mode starts on %d", e->chosen);
   t_edit_end();
}

/* Screen 2 chosen behind screen 1: the grip takes it from where the
 * hand is and moves it as on a stick; the other hand takes what its
 * laser is on meanwhile; let go, it stays chosen; Reset resets it, not
 * what the laser is on. */
static void test_edit_select_grab(void)
{
   input_openxr_edit_t *e   = &input_openxr_edit;
   const video_xr_grab_t *l = &e->hand[0].g;
   const video_xr_grab_t *r = &e->hand[1].g;
   t_edit_start();
   t_edit_buried();
   t_edit_select(1);
   t_edit_select(1);
   /* The right hand 0.5 right, 0.2 up, turned 30 degrees left: its
    * laser on screen 1. */
   t_aim[2].position.x    = 0.5f;
   t_aim[2].position.y    = 0.2f;
   t_aim[2].orientation.y = (float)sin(M_PI / 12.0);
   t_aim[2].orientation.w = (float)cos(M_PI / 12.0);
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(r->state == VIDEO_XR_GRAB_HELD && r->slot == 1,
         "the grip took slot %u in state %u", r->slot, r->state);
   CHECK(!t_modified, "the grab itself moved something");
   t_aim[2].position.x = 0.6f;
   t_edit_poll();
   CHECK(t_near(t_xr[1].x, 0.1f) && t_near(t_xr[1].y, 0.0f)
         && t_near(t_xr[1].z, 0.0f) && t_near(t_xr[0].x, 0.0f),
         "screen 2 at %.3f %.3f %.3f, screen 1 at %.3f", t_xr[1].x,
         t_xr[1].y, t_xr[1].z, t_xr[0].x);
   /* The hand turns back straight: screen 2 swings round it. */
   t_aim[2].orientation.y = 0.0f;
   t_aim[2].orientation.w = 1.0f;
   t_edit_poll();
   CHECK(t_near(t_xr[1].x, 1.2169873f) && t_near(t_xr[1].z, 0.0313467f)
         && t_near(t_xr[1].yaw, -30.0f),
         "turned with the hand to %.4f, %.4f, yaw %.2f", t_xr[1].x,
         t_xr[1].z, t_xr[1].yaw);
   /* The left hand, its laser on screen 1, takes screen 1. */
   t_hold("edit/grab", 1, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(l->state == VIDEO_XR_GRAB_HELD && l->slot == 0,
         "the free hand took slot %u in state %u", l->slot, l->state);
   t_let_go();
   t_edit_poll();
   CHECK(e->chosen == 1, "letting go forgot the choice: %d", e->chosen);
   /* Reset with the right hand's laser on screen 1. */
   t_xr[0].x = 0.4f;
   t_hold("edit/reset", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(t_near(t_xr[1].x, 0.0f) && t_near(t_xr[1].yaw, 0.0f)
         && t_near(t_xr[0].x, 0.4f),
         "Reset: screen 2 at %.3f, screen 1 at %.3f", t_xr[1].x,
         t_xr[0].x);
   t_edit_end();
}

/* Screen 2 chosen: an untracked hand's grip takes nothing; both grips
 * in one poll, the left hand's first takes it and the right then takes
 * what its laser is on. */
static void test_edit_select_grips(void)
{
   input_openxr_edit_t *e   = &input_openxr_edit;
   const video_xr_grab_t *l = &e->hand[0].g;
   const video_xr_grab_t *r = &e->hand[1].g;
   t_edit_start();
   t_edit_buried();
   t_edit_select(1);
   t_edit_select(1);
   t_aim_tracked[2] = false;
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(r->state != VIDEO_XR_GRAB_HELD && !t_modified,
         "untracked, the grip took slot %u in state %u", r->slot, r->state);
   t_let_go();
   t_edit_poll();
   t_aim_tracked[2] = true;
   t_hold("edit/grab", 1, 1.0f, 0.0f);
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(     l->state == VIDEO_XR_GRAB_HELD && l->slot == 1
         && r->state == VIDEO_XR_GRAB_HELD && r->slot == 0,
         "both grips: the left took slot %u (%u), the right %u (%u)",
         l->slot, l->state, r->slot, r->state);
   CHECK(e->chosen == 1, "the grips changed the choice to %d", e->chosen);
   t_edit_end();
}

/* A held item is bright wherever the lasers are: screen 1 held with the
 * laser turned off it, as a snap can slide it; with screen 2 chosen,
 * screen 1 held by the other hand too. */
static void test_edit_held_lit(void)
{
   unsigned n;
   video_xr_quad_set_t shown;
   const XrCompositionLayerBaseHeader *layers[OPENXR_MAX_EXTRA_LAYERS];
   XrSpace local;
   t_edit_start();
   t_edit_buried();
   local            = t_xr_session.local_space;
   t_cursor         = true;
   t_aim_tracked[1] = false;
   shown            = t_quads;
   t_xr_frame.grabs = t_grabs;
   t_xr_frame.grabs.hand[1].state = VIDEO_XR_GRAB_HELD;
   t_xr_frame.grabs.hand[1].slot  = 0;
   /* The right hand's laser turned up, off everything. */
   t_aim[2].orientation.x = (float)sin(M_PI / 4.0);
   t_aim[2].orientation.w = (float)cos(M_PI / 4.0);
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   CHECK(t_lit(layers, n) == 1u, "held off its laser, %#x lit",
         t_lit(layers, n));
   t_xr_frame.grabs.hand[1].state = VIDEO_XR_GRAB_NONE;
   t_edit_select(1);
   t_edit_select(1);
   t_xr_frame.grabs = t_grabs;
   t_xr_frame.grabs.hand[0].state = VIDEO_XR_GRAB_HELD;
   t_xr_frame.grabs.hand[0].slot  = 0;
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   CHECK(t_lit(layers, n) == 3u, "screen 2 chosen, screen 1 held, %#x lit",
         t_lit(layers, n));
   memset(&t_xr_frame.grabs, 0, sizeof(t_xr_frame.grabs));
   t_cursor = false;
   t_edit_end();
}

/* The frame loop's outlines: with an item chosen it alone is bright,
 * wherever the lasers are; while one hand holds it, what the other
 * points at is bright too; on the laser, what the lasers are on. */
static void test_edit_select_lit(void)
{
   unsigned n;
   video_xr_quad_set_t shown;
   const XrCompositionLayerBaseHeader *layers[OPENXR_MAX_EXTRA_LAYERS];
   XrSpace local;
   t_edit_start();
   t_edit_buried();
   local            = t_xr_session.local_space;
   t_cursor         = true;
   t_aim_tracked[1] = false;
   shown            = t_quads;
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   CHECK(t_lit(layers, n) == 1u, "the laser on screen 1 lit %#x",
         t_lit(layers, n));
   t_edit_select(1);
   t_edit_select(1);
   t_xr_frame.grabs = t_grabs;
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   CHECK(t_lit(layers, n) == 2u, "screen 2 chosen, %#x lit",
         t_lit(layers, n));
   t_xr_frame.grabs.hand[0].state = VIDEO_XR_GRAB_HELD;
   t_xr_frame.grabs.hand[0].slot  = 1;
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   CHECK(t_lit(layers, n) == 3u, "the left holding it, %#x lit",
         t_lit(layers, n));
   t_xr_frame.grabs.hand[0].state = VIDEO_XR_GRAB_NONE;
   t_xr_frame.grabs.hand[1].state = VIDEO_XR_GRAB_HELD;
   t_xr_frame.grabs.hand[1].slot  = 1;
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   CHECK(t_lit(layers, n) == 2u, "the right holding it, %#x lit",
         t_lit(layers, n));
   memset(&t_xr_frame.grabs, 0, sizeof(t_xr_frame.grabs));
   t_cursor = false;
   t_edit_end();
}

/* Screen 2, 0.8 x 0.6, at its spot 1.245 m right of screen 1's middle,
 * 4.5 cm off standing flush beside it: inside the edge's 3-6 cm fade,
 * where a whole pull would move it 1.5 cm. */
static void t_snap_beside(void)
{
   video_xr_quad_t *q = &t_quads.quads[t_quads.num_quads++];
   memset(q, 0, sizeof(*q));
   video_xr_pose_identity(&q->pose);
   q->pose.position.x      = 1.245f;
   q->pose.position.z      = -1.8f;
   q->width                = 0.8f;
   q->height               = 0.6f;
   q->kind                 = VIDEO_XR_QUAD_SCREEN;
   q->screen               = 1;
   q->slot                 = 1;
   t_quads.slots[1].base.x = 1.245f;
   t_quads.slots[1].base.z = -1.8f;
}

/* Snap to Screens on the main thread: a screen taken near a line
 * stays put; carried 5 cm or more it snaps to one no hand holds and
 * its settings say so; the hand buzzes once as a snap takes hold, not
 * while it holds, and again as one takes hold anew; the frame loop is
 * told the setting and where the screen was taken. Off, nothing snaps
 * or buzzes. */
static void test_edit_snap(void)
{
   const video_xr_grab_t *g = &input_openxr_edit.hand[1].g;
   unsigned n;
   t_snap = true;
   t_edit_start();
   CHECK(t_grabs.snap, "the grabs record does not carry Snap to Screens");
   t_snap_beside();
   t_aim[2].position.x = 1.245f;
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(g->state == VIDEO_XR_GRAB_HELD && g->slot == 1,
         "took slot %u in state %u", g->slot, g->state);
   CHECK(t_near(t_grabs.start[1].position.x, 1.245f)
         && t_near(t_grabs.start[1].position.z, -1.8f),
         "the grabs record has it taken at %.4f %.4f",
         t_grabs.start[1].position.x, t_grabs.start[1].position.z);
   n = t_applies[2];
   t_edit_poll();
   CHECK(t_near(t_xr[1].x, 0.0f) && t_applies[2] == n,
         "taken and held still, it hopped to %.4f, %u buzzes", t_xr[1].x,
         t_applies[2] - n);
   /* 5.3 cm from where it was taken: whole. */
   t_aim[2].position.x = 1.205f;
   t_aim[2].position.y = 0.025f;
   t_aim[2].position.z = 0.025f;
   t_edit_poll();
   CHECK(t_near(t_xr[1].x, -0.045f) && t_near(t_xr[1].y, 0.0f)
         && t_near(t_xr[1].z, 0.0f),
         "snapped to %.4f %.4f %.4f", t_xr[1].x, t_xr[1].y, t_xr[1].z);
   CHECK(t_applies[2] == n + 1
         && t_near(t_amp[2], INPUT_OPENXR_SNAP_TICK_AMP),
         "%u buzzes as it took hold, at %.2f", t_applies[2] - n, t_amp[2]);
   t_aim[2].position.x = 1.20f;
   t_edit_poll();
   CHECK(t_near(t_xr[1].x, -0.045f) && t_applies[2] == n + 1,
         "held on the line: at %.4f, %u buzzes", t_xr[1].x,
         t_applies[2] - n);
   t_aim[2].position.x = 1.30f;
   t_edit_poll();
   CHECK(t_near(t_xr[1].x, 0.055f) && t_near(t_xr[1].y, 0.0f)
         && t_applies[2] == n + 1,
         "off the edge: at %.4f, %u buzzes", t_xr[1].x, t_applies[2] - n);
   t_aim[2].position.x = 1.205f;
   t_edit_poll();
   CHECK(t_applies[2] == n + 2, "back on the edge: %u buzzes",
         t_applies[2] - n);
   t_edit_end();

   t_snap = false;
   t_edit_start();
   CHECK(!t_grabs.snap, "the grabs record says snap with it off");
   t_snap_beside();
   t_aim[2].position.x = 1.245f;
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   n                   = t_applies[2];
   t_aim[2].position.x = 1.205f;
   t_aim[2].position.y = 0.025f;
   t_aim[2].position.z = 0.025f;
   t_edit_poll();
   CHECK(t_near(t_xr[1].x, -0.04f) && t_near(t_xr[1].y, 0.025f)
         && t_near(t_xr[1].z, 0.025f) && t_applies[2] == n,
         "off: at %.4f %.4f %.4f, %u buzzes", t_xr[1].x, t_xr[1].y,
         t_xr[1].z, t_applies[2] - n);
   t_edit_end();
}

/* Snapping on, each hand holding a screen: neither snaps to the other,
 * whichever hand carries its screen to it; the frame loop is told both
 * holds. */
static void test_edit_snap_two_hands(void)
{
   const video_xr_grab_t *l = &input_openxr_edit.hand[0].g;
   const video_xr_grab_t *r = &input_openxr_edit.hand[1].g;
   t_snap = true;
   t_edit_start();
   t_snap_beside();
   t_aim[2].position.x = 1.245f;
   t_hold("edit/grab", 1, 1.0f, 0.0f);
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(     l->state == VIDEO_XR_GRAB_HELD && l->slot == 0
         && r->state == VIDEO_XR_GRAB_HELD && r->slot == 1
         && t_grabs.snap && t_grabs.hand[0].state == VIDEO_XR_GRAB_HELD
         && t_grabs.hand[1].state == VIDEO_XR_GRAB_HELD,
         "took slots %u and %u", l->slot, r->slot);
   /* Screen 2 carried 5.3 cm, to 4 cm off flush beside screen 1. */
   t_aim[2].position.x = 1.205f;
   t_aim[2].position.y = 0.025f;
   t_aim[2].position.z = 0.025f;
   t_edit_poll();
   CHECK(t_near(t_xr[1].x, -0.04f) && t_near(t_xr[1].y, 0.025f)
         && t_near(t_xr[1].z, 0.025f),
         "screen 2 snapped to a held screen 1: %.4f %.4f %.4f", t_xr[1].x,
         t_xr[1].y, t_xr[1].z);
   /* Screen 1 carried 5.2 cm, 2.5 cm into screen 2's edge. */
   t_aim[1].position.x = 0.03f;
   t_aim[1].position.y = 0.03f;
   t_aim[1].position.z = 0.03f;
   t_edit_poll();
   CHECK(t_near(t_xr[0].x, 0.03f) && t_near(t_xr[0].y, 0.03f)
         && t_near(t_xr[0].z, 0.03f),
         "screen 1 snapped to a held screen 2: %.4f %.4f %.4f", t_xr[0].x,
         t_xr[0].y, t_xr[0].z);
   t_edit_end();
   t_snap = false;
}

/* Snapping on, a held screen 8 degrees short of parallel, turned into
 * the window by the free hand's stick, snaps while the stick turns it,
 * and the holding hand buzzes. */
static void test_edit_turn_snap(void)
{
   unsigned n;
   video_xr_quad_t *q;
   t_snap = true;
   t_edit_start();
   t_snap_beside();
   q                     = &t_quads.quads[t_quads.num_quads - 1];
   q->pose.orientation.y = (float)-sin(M_PI / 45.0);
   q->pose.orientation.w = (float)cos(M_PI / 45.0);
   t_aim[2].position.x   = 1.245f;
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   /* Carried 5.3 cm, so the snaps are whole. */
   t_aim[2].position.x = 1.205f;
   t_aim[2].position.y = 0.025f;
   t_aim[2].position.z = 0.025f;
   t_edit_poll();
   CHECK(!t_near(t_xr[1].yaw, 0.0f), "parallel 8 degrees out");
   n = t_applies[2];
   t_hold("edit/stick", 1, 1.0f, 0.0f);
   t_clock += 100000;
   t_edit_poll();
   CHECK(t_near(t_xr[1].yaw, 0.0f) && t_applies[2] == n + 1,
         "turned in by the stick: yaw %.2f, %u buzzes", t_xr[1].yaw,
         t_applies[2] - n);
   t_edit_end();
   t_snap = false;
}

/* Carried in, each snap buzzes as it takes hold, one still fading
 * staying armed. Resting at the edge of its whole pull, where it takes
 * hold and lets go poll by poll, no more; again only once that axis has
 * stopped pulling at all, or after a new grab. */
static void test_edit_snap_edge(void)
{
   unsigned i, n;
   const input_openxr_hold_t *hold = &input_openxr_edit.hand[1];
   t_snap = true;
   t_edit_start();
   t_snap_beside();
   /* Screen 2 10 cm right of flush, beyond any pull: taken there. */
   t_quads.quads[t_quads.num_quads - 1].pose.position.x = 1.30f;
   t_quads.slots[1].base.x = 1.30f;
   t_aim[2].position.x     = 1.30f;
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(hold->g.state == VIDEO_XR_GRAB_HELD && hold->g.slot == 1
         && hold->armed == VIDEO_XR_SNAP_ALL,
         "took slot %u armed %#x", hold->g.slot, hold->armed);
   n = t_applies[2];
   /* 4.5 cm off flush, 5.5 cm carried: the turn, Y and Z whole, one
    * buzz; the edge fades, still armed. */
   t_aim[2].position.x = 1.245f;
   t_edit_poll();
   CHECK(t_applies[2] == n + 1 && hold->armed == VIDEO_XR_SNAP_X,
         "%u buzzes in the edge's fade, armed %#x", t_applies[2] - n,
         hold->armed);
   /* 2.9 cm off: the edge whole, one more. */
   t_aim[2].position.x = 1.229f;
   t_edit_poll();
   CHECK(t_applies[2] == n + 2, "%u buzzes taking hold",
         t_applies[2] - n);
   /* 3.1 cm off and back again: the edge's whole pull comes and goes. */
   for (i = 0; i < 3; i++)
   {
      t_aim[2].position.x = 1.231f;
      t_edit_poll();
      t_aim[2].position.x = 1.229f;
      t_edit_poll();
   }
   CHECK(t_applies[2] == n + 2, "%u buzzes resting at the edge",
         t_applies[2] - n);
   /* 7 cm off the edge lets go; back on it, one more. */
   t_aim[2].position.x = 1.27f;
   t_edit_poll();
   CHECK(hold->armed & VIDEO_XR_SNAP_X, "the edge not re-armed: %#x",
         hold->armed);
   t_aim[2].position.x = 1.229f;
   t_edit_poll();
   CHECK(t_applies[2] == n + 3, "%u buzzes back on the edge",
         t_applies[2] - n);
   /* A new grab re-arms them all. */
   t_let_go();
   t_edit_poll();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(hold->armed == VIDEO_XR_SNAP_ALL, "armed %#x after a new grab",
         hold->armed);
   t_edit_end();
   t_snap = false;
}

/* Edit Smoothing: the frame loop steps each hand's aim once a frame,
 * starting from the raw aim, whichever hook asks first, and publishes
 * it; losing the hand, a recenter or a jump start it afresh. The main
 * thread grabs and lets go from the published aim, so a screen is saved
 * where the frame loop last showed it. Off: the raw aim, nothing
 * published. */
static void test_edit_smoothing(void)
{
   unsigned n;
   video_xr_pose_t a, b, want;
   video_xr_grab_t held;
   video_screen_xr_t s;
   video_xr_quad_set_t set;
   const XrCompositionLayerBaseHeader *layers[OPENXR_MAX_EXTRA_LAYERS];
   XrSpace local            = XR_NULL_HANDLE;
   const video_xr_grab_t *g = &input_openxr_edit.hand[1].g;

   t_smooth = VIDEO_OPENXR_EDIT_SMOOTHING_MEDIUM;
   t_edit_start();
   local   = t_xr_session.local_space;
   t_time += 11111111;
   CHECK(t_hooks.aim(t_hooks.user, 1, local, t_time, &a)
         && a.position.x == 0.0f, "the first frame's aim at %.4f",
         a.position.x);
   /* The hand 1 cm right: the next frame's aim goes part way, and the
    * layers in that frame get the same, not a second step. */
   t_aim[2].position.x = 0.01f;
   t_time += 11111111;
   CHECK(t_hooks.aim(t_hooks.user, 1, local, t_time, &a)
         && a.position.x > 0.0005f && a.position.x < 0.009f,
         "smoothed to %.4f", a.position.x);
   n   = t_aims_published;
   set = t_quads;
   t_hooks.frame_layers(t_hooks.user, t_time, &set, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   t_hooks.aim(t_hooks.user, 1, local, t_time, &b);
   CHECK(b.position.x == a.position.x && t_aims_published == n + 1
         && (t_aims.valid & 3u) == 3u
         && t_aims.pose[1].position.x == a.position.x,
         "stepped again to %.4f, %u publishes, valid %#x",
         b.position.x, t_aims_published - n, t_aims.valid);
   /* The main thread grabs from the published aim. */
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(g->state == VIDEO_XR_GRAB_HELD
         && t_near(g->rel.position.x, -a.position.x),
         "taken from %.4f", -g->rel.position.x);
   /* The hand moves on: what the frame loop shows is what is let go. */
   t_aim[2].position.x = 0.05f;
   t_time += 11111111;
   t_hooks.aim(t_hooks.user, 1, local, t_time, &a);
   held = *g;
   video_xr_grab_pose(&held, &a, NULL, 0, NULL, &want, &s, NULL);
   t_let_go();
   t_edit_poll();
   CHECK(t_near(t_xr[0].x, s.x) && t_xr[0].x < 0.045f,
         "let go at %.4f, shown at %.4f", t_xr[0].x, s.x);
   /* Lost, then back 10 cm on: the raw aim at once. */
   t_aim_tracked[2] = false;
   t_time += 11111111;
   CHECK(!t_hooks.aim(t_hooks.user, 1, local, t_time, &a)
         && !(t_aims.valid & 2u), "untracked, still published");
   t_aim_tracked[2]    = true;
   t_aim[2].position.x = 0.15f;
   t_time += 11111111;
   t_hooks.aim(t_hooks.user, 1, local, t_time, &a);
   CHECK(a.position.x == 0.15f, "back at %.4f", a.position.x);
   /* A recenter: afresh. */
   t_aim[2].position.x = 0.16f;
   t_recenters++;
   t_time += 11111111;
   t_hooks.aim(t_hooks.user, 1, local, t_time, &a);
   CHECK(a.position.x == 0.16f, "after a recenter at %.4f",
         a.position.x);
   /* 0.6 m in a frame: afresh. */
   t_aim[2].position.x = 0.76f;
   t_time += 11111111;
   t_hooks.aim(t_hooks.user, 1, local, t_time, &a);
   CHECK(a.position.x == 0.76f, "after a jump at %.4f", a.position.x);
   t_edit_end();

   /* Past High, as a config may hold: Medium. */
   t_smooth = VIDEO_OPENXR_EDIT_SMOOTHING_LAST;
   t_edit_start();
   t_time += 11111111;
   t_hooks.aim(t_hooks.user, 1, local, t_time, &a);
   t_aim[2].position.x = 0.01f;
   t_time += 11111111;
   CHECK(t_hooks.aim(t_hooks.user, 1, local, t_time, &a)
         && a.position.x > 0.0005f && a.position.x < 0.009f
         && (t_aims.valid & 2u), "past High: at %.4f, valid %#x",
         a.position.x, t_aims.valid);
   t_edit_end();

   /* Off: the raw aim, nothing published. */
   t_smooth = VIDEO_OPENXR_EDIT_SMOOTHING_OFF;
   t_edit_start();
   n                   = t_aims_published;
   t_aim[2].position.x = 0.01f;
   t_time += 11111111;
   CHECK(t_hooks.aim(t_hooks.user, 1, local, t_time, &a)
         && a.position.x == 0.01f && t_aims_published == n,
         "off: at %.4f, %u publishes", a.position.x, t_aims_published - n);
   t_edit_end();
}

/* Let go with the hand still moving: the release poll's own aim puts
 * the item, so the frame loop, which showed it from each frame's aim,
 * shows no step back to the poll before; the stick moves nothing then.
 * Untracked at the release, it stays where the last poll put it. */
static void test_edit_release_moving(void)
{
   t_edit_start();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   t_aim[2].position.x = 0.3f;
   t_edit_poll();
   t_let_go();
   t_hold("edit/stick", 2, 1.0f, 1.0f);
   t_aim[2].position.x = 0.5f;
   t_edit_poll();
   CHECK(     t_grabs.let_go[0].state == VIDEO_XR_GRAB_RELEASED
         && t_near(t_grabs.let_go[0].last.position.x, 0.5f)
         && t_near(t_grabs.let_go[0].last.position.z, -1.8f)
         && t_near(t_grabs.let_go[0].width, 1.6f)
         && t_near(t_xr[0].x, 0.5f) && t_near(t_xr[0].z, 0.0f)
         && t_near(t_xr[0].scale, 1.0f),
         "let go moving: at %.3f, saved x %.3f z %.3f scale %.3f",
         t_grabs.let_go[0].last.position.x, t_xr[0].x, t_xr[0].z,
         t_xr[0].scale);
   t_let_go();
   t_aim[2].position.x = 0.0f;
   t_edit_poll();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   t_aim[2].position.x = 0.4f;
   t_edit_poll();
   t_let_go();
   t_aim_tracked[2]    = false;
   t_aim[2].position.x = 0.6f;
   t_edit_poll();
   CHECK(     t_grabs.let_go[0].state == VIDEO_XR_GRAB_RELEASED
         && t_near(t_grabs.let_go[0].last.position.x, 0.4f)
         && t_near(t_xr[0].x, 0.4f),
         "let go untracked: at %.3f, saved %.3f",
         t_grabs.let_go[0].last.position.x, t_xr[0].x);
   t_edit_end();
}

/* A hand lets go and grips the same item again before quads planned at
 * the release come: it takes the item where the headset shows it, where
 * the hand left it, not where the last quads had it. Once they come,
 * the let-go hold is theirs: a reset since shows. */
static void test_edit_regrab(void)
{
   const video_xr_grab_t *g = &input_openxr_edit.hand[1].g;
   t_edit_start();
   t_quads_seq    = t_edit_seq;
   t_quads_behind = true;
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   t_aim[2].position.x = 0.3f;
   t_edit_poll();
   t_let_go();
   t_edit_poll();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(     g->state == VIDEO_XR_GRAB_HELD && g->slot == 0
         && t_near(g->last.position.x, 0.3f)
         && t_near(t_grabs.hand[1].last.position.x, 0.3f),
         "regrabbed at %.3f, not where it was let go", g->last.position.x);
   t_edit_poll();
   CHECK(t_near(t_xr[0].x, 0.3f), "the regrab moved it to %.3f", t_xr[0].x);
   t_let_go();
   t_edit_poll();
   /* The quads catch up, screen 1 since reset: they show it. */
   t_quads_behind = false;
   config_set_video_screen_xr(0, NULL);
   t_aim[2].position.x = 0.0f;
   t_edit_poll();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(g->state == VIDEO_XR_GRAB_HELD && t_near(g->last.position.x, 0.0f),
         "caught up, the let-go hold still applied: at %.3f",
         g->last.position.x);
   t_edit_end();
}

/* The simple controller: grab and the menu, no tilt, stick or reset;
 * Follow Head locates the hands in VIEW, as the screens are. */
static void test_edit_simple_and_view(void)
{
   const char *simple = "/interaction_profiles/khr/simple_controller";
   t_edit_start();
   CHECK(t_binds(simple, "edit/grab", "/user/hand/right/input/select/click")
         && t_binds(simple, "edit/menu", "/user/hand/left/input/menu/click")
         && !t_binds(simple, "edit/tilt", NULL)
         && !t_binds(simple, "edit/stick", NULL)
         && !t_binds(simple, "edit/reset", NULL),
         "the simple controller's edit bindings");
   t_space = t_xr_session.view_space;
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(t_located_base == t_xr_session.view_space,
         "the aim was not located where the screens are");
   t_aim[2].position.x = 0.1f;
   t_edit_poll();
   CHECK(t_near(t_xr[0].x, 0.1f), "grab only: %.3f", t_xr[0].x);
   t_edit_end();
}

/* Fixed in Room: a hand in the room moves an item in the anchor's
 * frame. The anchor turned a quarter left, a hand moved 20 cm along the
 * room's +Z moves screen 1 20 cm to its left. */
static void test_edit_room(void)
{
   t_edit_start();
   t_screens.orientation.y = 0.70710678f;
   t_screens.orientation.w = 0.70710678f;
   t_aim[2].orientation.y  = 0.70710678f;
   t_aim[2].orientation.w  = 0.70710678f;
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(input_openxr_edit.hand[1].g.state == VIDEO_XR_GRAB_HELD
         && t_located_base == t_xr_session.local_space,
         "the hand aiming where the anchor looks took nothing");
   t_aim[2].position.z = 0.2f;
   t_edit_poll();
   CHECK(t_near(t_xr[0].x, -0.2f) && t_near(t_xr[0].z, 0.0f),
         "moved to %.3f %.3f", t_xr[0].x, t_xr[0].z);
   t_edit_end();
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
   test_edit_mode();
   test_edit_trigger_held();
   test_edit_unavailable();
   test_edit_profile_refused();
   test_edit_pause_refused();
   test_edit_repause();
   test_edit_unbound();
   test_edit_oversized();
   test_edit_pause();
   test_edit_grab();
   test_edit_turn();
   test_edit_tilt_from_grab();
   test_edit_limits();
   test_edit_spot_moved();
   test_edit_two_hands();
   test_edit_menu_handle();
   test_edit_reset();
   test_edit_held_at_start();
   test_edit_recenter();
   test_edit_held_moves();
   test_edit_stereo();
   test_edit_fallback_frame();
   test_edit_simple_and_view();
   test_edit_hold_record();
   test_edit_release_moving();
   test_edit_regrab();
   test_edit_frame_aim();
   test_edit_outlines();
   test_edit_handle_sticky();
   test_edit_beams();
   test_edit_select_bindings();
   test_edit_select_cycle();
   test_edit_select_fallback();
   test_edit_select_grab();
   test_edit_select_grips();
   test_edit_held_lit();
   test_edit_select_lit();
   test_edit_snap();
   test_edit_snap_two_hands();
   test_edit_turn_snap();
   test_edit_snap_edge();
   test_edit_smoothing();
   test_edit_room();
   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("PASS openxr_input_test\n");
   return 0;
}
