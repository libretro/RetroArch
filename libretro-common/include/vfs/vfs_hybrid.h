/* Copyright  (C) 2026 The RetroArch team
 *
 * vfs_hybrid.h: local-first VFS dispatch with frontend fallback.
 *
 * Resolves the standing tension for need_fullpath cores: sandboxed
 * platforms (Android scoped storage) can only reach some content
 * through the frontend VFS, but frontend handles are opaque (no fd,
 * no mmap) and wholesale filestream_vfs_init() taxes every read on
 * platforms that never needed it and disables local mappings.
 *
 * The hybrid dispatches per FILE: local implementation first (real
 * fd, filestream_get_mapped_ptr keeps working), frontend only for
 * URI-shaped paths or, on sandboxed platforms, after a local
 * failure. Installed only when the frontend provides a VFS at all.
 *
 * Adoption is one call at retro_set_environment:
 *     vfs_hybrid_init(environ_cb, log_cb);
 */
#ifndef __LIBRETRO_SDK_VFS_HYBRID_H
#define __LIBRETRO_SDK_VFS_HYBRID_H

#include <libretro.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

void vfs_hybrid_init(retro_environment_t env_cb, retro_log_printf_t log);

RETRO_END_DECLS

#endif
