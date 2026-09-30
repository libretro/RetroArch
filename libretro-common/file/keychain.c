/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (keychain.c).
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <file/keychain.h>
#include <crypto/crypto.h>
#include <crypto/kdf.h>
#include <encodings/base64.h>
#include <streams/file_stream.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <compat/strl.h>

#if defined(_WIN32) && !defined(__WINRT__) && !defined(_XBOX)
#include <windows.h>
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if !TARGET_OS_IPHONE
/* gethostuuid() has been in libSystem since 10.5 but the SDKs the
 * PowerPC and early Intel jobs build against do not declare it, and
 * a 10.4 libSystem does not have it at all; resolve it at run time
 * so neither the header nor the link line has to know. */
#define KEYCHAIN_HAVE_GETHOSTUUID 1
#include <dlfcn.h>
#include <sys/time.h>
struct timespec;
typedef int (*keychain_gethostuuid_t)(unsigned char *, const struct timespec *);
#endif
#endif

#define KEYCHAIN_SALT_SIZE  32
#define KEYCHAIN_PREFIX_LEN (sizeof(KEYCHAIN_VALUE_PREFIX) - 1)
#define KEYCHAIN_MACHINE_ID_MAX 128

static const char keychain_info[] = "retroarch-keychain-v1";

/* Key file wrap lines (after the salt line; builds that predate them
 * read the salt and stop there):
 *   machine <base64 nonce || wrapped data key || tag>
 *   passphrase <iterations> <base64 pbkdf2 salt || nonce || wrapped || tag>
 * The data key seals the values. Without wrap lines it is the key
 * derived from the salt and the machine identity, as it always was; a
 * passphrase stores that same key wrapped under the passphrase, and a
 * machine line wrapped under this machine's identity, so the file can
 * move to another machine and be opened there with the passphrase. */
#define KEYCHAIN_PASS_ITERS     200000
#define KEYCHAIN_WRAP_SIZE      (AEAD_NONCE_SIZE + AEAD_KEY_SIZE + AEAD_TAG_SIZE)

static const char keychain_wrap_info[]    = "retroarch-keychain-wrap-v1";
static const char keychain_ad_machine[]   = "retroarch-keychain machine";
static const char keychain_ad_passphrase[] = "retroarch-keychain passphrase";

static uint8_t keychain_master[AEAD_KEY_SIZE];
static bool    keychain_ready  = false;
static bool    keychain_locked = false;
static bool    keychain_has_pass = false;
static char   *keychain_path   = NULL;   /* the key file, for rewrites */
static uint8_t keychain_salt[KEYCHAIN_SALT_SIZE];
static char   *keychain_pass_line = NULL; /* kept verbatim across rewrites */

/* Machine identity, best effort; an empty string is a valid answer. */
static size_t keychain_machine_id(char *s, size_t len)
{
   s[0] = '\0';
#if defined(_WIN32) && !defined(__WINRT__) && !defined(_XBOX)
   {
      HKEY  key  = NULL;
      DWORD type = 0;
      DWORD size = (DWORD)len;
      /* A 32-bit process on 64-bit Windows is redirected to the
       * WOW6432Node view, where MachineGuid does not exist; ask for
       * the 64-bit view, and again without the flag for hosts that
       * predate it. */
      REGSAM sam = KEY_QUERY_VALUE | 0x0100 /* KEY_WOW64_64KEY */;
      if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
               "SOFTWARE\\Microsoft\\Cryptography", 0, sam, &key) != ERROR_SUCCESS)
      {
         key = NULL;
         if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                  "SOFTWARE\\Microsoft\\Cryptography", 0,
                  KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
            return 0;
      }
      if (RegQueryValueExA(key, "MachineGuid", NULL, &type,
               (LPBYTE)s, &size) != ERROR_SUCCESS
            || type != REG_SZ || size == 0)
         s[0] = '\0';
      else
         s[(size < len) ? size : len - 1] = '\0';
      RegCloseKey(key);
   }
