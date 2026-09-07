/* Single-source definitions: 3DS bottom LCD setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

#ifdef _3DS
S_BOOL_EX(video_3ds_lcd_bottom, VIDEO_3DS_LCD_BOTTOM,
      "video_3ds_lcd_bottom",
      DEFAULT_VIDEO_3DS_LCD_BOTTOM, SD_FLAG_CMD_APPLY_AUTO, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, NULL, NULL, 0,
      "3DS Bottom Screen",
      "Enable display of status information on bottom screen. Disable to increase battery life and improve performance.")
#endif
