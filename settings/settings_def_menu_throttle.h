/* Single-source definitions: menu throttle setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL_LV_NS(menu_throttle_framerate, MENU_THROTTLE_FRAMERATE, MENU_ENUM_THROTTLE_FRAMERATE,
      "menu_throttle_framerate",
      true, SD_FLAG_ADVANCED, 0, 0,
      "Throttle Menu Framerate")
