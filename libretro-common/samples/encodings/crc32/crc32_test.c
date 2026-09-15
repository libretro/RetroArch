/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (crc32_test.c).
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

/* Regression test for encoding_crc32().
 *
 * encoding_crc32() has three interchangeable implementations selected
 * per target and, on x86, per CPU at runtime: PCLMULQDQ folding, the
 * ARMv8 CRC32 instructions, and a portable slicing-by-8 table.  Nearly
 * every failure mode of that arrangement is silent -- a wrong checksum
 * is still a plausible-looking 32-bit number -- so the value must be
 * pinned from outside the implementation, on every path, at every
 * length and alignment where the paths hand off to one another.
 *
 * Six parts.
 *
 * The frozen vectors are the published CRC-32/ISO-HDLC check value and
 * a handful of strings run through zlib's crc32() and recorded here.
 * They exist so that a broken bitwise reference cannot cancel out a
 * broken implementation: at least one number in this file has an
 * origin outside it.
 *
 * Everything after that compares against a bit-by-bit reference built
 * straight from the polynomial, with no tables and no word loads.  It
 * is far too slow to ship but it is transparently correct, and it is
 * endian-agnostic, which the slicing-by-8 loop is not -- that loop
 * byte-swaps under MSB_FIRST, and this test is the thing that catches
 * it if the swap is wrong or missing.
 *
 * The sweep hashes every length from 0 to 1200 at all 16 start
 * alignments.  Each buffer is allocated at exactly the size that will
 * be read, so the last valid byte is immediately followed by an ASan
 * redzone and a 16-byte SIMD load that runs one iteration too far is
 * an error rather than a silent success.  Build with
 * SANITIZER=address,undefined and let ASan be the discriminator.
 *
 * The boundary cases sit on the seams: 63/64/65 where the four-way
 * fold loop starts and stops, 15/16/17 where the single-accumulator
 * tail does, and the same +/-1 pattern around every multiple that
 * matters.  These are where an off-by-one lives if one exists.
 *
 * Cross-path agreement is the check specific to runtime dispatch.  The
 * same bytes hashed in one call and in a chain of short calls take
 * different code paths -- a 5 MiB call folds, a chain of 7-byte calls
 * never leaves the byte tail -- and the two must produce the same
 * number.  A dispatch that mishandles the complement at a path
 * boundary passes every one-shot test and fails here.
 *
 * The guard page part is the same overread check as the sweep but done
 * without a sanitizer: buffers are placed to end flush against an
 * unmapped page, so an overread is a SIGSEGV.  That works under
 * qemu-user, where sanitizer runtimes do not, and so covers the
 * cross-architecture lanes.
 *
 * The concurrency part runs first, and has to.  Feature detection is
 * cached on first use, so the window worth testing is several threads
 * calling in for the very first time at once; they are held on an
 * atomic gate and released together.  Any section running ahead of it
 * would warm that cache on the main thread and leave the threads
 * reading an already published value, which is not the case that
 * breaks.  A cache that publishes without ordering is caught here by
 * ThreadSanitizer, and a table built lazily into mutable storage is
 * caught as a wrong checksum.
 *
 * Build:  make            (SANITIZER=address,undefined for a checked run)
 *         make SANITIZER=thread   for the concurrency lane
 * Run:    ./crc32_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <encodings/crc32.h>
#include <retro_atomic.h>

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#if defined(__unix__) || defined(__APPLE__)
#define CRC32_TEST_HAVE_MMAP 1
#include <sys/mman.h>
#include <unistd.h>
#endif

static int failures = 0;

/* Bit-by-bit CRC-32, straight from the reflected polynomial.  No
 * tables, no multi-byte loads, nothing that could share a bug with the
 * implementation under test. */
static uint32_t crc32_bitwise(uint32_t crc, const uint8_t *data, size_t len)
{
   size_t i;
   int    b;

   crc = ~crc;
   for (i = 0; i < len; i++)
   {
      crc ^= data[i];
      for (b = 0; b < 8; b++)
         crc = (crc & 1) ? ((crc >> 1) ^ 0xEDB88320U) : (crc >> 1);
   }
   return ~crc;
}

/* ------------------------------------------------------------------ */
/* 1. Frozen vectors                                                  */
/* ------------------------------------------------------------------ */

