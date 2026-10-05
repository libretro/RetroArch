/* The named settings calls of input/input_driver.c, for a sample that
 * links a driver without the frontend.
 *
 * A driver asks for a setting by name (input_config_get_mouse_index(),
 * ...) and no longer takes the settings and picks the field out. The
 * frontend answers those calls in the real program. A sample that has
 * its own settings and its own config_get_ptr() includes this after
 * them, and the calls read the sample's settings the way the
 * frontend's read the real ones.
 *
 * Each sits under the condition its setting has in configuration.h. */

#ifndef SAMPLES_INPUT_CONFIG_STUBS_H__
#define SAMPLES_INPUT_CONFIG_STUBS_H__

unsigned input_config_get_mouse_index(unsigned port)
{ return ((settings_t*)config_get_ptr())->uints.input_mouse_index[port]; }

unsigned input_config_get_joypad_index(unsigned port)
{ return ((settings_t*)config_get_ptr())->uints.input_joypad_index[port]; }

unsigned input_config_get_rumble_gain(void)
{ return ((settings_t*)config_get_ptr())->uints.input_rumble_gain; }

#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
bool input_config_get_nowinkey_enable(void)
{ return ((settings_t*)config_get_ptr())->bools.input_nowinkey_enable; }
#endif

#if defined(_WIN32) && defined(HAVE_WINRAWINPUT)
bool input_config_get_keyboard_background(void)
{ return ((settings_t*)config_get_ptr())->bools.input_keyboard_background; }

bool input_config_get_winraw_xinput_enable(void)
{ return ((settings_t*)config_get_ptr())->bools.input_winraw_xinput_enable; }

bool input_config_get_winraw_player_lights(void)
{ return ((settings_t*)config_get_ptr())->bools.input_winraw_player_lights; }
#endif

bool input_config_overlay_configured(void)
{
   settings_t *settings = (settings_t*)config_get_ptr();
   return settings->bools.input_overlay_enable && *settings->paths.path_overlay;
}

const char *input_config_get_joypad_driver(void)
{ return ((settings_t*)config_get_ptr())->arrays.input_joypad_driver; }

const char *input_config_get_keyboard_layout(void)
{ return ((settings_t*)config_get_ptr())->arrays.input_keyboard_layout; }

#ifdef UDEV_TOUCH_SUPPORT
bool input_config_get_touch_vmouse_pointer(void)
{ return ((settings_t*)config_get_ptr())->bools.input_touch_vmouse_pointer; }
bool input_config_get_touch_vmouse_mouse(void)
{ return ((settings_t*)config_get_ptr())->bools.input_touch_vmouse_mouse; }
bool input_config_get_touch_vmouse_touchpad(void)
{ return ((settings_t*)config_get_ptr())->bools.input_touch_vmouse_touchpad; }
bool input_config_get_touch_vmouse_trackball(void)
{ return ((settings_t*)config_get_ptr())->bools.input_touch_vmouse_trackball; }
bool input_config_get_touch_vmouse_gesture(void)
{ return ((settings_t*)config_get_ptr())->bools.input_touch_vmouse_gesture; }
#endif

/* and the one thing the udev input driver changes in them */
bool input_driver_first_start_fallback(const char *ident)
{
   settings_t *settings = (settings_t*)config_get_ptr();
   if (!settings->bools.menu_show_start_screen || !ident)
      return false;
   strlcpy(settings->arrays.input_driver, ident,
         sizeof(settings->arrays.input_driver));
   return true;
}

#endif
