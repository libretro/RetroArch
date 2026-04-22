/* Regression tests for libretro-common/formats/png/rpng_encode.c
 *
 * Round-trip tests the PNG encoder by writing a known pixel pattern
 * to disk, reading it back with the PNG decoder, and comparing pixel
 * for pixel. Any byte-level change to the encoder's deflate output
 * is allowed (different chunking of the same input data can legitimately
 * produce different deflate streams), but the decoded pixels must
 * always round-trip unchanged. This is the right invariant for any
 * encoder refactor -- including a future streaming-deflate rewrite.
 *
 * Exercised entry points:
 *   - rpng_save_image_argb      (uint32 ARGB source, RGBA PNG output)
 *   - rpng_save_image_bgr24     (BGR24 source, RGB PNG output, positive pitch)
 *   - rpng_save_image_bgr24     (BGR24 source, bottom-up, negative pitch
 *                                via the (unsigned)(-pitch) convention used
 *                                by task_screenshot's viewport fast path)
 *
 * Exercised sizes: tiny (4x4), moderate non-power-of-two (37x29),
 * and screenshot-shaped (320x240). The last is big enough that a
 * streaming encoder would span multiple deflate output chunks.
 *
 * Exercised pixel patterns: solid colour, horizontal gradient
 * (stresses the 'sub' filter), vertical gradient ('up' filter),
 * diagonal ('paeth'), and a pseudo-random pattern ('none' filter
 * typically wins). Catches filter-selection regressions as well as
 * byte-level round-trip failures.
 *
 * Usage:
 *   rpng_roundtrip_test            (uses ./rpng_roundtrip_tmp.png)
 *   rpng_roundtrip_test <tmp.png>  (caller-chosen path, useful on
 *                                   systems without /tmp semantics)
 *
 * Returns 0 on success, nonzero on any round-trip mismatch.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <file/nbio.h>
#include <formats/rpng.h>
#include <formats/image.h>
#include <encodings/crc32.h>

static int failures = 0;

/* ---- Structural validation of encoded PNGs ----
 *
 * Two layers, tried in order:
 *
 *   1. External pngcheck(1) if installed -- the gold-standard
 *      third-party PNG validator. Used when available.
 *   2. Built-in fallback otherwise -- walks chunk headers, verifies
 *      CRCs, checks required chunk ordering (IHDR first, IEND last,
 *      exactly one of each). Covers the main class of bug a
 *      streaming-encoder rewrite could introduce: rpng's own encoder
 *      producing output rpng's own decoder will happily read but
 *      other PNG decoders reject.
 *
 * Neither pngcheck nor any additional libraries are a hard
 * dependency. The built-in fallback has no external deps at all
 * (uses encoding_crc32 which is already linked for the encoder).
 *
 * The test banner reports which layer is active so reviewers can
 * tell at a glance whether the stronger third-party check ran. */

static const uint8_t png_magic_bytes[8] = {
   0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A
};

static uint32_t read_be32(const uint8_t *p)
{
   return ((uint32_t)p[0] << 24)
        | ((uint32_t)p[1] << 16)
        | ((uint32_t)p[2] <<  8)
        | ((uint32_t)p[3] <<  0);
}

/* Built-in fallback. Returns true if the file at `path` looks like
 * a well-formed PNG: valid magic, chunks with correct CRCs, IHDR
 * first and present exactly once, IEND last and present exactly
 * once, at least one IDAT. Does not decompress the IDAT stream
 * (the round-trip check that follows does that). Reads whole file
 * into memory -- fine for test image sizes, not a strategy for
 * production code. */
