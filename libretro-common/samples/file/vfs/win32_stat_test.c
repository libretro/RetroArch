/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
/* Windows stat normalization contracts; native SDK calls are injected. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#ifndef _WIN32
#define _WIN32 1
#endif
#if PROFILE == 1
#define LEGACY_WIN32
#elif PROFILE == 2 || PROFILE == 3
#define LEGACY_WIN32_RUNTIME
#endif
#if PROFILE == 3
#define _MSC_VER 1200
#endif
#define DWORD mock_dword
#define FILETIME mock_filetime
#define WIN32_FILE_ATTRIBUTE_DATA mock_attributes
#undef _stat64
#undef _stati64
#undef _wstat64
#define _stat64 mock_stat64
#define _stati64 mock_stat64
#define _wstat64 mock_wstat64
#define GetFileAttributesExA mock_attributes_a
#define GetFileAttributesExW mock_attributes_w
#define GetFileExInfoStandard 0
#define FILE_ATTRIBUTE_DIRECTORY 16
#define FILE_ATTRIBUTE_READONLY 1
#define RETRO_VFS_STAT_IS_VALID 1
#define RETRO_VFS_STAT_IS_DIRECTORY 2
#define RETRO_VFS_STAT_IS_READONLY 4
#define PATH_MAX_LENGTH 2048

typedef uint32_t mock_dword;
typedef struct
{
   int64_t seconds;
} mock_filetime;
typedef struct
{
   mock_dword dwFileAttributes;
   mock_filetime ftLastWriteTime;
} mock_attributes;
struct mock_stat64
{
   int64_t st_size;
};
static const char *original, *expected;
static unsigned conversions, releases, attributes, stats;
static int mode, fail_conversion, fail_attributes, fail_stat, dbcs;

int win32_needs_local_encoding(void)
{
   return mode;
}
int64_t vfs_filetime_to_unix(const FILETIME *time)
{
   return time->seconds;
}
char *utf8_to_local_string_alloc(const char *path)
{
   char *native;
   size_t suffix;
   assert(path == original);
   conversions++;
   if (fail_conversion)
      return NULL;
   if (!dbcs)
   {
      native = (char*)malloc(strlen(path) + 1);
      assert(native);
      strcpy(native, path);
      return native;
   }
   suffix = strlen(path) - 6;
   native = (char*)malloc(6 + suffix);
   assert(native);
   memcpy(native, "C:\\\203\\", 5);
   memcpy(native + 5, path + 6, suffix + 1);
   return native;
}
wchar_t *utf8_to_utf16_string_alloc(const char *path)
{
   size_t i, length = strlen(path);
   wchar_t *native;
   assert(path == original && !dbcs);
   conversions++;
   if (fail_conversion)
      return NULL;
   native = (wchar_t*)malloc((length + 1) * sizeof(*native));
   assert(native);
   for (i = 0; i <= length; i++)
      native[i] = (unsigned char)path[i];
   return native;
}
void mock_free(void *memory)
{
   assert(memory);
   releases++;
   free(memory);
}
static void check_wide(const wchar_t *path)
{
   size_t i;
   for (i = 0; expected[i]; i++)
      assert(path[i] == (unsigned char)expected[i]);
   assert(!path[i]);
}
int mock_attributes_a(const char *path, int kind, WIN32_FILE_ATTRIBUTE_DATA *data)
{
   assert(!strcmp(path, expected) && !kind);
   attributes++;
   data->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_READONLY;
   data->ftLastWriteTime.seconds = -123;
   return !fail_attributes;
}
int mock_attributes_w(const wchar_t *path, int kind, WIN32_FILE_ATTRIBUTE_DATA *data)
{
   check_wide(path);
   assert(!kind);
   attributes++;
   data->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_READONLY;
   data->ftLastWriteTime.seconds = -123;
   return !fail_attributes;
}
int mock_stat64(const char *path, struct mock_stat64 *data)
{
   assert(!strcmp(path, expected));
   stats++;
   data->st_size = (int64_t)8589934595.0;
   return fail_stat ? -1 : 0;
}
int mock_wstat64(const wchar_t *path, struct mock_stat64 *data)
{
   check_wide(path);
   stats++;
   data->st_size = (int64_t)8589934595.0;
   return fail_stat ? -1 : 0;
}
#define free mock_free
#include "win32_stat_fragment.h"
#undef free
static void check(const char *path, const char *native)
{
   int64_t size = 3, mtime = 4;
   int result;
   original = path;
   expected = native;
   conversions = releases = attributes = stats = 0;
   result = test_stat(path, &size, &mtime);
   if (!result)
      assert(size == 3 && mtime == 4);
   if (!path || !*path || !*native)
   {
      assert(!result && !conversions && !releases && !attributes && !stats);
      return;
   }
   assert(conversions == 1);
   if (fail_conversion)
      assert(!result && !releases && !attributes && !stats);
   else
   {
      assert(releases == 1 && attributes == 1);
      if (fail_attributes)
         assert(!result && !stats);
      else if (fail_stat)
         assert(!result && stats == 1);
      else
         assert(result == 7 && stats == 1 && size == (int64_t)8589934595.0 && mtime == -123);
   }
}
int main(void)
{
   char *long_path = (char*)malloc(PATH_MAX_LENGTH + 2);
   unsigned run;
   assert(long_path);
   memset(long_path, 'x', PATH_MAX_LENGTH); long_path[PATH_MAX_LENGTH] = '\\';
   long_path[PATH_MAX_LENGTH + 1] = 0;
   for (run = 0; run < 2; run++)
   {
      mode = (int)run;
      check("C:\\folder", "C:\\folder");
      check("C:\\folder\\//", "C:\\folder");
      check("C:\\", "C:\\");
      check("c:\\\\\\", "c:\\");
      check("C:/", "C:");
      check("C:\\/", "C:\\");
      check("\\\\server\\share\\", "\\\\server\\share");
      check("C:relative/", "C:relative");
      check("relative//", "relative");
      check(long_path, long_path);
      check(NULL, "");
      check("", "");
      check("///", "");
      fail_conversion = 1;
      check("file/", "file");
      fail_conversion = 0;
      fail_attributes = 1;
      check("file/", "file");
      fail_attributes = 0;
      fail_stat = 1;
      check("file/", "file");
      fail_stat = 0;
#if PROFILE != 0
      if (mode || PROFILE == 1)
      {
         dbcs = 1;
         check("C:\\\343\202\275\\//", "C:\\\203\\");
         check("C:\\\343\202\275", "C:\\\203\\");
         dbcs = 0;
      }
#endif
   }
   free(long_path);
   puts("PASS Windows stat conversion ownership, roots, separators, DBCS and failure cleanup");
   return 0;
}
