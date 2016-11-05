/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2016 - Brad Parker
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

int menu_hash_get_help_us_enum(enum msg_hash_enums msg, char *s, size_t len)
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
                  "Toggles between fast-forwarding and \n"
                  "normal speed."
                  );
            break;
         case RARCH_FAST_FORWARD_HOLD_KEY:
            snprintf(s, len,
                  "Hold for fast-forward. \n"
                  " \n"
                  "Releasing button disables fast-forward."
                  );
            break;
         case RARCH_PAUSE_TOGGLE:
            snprintf(s, len,
                  "Toggle between paused and non-paused state.");
            break;
         case RARCH_FRAMEADVANCE:
            snprintf(s, len,
                  "Frame advance when content is paused.");
            break;
         case RARCH_SHADER_NEXT:
            snprintf(s, len,
                  "Applies next shader in directory.");
            break;
         case RARCH_SHADER_PREV:
            snprintf(s, len,
                  "Applies previous shader in directory.");
            break;
         case RARCH_CHEAT_INDEX_PLUS:
         case RARCH_CHEAT_INDEX_MINUS:
         case RARCH_CHEAT_TOGGLE:
            snprintf(s, len,
                  "Cheats.");
            break;
         case RARCH_RESET:
            snprintf(s, len,
                  "Reset the content.");
            break;
         case RARCH_SCREENSHOT:
            snprintf(s, len,
                  "Take screenshot.");
            break;
         case RARCH_MUTE:
            snprintf(s, len,
                  "Mute/unmute audio.");
            break;
         case RARCH_OSK:
            snprintf(s, len,
                  "Toggles onscreen keyboard.");
            break;
         case RARCH_NETPLAY_FLIP:
            snprintf(s, len,
                  "Netplay flip users.");
            break;
         case RARCH_SLOWMOTION:
            snprintf(s, len,
                  "Hold for slowmotion.");
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
                  "Increases audio volume.");
            break;
         case RARCH_VOLUME_DOWN:
            snprintf(s, len,
                  "Decreases audio volume.");
            break;
         case RARCH_OVERLAY_NEXT:
            snprintf(s, len,
                  "Switches to next overlay. Wraps around.");
            break;
         case RARCH_DISK_EJECT_TOGGLE:
            snprintf(s, len,
                  "Toggles eject for disks. \n"
                  " \n"
                  "Used for multiple-disk content. ");
            break;
         case RARCH_DISK_NEXT:
         case RARCH_DISK_PREV:
            snprintf(s, len,
                  "Cycles through disk images. Use after ejecting. \n"
                  " \n"
                  "Complete by toggling eject again.");
            break;
         case RARCH_GRAB_MOUSE_TOGGLE:
            snprintf(s, len,
                  "Toggles mouse grab. \n"
                  " \n"
                  "When mouse is grabbed, RetroArch hides the \n"
                  "mouse, and keeps the mouse pointer inside \n"
                  "the window to allow relative mouse input to \n"
                  "work better.");
            break;
         case RARCH_MENU_TOGGLE:
            snprintf(s, len, "Toggles menu.");
            break;
         case RARCH_LOAD_STATE_KEY:
            snprintf(s, len,
                  "Loads state.");
            break;
         case RARCH_FULLSCREEN_TOGGLE_KEY:
            snprintf(s, len,
                  "Toggles fullscreen.");
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
         case RARCH_SAVE_STATE_KEY:
            snprintf(s, len,
                  "Saves state.");
            break;
         case RARCH_REWIND:
            snprintf(s, len,
                  "Hold button down to rewind. \n"
                  " \n"
                  "Rewinding must be enabled.");
            break;
         case RARCH_MOVIE_RECORD_TOGGLE:
            snprintf(s, len,
                  "Toggle between recording and not.");
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
      case MENU_ENUM_LABEL_CHEEVOS_USERNAME:
         snprintf(s, len, "Username for your Retro Achievements account.");
         break;
      case MENU_ENUM_LABEL_CHEEVOS_PASSWORD:
         snprintf(s, len, "Password for your Retro Achievements account.");
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
         snprintf(s, len, "Automatically load content-specific core options.");
         break;
      case MENU_ENUM_LABEL_AUTO_OVERRIDES_ENABLE:
         snprintf(s, len, "Automatically load override configurations.");
         break;
      case MENU_ENUM_LABEL_AUTO_REMAPS_ENABLE:
         snprintf(s, len, "Automatically load input remapping files.");
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
               "Screenshot Directory.");
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
               "Select this to scan the current directory \n"
               "for content.");
         break;
      case MENU_ENUM_LABEL_USE_THIS_DIRECTORY:
         snprintf(s, len,
               "Select this to set this as the directory.");
         break;
      case MENU_ENUM_LABEL_CONTENT_DATABASE_DIRECTORY:
         snprintf(s, len,
               "Content Database Directory. \n"
               " \n"
               "Path to content database \n"
               "directory.");
         break;
      case MENU_ENUM_LABEL_THUMBNAILS_DIRECTORY:
         snprintf(s, len,
               "Thumbnails Directory. \n"
               " \n"
               "To store thumbnail files.");
         break;
      case MENU_ENUM_LABEL_LIBRETRO_INFO_PATH:
         snprintf(s, len,
               "Core Info Directory. \n"
               " \n"
               "A directory for where to search \n"
               "for libretro core information.");
         break;
      case MENU_ENUM_LABEL_PLAYLIST_DIRECTORY:
         snprintf(s, len,
               "Playlist Directory. \n"
               " \n"
               "Save all playlist files to this \n"
               "directory.");
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
      case MENU_ENUM_LABEL_PARENT_DIRECTORY:
         snprintf(s, len,
               "Go back to the parent directory.");
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
      case MENU_ENUM_LABEL_HISTORY_LIST_ENABLE:
         snprintf(s, len,
               "If enabled, every content loaded \n"
               "in RetroArch will be automatically \n"
               "added to the recent history list.");
         break;
      case MENU_ENUM_LABEL_RGUI_BROWSER_DIRECTORY:
         snprintf(s, len,
               "File Browser Directory. \n"
               " \n"
               "Sets start directory for menu file browser.");
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
      case MENU_ENUM_LABEL_KEYBOARD_OVERLAY_PRESET:
         snprintf(s, len,
               "Path to onscreen keyboard overlay.");
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
               "Load Shader Preset. \n"
               " \n"
               " Load a "
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
               "Set Shader Directory to set where \n"
               "the browser starts to look for \n"
               "shaders."
               );
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
               "saved to Shader Directory."
               );
         break;
      case MENU_ENUM_LABEL_MENU_TOGGLE:
         snprintf(s, len,
               "Toggles menu.");
         break;
      case MENU_ENUM_LABEL_GRAB_MOUSE_TOGGLE:
         snprintf(s, len,
               "Toggles mouse grab.\n"
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
               "Toggles eject for disks.\n"
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
               "Core Directory. \n"
               " \n"
               "A directory for where to search for \n"
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
               "Toggles to next overlay.\n"
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
               "Increases audio volume.");
         break;
      case MENU_ENUM_LABEL_VOLUME_DOWN:
         snprintf(s, len,
               "Decreases audio volume.");
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
               "System Directory. \n"
               " \n"
               "Sets the 'system' directory.\n"
               "Cores can query for this\n"
               "directory to load BIOSes, \n"
               "system-specific configs, etc.");
         break;
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE:
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_LOAD:
         snprintf(s, len,
               "Automatically saves a savestate at the \n"
               "end of RetroArch's lifetime.\n"
               " \n"
               "RetroArch will automatically load any savestate\n"
               "with this path on startup if 'Auto Load State\n"
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
               "Take screenshot.");
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
               "Load up a specific config file \n"
               "based on the core being used.\n");
         break;
      case MENU_ENUM_LABEL_VIDEO_FULLSCREEN:
         snprintf(s, len, "Toggles fullscreen.");
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
               "Screenshot Directory. \n"
               " \n"
               "Directory to dump screenshots to."
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
               "Savefile Directory. \n"
               " \n"
               "Save all save files (*.srm) to this \n"
               "directory. This includes related files like \n"
               ".bsv, .rt, .psrm, etc...\n"
               " \n"
               "This will be overridden by explicit command line\n"
               "options.");
         break;
      case MENU_ENUM_LABEL_SAVESTATE_DIRECTORY:
         snprintf(s, len,
               "Savestate Directory. \n"
               " \n"
               "Save all save states (*.state) to this \n"
               "directory.\n"
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
         snprintf(s, len, "Path to an image to set as menu wallpaper.");
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
      case MENU_ENUM_LABEL_DYNAMIC_WALLPAPER:
         snprintf(s, len,
               "Dynamically load a new wallpaper \n"
               "depending on context.");
         break;
      case MENU_ENUM_LABEL_CORE_UPDATER_BUILDBOT_URL:
         snprintf(s, len,
               "URL to core updater directory on the \n"
               "Libretro buildbot.");
         break;
      case MENU_ENUM_LABEL_BUILDBOT_ASSETS_URL:
         snprintf(s, len,
               "URL to assets updater directory on the \n"
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
               "Overlay Directory. \n"
               " \n"
               "Defines a directory where overlays are \n"
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
               "Mute/unmute audio.");
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
               "Loads state.");
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
               "Applies previous shader in directory.");
         break;
      case MENU_ENUM_LABEL_SHADER_NEXT:
         snprintf(s, len,
               "Applies next shader in directory.");
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
      default:
         if (string_is_empty(s))
            strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
         return -1;
   }

   return 0;
}

