/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2015 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

/* TODO/FIXME - set this once the kqueue codepath is implemented and working properly,
 * also remove libepoll-shim from the Makefile when that happens. */
#if 1
#define HAVE_EPOLL
#else
#ifdef __linux__
#define HAVE_EPOLL 1
#endif

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined (__NetBSD__)
#define HAVE_KQUEUE 1
#endif
#endif


#include <stdint.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#if defined(HAVE_EPOLL)
#include <sys/epoll.h>
#elif defined(HAVE_KQUEUE)
#include <sys/event.h>
#endif
#include <poll.h>

#include <libudev.h>
#if defined(__linux__)
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kd.h>
#elif defined(__FreeBSD__)
#include <dev/evdev/input.h>
#endif

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_X11
#include <X11/Xlib.h>
#endif

#include <file/file_path.h>
#include <compat/strl.h>
#include <string/stdstring.h>
#include <retro_miscellaneous.h>

#include "../input_keymaps.h"

#include "../common/linux_common.h"

#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

#if defined(HAVE_XKBCOMMON) && defined(HAVE_KMS)
#define UDEV_XKB_HANDLING
#endif

/* Force UDEV_XKB_HANDLING for Lakka */
#ifdef HAVE_LAKKA
#ifndef UDEV_XKB_HANDLING
#define UDEV_XKB_HANDLING
#endif
#endif

#define UDEV_MAX_KEYS (KEY_MAX + 7) / 8

#ifdef UDEV_TOUCH_SUPPORT

/* Temporary defines for debugging purposes */

/* Define this to use direct printf debug messages. */
/*#define UDEV_TOUCH_PRINTF_DEBUG*/
/* Define this to add more deep debugging messages - performace will suffer... */
/*#define UDEV_TOUCH_DEEP_DEBUG*/

/* TODO - Temporary debugging using direct printf */
#ifdef UDEV_TOUCH_PRINTF_DEBUG
#define RARCH_ERR(...) do{ \
    printf("[ERR]" __VA_ARGS__); \
} while (0)

#define RARCH_WARN(...) do{ \
    printf("[WARN]" __VA_ARGS__); \
} while (0)

#define RARCH_LOG(...) do{ \
    printf("[LOG]" __VA_ARGS__); \
} while (0)

#define RARCH_DBG(...) do{ \
    printf("[DBG]" __VA_ARGS__); \
} while (0)
#endif 
/* UDEV_TOUCH_PRINTF_DEBUG */

#ifdef UDEV_TOUCH_DEEP_DEBUG
#define RARCH_DDBG(msg, ...) do{ \
    RARCH_DBG(msg, __VA_ARGS__); \
} while (0)
#else 
/* TODO - Since C89 doesn't allow variadic macros, we have an empty function instead... */
void RARCH_DDBG(const char *fmt, ...) { }
#endif 
/* UDEV_TOUCH_DEEP_DEBUG */

/* Helper macro for declaring things as unused - use sparingly. */
#define UDEV_INPUT_TOUCH_UNUSED(X) (void)(X)

/* Tracking ID used when not in use */
#define UDEV_INPUT_TOUCH_TRACKING_ID_NONE -1
/* Slot ID used when not in use */
#define UDEV_INPUT_TOUCH_SLOT_ID_NONE -1
/* Upper limit of fingers tracked by this implementation */
#define UDEV_INPUT_TOUCH_FINGER_LIMIT 10
/* Conversion factor from seconds to microseconds */
#define UDEV_INPUT_TOUCH_S_TO_US 1000000
/* Conversion factor from microseconds to nanoseconds */
#define UDEV_INPUT_TOUCH_US_TO_NS 1000
/* Default state of pointer simulation. */
#define UDEV_INPUT_TOUCH_POINTER_EN settings->bools.input_touch_vmouse_pointer
/* Default state of mouse simulation. */
#define UDEV_INPUT_TOUCH_MOUSE_EN settings->bools.input_touch_vmouse_mouse
/* Default state of touchpad simulation. */
#define UDEV_INPUT_TOUCH_TOUCHPAD_EN settings->bools.input_touch_vmouse_touchpad
/* Default state of trackball simulation. */
#define UDEV_INPUT_TOUCH_TRACKBALL_EN settings->bools.input_touch_vmouse_trackball
/* Default state of gesture simulation. */
#define UDEV_INPUT_TOUCH_GEST_EN settings->bools.input_touch_vmouse_gesture
/* Default value of tap time in us. */
#define UDEV_INPUT_TOUCH_MAX_TAP_TIME 250000
/* Default value of tap distance - squared distance in 0x7fff space. */
#define UDEV_INPUT_TOUCH_MAX_TAP_DIST 0x1000
/* Default value of tap time in us. */
#define UDEV_INPUT_TOUCH_GEST_CLICK_TIME 10000
/* Default value of gesture inactivity timeout. */
#define UDEV_INPUT_TOUCH_GEST_TIMEOUT 70000
/* Default sensitivity of the simulated touchpad. */
#define UDEV_INPUT_TOUCH_PAD_SENSITIVITY 0.6f
/* Default value of scroll gesture sensitivity. */
#define UDEV_INPUT_TOUCH_GEST_SCROLL_SENSITIVITY 0xa0
/* Default value of scroll gesture step. */
#define UDEV_INPUT_TOUCH_GEST_SCROLL_STEP 0x10
/* Default value of panel percentage considered corner */
#define UDEV_INPUT_TOUCH_GEST_CORNER 5
/* Default friction of the trackball x-axis rotation, used as a multiplier */
#define UDEV_INPUT_TOUCH_TRACKBALL_FRICT_X 0.9
/* Default friction of the trackball y-axis rotation, used as a multiplier */
#define UDEV_INPUT_TOUCH_TRACKBALL_FRICT_Y 0.9
/* Default sensitivity of the trackball to original movement */
#define UDEV_INPUT_TOUCH_TRACKBALL_SENSITIVITY_X 10
/* Default sensitivity of the trackball to original movement */
#define UDEV_INPUT_TOUCH_TRACKBALL_SENSITIVITY_Y 10
/* Default squared cutoff velocity when trackball ceases all movement */
#define UDEV_INPUT_TOUCH_TRACKBALL_SQ_VEL_CUTOFF 10

typedef enum udev_dragging_type
{
   DRAG_NONE = 0,
   DRAG_TWO_FINGER,
   DRAG_THREE_FINGER
} udev_touch_dragging_type;

typedef enum udev_touch_event_type
{
   FINGERDOWN,
   FINGERUP,
   FINGERMOTION
} udev_touch_event_type;

#endif 
/* UDEV_TOUCH_SUPPORT */

struct udev_input;

struct udev_input_device;

enum udev_input_dev_type
{
   UDEV_INPUT_KEYBOARD = 0,
   UDEV_INPUT_MOUSE,
   UDEV_INPUT_TOUCHPAD, 
   UDEV_INPUT_TOUCHSCREEN
};

/* NOTE: must be in sync with enum udev_input_dev_type */
static const char *g_dev_type_str[] =
{
   "ID_INPUT_KEY",
   "ID_INPUT_MOUSE",
   "ID_INPUT_TOUCHPAD", 
   "ID_INPUT_TOUCHSCREEN"
};

typedef struct
{
   /* If device is "absolute" coords will be in device specific units
      and axis min value will be less than max, otherwise coords will be
      relative to full viewport and min and max values will be zero. */
   int32_t x_abs, y_abs;
   int32_t x_min, y_min;
   int32_t x_max, y_max;
   int32_t x_rel, y_rel;
   bool l, r, m, b4, b5;
   bool wu, wd, whu, whd;
   bool pp;
   int32_t abs;
} udev_input_mouse_t;

#ifdef UDEV_TOUCH_SUPPORT

/**
 * Types of slot changes
 */
typedef enum 
{
   /* No change */
   UDEV_TOUCH_CHANGE_NONE = 0, 
   /* Touch down, start tracking */
   UDEV_TOUCH_CHANGE_DOWN, 
   /* Touch up, end tracking */
   UDEV_TOUCH_CHANGE_UP, 
   /* Change in position, size, or pressure */
   UDEV_TOUCH_CHANGE_MOVE
} udev_input_touch_change_type;

/**
 * Types of tap gestures.
 */
typedef enum 
{
   /* Gesture selection timeout. */
   UDEV_TOUCH_TGEST_TIMEOUT = 0, 
   /* Gesture for one finger tap. */
   UDEV_TOUCH_TGEST_S_TAP, 
   /* Gesture for two finger tap. */
   UDEV_TOUCH_TGEST_D_TAP, 
   /* Gesture for three finger tap. */
   UDEV_TOUCH_TGEST_T_TAP, 
   /* Sentinel and total number of gestures. */
   UDEV_TOUCH_TGEST_LAST
} udev_input_touch_tgest_type;

/**
 * Types of move gestures.
 */
typedef enum
{
   /* No move gesture in process. */
   UDEV_TOUCH_MGEST_NONE = 0, 
   /* Gesture for one finger single tap drag. */
   UDEV_TOUCH_MGEST_S_STAP_DRAG, 
   /* Gesture for one finger double tap drag. */
   UDEV_TOUCH_MGEST_S_DTAP_DRAG, 
   /* Gesture for one finger triple tap drag. */
   UDEV_TOUCH_MGEST_S_TTAP_DRAG, 
   /* Gesture for two finger no tap drag. */
   UDEV_TOUCH_MGEST_D_NTAP_DRAG, 
   /* Gesture for two finger single tap drag. */
   UDEV_TOUCH_MGEST_D_STAP_DRAG, 
   /* Gesture for three finger no tap drag. */
   UDEV_TOUCH_MGEST_T_NTAP_DRAG, 
   /* Gesture for three finger single tap drag. */
   UDEV_TOUCH_MGEST_T_STAP_DRAG, 
   /* Sentinel and total number of motion gestures. */
   UDEV_TOUCH_MGEST_LAST
} udev_input_touch_mgest_type;

/**
 * Definition of feature limits.
 */
typedef struct
{
   /* Is this feature enabled? */
   bool enabled;

   /* Range of values supported by this feature:  */
   int32_t min;
   int32_t max;
   int32_t range;

   /* Optional, but could be useful later:  */
   /* Fuzz value used to filter noise from event stream. */
   int32_t fuzz;
   /* Values within this value will be discarded and reported as 0 */
   int32_t flat;
   /* Resolution for the reported values. */
   int32_t resolution;
} udev_input_touch_limits_t;

/**
 * Helper structure for representing the time of a touch event.
 */
typedef struct
{
   /* Seconds */
   uint32_t s;
   /* Microseconds */
   uint32_t us;
} udev_touch_ts_t;

/**
 * Abstraction for time-delayed callback.
 */
typedef struct 
{
   /* Is this event active? */
   bool active;
   /* Execute this callback at this or later time. */
   udev_touch_ts_t execute_at;
   /* Callback to execute, providing the custom data. */
   void (*cb)(void *touch, void *data);
   /* Custom data pointer provided to the callback. */
   void *data;
} udev_touch_timed_cb_t;

/**
 * Structure representing the state of a single touchscreen slot.
 */
typedef struct
{
   /* Unique tracking ID for this slot. UDEV_TRACKING_ID_NONE -> No touch. */
   int32_t tracking_id;
   /* Current position of the tracked touch. */
   int16_t pos_x;
   int16_t pos_y;
   /* Current major and minor axis. */
   int16_t minor;
   int16_t major;
   /* Current pressure. */
   int16_t pressure;
   /* Type of change which occurred - udev_input_touch_change_type. */
   uint16_t change;
   /* Start timestamp of the current or last touch down. */
   udev_touch_ts_t td_time;
   /* Start position of the current or last touch down. */
   int16_t td_pos_x;
   int16_t td_pos_y;
} udev_slot_state_t;

/* Type used to represent touch slot ID, UDEV_INPUT_TOUCH_SLOT_ID_NONE (-1) is used for none. */
typedef int32_t udev_input_touch_slot_id;

/**
 * Container for touch-related tracking data.
 */
typedef struct
{
   /* Touch panel properties */
   bool is_touch_device;
   /* Info for number of tracked fingers/touch-points/slots */
   udev_input_touch_limits_t info_slots; 
   /* Info for x-axis limits */
   udev_input_touch_limits_t info_x_limits; 
   /* Info for y-axis limits */
   udev_input_touch_limits_t info_y_limits; 
   /* Info for primary touch axis limits */
   udev_input_touch_limits_t info_major; 
   /* Info for secondary touch axis limits */
   udev_input_touch_limits_t info_minor; 
   /* Info for pressure limits */
   udev_input_touch_limits_t info_pressure; 

   /*
    * Allocated data block compatible with the following structure: 
    * struct input_mt_request_layout {
    *     __u32 code;
    *     __s32 values[info_slots->range];
    * };
    * For reference, see linux kernel: include/uapi/linux/input.h#L147
    */
   uint8_t *request_data;
   size_t request_data_size;

   udev_input_touch_slot_id current_slot;
   /* Staging data for the slot states. Sized to [info_slots->range]. */
   udev_slot_state_t *staging;
   uint16_t staging_active;
   /* Current data for the slot states. Sized to [info_slots->range]. */
   udev_slot_state_t *current;
   uint16_t current_active;

   /* Flag used to run the state update. */
   bool run_state_update;
   /* Timestamp of when the last touch state update ocurred */
   udev_touch_ts_t last_state_update;

   /* Simulated pointer / touchscreen */
   /* Enable pointer simulation? */
   bool pointer_enabled;
   /* Pointer pressed. */
   bool pointer_btn_pp;
   /* Pointer back - TODO unknown action, RETRO_DEVICE_ID_POINTER_BACK. */
   bool pointer_btn_pb;
   /* Pointer position in the touch panel coordinates. */
   int32_t pointer_pos_x;
   int32_t pointer_pos_y;
   /* Pointer position in the main panel coordinates. */
   int32_t pointer_ma_pos_x;
   int32_t pointer_ma_pos_y;
   /* Pointer position delta in main panel pixels. */
   int16_t pointer_ma_rel_x;
   int16_t pointer_ma_rel_y;
   /* Pointer position mapped onto the primary screen (-0x7fff - 0x7fff). */
   int16_t pointer_scr_pos_x;
   int16_t pointer_scr_pos_y;
   /* Pointer position within the window, -0x7fff - 0x7fff -> inside. */
   int16_t pointer_vp_pos_x;
   int16_t pointer_vp_pos_y;

   /* Simulated mouse */
   /* Enable mouse simulation? */
   bool mouse_enabled;
   /* Freeze the mouse cursor in place? */
   bool mouse_freeze_cursor;
   /* Mouse position in the original pixel coordinates. */
   int16_t mouse_pos_x;
   int16_t mouse_pos_y;
   /* Mouse position mapped onto the primary screen (-0x7fff - 0x7fff). */
   int16_t mouse_scr_pos_x;
   int16_t mouse_scr_pos_y;
   /* Mouse position within the window, -0x7fff - 0x7fff -> inside. */
   int16_t mouse_vp_pos_x;
   int16_t mouse_vp_pos_y;
   /* Mouse position delta in screen pixels. */
   int16_t mouse_rel_x;
   int16_t mouse_rel_y;
   /* Mouse wheel delta in number of frames to hold that value. */
   int16_t mouse_wheel_x;
   int16_t mouse_wheel_y;
   /* Mouse buttons. */
   bool mouse_btn_l;
   bool mouse_btn_r;
   bool mouse_btn_m;
   bool mouse_btn_b4;
   bool mouse_btn_b5;

   /* Mouse touchpad simulation */
   /* Enable the touchpad mode? Switches between direct (false) and touchpad (true) modes */
   bool touchpad_enabled;
   /* Sensitivity of the touchpad mouse. */
   float touchpad_sensitivity;
   /* High resolution touchpad pointer position mapped onto the primary screen. */
   float touchpad_pos_x;
   float touchpad_pos_y;

   /* Mouse trackball simulation */
   /* Enable the trackball mode? Switches between immediate stop and trackball-style */
   bool trackball_enabled;
   /* Is the trackball free to rotate under its own inertia? */
   bool trackball_inertial;
   /* Current high resolution position of the trackball */
   float trackball_pos_x;
   float trackball_pos_y;
   /* Sensitivity of the trackball to movement */
   float trackball_sensitivity_x;
   float trackball_sensitivity_y;
   /* Current velocity of the trackball */
   float trackball_vel_x;
   float trackball_vel_y;
   /* Friction of the trackball */
   float trackball_frict_x;
   float trackball_frict_y;
   /* Squared cutoff velocity of when the trackball ceases all movement */
   int16_t trackball_sq_vel_cutoff;

   /* Gestures and multi-touch tracking */
   /* Enable the gestures? */
   bool gest_enabled;
   /* Primary slot used as the slot index for MT gestures */
   udev_input_touch_slot_id gest_primary_slot;
   /* Secondary slot used as the slot index for MT gestures */
   udev_input_touch_slot_id gest_secondary_slot;
   /* Time of inactivity before gesture is selected. */
   uint32_t gest_timeout;
   /* Maximum contact time in us considered as a "tap". */
   uint32_t gest_tap_time;
   /* Maximum contact distance in normalized screen units considered as a "tap". */
   uint32_t gest_tap_dist;
   /* Time a button should be held down, i.e. a "click". */
   uint32_t gest_click_time;
   /* Number of taps in the current gesture. */
   uint16_t gest_tap_count;
   /* Is it possible for a tap to happen? Set to false once time or distance is over limit. */
   bool gest_tap_possible;
   /* Sensitivity of scrolling for the scroll wheel gesture. */
   uint16_t gest_scroll_sensitivity;
   /* Step of scrolling for the scroll wheel gesture. */
   uint16_t gest_scroll_step;
   /* High resolution scroll. */
   int16_t gest_scroll_x;
   int16_t gest_scroll_y;
   /* Percentage of screen considered as a corner. */
   uint16_t gest_corner;
   /* Time-delayed callbacks used for tap gesture automation. */
   udev_touch_timed_cb_t gest_tcbs[UDEV_TOUCH_TGEST_LAST]; 
   /* Current move gesture in process. */
   udev_input_touch_mgest_type gest_mgest_type;
   /* Time-delayed callbacks used for move gesture automation. */
   udev_touch_timed_cb_t gest_mcbs[UDEV_TOUCH_MGEST_LAST]; 

} udev_input_touch_t;

#endif 
/* UDEV_TOUCH_SUPPORT */

