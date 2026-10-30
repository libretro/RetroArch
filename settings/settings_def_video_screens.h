/* Indexed menu templates; configuration uses the indexed XR passes. */
S_ACTION(VIDEO_SCREEN_SELECTED,
      "video_screen_selected",
      "Screen",
      "Select the floating screen to adjust.")
S_FLOAT(video_screen_xr_x[0], VIDEO_SCREEN_XR_X,
      "video_screen_xr_x",
      0.0f, "%.1f m", SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, -10, 10, .1, NULL, NULL,
      "Horizontal Offset",
      "Move this screen left or right in metres.")
S_FLOAT(video_screen_xr_y[0], VIDEO_SCREEN_XR_Y,
      "video_screen_xr_y",
      0.0f, "%.1f m", SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, -10, 10, .1, NULL, NULL,
      "Vertical Offset",
      "Move this screen up or down in metres.")
S_FLOAT(video_screen_xr_z[0], VIDEO_SCREEN_XR_Z,
      "video_screen_xr_z",
      0.0f, "%.1f m", SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, -10, 10, .1, NULL, NULL,
      "Depth Offset",
      "Move this screen towards or away from you in metres.")
S_FLOAT(video_screen_xr_yaw[0], VIDEO_SCREEN_XR_YAW,
      "video_screen_xr_yaw",
      0.0f, "%.0f deg", SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, -180, 180, 1, NULL, NULL,
      "Yaw",
      "Turn this screen left or right in degrees.")
S_FLOAT(video_screen_xr_pitch[0], VIDEO_SCREEN_XR_PITCH,
      "video_screen_xr_pitch",
      0.0f, "%.0f deg", SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, -180, 180, 1, NULL, NULL,
      "Pitch",
      "Tilt this screen up or down in degrees.")
S_FLOAT(video_screen_xr_roll[0], VIDEO_SCREEN_XR_ROLL,
      "video_screen_xr_roll",
      0.0f, "%.0f deg", SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, -180, 180, 1, NULL, NULL,
      "Roll",
      "Rotate this screen in degrees.")
S_FLOAT(video_screen_xr_scale[0], VIDEO_SCREEN_XR_SCALE,
      "video_screen_xr_scale",
      1.0f, "%.1f", SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 0.1, 10, .1, NULL, NULL,
      "Scale",
      "Resize this floating screen.")
S_BOOL(video_screen_xr_visible[0], VIDEO_SCREEN_XR_VISIBLE,
      "video_screen_xr_visible",
      true, SD_FLAG_NONE, 0, 0,
      "Visible",
      "Show this floating screen. The menu remains available when screens are hidden.")
S_ACTION(VIDEO_SCREEN_XR_RESET,
      "video_screen_xr_reset",
      "Reset Screen",
      "Restore the selected floating screen's position, rotation, scale and visibility.")
S_ACTION(VIDEO_SCREEN_XR_RESET_ALL,
      "video_screen_xr_reset_all",
      "Reset All Screens",
      "Restore every screen's position, rotation, scale and visibility, including screens outside the current map, and return the menu to where it opens unplaced.")
S_ACTION(VIDEO_SCREEN_XR_EDIT,
      "video_screen_xr_edit",
      "Edit Screens",
      "Close the menu and move the headset's screens and menu with the controllers: grip grabs, the trigger tilts, the stick changes distance and size, the free hand's stick turns and tilts, the main button resets, Select Next and Select Previous choose an item.")
