/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef CORE_INSTALL_CHECK_H__
#define CORE_INSTALL_CHECK_H__

#include <boolean.h>
#include <retro_common_api.h>

#include "core_signature.h"

RETRO_BEGIN_DECLS

/* The check of a core staged for install under the name it is
 * installed as, with its .sig beside it, run by a task a step at a
 * time. None of these touches the store. */
struct core_install_check
{
   struct core_check c;
   struct core_hash h;
   core_file_t *f;
   int step;
   bool hashing;
};

/* False when c.result is final already: ERROR, or OK with checks off.
 * The newest builds it compares are install_path's folder's. */
bool core_install_check_begin(struct core_install_check *ic,
      const core_trust_set_t *s, const char *staged,
      const char *install_path);
/* One chunk; true once the file is read or failed */
bool core_install_check_step(struct core_install_check *ic);
/* After the last step: the result, against staged's .sig */
void core_install_check_end(struct core_install_check *ic,
      const core_trust_set_t *s, const char *staged, unsigned flags);
/* What a check left unended holds; a zeroed one is fine */
void core_install_check_free(struct core_install_check *ic);

/* <src>.sig to <dst>.sig, no further than the longest valid one and a
 * byte, so a longer one stays invalid. False when src has none, leaving
 * dst's as it is, or when the write fails, which can leave a short one
 * or the one there before. */
bool core_install_copy_sig(const char *src, const char *dst);
/* Deletes <path>.sig, if there is one */
void core_install_drop_sig(const char *path);
/* <path>.sig to <path>.sig.old while a core is swapped in, after
 * deleting one a crash left; dropped if it won't move */
void core_install_set_aside_sig(const char *path);
/* The set-aside .sig goes once the core is replaced, or comes back */
void core_install_end_aside_sig(const char *path, bool replaced);
/* Once staged is renamed to install_path: its .sig follows, or the
 * one there goes, so the old core is never left with a new .sig */
void core_install_place_sig(const char *staged, const char *install_path);
/* The installed file's identity into c, as a rename changes ctime;
 * c->record false when it isn't the file that was checked, as it was
 * when the check opened it */
void core_install_identify(const char *install_path, struct core_check *c);
/* A core passed by its trusted hash: staged's .sig, never read, stays
 * only if it verifies on its own */
void core_install_vet_sig(const struct core_check *c, const char *staged);
/* Vets staged's .sig, sets install_path's aside, then staged to
 * install_path and its .sig and identity. A failed rename puts the old
 * .sig back. */
bool core_install_commit(const char *staged, const char *install_path,
      struct core_check *c);

RETRO_END_DECLS

#endif
