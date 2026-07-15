/* Single-source definitions: on-screen keyboard overlay group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

#ifdef HAVE_OVERLAY
/* Preset path defaults to the OSK overlay directory at runtime, so
 * the default rides in via the _DS variant reading that settings
 * field; the "cfg" filter and overlay-init command are row data. */
S_PATH_DS(path_osk_overlay, OSK_OVERLAY_PRESET,
      "osk_overlay_preset",
      directory_osk_overlay, SD_FLAG_NONE, CMD_EVENT_OVERLAY_INIT, "cfg", NULL, 0,
      "OSK Overlay Preset",
      "Select and load an on-screen keyboard overlay preset.")
/* Auto-scale toggles a live rescale: refresh on left/right/ok and a
 * scale-factor command, applied automatically. */
S_BOOL_EX(input_osk_overlay_auto_scale, INPUT_OSK_OVERLAY_AUTO_SCALE,
      "input_osk_overlay_auto_scale",
      DEFAULT_INPUT_OVERLAY_AUTO_SCALE, SD_FLAG_CMD_APPLY_AUTO, 0, CMD_EVENT_OVERLAY_SET_SCALE_FACTOR, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Auto-Scale OSK Overlay",
      "Automatically scale the on-screen keyboard overlay to fit the screen and adjust for aspect ratios.")
S_FLOAT_EX(input_osk_overlay_opacity, OSK_OVERLAY_OPACITY,
      "input_osk_overlay_opacity",
      DEFAULT_INPUT_OVERLAY_OPACITY, "%.2f", SD_FLAG_CMD_APPLY_AUTO, SDESC_RANGE_MINMAX, CMD_EVENT_OVERLAY_SET_ALPHA_MOD, 0.0f, 1.0f, 0.01f, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "OSK Overlay Opacity",
      "Opacity of the on-screen keyboard overlay.")
#endif
