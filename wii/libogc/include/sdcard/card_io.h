#ifndef __CARD_IO_H__
#define __CARD_IO_H__

#include <gctypes.h>

#define MAX_MI_NUM							1
#define MAX_DI_NUM							5

#define PAGE_SIZE256						256
#define PAGE_SIZE512						512

/* CID Register */
#define MANUFACTURER_ID(drv_no)				((u8)(g_CID[drv_no][0]))

/* CSD Register */
#define READ_BL_LEN(drv_no)					((u8)(g_CSD[drv_no][5]&0x0f))
#define WRITE_BL_LEN(drv_no)				((u8)((g_CSD[drv_no][12]&0x03)<<2)|((g_CSD[drv_no][13]>>6)&0x03))
#define C_SIZE(drv_no)						((u16)(((g_CSD[drv_no][6]&0x03)<<10)|(g_CSD[drv_no][7]<<2)|((g_CSD[drv_no][8]>>6)&0x03)))
#define C_SIZE_MULT(drv_no)					((u8)((g_CSD[drv_no][9]&0x03)<<1)|((g_CSD[drv_no][10]>>7)&0x01))

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

extern u8 g_CSD[MAX_DRIVE][16];
extern u8 g_CID[MAX_DRIVE][16];
extern u8 g_mCode[MAX_MI_NUM];
extern u16 g_dCode[MAX_MI_NUM][MAX_DI_NUM];

void sdgecko_initIODefault();
s32 sdgecko_initIO(s32 drv_no);
s32 sdgecko_preIO(s32 drv_no);
s32 sdgecko_readCID(s32 drv_no);
s32 sdgecko_readCSD(s32 drv_no);
s32 sdgecko_readStatus(s32 drv_no);
s32 sdgecko_readSectors(s32 drv_no,u32 sector_no,u32 num_sectors,void *buf);
s32 sdgecko_writeSector(s32 drv_no,u32 sector_no,const void *buf,u32 len);
s32 sdgecko_writeSectors(s32 drv_no,u32 sector_no,u32 num_sectors,const void *buf);

s32 sdgecko_doUnmount(s32 drv_no);

void sdgecko_insertedCB(s32 drv_no);
void sdgecko_ejectedCB(s32 drv_no);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
