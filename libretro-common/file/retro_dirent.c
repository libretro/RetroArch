/* Copyright  (C) 2010-2017 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_dirent.c).
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

#include <retro_common.h>

#include <boolean.h>
#include <retro_dirent.h>
#include <encodings/utf.h>
#include <compat/strl.h>

#if defined(_WIN32)
#  ifdef _MSC_VER
#    define setmode _setmode
#  endif
#include <sys/stat.h>
#  ifdef _XBOX
#    include <xtl.h>
#    define INVALID_FILE_ATTRIBUTES -1
#  else
#    include <io.h>
#    include <fcntl.h>
#    include <direct.h>
#    include <windows.h>
#  endif
#elif defined(VITA)
#  include <psp2/io/fcntl.h>
#  include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#else
#  if defined(PSP)
#    include <pspiofilemgr.h>
#  endif
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <dirent.h>
#  include <unistd.h>
#endif

#ifdef __CELLOS_LV2__
#include <cell/cell_fs.h>
#endif

#if (defined(__CELLOS_LV2__) && !defined(__PSL1GHT__)) || defined(__QNX__) || defined(PSP)
#include <unistd.h> /* stat() is defined here */
#endif

#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0500 || defined(_XBOX)
#ifndef LEGACY_WIN32
#define LEGACY_WIN32
#endif
#endif

struct RDIR
{
#if defined(_WIN32)
#if defined(LEGACY_WIN32)
   WIN32_FIND_DATA entry;
#else
   WIN32_FIND_DATAW entry;
#endif
   HANDLE directory;
   bool next;
   char path[PATH_MAX_LENGTH];
#elif defined(VITA) || defined(PSP)
   SceUID directory;
   SceIoDirent entry;
#elif defined(__CELLOS_LV2__)
   CellFsErrno error;
   int directory;
   CellFsDirent entry;
#else
   DIR *directory;
   const struct dirent *entry;
#endif
};

struct RDIR *retro_opendir(const char *name)
{
#if defined(_WIN32)
   char path_buf[1024];
   char *path_local   = NULL;
   wchar_t *path_wide = NULL;
   unsigned path_len;
#endif
   struct RDIR *rdir  = (struct RDIR*)calloc(1, sizeof(*rdir));

   if (!rdir)
      return NULL;

#if defined(_WIN32)
   (void)path_wide;
   (void)path_local;

   path_buf[0] = '\0';
   path_len = strlen(name);

   /* Non-NT platforms don't like extra slashes in the path */
   if (name[path_len - 1] == '\\')
      snprintf(path_buf, sizeof(path_buf), "%s*", name);
   else
      snprintf(path_buf, sizeof(path_buf), "%s\\*", name);
#if defined(LEGACY_WIN32)
   path_local      = utf8_to_local_string_alloc(path_buf);
   rdir->directory = FindFirstFile(path_local, &rdir->entry);

   if (path_local)
      free(path_local);
#else
   path_wide       = utf8_to_utf16_string_alloc(path_buf);
   rdir->directory = FindFirstFileW(path_wide, &rdir->entry);

   if (path_wide)
      free(path_wide);
#endif
#elif defined(VITA) || defined(PSP)
   rdir->directory = sceIoDopen(name);
#elif defined(_3DS)
   rdir->directory = (name && *name)? opendir(name) : NULL;
   rdir->entry     = NULL;
#elif defined(__CELLOS_LV2__)
   rdir->error = cellFsOpendir(name, &rdir->directory);
#else
   rdir->directory = opendir(name);
   rdir->entry     = NULL;
#endif

   return rdir;
}

bool retro_dirent_error(struct RDIR *rdir)
{
#if defined(_WIN32)
   return (rdir->directory == INVALID_HANDLE_VALUE);
#elif defined(VITA) || defined(PSP)
   return (rdir->directory < 0);
#elif defined(__CELLOS_LV2__)
   return (rdir->error != CELL_FS_SUCCEEDED);
#else
   return !(rdir->directory);
#endif
}

