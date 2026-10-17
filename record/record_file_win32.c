/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_file_win32.h"
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) && !defined(__WINRT__) && !defined(_XBOX)
#include <windows.h>
#include <encodings/utf.h>

struct record_file_win32
{
   OVERLAPPED overlapped;
   uint64_t bytes;
   HANDLE handle;
   const unsigned char *data;
   size_t remaining;
   DWORD error;
   bool pending, finished, cancelling;
};

record_file_win32_t *record_file_win32_new(const char *path)
{
   record_file_win32_t *f;
   wchar_t *wide;
   if (!path || !*path)
      return NULL;
   f = (record_file_win32_t*)calloc(1, sizeof(*f));
   if (!f)
      return NULL;
   wide = utf8_to_utf16_string_alloc(path);
   if (!wide)
   {
      free(f);
      return NULL;
   }
   f->handle = CreateFileW(wide, GENERIC_WRITE, FILE_SHARE_READ,
         NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
   free(wide);
   if (f->handle == INVALID_HANDLE_VALUE)
   {
      free(f);
      return NULL;
   }
   if (GetFileType(f->handle) != FILE_TYPE_DISK)
   {
      CloseHandle(f->handle);
      free(f);
      return NULL;
   }
   return f;
}

static bool record_file_win32_issue(record_file_win32_t *f)
{
   memset(&f->overlapped, 0, sizeof(f->overlapped));
   f->overlapped.Offset     = (DWORD)f->bytes;
   f->overlapped.OffsetHigh = (DWORD)(f->bytes >> 32);
   if (!WriteFile(f->handle, f->data, (DWORD)f->remaining,
            NULL, &f->overlapped))
   {
      DWORD error = GetLastError();
      if (error != ERROR_IO_PENDING)
      {
         f->error = error;
         f->data = NULL;
         f->remaining = 0;
         return false;
      }
   }
   f->pending = true;
   return true;
}

int record_file_win32_push(record_file_win32_t *f,
      const void *data, size_t size)
{
   if (!f || f->error || f->finished || !data || !size
         || size > (size_t)0xffffffffu
         || (uint64_t)size > (uint64_t)0x7fffffffffffffff - f->bytes)
      return RECORD_FILE_ERROR;
   if (f->pending)
      return RECORD_FILE_BUSY;
   f->data = (const unsigned char*)data;
   f->remaining = size;
   return record_file_win32_issue(f) ? RECORD_FILE_OK : RECORD_FILE_ERROR;
}

int record_file_win32_step(record_file_win32_t *f)
{
   DWORD count;
   if (!f || f->error)
      return RECORD_FILE_ERROR;
   if (!f->pending)
      return RECORD_FILE_OK;
   if (!GetOverlappedResult(f->handle, &f->overlapped, &count, FALSE))
   {
      DWORD error = GetLastError();
      if (error == ERROR_IO_INCOMPLETE)
         return RECORD_FILE_BUSY;
      f->error = error;
   }
   else if (!count || count > f->remaining)
      f->error = ERROR_WRITE_FAULT;
   else
   {
      f->bytes += count;
      f->data += count;
      f->remaining -= count;
   }
   f->pending = false;
   if (f->cancelling && !f->error)
      f->error = ERROR_OPERATION_ABORTED;
   if (f->error || !f->remaining)
   {
      f->data = NULL;
      f->remaining = 0;
      return f->error ? RECORD_FILE_ERROR : RECORD_FILE_OK;
   }
   return record_file_win32_issue(f) ? RECORD_FILE_BUSY : RECORD_FILE_ERROR;
}

void *record_file_win32_wait_handle(const record_file_win32_t *f)
{
   return f && f->pending ? f->handle : NULL;
}

bool record_file_win32_cancel(record_file_win32_t *f)
{
   DWORD error;
   if (!f || f->finished || f->error)
      return false;
   if (f->cancelling)
      return true;
   if (!f->pending)
   {
      f->error = ERROR_OPERATION_ABORTED;
      return true;
   }
   if (!CancelIoEx(f->handle, &f->overlapped))
   {
      error = GetLastError();
      if (error != ERROR_NOT_FOUND)
         return false;
   }
   f->cancelling = true;
   return true;
}

uint64_t record_file_win32_bytes(const record_file_win32_t *f)
{
   return f ? f->bytes : 0;
}

uint32_t record_file_win32_error(const record_file_win32_t *f)
{
   return f ? (uint32_t)f->error : (uint32_t)ERROR_INVALID_PARAMETER;
}

bool record_file_win32_finish(record_file_win32_t *f)
{
   if (!f || f->pending || f->error)
      return false;
   if (f->finished)
      return true;
   if (!FlushFileBuffers(f->handle) || !CloseHandle(f->handle))
   {
      f->error = GetLastError();
      return false;
   }
   f->handle = INVALID_HANDLE_VALUE;
   f->finished = true;
   return true;
}

bool record_file_win32_free(record_file_win32_t *f)
{
   if (!f)
      return true;
   if (f->pending)
      return false;
   if (f->handle != INVALID_HANDLE_VALUE && !CloseHandle(f->handle))
   {
      if (!f->error)
         f->error = GetLastError();
      return false;
   }
   free(f);
   return true;
}
#else
record_file_win32_t *record_file_win32_new(const char *path)
{ (void)path; return NULL; }
int record_file_win32_push(record_file_win32_t *f, const void *data, size_t size)
{ (void)f; (void)data; (void)size; return RECORD_FILE_ERROR; }
int record_file_win32_step(record_file_win32_t *f)
{ (void)f; return RECORD_FILE_ERROR; }
void *record_file_win32_wait_handle(const record_file_win32_t *f)
{
   (void)f; return NULL;
}

bool record_file_win32_cancel(record_file_win32_t *f)
{ (void)f; return false; }
uint64_t record_file_win32_bytes(const record_file_win32_t *f)
{ (void)f; return 0; }
uint32_t record_file_win32_error(const record_file_win32_t *f)
{ (void)f; return 87; }
bool record_file_win32_finish(record_file_win32_t *f)
{ (void)f; return false; }
bool record_file_win32_free(record_file_win32_t *f)
{ return f == NULL; }
#endif
