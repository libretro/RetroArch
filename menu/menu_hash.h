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

#include "../retroarch.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MENU_LABEL_MENU_LINEAR_FILTER                                          0x5fe9128cU
#define MENU_LABEL_VALUE_MENU_LINEAR_FILTER                                    0x192de208U

#define MENU_LABEL_MENU_THROTTLE_FRAMERATE                                     0x9a8681c5U
#define MENU_LABEL_VALUE_MENU_THROTTLE_FRAMERATE                               0x285bb667U

#define MENU_LABEL_INPUT_POLL_TYPE_BEHAVIOR                                    0x8360107bU
#define MENU_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR                              0xaa23fc1eU

#define MENU_LABEL_VALUE_SYSTEM_INFO_DYNAMIC_SUPPORT                           0xe5f4b599U

#define MENU_LABEL_UI_COMPANION_ENABLE                                         0xb2d7a20cU
#define MENU_LABEL_VALUE_UI_COMPANION_ENABLE                                   0xee4933ceU

#define MENU_LABEL_CHEEVOS_HARDCORE_MODE_ENABLE                                0xf71b3b16U
#define MENU_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE                          0xa4d69592U

#define MENU_LABEL_CHEEVOS_TEST_UNOFFICIAL                                     0xa1ae28f0U
#define MENU_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL                               0x0698e665U

#define MENU_LABEL_VALUE_ENABLE                                                0xb0d05f8cU

#define MENU_LABEL_VALUE_CHEEVOS_SETTINGS                                      0x1fe3be93U

#define MENU_LABEL_CHEEVOS_ENABLE                                              0x2748f998U

#define MENU_LABEL_VALUE_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE                   0x507c52f3U
#define MENU_LABEL_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE                         0x3665cbb0U

#define MENU_LABEL_CHEEVOS_DESCRIPTION                                         0x7e00e0f5U

#define MENU_LABEL_VALUE_CHEEVOS_DESCRIPTION                                   0xab3975d6U

#define MENU_LABEL_VALUE_STATE_SLOT                                            0xa1dec768U

#define MENU_LABEL_STATE_SLOT                                                  0x27b67f67U

#define MENU_LABEL_INPUT_HOTKEY_BINDS_BEGIN                                    0x5a56139bU

#define MENU_LABEL_INPUT_SETTINGS                                              0x78b4a7c5U

#define MENU_LABEL_PLAYLIST_SETTINGS_BEGIN                                     0x80a8d2cbU
#define MENU_LABEL_DEFERRED_PLAYLIST_SETTINGS_LIST                             0x9518e0c7U

#define MENU_LABEL_INPUT_SETTINGS_BEGIN                                        0xddee308bU

#define MENU_LABEL_DEFERRED_INPUT_SETTINGS_LIST                                0x050bec60U

#define MENU_LABEL_DEFERRED_USER_BINDS_LIST                                    0x28c5750eU

#define MENU_LABEL_CHEEVOS_USERNAME                                            0x6ce57e31U
#define MENU_LABEL_CHEEVOS_PASSWORD                                            0x86c38d24U

#define MENU_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS                             0xe0b53ce3U

#define MENU_LABEL_ACCOUNTS_CHEEVOS_PASSWORD                                   0x45cf62e3U
#define MENU_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD                             0xe5a73d05U

#define MENU_LABEL_ACCOUNTS_CHEEVOS_USERNAME                                   0x2bf153f0U
#define MENU_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME                             0xcbc92e12U

#define MENU_LABEL_ACCOUNTS_RETRO_ACHIEVEMENTS                                 0xe6b7c16cU
#define MENU_LABEL_VALUE_ACCOUNTS_RETRO_ACHIEVEMENTS                           0x7d247a6dU

#define MENU_LABEL_DEFERRED_ACCOUNTS_CHEEVOS_LIST                              0x1322324cU
#define MENU_LABEL_DEFERRED_ACCOUNTS_LIST                                      0x3d2b8860U
#define MENU_LABEL_ACCOUNTS_LIST                                               0x774c15a0U
#define MENU_LABEL_VALUE_ACCOUNTS_LIST                                         0x86e551a1U
#define MENU_LABEL_VALUE_ACCOUNTS_LIST_END                                     0x3d559522U

#define MENU_LABEL_DEBUG_PANEL_ENABLE                                          0xbad176a1U
#define MENU_LABEL_VALUE_DEBUG_PANEL_ENABLE                                    0x15042803U

#define MENU_LABEL_VALUE_MENU_CONTROLS_PROLOG                                  0x72674cdfU

#define MENU_LABEL_VALUE_HELP_WHAT_IS_A_CORE                                   0xf3b0f77eU
#define MENU_LABEL_HELP_WHAT_IS_A_CORE                                         0x83fcbc44U

#define MENU_LABEL_HELP_LOADING_CONTENT                                        0x231d8245U
#define MENU_LABEL_VALUE_HELP_LOADING_CONTENT                                  0x70bab027U

#define MENU_LABEL_HELP_LIST                                                   0x006af669U
#define MENU_LABEL_VALUE_HELP_LIST                                             0x6c57426aU

#define MENU_LABEL_VALUE_HELP_CONTROLS                                         0xe5c9f6a2U
#define MENU_LABEL_HELP_CONTROLS                                               0x04859221U

#define MENU_LABEL_VALUE_BASIC_MENU_CONTROLS                                   0x7c05810eU
#define MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP                         0x8c2f7b00U
#define MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN                       0x55f0f413U
#define MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM                           0x40e50edbU
#define MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK                              0xd834d89eU
#define MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_START                             0xe00df0fbU
#define MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_INFO                              0xd838e6f9U
#define MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU                       0x98159c23U
#define MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT                              0xd83d6830U
#define MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD                   0xe408c2ffU

#define MENU_LABEL_VALUE_EXTRACTING_PLEASE_WAIT                                0xec5a348bU

#define MENU_LABEL_WELCOME_TO_RETROARCH                                        0xbcff0b3cU

#define MENU_LABEL_DEFERRED_ARCHIVE_ACTION_DETECT_CORE                         0xdc9c0064U
#define MENU_LABEL_DEFERRED_ARCHIVE_ACTION                                     0x7faf0284U
#define MENU_LABEL_DEFERRED_ARCHIVE_OPEN_DETECT_CORE                           0xd9452498U
#define MENU_LABEL_DEFERRED_ARCHIVE_OPEN                                       0xfa0938b8U

#define MENU_LABEL_VALUE_INPUT_BACK_AS_MENU_TOGGLE_ENABLE                      0x1cf1e6a8U
#define MENU_LABEL_INPUT_BACK_AS_MENU_TOGGLE_ENABLE                            0x60bacd04U

#define MENU_LABEL_INPUT_MENU_TOGGLE_GAMEPAD_COMBO                             0xc5b7aa47U
#define MENU_LABEL_VALUE_INPUT_MENU_TOGGLE_GAMEPAD_COMBO                       0x0dedea3bU

#define MENU_LABEL_INPUT_OVERLAY_HIDE_IN_MENU                                  0xf09e230aU
#define MENU_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU                            0x39b5bd0dU

#define MENU_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST                      0x39310fc8U
#define MENU_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST                            0xb4f82700U

#define MENU_LABEL_VALUE_UPDATE_CORE_INFO_FILES                                0xba274810U
#define MENU_LABEL_UPDATE_CORE_INFO_FILES                                      0x620d758dU

#define MENU_VALUE_SEARCH                                                      0xd0d5febbU

#define MENU_LABEL_DEFERRED_CORE_CONTENT_LIST                                  0x76150c63U
#define MENU_LABEL_DEFERRED_LAKKA_LIST                                         0x3db437c4U

#define MENU_LABEL_VALUE_DOWNLOAD_CORE_CONTENT                                 0xa8bb22d8U
#define MENU_LABEL_DOWNLOAD_CORE_CONTENT                                       0xc63b1d3fU

#define MENU_LABEL_SCAN_THIS_DIRECTORY                                         0x6921b775U
#define MENU_LABEL_VALUE_SCAN_THIS_DIRECTORY                                   0x2911e177U

#define MENU_LABEL_SCAN_DIRECTORY                                              0x57de303eU
#define MENU_LABEL_VALUE_SCAN_DIRECTORY                                        0x61af24dfU

#define MENU_LABEL_VALUE_SCAN_FILE                                             0x41be3aeaU
#define MENU_LABEL_SCAN_FILE                                                   0xd5d1eee9U

#define MENU_LABEL_ADD_CONTENT_LIST                                            0x046f4668U
#define MENU_LABEL_VALUE_ADD_CONTENT_LIST                                      0x955da2c9U

#define MENU_LABEL_UPDATE_AUTOCONFIG_PROFILES_HID                              0x1e94ee4dU
#define MENU_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES_HID                        0x524f4590U

#define MENU_VALUE_DONT_CARE                                                   0x19da07bcU
#define MENU_VALUE_LINEAR                                                      0xc0d12dc0U
#define MENU_VALUE_NEAREST                                                     0x6ab2b0b7U
#define MENU_VALUE_UNKNOWN                                                     0x9b3bb635U
#define MENU_VALUE_USER                                                        0x7c8da264U
#define MENU_VALUE_CHEAT                                                       0x0cf62beaU
#define MENU_VALUE_SHADER                                                      0xd10c0cfcU
#define MENU_VALUE_DIRECTORY_CONTENT                                           0x89a45bd9U
#define MENU_VALUE_DIRECTORY_NONE                                              0x9996c10fU
#define MENU_VALUE_DIRECTORY_DEFAULT                                           0xdcc3a2e4U
#define MENU_VALUE_NOT_AVAILABLE                                               0x0b880503U
#define MENU_VALUE_ASK_ARCHIVE                                                 0x0b87d6a4U

#define MENU_LABEL_UPDATE_ASSETS                                               0x37fa42daU
#define MENU_LABEL_VALUE_UPDATE_ASSETS                                         0x0fdf0b1bU

#define MENU_LABEL_UPDATE_LAKKA                                                0x19b51eebU
#define MENU_LABEL_VALUE_UPDATE_LAKKA                                          0x6611630cU

#define MENU_LABEL_UPDATE_CHEATS                                               0x3bd5c83fU
#define MENU_LABEL_VALUE_UPDATE_CHEATS                                         0x13ba9080U

#define MENU_LABEL_UPDATE_AUTOCONFIG_PROFILES                                  0xddfcf979U
#define MENU_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES                            0x6ddaf7fbU

#define MENU_LABEL_UPDATE_DATABASES                                            0x158ee0cfU
#define MENU_LABEL_VALUE_UPDATE_DATABASES                                      0x00c24d70U