int retro_readdir(struct RDIR *rdir)
{
#if defined(_WIN32)
   if(rdir->next)
#if defined(LEGACY_WIN32)
      return (FindNextFile(rdir->directory, &rdir->entry) != 0);
#else
      return (FindNextFileW(rdir->directory, &rdir->entry) != 0);
#endif

   rdir->next = true;
   return (rdir->directory != INVALID_HANDLE_VALUE);
#elif defined(VITA) || defined(PSP)
   return (sceIoDread(rdir->directory, &rdir->entry) > 0);
#elif defined(__CELLOS_LV2__)
   uint64_t nread;
   rdir->error = cellFsReaddir(rdir->directory, &rdir->entry, &nread);
   return (nread != 0);
#else
   return ((rdir->entry = readdir(rdir->directory)) != NULL);
#endif
}

const char *retro_dirent_get_name(struct RDIR *rdir)
{
#if defined(_WIN32)
#if defined(LEGACY_WIN32)
   char *name_local = local_to_utf8_string_alloc(rdir->entry.cFileName);
   memset(rdir->entry.cFileName, 0, sizeof(rdir->entry.cFileName));
   strlcpy(rdir->entry.cFileName, name_local, sizeof(rdir->entry.cFileName));

   if (name_local)
      free(name_local);
#else
   char *name = utf16_to_utf8_string_alloc(rdir->entry.cFileName);
   memset(rdir->entry.cFileName, 0, sizeof(rdir->entry.cFileName));
   strlcpy((char*)rdir->entry.cFileName, name, sizeof(rdir->entry.cFileName));

   if (name)
      free(name);
#endif
   return (char*)rdir->entry.cFileName;
#elif defined(VITA) || defined(PSP) || defined(__CELLOS_LV2__)
   return rdir->entry.d_name;
#else
   return rdir->entry->d_name;
#endif
}

/**
 *
 * retro_dirent_is_dir:
 * @rdir         : pointer to the directory entry.
 * @path         : path to the directory entry.
 *
 * Is the directory listing entry a directory?
 *
 * Returns: true if directory listing entry is
 * a directory, false if not.
 */
bool retro_dirent_is_dir(struct RDIR *rdir, const char *path)
{
#if defined(_WIN32)
   const WIN32_FIND_DATA *entry = (const WIN32_FIND_DATA*)&rdir->entry;
   return entry->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
#elif defined(PSP) || defined(VITA)
   const SceIoDirent *entry = (const SceIoDirent*)&rdir->entry;
#if defined(PSP)
   return (entry->d_stat.st_attr & FIO_SO_IFDIR) == FIO_SO_IFDIR;
#elif defined(VITA)
   return SCE_S_ISDIR(entry->d_stat.st_mode);
#endif
#elif defined(__CELLOS_LV2__)
   CellFsDirent *entry = (CellFsDirent*)&rdir->entry;
   return (entry->d_type == CELL_FS_TYPE_DIRECTORY);
#else
   struct stat buf;
#if defined(DT_DIR)
   const struct dirent *entry = (const struct dirent*)rdir->entry;
   if (entry->d_type == DT_DIR)
      return true;
   /* This can happen on certain file systems. */
   if (!(entry->d_type == DT_UNKNOWN || entry->d_type == DT_LNK))
      return false;
#endif
   /* dirent struct doesn't have d_type, do it the slow way ... */
   if (stat(path, &buf) < 0)
      return false;
   return S_ISDIR(buf.st_mode);
#endif
}

void retro_dirent_include_hidden(struct RDIR *rdir, bool include_hidden)
{
#ifdef _WIN32
   if (include_hidden)
      rdir->entry.dwFileAttributes |= FILE_ATTRIBUTE_HIDDEN;
   else
      rdir->entry.dwFileAttributes &= ~FILE_ATTRIBUTE_HIDDEN;
#endif
}

void retro_closedir(struct RDIR *rdir)
{
   if (!rdir)
      return;

#if defined(_WIN32)
   if (rdir->directory != INVALID_HANDLE_VALUE)
      FindClose(rdir->directory);
#elif defined(VITA) || defined(PSP)
   sceIoDclose(rdir->directory);
#elif defined(__CELLOS_LV2__)
   rdir->error = cellFsClosedir(rdir->directory);
#else
   if (rdir->directory)
      closedir(rdir->directory);
#endif

   free(rdir);
}
