/* Single-source definitions: screen brightness setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_UINT_EX(screen_brightness, BRIGHTNESS_CONTROL,
      "screen_brightness",
      DEFAULT_SCREEN_BRIGHTNESS, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 5, 100, 5, 0, setting_action_ok_uint_special, setting_get_string_representation_percentage, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Screen Brightness",
      "Increase or decrease the screen brightness.")
