/* Single-source definitions: temporal upscaler group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_TEMPORAL_UPSCALER;
 * the string tables always carry these rows via the strings pass. */
#if defined(HAVE_TEMPORAL_UPSCALER) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT(video_temporal_upscaler, VIDEO_TEMPORAL_UPSCALER,
      "video_temporal_upscaler",
      DEFAULT_VIDEO_TEMPORAL_UPSCALER, SD_FLAG_CMD_APPLY_AUTO, SDESC_RANGE_MINMAX, CMD_EVENT_REINIT, 0, 3, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_video_temporal_upscaler,
      "Temporal Upscaler",
      "Upscale hardware-rendered content with a temporal upscaler on the Vulkan and Direct3D 12 drivers, from the depth and motion vectors the core exports; cores that export none are shown as they are. Each runs on its vendor's library, not bundled: DLSS needs the DLSS runtime in the DLSS Directory and an NVIDIA driver; FSR the FidelityFX SDK's library in the FSR Directory (FSR 4 on the GPUs that have it, else FSR 3.1); XeSS libxess in the XeSS Directory, on any GPU. See System Information for status.")
S_UINT(video_temporal_upscaler_quality, VIDEO_TEMPORAL_UPSCALER_QUALITY,
      "video_temporal_upscaler_quality",
      DEFAULT_VIDEO_TEMPORAL_UPSCALER_QUALITY, SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 0, 5, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_video_temporal_upscaler_quality,
      "Temporal Upscaler Quality",
      "The upscaler's mode: how much smaller than the screen the core renders. Quality renders at about 2/3 of the output size, Balanced at about 58%, Performance at 1/2, Ultra Performance at 1/3 (each upscaler's own ratios); Native keeps the output size and only anti-aliases. Auto picks Quality, or Balanced above 1440 lines. The core is told the size each frame.")
S_UINT(video_temporal_upscaler_preset, VIDEO_TEMPORAL_UPSCALER_PRESET,
      "video_temporal_upscaler_preset",
      DEFAULT_VIDEO_TEMPORAL_UPSCALER_PRESET, SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 0, 15, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_video_temporal_upscaler_preset,
      "Temporal Upscaler Preset",
      "The DLSS model the upscaler runs (DLSS only; under OptiScaler, whichever upscaler it substitutes). Default lets the runtime pick. A to G are the CNN models of DLSS 2 and 3; K is the transformer model of DLSS 4, the best image at a higher GPU cost, J its sibling with less ghosting and some flicker; the later letters are those of newer runtimes. A letter the loaded runtime does not have falls back to Default. Takes effect on the next frame.")
S_UINT(video_temporal_upscaler_sharpness, VIDEO_TEMPORAL_UPSCALER_SHARPNESS,
      "video_temporal_upscaler_sharpness",
      DEFAULT_VIDEO_TEMPORAL_UPSCALER_SHARPNESS, SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 0, 100, 5, 0, setting_action_ok_uint, setting_get_string_representation_percentage,
      "Temporal Upscaler Sharpness",
      "Sharpening applied by the upscaler to its output, in percent; 0 is off. FSR sharpens with its own pass. DLSS 2.5.1 and newer have no sharpening and ignore it, but OptiScaler applies its RCAS pass at this strength to DLSS and XeSS alike. Takes effect on the next frame.")
S_ACTION(VIDEO_DLSS_NR_SETTINGS,
      "video_dlss_nr_settings",
      "DLSS Neural Rendering",
      "DLSS 5's Neural Rendering model, run over DLSS's output. Needs nvngx_dlssnr.dll in the DLSS Directory and an NVIDIA driver that has it.")
S_BOOL(video_dlss_nr_enable, VIDEO_DLSS_NR_ENABLE,
      "video_dlss_nr_enable",
      DEFAULT_VIDEO_DLSS_NR_ENABLE, SD_FLAG_NONE, SDESC_FLG_REFRESH, CMD_EVENT_NONE,
      "Neural Rendering",
      "Run DLSS 5's Neural Rendering model over the upscaled frame. It reworks lighting and materials with the depth and motion the core exports, and costs a few milliseconds a frame. Takes effect on the next frame.")
