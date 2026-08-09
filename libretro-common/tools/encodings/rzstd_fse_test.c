/* Builds the three FSE tables RFC 8878 predefines and checks that the
 * spread closes and the widths land in range.
 *
 * These are the one set of tables whose contents the specification
 * states outright, so they are the only ones that can be checked
 * without a reference decoder to compare against.
 *
 *   cc -I libretro-common/include -o rzstd_fse_test \
 *      libretro-common/tools/encodings/rzstd_fse_test.c \
 *      libretro-common/encodings/encoding_rzstd.c
 */
/* Builds the three tables RFC 8878 predefines and checks the spread. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
int rzstd_probe_fse(const int16_t*,uint32_t,uint32_t,uint8_t*,uint16_t*,uint8_t*);

/* Section 3.1.1.3.2.2.1, given as normalised counts. */
static const int16_t ll[] = {
 4, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1,
 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 2, 1, 1, 1, 1, 1,
 -1,-1,-1,-1 };
static const int16_t ml[] = {
 1, 4, 3, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1,
 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
 -1,-1,-1,-1,-1 };
static const int16_t of[] = {
 1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1,
 1, 1, 1, 1, 1, 1, 1, 1,-1,-1,-1,-1,-1 };

static void check(const char*name,const int16_t*c,uint32_t n,uint32_t log)
{
   uint8_t bits[512], sym[512]; uint16_t st[512];
   uint32_t size=1u<<log, i, sum=0, bad=0;
   int e = rzstd_probe_fse(c,n,log,bits,st,sym);
   for(i=0;i<n;i++) sum += c[i]<0 ? 1 : (uint32_t)c[i];
   for(i=0;i<size;i++) if(bits[i]>log) bad++;
   printf("  %-16s log=%u symbols=%-3u counts sum=%u (need %u)  build=%d  "
          "widths out of range=%u\n", name, log, n, sum, size, e, bad);
}
int main(void)
{
   check("literal length", ll, sizeof(ll)/2, 6);
   check("match length",   ml, sizeof(ml)/2, 6);
   check("offset",         of, sizeof(of)/2, 5);
   return 0;
}
