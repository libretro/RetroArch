/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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
#pragma warning( disable : 4566 )
#endif

int menu_hash_get_help_chs_enum(enum msg_hash_enums msg, char *s, size_t len)
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
                  "在快进和正常速度之间切换。"
                  );
            break;
         case RARCH_FAST_FORWARD_HOLD_KEY:
            snprintf(s, len,
                  "按住来快进。 \n"
                  " \n"
                  "放开按键来取消快进。"
                  );
            break;
         case RARCH_PAUSE_TOGGLE:
            snprintf(s, len,
                  "在暂停和不暂停状态间切换。");
            break;
         case RARCH_FRAMEADVANCE:
            snprintf(s, len,
                  "Frame advance when content is paused.");
            break;
         case RARCH_SHADER_NEXT:
            snprintf(s, len,
                  "应用文件夹中的下一个Shader特效。");
            break;
         case RARCH_SHADER_PREV:
            snprintf(s, len,
                  "应用文件夹中的上一个Shader特效。");
            break;
         case RARCH_CHEAT_INDEX_PLUS:
         case RARCH_CHEAT_INDEX_MINUS:
         case RARCH_CHEAT_TOGGLE:
            snprintf(s, len,
                  "金手指。");
            break;
         case RARCH_RESET:
            snprintf(s, len,
                  "重置游戏内容。");
            break;
         case RARCH_SCREENSHOT:
            snprintf(s, len,
                  "截图。");
            break;
         case RARCH_MUTE:
            snprintf(s, len,
                  "静音/取消静音。");
            break;
         case RARCH_OSK:
            snprintf(s, len,
                  "显示/隐藏屏显键盘。");
            break;
         case RARCH_NETPLAY_FLIP:
            snprintf(s, len,
                  "Netplay flip users.");
            break;
         case RARCH_SLOWMOTION:
            snprintf(s, len,
                  "按住并以慢动作运行。");
            break;
         case RARCH_ENABLE_HOTKEY:
            snprintf(s, len,
                  "启用其他热键. \n"
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
                  "提高音量。");
            break;
         case RARCH_VOLUME_DOWN:
            snprintf(s, len,
                  "降低音量。");
            break;
         case RARCH_OVERLAY_NEXT:
            snprintf(s, len,
                  "切换到下一个屏幕覆层。将会循环选择。");
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
                  "鼠标捕获开关 \n"
                  " \n"
                  "当鼠标捕获开启时，RetroArch将会隐藏鼠标指针，并 \n"
                  "使之控制在窗口中。这使得一些依靠鼠标相对位置的程 \n"
                  "序能更好运行。");
            break;
         case RARCH_MENU_TOGGLE:
            snprintf(s, len, "切换菜单显示。");
            break;
         case RARCH_LOAD_STATE_KEY:
            snprintf(s, len,
                  "加载游戏状态。");
            break;
         case RARCH_FULLSCREEN_TOGGLE_KEY:
            snprintf(s, len,
                  "切换到全屏模式。");
            break;
         case RARCH_QUIT_KEY:
            snprintf(s, len,
                  "用于正常退出 RetroArch 的按键 \n"
                  " \n"
                  "使用任何强制手段来退出RetroArch（如发送SIGKILL） \n"
                  "可能导致系统无法正确存储数据。"
#ifdef __unix__
                  "\n在类Unix系统上，SIGINT/SIGTERM信号能够保证正常 \n"
                  "关闭程序。"
#endif
                  "");
            break;
         case RARCH_STATE_SLOT_PLUS:
         case RARCH_STATE_SLOT_MINUS:
            snprintf(s, len,
                  "状态存储槽 \n"
                  " \n"
                  "当槽位选择为0时，状态存档将以*.state命名（或其他 \n"
                  "在命令行中定义的名称）。 \n"
                  " \n"
                  "当状态槽不为0时，路径将会设为<path><d>，其中<d> \n"
                  "是状态槽的索引。");
            break;
         case RARCH_SAVE_STATE_KEY:
            snprintf(s, len,
                  "保存游戏状态。");
            break;
         case RARCH_REWIND:
            snprintf(s, len,
                  "按住按钮来回溯 \n"
                  " \n"
                  "回溯功能必须被启用。");
            break;
         case RARCH_MOVIE_RECORD_TOGGLE:
            snprintf(s, len,
                  "在录制和非录制模式切换。");
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
         snprintf(s, len, "你的登陆信息 \n"
               "Retro Achievements 账号. \n"
               " \n"
               "访问 retroachievements.org 并注册 \n"
               "以获取一个免费账号. \n"
               " \n"
               "在你注册以后, 你需要 \n"
               "在RetroArch输入你的 \n"
               "账号以及密码.");
         break;
      case MENU_ENUM_LABEL_CHEEVOS_USERNAME:
         snprintf(s, len, "Username for your Retro Achievements account.");
         break;
      case MENU_ENUM_LABEL_CHEEVOS_PASSWORD:
         snprintf(s, len, "Password for your Retro Achievements account.");
         break;
      case MENU_ENUM_LABEL_USER_LANGUAGE:
         snprintf(s, len, "依据选择的语言来本地化菜单和其他屏显消息。 \n"
               " \n"
               "需要重新启动来使之生效。 \n"
               " \n"
               "注意：可能不是所有的语言都已完成翻译工作。 \n"
               " \n"
               "若一个语言没有被翻译，则会退回到英语显示。");
         break;
      case MENU_ENUM_LABEL_VIDEO_FONT_PATH:
         snprintf(s, len, "改变屏显文字的字体。");
         break;
      case MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS:
         snprintf(s, len, "自动加载游戏内容指定的核心选项.");
         break;
      case MENU_ENUM_LABEL_AUTO_OVERRIDES_ENABLE:
         snprintf(s, len, "Automatically load override configurations.");
         break;
      case MENU_ENUM_LABEL_AUTO_REMAPS_ENABLE:
         snprintf(s, len, "自动加载输入重映射文件.");
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
         snprintf(s, len, "创建一份截图. \n"
               " \n"
               "截图文件将会存放在 \n"
               "截图目录之中.");
         break;
      case MENU_ENUM_LABEL_RUN:
         snprintf(s, len, "启动内容.");
         break;
      case MENU_ENUM_LABEL_INFORMATION:
         snprintf(s, len, "显示本内容的额外 \n"
               "元数据信息.");
         break;
      case MENU_ENUM_LABEL_FILE_BROWSER_CONFIG:
         snprintf(s, len, "配置文件.");
         break;
      case MENU_ENUM_LABEL_FILE_BROWSER_COMPRESSED_ARCHIVE:
         snprintf(s, len, "压缩归档文件.");
         break;
      case MENU_ENUM_LABEL_FILE_BROWSER_RECORD_CONFIG:
         snprintf(s, len, "记录配置文件.");
         break;
      case MENU_ENUM_LABEL_FILE_BROWSER_CURSOR:
         snprintf(s, len, "Database cursor file.");
         break;
      case MENU_ENUM_LABEL_FILE_CONFIG:
         snprintf(s, len, "配置文件.");
         break;
      case MENU_ENUM_LABEL_SCAN_THIS_DIRECTORY:
         snprintf(s, len,
               "选择本项以扫描当前 \n"
               "目录中的内容.");
         break;
      case MENU_ENUM_LABEL_USE_THIS_DIRECTORY:
         snprintf(s, len,
               "选择本目录作为指定目录.");
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
               "缩略图目录. \n"
               " \n"
               "用以存放缩略图.");
         break;
      case MENU_ENUM_LABEL_LIBRETRO_INFO_PATH:
         snprintf(s, len,
               "核心Core信息目录. \n"
               " \n"
               "A directory for where to search \n"
               "for libretro core information.");
         break;
      case MENU_ENUM_LABEL_PLAYLIST_DIRECTORY:
         snprintf(s, len,
               "运行列表目录. \n"
               " \n"
               "Save all playlist files to this \n"
               "directory.");
         break;
      case MENU_ENUM_LABEL_DUMMY_ON_CORE_SHUTDOWN:
         snprintf(s, len,
               "某些libretro核心可能会 \n"
               "支持关机特性. \n"
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
               "回到上级目录。");
         break;
      case MENU_ENUM_LABEL_FILE_BROWSER_SHADER_PRESET:
         snprintf(s, len,
               "Shader预设文件。");
         break;
      case MENU_ENUM_LABEL_FILE_BROWSER_SHADER:
         snprintf(s, len,
               "Shader文件。");
         break;
      case MENU_ENUM_LABEL_FILE_BROWSER_REMAP:
         snprintf(s, len,
               "控制重映射文件。");
         break;
      case MENU_ENUM_LABEL_FILE_BROWSER_CHEAT:
         snprintf(s, len,
               "金手指文件。");
         break;
      case MENU_ENUM_LABEL_FILE_BROWSER_OVERLAY:
         snprintf(s, len,
               "覆层文件。");
         break;
      case MENU_ENUM_LABEL_FILE_BROWSER_RDB:
         snprintf(s, len,
               "数据库文件。");
         break;
      case MENU_ENUM_LABEL_FILE_BROWSER_FONT:
         snprintf(s, len,
               "TrueType字体文件。");
         break;
      case MENU_ENUM_LABEL_FILE_BROWSER_PLAIN_FILE:
         snprintf(s, len,
               "普通文件。");
         break;
      case MENU_ENUM_LABEL_FILE_BROWSER_MOVIE_OPEN:
         snprintf(s, len,
               "视频 \n"
               " \n"
               "选择文件并使用视频播放器打开。");
         break;
      case MENU_ENUM_LABEL_FILE_BROWSER_MUSIC_OPEN:
         snprintf(s, len,
               "音乐 \n"
               " \n"
               "选择文件并使用音乐播放器打开。");
         break;
      case MENU_ENUM_LABEL_FILE_BROWSER_IMAGE:
         snprintf(s, len,
               "图片文件。");
         break;
      case MENU_ENUM_LABEL_FILE_BROWSER_IMAGE_OPEN_WITH_VIEWER:
         snprintf(s, len,
               "图片 \n"
               " \n"
               "选择文件并使用图片浏览器打开。");
         break;
      case MENU_ENUM_LABEL_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION:
         snprintf(s, len,
               "Libretro核心 \n"
               " \n"
               "选中核心将会使其关联至游戏。");
         break;
      case MENU_ENUM_LABEL_FILE_BROWSER_CORE:
         snprintf(s, len,
               "Libretro核心 \n"
               " \n"
               "选择该文件使 RetroArch 加载该核心。");
         break;
      case MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY:
         snprintf(s, len,
               "目录 \n"
               " \n"
               "选择并打开该文件夹。");
         break;
      case MENU_ENUM_LABEL_CACHE_DIRECTORY:
         snprintf(s, len,
               "缓存目录 \n"
               " \n"
               "被RetroArch解压的游戏内容会临时存放到这个文 \n"
               "件夹。");
         break;
      case MENU_ENUM_LABEL_HISTORY_LIST_ENABLE:
         snprintf(s, len,
               "若开启，所有在RetroArch中加载过的文件 \n"
               "都会自动的放入最近使用历史列表中。");
         break;
      case MENU_ENUM_LABEL_RGUI_BROWSER_DIRECTORY:
         snprintf(s, len,
               "文件浏览器目录 \n"
               " \n"
               "设置文件浏览器的初始目录。");
         break;
      case MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR:
         snprintf(s, len,
               "影响输入轮询过程在RetroArch中的执行方式。 \n"
               " \n"
               "稍早 - 输入轮询过程将在帧生成之前执行。 \n"
               "正常 - 输入轮询过程将在被请求时执行。 \n"
               "稍晚 - 输入轮询过程将在每一帧的首次请求时执行。 \n"
               " \n"
               "依据设置的不同，设置为“稍早”或“稍晚”可以获得较低 \n"
               "的时延。 \n"
               "当在进行在线游戏时，不管设置的值如何，都只会启用 \n"
               "正常模式进行输入轮询过程。"
               );
         break;
      case MENU_ENUM_LABEL_INPUT_DESCRIPTOR_HIDE_UNBOUND:
         snprintf(s, len,
               "隐藏不被核心使用的输入描述。");
         break;
      case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE:
         snprintf(s, len,
               "显示器的视频刷新率。 \n"
               "可被用来计算一个合适的音频输入率。");
         break;
      case MENU_ENUM_LABEL_VIDEO_FORCE_SRGB_DISABLE:
         snprintf(s, len,
               "强制关闭sRGB帧缓冲支持。\n"
               "\n"
               "某些在Windows上的Intel的OpenGL驱动会对sRGB帧缓 \n"
               "冲支持产生问题，需要启用以强制关闭程序对其的使用。");
         break;
      case MENU_ENUM_LABEL_AUDIO_ENABLE:
         snprintf(s, len,
               "启用音频输出。");
         break;
      case MENU_ENUM_LABEL_AUDIO_SYNC:
         snprintf(s, len,
               "同步音频（推荐）。");
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
               "屏显信息的字体大小.");
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
               "显示/隐藏屏显信息.");
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
               "音频输出采样率.");
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
               "加载内核. \n"
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
               "你可以使用下述的方式通过游戏控制器或者键盘来对\n"
               "菜单进行控制：\n"
               " \n"
               );
         break;
      case MENU_ENUM_LABEL_WELCOME_TO_RETROARCH:
         snprintf(s, len,
               "欢迎来到 RetroArch\n"
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
               "若要扫描游戏内容，请访问菜单「%s」 \n"
               "并选择「%s」或者「%s」。\n"
               " \n"
               "文件将会同数据库中的条目进行对比。 \n"
               "若文件匹配某个条目，则它会被加入收藏中。 \n"
               " \n"
               "你可以无需每次都打开文件浏览器，而可以直接 \n"
               "通过菜单项「%s」->「%s」 来访\n"
               "问这些游戏内容。 \n"
               " \n"
               "注意：不是所有核心的游戏内容都支持扫描录入。"
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
               "欢迎使用RetroArch\n"
               "\n"
               "正在解压必要文件, 请稍等。\n"
               "这可能需要一点时间……\n"
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
               "加载游戏内容 \n"
               "通过浏览来加载游戏内容。 \n"
               " \n"
               "你需要同时提供一个“核心”和游戏内容文 \n"
               "件才能启动并加载游戏内容。 \n"
               " \n"
               "设置“文件浏览器目录”可以指定以哪个位 \n"
               "置为文件浏览器的默认目录以方便加载。 \n"
               "若没有设置，默认以根目录为基准。 \n"
               " \n"
               "文件浏览器会以上次加载的核心所支持的 \n"
               "扩展名进行过滤，并使用该核心来加载游 \n"
               "戏内容。"
               );
         break;
      case MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY:
         snprintf(s, len,
               "从历史记录中加载内容. \n"
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
               "当前视频驱动.");

         if (string_is_equal(settings->video.driver, "gl"))
         {
            snprintf(s, len,
                  "OpenGL视频驱动. \n"
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
                  "SDL 2 视频驱动.\n"
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
                  "SDL 视频驱动.\n"
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
                  "Direct3D 视频驱动. \n"
                  " \n"
                  "Performance for software-rendered cores \n"
                  "is dependent on your graphic card's \n"
                  "underlying D3D driver).");
         }
         else if (string_is_equal(settings->video.driver, "exynos"))
         {
            snprintf(s, len,
                  "Exynos-G2D 视频驱动. \n"
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
                  "Plain DRM 视频驱动. \n"
                  " \n"
                  "This is a low-level video driver using. \n"
                  "libdrm for hardware scaling using \n"
                  "GPU overlays.");
         }
         else if (string_is_equal(settings->video.driver, "sunxi"))
         {
            snprintf(s, len,
                  "Sunxi-G2D 视频驱动. \n"
                  " \n"
                  "This is a low-level Sunxi video driver. \n"
                  "Uses the G2D block in Allwinner SoCs.");
         }
         break;
      case MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN:
         snprintf(s, len,
               "音频DSP插件.\n"
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
               "载入预设 Shader. \n"
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
               " 预设目录. \n"
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
      case MENU_ENUM_LABEL_SHOW_HIDDEN_FILES:
         snprintf(s, len, "显示隐藏文件和文件夹。");
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
               "输入设备类型. \n"
               " \n"
               "Picks which device type to use. This is \n"
               "relevant for the libretro core itself."
               );
         break;
      case MENU_ENUM_LABEL_LIBRETRO_LOG_LEVEL:
         snprintf(s, len,
               "设置libretro核心的log等级 \n"
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
               "即时存档栏位.\n"
               " \n"
               " With slot set to 0, save state name is *.state \n"
               " (or whatever defined on commandline).\n"
               "When slot is != 0, path will be (path)(d), \n"
               "where (d) is slot number.");
         break;
      case MENU_ENUM_LABEL_SHADER_APPLY_CHANGES:
         snprintf(s, len,
               "应用Shader更改. \n"
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
               "切换菜单.");
         break;
      case MENU_ENUM_LABEL_GRAB_MOUSE_TOGGLE:
         snprintf(s, len,
               "切换鼠标抓取.\n"
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
               "ALSA 需要一个PCM设备."
#endif
#ifdef HAVE_OSS
               " \n"
               "OSS 需要一个路径 (例如. /dev/dsp)."
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
               "启用其他热键.\n"
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
               "启用回溯倒带功能.\n"
               " \n"
               "这可能会严重影响性能, \n"
               "所以缺省设置为关闭.");
         break;
      case MENU_ENUM_LABEL_LIBRETRO_DIR_PATH:
         snprintf(s, len,
               "核心目录. \n"
               " \n"
               "A directory for where to search for \n"
               "libretro core implementations.");
         break;
      case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_AUTO:
         snprintf(s, len,
               "自动匹配刷新率.\n"
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
               "全屏分辨率.\n"
               " \n"
               "Resolution of 0 uses the \n"
               "resolution of the environment.\n");
         break;
      case MENU_ENUM_LABEL_FASTFORWARD_RATIO:
         snprintf(s, len,
               "快进比率."
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
               "指定输出显示器.\n"
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
               "调高音量.");
         break;
      case MENU_ENUM_LABEL_VOLUME_DOWN:
         snprintf(s, len,
               "降低音量.");
         break;
      case MENU_ENUM_LABEL_VIDEO_DISABLE_COMPOSITION:
         snprintf(s, len,
               "Forcibly disable composition.\n"
               "Only valid on Windows Vista/7 for now.");
         break;
      case MENU_ENUM_LABEL_PERFCNT_ENABLE:
         snprintf(s, len,
               "启用或关闭前端 \n"
               "性能计数.");
         break;
      case MENU_ENUM_LABEL_SYSTEM_DIRECTORY:
         snprintf(s, len,
               "系统目录. \n"
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
               "视频垂直同步.\n");
         break;
      case MENU_ENUM_LABEL_VIDEO_HARD_SYNC:
         snprintf(s, len,
               "尝试硬件同步 \n"
               "CPU和GPU.\n"
               " \n"
               "可以降低潜在的性能 \n"
               "开销.");
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
               "截图目录 \n"
               " \n"
               "用于保存截图的文件夹。"
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
               "游戏存盘目录. \n"
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
               "即时存档目录. \n"
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
               "动态壁纸目录 \n"
               " \n"
               "保存用于主界面的、依据游戏内容变化的动态壁纸。");
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
      case MENU_ENUM_LABEL_NETPLAY_LAN_SCAN_SETTINGS:
         snprintf(s, len,
               "Search for and connect to netplay hosts on the local network.");
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
               "打开金手指索引.\n");
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
               "RetroArch本身并不能做什么事情。 \n"
               " \n"
               "如果想在上面干点什么，你需要向它加载一个程 \n"
               "序。 \n"
               "\n"
               "我们把这样的程序叫做“Libretro核心”，简单 \n"
               "的称呼其为“核心”。 \n"
               " \n"
               "你可以从“加载核心”菜单中选择一个核心。 \n"
               " \n"
