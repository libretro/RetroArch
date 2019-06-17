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

#if defined(_MSC_VER) && !defined(_XBOX) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
#if (_MSC_VER >= 1700)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif
#pragma warning(disable:4566)
#endif

int menu_hash_get_help_chs_enum(enum msg_hash_enums msg, char *s, size_t len)
{
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
         case RARCH_SLOWMOTION_HOLD_KEY:
            snprintf(s, len,
                  "按住并以慢动作运行。");
            break;
         case RARCH_PAUSE_TOGGLE:
            snprintf(s, len,
                  "在暂停和非暂停状态间切换。");
            break;
         case RARCH_FRAMEADVANCE:
            snprintf(s, len,
                  "在暂停时，运行一帧。");
            break;
         case RARCH_SHADER_NEXT:
            snprintf(s, len,
                  "应用文件夹中的后一个渲染器特效。");
            break;
         case RARCH_SHADER_PREV:
            snprintf(s, len,
                  "应用文件夹中的前一个渲染器特效。");
            break;
         case RARCH_CHEAT_INDEX_PLUS:
         case RARCH_CHEAT_INDEX_MINUS:
         case RARCH_CHEAT_TOGGLE:
            snprintf(s, len,
                  "金手指。");
            break;
         case RARCH_RESET:
            snprintf(s, len,
                  "重启游戏。");
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
         case RARCH_ENABLE_HOTKEY:
            snprintf(s, len,
                  "启用其他热键。 \n"
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
                  "切换到下一个屏幕图层。将会循环选择。");
            break;
         case RARCH_DISK_EJECT_TOGGLE:
            snprintf(s, len,
                  "切换光盘弹出状态。 \n"
                  " \n"
                  "用于多光盘游戏。 ");
            break;
         case RARCH_DISK_NEXT:
         case RARCH_DISK_PREV:
            snprintf(s, len,
                  "切换下一张光盘。弹出光盘后才能使用。 \n"
                  " \n"
                  "再次切换光盘弹出状态完成换盘。");
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
                  "加载即时存档。");
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
                  "即时存档栏位 \n"
                  "\n"
                  "当选择0号栏位时，即时存档将以*.state命名（或其他 \n"
                  "在命令行中定义的名称）。 \n"
                  "\n"
                  "当栏位不为0时，路径将会设为<path><d>，其中<d> \n"
                  "是栏位的编号。");
            break;
         case RARCH_SAVE_STATE_KEY:
            snprintf(s, len,
                  "保存即时存档。");
            break;
         case RARCH_REWIND:
            snprintf(s, len,
                  "按住按钮来回溯 \n"
                  " \n"
                  "必须先启用回溯倒带功能。");
            break;
         case RARCH_BSV_RECORD_TOGGLE:
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
               "Retro成就账号。 \n"
               " \n"
               "访问 retroachievements.org 并注册 \n"
               "以获取一个免费账号。 \n"
               " \n"
               "在你注册以后, 你需要 \n"
               "在RetroArch输入你的 \n"
               "账号以及密码。");
         break;
      case MENU_ENUM_LABEL_CHEEVOS_USERNAME:
         snprintf(s, len, "你的Retro成就账号的用户名。");
         break;
      case MENU_ENUM_LABEL_CHEEVOS_PASSWORD:
         snprintf(s, len, "你的Retro成就账号的密码。");
         break;
      case MENU_ENUM_LABEL_USER_LANGUAGE:
         snprintf(s, len, "变更菜单和提示消息的语言设定， \n"
               " \n"
               "需要重新启动才能生效。 \n"
               " \n"
               "注意：部分语言的翻译并不完全。 \n"
               " \n"
               "若一个语言尚未翻译，则会恢复英文显示。");
         break;
      case MENU_ENUM_LABEL_VIDEO_FONT_PATH:
         snprintf(s, len, "改变屏显文字的字体。");
         break;
      case MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS:
         snprintf(s, len, "自动加载各游戏单独设定的选项。");
         break;
      case MENU_ENUM_LABEL_AUTO_OVERRIDES_ENABLE:
         snprintf(s, len, "自动加载覆盖配置。");
         break;
      case MENU_ENUM_LABEL_AUTO_REMAPS_ENABLE:
         snprintf(s, len, "自动加载自定义键位文件。");
         break;
      case MENU_ENUM_LABEL_SORT_SAVESTATES_ENABLE:
         snprintf(s, len, "在即时存档的文件名前面加上核心名称 \n"
               "来进行排序。");
         break;
      case MENU_ENUM_LABEL_SORT_SAVEFILES_ENABLE:
         snprintf(s, len, "在游戏存档的文件名前面加上核心名称 \n"
               "来进行排序。");
         break;
      case MENU_ENUM_LABEL_RESUME_CONTENT:
         snprintf(s, len, "关闭菜单，返回游戏。");
         break;
      case MENU_ENUM_LABEL_RESTART_CONTENT:
         snprintf(s, len, "从头开始游戏。");
         break;
      case MENU_ENUM_LABEL_CLOSE_CONTENT:
         snprintf(s, len, "完全关闭游戏。");
         break;
      case MENU_ENUM_LABEL_UNDO_LOAD_STATE:
         snprintf(s, len, "如果你不慎读错了即时存档，\n"
               "使用此命令来撤销。");
         break;
      case MENU_ENUM_LABEL_UNDO_SAVE_STATE:
         snprintf(s, len, "如果你不慎覆盖了即时存档，\n"
               "使用此命令来撤销。");
         break;
      case MENU_ENUM_LABEL_TAKE_SCREENSHOT:
         snprintf(s, len, "创建一份截图。\n"
               " \n"
               "截图文件将会存放在 \n"
               "截图文件夹之中.");
         break;
      case MENU_ENUM_LABEL_ADD_TO_FAVORITES:
         snprintf(s, len, "添加到收藏夹.");
         break;
      case MENU_ENUM_LABEL_RUN:
         snprintf(s, len, "启动游戏.");
         break;
      case MENU_ENUM_LABEL_INFORMATION:
         snprintf(s, len, "显示本游戏的额外 \n"
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
               "文件夹中的游戏。");
         break;
      case MENU_ENUM_LABEL_USE_THIS_DIRECTORY:
         snprintf(s, len,
               "选择本文件夹作为指定文件夹。");
         break;
      case MENU_ENUM_LABEL_CONTENT_DATABASE_DIRECTORY:
         snprintf(s, len,
               "游戏数据库文件夹。 \n"
               " \n"
               "到游戏数据库文件夹的路径。");
         break;
      case MENU_ENUM_LABEL_THUMBNAILS_DIRECTORY:
         snprintf(s, len,
               "缩略图文件夹。 \n"
               " \n"
               "用以存放缩略图。");
         break;
      case MENU_ENUM_LABEL_LIBRETRO_INFO_PATH:
         snprintf(s, len,
               "模拟器核心信息文件夹。 \n"
               " \n"
               "用于搜索libretro核心信息 \n"
               "的文件夹。");
         break;
      case MENU_ENUM_LABEL_PLAYLIST_DIRECTORY:
         snprintf(s, len,
               "运行列表文件夹. \n"
               " \n"
               "保存所有播放列表到 \n"
               "此文件夹。");
         break;
      case MENU_ENUM_LABEL_DUMMY_ON_CORE_SHUTDOWN:
         snprintf(s, len,
               "某些libretro核心可能会 \n"
               "支持关机特性。 \n"
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
               "某些核心可能需要固件或 BIOS 文件。 \n"
               " \n"
               "如果禁用此选项，即使没有固件或 BIOS"
               "也会尝试强行加载。 \n");
         break;
      case MENU_ENUM_LABEL_PARENT_DIRECTORY:
         snprintf(s, len,
               "回到上级文件夹。");
         break;
      case MENU_ENUM_LABEL_FILE_BROWSER_SHADER_PRESET:
         snprintf(s, len,
               "渲染器预设文件。");
         break;
      case MENU_ENUM_LABEL_FILE_BROWSER_SHADER:
         snprintf(s, len,
               "渲染器文件。");
         break;
      case MENU_ENUM_LABEL_FILE_BROWSER_REMAP:
         snprintf(s, len,
               "控制自定义键位文件。");
         break;
      case MENU_ENUM_LABEL_FILE_BROWSER_CHEAT:
         snprintf(s, len,
               "金手指文件。");
         break;
      case MENU_ENUM_LABEL_FILE_BROWSER_OVERLAY:
         snprintf(s, len,
               "图层文件。");
         break;
      case MENU_ENUM_LABEL_FILE_BROWSER_RDB:
         snprintf(s, len,
               "数据库文件。");
         break;
      case MENU_ENUM_LABEL_FILE_BROWSER_FONT:
         snprintf(s, len,
               "TrueType 字体文件。");
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
               "Libretro 核心 \n"
               " \n"
               "选中核心将会使其关联至游戏。");
         break;
      case MENU_ENUM_LABEL_FILE_BROWSER_CORE:
         snprintf(s, len,
               "Libretro 核心 \n"
               " \n"
               "选择该文件使 RetroArch 加载该核心。");
         break;
      case MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY:
         snprintf(s, len,
               "文件夹 \n"
               " \n"
               "选择并打开该文件夹。");
         break;
      case MENU_ENUM_LABEL_CACHE_DIRECTORY:
         snprintf(s, len,
               "缓存文件夹 \n"
               " \n"
               "被 RetroArch 解压的游戏内容会临时存放到这个文 \n"
               "件夹。");
         break;
      case MENU_ENUM_LABEL_HISTORY_LIST_ENABLE:
         snprintf(s, len,
               "若开启，所有在 RetroArch 中加载过的文件 \n"
               "都会自动的放入最近使用历史列表中。");
         break;
      case MENU_ENUM_LABEL_RGUI_BROWSER_DIRECTORY:
         snprintf(s, len,
               "文件浏览器文件夹 \n"
               " \n"
               "设置文件浏览器的初始文件夹。");
         break;
      case MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR:
         snprintf(s, len,
               "影响输入轮询过程在 RetroArch 中的执行方式。 \n"
               " \n"
               "稍早 - 输入轮询过程将在帧生成之前执行。 \n"
               "正常 - 输入轮询过程将在被请求时执行。 \n"
               "稍晚 - 输入轮询过程将在每一帧的首次请求时执行。 \n"
               " \n"
               "依据设置的不同，设置为「稍早」或「稍晚」可以获得 \n"
               "较低的延迟。 \n"
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
               "禁用 sRGB 帧缓冲支持。\n"
               "\n"
               "某些在 Windows 上的 Intel OpenGL 驱动 \n"
               "对 sRGB 帧缓冲支持存在问题， \n"
               "需要启用此选项以禁用帧缓冲。");
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
               "音频延迟（单位：毫秒） \n"
               "如果设置太低，音频驱动可能 \n"
               "并不支持，从而不会生效。");
         break;
      case MENU_ENUM_LABEL_VIDEO_ALLOW_ROTATE:
         snprintf(s, len,
               "允许核心自行设置屏幕旋转角度，\n"
               "若关闭，旋转请求将不会得到执行。\n"
               "需要旋转显示器的用户请设置此选项。\n");
         break;
      case MENU_ENUM_LABEL_INPUT_DESCRIPTOR_LABEL_SHOW:
         snprintf(s, len,
               "显示由核心设置的输入设备硬件信息，而非默认。");
         break;
      case MENU_ENUM_LABEL_CONTENT_HISTORY_SIZE:
         snprintf(s, len,
               "在历史记录中最多保存的游戏个数。");
         break;
      case MENU_ENUM_LABEL_VIDEO_WINDOWED_FULLSCREEN:
         snprintf(s, len,
               "使用无边框全屏替代普通全屏模式。");
         break;
      case MENU_ENUM_LABEL_VIDEO_FONT_SIZE:
         snprintf(s, len,
               "屏显信息的字体大小。");
         break;
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_INDEX:
         snprintf(s, len,
               "每次储存即时存档时，都在新的栏位储存， \n"
               "以避免覆盖原有的即时存档。 \n"
               "每次运行游戏时，都会定位到最新的 \n"
               "即时存档栏位。");
         break;
      case MENU_ENUM_LABEL_FPS_SHOW:
         snprintf(s, len,
               "显示/隐藏帧数");
         break;
      case MENU_ENUM_LABEL_VIDEO_FONT_ENABLE:
         snprintf(s, len,
               "显示/隐藏屏显信息.");
         break;
      case MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_X:
      case MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_Y:
         snprintf(s, len,
               "提示消息在屏幕上显示的位置，\n"
               "数值范围为0.0至1.0。");
         break;
      case MENU_ENUM_LABEL_INPUT_OVERLAY_ENABLE:
         snprintf(s, len,
               "显示/隐藏当前按下的键位。");
         break;
      case MENU_ENUM_LABEL_INPUT_OVERLAY_HIDE_IN_MENU:
         snprintf(s, len,
               "在进入菜单时，不显示当前按下的键位。");
         break;
      case MENU_ENUM_LABEL_OVERLAY_PRESET:
         snprintf(s, len,
               "Path to input overlay.");
         break;
      case MENU_ENUM_LABEL_OVERLAY_OPACITY:
         snprintf(s, len,
               "图层透明度。");
         break;
      case MENU_ENUM_LABEL_INPUT_BIND_TIMEOUT:
         snprintf(s, len,
               "绑定键位时的等待时间（单位：秒）");
         break;
      case MENU_ENUM_LABEL_INPUT_BIND_HOLD:
         snprintf(s, len,
               "绑定键位所需要按下的时间（单位：秒）");
         break;
      case MENU_ENUM_LABEL_OVERLAY_SCALE:
         snprintf(s, len,
               "图层放大比例。");
         break;
      case MENU_ENUM_LABEL_AUDIO_OUTPUT_RATE:
         snprintf(s, len,
               "音频输出采样率。");
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
               "你可以使用下述的方式通过游戏控制\n"
               "器或者键盘来对菜单进行控制：\n"
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
                  "RetroArch 依赖于一种独特的音频/视频同步形式，\n"
                  "需要根据显示器的刷新率对其进行校准，以获得最佳\n"
                  "性能。\n"
                  " \n"
                  "如果你的音频出现任何噼啪声或图像撕裂，通常意味\n"
                  "着你需要调整设置。下面是调整方法：\n"
                  " \n";
            snprintf(u, sizeof(u), /* can't inline this due to the printf arguments */
                  "a) 在「%s」 ->「%s」中开启\n"
                  "「多线程渲染」。在这种模式下，刷新率不会发生变\n"
                  "化、帧率会提高，但是画面可能不那么流畅。\n"
                  "b) 在「%s」 -> 「%s」, 查看\n"
                  "「%s」。令其运行 2048 帧后，按 OK 键。",
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
               "若要扫描游戏内容，请访问菜单「%s」\n"
               "并选择「%s」或者「%s」。\n"
               "\n"
               "文件将会同数据库中的条目进行对比。\n"
               "若文件匹配某个条目，则它会被加入戏列表中。\n"
               "\n"
               "你可以无需每次都打开文件浏览器，而可以直接\n"
               "通过菜单项「%s」->「%s」 来访\n"
               "问这些游戏内容。\n"
               " \n"
               "注意：不是所有核心的游戏内容都支持扫描录入。"
               ,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_FILE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB)
               );
         break;
      case MENU_ENUM_LABEL_VALUE_EXTRACTING_PLEASE_WAIT:
         snprintf(s, len,
               "欢迎使用 RetroArch\n"
               "\n"
               "正在解压必要文件, 请稍等。\n"
               "这可能需要一点时间……\n"
               );
         break;
      case MENU_ENUM_LABEL_INPUT_DRIVER:
         {
            const char *lbl = settings ? settings->arrays.input_driver : NULL;

            if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_INPUT_DRIVER_UDEV)))
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
               "加载游戏内容 \n"
               "通过浏览来加载游戏内容。 \n"
               " \n"
               "你需要同时提供一个「核心」和游戏内容 \n"
               "文件才能启动并加载游戏内容。 \n"
               " \n"
               "设置「文件浏览器文件夹」可以指定以哪个 \n"
               "位置为文件浏览器的默认文件夹以方便加载。 \n"
               "若没有设置，默认以根目录为基准。 \n"
               " \n"
               "文件浏览器会以上次加载的核心所支持的 \n"
               "扩展名进行过滤，并使用该核心来加载 \n"
               "游戏内容。"
               );
         break;
      case MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY:
         snprintf(s, len,
               "从历史记录中加载游戏内容。 \n"
               " \n"
               "每当你加载一个游戏时，它和所使用的核心 \n"
               "将被保存到历史记录中。 \n"
               " \n"
               "历史记录文件和 RetroArch 设置文件在同一个 \n"
               "文件夹中。如果 RetroArch 启动时没有找到 \n"
               "历史记录文件，主菜单中将不会显示历史记录。"
               );
         break;
      case MENU_ENUM_LABEL_VIDEO_DRIVER:
         snprintf(s, len,
               "当前视频驱动.");

         if (string_is_equal(settings->arrays.video_driver, "gl"))
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
         else if (string_is_equal(settings->arrays.video_driver, "sdl2"))
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
         else if (string_is_equal(settings->arrays.video_driver, "sdl1"))
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
         else if (string_is_equal(settings->arrays.video_driver, "d3d"))
         {
            snprintf(s, len,
                  "Direct3D 视频驱动. \n"
                  " \n"
                  "Performance for software-rendered cores \n"
                  "is dependent on your graphic card's \n"
                  "underlying D3D driver).");
         }
         else if (string_is_equal(settings->arrays.video_driver, "exynos"))
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
         else if (string_is_equal(settings->arrays.video_driver, "drm"))
         {
            snprintf(s, len,
                  "Plain DRM 视频驱动. \n"
                  " \n"
                  "This is a low-level video driver using. \n"
                  "libdrm for hardware scaling using \n"
                  "GPU overlays.");
         }
         else if (string_is_equal(settings->arrays.video_driver, "sunxi"))
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
               "载入预设渲染器. \n"
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
               " 预设文件夹. \n"
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
               "设置Libretro核心的日志输出级别 \n"
               "(GET_LOG_INTERFACE). \n"
               "\n"
               "If a log level issued by a libretro \n"
               "core is below libretro_log level, it \n"
               "is ignored.\n"
               "\n"
               "DEBUG logs are always ignored unless \n"
               "verbose mode is activated (--verbose).\n"
               "\n"
               "调试 = 0\n"
               "提示 = 1\n"
               "警告 = 2\n"
               "错误 = 3"
               );
         break;
      case MENU_ENUM_LABEL_STATE_SLOT_INCREASE:
      case MENU_ENUM_LABEL_STATE_SLOT_DECREASE:
         snprintf(s, len,
               "即时存档栏位\n"
               "\n"
               "当选择0号栏位时，即时存档将以*.state命名（或其他 \n"
               "在命令行中定义的名称）。 \n"
               "\n"
               "当栏位不为0时，路径将会设为<path><d>，其中<d> \n"
               "是栏位的编号。");
         break;
      case MENU_ENUM_LABEL_SHADER_APPLY_CHANGES:
         snprintf(s, len,
               "应用渲染器更改。 \n"
               " \n"
               "当你修改渲染器设置后，执行此选项 \n"
               "以应用更改。 \n"
               " \n"
               "渲染器可能非常耗费资源， \n"
               "因此必须单独进行设置。 \n"
               " \n"
               "应用渲染器更改后，渲染器设置将被 \n"
               "保存到 Shader 文件夹下的文件中 \n"
               "（扩展名为 .cgp 或 .glslp）。 \n"
               "即使退出 RetroArch，该文件也会被保留。"
               );
         break;
      case MENU_ENUM_LABEL_MENU_TOGGLE:
         snprintf(s, len,
               "切换菜单。");
         break;
      case MENU_ENUM_LABEL_GRAB_MOUSE_TOGGLE:
         snprintf(s, len,
               "切换鼠标抓取。\n"
               " \n"
               "When mouse is grabbed, RetroArch hides the \n"
               "mouse, and keeps the mouse pointer inside \n"
               "the window to allow relative mouse input to \n"
               "work better.");
         break;
      case MENU_ENUM_LABEL_DISK_NEXT:
         snprintf(s, len,
               "切换下一张光盘。弹出光盘后才能使用。 \n"
               " \n"
               "再次切换光盘弹出状态完成换盘。");
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
               "RSound 需要 RSound 服务器的 IP 地址。 \n"
