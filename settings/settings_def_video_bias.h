/* Single-source definitions: viewport bias group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_FLOAT(video_vp_bias_x, VIDEO_VIEWPORT_BIAS_X,
      "video_viewport_bias_x",
      DEFAULT_VIEWPORT_BIAS_X, "%.2f", SD_FLAG_ALLOW_INPUT | SD_FLAG_LAKKA_ADVANCED, SDESC_RANGE_MINMAX, CMD_EVENT_VIDEO_APPLY_STATE_CHANGES, 0.0, 1.0, 0.05, NULL, NULL,
      "Viewport Anchor Bias X",
      "Horizontal position of content when viewport is wider than content width. 0.0 is far left, 0.5 is center, 1.0 is far right.")
S_FLOAT(video_vp_bias_y, VIDEO_VIEWPORT_BIAS_Y,
      "video_viewport_bias_y",
      DEFAULT_VIEWPORT_BIAS_Y, "%.2f", SD_FLAG_ALLOW_INPUT | SD_FLAG_LAKKA_ADVANCED, SDESC_RANGE_MINMAX, CMD_EVENT_VIDEO_APPLY_STATE_CHANGES, 0.0, 1.0, 0.05, NULL, NULL,
      "Viewport Anchor Bias Y",
      "Vertical position of content when viewport is taller than content height. 0.0 is top, 0.5 is center, 1.0 is bottom.")
#if defined(RARCH_MOBILE)
S_FLOAT(video_vp_bias_portrait_x, VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
      "video_viewport_bias_portrait_x",
      DEFAULT_VIEWPORT_BIAS_PORTRAIT_X, "%.2f", SD_FLAG_ALLOW_INPUT | SD_FLAG_LAKKA_ADVANCED, SDESC_RANGE_MINMAX, CMD_EVENT_VIDEO_APPLY_STATE_CHANGES, 0.0, 1.0, 0.05, NULL, NULL,
      "Viewport Anchor Bias X (Portrait Orientation)",
      "Horizontal position of content when viewport is wider than content width. 0.0 is far left, 0.5 is center, 1.0 is far right. (Portrait Orientation)")
#endif
#if defined(RARCH_MOBILE)
S_FLOAT(video_vp_bias_portrait_y, VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
      "video_viewport_bias_portrait_y",
      DEFAULT_VIEWPORT_BIAS_PORTRAIT_Y, "%.2f", SD_FLAG_ALLOW_INPUT | SD_FLAG_LAKKA_ADVANCED, SDESC_RANGE_MINMAX, CMD_EVENT_VIDEO_APPLY_STATE_CHANGES, 0.0, 1.0, 0.05, NULL, NULL,
      "Viewport Anchor Bias Y (Portrait Orientation)",
      "Vertical position of content when viewport is taller than content height. 0.0 is top, 0.5 is center, 1.0 is bottom. (Portrait Orientation)")
#endif

S_FLOAT(video_ios_margin_left, VIDEO_IOS_MARGIN_LEFT,
      "video_ios_margin_left",
      0.0f, "%.1f", SD_FLAG_ALLOW_INPUT, SDESC_RANGE_MINMAX, CMD_EVENT_VIDEO_APPLY_STATE_CHANGES, 0.0f, 25.0f, 0.5f, NULL, NULL,
      "Left Screen Margin (%)",
      "Leave space at the left edge of the game and menu as a percentage of the window on iOS. 0 adds no margin.")

S_FLOAT(video_ios_margin_right, VIDEO_IOS_MARGIN_RIGHT,
      "video_ios_margin_right",
      0.0f, "%.1f", SD_FLAG_ALLOW_INPUT, SDESC_RANGE_MINMAX, CMD_EVENT_VIDEO_APPLY_STATE_CHANGES, 0.0f, 25.0f, 0.5f, NULL, NULL,
      "Right Screen Margin (%)",
      "Leave space at the right edge of the game and menu as a percentage of the window on iOS. 0 adds no margin.")

S_FLOAT(video_ios_margin_top, VIDEO_IOS_MARGIN_TOP,
      "video_ios_margin_top",
      0.0f, "%.1f", SD_FLAG_ALLOW_INPUT, SDESC_RANGE_MINMAX, CMD_EVENT_VIDEO_APPLY_STATE_CHANGES, 0.0f, 25.0f, 0.5f, NULL, NULL,
      "Top Screen Margin (%)",
      "Leave space at the top edge of the game and menu as a percentage of the window on iOS. 0 adds no margin.")

S_FLOAT(video_ios_margin_bottom, VIDEO_IOS_MARGIN_BOTTOM,
      "video_ios_margin_bottom",
      0.0f, "%.1f", SD_FLAG_ALLOW_INPUT, SDESC_RANGE_MINMAX, CMD_EVENT_VIDEO_APPLY_STATE_CHANGES, 0.0f, 25.0f, 0.5f, NULL, NULL,
      "Bottom Screen Margin (%)",
      "Leave space at the bottom edge of the game and menu as a percentage of the window on iOS. 0 adds no margin.")