static bool rpng_structural_check(const char *path)
{
   FILE *f;
   long  file_len_l;
   size_t file_len;
   uint8_t *buf = NULL;
   size_t off;
   bool ret       = false;
   bool seen_ihdr = false;
   bool seen_iend = false;
   unsigned idat_count = 0;

   f = fopen(path, "rb");
   if (!f)
      return false;

   if (fseek(f, 0, SEEK_END) != 0) goto done;
   file_len_l = ftell(f);
   if (file_len_l < 0) goto done;
   file_len = (size_t)file_len_l;
   if (fseek(f, 0, SEEK_SET) != 0) goto done;

   if (file_len < sizeof(png_magic_bytes) + 12) /* magic + one chunk min */
      goto done;

   buf = (uint8_t*)malloc(file_len);
   if (!buf) goto done;
   if (fread(buf, 1, file_len, f) != file_len) goto done;

   if (memcmp(buf, png_magic_bytes, sizeof(png_magic_bytes)) != 0)
      goto done;

   off = sizeof(png_magic_bytes);
   while (off + 12 <= file_len) /* length(4) + type(4) + CRC(4) */
   {
      uint32_t chunk_len = read_be32(buf + off);
      const uint8_t *type;
      uint32_t declared_crc;
      uint32_t computed_crc;

      /* Guard chunk_len before it's used in pointer arithmetic:
       * attacker-controlled value, must not overflow off. */
      if (chunk_len > file_len - off - 12)
         goto done;

      type         = buf + off + 4;
      declared_crc = read_be32(buf + off + 8 + chunk_len);
      /* CRC is computed over type(4) + data(chunk_len). */
      computed_crc = encoding_crc32(0, buf + off + 4, 4 + chunk_len);

      if (declared_crc != computed_crc)
         goto done;

      if (!memcmp(type, "IHDR", 4))
      {
         /* Must be first chunk and exactly once. */
         if (off != sizeof(png_magic_bytes) || seen_ihdr)
            goto done;
         seen_ihdr = true;
      }
      else if (!memcmp(type, "IEND", 4))
      {
         if (seen_iend) goto done;
         seen_iend = true;
      }
      else if (!memcmp(type, "IDAT", 4))
      {
         if (!seen_ihdr || seen_iend) goto done;
         idat_count++;
      }
      /* Other ancillary chunks are permitted without further checks. */

      off += 12 + chunk_len;
   }

   /* Must have ended cleanly at end-of-file, seen IHDR and IEND
    * exactly once each, and at least one IDAT. */
   ret = (off == file_len) && seen_ihdr && seen_iend && idat_count > 0;

done:
   free(buf);
   fclose(f);
   return ret;
}

static int pngcheck_available = -1; /* -1 = untested, 0 = no, 1 = yes */

static void pngcheck_probe(void)
{
   /* system() returns 0 only if pngcheck is installed and the
    * shell could locate and run it. Any other value (127 on Unix
    * for command-not-found, whatever cmd.exe produces on Windows)
    * treats pngcheck as unavailable. -h exits 0 on success without
    * reading stdin; if a future pngcheck version changes that
    * behaviour, worst case is that detection turns up missing
    * and we silently fall back to the built-in check. */
   pngcheck_available =
      (system("pngcheck -h >/dev/null 2>&1") == 0) ? 1 : 0;
}

/* Dispatcher. Returns true if the file passes structural
 * validation via whichever layer is active. Always runs some
 * check -- no silent skip path. */
static bool structural_check_ok(const char *path)
{
   if (pngcheck_available > 0)
   {
      char cmd[1024];
      /* -q suppresses per-chunk chatter; on success silent, on
       * failure prints a short ERROR line. We discard both streams
       * and just look at the exit code. */
      snprintf(cmd, sizeof(cmd),
            "pngcheck -q \"%s\" >/dev/null 2>&1", path);
      return system(cmd) == 0;
   }
   return rpng_structural_check(path);
}

/* Sanity-check the built-in structural validator itself: generate
 * a known-good PNG, confirm the check accepts it; corrupt a byte,
 * confirm the check rejects it. If either expectation fails the
 * fallback is broken and we bail rather than run 93 tests under a
 * broken validator. Only runs when the external pngcheck isn't
 * available -- when it is, pngcheck's own correctness is the
 * reference. */
