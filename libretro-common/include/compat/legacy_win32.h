/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (legacy_win32.h).
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

#ifndef __LIBRETRO_SDK_COMPAT_LEGACY_WIN32_H
#define __LIBRETRO_SDK_COMPAT_LEGACY_WIN32_H

/* LEGACY_WIN32 selects the ANSI (A) Win32 entry points over the wide
 * (W) ones.  It is a statement about the target, not a preference, and
 * there are exactly three ways to earn it.
 *
 * 1. Xbox, where the W entry points are not stubs - they are absent.
 *    The 2002 XDK's WinBase.h declares CreateFileA and #defines
 *    CreateFile to it; there is no CreateFileW anywhere in the header,
 *    nor GetFileAttributesW, FindFirstFileW, DeleteFileW, MoveFileW,
 *    CreateDirectoryW or RemoveDirectoryW.  The only W entry points it
 *    has are string helpers: lstrlenW, lstrcpyW, lstrcatW, lstrcmpW,
 *    lstrcmpiW, lstrcpynW, wsprintfW, wvsprintfW, CharUpperW,
 *    CharLowerW, IsBadStringPtrW, OutputDebugStringW.  A W filesystem
 *    call there does not fail at runtime, it fails to link.
 *
 *    (The CRT is the odd one out: _wopen, _wsopen, _wcreat, _wfopen
 *    and _wstat64 are all declared, and _wopen really does ship in
 *    libcmt.lib.  But the Win32 layer underneath is ANSI-only, so the
 *    branch has to go ANSI as a whole.)
 *
 * 2. Windows 95/98/Me, where the W entry points exist but are stubs
 *    that fail with ERROR_CALL_NOT_IMPLEMENTED - the reason Microsoft
 *    shipped MSLU/unicows.dll at all.  Only a small documented set
 *    (the lstr* family, MultiByteToWideChar, WideCharToMultiByte, the
 *    text-out calls) actually works.
 *
 *    Detected by _WIN32_WINDOWS, which is the macro that means "this
 *    build targets 9x" - w32api.h spells out the scale: Windows95
 *    0x0400, Windows98 0x0410, WindowsME 0x0500 - or by HAVE_WIN9X,
 *    for build systems that would rather say it in one place.
 *
 * 3. Anything a build forces with -DLEGACY_WIN32.
 *
 * WinRT/UWP is never legacy, Xbox or not: the ANSI entry points are
 * outside the app API partition there, so it needs the W path.
 *
 * What this deliberately does NOT key off is _WIN32_WINNT, which is
 * what six separate copies of this test used to do:
 *
 *    #if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0500 || defined(_XBOX)
 *
 * That is the NT scale, and reading it as a 9x test was wrong in both
 * directions.  It caught NT 3.51 and NT 4.0 targets, which are
 * Unicode-native - W is the real entry point on NT and A is the
 * wrapper - so it forced ANSI on the one family that never needed it,
 * discarding every path not representable in the ANSI codepage to do
 * it.  And it missed 9x unless someone happened to set an NT macro
 * low, which a 9x build has no reason to do: mingw-w64 does not
 * predefine _WIN32_WINNT, and sdkddkver.h defaults it to 0x0A00 when
 * the build is silent, so the default configuration took the W path on
 * the exact platform the test existed to protect.
 *
 * The two scales even collide: _WIN32_WINDOWS 0x0500 is Windows Me,
 * while _WIN32_WINNT 0x0500 is Windows 2000.
 */

#if !defined(LEGACY_WIN32) && defined(_WIN32) && !defined(__WINRT__)
#if defined(_XBOX) || defined(_WIN32_WINDOWS) || defined(HAVE_WIN9X)
#define LEGACY_WIN32
#endif
#endif

