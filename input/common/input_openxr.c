/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <boolean.h>
#include <compat/strl.h>
#include <string/stdstring.h>
#include <retro_miscellaneous.h>
#include <retro_atomic.h>
#include <features/features_cpu.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "input_openxr.h"

#include "../input_defines.h"
#include "../input_driver.h"
#include "../../runloop.h"
#include "../../tasks/tasks_internal.h"
#include "../../gfx/video_views.h"
#include "../../gfx/common/openxr_frame.h"
#include "../../gfx/video_driver.h"
#include "../../msg_hash.h"
#include "../../verbosity.h"

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#define INPUT_OPENXR_HANDS     2
#define INPUT_OPENXR_MAX_BINDS 48
/* With XR_EXT_palm_pose, every profile gets both hands' palm too. */
#define INPUT_OPENXR_PALM_BINDS 2
/* A steady rumble is applied again this often (us), each time for
 * INPUT_OPENXR_HAPTIC_DURATION (ns). */
#define INPUT_OPENXR_HAPTIC_REFRESH  500000
#define INPUT_OPENXR_HAPTIC_DURATION 1000000000
/* A snap's buzz as it takes hold: its strength and length (ns). */
#define INPUT_OPENXR_SNAP_TICK_AMP 0.5f
#define INPUT_OPENXR_SNAP_TICK_NS  20000000
/* A hinge's as it takes hold, longer and stronger; snaps' due while it
 * plays are skipped. */
#define INPUT_OPENXR_HINGE_BUZZ_AMP 0.8f
#define INPUT_OPENXR_HINGE_BUZZ_NS  150000000
/* How long a held screen's hinge stays marked once out of where a
 * let-go would settle in it (us): a shaking hand never flickers it. */
#define INPUT_OPENXR_HINGE_DEBOUNCE_US 150000
/* The most each poll seen out adds to that (us): a stall saw no hand. */
#define INPUT_OPENXR_HINGE_OUT_STEP_US 50000
/* Half a stick's travel. */
#define INPUT_OPENXR_YIELD_STICK   0x4000
/* Polls in a row with neither of edit mode's grab nor its menu bound
 * that end it: a runtime can read a binding inactive for a sync around
 * a controller change. */
#define INPUT_OPENXR_EDIT_UNBOUND  3
/* Touch's one menu button opens the RetroArch menu once held this long
 * (us). */
#define INPUT_OPENXR_MENU_HOLD     1000000
/* Edit mode's longest stick step (s): a stalled poll does not fling the
 * item. */
#define INPUT_OPENXR_EDIT_MAX_DT   0.1
/* A let-go puts the item where, turned and as large as, it showed this
 * long ago (us), before the grip's twitch. The trail keeps an entry a
 * frame, INPUT_OPENXR_EDIT_TRAIL_US apart at least; the rewind stops at
 * a stall, entries more than INPUT_OPENXR_EDIT_GAP frames apart. */
#define INPUT_OPENXR_EDIT_REWIND_US 80000
#define INPUT_OPENXR_EDIT_TRAIL     24
#define INPUT_OPENXR_EDIT_TRAIL_US  10000
#define INPUT_OPENXR_EDIT_GAP       2.5

enum input_openxr_set
{
   INPUT_OPENXR_SET_COMBINED = 0,
   INPUT_OPENXR_SET_SEPARATE,
   INPUT_OPENXR_SET_POINTER,
   INPUT_OPENXR_SET_EDIT,
   INPUT_OPENXR_SETS
};

/* Combined's first sixteen are the RetroPad's ids, in order. */
enum input_openxr_action
{
   IXA_C_B = 0,
   IXA_C_Y,
   IXA_C_SELECT,
   IXA_C_START,
   IXA_C_UP,
   IXA_C_DOWN,
   IXA_C_LEFT,
   IXA_C_RIGHT,
   IXA_C_A,
   IXA_C_X,
   IXA_C_L,
   IXA_C_R,
   IXA_C_L2,
   IXA_C_R2,
   IXA_C_L3,
   IXA_C_R3,
   IXA_C_LSTICK,
   IXA_C_RSTICK,
   IXA_C_MENU,
   IXA_C_RECENTER,
   IXA_C_LASER,
   IXA_C_VIEW,
   IXA_C_START_MENU,
   IXA_C_EDIT,
   IXA_S_B,
   IXA_S_A,
   IXA_S_R,
   IXA_S_R2,
   IXA_S_START,
   IXA_S_STICK,
   IXA_S_MENU,
   IXA_S_RECENTER,
   IXA_S_LASER,
   IXA_S_VIEW,
   IXA_S_EDIT,
   IXA_AIM,
   IXA_GRIP,
   IXA_PALM,
   IXA_RUMBLE,
   IXA_E_GRAB,
   IXA_E_TILT,
   IXA_E_STICK,
   IXA_E_RESET,
   IXA_E_MENU,
   IXA_E_EDIT,
   IXA_E_NEXT,
   IXA_E_PREV,
   IXA_COUNT
};

typedef struct input_openxr_action_def
{
   unsigned set;
   const char *name;
   const char *label;
   XrActionType type;
   bool hands;
} input_openxr_action_def_t;

/* Named after the RetroPad: the runtime's binding UI shows these, and
 * keeps the user's rebinds by name. */
static const input_openxr_action_def_t input_openxr_actions[IXA_COUNT] = {
   { INPUT_OPENXR_SET_COMBINED, "b",           "B",              XR_ACTION_TYPE_BOOLEAN_INPUT,    false },
   { INPUT_OPENXR_SET_COMBINED, "y",           "Y",              XR_ACTION_TYPE_BOOLEAN_INPUT,    false },
   { INPUT_OPENXR_SET_COMBINED, "select",      "Select",         XR_ACTION_TYPE_BOOLEAN_INPUT,    false },
   { INPUT_OPENXR_SET_COMBINED, "start",       "Start",          XR_ACTION_TYPE_BOOLEAN_INPUT,    false },
   { INPUT_OPENXR_SET_COMBINED, "dpad_up",     "D-Pad Up",       XR_ACTION_TYPE_BOOLEAN_INPUT,    false },
   { INPUT_OPENXR_SET_COMBINED, "dpad_down",   "D-Pad Down",     XR_ACTION_TYPE_BOOLEAN_INPUT,    false },
   { INPUT_OPENXR_SET_COMBINED, "dpad_left",   "D-Pad Left",     XR_ACTION_TYPE_BOOLEAN_INPUT,    false },
   { INPUT_OPENXR_SET_COMBINED, "dpad_right",  "D-Pad Right",    XR_ACTION_TYPE_BOOLEAN_INPUT,    false },
   { INPUT_OPENXR_SET_COMBINED, "a",           "A",              XR_ACTION_TYPE_BOOLEAN_INPUT,    false },
   { INPUT_OPENXR_SET_COMBINED, "x",           "X",              XR_ACTION_TYPE_BOOLEAN_INPUT,    false },
   { INPUT_OPENXR_SET_COMBINED, "l",           "L",              XR_ACTION_TYPE_BOOLEAN_INPUT,    false },
   { INPUT_OPENXR_SET_COMBINED, "r",           "R",              XR_ACTION_TYPE_BOOLEAN_INPUT,    false },
   { INPUT_OPENXR_SET_COMBINED, "l2",          "L2",             XR_ACTION_TYPE_FLOAT_INPUT,      false },
   { INPUT_OPENXR_SET_COMBINED, "r2",          "R2",             XR_ACTION_TYPE_FLOAT_INPUT,      false },
   { INPUT_OPENXR_SET_COMBINED, "l3",          "L3",             XR_ACTION_TYPE_BOOLEAN_INPUT,    false },
   { INPUT_OPENXR_SET_COMBINED, "r3",          "R3",             XR_ACTION_TYPE_BOOLEAN_INPUT,    false },
   { INPUT_OPENXR_SET_COMBINED, "left_stick",  "Left Stick",     XR_ACTION_TYPE_VECTOR2F_INPUT,   false },
   { INPUT_OPENXR_SET_COMBINED, "right_stick", "Right Stick",    XR_ACTION_TYPE_VECTOR2F_INPUT,   false },
   { INPUT_OPENXR_SET_COMBINED, "menu",        "RetroArch Menu", XR_ACTION_TYPE_BOOLEAN_INPUT,    false },
   { INPUT_OPENXR_SET_COMBINED, "recenter",    "Recenter",       XR_ACTION_TYPE_BOOLEAN_INPUT,    false },
   { INPUT_OPENXR_SET_COMBINED, "laser",       "Laser Pointer",  XR_ACTION_TYPE_BOOLEAN_INPUT,    false },
   { INPUT_OPENXR_SET_COMBINED, "view",        "Headset View",   XR_ACTION_TYPE_BOOLEAN_INPUT,    false },
   { INPUT_OPENXR_SET_COMBINED, "start_menu",  "Start (Held: RetroArch Menu)", XR_ACTION_TYPE_BOOLEAN_INPUT, false },
   { INPUT_OPENXR_SET_COMBINED, "edit_screens", "Edit Screens",  XR_ACTION_TYPE_BOOLEAN_INPUT,    false },
   { INPUT_OPENXR_SET_SEPARATE, "b",           "B",              XR_ACTION_TYPE_BOOLEAN_INPUT,    true  },
   { INPUT_OPENXR_SET_SEPARATE, "a",           "A",              XR_ACTION_TYPE_BOOLEAN_INPUT,    true  },
   { INPUT_OPENXR_SET_SEPARATE, "r",           "R",              XR_ACTION_TYPE_BOOLEAN_INPUT,    true  },
   { INPUT_OPENXR_SET_SEPARATE, "r2",          "R2",             XR_ACTION_TYPE_FLOAT_INPUT,      true  },
   { INPUT_OPENXR_SET_SEPARATE, "start",       "Start",          XR_ACTION_TYPE_BOOLEAN_INPUT,    true  },
   { INPUT_OPENXR_SET_SEPARATE, "stick",       "Stick",          XR_ACTION_TYPE_VECTOR2F_INPUT,   true  },
   { INPUT_OPENXR_SET_SEPARATE, "menu",        "RetroArch Menu", XR_ACTION_TYPE_BOOLEAN_INPUT,    false },
   { INPUT_OPENXR_SET_SEPARATE, "recenter",    "Recenter",       XR_ACTION_TYPE_BOOLEAN_INPUT,    false },
   { INPUT_OPENXR_SET_SEPARATE, "laser",       "Laser Pointer",  XR_ACTION_TYPE_BOOLEAN_INPUT,    false },
   { INPUT_OPENXR_SET_SEPARATE, "view",        "Headset View",   XR_ACTION_TYPE_BOOLEAN_INPUT,    false },
   { INPUT_OPENXR_SET_SEPARATE, "edit_screens", "Edit Screens",  XR_ACTION_TYPE_BOOLEAN_INPUT,    false },
   { INPUT_OPENXR_SET_POINTER,  "aim",         "Aim",            XR_ACTION_TYPE_POSE_INPUT,       true  },
   { INPUT_OPENXR_SET_POINTER,  "grip",        "Grip",           XR_ACTION_TYPE_POSE_INPUT,       true  },
   { INPUT_OPENXR_SET_POINTER,  "palm",        "Palm",           XR_ACTION_TYPE_POSE_INPUT,       true  },
   { INPUT_OPENXR_SET_POINTER,  "rumble",      "Rumble",         XR_ACTION_TYPE_VIBRATION_OUTPUT, true  },
   { INPUT_OPENXR_SET_EDIT,     "grab",        "Grab",           XR_ACTION_TYPE_BOOLEAN_INPUT,    true  },
   { INPUT_OPENXR_SET_EDIT,     "tilt",        "Tilt",           XR_ACTION_TYPE_BOOLEAN_INPUT,    true  },
   { INPUT_OPENXR_SET_EDIT,     "stick",       "Distance and Size", XR_ACTION_TYPE_VECTOR2F_INPUT, true },
   { INPUT_OPENXR_SET_EDIT,     "reset",       "Reset",          XR_ACTION_TYPE_BOOLEAN_INPUT,    true  },
   { INPUT_OPENXR_SET_EDIT,     "menu",        "RetroArch Menu", XR_ACTION_TYPE_BOOLEAN_INPUT,    false },
   { INPUT_OPENXR_SET_EDIT,     "edit_screens", "Edit Screens",  XR_ACTION_TYPE_BOOLEAN_INPUT,    false },
   { INPUT_OPENXR_SET_EDIT,     "select_next", "Select Next",    XR_ACTION_TYPE_BOOLEAN_INPUT,    true  },
   { INPUT_OPENXR_SET_EDIT,     "select_previous", "Select Previous", XR_ACTION_TYPE_BOOLEAN_INPUT, true }
};

static const char *const input_openxr_set_names[INPUT_OPENXR_SETS] = {
   "combined", "separate", "pointer", "edit" };
static const char *const input_openxr_set_labels[INPUT_OPENXR_SETS] = {
   "Combined Controllers", "Separate Controllers", "Laser and Rumble",
   "Edit Screens" };

typedef struct input_openxr_bind
{
   unsigned action;
   const char *path;
} input_openxr_bind_t;

#define IXB_L(a, p)    { (a), "/user/hand/left/input/" p }
#define IXB_R(a, p)    { (a), "/user/hand/right/input/" p }
#define IXB_BOTH(a, p) IXB_L(a, p), IXB_R(a, p)
#define IXB_POINTER \
   IXB_BOTH(IXA_AIM, "aim/pose"), \
   IXB_BOTH(IXA_GRIP, "grip/pose"), \
   { IXA_RUMBLE, "/user/hand/left/output/haptic" }, \
   { IXA_RUMBLE, "/user/hand/right/output/haptic" }

/* Valve Index: no menu buttons, so Start and the RetroArch menu are
 * the trackpads' press. */
static const input_openxr_bind_t input_openxr_index[] = {
   IXB_R(IXA_C_B,      "a/click"),
   IXB_R(IXA_C_A,      "b/click"),
   IXB_L(IXA_C_Y,      "a/click"),
   IXB_L(IXA_C_X,      "b/click"),
   IXB_L(IXA_C_L,      "squeeze/value"),
   IXB_R(IXA_C_R,      "squeeze/value"),
   IXB_L(IXA_C_L2,     "trigger/value"),
   IXB_R(IXA_C_R2,     "trigger/value"),
   IXB_L(IXA_C_L3,     "thumbstick/click"),
   IXB_R(IXA_C_R3,     "thumbstick/click"),
   IXB_L(IXA_C_LSTICK, "thumbstick"),
   IXB_R(IXA_C_RSTICK, "thumbstick"),
   IXB_R(IXA_C_START,  "trackpad/force"),
   IXB_L(IXA_C_MENU,   "trackpad/force"),
   IXB_BOTH(IXA_S_B,     "a/click"),
   IXB_BOTH(IXA_S_A,     "b/click"),
   IXB_BOTH(IXA_S_R,     "squeeze/value"),
   IXB_BOTH(IXA_S_R2,    "trigger/value"),
   IXB_BOTH(IXA_S_START, "thumbstick/click"),
   IXB_BOTH(IXA_S_STICK, "thumbstick"),
   IXB_L(IXA_S_MENU,     "trackpad/force"),
   IXB_POINTER
};

/* Oculus Touch: X and Y on the left, A and B on the right, and one menu
 * button (left): Start, and the RetroArch menu once held a second. */
static const input_openxr_bind_t input_openxr_touch[] = {
   IXB_R(IXA_C_B,      "a/click"),
   IXB_R(IXA_C_A,      "b/click"),
   IXB_L(IXA_C_Y,      "x/click"),
   IXB_L(IXA_C_X,      "y/click"),
   IXB_L(IXA_C_L,      "squeeze/value"),
   IXB_R(IXA_C_R,      "squeeze/value"),
   IXB_L(IXA_C_L2,     "trigger/value"),
   IXB_R(IXA_C_R2,     "trigger/value"),
   IXB_L(IXA_C_L3,     "thumbstick/click"),
   IXB_R(IXA_C_R3,     "thumbstick/click"),
   IXB_L(IXA_C_LSTICK, "thumbstick"),
   IXB_R(IXA_C_RSTICK, "thumbstick"),
   IXB_L(IXA_C_START_MENU, "menu/click"),
   IXB_L(IXA_S_B,      "x/click"),
   IXB_R(IXA_S_B,      "a/click"),
   IXB_L(IXA_S_A,      "y/click"),
   IXB_R(IXA_S_A,      "b/click"),
   IXB_BOTH(IXA_S_R,     "squeeze/value"),
   IXB_BOTH(IXA_S_R2,    "trigger/value"),
   IXB_BOTH(IXA_S_START, "thumbstick/click"),
   IXB_BOTH(IXA_S_STICK, "thumbstick"),
   IXB_L(IXA_S_MENU,     "menu/click"),
   IXB_POINTER
};

/* Steam Frame: A, B, X and Y on the right, a D-pad on the left; View
 * is Select, Menu is Start, and the right stick's click is the
 * RetroArch menu in place of R3. The grips stay free. */
static const input_openxr_bind_t input_openxr_frame[] = {
   IXB_R(IXA_C_B,      "a/click"),
   IXB_R(IXA_C_A,      "b/click"),
   IXB_R(IXA_C_Y,      "x/click"),
   IXB_R(IXA_C_X,      "y/click"),
   IXB_L(IXA_C_UP,     "dpad_up/click"),
   IXB_L(IXA_C_DOWN,   "dpad_down/click"),
   IXB_L(IXA_C_LEFT,   "dpad_left/click"),
   IXB_L(IXA_C_RIGHT,  "dpad_right/click"),
   IXB_L(IXA_C_SELECT, "view/click"),
   IXB_R(IXA_C_START,  "menu/click"),
   IXB_L(IXA_C_L,      "bumper/click"),
   IXB_R(IXA_C_R,      "bumper/click"),
   IXB_L(IXA_C_L2,     "trigger/value"),
   IXB_R(IXA_C_R2,     "trigger/value"),
   IXB_L(IXA_C_L3,     "thumbstick/click"),
   IXB_R(IXA_C_MENU,   "thumbstick/click"),
   IXB_L(IXA_C_LSTICK, "thumbstick"),
   IXB_R(IXA_C_RSTICK, "thumbstick"),
   IXB_L(IXA_S_B,      "dpad_down/click"),
   IXB_R(IXA_S_B,      "a/click"),
   IXB_L(IXA_S_A,      "dpad_right/click"),
   IXB_R(IXA_S_A,      "b/click"),
   IXB_BOTH(IXA_S_R,     "bumper/click"),
   IXB_BOTH(IXA_S_R2,    "trigger/value"),
   IXB_BOTH(IXA_S_START, "thumbstick/click"),
   IXB_BOTH(IXA_S_STICK, "thumbstick"),
   IXB_L(IXA_S_MENU,     "view/click"),
   IXB_POINTER
};

/* HTC Vive: the trackpads are the sticks. */
static const input_openxr_bind_t input_openxr_vive[] = {
   IXB_L(IXA_C_L,      "squeeze/click"),
   IXB_R(IXA_C_R,      "squeeze/click"),
   IXB_L(IXA_C_L2,     "trigger/value"),
   IXB_R(IXA_C_R2,     "trigger/value"),
   IXB_L(IXA_C_L3,     "trackpad/click"),
   IXB_R(IXA_C_R3,     "trackpad/click"),
   IXB_L(IXA_C_LSTICK, "trackpad"),
   IXB_R(IXA_C_RSTICK, "trackpad"),
   IXB_R(IXA_C_START,  "menu/click"),
   IXB_L(IXA_C_MENU,   "menu/click"),
   IXB_BOTH(IXA_S_R,     "squeeze/click"),
   IXB_BOTH(IXA_S_R2,    "trigger/value"),
   IXB_BOTH(IXA_S_START, "trackpad/click"),
   IXB_BOTH(IXA_S_STICK, "trackpad"),
   IXB_L(IXA_S_MENU,     "menu/click"),
   IXB_POINTER
};

static const input_openxr_bind_t input_openxr_simple[] = {
   IXB_L(IXA_C_L2,     "select/click"),
   IXB_R(IXA_C_R2,     "select/click"),
   IXB_R(IXA_C_START,  "menu/click"),
   IXB_L(IXA_C_MENU,   "menu/click"),
   IXB_BOTH(IXA_S_R2,    "select/click"),
   IXB_BOTH(IXA_S_START, "menu/click"),
   IXB_POINTER
};

/* Edit mode's: the grip grabs, the trigger tilts, the stick (Vive: the
 * trackpad) pushes and resizes, the main face button resets, the menu
 * button opens the RetroArch menu, which ends it, and spare buttons
 * choose the next or previous item. Edit Screens is the user's to
 * bind. */
static const input_openxr_bind_t input_openxr_index_edit[] = {
   IXB_BOTH(IXA_E_GRAB,  "squeeze/value"),
   IXB_BOTH(IXA_E_TILT,  "trigger/value"),
   IXB_BOTH(IXA_E_STICK, "thumbstick"),
   IXB_BOTH(IXA_E_RESET, "a/click"),
   IXB_L(IXA_E_MENU,     "trackpad/force"),
   IXB_R(IXA_E_NEXT,     "b/click"),
   IXB_L(IXA_E_PREV,     "b/click")
};

static const input_openxr_bind_t input_openxr_touch_edit[] = {
   IXB_BOTH(IXA_E_GRAB,  "squeeze/value"),
   IXB_BOTH(IXA_E_TILT,  "trigger/value"),
   IXB_BOTH(IXA_E_STICK, "thumbstick"),
   IXB_L(IXA_E_RESET,    "x/click"),
   IXB_R(IXA_E_RESET,    "a/click"),
   IXB_L(IXA_E_MENU,     "menu/click"),
   IXB_R(IXA_E_NEXT,     "b/click"),
   IXB_L(IXA_E_PREV,     "y/click")
};

/* The left hand has no face buttons: its D-pad resets (down) and
 * chooses (right, left). */
static const input_openxr_bind_t input_openxr_frame_edit[] = {
   IXB_BOTH(IXA_E_GRAB,  "squeeze/value"),
   IXB_BOTH(IXA_E_TILT,  "trigger/value"),
   IXB_BOTH(IXA_E_STICK, "thumbstick"),
   IXB_L(IXA_E_RESET,    "dpad_down/click"),
   IXB_R(IXA_E_RESET,    "a/click"),
   IXB_R(IXA_E_MENU,     "thumbstick/click"),
   IXB_L(IXA_E_NEXT,     "dpad_right/click"),
   IXB_L(IXA_E_PREV,     "dpad_left/click")
};