static bool selftest_builtin_check(const char *path)
{
   /* Tiny 2x2 ARGB PNG. */
   uint32_t pixels[4] = {
      0xFFFF0000u, 0xFF00FF00u,
      0xFF0000FFu, 0xFFFFFFFFu
   };
   FILE *f;
   long  file_len_l;
   size_t file_len;
   uint8_t *buf = NULL;
   size_t i;
   bool ok_before, ok_after;

   if (!rpng_save_image_argb(path, pixels, 2, 2,
            (unsigned)(2 * sizeof(uint32_t))))
   {
      printf("[ERROR] self-test: could not generate sample PNG\n");
      return false;
   }

   ok_before = rpng_structural_check(path);
   if (!ok_before)
   {
      printf("[ERROR] self-test: built-in check rejected valid PNG\n");
      return false;
   }

   /* Flip one byte roughly in the middle of the file -- with very
    * high probability this lands inside an IDAT chunk's data and
    * corrupts its CRC. Read the file, flip, write back. */
   f = fopen(path, "rb");
   if (!f) goto self_err;
   if (fseek(f, 0, SEEK_END) != 0) { fclose(f); goto self_err; }
   file_len_l = ftell(f);
   if (file_len_l < 16) { fclose(f); goto self_err; }
   file_len = (size_t)file_len_l;
   if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); goto self_err; }
   buf = (uint8_t*)malloc(file_len);
   if (!buf) { fclose(f); goto self_err; }
   if (fread(buf, 1, file_len, f) != file_len)
   { fclose(f); goto self_err; }
   fclose(f);

   /* Flip a byte in the back half, well past the IHDR CRC, likely
    * inside IDAT data. */
   i       = file_len * 3 / 4;
   buf[i] ^= 0xFF;

   f = fopen(path, "wb");
   if (!f) goto self_err;
   if (fwrite(buf, 1, file_len, f) != file_len)
   { fclose(f); goto self_err; }
   fclose(f);
   free(buf); buf = NULL;

   ok_after = rpng_structural_check(path);
   if (ok_after)
   {
      printf("[ERROR] self-test: built-in check ACCEPTED corrupted PNG "
             "(CRC validation broken)\n");
      return false;
   }

   return true;

self_err:
   free(buf);
   printf("[ERROR] self-test: I/O failure during corruption step\n");
   return false;
}


static bool load_argb(const char *path, uint32_t **data,
      unsigned *width, unsigned *height)
{
   int retval;
   size_t file_len       = 0;
   bool ret              = true;
   rpng_t *rpng          = NULL;
   void *ptr             = NULL;
   struct nbio_t *handle = (struct nbio_t*)nbio_open(path, NBIO_READ);

   if (!handle)
      return false;

   nbio_begin_read(handle);
   while (!nbio_iterate(handle));
   ptr = nbio_get_ptr(handle, &file_len);
   if (!ptr)          { ret = false; goto done; }

   rpng = rpng_alloc();
   if (!rpng)         { ret = false; goto done; }
   if (!rpng_set_buf_ptr(rpng, (uint8_t*)ptr, file_len))
                      { ret = false; goto done; }
   if (!rpng_start(rpng))
                      { ret = false; goto done; }
   while (rpng_iterate_image(rpng));
   if (!rpng_is_valid(rpng))
                      { ret = false; goto done; }

   do
   {
      retval = rpng_process_image(rpng,
            (void**)data, file_len, width, height, false);
   } while (retval == IMAGE_PROCESS_NEXT);

   if (retval == IMAGE_PROCESS_ERROR || retval == IMAGE_PROCESS_ERROR_END)
      ret = false;

done:
   if (handle) nbio_free(handle);
   if (rpng)   rpng_free(rpng);
   if (!ret && *data) { free(*data); *data = NULL; }
   return ret;
}

/* ---- pixel-pattern generators ---- */

enum pattern
{
   PAT_SOLID = 0,
   PAT_H_GRADIENT,
   PAT_V_GRADIENT,
   PAT_DIAGONAL,
   PAT_PSEUDORANDOM,
   PAT_COUNT
};

static const char *pattern_name(enum pattern p)
{
   switch (p)
   {
      case PAT_SOLID:        return "solid";
      case PAT_H_GRADIENT:   return "h-gradient";
      case PAT_V_GRADIENT:   return "v-gradient";
      case PAT_DIAGONAL:     return "diagonal";
      case PAT_PSEUDORANDOM: return "pseudorandom";
      default:               return "?";
   }
}

static uint32_t sample_argb(enum pattern p, unsigned x, unsigned y,
      unsigned w, unsigned h)
{
   uint32_t r, g, b, a = 0xFF;
   (void)w; (void)h;
   switch (p)
   {
      case PAT_SOLID:
         r = 0x40; g = 0x80; b = 0xC0; break;
      case PAT_H_GRADIENT:
         r = (uint8_t)x; g = 0x20; b = 0x80; break;
      case PAT_V_GRADIENT:
         r = 0x20; g = (uint8_t)y; b = 0x80; break;
      case PAT_DIAGONAL:
         r = (uint8_t)(x + y);
         g = (uint8_t)(x ^ y);
         b = (uint8_t)(x * 3 + y * 5);
         break;
      case PAT_PSEUDORANDOM:
      {
         /* Deterministic LCG-ish mix. No PRNG state, no dependency
          * on libc rand(); we want bit-identical input across hosts. */
         uint32_t s = (x * 2654435761u) ^ (y * 40503u);
         r = (s >>  0) & 0xFF;
         g = (s >>  8) & 0xFF;
         b = (s >> 16) & 0xFF;
         break;
      }
      default:
         r = g = b = 0;
   }
   return (a << 24) | (r << 16) | (g << 8) | b;
}

