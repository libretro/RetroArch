# Samsung S Pen Hover Phantom Click Prevention

## Overview

This document explains the comprehensive S Pen hover→tap prevention system implemented in RetroArch Android to resolve phantom touchscreen clicks caused by Samsung firmware synthesizing touch events during stylus hover transitions.

## Problem Background

### Original Issue
Samsung S Pen devices generate phantom touchscreen events when the stylus transitions in/out of hover proximity. These synthesized events cause unwanted menu clicks and paint strokes in RetroArch, making stylus hover unusable.

**Symptoms:**
- Hovering over menu items causes automatic selection
- S Pen hover transitions trigger paint strokes in emulated games
- Quick hover gestures produce delayed phantom clicks
- Menu navigation becomes unusable with stylus proximity

### Root Cause Analysis
Based on xlabs' previous research and our investigation, the issue stems from Samsung's firmware architecture:

1. **Dual Event Generation:** S Pen hover events generate both stylus events (`0x5002`) AND phantom touchscreen events (`0x1002`)
2. **Timing Dependency:** Phantom events arrive 50-100ms after hover transitions
3. **Source Ambiguity:** Some stylus events are reported through `TOUCHSCREEN` source, making source-only filtering insufficient
4. **Shared Input Pipeline:** Android's input system processes both event types through the same motion event handlers

## Multi-Layer Protection Architecture

The implementation uses a defense-in-depth approach with four complementary protection layers:

### Layer 1: Hover Guard (Temporal/Spatial Filtering)
**Location:** `input/drivers/android_input.c` - Global state variables
```c
static bool     g_hover_guard_active = false;
static int64_t  g_hover_guard_until_ms = 0;
static float    g_hover_guard_x = 0.0f, g_hover_guard_y = 0.0f;
```

**Function:** Filters phantom touchscreen events immediately following stylus hover transitions.

**Logic:**
- Armed on any stylus `HOVER_ENTER/MOVE/EXIT` event (100ms window)
- Drops finger/touch events within 12px radius during guard period
- Prevents Samsung synthesized touches from promoting to clicks

### Layer 2: Stylus Proximity Tracking
**Location:** `android_input_t` struct fields
```c
bool    stylus_proximity_active;
int64_t stylus_proximity_until_ns;
```

**Function:** Tracks stylus hover state with longer temporal window for quick-tap suppression.

**Logic:**
- Activated on stylus hover events (120ms window)
- Disables quick-tap mouse emulation while stylus is nearby
- Uses nanosecond timestamps for precise timing control

### Layer 3: Quick-Tap Defense-in-Depth
**Location:** `android_check_quick_tap()` function
```c
if (g_hover_guard_active) {
    android->quick_tap_time = 0;
    return 0;
}
```

**Function:** Final safety layer preventing phantom touch promotion to mouse clicks.

**Logic:**
- Direct hover guard check inside quick-tap function
- Cancels pending quick-tap timers when guard is active
- Ensures no hover transition can accidentally trigger clicks

### Layer 4: Menu Gesture Isolation
**Location:** `menu/menu_driver.c` - Gesture detection logic
```c
if (menu_input->pointer.type != MENU_POINTER_TOUCHSCREEN) {
    point.gesture = MENU_INPUT_GESTURE_NONE;
}
```

**Function:** Restricts gesture-based menu interactions to touchscreen-only input.

**Logic:**
- Blocks `TAP/SHORT_PRESS/LONG_PRESS/SWIPE` gestures for mouse/stylus input
- Forces stylus to use explicit button presses rather than motion timing
- Prevents hover motion from being interpreted as intentional gestures

## Input Channel Separation

### Design Philosophy
The implementation maintains strict separation between stylus and finger input channels using the `mouse_activated` flag as a channel selector.

### Channel Logic
```c
if (android->mouse_activated) {
    // Stylus/mouse mode: Direct button state access
    return android->mouse_l;
} else {
    // Touch mode: Quick-tap emulation with proximity guards
    if (!android->stylus_proximity_active && !g_hover_guard_active)
        return android_check_quick_tap(android);
}
```

### State Transitions
1. **Stylus Contact:** `mouse_activated = true` → Switches to mouse mode
2. **Stylus Lift:** `mouse_activated = false` → Returns to touch mode  
3. **Finger Touch:** Only processes when `mouse_activated = false`

This design prevents input interference while maintaining responsive behavior.

## ToolType Classification System

### Primary Classification
Uses Android NDK `AMotionEvent_getToolType()` as the primary discriminator:
```c
int32_t tool = AMotionEvent_getToolType(event, 0);
bool is_stylus = (tool == AMOTION_EVENT_TOOL_TYPE_STYLUS);
bool is_finger = (tool == AMOTION_EVENT_TOOL_TYPE_FINGER);
```

