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
/* Half a stick's travel. */
#define INPUT_OPENXR_YIELD_STICK   0x4000
/* Touch's one menu button opens the RetroArch menu once held this long
 * (us). */
#define INPUT_OPENXR_MENU_HOLD     1000000

enum input_openxr_set
{
   INPUT_OPENXR_SET_COMBINED = 0,
   INPUT_OPENXR_SET_SEPARATE,
   INPUT_OPENXR_SET_POINTER,
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
   IXA_AIM,
   IXA_GRIP,
   IXA_PALM,
   IXA_RUMBLE,
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
   { INPUT_OPENXR_SET_POINTER,  "aim",         "Aim",            XR_ACTION_TYPE_POSE_INPUT,       true  },
   { INPUT_OPENXR_SET_POINTER,  "grip",        "Grip",           XR_ACTION_TYPE_POSE_INPUT,       true  },
   { INPUT_OPENXR_SET_POINTER,  "palm",        "Palm",           XR_ACTION_TYPE_POSE_INPUT,       true  },
   { INPUT_OPENXR_SET_POINTER,  "rumble",      "Rumble",         XR_ACTION_TYPE_VIBRATION_OUTPUT, true  }
};

static const char *const input_openxr_set_names[INPUT_OPENXR_SETS] = {
   "combined", "separate", "pointer" };
static const char *const input_openxr_set_labels[INPUT_OPENXR_SETS] = {
   "Combined Controllers", "Separate Controllers", "Laser and Rumble" };

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

typedef struct input_openxr_profile
{
   const char *path;
   const input_openxr_bind_t *binds;
   unsigned count;
   /* Only with XR_VALVE_frame_controller_interaction. */
   bool frame;
} input_openxr_profile_t;

static const input_openxr_profile_t input_openxr_profiles[] = {
   { "/interaction_profiles/valve/index_controller",
      input_openxr_index,  ARRAY_SIZE(input_openxr_index),  false },
   { "/interaction_profiles/oculus/touch_controller",
      input_openxr_touch,  ARRAY_SIZE(input_openxr_touch),  false },
   { "/interaction_profiles/valve/frame_controller_valve",
      input_openxr_frame,  ARRAY_SIZE(input_openxr_frame),  true  },
   { "/interaction_profiles/htc/vive_controller",
      input_openxr_vive,   ARRAY_SIZE(input_openxr_vive),   false },
   { "/interaction_profiles/khr/simple_controller",
      input_openxr_simple, ARRAY_SIZE(input_openxr_simple), false }
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
   /* Touch's menu button: held since start_menu_since. */
   bool start_menu_held;
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

   /* XR thread. */
   XrCompositionLayerQuad cursors[INPUT_OPENXR_HANDS];
} input_openxr_t;

static input_openxr_t input_openxr_st;

/* The main thread's laser mode and menu, for the XR thread's dots. */
#define INPUT_OPENXR_LASER_MASK 3
#define INPUT_OPENXR_MENU_OPEN  4
static retro_atomic_int_t input_openxr_pointer_config;

/* A core's rumble per pad, strong then weak, and whether a session's
 * controllers exist: asked from any thread. */
static retro_atomic_int_t input_openxr_rumble[INPUT_OPENXR_PADS][2];
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

/* One profile's suggestions; a runtime without the profile, or that
 * refuses a path, only loses that profile. */
