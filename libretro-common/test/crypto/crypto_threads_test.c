/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (crypto_threads_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* First use of the crypto library from several threads at once, run
 * under TSan. Each primitive with a table or a CPU dispatch that could
 * be set up on first use is first used here by every thread at the
 * same moment (a barrier releases them together), so a lazily built
 * table or cached feature check is a reported race on any runner,
 * however many processors it has. Every thread must also compute the
 * same bytes: a table seen half built gives a different answer. */

#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <crypto/crypto.h>
#include <crypto/kdf.h>

#define NTHREADS 8

static pthread_barrier_t go;
/* AES block, GCM ciphertext and tag, ChaCha20-Poly1305 ciphertext and
 * tag, HMAC-SHA256 */
static uint8_t results[NTHREADS][16 + 16 + 16 + 16 + 16 + 32];

static void *worker(void *arg)
{
   static const uint8_t key[32] = {
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
      17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 };
   static const uint8_t nonce[12] = { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 1, 2 };
   static const uint8_t pt[16]    = "sixteen octets!";
   size_t          n   = (size_t)arg;
   uint8_t        *out = results[n];
   struct aes_ctx  aes;

   pthread_barrier_wait(&go);

   /* AES tables and the AES-NI / ARMv8 dispatch */
   aes_init(&aes, key, 16);
   aes_encrypt_block(&aes, pt, out);
   /* GCM: the PCLMUL / PMULL dispatch */
   aead_encrypt(AEAD_AES256_GCM, key, 32, nonce, 12, NULL, 0,
         pt, 16, out + 16, out + 32, 16);
   /* ChaCha20-Poly1305 */
   aead_encrypt(AEAD_CHACHA20_POLY1305, key, 32, nonce, 12, NULL, 0,
         pt, 16, out + 48, out + 64, 16);
   /* SHA-256 through HMAC: the SHA-NI / ARMv8 dispatch */
   hmac_sha256(key, 32, pt, 16, out + 80);
   return NULL;
}

int main(void)
{
   pthread_t t[NTHREADS];
   size_t    i;
   int       bad = 0;

   pthread_barrier_init(&go, NULL, NTHREADS);
   for (i = 0; i < NTHREADS; i++)
      pthread_create(&t[i], NULL, worker, (void*)i);
   for (i = 0; i < NTHREADS; i++)
      pthread_join(t[i], NULL);
   pthread_barrier_destroy(&go);

   for (i = 1; i < NTHREADS; i++)
      if (memcmp(results[i], results[0], sizeof(results[0])) != 0)
      {
         fprintf(stderr, "FAIL: thread %u computed different bytes\n",
               (unsigned)i);
         bad = 1;
      }
   if (!bad)
      printf("ok:   first use of the crypto library from %d threads at once\n",
            NTHREADS);
   return bad;
}
