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

#if defined(_MSC_VER) && !defined(_XBOX) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
#if (_MSC_VER >= 1700)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif
#pragma warning(disable:4566)
#endif

int msg_hash_get_help_vn_enum(enum msg_hash_enums msg, char *s, size_t len)
{
   if (  msg <= MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_END &&
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
         case RARCH_SLOWMOTION_HOLD_KEY:
            snprintf(s, len,
                  "Nhấn để xem chậm.");
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
         case RARCH_BSV_RECORD_TOGGLE:
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
      case MENU_ENUM_LABEL_MENU_TOGGLE:
         snprintf(s, len,
               "Bật/tắt menu.");
         break;
      case MENU_ENUM_LABEL_VOLUME_UP:
         snprintf(s, len,
               "Tăng âm lượng.");
         break;
      case MENU_ENUM_LABEL_VOLUME_DOWN:
         snprintf(s, len,
               "Giảm âm lượng.");
         break;
      case MENU_ENUM_LABEL_SCREENSHOT:
         snprintf(s, len,
               "Chụp ảnh màn hình.");
         break;
      case MENU_ENUM_LABEL_VIDEO_FULLSCREEN:
         snprintf(s, len, "Bật/tắt chế độ toàn màn hình.");
         break;
      case MENU_ENUM_LABEL_SHADER_PREV:
         snprintf(s, len,
               "Áp dụng previous shader in thư mục.");
         break;
      case MENU_ENUM_LABEL_SHADER_NEXT:
         snprintf(s, len,
               "Áp dụng next shader in thư mục.");
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
      #include "msg_hash_vn.h"
      default:
         break;
   }

   return "null";
}
