#include "openxr_input.h"
#include <features/features_cpu.h>
#include <math.h>
#ifdef ANDROID
#include <android/keycodes.h>
#endif
#include <compat/strl.h>

#include "../../tasks/tasks_internal.h"

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
static bool openxr_buttons[XR_COUNT];
static int16_t openxr_axes[6];
static bool openxr_attached;

/* long press menu button for F1 */
static bool openxr_menu_was_down;
static bool openxr_menu_long_press;
static uint64_t openxr_menu_down_time;

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
  openxr_attached = xrAttachSessionActionSets(session, &info) == XR_SUCCESS;

  if (openxr_attached)
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

  return openxr_attached;
}

void openxr_input_sync(XrSession session)
{
  XrActiveActionSet active = {openxr_action_set, XR_NULL_PATH};
  XrActionsSyncInfo sync = {XR_TYPE_ACTIONS_SYNC_INFO};
  unsigned i;

  if (!openxr_attached)
    return;

  sync.countActiveActionSets = 1;
  sync.activeActionSets = &active;

  if (xrSyncActions(session, &sync) != XR_SUCCESS)
    return;

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

    if (xrGetActionStateBoolean(session, &info, &state) == XR_SUCCESS)
      openxr_buttons[i] = state.isActive && state.currentState;
    else
      openxr_buttons[i] = false;
  }

  for (i = 0; i < 2; i++)
  {
    XrActionStateGetInfo info = {XR_TYPE_ACTION_STATE_GET_INFO};
    XrActionStateFloat state = {XR_TYPE_ACTION_STATE_FLOAT};
    info.action = openxr_actions[XR_LTRIGGER + i];

    if (xrGetActionStateFloat(session, &info, &state) == XR_SUCCESS)
      openxr_axes[4 + i] = state.isActive ? (int16_t)(state.currentState * 32767.0f) : 0;
  }
  for (i = 0; i < 2; i++)
  {
    XrActionStateGetInfo info = {XR_TYPE_ACTION_STATE_GET_INFO};
    XrActionStateVector2f state = {XR_TYPE_ACTION_STATE_VECTOR2F};
    info.action = openxr_actions[XR_LSTICK + i];
    if (xrGetActionStateVector2f(session, &info, &state) == XR_SUCCESS)
    {
      if (state.isActive)
      {
          openxr_axes[i * 2] =
            (int16_t)(state.currentState.x * 32767.0f);
          openxr_axes[i * 2 + 1] =
            (int16_t)(state.currentState.y * 32767.0f);
      }
      else
      {
          openxr_axes[i * 2] = 0;
          openxr_axes[i * 2 + 1] = 0;
      }
    }
  }

  {
    bool menu_down = openxr_buttons[XR_MENU];
    uint64_t now = cpu_features_get_time_usec();

    if (menu_down && !openxr_menu_was_down)
    {
        openxr_menu_down_time = now;
        openxr_menu_long_press = false;
    }
    else if (menu_down && !openxr_menu_long_press)
    {
        if (now - openxr_menu_down_time >= 1000000)
        {
          openxr_menu_long_press = true;
        }
    }

    openxr_menu_was_down = menu_down;
  }
}

void openxr_input_deinit(void)
{
  if (openxr_action_set != XR_NULL_HANDLE)
    xrDestroyActionSet(openxr_action_set);

  openxr_action_set = XR_NULL_HANDLE;
  openxr_attached = false;
  openxr_menu_was_down = false;
  openxr_menu_long_press = false;
  openxr_menu_down_time = 0;
}

#ifdef ANDROID
bool android_vk_openxr_button(unsigned button)
{
  switch (button)
  {
    case AKEYCODE_BUTTON_X:
      return openxr_buttons[XR_X];
    case AKEYCODE_BUTTON_Y:
      return openxr_buttons[XR_Y];
    case AKEYCODE_BUTTON_A:
      return openxr_buttons[XR_A];
    case AKEYCODE_BUTTON_B:
      return openxr_buttons[XR_B];
    case AKEYCODE_BUTTON_L1:
      return openxr_buttons[XR_LGRIP];
    case AKEYCODE_BUTTON_R1:
      return openxr_buttons[XR_RGRIP];
    case AKEYCODE_BUTTON_THUMBL:
      return openxr_buttons[XR_LCLICK];
    case AKEYCODE_BUTTON_THUMBR:
      return openxr_buttons[XR_RCLICK];
    case AKEYCODE_BACK:
      return openxr_buttons[XR_MENU];
  }

  return false;
}

int16_t android_vk_openxr_axis(unsigned axis)
{
  int32_t v;
  if (axis < 4)
    v = openxr_axes[axis];
  else if (axis == 6 || axis == 7)
    return openxr_axes[axis - 2];
  else
    return 0;

  if (v >  32767) v =  32767;
  if (v < -32767) v = -32767;
  return (int16_t)v;
}

bool android_vk_openxr_menu_long_press(void)
{
  if (!openxr_menu_long_press)
    return false;

  openxr_menu_long_press = false;
  return true;
}
#endif
