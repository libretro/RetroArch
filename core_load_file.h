/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef CORE_LOAD_FILE_H__
#define CORE_LOAD_FILE_H__

#include <stddef.h>

#include <boolean.h>
#include <dynamic/dylib.h>
#include <retro_common_api.h>

#include "core_signature.h"

RETRO_BEGIN_DECLS

/* Loads the core f (non-NULL) was opened from, path being the resolved
 * absolute path f was opened by (Android: its real path). Android loads
 * from f itself; Windows by path while f, sharing nothing but reads,
 * holds the file; elsewhere by path. f stays open: close it after. */
dylib_t core_load_file(core_file_t *f, const char *path);

/* Apple builds only, where dylib_load opens a .framework folder's
 * binary, <name>.framework/<name>, so the check must too. s must not
 * overlap path. False for other paths. */
bool core_load_framework_binary(const char *path, char *s, size_t len);

RETRO_END_DECLS

#endif