static const input_openxr_bind_t input_openxr_vive_edit[] = {
   IXB_BOTH(IXA_E_GRAB,  "squeeze/click"),
   IXB_BOTH(IXA_E_TILT,  "trigger/value"),
   IXB_BOTH(IXA_E_STICK, "trackpad"),
   IXB_BOTH(IXA_E_RESET, "trackpad/click"),
   IXB_L(IXA_E_MENU,     "menu/click"),
   IXB_R(IXA_E_NEXT,     "menu/click")
};

static const input_openxr_bind_t input_openxr_simple_edit[] = {
   IXB_BOTH(IXA_E_GRAB,  "select/click"),
   IXB_L(IXA_E_MENU,     "menu/click")
};

typedef struct input_openxr_profile
{
   const char *path;
   const input_openxr_bind_t *binds;
   const input_openxr_bind_t *edit;
   unsigned count;
   unsigned edit_count;
   /* Only with XR_VALVE_frame_controller_interaction. */
   bool frame;
} input_openxr_profile_t;

static const input_openxr_profile_t input_openxr_profiles[] = {
   { "/interaction_profiles/valve/index_controller",
      input_openxr_index,  input_openxr_index_edit,
      ARRAY_SIZE(input_openxr_index),  ARRAY_SIZE(input_openxr_index_edit),
      false },
   { "/interaction_profiles/oculus/touch_controller",
      input_openxr_touch,  input_openxr_touch_edit,
      ARRAY_SIZE(input_openxr_touch),  ARRAY_SIZE(input_openxr_touch_edit),
      false },
   { "/interaction_profiles/valve/frame_controller_valve",
      input_openxr_frame,  input_openxr_frame_edit,
      ARRAY_SIZE(input_openxr_frame),  ARRAY_SIZE(input_openxr_frame_edit),
      true  },
   { "/interaction_profiles/htc/vive_controller",
      input_openxr_vive,   input_openxr_vive_edit,
      ARRAY_SIZE(input_openxr_vive),   ARRAY_SIZE(input_openxr_vive_edit),
      false },
   { "/interaction_profiles/khr/simple_controller",
      input_openxr_simple, input_openxr_simple_edit,
      ARRAY_SIZE(input_openxr_simple), ARRAY_SIZE(input_openxr_simple_edit),
      false }
};

typedef struct input_openxr_pad
{
   uint16_t buttons;     /* 1 << RETRO_DEVICE_ID_JOYPAD_* */
   int16_t analog[4];    /* left x, left y, right x, right y */
   int16_t trigger[2];   /* analog L2, R2 */
} input_openxr_pad_t;

/* A hand's trigger, and the pad and L2 (0) or R2 (1) it feeds. */
typedef struct input_openxr_trigger
{
   float value;
   unsigned pad;
   unsigned slot;
} input_openxr_trigger_t;

/* What a trigger does from its pull until it is let go. */
enum input_openxr_role
{
   INPUT_OPENXR_ROLE_UP = 0,
   INPUT_OPENXR_ROLE_PAD,   /* L2 or R2 */
   INPUT_OPENXR_ROLE_TOUCH, /* the core's pointer and gun */
   INPUT_OPENXR_ROLE_MENU,  /* the menu's press */
   INPUT_OPENXR_ROLE_SPENT  /* nothing */
};

/* A hand's Edit Smoothing, the XR thread's: its filter, the frame and
 * space it was last stepped for, what that gave, and the recenters it
 * had seen. */
typedef struct input_openxr_smooth
{
   video_xr_smooth_t filter;
   video_xr_pose_t pose;
   XrTime time;
   XrSpace space;
   int recenters;
   bool tracked;
} input_openxr_smooth_t;

typedef struct input_openxr
{
   /* Written by the session hooks, while neither the poll nor the XR
    * thread runs. */
   openxr_frame_t *frame;
   XrInstance instance;
   XrSession session;
   XrPath hands[INPUT_OPENXR_HANDS];
   XrActionSet sets[INPUT_OPENXR_SETS];
   XrAction actions[IXA_COUNT];
   XrSpace aim[INPUT_OPENXR_HANDS];
   XrSpace grip[INPUT_OPENXR_HANDS];
   XrSpace palm[INPUT_OPENXR_HANDS];
   XrHandTrackerEXT trackers[INPUT_OPENXR_HANDS];
   /* The profiles that refused edit mode's bindings. */
   XrPath edit_refused[ARRAY_SIZE(input_openxr_profiles)];
   unsigned num_edit_refused;
   PFN_xrStringToPath StringToPath;
   PFN_xrPathToString PathToString;
   PFN_xrCreateActionSet CreateActionSet;
   PFN_xrDestroyActionSet DestroyActionSet;
   PFN_xrCreateAction CreateAction;
   PFN_xrSuggestInteractionProfileBindings SuggestInteractionProfileBindings;
   PFN_xrAttachSessionActionSets AttachSessionActionSets;
   PFN_xrCreateActionSpace CreateActionSpace;
   PFN_xrDestroySpace DestroySpace;
   PFN_xrSyncActions SyncActions;
   PFN_xrGetActionStateBoolean GetActionStateBoolean;
   PFN_xrGetActionStateFloat GetActionStateFloat;
   PFN_xrGetActionStateVector2f GetActionStateVector2f;
   PFN_xrGetActionStatePose GetActionStatePose;
   PFN_xrGetCurrentInteractionProfile GetCurrentInteractionProfile;
   PFN_xrLocateSpace LocateSpace;
   PFN_xrCreateHandTrackerEXT CreateHandTrackerEXT;
   PFN_xrDestroyHandTrackerEXT DestroyHandTrackerEXT;
   PFN_xrLocateHandJointsEXT LocateHandJointsEXT;
   PFN_xrApplyHapticFeedback ApplyHapticFeedback;
   PFN_xrStopHapticFeedback StopHapticFeedback;
   bool palm_pose;
   bool hand_data_source;

   /* Main thread. */
   XrPath profile_path[INPUT_OPENXR_HANDS]; /* profile[]'s, or none */
   char profile[INPUT_OPENXR_HANDS][XR_MAX_PATH_LENGTH];
   input_openxr_pad_t pads[INPUT_OPENXR_PADS];
   input_openxr_trigger_t trig[INPUT_OPENXR_HANDS];
   XrResult sync_result; /* the last xrSyncActions */
   int mode;             /* the controllers mode last logged, or -1 */
   bool focused;
   bool menu_toggle;
   bool recenter;
   bool laser_toggle;
   bool view_toggle;
   bool edit_toggle;
   bool edit_ok;         /* a profile took edit mode's bindings */
   bool edit_warned;     /* its unavailability logged */
   /* Touch's menu button: held since start_menu_since; spent by edit
    * mode's menu press until let go. */
   bool start_menu_held;
   bool start_menu_spent;
   retro_time_t start_menu_since;
   float haptic[INPUT_OPENXR_HANDS];      /* the amplitude last accepted */
   float haptic_attempt[INPUT_OPENXR_HANDS];
   bool haptic_retry[INPUT_OPENXR_HANDS];
   retro_time_t haptic_time[INPUT_OPENXR_HANDS];
   /* The laser, from this poll. */
   video_xr_quad_set_t quads;
   int hit[INPUT_OPENXR_HANDS];         /* a live quad, or -1 */
   float hit_u[INPUT_OPENXR_HANDS];
   float hit_v[INPUT_OPENXR_HANDS];
   bool trig_down[INPUT_OPENXR_HANDS];
   enum input_openxr_role role[INPUT_OPENXR_HANDS];
   int pointer;                         /* the hand that points, or -1 */
   bool ptr_owned;
   bool ptr_on;
   bool ptr_pressed;
   bool gun_pressed;                    /* also off the screens */
   bool touch_on;                       /* the last poll touched */
   int16_t ptr_x;
   int16_t ptr_y;
   bool menu_on;
   bool menu_pressed;
   float menu_u;
   float menu_v;
   /* The laser's yield to the controllers: the last read's buttons and
    * sticks past half. */
   bool yield;
   uint16_t last_buttons[INPUT_OPENXR_PADS];
   unsigned last_sticks[INPUT_OPENXR_PADS];

   /* XR thread: the dots, the beams, edit mode's outlines, the menu's
    * handle, the hinges' lines and the tag, and Edit Smoothing's
    * filters and the aims they published. */
   XrCompositionLayerQuad cursors[INPUT_OPENXR_HANDS];
   XrCompositionLayerQuad beams[INPUT_OPENXR_HANDS];
   XrCompositionLayerQuad outlines[VIDEO_XR_MAX_SLOTS + 1];
   XrCompositionLayerQuad hinge_lines[OPENXR_HINGE_LAYERS];
   XrCompositionLayerQuad tag;
   input_openxr_smooth_t smooth[INPUT_OPENXR_HANDS];
   openxr_aims_t aims;
} input_openxr_t;

static input_openxr_t input_openxr_st;

typedef struct input_openxr_trail
{
   video_xr_pose_t shown;
   float width;
   float height;
   float scale;
   XrTime t; /* its frame's display time */
   /* Where the hand put it, unsnapped, and the hinge a let-go there
    * settles in: its target's slot plus one (0, none) and both edges. */
   video_xr_pose_t hand;
   unsigned char settle_slot;
   unsigned char settle_on;
   unsigned char settle_edge;
} input_openxr_trail_t;

/* A hand in edit mode. */
typedef struct input_openxr_hold
{
   video_xr_grab_t g;
   video_xr_pose_t start;
   video_xr_pose_t aim;
   input_openxr_trail_t trail[INPUT_OPENXR_EDIT_TRAIL];
   unsigned trails;   /* entries held */
   unsigned trail_at; /* the newest */
   retro_time_t polled; /* its last tracked poll (us) */
   video_xr_hinge_t lid;  /* the hinge last swung on, while lidded */
   video_xr_hinge_t mark; /* while buzzed, where a let-go settles */
   retro_time_t buzz_end;
   retro_time_t out;      /* seen out of it since (us), 0 in */
   float scale;
   float scale0;
   float width0;
   float height0;
   float rest[2];
   unsigned armed;
   unsigned steps;
   bool pushed;
   bool hinged;
   bool lidded;
   bool buzzed;
   bool grip;
   bool reset;
} input_openxr_hold_t;

/* Edit Screens, main thread. Kept over sessions: a session's end stops
 * it at the next poll. */
typedef struct input_openxr_edit
{
   retro_time_t last; /* the last poll's, for the stick's step */
   input_openxr_hold_t hand[INPUT_OPENXR_HANDS];
   openxr_grabs_t grabs; /* as the frame loop is given them */
   unsigned seq;      /* grabs let go so far, for the frame snapshot */
   unsigned unbound;  /* polls in a row with its grab and menu unbound */
   int chosen;        /* the item Select Next chose: a slot, -1 none */
   unsigned stuck;    /* hands whose laser has the handle stuck, bits */
   unsigned tag;      /* the hand that pressed Select last, for the tag */
   bool on;
   bool paused;       /* edit mode paused the content */
   bool changed;      /* it wrote a setting */
   bool snap;         /* Snap to Screens */
   float step;        /* Snap to Angles in degrees, 0 Off */
   unsigned char gap; /* Hinge Gap in whole centimetres */
   unsigned smoothing; /* Edit Smoothing, as edit mode started */
   /* Its screens at rest as it last counted their hinges: their slots,
    * as bits, each one's first quad, and whether a count is due. */
   unsigned rest_shown;
   video_xr_quad_t rest_at[VIDEO_XR_MAX_SLOTS];
   bool rests_due;
   bool next;         /* last poll's Select Next and Previous */
   bool prev;
} input_openxr_edit_t;

static input_openxr_edit_t input_openxr_edit;

/* The main thread's laser mode, menu, Laser Beam and edit mode's Edit
 * Smoothing, for the XR thread's dots, beams and aims. */
#define INPUT_OPENXR_LASER_MASK   3
#define INPUT_OPENXR_MENU_OPEN    4
#define INPUT_OPENXR_BEAM_SHIFT   3
#define INPUT_OPENXR_BEAM_MASK    3
#define INPUT_OPENXR_SMOOTH_SHIFT 5
#define INPUT_OPENXR_SMOOTH_MASK  3
static retro_atomic_int_t input_openxr_pointer_config;

/* A core's rumble per pad, strong in the high half and weak in the
 * low, so the two are read together; the pads' slots, each plus one
 * a byte, as the main thread last gave them; and whether a session's
 * controllers exist: asked from any thread. */
static retro_atomic_int_t input_openxr_rumble[INPUT_OPENXR_PADS];
static retro_atomic_int_t input_openxr_rumble_slots;
static retro_atomic_int_t input_openxr_ready;

/* The pads as joypad devices. Main thread; apart from input_openxr_st,
 * which a session's release clears, so a session remade before the
 * next poll keeps them. */
typedef struct input_openxr_devices
{
   int slot[INPUT_OPENXR_PADS];    /* each pad's slot, or -1 */
   int lost[INPUT_OPENXR_PADS];    /* a slot a pad took from it, or -1 */
   unsigned count;                 /* named for 1 (Combined) or 2 pads */
   bool landed[INPUT_OPENXR_PADS]; /* its last connect was applied */
   bool warned;
} input_openxr_devices_t;

static input_openxr_devices_t input_openxr_devs = {
   { -1, -1 }, { -1, -1 }, 0, { false, false }, false };

/* The joypad driver under the devices' slots, and two tables to hand
 * out: a new driver under them gets the other, as the snapshot bridge
 * knows a driver by its table. */
static const input_device_driver_t *input_openxr_inner;
static input_device_driver_t input_openxr_wrap[2];
static unsigned input_openxr_wrap_cur;

#define INPUT_OPENXR_PROC(st, get, inst, name) \
   (XR_SUCCEEDED((get)((inst), "xr" #name, \
         (PFN_xrVoidFunction*)&(st)->name)) && (st)->name)

static void input_openxr_clear(input_openxr_t *st)
{
   memset(st, 0, sizeof(*st));
   /* a trigger held across a session's restart is no new pull: none
    * until it is let go, as after a loss */
   st->trig_down[0] = true;
   st->trig_down[1] = true;
   st->mode    = -1;
   st->hit[0]  = -1;
   st->hit[1]  = -1;
   st->pointer = -1;
}

static bool input_openxr_load(input_openxr_t *st,
      PFN_xrGetInstanceProcAddr get, XrInstance inst)
{
   return INPUT_OPENXR_PROC(st, get, inst, StringToPath)
      && INPUT_OPENXR_PROC(st, get, inst, PathToString)
      && INPUT_OPENXR_PROC(st, get, inst, CreateActionSet)
      && INPUT_OPENXR_PROC(st, get, inst, DestroyActionSet)
      && INPUT_OPENXR_PROC(st, get, inst, CreateAction)
      && INPUT_OPENXR_PROC(st, get, inst, SuggestInteractionProfileBindings)
      && INPUT_OPENXR_PROC(st, get, inst, AttachSessionActionSets)
      && INPUT_OPENXR_PROC(st, get, inst, CreateActionSpace)
      && INPUT_OPENXR_PROC(st, get, inst, DestroySpace)
      && INPUT_OPENXR_PROC(st, get, inst, SyncActions)
      && INPUT_OPENXR_PROC(st, get, inst, GetActionStateBoolean)
      && INPUT_OPENXR_PROC(st, get, inst, GetActionStateFloat)
      && INPUT_OPENXR_PROC(st, get, inst, GetActionStateVector2f)
      && INPUT_OPENXR_PROC(st, get, inst, GetActionStatePose)
      && INPUT_OPENXR_PROC(st, get, inst, GetCurrentInteractionProfile)
      && INPUT_OPENXR_PROC(st, get, inst, LocateSpace)
      && INPUT_OPENXR_PROC(st, get, inst, ApplyHapticFeedback)
      && INPUT_OPENXR_PROC(st, get, inst, StopHapticFeedback);
}

/* One profile's suggestions, edit mode's and the palm's with them. A
 * runtime that refuses either keeps the rest; one without the profile,
 * or that refuses a path, only loses that profile. Returns whether edit
 * mode's were taken. */
static bool input_openxr_suggest(input_openxr_t *st,
      const input_openxr_profile_t *p)
{
   unsigned i;
   unsigned count;
   unsigned palms;
   unsigned edit = p->edit_count;
   XrResult res;
   XrPath profile = XR_NULL_PATH;
   XrActionSuggestedBinding binds[INPUT_OPENXR_MAX_BINDS
      + INPUT_OPENXR_PALM_BINDS];
   XrInteractionProfileSuggestedBinding sb;
   static const char *const palm[INPUT_OPENXR_PALM_BINDS] = {
      "/user/hand/left/input/palm_ext/pose",
      "/user/hand/right/input/palm_ext/pose" };

   if (p->count > INPUT_OPENXR_MAX_BINDS)
      return false;
   /* Past one suggestion's room, the rest go without edit mode's. */
   if (p->count + edit > INPUT_OPENXR_MAX_BINDS)
      edit = 0;
   if (XR_FAILED(res = st->StringToPath(st->instance, p->path, &profile)))
   {
      RARCH_WARN("[OpenXR] No path %s (%d).\n", p->path, (int)res);
      return false;
   }
   for (i = 0; i < p->count + edit; i++)
   {
      const input_openxr_bind_t *b = (i < p->count)
         ? &p->binds[i] : &p->edit[i - p->count];
      binds[i].action = st->actions[b->action];
      if (XR_FAILED(res = st->StringToPath(st->instance, b->path,
                  &binds[i].binding)))
      {
         RARCH_WARN("[OpenXR] No path %s (%d).\n", b->path, (int)res);
         return false;
      }
   }
   count = p->count + edit;
   if (st->palm_pose)
      for (i = 0; i < INPUT_OPENXR_PALM_BINDS; i++)
      {
         binds[count].action = st->actions[IXA_PALM];
         if (XR_SUCCEEDED(st->StringToPath(st->instance, palm[i],
                     &binds[count].binding)))
            count++;
      }
   palms                     = count - p->count - edit;
   sb.type                   = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
   sb.next                   = NULL;
   sb.interactionProfile     = profile;
   sb.countSuggestedBindings = count;
   sb.suggestedBindings      = binds;
   res = st->SuggestInteractionProfileBindings(st->instance, &sb);

   /* Refused: without the palm, then without edit mode's, then the rest
    * on their own. */
   if (XR_FAILED(res) && palms && edit)
   {
      sb.countSuggestedBindings = p->count + edit;
      if (XR_SUCCEEDED(res = st->SuggestInteractionProfileBindings(
                  st->instance, &sb)))
         palms = 0;
   }
   if (XR_FAILED(res) && edit)
   {
      for (i = 0; i < palms; i++)
         binds[p->count + i] = binds[p->count + edit + i];
      sb.countSuggestedBindings = p->count + palms;
      if (XR_SUCCEEDED(res = st->SuggestInteractionProfileBindings(
                  st->instance, &sb)))
         edit = 0;
   }
   if (XR_FAILED(res) && palms)
   {
      sb.countSuggestedBindings = p->count;
      res = st->SuggestInteractionProfileBindings(st->instance, &sb);
      edit  = 0;
      palms = 0;
   }
   if (XR_FAILED(res))
   {
      RARCH_WARN("[OpenXR] Bindings refused for %s (%d).\n", p->path,
            (int)res);
      return false;
   }
   if (edit < p->edit_count)
   {
      RARCH_WARN("[OpenXR] Edit Screens bindings refused for %s.\n",
            p->path);
      st->edit_refused[st->num_edit_refused++] = profile;
   }
   RARCH_LOG("[OpenXR] Bindings suggested for %s%s.\n", p->path,
         palms ? ", with the palm" : "");
   return edit > 0;
}

static void input_openxr_release(input_openxr_t *st)
{
   unsigned i;
   retro_atomic_store_release_int(&input_openxr_ready, 0);
   for (i = 0; i < INPUT_OPENXR_HANDS; i++)
   {
      if (st->aim[i] && st->DestroySpace)
         st->DestroySpace(st->aim[i]);
      if (st->grip[i] && st->DestroySpace)
         st->DestroySpace(st->grip[i]);
      if (st->palm[i] && st->DestroySpace)
         st->DestroySpace(st->palm[i]);
      if (st->trackers[i] && st->DestroyHandTrackerEXT)
         st->DestroyHandTrackerEXT(st->trackers[i]);
   }
   /* Its actions go with it. */
   for (i = 0; i < INPUT_OPENXR_SETS; i++)
      if (st->sets[i] && st->DestroyActionSet)
         st->DestroyActionSet(st->sets[i]);
   input_openxr_clear(st);
}

static void input_openxr_hand_trackers(input_openxr_t *st,
      const openxr_input_handles_t *h)
{
   unsigned i;
   XrResult res;
   static const XrHandEXT hands[INPUT_OPENXR_HANDS] = {
      XR_HAND_LEFT_EXT, XR_HAND_RIGHT_EXT };

   if (     !INPUT_OPENXR_PROC(st, h->get_proc, h->instance,
               CreateHandTrackerEXT)
         || !INPUT_OPENXR_PROC(st, h->get_proc, h->instance,
               DestroyHandTrackerEXT)
         || !INPUT_OPENXR_PROC(st, h->get_proc, h->instance,
               LocateHandJointsEXT))
   {
      RARCH_WARN("[OpenXR] No hand joints: the runtime lacks the hand tracking functions.\n");
      return;
   }
   for (i = 0; i < INPUT_OPENXR_HANDS; i++)
   {
      XrHandTrackerCreateInfoEXT ci;
      XrHandTrackingDataSourceInfoEXT src;
      /* Seen hands, and hands guessed around a held controller. */
      XrHandTrackingDataSourceEXT sources[2] = {
         XR_HAND_TRACKING_DATA_SOURCE_UNOBSTRUCTED_EXT,
         XR_HAND_TRACKING_DATA_SOURCE_CONTROLLER_EXT };
      src.type                     = XR_TYPE_HAND_TRACKING_DATA_SOURCE_INFO_EXT;
      src.next                     = NULL;
      src.requestedDataSourceCount = 2;
      src.requestedDataSources     = sources;
      ci.type                      = XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT;
      ci.next                      = h->hand_data_source ? &src : NULL;
      ci.hand                      = hands[i];
      ci.handJointSet              = XR_HAND_JOINT_SET_DEFAULT_EXT;
      if (XR_FAILED(res = st->CreateHandTrackerEXT(h->session, &ci,
                  &st->trackers[i])))
      {
         RARCH_WARN("[OpenXR] No hand joints (xrCreateHandTrackerEXT: %d).\n",
               (int)res);
         if (i && st->trackers[0])
            st->DestroyHandTrackerEXT(st->trackers[0]);
         st->trackers[0] = XR_NULL_HANDLE;
         st->trackers[1] = XR_NULL_HANDLE;
         return;
      }
   }
   st->hand_data_source = h->hand_data_source;
   RARCH_LOG("[OpenXR] Hand joints tracked.\n");
}

