/* Serial interface: controller detection, the per-field poll and its
 * interrupt.  Register layout: YAGCD and NetBSD's si.c. */

#include <string.h>

#include <gekko/irq.h>
#include <gekko/pad.h>
#include <gekko/thread.h>

#define SI_BASE        0xcc006400u
#define SI_OUTBUF(n)   (SI_BASE + (n) * 12)
#define SI_INBUFH(n)   (SI_BASE + (n) * 12 + 4)
#define SI_INBUFL(n)   (SI_BASE + (n) * 12 + 8)
#define SI_POLL        (SI_BASE + 0x30)
#define SI_COMCSR      (SI_BASE + 0x34)
#define SI_SR          (SI_BASE + 0x38)
#define SI_IOBUF       (SI_BASE + 0x80)

#define POLL_X(x)      ((uint32_t)(x) << 16)
#define POLL_Y(y)      ((uint32_t)(y) << 8)
#define POLL_EN(n)     (1u << (7 - (n)))

#define COM_TCINT      0x80000000u
#define COM_TCINTMSK   0x40000000u
#define COM_RDSTINT    0x10000000u
#define COM_RDSTINTMSK 0x08000000u
#define COM_OUT(n)     ((uint32_t)((n) & 0x7f) << 16)
#define COM_IN(n)      ((uint32_t)((n) & 0x7f) << 8)
#define COM_CHAN(n)    ((uint32_t)(n) << 1)
#define COM_TSTART     0x00000001u

#define SR_SHIFT(n)    ((3 - (n)) * 8)
#define SR_WR(n)       (0x80u << SR_SHIFT(n))
#define SR_RDST(n)     (0x20u << SR_SHIFT(n))
#define SR_ERRORS(n)   (0x0fu << SR_SHIFT(n))
#define SR_NOREP(n)    (0x08u << SR_SHIFT(n))

#define CMD_ID         0x00
#define CMD_POLL       0x00400300u   /* poll, analogue mode 3 */
#define CMD_ORIGIN     0x41

#define RESCAN_US      500000

static gk_pad_t          pads[GK_PAD_PORTS];
static uint8_t           origin[GK_PAD_PORTS][6];
static uint32_t          rumble[GK_PAD_PORTS];
static uint32_t          poll_mask;
static uint64_t          next_scan;
static gk_mutex_t        scan_lock = GK_MUTEX_INIT;
static volatile uint32_t xfer_done;

/* One command-and-response transfer on a channel; 0 on a reply. */
static int transfer(unsigned chan, const uint8_t *out, unsigned out_len,
      uint8_t *in, unsigned in_len)
{
   uint32_t words[32];
   uint32_t done, com, i;
   uint64_t deadline;
   memset(words, 0, sizeof(words));
   memcpy(words, out, out_len);
   for (i = 0; i < (out_len + 3) / 4; i++)
      GK_REG32(SI_IOBUF + i * 4) = words[i];
   GK_REG32(SI_SR) = SR_ERRORS(chan);
   done = xfer_done;
   com  = GK_REG32(SI_COMCSR) & (COM_RDSTINTMSK | COM_TCINTMSK);
   GK_REG32(SI_COMCSR) = com | COM_TCINT | COM_TCINTMSK | COM_OUT(out_len)
      | COM_IN(in_len) | COM_CHAN(chan) | COM_TSTART;
   deadline = gk_ticks() + GK_US_TO_TICKS(5000);
   while (xfer_done == done && gk_ticks() < deadline)
      gk_futex_wait(&xfer_done, done, GK_US_TO_TICKS(1000));
   if (xfer_done == done || (GK_REG32(SI_SR) & SR_NOREP(chan)))
      return -1;
   for (i = 0; i < (in_len + 3) / 4; i++)
      words[i] = GK_REG32(SI_IOBUF + i * 4);
   memcpy(in, words, in_len);
   return 0;
}

