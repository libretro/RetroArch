/* Single-source definitions: Wii U DRC preference setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

#ifdef WIIU
S_BOOL(video_wiiu_prefer_drc, VIDEO_WIIU_PREFER_DRC,
      "video_wiiu_prefer_drc",
      DEFAULT_WIIU_PREFER_DRC, SD_FLAG_NONE, 0, 0,
      "Optimize for Wii U GamePad (Restart required)",
      "Use an exact 2x scale of the GamePad as the viewport. Disable to display at the native TV resolution.")
#endif
