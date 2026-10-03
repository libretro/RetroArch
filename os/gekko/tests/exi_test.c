/* External interface checks, run in Dolphin by run-dolphin.sh, which
 * sets the clock to 1790000000 (2026-09-21 14:13:20): the C library's
 * clock starts from the clock chip, DMA and immediate transfers agree
 * on the chip's SRAM, and SD adapter probes on empty ports give up
 * quickly. */

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <gekko/disk.h>
#include <gekko/exi.h>
#include <gekko/power.h>
#include <gekko/thread.h>

static unsigned failures;

#define CHECK(cond, what) \
   do { \
      if (cond) \
         gk_debug_printf("ok   %s", what); \
      else \
      { \
         gk_debug_printf("FAIL %s", what); \
         failures++; \
      } \
   } while (0)

#define CLOCK_SET 1790000000L

static uint8_t dma_buf[64] __attribute__((aligned(32)));

static void sram_imm(uint8_t *out)
{
   unsigned i;
   gk_exi_select(0, 1, GK_EXI_8MHZ);
   gk_exi_imm(0, 0x20000100u, 4, GK_EXI_WRITE);
   for (i = 0; i < 64; i += 4)
   {
      uint32_t v = gk_exi_imm(0, 0, 4, GK_EXI_READ);
      out[i]     = (uint8_t)(v >> 24);
      out[i + 1] = (uint8_t)(v >> 16);
      out[i + 2] = (uint8_t)(v >> 8);
      out[i + 3] = (uint8_t)v;
   }
   gk_exi_deselect(0);
}

static int sram_dma(int write)
{
   int ret;
   gk_exi_select(0, 1, GK_EXI_8MHZ);
   gk_exi_imm(0, write ? 0xa0000100u : 0x20000100u, 4, GK_EXI_WRITE);
   ret = gk_exi_dma(0, dma_buf, sizeof(dma_buf),
         write ? GK_EXI_WRITE : GK_EXI_READ);
   gk_exi_deselect(0);
   return ret;
}

static void test_clock(void)
{
   char what[96];
   time_t now = time(NULL);
   uint32_t raw;
   snprintf(what, sizeof(what), "time() %ld, clock set to %ld", (long)now,
         CLOCK_SET);
   CHECK(now >= CLOCK_SET && now < CLOCK_SET + 30, what);
   CHECK(gk_rtc_read(&raw) == 0 && raw + 946684800u >= (uint32_t)CLOCK_SET,
         "the counter");
}

static void test_sram(void)
{
   uint8_t a[64], b[64];
   gk_exi_lock(0);
   sram_imm(a);
   memset(dma_buf, 0x5a, sizeof(dma_buf));
   CHECK(sram_dma(0) == 0 && !memcmp(a, dma_buf, 64),
         "SRAM: DMA read matches immediate reads");
   /* Write the same bytes back with the checksum's complement flipped,
    * read them, then restore. */
   dma_buf[2] ^= 0xff;
   CHECK(sram_dma(1) == 0, "SRAM: DMA write");
   sram_imm(b);
   CHECK(b[2] == (uint8_t)(a[2] ^ 0xff) && !memcmp(a + 3, b + 3, 61),
         "SRAM: immediate read sees the DMA write");
   memcpy(dma_buf, a, sizeof(a));
   sram_dma(1);
   sram_imm(b);
   CHECK(!memcmp(a, b, 64), "SRAM: restored");
   gk_exi_unlock(0);
}

static void test_sd_probe(void)
{
   char what[96];
   uint64_t t0 = gk_ticks();
   gk_blockdev_t *dev = gk_sdgecko_open(2);
   uint64_t us = GK_TICKS_TO_US(gk_ticks() - t0);
   snprintf(what, sizeof(what), "no SD card on serial port 2 (%u ms)",
         (unsigned)(us / 1000));
   CHECK(!dev && us < 1000000, what);
}

int main(int argc, char **argv)
{
   (void)argc;
   (void)argv;
   test_clock();
   test_sram();
   test_sd_probe();
   gk_debug_printf("%s (%u failure(s))", failures ? "FAILED" : "PASSED",
         failures);
#ifdef HW_RVL
   gk_power_off();
#endif
   return 0;
}
