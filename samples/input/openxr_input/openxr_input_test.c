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

/* Each hand's haptics: the last amplitude, 0 once stopped, and length,
 * how many were applied, and how many ran longer than a snap's tick,
 * T_TICK_NS (INPUT_OPENXR_SNAP_TICK_NS, pinned in test_edit_hinge_buzz). */
#define T_TICK_NS 20000000
static float      t_amp[3];
static XrDuration t_dur[3];
static unsigned   t_applies[3];
static unsigned   t_longs[3];
static XRAPI_ATTR XrResult XRAPI_CALL t_apply(XrSession s,
      const XrHapticActionInfo *hi, const XrHapticBaseHeader *h)
{
   const XrHapticVibration *v = (const XrHapticVibration*)h;
   unsigned hand              = t_hand(hi->subactionPath);
   t_amp[hand] = v->amplitude;
   t_dur[hand] = v->duration;
   t_applies[hand]++;
   if (v->duration > T_TICK_NS)
      t_longs[hand]++;
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
/* The frame the main thread locates at: a new one as t_clock moves, as
 * the frame loop runs alongside. */
XrTime openxr_frame_predicted_time(openxr_frame_t *f)
{
   return t_time + (XrTime)t_clock * 1000;
}
/* The headset's display period as published: 90 Hz unless a case sets
 * it. */
static XrDuration t_period = 11111111;
XrDuration openxr_frame_period(openxr_frame_t *f) { return t_period; }
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
/* Its width: with the cut outlines, unless a case narrows it. */
static unsigned t_cursor_w = VIDEO_XR_CURSOR_W;
unsigned openxr_frame_cursor_w(const openxr_frame_t *f)
{
   return t_cursor_w;
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

/* Touch's left menu button, which opens the menu from edit mode, held
 * on as edit mode ends: spent, it is no Start and toggles no menu again
 * until let go; held anew it opens the menu as before. */
static void test_start_menu_after_edit(void)
{
   t_menu_open = false;
   t_let_go();
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   input_openxr_edit_begin(false, false, 0, 0, 0);
   t_hold("edit/menu", 0, 1.0f, 0.0f);
   t_hold("combined/start_menu", 0, 1.0f, 0.0f);
   t_clock = 70000000;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_btn(3, RARCH_MENU_TOGGLE), "edit mode's menu press opens no menu");
   t_menu_open = true;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(!t_editing, "edit mode went on with the menu open");
   t_clock += 1500000;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(     !t_btn(0, RARCH_MENU_TOGGLE)
         && !t_btn(0, RETRO_DEVICE_ID_JOYPAD_START),
         "the press held from edit mode: menu %d, Start %d",
         t_btn(0, RARCH_MENU_TOGGLE),
         t_btn(0, RETRO_DEVICE_ID_JOYPAD_START));
   t_let_go();
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   t_hold("combined/start_menu", 0, 1.0f, 0.0f);
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_btn(0, RETRO_DEVICE_ID_JOYPAD_START), "a new press is no Start");
   t_clock += 1000000;
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_OFF);
   CHECK(t_btn(0, RARCH_MENU_TOGGLE), "a new second's hold opens no menu");
   t_let_go();
   t_menu_open = false;
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
static unsigned t_angles = VIDEO_OPENXR_EDIT_SNAP_ANGLES_15;
/* Hinge Gap, as edit mode starts: touching unless a case says. */
static unsigned t_gap = VIDEO_OPENXR_EDIT_HINGE_GAP_0;
static unsigned t_smooth;

/* As the frontend's command does it, between frames, with the menu
 * closed. */
