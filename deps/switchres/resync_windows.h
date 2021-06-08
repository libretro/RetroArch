/**************************************************************

    resync_windows.h - Windows device change notifying helper

    ---------------------------------------------------------

    Switchres   Modeline generation engine for emulation

    License     GPL-2.0+
    Copyright   2010-2021 Chris Kennedy, Antonio Giner,
                         Alexandre Wodarczyk, Gil Delescluse

 **************************************************************/

#ifndef __RESYNC_WINDOWS__
#define __RESYNC_WINDOWS__

#include <chrono>
#include <windows.h>
#include <dbt.h>

class resync_handler
{
	public:
		resync_handler();
		~resync_handler();

		void wait();

	private:
		static LRESULT CALLBACK resync_wnd_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
		LRESULT CALLBACK my_wnd_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
		static DWORD WINAPI handler_thread(LPVOID lpParameter);
		DWORD handler_thread_wt();

		HWND m_hwnd;
		DWORD my_thread;
		bool m_is_notified_1;
		bool m_is_notified_2;
		HANDLE m_event;
};

#endif
