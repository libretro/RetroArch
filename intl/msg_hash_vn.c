/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2017 - Brad Parker
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

#include <stdint.h>
#include <stddef.h>

#include <compat/strl.h>
#include <string/stdstring.h>

#include "../msg_hash.h"
#include "../configuration.h"
#include "../verbosity.h"

#if defined(_MSC_VER) && !defined(_XBOX)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif

int menu_hash_get_help_vn_enum(enum msg_hash_enums msg, char *s, size_t len)
{
   uint32_t driver_hash = 0;
   settings_t      *settings = config_get_ptr();

   if (msg <= MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_END &&
         msg >= MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN)
   {
      unsigned idx = msg - MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN;

      switch (idx)
      {
         case RARCH_FAST_FORWARD_KEY:
            snprintf(s, len,
                  "Chọn giữa tốc độ nhanh và \n"
                  "bình thường."
                  );
            break;
         case RARCH_FAST_FORWARD_HOLD_KEY:
            snprintf(s, len,
                  "Nhấn nút để xem nhanh. \n"
                  " \n"
                  "Thả nút để ngừng xem nhanh."
                  );
            break;
         case RARCH_PAUSE_TOGGLE:
            snprintf(s, len,
                  "Bật/tắt chức năng tạm dừng.");
            break;
         case RARCH_FRAMEADVANCE:
            snprintf(s, len,
                  "Xem khung kế tiếp khi đã tạm dừng.");
            break;
         case RARCH_SHADER_NEXT:
            snprintf(s, len,
                  "Áp dụng shader kế tiếp trong thư mục.");
            break;
         case RARCH_SHADER_PREV:
            snprintf(s, len,
                  "Áp dụng shader trước trong thư mục.");
            break;
         case RARCH_CHEAT_INDEX_PLUS:
         case RARCH_CHEAT_INDEX_MINUS:
         case RARCH_CHEAT_TOGGLE:
            snprintf(s, len,
                  "Gian lận.");
            break;
         case RARCH_RESET:
            snprintf(s, len,
                  "Đặt lại nội dung.");
            break;
         case RARCH_SCREENSHOT:
            snprintf(s, len,
                  "Chụp ảnh màn hình.");
            break;
         case RARCH_MUTE:
            snprintf(s, len,
                  "Tắt/Bật âm thanh.");
            break;
         case RARCH_OSK:
            snprintf(s, len,
                  "Bật/tắt bàn phím trên màn hình.");
            break;
         case RARCH_NETPLAY_FLIP:
            snprintf(s, len,
                  "Netplay flip users.");
            break;
         case RARCH_SLOWMOTION:
            snprintf(s, len,
                  "Nhấn để xem chậm.");
            break;
         case RARCH_ENABLE_HOTKEY:
            snprintf(s, len,
                  "Enable other hotkeys. \n"
                  " \n"
                  "If this hotkey is bound to either\n"
                  "a keyboard, joybutton or joyaxis, \n"
                  "all other hotkeys will be enabled only \n"
                  "if this one is held at the same time. \n"
                  " \n"
                  "This is useful for RETRO_KEYBOARD centric \n"
                  "implementations which query a large area of \n"
                  "the keyboard, where it is not desirable that \n"
                  "hotkeys get in the way. \n"
                  " \n"
                  "Alternatively, all hotkeys for keyboard \n"
                  "could be disabled by the user.");
            break;
         case RARCH_VOLUME_UP:
            snprintf(s, len,
                  "Tăng âm lượng.");
            break;
         case RARCH_VOLUME_DOWN:
            snprintf(s, len,
                  "Giảm âm lượng.");
            break;
         case RARCH_OVERLAY_NEXT:
            snprintf(s, len,
                  "Đổi qua overlay kế tiếp.");
            break;
         case RARCH_DISK_EJECT_TOGGLE:
            snprintf(s, len,
                  "Bật/tắt nhả đĩa. \n"
                  " \n"
                  "Được sử dụng cho nội dung có nhiều đĩa. ");
            break;
         case RARCH_DISK_NEXT:
         case RARCH_DISK_PREV:
            snprintf(s, len,
                  "Xem qua các đĩa game. Sử dụng sau khi nhả đĩa. \n"
                  " \n"
                  "Bấm nút 'Bật/tắt nhả đĩa' để chọn đĩa.");
            break;
         case RARCH_GRAB_MOUSE_TOGGLE:
            snprintf(s, len,
                  "Bật/tắt mouse grab. \n"
                  " \n"
                  "When mouse is grabbed, RetroArch hides the \n"
                  "mouse, and keeps the mouse pointer inside \n"
                  "the window to allow relative mouse input to \n"
                  "work better.");
            break;
         case RARCH_MENU_TOGGLE:
            snprintf(s, len, "Bật/tắt menu.");
            break;
         case RARCH_LOAD_STATE_KEY:
            snprintf(s, len,
                  "Tải state.");
            break;
         case RARCH_FULLSCREEN_TOGGLE_KEY:
            snprintf(s, len,
                  "Bật/tắt chế độ toàn màn hình.");
            break;
         case RARCH_QUIT_KEY:
            snprintf(s, len,
                  "Nút để an toàn thoát RetroArch. \n"
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
         case RARCH_SAVE_STATE_KEY:
            snprintf(s, len,
                  "Lưu state.");
            break;
         case RARCH_REWIND:
            snprintf(s, len,
                  "Giữ nút đễ quay lại. \n"
                  " \n"
                  "Cần phải bật chức năng quay lại.");
            break;
         case RARCH_MOVIE_RECORD_TOGGLE:
            snprintf(s, len,
                  "Bật/tắt ghi chép video.");
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
         snprintf(s, len, "Chi tiết đăng nhập cho tài khoản \n"
               "Retro Achievements của bạn. \n"
               " \n"
               "Truy cập retroachievements.org đễ \n"
               "đăng ký tài khoản miễn phí. \n"
               " \n"
               "Sau khi đăng ký, bạn phải \n"
               "cung cấp tên tài khoản và mật mã vào \n"
               "RetroArch.");
         break;
      case MENU_ENUM_LABEL_CHEEVOS_USERNAME:
         snprintf(s, len, "Tên tài khoản của Retro Achievements.");
         break;
      case MENU_ENUM_LABEL_CHEEVOS_PASSWORD:
         snprintf(s, len, "Mật mã của Retro Achievements tài khoản.");
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
      case MENU_ENUM_LABEL_VIDEO_FONT_PATH:
         snprintf(s, len, "Change the font that is used \n"
               "for the Onscreen Display text.");
         break;
      case MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS:
         snprintf(s, len, "Automatically tải content-specific core options.");
         break;
      case MENU_ENUM_LABEL_AUTO_OVERRIDES_ENABLE:
         snprintf(s, len, "Automatically tải override configurations.");
         break;
      case MENU_ENUM_LABEL_AUTO_REMAPS_ENABLE:
         snprintf(s, len, "Automatically tải input remapping files.");
         break;
      case MENU_ENUM_LABEL_SORT_SAVESTATES_ENABLE:
         snprintf(s, len, "Sort save states in folders \n"
               "named after the libretro core used.");
         break;
      case MENU_ENUM_LABEL_SORT_SAVEFILES_ENABLE:
         snprintf(s, len, "Sort save files in folders \n"
               "named after the libretro core used.");
         break;
      case MENU_ENUM_LABEL_RESUME_CONTENT:
         snprintf(s, len, "Exits from the menu and returns back \n"
               "to the content.");
         break;
      case MENU_ENUM_LABEL_RESTART_CONTENT:
         snprintf(s, len, "Restarts the content from the beginning.");
         break;
      case MENU_ENUM_LABEL_CLOSE_CONTENT:
         snprintf(s, len, "Closes the content and unloads it from \n"
               "memory.");
         break;
      case MENU_ENUM_LABEL_UNDO_LOAD_STATE:
         snprintf(s, len, "If a state was loaded, content will \n"
               "go back to the state prior to loading.");
         break;
      case MENU_ENUM_LABEL_UNDO_SAVE_STATE:
         snprintf(s, len, "If a state was overwritten, it will \n"
               "roll back to the previous save state.");
         break;
      case MENU_ENUM_LABEL_TAKE_SCREENSHOT:
         snprintf(s, len, "Create a screenshot. \n"
               " \n"
               "The screenshot will be stored inside the \n"
               "Screenshot Thư mục.");
         break;
      case MENU_ENUM_LABEL_RUN:
         snprintf(s, len, "Start the content.");
         break;
      case MENU_ENUM_LABEL_INFORMATION:
         snprintf(s, len, "Show additional metadata information \n"
               "about the content.");
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
               "Select this to scan the current thư mục \n"
               "for content.");
         break;
      case MENU_ENUM_LABEL_USE_THIS_DIRECTORY:
         snprintf(s, len,
               "Select this to set this as the thư mục.");
         break;
      case MENU_ENUM_LABEL_CONTENT_DATABASE_DIRECTORY:
         snprintf(s, len,
               "Content Database thư mục. \n"
               " \n"
               "Path to content database \n"
               "thư mục.");
         break;
      case MENU_ENUM_LABEL_THUMBNAILS_DIRECTORY:
         snprintf(s, len,
               "Thumbnails thư mục. \n"
               " \n"
               "To store thumbnail files.");
         break;
      case MENU_ENUM_LABEL_LIBRETRO_INFO_PATH:
         snprintf(s, len,
               "Core Info thư mục. \n"
               " \n"
               "A thư mục for where to search \n"
               "for libretro core information.");
         break;
      case MENU_ENUM_LABEL_PLAYLIST_DIRECTORY:
         snprintf(s, len,
               "Playlist thư mục. \n"
               " \n"
               "Save all playlist files to this \n"
               "thư mục.");
         break;
      case MENU_ENUM_LABEL_DUMMY_ON_CORE_SHUTDOWN:
         snprintf(s, len,
               "Some libretro cores might have \n"
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
               "firmware is missing. \n"
               "down. \n");
         break;
      case MENU_ENUM_LABEL_PARENT_DIRECTORY:
         snprintf(s, len,
               "Go back to the parent thư mục.");
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
               "thư mục. \n"
               " \n"
               "Select it to open this thư mục.");
         break;
      case MENU_ENUM_LABEL_CACHE_DIRECTORY:
         snprintf(s, len,
               "Cache thư mục. \n"
               " \n"
               "Content decompressed by RetroArch will be \n"
               "temporarily extracted to this thư mục.");
         break;
      case MENU_ENUM_LABEL_HISTORY_LIST_ENABLE:
         snprintf(s, len,
               "If enabled, every content loaded \n"
               "in RetroArch will be automatically \n"
               "added to the recent history list.");
         break;
      case MENU_ENUM_LABEL_RGUI_BROWSER_DIRECTORY:
         snprintf(s, len,
               "File Browser thư mục. \n"
               " \n"
               "Sets start thư mục for menu file browser.");
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
               "When netplay is enabled, the default polling \n"
               "behavior (Normal) will be used, \n"
               "regardless of the value set here."
               );
         break;
      case MENU_ENUM_LABEL_INPUT_DESCRIPTOR_HIDE_UNBOUND:
         snprintf(s, len,
               "Hide input descriptors that were not set \n"
               "by the core.");
         break;
      case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE:
         snprintf(s, len,
               "Video refresh rate of your monitor. \n"
               "Used to calculate a suitable audio input rate.");
         break;
      case MENU_ENUM_LABEL_VIDEO_FORCE_SRGB_DISABLE:
         snprintf(s, len,
               "Forcibly disable sRGB FBO support. Some Intel \n"
               "OpenGL drivers on Windows have video problems \n"
               "with sRGB FBO support enabled.");
         break;
      case MENU_ENUM_LABEL_AUDIO_ENABLE:
         snprintf(s, len,
               "Enable audio output.");
         break;
      case MENU_ENUM_LABEL_AUDIO_SYNC:
         snprintf(s, len,
               "Synchronize audio (recommended).");
         break;
      case MENU_ENUM_LABEL_AUDIO_LATENCY:
         snprintf(s, len,
               "Desired audio latency in milliseconds. \n"
               "Might not be honored if the audio driver \n"
               "can't provide given latency.");
         break;
      case MENU_ENUM_LABEL_VIDEO_ALLOW_ROTATE:
         snprintf(s, len,
               "Allow cores to set rotation. If false, \n"
               "rotation requests are honored, but ignored.\n\n"
               "Used for setups where one manually rotates \n"
               "the monitor.");
         break;
      case MENU_ENUM_LABEL_INPUT_DESCRIPTOR_LABEL_SHOW:
         snprintf(s, len,
               "Show the input descriptors set by the core \n"
               "instead of the default ones.");
         break;
      case MENU_ENUM_LABEL_CONTENT_HISTORY_SIZE:
         snprintf(s, len,
               "Number of entries that will be kept in \n"
               "content history playlist.");
         break;
      case MENU_ENUM_LABEL_VIDEO_WINDOWED_FULLSCREEN:
         snprintf(s, len,
               "To use windowed mode or not when going \n"
               "fullscreen.");
         break;
      case MENU_ENUM_LABEL_VIDEO_FONT_SIZE:
         snprintf(s, len,
               "Font size for on-screen messages.");
         break;
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_INDEX:
         snprintf(s, len,
               "Automatically increment slot index on each save, \n"
               "generating multiple savestate files. \n"
               "When the content is loaded, state slot will be \n"
               "set to the highest existing value (last savestate).");
         break;
      case MENU_ENUM_LABEL_FPS_SHOW:
         snprintf(s, len,
               "Enables displaying the current frames \n"
               "per second.");
         break;
      case MENU_ENUM_LABEL_VIDEO_FONT_ENABLE:
         snprintf(s, len,
               "Show and/or hide onscreen messages.");
         break;
      case MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_X:
      case MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_Y:
         snprintf(s, len,
               "Offset for where messages will be placed \n"
               "onscreen. Values are in range [0.0, 1.0].");
         break;
      case MENU_ENUM_LABEL_INPUT_OVERLAY_ENABLE:
         snprintf(s, len,
               "Enable or disable the current overlay.");
         break;
      case MENU_ENUM_LABEL_INPUT_OVERLAY_HIDE_IN_MENU:
         snprintf(s, len,
               "Hide the current overlay from appearing \n"
               "inside the menu.");
         break;
      case MENU_ENUM_LABEL_OVERLAY_PRESET:
         snprintf(s, len,
               "Path to input overlay.");
         break;
      case MENU_ENUM_LABEL_OVERLAY_OPACITY:
         snprintf(s, len,
               "Overlay opacity.");
         break;
      case MENU_ENUM_LABEL_INPUT_BIND_TIMEOUT:
         snprintf(s, len,
               "Input bind timer timeout (in seconds). \n"
               "Amount of seconds to wait until proceeding \n"
               "to the next bind.");
         break;
      case MENU_ENUM_LABEL_OVERLAY_SCALE:
         snprintf(s, len,
               "Overlay scale.");
         break;
      case MENU_ENUM_LABEL_AUDIO_OUTPUT_RATE:
         snprintf(s, len,
               "Audio output samplerate.");
         break;
      case MENU_ENUM_LABEL_VIDEO_SHARED_CONTEXT:
         snprintf(s, len,
               "Set to true if hardware-rendered cores \n"
               "should get their private context. \n"
               "Avoids having to assume hardware state changes \n"
               "inbetween frames."
               );
         break;
      case MENU_ENUM_LABEL_CORE_LIST:
         snprintf(s, len,
               "Tải Core. \n"
               " \n"
               "Browse for a libretro core \n"
               "implementation. Where the browser \n"
               "starts depends on your Core thư mục \n"
               "path. If blank, it will start in root. \n"
               " \n"
               "If Core thư mục is a thư mục, the menu \n"
               "will use that as top folder. If Core \n"
               "thư mục is a full path, it will start \n"
               "in the folder where the file is.");
         break;
      case MENU_ENUM_LABEL_VALUE_MENU_ENUM_CONTROLS_PROLOG:
         snprintf(s, len,
               "You can use the following controls below \n"
               "on either your gamepad or keyboard in order\n"
               "to control the menu: \n"
               " \n"
               );
         break;
      case MENU_ENUM_LABEL_WELCOME_TO_RETROARCH:
         snprintf(s, len,
               "Welcome to RetroArch\n"
               );
         break;
      case MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING_DESC:
         {
            /* Work around C89 limitations */
            char u[501];
            const char * t =
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
               " \n"
               "Files will be compared to database entries.\n"
               "If there is a match, it will add an entry\n"
               "to a collection.\n"
               " \n"
               "You can then easily access this content by\n"
               "going to '%s' ->\n"
               "'%s'\n"
               "instead of having to go through the\n"
               "filebrowser everytime.\n"
               " \n"
               "NOTE: Content for some cores might still not be\n"
               "scannable."
               ,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_FILE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT_COLLECTION_LIST)
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
         if (settings)
            driver_hash = msg_hash_calculate(settings->input.driver);

         switch (driver_hash)
         {
            case MENU_LABEL_INPUT_DRIVER_UDEV:
               snprintf(s, len,
                     "udev Input driver. \n"
                     " \n"
                     "This driver can run without X. \n"
                     " \n"
                     "It uses the recent evdev joypad API \n"
                     "for joystick support. It supports \n"
                     "hotplugging and force feedback (if \n"
                     "supported by device). \n"
                     " \n"
                     "The driver reads evdev events for keyboard \n"
                     "support. It also supports keyboard callback, \n"
                     "mice and touchpads. \n"
                     " \n"
                     "By default in most distros, /dev/input nodes \n"
                     "are root-only (mode 600). You can set up a udev \n"
                     "rule which makes these accessible to non-root."
                     );
               break;
            case MENU_LABEL_INPUT_DRIVER_LINUXRAW:
               snprintf(s, len,
                     "linuxraw Input driver. \n"
                     " \n"
                     "This driver requires an active TTY. Keyboard \n"
                     "events are read directly from the TTY which \n"
                     "makes it simpler, but not as flexible as udev. \n" "Mice, etc, are not supported at all. \n"
                     " \n"
                     "This driver uses the older joystick API \n"
                     "(/dev/input/js*).");
               break;
            default:
               snprintf(s, len,
                     "Input driver.\n"
                     " \n"
                     "Depending on video driver, it might \n"
                     "force a different input driver.");
               break;
         }
         break;
      case MENU_ENUM_LABEL_LOAD_CONTENT_LIST:
         snprintf(s, len,
               "Tải Content. \n"
               "Browse for content. \n"
               " \n"
               "To tải content, you need a \n"
               "'Core' to use, and a content file. \n"
               " \n"
               "To control where the menu starts \n"
               "to browse for content, set  \n"
               "'File Browser thư mục'. \n"
               "If not set, it will start in root. \n"
               " \n"
               "The browser will filter out \n"
               "extensions for the last core set \n"
               "in 'Tải Core', and use that core \n"
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
               "thư mục as the RetroArch config file. If \n"
               "no config file was loaded in startup, history \n"
               "will not be saved or loaded, and will not exist \n"
               "in the main menu."
               );
         break;
      case MENU_ENUM_LABEL_VIDEO_DRIVER:
         snprintf(s, len,
               "Current Video driver.");

         if (string_is_equal(settings->video.driver, "gl"))
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
         else if (string_is_equal(settings->video.driver, "sdl2"))
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
         else if (string_is_equal(settings->video.driver, "sdl1"))
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
         else if (string_is_equal(settings->video.driver, "d3d"))
         {
            snprintf(s, len,
                  "Direct3D Video driver. \n"
                  " \n"
                  "Performance for software-rendered cores \n"
                  "is dependent on your graphic card's \n"
                  "underlying D3D driver).");
         }
         else if (string_is_equal(settings->video.driver, "exynos"))
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
         else if (string_is_equal(settings->video.driver, "drm"))
         {
            snprintf(s, len,
                  "Plain DRM Video Driver. \n"
                  " \n"
                  "This is a low-level video driver using. \n"
                  "libdrm for hardware scaling using \n"
                  "GPU overlays.");
         }
         else if (string_is_equal(settings->video.driver, "sunxi"))
         {
            snprintf(s, len,
                  "Sunxi-G2D Video Driver. \n"
                  " \n"
                  "This is a low-level Sunxi video driver. \n"
                  "Uses the G2D block in Allwinner SoCs.");
         }
         break;
      case MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN:
         snprintf(s, len,
               "Audio DSP plugin.\n"
               " Processes audio before it's sent to \n"
               "the driver."
               );
         break;
      case MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER:
         if (settings)
            driver_hash = msg_hash_calculate(settings->audio.resampler);

         switch (driver_hash)
         {
            case MENU_LABEL_AUDIO_RESAMPLER_DRIVER_SINC:
               snprintf(s, len,
                     "Windowed SINC implementation.");
               break;
            case MENU_LABEL_AUDIO_RESAMPLER_DRIVER_CC:
               snprintf(s, len,
                     "Convoluted Cosine implementation.");
               break;
            default:
               if (string_is_empty(s))
                  strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
               break;
         }
         break;
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET:
         snprintf(s, len,
               "Tải Shader Preset. \n"
               " \n"
               " Tải a "
#ifdef HAVE_CG
               "Cg"
#endif
#ifdef HAVE_GLSL
#ifdef HAVE_CG
               "/"
#endif
               "GLSL"
#endif
#ifdef HAVE_HLSL
#if defined(HAVE_CG) || defined(HAVE_HLSL)
               "/"
#endif
               "HLSL"
#endif
               " preset directly. \n"
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
      case MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS:
         snprintf(s, len,
               "Shader Parameters. \n"
               " \n"
               "Modifies current shader directly. Will not be \n"
               "saved to CGP/GLSLP preset file.");
         break;
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
         snprintf(s, len,
               "Shader Preset Parameters. \n"
               " \n"
               "Modifies shader preset currently in menu."
               );
         break;
      case MENU_ENUM_LABEL_VIDEO_SHADER_PASS:
         snprintf(s, len,
               "Path to shader. \n"
               " \n"
               "All shaders must be of the same \n"
               "type (i.e. CG, GLSL or HLSL). \n"
               " \n"
               "Set Shader thư mục to set where \n"
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
      case MENU_ENUM_LABEL_CONFIRM_ON_EXIT:
         snprintf(s, len, "Are you sure you want to quit?");
         break;
      case MENU_ENUM_LABEL_SHOW_HIDDEN_FILES:
         snprintf(s, len, "Show hidden files\n"
               "and folders.");
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
               "saved to Shader thư mục."
               );
         break;
      case MENU_ENUM_LABEL_MENU_TOGGLE:
         snprintf(s, len,
               "Bật/tắt menu.");
         break;
      case MENU_ENUM_LABEL_GRAB_MOUSE_TOGGLE:
         snprintf(s, len,
               "Bật/tắt mouse grab.\n"
               " \n"
               "When mouse is grabbed, RetroArch hides the \n"
               "mouse, and keeps the mouse pointer inside \n"
               "the window to allow relative mouse input to \n"
               "work better.");
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
      case MENU_ENUM_LABEL_DISK_EJECT_TOGGLE:
         snprintf(s, len,
               "Bật/tắt eject for disks.\n"
               " \n"
               "Used for multiple-disk content.");
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
      case MENU_ENUM_LABEL_REWIND_ENABLE:
         snprintf(s, len,
               "Enable rewinding.\n"
               " \n"
               "This will take a performance hit, \n"
               "so it is disabled by default.");
         break;
      case MENU_ENUM_LABEL_LIBRETRO_DIR_PATH:
         snprintf(s, len,
               "Core thư mục. \n"
               " \n"
               "A thư mục for where to search for \n"
               "libretro core implementations.");
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
      case MENU_ENUM_LABEL_VIDEO_ROTATION:
         snprintf(s, len,
               "Forces a certain rotation \n"
               "of the screen.\n"
               " \n"
               "The rotation is added to rotations which\n"
               "the libretro core sets (see Video Allow\n"
               "Rotate).");
         break;
      case MENU_ENUM_LABEL_VIDEO_SCALE:
         snprintf(s, len,
               "Fullscreen resolution.\n"
               " \n"
               "Resolution of 0 uses the \n"
               "resolution of the environment.\n");
         break;
      case MENU_ENUM_LABEL_FASTFORWARD_RATIO:
         snprintf(s, len,
               "Fastforward ratio."
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
      case MENU_ENUM_LABEL_VIDEO_MONITOR_INDEX:
         snprintf(s, len,
               "Which monitor to prefer.\n"
               " \n"
               "0 (default) means no particular monitor \n"
               "is preferred, 1 and up (1 being first \n"
               "monitor), suggests RetroArch to use that \n"
               "particular monitor.");
         break;
      case MENU_ENUM_LABEL_VIDEO_CROP_OVERSCAN:
         snprintf(s, len,
               "Forces cropping of overscanned \n"
               "frames.\n"
               " \n"
               "Exact behavior of this option is \n"
               "core-implementation specific.");
         break;
      case MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER:
         snprintf(s, len,
               "Only scales video in integer \n"
               "steps.\n"
               " \n"
               "The base size depends on system-reported \n"
               "geometry and aspect ratio.\n"
               " \n"
               "If Force Aspect is not set, X/Y will be \n"
               "integer scaled independently.");
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
      case MENU_ENUM_LABEL_OVERLAY_NEXT:
         snprintf(s, len,
               "Bật/tắt to next overlay.\n"
               " \n"
               "Wraps around.");
         break;
      case MENU_ENUM_LABEL_LOG_VERBOSITY:
         snprintf(s, len,
               "Enable or disable verbosity level \n"
               "of frontend.");
         break;
      case MENU_ENUM_LABEL_VOLUME_UP:
         snprintf(s, len,
               "Tăng âm lượng.");
         break;
      case MENU_ENUM_LABEL_VOLUME_DOWN:
         snprintf(s, len,
               "Giảm âm lượng.");
         break;
      case MENU_ENUM_LABEL_VIDEO_DISABLE_COMPOSITION:
         snprintf(s, len,
               "Forcibly disable composition.\n"
               "Only valid on Windows Vista/7 for now.");
         break;
      case MENU_ENUM_LABEL_PERFCNT_ENABLE:
         snprintf(s, len,
               "Enable or disable frontend \n"
               "performance counters.");
         break;
      case MENU_ENUM_LABEL_SYSTEM_DIRECTORY:
         snprintf(s, len,
               "System thư mục. \n"
               " \n"
               "Sets the 'system' thư mục.\n"
               "Cores can query for this\n"
               "thư mục to tải BIOSes, \n"
               "system-specific configs, etc.");
         break;
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE:
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_LOAD:
         snprintf(s, len,
               "Automatically saves a savestate at the \n"
               "end of RetroArch's lifetime.\n"
               " \n"
               "RetroArch will automatically tải any savestate\n"
               "with this path on startup if 'Auto Tải State\n"
               "is enabled.");
         break;
      case MENU_ENUM_LABEL_VIDEO_THREADED:
         snprintf(s, len,
               "Use threaded video driver.\n"
               " \n"
               "Using this might improve performance at the \n"
               "possible cost of latency and more video \n"
               "stuttering.");
         break;
      case MENU_ENUM_LABEL_VIDEO_VSYNC:
         snprintf(s, len,
               "Video V-Sync.\n");
         break;
      case MENU_ENUM_LABEL_VIDEO_HARD_SYNC:
         snprintf(s, len,
               "Attempts to hard-synchronize \n"
               "CPU and GPU.\n"
               " \n"
               "Can reduce latency at the cost of \n"
               "performance.");
         break;
      case MENU_ENUM_LABEL_REWIND_GRANULARITY:
         snprintf(s, len,
               "Rewind granularity.\n"
               " \n"
               " When rewinding defined number of \n"
               "frames, you can rewind several frames \n"
               "at a time, increasing the rewinding \n"
               "speed.");
         break;
      case MENU_ENUM_LABEL_SCREENSHOT:
         snprintf(s, len,
               "Chụp ảnh màn hình.");
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
      case MENU_ENUM_LABEL_RGUI_SHOW_START_SCREEN:
         snprintf(s, len,
               "Show startup screen in menu.\n"
               "Is automatically set to false when seen\n"
               "for the first time.\n"
               " \n"
               "This is only updated in config if\n"
               "'Save Configuration on Exit' is enabled.\n");
         break;
      case MENU_ENUM_LABEL_CORE_SPECIFIC_CONFIG:
         snprintf(s, len,
               "Tải up a specific config file \n"
               "based on the core being used.\n");
         break;
      case MENU_ENUM_LABEL_VIDEO_FULLSCREEN:
         snprintf(s, len, "Bật/tắt chế độ toàn màn hình.");
         break;
      case MENU_ENUM_LABEL_BLOCK_SRAM_OVERWRITE:
         snprintf(s, len,
               "Block SRAM from being overwritten \n"
               "when loading save states.\n"
               " \n"
               "Might potentially lead to buggy games.");
         break;
      case MENU_ENUM_LABEL_PAUSE_NONACTIVE:
         snprintf(s, len,
               "Pause gameplay when window focus \n"
               "is lost.");
         break;
      case MENU_ENUM_LABEL_VIDEO_GPU_SCREENSHOT:
         snprintf(s, len,
               "Screenshots output of GPU shaded \n"
               "material if available.");
         break;
      case MENU_ENUM_LABEL_SCREENSHOT_DIRECTORY:
         snprintf(s, len,
               "Screenshot thư mục. \n"
               " \n"
               "thư mục to dump screenshots to."
               );
         break;
      case MENU_ENUM_LABEL_VIDEO_SWAP_INTERVAL:
         snprintf(s, len,
               "VSync Swap Interval.\n"
               " \n"
               "Uses a custom swap interval for VSync. Set this \n"
               "to effectively halve monitor refresh rate.");
         break;
      case MENU_ENUM_LABEL_SAVEFILE_DIRECTORY:
         snprintf(s, len,
               "Savefile thư mục. \n"
               " \n"
               "Save all save files (*.srm) to this \n"
               "thư mục. This includes related files like \n"
               ".bsv, .rt, .psrm, etc...\n"
               " \n"
               "This will be overridden by explicit command line\n"
               "options.");
         break;
      case MENU_ENUM_LABEL_SAVESTATE_DIRECTORY:
         snprintf(s, len,
               "Savestate thư mục. \n"
               " \n"
               "Save all save states (*.state) to this \n"
               "thư mục.\n"
               " \n"
               "This will be overridden by explicit command line\n"
               "options.");
         break;
      case MENU_ENUM_LABEL_ASSETS_DIRECTORY:
         snprintf(s, len,
               "Assets thư mục. \n"
               " \n"
               " This location is queried by default when \n"
               "menu interfaces try to look for loadable \n"
               "assets, etc.");
         break;
      case MENU_ENUM_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY:
         snprintf(s, len,
               "Dynamic Wallpapers thư mục. \n"
               " \n"
               " The place to store wallpapers that will \n"
               "be loaded dynamically by the menu depending \n"
               "on context.");
         break;
      case MENU_ENUM_LABEL_SLOWMOTION_RATIO:
         snprintf(s, len,
               "Slowmotion ratio."
               " \n"
               "When slowmotion, content will slow\n"
               "down by factor.");
         break;
      case MENU_ENUM_LABEL_INPUT_AXIS_THRESHOLD:
         snprintf(s, len,
               "Defines axis threshold.\n"
               " \n"
               "How far an axis must be tilted to result\n"
               "in a button press.\n"
               " Possible values are [0.0, 1.0].");
         break;
      case MENU_ENUM_LABEL_INPUT_TURBO_PERIOD:
         snprintf(s, len,
               "Turbo period.\n"
               " \n"
               "Describes the period of which turbo-enabled\n"
               "buttons toggle.\n"
               " \n"
               "Numbers are described in frames."
               );
         break;
      case MENU_ENUM_LABEL_INPUT_DUTY_CYCLE:
         snprintf(s, len,
               "Duty cycle.\n"
               " \n"
               "Describes how long the period of a turbo-enabled\n"
               "should be.\n"
               " \n"
               "Numbers are described in frames."
               );
         break;
      case MENU_ENUM_LABEL_INPUT_TOUCH_ENABLE:
         snprintf(s, len, "Enable touch support.");
         break;
      case MENU_ENUM_LABEL_INPUT_PREFER_FRONT_TOUCH:
         snprintf(s, len, "Use front instead of back touch.");
         break;
      case MENU_ENUM_LABEL_MOUSE_ENABLE:
         snprintf(s, len, "Enable mouse input inside the menu.");
         break;
      case MENU_ENUM_LABEL_POINTER_ENABLE:
         snprintf(s, len, "Enable touch input inside the menu.");
         break;
      case MENU_ENUM_LABEL_MENU_WALLPAPER:
         snprintf(s, len, "Path to an image to set as the background.");
         break;
      case MENU_ENUM_LABEL_NAVIGATION_WRAPAROUND:
         snprintf(s, len,
               "Wrap-around to beginning and/or end \n"
               "if boundary of list is reached \n"
               "horizontally and/or vertically.");
         break;
      case MENU_ENUM_LABEL_PAUSE_LIBRETRO:
         snprintf(s, len,
               "If disabled, the libretro core will keep \n"
               "running in the background when we are in the \n"
               "menu.");
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
      case MENU_ENUM_LABEL_TIMEDATE_ENABLE:
         snprintf(s, len,
               "Shows current date and/or time inside menu.");
         break;
      case MENU_ENUM_LABEL_CORE_ENABLE:
         snprintf(s, len,
               "Shows current core inside menu.");
         break;
      case MENU_ENUM_LABEL_NETPLAY_ENABLE_HOST:
         snprintf(s, len,
               "Enables Netplay in host (server) mode.");
         break;
      case MENU_ENUM_LABEL_NETPLAY_ENABLE_CLIENT:
         snprintf(s, len,
               "Enables Netplay in client mode.");
         break;
      case MENU_ENUM_LABEL_NETPLAY_DISCONNECT:
         snprintf(s, len,
               "Disconnects an active Netplay connection.");
         break;
      case MENU_ENUM_LABEL_NETPLAY_SETTINGS:
         snprintf(s, len,
               "Setting related to Netplay.");
         break;
      case MENU_ENUM_LABEL_NETPLAY_LAN_SCAN_SETTINGS:
         snprintf(s, len,
               "Search for and connect to netplay hosts on the local network.");
         break;
      case MENU_ENUM_LABEL_DYNAMIC_WALLPAPER:
         snprintf(s, len,
               "Dynamically tải a new wallpaper \n"
               "depending on context.");
         break;
      case MENU_ENUM_LABEL_CORE_UPDATER_BUILDBOT_URL:
         snprintf(s, len,
               "URL to core updater thư mục on the \n"
               "Libretro buildbot.");
         break;
      case MENU_ENUM_LABEL_BUILDBOT_ASSETS_URL:
         snprintf(s, len,
               "URL to assets updater thư mục on the \n"
               "Libretro buildbot.");
         break;
      case MENU_ENUM_LABEL_INPUT_REMAP_BINDS_ENABLE:
         snprintf(s, len,
               "if enabled, overrides the input binds \n"
               "with the remapped binds set for the \n"
               "current core.");
         break;
      case MENU_ENUM_LABEL_OVERLAY_DIRECTORY:
         snprintf(s, len,
               "Overlay thư mục. \n"
               " \n"
               "Defines a thư mục where overlays are \n"
               "kept for easy access.");
         break;
      case MENU_ENUM_LABEL_INPUT_MAX_USERS:
         snprintf(s, len,
               "Maximum amount of users supported by \n"
               "RetroArch.");
         break;
      case MENU_ENUM_LABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE:
         snprintf(s, len,
               "After downloading, automatically extract \n"
               "archives that the downloads are contained \n"
               "inside.");
         break;
      case MENU_ENUM_LABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
         snprintf(s, len,
               "Filter files being shown by \n"
               "supported extensions.");
         break;
      case MENU_ENUM_LABEL_NETPLAY_NICKNAME:
         snprintf(s, len,
               "The username of the person running RetroArch. \n"
               "This will be used for playing online games.");
         break;
      case MENU_ENUM_LABEL_NETPLAY_CLIENT_SWAP_INPUT:
         snprintf(s, len,
               "When being client over netplay, use \n"
               "keybinds for player 1.");
         break;
      case MENU_ENUM_LABEL_NETPLAY_TCP_UDP_PORT:
         snprintf(s, len,
               "The port of the host IP address. \n"
               "Can be either a TCP or UDP port.");
         break;
      case MENU_ENUM_LABEL_NETPLAY_SPECTATOR_MODE_ENABLE:
         snprintf(s, len,
               "Enable or disable spectator mode for \n"
               "the user during netplay.");
         break;
      case MENU_ENUM_LABEL_NETPLAY_IP_ADDRESS:
         snprintf(s, len,
               "The address of the host to connect to.");
         break;
      case MENU_ENUM_LABEL_STDIN_CMD_ENABLE:
         snprintf(s, len,
               "Enable stdin command interface.");
         break;
      case MENU_ENUM_LABEL_UI_COMPANION_START_ON_BOOT:
         snprintf(s, len,
               "Start User Interface companion driver \n"
               "on boot (if available).");
         break;
      case MENU_ENUM_LABEL_MENU_DRIVER:
         snprintf(s, len, "Menu driver to use.");
         break;
      case MENU_ENUM_LABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO:
         snprintf(s, len,
               "Gamepad button combination to toggle menu. \n"
               " \n"
               "0 - None \n"
               "1 - Press L + R + Y + D-Pad Down \n"
               "simultaneously. \n"
               "2 - Press L3 + R3 simultaneously. \n"
               "3 - Press Start + Select simultaneously.");
         break;
      case MENU_ENUM_LABEL_INPUT_ALL_USERS_CONTROL_MENU:
         snprintf(s, len, "Allow any RetroPad to control the menu.");
         break;
      case MENU_ENUM_LABEL_INPUT_AUTODETECT_ENABLE:
         snprintf(s, len,
               "Enable input auto-detection.\n"
               " \n"
               "Will attempt to auto-configure \n"
               "joypads, Plug-and-Play style.");
         break;
      case MENU_ENUM_LABEL_CAMERA_ALLOW:
         snprintf(s, len,
               "Allow or disallow camera access by \n"
               "cores.");
         break;
      case MENU_ENUM_LABEL_LOCATION_ALLOW:
         snprintf(s, len,
               "Allow or disallow location services \n"
               "access by cores.");
         break;
      case MENU_ENUM_LABEL_TURBO:
         snprintf(s, len,
               "Turbo enable.\n"
               " \n"
               "Holding the turbo while pressing another \n"
               "button will let the button enter a turbo \n"
               "mode where the button state is modulated \n"
               "with a periodic signal. \n"
               " \n"
               "The modulation stops when the button \n"
               "itself (not turbo button) is released.");
         break;
      case MENU_ENUM_LABEL_OSK_ENABLE:
         snprintf(s, len,
               "Enable/disable on-screen keyboard.");
         break;
      case MENU_ENUM_LABEL_AUDIO_MUTE:
         snprintf(s, len,
               "Tắt/Bật âm thanh.");
         break;
      case MENU_ENUM_LABEL_REWIND:
         snprintf(s, len,
               "Hold button down to rewind.\n"
               " \n"
               "Rewind must be enabled.");
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
      case MENU_ENUM_LABEL_LOAD_STATE:
         snprintf(s, len,
               "Tải state.");
         break;
      case MENU_ENUM_LABEL_SAVE_STATE:
         snprintf(s, len,
               "Saves state.");
         break;
      case MENU_ENUM_LABEL_NETPLAY_FLIP_PLAYERS:
         snprintf(s, len,
               "Netplay flip users.");
         break;
      case MENU_ENUM_LABEL_CHEAT_INDEX_PLUS:
         snprintf(s, len,
               "Increment cheat index.\n");
         break;
      case MENU_ENUM_LABEL_CHEAT_INDEX_MINUS:
         snprintf(s, len,
               "Decrement cheat index.\n");
         break;
      case MENU_ENUM_LABEL_SHADER_PREV:
         snprintf(s, len,
               "Áp dụng previous shader in thư mục.");
         break;
      case MENU_ENUM_LABEL_SHADER_NEXT:
         snprintf(s, len,
               "Áp dụng next shader in thư mục.");
         break;
      case MENU_ENUM_LABEL_RESET:
         snprintf(s, len,
               "Reset the content.\n");
         break;
      case MENU_ENUM_LABEL_PAUSE_TOGGLE:
         snprintf(s, len,
               "Toggle between paused and non-paused state.");
         break;
      case MENU_ENUM_LABEL_CHEAT_TOGGLE:
         snprintf(s, len,
               "Toggle cheat index.\n");
         break;
      case MENU_ENUM_LABEL_HOLD_FAST_FORWARD:
         snprintf(s, len,
               "Hold for fast-forward. Releasing button \n"
               "disables fast-forward.");
         break;
      case MENU_ENUM_LABEL_SLOWMOTION:
         snprintf(s, len,
               "Hold for slowmotion.");
         break;
      case MENU_ENUM_LABEL_FRAME_ADVANCE:
         snprintf(s, len,
               "Frame advance when content is paused.");
         break;
      case MENU_ENUM_LABEL_MOVIE_RECORD_TOGGLE:
         snprintf(s, len,
               "Toggle between recording and not.");
         break;
      case MENU_ENUM_LABEL_L_X_PLUS:
      case MENU_ENUM_LABEL_L_X_MINUS:
      case MENU_ENUM_LABEL_L_Y_PLUS:
      case MENU_ENUM_LABEL_L_Y_MINUS:
      case MENU_ENUM_LABEL_R_X_PLUS:
      case MENU_ENUM_LABEL_R_X_MINUS:
      case MENU_ENUM_LABEL_R_Y_PLUS:
      case MENU_ENUM_LABEL_R_Y_MINUS:
         snprintf(s, len,
               "Axis for analog stick (DualShock-esque).\n"
               " \n"
               "Bound as usual, however, if a real analog \n"
               "axis is bound, it can be read as a true analog.\n"
               " \n"
               "Positive X axis is right. \n"
               "Positive Y axis is down.");
         break;
      case MENU_ENUM_LABEL_VALUE_WHAT_IS_A_CORE_DESC:
         snprintf(s, len,
               "RetroArch by itself does nothing. \n"
               " \n"
               "To make it do things, you need to \n"
               "tải a program into it. \n"
               "\n"
               "We call such a program 'Libretro core', \n"
               "or 'core' in short. \n"
               " \n"
               "To tải a core, select one from\n"
               "'Tải Core'.\n"
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
      default:
         if (string_is_empty(s))
            strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
         return -1;
   }

   return 0;
}

const char *msg_hash_to_str_vn(enum msg_hash_enums msg)
{
   switch (msg)
   {
      case MENU_ENUM_SUBLABEL_MENU_SETTINGS:
         return "Điều chỉnh thiết lập related to the appearance of the menu screen.";
      case MSG_AUDIO_VOLUME:
         return "Âm lượng âm thanh";
      case MSG_AUTODETECT:
         return "Tự động phát hiện";
      case MSG_AUTOLOADING_SAVESTATE_FROM:
         return "Đang tự đông tải savestate từ";
      case MSG_CONNECTING_TO_NETPLAY_HOST:
         return "Đang kết nối vào máy chủ netplay";
      case MSG_CONNECTING_TO_PORT:
         return "Đang kết nối vào port";
      case MSG_CONNECTION_SLOT:
         return "Khe kết nối";
      case MSG_FAILED:
         return "Bị Lỗi";
      case MSG_FILE_ALREADY_EXISTS_SAVING_TO_BACKUP_BUFFER:
         return "Ttệp đã tồn tại. Đang lưu vào backup buffer";
      case MSG_GOT_CONNECTION_FROM:
         return "Được kết nối từ: \"%s\"";
      case MSG_NETPLAY_USERS_HAS_FLIPPED:
         return "Người dùng Netplay đã flipped";
      case MSG_SUCCEEDED:
         return "Đã thành công";
      case MSG_UNKNOWN_NETPLAY_COMMAND_RECEIVED:
         return "Netplay không biết lệnh nhận được";
      case MSG_WAITING_FOR_CLIENT:
         return "Đang đợi máy khách  ...";
	  case MSG_SHADER:
         return "Shader";
      case  MSG_SORRY_UNIMPLEMENTED_CORES_DONT_DEMAND_CONTENT_NETPLAY:
         return "Sorry, unimplemented: cores that don't demand content cannot participate in netplay.";
      case MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD:
         return "Mật khẩu";
      case MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS:
         return "Tài khoản Cheevos";
      case MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME:
         return "Tên truy nhập";
      case MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST:
         return "Những tài khoản";
      case MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST_END:
         return "Điểm cuối của danh sách tài khoản";
      case MENU_ENUM_LABEL_VALUE_ACCOUNTS_RETRO_ACHIEVEMENTS:
         return "Thành tích Retro";
      case MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST:
         return "Danh sách thành tích";
      case MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST:
         return "Thêm nội dung";
      case MENU_ENUM_LABEL_VALUE_ADD_TAB:
         return "Tạo nội dung";
      case MENU_ENUM_LABEL_VALUE_ARCHIVE_MODE:
         return "Archive File Association Action";
      case MENU_ENUM_LABEL_VALUE_ASK_ARCHIVE:
         return "Hỏi";
      case MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY:
         return "Danh mục assets";
      case MENU_ENUM_LABEL_VALUE_AUDIO_BLOCK_FRAMES:
         return "Block Frames";
      case MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE:
         return "Thiết bị âm thanh";
      case MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER:
         return "Trình điều khiển âm thanh";
      case MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN:
         return "Âm thanh DSP Plugin";
      case MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE:
         return "Kích hoạt âm thanh";
      case MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR:
         return "Âm thanh Filter Danh mục";
      case MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY:
         return "Âm thanh Latency (ms)";
      case MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW:
         return "Âm thanh Maximum Timing Skew";
      case MENU_ENUM_LABEL_VALUE_AUDIO_MUTE:
         return "Âm thanh Mute";
      case MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE:
         return "Âm thanh Output Rate (KHz)";
      case MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA:
         return "Âm thanh Rate Control Delta";
      case MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER:
         return "Âm thanh Resampler Driver";
      case MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS:
         return "Âm thanh";
      case MENU_ENUM_LABEL_VALUE_AUDIO_SYNC:
         return "Âm thanh Sync";
      case MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME:
         return "Mức âm lượng (dB)";
      case MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL:
         return "SaveRAM Autosave Interval";
      case MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE:
         return "Tự động tải tập tin ghi đè";
      case MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE:
         return "Tự động tải tập tin Remap";
      case MENU_ENUM_LABEL_VALUE_AUTO_SHADERS_ENABLE:
         return "Tự động tải Shader Presets";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK:
         return "Trở lại";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM:
         return "Xác nhận";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_INFO:
         return "Thông tin";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT:
         return "Thoát";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN:
         return "Cuộn xuống";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP:
         return "Cuộn lên";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_START:
         return "Start";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD:
         return "Bật/tắt bàn phím";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU:
         return "Bật/tắt trình đơn";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS:
         return "Trình đơn điều khiển căn bản";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_CONFIRM:
         return "Confirm/OK";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_INFO:
         return "Thông tin";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_QUIT:
         return "Thoát";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_SCROLL_UP:
         return "Cuộn lên";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_START:
         return "Những mặc định";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_KEYBOARD:
         return "Bật/tắt bàn phím";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_MENU:
         return "Bật/tắt trình đơn";
      case MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE:
         return "Khi tải savestate đừng ghi đè SaveRAM";
      case MENU_ENUM_LABEL_VALUE_BLUETOOTH_ENABLE:
         return "Kích hoạt Bluetooth";
      case MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL:
         return "URL của Buildbot Assets";
      case MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY:
         return "Danh mục cache";
      case MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW:
         return "Cho phép Camera";
      case MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER:
         return "Trình Camera";
      case MENU_ENUM_LABEL_VALUE_CHEAT:
         return "Cheat";
      case MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES:
         return "Áp dụng thay đổi của Cheat";
      case MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH:
         return "Danh mục tập tin Cheat";
      case MENU_ENUM_LABEL_VALUE_CHEAT_FILE:
         return "Tập tin Cheat";
      case MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD:
         return "Tải tập tin Cheat";
      case MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS:
         return "Lưu tập tin Cheat như";
      case MENU_ENUM_LABEL_VALUE_CHEAT_NUM_PASSES:
         return "Cheat Passes";
      case MENU_ENUM_LABEL_VALUE_CHEEVOS_DESCRIPTION:
         return "Miêu tả";
      case MENU_ENUM_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE:
         return "Chế độ Hardcore";
      case MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ACHIEVEMENTS:
         return "Thành tựu bị khóa:";
      case MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY:
         return "Đã khóa";
      case MENU_ENUM_LABEL_VALUE_CHEEVOS_SETTINGS:
         return "Những thành tựu Retro";
      case MENU_ENUM_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL:
         return "Thử nghiệm không chính thức";
      case MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ACHIEVEMENTS:
         return "Những thành tựu đã mở:";
      case MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY:
         return "Đã mở";
      case MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT:
         return "Đóng nội dung";
      case MENU_ENUM_LABEL_VALUE_CONFIG:
         return "Config";
      case MENU_ENUM_LABEL_VALUE_CONFIGURATIONS:
         return "Tải cấu hình";
      case MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS:
         return "Cấu hình";
      case MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT:
         return "Lưu cấu hình khi thoát";
      case MENU_ENUM_LABEL_VALUE_CONTENT_COLLECTION_LIST:
         return "Bộ sưu tập";
      case MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY:
         return "Mục cơ sở dữ liệu nội dung";
      case MENU_ENUM_LABEL_VALUE_CONTENT_DIR:
         return "Mục nội dung";
      case MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE:
         return "Kích thước danh sách lịch sử";
      case MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS:
         return "Trình đơn lẹ";
      case MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIR:
         return "Mục Core Assets";
      case MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY:
         return "Mục Downloads";
      case MENU_ENUM_LABEL_VALUE_CORE_CHEAT_OPTIONS:
         return "Cheats";
      case MENU_ENUM_LABEL_VALUE_CORE_COUNTERS:
         return "Core Counters";
      case MENU_ENUM_LABEL_VALUE_CORE_ENABLE:
         return "Hiển thị tên của core";
      case MENU_ENUM_LABEL_VALUE_CORE_INFORMATION:
         return "Core Thông tin";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS:
         return "Tác giả";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES:
         return "Thể loại";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL:
         return "Nhãn hiệu Core";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME:
         return "Tên của Core";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE:
         return "Phần vững";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES:
         return "Bản quyền";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS:
         return "Phép";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS:
         return "Tiện ích mở rộng được hỗ trợ";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER:
         return "Nhà sản xuất hệ thống";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME:
         return "Tên hệ thống";
      case MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS:
         return "Điều khiển";
      case MENU_ENUM_LABEL_VALUE_CORE_LIST:
         return "Tải Core";
      case MENU_ENUM_LABEL_VALUE_CORE_OPTIONS:
         return "Tùy chọn";
      case MENU_ENUM_LABEL_VALUE_CORE_SETTINGS:
         return "Core";
      case MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE:
/* FIXME? Translate 'Start a Core Automatically' */
         return "Tự động chạy Core";
      case MENU_ENUM_LABEL_VALUE_CORE_SPECIFIC_CONFIG:
         return "Mỗi Core mỗi cấu hình";
      case MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE:
         return "Tự động giải nén lưu trữ tải về";
      case MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL:
         return "URL của Buildbot Cores";
      case MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST:
         return "Cập nhật cho Core";
      case MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS:
         return "Cập nhật";
      case MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE:
         return "Kiến trúc CPU:";
      case MENU_ENUM_LABEL_VALUE_CPU_CORES:
         return "CPU Cores:";
      case MENU_ENUM_LABEL_VALUE_CURSOR_DIRECTORY:
         return "Mục Cursor";
      case MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER:
         return "Cursor Manager";
      case MENU_ENUM_LABEL_VALUE_CUSTOM_RATIO:
         return "Custom Ratio";
      case MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER:
         return "Quản lý cho cơ sở dữ liệu";
      case MENU_ENUM_LABEL_VALUE_DATABASE_SELECTION:
         return "Lựa chọn cơ sở dữ liệu";
      case MENU_ENUM_LABEL_VALUE_DELETE_ENTRY:
         return "Xoá";
      case MENU_ENUM_LABEL_VALUE_FAVORITES:
         return "Yêu thích";
      case MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT:
         return "<Mục nội dung>";
      case MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT:
         return "<Mặc định>";
      case MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE:
         return "<Không có gì>";
      case MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND:
         return "Không tìm thấy thư mục.";
      case MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS:
         return "Thư mục";
      case MENU_ENUM_LABEL_VALUE_DISABLED:
         return "Vô hiệu hoá";
      case MENU_ENUM_LABEL_VALUE_DISK_CYCLE_TRAY_STATUS:
         return "Tình trạng khay đĩa";
      case MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND:
         return "Thêm đĩa";
      case MENU_ENUM_LABEL_VALUE_DISK_INDEX:
         return "Chỉ số đĩa";
      case MENU_ENUM_LABEL_VALUE_DISK_OPTIONS:
         return "Điều khiển đĩa";
      case MENU_ENUM_LABEL_VALUE_DONT_CARE:
         return "Không quan tâm";
      case MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST:
         return "Mục Downloads";
      case MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE:
         return "Tải về Core...";
      case MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT:
         return "Tải về nội dung";
      case MENU_ENUM_LABEL_VALUE_DPI_OVERRIDE_VALUE:
         return "Kích hoạt DPI Override";
      case MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS:
         return "Trình điều khiển";
      case MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN:
/* FIXME? Translate 'Load Dummy on Core Shutdown' */
         return "Tải Core giả khi tắt máy";
      case MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER:
         return "Nền năng động";
      case MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY:
         return "Mục nền năng động";
      case MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE:
         return "Kích hoạt";
      case MENU_ENUM_LABEL_VALUE_ENTRY_HOVER_COLOR:
         return "Menu entry hover color";
      case MENU_ENUM_LABEL_VALUE_ENTRY_NORMAL_COLOR:
         return "Menu entry normal color";
      case MENU_ENUM_LABEL_VALUE_FALSE:
         return "False";
      case MENU_ENUM_LABEL_VALUE_FASTFORWARD_RATIO:
         return "Maximum Run Speed";
      case MENU_ENUM_LABEL_VALUE_FPS_SHOW:
         return "Display Framerate";
      case MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_ENABLE:
         return "Limit Maximum Run Speed";
      case MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS:
         return "Frame Throttle";
      case MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS:
         return "Frontend Counters";
      case MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS:
         return "Tải Content-Specific Core Options Automatically";
      case MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE:
         return "Create game-options file";
      case MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_IN_USE:
         return "Game-options file";
      case MENU_ENUM_LABEL_VALUE_HELP:
         return "help";
      case MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING:
         return "Âm thanh/Video Troubleshooting";
      case MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD:
         return "Changing Virtual Gamepad Overlay";
      case MENU_ENUM_LABEL_VALUE_HELP_CONTROLS:
         return "Basic Menu Controls";
      case MENU_ENUM_LABEL_VALUE_HELP_LIST:
         return "Help";
      case MENU_ENUM_LABEL_VALUE_HELP_LOADING_CONTENT:
         return "Đang tải Content";
      case MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT:
         return "Scanning For Content";
      case MENU_ENUM_LABEL_VALUE_HELP_WHAT_IS_A_CORE:
         return "What Is A Core?";
      case MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE:
         return "History List Enable";
      case MENU_ENUM_LABEL_VALUE_HISTORY_TAB:
         return "History";
      case MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU:
         return "Horizontal Menu";
      case MENU_ENUM_LABEL_VALUE_IMAGES_TAB:
         return "Image";
      case MENU_ENUM_LABEL_VALUE_INFORMATION:
         return "Information";
      case MENU_ENUM_LABEL_VALUE_INFORMATION_LIST:
         return "Information";
      case MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE:
         return "Analog To Digital Type";
      case MENU_ENUM_LABEL_VALUE_INPUT_ALL_USERS_CONTROL_MENU:
         return "All Users Control Menu";
      case MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X:
         return "Left Analog X";
      case MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_MINUS:
         return "Left analog X- (left)";
      case MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_PLUS:
         return "Left analog X+ (right)";
      case MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y:
         return "Left Analog Y";
      case MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_MINUS:
         return "Left analog Y- (up)";
      case MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_PLUS:
         return "Left analog Y+ (down)";
      case MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X:
         return "Right Analog X";
      case MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_MINUS:
         return "Right analog X- (left)";
      case MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_PLUS:
         return "Right analog X+ (right)";
      case MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y:
         return "Right Analog Y";
      case MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_MINUS:
         return "Right analog Y- (up)";
      case MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_PLUS:
         return "Right analog Y+ (down)";
      case MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE:
         return "Kích hoạt Autoconfig";
      case MENU_ENUM_LABEL_VALUE_INPUT_AXIS_THRESHOLD:
         return "Input Axis Threshold";
      case MENU_ENUM_LABEL_VALUE_INPUT_BIND_ALL:
         return "Bind All";
      case MENU_ENUM_LABEL_VALUE_INPUT_BIND_DEFAULT_ALL:
         return "Bind Default All";
      case MENU_ENUM_LABEL_VALUE_INPUT_BIND_TIMEOUT:
         return "Bind Timeout";
      case MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND:
         return "Hide Unbound Core Input Descriptors";
      case MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW:
         return "Display Input Descriptor Labels";
      case MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_INDEX:
         return "Device Index";
      case MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_TYPE:
         return "Device Type";
      case MENU_ENUM_LABEL_VALUE_INPUT_DRIVER:
         return "Input Driver";
      case MENU_ENUM_LABEL_VALUE_INPUT_DUTY_CYCLE:
         return "Duty Cycle";
      case MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS:
         return "Input Hotkey Binds";
      case MENU_ENUM_LABEL_VALUE_INPUT_ICADE_ENABLE:
         return "Kích hoạt Keyboard Gamepad Mapping";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_A:
         return "A button (right)";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_B:
         return "B button (down)";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_DOWN:
         return "Down D-pad";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L2:
         return "L2 button (trigger)";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L3:
         return "L3 button (thumb)";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L:
         return "L button (shoulder)";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_LEFT:
         return "Left D-pad";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R2:
         return "R2 button (trigger)";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R3:
         return "R3 button (thumb)";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R:
         return "R button (shoulder)";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_RIGHT:
         return "Right D-pad";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_SELECT:
         return "Select button";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_START:
         return "Start button";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_UP:
         return "Up D-pad";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_X:
         return "X button (top)";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_Y:
         return "Y button (left)";
      case MENU_ENUM_LABEL_VALUE_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE:
         return "Keyboard Gamepad Mapping Type";
      case MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS:
         return "Max Users";
      case MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO:
         return "Menu Toggle Gamepad Combo";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_MINUS:
         return "Cheat index -";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_PLUS:
         return "Cheat index +";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_TOGGLE:
         return "Cheat toggle";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE:
         return "Disk eject toggle";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_NEXT:
         return "Disk next";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_PREV:
         return "Disk prev";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_ENABLE_HOTKEY:
         return "Kích hoạt hotkeys";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_HOLD_KEY:
         return "Fast forward hold";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_KEY:
         return "Fast forward toggle";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_FRAMEADVANCE:
         return "Frameadvance";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY:
         return "Bật/tắt chế độ toàn màn hình";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_GRAB_MOUSE_TOGGLE:
         return "Grab mouse toggle";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_LOAD_STATE_KEY:
         return "Tải state";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE:
         return "Menu toggle";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_MOVIE_RECORD_TOGGLE:
         return "Movie record toggle";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_MUTE:
         return "Âm thanh mute toggle";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_FLIP:
         return "Netplay flip users";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_OSK:
         return "On-screen keyboard toggle";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_OVERLAY_NEXT:
         return "Overlay next";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_PAUSE_TOGGLE:
         return "Pause toggle";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY:
         return "Quit RetroArch";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_RESET:
         return "Reset game";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_REWIND:
         return "Rewind";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_STATE_KEY:
         return "Save state";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT:
         return "Chụp ảnh màn hình";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_NEXT:
         return "Next shader";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_PREV:
         return "Previous shader";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION:
         return "Slow motion";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_MINUS:
         return "Savestate slot -";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_PLUS:
         return "Savestate slot +";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_DOWN:
         return "Volume -";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_UP:
         return "Volume +";
      case MENU_ENUM_LABEL_VALUE_INPUT_OSK_OVERLAY_ENABLE:
         return "Display Keyboard Overlay";
      case MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE:
         return "Display Overlay";
      case MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU:
         return "Hide Overlay In Menu";
      case MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR:
         return "Poll Type Behavior";
      case MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_EARLY:
         return "Early";
      case MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_LATE:
         return "Late";
      case MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_NORMAL:
         return "Normal";
      case MENU_ENUM_LABEL_VALUE_INPUT_PREFER_FRONT_TOUCH:
         return "Prefer Front Touch";
      case MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY:
         return "Input Remapping Danh mục";
      case MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE:
         return "Kích hoạt Remap Binds";
      case MENU_ENUM_LABEL_VALUE_INPUT_SAVE_AUTOCONFIG:
         return "Lưu Autoconfig";
      case MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS:
         return "Input";
      case MENU_ENUM_LABEL_VALUE_INPUT_SMALL_KEYBOARD_ENABLE:
         return "Small Keyboard Enable";
      case MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_ENABLE:
         return "Kích hoạt Touch";
      case MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ENABLE:
         return "Kích hoạt Turbo";
      case MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD:
         return "Turbo Period";
      case MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS:
         return "Input User %u Binds";
      case MENU_ENUM_LABEL_VALUE_INTERNAL_STORAGE_STATUS:
         return "Internal storage status";
      case MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR:
         return "Input Device Autoconfig Danh mục";
      case MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER:
         return "Joypad Driver";
      case MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES:
         return "Services";
      case MENU_ENUM_LABEL_VALUE_LANG_CHINESE_SIMPLIFIED:
         return "Tiếng Trung Quốc (Chữ Hán giản thể)";
      case MENU_ENUM_LABEL_VALUE_LANG_CHINESE_TRADITIONAL:
         return "Tiếng Trung Quốc (Chữ Hán phồn thể)";
      case MENU_ENUM_LABEL_VALUE_LANG_DUTCH:
         return "Tiếng Hà Lan";
      case MENU_ENUM_LABEL_VALUE_LANG_ENGLISH:
         return "Tiếng Anh";
      case MENU_ENUM_LABEL_VALUE_LANG_ESPERANTO:
         return "Tiếng Quốc tế ngữ";
      case MENU_ENUM_LABEL_VALUE_LANG_FRENCH:
         return "Tiếng Pháp";
      case MENU_ENUM_LABEL_VALUE_LANG_GERMAN:
         return "Tiếng Đức";
      case MENU_ENUM_LABEL_VALUE_LANG_ITALIAN:
         return "Tiếng Ý";
      case MENU_ENUM_LABEL_VALUE_LANG_JAPANESE:
         return "Tiếng Nhật";
      case MENU_ENUM_LABEL_VALUE_LANG_KOREAN:
         return "Tiếng Hàn Quốc";
      case MENU_ENUM_LABEL_VALUE_LANG_POLISH:
         return "Polish";
      case MENU_ENUM_LABEL_VALUE_LANG_PORTUGUESE:
         return "Tiếng Bồ Đào Nha";
      case MENU_ENUM_LABEL_VALUE_LANG_RUSSIAN:
         return "Tiếng Nga";
      case MENU_ENUM_LABEL_VALUE_LANG_SPANISH:
         return "Tiếng Tây Ban Nha";
      case MENU_ENUM_LABEL_VALUE_LANG_VIETNAMESE:
         return "Tiếng Việt";
      case MENU_ENUM_LABEL_VALUE_LEFT_ANALOG:
         return "Left Analog";
      case MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH:
         return "Core Danh mục";
      case MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH:
         return "Core Info Danh mục";
      case MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL:
         return "Core Logging Level";
      case MENU_ENUM_LABEL_VALUE_LINEAR:
         return "Linear";
      case MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE:
         return "Tải Archive With Core";
      case MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY:
         return "Tải Recent";
      case MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST:
         return "Tải Content";
      case MENU_ENUM_LABEL_VALUE_LOAD_STATE:
         return "Tải State";
      case MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW:
         return "Allow Location";
      case MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER:
         return "Location Driver";
      case MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS:
         return "Logging";
      case MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY:
         return "Logging Verbosity";
      case MENU_ENUM_LABEL_VALUE_MAIN_MENU:
         return "Main Menu";
      case MENU_ENUM_LABEL_VALUE_MANAGEMENT:
         return "Database thiết lập";
      case MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME:
         return "Menu Color Theme";
      case MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE:
         return "Blue";
      case MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE_GREY:
         return "Blue Grey";
      case MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_DARK_BLUE:
         return "Dark Blue";
      case MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GREEN:
         return "Green";
      case MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NVIDIA_SHIELD:
         return "Shield";
      case MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_RED:
         return "Red";
      case MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_YELLOW:
         return "Yellow";
      case MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_FOOTER_OPACITY:
         return "Footer Opacity";
      case MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_HEADER_OPACITY:
         return "Header Opacity";
      case MENU_ENUM_LABEL_VALUE_MENU_DRIVER:
         return "Menu Driver";
      case MENU_ENUM_LABEL_VALUE_MENU_ENUM_THROTTLE_FRAMERATE:
         return "Throttle Menu Framerate";
      case MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS:
         return "Menu File Browser";
      case MENU_ENUM_LABEL_VALUE_MENU_LINEAR_FILTER:
         return "Menu Linear Filter";
      case MENU_ENUM_LABEL_VALUE_MENU_SETTINGS:
         return "Menu";
      case MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER:
         return "Background";
      case MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER_OPACITY:
         return "Background opacity";
      case MENU_ENUM_LABEL_VALUE_MISSING:
         return "Missing";
      case MENU_ENUM_LABEL_VALUE_MORE:
         return "...";
      case MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE:
         return "Mouse Support";
      case MENU_ENUM_LABEL_VALUE_MULTIMEDIA_SETTINGS:
         return "Multimedia";
      case MENU_ENUM_LABEL_VALUE_MUSIC_TAB:
         return "Music";
      case MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
         return "Filter unknown extensions";
      case MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND:
         return "Navigation Wrap-Around";
      case MENU_ENUM_LABEL_VALUE_NEAREST:
         return "Nearest";
      case MENU_ENUM_LABEL_VALUE_NETPLAY:
         return "Netplay";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_CHECK_FRAMES:
         return "Netplay Check Frames";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_CLIENT_SWAP_INPUT:
         return "Netplay P2 Uses C1";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_DELAY_FRAMES:
         return "Netplay Delay Frames";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_DISCONNECT:
         return "Disconnect";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE:
         return "Kích hoạt Netplay";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_CLIENT:
         return "Connect to Netplay host";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_HOST:
         return "Start hosting";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS:
         return "Server Address";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_MODE:
         return "Kích hoạt Netplay Client";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME:
         return "Username";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_SETTINGS:
         return "Netplay thiết lập";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE:
         return "Kích hoạt Netplay Spectator";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT:
         return "Netplay TCP/UDP Port";
      case MENU_ENUM_LABEL_VALUE_NETWORK_CMD_ENABLE:
         return "Network Commands";
      case MENU_ENUM_LABEL_VALUE_NETWORK_CMD_PORT:
         return "Network Command Port";
      case MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION:
         return "Network Information";
      case MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_ENABLE:
         return "Network Gamepad";
      case MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_PORT:
         return "Network Remote Base Port";
      case MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS:
         return "Network";
      case MENU_ENUM_LABEL_VALUE_NO:
         return "No";
      case MENU_ENUM_LABEL_VALUE_NONE:
         return "None";
      case MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE:
         return "N/A";
      case MENU_ENUM_LABEL_VALUE_NO_ACHIEVEMENTS_TO_DISPLAY:
         return "No achievements to display.";
      case MENU_ENUM_LABEL_VALUE_NO_CORE:
         return "No Core";
      case MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE:
         return "No cores available.";
      case MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE:
         return "No core information available.";
      case MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE:
         return "No core options available.";
      case MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY:
         return "No entries to display.";
      case MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE:
         return "No history available.";
      case MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE:
         return "No information is available.";
      case MENU_ENUM_LABEL_VALUE_NO_ITEMS:
         return "No items.";
      case MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS:
         return "No performance counters.";
      case MENU_ENUM_LABEL_VALUE_NO_PLAYLISTS:
         return "No playlists.";
      case MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE:
         return "No playlist entries available.";
      case MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND:
         return "No thiết lập found.";
      case MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS:
         return "No shader parameters.";
      case MENU_ENUM_LABEL_VALUE_OFF:
         return "OFF";
      case MENU_ENUM_LABEL_VALUE_ON:
         return "ON";
      case MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER:
         return "Online Updater";
      case MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS:
         return "Onscreen Display";
      case MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS:
         return "Onscreen Overlay";
      case MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE:
         return "Open Archive As Folder";
      case MENU_ENUM_LABEL_VALUE_OPTIONAL:
         return "Optional";
      case MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_DIRECTORY:
         return "OSK Overlay Dir";
      case MENU_ENUM_LABEL_VALUE_OVERLAY:
         return "Overlay";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED:
         return "Tự động tải Preferred Overlay";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY:
         return "Overlay Dir";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY:
         return "Overlay Opacity";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET:
         return "Overlay Preset";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE:
         return "Overlay Scale";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS:
         return "Onscreen Overlay";
      case MENU_ENUM_LABEL_VALUE_PAL60_ENABLE:
         return "Use PAL60 Mode";
      case MENU_ENUM_LABEL_VALUE_PARENT_DIRECTORY:
         return "Parent directory";
      case MENU_ENUM_LABEL_VALUE_PAUSE_LIBRETRO:
         return "Pause when menu activated";
      case MENU_ENUM_LABEL_VALUE_PAUSE_NONACTIVE:
         return "Don't run in background";
      case MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE:
         return "Performance Counters";
      case MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB:
         return "Playlists";
      case MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY:
         return "Playlist Danh mục";
      case MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS:
         return "Playlists";
      case MENU_ENUM_LABEL_VALUE_POINTER_ENABLE:
         return "Touch Support";
      case MENU_ENUM_LABEL_VALUE_PORT:
         return "Port";
      case MENU_ENUM_LABEL_VALUE_PRESENT:
         return "Present";
      case MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS:
         return "Privacy";
      case MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH:
         return "Thoát RetroArch";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ANALOG:
         return "Analog supported";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_BBFC_RATING:
         return "BBFC Rating";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CERO_RATING:
         return "CERO Rating";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_COOP:
         return "Co-op supported";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CRC32:
         return "CRC32";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION:
         return "Description";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER:
         return "Developer";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_ISSUE:
         return "Edge Magazine Issue";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_RATING:
         return "Edge Magazine Rating";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_REVIEW:
         return "Edge Magazine Review";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ELSPA_RATING:
         return "ELSPA Rating";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ENHANCEMENT_HW:
         return "Enhancement Hardware";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ESRB_RATING:
         return "ESRB Rating";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FAMITSU_MAGAZINE_RATING:
         return "Famitsu Magazine Rating";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FRANCHISE:
         return "Franchise";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GENRE:
         return "Genre";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_MD5:
         return "MD5";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME:
         return "Name";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN:
         return "Origin";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PEGI_RATING:
         return "PEGI Rating";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER:
         return "Publisher";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH:
         return "Releasedate Month";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR:
         return "Releasedate Year";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RUMBLE:
         return "Rumble supported";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SERIAL:
         return "Serial";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SHA1:
         return "SHA1";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_START_CONTENT:
         return "Start Content";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_TGDB_RATING:
         return "TGDB Rating";
      case MENU_ENUM_LABEL_VALUE_REBOOT:
         return "Khởi động lại";
      case MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY:
         return "Recording Config Danh mục";
      case MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY:
         return "Recording Output Danh mục";
      case MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS:
         return "Recording";
      case MENU_ENUM_LABEL_VALUE_RECORD_CONFIG:
         return "Load Recording Config...";
      case MENU_ENUM_LABEL_VALUE_RECORD_DRIVER:
         return "Record Driver";
      case MENU_ENUM_LABEL_VALUE_RECORD_ENABLE:
         return "Enabel Recording";
      case MENU_ENUM_LABEL_VALUE_RECORD_PATH:
         return "Save Recording Output as...";
      case MENU_ENUM_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY:
         return "Save Recordings in Output Dir";
      case MENU_ENUM_LABEL_VALUE_REMAP_FILE:
         return "Remap File";
      case MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD:
         return "Tải Remap File";
      case MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE:
         return "Save Core Remap File";
      case MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME:
         return "Save Game Remap File";
      case MENU_ENUM_LABEL_VALUE_REQUIRED:
         return "Required";
      case MENU_ENUM_LABEL_VALUE_RESTART_CONTENT:
         return "Khởi động lại";
      case MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH:
         return "Restart RetroArch";
      case MENU_ENUM_LABEL_VALUE_RESUME:
         return "Resume";
      case MENU_ENUM_LABEL_VALUE_RESUME_CONTENT:
         return "Resume";
      case MENU_ENUM_LABEL_VALUE_RETROKEYBOARD:
         return "RetroKeyboard";
      case MENU_ENUM_LABEL_VALUE_RETROPAD:
         return "RetroPad";
      case MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS:
         return "Retro Achievements";
      case MENU_ENUM_LABEL_VALUE_REWIND_ENABLE:
         return "Kích hoạt Rewind";
      case MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY:
         return "Rewind Granularity";
      case MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS:
         return "Rewind";
      case MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY:
         return "File Browser Danh mục";
      case MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY:
         return "Config Danh mục";
      case MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN:
         return "Show Start Screen";
      case MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG:
         return "Right Analog";
      case MENU_ENUM_LABEL_VALUE_RUN:
         return "Run";
      case MENU_ENUM_LABEL_VALUE_SAMBA_ENABLE:
         return "Kích hoạt SAMBA";
      case MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY:
         return "Savefile Danh mục";
      case MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX:
         return "Save State Auto Index";
      case MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD:
         return "Tự động tải State";
      case MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE:
         return "Auto Save State";
      case MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY:
         return "Savestate Danh mục";
      case MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG:
         return "Save Current Config";
      case MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CORE:
         return "Save Core Overrides";
      case MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME:
         return "Save Game Overrides";
      case MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG:
         return "Save New Config";
      case MENU_ENUM_LABEL_VALUE_SAVE_STATE:
         return "Save State";
      case MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS:
         return "Saving";
      case MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY:
         return "Scan Directory";
      case MENU_ENUM_LABEL_VALUE_SCAN_FILE:
         return "Scan File";
      case MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY:
         return "<Scan This Directory>";
      case MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY:
         return "Screenshot Danh mục";
      case MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION:
         return "Screen Resolution";
      case MENU_ENUM_LABEL_VALUE_SEARCH:
         return "Search";
      case MENU_ENUM_LABEL_VALUE_SECONDS:
         return "seconds";
      case MENU_ENUM_LABEL_VALUE_SETTINGS:
         return "Thiết lập";
      case MENU_ENUM_LABEL_VALUE_SETTINGS_TAB:
         return "Thiết lập";
      case MENU_ENUM_LABEL_VALUE_SHADER:
         return "Shader";
      case MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES:
         return "Áp dụng Shader Changes";
      case MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS:
         return "Shaders";
      case MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON:
         return "Ribbon";
      case MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON_SIMPLIFIED:
         return "Ribbon (simplified)";
      case MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS:
         return "Show Advanced thiết lập";
      case MENU_ENUM_LABEL_VALUE_SHOW_HIDDEN_FILES:
         return "Show Hidden Files and Folders";
      case MENU_ENUM_LABEL_VALUE_SHUTDOWN:
         return "Tắt Máy";
      case MENU_ENUM_LABEL_VALUE_SLOWMOTION_RATIO:
         return "Slow-Motion Ratio";
      case MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE:
         return "Sort Saves In Folders";
      case MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE:
         return "Sort Savestates In Folders";
      case MENU_ENUM_LABEL_VALUE_SSH_ENABLE:
         return "Kích hoạt SSH";
      case MENU_ENUM_LABEL_VALUE_START_CORE:
         return "Start Core";
      case MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD:
         return "Start Remote RetroPad";
      case MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR:
         return "Start Video Processor";
      case MENU_ENUM_LABEL_VALUE_STATE_SLOT:
         return "State Slot";
      case MENU_ENUM_LABEL_VALUE_STATUS:
         return "Status";
      case MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE:
         return "stdin Commands";
      case MENU_ENUM_LABEL_VALUE_SUPPORTED_CORES:
         return "Supported cores";
      case MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE:
         return "Suspend Screensaver";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_BGM_ENABLE:
         return "Kích hoạt System BGM";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY:
         return "System/BIOS Danh mục";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION:
         return "System Information";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT:
         return "7zip support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT:
         return "ALSA support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE:
         return "Build date";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT:
         return "Cg support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT:
         return "Cocoa support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT:
         return "Command interface support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT:
         return "CoreText support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES:
         return "CPU Features";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI:
         return "Display metric DPI";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT:
         return "Display metric height (mm)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH:
         return "Display metric width (mm)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT:
         return "DirectSound support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT:
         return "Dynamic library support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYNAMIC_SUPPORT:
         return "Dynamic run-time loading of libretro library";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT:
         return "EGL support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT:
         return "OpenGL/Direct3D render-to-texture (multi-pass shaders) support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT:
         return "FFmpeg support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT:
         return "FreeType support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER:
         return "Frontend identifier";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME:
         return "Frontend name";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS:
         return "Frontend OS";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION:
         return "Git version";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT:
         return "GLSL support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT:
         return "HLSL support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT:
         return "JACK support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT:
         return "KMS/EGL support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT:
         return "LibretroDB support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT:
         return "Libusb support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBXML2_SUPPORT:
         return "libxml2 XML parsing support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT:
         return "Netplay (peer-to-peer) support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT:
         return "Network Command interface support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_REMOTE_SUPPORT:
         return "Network Gamepad support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT:
         return "OpenAL support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT:
         return "OpenGL ES support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT:
         return "OpenGL support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT:
         return "OpenSL support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT:
         return "OpenVG support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT:
         return "OSS support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT:
         return "Overlay support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE:
         return "Power source";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED:
         return "Charged";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING:
         return "Charging";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING:
         return "Discharging";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE:
         return "No source";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT:
         return "PulseAudio support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PYTHON_SUPPORT:
         return "Python (script support in shaders) support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RBMP_SUPPORT:
         return "BMP support (RBMP)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL:
         return "RetroRating level";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RJPEG_SUPPORT:
         return "JPEG support (RJPEG)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT:
         return "RoarAudio support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT:
         return "PNG support (RPNG)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT:
         return "RSound support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RTGA_SUPPORT:
         return "TGA support (RTGA)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT:
         return "SDL2 support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT:
         return "SDL image support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT:
         return "SDL1.2 support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SLANG_SUPPORT:
         return "Slang support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT:
         return "Threading support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT:
         return "Udev support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT:
         return "Video4Linux2 support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER:
         return "Video context driver";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VULKAN_SUPPORT:
         return "Vulkan support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT:
         return "Wayland support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT:
         return "X11 support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT:
         return "XAudio2 support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT:
         return "XVideo support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT:
         return "Zlib support";
      case MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT:
         return "Chụp ảnh màn hình";
      case MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE:
         return "Threaded data runloop";
      case MENU_ENUM_LABEL_VALUE_THUMBNAILS:
         return "Thumbnails";
      case MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY:
         return "Thumbnails Danh mục";
      case MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST:
         return "Thumbnails Updater";
      case MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_BOXARTS:
         return "Boxarts";
      case MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_SCREENSHOTS:
         return "Screenshots";
      case MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_TITLE_SCREENS:
         return "Title Screens";
      case MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE:
         return "Display time / date";
      case MENU_ENUM_LABEL_VALUE_TITLE_COLOR:
         return "Menu title color";
      case MENU_ENUM_LABEL_VALUE_TRUE:
         return "True";
      case MENU_ENUM_LABEL_VALUE_UI_COMPANION_ENABLE:
         return "Kích hoạt UI Companion";
      case MENU_ENUM_LABEL_VALUE_UI_COMPANION_START_ON_BOOT:
         return "UI Companion Start On Boot";
      case MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE:
         return "Menubar";
      case MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE:
         return "Unable to read compressed file.";
      case MENU_ENUM_LABEL_VALUE_UNDO_LOAD_STATE:
         return "Undo Tải State";
      case MENU_ENUM_LABEL_VALUE_UNDO_SAVE_STATE:
         return "Undo Save State";
      case MENU_ENUM_LABEL_VALUE_UNKNOWN:
         return "Unknown";
      case MENU_ENUM_LABEL_VALUE_UPDATER_SETTINGS:
         return "Updater";
      case MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS:
         return "Cập nhật Assets";
      case MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES:
         return "Cập nhật Autoconfig Profiles";
      case MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS:
         return "Cập nhật Cg Shaders";
      case MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS:
         return "Cập nhật Cheats";
      case MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES:
         return "Cập nhật Core Info Files";
      case MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES:
         return "Cập nhật Databases";
      case MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS:
         return "Cập nhật GLSL Shaders";
      case MENU_ENUM_LABEL_VALUE_UPDATE_LAKKA:
         return "Cập nhật Lakka";
      case MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS:
         return "Cập nhật Overlays";
      case MENU_ENUM_LABEL_VALUE_UPDATE_SLANG_SHADERS:
         return "Cập nhật Slang Shaders";
      case MENU_ENUM_LABEL_VALUE_USER:
         return "User";
      case MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS:
         return "User Interface";
      case MENU_ENUM_LABEL_VALUE_USER_LANGUAGE:
         return "Language";
      case MENU_ENUM_LABEL_VALUE_USER_SETTINGS:
         return "User";
      case MENU_ENUM_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER:
         return "Use Builtin Image Viewer";
      case MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER:
         return "Use Builtin Media Player";
      case MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY:
         return "<Use this directory>";
      case MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE:
         return "Allow rotation";
      case MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO:
         return "Auto Aspect Ratio";
      case MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX:
         return "Aspect Ratio Index";
      case MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION:
         return "Black Frame Insertion";
      case MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN:
         return "Crop Overscan (Reload)";
      case MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION:
         return "Disable Desktop Composition";
      case MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER:
         return "Video Driver";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FILTER:
         return "Video Filter";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR:
         return "Video Filter Danh mục";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_FLICKER:
         return "Flicker filter";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE:
         return "Display OSD Message";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH:
         return "OSD Message Font";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE:
         return "OSD Message Size";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_ASPECT:
         return "Force aspect ratio";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE:
         return "Force-disable sRGB FBO";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY:
         return "Frame Delay";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN:
         return "Sử dụng chế độ toàn màn hình";
      case MENU_ENUM_LABEL_VALUE_VIDEO_GAMMA:
         return "Video Gamma";
      case MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD:
         return "Use GPU Recording";
      case MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT:
         return "Kích hoạt GPU Screenshot";
      case MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC:
         return "Hard GPU Sync";
      case MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES:
         return "Hard GPU Sync Frames";
      case MENU_ENUM_LABEL_VALUE_VIDEO_MAX_SWAPCHAIN_IMAGES:
         return "Max swapchain images";
      case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X:
         return "OSD Message X Position";
      case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y:
         return "OSD Message Y Position";
      case MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX:
         return "Monitor Index";
      case MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD:
         return "Kích hoạt Post Filter Recording";
      case MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE:
         return "Refresh Rate";
      case MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO:
         return "Estimated Monitor Framerate";
      case MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION:
         return "Rotation";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SCALE:
         return "Windowed Scale";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER:
         return "Integer Scale";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS:
         return "Video";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR:
         return "Video Shader Danh mục";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES:
         return "Shader Passes";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS:
         return "Preview Shader Parameters";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET:
         return "Tải Shader Preset";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_PARAMETERS:
         return "Menu Shader Parameters";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS:
         return "Save Shader Preset As";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CORE:
         return "Save Core Preset";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME:
         return "Save Game Preset";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT:
/* FIXME? Translate 'Enable Hardware Shared Context' */
         return "Kích hoạt Hardware Shared Context";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH:
         return "HW Bilinear Filtering";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER:
         return "Kích hoạt Soft Filter";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL:
         return "VSync Swap Interval";
      case MENU_ENUM_LABEL_VALUE_VIDEO_TAB:
         return "Video";
      case MENU_ENUM_LABEL_VALUE_VIDEO_THREADED:
         return "Threaded Video";
      case MENU_ENUM_LABEL_VALUE_VIDEO_VFILTER:
         return "Deflicker";
      case MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_HEIGHT:
         return "Custom Viewport Height";
      case MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_WIDTH:
         return "Custom Viewport Width";
      case MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_X:
         return "Custom Viewport X";
      case MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_Y:
         return "Custom Viewport Y";
      case MENU_ENUM_LABEL_VALUE_VIDEO_VI_WIDTH:
         return "Set VI Screen Width";
      case MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC:
         return "VSync";
      case MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN:
         return "Chế độ toàn màn hình trong khung";
      case MENU_ENUM_LABEL_VALUE_WIFI_DRIVER:
         return "Wi-Fi Driver";
      case MENU_ENUM_LABEL_VALUE_WIFI_SETTINGS:
         return "Wi-Fi";
      case MENU_ENUM_LABEL_VALUE_XMB_ALPHA_FACTOR:
         return "Menu Alpha Factor";
      case MENU_ENUM_LABEL_VALUE_XMB_FONT:
         return "Menu Font";
      case MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_CUSTOM:
         return "Custom";
      case MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_FLATUI:
         return "FlatUI";
      case MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME:
         return "Monochrome";
      case MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_SYSTEMATIC:
         return "Systematic";
      case MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_PIXEL:
         return "Pixel";
      case MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_RETROACTIVE:
         return "RetroActive";
      case MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME:
         return "Menu Color Theme";
      case MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_APPLE_GREEN:
         return "Apple Green";
      case MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK:
         return "Dark";
      case MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK_PURPLE:
         return "Dark Purple";
      case MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ELECTRIC_BLUE:
         return "Electric Blue";
      case MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GOLDEN:
         return "Golden";
      case MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LEGACY_RED:
         return "Legacy Red";
      case MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDNIGHT_BLUE:
         return "Midnight Blue";
      case MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PLAIN:
         return "Plain";
      case MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_UNDERSEA:
         return "Undersea";
      case MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_VOLCANIC_RED:
         return "Volcanic Red";
      case MENU_ENUM_LABEL_VALUE_XMB_RIBBON_ENABLE:
         return "Menu Shader Pipeline";
      case MENU_ENUM_LABEL_VALUE_XMB_SCALE_FACTOR:
         return "Menu Scale Factor";
      case MENU_ENUM_LABEL_VALUE_XMB_SHADOWS_ENABLE:
         return "Kích hoạt Icon Shadows";
      case MENU_ENUM_LABEL_VALUE_XMB_SHOW_HISTORY:
         return "Show History Tab";
      case MENU_ENUM_LABEL_VALUE_XMB_SHOW_IMAGES:
         return "Show Images Tab";
      case MENU_ENUM_LABEL_VALUE_XMB_SHOW_MUSIC:
         return "Show Music Tab";
      case MENU_ENUM_LABEL_VALUE_XMB_SHOW_SETTINGS:
         return "Show thiết lập Tab";
      case MENU_ENUM_LABEL_VALUE_XMB_SHOW_VIDEO:
         return "Show Video Tab";
      case MENU_ENUM_LABEL_VALUE_XMB_THEME:
         return "Menu Icon Theme";
      case MENU_ENUM_LABEL_VALUE_YES:
         return "Yes";
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_TWO:
         return "Shader Preset";
      case MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST:
         return "Tải/quét nội dung và thêm vào bộ sưu tập.";
      case MENU_ENUM_SUBLABEL_AUDIO_SETTINGS:
         return "Điều chỉnh thiết lập cho âm thanh ra.";
      case MENU_ENUM_SUBLABEL_BLUETOOTH_ENABLE:
         return "Bật/tắt bluetooth.";
      case MENU_ENUM_SUBLABEL_CONFIG_SAVE_ON_EXIT:
         return "Lưu cấu hình khi thoát retroarch.";
      case MENU_ENUM_SUBLABEL_CONFIGURATION_SETTINGS:
         return "Change default settings for configuration files.";
      case MENU_ENUM_SUBLABEL_CONFIGURATIONS_LIST:
         return "Manage and create configuration files.";
      case MENU_ENUM_SUBLABEL_CPU_CORES:
         return "Số lượng lõi của CPU.";
      case MENU_ENUM_SUBLABEL_FPS_SHOW:
         return "Hiển thị tốc độ khung hình/giây trên màn hình.";
      case MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS:
         return "Đặt cấu hình thiết lập của hotkey.";
      case MENU_ENUM_SUBLABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO:
         return "Kết hợp nút Gamepad để vào menu bật/tắt.";
      case MENU_ENUM_SUBLABEL_INPUT_SETTINGS:
         return "Điều chỉnh thiết lập cho joypads, bàn phím và chuột.";
      case MENU_ENUM_SUBLABEL_INPUT_USER_BINDS:
         return "Đặt cấu hình điều khiển cho người dùng này.";
      case MENU_ENUM_SUBLABEL_LOG_VERBOSITY:
         return "Kích hoạt or disable logging to the terminal.";
      case MENU_ENUM_SUBLABEL_NETPLAY:
         return "Tham gia hoặc làm máy chủ cho netplay.";
      case MENU_ENUM_SUBLABEL_ONLINE_UPDATER:
         return "Tải/cập nhật tiện ích và thành phần của RetroArch.";
      case MENU_ENUM_SUBLABEL_SAMBA_ENABLE:
         return "Bật/tắt chia sẻ thư mục trên mạng.";
      case MENU_ENUM_SUBLABEL_SERVICES_SETTINGS:
         return "Quản lý dịch vụ của hệ điều hành.";
      case MENU_ENUM_SUBLABEL_SHOW_HIDDEN_FILES:
         return "Hiện ra tập tin và thư mục ẩn trong trình duyệt tập tin.";
      case MENU_ENUM_SUBLABEL_SSH_ENABLE:
         return "Bật/tắt giao thức SSH.";
      case MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE:
         return "Chặn tính năng screensaver (màn hình chờ).";
      case MENU_ENUM_SUBLABEL_USER_LANGUAGE:
         return "Thiết lập ngôn ngữ của giao diện .";
      case MENU_ENUM_SUBLABEL_VIDEO_BLACK_FRAME_INSERTION:
         return "Inserts a black frame inbetween frames. Useful for users of 120 Hz monitors who want to play 60 Hz material with eliminated ghosting.";
      case MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY:
         return "Reduces latency at the cost of higher risk of video stuttering. Adds a delay after V-Sync (in ms).";
      case MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC_FRAMES:
         return "Sets how many frames the CPU can run ahead of the GPU when using 'Hard GPU Sync'.";
      case MENU_ENUM_SUBLABEL_VIDEO_MAX_SWAPCHAIN_IMAGES:
         return "Tells the video driver to explicitly use a specified buffering mode.";
      case MENU_ENUM_SUBLABEL_VIDEO_MONITOR_INDEX:
         return "Chọn màn hình hiển thị để sử dụng.";
      case MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_AUTO:
         return "The accurate estimated refresh rate of the monitor in Hz.";
      case MENU_ENUM_SUBLABEL_VIDEO_SETTINGS:
         return "Điều chỉnh thiết lập cho video ra.";
      case MENU_ENUM_SUBLABEL_WIFI_SETTINGS:
         return "Tìm mạng không dây và thiết lập kết nối.";
      case MSG_APPENDED_DISK:
         return "Appended disk";
      case MSG_APPLICATION_DIR:
         return "Application Danh mục";
      case MSG_APPLYING_CHEAT:
         return "Đang áp dụng cheat changes.";
      case MSG_APPLYING_SHADER:
         return "Đang áp dụng shader";
      case MSG_AUDIO_MUTED:
         return "Âm thanh muted.";
      case MSG_AUDIO_UNMUTED:
         return "Âm thanh unmuted.";
      case MSG_AUTOCONFIG_FILE_ERROR_SAVING:
         return "Tập tin Autoconfig bị lỗi khi lưu.";
      case MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY:
         return "Tập tin Autoconfig đã lưu thành công.";
      case MSG_AUTOSAVE_FAILED:
         return "Could not initialize autosave.";
      case MSG_AUTO_SAVE_STATE_TO:
         return "Auto save state to";
      case MSG_BLOCKING_SRAM_OVERWRITE:
         return "Blocking SRAM Overwrite";
      case MSG_BRINGING_UP_COMMAND_INTERFACE_ON_PORT:
         return "Đang đưa lên lệnh giao diện trên cổng";
      case MSG_BYTES:
         return "bytes";
      case MSG_CANNOT_INFER_NEW_CONFIG_PATH:
         return "Cannot infer new config path. Use current time.";
      case MSG_CHEEVOS_HARDCORE_MODE_ENABLE:
         return "Hardcore Mode Enabled: savestate & rewind were disabled.";
      case MSG_COMPARING_WITH_KNOWN_MAGIC_NUMBERS:
         return "Comparing with known magic numbers...";
      case MSG_COMPILED_AGAINST_API:
         return "Compiled against API";
      case MSG_CONFIG_DIRECTORY_NOT_SET:
         return "Config directory not set. Cannot save new config.";
      case MSG_CONNECTED_TO:
         return "Connected to";
      case MSG_CONTENT_CRC32S_DIFFER:
         return "Content CRC32s differ. Cannot use different games.";
      case MSG_CONTENT_LOADING_SKIPPED_IMPLEMENTATION_WILL_DO_IT:
         return "Content loading skipped. Implementation will tải it on its own.";
      case MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES:
         return "Core does not support save states.";
      case MSG_CORE_OPTIONS_FILE_CREATED_SUCCESSFULLY:
         return "Core options file created successfully.";
      case MSG_COULD_NOT_FIND_ANY_NEXT_DRIVER:
         return "Could not find any next driver";
      case MSG_COULD_NOT_FIND_COMPATIBLE_SYSTEM:
         return "Could not find compatible system.";
      case MSG_COULD_NOT_FIND_VALID_DATA_TRACK:
         return "Could not find valid data track";
      case MSG_COULD_NOT_OPEN_DATA_TRACK:
         return "could not open data track";
      case MSG_COULD_NOT_READ_CONTENT_FILE:
         return "Could not read content file";
      case MSG_COULD_NOT_READ_MOVIE_HEADER:
         return "Could not read movie header.";
      case MSG_COULD_NOT_READ_STATE_FROM_MOVIE:
         return "Could not read state from movie.";
      case MSG_CRC32_CHECKSUM_MISMATCH:
         return "CRC32 checksum mismatch between content file and saved content checksum in replay file header; replay highly likely to desync on playback.";
      case MSG_CUSTOM_TIMING_GIVEN:
         return "Custom timing given";
      case MSG_DECOMPRESSION_ALREADY_IN_PROGRESS:
         return "Decompression already in progress.";
      case MSG_DECOMPRESSION_FAILED:
         return "Decompression failed.";
      case MSG_DETECTED_VIEWPORT_OF:
         return "Detected viewport of";
      case MSG_DID_NOT_FIND_A_VALID_CONTENT_PATCH:
         return "Did not find a valid content patch.";
      case MSG_DISCONNECT_DEVICE_FROM_A_VALID_PORT:
         return "Ngắt kết nối thiết bị từ cổng hợp lệ.";
      case MSG_DISK_CLOSED:
         return "Closed";
      case MSG_DISK_EJECTED:
         return "Ejected";
      case MSG_DOWNLOADING:
         return "Downloading";
      case MSG_DOWNLOAD_FAILED:
         return "Download failed";
      case MSG_ERROR:
         return "Error";
      case MSG_ERROR_LIBRETRO_CORE_REQUIRES_CONTENT:
         return "Libretro core requires content, but nothing was provided.";
      case MSG_ERROR_LIBRETRO_CORE_REQUIRES_SPECIAL_CONTENT:
         return "Libretro core requires special content, but none were provided.";
      case MSG_ERROR_PARSING_ARGUMENTS:
         return "Error parsing arguments.";
      case MSG_ERROR_SAVING_CORE_OPTIONS_FILE:
         return "Error saving core options file.";
      case MSG_ERROR_SAVING_REMAP_FILE:
         return "Error saving remap file.";
      case MSG_ERROR_SAVING_SHADER_PRESET:
         return "Error saving shader preset.";
      case MSG_EXTERNAL_APPLICATION_DIR:
         return "External Application Danh mục";
      case MSG_EXTRACTING:
         return "Extracting";
      case MSG_EXTRACTING_FILE:
         return "Extracting file";
      case MSG_FAILED_SAVING_CONFIG_TO:
         return "Failed saving config to";
      case MSG_FAILED_TO:
         return "Failed to";
      case MSG_FAILED_TO_ACCEPT_INCOMING_SPECTATOR:
         return "Failed to accept incoming spectator.";
      case MSG_FAILED_TO_ALLOCATE_MEMORY_FOR_PATCHED_CONTENT:
         return "Failed to allocate memory for patched content...";
      case MSG_FAILED_TO_APPLY_SHADER:
         return "Failed to áp dụng shader.";
      case MSG_FAILED_TO_BIND_SOCKET:
         return "Failed to bind socket.";
      case MSG_FAILED_TO_CREATE_THE_DIRECTORY:
         return "Failed to create the directory.";
      case MSG_FAILED_TO_EXTRACT_CONTENT_FROM_COMPRESSED_FILE:
         return "Failed to extract content from compressed file";
      case MSG_FAILED_TO_GET_NICKNAME_FROM_CLIENT:
         return "Failed to get nickname from client.";
      case MSG_FAILED_TO_LOAD:
         return "Bị lỗi khi tải";
      case MSG_FAILED_TO_LOAD_CONTENT:
         return "Failed to tải content";
      case MSG_FAILED_TO_LOAD_MOVIE_FILE:
         return "Failed to tải movie file";
      case MSG_FAILED_TO_LOAD_OVERLAY:
         return "Failed to tải overlay.";
      case MSG_FAILED_TO_LOAD_STATE:
         return "Failed to tải state from";
      case MSG_FAILED_TO_OPEN_LIBRETRO_CORE:
         return "Failed to open libretro core";
      case MSG_FAILED_TO_PATCH:
         return "Failed to patch";
      case MSG_FAILED_TO_RECEIVE_HEADER_FROM_CLIENT:
         return "Failed to receive header from client.";
      case MSG_FAILED_TO_RECEIVE_NICKNAME:
         return "Failed to receive nickname.";
      case MSG_FAILED_TO_RECEIVE_NICKNAME_FROM_HOST:
         return "Failed to receive nickname from host.";
      case MSG_FAILED_TO_RECEIVE_NICKNAME_SIZE_FROM_HOST:
         return "Failed to receive nickname size from host.";
      case MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST:
         return "Failed to receive SRAM data from host.";
      case MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY:
         return "Failed to remove disk from tray.";
      case MSG_FAILED_TO_REMOVE_TEMPORARY_FILE:
         return "Failed to remove temporary file";
      case MSG_FAILED_TO_SAVE_SRAM:
         return "Failed to save SRAM";
      case MSG_FAILED_TO_SAVE_STATE_TO:
         return "Failed to save state to";
      case MSG_FAILED_TO_SEND_NICKNAME:
         return "Failed to send nickname.";
      case MSG_FAILED_TO_SEND_NICKNAME_SIZE:
         return "Failed to send nickname size.";
      case MSG_FAILED_TO_SEND_NICKNAME_TO_CLIENT:
         return "Failed to send nickname to client.";
      case MSG_FAILED_TO_SEND_NICKNAME_TO_HOST:
         return "Failed to send nickname to host.";
      case MSG_FAILED_TO_SEND_SRAM_DATA_TO_CLIENT:
         return "Failed to send SRAM data to client.";
      case MSG_FAILED_TO_START_AUDIO_DRIVER:
         return "Bị lỗi khi chạy chương trình điều khiển âm thanh. Sẽ tiếp tục chạy và bỏ âm thanh.";
      case MSG_FAILED_TO_START_MOVIE_RECORD:
         return "Failed to start movie record.";
      case MSG_FAILED_TO_START_RECORDING:
         return "Failed to start recording.";
      case MSG_FAILED_TO_TAKE_SCREENSHOT:
         return "Bị lỗi khi chụp ảnh màn hình.";
      case MSG_FAILED_TO_UNDO_LOAD_STATE:
         return "Failed to undo tải state.";
      case MSG_FAILED_TO_UNDO_SAVE_STATE:
         return "Failed to undo save state.";
      case MSG_FAILED_TO_UNMUTE_AUDIO:
         return "Failed to unmute Âm thanh.";
      case MSG_FATAL_ERROR_RECEIVED_IN:
         return "Fatal error received in";
      case MSG_FILE_NOT_FOUND:
         return "Không tìm thấy tệp";
      case MSG_FOUND_AUTO_SAVESTATE_IN:
         return "Tìm thấy savestate tự đông trong";
      case MSG_FOUND_DISK_LABEL:
         return "Found disk label";
      case MSG_FOUND_FIRST_DATA_TRACK_ON_FILE:
         return "Found first data track on file";
      case MSG_FOUND_LAST_STATE_SLOT:
         return "Tiềm thấy state slot xài lần trước";
      case MSG_FOUND_SHADER:
         return "Found shader";
      case MSG_FRAMES:
         return "Frames";
      case MSG_GAME_SPECIFIC_CORE_OPTIONS_FOUND_AT:
         return "Per-Game Options: game-specific core options found at";
      case MSG_GOT_INVALID_DISK_INDEX:
         return "Got invalid disk index.";
      case MSG_GRAB_MOUSE_STATE:
         return "Grab mouse state";
      case MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING:
         return "Libretro core is hardware rendered. Must use post-shaded recording as well.";
      case MSG_INFLATED_CHECKSUM_DID_NOT_MATCH_CRC32:
         return "Inflated checksum did not match CRC32.";
      case MSG_INPUT_CHEAT:
         return "Input Cheat";
      case MSG_INPUT_CHEAT_FILENAME:
         return "Cheat Filename";
      case MSG_INPUT_PRESET_FILENAME:
         return "Preset Filename";
      case MSG_INTERFACE:
         return "Interface";
      case MSG_INTERNAL_STORAGE:
         return "Internal Storage";
      case MSG_REMOVABLE_STORAGE:
         return "Removable Storage";
      case MSG_INVALID_NICKNAME_SIZE:
         return "Invalid nickname size.";
      case MSG_IN_BYTES:
         return "in bytes";
      case MSG_IN_GIGABYTES:
         return "in gigabytes";
      case MSG_IN_MEGABYTES:
         return "in megabytes";
      case MSG_LIBRETRO_ABI_BREAK:
         return "is compiled against a different version of libretro than this libretro implementation.";
      case MSG_LIBRETRO_FRONTEND:
         return "Frontend for libretro";
      case MSG_LOADED_STATE_FROM_SLOT:
         return "Loaded state from slot #%d.";
      case MSG_LOADED_STATE_FROM_SLOT_AUTO:
         return "Loaded state from slot -1 (auto).";
      case MSG_LOADING:
         return "Đang tải";
      case MSG_LOADING_CONTENT_FILE:
         return "Loading content file";
      case MSG_LOADING_HISTORY_FILE:
         return "Đang nạp tập tin lịch sử";
      case MSG_LOADING_STATE:
         return "Đang tải state";
      case MSG_MEMORY:
         return "Memory";
      case MSG_MOVIE_FILE_IS_NOT_A_VALID_BSV1_FILE:
         return "Movie file is not a valid BSV1 file.";
      case MSG_MOVIE_FORMAT_DIFFERENT_SERIALIZER_VERSION:
         return "Movie format seems to have a different serializer version. Will most likely fail.";
      case MSG_MOVIE_PLAYBACK_ENDED:
         return "Movie playback ended.";
      case MSG_MOVIE_RECORD_STOPPED:
         return "Stopping movie record.";
      case MSG_NETPLAY_FAILED:
         return "Failed to initialize netplay.";
      case MSG_NO_CONTENT_STARTING_DUMMY_CORE:
         return "No content, starting dummy core.";
      case MSG_NO_SAVE_STATE_HAS_BEEN_OVERWRITTEN_YET:
         return "No save state has been overwritten yet.";
      case MSG_NO_STATE_HAS_BEEN_LOADED_YET:
         return "No state has been loaded yet.";
      case MSG_OVERRIDES_ERROR_SAVING:
         return "Error saving overrides.";
      case MSG_OVERRIDES_SAVED_SUCCESSFULLY:
         return "Overrides saved successfully.";
      case MSG_PAUSED:
         return "Paused.";
      case MSG_PROGRAM:
         return "RetroArch";
      case MSG_READING_FIRST_DATA_TRACK:
         return "Reading first data track...";
      case MSG_RECEIVED:
         return "received";
      case MSG_RECORDING_TERMINATED_DUE_TO_RESIZE:
         return "Recording terminated due to resize.";
      case MSG_RECORDING_TO:
         return "Recording to";
      case MSG_REDIRECTING_CHEATFILE_TO:
         return "Redirecting cheat file to";
      case MSG_REDIRECTING_SAVEFILE_TO:
         return "Redirecting save file to";
      case MSG_REDIRECTING_SAVESTATE_TO:
         return "Redirecting savestate to";
      case MSG_REMAP_FILE_SAVED_SUCCESSFULLY:
         return "Remap file saved successfully.";
      case MSG_REMOVED_DISK_FROM_TRAY:
         return "Removed disk from tray.";
      case MSG_REMOVING_TEMPORARY_CONTENT_FILE:
         return "Removing temporary content file";
      case MSG_RESET:
         return "Reset";
      case MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT:
         return "Restarting recording due to driver reinit.";
      case MSG_RESTORED_OLD_SAVE_STATE:
         return "Restored old save state.";
      case MSG_RESTORING_DEFAULT_SHADER_PRESET_TO:
         return "Shaders: restoring default shader preset to";
      case MSG_REVERTING_SAVEFILE_DIRECTORY_TO:
         return "Reverting savefile directory to";
      case MSG_REVERTING_SAVESTATE_DIRECTORY_TO:
         return "Reverting savestate directory to";
      case MSG_REWINDING:
         return "Rewinding.";
      case MSG_REWIND_INIT:
         return "Initializing rewind buffer with size";
      case MSG_REWIND_INIT_FAILED:
         return "Failed to initialize rewind buffer. Rewinding will be disabled.";
      case MSG_REWIND_INIT_FAILED_THREADED_AUDIO:
         return "Implementation uses threaded Âm thanh. Cannot use rewind.";
      case MSG_REWIND_REACHED_END:
         return "Reached end of rewind buffer.";
      case MSG_SAVED_NEW_CONFIG_TO:
         return "Saved new config to";
      case MSG_SAVED_STATE_TO_SLOT:
         return "Saved state to slot #%d.";
      case MSG_SAVED_STATE_TO_SLOT_AUTO:
         return "Saved state to slot #-1 (auto).";
      case MSG_SAVED_SUCCESSFULLY_TO:
         return "Saved successfully to";
      case MSG_SAVING_RAM_TYPE:
         return "Saving RAM type";
      case MSG_SAVING_STATE:
         return "Saving state";
      case MSG_SCANNING:
         return "Scanning";
      case MSG_SCANNING_OF_DIRECTORY_FINISHED:
         return "Scanning of directory finished";
      case MSG_SENDING_COMMAND:
         return "Sending command";
      case MSG_SEVERAL_PATCHES_ARE_EXPLICITLY_DEFINED:
         return "Several patches are explicitly defined, ignoring all...";
      case MSG_SHADER_PRESET_SAVED_SUCCESSFULLY:
         return "Shader preset saved successfully.";
      case MSG_SKIPPING_SRAM_LOAD:
         return "Skipping SRAM tải.";
      case MSG_SLOW_MOTION:
         return "Slow motion.";
      case MSG_SLOW_MOTION_REWIND:
         return "Slow motion rewind.";
      case MSG_SRAM_WILL_NOT_BE_SAVED:
         return "SRAM will not be saved.";
      case MSG_STARTING_MOVIE_PLAYBACK:
         return "Starting movie playback.";
      case MSG_STARTING_MOVIE_RECORD_TO:
         return "Starting movie record to";
      case MSG_STATE_SIZE:
         return "State size";
      case MSG_STATE_SLOT:
         return "State slot";
      case MSG_TAKING_SCREENSHOT:
         return "Taking screenshot.";
      case MSG_TO:
         return "to";
      case MSG_UNDID_LOAD_STATE:
         return "Undid Tải state.";
      case MSG_UNDOING_SAVE_STATE:
         return "Undoing save state";
      case MSG_UNKNOWN:
         return "Unknown";
      case MSG_UNPAUSED:
         return "Unpaused.";
      case MSG_UNRECOGNIZED_COMMAND:
         return "Unrecognized command";
      case MSG_USING_CORE_NAME_FOR_NEW_CONFIG:
         return "Using core name for new config.";
      case MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED:
         return "Using libretro dummy core. Skipping recording.";
      case MSG_VALUE_CONNECT_DEVICE_FROM_A_VALID_PORT:
         return "Kết nối thiết bị từ cổng hợp lệ.";
      case MSG_VALUE_DISCONNECTING_DEVICE_FROM_PORT:
         return "Đang ngắt kết nối thiết bị từ cổng";
      case MSG_VALUE_REBOOTING:
         return "Đang khởi động lại...";
      case MSG_VALUE_SHUTTING_DOWN:
         return "Đang tắt máy...";
      case MSG_VERSION_OF_LIBRETRO_API:
         return "Phiên bản của libretro API";
      case MSG_VIEWPORT_SIZE_CALCULATION_FAILED:
         return "Viewport size calculation failed! Will continue using raw data. This will probably not work right ...";
      case MSG_VIRTUAL_DISK_TRAY:
         return "virtual disk tray.";
      default:
#if 0
         RARCH_LOG("Unimplemented: [%d]\n", msg);
#endif
         break;
   }

   return "null";
}
