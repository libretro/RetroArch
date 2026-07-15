/* Single-source definitions: RGUI thumbnail group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(menu_rgui_inline_thumbnails, MENU_RGUI_INLINE_THUMBNAILS,
      "rgui_inline_thumbnails",
      DEFAULT_RGUI_INLINE_THUMBNAILS, SD_FLAG_NONE, 0, 0,
      "Show Playlist Thumbnails",
      "Enable display of inline downscaled thumbnails while viewing playlists. Toggleable with RetroPad Select. When disabled, thumbnails can still be toggled fullscreen with RetroPad Start.")
S_BOOL(menu_rgui_swap_thumbnails, MENU_RGUI_SWAP_THUMBNAILS,
      "rgui_swap_thumbnails",
      DEFAULT_RGUI_SWAP_THUMBNAILS, SD_FLAG_NONE, 0, 0,
      "Swap Thumbnails",
      "Swaps the display positions of 'Top Thumbnail' and 'Bottom Thumbnail'.")