#endif
               );
         break;
      case MENU_ENUM_LABEL_DISK_EJECT_TOGGLE:
         snprintf(s, len,
               "切换光盘弹出状态。\n"
               " \n"
               "用于多光盘游戏。");
         break;
      case MENU_ENUM_LABEL_ENABLE_HOTKEY:
         snprintf(s, len,
               "启用其他热键。\n"
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
               "启用回溯倒带功能。\n"
               " \n"
               "这可能会严重影响性能, \n"
               "所以默认设置为关闭。");
         break;
      case MENU_ENUM_LABEL_LIBRETRO_DIR_PATH:
         snprintf(s, len,
               "核心文件夹。 \n"
               " \n"
               "放置 Libretro 核心的文件夹。");
         break;
      case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_AUTO:
         snprintf(s, len,
               "自动匹配刷新率。\n"
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
               "全屏分辨率。\n"
               " \n"
               "如果设置为 0，则使用设备默认分辨率。\n");
         break;
      case MENU_ENUM_LABEL_FASTFORWARD_RATIO:
         snprintf(s, len,
               "快进速度。\n"
               " \n"
               "开启快进模式时的最大运行速度倍数。\n"
               " \n"
               "例如 60fps 的游戏开启 5 倍速 就是 300 fps。\n"
               " \n"
               "RetroArch 会尽可能保证快进时不超过该速度，\n"
               "但不会特别精确。");
         break;
      case MENU_ENUM_LABEL_VIDEO_MONITOR_INDEX:
         snprintf(s, len,
               "指定输出显示器。\n"
               " \n"
               "（默认 0）表示不指定显示器，\n"
               "1 表示使用 1 号显示器，以此类推。");
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
               "只放大整数倍。\n"
               "\n"
               "基础分辨率与游戏、宽高比有关。\n"
               "\n"
               "如果「保持宽高比」选项未开启，\n"
               "不保证宽高放大倍数相同。");
         break;
      case MENU_ENUM_LABEL_AUDIO_VOLUME:
         snprintf(s, len,
               "音量增益（分贝）。\n"
               " \n"
               "0 表示正常音量。\n"
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
               "调高音量。");
         break;
      case MENU_ENUM_LABEL_VOLUME_DOWN:
         snprintf(s, len,
               "降低音量。");
         break;
      case MENU_ENUM_LABEL_VIDEO_DISABLE_COMPOSITION:
         snprintf(s, len,
               "Forcibly disable composition.\n"
               "Only valid on Windows Vista/7 for now.");
         break;
      case MENU_ENUM_LABEL_PERFCNT_ENABLE:
         snprintf(s, len,
               "启用或关闭前端 \n"
               "性能计数。");
         break;
      case MENU_ENUM_LABEL_SYSTEM_DIRECTORY:
         snprintf(s, len,
               "系统文件夹。 \n"
               " \n"
               "Sets the 'system' directory.\n"
               "Cores can query for this\n"
               "directory to load BIOSes, \n"
               "system-specific configs, etc.");
         break;
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE:
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_LOAD:
         snprintf(s, len,
               "在 RetroArch 退出时 \n"
               "自动保存即时存档。\n"
               " \n"
               "如果自动读档选项已打开，\n"
               "每次运行游戏时还会读取该存档。");
         break;
      case MENU_ENUM_LABEL_VIDEO_THREADED:
         snprintf(s, len,
               "使用独立线程来处理视频。\n"
               " \n"
               "虽然可能提升游戏速度，但也可能"
               "增加延迟或导致图像卡顿。");
         break;
      case MENU_ENUM_LABEL_VIDEO_VSYNC:
         snprintf(s, len,
               "视频垂直同步.\n");
         break;
      case MENU_ENUM_LABEL_VIDEO_HARD_SYNC:
         snprintf(s, len,
               "尝试硬件同步CPU和GPU。\n"
               " \n"
               "可以减少潜在的性能开销。");
         break;
      case MENU_ENUM_LABEL_REWIND_GRANULARITY:
         snprintf(s, len,
               "每次回溯的帧数\n"
               " \n"
               "通过一次回溯多帧来提升回溯的速度。");
         break;
      case MENU_ENUM_LABEL_SCREENSHOT:
         snprintf(s, len,
               "截图。");
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
               "在每帧之间插入一帧黑屏。\n"
               " \n"
               "120 Hz 显示器开启此选项运行 \n"
               "60 Hz 游戏可以避免重影。 \n"
               " \n"
               "图像刷新率仍应按照 60 Hz 显示器 \n"
               "来进行设置。（除以 2） \n");
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
               "切换窗口时暂停游戏。");
         break;
      case MENU_ENUM_LABEL_VIDEO_GPU_SCREENSHOT:
         snprintf(s, len,
               "使用 GPU 输出来进行截图（如果可能的话）。");
         break;
      case MENU_ENUM_LABEL_SCREENSHOT_DIRECTORY:
         snprintf(s, len,
               "截图文件夹 \n"
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
               "游戏存档文件夹。 \n"
               " \n"
               "所有游戏存档都保存在此文件夹。 \n"
               "常见的游戏存档格式有 \n"
               ".srm, .bsv, .rt, .psrm 等\n"
               " \n"
               "此选项可能被特定命令行选项覆盖。");
         break;
      case MENU_ENUM_LABEL_SAVESTATE_DIRECTORY:
         snprintf(s, len,
               "即时存档文件夹. \n"
               " \n"
               "所有即时存档 (.state 文件) 都 \n"
               "保存在此文件夹。 \n"
               " \n"
               "此选项可能被特定命令行选项覆盖。");
         break;
      case MENU_ENUM_LABEL_ASSETS_DIRECTORY:
         snprintf(s, len,
               "Assets Directory. \n"
               " \n"
               "This location is queried by default when \n"
               "menu interfaces try to look for loadable \n"
               "assets, etc.");
         break;
      case MENU_ENUM_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY:
         snprintf(s, len,
               "动态壁纸文件夹 \n"
               " \n"
               "保存用于主界面的、依据游戏内容变化的动态壁纸。");
         break;
      case MENU_ENUM_LABEL_SLOWMOTION_RATIO:
         snprintf(s, len,
               "减速比例"
               " \n"
               "减速游戏时，速度将被降低的倍数。");
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
         snprintf(s, len, "启用触摸屏支持。");
         break;
      case MENU_ENUM_LABEL_INPUT_PREFER_FRONT_TOUCH:
         snprintf(s, len, "使用前触摸屏，而非后触摸屏。");
         break;
      case MENU_ENUM_LABEL_MOUSE_ENABLE:
         snprintf(s, len, "在菜单中，启用鼠标。");
         break;
      case MENU_ENUM_LABEL_POINTER_ENABLE:
         snprintf(s, len, "在菜单中，启用触摸屏。");
         break;
      case MENU_ENUM_LABEL_MENU_WALLPAPER:
         snprintf(s, len, "菜单壁纸图片的路径。");
         break;
      case MENU_ENUM_LABEL_NAVIGATION_WRAPAROUND:
         snprintf(s, len,
               "Wrap-around to beginning and/or end \n"
               "if boundary of list is reached \n"
               "horizontally and/or vertically.");
         break;
      case MENU_ENUM_LABEL_PAUSE_LIBRETRO:
         snprintf(s, len,
               "如果关闭此选项，打开菜单时 \n"
               "游戏仍会后台运行。");
         break;
      case MENU_ENUM_LABEL_SUSPEND_SCREENSAVER_ENABLE:
         snprintf(s, len,
               "Suspends the screensaver. Is a hint that \n"
               "does not necessarily have to be \n"
               "honored by the video driver.");
         break;
      case MENU_ENUM_LABEL_NETPLAY_MODE:
         snprintf(s, len,
               "开启：联网时为客户端模式。\n"
               "关闭：联网时为服务器模式。");
         break;
      case MENU_ENUM_LABEL_NETPLAY_DELAY_FRAMES:
         snprintf(s, len,
               "联网时延迟的帧数。 \n"
               " \n"
               "提升数值将改善游戏表现， \n"
               "但延迟也会增加。");
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
               "用双线性过滤使图像平滑。 \n"
               "如果你使用了渲染器，请关闭它。");
         break;
      case MENU_ENUM_LABEL_TIMEDATE_ENABLE:
         snprintf(s, len,
               "在菜单中显示当前日期和时间。");
         break;
      case MENU_ENUM_LABEL_CORE_ENABLE:
         snprintf(s, len,
               "在菜单中显示当前运行的核心名称。");
         break;
      case MENU_ENUM_LABEL_NETPLAY_ENABLE_HOST:
         snprintf(s, len,
               "启用联机游戏（服务器模式）。");
         break;
      case MENU_ENUM_LABEL_NETPLAY_ENABLE_CLIENT:
         snprintf(s, len,
               "启用联机游戏（客户端模式）。");
         break;
      case MENU_ENUM_LABEL_NETPLAY_DISCONNECT:
         snprintf(s, len,
               "断开当前网络连接。");
         break;
      case MENU_ENUM_LABEL_NETPLAY_SETTINGS:
         snprintf(s, len,
               "联机游戏设置。");
         break;
      case MENU_ENUM_LABEL_NETPLAY_LAN_SCAN_SETTINGS:
         snprintf(s, len,
               "在局域网中搜索并加入联机游戏服务器。");
         break;
      case MENU_ENUM_LABEL_DYNAMIC_WALLPAPER:
         snprintf(s, len,
               "在不同的环境下加载不同的壁纸。");
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
               "RetroArch 支持的最大用户数量。");
         break;
      case MENU_ENUM_LABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE:
         snprintf(s, len,
               "下载后自动解压。");
         break;
      case MENU_ENUM_LABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
         snprintf(s, len,
               "Filter files being shown by \n"
               "supported extensions.");
         break;
      case MENU_ENUM_LABEL_NETPLAY_NICKNAME:
         snprintf(s, len,
               "你的 RetroArch 用户名，用于联机游戏。");
         break;
      case MENU_ENUM_LABEL_NETPLAY_TCP_UDP_PORT:
         snprintf(s, len,
               "服务器的端口。 \n"
               "TCP 或 UDP 端口均可。");
         break;
      case MENU_ENUM_LABEL_NETPLAY_SPECTATOR_MODE_ENABLE:
         snprintf(s, len,
               "启用或禁用网络对战的观战模式。");
         break;
      case MENU_ENUM_LABEL_NETPLAY_IP_ADDRESS:
         snprintf(s, len,
               "服务器的地址。");
         break;
      case MENU_ENUM_LABEL_STDIN_CMD_ENABLE:
         snprintf(s, len,
               "启用标准命令行输入。");
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
               "切换菜单的按键。 \n"
               " \n"
               "0 - 无 \n"
               "1 - 同时按下 L + R + Y + D-Pad \n"
               "2 - 同时按下 L3 + R3 \n"
               "3 - 同时按下 Start + Select ");
         break;
      case MENU_ENUM_LABEL_INPUT_ALL_USERS_CONTROL_MENU:
         snprintf(s, len, "允许任何玩家打开菜单。");
         break;
      case MENU_ENUM_LABEL_INPUT_AUTODETECT_ENABLE:
         snprintf(s, len,
               "启用自动配置按键。\n"
               " \n"
               "RetroArch 将尝试自动配置手柄按键， \n"
               "和即插即用模式。");
         break;
      case MENU_ENUM_LABEL_CAMERA_ALLOW:
         snprintf(s, len,
               "允许或禁止核心调用摄像头。");
         break;
      case MENU_ENUM_LABEL_LOCATION_ALLOW:
         snprintf(s, len,
               "允许或禁止核心调用定位系统（GPS）。");
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
               "启用或禁用屏幕软键盘。");
         break;
      case MENU_ENUM_LABEL_AUDIO_MUTE:
         snprintf(s, len,
               "静音或恢复声音。");
         break;
      case MENU_ENUM_LABEL_REWIND:
         snprintf(s, len,
               "按住按钮来回溯 \n"
               " \n"
               "必须先启用回溯倒带功能。");
         break;
      case MENU_ENUM_LABEL_EXIT_EMULATOR:
         snprintf(s, len,
               "正常退出 RetroArch。"
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
               "读取即时存档。");
         break;
      case MENU_ENUM_LABEL_SAVE_STATE:
         snprintf(s, len,
               "保存即时存档。");
         break;
      case MENU_ENUM_LABEL_CHEAT_INDEX_PLUS:
         snprintf(s, len,
               "下一个金手指。");
         break;
      case MENU_ENUM_LABEL_CHEAT_INDEX_MINUS:
         snprintf(s, len,
               "前一个金手指。");
         break;
      case MENU_ENUM_LABEL_SHADER_PREV:
         snprintf(s, len,
               "应用文件夹中的前一个渲染器特效。");
         break;
      case MENU_ENUM_LABEL_SHADER_NEXT:
         snprintf(s, len,
               "应用文件夹中的后一个渲染器特效。");
         break;
      case MENU_ENUM_LABEL_RESET:
         snprintf(s, len,
               "重启游戏。");
         break;
      case MENU_ENUM_LABEL_PAUSE_TOGGLE:
         snprintf(s, len,
               "在暂停和非暂停状态间切换。");
         break;
      case MENU_ENUM_LABEL_CHEAT_TOGGLE:
         snprintf(s, len,
               "切换金手指启用状态。");
         break;
      case MENU_ENUM_LABEL_HOLD_FAST_FORWARD:
         snprintf(s, len,
               "按下按键快进，松开按键恢复正常。");
         break;
      case MENU_ENUM_LABEL_SLOWMOTION_HOLD:
         snprintf(s, len,
               "按下按键减速，松开按键恢复正常。");
         break;
      case MENU_ENUM_LABEL_FRAME_ADVANCE:
         snprintf(s, len,
               "游戏暂停时，运行一帧。");
         break;
      case MENU_ENUM_LABEL_BSV_RECORD_TOGGLE:
         snprintf(s, len,
               "切换是否处于录像状态。");
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
               "RetroArch 本身并不能做什么事情。 \n"
               " \n"
               "如果想在上面干点什么，你需要向它加载 \n"
               "一个程序。\n"
               " \n"
               "我们把这样的程序叫做「Libretro 核心」， \n"
               "简称「核心」。 \n"
               " \n"
               "你可以从「加载核心」菜单中选择一个核心。 \n"
               " \n"
#ifdef HAVE_NETWORKING
               "你可以通过以下几种方法来获取核心: \n"
               "一、通过访问菜单项「%s」 \n"
               " -> 「%s」来下载；\n"
               " \n"
               "二、手动将其移入核心文件夹中，访问文件\n"
               "夹设置找到你的「%s」。",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH)
#else
               "你可以通过手动将核心移入文件夹中来添加它们， \n"
               "访问文件夹设置找到你的「%s」。",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH)
#endif
               );
         break;
      case MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD_DESC:
         snprintf(s, len,
               "你可以通过「%s」->「%s」更改虚拟按键。\n"
               " \n"
               "从那里你可以改变虚拟按键的样式、大小和不透\n"
               "明度。\n"
               " \n"
               "注意：默认情况下，在菜单中虚拟按键是隐藏的。\n"
               "如果您想在菜单中显示，可以将「%s」\n"
               "设置为 OFF 。",
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
