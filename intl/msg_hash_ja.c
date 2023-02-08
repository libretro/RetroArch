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

#if defined(_MSC_VER) && !defined(_XBOX)
#if (_MSC_VER >= 1700 && _MSC_VER < 1900)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif
#pragma warning(disable:4566)
#endif

int msg_hash_get_help_jp_enum(enum msg_hash_enums msg, char *s, size_t len)
{
   if (msg <= MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_END &&
         msg >= MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN)
   {
      unsigned idx = msg - MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN;

      switch (idx)
      {
         case RARCH_FAST_FORWARD_KEY:
            snprintf(s, len,
                  "普通のスピードから早送りまで切り替える。"
                  );
            break;
         case RARCH_FAST_FORWARD_HOLD_KEY:
            snprintf(s, len,
                  "ホールドで早送り。\n"
                  " \n"
                  "離すと普通のスピードで戻ります。"
                  );
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
      case MENU_ENUM_LABEL_VALUE_MENU_ENUM_CONTROLS_PROLOG:
         snprintf(s, len,
            "メニューを操作するには \n"
            "ゲームパッドまたはキーボードで以下の入力を使用できます: \n"
            " \n"
               );
         break;
      case MENU_ENUM_LABEL_WELCOME_TO_RETROARCH:
         snprintf(s, len,
               "RetroArchにようこそ\n"
               );
         break;
      case MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING_DESC: {
            /* Work around C89 limitations */
            char u[501];
            const char *t =
                  "RetroArchは、オーディオとビデオの同期を独自の形態で行っています。\n"
                  "最高のパフォーマンスを得るためには、ディスプレイの\n"
                  "リフレッシュレート調整が必要です。\n"
                  " \n"
                  "もしオーディオの歪みやビデオのティアリングが発生しているなら、\n"
                  "設定を確認してみてください。\n"
                  "以下の選択肢を試してみてください:\n"
                  " \n";
            snprintf(u, sizeof(u), /* can't inline this due to the printf arguments */
                  "a) [%s]→[%s]に行き、[%s]を有効にしてください。\n"
                  "このモードでは、リフレッシュレートは重要ではありません。\n"
                  "フレームレートは高くなりますが、ビデオは少しスムーズでなくなります。\n"
                  "b) [%s]→[%s]に行き、[%s]が2048 framesに\n"
                  "なっていることを確認してください。",
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SETTINGS),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_THREADED),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SETTINGS),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO));
            strlcpy(s, t, len);
            strlcat(s, u, len);
         }
         break;
         case MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT_DESC:
            snprintf(s, len,
               "コンテンツをスキャンするには、[%s]に行き\n"
                   "[%s]または[%s]を選択してください。\n"
                   "\n"
                   "コンテンツファイルはデータベースのエントリーと比較されます。\n"
                   "一致した場合、プレイリストに追加されます。\n"
                   "\n"
                   "毎回ファイルブラウザを使用するかわりに、\n"
                   "[%s]→[%s]でこのコンテンツに簡単にアクセスできます。\n"
                   "\n"
                   "NOTE: いくつかのコア向けのコンテンツはまだスキャンできません。\n",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_FILE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB)
            );
            break;
      case MENU_ENUM_LABEL_LOAD_CONTENT_LIST:
         snprintf(s, len,
               "コンテンツをロードするには、それを実行する \n"
               "ための｢コア｣と、コンテンツファイルが必要です。 \n"
               " \n"
               "コンテンツの参照開始ディレクトリは、 \n"
               "[%s]で設定してください。 \n"
               "未設定のときは、ルートディレクトリとなります。 \n"
               " \n"
               "ブラウザでの参照時には、[%s]で \n"
               "最後に設定したコアに対応する拡張子で \n"
               "絞り込みます。 \n",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_LIST)
               );
         break;
      case MENU_ENUM_LABEL_CONFIRM_ON_EXIT:
         snprintf(s, len, "本当に終了しますか？");
         break;
      case MENU_ENUM_LABEL_SHOW_HIDDEN_FILES:
         snprintf(s, len, "隠しファイルとフォルダを表示する。");
         break;
      case MENU_ENUM_LABEL_VALUE_WHAT_IS_A_CORE_DESC: {
         /* Work around C89 limitations */
         char u[501];
         const char *t =
            "RetroArch自身だけでは何も実行できません。\n"
            "\n"
            "何かを実行するためには、それに対応する\n"
            "プログラムをロードする必要があります。\n"
            "\n"
            "そのようなプログラムを｢Libretroコア｣、\n"
            "または省略して｢コア｣と呼びます。\n"
            " \n";
         snprintf(u, sizeof(u),
            "コアをロードするには、[%s]から\n"
            "対応するコアを選択してください。\n"
            "\n"
#ifdef HAVE_NETWORKING
            "以下のいずれかの方法でコアを利用できます:\n"
            "* [%s]→[%s]\n"
            "からダウンロードする。\n"
            "* [%s] に手動で移動する。"
            " \n",
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_LIST),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH)
#else
            "手動で[%s]に移動することでコアを利用できます。\n",
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_LIST),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH)
#endif
         );
         strlcpy(s, t, len);
         strlcat(s, u, len);
      }
         break;
      case MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD_DESC:
         snprintf(s, len,
               "[%s]→[%s]からバーチャルゲームパッドのオーバーレイを変更できます。\n"
               " \n"
               "ボタンのサイズ、不透明度、その他を変更できます。\n"
               " \n"
               "NOTE: デフォルトでは、バーチャルゲームパッドのオーバーレイは\n"
               "メニュー表示中は隠されています。\n"
               "この挙動を変更したければ、\n"
               "[%s]をオフに変更してください。",
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

const char *msg_hash_to_str_jp(enum msg_hash_enums msg) 
{
   switch (msg) 
   {
#include "msg_hash_ja.h"
      default:
         break;
   }

   return "null";
}

const char *msg_hash_get_wideglyph_str_jp(void)
{
   return "漢";
}