typedef struct udev_input_device
{
   void (*handle_cb)(void *data,
         const struct input_event *event, 
         struct udev_input_device *dev);
   int fd; /* Device file descriptor */
   dev_t dev; /* Device handle */
   udev_input_mouse_t mouse; /* State tracking for mouse-type devices */
#ifdef UDEV_TOUCH_SUPPORT
   udev_input_touch_t touch; /* State tracking for touch-type devices */
#endif 
   enum udev_input_dev_type type; /* Type of this device */
   char devnode[NAME_MAX_LENGTH]; /* Device node path */
   char ident[255]; /* Identifier of the device */
} udev_input_device_t;

typedef void (*device_handle_cb)(void *data,
      const struct input_event *event, udev_input_device_t *dev);

typedef struct udev_input
{
   struct udev *udev;
   struct udev_monitor *monitor;
   udev_input_device_t **devices;

   /* Indices of keyboards in the devices array. Negative values are invalid. */
   int32_t keyboards[MAX_INPUT_DEVICES];
   /* Indices of pointers in the devices array. Negative values are invalid. */
   int32_t pointers[MAX_INPUT_DEVICES];

   int fd;
   /* OS pointer coords (zeros if we don't have X11) */
   int pointer_x;
   int pointer_y;

   unsigned num_devices;

   uint8_t state[UDEV_MAX_KEYS];

#ifdef UDEV_XKB_HANDLING
   bool xkb_handling;
#endif
} udev_input_t;

#ifdef UDEV_XKB_HANDLING
int init_xkb(int fd, size_t size);
void free_xkb(void);
int handle_xkb(int code, int value);
#endif

static unsigned input_unify_ev_key_code(unsigned code)
{
   /* input_keymaps_translate_keysym_to_rk does not support the case
    * where multiple keysyms translate to the same RETROK_* code,
    * so unify remote control keysyms to keyboard keysyms here.
    *
    * Addendum: The rarch_keysym_lut lookup table also becomes
    * unusable if more than one keysym translates to the same
    * RETROK_* code, so certain keys must be left unmapped in
    * rarch_key_map_linux and instead be handled here */
   switch (code)
   {
      case KEY_OK:
      case KEY_SELECT:
         return KEY_ENTER;
      case KEY_BACK:
         return KEY_BACKSPACE;
      case KEY_EXIT:
         return KEY_CLEAR;
      default:
         break;
   }

   return code;
}

static void udev_handle_keyboard(void *data,
      const struct input_event *event, udev_input_device_t *dev)
{
   unsigned keysym;
   udev_input_t *udev = (udev_input_t*)data;

   switch (event->type)
   {
      case EV_KEY:
         keysym = input_unify_ev_key_code(event->code);
         if (event->value && video_driver_has_focus())
            BIT_SET(udev->state, keysym);
         else
            BIT_CLEAR(udev->state, keysym);

         /* TODO/FIXME: The udev driver is incomplete.
          * When calling input_keyboard_event() the
          * following parameters are omitted:
          * - character: the localised Unicode/UTF-8
          *   value of the pressed key
          * - mod: the current keyboard modifier
          *   bitmask
          * Without these values, input_keyboard_event()
          * does not function correctly (e.g. it is
          * impossible to use text entry in the menu).
          * I cannot find any usable reference for
          * converting a udev-returned key code into a
          * localised Unicode/UTF-8 value, so for the
          * time being we must rely on other sources:
          * - If we are using an X11-based context driver,
          *   input_keyboard_event() is handled correctly
          *   in x11_common:x11_check_window()
          * - If we are using KMS, input_keyboard_event()
          *   is handled correctly in
          *   keyboard_event_xkb:handle_xkb()
          * If neither are available, then just call
          * input_keyboard_event() without character and
          * mod, and hope for the best... */

         if (video_driver_display_type_get() != RARCH_DISPLAY_X11)
         {
#ifdef UDEV_XKB_HANDLING
            if (udev->xkb_handling && handle_xkb(keysym, event->value) == 0)
               return;
#endif
            input_keyboard_event(event->value,
                  input_keymaps_translate_keysym_to_rk(keysym),
                  0, 0, RETRO_DEVICE_KEYBOARD);
         }

         break;

      default:
         break;
   }
}

static void udev_input_kb_free(struct udev_input *udev)
{
   unsigned i;

   for (i = 0; i < UDEV_MAX_KEYS; i++)
      udev->state[i] = 0;

#ifdef UDEV_XKB_HANDLING
   free_xkb();
#endif
}

static udev_input_mouse_t *udev_get_mouse(
      struct udev_input *udev, unsigned port)
{
   unsigned i;
   unsigned mouse_index      = 0;
   int dev_index             = -1;
   settings_t *settings      = config_get_ptr();
   udev_input_mouse_t *mouse = NULL;

   if (port >= MAX_USERS || !video_driver_has_focus())
      return NULL;

   mouse_index = settings->uints.input_mouse_index[port];
   if (mouse_index < MAX_INPUT_DEVICES)
       dev_index = udev->pointers[mouse_index];
   if (dev_index < 0)
       return NULL;
   else
       return &udev->devices[dev_index]->mouse;
}

static void udev_mouse_set_x(udev_input_mouse_t *mouse, int32_t x, bool abs)
{
    video_viewport_t vp;

   if (abs)
   {
      mouse->x_rel += x - mouse->x_abs;
      mouse->x_abs = x;
   }
   else
   {
      mouse->x_rel += x;
      if (video_driver_get_viewport_info(&vp))
      {
         mouse->x_abs += x;

         if (mouse->x_abs < vp.x)
            mouse->x_abs = vp.x;
         else if (mouse->x_abs >= vp.x + vp.full_width)
            mouse->x_abs = vp.x + vp.full_width - 1;
      }
   }
}

static int16_t udev_mouse_get_x(const udev_input_mouse_t *mouse)
{
   video_viewport_t vp;
   double src_width;
   double x;

   if (!video_driver_get_viewport_info(&vp))
      return 0;

   if (mouse->abs) /* mouse coords are absolute */
      src_width = mouse->x_max - mouse->x_min + 1;
   else
      src_width = vp.full_width;

   x = (double)vp.width / src_width * mouse->x_rel;

   return x + (x < 0 ? -0.5 : 0.5);
}

static void udev_mouse_set_y(udev_input_mouse_t *mouse, int32_t y, bool abs)
{
   video_viewport_t vp;

   if (abs)
   {
      mouse->y_rel += y - mouse->y_abs;
      mouse->y_abs = y;
   }
   else
   {
      mouse->y_rel += y;
      if (video_driver_get_viewport_info(&vp))
      {
         mouse->y_abs += y;

         if (mouse->y_abs < vp.y)
            mouse->y_abs = vp.y;
         else if (mouse->y_abs >= vp.y + vp.full_height)
            mouse->y_abs = vp.y + vp.full_height - 1;
      }
   }
}

static int16_t udev_mouse_get_y(const udev_input_mouse_t *mouse)
{
   video_viewport_t vp;
   double src_height;
   double y;

   if (!video_driver_get_viewport_info(&vp))
      return 0;

   if (mouse->abs) /* mouse coords are absolute */
      src_height = mouse->y_max - mouse->y_min + 1;
   else
      src_height = vp.full_height;

   y = (double)vp.height / src_height * mouse->y_rel;

   return y + (y < 0 ? -0.5 : 0.5);
}

static int16_t udev_mouse_get_pointer_x(const udev_input_mouse_t *mouse, bool screen)
{
   video_viewport_t vp;
   double src_min;
   double src_width;
   int32_t x;

   if (!video_driver_get_viewport_info(&vp))
      return 0;

   if (mouse->abs) /* mouse coords are absolute */
   {
      /* mouse coordinates are relative to the screen; convert them
       * to be relative to the viewport */
      double scaled_x;
      src_min = mouse->x_min;
      src_width = mouse->x_max - mouse->x_min + 1;
      scaled_x = vp.full_width * (mouse->x_abs - src_min) / src_width;
      x = -32767.0 + 65535.0 / vp.width * (scaled_x - vp.x);
   }
   else /* mouse coords are viewport relative */
   {
      if (screen) 
      {
         src_min = 0.0;
         src_width = vp.full_width;
      }
      else 
      {
         src_min = vp.x;
         src_width = vp.width;
      }
      x  = -32767.0 + 65535.0 / src_width * (mouse->x_abs - src_min);
   }

   x += (x < 0 ? -0.5 : 0.5);

   if (x < -0x7fff)
      return -0x7fff;
   else if (x > 0x7fff)
      return 0x7fff;

   return x;
}

static int16_t udev_mouse_get_pointer_y(const udev_input_mouse_t *mouse, bool screen)
{
   video_viewport_t vp;
   double src_min;
   double src_height;
   int32_t y;

   if (!video_driver_get_viewport_info(&vp))
      return 0;

   if (mouse->abs) /* mouse coords are absolute */
   {
      double scaled_y;
      /* mouse coordinates are relative to the screen; convert them
       * to be relative to the viewport */
      src_min = mouse->y_min;
      src_height = mouse->y_max - mouse->y_min + 1;
      scaled_y = vp.full_height * (mouse->y_abs - src_min) / src_height;
      y = -32767.0 + 65535.0 / vp.height * (scaled_y - vp.y);
   }
   else /* mouse coords are viewport relative */
   {
      if (screen)
      {
         src_min = 0.0;
         src_height = vp.full_height;
      }
      else
      {
         src_min = vp.y;
         src_height = vp.height;
      }
      y = -32767.0 + 65535.0 / src_height * (mouse->y_abs - src_min);
   }

   y += (y < 0 ? -0.5 : 0.5);

   if (y < -0x7fff)
      return -0x7fff;
   else if (y > 0x7fff)
      return 0x7fff;

   return y;
}

static void udev_handle_mouse(void *data,
      const struct input_event *event, udev_input_device_t *dev)
{
   udev_input_mouse_t *mouse = &dev->mouse;

   switch (event->type)
   {
      case EV_KEY:
         switch (event->code)
         {
            case BTN_LEFT:
               mouse->l = event->value;
               break;

            case BTN_RIGHT:
               mouse->r = event->value;
               break;

            case BTN_MIDDLE:
               mouse->m = event->value;
               break;
            case BTN_TOUCH:
               mouse->pp = event->value;
               break;
            /*case BTN_??:
               mouse->b4 = event->value;
               break;*/

            /*case BTN_??:
               mouse->b5 = event->value;
               break;*/

            default:
               break;
         }
         break;

      case EV_REL:
         switch (event->code)
         {
            case REL_X:
               udev_mouse_set_x(mouse, event->value, false);
               break;
            case REL_Y:
               udev_mouse_set_y(mouse, event->value, false);
               break;
            case REL_WHEEL:
               if (event->value == 1)
                  mouse->wu = 1;
               else if (event->value == -1)
                  mouse->wd = 1;
               break;
            case REL_HWHEEL:
               if (event->value == 1)
                  mouse->whu = 1;
               else if (event->value == -1)
                  mouse->whd = 1;
               break;
         }
         break;

      case EV_ABS:
         switch (event->code)
         {
            case ABS_X:
               udev_mouse_set_x(mouse, event->value, true);
               break;
            case ABS_Y:
               udev_mouse_set_y(mouse, event->value, true);
               break;
         }
         break;
   }
}

#ifdef UDEV_TOUCH_SUPPORT

/**
 * Copy timestamp from given input event to a timestamp structure.
 *
 * @param event The source input event.
 * @param ts Destination timestamp data.
 */
static void udev_touch_event_ts_copy(const struct input_event *event, udev_touch_ts_t *ts)
{
   ts->s = event->input_event_sec;
   ts->us = event->input_event_usec;
}

/**
 * Copy timestamp to timestamp.
 *
 * @param first The source timestamp.
 * @param second Destination timestamp.
 */
static void udev_touch_ts_copy(const udev_touch_ts_t *first, udev_touch_ts_t *second)
{
   second->s = first->s;
   second->us = first->us;
}

/**
 * Get current time and store it in the given timestamp structure.
 *
 * @param ts Destination timestamp data.
 */
static void udev_touch_ts_now(udev_touch_ts_t *ts)
{
   struct timeval now;
   gettimeofday(&now, NULL);
   ts->s = now.tv_sec;
   ts->us = now.tv_usec;
}

/**
 * Add time to given base and store resulting timestamp.
 *
 * @param base Base timestamp.
 * @param s Number of seconds to add.
 * @param us Number of microseconds to add.
 * @param dest Destination structure to store the time in.
 */
static void udev_touch_ts_add(const udev_touch_ts_t *base, 
        uint32_t s, uint32_t us, udev_touch_ts_t *dest)
{
   dest->s = base->s + s + us / UDEV_INPUT_TOUCH_S_TO_US;
   dest->us = base->us + us % UDEV_INPUT_TOUCH_S_TO_US;
}

/**
 * Calculate difference between two timestamps in microseconds.
 * @param first The first timestamp.
 * @param second The second timestamp.
 * @return Returns difference of second - first in microseconds.
 */
static int32_t udev_touch_ts_diff(const udev_touch_ts_t *first, const udev_touch_ts_t *second)
{
   return (second->s - first->s) * UDEV_INPUT_TOUCH_S_TO_US + 
          (second->us - first->us);
}

/* Touch point directions between two points. */
enum udev_input_touch_dir
{
   UDEV_INPUT_TOUCH_DIR_TL, 
   UDEV_INPUT_TOUCH_DIR_TT, 
   UDEV_INPUT_TOUCH_DIR_TR, 

   UDEV_INPUT_TOUCH_DIR_ML, 
   UDEV_INPUT_TOUCH_DIR_MT, 
   UDEV_INPUT_TOUCH_DIR_MR, 

   UDEV_INPUT_TOUCH_DIR_BL, 
   UDEV_INPUT_TOUCH_DIR_BT, 
   UDEV_INPUT_TOUCH_DIR_BR
};

/* Array with touch directions. */
static const enum udev_input_touch_dir g_udev_input_touch_dir_val[] = 
{
   UDEV_INPUT_TOUCH_DIR_TL, UDEV_INPUT_TOUCH_DIR_TT, UDEV_INPUT_TOUCH_DIR_TR, 
   UDEV_INPUT_TOUCH_DIR_ML, UDEV_INPUT_TOUCH_DIR_MT, UDEV_INPUT_TOUCH_DIR_MR, 
   UDEV_INPUT_TOUCH_DIR_BL, UDEV_INPUT_TOUCH_DIR_BT, UDEV_INPUT_TOUCH_DIR_BR, 
};

/* Array with touch directions names. */
static const char *g_udev_input_touch_dir_str[] =
{
   "TopLeft", "Top", "TopRight", 
   "MidLeft", "Mid", "MidRight", 
   "BotLeft", "Bot", "BotRight", 
};

/* Convert touch direction enum to a string representation. */
static const char *udev_touch_dir_to_str(enum udev_input_touch_dir dir)
{ return g_udev_input_touch_dir_str[dir]; }

/* Helper functions for determining touch direction. */
static const bool udev_touch_dir_top(enum udev_input_touch_dir dir)
{ return dir >= UDEV_INPUT_TOUCH_DIR_TL && dir <= UDEV_INPUT_TOUCH_DIR_TR; }
static const bool udev_touch_dir_left(enum udev_input_touch_dir dir)
{ return dir == UDEV_INPUT_TOUCH_DIR_TL || dir == UDEV_INPUT_TOUCH_DIR_ML || dir == UDEV_INPUT_TOUCH_DIR_BL; }
static const bool udev_touch_dir_bot(enum udev_input_touch_dir dir)
{ return dir >= UDEV_INPUT_TOUCH_DIR_BL && dir <= UDEV_INPUT_TOUCH_DIR_BR; }
static const bool udev_touch_dir_right(enum udev_input_touch_dir dir)
{ return dir == UDEV_INPUT_TOUCH_DIR_TR || dir == UDEV_INPUT_TOUCH_DIR_MR || dir == UDEV_INPUT_TOUCH_DIR_BR; }

/* Get signum for given value. */
static int16_t udev_touch_sign(int16_t val)
{ return (val > 0) - (val < 0); }

/* Get max of given values. */
static int16_t udev_touch_max(int16_t first, int16_t second)
{ return first > second ? first : second; }

/* Get min of given values. */
static int16_t udev_touch_min(int16_t first, int16_t second)
{ return first < second ? first : second; }

/**
 * Calculate distance between given points and a direction.
 * Coordinates and distances are calculated with system 
 * origin placed in the top left corner.
 *
 * @param pos1_x X-coordinate of the first position.
 * @param pos1_y Y-coordinate of the first position.
 * @param pos2_x X-coordinate of the second position.
 * @param pos2_y Y-coordinate of the second position.
 * @param dir Estimated direction from pos1 -> pos2.
 *
 * @return Returns squared distance between the two points.
 */
static uint32_t udev_touch_tp_diff(
        int16_t pos1_x, int16_t pos1_y, 
        int16_t pos2_x, int16_t pos2_y, 
        enum udev_input_touch_dir *dir)
{
   /* Position differences pos1 -> pos2. */
   int16_t diff_x = pos2_x - pos1_x;
   int16_t diff_y = pos2_y - pos1_y;

   /*
    * Directions: 
    * TL TT TR
    * ML MM MR
    * BL BB BR
    */
   if (dir != NULL)
   {
      *dir = g_udev_input_touch_dir_val[
         /* Index using y-axis sign. */
         (udev_touch_sign(diff_y) + 1) * 3 + 
         /* Index using x-axis sign. */
         (udev_touch_sign(diff_x) + 1)
      ];
   }

   /* Squared distance */
   return diff_x * diff_x + diff_y * diff_y;
}

/* Print out given timestamp */
static void udev_touch_ts_print(const udev_touch_ts_t *ts)
{
   RARCH_DBG("%u,%u", ts->s, ts->us);
}

/**
 * Get pointer device for given port.
 *
 * @param udev UDev system to search.
 * @param port Target port.
 */
static udev_input_device_t *udev_get_pointer_port_dev(
      struct udev_input *udev, unsigned port)
{
   uint16_t i;
   uint16_t pointer_index    = 0;
   int16_t dev_index         = -1;
   settings_t *settings      = config_get_ptr();
   udev_input_mouse_t *mouse = NULL;

   if (port >= MAX_USERS || !video_driver_has_focus())
      return NULL;

   pointer_index = settings->uints.input_mouse_index[port];
   if (pointer_index < MAX_INPUT_DEVICES)
       dev_index = udev->pointers[pointer_index];
   if (dev_index < 0)
       return NULL;
   else
       return udev->devices[dev_index];
}

/**
 * Dump information about the given absinfo structure.
 *
 * @param label Label to prefix the message with.
 * @param info Info structure to dump.
 */
