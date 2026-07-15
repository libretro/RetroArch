/* Single-source definitions: notification position group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_FLOAT_EX(video_font_size, VIDEO_FONT_SIZE,
      "video_font_size",
      DEFAULT_FONT_SIZE, "%.1f", SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_REINIT, 1.00, 100.00, 1.0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Notification Size",
      "Specify the font size in points. When widgets are used, this size has effect only to on-screen statistics display.")
S_FLOAT_EX(video_msg_pos_x, VIDEO_MESSAGE_POS_X,
      "video_message_pos_x",
      DEFAULT_MESSAGE_POS_OFFSET_X, "%.3f", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 1, 0.01, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Notification Position (Horizontal)",
      "Specify custom X axis position for on-screen text. 0 is left edge.")
S_FLOAT_EX(video_msg_pos_y, VIDEO_MESSAGE_POS_Y,
      "video_message_pos_y",
      DEFAULT_MESSAGE_POS_OFFSET_Y, "%.3f", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 1, 0.01, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Notification Position (Vertical)",
      "Specify custom Y axis position for on-screen text. 0 is bottom edge.")
/* Persistence lives in custom configuration code; no config
 * row is emitted. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_FLOAT_EX(video_msg_color_r, VIDEO_MESSAGE_COLOR_RED,
      "video_msg_color_red",
      ((DEFAULT_MESSAGE_COLOR >> 16) & 0xff) / 255.0f, "%.3f", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 1, 1.0f/255.0f, NULL, setting_get_string_representation_float_video_msg_color, NULL, NULL, NULL, NULL, 0,
      "Notification Color (Red)",
      "Sets the red value of the OSD text color. Valid values are between 0 and 255.")
#endif
/* Persistence lives in custom configuration code; no config
 * row is emitted. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_FLOAT_EX(video_msg_color_g, VIDEO_MESSAGE_COLOR_GREEN,
      "video_msg_color_green",
      ((DEFAULT_MESSAGE_COLOR >> 8) & 0xff) / 255.0f, "%.3f", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 1, 1.0f/255.0f, NULL, setting_get_string_representation_float_video_msg_color, NULL, NULL, NULL, NULL, 0,
      "Notification Color (Green)",
      "Sets the green value of the OSD text color. Valid values are between 0 and 255.")
#endif
/* Persistence lives in custom configuration code; no config
 * row is emitted. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_FLOAT_EX(video_msg_color_b, VIDEO_MESSAGE_COLOR_BLUE,
      "video_msg_color_blue",
      ((DEFAULT_MESSAGE_COLOR >> 0) & 0xff) / 255.0f, "%.3f", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 1, 1.0f/255.0f, NULL, setting_get_string_representation_float_video_msg_color, NULL, NULL, NULL, NULL, 0,
      "Notification Color (Blue)",
      "Sets the blue value of the OSD text color. Valid values are between 0 and 255.")
#endif
S_BOOL_EX(video_msg_bgcolor_enable, VIDEO_MESSAGE_BGCOLOR_ENABLE,
      "video_msg_bgcolor_enable",
      DEFAULT_MESSAGE_BGCOLOR_ENABLE, SD_FLAG_NONE, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Notification Background",
      "Enables a background color for the OSD.")
S_UINT_EX(video_msg_bgcolor_red, VIDEO_MESSAGE_BGCOLOR_RED,
      "video_msg_bgcolor_red",
      DEFAULT_MESSAGE_BGCOLOR_RED, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 255, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, 0,
      "Notification Background Color (Red)",
      "Sets the red value of the OSD background color. Valid values are between 0 and 255.")
S_UINT_EX(video_msg_bgcolor_green, VIDEO_MESSAGE_BGCOLOR_GREEN,
      "video_msg_bgcolor_green",
      DEFAULT_MESSAGE_BGCOLOR_GREEN, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 255, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, 0,
      "Notification Background Color (Green)",
      "Sets the green value of the OSD background color. Valid values are between 0 and 255.")
S_UINT_EX(video_msg_bgcolor_blue, VIDEO_MESSAGE_BGCOLOR_BLUE,
      "video_msg_bgcolor_blue",
      DEFAULT_MESSAGE_BGCOLOR_BLUE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 255, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Notification Background Color (Blue)",
      "Sets the blue value of the OSD background color. Valid values are between 0 and 255.")
S_FLOAT_EX(video_msg_bgcolor_opacity, VIDEO_MESSAGE_BGCOLOR_OPACITY,
      "video_msg_bgcolor_opacity",
      DEFAULT_MESSAGE_BGCOLOR_OPACITY, "%.2f", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 1, 0.01, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Notification Background Opacity",
      "Sets the opacity of the OSD background color. Valid values are between 0.0 and 1.0.")
