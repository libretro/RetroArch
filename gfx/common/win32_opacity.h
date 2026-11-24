/* Window styles are changed only by the thread that owns the window. */
#ifndef WIN32_OPACITY_H
#define WIN32_OPACITY_H

#define WIN32_WM_OPACITY (WM_APP + 0x7f)

#ifdef HAVE_WINDOW_TRANSP
#include <retro_atomic.h>

static retro_atomic_int_t win32_opacity_state;

static bool win32_opacity_apply(HWND hwnd)
{
   unsigned request = (unsigned)retro_atomic_load_acquire_int(&win32_opacity_state);
   unsigned opacity = request & 0xff;
   unsigned alpha   = (request & 0x100) ? 255 : (255 * opacity) / 100;
   LONG_PTR style   = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
   LONG_PTR wanted;

   if (!(request & 0x100) && (request & 0x200) && alpha == 255)
      alpha = 254;
   wanted = alpha < 255 ? style | WS_EX_LAYERED : style & ~WS_EX_LAYERED;
   if (wanted != style)
   {
      SetLastError(0);
      if (!SetWindowLongPtr(hwnd, GWL_EXSTYLE, wanted) && GetLastError())
         return false;
   }
   if (alpha < 255)
      return SetLayeredWindowAttributes(hwnd, 0, (BYTE)alpha, LWA_ALPHA) != 0;
   return true;
}

static bool win32_opacity_request(HWND hwnd, unsigned opacity,
      bool fullscreen, bool vrr_workaround)
{
   DWORD owner = hwnd ? GetWindowThreadProcessId(hwnd, NULL) : 0;
   if (!owner)
      return false;
   retro_atomic_store_release_int(&win32_opacity_state,
         (int)((opacity < 100 ? opacity : 100)
            | (fullscreen ? 0x100 : 0) | (vrr_workaround ? 0x200 : 0)));
   if (owner != GetCurrentThreadId())
      return PostMessage(hwnd, WIN32_WM_OPACITY, 0, 0) != 0;
   return win32_opacity_apply(hwnd);
}
#endif
#endif
