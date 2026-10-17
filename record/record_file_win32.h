/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_FILE_WIN32_H
#define RECORD_FILE_WIN32_H
#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS

enum record_file_result
{
   RECORD_FILE_ERROR = -1, RECORD_FILE_BUSY, RECORD_FILE_OK
};
typedef struct record_file_win32 record_file_win32_t;

/* One I/O worker owns all calls. Never call from rendering, audio or a task
 * handler that may run on the main thread. Overlapped disk I/O may still block
 * inside Windows. Creates a new file; refuses to overwrite a recording.
 * No thread is created here; the caller supplies scheduling. UTF-8 path. */
record_file_win32_t *record_file_win32_new(const char *path);
/* OK borrows the bytes until step returns OK or ERROR. BUSY/ERROR take
 * nothing. One write at a time; no allocation or copy after startup. */
int record_file_win32_push(record_file_win32_t *file,
      const void *data, size_t size);
/* One completion check and at most one short-write continuation, no waits.
 * Terminal errors release the borrowed bytes and preserve the Win32 error.
 * Idle returns OK. Successful completion advances the completed byte count. */
int record_file_win32_step(record_file_win32_t *file);
/* Requests cancellation without waiting. Keep the object and bytes alive until
 * step returns ERROR, even if cancellation races successful completion. */
/* Worker-only borrowed Win32 completion handle, or NULL without pending I/O.
 * Never close/reset it. Re-query after servicing; no state is consumed here. */
void *record_file_win32_wait_handle(const record_file_win32_t *f);
bool record_file_win32_cancel(record_file_win32_t *file);
uint64_t record_file_win32_bytes(const record_file_win32_t *file);
uint32_t record_file_win32_error(const record_file_win32_t *file);
/* Refuses pending writes. Flush/close run only on the I/O worker, after drain;
 * finish may block in the filesystem. Failure is sticky and never reports
 * successful finalization. free refuses pending I/O, discards on error/abort. */
bool record_file_win32_finish(record_file_win32_t *file);
bool record_file_win32_free(record_file_win32_t *file);
RETRO_END_DECLS
#endif
