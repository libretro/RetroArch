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

int msg_hash_get_help_ko_enum(enum msg_hash_enums msg, char *s, size_t len)
{
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
        default:
            if (string_is_empty(s))
                strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
            return -1;
    }

    return 0;
}

const char *msg_hash_to_str_ko(enum msg_hash_enums msg) 
{
   switch (msg) 
   {
#include "msg_hash_ko.h"
        default:
            break;
   }

   return "null";
}

const char *msg_hash_get_wideglyph_str_ko(void)
{
   return "메";
}
