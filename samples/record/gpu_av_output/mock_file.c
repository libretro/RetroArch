/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdlib.h>
#include "mock_file.h"
unsigned file_delay, file_writes;
bool file_open_fail, file_push_fail, file_step_fail, file_finish_fail, file_cancel_fail;
record_file_win32_t *record_file_win32_new(const char *path)
{ (void)path; return file_open_fail ? NULL : (record_file_win32_t*)calloc(1, sizeof(record_file_win32_t)); }
int record_file_win32_push(record_file_win32_t *f, const void *data, size_t size)
{
   if (file_push_fail) { f->error = 112; return RECORD_FILE_ERROR; }
   if (f->active) return RECORD_FILE_BUSY;
   f->active = true; f->data = data; f->size = size; f->pending = file_delay;
   file_writes++; return RECORD_FILE_OK;
}
int record_file_win32_step(record_file_win32_t *f)
{
   if (f->pending) { f->pending--; return RECORD_FILE_BUSY; }
   f->active = false; f->data = NULL;
   if (file_step_fail) f->error = 112;
   if (f->error) return RECORD_FILE_ERROR;
   f->bytes += f->size; return RECORD_FILE_OK;
}
bool record_file_win32_cancel(record_file_win32_t *f)
{ if (file_cancel_fail) return false; f->error = 995; return true; }
uint64_t record_file_win32_bytes(const record_file_win32_t *f) { return f->bytes; }
uint32_t record_file_win32_error(const record_file_win32_t *f) { return f ? f->error : 87; }
bool record_file_win32_finish(record_file_win32_t *f)
{ if (file_finish_fail) f->error = 5; return !f->active && !f->error; }
bool record_file_win32_free(record_file_win32_t *f)
{ if (f && f->active) return false; free(f); return true; }
void *record_file_win32_wait_handle(const record_file_win32_t *f)
{ return f && f->active ? (void*)f : NULL; }