#elif defined(KEYCHAIN_HAVE_GETHOSTUUID)
   {
      /* IOPlatformUUID without linking IOKit. A host without the
       * symbol falls through to the key-file-only key. */
      unsigned char  uuid[16];
      struct timespec wait;
      keychain_gethostuuid_t fn =
         (keychain_gethostuuid_t)dlsym(RTLD_DEFAULT, "gethostuuid");
      wait.tv_sec  = 0;
      wait.tv_nsec = 0;
      if (fn && fn(uuid, &wait) == 0)
      {
         unsigned i;
         for (i = 0; i < 16 && 2 * i + 3 <= len; i++)
            sprintf(s + 2 * i, "%02x", uuid[i]);
      }
   }
#elif defined(__linux__) && !defined(ANDROID)
   {
      static const char *paths[] = {
         "/etc/machine-id", "/var/lib/dbus/machine-id"
      };
      unsigned i;
      for (i = 0; i < 2 && s[0] == '\0'; i++)
      {
         void   *buf  = NULL;
         int64_t blen = 0;
         if (filestream_read_file(paths[i], &buf, &blen) && buf)
         {
            size_t n = (size_t)blen;
            if (n >= len)
               n = len - 1;
            memcpy(s, buf, n);
            s[n] = '\0';
            string_trim_whitespace(s);
         }
         free(buf);
      }
   }
#endif
   return strlen(s);
}

static bool keychain_load_or_create_salt(const char *path, uint8_t *salt)
{
   void   *buf  = NULL;
   int64_t blen = 0;
   char    hex[KEYCHAIN_SALT_SIZE * 2 + 2];
   unsigned i;

   if (path_is_valid(path)
         && filestream_read_file(path, &buf, &blen) && buf
         && blen >= (int64_t)(KEYCHAIN_SALT_SIZE * 2))
   {
      const char *p = (const char*)buf;
      bool ok = true;
      for (i = 0; i < KEYCHAIN_SALT_SIZE && ok; i++)
      {
         unsigned v = 0;
         char     t[3];
         t[0] = p[2 * i]; t[1] = p[2 * i + 1]; t[2] = '\0';
         if (sscanf(t, "%2x", &v) != 1)
            ok = false;
         salt[i] = (uint8_t)v;
      }
      free(buf);
      if (ok)
         return true;
      /* Unreadable content: do not overwrite what may be someone's
       * key file, refuse instead. */
      return false;
   }
   free(buf);

   if (crypto_random_bytes(salt, KEYCHAIN_SALT_SIZE) != 0)
      return false;
   for (i = 0; i < KEYCHAIN_SALT_SIZE; i++)
      sprintf(hex + 2 * i, "%02x", salt[i]);
   hex[KEYCHAIN_SALT_SIZE * 2]     = '\n';
   hex[KEYCHAIN_SALT_SIZE * 2 + 1] = '\0';
   if (!filestream_write_file(path, hex, KEYCHAIN_SALT_SIZE * 2 + 1))
      return false;
   path_set_private(path);
   return true;
}

/* The key a machine line is wrapped under (@info = keychain_wrap_info)
 * or, for files without wrap lines, the data key itself (@info =
 * keychain_info): both from the salt and this machine's identity. */
static bool keychain_machine_key(const uint8_t *salt, const char *info,
      uint8_t *out)
{
   char  *mid;
   size_t mid_len;
   bool   ok;

   if (!(mid = (char*)malloc(KEYCHAIN_MACHINE_ID_MAX)))
      return false;
   mid_len = keychain_machine_id(mid, KEYCHAIN_MACHINE_ID_MAX);
   if (!mid_len)
   {
      /* No identity on this platform: the key file is the key. */
      strlcpy(mid, "no-machine-id", KEYCHAIN_MACHINE_ID_MAX);
      mid_len = strlen(mid);
   }
   ok = hkdf_sha256(salt, KEYCHAIN_SALT_SIZE, (const uint8_t*)mid, mid_len,
         (const uint8_t*)info, strlen(info), out, AEAD_KEY_SIZE) == 0;
   crypto_memzero(mid, KEYCHAIN_MACHINE_ID_MAX);
   free(mid);
   return ok;
}

