/* Single-source definitions: temporal upscaler group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_NGX; the string
 * tables always carry these rows via the strings pass. */
#if defined(HAVE_NGX) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT(video_temporal_upscaler, VIDEO_TEMPORAL_UPSCALER,
      "video_temporal_upscaler",
      DEFAULT_VIDEO_TEMPORAL_UPSCALER, SD_FLAG_CMD_APPLY_AUTO, SDESC_RANGE_MINMAX, CMD_EVENT_REINIT, 0, 1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_video_temporal_upscaler,
      "Temporal Upscaler",
      "Upscale hardware-rendered content with a temporal upscaler on the Vulkan and Direct3D 12 drivers, from the depth and motion vectors the core exports; cores that export none are shown as they are. NVIDIA DLSS Super Resolution needs the official DLSS runtime (nvngx_dlss.dll on Windows, libnvidia-ngx-dlss.so on Linux) in the DLSS Directory and an NVIDIA driver with NGX; nothing is bundled. Offered only when both are found. See Statistics/System Information for status.")
S_UINT(video_temporal_upscaler_quality, VIDEO_TEMPORAL_UPSCALER_QUALITY,
      "video_temporal_upscaler_quality",
      DEFAULT_VIDEO_TEMPORAL_UPSCALER_QUALITY, SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 0, 5, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_video_temporal_upscaler_quality,
      "Temporal Upscaler Quality",
      "The upscaler's mode: how much smaller than the screen the core renders. Quality renders at 2/3 of the output size, Balanced at 58%, Performance at 1/2, Ultra Performance at 1/3; Native keeps the output size and only anti-aliases (DLAA). Auto picks Quality, or Balanced above 1440 lines. The core is told the size each frame.")
/* config key "video_dlss_directory" is a path; the configuration.c row
 * stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_DIR(directory_dlss, VIDEO_DLSS_DIRECTORY,
      "video_dlss_directory",
      "", DIRECTORY_DEFAULT, SD_FLAG_NONE, 0, directory_action_start_generic,
      "DLSS Directory",
      "The directory the DLSS runtime is loaded from. Default: 'upscalers/dlss' in the RetroArch directory.")
#endif
#endif
