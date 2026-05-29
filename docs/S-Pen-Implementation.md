# Samsung S Pen Input Framework (Android)

## Overview

This document describes the S Pen input framework for RetroArch on Android. It has two goals:

1) Provide a robust, regression‑free stylus foundation in RetroArch itself (input driver + menu integration).

2) Enable cores to consume stylus input via standard libretro devices (RETRO_DEVICE_POINTER, mouse, lightgun) without conflicting settings between RetroArch and cores.

The framework resolves historical regressions (phantom hover taps, mixed touch/stylus channels) while keeping a clean contract: RetroArch gathers and normalizes stylus signals; cores decide how to interpret them.

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

### Side Button Support (Updated August 2025)
Comprehensive button detection supports both common stylus button types:
```c
buttons = p_AMotionEvent_getButtonState ? AMotionEvent_getButtonState(event) : 0;
side_primary   = (buttons & AMOTION_EVENT_BUTTON_STYLUS_PRIMARY)   != 0;
side_secondary = (buttons & AMOTION_EVENT_BUTTON_STYLUS_SECONDARY) != 0;
bool side_pressed = side_primary || side_secondary;
```

This ensures compatibility with stylus devices that report barrel button as either PRIMARY or SECONDARY.

## Contact Detection Logic (Updated October 2025)

### Separated Contact and Click Detection

**Critical Design Change:** Contact detection and click detection are now separated for natural, responsive stylus behavior matching other Android apps.

#### Contact Detection (Cursor Movement)
Uses distance-based detection for instant cursor response:
```c
// Physical contact - instant, no pressure needed
bool tip_touching = (action != AMOTION_EVENT_ACTION_UP) && (distance <= 0.0f);
```

**Purpose:** Instant cursor movement when stylus touches screen, mimicking behavior of native Android apps.

#### Click Detection (Button Presses)
Uses pressure-based detection with configurable threshold:
```c
// Click/press - requires sufficient pressure
float pressure_threshold = 0.0f + ((100 - sensitivity) * 0.00025f);
bool tip_down = tip_touching && (pressure > pressure_threshold);
```

**Purpose:** Configurable click sensitivity independent of cursor movement.

### Pressure Sensitivity Setting

**New Setting:** `input_stylus_pressure_sensitivity` (1-100, default 70)

**Threshold Calculation:**
- `sensitivity=100`: threshold=0.0000 (instant click on contact)
- `sensitivity=70`: threshold=0.0075 (default - light touch)
- `sensitivity=1`: threshold=0.0248 (firm press required)

**Formula:** `threshold = 0.0f + ((100 - sensitivity) * 0.00025f)`

**User Experience:**
- Higher values = more sensitive (easier to click)
- Lower values = less sensitive (harder to click)
- Recommended: 70 for normal use, 40-50 with screen protectors

### State Latching

Two independent latches track stylus state:

```c
// Contact latch - for cursor movement (distance-based)
if (tip_touching)
   android->stylus_contact_active = true;

// Press latch - for clicks (pressure-based)
if (stylus_pressed)
   android->stylus_press_active = true;
```

Both latches are cleared on `ACTION_UP` to handle quick taps reliably.

### Settings Integration
RetroArch exposes user‑visible toggles under Input (Android only):

- `input_stylus_enable` — Master switch. When OFF, all stylus events are ignored by the input driver.
- `input_stylus_pressure_sensitivity` — Controls click pressure threshold (1-100). Higher = more sensitive.
- `input_stylus_require_contact_for_click` — When ON, only tip contact counts as a click (side button alone does not click). When OFF, tip contact OR side button may click.
- `input_stylus_hover_moves_pointer` — When ON, stylus hover updates pointer coordinates without asserting PRESSED (useful for mouse/lightgun cores). When OFF, hover does not move the pointer.

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

## Overlay Auto-Hide (Stylus Active)

While an S-Pen is in use, the on-screen touch overlay is automatically hidden,
because the overlay's input zones conflict with stylus taps. The overlay is
restored once no stylus event has occurred for ~2 seconds, so the touch menu
remains reachable when the pen is set down.

