/* Regression test for the lc/lp limit the LZMA decoder accepts.
 *
 * RLZMA_LCLP_MAX was 3, on the stated assumption that the 7z streams
 * libretro-common reads would not exceed that. They do: 7-Zip emits
 * lc=4, lp=0 routinely, which is a properties byte of 0x5E, and
 * rlzma_dec_init() rejected every such stream.
 *
 * 7z keeps its own header as an LZMA stream, so the rejection landed
 * at r7z_archive_open() rather than on any one member: an archive
 * whose header was written that way could not be opened at all, and
 * the file list came back empty. A scan walking a directory of those
 * then stalled, because the caller looking for a member it would
 * never see had no way out of its loop.
 *
 * Two things are checked:
 *
 *   - the properties byte 7-Zip actually produces is accepted;
 *   - every lc/lp pair within the declared limit is accepted, and the
 *     probability array is sized for it, so the constant and the
 *     array cannot drift apart again.
 *
 * Build: make -C libretro-common/samples/formats/r7z check
 */

#include <stdio.h>
#include <stdint.h>

#include <7z/r7z_lzma.h>

static int failures = 0;
static int checks   = 0;

static void check(int ok, const char *what, const char *detail)
{
   checks++;
   printf("  %-5s %-44s %s\n", ok ? "ok" : "FAIL", what,
         detail ? detail : "");
   if (!ok)
      failures++;
}

/* props[0] encodes (pb * 5 + lp) * 9 + lc; the remaining four bytes
 * are the dictionary size, which this test does not care about. */
static void make_props(uint8_t out[5], unsigned lc, unsigned lp,
      unsigned pb)
{
   out[0] = (uint8_t)((pb * 5 + lp) * 9 + lc);
   out[1] = 0;
   out[2] = 0;
   out[3] = 0;
   out[4] = 1;
}

int main(void)
{
   /* Static rather than automatic: the probability array inside is
    * tens of kilobytes, which is more than some of the platforms this
    * builds for give a thread. */
   static rlzma_dec_t dec;
   uint8_t  props[5];
   unsigned lc, lp, pb;
   int      rejected_within_limit = 0;
   char     detail[128];

   printf("lzma property limit test\n\n");

   /* The case that was failing in the field. */
   make_props(props, 4, 0, 2);
   sprintf(detail, "props 0x%02X", props[0]);
   check(rlzma_dec_init(&dec, props) == RLZMA_OK,
         "lc=4 lp=0 pb=2, as 7-Zip writes it", detail);

   /* Anything the header says it accepts, it has to accept. */
   for (pb = 0; pb <= 4; pb++)
   {
      for (lp = 0; lp <= 4; lp++)
      {
         for (lc = 0; lc <= 8; lc++)
         {
            unsigned d = (pb * 5 + lp) * 9 + lc;

            if (d >= 9 * 5 * 5)          /* not encodable in one byte */
               continue;
            if (lc + lp > RLZMA_LCLP_MAX)
               continue;

            make_props(props, lc, lp, pb);
            if (rlzma_dec_init(&dec, props) != RLZMA_OK)
            {
               if (!rejected_within_limit)
                  printf("        first rejected: lc=%u lp=%u pb=%u "
                         "(props 0x%02X)\n", lc, lp, pb, d);
               rejected_within_limit++;
            }
         }
      }
   }
   sprintf(detail, "%d rejected", rejected_within_limit);
   check(rejected_within_limit == 0,
         "every lc/lp within RLZMA_LCLP_MAX accepted", detail);

   /* The array has to be big enough for the limit the header claims.
    * These moved apart once already: the constant said 3 while the
    * comment beside it described the size for 4. */
   sprintf(detail, "max=%d, probs=%u", RLZMA_LCLP_MAX,
         (unsigned)RLZMA_NUM_PROBS);
   check(RLZMA_NUM_PROBS
            >= (unsigned)(1984 + (0x300u << RLZMA_LCLP_MAX)),
         "probability array sized for the limit", detail);

   /* LZMA2 wraps the same coder; a stream legal for one is legal for
    * the other, so a lower limit here would reject content that
    * arrives through the other path perfectly well. */
   check(RLZMA_LCLP_MAX >= 4,
         "limit covers what the format allows in practice",
         RLZMA_LCLP_MAX >= 4 ? "4 or more" : "below 4");

   printf("\n%d checks, %d failures\n", checks, failures);
   return failures ? 1 : 0;
}