static void udev_dump_absinfo(const char *label, const struct input_absinfo *info)
{
   RARCH_DBG("[udev] %s: %d %d-%d ~%d |%d r%d\n", label, 
           info->value, info->minimum, info->maximum, 
           info->fuzz, info->flat, info->resolution);
}

/**
 * Dump information about the given touch limits structure.
 * 
 * @param label Label to prefix the message with.
 * @param limits Limits structure to dump.
 */
static void udev_dump_touch_limit(const char *label, const udev_input_touch_limits_t *limits)
{
   if (limits->enabled)
   {
      RARCH_DBG("[udev] %s: %d-%d [%d] ~%d |%d r%d\n", label, 
                limits->min, limits->max, limits->range, 
                limits->fuzz, limits->flat, limits->resolution);
   }
   else
   {
      RARCH_DBG("[udev] %s: DISABLED\n", label);
   }
}

/* Convert given udev ABS_MT_* code to string representation. */
static const char *udev_mt_code_to_str(uint32_t code)
{
   switch (code)
   {
      case ABS_MT_SLOT: 
         return "ABS_MT_SLOT";
      case ABS_MT_TOUCH_MAJOR: 
         return "ABS_MT_TOUCH_MAJOR";
      case ABS_MT_TOUCH_MINOR: 
         return "ABS_MT_TOUCH_MINOR";
      case ABS_MT_WIDTH_MAJOR: 
         return "ABS_MT_WIDTH_MAJOR";
      case ABS_MT_WIDTH_MINOR: 
         return "ABS_MT_WIDTH_MINOR";
      case ABS_MT_ORIENTATION: 
         return "ABS_MT_ORIENTATION";
      case ABS_MT_POSITION_X: 
         return "ABS_MT_POSITION_X";
      case ABS_MT_POSITION_Y: 
         return "ABS_MT_POSITION_Y";
      case ABS_MT_TOOL_TYPE: 
         return "ABS_MT_TOOL_TYPE";
      case ABS_MT_BLOB_ID: 
         return "ABS_MT_BLOB_ID";
      case ABS_MT_TRACKING_ID: 
         return "ABS_MT_TRACKING_ID";
      case ABS_MT_PRESSURE: 
         return "ABS_MT_PRESSURE";
      case ABS_MT_DISTANCE: 
         return "ABS_MT_DISTANCE";
      case ABS_MT_TOOL_X: 
         return "ABS_MT_TOOL_X";
      case ABS_MT_TOOL_Y: 
         return "ABS_MT_TOOL_Y";
      default:
         return "UNKNOWN";
   }
}

/**
 * Dump information contained within given request_data with structure: 
 * struct input_mt_request_layout {
 *     __u32 code;
 *     __s32 values[count];
 * };
 * For reference, see linux kernel: include/uapi/linux/input.h#L147
 * 
 * @param label Label to prefix the message with.
 * @param request_data Input data structure to dump.
 * @param count Number of elements in the values array.
 */
static void udev_dump_mt_request_data(const char *label, const uint8_t *request_data, size_t count)
{
   uint32_t *mt_req_code = (uint32_t*) request_data;
   int32_t *mt_req_values = ((int32_t*) request_data) + 1;
   RARCH_DBG("[udev] %s: Req { %s, [ ", label, udev_mt_code_to_str(*mt_req_code));
   for (; mt_req_values < (((int32_t*) mt_req_code) + count + 1); ++mt_req_values)
   {
      RARCH_DBG("%d, ", *mt_req_values);
   }
   RARCH_DBG("]\n");
}

/* Convert given UDEV_TOUCH_CHANGE_* code to string representation */
static const char *udev_touch_change_to_str(uint16_t change)
{
   switch (change)
   {
      case UDEV_TOUCH_CHANGE_NONE:
         return "NONE";
      case UDEV_TOUCH_CHANGE_DOWN:
         return "DOWN";
      case UDEV_TOUCH_CHANGE_UP:
         return "UP";
      case UDEV_TOUCH_CHANGE_MOVE:
         return "MOVE";
      default:
         return "UNKNOWN";
   }
}

/**
 * Dump information about given touchscreen slot.
 *
 * @param label Label prefix for the printed line.
 * @param slot_state Input slot state to print.
 */
static void udev_dump_touch_slot(const char *label, const udev_slot_state_t *slot_state)
{
   RARCH_DBG("[udev] %s\t(%u,%u) %d:%hdx%hd (@%hdx%hd) %hd <%hdx%hd> %s\n", label, 
             slot_state->td_time.s, slot_state->td_time.us, 
             slot_state->tracking_id, 
             slot_state->pos_x, slot_state->pos_y, 
             slot_state->td_pos_x, slot_state->td_pos_y, 
             slot_state->pressure, 
             slot_state->minor, slot_state->major, 
             udev_touch_change_to_str(slot_state->change));
}

/**
 * Dump information about tracked slots in given touchscreen device.
 * 
 * @param indent Indentation prefix for each printed line.
 * @param touch Input touchscreen device to dump.
 */
static void udev_dump_touch_device_slots(const char *indent, const udev_input_touch_t *touch)
{
   int iii;

   RARCH_DBG("[udev] %sStagingSlots: {\n", indent);
   if (touch->staging != NULL)
   {
      for (iii = 0; iii < touch->info_slots.range; ++iii)
      {
         udev_dump_touch_slot(indent, &touch->staging[iii]);
      }
   }
   else
   {
      RARCH_DBG("[udev] %s\tNOT ALLOCATED\n", indent);
   }
   RARCH_DBG("[udev] %s}\n", indent);

   RARCH_DBG("[udev] %sCurrentSlots: {\n", indent);
   if (touch->staging != NULL)
   {
      for (iii = 0; iii < touch->info_slots.range; ++iii)
      {
         udev_dump_touch_slot(indent, &touch->current[iii]);
      }
   }
   else
   {
      RARCH_DBG("[udev] %s\tNOT ALLOCATED\n", indent);
   }
   RARCH_DBG("[udev] %s}\n", indent);
}

/**
 * Set touch limits from given absinfo structure.
 *
 * @param info Input info structure.
 * @param limits Target limits structure.
 */
static void udev_touch_set_limits_from(struct input_absinfo *info, 
        udev_input_touch_limits_t *limits)
{
   limits->enabled = true;
   limits->min = info->minimum;
   limits->max = info->maximum;
   limits->range = limits->max - limits->min + 1;
   limits->fuzz = info->fuzz;
   limits->flat = info->flat;
   limits->resolution = info->resolution;
}

/**
 * Dump information about the provided device. Works even 
 * for non-touch devices.
 * 
 * @param dev Input device to dump information for.
 */
static void udev_dump_touch_dev(udev_input_device_t *dev)
{
   udev_input_mouse_t *mouse = &dev->mouse;
   udev_input_touch_t *touch = &dev->touch;

   RARCH_DBG("[udev] === UDEV_INPUT_DEVICE INFO DUMP ===\n");

   RARCH_DBG("[udev] \tident = %s\n", dev->ident);
   RARCH_DBG("[udev] \tdevnode = %s\n", dev->devnode);
   RARCH_DBG("[udev] \ttype = %s\n", g_dev_type_str[dev->type]);
   RARCH_DBG("[udev] \thandle_cb = %p\n", dev->handle_cb);
   RARCH_DBG("[udev] \tfd = %d\n", dev->fd);
   RARCH_DBG("[udev] \tdev = %lu\n", dev->dev);

   RARCH_DBG("[udev] \tmouse = {\n");
   RARCH_DBG("[udev] \t\tabs = %d\n", mouse->abs);
   RARCH_DBG("[udev] \t\tx -> %d~%d (%d-%d)\n", 
             mouse->x_abs, mouse->x_rel, 
             mouse->x_min, mouse->x_max);
   RARCH_DBG("[udev] \t\ty -> %d~%d (%d-%d)\n", 
             mouse->y_abs, mouse->y_rel, 
             mouse->y_min, mouse->y_max);
   RARCH_DBG("[udev] \t\tL = %c | R = %c | M = %c | 4 = %c | 5 = %c\n", 
             mouse->l ? 'X' : 'O', mouse->r ? 'X' : 'O', 
             mouse->m ? 'X' : 'O', 
             mouse->b4 ? 'X' : 'O', mouse->b5 ? 'X' : 'O');
   RARCH_DBG("[udev] \t\tWU = %c | WD = %c | WHU = %c | WHD = %c\n", 
             mouse->wu ? 'X' : 'O', mouse->wd ? 'X' : 'O', 
             mouse->whu ? 'X' : 'O', mouse->whd ? 'X' : 'O');
   RARCH_DBG("[udev] \t\tPP = %c\n", mouse->pp ? 'X' : 'O');
   RARCH_DBG("[udev] \t}\n");

   RARCH_DBG("[udev] \ttouch = {\n");
   udev_dump_touch_limit("\t\tSlots", &touch->info_slots);
   udev_dump_touch_limit("\t\tX-Axis", &touch->info_x_limits);
   udev_dump_touch_limit("\t\tY-Axis", &touch->info_y_limits);
   udev_dump_touch_limit("\t\tMajor", &touch->info_major);
   udev_dump_touch_limit("\t\tMinor", &touch->info_minor);
   udev_dump_touch_limit("\t\tPressure", &touch->info_pressure);
   udev_dump_mt_request_data("\t\tRequestData", touch->request_data, touch->info_slots.range);
   udev_dump_touch_device_slots("\t\t", touch);

   RARCH_DBG("[udev] \t}\n");

   RARCH_DBG("[udev] === END OF INFO DUMP ===\n");
}

/**
 * Cleanup and destroy given touch device.
 *
 * @param dev Device to cleanup and destroy.
 */
static void udev_destroy_touch_dev(udev_input_device_t *dev)
{
   udev_input_touch_t *touch = &dev->touch;

   RARCH_DBG("[udev] Destroying touch device \"%s\"\n", dev->ident);

   if (touch->request_data != NULL)
   {
       free(touch->request_data);
       touch->request_data = NULL;
       touch->request_data_size = 0;
   }

   if (touch->staging != NULL)
   {
       free(touch->staging);
       touch->staging = NULL;
   }

   if (touch->current != NULL)
   {
       free(touch->current);
       touch->current = NULL;
   }

   touch->current_slot = UDEV_INPUT_TOUCH_SLOT_ID_NONE;
}

/**
 * Initialize given touch device.
 *
 * @param dev Input touch device to initialize.
 */
static void udev_init_touch_dev(udev_input_device_t *dev)
{
   udev_input_touch_t *touch;
   struct input_absinfo abs_info;
   int iii, ret;
   unsigned long xreq, yreq;
   settings_t *settings;

   touch = &dev->touch;
   settings = config_get_ptr();

   RARCH_DBG("[udev] Initializing touch device \"%s\"\n", dev->ident);

   /* TODO - Unused for now. */
   UDEV_INPUT_TOUCH_UNUSED(udev_touch_dir_to_str);
   UDEV_INPUT_TOUCH_UNUSED(udev_touch_dir_top);
   UDEV_INPUT_TOUCH_UNUSED(udev_touch_dir_left);
   UDEV_INPUT_TOUCH_UNUSED(udev_touch_dir_bot);
   UDEV_INPUT_TOUCH_UNUSED(udev_touch_dir_right);
   UDEV_INPUT_TOUCH_UNUSED(udev_touch_min);
   UDEV_INPUT_TOUCH_UNUSED(udev_touch_max);
   UDEV_INPUT_TOUCH_UNUSED(udev_dump_absinfo);
   UDEV_INPUT_TOUCH_UNUSED(udev_touch_ts_print);

   /* Get slot limits - number of touch points */
   ret = ioctl(dev->fd, EVIOCGABS(ABS_MT_SLOT), &abs_info);
   if (ret < 0) 
   {
      RARCH_WARN("[udev] Failed to get touchscreen limits\n");

      touch->info_slots.enabled = false;
   }
   else 
   {
      udev_touch_set_limits_from(&abs_info, &touch->info_slots);
   }

   /* Single-touch devices use ABS_X/Y, Multi-touch use ABS_MT_POSITION_X/Y */
   if (abs_info.maximum == 0) 
   {
      /* TODO - Test for single-touch devices. */
      RARCH_WARN("[udev] Single-touch devices are currently untested!\n");

      xreq = EVIOCGABS(ABS_X);
      yreq = EVIOCGABS(ABS_Y);
   }
   else 
   {
      xreq = EVIOCGABS(ABS_MT_POSITION_X);
      yreq = EVIOCGABS(ABS_MT_POSITION_Y);
   }

   /* Get x-axis limits */
   ret = ioctl(dev->fd, xreq, &abs_info);
   if (ret < 0) 
   {
      RARCH_DBG("[udev] Failed to get touchscreen x-limits\n");

      touch->info_x_limits.enabled = false;
   }
   else 
   {
      udev_touch_set_limits_from(&abs_info, &touch->info_x_limits);
   }

   /* Get y-axis limits */
   ret = ioctl(dev->fd, yreq, &abs_info);
   if (ret < 0) 
   {
      RARCH_DBG("[udev] Failed to get touchscreen y-limits\n");

      touch->info_y_limits.enabled = false;
   }
   else 
   {
      udev_touch_set_limits_from(&abs_info, &touch->info_y_limits);
   }

   /* Get major axis limits - i.e. primary radius of the touch */
   ret = ioctl(dev->fd, EVIOCGABS(ABS_MT_TOUCH_MAJOR), &abs_info);
   if (ret < 0) 
   {
      RARCH_DBG("[udev] Failed to get touchscreen major limits\n");

      touch->info_major.enabled = false;
   }
   else 
   {
      udev_touch_set_limits_from(&abs_info, &touch->info_major);
   }

   /* Get minor axis limits - i.e. secondary radius of the touch */
   ret = ioctl(dev->fd, EVIOCGABS(ABS_MT_TOUCH_MINOR), &abs_info);
   if (ret < 0) 
   {
      RARCH_DBG("[udev] Failed to get touchscreen minor limits\n");

      touch->info_minor.enabled = false;
   }
   else 
   {
      udev_touch_set_limits_from(&abs_info, &touch->info_minor);
   }

   /* Get pressure limits */
   ret = ioctl(dev->fd, EVIOCGABS(ABS_MT_PRESSURE), &abs_info);
   if (ret < 0) 
   {
      RARCH_DBG("[udev] Failed to get touchscreen pres-limits\n");

      touch->info_pressure.enabled = false;
   }
   else 
   {
      udev_touch_set_limits_from(&abs_info, &touch->info_pressure);
   }

   /* Allocate the data blocks required for state tracking */
   /* __u32 + __s32[num_slots] */
   touch->request_data_size = sizeof(int32_t) + sizeof(uint32_t) * touch->info_slots.range;
   touch->request_data = calloc(1, touch->request_data_size);
   if (touch->request_data == NULL)
   {
      RARCH_ERR("[udev] Failed to allocate request_data for touch state tracking!\n");
      udev_destroy_touch_dev(dev);
      return;
   }

   touch->staging = calloc(1, sizeof(*touch->staging) * touch->info_slots.range);
   touch->staging_active = 0;
   if (touch->staging == NULL)
   {
      RARCH_ERR("[udev] Failed to allocate staging for touch state tracking!\n");
      udev_destroy_touch_dev(dev);
      return;
   }
   touch->current = calloc(1, sizeof(*touch->current) * touch->info_slots.range);
   touch->current_active = 0;
   if (touch->current == NULL)
   {
      RARCH_ERR("[udev] Failed to allocate current for touch state tracking!\n");
      udev_destroy_touch_dev(dev);
      return;
   }

   /* Initialize touch device */
   touch->current_slot = UDEV_INPUT_TOUCH_SLOT_ID_NONE;
   touch->is_touch_device = true;
   touch->run_state_update = false;
   udev_touch_ts_now(&touch->last_state_update);

   /* Initialize pointer simulation */
   touch->pointer_enabled = UDEV_INPUT_TOUCH_POINTER_EN;
   touch->pointer_btn_pp = false;
   touch->pointer_btn_pb = false;
   touch->pointer_pos_x = 0;
   touch->pointer_pos_y = 0;
   touch->pointer_ma_pos_x = 0;
   touch->pointer_ma_pos_y = 0;
   touch->pointer_ma_rel_x = 0;
   touch->pointer_ma_rel_y = 0;
   touch->pointer_scr_pos_x = 0;
   touch->pointer_scr_pos_y = 0;
   touch->pointer_vp_pos_x = 0;
   touch->pointer_vp_pos_y = 0;

   /* Initialize mouse simulation */
   touch->mouse_enabled = UDEV_INPUT_TOUCH_MOUSE_EN;
   touch->mouse_freeze_cursor = false;
   touch->mouse_scr_pos_x = 0;
   touch->mouse_scr_pos_y = 0;
   touch->mouse_vp_pos_x = 0;
   touch->mouse_vp_pos_y = 0;
   touch->mouse_rel_x = 0;
   touch->mouse_rel_y = 0;
   touch->mouse_wheel_x = 0;
   touch->mouse_wheel_y = 0;
   touch->mouse_btn_l = false;
   touch->mouse_btn_r = false;
   touch->mouse_btn_m = false;
   touch->mouse_btn_b4 = false;
   touch->mouse_btn_b5 = false;

   /* Initialize touchpad simulation */
   touch->touchpad_enabled = UDEV_INPUT_TOUCH_TOUCHPAD_EN;
   touch->touchpad_sensitivity = UDEV_INPUT_TOUCH_PAD_SENSITIVITY;
   touch->touchpad_pos_x = 0.0f;
   touch->touchpad_pos_y = 0.0f;

   /* Initialize trackball simulation */
   touch->trackball_enabled = UDEV_INPUT_TOUCH_TRACKBALL_EN;
   touch->trackball_inertial = false;
   touch->trackball_pos_x = 0.0f;
   touch->trackball_pos_y = 0.0f;
   touch->trackball_sensitivity_x = UDEV_INPUT_TOUCH_TRACKBALL_SENSITIVITY_X;
   touch->trackball_sensitivity_y = UDEV_INPUT_TOUCH_TRACKBALL_SENSITIVITY_Y;
   touch->trackball_vel_x = 0.0f;
   touch->trackball_vel_y = 0.0f;
   touch->trackball_frict_x = UDEV_INPUT_TOUCH_TRACKBALL_FRICT_X;
   touch->trackball_frict_y = UDEV_INPUT_TOUCH_TRACKBALL_FRICT_Y;
   touch->trackball_sq_vel_cutoff = UDEV_INPUT_TOUCH_TRACKBALL_SQ_VEL_CUTOFF;

   /* Initialize gestures */
   touch->gest_enabled = UDEV_INPUT_TOUCH_GEST_EN;
   touch->gest_primary_slot = UDEV_INPUT_TOUCH_SLOT_ID_NONE;
   touch->gest_secondary_slot = UDEV_INPUT_TOUCH_SLOT_ID_NONE;
   touch->gest_timeout = UDEV_INPUT_TOUCH_GEST_TIMEOUT;
   touch->gest_tap_time = UDEV_INPUT_TOUCH_MAX_TAP_TIME;
   touch->gest_tap_dist = UDEV_INPUT_TOUCH_MAX_TAP_DIST;
   touch->gest_click_time = UDEV_INPUT_TOUCH_GEST_CLICK_TIME;
   touch->gest_tap_count = 0;
   touch->gest_tap_possible = false;
   touch->gest_scroll_sensitivity = UDEV_INPUT_TOUCH_GEST_SCROLL_SENSITIVITY;
   touch->gest_scroll_step = UDEV_INPUT_TOUCH_GEST_SCROLL_STEP;
   touch->gest_scroll_x = 0;
   touch->gest_scroll_y = 0;
   touch->gest_corner = UDEV_INPUT_TOUCH_GEST_CORNER;

   for (iii = 0; iii < UDEV_TOUCH_TGEST_LAST; ++iii)
   {
      touch->gest_tcbs[iii].active = false;
      touch->gest_tcbs[iii].execute_at.s = 0;
      touch->gest_tcbs[iii].execute_at.us = 0;
      touch->gest_tcbs[iii].cb = NULL;
      touch->gest_tcbs[iii].data = NULL;
   }

   touch->gest_mgest_type = UDEV_TOUCH_MGEST_NONE;
   for (iii = 0; iii < UDEV_TOUCH_MGEST_LAST; ++iii)
   {
      touch->gest_mcbs[iii].active = false;
      touch->gest_mcbs[iii].execute_at.s = 0;
      touch->gest_mcbs[iii].execute_at.us = 0;
      touch->gest_mcbs[iii].cb = NULL;
      touch->gest_mcbs[iii].data = NULL;
   }

   /* Print debug information */
   udev_dump_touch_dev(dev);
}

