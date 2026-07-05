/* Single-source definitions: thumbnail background setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(menu_thumbnail_background_enable, MENU_THUMBNAIL_BACKGROUND_ENABLE,
      "menu_thumbnail_background_enable",
      DEFAULT_MENU_THUMBNAIL_BACKGROUND_ENABLE, SD_FLAG_NONE, 0, 0,
      "Thumbnail Backgrounds",
      "Enables padding of unused space in thumbnail images with a solid background. This ensures a uniform display size for all images, improving menu appearance when viewing mixed content thumbnails with varying base dimensions.")