static void input_openxr_suggest(input_openxr_t *st,
      const input_openxr_profile_t *p)
{
   unsigned i;
   unsigned count;
   XrResult res;
   XrPath profile = XR_NULL_PATH;
   XrActionSuggestedBinding binds[INPUT_OPENXR_MAX_BINDS
      + INPUT_OPENXR_PALM_BINDS];
   XrInteractionProfileSuggestedBinding sb;
   static const char *const palm[INPUT_OPENXR_PALM_BINDS] = {
      "/user/hand/left/input/palm_ext/pose",
      "/user/hand/right/input/palm_ext/pose" };

   if (p->count > INPUT_OPENXR_MAX_BINDS)
      return;
   if (XR_FAILED(res = st->StringToPath(st->instance, p->path, &profile)))
   {
      RARCH_WARN("[OpenXR] No path %s (%d).\n", p->path, (int)res);
      return;
   }
   for (i = 0; i < p->count; i++)
   {
      binds[i].action = st->actions[p->binds[i].action];
      if (XR_FAILED(res = st->StringToPath(st->instance, p->binds[i].path,
                  &binds[i].binding)))
      {
         RARCH_WARN("[OpenXR] No path %s (%d).\n", p->binds[i].path,
               (int)res);
         return;
      }
   }
   count = p->count;
   if (st->palm_pose)
      for (i = 0; i < INPUT_OPENXR_PALM_BINDS; i++)
      {
         binds[count].action = st->actions[IXA_PALM];
         if (XR_SUCCEEDED(st->StringToPath(st->instance, palm[i],
                     &binds[count].binding)))
            count++;
      }
   sb.type                   = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
   sb.next                   = NULL;
   sb.interactionProfile     = profile;
   sb.countSuggestedBindings = count;
   sb.suggestedBindings      = binds;
   res = st->SuggestInteractionProfileBindings(st->instance, &sb);

   if (XR_FAILED(res) && count > p->count)
   {
      sb.countSuggestedBindings = p->count;
      res = st->SuggestInteractionProfileBindings(st->instance, &sb);
   }
   if (XR_FAILED(res))
      RARCH_WARN("[OpenXR] Bindings refused for %s (%d).\n", p->path,
            (int)res);
   else
      RARCH_LOG("[OpenXR] Bindings suggested for %s%s.\n", p->path,
            sb.countSuggestedBindings > p->count ? ", with the palm" : "");
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
      if (!input_openxr_profiles[i].frame || h->frame_controller)
         input_openxr_suggest(st, &input_openxr_profiles[i]);

   /* All three are attached; each poll syncs the pad set in use and the
    * pointer set. */
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
 * menu too when let go after a second. */
static void input_openxr_start_menu(input_openxr_t *st, bool down,
      input_openxr_pad_t *pad)
{
   retro_time_t now;
   if (!down)
   {
      st->start_menu_held = false;
      return;
   }
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

/* A hand's aim ray in space at time, when it is tracked. Either
 * thread: it only reads what the session hooks wrote. */
static bool input_openxr_ray(const input_openxr_t *st, unsigned h,
      XrSpace space, XrTime time, video_xr_vec3_t *o, video_xr_vec3_t *d)
{
   XrSpaceLocation loc;
   video_xr_quat_t q;
   video_xr_vec3_t fwd;
   XrSpaceLocationFlags valid = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT
      | XR_SPACE_LOCATION_POSITION_VALID_BIT;

   loc.type          = XR_TYPE_SPACE_LOCATION;
   loc.next          = NULL;
   loc.locationFlags = 0;
   if (     !st->aim[h] || !time
         || XR_FAILED(st->LocateSpace(st->aim[h], space, time, &loc))
         || (loc.locationFlags & valid) != valid)
      return false;
   q.x   = loc.pose.orientation.x;
   q.y   = loc.pose.orientation.y;
   q.z   = loc.pose.orientation.z;
   q.w   = loc.pose.orientation.w;
   o->x  = loc.pose.position.x;
   o->y  = loc.pose.position.y;
   o->z  = loc.pose.position.z;
   /* An aim pose's ray is its -Z. */
   fwd.x = 0.0f;
   fwd.y = 0.0f;
   fwd.z = -1.0f;
   video_xr_rotate(&q, &fwd, d);
   return true;
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

/* The XR thread, each headset frame: a dot where each hand points at a
 * live quad, from its own locate at the frame's display time. */
static unsigned input_openxr_frame_layers(void *user, XrTime time,
      const XrCompositionLayerBaseHeader **layers, unsigned cap)
{
   unsigned h;
   XrSwapchain cursor;
   video_xr_quad_set_t set;
   unsigned n         = 0;
   XrSpace space      = XR_NULL_HANDLE;
   input_openxr_t *st = &input_openxr_st;
   int config         = retro_atomic_load_acquire_int(
         &input_openxr_pointer_config);
   int laser          = config & INPUT_OPENXR_LASER_MASK;
   bool menu_open     = (config & INPUT_OPENXR_MENU_OPEN) != 0;
   (void)user;

   if (     !st->frame || laser == VIDEO_OPENXR_LASER_OFF
         || !openxr_session_focused(st->frame->session)
         || !(cursor = openxr_frame_cursor(st->frame)))
      return 0;
   openxr_frame_get_shown_quads(st->frame, &set, &space);
   for (h = 0; h < INPUT_OPENXR_HANDS && n < cap; h++)
   {
      int q;
      float u, v, t, size;
      video_xr_pose_t pose;
      video_xr_vec3_t o, d;
      XrCompositionLayerQuad *l = &st->cursors[h];
      if (     !input_openxr_ray(st, h, space, time, &o, &d)
            || (q = video_xr_pick(&set, (unsigned)laser, menu_open,
                  &o, &d, &u, &v, &t)) < 0)
         continue;
      size = video_xr_cursor(&set.quads[q], u, v, t, &pose);
      l->type                             = XR_TYPE_COMPOSITION_LAYER_QUAD;
      l->next                             = NULL;
      l->subImage.imageRect.offset.x      = 0;
      l->subImage.imageRect.offset.y      = 0;
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
      layers[n++]                         =
         (const XrCompositionLayerBaseHeader*)l;
   }
   return n;
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
         int strong = retro_atomic_load_acquire_int(
               &input_openxr_rumble[h][0]);
         int weak   = retro_atomic_load_acquire_int(
               &input_openxr_rumble[h][1]);
         s          = MAX(strong, weak);
      }
      else
         s = retro_atomic_load_acquire_int(&input_openxr_rumble[0][h]);
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
   p = input_openxr_pad_of(pad);
   if (p < 0)
      return input_openxr_inner->set_rumble
         && input_openxr_inner->set_rumble(pad, effect, strength);
   retro_atomic_store_release_int(
         &input_openxr_rumble[p][(effect == RETRO_RUMBLE_STRONG) ? 0 : 1],
         (int)strength);
   return input_openxr_active();
}

static const char *input_openxr_joypad_name(unsigned pad)
{
   int p = input_openxr_pad_of(pad);
   if (p >= 0)
      return input_openxr_device_name((unsigned)p);
   return input_openxr_inner->name ? input_openxr_inner->name(pad) : NULL;
}

void input_openxr_poll(unsigned max_users, unsigned controllers,
      unsigned laser, float threshold)
{
   unsigned p;
   XrResult res;
   XrActionsSyncInfo si;
   XrActiveActionSet active[2];
   input_openxr_t *st   = &input_openxr_st;
   uint32_t run_flags   = runloop_get_flags();
   bool separate        = controllers == VIDEO_OPENXR_CONTROLLERS_SEPARATE;
   bool was_pressed     = st->gun_pressed || st->menu_pressed;
#ifdef HAVE_MENU
   bool menu_open       = menu_driver_alive();
#else
   bool menu_open       = false;
#endif

   input_openxr_devices(max_users, separate);

   /* The XR thread places the dots by these: none while the laser
    * yields, which a closed menu or the laser Off ends. */
   if (!menu_open || laser == VIDEO_OPENXR_LASER_OFF)
      st->yield = false;
   retro_atomic_store_release_int(&input_openxr_pointer_config,
         (st->yield ? VIDEO_OPENXR_LASER_OFF : (int)laser)
         | (menu_open ? INPUT_OPENXR_MENU_OPEN : 0));

   /* Nothing stops the last rumble of a core closed behind a menu that
    * ran it; no later content may inherit it. */
   if (!(run_flags & RUNLOOP_FLAG_CORE_RUNNING))
      for (p = 0; p < INPUT_OPENXR_PADS; p++)
      {
         retro_atomic_store_release_int(&input_openxr_rumble[p][0], 0);
         retro_atomic_store_release_int(&input_openxr_rumble[p][1], 0);
      }

   memset(st->pads, 0, sizeof(st->pads));
   memset(st->trig, 0, sizeof(st->trig));
   st->menu_toggle  = false;
   st->recenter     = false;
   st->laser_toggle = false;
   st->view_toggle  = false;
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

   active[0].actionSet      = st->sets[separate
      ? INPUT_OPENXR_SET_SEPARATE : INPUT_OPENXR_SET_COMBINED];
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
   if (separate)
      input_openxr_read_separate(st);
   else
      input_openxr_read_combined(st);
   input_openxr_yield(st, laser, menu_open, threshold);
   if (st->yield)
      laser = VIDEO_OPENXR_LASER_OFF;
   retro_atomic_store_release_int(&input_openxr_pointer_config,
         (int)laser | (menu_open ? INPUT_OPENXR_MENU_OPEN : 0));
   input_openxr_laser_poll(st, laser, menu_open, threshold, was_pressed);
   input_openxr_triggers(st, threshold);
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

bool input_openxr_has_slot(unsigned slot)
{
   return input_openxr_pad_of(slot) >= 0;
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
   return false;
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
   hooks.user               = NULL;
   openxr_frame_set_input_hooks(&hooks);
}
