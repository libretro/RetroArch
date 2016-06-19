/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MENU_HASH_H__
#define MENU_HASH_H__

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Callback strings */

#define CB_CORE_UPDATER_DOWNLOAD                                               0x7412da7dU
#define CB_UPDATE_ASSETS                                                       0xbf85795eU

/* Deferred */

#define MENU_LABEL_DEFERRED_THUMBNAILS_UPDATER_LIST                            0x364dfa2bU
#define MENU_LABEL_DEFERRED_VIDEO_FILTER                                       0x966ad201U
#define MENU_LABEL_DEFERRED_CORE_LIST_SET                                      0xa6d5fdb4U
#define MENU_LABEL_DEFERRED_DATABASE_MANAGER_LIST                              0x7c0b704fU
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST                                0x45446638U
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_DEVELOPER            0xcbd89be5U
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_PUBLISHER            0x125e594dU
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ORIGIN               0x4ebaa767U
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_FRANCHISE            0x77f9eff2U
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_EDGE_MAGAZINE_RATING 0x1c7f8a43U
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_EDGE_MAGAZINE_ISSUE  0xaaeebde7U
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_FAMITSU_MAGAZINE_RATING 0xbf7ff5e7U
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ENHANCEMENT_HW       0x9866bda3U
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_RELEASEMONTH         0x2b36ce66U
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_RELEASEYEAR          0x9c7c6e91U
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ESRB_RATING          0x68eba20fU
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ELSPA_RATING         0x8bf6ab18U
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_PEGI_RATING          0x5fc77328U
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_CERO_RATING          0x24f6172cU
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_BBFC_RATING          0x0a8e67f0U
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_MAX_USERS            0xbfcba816U
#define MENU_LABEL_DEFERRED_RDB_ENTRY_DETAIL                                   0xc35416c0U
#define MENU_LABEL_DEFERRED_RPL_ENTRY_ACTIONS                                  0x358a7494U
#define MENU_LABEL_DEFERRED_CORE_LIST                                          0xf157d289U
#define MENU_LABEL_DEFERRED_CORE_UPDATER_LIST                                  0xc315f682U
#define MENU_LABEL_DEFERRED_DRIVER_SETTINGS_LIST                               0xaa5efefcU
#define MENU_LABEL_DEFERRED_VIDEO_SETTINGS_LIST                                0x83c65827U
#define MENU_LABEL_DEFERRED_AUDIO_SETTINGS_LIST                                0x5bba25e2U
#define MENU_LABEL_DEFERRED_PLAYLIST_SETTINGS_LIST                             0x9518e0c7U
#define MENU_LABEL_DEFERRED_INPUT_SETTINGS_LIST                                0x050bec60U
#define MENU_LABEL_DEFERRED_USER_BINDS_LIST                                    0x28c5750eU
#define MENU_LABEL_DEFERRED_ACCOUNTS_CHEEVOS_LIST                              0x1322324cU
#define MENU_LABEL_DEFERRED_ACCOUNTS_LIST                                      0x3d2b8860U
#define MENU_LABEL_DEFERRED_ARCHIVE_ACTION_DETECT_CORE                         0xdc9c0064U
#define MENU_LABEL_DEFERRED_ARCHIVE_ACTION                                     0x7faf0284U
#define MENU_LABEL_DEFERRED_ARCHIVE_OPEN_DETECT_CORE                           0xd9452498U
#define MENU_LABEL_DEFERRED_ARCHIVE_OPEN                                       0xfa0938b8U
#define MENU_LABEL_DEFERRED_CORE_CONTENT_LIST                                  0x76150c63U
#define MENU_LABEL_DEFERRED_LAKKA_LIST                                         0x3db437c4U
#define MENU_LABEL_DEFERRED_INPUT_HOTKEY_BINDS_LIST                            0x10b41d97U

/* Cheevos settings */

#define MENU_LABEL_CHEEVOS_DESCRIPTION                                         0x7e00e0f5U

/* Playlist settings */

#define MENU_LABEL_PLAYLIST_SETTINGS_BEGIN                                     0x80a8d2cbU

/* Accounts settings */

#define MENU_LABEL_ACCOUNTS_CHEEVOS_PASSWORD                                   0x45cf62e3U
#define MENU_LABEL_ACCOUNTS_CHEEVOS_USERNAME                                   0x2bf153f0U
#define MENU_LABEL_ACCOUNTS_RETRO_ACHIEVEMENTS                                 0xe6b7c16cU
#define MENU_LABEL_ACCOUNTS_LIST                                               0x774c15a0U

#define MENU_LABEL_VALUE_ACCOUNTS_LIST_END                                     0x3d559522U


#define MENU_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST                            0xb4f82700U

/* Scan values */
#define MENU_LABEL_SCAN_THIS_DIRECTORY                                         0x6921b775U
#define MENU_LABEL_SCAN_DIRECTORY                                              0x57de303eU
#define MENU_LABEL_SCAN_FILE                                                   0xd5d1eee9U

/* Online updater settings */

#define MENU_LABEL_UPDATE_LAKKA                                                0x19b51eebU

/* Information settings */

