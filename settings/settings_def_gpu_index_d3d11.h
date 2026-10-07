/* Single-source definitions: Direct3D 12 GPU index setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Row referencing VIDEO_GPU_INDEX; strings owned by another def file. */
#if !defined(SETTINGS_DEF_STRINGS_PASS) && !defined(SETTINGS_DEF_CONFIG_PASS) && !defined(SETTINGS_DEF_ENUM_PASS)
SDESC_INT_ROW_EX(d3d11_gpu_index, VIDEO_GPU_INDEX,
                     0,
                     SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0,
                     0, 15, 1, 0,
                     setting_action_ok_uint, setting_get_string_representation_int_gpu_index,
                     NULL, NULL, NULL, NULL, 0),
#endif