static void t_edit_on(bool pause)
{
   if (input_openxr_edit_toggle())
      input_openxr_edit_begin(pause, t_snap, t_angles, t_gap, t_smooth);
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
   input_openxr_edit_begin(false, false, 0, 0, 0);
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
   input_openxr_edit_begin(false, false, 0, 0, 0);
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

/* t_clock's step past a hinge's buzz (INPUT_OPENXR_HINGE_BUZZ_NS), us:
 * a tick a case means after an attach lands outside it, and no case
 * starts inside the last one's. */
#define T_BUZZ_CLEAR 200000

/* A fresh session in edit mode, started as the frontend's command
 * starts it. */
static void t_edit_start(void)
{
   t_clock += T_BUZZ_CLEAR;
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

/* With Edit Smoothing on, the frame at t_clock: the right hand's aim
 * stepped for it and published as the frame loop does, into *a. */
static bool t_frame_aim(video_xr_pose_t *a)
{
   return t_hooks.aim(t_hooks.user, 1, t_xr_session.local_space,
         openxr_frame_predicted_time(&t_xr_frame), a);
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

/* The free hand's stick up tips what the other hand holds back, its
 * top away, about its own left-right line through its middle, as fast
 * as it turns it, the trigger held or not; left and right turn it at
 * once. The holding hand's own stick up still pushes and tips nothing. */
static void test_edit_tilt_stick(void)
{
   t_edit_start();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   /* The left stick up for 0.1 s: nine degrees back, in place. */
   t_hold("edit/stick", 1, 0.0f, 1.0f);
   t_clock += 100000;
   t_edit_poll();
   CHECK(t_near(t_xr[0].pitch, -9.0f) && t_near(t_xr[0].yaw, 0.0f)
         && t_near(t_xr[0].roll, 0.0f) && t_near(t_xr[0].x, 0.0f)
         && t_near(t_xr[0].y, 0.0f) && t_near(t_xr[0].z, 0.0f),
         "tilted to pitch %.2f yaw %.2f roll %.2f at %.3f,%.3f,%.3f",
         t_xr[0].pitch, t_xr[0].yaw, t_xr[0].roll, t_xr[0].x, t_xr[0].y,
         t_xr[0].z);
   /* The holding hand's own stick up pushes it and tips nothing. */
   t_let_go();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_hold("edit/stick", 2, 0.0f, 1.0f);
   t_clock += 100000;
   t_edit_poll();
   CHECK(t_near(t_xr[0].pitch, -9.0f) && t_near(t_xr[0].z, -0.15f),
         "own stick: pitch %.2f, pushed to %.3f", t_xr[0].pitch,
         t_xr[0].z);
   /* The trigger held, the left stick down and right: tipped back up
    * and turned, in place. */
   t_let_go();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_hold("edit/tilt", 2, 1.0f, 0.0f);
   t_hold("edit/stick", 1, 1.0f, -1.0f);
   t_clock += 100000;
   t_edit_poll();
   CHECK(t_near(t_xr[0].pitch, 0.0f) && t_near(t_xr[0].yaw, 9.0f)
         && t_near(t_xr[0].roll, 0.0f) && t_near(t_xr[0].x, 0.0f)
         && t_near(t_xr[0].z, -0.15f),
         "tilting: pitch %.2f yaw %.2f roll %.2f at %.3f,%.3f",
         t_xr[0].pitch, t_xr[0].yaw, t_xr[0].roll, t_xr[0].x, t_xr[0].z);
   t_let_go();
   t_edit_poll();
   t_edit_end();
}

/* Degrees in 0.1 s from a stick axis at v. */
static float t_stick_deg(float v)
{
   double a = (fabs(v) - VIDEO_XR_EDIT_DEADZONE)
      / (1.0 - VIDEO_XR_EDIT_DEADZONE);
   return (a > 0.0) ? (float)(a * VIDEO_XR_EDIT_TURN_RATE * 0.1) : 0.0f;
}

/* The free hand's stick near left or right only turns what the other
 * hand holds, near up or down only tilts it, and on a diagonal does
 * both. The holding hand's own stick 15 degrees off right still pushes
 * as it resizes. */
static void test_edit_stick_lock(void)
{
   float c = (float)cos(M_PI / 12.0);
   float s = (float)sin(M_PI / 12.0);
   float d = (float)cos(M_PI / 4.0);
   float yaw, pitch;
   t_edit_start();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   /* The left stick 15 degrees off right for 0.1 s: turned only. */
   t_hold("edit/stick", 1, c, s);
   t_clock += 100000;
   t_edit_poll();
   yaw = t_stick_deg(c);
   CHECK(fabs(t_xr[0].yaw - yaw) < 0.01 && t_near(t_xr[0].pitch, 0.0f)
         && t_near(t_xr[0].roll, 0.0f),
         "15 off right: yaw %.2f pitch %.2f roll %.2f, want yaw %.2f",
         t_xr[0].yaw, t_xr[0].pitch, t_xr[0].roll, yaw);
   /* 15 degrees off down: tilted only. */
   t_let_go();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_hold("edit/stick", 1, -s, -c);
   t_clock += 100000;
   t_edit_poll();
   pitch = t_stick_deg(c);
   CHECK(fabs(t_xr[0].yaw - yaw) < 0.01
         && fabs(t_xr[0].pitch - pitch) < 0.01
         && t_near(t_xr[0].roll, 0.0f),
         "15 off down: yaw %.2f pitch %.2f roll %.2f, want %.2f %.2f",
         t_xr[0].yaw, t_xr[0].pitch, t_xr[0].roll, yaw, pitch);
   /* Up and right together: both. */
   t_let_go();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_hold("edit/stick", 1, d, d);
   t_clock += 100000;
   t_edit_poll();
   yaw   += t_stick_deg(d);
   pitch -= t_stick_deg(d);
   CHECK(fabs(t_xr[0].yaw - yaw) < 0.01
         && fabs(t_xr[0].pitch - pitch) < 0.01
         && t_near(t_xr[0].roll, 0.0f) && t_near(t_xr[0].z, 0.0f),
         "diagonal: yaw %.2f pitch %.2f roll %.2f, want %.2f %.2f",
         t_xr[0].yaw, t_xr[0].pitch, t_xr[0].roll, yaw, pitch);
   /* The holding hand's own stick 15 degrees off right. */
   t_let_go();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_hold("edit/stick", 2, c, s);
   t_clock += 100000;
   t_edit_poll();
   CHECK(fabs(t_xr[0].z + t_stick_deg(s) / 60.0f) < 1e-3
         && fabs(t_xr[0].scale - pow(2.0, t_stick_deg(c) / 90.0)) < 1e-3
         && fabs(t_xr[0].yaw - yaw) < 0.01,
         "own stick: pushed to %.4f, scale %.4f, yaw %.2f", t_xr[0].z,
         t_xr[0].scale, t_xr[0].yaw);
   t_let_go();
   t_edit_poll();
   t_edit_end();
}

/* n polls 10 ms apart, the right hand gripping and the left stick at
 * x, y: 0.9 degrees each at full travel. */
static void t_step_polls(unsigned n, float x, float y)
{
   unsigned i;
   t_let_go();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_hold("edit/stick", 1, x, y);
   for (i = 0; i < n; i++)
   {
      t_clock += 10000;
      t_edit_poll();
   }
}

static bool t_deg_near(float a, double b)
{
   return fabs((double)a - b) < 0.01;
}

/* The free hand's stick turns and tilts what the other hand holds in
 * 15 degree steps from facing the head and from upright: within 2 of
 * one it holds there, the holding hand buzzing once as it takes hold,
 * and again only once it has left that step. Taken at a step, no buzz
 * there. */
static void test_edit_step_stick(void)
{
   unsigned n;
   t_head_tracked = true;
   t_edit_start();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   n = t_applies[2];
   /* Right from facing the head: held at 0 to 1.8, then on. */
   t_step_polls(2, 1.0f, 0.0f);
   CHECK(t_deg_near(t_xr[0].yaw, 0.0) && t_applies[2] == n,
         "at 1.8: yaw %.3f, %u buzzes", t_xr[0].yaw, t_applies[2] - n);
   t_step_polls(12, 1.0f, 0.0f);
   CHECK(t_deg_near(t_xr[0].yaw, 12.6) && t_applies[2] == n,
         "at 12.6: yaw %.3f, %u buzzes", t_xr[0].yaw, t_applies[2] - n);
   /* 13.5 to 16.2 at 15, one buzz; 17.1 is past it. */
   t_step_polls(1, 1.0f, 0.0f);
   CHECK(t_deg_near(t_xr[0].yaw, 15.0) && t_applies[2] == n + 1,
         "at 13.5: yaw %.3f, %u buzzes", t_xr[0].yaw, t_applies[2] - n);
   t_step_polls(3, 1.0f, 0.0f);
   CHECK(t_deg_near(t_xr[0].yaw, 15.0) && t_applies[2] == n + 1,
         "at 16.2: yaw %.3f, %u buzzes", t_xr[0].yaw, t_applies[2] - n);
   t_step_polls(1, 1.0f, 0.0f);
   CHECK(t_deg_near(t_xr[0].yaw, 17.1) && t_applies[2] == n + 1,
         "at 17.1: yaw %.3f, %u buzzes", t_xr[0].yaw, t_applies[2] - n);
   /* On to 31.5, back to 28.8, all within 30's hold: one buzz. */
   t_step_polls(16, 1.0f, 0.0f);
   t_step_polls(3, -1.0f, 0.0f);
   CHECK(t_deg_near(t_xr[0].yaw, 30.0) && t_applies[2] == n + 2,
         "at 28.8: yaw %.3f, %u buzzes", t_xr[0].yaw, t_applies[2] - n);
   /* Out of it at 27.9 and back in at 28.8: again. */
   t_step_polls(1, -1.0f, 0.0f);
   CHECK(t_deg_near(t_xr[0].yaw, 27.9) && t_applies[2] == n + 2,
         "at 27.9: yaw %.3f, %u buzzes", t_xr[0].yaw, t_applies[2] - n);
   t_step_polls(1, 1.0f, 0.0f);
   CHECK(t_deg_near(t_xr[0].yaw, 30.0) && t_applies[2] == n + 3,
         "back at 28.8: yaw %.3f, %u buzzes", t_xr[0].yaw,
         t_applies[2] - n);
   /* Up from upright: -13.5 holds at -15 with a buzz, in place. */
   t_step_polls(15, 0.0f, 1.0f);
   CHECK(t_deg_near(t_xr[0].pitch, -15.0) && t_deg_near(t_xr[0].yaw, 30.0)
         && t_near(t_xr[0].x, 0.0f) && t_near(t_xr[0].z, 0.0f)
         && t_applies[2] == n + 4,
         "tilted to -13.5: pitch %.3f yaw %.3f, %u buzzes", t_xr[0].pitch,
         t_xr[0].yaw, t_applies[2] - n);
   t_let_go();
   t_edit_poll();
   t_edit_end();

   /* Carried, nothing steps: turned to 12.6, then the hand 4.4 cm right
    * puts it 14 from facing the head, and it stays at yaw 12.6. */
   t_edit_start();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   t_step_polls(14, 1.0f, 0.0f);
   n = t_applies[2];
   t_let_go();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_aim[2].position.x = (float)(1.8 * tan(1.4 * M_PI / 180.0));
   t_edit_poll();
   CHECK(t_deg_near(t_xr[0].yaw, 12.6) && t_applies[2] == n,
         "carried to 14 from facing: yaw %.3f, %u buzzes", t_xr[0].yaw,
         t_applies[2] - n);
   t_let_go();
   t_edit_poll();
   t_edit_end();

   /* Taken at 0, carried out of its hold and into 15's with the stick
    * idle: the first push holds at 15 with a buzz. */
   t_edit_start();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   n                   = t_applies[2];
   t_aim[2].position.x = (float)(1.8 * tan(8.0 * M_PI / 180.0));
   t_edit_poll();
   t_aim[2].position.x = (float)(1.8 * tan(14.0 * M_PI / 180.0));
   t_edit_poll();
   t_step_polls(1, 1.0f, 0.0f);
   CHECK(t_deg_near(t_xr[0].yaw, 1.0) && t_applies[2] == n + 1,
         "carried to 14, pushed: yaw %.3f, %u buzzes", t_xr[0].yaw,
         t_applies[2] - n);
   t_let_go();
   t_edit_poll();
   t_edit_end();

   /* Tilted by the trigger to 14, it stays there. */
   t_edit_start();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_hold("edit/tilt", 2, 1.0f, 0.0f);
   t_edit_poll();
   n = t_applies[2];
   t_aim[2].orientation.x = (float)sin(7.0 * M_PI / 180.0);
   t_aim[2].orientation.w = (float)cos(7.0 * M_PI / 180.0);
   t_edit_poll();
   CHECK(t_deg_near(t_xr[0].pitch, 14.0) && t_applies[2] == n,
         "tilted by the trigger: pitch %.3f, %u buzzes", t_xr[0].pitch,
         t_applies[2] - n);
   t_let_go();
   t_edit_poll();
   t_edit_end();
   t_head_tracked = false;
}

/* What the stick gave past a step is kept while the stick is out of
 * its deadzone, though a wavering push drops its axis now and then, and
 * let go of as the stick is: held at 0, the stick let go, then pushed
 * back, it leaves after 2. */
static void test_edit_step_waver(void)
{
   unsigned i;
   double on  = 26.0 * M_PI / 180.0;
   double off = 24.0 * M_PI / 180.0;
   t_head_tracked = true;
   t_edit_start();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   t_step_polls(2, 1.0f, 0.0f);
   t_step_polls(1, 0.0f, 0.0f);
   t_step_polls(3, -1.0f, 0.0f);
   CHECK(t_deg_near(t_xr[0].yaw, -2.7), "let go at 1.8, back 2.7: yaw %.3f",
         t_xr[0].yaw);
   t_let_go();
   t_edit_poll();
   t_edit_end();

   /* About the lock line, the tilt acts every other poll. */
   t_edit_start();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   for (i = 0; i < 10; i++)
   {
      t_step_polls(1, (float)cos(on), (float)sin(on));
      t_step_polls(1, (float)cos(off), (float)sin(off));
   }
   CHECK(t_deg_near(t_xr[0].pitch, -9.0 * (sin(on) - 0.2) / 0.8),
         "lock line: pitch %.3f", t_xr[0].pitch);
   t_let_go();
   t_edit_poll();
   t_edit_end();

   /* The turn about its deadzone, a little up, too little to tilt,
    * keeping the stick out of it. */
   t_edit_start();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   for (i = 0; i < 20; i++)
   {
      t_step_polls(1, 0.3f, 0.15f);
      t_step_polls(1, 0.19f, 0.15f);
   }
   CHECK(t_deg_near(t_xr[0].yaw, 18.0 * 0.1 / 0.8) && t_near(t_xr[0].pitch,
            0.0f), "deadzone: yaw %.3f pitch %.3f", t_xr[0].yaw,
         t_xr[0].pitch);
   t_let_go();
   t_edit_poll();
   t_edit_end();
   t_head_tracked = false;
}

/* Snap to Angles sets the step. At 45 the stick turns on past 15 and
 * 30, holds within 2 of 45 with one buzz, leaves it, buzzes again back
 * in its hold, and again at 90. Off, it neither holds nor buzzes,
 * turning or tilting. A config's value past 90 steps at 15. */
static void test_edit_step_sizes(void)
{
   unsigned n;
   t_head_tracked = true;
   t_angles       = VIDEO_OPENXR_EDIT_SNAP_ANGLES_45;
   t_edit_start();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   n = t_applies[2];
   t_step_polls(15, 1.0f, 0.0f);
   CHECK(t_deg_near(t_xr[0].yaw, 13.5) && t_applies[2] == n,
         "45, at 13.5: yaw %.3f, %u buzzes", t_xr[0].yaw, t_applies[2] - n);
   t_step_polls(17, 1.0f, 0.0f);
   CHECK(t_deg_near(t_xr[0].yaw, 28.8) && t_applies[2] == n,
         "45, at 28.8: yaw %.3f, %u buzzes", t_xr[0].yaw, t_applies[2] - n);
   t_step_polls(16, 1.0f, 0.0f);
   CHECK(t_deg_near(t_xr[0].yaw, 45.0) && t_applies[2] == n + 1,
         "45, at 43.2: yaw %.3f, %u buzzes", t_xr[0].yaw, t_applies[2] - n);
   t_step_polls(4, 1.0f, 0.0f);
   CHECK(t_deg_near(t_xr[0].yaw, 45.0) && t_applies[2] == n + 1,
         "45, at 46.8: yaw %.3f, %u buzzes", t_xr[0].yaw, t_applies[2] - n);
   t_step_polls(1, 1.0f, 0.0f);
   CHECK(t_deg_near(t_xr[0].yaw, 47.7) && t_applies[2] == n + 1,
         "45, at 47.7: yaw %.3f, %u buzzes", t_xr[0].yaw, t_applies[2] - n);
   t_step_polls(1, -1.0f, 0.0f);
   CHECK(t_deg_near(t_xr[0].yaw, 45.0) && t_applies[2] == n + 2,
         "45, back at 46.8: yaw %.3f, %u buzzes", t_xr[0].yaw,
         t_applies[2] - n);
   t_step_polls(46, 1.0f, 0.0f);
   CHECK(t_deg_near(t_xr[0].yaw, 90.0) && t_applies[2] == n + 3,
         "45, at 88.2: yaw %.3f, %u buzzes", t_xr[0].yaw, t_applies[2] - n);
   t_let_go();
   t_edit_poll();
   t_edit_end();

   t_angles = VIDEO_OPENXR_EDIT_SNAP_ANGLES_OFF;
   t_edit_start();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   n = t_applies[2];
   t_step_polls(2, 1.0f, 0.0f);
   CHECK(t_deg_near(t_xr[0].yaw, 1.8), "off, at 1.8: yaw %.3f",
         t_xr[0].yaw);
   t_step_polls(13, 1.0f, 0.0f);
   t_step_polls(15, 0.0f, 1.0f);
   CHECK(t_deg_near(t_xr[0].yaw, 13.5) && t_deg_near(t_xr[0].pitch, -13.5)
         && t_applies[2] == n, "off, at 13.5 and -13.5: yaw %.3f pitch %.3f,"
         " %u buzzes", t_xr[0].yaw, t_xr[0].pitch, t_applies[2] - n);
   t_let_go();
   t_edit_poll();
   t_edit_end();

   t_angles = VIDEO_OPENXR_EDIT_SNAP_ANGLES_LAST;
   t_edit_start();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   n = t_applies[2];
   t_step_polls(15, 1.0f, 0.0f);
   CHECK(t_deg_near(t_xr[0].yaw, 15.0) && t_applies[2] == n + 1,
         "past 90, at 13.5: yaw %.3f, %u buzzes", t_xr[0].yaw,
         t_applies[2] - n);
   t_let_go();
   t_edit_poll();
   t_edit_end();
   t_angles       = VIDEO_OPENXR_EDIT_SNAP_ANGLES_15;
   t_head_tracked = false;
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

/* Where l's front faces, along z. */
static float t_front_z(const XrCompositionLayerQuad *l)
{
   video_xr_quat_t o;
   video_xr_vec3_t z;
   o.x = l->pose.orientation.x;
   o.y = l->pose.orientation.y;
   o.z = l->pose.orientation.z;
   o.w = l->pose.orientation.w;
   z.x = 0.0f;
   z.y = 0.0f;
   z.z = 1.0f;
   video_xr_rotate(&o, &z, &z);
   return z.z;
}

/* Items the frame shows by their backs: their outlines, the menu's
 * handle and the dot on one face the head, just in front of the back;
 * a hand behind a screen points at it and grabs it by its back. Out of
 * edit mode the dot is on the back too. */
static void test_edit_backs(void)
{
   unsigned i, n;
   video_xr_quad_set_t shown;
   const XrCompositionLayerQuad *q;
   const XrCompositionLayerBaseHeader *layers[OPENXR_MAX_EXTRA_LAYERS];
   const video_xr_grab_t *g = &input_openxr_edit.hand[1].g;
   XrSpace local            = XR_NULL_HANDLE;
   bool facing              = true;
   t_edit_start();
   local                  = t_xr_session.local_space;
   t_aim_tracked[1]       = false;
   t_cursor               = true;
   shown                  = t_quads;
   /* The right hand 1.2 m behind screen 1, aiming back at it. */
   t_aim[2].position.x    = 0.4f;
   t_aim[2].position.z    = -3.0f;
   t_aim[2].orientation.y = 1.0f;
   t_aim[2].orientation.w = 0.0f;
   t_xr_frame.backs       = 3u;
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   for (i = 0; i < n; i++)
      facing &= t_near(t_front_z((const XrCompositionLayerQuad*)layers[i]),
            -1.0f);
   CHECK(n == 4 && facing, "%u layers from behind, all facing it %d", n,
         (int)facing);
   q = (const XrCompositionLayerQuad*)layers[0];
   CHECK(     q->subImage.imageRect.offset.x == VIDEO_XR_OUTLINE_X(1)
         && t_near(q->pose.position.z, -1.8f - VIDEO_XR_CURSOR_LIFT),
         "screen 1's outline from behind: offset %d at z %.4f",
         (int)q->subImage.imageRect.offset.x, q->pose.position.z);
   q = (const XrCompositionLayerQuad*)layers[2];
   CHECK(     q->subImage.imageRect.offset.x == VIDEO_XR_HANDLE_X(0)
         && t_near(q->pose.position.x, 2.0f)
         && t_near(q->pose.position.y, t_handle_y(0.6f))
         && t_near(q->pose.position.z, -1.7f - VIDEO_XR_CURSOR_LIFT),
         "the menu's handle from behind at %.3f,%.3f,%.4f",
         q->pose.position.x, q->pose.position.y, q->pose.position.z);
   q = (const XrCompositionLayerQuad*)layers[3];
   CHECK(     q->subImage.imageRect.extent.width == VIDEO_XR_CURSOR_DIM
         && t_near(q->pose.position.x, 0.4f)
         && t_near(q->pose.position.z, -1.8f - VIDEO_XR_CURSOR_LIFT),
         "the dot on screen 1's back at %.3f,%.4f", q->pose.position.x,
         q->pose.position.z);
   /* Shown by their fronts, all face the screens' front. */
   t_xr_frame.backs = 0;
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   q = (const XrCompositionLayerQuad*)layers[3];
   CHECK(n == 4 && t_near(t_front_z(q), 1.0f)
         && t_near(q->pose.position.x, 0.4f)
         && t_near(q->pose.position.z, -1.8f + VIDEO_XR_CURSOR_LIFT),
         "fronts: the dot at %.3f,%.4f facing %.2f", q->pose.position.x,
         q->pose.position.z, t_front_z(q));

   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(g->state == VIDEO_XR_GRAB_HELD && g->slot == 0,
         "from behind the grip grabbed %u in state %u", g->slot, g->state);

   /* Out of edit mode, Laser Pointer Always: a hand in front of screen
    * 1 with the head behind it puts the dot on its back. */
   t_let_go();
   input_openxr_edit_toggle();
   t_poll(VIDEO_OPENXR_CONTROLLERS_COMBINED, VIDEO_OPENXR_LASER_ALWAYS);
   t_aim[2].position.z    = 0.0f;
   t_aim[2].orientation.y = 0.0f;
   t_aim[2].orientation.w = 1.0f;
   t_xr_frame.backs       = 1u;
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   q = (const XrCompositionLayerQuad*)layers[0];
   CHECK(n == 1 && t_near(t_front_z(q), -1.0f)
         && t_near(q->pose.position.x, 0.4f)
         && t_near(q->pose.position.z, -1.8f - VIDEO_XR_CURSOR_LIFT),
         "Laser Pointer Always: %u layers, the dot at %.3f,%.4f facing "
         "%.2f", n, q->pose.position.x, q->pose.position.z, t_front_z(q));
   t_xr_frame.backs = 0;
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
   /* Two hands on screen 1, one more layer's room: the left's tag. */
   t_aim_tracked[1]    = true;
   t_aim[1].position.x = -0.2f;
   t_aim[1].position.y = -0.3f;
   n = t_hooks.frame_layers(t_hooks.user, t_time, &t_quads, local, layers,
         OPENXR_KEPT_EXTRA_LAYERS + 4, &t_under);
   q = (const XrCompositionLayerQuad*)layers[3];
   CHECK(n == 6 && q->subImage.imageRect.offset.x == VIDEO_XR_TAG_X(0),
         "two hands, one more layer's room: the tag, beams dropped first: "
         "%u layers", n);
   n = t_hooks.frame_layers(t_hooks.user, t_time, &t_quads, local, layers,
         OPENXR_KEPT_EXTRA_LAYERS + 5, &t_under);
   q = (const XrCompositionLayerQuad*)layers[4];
   CHECK(n == 7 && q->subImage.imageRect.offset.x == VIDEO_XR_TAG_X(0),
         "two hands, two more layers' room: %u layers, want 3 + 1 + 1 + 2",
         n);
   n = t_hooks.frame_layers(t_hooks.user, t_time, &t_quads, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   CHECK(n == 8, "two hands with room: %u layers, want 3 + 2 + 1 + 2", n);
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

/* The tag among layers, and where: the one showing a tag cell. */
static const XrCompositionLayerQuad *t_tag_layer(
      const XrCompositionLayerBaseHeader **layers, unsigned n,
      unsigned *at)
{
   unsigned i;
   for (i = 0; i < n; i++)
   {
      const XrCompositionLayerQuad *q =
         (const XrCompositionLayerQuad*)layers[i];
      if (     q->subImage.imageRect.extent.width == VIDEO_XR_TAG_COLS
            && q->subImage.imageRect.extent.height == VIDEO_XR_TAG_ROWS)
      {
         *at = i;
         return q;
      }
   }
   return NULL;
}

/* Edit mode's tag: over the hand that last pressed Select Next or
 * Previous, the left at each start, showing the chosen item's cell; after
 * the outlines and beams, before the dots; only while that hand and
 * the head are tracked. */
static void test_edit_tag(void)
{
   unsigned n, at, k, beams;
   video_xr_quad_set_t shown;
   const XrCompositionLayerQuad *q;
   const XrCompositionLayerBaseHeader *layers[OPENXR_MAX_EXTRA_LAYERS];
   XrSpace local = XR_NULL_HANDLE;
   t_edit_start();
   local               = t_xr_session.local_space;
   t_cursor            = true;
   t_head_tracked      = true;
   shown               = t_quads;
   t_aim[1].position.x = -0.2f;
   t_aim[1].position.y = -0.3f;
   t_edit_poll();
   CHECK(t_grabs.tag == 0, "the tag starts on hand %u", t_grabs.tag);
   t_xr_frame.grabs = t_grabs;
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   q = t_tag_layer(layers, n, &at);
   CHECK(q && q->subImage.imageRect.offset.x == VIDEO_XR_TAG_X(0)
         && q->subImage.imageRect.offset.y == VIDEO_XR_TAG_Y
         && q->subImage.swapchain == (XrSwapchain)(uintptr_t)0x51
         && t_near(q->pose.position.x, -0.2f)
         && t_near(q->pose.position.y, -0.3f + VIDEO_XR_TAG_LIFT)
         && t_near(q->size.height, VIDEO_XR_TAG_HEIGHT)
         && t_near(q->size.width, VIDEO_XR_TAG_HEIGHT * VIDEO_XR_TAG_COLS
            / VIDEO_XR_TAG_ROWS)
         && at == n - 3,
         "the laser's tag: layer %u of %u", q ? at : 99u, n);
   /* The right hand's Select Next: Screen 1's, over the right. */
   t_hold("edit/select_next", 2, 1.0f, 0.0f);
   t_edit_poll();
   t_let_go();
   t_edit_poll();
   CHECK(t_grabs.tag == 1 && t_grabs.chosen == 1, "tag %u chosen %u",
         t_grabs.tag, t_grabs.chosen);
   t_xr_frame.grabs = t_grabs;
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   q = t_tag_layer(layers, n, &at);
   CHECK(q && q->subImage.imageRect.offset.x == VIDEO_XR_TAG_X(1)
         && t_near(q->pose.position.x, 0.0f)
         && t_near(q->pose.position.y, VIDEO_XR_TAG_LIFT),
         "screen 1's tag over the right hand");
   /* The right hand lost, or the head: no tag. */
   t_aim_tracked[2] = false;
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   CHECK(!t_tag_layer(layers, n, &at), "a tag over an untracked hand");
   t_aim_tracked[2] = true;
   t_head_tracked   = false;
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   CHECK(!t_tag_layer(layers, n, &at), "a tag with the head lost");
   t_head_tracked = true;
   /* Nothing to edit, as when the screens go at the end: no tag. */
   memset(&shown, 0, sizeof(shown));
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   CHECK(!t_tag_layer(layers, n, &at), "a tag with nothing to edit");
   shown = t_quads;
   /* An image too narrow for the tags: no tag; the beams and dots. */
   n          = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local,
         layers, OPENXR_MAX_EXTRA_LAYERS, &t_under);
   t_cursor_w = VIDEO_XR_TAG_X(0) - 1;
   at         = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local,
         layers, OPENXR_MAX_EXTRA_LAYERS, &t_under);
   t_cursor_w = VIDEO_XR_CURSOR_W;
   for (k = 0, beams = 0; k < at; k++)
   {
      int32_t x = ((const XrCompositionLayerQuad*)layers[k])
         ->subImage.imageRect.offset.x;
      beams += x == VIDEO_XR_BEAM_X(0) || x == VIDEO_XR_BEAM_X(1);
   }
   CHECK(     at == n - 1 && !t_tag_layer(layers, at, &k) && beams
         && ((const XrCompositionLayerQuad*)layers[at - 1])
            ->subImage.imageRect.offset.x == 0,
         "a narrow image: %u layers of %u, %u beams, a tag or no dot", at,
         n, beams);
   /* The left's Select Previous: back to the laser, over the left. */
   t_hold("edit/select_previous", 1, 1.0f, 0.0f);
   t_edit_poll();
   t_let_go();
   t_edit_poll();
   CHECK(t_grabs.tag == 0 && t_grabs.chosen == 0, "tag %u chosen %u",
         t_grabs.tag, t_grabs.chosen);
   /* Both hands at once: the left's. */
   t_hold("edit/select_next", 2, 1.0f, 0.0f);
   t_edit_poll();
   t_let_go();
   t_edit_poll();
   t_hold("edit/select_previous", 1, 1.0f, 0.0f);
   t_hold("edit/select_previous", 2, 1.0f, 0.0f);
   t_edit_poll();
   t_let_go();
   t_edit_poll();
   CHECK(t_grabs.tag == 0 && t_grabs.chosen == 0,
         "both hands: tag %u chosen %u", t_grabs.tag, t_grabs.chosen);
   /* The right's, then edit mode again: the left's from the first poll,
    * whose frames read what the stop published. */
   t_hold("edit/select_next", 2, 1.0f, 0.0f);
   t_edit_poll();
   t_let_go();
   t_edit_poll();
   input_openxr_edit_toggle();
   t_edit_on(false);
   CHECK(t_editing && t_grabs.tag == 0,
         "edit mode again: the tag on hand %u", t_grabs.tag);
   t_edit_poll();
   CHECK(t_grabs.tag == 0, "its first poll: the tag on hand %u",
         t_grabs.tag);
   memset(&t_xr_frame.grabs, 0, sizeof(t_xr_frame.grabs));
   t_cursor       = false;
   t_head_tracked = false;
   t_edit_end();
}

/* A tenth of a millimetre in front of screen 1's plane: no screen rests
 * in a hinge there, so cases about carrying one into its hinge do not
 * take it hinged. */
#define T_APART 0.0001f

/* Screen 2, 0.8 x 0.6, at its spot 1.245 m right of screen 1's middle,
 * 4.5 cm off standing flush beside it: a free gap, past the edge's
 * click, a hair off screen 1's plane (T_APART). */
static void t_snap_beside(void)
{
   video_xr_quad_t *q = &t_quads.quads[t_quads.num_quads++];
   memset(q, 0, sizeof(*q));
   video_xr_pose_identity(&q->pose);
   q->pose.position.x      = 1.245f;
   q->pose.position.z      = -1.8f + T_APART;
   q->width                = 0.8f;
   q->height               = 0.6f;
   q->kind                 = VIDEO_XR_QUAD_SCREEN;
   q->screen               = 1;
   q->slot                 = 1;
   t_quads.slots[1].base.x = 1.245f;
   t_quads.slots[1].base.z = -1.8f + T_APART;
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
         && t_near(t_amp[2], INPUT_OPENXR_HINGE_BUZZ_AMP)
         && t_dur[2] == INPUT_OPENXR_HINGE_BUZZ_NS,
         "%u buzzes as it hinged, at %.2f", t_applies[2] - n, t_amp[2]);
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
   t_clock += T_BUZZ_CLEAR;
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

/* A snap to another screen beats a step. Screen 2 beside screen 1,
 * 10 degrees past parallel, the head where the 30 step is 6.5 past it:
 * turned back by the stick it holds at that step with one buzz, and at
 * 4.6 from parallel, still in the step's hold, snaps parallel with the
 * snap's buzz. */
static void test_edit_step_snap(void)
{
   unsigned i, n;
   video_xr_quad_t *q;
   t_snap = true;
   t_head_tracked = true;
   t_edit_start();
   t_snap_beside();
   q                     = &t_quads.quads[t_quads.num_quads - 1];
   q->pose.orientation.y = (float)sin(5.0 * M_PI / 180.0);
   q->pose.orientation.w = (float)cos(5.0 * M_PI / 180.0);
   t_aim[2].position.x   = 1.245f;
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   /* Carried 5.3 cm, so the snaps are whole. */
   t_aim[2].position.x = 1.205f;
   t_aim[2].position.y = 0.025f;
   t_aim[2].position.z = 0.025f;
   t_head.position.x   = (float)(1.205 - 1.775 * tan(23.5 * M_PI / 180.0));
   t_edit_poll();
   n = t_applies[2];
   CHECK(t_deg_near(t_xr[1].yaw, 10.0), "taken at %.3f", t_xr[1].yaw);
   t_step_polls(2, -1.0f, 0.0f);
   /* Held 6.5 past parallel, the snap's pull shows it at 3. */
   CHECK(t_deg_near(t_xr[1].yaw, 3.0) && t_applies[2] == n + 1,
         "at 31.7 from facing: yaw %.3f, %u buzzes", t_xr[1].yaw,
         t_applies[2] - n);
   t_step_polls(3, -1.0f, 0.0f);
   CHECK(t_applies[2] == n + 1, "held at 30: %u buzzes", t_applies[2] - n);
   t_step_polls(1, -1.0f, 0.0f);
   CHECK(t_deg_near(t_xr[1].yaw, 0.0) && t_applies[2] == n + 2,
         "at 28.1 from facing: yaw %.3f, %u buzzes", t_xr[1].yaw,
         t_applies[2] - n);
   /* Out of the snap and back to 30, never out of its hold: no step
    * buzz, however often. */
   for (i = 0; i < 2; i++)
   {
      n = t_applies[2];
      t_step_polls(1, 1.0f, 0.0f);
      CHECK(t_deg_near(t_xr[1].yaw, 3.0) && t_applies[2] == n,
            "back at 29.0 from facing: yaw %.3f, %u buzzes", t_xr[1].yaw,
            t_applies[2] - n);
      t_step_polls(1, -1.0f, 0.0f);
      CHECK(t_deg_near(t_xr[1].yaw, 0.0), "at 28.1 again: yaw %.3f",
            t_xr[1].yaw);
   }
   t_head.position.x = 0.0f;
   t_head_tracked    = false;
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
   /* Screen 2 10 cm right of flush, past the edge's click: taken there. */
   t_quads.quads[t_quads.num_quads - 1].pose.position.x = 1.30f;
   t_quads.slots[1].base.x = 1.30f;
   t_aim[2].position.x     = 1.30f;
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(hold->g.state == VIDEO_XR_GRAB_HELD && hold->g.slot == 1
         && hold->armed == (VIDEO_XR_SNAP_ALL | VIDEO_XR_SNAP_GAP),
         "took slot %u armed %#x", hold->g.slot, hold->armed);
   n = t_applies[2];
   /* 1.5 cm off flush, 8.5 cm carried: the turn, Y and Z whole, one
    * buzz; the edge and its gap fade, still armed. */
   t_aim[2].position.x = 1.215f;
   t_edit_poll();
   CHECK(     t_applies[2] == n + 1
         && hold->armed == (VIDEO_XR_SNAP_X | VIDEO_XR_SNAP_GAP),
         "%u buzzes in the edge's fade, armed %#x", t_applies[2] - n,
         hold->armed);
   /* 0.9 cm off: the edge whole, one more. */
   t_aim[2].position.x = 1.209f;
   t_clock += T_BUZZ_CLEAR;
   t_edit_poll();
   CHECK(t_applies[2] == n + 2, "%u buzzes taking hold",
         t_applies[2] - n);
   /* 1.1 cm off and back again: the edge's whole pull comes and goes. */
   for (i = 0; i < 3; i++)
   {
      t_aim[2].position.x = 1.211f;
      t_edit_poll();
      t_aim[2].position.x = 1.209f;
      t_edit_poll();
   }
   CHECK(t_applies[2] == n + 2, "%u buzzes resting at the edge",
         t_applies[2] - n);
   /* 7 cm off the edge lets go; back on it, one more. */
   t_aim[2].position.x = 1.27f;
   t_edit_poll();
   CHECK(hold->armed & VIDEO_XR_SNAP_X, "the edge not re-armed: %#x",
         hold->armed);
   t_aim[2].position.x = 1.209f;
   t_edit_poll();
   CHECK(t_applies[2] == n + 3, "%u buzzes back on the edge",
         t_applies[2] - n);
   /* A new grab re-arms them all. */
   t_let_go();
   t_edit_poll();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   CHECK(hold->armed == (VIDEO_XR_SNAP_ALL | VIDEO_XR_SNAP_GAP),
         "armed %#x after a new grab", hold->armed);
   t_edit_end();
   t_snap = false;
}

/* Screen 2, 0.8 x 0.6, 18 degrees short of square to screen 1 (yaw
 * -72), its middle 1.4 m ahead of a right hand at x 0.82: square, it
 * would stand 2 cm right of screen 1's right corner, a free gap;
 * turned 14.4 in about its middle, its edge is 0.5 cm over the corner,
 * which clicks it there. */
static void t_snap_square(void)
{
   video_xr_quad_t *q = &t_quads.quads[t_quads.num_quads++];
   memset(q, 0, sizeof(*q));
   q->pose.orientation.y   = (float)-sin(0.2 * M_PI);
   q->pose.orientation.w   = (float)cos(0.2 * M_PI);
   q->pose.position.x      = 0.82f;
   q->pose.position.z      = -1.4f;
   q->width                = 0.8f;
   q->height               = 0.6f;
   q->kind                 = VIDEO_XR_QUAD_SCREEN;
   q->screen               = 1;
   q->slot                 = 1;
   t_quads.slots[1].base.x = 0.82f;
   t_quads.slots[1].base.z = -1.4f;
   t_aim[2].position.x     = 0.82f;
}

/* n polls us apart, the right hand gripping, its trigger held if tilt,
 * and the left stick at x. */
static void t_turn_polls(unsigned n, float x, retro_time_t us, bool tilt)
{
   unsigned i;
   t_let_go();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   if (tilt)
      t_hold("edit/tilt", 2, 1.0f, 0.0f);
   t_hold("edit/stick", 1, x, 0.0f);
   for (i = 0; i < n; i++)
   {
      t_clock += us;
      t_edit_poll();
   }
}

/* Edit mode with Snap to Screens as snap, screen 2 taken by the right
 * hand and carried 7 cm up, past the lines across screen 1's middle,
 * so its snaps are whole. */
static void t_take_square(bool snap, bool tilt)
{
   t_snap = snap;
   t_edit_start();
   t_snap_square();
   t_turn_polls(1, 0.0f, 10000, tilt);
   CHECK(     input_openxr_edit.hand[1].g.state == VIDEO_XR_GRAB_HELD
         && input_openxr_edit.hand[1].g.slot == 1
         && input_openxr_edit.hand[1].g.tilt == tilt,
         "took slot %u", input_openxr_edit.hand[1].g.slot);
   t_aim[2].position.y = 0.07f;
   t_turn_polls(1, 0.0f, 10000, tilt);
}

static void t_take_end(void)
{
   t_let_go();
   t_edit_poll();
   t_edit_end();
   t_snap = false;
}

/* The right hand turned deg about the vertical, its ray still through
 * where screen 2's middle was taken. */
static void t_point(double deg)
{
   double r = deg * M_PI / 180.0;
   t_aim[2].orientation.y = (float)sin(r / 2.0);
   t_aim[2].orientation.w = (float)cos(r / 2.0);
   t_aim[2].position.x    = (float)(0.82 + 1.4 * sin(r));
   t_aim[2].position.z    = (float)(-1.4 + 1.4 * cos(r));
}

/* Screen 2's hinged edge at pose: metres off screen 1's edge line, at
 * the farther of its ends. Its near edge, its own -X side 0.4 m from
 * its middle, off screen 1's right edge line, x 0.8, z -1.8; or under,
 * its top edge, 0.3 m up, off screen 1's bottom edge line, y -0.48,
 * z -1.8. */
static double t_edge_off(const video_xr_pose_t *pose, bool under)
{
   unsigned i;
   double worst = 0.0;
   video_xr_vec3_t m, w;
   for (i = 0; i < 2; i++)
   {
      double d[3];
      m.x  = (under && i) ? 0.4f : -0.4f;
      m.y  = under ? 0.3f : (i ? 0.3f : -0.3f);
      m.z  = 0.0f;
      video_xr_rotate(&pose->orientation, &m, &w);
      d[0] = under ? 0.0 : (double)pose->position.x + w.x - 0.8;
      d[1] = under ? (double)pose->position.y + w.y + 0.48 : 0.0;
      d[2] = (double)pose->position.z + w.z + 1.8;
      if (sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) > worst)
         worst = sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
   }
   return worst;
}

/* Screen 2's near edge off the line as edit mode last wrote it. */
static double t_hinge_off(void)
{
   video_xr_pose_t pose;
   video_xr_item_pose(&t_quads.slots[1].base, &t_xr[1], &pose);
   return t_edge_off(&pose, false);
}

/* Screen 2's top edge off the line under screen 1 as edit mode last
 * wrote it. */
static double t_under_off(void)
{
   video_xr_pose_t pose;
   video_xr_item_pose(&t_quads.slots[1].base, &t_xr[1], &pose);
   return t_edge_off(&pose, true);
}

/* Where the right hand holds screen 2, with no snap. */
static void t_held_pose(video_xr_pose_t *pose)
{
   video_xr_pose_t aim;
   aim.orientation.x = t_aim[2].orientation.x;
   aim.orientation.y = t_aim[2].orientation.y;
   aim.orientation.z = t_aim[2].orientation.z;
   aim.orientation.w = t_aim[2].orientation.w;
   aim.position.x    = t_aim[2].position.x;
   aim.position.y    = t_aim[2].position.y;
   aim.position.z    = t_aim[2].position.z;
   video_xr_grab_pose(&input_openxr_edit.hand[1].g, &aim, NULL, 0, NULL,
         pose, NULL, NULL);
}

/* Screen 2's hinged edge off the line where the right hand holds it,
 * with no snap. */
static double t_held_off(bool under)
{
   video_xr_pose_t pose;
   t_held_pose(&pose);
   return t_edge_off(&pose, under);
}

/* Screen 2's hinged edge, as t_edge_off takes it, at pose against at
 * from: metres moved across the line, at the farther of its ends. */
static double t_edge_moved(const video_xr_pose_t *pose,
      const video_xr_pose_t *from, bool under)
{
   unsigned i;
   double worst = 0.0;
   video_xr_vec3_t m, w, v;
   for (i = 0; i < 2; i++)
   {
      double d[3];
      m.x  = (under && i) ? 0.4f : -0.4f;
      m.y  = under ? 0.3f : (i ? 0.3f : -0.3f);
      m.z  = 0.0f;
      video_xr_rotate(&pose->orientation, &m, &w);
      video_xr_rotate(&from->orientation, &m, &v);
      d[0] = under ? 0.0 : (double)pose->position.x + w.x
         - from->position.x - v.x;
      d[1] = under ? (double)pose->position.y + w.y
         - from->position.y - v.y : 0.0;
      d[2] = (double)pose->position.z + w.z - from->position.z - v.z;
      if (sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) > worst)
         worst = sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
   }
   return worst;
}

/* An engaged screen snap's turn sticks as the free hand's stick lets
 * go or starts a push, carried or tilting: turned on, the screen moves
 * on from the angle it showed, and its place is still the hand's. Where
 * the pull only fades nothing sticks: let go, it stays where it shows,
 * and pushed away, it never hops toward the snap. Carrying alone keeps
 * the hand's angle. */
static void t_snap_commit(bool tilt)
{
   const char *how = tilt ? "tilting" : "carried";
   /* Turned 14.4 in, 3.6 short: square, at the corner. */
   t_take_square(true, tilt);
   t_turn_polls(16, -1.0f, 10000, tilt);
   CHECK(t_deg_near(t_xr[1].yaw, -90.0) && t_near(t_xr[1].x, -0.02f)
         && t_near(t_xr[1].y, 0.07f),
         "%s, turned in: yaw %.3f at %.4f,%.4f", how, t_xr[1].yaw,
         t_xr[1].x, t_xr[1].y);
   t_turn_polls(1, 0.0f, 10000, tilt);
   CHECK(t_deg_near(t_xr[1].yaw, -90.0), "%s, let go: yaw %.3f", how,
         t_xr[1].yaw);
   /* Out 9 from square, where the pull leaves 1; then 18, unpulled, at
    * the hand's place. */
   t_turn_polls(1, 1.0f, 100000, tilt);
   CHECK(t_deg_near(t_xr[1].yaw, -82.0), "%s, pushed out: yaw %.3f", how,
         t_xr[1].yaw);
   t_turn_polls(1, 1.0f, 100000, tilt);
   CHECK(t_deg_near(t_xr[1].yaw, -72.0) && t_near(t_xr[1].x, 0.1036f)
         && t_hinge_off() < 1e-3 && t_near(t_xr[1].y, 0.07f),
         "%s, pushed on: yaw %.3f at %.4f,%.4f", how, t_xr[1].yaw,
         t_xr[1].x, t_xr[1].y);
   t_take_end();

   /* Turned 10.8 in, 7.2 short, the pull shows it at 4.4: let go, it
    * stays there; pushed out, it goes on from its own 7.2. */
   t_take_square(true, tilt);
   t_turn_polls(12, -1.0f, 10000, tilt);
   CHECK(t_deg_near(t_xr[1].yaw, -85.6), "%s, in the fade: yaw %.3f", how,
         t_xr[1].yaw);
   t_turn_polls(1, 0.0f, 10000, tilt);
   CHECK(t_deg_near(t_xr[1].yaw, -85.6), "%s, let go in the fade: yaw %.3f",
         how, t_xr[1].yaw);
   t_turn_polls(1, 1.0f, 100000, tilt);
   CHECK(t_deg_near(t_xr[1].yaw, -73.8),
         "%s, pushed out of the fade: yaw %.3f", how, t_xr[1].yaw);
   t_take_end();

   /* Turned 9 in, 9 short, shown at 8: let go, it stays; pushed slowly
    * away, it goes on from there. */
   t_take_square(true, tilt);
   t_turn_polls(10, -1.0f, 10000, tilt);
   CHECK(t_deg_near(t_xr[1].yaw, -82.0), "%s, at 9: yaw %.3f", how,
         t_xr[1].yaw);
   t_turn_polls(1, 0.0f, 10000, tilt);
   CHECK(t_deg_near(t_xr[1].yaw, -82.0), "%s, let go at 9: yaw %.3f", how,
         t_xr[1].yaw);
   t_turn_polls(1, 1.0f, 10000, tilt);
   CHECK(t_deg_near(t_xr[1].yaw, -80.2), "%s, pushed on from 9: yaw %.3f",
         how, t_xr[1].yaw);
   t_take_end();

   /* Pointed 11 round, 7 short, shown at 4: pushed slowly away, it
    * leaves the snap from there, a step a poll. */
   t_take_square(true, tilt);
   t_point(-11.0);
   t_turn_polls(1, 0.0f, 10000, tilt);
   CHECK(t_deg_near(t_xr[1].yaw, -86.0), "%s, pointed to 7: yaw %.3f", how,
         t_xr[1].yaw);
   t_turn_polls(1, 1.0f, 10000, tilt);
   CHECK(t_deg_near(t_xr[1].yaw, -84.2), "%s, pointed, pushed: yaw %.3f",
         how, t_xr[1].yaw);
   t_turn_polls(1, 1.0f, 10000, tilt);
   CHECK(t_deg_near(t_xr[1].yaw, -82.4), "%s, pointed, pushed on: yaw %.3f",
         how, t_xr[1].yaw);
   t_take_end();

   /* Pointed 14 round, 4 short: square; the stick then pushes on from
    * square. */
   t_take_square(true, tilt);
   t_point(-14.0);
   t_turn_polls(1, 0.0f, 10000, tilt);
   CHECK(t_deg_near(t_xr[1].yaw, -90.0), "%s, pointed in: yaw %.3f", how,
         t_xr[1].yaw);
   t_turn_polls(1, 1.0f, 100000, tilt);
   CHECK(t_deg_near(t_xr[1].yaw, -82.0), "%s, pointed, pushed: yaw %.3f",
         how, t_xr[1].yaw);
   t_take_end();

   /* Pointed in, then out again with the stick never touched: at the
    * hand's angle. */
   t_take_square(true, tilt);
   t_point(-14.0);
   t_turn_polls(1, 0.0f, 10000, tilt);
   t_point(4.0);
   t_turn_polls(1, 0.0f, 10000, tilt);
   CHECK(t_deg_near(t_xr[1].yaw, -68.0), "%s, pointed out: yaw %.3f", how,
         t_xr[1].yaw);
   t_take_end();
}

static void test_edit_snap_commit(void)
{
   float back;
   t_snap_commit(false);
   t_snap_commit(true);
   /* In the fade, the grip let go with the stick still pushed: left
    * where it showed 80 ms before. */
   t_take_square(true, false);
   t_turn_polls(4, -1.0f, 10000, false);
   back = t_xr[1].yaw;
   t_turn_polls(8, -1.0f, 10000, false);
   t_let_go();
   t_hold("edit/stick", 1, -1.0f, 0.0f);
   t_edit_poll();
   CHECK(     input_openxr_edit.hand[1].g.state != VIDEO_XR_GRAB_HELD
         && t_deg_near(t_xr[1].yaw, -75.6) && t_deg_near(back, -75.6),
         "the grip let go in the fade: yaw %.3f, shown %.3f 80 ms before",
         t_xr[1].yaw, back);
   t_take_end();
   /* Snap to Screens off: nothing snaps, nothing sticks. */
   t_take_square(false, false);
   t_turn_polls(15, -1.0f, 10000, false);
   t_turn_polls(1, 0.0f, 10000, false);
   CHECK(t_deg_near(t_xr[1].yaw, -85.5) && t_near(t_xr[1].x, 0.0f),
         "off, let go: yaw %.3f at %.4f", t_xr[1].yaw, t_xr[1].x);
   t_turn_polls(1, 1.0f, 100000, false);
   CHECK(t_deg_near(t_xr[1].yaw, -76.5), "off, pushed: yaw %.3f",
         t_xr[1].yaw);
   t_take_end();
}

/* Square at screen 1's right corner, screen 2 is hinged 90 degrees from
 * flat: the turn stick swings it about the shared edge, which stays on
 * the line all the way to flat beside screen 1, carried or tilting.
 * The hinge takes hold as it comes square, with its long buzz, and the
 * swing keeps it with no other. Snap to Angles holds it at 75 with no
 * tick, and at 60 with one; Off, it swings through with none. */
static void t_hinge_sweep(bool tilt, unsigned angles)
{
   unsigned i, n, l;
   double worst = 0.0;
   unsigned held75 = 0;
   t_angles = angles;
   t_take_square(true, tilt);
   l = t_longs[2];
   t_turn_polls(16, -1.0f, 10000, tilt);
   CHECK(     t_deg_near(t_xr[1].yaw, -90.0) && t_hinge_off() < 1e-3
         && input_openxr_edit.hand[1].hinged && t_longs[2] == l + 1,
         "square: yaw %.3f, %.4f off the line, hinged %d, %u long buzzes",
         t_xr[1].yaw, t_hinge_off(), (int)input_openxr_edit.hand[1].hinged,
         t_longs[2] - l);
   n = t_applies[2];
   l = t_longs[2];
   for (i = 0; i < 84; i++)
   {
      t_turn_polls(1, 1.0f, 10000, tilt);
      if (t_hinge_off() > worst)
         worst = t_hinge_off();
      if (t_deg_near(t_xr[1].yaw, -75.0))
         held75++;
      if (i == 30)
         CHECK(     t_applies[2] - n - (t_longs[2] - l) == (angles ? 1u : 0u)
               && t_longs[2] == l,
               "%u step ticks and %u long buzzes by 60",
               t_applies[2] - n - (t_longs[2] - l), t_longs[2] - l);
   }
   CHECK(worst < 1e-3, "%.4f off the line in the sweep", worst);
   CHECK(angles ? held75 >= 3 : held75 <= 1, "held at 75 for %u polls",
         held75);
   t_take_end();
   t_angles = VIDEO_OPENXR_EDIT_SNAP_ANGLES_15;
}

static void test_edit_hinge_swing(void)
{
   t_hinge_sweep(false, VIDEO_OPENXR_EDIT_SNAP_ANGLES_15);
   t_hinge_sweep(true, VIDEO_OPENXR_EDIT_SNAP_ANGLES_15);
   t_hinge_sweep(false, VIDEO_OPENXR_EDIT_SNAP_ANGLES_OFF);
}

/* Swung to 67.5 from flat and let go, screen 2 stays there, on the line,
 * and stays so carried 1 cm up along the edge. The tilt stick, the
 * other axis, pulls it off the line: 10.8 degrees of it leave the edge
 * twisted out of the hinge's reach. Snap to Screens off: no hinge, the
 * turn stick turns it about its own middle. */
static void test_edit_hinge_hold(void)
{
   unsigned i;
   t_take_square(true, false);
   t_turn_polls(16, -1.0f, 10000, false);
   t_turn_polls(21, 1.0f, 10000, false);
   t_turn_polls(1, 0.0f, 10000, false);
   CHECK(t_deg_near(t_xr[1].yaw, -67.5) && t_hinge_off() < 1e-3,
         "let go at yaw %.3f, %.4f off", t_xr[1].yaw, t_hinge_off());
   t_aim[2].position.y += 0.01f;
   t_turn_polls(1, 0.0f, 10000, false);
   CHECK(t_deg_near(t_xr[1].yaw, -67.5) && t_hinge_off() < 1e-3,
         "carried up: yaw %.3f, %.4f off", t_xr[1].yaw, t_hinge_off());
   t_let_go();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_hold("edit/stick", 1, 0.0f, 1.0f);
   for (i = 0; i < 12; i++)
   {
      t_clock += 10000;
      t_edit_poll();
   }
   CHECK(t_hinge_off() > 0.005, "tilted 10.8: %.4f off", t_hinge_off());
   t_take_end();
   t_take_square(false, false);
   t_turn_polls(10, -1.0f, 10000, false);
   CHECK(t_near(t_xr[1].x, 0.0f), "snap off: x %.4f", t_xr[1].x);
   t_take_end();
}

/* Hinged at square and swung toward flat, the step at 75 buzzes once:
 * a wiggle inside its hold buzzes no more, whatever the head sees; out
 * of the hold and back, it buzzes again. */
static void test_edit_hinge_rearm(void)
{
   unsigned n;
   t_head_tracked    = true;
   t_head.position.x = 0.3f;
   t_take_square(true, false);
   t_turn_polls(15, -1.0f, 10000, false);
   t_turn_polls(1, 0.0f, 10000, false);
   n = t_applies[2];
   t_turn_polls(15, 1.0f, 10000, false);
   CHECK(t_deg_near(t_xr[1].yaw, -75.0) && t_applies[2] == n + 1,
         "at 76.5: yaw %.3f, %u buzzes", t_xr[1].yaw, t_applies[2] - n);
   t_turn_polls(1, -1.0f, 5000, false);
   t_turn_polls(1, 1.0f, 5000, false);
   CHECK(t_deg_near(t_xr[1].yaw, -75.0) && t_applies[2] == n + 1,
         "wiggled in the hold: yaw %.3f, %u buzzes", t_xr[1].yaw,
         t_applies[2] - n);
   t_turn_polls(3, -1.0f, 10000, false);
   t_turn_polls(3, 1.0f, 10000, false);
   CHECK(t_deg_near(t_xr[1].yaw, -75.0) && t_applies[2] == n + 2,
         "out of the hold and back: yaw %.3f, %u buzzes", t_xr[1].yaw,
         t_applies[2] - n);
   t_take_end();
   t_head.position.x = 0.0f;
   t_head_tracked    = false;
}

/* Held in the 75 step's hold, the stick still pushed, then carried
 * 4 cm off the line so the hinge lets go: the turn goes on from 75 by
 * one poll's worth, without the hold's carry. */
static void test_edit_hinge_drop(void)
{
   t_take_square(true, false);
   t_turn_polls(15, -1.0f, 10000, false);
   t_turn_polls(1, 0.0f, 10000, false);
   t_turn_polls(15, 1.0f, 10000, false);
   CHECK(t_deg_near(t_xr[1].yaw, -75.0), "held at 75: yaw %.3f",
         t_xr[1].yaw);
   t_aim[2].position.z += 0.04f;
   t_turn_polls(1, 1.0f, 10000, false);
   CHECK(!input_openxr_edit.hand[1].hinged
         && t_deg_near(t_xr[1].yaw, -74.1),
         "carried off: yaw %.3f, hinged %d", t_xr[1].yaw,
         input_openxr_edit.hand[1].hinged);
   t_take_end();
}

/* Screen 2 under screen 1, its top edge on screen 1's bottom edge,
 * folded fold degrees from flat toward the viewer: the 3DS layout,
 * apart metres in front of screen 1's plane. */
static void t_under_at(double fold, float apart)
{
   double r = fold * M_PI / 180.0;
   video_xr_quad_t *q = &t_quads.quads[t_quads.num_quads++];
   memset(q, 0, sizeof(*q));
   q->pose.orientation.x   = (float)-sin(r / 2.0);
   q->pose.orientation.w   = (float)cos(r / 2.0);
   q->pose.position.y      = (float)(-0.48 - 0.3 * cos(r));
   q->pose.position.z      = (float)(-1.8 + 0.3 * sin(r)) + apart;
   q->width                = 0.8f;
   q->height               = 0.6f;
   q->kind                 = VIDEO_XR_QUAD_SCREEN;
   q->screen               = 1;
   q->slot                 = 1;
   t_quads.slots[1].base.y = q->pose.position.y;
   t_quads.slots[1].base.z = q->pose.position.z;
   t_aim[2].position.y     = q->pose.position.y;
}

/* Screen 2 in the 3DS layout a hair off its hinge (T_APART). */
static void t_snap_under(double fold)
{
   t_under_at(fold, T_APART);
}

/* Screen 2 in the 3DS layout resting in its hinge on screen 1. */
static void t_rest_under(double fold)
{
   t_under_at(fold, 0.0f);
}

/* A hinge line from a to b as its layer shows it, each end
 * VIDEO_XR_HINGE_LIFT nearer the head at the origin: its middle and
 * length. */
static void t_lifted(float ax, float ay, float az, float bx, float by,
      float bz, video_xr_vec3_t *mid, float *len)
{
   double sa = 1.0 - VIDEO_XR_HINGE_LIFT
      / sqrt((double)ax * ax + (double)ay * ay + (double)az * az);
   double sb = 1.0 - VIDEO_XR_HINGE_LIFT
      / sqrt((double)bx * bx + (double)by * by + (double)bz * bz);
   double dx = bx * sb - ax * sa;
   double dy = by * sb - ay * sa;
   double dz = bz * sb - az * sa;
   mid->x    = (float)((ax * sa + bx * sb) / 2.0);
   mid->y    = (float)((ay * sa + by * sb) / 2.0);
   mid->z    = (float)((az * sa + bz * sb) / 2.0);
   *len      = (float)sqrt(dx * dx + dy * dy + dz * dz);
}

/* openxr_frame_hold from t_grabs into set, each hand at main's last
 * pose, the screens unturned. */
static void t_follow(video_xr_quad_set_t *set)
{
   unsigned h, i;
   unsigned holding  = 0;
   t_xr_frame.grabs  = t_grabs;
   t_xr_frame.hinged = 0;
   for (h = 0; h < 2; h++)
      if (     t_grabs.hand[h].state == VIDEO_XR_GRAB_HELD
            && t_grabs.hand[h].slot < VIDEO_XR_MAX_SLOTS)
         holding |= 1u << t_grabs.hand[h].slot;
   for (h = 0; h < 2; h++)
   {
      const video_xr_grab_t *g = &t_grabs.hand[h];
      if (g->state != VIDEO_XR_GRAB_HELD || g->slot >= VIDEO_XR_MAX_SLOTS)
         continue;
      for (i = 0; i < set->num_quads; i++)
         if (set->quads[i].slot == g->slot && !set->slots[g->slot].fixed)
         {
            set->quads[i].pose   = g->last;
            set->quads[i].width  = g->width;
            set->quads[i].height = g->height;
         }
      if (video_xr_grab_marked(g, &g->last, set, holding,
               &t_xr_frame.hinges[h]))
         t_xr_frame.hinged |= 1u << h;
   }
}

/* What an edit mode layer shows: 1 an outline, 2 the menu's handle, 3 a
 * hinge slot, 4 a beam, 5 the tag, 6 a dot; in *cut what a mark may
 * change, an outline's image (its cut's cell, below 0, or its offset
 * from its column and width) or a slot's first row; never its light. */
static unsigned t_kind(const XrCompositionLayerBaseHeader *l, int32_t *cut)
{
   const XrCompositionLayerQuad *q = (const XrCompositionLayerQuad*)l;
   int32_t x                       = q->subImage.imageRect.offset.x;
   *cut                            = 0;
   if (q->subImage.imageRect.extent.height == VIDEO_XR_OUTLINE_DIM)
   {
      if (x >= VIDEO_XR_CUT_X(0, 0))
         *cut = -1 - (x - VIDEO_XR_CUT_X(0, 0))
            / (VIDEO_XR_OUTLINE_DIM + 1) % VIDEO_XR_OUTLINE_CUTS;
      else
         *cut = (x - ((x >= VIDEO_XR_OUTLINE_X(1))
                  ? VIDEO_XR_OUTLINE_X(1) : VIDEO_XR_OUTLINE_X(0))) * 1024
            + q->subImage.imageRect.extent.width;
      return 1;
   }
   if (x == VIDEO_XR_HANDLE_X(0) || x == VIDEO_XR_HANDLE_X(1))
      return 2;
   if (x == VIDEO_XR_HINGE_X + 1)
   {
      *cut = q->subImage.imageRect.offset.y;
      return 3;
   }
   if (x == VIDEO_XR_BEAM_X(0) || x == VIDEO_XR_BEAM_X(1))
      return 4;
   if (q->subImage.imageRect.extent.height == VIDEO_XR_TAG_ROWS)
      return 5;
   return 6;
}

/* Whether q is a hinge slot's layer from a to b, hidden or shown. */
static bool t_slot_is(const XrCompositionLayerQuad *q, bool hidden,
      float ax, float ay, float az, float bx, float by, float bz)
{
   video_xr_vec3_t m;
   float len;
   t_lifted(ax, ay, az, bx, by, bz, &m, &len);
   return q->subImage.imageRect.offset.x == VIDEO_XR_HINGE_X + 1
      && q->subImage.imageRect.offset.y
         == (hidden ? VIDEO_XR_HINGE_HIDDEN_Y : VIDEO_XR_HINGE_Y)
      && q->subImage.imageRect.extent.width == VIDEO_XR_HINGE_COLS - 2
      && q->subImage.imageRect.extent.height == VIDEO_XR_HINGE_ROWS
      && t_near(q->size.height, VIDEO_XR_HINGE_WIDTH)
      && t_near(q->size.width, len) && t_near(q->pose.position.x, m.x)
      && t_near(q->pose.position.y, m.y) && t_near(q->pose.position.z, m.z);
}

/* A frame's edit mode layers: how many, each one's kind and cut; the
 * frames with an outline not its item's whole one. */
typedef struct
{
   unsigned n;
   unsigned kind[OPENXR_MAX_EXTRA_LAYERS];
   int32_t cut[OPENXR_MAX_EXTRA_LAYERS];
   unsigned bent;
} t_layout_t;

/* Whether every outline in layers' first n has the pose, lifted toward
 * the head, and the size of an item's whole outline in shown. */
static bool t_outlines_whole(const video_xr_quad_set_t *shown,
      const XrCompositionLayerBaseHeader **layers, unsigned n)
{
   unsigned i, j;
   for (i = 0; i < n; i++)
   {
      const XrCompositionLayerQuad *l = (const XrCompositionLayerQuad*)
         layers[i];
      bool whole                      = false;
      if (l->subImage.imageRect.extent.height != VIDEO_XR_OUTLINE_DIM)
         continue;
      for (j = 0; j < shown->num_quads && !whole; j++)
      {
         video_xr_quad_t o;
         double dx, dy, dz, dot;
         video_xr_outline(&shown->quads[j], &o);
         dx    = l->pose.position.x - o.pose.position.x;
         dy    = l->pose.position.y - o.pose.position.y;
         dz    = l->pose.position.z - o.pose.position.z;
         dot   = l->pose.orientation.x * o.pose.orientation.x
            + l->pose.orientation.y * o.pose.orientation.y
            + l->pose.orientation.z * o.pose.orientation.z
            + l->pose.orientation.w * o.pose.orientation.w;
         whole = t_near(l->size.width, o.width)
            && t_near(l->size.height, o.height)
            && sqrt(dx * dx + dy * dy + dz * dz)
               < VIDEO_XR_CURSOR_LIFT + 1e-3
            && fabs(fabs(dot) - 1.0) < 1e-5;
      }
      if (!whole)
         return false;
   }
   return true;
}

/* Three frames between two polls, from t_quads and t_grabs through
 * t_follow, the head tracked, against s, the first frame and the last
 * one's cuts (s->n 0: this is the first): adds to *counts the frames
 * whose count is not the first's, to *moved those whose kinds by index
 * are not; returns those whose cuts are not the last's. */
static unsigned t_frames(t_layout_t *s, unsigned *counts, unsigned *moved)
{
   unsigned f, i;
   video_xr_quad_set_t shown;
   const XrCompositionLayerBaseHeader *layers[OPENXR_MAX_EXTRA_LAYERS];
   unsigned changed = 0;
   bool head        = t_head_tracked;
   bool cursor      = t_cursor;
   t_head_tracked   = true;
   t_cursor         = true;
   shown            = t_quads;
   t_follow(&shown);
   for (f = 0; f < 3; f++)
   {
      bool order = false;
      bool cut   = false;
      unsigned n = t_hooks.frame_layers(t_hooks.user, t_time, &shown,
            t_xr_session.local_space, layers, OPENXR_MAX_EXTRA_LAYERS,
            &t_under);
      s->bent += t_outlines_whole(&shown, layers, n) ? 0u : 1u;
      if (!s->n)
      {
         s->n = n;
         for (i = 0; i < n; i++)
            s->kind[i] = t_kind(layers[i], &s->cut[i]);
         continue;
      }
      if (n != s->n)
      {
         (*counts)++;
         continue;
      }
      for (i = 0; i < n; i++)
      {
         int32_t c;
         order = order || t_kind(layers[i], &c) != s->kind[i];
         cut   = cut || c != s->cut[i];
         s->cut[i] = c;
      }
      *moved  += order ? 1u : 0u;
      changed += cut ? 1u : 0u;
   }
   t_head_tracked = head;
   t_cursor       = cursor;
   return changed;
}

/* The frame loop's hinges: the held screen's paired edge and its
 * target's lose their rim, the rest of each outline as before; a green
 * line along each hinge where the edges overlap, midway across the
 * gap, facing the head, after the outlines and before the beams. Short
 * of layers the beams go first, then the line; the rims go whatever the
 * room. Two hands, two lines, the first hand's first; on a target's
 * opposite edges both rims go, on adjacent ones the first hand's. */
static void test_edit_hinge_layers(void)
{
   static const float gaps[] = { 0.0f, 0.001f, 0.002f, 0.004f, 0.006f,
      0.01f };
   unsigned i, n;
   video_xr_vec3_t ax, az;
   video_xr_vec3_t m;
   float len;
   video_xr_quad_set_t shown;
   video_xr_hinge_t *k;
   const XrCompositionLayerQuad *q;
   const XrCompositionLayerBaseHeader *layers[OPENXR_MAX_EXTRA_LAYERS];
   const float cut  = (float)(VIDEO_XR_OUTLINE_DIM - VIDEO_XR_OUTLINE_CROP)
      / VIDEO_XR_OUTLINE_DIM;
   const float both = (float)(VIDEO_XR_OUTLINE_DIM
         - 2 * VIDEO_XR_OUTLINE_CROP) / VIDEO_XR_OUTLINE_DIM;
   const float half = (float)VIDEO_XR_OUTLINE_CROP
      / (2.0f * VIDEO_XR_OUTLINE_DIM);
   XrSpace local    = XR_NULL_HANDLE;
   t_edit_start();
   t_snap_under(0.0);
   local            = t_xr_session.local_space;
   t_aim_tracked[1] = false;
   t_head_tracked   = true;
   t_cursor         = true;
   shown            = t_quads;
   /* Screen 2 held 2 cm under screen 1, the right hand on it. */
   shown.quads[2].pose.position.y = -0.80f;
   memset(&t_xr_frame.grabs, 0, sizeof(t_xr_frame.grabs));
   t_xr_frame.grabs.hand[1].slot  = 1;
   t_xr_frame.grabs.hand[1].state = VIDEO_XR_GRAB_HELD;
   t_xr_frame.hinged              = 2u;
   k                              = &t_xr_frame.hinges[1];
   memset(k, 0, sizeof(*k));
   k->point.y = -0.50f;
   k->point.z = -1.8f;
   k->dir.x   = 1.0f;
   k->flat.y  = -1.0f;
   k->gap     = 0.02f;
   k->edge    = 0;
   k->slot    = 0;
   k->on      = 1;
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   CHECK(n == 7 && t_under == 2, "%u layers, %u under: want the screens' "
         "outlines under, the menu's and its handle, a line, a beam and a "
         "dot", n, t_under);
   q = (const XrCompositionLayerQuad*)layers[0];
   t_layer_axis(q, 1.0f, 0.0f, 0.0f, &ax);
   CHECK(     q->subImage.imageRect.offset.x
            == VIDEO_XR_OUTLINE_X(0) + VIDEO_XR_OUTLINE_CROP
         && q->subImage.imageRect.offset.y == 0
         && q->subImage.imageRect.extent.width
            == VIDEO_XR_OUTLINE_DIM - VIDEO_XR_OUTLINE_CROP
         && q->subImage.imageRect.extent.height == VIDEO_XR_OUTLINE_DIM
         && t_near(ax.y, 1.0f)
         && t_near(q->size.width, t_outline(0.96f) * cut)
         && t_near(q->size.height, t_outline(1.6f))
         && t_near(q->pose.position.y, t_outline(0.96f) * half)
         && t_near(q->pose.position.z, -1.8f + VIDEO_XR_CURSOR_LIFT),
         "screen 1's outline without its bottom rim: offset %d, %.4f x "
         "%.4f at y %.4f", (int)q->subImage.imageRect.offset.x,
         q->size.width, q->size.height, q->pose.position.y);
   q = (const XrCompositionLayerQuad*)layers[1];
   t_layer_axis(q, 1.0f, 0.0f, 0.0f, &ax);
   CHECK(     q->subImage.imageRect.offset.x == VIDEO_XR_OUTLINE_X(1)
         && q->subImage.imageRect.extent.width
            == VIDEO_XR_OUTLINE_DIM - VIDEO_XR_OUTLINE_CROP
         && t_near(ax.y, 1.0f)
         && t_near(q->size.width, t_outline(0.6f) * cut)
         && t_near(q->pose.position.y, -0.80f - t_outline(0.6f) * half),
         "screen 2's outline without its top rim: offset %d at y %.4f",
         (int)q->subImage.imageRect.offset.x, q->pose.position.y);
   q = (const XrCompositionLayerQuad*)layers[2];
   CHECK(q->subImage.imageRect.offset.x == VIDEO_XR_OUTLINE_X(0)
         && q->subImage.imageRect.extent.width == VIDEO_XR_OUTLINE_DIM,
         "the menu's outline was cut");
   q = (const XrCompositionLayerQuad*)layers[4];
   t_layer_axis(q, 1.0f, 0.0f, 0.0f, &ax);
   t_layer_axis(q, 0.0f, 0.0f, 1.0f, &az);
   /* Each end a centimetre toward the head, so the line shows over both
    * screens whatever order the runtime draws them in. */
   t_lifted(-0.4f, -0.49f, -1.8f, 0.4f, -0.49f, -1.8f, &m, &len);
   CHECK(     q->subImage.imageRect.offset.x == VIDEO_XR_HINGE_X + 1
         && q->subImage.imageRect.offset.y == VIDEO_XR_HINGE_Y
         && q->subImage.imageRect.extent.width == VIDEO_XR_HINGE_COLS - 2
         && q->subImage.imageRect.extent.height == VIDEO_XR_HINGE_ROWS
         && q->subImage.swapchain == (XrSwapchain)(uintptr_t)0x51
         && q->space == local
         && (q->layerFlags
            & XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT)
         && t_near(q->size.width, len)
         && t_near(q->size.height, VIDEO_XR_HINGE_WIDTH)
         && t_near(q->pose.position.x, m.x)
         && t_near(q->pose.position.y, m.y)
         && t_near(q->pose.position.z, m.z)
         && t_near(ax.x, 1.0f)
         && t_near(az.y, 0.2627f) && t_near(az.z, 0.9649f),
         "the line: offset %d, %.4f x %.4f at %.4f,%.4f,%.4f facing "
         "%.3f,%.3f,%.3f; want %.4f long at %.4f,%.4f,%.4f",
         (int)q->subImage.imageRect.offset.x, q->size.width,
         q->size.height, q->pose.position.x, q->pose.position.y,
         q->pose.position.z, az.x, az.y, az.z, len, m.x, m.y, m.z);
   q = (const XrCompositionLayerQuad*)layers[5];
   CHECK(q->subImage.imageRect.offset.x == VIDEO_XR_BEAM_X(0),
         "the beam is not after the line");
   /* 6 mm wide at every gap. */
   for (i = 0; i < 6; i++)
   {
      k->gap     = gaps[i];
      k->point.y = -0.48f - gaps[i];
      n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
            OPENXR_MAX_EXTRA_LAYERS, &t_under);
      q = (const XrCompositionLayerQuad*)layers[4];
      CHECK(     n == 7
            && q->subImage.imageRect.offset.x == VIDEO_XR_HINGE_X + 1
            && fabs(q->size.height - VIDEO_XR_HINGE_WIDTH) < 1e-6,
            "a %.3f gap: the line %.4f wide, want %.4f", gaps[i],
            q->size.height, VIDEO_XR_HINGE_WIDTH);
   }
   k->gap     = 0.02f;
   k->point.y = -0.50f;
   /* Short of layers: the beam goes, then the line; the rims stay cut. */
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_KEPT_EXTRA_LAYERS + 5, &t_under);
   q = (const XrCompositionLayerQuad*)layers[4];
   CHECK(n == 6 && q->subImage.imageRect.offset.x == VIDEO_XR_HINGE_X + 1,
         "one layer past the outlines: %u layers, the line not kept", n);
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_KEPT_EXTRA_LAYERS + 4, &t_under);
   q = (const XrCompositionLayerQuad*)layers[4];
   CHECK(n == 5 && q->subImage.imageRect.extent.width == VIDEO_XR_CURSOR_DIM,
         "room for the outlines alone: %u layers", n);
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_KEPT_EXTRA_LAYERS + 2, &t_under);
   q = (const XrCompositionLayerQuad*)layers[0];
   CHECK(n == 3 && q->subImage.imageRect.offset.x
         == VIDEO_XR_OUTLINE_X(0) + VIDEO_XR_OUTLINE_CROP,
         "room for two outlines: %u layers, screen 1's offset %d", n,
         (int)q->subImage.imageRect.offset.x);
   /* No head: no line, nor beam; the rims still go. */
   t_head_tracked = false;
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   q = (const XrCompositionLayerQuad*)layers[0];
   CHECK(     n == 5 && t_under == 2
         && q->subImage.imageRect.offset.x
            == VIDEO_XR_OUTLINE_X(0) + VIDEO_XR_OUTLINE_CROP
         && ((const XrCompositionLayerQuad*)layers[1])
            ->subImage.imageRect.extent.width
            == VIDEO_XR_OUTLINE_DIM - VIDEO_XR_OUTLINE_CROP
         && ((const XrCompositionLayerQuad*)layers[4])
            ->subImage.imageRect.extent.width == VIDEO_XR_CURSOR_DIM,
         "no head: %u layers, screen 1's offset %d", n,
         (int)q->subImage.imageRect.offset.x);
   t_head_tracked = true;

   /* The left hand holds screen 3, on screen 1's top edge: both rims of
    * screen 1 go, two lines, the left hand's first. */
   shown.quads[3]                 = shown.quads[2];
   shown.quads[3].pose.position.y = 0.78f;
   shown.quads[3].screen          = 2;
   shown.quads[3].slot            = 2;
   shown.num_quads                = 4;
   t_xr_frame.grabs.hand[0].slot  = 2;
   t_xr_frame.grabs.hand[0].state = VIDEO_XR_GRAB_HELD;
   t_xr_frame.hinged              = 3u;
   k                              = &t_xr_frame.hinges[0];
   memset(k, 0, sizeof(*k));
   k->point.y = 0.48f;
   k->point.z = -1.8f;
   k->dir.x   = 1.0f;
   k->flat.y  = 1.0f;
   k->edge    = 1;
   k->slot    = 0;
   k->on      = 0;
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   q = (const XrCompositionLayerQuad*)layers[0];
   CHECK(     n == 9 && t_under == 3
         && q->subImage.imageRect.offset.x
            == VIDEO_XR_OUTLINE_X(0) + VIDEO_XR_OUTLINE_CROP
         && q->subImage.imageRect.extent.width
            == VIDEO_XR_OUTLINE_DIM - 2 * VIDEO_XR_OUTLINE_CROP
         && t_near(q->size.width, t_outline(0.96f) * both)
         && t_near(q->pose.position.y, 0.0f),
         "two hands, opposite edges: %u layers, screen 1's offset %d "
         "width %d", n, (int)q->subImage.imageRect.offset.x,
         (int)q->subImage.imageRect.extent.width);
   q = (const XrCompositionLayerQuad*)layers[5];
   t_lifted(-0.4f, 0.48f, -1.8f, 0.4f, 0.48f, -1.8f, &m, &len);
   CHECK(q->subImage.imageRect.offset.x == VIDEO_XR_HINGE_X + 1
         && t_near(q->pose.position.y, m.y)
         && t_near(q->pose.position.z, m.z),
         "the left hand's line is not first");
   q = (const XrCompositionLayerQuad*)layers[6];
   t_lifted(-0.4f, -0.49f, -1.8f, 0.4f, -0.49f, -1.8f, &m, &len);
   CHECK(q->subImage.imageRect.offset.x == VIDEO_XR_HINGE_X + 1
         && t_near(q->pose.position.y, m.y)
         && t_near(q->pose.position.z, m.z),
         "the right hand's line is not second");

   /* Screen 3 on screen 1's right edge instead, adjacent to its bottom:
    * the left hand's rim alone goes there; both lines show. Screen 3
    * seen from behind loses the same side's rim, its left. */
   shown.quads[3].pose.position.x = 1.2f;
   shown.quads[3].pose.position.y = 0.0f;
   t_xr_frame.backs               = 1u << 3;
   memset(k, 0, sizeof(*k));
   k->point.x = 0.8f;
   k->point.z = -1.8f;
   k->dir.y   = 1.0f;
   k->flat.x  = 1.0f;
   k->edge    = 3;
   k->slot    = 0;
   k->on      = 2;
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   q = (const XrCompositionLayerQuad*)layers[0];
   t_layer_axis(q, 1.0f, 0.0f, 0.0f, &ax);
   CHECK(     n == 9 && q->subImage.imageRect.offset.x == VIDEO_XR_OUTLINE_X(0)
         && q->subImage.imageRect.extent.width
            == VIDEO_XR_OUTLINE_DIM - VIDEO_XR_OUTLINE_CROP
         && t_near(ax.x, 1.0f)
         && t_near(q->pose.position.x, -t_outline(1.6f) * half),
         "adjacent edges: %u layers, screen 1's offset %d at x %.4f", n,
         (int)q->subImage.imageRect.offset.x, q->pose.position.x);
   q = (const XrCompositionLayerQuad*)layers[2];
   t_layer_axis(q, 1.0f, 0.0f, 0.0f, &ax);
   CHECK(     q->subImage.imageRect.offset.x == VIDEO_XR_OUTLINE_X(1)
         && q->subImage.imageRect.extent.width
            == VIDEO_XR_OUTLINE_DIM - VIDEO_XR_OUTLINE_CROP
         && t_near(ax.x, -1.0f)
         && t_near(q->pose.position.x, 1.2f + t_outline(0.8f) * half),
         "screen 3 from behind: offset %d at x %.4f",
         (int)q->subImage.imageRect.offset.x, q->pose.position.x);
   q = (const XrCompositionLayerQuad*)layers[5];
   t_lifted(0.8f, -0.3f, -1.8f, 0.8f, 0.3f, -1.8f, &m, &len);
   CHECK(q->subImage.imageRect.offset.x == VIDEO_XR_HINGE_X + 1
         && t_near(q->pose.position.x, m.x)
         && t_near(q->size.width, len),
         "the side line: at x %.4f, %.4f long; want %.4f, %.4f",
         q->pose.position.x, q->size.width, m.x, len);
   memset(&t_xr_frame.grabs, 0, sizeof(t_xr_frame.grabs));
   t_xr_frame.hinged = 0;
   t_xr_frame.backs  = 0;
   t_head_tracked    = false;
   t_edit_end();
}