/* nonce || AEAD(@kek, data key) || tag, bound to the salt and @ad. */
static bool keychain_wrap(const uint8_t *kek, const char *ad,
      const uint8_t *key, uint8_t *out)
{
   if (crypto_random_bytes(out, AEAD_NONCE_SIZE) != 0)
      return false;
   return aead_encrypt(AEAD_CHACHA20_POLY1305, kek, AEAD_KEY_SIZE,
         out, AEAD_NONCE_SIZE, (const uint8_t*)ad, strlen(ad),
         key, AEAD_KEY_SIZE, out + AEAD_NONCE_SIZE,
         out + AEAD_NONCE_SIZE + AEAD_KEY_SIZE, AEAD_TAG_SIZE) == 0;
}

static bool keychain_unwrap(const uint8_t *kek, const char *ad,
      const uint8_t *in, uint8_t *key)
{
   return aead_decrypt(AEAD_CHACHA20_POLY1305, kek, AEAD_KEY_SIZE,
         in, AEAD_NONCE_SIZE, (const uint8_t*)ad, strlen(ad),
         in + AEAD_NONCE_SIZE, AEAD_KEY_SIZE,
         in + AEAD_NONCE_SIZE + AEAD_KEY_SIZE, AEAD_TAG_SIZE, key) == 0;
}

/* The value of the line starting with @tag (followed by a space) in
 * @text, on the heap, trimmed; NULL when absent. */
static char *keychain_line(const char *text, const char *tag)
{
   size_t      tl = strlen(tag);
   const char *p  = text;
   while (p && *p)
   {
      const char *eol = strchr(p, '\n');
      size_t      ll  = eol ? (size_t)(eol - p) : strlen(p);
      if (ll > tl && strncmp(p, tag, tl) == 0 && p[tl] == ' ')
      {
         char *v = strldup(p + tl + 1, ll - tl);
         if (v)
            string_trim_whitespace(v);
         return v;
      }
      p = eol ? eol + 1 : NULL;
   }
   return NULL;
}

/* Decode a base64 wrap of exactly @want octets into @out. */
static bool keychain_b64_exact(const char *b64, uint8_t *out, size_t want)
{
   int      n    = 0;
   uint8_t *blob = b64 ? unbase64(b64, (int)strlen(b64), &n) : NULL;
   bool     ok   = blob && n == (int)want;
   if (ok)
      memcpy(out, blob, want);
   if (blob)
   {
      crypto_memzero(blob, (size_t)n);
      free(blob);
   }
   return ok;
}

/* Rewrite the key file: the salt, then @machine and the passphrase
 * line when present. Written beside it and renamed over it, so a
 * crash leaves the old file or the new one, never half of one. */
static bool keychain_write_file(const uint8_t *machine)
{
   char    *text;
   char    *tmp;
   char    *b64 = NULL;
   size_t   cap, len = 0, plen;
   unsigned i;
   int      n   = 0;
   bool     ok;

   if (!keychain_path)
      return false;
   if (machine && !(b64 = base64(machine, KEYCHAIN_WRAP_SIZE, &n)))
      return false;
   cap = KEYCHAIN_SALT_SIZE * 2 + 2 + (b64 ? (size_t)n + 10 : 0)
      + (keychain_pass_line ? strlen(keychain_pass_line) + 13 : 0) + 1;
   if (!(text = (char*)malloc(cap)))
   {
      free(b64);
      return false;
   }
   for (i = 0; i < KEYCHAIN_SALT_SIZE; i++)
      sprintf(text + 2 * i, "%02x", keychain_salt[i]);
   len = KEYCHAIN_SALT_SIZE * 2;
   text[len++] = '\n';
   if (b64)
      len += (size_t)sprintf(text + len, "machine %s\n", b64);
   if (keychain_pass_line)
      len += (size_t)sprintf(text + len, "passphrase %s\n", keychain_pass_line);
   free(b64);

   plen = strlen(keychain_path);
   if (!(tmp = (char*)malloc(plen + 5)))
   {
      free(text);
      return false;
   }
   memcpy(tmp, keychain_path, plen);
   memcpy(tmp + plen, ".new", 5);
   ok = filestream_write_file(tmp, text, (int64_t)len);
   crypto_memzero(text, len);
   free(text);
   if (ok)
   {
      path_set_private(tmp);
      ok = filestream_rename(tmp, keychain_path) == 0;
   }
   if (!ok)
      filestream_delete(tmp);
   free(tmp);
   return ok;
}