/**
 * Fully synchronize the statue of given touch device.
 * This is inefficient compared to the event-driven 
 * approach, but results in fully synchronized state.
 *
 * @param dev Input touch device to synchronize.
 */
static void udev_sync_touch(udev_input_device_t *dev)
{
   int iii, ret;
   struct input_absinfo abs_info;
   uint32_t *mt_req_code;
   int32_t *mt_req_values;
   uint8_t *mt_request_data;
   size_t mt_request_data_size;
   size_t slot_count;
   udev_touch_ts_t now;
   udev_input_touch_t *touch = &dev->touch;
   udev_slot_state_t *staging = dev->touch.staging;

   RARCH_DDBG("[udev] Synchronizing touch data...\n");

   /* Get time for timestamp purposes */
   /*ktime_get_ts64(&ts); */
   /*u64_t Vkk */

   /*
    * Data block compatible with the following structure: 
    * struct input_mt_request_layout {
    *     __u32 code;
    *     __s32 values[slot_count];
    * };
    */
   mt_request_data = touch->request_data;
   mt_request_data_size = touch->request_data_size;
   slot_count = touch->info_slots.range;
   
   /* Get current time for initialization */
   udev_touch_ts_now(&now);

   if (mt_request_data == NULL)
   {
      return;
   }

   mt_req_code = (uint32_t*) mt_request_data;
   mt_req_values = ((int32_t*) mt_request_data) + 1;

   /* Request tracking IDs for each slot */
   *mt_req_code = ABS_MT_TRACKING_ID;
   ret = ioctl(dev->fd, EVIOCGMTSLOTS(mt_request_data_size), mt_request_data);
   if (ret < 0)
   {
      return;
   }
   RARCH_DDBG("[udev] \tTracking IDs:\n");
   for (iii = 0; iii < slot_count; ++iii)
   {
      RARCH_DDBG("[udev] \t\t%d: %d -> %d\n", iii, staging[iii].tracking_id, mt_req_values[iii]);
      staging[iii].tracking_id = mt_req_values[iii];
      udev_touch_ts_copy(&now, &staging[iii].td_time);
   }

   /* Request MT position on the x-axis for each slot */
   *mt_req_code = ABS_MT_POSITION_X;
   ret = ioctl(dev->fd, EVIOCGMTSLOTS(mt_request_data_size), mt_request_data);
   if (ret < 0)
   {
      return;
   }
   RARCH_DDBG("[udev] \tMT X-Positions:\n");
   for (iii = 0; iii < slot_count; ++iii)
   {
      RARCH_DDBG("[udev] \t\t%d: %d -> %d\n", iii, staging[iii].pos_x, mt_req_values[iii]);
      staging[iii].pos_x = mt_req_values[iii];
      staging[iii].td_pos_x = mt_req_values[iii];
   }

   /* Request MT position on the y-axis for each slot */
   *mt_req_code = ABS_MT_POSITION_Y;
   ret = ioctl(dev->fd, EVIOCGMTSLOTS(mt_request_data_size), mt_request_data);
   if (ret < 0)
   {
      return;
   }
   RARCH_DDBG("[udev] \tMT Y-Positions:\n");
   for (iii = 0; iii < slot_count; ++iii)
   {
      RARCH_DDBG("[udev] \t\t%d: %d -> %d\n", iii, staging[iii].pos_y, mt_req_values[iii]);
      staging[iii].pos_y = mt_req_values[iii];
      staging[iii].td_pos_y = mt_req_values[iii];
   }

   /* Request minor axis for each slot */
   *mt_req_code = ABS_MT_TOUCH_MINOR;
   ret = ioctl(dev->fd, EVIOCGMTSLOTS(mt_request_data_size), mt_request_data);
   if (ret < 0)
   {
      return;
   }
   RARCH_DDBG("[udev] \tMinor:\n");
   for (iii = 0; iii < slot_count; ++iii)
   {
      RARCH_DDBG("[udev] \t\t%d: %d -> %d\n", iii, staging[iii].minor, mt_req_values[iii]);
      staging[iii].minor = mt_req_values[iii];
   }

   /* Request major axis for each slot */
   *mt_req_code = ABS_MT_TOUCH_MAJOR;
   ret = ioctl(dev->fd, EVIOCGMTSLOTS(mt_request_data_size), mt_request_data);
   if (ret < 0)
   {
      return;
   }
   RARCH_DDBG("[udev] \tMajor:\n");
   for (iii = 0; iii < slot_count; ++iii)
   {
      RARCH_DDBG("[udev] \t\t%d: %d -> %d\n", iii, staging[iii].major, mt_req_values[iii]);
      staging[iii].major = mt_req_values[iii];
   }

   /* Request major axis for each slot */
   *mt_req_code = ABS_MT_PRESSURE;
   ret = ioctl(dev->fd, EVIOCGMTSLOTS(mt_request_data_size), mt_request_data);
   if (ret < 0)
   {
      return;
   }
   RARCH_DDBG("[udev] \tPressure:\n");
   for (iii = 0; iii < slot_count; ++iii)
   {
      RARCH_DDBG("[udev] \t\t%d: %d -> %d\n", iii, staging[iii].pressure, mt_req_values[iii]);
      staging[iii].pressure = mt_req_values[iii];
   }
   
   /* Get the current slot */
   ret = ioctl(dev->fd, EVIOCGABS(ABS_MT_SLOT), &abs_info);
   if (ret < 0)
   {
       return;
   }
   RARCH_DDBG("[udev] \tCurrent slot: %d -> %d\n", touch->current_slot, abs_info.value);
   touch->current_slot = abs_info.value;
}

/**
 * Translate touch panel position into other coordinate systems.
 *
 * @param src_touch Information concerning the source touch panel.
 * @param target_vp Information about the target panel. Pre-filled 
 *   with the video_driver_get_viewport_info function.
 * @param pointer_pos_x Input x-coordinate on the touch panel.
 * @param pointer_pos_y Input y-coordinate on the touch panel.
 * @param pointer_ma_pos_x Output x-coordinate on the target panel.
 * @param pointer_ma_pos_y Output y-coordinate on the target panel.
 * @param pointer_ma_rel_x Output x-coordinate change on the target panel.
 *   The resulting delta is added to this output!
 * @param pointer_ma_rel_y Output y-coordinate change on the target panel.
 *   The resulting delta is added to this output!
 * @param pointer_scr_pos_x Output x-coordinate on the target screen. 
 *   Uses the scaled coordinates -0x7fff - 0x7fff.
 * @param pointer_vp_pos_x Output x-coordinate on the target window. 
 *   Uses the scaled coordinates -0x7fff - 0x7fff and -0x8000 when OOB.
 * @return Returns true on success.
 */
static bool udev_translate_touch_pos(
        const udev_input_touch_t *src_touch, 
        video_viewport_t *target_vp, 
        int32_t pointer_pos_x, int32_t pointer_pos_y, 
        int32_t *pointer_ma_pos_x, int32_t *pointer_ma_pos_y, 
        int16_t *pointer_ma_rel_x, int16_t *pointer_ma_rel_y, 
        int16_t *pointer_scr_pos_x, int16_t *pointer_scr_pos_y, 
        int16_t *pointer_vp_pos_x, int16_t *pointer_vp_pos_y)
{
   /* Touch panel -> Main panel */
   /*
    * TODO - This keeps the precision, but might result in +-1 pixel difference
    *   One way to fix this is to add or remove 0.5, but this needs floating 
    *   point operations which might not be desireable.
    */
   int32_t ma_pos_x = (((((pointer_pos_x + src_touch->info_x_limits.min) * 0x7fff) / src_touch->info_x_limits.range) * target_vp->full_width) / 0x7fff);
   int32_t ma_pos_y = (((((pointer_pos_y + src_touch->info_y_limits.min) * 0x7fff) / src_touch->info_y_limits.range) * target_vp->full_height) / 0x7fff);

   /* Calculate relative offsets. */
   *pointer_ma_rel_x += ma_pos_x - *pointer_ma_pos_x;
   *pointer_ma_rel_y += ma_pos_y - *pointer_ma_pos_y;

   /* Set the new main panel positions. */
   *pointer_ma_pos_x = ma_pos_x;
   *pointer_ma_pos_y = ma_pos_y;

   /* Main panel -> Screen and Viewport */
   return video_driver_translate_coord_viewport(
      target_vp, *pointer_ma_pos_x, *pointer_ma_pos_y, 
      pointer_vp_pos_x, pointer_vp_pos_y, 
      pointer_scr_pos_x, pointer_scr_pos_y
   );
}

/**
 * Setup a delayed callback. Automatically activates it.
 *
 * @param timed_cb Target structure to fill.
 * @param now Current time.
 * @param delay_us Delay in microseconds.
 * @param cb Callback function to execute.
 * @param data Data passed to the callback.
 */
static void udev_input_touch_gest_set_cb(
        udev_touch_timed_cb_t *timed_cb, 
        const udev_touch_ts_t *now, 
        uint32_t delay_us, 
        void (*cb) (void*, void*), void *data)
{
   timed_cb->active = true;
   udev_touch_ts_add(now, 0, delay_us, &timed_cb->execute_at);
   timed_cb->cb = cb;
   timed_cb->data = data;
}

/**
 * Execute given callback and set it as inactive.
 *
 * @param timed_cb Callback to execute.
 * @param touch Touch structure to pass to the callback.
 */
static void udev_input_touch_gest_exec_cb(
        udev_touch_timed_cb_t *timed_cb, 
        udev_input_touch_t *touch)
{
   timed_cb->active = false;
   timed_cb->cb(touch, timed_cb->data);
}

/**
 * Disable given callback, rendering it inactive.
 *
 * @param timed_cb Callback to execute.
 */
static void udev_input_touch_gest_disable_cb(
        udev_touch_timed_cb_t *timed_cb)
{
   timed_cb->active = false;
}

/* Callback used for resetting boolean variable to false. */
static void udev_input_touch_gest_reset_bool(void *touch, void *tgt)
{ *((bool*)tgt) = false; }
/* Callback used for resetting mouse_wheel_x/y variables to 0. */
static void udev_input_touch_gest_reset_scroll(void *touch, void *none)
{ 
   ((udev_input_touch_t*)touch)->gest_scroll_x = 0; 
   ((udev_input_touch_t*)touch)->gest_scroll_y = 0; 
}

/**
 * Runner for special functions. Currently, the following 
 * functions are implemented: 
 * * Top Left corner -> Enable/Disable mouse.
 * * Top Right corner -> Enable/Disable touchpad.
 * * Bottom Left corner -> Enable/Disable pointer.
 * * Bottom Right corner -> Enable/Disable trackball.
 */
static bool udev_input_touch_tgest_special(udev_input_touch_t *touch, 
        const udev_touch_ts_t *now, int32_t pos_x, int32_t pos_y)
{
   /* Did we act for the input position? */
   bool serviced = false;
   /* Convert pos_x/y into percentage of touch panel. */
   int32_t ptg_x = ((pos_x + touch->info_x_limits.min) * 100) / touch->info_x_limits.range;
   int32_t ptg_y = ((pos_y + touch->info_y_limits.min) * 100) / touch->info_y_limits.range;

   /* Negative (left, bottom) and positive (right, up) corner percentages */
   uint16_t ptg_corner_neg = touch->gest_corner;
   uint16_t ptg_corner_pos = 100 - ptg_corner_neg;

   /* TODO - Currently unused. */
   UDEV_INPUT_TOUCH_UNUSED(now);

   if (ptg_y < ptg_corner_neg && ptg_x < ptg_corner_neg)
   { /* Top Left corner */
      touch->mouse_enabled = !touch->mouse_enabled;
      RARCH_DBG("[udev] TGesture: SF/TT -> Top Left: Mouse %s\n", 
              touch->mouse_enabled ? "enabled" : "disabled");
      serviced = true;
   }
   else if (ptg_y < ptg_corner_neg && ptg_x > ptg_corner_pos)
   { /* Top Right corner */
      touch->touchpad_enabled = !touch->touchpad_enabled;
      RARCH_DBG("[udev] TGesture: SF/TT -> Top Right: Touchpad %s\n", 
              touch->touchpad_enabled ? "enabled" : "disabled");
      serviced = true;
   }
   else if (ptg_y > ptg_corner_pos && ptg_x < ptg_corner_neg)
   { /* Bottom Left corner */
      touch->pointer_enabled = !touch->pointer_enabled;
      RARCH_DBG("[udev] TGesture: SF/TT -> Bottom Left: Pointer %s\n", 
              touch->pointer_enabled ? "enabled" : "disabled");
      serviced = true;
   }
   else if (ptg_y > ptg_corner_pos && ptg_x > ptg_corner_pos)
   { /* Bottom Right corner */
      touch->trackball_enabled = !touch->trackball_enabled;
      RARCH_DBG("[udev] TGesture: SF/TT -> Bottom Right: Trackball %s\n", 
              touch->gest_enabled ? "enabled" : "disabled");
      serviced = true;
   }

   return serviced;
}

/**
 * Callback performing tap gesture selection, usually triggered 
 * upon reaching gesture timeout.
 *
 * @param touch_ptr Pointer to the touch structure.
 * @param data Additional data. TODO - Currently unused (NULL).
 */
static void udev_input_touch_tgest_select(void *touch_ptr, void *data)
{
   /* Based on the gesture, change current touch state */
   udev_input_touch_t *touch = (udev_input_touch_t*) touch_ptr;

   /* Get current time for measurements */
   udev_touch_ts_t now;
   udev_touch_ts_now(&now);

   if (touch->staging_active == 0)
   { /* No hold with single finger tap */
      switch (touch->gest_tap_count)
      {
         case 1:
            /* One tap -> Left mouse button */
            touch->mouse_btn_l = true;
            /* Setup callback to reset the button state. */
            udev_input_touch_gest_set_cb(
               &touch->gest_tcbs[UDEV_TOUCH_TGEST_S_TAP], 
               &now, touch->gest_click_time, 
               udev_input_touch_gest_reset_bool, &touch->mouse_btn_l);
            RARCH_DDBG("[udev] TGesture: SF/ST -> Left Mouse Button\n");
            break;
         case 2:
            /* Two taps -> Right mouse button */
            touch->mouse_btn_r = true;
            /* Setup callback to reset the button state. */
            udev_input_touch_gest_set_cb(
               &touch->gest_tcbs[UDEV_TOUCH_TGEST_D_TAP], 
               &now, touch->gest_click_time, 
               udev_input_touch_gest_reset_bool, &touch->mouse_btn_r);
            RARCH_DDBG("[udev] TGesture: SF/DT -> Right Mouse Button\n");
            break;
         case 3:
            /* Three taps -> Middle mouse button & Specials */
            if (!udev_input_touch_tgest_special(touch, &now, 
                        touch->pointer_pos_x, touch->pointer_pos_y))
            {
               touch->mouse_btn_m = true;
               /* Setup callback to reset the button state. */
               udev_input_touch_gest_set_cb(
                  &touch->gest_tcbs[UDEV_TOUCH_TGEST_T_TAP], 
                  &now, touch->gest_click_time, 
                  udev_input_touch_gest_reset_bool, &touch->mouse_btn_r);
               RARCH_DDBG("[udev] TGesture: SF/TT -> Middle Mouse Button\n");
            }
            break;
         default:
            /* More taps -> No action */
            break;
      }
   }
   else if (touch->staging_active == 1)
   { /* Single hold with second finger tap */
      touch->mouse_btn_r = true;
      /* Setup callback to reset the button state. */
      udev_input_touch_gest_set_cb(
         &touch->gest_tcbs[UDEV_TOUCH_TGEST_D_TAP], 
         &now, touch->gest_click_time, 
         udev_input_touch_gest_reset_bool, &touch->mouse_btn_r);
      RARCH_DDBG("[udev] TGesture: DF/ST -> Right Mouse Button\n");
   }
   else if (touch->staging_active == 2)
   { /* Two hold with third finger tap */
      touch->mouse_btn_m = true;
      /* Setup callback to reset the button state. */
      udev_input_touch_gest_set_cb(
         &touch->gest_tcbs[UDEV_TOUCH_TGEST_T_TAP], 
         &now, touch->gest_click_time, 
         udev_input_touch_gest_reset_bool, &touch->mouse_btn_m);
      RARCH_DDBG("[udev] TGesture: TF/ST -> Middle Mouse Button\n");
   }

   /* Reset the tap count for the next gesture. */
   touch->gest_tap_count = 0;
}

