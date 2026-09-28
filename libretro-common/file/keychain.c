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

static uint8_t keychain_master[AEAD_KEY_SIZE];
static bool    keychain_ready  = false;

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

bool keychain_init(const char *keyfile_path)
{
   uint8_t salt[KEYCHAIN_SALT_SIZE];
   char   *mid;
   size_t  mid_len;
   bool    ok;

   if (keychain_ready)
      return true;
   if (string_is_empty(keyfile_path))
      return false;

   if (!keychain_load_or_create_salt(keyfile_path, salt))
      return false;

   if (!(mid = (char*)malloc(KEYCHAIN_MACHINE_ID_MAX)))
      return false;
   mid_len = keychain_machine_id(mid, KEYCHAIN_MACHINE_ID_MAX);
   if (!mid_len)
   {
      /* No identity on this platform: the key file is the key. */
      strlcpy(mid, "no-machine-id", KEYCHAIN_MACHINE_ID_MAX);
      mid_len = strlen(mid);
   }

   ok = hkdf_sha256(salt, sizeof(salt), (const uint8_t*)mid, mid_len,
         (const uint8_t*)keychain_info, sizeof(keychain_info) - 1,
         keychain_master, sizeof(keychain_master)) == 0;

   crypto_memzero(mid, KEYCHAIN_MACHINE_ID_MAX);
   free(mid);
   crypto_memzero(salt, sizeof(salt));

   keychain_ready = ok;
   return ok;
}

bool keychain_is_ready(void)
{
   return keychain_ready;
}

void keychain_deinit(void)
{
   crypto_memzero(keychain_master, sizeof(keychain_master));
   keychain_ready = false;
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