#define MENU_LABEL_UPDATE_OVERLAYS                                             0xd25d221cU
#define MENU_LABEL_VALUE_UPDATE_OVERLAYS                                       0x3694fe9dU

#define MENU_LABEL_UPDATE_CG_SHADERS                                           0x9473991aU
#define MENU_LABEL_VALUE_UPDATE_CG_SHADERS                                     0x22999e5cU

#define MENU_LABEL_UPDATE_GLSL_SHADERS                                         0x2413b762U
#define MENU_LABEL_VALUE_UPDATE_GLSL_SHADERS                                   0xe2060484U

#define MENU_LABEL_INFORMATION_LIST                                            0x225e7606U
#define MENU_LABEL_VALUE_INFORMATION_LIST                                      0xd652344bU

#define MENU_LABEL_USE_BUILTIN_PLAYER                                          0x9927ca74U
#define MENU_LABEL_VALUE_USE_BUILTIN_PLAYER                                    0x038e4816U

#define MENU_LABEL_CONTENT_SETTINGS                                            0xe789f7f6U
#define MENU_LABEL_VALUE_CONTENT_SETTINGS                                      0x61b23ff7U

#define MENU_LABEL_LOAD_CONTENT_LIST                                           0x5745de1fU
#define MENU_LABEL_VALUE_LOAD_CONTENT_LIST                                     0x55ff08eaU

#define MENU_LABEL_NO_SETTINGS_FOUND                                           0xabf77040U
#define MENU_LABEL_VALUE_NO_SETTINGS_FOUND                                     0xffcc5b5dU
#define MENU_LABEL_VALUE_NO_PERFORMANCE_COUNTERS                               0xb4b52b95U

#define MENU_LABEL_VIDEO_FONT_ENABLE                                           0x697d9b58U
#define MENU_LABEL_VALUE_VIDEO_FONT_ENABLE                                     0x272a12a6U
#define MENU_LABEL_VIDEO_FONT_PATH                                             0xd0de729eU
#define MENU_LABEL_VALUE_VIDEO_FONT_PATH                                       0x025c4de7U
#define MENU_LABEL_VIDEO_FONT_SIZE                                             0xd0e03a8cU
#define MENU_LABEL_VALUE_VIDEO_FONT_SIZE                                       0x026356cbU
#define MENU_LABEL_VIDEO_MESSAGE_POS_X                                         0xa133c368U
#define MENU_LABEL_VALUE_VIDEO_MESSAGE_POS_X                                   0x4b1ac89dU
#define MENU_LABEL_VIDEO_MESSAGE_POS_Y                                         0xa133c369U
#define MENU_LABEL_VALUE_VIDEO_MESSAGE_POS_Y                                   0x4f2559beU

#define MENU_LABEL_DEFERRED_INPUT_HOTKEY_BINDS_LIST                            0x10b41d97U
#define MENU_LABEL_INPUT_HOTKEY_BINDS                                          0x1b7ef2d7U
#define MENU_LABEL_VALUE_INPUT_HOTKEY_BINDS                                    0x1cb39c19U

#define MENU_LABEL_VALUE_FRAME_THROTTLE_SETTINGS                               0x573b8837U

#define MENU_LABEL_FRAME_THROTTLE_ENABLE                                       0xbe52e701U
#define MENU_LABEL_VALUE_FRAME_THROTTLE_ENABLE                                 0x936f04a8U

#define MENU_LABEL_VIDEO_FILTER_FLICKER                                        0x2e21eba0U
#define MENU_LABEL_VALUE_VIDEO_FILTER_FLICKER                                  0x87c7226bU

#define MENU_LABEL_VIDEO_SOFT_FILTER                                           0x92819a46U
#define MENU_LABEL_VALUE_VIDEO_SOFT_FILTER                                     0xd035df8eU

#define MENU_LABEL_CORE_ENABLE                                                 0x2f37fe48U
#define MENU_LABEL_VALUE_CORE_ENABLE                                           0x751e2065U

#define MENU_LABEL_MOUSE_ENABLE                                                0x1240fa88U
#define MENU_LABEL_VALUE_MOUSE_ENABLE                                          0xd5bf366bU

#define MENU_LABEL_SHOW_ADVANCED_SETTINGS                                      0xbc6ac8dfU
#define MENU_LABEL_VALUE_SHOW_ADVANCED_SETTINGS                                0x851ee46dU

#define MENU_LABEL_POINTER_ENABLE                                              0xf051a7a0U
#define MENU_LABEL_VALUE_POINTER_ENABLE                                        0x1e24b9e5U

#define MENU_LABEL_COLLAPSE_SUBGROUPS_ENABLE                                   0x585ad75bU
#define MENU_LABEL_VALUE_COLLAPSE_SUBGROUPS_ENABLE                             0xdb677262U

#define MENU_LABEL_RESET                                                       0x10474288U
#define MENU_LABEL_SLOWMOTION                                                  0x6a269ea0U
#define MENU_LABEL_HOLD_FAST_FORWARD                                           0xebe2e4cdU
#define MENU_LABEL_CHEAT_TOGGLE                                                0xe515e0cbU
#define MENU_LABEL_PAUSE_TOGGLE                                                0x557634e4U

#define MENU_LABEL_PAUSE_LIBRETRO                                              0xf954afb9U
#define MENU_LABEL_VALUE_PAUSE_LIBRETRO                                        0x632ea57fU

#define MENU_LABEL_VALUE_DIRECTORY_NOT_FOUND                                   0xcdf3c0d5U
#define MENU_LABEL_VALUE_NO_ITEMS                                              0x7d33e412U

#define MENU_LABEL_UI_MENUBAR_ENABLE                                           0x1ddc5492U
#define MENU_LABEL_VALUE_UI_MENUBAR_ENABLE                                     0x11927e13U

#define MENU_LABEL_UI_COMPANION_START_ON_BOOT                                  0x36b23782U
#define MENU_LABEL_VALUE_UI_COMPANION_START_ON_BOOT                            0x94796ba6U

#define MENU_LABEL_ARCHIVE_MODE                                                0x7fac00cbU
#define MENU_LABEL_VALUE_ARCHIVE_MODE                                          0xe4c4b559U
#define MENU_LABEL_VALUE_SHADER_OPTIONS                                        0xf3fb0028U
#define MENU_LABEL_VALUE_USE_THIS_DIRECTORY                                    0xc5fc9ed9U
#define MENU_LABEL_USE_THIS_DIRECTORY                                          0xc51d351dU
#define MENU_LABEL_VALUE_CORE_OPTIONS                                          0x1477b95aU
#define MENU_LABEL_VALUE_NO_SHADER_PARAMETERS                                  0x8ccc809bU
#define MENU_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE                         0x2a11fe80U
#define MENU_LABEL_VALUE_NO_CORES_AVAILABLE                                    0xe16bfd0dU
#define MENU_LABEL_SAVE_STATE                                                  0x3a4849b5U
#define MENU_LABEL_VALUE_LOAD_STATE                                            0xd23ba706U
#define MENU_LABEL_VALUE_SAVE_STATE                                            0x3e182415U
#define MENU_LABEL_LOAD_STATE                                                  0xa39eb286U
#define MENU_LABEL_REWIND                                                      0x1931d5aeU
#define MENU_LABEL_NETPLAY_FLIP_PLAYERS                                        0x801425abU
#define MENU_LABEL_CHEAT_INDEX_MINUS                                           0x57f58b6cU
#define MENU_LABEL_CHEAT_INDEX_PLUS                                            0x678542a4U
#define MENU_LABEL_AUDIO_ENABLE                                                0x28614f5dU
#define MENU_LABEL_VALUE_AUDIO_ENABLE                                          0xcdbb9b9eU
#define MENU_LABEL_SCREENSHOT_DIRECTORY                                        0x552612d7U
#define MENU_LABEL_SHADER_NEXT                                                 0x54d359baU
#define MENU_LABEL_SHADER_PREV                                                 0x54d4a758U
#define MENU_LABEL_FRAME_ADVANCE                                               0xd80302a1U
#define MENU_LABEL_FPS_SHOW                                                    0x5ea1e10eU
#define MENU_LABEL_VALUE_FPS_SHOW                                              0x92588792U
#define MENU_LABEL_MOVIE_RECORD_TOGGLE                                         0xa2d2ff04U
#define MENU_LABEL_L_X_PLUS                                                    0xd7370d4bU
#define MENU_LABEL_L_X_MINUS                                                   0xbde0aaf3U
#define MENU_LABEL_L_Y_PLUS                                                    0xd98c35ecU
#define MENU_LABEL_L_Y_MINUS                                                   0x0adae7b4U
#define MENU_LABEL_R_X_PLUS                                                    0x60c20a91U
#define MENU_LABEL_R_X_MINUS                                                   0x78cb50f9U
#define MENU_LABEL_R_Y_MINUS                                                   0xc5c58dbaU
#define MENU_LABEL_R_Y_PLUS                                                    0x63173332U
#define MENU_LABEL_VIDEO_SWAP_INTERVAL                                         0x5673ff9aU
#define MENU_LABEL_VALUE_VIDEO_SWAP_INTERVAL                                   0xe41b3878U
#define MENU_LABEL_VIDEO_GPU_SCREENSHOT                                        0xee2fcb44U
#define MENU_LABEL_VALUE_VIDEO_GPU_SCREENSHOT                                  0x4af80c36U
#define MENU_LABEL_PAUSE_NONACTIVE                                             0x580bf549U
#define MENU_LABEL_VALUE_PAUSE_NONACTIVE                                       0xe985d38dU
#define MENU_LABEL_BLOCK_SRAM_OVERWRITE                                        0xc4e88d08U
#define MENU_LABEL_VALUE_BLOCK_SRAM_OVERWRITE                                  0x9b38260aU
#define MENU_LABEL_VIDEO_FULLSCREEN                                            0x9506dd4eU
#define MENU_LABEL_VALUE_VIDEO_FULLSCREEN                                      0x232743caU
#define MENU_LABEL_CORE_SPECIFIC_CONFIG                                        0x3c9a55e8U
#define MENU_LABEL_VALUE_CORE_SPECIFIC_CONFIG                                  0x8b8bec5aU
#define MENU_LABEL_GAME_SPECIFIC_OPTIONS                                       0x142ec90fU
#define MENU_LABEL_VALUE_GAME_SPECIFIC_OPTIONS                                 0x6aed8a05U
#define MENU_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE                          0xf8d2456cU
#define MENU_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_IN_USE                          0x06BF9E5F8
#define MENU_LABEL_AUTO_OVERRIDES_ENABLE                                       0x35ff91b6U
#define MENU_LABEL_VALUE_AUTO_OVERRIDES_ENABLE                                 0xc21c3a11U
#define MENU_LABEL_AUTO_REMAPS_ENABLE                                          0x98c8f98bU
#define MENU_LABEL_VALUE_AUTO_REMAPS_ENABLE                                    0x390f9666U
#define MENU_LABEL_RGUI_SHOW_START_SCREEN                                      0x6b38f0e8U
#define MENU_LABEL_VALUE_RGUI_SHOW_START_SCREEN                                0x76784454U
#define MENU_LABEL_VIDEO_BLACK_FRAME_INSERTION                                 0x53477f5cU
#define MENU_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION                           0xb823faa8U
#define MENU_LABEL_VIDEO_HARD_SYNC_FRAMES                                      0xce0ece13U
#define MENU_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES                                0x1edcab0bU
#define MENU_LABEL_VIDEO_FRAME_DELAY                                           0xd4aa9df4U
#define MENU_LABEL_VALUE_VIDEO_FRAME_DELAY                                     0x990d36bfU
#define MENU_LABEL_SCREENSHOT                                                  0x9a37f083U
#define MENU_LABEL_REWIND_GRANULARITY                                          0xe859cbdfU
#define MENU_LABEL_VALUE_REWIND_GRANULARITY                                    0x6e1ae4c0U
#define MENU_LABEL_VALUE_VIDEO_ROTATION                                        0x9efcecf5U
#define MENU_LABEL_THREADED_DATA_RUNLOOP_ENABLE                                0xdf5c6d33U
#define MENU_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE                          0x04d8c10fU
#define MENU_LABEL_VIDEO_THREADED                                              0x0626179cU
#define MENU_LABEL_VALUE_VIDEO_THREADED                                        0xc7524afdU
#define MENU_LABEL_VALUE_RUN                                                   0x0b881f3aU
#define MENU_LABEL_SCREEN_RESOLUTION                                           0x5c9b3a58U
#define MENU_LABEL_VALUE_SCREEN_RESOLUTION                                     0xae3c3b19U

