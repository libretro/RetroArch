#include "openxr_input.h"
#include <features/features_cpu.h>
#include <math.h>
#ifdef ANDROID
#include <android/keycodes.h>
#endif
#include <compat/strl.h>
#include <retro_atomic.h>

#include "../../tasks/tasks_internal.h"
#include "../../verbosity.h"

enum
{
  XR_X,
  XR_Y,
  XR_A,
  XR_B,
  XR_LGRIP,
  XR_RGRIP,
  XR_LCLICK,
  XR_RCLICK,
  XR_LTRIGGER,
  XR_RTRIGGER,
  XR_LSTICK,
  XR_RSTICK,
  XR_MENU,
  XR_SYSTEM,
  XR_COUNT
};

#define CONTROLLER_PROFILE "/interaction_profiles/oculus/touch_controller"

static XrActionSet openxr_action_set;
static XrAction openxr_actions[XR_COUNT];
/* Written by openxr_input_sync (the frame loop's thread, which is the
 * video thread under threaded video) and read by the input drivers on
 * the main thread, so these cross-thread values are atomics.
 *
 * Four words, each stored and loaded whole: the buttons as a set, a
 * stick's x and y together, the two triggers together. They were a
 * word a button and a word an axis, so a read could take a stick's x
 * from one sync and its y from the next; a pair is one sync's now.
 * (One word is still not the next word's sync: a button and a stick
 * read in the same poll can be a sync apart.)
 *
 * What they hold while there is no fresh state to give - no session
 * attached, the session not running, a sync that failed, the session
 * without the focus - is "nothing held": see
 * openxr_input_release_all(). */
static retro_atomic_int_t openxr_buttons;      /* bit n: action n is down */
static retro_atomic_int_t openxr_sticks[2];    /* left, right: XR_PAIR(x, y) */
static retro_atomic_int_t openxr_triggers;     /* XR_PAIR(left, right) */
static retro_atomic_int_t openxr_attached_flag;
static retro_atomic_int_t openxr_menu_long_press;

/* two axis values, each -32767 to 32767, in one word */
#define XR_PAIR(lo, hi) ((int)(  (uint32_t)(uint16_t)(int16_t)(lo) \
                               | ((uint32_t)(uint16_t)(int16_t)(hi) << 16)))
#define XR_PAIR_LO(v)   ((int16_t)((uint32_t)(v) & 0xffff))
#define XR_PAIR_HI(v)   ((int16_t)((uint32_t)(v) >> 16))

/* long press menu button for F1; sync-thread only */
static bool openxr_menu_was_down;
static uint64_t openxr_menu_down_time;
/* something other than "nothing held" may be published; sync-thread only */
static bool openxr_state_live;

/* An axis as the runtime gives it, -1 to 1, as a stick's value. */
static int openxr_axis_value(float v)
{
  int out = (int)(v * 32767.0f);
  if (out >  32767) out =  32767;
  if (out < -32767) out = -32767;
  return out;
}

/* Nothing held. What readers are given whenever this file has no fresh
 * state for them: left as it was, a button that was down when the
 * state stopped coming stayed down for the frontend for as long as it
 * stopped - a sync that failed, the headset's own menu taking the
 * focus, the session ended. On the sync's thread. */
static void openxr_input_release_all(void)
{
  if (!openxr_state_live)
    return;
  retro_atomic_store_relaxed_int(&openxr_buttons, 0);
  retro_atomic_store_relaxed_int(&openxr_sticks[0], 0);
  retro_atomic_store_relaxed_int(&openxr_sticks[1], 0);
  retro_atomic_store_relaxed_int(&openxr_triggers, 0);
  retro_atomic_store_relaxed_int(&openxr_menu_long_press, 0);
  openxr_menu_was_down = false;
  openxr_state_live    = false;
}

/* The session is not running: there is no state to sync, and what was
 * synced last is not what is held. For the context's frame loop, in
 * place of the sync on a frame it does not begin. */
void openxr_input_idle(void)
{
  openxr_input_release_all();
}

#define HEAD_RAD2DEG 57.29577951f

