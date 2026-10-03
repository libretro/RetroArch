/* The external interface, and the clock on it. */

#include <string.h>

#include <gekko/conf.h>
#include <gekko/exi.h>
#include <gekko/irq.h>
#include <gekko/thread.h>

#include "kernel.h"

#define EXI_BASE      0xcc006800u
#define EXI_CSR(ch)   (EXI_BASE + (ch) * 0x14)
#define EXI_MAR(ch)   (EXI_BASE + (ch) * 0x14 + 0x04)
#define EXI_LEN(ch)   (EXI_BASE + (ch) * 0x14 + 0x08)
#define EXI_CR(ch)    (EXI_BASE + (ch) * 0x14 + 0x0c)
#define EXI_DATA(ch)  (EXI_BASE + (ch) * 0x14 + 0x10)

#define CSR_EXIINTMASK 0x0001u
#define CSR_EXIINT     0x0002u
#define CSR_TCINTMASK  0x0004u
#define CSR_TCINT      0x0008u
#define CSR_CLK(c)     ((uint32_t)(c) << 4)
#define CSR_CS(dev)    (0x80u << (dev))
#define CSR_EXTINTMASK 0x0400u
#define CSR_EXTINT     0x0800u
#define CSR_EXT        0x1000u
#define CSR_ROMDIS     0x2000u
/* Status bits that a write of 1 clears. */
#define CSR_ACKS       (CSR_EXIINT | CSR_TCINT | CSR_EXTINT)
/* What a select or deselect keeps. */
#define CSR_KEEP       (CSR_EXIINTMASK | CSR_TCINTMASK | CSR_EXTINTMASK \
                        | CSR_ROMDIS)

#define CR_TSTART      0x01u
#define CR_DMA         0x02u
#define CR_RW(m)       ((uint32_t)(m) << 2)
#define CR_TLEN(n)     ((uint32_t)((n) - 1) << 4)

/* The clock chip: the counter, then SRAM's 64 bytes. */
#define RTC_DEV        1
#define RTC_CMD_COUNT  0x20000000u
#define RTC_CMD_SRAM   0x20000100u
#define SRAM_BIAS      0x0c

#define GC_EPOCH_UNIX  946684800u

static gk_mutex_t        locks[GK_EXI_CHANNELS];
static volatile uint32_t dma_done[GK_EXI_CHANNELS];
static int               irq_installed;

void gk_exi_lock(unsigned ch)
{
   gk_mutex_lock(&locks[ch]);
}

void gk_exi_unlock(unsigned ch)
{
   gk_mutex_unlock(&locks[ch]);
}

int gk_exi_attached(unsigned ch)
{
   return ch < 2 && (GK_REG32(EXI_CSR(ch)) & CSR_EXT) != 0;
}

void gk_exi_select(unsigned ch, unsigned dev, enum gk_exi_clock clock)
{
   uint32_t csr = GK_REG32(EXI_CSR(ch)) & CSR_KEEP;
   GK_REG32(EXI_CSR(ch)) = csr | CSR_CLK(clock) | CSR_CS(dev);
}

void gk_exi_deselect(unsigned ch)
{
   GK_REG32(EXI_CSR(ch)) = GK_REG32(EXI_CSR(ch)) & CSR_KEEP;
}

uint32_t gk_exi_imm(unsigned ch, uint32_t out, unsigned len,
      enum gk_exi_mode mode)
{
   unsigned shift = (4 - len) * 8;
   GK_REG32(EXI_DATA(ch)) = out << shift;
   GK_REG32(EXI_CR(ch))   = CR_TLEN(len) | CR_RW(mode) | CR_TSTART;
   while (GK_REG32(EXI_CR(ch)) & CR_TSTART)
      ;
   return mode == GK_EXI_WRITE ? 0 : GK_REG32(EXI_DATA(ch)) >> shift;
}

void gk_exi_imm_bytes(unsigned ch, const uint8_t *out, uint8_t *in,
      uint32_t len)
{
   while (len)
   {
      unsigned n = len < 4 ? (unsigned)len : 4, i;
      uint32_t v = 0;
      for (i = 0; i < n; i++)
         v = (v << 8) | (out ? out[i] : 0xff);
      v = gk_exi_imm(ch, v, n, GK_EXI_RW);
      if (in)
         for (i = n; i-- > 0; v >>= 8)
            in[i] = (uint8_t)v;
      if (out)
         out += n;
      if (in)
         in += n;
      len -= n;
   }
}

