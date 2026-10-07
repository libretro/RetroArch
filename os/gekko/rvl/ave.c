/* Wii: the A/V encoder, an I2C device (address 0xE0) on two GPIO
 * lines the processor owns.  Register map and bus timing from
 * WiiBrew's Hardware/AV Encoder page. */

#include <gekko/thread.h>
#include <gekko/video.h>

#include "ave_gamma.h"

#define GPIOB_OUT 0xcd8000c0u
#define GPIOB_DIR 0xcd8000c4u
#define GPIOB_IN  0xcd8000c8u
#define AVE_SCL   (1u << 14)
#define AVE_SDA   (1u << 15)

#define AVE_ADDR  0xe0

#define AVE_OUTPUT_CONFIG 0x01   /* colour encoding, YUV (component) */
#define AVE_TRAP_FILTER   0x03
#define AVE_GAMMA         0x10   /* AVE_GAMMA_LEN bytes */
#define AVE_RGB_FILTER    0x6e   /* on for EURGB60 */

static gk_mutex_t lock;
static uint8_t    gamma_tenths;   /* 0: as the encoder was found */

static void udelay(unsigned us)
{
   uint64_t end = gk_ticks() + GK_US_TO_TICKS(us);
   while (gk_ticks() < end)
      ;
}

static void line(uint32_t bit, int high)
{
   uint32_t v = GK_REG32(GPIOB_OUT);
   GK_REG32(GPIOB_OUT) = high ? (v | bit) : (v & ~bit);
}

static void sda_output(int out)
{
   uint32_t v = GK_REG32(GPIOB_DIR);
   GK_REG32(GPIOB_DIR) = out ? (v | AVE_SDA) : (v & ~AVE_SDA);
}

static int send_byte(uint8_t c)
{
   int i, ack;
   for (i = 0; i < 8; i++)
   {
      line(AVE_SDA, c & 0x80);
      udelay(2);
      line(AVE_SCL, 1);
      udelay(2);
      line(AVE_SCL, 0);
      c <<= 1;
   }
   /* The device pulls SDA low to acknowledge. */
   sda_output(0);
   udelay(2);
   line(AVE_SCL, 1);
   udelay(2);
   ack = !(GK_REG32(GPIOB_IN) & AVE_SDA);
   line(AVE_SDA, 0);
   sda_output(1);
   line(AVE_SCL, 0);
   return ack;
}

static int write_reg(uint8_t reg, const uint8_t *data, unsigned len)
{
   unsigned i;
   int ok;
   uint32_t dir = GK_REG32(GPIOB_DIR);
   GK_REG32(GPIOB_DIR) = dir | AVE_SCL | AVE_SDA;
   line(AVE_SCL, 1);
   line(AVE_SDA, 1);
   udelay(4);
   /* Start: SDA falls while SCL is high. */
   line(AVE_SDA, 0);
   udelay(2);
   line(AVE_SCL, 0);
   ok = send_byte(AVE_ADDR) && send_byte(reg);
   for (i = 0; ok && i < len; i++)
      ok = send_byte(data[i]);
   /* Stop: SDA rises while SCL is high. */
   line(AVE_SDA, 0);
   udelay(2);
   line(AVE_SCL, 1);
   udelay(2);
   line(AVE_SDA, 1);
   udelay(2);
   return ok;
}

static int write8(uint8_t reg, uint8_t v)
{
   return write_reg(reg, &v, 1);
}

void gk_ave_set(unsigned std, int component)
{
   uint8_t enc;
   switch (std)
   {
      case GK_VI_PAL:
      case GK_VI_EURGB60:
         enc = 2;
         break;
      case GK_VI_MPAL:
         enc = 1;
         break;
      default:
         enc = 0;
         break;
   }
   gk_mutex_lock(&lock);
   write8(AVE_OUTPUT_CONFIG, (uint8_t)(enc | (component ? 0x20 : 0)));
   write8(AVE_RGB_FILTER, std == GK_VI_EURGB60 ? 1 : 0);
   gk_mutex_unlock(&lock);
}


void gk_vi_set_trap_filter(int on)
{
   gk_mutex_lock(&lock);
   write8(AVE_TRAP_FILTER, on ? 1 : 0);
   gk_mutex_unlock(&lock);
}

void gk_vi_set_gamma(unsigned tenths)
{
   uint8_t t[AVE_GAMMA_LEN];
   tenths = tenths < 1 ? 1 : tenths > 30 ? 30 : tenths;
   gk_mutex_lock(&lock);
   if (tenths != gamma_tenths)
   {
      gamma_tenths = (uint8_t)tenths;
      ave_gamma_table(tenths, t);
      write_reg(AVE_GAMMA, t, sizeof(t));
   }
   gk_mutex_unlock(&lock);
}
