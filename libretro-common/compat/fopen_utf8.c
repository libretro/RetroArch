/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (fopen_utf8.c).
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

#include <compat/fopen_utf8.h>
#include <encodings/utf.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(__WINRT__)
#include <stdint.h>
#include <windows.h>
#include <fileapifromapp.h>
#include <io.h>
#include <fcntl.h>
#endif

#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0500 || (defined(_XBOX) && !defined(__WINRT__))
#ifndef LEGACY_WIN32
#define LEGACY_WIN32
#endif
#endif

#ifdef _WIN32
#undef fopen

void *fopen_utf8(const char * filename, const char * mode)
{
#if defined(LEGACY_WIN32)
   char * filename_local = utf8_to_local_string_alloc(filename);
   if (filename_local)
   {
      FILE *ret          = fopen(filename_local, mode);
      free(filename_local);
      return ret;
   }
#else
   wchar_t * filename_w  = utf8_to_utf16_string_alloc(filename);
   if (filename_w)
   {
      FILE    *ret       = NULL;
#if defined(__WINRT__)
      HANDLE   file_handle = INVALID_HANDLE_VALUE;
      DWORD    desired_access = 0;
      DWORD    creation_disposition = OPEN_EXISTING;
      int      open_flags = O_BINARY;
      int      fd = -1;
      bool     append = mode && strchr(mode, 'a');
      bool     write = mode && strchr(mode, 'w');
      bool     plus = mode && strchr(mode, '+');
      wchar_t *path = filename_w;

      while (*path)
      {
         if (*path == L'/')
            *path = L'\\';
         path++;
      }

      if (mode && strchr(mode, 'r'))
         desired_access |= GENERIC_READ;
      if (write || append || plus)
         desired_access |= GENERIC_WRITE;
      if (plus)
         desired_access |= GENERIC_READ;

      if (append)
         creation_disposition = OPEN_ALWAYS;
      else if (write)
         creation_disposition = CREATE_ALWAYS;

      if (plus)
         open_flags |= O_RDWR;
      else if (append || write)
         open_flags |= O_WRONLY;
      else
         open_flags |= O_RDONLY;

      if (append)
         open_flags |= O_APPEND;
      if (write || append)
         open_flags |= O_CREAT;
      if (write)
         open_flags |= O_TRUNC;

      file_handle = CreateFile2FromAppW(filename_w, desired_access,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            creation_disposition, NULL);
      if (file_handle != INVALID_HANDLE_VALUE)
      {
         fd = _open_osfhandle((intptr_t)file_handle, open_flags);
         if (fd != -1)
            ret = _fdopen(fd, mode);

         if (!ret)
         {
            if (fd != -1)
               _close(fd);
            else
               CloseHandle(file_handle);
         }
      }
#else
      wchar_t *mode_w    = utf8_to_utf16_string_alloc(mode);
      if (mode_w)
      {
         ret             = _wfopen(filename_w, mode_w);
         free(mode_w);
      }
#endif
      free(filename_w);
      return ret;
   }
#endif
   return NULL;
}
#endif
