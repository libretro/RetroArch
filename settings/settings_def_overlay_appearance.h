/* Single-source definitions: overlay appearance group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_FLOAT_EX(input_overlay_scale_landscape, OVERLAY_SCALE_LANDSCAPE,
      "input_overlay_scale_landscape",
      DEFAULT_INPUT_OVERLAY_SCALE_LANDSCAPE, "%.3f", SD_FLAG_CMD_APPLY_AUTO, SDESC_RANGE_MINMAX, CMD_EVENT_OVERLAY_SET_SCALE_FACTOR, 0.0f, 2.0f, 0.005, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "(Landscape) Overlay Scale",
      "Scale of all UI elements of the overlay when using landscape display orientations.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_FLOAT_EX(input_overlay_aspect_adjust_landscape, OVERLAY_ASPECT_ADJUST_LANDSCAPE,
      "input_overlay_aspect_adjust_landscape",
      DEFAULT_INPUT_OVERLAY_ASPECT_ADJUST_LANDSCAPE, "%.3f", SD_FLAG_CMD_APPLY_AUTO, SDESC_RANGE_MINMAX, CMD_EVENT_OVERLAY_SET_SCALE_FACTOR, -2.0f, 2.0f, 0.005f, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "(Landscape) Overlay Aspect Adjustment",
      "Apply an aspect ratio correction factor to the overlay when using landscape display orientations. Positive values increase (while negative values decrease) effective overlay width.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_FLOAT_EX(input_overlay_x_separation_landscape, OVERLAY_X_SEPARATION_LANDSCAPE,
      "input_overlay_x_separation_landscape",
      DEFAULT_INPUT_OVERLAY_X_SEPARATION_LANDSCAPE, "%.3f", SD_FLAG_CMD_APPLY_AUTO, SDESC_RANGE_MINMAX, CMD_EVENT_OVERLAY_SET_SCALE_FACTOR, -2.0f, 2.0f, 0.005f, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "(Landscape) Overlay Horizontal Separation",
      "If supported by current preset, adjust the spacing between UI elements in the left and right halves of an overlay when using landscape display orientations. Positive values increase (while negative values decrease) the separation of the two halves.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_FLOAT_EX(input_overlay_y_separation_landscape, OVERLAY_Y_SEPARATION_LANDSCAPE,
      "input_overlay_y_separation_landscape",
      DEFAULT_INPUT_OVERLAY_Y_SEPARATION_LANDSCAPE, "%.3f", SD_FLAG_CMD_APPLY_AUTO, SDESC_RANGE_MINMAX, CMD_EVENT_OVERLAY_SET_SCALE_FACTOR, -2.0f, 2.0f, 0.005f, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "(Landscape) Overlay Vertical Separation",
      "If supported by current preset, adjust the spacing between UI elements in the top and bottom halves of an overlay when using landscape display orientations. Positive values increase (while negative values decrease) the separation of the two halves.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_FLOAT_EX(input_overlay_x_offset_landscape, OVERLAY_X_OFFSET_LANDSCAPE,
      "input_overlay_x_offset_landscape",
      DEFAULT_INPUT_OVERLAY_X_OFFSET_LANDSCAPE, "%.3f", SD_FLAG_CMD_APPLY_AUTO, SDESC_RANGE_MINMAX, CMD_EVENT_OVERLAY_SET_SCALE_FACTOR, -1.0f, 1.0f, 0.005f, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "(Landscape) Overlay X Offset",
      "Horizontal overlay offset when using landscape display orientations. Positive values shift overlay to the right; negative values to the left.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_FLOAT_EX(input_overlay_y_offset_landscape, OVERLAY_Y_OFFSET_LANDSCAPE,
      "input_overlay_y_offset_landscape",
      DEFAULT_INPUT_OVERLAY_Y_OFFSET_LANDSCAPE, "%.3f", SD_FLAG_CMD_APPLY_AUTO, SDESC_RANGE_MINMAX, CMD_EVENT_OVERLAY_SET_SCALE_FACTOR, -1.0f, 1.0f, 0.005f, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "(Landscape) Overlay Y Offset",
      "Vertical overlay offset when using landscape display orientations. Positive values shift overlay upwards; negative values downwards.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_FLOAT_EX(input_overlay_scale_portrait, OVERLAY_SCALE_PORTRAIT,
      "input_overlay_scale_portrait",
      DEFAULT_INPUT_OVERLAY_SCALE_PORTRAIT, "%.3f", SD_FLAG_CMD_APPLY_AUTO, SDESC_RANGE_MINMAX, CMD_EVENT_OVERLAY_SET_SCALE_FACTOR, 0.0f, 2.0f, 0.005, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "(Portrait) Overlay Scale",
      "Scale of all UI elements of the overlay when using portrait display orientations.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_FLOAT_EX(input_overlay_aspect_adjust_portrait, OVERLAY_ASPECT_ADJUST_PORTRAIT,
      "input_overlay_aspect_adjust_portrait",
      DEFAULT_INPUT_OVERLAY_ASPECT_ADJUST_PORTRAIT, "%.3f", SD_FLAG_CMD_APPLY_AUTO, SDESC_RANGE_MINMAX, CMD_EVENT_OVERLAY_SET_SCALE_FACTOR, -2.0f, 2.0f, 0.005f, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "(Portrait) Overlay Aspect Adjustment",
      "Apply an aspect ratio correction factor to the overlay when using portrait display orientations. Positive values increase (while negative values decrease) effective overlay height.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_FLOAT_EX(input_overlay_x_separation_portrait, OVERLAY_X_SEPARATION_PORTRAIT,
      "input_overlay_x_separation_portrait",
      DEFAULT_INPUT_OVERLAY_X_SEPARATION_PORTRAIT, "%.3f", SD_FLAG_CMD_APPLY_AUTO, SDESC_RANGE_MINMAX, CMD_EVENT_OVERLAY_SET_SCALE_FACTOR, -2.0f, 2.0f, 0.005f, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "(Portrait) Overlay Horizontal Separation",
      "If supported by current preset, adjust the spacing between UI elements in the left and right halves of an overlay when using portrait display orientations. Positive values increase (while negative values decrease) the separation of the two halves.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_FLOAT_EX(input_overlay_y_separation_portrait, OVERLAY_Y_SEPARATION_PORTRAIT,
      "input_overlay_y_separation_portrait",
      DEFAULT_INPUT_OVERLAY_Y_SEPARATION_PORTRAIT, "%.3f", SD_FLAG_CMD_APPLY_AUTO, SDESC_RANGE_MINMAX, CMD_EVENT_OVERLAY_SET_SCALE_FACTOR, -2.0f, 2.0f, 0.005f, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "(Portrait) Overlay Vertical Separation",
      "If supported by current preset, adjust the spacing between UI elements in the top and bottom halves of an overlay when using portrait display orientations. Positive values increase (while negative values decrease) the separation of the two halves.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_FLOAT_EX(input_overlay_x_offset_portrait, OVERLAY_X_OFFSET_PORTRAIT,
      "input_overlay_x_offset_portrait",
      DEFAULT_INPUT_OVERLAY_X_OFFSET_PORTRAIT, "%.3f", SD_FLAG_CMD_APPLY_AUTO, SDESC_RANGE_MINMAX, CMD_EVENT_OVERLAY_SET_SCALE_FACTOR, -1.0f, 1.0f, 0.005f, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "(Portrait) Overlay X Offset",
      "Horizontal overlay offset when using portrait display orientations. Positive values shift overlay to the right; negative values to the left.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_FLOAT_EX(input_overlay_y_offset_portrait, OVERLAY_Y_OFFSET_PORTRAIT,
      "input_overlay_y_offset_portrait",
      DEFAULT_INPUT_OVERLAY_Y_OFFSET_PORTRAIT, "%.3f", SD_FLAG_CMD_APPLY_AUTO, SDESC_RANGE_MINMAX, CMD_EVENT_OVERLAY_SET_SCALE_FACTOR, -1.0f, 1.0f, 0.005f, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "(Portrait) Overlay Y Offset",
      "Vertical overlay offset when using portrait display orientations. Positive values shift overlay upwards; negative values downwards.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL_EX(input_overlay_pointer_enable, INPUT_OVERLAY_POINTER_ENABLE,
      "input_overlay_pointer_enable",
      DEFAULT_INPUT_OVERLAY_POINTER_ENABLE, SD_FLAG_NONE, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Enable Overlay Lightgun, Mouse, and Pointer",
      "Use any touch inputs not pressing overlay controls to create pointing device input for the core.")
#endif