#define MENU_LABEL_INFORMATION_LIST                                            0x225e7606U
#define MENU_LABEL_SYSTEM_INFORMATION                                          0x206ebf0fU
#define MENU_LABEL_NETWORK_INFORMATION                                         0x73ae3cb4U
#define MENU_LABEL_DEBUG_INFORMATION                                           0xeb0d82b1U

#define MENU_LABEL_CONTENT_SETTINGS                                            0xe789f7f6U

#define MENU_LABEL_RESET                                                       0x10474288U

#define MENU_LABEL_ARCHIVE_MODE                                                0x7fac00cbU
#define MENU_LABEL_USE_THIS_DIRECTORY                                          0xc51d351dU
#define MENU_LABEL_SAVE_STATE                                                  0x3a4849b5U
#define MENU_LABEL_LOAD_STATE                                                  0xa39eb286U
#define MENU_LABEL_REWIND                                                      0x1931d5aeU
#define MENU_LABEL_SHADER_NEXT                                                 0x54d359baU
#define MENU_LABEL_SHADER_PREV                                                 0x54d4a758U
#define MENU_LABEL_FRAME_ADVANCE                                               0xd80302a1U
#define MENU_LABEL_FPS_SHOW                                                    0x5ea1e10eU
#define MENU_LABEL_MOVIE_RECORD_TOGGLE                                         0xa2d2ff04U
#define MENU_LABEL_L_X_PLUS                                                    0xd7370d4bU
#define MENU_LABEL_L_X_MINUS                                                   0xbde0aaf3U
#define MENU_LABEL_L_Y_PLUS                                                    0xd98c35ecU
#define MENU_LABEL_L_Y_MINUS                                                   0x0adae7b4U
#define MENU_LABEL_R_X_PLUS                                                    0x60c20a91U
#define MENU_LABEL_R_X_MINUS                                                   0x78cb50f9U
#define MENU_LABEL_R_Y_MINUS                                                   0xc5c58dbaU
#define MENU_LABEL_R_Y_PLUS                                                    0x63173332U
#define MENU_LABEL_BLOCK_SRAM_OVERWRITE                                        0xc4e88d08U
#define MENU_LABEL_CORE_SPECIFIC_CONFIG                                        0x3c9a55e8U
#define MENU_LABEL_GAME_SPECIFIC_OPTIONS                                       0x142ec90fU
#define MENU_LABEL_AUTO_OVERRIDES_ENABLE                                       0x35ff91b6U
#define MENU_LABEL_AUTO_REMAPS_ENABLE                                          0x98c8f98bU
#define MENU_LABEL_SCREENSHOT                                                  0x9a37f083U
#define MENU_LABEL_SCREEN_RESOLUTION                                           0x5c9b3a58U

/* Menu settings */
#define MENU_LABEL_THUMBNAILS                                                  0x0a3ec67cU
#define MENU_LABEL_SAVESTATE_AUTO_SAVE                                         0xf6f4a05bU
#define MENU_LABEL_SAVESTATE_AUTO_LOAD                                         0xf6f1028cU
#define MENU_LABEL_SAVESTATE_AUTO_INDEX                                        0xd4da8b84U
#define MENU_LABEL_XMB_SCALE_FACTOR                                            0x0177E8DF1
#define MENU_LABEL_XMB_ALPHA_FACTOR                                            0x01049C5CF
#define MENU_LABEL_XMB_FONT                                                    0x0ECA56CA2
#define MENU_LABEL_XMB_THEME                                                   0x824c5a7eU
#define MENU_LABEL_XMB_GRADIENT                                                0x18e63099U
#define MENU_LABEL_XMB_SHADOWS_ENABLE                                          0xd0fcc82aU
#define MENU_LABEL_XMB_RIBBON_ENABLE                                           0x8e89c3edU

#define MENU_LABEL_SSH_ENABLE                                                  0xd9854a79U
#define MENU_LABEL_SAMBA_ENABLE                                                0x379e15efU
#define MENU_LABEL_BLUETOOTH_ENABLE                                            0xbac1e1e1U
#define MENU_LABEL_SORT_SAVEFILES_ENABLE                                       0xed0d0df4U
#define MENU_LABEL_SORT_SAVESTATES_ENABLE                                      0x66ff2495U
#define MENU_LABEL_PERFCNT_ENABLE                                              0x6823dbddU
#define MENU_LABEL_KEYBOARD_OVERLAY_PRESET                                     0x11f1c582U


