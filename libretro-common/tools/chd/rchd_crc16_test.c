/* Checks the table-driven CRC-16 against the bitwise definition.
 *
 * Both forms are here rather than one being included, so the test is
 * checking the algorithm rather than checking a copy of itself.
 *
 * The shared entry point is checked against the same bitwise oracle,
 * so what it agrees with is the definition rather than either table.
 *
 *   cc -o rchd_crc16_test libretro-common/tools/chd/rchd_crc16_test.c \
 *         libretro-common/encodings/encoding_crc32.c \
 *         libretro-common/features/features_cpu.c \
 *         -Ilibretro-common/include
 */
/* The table form must agree with the bitwise form on every input. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <encodings/crc32.h>
static uint16_t bitwise(const uint8_t *d, size_t n)
{ uint16_t c=0xffff; size_t i; int b;
  for(i=0;i<n;i++){ c ^= (uint16_t)((uint16_t)d[i]<<8);
    for(b=0;b<8;b++) c=(uint16_t)((c&0x8000)?((c<<1)^0x1021):(c<<1)); }
  return c; }
static uint16_t T[256]; static int ready;
static void build(void){ uint32_t i; int b; if(ready)return;
  for(i=0;i<256;i++){ uint16_t c=(uint16_t)(i<<8);
    for(b=0;b<8;b++) c=(uint16_t)((c&0x8000)?((c<<1)^0x1021):(c<<1)); T[i]=c; }
  ready=1; }
static uint16_t tabular(const uint8_t *d, size_t n)
{ uint16_t c=0xffff; size_t i; build();
  for(i=0;i<n;i++) c=(uint16_t)((c<<8)^T[(uint8_t)((c>>8)^d[i])]);
  return c; }
int main(void)
{ unsigned s=1; int bad=0, shared_bad=0; size_t len; uint8_t buf[4096]; int t;
  for(t=0;t<5000;t++){ size_t i; uint16_t want;
    len = (size_t)(t % 4096);
    for(i=0;i<len;i++){ s=s*1103515245u+12345u; buf[i]=(uint8_t)(s>>16); }
    want = bitwise(buf,len);
    if (want != tabular(buf,len)) { bad++;
      if(bad<3) printf("  mismatch at len %lu\n",(unsigned long)len); }
    /* The shared implementation slices eight bytes at a time and takes
     * its seed from the caller, so it is checked at the seed the CHD
     * readers pass. */
    if (want != encoding_crc16_ccitt(0xffff, buf, len)) { shared_bad++;
      if(shared_bad<3) printf("  shared mismatch at len %lu\n",
                              (unsigned long)len); } }
  printf("  5000 random buffers up to 4096 bytes: %d mismatches\n", bad);
  printf("  same against encoding_crc16_ccitt:    %d mismatches\n", shared_bad);
  /* Split at an offset that is not a multiple of eight, so the bulk
   * path and the tail both run and the seeded resume is exercised. */
  { uint16_t whole = encoding_crc16_ccitt(0xffff, buf, 1000);
    uint16_t split = encoding_crc16_ccitt(
                        encoding_crc16_ccitt(0xffff, buf, 237), buf+237, 763);
    printf("  seeded continuation:                  %s\n",
           whole==split ? "composes" : "DIFFERS");
    if (whole != split) shared_bad++; }
  return (bad||shared_bad)?1:0; }