bool openxr_input_init(XrInstance instance)
{
  static const char* names[] = {"x",
                                "y",
                                "a",
                                "b",
                                "left_grip",
                                "right_grip",
                                "left_stick_click",
                                "right_stick_click",
                                "left_trigger",
                                "right_trigger",
                                "left_stick",
                                "right_stick",
                                "menu",
                                "system"};
  static const char* paths[] = {
      "/user/hand/left/input/x/click",          "/user/hand/left/input/y/click",
      "/user/hand/right/input/a/click",         "/user/hand/right/input/b/click",
      "/user/hand/left/input/squeeze/value",    "/user/hand/right/input/squeeze/value",
      "/user/hand/left/input/thumbstick/click", "/user/hand/right/input/thumbstick/click",
      "/user/hand/left/input/trigger/value",    "/user/hand/right/input/trigger/value",
      "/user/hand/left/input/thumbstick",       "/user/hand/right/input/thumbstick",
      "/user/hand/left/input/menu/click",       "/user/hand/right/input/system/click" };

  XrActionSetCreateInfo set_info = {XR_TYPE_ACTION_SET_CREATE_INFO};
  XrActionSuggestedBinding bindings[XR_COUNT];
  XrInteractionProfileSuggestedBinding suggest = {XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
  XrResult result;
  unsigned i;

  strlcpy(set_info.actionSetName, "gameplay", sizeof(set_info.actionSetName));
  strlcpy(set_info.localizedActionSetName, "Gameplay", sizeof(set_info.localizedActionSetName));

  result = xrCreateActionSet(instance, &set_info, &openxr_action_set);
  if (result != XR_SUCCESS)
  {
    RARCH_ERR("[XR] xrCreateActionSet failed: %d\n", result);
    return false;
  }

  for (i = 0; i < XR_COUNT; i++)
  {
    XrActionCreateInfo info = {XR_TYPE_ACTION_CREATE_INFO};
    if (i == XR_LTRIGGER || i == XR_RTRIGGER)
      info.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
    else if (i == XR_LSTICK || i == XR_RSTICK)
      info.actionType = XR_ACTION_TYPE_VECTOR2F_INPUT;
    else
      info.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
    strlcpy(info.actionName, names[i], sizeof(info.actionName));
    strlcpy(info.localizedActionName, names[i], sizeof(info.localizedActionName));

    result = xrCreateAction(openxr_action_set, &info, &openxr_actions[i]);
    if (result != XR_SUCCESS)
    {
      RARCH_ERR("[XR] xrCreateAction failed for '%s': %d\n",
            names[i], result);
      return false;
    }

    result = xrStringToPath(instance, paths[i], &bindings[i].binding);
    if (result != XR_SUCCESS)
    {
      RARCH_ERR("[XR] xrStringToPath failed for '%s': %d\n",
            paths[i], result);
      return false;
    }

    bindings[i].action = openxr_actions[i];
  }

  result = xrStringToPath(instance,
    CONTROLLER_PROFILE,
    &suggest.interactionProfile);

  if (result != XR_SUCCESS)
  {
    RARCH_ERR("[XR] xrStringToPath failed for Touch controller profile: %d\n",
      result);
    return false;
  }

  suggest.countSuggestedBindings = XR_COUNT;
  suggest.suggestedBindings = bindings;

  result = xrSuggestInteractionProfileBindings(instance, &suggest);
  if (result != XR_SUCCESS)
  {
    RARCH_ERR("[XR] xrSuggestInteractionProfileBindings failed: %d\n",
      result);
    return false;
  }

  return true;
}

bool openxr_input_attach(XrSession session)
{
  XrSessionActionSetsAttachInfo info = {XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
  info.countActionSets = 1;
  info.actionSets = &openxr_action_set;
  bool attached = xrAttachSessionActionSets(session, &info) == XR_SUCCESS;
  retro_atomic_store_relaxed_int(&openxr_attached_flag, attached ? 1 : 0);

  if (attached)
  {
    input_autoconfigure_connect(
          "Meta Quest Touch Plus Controller",
          NULL,
          NULL,
          "android",
          0,
          0,
          0);

    RARCH_LOG("[XR] Registered Meta Quest Touch Plus Controller on port 0\n");
  }

  return retro_atomic_load_relaxed_int(&openxr_attached_flag) != 0;
}

void openxr_input_sync(XrSession session)
{
  XrActiveActionSet active = {openxr_action_set, XR_NULL_PATH};
  XrActionsSyncInfo sync = {XR_TYPE_ACTIONS_SYNC_INFO};
  unsigned i;
  unsigned buttons = 0;
  int stick[2]     = {0, 0};
  int trigger[2]   = {0, 0};

  if (!retro_atomic_load_relaxed_int(&openxr_attached_flag))
    return;

  sync.countActiveActionSets = 1;
  sync.activeActionSets = &active;

  /* Anything but success is no fresh state: an error, or the session
   * without the focus (XR_SESSION_NOT_FOCUSED is not XR_SUCCESS), which
   * is every frame the headset's own menu is up. */
  if (xrSyncActions(session, &sync) != XR_SUCCESS)
  {
    openxr_input_release_all();
    return;
  }

  /* A control whose state cannot be had, or that is not active, is not
   * held. (A stick or a trigger kept its last value before.) */
  for (i = 0; i < XR_COUNT; i++)
  {
    XrActionStateGetInfo info = {XR_TYPE_ACTION_STATE_GET_INFO};
    XrActionStateBoolean state = {XR_TYPE_ACTION_STATE_BOOLEAN};

    if (i == XR_LTRIGGER ||
        i == XR_RTRIGGER ||
        i == XR_LSTICK ||
        i == XR_RSTICK)
        continue;

    info.action = openxr_actions[i];

    if (     xrGetActionStateBoolean(session, &info, &state) == XR_SUCCESS
          && state.isActive && state.currentState)
      buttons |= (1u << i);
  }

  for (i = 0; i < 2; i++)
  {
    XrActionStateGetInfo info = {XR_TYPE_ACTION_STATE_GET_INFO};
    XrActionStateFloat state = {XR_TYPE_ACTION_STATE_FLOAT};
    info.action = openxr_actions[XR_LTRIGGER + i];

    if (     xrGetActionStateFloat(session, &info, &state) == XR_SUCCESS
          && state.isActive)
      trigger[i] = openxr_axis_value(state.currentState);
  }
  for (i = 0; i < 2; i++)
  {
    XrActionStateGetInfo info = {XR_TYPE_ACTION_STATE_GET_INFO};
    XrActionStateVector2f state = {XR_TYPE_ACTION_STATE_VECTOR2F};
    info.action = openxr_actions[XR_LSTICK + i];
    if (     xrGetActionStateVector2f(session, &info, &state) == XR_SUCCESS
          && state.isActive)
      stick[i] = XR_PAIR(openxr_axis_value(state.currentState.x),
                         openxr_axis_value(state.currentState.y));
  }

  retro_atomic_store_relaxed_int(&openxr_buttons, (int)buttons);
  retro_atomic_store_relaxed_int(&openxr_sticks[0], stick[0]);
  retro_atomic_store_relaxed_int(&openxr_sticks[1], stick[1]);
  retro_atomic_store_relaxed_int(&openxr_triggers,
        XR_PAIR(trigger[0], trigger[1]));
  openxr_state_live = true;

  {
    bool menu_down = (buttons >> XR_MENU) & 1;
    uint64_t now = cpu_features_get_time_usec();

    /* Held, as a state: set from a second into the press until the
     * button comes up. The reader looks, and takes nothing away. */
    if (menu_down && !openxr_menu_was_down)
        openxr_menu_down_time = now;
    retro_atomic_store_relaxed_int(&openxr_menu_long_press,
          (menu_down && now - openxr_menu_down_time >= 1000000) ? 1 : 0);

    openxr_menu_was_down = menu_down;
  }
}

void openxr_input_deinit(void)
{
  if (openxr_action_set != XR_NULL_HANDLE)
    xrDestroyActionSet(openxr_action_set);

  openxr_action_set = XR_NULL_HANDLE;
  retro_atomic_store_relaxed_int(&openxr_attached_flag, 0);
  /* and nothing of the session that is gone is still held: its last
   * buttons and axes stayed readable after this before */
  openxr_state_live = true;
  openxr_input_release_all();
  openxr_menu_down_time = 0;
}

bool openxr_input_session_active(void)
{
  return retro_atomic_load_relaxed_int(&openxr_attached_flag) != 0;
}

#ifdef ANDROID
bool openxr_input_button(unsigned button)
{
  int idx;
  switch (button)
  {
    case AKEYCODE_BUTTON_X:      idx = XR_X;      break;
    case AKEYCODE_BUTTON_Y:      idx = XR_Y;      break;
    case AKEYCODE_BUTTON_A:      idx = XR_A;      break;
    case AKEYCODE_BUTTON_B:      idx = XR_B;      break;
    case AKEYCODE_BUTTON_L1:     idx = XR_LGRIP;  break;
    case AKEYCODE_BUTTON_R1:     idx = XR_RGRIP;  break;
    case AKEYCODE_BUTTON_THUMBL: idx = XR_LCLICK; break;
    case AKEYCODE_BUTTON_THUMBR: idx = XR_RCLICK; break;
    case AKEYCODE_BACK:          idx = XR_MENU;   break;
    default:
      return false;
  }
  return ((unsigned)retro_atomic_load_relaxed_int(&openxr_buttons) >> idx) & 1;
}

int16_t openxr_input_axis(unsigned axis)
{
  int v;
  /* a stick's x then its y, the left stick then the right; the
   * triggers after them, left then right */
  if (axis < 4)
    v = retro_atomic_load_relaxed_int(&openxr_sticks[axis >> 1]);
  else if (axis == 6 || axis == 7)
    v = retro_atomic_load_relaxed_int(&openxr_triggers);
  else
    return 0;
  return (axis & 1) ? XR_PAIR_HI(v) : XR_PAIR_LO(v);
}

/* The menu button has been held a second, and still is. The frontend
 * reads it as a hotkey that is down, and its hotkeys act once a press:
 * so this says "down" for as long as it is, on every read. It took the
 * flag away as it read it before, and the next sync put it back - a
 * frame read between two syncs saw the button let go and pressed
 * again, and the menu toggled a second time. */
bool openxr_input_menu_long_press(void)
{
  return retro_atomic_load_relaxed_int(&openxr_menu_long_press) != 0;
}
#endif