/* Runtime selection, for the one binary that has to be both.
 *
 * Everything above is compile-time: a build is ANSI or it is wide, and
 * a 9x-capable executable gives up Unicode paths on NT to get there.
 * HAVE_WIN9X_RUNTIME compiles both branches and picks per call from a
 * cached predicate instead.
 *
 * Opt-in, because it is not free: both branches in the binary, and a
 * branch the other configurations do not execute.  A build that does
 * not ask keeps exactly the compile-time selection above and pays
 * nothing - not the code, not the test.
 *
 * Never available where one of the two branches cannot exist:
 *
 *   _XBOX     - the W entry points are absent, so the wide branch
 *               would not link, never mind not run.
 *   __WINRT__ - the ANSI entry points are outside the app API
 *               partition, so the legacy branch would not compile.
 *   LEGACY_WIN32 forced - the build has already said what it wants.
 *
 * The predicate is inline in this header on purpose: a new .c file in
 * libretro-common is a line every consuming repo has to add to its own
 * build, for a feature almost none of them want.  One cached int per
 * translation unit costs a probe each rather than one, which is 16 us
 * of startup in the only configuration that compiles it at all.
 */
#if defined(HAVE_WIN9X_RUNTIME) && defined(_WIN32) \
      && !defined(_XBOX) && !defined(__WINRT__) && !defined(LEGACY_WIN32)
#define LEGACY_WIN32_RUNTIME 1
#endif

#ifdef LEGACY_WIN32_RUNTIME
#include <retro_inline.h>

/* _WINDOWS_ is the include guard both the Microsoft and the mingw-w64
 * windows.h define, so this pulls it in only where a translation unit
 * has not already. */
#ifndef _WINDOWS_
#include <windows.h>
#endif

/* Non-zero when the W entry points are not usable, i.e. on 95/98/Me.
 *
 * A feature probe rather than a version check.  GetVersionEx would
 * work - dwPlatformId is VER_PLATFORM_WIN32_WINDOWS only on 9x, and
 * that field is not touched by the version lie a manifest-less process
 * gets on 8.1 and up - but it answers the question next to the one
 * being asked.  What matters is not which Windows this is, it is
 * whether the W entry points do anything, and that is directly
 * observable: on 9x they are stubs that set ERROR_CALL_NOT_IMPLEMENTED.
 * Asking the thing itself also credits a wrapper that implements them
 * - MSLU/unicows, wine, anything later - rather than talking past it.
 *
 * GetFileAttributesW on "." is the cheapest probe there is: no handle,
 * no allocation, a path that exists everywhere.  Its return value is
 * discarded, since INVALID_FILE_ATTRIBUTES is a perfectly good answer
 * from a real implementation in some sandbox; only the error code
 * carries the signal.
 *
 * The cache is a plain int with no interlock.  Two threads racing both
 * call the same stub, compute the same value and store it; losing the
 * race costs a second probe and nothing else. */
static INLINE int retro_win32_is_legacy(void)
{
   static int cached = -1;

   if (cached < 0)
   {
      SetLastError(NO_ERROR);
      GetFileAttributesW(L".");
      cached = (GetLastError() == ERROR_CALL_NOT_IMPLEMENTED) ? 1 : 0;
   }

   return cached;
}
#endif

/* Migration aid for out-of-tree builds that expressed "old Windows"
 * the only way this used to understand.  Silent for anything that has
 * already said what it means, and it never changes the outcome - the
 * decision above is already made. */
#if !defined(LEGACY_WIN32) && !defined(LEGACY_WIN32_NO_MIGRATION_NOTE)
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0500 && !defined(_XBOX) && !defined(__WINRT__)
#if defined(_MSC_VER)
#pragma message("libretro-common: a low _WIN32_WINNT no longer implies the ANSI Win32 path. NT is Unicode-native; if this target runs on Windows 95/98/Me, define _WIN32_WINDOWS (or HAVE_WIN9X).")
#elif defined(__GNUC__)
#warning "libretro-common: a low _WIN32_WINNT no longer implies the ANSI Win32 path. NT is Unicode-native; if this target runs on Windows 95/98/Me, define _WIN32_WINDOWS (or HAVE_WIN9X)."
#endif
#endif
#endif

#endif