static void frozen_vectors(void)
{
   /* 0xCBF43926 for "123456789" is the published check value for
    * CRC-32/ISO-HDLC.  The rest were produced by zlib's crc32() and
    * frozen here. */
   static const struct
   {
      const char *text;
      size_t      len;
      uint32_t    crc;
   } vec[] = {
      { "",                                            0, 0x00000000U },
      { "a",                                           1, 0xE8B7BE43U },
      { "abc",                                         3, 0x352441C2U },
      { "123456789",                                   9, 0xCBF43926U },
      { "message digest",                             14, 0x20159D7FU },
      { "abcdefghijklmnopqrstuvwxyz",                 26, 0x4C2750BDU },
      { "The quick brown fox jumps over the lazy dog",43, 0x414FA339U }
   };
   uint8_t  buf[1000];
   unsigned i;
   int      bad = 0;

   for (i = 0; i < sizeof(vec) / sizeof(vec[0]); i++)
   {
      uint32_t got = encoding_crc32(0, (const uint8_t *)vec[i].text,
                                    vec[i].len);
      if (got != vec[i].crc)
      {
         printf("[FAIL] %-26s \"%s\" -> %08X, expected %08X\n",
                "frozen vectors", vec[i].text, got, vec[i].crc);
         bad++;
      }
   }

   /* Buffers that a string literal cannot express: all-zero, all-ones,
    * the full byte range, and a long run.  Also from zlib. */
   memset(buf, 0x00, 256);
   if (encoding_crc32(0, buf, 256) != 0x0D968558U)
   {
      printf("[FAIL] %-26s 256 zero bytes\n", "frozen vectors");
      bad++;
   }
   memset(buf, 0xFF, 256);
   if (encoding_crc32(0, buf, 256) != 0xFEA8A821U)
   {
      printf("[FAIL] %-26s 256 0xFF bytes\n", "frozen vectors");
      bad++;
   }
   for (i = 0; i < 256; i++)
      buf[i] = (uint8_t)i;
   if (encoding_crc32(0, buf, 256) != 0x29058C73U)
   {
      printf("[FAIL] %-26s byte sequence 0..255\n", "frozen vectors");
      bad++;
   }
   memset(buf, 'a', 1000);
   if (encoding_crc32(0, buf, 1000) != 0x9A38DA03U)
   {
      printf("[FAIL] %-26s 1000 'a' bytes\n", "frozen vectors");
      bad++;
   }

   /* A seeded continuation must land on the one-shot value. */
   if (encoding_crc32(encoding_crc32(0, (const uint8_t *)"12345", 5),
                      (const uint8_t *)"6789", 4) != 0xCBF43926U)
   {
      printf("[FAIL] %-26s seeded continuation\n", "frozen vectors");
      bad++;
   }

   failures += bad;
   if (!bad)
      printf("[ok]   %-26s 12 published/zlib values reproduced\n",
             "frozen vectors");
}

/* ------------------------------------------------------------------ */
/* 2. Length x alignment sweep, exact-size allocations                */
/* ------------------------------------------------------------------ */

#define SWEEP_MAX 1200

static void length_alignment_sweep(void)
{
   size_t n, off;
   long   cases = 0;
   int    bad   = 0;

   for (n = 0; n <= SWEEP_MAX && bad < 8; n++)
   {
      for (off = 0; off < 16; off++)
      {
         /* off + n bytes allocated, n bytes hashed starting at +off:
          * the read window ends exactly at the end of the allocation
          * while the start pointer sweeps every alignment. */
         size_t   total = off + n;
         uint8_t *m     = (uint8_t *)malloc(total ? total : 1);
         size_t   i;
         uint32_t got, want;

         if (!m)
         {
            printf("[FAIL] %-26s out of memory\n", "length/alignment sweep");
            failures++;
            return;
         }
         for (i = 0; i < total; i++)
            m[i] = (uint8_t)(i * 31U + n);

         got  = encoding_crc32(0, m + off, n);
         want = crc32_bitwise(0, m + off, n);
         if (got != want)
         {
            printf("[FAIL] %-26s len=%lu align=%lu -> %08X, expected %08X\n",
                   "length/alignment sweep", (unsigned long)n,
                   (unsigned long)off, got, want);
            bad++;
         }

         /* repeat with a non-zero seed: the entry and exit complements
          * are easy to get right for seed 0 and wrong otherwise */
         got  = encoding_crc32(0xDEADBEEFU, m + off, n);
         want = crc32_bitwise(0xDEADBEEFU, m + off, n);
         if (got != want)
         {
            printf("[FAIL] %-26s len=%lu align=%lu seeded -> %08X, "
                   "expected %08X\n", "length/alignment sweep",
                   (unsigned long)n, (unsigned long)off, got, want);
            bad++;
         }

         cases += 2;
         free(m);
      }
   }

   failures += bad;
   if (!bad)
      printf("[ok]   %-26s %ld cases, lengths 0-%d x 16 alignments\n",
             "length/alignment sweep", cases, SWEEP_MAX);
}