/* Video settings */
#define MENU_LABEL_VIDEO_FILTER                                                0x1c0eb741U
#define MENU_LABEL_VIDEO_REFRESH_RATE_AUTO                                     0x9addb6cdU
#define MENU_LABEL_VIDEO_REFRESH_RATE                                          0x56ccabf5U
#define MENU_LABEL_VIDEO_WINDOWED_FULLSCREEN                                   0x6436d6f8U
#define MENU_LABEL_VIDEO_FORCE_SRGB_DISABLE                                    0x0a7b68aaU
#define MENU_LABEL_VIDEO_ROTATION                                              0x4ce6882bU
#define MENU_LABEL_VIDEO_SCALE                                                 0x09835d63U
#define MENU_LABEL_VIDEO_SMOOTH                                                0x3aabbb35U
#define MENU_LABEL_VIDEO_CROP_OVERSCAN                                         0x861f7a2fU
#define MENU_LABEL_VIDEO_SCALE_INTEGER                                         0x65c4b090U
#define MENU_LABEL_VIDEO_DISABLE_COMPOSITION                                   0x5cbb6222U
#define MENU_LABEL_VIDEO_VSYNC                                                 0x09c2d34eU
#define MENU_LABEL_VIDEO_BLACK_FRAME_INSERTION                                 0x53477f5cU
#define MENU_LABEL_VIDEO_HARD_SYNC_FRAMES                                      0xce0ece13U
#define MENU_LABEL_VIDEO_FRAME_DELAY                                           0xd4aa9df4U
#define MENU_LABEL_VIDEO_FULLSCREEN                                            0x9506dd4eU
#define MENU_LABEL_VIDEO_SWAP_INTERVAL                                         0x5673ff9aU
#define MENU_LABEL_VIDEO_GPU_SCREENSHOT                                        0xee2fcb44U
#define MENU_LABEL_VIDEO_FONT_ENABLE                                           0x697d9b58U
#define MENU_LABEL_VIDEO_FONT_PATH                                             0xd0de729eU
#define MENU_LABEL_VIDEO_FONT_SIZE                                             0xd0e03a8cU
#define MENU_LABEL_VIDEO_MESSAGE_POS_X                                         0xa133c368U
#define MENU_LABEL_VIDEO_MESSAGE_POS_Y                                         0xa133c369U
#define MENU_LABEL_VIDEO_FILTER_FLICKER                                        0x2e21eba0U
#define MENU_LABEL_VIDEO_SOFT_FILTER                                           0x92819a46U
#define MENU_LABEL_VIDEO_POST_FILTER_RECORD                                    0xa7b6e724U
#define MENU_LABEL_VIDEO_VI_WIDTH                                              0x6e4a6d3aU
#define MENU_LABEL_VIDEO_ASPECT_RATIO_AUTO                                     0xa7c31991U
#define MENU_LABEL_VIDEO_ASPECT_RATIO_INDEX                                    0x3b01a19aU
#define MENU_LABEL_VIDEO_VFILTER                                               0x664f8397U
#define MENU_LABEL_VIDEO_GPU_RECORD                                            0xb6059a65U
#define MENU_LABEL_VIDEO_SHARED_CONTEXT                                        0x7d7dad16U
#define MENU_LABEL_VIDEO_GAMMA                                                 0x08a951beU
#define MENU_LABEL_VIDEO_ALLOW_ROTATE                                          0x2880f0e8U

#define MENU_LABEL_VIDEO_DRIVER                                                0x1805a5e7U
#define MENU_LABEL_VIDEO_DRIVER_GL                                             0x005977f8U
#define MENU_LABEL_VIDEO_DRIVER_SDL2                                           0x7c9dd69aU
#define MENU_LABEL_VIDEO_DRIVER_SDL1                                           0x0b88a968U
#define MENU_LABEL_VIDEO_DRIVER_D3D                                            0x0b886340U
#define MENU_LABEL_VIDEO_DRIVER_EXYNOS                                         0xfc37c54bU
#define MENU_LABEL_VIDEO_DRIVER_SUNXI                                          0x10620e3cU

#define MENU_LABEL_VIDEO_SHADER_PRESET_SAVE_AS                                 0x3d6e5ce5U
#define MENU_LABEL_VIDEO_SHADER_DEFAULT_FILTER                                 0x4468cb1bU
#define MENU_LABEL_VIDEO_SHADER_FILTER_PASS                                    0x1906c38dU
#define MENU_LABEL_VIDEO_SHADER_SCALE_PASS                                     0x18f7b82fU
#define MENU_LABEL_VIDEO_SHADER_NUM_PASSES                                     0x79b2992fU
#define MENU_LABEL_VIDEO_SHADER_PARAMETERS                                     0x9895c3e5U
#define MENU_LABEL_VIDEO_SHADER_PRESET_PARAMETERS                              0xd18158d7U
#define MENU_LABEL_VIDEO_SHADER_PASS                                           0x4fa31028U
#define MENU_LABEL_VIDEO_SHADER_PRESET                                         0xc5d3bae4U

/* Audio settings */
#define MENU_LABEL_AUDIO_MUTE                                                  0xe0ca1151U
#define MENU_LABEL_AUDIO_DSP_PLUGIN                                            0x4a69572bU
#define MENU_LABEL_AUDIO_OUTPUT_RATE                                           0x477b97b9U
#define MENU_LABEL_AUDIO_MAX_TIMING_SKEW                                       0x4c96f75cU

#define MENU_LABEL_AUDIO_RESAMPLER_DRIVER                                      0xedcba9ecU
#define MENU_LABEL_AUDIO_RESAMPLER_DRIVER_SINC                                 0x7c9dec52U
#define MENU_LABEL_AUDIO_RESAMPLER_DRIVER_CC                                   0x0059732bU

/* Netplay settings */
#define MENU_LABEL_NETPLAY_FLIP_PLAYERS                                        0x801425abU
#define MENU_LABEL_NETPLAY_NICKNAME                                            0x52204787U
#define MENU_LABEL_NETPLAY_CLIENT_SWAP_INPUT                                   0xd87bbba9U
#define MENU_LABEL_NETPLAY_DELAY_FRAMES                                        0x86b2c48dU
#define MENU_LABEL_NETPLAY_SPECTATOR_MODE_ENABLE                               0x6f9a9440U
#define MENU_LABEL_NETPLAY_TCP_UDP_PORT                                        0x98407774U
#define MENU_LABEL_NETPLAY_IP_ADDRESS                                          0xac9a53ffU
#define MENU_LABEL_NETPLAY_MODE                                                0xc1cf6506U