#define MENU_LABEL_TITLE_COLOR                                                 0x10059879U
#define MENU_LABEL_VALUE_TITLE_COLOR                                           0xea87e1dbU
#define MENU_LABEL_TIMEDATE_ENABLE                                             0xd3adcbecU
#define MENU_LABEL_VALUE_TIMEDATE_ENABLE                                       0x104bcdf7U
#define MENU_LABEL_ENTRY_NORMAL_COLOR                                          0x5154ffd1U
#define MENU_LABEL_VALUE_ENTRY_NORMAL_COLOR                                    0xa989a754U
#define MENU_LABEL_ENTRY_HOVER_COLOR                                           0x4143cfccU
#define MENU_LABEL_VALUE_ENTRY_HOVER_COLOR                                     0xb56f1b0fU
#define MENU_LABEL_AUDIO_SYNC                                                  0xe0cd6bd3U
#define MENU_LABEL_VALUE_AUDIO_SYNC                                            0xcbeb903bU
#define MENU_LABEL_VIDEO_VSYNC                                                 0x09c2d34eU
#define MENU_LABEL_VALUE_VIDEO_VSYNC                                           0xd69cd742U
#define MENU_LABEL_VIDEO_HARD_SYNC                                             0xdcd623b6U
#define MENU_LABEL_VALUE_VIDEO_HARD_SYNC                                       0x3012142dU
#define MENU_LABEL_SAVESTATE_AUTO_SAVE                                         0xf6f4a05bU
#define MENU_LABEL_SAVESTATE_AUTO_LOAD                                         0xf6f1028cU
#define MENU_LABEL_SAVESTATE_AUTO_INDEX                                        0xd4da8b84U
#define MENU_LABEL_VALUE_SAVESTATE_AUTO_INDEX                                  0x29b65b06U
#define MENU_LABEL_VALUE_SAVESTATE_AUTO_SAVE                                   0x07391f6eU
#define MENU_LABEL_VALUE_SAVESTATE_AUTO_LOAD                                   0x9b5ca25fU
#define MENU_LABEL_SYSTEM_DIRECTORY                                            0x35a6fb9eU
#define MENU_LABEL_VIDEO_DISABLE_COMPOSITION                                   0x5cbb6222U
#define MENU_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION                             0xa6200347U
#define MENU_LABEL_SUSPEND_SCREENSAVER_ENABLE                                  0x459fcb0dU
#define MENU_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE                            0xf423ff48U
#define MENU_LABEL_DPI_OVERRIDE_ENABLE                                         0xb4bf52c7U
#define MENU_LABEL_VALUE_DPI_OVERRIDE_ENABLE                                   0xd535f449U
#define MENU_LABEL_DPI_OVERRIDE_VALUE                                          0x543a3efdU
#define MENU_LABEL_VALUE_DPI_OVERRIDE_VALUE                                    0x1462aba2U
#define MENU_LABEL_XMB_SCALE_FACTOR                                            0x0177E8DF1
#define MENU_LABEL_VALUE_XMB_SCALE_FACTOR                                      0x0DCDBDB13
#define MENU_LABEL_XMB_ALPHA_FACTOR                                            0x01049C5CF
#define MENU_LABEL_VALUE_XMB_ALPHA_FACTOR                                      0x0D5A712F1
#define MENU_LABEL_XMB_FONT                                                    0x0ECA56CA2
#define MENU_LABEL_VALUE_XMB_FONT                                              0x0020337E7
#define MENU_LABEL_VOLUME_UP                                                   0xa66e9681U
#define MENU_LABEL_VOLUME_DOWN                                                 0xfc64f3d4U
#define MENU_LABEL_LOG_VERBOSITY                                               0x6648c96dU
#define MENU_LABEL_VALUE_LOG_VERBOSITY                                         0x2f9f6013U
#define MENU_LABEL_OVERLAY_NEXT                                                0x7a459145U
#define MENU_LABEL_AUDIO_VOLUME                                                0x502173aeU
#define MENU_LABEL_VALUE_AUDIO_VOLUME                                          0x0fa6ccfeU
#define MENU_LABEL_AUDIO_LATENCY                                               0x32695386U
#define MENU_LABEL_VALUE_AUDIO_LATENCY                                         0x89900e38U
#define MENU_LABEL_NETPLAY_ENABLE                                              0x607fbd68U
#define MENU_LABEL_VALUE_NETPLAY_ENABLE                                        0xbc3e81a9U
#define MENU_LABEL_NETPLAY_CLIENT_SWAP_INPUT                                   0xd87bbba9U
#define MENU_LABEL_VALUE_NETPLAY_CLIENT_SWAP_INPUT                             0x57e5be2dU
#define MENU_LABEL_NETPLAY_DELAY_FRAMES                                        0x86b2c48dU
#define MENU_LABEL_VALUE_NETPLAY_DELAY_FRAMES                                  0x1ec3edefU
#define MENU_LABEL_NETPLAY_SPECTATOR_MODE_ENABLE                               0x6f9a9440U
#define MENU_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE                         0xd78ede3eU
#define MENU_LABEL_NETPLAY_TCP_UDP_PORT                                        0x98407774U
#define MENU_LABEL_VALUE_NETPLAY_TCP_UDP_PORT                                  0xf1a0cfc6U
#define MENU_LABEL_SORT_SAVEFILES_ENABLE                                       0xed0d0df4U
#define MENU_LABEL_VALUE_SORT_SAVEFILES_ENABLE                                 0x1a6db795U
#define MENU_LABEL_SORT_SAVESTATES_ENABLE                                      0x66ff2495U
#define MENU_LABEL_VALUE_SORT_SAVESTATES_ENABLE                                0x82c5e076U
#define MENU_LABEL_NETPLAY_IP_ADDRESS                                          0xac9a53ffU
#define MENU_LABEL_VALUE_NETPLAY_IP_ADDRESS                                    0xc7ee4c84U
#define MENU_LABEL_NETPLAY_MODE                                                0xc1cf6506U
#define MENU_LABEL_VALUE_NETPLAY_MODE                                          0x2da6c748U
#define MENU_LABEL_PERFCNT_ENABLE                                              0x6823dbddU
#define MENU_LABEL_VALUE_PERFCNT_ENABLE                                        0x20eb18caU
#define MENU_LABEL_OVERLAY_SCALE                                               0x2dce2a3dU
#define MENU_LABEL_VALUE_OVERLAY_SCALE                                         0x4237794fU
#define MENU_LABEL_KEYBOARD_OVERLAY_PRESET                                     0x11f1c582U
#define MENU_LABEL_VALUE_KEYBOARD_OVERLAY_PRESET                               0x7bc03f8bU
#define MENU_LABEL_OVERLAY_PRESET                                              0x24e24796U
#define MENU_LABEL_VALUE_OVERLAY_PRESET                                        0x8338e89aU
#define MENU_LABEL_OVERLAY_OPACITY                                             0xc466fbaeU
#define MENU_LABEL_VALUE_OVERLAY_OPACITY                                       0x98605740U

#define MENU_LABEL_MENU_WALLPAPER                                              0x3b84de01U
#define MENU_LABEL_VALUE_MENU_WALLPAPER                                        0x4555d2a2U
#define MENU_LABEL_DYNAMIC_WALLPAPER                                           0xf011ccabU
#define MENU_LABEL_VALUE_DYNAMIC_WALLPAPER                                     0x66928c32U
#define MENU_LABEL_VALUE_BOXART                                                0x716441ebU
#define MENU_LABEL_BOXART                                                      0xa269b0afU

#define MENU_LABEL_FASTFORWARD_RATIO                                           0x3a0c2706U
#define MENU_LABEL_VALUE_FASTFORWARD_RATIO                                     0x3c719749U
#define MENU_LABEL_VIDEO_MONITOR_INDEX                                         0xb6fcdc9aU
#define MENU_LABEL_VALUE_VIDEO_MONITOR_INDEX                                   0x4cabbfe5U
#define MENU_LABEL_INPUT_OVERLAY_ENABLE                                        0xc7b21d5cU
#define MENU_LABEL_VALUE_INPUT_OVERLAY_ENABLE                                  0x95a716ddU
#define MENU_LABEL_INPUT_OSK_OVERLAY_ENABLE                                    0x7f8339c8U
#define MENU_LABEL_VALUE_INPUT_OSK_OVERLAY_ENABLE                              0x44e487aeU
#define MENU_LABEL_VIDEO_REFRESH_RATE_AUTO                                     0x9addb6cdU
#define MENU_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO                               0xaf24a804U
#define MENU_LABEL_VIDEO_REFRESH_RATE                                          0x56ccabf5U
#define MENU_LABEL_VALUE_VIDEO_REFRESH_RATE                                    0xdf36d1e0U
#define MENU_LABEL_VIDEO_WINDOWED_FULLSCREEN                                   0x6436d6f8U
#define MENU_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN                             0x133c7afeU
#define MENU_LABEL_VIDEO_FORCE_SRGB_DISABLE                                    0x0a7b68aaU
#define MENU_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE                              0xe5bfa9baU
#define MENU_LABEL_VIDEO_ROTATION                                              0x4ce6882bU
#define MENU_LABEL_VIDEO_SCALE                                                 0x09835d63U
#define MENU_LABEL_VALUE_VIDEO_SCALE                                           0x5cde89ceU
#define MENU_LABEL_VIDEO_SMOOTH                                                0x3aabbb35U
#define MENU_LABEL_VALUE_VIDEO_SMOOTH                                          0xeb0723aeU
#define MENU_LABEL_VIDEO_CROP_OVERSCAN                                         0x861f7a2fU
#define MENU_LABEL_VALUE_VIDEO_CROP_OVERSCAN                                   0xc0b575e2U
#define MENU_LABEL_VIDEO_SCALE_INTEGER                                         0x65c4b090U
#define MENU_LABEL_VALUE_VIDEO_SCALE_INTEGER                                   0xca090a9bU

