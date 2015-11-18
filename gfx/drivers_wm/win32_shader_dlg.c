/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015      - Ali Bouhlel
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

#ifdef _MSC_VER
#pragma comment( lib, "comctl32" )
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500 //_WIN32_WINNT_WIN2K
#endif

#ifndef _WIN32_IE
#define _WIN32_IE 0x0300
#endif

#include <string.h>

#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>

#include <retro_inline.h>

#include "../../driver.h"
#include "../../runloop.h"
#include "../video_context_driver.h"
#include "../video_monitor.h"
#include "win32_shader_dlg.h"

#include "../common/gl_common.h"
#include "../common/win32_common.h"

#define IDI_ICON 1

#define SHADER_DLG_WIDTH                  220
#define SHADER_DLG_MIN_HEIGHT             200
#define SHADER_DLG_MAX_HEIGHT             800
#define SHADER_DLG_CTRL_MARGIN            8
#define SHADER_DLG_CTRL_X                 10
#define SHADER_DLG_CHECKBOX_HEIGHT        15
#define SHADER_DLG_SEPARATOR_HEIGHT       10
#define SHADER_DLG_LABEL_HEIGHT           14
#define SHADER_DLG_TRACKBAR_HEIGHT        22
#define SHADER_DLG_TRACKBAR_LABEL_WIDTH   30

#define SHADER_DLG_CTRL_WIDTH      (SHADER_DLG_WIDTH - 2 * SHADER_DLG_CTRL_X)
#define SHADER_DLG_TRACKBAR_WIDTH  (SHADER_DLG_CTRL_WIDTH - SHADER_DLG_TRACKBAR_LABEL_WIDTH)

enum shader_param_ctrl_type
{
   SHADER_PARAM_CTRL_NONE = 0,
   SHADER_PARAM_CTRL_CHECKBOX,
   SHADER_PARAM_CTRL_TRACKBAR
};

enum
{
   SHADER_DLG_CHECKBOX_ONTOP_ID = GFX_MAX_PARAMETERS,
   SHADER_DLG_CHECKBOX_BUTTON1_ID,
   SHADER_DLG_CHECKBOX_BUTTON2_ID
};

typedef struct
{
   enum shader_param_ctrl_type type;
   union
   {
      struct
      {
         HWND hwnd;
      } checkbox;
      struct
      {
         HWND hwnd;
         HWND label_title;
         HWND label_val;
      } trackbar;
   };
} shader_param_ctrl_t;

typedef struct
{
   HWND hwnd;
   HWND on_top_checkbox;
   HWND separator;
   shader_param_ctrl_t controls[GFX_MAX_PARAMETERS];
   int parameters_start_y;
} shader_dlg_t;

static shader_dlg_t g_shader_dlg = {0};

static INLINE void shader_dlg_refresh_trackbar_label(int index)
{
   char val_buffer[32]         = {0};
   struct video_shader* shader = video_shader_driver_get_current_shader();

   if (floorf(shader->parameters[index].current) == shader->parameters[index].current)
      snprintf(val_buffer, sizeof(val_buffer), "%.0f", shader->parameters[index].current);
   else
      snprintf(val_buffer, sizeof(val_buffer), "%.2f", shader->parameters[index].current);

   SendMessage(g_shader_dlg.controls[index].trackbar.label_val, WM_SETTEXT, 0, (LPARAM)val_buffer);

}

static void shader_dlg_params_refresh(void)
{
   int i;
   struct video_shader* shader = video_shader_driver_get_current_shader();

   for (i = 0; i < GFX_MAX_PARAMETERS; i++)
   {
      if (g_shader_dlg.controls[i].type == SHADER_PARAM_CTRL_NONE)
         break;

      if (g_shader_dlg.controls[i].type == SHADER_PARAM_CTRL_CHECKBOX)
      {
         bool checked = (shader->parameters[i].current == shader->parameters[i].maximum);
         SendMessage(g_shader_dlg.controls[i].checkbox.hwnd, BM_SETCHECK, checked, 0);

      }
      else if (g_shader_dlg.controls[i].type == SHADER_PARAM_CTRL_TRACKBAR)
      {
         shader_dlg_refresh_trackbar_label(i);

         SendMessage(g_shader_dlg.controls[i].trackbar.hwnd, TBM_SETRANGEMIN, (WPARAM)TRUE, (LPARAM)0);
         SendMessage(g_shader_dlg.controls[i].trackbar.hwnd, TBM_SETRANGEMAX, (WPARAM)TRUE,
               (LPARAM)((shader->parameters[i].maximum - shader->parameters[i].minimum) / shader->parameters[i].step));
         SendMessage(g_shader_dlg.controls[i].trackbar.hwnd, TBM_SETPOS, (WPARAM)TRUE,
               (LPARAM)((shader->parameters[i].current - shader->parameters[i].minimum) / shader->parameters[i].step));

      }
   }
}