bool keychain_init(const char *keyfile_path)
{
   void    *buf  = NULL;
   int64_t  blen = 0;
   char    *text = NULL;
   char    *mline;
   uint8_t  kek[AEAD_KEY_SIZE];
   uint8_t  wrap[KEYCHAIN_WRAP_SIZE];
   bool     ok   = false;

   if (keychain_ready)
      return true;
   if (string_is_empty(keyfile_path))
      return false;

   if (!keychain_load_or_create_salt(keyfile_path, keychain_salt))
      return false;

   free(keychain_path);
   keychain_path = strldup(keyfile_path, strlen(keyfile_path) + 1);
   free(keychain_pass_line);
   keychain_pass_line = NULL;
   keychain_locked    = false;

   if (filestream_read_file(keyfile_path, &buf, &blen) && buf)
      text = (char*)buf;
   if (text)
      keychain_pass_line = keychain_line(text, "passphrase");
   keychain_has_pass = keychain_pass_line != NULL;
   mline = text ? keychain_line(text, "machine") : NULL;
   free(text);

   if (!mline)
      /* no wrap lines: the data key is the machine-derived key */
      ok = keychain_machine_key(keychain_salt, keychain_info, keychain_master);
   else
   {
      ok = keychain_b64_exact(mline, wrap, sizeof(wrap))
         && keychain_machine_key(keychain_salt, keychain_wrap_info, kek)
         && keychain_unwrap(kek, keychain_ad_machine, wrap, keychain_master);
      free(mline);
      if (!ok)
      {
         /* wrapped for another machine: the passphrase opens it */
         crypto_memzero(keychain_master, sizeof(keychain_master));
         keychain_locked = true;
      }
   }

   crypto_memzero(kek, sizeof(kek));
   crypto_memzero(wrap, sizeof(wrap));
   keychain_ready = ok;
   return ok;
}

bool keychain_is_ready(void)
{
   return keychain_ready;
}

bool keychain_is_locked(void)
{
   return keychain_locked;
}

bool keychain_has_passphrase(void)
{
   return keychain_has_pass;
}

bool keychain_passphrase_params(bool for_unlock,
      uint8_t *psalt, uint32_t *iterations)
{
   if (for_unlock)
   {
      char    *sp;
      uint8_t  blob[KEYCHAIN_PASS_SALT_SIZE + KEYCHAIN_WRAP_SIZE];
      bool     ok;
      if (!keychain_locked || !keychain_pass_line)
         return false;
      *iterations = (uint32_t)strtoul(keychain_pass_line, &sp, 10);
      ok = *iterations && sp && *sp == ' '
         && keychain_b64_exact(sp + 1, blob, sizeof(blob));
      if (ok)
         memcpy(psalt, blob, KEYCHAIN_PASS_SALT_SIZE);
      crypto_memzero(blob, sizeof(blob));
      return ok;
   }
   if (!keychain_ready)
      return false;
   *iterations = KEYCHAIN_PASS_ITERS;
   return crypto_random_bytes(psalt, KEYCHAIN_PASS_SALT_SIZE) == 0;
}