static void input_openxr_session_created(void *user,
      const openxr_input_handles_t *h)
{
   unsigned i;
   XrResult res             = XR_SUCCESS;
   const char *step         = NULL;
   input_openxr_t *st       = &input_openxr_st;
   XrSessionActionSetsAttachInfo ai;
   (void)user;

   input_openxr_clear(st);
   if (!input_openxr_load(st, h->get_proc, h->instance))
   {
      RARCH_WARN("[OpenXR] Headset controllers unavailable: the runtime lacks action functions.\n");
      input_openxr_clear(st);
      return;
   }
   st->instance    = h->instance;
   st->session     = h->session;
   st->palm_pose   = h->palm_pose;
   if (     XR_FAILED(res = st->StringToPath(h->instance,
               "/user/hand/left", &st->hands[0]))
         || XR_FAILED(res = st->StringToPath(h->instance,
               "/user/hand/right", &st->hands[1])))
   {
      step = "xrStringToPath";
      goto error;
   }

   for (i = 0; i < INPUT_OPENXR_SETS; i++)
   {
      XrActionSetCreateInfo ci;
      ci.type     = XR_TYPE_ACTION_SET_CREATE_INFO;
      ci.next     = NULL;
      ci.priority = 0;
      strlcpy(ci.actionSetName, input_openxr_set_names[i],
            sizeof(ci.actionSetName));
      strlcpy(ci.localizedActionSetName, input_openxr_set_labels[i],
            sizeof(ci.localizedActionSetName));
      if (XR_FAILED(res = st->CreateActionSet(h->instance, &ci,
                  &st->sets[i])))
      {
         step = "xrCreateActionSet";
         goto error;
      }
   }
   for (i = 0; i < IXA_COUNT; i++)
   {
      XrActionCreateInfo ci;
      const input_openxr_action_def_t *d = &input_openxr_actions[i];
      ci.type                = XR_TYPE_ACTION_CREATE_INFO;
      ci.next                = NULL;
      ci.countSubactionPaths = 0;
      ci.subactionPaths      = NULL;
      ci.actionType          = d->type;
      strlcpy(ci.actionName, d->name, sizeof(ci.actionName));
      strlcpy(ci.localizedActionName, d->label,
            sizeof(ci.localizedActionName));
      if (d->hands)
      {
         ci.countSubactionPaths = INPUT_OPENXR_HANDS;
         ci.subactionPaths      = st->hands;
      }
      if (XR_FAILED(res = st->CreateAction(st->sets[d->set], &ci,
                  &st->actions[i])))
      {
         step = "xrCreateAction";
         goto error;
      }
   }

   for (i = 0; i < ARRAY_SIZE(input_openxr_profiles); i++)
      if (     (!input_openxr_profiles[i].frame || h->frame_controller)
            && input_openxr_suggest(st, &input_openxr_profiles[i]))
         st->edit_ok = true;

   /* All four are attached; each poll syncs the pointer set and the pad
    * set in use, or edit mode's. */
   ai.type            = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO;
   ai.next            = NULL;
   ai.countActionSets = INPUT_OPENXR_SETS;
   ai.actionSets      = st->sets;
   if (XR_FAILED(res = st->AttachSessionActionSets(h->session, &ai)))
   {
      step = "xrAttachSessionActionSets";
      goto error;
   }

   for (i = 0; i < INPUT_OPENXR_HANDS; i++)
   {
      XrActionSpaceCreateInfo si;
      si.type                            = XR_TYPE_ACTION_SPACE_CREATE_INFO;
      si.next                            = NULL;
      si.poseInActionSpace.orientation.x = 0.0f;
      si.poseInActionSpace.orientation.y = 0.0f;
      si.poseInActionSpace.orientation.z = 0.0f;
      si.poseInActionSpace.position.x    = 0.0f;
      si.poseInActionSpace.position.y    = 0.0f;
      si.poseInActionSpace.position.z    = 0.0f;
      si.action                          = st->actions[IXA_AIM];
      si.subactionPath                   = st->hands[i];
      si.poseInActionSpace.orientation.w = 1.0f;
      if (XR_FAILED(res = st->CreateActionSpace(h->session, &si,
                  &st->aim[i])))
      {
         step = "xrCreateActionSpace";
         goto error;
      }
      si.action = st->actions[IXA_GRIP];
      if (XR_FAILED(res = st->CreateActionSpace(h->session, &si,
                  &st->grip[i])))
      {
         step = "xrCreateActionSpace";
         goto error;
      }
      si.action = st->actions[IXA_PALM];
      if (     st->palm_pose
            && XR_FAILED(res = st->CreateActionSpace(h->session, &si,
                  &st->palm[i])))
      {
         step = "xrCreateActionSpace";
         goto error;
      }
   }

   if (h->hand_tracking)
      input_openxr_hand_trackers(st, h);

   st->frame = h->frame;
   retro_atomic_store_release_int(&input_openxr_ready, 1);
   RARCH_LOG("[OpenXR] Headset controllers ready.\n");
   return;

error:
   RARCH_WARN("[OpenXR] Headset controllers unavailable (%s: %d).\n",
         step, (int)res);
   input_openxr_release(st);
}

static void input_openxr_session_destroying(void *user)
{
   (void)user;
   input_openxr_release(&input_openxr_st);
}

static int16_t input_openxr_axis(float v)
{
   if (v > 1.0f)
      v = 1.0f;
   else if (v < -1.0f)
      v = -1.0f;
   return (int16_t)(v * 32767.0f);
}

static void input_openxr_get_info(input_openxr_t *st, unsigned a,
      XrPath hand, XrActionStateGetInfo *gi)
{
   gi->type          = XR_TYPE_ACTION_STATE_GET_INFO;
   gi->next          = NULL;
   gi->action        = st->actions[a];
   gi->subactionPath = hand;
}

static bool input_openxr_bool(input_openxr_t *st, unsigned a, XrPath hand)
{
   XrActionStateGetInfo gi;
   XrActionStateBoolean s;
   input_openxr_get_info(st, a, hand, &gi);
   s.type         = XR_TYPE_ACTION_STATE_BOOLEAN;
   s.next         = NULL;
   s.currentState = XR_FALSE;
   s.isActive     = XR_FALSE;
   return XR_SUCCEEDED(st->GetActionStateBoolean(st->session, &gi, &s))
      && s.isActive && s.currentState;
}

/* Whether a boolean action is bound to anything either hand holds. */
static bool input_openxr_bound(input_openxr_t *st, unsigned a)
{
   XrActionStateGetInfo gi;
   XrActionStateBoolean s;
   input_openxr_get_info(st, a, XR_NULL_PATH, &gi);
   s.type         = XR_TYPE_ACTION_STATE_BOOLEAN;
   s.next         = NULL;
   s.currentState = XR_FALSE;
   s.isActive     = XR_FALSE;
   return XR_SUCCEEDED(st->GetActionStateBoolean(st->session, &gi, &s))
      && s.isActive;
}

/* Edit mode cannot be used: the desktop told every time, the log once a
 * session. */
