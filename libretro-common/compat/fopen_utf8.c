#include <compat/fopen_utf8.h>
#include <encodings/utf.h>
#include <stdlib.h>

#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0500 || defined(_XBOX)
#ifndef LEGACY_WIN32
#define LEGACY_WIN32
#endif
#endif

#ifdef _WIN32
#undef fopen

FILE* fopen_utf8(const char * filename, const char * mode)
{
#if defined(_XBOX)
   return fopen(filename, mode);
#elif defined(LEGACY_WIN32)
   FILE             *ret = NULL;
   char * filename_local = utf8_to_local_string_alloc(filename);

   if (!filename_local)
      return NULL;
   ret = fopen(filename_local, mode);
   if (filename_local)
      free(filename_local);
   return ret;
#else
   wchar_t * filename_w = utf8_to_utf16_string_alloc(filename);
   wchar_t * mode_w = utf8_to_utf16_string_alloc(mode);
   FILE* ret = _wfopen(filename_w, mode_w);
   free(filename_w);
   free(mode_w);
   return ret;
#endif
}
#endif
