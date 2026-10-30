/* Every menu label row as MSG_HASH_LBL_ROW(Id, "str"): the
 * hand-maintained intl/msg_hash_lbl.h rows, then the single-source
 * settings def rows under the strings pass. No include guard: it is
 * expanded once per table object, by msg_hash_lbl_str.h for the blob
 * type and by intl/msg_hash_us.c for the blob and the id array, each
 * defining MSG_HASH_LBL_ROW first. MSG_HASH is left as msg_hash.h
 * defines it. */
#undef MSG_HASH
#define MSG_HASH(Id, str) MSG_HASH_LBL_ROW(Id, str)
#include "msg_hash_lbl.h"
#define SETTINGS_DEF_STRINGS_PASS
#define S_BOOL(f, T, n, d, sd, df, c, us, sub) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_BOOL_NS(f, T, n, d, sd, df, c, us) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_UINT(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, us, sub) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_UINT_NS(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, us) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_INT(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, us, sub) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_INT_NS(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, us) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_FLOAT(f, T, n, d, rnd, sd, df, c, mn, mx, st, ok, rp, us, sub) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_FLOAT_NS(f, T, n, d, rnd, sd, df, c, mn, mx, st, ok, rp, us) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_STRING(f, T, n, d, sd, c, ok, rp, sta, sel, lf, rt, ui, us, sub) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_STRING_NS(f, T, n, d, sd, c, ok, rp, sta, sel, lf, rt, ui, us) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_DIR(f, T, n, d, el, sd, c, sta, us, sub) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_DIR_NS(f, T, n, d, el, sd, c, sta, us) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_STRING_P(f, T, n, d, sd, c, ok, rp, sta, sel, lf, rt, ui, us, sub) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_STRING_P_NS(f, T, n, d, sd, c, ok, rp, sta, sel, lf, rt, ui, us) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_PATH(f, T, n, d, sd, c, vals, rp, ui, us, sub) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_PATH_NS(f, T, n, d, sd, c, vals, rp, ui, us) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_PATH_DS(f, T, n, df2, sd, c, vals, rp, ui, us, sub) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_PATH_DS_NS(f, T, n, df2, sd, c, vals, rp, ui, us) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_ACTION(T, n, us, sub) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_ACTION_NS(T, n, us) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_BOOL_LV(f, T, TV, n, d, sd, df, c, us, sub) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_BOOL_LV_NS(f, T, TV, n, d, sd, df, c, us) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_FLOAT_LV(f, T, TV, n, d, rnd, sd, df, c, us, sub) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_FLOAT_LV_NS(f, T, TV, n, d, rnd, sd, df, c, us) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_STRING_LV(f, T, TV, n, d, sd, c, ok, rp, sta, sel, lf, rt, ui, us, sub) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_STRING_LV_NS(f, T, TV, n, d, sd, c, ok, rp, sta, sel, lf, rt, ui, us) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_ACTION_LV(T, TV, n, sd, ok, rp, c, us, sub) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_ACTION_LV_NS(T, TV, n, sd, ok, rp, c, us) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_INT_AT(offs, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, us, sub) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_INT_AT_NS(offs, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, us) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_UINT_AT_EX(offs, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, sta, sel, lf, rt, ui, us, sub) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_UINT_AT_EX_NS(offs, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, sta, sel, lf, rt, ui, us) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_BOOL_EX(f, T, n, d, sd, df, c, ok, rp, sta, sel, lf, rt, ui, us, sub) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_BOOL_EX_NS(f, T, n, d, sd, df, c, ok, rp, sta, sel, lf, rt, ui, us) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_UINT_EX(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, sta, sel, lf, rt, ui, us, sub) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_UINT_EX_NS(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, sta, sel, lf, rt, ui, us) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_INT_EX(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, sta, sel, lf, rt, ui, us, sub) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_INT_EX_NS(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, sta, sel, lf, rt, ui, us) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_FLOAT_EX(f, T, n, d, rnd, sd, df, c, mn, mx, st, ok, rp, sta, sel, lf, rt, ui, us, sub) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_FLOAT_EX_NS(f, T, n, d, rnd, sd, df, c, mn, mx, st, ok, rp, sta, sel, lf, rt, ui, us) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_ACTION_EX(T, n, sd, ok, rp, c, us, sub) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#define S_ACTION_EX_NS(T, n, sd, ok, rp, c, us) MSG_HASH_LBL_ROW(MENU_ENUM_LABEL_##T, n)
#include "../settings/settings_def_video_fullscreen.h"
#include "../settings/settings_def_video_sync.h"
#include "../settings/settings_def_game_ai.h"
#include "../settings/settings_def_services_actions.h"
#include "../settings/settings_def_video_driver_actions.h"
#include "../settings/settings_def_gpu_index_metal.h"
#include "../settings/settings_def_gpu_index_d3d12.h"
#include "../settings/settings_def_gpu_index_d3d11.h"
#include "../settings/settings_def_gpu_index_d3d10.h"
#include "../settings/settings_def_gpu_index_egl_gl.h"
#include "../settings/settings_def_aspect_ratio.h"
#include "../settings/settings_def_viewport_size.h"
#include "../settings/settings_def_quit_visibility.h"
#include "../settings/settings_def_widget_scale_fullscreen.h"
#include "../settings/settings_def_widget_scale.h"
#include "../settings/settings_def_cheevos_account.h"
#include "../settings/settings_def_menu_show_restart.h"
#include "../settings/settings_def_quit_restart.h"
#include "../settings/settings_def_menu_frame_rate.h"
#include "../settings/settings_def_video_ctx_scaling.h"
#include "../settings/settings_def_input_sensors_extra.h"
#include "../settings/settings_def_netplay_advanced.h"
#include "../settings/settings_def_menu_main_state.h"
#include "../settings/settings_def_playlist_management.h"
#include "../settings/settings_def_menu_privacy.h"
#include "../settings/settings_def_menu_landscape.h"
#include "../settings/settings_def_cloud_sync_general.h"
#include "../settings/settings_def_overlay_appearance.h"
#include "../settings/settings_def_notification_views.h"
#include "../settings/settings_def_ozone_typography.h"
#include "../settings/settings_def_saving.h"
#include "../settings/settings_def_notification_positions.h"
#include "../settings/settings_def_menu_main_lists_4.h"
#include "../settings/settings_def_menu_main_lists_3.h"
#include "../settings/settings_def_ozone_extras.h"
#include "../settings/settings_def_menu_main_lists_2.h"
#include "../settings/settings_def_overlay_mouse.h"
#include "../settings/settings_def_overlay_lightgun.h"
#include "../settings/settings_def_input_turbo_fire.h"
#include "../settings/settings_def_ui_appearance.h"
#include "../settings/settings_def_menu_entry_display.h"
#include "../settings/settings_def_crt_switchres.h"
#include "../settings/settings_def_video_sdl_display_server.h"
#include "../settings/settings_def_audio_state.h"
#include "../settings/settings_def_analog_deadzone.h"
#include "../settings/settings_def_desktop_menu.h"
#include "../settings/settings_def_audio_device.h"
#include "../settings/settings_def_ai_service_options.h"
#include "../settings/settings_def_video_output_misc.h"
#include "../settings/settings_def_netplay_visibility.h"
#include "../settings/settings_def_microphone_general.h"
#include "../settings/settings_def_menu_startup.h"
#include "../settings/settings_def_accessibility.h"
#include "../settings/settings_def_video_refresh_rate.h"
#include "../settings/settings_def_menu_header_footer.h"
#include "../settings/settings_def_logging.h"
#include "../settings/settings_def_frame_throttle_general.h"
#include "../settings/settings_def_refresh_autoswitch.h"
#include "../settings/settings_def_audio_skew.h"
#include "../settings/settings_def_wifi.h"
#include "../settings/settings_def_steam_presence.h"
#include "../settings/settings_def_playlist_sorting.h"
#include "../settings/settings_def_playlist_flags.h"
#include "../settings/settings_def_notification_enable.h"
#include "../settings/settings_def_rgui_particle_effect.h"
#include "../settings/settings_def_menu_savestate_resume.h"
#include "../settings/settings_def_menu_framebuffer_opacity.h"
#include "../settings/settings_def_input_haptics.h"
#include "../settings/settings_def_input_general.h"
#include "../settings/settings_def_ozone_appearance.h"
#include "../settings/settings_def_rgui_appearance.h"
#include "../settings/settings_def_playlist_display.h"
#include "../settings/settings_def_cheevos_general.h"
#include "../settings/settings_def_menu_appearance.h"
#include "../settings/settings_def_menu_visibility.h"
#include "../settings/settings_def_netplay_sync.h"
#include "../settings/settings_def_mic_wasapi.h"
#include "../settings/settings_def_ozone_sidebar.h"
#include "../settings/settings_def_input_bind_timeouts.h"
#include "../settings/settings_def_video_hdr_toggles.h"
#include "../settings/settings_def_video_upscaler.h"
#include "../settings/settings_def_video_stereo.h"
#include "../settings/settings_def_video_headset.h"
#include "../settings/settings_def_video_screens.h"
#include "../settings/settings_def_rewind.h"
#include "../settings/settings_def_playlist_history.h"
#include "../settings/settings_def_menu_scroll.h"
#include "../settings/settings_def_menu_thumbnails.h"
#include "../settings/settings_def_input_turbo.h"
#include "../settings/settings_def_frame_delay.h"
#include "../settings/settings_def_audio_wasapi.h"
#include "../settings/settings_def_audio_resampler_quality.h"
#include "../settings/settings_def_wallpaper_opacity.h"
#include "../settings/settings_def_overlay_opacity.h"
#include "../settings/settings_def_widget_scale_windowed.h"
#include "../settings/settings_def_video_hdr.h"
#include "../settings/settings_def_video_window_offset.h"
#include "../settings/settings_def_vulkan_gpu_index.h"
#include "../settings/settings_def_smb_client_auth.h"
#include "../settings/settings_def_midi_volume.h"
#include "../settings/settings_def_netplay_ports.h"
#include "../settings/settings_def_rgui_thumbnail_downscale.h"
#include "../settings/settings_def_thumbnail_upscale.h"
#include "../settings/settings_def_xmb_color_theme.h"
#include "../settings/settings_def_xmb_shader_pipeline.h"
#include "../settings/settings_def_xmb_animations.h"
#include "../settings/settings_def_rgui_color_theme.h"
#include "../settings/settings_def_rgui_aspect.h"
#include "../settings/settings_def_record_threads.h"
#include "../settings/settings_def_stream_quality.h"
#include "../settings/settings_def_record_quality.h"
#include "../settings/settings_def_input_touch_scale.h"
#include "../settings/settings_def_input_mouse_scale.h"
#include "../settings/settings_def_black_frame_insertion.h"
#include "../settings/settings_def_shader_delay.h"
#include "../settings/settings_def_shader_simulated_resolution.h"
#include "../settings/settings_def_screen_brightness.h"
#include "../settings/settings_def_video_rotation.h"
#include "../settings/settings_def_video_monitor_index.h"
#include "../settings/settings_def_rewind_step.h"
#include "../settings/settings_def_frame_throttle_slowmotion.h"
#include "../settings/settings_def_frame_throttle_fastforward.h"
#include "../settings/settings_def_dingux_rs90.h"
#include "../settings/settings_def_dingux_refresh_rate.h"
#include "../settings/settings_def_dingux_ipu.h"
#include "../settings/settings_def_ai_service.h"
#include "../settings/settings_def_video_window_custom_size.h"
#include "../settings/settings_def_video_window_save_position.h"
#include "../settings/settings_def_user_language_action.h"
#include "../settings/settings_def_3ds_bottom_lcd.h"
#include "../settings/settings_def_overlay_auto_scale.h"
#include "../settings/settings_def_notification_widgets.h"
#include "../settings/settings_def_smb_client.h"
#include "../settings/settings_def_netplay_stateless.h"
#include "../settings/settings_def_audio_format.h"
#include "../settings/settings_def_menu_main_actions_12.h"
#include "../settings/settings_def_menu_main_actions_11.h"
#include "../settings/settings_def_menu_main_actions_10.h"
#include "../settings/settings_def_audio_asio_action.h"
#include "../settings/settings_def_netplay_nat.h"
#include "../settings/settings_def_menu_ex_pilot.h"
#include "../settings/settings_def_accounts_streaming.h"
#include "../settings/settings_def_accounts_cheevos.h"
#include "../settings/settings_def_power_action.h"
#include "../settings/settings_def_netplay_action.h"
#include "../settings/settings_def_video_actions_5.h"
#include "../settings/settings_def_video_actions_3.h"
#include "../settings/settings_def_input_actions.h"
#include "../settings/settings_def_video_actions_2.h"
#include "../settings/settings_def_video_actions_1.h"
#include "../settings/settings_def_saving_actions.h"
#include "../settings/settings_def_menu_main_actions_9.h"
#include "../settings/settings_def_menu_main_actions_8.h"
#include "../settings/settings_def_menu_main_actions_7.h"
#include "../settings/settings_def_menu_main_actions_6.h"
#include "../settings/settings_def_menu_main_actions_5.h"
#include "../settings/settings_def_menu_main_actions_4.h"
#include "../settings/settings_def_menu_main_actions_2.h"
#include "../settings/settings_def_menu_main_actions_3.h"
#include "../settings/settings_def_menu_main_actions_1.h"
#include "../settings/settings_def_user_identity.h"
#include "../settings/settings_def_netplay_passwords.h"
#include "../settings/settings_def_netplay_server.h"
#include "../settings/settings_def_recording_paths.h"
#include "../settings/settings_def_streaming_paths.h"
#include "../settings/settings_def_settings_password.h"
#include "../settings/settings_def_kiosk_password.h"
#include "../settings/settings_def_ozone_font_path.h"
#include "../settings/settings_def_xmb_font_path.h"
#include "../settings/settings_def_rgui_theme_path.h"
#include "../settings/settings_def_overlay_preset_path.h"
#include "../settings/settings_def_notification_font_path.h"
#include "../settings/settings_def_menu_wallpaper_path.h"
#include "../settings/settings_def_audio_dsp_path.h"
#include "../settings/settings_def_video_filter_path.h"
#include "../settings/settings_def_dir_user.h"
#include "../settings/settings_def_dir_core.h"
#include "../settings/settings_def_dir_cache_log.h"
#include "../settings/settings_def_cloud_sync_s3.h"
#include "../settings/settings_def_cloud_sync_webdav.h"
#include "../settings/settings_def_microphone_block.h"
#include "../settings/settings_def_updater_backup.h"
#include "../settings/settings_def_updater_experimental.h"
#include "../settings/settings_def_updater_extract.h"
#include "../settings/settings_def_gamemode.h"
#include "../settings/settings_def_sustained_performance.h"
#include "../settings/settings_def_power_plan.h"
#include "../settings/settings_def_quick_menu_shaders_view.h"
#include "../settings/settings_def_menu_online_updater_view.h"
#include "../settings/settings_def_settings_show_smb.h"
#include "../settings/settings_def_settings_show_steam.h"
#include "../settings/settings_def_network_ondemand_thumbnails.h"
#include "../settings/settings_def_network_stdin_cmd.h"
#include "../settings/settings_def_privacy_discord.h"
#include "../settings/settings_def_privacy_location.h"
#include "../settings/settings_def_privacy_camera.h"
#include "../settings/settings_def_menu_core_updater_view.h"
#include "../settings/settings_def_menu_restart_view.h"
#include "../settings/settings_def_menu_thumbnail_background.h"
#include "../settings/settings_def_menu_start_screen.h"
#include "../settings/settings_def_menu_content_settings_view.h"
#include "../settings/settings_def_menu_threaded_data.h"
#include "../settings/settings_def_menu_wraparound.h"
#include "../settings/settings_def_menu_horizontal_animation.h"
#include "../settings/settings_def_menu_show_full_paths.h"
#include "../settings/settings_def_menu_rgui_transparency.h"
#include "../settings/settings_def_menu_wallpaper.h"
#include "../settings/settings_def_runahead_warnings.h"
#include "../settings/settings_def_input_auto_mouse_grab.h"
#include "../settings/settings_def_input_nowinkey.h"
#include "../settings/settings_def_overlay_enable.h"
#include "../settings/settings_def_microphone.h"
#include "../settings/settings_def_audio_sync.h"
#include "../settings/settings_def_audio_enable.h"
#include "../settings/settings_def_ui_menubar.h"
#include "../settings/settings_def_video_window_decorations.h"
#include "../settings/settings_def_video_srgb.h"
#include "../settings/settings_def_video_wiiu_drc.h"
#include "../settings/settings_def_shader_watch.h"
#include "../settings/settings_def_video_dingux_ipu.h"
#include "../settings/settings_def_video_notch.h"
#include "../settings/settings_def_video_console_screen.h"
#include "../settings/settings_def_menu_steam.h"
#include "../settings/settings_def_input_android_workaround.h"
#include "../settings/settings_def_video_frame_time_sample.h"
#include "../settings/settings_def_video_adaptive_vsync.h"
#include "../settings/settings_def_video_gl_direct_spirv.h"
#include "../settings/settings_def_shader_compiler.h"
#include "../settings/settings_def_video_smooth.h"
#include "../settings/settings_def_frame_time_counter.h"
#include "../settings/settings_def_menu_filebrowser.h"
#include "../settings/settings_def_video_filter_rotation.h"
#include "../settings/settings_def_cheevos.h"
#include "../settings/settings_def_video_suspend_screensaver.h"
#include "../settings/settings_def_cheevos_visibility.h"
#include "../settings/settings_def_ui_focus.h"
#include "../settings/settings_def_multimedia.h"
#include "../settings/settings_def_menu_rgui_thumbnails.h"
#include "../settings/settings_def_menu_power_views.h"
#include "../settings/settings_def_recording_video.h"
#include "../settings/settings_def_input_backtouch.h"
#include "../settings/settings_def_menu_rgui_layout.h"
#include "../settings/settings_def_cheats_apply.h"
#include "../settings/settings_def_shader_preset.h"
#include "../settings/settings_def_input_vmouse.h"
#include "../settings/settings_def_menu_desktop.h"
#include "../settings/settings_def_video_gamecube.h"
#include "../settings/settings_def_menu_sounds.h"
#include "../settings/settings_def_menu_main_views.h"
#include "../settings/settings_def_menu_quick_views.h"
#include "../settings/settings_def_menu_settings_views.h"
#include "../settings/settings_def_video_bias.h"
#include "../settings/settings_def_video_window.h"
#undef S_BOOL
#undef S_BOOL_NS
#undef S_BOOL_H
#undef S_BOOL_NS_H
#undef S_UINT
#undef S_UINT_NS
#undef S_UINT_H
#undef S_UINT_NS_H
#undef S_INT
#undef S_INT_NS
#undef S_INT_H
#undef S_INT_NS_H
#undef S_FLOAT
#undef S_FLOAT_NS
#undef S_FLOAT_H
#undef S_FLOAT_NS_H
#undef S_STRING
#undef S_STRING_NS
#undef S_STRING_H
#undef S_STRING_NS_H
#undef S_DIR
#undef S_DIR_NS
#undef S_DIR_H
#undef S_DIR_NS_H
#undef S_STRING_P
#undef S_STRING_P_NS
#undef S_STRING_P_H
#undef S_STRING_P_NS_H
#undef S_PATH
#undef S_PATH_NS
#undef S_PATH_H
#undef S_PATH_NS_H
#undef S_PATH_DS
#undef S_PATH_DS_NS
#undef S_PATH_DS_H
#undef S_PATH_DS_NS_H
#undef S_ACTION
#undef S_ACTION_NS
#undef S_ACTION_H
#undef S_ACTION_NS_H
#undef S_BOOL_EX
#undef S_BOOL_EX_NS
#undef S_BOOL_EX_H
#undef S_BOOL_EX_NS_H
#undef S_UINT_EX
#undef S_UINT_EX_NS
#undef S_UINT_EX_H
#undef S_UINT_EX_NS_H
#undef S_INT_EX
#undef S_INT_EX_NS
#undef S_INT_EX_H
#undef S_INT_EX_NS_H
#undef S_FLOAT_EX
#undef S_FLOAT_EX_NS
#undef S_FLOAT_EX_H
#undef S_FLOAT_EX_NS_H
#undef S_ACTION_EX
#undef S_ACTION_EX_NS
#undef S_ACTION_EX_H
#undef S_ACTION_EX_NS_H
#undef S_BOOL_LV
#undef S_BOOL_LV_NS
#undef S_BOOL_LV_H
#undef S_BOOL_LV_NS_H
#undef S_FLOAT_LV
#undef S_FLOAT_LV_NS
#undef S_FLOAT_LV_H
#undef S_FLOAT_LV_NS_H
#undef S_STRING_LV
#undef S_STRING_LV_NS
#undef S_STRING_LV_H
#undef S_STRING_LV_NS_H
#undef S_ACTION_LV
#undef S_ACTION_LV_NS
#undef S_ACTION_LV_H
#undef S_ACTION_LV_NS_H
#undef S_INT_AT
#undef S_INT_AT_NS
#undef S_INT_AT_H
#undef S_INT_AT_NS_H
#undef S_UINT_AT_EX
#undef S_UINT_AT_EX_NS
#undef S_UINT_AT_EX_H
#undef S_UINT_AT_EX_NS_H
#undef SETTINGS_DEF_STRINGS_PASS
#undef MSG_HASH
#define MSG_HASH(Id, str) case Id: return str;
