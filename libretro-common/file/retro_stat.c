/* Copyright  (C) 2010-2015 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_stat.c).
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if defined(_WIN32)
#ifdef _MSC_VER
#define setmode _setmode
#endif
#ifdef _XBOX
#include <xtl.h>
#define INVALID_FILE_ATTRIBUTES -1
#else
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#include <windows.h>
#endif
#elif defined(VITA)
#define SCE_ERROR_ERRNO_EEXIST 0x80010011
#include <psp2/io/fcntl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#if defined(PSP)
#include <pspkernel.h>
#endif

#ifdef __HAIKU__
#include <kernel/image.h>
#endif

#if defined(__CELLOS_LV2__)
#include <cell/cell_fs.h>
#endif

#if defined(VITA)
#define FIO_S_ISDIR PSP2_S_ISDIR
#endif

#if (defined(__CELLOS_LV2__) && !defined(__PSL1GHT__)) || defined(__QNX__) || defined(PSP)
#include <unistd.h> /* stat() is defined here */
#endif

#include <retro_miscellaneous.h>
#include <boolean.h>

enum stat_mode
{
   IS_DIRECTORY = 0,
   IS_CHARACTER_SPECIAL,
   IS_VALID
};

static bool path_stat(const char *path, enum stat_mode mode, int32_t *size)
{
#if defined(VITA) || defined(PSP)
   SceIoStat buf;
   char *tmp = strdup(path);
   size_t len = strlen(tmp);
   if (tmp[len-1] == '/')
      tmp[len-1]='\0';

   if (sceIoGetstat(tmp, &buf) < 0)
   {
      free(tmp);
      return false;
   }
   free(tmp);

#elif defined(__CELLOS_LV2__)
    CellFsStat buf;
    if (cellFsStat(path, &buf) < 0)
       return false;
#elif defined(_WIN32)
   WIN32_FILE_ATTRIBUTE_DATA file_info;
   GET_FILEEX_INFO_LEVELS fInfoLevelId = GetFileExInfoStandard;
   DWORD ret = GetFileAttributesEx(path, fInfoLevelId, &file_info);
   if (ret == 0)
      return false;
#else
   struct stat buf;
   if (stat(path, &buf) < 0)
      return false;
#endif

#if defined(_WIN32)
   if (size)
      *size = file_info.nFileSizeLow;
#else
   if (size)
      *size = buf.st_size;
#endif

   switch (mode)
   {
      case IS_DIRECTORY:
#if defined(VITA) || defined(PSP)
         return FIO_S_ISDIR(buf.st_mode);
#elif defined(__CELLOS_LV2__)
         return ((buf.st_mode & S_IFMT) == S_IFDIR);
#elif defined(_WIN32)
         return (file_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
#else
         return S_ISDIR(buf.st_mode);
#endif
      case IS_CHARACTER_SPECIAL:
#if defined(VITA) || defined(PSP) || defined(__CELLOS_LV2__) || defined(_WIN32)
         return false;
#else
         return S_ISCHR(buf.st_mode);
#endif
      case IS_VALID:
         return true;
   }

   return false;
}

/**
 * path_is_directory:
 * @path               : path
 *
 * Checks if path is a directory.
 *
 * Returns: true (1) if path is a directory, otherwise false (0).
 */
bool path_is_directory(const char *path)
{
   return path_stat(path, IS_DIRECTORY, NULL);
}

bool path_is_character_special(const char *path)
{
   return path_stat(path, IS_CHARACTER_SPECIAL, NULL);
}

bool path_is_valid(const char *path)
{
   return path_stat(path, IS_VALID, NULL);
}

int32_t path_get_size(const char *path)
{
   int32_t filesize = 0;
   if (path_stat(path, IS_VALID, &filesize))
      return filesize;

   return -1;
}

/**
 * path_mkdir_norecurse:
 * @dir                : directory
 *
 * Create directory on filesystem.
 *
 * Returns: true (1) if directory could be created, otherwise false (0).
 **/
bool mkdir_norecurse(const char *dir)
{
   int ret;
#if defined(_WIN32)
   ret = _mkdir(dir);
#elif defined(IOS)
   ret = mkdir(dir, 0755);
#elif defined(VITA) || defined(PSP)
   ret = sceIoMkdir(dir, 0777);
#else
   ret = mkdir(dir, 0750);
#endif
   /* Don't treat this as an error. */
#if defined(VITA)
   if ((ret == SCE_ERROR_ERRNO_EEXIST) && path_is_directory(dir))
      ret = 0;
#elif defined(PSP) || defined(_3DS)
   if ((ret == -1) && path_is_directory(dir))
      ret = 0;
#else 
   if (ret < 0 && errno == EEXIST && path_is_directory(dir))
      ret = 0;
#endif
   if (ret < 0)
      printf("mkdir(%s) error: %s.\n", dir, strerror(errno));
   return ret == 0;
}
