/* Single-source definitions: camera permission setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(camera_allow, CAMERA_ALLOW,
      "camera_allow",
      false, SD_FLAG_NONE, 0, 0,
      "Allow Camera",
      "Allow cores to access the camera.")