/* n polls 10 ms apart, the right hand gripping, its trigger held if
 * tilt, and the left stick at y. */
static void t_tilt_polls(unsigned n, float y, bool tilt)
{
   unsigned i;
   t_let_go();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   if (tilt)
      t_hold("edit/tilt", 2, 1.0f, 0.0f);
   t_hold("edit/stick", 1, 0.0f, y);
   for (i = 0; i < n; i++)
   {
      video_xr_pose_t a;
      t_clock += 10000;
      if (t_smooth)
         t_frame_aim(&a);
      t_edit_poll();
   }
}

/* Edit mode with Snap to Screens on, screen 2 taken under screen 1
 * folded fold degrees and carried 7 cm along the edge, so its snaps
 * are whole. */
static void t_take_under(double fold, bool tilt)
{
   t_snap = true;
   t_edit_start();
   t_snap_under(fold);
   t_tilt_polls(1, 0.0f, tilt);
   CHECK(     input_openxr_edit.hand[1].g.state == VIDEO_XR_GRAB_HELD
         && input_openxr_edit.hand[1].g.slot == 1,
         "took slot %u under", input_openxr_edit.hand[1].g.slot);
   t_aim[2].position.x = 0.07f;
   t_tilt_polls(1, 0.0f, tilt);
}

/* Screens resting in a hinge, as the main thread counts them for the
 * frame loop. Let go hinged, screen 2 rests on screen 1's bottom edge,
 * in the record that lets it go. So the frame that first sees the
 * release draws the line and hides both rims, as every frame after;
 * a hand holding screen 2 again makes it give way; short of room the
 * line goes, the rims stay cut. Nothing is counted again in a poll
 * where nothing changed, and a moved screen is. A layout loaded
 * hinged counts once edit mode starts, again when it starts again;
 * edit mode's end and Snap to Screens off count none. */
static void test_edit_rest(void)
{
   unsigned i, n;
   float len, x;
   video_xr_vec3_t m, ax;
   video_xr_quad_set_t shown;
   const XrCompositionLayerQuad *q;
   const XrCompositionLayerBaseHeader *layers[OPENXR_MAX_EXTRA_LAYERS];
   XrSpace local;

   t_take_under(0.0, false);
   CHECK(     input_openxr_edit.hand[1].hinged && t_grabs.num_rests == 0,
         "carried hinged: hinged %d, %u at rest",
         (int)input_openxr_edit.hand[1].hinged, t_grabs.num_rests);
   t_tilt_polls(8, 0.0f, false);
   t_let_go();
   t_edit_poll();
   CHECK(     t_grabs.hand[1].state == VIDEO_XR_GRAB_RELEASED
         && t_grabs.num_rests == 1
         && t_grabs.rests[0].slot == 0 && t_grabs.rests[0].edge == 1
         && t_grabs.rests[0].target == 1 && t_grabs.rests[0].on == 0,
         "let go: state %u, %u at rest, the first %u's edge %u on %u's %u",
         t_grabs.hand[1].state, t_grabs.num_rests, t_grabs.rests[0].slot,
         t_grabs.rests[0].edge, t_grabs.rests[0].target,
         t_grabs.rests[0].on);

   /* The frame loop with that record, the screen let go where it is. */
   local               = t_xr_session.local_space;
   t_xr_frame.grabs    = t_grabs;
   t_xr_frame.hinged   = 0;
   t_aim_tracked[1]    = false;
   t_aim_tracked[2]    = false;
   t_head_tracked      = true;
   t_cursor            = true;
   shown               = t_quads;
   shown.quads[2].pose = t_grabs.let_go[1].last;
   x                   = shown.quads[2].pose.position.x;
   t_lifted(x - 0.4f, -0.48f, -1.8f, x + 0.4f, -0.48f, -1.8f, &m, &len);
   for (i = 0; i < 2; i++)
   {
      n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
            OPENXR_MAX_EXTRA_LAYERS, &t_under);
      q = (const XrCompositionLayerQuad*)layers[0];
      t_layer_axis(q, 1.0f, 0.0f, 0.0f, &ax);
      CHECK(     n == 5 && t_under == 2
            && q->subImage.imageRect.offset.x
               == VIDEO_XR_OUTLINE_X(0) + VIDEO_XR_OUTLINE_CROP
            && q->subImage.imageRect.extent.width
               == VIDEO_XR_OUTLINE_DIM - VIDEO_XR_OUTLINE_CROP
            && t_near(ax.y, 1.0f),
            "frame %u at rest: %u layers, %u under, screen 1's offset %d",
            i, n, t_under, (int)q->subImage.imageRect.offset.x);
      q = (const XrCompositionLayerQuad*)layers[1];
      CHECK(     q->subImage.imageRect.offset.x == VIDEO_XR_OUTLINE_X(0)
            && q->subImage.imageRect.extent.width
               == VIDEO_XR_OUTLINE_DIM - VIDEO_XR_OUTLINE_CROP,
            "frame %u at rest: screen 2's top rim not cut", i);
      q = (const XrCompositionLayerQuad*)layers[4];
      CHECK(     q->subImage.imageRect.offset.x == VIDEO_XR_HINGE_X + 1
            && t_near(q->size.width, len)
            && t_near(q->pose.position.x, m.x)
            && t_near(q->pose.position.y, m.y)
            && t_near(q->pose.position.z, m.z),
            "frame %u at rest: the line %.4f long at %.4f,%.4f,%.4f, want "
            "%.4f at %.4f,%.4f,%.4f", i, q->size.width, q->pose.position.x,
            q->pose.position.y, q->pose.position.z, len, m.x, m.y, m.z);
   }
   /* Short of room: the line goes, the rims stay cut. */
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_KEPT_EXTRA_LAYERS + 4, &t_under);
   q = (const XrCompositionLayerQuad*)layers[0];
   CHECK(n == 4 && q->subImage.imageRect.offset.x
         == VIDEO_XR_OUTLINE_X(0) + VIDEO_XR_OUTLINE_CROP,
         "room for the outlines alone: %u layers, screen 1's offset %d", n,
         (int)q->subImage.imageRect.offset.x);
   /* Held again, its pair gives way, hidden in its slot after the
    * hand's own. */
   t_xr_frame.grabs.hand[1].state = VIDEO_XR_GRAB_HELD;
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   q = (const XrCompositionLayerQuad*)layers[0];
   CHECK(     n == 6
         && q->subImage.imageRect.offset.x == VIDEO_XR_OUTLINE_X(0)
         && q->subImage.imageRect.extent.width == VIDEO_XR_OUTLINE_DIM
         && t_slot_is((const XrCompositionLayerQuad*)layers[4], true,
            x, shown.quads[2].pose.position.y, -1.8f,
            x + VIDEO_XR_HINGE_WIDTH, shown.quads[2].pose.position.y, -1.8f)
         && t_slot_is((const XrCompositionLayerQuad*)layers[5], true,
            0.0f, 0.0f, -1.8f, VIDEO_XR_HINGE_WIDTH, 0.0f, -1.8f),
         "held again: %u layers, screen 1's offset %d width %d", n,
         (int)q->subImage.imageRect.offset.x,
         (int)q->subImage.imageRect.extent.width);

   /* The drawing thread plans it there; then nothing changes. */
   t_quads.quads[2].pose = t_grabs.let_go[1].last;
   t_edit_poll();
   input_openxr_edit.grabs.num_rests = 0;
   t_edit_poll();
   CHECK(t_grabs.num_rests == 0, "counted again with nothing changed");
   t_quads.quads[0].pose.position.x += 0.001f;
   t_edit_poll();
   CHECK(t_grabs.num_rests == 1, "screen 1 moved: %u at rest",
         t_grabs.num_rests);
   t_take_end();

   /* Carried off in the grip's last 80 ms, it is let go hinged, resting
    * in the record that lets it go. */
   t_take_under(0.0, false);
   t_tilt_polls(8, 0.0f, false);
   t_aim[2].position.z += 0.1f;
   t_tilt_polls(1, 0.0f, false);
   CHECK(     !input_openxr_edit.hand[1].hinged && t_grabs.num_rests == 0,
         "carried off: hinged %d, %u at rest",
         (int)input_openxr_edit.hand[1].hinged, t_grabs.num_rests);
   t_let_go();
   t_edit_poll();
   CHECK(     t_grabs.hand[1].state == VIDEO_XR_GRAB_RELEASED
         && t_grabs.num_rests == 1,
         "let go hinged from 80 ms before: state %u, %u at rest",
         t_grabs.hand[1].state, t_grabs.num_rests);
   t_take_end();

   t_snap = true;
   t_edit_start();
   t_rest_under(0.0);
   t_edit_poll();
   CHECK(t_grabs.num_rests == 1, "loaded hinged: %u at rest",
         t_grabs.num_rests);
   input_openxr_edit_toggle();
   t_edit_on(false);
   t_edit_poll();
   CHECK(t_grabs.num_rests == 1, "on again, nothing moved: %u at rest",
         t_grabs.num_rests);
   t_edit_end();
   CHECK(t_grabs.num_rests == 0, "edit mode's end left %u at rest",
         t_grabs.num_rests);
   t_snap = false;
   t_edit_start();
   t_rest_under(0.0);
   t_edit_poll();
   CHECK(t_grabs.num_rests == 0, "Snap to Screens off: %u at rest",
         t_grabs.num_rests);
   memset(&t_xr_frame.grabs, 0, sizeof(t_xr_frame.grabs));
   t_cursor       = false;
   t_head_tracked = false;
   t_edit_end();
}

/* At rest on two adjacent edges, screen 1 loses only the first one's
 * rim, each neighbour its own; both lines show, in the list's order,
 * the first kept when there is room for one. */
static void test_edit_rest_corner(void)
{
   unsigned n;
   float len;
   video_xr_vec3_t m, ax;
   video_xr_quad_set_t shown;
   const XrCompositionLayerQuad *q;
   const XrCompositionLayerBaseHeader *layers[OPENXR_MAX_EXTRA_LAYERS];
   XrSpace local;
   t_edit_start();
   local            = t_xr_session.local_space;
   t_aim_tracked[1] = false;
   t_aim_tracked[2] = false;
   t_head_tracked   = true;
   t_cursor         = true;
   shown            = t_quads;
   /* Screen 2 right of screen 1, screen 3 under it, all touching. */
   shown.quads[2]                 = shown.quads[0];
   shown.quads[2].pose.position.x = 1.6f;
   shown.quads[2].slot            = 1;
   shown.quads[2].screen          = 1;
   shown.quads[3]                 = shown.quads[0];
   shown.quads[3].pose.position.y = -0.96f;
   shown.quads[3].slot            = 2;
   shown.quads[3].screen          = 2;
   shown.num_quads                = 4;
   memset(&t_xr_frame.grabs, 0, sizeof(t_xr_frame.grabs));
   t_xr_frame.hinged                = 0;
   t_xr_frame.grabs.num_rests       = 2;
   t_xr_frame.grabs.rests[0].slot   = 0;
   t_xr_frame.grabs.rests[0].edge   = 2;
   t_xr_frame.grabs.rests[0].target = 1;
   t_xr_frame.grabs.rests[0].on     = 3;
   t_xr_frame.grabs.rests[1].slot   = 0;
   t_xr_frame.grabs.rests[1].edge   = 1;
   t_xr_frame.grabs.rests[1].target = 2;
   t_xr_frame.grabs.rests[1].on     = 0;
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   CHECK(n == 7 && t_under == 3, "%u layers, %u under: want three "
         "outlines under, the menu's and its handle, two lines", n, t_under);
   q = (const XrCompositionLayerQuad*)layers[0];
   t_layer_axis(q, 1.0f, 0.0f, 0.0f, &ax);
   CHECK(     q->subImage.imageRect.offset.x == VIDEO_XR_OUTLINE_X(0)
         && q->subImage.imageRect.extent.width
            == VIDEO_XR_OUTLINE_DIM - VIDEO_XR_OUTLINE_CROP
         && t_near(ax.x, 1.0f),
         "screen 1: its right rim alone cut: offset %d width %d",
         (int)q->subImage.imageRect.offset.x,
         (int)q->subImage.imageRect.extent.width);
   q = (const XrCompositionLayerQuad*)layers[1];
   CHECK(     q->subImage.imageRect.offset.x
            == VIDEO_XR_OUTLINE_X(0) + VIDEO_XR_OUTLINE_CROP
         && q->subImage.imageRect.extent.width
            == VIDEO_XR_OUTLINE_DIM - VIDEO_XR_OUTLINE_CROP,
         "screen 2: its left rim not cut");
   q = (const XrCompositionLayerQuad*)layers[2];
   t_layer_axis(q, 1.0f, 0.0f, 0.0f, &ax);
   CHECK(     q->subImage.imageRect.offset.x == VIDEO_XR_OUTLINE_X(0)
         && q->subImage.imageRect.extent.width
            == VIDEO_XR_OUTLINE_DIM - VIDEO_XR_OUTLINE_CROP
         && t_near(ax.y, 1.0f),
         "screen 3: its top rim not cut");
   q = (const XrCompositionLayerQuad*)layers[5];
   t_lifted(0.8f, -0.48f, -1.8f, 0.8f, 0.48f, -1.8f, &m, &len);
   CHECK(q->subImage.imageRect.offset.x == VIDEO_XR_HINGE_X + 1
         && t_near(q->pose.position.x, m.x) && t_near(q->size.width, len),
         "the first line not on screen 1's right edge");
   q = (const XrCompositionLayerQuad*)layers[6];
   t_lifted(-0.8f, -0.48f, -1.8f, 0.8f, -0.48f, -1.8f, &m, &len);
   CHECK(q->subImage.imageRect.offset.x == VIDEO_XR_HINGE_X + 1
         && t_near(q->pose.position.y, m.y) && t_near(q->size.width, len),
         "the second line not on screen 1's bottom edge");
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_KEPT_EXTRA_LAYERS + 6, &t_under);
   q = (const XrCompositionLayerQuad*)layers[5];
   t_lifted(0.8f, -0.48f, -1.8f, 0.8f, 0.48f, -1.8f, &m, &len);
   CHECK(n == 6 && q->subImage.imageRect.offset.x == VIDEO_XR_HINGE_X + 1
         && t_near(q->pose.position.x, m.x),
         "room for one line: %u layers, not the first", n);
   memset(&t_xr_frame.grabs, 0, sizeof(t_xr_frame.grabs));
   t_cursor       = false;
   t_head_tracked = false;
   t_edit_end();
}

/* Outlines of shown as t_xr_frame has them, each against the same
 * frame with nothing hinged or resting on them: its pose and size the same, its
 * image the whole square, or for want's (in the order the outlines
 * come, a VIDEO_XR_CUT_X cut, -1 none) the cut's cell. Returns how many
 * are not, or are not as many as want. */