#define MENU_LABEL_AUDIO_RATE_CONTROL_DELTA                                    0xc8bde3cbU
#define MENU_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA                              0x8d242b0eU
#define MENU_LABEL_AUDIO_MAX_TIMING_SKEW                                       0x4c96f75cU
#define MENU_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW                                 0x8e873f6eU

#define MENU_LABEL_INPUT_PLAYER1_JOYPAD_INDEX                                  0xfad6ab2fU
#define MENU_LABEL_INPUT_PLAYER2_JOYPAD_INDEX                                  0x3616e4d0U
#define MENU_LABEL_INPUT_PLAYER3_JOYPAD_INDEX                                  0x71571e71U
#define MENU_LABEL_INPUT_PLAYER4_JOYPAD_INDEX                                  0xac975812U
#define MENU_LABEL_INPUT_PLAYER5_JOYPAD_INDEX                                  0xe7d791b3U

#define MENU_LABEL_AUDIO_DEVICE                                                0x2574eac6U
#define MENU_LABEL_VALUE_AUDIO_DEVICE                                          0xcacf3707U

#define MENU_LABEL_REWIND_ENABLE                                               0x9761e074U
#define MENU_LABEL_VALUE_REWIND_ENABLE                                         0xce8cc18eU
#define MENU_LABEL_ENABLE_HOTKEY                                               0xc04037bfU
#define MENU_LABEL_DISK_EJECT_TOGGLE                                           0x49633fbbU
#define MENU_LABEL_DISK_NEXT                                                   0xeeaf6c6eU
#define MENU_LABEL_GRAB_MOUSE_TOGGLE                                           0xb2869aaaU
#define MENU_LABEL_MENU_TOGGLE                                                 0xfb22e3dbU
#define MENU_LABEL_STATE_SLOT_DECREASE                                         0xe48b8082U
#define MENU_LABEL_STATE_SLOT_INCREASE                                         0x36a0cbb0U

#define MENU_LABEL_LIBRETRO_LOG_LEVEL                                          0x57971ac0U
#define MENU_LABEL_VALUE_LIBRETRO_LOG_LEVEL                                    0x4455456dU

#define MENU_LABEL_INPUT_BIND_DEVICE_ID                                        0xd1ea94ecU
#define MENU_LABEL_INPUT_BIND_DEVICE_TYPE                                      0xf6e9f041U

#define MENU_LABEL_AUTOSAVE_INTERVAL                                           0xecc87351U
#define MENU_LABEL_VALUE_AUTOSAVE_INTERVAL                                     0x256f3981U
#define MENU_LABEL_CONFIG_SAVE_ON_EXIT                                         0x79b590feU
#define MENU_LABEL_VALUE_CONFIG_SAVE_ON_EXIT                                   0x4be88ae3U

#define MENU_LABEL_AUDIO_DRIVER                                                0x26594002U
#define MENU_LABEL_VALUE_AUDIO_DRIVER                                          0xcbb38c43U

#define MENU_LABEL_JOYPAD_DRIVER                                               0xab124146U
#define MENU_LABEL_VALUE_JOYPAD_DRIVER                                         0x18799878U

#define MENU_LABEL_INPUT_DRIVER                                                0x4c087840U
#define MENU_LABEL_VALUE_INPUT_DRIVER                                          0xf162c481U
#define MENU_LABEL_INPUT_DRIVER_LINUXRAW                                       0xc33c6b9fU
#define MENU_LABEL_INPUT_DRIVER_UDEV                                           0x7c9eeeb9U

#define MENU_LABEL_VIDEO_DRIVER                                                0x1805a5e7U
#define MENU_LABEL_VALUE_VIDEO_DRIVER                                          0xbd5ff228U
#define MENU_LABEL_VIDEO_DRIVER_GL                                             0x005977f8U
#define MENU_LABEL_VIDEO_DRIVER_SDL2                                           0x7c9dd69aU
#define MENU_LABEL_VIDEO_DRIVER_SDL1                                           0x0b88a968U
#define MENU_LABEL_VIDEO_DRIVER_D3D                                            0x0b886340U
#define MENU_LABEL_VIDEO_DRIVER_EXYNOS                                         0xfc37c54bU
#define MENU_LABEL_VIDEO_DRIVER_SUNXI                                          0x10620e3cU

#define MENU_LABEL_LOCATION_DRIVER                                             0x09189689U
#define MENU_LABEL_VALUE_LOCATION_DRIVER                                       0x63f0d6caU

#define MENU_LABEL_MENU_DRIVER                                                 0xd607fb05U
#define MENU_LABEL_VALUE_MENU_DRIVER                                           0xee374b46U

#define MENU_LABEL_CAMERA_DRIVER                                               0xf25db959U
#define MENU_LABEL_VALUE_CAMERA_DRIVER                                         0xca42819aU

#define MENU_LABEL_RECORD_DRIVER                                               0x144cd2cfU
#define MENU_LABEL_VALUE_RECORD_DRIVER                                         0xec319b10U

#define MENU_LABEL_AUDIO_RESAMPLER_DRIVER                                      0xedcba9ecU
#define MENU_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER                                0xb1c4f3ceU
#define MENU_LABEL_AUDIO_RESAMPLER_DRIVER_SINC                                 0x7c9dec52U
#define MENU_LABEL_AUDIO_RESAMPLER_DRIVER_CC                                   0x0059732bU

#define MENU_LABEL_SAVEFILE_DIRECTORY                                          0x92773488U
#define MENU_LABEL_VALUE_SAVEFILE_DIRECTORY                                    0x418b1929U
#define MENU_LABEL_SAVESTATE_DIRECTORY                                         0x90551289U
#define MENU_LABEL_VALUE_SAVESTATE_DIRECTORY                                   0xe6e0732aU
#define MENU_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY                                0x62f975b8U
#define MENU_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY                          0x0a3a407aU
#define MENU_LABEL_BOXARTS_DIRECTORY                                           0x9e2bdbddU
#define MENU_LABEL_VALUE_BOXARTS_DIRECTORY                                     0x56a0b90aU

#define MENU_LABEL_SLOWMOTION_RATIO                                            0x626b3ffeU
#define MENU_LABEL_VALUE_SLOWMOTION_RATIO                                      0x81c6f8ecU
#define MENU_LABEL_INPUT_MAX_USERS                                             0x2720206bU
#define MENU_LABEL_VALUE_INPUT_MAX_USERS                                       0xe6b0aefdU
#define MENU_LABEL_INPUT_REMAP_BINDS_ENABLE                                    0x536dcafeU
#define MENU_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE                              0x731709f1U
#define MENU_LABEL_INPUT_AXIS_THRESHOLD                                        0xe95c2095U
#define MENU_LABEL_VALUE_INPUT_AXIS_THRESHOLD                                  0x3fb34697U
#define MENU_LABEL_INPUT_TURBO_PERIOD                                          0xf7a97482U
#define MENU_LABEL_VALUE_INPUT_TURBO_PERIOD                                    0x9207b594U

#define MENU_LABEL_VIDEO_GAMMA                                                 0x08a951beU
#define MENU_LABEL_VALUE_VIDEO_GAMMA                                           0xc7da99dfU

#define MENU_LABEL_VIDEO_ALLOW_ROTATE                                          0x2880f0e8U
#define MENU_LABEL_VALUE_VIDEO_ALLOW_ROTATE                                    0x29a66fb4U

#define MENU_LABEL_CAMERA_ALLOW                                                0xc14d302cU
#define MENU_LABEL_VALUE_CAMERA_ALLOW                                          0x553824adU
#define MENU_LABEL_LOCATION_ALLOW                                              0xf089275cU
#define MENU_LABEL_VALUE_LOCATION_ALLOW                                        0xf039239dU

#define MENU_LABEL_TURBO                                                       0x107434f1U

#define MENU_LABEL_NO_PLAYLIST_ENTRIES_AVAILABLE                               0x8888c5acU
#define MENU_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE                         0xea82695dU
#define MENU_LABEL_VALUE_RDB_ENTRY_START_CONTENT                               0x2cf73cceU
#define MENU_LABEL_RDB_ENTRY_ANALOG                                            0x9081c2ffU
#define MENU_LABEL_RDB_ENTRY_RUMBLE                                            0xb8ae8ad4U
#define MENU_LABEL_RDB_ENTRY_COOP                                              0x7c953ff6U
#define MENU_LABEL_RDB_ENTRY_START_CONTENT                                     0x95025a55U
#define MENU_LABEL_RDB_ENTRY_DESCRIPTION                                       0x26aa1f71U
#define MENU_LABEL_RDB_ENTRY_GENRE                                             0x9fefab3eU
#define MENU_LABEL_VALUE_RDB_ENTRY_DESCRIPTION                                 0xe61a1f69U
#define MENU_LABEL_VALUE_RDB_ENTRY_GENRE                                       0x0d3d1136U
#define MENU_LABEL_RDB_ENTRY_NAME                                              0xc6ccf92eU
#define MENU_LABEL_VALUE_RDB_ENTRY_NAME                                        0x7c898026U
#define MENU_LABEL_RDB_ENTRY_PUBLISHER                                         0x4d7bcdfbU
#define MENU_LABEL_VALUE_RDB_ENTRY_PUBLISHER                                   0xce7b6ff3U
#define MENU_LABEL_RDB_ENTRY_DEVELOPER                                         0x06f61093U
#define MENU_LABEL_VALUE_RDB_ENTRY_DEVELOPER                                   0x87f5b28bU
#define MENU_LABEL_RDB_ENTRY_ORIGIN                                            0xb176aad5U
#define MENU_LABEL_VALUE_RDB_ENTRY_ORIGIN                                      0xc870cfcdU
#define MENU_LABEL_RDB_ENTRY_FRANCHISE                                         0xb31764a0U
#define MENU_LABEL_VALUE_RDB_ENTRY_FRANCHISE                                   0x34170698U
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
#define MENU_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH                               0xb68af36aU
#define MENU_LABEL_RDB_ENTRY_RELEASE_YEAR                                      0x14c9c6bfU
#define MENU_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR                                0x439e0315U
#define MENU_LABEL_RDB_ENTRY_MAX_USERS                                         0xfae91cc4U
#define MENU_LABEL_VALUE_RDB_ENTRY_MAX_USERS                                   0xe6b0aefdU
#define MENU_LABEL_RDB_ENTRY_SHA1                                              0xc6cfd31aU
#define MENU_LABEL_VALUE_RDB_ENTRY_SHA1                                        0x2d142625U
#define MENU_LABEL_VALUE_RDB_ENTRY_MD5                                         0xf1ecb7deU
#define MENU_LABEL_RDB_ENTRY_MD5                                               0xdf3c7f93U
#define MENU_LABEL_RDB_ENTRY_CRC32                                             0x9fae330aU
#define MENU_LABEL_VALUE_RDB_ENTRY_CRC32                                       0xc326ab15U