/* ---- the actual tests ---- */

/* ARGB32 round-trip: pack pattern into uint32[] (ARGB), save via
 * rpng_save_image_argb, load via load_argb, compare pixel-for-pixel. */
static void test_argb_roundtrip(const char *path,
      unsigned w, unsigned h, enum pattern pat)
{
   uint32_t *src = NULL;
   uint32_t *got = NULL;
   unsigned got_w = 0, got_h = 0;
   unsigned x, y;
   const char *pname = pattern_name(pat);

   src = (uint32_t*)malloc((size_t)w * h * sizeof(uint32_t));
   if (!src)
   {
      printf("[ERROR] argb %s %ux%u: OOM for source\n", pname, w, h);
      failures++;
      return;
   }

   for (y = 0; y < h; y++)
      for (x = 0; x < w; x++)
         src[y * w + x] = sample_argb(pat, x, y, w, h);

   if (!rpng_save_image_argb(path, src,
            w, h, (unsigned)(w * sizeof(uint32_t))))
   {
      printf("[ERROR] argb %s %ux%u: save failed\n", pname, w, h);
      failures++;
      goto out;
   }

   if (!structural_check_ok(path))
   {
      printf("[ERROR] argb %s %ux%u: structural check rejected output\n",
            pname, w, h);
      failures++;
      /* Continue to the pixel round-trip check anyway -- a file
       * rpng can decode but the structural check rejects is worth
       * recording both ways. */
   }

   if (!load_argb(path, &got, &got_w, &got_h))
   {
      printf("[ERROR] argb %s %ux%u: load failed\n", pname, w, h);
      failures++;
      goto out;
   }

   if (got_w != w || got_h != h)
   {
      printf("[ERROR] argb %s %ux%u: loaded dimensions %ux%u\n",
            pname, w, h, got_w, got_h);
      failures++;
      goto out;
   }

   for (y = 0; y < h; y++)
   {
      for (x = 0; x < w; x++)
      {
         if (src[y * w + x] != got[y * w + x])
         {
            printf("[ERROR] argb %s %ux%u: pixel (%u,%u) "
                   "src=0x%08x got=0x%08x\n",
                   pname, w, h, x, y,
                   src[y * w + x], got[y * w + x]);
            failures++;
            goto out;
         }
      }
   }

   printf("[SUCCESS] argb %s %ux%u round-trip\n", pname, w, h);

out:
   free(src);
   free(got);
}

/* BGR24 round-trip: pack pattern into uint8[] with BGR byte order,
 * save via rpng_save_image_bgr24, load, compare. The loaded form is
 * ARGB32 (alpha=0xFF), so we compare ignoring alpha. */