/**
 * Reset the move-based gestures.
 *
 * @param touch Pointer to the touch structure.
 */
static void udev_input_touch_mgest_reset(udev_input_touch_t *touch, 
        udev_input_touch_slot_id current_slot_id, 
        const udev_touch_ts_t *now)
{
   switch (touch->gest_mgest_type)
   {
      case UDEV_TOUCH_MGEST_S_STAP_DRAG: 
         /* Gesture for one finger single tap drag. */
         touch->mouse_btn_l = false;
         break;
      case UDEV_TOUCH_MGEST_S_DTAP_DRAG: 
         /* Gesture for one finger double tap drag. */
         touch->mouse_btn_r = false;
         break;
      case UDEV_TOUCH_MGEST_S_TTAP_DRAG: 
         /* Gesture for one finger triple tap drag. */
         touch->mouse_btn_m = false;
         break;
      case UDEV_TOUCH_MGEST_D_NTAP_DRAG: 
         /* Gesture for two finger no tap drag. */
         /* Completely reset the scroll after timeout. */
         udev_input_touch_gest_set_cb(
            &touch->gest_mcbs[UDEV_TOUCH_MGEST_D_NTAP_DRAG], 
            now, touch->gest_timeout, 
            udev_input_touch_gest_reset_scroll, NULL);
         break;
      case UDEV_TOUCH_MGEST_D_STAP_DRAG: 
         /* Gesture for two finger single tap drag. */
         break;
      case UDEV_TOUCH_MGEST_T_NTAP_DRAG: 
         /* Gesture for three finger no tap drag. */
         break;
      case UDEV_TOUCH_MGEST_T_STAP_DRAG: 
         /* Gesture for three finger single tap drag. */
         break;
      default:
         /* No action */
         break;
   }

   /* Reset the current move-gesture */
   touch->gest_mgest_type = UDEV_TOUCH_MGEST_NONE;
}

/**
 * Perform move-based gesture selection, usually triggered upon 
 * movement after tap is impossible.
 *
 * @param touch Pointer to the touch structure.
 * @param current_slot_id ID of the currently serviced slot. Can be 
 *   used to compare against gest_primary/secondary_slot.
 * @param now Current time.
 */
static void udev_input_touch_mgest_select(udev_input_touch_t *touch, 
        udev_input_touch_slot_id current_slot_id, 
        const udev_touch_ts_t *now)
{
   /* Perform actions only for the primary slot */
   if (current_slot_id != touch->gest_primary_slot)
   { return; }

   if (touch->gest_mgest_type == UDEV_TOUCH_MGEST_NONE)
   { /* Determine type of move-gesture */
      if (touch->staging_active == 1)
      { /* Single finger hold with single tap */
         switch (touch->gest_tap_count)
         {
            case 1:
               /* One tap hold -> Left mouse button drag */
               touch->mouse_btn_l = true;
               touch->gest_mgest_type = UDEV_TOUCH_MGEST_S_STAP_DRAG;
               RARCH_DDBG("[udev] MGesture: SF/STD -> Left Mouse Button Drag\n");
               break;
            case 2:
               /* Two taps -> Right mouse button drag */
               touch->mouse_btn_r = true;
               touch->gest_mgest_type = UDEV_TOUCH_MGEST_S_DTAP_DRAG;
               RARCH_DDBG("[udev] MGesture: SF/DTD -> Right Mouse Button\n");
               break;
            case 3:
               /* Three taps -> Middle mouse button drag */
               touch->mouse_btn_m = true;
               touch->gest_mgest_type = UDEV_TOUCH_MGEST_S_TTAP_DRAG;
               RARCH_DDBG("[udev] MGesture: SF/TTD -> Middle Mouse Button\n");
               break;
            default:
               /* More taps drag -> No action */
               break;
         }
      }
      else if (touch->staging_active == 2)
      { /* Two finger hold with single tap */
         switch (touch->gest_tap_count)
         {
            case 0:
               /* No tap hold -> 3D wheel scrolling */
               touch->gest_mgest_type = UDEV_TOUCH_MGEST_D_NTAP_DRAG;
               RARCH_DDBG("[udev] MGesture: DF/NTD -> 3D Wheel\n");
               break;
            case 1:
               /* Single tap hold -> TODO - Some interesting action? */
               touch->gest_mgest_type = UDEV_TOUCH_MGEST_D_STAP_DRAG;
               RARCH_DDBG("[udev] MGesture: DF/STD -> TODO\n");
               break;
            default:
               /* More taps drag -> No action */
               break;
         }
      }
      else if (touch->staging_active == 3)
      { /* Three finger hold with single tap */
         switch (touch->gest_tap_count)
         {
            case 0:
               /* No tap hold -> TODO - Some interesting action? */
               touch->gest_mgest_type = UDEV_TOUCH_MGEST_T_NTAP_DRAG;
               RARCH_DDBG("[udev] MGesture: DF/NTD -> TODO\n");
               break;
            case 1:
               /* Single tap hold -> TODO - Some interesting action? */
               touch->gest_mgest_type = UDEV_TOUCH_MGEST_T_STAP_DRAG;
               RARCH_DDBG("[udev] MGesture: DF/STD -> TODO\n");
               break;
            default:
               /* More taps drag -> No action */
               break;
         }
      }

      /* Reset the tap count for the next gesture. */
      touch->gest_tap_count = 0;
   }

   /* Update the current state of move-gesture */
   switch (touch->gest_mgest_type)
   {
      case UDEV_TOUCH_MGEST_S_STAP_DRAG: 
         /* Gesture for one finger single tap drag. */
         break;
      case UDEV_TOUCH_MGEST_S_DTAP_DRAG: 
         /* Gesture for one finger double tap drag. */
         break;
      case UDEV_TOUCH_MGEST_S_TTAP_DRAG: 
         /* Gesture for one finger triple tap drag. */
         break;
      case UDEV_TOUCH_MGEST_D_NTAP_DRAG: 
         /* Gesture for two finger no tap drag. */
         touch->gest_scroll_x += 
            (
               touch->pointer_ma_rel_x * 
               touch->gest_scroll_sensitivity
            ) / touch->info_x_limits.range;
         touch->gest_scroll_y += 
            (
               touch->pointer_ma_rel_y * 
               touch->gest_scroll_sensitivity
            ) / touch->info_y_limits.range;
         RARCH_DDBG("[udev] MGesture: DF/NTD -> 3D Wheel: %hd x %hd | %hd\n", touch->gest_scroll_x, touch->gest_scroll_y, touch->gest_scroll_step);
         break;
      case UDEV_TOUCH_MGEST_D_STAP_DRAG: 
         /* Gesture for two finger single tap drag. */
         break;
      case UDEV_TOUCH_MGEST_T_NTAP_DRAG: 
         /* Gesture for three finger no tap drag. */
         break;
      case UDEV_TOUCH_MGEST_T_STAP_DRAG: 
         /* Gesture for three finger single tap drag. */
         break;
      default:
         /* No action */
         break;
   }
}

/**
 * Synchronize touch events to other endpoints and 
 * reset for the next event packet.
 *
 * @param udev Input udev system.
 * @param dev Input touch device to report for.
 */
static void udev_report_touch(udev_input_t *udev, udev_input_device_t *dev)
{
   /* Touch state being modified. */
   udev_input_touch_t *touch = &dev->touch;

   /* Helper variables. */
   int iii;
   int32_t ts_diff;
   uint32_t tp_diff;
   int32_t last_mouse_pos_x;
   int32_t last_mouse_pos_y;
   enum udev_input_touch_dir tp_diff_dir;
   video_viewport_t vp;
   udev_touch_ts_t now;
   udev_slot_state_t *slot_curr;
   udev_slot_state_t *slot_prev;

   /* Get main panel information for coordinate translation */
   video_driver_get_viewport_info(&vp);
   /* Get current time for measurements */
   udev_touch_ts_now(&now);

   /*udev_dump_touch_dev(dev); */

   for (iii = 0; iii < touch->info_slots.range; ++iii)
   {
      slot_curr = &touch->staging[iii];
      slot_prev = &touch->current[iii];

      /* TODO - Unused for now */
      UDEV_INPUT_TOUCH_UNUSED(slot_prev);

      switch (slot_curr->change)
      {
         case UDEV_TOUCH_CHANGE_NONE:
            /* No operation */
            break;
         case UDEV_TOUCH_CHANGE_DOWN:
            /* Tracking started -> touchdown */
            touch->staging_active++;

            /* First touchpoint is our primary gesture point */
            if (touch->staging_active == 1)
            { touch->gest_primary_slot = iii; }
            /* Second touchpoint is our secondary gesture point */
            if (touch->staging_active == 2)
            { touch->gest_secondary_slot = iii; }
            RARCH_DDBG("[udev] Active: %d\n", touch->staging_active);

            /* Touchscreen update */
            if (iii == touch->gest_primary_slot)
            { /* Use position of the primary touch point */
               touch->pointer_pos_x = slot_curr->pos_x;
               touch->pointer_pos_y = slot_curr->pos_y;
               udev_translate_touch_pos(
                  touch, &vp, 
                  touch->pointer_pos_x, touch->pointer_pos_y, 
                  &touch->pointer_ma_pos_x, &touch->pointer_ma_pos_y, 
                  &touch->pointer_ma_rel_x, &touch->pointer_ma_rel_y, 
                  &touch->pointer_scr_pos_x, &touch->pointer_scr_pos_y, 
                  &touch->pointer_vp_pos_x, &touch->pointer_vp_pos_y
               );
 
               /* Reset deltas after, since first touchdown has no delta. */
               touch->pointer_ma_rel_x = 0;
               touch->pointer_ma_rel_y = 0;
            }

            /* Pointer section */
            if (touch->pointer_enabled)
            { /* Simulated pointer */
               touch->pointer_btn_pp = true;
            }

            /* Mouse section */
            if (touch->mouse_enabled)
            { /* Simulated mouse */
               if (touch->touchpad_enabled)
               { /* Touchpad mode -> Touchpad virtual mouse. */
                  /* Initialize touchpad position to the current mouse position */
                  touch->touchpad_pos_x = (float) touch->mouse_pos_x;
                  touch->touchpad_pos_y = (float) touch->mouse_pos_y;
               }
               else
               { /* Direct mode -> Direct virtual mouse. */
               }

               if (touch->trackball_enabled)
               { /* Trackball mode */
                  /* The trackball is anchored */
                  touch->trackball_inertial = false;
               }
            }

            /* Gesture section */
            if (touch->gest_enabled)
            {
               /* Pause the tap-gesture detection timeout. */
               udev_input_touch_gest_disable_cb(
                  &touch->gest_tcbs[UDEV_TOUCH_TGEST_TIMEOUT]
               );
               /* Expect the possibility of a tap. */
               touch->gest_tap_possible = true;

               /* Freeze the mouse cursor upon getting second contact */
               if (touch->staging_active > 1)
               { touch->mouse_freeze_cursor = true; }
            }

            break;
         case UDEV_TOUCH_CHANGE_UP:
            /* Ending tracking, touch up */
            if (touch->staging_active > 0)
            { touch->staging_active--; }
            else
            { RARCH_ERR("[udev] Cannot report touch up since there are no active points!\n"); }

            /* Letting go of the primary gesture point -> Wait for full relese */
            if (iii == touch->gest_primary_slot)
            { touch->gest_primary_slot = UDEV_INPUT_TOUCH_TRACKING_ID_NONE; }
            /* Letting go of the secondary gesture point -> Wait for full relese */
            if (iii == touch->gest_secondary_slot)
            { touch->gest_secondary_slot = UDEV_INPUT_TOUCH_TRACKING_ID_NONE; }

            /* Touchscreen update */
            if (iii == touch->gest_primary_slot)
            {
            }

            /* Pointer section */
            if (touch->pointer_enabled)
            { /* Simulated pointer */
               /* Release the pointer only after all contacts are released */
               touch->pointer_btn_pp = (touch->staging_active == 0);
            }

            /* Mouse section */
            if (touch->mouse_enabled)
            { /* Simulated mouse */
               if (touch->touchpad_enabled)
               { /* Touchpad mode -> Touchpad virtual mouse. */
                  /* Mouse buttons are governed by gestures. */
               }
               else
               { /* Direct mode -> Direct virtual mouse. */
                  /* Mouse buttons are governed by gestures. */
               }

               if (touch->trackball_enabled)
               { /* Trackball mode */
                  /* Update trackball position */
                  touch->trackball_pos_x = (float) touch->mouse_pos_x;
                  touch->trackball_pos_y = (float) touch->mouse_pos_y;
                  /* The trackball is free to move */
                  touch->trackball_inertial = true;
               }
            }

            /* Gesture section */
            if (touch->gest_enabled)
            {
               if (touch->gest_tap_possible)
               {
                  /* Time of contact */
                  ts_diff = udev_touch_ts_diff(&slot_curr->td_time, &now);
                  /* Distance and direction of start -> end touch point */
                  tp_diff = udev_touch_tp_diff(
                     slot_curr->td_pos_x, slot_curr->td_pos_y, 
                     slot_curr->pos_x, slot_curr->pos_y, 
                     &tp_diff_dir
                  );
                  /* Tap is possible only if neither time nor distance is over limit. */
                  touch->gest_tap_possible = 
                      ts_diff < touch->gest_tap_time && 
                      tp_diff < touch->gest_tap_dist;
               }
   
               if (touch->gest_tap_possible)
               { /* Tap detected */
                  /* Only single tap should be possible. */
                  touch->gest_tap_possible = false;
                  /* Add additional tap. */
                  touch->gest_tap_count++;
                  /* Setup gesture timeout after which gesture is detected. */
                  udev_input_touch_gest_set_cb(
                     &touch->gest_tcbs[UDEV_TOUCH_TGEST_TIMEOUT], 
                     &now, touch->gest_timeout, 
                     /* TODO - Use the additional data? */
                     udev_input_touch_tgest_select, NULL);
               }

               /* Reset the move-based gestures. */
               udev_input_touch_mgest_reset(touch, iii, &now);

               /* Unfreeze the mouse cursor when releasing all contacts */
               if (touch->staging_active == 0)
               { touch->mouse_freeze_cursor = false; }
            }

            break;
         case UDEV_TOUCH_CHANGE_MOVE:
            /* Change of position, size, or pressure */

            /* Touchscreen update */
            if (iii == touch->gest_primary_slot)
            { /* Use position of the primary touch point */
               touch->pointer_pos_x = slot_curr->pos_x;
               touch->pointer_pos_y = slot_curr->pos_y;
 
               /* Reset deltas first, so we can get new change. */
               touch->pointer_ma_rel_x = 0;
               touch->pointer_ma_rel_y = 0;
 
               udev_translate_touch_pos(
                  touch, &vp, 
                  touch->pointer_pos_x, touch->pointer_pos_y, 
                  &touch->pointer_ma_pos_x, &touch->pointer_ma_pos_y, 
                  &touch->pointer_ma_rel_x, &touch->pointer_ma_rel_y, 
                  &touch->pointer_scr_pos_x, &touch->pointer_scr_pos_y, 
                  &touch->pointer_vp_pos_x, &touch->pointer_vp_pos_y
               );
            }

            /* Pointer section */
            if (touch->pointer_enabled)
            { /* Simulated pointer */
            }

            /* Mouse section */
            if (touch->mouse_enabled && !touch->mouse_freeze_cursor)
            { /* Simulated mouse */
               if (touch->touchpad_enabled)
               { /* Touchpad mode -> Touchpad virtual mouse. */
                  
                  /* Calculate high resolution positions and clip them. */
                  touch->touchpad_pos_x += touch->pointer_ma_rel_x * touch->touchpad_sensitivity;
                  if (touch->touchpad_pos_x < 0.0f)
                  { touch->touchpad_pos_x = 0.0f; }
                  else if (touch->touchpad_pos_x > vp.full_width)
                  { touch->touchpad_pos_x = vp.full_width; }
                  touch->touchpad_pos_y += touch->pointer_ma_rel_y * touch->touchpad_sensitivity;
                  if (touch->touchpad_pos_y < 0.0f)
                  { touch->touchpad_pos_y = 0.0f; }
                  else if (touch->touchpad_pos_y > vp.full_height)
                  { touch->touchpad_pos_y = vp.full_height; }

                  /* Backup last values for delta. */
                  last_mouse_pos_x = touch->mouse_pos_x;
                  last_mouse_pos_y = touch->mouse_pos_y;

                  /* Convert high resolution (sub-pixels) -> low resolution (pixels) */
                  touch->mouse_pos_x = (int32_t) touch->touchpad_pos_x;
                  touch->mouse_pos_y = (int32_t) touch->touchpad_pos_y;

                  /* Translate the panel coordinates into normalized coordinates. */
                  video_driver_translate_coord_viewport(
                     &vp, touch->mouse_pos_x, touch->mouse_pos_y, 
                     &touch->mouse_vp_pos_x, &touch->mouse_vp_pos_y, 
                     &touch->mouse_scr_pos_x, &touch->mouse_scr_pos_y
                  );

                  /* Calculate cursor delta in screen space. */
                  touch->mouse_rel_x += touch->mouse_pos_x - last_mouse_pos_x;
                  touch->mouse_rel_y += touch->mouse_pos_y - last_mouse_pos_y;
               }
               else
               { /* Direct mode -> Direct virtual mouse. */
                  /* Set mouse cursor position directly from the pointer. */
                  last_mouse_pos_x = touch->mouse_pos_x;
                  last_mouse_pos_y = touch->mouse_pos_y;
                  touch->mouse_rel_x += touch->pointer_ma_pos_x - touch->mouse_pos_x;
                  touch->mouse_rel_y += touch->pointer_ma_pos_y - touch->mouse_pos_y;
                  touch->mouse_pos_x = touch->pointer_ma_pos_x;
                  touch->mouse_pos_y = touch->pointer_ma_pos_y;
                  touch->mouse_scr_pos_x = touch->pointer_scr_pos_x;
                  touch->mouse_scr_pos_y = touch->pointer_scr_pos_y;
                  touch->mouse_vp_pos_x = touch->pointer_vp_pos_x;
                  touch->mouse_vp_pos_y = touch->pointer_vp_pos_y;
               }

               if (touch->trackball_enabled)
               { /* Trackball mode */
                  /* Update trackball position */
                  touch->trackball_pos_x = (float) touch->mouse_pos_x;
                  touch->trackball_pos_y = (float) touch->mouse_pos_y;
                  /* Accumulate trackball velocity */
                  touch->trackball_vel_x = \
                     touch->trackball_frict_x * touch->trackball_vel_x + \
                     touch->trackball_sensitivity_x * \
                     (touch->mouse_pos_x - last_mouse_pos_x);
                  touch->trackball_vel_y = \
                     touch->trackball_frict_y * touch->trackball_vel_y + \
                     touch->trackball_sensitivity_y * \
                     (touch->mouse_pos_y - last_mouse_pos_y);
               }
            }

            /* Gesture section */
            if (touch->gest_enabled)
            { /* Move-based gestures - swiping, pinching, etc. */
               if (touch->gest_tap_possible)
               {
                  /* Time of contact */
                  ts_diff = udev_touch_ts_diff(&slot_curr->td_time, &now);
                  /* Distance and direction of start -> end touch point */
                  tp_diff = udev_touch_tp_diff(
                     slot_curr->td_pos_x, slot_curr->td_pos_y, 
                     slot_curr->pos_x, slot_curr->pos_y, 
                     &tp_diff_dir
                  );
                  /* Tap is possible only if neither time nor distance is over limit. */
                  touch->gest_tap_possible = 
                      ts_diff < touch->gest_tap_time && 
                      tp_diff < touch->gest_tap_dist;
               }
               if (!touch->gest_tap_possible)
               { /* Once tap is impossible, we can detect move-based gestures. */
                  /* 
                   * At this moment, tap gestures are no longer possible, 
                   * since gest_tap_possible == false
                   */
                  udev_input_touch_mgest_select(touch, iii, &now);
               }
            }

            break;
         default:
            RARCH_ERR("[udev] Unknown slot change %d!\n", touch->staging[iii].change);
            break;
      }

   }

   /* Copy staging to current slots and prepare for next round */
   memcpy(touch->current, touch->staging, sizeof(udev_slot_state_t) * touch->info_slots.range);
   touch->current_active = touch->staging_active;
   for (iii = 0; iii < touch->info_slots.range; ++iii)
   { /* Reset the change flag to prepare for next round */
      touch->staging[iii].change = UDEV_TOUCH_CHANGE_NONE;
   }
}

