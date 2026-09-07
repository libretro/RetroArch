/* Single-source definitions: HDR toggle group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(video_hdr_scanlines, VIDEO_HDR_SCANLINES,
      "video_hdr_scanlines",
      DEFAULT_VIDEO_HDR_SCANLINES, SD_FLAG_NONE, SDESC_FLG_REFRESH, CMD_EVENT_VIDEO_APPLY_STATE_CHANGES,
      "Scanlines",
      "Enable HDR scanlines.  Scanlines are the main reason for using HDR in RetroArch as an accurate scanline implementation turns off most of the screen and HDR recovers some of that lost brightness.  If you require more control over your scanlines, look to custom shaders that RetroArch provides.")
S_UINT_EX(video_hdr_subpixel_layout, VIDEO_HDR_SUBPIXEL_LAYOUT,
      "video_hdr_subpixel_layout",
      DEFAULT_VIDEO_HDR_SUBPIXEL_LAYOUT, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 2, 1, 0, setting_action_ok_uint, setting_get_string_representation_video_hdr_subpixel_layout, NULL, NULL, NULL, NULL, 0,
      "Subpixel Layout",
      "Select your displays subpixel layout, this only effects scanlines.  If you have no idea what your displays sub pixel layout is see Rtings.com for your display's 'subpixel layout'")
