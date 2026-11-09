/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef CORE_TRUST_RUNTIME_H__
#define CORE_TRUST_RUNTIME_H__

#include <boolean.h>
#include <retro_common_api.h>

#include "core_signature.h"
#include "msg_hash.h"

RETRO_BEGIN_DECLS

/* RetroArch's one trust store. Main thread only, but for
 * core_trust_set_put() on what core_trust_runtime_get() returned. */

/* Before any core is opened: loads the store at path (NULL for
 * core_trust_default_path; on macOS always the one in ~/Library/
 * Application Support/RetroArch; test-key builds' own file beside it).
 * False, and the feature inactive, with no keys at all. Again after
 * true does nothing. */
bool core_trust_runtime_init(const char *path, const char *seed_dir,
      const char *core_ext);
/* Writes what is unsaved, within the exit drain's limit */
void core_trust_runtime_deinit(void);
/* Once on-screen messages can show */
void core_trust_runtime_notify(void);

bool core_trust_runtime_active(void);
/* The store couldn't be read: changes last for this session only */
bool core_trust_runtime_read_only(void);
/* NULL when inactive */
core_trust_set_t *core_trust_runtime_get(void);
/* core_trust_apply() and core_trust_always(), then saved */
bool core_trust_runtime_apply(const struct core_check *c);
bool core_trust_runtime_always(const struct core_check *c);
bool core_trust_runtime_checks_on(void);
/* False, changing nothing, when inactive or read-only */
bool core_trust_runtime_set_checks(bool on);
/* True from startup until the seed has recorded what it found (builds
 * with the load check only) */
bool core_trust_runtime_seed_pending(void);
/* The seed is pending and core_path sits directly in its folder */
bool core_trust_runtime_seed_covers(const char *core_path);

RETRO_END_DECLS

#endif
