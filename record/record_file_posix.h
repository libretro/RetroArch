/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_FILE_POSIX_H
#define RECORD_FILE_POSIX_H
#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS
enum record_file_posix_result
{
   RECORD_FILE_POSIX_ERROR = -1,
   RECORD_FILE_POSIX_BUSY,
   RECORD_FILE_POSIX_OK
};
typedef struct record_file_posix record_file_posix_t;
/* Dedicated I/O worker only, including creation, writes and final sync/close.
 * Filesystem calls may block that worker. Caller supplies the thread.
 * Creates a new regular file exclusively; never overwrites or unlinks. */
record_file_posix_t *record_file_posix_new(const char *path);
/* OK borrows bytes through completion/error/cancel. BUSY/ERROR take nothing.
 * No allocations or copies after construction. One write at a time. */
int record_file_posix_push(record_file_posix_t *file, const void *data, size_t size);
/* At most one 64 KiB write. Handles short writes and transient interruptions
 * across calls. Terminal errors release the borrow and retain errno. */
int record_file_posix_step(record_file_posix_t *file);
/* Same I/O worker, after its active write returns. Discards the borrow. */
bool record_file_posix_cancel(record_file_posix_t *file);
uint64_t record_file_posix_bytes(const record_file_posix_t *file);
int record_file_posix_error(const record_file_posix_t *file);
/* Refuses pending bytes. Success requires sync AND close; failure is sticky.
 * Close consumes the descriptor even on EINTR. */
bool record_file_posix_finish(record_file_posix_t *file);
/* Refuses pending bytes. Quiesced disposal consumes the object, including
 * after finish/error/cancel. Inspect the final error before disposal. */
bool record_file_posix_free(record_file_posix_t *file);
RETRO_END_DECLS
#endif