/* Input settings */
#define MENU_LABEL_INPUT_OVERLAY                                               0x24e24796U
#define MENU_LABEL_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE                         0x3665cbb0U

#define MENU_LABEL_INPUT_HOTKEY_BINDS_BEGIN                                    0x5a56139bU
#define MENU_LABEL_INPUT_SMALL_KEYBOARD_ENABLE                                 0xe6736fc3U
#define MENU_LABEL_INPUT_ICADE_ENABLE                                          0xcd534dd0U
#define MENU_LABEL_INPUT_HOTKEY_SETTINGS                                       0x1cb39c19U
#define MENU_LABEL_INPUT_AUTODETECT_ENABLE                                     0xb1e07facU
#define MENU_LABEL_INPUT_DUTY_CYCLE                                            0xec787129U
#define MENU_LABEL_INPUT_BIND_DEVICE_ID                                        0xd1ea94ecU
#define MENU_LABEL_INPUT_BIND_DEVICE_TYPE                                      0xf6e9f041U
#define MENU_LABEL_INPUT_DRIVER                                                0x4c087840U
#define MENU_LABEL_INPUT_DRIVER_LINUXRAW                                       0xc33c6b9fU
#define MENU_LABEL_INPUT_DRIVER_UDEV                                           0x7c9eeeb9U
#define MENU_LABEL_INPUT_REMAP_BINDS_ENABLE                                    0x536dcafeU
#define MENU_LABEL_INPUT_AXIS_THRESHOLD                                        0xe95c2095U
#define MENU_LABEL_INPUT_DESCRIPTOR_LABEL_SHOW                                 0x7eefdf52U
#define MENU_LABEL_INPUT_DESCRIPTOR_HIDE_UNBOUND                               0x7051d870U
#define MENU_LABEL_INPUT_BIND_MODE                                             0x90281b55U

#define MENU_LABEL_INPUT_OSK_OVERLAY                                           0x11f1c582U
#define MENU_LABEL_INPUT_OSK_OVERLAY_ENABLE                                    0x7f8339c8U

/* Record settings */

#define MENU_LABEL_RECORD_USE_OUTPUT_DIRECTORY                                 0x8343eff4U
#define MENU_LABEL_RECORD_CONFIG                                               0x11c3daf9U
#define MENU_LABEL_RECORD_PATH                                                 0x016d7afaU
#define MENU_LABEL_RECORD_ENABLE                                               0x1654e22aU

/* Cheat options */

#define MENU_LABEL_CHEAT_DATABASE_PATH                                         0x01388b8aU
#define MENU_LABEL_CHEAT_FILE_LOAD                                             0x57336148U
#define MENU_LABEL_CHEAT_INDEX_MINUS                                           0x57f58b6cU
#define MENU_LABEL_CHEAT_INDEX_PLUS                                            0x678542a4U
#define MENU_LABEL_CHEAT_TOGGLE                                                0xe515e0cbU
#define MENU_LABEL_CHEAT_FILE_SAVE_AS                                          0x1f58dccaU
#define MENU_LABEL_CHEAT_APPLY_CHANGES                                         0xde88aa27U

/* Rewind settings */

#define MENU_LABEL_REWIND_ENABLE                                               0x9761e074U
#define MENU_LABEL_REWIND_GRANULARITY                                          0xe859cbdfU

/* Overlay settings */

#define MENU_LABEL_OVERLAY_AUTOLOAD_PREFERRED                                  0xc9298cbdU
#define MENU_LABEL_OVERLAY_PRESET                                              0x24e24796U
#define MENU_LABEL_OVERLAY_OPACITY                                             0xc466fbaeU
#define MENU_LABEL_OVERLAY_SCALE                                               0x2dce2a3dU
#define MENU_LABEL_OVERLAY_NEXT                                                0x7a459145U

/* Disk settings */

#define MENU_LABEL_DISK_EJECT_TOGGLE                                           0x49633fbbU
#define MENU_LABEL_DISK_NEXT                                                   0xeeaf6c6eU
#define MENU_LABEL_DISK_CYCLE_TRAY_STATUS                                      0x3035cdc1U
#define MENU_LABEL_DISK_INDEX                                                  0x6c14bf54U
#define MENU_LABEL_DISK_OPTIONS                                                0xc61ab5fbU
#define MENU_LABEL_DISK_IMAGE_APPEND                                           0x5af7d709U

/* Menu settings */

#define MENU_LABEL_MENU_TOGGLE                                                 0xfb22e3dbU
#define MENU_LABEL_MENU_WALLPAPER                                              0x3b84de01U
#define MENU_LABEL_MENU_LINEAR_FILTER                                          0x5fe9128cU
#define MENU_LABEL_MENU_THROTTLE_FRAMERATE                                     0x9a8681c5U
#define MENU_LABEL_MENU_SETTINGS                                               0x61e4544bU