#ifdef HAVE_NETWORKING
               "你可以通过以下几种方法来获取核心: \n"
               "一、通过访问菜单项「%s」 \n"
               " -> 「%s」来下载；\n"
               "二、手动将其移入核心目录中，访问目录设置 \n"
               "找到你的“%s”。",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH)
#else
               "你可以通过手动将核心移入目录中来添加他 \n"
               "们，访问目录设置找到你的“%s”。",
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

const char *msg_hash_to_str_chs(enum msg_hash_enums msg)
{
   switch (msg)
   {
      case MSG_SUCCEEDED:
         return "已成功";
      case MENU_ENUM_SUBLABEL_MENU_SETTINGS:
         return "调整菜单屏幕相关的设置。";
      case MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC:
         return "强制同步CPU和GPU，以性能为代价换取低延迟。";
      case MENU_ENUM_SUBLABEL_VIDEO_THREADED:
         return "以延迟和视频撕裂为代价换取高性能，当且仅当能达到全速模拟时使用。";
      case MSG_AUTODETECT:
         return "自动检测";
      case MSG_DEVICE_NOT_CONFIGURED:
         return "没有配置";
      case MSG_FAILED_TO_SET_DISK:
         return "设置磁盘失败";
      case MSG_FAILED:
         return "已失败";
      case MSG_FILE_ALREADY_EXISTS_SAVING_TO_BACKUP_BUFFER:
         return "文件已存在. Saving to backup buffer";
      case MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD:
         return "密码";
      case MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS:
         return "Accounts Cheevos";
      case MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME:
         return "用户名";
      case MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST:
         return "账户";
      case MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST_END:
         return "账户列表终端";
      case MENU_ENUM_LABEL_VALUE_ACCOUNTS_RETRO_ACHIEVEMENTS:
         return "Retro 成就";
      case MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST:
         return "成就列表";
      case MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST:
         return "添加游戏内容";
      case MENU_ENUM_LABEL_VALUE_ADD_TAB:
         return "导入游戏内容";
      case MENU_ENUM_LABEL_VALUE_ARCHIVE_MODE:
         return "Archive File Association Action";
      case MENU_ENUM_LABEL_VALUE_ASK_ARCHIVE:
         return "询问";
      case MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY:
         return "资源目录";
      case MENU_ENUM_LABEL_VALUE_AUDIO_BLOCK_FRAMES:
         return "Block Frames";
      case MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE:
         return "音频设备";
      case MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER:
         return "音频驱动";
      case MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN:
         return "音频DSP插件";
      case MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE:
         return "启用音频";
      case MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR:
         return "音频过滤器目录";
      case MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY:
         return "音频时延(ms)";
      case MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW:
         return "音频最大采样间隔";
      case MENU_ENUM_LABEL_VALUE_AUDIO_MUTE:
         return "音频静音";
      case MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE:
         return "音频输出码率(KHz)";
      case MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA:
         return "音频码率控制间隔";
      case MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER:
         return "音频重采样驱动";
      case MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS:
         return "音频";
      case MENU_ENUM_LABEL_VALUE_AUDIO_SYNC:
         return "启用音频同步";
      case MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME:
         return "音频音量级别(dB)";
      case MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL:
         return "SaveRAM自动保存间隔";
      case MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE:
         return "自动加载覆写文件";
      case MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE:
         return "自动加载重映射文件";
      case MENU_ENUM_LABEL_VALUE_AUTO_SHADERS_ENABLE:
         return "自动加载Shader预设";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK:
         return "返回";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM:
         return "确认";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_INFO:
         return "信息";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT:
         return "退出";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN:
         return "向下滚动";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP:
         return "向上滚动";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_START:
         return "开始";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD:
         return "切换键盘";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU:
         return "切换菜单";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS:
         return "基本菜单控制";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_CONFIRM:
         return "确认";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_INFO:
         return "信息";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_QUIT:
         return "退出";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_SCROLL_UP:
         return "向上滚动";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_START:
         return "默认值";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_KEYBOARD:
         return "切换键盘";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_MENU:
         return "切换菜单";
      case MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE:
         return "加载保存状态时不覆盖SaveRAM";
      case MENU_ENUM_LABEL_VALUE_BLUETOOTH_ENABLE:
         return "启用蓝牙服务";
      case MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL:
         return "构建机器人资源URL";
      case MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY:
         return "缓存目录";
      case MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW:
         return "允许使用相机";
      case MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER:
         return "相机驱动";
      case MENU_ENUM_LABEL_VALUE_CHEAT:
         return "金手指";
      case MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES:
         return "应用金手指修改";
      case MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH:
         return "金手指文件目录";
      case MENU_ENUM_LABEL_VALUE_CHEAT_FILE:
         return "金手指文件";
      case MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD:
         return "加载金手指文件";
      case MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS:
         return "另存为金手指文件";
      case MENU_ENUM_LABEL_VALUE_CHEAT_NUM_PASSES:
         return "Cheat Passes";
      case MENU_ENUM_LABEL_VALUE_CHEEVOS_DESCRIPTION:
         return "描述";
      case MENU_ENUM_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE:
/* FIXME? Translate 'Achievements Hardcore Mode' */
         return "专家模式";
      case MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ACHIEVEMENTS:
         return "已解锁的成就:";
      case MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY:
         return "锁定";
      case MENU_ENUM_LABEL_VALUE_CHEEVOS_SETTINGS:
         return "Retro 成就";
      case MENU_ENUM_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL:
/* FIXME? Translate 'Test Unofficial Achievements' */
         return "非官方测试";
      case MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ACHIEVEMENTS:
         return "未解锁的成就:";
      case MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY:
         return "未锁定";
      case MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT:
         return "关闭";
      case MENU_ENUM_LABEL_VALUE_CONFIG:
         return "配置";
      case MENU_ENUM_LABEL_VALUE_CONFIGURATIONS:
         return "加载配置";
      case MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS:
         return "配置";
      case MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT:
         return "退出时保存配置";
      case MENU_ENUM_LABEL_VALUE_CONFIRM_ON_EXIT:
         return "退出时进行询问";
      case MENU_ENUM_LABEL_VALUE_CONTENT_COLLECTION_LIST:
         return "收藏";
      case MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY:
         return "游戏内容数据库目录";
      case MENU_ENUM_LABEL_VALUE_CONTENT_DIR:
         return "游戏内容目录";
      case MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE:
         return "历史记录数量";
      case MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS:
         return "快捷菜单";
      case MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIR:
         return "核心资源目录";
      case MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY:
         return "下载目录";
      case MENU_ENUM_LABEL_VALUE_CORE_CHEAT_OPTIONS:
         return "金手指";
      case MENU_ENUM_LABEL_VALUE_CORE_COUNTERS:
         return "核心计数器";
      case MENU_ENUM_LABEL_VALUE_CORE_ENABLE:
         return "显示核心名称";
      case MENU_ENUM_LABEL_VALUE_CORE_INFORMATION:
         return "核心信息";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS:
         return "作者";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES:
         return "分类";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL:
         return "核心标签";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME:
         return "核心名称";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE:
         return "固件";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES:
         return "许可证";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS:
         return "许可";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS:
         return "支持的扩展";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER:
         return "系统制造商";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME:
         return "系统名称";
      case MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS:
         return "控制";
      case MENU_ENUM_LABEL_VALUE_CORE_LIST:
         return "加载核心";
      case MENU_ENUM_LABEL_VALUE_CORE_OPTIONS:
         return "选项";
      case MENU_ENUM_LABEL_VALUE_CORE_SETTINGS:
         return "核心";
      case MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE:
         return "自动启动一个核心";
      case MENU_ENUM_LABEL_VALUE_CORE_SPECIFIC_CONFIG:
         return "单核心配置";
      case MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE:
         return "自动解压下载的档案";
      case MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL:
         return "构建机器人核心URL";
      case MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST:
         return "核心更新程序";
      case MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS:
         return "更新程序";
      case MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE:
         return "CPU架构:";
      case MENU_ENUM_LABEL_VALUE_CPU_CORES:
         return "CPU核心:";
      case MENU_ENUM_LABEL_VALUE_CURSOR_DIRECTORY:
         return "指针目录";
      case MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER:
         return "光标管理器";
      case MENU_ENUM_LABEL_VALUE_CUSTOM_RATIO:
         return "自定义比率";
      case MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER:
         return "数据库管理器";
      case MENU_ENUM_LABEL_VALUE_DATABASE_SELECTION:
         return "选择数据库";
      case MENU_ENUM_LABEL_VALUE_DELETE_ENTRY:
         return "移除";
      case MENU_ENUM_LABEL_VALUE_FAVORITES:
         return "选择文件并探测核心"; /* TODO/FIXME - update */
      case MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT:
         return "<游戏内容目录>";
      case MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT:
         return "<默认>";
      case MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE:
         return "<无>";
      case MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND:
         return "没有找到文件夹。";
      case MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS:
         return "目录";
      case MENU_ENUM_LABEL_VALUE_DISABLED:
         return "禁用";
      case MENU_ENUM_LABEL_VALUE_DISK_CYCLE_TRAY_STATUS:
         return "Disk Cycle Tray Status";
      case MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND:
         return "追加光盘镜像";
      case MENU_ENUM_LABEL_VALUE_DISK_INDEX:
         return "光盘索引";
      case MENU_ENUM_LABEL_VALUE_DISK_OPTIONS:
         return "光盘控制";
      case MENU_ENUM_LABEL_VALUE_DONT_CARE:
         return "不关心";
      case MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST:
         return "下载目录";
      case MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE:
         return "下载核心……";
      case MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT:
         return "下载游戏内容";
      case MENU_ENUM_LABEL_VALUE_DPI_OVERRIDE_ENABLE:
         return "启用DPI覆盖";
      case MENU_ENUM_LABEL_VALUE_DPI_OVERRIDE_VALUE:
         return "DPI覆盖";
      case MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS:
         return "驱动";
      case MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN:
/* FIXME? Translate 'Load Dummy on Core Shutdown' */
         return "Load Dummy on Core Shutdown";
      case MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER:
         return "动态壁纸";
      case MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY:
         return "动态壁纸目录";
      case MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE:
/* FIXME? Translate 'Enable Achievements' */
         return "启用";
      case MENU_ENUM_LABEL_VALUE_ENTRY_HOVER_COLOR:
         return "菜单项悬停颜色";
      case MENU_ENUM_LABEL_VALUE_ENTRY_NORMAL_COLOR:
         return "菜单项正常颜色";
      case MENU_ENUM_LABEL_VALUE_FALSE:
         return "假";
      case MENU_ENUM_LABEL_VALUE_FASTFORWARD_RATIO:
         return "最大运行速度";
      case MENU_ENUM_LABEL_VALUE_FPS_SHOW:
         return "显示帧率";
      case MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_ENABLE:
         return "限制最大运行速度";
      case MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS:
         return "帧率限制";
      case MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS:
         return "前端计数器";
      case MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS:
         return "自动加载游戏内容特定的核心选项";
      case MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE:
         return "创建游戏选项文件";
      case MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_IN_USE:
         return "游戏选项文件";
      case MENU_ENUM_LABEL_VALUE_HELP:
         return "帮助";
      case MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING:
         return "音频/视频故障排除";
      case MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD:
         return "变更虚拟游戏控制器覆层";
      case MENU_ENUM_LABEL_VALUE_HELP_CONTROLS:
         return "基本菜单控制";
      case MENU_ENUM_LABEL_VALUE_HELP_LIST:
         return "帮助";
      case MENU_ENUM_LABEL_VALUE_HELP_LOADING_CONTENT:
         return "加载游戏内容";
      case MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT:
         return "扫描游戏内容";
      case MENU_ENUM_LABEL_VALUE_HELP_WHAT_IS_A_CORE:
         return "什么是“核心”？";
      case MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE:
         return "启用历史记录";
      case MENU_ENUM_LABEL_VALUE_HISTORY_TAB:
         return "历史";
      case MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU:
         return "水平化菜单";
      case MENU_ENUM_LABEL_VALUE_IMAGES_TAB:
         return "图像";
      case MENU_ENUM_LABEL_VALUE_INFORMATION:
         return "信息";
      case MENU_ENUM_LABEL_VALUE_INFORMATION_LIST:
         return "信息";
      case MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE:
         return "手柄输入转数字选项";
      case MENU_ENUM_LABEL_VALUE_INPUT_ALL_USERS_CONTROL_MENU:
         return "所有用户都能控制菜单";
      case MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X:
         return "左摇杆X";
      case MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_MINUS:
         return "左摇杆X- (左)";
      case MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_PLUS:
         return "左摇杆X+ (右)";
      case MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y:
         return "左摇杆Y";
      case MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_MINUS:
         return "左摇杆Y- (上)";
      case MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_PLUS:
         return "左摇杆Y+ (下)";
      case MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X:
         return "右摇杆X";
      case MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_MINUS:
         return "右摇杆X- (左)";
      case MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_PLUS:
         return "右摇杆X+ (右)";
      case MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y:
         return "右摇杆Y";
      case MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_MINUS:
         return "右摇杆Y- (上)";
      case MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_PLUS:
         return "右摇杆Y+ (下)";
      case MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE:
         return "启用自动配置";
      case MENU_ENUM_LABEL_VALUE_INPUT_AXIS_THRESHOLD:
         return "输入轴阈值";
      case MENU_ENUM_LABEL_VALUE_INPUT_BIND_ALL:
         return "绑定全部";
      case MENU_ENUM_LABEL_VALUE_INPUT_BIND_DEFAULT_ALL:
         return "绑定全部至默认值";
      case MENU_ENUM_LABEL_VALUE_INPUT_BIND_TIMEOUT:
         return "绑定超时时间";
      case MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND:
         return "隐藏未绑定的核心输入描述";
      case MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW:
         return "显示输入描述标签";
      case MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_INDEX:
         return "设备索引";
      case MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_TYPE:
         return "设备类型";
      case MENU_ENUM_LABEL_VALUE_INPUT_DRIVER:
         return "输入驱动";
      case MENU_ENUM_LABEL_VALUE_INPUT_DUTY_CYCLE:
         return "Turbo占空比";
      case MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS:
         return "输入热键绑定";
      case MENU_ENUM_LABEL_VALUE_INPUT_ICADE_ENABLE:
         return "键盘控制器映射启用";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_A:
         return "A键(右侧)";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_B:
         return "B键(下方)";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_DOWN:
         return "下十字键";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L2:
         return "L2键(触发)";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L3:
         return "L3键(拇指)";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L:
         return "L键(手柄肩部)";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_LEFT:
         return "左十字键";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R2:
         return "R2键(触发)";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R3:
         return "R3键(拇指)";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R:
         return "R键(手柄肩部)";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_RIGHT:
         return "右十字键";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_SELECT:
         return "选择键";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_START:
         return "开始键";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_UP:
         return "上十字键";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_X:
         return "X键(上方)";
      case MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_Y:
         return "Y键(左侧)";
      case MENU_ENUM_LABEL_VALUE_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE:
         return "键盘控制器映射类型";
      case MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS:
         return "最大用户数";
      case MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO:
         return "游戏控制器菜单切出组合键";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_MINUS:
         return "金手指索引 -";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_PLUS:
         return "金手指索引 +";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_TOGGLE:
         return "金手指开关";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE:
         return "光驱出仓切换";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_NEXT:
         return "下一张光盘";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_PREV:
         return "上一张光盘";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_ENABLE_HOTKEY:
         return "启用热键";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_HOLD_KEY:
         return "快进保持";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_KEY:
         return "快进切换";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_FRAMEADVANCE:
         return "帧提前量";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY:
         return "切换全屏幕";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_GRAB_MOUSE_TOGGLE:
         return "鼠标捕获开关";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_LOAD_STATE_KEY:
         return "加载状态";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE:
         return "切换菜单";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_MOVIE_RECORD_TOGGLE:
         return "视频录制开关";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_MUTE:
         return "静音开关";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_FLIP:
         return "Netplay flip users";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_OSK:
         return "切换屏幕键盘";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_OVERLAY_NEXT:
         return "下一个覆层";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_PAUSE_TOGGLE:
         return "切换暂停";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY:
         return "退出 RetroArch";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_RESET:
         return "重置游戏";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_REWIND:
         return "回溯";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_STATE_KEY:
         return "保存状态";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT:
         return "屏幕截图";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_NEXT:
         return "下一个Shader";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_PREV:
         return "上一个Shader";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION:
         return "慢动作";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_MINUS:
         return "存档槽 -";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_PLUS:
         return "存档槽 +";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_DOWN:
         return "音量 -";
      case MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_UP:
         return "音量 +";
      case MENU_ENUM_LABEL_VALUE_INPUT_OSK_OVERLAY_ENABLE:
         return "显示键盘覆层";
      case MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE:
         return "显示覆层";
      case MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU:
         return "在菜单中隐藏覆层";
      case MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR:
         return "轮询类型行为";
      case MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_EARLY:
         return "较早";
      case MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_LATE:
         return "稍晚";
      case MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_NORMAL:
         return "正常";
      case MENU_ENUM_LABEL_VALUE_INPUT_PREFER_FRONT_TOUCH:
         return "优先前置触摸";
      case MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY:
         return "输入重映射目录";
      case MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE:
         return "启用绑定重映射";
      case MENU_ENUM_LABEL_VALUE_INPUT_SAVE_AUTOCONFIG:
         return "保存自动设置";
      case MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS:
         return "输入";
      case MENU_ENUM_LABEL_VALUE_INPUT_SMALL_KEYBOARD_ENABLE:
         return "启用小键盘";
      case MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_ENABLE:
         return "启用触摸";
      case MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ENABLE:
         return "TURBO开关";
      case MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD:
         return "Turbo区间";
      case MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS:
         return "输入用户 %u 的绑定";
      case MENU_ENUM_LABEL_VALUE_INTERNAL_STORAGE_STATUS:
         return "内部存储状态";
      case MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR:
         return "输入设备自动配置目录";
      case MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER:
         return "手柄驱动";
      case MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES:
         return "Lakka 服务";
      case MENU_ENUM_LABEL_VALUE_LANG_CHINESE_SIMPLIFIED:
         return "简体中文";
      case MENU_ENUM_LABEL_VALUE_LANG_CHINESE_TRADITIONAL:
         return "繁体中文";
      case MENU_ENUM_LABEL_VALUE_LANG_DUTCH:
         return "荷兰语";
      case MENU_ENUM_LABEL_VALUE_LANG_ENGLISH:
         return "英语";
      case MENU_ENUM_LABEL_VALUE_LANG_ESPERANTO:
         return "世界语";
      case MENU_ENUM_LABEL_VALUE_LANG_FRENCH:
         return "法语";
      case MENU_ENUM_LABEL_VALUE_LANG_GERMAN:
         return "德语";
      case MENU_ENUM_LABEL_VALUE_LANG_ITALIAN:
         return "意大利语";
      case MENU_ENUM_LABEL_VALUE_LANG_JAPANESE:
         return "日语";
      case MENU_ENUM_LABEL_VALUE_LANG_KOREAN:
         return "韩语";
      case MENU_ENUM_LABEL_VALUE_LANG_POLISH:
         return "波兰语";
      case MENU_ENUM_LABEL_VALUE_LANG_PORTUGUESE:
         return "葡萄牙语";
      case MENU_ENUM_LABEL_VALUE_LANG_RUSSIAN:
         return "俄语";
      case MENU_ENUM_LABEL_VALUE_LANG_SPANISH:
         return "西班牙语";
      case MENU_ENUM_LABEL_VALUE_LANG_VIETNAMESE:
         return "越南语";
      case MENU_ENUM_LABEL_VALUE_LEFT_ANALOG:
         return "左侧摇杆";
      case MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH:
         return "核心目录";
      case MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH:
         return "核心信息目录";
      case MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL:
         return "核心日志级别";
      case MENU_ENUM_LABEL_VALUE_LINEAR:
         return "线性";
      case MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE:
         return "使用核心加载压缩包";
      case MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY:
         return "加载最近的游戏内容";
      case MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST:
         return "载入游戏内容";
      case MENU_ENUM_LABEL_VALUE_LOAD_STATE:
         return "加载状态";
      case MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW:
         return "允许使用位置";
      case MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER:
         return "定位驱动";
      case MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS:
         return "日志";
      case MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY:
         return "完整日志记录";
      case MENU_ENUM_LABEL_VALUE_MAIN_MENU:
         return "主菜单";
      case MENU_ENUM_LABEL_VALUE_MANAGEMENT:
         return "数据库设置";
      case MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME:
         return "菜单颜色主题";
      case MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE:
         return "蓝色";
      case MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE_GREY:
         return "蓝灰色";
      case MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_DARK_BLUE:
         return "深蓝色";
      case MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GREEN:
         return "绿色";
      case MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NVIDIA_SHIELD:
         return "NV SHIELD";
      case MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_RED:
         return "红色";
      case MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_YELLOW:
         return "黄色";
      case MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_FOOTER_OPACITY:
         return "底部不透明度";
      case MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_HEADER_OPACITY:
         return "顶部不透明度";
      case MENU_ENUM_LABEL_VALUE_MENU_DRIVER:
         return "菜单驱动";
      case MENU_ENUM_LABEL_VALUE_MENU_ENUM_THROTTLE_FRAMERATE:
         return "限制菜单帧率";
      case MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS:
         return "菜单文件浏览器";
      case MENU_ENUM_LABEL_VALUE_MENU_LINEAR_FILTER:
         return "菜单线性过滤";
      case MENU_ENUM_LABEL_VALUE_MENU_SETTINGS:
         return "菜单";
      case MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER:
         return "菜单壁纸";
      case MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER_OPACITY:
         return "壁纸不透明度";
      case MENU_ENUM_LABEL_VALUE_MISSING:
         return "丢失";
      case MENU_ENUM_LABEL_VALUE_MORE:
         return "...";
      case MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE:
         return "鼠标支持";
      case MENU_ENUM_LABEL_VALUE_MULTIMEDIA_SETTINGS:
         return "多媒体";
      case MENU_ENUM_LABEL_VALUE_MUSIC_TAB:
         return "音乐";
      case MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
         return "过滤未知扩展名";
      case MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND:
         return "环绕式导航";
      case MENU_ENUM_LABEL_VALUE_NEAREST:
         return "最近";
      case MENU_ENUM_LABEL_VALUE_NETPLAY:
         return "在线游戏";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_CHECK_FRAMES:
         return "在线游戏检查帧数";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_CLIENT_SWAP_INPUT:
         return "在线玩家P2使用C1";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_DELAY_FRAMES:
         return "在线游戏延迟帧数";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_DISCONNECT:
         return "断开连接";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE:
         return "启用在线游戏";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_CLIENT:
         return "连接到游戏主机";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_HOST:
         return "作为游戏主机";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS:
         return "服务器地址";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_MODE:
         return "启用在线游戏客户端";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME:
         return "用户名";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_SETTINGS:
         return "在线游戏设置";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE:
         return "启用在线游戏旁观者";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT:
         return "在线游戏TCP/UDP端口";
      case MENU_ENUM_LABEL_VALUE_NETWORK_CMD_ENABLE:
         return "网络命令";
      case MENU_ENUM_LABEL_VALUE_NETWORK_CMD_PORT:
         return "网络命令端口";
      case MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION:
         return "网络信息";
      case MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_ENABLE:
         return "网络游戏控制器";
      case MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_PORT:
         return "网络远端基本端口";
      case MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS:
         return "网络";
      case MENU_ENUM_LABEL_VALUE_NO:
         return "否";
      case MENU_ENUM_LABEL_VALUE_NONE:
         return "无";
      case MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE:
         return "N/A";
      case MENU_ENUM_LABEL_VALUE_NO_ACHIEVEMENTS_TO_DISPLAY:
         return "没有可显示的成就。";
      case MENU_ENUM_LABEL_VALUE_NO_CORE:
         return "没有核心";
      case MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE:
         return "没有可用的核心。";
      case MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE:
         return "没有可用的核心信息。";
      case MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE:
         return "没有可用的核心选项。";
      case MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY:
         return "没有可显示的条目。";
      case MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE:
         return "没有可用的历史记录。";
      case MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE:
         return "没有可用的信息。";
      case MENU_ENUM_LABEL_VALUE_NO_ITEMS:
         return "没有条目。";
      case MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS:
         return "没有性能计数器。";
      case MENU_ENUM_LABEL_VALUE_NO_PLAYLISTS:
         return "没有游戏列表。";
      case MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE:
         return "没有可用的游戏列表项目。";
      case MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND:
         return "没有找到设置。";
      case MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS:
         return "没有Shader参数.";
      case MENU_ENUM_LABEL_VALUE_OFF:
         return "关";
      case MENU_ENUM_LABEL_VALUE_ON:
         return "开";
      case MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER:
         return "在线更新器";
      case MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS:
         return "屏幕显示";
      case MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS:
         return "屏幕覆层";
      case MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE:
         return "以文件夹形式打开压缩包";
      case MENU_ENUM_LABEL_VALUE_OPTIONAL:
         return "任意";
      case MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_DIRECTORY:
         return "OSK覆层目录";
      case MENU_ENUM_LABEL_VALUE_OVERLAY:
         return "覆层";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED:
         return "自动加载最佳的覆层";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY:
         return "覆层目录";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY:
         return "覆层不透明度";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET:
         return "覆层预设";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE:
         return "覆层缩放比例";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS:
         return "屏幕覆层";
      case MENU_ENUM_LABEL_VALUE_PAL60_ENABLE:
         return "使用PAL60模式";
      case MENU_ENUM_LABEL_VALUE_PARENT_DIRECTORY:
         return "上一级目录";
      case MENU_ENUM_LABEL_VALUE_PAUSE_LIBRETRO:
         return "当菜单激活时暂停";
      case MENU_ENUM_LABEL_VALUE_PAUSE_NONACTIVE:
         return "禁止后台运行";
      case MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE:
         return "性能计数器";
      case MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB:
         return "游戏列表";
      case MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY:
         return "游戏列表目录";
      case MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS:
         return "游戏列表";
      case MENU_ENUM_LABEL_VALUE_POINTER_ENABLE:
         return "触摸支持";
      case MENU_ENUM_LABEL_VALUE_PORT:
         return "端口";
      case MENU_ENUM_LABEL_VALUE_PRESENT:
         return "现在";
      case MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS:
         return "隐私";
      case MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH:
         return "退出 RetroArch";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ANALOG:
         return "支持摇杆输入";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_BBFC_RATING:
         return "BBFC 分级";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CERO_RATING:
         return "CERO 分级";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_COOP:
         return "多人游戏支持";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CRC32:
         return "CRC32";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION:
         return "描述";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER:
         return "开发者";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_ISSUE:
         return "Edge Magazine Issue";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_RATING:
         return "Edge Magazine Rating";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_REVIEW:
         return "Edge Magazine Review";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ELSPA_RATING:
         return "ELSPA 分级";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ENHANCEMENT_HW:
         return "增强硬件";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ESRB_RATING:
         return "ESRB 分级";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FAMITSU_MAGAZINE_RATING:
         return "Famitsu Magazine Rating";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FRANCHISE:
         return "经销商";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GENRE:
         return "类型";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_MD5:
         return "MD5";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME:
         return "名称";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN:
         return "起源";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PEGI_RATING:
         return "PEGI 分级";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER:
         return "出版方";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH:
         return "发售月份";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR:
         return "发售年份";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RUMBLE:
         return "震动支持";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SERIAL:
         return "系列";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SHA1:
         return "SHA1";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_START_CONTENT:
         return "启动游戏内容";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_TGDB_RATING:
         return "TGDB Rating";
      case MENU_ENUM_LABEL_VALUE_REBOOT:
         return "重启";
      case MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY:
         return "录像配置目录";
      case MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY:
         return "录像输出目录";
      case MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS:
         return "录像";
      case MENU_ENUM_LABEL_VALUE_RECORD_CONFIG:
         return "录像配置";
      case MENU_ENUM_LABEL_VALUE_RECORD_DRIVER:
         return "录像驱动";
      case MENU_ENUM_LABEL_VALUE_RECORD_ENABLE:
         return "启用录像";
      case MENU_ENUM_LABEL_VALUE_RECORD_PATH:
         return "输出文件";
      case MENU_ENUM_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY:
         return "使用输出目录";
      case MENU_ENUM_LABEL_VALUE_REMAP_FILE:
         return "重映射文件";
      case MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD:
         return "加载重映射文件";
      case MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE:
         return "保存核心重映射文件";
      case MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME:
         return "保存游戏重映射文件";
      case MENU_ENUM_LABEL_VALUE_REQUIRED:
         return "必须";
      case MENU_ENUM_LABEL_VALUE_RESTART_CONTENT:
         return "重启";
      case MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH:
         return "重启 RetroArch";
      case MENU_ENUM_LABEL_VALUE_RESUME:
         return "继续";
      case MENU_ENUM_LABEL_VALUE_RESUME_CONTENT:
         return "继续";
      case MENU_ENUM_LABEL_VALUE_RETROKEYBOARD:
         return "RetroKeyboard";
      case MENU_ENUM_LABEL_VALUE_RETROPAD:
         return "RetroPad";
      case MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS:
         return "Retro 成就";
      case MENU_ENUM_LABEL_VALUE_REWIND_ENABLE:
         return "启用回溯";
      case MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY:
         return "回溯粒度";
      case MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS:
         return "回溯";
      case MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY:
         return "文件浏览器目录";
      case MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY:
         return "配置目录";
      case MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN:
         return "显示开始屏幕";
      case MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG:
         return "右侧摇杆";
      case MENU_ENUM_LABEL_VALUE_RUN:
         return "运行";
      case MENU_ENUM_LABEL_VALUE_SAMBA_ENABLE:
         return "启用SAMBA文件共享服务";
      case MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY:
         return "存档文件目录";
      case MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX:
         return "自动索引保存状态";
      case MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD:
         return "自动加载状态";
      case MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE:
         return "自动保存状态";
      case MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY:
         return "状态存储目录";
      case MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG:
         return "保存当前配置";
      case MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CORE:
         return "保存核心覆写";
      case MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME:
         return "保存游戏覆写";
      case MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG:
         return "保存新配置";
      case MENU_ENUM_LABEL_VALUE_SAVE_STATE:
         return "保存状态";
      case MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS:
         return "存档";
      case MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY:
         return "扫描文件夹";
      case MENU_ENUM_LABEL_VALUE_SCAN_FILE:
         return "扫描文件";
      case MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY:
         return "<扫描当前目录>";
      case MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY:
         return "屏幕截图目录";
      case MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION:
         return "屏幕分辨率";
      case MENU_ENUM_LABEL_VALUE_SEARCH:
         return "搜索：";
      case MENU_ENUM_LABEL_VALUE_SECONDS:
         return "秒";
      case MENU_ENUM_LABEL_VALUE_SETTINGS:
         return "设置";
      case MENU_ENUM_LABEL_VALUE_SETTINGS_TAB:
         return "设置";
      case MENU_ENUM_LABEL_VALUE_SHADER:
         return "Shader";
      case MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES:
         return "应用Shader修改";
      case MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS:
         return "Shader效果";
      case MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON:
         return "Ribbon";
      case MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON_SIMPLIFIED:
         return "Ribbon (简化)";
      case MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS:
         return "显示高级设置";
      case MENU_ENUM_LABEL_VALUE_SHOW_HIDDEN_FILES:
         return "显示隐藏的文件和文件夹";
      case MENU_ENUM_LABEL_VALUE_SHUTDOWN:
         return "关机";
      case MENU_ENUM_LABEL_VALUE_SLOWMOTION_RATIO:
         return "慢动作倍率";
      case MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE:
         return "排序文件夹中的存档";
      case MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE:
         return "排序文件夹中的状态存储";
      case MENU_ENUM_LABEL_VALUE_SSH_ENABLE:
         return "启用SSH远程终端服务";
      case MENU_ENUM_LABEL_VALUE_START_CORE:
         return "启动核心";
      case MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD:
         return "启动远程的RetroPad";
      case MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR:
         return "启动视频处理";
      case MENU_ENUM_LABEL_VALUE_STATE_SLOT:
         return "状态存储槽";
      case MENU_ENUM_LABEL_VALUE_STATUS:
         return "状态";
      case MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE:
         return "标准输入流命令";
      case MENU_ENUM_LABEL_VALUE_SUPPORTED_CORES:
         return "支持的核心";
      case MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE:
         return "暂停屏保程序";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_BGM_ENABLE:
         return "启用系统背景音乐";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY:
         return "系统/BIOS目录";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION:
         return "系统信息";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT:
         return "7zip 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT:
         return "ALSA 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE:
         return "编译日期";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT:
         return "Cg 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT:
         return "Cocoa 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT:
         return "控制台支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT:
         return "CoreText 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES:
         return "CPU特性";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI:
         return "显示器度量DPI";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT:
         return "显示器度量高度(mm)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH:
         return "显示器度量宽度(mm)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT:
         return "DirectSound 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT:
         return "动态链接库支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYNAMIC_SUPPORT:
         return "运行时动态加载libretro库";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT:
         return "EGL 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT:
         return "OpenGL/Direct3D 渲染至纹理 (多渲染批次Shader) 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT:
         return "FFmpeg 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT:
         return "FreeType 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER:
         return "前端标识";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME:
         return "前端名称";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS:
         return "前端操作系统";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION:
         return "Git版本";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT:
         return "GLSL 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT:
         return "HLSL 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT:
         return "JACK 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT:
         return "KMS/EGL 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT:
         return "LibretroDB 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT:
         return "Libusb 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBXML2_SUPPORT:
         return "libxml2 XML解析支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT:
         return "Netplay (点对点) 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT:
         return "网络控制台支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_REMOTE_SUPPORT:
         return "网络控制器支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT:
         return "OpenAL 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT:
         return "OpenGL ES 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT:
         return "OpenGL 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT:
         return "OpenSL 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT:
         return "OpenVG 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT:
         return "OSS 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT:
         return "覆层支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE:
         return "电源";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED:
         return "已充满电";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING:
         return "充电中";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING:
         return "放电中";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE:
         return "没有电源";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT:
         return "PulseAudio 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PYTHON_SUPPORT:
         return "Python (Shader中脚本支持) 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RBMP_SUPPORT:
         return "BMP 支持 (RBMP)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL:
         return "RetroRating 等级";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RJPEG_SUPPORT:
         return "JPEG 支持 (RJPEG)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT:
         return "RoarAudio 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT:
         return "PNG 支持 (RPNG)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT:
         return "RSound 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RTGA_SUPPORT:
         return "TGA 支持 (RTGA)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT:
         return "SDL2 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT:
         return "SDL 图像支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT:
         return "SDL1.2 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SLANG_SUPPORT:
         return "Slang 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT:
         return "多线程支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT:
         return "Udev 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT:
         return "Video4Linux2 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER:
         return "视频上下文驱动";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VULKAN_SUPPORT:
         return "Vulkan 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT:
         return "Wayland 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT:
         return "X11 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT:
         return "XAudio2 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT:
         return "XVideo 支持";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT:
         return "Zlib 支持";
      case MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT:
         return "截取屏幕";
      case MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE:
         return "启用多线程数据执行循环";
      case MENU_ENUM_LABEL_VALUE_THUMBNAILS:
         return "缩略图";
      case MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY:
         return "缩略图目录";
      case MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST:
         return "缩略图更新程序";
      case MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_BOXARTS:
         return "Boxarts";
      case MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_SCREENSHOTS:
         return "截屏";
      case MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_TITLE_SCREENS:
         return "标题画面";
      case MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE:
         return "显示时间日期";
      case MENU_ENUM_LABEL_VALUE_TITLE_COLOR:
         return "菜单标题颜色";
      case MENU_ENUM_LABEL_VALUE_TRUE:
         return "真";
      case MENU_ENUM_LABEL_VALUE_UI_COMPANION_ENABLE:
         return "UI Companion Enable";
      case MENU_ENUM_LABEL_VALUE_UI_COMPANION_START_ON_BOOT:
         return "UI Companion Start On Boot";
      case MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE:
         return "Menubar";
      case MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE:
         return "无法读取压缩的文件。";
      case MENU_ENUM_LABEL_VALUE_UNDO_LOAD_STATE:
         return "撤销加载状态";
      case MENU_ENUM_LABEL_VALUE_UNDO_SAVE_STATE:
         return "撤销保存状态";
      case MENU_ENUM_LABEL_VALUE_UNKNOWN:
         return "未知";
      case MENU_ENUM_LABEL_VALUE_UPDATER_SETTINGS:
         return "更新程序";
      case MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS:
         return "更新资源";
      case MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES:
         return "更新自动配置档案";
      case MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS:
         return "更新CG Shader效果文件";
      case MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS:
         return "更新金手指";
      case MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES:
         return "更新核心信息文件";
      case MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES:
         return "更新数据库";
      case MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS:
         return "更新GLSL Shader效果文件";
      case MENU_ENUM_LABEL_VALUE_UPDATE_LAKKA:
         return "更新 Lakka";
      case MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS:
         return "更新覆层";
      case MENU_ENUM_LABEL_VALUE_UPDATE_SLANG_SHADERS:
         return "更新Slang Shader效果文件";
      case MENU_ENUM_LABEL_VALUE_USER:
         return "用户";
      case MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS:
         return "用户界面";
      case MENU_ENUM_LABEL_VALUE_USER_LANGUAGE:
         return "语言";
      case MENU_ENUM_LABEL_VALUE_USER_SETTINGS:
         return "用户";
      case MENU_ENUM_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER:
         return "使用内建的图像浏览器";
      case MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER:
         return "使用内建媒体播放器";
      case MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY:
         return "<使用当前目录>";
      case MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE:
         return "允许旋转";
      case MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO:
         return "自动选择视口比例";
      case MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX:
         return "视口比例选项";
      case MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION:
         return "黑色帧补间";
      case MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN:
         return "Crop Overscan (Reload)";
      case MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION:
         return "禁用桌面元素";
      case MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER:
         return "视频驱动";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FILTER:
         return "视频滤镜";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR:
         return "视频滤镜目录";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_FLICKER:
         return "闪烁过滤器";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE:
         return "显示屏显消息(OSD)";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH:
         return "屏显消息(OSD)字体";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE:
         return "屏显消息(OSD)大小";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_ASPECT:
         return "强制视口比例";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE:
         return "强制禁止sRGB帧缓冲";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY:
         return "帧延时";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN:
         return "使用全屏模式";
      case MENU_ENUM_LABEL_VALUE_VIDEO_GAMMA:
         return "视频Gamma";
      case MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD:
         return "启用GPU录像";
      case MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT:
         return "启用GPU截屏";
      case MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC:
         return "强制GPU同步";
      case MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES:
         return "强制GPU同步帧数";
      case MENU_ENUM_LABEL_VALUE_VIDEO_MAX_SWAPCHAIN_IMAGES:
         return "最大交换链图像数";
      case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X:
         return "屏显消息(OSD)X轴位置";
      case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y:
         return "屏显消息(OSD)Y轴位置";
      case MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX:
         return "显示器索引";
      case MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD:
         return "启用录像后期滤镜";
      case MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE:
         return "刷新率";
      case MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO:
         return "估算的显示器帧率";
      case MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION:
         return "旋转";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SCALE:
         return "窗口缩放量";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER:
         return "整数化缩放量";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS:
         return "视频";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR:
         return "视频Shader目录";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES:
         return "Shader渲染遍数";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS:
         return "预览Shader参数";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET:
         return "加载Shader预设";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_PARAMETERS:
         return "菜单Shader参数";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS:
         return "保存Shader预设为";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CORE:
         return "保存核心预设";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME:
         return "保存游戏预设";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT:
         return "启用硬件共享上下文";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH:
         return "硬件双线性过滤";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER:
         return "启用软件过滤器";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL:
         return "垂直同步交换间隔";
      case MENU_ENUM_LABEL_VALUE_VIDEO_TAB:
         return "视频";
      case MENU_ENUM_LABEL_VALUE_VIDEO_THREADED:
         return "多线程渲染";
      case MENU_ENUM_LABEL_VALUE_VIDEO_VFILTER:
         return "Deflicker";
      case MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_HEIGHT:
         return "自定义视口高度";
      case MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_WIDTH:
         return "自定义视口宽度";
      case MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_X:
         return "自定义视口X";
      case MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_Y:
         return "自定义视口Y";
      case MENU_ENUM_LABEL_VALUE_VIDEO_VI_WIDTH:
         return "Set VI Screen Width";
      case MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC:
         return "垂直同步";
      case MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN:
         return "无边框窗口全屏模式";
      case MENU_ENUM_LABEL_VALUE_WIFI_DRIVER:
         return "Wi-Fi驱动";
      case MENU_ENUM_LABEL_VALUE_WIFI_SETTINGS:
         return "Wi-Fi";
      case MENU_ENUM_LABEL_VALUE_XMB_ALPHA_FACTOR:
         return "菜单透明度因子";
      case MENU_ENUM_LABEL_VALUE_XMB_FONT:
         return "菜单字体";
      case MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_CUSTOM:
         return "自定义";
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
         return "菜单颜色主题";
      case MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_APPLE_GREEN:
         return "苹果绿";
      case MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK:
         return "深色";
      case MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK_PURPLE:
         return "深紫色";
      case MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ELECTRIC_BLUE:
         return "铁蓝色";
      case MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GOLDEN:
         return "金色";
      case MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LEGACY_RED:
         return "传统红";
      case MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDNIGHT_BLUE:
         return "蓝黑色";
      case MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PLAIN:
         return "朴素";
      case MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_UNDERSEA:
         return "海底";
      case MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_VOLCANIC_RED:
         return "火山红";
      case MENU_ENUM_LABEL_VALUE_XMB_RIBBON_ENABLE:
         return "菜单Shader管线";
      case MENU_ENUM_LABEL_VALUE_XMB_SCALE_FACTOR:
         return "菜单缩放因子";
      case MENU_ENUM_LABEL_VALUE_XMB_SHADOWS_ENABLE:
         return "启用图标阴影";
      case MENU_ENUM_LABEL_VALUE_XMB_SHOW_HISTORY:
         return "显示历史页";
      case MENU_ENUM_LABEL_VALUE_XMB_SHOW_IMAGES:
         return "显示图像页";
      case MENU_ENUM_LABEL_VALUE_XMB_SHOW_MUSIC:
         return "显示音乐页";
      case MENU_ENUM_LABEL_VALUE_XMB_SHOW_SETTINGS:
         return "显示设置页";
      case MENU_ENUM_LABEL_VALUE_XMB_SHOW_VIDEO:
         return "显示视频页";
      case MENU_ENUM_LABEL_VALUE_XMB_THEME:
         return "菜单图标主题";
      case MENU_ENUM_LABEL_VALUE_YES:
         return "是";
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_TWO:
         return "Shader预设";
      case MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST:
         return "下载且/或者扫描游戏内容，并将其加入你的收藏中。";
      case MENU_ENUM_SUBLABEL_AUDIO_SETTINGS:
         return "调整音频输出的选项。";
      case MENU_ENUM_SUBLABEL_BLUETOOTH_ENABLE:
         return "启用或者禁止蓝牙。";
      case MENU_ENUM_SUBLABEL_CONFIG_SAVE_ON_EXIT:
         return "程序将在退出时保存修改到配置文件。";
      case MENU_ENUM_SUBLABEL_CONFIGURATION_SETTINGS:
         return "Change default settings for configuration files.";
      case MENU_ENUM_SUBLABEL_CONFIGURATIONS_LIST:
         return "Manage and create configuration files.";
      case MENU_ENUM_SUBLABEL_CPU_CORES:
         return "CPU拥有的核心总数。";
      case MENU_ENUM_SUBLABEL_FPS_SHOW:
         return "在屏幕上显示当前每秒的帧率。";
      case MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS:
         return "配置热键选项。";
      case MENU_ENUM_SUBLABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO:
         return "控制器用来切出菜单的组合键。";
      case MENU_ENUM_SUBLABEL_INPUT_SETTINGS:
         return "调整游戏控制器、键盘和鼠标的设置。";
      case MENU_ENUM_SUBLABEL_INPUT_USER_BINDS:
         return "配置该用户的控制选项。";
      case MENU_ENUM_SUBLABEL_LOG_VERBOSITY:
         return "启用或禁止向控制台打印日志。";
      case MENU_ENUM_SUBLABEL_NETPLAY:
         return "加入或者开启一个在线多人游戏的会话。";
      case MENU_ENUM_SUBLABEL_ONLINE_UPDATER:
         return "下载并更新RetroArch的附加插件和组件。";
      case MENU_ENUM_SUBLABEL_SAMBA_ENABLE:
         return "启用或者禁止网络文件夹共享(SAMBA)。";
      case MENU_ENUM_SUBLABEL_SERVICES_SETTINGS:
         return "管理操作系统层级的服务。";
      case MENU_ENUM_SUBLABEL_SHOW_HIDDEN_FILES:
         return "在文件浏览器中显示隐藏的文件或文件夹。";
      case MENU_ENUM_SUBLABEL_SSH_ENABLE:
         return "启用或者禁止远程终端访问(SSH)。";
      case MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE:
         return "阻止系统激活屏幕保护程序。";
      case MENU_ENUM_SUBLABEL_USER_LANGUAGE:
         return "设置用户界面的语言。";
      case MENU_ENUM_SUBLABEL_VIDEO_BLACK_FRAME_INSERTION:
         return "在帧与帧之间插入黑色的中间帧，通常用于消除在120Hz刷新率的显示器上运行60Hz的游戏内容带来的鬼影。";
      case MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY:
         return "以增加画面卡顿的风险换取低延时，在垂直同步后增加时延(毫秒)。";
      case MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC_FRAMES:
         return "设置当开启“强制GPU同步”时CPU可以预先GPU多少帧。";
      case MENU_ENUM_SUBLABEL_VIDEO_MAX_SWAPCHAIN_IMAGES:
         return "强制显示驱动程序使用特定的缓冲模式。";
      case MENU_ENUM_SUBLABEL_VIDEO_MONITOR_INDEX:
         return "选择将要使用哪一个显示器。";
      case MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_AUTO:
         return "估算的显示器刷新率(Hz)。";
      case MENU_ENUM_SUBLABEL_VIDEO_SETTINGS:
         return "调整视频输出的选项。";
      case MENU_ENUM_SUBLABEL_WIFI_SETTINGS:
         return "扫描无线网络并且建立连接。";
      case MSG_APPENDED_DISK:
         return "Appended disk";
      case MSG_APPLICATION_DIR:
         return "应用程序目录";
      case MSG_APPLYING_SHADER:
         return "Applying shader";
      case MSG_AUDIO_MUTED:
         return "静音。";
      case MSG_AUDIO_UNMUTED:
         return "取消静音。";
      case MSG_AUTOCONFIG_FILE_ERROR_SAVING:
         return "Error saving autoconf file.";
      case MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY:
         return "Autoconfig file saved successfully.";
      case MSG_AUTOSAVE_FAILED:
         return "Could not initialize autosave.";
      case MSG_AUTO_SAVE_STATE_TO:
         return "自动保存状态至";
      case MSG_BLOCKING_SRAM_OVERWRITE:
         return "Blocking SRAM Overwrite";
      case MSG_BRINGING_UP_COMMAND_INTERFACE_ON_PORT:
         return "Bringing up command interface on port";
      case MSG_BYTES:
         return "bytes";
      case MSG_CANNOT_INFER_NEW_CONFIG_PATH:
         return "Cannot infer new config path. Use current time.";
      case MSG_CHEEVOS_HARDCORE_MODE_ENABLE:
         return "硬核模式开启：及时存档和回放被禁用.";
      case MSG_COMPARING_WITH_KNOWN_MAGIC_NUMBERS:
         return "Comparing with known magic numbers...";
      case MSG_COMPILED_AGAINST_API:
         return "Compiled against API";
      case MSG_CONFIG_DIRECTORY_NOT_SET:
         return "Config directory not set. Cannot save new config.";
      case MSG_CONNECTED_TO:
         return "连接至";
      case MSG_CONTENT_CRC32S_DIFFER:
         return "Content CRC32s differ. Cannot use different games.";
      case MSG_CONTENT_LOADING_SKIPPED_IMPLEMENTATION_WILL_DO_IT:
         return "Content loading skipped. Implementation will load it on its own.";
      case MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES:
         return "核心不支持保存状态。";
      case MSG_CORE_OPTIONS_FILE_CREATED_SUCCESSFULLY:
         return "Core options file created successfully.";
      case MSG_COULD_NOT_FIND_ANY_NEXT_DRIVER:
         return "Could not find any next driver";
      case MSG_COULD_NOT_FIND_COMPATIBLE_SYSTEM:
         return "Could not find compatible system.";
      case MSG_COULD_NOT_FIND_VALID_DATA_TRACK:
         return "无法找到有效的数据轨";
      case MSG_COULD_NOT_OPEN_DATA_TRACK:
         return "无法打开数据轨";
      case MSG_COULD_NOT_READ_CONTENT_FILE:
         return "无法读取内容文件";
      case MSG_COULD_NOT_READ_MOVIE_HEADER:
         return "无法读取视频头部信息.";
      case MSG_COULD_NOT_READ_STATE_FROM_MOVIE:
         return "无法读取视频状态.";
      case MSG_CRC32_CHECKSUM_MISMATCH:
         return "CRC32 checksum mismatch between content file and saved content checksum in replay file header; replay highly likely to desync on playback.";
      case MSG_CUSTOM_TIMING_GIVEN:
         return "Custom timing given";
      case MSG_DECOMPRESSION_ALREADY_IN_PROGRESS:
         return "解压缩已在进行中。";
      case MSG_DECOMPRESSION_FAILED:
         return "解压缩失败。";
      case MSG_DETECTED_VIEWPORT_OF:
         return "Detected viewport of";
      case MSG_DID_NOT_FIND_A_VALID_CONTENT_PATCH:
         return "Did not find a valid content patch.";
      case MSG_DISCONNECT_DEVICE_FROM_A_VALID_PORT:
         return "Disconnect device from a valid port.";
      case MSG_DISK_CLOSED:
         return "已关闭";
      case MSG_DISK_EJECTED:
         return "已弹出";
      case MSG_DOWNLOADING:
         return "正在下载";
      case MSG_DOWNLOAD_FAILED:
         return "下载失败";
      case MSG_ERROR:
         return "错误";
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
         return "外部应用程序目录";
      case MSG_EXTRACTING:
         return "正在解压";
      case MSG_EXTRACTING_FILE:
         return "解压文件";
      case MSG_FAILED_SAVING_CONFIG_TO:
         return "无法保存配置到";
      case MSG_FAILED_TO:
         return "Failed to";
      case MSG_FAILED_TO_ACCEPT_INCOMING_SPECTATOR:
         return "Failed to accept incoming spectator.";
      case MSG_FAILED_TO_ALLOCATE_MEMORY_FOR_PATCHED_CONTENT:
         return "Failed to allocate memory for patched content...";
      case MSG_FAILED_TO_APPLY_SHADER:
         return "Failed to apply shader.";
      case MSG_FAILED_TO_BIND_SOCKET:
         return "Failed to bind socket.";
      case MSG_FAILED_TO_CREATE_THE_DIRECTORY:
         return "创建目录失败。";
      case MSG_FAILED_TO_EXTRACT_CONTENT_FROM_COMPRESSED_FILE:
         return "从压缩文件中提取内容失败";
      case MSG_FAILED_TO_GET_NICKNAME_FROM_CLIENT:
         return "从客户端获取昵称失败.";
      case MSG_FAILED_TO_LOAD:
         return "无法加载";
      case MSG_FAILED_TO_LOAD_CONTENT:
         return "载入内容失败";
      case MSG_FAILED_TO_LOAD_MOVIE_FILE:
         return "载入视频文件失败";
      case MSG_FAILED_TO_LOAD_OVERLAY:
         return "Failed to load overlay.";
      case MSG_FAILED_TO_LOAD_STATE:
         return "Failed to load state from";
      case MSG_FAILED_TO_OPEN_LIBRETRO_CORE:
         return "打开libretro核心失败";
      case MSG_FAILED_TO_PATCH:
         return "补丁应用失败";
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
         return "移除临时文件失败";
      case MSG_FAILED_TO_SAVE_SRAM:
         return "Failed to save SRAM";
      case MSG_FAILED_TO_SAVE_STATE_TO:
         return "Failed to save state to";
      case MSG_FAILED_TO_SEND_NICKNAME:
         return "发送昵称失败.";
      case MSG_FAILED_TO_SEND_NICKNAME_SIZE:
         return "发送昵称尺寸失败.";
      case MSG_FAILED_TO_SEND_NICKNAME_TO_CLIENT:
         return "发送昵称至客户端失败.";
      case MSG_FAILED_TO_SEND_NICKNAME_TO_HOST:
         return "发送昵称至宿主端失败.";
      case MSG_FAILED_TO_SEND_SRAM_DATA_TO_CLIENT:
         return "发送SRAM数据至客户端失败.";
      case MSG_FAILED_TO_START_AUDIO_DRIVER:
         return "音频驱动启动失败，将在无音频模式下继续启动。";
      case MSG_FAILED_TO_START_MOVIE_RECORD:
         return "启动视频录制失败.";
      case MSG_FAILED_TO_START_RECORDING:
         return "Failed to start recording.";
      case MSG_FAILED_TO_TAKE_SCREENSHOT:
         return "Failed to take screenshot.";
      case MSG_FAILED_TO_UNDO_LOAD_STATE:
         return "Failed to undo load state.";
      case MSG_FAILED_TO_UNDO_SAVE_STATE:
         return "Failed to undo save state.";
      case MSG_FAILED_TO_UNMUTE_AUDIO:
         return "Failed to unmute audio.";
      case MSG_FATAL_ERROR_RECEIVED_IN:
         return "Fatal error received in";
      case MSG_FILE_NOT_FOUND:
         return "未找到文件";
      case MSG_FOUND_AUTO_SAVESTATE_IN:
         return "Found auto savestate in";
      case MSG_FOUND_DISK_LABEL:
         return "Found disk label";
      case MSG_FOUND_FIRST_DATA_TRACK_ON_FILE:
         return "Found first data track on file";
      case MSG_FOUND_LAST_STATE_SLOT:
         return "Found last state slot";
      case MSG_FOUND_SHADER:
         return "Found shader";
      case MSG_FRAMES:
         return "帧";
      case MSG_GAME_SPECIFIC_CORE_OPTIONS_FOUND_AT:
         return "Per-Game Options: game-specific core options found at";
      case MSG_GOT_INVALID_DISK_INDEX:
         return "Got invalid disk index.";
      case MSG_GRAB_MOUSE_STATE:
         return "抓取鼠标状态";
      case MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING:
         return "Libretro core is hardware rendered. Must use post-shaded recording as well.";
      case MSG_INFLATED_CHECKSUM_DID_NOT_MATCH_CRC32:
         return "Inflated checksum did not match CRC32.";
      case MSG_INPUT_CHEAT:
         return "输入金手指";
      case MSG_INPUT_CHEAT_FILENAME:
         return "Cheat Filename";
      case MSG_INPUT_PRESET_FILENAME:
         return "Preset Filename";
      case MSG_INTERFACE:
         return "接口";
      case MSG_INTERNAL_STORAGE:
         return "内部存储";
      case MSG_REMOVABLE_STORAGE:
         return "Removable Storage";
      case MSG_INVALID_NICKNAME_SIZE:
         return "Invalid nickname size.";
      case MSG_IN_BYTES:
         return "(字节)";
      case MSG_IN_GIGABYTES:
         return "(吉字节)";
      case MSG_IN_MEGABYTES:
         return "(兆字节)";
      case MSG_LIBRETRO_ABI_BREAK:
         return "is compiled against a different version of libretro than this libretro implementation.";
      case MSG_LIBRETRO_FRONTEND:
         return "为libretro而设计的前端";
      case MSG_LOADED_STATE_FROM_SLOT:
         return "加载状态从槽 #%d.";
      case MSG_LOADED_STATE_FROM_SLOT_AUTO:
         return "加载状态从槽 #-1 (auto).";
      case MSG_LOADING:
         return "正在加载";
      case MSG_LOADING_CONTENT_FILE:
         return "正在加载内容文件";
      case MSG_LOADING_HISTORY_FILE:
         return "正在读取历史文件";
      case MSG_LOADING_STATE:
         return "正在读取存档";
      case MSG_MEMORY:
         return "内存";
      case MSG_MOVIE_FILE_IS_NOT_A_VALID_BSV1_FILE:
         return "Movie file is not a valid BSV1 file.";
      case MSG_MOVIE_FORMAT_DIFFERENT_SERIALIZER_VERSION:
         return "Movie format seems to have a different serializer version. Will most likely fail.";
      case MSG_MOVIE_PLAYBACK_ENDED:
         return "视频回放结束.";
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
         return "暂停。";
      case MSG_PROGRAM:
         return "RetroArch";
      case MSG_READING_FIRST_DATA_TRACK:
         return "读取第一条数据轨...";
      case MSG_RECEIVED:
         return "接收完毕";
      case MSG_RECORDING_TERMINATED_DUE_TO_RESIZE:
         return "Recording terminated due to resize.";
      case MSG_RECORDING_TO:
         return "Recording to";
      case MSG_REDIRECTING_CHEATFILE_TO:
         return "重定向金手指文件至";
      case MSG_REDIRECTING_SAVEFILE_TO:
         return "Redirecting save file to";
      case MSG_REDIRECTING_SAVESTATE_TO:
         return "Redirecting savestate to";
      case MSG_REMAP_FILE_SAVED_SUCCESSFULLY:
         return "Remap file saved successfully.";
      case MSG_REMOVED_DISK_FROM_TRAY:
         return "Removed disk from tray.";
      case MSG_REMOVING_TEMPORARY_CONTENT_FILE:
         return "移除临时内容文件";
      case MSG_RESET:
         return "重置";
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
         return "正在回溯。";
      case MSG_REWIND_INIT:
         return "Initializing rewind buffer with size";
      case MSG_REWIND_INIT_FAILED:
         return "初始化回放缓存失败. 回放功能关闭.";
      case MSG_REWIND_INIT_FAILED_THREADED_AUDIO:
         return "Implementation uses threaded audio. Cannot use rewind.";
      case MSG_REWIND_REACHED_END:
         return "到达回放缓存末端.";
      case MSG_SAVED_NEW_CONFIG_TO:
         return "已保存新配置到";
      case MSG_SAVED_STATE_TO_SLOT:
         return "保存状态至槽 #%d.";
      case MSG_SAVED_STATE_TO_SLOT_AUTO:
         return "保存状态至槽 #-1 (auto).";
      case MSG_SAVED_SUCCESSFULLY_TO:
         return "成功保存至";
      case MSG_SAVING_RAM_TYPE:
         return "Saving RAM type";
      case MSG_SAVING_STATE:
         return "Saving state";
      case MSG_SCANNING:
         return "扫描中";
      case MSG_SCANNING_OF_DIRECTORY_FINISHED:
         return "已完成对文件夹的扫描";
      case MSG_SENDING_COMMAND:
         return "Sending command";
      case MSG_SEVERAL_PATCHES_ARE_EXPLICITLY_DEFINED:
         return "Several patches are explicitly defined, ignoring all...";
      case MSG_SHADER:
         return "Shader";
      case MSG_SHADER_PRESET_SAVED_SUCCESSFULLY:
         return "Shader preset saved successfully.";
      case MSG_SKIPPING_SRAM_LOAD:
         return "Skipping SRAM load.";
      case MSG_SLOW_MOTION:
         return "慢动作。";
      case MSG_SLOW_MOTION_REWIND:
         return "慢动作回溯。";
      case MSG_SORRY_UNIMPLEMENTED_CORES_DONT_DEMAND_CONTENT_NETPLAY:
         return "Sorry, unimplemented: cores that don't demand content cannot participate in netplay.";
      case MSG_SRAM_WILL_NOT_BE_SAVED:
         return "SRAM will not be saved.";
      case MSG_STARTING_MOVIE_PLAYBACK:
         return "视频回放.";
      case MSG_STARTING_MOVIE_RECORD_TO:
         return "Starting movie record to";
      case MSG_STATE_SIZE:
         return "State size";
      case MSG_STATE_SLOT:
         return "状态存档槽";
      case MSG_TAKING_SCREENSHOT:
         return "Taking screenshot.";
      case MSG_TO:
         return "to";
      case MSG_UNDID_LOAD_STATE:
         return "已撤销加载状态。";
      case MSG_UNDOING_SAVE_STATE:
         return "撤销即时存档";
      case MSG_UNKNOWN:
         return "未知";
      case MSG_UNPAUSED:
         return "取消暂停。";
      case MSG_UNRECOGNIZED_COMMAND:
         return "Unrecognized command";
      case MSG_USING_CORE_NAME_FOR_NEW_CONFIG:
         return "Using core name for new config.";
      case MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED:
         return "Using libretro dummy core. Skipping recording.";
      case MSG_VALUE_CONNECT_DEVICE_FROM_A_VALID_PORT:
         return "Connect device from a valid port.";
      case MSG_VALUE_DISCONNECTING_DEVICE_FROM_PORT:
         return "Disconnecting device from port";
      case MSG_VALUE_REBOOTING:
         return "正在重启……";
      case MSG_VALUE_SHUTTING_DOWN:
         return "正在关机……";
      case MSG_VERSION_OF_LIBRETRO_API:
         return "libretro API版本";
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