static void input_openxr_edit_unavailable(input_openxr_t *st,
      const char *why)
{
   const char *msg = msg_hash_to_str(MSG_OPENXR_EDIT_UNAVAILABLE);
   if (!st->edit_warned)
      RARCH_WARN("[OpenXR] Edit Screens unavailable: %s.\n", why);
   st->edit_warned = true;
   runloop_msg_queue_push(msg, strlen(msg), 1, 180, true, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
}

static float input_openxr_float(input_openxr_t *st, unsigned a, XrPath hand)
{
   XrActionStateGetInfo gi;
   XrActionStateFloat s;
   input_openxr_get_info(st, a, hand, &gi);
   s.type         = XR_TYPE_ACTION_STATE_FLOAT;
   s.next         = NULL;
   s.currentState = 0.0f;
   s.isActive     = XR_FALSE;
   if (     XR_FAILED(st->GetActionStateFloat(st->session, &gi, &s))
         || !s.isActive)
      return 0.0f;
   return s.currentState;
}

/* A stick into xy[0] and xy[1], with Y down as the RetroPad's is. */
static void input_openxr_stick(input_openxr_t *st, unsigned a, XrPath hand,
      int16_t *xy)
{
   XrActionStateGetInfo gi;
   XrActionStateVector2f s;
   input_openxr_get_info(st, a, hand, &gi);
   s.type           = XR_TYPE_ACTION_STATE_VECTOR2F;
   s.next           = NULL;
   s.currentState.x = 0.0f;
   s.currentState.y = 0.0f;
   s.isActive       = XR_FALSE;
   if (     XR_FAILED(st->GetActionStateVector2f(st->session, &gi, &s))
         || !s.isActive)
      return;
   xy[0] = input_openxr_axis(s.currentState.x);
   xy[1] = input_openxr_axis(-s.currentState.y);
}

/* Touch's one menu button: Start while it is held, and the RetroArch
 * menu too when let go after a second. A press that opened the menu
 * from edit mode is neither until let go. */
static void input_openxr_start_menu(input_openxr_t *st, bool down,
      input_openxr_pad_t *pad)
{
   retro_time_t now;
   if (!down)
   {
      st->start_menu_held  = false;
      st->start_menu_spent = false;
      return;
   }
   if (st->start_menu_spent)
      return;
   now = cpu_features_get_time_usec();
   if (!st->start_menu_held)
   {
      st->start_menu_held  = true;
      st->start_menu_since = now;
   }
   pad->buttons |= (uint16_t)(1u << RETRO_DEVICE_ID_JOYPAD_START);
   if (now - st->start_menu_since >= INPUT_OPENXR_MENU_HOLD)
      st->menu_toggle = true;
}

static void input_openxr_read_combined(input_openxr_t *st)
{
   unsigned id;
   input_openxr_pad_t *pad = &st->pads[0];
   for (id = 0; id < RARCH_FIRST_CUSTOM_BIND; id++)
      if (     id != RETRO_DEVICE_ID_JOYPAD_L2
            && id != RETRO_DEVICE_ID_JOYPAD_R2
            && input_openxr_bool(st, IXA_C_B + id, XR_NULL_PATH))
         pad->buttons |= (uint16_t)(1u << id);
   input_openxr_stick(st, IXA_C_LSTICK, XR_NULL_PATH, &pad->analog[0]);
   input_openxr_stick(st, IXA_C_RSTICK, XR_NULL_PATH, &pad->analog[2]);
   /* The left hand's trigger is L2, the right's R2. */
   st->trig[0].value = input_openxr_float(st, IXA_C_L2, XR_NULL_PATH);
   st->trig[0].pad   = 0;
   st->trig[0].slot  = 0;
   st->trig[1].value = input_openxr_float(st, IXA_C_R2, XR_NULL_PATH);
   st->trig[1].pad   = 0;
   st->trig[1].slot  = 1;
   st->menu_toggle   = input_openxr_bool(st, IXA_C_MENU, XR_NULL_PATH);
   st->recenter      = input_openxr_bool(st, IXA_C_RECENTER, XR_NULL_PATH);
   st->laser_toggle  = input_openxr_bool(st, IXA_C_LASER, XR_NULL_PATH);
   st->view_toggle   = input_openxr_bool(st, IXA_C_VIEW, XR_NULL_PATH);
   st->edit_toggle   = input_openxr_bool(st, IXA_C_EDIT, XR_NULL_PATH);
   input_openxr_start_menu(st,
         input_openxr_bool(st, IXA_C_START_MENU, XR_NULL_PATH), pad);
}

/* The left hand player 1, the right player 2. */
static void input_openxr_read_separate(input_openxr_t *st)
{
   unsigned h;
   st->start_menu_held = false;
   for (h = 0; h < INPUT_OPENXR_HANDS; h++)
   {
      XrPath hand             = st->hands[h];
      input_openxr_pad_t *pad = &st->pads[h];
      if (input_openxr_bool(st, IXA_S_B, hand))
         pad->buttons |= (uint16_t)(1u << RETRO_DEVICE_ID_JOYPAD_B);
      if (input_openxr_bool(st, IXA_S_A, hand))
         pad->buttons |= (uint16_t)(1u << RETRO_DEVICE_ID_JOYPAD_A);
      if (input_openxr_bool(st, IXA_S_R, hand))
         pad->buttons |= (uint16_t)(1u << RETRO_DEVICE_ID_JOYPAD_R);
      if (input_openxr_bool(st, IXA_S_START, hand))
         pad->buttons |= (uint16_t)(1u << RETRO_DEVICE_ID_JOYPAD_START);
      input_openxr_stick(st, IXA_S_STICK, hand, &pad->analog[0]);
      st->trig[h].value = input_openxr_float(st, IXA_S_R2, hand);
      st->trig[h].pad   = h;
      st->trig[h].slot  = 1;
   }
   st->menu_toggle  = input_openxr_bool(st, IXA_S_MENU, XR_NULL_PATH);
   st->recenter     = input_openxr_bool(st, IXA_S_RECENTER, XR_NULL_PATH);
   st->laser_toggle = input_openxr_bool(st, IXA_S_LASER, XR_NULL_PATH);
   st->view_toggle  = input_openxr_bool(st, IXA_S_VIEW, XR_NULL_PATH);
   st->edit_toggle  = input_openxr_bool(st, IXA_S_EDIT, XR_NULL_PATH);
}

static bool input_openxr_pressing(enum input_openxr_role role)
{
   return role == INPUT_OPENXR_ROLE_TOUCH
      || role == INPUT_OPENXR_ROLE_MENU;
}

/* The triggers were not read: a press ends, and a trigger still held
 * when they are read again is no new pull. */
static void input_openxr_triggers_lost(input_openxr_t *st)
{
   unsigned h;
   for (h = 0; h < INPUT_OPENXR_HANDS; h++)
   {
      if (input_openxr_pressing(st->role[h]))
         st->role[h] = INPUT_OPENXR_ROLE_SPENT;
      st->trig_down[h] = true;
   }
   st->touch_on = false;
}

/* Each hand's trigger into its L2 or R2: the value, and the button
 * past the threshold. Before its pull, a hand on a live quad or the
 * pointer keeps its trigger for the laser. */
static void input_openxr_triggers(input_openxr_t *st, float threshold)
{
   unsigned h;
   for (h = 0; h < INPUT_OPENXR_HANDS; h++)
   {
      input_openxr_pad_t *pad = &st->pads[st->trig[h].pad];
      float v                 = st->trig[h].value;
      if (     v <= 0.0f
            || (st->role[h] != INPUT_OPENXR_ROLE_PAD
               && (st->role[h] != INPUT_OPENXR_ROLE_UP
                  || st->hit[h] >= 0 || (int)h == st->pointer)))
         continue;
      pad->trigger[st->trig[h].slot] = input_openxr_axis(v);
      if (v > threshold)
         pad->buttons |= (uint16_t)(1u
               << (RETRO_DEVICE_ID_JOYPAD_L2 + st->trig[h].slot));
   }
}

/* A space's pose in another at time, when the runtime finds it. Either
 * thread: it only reads what the session hooks wrote. */
static bool input_openxr_locate(const input_openxr_t *st, XrSpace what,
      XrSpace space, XrTime time, video_xr_pose_t *pose)
{
   XrSpaceLocation loc;
   XrSpaceLocationFlags valid = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT
      | XR_SPACE_LOCATION_POSITION_VALID_BIT;

   loc.type          = XR_TYPE_SPACE_LOCATION;
   loc.next          = NULL;
   loc.locationFlags = 0;
   if (     !what || !time
         || XR_FAILED(st->LocateSpace(what, space, time, &loc))
         || (loc.locationFlags & valid) != valid)
      return false;
   pose->orientation.x = loc.pose.orientation.x;
   pose->orientation.y = loc.pose.orientation.y;
   pose->orientation.z = loc.pose.orientation.z;
   pose->orientation.w = loc.pose.orientation.w;
   pose->position.x    = loc.pose.position.x;
   pose->position.y    = loc.pose.position.y;
   pose->position.z    = loc.pose.position.z;
   return true;
}

/* A hand's aim pose in space at time, when it is tracked. */
static bool input_openxr_aim(const input_openxr_t *st, unsigned h,
      XrSpace space, XrTime time, video_xr_pose_t *pose)
{
   return input_openxr_locate(st, st->aim[h], space, time, pose);
}

/* An aim pose's ray: from its position along its -Z. */
static void input_openxr_aim_ray(const video_xr_pose_t *pose,
      video_xr_vec3_t *o, video_xr_vec3_t *d)
{
   video_xr_vec3_t fwd;
   *o    = pose->position;
   fwd.x = 0.0f;
   fwd.y = 0.0f;
   fwd.z = -1.0f;
   video_xr_rotate(&pose->orientation, &fwd, d);
}

/* A hand's aim ray in space at time, when it is tracked. */
static bool input_openxr_ray(const input_openxr_t *st, unsigned h,
      XrSpace space, XrTime time, video_xr_vec3_t *o, video_xr_vec3_t *d)
{
   video_xr_pose_t pose;
   if (!input_openxr_aim(st, h, space, time, &pose))
      return false;
   input_openxr_aim_ray(&pose, o, d);
   return true;
}

/* Edit Smoothing's level for the frame loop from the main thread's
 * config: edit mode's, else Off. */
static unsigned input_openxr_smoothing(int config)
{
   if ((config & INPUT_OPENXR_LASER_MASK) != VIDEO_XR_LIVE_EDIT)
      return VIDEO_OPENXR_EDIT_SMOOTHING_OFF;
   return ((unsigned)config >> INPUT_OPENXR_SMOOTH_SHIFT)
      & INPUT_OPENXR_SMOOTH_MASK;
}

/* The frame loop's aim for hand h at time in space: in edit mode with
 * Edit Smoothing on, its filter's, stepped once a frame whichever hook
 * asks first and published for the main thread; else as located. Lost,
 * recentred or in another space, the filter starts afresh. */
static bool input_openxr_frame_pose(input_openxr_t *st, unsigned h,
      XrSpace space, XrTime time, video_xr_pose_t *pose)
{
   video_xr_pose_t raw;
   input_openxr_smooth_t *s = &st->smooth[h];
   unsigned level           = input_openxr_smoothing(
         retro_atomic_load_acquire_int(&input_openxr_pointer_config));
   if (!level || !st->frame)
   {
      /* The next edit mode with it on starts afresh. */
      s->filter.on = false;
      s->time      = 0;
      return input_openxr_aim(st, h, space, time, pose);
   }
   if (s->time != time || s->space != space)
   {
      int recenters = openxr_frame_recenters(st->frame);
      double dt     = (s->time && time > s->time && s->space == space)
         ? (double)(time - s->time) / 1000000000.0 : 0.0;
      s->tracked    = input_openxr_aim(st, h, space, time, &raw);
      if (!s->tracked || recenters != s->recenters)
         s->filter.on = false;
      if (s->tracked)
      {
         video_xr_smooth_step(&s->filter, level, &raw, dt, &s->pose);
         st->aims.pose[h] = s->pose;
         st->aims.valid  |= 1u << h;
      }
      else
         st->aims.valid  &= ~(1u << h);
      st->aims.time[h] = time;
      s->time      = time;
      s->space     = space;
      s->recenters = recenters;
      openxr_frame_publish_aims(st->frame, &st->aims);
   }
   if (s->tracked)
      *pose = s->pose;
   return s->tracked;
}

/* First in each headset frame: in edit mode with Edit Smoothing on,
 * both hands' filters stepped and published whatever the layers then
 * draw; else every filter afresh and any aims published taken back,
 * so the main thread never reads an earlier edit's. */
static void input_openxr_frame_aims(input_openxr_t *st, int config,
      XrSpace space, XrTime time)
{
   unsigned h;
   video_xr_pose_t pose;
   if (input_openxr_smoothing(config))
   {
      for (h = 0; h < INPUT_OPENXR_HANDS; h++)
         input_openxr_frame_pose(st, h, space, time, &pose);
      return;
   }
   for (h = 0; h < INPUT_OPENXR_HANDS; h++)
   {
      st->smooth[h].filter.on = false;
      st->smooth[h].time      = 0;
   }
   if (st->aims.valid)
   {
      st->aims.valid = 0;
      openxr_frame_publish_aims(st->frame, &st->aims);
   }
}

/* A point on a screen's quad as the core's pointer coordinates: into
 * the core's view map when it has one, over the whole frame when not.
 * A window has the frame where the video driver's viewport puts it, as
 * the window's own pointer does. */
static bool input_openxr_frame_point(const video_xr_quad_t *q, float u,
      float v, int16_t *x, int16_t *y)
{
   unsigned dims                = 0;
   const video_views_map_t *map = NULL;
   if (q->kind == VIDEO_XR_QUAD_WINDOW)
   {
      video_viewport_t vp;
      if (     !video_driver_get_viewport_info(&vp)
            || !video_xr_window_point(&vp, &u, &v))
         return false;
   }
   if (!video_driver_get_views_core(&map, &dims))
      return false;
   return video_xr_quad_to_frame(q, u, v, map, dims, x, y);
}

/* The menu's own quad, or a window while the menu is open over it. */
static bool input_openxr_on_menu(const video_xr_quad_t *q, bool menu_open)
{
   return q->kind == VIDEO_XR_QUAD_MENU
      || (q->kind == VIDEO_XR_QUAD_WINDOW && menu_open);
}

/* Where each hand points, which hand is the pointer, and what it points
 * at: a point in the core's frame, the menu, or in Always nothing (an
 * off-screen shot). A trigger's role is set by its pull and kept until
 * it is let go, so a press needs a pull of its own. was_pressed is the
 * last poll's press. */
static void input_openxr_laser_poll(input_openxr_t *st, unsigned laser,
      bool menu_open, float threshold, bool was_pressed)
{
   unsigned h, i;
   int p;
   bool lift;
   bool can[INPUT_OPENXR_HANDS];
   bool pulled[INPUT_OPENXR_HANDS];
   bool tracked[INPUT_OPENXR_HANDS];
   XrTime time              = 0;
   XrSpace space            = XR_NULL_HANDLE;
   const video_xr_quad_t *q = NULL;
   bool menu_live           = false;
   bool touch_on            = st->touch_on;
   int prev                 = st->pointer;

   st->touch_on = false;
   for (h = 0; h < INPUT_OPENXR_HANDS; h++)
   {
      float v          = st->trig[h].value;
      bool down        = v > threshold;
      pulled[h]        = down && !st->trig_down[h];
      st->trig_down[h] = down;
      /* L2 or R2, unless a live quad takes the pull below. */
      if (pulled[h])
         st->role[h] = INPUT_OPENXR_ROLE_PAD;
      else if (v <= 0.0f)
         st->role[h] = INPUT_OPENXR_ROLE_UP;
      else if (!down && input_openxr_pressing(st->role[h]))
         st->role[h] = INPUT_OPENXR_ROLE_SPENT;
   }

   st->pointer         = -1;
   st->quads.num_quads = 0;
   if (     laser != VIDEO_OPENXR_LASER_OFF
         && (time = openxr_frame_predicted_time(st->frame)))
      openxr_frame_get_shown_quads(st->frame, &st->quads, &space);
   for (i = 0; i < st->quads.num_quads; i++)
   {
      const video_xr_quad_t *lq = &st->quads.quads[i];
      if (!video_xr_quad_live(lq, laser, menu_open))
         continue;
      /* Without a live screen, the mouse and the overlay keep the
       * core's pointer and gun. */
      if (input_openxr_on_menu(lq, menu_open))
         menu_live     = true;
      else
         st->ptr_owned = true;
   }
   /* A press ends with its quads. */
   for (h = 0; h < INPUT_OPENXR_HANDS; h++)
      if (     (st->role[h] == INPUT_OPENXR_ROLE_MENU && !menu_live)
            || (st->role[h] == INPUT_OPENXR_ROLE_TOUCH && !st->ptr_owned))
         st->role[h] = INPUT_OPENXR_ROLE_SPENT;
   if (!menu_live && !st->ptr_owned)
      return;

   st->pointer = prev;
   for (h = 0; h < INPUT_OPENXR_HANDS; h++)
   {
      float t;
      video_xr_vec3_t o, d;
      /* A press keeps its hand off the quads too. */
      can[h]     = input_openxr_pressing(st->role[h]);
      tracked[h] = input_openxr_ray(st, h, space, time, &o, &d);
      if (tracked[h])
      {
         st->hit[h] = video_xr_pick(&st->quads, laser, menu_open, &o, &d,
               &st->hit_u[h], &st->hit_v[h], &t);
         /* Always: a hand off every screen still aims the gun. */
         if (st->hit[h] >= 0 || laser == VIDEO_OPENXR_LASER_ALWAYS)
            can[h] = true;
      }
      /* With both hands on the screens, the last pull points. */
      if (pulled[h] && st->hit[h] >= 0)
         st->pointer = (int)h;
   }
   /* Always: the gun stays with its hand through a tracking loss. */
   if (laser == VIDEO_OPENXR_LASER_ALWAYS && prev >= 0)
      can[prev] = true;
   if (!can[0] && !can[1])
      st->pointer = -1;
   else if (st->pointer < 0 || !can[st->pointer])
      st->pointer = can[1] ? 1 : 0;
   p = st->pointer;

   /* Only the pointer presses: a pull on a live quad, or in Always its
    * pull anywhere. */
   for (h = 0; h < INPUT_OPENXR_HANDS; h++)
   {
      if ((int)h != p)
      {
         if (     input_openxr_pressing(st->role[h])
               || (pulled[h] && st->hit[h] >= 0))
            st->role[h] = INPUT_OPENXR_ROLE_SPENT;
      }
      else if (pulled[h] && st->hit[h] >= 0)
         st->role[h] = input_openxr_on_menu(
               &st->quads.quads[st->hit[h]], menu_open)
            ? INPUT_OPENXR_ROLE_MENU : INPUT_OPENXR_ROLE_TOUCH;
      else if (pulled[h] && laser == VIDEO_OPENXR_LASER_ALWAYS)
         st->role[h] = INPUT_OPENXR_ROLE_TOUCH;
   }
   if (p < 0)
      return;

   /* A touchscreen core must see a lift between hands, not a drag. */
   lift = p != prev && was_pressed;
   if (st->hit[p] >= 0)
      q = &st->quads.quads[st->hit[p]];
   if (st->role[p] == INPUT_OPENXR_ROLE_MENU)
   {
      /* Off the quad, the press stays where it left it. */
      if (q && input_openxr_on_menu(q, menu_open))
      {
         st->menu_u = st->hit_u[p];
         st->menu_v = st->hit_v[p];
      }
      st->menu_on      = true;
      st->menu_pressed = !lift;
      return;
   }
   if (q && input_openxr_on_menu(q, menu_open))
   {
      st->menu_on = true;
      st->menu_u  = st->hit_u[p];
      st->menu_v  = st->hit_v[p];
   }
   else if (q)
      st->ptr_on = input_openxr_frame_point(q, st->hit_u[p], st->hit_v[p],
            &st->ptr_x, &st->ptr_y);
   /* Untracked mid-press, a touch holds its last point, as the menu's
    * press does. */
   else if (     !tracked[p] && touch_on && p == prev && !pulled[p]
             && st->role[p] == INPUT_OPENXR_ROLE_TOUCH)
      st->ptr_on = true;
   /* Held, a touch lifts off the screens and lands again back on them,
    * as a stylus does; Always's gun fires off them too. */
   st->ptr_pressed = !lift && st->ptr_on
      && st->role[p] == INPUT_OPENXR_ROLE_TOUCH;
   st->touch_on    = st->ptr_pressed;
   st->gun_pressed = st->ptr_pressed || (!lift
         && laser == VIDEO_OPENXR_LASER_ALWAYS
         && st->role[p] == INPUT_OPENXR_ROLE_TOUCH);
}

/* Whether this read newly presses a button other than the triggers' L2
 * and R2, or pushes a stick past half. */
static bool input_openxr_new_press(input_openxr_t *st)
{
   unsigned p, s;
   bool press = false;
   for (p = 0; p < INPUT_OPENXR_PADS; p++)
   {
      const input_openxr_pad_t *pad = &st->pads[p];
      unsigned sticks               = 0;
      uint16_t buttons              = (uint16_t)(pad->buttons
            & ~((1u << RETRO_DEVICE_ID_JOYPAD_L2)
               | (1u << RETRO_DEVICE_ID_JOYPAD_R2)));
      for (s = 0; s < 2; s++)
         if (     abs(pad->analog[s * 2])     > INPUT_OPENXR_YIELD_STICK
               || abs(pad->analog[s * 2 + 1]) > INPUT_OPENXR_YIELD_STICK)
            sticks |= 1u << s;
      if (     (buttons & ~st->last_buttons[p])
            || (sticks  & ~st->last_sticks[p]))
         press = true;
      st->last_buttons[p] = buttons;
      st->last_sticks[p]  = sticks;
   }
   return press;
}

/* With the menu open, the laser yields to the controllers' buttons and
 * sticks, so hand drift cannot undo their navigation: from a press until
 * a trigger is pulled. Moving the controllers does not end it: a D-pad
 * press alone tilts one by over 3 degrees on the Steam Frame.
 * That pull is spent, with a press's or without: an unseen laser must
 * not click. */
static void input_openxr_yield(input_openxr_t *st, unsigned laser,
      bool menu_open, float threshold)
{
   unsigned h;
   bool press = input_openxr_new_press(st);

   if (     !menu_open || laser == VIDEO_OPENXR_LASER_OFF
         || (!st->yield && !press))
      return;
   for (h = 0; h < INPUT_OPENXR_HANDS; h++)
   {
      if (st->trig[h].value > threshold && !st->trig_down[h])
      {
         /* No pull until let go, as after a loss. */
         st->trig_down[h] = true;
         st->role[h]      = INPUT_OPENXR_ROLE_SPENT;
         st->yield        = false;
      }
   }
   if (press)
      st->yield = true;
}

/* One of edit mode's layers: the laser's image's w by h part from x, y,
 * over q, just in front of it. */
static const XrCompositionLayerBaseHeader *input_openxr_edit_layer(
      XrCompositionLayerQuad *l, const video_xr_quad_t *q, XrSpace space,
      XrSwapchain cursor, int32_t x, int32_t y, int32_t w, int32_t h)
{
   video_xr_vec3_t lift, world;
   lift.x = 0.0f;
   lift.y = 0.0f;
   lift.z = VIDEO_XR_CURSOR_LIFT;
   video_xr_rotate(&q->pose.orientation, &lift, &world);
   l->type                             = XR_TYPE_COMPOSITION_LAYER_QUAD;
   l->next                             = NULL;
   l->subImage.imageArrayIndex         = 0;
   l->layerFlags                       =
      XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
   l->space                            = space;
   l->eyeVisibility                    = XR_EYE_VISIBILITY_BOTH;
   l->subImage.swapchain               = cursor;
   l->subImage.imageRect.offset.x      = x;
   l->subImage.imageRect.offset.y      = y;
   l->subImage.imageRect.extent.width  = w;
   l->subImage.imageRect.extent.height = h;
   l->pose.orientation.x               = q->pose.orientation.x;
   l->pose.orientation.y               = q->pose.orientation.y;
   l->pose.orientation.z               = q->pose.orientation.z;
   l->pose.orientation.w               = q->pose.orientation.w;
   l->pose.position.x                  = q->pose.position.x + world.x;
   l->pose.position.y                  = q->pose.position.y + world.y;
   l->pose.position.z                  = q->pose.position.z + world.z;
   l->size.width                       = q->width;
   l->size.height                      = q->height;
   return (const XrCompositionLayerBaseHeader*)l;
}

/* Edge's rim of slot to hide, unless one beside it already is: one crop
 * hides one edge or two opposite. */
static void input_openxr_hinge_hide(unsigned *hide, unsigned slot,
      unsigned edge)
{
   unsigned beside = (edge < 2) ? 12u : 3u;
   if (!(hide[slot] & beside))
      hide[slot] |= 1u << edge;
}

/* A hidden hinge slot's line: VIDEO_XR_HINGE_WIDTH along the x of
 * slot's screen from its middle, else of set's first quad; false with
 * no quad. */
static bool input_openxr_hinge_hidden(const video_xr_quad_set_t *set,
      unsigned slot, video_xr_vec3_t *a, video_xr_vec3_t *b)
{
   unsigned i;
   video_xr_vec3_t x;
   const video_xr_quad_t *q = set->num_quads ? &set->quads[0] : NULL;
   for (i = 0; i < set->num_quads; i++)
      if (     set->quads[i].kind != VIDEO_XR_QUAD_MENU
            && set->quads[i].slot == slot)
      {
         q = &set->quads[i];
         break;
      }
   if (!q)
      return false;
   x.x  = VIDEO_XR_HINGE_WIDTH;
   x.y  = 0.0f;
   x.z  = 0.0f;
   video_xr_rotate(&q->pose.orientation, &x, &x);
   *a   = q->pose.position;
   b->x = a->x + x.x;
   b->y = a->y + x.y;
   b->z = a->z + x.z;
   return true;
}

/* This frame's hinge slots, one for each hand holding a screen, hand 0
 * first, then one for each pair at rest: in hide, by slot, the edges
 * whose rims go, each held screen's paired edge and its target's, then
 * both edges of each pair at rest no hand holds (of a screen's two
 * adjacent ones, the first); from a to b each slot's line, bit k of
 * shown set where slot k's shows. A slot with no line shown is hidden
 * in place, so a mark or a wobble moves no layer. Returns the slots. */
static unsigned input_openxr_hinges(const openxr_frame_t *f,
      const video_xr_quad_set_t *set, unsigned *hide, video_xr_vec3_t *a,
      video_xr_vec3_t *b, unsigned *shown)
{
   unsigned h, i, r;
   unsigned holding = 0;
   unsigned lines   = 0;
   memset(hide, 0, VIDEO_XR_MAX_SLOTS * sizeof(*hide));
   *shown = 0;
   for (h = 0; h < INPUT_OPENXR_HANDS; h++)
   {
      const video_xr_hinge_t *k     = &f->hinges[h];
      const video_xr_quad_t *held   = NULL;
      const video_xr_quad_t *target = NULL;
      bool hinged                   = ((f->hinged >> h) & 1u) != 0;
      bool line;
      if (     f->grabs.hand[h].state != VIDEO_XR_GRAB_HELD
            || f->grabs.hand[h].slot >= VIDEO_XR_MENU_SLOT)
         continue;
      if (hinged)
      {
         hide[f->grabs.hand[h].slot] |= 1u << k->edge;
         input_openxr_hinge_hide(hide, k->slot, k->on);
      }
      for (i = 0; i < set->num_quads; i++)
      {
         const video_xr_quad_t *q = &set->quads[i];
         if (q->kind == VIDEO_XR_QUAD_MENU)
            continue;
         if (!held && q->slot == f->grabs.hand[h].slot)
            held = q;
         if (!target && q->slot == k->slot)
            target = q;
      }
      line = hinged && held && target
         && video_xr_hinge_line(k, held->width, held->height, target,
            &a[lines], &b[lines]);
      if (line)
         *shown |= 1u << lines;
      if (     line || input_openxr_hinge_hidden(set,
               f->grabs.hand[h].slot, &a[lines], &b[lines]))
         lines++;
   }
   for (h = 0; h < INPUT_OPENXR_HANDS; h++)
      if (     f->grabs.hand[h].state == VIDEO_XR_GRAB_HELD
            && f->grabs.hand[h].slot < VIDEO_XR_MAX_SLOTS)
         holding |= 1u << f->grabs.hand[h].slot;
   for (r = 0; r < f->grabs.num_rests && r < OPENXR_REST_HINGES; r++)
   {
      video_xr_hinge_t k;
      const video_xr_rest_t *p      = &f->grabs.rests[r];
      const video_xr_quad_t *held   = NULL;
      const video_xr_quad_t *target = NULL;
      bool over                     = false;
      bool line                     = false;
      bool idle                     =
         !(((holding >> p->slot) | (holding >> p->target)) & 1u);
      for (i = 0; i < set->num_quads; i++)
      {
         const video_xr_quad_t *q = &set->quads[i];
         if (q->kind == VIDEO_XR_QUAD_MENU)
            continue;
         if (!held && q->slot == p->slot)
         {
            held = q;
            over = target != NULL;
         }
         if (!target && q->slot == p->target)
            target = q;
      }
      if (idle && held && target)
      {
         input_openxr_hinge_hide(hide, p->slot, p->edge);
         input_openxr_hinge_hide(hide, p->target, p->on);
         memset(&k, 0, sizeof(k));
         k.edge = p->edge;
         k.slot = p->target;
         k.on   = p->on;
         /* On the edges as they are; crossed, on the one drawn later. */
         video_xr_hinge_place(&k, &held->pose, held->width, held->height,
               target, over);
         line = video_xr_hinge_line(&k, held->width, held->height, target,
               &a[lines], &b[lines]);
      }
      if (line)
         *shown |= 1u << lines;
      if (     line || input_openxr_hinge_hidden(set, p->slot, &a[lines],
               &b[lines]))
         lines++;
   }
   return lines;
}

/* Edit mode's outlines after layers' first n, of the menu or of every
 * other item: one an item (a stereo screen's two quads are one, a fixed
 * slot none), each just outside its item without the rims of its edges
 * in hide (by slot), the menu's handle after its outline: bright for
 * the slots in lit, faint for the rest; up to cap in all; turned to the
 * head with an item the frame shows by its back. With the cut outlines
 * in the image, a cut changes the outline's cell alone, never its quad. */
static unsigned input_openxr_outlines(input_openxr_t *st,
      const video_xr_quad_set_t *set, XrSpace space, XrSwapchain cursor,
      unsigned lit, const unsigned *hide, bool menu,
      const XrCompositionLayerBaseHeader **layers, unsigned n, unsigned cap)
{
   unsigned i;
   unsigned done = 0;
   bool cells    = openxr_frame_cursor_w(st->frame) == VIDEO_XR_CURSOR_W;
   for (i = 0; i < set->num_quads && n < cap; i++)
   {
      const video_xr_quad_t *q = &set->quads[i];
      video_xr_quad_t outline;
      unsigned bright;
      unsigned x, w;
      bool back = ((st->frame->backs >> i) & 1u) != 0;
      if (     q->slot >= VIDEO_XR_MAX_SLOTS || (done & (1u << q->slot))
            || set->slots[q->slot].fixed
            || (q->kind == VIDEO_XR_QUAD_MENU) != menu)
         continue;
      done     |= 1u << q->slot;
      bright    = (lit >> q->slot) & 1u;
      video_xr_outline(q, &outline);
      if (back)
         video_xr_turn_back(&outline.pose);
      if (cells)
      {
         x = video_xr_outline_cell(hide[q->slot], back, bright);
         w = VIDEO_XR_OUTLINE_DIM;
      }
      else
      {
         video_xr_outline_crop(&outline, hide[q->slot], back, &x, &w);
         x += VIDEO_XR_OUTLINE_X(bright);
      }
      layers[n] = input_openxr_edit_layer(&st->outlines[n], &outline,
            space, cursor, (int32_t)x, 0, (int32_t)w, VIDEO_XR_OUTLINE_DIM);
      n++;
      if (q->kind == VIDEO_XR_QUAD_MENU && n < cap)
      {
         video_xr_quad_t handle;
         video_xr_menu_handle(q, &handle);
         if (back)
            video_xr_turn_back(&handle.pose);
         layers[n] = input_openxr_edit_layer(&st->outlines[n], &handle,
               space, cursor, VIDEO_XR_HANDLE_X(bright), VIDEO_XR_HANDLE_Y,
               VIDEO_XR_HANDLE_COLS, VIDEO_XR_HANDLE_ROWS);
         n++;
      }
   }
   return n;
}

/* Whether a laser in this mode has anything in set to point at. */
static bool input_openxr_any_live(const video_xr_quad_set_t *set,
      unsigned laser, bool menu_open)
{
   unsigned i;
   for (i = 0; i < set->num_quads; i++)
   {
      const video_xr_quad_t *q = &set->quads[i];
      if (     video_xr_quad_live(q, laser, menu_open)
            && !(laser == VIDEO_XR_LIVE_EDIT && set->slots[q->slot].fixed))
         return true;
   }
   return false;
}

/* A hand's beam from o along d, reach metres to its dot when it hit
 * something, facing eye. */
static const XrCompositionLayerBaseHeader *input_openxr_beam_layer(
      XrCompositionLayerQuad *l, const video_xr_vec3_t *o,
      const video_xr_vec3_t *d, float reach, bool hit,
      const video_xr_vec3_t *eye, XrSpace space, XrSwapchain cursor)
{
   video_xr_pose_t pose;
   video_xr_beam(o, d, reach, eye, &pose);
   l->type                             = XR_TYPE_COMPOSITION_LAYER_QUAD;
   l->next                             = NULL;
   l->subImage.imageArrayIndex         = 0;
   l->layerFlags                       =
      XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
   l->space                            = space;
   l->eyeVisibility                    = XR_EYE_VISIBILITY_BOTH;
   l->subImage.swapchain               = cursor;
   l->subImage.imageRect.offset.x      = VIDEO_XR_BEAM_X(hit ? 0 : 1);
   l->subImage.imageRect.offset.y      = VIDEO_XR_BEAM_Y;
   l->subImage.imageRect.extent.width  = VIDEO_XR_BEAM_LEN;
   l->subImage.imageRect.extent.height = VIDEO_XR_BEAM_ROWS;
   l->pose.orientation.x               = pose.orientation.x;
   l->pose.orientation.y               = pose.orientation.y;
   l->pose.orientation.z               = pose.orientation.z;
   l->pose.orientation.w               = pose.orientation.w;
   l->pose.position.x                  = pose.position.x;
   l->pose.position.y                  = pose.position.y;
   l->pose.position.z                  = pose.position.z;
   l->size.width                       = reach;
   l->size.height                      = VIDEO_XR_BEAM_WIDTH;
   return (const XrCompositionLayerBaseHeader*)l;
}

/* p moved VIDEO_XR_HINGE_LIFT toward eye: a runtime need not draw
 * overlapping quads in order, and SteamVR draws screens over a line on
 * their seam. */
static void input_openxr_hinge_lift(const video_xr_vec3_t *p,
      const video_xr_vec3_t *eye, video_xr_vec3_t *out)
{
   video_xr_vec3_t t;
   float n, s;
   t.x    = eye->x - p->x;
   t.y    = eye->y - p->y;
   t.z    = eye->z - p->z;
   n      = (float)sqrt(t.x * t.x + t.y * t.y + t.z * t.z);
   s      = (n > VIDEO_XR_HINGE_LIFT) ? VIDEO_XR_HINGE_LIFT / n : 0.0f;
   out->x = p->x + t.x * s;
   out->y = p->y + t.y * s;
   out->z = p->z + t.z * s;
}

/* A hinge's line from a to b, each end lifted toward eye and the ribbon
 * facing it; the strip a texel in from each end, so filtering there
 * meets green alone; hidden, from rows of its column nothing is drawn
 * in. */
static const XrCompositionLayerBaseHeader *input_openxr_hinge_layer(
      XrCompositionLayerQuad *l, const video_xr_vec3_t *a,
      const video_xr_vec3_t *b, const video_xr_vec3_t *eye,
      XrSpace space, XrSwapchain cursor, bool hidden)
{
   video_xr_pose_t pose;
   video_xr_vec3_t p, q, d;
   float len;
   input_openxr_hinge_lift(a, eye, &p);
   input_openxr_hinge_lift(b, eye, &q);
   d.x  = q.x - p.x;
   d.y  = q.y - p.y;
   d.z  = q.z - p.z;
   len  = (float)sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
   d.x /= len;
   d.y /= len;
   d.z /= len;
   video_xr_beam(&p, &d, len, eye, &pose);
   l->type                             = XR_TYPE_COMPOSITION_LAYER_QUAD;
   l->next                             = NULL;
   l->subImage.imageArrayIndex         = 0;
   l->layerFlags                       =
      XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
   l->space                            = space;
   l->eyeVisibility                    = XR_EYE_VISIBILITY_BOTH;
   l->subImage.swapchain               = cursor;
   l->subImage.imageRect.offset.x      = VIDEO_XR_HINGE_X + 1;
   l->subImage.imageRect.offset.y      = hidden
      ? VIDEO_XR_HINGE_HIDDEN_Y : VIDEO_XR_HINGE_Y;
   l->subImage.imageRect.extent.width  = VIDEO_XR_HINGE_COLS - 2;
   l->subImage.imageRect.extent.height = VIDEO_XR_HINGE_ROWS;
   l->pose.orientation.x               = pose.orientation.x;
   l->pose.orientation.y               = pose.orientation.y;
   l->pose.orientation.z               = pose.orientation.z;
   l->pose.orientation.w               = pose.orientation.w;
   l->pose.position.x                  = pose.position.x;
   l->pose.position.y                  = pose.position.y;
   l->pose.position.z                  = pose.position.z;
   l->size.width                       = len;
   l->size.height                      = VIDEO_XR_HINGE_WIDTH;
   return (const XrCompositionLayerBaseHeader*)l;
}

/* Edit mode's tag over the hand at o, facing eye: tag's cell, the
 * chosen item's words. */
static const XrCompositionLayerBaseHeader *input_openxr_tag_layer(
      XrCompositionLayerQuad *l, const video_xr_vec3_t *o,
      const video_xr_vec3_t *eye, unsigned tag, XrSpace space,
      XrSwapchain cursor)
{
   video_xr_pose_t pose;
   video_xr_tag(o, eye, &pose);
   l->type                             = XR_TYPE_COMPOSITION_LAYER_QUAD;
   l->next                             = NULL;
   l->subImage.imageArrayIndex         = 0;
   l->layerFlags                       =
      XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
   l->space                            = space;
   l->eyeVisibility                    = XR_EYE_VISIBILITY_BOTH;
   l->subImage.swapchain               = cursor;
   l->subImage.imageRect.offset.x      = VIDEO_XR_TAG_X(tag);
   l->subImage.imageRect.offset.y      = VIDEO_XR_TAG_Y;
   l->subImage.imageRect.extent.width  = VIDEO_XR_TAG_COLS;
   l->subImage.imageRect.extent.height = VIDEO_XR_TAG_ROWS;
   l->pose.orientation.x               = pose.orientation.x;
   l->pose.orientation.y               = pose.orientation.y;
   l->pose.orientation.z               = pose.orientation.z;
   l->pose.orientation.w               = pose.orientation.w;
   l->pose.position.x                  = pose.position.x;
   l->pose.position.y                  = pose.position.y;
   l->pose.position.z                  = pose.position.z;
   l->size.width                       = VIDEO_XR_TAG_HEIGHT
      * VIDEO_XR_TAG_COLS / VIDEO_XR_TAG_ROWS;
   l->size.height                      = VIDEO_XR_TAG_HEIGHT;
   return (const XrCompositionLayerBaseHeader*)l;
}

/* Edit mode's bright items, from the slot each hand's dot is on (-1:
 * none): every held item; with nothing chosen, those pointed at; else
 * the chosen item, and what a hand points at while the other holds the
 * chosen item. */
static unsigned input_openxr_edit_lit(const openxr_grabs_t *g,
      const int *pointed)
{
   unsigned h;
   unsigned lit = 0;
   for (h = 0; h < INPUT_OPENXR_HANDS; h++)
   {
      const video_xr_grab_t *other = &g->hand[h ^ 1];
      if (     g->hand[h].state == VIDEO_XR_GRAB_HELD
            && g->hand[h].slot < VIDEO_XR_MAX_SLOTS)
         lit |= 1u << g->hand[h].slot;
      if (     pointed[h] >= 0
            && (!g->chosen
               || (other->state == VIDEO_XR_GRAB_HELD
                  && other->slot == g->chosen - 1)))
         lit |= 1u << pointed[h];
   }
   if (g->chosen)
      lit |= 1u << (g->chosen - 1);
   return lit;
}

/* The XR thread, each headset frame, from each hand's own locate: its
 * dot on a live quad; in edit mode, in the layers left, the outlines
 * (screens' under the screens, but rims a hinge pairs), the hinge lines
 * and the chosen item's tag; then the beams, as Laser Beam says. */
static unsigned input_openxr_frame_layers(void *user, XrTime time,
      const video_xr_quad_set_t *set, XrSpace space,
      const XrCompositionLayerBaseHeader **layers, unsigned cap,
      unsigned *under)
{
   unsigned h;
   XrSwapchain cursor;
   video_xr_pose_t head;
   video_xr_pose_t aim;
   video_xr_vec3_t o[INPUT_OPENXR_HANDS];
   video_xr_vec3_t d[INPUT_OPENXR_HANDS];
   float reach[INPUT_OPENXR_HANDS];
   bool hit[INPUT_OPENXR_HANDS];
   int pointed[INPUT_OPENXR_HANDS];
   int ray_of[INPUT_OPENXR_HANDS];
   bool live, tagged, beaming;
   const openxr_grabs_t *g;
   unsigned lit;
   unsigned hide[VIDEO_XR_MAX_SLOTS];
   video_xr_vec3_t from[OPENXR_HINGE_LAYERS];
   video_xr_vec3_t to[OPENXR_HINGE_LAYERS];
   unsigned lines     = 0;
   unsigned shown     = 0;
   unsigned n         = 0;
   unsigned dots      = 0;
   unsigned rays      = 0;
   input_openxr_t *st = &input_openxr_st;
   int config         = retro_atomic_load_acquire_int(
         &input_openxr_pointer_config);
   int laser          = config & INPUT_OPENXR_LASER_MASK;
   bool menu_open     = (config & INPUT_OPENXR_MENU_OPEN) != 0;
   unsigned beam      = ((unsigned)config >> INPUT_OPENXR_BEAM_SHIFT)
      & INPUT_OPENXR_BEAM_MASK;
   (void)user;

   *under = 0;
   if (st->frame)
      input_openxr_frame_aims(st, config, space, time);
   if (     !st->frame || laser == VIDEO_OPENXR_LASER_OFF
         || !openxr_session_focused(st->frame->session)
         || !(cursor = openxr_frame_cursor(st->frame)))
      return 0;
   for (h = 0; h < INPUT_OPENXR_HANDS; h++)
   {
      pointed[h] = -1;
      ray_of[h]  = -1;
   }
   for (h = 0; h < INPUT_OPENXR_HANDS && dots < cap; h++)
   {
      int q;
      float u, v, size;
      video_xr_quad_t on;
      float t                   = 0.0f;
      video_xr_pose_t pose;
      XrCompositionLayerQuad *l = &st->cursors[dots];
      if (!input_openxr_frame_pose(st, h, space, time, &aim))
         continue;
      input_openxr_aim_ray(&aim, &o[rays], &d[rays]);
      q           = (laser == VIDEO_XR_LIVE_EDIT)
         ? video_xr_pick_edit(set,
               ((st->frame->grabs.stuck >> h) & 1u) != 0, &o[rays],
               &d[rays], &u, &v, &t, NULL)
         : video_xr_pick(set, (unsigned)laser, menu_open, &o[rays],
               &d[rays], &u, &v, &t);
      hit[rays]   = q >= 0;
      reach[rays] = (q >= 0) ? t : VIDEO_XR_BEAM_REACH;
      ray_of[h]   = (int)rays;
      rays++;
      if (q < 0)
         continue;
      pointed[h] = (int)set->quads[q].slot;
      on         = set->quads[q];
      /* On a back, facing the head as the back does. */
      if ((st->frame->backs >> q) & 1u)
      {
         video_xr_turn_back(&on.pose);
         u = 1.0f - u;
      }
      size       = video_xr_cursor(&on, u, v, t, &pose);
      l->type                             = XR_TYPE_COMPOSITION_LAYER_QUAD;
      l->next                             = NULL;
      l->subImage.imageRect.offset.x      = 0;
      l->subImage.imageRect.offset.y      = VIDEO_XR_CURSOR_Y;
      l->subImage.imageArrayIndex         = 0;
      l->layerFlags                       =
         XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
      l->space                            = space;
      l->eyeVisibility                    = XR_EYE_VISIBILITY_BOTH;
      l->subImage.swapchain               = cursor;
      l->subImage.imageRect.extent.width  = VIDEO_XR_CURSOR_DIM;
      l->subImage.imageRect.extent.height = VIDEO_XR_CURSOR_DIM;
      l->pose.orientation.x               = pose.orientation.x;
      l->pose.orientation.y               = pose.orientation.y;
      l->pose.orientation.z               = pose.orientation.z;
      l->pose.orientation.w               = pose.orientation.w;
      l->pose.position.x                  = pose.position.x;
      l->pose.position.y                  = pose.position.y;
      l->pose.position.z                  = pose.position.z;
      l->size.width                       = size;
      l->size.height                      = size;
      dots++;
   }
   if (laser == VIDEO_XR_LIVE_EDIT && cap > OPENXR_KEPT_EXTRA_LAYERS)
   {
      lines  = input_openxr_hinges(st->frame, set, hide, from, to,
            &shown);
      lit    = input_openxr_edit_lit(&st->frame->grabs, pointed);
      n      = input_openxr_outlines(st, set, space, cursor, lit, hide,
            false, layers, 0, cap - OPENXR_KEPT_EXTRA_LAYERS);
      *under = n;
      n      = input_openxr_outlines(st, set, space, cursor, lit, hide,
            true, layers, n, cap - OPENXR_KEPT_EXTRA_LAYERS);
      if (lines > cap - OPENXR_KEPT_EXTRA_LAYERS - n)
         lines = cap - OPENXR_KEPT_EXTRA_LAYERS - n;
   }
   /* The lines in what the outlines leave, the tag in what they leave,
    * the beams in what it leaves; drawn lines and beams first, so the
    * tag is over them. */
   g       = &st->frame->grabs;
   live    = input_openxr_any_live(set, (unsigned)laser, menu_open);
   tagged  = live && laser == VIDEO_XR_LIVE_EDIT
      && g->tag < INPUT_OPENXR_HANDS && ray_of[g->tag] >= 0
      && n + lines + OPENXR_KEPT_EXTRA_LAYERS < cap
      && openxr_frame_cursor_w(st->frame) >= VIDEO_XR_CURSOR_TAGGED_W;
   beaming = live && rays
      && n + lines + (tagged ? 1u : 0u) + OPENXR_KEPT_EXTRA_LAYERS < cap
      && (beam == VIDEO_OPENXR_LASER_BEAM_ALWAYS
         || (beam == VIDEO_OPENXR_LASER_BEAM_EDIT
            && laser == VIDEO_XR_LIVE_EDIT));
   if (     (lines || tagged || beaming)
         && input_openxr_locate(st, st->frame->session->view_space, space,
            time, &head))
   {
      for (h = 0; h < lines; h++)
         layers[n++] = input_openxr_hinge_layer(&st->hinge_lines[h],
               &from[h], &to[h], &head.position, space, cursor,
               !((shown >> h) & 1u));
      if (beaming)
         for (h = 0; h < rays && n + (tagged ? 1u : 0u)
               + OPENXR_KEPT_EXTRA_LAYERS < cap; h++)
            layers[n++] = input_openxr_beam_layer(&st->beams[h], &o[h],
                  &d[h], reach[h], hit[h], &head.position, space, cursor);
      if (tagged)
         layers[n++] = input_openxr_tag_layer(&st->tag, &o[ray_of[g->tag]],
               &head.position, (g->chosen < VIDEO_XR_TAGS) ? g->chosen : 0,
               space, cursor);
   }
   for (h = 0; h < dots; h++)
      layers[n++] = (const XrCompositionLayerBaseHeader*)&st->cursors[h];
   return n;
}

/* The XR thread: a hand's aim pose at a frame's time, for what it
 * holds; edit mode's smoothed as Edit Smoothing says. */
static bool input_openxr_frame_aim(void *user, unsigned hand,
      XrSpace space, XrTime time, video_xr_pose_t *pose)
{
   (void)user;
   return hand < INPUT_OPENXR_HANDS
      && input_openxr_frame_pose(&input_openxr_st, hand, space, time,
            pose);
}

/* Rumble into each hand's haptics: in Combined player 1's strong motor
 * is the left hand and its weak the right; in Separate each player's
 * hand takes the stronger of its two. Paused, the refresh would keep
 * the rumble going for the whole pause: stop it until resumed. */
static void input_openxr_haptics(input_openxr_t *st, bool separate,
      bool paused)
{
   unsigned h;
   retro_time_t now = cpu_features_get_time_usec();
   for (h = 0; h < INPUT_OPENXR_HANDS; h++)
   {
      XrHapticActionInfo info;
      XrResult res;
      float amp;
      int s;
      if (paused)
         s = 0;
      else if (separate)
      {
         unsigned both = (unsigned)retro_atomic_load_acquire_int(
               &input_openxr_rumble[h]);
         s             = (int)MAX(both >> 16, both & 0xffff);
      }
      else
      {
         unsigned both = (unsigned)retro_atomic_load_acquire_int(
               &input_openxr_rumble[0]);
         s             = (int)(h ? (both & 0xffff) : (both >> 16));
      }
      amp = (float)s / 65535.0f;
      if (st->haptic_retry[h])
      {
         if (amp == st->haptic_attempt[h]
               && now - st->haptic_time[h] < INPUT_OPENXR_HAPTIC_REFRESH)
            continue;
      }
      else if (amp == st->haptic[h]
            && (amp <= 0.0f
               || now - st->haptic_time[h] < INPUT_OPENXR_HAPTIC_REFRESH))
         continue;
      info.type          = XR_TYPE_HAPTIC_ACTION_INFO;
      info.next          = NULL;
      info.action        = st->actions[IXA_RUMBLE];
      info.subactionPath = st->hands[h];
      if (amp > 0.0f)
      {
         XrHapticVibration vib;
         vib.type      = XR_TYPE_HAPTIC_VIBRATION;
         vib.next      = NULL;
         vib.duration  = INPUT_OPENXR_HAPTIC_DURATION;
         vib.frequency = XR_FREQUENCY_UNSPECIFIED;
         vib.amplitude = amp;
         res = st->ApplyHapticFeedback(st->session, &info,
               (const XrHapticBaseHeader*)&vib);
      }
      else
         res = st->StopHapticFeedback(st->session, &info);
      st->haptic_attempt[h] = amp;
      st->haptic_time[h]    = now;
      /* NOT_FOCUSED is positive, but the runtime discards the request. */
      st->haptic_retry[h]   = XR_FAILED(res) || res == XR_SESSION_NOT_FOCUSED;
      if (!st->haptic_retry[h])
         st->haptic[h] = amp;
   }
}

/* The pad in @slot, or -1. */
static int input_openxr_pad_of(unsigned slot)
{
   unsigned p;
   for (p = 0; p < INPUT_OPENXR_PADS; p++)
      if (input_openxr_devs.slot[p] == (int)slot)
         return (int)p;
   return -1;
}

/* The same, from any thread: the slots as the main thread last gave
 * them. */
static int input_openxr_pad_of_any(unsigned slot)
{
   unsigned p;
   unsigned slots = (unsigned)retro_atomic_load_acquire_int(
         &input_openxr_rumble_slots);
   for (p = 0; p < INPUT_OPENXR_PADS; p++)
      if (((slots >> (p * 8)) & 0xff) == slot + 1)
         return (int)p;
   return -1;
}

static void input_openxr_slots_publish(void)
{
   unsigned p, slots = 0;
   for (p = 0; p < INPUT_OPENXR_PADS; p++)
      if (input_openxr_devs.slot[p] >= 0)
         slots |= ((unsigned)input_openxr_devs.slot[p] + 1) << (p * 8);
   retro_atomic_store_release_int(&input_openxr_rumble_slots, (int)slots);
}

static const char *input_openxr_device_name(unsigned p)
{
   if (p)
      return INPUT_OPENXR_JOYPAD_RIGHT;
   return input_openxr_devs.count < 2 ? INPUT_OPENXR_JOYPAD_COMBINED
      : INPUT_OPENXR_JOYPAD_LEFT;
}

/* Pad @p's device reported in its slot. Its layout is RetroArch's own,
 * the runtime binding the controllers, so its profile loads with
 * autodetect off too. */
static void input_openxr_announce(unsigned p)
{
   input_openxr_devs.landed[p] = false;
   input_autoconfigure_connect_ex(input_openxr_device_name(p), NULL, NULL,
         INPUT_OPENXR_JOYPAD, (unsigned)input_openxr_devs.slot[p], 0, 0,
         AUTOCONF_FLAG_AUTOCONFIG_ENABLED);
}

/* The lowest slot no controller has, as a pad plugged in gets. */
static int input_openxr_free_slot(void)
{
   unsigned s;
   for (s = 0; s < MAX_USERS; s++)
      if (!input_config_get_device_name(s) && input_openxr_pad_of(s) < 0)
         return (int)s;
   return -1;
}

/* A device for each pad the session's controllers make: none without
 * them or once the session has ended, one in Combined, a hand each in
 * Separate. */
static void input_openxr_devices(unsigned max_users, bool separate)
{
   unsigned p;
   input_openxr_devices_t *dv = &input_openxr_devs;
   input_openxr_t *st         = &input_openxr_st;
   /* an ended session keeps its controllers until the driver goes */
   bool ended                 = st->frame
      && openxr_session_ended(st->frame->session);
   unsigned want              = (!input_openxr_active() || ended) ? 0
      : separate ? 2 : 1;
   /* a connect still to come may be for a slot that looks free */
   bool pending               = input_autoconfigure_pending();

   /* Gone once its connect has landed, so the registry is told of the
    * one before the other. */
   for (p = want; p < INPUT_OPENXR_PADS; p++)
   {
      dv->lost[p] = -1;
      if (dv->slot[p] >= 0 && dv->landed[p])
      {
         input_autoconfigure_disconnect((unsigned)dv->slot[p],
               input_openxr_device_name(p));
         dv->slot[p]   = -1;
         dv->landed[p] = false;
      }
   }
   if (!want)
   {
      dv->warned = false;
      input_openxr_slots_publish();
      return;
   }
   /* A mode switch renames the first in place, so the left hand keeps
    * the pair's port. */
   if (want != dv->count)
   {
      dv->count = want;
      if (dv->slot[0] >= 0)
         input_openxr_announce(0);
   }

   for (p = 0; p < want; p++)
   {
      int s = dv->slot[p];
      if (s >= 0)
      {
         /* forgotten, as a config load forgets */
         if (     dv->landed[p]
               && !input_config_get_device_autoconfigured((unsigned)s))
            input_openxr_announce(p);
         continue;
      }
      if (pending)
         continue;
      if ((s = input_openxr_free_slot()) < 0)
      {
         if (!dv->warned)
            RARCH_WARN("[OpenXR] No free slot for the headset's controllers.\n");
         dv->warned = true;
         continue;
      }
      dv->slot[p] = s;
      /* a pad took its slot: the port goes where it went, for this run */
      if (dv->lost[p] >= 0)
         input_driver_exchange_ports((unsigned)dv->lost[p], (unsigned)s);
      dv->lost[p] = -1;
      input_openxr_announce(p);
      if ((unsigned)s >= max_users && !dv->warned)
      {
         RARCH_WARN("[OpenXR] The headset's controllers are in slot %d, past Max Users (%u).\n",
               s, max_users);
         dv->warned = true;
      }
   }
   input_openxr_slots_publish();
}

/* The joypad driver readers get while the devices have slots: theirs
 * answered from their pads, the RetroPad's ids as buttons, the sticks
 * as axes 0 to 3 and L2 and R2 as 4 and 5, every other slot by the
 * driver under them. */
static const input_openxr_pad_t *input_openxr_slot_pad(unsigned slot)
{
   int p = input_openxr_pad_of(slot);
   return p >= 0 ? &input_openxr_st.pads[p] : NULL;
}

static int16_t input_openxr_pad_axis(const input_openxr_pad_t *pad,
      unsigned axis)
{
   if (axis < 4)
      return pad->analog[axis];
   return pad->trigger[axis - 4];
}

static bool input_openxr_joypad_query(unsigned pad)
{
   if (input_openxr_slot_pad(pad))
      return true;
   return input_openxr_inner->query_pad
      && input_openxr_inner->query_pad(pad);
}

static int32_t input_openxr_joypad_button(unsigned port, uint16_t joykey)
{
   const input_openxr_pad_t *pad = input_openxr_slot_pad(port);
   if (!pad)
      return input_openxr_inner->button
         ? input_openxr_inner->button(port, joykey) : 0;
   return joykey < RARCH_FIRST_CUSTOM_BIND
      && (pad->buttons & (1u << joykey));
}

static int16_t input_openxr_joypad_axis(unsigned port, uint32_t joyaxis)
{
   int16_t v;
   const input_openxr_pad_t *pad = input_openxr_slot_pad(port);
   if (!pad)
      return input_openxr_inner->axis
         ? input_openxr_inner->axis(port, joyaxis) : 0;
   if (AXIS_NEG_GET(joyaxis) < 6)
   {
      v = input_openxr_pad_axis(pad, AXIS_NEG_GET(joyaxis));
      return v < 0 ? v : 0;
   }
   if (AXIS_POS_GET(joyaxis) < 6)
   {
      v = input_openxr_pad_axis(pad, AXIS_POS_GET(joyaxis));
      return v > 0 ? v : 0;
   }
   return 0;
}

static int16_t input_openxr_joypad_state(rarch_joypad_info_t *joypad_info,
      const uint16_t *joykeys, const uint32_t *joyaxes, unsigned port)
{
   int i;
   int16_t ret       = 0;
   uint16_t port_idx = joypad_info->joy_idx;

   if (!input_openxr_slot_pad(port_idx))
      return input_openxr_inner->state ? input_openxr_inner->state(
            joypad_info, joykeys, joyaxes, port) : 0;
   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      const uint64_t joykey  = joykeys[i];
      const uint32_t joyaxis = joyaxes[i];
      if (     (uint16_t)joykey != NO_BTN
            && input_openxr_joypad_button(port_idx, (uint16_t)joykey))
         ret |= (1 << i);
      else if (joyaxis != AXIS_NONE &&
            ((float)abs(input_openxr_joypad_axis(port_idx, joyaxis))
             / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }
   return ret;
}

static void input_openxr_joypad_get_buttons(unsigned port,
      input_bits_t *state)
{
   unsigned id;
   const input_openxr_pad_t *pad = input_openxr_slot_pad(port);
   if (!pad)
   {
      input_openxr_inner->get_buttons(port, state);
      return;
   }
   BIT256_CLEAR_ALL_PTR(state);
   for (id = 0; id < RARCH_FIRST_CUSTOM_BIND; id++)
      if (pad->buttons & (1u << id))
         BIT256_SET_PTR(state, id);
}

/* A core's rumble, applied at the next poll. */
static bool input_openxr_joypad_rumble(unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   int p;
   if (pad >= MAX_USERS
         || (effect != RETRO_RUMBLE_STRONG && effect != RETRO_RUMBLE_WEAK))
      return false;
   p = input_openxr_pad_of_any(pad);
   if (p < 0)
      return input_openxr_inner->set_rumble
         && input_openxr_inner->set_rumble(pad, effect, strength);
   {
      /* one half changed, the other kept: rumble may come from any
       * thread */
      unsigned shift = (effect == RETRO_RUMBLE_STRONG) ? 16 : 0;
      int old, now;
      do
      {
         old = retro_atomic_load_relaxed_int(&input_openxr_rumble[p]);
         now = (int)(((unsigned)old & ~(0xffffu << shift))
             | ((unsigned)strength << shift));
      } while (!retro_atomic_cas_int(&input_openxr_rumble[p], old, now));
   }
   return input_openxr_active();
}

static const char *input_openxr_joypad_name(unsigned pad)
{
   int p = input_openxr_pad_of(pad);
   if (p >= 0)
      return input_openxr_device_name((unsigned)p);
   return input_openxr_inner->name ? input_openxr_inner->name(pad) : NULL;
}

/* Whether set shows slot's item, and as one. */
static bool input_openxr_edit_shown(const video_xr_quad_set_t *set,
      unsigned slot)
{
   unsigned i;
   if (set->slots[slot].fixed)
      return false;
   for (i = 0; i < set->num_quads; i++)
      if (set->quads[i].slot == slot)
         return true;
   return false;
}

/* The item after chosen, step 1, or before it, -1: the shown items by
 * slot, so the screens by number and the menu last, then the laser
 * (-1). */
static int input_openxr_edit_cycle(const video_xr_quad_set_t *set,
      int chosen, int step)
{
   unsigned n;
   int i = (chosen < 0) ? VIDEO_XR_MAX_SLOTS : chosen;
   for (n = 0; n < VIDEO_XR_MAX_SLOTS; n++)
   {
      i = (i + step + VIDEO_XR_MAX_SLOTS + 1) % (VIDEO_XR_MAX_SLOTS + 1);
      if (i == VIDEO_XR_MAX_SLOTS)
         return -1;
      if (input_openxr_edit_shown(set, (unsigned)i))
         return i;
   }
   return -1;
}

/* What hand h's grip and Reset act on, as an index into st->quads: the
 * chosen item, wherever it is, unless the other hand holds it; else
 * what h's laser is on. -1: nothing. */
static int input_openxr_edit_target(const input_openxr_t *st,
      const input_openxr_edit_t *e, unsigned h)
{
   unsigned i;
   const video_xr_grab_t *other = &e->hand[h ^ 1].g;
   if (     e->chosen < 0
         || (other->state == VIDEO_XR_GRAB_HELD
            && other->slot == (unsigned)e->chosen))
      return st->hit[h];
   for (i = 0; i < st->quads.num_quads; i++)
      if (st->quads.quads[i].slot == (unsigned)e->chosen)
         return (int)i;
   return -1;
}

/* A held item where its hand last put it, both eyes of a stereo screen. */
static void input_openxr_edit_place(video_xr_quad_set_t *set,
      const video_xr_grab_t *g)
{
   unsigned i;
   for (i = 0; i < set->num_quads; i++)
      if (set->quads[i].slot == g->slot)
      {
         set->quads[i].pose   = g->last;
         set->quads[i].width  = g->width;
         set->quads[i].height = g->height;
      }
}

/* An item's place, where the menu's rows write it. */
static void input_openxr_edit_write(input_openxr_edit_t *e,
      const input_openxr_hold_t *hold, const video_screen_xr_t *values)
{
   if (hold->g.slot == VIDEO_XR_MENU_SLOT)
      config_set_video_menu_xr(values);
   else
      config_set_video_screen_xr(hold->g.slot, values);
   e->changed = true;
}

/* The slots the hands hold, as bits: nothing snaps to them. */
static unsigned input_openxr_edit_holding(const input_openxr_edit_t *e)
{
   unsigned h;
   unsigned bits = 0;
   for (h = 0; h < INPUT_OPENXR_HANDS; h++)
      if (     e->hand[h].g.state == VIDEO_XR_GRAB_HELD
            && e->hand[h].g.slot < VIDEO_XR_MAX_SLOTS)
         bits |= 1u << e->hand[h].g.slot;
   return bits;
}

/* hold's item where it shows as the other hand takes a screen that
 * pulled it, the screens in was held before: so it does not hop, and
 * carries on from there free of that pull. */
static void input_openxr_edit_rebase(input_openxr_edit_t *e,
      input_openxr_hold_t *hold, const video_xr_quad_set_t *targets,
      unsigned was)
{
   video_xr_pose_t at, now;
   if (hold->g.state != VIDEO_XR_GRAB_HELD || !targets)
      return;
   video_xr_grab_pose(&hold->g, &hold->aim, targets, was, &hold->start,
         &at, NULL, NULL);
   video_xr_grab_pose(&hold->g, &hold->aim, targets,
         input_openxr_edit_holding(e), &hold->start, &now, NULL, NULL);
   if (memcmp(&at, &now, sizeof(at)))
      video_xr_grab_put(&hold->g, &hold->aim, &at);
}

/* The screens no hand holds that rest in a hinge, for the frame loop:
 * counted again only when one came, went, moved or was let go, or edit
 * mode began. */
static void input_openxr_edit_rests(input_openxr_edit_t *e,
      const video_xr_quad_set_t *set)
{
   unsigned i;
   unsigned skip  = input_openxr_edit_holding(e);
   unsigned shown = 0;
   bool due       = e->rests_due;
   for (i = 0; i < set->num_quads; i++)
   {
      const video_xr_quad_t *q = &set->quads[i];
      video_xr_quad_t *at;
      if (     q->slot >= VIDEO_XR_MENU_SLOT || set->slots[q->slot].fixed
            || (((skip | shown) >> q->slot) & 1u))
         continue;
      shown |= 1u << q->slot;
      at     = &e->rest_at[q->slot];
      if (     memcmp(&at->pose, &q->pose, sizeof(q->pose))
            || at->width != q->width || at->height != q->height)
      {
         *at = *q;
         due = true;
      }
   }
   if (shown != e->rest_shown)
      due = true;
   e->rest_shown = shown;
   e->rests_due  = false;
   if (due)
      e->grabs.num_rests = (e->on && e->snap)
         ? video_xr_rest_hinges(set, skip, e->gap * 0.01f, e->grabs.rests,
               OPENXR_REST_HINGES)
         : 0;
}

/* A buzz on hand h, duration ns at amp. */
static void input_openxr_edit_tick(input_openxr_t *st, unsigned h,
      XrDuration duration, float amp)
{
   XrHapticActionInfo info;
   XrHapticVibration vib;
   info.type          = XR_TYPE_HAPTIC_ACTION_INFO;
   info.next          = NULL;
   info.action        = st->actions[IXA_RUMBLE];
   info.subactionPath = st->hands[h];
   vib.type           = XR_TYPE_HAPTIC_VIBRATION;
   vib.next           = NULL;
   vib.duration       = duration;
   vib.frequency      = XR_FREQUENCY_UNSPECIFIED;
   vib.amplitude      = amp;
   st->ApplyHapticFeedback(st->session, &info,
         (const XrHapticBaseHeader*)&vib);
}

/* The snaps whose buzz may still fire after a move that snapped: one
 * that took hold is spent until its axis, or the gap, pulls no more. */
static unsigned input_openxr_edit_armed(unsigned armed, unsigned snapped)
{
   const unsigned buzz = VIDEO_XR_SNAP_ALL | VIDEO_XR_SNAP_GAP;
   unsigned pulling    = (snapped >> VIDEO_XR_SNAP_PULL_SHIFT) & buzz;
   return (armed & ~snapped & buzz) | (buzz & ~pulling);
}

/* The hinge a let-go of hold's item, where aim puts it now, settles in
 * on targets: hinge if it does (NULL: none), else its mark if that
 * does; NULL: neither. */
static const video_xr_hinge_t *input_openxr_edit_settles(
      const input_openxr_edit_t *e, const input_openxr_hold_t *hold,
      const video_xr_pose_t *aim, const video_xr_quad_set_t *targets,
      const video_xr_hinge_t *hinge)
{
   video_xr_pose_t pose;
   if (     hinge
         && video_xr_grab_settle(&hold->g, aim, targets,
            input_openxr_edit_holding(e), hinge, &pose))
      return hinge;
   if (     hold->buzzed
         && video_xr_grab_settle(&hold->g, aim, targets,
            input_openxr_edit_holding(e), &hold->mark, &pose))
      return &hold->mark;
   return NULL;
}

/* hold's trail gains, dated by the frame its aim is from, where its
 * item shows now, where aim puts it unsnapped, and whether a let-go
 * there settles in its hinge, or else its mark, on targets (NULL:
 * none); its newest's frame again replaces it. */
static void input_openxr_edit_trail(const input_openxr_edit_t *e,
      input_openxr_hold_t *hold, XrTime frame,
      const video_xr_pose_t *aim, const video_xr_quad_set_t *targets)
{
   input_openxr_trail_t *at;
   const video_xr_hinge_t *h;
   const input_openxr_trail_t *newest = &hold->trail[hold->trail_at];
   const input_openxr_trail_t *before = &hold->trail[(hold->trail_at
         + INPUT_OPENXR_EDIT_TRAIL - 1) % INPUT_OPENXR_EDIT_TRAIL];
   if (     !hold->trails
         || (frame > newest->t && (hold->trails < 2
               || newest->t - before->t
                  >= (XrTime)INPUT_OPENXR_EDIT_TRAIL_US * 1000)))
   {
      hold->trail_at = (hold->trail_at + 1) % INPUT_OPENXR_EDIT_TRAIL;
      if (hold->trails < INPUT_OPENXR_EDIT_TRAIL)
         hold->trails++;
   }
   at              = &hold->trail[hold->trail_at];
   at->t           = frame;
   at->shown       = hold->g.last;
   at->width       = hold->g.width;
   at->height      = hold->g.height;
   at->scale       = hold->scale;
   at->settle_slot = 0;
   at->settle_on   = 0;
   at->settle_edge = 0;
   video_xr_grab_pose(&hold->g, aim, NULL, 0, NULL, &at->hand, NULL,
         NULL);
   h = input_openxr_edit_settles(e, hold, aim, targets,
         hold->hinged ? &hold->lid : NULL);
   if (h)
   {
      at->settle_slot = (unsigned char)(h->slot + 1);
      at->settle_on   = (unsigned char)h->on;
      at->settle_edge = (unsigned char)h->edge;
   }
}

/* Whether hold's trail entry at follows a stall in st's frames. */
static bool input_openxr_edit_stalled(input_openxr_t *st,
      const input_openxr_hold_t *hold, unsigned at)
{
   unsigned before   = (at + INPUT_OPENXR_EDIT_TRAIL - 1)
      % INPUT_OPENXR_EDIT_TRAIL;
   XrDuration period = openxr_frame_period(st->frame);
   if (period < (XrDuration)INPUT_OPENXR_EDIT_TRAIL_US * 2000)
      period = (XrDuration)INPUT_OPENXR_EDIT_TRAIL_US * 2000;
   return (double)(hold->trail[at].t - hold->trail[before].t)
      > INPUT_OPENXR_EDIT_GAP * (double)period;
}

/* hold's newest trail entry INPUT_OPENXR_EDIT_REWIND_US before frame,
 * else its oldest, but never past a stall in st's frames. */
static const input_openxr_trail_t *input_openxr_edit_rewind(
      input_openxr_t *st, const input_openxr_hold_t *hold, XrTime frame)
{
   unsigned i;
   unsigned at = hold->trail_at;
   for (i = 1; i < hold->trails && frame - hold->trail[at].t
         < (XrTime)INPUT_OPENXR_EDIT_REWIND_US * 1000; i++)
   {
      if (input_openxr_edit_stalled(st, hold, at))
         break;
      at = (at + INPUT_OPENXR_EDIT_TRAIL - 1) % INPUT_OPENXR_EDIT_TRAIL;
   }
   return &hold->trail[at];
}

/* hold's item put at pose as large as its trail's entry at shows it
 * (NULL: as large as now), with no snap, from its last aim. */
static void input_openxr_edit_put(input_openxr_edit_t *e,
      input_openxr_hold_t *hold, const input_openxr_trail_t *at,
      const video_xr_pose_t *pose)
{
   video_screen_xr_t values;
   if (at)
   {
      hold->g.width  = at->width;
      hold->g.height = at->height;
      hold->scale    = at->scale;
   }
   video_xr_grab_put(&hold->g, &hold->aim, pose);
   video_views_xr_reset(&values, 1);
   video_xr_grab_pose(&hold->g, &hold->aim, NULL, 0, NULL,
         &hold->g.last, &values, NULL);
   values.scale = hold->scale;
   input_openxr_edit_write(e, hold, &values);
}

/* Where hold's item, let go while its mark shows, settles in it on
 * targets: from its trail's entry *at if a let-go there settles in the
 * mark, else from the newest entry where one does, *at then that. */
static bool input_openxr_edit_settle(const input_openxr_edit_t *e,
      const input_openxr_hold_t *hold, const video_xr_quad_set_t *targets,
      const input_openxr_trail_t **at, video_xr_pose_t *pose)
{
   unsigned i;
   unsigned n                       = hold->trail_at;
   const input_openxr_trail_t *from = *at;
   if (!hold->buzzed || !targets)
      return false;
   for (i = 0; i <= hold->trails; i++)
   {
      if (     from->settle_slot == hold->mark.slot + 1u
            && from->settle_on == hold->mark.on
            && from->settle_edge == hold->mark.edge)
      {
         if (!video_xr_hinge_settle(targets, hold->g.slot,
                  input_openxr_edit_holding(e) & ~(1u << hold->g.slot),
                  from->width, from->height, e->gap * 0.01f, &hold->mark,
                  &from->hand, pose))
            return false;
         *at = from;
         return true;
      }
      from = &hold->trail[n];
      n    = (n + INPUT_OPENXR_EDIT_TRAIL - 1) % INPUT_OPENXR_EDIT_TRAIL;
   }
   return false;
}

/* hold's mark, if buzzed, in its grab for the frame loop. */
static void input_openxr_edit_show_mark(input_openxr_hold_t *hold)
{
   hold->g.mark_slot = hold->buzzed
      ? (unsigned char)(hold->mark.slot + 1) : 0;
   hold->g.mark_on   = hold->buzzed ? (unsigned char)hold->mark.on : 0;
   hold->g.mark_edge = hold->buzzed ? (unsigned char)hold->mark.edge : 0;
}

/* Hand h grabs st->quads' quad target at when (us), aim from frame:
 * the item keeps its place in the aim's frame, wherever the hand is,
 * its yaw from the aim's heading, and its pitch and roll. A second hand
 * on an item does nothing. */
static void input_openxr_edit_grab(input_openxr_t *st,
      input_openxr_edit_t *e, unsigned h, int target,
      const video_xr_pose_t *aim, bool tilt, retro_time_t when,
      XrTime frame)
{
   unsigned was;
   video_xr_pose_t inv;
   video_screen_xr_t now;
   const video_xr_quad_t *q    = &st->quads.quads[target];
   const video_xr_vec3_t *base = &st->quads.slots[q->slot].base;
   input_openxr_hold_t *hold   = &e->hand[h];
   input_openxr_hold_t *other  = &e->hand[h ^ 1];
   if (other->g.state == VIDEO_XR_GRAB_HELD && other->g.slot == q->slot)
      return;
   was = input_openxr_edit_holding(e);
   memset(&hold->g, 0, sizeof(hold->g));
   video_xr_pose_invert(aim, &inv);
   video_xr_compose(&inv, &q->pose, &hold->g.rel);
   video_xr_grab_hold(&hold->g, aim, &q->pose.orientation);
   hold->g.last   = q->pose;
   hold->g.base   = *base;
   hold->g.width  = q->width;
   hold->g.height = q->height;
   hold->g.slot   = q->slot;
   hold->g.state  = VIDEO_XR_GRAB_HELD;
   hold->g.tilt   = tilt;
   hold->g.gap    = e->gap;
   hold->start    = q->pose;
   hold->aim      = *aim;
   hold->armed    = VIDEO_XR_SNAP_ALL | VIDEO_XR_SNAP_GAP;
   /* Taken at a step, it buzzes only once it has left it. */
   hold->rest[0]  = 0.0f;
   hold->rest[1]  = 0.0f;
   hold->steps    = 0;
   hold->pushed   = false;
   hold->hinged   = false;
   hold->lidded   = false;
   hold->buzzed   = false;
   /* Taken where it rests hinged, it carries that hinge from the start,
    * its buzz and its snaps' spent: nothing new took hold. */
   if (     e->snap && q->slot < VIDEO_XR_MENU_SLOT
         && video_xr_rest_hinge(&st->quads, q->slot,
               input_openxr_edit_holding(e), q->width, q->height,
               e->gap * 0.01f, &q->pose, &hold->lid))
   {
      video_xr_rest_take(&st->quads, q->slot, q->width, q->height,
            e->gap * 0.01f, &hold->lid, &hold->start);
      video_xr_compose(&inv, &hold->start, &hold->g.rel);
      hold->g.rest_slot = (unsigned char)(hold->lid.slot + 1);
      hold->g.rest_on   = (unsigned char)hold->lid.on;
      hold->g.rest_edge = (unsigned char)hold->lid.edge;
      hold->armed       = 0;
      hold->lidded      = true;
      hold->buzzed      = true;
      hold->mark        = hold->lid;
      hold->out         = 0;
      input_openxr_edit_show_mark(hold);
   }
   if (q->slot != VIDEO_XR_MENU_SLOT)
      config_get_video_screen_xr(q->slot, &now);
   else if (!config_get_video_menu_xr(&now))
   {
      /* Unplaced, it is sized by its distance against its spot's:
       * placed, that scale keeps it as large. */
      const video_xr_vec3_t *p = &q->pose.position;
      double spot              = sqrt((double)base->x * base->x
            + (double)base->y * base->y + (double)base->z * base->z);
      if (spot > 0.0)
         now.scale = (float)(sqrt((double)p->x * p->x
                  + (double)p->y * p->y + (double)p->z * p->z) / spot);
      video_views_xr_normalize(&now);
   }
   hold->scale   = now.scale;
   hold->scale0  = now.scale;
   hold->width0  = q->width;
   hold->height0 = q->height;
   hold->trails  = 0;
   hold->polled  = when;
   input_openxr_edit_trail(e, hold, frame, aim, NULL);
   /* Taken hinged, a let-go settles in that hinge from the start. */
   hold->trail[hold->trail_at].settle_slot = hold->g.rest_slot;
   hold->trail[hold->trail_at].settle_on   = hold->g.rest_on;
   hold->trail[hold->trail_at].settle_edge = hold->g.rest_edge;
   input_openxr_edit_rebase(e, other, e->snap ? &st->quads : NULL, was);
}

/* Whether hinge takes hold anew: not the one hold's mark shows. */
static bool input_openxr_edit_attach(const input_openxr_hold_t *hold,
      const video_xr_hinge_t *hinge)
{
   return !hold->buzzed || hold->mark.slot != hinge->slot
      || hold->mark.on != hinge->on || hold->mark.edge != hinge->edge;
}

/* A held item to where the hand puts it, stepped by the free stick and
 * snapped to targets unless NULL. Returns the snaps engaged or pulling
 * (video_xr_snap); *stepped: a step took hold with its buzz armed;
 * *attached: its mark took hold anew. */
static unsigned input_openxr_edit_move(input_openxr_edit_t *e,
      input_openxr_hold_t *hold, const video_xr_pose_t *aim,
      const video_xr_vec3_t *head, bool tilt, const int16_t *stick,
      const int16_t *free_stick, double dt,
      const video_xr_quad_set_t *targets, bool *stepped, bool *attached,
      retro_time_t now)
{
   unsigned snapped, axes, held;
   video_xr_pose_t inv, pose;
   video_screen_xr_t values;
   video_xr_hinge_t hinge;
   const video_xr_hinge_t *c;
   bool hinged = false;
   float x = (float)free_stick[0] / 32767.0f;
   float y = -(float)free_stick[1] / 32767.0f;
   bool pushed = x * x + y * y
      > VIDEO_XR_EDIT_DEADZONE * VIDEO_XR_EDIT_DEADZONE;
   *attached   = false;
   hold->aim   = *aim;
   video_xr_pose_invert(aim, &inv);
   if (tilt && !hold->g.tilt)
      video_xr_compose(&inv, &hold->g.last, &hold->g.rel);
   else if (!tilt && hold->g.tilt)
      video_xr_grab_hold(&hold->g, aim, &hold->g.last.orientation);
   hold->g.tilt = tilt;
   /* The stick's Y is down, as the RetroPad's. */
   video_xr_grab_stick(&hold->g, (float)stick[0] / 32767.0f,
         -(float)stick[1] / 32767.0f, dt, &hold->scale);
   hold->g.width  = hold->width0 * hold->scale / hold->scale0;
   hold->g.height = hold->height0 * hold->scale / hold->scale0;
   /* Only a stick let go leaves the item where it shows: a wavering
    * push that drops an axis for a poll would stay at its step. */
   if (!pushed)
   {
      hold->rest[0] = 0.0f;
      hold->rest[1] = 0.0f;
   }
   /* A hinge the snap holds now: the stick swings about its edge. An
    * engaged turn's angle sticks as that stick lets go or starts a
    * push, and a hinge's place on its line with it, so the turn or
    * swing goes on from there; never by carrying alone, which can pull
    * it out, nor a fading pull, which would hop it into the snap. */
   if (targets)
   {
      snapped = video_xr_grab_hinge(&hold->g, aim, targets,
            input_openxr_edit_holding(e), &hold->start, &pose, &hinge);
      hinged  = (snapped & VIDEO_XR_SNAP_HINGE) != 0;
      /* Ends as a carried hinge does: out of reach (once the fade is
       * whole), another hinge taking hold, or the other hand taking its
       * target. */
      if (hold->g.rest_slot)
      {
         video_xr_hinge_t kept;
         if (     !video_xr_grab_kept(&hold->g, aim, targets, &hold->start,
                     &kept)
               || ((input_openxr_edit_holding(e) >> kept.slot) & 1u)
               || (hinged && (hinge.slot != kept.slot
                     || hinge.on != kept.on || hinge.edge != kept.edge)))
            hold->g.rest_slot = 0;
      }
      /* The one state the buzz, lid, rims and line follow: a hinge a
       * let-go would settle into takes it at once; seen out of that
       * range INPUT_OPENXR_HINGE_DEBOUNCE_US on end, it lets go, so a
       * shaking hand never flickers it. */
      c = input_openxr_edit_settles(e, hold, aim, targets,
            hinged ? &hinge : NULL);
      *attached = c && input_openxr_edit_attach(hold, c);
      if (*attached)
      {
         hold->mark   = *c;
         hold->buzzed = true;
      }
      if (c)
         hold->out = 0;
      else if (hold->buzzed)
      {
         retro_time_t gap = now - hold->polled;
         if (!hold->out)
            hold->out = now;
         else if (gap > INPUT_OPENXR_HINGE_OUT_STEP_US)
            hold->out += gap - INPUT_OPENXR_HINGE_OUT_STEP_US;
         if (now - hold->out >= INPUT_OPENXR_HINGE_DEBOUNCE_US)
            hold->buzzed = false;
      }
      input_openxr_edit_show_mark(hold);
      if (pushed != hold->pushed)
      {
         if (hinged)
            video_xr_grab_put_hinge(&hold->g, aim, &hinge, &pose,
                  (snapped & VIDEO_XR_SNAP_TURN) != 0);
         else if (snapped & VIDEO_XR_SNAP_TURN)
         {
            if (hold->g.tilt)
            {
               video_xr_compose(&inv, &pose, &pose);
               hold->g.rel.orientation = pose.orientation;
            }
            else
               video_xr_grab_hold(&hold->g, aim, &pose.orientation);
         }
      }
      /* Mid-push, a hinge taking hold puts its edge on the line, its
       * angles the hand's: the turn about the middle when it drops
       * would carry it off. */
      else if (pushed && hinged && !hold->hinged)
         video_xr_grab_put_line(&hold->g, aim, &hinge);
   }
   hold->pushed = pushed;
   /* Hinged, the steps count from flat: across a change, the stick's
    * carry over a step counts no more. */
   if (hinged != hold->hinged)
   {
      hold->rest[0] = 0.0f;
      hold->rest[1] = 0.0f;
      hold->hinged  = hinged;
   }
   /* As the snaps' buzz, armed by where the item is, however it got
    * there. */
   if (hinged)
   {
      hold->lid       = hinge;
      hold->lidded    = true;
      hold->g.through = 0;
      hold->steps |= (video_xr_grab_off_step(&hold->g, aim, head, e->step,
               hold->rest) & ~hinge.axis)
         | video_xr_grab_off_hinge(&hold->g, aim, &hinge, e->step,
               hold->rest);
      axes = video_xr_grab_turn_hinge(&hold->g, aim, &hinge, x, y, dt);
      held = video_xr_grab_step_hinge(&hold->g, aim, &hinge, e->step,
            axes, hold->rest, targets, input_openxr_edit_holding(e),
            &hold->start);
   }
   else
   {
      const video_xr_hinge_t *lid = NULL;
      /* Pushed past the stop's hold, the hinge ends with it, and the
       * stop holds short until one takes hold, so a wobble there
       * neither takes it back nor buzzes. While marked, as its mark
       * keeps that far off the plane too. */
      if (     hold->buzzed
            && video_xr_grab_through(&hold->g, aim, targets, &hold->mark))
         hold->g.through = 1;
      /* Out of its hinge's reach, its mark gone, it turns as any free
       * screen. */
      if (     hold->lidded && !hold->buzzed
            && !video_xr_grab_near_hinge(&hold->g, aim, targets,
               &hold->lid, VIDEO_XR_SNAP_DIST))
         hold->lidded = false;
      if (hold->lidded)
         lid = &hold->lid;
      hold->steps |= video_xr_grab_off_step(&hold->g, aim, head, e->step,
            hold->rest);
      axes = video_xr_grab_turn_lid(&hold->g, aim, lid, x, y, dt);
      held = video_xr_grab_step(&hold->g, aim, head, e->step, axes,
            hold->rest, targets, input_openxr_edit_holding(e),
            &hold->start, lid);
   }
   *stepped       = (held & hold->steps) != 0;
   hold->steps   &= ~held;
   video_views_xr_reset(&values, 1);
   if (video_xr_grab_pose(&hold->g, aim, targets,
            input_openxr_edit_holding(e), &hold->start, &pose, &values,
            &snapped))
      video_xr_compose(&inv, &pose, &hold->g.rel);
   hold->g.last = pose;
   values.scale = hold->scale;
   input_openxr_edit_write(e, hold, &values);
   return snapped;
}

/* A hold ends. Let go, the item stays where the hand left it, which its
 * settings hold already, and the count rides the next frame snapshot;
 * the frame loop shows it there until quads planned at that count do,
 * even once the hand holds another. Gone from the quads, nothing is
 * left to show. */
static void input_openxr_edit_let_go(input_openxr_edit_t *e,
      input_openxr_hold_t *hold, bool shown)
{
   hold->buzzed = false;
   hold->out    = 0;
   hold->trails = 0;
   input_openxr_edit_show_mark(hold);
   if (!shown)
   {
      hold->g.state = VIDEO_XR_GRAB_NONE;
      return;
   }
   e->seq++;
   hold->g.release_seq           = e->seq;
   hold->g.state                 = VIDEO_XR_GRAB_RELEASED;
   e->grabs.let_go[hold->g.slot] = hold->g;
   video_driver_headset_edit(e->on, e->seq);
}

/* What the hands hold and where they took it, the chosen item and
 * Snap to Screens, for the frame loop. */
static void input_openxr_edit_publish(input_openxr_t *st,
      input_openxr_edit_t *e)
{
   if (!st->frame)
      return;
   e->grabs.hand[0]  = e->hand[0].g;
   e->grabs.hand[1]  = e->hand[1].g;
   e->grabs.start[0] = e->hand[0].start;
   e->grabs.start[1] = e->hand[1].start;
   e->grabs.chosen   = (unsigned)(e->chosen + 1);
   e->grabs.stuck    = e->stuck;
   e->grabs.tag      = e->tag;
   e->grabs.snap     = e->snap;
   e->grabs.editing  = e->on;
   openxr_frame_publish_grabs(st->frame, &e->grabs);
}

/* Every hold let go where it is, or settled in its mark. */
static void input_openxr_edit_let_go_all(input_openxr_edit_t *e)
{
   unsigned h;
   bool let_go = false;
   for (h = 0; h < INPUT_OPENXR_HANDS; h++)
      if (e->hand[h].g.state == VIDEO_XR_GRAB_HELD)
      {
         video_xr_pose_t pose;
         input_openxr_hold_t *hold      = &e->hand[h];
         const input_openxr_trail_t *at = &hold->trail[hold->trail_at];
         if (input_openxr_edit_settle(e, hold,
                  e->snap ? &input_openxr_st.quads : NULL, &at, &pose))
            input_openxr_edit_put(e, hold, at, &pose);
         input_openxr_edit_let_go(e, hold, true);
         input_openxr_edit_place(&input_openxr_st.quads, &hold->g);
         let_go = true;
      }
   if (let_go)
      input_openxr_edit_rests(e, &input_openxr_st.quads);
   if (let_go || e->on)
      input_openxr_edit_publish(&input_openxr_st, e);
}

/* The main button on an item no hand holds: a screen as Reset Screen
 * leaves it, the menu back where it opens unplaced. */
static void input_openxr_edit_reset(input_openxr_edit_t *e, unsigned slot)
{
   unsigned h;
   for (h = 0; h < INPUT_OPENXR_HANDS; h++)
      if (     e->hand[h].g.state == VIDEO_XR_GRAB_HELD
            && e->hand[h].g.slot == slot)
         return;
   if (slot == VIDEO_XR_MENU_SLOT)
      config_set_video_menu_xr(NULL);
   else
      config_set_video_screen_xr(slot, NULL);
   e->changed = true;
}

/* Edit mode's start: the content paused if While Editing Screens asks
 * and nothing paused it already, held screens snapping if Snap to
 * Screens asks, and the desktop told. Video hears of it first, so the
 * frame the pause draws shows edit mode. */
static void input_openxr_edit_start(input_openxr_edit_t *e, bool pause,
      bool snap, float step, unsigned char gap, unsigned smoothing)
{
   unsigned h;
   const char *msg = msg_hash_to_str(MSG_OPENXR_EDIT_ON);
   uint32_t flags  = runloop_get_flags();
   e->on      = true;
   e->snap    = snap;
   e->step    = step;
   e->gap     = gap;
   e->smoothing = smoothing;
   e->changed = false;
   e->unbound = 0;
   e->paused  = false;
   e->last    = 0;
   e->chosen  = -1;
   e->stuck   = 0;
   e->tag     = 0;
   e->rests_due = true;
   /* Held as it starts (the menu's OK on A, which is Reset): no press
    * until let go. */
   for (h = 0; h < INPUT_OPENXR_HANDS; h++)
   {
      e->hand[h].grip   = true;
      e->hand[h].reset  = true;
      e->hand[h].buzzed = false;
      e->hand[h].out    = 0;
      e->hand[h].trails = 0;
   }
   e->next = true;
   e->prev = true;
   RARCH_LOG("[OpenXR] Edit Screens on.\n");
   video_driver_headset_edit(true, e->seq);
   if (     pause
         && (flags & RUNLOOP_FLAG_CORE_RUNNING)
         && !(flags & RUNLOOP_FLAG_PAUSED))
   {
      input_driver_platform_request(INPUT_PLATFORM_PAUSE);
      e->paused = (runloop_get_flags() & RUNLOOP_FLAG_PAUSED) != 0;
   }
   /* After the pause's own notice, which would clear it; long enough
    * to read all the controls. */
   runloop_msg_queue_push(msg, strlen(msg), 1, 480, true, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
}

/* Its end: what the hands hold let go, the content resumed only if edit
 * mode paused it and it still is, and the desktop told whether settings
 * changed. */
static void input_openxr_edit_stop(input_openxr_edit_t *e)
{
   const char *msg = msg_hash_to_str(e->changed
         ? MSG_OPENXR_EDIT_UNSAVED : MSG_OPENXR_EDIT_OFF);
   e->chosen = -1;
   e->stuck  = 0;
   e->tag    = 0;
   input_openxr_edit_let_go_all(e);
   e->on = false;
   /* No hinges at rest outside edit mode, from its next frame. */
   e->grabs.num_rests = 0;
   input_openxr_edit_publish(&input_openxr_st, e);
   if (e->paused && (runloop_get_flags() & RUNLOOP_FLAG_PAUSED))
      input_driver_platform_request(INPUT_PLATFORM_UNPAUSE);
   e->paused = false;
   RARCH_LOG("[OpenXR] Edit Screens off.\n");
   runloop_msg_queue_push(msg, strlen(msg), 1, 180, true, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   video_driver_headset_edit(false, e->seq);
}

/* Edit mode's poll, in the screens' frame: Select Next and Previous
 * choose an item; a grip grabs it, or what the hand points at, and the
 * other hand's stick turns it; the main button resets it. The menu and
 * Edit Screens come from its own set; nothing reaches the pads. */
static void input_openxr_edit_poll(input_openxr_t *st,
      input_openxr_edit_t *e)
{
   unsigned h;
   bool next, prev;
   video_xr_pose_t screens, inv;
   openxr_aims_t aims;
   XrSpace space    = XR_NULL_HANDLE;
   XrTime time      = openxr_frame_predicted_time(st->frame);
   retro_time_t now = cpu_features_get_time_usec();
   double dt        = (e->last && now > e->last)
      ? (double)(now - e->last) / 1000000.0 : 0.0;

   e->last             = now;
   if (dt > INPUT_OPENXR_EDIT_MAX_DT)
      dt = INPUT_OPENXR_EDIT_MAX_DT;
   st->start_menu_held = false;
   input_openxr_triggers_lost(st);
   st->menu_toggle     = input_openxr_bool(st, IXA_E_MENU, XR_NULL_PATH);
   if (st->menu_toggle)
      st->start_menu_spent = true;
   st->edit_toggle     = input_openxr_bool(st, IXA_E_EDIT, XR_NULL_PATH);
   next                = input_openxr_bool(st, IXA_E_NEXT, XR_NULL_PATH);
   prev                = input_openxr_bool(st, IXA_E_PREV, XR_NULL_PATH);
   openxr_frame_get_quads(st->frame, &st->quads);
   openxr_frame_get_screens(st->frame, &screens, &space);
   video_xr_pose_invert(&screens, &inv);
   /* Edit Smoothing: the aims the frame loop built its last frame
    * from, so what is let go is where it was shown. */
   memset(&aims, 0, sizeof(aims));
   if (e->smoothing)
      openxr_frame_get_aims(st->frame, &aims);
   /* Items where the headset shows them: let go and not yet in the
    * quads, as the frame loop holds them; held, where the hand put
    * them. */
   for (h = 0; h < VIDEO_XR_MAX_SLOTS; h++)
   {
      const video_xr_grab_t *g = &e->grabs.let_go[h];
      if (     g->state == VIDEO_XR_GRAB_RELEASED
            && !st->quads.slots[h].fixed
            && (int)(st->quads.slots[h].edit_seq - g->release_seq) < 0)
         input_openxr_edit_place(&st->quads, g);
   }
   for (h = 0; h < INPUT_OPENXR_HANDS; h++)
      if (e->hand[h].g.state == VIDEO_XR_GRAB_HELD)
         input_openxr_edit_place(&st->quads, &e->hand[h].g);
   /* The choice: the laser again once its item is gone; a step each
    * press. */
   if (     e->chosen >= 0
         && !input_openxr_edit_shown(&st->quads, (unsigned)e->chosen))
      e->chosen = -1;
   /* The tag goes to the hand that pressed, the left if both did. */
   if (next && !e->next)
   {
      e->chosen = input_openxr_edit_cycle(&st->quads, e->chosen, 1);
      e->tag    = input_openxr_bool(st, IXA_E_NEXT, st->hands[0]) ? 0 : 1;
   }
   if (prev && !e->prev)
   {
      e->chosen = input_openxr_edit_cycle(&st->quads, e->chosen, -1);
      e->tag    = input_openxr_bool(st, IXA_E_PREV, st->hands[0]) ? 0 : 1;
   }
   e->next = next;
   e->prev = prev;

   for (h = 0; h < INPUT_OPENXR_HANDS; h++)
   {
      float u, v, t;
      int target;
      bool sticks;
      int16_t stick[2];
      video_xr_pose_t aim;
      video_xr_vec3_t o, d;
      const video_xr_quad_set_t *targets = e->snap ? &st->quads : NULL;
      input_openxr_hold_t *hold = &e->hand[h];
      XrPath hand               = st->hands[h];
      bool grip                 = input_openxr_bool(st, IXA_E_GRAB, hand);
      bool reset                = input_openxr_bool(st, IXA_E_RESET, hand);
      bool tilt                 = input_openxr_bool(st, IXA_E_TILT, hand);
      bool tracked              = e->smoothing
         ? ((aims.valid >> h) & 1u) != 0
         : input_openxr_aim(st, h, space, time, &aim);
      bool grabbed              = grip && !hold->grip;
      bool reset_pressed        = reset && !hold->reset;
      /* The frame its aim is from, which dates its trail. */
      XrTime frame              = e->smoothing ? aims.time[h] : time;

      if (e->smoothing && tracked)
         aim = aims.pose[h];
      hold->grip  = grip;
      hold->reset = reset;
      sticks      = false;
      if (tracked)
      {
         video_xr_compose(&inv, &aim, &aim);
         input_openxr_aim_ray(&aim, &o, &d);
         st->hit[h] = video_xr_pick_edit(&st->quads,
               ((e->stuck >> h) & 1u) != 0, &o, &d, &u, &v, &t, &sticks);
      }
      e->stuck = sticks ? (e->stuck | (1u << h)) : (e->stuck & ~(1u << h));
      target = input_openxr_edit_target(st, e, h);
      if (hold->g.state == VIDEO_XR_GRAB_HELD)
      {
         const video_xr_vec3_t *base = &st->quads.slots[hold->g.slot].base;
         /* Its spot moved (the core's layout changed): the hold's
          * settings are from the old one, so it ends as if gone. */
         bool shown = input_openxr_edit_shown(&st->quads, hold->g.slot)
            && base->x == hold->g.base.x && base->y == hold->g.base.y
            && base->z == hold->g.base.z;
         /* Its mark's target gone or taken by the other hand, the mark
          * ends at once, the hand tracked or not, so letting that go
          * takes it again only anew, with the buzz. */
         if (     hold->buzzed
               && (!input_openxr_edit_shown(&st->quads, hold->mark.slot)
                  || ((input_openxr_edit_holding(e) >> hold->mark.slot)
                     & 1u)))
         {
            hold->buzzed = false;
            hold->out    = 0;
            input_openxr_edit_show_mark(hold);
         }
         /* Its lid's target taken ends the lid too, hinged elsewhere or
          * not. */
         if (     hold->lidded
               && ((input_openxr_edit_holding(e) >> hold->lid.slot) & 1u))
            hold->lidded = false;
         if (!grip || !shown)
         {
            if (shown)
            {
               video_xr_pose_t pose;
               const input_openxr_trail_t *at;
               /* A new frame's aims first, as that frame shows it. */
               if (tracked && frame > hold->trail[hold->trail_at].t)
               {
                  video_xr_grab_hinge(&hold->g, &aim, targets,
                        input_openxr_edit_holding(e), &hold->start, &pose,
                        NULL);
                  hold->g.last = pose;
                  input_openxr_edit_trail(e, hold, frame, &aim, targets);
               }
               /* As it showed before the grip began to open: the hand
                * twitches as it does. */
               at = input_openxr_edit_rewind(st, hold, frame);
               /* Exactly into the hinge it showed, so it rests there
                * green, among the screens held once it is let go. */
               if (input_openxr_edit_settle(e, hold, targets, &at, &pose))
                  input_openxr_edit_put(e, hold, at, &pose);
               /* Smoothed, where it shows: smoothing damps the twitch,
                * and a rewind would step back by what it lags. */
               else if (e->smoothing)
               {
                  pose = hold->g.last;
                  input_openxr_edit_put(e, hold, NULL, &pose);
               }
               else
                  input_openxr_edit_put(e, hold, at, &at->shown);
            }
            input_openxr_edit_let_go(e, hold, shown);
            /* Where it rests now, for the hinges counted below. */
            if (shown)
               input_openxr_edit_place(&st->quads, &hold->g);
         }
         /* Untracked, the item holds still. */
         else if (tracked)
         {
            unsigned snapped;
            bool stepped;
            bool attached;
            int16_t free_stick[2];
            video_xr_pose_t head;
            const video_xr_vec3_t *at = NULL;
            stick[0]      = 0;
            stick[1]      = 0;
            free_stick[0] = 0;
            free_stick[1] = 0;
            input_openxr_stick(st, IXA_E_STICK, hand, stick);
            /* The other hand's stick turns and tilts it while that hand
             * holds nothing. */
            if (e->hand[h ^ 1].g.state != VIDEO_XR_GRAB_HELD)
               input_openxr_stick(st, IXA_E_STICK, st->hands[h ^ 1],
                     free_stick);
            if (input_openxr_locate(st, st->frame->session->view_space,
                     space, time, &head))
            {
               video_xr_compose(&inv, &head, &head);
               at = &head.position;
            }
            snapped = input_openxr_edit_move(e, hold, &aim, at, tilt, stick,
                  free_stick, dt, targets, &stepped, &attached, now);
            /* A new apply cuts the one playing short: ticks due inside
             * the hinge's buzz are skipped, their snaps still spent. */
            if (attached)
            {
               input_openxr_edit_tick(st, h, INPUT_OPENXR_HINGE_BUZZ_NS,
                     INPUT_OPENXR_HINGE_BUZZ_AMP);
               hold->buzz_end = now + INPUT_OPENXR_HINGE_BUZZ_NS / 1000;
            }
            else if (((snapped & hold->armed) || stepped)
                  && now >= hold->buzz_end)
               input_openxr_edit_tick(st, h, INPUT_OPENXR_SNAP_TICK_NS,
                     INPUT_OPENXR_SNAP_TICK_AMP);
            hold->armed = input_openxr_edit_armed(hold->armed, snapped);
            hold->polled = now;
            input_openxr_edit_trail(e, hold, frame, &aim, targets);
         }
      }
      /* Taken from anywhere, the hand's aim is needed: untracked, it
       * takes nothing. */
      else if (grabbed && tracked && target >= 0)
         input_openxr_edit_grab(st, e, h, target, &aim, tilt, now, frame);
      else if (reset_pressed && target >= 0)
         input_openxr_edit_reset(e, st->quads.quads[target].slot);
   }
   input_openxr_edit_rests(e, &st->quads);
   input_openxr_edit_publish(st, e);
}

void input_openxr_poll(unsigned max_users, unsigned controllers,
      unsigned laser, unsigned beam, float threshold)
{
   unsigned p;
   XrResult res;
   XrActionsSyncInfo si;
   XrActiveActionSet active[2];
   input_openxr_t *st      = &input_openxr_st;
   input_openxr_edit_t *e  = &input_openxr_edit;
   uint32_t run_flags      = runloop_get_flags();
   bool separate           = controllers == VIDEO_OPENXR_CONTROLLERS_SEPARATE;
   bool was_pressed        = st->gun_pressed || st->menu_pressed;
#ifdef HAVE_MENU
   bool menu_open          = menu_driver_alive();
#else
   bool menu_open          = false;
#endif

   /* A config can hold any value: past the laser's choices is Auto, and
    * never edit mode's; past the beam's, Edit Mode Only. */
   if (laser >= VIDEO_OPENXR_LASER_LAST)
      laser = VIDEO_OPENXR_LASER_AUTO;
   if (beam >= VIDEO_OPENXR_LASER_BEAM_LAST)
      beam = VIDEO_OPENXR_LASER_BEAM_EDIT;

   input_openxr_devices(max_users, separate);

   /* A pause edit mode made that the user lifted is no longer its own. */
   if (e->paused && !(run_flags & RUNLOOP_FLAG_PAUSED))
      e->paused = false;

   /* Edit mode ends with the session and when the menu opens. It
    * starts only between frames (input_openxr_edit_begin()): with late
    * polling this poll is inside the core's frame, where a pause would
    * draw one. A resume draws none, and paused content runs no frame. */
   if (     e->on
         && (menu_open || !st->frame
            || openxr_session_ended(st->frame->session)))
      input_openxr_edit_stop(e);

   /* The XR thread places the dots by these: none while the laser
    * yields, which a closed menu or the laser Off ends; on every item
    * while editing, whatever the setting. */
   if (!menu_open || laser == VIDEO_OPENXR_LASER_OFF)
      st->yield = false;
   retro_atomic_store_release_int(&input_openxr_pointer_config,
         (e->on ? VIDEO_XR_LIVE_EDIT
         : ((st->yield ? VIDEO_OPENXR_LASER_OFF : (int)laser)
            | (menu_open ? INPUT_OPENXR_MENU_OPEN : 0)))
         | ((int)beam << INPUT_OPENXR_BEAM_SHIFT)
         | (e->on ? (int)(e->smoothing & INPUT_OPENXR_SMOOTH_MASK)
            << INPUT_OPENXR_SMOOTH_SHIFT : 0));

   /* Nothing stops the last rumble of a core closed behind a menu that
    * ran it; no later content may inherit it. */
   if (!(run_flags & RUNLOOP_FLAG_CORE_RUNNING))
      for (p = 0; p < INPUT_OPENXR_PADS; p++)
         retro_atomic_store_release_int(&input_openxr_rumble[p], 0);

   memset(st->pads, 0, sizeof(st->pads));
   memset(st->trig, 0, sizeof(st->trig));
   st->menu_toggle  = false;
   st->recenter     = false;
   st->laser_toggle = false;
   st->view_toggle  = false;
   st->edit_toggle  = false;
   st->hit[0]       = -1;
   st->hit[1]       = -1;
   st->ptr_owned    = false;
   st->ptr_on       = false;
   st->ptr_pressed  = false;
   st->gun_pressed  = false;
   st->menu_on      = false;
   st->menu_pressed = false;
   if (!st->frame)
   {
      st->start_menu_held = false;
      return;
   }
   if (!openxr_session_focused(st->frame->session))
   {
      if (st->focused)
         RARCH_LOG("[OpenXR] Controllers released: the headset is not focused.\n");
      st->focused = false;
      st->start_menu_held = false;
      input_openxr_triggers_lost(st);
      /* So their return is logged too. */
      st->mode    = -1;
      /* The runtime drops haptics out of focus: apply again after. */
      st->haptic[0] = 0.0f;
      st->haptic[1] = 0.0f;
      st->haptic_retry[0] = false;
      st->haptic_retry[1] = false;
      return;
   }
   st->focused = true;

   active[0].actionSet      = st->sets[e->on ? INPUT_OPENXR_SET_EDIT
      : (separate ? INPUT_OPENXR_SET_SEPARATE : INPUT_OPENXR_SET_COMBINED)];
   active[0].subactionPath  = XR_NULL_PATH;
   active[1].actionSet      = st->sets[INPUT_OPENXR_SET_POINTER];
   active[1].subactionPath  = XR_NULL_PATH;
   si.type                  = XR_TYPE_ACTIONS_SYNC_INFO;
   si.next                  = NULL;
   si.countActiveActionSets = 2;
   si.activeActionSets      = active;
   /* XR_SESSION_NOT_FOCUSED succeeds too, with nothing active. */
   if ((res = st->SyncActions(st->session, &si)) != XR_SUCCESS)
   {
      if (res != st->sync_result && XR_FAILED(res))
         RARCH_WARN("[OpenXR] Controllers not read: xrSyncActions %d.\n",
               (int)res);
      st->sync_result = res;
      st->start_menu_held = false;
      input_openxr_triggers_lost(st);
      return;
   }
   st->sync_result = XR_SUCCESS;
   if (st->mode != (int)separate)
   {
      RARCH_LOG("[OpenXR] Controllers: %s.\n",
            separate ? "separate" : "combined");
      st->mode = (int)separate;
   }
   /* The runtime may bind none of edit mode's actions (a binding the
    * user saved before they existed): nothing would grab or leave. */
   if (e->on)
   {
      if (     input_openxr_bound(st, IXA_E_GRAB)
            || input_openxr_bound(st, IXA_E_MENU))
         e->unbound = 0;
      else if (++e->unbound >= INPUT_OPENXR_EDIT_UNBOUND)
      {
         input_openxr_edit_stop(e);
         input_openxr_edit_unavailable(st, "the controllers' bindings "
               "have none of its actions");
      }
   }
   if (e->on)
      input_openxr_edit_poll(st, e);
   else
   {
      if (separate)
         input_openxr_read_separate(st);
      else
         input_openxr_read_combined(st);
      input_openxr_yield(st, laser, menu_open, threshold);
      if (st->yield)
         laser = VIDEO_OPENXR_LASER_OFF;
      retro_atomic_store_release_int(&input_openxr_pointer_config,
            (int)laser | (menu_open ? INPUT_OPENXR_MENU_OPEN : 0)
            | ((int)beam << INPUT_OPENXR_BEAM_SHIFT));
      input_openxr_laser_poll(st, laser, menu_open, threshold,
            was_pressed);
      input_openxr_triggers(st, threshold);
   }
   input_openxr_haptics(st, separate,
         (run_flags & RUNLOOP_FLAG_PAUSED) ? true : false);
}

const input_device_driver_t *input_openxr_joypad(
      const input_device_driver_t *joypad)
{
   input_device_driver_t *wrap;
   if (     !joypad
         || (input_openxr_devs.slot[0] < 0 && input_openxr_devs.slot[1] < 0))
      return joypad;
   if (joypad != input_openxr_inner)
   {
      input_openxr_wrap_cur ^= 1;
      wrap                   = &input_openxr_wrap[input_openxr_wrap_cur];
      *wrap                  = *joypad;
      wrap->init             = NULL;
      wrap->destroy          = NULL;
      wrap->query_pad        = input_openxr_joypad_query;
      wrap->button           = input_openxr_joypad_button;
      wrap->state            = input_openxr_joypad_state;
      wrap->axis             = input_openxr_joypad_axis;
      wrap->set_rumble       = input_openxr_joypad_rumble;
      wrap->name             = input_openxr_joypad_name;
      if (joypad->get_buttons)
         wrap->get_buttons   = input_openxr_joypad_get_buttons;
      input_openxr_inner     = joypad;
   }
   return &input_openxr_wrap[input_openxr_wrap_cur];
}

/* From any thread: input_driver_set_rumble()'s. */
bool input_openxr_has_slot(unsigned slot)
{
   return input_openxr_pad_of_any(slot) >= 0;
}

void input_openxr_slot_connected(unsigned slot, const char *provider)
{
   int p = input_openxr_pad_of(slot);
   if (p < 0)
      return;
   if (string_is_equal(provider, INPUT_OPENXR_JOYPAD))
      input_openxr_devs.landed[p] = true;
   else
   {
      /* the pad's now; the device moves at the next poll */
      input_openxr_devs.lost[p]   = (int)slot;
      input_openxr_devs.slot[p]   = -1;
      input_openxr_devs.landed[p] = false;
      input_openxr_slots_publish();
   }
}

void input_openxr_reannounce(void)
{
   unsigned p;
   for (p = 0; p < INPUT_OPENXR_PADS; p++)
      if (input_openxr_devs.slot[p] >= 0)
         input_openxr_announce(p);
}

bool input_openxr_autoconfigure(unsigned slot)
{
   int p = input_openxr_pad_of(slot);
   if (p < 0)
      return false;
   input_openxr_announce((unsigned)p);
   return true;
}

bool input_openxr_hotkey(unsigned id)
{
   const input_openxr_t *st = &input_openxr_st;
   if (id == RARCH_MENU_TOGGLE)
      return st->menu_toggle;
   if (id == RARCH_HEADSET_RECENTER)
      return st->recenter;
   if (id == RARCH_LASER_POINTER_TOGGLE)
      return st->laser_toggle;
   if (id == RARCH_HEADSET_VIEW_TOGGLE)
      return st->view_toggle;
   if (id == RARCH_HEADSET_EDIT_SCREENS)
      return st->edit_toggle;
   return false;
}

/* Whether a hand holds a controller whose profile refused edit mode's
 * bindings: in edit mode it could not leave it. */
static bool input_openxr_edit_refused(input_openxr_t *st)
{
   unsigned h, i;
   for (h = 0; h < INPUT_OPENXR_HANDS; h++)
   {
      XrInteractionProfileState ps;
      ps.type = XR_TYPE_INTERACTION_PROFILE_STATE;
      ps.next = NULL;
      if (XR_FAILED(st->GetCurrentInteractionProfile(st->session,
                  st->hands[h], &ps)))
         continue;
      for (i = 0; i < st->num_edit_refused; i++)
         if (ps.interactionProfile == st->edit_refused[i])
            return true;
   }
   return false;
}

bool input_openxr_edit_toggle(void)
{
   const char *why        = NULL;
   input_openxr_edit_t *e = &input_openxr_edit;
   input_openxr_t *st     = &input_openxr_st;
   if (e->on)
   {
      input_openxr_edit_stop(e);
      return false;
   }
   if (!input_openxr_active() || !st->frame)
      why = "no headset controllers";
   else if (openxr_session_ended(st->frame->session))
      why = "the session has ended";
   else if (!st->edit_ok || input_openxr_edit_refused(st))
      why = "the runtime refused its bindings";
   if (why)
   {
      input_openxr_edit_unavailable(st, why);
      return false;
   }
   return true;
}

void input_openxr_edit_begin(bool pause, bool snap, unsigned snap_angles,
      unsigned hinge_gap, unsigned smoothing)
{
   static const float steps[VIDEO_OPENXR_EDIT_SNAP_ANGLES_LAST] =
         { 0.0f, 15.0f, 30.0f, 45.0f, 90.0f };
   static const unsigned char gaps[VIDEO_OPENXR_EDIT_HINGE_GAP_LAST] =
         { 0, 1, 2, 3, 5, 10 };
   input_openxr_edit_t *e = &input_openxr_edit;
   if (e->on || !input_openxr_st.frame)
      return;
#ifdef HAVE_MENU
   /* Without content the menu cannot close, and would end it at once. */
   if (menu_driver_alive())
   {
      const char *msg = msg_hash_to_str(MSG_OPENXR_EDIT_NO_CONTENT);
      runloop_msg_queue_push(msg, strlen(msg), 1, 180, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      return;
   }
#endif
   /* A config can hold any value: past Snap to Angles', 15 degrees;
    * past Hinge Gap's, 2 cm; past Edit Smoothing's, Medium. */
   if (snap_angles >= VIDEO_OPENXR_EDIT_SNAP_ANGLES_LAST)
      snap_angles = VIDEO_OPENXR_EDIT_SNAP_ANGLES_15;
   if (hinge_gap >= VIDEO_OPENXR_EDIT_HINGE_GAP_LAST)
      hinge_gap = VIDEO_OPENXR_EDIT_HINGE_GAP_2;
   if (smoothing >= VIDEO_OPENXR_EDIT_SMOOTHING_LAST)
      smoothing = VIDEO_OPENXR_EDIT_SMOOTHING_MEDIUM;
   input_openxr_edit_start(e, pause, snap, steps[snap_angles],
         gaps[hinge_gap], smoothing);
}

void input_openxr_edit_end(void)
{
   input_openxr_edit_t *e = &input_openxr_edit;
   if (e->on)
      input_openxr_edit_stop(e);
}

void input_openxr_edit_release(void)
{
   input_openxr_edit_let_go_all(&input_openxr_edit);
}

bool input_openxr_pointer(unsigned port, unsigned device, unsigned idx,
      unsigned id, int16_t *res)
{
   const input_openxr_t *st = &input_openxr_st;
   bool on                  = st->ptr_on && idx == 0;
   if (port != 0 || !st->ptr_owned)
      return false;
   if (device == RETRO_DEVICE_POINTER)
   {
      switch (id)
      {
         case RETRO_DEVICE_ID_POINTER_X:
            *res = on ? st->ptr_x : -0x8000;
            break;
         case RETRO_DEVICE_ID_POINTER_Y:
            *res = on ? st->ptr_y : -0x8000;
            break;
         case RETRO_DEVICE_ID_POINTER_PRESSED:
         case RETRO_DEVICE_ID_POINTER_COUNT:
            *res = (on && st->ptr_pressed) ? 1 : 0;
            break;
         case RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN:
            *res = on ? 0 : 1;
            break;
         default:
            *res = 0;
            break;
      }
      return true;
   }
   if (device == RETRO_DEVICE_LIGHTGUN)
   {
      switch (id)
      {
         case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
            *res = st->ptr_on ? st->ptr_x : -0x8000;
            return true;
         case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
            *res = st->ptr_on ? st->ptr_y : -0x8000;
            return true;
         case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
            *res = st->ptr_on ? 0 : 1;
            return true;
         case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
            /* Off the screens too: the usual reload. */
            *res = st->gun_pressed ? 1 : 0;
            return true;
         default:
            break;
      }
   }
   /* The gun's other buttons and the mouse stay the input driver's. */
   return false;
}

bool input_openxr_menu_pointer(float *u, float *v, bool *pressed)
{
   const input_openxr_t *st = &input_openxr_st;
   if (!st->menu_on)
      return false;
   *u       = st->menu_u;
   *v       = st->menu_v;
   *pressed = st->menu_pressed;
   return true;
}

/* location and velocity. */
static void input_openxr_vr_pose(const XrPosef *pose, XrSpaceLocationFlags lf,
      const XrVector3f *lin, const XrVector3f *ang, XrSpaceVelocityFlags vf,
      struct retro_vr_pose *out)
{
   memset(out, 0, sizeof(*out));
   if (lf & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)
   {
      out->orientation[0] = pose->orientation.x;
      out->orientation[1] = pose->orientation.y;
      out->orientation[2] = pose->orientation.z;
      out->orientation[3] = pose->orientation.w;
      out->flags         |= RETRO_VR_POSE_ORIENTATION_VALID;
      if (lf & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT)
         out->flags      |= RETRO_VR_POSE_ORIENTATION_TRACKED;
   }
   if (lf & XR_SPACE_LOCATION_POSITION_VALID_BIT)
   {
      out->position[0]    = pose->position.x;
      out->position[1]    = pose->position.y;
      out->position[2]    = pose->position.z;
      out->flags         |= RETRO_VR_POSE_POSITION_VALID;
      if (lf & XR_SPACE_LOCATION_POSITION_TRACKED_BIT)
         out->flags      |= RETRO_VR_POSE_POSITION_TRACKED;
   }
   if (vf & XR_SPACE_VELOCITY_LINEAR_VALID_BIT)
   {
      out->linear_velocity[0]  = lin->x;
      out->linear_velocity[1]  = lin->y;
      out->linear_velocity[2]  = lin->z;
      out->flags              |= RETRO_VR_POSE_LINEAR_VELOCITY_VALID;
   }
   if (vf & XR_SPACE_VELOCITY_ANGULAR_VALID_BIT)
   {
      out->angular_velocity[0] = ang->x;
      out->angular_velocity[1] = ang->y;
      out->angular_velocity[2] = ang->z;
      out->flags              |= RETRO_VR_POSE_ANGULAR_VELOCITY_VALID;
   }
}

static void input_openxr_locate_pose(const input_openxr_t *st,
      XrSpace space, XrSpace base, XrTime time, struct retro_vr_pose *out)
{
   XrSpaceVelocity vel;
   XrSpaceLocation loc;

   memset(out, 0, sizeof(*out));
   if (!space)
      return;
   vel.type = XR_TYPE_SPACE_VELOCITY;
   vel.next = NULL;
   loc.type = XR_TYPE_SPACE_LOCATION;
   loc.next = &vel;
   if (XR_FAILED(st->LocateSpace(space, base, time, &loc)))
      return;
   input_openxr_vr_pose(&loc.pose, loc.locationFlags, &vel.linearVelocity,
         &vel.angularVelocity, vel.velocityFlags, out);
}

/* The joints are copied by index: libretro's order is OpenXR's. */
typedef char input_openxr_joint_order_check[
   (     (int)RETRO_VR_HAND_JOINT_COUNT      == (int)XR_HAND_JOINT_COUNT_EXT
      && (int)RETRO_VR_HAND_JOINT_WRIST      == (int)XR_HAND_JOINT_WRIST_EXT
      && (int)RETRO_VR_HAND_JOINT_LITTLE_TIP == (int)XR_HAND_JOINT_LITTLE_TIP_EXT)
   ? 1 : -1];

static void input_openxr_locate_joints(const input_openxr_t *st,
      unsigned h, XrSpace base, XrTime time, struct retro_vr_hand_state *out)
{
   unsigned j;
   XrHandJointsLocateInfoEXT li;
   XrHandJointLocationsEXT locs;
   XrHandJointVelocitiesEXT vels;
   XrHandTrackingDataSourceStateEXT src;
   XrHandJointLocationEXT jl[XR_HAND_JOINT_COUNT_EXT];
   XrHandJointVelocityEXT jv[XR_HAND_JOINT_COUNT_EXT];

   if (!st->trackers[h])
      return;
   li.type              = XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT;
   li.next              = NULL;
   li.baseSpace         = base;
   li.time              = time;
   src.type             = XR_TYPE_HAND_TRACKING_DATA_SOURCE_STATE_EXT;
   src.next             = NULL;
   vels.type            = XR_TYPE_HAND_JOINT_VELOCITIES_EXT;
   vels.next            = st->hand_data_source ? &src : NULL;
   vels.jointCount      = XR_HAND_JOINT_COUNT_EXT;
   vels.jointVelocities = jv;
   locs.type            = XR_TYPE_HAND_JOINT_LOCATIONS_EXT;
   locs.next            = &vels;
   locs.jointCount      = XR_HAND_JOINT_COUNT_EXT;
   locs.jointLocations  = jl;
   if (     XR_FAILED(st->LocateHandJointsEXT(st->trackers[h], &li, &locs))
         || !locs.isActive)
      return;

   for (j = 0; j < RETRO_VR_HAND_JOINT_COUNT; j++)
   {
      input_openxr_vr_pose(&jl[j].pose, jl[j].locationFlags,
            &jv[j].linearVelocity, &jv[j].angularVelocity,
            jv[j].velocityFlags, &out->joints[j].pose);
      out->joints[j].radius = jl[j].radius;
   }
   out->flags |= RETRO_VR_HAND_JOINTS;
   if (st->hand_data_source && src.isActive)
   {
      if (src.dataSource == XR_HAND_TRACKING_DATA_SOURCE_UNOBSTRUCTED_EXT)
         out->flags |= RETRO_VR_HAND_JOINTS_OPTICAL;
      else if (src.dataSource == XR_HAND_TRACKING_DATA_SOURCE_CONTROLLER_EXT)
         out->flags |= RETRO_VR_HAND_JOINTS_CONTROLLER;
   }
}

static bool input_openxr_locate_hands(void *user, XrSpace base, XrTime time,
      struct retro_vr_controller_poses *out)
{
   unsigned h;
   const input_openxr_t *st = &input_openxr_st;
   (void)user;

   memset(out, 0, sizeof(*out));
   if (!input_openxr_active())
      return false;
   for (h = 0; h < INPUT_OPENXR_HANDS; h++)
   {
      struct retro_vr_hand_state *hs = &out->hands[h];
      input_openxr_locate_pose(st, st->grip[h], base, time, &hs->grip);
      input_openxr_locate_pose(st, st->aim[h],  base, time, &hs->aim);
      input_openxr_locate_pose(st, st->palm[h], base, time, &hs->palm);
      input_openxr_locate_joints(st, h, base, time, hs);
   }
   return true;
}

static bool input_openxr_pose_active(input_openxr_t *st, unsigned a,
      XrPath hand)
{
   XrActionStateGetInfo gi;
   XrActionStatePose s;
   input_openxr_get_info(st, a, hand, &gi);
   s.type = XR_TYPE_ACTION_STATE_POSE;
   s.next = NULL;
   return XR_SUCCEEDED(st->GetActionStatePose(st->session, &gi, &s))
      && s.isActive;
}

void input_openxr_controller_poses(struct retro_vr_controller_poses *out)
{
   unsigned h;
   input_openxr_t *st = &input_openxr_st;
   /* The pose actions' states are those of the last sync. */
   bool synced        = input_openxr_active() && st->focused
      && st->sync_result == XR_SUCCESS;

   for (h = 0; h < INPUT_OPENXR_HANDS; h++)
   {
      struct retro_vr_hand_state *hs = &out->hands[h];
      bool held                      = false;

      hs->interaction_profile = NULL;
      if (synced)
      {
         XrInteractionProfileState ps;
         held = input_openxr_pose_active(st, IXA_GRIP, st->hands[h])
             || input_openxr_pose_active(st, IXA_AIM,  st->hands[h]);
         ps.type = XR_TYPE_INTERACTION_PROFILE_STATE;
         ps.next = NULL;
         if (     XR_SUCCEEDED(st->GetCurrentInteractionProfile(st->session,
                     st->hands[h], &ps))
               && ps.interactionProfile != XR_NULL_PATH)
         {
            /* The string is looked up when the profile changes. */
            if (ps.interactionProfile != st->profile_path[h])
            {
               uint32_t n = 0;
               st->profile_path[h] = XR_NULL_PATH;
               if (XR_SUCCEEDED(st->PathToString(st->instance,
                           ps.interactionProfile, sizeof(st->profile[h]),
                           &n, st->profile[h])))
                  st->profile_path[h] = ps.interactionProfile;
            }
            if (st->profile_path[h] != XR_NULL_PATH)
               hs->interaction_profile = st->profile[h];
         }
      }

      if (!held)
      {
         memset(&hs->grip, 0, sizeof(hs->grip));
         memset(&hs->aim,  0, sizeof(hs->aim));
         memset(&hs->palm, 0, sizeof(hs->palm));
      }
      if (held || (hs->flags & RETRO_VR_HAND_JOINTS))
         hs->flags |= RETRO_VR_HAND_ACTIVE;
      else
         memset(hs, 0, sizeof(*hs));
   }
}

bool input_openxr_can_rumble(void)
{
   return input_openxr_active();
}

bool input_openxr_active(void)
{
   return retro_atomic_load_acquire_int(&input_openxr_ready) != 0;
}

void input_openxr_register(void)
{
   openxr_input_hooks_t hooks;
   hooks.session_created    = input_openxr_session_created;
   hooks.session_destroying = input_openxr_session_destroying;
   hooks.frame_layers       = input_openxr_frame_layers;
   hooks.locate_hands       = input_openxr_locate_hands;
   hooks.aim                = input_openxr_frame_aim;
   hooks.user               = NULL;
   openxr_frame_set_input_hooks(&hooks);
}