#define MENU_LABEL_ENABLE_HOTKEY                                               0xc04037bfU
#define MENU_LABEL_GRAB_MOUSE_TOGGLE                                           0xb2869aaaU
#define MENU_LABEL_STATE_SLOT_DECREASE                                         0xe48b8082U
#define MENU_LABEL_STATE_SLOT_INCREASE                                         0x36a0cbb0U

/* Libretro settings */
#define MENU_LABEL_LIBRETRO_LOG_LEVEL                                          0x57971ac0U
#define MENU_LABEL_LIBRETRO_INFO_PATH                                          0xe552b25fU

#define MENU_LABEL_AUTOSAVE_INTERVAL                                           0xecc87351U
#define MENU_LABEL_CONFIG_SAVE_ON_EXIT                                         0x79b590feU


/* Privacy settings */

#define MENU_LABEL_CAMERA_ALLOW                                                0xc14d302cU
#define MENU_LABEL_LOCATION_ALLOW                                              0xf089275cU

/* Directory settings */

#define MENU_LABEL_CURSOR_DIRECTORY                                            0xdee8d377U
#define MENU_LABEL_OSK_OVERLAY_DIRECTORY                                       0xcce86287U
#define MENU_LABEL_RECORDING_OUTPUT_DIRECTORY                                  0x30bece06U
#define MENU_LABEL_RECORDING_CONFIG_DIRECTORY                                  0x3c3f274bU
#define MENU_LABEL_LIBRETRO_DIR_PATH                                           0x1af1eb72U
#define MENU_LABEL_AUDIO_FILTER_DIR                                            0x4bd96ebaU
#define MENU_LABEL_VIDEO_SHADER_DIR                                            0x30f53b10U
#define MENU_LABEL_VIDEO_FILTER_DIR                                            0x67603f1fU
#define MENU_LABEL_SCREENSHOT_DIRECTORY                                        0x552612d7U
#define MENU_LABEL_SYSTEM_DIRECTORY                                            0x35a6fb9eU
#define MENU_LABEL_INPUT_REMAPPING_DIRECTORY                                   0x5233c20bU
#define MENU_LABEL_OVERLAY_DIRECTORY                                           0xc4ed3d1bU
#define MENU_LABEL_SAVEFILE_DIRECTORY                                          0x92773488U
#define MENU_LABEL_SAVESTATE_DIRECTORY                                         0x90551289U
#define MENU_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY                                0x62f975b8U
#define MENU_LABEL_THUMBNAILS_DIRECTORY                                        0xdea77410U
#define MENU_LABEL_RGUI_BROWSER_DIRECTORY                                      0xa86cba73U
#define MENU_LABEL_CONTENT_DATABASE_DIRECTORY                                  0x6b443f80U
#define MENU_LABEL_PLAYLIST_DIRECTORY                                          0x6361820bU
#define MENU_LABEL_CORE_ASSETS_DIRECTORY                                       0x8ba5ee54U
#define MENU_LABEL_CONTENT_DIRECTORY                                           0x7738dc14U
#define MENU_LABEL_RGUI_CONFIG_DIRECTORY                                       0x0cb3e005U
#define MENU_LABEL_ASSETS_DIRECTORY                                            0xde1ae8ecU
#define MENU_LABEL_CACHE_DIRECTORY                                             0x851dfb8dU

/* RDB settings */

#define MENU_LABEL_NO_PLAYLIST_ENTRIES_AVAILABLE                               0x8888c5acU
#define MENU_LABEL_RDB_ENTRY_ANALOG                                            0x9081c2ffU
#define MENU_LABEL_RDB_ENTRY_RUMBLE                                            0xb8ae8ad4U
#define MENU_LABEL_RDB_ENTRY_COOP                                              0x7c953ff6U
#define MENU_LABEL_RDB_ENTRY_START_CONTENT                                     0x95025a55U
#define MENU_LABEL_RDB_ENTRY_DESCRIPTION                                       0x26aa1f71U
#define MENU_LABEL_RDB_ENTRY_GENRE                                             0x9fefab3eU
#define MENU_LABEL_RDB_ENTRY_NAME                                              0xc6ccf92eU
#define MENU_LABEL_RDB_ENTRY_PUBLISHER                                         0x4d7bcdfbU
#define MENU_LABEL_RDB_ENTRY_DEVELOPER                                         0x06f61093U
#define MENU_LABEL_RDB_ENTRY_ORIGIN                                            0xb176aad5U
#define MENU_LABEL_RDB_ENTRY_FRANCHISE                                         0xb31764a0U
#define MENU_LABEL_RDB_ENTRY_ENHANCEMENT_HW                                    0x79ee4f11U
#define MENU_LABEL_RDB_ENTRY_ESRB_RATING                                       0xe138fa3dU
#define MENU_LABEL_RDB_ENTRY_BBFC_RATING                                       0x82dbc01eU
#define MENU_LABEL_RDB_ENTRY_ELSPA_RATING                                      0x0def0906U
#define MENU_LABEL_RDB_ENTRY_PEGI_RATING                                       0xd814cb56U
#define MENU_LABEL_RDB_ENTRY_CERO_RATING                                       0x9d436f5aU
#define MENU_LABEL_RDB_ENTRY_EDGE_MAGAZINE_RATING                              0x9735f631U
#define MENU_LABEL_RDB_ENTRY_EDGE_MAGAZINE_ISSUE                               0xd5706415U
#define MENU_LABEL_RDB_ENTRY_EDGE_MAGAZINE_REVIEW                              0x977f6fdeU
#define MENU_LABEL_RDB_ENTRY_FAMITSU_MAGAZINE_RATING                           0x01a50315U
#define MENU_LABEL_RDB_ENTRY_TGDB_RATING                                       0x225a9d72U
#define MENU_LABEL_RDB_ENTRY_RELEASE_MONTH                                     0xad2f2c54U
#define MENU_LABEL_RDB_ENTRY_RELEASE_YEAR                                      0x14c9c6bfU
#define MENU_LABEL_RDB_ENTRY_MAX_USERS                                         0xfae91cc4U
#define MENU_LABEL_RDB_ENTRY_SHA1                                              0xc6cfd31aU
#define MENU_LABEL_RDB_ENTRY_MD5                                               0xdf3c7f93U
#define MENU_LABEL_RDB_ENTRY_CRC32                                             0x9fae330aU