static void shader_dlg_params_clear(void)
{
   int i;

   for (i = 0; i < GFX_MAX_PARAMETERS; i++)
   {
      if (g_shader_dlg.controls[i].type == SHADER_PARAM_CTRL_NONE)
         break;
      else if (g_shader_dlg.controls[i].type == SHADER_PARAM_CTRL_CHECKBOX)
         DestroyWindow(g_shader_dlg.controls[i].checkbox.hwnd);
      else if (g_shader_dlg.controls[i].type == SHADER_PARAM_CTRL_TRACKBAR)
      {
         DestroyWindow(g_shader_dlg.controls[i].trackbar.label_title);
         DestroyWindow(g_shader_dlg.controls[i].trackbar.label_val);
         DestroyWindow(g_shader_dlg.controls[i].trackbar.hwnd);
      }

      g_shader_dlg.controls[i].type = SHADER_PARAM_CTRL_NONE;
   }
}

void shader_dlg_params_reload(void)
{
   HFONT hFont;
   RECT parent_rect;
   int i, pos_x, pos_y;
   struct video_shader* shader = video_shader_driver_get_current_shader();

   shader_dlg_params_clear();

   if (!shader)
      return;

   if (shader->num_parameters > GFX_MAX_PARAMETERS)
      return;

   hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

   pos_y = g_shader_dlg.parameters_start_y;
   pos_x = SHADER_DLG_CTRL_X;

   for (i = 0; i < (int)shader->num_parameters; i++)
   {
      if ((shader->parameters[i].minimum == 0.0)
            && (shader->parameters[i].maximum == (shader->parameters[i].minimum + shader->parameters[i].step)))
      {
         if ((pos_y + SHADER_DLG_CHECKBOX_HEIGHT + SHADER_DLG_CTRL_MARGIN + 20) > SHADER_DLG_MAX_HEIGHT)
         {
            pos_y = g_shader_dlg.parameters_start_y;
            pos_x += SHADER_DLG_WIDTH;
         }

         g_shader_dlg.controls[i].type = SHADER_PARAM_CTRL_CHECKBOX;
         g_shader_dlg.controls[i].checkbox.hwnd = CreateWindowEx(0, "BUTTON", shader->parameters[i].desc,
               WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, pos_x, pos_y, SHADER_DLG_CTRL_WIDTH, SHADER_DLG_CHECKBOX_HEIGHT,
               g_shader_dlg.hwnd, (HMENU)(size_t)i, NULL, NULL);
         SendMessage(g_shader_dlg.controls[i].checkbox.hwnd, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
         pos_y += SHADER_DLG_CHECKBOX_HEIGHT + SHADER_DLG_CTRL_MARGIN;
      }
      else
      {
         if ((pos_y + SHADER_DLG_LABEL_HEIGHT + SHADER_DLG_TRACKBAR_HEIGHT +
                  SHADER_DLG_CTRL_MARGIN + 20) > SHADER_DLG_MAX_HEIGHT)
         {
            pos_y = g_shader_dlg.parameters_start_y;
            pos_x += SHADER_DLG_WIDTH;
         }

         g_shader_dlg.controls[i].type = SHADER_PARAM_CTRL_TRACKBAR;
         g_shader_dlg.controls[i].trackbar.label_title = CreateWindowEx(0, "STATIC", shader->parameters[i].desc,
               WS_CHILD | WS_VISIBLE | SS_LEFT, pos_x, pos_y, SHADER_DLG_CTRL_WIDTH, SHADER_DLG_LABEL_HEIGHT, g_shader_dlg.hwnd,
               (HMENU)(size_t)i, NULL, NULL);
         SendMessage(g_shader_dlg.controls[i].trackbar.label_title, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));

         pos_y += SHADER_DLG_LABEL_HEIGHT;
         g_shader_dlg.controls[i].trackbar.hwnd = CreateWindowEx(0, TRACKBAR_CLASS, "",
               WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS, pos_x + SHADER_DLG_TRACKBAR_LABEL_WIDTH, pos_y,
               SHADER_DLG_TRACKBAR_WIDTH, SHADER_DLG_TRACKBAR_HEIGHT, g_shader_dlg.hwnd, (HMENU)(size_t)i, NULL, NULL);

         g_shader_dlg.controls[i].trackbar.label_val = CreateWindowEx(0, "STATIC", "", WS_CHILD | WS_VISIBLE | SS_LEFT, pos_x,
               pos_y, SHADER_DLG_TRACKBAR_LABEL_WIDTH, SHADER_DLG_LABEL_HEIGHT, g_shader_dlg.hwnd, (HMENU)(size_t)i, NULL, NULL);
         SendMessage(g_shader_dlg.controls[i].trackbar.label_val, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));

         SendMessage(g_shader_dlg.controls[i].trackbar.hwnd, TBM_SETBUDDY, (WPARAM)TRUE,
               (LPARAM)g_shader_dlg.controls[i].trackbar.label_val);

         pos_y += SHADER_DLG_TRACKBAR_HEIGHT + SHADER_DLG_CTRL_MARGIN;

      }

   }

   if (g_shader_dlg.separator)
      DestroyWindow(g_shader_dlg.separator);

   g_shader_dlg.separator = CreateWindowEx(0, "STATIC", "", SS_ETCHEDHORZ | WS_VISIBLE | WS_CHILD, SHADER_DLG_CTRL_X,
         g_shader_dlg.parameters_start_y - SHADER_DLG_CTRL_MARGIN - SHADER_DLG_SEPARATOR_HEIGHT / 2,
         (pos_x - SHADER_DLG_CTRL_X) + SHADER_DLG_CTRL_WIDTH, SHADER_DLG_SEPARATOR_HEIGHT / 2, g_shader_dlg.hwnd, NULL, NULL,
         NULL);

   shader_dlg_params_refresh();

   GetWindowRect(g_shader_dlg.hwnd, &parent_rect);
   SetWindowPos(g_shader_dlg.hwnd, NULL, 0, 0,
         (pos_x - SHADER_DLG_CTRL_X) + SHADER_DLG_WIDTH,
         (pos_x == SHADER_DLG_CTRL_X) ? pos_y + 30 : SHADER_DLG_MAX_HEIGHT,
         SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

}