S_UINT(video_dlss_nr_preset, VIDEO_DLSS_NR_PRESET,
      "video_dlss_nr_preset",
      DEFAULT_VIDEO_DLSS_NR_PRESET, SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 0, 3, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_video_dlss_nr_preset,
      "Model Preset",
      "The Neural Rendering model to use. Default lets the runtime pick. Not numbered as the Temporal Upscaler Preset is.")
S_UINT(video_dlss_nr_style, VIDEO_DLSS_NR_STYLE,
      "video_dlss_nr_style",
      DEFAULT_VIDEO_DLSS_NR_STYLE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 0, 2, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_video_dlss_nr_style,
      "Style",
      "The model's processing profile. Default is the strongest: more local contrast and deeper lighting, at times oversaturated. Natural does the same work more gently and keeps the tones closer to the core's. Cinematic tones down the shine for a film-like look.")
S_UINT(video_dlss_nr_intensity, VIDEO_DLSS_NR_INTENSITY,
      "video_dlss_nr_intensity",
      DEFAULT_VIDEO_DLSS_NR_INTENSITY, SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 0, 200, 5, 0, setting_action_ok_uint, setting_get_string_representation_percentage,
      "Intensity",
      "The model's overall strength, in percent.")
S_UINT(video_dlss_nr_local_structure, VIDEO_DLSS_NR_LOCAL_STRUCTURE,
      "video_dlss_nr_local_structure",
      DEFAULT_VIDEO_DLSS_NR_LOCAL_STRUCTURE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 0, 200, 5, 0, setting_action_ok_uint, setting_get_string_representation_percentage,
      "Local Structure",
      "How strongly the model works fine detail and surface structure, in percent.")
S_UINT(video_dlss_nr_local_tone, VIDEO_DLSS_NR_LOCAL_TONE,
      "video_dlss_nr_local_tone",
      DEFAULT_VIDEO_DLSS_NR_LOCAL_TONE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 0, 200, 5, 0, setting_action_ok_uint, setting_get_string_representation_percentage,
      "Local Tone",
      "How strongly the model reworks local light and shade, in percent.")
S_INT(video_dlss_nr_skin_structure, VIDEO_DLSS_NR_SKIN_STRUCTURE,
      "video_dlss_nr_skin_structure",
      DEFAULT_VIDEO_DLSS_NR_SKIN_STRUCTURE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, -5, 200, 5, 0, setting_action_ok_uint, setting_get_string_representation_int_video_dlss_nr_skin_structure,
      "Skin Structure",
      "The structure strength on skin, in percent. Follow Local Structure, the model's default, uses Local Structure there too.")
S_BOOL(video_dlss_nr_auto_mask, VIDEO_DLSS_NR_AUTO_MASK,
      "video_dlss_nr_auto_mask",
      DEFAULT_VIDEO_DLSS_NR_AUTO_MASK, SD_FLAG_NONE, 0, CMD_EVENT_NONE,
      "Auto Skin Mask",
      "Let the model find skin itself and treat it apart from the rest of the frame.")
/* config keys "video_dlss_directory" and its siblings are paths; the
 * configuration.c rows stay literal for these settings. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_DIR(directory_dlss, VIDEO_DLSS_DIRECTORY,
      "video_dlss_directory",
      "", DIRECTORY_DEFAULT, SD_FLAG_NONE, 0, directory_action_start_generic,
      "DLSS Directory",
      "The directory the DLSS runtime is loaded from. Default: 'upscalers/dlss' in the RetroArch directory.")
S_DIR(directory_fsr, VIDEO_FSR_DIRECTORY,
      "video_fsr_directory",
      "", DIRECTORY_DEFAULT, SD_FLAG_NONE, 0, directory_action_start_generic,
      "FSR Directory",
      "The directory the FidelityFX SDK's FSR library is loaded from. Default: 'upscalers/fsr' in the RetroArch directory.")
S_DIR(directory_xess, VIDEO_XESS_DIRECTORY,
      "video_xess_directory",
      "", DIRECTORY_DEFAULT, SD_FLAG_NONE, 0, directory_action_start_generic,
      "XeSS Directory",
      "The directory the XeSS library (libxess.dll) is loaded from. Default: 'upscalers/xess' in the RetroArch directory.")
#endif
#endif
