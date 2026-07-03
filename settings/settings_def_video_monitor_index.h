/* Single-source definitions: monitor index setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Rows marked _EX carry the extended descriptor arguments
 * (callbacks and ui type); every pass except the menu table
 * drops them and behaves exactly like the base row. */
#ifndef S_INT_EX
#define S_INT_EX(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, stax, selx, lfx, rtx, uix, us, sub) S_INT(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, us, sub)
#endif
/* Descriptor and configuration rows are #ifdef HAVE_VULKAN; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_VULKAN) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "vulkan_gpu_index" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_INT_EX(vulkan_gpu_index, VIDEO_GPU_INDEX,
      "gpu_index",
      0, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 15, 1, 0, setting_action_ok_uint, setting_get_string_representation_int_gpu_index, NULL, NULL, NULL, NULL, 0,
      "GPU Index",
      "Select which graphics card to use.")
#endif
#endif