static void shader_dlg_update_on_top_state(void)
{
   bool on_top = SendMessage(g_shader_dlg.on_top_checkbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
   SetWindowPos(g_shader_dlg.hwnd, on_top ? HWND_TOPMOST : HWND_NOTOPMOST , 0, 0, 0, 0,
         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void shader_dlg_show(HWND parent_hwnd)
{
   const video_driver_t* vid_drv;

   video_driver_get_ptr(&vid_drv);

   if(vid_drv != &video_gl)
      return;

   if (!IsWindowVisible(g_shader_dlg.hwnd))
   {
      if (parent_hwnd)
      {
         RECT parent_rect;
         GetWindowRect(parent_hwnd, &parent_rect);
         SetWindowPos(g_shader_dlg.hwnd, HWND_TOP, parent_rect.right, parent_rect.top,
               0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
      }
      else
         ShowWindow(g_shader_dlg.hwnd, SW_SHOW);

      shader_dlg_update_on_top_state();

      shader_dlg_params_reload();

   }

   SetFocus(g_shader_dlg.hwnd);
}

static LRESULT CALLBACK ShaderDlgWndProc(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam)
{
   int i, pos;
   struct video_shader* shader = video_shader_driver_get_current_shader();

   switch (message)
   {
      case WM_CREATE:
         break;

      case WM_CLOSE:
      case WM_DESTROY:
      case WM_QUIT:
         ShowWindow(g_shader_dlg.hwnd, 0);
         return 0;

      case WM_COMMAND:
         i = LOWORD(wparam);

         if (i == SHADER_DLG_CHECKBOX_ONTOP_ID)
         {
            shader_dlg_update_on_top_state();
            break;
         }

         if (i >= GFX_MAX_PARAMETERS)
            break;

         if (g_shader_dlg.controls[i].type != SHADER_PARAM_CTRL_CHECKBOX)
            break;

         if (SendMessage(g_shader_dlg.controls[i].checkbox.hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED)
            shader->parameters[i].current = shader->parameters[i].maximum;
         else
            shader->parameters[i].current = shader->parameters[i].minimum;

         break;

      case WM_HSCROLL:
         i = GetWindowLong((HWND)lparam, GWL_ID);

         if (i >= GFX_MAX_PARAMETERS)
            break;

         if (g_shader_dlg.controls[i].type != SHADER_PARAM_CTRL_TRACKBAR)
            break;

         pos = (int)SendMessage(g_shader_dlg.controls[i].trackbar.hwnd, TBM_GETPOS, 0, 0);
         shader->parameters[i].current = shader->parameters[i].minimum + pos * shader->parameters[i].step;

         shader_dlg_refresh_trackbar_label(i);
         break;

   }

   return DefWindowProc(hwnd, message, wparam, lparam);
}

bool win32_shader_dlg_init(void)
{
   static bool inited = false;
   const video_driver_t* vid_drv;
   int pos_y;
   HFONT hFont;

   if (g_shader_dlg.hwnd)
      return true;

   if (!inited)
   {
      WNDCLASSEX wc_shader_dlg = {0};
      INITCOMMONCONTROLSEX comm_ctrl_init = {0};

      comm_ctrl_init.dwSize = sizeof(comm_ctrl_init);
      comm_ctrl_init.dwICC  = ICC_BAR_CLASSES;

      if (!InitCommonControlsEx(&comm_ctrl_init))
         return false;

      wc_shader_dlg.cbSize = sizeof(wc_shader_dlg);
      wc_shader_dlg.style = CS_HREDRAW | CS_VREDRAW | CS_CLASSDC | CS_OWNDC;
      wc_shader_dlg.lpfnWndProc = ShaderDlgWndProc;
      wc_shader_dlg.hInstance = GetModuleHandle(NULL);
      wc_shader_dlg.hCursor = LoadCursor(NULL, IDC_ARROW);
      wc_shader_dlg.lpszClassName = "Shader Dialog";
      wc_shader_dlg.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON));
      wc_shader_dlg.hIconSm = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON), IMAGE_ICON, 16, 16, 0);
      wc_shader_dlg.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);

      if (!RegisterClassEx(&wc_shader_dlg))
         return false;

      inited = true;
   }

   hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

   g_shader_dlg.hwnd = CreateWindowEx(0, "Shader Dialog", "Shader Parameters", WS_POPUPWINDOW | WS_CAPTION, 100, 100,
         SHADER_DLG_WIDTH, SHADER_DLG_MIN_HEIGHT, NULL, NULL, NULL, NULL);

   pos_y = SHADER_DLG_CTRL_MARGIN;
   g_shader_dlg.on_top_checkbox = CreateWindowEx(0, "BUTTON", "Always on Top", BS_AUTOCHECKBOX | WS_VISIBLE | WS_CHILD,
         SHADER_DLG_CTRL_X, pos_y, SHADER_DLG_CTRL_WIDTH, SHADER_DLG_CHECKBOX_HEIGHT, g_shader_dlg.hwnd,
         (HMENU)SHADER_DLG_CHECKBOX_ONTOP_ID, NULL, NULL);
   pos_y +=  SHADER_DLG_CHECKBOX_HEIGHT + SHADER_DLG_CTRL_MARGIN;

   SendMessage(g_shader_dlg.on_top_checkbox, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));

   pos_y +=  SHADER_DLG_SEPARATOR_HEIGHT + SHADER_DLG_CTRL_MARGIN;

   g_shader_dlg.parameters_start_y = pos_y;

   return true;
}