/* PBKDF2-HMAC-SHA256 for one 32-octet block, as pbkdf2_hmac_sha256(),
 * but resumable: the chain of HMACs advances @n links per step, so a
 * caller can spread it over frames or report progress. The keyed HMAC
 * state is 1.5 KiB with its working copy; it lives on the heap. */
struct keychain_kdf
{
   struct hmac_sha256_ctx base;
   struct hmac_sha256_ctx ctx;
   uint8_t  u[32];
   uint8_t  t[32];
   uint32_t done;
   uint32_t total;
};

struct keychain_kdf *keychain_kdf_begin(const char *passphrase,
      const uint8_t *psalt, uint32_t iterations)
{
   struct keychain_kdf *k;
   uint8_t ibe[4] = { 0, 0, 0, 1 };   /* block 1, big-endian */

   if (string_is_empty(passphrase) || !iterations)
      return NULL;
   if (!(k = (struct keychain_kdf*)malloc(sizeof(*k))))
      return NULL;
   hmac_sha256_init(&k->base, (const uint8_t*)passphrase, strlen(passphrase));
   memcpy(&k->ctx, &k->base, sizeof(k->ctx));
   hmac_sha256_update(&k->ctx, psalt, KEYCHAIN_PASS_SALT_SIZE);
   hmac_sha256_update(&k->ctx, ibe, 4);
   hmac_sha256_final(&k->ctx, k->u);
   memcpy(k->t, k->u, 32);
   k->done  = 1;
   k->total = iterations;
   return k;
}

bool keychain_kdf_step(struct keychain_kdf *k, uint32_t n)
{
   while (n-- && k->done < k->total)
   {
      unsigned j;
      memcpy(&k->ctx, &k->base, sizeof(k->ctx));
      hmac_sha256_update(&k->ctx, k->u, 32);
      hmac_sha256_final(&k->ctx, k->u);
      for (j = 0; j < 32; j++)
         k->t[j] ^= k->u[j];
      k->done++;
   }
   return k->done >= k->total;
}

unsigned keychain_kdf_progress(const struct keychain_kdf *k)
{
   return (unsigned)((uint64_t)k->done * 100 / k->total);
}

void keychain_kdf_end(struct keychain_kdf *k, uint8_t *kek)
{
   if (!k)
      return;
   if (kek)
      memcpy(kek, k->t, AEAD_KEY_SIZE);
   crypto_memzero(k, sizeof(*k));
   free(k);
}

bool keychain_passphrase_derive(const char *passphrase,
      const uint8_t *psalt, uint32_t iterations, uint8_t *kek)
{
   struct keychain_kdf *k = keychain_kdf_begin(passphrase, psalt, iterations);
   if (!k)
      return false;
   keychain_kdf_step(k, iterations);
   keychain_kdf_end(k, kek);
   return true;
}

bool keychain_unlock_kek(const uint8_t *kek)
{
   char    *sp;
   uint8_t  blob[KEYCHAIN_PASS_SALT_SIZE + KEYCHAIN_WRAP_SIZE];
   uint8_t  mkek[AEAD_KEY_SIZE];
   uint8_t  mwrap[KEYCHAIN_WRAP_SIZE];
   bool     ok = false;

   if (!keychain_locked || !keychain_pass_line)
      return false;
   if (!strtoul(keychain_pass_line, &sp, 10) || !sp || *sp != ' '
         || !keychain_b64_exact(sp + 1, blob, sizeof(blob)))
      return false;

   if (keychain_unwrap(kek, keychain_ad_passphrase,
            blob + KEYCHAIN_PASS_SALT_SIZE, keychain_master))
   {
      /* wrap the data key for this machine, so it opens by itself
       * from now on; the passphrase line stays for the next move */
      ok = keychain_machine_key(keychain_salt, keychain_wrap_info, mkek)
         && keychain_wrap(mkek, keychain_ad_machine, keychain_master, mwrap)
         && keychain_write_file(mwrap);
      if (ok)
      {
         keychain_locked = false;
         keychain_ready  = true;
      }
      else
         crypto_memzero(keychain_master, sizeof(keychain_master));
   }
   crypto_memzero(mkek, sizeof(mkek));
   crypto_memzero(blob, sizeof(blob));
   crypto_memzero(mwrap, sizeof(mwrap));
   return ok;
}

