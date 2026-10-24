#ifndef RARCH_WIN32_PUMP_H
#define RARCH_WIN32_PUMP_H

#include <float.h>
#include <string.h>
#include <windows.h>
#include <boolean.h>

#ifndef ERROR_ALREADY_FIBER
#define ERROR_ALREADY_FIBER 1280L
#endif

typedef LPVOID (WINAPI *win32_pump_convert_t)(LPVOID, DWORD);
typedef LPVOID (WINAPI *win32_pump_create_t)(SIZE_T, SIZE_T, DWORD,
      LPFIBER_START_ROUTINE, LPVOID);
typedef BOOL (WINAPI *win32_pump_restore_t)(void);
typedef VOID (WINAPI *win32_pump_fiber_t)(LPVOID);

typedef struct win32_pump
{
   void *caller;
   void *messages;
   HWND modal_window;
   bool (*next)(void);
   win32_pump_convert_t convert;
   win32_pump_create_t create;
   win32_pump_restore_t restore;
   win32_pump_fiber_t resume;
   win32_pump_fiber_t destroy;
   unsigned depth;
   unsigned rounding;
   bool converted;
   bool on_messages;
   bool busy;
   bool retiring;
   bool disabled;
} win32_pump_t;

static bool win32_pump_dispatching(const win32_pump_t *p)
{
   return p->on_messages;
}

static void win32_pump_yield(win32_pump_t *p)
{
   p->on_messages = false;
   p->resume(p->caller);
   p->on_messages = true;
}

static VOID WINAPI win32_pump_messages(void *data)
{
   win32_pump_t *p = (win32_pump_t*)data;
   unsigned initial = _controlfp(0, 0) & _MCW_RC;
   _controlfp(p->rounding == _RC_UP ? _RC_DOWN : _RC_UP, _MCW_RC);
   win32_pump_yield(p);
   _controlfp(initial, _MCW_RC);
   for (;;)
   {
      p->busy = true;
      while (!p->retiring && p->next()) { }
      p->busy = false;
      if (!p->depth)
         p->modal_window = NULL;
      win32_pump_yield(p);
   }
}

static bool win32_pump_init(win32_pump_t *p)
{
   if (p->disabled)
      return false;
   if (!p->convert)
   {
      HMODULE module = GetModuleHandleA("kernel32.dll");
      union
      {
         FARPROC proc;
         win32_pump_convert_t convert;
         win32_pump_create_t create;
         win32_pump_restore_t restore;
         win32_pump_fiber_t fiber;
      } symbol;
      symbol.proc = GetProcAddress(module, "ConvertThreadToFiberEx");
      p->convert = symbol.convert;
      symbol.proc = GetProcAddress(module, "CreateFiberEx");
      p->create = symbol.create;
      symbol.proc = GetProcAddress(module, "ConvertFiberToThread");
      p->restore = symbol.restore;
      symbol.proc = GetProcAddress(module, "SwitchToFiber");
      p->resume = symbol.fiber;
      symbol.proc = GetProcAddress(module, "DeleteFiber");
      p->destroy = symbol.fiber;
      if (!p->convert || !p->create || !p->restore
            || !p->resume || !p->destroy)
      {
         p->disabled = true;
         return false;
      }
   }
   if (!p->caller)
   {
      /* Both stacks preserve the x86 floating-point state. */
      p->caller = p->convert(NULL, 1);
      p->converted = p->caller != NULL;
      if (!p->caller && GetLastError() == ERROR_ALREADY_FIBER)
         p->caller = GetCurrentFiber();
      if (!p->caller)
      {
         p->disabled = true;
         return false;
      }
   }
   if (!p->messages)
   {
      p->rounding = _controlfp(0, 0) & _MCW_RC;
      p->messages = p->create(0, 0, 1, win32_pump_messages, p);
      if (p->messages)
      {
         p->on_messages = true;
         p->resume(p->messages);
         p->on_messages = false;
         if ((_controlfp(0, 0) & _MCW_RC) != p->rounding)
         {
            /* Some runtimes ignore FIBER_FLAG_FLOAT_SWITCH. */
            _controlfp(p->rounding, _MCW_RC);
            p->destroy(p->messages);
            p->messages = NULL;
         }
      }
   }
   if (!p->messages)
   {
      if (p->converted && p->restore())
      {
         p->caller = NULL;
         p->converted = false;
      }
      p->disabled = true;
      return false;
   }
   return true;
}

static bool win32_pump_run(win32_pump_t *p, bool (*next)(void))
{
   if (!win32_pump_init(p) || win32_pump_dispatching(p))
      return false;
   p->next = next;
   if (p->depth && IsWindow(p->modal_window))
      RedrawWindow(p->modal_window, NULL, NULL, RDW_INTERNALPAINT);
   p->on_messages = true;
   p->resume(p->messages);
   p->on_messages = false;
   if (!p->busy)
      p->retiring = false;
   return true;
}

static bool win32_pump_enter(win32_pump_t *p, HWND hwnd)
{
   if (!win32_pump_dispatching(p))
      return false;
   if (!p->depth++)
      p->modal_window = hwnd;
   win32_pump_yield(p);
   return true;
}

static void win32_pump_leave(win32_pump_t *p)
{
   if (p->depth && !--p->depth)
      p->modal_window = NULL;
}

static void win32_pump_abort(win32_pump_t *p, HWND hwnd)
{
   if (p->modal_window == hwnd)
      p->depth = 0;
}

static bool win32_pump_paint(win32_pump_t *p, HWND hwnd)
{
   if (!p->depth || p->modal_window != hwnd || !win32_pump_dispatching(p))
      return false;
   /* Generated paint follows input; posted frame messages do not. */
   ValidateRect(hwnd, NULL);
   if (!p->retiring)
      win32_pump_yield(p);
   return true;
}

static bool win32_pump_cancel(win32_pump_t *p)
{
   if (win32_pump_dispatching(p))
      return false;
   if (p->busy)
   {
      p->retiring = true;
      if (IsWindow(p->modal_window))
      {
         SendMessage(p->modal_window, WM_CANCELMODE, 0, 0);
         RedrawWindow(p->modal_window, NULL, NULL, RDW_INTERNALPAINT);
      }
      p->on_messages = true;
      p->resume(p->messages);
      p->on_messages = false;
      if (p->busy)
         return false;
   }
   p->retiring = false;
   p->depth = 0;
   p->modal_window = NULL;
   return true;
}

static bool win32_pump_free(win32_pump_t *p)
{
   if (!win32_pump_cancel(p))
      return false;
   if (p->messages)
   {
      p->destroy(p->messages);
      p->messages = NULL;
   }
   if (p->converted && !p->restore())
      return false;
   memset(p, 0, sizeof(*p));
   return true;
}

#endif
