/* Handlers settings descriptor rows may name, as SDX(slot, function).
 * menu_setting.c includes this list twice, for the SDX_* index enum
 * and for the table it indexes; guards match the handlers'. */

SDX(OK, setting_action_ok_uint)
SDX(OK, setting_action_ok_uint_special)
SDX(OK, setting_action_ok_crt_switch_resolution_super)
SDX(OK, setting_action_ok_retropad_bind)
SDX(OK, setting_action_ok_video_openxr_refresh_rate)
SDX(OK, setting_action_ok_video_refresh_rate_auto)
SDX(OK, setting_bool_action_left_with_refresh)
SDX(OK, setting_bool_action_ok_video_openxr_enable)
#ifdef HAVE_ASIO
SDX(OK, setting_action_asio_control_panel)
#endif
#ifdef HAVE_MODELINE
SDX(OK, setting_action_crt_switch_write_edid)
#endif
#if defined(HAVE_KEYCHAIN) && defined(HAVE_CRYPTO) && defined(HAVE_CONFIGFILE)
SDX(OK, setting_action_keychain_passphrase)
#endif
#ifdef HAVE_NETWORKING
SDX(OK, setting_action_ok_color_rgb)
#endif
#ifndef RARCH_CONSOLE
SDX(OK, setting_string_action_ok_audio_device)
SDX(OK, setting_string_action_ok_midi_device)
#ifdef HAVE_MICROPHONE
SDX(OK, setting_string_action_ok_microphone_device)
#endif
#endif

SDX(START, directory_action_start_generic)
SDX(START, setting_generic_action_start_default)
SDX(START, setting_action_start_custom_vp_width)
SDX(START, setting_action_start_custom_vp_height)
SDX(START, setting_action_start_video_openxr_enable)
SDX(START, setting_action_start_video_refresh_rate_auto)
#ifndef RARCH_CONSOLE
SDX(START, setting_string_action_start_audio_device)
SDX(START, setting_string_action_start_midi_device)
#ifdef HAVE_MICROPHONE
SDX(START, setting_string_action_start_microphone_device)
#endif
#endif

SDX(SELECT, setting_action_ok_video_refresh_rate_auto)
#ifdef HAVE_NETWORKING
SDX(SELECT, setting_action_ok_color_rgb)
#endif

SDX(LEFT, setting_bool_action_left_with_refresh)
SDX(LEFT, setting_uint_action_left_with_refresh)
SDX(LEFT, setting_uint_action_left_default)
SDX(LEFT, setting_action_left_retropad_bind)
SDX(LEFT, setting_uint_action_left_crt_switch_resolution_super)
SDX(LEFT, setting_uint_action_left_custom_vp_width)
SDX(LEFT, setting_uint_action_left_custom_vp_height)
SDX(LEFT, setting_uint_action_left_video_openxr_refresh_rate)
SDX(LEFT, setting_bool_action_ok_video_openxr_enable)
#ifdef HAVE_OZONE
SDX(LEFT, setting_string_action_left_ozone_menu_color_theme)
#endif
#ifndef RARCH_CONSOLE
SDX(LEFT, setting_string_action_left_audio_device)
SDX(LEFT, setting_string_action_left_midi_input)
SDX(LEFT, setting_string_action_left_midi_output)
#ifdef HAVE_MICROPHONE
SDX(LEFT, setting_string_action_left_microphone_device)
#endif
#endif

SDX(RIGHT, setting_bool_action_right_with_refresh)
SDX(RIGHT, setting_uint_action_right_with_refresh)
SDX(RIGHT, setting_uint_action_right_default)
SDX(RIGHT, setting_action_right_retropad_bind)
SDX(RIGHT, setting_uint_action_right_crt_switch_resolution_super)
SDX(RIGHT, setting_uint_action_right_custom_vp_width)
SDX(RIGHT, setting_uint_action_right_custom_vp_height)
SDX(RIGHT, setting_uint_action_right_video_openxr_refresh_rate)
SDX(RIGHT, setting_bool_action_ok_video_openxr_enable)
#ifdef HAVE_OZONE
SDX(RIGHT, setting_string_action_right_ozone_menu_color_theme)
#endif
#ifndef RARCH_CONSOLE
SDX(RIGHT, setting_string_action_right_audio_device)
SDX(RIGHT, setting_string_action_right_midi_input)
SDX(RIGHT, setting_string_action_right_midi_output)
#ifdef HAVE_MICROPHONE
SDX(RIGHT, setting_string_action_right_microphone_device)
#endif
#endif

SDX(CHANGE, frontend_log_level_change_handler)
#ifdef HAVE_OVERLAY
SDX(CHANGE, overlay_enable_toggle_change_handler)
SDX(CHANGE, overlay_show_mouse_cursor_change_handler)
#endif
#ifdef HAVE_RUNAHEAD
SDX(CHANGE, runahead_change_handler)
#endif

SDX(RESOLVER, settings_def_audio_latency)
#ifdef HAVE_MICROPHONE
SDX(RESOLVER, settings_def_microphone_latency)
#endif
#if defined(__APPLE__) && defined(HAVE_VULKAN)
SDX(RESOLVER, settings_def_metal_arg_buffers)
#endif