/**
 * Function handling incoming udev events pertaining to a touch device.
 *
 * @param data Data passed by the callback -> udev_input_t*
 * @param event Incoming event.
 * @param dev The source device.
 */
static void udev_handle_touch(void *data,
      const struct input_event *event, udev_input_device_t *dev)
{
   udev_input_t *udev = (udev_input_t*)data;
   udev_input_mouse_t *mouse = &dev->mouse;
   udev_input_touch_t *touch = &dev->touch;

   /* struct input_event */
   /* { */
   /*   (pseudo) input_event_sec; */
   /*   (pseudo) input_event_usec; */
   /*   __u16 type; */
   /*   __u16 code; */
   /*   __s32 value; */
   /* } */
   /* Timestamp for the event in event->time */
   /* type: code -> value */
   /* EV_ABS: ABS_MT_TRACKING_ID -> Unique ID for each touch */
   /* EV_ABS: ABS_MT_POSITION_X/Y -> Absolute position of the multitouch */
   /* EV_ABS: ABS_MT_TOUCH_MAJOR -> Major axis (size) of the touch */
   /* EV_KEY: BTN_TOUCH -> Signal for any touch - 1 down and 0 up */
   /* EV_ABS: ABS_X/Y -> Absolute position of the touch */
   /* SYN_REPORT -> End of packet */
   
   switch (event->type)
   {
      case EV_ABS: 
         switch (event->code)
         {
            case ABS_MT_SLOT:
               if (event->value >= 0 && event->value < touch->info_slots.range)
               { /* Move to a specific slot */
                  touch->current_slot = event->value; 
               }
               else
               { RARCH_WARN("[udev] handle_touch: Invalid touch slot id [%d]\n", event->value); }
               break;
            case ABS_MT_TRACKING_ID:
               touch->staging[touch->current_slot].tracking_id = event->value;

               if (event->value >= 0)
               { /* Starting a new tracking */
                  RARCH_DDBG("[udev] handle_touch: Tracking slot [%d]\n", touch->current_slot);
                  touch->staging[touch->current_slot].change = UDEV_TOUCH_CHANGE_DOWN;
                  udev_touch_event_ts_copy(event, &touch->staging[touch->current_slot].td_time);
               }
               else
               { /* End tracking */
                  RARCH_DDBG("[udev] handle_touch: End tracking slot [%d]\n", touch->current_slot);
                  touch->staging[touch->current_slot].change = UDEV_TOUCH_CHANGE_UP;
                  /* TODO - Just to be sure, event->value should be negative? */
                  touch->staging[touch->current_slot].tracking_id = UDEV_INPUT_TOUCH_TRACKING_ID_NONE;
               }
               break;
            case ABS_X:
               /* TODO - Currently using single-touch events as touch with id 0 */
               RARCH_DDBG("[udev] handle_touch: [0] ST_X to %d\n", event->value);
               touch->staging[0].pos_x = event->value;
               if (touch->staging[0].change == UDEV_TOUCH_CHANGE_NONE) 
               { /* Low priority event, mark the change. */
                  touch->staging[0].change = UDEV_TOUCH_CHANGE_MOVE;
               }
               else if (touch->staging[0].change == UDEV_TOUCH_CHANGE_DOWN) 
               { /* Starting tracing, remember touchdown position. */
                  touch->staging[0].td_pos_x = event->value;
               }
               break;
            case ABS_MT_POSITION_X:
               RARCH_DDBG("[udev] handle_touch: [%d] MT_X to %d\n", touch->current_slot, event->value);
               touch->staging[touch->current_slot].pos_x = event->value;
               if (touch->staging[touch->current_slot].change == UDEV_TOUCH_CHANGE_NONE) 
               { /* Low priority event, mark the change. */
                  touch->staging[touch->current_slot].change = UDEV_TOUCH_CHANGE_MOVE;
               }
               else if (touch->staging[touch->current_slot].change == UDEV_TOUCH_CHANGE_DOWN) 
               { /* Starting tracing, remember touchdown position. */
                  touch->staging[touch->current_slot].td_pos_x = event->value;
               }
               break;
            case ABS_Y:
               /* TODO - Currently using single-touch events as touch with id 0 */
               RARCH_DDBG("[udev] handle_touch: [0] ST_Y to %d\n", event->value);
               touch->staging[0].pos_y = event->value;
               if (touch->staging[0].change == UDEV_TOUCH_CHANGE_NONE) 
               { /* Low priority event, mark the change. */
                  touch->staging[0].change = UDEV_TOUCH_CHANGE_MOVE;
               }
               else if (touch->staging[0].change == UDEV_TOUCH_CHANGE_DOWN) 
               { /* Starting tracing, remember touchdown position. */
                  touch->staging[0].td_pos_y = event->value;
               }
               break;
            case ABS_MT_POSITION_Y:
               RARCH_DDBG("[udev] handle_touch: [%d] MT_Y to %d\n", touch->current_slot, event->value);
               touch->staging[touch->current_slot].pos_y = event->value;
               if (touch->staging[touch->current_slot].change == UDEV_TOUCH_CHANGE_NONE) 
               { /* Low priority event, mark the change. */
                  touch->staging[touch->current_slot].change = UDEV_TOUCH_CHANGE_MOVE;
               }
               else if (touch->staging[touch->current_slot].change == UDEV_TOUCH_CHANGE_DOWN) 
               { /* Starting tracing, remember touchdown position. */
                  touch->staging[touch->current_slot].td_pos_y = event->value;
               }
               break;
            case ABS_MT_TOUCH_MINOR:
               RARCH_DDBG("[udev] handle_touch: [%d] MINOR to %d\n", touch->current_slot, event->value);
               touch->staging[touch->current_slot].minor = event->value;
               if (touch->staging[touch->current_slot].change == UDEV_TOUCH_CHANGE_NONE) 
               { /* Low priority event, mark the change. */
                  touch->staging[touch->current_slot].change = UDEV_TOUCH_CHANGE_MOVE;
               }
               break;
            case ABS_MT_TOUCH_MAJOR:
               RARCH_DDBG("[udev] handle_touch: [%d] MAJOR to %d\n", touch->current_slot, event->value);
               touch->staging[touch->current_slot].major = event->value;
               if (touch->staging[touch->current_slot].change == UDEV_TOUCH_CHANGE_NONE) 
               { /* Low priority event, mark the change. */
                  touch->staging[touch->current_slot].change = UDEV_TOUCH_CHANGE_MOVE;
               }
               break;
            case ABS_MT_PRESSURE:
               RARCH_DDBG("[udev] handle_touch: [%d] PRES to %d\n", touch->current_slot, event->value);
               touch->staging[touch->current_slot].pressure = event->value;
               if (touch->staging[touch->current_slot].change == UDEV_TOUCH_CHANGE_NONE) 
               { /* Low priority event, mark the change. */
                  touch->staging[touch->current_slot].change = UDEV_TOUCH_CHANGE_MOVE;
               }
               break;
            default:
               RARCH_WARN("[udev] handle_touch: EV_ABS (code %d) is not handled!\n", event->code);
               break;
         }
         break;
      case EV_KEY:
         if (event->code == BTN_TOUCH)
         {
            RARCH_DDBG("[udev] handle_touch: [%d] TOUCH %d\n", touch->current_slot, event->value);
            if (event->value > 0)
            { touch->staging[touch->current_slot].change = UDEV_TOUCH_CHANGE_DOWN; }
            else
            { touch->staging[touch->current_slot].change = UDEV_TOUCH_CHANGE_UP; }
            break;
         }
         else
         { RARCH_WARN("[udev] handle_touch: EV_KEY (code %d) is not handled!\n", event->code); }
         break;
      case EV_REL:
         RARCH_WARN("[udev] handle_touch: EV_REL (code %d) is not handled!\n", event->code);
         break;
      case EV_SYN:
         switch (event->code)
         {
            case SYN_DROPPED:
               RARCH_DDBG("[udev] handle_touch: SYN_DROP -> !sync!\n");
               udev_sync_touch(dev);
               break;
            case SYN_MT_REPORT:
               /* TODO - Unused, add to support type-A devices [multi-touch-protocol.txt]*/
               break;
            case SYN_REPORT:
               RARCH_DDBG("[udev] handle_touch: SYN_REPORT\n");
               udev_report_touch(udev, dev);
               break;
            default:
               RARCH_WARN("[udev] handle_touch: EV_SYN (code %d) is not handled!\n", event->code);
               break;
         }
         break;
      default:
         RARCH_WARN("[udev] handle_touch: Event type %d is not handled!\n", event->type);
         break;
   }
}

/**
 * Periodic update of trackball state.
 *
 * @param touch Target touch state.
 * @param now Current time.
 */
static void udev_input_touch_state_trackball(
        udev_input_touch_t *touch, 
        const udev_touch_ts_t *now)
{
   video_viewport_t vp;
   float delta_x;
   float delta_y;
   float delta_t;

   /* Let's perform these only once per state polling loop */
   if (!touch->run_state_update)
   { return; }

   /* Update trackball mouse position */
   if (touch->trackball_enabled)
   { /* Trackball is moving */
      if (touch->trackball_inertial)
      {
         /* Calculate time delta */
         delta_t = (float) udev_touch_ts_diff(&touch->last_state_update, now) / \
            UDEV_INPUT_TOUCH_S_TO_US;
         /* Calculate position delta */
         delta_x = touch->trackball_vel_x * delta_t;
         delta_y = touch->trackball_vel_y * delta_t;
         /* Update the high-resolution mouse position */
         touch->trackball_pos_x += delta_x;
         touch->trackball_pos_y += delta_y;
         /* Update the real mouse position */
         touch->mouse_pos_x = (int16_t) touch->trackball_pos_x;
         touch->mouse_pos_y = (int16_t) touch->trackball_pos_y;

         /* Get current viewport information */
         video_driver_get_viewport_info(&vp);
         /* Translate the raw coordinates into normalized coordinates. */
         video_driver_translate_coord_viewport(
            &vp, touch->mouse_pos_x, touch->mouse_pos_y, 
            &touch->mouse_vp_pos_x, &touch->mouse_vp_pos_y, 
            &touch->mouse_scr_pos_x, &touch->mouse_scr_pos_y
         );

         /* Add the movement to mouse delta */
         touch->mouse_rel_x += delta_x;
         touch->mouse_rel_y += delta_y;
      }

      /* Attenuate the velocity */
      touch->trackball_vel_x *= touch->trackball_frict_x;
      touch->trackball_vel_y *= touch->trackball_frict_y;
      /* Automatically cut off the inertia when the velocity is low enough */
      if ((touch->trackball_vel_x * touch->trackball_vel_x) + 
          (touch->trackball_vel_y * touch->trackball_vel_y) <= 
          touch->trackball_sq_vel_cutoff)
      {
          touch->trackball_vel_x = 0.0f;
          touch->trackball_vel_y = 0.0f;
          touch->trackball_inertial = false;
      }
   }
}

/**
 * Gesture time-dependant processing.
 * TODO - Current implementation may result in cancelling a button 
 * state before it gets reported at least once. However, this is 
 * a better approach compared to waiting, since _state call may 
 * never occur.
 *
 * @param touch Target touch state.
 * @param now Current time.
 */
static void udev_input_touch_state_gest(
        udev_input_touch_t *touch, 
        const udev_touch_ts_t *now)
{
   int iii;

   /* Tap-based gesture processing */
   for (iii = 0; iii < UDEV_TOUCH_TGEST_LAST; ++iii)
   { /* Process time-delayed callbacks. */
      if (touch->gest_tcbs[iii].active)
      { /* Callback is active. */
         if (udev_touch_ts_diff(now, &touch->gest_tcbs[iii].execute_at) <= 0)
         { /* Execution time passed. */
            udev_input_touch_gest_exec_cb(&touch->gest_tcbs[iii], touch);
         }
      }
   }

   /* Move-based gesture processing */
   for (iii = 0; iii < UDEV_TOUCH_MGEST_LAST; ++iii)
   { /* Process time-delayed callbacks. */
      if (touch->gest_mcbs[iii].active)
      { /* Callback is active. */
         if (udev_touch_ts_diff(now, &touch->gest_mcbs[iii].execute_at) <= 0)
         { /* Execution time passed. */
            udev_input_touch_gest_exec_cb(&touch->gest_mcbs[iii], touch);
         }
      }
   }

   /* Let's perform these only once per state polling loop */
   if (!touch->run_state_update)
   { return; }

   /* Tap-based gesture processing */
   /* TODO - Currently none */

   /* Move-based gesture processing */
   switch (touch->gest_mgest_type)
   {
      case UDEV_TOUCH_MGEST_S_STAP_DRAG: 
         /* Gesture for one finger single tap drag. */
         break;
      case UDEV_TOUCH_MGEST_S_DTAP_DRAG: 
         /* Gesture for one finger double tap drag. */
         break;
      case UDEV_TOUCH_MGEST_S_TTAP_DRAG: 
         /* Gesture for one finger triple tap drag. */
         break;
      case UDEV_TOUCH_MGEST_D_NTAP_DRAG: 
         /* Gesture for two finger no tap drag. */
         /* Convert accumulated scrolls to mouse_wheel_x/y */
         if (touch->gest_scroll_x > touch->gest_scroll_step || 
             touch->gest_scroll_x < -touch->gest_scroll_step)
         { /* Add one scroll step. TODO - Add multiple? */
            /* Add if oriented the same or simply set to one. */
            if (touch->gest_scroll_x * touch->mouse_wheel_x > 0)
            { touch->mouse_wheel_x += 1 * udev_touch_sign(touch->gest_scroll_x); }
            else
            { touch->mouse_wheel_x = 1 * udev_touch_sign(touch->gest_scroll_x); }
            /* Reset the scroll for the next delta */
            touch->gest_scroll_x -= touch->gest_scroll_step * 
               udev_touch_sign(touch->gest_scroll_x);
         }
         if (touch->gest_scroll_y > touch->gest_scroll_step || 
             touch->gest_scroll_y < -touch->gest_scroll_step)
         { /* Add one scroll step. TODO - Add multiple? */
            /* TODO - Note the -sign, the vertical scroll seems inverted. */
            /* Add if oriented the same or simply set to one. */
            if (touch->gest_scroll_y * touch->mouse_wheel_x > 0)
            { touch->mouse_wheel_y += 1 * -udev_touch_sign(touch->gest_scroll_y); }
            else
            { touch->mouse_wheel_y = 1 * -udev_touch_sign(touch->gest_scroll_y); }
            /* Reset the scroll for the next delta */
            touch->gest_scroll_y -= touch->gest_scroll_step * 
               udev_touch_sign(touch->gest_scroll_y);
         }
         break;
      case UDEV_TOUCH_MGEST_D_STAP_DRAG: 
         /* Gesture for two finger single tap drag. */
         break;
      case UDEV_TOUCH_MGEST_T_NTAP_DRAG: 
         /* Gesture for three finger no tap drag. */
         break;
      case UDEV_TOUCH_MGEST_T_STAP_DRAG: 
         /* Gesture for three finger single tap drag. */
         break;
      default:
         /* No action */
         break;
   }
}

/**
 * State function handling touch devices.
 *
 * @param udev Source UDev system.
 * @param dev The touch device being polled.
 * @param binds Bindings structure.
 * @param keyboard_mapping_blocked Block keyboard mapped inputs.
 * @param port Port (player) of the device being polled.
 * @param device Type of device RETRO_DEVICE_* being polled.
 * @param idx Index of the device being polled.
 * @param id Identifier of the axis / button being polled - 
 *   e.g. RETRO_DEVICE_ID_*.
 */
