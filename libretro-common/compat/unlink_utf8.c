#include <compat/unlink_utf8.h>
#include <encodings/utf.h>
#include <boolean.h>

#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0500 || defined(_XBOX)
#ifndef LEGACY_WIN32
#define LEGACY_WIN32
#endif
#endif

#ifdef _WIN32

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <malloc.h>

bool unlink_utf8(const char * filename)
{
#if defined(LEGACY_WIN32)
	bool result = DeleteFileA(filename);
#else
	wchar_t * filename_w = utf8_to_utf16_string_alloc(filename);
	bool result = DeleteFileW(filename_w);
	free(filename_w);
#endif
	return result;
}

#endif