### Fallback Classification
When toolType is unavailable or unknown, falls back to input source classification:
```c
if (!is_stylus && !is_finger) {
    is_stylus = ((source & AINPUT_SOURCE_STYLUS) == AINPUT_SOURCE_STYLUS);
    is_finger = ((source & AINPUT_SOURCE_TOUCHSCREEN) == AINPUT_SOURCE_TOUCHSCREEN);
}
```

**Note:** ToolType classification is more reliable than source-only filtering because xlabs' research showed stylus events sometimes come through `TOUCHSCREEN` source.

## Contact Detection Logic

### Hardware-Based Detection
Uses actual pressure and distance sensors for precise contact detection:
```c
float pressure = AMotionEvent_getPressure(event, motion_ptr);
float distance = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_DISTANCE, motion_ptr);
bool tip_down = (action != AMOTION_EVENT_ACTION_UP) &&
                (pressure > 0.01f) && 
                (distance <= 0.0f);
```

### Settings Integration
Respects `input_stylus_require_contact_for_click` user preference:
- **ON:** Only tip pressure contact triggers clicks (hover never clicks)
- **OFF:** Tip contact OR side button triggers clicks (hover still never clicks)

## Time Unit Consistency

### Timestamp Handling
The implementation carefully manages different time units across the Android NDK:

- **Event Times:** `AMotionEvent_getEventTime()` returns nanoseconds
- **System Time:** `cpu_features_get_time_usec()` returns microseconds  
- **Guard Windows:** Stored in milliseconds for human-readable timeouts

### Conversion Patterns
```c
// Event time (ns) to milliseconds
event_time_ms = AMotionEvent_getEventTime(event) / 1000000;

// Proximity timeout (120ms in nanoseconds)  
android->stylus_proximity_until_ns = AMotionEvent_getEventTime(event) + 120000000;

// Time comparison (both converted to milliseconds)
if (now / 1000 > android->stylus_proximity_until_ns / 1000000)
```

## Settings Framework Integration

### Configuration Options
Two user-configurable settings are provided:

1. **`input_stylus_require_contact_for_click`**
   - Controls stylus click triggering behavior
   - Default: ON (contact required for clicks)

2. **`input_stylus_hover_moves_pointer`** 
   - Controls cursor movement during hover
   - Default: OFF (reduces phantom potential)

### Menu Integration
Full settings menu integration includes:
- Configuration entries in `menu/menu_setting.c`
- Internationalization support in `msg_hash_*.h`
- Context help in `menu_cbs_sublabel.c`
- Display logic in `menu_displaylist.c`

## Performance Considerations

### Minimal Overhead
The protection system adds minimal computational overhead:
- Static guard variables avoid memory allocation
- Simple boolean/timestamp checks in hot paths
- Guard expiration only computed when needed
- No impact on non-stylus input processing

### Guard Window Tuning
Current timeouts are empirically determined:
- **Hover Guard:** 100ms (phantom event suppression)
- **Proximity Tracking:** 120ms (quick-tap suppression)  
- **Spatial Tolerance:** 12px (phantom event detection)

These values can be adjusted per device if needed.

## Future Maintainers

### Key Code Locations
- **Core Logic:** `input/drivers/android_input.c`
  - Lines ~105-143: Hover guard implementation
  - Lines ~698-722: Quick-tap defense function
  - Lines ~853-983: Stylus event processing
  - Lines ~2040-2055, ~2128-2143: Input state queries

- **Menu Integration:** `menu/menu_driver.c`
  - Lines ~6084-6170: Gesture isolation logic

### Debug Support  
Enable `DEBUG_ANDROID_INPUT` flag for comprehensive logging:
```c
#ifdef DEBUG_ANDROID_INPUT
RARCH_LOG("[RA Input] act=%d src=0x%x tool=%d dev=%d btn=0x%x dropped=%d\n", ...);
#endif
```

### Testing Requirements
Always test changes on actual Samsung S Pen devices:
- Galaxy Note series
- Galaxy Tab S series  
- Galaxy Z Fold series

Virtual devices cannot reproduce the firmware phantom event behavior.

## References

- **xlabs Research:** Previous investigation identified the need for stylus/touch distinction
- **Samsung S Pen Documentation:** Android NDK motion event handling
- **RetroArch Input Architecture:** Existing mouse emulation and quick-tap systems

## Commit History

The implementation was developed across multiple commits:
- `7daf2ae`: Base S Pen implementation with toolType classification
- `050a396`: Comprehensive hover→tap prevention with proximity tracking  
- `4c47eac`: Defense-in-depth enhancement to quick-tap function

---

*This documentation should be updated when modifying the S Pen implementation to ensure future maintainers understand the complete protection strategy.*