#define MENU_LABEL_VIDEO_SHADER_DEFAULT_FILTER                                 0x4468cb1bU
#define MENU_LABEL_VIDEO_SHADER_FILTER_PASS                                    0x1906c38dU
#define MENU_LABEL_VIDEO_SHADER_SCALE_PASS                                     0x18f7b82fU
#define MENU_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES                               0xb354b30bU
#define MENU_LABEL_VIDEO_SHADER_NUM_PASSES                                     0x79b2992fU
#define MENU_LABEL_VALUE_CHEAT_NUM_PASSES                                      0x8024fa39U
#define MENU_LABEL_CHEAT_NUM_PASSES                                            0x1910eb87U

#define MENU_VALUE_NO_DISK                                                     0x7d54e5cdU

#define MENU_VALUE_MD5                                                         0x0b888fabU
#define MENU_VALUE_SHA1                                                        0x7c9de632U
#define MENU_VALUE_CRC                                                         0x0b88671dU
#define MENU_VALUE_MORE                                                        0x0b877cafU
#define MENU_VALUE_HORIZONTAL_MENU                                             0x35761704U
#define MENU_VALUE_SETTINGS_TAB                                                0x6548d16dU
#define MENU_VALUE_HISTORY_TAB                                                 0xea9b0ceeU
#define MENU_VALUE_ADD_TAB                                                     0x7fb20225U
#define MENU_VALUE_PLAYLISTS_TAB                                               0x092d3161U
#define MENU_VALUE_MAIN_MENU                                                   0x1625971fU   
#define MENU_LABEL_VALUE_SETTINGS                                              0x8aca3ff6U
#define MENU_VALUE_INPUT_SETTINGS                                              0xddd30846U
#define MENU_VALUE_ON                                                          0x005974c2U
#define MENU_VALUE_OFF                                                         0x0b880c40U
#define MENU_VALUE_TRUE                                                        0x7c9e9fe5U
#define MENU_VALUE_FALSE                                                       0x0f6bcef0U
#define MENU_VALUE_COMP                                                        0x6a166ba5U
#define MENU_VALUE_MUSIC                                                       0xc4a73997U
#define MENU_VALUE_IMAGE                                                       0xbab7ebf9U
#define MENU_VALUE_MOVIE                                                       0xc43c4bf6U
#define MENU_VALUE_CORE                                                        0x6a167f7fU
#define MENU_VALUE_CURSOR                                                      0x57bba8b4U
#define MENU_VALUE_FILE                                                        0x6a496536U
#define MENU_VALUE_MISSING                                                     0x28536c3fU
#define MENU_VALUE_PRESENT                                                     0x23432826U
#define MENU_VALUE_OPTIONAL                                                    0x27bfc4abU
#define MENU_VALUE_REQUIRED                                                    0x979b1a66U
#define MENU_VALUE_RDB                                                         0x0b00f54eU
#define MENU_VALUE_DIR                                                         0x0af95f55U
#define MENU_VALUE_NO_CORE                                                     0x7d5472cbU
#define MENU_VALUE_DETECT                                                      0xab8da89eU
#define MENU_VALUE_GLSLP                                                       0x0f840c87U
#define MENU_VALUE_CGP                                                         0x0b8865bfU
#define MENU_VALUE_GLSL                                                        0x7c976537U
#define MENU_VALUE_CG                                                          0x0059776fU
#define MENU_VALUE_SLANG                                                       0x105ce63aU
#define MENU_VALUE_SLANGP                                                      0x1bf9adeaU

#define MENU_VALUE_RETROPAD                                                    0x9e6703e6U
#define MENU_VALUE_RETROKEYBOARD                                               0x9d8b6ea2U

#define MENU_LABEL_SYSTEM_BGM_ENABLE                                           0x9287a1c5U
#define MENU_LABEL_VALUE_SYSTEM_BGM_ENABLE                                     0x9025dea7U

#define MENU_LABEL_AUDIO_BLOCK_FRAMES                                          0xa85a655eU
#define MENU_LABEL_VALUE_AUDIO_BLOCK_FRAMES                                    0x118c952eU

#define MENU_LABEL_INPUT_DESCRIPTOR_LABEL_SHOW                                 0x7eefdf52U
#define MENU_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW                           0x78d0ea06U

#define MENU_LABEL_INPUT_DESCRIPTOR_HIDE_UNBOUND                               0x7051d870U
#define MENU_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND                         0xc26ddec5U

#define MENU_LABEL_INPUT_BIND_MODE                                             0x90281b55U
#define MENU_LABEL_VALUE_INPUT_BIND_MODE                                       0xe06b25c7U

#define MENU_LABEL_NETWORK_CMD_ENABLE                                          0xfdf03a08U
#define MENU_LABEL_VALUE_NETWORK_CMD_ENABLE                                    0xb822b7a1U
#define MENU_LABEL_NETWORK_CMD_PORT                                            0xc1b9e0a6U
#define MENU_LABEL_VALUE_NETWORK_CMD_PORT                                      0xee5773f3U
#define MENU_LABEL_STDIN_CMD_ENABLE                                            0x665069c0U
#define MENU_LABEL_NETWORK_REMOTE_ENABLE                                       0x99cd4420U
#define MENU_LABEL_NETWORK_REMOTE_PORT                                         0x9aef9e18U