/* ------------------------------------------------------------------ */
/* 3. Fold-loop boundaries                                            */
/* ------------------------------------------------------------------ */

static void fold_boundaries(void)
{
   /* +/-1 around every size where an implementation changes gear: the
    * 64-byte four-way fold, the 16-byte single fold, and page-sized
    * multiples for good measure. */
   static const size_t sizes[] = {
       1,   2,   3,   7,   8,   9,  15,  16,  17,
      31,  32,  33,  47,  48,  49,  62,  63,  64,  65,  66,
      79,  80,  81,  95,  96,  97, 111, 112, 113, 127, 128, 129,
     143, 144, 145, 175, 176, 191, 192, 193, 255, 256, 257,
     511, 512, 513, 1023, 1024, 1025, 4095, 4096, 4097,
     65535, 65536, 65537
   };
   unsigned k;
   size_t   off;
   long     cases = 0;
   int      bad   = 0;

   for (k = 0; k < sizeof(sizes) / sizeof(sizes[0]) && bad < 8; k++)
   {
      for (off = 0; off < 16; off++)
      {
         size_t   n = sizes[k];
         uint8_t *m = (uint8_t *)malloc(off + n);
         size_t   i;
         uint32_t got, want;

         if (!m)
         {
            printf("[FAIL] %-26s out of memory\n", "fold boundaries");
            failures++;
            return;
         }
         for (i = 0; i < off + n; i++)
            m[i] = (uint8_t)(i ^ (n >> 3));

         got  = encoding_crc32(0, m + off, n);
         want = crc32_bitwise(0, m + off, n);
         if (got != want)
         {
            printf("[FAIL] %-26s len=%lu align=%lu -> %08X, expected %08X\n",
                   "fold boundaries", (unsigned long)n,
                   (unsigned long)off, got, want);
            bad++;
         }
         cases++;
         free(m);
      }
   }

   failures += bad;
   if (!bad)
      printf("[ok]   %-26s %ld cases across %u seam sizes\n",
             "fold boundaries", cases,
             (unsigned)(sizeof(sizes) / sizeof(sizes[0])));
}

/* ------------------------------------------------------------------ */
/* 4. Cross-path agreement                                            */
/* ------------------------------------------------------------------ */

#define CHAIN_LEN (1024 * 1024)

static void cross_path_agreement(void)
{
   /* Chunk sizes chosen so that each chain stays inside a different
    * implementation: 1 and 7 never leave the byte tail, 16 and 63 stay
    * in the single-accumulator fold or the table path, 64 and 4096
    * reach the four-way fold.  All must agree with the one-shot value
    * and with each other. */
   static const size_t chunk[] = { 1, 3, 7, 8, 15, 16, 17, 31, 63, 64,
                                   65, 127, 128, 255, 256, 1000, 4096 };
   uint8_t *buf = (uint8_t *)malloc(CHAIN_LEN);
   uint32_t one_shot;
   unsigned k;
   size_t   i;
   int      bad = 0;

   if (!buf)
   {
      printf("[FAIL] %-26s out of memory\n", "cross-path agreement");
      failures++;
      return;
   }
   for (i = 0; i < CHAIN_LEN; i++)
      buf[i] = (uint8_t)(i * 2654435761U >> 13);

   one_shot = encoding_crc32(0, buf, CHAIN_LEN);
   if (one_shot != crc32_bitwise(0, buf, CHAIN_LEN))
   {
      printf("[FAIL] %-26s one-shot disagrees with reference\n",
             "cross-path agreement");
      bad++;
   }

   for (k = 0; k < sizeof(chunk) / sizeof(chunk[0]); k++)
   {
      uint32_t chained = 0;
      size_t   pos;
      for (pos = 0; pos < CHAIN_LEN; pos += chunk[k])
      {
         size_t take = CHAIN_LEN - pos;
         if (take > chunk[k])
            take = chunk[k];
         chained = encoding_crc32(chained, buf + pos, take);
      }
      if (chained != one_shot)
      {
         printf("[FAIL] %-26s chunk=%lu -> %08X, one-shot %08X\n",
                "cross-path agreement", (unsigned long)chunk[k],
                chained, one_shot);
         bad++;
      }
   }

   /* An uneven split is the case a fixed chunk size can miss: the
    * second call starts at an arbitrary alignment with an arbitrary
    * incoming seed. */
   for (i = 0; i < 512; i++)
   {
      size_t   split   = (i * 7919) % CHAIN_LEN;
      uint32_t chained = encoding_crc32(encoding_crc32(0, buf, split),
                                        buf + split, CHAIN_LEN - split);
      if (chained != one_shot)
      {
         printf("[FAIL] %-26s split=%lu -> %08X, one-shot %08X\n",
                "cross-path agreement", (unsigned long)split,
                chained, one_shot);
         bad++;
         break;
      }
   }

   free(buf);
   failures += bad;
   if (!bad)
      printf("[ok]   %-26s %u chunk sizes + 512 splits over %d KiB\n",
             "cross-path agreement",
             (unsigned)(sizeof(chunk) / sizeof(chunk[0])),
             CHAIN_LEN / 1024);
}

