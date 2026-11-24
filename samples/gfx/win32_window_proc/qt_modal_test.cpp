#include <QApplication>
#include <QAbstractEventDispatcher>
#include <QEvent>
#include <cstdio>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include "../../../gfx/common/win32_pump.h"
static win32_pump_t qt_modal_pump;

static bool win32_message_pump(bool (*next)(void))
{
   return win32_pump_run(&qt_modal_pump, next);
}
#endif

#include "qt_process_events.inc"

static unsigned failures, delivered;

#define CHECK(test) do { if (!(test)) { \
   std::fprintf(stderr, "FAIL line %u: %s\n", (unsigned)__LINE__, #test); \
   failures++; } } while (0)

class QtModalMarker : public QObject
{
public:
   bool event(QEvent *event) override
   {
      if (event->type() == QEvent::User)
      {
         delivered++;
         return true;
      }
      return QObject::event(event);
   }
};

#ifdef _WIN32
static unsigned enters, native_delivered;

static LRESULT CALLBACK qt_modal_window(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam)
{
   switch (message)
   {
      case WM_ENTERSIZEMOVE:
         enters++;
         CHECK(win32_pump_dispatching(&qt_modal_pump));
         CHECK(win32_pump_enter(&qt_modal_pump, hwnd));
         return 0;
      case WM_EXITSIZEMOVE:
         win32_pump_leave(&qt_modal_pump);
         return 0;
      case WM_APP:
         native_delivered++;
         return 0;
   }
   return DefWindowProc(hwnd, message, wparam, lparam);
}

static bool qt_modal_native_next(void)
{
   MSG message;
   if (!PeekMessage(&message, NULL, 0, 0, PM_REMOVE))
      return false;
   TranslateMessage(&message);
   DispatchMessage(&message);
   return true;
}

static void qt_modal_windows(void)
{
   WNDCLASSA wc = {};
   HWND hwnd;
   unsigned mode;
   wc.lpfnWndProc = qt_modal_window;
   wc.hInstance = GetModuleHandle(NULL);
   wc.lpszClassName = "RetroArchQtModalTest";
   CHECK(RegisterClassA(&wc) != 0);
   hwnd = CreateWindowA(wc.lpszClassName, "Qt modal pump test",
         WS_OVERLAPPEDWINDOW, 0, 0, 320, 240, NULL, NULL, wc.hInstance, NULL);
   CHECK(hwnd != NULL);
   if (!hwnd) return;
   for (mode = 0; mode < 2; mode++)
   {
      unsigned i;
      native_delivered = enters = 0;
      PostMessage(hwnd, WM_ENTERSIZEMOVE, 0, 0);
      PostMessage(hwnd, WM_APP, 0, 0);
      ui_application_qt_process_events();
      CHECK(enters == 1 && qt_modal_pump.depth == 1);
      CHECK(native_delivered == 0);
      PostMessage(hwnd, WM_EXITSIZEMOVE, 0, 0);
      for (i = 0; i < 8 && (qt_modal_pump.depth || !native_delivered); i++)
      {
         if (mode)
            CHECK(win32_message_pump(qt_modal_native_next));
         else
            ui_application_qt_process_events();
      }
      CHECK(native_delivered == 1 && !qt_modal_pump.depth && !qt_modal_pump.busy);
   }
   DestroyWindow(hwnd);
   UnregisterClassA(wc.lpszClassName, wc.hInstance);
   CHECK(win32_pump_free(&qt_modal_pump));
}
#endif

int main(int argc, char **argv)
{
   QApplication app(argc, argv);
   QtModalMarker marker;
   QApplication::postEvent(&marker, new QEvent(QEvent::User));
   ui_application_qt_process_events();
   CHECK(delivered == 1);
#ifdef _WIN32
   if (!win32_pump_init(&qt_modal_pump))
   {
      std::puts("UNAVAILABLE: runtime does not preserve fiber FP state");
      CHECK(win32_pump_free(&qt_modal_pump));
      return failures ? 1 : 77;
   }
   qt_modal_windows();
   qt_modal_pump.disabled = true;
   QApplication::postEvent(&marker, new QEvent(QEvent::User));
   ui_application_qt_process_events();
   CHECK(delivered == 2);
   CHECK(win32_pump_free(&qt_modal_pump));
#endif
   std::printf("Qt event pump: %u failure(s)\n", failures);
   return failures ? 1 : 0;
}
