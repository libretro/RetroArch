/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef CORE_LOAD_CHECK_H__
#define CORE_LOAD_CHECK_H__

#include <boolean.h>
#include <dynamic/dylib.h>
#include <retro_common_api.h>

#include "core_signature.h"

RETRO_BEGIN_DECLS

/* Every core is checked before it is opened. Main thread, unless a
 * function says otherwise. */

enum core_load_gate
{
   /* No trust store: open the core as a build without the check would */
   CORE_LOAD_GATE_OFF = 0,
   /* Opened from the file that was checked; *lib is NULL when the
    * system loader failed, and dylib_error() says why */
   CORE_LOAD_GATE_OPENED,
   /* Not trusted, or its check still runs: nothing was opened */
   CORE_LOAD_GATE_REFUSED
};

/* path as the loader takes it (Android: the real path) */
enum core_load_gate core_load_check_open(const char *path, dylib_t *lib);
#ifdef _WIN32
/* A \\?\ path ending in '.': the open refuses it as given */
bool core_load_verbatim_dot(const char *path);
#endif
/* The last open of path was refused by the check, not the loader */
bool core_load_check_refused(const char *path);
/* That refusal waits only for the first run's seed */
bool core_load_check_refused_seed(const char *path);
/* The core the last core_load_check_open() opened is now running */
void core_load_check_loaded(void);
void core_load_check_unloaded(void);
/* While the startup load runs before the frame loop: a core the quick
 * check doesn't pass is checked whole, in place */
void core_load_check_set_startup(bool on);
void core_load_check_deinit(void);

RETRO_END_DECLS

#endif
