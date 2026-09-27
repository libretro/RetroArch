/* Compile-only PS2 stub for the matrix's ps2 video lane, carrying
 * the DMA channel setup the driver does once at init. Each
 * declaration is the shape the real header gives it. */
#ifndef STUB_PS2_DMAKIT
#define STUB_PS2_DMAKIT
#include <tamtypes.h>

#define DMA_CHANNEL_GIF		0x2

#define DMA_TAG(QWC,PCE,ID,IRQ,ADDR,SPR) ( \
((u64)(QWC)  <<  0) | ((u64)(PCE) << 26) | \
((u64)(ID)   << 28) | ((u64)(IRQ) << 31) | \
((u64)(ADDR) << 32) | ((u64)(SPR) << 63))

#define D_CTRL_MFD_OFF 0x0
#define D_CTRL_RCYC_8 0x0
#define D_CTRL_RELE_OFF 0x0
#define D_CTRL_STD_OFF 0x0
#define D_CTRL_STS_UNSPEC 0x0
int dmaKit_chan_init(u32 channel);
int dmaKit_init(u32 RELE, u32 MFD, u32 STS, u32 STD, u32 RCYC, u16 fastwaitchannels);

#endif
