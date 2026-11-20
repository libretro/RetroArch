/* Single-source definitions: camera permission setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(camera_allow, CAMERA_ALLOW,
      "camera_allow",
      false, SD_FLAG_NONE, 0, CMD_EVENT_CAMERA_ALLOW_APPLY,
      "Allow Camera",
      "Allow cores to access the camera.")

/* String settings keep their configuration.c row literal. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_STRING(camera_device, CAMERA_DEVICE,
      "camera_device",
      "", SD_FLAG_ALLOW_INPUT, 0, NULL, NULL, setting_generic_action_start_default, NULL, NULL, NULL, ST_UI_TYPE_STRING_LINE_EDIT,
      "Device",
      "Override the default camera device the camera driver uses. This is driver dependent; leave empty for the default.")
#endif
