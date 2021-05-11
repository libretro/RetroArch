/**************************************************************

    resync_windows.cpp - Windows device change notifying helper

    ---------------------------------------------------------

    Switchres   Modeline generation engine for emulation

    License     GPL-2.0+
    Copyright   2010-2021 Chris Kennedy, Antonio Giner,
                          Alexandre Wodarczyk, Gil Delescluse

 **************************************************************/

#include <functional>
#include "resync_windows.h"
#include "log.h"

GUID GUID_DEVINTERFACE_MONITOR = { 0xe6f07b5f, 0xee97, 0x4a90, 0xb0, 0x76, 0x33, 0xf5, 0x7b, 0xf4, 0xea, 0xa7 };

//============================================================
//  resync_handler::resync_handler
//============================================================

resync_handler::resync_handler()
{
	m_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	CreateThread(NULL, 0, handler_thread, (LPVOID)this, 0, &my_thread);
}

//============================================================
//  resync_handler::~resync_handler
//============================================================

resync_handler::~resync_handler()
{
	SendMessage(m_hwnd, WM_CLOSE, 0, 0);
	if (m_event) CloseHandle(m_event);
}

//============================================================
//  resync_handler::handler_thread
//============================================================

DWORD WINAPI resync_handler::handler_thread(LPVOID lpParameter)
{
	return ((resync_handler *)lpParameter)->handler_thread_wt();
}

DWORD resync_handler::handler_thread_wt()
{
	WNDCLASSEX wc;
	MSG msg;
	HINSTANCE hinst = GetModuleHandle(NULL);

	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = this->resync_wnd_proc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.cbWndExtra = 0;
	wc.cbClsExtra = 0;
	wc.hInstance = hinst;
	wc.hbrBackground = 0;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "resync_handler";
	wc.hIcon = NULL;
	wc.hIconSm = wc.hIcon;
	wc.hCursor = LoadCursor(NULL, IDC_HAND);

	RegisterClassEx(&wc);

	m_hwnd = CreateWindowEx(0, "resync_handler", NULL, WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, NULL, NULL, hinst, NULL);
	SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);

	// Register notifications of display monitor events
	DEV_BROADCAST_DEVICEINTERFACE filter;
	ZeroMemory(&filter, sizeof(filter));
	filter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
	filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
	filter.dbcc_classguid = GUID_DEVINTERFACE_MONITOR;
	HDEVNOTIFY hDeviceNotify = RegisterDeviceNotification(m_hwnd, &filter, DEVICE_NOTIFY_WINDOW_HANDLE);
	if (hDeviceNotify == NULL)
		log_error("Error registering notification\n");

	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return -1;
}

//============================================================
//  resync_handler::wait
//============================================================

void resync_handler::wait()
{
	m_is_notified_1 = false;
	m_is_notified_2 = false;

	auto start = std::chrono::steady_clock::now();

	while (!m_is_notified_1 || !m_is_notified_2)
		WaitForSingleObject(m_event, 10);

	auto end = std::chrono::steady_clock::now();
	log_verbose("resync time elapsed %I64d ms\n", std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count());
}

//============================================================
//  resync_handler::resync_wnd_proc
//============================================================

LRESULT CALLBACK resync_handler::resync_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	resync_handler *me = reinterpret_cast<resync_handler*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	if (me) return me->my_wnd_proc(hwnd, msg, wparam, lparam);

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

LRESULT CALLBACK resync_handler::my_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
		case WM_DEVICECHANGE:
		{
			switch (wparam)
			{
				case DBT_DEVICEARRIVAL:
				{
					log_verbose("Message: DBT_DEVICEARRIVAL\n");
					PDEV_BROADCAST_DEVICEINTERFACE db = (PDEV_BROADCAST_DEVICEINTERFACE) lparam;
					if (db != nullptr)
					{
						if (db->dbcc_classguid == GUID_DEVINTERFACE_MONITOR)
						{
							m_is_notified_1 = true;
							SetEvent(m_event);
						}
					}
					break;
				}
				case DBT_DEVICEREMOVECOMPLETE:
					log_verbose("Message: DBT_DEVICEREMOVECOMPLETE\n");
					break;
				case DBT_DEVNODES_CHANGED:
					log_verbose("Message: DBT_DEVNODES_CHANGED\n");
					m_is_notified_2 = true;
					SetEvent(m_event);
					break;
				default:
					log_verbose("Message: WM_DEVICECHANGE message received, value %x unhandled.\n", (int)wparam);
					break;
			}
			return 0;
		}
		break;

		case WM_CLOSE:
		{
			PostQuitMessage(0);
			return 0;
		}

		default:
			return DefWindowProc(hwnd, msg, wparam, lparam);
	}
	return 0;
}
