/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (x509_store_threads_test.c).
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

/* The x509 trust store from several threads at once, under TSan and
 * ASan. Four threads load the same store at the same moment and must
 * each verify against it; then they keep verifying while a fifth
 * swaps between two stores and none, so a walk is always running over
 * a store that is being replaced. A walk must see one whole store or
 * none, and a replaced store must outlive every walk still on it. */

#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <crypto/x509.h>
#include "test_x509_vectors.h"

#define NTHREADS 4
#define ROUNDS   300
#define SWAPS    600

static pthread_barrier_t go;
static volatile int swapping;
static int bad_first, bad_walk;
static pthread_mutex_t bad_lock = PTHREAD_MUTEX_INITIALIZER;

static int verify(char *info, size_t info_len)
{
   const uint8_t *chain[2];
   size_t lens[2];
   chain[0] = x509_leaf;  lens[0] = sizeof(x509_leaf);
   chain[1] = x509_inter; lens[1] = sizeof(x509_inter);
   return x509_verify_chain(chain, lens, 2, "example.com",
         (time_t)X509_TEST_NOW, info, info_len);
}

static void *first_use(void *arg)
{
   char info[128];
   (void)arg;
   pthread_barrier_wait(&go);
   if (     x509_trust_load_pem(x509_root_pem, sizeof(x509_root_pem) - 1) != 1
         || verify(info, sizeof(info)) != 0)
   {
      pthread_mutex_lock(&bad_lock);
      bad_first++;
      pthread_mutex_unlock(&bad_lock);
   }
   return NULL;
}

static void *walker(void *arg)
{
   int i;
   char info[128];
   (void)arg;
   pthread_barrier_wait(&go);
   for (i = 0; i < ROUNDS; i++)
   {
      /* the root store verifies it; the other store or none refuses it
       * for want of an issuer, never for anything else */
      if (     verify(info, sizeof(info)) != 0
            && strcmp(info, "issuer not found or signature invalid")
            && strcmp(info, "no trust anchors loaded"))
      {
         pthread_mutex_lock(&bad_lock);
         if (!bad_walk++)
            fprintf(stderr, "walk: %s\n", info);
         pthread_mutex_unlock(&bad_lock);
      }
   }
   return NULL;
}

static void *swapper(void *arg)
{
   int i;
   (void)arg;
   pthread_barrier_wait(&go);
   for (i = 0; i < SWAPS; i++)
   {
      switch (i % 3)
      {
         case 0:
            x509_trust_load_pem(x509_policy_ca_pem, sizeof(x509_policy_ca_pem) - 1);
            break;
         case 1:
            x509_trust_free();
            break;
         default:
            x509_trust_load_pem(x509_root_pem, sizeof(x509_root_pem) - 1);
            break;
      }
   }
   return NULL;
}

int main(void)
{
   pthread_t t[NTHREADS + 1];
   char info[128];
   int i, fails = 0;

   pthread_barrier_init(&go, NULL, NTHREADS);
   for (i = 0; i < NTHREADS; i++)
      pthread_create(&t[i], NULL, first_use, NULL);
   for (i = 0; i < NTHREADS; i++)
      pthread_join(t[i], NULL);
   pthread_barrier_destroy(&go);
   if (bad_first)
   {
      printf("FAIL: %d of %d first users could not verify against the store they loaded\n",
            bad_first, NTHREADS);
      fails++;
   }
   else
      printf("ok:   %d threads load the store at once, each verifies against it\n", NTHREADS);

   pthread_barrier_init(&go, NULL, NTHREADS + 1);
   for (i = 0; i < NTHREADS; i++)
      pthread_create(&t[i], NULL, walker, NULL);
   pthread_create(&t[NTHREADS], NULL, swapper, NULL);
   for (i = 0; i <= NTHREADS; i++)
      pthread_join(t[i], NULL);
   pthread_barrier_destroy(&go);
   if (bad_walk)
   {
      printf("FAIL: %d walks over a store being replaced went wrong\n", bad_walk);
      fails++;
   }
   else
      printf("ok:   %d threads x %d walks while the store is swapped %d times\n",
            NTHREADS, ROUNDS, SWAPS);

   x509_trust_load_pem(x509_root_pem, sizeof(x509_root_pem) - 1);
   if (verify(info, sizeof(info)) != 0)
   {
      printf("FAIL: the store loaded last does not verify: %s\n", info);
      fails++;
   }
   x509_trust_free();
   return fails ? 1 : 0;
}
