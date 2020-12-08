/* Copyright  (C) 2019 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (open_browser.cpp).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <boolean.h>

#include <net/open_browser.h>

#if defined(_WIN32) || defined (_WIN64)
   #include <bits/stdc++.h>
   #include <windows.h>
#else
   #include <stdio.h>
   #include <stdlib.h>

   #define CMD_MAX_LENGTH 2048

   #if defined(QT5GUI)
      #include <QDesktopServices>
      #include <QUrl>
      #include <QString>
   #endif
#endif

bool open_browser(char *url)
{
#if defined(_WIN32) || defined (_WIN64)
   ShellExecute(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
   return true;
#elif defined(__APPLE__)
   char cmd[CMD_MAX_LENGTH];

   snprintf(cmd, CMD_MAX_LENGTH, "open \"%s\"", url);
   int ret = system(cmd);
   if (ret == 0)
   {
      return true;
   }

   return false;
#else
#if defined(QT5GUI)
   QString urlString(url);
   QUrl urlObj(urlString);
   return QDesktopServices::openUrl(urlObj);
#else
   int ret;
   char cmd[CMD_MAX_LENGTH];

   snprintf(cmd, CMD_MAX_LENGTH, "xdg-open \"%s\"", url);
   ret = system(cmd);
   if (ret == 0)
   {
      return true;
   }

   snprintf(cmd, CMD_MAX_LENGTH, "gnome-open \"%s\"", url);
   ret = system(cmd);
   if (ret == 0)
   {
      return true;
   }

   snprintf(cmd, CMD_MAX_LENGTH, "kde-open \"%s\"", url);
   ret = system(cmd);
   if (ret == 0)
   {
      return true;
   }

   snprintf(cmd, CMD_MAX_LENGTH, "exo-open \"%s\"", url);
   ret = system(cmd);
   if (ret == 0)
   {
      return true;
   }

   snprintf(cmd, CMD_MAX_LENGTH, "google-chrome \"%s\"", url);
   ret = system(cmd);
   if (ret == 0)
   {
      return true;
   }

   snprintf(cmd, CMD_MAX_LENGTH, "firefox \"%s\"", url);
   ret = system(cmd);
   if (ret == 0)
   {
      return true;
   }

   snprintf(cmd, CMD_MAX_LENGTH, "opera \"%s\"", url);
   ret = system(cmd);
   if (ret == 0)
   {
      return true;
   }

   snprintf(cmd, CMD_MAX_LENGTH, "chromium-browser \"%s\"", url);
   ret = system(cmd);
   if (ret == 0)
   {
      return true;
   }

   snprintf(cmd, CMD_MAX_LENGTH, "vivaldi \"%s\"", url);
   ret = system(cmd);
   if (ret == 0)
   {
      return true;
   }

   return false;
#endif
#endif
}
