/* A USB Gecko in a memory card slot: debug output to the computer it
 * is plugged into.  Each command is one 16-bit exchange, the command
 * in the top nibble; the reply has 0x0400 set when the adapter took a
 * byte or has room for one. */

#include <gekko/exi.h>
#include <gekko/usbgecko.h>

#include "kernel.h"

#define CMD_IDENT   0x9000u
#define CMD_SEND    0xb000u
#define CMD_TX_ROOM 0xc000u
#define IDENT       0x0470u
#define OK          0x0400u

/* A computer that stopped reading leaves the adapter full: a byte gets
 * this many tries, and then output stops until there is room again. */
#define SEND_TRIES  64

static int channel = -1;

static uint32_t command(unsigned ch, uint32_t cmd)
{
   uint32_t r;
   gk_exi_select(ch, 0, GK_EXI_32MHZ);
   r = gk_exi_imm(ch, cmd, 2, GK_EXI_RW);
   gk_exi_deselect(ch);
   return r;
}

void gk_usbgecko_probe(void)
{
   unsigned ch;
   channel = -1;
   /* Slot B first: slot A is where SD adapters usually go. */
   for (ch = 2; ch-- > 0;)
      if (gk_exi_attached(ch) && command(ch, CMD_IDENT) == IDENT)
      {
         channel = (int)ch;
         return;
      }
}

int gk_usbgecko_channel(void)
{
   return channel;
}

void gk_usbgecko_write(const char *s, size_t len)
{
   unsigned ch, tries;
   uint32_t level;
   if (channel < 0 || !len)
      return;
   ch    = (unsigned)channel;
   level = gk_irq_disable();
   if (command(ch, CMD_TX_ROOM) & OK)
      while (len--)
      {
         uint32_t cmd = CMD_SEND | ((uint32_t)(uint8_t)*s++ << 4);
         for (tries = 0; tries < SEND_TRIES; tries++)
            if (command(ch, cmd) & OK)
               break;
         if (tries == SEND_TRIES)
            break;
      }
   gk_irq_restore(level);
}
