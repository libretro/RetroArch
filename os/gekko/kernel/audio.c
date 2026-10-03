/* Audio DMA through the DSP interface, rate set in the audio
 * interface.  Register layouts: YAGCD, Dolphin, NetBSD's bwdsp. */

#include <gekko/audio.h>
#include <gekko/irq.h>

#include "kernel.h"

#define DSP_CSR      0xcc00500au
#define DMA_START_HI 0xcc005030u
#define DMA_START_LO 0xcc005032u
#define DMA_CTRL_LEN 0xcc005036u
#define DMA_LEFT     0xcc00503au
#define AI_CR        0xcc006c00u

/* DSP control: the interrupt bits are cleared by writing 1, so any
 * write carries only the one meant. */
#define CSR_AID      0x0008u
#define CSR_AID_MASK 0x0010u
#define CSR_ARAM     0x0020u
#define CSR_DSP      0x0080u
#define CSR_ACKS     (CSR_AID | CSR_ARAM | CSR_DSP)

#define AI_SCRESET   0x0020u
#define AI_DSP_32K   0x0040u

#define DMA_ENABLE   0x8000u

static gk_audio_fn audio_fn;
static void       *audio_data;
static struct gk_exit_hook audio_hook = { gk_audio_stop, NULL };
static int                 audio_hooked;

static void csr_write(uint16_t set, uint16_t clear, uint16_t ack)
{
   uint16_t v = GK_REG16(DSP_CSR);
   GK_REG16(DSP_CSR) = (uint16_t)(((v & ~CSR_ACKS & ~clear) | set) | ack);
}

static void aid_irq(enum gk_irq irq, void *data)
{
   (void)irq;
   (void)data;
   if (!(GK_REG16(DSP_CSR) & CSR_AID))
      return;
   csr_write(0, 0, CSR_AID);
   if (audio_fn)
      audio_fn(audio_data);
}

void gk_audio_init(unsigned rate)
{
   uint32_t level = gk_irq_disable();
   GK_REG16(DMA_CTRL_LEN) = 0;
   GK_REG32(AI_CR) = (rate == 32000 ? AI_DSP_32K : 0) | AI_SCRESET;
   csr_write(CSR_AID_MASK, 0, CSR_AID);
   gk_irq_set(GK_IRQ_DSP, aid_irq, NULL);
   gk_irq_restore(level);
   if (!audio_hooked)
   {
      audio_hooked = 1;
      gk_exit_hook_add(&audio_hook);
   }
}

void gk_audio_set_cb(gk_audio_fn fn, void *data)
{
   uint32_t level = gk_irq_disable();
   audio_fn   = fn;
   audio_data = data;
   gk_irq_restore(level);
}

void gk_audio_queue(const void *pcm, size_t bytes)
{
   uint32_t phys = GK_PHYS(pcm);
   uint32_t level = gk_irq_disable();
   GK_REG16(DMA_START_HI) = (uint16_t)(phys >> 16);
   GK_REG16(DMA_START_LO) = (uint16_t)(phys & 0xffe0);
   GK_REG16(DMA_CTRL_LEN) = (uint16_t)((GK_REG16(DMA_CTRL_LEN) & DMA_ENABLE)
         | ((bytes / 32) & 0x7fff));
   gk_irq_restore(level);
}

void gk_audio_start(void)
{
   GK_REG16(DMA_CTRL_LEN) = GK_REG16(DMA_CTRL_LEN) | DMA_ENABLE;
}

void gk_audio_stop(void)
{
   GK_REG16(DMA_CTRL_LEN) = GK_REG16(DMA_CTRL_LEN) & ~DMA_ENABLE;
}

size_t gk_audio_remaining(void)
{
   return (size_t)(GK_REG16(DMA_LEFT) & 0x7fff) * 32;
}
