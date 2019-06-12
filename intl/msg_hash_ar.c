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
#include "../configuration.h"
#include "../verbosity.h"

#if defined(_MSC_VER) && !defined(_XBOX) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
#if (_MSC_VER >= 1700)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif
#pragma warning(disable: 4566)
#endif

int menu_hash_get_help_ar_enum(enum msg_hash_enums msg, char *s, size_t len)
{
    settings_t *settings = config_get_ptr();

    if (msg == MENU_ENUM_LABEL_CONNECT_NETPLAY_ROOM)
    {
       snprintf(s, len,
             "TODO/FIXME - Fill in message here."
             );
       return 0;
    }
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
          case RARCH_SLOWMOTION_KEY:
             snprintf(s, len,
                   "Toggles slowmotion.");
             break;
          case RARCH_SLOWMOTION_HOLD_KEY:
             snprintf(s, len,
                   "Hold for slowmotion.");
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
          case RARCH_NETPLAY_GAME_WATCH:
             snprintf(s, len,
                   "Netplay toggle play/spectate mode.");
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
          case RARCH_GAME_FOCUS_TOGGLE:
             snprintf(s, len,
                   "Toggles game focus.\n"
                   " \n"
                   "When a game has focus, RetroArch will both disable \n"
                   "hotkeys and keep/warp the mouse pointer inside the window.");
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
          case RARCH_BSV_RECORD_TOGGLE:
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
        case MENU_ENUM_LABEL_ADD_TO_FAVORITES:
            snprintf(s, len, "Add the entry to your Favorites.");
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
                             "Will be ignored when using netplay."
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
        case MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS:
            snprintf(s, len,
                      "Show keyboard/controller button presses on \n"
                            "the onscreen overlay.");
            break;
        case MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT:
            snprintf(s, len,
                      "Select the port to listen for controller input \n"
                            "to display on the onscreen overlay.");
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
        case MENU_ENUM_LABEL_INPUT_BIND_HOLD:
            snprintf(s, len,
               "Input bind hold time (in seconds). \n"
               "Amount of seconds to hold an input to bind it.");
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
                             "file browser everytime.\n"
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
               else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_INPUT_DRIVER_LINUXRAW)))
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
            snprintf(s, len,
                     "Current Video driver.");

            if (string_is_equal(settings->arrays.video_driver, "gl"))
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
            else if (string_is_equal(settings->arrays.video_driver, "sdl2"))
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
            else if (string_is_equal(settings->arrays.video_driver, "sdl1"))
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
            else if (string_is_equal(settings->arrays.video_driver, "d3d"))
            {
                snprintf(s, len,
                         "Direct3D Video driver. \n"
                                 " \n"
                                 "Performance for software-rendered cores \n"
                                 "is dependent on your graphic card's \n"
                                 "underlying D3D driver).");
            }
            else if (string_is_equal(settings->arrays.video_driver, "exynos"))
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
            else if (string_is_equal(settings->arrays.video_driver, "drm"))
            {
                snprintf(s, len,
                         "Plain DRM Video Driver. \n"
                                 " \n"
                                 "This is a low-level video driver using. \n"
                                 "libdrm for hardware scaling using \n"
                                 "GPU overlays.");
            }
            else if (string_is_equal(settings->arrays.video_driver, "sunxi"))
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
            {
               const char *lbl = settings ? settings->arrays.audio_resampler : NULL;

               if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER_SINC)))
                  strlcpy(s,
                        "Windowed SINC implementation.", len);
               else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER_CC)))
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
        case MENU_ENUM_LABEL_GAME_FOCUS_TOGGLE:
            snprintf(s, len,
                     "Toggles game focus.\n"
                             " \n"
                             "When a game has focus, RetroArch will both disable \n"
                             "hotkeys and keep/warp the mouse pointer inside the window.");
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
                             " The place to store backgrounds that will \n"
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
                     "If disabled, the game will keep \n"
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
        case MENU_ENUM_LABEL_NETPLAY_PUBLIC_ANNOUNCE:
            snprintf(s, len,
                     "Whether to announce netplay games publicly. \n"
                             " \n"
                             "If set to false, clients must manually connect \n"
                             "rather than using the public lobby.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_START_AS_SPECTATOR:
            snprintf(s, len,
                     "Whether to start netplay in spectator mode. \n"
                             " \n"
                             "If set to true, netplay will be in spectator mode \n"
                             "on start. It's always possible to change mode \n"
                             "later.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_ALLOW_SLAVES:
            snprintf(s, len,
                     "Whether to allow connections in slave mode. \n"
                             " \n"
                             "Slave-mode clients require very little processing \n"
                             "power on either side, but will suffer \n"
                             "significantly from network latency.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_REQUIRE_SLAVES:
            snprintf(s, len,
                     "Whether to disallow connections not in slave mode. \n"
                             " \n"
                             "Not recommended except for very fast networks \n"
                             "with very weak machines. \n");
            break;
        case MENU_ENUM_LABEL_NETPLAY_STATELESS_MODE:
            snprintf(s, len,
                     "Whether to run netplay in a mode not requiring\n"
                             "save states. \n"
                             " \n"
                             "If set to true, a very fast network is required,\n"
                             "but no rewinding is performed, so there will be\n"
                             "no netplay jitter.\n");
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
        case MENU_ENUM_LABEL_NETPLAY_NAT_TRAVERSAL:
            snprintf(s, len,
                     "When hosting, attempt to listen for\n"
                             "connections from the public internet, using\n"
                             "UPnP or similar technologies to escape LANs. \n");
            break;
        case MENU_ENUM_LABEL_NETPLAY_USE_MITM_SERVER:
            snprintf(s, len,
                     "When hosting a netplay session, relay connection through a \n"
                             "man-in-the-middle server \n"
                             "to get around firewalls or NAT/UPnP issues. \n");
            break;
        case MENU_ENUM_LABEL_NETPLAY_MITM_SERVER:
            snprintf(s, len,
                     "Specifies the man-in-the-middle server \n"
                             "to use for netplay. A server that is \n"
                             "located closer to you may have less latency. \n");
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
        case MENU_ENUM_LABEL_BATTERY_LEVEL_ENABLE:
            snprintf(s, len,
                     "Shows current battery level inside menu.");
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
        case MENU_ENUM_LABEL_NETPLAY_LAN_SCAN_SETTINGS:
            snprintf(s, len,
                     "Search for and connect to netplay hosts on the local network.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_SETTINGS:
            snprintf(s, len,
                     "Setting related to Netplay.");
            break;
        case MENU_ENUM_LABEL_DYNAMIC_WALLPAPER:
            snprintf(s, len,
                     "Dynamically load a new background \n"
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
        case MENU_ENUM_LABEL_NETPLAY_PASSWORD:
            snprintf(s, len,
                     "The password for connecting to the netplay \n"
                             "host. Used only in host mode.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_SPECTATE_PASSWORD:
            snprintf(s, len,
                     "The password for connecting to the netplay \n"
                             "host with only spectator privileges. Used \n"
                             "only in host mode.");
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
            snprintf(s, len, "Allows any user to control the menu. \n"
                    " \n"
                    "When disabled, only user 1 can control the menu.");
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
        case MENU_ENUM_LABEL_NETPLAY_GAME_WATCH:
            snprintf(s, len,
                     "Netplay toggle play/spectate mode.");
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
        case MENU_ENUM_LABEL_SLOWMOTION_HOLD:
            snprintf(s, len,
                     "Hold for slowmotion.");
            break;
        case MENU_ENUM_LABEL_FRAME_ADVANCE:
            snprintf(s, len,
                     "Frame advance when content is paused.");
            break;
        case MENU_ENUM_LABEL_BSV_RECORD_TOGGLE:
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
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_ENABLE:
            snprintf(s, len,
                     "Enables a background color for the OSD.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_RED:
            snprintf(s, len,
                     "Sets the red value of the OSD background color. Valid values are between 0 and 255.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_GREEN:
            snprintf(s, len,
                     "Sets the green value of the OSD background color. Valid values are between 0 and 255.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_BLUE:
            snprintf(s, len,
                     "Sets the blue value of the OSD background color. Valid values are between 0 and 255.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_OPACITY:
            snprintf(s, len,
                     "Sets the opacity of the OSD background color. Valid values are between 0.0 and 1.0.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_RED:
            snprintf(s, len,
                     "Sets the red value of the OSD text color. Valid values are between 0 and 255.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_GREEN:
            snprintf(s, len,
                     "Sets the green value of the OSD text color. Valid values are between 0 and 255.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_BLUE:
            snprintf(s, len,
                     "Sets the blue value of the OSD text color. Valid values are between 0 and 255.");
            break;
        default:
            if (string_is_empty(s))
                strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
            return -1;
    }

    return 0;
}

#ifdef HAVE_MENU
static const char *menu_hash_to_str_ar_label_enum(enum msg_hash_enums msg)
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

const char *msg_hash_to_str_ar(enum msg_hash_enums msg) {
#ifdef HAVE_MENU
    const char *ret = menu_hash_to_str_ar_label_enum(msg);

    if (ret && !string_is_equal(ret, "null"))
       return ret;
#endif

    switch (msg) {
#include "msg_hash_ar.h"
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