static void test_bgr24_roundtrip(const char *path,
      unsigned w, unsigned h, enum pattern pat, bool flip_bottom_up)
{
   uint8_t  *src = NULL;
   uint32_t *got = NULL;
   unsigned got_w = 0, got_h = 0;
   unsigned x, y;
   const size_t stride = (size_t)w * 3;
   const char *pname   = pattern_name(pat);
   const char *orient  = flip_bottom_up ? "bottom-up" : "top-down";

   src = (uint8_t*)malloc(stride * h);
   if (!src)
   {
      printf("[ERROR] bgr24 %s %s %ux%u: OOM for source\n",
            pname, orient, w, h);
      failures++;
      return;
   }

   /* Fill source in-order regardless of orientation, so src[y*stride..]
    * always holds the y-th image row top-down. We adjust the pointer
    * and pitch we hand the encoder afterward. */
   for (y = 0; y < h; y++)
   {
      for (x = 0; x < w; x++)
      {
         uint32_t px = sample_argb(pat, x, y, w, h);
         uint8_t *p  = src + y * stride + x * 3;
         /* BGR byte order: dst[0]=B, dst[1]=G, dst[2]=R. */
         p[0] = (uint8_t)(px >>  0);
         p[1] = (uint8_t)(px >>  8);
         p[2] = (uint8_t)(px >> 16);
      }
   }

   if (flip_bottom_up)
   {
      /* Emulate the task_screenshot viewport fast path: point at the
       * last row and pass a negative pitch via the unsigned-wrap
       * convention rpng_save_image_bgr24 uses (it casts back to
       * signed internally). */
      const uint8_t *last_row = src + (size_t)(h - 1) * stride;
      if (!rpng_save_image_bgr24(path, last_row,
               w, h, (unsigned)(-(int)stride)))
      {
         printf("[ERROR] bgr24 %s %s %ux%u: save failed\n",
               pname, orient, w, h);
         failures++;
         goto out;
      }
   }
   else
   {
      if (!rpng_save_image_bgr24(path, src,
               w, h, (unsigned)stride))
      {
         printf("[ERROR] bgr24 %s %s %ux%u: save failed\n",
               pname, orient, w, h);
         failures++;
         goto out;
      }
   }

   if (!structural_check_ok(path))
   {
      printf("[ERROR] bgr24 %s %s %ux%u: structural check rejected output\n",
            pname, orient, w, h);
      failures++;
      /* Continue to the pixel round-trip check anyway. */
   }

   if (!load_argb(path, &got, &got_w, &got_h))
   {
      printf("[ERROR] bgr24 %s %s %ux%u: load failed\n",
            pname, orient, w, h);
      failures++;
      goto out;
   }

   if (got_w != w || got_h != h)
   {
      printf("[ERROR] bgr24 %s %s %ux%u: loaded dimensions %ux%u\n",
            pname, orient, w, h, got_w, got_h);
      failures++;
      goto out;
   }

   for (y = 0; y < h; y++)
   {
      for (x = 0; x < w; x++)
      {
         /* When flip_bottom_up is true, the encoder walked source
          * rows bottom-up via the negative-pitch trick, so PNG row
          * y holds source row (h-1-y). Map the comparison back.
          * Alpha is always 0xFF on the loaded side since PNG
          * color-type-2 has no alpha. */
         unsigned src_y    = flip_bottom_up ? (h - 1 - y) : y;
         uint32_t exp_argb = sample_argb(pat, x, src_y, w, h) | 0xFF000000u;
         /* Compare ignoring alpha. */
         uint32_t got_rgb  = got[y * w + x] & 0x00FFFFFFu;
         uint32_t exp_rgb  = exp_argb       & 0x00FFFFFFu;
         if (got_rgb != exp_rgb)
         {
            printf("[ERROR] bgr24 %s %s %ux%u: pixel (%u,%u) "
                   "exp=0x%06x got=0x%06x\n",
                   pname, orient, w, h, x, y,
                   exp_rgb, got_rgb);
            failures++;
            goto out;
         }
      }
   }

   printf("[SUCCESS] bgr24 %s %s %ux%u round-trip\n",
         pname, orient, w, h);

out:
   free(src);
   free(got);
}

/* ---- harness ---- */

struct size_case { unsigned w, h; };

static const struct size_case sizes[] = {
   {  4,   4},   /* smoke: sub-chunk, stresses filter-buffer init */
   { 37,  29},   /* non-round, not aligned to any natural boundary */
   {320, 240}    /* screenshot-shaped, spans many deflate output chunks */
};

/* Targeted width fuzz: catches off-by-one regressions in the
 * encoder at size boundaries a full-buffer encode might handle
 * correctly but a streaming-deflate encoder could get wrong.
 *
 *   - width=1,2,3:  degenerate; exercises filter loops near the
 *                   per-pixel-stride boundary (filter_sub et al
 *                   read line[i - bpp], so widths <= bpp are the
 *                   narrowest valid inputs).
 *   - width=8,32:   common SIMD alignment boundaries for any
 *                   future vectorised filter.
 *   - width=31,33:  one-off-from-32, catches alignment-vs-tail
 *                   boundary bugs.
 *   - width=257:    one-off-from-256, same idea at a larger size.
 *
 * Pair each width with height=1 (single-row; no valid prev-row
 * data, up/avg/paeth filters see only the zero-initialised
 * prev_encoded buffer) and height=3 (exercises the normal
 * multi-row path at a small enough total size that deflate may
 * produce zero output on early trans() calls -- a case a
 * streaming-deflate loop has to handle correctly). */
static const unsigned fuzz_widths[]  = {1, 2, 3, 8, 31, 32, 33, 257};
static const unsigned fuzz_heights[] = {1, 3};