static unsigned t_cells_wrong(const video_xr_quad_set_t *shown,
      XrSpace local, const int *want, unsigned outlines)
{
   static XrCompositionLayerQuad cut[OPENXR_MAX_EXTRA_LAYERS];
   static openxr_grabs_t grabs;
   unsigned i, n, m;
   const XrCompositionLayerBaseHeader *layers[OPENXR_MAX_EXTRA_LAYERS];
   unsigned o      = 0;
   unsigned wrong  = 0;
   unsigned hinged = t_xr_frame.hinged;
   n = t_hooks.frame_layers(t_hooks.user, t_time, shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   for (i = 0; i < n; i++)
      cut[i] = *(const XrCompositionLayerQuad*)layers[i];
   /* Rests on the menu's slot hinge nothing but keep their slots. */
   grabs             = t_xr_frame.grabs;
   t_xr_frame.hinged = 0;
   for (i = 0; i < grabs.num_rests; i++)
      t_xr_frame.grabs.rests[i].target = VIDEO_XR_MENU_SLOT;
   m = t_hooks.frame_layers(t_hooks.user, t_time, shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   t_xr_frame.hinged = hinged;
   t_xr_frame.grabs  = grabs;
   if (m != n)
      return outlines + 1;
   for (i = 0; i < n; i++)
   {
      const XrCompositionLayerQuad *w = (const XrCompositionLayerQuad*)
         layers[i];
      const XrCompositionLayerQuad *c = &cut[i];
      int32_t lit, x;
      if (w->subImage.imageRect.extent.height != VIDEO_XR_OUTLINE_DIM)
         continue;
      lit = w->subImage.imageRect.offset.x == VIDEO_XR_OUTLINE_X(1);
      x   = (o < outlines && want[o] >= 0)
         ? VIDEO_XR_CUT_X(want[o], lit) : VIDEO_XR_OUTLINE_X(lit);
      if (     memcmp(&c->pose, &w->pose, sizeof(c->pose))
            || memcmp(&c->size, &w->size, sizeof(c->size))
            || c->subImage.imageRect.offset.x != x
            || c->subImage.imageRect.offset.y != 0
            || c->subImage.imageRect.extent.width != VIDEO_XR_OUTLINE_DIM
            || c->subImage.imageRect.extent.height != VIDEO_XR_OUTLINE_DIM
            || w->subImage.imageRect.extent.width != VIDEO_XR_OUTLINE_DIM)
         wrong++;
      o++;
   }
   return wrong + (o != outlines ? 1u : 0u);
}

/* At full width, every cut a hinge or a rest makes, top, bottom, top
 * and bottom, right, left, right and left, a side from behind, a
 * corner's one rim: each
 * outline keeps its whole pose and size, only its image changes. */
static void test_edit_hinge_cells(void)
{
   /* Outlines: screens 1, 2 and 3, then the menu's. */
   static const int under[]  = { 1, 0, -1 };
   static const int both[]   = { 4, 0, 1, -1 };
   static const int side[]   = { 2, -1, 3, -1 };
   static const int behind[] = { 2, -1, 2, -1 };
   static const int sides[]  = { 5, 2, 3, -1 };
   static const int corner[] = { 2, 3, 0, -1 };
   video_xr_quad_set_t shown;
   video_xr_hinge_t *k;
   unsigned wrong;
   XrSpace local;
   t_edit_start();
   t_snap_under(0.0);
   local            = t_xr_session.local_space;
   t_aim_tracked[1] = false;
   t_head_tracked   = true;
   t_cursor         = true;
   shown            = t_quads;
   /* Screen 2 held under screen 1, hinged by the right hand. */
   shown.quads[2].pose.position.y = -0.80f;
   memset(&t_xr_frame.grabs, 0, sizeof(t_xr_frame.grabs));
   t_xr_frame.grabs.hand[1].slot  = 1;
   t_xr_frame.grabs.hand[1].state = VIDEO_XR_GRAB_HELD;
   t_xr_frame.hinged              = 2u;
   k                              = &t_xr_frame.hinges[1];
   memset(k, 0, sizeof(*k));
   k->point.y = -0.50f;
   k->point.z = -1.8f;
   k->dir.x   = 1.0f;
   k->flat.y  = -1.0f;
   k->gap     = 0.02f;
   k->edge    = 0;
   k->slot    = 0;
   k->on      = 1;
   wrong = t_cells_wrong(&shown, local, under, 3);
   CHECK(!wrong, "under: %u outlines moved or not their cut's cell", wrong);
   /* Screen 3 held on screen 1's top by the left hand. */
   shown.quads[3]                 = shown.quads[2];
   shown.quads[3].pose.position.y = 0.78f;
   shown.quads[3].screen          = 2;
   shown.quads[3].slot            = 2;
   shown.num_quads                = 4;
   t_xr_frame.grabs.hand[0].slot  = 2;
   t_xr_frame.grabs.hand[0].state = VIDEO_XR_GRAB_HELD;
   t_xr_frame.hinged              = 3u;
   k                              = &t_xr_frame.hinges[0];
   memset(k, 0, sizeof(*k));
   k->point.y = 0.48f;
   k->point.z = -1.8f;
   k->dir.x   = 1.0f;
   k->flat.y  = 1.0f;
   k->edge    = 1;
   k->slot    = 0;
   k->on      = 0;
   wrong = t_cells_wrong(&shown, local, both, 4);
   CHECK(!wrong, "top and bottom: %u outlines wrong", wrong);
   /* Screen 3 on screen 1's right edge, the left hand's hinge alone;
    * then screen 3 seen from behind. */
   shown.quads[3].pose.position.x = 1.2f;
   shown.quads[3].pose.position.y = 0.0f;
   t_xr_frame.hinged              = 1u;
   memset(k, 0, sizeof(*k));
   k->point.x = 0.8f;
   k->point.z = -1.8f;
   k->dir.y   = 1.0f;
   k->flat.x  = 1.0f;
   k->edge    = 3;
   k->slot    = 0;
   k->on      = 2;
   wrong = t_cells_wrong(&shown, local, side, 4);
   CHECK(!wrong, "right and left: %u outlines wrong", wrong);
   t_xr_frame.backs = 1u << 3;
   wrong = t_cells_wrong(&shown, local, behind, 4);
   CHECK(!wrong, "from behind: %u outlines wrong", wrong);
   t_xr_frame.backs = 0;
   /* Screen 2 on screen 1's left edge too: screen 1 loses both sides. */
   shown.quads[2].pose.position.x = -1.2f;
   shown.quads[2].pose.position.y = 0.0f;
   t_xr_frame.hinged              = 3u;
   k                              = &t_xr_frame.hinges[1];
   memset(k, 0, sizeof(*k));
   k->point.x = -0.8f;
   k->point.z = -1.8f;
   k->dir.y   = 1.0f;
   k->flat.x  = -1.0f;
   k->edge    = 2;
   k->slot    = 0;
   k->on      = 3;
   wrong = t_cells_wrong(&shown, local, sides, 4);
   CHECK(!wrong, "both sides: %u outlines wrong", wrong);
   /* Let go, screen 2 rests under screen 1. */
   shown.quads[2].pose.position.x   = 0.0f;
   shown.quads[2].pose.position.y   = -0.80f;
   shown.num_quads                  = 3;
   memset(&t_xr_frame.grabs, 0, sizeof(t_xr_frame.grabs));
   t_xr_frame.hinged                = 0;
   t_xr_frame.grabs.num_rests       = 1;
   t_xr_frame.grabs.rests[0].slot   = 1;
   t_xr_frame.grabs.rests[0].edge   = 0;
   t_xr_frame.grabs.rests[0].target = 0;
   t_xr_frame.grabs.rests[0].on     = 1;
   wrong = t_cells_wrong(&shown, local, under, 3);
   CHECK(!wrong, "resting under: %u outlines wrong", wrong);
   /* Screen 1 resting on screen 2's left edge and screen 3's top, as
    * test_edit_rest_corner: one rim kept, its right. */
   shown.quads[2]                   = shown.quads[0];
   shown.quads[2].pose.position.x   = 1.6f;
   shown.quads[2].slot              = 1;
   shown.quads[2].screen            = 1;
   shown.quads[3]                   = shown.quads[0];
   shown.quads[3].pose.position.y   = -0.96f;
   shown.quads[3].slot              = 2;
   shown.quads[3].screen            = 2;
   shown.num_quads                  = 4;
   t_xr_frame.grabs.num_rests       = 2;
   t_xr_frame.grabs.rests[0].slot   = 0;
   t_xr_frame.grabs.rests[0].edge   = 2;
   t_xr_frame.grabs.rests[0].target = 1;
   t_xr_frame.grabs.rests[0].on     = 3;
   t_xr_frame.grabs.rests[1].slot   = 0;
   t_xr_frame.grabs.rests[1].edge   = 1;
   t_xr_frame.grabs.rests[1].target = 2;
   t_xr_frame.grabs.rests[1].on     = 0;
   wrong = t_cells_wrong(&shown, local, corner, 4);
   CHECK(!wrong, "resting in a corner: %u outlines wrong", wrong);
   memset(&t_xr_frame.grabs, 0, sizeof(t_xr_frame.grabs));
   t_xr_frame.hinged = 0;
   t_head_tracked    = false;
   t_cursor          = false;
   t_edit_end();
}

/* Narrower than the cut outlines, with the tags or without: a cut
 * outline as before, cut to its crop, turned for a top or bottom. */
static void test_edit_hinge_crops(void)
{
   static const unsigned widths[2] = { VIDEO_XR_CURSOR_TAGGED_W,
      VIDEO_XR_CURSOR_UNTAGGED_W };
   unsigned i;
   for (i = 0; i < 2; i++)
   {
      t_cursor_w = widths[i];
      test_edit_hinge_layers();
      test_edit_rest();
      test_edit_rest_corner();
   }
   t_cursor_w = VIDEO_XR_CURSOR_W;
}

/* Taken where it rests hinged, screen 2 carries that hinge, its lid and
 * buzz seeded at the grab, its mark in the grab's first publish, and is
 * hinged from the first poll that moves it: no long buzz, no tick,
 * nothing moves. Pulled 6 cm off screen 1's plane it lets go, its mark
 * 150 ms later; carried back 7 cm along, it hinges with the long buzz. */
static void test_edit_rest_take(void)
{
   unsigned n, l;
   video_xr_pose_t at;
   const input_openxr_hold_t *hold = &input_openxr_edit.hand[1];
   t_snap = true;
   t_edit_start();
   t_rest_under(0.0);
   at = t_quads.quads[2].pose;
   n  = t_applies[2];
   l  = t_longs[2];
   /* The grip's poll takes it; the next is the carry's first. */
   t_tilt_polls(1, 0.0f, false);
   CHECK(     hold->g.state == VIDEO_XR_GRAB_HELD && hold->g.slot == 1
         && hold->g.rest_slot == 1 && hold->g.rest_on == 1
         && hold->g.rest_edge == 0 && hold->lidded && hold->buzzed
         && !hold->armed && t_grabs.hand[1].rest_slot == 1
         && t_grabs.hand[1].mark_slot == 1 && t_grabs.hand[1].mark_on == 1
         && t_grabs.hand[1].mark_edge == 0,
         "taken resting hinged: kept %u, lidded %d, buzzed %d, armed %#x, "
         "published mark %u/%u/%u", (unsigned)hold->g.rest_slot,
         (int)hold->lidded, (int)hold->buzzed, hold->armed,
         (unsigned)t_grabs.hand[1].mark_slot,
         (unsigned)t_grabs.hand[1].mark_on,
         (unsigned)t_grabs.hand[1].mark_edge);
   t_tilt_polls(1, 0.0f, false);
   CHECK(     hold->hinged && hold->g.rest_slot == 1
         && hold->lid.slot == 0 && hold->lid.on == 1 && hold->lid.edge == 0
         && t_applies[2] == n
         && fabs(hold->g.last.position.x - at.position.x) < 1e-5
         && fabs(hold->g.last.position.y - at.position.y) < 1e-5
         && fabs(hold->g.last.position.z - at.position.z) < 1e-5,
         "first carried poll: hinged %d, kept %u, lid %u/%u/%u, %u "
         "buzzes, at %.5f,%.5f,%.5f", (int)hold->hinged,
         (unsigned)hold->g.rest_slot, hold->lid.slot, hold->lid.on,
         hold->lid.edge, t_applies[2] - n, hold->g.last.position.x,
         hold->g.last.position.y, hold->g.last.position.z);
   t_tilt_polls(10, 0.0f, false);
   CHECK(hold->hinged && t_applies[2] == n,
         "held still: hinged %d, %u buzzes", (int)hold->hinged,
         t_applies[2] - n);
   t_aim[2].position.z += 0.06f;
   t_tilt_polls(1, 0.0f, false);
   CHECK(!hold->hinged && !hold->g.rest_slot && hold->buzzed,
         "6 cm off: hinged %d, kept %u, buzzed %d", (int)hold->hinged,
         (unsigned)hold->g.rest_slot, (int)hold->buzzed);
   t_tilt_polls(15, 0.0f, false);
   CHECK(!hold->buzzed, "6 cm off 150 ms: buzzed %d", (int)hold->buzzed);
   t_aim[2].position.z -= 0.06f;
   t_aim[2].position.x  = 0.07f;
   t_tilt_polls(1, 0.0f, false);
   CHECK(hold->hinged && t_longs[2] == l + 1,
         "carried back: hinged %d, %u long buzzes", (int)hold->hinged,
         t_longs[2] - l);
   t_take_end();
}

/* Taken resting hinged, the free hand's stick swings screen 2 about the
 * shared edge at once, the edge staying on screen 1's bottom edge. */
static void test_edit_rest_swing(void)
{
   video_xr_vec3_t mid, edge;
   video_xr_pose_t at;
   const input_openxr_hold_t *hold = &input_openxr_edit.hand[1];
   t_snap = true;
   t_edit_start();
   t_rest_under(0.0);
   at = t_quads.quads[2].pose;
   t_tilt_polls(20, 0.5f, false);
   mid.x = 0.0f;
   mid.y = 0.3f;
   mid.z = 0.0f;
   video_xr_rotate(&hold->g.last.orientation, &mid, &edge);
   edge.x += hold->g.last.position.x;
   edge.y += hold->g.last.position.y;
   edge.z += hold->g.last.position.z;
   CHECK(     hold->hinged
         && fabs(hold->g.last.position.z - at.position.z) > 0.005
         && t_near(edge.y, -0.48f) && t_near(edge.z, -1.8f),
         "swung at once: hinged %d, middle z %.4f from %.4f, top edge at "
         "%.4f,%.4f", (int)hold->hinged, hold->g.last.position.z,
         at.position.z, edge.y, edge.z);
   t_take_end();
}

/* Resting hinged to screen 1 above and screen 3 4 cm under, screen 2 is
 * taken on the hinge the snap ranks best: screen 1's, touching. */
static void test_edit_rest_take_two(void)
{
   video_xr_quad_t *q;
   const input_openxr_hold_t *hold = &input_openxr_edit.hand[1];
   t_snap = true;
   t_edit_start();
   t_rest_under(0.0);
   q                       = &t_quads.quads[t_quads.num_quads++];
   *q                      = t_quads.quads[2];
   q->pose.position.y      = -0.48f - 0.6f - 0.04f - 0.3f;
   q->slot                 = 2;
   q->screen               = 2;
   t_quads.slots[2].base.y = q->pose.position.y;
   t_quads.slots[2].base.z = -1.8f;
   t_tilt_polls(2, 0.0f, false);
   CHECK(     hold->g.slot == 1 && hold->hinged && hold->g.rest_slot == 1
         && hold->lid.slot == 0 && hold->lid.on == 1 && hold->lid.edge == 0,
         "two neighbours: hinged %d, kept %u, lid on slot %u's edge %u",
         (int)hold->hinged, (unsigned)hold->g.rest_slot, hold->lid.slot,
         hold->lid.on);
   t_take_end();
}

/* With the left hand holding screen 1, screen 2 is taken as any screen:
 * not hinged, no lid. */
static void test_edit_rest_take_other(void)
{
   const input_openxr_hold_t *hold = &input_openxr_edit.hand[1];
   t_snap = true;
   t_edit_start();
   t_rest_under(0.0);
   t_let_go();
   t_hold("edit/grab", 1, 1.0f, 0.0f);
   t_clock += 10000;
   t_edit_poll();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_clock += 10000;
   t_edit_poll();
   t_clock += 10000;
   t_edit_poll();
   CHECK(     input_openxr_edit.hand[0].g.state == VIDEO_XR_GRAB_HELD
         && input_openxr_edit.hand[0].g.slot == 0
         && hold->g.state == VIDEO_XR_GRAB_HELD && hold->g.slot == 1
         && !hold->hinged && !hold->g.rest_slot && !hold->lidded,
         "the other hand on screen 1: hinged %d, kept %u, lidded %d",
         (int)hold->hinged, (unsigned)hold->g.rest_slot,
         (int)hold->lidded);
   t_take_end();
}

/* Screen 2 taken resting hinged 2 cm under screen 1 at Hinge Gap 2;
 * the left hand then takes screen 1, which ends the hinge kept: it is
 * no longer hinged, nor kept, its buzz spent no more. Screen 1 let go
 * 5 mm higher, screen 2 stays where the hand holds it, as the grab's
 * fade of none has it, not hopping into the hinge; carried 7 cm along,
 * it hinges anew with the long buzz. */
static void test_edit_rest_take_target(void)
{
   unsigned l;
   video_xr_pose_t at;
   const input_openxr_hold_t *hold = &input_openxr_edit.hand[1];
   t_gap  = VIDEO_OPENXR_EDIT_HINGE_GAP_2;
   t_snap = true;
   t_edit_start();
   t_rest_under(0.0);
   t_quads.quads[2].pose.position.y -= 0.02f;
   t_quads.slots[1].base.y          -= 0.02f;
   t_aim[2].position.y              -= 0.02f;
   t_tilt_polls(2, 0.0f, false);
   at = hold->g.last;
   l  = t_longs[2];
   CHECK(hold->hinged && hold->g.rest_slot == 1,
         "taken resting hinged: hinged %d, kept %u", (int)hold->hinged,
         (unsigned)hold->g.rest_slot);
   t_hold("edit/grab", 1, 1.0f, 0.0f);
   t_clock += 10000;
   t_edit_poll();
   CHECK(     input_openxr_edit.hand[0].g.state == VIDEO_XR_GRAB_HELD
         && input_openxr_edit.hand[0].g.slot == 0 && !hold->hinged
         && !hold->g.rest_slot && !hold->buzzed && !hold->lidded
         && !t_grabs.hand[1].rest_slot,
         "screen 1 taken: hinged %d, kept %u, buzzed %d, lidded %d, "
         "published %u", (int)hold->hinged, (unsigned)hold->g.rest_slot,
         (int)hold->buzzed, (int)hold->lidded,
         (unsigned)t_grabs.hand[1].rest_slot);
   t_aim[1].position.y              += 0.005f;
   t_quads.quads[0].pose.position.y += 0.005f;
   t_clock += 10000;
   t_edit_poll();
   t_let_go();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_clock += 10000;
   t_edit_poll();
   t_clock += 10000;
   t_edit_poll();
   CHECK(     input_openxr_edit.hand[0].g.state != VIDEO_XR_GRAB_HELD
         && !hold->hinged && !hold->g.rest_slot && t_longs[2] == l
         && fabs(hold->g.last.position.x - at.position.x) < 1e-5
         && fabs(hold->g.last.position.y - at.position.y) < 1e-5
         && fabs(hold->g.last.position.z - at.position.z) < 1e-5,
         "screen 1 let go 5 mm up: hinged %d, kept %u, %u long buzzes, "
         "at %.5f,%.5f,%.5f", (int)hold->hinged,
         (unsigned)hold->g.rest_slot, t_longs[2] - l,
         hold->g.last.position.x, hold->g.last.position.y,
         hold->g.last.position.z);
   t_aim[2].position.x = 0.07f;
   t_tilt_polls(3, 0.0f, false);
   CHECK(hold->hinged && t_longs[2] == l + 1,
         "carried 7 cm along: hinged %d, %u long buzzes",
         (int)hold->hinged, t_longs[2] - l);
   t_take_end();
   t_gap = VIDEO_OPENXR_EDIT_HINGE_GAP_0;
}

/* Screen 2 taken resting hinged on screen 1 at Hinge Gap 0, a third
 * screen level 10 cm under it. Carried 6 cm down, still in reach of
 * screen 1's edge, the third's 4 cm free gap is nearer: its hinge takes
 * hold, which ends the one kept, with the long buzz once; held there,
 * the hinge stays and nothing moves. Back 3 cm up, screen 1 pulls only
 * as the fade has it, not whole. */
static void test_edit_rest_take_over(void)
{
   unsigned l;
   video_xr_pose_t at;
   video_xr_quad_t *q;
   const input_openxr_hold_t *hold = &input_openxr_edit.hand[1];
   t_snap = true;
   t_edit_start();
   t_rest_under(0.0);
   q                       = &t_quads.quads[t_quads.num_quads++];
   *q                      = t_quads.quads[2];
   q->pose.position.y      = -0.48f - 0.6f - 0.1f - 0.3f;
   q->slot                 = 2;
   q->screen               = 2;
   t_quads.slots[2].base.y = q->pose.position.y;
   t_quads.slots[2].base.z = -1.8f;
   t_tilt_polls(2, 0.0f, false);
   l = t_longs[2];
   CHECK(hold->hinged && hold->g.rest_slot == 1 && hold->lid.slot == 0,
         "taken resting hinged: hinged %d, kept %u, lid on slot %u",
         (int)hold->hinged, (unsigned)hold->g.rest_slot, hold->lid.slot);
   t_aim[2].position.y -= 0.06f;
   t_tilt_polls(1, 0.0f, false);
   at = hold->g.last;
   CHECK(     hold->hinged && hold->lid.slot == 2 && hold->lid.on == 0
         && hold->lid.edge == 1 && !hold->g.rest_slot
         && t_longs[2] == l + 1,
         "carried 6 cm down: hinged %d, lid %u/%u/%u, kept %u, %u long "
         "buzzes, at %.5f", (int)hold->hinged, hold->lid.slot,
         hold->lid.on, hold->lid.edge, (unsigned)hold->g.rest_slot,
         t_longs[2] - l, hold->g.last.position.y);
   t_tilt_polls(3, 0.0f, false);
   CHECK(     hold->hinged && hold->lid.slot == 2 && t_longs[2] == l + 1
         && fabs(hold->g.last.position.x - at.position.x) < 1e-5
         && fabs(hold->g.last.position.y - at.position.y) < 1e-5
         && fabs(hold->g.last.position.z - at.position.z) < 1e-5,
         "held there: hinged %d, lid on slot %u, %u long buzzes, at "
         "%.5f,%.5f,%.5f from %.5f,%.5f,%.5f", (int)hold->hinged,
         hold->lid.slot, t_longs[2] - l, hold->g.last.position.x,
         hold->g.last.position.y, hold->g.last.position.z, at.position.x,
         at.position.y, at.position.z);
   t_aim[2].position.y += 0.03f;
   t_tilt_polls(1, 0.0f, false);
   CHECK(     !(hold->hinged && hold->lid.slot == 0)
         && t_longs[2] == l + 1,
         "back 3 cm up: hinged %d, lid on slot %u, %u long buzzes",
         (int)hold->hinged, hold->lid.slot, t_longs[2] - l);
   t_take_end();
}

/* Screen 2 taken resting hinged on screen 1, then pulled straight off
 * screen 1's plane 2 mm a poll to 4.8 cm and back: no poll moves it
 * more than twice the hand, though the hinge's reach ends with the
 * grab's fade short of whole; its mark shows out to 4.8 cm, within 150
 * ms of leaving its range. Back in the plane, the fade still short, it
 * hinges only anew,
 * with the long buzz once. Taken again and pulled 7 cm off, the hinge
 * kept lasts past the reach until the fade is whole. */
static void test_edit_rest_take_out(void)
{
   unsigned i, l;
   float z;
   const input_openxr_hold_t *hold = &input_openxr_edit.hand[1];
   t_snap = true;
   t_edit_start();
   t_rest_under(0.0);
   t_tilt_polls(2, 0.0f, false);
   l = t_longs[2];
   z = hold->g.last.position.z;
   CHECK(hold->hinged && hold->g.rest_slot == 1,
         "taken resting hinged: hinged %d, kept %u", (int)hold->hinged,
         (unsigned)hold->g.rest_slot);
   for (i = 1; i <= 48; i++)
   {
      unsigned k = (i <= 24) ? i : 48 - i;
      t_aim[2].position.z = 0.002f * k;
      t_tilt_polls(1, 0.0f, false);
      CHECK(fabs(hold->g.last.position.z - z) <= 0.00401,
            "%s %.1f cm off: moved %.2f mm", (i <= 24) ? "out" : "back",
            0.2 * k, fabs(hold->g.last.position.z - z) * 1e3);
      if (i <= 24)
         CHECK(hold->buzzed,
               "out %.1f cm off: buzzed %d", 0.2 * k, (int)hold->buzzed);
      CHECK(!hold->hinged || t_longs[2] == l + (i <= 24 ? 0u : 1u),
            "%s %.1f cm off: hinged %d, %u long buzzes",
            (i <= 24) ? "out" : "back", 0.2 * k, (int)hold->hinged,
            t_longs[2] - l);
      z = hold->g.last.position.z;
   }
   CHECK(hold->hinged && t_longs[2] == l + 1,
         "back in the plane: hinged %d, %u long buzzes", (int)hold->hinged,
         t_longs[2] - l);
   t_take_end();
   t_snap = true;
   t_edit_start();
   t_rest_under(0.0);
   t_tilt_polls(2, 0.0f, false);
   z = hold->g.last.position.z;
   for (i = 1; i <= 35; i++)
   {
      t_aim[2].position.z = 0.002f * i;
      t_tilt_polls(1, 0.0f, false);
      CHECK(fabs(hold->g.last.position.z - z) <= 0.00401,
            "%.1f cm off: moved %.2f mm", 0.2 * i,
            fabs(hold->g.last.position.z - z) * 1e3);
      if (i == 24 || i >= 26)
         CHECK(     (hold->g.rest_slot != 0) == (i == 24)
               && t_grabs.hand[1].rest_slot == hold->g.rest_slot,
               "%.1f cm off: kept %u, published %u", 0.2 * i,
               (unsigned)hold->g.rest_slot,
               (unsigned)t_grabs.hand[1].rest_slot);
      z = hold->g.last.position.z;
   }
   t_take_end();
}

/* Flat at Hinge Gap 0, the pair shown at rest (screen 1 carried) is
 * taken hinged from screen 1's side too, by the quads' own sizes as the
 * list tests them: hinged on screen 2's top edge from the first poll
 * that moves it, no buzz, unmoved. */
static void test_edit_rest_take_sides(void)
{
   unsigned n;
   video_xr_pose_t at;
   const input_openxr_hold_t *hold = &input_openxr_edit.hand[0];
   t_snap = true;
   t_edit_start();
   t_rest_under(0.0);
   t_edit_poll();
   CHECK(     t_grabs.num_rests == 1 && t_grabs.rests[0].slot == 0
         && t_grabs.rests[0].edge == 1 && t_grabs.rests[0].target == 1
         && t_grabs.rests[0].on == 0,
         "at rest: %u, the first %u's edge %u on %u's %u",
         t_grabs.num_rests, t_grabs.rests[0].slot, t_grabs.rests[0].edge,
         t_grabs.rests[0].target, t_grabs.rests[0].on);
   at = t_quads.quads[0].pose;
   n  = t_applies[1];
   t_let_go();
   t_hold("edit/grab", 1, 1.0f, 0.0f);
   t_clock += 10000;
   t_edit_poll();
   CHECK(     hold->g.state == VIDEO_XR_GRAB_HELD && hold->g.slot == 0
         && hold->g.rest_slot == 2 && hold->g.rest_on == 0
         && hold->g.rest_edge == 1 && hold->lidded && hold->buzzed,
         "screen 1 taken resting hinged: kept %u/%u/%u, lidded %d",
         (unsigned)hold->g.rest_slot, (unsigned)hold->g.rest_on,
         (unsigned)hold->g.rest_edge, (int)hold->lidded);
   t_clock += 10000;
   t_edit_poll();
   CHECK(     hold->hinged && hold->lid.slot == 1 && hold->lid.on == 0
         && hold->lid.edge == 1 && t_applies[1] == n
         && fabs(hold->g.last.position.x - at.position.x) < 1e-5
         && fabs(hold->g.last.position.y - at.position.y) < 1e-5
         && fabs(hold->g.last.position.z - at.position.z) < 1e-5,
         "screen 1 carried: hinged %d, lid %u/%u/%u, %u buzzes, at "
         "%.5f,%.5f,%.5f", (int)hold->hinged, hold->lid.slot,
         hold->lid.on, hold->lid.edge, t_applies[1] - n,
         hold->g.last.position.x, hold->g.last.position.y,
         hold->g.last.position.z);
   t_take_end();
}

/* Flat under screen 1, screen 2 is hinged on screen 1's bottom edge:
 * the tilt stick swings it about that edge, which stays on the line,
 * carried or tilting, with a buzz at 15 and at 30. */
static void t_hinge_under(bool tilt)
{
   unsigned i, n;
   double worst = 0.0;
   const char *how = tilt ? "tilting" : "carried";
   t_take_under(0.0, tilt);
   n = t_applies[2];
   for (i = 0; i < 42; i++)
   {
      t_tilt_polls(1, 1.0f, tilt);
      if (t_under_off() > worst)
         worst = t_under_off();
   }
   CHECK(     worst < 1e-3 && t_applies[2] - n == 2
         && t_xr[1].pitch < -30.0f && t_deg_near(t_xr[1].yaw, 0.0),
         "%s under: %.4f off the line, %u buzzes, pitch %.3f yaw %.3f",
         how, worst, t_applies[2] - n, t_xr[1].pitch, t_xr[1].yaw);
   t_take_end();
}

static void test_edit_hinge_under(void)
{
   t_hinge_under(false);
   t_hinge_under(true);
}

/* Screen 2's hinged edge as edit mode last wrote it: metres off the line
 * (from NULL), else moved across it from where it was at from. */
static double t_flick_off(bool under, const video_xr_pose_t *from)
{
   video_xr_pose_t pose;
   if (!from)
      return under ? t_under_off() : t_hinge_off();
   video_xr_item_pose(&t_quads.slots[1].base, &t_xr[1], &pose);
   return t_edge_moved(&pose, from, under);
}

/* Flicked past where the hinge lets go and back, over and over with no
 * let-go, screen 2 keeps its edge on the line: back each time, the
 * hinge holds it whole, and swung on from there the edge stays on the
 * line. At screen 1's right corner past square; under it past 90. At
 * Hinge Gap gap cm, where it hinged is a free gap, and stays put. */
static void t_hinge_flick(bool under, unsigned angles, unsigned gap)
{
   unsigned i, n = under ? 18 : 6;
   double off, worst = 0.0;
   video_xr_pose_t from;
   const char *how = !under ? "corner" : angles ? "under" : "under, Off";
   float in = under ? 1.0f : -1.0f;
   t_angles = angles;
   if (under)
      t_take_under(80.0, false);
   else
      t_take_square(true, false);
   input_openxr_edit.hand[1].g.gap = (unsigned char)gap;
   if (!under)
      t_turn_polls(23, in, 10000, false);
   video_xr_item_pose(&t_quads.slots[1].base, &t_xr[1], &from);
   for (i = 0; i < 8; i++)
   {
      if (under)
      {
         t_tilt_polls(n, in, false);
         t_tilt_polls(n, -in, false);
      }
      else
      {
         t_turn_polls(n, in, 10000, false);
         t_turn_polls(n, -in, 10000, false);
      }
      off = t_flick_off(under, gap ? &from : NULL);
      CHECK(     input_openxr_edit.hand[1].hinged && off < 1e-3
            && t_held_off(under) <= VIDEO_XR_SNAP_DIST,
            "%s, gap %u, flick %u: hinged %d, %.4f off, held %.4f off",
            how, gap, i + 1, input_openxr_edit.hand[1].hinged, off,
            t_held_off(under));
   }
   for (i = 0; i < 20; i++)
   {
      if (under)
         t_tilt_polls(1, -in, false);
      else
         t_turn_polls(1, -in, 10000, false);
      off = t_flick_off(under, gap ? &from : NULL);
      if (off > worst)
         worst = off;
   }
   CHECK(worst < 1e-3, "%s, gap %u, swung on: %.4f off", how, gap, worst);
   t_take_end();
   t_angles = VIDEO_OPENXR_EDIT_SNAP_ANGLES_15;
}

static void test_edit_hinge_flick(void)
{
   /* Touching, 2 cm is as far as the click's fade reaches. */
   static const unsigned gaps[] = { 0, 2, 5 };
   unsigned k;
   for (k = 0; k < 3; k++)
   {
      t_hinge_flick(false, VIDEO_OPENXR_EDIT_SNAP_ANGLES_15, gaps[k]);
      t_hinge_flick(true, VIDEO_OPENXR_EDIT_SNAP_ANGLES_15, gaps[k]);
      t_hinge_flick(true, VIDEO_OPENXR_EDIT_SNAP_ANGLES_OFF, gaps[k]);
   }
}

/* Let go off its hinge past 90 and carried 11.5 cm down, apart in the
 * hinge's fade, so the hinge cannot take it back: pushed back, screen 2
 * is held at the 90 step with the stick still pushing, and its edge
 * stays still. */
static void test_edit_hinge_glide(void)
{
   unsigned i, k, held = 0;
   double worst[2] = { 0.0, 0.0 };
   video_xr_pose_t from[2], pose[2];
   t_take_under(80.0, false);
   t_tilt_polls(16, 1.0f, false);
   t_aim[2].position.y -= 0.115f;
   t_tilt_polls(1, 0.0f, false);
   for (i = 0; i < 12; i++)
   {
      t_tilt_polls(1, -1.0f, false);
      if (input_openxr_edit.hand[1].rest[1] == 0.0f)
         continue;
      video_xr_item_pose(&t_quads.slots[1].base, &t_xr[1], &pose[0]);
      t_held_pose(&pose[1]);
      for (k = 0; k < 2; k++)
      {
         if (!held)
            from[k] = pose[k];
         if (t_edge_moved(&pose[k], &from[k], true) > worst[k])
            worst[k] = t_edge_moved(&pose[k], &from[k], true);
      }
      held++;
   }
   /* Shown, the settings' pitch at 90 is good to about 0.02 degrees. */
   CHECK(     held >= 3 && worst[0] < 1e-3 && worst[1] < 1e-4
         && !input_openxr_edit.hand[1].hinged
         && input_openxr_edit.hand[1].lidded,
         "held %u polls: edge %.4f shown, %.4f held moved, hinged %d,"
         " lidded %d", held, worst[0], worst[1],
         input_openxr_edit.hand[1].hinged, input_openxr_edit.hand[1].lidded);
   t_take_end();
}

/* With the stick at v for 4 polls, tilting screen 2 hinged under screen
 * 1, else turning it hinged on its right edge, screen 2 as the right
 * hand holds it turns about the hinged edge if lid, else its middle;
 * and the hold is lidded as lid. */
static void t_lid_swing(float v, bool under, bool lid, const char *how)
{
   unsigned i;
   double edge = 0.0, mid = 0.0;
   video_xr_pose_t from, pose;
   if (under)
      t_tilt_polls(1, 0.0f, false);
   else
      t_turn_polls(1, 0.0f, 10000, false);
   t_held_pose(&from);
   for (i = 0; i < 4; i++)
   {
      double x[3];
      if (under)
         t_tilt_polls(1, v, false);
      else
         t_turn_polls(1, v, 10000, false);
      t_held_pose(&pose);
      x[0] = (double)pose.position.x - from.position.x;
      x[1] = (double)pose.position.y - from.position.y;
      x[2] = (double)pose.position.z - from.position.z;
      if (t_edge_moved(&pose, &from, under) > edge)
         edge = t_edge_moved(&pose, &from, under);
      if (sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]) > mid)
         mid = sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
   }
   CHECK(     (lid ? edge : mid) < 1e-4 && (lid ? mid : edge) > 0.01
         && !input_openxr_edit.hand[1].hinged
         && input_openxr_edit.hand[1].lidded == lid,
         "%s: edge %.4f, middle %.4f moved, hinged %d, lidded %d", how,
         edge, mid, input_openxr_edit.hand[1].hinged,
         input_openxr_edit.hand[1].lidded);
}

/* Let go off its hinge past 90, screen 2 swings on about that edge while
 * the edge is within 3 cm of screen 1's plane. Carried 3.5 cm off, it
 * turns about its middle for the rest of the hold, even back within
 * reach; hinged again, the new hinge holds it as the first did. */
static void test_edit_hinge_lid_far(void)
{
   video_xr_pose_t pose;
   video_xr_vec3_t m, w;
   t_angles = VIDEO_OPENXR_EDIT_SNAP_ANGLES_OFF;
   t_take_under(80.0, false);
   t_tilt_polls(16, 1.0f, false);
   t_aim[2].position.z += 0.025f;
   t_lid_swing(1.0f, true, true, "2.5 cm off");
   t_lid_swing(-1.0f, true, true, "2.5 cm off, back");
   t_aim[2].position.z += 0.01f;
   t_lid_swing(1.0f, true, false, "3.5 cm off");
   t_aim[2].position.z -= 0.01f;
   t_lid_swing(-1.0f, true, false, "2.5 cm off again");
   /* Turned inside 90 about its middle, then carried onto the line. */
   t_tilt_polls(6, -1.0f, false);
   t_held_pose(&pose);
   m.x  = 0.0f;
   m.y  = 0.3f;
   m.z  = 0.0f;
   video_xr_rotate(&pose.orientation, &m, &w);
   t_aim[2].position.y -= pose.position.y + w.y + 0.48f;
   t_aim[2].position.z -= pose.position.z + w.z + 1.8f;
   t_tilt_polls(1, 0.0f, false);
   CHECK(     input_openxr_edit.hand[1].hinged
         && input_openxr_edit.hand[1].lidded,
         "hinged again: hinged %d, lidded %d",
         input_openxr_edit.hand[1].hinged,
         input_openxr_edit.hand[1].lidded);
   t_tilt_polls(8, 1.0f, false);
   t_lid_swing(1.0f, true, true, "let go again");
   t_take_end();
   t_angles = VIDEO_OPENXR_EDIT_SNAP_ANGLES_15;
}

/* Screen 2 taken flush right of screen 1 and carried 7 cm up, so the
 * hinge on screen 1's right edge holds it, with Snap to Angles Off. */
static void t_take_flush(void)
{
   t_angles = VIDEO_OPENXR_EDIT_SNAP_ANGLES_OFF;
   t_snap   = true;
   t_edit_start();
   t_snap_beside();
   t_quads.quads[t_quads.num_quads - 1].pose.position.x = 1.20f;
   t_quads.slots[1].base.x = 1.20f;
   t_aim[2].position.x     = 1.20f;
   t_turn_polls(1, 0.0f, 10000, false);
   t_aim[2].position.y = 0.07f;
   t_turn_polls(1, 0.0f, 10000, false);
   CHECK(     input_openxr_edit.hand[1].hinged
         && input_openxr_edit.hand[1].lidded,
         "flush: hinged %d, lidded %d", input_openxr_edit.hand[1].hinged,
         input_openxr_edit.hand[1].lidded);
}

/* Hinged flush beside screen 1, then carried 1.5 m along its plane and
 * 0.4 m up, out of that hinge's reach, its mark gone: the stick turns
 * screen 2 about its middle, not as a door on that edge. */
