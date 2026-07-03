/* Single-source definitions: video synchronization group.
 *
 * One row per setting, consumed by multi-include expansion.  Each
 * consumer defines S_BOOL / S_UINT / S_INT (fixed arity per kind,
 * strict C89, no variadics) and includes this file:
 *
 *   S_BOOL(field, TOKEN, config_name_string,
 *          default, sd_flags, desc_flags, cmd,
 *          us_display_string, us_sublabel_string)
 *   S_UINT / S_INT additionally carry min, max, step, offset_by,
 *          ok_handler, repr_handler before the two strings.
 *
 * The descriptor argument span matches SDESC_<kind>_ROW exactly.
 * Row order is authoritative for menu display order.  h2json.py
 * parses these rows when building the Crowdin source upload.
 * video_scanline_sync stays outside this file: upstream carries no
 * label-table row for it, so migrating it would change behavior. */

S_BOOL(video_vsync, VIDEO_VSYNC,
      "video_vsync",
      DEFAULT_VSYNC, SD_FLAG_NONE, SDESC_FLG_REFRESH, CMD_EVENT_NONE,
      "Vertical Sync (VSync)",
      "Synchronize the output video of the graphics card to the refresh rate of the screen. Recommended.")
S_UINT(video_swap_interval, VIDEO_SWAP_INTERVAL,
      "video_swap_interval",
      DEFAULT_SWAP_INTERVAL, SD_FLAG_CMD_APPLY_AUTO | SD_FLAG_LAKKA_ADVANCED, SDESC_RANGE_MINMAX, CMD_EVENT_REINIT, 0, 4, 1, 0, setting_action_ok_uint, setting_get_string_representation_video_swap_interval,
      "VSync Swap Interval",
      "Use a custom swap interval for VSync. Effectively reduces monitor refresh rate by the specified factor. 'Auto' sets factor based on core-reported frame rate, providing improved frame pacing when running e.g. 30 fps content on a 60 Hz display or 60 fps content on a 120 Hz display.")
S_UINT(video_shader_subframes, VIDEO_SHADER_SUBFRAMES,
      "video_shader_subframes",
      DEFAULT_SHADER_SUBFRAMES, SD_FLAG_CMD_APPLY_AUTO | SD_FLAG_LAKKA_ADVANCED, SDESC_RANGE_MINMAX, CMD_EVENT_REINIT, 1, 16, 1, 1, setting_action_ok_uint, setting_get_string_representation_shader_subframes,
      "Shader Sub-frames",
      "WARNING: Rapid flickering may cause image persistence on some displays. Use at your own risk // Simulates a basic rolling scanline over multiple sub-frames by dividing the screen up vertically and rendering each part of the screen according to how many sub-frames there are.")
S_BOOL(video_scan_subframes, VIDEO_SCAN_SUBFRAMES,
      "video_scan_subframes",
      DEFAULT_SCAN_SUBFRAMES, SD_FLAG_CMD_APPLY_AUTO, SDESC_FLG_REFRESH, CMD_EVENT_REINIT,
      "Rolling scanline simulation",
      "WARNING: Rapid flickering may cause image persistence on some displays. Use at your own risk // Simulates a basic rolling scanline over multiple sub-frames by dividing the screen up vertically and rendering each part of the screen according to how many sub-frames there are.")
S_UINT(video_max_swapchain_images, VIDEO_MAX_SWAPCHAIN_IMAGES,
      "video_max_swapchain_images",
      DEFAULT_MAX_SWAPCHAIN_IMAGES, SD_FLAG_CMD_APPLY_AUTO, SDESC_RANGE_MINMAX, CMD_EVENT_REINIT, MINIMUM_MAX_SWAPCHAIN_IMAGES, MAXIMUM_MAX_SWAPCHAIN_IMAGES, 1, MINIMUM_MAX_SWAPCHAIN_IMAGES, setting_action_ok_uint, NULL,
      "Max Swapchain Images",
      "Tells the video driver to explicitly use a specified buffering mode.")
S_BOOL(video_waitable_swapchains, VIDEO_WAITABLE_SWAPCHAINS,
      "video_waitable_swapchains",
      DEFAULT_WAITABLE_SWAPCHAINS, SD_FLAG_CMD_APPLY_AUTO, SDESC_FLG_REFRESH, CMD_EVENT_REINIT,
      "Waitable Swapchains",
      "Hard-synchronize the CPU and GPU. Reduces latency at the cost of performance.")
S_INT(video_max_frame_latency, VIDEO_MAX_FRAME_LATENCY,
      "video_max_frame_latency",
      DEFAULT_MAX_FRAME_LATENCY, SD_FLAG_CMD_APPLY_AUTO, SDESC_RANGE_MINMAX, CMD_EVENT_REINIT, -1, MAXIMUM_MAX_FRAME_LATENCY, 1, -1, setting_action_ok_uint, NULL,
      "Max Frame Latency",
      "Tells the video driver to explicitly use a specified buffering mode.")
S_BOOL(video_hard_sync, VIDEO_HARD_SYNC,
      "video_hard_sync",
      DEFAULT_HARD_SYNC, SD_FLAG_NONE, SDESC_FLG_REFRESH, CMD_EVENT_NONE,
      "Hard GPU Sync",
      "Hard-synchronize the CPU and GPU. Reduces latency at the cost of performance.")
S_UINT(video_hard_sync_frames, VIDEO_HARD_SYNC_FRAMES,
      "video_hard_sync_frames",
      DEFAULT_HARD_SYNC_FRAMES, SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, MINIMUM_HARD_SYNC_FRAMES, MAXIMUM_HARD_SYNC_FRAMES, 1, 0, setting_action_ok_uint, NULL,
      "Hard GPU Sync Frames",
      "Set how many frames the CPU can run ahead of the GPU when using 'Hard GPU Sync'.")
