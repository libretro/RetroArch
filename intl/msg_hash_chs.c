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

#include <stdio.h>
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
                  "切换弹出光盘。 \n"
                  " \n"
                  "用于多光盘内容。 ");
            break;
         case RARCH_DISK_NEXT:
         case RARCH_DISK_PREV:
            snprintf(s, len,
                  "磁盘镜像周期。弹出后使用。 \n"
                  " \n"
                  "再次开关弹出完成。");
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
         snprintf(s, len, "你的Retro Achievements账号的用户名。");
         break;
      case MENU_ENUM_LABEL_CHEEVOS_PASSWORD:
         snprintf(s, len, "你的Retro Achievements账号的密码。");
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
         snprintf(s, len, "自动加载覆盖配置。");
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
         snprintf(s, len, "关闭内容并从内存中卸载。");
         break;
      case MENU_ENUM_LABEL_UNDO_LOAD_STATE:
         snprintf(s, len, "If a state was loaded, content will \n"
               "go back to the state prior to loading.");
         break;
      case MENU_ENUM_LABEL_UNDO_SAVE_STATE:
         snprintf(s, len, "如果状态被覆盖，它将 \n"
               "它将回滚到上一保存的状态。");
         break;
      case MENU_ENUM_LABEL_TAKE_SCREENSHOT:
         snprintf(s, len, "创建一份截图. \n"
               " \n"
               "截图文件将会存放在 \n"
               "截图目录之中.");
         break;
      case MENU_ENUM_LABEL_ADD_TO_FAVORITES:
         snprintf(s, len, "添加到收藏夹.");
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
         snprintf(s, len, "数据库指针文件。");
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
               "内容数据库目录。 \n"
               " \n"
               "到内容数据库目录的 \n"
               "路径。");
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
               "用于搜索libretro核心信息 \n"
               "的目录。");
         break;
      case MENU_ENUM_LABEL_PLAYLIST_DIRECTORY:
         snprintf(s, len,
               "运行列表目录. \n"
               " \n"
               "保存所有播放列表到 \n"
               "此目录。");
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
            driver_hash = msg_hash_calculate(settings->arrays.input_driver);

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

         if (string_is_equal_fast(settings->arrays.video_driver, "gl", 2))
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
         else if (string_is_equal_fast(settings->arrays.video_driver, "sdl2", 4))
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
         else if (string_is_equal_fast(settings->arrays.video_driver, "sdl1", 4))
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
         else if (string_is_equal_fast(settings->arrays.video_driver, "d3d", 3))
         {
            snprintf(s, len,
                  "Direct3D 视频驱动. \n"
                  " \n"
                  "Performance for software-rendered cores \n"
                  "is dependent on your graphic card's \n"
                  "underlying D3D driver).");
         }
         else if (string_is_equal_fast(settings->arrays.video_driver, "exynos", 6))
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
         else if (string_is_equal_fast(settings->arrays.video_driver, "drm", 3))
         {
            snprintf(s, len,
                  "Plain DRM 视频驱动. \n"
                  " \n"
                  "This is a low-level video driver using. \n"
                  "libdrm for hardware scaling using \n"
                  "GPU overlays.");
         }
         else if (string_is_equal_fast(settings->arrays.video_driver, "sunxi", 5))
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
            driver_hash = msg_hash_calculate(settings->arrays.audio_resampler);

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
#include "msg_hash_chs.h"

      default:
#if 0
         RARCH_LOG("Unimplemented: [%d]\n", msg);
#endif
         break;
   }

   return "null";
}