static void test_edit_hinge_door(void)
{
   t_take_flush();
   t_aim[2].position.x += 1.5f;
   t_aim[2].position.y += 0.4f;
   t_turn_polls(15, 0.0f, 10000, false);
   t_lid_swing(1.0f, false, false, "carried 1.5 m along");
   t_take_end();
   t_angles = VIDEO_OPENXR_EDIT_SNAP_ANGLES_15;
}

/* Hinged flush beside screen 1 and carried off it, its mark gone,
 * screen 2 turns about that edge while the hinge could take it again:
 * 12.5 cm out, in the hinge's fade, or 1.5 cm still alongside screen
 * 1's edge. 13.5 cm out, or 1.5 cm past that edge's end, about its
 * middle. */
static void test_edit_hinge_lid_reach(void)
{
   t_take_flush();
   t_aim[2].position.x += 0.125f;
   t_turn_polls(16, 0.0f, 10000, false);
   CHECK(!input_openxr_edit.hand[1].buzzed, "12.5 cm out 150 ms: buzzed");
   t_lid_swing(1.0f, false, true, "12.5 cm out");
   t_lid_swing(-1.0f, false, true, "12.5 cm out, back");
   t_aim[2].position.x += 0.01f;
   t_turn_polls(15, 0.0f, 10000, false);
   t_lid_swing(1.0f, false, false, "13.5 cm out");
   t_take_end();
   t_take_flush();
   t_aim[2].position.y = 0.765f;
   t_turn_polls(16, 0.0f, 10000, false);
   CHECK(!input_openxr_edit.hand[1].buzzed,
         "1.5 cm alongside 150 ms: buzzed");
   t_lid_swing(1.0f, false, true, "1.5 cm alongside");
   t_lid_swing(-1.0f, false, true, "1.5 cm alongside, back");
   t_aim[2].position.y = 0.795f;
   t_turn_polls(15, 0.0f, 10000, false);
   t_lid_swing(1.0f, false, false, "1.5 cm past its end");
   t_take_end();
   t_angles = VIDEO_OPENXR_EDIT_SNAP_ANGLES_15;
}

/* A poll 10 ms on, both hands gripping and no stick. */
static void t_both_polls(unsigned n)
{
   unsigned i;
   t_let_go();
   t_hold("edit/grab", 1, 1.0f, 0.0f);
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   for (i = 0; i < n; i++)
   {
      t_clock += 10000;
      t_edit_poll();
   }
}

/* Let go off its hinge past 90, screen 2's lid ends as the left hand
 * takes screen 1: carried 1 cm toward the viewer, then 3.5 cm, screen 2
 * turns about its middle. */
static void test_edit_hinge_lid_moved(void)
{
   t_angles = VIDEO_OPENXR_EDIT_SNAP_ANGLES_OFF;
   t_take_under(80.0, false);
   t_tilt_polls(16, 1.0f, false);
   t_both_polls(1);
   CHECK(input_openxr_edit.hand[0].g.state == VIDEO_XR_GRAB_HELD
         && input_openxr_edit.hand[0].g.slot == 0,
         "the left hand took slot %u", input_openxr_edit.hand[0].g.slot);
   t_aim[1].position.z += 0.01f;
   t_both_polls(2);
   CHECK(     !input_openxr_edit.hand[1].hinged
         && !input_openxr_edit.hand[1].lidded,
         "screen 1 1 cm on: hinged %d, lidded %d",
         input_openxr_edit.hand[1].hinged,
         input_openxr_edit.hand[1].lidded);
   t_aim[1].position.z += 0.025f;
   t_both_polls(2);
   CHECK(!input_openxr_edit.hand[1].lidded,
         "screen 1 3.5 cm on: lidded %d", input_openxr_edit.hand[1].lidded);
   t_lid_swing(1.0f, true, false, "screen 1 moved");
   t_take_end();
   t_angles = VIDEO_OPENXR_EDIT_SNAP_ANGLES_15;
}

/* A new hold starts with no lid: let go off its hinge past 90, then
 * dropped and taken again folded past 90, so never hinged though its
 * edge is still on the line, screen 2 turns about its middle. */
static void test_edit_hinge_lid_new_hold(void)
{
   t_angles = VIDEO_OPENXR_EDIT_SNAP_ANGLES_OFF;
   t_take_under(80.0, false);
   t_tilt_polls(16, 1.0f, false);
   CHECK(input_openxr_edit.hand[1].lidded, "let go past 90: lidded %d",
         input_openxr_edit.hand[1].lidded);
   t_take_end();
   t_take_under(95.0, false);
   t_lid_swing(1.0f, true, false, "taken again past 90");
   t_take_end();
   t_angles = VIDEO_OPENXR_EDIT_SNAP_ANGLES_15;
}

/* Let go off its hinge past 90, then dropped and taken again with no
 * snap, so never hinged, screen 2 tilts to the 90 step about its
 * middle, not the edge that hinge held. */
static void test_edit_hinge_lid_ends(void)
{
   unsigned i;
   double worst = 0.0;
   video_xr_pose_t first, pose;
   t_take_under(80.0, false);
   t_tilt_polls(16, 1.0f, false);
   CHECK(     !input_openxr_edit.hand[1].hinged
         && input_openxr_edit.hand[1].lidded,
         "let go past 90: hinged %d, lidded %d",
         input_openxr_edit.hand[1].hinged, input_openxr_edit.hand[1].lidded);
   t_take_end();
   t_edit_start();
   t_snap_under(80.0);
   t_tilt_polls(1, 0.0f, false);
   video_xr_item_pose(&t_quads.slots[1].base, &t_xr[1], &first);
   for (i = 0; i < 12; i++)
   {
      double x, y, z;
      t_tilt_polls(1, 1.0f, false);
      video_xr_item_pose(&t_quads.slots[1].base, &t_xr[1], &pose);
      x = (double)pose.position.x - first.position.x;
      y = (double)pose.position.y - first.position.y;
      z = (double)pose.position.z - first.position.z;
      if (sqrt(x * x + y * y + z * z) > worst)
         worst = sqrt(x * x + y * y + z * z);
   }
   /* 10.8 tilted, held at 90 with 0.8 past it. */
   CHECK(     worst < 1e-4
         && fabs(fabs(input_openxr_edit.hand[1].rest[1]) - 0.8) < 1e-3
         && !input_openxr_edit.hand[1].lidded,
         "taken again: middle %.4f off, rest %.3f, lidded %d", worst,
         input_openxr_edit.hand[1].rest[1],
         input_openxr_edit.hand[1].lidded);
   t_take_end();
}

/* Swung to 75, then the stick pushed back and forth on a diagonal past
 * the axis lock, with no let-go: the tilt takes screen 2 off the hinge
 * and back each time, and it comes back level and on the line, the
 * hand's own tilt never traded for the pull's. */
static void test_edit_hinge_diagonal(void)
{
   unsigned i, j, k;
   for (k = 0; k < 2; k++)
   {
      float y = k ? -1.0f : 1.0f;
      t_take_square(true, false);
      t_turn_polls(16, -1.0f, 10000, false);
      t_turn_polls(11, 1.0f, 10000, false);
      for (i = 0; i < 6; i++)
      {
         for (j = 0; j < 24; j++)
            t_step_polls(1, j < 12 ? 1.0f : -1.0f, j < 12 ? y : -y);
         CHECK(     input_openxr_edit.hand[1].hinged
               && t_deg_near(t_xr[1].pitch, 0.0) && t_hinge_off() < 1e-3
               && t_held_off(false) <= VIDEO_XR_SNAP_DIST,
               "diagonal %u, %u: hinged %d, pitch %.3f, %.4f off, held %.4f"
               " off",
               k, i + 1, input_openxr_edit.hand[1].hinged, t_xr[1].pitch,
               t_hinge_off(), t_held_off(false));
      }
      t_take_end();
   }
}

/* Under screen 1 at 30 from flat, taken 4 cm left of the centres'
 * level and carried 3.5 to 5.5 cm right of it, where its pull fades:
 * pushed, swung and let go, screen 2 never moves along the edge,
 * carried or tilting. */
static void test_edit_hinge_level(void)
{
   static const float xs[] = { 0.035f, 0.045f, 0.055f };
   unsigned i, j, k;
   for (k = 0; k < 2; k++)
   {
      for (j = 0; j < 3; j++)
      {
         float x0, pitch0;
         double worst = 0.0;
         t_snap = true;
         t_edit_start();
         t_snap_under(30.0);
         t_quads.quads[t_quads.num_quads - 1].pose.position.x = -0.04f;
         t_quads.slots[1].base.x = -0.04f;
         t_aim[2].position.x     = -0.04f;
         t_tilt_polls(1, 0.0f, k != 0);
         t_aim[2].position.x     = xs[j];
         t_tilt_polls(3, 0.0f, k != 0);
         x0     = t_xr[1].x;
         pitch0 = t_xr[1].pitch;
         for (i = 0; i < 26; i++)
         {
            t_tilt_polls(1, (i >= 3 && i < 23) ? 1.0f : 0.0f, k != 0);
            if (fabs(t_xr[1].x - x0) > worst)
               worst = fabs(t_xr[1].x - x0);
         }
         CHECK(     input_openxr_edit.hand[1].hinged && worst < 1e-4
               && t_under_off() < 1e-3
               && fabs(t_xr[1].pitch - pitch0) > 5.0f,
               "%s %.3f along: hinged %d, %.4f along, %.4f off, pitch %.3f"
               " from %.3f", k ? "tilting" : "carried", xs[j],
               input_openxr_edit.hand[1].hinged, worst, t_under_off(),
               t_xr[1].pitch, pitch0);
         t_take_end();
      }
   }
}

/* Screen 2's hinged edge's middle as edit mode last wrote it: its place
 * along the line, x under screen 1, y beside it. */
static double t_edge_along(bool under)
{
   video_xr_pose_t pose;
   video_xr_vec3_t m, w;
   video_xr_item_pose(&t_quads.slots[1].base, &t_xr[1], &pose);
   m.x = under ? 0.0f : -0.4f;
   m.y = under ? 0.3f : 0.0f;
   m.z = 0.0f;
   video_xr_rotate(&pose.orientation, &m, &w);
   return under ? (double)pose.position.x + w.x
      : (double)pose.position.y + w.y;
}

/* Screen 2, as t_snap_under placed it, and the hand on it, d metres
 * further under screen 1, across its bottom edge in its plane. */
static void t_gap_shift(double d)
{
   t_quads.quads[t_quads.num_quads - 1].pose.position.y -= (float)d;
   t_quads.slots[1].base.y -= (float)d;
   t_aim[2].position.y     -= (float)d;
}

/* Screen 2's top edge's middle as edit mode last wrote it: how far
 * under screen 1's bottom edge in screen 1's plane, and in *plane how
 * far out of that plane. */
static double t_under_gap(double *plane)
{
   video_xr_pose_t pose;
   video_xr_vec3_t m, w;
   video_xr_item_pose(&t_quads.slots[1].base, &t_xr[1], &pose);
   m.x = 0.0f;
   m.y = 0.3f;
   m.z = 0.0f;
   video_xr_rotate(&pose.orientation, &m, &w);
   *plane = (double)pose.position.z + w.z + 1.8;
   return -0.48 - ((double)pose.position.y + w.y);
}

static void t_qmul(const XrQuaternionf *a, const XrQuaternionf *b,
      XrQuaternionf *o)
{
   XrQuaternionf r;
   r.w = a->w * b->w - a->x * b->x - a->y * b->y - a->z * b->z;
   r.x = a->w * b->x + a->x * b->w + a->y * b->z - a->z * b->y;
   r.y = a->w * b->y - a->x * b->z + a->y * b->w + a->z * b->x;
   r.z = a->w * b->z + a->x * b->y - a->y * b->x + a->z * b->w;
   *o = r;
}

/* The right hand's aim turned pitch, yaw and roll degrees about its own
 * x, y and z, in that order. */
static void t_turn_aim(double pitch, double yaw, double roll)
{
   unsigned i;
   double deg[3];
   deg[0] = pitch;
   deg[1] = yaw;
   deg[2] = roll;
   for (i = 0; i < 3; i++)
   {
      XrQuaternionf d;
      double half = deg[i] * M_PI / 360.0;
      memset(&d, 0, sizeof(d));
      d.w = (float)cos(half);
      if (i == 0)
         d.x = (float)sin(half);
      else if (i == 1)
         d.y = (float)sin(half);
      else
         d.z = (float)sin(half);
      t_qmul(&t_aim[2].orientation, &d, &t_aim[2].orientation);
   }
}

/* Edit mode at Hinge Gap gap, screen 2 taken under screen 1 folded fold
 * degrees, apart metres further under and x along, and carried 7 cm on
 * along the edge, polled until it hinges. */
static void t_hinged_along(unsigned gap, double fold, double apart,
      double x)
{
   unsigned i;
   t_gap  = gap;
   t_snap = true;
   t_edit_start();
   t_snap_under(fold);
   t_gap_shift(apart);
   t_quads.quads[t_quads.num_quads - 1].pose.position.x = (float)x;
   t_aim[2].position.x = (float)x;
   t_tilt_polls(1, 0.0f, false);
   t_aim[2].position.x = (float)(x + 0.07);
   for (i = 0; i < 10 && !input_openxr_edit.hand[1].hinged; i++)
      t_tilt_polls(1, 0.0f, false);
}

static void t_hinged_at(unsigned gap, double fold, double apart)
{
   t_hinged_along(gap, fold, apart, 0.0);
}

/* Tilting with the hand rolled 1 to 4 degrees, the stick swings screen
 * 2 from 12 degrees through flat's fade band under screen 1, or from
 * about 12 short through square's at its corner: its hinged edge never
 * slides along the line. */
static void test_edit_hinge_rolled(void)
{
   static const double rolls[] = { 1.0, 2.0, 4.0 };
   unsigned i, j, k;
   for (k = 0; k < 2; k++)
   {
      for (j = 0; j < 3; j++)
      {
         double r = rolls[j] * M_PI / 180.0;
         double last, worst = 0.0;
         if (k)
         {
            t_take_square(true, true);
            t_turn_polls(16, -1.0f, 10000, true);
            t_turn_polls(13, 1.0f, 10000, true);
            t_turn_polls(1, 0.0f, 10000, true);
         }
         else
            t_take_under(12.0, true);
         t_aim[2].orientation.z = (float)sin(r / 2.0);
         t_aim[2].orientation.w = (float)cos(r / 2.0);
         if (k)
            t_turn_polls(2, 0.0f, 10000, true);
         else
            t_tilt_polls(2, 0.0f, true);
         last = t_edge_along(!k);
         for (i = 0; i < 16; i++)
         {
            if (k)
               t_turn_polls(1, -1.0f, 10000, true);
            else
               t_tilt_polls(1, -1.0f, true);
            if (fabs(t_edge_along(!k) - last) > worst)
               worst = fabs(t_edge_along(!k) - last);
            last = t_edge_along(!k);
         }
         CHECK(     input_openxr_edit.hand[1].hinged && worst < 1e-4
               && (k ? t_deg_near(t_xr[1].yaw, -90.0)
                     : fabs(t_xr[1].pitch) < 1.0f),
               "%s rolled %.0f: hinged %d, %.4f along a poll, yaw %.3f"
               " pitch %.3f", k ? "square" : "flat", rolls[j],
               input_openxr_edit.hand[1].hinged, worst, t_xr[1].yaw,
               t_xr[1].pitch);
         t_take_end();
      }
   }
}

/* Hinge Gap 2 cm, screen 2 taken under screen 1 at 30 degrees from
 * flat, its top edge under by d, carried 7 cm along so its snaps are
 * whole. */
static void t_take_gap(double d, bool tilt)
{
   t_gap  = VIDEO_OPENXR_EDIT_HINGE_GAP_2;
   t_snap = true;
   t_edit_start();
   t_snap_under(30.0);
   t_gap_shift(d);
   t_tilt_polls(1, 0.0f, tilt);
   t_aim[2].position.x = 0.07f;
   t_tilt_polls(3, 0.0f, tilt);
}

/* Snap to Screens as snap, Hinge Gap gap, screen 2 taken touching
 * under screen 1 at 30 degrees from flat and carried 7 cm along, so
 * its snaps are whole; the hand then c metres higher, pushing it past
 * touching. Returns the hand's height touching. */
static double t_take_stop(bool snap, unsigned gap, double c)
{
   double y;
   t_gap  = gap;
   t_snap = snap;
   t_edit_start();
   t_snap_under(30.0);
   y = t_aim[2].position.y;
   t_tilt_polls(1, 0.0f, false);
   t_aim[2].position.x = 0.07f;
   t_aim[2].position.y = (float)(y + c);
   t_tilt_polls(3, 0.0f, false);
   return y;
}

/* Hinged, pushed 0.5 to 2.9 cm past touching, screen 2 shows touching
 * at Hinge Gap 0, 2 and 10; shaken 3 mm every way 5 mm or 2.5 cm past,
 * and shaken in depth and along held exactly touching, it stays
 * touching; let go there, it is saved touching. 4 cm past it is partly
 * stopped, 5.5 cm past not at all; 3 mm apart it shows as the gap
 * leaves it. Snap to Screens off, 2 cm past it overlaps. */
static void test_edit_gap_stop(void)
{
   static const unsigned gaps[] = { VIDEO_OPENXR_EDIT_HINGE_GAP_0,
      VIDEO_OPENXR_EDIT_HINGE_GAP_2, VIDEO_OPENXR_EDIT_HINGE_GAP_10 };
   static const unsigned cm[]    = { 0, 2, 10 };
   static const double apart[]   = { 0.0, 0.006, 0.003 };
   static const double past[]    = { 0.005, 0.01, 0.025, 0.029 };
   unsigned i, j, k;
   double plane, gap, y, z;
   for (i = 0; i < 3; i++)
   {
      for (j = 0; j < 4; j++)
      {
         double worst = 0.0;
         y   = t_take_stop(true, gaps[i], past[j]);
         z   = t_aim[2].position.z;
         gap = t_under_gap(&plane);
         CHECK(     input_openxr_edit.hand[1].hinged && fabs(gap) < 1e-5
               && fabs(plane) < 1e-5,
               "Hinge Gap %u cm, %.3f past: hinged %d, gap %.5f, plane %.5f",
               cm[i], past[j], input_openxr_edit.hand[1].hinged, gap,
               plane);
         for (k = 0; k < 80 && !(j & 1); k++)
         {
            double c = (k < 40) ? past[j] + 0.003 * sin(k * 2.1) : 0.0;
            t_aim[2].position.x = (float)(0.07 + 0.003 * sin(k * 1.3));
            t_aim[2].position.y = (float)(y + c);
            t_aim[2].position.z = (float)(z + 0.003 * sin(k * 0.7));
            t_tilt_polls(1, 0.0f, false);
            gap = t_under_gap(&plane);
            if (fabs(gap) > worst)
               worst = fabs(gap);
         }
         t_tilt_polls(8, 0.0f, false);
         t_let_go();
         t_clock += 10000;
         t_edit_poll();
         gap = t_under_gap(&plane);
         CHECK(worst < 1e-5 && fabs(gap) < 1e-5 && fabs(plane) < 1e-5,
               "Hinge Gap %u cm, %.3f past: shaken %.5f off, saved %.5f",
               cm[i], past[j], worst, gap);
         t_take_end();
      }
      t_take_stop(true, gaps[i], 0.04);
      gap = t_under_gap(&plane);
      CHECK(gap < -0.001 && gap > -0.0399,
            "Hinge Gap %u cm, 4 cm past: gap %.5f", cm[i], gap);
      t_take_end();
      t_take_stop(true, gaps[i], 0.055);
      gap = t_under_gap(&plane);
      CHECK(fabs(gap + 0.055) < 1e-4,
            "Hinge Gap %u cm, 5.5 cm past: gap %.5f", cm[i], gap);
      t_take_end();
      t_take_stop(true, gaps[i], -0.003);
      gap = t_under_gap(&plane);
      CHECK(     input_openxr_edit.hand[1].hinged
            && fabs(gap - apart[i]) < 1e-5,
            "Hinge Gap %u cm, 3 mm apart: hinged %d, gap %.5f, want %.3f",
            cm[i], input_openxr_edit.hand[1].hinged, gap, apart[i]);
      t_take_end();
   }
   t_take_stop(false, VIDEO_OPENXR_EDIT_HINGE_GAP_2, 0.02);
   gap = t_under_gap(&plane);
   CHECK(fabs(gap + 0.02) < 1e-4, "Snap to Screens off, 2 cm past: gap "
         "%.5f", gap);
   t_take_end();
   t_gap = VIDEO_OPENXR_EDIT_HINGE_GAP_0;
}

/* Hinge Gap 1 cm: its click's fade shows touching 5 mm past, so
 * screen 2 shows touching from there to 35 mm past, 0.5 to 3.4 cm
 * past; shaken 3 mm every way about 1 or 3 cm past at 8 Hz it shows
 * touching on every poll, and let go there it is saved touching. */
static void test_edit_gap_stop_one(void)
{
   static const double past[] = { 0.005, 0.01, 0.014, 0.025, 0.034 };
   unsigned i, j, k;
   double plane, gap, y, z;
   for (j = 0; j < 5; j++)
   {
      t_take_stop(true, VIDEO_OPENXR_EDIT_HINGE_GAP_1, past[j]);
      gap = t_under_gap(&plane);
      CHECK(     input_openxr_edit.hand[1].hinged && fabs(gap) < 1e-5
            && fabs(plane) < 1e-5,
            "Hinge Gap 1 cm, %.3f past: hinged %d, gap %.5f, plane %.5f",
            past[j], input_openxr_edit.hand[1].hinged, gap, plane);
      t_take_end();
   }
   for (i = 0; i < 2; i++)
   {
      double worst = 0.0;
      y = t_take_stop(true, VIDEO_OPENXR_EDIT_HINGE_GAP_1, i ? 0.03 : 0.01);
      z = t_aim[2].position.z;
      for (k = 0; k < 450; k++)
      {
         t_aim[2].position.x = (float)(0.07 + 0.003 * sin(k * 1.3));
         t_aim[2].position.y = (float)(y + (i ? 0.03 : 0.01)
               + 0.003 * sin(k * 0.5));
         t_aim[2].position.z = (float)(z + 0.003 * sin(k * 0.7));
         t_tilt_polls(1, 0.0f, false);
         gap = t_under_gap(&plane);
         if (fabs(gap) > worst)
            worst = fabs(gap);
      }
      t_let_go();
      t_clock += 10000;
      t_edit_poll();
      gap = t_under_gap(&plane);
      CHECK(worst < 1e-5 && fabs(gap) < 1e-5 && fabs(plane) < 1e-5,
            "Hinge Gap 1 cm, shaken about %u cm past: %.5f off, saved "
            "%.5f", i ? 3u : 1u, worst, gap);
      t_take_end();
   }
   t_gap = VIDEO_OPENXR_EDIT_HINGE_GAP_0;
}

/* Screen 2 carried into its hinge under screen 1 and pushed on 1 mm a
 * poll to 5 cm past touching and back, at Hinge Gap 0, 1 and 2: hinged
 * and lidded to VIDEO_XR_SNAP_STOP past where its stop starts, touching
 * from there, then neither, the lid once its mark has gone 150 ms on,
 * the push marking it through; back, neither until
 * VIDEO_XR_SNAP_STOP_ARM short of there, then hinged touching, no
 * longer through. No poll moves it more than the fade's steepest, 3.8
 * times the hand at Hinge Gap 1. */
static void test_edit_gap_push(void)
{
   static const unsigned gaps[] = { VIDEO_OPENXR_EDIT_HINGE_GAP_0,
      VIDEO_OPENXR_EDIT_HINGE_GAP_1, VIDEO_OPENXR_EDIT_HINGE_GAP_2 };
   static const unsigned cm[] = { 0, 1, 2 };
   const input_openxr_hold_t *hold = &input_openxr_edit.hand[1];
   unsigned i, k;
   for (i = 0; i < 3; i++)
   {
      unsigned from = (cm[i] == 1) ? 5u : 0u;
      double plane, gap, last;
      double y = t_take_stop(true, gaps[i], 0.0);
      last     = t_under_gap(&plane);
      for (k = 1; k <= 100; k++)
      {
         unsigned x = (k <= 50) ? k : 100 - k;
         bool want  = x <= from + ((k <= 50) ? 30u : 25u);
         bool lid   = want || (k <= 50 && x <= from + 45u);
         t_aim[2].position.y = (float)(y + 0.001 * x);
         t_tilt_polls(1, 0.0f, false);
         gap = t_under_gap(&plane);
         CHECK(fabs(gap - last) <= 0.00381,
               "Hinge Gap %u cm, %s %u mm past: moved %.2f mm", cm[i],
               (k <= 50) ? "in" : "back", x, fabs(gap - last) * 1e3);
         CHECK(     hold->hinged == want && hold->lidded == lid
               && (hold->g.through != 0) == !want
               && (!want || x < from || fabs(gap) < 1e-5),
               "Hinge Gap %u cm, %s %u mm past: hinged %d, lidded %d, "
               "through %u, gap %.5f", cm[i], (k <= 50) ? "in" : "back", x,
               (int)hold->hinged, (int)hold->lidded,
               (unsigned)hold->g.through, gap);
         last = gap;
      }
      t_take_end();
   }
   t_gap = VIDEO_OPENXR_EDIT_HINGE_GAP_0;
}

/* Screen 2 hinged under screen 1 at Hinge Gap 0, 2 and 1, shaken 3 mm
 * every way at 8 Hz for 2 s about where the push lets its hinge go (3
 * cm past touching, 3.5 at Gap 1): it ends once and stays ended, its
 * mark and lid kept, as the hand comes back within the stop's hold
 * inside 150 ms. Then shaken about VIDEO_XR_SNAP_STOP_ARM short of
 * there: it takes hold once and stays hinged. */
static void test_edit_gap_push_shaken(void)
{
   static const unsigned gaps[] = { VIDEO_OPENXR_EDIT_HINGE_GAP_0,
      VIDEO_OPENXR_EDIT_HINGE_GAP_2, VIDEO_OPENXR_EDIT_HINGE_GAP_1 };
   static const unsigned cm[]   = { 0, 2, 1 };
   const input_openxr_hold_t *hold = &input_openxr_edit.hand[1];
   unsigned i, k;
   for (i = 0; i < 3; i++)
   {
      unsigned ends  = 0;
      unsigned takes = 0;
      double from    = (cm[i] == 1) ? 0.005 : 0.0;
      double y       = t_take_stop(true, gaps[i], from + 0.02);
      double z       = t_aim[2].position.z;
      bool was       = hold->hinged;
      for (k = 0; k < 400; k++)
      {
         double c = from + ((k < 200) ? 0.03 : 0.025);
         t_aim[2].position.x = (float)(0.07 + 0.003 * sin(k * 1.3));
         t_aim[2].position.y = (float)(y + c + 0.003 * sin(k * 0.5));
         t_aim[2].position.z = (float)(z + 0.003 * sin(k * 0.7));
         t_tilt_polls(1, 0.0f, false);
         ends  += (was && !hold->hinged) ? 1u : 0u;
         takes += (!was && hold->hinged) ? 1u : 0u;
         was    = hold->hinged;
         if (k == 199)
            CHECK(     ends == 1 && takes == 0 && hold->lidded
                  && hold->buzzed,
                  "Hinge Gap %u cm, shaken about its end: ended %u, "
                  "taken %u, lidded %d, buzzed %d", cm[i], ends, takes,
                  (int)hold->lidded, (int)hold->buzzed);
      }
      CHECK(     ends == 1 && takes == 1 && hold->hinged && hold->lidded
            && !hold->g.through,
            "Hinge Gap %u cm, shaken 5 mm short of its end: ended %u, "
            "taken %u, hinged %d, through %u", cm[i], ends, takes,
            (int)hold->hinged, (unsigned)hold->g.through);
      t_take_end();
   }
   t_gap = VIDEO_OPENXR_EDIT_HINGE_GAP_0;
}

/* Screen 2 pushed through its hinge to 5 cm past touching at Hinge Gap
 * 0, 2 and 1 gives no long buzz; back to 1 mm short of where it takes
 * hold again (2.5 cm past, 3 at Gap 1), still none; shaken 3 mm at 8
 * Hz for 2 s about there, one; carried back to touching, still one. */
static void test_edit_gap_push_buzz(void)
{
   static const unsigned gaps[] = { VIDEO_OPENXR_EDIT_HINGE_GAP_0,
      VIDEO_OPENXR_EDIT_HINGE_GAP_2, VIDEO_OPENXR_EDIT_HINGE_GAP_1 };
   static const unsigned cm[]   = { 0, 2, 1 };
   const input_openxr_hold_t *hold = &input_openxr_edit.hand[1];
   unsigned i, k;
   for (i = 0; i < 3; i++)
   {
      unsigned from = (cm[i] == 1) ? 5u : 0u;
      double y      = t_take_stop(true, gaps[i], 0.0);
      unsigned l    = t_longs[2];
      for (k = 1; k <= 74 - from; k++)
      {
         unsigned x = (k <= 50) ? k : 100 - k;
         t_aim[2].position.y = (float)(y + 0.001 * x);
         t_tilt_polls(1, 0.0f, false);
      }
      CHECK(!hold->hinged && t_longs[2] == l,
            "Hinge Gap %u cm, through and back to %u mm: hinged %d, %u "
            "long buzzes", cm[i], from + 26, (int)hold->hinged,
            t_longs[2] - l);
      for (k = 0; k < 200; k++)
      {
         t_aim[2].position.y = (float)(y + 0.001 * from + 0.025
               + 0.003 * cos(k * 0.5));
         t_tilt_polls(1, 0.0f, false);
      }
      CHECK(hold->hinged && t_longs[2] == l + 1,
            "Hinge Gap %u cm, shaken about %u mm: hinged %d, %u long "
            "buzzes", cm[i], from + 25, (int)hold->hinged, t_longs[2] - l);
      for (k = from + 25; k-- > 0;)
      {
         t_aim[2].position.y = (float)(y + 0.001 * k);
         t_tilt_polls(1, 0.0f, false);
      }
      CHECK(hold->hinged && t_longs[2] == l + 1,
            "Hinge Gap %u cm, back to touching: hinged %d, %u long buzzes",
            cm[i], (int)hold->hinged, t_longs[2] - l);
      t_take_end();
   }
   t_gap = VIDEO_OPENXR_EDIT_HINGE_GAP_0;
}

/* Screen 2 hinged under screen 1 flat at Hinge Gap 0, carried 3.6 cm
 * toward the user, its lid and mark kept 150 ms; pushed 3.2 cm past
 * touching there, the push ends the hinge, so back in screen 1's plane,
 * overlapping, the published mark goes 150 ms after it left. */
static void test_edit_gap_push_off(void)
{
   double y;
   const input_openxr_hold_t *hold = &input_openxr_edit.hand[1];
   t_gap  = VIDEO_OPENXR_EDIT_HINGE_GAP_0;
   t_snap = true;
   t_edit_start();
   t_snap_under(0.0);
   y = t_aim[2].position.y;
   t_tilt_polls(1, 0.0f, false);
   t_aim[2].position.x = 0.07f;
   t_tilt_polls(1, 0.0f, false);
   CHECK(hold->hinged, "carried in: hinged %d", (int)hold->hinged);
   t_aim[2].position.z += 0.036f;
   t_tilt_polls(1, 0.0f, false);
   CHECK(hold->lidded && hold->buzzed && !hold->g.through,
         "3.6 cm off: lidded %d, buzzed %d, through %u",
         (int)hold->lidded, (int)hold->buzzed, (unsigned)hold->g.through);
   t_aim[2].position.y = (float)(y + 0.032);
   t_tilt_polls(1, 0.0f, false);
   CHECK(hold->g.through, "pushed 3.2 cm past, off: through %u",
         (unsigned)hold->g.through);
   t_aim[2].position.z -= 0.036f;
   t_tilt_polls(2, 0.0f, false);
   CHECK(     !hold->hinged && hold->g.through
         && input_openxr_edit.grabs.hand[1].through
         && hold->buzzed && input_openxr_edit.grabs.hand[1].mark_slot == 1,
         "back in the plane: hinged %d, through %u, published %u, "
         "buzzed %d, published mark %u", (int)hold->hinged,
         (unsigned)hold->g.through,
         (unsigned)input_openxr_edit.grabs.hand[1].through,
         (int)hold->buzzed,
         (unsigned)input_openxr_edit.grabs.hand[1].mark_slot);
   t_tilt_polls(12, 0.0f, false);
   CHECK(     !hold->hinged && !hold->buzzed
         && !input_openxr_edit.grabs.hand[1].mark_slot,
         "back in the plane, 150 ms out: hinged %d, buzzed %d, published "
         "mark %u", (int)hold->hinged, (int)hold->buzzed,
         (unsigned)input_openxr_edit.grabs.hand[1].mark_slot);
   t_take_end();
}

/* Screen 2 hinged under screen 1 at Hinge Gap 0, 1 and 2, pushed 1 mm a
 * poll past touching and let go: the pair shows at rest exactly when
 * its hinge was marked as it was let go, settled touching, and taken
 * again it is hinged exactly then. So at Gap 0 and 2, 1, 2 and 2.9 cm
 * past it is, and 3.2, inside the mark's 150 ms past the stop's end;
 * 4 and 6 cm past it is not, nor back at 2.8 or 2.6 cm from 4 cm; back
 * at 2.3 cm it is again. At Gap 1, whose stop starts 5 mm past, let go
 * touching from there to 4 cm past it rests hinged too. */
