/* Single-source definitions: touch virtual mouse group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

#ifdef UDEV_TOUCH_SUPPORT
S_BOOL(input_touch_vmouse_pointer, INPUT_TOUCH_VMOUSE_POINTER,
      "input_touch_vmouse_pointer",
      DEFAULT_INPUT_TOUCH_VMOUSE_POINTER, SD_FLAG_NONE, 0, 0,
      "Touch VMouse as Pointer",
      "Enable to pass touch events from the input touchscreen.")
#endif
#ifdef UDEV_TOUCH_SUPPORT
S_BOOL(input_touch_vmouse_mouse, INPUT_TOUCH_VMOUSE_MOUSE,
      "input_touch_vmouse_mouse",
      DEFAULT_INPUT_TOUCH_VMOUSE_MOUSE, SD_FLAG_NONE, 0, 0,
      "Touch VMouse as Mouse",
      "Enable virtual mouse emulation using input touch events.")
#endif
#ifdef UDEV_TOUCH_SUPPORT
S_BOOL(input_touch_vmouse_touchpad, INPUT_TOUCH_VMOUSE_TOUCHPAD,
      "input_touch_vmouse_touchpad",
      DEFAULT_INPUT_TOUCH_VMOUSE_TOUCHPAD, SD_FLAG_NONE, 0, 0,
      "Touch VMouse Touchpad Mode",
      "Enable along with Mouse to utilize use the touch screen as a touchpad.")
#endif
#ifdef UDEV_TOUCH_SUPPORT
S_BOOL(input_touch_vmouse_trackball, INPUT_TOUCH_VMOUSE_TRACKBALL,
      "input_touch_vmouse_trackball",
      DEFAULT_INPUT_TOUCH_VMOUSE_TRACKBALL, SD_FLAG_NONE, 0, 0,
      "Touch VMouse Trackball Mode",
      "Enable along with Mouse to utilize use the touch screen as a trackball, adding inertia to the pointer.")
#endif
#ifdef UDEV_TOUCH_SUPPORT
S_BOOL(input_touch_vmouse_gesture, INPUT_TOUCH_VMOUSE_GESTURE,
      "input_touch_vmouse_gesture",
      DEFAULT_INPUT_TOUCH_VMOUSE_GESTURE, SD_FLAG_NONE, 0, 0,
      "Touch VMouse Gestures",
      "Enable touchscreen gestures, including tapping, tap-dragging, and finger swiping.")
#endif
