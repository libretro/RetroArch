/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (stdin_stream.c).
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
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#ifndef _XBOX
#include <windows.h>
#endif
#include <direct.h>
#else
#include <unistd.h>
#endif

#include <boolean.h>
#include <retro_environment.h>
#include <streams/stdin_stream.h>

#if (defined(_WIN32) && defined(_XBOX)) || defined(__WINRT__) || !defined(__PSL1GHT__) && defined(__PS3__)
size_t read_stdin(char *buf, size_t size)
{
   /* Not implemented. */
   return 0;
}
#elif defined(_WIN32)
size_t read_stdin(char *buf, size_t size)
{
   DWORD i;
   DWORD has_read = 0;
   DWORD avail    = 0;
   bool echo      = false;
   HANDLE hnd     = GetStdHandle(STD_INPUT_HANDLE);

   if (hnd == INVALID_HANDLE_VALUE)
      return 0;

   /* Check first if we're a pipe
    * (not console). */

   /* If not a pipe, check if we're running in a console. */
   if (!PeekNamedPipe(hnd, NULL, 0, NULL, &avail, NULL))
   {
      INPUT_RECORD recs[256];
      bool has_key   = false;
      DWORD mode     = 0;
      DWORD has_read = 0;

      if (!GetConsoleMode(hnd, &mode))
         return 0;

      if ((mode & (ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT))
            && !SetConsoleMode(hnd,
               mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT)))
         return 0;

      /* Win32, Y U NO SANE NONBLOCK READ!? */
      if (!PeekConsoleInput(hnd, recs,
               sizeof(recs) / sizeof(recs[0]), &has_read))
         return 0;

      for (i = 0; i < has_read; i++)
      {
         /* Very crude, but should get the job done. */
         if (recs[i].EventType == KEY_EVENT &&
               recs[i].Event.KeyEvent.bKeyDown &&
               (isgraph(recs[i].Event.KeyEvent.wVirtualKeyCode) ||
                recs[i].Event.KeyEvent.wVirtualKeyCode == VK_RETURN))
         {
            has_key = true;
            echo    = true;
            avail   = size;
            break;
         }
      }

      if (!has_key)
      {
         FlushConsoleInputBuffer(hnd);
         return 0;
      }
   }

   if (!avail)
      return 0;

   if (avail > size)
      avail = size;

   if (!ReadFile(hnd, buf, avail, &has_read, NULL))
      return 0;

   for (i = 0; i < has_read; i++)
      if (buf[i] == '\r')
         buf[i] = '\n';

   /* Console won't echo for us while in non-line mode,
    * so do it manually ... */
   if (echo)
   {
      HANDLE hnd_out = GetStdHandle(STD_OUTPUT_HANDLE);
      if (hnd_out != INVALID_HANDLE_VALUE)
      {
         DWORD has_written;
         WriteConsole(hnd_out, buf, has_read, &has_written, NULL);
      }
   }

   return has_read;
}
#else
size_t read_stdin(char *buf, size_t size)
{
   size_t has_read = 0;

   while (size)
   {
      ssize_t ret = read(STDIN_FILENO, buf, size);

      if (ret <= 0)
         break;

      buf      += ret;
      has_read += ret;
      size     -= ret;
   }

   return has_read;
}
#endif
