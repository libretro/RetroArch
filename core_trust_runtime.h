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

/* The Security Warning: one request at a time */
enum core_warning_kind
{
   CORE_WARNING_INSTALL = 0,
   CORE_WARNING_LOAD
};

enum core_warning_choice
{
   CORE_WARNING_CANCEL = 0,
   CORE_WARNING_INSTALL_AND_TRUST,
   CORE_WARNING_LOAD_ONCE,
   CORE_WARNING_ALWAYS_TRUST
};

#define CORE_WARNING_CHOICES_MAX 3

typedef void (*core_warning_cb_t)(enum core_warning_choice choice,
      const struct core_check *c, const char *core_path, void *userdata);

/* Opens the screen for c, a check that didn't end in OK. True: cb runs
 * once, with the choice, or CANCEL on Back, on a newer push or at
 * deinit, and owns userdata from then on. False (always inside
 * deinit's cancel, and with a menu that can't show it): cb never runs.
 * What cb gets lasts for the call only. The slot is free when cb runs
 * for a choice, Back or deinit, and OK and Back take the screen down
 * before it runs; a newer push runs cb inside itself, with the newer
 * request already in the slot and the screen kept for it. Never push
 * or choose from a CANCEL. */
bool core_warning_push(const struct core_check *c,
      enum core_warning_kind kind, const char *core_path,
      core_warning_cb_t cb, void *userdata);
bool core_warning_pending(enum core_warning_kind *kind,
      enum core_check_result *result);
/* The pending request's own, 0 with none: a choice made on the screen
 * counts only for the request it showed */
unsigned core_warning_serial(void);
/* Cancel first; Cancel only with nothing pending or for ERROR */
unsigned core_warning_choices(enum core_warning_choice *choices);
/* False, changing nothing, for a choice core_warning_choices() doesn't
 * give, and inside the CANCEL a newer push runs */
bool core_warning_choose(enum core_warning_choice choice);
enum msg_hash_enums core_warning_reason(enum core_check_result result);
/* Install and Trust: what is being installed now is what was warned about */
bool core_warning_same_core(const struct core_check *warned,
      const struct core_check *now);

/* The menu's: opens the screen on its next frame */
void menu_core_warning_open(void);
/* The menu's: false when its driver shows no screens (null) */
bool menu_core_warning_can_open(void);

RETRO_END_DECLS

#endif