#define MENU_LABEL_SYSTEM_BGM_ENABLE                                           0x9287a1c5U

/* Network settings */

#define MENU_LABEL_STDIN_CMD_ENABLE                                            0x665069c0U
#define MENU_LABEL_NETWORK_CMD_ENABLE                                          0xfdf03a08U
#define MENU_LABEL_NETWORK_CMD_PORT                                            0xc1b9e0a6U
#define MENU_LABEL_NETWORK_REMOTE_ENABLE                                       0x99cd4420U
#define MENU_LABEL_NETWORK_REMOTE_PORT                                         0x9aef9e18U

#define MENU_LABEL_DETECT_CORE_LIST                                            0xaa07c341U
#define MENU_LABEL_DETECT_CORE_LIST_OK                                         0xabba2a7aU
#define MENU_LABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE                           0xa3d605f5U
#define MENU_LABEL_CORE_UPDATER_BUILDBOT_URL                                   0xe9ad8448U
#define MENU_LABEL_BUILDBOT_ASSETS_URL                                         0x1895c71eU
#define MENU_LABEL_DUMMY_ON_CORE_SHUTDOWN                                      0x78579f70U
#define MENU_LABEL_NAVIGATION_WRAPAROUND                                       0xe76ad251U
#define MENU_LABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE       0xea48426bU
#define MENU_LABEL_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE                         0x593d2623U
#define MENU_LABEL_CLOSE_CONTENT                                               0x4b622170U
#define MENU_LABEL_SHUTDOWN                                                    0xfc460361U
#define MENU_LABEL_REBOOT                                                      0x19266b70U
#define MENU_LABEL_CORE_LIST                                                   0xa22bb14dU
#define MENU_LABEL_MANAGEMENT                                                  0x2516c88aU
#define MENU_LABEL_FRONTEND_COUNTERS                                           0xe5696877U
#define MENU_LABEL_CORE_COUNTERS                                               0x64cc83e0U
#define MENU_LABEL_ACHIEVEMENT_LIST                                            0x7b90fc49U
#define MENU_LABEL_CORE_INFORMATION                                            0xb638e0d3U
#define MENU_LABEL_CORE_OPTIONS                                                0xf65e60f9U
#define MENU_LABEL_SHADER_OPTIONS                                              0x1f7d2fc7U
#define MENU_LABEL_CORE_CHEAT_OPTIONS                                          0x9293171dU
#define MENU_LABEL_CORE_INPUT_REMAPPING_OPTIONS                                0x7836a8caU
#define MENU_LABEL_DATABASE_MANAGER_LIST                                       0x7f853d8fU
#define MENU_LABEL_CURSOR_MANAGER_LIST                                         0xa969e378U
#define MENU_LABEL_REMAP_FILE_LOAD                                             0x9c2799b8U
#define MENU_LABEL_MESSAGE                                                     0xbe463eeaU
#define MENU_LABEL_INFO_SCREEN                                                 0xd97853d0U
#define MENU_LABEL_LOAD_OPEN_ZIP                                               0x8aa3c068U
#define MENU_LABEL_CUSTOM_RATIO                                                0xf038731eU
#define MENU_LABEL_PAL60_ENABLE                                                0x62bc416eU
#define MENU_LABEL_CONTENT_HISTORY_PATH                                        0x6f22fb9dU
#define MENU_LABEL_JOYPAD_AUTOCONFIG_DIR                                       0x2f4822d8U
#define MENU_LABEL_RECORDING_SETTINGS                                          0x1a80b313U
#define MENU_LABEL_ONSCREEN_KEYBOARD_OVERLAY_SETTINGS                          0xa6de9ba6U
#define MENU_LABEL_PLAYLIST_SETTINGS                                           0xdb3e0e07U
#define MENU_LABEL_ARCHIVE_SETTINGS                                            0x78e85398U
#define MENU_LABEL_DIRECTORY_SETTINGS                                          0xb817bd2bU
#define MENU_LABEL_SHADER_APPLY_CHANGES                                        0x4f7306b9U
#define MENU_LABEL_ONSCREEN_DISPLAY_SETTINGS                                   0x67571029U
#define MENU_LABEL_CUSTOM_BIND                                                 0x1e84b3fcU
#define MENU_LABEL_CUSTOM_BIND_ALL                                             0x79ac14f4U
#define MENU_LABEL_CUSTOM_BIND_DEFAULTS                                        0xe88f7b13U
#define MENU_LABEL_CONFIGURATIONS                                              0x3e930a50U
#define MENU_LABEL_REMAP_FILE_SAVE_CORE                                        0x7c9d4c8fU
#define MENU_LABEL_REMAP_FILE_SAVE_GAME                                        0x7c9f41e0U
#define MENU_LABEL_CONTENT_COLLECTION_LIST                                     0x32d1df83U
#define MENU_LABEL_OSK_ENABLE                                                  0x8e208498U
#define MENU_LABEL_EXIT_EMULATOR                                               0x86d5d467U
#define MENU_LABEL_COLLECTION                                                  0x5fea5991U

