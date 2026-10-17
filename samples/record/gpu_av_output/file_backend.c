/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef _WIN32
#include "../../../record/record_file_win32.c"
#else
/* Test-only stdio sink: Linux has no production overlapped Win32 writer. */
#include <stdio.h>
#include <stdlib.h>
#include "../../../record/record_file_win32.h"
struct record_file_win32 { FILE *out; const void *data; size_t size; uint32_t error; bool active; };
record_file_win32_t *record_file_win32_new(const char *path)
{
   record_file_win32_t *f = (record_file_win32_t*)calloc(1, sizeof(*f));
   if (!f) return NULL;
   f->out = fopen(path, "wb");
   if (!f->out) { free(f); return NULL; }
   return f;
}
int record_file_win32_push(record_file_win32_t *f, const void *data, size_t size)
{
   if (f->active) return RECORD_FILE_BUSY;
   f->data = data; f->size = size; f->active = true; return RECORD_FILE_OK;
}
int record_file_win32_step(record_file_win32_t *f)
{
   f->active = false;
   if (fwrite(f->data, 1, f->size, f->out) != f->size) f->error = 112;
   f->data = NULL; return f->error ? RECORD_FILE_ERROR : RECORD_FILE_OK;
}
void *record_file_win32_wait_handle(const record_file_win32_t *f)
{ (void)f; return NULL; }
bool record_file_win32_cancel(record_file_win32_t *f) { f->error = 995; return true; }
uint64_t record_file_win32_bytes(const record_file_win32_t *f) { (void)f; return 0; }
uint32_t record_file_win32_error(const record_file_win32_t *f) { return f ? f->error : 87; }
bool record_file_win32_finish(record_file_win32_t *f)
{ if (f->active || fflush(f->out)) return false; return true; }
bool record_file_win32_free(record_file_win32_t *f)
{ if (f && f->active) return false; if (f) { fclose(f->out); free(f); } return true; }
#endif
