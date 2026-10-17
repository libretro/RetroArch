/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../record/record_file_win32.h"
#ifdef _WIN32
#include <windows.h>
#include <encodings/utf.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %u: %s\n", (unsigned)__LINE__, #x); return 1; } } while (0)
static DWORD test_error, completion, count, write_error, flush_error, close_error, cancel_error;
static unsigned writes, checks, closes, flushes;
static const void *borrowed;
static uint64_t position;
static BOOL WINAPI test_write(HANDLE h, LPCVOID data, DWORD size, LPDWORD n, LPOVERLAPPED o)
{
   (void)h; (void)n;
   writes++; borrowed = data; count = size;
   position = (uint64_t)o->OffsetHigh << 32 | o->Offset;
   test_error = write_error;
   return !write_error;
}
static BOOL WINAPI test_result(HANDLE h, LPOVERLAPPED o, LPDWORD n, BOOL wait)
{
   (void)h; (void)o;
   if (wait) abort();
   checks++; test_error = completion; *n = count;
   return !completion;
}
static BOOL WINAPI test_flush(HANDLE h)
{ (void)h; flushes++; test_error = flush_error; return !flush_error; }
static BOOL WINAPI test_close(HANDLE h)
{ (void)h; closes++; test_error = close_error; return !close_error; }
static BOOL WINAPI test_cancel(HANDLE h, LPOVERLAPPED o)
{ (void)h; (void)o; test_error = cancel_error; return !cancel_error; }
static DWORD WINAPI test_last_error(void) { return test_error; }
#define CancelIoEx test_cancel
#define WriteFile test_write
#define GetOverlappedResult test_result
#define FlushFileBuffers test_flush
#define CloseHandle test_close
#define GetLastError test_last_error
#define record_file_win32_wait_handle mock_file_wait_handle
#define record_file_win32_cancel mock_file_cancel
#define record_file_win32_new mock_file_new
#define record_file_win32_push mock_file_push
#define record_file_win32_step mock_file_step
#define record_file_win32_bytes mock_file_bytes
#define record_file_win32_error mock_file_error
#define record_file_win32_finish mock_file_finish
#define record_file_win32_free mock_file_free
#include "../../../record/record_file_win32.c"
#undef record_file_win32_wait_handle
#undef record_file_win32_cancel
#undef record_file_win32_new
#undef record_file_win32_push
#undef record_file_win32_step
#undef record_file_win32_bytes
#undef record_file_win32_error
#undef record_file_win32_finish
#undef record_file_win32_free
#undef CancelIoEx
#undef WriteFile
#undef GetOverlappedResult
#undef FlushFileBuffers
#undef CloseHandle
#undef GetLastError
static record_file_win32_t *mock_new(void)
{
   record_file_win32_t *f = (record_file_win32_t*)calloc(1, sizeof(*f));
   if (!f) abort();
   f->handle = (HANDLE)(uintptr_t)1;
   completion = write_error = flush_error = close_error = cancel_error = 0;
   return f;
}
static int faults(void)
{
   record_file_win32_t *f;
   unsigned char data[8] = {0};
   unsigned before;
   f = mock_new();
   CHECK(!mock_file_wait_handle(f) && !mock_file_wait_handle(NULL));
   CHECK(mock_file_push(f, data, 8) == RECORD_FILE_OK);
   CHECK(writes == 1 && borrowed == data);
   CHECK(mock_file_wait_handle(f) == f->handle);
   CHECK(mock_file_push(f, data, 8) == RECORD_FILE_BUSY);
   CHECK(!mock_file_free(f) && !mock_file_finish(f) && !closes && !flushes);
   completion = ERROR_IO_INCOMPLETE; before = checks;
   CHECK(mock_file_step(f) == RECORD_FILE_BUSY && checks == before + 1 && writes == 1);
   completion = 0; count = 3;
   CHECK(mock_file_step(f) == RECORD_FILE_BUSY && writes == 2);
   CHECK(borrowed == data + 3 && position == 3 && mock_file_bytes(f) == 3);
   CHECK(mock_file_step(f) == RECORD_FILE_OK && mock_file_bytes(f) == 8);
   CHECK(!mock_file_wait_handle(f));
   CHECK(mock_file_finish(f) && mock_file_finish(f) && flushes == 1 && closes == 1);
   CHECK(mock_file_push(f, data, 8) == RECORD_FILE_ERROR && mock_file_free(f));
   f = mock_new(); write_error = ERROR_IO_PENDING;
   CHECK(mock_file_push(f, data, 8) == RECORD_FILE_OK);
   completion = ERROR_DISK_FULL;
   CHECK(mock_file_step(f) == RECORD_FILE_ERROR && !f->data && !f->pending);
   CHECK(mock_file_error(f) == ERROR_DISK_FULL && !mock_file_finish(f) && mock_file_free(f));
   f = mock_new(); write_error = ERROR_DISK_FULL;
   CHECK(mock_file_push(f, data, 8) == RECORD_FILE_ERROR && !f->pending && !f->data);
   CHECK(mock_file_free(f));
   f = mock_new(); CHECK(mock_file_push(f, data, 8) == RECORD_FILE_OK); count = 0;
   CHECK(mock_file_step(f) == RECORD_FILE_ERROR && mock_file_error(f) == ERROR_WRITE_FAULT);
   CHECK(mock_file_free(f));
   f = mock_new(); CHECK(mock_file_push(f, data, 8) == RECORD_FILE_OK); count = 9;
   CHECK(mock_file_step(f) == RECORD_FILE_ERROR && mock_file_bytes(f) == 0 && mock_file_free(f));
   f = mock_new(); f->bytes = ((uint64_t)1 << 32) - 2;
   CHECK(mock_file_push(f, data, 8) == RECORD_FILE_OK && position == f->bytes);
   count = 3; CHECK(mock_file_step(f) == RECORD_FILE_BUSY && position == ((uint64_t)1 << 32) + 1);
   CHECK(mock_file_step(f) == RECORD_FILE_OK && mock_file_free(f));
   f = mock_new(); f->bytes = (uint64_t)0x7fffffffffffffff - 2;
   CHECK(mock_file_push(f, data, 8) == RECORD_FILE_ERROR && !f->pending && mock_file_free(f));
   f = mock_new(); CHECK(mock_file_push(f, data, 8) == RECORD_FILE_OK); count = 3; write_error = ERROR_DISK_FULL;
   CHECK(mock_file_step(f) == RECORD_FILE_ERROR && mock_file_bytes(f) == 3 && mock_file_free(f));
   f = mock_new(); flush_error = ERROR_WRITE_FAULT;
   CHECK(!mock_file_finish(f) && mock_file_error(f) == ERROR_WRITE_FAULT && mock_file_free(f));
   f = mock_new(); close_error = ERROR_INVALID_HANDLE;
   CHECK(!mock_file_finish(f) && !mock_file_free(f)); close_error = 0;
   CHECK(!mock_file_finish(f) && mock_file_free(f));
   f = mock_new(); CHECK(mock_file_push(f, data, 8) == RECORD_FILE_OK);
   cancel_error = ERROR_ACCESS_DENIED;
   CHECK(!mock_file_cancel(f) && !f->cancelling && !mock_file_free(f));
   cancel_error = 0; CHECK(mock_file_cancel(f) && mock_file_cancel(f));
   completion = ERROR_IO_INCOMPLETE;
   CHECK(mock_file_step(f) == RECORD_FILE_BUSY && !mock_file_free(f));
   completion = ERROR_OPERATION_ABORTED;
   CHECK(mock_file_step(f) == RECORD_FILE_ERROR && !f->data && mock_file_free(f));
   f = mock_new(); CHECK(mock_file_push(f, data, 8) == RECORD_FILE_OK);
   cancel_error = ERROR_NOT_FOUND; CHECK(mock_file_cancel(f)); count = 3; before = writes;
   CHECK(mock_file_step(f) == RECORD_FILE_ERROR && writes == before && mock_file_bytes(f) == 3);
   CHECK(mock_file_error(f) == ERROR_OPERATION_ABORTED && mock_file_free(f));
   f = mock_new(); CHECK(mock_file_cancel(f) && !mock_file_finish(f) && mock_file_free(f));
   puts("PASS: overlapped file ownership, short writes, 64-bit offsets and error teardown");
   return 0;
}
int main(void)
{
   char path[128];
   unsigned char *data;
   record_file_win32_t *f;
   FILE *in;
   wchar_t *wide;
   unsigned i, n;
   int result;
   if (faults()) return 1;
   snprintf(path, sizeof(path), "record-file-%lu-\xc3\xa9.bin", (unsigned long)GetCurrentProcessId());
   data = (unsigned char*)malloc(1024 * 1024); CHECK(data != NULL);
   for (i = 0; i < 1024 * 1024; i++) data[i] = (unsigned char)(i * 37 + i / 257);
   CHECK(!record_file_win32_new(NULL) && !record_file_win32_new(""));
   f = record_file_win32_new(path); CHECK(f != NULL);
   CHECK(!record_file_win32_new(path));
   for (i = 0; i < 4; i++)
   {
      CHECK(record_file_win32_push(f, data, 1024 * 1024) == RECORD_FILE_OK);
      CHECK(record_file_win32_push(f, data, 1) == RECORD_FILE_BUSY);
      CHECK(!record_file_win32_free(f) && !record_file_win32_finish(f));
      for (n = 0; n < 10000; n++)
      {
         result = record_file_win32_step(f);
         if (result != RECORD_FILE_BUSY) break;
         CHECK(WaitForSingleObject(record_file_win32_wait_handle(f), 10000) == WAIT_OBJECT_0);
      }
      CHECK(result == RECORD_FILE_OK);
   }
   CHECK(record_file_win32_bytes(f) == 4 * 1024 * 1024);
   CHECK(record_file_win32_finish(f) && record_file_win32_free(f));
   wide = utf8_to_utf16_string_alloc(path); CHECK(wide != NULL);
   in = _wfopen(wide, L"rb"); CHECK(in != NULL);
   for (i = 0; i < 4 * 1024 * 1024; i++) CHECK(fgetc(in) == data[i % (1024 * 1024)]);
   CHECK(fgetc(in) == EOF && !fclose(in) && !_wremove(wide));
   f = record_file_win32_new(path); CHECK(f != NULL);
   CHECK(record_file_win32_push(f, data, 1024 * 1024) == RECORD_FILE_OK);
   CHECK(record_file_win32_cancel(f) && !record_file_win32_free(f));
   for (n = 0; n < 10000; n++)
   {
      result = record_file_win32_step(f);
      if (result != RECORD_FILE_BUSY) break;
      CHECK(WaitForSingleObject(record_file_win32_wait_handle(f), 10000) == WAIT_OBJECT_0);
   }
   CHECK(result == RECORD_FILE_ERROR && record_file_win32_error(f) == ERROR_OPERATION_ABORTED);
   CHECK(!record_file_win32_finish(f) && record_file_win32_free(f) && !_wremove(wide));
   free(wide); free(data);
   puts("PASS: real overlapped file writes match all 4 MiB after flush/close");
   return 0;
}
#else
int main(void)
{
   if (record_file_win32_wait_handle(NULL) || record_file_win32_new("unused") || record_file_win32_finish(NULL)
         || !record_file_win32_free(NULL)
         || record_file_win32_push(NULL, "x", 1) != RECORD_FILE_ERROR
         || record_file_win32_step(NULL) != RECORD_FILE_ERROR) return 1;
   puts("PASS: non-Windows file writer stub");
   return 0;
}
#endif