bool keychain_set_passphrase_kek(const uint8_t *kek,
      const uint8_t *psalt, uint32_t iterations)
{
   uint8_t  blob[KEYCHAIN_PASS_SALT_SIZE + KEYCHAIN_WRAP_SIZE];
   uint8_t  mkek[AEAD_KEY_SIZE];
   uint8_t  mwrap[KEYCHAIN_WRAP_SIZE];
   char    *old_line = keychain_pass_line;
   char    *b64      = NULL;
   int      n        = 0;
   bool     ok       = false;

   if (!keychain_ready)
      return false;

   if (!kek)
      keychain_pass_line = NULL;
   else
   {
      char *line;
      memcpy(blob, psalt, KEYCHAIN_PASS_SALT_SIZE);
      if (!keychain_wrap(kek, keychain_ad_passphrase, keychain_master,
                  blob + KEYCHAIN_PASS_SALT_SIZE)
            || !(b64 = base64(blob, (int)sizeof(blob), &n)))
         goto end;
      if (!(line = (char*)malloc((size_t)n + 16)))
         goto end;
      sprintf(line, "%u %s", (unsigned)iterations, b64);
      keychain_pass_line = line;
   }

   /* The machine line goes with a passphrase line, so a file moved to
    * another machine knows it is foreign. With neither, the file is
    * the plain salt again when the data key is still this machine's
    * derived key; one unlocked from elsewhere keeps its machine line. */
   {
      uint8_t derived[AEAD_KEY_SIZE];
      bool    native = keychain_machine_key(keychain_salt, keychain_info,
               derived)
         && crypto_memeq_ct(derived, keychain_master, AEAD_KEY_SIZE);
      crypto_memzero(derived, sizeof(derived));
      if (!keychain_pass_line && native)
         ok = keychain_write_file(NULL);
      else
         ok = keychain_machine_key(keychain_salt, keychain_wrap_info, mkek)
            && keychain_wrap(mkek, keychain_ad_machine, keychain_master, mwrap)
            && keychain_write_file(mwrap);
   }

end:
   if (ok)
   {
      if (old_line != keychain_pass_line)
         free(old_line);
      keychain_has_pass = keychain_pass_line != NULL;
   }
   else
   {
      if (keychain_pass_line != old_line)
         free(keychain_pass_line);
      keychain_pass_line = old_line;
   }
   free(b64);
   crypto_memzero(mkek, sizeof(mkek));
   crypto_memzero(blob, sizeof(blob));
   crypto_memzero(mwrap, sizeof(mwrap));
   return ok;
}

bool keychain_unlock(const char *passphrase)
{
   uint8_t  psalt[KEYCHAIN_PASS_SALT_SIZE];
   uint8_t  kek[AEAD_KEY_SIZE];
   uint32_t iters = 0;
   bool     ok    = keychain_passphrase_params(true, psalt, &iters)
      && keychain_passphrase_derive(passphrase, psalt, iters, kek)
      && keychain_unlock_kek(kek);
   crypto_memzero(kek, sizeof(kek));
   return ok;
}

bool keychain_set_passphrase(const char *passphrase)
{
   uint8_t  psalt[KEYCHAIN_PASS_SALT_SIZE];
   uint8_t  kek[AEAD_KEY_SIZE];
   uint32_t iters = 0;
   bool     ok;
   if (string_is_empty(passphrase))
      return keychain_set_passphrase_kek(NULL, NULL, 0);
   ok = keychain_passphrase_params(false, psalt, &iters)
      && keychain_passphrase_derive(passphrase, psalt, iters, kek)
      && keychain_set_passphrase_kek(kek, psalt, iters);
   crypto_memzero(kek, sizeof(kek));
   return ok;
}

