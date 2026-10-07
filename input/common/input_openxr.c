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
#include "../../gfx/video_views.h"
#include "../../gfx/common/vulkan_openxr.h"
#include "../../gfx/video_driver.h"
#include "../../verbosity.h"

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#define INPUT_OPENXR_HANDS     2
#define INPUT_OPENXR_MAX_BINDS 48
/* A steady rumble is applied again this often (us), each time for
 * INPUT_OPENXR_HAPTIC_DURATION (ns). */
#define INPUT_OPENXR_HAPTIC_REFRESH  500000
#define INPUT_OPENXR_HAPTIC_DURATION 1000000000
/* Half a stick's travel. */
#define INPUT_OPENXR_YIELD_STICK   0x4000

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
   IXA_S_B,
   IXA_S_A,
   IXA_S_R,
   IXA_S_R2,
   IXA_S_START,
   IXA_S_STICK,
   IXA_S_MENU,
   IXA_S_RECENTER,
   IXA_S_LASER,
   IXA_AIM,
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
   { INPUT_OPENXR_SET_SEPARATE, "b",           "B",              XR_ACTION_TYPE_BOOLEAN_INPUT,    true  },
   { INPUT_OPENXR_SET_SEPARATE, "a",           "A",              XR_ACTION_TYPE_BOOLEAN_INPUT,    true  },
   { INPUT_OPENXR_SET_SEPARATE, "r",           "R",              XR_ACTION_TYPE_BOOLEAN_INPUT,    true  },
   { INPUT_OPENXR_SET_SEPARATE, "r2",          "R2",             XR_ACTION_TYPE_FLOAT_INPUT,      true  },
   { INPUT_OPENXR_SET_SEPARATE, "start",       "Start",          XR_ACTION_TYPE_BOOLEAN_INPUT,    true  },
   { INPUT_OPENXR_SET_SEPARATE, "stick",       "Stick",          XR_ACTION_TYPE_VECTOR2F_INPUT,   true  },
   { INPUT_OPENXR_SET_SEPARATE, "menu",        "RetroArch Menu", XR_ACTION_TYPE_BOOLEAN_INPUT,    false },
   { INPUT_OPENXR_SET_SEPARATE, "recenter",    "Recenter",       XR_ACTION_TYPE_BOOLEAN_INPUT,    false },
   { INPUT_OPENXR_SET_SEPARATE, "laser",       "Laser Pointer",  XR_ACTION_TYPE_BOOLEAN_INPUT,    false },
   { INPUT_OPENXR_SET_POINTER,  "aim",         "Aim",            XR_ACTION_TYPE_POSE_INPUT,       true  },
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
 * button (left), which Start takes. */
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
   IXB_L(IXA_C_START,  "menu/click"),
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
   vulkan_openxr_t *xr;
   XrInstance instance;
   XrSession session;
   XrSpace local_space;
   XrPath hands[INPUT_OPENXR_HANDS];
   XrActionSet sets[INPUT_OPENXR_SETS];
   XrAction actions[IXA_COUNT];
   XrSpace aim[INPUT_OPENXR_HANDS];
   PFN_xrStringToPath StringToPath;
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
   PFN_xrLocateSpace LocateSpace;
   PFN_xrApplyHapticFeedback ApplyHapticFeedback;
   PFN_xrStopHapticFeedback StopHapticFeedback;

   /* Main thread. */
   input_openxr_pad_t pads[INPUT_OPENXR_PADS];
   input_openxr_trigger_t trig[INPUT_OPENXR_HANDS];
   XrResult sync_result; /* the last xrSyncActions */
   int mode;             /* the controllers mode last logged, or -1 */
   bool focused;
   bool menu_toggle;
   bool recenter;
   bool laser_toggle;
   float haptic[INPUT_OPENXR_HANDS];      /* the amplitude last applied */
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
static retro_atomic_int_t input_openxr_laser;
static retro_atomic_int_t input_openxr_menu_open;

/* A core's rumble per player, strong then weak, and whether a session's
 * controllers exist: cores set rumble from any thread. */
