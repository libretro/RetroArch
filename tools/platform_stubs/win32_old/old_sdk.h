/* Force-included by the compile matrix's "win32-old" lanes.
 *
 * The Windows SDKs of the compilers RetroArch still builds with for
 * Windows 95/98 - Visual Studio 2005's among them - declare what came
 * with Windows XP only when _WIN32_WINNT is at least 0x0501. mingw-w64's
 * headers declare some of it whatever _WIN32_WINNT is, so a file that
 * names one of these outside a _WIN32_WINNT check compiles with the
 * cross compiler and breaks the MSVC 2005 job. This takes them away
 * again, so the cross compiler sees what that SDK shows. */
#ifndef WIN32_OLD_SDK_H
#define WIN32_OLD_SDK_H

#include <windows.h>

#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0501
#undef WM_INPUT
#undef WM_INPUT_DEVICE_CHANGE
#endif

#endif