**Mechanism:**
- `input/drivers/android_input.c` records `g_android_stylus_last_event_ns` on
  every stylus event (set before any early return, so physical pen presence
  always refreshes the timer).
- `android_input_stylus_recently_active()` returns true if a stylus event
  occurred within the last 2 seconds (`2000000000` ns window).
- `runloop.c` (Android only) edge-triggers on that signal, sitting next to the
  existing controller-connected hide block: it calls `input_overlay_unload()`
  when the stylus becomes active and `input_overlay_init()` when it goes idle,
  tracked via a `static bool last_stylus_hidden`.

This reuses the same overlay-toggle mechanism RetroArch already uses for the
"hide overlay when a controller is connected" behavior. It partially anticipates
upstream feature request **libretro/RetroArch#18178** (conditional overlay
profiles / input-aware overlay switching); generalizing this Android-only,
stylus-specific toggle into a cross-platform overlay-profile resolver is the
natural path to upstreaming it.

## Contract With Cores

RetroArch passes stylus data to cores via standard libretro interfaces. Cores choose how to map stylus signals to gameplay semantics without clashing with RetroArch settings.

- Pointer device: `RETRO_DEVICE_POINTER`
  - PRESSED is true only on tip contact (and per user settings); hover never asserts PRESSED.
  - Index semantics:
    - `index 0`: current stylus cursor (hover/contact)
    - `index 1`: tip contact coordinates (active only during tip contact)
    - `index 2`: barrel/side button coordinates (active only while held)

- Mouse/lightgun: RetroArch does not force mouse button states for stylus. Cores that consume mouse or lightgun can synthesize LEFT/RIGHT/TRIGGER/RELOAD from pointer + stylus side button according to their own options.

### Expected Core Behavior Examples

- SNES9x drawing/painting games in mouse mode:
  - Hover moves cursor (enable `input_stylus_hover_moves_pointer`).
  - Tip tap -> left click.
  - Barrel button -> right click.

- Lightgun‑style cores:
  - Hover moves crosshair (enable `input_stylus_hover_moves_pointer`).
  - Tap or barrel button mapped to Fire/Reload per core settings.

In all cases, avoid conflicting configuration: RetroArch provides raw, normalized pointer + side button signals; the core owns the mapping to game actions. Do not duplicate the same mapping toggle in both places.

## Future Maintainers

### Key Code Locations

> Line numbers drift when the branch is rebased onto upstream; the function and
> symbol names are the stable anchors. Ranges below are current as of this revision.

- **Core Logic:** `input/drivers/android_input.c`
  - `hover_guard_arm()` / `hover_guard_drop()` + guard state vars — ~104-134
  - `android_check_quick_tap()` (hover-guard gate at the top) — ~722-749
  - Stylus HOVER + side-button processing — ~915-1049
  - Stylus contact (DOWN/MOVE/UP) processing, incl. pointer index 0/1/2 — ~1051-1225
  - Pressure threshold formula — ~1083
  - Input-state queries: `MOUSE_LEFT` ~2283-2309, `LIGHTGUN_TRIGGER` ~2377-2397
  - Overlay auto-hide: `g_android_stylus_last_event_ns` ~233, `android_input_stylus_recently_active()` ~237-241

- **Menu Integration:** `menu/menu_driver.c`
  - Gesture isolation (`pointer.type != MENU_POINTER_TOUCHSCREEN`) — ~6233-6237

- **Overlay Auto-Hide Hook:** `runloop.c`
  - `extern android_input_stylus_recently_active()` ~102; Android overlay-hide block ~5752-5772

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

## Build Integration

### Android Build System
The S Pen implementation has been fully integrated into the RetroArch Android build system:

- **Enum Declarations:** Added to `/msg_hash.h` for proper compilation
- **Settings Integration:** Menu entries, internationalization, and help text included
- **Griffin Build:** Successfully compiles through the unified griffin.c build system
- **Multi-Architecture:** Supports ARM64, ARM32, and x86 Android targets

### Build Artifacts
The Android Gradle build produces APK variants per flavor, e.g.:
- `aarch64` debug flavor — ARM64 build for modern devices
- `playStoreNormal` debug flavor — universal multi-architecture build

