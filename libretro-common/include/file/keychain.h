/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (keychain.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _LIBRETRO_FILE_KEYCHAIN_H
#define _LIBRETRO_FILE_KEYCHAIN_H

#include <stddef.h>
#include <boolean.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Encryption at rest for the values in retroarch-keychain.cfg.
 *
 * A sealed value is a config-safe ASCII string: the prefix "$kc1$"
 * followed by base64(nonce || ciphertext || tag), ChaCha20-Poly1305
 * under a master key with the setting's own name as associated data,
 * so a blob cannot be moved from one key to another. The master key
 * is HKDF-SHA256(salt = the per-install key file, ikm = the machine
 * identity, info = "retroarch-keychain-v1"): the file that holds the
 * secrets, the file that holds the salt, and the machine all have to
 * be present to recover a value. Machine identity is the Windows
 * MachineGuid, /etc/machine-id on Linux, gethostuuid() on Darwin, and
 * nothing elsewhere - there the key file alone is the key.
 *
 * Values that carry no prefix are plaintext from before sealing was
 * in place; keychain_open() passes them through unchanged, and the
 * next save seals them. */

#define KEYCHAIN_VALUE_PREFIX "$kc1$"

/**
 * keychain_init:
 * @keyfile_path      : Per-install key file (32 random octets, hex).
 *                      Created, owner-only, when missing.
 *
 * Derives the master key. Idempotent; a second call with the same
 * path is a no-op.
 *
 * Returns: true when sealed values can be opened and made, false when
 * the key file could neither be read nor created (the caller keeps
 * values in the clear).
 **/
bool keychain_init(const char *keyfile_path);

bool keychain_is_ready(void);

/**
 * keychain_deinit:
 *
 * Wipes the master key from memory.
 **/
void keychain_deinit(void);

bool keychain_value_is_sealed(const char *value);

/**
 * keychain_seal_alloc:
 *
 * Returns: the sealed form of @plaintext for the setting @id, on the
 * heap (free() it), or NULL when the keychain is not ready, entropy
 * failed or memory ran out.
 **/
char *keychain_seal_alloc(const char *id, const char *plaintext);

/**
 * keychain_open_alloc:
 *
 * Returns: the plaintext of @stored for setting @id, on the heap
 * (free() it); an unsealed @stored is returned as a copy. NULL when
 * the value was sealed on another machine or install, was tampered
 * with, or the keychain is not ready.
 **/
char *keychain_open_alloc(const char *id, const char *stored);

RETRO_END_DECLS

#endif
