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
#include <stdint.h>
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
 * keychain_is_locked:
 *
 * Returns: true when the key file was wrapped on another machine and
 * nothing here opens it until keychain_unlock() is given the
 * passphrase. Nothing may be written in the clear meanwhile: sealed
 * values in the file stay as they are.
 **/
bool keychain_is_locked(void);

/**
 * keychain_has_passphrase:
 *
 * Returns: true when the key file carries a passphrase wrap, so that
 * it can be moved to another machine and opened there.
 **/
bool keychain_has_passphrase(void);

/**
 * keychain_unlock:
 *
 * Opens a locked keychain with @passphrase and wraps its key for this
 * machine, so that it opens by itself from then on.
 *
 * Returns: true when the passphrase was right and the key file could
 * be rewritten.
 **/
bool keychain_unlock(const char *passphrase);

/**
 * keychain_set_passphrase:
 * @passphrase        : New passphrase, or NULL/empty to remove it.
 *
 * Wraps the keychain's key under @passphrase in the key file, next to
 * this machine's wrap. The sealed values do not change, so nothing
 * needs sealing again. Needs a ready keychain.
 *
 * Returns: true when the key file was rewritten.
 **/
bool keychain_set_passphrase(const char *passphrase);

/* The same two operations in three steps, for callers that must not
 * wait on the key derivation (it runs hundreds of milliseconds on a
 * PC, seconds on a handheld):
 *   1. keychain_passphrase_params()  - caller's thread, instant: the
 *      salt and iteration count, read from the key file to unlock
 *      (@for_unlock) or made fresh to set a passphrase;
 *   2. keychain_passphrase_derive()  - any thread: the slow part,
 *      touching nothing of the keychain's own state;
 *   3. keychain_unlock_kek() or keychain_set_passphrase_kek() - the
 *      keychain's thread again, instant. A NULL @kek removes the
 *      passphrase.
 * @psalt is KEYCHAIN_PASS_SALT_SIZE octets, @kek 32. */
#define KEYCHAIN_PASS_SALT_SIZE 16
bool keychain_passphrase_params(bool for_unlock,
      uint8_t *psalt, uint32_t *iterations);
bool keychain_passphrase_derive(const char *passphrase,
      const uint8_t *psalt, uint32_t iterations, uint8_t *kek);
/* Step 2 in slices, for a caller with no thread to give it to: begin,
 * step @n iterations at a time until it returns true, then end, which
 * writes the key to @kek (NULL discards it) and frees the state. */
struct keychain_kdf;
struct keychain_kdf *keychain_kdf_begin(const char *passphrase,
      const uint8_t *psalt, uint32_t iterations);
bool keychain_kdf_step(struct keychain_kdf *k, uint32_t n);
unsigned keychain_kdf_progress(const struct keychain_kdf *k);
void keychain_kdf_end(struct keychain_kdf *k, uint8_t *kek);
bool keychain_unlock_kek(const uint8_t *kek);
bool keychain_set_passphrase_kek(const uint8_t *kek,
      const uint8_t *psalt, uint32_t iterations);

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