(Exact output filenames and sizes depend on the Gradle flavor/build type; see
`pkg/android/phoenix/build.gradle`.)

### Build Requirements
- Android SDK with API level 16+ support
- Android NDK 22.0.7026061 (tested and verified)
- Gradle build system with native build tools

## Testing Requirements

### Hardware Testing
**Critical:** Always test on actual Samsung S Pen devices, as virtual devices cannot reproduce firmware phantom event behavior.

**Supported Device Classes:**
- Galaxy Note series (Note 8, 9, 10, 20, etc.)
- Galaxy Tab S series with S Pen support
- Galaxy Z Fold series with S Pen support
- Any Samsung device with S Pen digitizer

**Testing Scenarios:**
1. **Hover Navigation:** S Pen hovering should move cursor without triggering clicks
2. **Phantom Prevention:** Quick hover transitions should not generate unwanted touches
3. **Instant Contact Response:** Cursor should move INSTANTLY when stylus touches screen (no pressure delay)
4. **Pressure Sensitivity:** Click detection should respect pressure sensitivity setting (test at 1, 70, 100)
5. **Natural Feel:** Cursor movement should feel identical to other Android apps (Chrome, launcher, etc.)
6. **Menu Interaction:** Stylus should require explicit button presses for menu gestures
7. **Channel Separation:** Finger touches and stylus input should operate independently

**Regression Testing:**
- Ensure cursor moves instantly on contact (October 2025 fix)
- Verify pressure sensitivity only affects clicks, not contact detection
- Test that light touches register cursor movement even with low sensitivity

## Commit History

> **Note:** This branch is periodically rebased onto upstream `master`, which
> rewrites commit hashes. Do not rely on specific hashes here — list the current
> series with `git log --oneline --grep="S Pen\|S-Pen\|stylus"`.

The implementation was developed across a series of commits (described by purpose):
- Base S Pen implementation with toolType classification
- Comprehensive hover→tap prevention with proximity tracking
- Defense-in-depth enhancement to the quick-tap function
- Side button hover-click detection, then PRIMARY + SECONDARY button support
- Semantic S-Pen pointer indices (the index 0/1/2 contract)
- Configurable pressure sensitivity setting
- Separate contact detection from click detection (the October 2025 breakthrough)
- Pressure-sensitivity menu slider fixes
- Overlay auto-hide while the stylus is active

### Major Breakthrough (October 2025)

**Contact/Click Separation**:
- **Issue**: Previous implementation used pressure threshold for BOTH contact detection and click detection
- **Symptom**: Cursor movement felt sluggish, required intentional pressure unlike other Android apps
- **Root Cause**: Pressure sensitivity was controlling the wrong behavior - contact should be distance-based, not pressure-based
- **Solution**: Separated detection into two independent systems:
  - **Contact (tip_touching)**: Distance-based (`distance <= 0`) - instant cursor response
  - **Click (tip_down)**: Pressure-based (`pressure > threshold`) - configurable sensitivity
- **Result**: Cursor now responds instantly when S-Pen touches screen, feels natural like other Android apps
- **Impact**: This is the correct architectural approach - other apps use distance for contact, pressure for click force

**Key Insight:** The pressure sensitivity setting should ONLY control click force, never contact detection. This matches how all native Android apps handle stylus input.

### Recent Fixes (August 2025)

**Side Button Drag Implementation**:
- **Issue**: Side button drag was broken due to `require_contact` gate preventing hover-drag when contact setting was enabled
- **Issue**: Only `STYLUS_PRIMARY` button was supported, missing `STYLUS_SECONDARY` button devices
- **Solution**: Removed contact requirement gate from hover path since hover-drag ≠ click semantics
- **Solution**: Added support for both PRIMARY and SECONDARY stylus buttons for broader device compatibility
- **Result**: Side button hover-drag now works regardless of contact setting and supports more stylus devices

## Build Status

✅ Core framework integrated and building successfully  
✅ Android APK ready for on‑device testing  
✅ Multi‑layer protection active (hover guard, proximity tracking, quick‑tap gating, menu isolation)

---

*This documentation should be updated when modifying the S Pen implementation to ensure future maintainers understand the complete protection strategy.*