static void test_edit_gap_push_rest(void)
{
   static const unsigned gaps[] = { VIDEO_OPENXR_EDIT_HINGE_GAP_0,
      VIDEO_OPENXR_EDIT_HINGE_GAP_1, VIDEO_OPENXR_EDIT_HINGE_GAP_2 };
   static const unsigned cm[]   = { 0, 1, 2 };
   static const unsigned peak[] = { 10, 20, 29, 32, 40, 60, 40, 40, 40 };
   static const unsigned at[]   = { 10, 20, 29, 32, 40, 60, 28, 26, 23 };
   const input_openxr_hold_t *hold = &input_openxr_edit.hand[1];
   unsigned i, j, k;
   for (i = 0; i < 3; i++)
   {
      unsigned from = (cm[i] == 1) ? 5u : 0u;
      for (j = 0; j < 9; j++)
      {
         bool was, marked;
         unsigned rests;
         double plane, before, after;
         bool want = at[j] <= from + ((peak[j] <= from + 30u) ? 30u : 25u);
         /* Held 8 polls where it stopped, its mark kept up to 15 polls
          * past the stop's end (test_edit_gap_push). */
         bool mark_want = want
            || (peak[j] == at[j] && at[j] + 8u <= from + 45u);
         double y  = t_take_stop(true, gaps[i], 0.0);
         for (k = 1; k <= peak[j]; k++)
         {
            t_aim[2].position.y = (float)(y + 0.001 * k);
            t_tilt_polls(1, 0.0f, false);
         }
         for (k = peak[j]; k-- > at[j];)
         {
            t_aim[2].position.y = (float)(y + 0.001 * k);
            t_tilt_polls(1, 0.0f, false);
         }
         t_tilt_polls(8, 0.0f, false);
         was    = hold->hinged;
         marked = hold->buzzed;
         before = t_under_gap(&plane);
         CHECK(was == want && marked == mark_want,
               "Hinge Gap %u cm, %u mm past from %u: hinged %d, marked %d",
               cm[i], at[j], peak[j], (int)was, (int)marked);
         t_let_go();
         t_clock += 10000;
         t_edit_poll();
         rests = t_grabs.num_rests;
         after = t_under_gap(&plane);
         /* Shown where it was saved, as the driver publishes it. */
         for (k = 0; k < t_quads.num_quads; k++)
            if (t_quads.quads[k].slot == 1)
               video_xr_item_pose(&t_quads.slots[1].base, &t_xr[1],
                     &t_quads.quads[k].pose);
         t_tilt_polls(2, 0.0f, false);
         CHECK(     (rests == 1) == mark_want
               && (hold->g.rest_slot != 0) == mark_want
               && hold->hinged == mark_want
               && fabs(after - ((mark_want && !was) ? 0.0 : before)) < 1e-5
               && (!mark_want
                  || fabs(t_under_gap(&plane) - (was ? before : 0.0))
                  < 1e-5),
               "Hinge Gap %u cm, let go %u mm past from %u, hinged %d, "
               "mark wanted %d: %u at rest, let go at %.5f, taken again kept "
               "%u, hinged %d, gap %.5f from %.5f", cm[i], at[j], peak[j],
               (int)was, (int)mark_want, rests, after,
               (unsigned)hold->g.rest_slot, (int)hold->hinged,
               t_under_gap(&plane), before);
         t_take_end();
      }
   }
   t_gap = VIDEO_OPENXR_EDIT_HINGE_GAP_0;
}

/* Snap ticks on the right hand while screen 2, under screen 1 stacked
 * flat or folded 30 degrees and carried 7 cm along, is shaken 2 mm at
 * 8 Hz for 5 s about touching, at each Hinge Gap: pushed there inside
 * the hinge's long buzz, its tick is skipped and none follows; pushed
 * from clearly apart (past the click's reach at 0 to 2 cm, past
 * VIDEO_XR_SNAP_STOP_ARM and short of the click from 3) one tick, the
 * click's or the stop's, and one again each time it parts so and is
 * pushed again. */
static void test_edit_gap_tick(void)
{
   static const unsigned gaps[] = { VIDEO_OPENXR_EDIT_HINGE_GAP_0,
      VIDEO_OPENXR_EDIT_HINGE_GAP_1, VIDEO_OPENXR_EDIT_HINGE_GAP_2,
      VIDEO_OPENXR_EDIT_HINGE_GAP_3, VIDEO_OPENXR_EDIT_HINGE_GAP_5,
      VIDEO_OPENXR_EDIT_HINGE_GAP_10 };
   static const unsigned cm[] = { 0, 1, 2, 3, 5, 10 };
   unsigned i, k, push;
   for (i = 0; i < 12; i++)
   {
      double apart = (cm[i % 6] < 3) ? 0.01 * cm[i % 6] + 0.025 : 0.008;
      double y;
      unsigned n;
      t_gap  = gaps[i % 6];
      t_snap = true;
      t_edit_start();
      t_snap_under((i < 6) ? 0.0 : 30.0);
      y = t_aim[2].position.y;
      t_tilt_polls(1, 0.0f, false);
      for (push = 0; push < 3; push++)
      {
         n = t_applies[2] - t_longs[2];
         if (push)
         {
            t_aim[2].position.y = (float)(y - apart);
            t_tilt_polls(20, 0.0f, false);
            n = t_applies[2] - t_longs[2];
            for (k = 1; k <= 10; k++)
            {
               t_aim[2].position.y = (float)(y - apart
                     + (apart + 0.001) * k / 10.0);
               t_tilt_polls(1, 0.0f, false);
            }
         }
         t_aim[2].position.x = 0.07f;
         for (k = 0; k < 500; k++)
         {
            t_aim[2].position.y = (float)(y + 0.002 * sin(k * 0.5));
            t_tilt_polls(1, 0.0f, false);
         }
         CHECK(     input_openxr_edit.hand[1].hinged
               && t_applies[2] - t_longs[2] == n + (push ? 1 : 0),
               "%s, Hinge Gap %u cm, push %u: hinged %d, %u ticks",
               (i < 6) ? "flat" : "folded 30", cm[i % 6], push,
               input_openxr_edit.hand[1].hinged,
               t_applies[2] - t_longs[2] - n);
      }
      t_take_end();
   }
   t_gap = VIDEO_OPENXR_EDIT_HINGE_GAP_0;
}

/* Screen 2 taken at screen 1's right corner, pushed 5 mm past touching
 * and turned 5 degrees off square, short or past, at Hinge Gap 2, 3 and
 * 10 cm; the hand's turn then shaken 0.2 degrees at 8 Hz for 4.5 s
 * about that edge of the turn's whole snap, moving it nothing like
 * clearly apart: no tick repeats. */
static void test_edit_gap_tick_turned(void)
{
   static const unsigned gaps[] = { VIDEO_OPENXR_EDIT_HINGE_GAP_2,
      VIDEO_OPENXR_EDIT_HINGE_GAP_3, VIDEO_OPENXR_EDIT_HINGE_GAP_10 };
   static const unsigned cm[]   = { 2, 3, 10 };
   unsigned i, k, side;
   for (i = 0; i < 3; i++)
   for (side = 0; side < 2; side++)
   {
      double turn = side ? -23.0 : -13.0;
      /* Turned about its middle, its edge's middle sits 0.4 m along. */
      float push  = (float)(0.4 * cos((turn - 72.0) * M_PI / 180.0)
            - 0.025);
      unsigned n;
      t_gap = gaps[i];
      t_take_square(true, false);
      t_point(turn);
      t_aim[2].position.x += push;
      t_turn_polls(20, 0.0f, 10000, false);
      n = t_applies[2] - t_longs[2];
      for (k = 0; k < 450; k++)
      {
         t_point(turn + (side ? -0.2 : 0.2) * sin(k * 0.5));
         t_aim[2].position.x += push;
         t_turn_polls(1, 0.0f, 10000, false);
      }
      CHECK(     input_openxr_edit.hand[1].hinged
            && t_applies[2] - t_longs[2] <= n + 1,
            "%s square, Hinge Gap %u cm: hinged %d, %u ticks",
            side ? "past" : "short of", cm[i],
            input_openxr_edit.hand[1].hinged,
            t_applies[2] - t_longs[2] - n);
      t_take_end();
   }
   t_gap = VIDEO_OPENXR_EDIT_HINGE_GAP_0;
}

/* The stick swings screen 2 from 30 degrees, carried or tilting: taken
 * 2.6 cm under it shows 2 (the click), 3.5 under 3 (its fade), 6 under
 * 6 (free); the gap holds through the swing. Let go, it is saved so,
 * but from the fade settled at 2. */
static void test_edit_gap_swing(void)
{
   static const double under[]   = { 0.026, 0.035, 0.06 };
   static const double shown[]   = { 0.02, 0.03, 0.06 };
   static const double settled[] = { 0.02, 0.02, 0.06 };
   unsigned i, k, u;
   t_angles = VIDEO_OPENXR_EDIT_SNAP_ANGLES_OFF;
   for (u = 0; u < 3; u++)
   for (k = 0; k < 2; k++)
   {
      double plane, gap, worst = 0.0;
      float pitch0;
      t_take_gap(under[u], k != 0);
      gap = t_under_gap(&plane);
      CHECK(     input_openxr_edit.hand[1].hinged
            && fabs(gap - shown[u]) < 1e-4 && fabs(plane) < 1e-4,
            "taken %.3f under: hinged %d, gap %.4f, plane %.4f",
            under[u], input_openxr_edit.hand[1].hinged, gap, plane);
      pitch0 = t_xr[1].pitch;
      for (i = 0; i < 20; i++)
      {
         t_tilt_polls(1, 1.0f, k != 0);
         gap = t_under_gap(&plane);
         if (fabs(gap - shown[u]) > worst)
            worst = fabs(gap - shown[u]);
         if (fabs(plane) > worst)
            worst = fabs(plane);
      }
      t_tilt_polls(3, 0.0f, k != 0);
      t_let_go();
      t_clock += 10000;
      t_edit_poll();
      gap = t_under_gap(&plane);
      CHECK(     worst < 1e-4 && fabs(gap - settled[u]) < 1e-4
            && fabs(t_xr[1].pitch - pitch0) > 5.0f,
            "%s from %.3f under: worst %.5f, saved gap %.4f, pitch %.3f"
            " from %.3f", k ? "tilting" : "carried", under[u], worst, gap,
            t_xr[1].pitch, pitch0);
      t_take_end();
   }
   t_gap    = VIDEO_OPENXR_EDIT_HINGE_GAP_0;
   t_angles = VIDEO_OPENXR_EDIT_SNAP_ANGLES_15;
}

/* Snap to Angles 15 with a 2 cm gap: the swing stops on a step from
 * flat, the gap kept. */
static void test_edit_gap_steps(void)
{
   unsigned i;
   double plane, gap, off;
   t_take_gap(0.026, false);
   for (i = 0; i < 16; i++)
      t_tilt_polls(1, 1.0f, false);
   t_tilt_polls(3, 0.0f, false);
   gap = t_under_gap(&plane);
   off = fmod(fabs(t_xr[1].pitch) + 0.001, 15.0);
   CHECK(     input_openxr_edit.hand[1].hinged && off < 0.011
         && fabs(gap - 0.02) < 1e-4 && fabs(plane) < 1e-4,
         "pitch %.3f (%.3f off a step), gap %.4f, plane %.4f",
         t_xr[1].pitch, off, gap, plane);
   t_take_end();
   t_gap = VIDEO_OPENXR_EDIT_HINGE_GAP_0;
}

/* The menu's value reaches the snap: 5 cm clicks from 5.6; a value
 * past the list is 2 cm; Snap to Screens off, no gap and no hinge. */
static void test_edit_gap_menu(void)
{
   double plane, gap;
   t_gap  = VIDEO_OPENXR_EDIT_HINGE_GAP_5;
   t_snap = true;
   t_edit_start();
   t_snap_under(30.0);
   t_gap_shift(0.056);
   t_tilt_polls(1, 0.0f, false);
   t_aim[2].position.x = 0.07f;
   t_tilt_polls(3, 0.0f, false);
   gap = t_under_gap(&plane);
   CHECK(fabs(gap - 0.05) < 1e-4, "Hinge Gap 5: gap %.4f", gap);
   t_take_end();
   t_gap  = VIDEO_OPENXR_EDIT_HINGE_GAP_LAST;
   t_snap = true;
   t_edit_start();
   t_snap_under(30.0);
   t_gap_shift(0.026);
   t_tilt_polls(1, 0.0f, false);
   t_aim[2].position.x = 0.07f;
   t_tilt_polls(3, 0.0f, false);
   gap = t_under_gap(&plane);
   CHECK(fabs(gap - 0.02) < 1e-4, "past the list: gap %.4f", gap);
   t_take_end();
   t_snap = false;
   t_gap  = VIDEO_OPENXR_EDIT_HINGE_GAP_2;
   t_edit_start();
   t_snap_under(30.0);
   t_gap_shift(0.026);
   t_tilt_polls(1, 0.0f, false);
   t_aim[2].position.x = 0.07f;
   t_tilt_polls(3, 0.0f, false);
   gap = t_under_gap(&plane);
   CHECK(     !input_openxr_edit.hand[1].hinged
         && fabs(gap - 0.026) < 1e-4,
         "Snap to Screens off: hinged %d, gap %.4f",
         input_openxr_edit.hand[1].hinged, gap);
   t_take_end();
   t_gap = VIDEO_OPENXR_EDIT_HINGE_GAP_0;
}

/* Carried to 2.5 cm under, a 2 cm gap set, from 3.5 or from a free 6,
 * where the plane holds its axes already and only the gap's own snap
 * can buzz: the click takes hold with one buzz on the holding hand. */
static void test_edit_gap_buzz(void)
{
   static const double from[] = { 0.035, 0.06 };
   unsigned i, n;
   for (i = 0; i < 2; i++)
   {
      t_take_gap(from[i], false);
      t_clock += T_BUZZ_CLEAR;
      n = t_applies[2];
      t_aim[2].position.y += (float)(from[i] - 0.025);
      t_tilt_polls(3, 0.0f, false);
      CHECK(t_applies[2] == n + 1,
            "%u buzzes as the gap clicked from %.3f under, want 1",
            t_applies[2] - n, from[i]);
      t_take_end();
   }
   t_gap = VIDEO_OPENXR_EDIT_HINGE_GAP_0;
}

/* A hinge taking hold buzzes once on the holding hand, longer and
 * stronger than a tick; held there, no more. */
static void test_edit_hinge_buzz(void)
{
   unsigned n, l, left;
   CHECK(     T_TICK_NS == INPUT_OPENXR_SNAP_TICK_NS
         && (retro_time_t)T_BUZZ_CLEAR * 1000 > INPUT_OPENXR_HINGE_BUZZ_NS,
         "the fake's tick or the harness's wait is off");
   t_snap = true;
   t_edit_start();
   t_snap_under(0.0);
   t_tilt_polls(1, 0.0f, false);
   CHECK(!input_openxr_edit.hand[1].hinged, "hinged as it was taken");
   n    = t_applies[2];
   l    = t_longs[2];
   left = t_applies[1];
   t_aim[2].position.x = 0.07f;
   t_tilt_polls(1, 0.0f, false);
   CHECK(     input_openxr_edit.hand[1].hinged && t_applies[2] == n + 1
         && t_longs[2] == l + 1 && t_dur[2] == INPUT_OPENXR_HINGE_BUZZ_NS
         && t_near(t_amp[2], INPUT_OPENXR_HINGE_BUZZ_AMP)
         && t_applies[1] == left,
         "hinged %d: %u buzzes, %u long, the last %lld ns at %.2f",
         (int)input_openxr_edit.hand[1].hinged, t_applies[2] - n,
         t_longs[2] - l, (long long)t_dur[2], t_amp[2]);
   t_tilt_polls(20, 0.0f, false);
   CHECK(t_applies[2] == n + 1, "held on the hinge: %u buzzes",
         t_applies[2] - n);
   t_take_end();
}

/* Ticks due while a hinge's buzz plays are skipped, never replayed:
 * taken 6 cm under, a free gap, screen 2 hinges with the long buzz;
 * carried into the 2 cm click 30 ms on, it clicks with no tick, the
 * click spent, and none once the buzz is over; out of the click's pull
 * and back, past the buzz, the click ticks. */
static void test_edit_hinge_buzz_skip(void)
{
   unsigned n = t_applies[2];
   unsigned l = t_longs[2];
   t_take_gap(0.06, false);
   CHECK(     input_openxr_edit.hand[1].hinged && t_applies[2] == n + 1
         && t_longs[2] == l + 1,
         "taken 6 cm under: hinged %d, %u buzzes, %u long",
         (int)input_openxr_edit.hand[1].hinged, t_applies[2] - n,
         t_longs[2] - l);
   t_aim[2].position.y += 0.035f;
   t_tilt_polls(3, 0.0f, false);
   CHECK(     t_applies[2] == n + 1
         && !(input_openxr_edit.hand[1].armed & VIDEO_XR_SNAP_GAP),
         "clicked inside the buzz: %u buzzes, armed %#x", t_applies[2] - n,
         input_openxr_edit.hand[1].armed);
   t_tilt_polls(20, 0.0f, false);
   CHECK(t_applies[2] == n + 1, "after the buzz: %u buzzes",
         t_applies[2] - n);
   t_aim[2].position.y -= 0.035f;
   t_tilt_polls(3, 0.0f, false);
   t_aim[2].position.y += 0.035f;
   t_tilt_polls(3, 0.0f, false);
   CHECK(     t_applies[2] == n + 2 && t_longs[2] == l + 1
         && t_dur[2] == INPUT_OPENXR_SNAP_TICK_NS,
         "out of the click and back: %u buzzes, %u long",
         t_applies[2] - n, t_longs[2] - l);
   t_take_end();
   t_gap = VIDEO_OPENXR_EDIT_HINGE_GAP_0;
}

/* Swung past 90 and back, its lid kept: the hinge does not buzz again. */
static void test_edit_hinge_buzz_flick(void)
{
   unsigned l;
   t_angles = VIDEO_OPENXR_EDIT_SNAP_ANGLES_OFF;
   t_take_under(80.0, false);
   CHECK(input_openxr_edit.hand[1].hinged, "not hinged at 80");
   l = t_longs[2];
   t_tilt_polls(16, 1.0f, false);
   CHECK(     !input_openxr_edit.hand[1].hinged
         && input_openxr_edit.hand[1].lidded,
         "past 90: hinged %d, lidded %d",
         (int)input_openxr_edit.hand[1].hinged,
         (int)input_openxr_edit.hand[1].lidded);
   t_tilt_polls(16, -1.0f, false);
   CHECK(input_openxr_edit.hand[1].hinged && t_longs[2] == l,
         "back from past 90: hinged %d, %u long buzzes",
         (int)input_openxr_edit.hand[1].hinged, t_longs[2] - l);
   t_take_end();
   t_angles = VIDEO_OPENXR_EDIT_SNAP_ANGLES_15;
}

/* Carried 4 cm off screen 1's plane, out of the stick's 3 cm reach,
 * its lid and mark kept for 150 ms: back on, no buzz. 5 cm off for 150
 * ms, its mark gone and the lid with it: back on, the hinge buzzes
 * again. */
static void test_edit_hinge_buzz_again(void)
{
   unsigned l = t_longs[2];
   t_take_under(0.0, false);
   CHECK(input_openxr_edit.hand[1].hinged && t_longs[2] == l + 1,
         "taken: hinged %d, %u long buzzes",
         (int)input_openxr_edit.hand[1].hinged, t_longs[2] - l);
   l = t_longs[2];
   t_aim[2].position.z += 0.04f;
   t_tilt_polls(1, 0.0f, false);
   CHECK(     !input_openxr_edit.hand[1].hinged
         && input_openxr_edit.hand[1].lidded,
         "4 cm off: hinged %d, lidded %d",
         (int)input_openxr_edit.hand[1].hinged,
         (int)input_openxr_edit.hand[1].lidded);
   t_aim[2].position.z -= 0.04f;
   t_tilt_polls(1, 0.0f, false);
   CHECK(input_openxr_edit.hand[1].hinged && t_longs[2] == l,
         "back from 4 cm: hinged %d, %u long buzzes",
         (int)input_openxr_edit.hand[1].hinged, t_longs[2] - l);
   t_aim[2].position.z += 0.05f;
   t_tilt_polls(16, 0.0f, false);
   CHECK(     !input_openxr_edit.hand[1].lidded
         && !input_openxr_edit.hand[1].buzzed,
         "5 cm off 150 ms: lidded %d, buzzed %d",
         (int)input_openxr_edit.hand[1].lidded,
         (int)input_openxr_edit.hand[1].buzzed);
   t_aim[2].position.z -= 0.05f;
   t_tilt_polls(1, 0.0f, false);
   CHECK(     input_openxr_edit.hand[1].hinged && t_longs[2] == l + 1
         && t_dur[2] == INPUT_OPENXR_HINGE_BUZZ_NS,
         "back from 5 cm: hinged %d, %u long buzzes",
         (int)input_openxr_edit.hand[1].hinged, t_longs[2] - l);
   t_take_end();
}

/* Screen 2 carried into its hinge on screen 1; the left hand then takes
 * screen 1, which ends the hinge: no lid, its buzz spent no more.
 * Screen 1 let go 5 mm higher, screen 2 hinges on it anew, with the
 * long buzz once. */
static void test_edit_hinge_take_target(void)
{
   unsigned i, l;
   const input_openxr_hold_t *hold = &input_openxr_edit.hand[1];
   t_take_under(0.0, false);
   l = t_longs[2];
   CHECK(hold->hinged && hold->lidded && hold->buzzed,
         "carried in: hinged %d, lidded %d, buzzed %d", (int)hold->hinged,
         (int)hold->lidded, (int)hold->buzzed);
   t_hold("edit/grab", 1, 1.0f, 0.0f);
   t_clock += 10000;
   t_edit_poll();
   CHECK(     input_openxr_edit.hand[0].g.state == VIDEO_XR_GRAB_HELD
         && input_openxr_edit.hand[0].g.slot == 0 && !hold->hinged
         && !hold->lidded && !hold->buzzed,
         "screen 1 taken: hinged %d, lidded %d, buzzed %d",
         (int)hold->hinged, (int)hold->lidded, (int)hold->buzzed);
   t_aim[1].position.y              += 0.005f;
   t_quads.quads[0].pose.position.y += 0.005f;
   t_clock += 10000;
   t_edit_poll();
   for (i = 0; i < 3; i++)
   {
      t_tilt_polls(1, 0.0f, false);
      CHECK(hold->hinged && t_longs[2] == l + 1,
            "screen 1 let go 5 mm up, poll %u: hinged %d, %u long buzzes",
            i, (int)hold->hinged, t_longs[2] - l);
   }
   t_aim[2].position.x = 0.0f;
   t_tilt_polls(3, 0.0f, false);
   t_aim[2].position.x = 0.07f;
   t_tilt_polls(3, 0.0f, false);
   CHECK(hold->hinged && t_longs[2] == l + 1,
         "carried along again: hinged %d, %u long buzzes",
         (int)hold->hinged, t_longs[2] - l);
   t_take_end();
}

/* Screen 2 carried in its hinge under screen 1: flat at Hinge Gap 0
 * and folded 30 at Gap 2, each pushed 2.5 cm past touching; flat at
 * Gap 0 pushed through to 4 cm; taken up where it rests touching flat
 * at Gap 1; swung square at the corner and pushed 3 mm on.
 * The left hand takes screen 1:
 * screen 2 stays where it shows, to the snap's ties, that poll and the
 * next, then carried on 1 cm it moves as the hand. */
static void test_edit_hinge_take_hop(void)
{
   unsigned i, k;
   for (i = 0; i < 5; i++)
   {
      video_screen_xr_t was;
      double hop  = 0.0;
      double turn = 0.0;
      t_gap  = (i == 1) ? VIDEO_OPENXR_EDIT_HINGE_GAP_2
         : (i == 3) ? VIDEO_OPENXR_EDIT_HINGE_GAP_1
         : VIDEO_OPENXR_EDIT_HINGE_GAP_0;
      t_snap = true;
      if (i < 4)
         t_edit_start();
      if (i == 4)
      {
         t_take_square(true, false);
         t_turn_polls(16, -1.0f, 10000, false);
         t_turn_polls(1, 0.0f, 10000, false);
         t_aim[2].position.x -= 0.003f;
         t_turn_polls(2, 0.0f, 10000, false);
      }
      else if (i == 3)
      {
         t_rest_under(0.0);
         t_tilt_polls(2, 0.0f, false);
      }
      else
      {
         double y;
         t_snap_under((i == 1) ? 30.0 : 0.0);
         y = t_aim[2].position.y;
         t_tilt_polls(1, 0.0f, false);
         t_aim[2].position.x = 0.07f;
         t_tilt_polls(1, 0.0f, false);
         t_aim[2].position.y = (float)(y + ((i == 2) ? 0.04 : 0.025));
         t_tilt_polls(2, 0.0f, false);
      }
      CHECK(input_openxr_edit.hand[1].hinged == (i != 2),
            "case %u, carried: hinged %d", i,
            (int)input_openxr_edit.hand[1].hinged);
      was = t_xr[1];
      for (k = 0; k < 2; k++)
      {
         double d[6];
         unsigned j;
         t_let_go();
         t_hold("edit/grab", 1, 1.0f, 0.0f);
         t_hold("edit/grab", 2, 1.0f, 0.0f);
         t_clock += 10000;
         t_edit_poll();
         d[0] = t_xr[1].x - was.x;
         d[1] = t_xr[1].y - was.y;
         d[2] = t_xr[1].z - was.z;
         d[3] = t_xr[1].yaw - was.yaw;
         d[4] = t_xr[1].pitch - was.pitch;
         d[5] = t_xr[1].roll - was.roll;
         for (j = 0; j < 6; j++)
         {
            double *w = (j < 3) ? &hop : &turn;
            if (fabs(d[j]) > *w)
               *w = fabs(d[j]);
         }
      }
      CHECK(     input_openxr_edit.hand[0].g.state == VIDEO_XR_GRAB_HELD
            && input_openxr_edit.hand[0].g.slot == 0
            && !input_openxr_edit.hand[1].hinged
            && hop <= 1e-5 && turn <= 1e-3,
            "case %u, screen 1 taken: slot %u, hinged %d, hopped %.3f mm, "
            "turned %.4f deg", i, input_openxr_edit.hand[0].g.slot,
            (int)input_openxr_edit.hand[1].hinged, hop * 1e3, turn);
      was = t_xr[1];
      t_aim[2].position.x += 0.01f;
      t_let_go();
      t_hold("edit/grab", 1, 1.0f, 0.0f);
      t_hold("edit/grab", 2, 1.0f, 0.0f);
      t_clock += 10000;
      t_edit_poll();
      CHECK(     fabs(t_xr[1].x - was.x - 0.01) < 1e-5
            && fabs(t_xr[1].y - was.y) < 1e-5
            && fabs(t_xr[1].z - was.z) < 1e-5,
            "case %u, carried on 1 cm: moved %.5f, %.5f, %.5f", i,
            t_xr[1].x - was.x, t_xr[1].y - was.y, t_xr[1].z - was.z);
      t_take_end();
   }
   t_gap = VIDEO_OPENXR_EDIT_HINGE_GAP_0;
}

/* Carried back into screen 1's plane from 5 cm in front at 1 cm/s, the
 * hand shaking 3 mm either way: one long buzz as it hinges, however
 * often the edge wobbles across the 3 cm where the hinge takes hold. */
static void test_edit_hinge_buzz_depth(void)
{
   static const double hz[3] = { 4.3, 7.1, 10.9 };
   unsigned i, j, l;
   float z;
   t_take_under(0.0, false);
   t_aim[2].position.z += 0.05f;
   t_tilt_polls(1, 0.0f, false);
   CHECK(!input_openxr_edit.hand[1].hinged, "5 cm off: hinged");
   l = t_longs[2];
   z = t_aim[2].position.z;
   for (i = 0; i < 500; i++)
   {
      double shake = 0.0;
      for (j = 0; j < 3; j++)
         shake += sin(2.0 * M_PI * hz[j] * i * 0.01 + 5.1 * (j + 1));
      t_aim[2].position.z = (float)(z - 0.0001 * i + 0.003 * shake / 1.7);
      t_tilt_polls(1, 0.0f, false);
   }
   t_aim[2].position.z = z - 0.05f;
   t_tilt_polls(1, 0.0f, false);
   CHECK(input_openxr_edit.hand[1].hinged && t_longs[2] == l + 1,
         "carried in: hinged %d, %u long buzzes",
         (int)input_openxr_edit.hand[1].hinged, t_longs[2] - l);
   t_take_end();
}

/* A hinge is a new attach unless the hold's mark is that hinge, its
 * buzz spent: the same target, its edge and the held screen's. */
static void test_edit_hinge_attach(void)
{
   input_openxr_hold_t hold;
   video_xr_hinge_t k;
   memset(&hold, 0, sizeof(hold));
   memset(&k, 0, sizeof(k));
   k.slot = 0;
   k.on   = 0;
   k.edge = 0;
   CHECK(input_openxr_edit_attach(&hold, &k), "no mark: no attach");
   hold.mark   = k;
   hold.buzzed = true;
   CHECK(!input_openxr_edit_attach(&hold, &k), "its own mark: an attach");
   k.slot = 2;
   CHECK(input_openxr_edit_attach(&hold, &k), "another screen: no attach");
   k.slot = 0;
   k.on   = 2;
   CHECK(input_openxr_edit_attach(&hold, &k), "another edge: no attach");
   k.on   = 0;
   k.edge = 3;
   CHECK(input_openxr_edit_attach(&hold, &k),
         "another held edge: no attach");
}

/* Edit Smoothing: the frame loop steps each hand's aim once a frame,
 * starting from the raw aim, whichever hook asks first, and publishes
 * it; losing the hand, a recenter or a jump start it afresh. The main
 * thread grabs and lets go from the published aim, so a screen is saved
 * where the frame loop last showed it. Off: the raw aim, nothing
 * published. */
