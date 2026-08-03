/* Single-source definitions: black frame insertion group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Rows marked _H reserve a MENU_ENUM_LABEL_HELP_ enum member;
 * outside the enum pass they behave exactly like the base row. */
#ifndef SETTINGS_DEF_ENUM_PASS
#ifndef S_UINT_EX_H
#define S_UINT_EX_H S_UINT_EX
#endif
#endif
S_UINT_EX_H(video_black_frame_insertion, VIDEO_BLACK_FRAME_INSERTION,
      "video_black_frame_insertion",
      DEFAULT_BLACK_FRAME_INSERTION, SD_FLAG_CMD_APPLY_AUTO, SDESC_RANGE_MINMAX, CMD_EVENT_REINIT, 0, 15, 1, 0, setting_action_ok_uint, setting_get_string_representation_black_frame_insertion, NULL, NULL, NULL, NULL, 0,
      "Black Frame Insertion",
      "WARNING: Rapid flickering may cause image persistence on some displays. Use at your own risk // Insert black frame(s) between frames. Can greatly reduce motion blur by emulating CRT scan out, but at cost of brightness.")
S_UINT_EX_H(video_bfi_dark_frames, VIDEO_BFI_DARK_FRAMES,
      "video_bfi_dark_frames",
      DEFAULT_BFI_DARK_FRAMES, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 1, 15, 1, 1, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Black Frame Insertion - Dark Frames",
      "Adjust number of black frames in total BFI scan out sequence. More equals higher motion clarity, less equals higher brightness. Not applicable at 120hz as there is only 1 BFI frame to work with total. Settings higher than possible will limit you to the maximum possible for your chosen refresh rate.")
