/* Single-source definitions: stereo 3D and screens group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_ACTION(VIDEO_STEREO_SETTINGS,
      "video_stereo_settings",
      "Stereo 3D & Screens",
      "Change how stereo 3D and multiple screens from cores that support them are shown.")
S_UINT_EX(video_stereo_mode, VIDEO_STEREO_MODE,
      "video_stereo_mode",
      0, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 5, 1, 0, setting_action_ok_uint, setting_get_string_representation_video_stereo_mode, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Stereo Mode",
      "How to show both eyes of stereo 3D content. 2D shows the left eye only. Side by Side (Full) suits headset viewer apps and cross-eyed viewing; Side by Side (Half) and Top-Bottom suit 3D TVs.")
S_BOOL(video_stereo_swap_eyes, VIDEO_STEREO_SWAP_EYES,
      "video_stereo_swap_eyes",
      false, SD_FLAG_NONE, 0, 0,
      "Swap Eyes",
      "Exchange the left and right eye images. Use for cross-eyed viewing, or when a display shows the eyes or rows reversed.")
S_UINT_EX(video_screen_layout, VIDEO_SCREEN_LAYOUT,
      "video_screen_layout",
      0, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 1, 1, 0, setting_action_ok_uint, setting_get_string_representation_video_screen_layout, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Screen Layout",
      "How to arrange the screens of multi-screen systems.")