void keychain_deinit(void)
{
   crypto_memzero(keychain_master, sizeof(keychain_master));
   keychain_ready    = false;
   keychain_locked   = false;
   keychain_has_pass = false;
   free(keychain_path);
   keychain_path = NULL;
   free(keychain_pass_line);
   keychain_pass_line = NULL;
}

bool keychain_value_is_sealed(const char *value)
{
   return value
      && strncmp(value, KEYCHAIN_VALUE_PREFIX, KEYCHAIN_PREFIX_LEN) == 0;
}

char *keychain_seal_alloc(const char *id, const char *plaintext)
{
   size_t   pt_len;
   size_t   blob_len;
   uint8_t *blob;
   char    *b64;
   char    *out;
   int      b64_len = 0;

   if (!keychain_ready || !id || !plaintext)
      return NULL;

   pt_len   = strlen(plaintext);
   blob_len = AEAD_NONCE_SIZE + pt_len + AEAD_TAG_SIZE;
   if (!(blob = (uint8_t*)malloc(blob_len)))
      return NULL;

   if (crypto_random_bytes(blob, AEAD_NONCE_SIZE) != 0
         || aead_encrypt(AEAD_CHACHA20_POLY1305,
               keychain_master, sizeof(keychain_master),
               blob, AEAD_NONCE_SIZE,
               (const uint8_t*)id, strlen(id),
               (const uint8_t*)plaintext, pt_len,
               blob + AEAD_NONCE_SIZE,
               blob + AEAD_NONCE_SIZE + pt_len, AEAD_TAG_SIZE) != 0)
   {
      free(blob);
      return NULL;
   }

   b64 = base64(blob, (int)blob_len, &b64_len);
   crypto_memzero(blob, blob_len);
   free(blob);
   if (!b64)
      return NULL;

   if (!(out = (char*)malloc(KEYCHAIN_PREFIX_LEN + (size_t)b64_len + 1)))
   {
      free(b64);
      return NULL;
   }
   memcpy(out, KEYCHAIN_VALUE_PREFIX, KEYCHAIN_PREFIX_LEN);
   memcpy(out + KEYCHAIN_PREFIX_LEN, b64, (size_t)b64_len);
   out[KEYCHAIN_PREFIX_LEN + (size_t)b64_len] = '\0';
   free(b64);
   return out;
}

char *keychain_open_alloc(const char *id, const char *stored)
{
   const char *b64;
   uint8_t    *blob;
   int         blob_len = 0;
   size_t      ct_len;
   char       *out;

   if (!id || !stored)
      return NULL;
   if (!keychain_value_is_sealed(stored))
      return strldup(stored, strlen(stored) + 1);
   if (!keychain_ready)
      return NULL;

   b64 = stored + KEYCHAIN_PREFIX_LEN;
   if (!(blob = unbase64(b64, (int)strlen(b64), &blob_len)))
      return NULL;
   if (blob_len < (int)(AEAD_NONCE_SIZE + AEAD_TAG_SIZE))
   {
      free(blob);
      return NULL;
   }
   ct_len = (size_t)blob_len - AEAD_NONCE_SIZE - AEAD_TAG_SIZE;

   if (!(out = (char*)malloc(ct_len + 1)))
   {
      free(blob);
      return NULL;
   }

   if (aead_decrypt(AEAD_CHACHA20_POLY1305,
            keychain_master, sizeof(keychain_master),
            blob, AEAD_NONCE_SIZE,
            (const uint8_t*)id, strlen(id),
            blob + AEAD_NONCE_SIZE, ct_len,
            blob + AEAD_NONCE_SIZE + ct_len, AEAD_TAG_SIZE,
            (uint8_t*)out) != 0)
   {
      free(blob);
      free(out);
      return NULL;
   }
   out[ct_len] = '\0';
   free(blob);
   return out;
}