#ifdef HAVE_MENU
static const char *menu_hash_to_str_us_label_enum(enum msg_hash_enums msg)
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
      case MENU_ENUM_LABEL_ACCOUNTS_CHEEVOS_USERNAME:
         return "accounts_cheevos_username";
      case MENU_ENUM_LABEL_ACCOUNTS_LIST:
         return "accounts_list";
      case MENU_ENUM_LABEL_ACCOUNTS_RETRO_ACHIEVEMENTS:
         return "retro_achievements";
      case MENU_ENUM_LABEL_ACHIEVEMENT_LIST:
         return "achievement_list";
      case MENU_ENUM_LABEL_ADD_CONTENT_LIST:
         return "add_content";
      case MENU_ENUM_LABEL_CONFIGURATIONS_LIST:
         return "configurations_list";
      case MENU_ENUM_LABEL_ADD_TAB:
         return "add_tab";
      case MENU_ENUM_LABEL_ARCHIVE_MODE:
         return "archive_mode";
      case MENU_ENUM_LABEL_ASSETS_DIRECTORY:
         return "assets_directory";
      case MENU_ENUM_LABEL_AUDIO_BLOCK_FRAMES:
         return "audio_block_frames";
      case MENU_ENUM_LABEL_AUDIO_DEVICE:
         return "audio_device";
      case MENU_ENUM_LABEL_AUDIO_DRIVER:
         return "audio_driver";
      case MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN:
         return "audio_dsp_plugin";
      case MENU_ENUM_LABEL_AUDIO_ENABLE:
         return "audio_enable";
      case MENU_ENUM_LABEL_AUDIO_FILTER_DIR:
         return "audio_filter_dir";
      case MENU_ENUM_LABEL_AUDIO_LATENCY:
         return "audio_latency";
      case MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW:
         return "audio_max_timing_skew";
      case MENU_ENUM_LABEL_AUDIO_MUTE:
         return "audio_mute_enable";
      case MENU_ENUM_LABEL_AUDIO_OUTPUT_RATE:
         return "audio_output_rate";
      case MENU_ENUM_LABEL_AUDIO_RATE_CONTROL_DELTA:
         return "audio_rate_control_delta";
      case MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER:
         return "audio_resampler_driver";
      case MENU_ENUM_LABEL_AUDIO_SETTINGS:
         return "audio_settings";
      case MENU_ENUM_LABEL_AUDIO_SYNC:
         return "audio_sync";
      case MENU_ENUM_LABEL_AUDIO_VOLUME:
         return "audio_volume";
      case MENU_ENUM_LABEL_AUTOSAVE_INTERVAL:
         return "autosave_interval";
      case MENU_ENUM_LABEL_AUTO_OVERRIDES_ENABLE:
         return "auto_overrides_enable";
      case MENU_ENUM_LABEL_AUTO_REMAPS_ENABLE:
         return "auto_remaps_enable";
      case MENU_ENUM_LABEL_AUTO_SHADERS_ENABLE:
         return "auto_shaders_enable";
      case MENU_ENUM_LABEL_BLOCK_SRAM_OVERWRITE:
         return "block_sram_overwrite";
      case MENU_ENUM_LABEL_BLUETOOTH_ENABLE:
         return "bluetooth_enable";
      case MENU_ENUM_LABEL_BUILDBOT_ASSETS_URL:
         return "buildbot_assets_url";
      case MENU_ENUM_LABEL_CACHE_DIRECTORY:
         return "cache_directory";
      case MENU_ENUM_LABEL_CAMERA_ALLOW:
         return "camera_allow";
      case MENU_ENUM_LABEL_CAMERA_DRIVER:
         return "camera_driver";
      case MENU_ENUM_LABEL_CB_CORE_CONTENT_DIRS_LIST:
         return "cb_core_content_dirs_list";
      case MENU_ENUM_LABEL_CB_CORE_CONTENT_DOWNLOAD:
         return "cb_core_content_download";
      case MENU_ENUM_LABEL_CB_CORE_CONTENT_LIST:
         return "cb_core_content_list";
      case MENU_ENUM_LABEL_CB_CORE_THUMBNAILS_DOWNLOAD:
         return "cb_core_thumbnails_download";
      case MENU_ENUM_LABEL_CB_CORE_UPDATER_DOWNLOAD:
         return "cb_core_updater_download";
      case MENU_ENUM_LABEL_CB_CORE_UPDATER_LIST:
         return "cb_core_updater_list";
      case MENU_ENUM_LABEL_CB_DOWNLOAD_URL:
         return "cb_download_url";
      case MENU_ENUM_LABEL_CB_LAKKA_DOWNLOAD:
         return "cb_lakka_download";
      case MENU_ENUM_LABEL_CB_LAKKA_LIST:
         return "cb_lakka_list";
      case MENU_ENUM_LABEL_CB_MENU_THUMBNAIL:
         return "cb_menu_thumbnail";
      case MENU_ENUM_LABEL_CB_MENU_WALLPAPER:
         return "cb_menu_wallpaper";
      case MENU_ENUM_LABEL_CB_THUMBNAILS_UPDATER_DOWNLOAD:
         return "cb_thumbnails_updater_download";
      case MENU_ENUM_LABEL_CB_THUMBNAILS_UPDATER_LIST:
         return "cb_thumbnails_updater_list";
      case MENU_ENUM_LABEL_CB_UPDATE_ASSETS:
         return "cb_update_assets";
      case MENU_ENUM_LABEL_CB_UPDATE_AUTOCONFIG_PROFILES:
         return "cb_update_autoconfig_profiles";
      case MENU_ENUM_LABEL_CB_UPDATE_CHEATS:
         return "cb_update_cheats";
      case MENU_ENUM_LABEL_CB_UPDATE_CORE_INFO_FILES:
         return "cb_update_core_info_files";
      case MENU_ENUM_LABEL_CB_UPDATE_DATABASES:
         return "cb_update_databases";
      case MENU_ENUM_LABEL_CB_UPDATE_OVERLAYS:
         return "cb_update_overlays";
      case MENU_ENUM_LABEL_CB_UPDATE_SHADERS_CG:
         return "cb_update_shaders_cg";
      case MENU_ENUM_LABEL_CB_UPDATE_SHADERS_GLSL:
         return "cb_update_shaders_glsl";
      case MENU_ENUM_LABEL_CHEAT_APPLY_CHANGES:
         return "cheat_apply_changes";
      case MENU_ENUM_LABEL_CHEAT_DATABASE_PATH:
         return "cheat_database_path";
      case MENU_ENUM_LABEL_CHEAT_FILE_LOAD:
         return "cheat_file_load";
      case MENU_ENUM_LABEL_CHEAT_FILE_SAVE_AS:
         return "cheat_file_save_as";
      case MENU_ENUM_LABEL_CHEAT_NUM_PASSES:
         return "cheat_num_passes";
      case MENU_ENUM_LABEL_CHEEVOS_DESCRIPTION:
         return "cheevos_description";
      case MENU_ENUM_LABEL_CHEEVOS_ENABLE:
         return "cheevos_enable";
      case MENU_ENUM_LABEL_CHEEVOS_HARDCORE_MODE_ENABLE:
         return "cheevos_hardcore_mode_enable";
      case MENU_ENUM_LABEL_CHEEVOS_LOCKED_ACHIEVEMENTS:
         return "cheevos_locked_achievements";
      case MENU_ENUM_LABEL_CHEEVOS_LOCKED_ENTRY:
         return "cheevos_locked_entry";
      case MENU_ENUM_LABEL_CHEEVOS_PASSWORD:
         return "cheevos_password";
      case MENU_ENUM_LABEL_CHEEVOS_TEST_UNOFFICIAL:
         return "cheevos_test_unofficial";
      case MENU_ENUM_LABEL_CHEEVOS_UNLOCKED_ACHIEVEMENTS:
         return "cheevos_unlocked_achievements";
      case MENU_ENUM_LABEL_CHEEVOS_UNLOCKED_ENTRY:
         return "cheevos_unlocked_entry";
      case MENU_ENUM_LABEL_CHEEVOS_USERNAME:
         return "cheevos_username";
      case MENU_ENUM_LABEL_CLOSE_CONTENT:
         return "unload_core";
      case MENU_ENUM_LABEL_COLLECTION:
         return "collection";
      case MENU_ENUM_LABEL_CONFIGURATIONS:
         return "configurations";
      case MENU_ENUM_LABEL_CONFIGURATION_SETTINGS:
         return "configuration_settings";
      case MENU_ENUM_LABEL_CONFIG_SAVE_ON_EXIT:
         return "config_save_on_exit";
      case MENU_ENUM_LABEL_CONNECT_WIFI:
         return "connect_wifi";
      case MENU_ENUM_LABEL_CONTENT_ACTIONS:
         return "content_actions";
      case MENU_ENUM_LABEL_CONTENT_COLLECTION_LIST:
         return "select_from_collection";
      case MENU_ENUM_LABEL_CONTENT_DATABASE_DIRECTORY:
         return "content_database_path";
      case MENU_ENUM_LABEL_CONTENT_HISTORY_SIZE:
         return "content_history_size";
      case MENU_ENUM_LABEL_CONTENT_SETTINGS:
         return "quick_menu";
      case MENU_ENUM_LABEL_CORE_ASSETS_DIRECTORY:
         return "core_assets_directory";
      case MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS:
         return "core_cheat_options";
      case MENU_ENUM_LABEL_CORE_COUNTERS:
         return "core_counters";
      case MENU_ENUM_LABEL_CORE_ENABLE:
         return "menu_core_enable";
      case MENU_ENUM_LABEL_CORE_INFORMATION:
         return "core_information";
      case MENU_ENUM_LABEL_CORE_INFO_ENTRY:
         return "core_info_entry";
      case MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
         return "core_input_remapping_options";
      case MENU_ENUM_LABEL_CORE_LIST:
         return "load_core";
      case MENU_ENUM_LABEL_CORE_OPTIONS:
         return "core_options";
      case MENU_ENUM_LABEL_CORE_OPTION_ENTRY:
         return "core_option_entry";
      case MENU_ENUM_LABEL_CORE_SETTINGS:
         return "core_settings";
      case MENU_ENUM_LABEL_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE:
         return "core_set_supports_no_content_enable";
      case MENU_ENUM_LABEL_CORE_SPECIFIC_CONFIG:
         return "core_specific_config";
      case MENU_ENUM_LABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE:
         return "core_updater_auto_extract_archive";
      case MENU_ENUM_LABEL_CORE_UPDATER_BUILDBOT_URL:
         return "core_updater_buildbot_url";
      case MENU_ENUM_LABEL_CORE_UPDATER_LIST:
         return "core_updater_list";
      case MENU_ENUM_LABEL_CPU_ARCHITECTURE:
         return "system_information_cpu_architecture";
      case MENU_ENUM_LABEL_CPU_CORES:
         return "system_information_cpu_cores";
      case MENU_ENUM_LABEL_CURSOR_DIRECTORY:
         return "cursor_directory";
      case MENU_ENUM_LABEL_CURSOR_MANAGER_LIST:
         return "cursor_manager_list";
      case MENU_ENUM_LABEL_CUSTOM_BIND:
         return "custom_bind";
      case MENU_ENUM_LABEL_CUSTOM_BIND_ALL:
         return "custom_bind_all";
      case MENU_ENUM_LABEL_CUSTOM_RATIO:
         return "custom_ratio";
      case MENU_ENUM_LABEL_DATABASE_MANAGER_LIST:
         return "database_manager_list";
      case MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_CHEEVOS_LIST:
         return "deferred_accounts_cheevos_list";
      case MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_LIST:
         return "deferred_accounts_list";
      case MENU_ENUM_LABEL_DEFERRED_ARCHIVE_ACTION:
         return "deferred_archive_action";
      case MENU_ENUM_LABEL_DEFERRED_ARCHIVE_ACTION_DETECT_CORE:
         return "deferred_archive_action_detect_core";
      case MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN:
         return "deferred_archive_open";
      case MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN_DETECT_CORE:
         return "deferred_archive_open_detect_core";
      case MENU_ENUM_LABEL_DEFERRED_AUDIO_SETTINGS_LIST:
         return "deferred_audio_settings_list";
      case MENU_ENUM_LABEL_DEFERRED_CONFIGURATION_SETTINGS_LIST:
         return "deferred_configuration_settings_list";
      case MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_DIRS_LIST:
         return "deferred_core_content_dirs_list";
      case MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_DIRS_SUBDIR_LIST:
         return "deferred_core_content_dirs_subdir_list";
      case MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_LIST:
         return "deferred_core_content_list";
      case MENU_ENUM_LABEL_DEFERRED_CORE_LIST:
         return "deferred_core_list";
      case MENU_ENUM_LABEL_DEFERRED_CORE_LIST_SET:
         return "deferred_core_list_set";
      case MENU_ENUM_LABEL_DEFERRED_CORE_SETTINGS_LIST:
         return "deferred_core_settings_list";
      case MENU_ENUM_LABEL_DEFERRED_CORE_UPDATER_LIST:
         return "core_updater";
      case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST:
         return "deferred_cursor_manager_list";
      case MENU_ENUM_LABEL_DEFERRED_DATABASE_MANAGER_LIST:
         return "deferred_database_manager_list";
      case MENU_ENUM_LABEL_DEFERRED_DIRECTORY_SETTINGS_LIST:
         return "deferred_directory_settings_list";
      case MENU_ENUM_LABEL_DEFERRED_DRIVER_SETTINGS_LIST:
         return "deferred_driver_settings_list";
      case MENU_ENUM_LABEL_DEFERRED_FRAME_THROTTLE_SETTINGS_LIST:
         return "deferred_frame_throttle_settings_list";
      case MENU_ENUM_LABEL_DEFERRED_INPUT_HOTKEY_BINDS_LIST:
         return "deferred_input_hotkey_binds";
      case MENU_ENUM_LABEL_DEFERRED_INPUT_SETTINGS_LIST:
         return "deferred_input_settings_list";
      case MENU_ENUM_LABEL_DEFERRED_LAKKA_LIST:
         return "deferred_lakka_list";
      case MENU_ENUM_LABEL_DEFERRED_LAKKA_SERVICES_LIST:
         return "deferred_lakka_services_list";
      case MENU_ENUM_LABEL_DEFERRED_LOGGING_SETTINGS_LIST:
         return "deferred_logging_settings_list";
      case MENU_ENUM_LABEL_DEFERRED_MENU_FILE_BROWSER_SETTINGS_LIST:
         return "deferred_menu_file_browser_settings_list";
      case MENU_ENUM_LABEL_DEFERRED_MENU_SETTINGS_LIST:
         return "deferred_menu_settings_list";
      case MENU_ENUM_LABEL_DEFERRED_NETWORK_SETTINGS_LIST:
         return "deferred_network_settings_list";
      case MENU_ENUM_LABEL_DEFERRED_ONSCREEN_DISPLAY_SETTINGS_LIST:
         return "deferred_onscreen_display_settings_list";
      case MENU_ENUM_LABEL_DEFERRED_ONSCREEN_OVERLAY_SETTINGS_LIST:
         return "deferred_onscreen_overlay_settings_list";
      case MENU_ENUM_LABEL_DEFERRED_PLAYLIST_SETTINGS_LIST:
         return "deferred_playlist_settings";
      case MENU_ENUM_LABEL_DEFERRED_PRIVACY_SETTINGS_LIST:
         return "deferred_privacy_settings_list";
      case MENU_ENUM_LABEL_DEFERRED_RDB_ENTRY_DETAIL:
         return "deferred_rdb_entry_detail";
      case MENU_ENUM_LABEL_DEFERRED_RECORDING_SETTINGS_LIST:
         return "deferred_recording_settings";
      case MENU_ENUM_LABEL_DEFERRED_RETRO_ACHIEVEMENTS_SETTINGS_LIST:
         return "deferred_retro_achievements_settings_list";
      case MENU_ENUM_LABEL_DEFERRED_REWIND_SETTINGS_LIST:
         return "deferred_rewind_settings_list";
      case MENU_ENUM_LABEL_DEFERRED_SAVING_SETTINGS_LIST:
         return "deferred_saving_settings_list";
      case MENU_ENUM_LABEL_DEFERRED_THUMBNAILS_UPDATER_LIST:
         return "deferred_thumbnails_updater_list";
      case MENU_ENUM_LABEL_DEFERRED_UPDATER_SETTINGS_LIST:
         return "deferred_updater_settings_list";
      case MENU_ENUM_LABEL_DEFERRED_USER_BINDS_LIST:
         return "deferred_user_binds_list";
      case MENU_ENUM_LABEL_DEFERRED_USER_INTERFACE_SETTINGS_LIST:
         return "deferred_user_interface_settings_list";
      case MENU_ENUM_LABEL_DEFERRED_USER_SETTINGS_LIST:
         return "deferred_user_settings_list";
      case MENU_ENUM_LABEL_DEFERRED_VIDEO_FILTER:
         return "deferred_video_filter";
      case MENU_ENUM_LABEL_DEFERRED_VIDEO_SETTINGS_LIST:
         return "deferred_video_settings_list";
      case MENU_ENUM_LABEL_DEFERRED_WIFI_SETTINGS_LIST:
         return "deferred_wifi_settings_list";
      case MENU_ENUM_LABEL_DELETE_ENTRY:
         return "delete_entry";
      case MENU_ENUM_LABEL_DETECT_CORE_LIST:
         return "detect_core_list";
      case MENU_ENUM_LABEL_DIRECTORY_SETTINGS:
         return "directory_settings";
      case MENU_ENUM_LABEL_DISK_CYCLE_TRAY_STATUS:
         return "disk_cycle_tray_status";
      case MENU_ENUM_LABEL_DISK_IMAGE_APPEND:
         return "disk_image_append";
      case MENU_ENUM_LABEL_DISK_OPTIONS:
         return "core_disk_options";
      case MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST:
         return "downloaded_file_detect_core_list";
      case MENU_ENUM_LABEL_DOWNLOAD_CORE_CONTENT:
         return "download_core_content";
      case MENU_ENUM_LABEL_DOWNLOAD_CORE_CONTENT_DIRS:
         return "download_core_content_dirs";
      case MENU_ENUM_LABEL_DPI_OVERRIDE_ENABLE:
         return "dpi_override_enable";
      case MENU_ENUM_LABEL_DPI_OVERRIDE_VALUE:
         return "dpi_override_value";
      case MENU_ENUM_LABEL_DRIVER_SETTINGS:
         return "driver_settings";
      case MENU_ENUM_LABEL_DUMMY_ON_CORE_SHUTDOWN:
         return "dummy_on_core_shutdown";
      case MENU_ENUM_LABEL_DYNAMIC_WALLPAPER:
         return "menu_dynamic_wallpaper_enable";
      case MENU_ENUM_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY:
         return "dynamic_wallpapers_directory";
      case MENU_ENUM_LABEL_ENTRY_HOVER_COLOR:
         return "menu_entry_hover_color";
      case MENU_ENUM_LABEL_ENTRY_NORMAL_COLOR:
         return "menu_entry_normal_color";
      case MENU_ENUM_LABEL_FASTFORWARD_RATIO:
         return "fastforward_ratio";
      case MENU_ENUM_LABEL_FILE_BROWSER_CORE:
         return "file_browser_core";
      case MENU_ENUM_LABEL_FILE_BROWSER_CORE_DETECTED:
         return "file_browser_core_detected";
      case MENU_ENUM_LABEL_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION:
         return "file_browser_core_select_from_collection";
      case MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY:
         return "file_browser_directory";
      case MENU_ENUM_LABEL_FILE_BROWSER_IMAGE:
         return "file_browser_image";
      case MENU_ENUM_LABEL_FILE_BROWSER_IMAGE_OPEN_WITH_VIEWER:
         return "file_browser_image_open_with_viewer";
      case MENU_ENUM_LABEL_FILE_BROWSER_MOVIE_OPEN:
         return "file_browser_movie_open";
      case MENU_ENUM_LABEL_FILE_BROWSER_MUSIC_OPEN:
         return "file_browser_music_open";
      case MENU_ENUM_LABEL_FILE_BROWSER_PLAIN_FILE:
         return "file_browser_plain_file";
      case MENU_ENUM_LABEL_FILE_BROWSER_REMAP:
         return "file_browser_remap";
      case MENU_ENUM_LABEL_FILE_BROWSER_SHADER:
         return "file_browser_shader";
      case MENU_ENUM_LABEL_FILE_BROWSER_SHADER_PRESET:
         return "file_browser_shader_preset";
      case MENU_ENUM_LABEL_FPS_SHOW:
         return "fps_show";
      case MENU_ENUM_LABEL_FRAME_THROTTLE_ENABLE:
         return "fastforward_ratio_throttle_enable";
      case MENU_ENUM_LABEL_FRAME_THROTTLE_SETTINGS:
         return "frame_throttle_settings";
      case MENU_ENUM_LABEL_FRONTEND_COUNTERS:
         return "frontend_counters";
      case MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS:
         return "game_specific_options";
      case MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS_CREATE:
         return "game_specific_options_create";
      case MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS_IN_USE:
         return "game_specific_options_in_use";
      case MENU_ENUM_LABEL_HELP:
         return "help";
      case MENU_ENUM_LABEL_HELP_AUDIO_VIDEO_TROUBLESHOOTING:
         return "help_audio_video_troubleshooting";
      case MENU_ENUM_LABEL_HELP_CHANGE_VIRTUAL_GAMEPAD:
         return "help_change_virtual_gamepad";
      case MENU_ENUM_LABEL_HELP_CONTROLS:
         return "help_controls";
      case MENU_ENUM_LABEL_HELP_LIST:
         return "help_list";
      case MENU_ENUM_LABEL_HELP_LOADING_CONTENT:
         return "help_loading_content";
      case MENU_ENUM_LABEL_HELP_SCANNING_CONTENT:
         return "help_scanning_content";
      case MENU_ENUM_LABEL_HELP_WHAT_IS_A_CORE:
         return "help_what_is_a_core";
      case MENU_ENUM_LABEL_HISTORY_LIST_ENABLE:
         return "history_list_enable";
      case MENU_ENUM_LABEL_HISTORY_TAB:
         return "history_tab";
      case MENU_ENUM_LABEL_HORIZONTAL_MENU:
         return "horizontal_menu";
      case MENU_ENUM_LABEL_IMAGES_TAB:
         return "images_tab";
      case MENU_ENUM_LABEL_INFORMATION:
         return "information";
      case MENU_ENUM_LABEL_INFORMATION_LIST:
         return "information_list";
      case MENU_ENUM_LABEL_INFO_SCREEN:
         return "info_screen";
      case MENU_ENUM_LABEL_INPUT_ALL_USERS_CONTROL_MENU:
         return "all_users_control_menu";
      case MENU_ENUM_LABEL_INPUT_AUTODETECT_ENABLE:
         return "input_autodetect_enable";
      case MENU_ENUM_LABEL_INPUT_AXIS_THRESHOLD:
         return "input_axis_threshold";
      case MENU_ENUM_LABEL_INPUT_BACK_AS_MENU_ENUM_TOGGLE_ENABLE:
         return "back_as_menu_toggle_enable";
      case MENU_ENUM_LABEL_INPUT_BIND_MODE:
         return "input_bind_mode";
      case MENU_ENUM_LABEL_INPUT_BIND_TIMEOUT:
         return "input_bind_timeout";
      case MENU_ENUM_LABEL_INPUT_DESCRIPTOR_HIDE_UNBOUND:
         return "input_descriptor_hide_unbound";
      case MENU_ENUM_LABEL_INPUT_DESCRIPTOR_LABEL_SHOW:
         return "input_descriptor_label_show";
      case MENU_ENUM_LABEL_INPUT_DRIVER:
         return "input_driver";
      case MENU_ENUM_LABEL_INPUT_DUTY_CYCLE:
         return "input_duty_cycle";
      case MENU_ENUM_LABEL_INPUT_HOTKEY_BINDS:
         return "input_hotkey_binds";
      case MENU_ENUM_LABEL_INPUT_HOTKEY_BINDS_BEGIN:
         return "input_hotkey_binds_begin";
      case MENU_ENUM_LABEL_INPUT_ICADE_ENABLE:
         return "input_icade_enable";
      case MENU_ENUM_LABEL_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE:
         return "keyboard_gamepad_mapping_type";
      case MENU_ENUM_LABEL_INPUT_LIBRETRO_DEVICE:
         return "input_libretro_device_p%u";
      case MENU_ENUM_LABEL_INPUT_MAX_USERS:
         return "input_max_users";
      case MENU_ENUM_LABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO:
         return "input_menu_toggle_gamepad_combo";
      case MENU_ENUM_LABEL_INPUT_OSK_OVERLAY_ENABLE:
         return "input_osk_overlay_enable";
      case MENU_ENUM_LABEL_INPUT_OVERLAY_ENABLE:
         return "input_overlay_enable";
      case MENU_ENUM_LABEL_INPUT_OVERLAY_HIDE_IN_MENU:
         return "overlay_hide_in_menu";
      case MENU_ENUM_LABEL_INPUT_PLAYER_ANALOG_DPAD_MODE:
         return "input_player%u_analog_dpad_mode";
      case MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR:
         return "input_poll_type_behavior";
      case MENU_ENUM_LABEL_INPUT_PREFER_FRONT_TOUCH:
         return "input_prefer_front_touch";
      case MENU_ENUM_LABEL_INPUT_REMAPPING_DIRECTORY:
         return "input_remapping_directory";
      case MENU_ENUM_LABEL_INPUT_REMAP_BINDS_ENABLE:
         return "input_remap_binds_enable";
      case MENU_ENUM_LABEL_INPUT_SETTINGS:
         return "input_settings";
      case MENU_ENUM_LABEL_INPUT_SETTINGS_BEGIN:
         return "input_settings_begin";
      case MENU_ENUM_LABEL_INPUT_SMALL_KEYBOARD_ENABLE:
         return "input_small_keyboard_enable";
      case MENU_ENUM_LABEL_INPUT_TOUCH_ENABLE:
         return "input_touch_enable";
      case MENU_ENUM_LABEL_INPUT_TURBO_PERIOD:
         return "input_turbo_period";
      case MENU_ENUM_LABEL_INPUT_USER_10_BINDS:
         return "10_input_binds_list";
      case MENU_ENUM_LABEL_INPUT_USER_11_BINDS:
         return "11_input_binds_list";
      case MENU_ENUM_LABEL_INPUT_USER_12_BINDS:
         return "12_input_binds_list";
      case MENU_ENUM_LABEL_INPUT_USER_13_BINDS:
         return "13_input_binds_list";
      case MENU_ENUM_LABEL_INPUT_USER_14_BINDS:
         return "14_input_binds_list";
      case MENU_ENUM_LABEL_INPUT_USER_15_BINDS:
         return "15_input_binds_list";
      case MENU_ENUM_LABEL_INPUT_USER_16_BINDS:
         return "16_input_binds_list";
      case MENU_ENUM_LABEL_INPUT_USER_1_BINDS:
         return "1_input_binds_list";
      case MENU_ENUM_LABEL_INPUT_USER_2_BINDS:
         return "2_input_binds_list";
      case MENU_ENUM_LABEL_INPUT_USER_3_BINDS:
         return "3_input_binds_list";
      case MENU_ENUM_LABEL_INPUT_USER_4_BINDS:
         return "4_input_binds_list";
      case MENU_ENUM_LABEL_INPUT_USER_5_BINDS:
         return "5_input_binds_list";
      case MENU_ENUM_LABEL_INPUT_USER_6_BINDS:
         return "6_input_binds_list";
      case MENU_ENUM_LABEL_INPUT_USER_7_BINDS:
         return "7_input_binds_list";
      case MENU_ENUM_LABEL_INPUT_USER_8_BINDS:
         return "8_input_binds_list";
      case MENU_ENUM_LABEL_INPUT_USER_9_BINDS:
         return "9_input_binds_list";
      case MENU_ENUM_LABEL_JOYPAD_AUTOCONFIG_DIR:
         return "joypad_autoconfig_dir";
      case MENU_ENUM_LABEL_JOYPAD_DRIVER:
         return "input_joypad_driver";
      case MENU_ENUM_LABEL_KEYBOARD_OVERLAY_PRESET:
         return "input_osk_overlay";
      case MENU_ENUM_LABEL_LAKKA_SERVICES:
         return "lakka_services";
      case MENU_ENUM_LABEL_LIBRETRO_DIR_PATH:
         return "libretro_dir_path";
      case MENU_ENUM_LABEL_LIBRETRO_INFO_PATH:
         return "libretro_info_path";
      case MENU_ENUM_LABEL_LIBRETRO_LOG_LEVEL:
         return "libretro_log_level";
      case MENU_ENUM_LABEL_LOAD_ARCHIVE:
         return "load_archive";
      case MENU_ENUM_LABEL_LOAD_ARCHIVE_DETECT_CORE:
         return "load_archive_detect_core";
      case MENU_ENUM_LABEL_LOAD_CONTENT:
         return "load_content_default";
      case MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY:
         return "load_recent";
      case MENU_ENUM_LABEL_LOAD_CONTENT_LIST:
         return "load_content";
      case MENU_ENUM_LABEL_LOAD_STATE:
         return "loadstate";
      case MENU_ENUM_LABEL_LOCATION_ALLOW:
         return "location_allow";
      case MENU_ENUM_LABEL_LOCATION_DRIVER:
         return "location_driver";
      case MENU_ENUM_LABEL_LOGGING_SETTINGS:
         return "logging_settings";
      case MENU_ENUM_LABEL_LOG_VERBOSITY:
         return "log_verbosity";
      case MENU_ENUM_LABEL_MAIN_MENU:
         return "main_menu";
      case MENU_ENUM_LABEL_MANAGEMENT:
         return "database_settings";
      case MENU_ENUM_LABEL_MATERIALUI_MENU_COLOR_THEME:
         return "materialui_menu_color_theme";
      case MENU_ENUM_LABEL_MATERIALUI_MENU_FOOTER_OPACITY:
         return "materialui_menu_footer_opacity";
      case MENU_ENUM_LABEL_MATERIALUI_MENU_HEADER_OPACITY:
         return "materialui_menu_header_opacity";
      case MENU_ENUM_LABEL_MENU_DRIVER:
         return "menu_driver";
      case MENU_ENUM_LABEL_MENU_ENUM_THROTTLE_FRAMERATE:
         return "menu_throttle_framerate";
      case MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS:
         return "menu_file_browser_settings";
      case MENU_ENUM_LABEL_MENU_LINEAR_FILTER:
         return "menu_linear_filter";
      case MENU_ENUM_LABEL_MENU_SETTINGS:
         return "menu_settings";
      case MENU_ENUM_LABEL_MENU_WALLPAPER:
         return "menu_wallpaper";
      case MENU_ENUM_LABEL_MENU_WALLPAPER_OPACITY:
         return "menu_wallpaper_opacity";
      case MENU_ENUM_LABEL_MOUSE_ENABLE:
         return "menu_mouse_enable";
      case MENU_ENUM_LABEL_MUSIC_TAB:
         return "music_tab";
      case MENU_ENUM_LABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
         return "menu_navigation_browser_filter_supported_extensions_enable";
      case MENU_ENUM_LABEL_NAVIGATION_WRAPAROUND:
         return "menu_navigation_wraparound_enable";
      case MENU_ENUM_LABEL_NETPLAY:
         return "netplay";
      case MENU_ENUM_LABEL_NETPLAY_CHECK_FRAMES:
         return "netplay_check_frames";
      case MENU_ENUM_LABEL_NETPLAY_CLIENT_SWAP_INPUT:
         return "netplay_client_swap_input";
      case MENU_ENUM_LABEL_NETPLAY_DELAY_FRAMES:
         return "netplay_delay_frames";
      case MENU_ENUM_LABEL_NETPLAY_DISCONNECT:
         return "menu_netplay_disconnect";
      case MENU_ENUM_LABEL_NETPLAY_ENABLE:
         return "netplay_enable";
      case MENU_ENUM_LABEL_NETPLAY_ENABLE_CLIENT:
         return "menu_netplay_enable_client";
      case MENU_ENUM_LABEL_NETPLAY_ENABLE_HOST:
         return "menu_netplay_enable_host";
      case MENU_ENUM_LABEL_NETPLAY_IP_ADDRESS:
         return "netplay_ip_address";
      case MENU_ENUM_LABEL_NETPLAY_MODE:
         return "netplay_mode";
      case MENU_ENUM_LABEL_NETPLAY_NICKNAME:
         return "netplay_nickname";
      case MENU_ENUM_LABEL_NETPLAY_SETTINGS:
         return "menu_netplay_settings";
      case MENU_ENUM_LABEL_NETPLAY_SPECTATOR_MODE_ENABLE:
         return "netplay_spectator_mode_enable";
      case MENU_ENUM_LABEL_NETPLAY_TCP_UDP_PORT:
         return "netplay_tcp_udp_port";
      case MENU_ENUM_LABEL_NETWORK_CMD_ENABLE:
         return "network_cmd_enable";
      case MENU_ENUM_LABEL_NETWORK_CMD_PORT:
         return "network_cmd_port";
      case MENU_ENUM_LABEL_NETWORK_INFORMATION:
         return "network_information";
      case MENU_ENUM_LABEL_NETWORK_INFO_ENTRY:
         return "network_info_entry";
      case MENU_ENUM_LABEL_NETWORK_REMOTE_ENABLE:
         return "network_remote_enable";
      case MENU_ENUM_LABEL_NETWORK_REMOTE_PORT:
         return "network_remote_base_port";
      case MENU_ENUM_LABEL_NETWORK_REMOTE_USER_1_ENABLE:
         return "network_remote_user_1_enable";
      case MENU_ENUM_LABEL_NETWORK_REMOTE_USER_LAST_ENABLE:
         return "network_remote_user_last_enable";
      case MENU_ENUM_LABEL_NETWORK_SETTINGS:
         return "network_settings";
      case MENU_ENUM_LABEL_NO_ACHIEVEMENTS_TO_DISPLAY:
         return "no_achievements_to_display";
      case MENU_ENUM_LABEL_NO_CORES_AVAILABLE:
         return "no_cores_available";
      case MENU_ENUM_LABEL_NO_CORE_INFORMATION_AVAILABLE:
         return "no_core_information_available";
      case MENU_ENUM_LABEL_NO_CORE_OPTIONS_AVAILABLE:
         return "no_core_options_available";
      case MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY:
         return "no_entries_to_display";
      case MENU_ENUM_LABEL_NO_HISTORY_AVAILABLE:
         return "no_history";
      case MENU_ENUM_LABEL_NO_ITEMS:
         return "no_items";
      case MENU_ENUM_LABEL_NO_PERFORMANCE_COUNTERS:
         return "no_performance_counters";
      case MENU_ENUM_LABEL_NO_PLAYLISTS:
         return "no_playlists";
      case MENU_ENUM_LABEL_NO_PLAYLIST_ENTRIES_AVAILABLE:
         return "no_playlist_entries_available";
      case MENU_ENUM_LABEL_NO_SETTINGS_FOUND:
         return "menu_label_no_settings_found";
      case MENU_ENUM_LABEL_NO_SHADER_PARAMETERS:
         return "no_shader_parameters.";
      case MENU_ENUM_LABEL_ONLINE_UPDATER:
         return "online_updater";
      case MENU_ENUM_LABEL_ONSCREEN_DISPLAY_SETTINGS:
         return "onscreen_display_settings";
      case MENU_ENUM_LABEL_ONSCREEN_OVERLAY_SETTINGS:
         return "onscreen_overlay_settings";
      case MENU_ENUM_LABEL_OPEN_ARCHIVE:
         return "open_archive";
      case MENU_ENUM_LABEL_OPEN_ARCHIVE_DETECT_CORE:
         return "open_archive_detect_core";
      case MENU_ENUM_LABEL_OSK_OVERLAY_DIRECTORY:
         return "osk_overlay_directory";
      case MENU_ENUM_LABEL_OVERLAY_AUTOLOAD_PREFERRED:
         return "overlay_autoload_preferred";
      case MENU_ENUM_LABEL_OVERLAY_DIRECTORY:
         return "overlay_directory";
      case MENU_ENUM_LABEL_OVERLAY_OPACITY:
         return "input_overlay_opacity";
      case MENU_ENUM_LABEL_OVERLAY_PRESET:
         return "input_overlay";
      case MENU_ENUM_LABEL_OVERLAY_SCALE:
         return "input_overlay_scale";
      case MENU_ENUM_LABEL_PAL60_ENABLE:
         return "pal60_enable";
      case MENU_ENUM_LABEL_PARENT_DIRECTORY:
         return "parent_directory";
      case MENU_ENUM_LABEL_PAUSE_LIBRETRO:
         return "menu_pause_libretro";
      case MENU_ENUM_LABEL_PAUSE_NONACTIVE:
         return "pause_nonactive";
      case MENU_ENUM_LABEL_PERFCNT_ENABLE:
         return "perfcnt_enable";
      case MENU_ENUM_LABEL_PLAYLISTS_TAB:
         return "playlists_tab";
      case MENU_ENUM_LABEL_PLAYLIST_COLLECTION_ENTRY:
         return "playlist_collection_entry";
      case MENU_ENUM_LABEL_PLAYLIST_DIRECTORY:
         return "playlist_directory";
      case MENU_ENUM_LABEL_PLAYLIST_ENTRY:
         return "playlist_entry";
      case MENU_ENUM_LABEL_PLAYLIST_SETTINGS:
         return "playlist_settings";
      case MENU_ENUM_LABEL_PLAYLIST_SETTINGS_BEGIN:
         return "playlist_settings_begin";
      case MENU_ENUM_LABEL_POINTER_ENABLE:
         return "menu_pointer_enable";
      case MENU_ENUM_LABEL_PRIVACY_SETTINGS:
         return "privacy_settings";
      case MENU_ENUM_LABEL_QUIT_RETROARCH:
         return "quit_retroarch";
      case MENU_ENUM_LABEL_RDB_ENTRY:
         return "rdb_entry";
      case MENU_ENUM_LABEL_RDB_ENTRY_ANALOG:
         return "rdb_entry_analog";
      case MENU_ENUM_LABEL_RDB_ENTRY_BBFC_RATING:
         return "rdb_entry_bbfc_rating";
      case MENU_ENUM_LABEL_RDB_ENTRY_CERO_RATING:
         return "rdb_entry_cero_rating";
      case MENU_ENUM_LABEL_RDB_ENTRY_CRC32:
         return "rdb_entry_crc32";
      case MENU_ENUM_LABEL_RDB_ENTRY_DESCRIPTION:
         return "rdb_entry_description";
      case MENU_ENUM_LABEL_RDB_ENTRY_DEVELOPER:
         return "rdb_entry_developer";
      case MENU_ENUM_LABEL_RDB_ENTRY_EDGE_MAGAZINE_ISSUE:
         return "rdb_entry_edge_magazine_issue";
      case MENU_ENUM_LABEL_RDB_ENTRY_EDGE_MAGAZINE_RATING:
         return "rdb_entry_edge_magazine_rating";
      case MENU_ENUM_LABEL_RDB_ENTRY_EDGE_MAGAZINE_REVIEW:
         return "rdb_entry_edge_magazine_review";
      case MENU_ENUM_LABEL_RDB_ENTRY_ELSPA_RATING:
         return "rdb_entry_elspa_rating";
      case MENU_ENUM_LABEL_RDB_ENTRY_ENHANCEMENT_HW:
         return "rdb_entry_enhancement_hw";
      case MENU_ENUM_LABEL_RDB_ENTRY_ESRB_RATING:
         return "rdb_entry_esrb_rating";
      case MENU_ENUM_LABEL_RDB_ENTRY_FAMITSU_MAGAZINE_RATING:
         return "rdb_entry_famitsu_magazine_rating";
      case MENU_ENUM_LABEL_RDB_ENTRY_FRANCHISE:
         return "rdb_entry_franchise";
      case MENU_ENUM_LABEL_RDB_ENTRY_GENRE:
         return "rdb_entry_genre";
      case MENU_ENUM_LABEL_RDB_ENTRY_MAX_USERS:
         return "rdb_entry_max_users";
      case MENU_ENUM_LABEL_RDB_ENTRY_MD5:
         return "rdb_entry_md5";
      case MENU_ENUM_LABEL_RDB_ENTRY_NAME:
         return "rdb_entry_name";
      case MENU_ENUM_LABEL_RDB_ENTRY_ORIGIN:
         return "rdb_entry_origin";
      case MENU_ENUM_LABEL_RDB_ENTRY_PEGI_RATING:
         return "rdb_entry_pegi_rating";
      case MENU_ENUM_LABEL_RDB_ENTRY_PUBLISHER:
         return "rdb_entry_publisher";
      case MENU_ENUM_LABEL_RDB_ENTRY_RELEASE_MONTH:
         return "rdb_entry_releasemonth";
      case MENU_ENUM_LABEL_RDB_ENTRY_RELEASE_YEAR:
         return "rdb_entry_releaseyear";
      case MENU_ENUM_LABEL_RDB_ENTRY_SERIAL:
         return "rdb_entry_serial";
      case MENU_ENUM_LABEL_RDB_ENTRY_SHA1:
         return "rdb_entry_sha1";
      case MENU_ENUM_LABEL_RDB_ENTRY_START_CONTENT:
         return "rdb_entry_start_content";
      case MENU_ENUM_LABEL_RDB_ENTRY_TGDB_RATING:
         return "rdb_entry_tgdb_rating";
      case MENU_ENUM_LABEL_REBOOT:
         return "reboot";
      case MENU_ENUM_LABEL_RECORDING_CONFIG_DIRECTORY:
         return "recording_config_directory";
      case MENU_ENUM_LABEL_RECORDING_OUTPUT_DIRECTORY:
         return "recording_output_directory";
      case MENU_ENUM_LABEL_RECORDING_SETTINGS:
         return "recording_settings";
      case MENU_ENUM_LABEL_RECORD_CONFIG:
         return "record_config";
      case MENU_ENUM_LABEL_RECORD_DRIVER:
         return "record_driver";
      case MENU_ENUM_LABEL_RECORD_ENABLE:
         return "record_enable";
      case MENU_ENUM_LABEL_RECORD_PATH:
         return "record_path";
      case MENU_ENUM_LABEL_RECORD_USE_OUTPUT_DIRECTORY:
         return "record_use_output_directory";
      case MENU_ENUM_LABEL_REMAP_FILE_LOAD:
         return "remap_file_load";
      case MENU_ENUM_LABEL_REMAP_FILE_SAVE_CORE:
         return "remap_file_save_core";
      case MENU_ENUM_LABEL_REMAP_FILE_SAVE_GAME:
         return "remap_file_save_game";
      case MENU_ENUM_LABEL_RESTART_CONTENT:
         return "restart_content";
      case MENU_ENUM_LABEL_RESTART_RETROARCH:
         return "restart_retroarch";
      case MENU_ENUM_LABEL_RESUME_CONTENT:
         return "resume_content";
      case MENU_ENUM_LABEL_RETRO_ACHIEVEMENTS_SETTINGS:
         return "retro_achievements_settings";
      case MENU_ENUM_LABEL_REWIND_ENABLE:
         return "rewind_enable";
      case MENU_ENUM_LABEL_REWIND_GRANULARITY:
         return "rewind_granularity";
      case MENU_ENUM_LABEL_REWIND_SETTINGS:
         return "rewind_settings";
      case MENU_ENUM_LABEL_RGUI_BROWSER_DIRECTORY:
         return "rgui_browser_directory";
      case MENU_ENUM_LABEL_RGUI_CONFIG_DIRECTORY:
         return "rgui_config_directory";
      case MENU_ENUM_LABEL_RGUI_SHOW_START_SCREEN:
         return "rgui_show_start_screen";
      case MENU_ENUM_LABEL_RUN:
         return "collection";
      case MENU_ENUM_LABEL_SAMBA_ENABLE:
         return "samba_enable";
      case MENU_ENUM_LABEL_SAVEFILE_DIRECTORY:
         return "savefile_directory";
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_INDEX:
         return "savestate_auto_index";
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_LOAD:
         return "savestate_auto_load";
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE:
         return "savestate_auto_save";
      case MENU_ENUM_LABEL_SAVESTATE_DIRECTORY:
         return "savestate_directory";
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG:
         return "save_current_config";
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE:
         return "save_current_config_override_core";
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME:
         return "save_current_config_override_game";
      case MENU_ENUM_LABEL_SAVE_NEW_CONFIG:
         return "save_new_config";
      case MENU_ENUM_LABEL_SAVE_STATE:
         return "savestate";
      case MENU_ENUM_LABEL_SAVING_SETTINGS:
         return "saving_settings";
      case MENU_ENUM_LABEL_SCAN_DIRECTORY:
         return "scan_directory";
      case MENU_ENUM_LABEL_SCAN_FILE:
         return "scan_file";
      case MENU_ENUM_LABEL_SCAN_THIS_DIRECTORY:
         return "scan_this_directory";
      case MENU_ENUM_LABEL_SCREENSHOT_DIRECTORY:
         return "screenshot_directory";
      case MENU_ENUM_LABEL_SCREEN_RESOLUTION:
         return "screen_resolution";
      case MENU_ENUM_LABEL_SETTINGS:
         return "settings";
      case MENU_ENUM_LABEL_SETTINGS_TAB:
         return "settings_tab";
      case MENU_ENUM_LABEL_SHADER_APPLY_CHANGES:
         return "shader_apply_changes";
      case MENU_ENUM_LABEL_SHADER_OPTIONS:
         return "shader_options";
      case MENU_ENUM_LABEL_SHADER_PARAMETERS_ENTRY:
         return "shader_parameters_entry";
      case MENU_ENUM_LABEL_SHOW_ADVANCED_SETTINGS:
         return "menu_show_advanced_settings";
      case MENU_ENUM_LABEL_SHOW_HIDDEN_FILES:
         return "show_hidden_files";
      case MENU_ENUM_LABEL_SHUTDOWN:
         return "shutdown";
      case MENU_ENUM_LABEL_SLOWMOTION_RATIO:
         return "slowmotion_ratio";
      case MENU_ENUM_LABEL_SORT_SAVEFILES_ENABLE:
         return "sort_savefiles_enable";
      case MENU_ENUM_LABEL_SORT_SAVESTATES_ENABLE:
         return "sort_savestates_enable";
      case MENU_ENUM_LABEL_SSH_ENABLE:
         return "ssh_enable";
      case MENU_ENUM_LABEL_START_CORE:
         return "start_core";
      case MENU_ENUM_LABEL_START_NET_RETROPAD:
         return "menu_start_net_retropad";
      case MENU_ENUM_LABEL_START_VIDEO_PROCESSOR:
         return "menu_start_video_processor";
      case MENU_ENUM_LABEL_STATE_SLOT:
         return "state_slot";
      case MENU_ENUM_LABEL_STDIN_CMD_ENABLE:
         return "stdin_commands";
      case MENU_ENUM_LABEL_SUSPEND_SCREENSAVER_ENABLE:
         return "suspend_screensaver_enable";
      case MENU_ENUM_LABEL_SYSTEM_BGM_ENABLE:
         return "system_bgm_enable";
      case MENU_ENUM_LABEL_SYSTEM_DIRECTORY:
         return "system_directory";
      case MENU_ENUM_LABEL_SYSTEM_INFORMATION:
         return "system_information";
      case MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY:
         return "system_info_entry";
      case MENU_ENUM_LABEL_TAKE_SCREENSHOT:
         return "take_screenshot";
      case MENU_ENUM_LABEL_THREADED_DATA_RUNLOOP_ENABLE:
         return "threaded_data_runloop_enable";
      case MENU_ENUM_LABEL_THUMBNAILS:
         return "thumbnails";
      case MENU_ENUM_LABEL_THUMBNAILS_DIRECTORY:
         return "thumbnails_directory";
      case MENU_ENUM_LABEL_THUMBNAILS_UPDATER_LIST:
         return "thumbnails_updater_list";
      case MENU_ENUM_LABEL_TIMEDATE_ENABLE:
         return "menu_timedate_enable";
      case MENU_ENUM_LABEL_TITLE_COLOR:
         return "menu_title_color";
      case MENU_ENUM_LABEL_UI_COMPANION_ENABLE:
         return "ui_companion_enable";
      case MENU_ENUM_LABEL_UI_COMPANION_START_ON_BOOT:
         return "ui_companion_start_on_boot";
      case MENU_ENUM_LABEL_UI_MENUBAR_ENABLE:
         return "ui_menubar_enable";
      case MENU_ENUM_LABEL_UNDO_LOAD_STATE:
         return "undoloadstate";
      case MENU_ENUM_LABEL_UNDO_SAVE_STATE:
         return "undosavestate";
      case MENU_ENUM_LABEL_UPDATER_SETTINGS:
         return "updater_settings";
      case MENU_ENUM_LABEL_UPDATE_ASSETS:
         return "update_assets";
      case MENU_ENUM_LABEL_UPDATE_AUTOCONFIG_PROFILES:
         return "update_autoconfig_profiles";
      case MENU_ENUM_LABEL_UPDATE_CG_SHADERS:
         return "update_cg_shaders";
      case MENU_ENUM_LABEL_UPDATE_CHEATS:
         return "update_cheats";
      case MENU_ENUM_LABEL_UPDATE_CORE_INFO_FILES:
         return "update_core_info_files";
      case MENU_ENUM_LABEL_UPDATE_DATABASES:
         return "update_databases";
      case MENU_ENUM_LABEL_UPDATE_GLSL_SHADERS:
         return "update_glsl_shaders";
      case MENU_ENUM_LABEL_UPDATE_LAKKA:
         return "update_lakka";
      case MENU_ENUM_LABEL_UPDATE_OVERLAYS:
         return "update_overlays";
      case MENU_ENUM_LABEL_UPDATE_SLANG_SHADERS:
         return "update_slang_shaders";
      case MENU_ENUM_LABEL_URL_ENTRY:
         return "url_entry";
      case MENU_ENUM_LABEL_USER_INTERFACE_SETTINGS:
         return "user_interface_settings";
      case MENU_ENUM_LABEL_USER_LANGUAGE:
         return "user_language";
      case MENU_ENUM_LABEL_USER_SETTINGS:
         return "user_settings";
      case MENU_ENUM_LABEL_USE_BUILTIN_IMAGE_VIEWER:
         return "use_builtin_image_viewer";
      case MENU_ENUM_LABEL_USE_BUILTIN_PLAYER:
         return "use_builtin_player";
      case MENU_ENUM_LABEL_USE_THIS_DIRECTORY:
         return "use_this_directory";
      case MENU_ENUM_LABEL_VIDEO_ALLOW_ROTATE:
         return "video_allow_rotate";
      case MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO_AUTO:
         return "video_aspect_ratio_auto";
      case MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO_INDEX:
         return "aspect_ratio_index";
      case MENU_ENUM_LABEL_VIDEO_BLACK_FRAME_INSERTION:
         return "video_black_frame_insertion";
      case MENU_ENUM_LABEL_VIDEO_CROP_OVERSCAN:
         return "video_crop_overscan";
      case MENU_ENUM_LABEL_VIDEO_DISABLE_COMPOSITION:
         return "video_disable_composition";
      case MENU_ENUM_LABEL_VIDEO_DRIVER:
         return "video_driver";
      case MENU_ENUM_LABEL_VIDEO_FILTER:
         return "video_filter";
      case MENU_ENUM_LABEL_VIDEO_FILTER_DIR:
         return "video_filter_dir";
      case MENU_ENUM_LABEL_VIDEO_FILTER_FLICKER:
         return "video_filter_flicker";
      case MENU_ENUM_LABEL_VIDEO_FONT_ENABLE:
         return "video_font_enable";
      case MENU_ENUM_LABEL_VIDEO_FONT_PATH:
         return "video_font_path";
      case MENU_ENUM_LABEL_VIDEO_FONT_SIZE:
         return "video_font_size";
      case MENU_ENUM_LABEL_VIDEO_FORCE_ASPECT:
         return "video_force_aspect";
      case MENU_ENUM_LABEL_VIDEO_FORCE_SRGB_DISABLE:
         return "video_force_srgb_disable";
      case MENU_ENUM_LABEL_VIDEO_FRAME_DELAY:
         return "video_frame_delay";
      case MENU_ENUM_LABEL_VIDEO_FULLSCREEN:
         return "video_fullscreen";
      case MENU_ENUM_LABEL_VIDEO_GAMMA:
         return "video_gamma";
      case MENU_ENUM_LABEL_VIDEO_GPU_RECORD:
         return "video_gpu_record";
      case MENU_ENUM_LABEL_VIDEO_GPU_SCREENSHOT:
         return "video_gpu_screenshot";
      case MENU_ENUM_LABEL_VIDEO_HARD_SYNC:
         return "video_hard_sync";
      case MENU_ENUM_LABEL_VIDEO_HARD_SYNC_FRAMES:
         return "video_hard_sync_frames";
      case MENU_ENUM_LABEL_VIDEO_MAX_SWAPCHAIN_IMAGES:
         return "video_max_swapchain_images";
      case MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_X:
         return "video_message_pos_x";
      case MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_Y:
         return "video_message_pos_y";
      case MENU_ENUM_LABEL_VIDEO_MONITOR_INDEX:
         return "video_monitor_index";
      case MENU_ENUM_LABEL_VIDEO_POST_FILTER_RECORD:
         return "video_post_filter_record";
      case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE:
         return "video_refresh_rate";
      case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_AUTO:
         return "video_refresh_rate_auto";
      case MENU_ENUM_LABEL_VIDEO_ROTATION:
         return "video_rotation";
      case MENU_ENUM_LABEL_VIDEO_SCALE:
         return "video_scale";
      case MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER:
         return "video_scale_integer";
      case MENU_ENUM_LABEL_VIDEO_SETTINGS:
         return "video_settings";
      case MENU_ENUM_LABEL_VIDEO_SHADER_DIR:
         return "video_shader_dir";
      case MENU_ENUM_LABEL_VIDEO_SHADER_FILTER_PASS:
         return "video_shader_filter_pass";
      case MENU_ENUM_LABEL_VIDEO_SHADER_NUM_PASSES:
         return "video_shader_num_passes";
      case MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS:
         return "video_shader_parameters";
      case MENU_ENUM_LABEL_VIDEO_SHADER_PASS:
         return "video_shader_pass";
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET:
         return "video_shader_preset";
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
         return "video_shader_preset_parameters";
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_AS:
         return "video_shader_preset_save_as";
      case MENU_ENUM_LABEL_VIDEO_SHADER_SCALE_PASS:
         return "video_shader_scale_pass";
      case MENU_ENUM_LABEL_VIDEO_SHARED_CONTEXT:
         return "video_shared_context";
      case MENU_ENUM_LABEL_VIDEO_SMOOTH:
         return "video_smooth";
      case MENU_ENUM_LABEL_VIDEO_SOFT_FILTER:
         return "soft_filter";
      case MENU_ENUM_LABEL_VIDEO_SWAP_INTERVAL:
         return "video_swap_interval";
      case MENU_ENUM_LABEL_VIDEO_TAB:
         return "video_tab";
      case MENU_ENUM_LABEL_VIDEO_THREADED:
         return "video_threaded";
      case MENU_ENUM_LABEL_VIDEO_VFILTER:
         return "video_vfilter";
      case MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT:
         return "video_viewport_custom_height";
      case MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH:
         return "video_viewport_custom_width";
      case MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_X:
         return "video_viewport_custom_x";
      case MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_Y:
         return "video_viewport_custom_y";
      case MENU_ENUM_LABEL_VIDEO_VI_WIDTH:
         return "video_vi_width";
      case MENU_ENUM_LABEL_VIDEO_VSYNC:
         return "video_vsync";
      case MENU_ENUM_LABEL_VIDEO_WINDOWED_FULLSCREEN:
         return "video_windowed_fullscreen";
      case MENU_ENUM_LABEL_WIFI_DRIVER:
         return "wifi_driver";
      case MENU_ENUM_LABEL_WIFI_SETTINGS:
         return "wifi_settings";
      case MENU_ENUM_LABEL_XMB_ALPHA_FACTOR:
         return "xmb_alpha_factor";
      case MENU_ENUM_LABEL_XMB_FONT:
         return "xmb_font";
      case MENU_ENUM_LABEL_XMB_MENU_COLOR_THEME:
         return "xmb_menu_color_theme";
      case MENU_ENUM_LABEL_XMB_RIBBON_ENABLE:
         return "xmb_ribbon_enable";
      case MENU_ENUM_LABEL_XMB_SCALE_FACTOR:
         return "xmb_scale_factor";
      case MENU_ENUM_LABEL_XMB_SHADOWS_ENABLE:
         return "xmb_shadows_enable";
      case MENU_ENUM_LABEL_XMB_SHOW_HISTORY:
         return "xmb_show_history";
      case MENU_ENUM_LABEL_XMB_SHOW_IMAGES:
         return "xmb_show_images";
      case MENU_ENUM_LABEL_XMB_SHOW_MUSIC:
         return "xmb_show_music";
      case MENU_ENUM_LABEL_XMB_SHOW_SETTINGS:
         return "xmb_show_settings";
      case MENU_ENUM_LABEL_XMB_SHOW_VIDEO:
         return "xmb_show_video";
      case MENU_ENUM_LABEL_XMB_THEME:
         return "xmb_theme";
      case MSG_BRINGING_UP_COMMAND_INTERFACE_ON_PORT:
         return "bringing_up_command_interface_at_port";
      case MSG_CONNECT_DEVICE_FROM_A_VALID_PORT:
         return "connect_device_from_a_valid_port";
      case MSG_DEVICE_CONFIGURED_IN_PORT:
         return "configured in port";
      case MSG_DEVICE_NOT_CONFIGURED:
         return "not configured";
      case MSG_DISCONNECTING_DEVICE_FROM_PORT:
         return "disconnecting_device_from_port";
      case MSG_DISCONNECT_DEVICE_FROM_A_VALID_PORT:
         return "disconnect_device_from_a_valid_port";
      case MSG_FAILED:
         return "failed";
      case MSG_FAILED_TO_SET_DISK:
         return "Failed to set disk";
      case MSG_FAILED_TO_START_AUDIO_DRIVER:
         return "failed_to_start_audio_driver";
      case MSG_FOUND_LAST_STATE_SLOT:
         return "found_last_state_slot";
      case MSG_SUCCEEDED:
         return "succeeded";
      default:
         break;
   }

   return "null";
}
#endif

#define MSG_HASH(Id, str) case Id: return str;

const char *msg_hash_to_str_us(enum msg_hash_enums msg)
{
#ifdef HAVE_MENU
   const char *ret = menu_hash_to_str_us_label_enum(msg);

   if (ret && !string_is_equal(ret, "null"))
      return ret;
#endif

   switch (msg)
   {
      #include "msg_hash_us.h"
      default:
#if 0
         RARCH_LOG("Unimplemented: [%d]\n", msg);
#endif
         break;
   }

   return "null";
}

#undef MSG_HASH
