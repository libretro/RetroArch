/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include <compat/strl.h>
#include <string/stdstring.h>

#include "../msg_hash.h"
#include "../verbosity.h"

#ifdef RARCH_INTERNAL
#include "../configuration.h"

#if defined(_MSC_VER) && !defined(_XBOX)
#if (_MSC_VER >= 1700 && _MSC_VER < 1900)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif
#pragma warning(disable:4566)
#endif

int msg_hash_get_help_ko_enum(enum msg_hash_enums msg, char *s, size_t len)
{
    settings_t *settings = config_get_ptr();

    if (msg <= MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_END &&
        msg >= MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN)
    {
       unsigned idx = msg - MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN;

       switch (idx)
       {
          case RARCH_FAST_FORWARD_KEY:
             snprintf(s, len,
                   "빨리감기와 보통속도 사이를\n"
                   "전환합니다."
                   );
             break;
          case RARCH_FAST_FORWARD_HOLD_KEY:
             snprintf(s, len,
                   "빨리감기를 실행합니다. \n"
                   " \n"
                   "버튼을 놓으면 빨리감기를 중지합니다."
                   );
             break;
          case RARCH_SLOWMOTION_KEY:
             snprintf(s, len,
                   "슬로우모션을 전환합니다.");
             break;
          case RARCH_SLOWMOTION_HOLD_KEY:
             snprintf(s, len,
                   "슬로우모션을 실행합니다.");
             break;
          case RARCH_PAUSE_TOGGLE:
             snprintf(s, len,
                   "일시정지/해제 상태를 전환합니다.");
             break;
          case RARCH_FRAMEADVANCE:
             snprintf(s, len,
                   "컨텐츠 일시정지시 프레임을 진행합니다.");
             break;
          case RARCH_SHADER_NEXT:
             snprintf(s, len,
                   "디렉토리 안의 다음 쉐이더를 적용합니다.");
             break;
          case RARCH_SHADER_PREV:
             snprintf(s, len,
                   "디렉토리 안의 이전 쉐이더를 적용합니다.");
             break;
          case RARCH_CHEAT_INDEX_PLUS:
          case RARCH_CHEAT_INDEX_MINUS:
          case RARCH_CHEAT_TOGGLE:
             snprintf(s, len,
                   "치트");
             break;
          case RARCH_RESET:
             snprintf(s, len,
                   "컨텐츠를 초기화합니다.");
             break;
          case RARCH_SCREENSHOT:
             snprintf(s, len,
                   "스크린샷을 촬영합니다.");
             break;
          case RARCH_MUTE:
             snprintf(s, len,
                   "음소거/음소거 해제.");
             break;
          case RARCH_OSK:
             snprintf(s, len,
                   "온스크린 키보드를 전환합니다.");
             break;
          case RARCH_FPS_TOGGLE:
           snprintf(s, len,
                   "FPS 표시를 전환합니다.");
             break;
          case RARCH_SEND_DEBUG_INFO:
             snprintf(s, len,
                   "기기 및 RetroArch 설정의 분적 정보를 분석을 위해 서버에 보냅니다.");
             break;
          case RARCH_NETPLAY_HOST_TOGGLE:
             snprintf(s, len,
                   "넷플레 호스트 켜기/끄기.");
             break;
          case RARCH_NETPLAY_GAME_WATCH:
             snprintf(s, len,
                   "넷플레이 플레이/관전 모드를 전환합니다.");
             break;
          case RARCH_ENABLE_HOTKEY:
             snprintf(s, len,
                   "추가 핫키를 사용합니다. \n"
                   " \n"
                   "이 핫키가 설정되면 키보드, 조이스틱 버튼,\n"
                   "조이스틱 축등 모든 핫키가 이 키와 \n"
                   "함께 눌렸을 때에만 사용가능하게 됩니다. \n"
                   " \n"
                   " \n"
                   "또는 키보드상의 모든 핫키가 \n"
                   "사용자에 의해 차단될수 있습니다.");
             break;
          case RARCH_VOLUME_UP:
             snprintf(s, len,
                  "오디오 볼륨을 증가합니다.");
             break;
          case RARCH_VOLUME_DOWN:
             snprintf(s, len,
                  "오디오 볼륨을 감소합니다.");
             break;
          case RARCH_OVERLAY_NEXT:
             snprintf(s, len,
                  "다음 오버레이로 전환합니다. 화면을 \n"
                  "덮어 씌웁니다.");
             break;
          case RARCH_DISK_EJECT_TOGGLE:
             snprintf(s, len,
                  "디스크 꺼내기를 전환합니다. \n"
                  " \n"
                  "다중 디스크 컨텐츠에 사용됩니다. ");
             break;
          case RARCH_DISK_NEXT:
          case RARCH_DISK_PREV:
             snprintf(s, len,
                  "디스크 이미지간 탐색합니다. \n"
                  "디스크 이미지를 꺼낸 후에 사용하세요. \n"
                  " \n"
                  "꺼내기 전환을 다시 눌러 완료합니다.");
             break;
          case RARCH_GRAB_MOUSE_TOGGLE:
             snprintf(s, len,
                  "마우스 고정을 전환합니다. \n"
                  " \n"
                  "마우스 고정이 활성화 되면, RetroArch가 마우스를 \n"
                  "숨기고 창 안에 고정시켜 마우스 입력을 원활하게 \n"
                  "끔 합니다.");
             break;
          case RARCH_GAME_FOCUS_TOGGLE:
             snprintf(s, len,
                  "Toggles game focus.\n"
                  " \n"
                  "When a game has focus, RetroArch will both disable \n"
                  "hotkeys and keep/warp the mouse pointer inside the window.");
             break;
          case RARCH_MENU_TOGGLE:
             snprintf(s, len, "메뉴를 전환합니다.");
             break;
          case RARCH_LOAD_STATE_KEY:
             snprintf(s, len,
                   "상태 불러오기");
             break;
          case RARCH_FULLSCREEN_TOGGLE_KEY:
             snprintf(s, len,
                   "전체화면을 전환합니다.");
             break;
          case RARCH_QUIT_KEY:
             snprintf(s, len,
                   "Key to exit RetroArch cleanly. \n"
                   " \n"
                   "Killing it in any hard way (SIGKILL, etc.) will \n"
                   "terminate RetroArch without saving RAM, etc."
#ifdef __unix__
                   "\nOn Unix-likes, SIGINT/SIGTERM allows a clean \n"
                   "deinitialization."
#endif
                   "");
             break;
          case RARCH_STATE_SLOT_PLUS:
          case RARCH_STATE_SLOT_MINUS:
             snprintf(s, len,
                   "State slots. \n"
                   " \n"
                   "With slot set to 0, save state name is \n"
                   "*.state (or whatever defined on commandline). \n"
                   " \n"
                   "When slot is not 0, path will be <path><d>, \n"
                   "where <d> is slot number.");
             break;
          default:
             if (string_is_empty(s))
                strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
             break;
       }

       return 0;
    }

    switch (msg)
    {
        case MENU_ENUM_LABEL_ACCOUNTS_RETRO_ACHIEVEMENTS:
            snprintf(s, len, "Login details for your \n"
                    "Retro Achievements account. \n"
                    " \n"
                    "Visit retroachievements.org and sign up \n"
                    "for a free account. \n"
                    " \n"
                    "After you are done registering, you need \n"
                    "to input the username and password into \n"
                    "RetroArch.");
            break;
        case MENU_ENUM_LABEL_USER_LANGUAGE:
            snprintf(s, len, "Localizes the menu and all onscreen messages \n"
                    "according to the language you have selected \n"
                    "here. \n"
                    " \n"
                    "Requires a restart for the changes \n"
                    "to take effect. \n"
                    " \n"
                    "Note: not all languages might be currently \n"
                    "implemented. \n"
                    " \n"
                    "In case a language is not implemented, \n"
                    "we fallback to English.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_CONFIG:
            snprintf(s, len, "Configuration file.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_COMPRESSED_ARCHIVE:
            snprintf(s, len, "Compressed archive file.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_RECORD_CONFIG:
            snprintf(s, len, "Recording configuration file.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_CURSOR:
            snprintf(s, len, "Database cursor file.");
            break;
        case MENU_ENUM_LABEL_FILE_CONFIG:
            snprintf(s, len, "Configuration file.");
            break;
        case MENU_ENUM_LABEL_SCAN_THIS_DIRECTORY:
            snprintf(s, len,
                     "Select this to scan the current directory \n"
                             "for content.");
            break;
        case MENU_ENUM_LABEL_USE_THIS_DIRECTORY:
            snprintf(s, len,
                     "Select this to set this as the directory.");
            break;
        case MENU_ENUM_LABEL_DUMMY_ON_CORE_SHUTDOWN:
            snprintf(s, len,
                     "Some cores might have \n"
                             "a shutdown feature. \n"
                             " \n"
                             "If this option is left disabled, \n"
                             "selecting the shutdown procedure \n"
                             "would trigger RetroArch being shut \n"
                             "down. \n"
                             " \n"
                             "Enabling this option will load a \n"
                             "dummy core instead so that we remain \n"
                             "inside the menu and RetroArch won't \n"
                             "shutdown.");
            break;
        case MENU_ENUM_LABEL_CHECK_FOR_MISSING_FIRMWARE:
            snprintf(s, len,
                     "Some cores might need \n"
                             "firmware or bios files. \n"
                             " \n"
                             "If this option is disabled, \n"
                             "it will try to load even if such \n"
                             "firmware is missing. \n");
            break;
        case MENU_ENUM_LABEL_PARENT_DIRECTORY:
            snprintf(s, len,
                     "Go back to the parent directory.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_OPEN_UWP_PERMISSIONS:
            snprintf(s, len,
                     "Open Windows permission settings to enable \n"
                     "the broadFileSystemAccess capability.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_SHADER_PRESET:
            snprintf(s, len,
                     "Shader preset file.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_SHADER:
            snprintf(s, len,
                     "Shader file.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_REMAP:
            snprintf(s, len,
                     "Remap controls file.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_CHEAT:
            snprintf(s, len,
                     "Cheat file.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_OVERLAY:
            snprintf(s, len,
                     "Overlay file.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_RDB:
            snprintf(s, len,
                     "Database file.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_FONT:
            snprintf(s, len,
                     "TrueType font file.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_PLAIN_FILE:
            snprintf(s, len,
                     "Plain file.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_MOVIE_OPEN:
            snprintf(s, len,
                     "Video. \n"
                             " \n"
                             "Select it to open this file with the \n"
                             "video player.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_MUSIC_OPEN:
            snprintf(s, len,
                     "Music. \n"
                             " \n"
                             "Select it to open this file with the \n"
                             "music player.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_IMAGE:
            snprintf(s, len,
                     "Image file.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_IMAGE_OPEN_WITH_VIEWER:
            snprintf(s, len,
                     "Image. \n"
                             " \n"
                             "Select it to open this file with the \n"
                             "image viewer.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION:
            snprintf(s, len,
                     "Libretro core. \n"
                             " \n"
                             "Selecting this will associate this core \n"
                             "to the game.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_CORE:
            snprintf(s, len,
                     "Libretro core. \n"
                             " \n"
                             "Select this file to have RetroArch load this core.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY:
            snprintf(s, len,
                     "Directory. \n"
                             " \n"
                             "Select it to open this directory.");
            break;
        case MENU_ENUM_LABEL_CACHE_DIRECTORY:
            snprintf(s, len,
                     "Cache Directory. \n"
                             " \n"
                             "Content decompressed by RetroArch will be \n"
                             "temporarily extracted to this directory.");
            break;
        case MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR:
            snprintf(s, len,
                     "Influence how input polling is done inside \n"
                             "RetroArch. \n"
                             " \n"
                             "Early  - Input polling is performed before \n"
                             "the frame is processed. \n"
                             "Normal - Input polling is performed when \n"
                             "polling is requested. \n"
                             "Late   - Input polling is performed on \n"
                             "first input state request per frame.\n"
                             " \n"
                             "Setting it to 'Early' or 'Late' can result \n"
                             "in less latency, \n"
                             "depending on your configuration.\n\n"
                             "Will be ignored when using netplay."
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_X:
        case MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_Y:
            snprintf(s, len,
                     "Offset for where messages will be placed \n"
                             "onscreen. Values are in range [0.0, 1.0].");
            break;
        case MENU_ENUM_LABEL_CORE_LIST:
            snprintf(s, len,
                     "Load Core. \n"
                             " \n"
                             "Browse for a libretro core \n"
                             "implementation. Where the browser \n"
                             "starts depends on your Core Directory \n"
                             "path. If blank, it will start in root. \n"
                             " \n"
                             "If Core Directory is a directory, the menu \n"
                             "will use that as top folder. If Core \n"
                             "Directory is a full path, it will start \n"
                             "in the folder where the file is.");
            break;
        case MENU_ENUM_LABEL_VALUE_MENU_ENUM_CONTROLS_PROLOG:
            snprintf(s, len,
                     "메뉴를 조작하려면 게임패드 또는 \n"
                     "키보드를 통해 다음의 조작 방법을\n"
                     "사용 할 수 있습니다: \n"
                     " \n"
            );
            break;
        case MENU_ENUM_LABEL_WELCOME_TO_RETROARCH:
            snprintf(s, len,
                     "RetroArch에 오신걸 환영합니다\n"
            );
            break;
        case MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING_DESC: {
            /* Work around C89 limitations */
            char u[501];
            const char *t =
                    "RetroArch relies on an unique form of\n"
                            "audio/video synchronization where it needs to be\n"
                            "calibrated against the refresh rate of your\n"
                            "display for best performance results.\n"
                            " \n"
                            "If you experience any audio crackling or video\n"
                            "tearing, usually it means that you need to\n"
                            "calibrate the settings. Some choices below:\n"
                            " \n";
            snprintf(u, sizeof(u), /* can't inline this due to the printf arguments */
                     "a) Go to '%s' -> '%s', and enable\n"
                             "'Threaded Video'. Refresh rate will not matter\n"
                             "in this mode, framerate will be higher,\n"
                             "but video might be less smooth.\n"
                             "b) Go to '%s' -> '%s', and look at\n"
                             "'%s'. Let it run for\n"
                             "2048 frames, then press 'OK'.",
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO));
            strlcpy(s, t, len);
            strlcat(s, u, len);
        }
            break;
        case MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT_DESC:
            snprintf(s, len,
                     "To scan for content, go to '%s' and\n"
                             "select either '%s' or %s'.\n"
                             "\n"
                             "Files will be compared to database entries.\n"
                             "If there is a match, it will add an entry\n"
                             "to a playlist.\n"
                             "\n"
                             "You can then easily access this content by\n"
                             "going to '%s' ->\n"
                             "'%s'\n"
                             "instead of having to go through the\n"
                             "file browser every time.\n"
                             "\n"
                             "NOTE: Content for some cores might still not be\n"
                             "scannable.",
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_FILE),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB)
            );
            break;
        case MENU_ENUM_LABEL_VALUE_EXTRACTING_PLEASE_WAIT:
            snprintf(s, len,
                     "Welcome to RetroArch\n"
                             "\n"
                             "Extracting assets, please wait.\n"
                             "This might take a while...\n"
            );
            break;
        case MENU_ENUM_LABEL_INPUT_DRIVER:
            {
               const char *lbl = settings ? settings->arrays.input_driver : NULL;

               if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_INPUT_DRIVER_UDEV)))
                     snprintf(s, len,
                           "udev Input driver. \n"
                           " \n"
                           "It uses the recent evdev joypad API \n"
                           "for joystick support. It supports \n"
                           "hotplugging and force feedback. \n"
                           " \n"
                           "The driver reads evdev events for keyboard \n"
                           "support. It also supports keyboard callback, \n"
                           "mice and touchpads. \n"
                           " \n"
                           "By default in most distros, /dev/input nodes \n"
                           "are root-only (mode 600). You can set up a udev \n"
                           "rule which makes these accessible to non-root."
                           );
               else if (string_is_equal(lbl,
                        msg_hash_to_str(MENU_ENUM_LABEL_INPUT_DRIVER_LINUXRAW)))
                     snprintf(s, len,
                           "linuxraw Input driver. \n"
                           " \n"
                           "This driver requires an active TTY. Keyboard \n"
                           "events are read directly from the TTY which \n"
                           "makes it simpler, but not as flexible as udev. \n" "Mice, etc, are not supported at all. \n"
                           " \n"
                           "This driver uses the older joystick API \n"
                           "(/dev/input/js*).");
               else
                     snprintf(s, len,
                           "Input driver.\n"
                           " \n"
                           "Depending on video driver, it might \n"
                           "force a different input driver.");
            }
            break;
        case MENU_ENUM_LABEL_LOAD_CONTENT_LIST:
            snprintf(s, len,
                     "Load Content. \n"
                             "Browse for content. \n"
                             " \n"
                             "To load content, you need a \n"
                             "'Core' to use, and a content file. \n"
                             " \n"
                             "To control where the menu starts \n"
                             "to browse for content, set  \n"
                             "'File Browser Directory'. \n"
                             "If not set, it will start in root. \n"
                             " \n"
                             "The browser will filter out \n"
                             "extensions for the last core set \n"
                             "in 'Load Core', and use that core \n"
                             "when content is loaded."
            );
            break;
        case MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY:
            snprintf(s, len,
                     "Loading content from history. \n"
                             " \n"
                             "As content is loaded, content and libretro \n"
                             "core combinations are saved to history. \n"
                             " \n"
                             "The history is saved to a file in the same \n"
                             "directory as the RetroArch config file. If \n"
                             "no config file was loaded in startup, history \n"
                             "will not be saved or loaded, and will not exist \n"
                             "in the main menu."
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_DRIVER:
            {
               const char *video_driver = settings->arrays.video_driver;

               snprintf(s, len,
                     "Current Video driver.");

               if (string_is_equal(video_driver, "gl"))
               {
                  snprintf(s, len,
                        "OpenGL Video driver. \n"
                        " \n"
                        "This driver allows libretro GL cores to  \n"
                        "be used in addition to software-rendered \n"
                        "core implementations.\n"
                        " \n"
                        "Performance for software-rendered and \n"
                        "libretro GL core implementations is \n"
                        "dependent on your graphics card's \n"
                        "underlying GL driver).");
               }
               else if (string_is_equal(video_driver, "sdl2"))
               {
                  snprintf(s, len,
                        "SDL 2 Video driver.\n"
                        " \n"
                        "This is an SDL 2 software-rendered video \n"
                        "driver.\n"
                        " \n"
                        "Performance for software-rendered libretro \n"
                        "core implementations is dependent \n"
                        "on your platform SDL implementation.");
               }
               else if (string_is_equal(video_driver, "sdl1"))
               {
                  snprintf(s, len,
                        "SDL Video driver.\n"
                        " \n"
                        "This is an SDL 1.2 software-rendered video \n"
                        "driver.\n"
                        " \n"
                        "Performance is considered to be suboptimal. \n"
                        "Consider using it only as a last resort.");
               }
               else if (string_is_equal(video_driver, "d3d"))
               {
                  snprintf(s, len,
                        "Direct3D Video driver. \n"
                        " \n"
                        "Performance for software-rendered cores \n"
                        "is dependent on your graphic card's \n"
                        "underlying D3D driver).");
               }
               else if (string_is_equal(video_driver, "exynos"))
               {
                  snprintf(s, len,
                        "Exynos-G2D Video Driver. \n"
                        " \n"
                        "This is a low-level Exynos video driver. \n"
                        "Uses the G2D block in Samsung Exynos SoC \n"
                        "for blit operations. \n"
                        " \n"
                        "Performance for software rendered cores \n"
                        "should be optimal.");
               }
               else if (string_is_equal(video_driver, "drm"))
               {
                  snprintf(s, len,
                        "Plain DRM Video Driver. \n"
                        " \n"
                        "This is a low-level video driver using. \n"
                        "libdrm for hardware scaling using \n"
                        "GPU overlays.");
               }
               else if (string_is_equal(video_driver, "sunxi"))
               {
                  snprintf(s, len,
                        "Sunxi-G2D Video Driver. \n"
                        " \n"
                        "This is a low-level Sunxi video driver. \n"
                        "Uses the G2D block in Allwinner SoCs.");
               }
            }
            break;
        case MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER:
            {
               const char *lbl = settings ? settings->arrays.audio_resampler : NULL;

               if (string_is_equal(lbl, msg_hash_to_str(
                           MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER_SINC)))
                  strlcpy(s,
                        "Windowed SINC implementation.", len);
               else if (string_is_equal(lbl, msg_hash_to_str(
                           MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER_CC)))
                  strlcpy(s,
                        "Convoluted Cosine implementation.", len);
               else if (string_is_empty(s))
                  strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
            }
            break;

        case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET:
            snprintf(s, len,
                     "Load Shader Preset. \n"
                             " \n"
                             " Load a shader preset directly. \n"
                             "The menu shader menu is updated accordingly. \n"
                             " \n"
                             "If the CGP uses scaling methods which are not \n"
                             "simple, (i.e. source scaling, same scaling \n"
                             "factor for X/Y), the scaling factor displayed \n"
                             "in the menu might not be correct."
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_SCALE_PASS:
            snprintf(s, len,
                     "Scale for this pass. \n"
                             " \n"
                             "The scale factor accumulates, i.e. 2x \n"
                             "for first pass and 2x for second pass \n"
                             "will give you a 4x total scale. \n"
                             " \n"
                             "If there is a scale factor for last \n"
                             "pass, the result is stretched to \n"
                             "screen with the filter specified in \n"
                             "'Default Filter'. \n"
                             " \n"
                             "If 'Don't Care' is set, either 1x \n"
                             "scale or stretch to fullscreen will \n"
                             "be used depending if it's not the last \n"
                             "pass or not."
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_NUM_PASSES:
            snprintf(s, len,
                     "Shader Passes. \n"
                             " \n"
                             "RetroArch allows you to mix and match various \n"
                             "shaders with arbitrary shader passes, with \n"
                             "custom hardware filters and scale factors. \n"
                             " \n"
                             "This option specifies the number of shader \n"
                             "passes to use. If you set this to 0, and use \n"
                             "Apply Shader Changes, you use a 'blank' shader. \n"
                             " \n"
                             "The Default Filter option will affect the \n"
                             "stretching filter.");
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_PASS:
            snprintf(s, len,
                     "Path to shader. \n"
                             " \n"
                             "All shaders must be of the same \n"
                             "type (i.e. CG, GLSL or HLSL). \n"
                             " \n"
                             "Set Shader Directory to set where \n"
                             "the browser starts to look for \n"
                             "shaders."
            );
            break;
        case MENU_ENUM_LABEL_CONFIGURATION_SETTINGS:
            snprintf(s, len,
                     "Determines how configuration files \n"
                             "are loaded and prioritized.");
            break;
        case MENU_ENUM_LABEL_CONFIG_SAVE_ON_EXIT:
            snprintf(s, len,
                     "Saves config to disk on exit.\n"
                             "Useful for menu as settings can be\n"
                             "modified. Overwrites the config.\n"
                             " \n"
                             "#include's and comments are not \n"
                             "preserved. \n"
                             " \n"
                             "By design, the config file is \n"
                             "considered immutable as it is \n"
                             "likely maintained by the user, \n"
                             "and should not be overwritten \n"
                             "behind the user's back."
#if defined(RARCH_CONSOLE) || defined(RARCH_MOBILE)
            "\nThis is not not the case on \n"
            "consoles however, where \n"
            "looking at the config file \n"
            "manually isn't really an option."
#endif
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_FILTER_PASS:
            snprintf(s, len,
                     "Hardware filter for this pass. \n"
                             " \n"
                             "If 'Don't Care' is set, 'Default \n"
                             "Filter' will be used."
            );
            break;
        case MENU_ENUM_LABEL_AUTOSAVE_INTERVAL:
            snprintf(s, len,
                     "Autosaves the non-volatile SRAM \n"
                             "at a regular interval.\n"
                             " \n"
                             "This is disabled by default unless set \n"
                             "otherwise. The interval is measured in \n"
                             "seconds. \n"
                             " \n"
                             "A value of 0 disables autosave.");
            break;
        case MENU_ENUM_LABEL_INPUT_BIND_DEVICE_TYPE:
            snprintf(s, len,
                     "Input Device Type. \n"
                             " \n"
                             "Picks which device type to use. This is \n"
                             "relevant for the libretro core itself."
            );
            break;
        case MENU_ENUM_LABEL_LIBRETRO_LOG_LEVEL:
            snprintf(s, len,
                     "Sets log level for libretro cores \n"
                             "(GET_LOG_INTERFACE). \n"
                             " \n"
                             " If a log level issued by a libretro \n"
                             " core is below libretro_log level, it \n"
                             " is ignored.\n"
                             " \n"
                             " DEBUG logs are always ignored unless \n"
                             " verbose mode is activated (--verbose).\n"
                             " \n"
                             " DEBUG = 0\n"
                             " INFO  = 1\n"
                             " WARN  = 2\n"
                             " ERROR = 3"
            );
            break;
        case MENU_ENUM_LABEL_STATE_SLOT_INCREASE:
        case MENU_ENUM_LABEL_STATE_SLOT_DECREASE:
            snprintf(s, len,
                     "State slots.\n"
                             " \n"
                             " With slot set to 0, save state name is *.state \n"
                             " (or whatever defined on commandline).\n"
                             "When slot is != 0, path will be (path)(d), \n"
                             "where (d) is slot number.");
            break;
        case MENU_ENUM_LABEL_SHADER_APPLY_CHANGES:
            snprintf(s, len,
                     "Apply Shader Changes. \n"
                             " \n"
                             "After changing shader settings, use this to \n"
                             "apply changes. \n"
                             " \n"
                             "Changing shader settings is a somewhat \n"
                             "expensive operation so it has to be \n"
                             "done explicitly. \n"
                             " \n"
                             "When you apply shaders, the menu shader \n"
                             "settings are saved to a temporary file (either \n"
                             "menu.cgp or menu.glslp) and loaded. The file \n"
                             "persists after RetroArch exits. The file is \n"
                             "saved to Shader Directory."
            );
            break;
        case MENU_ENUM_LABEL_SHADER_WATCH_FOR_CHANGES:
            snprintf(s, len,
                     "Watch shader files for new changes. \n"
                     " \n"
                     "After saving changes to a shader on disk, \n"
                     "it will automatically be recompiled \n"
                     "and applied to the running content."
            );
            break;
        case MENU_ENUM_LABEL_DISK_NEXT:
            snprintf(s, len,
                     "Cycles through disk images. Use after \n"
                             "ejecting. \n"
                             " \n"
                             " Complete by toggling eject again.");
            break;
        case MENU_ENUM_LABEL_VIDEO_FILTER:
#ifdef HAVE_FILTERS_BUILTIN
            snprintf(s, len,
                  "CPU-based video filter.");
#else
            snprintf(s, len,
                     "CPU-based video filter.\n"
                             " \n"
                             "Path to a dynamic library.");
#endif
            break;
        case MENU_ENUM_LABEL_AUDIO_DEVICE:
            snprintf(s, len,
                     "Override the default audio device \n"
                             "the audio driver uses.\n"
                             "This is driver dependent. E.g.\n"
#ifdef HAVE_ALSA
            " \n"
            "ALSA wants a PCM device."
#endif
#ifdef HAVE_OSS
            " \n"
            "OSS wants a path (e.g. /dev/dsp)."
#endif
#ifdef HAVE_JACK
            " \n"
            "JACK wants portnames (e.g. system:playback1\n"
            ",system:playback_2)."
#endif
#ifdef HAVE_RSOUND
            " \n"
            "RSound wants an IP address to an RSound \n"
            "server."
#endif
            );
            break;
        case MENU_ENUM_LABEL_ENABLE_HOTKEY:
            snprintf(s, len,
                     "Enable other hotkeys.\n"
                             " \n"
                             " If this hotkey is bound to either keyboard, \n"
                             "joybutton or joyaxis, all other hotkeys will \n"
                             "be disabled unless this hotkey is also held \n"
                             "at the same time. \n"
                             " \n"
                             "This is useful for RETRO_KEYBOARD centric \n"
                             "implementations which query a large area of \n"
                             "the keyboard, where it is not desirable that \n"
                             "hotkeys get in the way.");
            break;
        case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_AUTO:
            snprintf(s, len,
                     "Refresh Rate Auto.\n"
                             " \n"
                             "The accurate refresh rate of our monitor (Hz).\n"
                             "This is used to calculate audio input rate with \n"
                             "the formula: \n"
                             " \n"
                             "audio_input_rate = game input rate * display \n"
                             "refresh rate / game refresh rate\n"
                             " \n"
                             "If the implementation does not report any \n"
                             "values, NTSC defaults will be assumed for \n"
                             "compatibility.\n"
                             " \n"
                             "This value should stay close to 60Hz to avoid \n"
                             "large pitch changes. If your monitor does \n"
                             "not run at 60Hz, or something close to it, \n"
                             "disable VSync, and leave this at its default.");
            break;
        case MENU_ENUM_LABEL_FASTFORWARD_RATIO:
            snprintf(s, len,
                     "Fastforward ratio.\n"
                             " \n"
                             "The maximum rate at which content will\n"
                             "be run when using fast forward.\n"
                             " \n"
                             " (E.g. 5.0 for 60 fps content => 300 fps \n"
                             "cap).\n"
                             " \n"
                             "RetroArch will go to sleep to ensure that \n"
                             "the maximum rate will not be exceeded.\n"
                             "Do not rely on this cap to be perfectly \n"
                             "accurate.");
            break;
        case MENU_ENUM_LABEL_VRR_RUNLOOP_ENABLE:
            snprintf(s, len,
                     "Sync to Exact Content Framerate.\n"
                             " \n"
                             "This option is the equivalent of forcing x1 speed\n"
                             "while still allowing fast forward.\n"
                             "No deviation from the core requested refresh rate,\n"
                             "no sound Dynamic Rate Control).");
            break;
        case MENU_ENUM_LABEL_VIDEO_MONITOR_INDEX:
            snprintf(s, len,
                     "Which monitor to prefer.\n"
                             " \n"
                             "0 (default) means no particular monitor \n"
                             "is preferred, 1 and up (1 being first \n"
                             "monitor), suggests RetroArch to use that \n"
                             "particular monitor.");
            break;
        case MENU_ENUM_LABEL_AUDIO_VOLUME:
            snprintf(s, len,
                     "Audio volume, expressed in dB.\n"
                             " \n"
                             " 0 dB is normal volume. No gain will be applied.\n"
                             "Gain can be controlled in runtime with Input\n"
                             "Volume Up / Input Volume Down.");
            break;
        case MENU_ENUM_LABEL_AUDIO_RATE_CONTROL_DELTA:
            snprintf(s, len,
                     "Audio rate control.\n"
                             " \n"
                             "Setting this to 0 disables rate control.\n"
                             "Any other value controls audio rate control \n"
                             "delta.\n"
                             " \n"
                             "Defines how much input rate can be adjusted \n"
                             "dynamically.\n"
                             " \n"
                             " Input rate is defined as: \n"
                             " input rate * (1.0 +/- (rate control delta))");
            break;
        case MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW:
            snprintf(s, len,
                     "Maximum audio timing skew.\n"
                             " \n"
                             "Defines the maximum change in input rate.\n"
                             "You may want to increase this to enable\n"
                             "very large changes in timing, for example\n"
                             "running PAL cores on NTSC displays, at the\n"
                             "cost of inaccurate audio pitch.\n"
                             " \n"
                             " Input rate is defined as: \n"
                             " input rate * (1.0 +/- (max timing skew))");
            break;
        case MENU_ENUM_LABEL_VIDEO_DISABLE_COMPOSITION:
            snprintf(s, len,
                     "Forcibly disable composition.\n"
                             "Only valid on Windows Vista/7 for now.");
            break;
        case MENU_ENUM_LABEL_VIDEO_THREADED:
            snprintf(s, len,
                     "Use threaded video driver.\n"
                             " \n"
                             "Using this might improve performance at the \n"
                             "possible cost of latency and more video \n"
                             "stuttering.");
            break;
        case MENU_ENUM_LABEL_VIDEO_FRAME_DELAY:
            snprintf(s, len,
                     "Sets how many milliseconds to delay\n"
                             "after VSync before running the core.\n"
                             "\n"
                             "Can reduce latency at the cost of\n"
                             "higher risk of stuttering.\n"
                             " \n"
                             "Maximum is 15.");
            break;
        case MENU_ENUM_LABEL_VIDEO_HARD_SYNC_FRAMES:
            snprintf(s, len,
                     "Sets how many frames CPU can \n"
                             "run ahead of GPU when using 'GPU \n"
                             "Hard Sync'.\n"
                             " \n"
                             "Maximum is 3.\n"
                             " \n"
                             " 0: Syncs to GPU immediately.\n"
                             " 1: Syncs to previous frame.\n"
                             " 2: Etc ...");
            break;
        case MENU_ENUM_LABEL_VIDEO_BLACK_FRAME_INSERTION:
            snprintf(s, len,
                     "Inserts a black frame inbetween \n"
                             "frames.\n"
                             " \n"
                             "Useful for 120 Hz monitors who want to \n"
                             "play 60 Hz material with eliminated \n"
                             "ghosting.\n"
                             " \n"
                             "Video refresh rate should still be \n"
                             "configured as if it is a 60 Hz monitor \n"
                             "(divide refresh rate by 2).");
            break;
        case MENU_ENUM_LABEL_SAVEFILE_DIRECTORY:
            snprintf(s, len,
                     "Savefile Directory. \n"
                             " \n"
                             "Save all save files (*.srm) to this \n"
                             "directory. This includes related files like \n"
                             ".bsv, .rt, .psrm, etc...\n"
                             " \n"
                             "This will be overridden by explicit command line\n"
                             "options.");
            break;
        case MENU_ENUM_LABEL_ASSETS_DIRECTORY:
            snprintf(s, len,
                     "Assets Directory. \n"
                             " \n"
                             " This location is queried by default when \n"
                             "menu interfaces try to look for loadable \n"
                             "assets, etc.");
            break;
        case MENU_ENUM_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY:
            snprintf(s, len,
                     "Dynamic Wallpapers Directory. \n"
                             " \n"
                             " The place to store backgrounds that will \n"
                             "be loaded dynamically by the menu depending \n"
                             "on context.");
            break;
        case MENU_ENUM_LABEL_INPUT_PREFER_FRONT_TOUCH:
            snprintf(s, len, "Use front instead of back touch.");
            break;
        case MENU_ENUM_LABEL_SUSPEND_SCREENSAVER_ENABLE:
            snprintf(s, len,
                     "Suspends the screensaver. Is a hint that \n"
                             "does not necessarily have to be \n"
                             "honored by the video driver.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_MODE:
            snprintf(s, len,
                     "Netplay client mode for the current user. \n"
                             "Will be 'Server' mode if disabled.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_DELAY_FRAMES:
            snprintf(s, len,
                     "The amount of delay frames to use for netplay. \n"
                             " \n"
                             "Increasing this value will increase \n"
                             "performance, but introduce more latency.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_START_AS_SPECTATOR:
            snprintf(s, len,
                     "Whether to start netplay in spectator mode. \n"
                             " \n"
                             "If set to true, netplay will be in spectator mode \n"
                             "on start. It's always possible to change mode \n"
                             "later.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_CHECK_FRAMES:
            snprintf(s, len,
                     "The frequency in frames with which netplay \n"
                             "will verify that the host and client are in \n"
                             "sync. \n"
                             " \n"
                             "With most cores, this value will have no \n"
                             "visible effect and can be ignored. With \n"
                             "nondeterminstic cores, this value determines \n"
                             "how often the netplay peers will be brought \n"
                             "into sync. With buggy cores, setting this \n"
                             "to any non-zero value will cause severe \n"
                             "performance issues. Set to zero to perform \n"
                             "no checks. This value is only used on the \n"
                             "netplay host. \n");
            break;
        case MENU_ENUM_LABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN:
            snprintf(s, len,
                     "The number of frames of input latency for \n"
                     "netplay to use to hide network latency. \n"
                     " \n"
                     "When in netplay, this option delays local \n"
                     "input, so that the frame being run is \n"
                     "closer to the frames being received from \n"
                     "the network. This reduces jitter and makes \n"
                     "netplay less CPU-intensive, but at the \n"
                     "price of noticeable input lag. \n");
            break;
        case MENU_ENUM_LABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE:
            snprintf(s, len,
                     "The range of frames of input latency that \n"
                     "may be used by netplay to hide network \n"
                     "latency. \n"
                     "\n"
                     "If set, netplay will adjust the number of \n"
                     "frames of input latency dynamically to \n"
                     "balance CPU time, input latency and \n"
                     "network latency. This reduces jitter and \n"
                     "makes netplay less CPU-intensive, but at \n"
                     "the price of unpredictable input lag. \n");
            break;
        case MENU_ENUM_LABEL_VIDEO_MAX_SWAPCHAIN_IMAGES:
            snprintf(s, len,
                     "Maximum amount of swapchain images. This \n"
                             "can tell the video driver to use a specific \n"
                             "video buffering mode. \n"
                             " \n"
                             "Single buffering - 1\n"
                             "Double buffering - 2\n"
                             "Triple buffering - 3\n"
                             " \n"
                             "Setting the right buffering mode can have \n"
                             "a big impact on latency.");
            break;
        case MENU_ENUM_LABEL_VIDEO_SMOOTH:
            snprintf(s, len,
                     "Smoothens picture with bilinear filtering. \n"
                             "Should be disabled if using shaders.");
            break;
      case MENU_ENUM_LABEL_VIDEO_CTX_SCALING:
         snprintf(s, len,
#ifdef HAVE_ODROIDGO2
               "RGA scaling and bicubic filtering. May break widgets."
#else
               "Hardware context scaling (if available)."
#endif
         );
         break;
        case MENU_ENUM_LABEL_NETPLAY_SETTINGS:
            snprintf(s, len,
                     "Setting related to Netplay.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_SPECTATOR_MODE_ENABLE:
            snprintf(s, len,
                     "Enable or disable spectator mode for \n"
                             "the user during netplay.");
            break;
        case MENU_ENUM_LABEL_UI_COMPANION_START_ON_BOOT:
            snprintf(s, len,
                     "Start User Interface companion driver \n"
                             "on boot (if available).");
            break;
        case MENU_ENUM_LABEL_EXIT_EMULATOR:
            snprintf(s, len,
                     "Key to exit RetroArch cleanly."
#if !defined(RARCH_MOBILE) && !defined(RARCH_CONSOLE)
                            "\nKilling it in any hard way (SIGKILL, \n"
                            "etc) will terminate without saving\n"
                            "RAM, etc. On Unix-likes,\n"
                            "SIGINT/SIGTERM allows\n"
                            "a clean deinitialization."
#endif
            );
            break;
        case MENU_ENUM_LABEL_CHEAT_START_OR_CONT:
            snprintf(s, len,
                     "Scan memory to create new cheats");
            break;
        case MENU_ENUM_LABEL_VALUE_WHAT_IS_A_CORE_DESC:
            snprintf(s, len,
                     "RetroArch by itself does nothing. \n"
                            " \n"
                            "To make it do things, you need to \n"
                            "load a program into it. \n"
                            "\n"
                            "We call such a program 'Libretro core', \n"
                            "or 'core' in short. \n"
                            " \n"
                            "To load a core, select one from\n"
                            "'Load Core'.\n"
                            " \n"
#ifdef HAVE_NETWORKING
                    "You can obtain cores in several ways: \n"
                    "* Download them by going to\n"
                    "'%s' -> '%s'.\n"
                    "* Manually move them over to\n"
                    "'%s'.",
                    msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER),
                    msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST),
                    msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH)
#else
                            "You can obtain cores by\n"
                            "manually moving them over to\n"
                            "'%s'.",
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH)
#endif
            );
            break;
        case MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD_DESC:
            snprintf(s, len,
                     "You can change the virtual gamepad overlay\n"
                             "by going to '%s' -> '%s'."
                             " \n"
                             "From there you can change the overlay,\n"
                             "change the size and opacity of the buttons, etc.\n"
                             " \n"
                             "NOTE: By default, virtual gamepad overlays are\n"
                             "hidden when in the menu.\n"
                             "If you'd like to change this behavior,\n"
                             "you can set '%s' to false.",
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU)
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_ENABLE:
            snprintf(s, len,
                     "Enables a background color for the OSD.");
            break;
        case MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_RED:
            snprintf(s, len,
                     "Sets the red value of the OSD background color. Valid values are between 0 and 255.");
            break;
        case MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_GREEN:
            snprintf(s, len,
                     "Sets the green value of the OSD background color. Valid values are between 0 and 255.");
            break;
        case MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_BLUE:
            snprintf(s, len,
                     "Sets the blue value of the OSD background color. Valid values are between 0 and 255.");
            break;
        case MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_OPACITY:
            snprintf(s, len,
                     "Sets the opacity of the OSD background color. Valid values are between 0.0 and 1.0.");
            break;
        case MENU_ENUM_LABEL_VIDEO_MESSAGE_COLOR_RED:
            snprintf(s, len,
                     "Sets the red value of the OSD text color. Valid values are between 0 and 255.");
            break;
        case MENU_ENUM_LABEL_VIDEO_MESSAGE_COLOR_GREEN:
            snprintf(s, len,
                     "Sets the green value of the OSD text color. Valid values are between 0 and 255.");
            break;
        case MENU_ENUM_LABEL_VIDEO_MESSAGE_COLOR_BLUE:
            snprintf(s, len,
                     "Sets the blue value of the OSD text color. Valid values are between 0 and 255.");
            break;
        case MENU_ENUM_LABEL_MIDI_INPUT:
            snprintf(s, len,
                     "Sets the input device (driver specific).\n"
                     "When set to \"Off\", MIDI input will be disabled.\n"
                     "Device name can also be typed in.");
            break;
        case MENU_ENUM_LABEL_MIDI_OUTPUT:
            snprintf(s, len,
                     "Sets the output device (driver specific).\n"
                     "When set to \"Off\", MIDI output will be disabled.\n"
                     "Device name can also be typed in.\n"
                     " \n"
                     "When MIDI output is enabled and core and game/app support MIDI output,\n"
                     "some or all sounds (depends on game/app) will be generated by MIDI device.\n"
                     "In case of \"null\" MIDI driver this means that those sounds won't be audible.");
            break;
        default:
            if (string_is_empty(s))
                strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
            return -1;
    }

    return 0;
}
#endif

#ifdef HAVE_MENU
static const char *menu_hash_to_str_ko_label_enum(enum msg_hash_enums msg)
{
   if (msg <= MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_END &&
         msg >= MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN)
   {
      static char hotkey_lbl[128] = {0};
      unsigned idx = msg - MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN;
      snprintf(hotkey_lbl, sizeof(hotkey_lbl), "input_hotkey_binds_%d", idx);
      return hotkey_lbl;
   }

   switch (msg)
   {
#include "msg_hash_lbl.h"
      default:
#if 0
         RARCH_LOG("Unimplemented: [%d]\n", msg);
#endif
         break;
   }

   return "null";
}
#endif

const char *msg_hash_to_str_ko(enum msg_hash_enums msg) {
#ifdef HAVE_MENU
    const char *ret = menu_hash_to_str_ko_label_enum(msg);

    if (ret && !string_is_equal(ret, "null"))
       return ret;
#endif

    switch (msg) {
#include "msg_hash_ko.h"
        default:
#if 0
            RARCH_LOG("Unimplemented: [%d]\n", msg);
            {
               RARCH_LOG("[%d] : %s\n", msg - 1, msg_hash_to_str(((enum msg_hash_enums)(msg - 1))));
            }
#endif
            break;
    }

    return "null";
}

const char *msg_hash_get_wideglyph_str_ko(void)
{
   return "메";
}