/* ------------------------------------------------------------------ */
/* 5. Guard page overread                                             */
/* ------------------------------------------------------------------ */

static void guard_page(void)
{
#ifdef CRC32_TEST_HAVE_MMAP
   long     pg = sysconf(_SC_PAGESIZE);
   size_t   span;
   uint8_t *base;
   size_t   n;
   long     cases = 0;
   int      bad   = 0;

   if (pg <= 0)
   {
      printf("[skip] %-26s page size unavailable\n", "guard page overread");
      return;
   }
   span = (size_t)pg * 2;
   base = (uint8_t *)mmap(NULL, span, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
   if (base == MAP_FAILED)
   {
      printf("[skip] %-26s mmap unavailable\n", "guard page overread");
      return;
   }
   /* Leave the second page unmapped.  Any read one byte past the end
    * of a buffer that terminates on the boundary faults. */
   if (mprotect(base + pg, (size_t)pg, PROT_NONE) != 0)
   {
      munmap(base, span);
      printf("[skip] %-26s mprotect unavailable\n", "guard page overread");
      return;
   }
   memset(base, 0xA5, (size_t)pg);

   for (n = 0; n <= 600; n++)
   {
      uint8_t *p = base + pg - n;      /* ends flush against the guard */
      if (encoding_crc32(0, p, n) != crc32_bitwise(0, p, n))
      {
         printf("[FAIL] %-26s len=%lu\n", "guard page overread",
                (unsigned long)n);
         bad++;
         break;
      }
      cases++;
   }
   /* Longer buffers so the four-way fold loop, not just the tail, runs
    * up against the boundary. */
   for (n = 0; n < 300; n++)
   {
      size_t   len = (size_t)pg - n;
      uint8_t *p   = base + n;
      if (encoding_crc32(0, p, len) != crc32_bitwise(0, p, len))
      {
         printf("[FAIL] %-26s page-length len=%lu\n",
                "guard page overread", (unsigned long)len);
         bad++;
         break;
      }
      cases++;
   }

   munmap(base, span);
   failures += bad;
   if (!bad)
      printf("[ok]   %-26s %ld buffers ended flush against PROT_NONE\n",
             "guard page overread", cases);
#else
   printf("[skip] %-26s needs mmap\n", "guard page overread");
#endif
}

/* ------------------------------------------------------------------ */
/* 6. Concurrent first touch                                          */
/* ------------------------------------------------------------------ */

#ifdef HAVE_THREADS

#define CRC_THREADS 8
#define CRC_ROUNDS  40
#define CRC_BUFLEN  (256 * 1024)

static uint8_t            crc_buf[CRC_BUFLEN];
static uint32_t           crc_expected;
static retro_atomic_int_t crc_gate;      /* 0 = hold, 1 = go */
static retro_atomic_int_t crc_mismatch;

static void crc_gate_wait(void)
{
   while (!retro_atomic_load_acquire_int(&crc_gate))
      ;
}

static void crc_note_mismatch(void)
{
   retro_atomic_store_release_int(&crc_mismatch, 1);
}

static int crc_run(void (*fn)(void *))
{
   sthread_t *t[CRC_THREADS];
   long       i;
   int        started = 0;

   retro_atomic_int_init(&crc_gate, 0);

   for (i = 0; i < CRC_THREADS; i++)
   {
      t[i] = sthread_create(fn, (void *)(intptr_t)i);
      if (t[i])
         started++;
   }

   retro_atomic_store_release_int(&crc_gate, 1);

   for (i = 0; i < CRC_THREADS; i++)
      if (t[i])
         sthread_join(t[i]);

   return started;
}

/* Exactly one call per thread, and it has to stay that way.
 *
 * The regression this is here to catch is feature detection publishing
 * its result without ordering, which ThreadSanitizer sees as one write
 * racing many reads.  TSan keeps only a few shadow entries per address,
 * so every additional access to that same word crowds the racing write
 * out of the history before a conflicting read arrives.  Measured on
 * this test: eight threads making one dispatching call each reports the
 * race in 30 runs out of 30; the same eight threads making forty calls
 * each report it in none.  Raising the thread count does not help
 * either -- sixteen threads drops to four runs in ten, thirty-two to
 * two -- because the extra reads evict faster than they conflict.
 *
 * So: one call, and the stress work lives in crc_stress_worker below,
 * which runs afterwards and is not what TSan is reading. */
static void crc_first_touch_worker(void *arg)
{
   (void)arg;
   crc_gate_wait();

   if (encoding_crc32(0, crc_buf, CRC_BUFLEN) != crc_expected)
      crc_note_mismatch();
}

/* Sustained concurrent hashing: varied lengths and alignments so that
 * different threads are inside different code paths at the same time.
 * A table built lazily into mutable storage shows up here as a wrong
 * checksum, deterministically, without needing a sanitizer at all. */
static void crc_stress_worker(void *arg)
{
   long id = (long)(intptr_t)arg;
   int  i;

   crc_gate_wait();

   for (i = 0; i < CRC_ROUNDS; i++)
   {
      size_t off = (size_t)((id * 3 + i) & 15);
      size_t n   = CRC_BUFLEN - off - (size_t)((id + i) & 63);

      if (encoding_crc32(0, crc_buf + off, n)
       != crc32_bitwise(0, crc_buf + off, n))
         crc_note_mismatch();

      /* short buffers take the non-SIMD path on the same thread */
      if (encoding_crc32(0, crc_buf + off, (size_t)((id + i) & 63))
       != crc32_bitwise(0, crc_buf + off, (size_t)((id + i) & 63)))
         crc_note_mismatch();
   }

   if (encoding_crc32(0, crc_buf, CRC_BUFLEN) != crc_expected)
      crc_note_mismatch();
}

static void concurrent_first_touch(void)
{
   long i;
   int  started;

   for (i = 0; i < CRC_BUFLEN; i++)
      crc_buf[i] = (uint8_t)(i * 2654435761U >> 13);
   crc_expected = crc32_bitwise(0, crc_buf, CRC_BUFLEN);

   retro_atomic_int_init(&crc_mismatch, 0);

   started = crc_run(crc_first_touch_worker);
   if (started != CRC_THREADS)
   {
      printf("[FAIL] %-26s only %d of %d threads started\n",
             "concurrent first touch", started, CRC_THREADS);
      failures++;
      return;
   }
   if (retro_atomic_load_acquire_int(&crc_mismatch))
   {
      printf("[FAIL] %-26s threads disagreed on the checksum\n",
             "concurrent first touch");
      failures++;
      return;
   }
   printf("[ok]   %-26s %d threads, one simultaneous first call each\n",
          "concurrent first touch", CRC_THREADS);
}

static void concurrent_stress(void)
{
   int started;

   retro_atomic_int_init(&crc_mismatch, 0);

   started = crc_run(crc_stress_worker);
   if (started != CRC_THREADS)
   {
      printf("[FAIL] %-26s only %d of %d threads started\n",
             "concurrent stress", started, CRC_THREADS);
      failures++;
      return;
   }
   if (retro_atomic_load_acquire_int(&crc_mismatch))
   {
      printf("[FAIL] %-26s wrong checksum under concurrent load\n",
             "concurrent stress");
      failures++;
      return;
   }
   printf("[ok]   %-26s %d threads x %d rounds over %d KiB agreed\n",
          "concurrent stress", CRC_THREADS, CRC_ROUNDS, CRC_BUFLEN / 1024);
}

#else /* !HAVE_THREADS */

static void concurrent_first_touch(void)
{
   printf("[skip] %-26s built without HAVE_THREADS\n",
          "concurrent first touch");
}

static void concurrent_stress(void)
{
   printf("[skip] %-26s built without HAVE_THREADS\n",
          "concurrent stress");
}

#endif

/* ------------------------------------------------------------------ */

int main(void)
{
   /* The concurrency check runs FIRST and must stay first.  Its whole
    * value is that the worker threads race each other into feature
    * detection on their very first call; any section running before it
    * would warm that cache on the main thread and quietly reduce the
    * check to eight threads reading an already published value.  A
    * regression that dropped the acquire/release pair would then go
    * unreported.  Do not reorder. */
   concurrent_first_touch();

   frozen_vectors();
   length_alignment_sweep();
   fold_boundaries();
   cross_path_agreement();
   guard_page();
   concurrent_stress();

   printf("%s\n", failures ? "FAILED" : "PASS");
   return failures ? 1 : 0;
}
