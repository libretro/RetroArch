/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#ifndef LIBRETRO_RDVD_VFS_H
#define LIBRETRO_RDVD_VFS_H
#include <formats/rdvd_source.h>
#include <streams/file_stream.h>
RETRO_BEGIN_DECLS
typedef struct rdvd_vfs rdvd_vfs_t;
/* Binds existing read-only RFILE handles in title segment order. Geometry
 * metadata is copied; handles are borrowed and must stay alive. Requires
 * HAVE_THREADS and real pointer atomics; unsupported builds return NULL. */
rdvd_vfs_t *rdvd_vfs_bind(RFILE *const *files, const uint64_t *sizes, unsigned count);
rdvd_source_backend_t *rdvd_vfs_backend(rdvd_vfs_t *vfs);
/* Worker-only: performs at most one bounded VFS read per call. The caller
 * schedules this on a dedicated I/O worker, never a main-thread task. */
bool rdvd_vfs_service(rdvd_vfs_t *vfs);
/* Worker must be quiescent. Refuses outstanding source/queue ownership;
 * successful teardown clears the pointer. Borrowed handles stay open. */
bool rdvd_vfs_close(rdvd_vfs_t **vfs);
RETRO_END_DECLS
#endif