static void exi_irq(enum gk_irq irq, void *data)
{
   unsigned ch;
   (void)irq;
   (void)data;
   for (ch = 0; ch < GK_EXI_CHANNELS; ch++)
   {
      uint32_t csr = GK_REG32(EXI_CSR(ch));
      if ((csr & CSR_TCINT) && (csr & CSR_TCINTMASK))
      {
         /* Keep the selection; clear and mask only this source. */
         GK_REG32(EXI_CSR(ch)) = (csr & ~CSR_ACKS & ~CSR_TCINTMASK)
            | CSR_TCINT;
         dma_done[ch]++;
         gk_futex_wake(&dma_done[ch], 1);
      }
   }
}

int gk_exi_dma(unsigned ch, void *buf, uint32_t len, enum gk_exi_mode mode)
{
   uint64_t deadline;
   uint32_t done, level;
   int sleep = (gk_msr_get() & GK_MSR_EE) != 0;

   /* Nothing of buf may be left dirty in the cache to land on top of
    * what the transfer brings in, or be missed by what it sends. */
   gk_dcache_flush(buf, len);
   if (sleep && !irq_installed)
   {
      irq_installed = 1;
      gk_irq_set(GK_IRQ_EXI, exi_irq, NULL);
   }
   level = gk_irq_disable();
   done  = dma_done[ch];
   if (sleep)
      GK_REG32(EXI_CSR(ch)) = (GK_REG32(EXI_CSR(ch)) & ~CSR_ACKS)
         | CSR_TCINT | CSR_TCINTMASK;
   GK_REG32(EXI_MAR(ch)) = GK_PHYS(buf);
   GK_REG32(EXI_LEN(ch)) = len;
   GK_REG32(EXI_CR(ch))  = CR_RW(mode) | CR_DMA | CR_TSTART;
   gk_irq_restore(level);

   /* At 1 MHz a byte takes 8 us; allow twice that and some. */
   deadline = gk_ticks() + GK_US_TO_TICKS((uint64_t)len * 16 + 10000);
   while (GK_REG32(EXI_CR(ch)) & CR_TSTART)
   {
      if (gk_ticks() > deadline)
         return -1;
      if (sleep && dma_done[ch] == done)
         gk_futex_wait(&dma_done[ch], done, GK_US_TO_TICKS(1000));
   }
   if (mode != GK_EXI_WRITE)
      gk_dcache_invalidate(buf, len);
   return 0;
}

/* ---- the clock ---- */

static uint32_t rtc_cmd(uint32_t cmd)
{
   uint32_t v;
   gk_exi_select(0, RTC_DEV, GK_EXI_8MHZ);
   gk_exi_imm(0, cmd, 4, GK_EXI_WRITE);
   v = gk_exi_imm(0, 0, 4, GK_EXI_READ);
   gk_exi_deselect(0);
   return v;
}

static uint32_t sram_bias(void)
{
   uint8_t sram[64];
   unsigned i;
   gk_exi_select(0, RTC_DEV, GK_EXI_8MHZ);
   gk_exi_imm(0, RTC_CMD_SRAM, 4, GK_EXI_WRITE);
   for (i = 0; i < sizeof(sram); i += 4)
   {
      uint32_t v = gk_exi_imm(0, 0, 4, GK_EXI_READ);
      sram[i]     = (uint8_t)(v >> 24);
      sram[i + 1] = (uint8_t)(v >> 16);
      sram[i + 2] = (uint8_t)(v >> 8);
      sram[i + 3] = (uint8_t)v;
   }
   gk_exi_deselect(0);
   return ((uint32_t)sram[SRAM_BIAS] << 24) | (sram[SRAM_BIAS + 1] << 16)
      | (sram[SRAM_BIAS + 2] << 8) | sram[SRAM_BIAS + 3];
}

int gk_rtc_read(uint32_t *seconds)
{
   uint32_t a, b = 0, bias;
   unsigned tries;
   int ok = 0;
#ifdef HW_RVL
   int have_conf = gk_conf_counter_bias(&bias) == 0;
#endif
   gk_exi_lock(0);
   /* The counter can tick between bytes: read until two agree. */
   a = rtc_cmd(RTC_CMD_COUNT);
   for (tries = 0; tries < 8 && !ok; tries++)
   {
      b  = rtc_cmd(RTC_CMD_COUNT);
      ok = a == b;
      a  = b;
   }
#ifdef HW_RVL
   if (!have_conf)
#endif
      bias = sram_bias();
   gk_exi_unlock(0);
   if (!ok)
      return -1;
   *seconds = b + bias;
   return 0;
}

/* Called once by the C library's clock before its first wall-clock
 * read. */
void gk_rtc_sync(void)
{
   uint32_t s;
   if (!gk_rtc_read(&s))
      gk_set_wall_clock((uint64_t)s + GC_EPOCH_UNIX);
}