static retro_atomic_int_t input_openxr_rumble[INPUT_OPENXR_PADS][2];
static retro_atomic_int_t input_openxr_ready;

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
   XrResult res;
   XrPath profile = XR_NULL_PATH;
   XrActionSuggestedBinding binds[INPUT_OPENXR_MAX_BINDS];
   XrInteractionProfileSuggestedBinding sb;

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
   memset(&sb, 0, sizeof(sb));
   sb.type                   = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
   sb.interactionProfile     = profile;
   sb.countSuggestedBindings = p->count;
   sb.suggestedBindings      = binds;
   if (XR_FAILED(res = st->SuggestInteractionProfileBindings(st->instance,
               &sb)))
      RARCH_WARN("[OpenXR] Bindings refused for %s (%d).\n", p->path,
            (int)res);
   else
      RARCH_LOG("[OpenXR] Bindings suggested for %s.\n", p->path);
}

static void input_openxr_release(input_openxr_t *st)
{
   unsigned i;
   retro_atomic_store_release_int(&input_openxr_ready, 0);
   for (i = 0; i < INPUT_OPENXR_HANDS; i++)
      if (st->aim[i] && st->DestroySpace)
         st->DestroySpace(st->aim[i]);
   /* Its actions go with it. */
   for (i = 0; i < INPUT_OPENXR_SETS; i++)
      if (st->sets[i] && st->DestroyActionSet)
         st->DestroyActionSet(st->sets[i]);
   input_openxr_clear(st);
}

static void input_openxr_session_created(void *user,
      const vulkan_openxr_handles_t *h)
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
   st->local_space = h->local_space;
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
      memset(&ci, 0, sizeof(ci));
      ci.type = XR_TYPE_ACTION_SET_CREATE_INFO;
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
      memset(&ci, 0, sizeof(ci));
      ci.type       = XR_TYPE_ACTION_CREATE_INFO;
      ci.actionType = d->type;
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
   memset(&ai, 0, sizeof(ai));
   ai.type            = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO;
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
      memset(&si, 0, sizeof(si));
      si.type                            = XR_TYPE_ACTION_SPACE_CREATE_INFO;
      si.action                          = st->actions[IXA_AIM];
      si.subactionPath                   = st->hands[i];
      si.poseInActionSpace.orientation.w = 1.0f;
      if (XR_FAILED(res = st->CreateActionSpace(h->session, &si,
                  &st->aim[i])))
      {
         step = "xrCreateActionSpace";
         goto error;
      }
   }

   st->xr = h->xr;
   retro_atomic_store_release_int(&input_openxr_ready, 1);
   RARCH_LOG("[OpenXR] Headset controllers ready.\n");
   return;

error:
   RARCH_WARN("[OpenXR] Headset controllers unavailable (%s: %d).\n",
         step, (int)res);
   input_openxr_release(st);
}

static void input_openxr_session_destroying(void *user, vulkan_openxr_t *xr)
{
   (void)user;
   (void)xr;
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
   memset(gi, 0, sizeof(*gi));
   gi->type          = XR_TYPE_ACTION_STATE_GET_INFO;
   gi->action        = st->actions[a];
   gi->subactionPath = hand;
}

static bool input_openxr_bool(input_openxr_t *st, unsigned a, XrPath hand)
{
   XrActionStateGetInfo gi;
   XrActionStateBoolean s;
   input_openxr_get_info(st, a, hand, &gi);
   memset(&s, 0, sizeof(s));
   s.type = XR_TYPE_ACTION_STATE_BOOLEAN;
   return XR_SUCCEEDED(st->GetActionStateBoolean(st->session, &gi, &s))
      && s.isActive && s.currentState;
}

