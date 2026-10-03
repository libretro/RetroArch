/* Single-source definitions: menu frame rate setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_UINT_EX(menu_frame_rate, MENU_FRAME_RATE,
      "menu_frame_rate",
      DEFAULT_MENU_FRAME_RATE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, MENU_FRAME_RATE_DISPLAY, MENU_FRAME_RATE_CONTENT, 1.0, 0, setting_action_ok_uint, setting_get_string_representation_uint_menu_frame_rate, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Menu Frame Rate",
      "The rate the menu runs at while content is loaded. 'Display Rate' runs the menu at the display's refresh rate; content left running behind the menu keeps its own rate. 'Content Rate' runs the menu at the content's frame rate.")
