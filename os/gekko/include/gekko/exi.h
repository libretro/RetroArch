/* The external interface: three serial channels to the memory card
 * slots (0 and 1), the clock and SRAM chip (channel 0, device 1) and
 * serial port 1 (channel 0, device 2) and 2 (channel 2).
 *
 * A transfer selects one device on a channel at a clock rate, moves
 * data and deselects it.  Callers hold the channel's lock across the
 * whole exchange. */

#ifndef GEKKO_EXI_H
#define GEKKO_EXI_H

#include <gekko/gekko.h>

#define GK_EXI_CHANNELS 3

enum gk_exi_clock
{
   GK_EXI_1MHZ = 0,
   GK_EXI_2MHZ,
   GK_EXI_4MHZ,
   GK_EXI_8MHZ,
   GK_EXI_16MHZ,
   GK_EXI_32MHZ
};

enum gk_exi_mode
{
   GK_EXI_READ = 0,
   GK_EXI_WRITE,
   GK_EXI_RW
};

void gk_exi_lock(unsigned ch);
void gk_exi_unlock(unsigned ch);

/* Something is plugged into memory card slot ch (0 or 1). */
int  gk_exi_attached(unsigned ch);

void gk_exi_select(unsigned ch, unsigned dev, enum gk_exi_clock clock);
void gk_exi_deselect(unsigned ch);

/* 1 to 4 bytes, most significant first in both directions. */
uint32_t gk_exi_imm(unsigned ch, uint32_t out, unsigned len,
      enum gk_exi_mode mode);
/* Any length both ways at once: out NULL sends 0xff, in NULL drops. */
void gk_exi_imm_bytes(unsigned ch, const uint8_t *out, uint8_t *in,
      uint32_t len);
/* buf 32-byte aligned in MEM1, len a multiple of 32; read or write.
 * 0, or -1 if it did not finish. */
int  gk_exi_dma(unsigned ch, void *buf, uint32_t len, enum gk_exi_mode mode);

/* The real-time clock: seconds since 2000-01-01, local time, plus the
 * console's offset.  0 on success. */
int  gk_rtc_read(uint32_t *seconds);

#endif