static float input_openxr_float(input_openxr_t *st, unsigned a, XrPath hand)
{
   XrActionStateGetInfo gi;
   XrActionStateFloat s;
   input_openxr_get_info(st, a, hand, &gi);
   memset(&s, 0, sizeof(s));
   s.type = XR_TYPE_ACTION_STATE_FLOAT;
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
   memset(&s, 0, sizeof(s));
   s.type = XR_TYPE_ACTION_STATE_VECTOR2F;
   if (     XR_FAILED(st->GetActionStateVector2f(st->session, &gi, &s))
         || !s.isActive)
      return;
   xy[0] = input_openxr_axis(s.currentState.x);
   xy[1] = input_openxr_axis(-s.currentState.y);
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
}

/* The left hand player 1, the right player 2. */
static void input_openxr_read_separate(input_openxr_t *st)
{
   unsigned h;
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

/* Analog to Digital as a gamepad on the port gets it: the D-pad (or
 * Twin Stick's face buttons) from the sticks the mode takes, which then
 * read as centred. The menu reads the sticks itself, as it does a
 * gamepad's. */
static void input_openxr_dpad(input_openxr_t *st, float threshold)
{
   unsigned p;
#ifdef HAVE_MENU
   if (menu_driver_alive())
      return;
#endif
   for (p = 0; p < INPUT_OPENXR_PADS; p++)
   {
      input_openxr_pad_t *pad = &st->pads[p];
      unsigned mode           = input_driver_analog_dpad_mode(p);
      pad->buttons |= input_driver_analog_dpad_buttons(mode, pad->analog,
            threshold);
      if (     mode == ANALOG_DPAD_LSTICK
            || mode == ANALOG_DPAD_LRSTICK
            || mode == ANALOG_DPAD_TWINSTICK)
         pad->analog[0] = pad->analog[1] = 0;
      if (     mode == ANALOG_DPAD_RSTICK
            || mode == ANALOG_DPAD_LRSTICK
            || mode == ANALOG_DPAD_TWINSTICK)
         pad->analog[2] = pad->analog[3] = 0;
   }
}

/* A hand's aim ray in LOCAL space at time, when it is tracked. Either
 * thread: it only reads what the session hooks wrote. */
static bool input_openxr_ray(const input_openxr_t *st, unsigned h,
      XrTime time, video_xr_vec3_t *o, video_xr_vec3_t *d)
{
   XrSpaceLocation loc;
   video_xr_quat_t q;
   video_xr_vec3_t fwd;
   XrSpaceLocationFlags valid = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT
      | XR_SPACE_LOCATION_POSITION_VALID_BIT;

   memset(&loc, 0, sizeof(loc));
   loc.type = XR_TYPE_SPACE_LOCATION;
   if (     !st->aim[h] || !time
         || XR_FAILED(st->LocateSpace(st->aim[h], st->local_space, time,
               &loc))
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
 * the core's view map when it has one, over the whole frame when not. */
static bool input_openxr_frame_point(const video_xr_quad_t *q, float u,
      float v, int16_t *x, int16_t *y)
{
   unsigned dims                = 0;
   const video_views_map_t *map = NULL;
   if (!video_driver_get_views_core(&map, &dims))
      return false;
   return video_xr_quad_to_frame(q, u, v, map, dims, x, y);
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

   st->pointer = -1;
   if (     laser != VIDEO_OPENXR_LASER_OFF
         && (time = vulkan_openxr_predicted_time(st->xr))
         && vulkan_openxr_get_quads(st->xr, &st->quads))
      for (i = 0; i < st->quads.num_quads; i++)
      {
         const video_xr_quad_t *lq = &st->quads.quads[i];
         if (!video_xr_quad_live(lq, laser, menu_open))
            continue;
         /* Without a live screen, the mouse and the overlay keep the
          * core's pointer and gun. */
         if (lq->kind == VIDEO_XR_QUAD_MENU)
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
      tracked[h] = input_openxr_ray(st, h, time, &o, &d);
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
         st->role[h] = (st->quads.quads[st->hit[h]].kind
               == VIDEO_XR_QUAD_MENU)
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
      if (q && q->kind == VIDEO_XR_QUAD_MENU)
      {
         st->menu_u = st->hit_u[p];
         st->menu_v = st->hit_v[p];
      }
      st->menu_on      = true;
      st->menu_pressed = !lift;
      return;
   }
   if (q && q->kind == VIDEO_XR_QUAD_MENU)
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
   input_openxr_t *st = &input_openxr_st;
   int laser          = retro_atomic_load_acquire_int(&input_openxr_laser);
   bool menu_open     = retro_atomic_load_acquire_int(
         &input_openxr_menu_open) != 0;
   (void)user;

   if (     !st->xr || laser == VIDEO_OPENXR_LASER_OFF
         || !vulkan_openxr_focused(st->xr)
         || !(cursor = vulkan_openxr_cursor(st->xr))
         || !vulkan_openxr_get_quads(st->xr, &set))
      return 0;
   for (h = 0; h < INPUT_OPENXR_HANDS && n < cap; h++)
   {
      int q;
      float u, v, t, size;
      video_xr_pose_t pose;
      video_xr_vec3_t o, d;
      XrCompositionLayerQuad *l = &st->cursors[h];
      if (     !input_openxr_ray(st, h, time, &o, &d)
            || (q = video_xr_pick(&set, (unsigned)laser, menu_open,
                  &o, &d, &u, &v, &t)) < 0)
         continue;
      size = video_xr_cursor(&set.quads[q], u, v, t, &pose);
      memset(l, 0, sizeof(*l));
      l->type                             = XR_TYPE_COMPOSITION_LAYER_QUAD;
      l->layerFlags                       =
         XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
      l->space                            = st->local_space;
      l->eyeVisibility                    = XR_EYE_VISIBILITY_BOTH;
      l->subImage.swapchain               = cursor;
      l->subImage.imageRect.extent.width  = VULKAN_OPENXR_CURSOR_DIM;
      l->subImage.imageRect.extent.height = VULKAN_OPENXR_CURSOR_DIM;
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
      if (     amp == st->haptic[h]
            && (amp <= 0.0f
               || now - st->haptic_time[h] < INPUT_OPENXR_HAPTIC_REFRESH))
         continue;
      memset(&info, 0, sizeof(info));
      info.type          = XR_TYPE_HAPTIC_ACTION_INFO;
      info.action        = st->actions[IXA_RUMBLE];
      info.subactionPath = st->hands[h];
      if (amp > 0.0f)
      {
         XrHapticVibration vib;
         memset(&vib, 0, sizeof(vib));
         vib.type      = XR_TYPE_HAPTIC_VIBRATION;
         vib.duration  = INPUT_OPENXR_HAPTIC_DURATION;
         vib.frequency = XR_FREQUENCY_UNSPECIFIED;
         vib.amplitude = amp;
         st->ApplyHapticFeedback(st->session, &info,
               (const XrHapticBaseHeader*)&vib);
      }
      else
         st->StopHapticFeedback(st->session, &info);
      st->haptic[h]      = amp;
      st->haptic_time[h] = now;
   }
}

void input_openxr_poll(unsigned controllers, unsigned laser,
      float threshold)
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

   /* The XR thread places the dots by these: none while the laser
    * yields, which a closed menu or the laser Off ends. */
   if (!menu_open || laser == VIDEO_OPENXR_LASER_OFF)
      st->yield = false;
   retro_atomic_store_release_int(&input_openxr_laser,
         st->yield ? VIDEO_OPENXR_LASER_OFF : (int)laser);
   retro_atomic_store_release_int(&input_openxr_menu_open,
         menu_open ? 1 : 0);

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
   st->hit[0]       = -1;
   st->hit[1]       = -1;
   st->ptr_owned    = false;
   st->ptr_on       = false;
   st->ptr_pressed  = false;
   st->gun_pressed  = false;
   st->menu_on      = false;
   st->menu_pressed = false;
   if (!st->xr)
      return;
   if (!vulkan_openxr_focused(st->xr))
   {
      if (st->focused)
         RARCH_LOG("[OpenXR] Controllers released: the headset is not focused.\n");
      st->focused = false;
      input_openxr_triggers_lost(st);
      /* So their return is logged too. */
      st->mode    = -1;
      /* The runtime drops haptics out of focus: apply again after. */
      st->haptic[0] = 0.0f;
      st->haptic[1] = 0.0f;
      return;
   }
   st->focused = true;

   memset(active, 0, sizeof(active));
   active[0].actionSet      = st->sets[separate
      ? INPUT_OPENXR_SET_SEPARATE : INPUT_OPENXR_SET_COMBINED];
   active[1].actionSet      = st->sets[INPUT_OPENXR_SET_POINTER];
   memset(&si, 0, sizeof(si));
   si.type                  = XR_TYPE_ACTIONS_SYNC_INFO;
   si.countActiveActionSets = 2;
   si.activeActionSets      = active;
   /* XR_SESSION_NOT_FOCUSED succeeds too, with nothing active. */
   if ((res = st->SyncActions(st->session, &si)) != XR_SUCCESS)
   {
      if (res != st->sync_result)
         RARCH_WARN("[OpenXR] Controllers not read: xrSyncActions %d.\n",
               (int)res);
      st->sync_result = res;
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
   retro_atomic_store_release_int(&input_openxr_laser, (int)laser);
   input_openxr_laser_poll(st, laser, menu_open, threshold, was_pressed);
   input_openxr_triggers(st, threshold);
   input_openxr_dpad(st, threshold);
   input_openxr_haptics(st, separate,
         (run_flags & RUNLOOP_FLAG_PAUSED) ? true : false);
}

bool input_openxr_button(unsigned port, unsigned id)
{
   input_openxr_t *st = &input_openxr_st;
   if (id < RARCH_FIRST_CUSTOM_BIND)
      return port < INPUT_OPENXR_PADS
         && (st->pads[port].buttons & (1u << id));
   if (id == RARCH_MENU_TOGGLE)
      return st->menu_toggle;
   if (id == RARCH_HEADSET_RECENTER)
      return st->recenter;
   if (id == RARCH_LASER_POINTER_TOGGLE)
      return st->laser_toggle;
   return false;
}

int16_t input_openxr_analog(unsigned port, unsigned idx, unsigned id,
      int16_t res)
{
   int v                     = 0;
   int r                     = res;
   const input_openxr_pad_t *pad;
   if (port >= INPUT_OPENXR_PADS)
      return res;
   pad = &input_openxr_st.pads[port];
   if (idx == RETRO_DEVICE_INDEX_ANALOG_BUTTON)
   {
      if (id == RETRO_DEVICE_ID_JOYPAD_L2)
         v = pad->trigger[0];
      else if (id == RETRO_DEVICE_ID_JOYPAD_R2)
         v = pad->trigger[1];
   }
   else if (     idx <= RETRO_DEVICE_INDEX_ANALOG_RIGHT
              && id  <= RETRO_DEVICE_ID_ANALOG_Y)
      v = pad->analog[idx * 2 + id];
   /* Analog values keep the larger magnitude, as the overlay's do. */
   if ((v < 0 ? -v : v) > (r < 0 ? -r : r))
      return (int16_t)v;
   return res;
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

bool input_openxr_set_rumble(unsigned port, enum retro_rumble_effect effect,
      uint16_t strength)
{
   if (port >= INPUT_OPENXR_PADS)
      return false;
   retro_atomic_store_release_int(
         &input_openxr_rumble[port][(effect == RETRO_RUMBLE_STRONG) ? 0 : 1],
         (int)strength);
   return retro_atomic_load_acquire_int(&input_openxr_ready) != 0;
}

bool input_openxr_can_rumble(void)
{
   return retro_atomic_load_acquire_int(&input_openxr_ready) != 0;
}

void input_openxr_register(void)
{
   vulkan_openxr_hooks_t hooks;
   hooks.session_created    = input_openxr_session_created;
   hooks.session_destroying = input_openxr_session_destroying;
   hooks.frame_layers       = input_openxr_frame_layers;
   hooks.user               = NULL;
   vulkan_openxr_set_hooks(&hooks);
}