#define MENU_LABEL_USER_LANGUAGE                                               0x33ebaa27U
#define MENU_LABEL_USER_SETTINGS                                               0xcdc9a8f5U

#define MENU_LABEL_OPEN_ARCHIVE                                                0x78c0ca58U
#define MENU_LABEL_OPEN_ARCHIVE_DETECT_CORE                                    0x92442638U
#define MENU_LABEL_LOAD_ARCHIVE_DETECT_CORE                                    0x681f2f46U
#define MENU_LABEL_LOAD_ARCHIVE                                                0xc3834e66U

/* Help */
#define MENU_LABEL_VIDEO_THREADED                                              0x0626179cU
#define MENU_LABEL_VIDEO_MONITOR_INDEX                                         0xb6fcdc9aU
#define MENU_LABEL_VIDEO_HARD_SYNC                                             0xdcd623b6U
#define MENU_LABEL_RGUI_SHOW_START_SCREEN                                      0x6b38f0e8U
#define MENU_LABEL_PAUSE_NONACTIVE                                             0x580bf549U
#define MENU_LABEL_AUDIO_DEVICE                                                0x2574eac6U
#define MENU_LABEL_AUDIO_VOLUME                                                0x502173aeU
#define MENU_LABEL_AUDIO_RATE_CONTROL_DELTA                                    0xc8bde3cbU
#define MENU_LABEL_SLOWMOTION_RATIO                                            0x626b3ffeU
#define MENU_LABEL_FASTFORWARD_RATIO                                           0x3a0c2706U
#define MENU_LABEL_INPUT_TURBO_PERIOD                                          0xf7a97482U
#define MENU_LABEL_LOG_VERBOSITY                                               0x6648c96dU
#define MENU_LABEL_SLOWMOTION                                                  0x6a269ea0U
#define MENU_LABEL_HOLD_FAST_FORWARD                                           0xebe2e4cdU
#define MENU_LABEL_PAUSE_TOGGLE                                                0x557634e4U
#define MENU_LABEL_WELCOME_TO_RETROARCH                                        0xbcff0b3cU
#define MENU_LABEL_HELP_CONTROLS                                               0x04859221U
#define MENU_LABEL_HELP_LIST                                                   0x006af669U
#define MENU_LABEL_HELP_WHAT_IS_A_CORE                                         0x83fcbc44U
#define MENU_LABEL_HELP_LOADING_CONTENT                                        0x231d8245U
#define MENU_LABEL_HELP_SCANNING_CONTENT                                       0x1dec52b8U
#define MENU_LABEL_HELP_CHANGE_VIRTUAL_GAMEPAD                                 0x6e66ef07U
#define MENU_LABEL_HELP_AUDIO_VIDEO_TROUBLESHOOTING                            0xd44d395cU
#define MENU_LABEL_TURBO                                                       0x107434f1U
#define MENU_LABEL_VOLUME_UP                                                   0xa66e9681U
#define MENU_LABEL_VOLUME_DOWN                                                 0xfc64f3d4U
#define MENU_LABEL_VALUE_EXTRACTING_PLEASE_WAIT                                0xec5a348bU

#define MENU_LABEL_VALUE_MENU_CONTROLS_PROLOG                                  0x72674cdfU
#define MENU_LABEL_VALUE_HELP_WHAT_IS_A_CORE                                   0xf3b0f77eU
#define MENU_LABEL_VALUE_HELP_LOADING_CONTENT                                  0x70bab027U
#define MENU_LABEL_VALUE_HELP_LIST                                             0x6c57426aU
#define MENU_LABEL_VALUE_HELP_CONTROLS                                         0xe5c9f6a2U
#define MENU_LABEL_VALUE_WHAT_IS_A_CORE_DESC                                   0xc832957eU
#define MENU_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD                           0x27ed0204U
#define MENU_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD_DESC                      0x9d0e79dbU
#define MENU_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING                      0xd0e5c3ffU
#define MENU_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING_DESC                 0x60031d7aU
#define MENU_LABEL_VALUE_HELP_SCANNING_CONTENT                                 0x74b36f11U
#define MENU_LABEL_VALUE_HELP_SCANNING_CONTENT_DESC                            0xac947056U

