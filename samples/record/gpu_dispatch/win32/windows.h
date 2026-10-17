/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
/* Test-only Win32 scheduling shim. */
#ifndef DISPATCH_TEST_WINDOWS_H
#define DISPATCH_TEST_WINDOWS_H
#include <stddef.h>
#include <stdint.h>
typedef void *HANDLE;
typedef uint32_t DWORD;
typedef int BOOL;
#define WINAPI
#define TRUE 1
#define FALSE 0
#define INFINITE 0xffffffffu
#define WAIT_OBJECT_0 0
#define WAIT_TIMEOUT 258
#define WAIT_FAILED 0xffffffffu
#define ERROR_GEN_FAILURE 31
#define CreateEventA CreateEvent
HANDLE CreateEvent(void*, BOOL, BOOL, const char*);
HANDLE CreateThread(void*, size_t, DWORD (*)(void*), void*, DWORD, DWORD*);
BOOL SetEvent(HANDLE);
BOOL ResetEvent(HANDLE);
BOOL CloseHandle(HANDLE);
DWORD WaitForSingleObject(HANDLE, DWORD);
DWORD WaitForMultipleObjects(DWORD, const HANDLE*, BOOL, DWORD);
DWORD GetLastError(void);
void SetLastError(DWORD);
DWORD GetCurrentThreadId(void);
void Sleep(DWORD);
#endif