/* Large-size fuzz: crosses boundaries a streaming-deflate encoder
 * is most likely to be sensitive to, without making the test slow.
 *
 *   1024x1:    single wide row, ~3 KiB. Tests a long per-row
 *              filter loop that exceeds typical L1 cache line
 *              boundaries but fits in a single deflate chunk.
 *   1x1024:    tall thin; 1024 row-boundary transitions packed
 *              into ~4 KiB. A streaming encoder's per-row reset
 *              / state-carry logic gets exercised far more times
 *              per byte than on typical inputs.
 *   2048x1:    6 KiB single row, likely to cross common
 *              chosen-chunk-size thresholds (a 4 KiB or 8 KiB
 *              deflate output buffer would see mid-row boundaries).
 *   1x12000:   ~48 KiB total input, crosses zlib's default 32 KiB
 *              sliding window. Tests that deflate's internal
 *              window-shift logic plays correctly with a row-by-row
 *              feeder. Bounded to stay fast under ASan. */
static const struct size_case large_fuzz[] = {
   {1024,     1},
   {   1,  1024},
   {2048,     1},
   {   1, 12000}
};


int main(int argc, char *argv[])
{
   const char *path = "./rpng_roundtrip_tmp.png";
   size_t i;
   int p;

   if (argc > 2)
   {
      printf("Usage: %s [tmp-png-path]\n", argv[0]);
      return 1;
   }
   if (argc == 2)
      path = argv[1];

   printf("Writing temp PNG to: %s\n", path);

   pngcheck_probe();
   if (pngcheck_available)
      printf("[INFO] pngcheck found, using external structural validation\n");
   else
   {
      printf("[INFO] pngcheck not found, using built-in structural validation\n");
      if (!selftest_builtin_check(path))
      {
         printf("\nBuilt-in structural validator self-test failed. "
                "Aborting before running pixel round-trip tests under "
                "a broken validator.\n");
         return 1;
      }
      printf("[INFO] built-in validator self-test passed\n");
   }

   for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
   {
      for (p = 0; p < PAT_COUNT; p++)
      {
         test_argb_roundtrip (path, sizes[i].w, sizes[i].h, (enum pattern)p);
         test_bgr24_roundtrip(path, sizes[i].w, sizes[i].h, (enum pattern)p, false);
         test_bgr24_roundtrip(path, sizes[i].w, sizes[i].h, (enum pattern)p, true);
      }
   }

   /* Width-boundary fuzz. One pattern (pseudorandom, least likely
    * to hit filter fast paths) across several widths at height=1
    * and height=3 each, for all three entry points. Catches
    * size-boundary regressions that the hand-picked sizes above
    * could miss -- in particular, bugs that surface when deflate
    * produces zero or small output on early trans() calls. */
   {
      size_t wi, hi;
      for (wi = 0; wi < sizeof(fuzz_widths)  / sizeof(fuzz_widths[0]);  wi++)
      {
         for (hi = 0; hi < sizeof(fuzz_heights) / sizeof(fuzz_heights[0]); hi++)
         {
            unsigned w = fuzz_widths[wi];
            unsigned h = fuzz_heights[hi];
            test_argb_roundtrip (path, w, h, PAT_PSEUDORANDOM);
            test_bgr24_roundtrip(path, w, h, PAT_PSEUDORANDOM, false);
            test_bgr24_roundtrip(path, w, h, PAT_PSEUDORANDOM, true);
         }
      }
   }

   /* Large-size fuzz. Targets deflate-internal state-transition
    * boundaries a streaming-deflate rewrite is most likely to get
    * wrong: long single rows (mid-row chunk-size boundaries in the
    * deflate output), many short rows (per-row state-carry
    * logic stressed), and total input size exceeding zlib's
    * default 32 KiB sliding window. Pseudorandom pattern only;
    * runs at all three entry points. */
   {
      size_t li;
      for (li = 0; li < sizeof(large_fuzz) / sizeof(large_fuzz[0]); li++)
      {
         unsigned w = large_fuzz[li].w;
         unsigned h = large_fuzz[li].h;
         test_argb_roundtrip (path, w, h, PAT_PSEUDORANDOM);
         test_bgr24_roundtrip(path, w, h, PAT_PSEUDORANDOM, false);
         test_bgr24_roundtrip(path, w, h, PAT_PSEUDORANDOM, true);
      }
   }

   /* Best-effort cleanup; not fatal if it fails. */
   remove(path);

   if (failures)
   {
      printf("\n%d rpng round-trip test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll rpng round-trip regression tests passed.\n");
   return 0;
}
