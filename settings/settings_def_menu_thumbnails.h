/* Single-source definitions: menu thumbnail group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* config key "menu_icon_thumbnails" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_UINT_EX(menu_icon_thumbnails, ICON_THUMBNAILS,
      "icon_thumbnails",
      DEFAULT_MENU_ICON_THUMBNAILS_DEFAULT, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, PLAYLIST_THUMBNAIL_MODE_LAST - PLAYLIST_THUMBNAIL_MODE_OFF - 1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_menu_thumbnails, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_RADIO_BUTTONS,
      "Icon Thumbnail",
      "Type of playlist icon thumbnail to display.")
#endif
S_BOOL(menu_xmb_vertical_thumbnails, XMB_VERTICAL_THUMBNAILS,
      "xmb_vertical_thumbnails",
      DEFAULT_XMB_VERTICAL_THUMBNAILS, SD_FLAG_NONE, 0, 0,
      "Thumbnail Vertical Disposition",
      "Display the left thumbnail under the right one, on the right side of the screen.")