static void test_edit_smoothing(void)
{
   unsigned i, n;
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
   /* The hand moves on and holds there 90 ms: what the frame loop
    * shows is what is let go. */
   t_aim[2].position.x = 0.05f;
   t_time += 11111111;
   t_hooks.aim(t_hooks.user, 1, local, t_time, &a);
   for (i = 0; i < 9; i++)
   {
      t_clock += 10000;
      t_edit_poll();
   }
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

/* Let go with the hand still moving: the release poll's own aim is not
 * used, so with no time passing it stays where the poll before put it;
 * the stick moves nothing then. Untracked at the release, it stays
 * where the last poll put it. */
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
         && t_near(t_grabs.let_go[0].last.position.x, 0.3f)
         && t_near(t_grabs.let_go[0].last.position.z, -1.8f)
         && t_near(t_grabs.let_go[0].width, 1.6f)
         && t_near(t_xr[0].x, 0.3f) && t_near(t_xr[0].z, 0.0f)
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

/* a and b the same place to 0.1 mm, and turn to 1e-4 of a quaternion. */
static bool t_pose_close(const video_xr_pose_t *a, const video_xr_pose_t *b)
{
   const video_xr_quat_t *p = &a->orientation;
   const video_xr_quat_t *q = &b->orientation;
   double s = ((double)p->x * q->x + (double)p->y * q->y
         + (double)p->z * q->z + (double)p->w * q->w) < 0.0 ? -1.0 : 1.0;
   return fabs((double)a->position.x - b->position.x) < 1e-4
      && fabs((double)a->position.y - b->position.y) < 1e-4
      && fabs((double)a->position.z - b->position.z) < 1e-4
      && fabs(p->x - s * q->x) < 1e-4 && fabs(p->y - s * q->y) < 1e-4
      && fabs(p->z - s * q->z) < 1e-4 && fabs(p->w - s * q->w) < 1e-4;
}

/* Hinged exactly at Hinge Gap 2 under screen 1, flat and folded 30
 * degrees, and held still: the grip opening turns the aim in pitch or
 * yaw, or, which the hinge holds anyway, in roll or across. Let go, it
 * rests hinged at the gap, saved where it is let go. */
static void test_edit_release_twitch(void)
{
   /* Pitch, yaw and roll in degrees, and metres across. */
   static const double twitch[][4] = {
      {  0.4, 0.0, 0.0, 0.0 }, { -0.4, 0.0, 0.0, 0.0 },
      {  0.6, 0.0, 0.0, 0.0 }, { -0.6, 0.0, 0.0, 0.0 },
      { 0.0,  0.4, 0.0, 0.0 }, { 0.0, -0.4, 0.0, 0.0 },
      { 0.0,  1.0, 0.0, 0.0 }, { 0.0, -1.0, 0.0, 0.0 },
      { 0.0,  2.0, 0.0, 0.0 }, { 0.0, -2.0, 0.0, 0.0 },
      { 0.0, 0.0,  2.0, 0.0 }, { 0.0, 0.0, -2.0, 0.0 },
      { 0.0, 0.0, 0.0,  0.01 }, { 0.0, 0.0, 0.0, -0.01 } };
   unsigned i, j;
   const video_xr_pose_t *at = &t_grabs.let_go[1].last;
   for (j = 0; j < 2; j++)
      for (i = 0; i < sizeof(twitch) / sizeof(twitch[0]); i++)
      {
         bool hinged;
         double gap, plane;
         video_xr_pose_t saved;
         t_hinged_at(VIDEO_OPENXR_EDIT_HINGE_GAP_2, j * 30.0, 0.02);
         t_tilt_polls(9, 0.0f, false);
         hinged = input_openxr_edit.hand[1].hinged;
         t_turn_aim(twitch[i][0], twitch[i][1], twitch[i][2]);
         t_aim[2].position.y += (float)twitch[i][3];
         t_let_go();
         t_clock += 10000;
         t_edit_poll();
         gap = t_under_gap(&plane);
         video_xr_item_pose(&t_quads.slots[1].base, &t_xr[1], &saved);
         CHECK(     hinged && t_grabs.num_rests == 1
               && fabs(gap - 0.02) < 1e-5 && t_pose_close(&saved, at),
               "twitch, fold %u, pitch %+.1f yaw %+.1f roll %+.1f across "
               "%+.0f mm: hinged %d, %u at rest, gap %.5f, saved at "
               "%.4f,%.4f,%.4f, let go at %.4f,%.4f,%.4f", j * 30,
               twitch[i][0], twitch[i][1], twitch[i][2],
               twitch[i][3] * 1000.0, (int)hinged, t_grabs.num_rests, gap,
               saved.position.x, saved.position.y, saved.position.z,
               at->position.x, at->position.y, at->position.z);
         t_take_end();
      }
   t_gap = VIDEO_OPENXR_EDIT_HINGE_GAP_0;
}

/* Screen 1 taken at x 0 and carried n polls us apart, the hand dx
 * further along x each, the last lost of them untracked, then let go
 * with the hand still moving: where it is let go, and in *hand where
 * the hand was then. */
static double t_rewind(unsigned n, retro_time_t us, double dx,
      unsigned lost, double *hand)
{
   unsigned i;
   t_edit_start();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_clock += us;
   t_edit_poll();
   for (i = 1; i <= n + 1; i++)
   {
      if (i > n)
         t_let_go();
      if (i + lost > n)
         t_aim_tracked[2] = false;
      t_aim[2].position.x = (float)(dx * i);
      t_clock += us;
      t_edit_poll();
   }
   *hand = t_aim[2].position.x;
   return t_grabs.let_go[0].last.position.x;
}

/* A grip let go puts its item where it showed the newest trail entry
 * at least INPUT_OPENXR_EDIT_REWIND_US back, entries at least
 * INPUT_OPENXR_EDIT_TRAIL_US apart, untracked polls adding none: past
 * the hand's twitch as the grip opens. A hold shorter than that lets go
 * where it was taken. The size is the rewound entry's too. */
static void test_edit_release_rewind(void)
{
   unsigned i;
   double x, hand, gap, plane, edge;
   float width, scale, grown;
   video_xr_pose_t saved;
   x = t_rewind(30, 10000, 0.001, 0, &hand);
   CHECK(     t_grabs.let_go[0].state == VIDEO_XR_GRAB_RELEASED
         && fabs(hand - x - 0.008) < 1e-5 && fabs(t_xr[0].x - x) < 1e-5,
         "rewind at 10 ms polls: let go at %.4f, saved %.4f, the hand at "
         "%.4f, want 8 mm short", x, t_xr[0].x, hand);
   t_edit_end();
   x = t_rewind(300, 1000, 0.0001, 0, &hand);
   CHECK(     t_grabs.let_go[0].state == VIDEO_XR_GRAB_RELEASED
         && hand - x > 0.008 - 1e-5 && hand - x < 0.0089 + 1e-5
         && fabs(t_xr[0].x - x) < 1e-5,
         "rewind at 1 ms polls: let go at %.5f, saved %.5f, the hand at "
         "%.5f, want 8.0 to 8.9 mm short", x, t_xr[0].x, hand);
   t_edit_end();
   x = t_rewind(5, 10000, 0.001, 0, &hand);
   CHECK(fabs(x) < 1e-5 && fabs(t_xr[0].x) < 1e-5,
         "held 50 ms: let go at %.4f, saved %.4f, want where it was taken",
         x, t_xr[0].x);
   t_edit_end();
   x = t_rewind(30, 10000, 0.001, 3, &hand);
   CHECK(fabs(x - 0.023) < 1e-5 && fabs(t_xr[0].x - x) < 1e-5,
         "untracked for 3 polls: let go at %.4f, saved %.4f, want 0.023",
         x, t_xr[0].x);
   t_edit_end();

   /* Grown on the stick for the last 50 ms: undone. */
   t_edit_start();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   for (i = 0; i < 10; i++)
   {
      t_clock += 10000;
      t_edit_poll();
   }
   width = input_openxr_edit.hand[1].g.width;
   scale = t_xr[0].scale;
   t_hold("edit/stick", 2, 1.0f, 0.0f);
   for (i = 0; i < 5; i++)
   {
      t_clock += 10000;
      t_edit_poll();
   }
   grown = input_openxr_edit.hand[1].g.width;
   t_let_go();
   t_clock += 10000;
   t_edit_poll();
   CHECK(     grown > 1.65f && t_near(t_grabs.let_go[0].width, width)
         && t_near(t_xr[0].scale, scale)
         && t_near(t_grabs.let_go[0].last.position.x, 0.0f),
         "grown to %.3f as let go: %.3f wide, scale %.3f, at %.3f, want "
         "%.3f, %.3f", grown, t_grabs.let_go[0].width, t_xr[0].scale,
         t_grabs.let_go[0].last.position.x, width, scale);
   t_edit_end();

   /* Hinged at Hinge Gap 2, then grown in the grip's last 80 ms: let go
    * as it showed before, its edge at the gap. */
   t_hinged_at(VIDEO_OPENXR_EDIT_HINGE_GAP_2, 0.0, 0.02);
   t_tilt_polls(9, 0.0f, false);
   width = input_openxr_edit.hand[1].g.width;
   scale = t_xr[1].scale;
   t_hold("edit/stick", 2, 1.0f, 0.0f);
   for (i = 0; i < 7; i++)
   {
      t_clock += 10000;
      t_edit_poll();
   }
   grown = input_openxr_edit.hand[1].g.width;
   t_let_go();
   t_clock += 10000;
   t_edit_poll();
   gap  = t_under_gap(&plane);
   /* t_under_gap's edge is 0.3 over the middle: flat, the let-go's own
    * is half its height. */
   edge = gap + 0.3 - 0.5 * t_grabs.let_go[1].height;
   video_xr_item_pose(&t_quads.slots[1].base, &t_xr[1], &saved);
   CHECK(     grown > width * 1.04f
         && t_near(t_grabs.let_go[1].width, width)
         && t_near(t_xr[1].scale, scale) && fabs(gap - 0.02) < 1e-5
         && fabs(edge - 0.02) < 1e-5 && t_grabs.num_rests == 1
         && t_pose_close(&saved, &t_grabs.let_go[1].last),
         "hinged, grown to %.3f as let go: %.3f wide, scale %.3f, want "
         "%.3f, %.3f; gap %.5f, its edge %.5f, %u at rest, saved at "
         "%.4f,%.4f,%.4f, let go at %.4f,%.4f,%.4f", grown,
         t_grabs.let_go[1].width, t_xr[1].scale, width, scale, gap, edge,
         t_grabs.num_rests,
         saved.position.x, saved.position.y, saved.position.z,
         t_grabs.let_go[1].last.position.x,
         t_grabs.let_go[1].last.position.y,
         t_grabs.let_go[1].last.position.z);
   t_take_end();
   t_gap = VIDEO_OPENXR_EDIT_HINGE_GAP_0;
}

/* Frames and polls 10 ms apart, then none for 85 or 340 ms while the
 * hand moves 5 cm on, then again: let go 20 ms after, the rewind stops
 * at the stall, never stepping back across it. */
static void test_edit_release_stall(void)
{
   static const retro_time_t stall[] = { 85000, 340000 };
   unsigned i, j;
   for (j = 0; j < sizeof(stall) / sizeof(stall[0]); j++)
   {
      double x;
      t_edit_start();
      t_hold("edit/grab", 2, 1.0f, 0.0f);
      t_clock += 10000;
      t_edit_poll();
      for (i = 1; i <= 20; i++)
      {
         t_aim[2].position.x = (float)(0.001 * i);
         t_clock += 10000;
         t_edit_poll();
      }
      t_aim[2].position.x = 0.07f;
      t_clock += stall[j];
      t_edit_poll();
      t_aim[2].position.x = 0.071f;
      t_clock += 10000;
      t_edit_poll();
      t_let_go();
      t_aim[2].position.x = 0.072f;
      t_clock += 10000;
      t_edit_poll();
      x = t_grabs.let_go[0].last.position.x;
      CHECK(     t_grabs.let_go[0].state == VIDEO_XR_GRAB_RELEASED
            && fabs(x - 0.07) < 1e-5 && fabs(t_xr[0].x - x) < 1e-5,
            "stalled %u ms: let go at %.4f, saved %.4f, want 0.070, not "
            "before the stall (0.020)", (unsigned)(stall[j] / 1000), x,
            t_xr[0].x);
      t_edit_end();
   }
}

/* 120 Hz frames seen by 60 Hz polls, one a frame on and the next three,
 * the hand twitching as the grip opens: no stall, so the let-go looks
 * the whole 80 ms back, to where it showed before the twitch. */
static void test_edit_release_fast_frames(void)
{
   unsigned i;
   double x;
   t_period = 8333333;
   t_edit_start();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_clock += 16667;
   t_edit_poll();
   for (i = 1; i <= 20; i++)
   {
      t_aim[2].position.x = (float)(i <= 18 ? 0.001 * i
            : (i == 19 ? 0.03 : 0.04));
      t_clock += i == 18 ? 8333 : (i == 19 ? 25000 : 16667);
      t_edit_poll();
   }
   t_let_go();
   t_aim[2].position.x = 0.05f;
   t_clock += 16667;
   t_edit_poll();
   x = t_grabs.let_go[0].last.position.x;
   CHECK(     t_grabs.let_go[0].state == VIDEO_XR_GRAB_RELEASED
         && fabs(x - 0.016) < 1e-5 && fabs(t_xr[0].x - x) < 1e-5,
         "120 Hz frames, 60 Hz polls: let go at %.4f, saved %.4f, want "
         "0.016, before the twitch (0.030 on)", x, t_xr[0].x);
   t_edit_end();
   t_period = 11111111;
}

/* Edit Smoothing on: each frame's aims add one trail entry, dated by
 * the frame, however many polls see them. */
static void test_edit_trail_frames(void)
{
   unsigned i, j;
   video_xr_pose_t a;
   const input_openxr_hold_t *hold = &input_openxr_edit.hand[1];
   bool ok                         = true;
   t_smooth = VIDEO_OPENXR_EDIT_SMOOTHING_MEDIUM;
   t_edit_start();
   t_clock += 10000;
   t_frame_aim(&a);
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   t_clock += 8000;
   for (i = 1; i <= 6 && ok; i++)
   {
      XrTime frame;
      t_aim[2].position.x = (float)(0.002 * i);
      t_clock += 3111;
      t_frame_aim(&a);
      frame = openxr_frame_predicted_time(&t_xr_frame);
      for (j = 0; j < 4; j++)
      {
         t_clock += 2000;
         t_edit_poll();
      }
      ok = hold->g.state == VIDEO_XR_GRAB_HELD && hold->trails == i + 1
         && hold->trail[hold->trail_at].t == frame;
      CHECK(ok, "frame %u seen by 4 polls: %u entries, the newest dated "
            "%.0f, want %u, %.0f", i, hold->trails,
            (double)hold->trail[hold->trail_at].t, i + 1, (double)frame);
   }
   t_let_go();
   t_edit_poll();
   t_edit_end();
   t_smooth = VIDEO_OPENXR_EDIT_SMOOTHING_OFF;
}

/* Edit Smoothing on: a hand's trail is dated by the frame its own aim
 * is from, not by a newer one the other hand's was stepped for first. */
static void test_edit_trail_hands(void)
{
   bool ok;
   XrTime first, next;
   video_xr_pose_t a;
   const input_openxr_hold_t *hold = &input_openxr_edit.hand[1];
   t_smooth = VIDEO_OPENXR_EDIT_SMOOTHING_MEDIUM;
   t_edit_start();
   t_clock += 10000;
   t_frame_aim(&a);
   first = openxr_frame_predicted_time(&t_xr_frame);
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   t_clock += 10000;
   next = openxr_frame_predicted_time(&t_xr_frame);
   t_hooks.aim(t_hooks.user, 0, t_xr_session.local_space, next, &a);
   t_edit_poll();
   ok = hold->trails == 1 && hold->trail[hold->trail_at].t == first;
   CHECK(ok, "the left hand's aim stepped first: %u entries, the newest "
         "dated %.0f, want 1, %.0f", hold->trails,
         (double)hold->trail[hold->trail_at].t, (double)first);
   t_frame_aim(&a);
   t_edit_poll();
   CHECK(hold->trails == 2 && hold->trail[hold->trail_at].t == next,
         "then the right's: %u entries, the newest dated %.0f, want 2, "
         "%.0f", hold->trails, (double)hold->trail[hold->trail_at].t,
         (double)next);
   t_let_go();
   t_edit_poll();
   t_edit_end();
   t_smooth = VIDEO_OPENXR_EDIT_SMOOTHING_OFF;
}

/* Edit Smoothing on: carried at 0.3 m/s, stopped 100 ms before the grip
 * opens and grown on the stick meanwhile, screen 1 is let go where it
 * shows as the grip opens, that large: smoothing has damped the
 * twitch, and a rewind would step back by what it still lagged. */
static void test_edit_release_smoothed(void)
{
   unsigned i;
   video_xr_pose_t a, want;
   video_xr_grab_t held;
   video_screen_xr_t s;
   float scale;
   const video_xr_grab_t *g = &input_openxr_edit.hand[1].g;
   t_smooth = VIDEO_OPENXR_EDIT_SMOOTHING_MEDIUM;
   t_edit_start();
   t_clock += 10000;
   t_frame_aim(&a);
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_edit_poll();
   for (i = 1; i <= 30; i++)
   {
      if (i <= 20)
         t_aim[2].position.x = (float)(0.003 * i);
      if (i == 26)
         t_hold("edit/stick", 2, 1.0f, 0.0f);
      t_clock += 10000;
      t_frame_aim(&a);
      t_edit_poll();
   }
   t_let_go();
   t_clock += 10000;
   t_frame_aim(&a);
   held  = *g;
   scale = input_openxr_edit.hand[1].scale;
   video_xr_grab_pose(&held, &a, NULL, 0, NULL, &want, &s, NULL);
   t_edit_poll();
   CHECK(     t_grabs.let_go[0].state == VIDEO_XR_GRAB_RELEASED
         && held.width > 1.62f
         && t_pose_close(&t_grabs.let_go[0].last, &want)
         && fabs(t_xr[0].x - s.x) < 1e-4
         && fabs(t_grabs.let_go[0].width - held.width) < 1e-5
         && fabs(t_xr[0].scale - scale) < 1e-5,
         "smoothed let-go at %.4f, %.3f wide, saved x %.4f scale %.4f; "
         "want where it shows, %.4f, %.3f wide, x %.4f scale %.4f",
         t_grabs.let_go[0].last.position.x, t_grabs.let_go[0].width,
         t_xr[0].x, t_xr[0].scale, want.position.x, held.width, s.x,
         scale);
   t_edit_end();
   t_smooth = VIDEO_OPENXR_EDIT_SMOOTHING_OFF;
}

/* As test_edit_release_twitch with Edit Smoothing on, the twitch over
 * the grip's last four frames, as a hand opening one does: the hinge
 * and the settle still look back past it. */
static void test_edit_release_smoothed_twitch(void)
{
   /* Pitch and yaw in degrees. */
   static const double twitch[][2] = {
      { 0.6, 0.0 }, { -0.6, 0.0 }, { 0.0, 2.0 }, { 0.0, -2.0 } };
   unsigned i, k;
   const video_xr_pose_t *at = &t_grabs.let_go[1].last;
   t_smooth = VIDEO_OPENXR_EDIT_SMOOTHING_MEDIUM;
   for (i = 0; i < sizeof(twitch) / sizeof(twitch[0]); i++)
   {
      bool hinged;
      double gap, plane, along, x;
      video_xr_pose_t a, saved;
      t_hinged_at(VIDEO_OPENXR_EDIT_HINGE_GAP_2, 0.0, 0.02);
      t_tilt_polls(40, 0.0f, false);
      hinged = input_openxr_edit.hand[1].hinged;
      x      = t_edge_along(true);
      for (k = 0; k < 3; k++)
      {
         t_turn_aim(twitch[i][0] / 4.0, twitch[i][1] / 4.0, 0.0);
         t_tilt_polls(1, 0.0f, false);
      }
      t_turn_aim(twitch[i][0] / 4.0, twitch[i][1] / 4.0, 0.0);
      t_let_go();
      t_clock += 10000;
      t_frame_aim(&a);
      t_edit_poll();
      gap   = t_under_gap(&plane);
      along = t_edge_along(true);
      video_xr_item_pose(&t_quads.slots[1].base, &t_xr[1], &saved);
      CHECK(     hinged && t_grabs.num_rests == 1
            && fabs(gap - 0.02) < 1e-5 && fabs(along - x) < 1e-3
            && t_pose_close(&saved, at),
            "smoothed twitch, pitch %+.1f yaw %+.1f: hinged %d, %u at "
            "rest, gap %.5f, along %.5f from %.5f, saved at "
            "%.4f,%.4f,%.4f, let go at %.4f,%.4f,%.4f", twitch[i][0],
            twitch[i][1], (int)hinged, t_grabs.num_rests, gap, along, x,
            saved.position.x, saved.position.y, saved.position.z,
            at->position.x, at->position.y, at->position.z);
      t_take_end();
   }
   t_smooth = VIDEO_OPENXR_EDIT_SMOOTHING_OFF;
   t_gap    = VIDEO_OPENXR_EDIT_HINGE_GAP_0;
}

/* n polls 10 ms apart, the right hand gripping, and the left on the
 * menu's handle too if menu. */
static void t_menu_polls(unsigned n, bool menu)
{
   unsigned i;
   t_let_go();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   if (menu)
      t_hold("edit/grab", 1, 1.0f, 0.0f);
   for (i = 0; i < n; i++)
   {
      t_clock += 10000;
      t_edit_poll();
   }
}

/* Where the right hand puts screen 2 with no snap: its top edge's
 * middle under screen 1's bottom edge, and in *along its x. */
static double t_hand_gap(double *along)
{
   video_xr_pose_t pose;
   video_xr_vec3_t m, w;
   const input_openxr_hold_t *hold = &input_openxr_edit.hand[1];
   video_xr_grab_pose(&hold->g, &hold->aim, NULL, 0, NULL, &pose, NULL,
         NULL);
   m.x = 0.0f;
   m.y = 0.3f;
   m.z = 0.0f;
   video_xr_rotate(&pose.orientation, &m, &w);
   *along = (double)pose.position.x + w.x;
   return -0.48 - ((double)pose.position.y + w.y);
}

/* The right hand, gripping screen 2, carried in 1 mm steps until it
 * puts it apart metres under screen 1 (negative: past touching), its
 * middle at x along, as t_hand_gap measures it. */
static void t_carry_under(double apart, double along, bool menu)
{
   unsigned k, n;
   double x;
   double dy = t_hand_gap(&x) - apart;
   double dx = along - x;
   double y0 = t_aim[2].position.y;
   double x0 = t_aim[2].position.x;
   n = (unsigned)ceil((fabs(dy) > fabs(dx) ? fabs(dy) : fabs(dx)) / 0.001);
   for (k = 1; k <= n; k++)
   {
      t_aim[2].position.y = (float)(y0 + dy * k / n);
      t_aim[2].position.x = (float)(x0 + dx * k / n);
      t_menu_polls(1, menu);
   }
}

/* Screen 2 hinged under screen 1, flat and folded 30, carried to where
 * a row's hand puts it, held still and let go. Within
 * VIDEO_XR_HINGE_SETTLE of Hinge Gap 2 it settles at the gap, from the
 * stop's hold at Gap 0 touching, and 4 cm off level, level; listed at
 * rest and saved there, and taken again it is hinged at once with no
 * long buzz; so too pushed through, let go inside its mark's 150 ms.
 * Elsewhere it rests where it shows: listed at a free gap, not past 10
 * cm or with Snap to Screens off. The left hand holding the menu
 * changes nothing. */
static void test_edit_settle(void)
{
   /* The Hinge Gap, Snap to Screens, the left hand on the menu; where
    * the hand puts it, metres apart and along; the gap let go (-1: as
    * it showed); level; how many listed. Taken 7 cm short of where it
    * is let go, so its snaps are whole. */
   static const struct
   {
      unsigned gap;
      bool snap, menu;
      double apart, along, want;
      bool level;
      unsigned rests;
   } rows[] = {
      { VIDEO_OPENXR_EDIT_HINGE_GAP_2, true, false, 0.049, 0.07, 0.02,
         false, 1 },
      { VIDEO_OPENXR_EDIT_HINGE_GAP_2, true, false, 0.035, 0.07, 0.02,
         false, 1 },
      { VIDEO_OPENXR_EDIT_HINGE_GAP_2, true, false, 0.003, 0.07, 0.02,
         false, 1 },
      { VIDEO_OPENXR_EDIT_HINGE_GAP_2, true, false, 0.055, 0.07, 0.055,
         false, 1 },
      { VIDEO_OPENXR_EDIT_HINGE_GAP_0, true, false, 0.025, 0.07, 0.0,
         false, 1 },
      { VIDEO_OPENXR_EDIT_HINGE_GAP_0, true, false, -0.02, 0.07, 0.0,
         false, 1 },
      { VIDEO_OPENXR_EDIT_HINGE_GAP_2, true, false, 0.02, 0.04, 0.02,
         true, 1 },
      { VIDEO_OPENXR_EDIT_HINGE_GAP_2, true, false, 0.109, 0.07, -1.0,
         false, 0 },
      { VIDEO_OPENXR_EDIT_HINGE_GAP_2, true, false, -0.035, 0.07, 0.0,
         false, 1 },
      { VIDEO_OPENXR_EDIT_HINGE_GAP_2, false, false, 0.049, 0.07, -1.0,
         false, 0 },
      { VIDEO_OPENXR_EDIT_HINGE_GAP_2, true, true, 0.049, 0.07, 0.02,
         false, 1 } };
   const input_openxr_hold_t *hold = &input_openxr_edit.hand[1];
   unsigned i, j, k;
   for (j = 0; j < 2; j++)
      for (i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
      {
         bool through, menu, at_once;
         unsigned l;
         double gap, along, plane, before, x;
         video_xr_pose_t saved;
         if (rows[i].snap)
            t_hinged_along(rows[i].gap, j * 30.0,
                  rows[i].gap == VIDEO_OPENXR_EDIT_HINGE_GAP_2 ? 0.02 : 0.0,
                  rows[i].along - 0.07);
         else
         {
            t_gap  = rows[i].gap;
            t_snap = false;
            t_edit_start();
            t_snap_under(j * 30.0);
            t_gap_shift(0.02);
            t_tilt_polls(1, 0.0f, false);
            t_aim[2].position.x = 0.07f;
            t_tilt_polls(1, 0.0f, false);
         }
         if (rows[i].menu)
         {
            t_aim[1].position.x = 2.0f;
            t_aim[1].position.y = t_handle_y(0.6f);
         }
         t_carry_under(rows[i].apart, rows[i].along, rows[i].menu);
         t_menu_polls(9, rows[i].menu);
         through = hold->g.through != 0;
         menu    = input_openxr_edit.hand[0].g.state == VIDEO_XR_GRAB_HELD
            && input_openxr_edit.hand[0].g.slot == VIDEO_XR_MENU_SLOT;
         before  = t_under_gap(&plane);
         x       = t_edge_along(true);
         t_let_go();
         if (rows[i].menu)
            t_hold("edit/grab", 1, 1.0f, 0.0f);
         t_clock += 10000;
         t_edit_poll();
         gap   = t_under_gap(&plane);
         along = t_edge_along(true);
         video_xr_item_pose(&t_quads.slots[1].base, &t_xr[1], &saved);
         CHECK(     t_grabs.let_go[1].state == VIDEO_XR_GRAB_RELEASED
               && menu == rows[i].menu
               && (rows[i].apart >= 0.0 || rows[i].want >= 0.0
                  || (through && before < 0.0))
               && fabs(gap - (rows[i].want < 0.0 ? before : rows[i].want))
                  < 1e-5
               && fabs(along - (rows[i].level ? 0.0 : x)) < 1e-5
               && t_grabs.num_rests == rows[i].rests
               && t_pose_close(&saved, &t_grabs.let_go[1].last),
               "settle row %u, fold %u: menu held %d, through %d, gap "
               "%.5f from %.5f, along %.5f from %.5f, %u at rest, saved "
               "at %.4f,%.4f,%.4f, let go at %.4f,%.4f,%.4f", i, j * 30,
               (int)menu, (int)through, gap, before, along, x,
               t_grabs.num_rests, saved.position.x, saved.position.y,
               saved.position.z, t_grabs.let_go[1].last.position.x,
               t_grabs.let_go[1].last.position.y,
               t_grabs.let_go[1].last.position.z);
         /* Shown where it was saved, as the driver publishes it. */
         for (k = 0; k < t_quads.num_quads; k++)
            if (t_quads.quads[k].slot == 1)
               video_xr_item_pose(&t_quads.slots[1].base, &t_xr[1],
                     &t_quads.quads[k].pose);
         l = t_longs[2];
         t_menu_polls(2, rows[i].menu);
         at_once = hold->hinged;
         t_menu_polls(1, rows[i].menu);
         CHECK(     (hold->g.rest_slot != 0) == (rows[i].rests != 0)
               && (!rows[i].rests
                  || (at_once && hold->hinged && t_longs[2] == l)),
               "settle row %u, fold %u, taken again: kept %u, hinged %d "
               "then %d, %u long buzzes", i, j * 30,
               (unsigned)hold->g.rest_slot, (int)at_once,
               (int)hold->hinged, t_longs[2] - l);
         t_take_end();
      }
   t_gap = VIDEO_OPENXR_EDIT_HINGE_GAP_0;
}

/* Screen 2, the left hand's, carried 4.9 cm under screen 1 at Hinge
 * Gap 2 and showing its mark, as the right hand takes screen 1 and edit
 * mode ends before the left's next poll: let go where it shows, never
 * settled on a held screen. */
static void test_edit_settle_target_held(void)
{
   unsigned k;
   bool marked, taken;
   double y, before, gap, plane;
   const input_openxr_hold_t *hold = &input_openxr_edit.hand[0];
   t_gap  = VIDEO_OPENXR_EDIT_HINGE_GAP_2;
   t_snap = true;
   t_edit_start();
   t_snap_under(0.0);
   t_gap_shift(0.02);
   y                   = t_aim[2].position.y;
   t_aim[1].position.y = (float)y;
   t_aim[2].position.y = 0.2f;
   t_hold("edit/grab", 1, 1.0f, 0.0f);
   /* 7 cm along, so its snaps are whole, then 2.9 cm under and still. */
   for (k = 1; k <= 108; k++)
   {
      if (k <= 70)
         t_aim[1].position.x = (float)(0.001 * k);
      else if (k <= 99)
         t_aim[1].position.y = (float)(y - 0.001 * (k - 70));
      t_clock += 10000;
      t_edit_poll();
   }
   marked = hold->buzzed;
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_clock += 10000;
   t_edit_poll();
   taken  = input_openxr_edit.hand[1].g.state == VIDEO_XR_GRAB_HELD
      && input_openxr_edit.hand[1].g.slot == 0 && hold->buzzed;
   before = t_under_gap(&plane);
   input_openxr_edit_toggle();
   gap    = t_under_gap(&plane);
   CHECK(marked && taken && before > 0.04 && fabs(gap - before) < 1e-5,
         "screen 1 taken as edit mode ends: marked %d, taken %d, gap "
         "%.5f from %.5f", (int)marked, (int)taken, gap, before);
   t_edit_end();
   t_snap = false;
   t_gap  = VIDEO_OPENXR_EDIT_HINGE_GAP_0;
}

/* As test_edit_release_twitch, from 4.9 cm apart at Hinge Gap 2 and
 * from 4 cm off level at the gap, the aim turned 0.6 degrees in pitch
 * or yaw as the grip opens: each settles at the gap, level from off
 * it, listed and saved there. */
static void test_edit_settle_twitch(void)
{
   static const double turn[][2] = { { 0.6, 0.0 }, { -0.6, 0.0 },
      { 0.0, 0.6 }, { 0.0, -0.6 } };
   /* The hand's metres apart and along. */
   static const double from[][2] = { { 0.049, 0.07 }, { 0.02, 0.04 } };
   unsigned i, j, k;
   for (k = 0; k < 2; k++)
      for (j = 0; j < 2; j++)
         for (i = 0; i < 4; i++)
         {
            double gap, along, plane, x;
            video_xr_pose_t saved;
            t_hinged_along(VIDEO_OPENXR_EDIT_HINGE_GAP_2, j * 30.0, 0.02,
                  from[k][1] - 0.07);
            t_carry_under(from[k][0], from[k][1], false);
            t_tilt_polls(9, 0.0f, false);
            x = t_edge_along(true);
            t_turn_aim(turn[i][0], turn[i][1], 0.0);
            t_let_go();
            t_clock += 10000;
            t_edit_poll();
            gap   = t_under_gap(&plane);
            along = t_edge_along(true);
            video_xr_item_pose(&t_quads.slots[1].base, &t_xr[1], &saved);
            CHECK(     t_grabs.num_rests == 1 && fabs(gap - 0.02) < 1e-5
                  && fabs(along - (k ? 0.0 : x)) < 1e-5
                  && t_pose_close(&saved, &t_grabs.let_go[1].last),
                  "settle twitch from %.3f apart at %.2f, fold %u, pitch "
                  "%+.1f yaw %+.1f: %u at rest, gap %.5f, along %.5f from "
                  "%.5f, saved at %.4f,%.4f,%.4f", from[k][0], from[k][1],
                  j * 30, turn[i][0], turn[i][1], t_grabs.num_rests, gap,
                  along, x, saved.position.x, saved.position.y,
                  saved.position.z);
            t_take_end();
         }
   t_gap = VIDEO_OPENXR_EDIT_HINGE_GAP_0;
}

/* Where t_shake shakes the right hand about. */
static XrPosef t_still;

/* t_still shaken as a hand on a device, for poll k of 10 ms: turned
 * deg degrees in pitch and yaw at 8 Hz and a fifth of that at 2.3 Hz,
 * the yaw a quarter period behind, and moved 0.3 mm each way. At 1.8 m
 * 0.5 degrees is 1.57 cm at the screen: pitch carries screen 2 across
 * its hinge under screen 1, yaw along it. */
static void t_shake(unsigned k, double deg)
{
   double t = k * 0.01;
   double a = 2.0 * M_PI * 8.0 * t;
   double b = 2.0 * M_PI * 2.3 * t + 1.0;
   double m = 0.0003 * sin(k * 1.3);
   t_aim[2] = t_still;
   t_turn_aim(deg * (sin(a) + 0.2 * sin(b)),
         deg * (sin(a - M_PI / 2.0) + 0.2 * sin(b - M_PI / 2.0)), 0.0);
   t_aim[2].position.x += (float)m;
   t_aim[2].position.y += (float)m;
   t_aim[2].position.z += (float)m;
}

/* The right hand, screen 2 hinged as t_hinged_at left it with the hand
 * at y and z putting it gap apart, now putting it d metres past
 * touching (out 0) or d in front of screen 1's plane (out 1). */
static void t_mark_out(unsigned out, double y, double z, double gap,
      double d)
{
   t_aim[2].position.y = (float)(out ? y : y + gap + d);
   t_aim[2].position.z = (float)(out ? z + d : z);
}

/* Screen 2 hinged under screen 1 flat at Hinge Gap 0, pushed 3.5 cm
 * past touching, out of the stop's hold, or carried 3.5 cm in front of
 * screen 1's plane: out 140 ms from the first poll seen out and back to
 * 2 cm, its hinge stays marked with no long buzz; 150 ms from it, it
 * lets go, and back it takes hold again with one. The other hand
 * taking screen 1 ends the mark at once, the hand tracked or not, and
 * a lid outliving it; so does screen 1 gone. Untracked polls neither
 * keep nor end it; out, a stall or a spell untracked between two
 * tracked polls counts as 50 ms. With Snap to Screens off it is never
 * marked. */
static void test_edit_mark_debounce(void)
{
   const input_openxr_hold_t *hold = &input_openxr_edit.hand[1];
   unsigned i, k, l;
   for (i = 0; i < 2; i++)
   {
      double x;
      double y   = 0.0;
      double z   = 0.0;
      double gap = 0.0;
      bool kept  = true;
      t_hinged_at(VIDEO_OPENXR_EDIT_HINGE_GAP_0, 0.0, 0.0);
      y   = t_aim[2].position.y;
      z   = t_aim[2].position.z;
      gap = t_hand_gap(&x);
      l   = t_longs[2];
      CHECK(hold->buzzed, "out %u: hinged %d, buzzed %d", i,
            (int)hold->hinged, (int)hold->buzzed);
      t_mark_out(i, y, z, gap, 0.035);
      for (k = 1; k <= 15; k++)
      {
         t_tilt_polls(1, 0.0f, false);
         kept = kept && hold->buzzed;
      }
      t_mark_out(i, y, z, gap, 0.02);
      t_tilt_polls(1, 0.0f, false);
      CHECK(kept && hold->buzzed && t_longs[2] == l,
            "out %u, 140 ms out and back: kept %d, buzzed %d, %u long "
            "buzzes", i, (int)kept, (int)hold->buzzed, t_longs[2] - l);
      t_mark_out(i, y, z, gap, 0.035);
      for (k = 1; k <= 16; k++)
      {
         t_tilt_polls(1, 0.0f, false);
         if (k == 15)
            kept = hold->buzzed;
      }
      CHECK(kept && !hold->buzzed,
            "out %u, 150 ms out: buzzed %d at 140 ms, %d at 150", i,
            (int)kept, (int)hold->buzzed);
      t_mark_out(i, y, z, gap, 0.02);
      t_tilt_polls(1, 0.0f, false);
      CHECK(hold->buzzed && t_longs[2] == l + 1,
            "out %u, back after 150 ms: buzzed %d, %u long buzzes", i,
            (int)hold->buzzed, t_longs[2] - l);
      t_take_end();
   }

   /* The other hand takes screen 1. */
   t_hinged_at(VIDEO_OPENXR_EDIT_HINGE_GAP_0, 0.0, 0.0);
   t_hold("edit/grab", 1, 1.0f, 0.0f);
   t_clock += 10000;
   t_edit_poll();
   CHECK(     input_openxr_edit.hand[0].g.state == VIDEO_XR_GRAB_HELD
         && input_openxr_edit.hand[0].g.slot == 0 && !hold->buzzed,
         "screen 1 taken: held %u, buzzed %d",
         input_openxr_edit.hand[0].g.slot, (int)hold->buzzed);
   t_take_end();

   /* The same with the right hand lost: its mark ends as screen 1 is
    * taken, and that hand letting go does not bring it back. */
   t_hinged_at(VIDEO_OPENXR_EDIT_HINGE_GAP_0, 0.0, 0.0);
   t_aim_tracked[2] = false;
   t_hold("edit/grab", 1, 1.0f, 0.0f);
   t_clock += 10000;
   t_edit_poll();
   CHECK(     input_openxr_edit.hand[0].g.state == VIDEO_XR_GRAB_HELD
         && input_openxr_edit.hand[0].g.slot == 0 && !hold->buzzed
         && !t_grabs.hand[1].mark_slot,
         "lost, screen 1 taken: held %u, buzzed %d, published %u",
         input_openxr_edit.hand[0].g.slot, (int)hold->buzzed,
         (unsigned)t_grabs.hand[1].mark_slot);
   t_let_go();
   t_hold("edit/grab", 2, 1.0f, 0.0f);
   t_clock += 10000;
   t_edit_poll();
   CHECK(     input_openxr_edit.hand[0].g.state != VIDEO_XR_GRAB_HELD
         && hold->g.state == VIDEO_XR_GRAB_HELD && !hold->buzzed
         && !t_grabs.hand[1].mark_slot,
         "lost, screen 1 let go: holding %d, buzzed %d, published %u",
         (int)(hold->g.state == VIDEO_XR_GRAB_HELD), (int)hold->buzzed,
         (unsigned)t_grabs.hand[1].mark_slot);
   t_aim_tracked[2] = true;
   t_take_end();

   /* Its lid outliving its mark: hinged flush beside screen 1, 12.5 cm
    * out and still till the mark has gone. */
   t_take_flush();
   t_aim[2].position.x += 0.125f;
   t_turn_polls(16, 0.0f, 10000, false);
   CHECK(hold->lidded && !hold->buzzed,
         "12.5 cm out 150 ms: lidded %d, buzzed %d", (int)hold->lidded,
         (int)hold->buzzed);
   t_hold("edit/grab", 1, 1.0f, 0.0f);
   t_clock += 10000;
   t_edit_poll();
   CHECK(     input_openxr_edit.hand[0].g.state == VIDEO_XR_GRAB_HELD
         && input_openxr_edit.hand[0].g.slot == 0 && !hold->lidded,
         "screen 1 taken under a lid: held %u, lidded %d",
         input_openxr_edit.hand[0].g.slot, (int)hold->lidded);
   t_take_end();
   t_angles = VIDEO_OPENXR_EDIT_SNAP_ANGLES_15;

   /* The same with the right hand lost, screen 1 then let go: the lid
    * stays gone. */
   {
      bool lidded, taken;
      t_take_flush();
      t_aim[2].position.x += 0.125f;
      t_turn_polls(16, 0.0f, 10000, false);
      lidded           = hold->lidded && !hold->buzzed;
      t_aim_tracked[2] = false;
      t_hold("edit/grab", 1, 1.0f, 0.0f);
      t_clock += 10000;
      t_edit_poll();
      taken = input_openxr_edit.hand[0].g.state == VIDEO_XR_GRAB_HELD
         && input_openxr_edit.hand[0].g.slot == 0;
      t_let_go();
      t_hold("edit/grab", 2, 1.0f, 0.0f);
      t_clock += 10000;
      t_edit_poll();
      CHECK(     lidded && taken
            && input_openxr_edit.hand[0].g.state != VIDEO_XR_GRAB_HELD
            && hold->g.state == VIDEO_XR_GRAB_HELD && !hold->lidded,
            "lost, screen 1 taken under a lid and let go: lidded %d "
            "before, taken %d, holding %d, lidded %d", (int)lidded,
            (int)taken, (int)(hold->g.state == VIDEO_XR_GRAB_HELD),
            (int)hold->lidded);
      t_aim_tracked[2] = true;
      t_take_end();
      t_angles = VIDEO_OPENXR_EDIT_SNAP_ANGLES_15;
   }

   /* Screen 1 gone from the quads. */
   t_hinged_at(VIDEO_OPENXR_EDIT_HINGE_GAP_0, 0.0, 0.0);
   memmove(&t_quads.quads[0], &t_quads.quads[1],
         (t_quads.num_quads - 1) * sizeof(t_quads.quads[0]));
   t_quads.num_quads--;
   t_tilt_polls(1, 0.0f, false);
   CHECK(!hold->buzzed, "screen 1 gone: buzzed %d", (int)hold->buzzed);
   t_take_end();

   /* Untracked 300 ms while out, then tracked and still out: the gap
    * counts 50 ms, so 100 ms more of tracked polls let it go. */
   {
      double x;
      double y, z, gap;
      bool kept = true;
      t_hinged_at(VIDEO_OPENXR_EDIT_HINGE_GAP_0, 0.0, 0.0);
      y   = t_aim[2].position.y;
      z   = t_aim[2].position.z;
      gap = t_hand_gap(&x);
      t_mark_out(0, y, z, gap, 0.035);
      t_tilt_polls(1, 0.0f, false);
      t_aim_tracked[2] = false;
      for (k = 0; k < 30; k++)
      {
         t_tilt_polls(1, 0.0f, false);
         kept = kept && hold->buzzed;
      }
      t_aim_tracked[2] = true;
      for (k = 1; k <= 11; k++)
      {
         t_tilt_polls(1, 0.0f, false);
         if (k <= 10)
            kept = kept && hold->buzzed;
      }
      CHECK(kept && !hold->buzzed,
            "untracked 300 ms out: kept %d to 140 ms seen out, buzzed %d "
            "at 150", (int)kept, (int)hold->buzzed);
      t_take_end();
   }

   /* A stall while out: 50 ms out, then one poll 200 ms on, which
    * counts 50 ms, so 50 ms more of polls let it go. */
   {
      double x;
      double y, z, gap;
      bool stalled;
      bool kept = true;
      t_hinged_at(VIDEO_OPENXR_EDIT_HINGE_GAP_0, 0.0, 0.0);
      y   = t_aim[2].position.y;
      z   = t_aim[2].position.z;
      gap = t_hand_gap(&x);
      t_mark_out(0, y, z, gap, 0.035);
      t_tilt_polls(6, 0.0f, false);
      t_turn_polls(1, 0.0f, 200000, false);
      stalled = hold->buzzed;
      for (k = 1; k <= 5; k++)
      {
         t_tilt_polls(1, 0.0f, false);
         if (k <= 4)
            kept = kept && hold->buzzed;
      }
      CHECK(stalled && kept && !hold->buzzed,
            "stalled 200 ms out: buzzed %d after it, kept %d to 140 ms "
            "seen out, buzzed %d at 150", (int)stalled, (int)kept,
            (int)hold->buzzed);
      t_take_end();
   }

   /* Untracked 200 ms in range, then tracked and out: 150 ms from that
    * poll. */
   {
      double x;
      double y, z, gap;
      bool kept;
      t_hinged_at(VIDEO_OPENXR_EDIT_HINGE_GAP_0, 0.0, 0.0);
      y   = t_aim[2].position.y;
      z   = t_aim[2].position.z;
      gap = t_hand_gap(&x);
      t_aim_tracked[2] = false;
      t_tilt_polls(20, 0.0f, false);
      kept             = hold->buzzed;
      t_aim_tracked[2] = true;
      t_mark_out(0, y, z, gap, 0.035);
      for (k = 1; k <= 16; k++)
      {
         t_tilt_polls(1, 0.0f, false);
         if (k <= 15)
            kept = kept && hold->buzzed;
      }
      CHECK(kept && !hold->buzzed,
            "untracked 200 ms in range, then out: kept %d to 140 ms, "
            "buzzed %d at 150", (int)kept, (int)hold->buzzed);
      t_take_end();
   }

   /* Snap to Screens off. */
   {
      double x;
      double y, z, gap;
      bool marked = false;
      t_gap  = VIDEO_OPENXR_EDIT_HINGE_GAP_0;
      t_snap = false;
      t_edit_start();
      t_snap_under(0.0);
      t_tilt_polls(1, 0.0f, false);
      t_aim[2].position.x = 0.07f;
      t_tilt_polls(1, 0.0f, false);
      marked = hold->buzzed;
      y      = t_aim[2].position.y;
      z      = t_aim[2].position.z;
      gap    = t_hand_gap(&x);
      for (k = 0; k < 40; k++)
      {
         t_mark_out(k / 20, y, z, gap, (k % 20 < 10) ? 0.035 : 0.02);
         t_tilt_polls(1, 0.0f, false);
         marked = marked || hold->buzzed;
      }
      CHECK(!marked, "Snap to Screens off: marked %d", (int)marked);
      t_take_end();
   }
}

/* Screen 2 hinged under screen 1 flat, its hand shaken as on a device
 * (t_shake, 0.5 degrees) for 640 polls, held about 2 mm short of where
 * a let-go stops settling, the shake straddling it: the stop's end at
 * Hinge Gap 0, 2 and 1, and screen 1's plane, there shaken in depth
 * too; short of 10 cm apart. Inside the click's fade and at the settle
 * band's end at Gap 2, and at the level band's end, where a let-go
 * settles in it either side. Marked there, its mark never flips, with
 * one long buzz, and every publish carries it. Swept from 1 cm inside
 * the stop's end or the plane's to 2 cm out, past the shake's reach,
 * and back, it flips twice, with two. Three frames a poll keep their
 * layer count, every layer's index and every outline's crop, but in
 * the polls where it flips. */
static void test_edit_mark_shaken(void)
{
   /* The Hinge Gap; the hand's mean metres apart (negative: past
    * touching), along and in front of the plane; swept, across 0 or
    * in depth 1, else 2; whether the shake takes it out of the mark's
    * range. */
   static const struct
   {
      unsigned gap;
      double apart, along, front;
      unsigned sweep;
      bool edge;
   } spots[] = {
      { VIDEO_OPENXR_EDIT_HINGE_GAP_0, -0.028, 0.07, 0.0, 2, true },
      { VIDEO_OPENXR_EDIT_HINGE_GAP_2, -0.028, 0.07, 0.0, 2, true },
      { VIDEO_OPENXR_EDIT_HINGE_GAP_1, -0.033, 0.07, 0.0, 2, true },
      { VIDEO_OPENXR_EDIT_HINGE_GAP_0, 0.0, 0.07, 0.028, 2, true },
      { VIDEO_OPENXR_EDIT_HINGE_GAP_0, 0.098, 0.07, 0.0, 2, true },
      { VIDEO_OPENXR_EDIT_HINGE_GAP_2, 0.035, 0.07, 0.0, 2, false },
      { VIDEO_OPENXR_EDIT_HINGE_GAP_2, 0.05, 0.07, 0.0, 2, false },
      { VIDEO_OPENXR_EDIT_HINGE_GAP_2, 0.02, 0.06, 0.0, 2, false },
      { VIDEO_OPENXR_EDIT_HINGE_GAP_0, -0.02, 0.07, 0.0, 0, true },
      { VIDEO_OPENXR_EDIT_HINGE_GAP_0, 0.0, 0.07, 0.02, 1, true } };
   const input_openxr_hold_t *hold = &input_openxr_edit.hand[1];
   video_xr_pose_t pose;
   unsigned i, k;
   for (i = 0; i < sizeof(spots) / sizeof(spots[0]); i++)
   {
      t_layout_t lay;
      unsigned fails  = 0;
      unsigned flips  = 0;
      unsigned pubs   = 0;
      unsigned counts = 0;
      unsigned moved  = 0;
      unsigned cuts   = 0;
      unsigned stray  = 0;
      unsigned l      = t_longs[2];
      unsigned want   = (spots[i].sweep < 2) ? 2u : 0u;
      double y, z;
      bool was;
      t_hinged_at(spots[i].gap, 0.0, 0.0);
      t_carry_under(spots[i].apart, spots[i].along, false);
      for (k = 1; k <= (unsigned)(spots[i].front * 1000.0 + 0.5); k++)
      {
         t_aim[2].position.z += 0.001f;
         t_tilt_polls(1, 0.0f, false);
      }
      t_tilt_polls(2, 0.0f, false);
      was     = hold->buzzed;
      CHECK(was, "spot %u: not marked before the shake", i);
      lay.n    = 0;
      lay.bent = 0;
      t_frames(&lay, &counts, &moved);
      t_still = t_aim[2];
      y       = t_still.position.y;
      z       = t_still.position.z;
      for (k = 0; k < 640; k++)
      {
         unsigned c;
         /* 3 cm further out by poll 320, back by 640. */
         double d = 0.03 * ((k < 320) ? k : 640 - k) / 320.0;
         if (spots[i].sweep == 0)
            t_still.position.y = (float)(y + d);
         else if (spots[i].sweep == 1)
            t_still.position.z = (float)(z + d);
         t_shake(k, 0.5);
         if (spots[i].front > 0.0)
            t_aim[2].position.z += (float)(1.8 * tan(0.5 * M_PI / 180.0)
                  * sin(2.0 * M_PI * 8.0 * k * 0.01));
         t_tilt_polls(1, 0.0f, false);
         if (!video_xr_grab_settle(&hold->g, &hold->aim, &t_quads,
                  input_openxr_edit_holding(&input_openxr_edit),
                  &hold->mark, &pose))
            fails++;
         c      = t_frames(&lay, &counts, &moved);
         cuts  += c;
         stray += (c && hold->buzzed == was) ? c : 0u;
         flips += (hold->buzzed != was) ? 1u : 0u;
         was    = hold->buzzed;
         if (     t_grabs.hand[1].mark_slot
                  != (hold->buzzed ? hold->mark.slot + 1u : 0u)
               || t_grabs.hand[1].mark_on
                  != (hold->buzzed ? hold->mark.on : 0u)
               || t_grabs.hand[1].mark_edge
                  != (hold->buzzed ? hold->mark.edge : 0u))
            pubs++;
      }
      CHECK(     flips == want && t_longs[2] - l == (want ? 2u : 1u)
            && (fails > 0) == spots[i].edge && !pubs,
            "shaken at spot %u: %u flips, %u long buzzes, out %u polls, "
            "%u published off its mark", i, flips, t_longs[2] - l, fails,
            pubs);
      CHECK(!counts && !moved && cuts == want && !stray && !lay.bent,
            "shaken at spot %u, its frames: %u with another layer count, "
            "%u with layers moved, %u cut otherwise, %u of them in polls "
            "it did not flip, want %u; %u with an outline not whole", i,
            counts, moved, cuts, stray, want, lay.bent);
      t_take_end();
   }
   memset(&t_xr_frame.grabs, 0, sizeof(t_xr_frame.grabs));
   t_xr_frame.hinged = 0;
   t_gap = VIDEO_OPENXR_EDIT_HINGE_GAP_0;
}

/* Edit mode's hinge lines are slots: one for each hand holding a
 * screen, hand 0 first, then one for each pair at rest. A slot whose
 * line does not show is drawn in place from its screen's middle, at
 * rows of the image transparent either way up: a mark taken or let go
 * changes no count or index, and a pair whose screen a hand holds is
 * hidden in its slot. Short of room the beams go first, then the tag,
 * then slots from the end, then outlines. */
static void test_edit_hinge_slots(void)
{
   unsigned i, n, at, c, k;
   int32_t cut;
   uint8_t *px;
   video_xr_quad_set_t shown;
   const XrCompositionLayerQuad *q;
   const XrCompositionLayerBaseHeader *layers[OPENXR_MAX_EXTRA_LAYERS];
   XrSpace local;
   float sx[4], sy[4];
   unsigned first                  = 0;
   const input_openxr_hold_t *hold = &input_openxr_edit.hand[1];

   /* Screen 2 held 20 cm under screen 1, then in its hinge, then out
    * past its mark's 150 ms: one slot after the outlines throughout. */
   t_hinged_at(VIDEO_OPENXR_EDIT_HINGE_GAP_0, 0.0, 0.0);
   local          = t_xr_session.local_space;
   t_head_tracked = true;
   t_cursor       = true;
   for (i = 0; i < 3; i++)
   {
      bool on = i == 1;
      t_carry_under(on ? 0.0 : 0.2, 0.07, false);
      t_tilt_polls(on ? 2 : 20, 0.0f, false);
      shown = t_quads;
      t_follow(&shown);
      n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
            OPENXR_MAX_EXTRA_LAYERS, &t_under);
      if (!i)
         first = n;
      for (at = 0, c = 0; at < n; at++)
         c += t_kind(layers[at], &cut) == 3;
      q = (const XrCompositionLayerQuad*)layers[4];
      CHECK(     hold->buzzed == on
            && t_grabs.hand[1].mark_slot == (on ? 1u : 0u)
            && n == first && t_under == 2 && c == 1
            && t_kind(layers[4], &cut) == 3
            && q->subImage.imageRect.offset.y
               == (on ? VIDEO_XR_HINGE_Y : VIDEO_XR_HINGE_HIDDEN_Y),
            "held, %s: marked %d (%u published), %u layers of %u, %u "
            "under, %u slots, layer 4 at row %d", on ? "hinged" : "free",
            (int)hold->buzzed, t_grabs.hand[1].mark_slot, n, first,
            t_under, c, (int)q->subImage.imageRect.offset.y);
   }
   memset(&t_xr_frame.grabs, 0, sizeof(t_xr_frame.grabs));
   t_xr_frame.hinged = 0;
   t_cursor          = false;
   t_head_tracked    = false;
   t_take_end();

   /* Hands 0 and 1 holding screens 4 and 5, screens 2 and 3 resting on
    * screen 1's right and bottom edges. */
   t_edit_start();
   local          = t_xr_session.local_space;
   t_head_tracked = true;
   t_cursor       = true;
   shown          = t_quads;
   for (i = 2; i < 6; i++)
   {
      shown.quads[i]        = shown.quads[0];
      shown.quads[i].slot   = i - 1;
      shown.quads[i].screen = i - 1;
   }
   shown.quads[2].pose.position.x = 1.6f;
   shown.quads[3].pose.position.y = -0.96f;
   shown.quads[4].pose.position.x = -2.0f;
   shown.quads[5].pose.position.x = -2.0f;
   shown.quads[5].pose.position.y = 1.2f;
   shown.num_quads                = 6;
   memset(&t_xr_frame.grabs, 0, sizeof(t_xr_frame.grabs));
   t_xr_frame.hinged                = 0;
   t_xr_frame.grabs.hand[0].state   = VIDEO_XR_GRAB_HELD;
   t_xr_frame.grabs.hand[0].slot    = 3;
   t_xr_frame.grabs.hand[1].state   = VIDEO_XR_GRAB_HELD;
   t_xr_frame.grabs.hand[1].slot    = 4;
   t_xr_frame.grabs.num_rests       = 2;
   t_xr_frame.grabs.rests[0].slot   = 0;
   t_xr_frame.grabs.rests[0].edge   = 2;
   t_xr_frame.grabs.rests[0].target = 1;
   t_xr_frame.grabs.rests[0].on     = 3;
   t_xr_frame.grabs.rests[1].slot   = 0;
   t_xr_frame.grabs.rests[1].edge   = 1;
   t_xr_frame.grabs.rests[1].target = 2;
   t_xr_frame.grabs.rests[1].on     = 0;
   n = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   for (at = 0, c = 0; at < n; at++)
      c += t_kind(layers[at], &cut) == 3;
   CHECK(     n > 10 && c == 4 && t_under == 5
         && t_slot_is((const XrCompositionLayerQuad*)layers[7], true,
            -2.0f, 0.0f, -1.8f, -2.0f + VIDEO_XR_HINGE_WIDTH, 0.0f, -1.8f)
         && t_slot_is((const XrCompositionLayerQuad*)layers[8], true,
            -2.0f, 1.2f, -1.8f, -2.0f + VIDEO_XR_HINGE_WIDTH, 1.2f, -1.8f)
         && t_slot_is((const XrCompositionLayerQuad*)layers[9], false,
            0.8f, -0.48f, -1.8f, 0.8f, 0.48f, -1.8f)
         && t_slot_is((const XrCompositionLayerQuad*)layers[10], false,
            -0.8f, -0.48f, -1.8f, 0.8f, -0.48f, -1.8f),
         "two hands, two pairs at rest: %u layers, %u under, %u slots; "
         "want hand 0's and hand 1's hidden on screens 4 and 5, then both "
         "lines, after seven outlines", n, t_under, c);
   for (i = 0; i < 4; i++)
   {
      q     = (const XrCompositionLayerQuad*)layers[(n > 10) ? 7 + i : 0];
      sx[i] = q->pose.position.x;
      sy[i] = q->pose.position.y;
   }

   /* Short of room, one layer at a time: the beams go, then the tag,
    * then slots from the end; the outlines last. */
   for (c = n; c >= OPENXR_KEPT_EXTRA_LAYERS; c--)
   {
      unsigned kinds[7];
      unsigned r    = c - OPENXR_KEPT_EXTRA_LAYERS;
      unsigned o    = (r < 7) ? r : 7;
      unsigned l    = (r - o < 4) ? r - o : 4;
      unsigned t    = (r - o - l > 0) ? 1u : 0u;
      unsigned b    = (r - o - l - t < 2) ? r - o - l - t : 2u;
      unsigned j    = 0;
      unsigned slid = 0;
      memset(kinds, 0, sizeof(kinds));
      at = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
            c, &t_under);
      for (k = 0; k < at; k++)
      {
         unsigned kind = t_kind(layers[k], &cut);
         kinds[kind]++;
         if (kind != 3)
            continue;
         q = (const XrCompositionLayerQuad*)layers[k];
         if (     k != 7 + j || j >= 4 || !t_near(q->pose.position.x, sx[j])
               || !t_near(q->pose.position.y, sy[j]))
            slid++;
         j++;
      }
      CHECK(     at == o + l + t + b + 2 && kinds[1] + kinds[2] == o
            && kinds[3] == l && kinds[5] == t && kinds[4] == b
            && kinds[6] == 2 && !slid,
            "room for %u: %u layers, %u outlines, %u slots (%u out of "
            "place), %u tags, %u beams, %u dots; want %u, %u, %u, %u, %u",
            c, at, kinds[1] + kinds[2], kinds[3], slid, kinds[5], kinds[4],
            kinds[6], o, l, t, b, 2u);
   }

   /* Hand 1 on screen 2 instead: the right edge's pair hidden in its
    * slot on screen 1, the count the same. */
   t_xr_frame.grabs.hand[1].slot = 1;
   at = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
         OPENXR_MAX_EXTRA_LAYERS, &t_under);
   CHECK(     at == n && n > 10
         && t_slot_is((const XrCompositionLayerQuad*)layers[7], true,
            -2.0f, 0.0f, -1.8f, -2.0f + VIDEO_XR_HINGE_WIDTH, 0.0f, -1.8f)
         && t_slot_is((const XrCompositionLayerQuad*)layers[8], true,
            1.6f, 0.0f, -1.8f, 1.6f + VIDEO_XR_HINGE_WIDTH, 0.0f, -1.8f)
         && t_slot_is((const XrCompositionLayerQuad*)layers[9], true,
            0.0f, 0.0f, -1.8f, VIDEO_XR_HINGE_WIDTH, 0.0f, -1.8f)
         && t_slot_is((const XrCompositionLayerQuad*)layers[10], false,
            -0.8f, -0.48f, -1.8f, 0.8f, -0.48f, -1.8f),
         "screen 2 held: %u layers of %u, want its pair hidden in place",
         at, n);

   /* The bottom edge's pair listed from screen 3, then hand 1 on
    * screen 3: that pair hidden in its slot on screen 3, the count the
    * same. */
   t_xr_frame.grabs.rests[1].slot   = 2;
   t_xr_frame.grabs.rests[1].edge   = 0;
   t_xr_frame.grabs.rests[1].target = 0;
   t_xr_frame.grabs.rests[1].on     = 1;
   for (i = 0; i < 2; i++)
   {
      bool own = i == 1;
      float hx = own ? 0.0f : -2.0f;
      float hy = own ? -0.96f : 1.2f;
      t_xr_frame.grabs.hand[1].slot = own ? 2 : 4;
      at = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
            OPENXR_MAX_EXTRA_LAYERS, &t_under);
      CHECK(     at == n && n > 10
            && t_slot_is((const XrCompositionLayerQuad*)layers[7], true,
               -2.0f, 0.0f, -1.8f, -2.0f + VIDEO_XR_HINGE_WIDTH, 0.0f,
               -1.8f)
            && t_slot_is((const XrCompositionLayerQuad*)layers[8], true,
               hx, hy, -1.8f, hx + VIDEO_XR_HINGE_WIDTH, hy, -1.8f)
            && t_slot_is((const XrCompositionLayerQuad*)layers[9], false,
               0.8f, -0.48f, -1.8f, 0.8f, 0.48f, -1.8f)
            && (own
               ? t_slot_is((const XrCompositionLayerQuad*)layers[10], true,
                  0.0f, -0.96f, -1.8f, VIDEO_XR_HINGE_WIDTH, -0.96f, -1.8f)
               : t_slot_is((const XrCompositionLayerQuad*)layers[10],
                  false, -0.8f, -0.48f, -1.8f, 0.8f, -0.48f, -1.8f)),
            "screen 3's pair, screen 3 %s: %u layers of %u, want it %s",
            own ? "held" : "free", at, n,
            own ? "hidden in place on screen 3" : "shown");
   }

   /* Hand 0 holding screen 4 on screen 1's left edge, hand 1 screen 5
    * on its top, the right edge's pair at rest: each hand's mark taken
    * and let go in turn moves no slot and changes no count. */
   shown.quads[4].pose.position.x = -1.6f;
   shown.quads[5].pose.position.x = 0.0f;
   shown.quads[5].pose.position.y = 0.96f;
   memset(&t_grabs, 0, sizeof(t_grabs));
   for (i = 0; i < 2; i++)
   {
      video_xr_grab_t *g = &t_grabs.hand[i];
      g->state           = VIDEO_XR_GRAB_HELD;
      g->slot            = 3 + i;
      g->last            = shown.quads[4 + i].pose;
      g->width           = shown.quads[4 + i].width;
      g->height          = shown.quads[4 + i].height;
      g->mark_on         = i ? 0 : 3;
      g->mark_edge       = i ? 1 : 2;
   }
   t_grabs.num_rests = 1;
   t_grabs.rests[0]  = t_xr_frame.grabs.rests[0];
   for (i = 0; i < 5; i++)
   {
      /* Per hand, hidden on its screen, then its line. */
      static const float ends[2][2][6] = {
         { { -1.6f, 0.0f, -1.8f, -1.6f + VIDEO_XR_HINGE_WIDTH, 0.0f, -1.8f },
           { -0.8f, -0.48f, -1.8f, -0.8f, 0.48f, -1.8f } },
         { { 0.0f, 0.96f, -1.8f, VIDEO_XR_HINGE_WIDTH, 0.96f, -1.8f },
           { -0.8f, 0.48f, -1.8f, 0.8f, 0.48f, -1.8f } } };
      static const unsigned marks[5] = { 0, 1, 3, 2, 0 };
      unsigned j;
      unsigned m    = marks[i];
      unsigned slid = 0;
      bool placed;
      t_grabs.hand[0].mark_slot = (m & 1u) ? 1 : 0;
      t_grabs.hand[1].mark_slot = (m & 2u) ? 1 : 0;
      t_follow(&shown);
      at = t_hooks.frame_layers(t_hooks.user, t_time, &shown, local, layers,
            OPENXR_MAX_EXTRA_LAYERS, &t_under);
      if (!i)
         first = at;
      for (k = 0, c = 0; k < at; k++)
         if (t_kind(layers[k], &cut) == 3)
         {
            c++;
            if (k < 7 || k > 9)
               slid++;
         }
      placed = at > 9 && t_slot_is((const XrCompositionLayerQuad*)layers[9],
            false, 0.8f, -0.48f, -1.8f, 0.8f, 0.48f, -1.8f);
      for (j = 0; j < 2 && placed; j++)
      {
         unsigned on     = (m >> j) & 1u;
         const float *e  = ends[j][on];
         placed          = t_slot_is(
               (const XrCompositionLayerQuad*)layers[7 + j], !on, e[0],
               e[1], e[2], e[3], e[4], e[5]);
      }
      CHECK(     at == first && c == 3 && !slid && placed
            && t_xr_frame.hinged == m,
            "marks %#x (shown %#x): %u layers of %u, %u slots, %u out of "
            "place, %s", m, t_xr_frame.hinged, at, first, c, slid,
            placed ? "each where it shows" : "one not where it shows");
   }
   memset(&t_grabs, 0, sizeof(t_grabs));
   memset(&t_xr_frame.grabs, 0, sizeof(t_xr_frame.grabs));
   t_xr_frame.hinged = 0;
   t_cursor          = false;
   t_head_tracked    = false;
   t_edit_end();

   /* The hidden rows, either way up: nothing drawn. */
   px = (uint8_t*)malloc((size_t)VIDEO_XR_CURSOR_W * VIDEO_XR_CURSOR_H * 4);
   CHECK(px != NULL, "no room for the image");
   if (!px)
      return;
   video_xr_cursor_pixels(px);
   for (i = 0, c = 0; i < VIDEO_XR_HINGE_ROWS; i++)
      for (k = VIDEO_XR_HINGE_X + 1;
            k < VIDEO_XR_HINGE_X + VIDEO_XR_HINGE_COLS - 1; k++)
      {
         const uint8_t *a = px + ((size_t)(VIDEO_XR_HINGE_HIDDEN_Y + i)
               * VIDEO_XR_CURSOR_W + k) * 4;
         const uint8_t *b = px + ((size_t)(VIDEO_XR_CURSOR_H - 1
                  - VIDEO_XR_HINGE_HIDDEN_Y - i) * VIDEO_XR_CURSOR_W + k)
            * 4;
         c += (a[0] | a[1] | a[2] | a[3]) ? 1u : 0u;
         c += (b[0] | b[1] | b[2] | b[3]) ? 1u : 0u;
      }
   free(px);
   CHECK(c == 0, "%u texels drawn in the hidden rows", c);
}

