/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#ifndef LIBRETRO_RDVD_VFS_H
#define LIBRETRO_RDVD_VFS_H
#include <formats/rdvd_source.h>
#include <streams/file_stream.h>
#include <rthreads/tpool.h>
RETRO_BEGIN_DECLS
typedef struct rdvd_vfs rdvd_vfs_t;
/* Setup-worker only: binds existing read-only RFILE handles in title segment order. Geometry
 * metadata is copied; borrowed handles stay alive and grant this worker
 * exclusive seek/read access. Requires
 * HAVE_THREADS and real pointer atomics; unsupported builds return NULL. */
rdvd_vfs_t *rdvd_vfs_bind(RFILE *const *files, const uint64_t *sizes, unsigned count);
/* Setup-worker only: opens read-only paths through the configured VFS and
 * measures geometry. Returned adapter owns those handles. No VFS bypass. */
rdvd_vfs_t *rdvd_vfs_open(const char *const *paths, unsigned count);
rdvd_source_backend_t *rdvd_vfs_backend(rdvd_vfs_t *vfs);
/* Worker-only: performs at most one bounded VFS read per call. The caller
 * schedules this on a dedicated I/O worker, never a main-thread task. */
bool rdvd_vfs_service(rdvd_vfs_t *vfs);
/* Teardown-worker only; service must be quiescent. Refuses pending ownership;
 * consumed teardown clears the pointer, including close errors (false).
 * A retained pointer means busy. Borrowed handles stay open. */
bool rdvd_vfs_close(rdvd_vfs_t **vfs);
/* Owner-side: try to schedule one queued read on a dedicated I/O pool.
 * False retains the request for a later retry. Do not destroy the pool or
 * run tpool_help on the main thread while this adapter uses it. Never mix
 * scheduled and manual service; pool lifetime extends through drained jobs. */
bool rdvd_vfs_schedule(rdvd_vfs_t *vfs, tpool_t *pool);
RETRO_END_DECLS
#endif