#define MENU_LABEL_VALUE_STDIN_CMD_ENABLE                                      0xc98ecc46U
#define MENU_LABEL_VALUE_NETWORK_REMOTE_ENABLE                                 0x32f1f6f1U
#define MENU_LABEL_HISTORY_LIST_ENABLE                                         0xe1c2ae78U
#define MENU_LABEL_VALUE_HISTORY_LIST_ENABLE                                   0xd2c13bbaU
#define MENU_LABEL_CONTENT_HISTORY_SIZE                                        0x6f24c38bU
#define MENU_LABEL_VALUE_CONTENT_HISTORY_SIZE                                  0xda9c5a6eU
#define MENU_LABEL_CONTENT_ACTIONS                                             0xa0d76970U
#define MENU_LABEL_DETECT_CORE_LIST                                            0xaa07c341U
#define MENU_LABEL_VALUE_DETECT_CORE_LIST                                      0x2a2ebd1aU
#define MENU_LABEL_DETECT_CORE_LIST_OK                                         0xabba2a7aU
#define MENU_LABEL_START_CORE                                                  0xb0b6ae5bU
#define MENU_LABEL_VALUE_START_CORE                                            0x2adef65cU
#define MENU_LABEL_LOAD_CONTENT                                                0x828943c3U
#define MENU_LABEL_VALUE_LOAD_CONTENT                                          0xf0e39e65U
#define MENU_LABEL_VALUE_CORE_UPDATER_LIST                                     0x0372767dU
#define MENU_LABEL_CORE_UPDATER_LIST                                           0xe12f4ee3U
#define MENU_LABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE                           0xa3d605f5U
#define MENU_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE                     0x5248591cU
#define MENU_LABEL_CORE_UPDATER_BUILDBOT_URL                                   0xe9ad8448U
#define MENU_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL                             0x1bc80956U
#define MENU_LABEL_BUILDBOT_ASSETS_URL                                         0x1895c71eU
#define MENU_LABEL_VALUE_BUILDBOT_ASSETS_URL                                   0xaa0327a0U
#define MENU_LABEL_VIDEO_SHARED_CONTEXT                                        0x7d7dad16U
#define MENU_LABEL_VALUE_VIDEO_SHARED_CONTEXT                                  0x353d3287U
#define MENU_LABEL_DUMMY_ON_CORE_SHUTDOWN                                      0x78579f70U
#define MENU_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN                                0xc50126d3U
#define MENU_LABEL_NAVIGATION_WRAPAROUND                                       0xe76ad251U
#define MENU_LABEL_VALUE_NAVIGATION_WRAPAROUND                                 0x2609b62fU
#define MENU_LABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE       0xea48426bU
#define MENU_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE 0x94af8500U
#define MENU_LABEL_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE                         0x593d2623U
#define MENU_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE                   0x9614c0b2U
#define MENU_LABEL_CLOSE_CONTENT                                               0x4b622170U
#define MENU_LABEL_VALUE_CLOSE_CONTENT                                         0x2b3d9556U
#define MENU_LABEL_QUIT_RETROARCH                                              0x84b0bc71U
#define MENU_LABEL_VALUE_QUIT_RETROARCH                                        0x8e7024f2U
#define MENU_LABEL_SHUTDOWN                                                    0xfc460361U
#define MENU_LABEL_VALUE_SHUTDOWN                                              0x740b6741U
#define MENU_LABEL_REBOOT                                                      0x19266b70U
#define MENU_LABEL_VALUE_REBOOT                                                0xce815750U
#define MENU_LABEL_DEFERRED_VIDEO_FILTER                                       0x966ad201U
#define MENU_LABEL_DEFERRED_CORE_LIST_SET                                      0xa6d5fdb4U
#define MENU_LABEL_VALUE_STARTING_DOWNLOAD                                     0x42e10f03U
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
#define MENU_LABEL_DISK_IMAGE_APPEND                                           0x5af7d709U
#define MENU_LABEL_CORE_LIST                                                   0xa22bb14dU
#define MENU_LABEL_VALUE_CORE_LIST                                             0x0e17fd4eU
#define MENU_LABEL_MANAGEMENT                                                  0x2516c88aU
#define MENU_LABEL_VALUE_MANAGEMENT                                            0x97001d0bU
#define MENU_LABEL_ONLINE_UPDATER                                              0xcac0025eU
#define MENU_LABEL_VALUE_ONLINE_UPDATER                                        0x9f3dd2bfU
#define MENU_LABEL_SETTINGS                                                    0x1304dc16U
#define MENU_LABEL_FRONTEND_COUNTERS                                           0xe5696877U
#define MENU_LABEL_VALUE_FRONTEND_COUNTERS                                     0x5752bcf8U
#define MENU_LABEL_VALUE_CORE_COUNTERS                                         0x4610e861U
#define MENU_LABEL_CORE_COUNTERS                                               0x64cc83e0U
#define MENU_LABEL_LOAD_CONTENT_HISTORY                                        0xfe1d79e5U
#define MENU_LABEL_VALUE_LOAD_CONTENT_HISTORY                                  0x5b362286U
#define MENU_LABEL_NETWORK_INFORMATION                                         0x73ae3cb4U
#define MENU_LABEL_VALUE_NETWORK_INFORMATION                                   0xa0e6d195U
#define MENU_LABEL_SYSTEM_INFORMATION                                          0x206ebf0fU
#define MENU_LABEL_DEBUG_INFORMATION                                           0xeb0d82b1U
#define MENU_LABEL_ACHIEVEMENT_LIST                                            0x7b90fc49U
#define MENU_LABEL_VALUE_SYSTEM_INFORMATION                                    0xa62fd7f0U
#define MENU_LABEL_VALUE_DEBUG_INFORMATION                                     0xd8569f92U
#define MENU_LABEL_VALUE_ACHIEVEMENT_LIST                                      0xf066ac4aU
#define MENU_LABEL_CORE_INFORMATION                                            0xb638e0d3U
#define MENU_LABEL_VALUE_CORE_INFORMATION                                      0x781981b4U
#define MENU_LABEL_VALUE_VIDEO_SHADER_PARAMETERS                               0x5ace99b3U
#define MENU_LABEL_VIDEO_SHADER_PARAMETERS                                     0x9895c3e5U
#define MENU_LABEL_VIDEO_SHADER_PRESET_PARAMETERS                              0xd18158d7U
#define MENU_LABEL_VALUE_VIDEO_SHADER_PRESET_PARAMETERS                        0x57f696abU
#define MENU_LABEL_DISK_OPTIONS                                                0xc61ab5fbU
#define MENU_LABEL_VALUE_DISK_OPTIONS                                          0xbee508e5U
#define MENU_LABEL_CORE_OPTIONS                                                0xf65e60f9U
#define MENU_LABEL_DISK_CYCLE_TRAY_STATUS                                      0x3035cdc1U
#define MENU_LABEL_INPUT_DUTY_CYCLE                                            0xec787129U
#define MENU_LABEL_VALUE_INPUT_DUTY_CYCLE                                      0x451cc9dbU
#define MENU_LABEL_VALUE_DISK_CYCLE_TRAY_STATUS                                0xf44928c4U
#define MENU_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE                             0xd064cbe6U
#define MENU_LABEL_VALUE_DISK_INDEX                                            0xadbce4a8U
#define MENU_LABEL_VALUE_DISK_IMAGE_APPEND                                     0x1cb28c6bU
#define MENU_LABEL_DISK_INDEX                                                  0x6c14bf54U
#define MENU_LABEL_SHADER_OPTIONS                                              0x1f7d2fc7U
#define MENU_LABEL_VALUE_CORE_CHEAT_OPTIONS                                    0x8ba478bfU
#define MENU_LABEL_CORE_CHEAT_OPTIONS                                          0x9293171dU
#define MENU_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS                          0x7c65016dU
#define MENU_LABEL_CORE_INPUT_REMAPPING_OPTIONS                                0x7836a8caU
#define MENU_LABEL_VALUE_CURSOR_MANAGER                                        0xc3696afeU
#define MENU_LABEL_VALUE_DATABASE_MANAGER                                      0x6af9d2b5U
#define MENU_LABEL_DATABASE_MANAGER_LIST                                       0x7f853d8fU
#define MENU_LABEL_CURSOR_MANAGER_LIST                                         0xa969e378U
#define MENU_LABEL_VIDEO_SHADER_PASS                                           0x4fa31028U
#define MENU_LABEL_VALUE_VIDEO_SHADER_PRESET                                   0xd149336fU
#define MENU_LABEL_VIDEO_SHADER_PRESET                                         0xc5d3bae4U
#define MENU_LABEL_CHEAT_FILE_LOAD                                             0x57336148U
#define MENU_LABEL_VALUE_CHEAT_FILE_LOAD                                       0x5b983e0aU
#define MENU_LABEL_REMAP_FILE_LOAD                                             0x9c2799b8U
#define MENU_LABEL_VALUE_REMAP_FILE_LOAD                                       0xabdd415aU
#define MENU_LABEL_MESSAGE                                                     0xbe463eeaU
#define MENU_LABEL_INFO_SCREEN                                                 0xd97853d0U
#define MENU_LABEL_LOAD_OPEN_ZIP                                               0x8aa3c068U
#define MENU_LABEL_CUSTOM_RATIO                                                0xf038731eU
#define MENU_LABEL_VALUE_CUSTOM_RATIO                                          0x3c94b73fU
#define MENU_LABEL_HELP                                                        0x7c97d2eeU
#define MENU_LABEL_VALUE_HELP                                                  0x7c8646ceU
#define MENU_LABEL_INPUT_OVERLAY                                               0x24e24796U
#define MENU_LABEL_INPUT_OSK_OVERLAY                                           0x11f1c582U
#define MENU_LABEL_CHEAT_DATABASE_PATH                                         0x01388b8aU
#define MENU_LABEL_VALUE_CHEAT_DATABASE_PATH                                   0x0a883d9fU
#define MENU_LABEL_CURSOR_DIRECTORY                                            0xdee8d377U
#define MENU_LABEL_VALUE_CURSOR_DIRECTORY                                      0xca1c4018U
#define MENU_LABEL_AUDIO_OUTPUT_RATE                                           0x477b97b9U
#define MENU_LABEL_VALUE_AUDIO_OUTPUT_RATE                                     0x5d4b0372U
#define MENU_LABEL_OSK_OVERLAY_DIRECTORY                                       0xcce86287U
#define MENU_LABEL_VALUE_OSK_OVERLAY_DIRECTORY                                 0x8a4000a9U
#define MENU_LABEL_RECORDING_OUTPUT_DIRECTORY                                  0x30bece06U
#define MENU_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY                            0x93a44152U
#define MENU_LABEL_RECORDING_CONFIG_DIRECTORY                                  0x3c3f274bU
#define MENU_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY                            0x1f7d918dU
#define MENU_LABEL_VIDEO_FILTER                                                0x1c0eb741U
#define MENU_LABEL_VALUE_VIDEO_FILTER                                          0xc1690382U
#define MENU_LABEL_PAL60_ENABLE                                                0x62bc416eU
#define MENU_LABEL_VALUE_PAL60_ENABLE                                          0x05a5bc9aU
#define MENU_LABEL_CONTENT_HISTORY_PATH                                        0x6f22fb9dU
#define MENU_LABEL_AUDIO_DSP_PLUGIN                                            0x4a69572bU
#define MENU_LABEL_VALUE_AUDIO_DSP_PLUGIN                                      0x1c9f180dU
#define MENU_LABEL_RGUI_BROWSER_DIRECTORY                                      0xa86cba73U
#define MENU_LABEL_VALUE_RGUI_BROWSER_DIRECTORY                                0x088d411eU
#define MENU_LABEL_CONTENT_DATABASE_DIRECTORY                                  0x6b443f80U
#define MENU_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY                            0xccdcaacaU
#define MENU_LABEL_PLAYLIST_DIRECTORY                                          0x6361820bU
#define MENU_LABEL_VALUE_PLAYLIST_DIRECTORY                                    0x61223c36U
#define MENU_LABEL_CORE_ASSETS_DIRECTORY                                       0x8ba5ee54U
#define MENU_LABEL_VALUE_CORE_ASSETS_DIRECTORY                                 0x319b6c96U
#define MENU_LABEL_CONTENT_DIRECTORY                                           0x7738dc14U
#define MENU_LABEL_VALUE_SCREENSHOT_DIRECTORY                                  0x42186f78U
#define MENU_LABEL_INPUT_REMAPPING_DIRECTORY                                   0x5233c20bU
#define MENU_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY                             0xe81058adU
#define MENU_LABEL_VIDEO_SHADER_DIR                                            0x30f53b10U
#define MENU_LABEL_VALUE_VIDEO_SHADER_DIR                                      0xc3770351U
#define MENU_LABEL_VIDEO_FILTER_DIR                                            0x67603f1fU
#define MENU_LABEL_VALUE_VIDEO_FILTER_DIR                                      0xbb865957U
#define MENU_LABEL_AUDIO_FILTER_DIR                                            0x4bd96ebaU
#define MENU_LABEL_VALUE_AUDIO_FILTER_DIR                                      0x509bb77cU
#define MENU_LABEL_LIBRETRO_DIR_PATH                                           0x1af1eb72U
#define MENU_LABEL_VALUE_LIBRETRO_DIR_PATH                                     0xf606d103U
#define MENU_LABEL_LIBRETRO_INFO_PATH                                          0xe552b25fU
#define MENU_LABEL_VALUE_LIBRETRO_INFO_PATH                                    0x3f39960fU
#define MENU_LABEL_RGUI_CONFIG_DIRECTORY                                       0x0cb3e005U
#define MENU_LABEL_VALUE_RGUI_CONFIG_DIRECTORY                                 0x20eb5170U
#define MENU_LABEL_OVERLAY_DIRECTORY                                           0xc4ed3d1bU
#define MENU_LABEL_VALUE_OVERLAY_DIRECTORY                                     0xdb8925bcU
#define MENU_LABEL_VALUE_SYSTEM_DIRECTORY                                      0x20da683fU
#define MENU_LABEL_ASSETS_DIRECTORY                                            0xde1ae8ecU
#define MENU_LABEL_VALUE_ASSETS_DIRECTORY                                      0xc94e558dU
#define MENU_LABEL_CACHE_DIRECTORY                                             0x851dfb8dU
#define MENU_LABEL_VALUE_CACHE_DIRECTORY                                       0x20a7bc9bU
#define MENU_LABEL_JOYPAD_AUTOCONFIG_DIR                                       0x2f4822d8U
#define MENU_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR                                 0x8bb1c2c9U
#define MENU_LABEL_INPUT_AUTODETECT_ENABLE                                     0xb1e07facU
#define MENU_LABEL_VALUE_INPUT_AUTODETECT_ENABLE                               0xf5209bdbU
#define MENU_LABEL_VALUE_DRIVER_SETTINGS                                       0x81cd2d62U
#define MENU_LABEL_VALUE_CORE_SETTINGS                                         0xcddea047U
#define MENU_LABEL_VALUE_CONFIGURATION_SETTINGS                                0x5a1558ceU
#define MENU_LABEL_VALUE_LOGGING_SETTINGS                                      0x902c003dU
#define MENU_LABEL_VALUE_SAVING_SETTINGS                                       0x32fea87eU
#define MENU_LABEL_VALUE_REWIND_SETTINGS                                       0xbff7775fU
#define MENU_LABEL_VALUE_VIDEO_SETTINGS                                        0x9dd23badU
#define MENU_LABEL_RECORDING_SETTINGS                                          0x1a80b313U
#define MENU_LABEL_VALUE_RECORDING_SETTINGS                                    0x1a80b313U
#define MENU_LABEL_SHADER_SETTINGS                                             0xd6657e8dU
#define MENU_LABEL_FONT_SETTINGS                                               0x67571029U
#define MENU_LABEL_AUDIO_SETTINGS                                              0x8f74c888U
#define MENU_LABEL_VALUE_AUDIO_SETTINGS                                        0x8f74c888U
#define MENU_LABEL_VALUE_INPUT_SETTINGS                                        0xddd30846U
#define MENU_LABEL_INPUT_HOTKEY_SETTINGS                                       0x1cb39c19U
#define MENU_LABEL_OVERLAY_SETTINGS                                            0x997b2fd5U
#define MENU_LABEL_VALUE_OVERLAY_SETTINGS                                      0x997b2fd5U
#define MENU_LABEL_ONSCREEN_KEYBOARD_OVERLAY_SETTINGS                          0xa6de9ba6U
#define MENU_LABEL_VALUE_MULTIMEDIA_SETTINGS                                   0x77d23103U
#define MENU_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS                            0x652cf7efU
#define MENU_LABEL_MENU_SETTINGS                                               0x61e4544bU
#define MENU_LABEL_VALUE_MENU_SETTINGS                                         0x61e4544bU
#define MENU_LABEL_UI_SETTINGS                                                 0xf8da6ef4U
#define MENU_LABEL_VALUE_UI_SETTINGS                                           0x76ebdc06U
#define MENU_LABEL_PATCH_SETTINGS                                              0xa78b0986U
#define MENU_LABEL_PLAYLIST_SETTINGS                                           0xdb3e0e07U
#define MENU_LABEL_VALUE_PLAYLIST_SETTINGS                                     0x4d276288U
#define MENU_LABEL_CORE_UPDATER_SETTINGS                                       0x124ad454U
#define MENU_LABEL_VALUE_CORE_UPDATER_SETTINGS                                 0x124ad454U
#define MENU_LABEL_NETWORK_SETTINGS                                            0x8b50d180U
#define MENU_LABEL_VALUE_NETWORK_SETTINGS                                      0x8b50d180U
#define MENU_LABEL_ARCHIVE_SETTINGS                                            0x78e85398U
#define MENU_LABEL_USER_SETTINGS                                               0xcdc9a8f5U
#define MENU_LABEL_VALUE_USER_SETTINGS                                         0xcdc9a8f5U
#define MENU_LABEL_INPUT_USER_1_BINDS                                          0x4d2b4e35U
#define MENU_LABEL_INPUT_USER_2_BINDS                                          0x9a258af6U
#define MENU_LABEL_INPUT_USER_3_BINDS                                          0xe71fc7b7U
#define MENU_LABEL_INPUT_USER_4_BINDS                                          0x341a0478U
#define MENU_LABEL_INPUT_USER_5_BINDS                                          0x81144139U
#define MENU_LABEL_INPUT_USER_6_BINDS                                          0xce0e7dfaU
#define MENU_LABEL_INPUT_USER_7_BINDS                                          0x1b08babbU
#define MENU_LABEL_INPUT_USER_8_BINDS                                          0x6802f77cU
#define MENU_LABEL_INPUT_USER_9_BINDS                                          0xb4fd343dU
#define MENU_LABEL_INPUT_USER_10_BINDS                                         0x70252b05U
#define MENU_LABEL_INPUT_USER_11_BINDS                                         0xbd1f67c6U
#define MENU_LABEL_INPUT_USER_12_BINDS                                         0x0a19a487U
#define MENU_LABEL_INPUT_USER_13_BINDS                                         0x5713e148U
#define MENU_LABEL_INPUT_USER_14_BINDS                                         0xa40e1e09U
#define MENU_LABEL_INPUT_USER_15_BINDS                                         0xf1085acaU
#define MENU_LABEL_INPUT_USER_16_BINDS                                         0x3e02978bU
#define MENU_LABEL_DIRECTORY_SETTINGS                                          0xb817bd2bU
#define MENU_LABEL_VALUE_DIRECTORY_SETTINGS                                    0xb817bd2bU
#define MENU_LABEL_VALUE_PRIVACY_SETTINGS                                      0xce106254U
#define MENU_LABEL_SHADER_APPLY_CHANGES                                        0x4f7306b9U
#define MENU_LABEL_VALUE_SHADER_APPLY_CHANGES                                  0x5ecf945bU
#define MENU_LABEL_SAVE_NEW_CONFIG                                             0xcce9ab72U
#define MENU_LABEL_VALUE_SAVE_NEW_CONFIG                                       0xd49f2c94U
#define MENU_LABEL_ONSCREEN_DISPLAY_SETTINGS                                   0x67571029U
#define MENU_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS                             0x67571029U
#define MENU_LABEL_VALUE_CHEAT_APPLY_CHANGES                                   0x7d5d3489U
#define MENU_LABEL_CHEAT_APPLY_CHANGES                                         0xde88aa27U
#define MENU_LABEL_CUSTOM_BIND                                                 0x1e84b3fcU
#define MENU_LABEL_CUSTOM_BIND_ALL                                             0x79ac14f4U
#define MENU_LABEL_CUSTOM_BIND_DEFAULTS                                        0xe88f7b13U
#define MENU_LABEL_SAVESTATE                                                   0x3a4849b5U
#define MENU_LABEL_LOADSTATE                                                   0xa39eb286U
#define MENU_LABEL_RESUME_CONTENT                                              0xd9f088b0U
#define MENU_LABEL_VALUE_RESUME_CONTENT                                        0xae6e5911U
#define MENU_LABEL_VALUE_RESUME                                                0xce8ac2f6U
#define MENU_LABEL_RESTART_CONTENT                                             0x1ea2e224U
#define MENU_LABEL_RESTART_RETROARCH                                           0xb57d3d73U
#define MENU_LABEL_VALUE_RESTART_RETROARCH                                     0xcc0799f4U
#define MENU_LABEL_VALUE_RESTART_CONTENT                                       0xf23a2e85U
#define MENU_LABEL_TAKE_SCREENSHOT                                             0x6786e867U
#define MENU_LABEL_VALUE_TAKE_SCREENSHOT                                       0xab767128U
#define MENU_LABEL_CONFIGURATIONS                                              0x3e930a50U
#define MENU_LABEL_VALUE_CONFIGURATIONS                                        0xce036cfdU
#define MENU_LABEL_VALUE_CHEAT_FILE_SAVE_AS                                    0xf2498a2dU
#define MENU_LABEL_CHEAT_FILE_SAVE_AS                                          0x1f58dccaU
#define MENU_LABEL_VIDEO_SHADER_PRESET_SAVE_AS                                 0x3d6e5ce5U
#define MENU_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS                           0x405d77b2U
#define MENU_LABEL_VALUE_REMAP_FILE_SAVE_CORE                                  0xd9891572U
#define MENU_LABEL_VALUE_REMAP_FILE_SAVE_GAME                                  0x9c487623U
#define MENU_LABEL_REMAP_FILE_SAVE_CORE                                        0x7c9d4c8fU
#define MENU_LABEL_REMAP_FILE_SAVE_GAME                                        0x7c9f41e0U
#define MENU_LABEL_CONTENT_COLLECTION_LIST                                     0x32d1df83U
#define MENU_LABEL_VALUE_CONTENT_COLLECTION_LIST                               0xdb177ea0U
#define MENU_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE                        0xbae7be3eU
#define MENU_LABEL_OSK_ENABLE                                                  0x8e208498U
#define MENU_LABEL_AUDIO_MUTE                                                  0xe0ca1151U
#define MENU_LABEL_VALUE_AUDIO_MUTE                                            0x5af25952U
#define MENU_LABEL_EXIT_EMULATOR                                               0x86d5d467U
#define MENU_LABEL_COLLECTION                                                  0x5fea5991U
#define MENU_LABEL_USER_LANGUAGE                                               0x33ebaa27U
#define MENU_LABEL_VALUE_USER_LANGUAGE                                         0xd230a5a9U
#define MENU_LABEL_NETPLAY_NICKNAME                                            0x52204787U
#define MENU_LABEL_VALUE_NETPLAY_NICKNAME                                      0x75de3125U
#define MENU_LABEL_VIDEO_VI_WIDTH                                              0x6e4a6d3aU
#define MENU_LABEL_VALUE_VIDEO_VI_WIDTH                                        0x03c07e50U