/* Hand 2 lets go: by the grip (how 0), a recenter (1) or edit mode's
 * end (2). */
static void t_let_go_by(unsigned how)
{
   if (how == 1)
      input_openxr_edit_release();
   else if (how == 2)
      input_openxr_edit_toggle();
   else
   {
      t_let_go();
      t_clock += 10000;
      t_edit_poll();
   }
}

/* Screen 2 hinged under screen 1 flat, its hand 12 polls out of its
 * hinge's range, the mark still shown, and let go by the grip, a
 * recenter or edit mode's end: it settles from the newest place it
 * would, touching at Hinge Gap 0 pushed 3.5 cm past, at the gap at Gap 2
 * carried 3.5 cm off the plane, listed at rest till edit mode ends. 20
 * polls out, the mark gone, it rests where it shows, none listed. In
 * range at Gap 2 and carried 1 cm along in its last 100 ms, the grip
 * settles it from 80 ms before; a recenter or edit mode's end from
 * where it shows. */
static void test_edit_mark_tail(void)
{
   static const struct
   {
      unsigned gap;
      unsigned out;
      double want;
   } rows[] = {
      { VIDEO_OPENXR_EDIT_HINGE_GAP_0, 0, 0.0 },
      { VIDEO_OPENXR_EDIT_HINGE_GAP_2, 1, 0.02 } };
   unsigned i, j, how;
   for (how = 0; how < 3; how++)
   {
      for (j = 0; j < 2; j++)
         for (i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
         {
            double x, y, z, gap, plane, before, held_plane;
            unsigned polls = j ? 20u : 12u;
            t_hinged_at(rows[i].gap, 0.0,
                  rows[i].gap == VIDEO_OPENXR_EDIT_HINGE_GAP_2 ? 0.02 : 0.0);
            y   = t_aim[2].position.y;
            z   = t_aim[2].position.z;
            gap = t_hand_gap(&x);
            t_mark_out(rows[i].out, y, z, gap, 0.035);
            t_tilt_polls(polls, 0.0f, false);
            before = t_under_gap(&held_plane);
            t_let_go_by(how);
            gap = t_under_gap(&plane);
            CHECK(     t_grabs.let_go[1].state == VIDEO_XR_GRAB_RELEASED
                  && (j
                     ? (t_grabs.num_rests == 0 && fabs(gap - before) < 1e-5
                        && fabs(plane - held_plane) < 1e-5)
                     : (t_grabs.num_rests == (how < 2 ? 1u : 0u)
                        && fabs(gap - rows[i].want) < 1e-5
                        && fabs(plane) < 1e-5)),
                  "tail row %u, %u polls out, let go by %u: %u at rest, "
                  "gap %.5f from %.5f, plane %.5f from %.5f", i, polls,
                  how, t_grabs.num_rests, gap, before, plane, held_plane);
            t_take_end();
         }
      {
         double gap, plane, along, x;
         t_hinged_at(VIDEO_OPENXR_EDIT_HINGE_GAP_2, 0.0, 0.02);
         t_carry_under(0.035, 0.07, false);
         t_tilt_polls(9, 0.0f, false);
         t_carry_under(0.035, 0.08, false);
         x = t_edge_along(true);
         t_let_go_by(how);
         gap   = t_under_gap(&plane);
         along = t_edge_along(true);
         CHECK(     t_grabs.let_go[1].state == VIDEO_XR_GRAB_RELEASED
               && fabs(gap - 0.02) < 1e-5 && fabs(plane) < 1e-5
               && (how ? fabs(along - x) < 1e-5 : fabs(along - x) > 0.005)
               && t_grabs.num_rests == (how < 2 ? 1u : 0u),
               "in range, carried along, let go by %u: gap %.5f, plane "
               "%.5f, along %.5f from %.5f, %u at rest", how, gap, plane,
               along, x, t_grabs.num_rests);
         t_take_end();
      }
   }
   t_gap = VIDEO_OPENXR_EDIT_HINGE_GAP_0;
}

/* Screen 2 hinged under screen 1 flat at Hinge Gap 0 and pushed 3.5 cm
 * past touching, its mark's 150 ms running, then carried flush right of
 * screen 1: that hinge takes the mark at once, with the long buzz, and
 * its time out starts afresh. */
static void test_edit_mark_switch(void)
{
   const input_openxr_hold_t *hold = &input_openxr_edit.hand[1];
   double x, y, z, gap;
   bool tail;
   unsigned l;
   video_xr_hinge_t was;
   t_hinged_at(VIDEO_OPENXR_EDIT_HINGE_GAP_0, 0.0, 0.0);
   y   = t_aim[2].position.y;
   z   = t_aim[2].position.z;
   gap = t_hand_gap(&x);
   was = hold->mark;
   t_mark_out(0, y, z, gap, 0.035);
   t_tilt_polls(5, 0.0f, false);
   tail = hold->buzzed && hold->out && !hold->hinged;
   l    = t_longs[2];
   t_aim[2].position.x = 1.20f;
   t_aim[2].position.y = 0.07f;
   t_tilt_polls(1, 0.0f, false);
   CHECK(     tail && hold->hinged && hold->buzzed && !hold->out
         && hold->mark.slot == 0 && hold->mark.edge != was.edge
         && t_longs[2] == l + 1 && t_dur[2] == INPUT_OPENXR_HINGE_BUZZ_NS,
         "switched mid-tail: from the tail %d, hinged %d, buzzed %d, out "
         "%d, edge %u from %u, %u long buzzes", (int)tail,
         (int)hold->hinged, (int)hold->buzzed, (int)(hold->out != 0),
         hold->mark.edge, was.edge, t_longs[2] - l);
   t_take_end();
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
   test_start_menu_after_edit();
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
   test_edit_tilt_stick();
   test_edit_stick_lock();
   test_edit_step_stick();
   test_edit_step_waver();
   test_edit_step_sizes();
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
   test_edit_release_rewind();
   test_edit_release_twitch();
   test_edit_release_stall();
   test_edit_release_fast_frames();
   test_edit_trail_frames();
   test_edit_trail_hands();
   test_edit_release_smoothed();
   test_edit_release_smoothed_twitch();
   test_edit_settle();
   test_edit_settle_target_held();
   test_edit_settle_twitch();
   test_edit_mark_debounce();
   test_edit_mark_shaken();
   test_edit_mark_tail();
   test_edit_mark_switch();
   test_edit_regrab();
   test_edit_frame_aim();
   test_edit_outlines();
   test_edit_backs();
   test_edit_handle_sticky();
   test_edit_beams();
   test_edit_select_bindings();
   test_edit_select_cycle();
   test_edit_select_fallback();
   test_edit_select_grab();
   test_edit_select_grips();
   test_edit_held_lit();
   test_edit_select_lit();
   test_edit_tag();
   test_edit_snap();
   test_edit_snap_two_hands();
   test_edit_turn_snap();
   test_edit_step_snap();
   test_edit_snap_edge();
   test_edit_snap_commit();
   test_edit_hinge_swing();
   test_edit_hinge_hold();
   test_edit_hinge_rearm();
   test_edit_hinge_drop();
   test_edit_hinge_cells();
   test_edit_hinge_crops();
   test_edit_hinge_slots();
   test_edit_rest_take();
   test_edit_rest_swing();
   test_edit_rest_take_two();
   test_edit_rest_take_other();
   test_edit_rest_take_sides();
   test_edit_rest_take_target();
   test_edit_rest_take_over();
   test_edit_rest_take_out();
   test_edit_gap_push_rest();
   test_edit_hinge_under();
   test_edit_hinge_flick();
   test_edit_hinge_lid_ends();
   test_edit_hinge_glide();
   test_edit_hinge_lid_far();
   test_edit_hinge_door();
   test_edit_hinge_lid_reach();
   test_edit_hinge_lid_moved();
   test_edit_hinge_lid_new_hold();
   test_edit_hinge_diagonal();
   test_edit_hinge_level();
   test_edit_hinge_rolled();
   test_edit_gap_swing();
   test_edit_gap_stop();
   test_edit_gap_stop_one();
   test_edit_gap_push();
   test_edit_gap_push_shaken();
   test_edit_gap_push_buzz();
   test_edit_gap_push_off();
   test_edit_gap_tick();
   test_edit_gap_tick_turned();
   test_edit_gap_steps();
   test_edit_gap_menu();
   test_edit_gap_buzz();
   test_edit_hinge_buzz();
   test_edit_hinge_buzz_skip();
   test_edit_hinge_buzz_flick();
   test_edit_hinge_buzz_again();
   test_edit_hinge_take_target();
   test_edit_hinge_take_hop();
   test_edit_hinge_buzz_depth();
   test_edit_hinge_attach();
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