/* Main menu */
#define MENU_LABEL_START_NET_RETROPAD                                          0xf2ae670dU
#define MENU_LABEL_LOAD_CONTENT                                                0x828943c3U
#define MENU_LABEL_LOAD_CONTENT_LIST                                           0x5745de1fU
#define MENU_LABEL_LOAD_CONTENT_HISTORY                                        0xfe1d79e5U
#define MENU_LABEL_ADD_CONTENT_LIST                                            0x046f4668U
#define MENU_LABEL_ONLINE_UPDATER                                              0xcac0025eU
#define MENU_LABEL_SETTINGS                                                    0x1304dc16U
#define MENU_LABEL_SAVE_CURRENT_CONFIG                                         0x8840ba8bU
#define MENU_LABEL_SAVE_NEW_CONFIG                                             0xcce9ab72U
#define MENU_LABEL_HELP                                                        0x7c97d2eeU
#define MENU_LABEL_QUIT_RETROARCH                                              0x84b0bc71U

/* File values */

#define MENU_VALUE_FILE_WEBM                                                   0x7ca00b50U
#define MENU_VALUE_FILE_F4F                                                    0x0b886be5U
#define MENU_VALUE_FILE_F4V                                                    0x0b886bf5U
#define MENU_VALUE_FILE_OGM                                                    0x0b8898c8U
#define MENU_VALUE_FILE_MKV                                                    0x0b8890d3U
#define MENU_VALUE_FILE_AVI                                                    0x0b885f25U
#define MENU_VALUE_FILE_M4A                                                    0x0b8889a7U
#define MENU_VALUE_FILE_3GP                                                    0x0b87998fU
#define MENU_VALUE_FILE_MP4                                                    0x0b889136U
#define MENU_VALUE_FILE_MP3                                                    0x0b889135U
#define MENU_VALUE_FILE_FLAC                                                   0x7c96d67bU
#define MENU_VALUE_FILE_OGG                                                    0x0b8898c2U
#define MENU_VALUE_FILE_FLV                                                    0x0b88732dU
#define MENU_VALUE_FILE_WAV                                                    0x0b88ba13U
#define MENU_VALUE_FILE_MOV                                                    0x0b889157U
#define MENU_VALUE_FILE_WMV                                                    0x0b88bb9fU

#define MENU_VALUE_FILE_JPG                                                    0x0b8884a6U
#define MENU_VALUE_FILE_JPEG                                                   0x7c99198bU
#define MENU_VALUE_FILE_JPG_CAPS                                               0x0b87f846U
#define MENU_VALUE_FILE_JPEG_CAPS                                              0x7c87010bU
#define MENU_VALUE_FILE_PNG                                                    0x0b889deaU
#define MENU_VALUE_FILE_PNG_CAPS                                               0x0b88118aU
#define MENU_VALUE_FILE_TGA                                                    0x0b88ae01U
#define MENU_VALUE_FILE_BMP                                                    0x0b886244U

#define MENU_VALUE_MD5                                                         0x0b888fabU
#define MENU_VALUE_SHA1                                                        0x7c9de632U
#define MENU_VALUE_CRC                                                         0x0b88671dU
#define MENU_VALUE_MORE                                                        0x0b877cafU
#define MENU_VALUE_HORIZONTAL_MENU                                             0x35761704U
#define MENU_VALUE_ON                                                          0x005974c2U
#define MENU_VALUE_OFF                                                         0x0b880c40U
#define MENU_VALUE_COMP                                                        0x6a166ba5U
#define MENU_VALUE_MUSIC                                                       0xc4a73997U
#define MENU_VALUE_IMAGE                                                       0xbab7ebf9U
#define MENU_VALUE_MOVIE                                                       0xc43c4bf6U
#define MENU_VALUE_CORE                                                        0x6a167f7fU
#define MENU_VALUE_CURSOR                                                      0x57bba8b4U
#define MENU_VALUE_FILE                                                        0x6a496536U
#define MENU_VALUE_RDB                                                         0x0b00f54eU
#define MENU_VALUE_DIR                                                         0x0af95f55U
#define MENU_VALUE_NO_CORE                                                     0x7d5472cbU
#define MENU_VALUE_GLSLP                                                       0x0f840c87U
#define MENU_VALUE_CGP                                                         0x0b8865bfU
#define MENU_VALUE_GLSL                                                        0x7c976537U
#define MENU_VALUE_HLSL                                                        0x7c97f198U
#define MENU_VALUE_HLSLP                                                       0x0f962508U
#define MENU_VALUE_CG                                                          0x0059776fU
#define MENU_VALUE_SLANG                                                       0x105ce63aU
#define MENU_VALUE_SLANGP                                                      0x1bf9adeaU

int menu_hash_get_help_de(uint32_t hash, char *s, size_t len);

int menu_hash_get_help_es(uint32_t hash, char *s, size_t len);

int menu_hash_get_help_fr(uint32_t hash, char *s, size_t len);

int menu_hash_get_help_it(uint32_t hash, char *s, size_t len);

int menu_hash_get_help_nl(uint32_t hash, char *s, size_t len);

int menu_hash_get_help_pl(uint32_t hash, char *s, size_t len);

int menu_hash_get_help_pt(uint32_t hash, char *s, size_t len);

int menu_hash_get_help_eo(uint32_t hash, char *s, size_t len);

int menu_hash_get_help_us(uint32_t hash, char *s, size_t len);

int menu_hash_get_help(uint32_t hash, char *s, size_t len);

RETRO_END_DECLS

#endif