static int16_t udev_input_touch_state(
      udev_input_t *udev,
      udev_input_device_t *dev, 
      const retro_keybind_set *binds,
      bool keyboard_mapping_blocked,
      unsigned port,
      unsigned device,
      unsigned idx,
      unsigned id)
{
   int16_t ret = 0;
   bool screen = false;
   udev_input_touch_t *touch = &dev->touch;
   udev_touch_ts_t now;

   /* Get current time for measurements */
   udev_touch_ts_now(&now);

   /* TODO - Process timed gestures before or after getting state? */
   /* Process timed gestures. */
   udev_input_touch_state_gest(touch, &now);
   /* Process trackball. */
   udev_input_touch_state_trackball(touch, &now);

   if (touch->run_state_update)
   { /* Perform state update only once */
       touch->run_state_update = false;
       /* Update last update timestamp */
       udev_touch_ts_copy(&now, &touch->last_state_update);
   }

   switch (device)
   {
      case RETRO_DEVICE_MOUSE:
      case RARCH_DEVICE_MOUSE_SCREEN:
         screen = (device == RARCH_DEVICE_MOUSE_SCREEN);
         switch (id)
         {
            case RETRO_DEVICE_ID_MOUSE_X:
               if (screen)
               { ret = touch->mouse_pos_x; }
               else
               { ret = touch->mouse_rel_x; touch->mouse_rel_x = 0; }
               break;
            case RETRO_DEVICE_ID_MOUSE_Y:
               if (screen)
               { ret = touch->mouse_pos_y; }
               else
               { ret = touch->mouse_rel_y; touch->mouse_rel_y = 0; }
               break;
            case RETRO_DEVICE_ID_MOUSE_LEFT:
               ret = touch->mouse_btn_l;
               break;
            case RETRO_DEVICE_ID_MOUSE_RIGHT:
               ret = touch->mouse_btn_r;
               break;
            case RETRO_DEVICE_ID_MOUSE_MIDDLE:
               ret = touch->mouse_btn_m;
               break;
            case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
               ret = touch->mouse_btn_b4;
               break;
            case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
               ret = touch->mouse_btn_b5;
               break;
            case RETRO_DEVICE_ID_MOUSE_WHEELUP:
               ret = touch->mouse_wheel_y > 0;
               if (ret)
               { touch->mouse_wheel_y--; }
               break;
            case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
               ret = touch->mouse_wheel_y < 0;
               if (ret)
               { touch->mouse_wheel_y++; }
               break;
            case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
               ret = touch->mouse_wheel_x > 0;
               if (ret)
               { touch->mouse_wheel_x--; }
               break;
            case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
               ret = touch->mouse_wheel_x < 0;
               if (ret)
               { touch->mouse_wheel_x++; }
               break;
            default:
               break;
         }
         break;

      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
         screen = (device == RARCH_DEVICE_MOUSE_SCREEN);
         break;
         switch (id)
         {
            case RETRO_DEVICE_ID_POINTER_X:
               if (screen)
               { ret = touch->pointer_scr_pos_x; }
               else
               { ret = touch->pointer_vp_pos_x; }
               break;
            case RETRO_DEVICE_ID_POINTER_Y:
               if (screen)
               { ret = touch->pointer_scr_pos_y; }
               else
               { ret = touch->pointer_vp_pos_y; }
               break;
            case RETRO_DEVICE_ID_POINTER_PRESSED:
               ret = touch->pointer_btn_pp;
               break;
            case RARCH_DEVICE_ID_POINTER_BACK:
               /* TODO - Remove this function? */
               ret = touch->pointer_btn_pb;
               break;
            default:
               break;
         }
         break;

      case RETRO_DEVICE_LIGHTGUN:
         switch (id)
         {
            /* TODO - Add simulated lightgun? */
            case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
            case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
            case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
            case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
            case RETRO_DEVICE_ID_LIGHTGUN_RELOAD:
            case RETRO_DEVICE_ID_LIGHTGUN_AUX_A:
            case RETRO_DEVICE_ID_LIGHTGUN_AUX_B:
            case RETRO_DEVICE_ID_LIGHTGUN_AUX_C:
            case RETRO_DEVICE_ID_LIGHTGUN_START:
            case RETRO_DEVICE_ID_LIGHTGUN_SELECT:
            case RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP:
            case RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN:
            case RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT:
            case RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT:
            case RETRO_DEVICE_ID_LIGHTGUN_PAUSE:
            case RETRO_DEVICE_ID_LIGHTGUN_X:
            case RETRO_DEVICE_ID_LIGHTGUN_Y:
               break;
         }
         break;
   }

   return ret;
}

#endif 
/* UDEV_TOUCH_SUPPORT */

#define test_bit(array, bit)    (array[bit/8] & (1<<(bit%8)))

static int udev_input_add_device(udev_input_t *udev,
      enum udev_input_dev_type type, const char *devnode, device_handle_cb cb)
{
   unsigned char keycaps[(KEY_MAX / 8) + 1] = {'\0'};
   unsigned char abscaps[(ABS_MAX / 8) + 1] = {'\0'};
   unsigned char relcaps[(REL_MAX / 8) + 1] = {'\0'};
   udev_input_device_t **tmp                = NULL;
   udev_input_device_t *device              = NULL;
   struct input_absinfo absinfo;
   int fd                                   = -1;
   int ret                                  = 0;
   struct stat st;
#if defined(HAVE_EPOLL)
   struct epoll_event event;
#elif defined(HAVE_KQUEUE)
   struct kevent event;
#endif

   st.st_dev = 0;

   if (stat(devnode, &st) < 0)
      goto end;

   fd = open(devnode, O_RDONLY | O_NONBLOCK);
   if (fd < 0)
      goto end;

   device = (udev_input_device_t*)calloc(1, sizeof(*device));
   if (!device)
      goto end;

   device->fd        = fd;
   device->dev       = st.st_dev;
   device->handle_cb = cb;
   device->type      = type;

   strlcpy(device->devnode, devnode, sizeof(device->devnode));

   if (ioctl(fd, EVIOCGNAME(sizeof(device->ident)), device->ident) < 0)
      device->ident[0] = '\0';

   /* UDEV_INPUT_MOUSE may report in absolute coords too */
   if (type == UDEV_INPUT_MOUSE || type == UDEV_INPUT_TOUCHPAD || type == UDEV_INPUT_TOUCHSCREEN )
   {
      bool mouse = 0;
      bool touch = 0;
      /* gotta have some buttons!  return -1 to skip error logging for this:)  */
      if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof (keycaps)), keycaps) == -1)
      {
         ret = -1;
         goto end;
      }

      if (ioctl(fd, EVIOCGBIT(EV_REL, sizeof (relcaps)), relcaps) != -1)
      {
         if ( (test_bit(relcaps, REL_X)) && (test_bit(relcaps, REL_Y)) )
         {
            mouse = 1;

            if (!test_bit(keycaps, BTN_MOUSE))
               RARCH_DBG("[udev]: Waring REL pointer device (%s) has no mouse button\n",device->ident);
         }
      }

      if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof (abscaps)), abscaps) != -1)
      {
         if ( (test_bit(abscaps, ABS_X)) && (test_bit(abscaps, ABS_Y)) )
         {
            mouse = 1;

            /* check for abs touch devices... */
            if (test_bit(keycaps, BTN_TOUCH))
            {
               touch = 1;
               device->mouse.abs = 1;
            }
            /* check for light gun or any other device that might not have a touch button */
            else
            {
               device->mouse.abs = 2;
            }

            if ( !test_bit(keycaps, BTN_TOUCH) && !test_bit(keycaps, BTN_MOUSE) )
               RARCH_DBG("[udev]: Warning ABS pointer device (%s) has no touch or mouse button\n",device->ident);
         }
      }

      device->mouse.x_min = 0;
      device->mouse.y_min = 0;
      device->mouse.x_max = 0;
      device->mouse.y_max = 0;

      if (device->mouse.abs)
      {

         if (ioctl(fd, EVIOCGABS(ABS_X), &absinfo) == -1)
         {
            RARCH_DBG("[udev]: ABS pointer device (%s) Failed to get ABS_X parameters \n",device->ident);
            goto end;
         }

         device->mouse.x_min = absinfo.minimum;
         device->mouse.x_max = absinfo.maximum;

         if (ioctl(fd, EVIOCGABS(ABS_Y), &absinfo) == -1)
         {
            RARCH_DBG("[udev]: ABS pointer device (%s) Failed to get ABS_Y parameters \n",device->ident);
            goto end;
         }
         device->mouse.y_min = absinfo.minimum;
         device->mouse.y_max = absinfo.maximum;
      }

#ifdef UDEV_TOUCH_SUPPORT
      if (touch)
      {
          udev_init_touch_dev(device);
          udev_sync_touch(device);
      }
#endif 

      if (!mouse)
         goto end;

   }

   tmp = (udev_input_device_t**)realloc(udev->devices,
         (udev->num_devices + 1) * sizeof(*udev->devices));

   if (!tmp)
      goto end;

   tmp[udev->num_devices++] = device;
   udev->devices            = tmp;

#if defined(HAVE_EPOLL)
   event.events             = EPOLLIN;
   event.data.ptr           = device;

   /* Shouldn't happen, but just check it. */
   if (epoll_ctl(udev->fd, EPOLL_CTL_ADD, fd, &event) < 0)
   {
      RARCH_ERR("udev]: Failed to add FD (%d) to epoll list (%s).\n",
            fd, strerror(errno));
   }
#elif defined(HAVE_KQUEUE)
   EV_SET(&event, fd, EVFILT_READ, EV_ADD, 0, 0, LISTENSOCKET);
   if (kevent(udev->fd, &event, 1, NULL, 0, NULL) == -1)
   {
      RARCH_ERR("udev]: Failed to add FD (%d) to kqueue list (%s).\n",
            fd, strerror(errno));
   }
#endif
   ret = 1;

end:
   /* Free resources in the event of
    * an error */
   if (ret != 1)
   {
      if (fd >= 0)
         close(fd);
      if (device)
         free(device);
   }

   return ret;
}

static void udev_input_remove_device(udev_input_t *udev, const char *devnode)
{
   unsigned i;

   for (i = 0; i < udev->num_devices; i++)
   {
      if (!string_is_equal(devnode, udev->devices[i]->devnode))
         continue;

      close(udev->devices[i]->fd);
      free(udev->devices[i]);
      memmove(udev->devices + i, udev->devices + i + 1,
            (udev->num_devices - (i + 1)) * sizeof(*udev->devices));
      udev->num_devices--;
   }
}

static void udev_input_handle_hotplug(udev_input_t *udev)
{
   device_handle_cb cb;
   enum udev_input_dev_type dev_type = UDEV_INPUT_KEYBOARD;
   const char *val_key               = NULL;
   const char *val_mouse             = NULL;
   const char *val_touchpad          = NULL;
   const char *val_touchscreen       = NULL;
   const char *action                = NULL;
   const char *devnode               = NULL;
   int mouse = 0;
   int keyboard = 0;
   int check = 0;
   int i = 0;
   struct udev_device *dev           = udev_monitor_receive_device(
         udev->monitor);

   if (!dev)
      return;

   val_key         = udev_device_get_property_value(dev, "ID_INPUT_KEY");
   val_mouse       = udev_device_get_property_value(dev, "ID_INPUT_MOUSE");
   val_touchpad    = udev_device_get_property_value(dev, "ID_INPUT_TOUCHPAD");
   val_touchscreen = udev_device_get_property_value(dev, "ID_INPUT_TOUCHSCREEN");
   action          = udev_device_get_action(dev);
   devnode         = udev_device_get_devnode(dev);

   if (val_key && string_is_equal(val_key, "1") && devnode)
   {
      /* EV_KEY device, can be a keyboard or a remote control device.  */
      dev_type   = UDEV_INPUT_KEYBOARD;
      cb         = udev_handle_keyboard;
   }
   else if (val_mouse && string_is_equal(val_mouse, "1") && devnode)
   {
      dev_type   = UDEV_INPUT_MOUSE;
      cb         = udev_handle_mouse;
   }
   else if (val_touchpad && string_is_equal(val_touchpad, "1") && devnode)
   {
      dev_type   = UDEV_INPUT_TOUCHPAD;
      cb         = udev_handle_mouse;
   }
   else if (val_touchscreen && string_is_equal(val_touchscreen, "1") && devnode)
   {
      dev_type   = UDEV_INPUT_TOUCHSCREEN;
      cb         = udev_handle_touch;
   }
   else
      goto end;

   /* Hotplug add */
   if (string_is_equal(action, "add"))
      udev_input_add_device(udev, dev_type, devnode, cb);
   /* Hotplug remove */
   else if (string_is_equal(action, "remove"))
      udev_input_remove_device(udev, devnode);

   /* we need to re index the mouse and keyboard indirection 
    * structures when a device is hotplugged 
    */
   /* first clear all */
   for (i = 0; i < MAX_USERS; i++)
   {
      input_config_set_mouse_display_name(i, "N/A");
      udev->pointers[i] = -1;
      udev->keyboards[i] = -1;
   }
 
   /* Add what devices we have now */
   for (i = 0; i < udev->num_devices; ++i)
   {
      if (udev->devices[i]->type != UDEV_INPUT_KEYBOARD)
      { /* Pointers */
         input_config_set_mouse_display_name(mouse, udev->devices[i]->ident);
         udev->pointers[mouse] = i;
         mouse++;
      }
      else
      { /* Keyboard */
         udev->keyboards[keyboard] = i;
         keyboard++;
      }
   }

end:
   udev_device_unref(dev);
}

#ifdef HAVE_X11
static void udev_input_get_pointer_position(int *x, int *y)
{
   if (video_driver_display_type_get() == RARCH_DISPLAY_X11)
   {
      Window w;
      int p;
      unsigned m;
      Display *display = (Display*)video_driver_display_get();
      Window window    = (Window)video_driver_window_get();

      XQueryPointer(display, window, &w, &w, &p, &p, x, y, &m);
   }
}

static void udev_input_adopt_rel_pointer_position_from_mouse(
      int *x, int *y, udev_input_mouse_t *mouse)
{
   static int noX11DispX = 0;
   static int noX11DispY = 0;

   struct video_viewport view;
   bool r = video_driver_get_viewport_info(&view);
   int dx = udev_mouse_get_x(mouse);
   int dy = udev_mouse_get_y(mouse);
   if (r && (dx || dy) &&
         video_driver_display_type_get() != RARCH_DISPLAY_X11)
   {
      int minX = view.x;
      int maxX = view.x + view.width;
      int minY = view.y;
      int maxY = view.y + view.height;

      /* Not running in a window. */
      noX11DispX = noX11DispX + dx;
      if (noX11DispX < minX)
         noX11DispX = minX;
      if (noX11DispX > maxX)
         noX11DispX = maxX;
      noX11DispY = noX11DispY + dy;
      if (noX11DispY < minY)
         noX11DispY = minY;
      if (noX11DispY > maxY)
         noX11DispY = maxY;
      *x = noX11DispX;
      *y = noX11DispY;
   }
   mouse->x_rel = 0;
   mouse->y_rel = 0;
}
#endif

static bool udev_input_poll_hotplug_available(struct udev_monitor *dev)
{
   struct pollfd fds;

   fds.fd      = udev_monitor_get_fd(dev);
   fds.events  = POLLIN;
   fds.revents = 0;

   return (poll(&fds, 1, 0) == 1) && (fds.revents & POLLIN);
}

static void udev_input_poll(void *data)
{
   int i, ret;
#if defined(HAVE_EPOLL)
   struct epoll_event events[32];
#elif defined(HAVE_KQUEUE)
   struct kevent events[32];
#endif
   udev_input_mouse_t *mouse = NULL;
   udev_input_t *udev        = (udev_input_t*)data;

#ifdef HAVE_X11
   udev_input_get_pointer_position(&udev->pointer_x, &udev->pointer_y);
#endif

   for (i = 0; i < udev->num_devices; ++i)
   {
      if (udev->devices[i]->type == UDEV_INPUT_KEYBOARD)
         continue;

      mouse = &udev->devices[i]->mouse;
#ifdef HAVE_X11
      udev_input_adopt_rel_pointer_position_from_mouse(
            &udev->pointer_x, &udev->pointer_y, mouse);
#else
      mouse->x_rel = 0;
      mouse->y_rel = 0;
#endif
      mouse->wu    = false;
      mouse->wd    = false;
      mouse->whu   = false;
      mouse->whd   = false;
   
#ifdef UDEV_TOUCH_SUPPORT
      /* Schedule touch state update. */
      udev->devices[i]->touch.run_state_update = true;
#endif
   }

   while (udev->monitor && udev_input_poll_hotplug_available(udev->monitor))
      udev_input_handle_hotplug(udev);

#if defined(HAVE_EPOLL)
   ret = epoll_wait(udev->fd, events, ARRAY_SIZE(events), 0);
#elif defined(HAVE_KQUEUE)
   {
      struct timespec timeoutspec;
      timeoutspec.tv_sec  = timeout;
      timeoutspec.tv_nsec = 0;
      ret                 = kevent(udev->fd, NULL, 0, events,
            ARRAY_SIZE(events), &timeoutspec);
   }
#endif

   for (i = 0; i < ret; i++)
   {
      /* TODO/FIXME - add HAVE_EPOLL/HAVE_KQUEUE codepaths here */
      if (events[i].events & EPOLLIN)
      {
         int j, len;
         struct input_event input_events[32];
#if defined(HAVE_EPOLL)
         udev_input_device_t *device = (udev_input_device_t*)events[i].data.ptr;
#elif defined(HAVE_KQUEUE)
         udev_input_device_t *device = (udev_input_device_t*)events[i].udata;
#endif

         while ((len = read(device->fd,
                     input_events, sizeof(input_events))) > 0)
         {
            len /= sizeof(*input_events);
            for (j = 0; j < len; j++)
               device->handle_cb(udev, &input_events[j], device);
         }
      }
   }
}

static bool udev_pointer_is_off_window(const udev_input_t *udev)
{
#ifdef HAVE_X11
   struct video_viewport view;
   bool r = video_driver_get_viewport_info(&view);

   if (r)
      r = udev->pointer_x < 0 ||
          udev->pointer_x >= view.full_width ||
          udev->pointer_y < 0 ||
          udev->pointer_y >= view.full_height;
   return r;
#else
   return false;
#endif
}

static int16_t udev_lightgun_aiming_state(
      udev_input_t *udev, unsigned port, unsigned id )
{

   const int edge_detect       = 32700;
   udev_input_mouse_t *mouse   = udev_get_mouse(udev, port);

   if (mouse)
   {
      bool inside              = false;
      int16_t res_x            = udev_mouse_get_pointer_x(mouse, false);
      int16_t res_y            = udev_mouse_get_pointer_y(mouse, false);

      switch ( id )
      {
         case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
            return res_x;
         case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
            return res_y;
         case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
            inside =   (res_x >= -edge_detect)
               &&      (res_y >= -edge_detect)
               &&      (res_x <=  edge_detect)
               &&      (res_y <=  edge_detect);
            return !inside;
         default:
            break;
      }
   }

   return 0;
}