#define MENU_LABEL_VIDEO_FORCE_ASPECT                                          0x8bbf9329U
#define MENU_LABEL_VALUE_VIDEO_FORCE_ASPECT                                    0xa5590df3U

#define MENU_LABEL_VIDEO_ASPECT_RATIO_AUTO                                     0xa7c31991U
#define MENU_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO                               0x40bd9f87U

#define MENU_LABEL_VIDEO_ASPECT_RATIO_INDEX                                    0x3b01a19aU
#define MENU_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX                              0x9ce00246U

#define MENU_LABEL_VIDEO_VFILTER                                               0x664f8397U
#define MENU_LABEL_VALUE_VIDEO_VFILTER                                         0xd58b0158U

#define MENU_LABEL_VIDEO_GPU_RECORD                                            0xb6059a65U
#define MENU_LABEL_VALUE_VIDEO_GPU_RECORD                                      0x2241deb7U

#define MENU_LABEL_RECORD_USE_OUTPUT_DIRECTORY                                 0x8343eff4U
#define MENU_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY                           0x8282ff38U

#define MENU_LABEL_RECORD_CONFIG                                               0x11c3daf9U
#define MENU_LABEL_VALUE_RECORD_CONFIG                                         0xe9a8a33aU

#define MENU_LABEL_RECORD_PATH                                                 0x016d7afaU
#define MENU_LABEL_VALUE_RECORD_PATH                                           0xeb15a0f1U

#define MENU_LABEL_VIDEO_POST_FILTER_RECORD                                    0xa7b6e724U
#define MENU_LABEL_VALUE_VIDEO_POST_FILTER_RECORD                              0x1362eaf7U

