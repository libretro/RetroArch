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
/* The configuration row lives under defined(HAVE_GFX_WIDGETS), which
 * draws the picture; other passes are unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || defined(HAVE_GFX_WIDGETS)
S_BOOL(camera_pip, CAMERA_PIP,
      "camera_pip",
      false, SD_FLAG_NONE, 0, CMD_EVENT_NONE,
      "Picture-in-Picture",
      "Show the camera's live picture in a corner of the screen while content runs. A core's own camera is shown as the core uses it.")
S_UINT(camera_pip_corner, CAMERA_PIP_CORNER,
      "camera_pip_corner",
      3 /* CAMERA_PIP_BOTTOM_RIGHT */, SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 0, 3, 1, 0, setting_action_ok_uint, setting_get_string_representation_camera_pip_corner,
      "Picture-in-Picture Position",
      "The corner of the screen the camera's picture is shown in.")
S_UINT(camera_pip_size, CAMERA_PIP_SIZE,
      "camera_pip_size",
      25, SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 10, 50, 5, 0, setting_action_ok_uint, setting_get_string_representation_percentage,
      "Picture-in-Picture Size",
      "The height of the camera's picture, as a share of the screen's.")
#endif