static int16_t udev_mouse_state(udev_input_t *udev,
      unsigned port, unsigned id, bool screen)
{
   udev_input_mouse_t *mouse = udev_get_mouse(udev, port);

   if (mouse)
   {
      if (id != RETRO_DEVICE_ID_MOUSE_X && id != RETRO_DEVICE_ID_MOUSE_Y &&
            udev_pointer_is_off_window(udev))
         return 0;

      switch (id)
      {
         case RETRO_DEVICE_ID_MOUSE_X:
            return screen ? udev->pointer_x : udev_mouse_get_x(mouse);
         case RETRO_DEVICE_ID_MOUSE_Y:
            return screen ? udev->pointer_y : udev_mouse_get_y(mouse);
         case RETRO_DEVICE_ID_MOUSE_LEFT:
            return mouse->l;
         case RETRO_DEVICE_ID_MOUSE_RIGHT:
            return mouse->r;
         case RETRO_DEVICE_ID_MOUSE_MIDDLE:
            return mouse->m;
         case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
            return mouse->b4;
         case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
            return mouse->b5;
         case RETRO_DEVICE_ID_MOUSE_WHEELUP:
            return mouse->wu;
         case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
            return mouse->wd;
         case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
            return mouse->whu;
         case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
            return mouse->whd;
      }
   }

   return 0;
}

static bool udev_keyboard_pressed(udev_input_t *udev, unsigned key)
{
   int bit = rarch_keysym_lut[key];
   return BIT_GET(udev->state, bit);
}

static bool udev_mouse_button_pressed(
      udev_input_t *udev, unsigned port, unsigned key)
{
   udev_input_mouse_t *mouse = udev_get_mouse(udev, port);

   if (!mouse)
      return false;
/* todo add multi touch button check pointer devices */
   switch ( key )
   {
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         return mouse->l;
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         return mouse->r;
      case RETRO_DEVICE_ID_MOUSE_MIDDLE:
         return mouse->m;
      case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
         return mouse->b4;
      case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
         return mouse->b5;
      case RETRO_DEVICE_ID_MOUSE_WHEELUP:
         return mouse->wu;
      case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
         return mouse->wd;
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
         return mouse->whu;
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
         return mouse->whd;
   }

   return false;
}

static int16_t udev_pointer_state(udev_input_t *udev,
      unsigned port, unsigned id, bool screen)
{
   udev_input_mouse_t *mouse = udev_get_mouse(udev, port);

   if (mouse)
   {
      switch (id)
      {
         case RETRO_DEVICE_ID_POINTER_X:
            return udev_mouse_get_pointer_x(mouse, screen);
         case RETRO_DEVICE_ID_POINTER_Y:
            return udev_mouse_get_pointer_y(mouse, screen);
         case RETRO_DEVICE_ID_POINTER_PRESSED:
            if (mouse->abs == 1)
               return mouse->pp;
            return mouse->l;
      }
   }
   return 0;
}

static unsigned udev_retro_id_to_rarch(unsigned id)
{
   switch (id)
   {
      case RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT:
         return RARCH_LIGHTGUN_DPAD_RIGHT;
      case RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT:
         return RARCH_LIGHTGUN_DPAD_LEFT;
      case RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP:
         return RARCH_LIGHTGUN_DPAD_UP;
      case RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN:
         return RARCH_LIGHTGUN_DPAD_DOWN;
      case RETRO_DEVICE_ID_LIGHTGUN_SELECT:
         return RARCH_LIGHTGUN_SELECT;
      case RETRO_DEVICE_ID_LIGHTGUN_PAUSE:
         return RARCH_LIGHTGUN_START;
      case RETRO_DEVICE_ID_LIGHTGUN_RELOAD:
         return RARCH_LIGHTGUN_RELOAD;
      case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
         return RARCH_LIGHTGUN_TRIGGER;
      case RETRO_DEVICE_ID_LIGHTGUN_AUX_A:
         return RARCH_LIGHTGUN_AUX_A;
      case RETRO_DEVICE_ID_LIGHTGUN_AUX_B:
         return RARCH_LIGHTGUN_AUX_B;
      case RETRO_DEVICE_ID_LIGHTGUN_AUX_C:
         return RARCH_LIGHTGUN_AUX_C;
      case RETRO_DEVICE_ID_LIGHTGUN_START:
         return RARCH_LIGHTGUN_START;
      default:
         break;
   }

   return 0;
}

static int16_t udev_input_state(
      void *data,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      rarch_joypad_info_t *joypad_info,
      const retro_keybind_set *binds,
      bool keyboard_mapping_blocked,
      unsigned port,
      unsigned device,
      unsigned idx,
      unsigned id)
{
   udev_input_t *udev         = (udev_input_t*)data;
#ifdef UDEV_TOUCH_SUPPORT
   udev_input_device_t *pointer_dev = udev_get_pointer_port_dev(udev, port);
#endif

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
         {
            unsigned i;
            int16_t ret = 0;

            for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
            {
               if (binds[port][i].valid)
               {
                  if (udev_mouse_button_pressed(udev, port, binds[port][i].mbutton))
                     ret |= (1 << i);
               }
            }
            if (!keyboard_mapping_blocked)
            {
               for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
               {
                  if (binds[port][i].valid)
                  {
                     if ((binds[port][i].key < RETROK_LAST) &&
                           udev_keyboard_pressed(udev, binds[port][i].key))
                        ret |= (1 << i);
                  }
               }
            }

            return ret;
         }

         if (id < RARCH_BIND_LIST_END)
         {
            if (binds[port][id].valid)
            {
               if (
                     (binds[port][id].key < RETROK_LAST) &&
                     udev_keyboard_pressed(udev, binds[port][id].key)
                     && ((    id != RARCH_GAME_FOCUS_TOGGLE)
                        && !keyboard_mapping_blocked)
                     )
                  return 1;
               else if (
                     (binds[port][id].key < RETROK_LAST) &&
                     udev_keyboard_pressed(udev, binds[port][id].key)
                     && (    id == RARCH_GAME_FOCUS_TOGGLE)
                     )
                  return 1;
               else if (udev_mouse_button_pressed(udev, port,
                        binds[port][id].mbutton))
                  return 1;
            }
         }
         break;
      case RETRO_DEVICE_ANALOG:
         if (binds[port])
         {
            int id_minus_key      = 0;
            int id_plus_key       = 0;
            unsigned id_minus     = 0;
            unsigned id_plus      = 0;
            int16_t ret           = 0;
            bool id_plus_valid    = false;
            bool id_minus_valid   = false;

            input_conv_analog_id_to_bind_id(idx, id, id_minus, id_plus);

            id_minus_valid        = binds[port][id_minus].valid;
            id_plus_valid         = binds[port][id_plus].valid;
            id_minus_key          = binds[port][id_minus].key;
            id_plus_key           = binds[port][id_plus].key;

            if (id_plus_valid && id_plus_key < RETROK_LAST)
            {
               unsigned sym = rarch_keysym_lut[(enum retro_key)id_plus_key];
               if BIT_GET(udev->state, sym)
                  ret = 0x7fff;
            }
            if (id_minus_valid && id_minus_key < RETROK_LAST)
            {
               unsigned sym = rarch_keysym_lut[(enum retro_key)id_minus_key];
               if (BIT_GET(udev->state, sym))
                  ret += -0x7fff;
            }

            return ret;
         }
         break;
      case RETRO_DEVICE_KEYBOARD:
         return (id < RETROK_LAST) && udev_keyboard_pressed(udev, id);

      case RETRO_DEVICE_MOUSE:
      case RARCH_DEVICE_MOUSE_SCREEN:
#ifdef UDEV_TOUCH_SUPPORT
         if (pointer_dev && pointer_dev->touch.is_touch_device)
             return udev_input_touch_state(udev, pointer_dev, binds, 
                     keyboard_mapping_blocked, port, device, idx, id);
#endif
         return udev_mouse_state(udev, port, id,
               device == RARCH_DEVICE_MOUSE_SCREEN);

      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
#ifdef UDEV_TOUCH_SUPPORT
         if (pointer_dev && pointer_dev->touch.is_touch_device)
             return udev_input_touch_state(udev, pointer_dev, binds, 
                     keyboard_mapping_blocked, port, device, idx, id);
#endif
         if (idx == 0) /* multi-touch unsupported (for now) */
            return udev_pointer_state(udev, port, id,
                  device == RARCH_DEVICE_POINTER_SCREEN);
         break;

      case RETRO_DEVICE_LIGHTGUN:
#ifdef UDEV_TOUCH_SUPPORT
         if (pointer_dev && pointer_dev->touch.is_touch_device)
             return udev_input_touch_state(udev, pointer_dev, binds, 
                     keyboard_mapping_blocked, port, device, idx, id);
#endif
         switch ( id )
         {
            /*aiming*/
            case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
            case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
            case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
               return udev_lightgun_aiming_state( udev, port, id );

               /*buttons*/
            case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
            case RETRO_DEVICE_ID_LIGHTGUN_RELOAD:
            case RETRO_DEVICE_ID_LIGHTGUN_AUX_A:
            case RETRO_DEVICE_ID_LIGHTGUN_AUX_B:
            case RETRO_DEVICE_ID_LIGHTGUN_AUX_C:
            case RETRO_DEVICE_ID_LIGHTGUN_START:
            case RETRO_DEVICE_ID_LIGHTGUN_SELECT:
            case RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP:
            case RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN:
            case RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT:
            case RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT:
            case RETRO_DEVICE_ID_LIGHTGUN_PAUSE: /* deprecated */
               {
                  unsigned new_id                = udev_retro_id_to_rarch(id);
                  const uint64_t bind_joykey     = input_config_binds[port][new_id].joykey;
                  const uint64_t bind_joyaxis    = input_config_binds[port][new_id].joyaxis;
                  const uint64_t autobind_joykey = input_autoconf_binds[port][new_id].joykey;
                  const uint64_t autobind_joyaxis= input_autoconf_binds[port][new_id].joyaxis;
                  uint16_t joyport                  = joypad_info->joy_idx;
                  float axis_threshold           = joypad_info->axis_threshold;
                  const uint64_t joykey          = (bind_joykey != NO_BTN)
                     ? bind_joykey  : autobind_joykey;
                  const uint32_t joyaxis         = (bind_joyaxis != AXIS_NONE)
                     ? bind_joyaxis : autobind_joyaxis;
                  if (!keyboard_mapping_blocked)
                     if ((binds[port][new_id].key < RETROK_LAST)
                           && udev_keyboard_pressed(udev, binds[port]
                              [new_id].key))
                        return 1;
                  if (binds[port][new_id].valid)
                  {
                     if ((uint16_t)joykey != NO_BTN && joypad->button(
                              joyport, (uint16_t)joykey))
                        return 1;
                     if (joyaxis != AXIS_NONE &&
                           ((float)abs(joypad->axis(joyport, joyaxis))
                            / 0x8000) > axis_threshold)
                        return 1;
                     if (udev_mouse_button_pressed(udev, port,
                              binds[port][new_id].mbutton))
                        return 1;
                  }
               }
               break;
               /*deprecated*/
            case RETRO_DEVICE_ID_LIGHTGUN_X:
               {
                  udev_input_mouse_t *mouse = udev_get_mouse(udev, port);
                  if (mouse)
                     return udev_mouse_get_x(mouse);
               }
               break;
            case RETRO_DEVICE_ID_LIGHTGUN_Y:
               {
                  udev_input_mouse_t *mouse = udev_get_mouse(udev, port);
                  if (mouse)
                     return udev_mouse_get_y(mouse);
               }
               break;
         }
         break;
   }

   return 0;
}

static void udev_input_free(void *data)
{
   unsigned i;
   udev_input_t *udev = (udev_input_t*)data;

   if (!data || !udev)
      return;

   if (udev->fd >= 0)
      close(udev->fd);

   udev->fd = -1;

   for (i = 0; i < udev->num_devices; i++)
   {
      close(udev->devices[i]->fd);
      free(udev->devices[i]);
   }
   free(udev->devices);

   if (udev->monitor)
      udev_monitor_unref(udev->monitor);
   if (udev->udev)
      udev_unref(udev->udev);

   udev_input_kb_free(udev);

   free(udev);
}

static bool open_devices(udev_input_t *udev,
      enum udev_input_dev_type type, device_handle_cb cb)
{
   const char             *type_str = g_dev_type_str[type];
   struct udev_list_entry     *devs = NULL;
   struct udev_list_entry     *item = NULL;
   struct udev_enumerate *enumerate = udev_enumerate_new(udev->udev);
   const char *name;
   struct udev_device *dev;
   const char *devnode;
   int fd;

   RARCH_DBG("[udev] Adding devices of type %u -> \"%s\"\n", type, type_str);
   if (!enumerate)
      return false;

   udev_enumerate_add_match_property(enumerate, type_str, "1");
   udev_enumerate_add_match_subsystem(enumerate, "input");
   udev_enumerate_scan_devices(enumerate);
   devs = udev_enumerate_get_list_entry(enumerate);

   for (item = devs; item; item = udev_list_entry_get_next(item))
   {
      name = udev_list_entry_get_name(item);

      RARCH_DBG("[udev] Adding device (t%u) \"%s\"\n", type, name);

      /* Get the filename of the /sys entry for the device
       * and create a udev_device object (dev) representing it. */
      dev = udev_device_new_from_syspath(udev->udev, name);
      devnode = udev_device_get_devnode(dev);

      if (devnode)
      {
         fd = open(devnode, O_RDONLY | O_NONBLOCK);

         if (fd != -1)
         {
            if (udev_input_add_device(udev, type, devnode, cb) == 0)
               RARCH_DBG("[udev]: udev_input_add_device error : %s (%s).\n",
                     devnode, strerror(errno));

            close(fd);
         }
      }
      udev_device_unref(dev);
   }

   udev_enumerate_unref(enumerate);

   return true;
}

static void *udev_input_init(const char *joypad_driver)
{
   int mouse = 0;
   int keyboard=0;
   int fd;
   int i;
#ifdef UDEV_XKB_HANDLING
   gfx_ctx_ident_t ctx_ident;
#endif
   udev_input_t *udev   = (udev_input_t*)calloc(1, sizeof(*udev));

   if (!udev)
      return NULL;

   udev->udev = udev_new();
   if (!udev->udev)
      goto error;

   if ((udev->monitor = udev_monitor_new_from_netlink(udev->udev, "udev")))
   {
      udev_monitor_filter_add_match_subsystem_devtype(udev->monitor, "input", NULL);
      udev_monitor_enable_receiving(udev->monitor);
   }

#ifdef UDEV_XKB_HANDLING
   if (init_xkb(-1, 0) == -1)
      goto error;

   video_context_driver_get_ident(&ctx_ident);
#ifdef HAVE_LAKKA
   /* Force xkb_handling on Lakka */
   udev->xkb_handling = true;
#else
   udev->xkb_handling = string_is_equal(ctx_ident.ident, "kms");
#endif /* HAVE_LAKKA */
#endif

#if defined(HAVE_EPOLL)
   fd = epoll_create(32);
   if (fd < 0)
      goto error;
#elif defined(HAVE_KQUEUE)
   fd = kqueue();
   if (fd == -1)
      goto error;
#endif

   udev->fd  = fd;

   if (!open_devices(udev, UDEV_INPUT_KEYBOARD, udev_handle_keyboard))
      goto error;

   if (!open_devices(udev, UDEV_INPUT_MOUSE, udev_handle_mouse))
      goto error;

   if (!open_devices(udev, UDEV_INPUT_TOUCHPAD, udev_handle_mouse))
      goto error;

#ifdef UDEV_TOUCH_SUPPORT
   if (!open_devices(udev, UDEV_INPUT_TOUCHSCREEN, udev_handle_touch))
      goto error;
#endif 

   /* If using KMS and we forgot this,
    * we could lock ourselves out completely. */
   if (!udev->num_devices)
      RARCH_WARN("[udev]: Couldn't open any keyboard, mouse or touchpad. Are permissions set correctly for /dev/input/event* and /run/udev/?\n");

   input_keymaps_init_keyboard_lut(rarch_key_map_linux);

#ifdef __linux__
   linux_terminal_disable_input();
#endif

#ifndef HAVE_X11
   /* TODO/FIXME - this can't be hidden behind a compile-time ifdef */
   RARCH_WARN("[udev]: Full-screen pointer won't be available.\n");
#endif

   /* Reset the indirection array */
   for (i = 0; i < MAX_USERS; i++)
   {
      udev->pointers[i] = -1;
      udev->keyboards[i] = -1;
   }

   for (i = 0; i < udev->num_devices; ++i)
   {
      if (udev->devices[i]->type != UDEV_INPUT_KEYBOARD)
      {
          RARCH_DBG("[udev]: Mouse/Touch #%u: \"%s\" (%s) %s\n",
             mouse,
             udev->devices[i]->ident,
             udev->devices[i]->mouse.abs ? "ABS" : "REL",
             udev->devices[i]->devnode);

          input_config_set_mouse_display_name(mouse, udev->devices[i]->ident);
          udev->pointers[mouse] = i;
          mouse++;
       }
       else
       {
          RARCH_DBG("[udev]: Keyboard #%u: \"%s\" (%s).\n",
             keyboard,
             udev->devices[i]->ident,
             udev->devices[i]->devnode);
          udev->keyboards[keyboard] = i;
          keyboard++;
       }
   }

   return udev;

error:
   udev_input_free(udev);
   return NULL;
}

static uint64_t udev_input_get_capabilities(void *data)
{
   return
        (1 << RETRO_DEVICE_JOYPAD)
      | (1 << RETRO_DEVICE_ANALOG)
      | (1 << RETRO_DEVICE_KEYBOARD)
      | (1 << RETRO_DEVICE_MOUSE)
      | (1 << RETRO_DEVICE_POINTER)
      | (1 << RETRO_DEVICE_LIGHTGUN);
}

static void udev_input_grab_mouse(void *data, bool state)
{
#ifdef HAVE_X11
   Window window;
   Display *display = NULL;

   if (video_driver_display_type_get() != RARCH_DISPLAY_X11)
   {
      RARCH_WARN("[udev]: Mouse grab/ungrab feature unavailable.\n");
      return;
   }

   display = (Display*)video_driver_display_get();
   window  = (Window)video_driver_window_get();

   if (state)
      XGrabPointer(display, window, False,
            ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
            GrabModeAsync, GrabModeAsync, window, None, CurrentTime);
   else
      XUngrabPointer(display, CurrentTime);
#else
   RARCH_WARN("[udev]: Mouse grab/ungrab feature unavailable.\n");
#endif
}

input_driver_t input_udev = {
   udev_input_init,
   udev_input_poll,
   udev_input_state,
   udev_input_free,
   NULL,
   NULL,
   udev_input_get_capabilities,
   "udev",
   udev_input_grab_mouse,
#ifdef __linux__
   linux_terminal_grab_stdin,
#else
   NULL,
#endif
   NULL
};