#define MENU_LABEL_RECORD_ENABLE                                               0x1654e22aU
#define MENU_LABEL_VALUE_RECORD_ENABLE                                         0xee39aa6bU

#define MENU_VALUE_SECONDS                                                     0x8b0028d4U
#define MENU_VALUE_STATUS                                                      0xd1e57929U

#define MENU_LABEL_VALUE_CORE_INFO_CORE_NAME                                   0x2a031110U
#define MENU_LABEL_VALUE_CORE_INFO_CORE_LABEL                                  0x6a40d38fU
#define MENU_LABEL_VALUE_CORE_INFO_SYSTEM_NAME                                 0xaff88f0cU
#define MENU_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER                         0x03be7018U
#define MENU_LABEL_VALUE_CORE_INFO_CATEGORIES                                  0xac3a39edU
#define MENU_LABEL_VALUE_CORE_INFO_AUTHORS                                     0x7167c44dU
#define MENU_LABEL_VALUE_CORE_INFO_PERMISSIONS                                 0x25d21423U
#define MENU_LABEL_VALUE_CORE_INFO_LICENSES                                    0x019b14bdU
#define MENU_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS                        0xb8ff231cU
#define MENU_LABEL_VALUE_CORE_INFO_FIRMWARE                                    0x9ba2e164U
#define MENU_LABEL_VALUE_CORE_INFO_CORE_NOTES                                  0x6a6cfe78U

#define MENU_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE                                0xbab040f0U
#define MENU_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION                               0x333df14cU
#define MENU_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES                              0x9515e369U
#define MENU_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER                       0x35817c25U
#define MENU_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME                             0x45d9b0e3U
#define MENU_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS                               0x412e46a4U
#define MENU_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL                         0xcc6a17ebU
#define MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE                              0xcda91ae0U
#define MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE                    0x0ed5776cU
#define MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING                     0x2b226d62U
#define MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED                      0x75abb52dU
#define MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING                  0x48c36402U
#define MENU_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER                      0x32f901e9U
#define MENU_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH                   0x86a53f14U
#define MENU_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT                  0x3809e50dU
#define MENU_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI                        0xaf2540b8U
#define MENU_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT                        0x3e91f988U
#define MENU_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT                           0x9e893c21U
#define MENU_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT                     0x91a47d95U
#define MENU_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT             0x9c9c8e3eU
#define MENU_LABEL_VALUE_SYSTEM_INFO_NETWORK_REMOTE_SUPPORT                    0x1a817f5bU
#define MENU_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT                             0x89849204U
#define MENU_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT                              0xe1dcea36U
#define MENU_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT                               0xf9bc2a42U
#define MENU_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT                              0x3c2d6134U
#define MENU_LABEL_VALUE_SYSTEM_INFO_VULKAN_SUPPORT                            0x6a06e373U
#define MENU_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT                            0xa4d164a4U
#define MENU_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT                          0xe2e627dcU
#define MENU_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT                         0x282bf995U
#define MENU_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT                               0x229cb16aU
#define MENU_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT                              0xe34a0833U
#define MENU_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT                            0x9b01b08eU
#define MENU_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT                               0xbac9f417U
#define MENU_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT                               0xfea303f9U
#define MENU_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT                           0xd590cd8fU
#define MENU_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT                            0x79dc360eU
#define MENU_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT                              0xe58b1160U
#define MENU_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT                               0x504eed34U
#define MENU_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT                            0x8c91fddeU
#define MENU_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT                            0xd5503230U
#define MENU_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT                            0x4c20387aU
#define MENU_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT                         0xb7793da5U
#define MENU_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT                              0x2f50e7f8U
#define MENU_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT                        0xe8d32f1aU
#define MENU_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT                            0x0e9d11acU
#define MENU_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT                           0xb1f1735bU
#define MENU_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT                              0xf961f590U
#define MENU_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT                              0xf1fc48e9U
#define MENU_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT                             0xcee4aad3U
#define MENU_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT                                0xef0baba9U
#define MENU_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT                              0x0c981cb1U
#define MENU_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT                              0x3d8b7a12U
#define MENU_LABEL_VALUE_SYSTEM_INFO_LIBXML2_SUPPORT                           0xdce2d3f9U
#define MENU_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT                         0x98b4d864U
#define MENU_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT                               0x896726b6U
#define MENU_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT                            0xb9a9fd34U
#define MENU_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT                          0x7d248acdU
#define MENU_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT                          0x00f65983U
#define MENU_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT                           0x50a8ce7cU
#define MENU_LABEL_VALUE_SYSTEM_INFO_PYTHON_SUPPORT                            0x71cc9801U
#define MENU_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT                              0x793c2547U
#define MENU_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT                            0x7dc8b560U

#define MENU_LABEL_VALUE_YES                                                   0x957cbc05U
#define MENU_LABEL_VALUE_NO                                                    0x521b0c11U

#define MENU_VALUE_BACK                                                        0x7c825df6U
#define MENU_VALUE_DISABLED                                                    0xe326e01dU
#define MENU_VALUE_PORT                                                        0x7c8ad52aU

#define MENU_VALUE_LEFT_ANALOG                                                 0xd168d0e2U
#define MENU_VALUE_RIGHT_ANALOG                                                0xf9244335U

#define MENU_VALUE_LANG_ENGLISH                                                0xcb4e554fU
#define MENU_VALUE_LANG_JAPANESE                                               0xfde6f60cU
#define MENU_VALUE_LANG_FRENCH                                                 0xb3704d9bU
#define MENU_VALUE_LANG_SPANISH                                                0x053c7edbU
#define MENU_VALUE_LANG_GERMAN                                                 0xb4e1541fU
#define MENU_VALUE_LANG_ITALIAN                                                0x0cc9a6c7U
#define MENU_VALUE_LANG_DUTCH                                                  0x0d0fa55dU
#define MENU_VALUE_LANG_PORTUGUESE                                             0x2a19df58U
#define MENU_VALUE_LANG_RUSSIAN                                                0xc53481eaU
#define MENU_VALUE_LANG_KOREAN                                                 0xbeeac9a5U
#define MENU_VALUE_LANG_CHINESE_TRADITIONAL                                    0x43f172d0U
#define MENU_VALUE_LANG_CHINESE_SIMPLIFIED                                     0x1ae5ee5bU
#define MENU_VALUE_LANG_ESPERANTO                                              0x1a933a76U
#define MENU_VALUE_LANG_POLISH                                                 0xca915dd4U

#define MENU_VALUE_NONE                                                        0x7c89bbd5U

#define MENU_LABEL_VALUE_NO_INFORMATION_AVAILABLE                              0xbae2c7f6U

#define MENU_LABEL_VALUE_INPUT_USER_BINDS                                      0x75fda711U

#define MENU_LABEL_USE_BUILTIN_IMAGE_VIEWER                                    0x5203b5bbU
#define MENU_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER                              0x1ab45d3eU

#define MENU_LABEL_OVERLAY_AUTOLOAD_PREFERRED                                  0xc9298cbdU
#define MENU_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED                            0x0e27e33fU

#define MENU_LABEL_OPEN_ARCHIVE                                                0x78c0ca58U
#define MENU_LABEL_OPEN_ARCHIVE_DETECT_CORE                                    0x92442638U
#define MENU_LABEL_LOAD_ARCHIVE_DETECT_CORE                                    0x681f2f46U
#define MENU_LABEL_LOAD_ARCHIVE                                                0xc3834e66U

#define MENU_LABEL_VALUE_OPEN_ARCHIVE                                          0x96da22b9U
#define MENU_LABEL_VALUE_LOAD_ARCHIVE                                          0xe19ca6c7U

#define MENU_LABEL_VALUE_WHAT_IS_A_CORE_DESC                                   0xc832957eU

#define MENU_LABEL_HELP_CHANGE_VIRTUAL_GAMEPAD                                 0x6e66ef07U
#define MENU_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD                           0x27ed0204U
#define MENU_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD_DESC                      0x9d0e79dbU

#define MENU_LABEL_HELP_AUDIO_VIDEO_TROUBLESHOOTING                            0xd44d395cU
#define MENU_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING                      0xd0e5c3ffU
#define MENU_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING_DESC                 0x60031d7aU

#define MENU_LABEL_HELP_SCANNING_CONTENT                                       0x1dec52b8U
#define MENU_LABEL_VALUE_HELP_SCANNING_CONTENT                                 0x74b36f11U
#define MENU_LABEL_VALUE_HELP_SCANNING_CONTENT_DESC                            0xac947056U

#define MENU_LABEL_SAVE_CURRENT_CONFIG                                         0x8840ba8bU
#define MENU_LABEL_VALUE_SAVE_CURRENT_CONFIG                                   0x9a1eb42dU

#define MENU_LABEL_INPUT_SMALL_KEYBOARD_ENABLE                                 0xe6736fc3U
#define MENU_LABEL_VALUE_INPUT_SMALL_KEYBOARD_ENABLE                           0xc5eefd76U

#define MENU_LABEL_INPUT_ICADE_ENABLE                                          0xcd534dd0U
#define MENU_LABEL_VALUE_INPUT_ICADE_ENABLE                                    0x67b18ee2U

const char *menu_hash_to_str_de(uint32_t hash);
int menu_hash_get_help_de(uint32_t hash, char *s, size_t len);

const char *menu_hash_to_str_es(uint32_t hash);
int menu_hash_get_help_es(uint32_t hash, char *s, size_t len);

const char *menu_hash_to_str_fr(uint32_t hash);
int menu_hash_get_help_fr(uint32_t hash, char *s, size_t len);

const char *menu_hash_to_str_it(uint32_t hash);
int menu_hash_get_help_it(uint32_t hash, char *s, size_t len);

const char *menu_hash_to_str_nl(uint32_t hash);
int menu_hash_get_help_nl(uint32_t hash, char *s, size_t len);

const char *menu_hash_to_str_pl(uint32_t hash);
int menu_hash_get_help_pl(uint32_t hash, char *s, size_t len);

const char *menu_hash_to_str_pt(uint32_t hash);
int menu_hash_get_help_pt(uint32_t hash, char *s, size_t len);

const char *menu_hash_to_str_eo(uint32_t hash);
int menu_hash_get_help_eo(uint32_t hash, char *s, size_t len);

const char *menu_hash_to_str_us(uint32_t hash);
int menu_hash_get_help_us(uint32_t hash, char *s, size_t len);

const char *menu_hash_to_str(uint32_t hash);
int menu_hash_get_help(uint32_t hash, char *s, size_t len);

uint32_t menu_hash_calculate(const char *s);

#ifdef __cplusplus
}
#endif

#endif