static void decode(unsigned n, uint32_t hi, uint32_t lo)
{
   gk_pad_t *p = &pads[n];
   uint8_t b0 = (uint8_t)(hi >> 24), b1 = (uint8_t)(hi >> 16);
   p->buttons = (uint16_t)(((b0 & 0x1f) << 8) | (b1 & 0x7f));
   p->stick_x   = (int8_t)((int)(uint8_t)(hi >> 8) - origin[n][0]);
   p->stick_y   = (int8_t)((int)(uint8_t)hi        - origin[n][1]);
   p->sub_x     = (int8_t)((int)(uint8_t)(lo >> 24) - origin[n][2]);
   p->sub_y     = (int8_t)((int)(uint8_t)(lo >> 16) - origin[n][3]);
   p->trigger_l = (uint8_t)(lo >> 8);
   p->trigger_r = (uint8_t)lo;
   p->polls++;
}

static void si_irq(enum gk_irq irq, void *data)
{
   uint32_t com = GK_REG32(SI_COMCSR);
   uint32_t sr  = GK_REG32(SI_SR);
   unsigned n;
   (void)irq;
   (void)data;
   if (com & COM_TCINT)
   {
      GK_REG32(SI_COMCSR) = (com & ~COM_TSTART) | COM_TCINT;
      xfer_done++;
      gk_futex_wake(&xfer_done, 1);
   }
   if (com & COM_RDSTINT)
      for (n = 0; n < GK_PAD_PORTS; n++)
      {
         if (!(sr & SR_RDST(n)))
            continue;
         /* Reading the input buffers clears the read status. */
         {
            uint32_t hi = GK_REG32(SI_INBUFH(n));
            uint32_t lo = GK_REG32(SI_INBUFL(n));
            if (!(poll_mask & (1u << n)))
               continue;
            if (hi & 0x80000000u)   /* the controller reports an error */
            {
               pads[n].connected = 0;
               poll_mask &= ~(1u << n);
               GK_REG32(SI_POLL) &= ~POLL_EN(n);
               continue;
            }
            decode(n, hi, lo);
         }
      }
   GK_REG32(SI_SR) = sr & (SR_ERRORS(0) | SR_ERRORS(1) | SR_ERRORS(2)
         | SR_ERRORS(3));
}

/* Look for controllers on the ports not being polled. */
static void scan(void)
{
   unsigned n;
   gk_mutex_lock(&scan_lock);
   for (n = 0; n < GK_PAD_PORTS; n++)
   {
      uint8_t cmd = CMD_ID, id[3], org[10];
      uint32_t level;
      if (poll_mask & (1u << n))
         continue;
      if (transfer(n, &cmd, 1, id, 3) || (id[0] & 0x09) != 0x09)
         continue;
      cmd = CMD_ORIGIN;
      if (transfer(n, &cmd, 1, org, 10))
         continue;
      memcpy(origin[n], org + 2, 4);   /* stick and C-stick centres */
      level = gk_irq_disable();
      memset(&pads[n], 0, sizeof(pads[n]));
      pads[n].connected = 1;
      poll_mask |= 1u << n;
      GK_REG32(SI_OUTBUF(n)) = CMD_POLL | rumble[n];
      GK_REG32(SI_SR)        = SR_WR(n);
      GK_REG32(SI_POLL)     |= POLL_EN(n);
      gk_irq_restore(level);
   }
   next_scan = gk_ticks() + GK_US_TO_TICKS(RESCAN_US);
   gk_mutex_unlock(&scan_lock);
}

void gk_pad_init(void)
{
   memset(pads, 0, sizeof(pads));
   poll_mask = 0;
   /* One poll per field, at line 7. */
   GK_REG32(SI_POLL)   = POLL_X(7) | POLL_Y(1);
   GK_REG32(SI_COMCSR) = COM_TCINT | COM_RDSTINT | COM_TCINTMSK
      | COM_RDSTINTMSK;
   gk_irq_set(GK_IRQ_SI, si_irq, NULL);
   scan();
}

void gk_pad_read(gk_pad_t out[GK_PAD_PORTS])
{
   uint32_t level;
   if (gk_ticks() >= next_scan && poll_mask != 0xf)
      scan();
   level = gk_irq_disable();
   memcpy(out, pads, sizeof(pads));
   gk_irq_restore(level);
}

void gk_pad_rumble(unsigned port, int on)
{
   uint32_t level;
   if (port >= GK_PAD_PORTS)
      return;
   level = gk_irq_disable();
   rumble[port] = on ? 1u : 0u;
   GK_REG32(SI_OUTBUF(port)) = CMD_POLL | rumble[port];
   GK_REG32(SI_SR) = SR_WR(port);
   gk_irq_restore(level);
}
