/* streams/file_stream.h, as a core includes it, must not bring in the
 * VFS handle's atomics: on MSVC those include <windows.h>, whose
 * declarations collide with macros a core defines first (prboom's
 * LONG(x) breaks winbase.h). */
#include <streams/file_stream.h>

#ifdef __LIBRETRO_SDK_ATOMIC_H
#error "streams/file_stream.h includes retro_atomic.h"
#endif
#ifdef _WINDOWS_
#error "streams/file_stream.h includes windows.h"
#endif

int main(void)
{
   return 0;
}
