/*-------------------------------------------------------------

card.c -- Memory card subsystem

Copyright (C) 2004
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include <gcutil.h>
#include "asm.h"
#include "processor.h"
#include "system.h"
#include "ogcsys.h"
#include "cache.h"
#include "dsp.h"
#include "lwp.h"
#include "exi.h"
#include "card.h"

#define CARD_SYSAREA				5
#define CARD_SYSDIR					0x2000
#define CARD_SYSDIR_BACK			0x4000
#define CARD_SYSBAT					0x6000
#define CARD_SYSBAT_BACK			0x8000

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))
#define _ROTL(v,s)			\
	(((u32)v<<s)|((u32)v>>(0x20-s)))

#define CARD_STATUS_UNLOCKED			0x40

struct card_header {
	u32 serial[0x08];
	u16 device_id;
	u16	size;
	u16 encoding;
	u8 padding[0x1d6];
	u16 chksum1;
	u16 chksum2;
} ATTRIBUTE_PACKED;

struct card_direntry {
	u8 gamecode[4];
	u8 company[2];
	u8 pad_00;
	u8 bannerfmt;
	u8 filename[CARD_FILENAMELEN];
	u32 lastmodified;
	u32 iconaddr;
	u16 iconfmt;
	u16 iconspeed;
	u8 permission;
	u8 copytimes;
	u16 block;
	u16 length;
	u16 pad_01;
	u32 commentaddr;
} ATTRIBUTE_PACKED;

struct card_dat {			// dir allocation table
	struct card_direntry entries[CARD_MAXFILES];
};

struct card_dircntrl {
	u8 pad[58];
	u16 updated;
	u16 chksum1;
	u16 chksum2;
} ATTRIBUTE_PACKED;

struct card_bat {
	u16 chksum1;
	u16 chksum2;
	u16 updated;
	u16 freeblocks;
	u16 lastalloc;
	u16 fat[0xffc];
} ATTRIBUTE_PACKED;

typedef struct _card_block {
	u8 cmd[9];
	u32 cmd_len;
	u32 cmd_mode;
	u32 cmd_blck_cnt;
	u32 cmd_sector_addr;
	u32 cmd_retries;
	u32 attached;
	s32 result;
	u32 cid;
	u16 card_size;
	u32 mount_step;
	u32 format_step;
	u32 sector_size;
	u16 blocks;
	u32 latency;
	u32 cipher;
	u32 key[3];
	u32 transfer_cnt;
	u16 curr_fileblock;
	card_file *curr_file;
	struct card_dat *curr_dir;
	struct card_bat *curr_fat;
	void *workarea;
	void *cmd_usr_buf;
	lwpq_t wait_sync_queue;
	syswd_t timeout_svc;
	dsptask_t dsp_task;

	cardcallback card_ext_cb;
	cardcallback card_tx_cb;
	cardcallback card_exi_cb;
	cardcallback card_api_cb;
	cardcallback card_xfer_cb;
	cardcallback card_erase_cb;
	cardcallback card_unlock_cb;
} card_block;

#if defined(HW_RVL)

static u32 _cardunlockdata[0x160] ATTRIBUTE_ALIGN(32) =
{
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000021,0x02ff0021,
	0x13061203,0x12041305,0x009200ff,0x0088ffff,
	0x0089ffff,0x008affff,0x008bffff,0x8f0002bf,
	0x008816fc,0xdcd116fd,0x000016fb,0x000102bf,
	0x008e25ff,0x0380ff00,0x02940027,0x02bf008e,
	0x1fdf24ff,0x02403fff,0x00980400,0x009a0010,
	0x00990000,0x8e0002bf,0x009402bf,0x864402bf,
	0x008816fc,0xdcd116fd,0x000316fb,0x00018f00,
	0x02bf008e,0x0380cdd1,0x02940048,0x27ff0380,
	0x00010295,0x005a0380,0x00020295,0x8000029f,
	0x00480021,0x8e0002bf,0x008e25ff,0x02bf008e,
	0x25ff02bf,0x008e25ff,0x02bf008e,0x00c5ffff,
	0x03403fff,0x1c9f02bf,0x008e00c7,0xffff02bf,
	0x008e00c6,0xffff02bf,0x008e00c0,0xffff02bf,
	0x008e20ff,0x03403fff,0x1f5f02bf,0x008e21ff,
	0x02bf008e,0x23ff1205,0x1206029f,0x80b50021,
	0x27fc03c0,0x8000029d,0x008802df,0x27fe03c0,
	0x8000029c,0x008e02df,0x2ece2ccf,0x00f8ffcd,
	0x00f9ffc9,0x00faffcb,0x26c902c0,0x0004029d,
	0x009c02df,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000
};

#elif defined(HW_DOL)

static u32 _cardunlockdata[0x160] ATTRIBUTE_ALIGN(32) =
{
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000021,0x02ff0021,
	0x13061203,0x12041305,0x009200ff,0x0088ffff,
	0x0089ffff,0x008affff,0x008bffff,0x8f0002bf,
	0x008816fc,0xdcd116fd,0x000016fb,0x000102bf,
	0x008e25ff,0x0380ff00,0x02940027,0x02bf008e,
	0x1fdf24ff,0x02400fff,0x00980400,0x009a0010,
	0x00990000,0x8e0002bf,0x009402bf,0x864402bf,
	0x008816fc,0xdcd116fd,0x000316fb,0x00018f00,
	0x02bf008e,0x0380cdd1,0x02940048,0x27ff0380,
	0x00010295,0x005a0380,0x00020295,0x8000029f,
	0x00480021,0x8e0002bf,0x008e25ff,0x02bf008e,
	0x25ff02bf,0x008e25ff,0x02bf008e,0x00c5ffff,
	0x03400fff,0x1c9f02bf,0x008e00c7,0xffff02bf,
	0x008e00c6,0xffff02bf,0x008e00c0,0xffff02bf,
	0x008e20ff,0x03400fff,0x1f5f02bf,0x008e21ff,
	0x02bf008e,0x23ff1205,0x1206029f,0x80b50021,
	0x27fc03c0,0x8000029d,0x008802df,0x27fe03c0,
	0x8000029c,0x008e02df,0x2ece2ccf,0x00f8ffcd,
	0x00f9ffc9,0x00faffcb,0x26c902c0,0x0004029d,
	0x009c02df,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000
};
#endif

static u32 card_sector_size[] =
{
	0x0002000,
	0x0004000,
	0x0008000,
	0x0010000,
	0x0020000,
	0x0040000,
	0x0000000,
	0x0000000
};

static u32 card_latency[] =
{
	0x00000004,
	0x00000008,
	0x00000010,
	0x00000020,
	0x00000030,
	0x00000080,
	0x00000100,
	0x00000200
};

static u32 card_inited = 0;
static u32 crand_next = 1;

static u8 card_gamecode[4] = {0xff,0xff,0xff,0xff};
static u8 card_company[2] = {0xff,0xff};
static card_block cardmap[2];

static void __card_mountcallback(s32 chn,s32 result);
static void __erase_callback(s32 chn,s32 result);
static s32 __dounlock(s32 chn,u32 *key);
static s32 __card_readsegment(s32 chn,cardcallback callback);
static s32 __card_read(s32 chn,u32 address,u32 block_len,void *buffer,cardcallback callback);
static s32 __card_updatefat(s32 chn,struct card_bat *fatblock,cardcallback callback);
static s32 __card_updatedir(s32 chn,cardcallback callback);
static s32 __card_write(s32 chn,u32 address,u32 block_len,void *buffer,cardcallback callback);
static s32 __card_writepage(s32 chn,cardcallback callback);
static s32 __card_sectorerase(s32 chn,u32 sector,cardcallback callback);
static s32 __card_onreset(s32 final);

static sys_resetinfo card_resetinfo = {
	{},
	__card_onreset,
	127
};

extern unsigned long gettick();
extern long long gettime();
extern syssram* __SYS_LockSram();
extern syssramex* __SYS_LockSramEx();
extern u32 __SYS_UnlockSram(u32 write);
extern u32 __SYS_UnlockSramEx(u32 write);

static vu16* const _viReg = (u16*)0xCC002000;

/* new api */
static s32 __card_onreset(s32 final)
{
	if(final==FALSE) {
		if(CARD_Unmount(CARD_SLOTA)==-1) return 0;
		if(CARD_Unmount(CARD_SLOTB)==-1) return 0;
	}
	return 1;
}

static void __card_checksum(u16 *buff,u32 len,u16 *cs1,u16 *cs2)
{
	u32 i;
    *cs1 = 0;
	*cs2 = 0;
	len /= 2;
    for (i = 0; i < len; ++i) {
        *cs1 += buff[i];
        *cs2 += (buff[i] ^ 0xffff);
    }
    if (*cs1 == 0xffff) *cs1 = 0;
    if (*cs2 == 0xffff) *cs2 = 0;
}

static s32 __card_putcntrlblock(card_block *card,s32 result)
{
	u32 level;

	_CPU_ISR_Disable(level);
	if(card->attached) card->result = result;
	else if(card->result==CARD_ERROR_BUSY) card->result = result;
	_CPU_ISR_Restore(level);
	return result;
}

static s32 __card_getcntrlblock(s32 chn,card_block **card)
{
	s32 ret;
	u32 level;
	card_block *rcard = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return CARD_ERROR_FATAL_ERROR;

	_CPU_ISR_Disable(level);
	rcard = &cardmap[chn];
	if(!rcard->attached) {
		_CPU_ISR_Restore(level);
		return CARD_ERROR_NOCARD;
	}

	ret = CARD_ERROR_BUSY;
	if(rcard->result!=CARD_ERROR_BUSY) {
		rcard->result = CARD_ERROR_BUSY;
		rcard->card_api_cb = NULL;
		*card = rcard;
		ret = CARD_ERROR_READY;
	}
	_CPU_ISR_Restore(level);
	return ret;
}

static __inline__ struct card_dat* __card_getdirblock(card_block *card)
{
	return card->curr_dir;
}

static __inline__ struct card_bat* __card_getbatblock(card_block *card)
{
	return card->curr_fat;
}

static s32 __card_sync(s32 chn)
{
	s32 ret;
	u32 level;
	card_block *card = &cardmap[chn];

	_CPU_ISR_Disable(level);
	while((ret=CARD_GetErrorCode(chn))==CARD_ERROR_BUSY) {
		LWP_ThreadSleep(card->wait_sync_queue);
	}
	_CPU_ISR_Restore(level);
	return ret;
}

static void __card_synccallback(s32 chn,s32 result)
{
	u32 level;
	card_block *card = &cardmap[chn];
	_CPU_ISR_Disable(level);
	LWP_ThreadBroadcast(card->wait_sync_queue);
	_CPU_ISR_Restore(level);
}

static void __card_updateiconoffsets(struct card_direntry *entry,card_stat *stats)
{
	s32 i;
	u8 bnrfmt,nicons;
	u32 iconaddr,iconbase;

	iconaddr = entry->iconaddr;
	if(iconaddr==-1) {
		stats->banner_fmt = 0;
		stats->icon_fmt = 0;
		stats->icon_speed = 0;
		iconaddr = 0;
	}

	if(entry->bannerfmt&CARD_BANNER_MASK) {
		if(!(entry->bannerfmt&0x10)) {
			bnrfmt = (entry->bannerfmt&CARD_BANNER_MASK);
			if(bnrfmt==CARD_BANNER_CI) {
				stats->banner_fmt = bnrfmt;
				stats->offset_banner = iconaddr;
				stats->offset_banner_tlut = iconaddr+3072;
				iconaddr += (3072+512);
			} else if(bnrfmt==CARD_BANNER_RGB) {
				stats->banner_fmt = bnrfmt;
				stats->offset_banner = iconaddr;
				stats->offset_banner_tlut = -1;
				iconaddr += 6144;
			}
		} else {
			stats->offset_banner = -1;
			stats->offset_banner_tlut = -1;
		}
	}

	nicons = 0;
	for(i=0;i<CARD_MAXICONS;i++) {
		stats->iconfmt[i] = ((entry->iconfmt>>(i<<1))&CARD_ICON_MASK);
		stats->iconspeed[i] = ((entry->iconspeed>>(i<<1))&CARD_SPEED_MASK);
		if(stats->iconspeed[i]==0) stats->iconfmt[i] = 0;
		if(stats->iconfmt[i]) nicons++;
	}

	iconbase = iconaddr;
	for(i=0;i<CARD_MAXICONS;i++) {
		switch(stats->iconfmt[i]) {
			case 1:			//CARD_ICON_CI with shared palette
				stats->offset_icon[i] = iconaddr;
				stats->offset_icon_tlut[i] = iconbase + (nicons*1024);
				iconaddr += 1024;
				break;
			case 2:			//CARD_ICON_RGB
				stats->offset_icon[i] = iconaddr;
				stats->offset_icon_tlut[i] = -1;
				iconaddr += 3072;
				break;
			case 3:			//CARD_ICON_CI with own palette
				stats->offset_icon[i] = iconaddr;
				stats->offset_icon_tlut[i] = iconaddr + 1024;
				iconaddr += 1536;
				break;
			default:		//CARD_ICON_NONE
				stats->offset_icon[i] = -1;
				stats->offset_icon_tlut[i] = -1;
				break;

		}
	}
//	stats->offset_data = iconaddr;
}

static s32 __card_getstatusex(s32 chn,s32 fileno,struct card_direntry *entry)
{
	s32 ret;
	card_block *card = NULL;
	struct card_dat *dirblock = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return CARD_ERROR_NOCARD;
	if(fileno<0 || fileno>=CARD_MAXFILES) return CARD_ERROR_FATAL_ERROR;
	if((ret=__card_getcntrlblock(chn,&card))<0) return ret;

	ret = CARD_ERROR_BROKEN;
	dirblock = __card_getdirblock(card);
	if(dirblock) {
		ret = CARD_ERROR_READY;
		memcpy(entry,&dirblock->entries[fileno],sizeof(struct card_direntry));
	}
	return __card_putcntrlblock(card,ret);
}

static s32 __card_setstatusexasync(s32 chn,s32 fileno,struct card_direntry *entry,cardcallback callback)
{
	s32 ret,i,bend;
	card_block *card = NULL;
	struct card_dat *dirblock = NULL;
	struct card_direntry *entries = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return CARD_ERROR_NOCARD;
	if(fileno<0 || fileno>=CARD_MAXFILES) return CARD_ERROR_FATAL_ERROR;
	if(entry->filename[0]==0xff || entry->filename[0]==0) return CARD_ERROR_FATAL_ERROR;
	if((ret=__card_getcntrlblock(chn,&card))<0) return ret;

	ret = CARD_ERROR_BROKEN;
	dirblock = __card_getdirblock(card);
	if(dirblock) {
		i = 0; bend = 0;
		ret = CARD_ERROR_READY;
		entries = dirblock->entries;
		while(i<CARD_FILENAMELEN) {
			if(bend || entry->filename[i]==0) {
				entry->filename[i] = 0;
				bend = 1;
			}
			i++;
		}

		if(memcmp(entries[fileno].filename,entry->filename,CARD_FILENAMELEN)
			|| memcmp(entries[fileno].gamecode,entry->gamecode,4)
			|| memcmp(entries[fileno].company,entry->company,2)) {
			i = 0;
			while(i<CARD_MAXFILES) {
				if(i!=fileno && entries[i].gamecode[0]!=0xff
					&& memcmp(entries[i].gamecode,entry->gamecode,4)==0
					&& memcmp(entries[i].company,entry->company,2)==0
					&& memcmp(entries[i].filename,entry->filename,CARD_FILENAMELEN)==0) {
					return __card_putcntrlblock(card,CARD_ERROR_EXIST);
				}
				i++;
			}
			memcpy(entries[fileno].filename,entry->filename,CARD_FILENAMELEN);
			memcpy(entries[fileno].gamecode,entry->gamecode,4);
			memcpy(entries[fileno].company,entry->company,2);
		}

		entries[fileno].lastmodified = entry->lastmodified;
		entries[fileno].bannerfmt = entry->bannerfmt;
		entries[fileno].iconaddr = entry->iconaddr;
		entries[fileno].iconfmt = entry->iconfmt;
		entries[fileno].iconspeed = entry->iconspeed;
		entries[fileno].commentaddr = entry->commentaddr;
		entries[fileno].permission = entry->permission;
		entries[fileno].copytimes = entry->copytimes;

		if((ret=__card_updatedir(chn,callback))>=0) return ret;
	}
	return __card_putcntrlblock(card,ret);
}

static s32 __card_getfilenum(card_block *card,const char *filename,const char *gamecode,const char *company,s32 *fileno)
{
	u32 i = 0;
	struct card_direntry *entries = NULL;
	struct card_dat *dirblock = NULL;
	if(!card->attached) return CARD_ERROR_NOCARD;
	dirblock = __card_getdirblock(card);

	entries = dirblock->entries;
	for(i=0;i<CARD_MAXFILES;i++) {
		if(entries[i].gamecode[0]!=0xff) {
			if(strcmp(filename,(const char*)entries[i].filename)==0) {
				if((gamecode && gamecode[0]!=0xff && memcmp(entries[i].gamecode,gamecode,4)!=0)
					|| (company && company[0]!=0xff && memcmp(entries[i].company,company,2)!=0)) continue;

				*fileno = i;
				break;
			}
		}
	}
	if(i>=CARD_MAXFILES) return CARD_ERROR_NOFILE;
	return CARD_ERROR_READY;
}

static s32 __card_seek(card_file *file,s32 len,s32 offset,card_block **rcard)
{
	s32 ret;
	s32 i,entry_len;
	card_block *card = NULL;
	struct card_direntry *entry = NULL;
	struct card_dat *dirblock = NULL;
	struct card_bat *fatblock = NULL;
	if(file->filenum<0 || file->filenum>=CARD_MAXFILES) return CARD_ERROR_FATAL_ERROR;
	if((ret=__card_getcntrlblock(file->chn,&card))<0) return ret;
	if(file->iblock<CARD_SYSAREA || file->iblock>=card->blocks) {
		__card_putcntrlblock(card,CARD_ERROR_FATAL_ERROR);
		return CARD_ERROR_FATAL_ERROR;
	}

	dirblock = __card_getdirblock(card);
	entry = &dirblock->entries[file->filenum];
	if(entry->gamecode[0]!=0xff) {
		entry_len = entry->length*card->sector_size;
		if(entry_len<offset || entry_len<(offset+len)) {
			__card_putcntrlblock(card,CARD_ERROR_LIMIT);
			return CARD_ERROR_LIMIT;
		}
		card->curr_file = file;
		file->len = len;

		if(offset<file->offset) {
			file->offset = 0;
			file->iblock = entry->block;
			if(file->iblock<CARD_SYSAREA || file->iblock>=card->blocks) {
				__card_putcntrlblock(card,CARD_ERROR_BROKEN);
				return CARD_ERROR_BROKEN;
			}
		}

		fatblock = __card_getbatblock(card);
		for(i=file->iblock;i<card->blocks && file->offset<(offset&~(card->sector_size-1));i=file->iblock) {
			file->offset += card->sector_size;
			file->iblock = fatblock->fat[i-CARD_SYSAREA];
			if(file->iblock<CARD_SYSAREA || file->iblock>=card->blocks) {
				__card_putcntrlblock(card,CARD_ERROR_BROKEN);
				return CARD_ERROR_BROKEN;
			}
		}
		file->offset = offset;
		*rcard = card;
	}
	return CARD_ERROR_READY;
}

static u32 __card_checkdir(card_block *card,u32 *currdir)
{
	u32 dir,bad,bad_dir;
	u16 chksum0,chksum1;
	struct card_dircntrl *dircntrl[2];
	struct card_dat *dirblock[2];
	dir = 0;
	bad = 0;
	bad_dir = 0;
	while(dir<2) {
		dirblock[dir] = card->workarea+((dir+1)<<13);
		dircntrl[dir] = (card->workarea+((dir+1)<<13))+8128;
		__card_checksum((u16*)dirblock[dir],0x1ffc,&chksum0,&chksum1);
		if(chksum0!=dircntrl[dir]->chksum1 || chksum1!=dircntrl[dir]->chksum2) {
			card->curr_dir = NULL;
			bad_dir = dir;
			bad++;
		}
		dir++;
	}

	dir = bad_dir;
	if(!bad) {
		if(dircntrl[0]->updated<dircntrl[1]->updated) dir = 0;
		else dir = 1;
	}
	if(card->curr_dir==NULL) {
		card->curr_dir = dirblock[dir];
		memcpy(dirblock[dir],dirblock[dir^1],8192);
	}
	else if(card->curr_dir==dirblock[0]) dir = 0;
	else dir = 1;

	if(currdir) *currdir = dir;
	return bad;
}

static u32 __card_checkfat(card_block *card,u32 *currfat)
{
	u32 fat,bad,bad_fat;
	u16 chksum0,chksum1;
	struct card_bat *fatblock[2];
	fat = 0;
	bad = 0;
	bad_fat = 0;
	while(fat<2) {
		fatblock[fat] = card->workarea+((fat+3)<<13);
		__card_checksum((u16*)(((u32)fatblock[fat])+4),0x1ffc,&chksum0,&chksum1);
		if(chksum0!=fatblock[fat]->chksum1 || chksum1!=fatblock[fat]->chksum2) {
			card->curr_fat = NULL;
			bad_fat = fat;
			bad++;
		} else {
			u16 curblock = CARD_SYSAREA;
			u16 freeblocks = 0;
			while(curblock<card->blocks) {
				if(!fatblock[fat]->fat[curblock-CARD_SYSAREA]) freeblocks++;
				curblock++;
			}
			if(freeblocks!=fatblock[fat]->freeblocks) {
				card->curr_fat = NULL;
				bad_fat = fat;
				bad++;
			}
		}
		fat++;
	}

	fat = bad_fat;
	if(!bad) {
		if(fatblock[0]->updated<fatblock[1]->updated) fat = 0;
		else fat = 1;
	}
	if(card->curr_fat==NULL) {
		card->curr_fat = fatblock[fat];
		memcpy(fatblock[fat],fatblock[fat^1],8192);
	}
	else if(card->curr_fat==fatblock[0]) fat = 0;
	else fat = 1;

	if(currfat) *currfat = fat;
	return bad;
}

static s32 __card_verify(card_block *card)
{
	u32 ret = 0;

	ret += __card_checkdir(card,NULL);
	ret += __card_checkfat(card,NULL);
	if(ret<=2) {
		if(card->curr_dir && card->curr_fat) return CARD_ERROR_READY;
	}
	return CARD_ERROR_BROKEN;
}

static u32 __card_iscard(u32 id)
{
	u32 ret;
	u32 idx,tmp,secsize;

	if(id&~0xffff) return 0;
	if(id&0x03) return 0;

	ret = 0;
	tmp = id&0xfc;
	if(tmp==EXI_MEMCARD59 || tmp==EXI_MEMCARD123
		|| tmp==EXI_MEMCARD251 || tmp==EXI_MEMCARD507
		|| tmp==EXI_MEMCARD1019 || tmp==EXI_MEMCARD2043) {
		idx = _ROTL(id,23)&0x1c;
		if((secsize=card_sector_size[idx>>2])==0) return 0;
		tmp = ((tmp<<20)&0x1FFE0000)/secsize;
		if(tmp>8) ret = 1;
	}
	return ret;
}

static s32 __card_allocblock(s32 chn,u32 blocksneed,cardcallback callback)
{
	s32 ret;
	u16 block,currblock = 0,prevblock = 0;
	u32 i,count;
	card_block *card = NULL;
	struct card_bat *fatblock = NULL;
	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return CARD_ERROR_FATAL_ERROR;
	card = &cardmap[chn];

	if(!card->attached) return CARD_ERROR_NOCARD;
	fatblock = __card_getbatblock(card);

	if(fatblock->freeblocks<blocksneed) return CARD_ERROR_INSSPACE;

	// Add which blocks this file will take up into the FAT
	count = 0;
	block = 0xffff;
	currblock = fatblock->lastalloc;
	i = blocksneed;
	while(1) {
		if(i==0) {
			// Done allocating blocks
			fatblock->freeblocks -= blocksneed;
			fatblock->lastalloc = currblock;
			card->curr_fileblock = block;
			ret = __card_updatefat(chn,fatblock,callback);
			break;
		}

		/*
		  Since testing free space has already been done, if all the blocks
		  the file takes up cannot be entered into the FAT, something is
		  wrong.
		*/
		count++;
		if(count>=(card->blocks-CARD_SYSAREA)) return CARD_ERROR_BROKEN;

		currblock++;
	    if(currblock<CARD_SYSAREA || currblock>=card->blocks) currblock = CARD_SYSAREA;
		if(fatblock->fat[currblock-CARD_SYSAREA]==0) {
			if(block!=0xffff)
				fatblock->fat[prevblock-CARD_SYSAREA] = currblock;
			else
				block = currblock;

			fatblock->fat[currblock-CARD_SYSAREA] = 0xffff;
			prevblock = currblock;
			i--;
		}
	}
	return ret;
}

static s32 __card_freeblock(s32 chn,u16 block,cardcallback callback)
{
	u16 next = 0xffff,prev = 0xffff;
	card_block *card = NULL;
	struct card_bat *fatblock = NULL;
	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return CARD_ERROR_NOCARD;
	card = &cardmap[chn];

	if(!card->attached) return CARD_ERROR_NOCARD;

	fatblock = __card_getbatblock(card);
	next = fatblock->fat[block-CARD_SYSAREA];
	while(1) {
		if(next==0xffff) break;
		if(next<CARD_SYSAREA || next>=card->blocks) return CARD_ERROR_BROKEN;

		// Get the file's next block and clear the previous one from the fat
		prev = next;
		next = fatblock->fat[prev-CARD_SYSAREA];
		fatblock->fat[prev-CARD_SYSAREA] = 0;
		fatblock->freeblocks++;
	}
	return __card_updatefat(chn,fatblock,callback);
}

static s32 __card_unlockedhandler(s32 chn,s32 dev)
{
	s32 ret;
	cardcallback cb = NULL;
	card_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return CARD_ERROR_NOCARD;
	card = &cardmap[chn];

	ret = CARD_ERROR_READY;
	cb = card->card_unlock_cb;
	if(cb) {
		card->card_unlock_cb = NULL;
		if(EXI_Probe(chn)==0) ret = CARD_ERROR_NOCARD;
		cb(chn,ret);
	}
	return CARD_ERROR_UNLOCKED;
}

static s32 __card_readstatus(s32 chn,u8 *pstatus)
{
	u8 val[2];
	u32 err;
	s32 ret;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return CARD_ERROR_NOCARD;
	if(EXI_Select(chn,EXI_DEVICE_0,EXI_SPEED16MHZ)==0) return CARD_ERROR_NOCARD;

	err = 0;
	val[0] = 0x83; val[1] = 0x00;
	if(EXI_Imm(chn,val,2,EXI_WRITE,NULL)==0) err |= 0x01;
	if(EXI_Sync(chn)==0) err |= 0x02;
	if(EXI_Imm(chn,pstatus,1,EXI_READ,NULL)==0) err |= 0x04;
	if(EXI_Sync(chn)==0) err |= 0x08;
	if(EXI_Deselect(chn)==0) err |= 0x10;

	if(err) ret = CARD_ERROR_NOCARD;
	else ret = CARD_ERROR_READY;
	return ret;
}

static s32 __card_clearstatus(s32 chn)
{
	u8 val;
	u32 err;
	s32 ret;
	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return CARD_ERROR_NOCARD;
	if(EXI_Select(chn,EXI_DEVICE_0,EXI_SPEED16MHZ)==0) return CARD_ERROR_NOCARD;

	err = 0;
	val = 0x89;
	if(EXI_Imm(chn,&val,1,EXI_WRITE,NULL)==0) err |= 0x01;
	if(EXI_Sync(chn)==0) err |= 0x02;
	if(EXI_Deselect(chn)==0) err |= 0x04;

	if(err) ret = CARD_ERROR_NOCARD;
	else ret = CARD_ERROR_READY;

	return ret;
}

static s32 __card_enableinterrupt(s32 chn,u32 enable)
{
	u8 val[2];
	u32 err;
	s32 ret;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return CARD_ERROR_NOCARD;
	if(EXI_Select(chn,EXI_DEVICE_0,EXI_SPEED16MHZ)==0) return CARD_ERROR_NOCARD;

	err = 0;
	val[0] = 0x81;
	if(enable) val[1] = 0x01;
	else val[1] = 0x00;
	if(EXI_Imm(chn,val,2,EXI_WRITE,NULL)==0) err |= 0x01;
	if(EXI_Sync(chn)==0) err |= 0x02;
	if(EXI_Deselect(chn)==0) err |= 0x04;

	if(err) ret = CARD_ERROR_BUSY;
	else ret = CARD_ERROR_READY;

	return ret;
}

static s32 __card_txhandler(s32 chn,s32 dev)
{
	u32 err;
	s32 ret = CARD_ERROR_READY;
	cardcallback cb = NULL;
	card_block *card = NULL;
	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return 0;
	card = &cardmap[chn];

	err = 0;
	if(EXI_Deselect(chn)==0) ret |= err;
	if(EXI_Unlock(chn)==0) ret |= err;

	cb = card->card_tx_cb;
	if(cb) {
		card->card_tx_cb = NULL;
		if(!err) {
			if(EXI_Probe(chn)==0) ret = CARD_ERROR_NOCARD;
		} else ret = CARD_ERROR_NOCARD;
		cb(chn,ret);
	}
	return 1;
}

static void __timeouthandler(syswd_t alarm,void *cbarg)
{
	u32 chn;
	s32 ret = CARD_ERROR_READY;
	cardcallback cb;
	card_block *card = NULL;
	chn = 0;
	while(chn<EXI_CHANNEL_2) {
		card = &cardmap[chn];
		if(card->timeout_svc==alarm) break;
		chn++;
	}
	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return;

	if(card->attached) {
		EXI_RegisterEXICallback(chn,NULL);
		cb = card->card_exi_cb;
		if(cb) {
			card->card_exi_cb = NULL;
			ret = CARD_ERROR_IOERROR;
			cb(chn,ret);
		}
	}
}

static void __setuptimeout(card_block *card)
{
	struct timespec tb;
	SYS_CancelAlarm(card->timeout_svc);

	if(card->cmd[0]==0xf1 || card->cmd[0]==0xf4) {
		tb.tv_sec = 1*(card->sector_size/8192);
		tb.tv_nsec = 0;
		SYS_SetAlarm(card->timeout_svc,&tb,__timeouthandler,NULL);
	} else if(card->cmd[0]==0xf2) {
		tb.tv_sec = 0;
		tb.tv_nsec = 100*TB_NSPERMS;
		SYS_SetAlarm(card->timeout_svc,&tb,__timeouthandler,NULL);
	}
}

static s32 __retry(s32 chn)
{
	u32 len;
	card_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return CARD_ERROR_NOCARD;
	card = &cardmap[chn];
	if(EXI_Select(chn,EXI_DEVICE_0,EXI_SPEED16MHZ)==0) {
		EXI_Unlock(chn);
		return CARD_ERROR_NOCARD;
	}

	__setuptimeout(card);

	if(EXI_ImmEx(chn,card->cmd,card->cmd_len,EXI_WRITE)==0) {
		EXI_Deselect(chn);
		EXI_Unlock(chn);
		return CARD_ERROR_NOCARD;
	}

	if(card->cmd[0]==0x52) {
		if(EXI_ImmEx(chn,card->workarea+CARD_READSIZE,card->latency,EXI_WRITE)==0) {
			EXI_Deselect(chn);
			EXI_Unlock(chn);
			return CARD_ERROR_NOCARD;
		}
	}

	if(card->cmd_mode==-1) {
		EXI_Deselect(chn);
		EXI_Unlock(chn);
		return CARD_ERROR_READY;
	}

	len = 128;
	if(card->cmd[0]==0x52) len  = CARD_READSIZE;
	if(EXI_Dma(chn,card->cmd_usr_buf,len,card->cmd_mode,__card_txhandler)==0) {
		EXI_Deselect(chn);
		EXI_Unlock(chn);
		return CARD_ERROR_NOCARD;
	}
	return CARD_ERROR_READY;
}

static void __card_defaultapicallback(s32 chn,s32 result)
{
	return;
}

static s32 __card_exihandler(s32 chn,s32 dev)
{
	u8 status;
	s32 ret = CARD_ERROR_READY;
	card_block *card = NULL;
	cardcallback cb;
	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return 1;
	card = &cardmap[chn];

	SYS_CancelAlarm(card->timeout_svc);
	if(card->attached) {
		if(EXI_Lock(chn,EXI_DEVICE_0,NULL)==1) {
			if((ret=__card_readstatus(chn,&status))>=0
				&& (ret=__card_clearstatus(chn))>=0) {
				if(status&0x18) ret = CARD_ERROR_IOERROR;
				else ret = CARD_ERROR_READY;

				if(ret==CARD_ERROR_IOERROR) {
					if((--card->cmd_retries)>0) {
						ret = __retry(chn);
						if(ret<0) goto exit;
						return 1;
					}
				}
			}
			EXI_Unlock(chn);
		} else ret = CARD_ERROR_FATAL_ERROR;
exit:
		cb = card->card_exi_cb;
		if(cb) {
			card->card_exi_cb = NULL;
			cb(chn,ret);
		}
	}
	return 1;
}

static s32 __card_exthandler(s32 chn,s32 dev)
{
	cardcallback cb;
	card_block *card = NULL;
	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return 0;
	card = &cardmap[chn];

	if(card->attached) {
		if(card->card_tx_cb) {
			printf("error: card->card_tx_cb!=NULL\n");
		}
		card->attached = 0;
		EXI_RegisterEXICallback(chn,NULL);
		SYS_CancelAlarm(card->timeout_svc);

		cb = card->card_exi_cb;
		if(cb) {
			card->card_exi_cb = NULL;
			cb(chn,CARD_ERROR_NOCARD);
		}

		cb = card->card_ext_cb;
		if(cb) {
			card->card_ext_cb = NULL;
			cb(chn,CARD_ERROR_NOCARD);
		}

	}
	return 1;
}

static void __write_callback(s32 chn,s32 result)
{
	s32 ret;
	cardcallback cb = NULL;
	card_file *file = NULL;
	struct card_bat *fatblock = NULL;
	struct card_dat *dirblock = NULL;
	struct card_direntry *entry = NULL;
	card_block *card = &cardmap[chn];
	ret = result;
	if(ret>=0) {
		file = card->curr_file;
		if(file->len>=0) {
			file->len = (card->sector_size-file->len);
			if(file->len<=0) {
				dirblock = __card_getdirblock(card);
				entry = &dirblock->entries[file->filenum];
				entry->lastmodified = time(NULL);
				cb = card->card_api_cb;
				card->card_api_cb = NULL;
				if((ret=__card_updatedir(chn,cb))>=0) return;
			} else {
				fatblock = __card_getbatblock(card);
				file->offset += card->sector_size;
				file->iblock = fatblock->fat[file->iblock-CARD_SYSAREA];
				if(file->iblock<CARD_SYSAREA || file->iblock>=card->blocks) {
					ret = CARD_ERROR_BROKEN;
					goto exit;
				}
				if((ret=__card_sectorerase(chn,(file->iblock*card->sector_size),__erase_callback))>=0) return;
			}
		} else
			ret = CARD_ERROR_CANCELED;
	}

exit:
	cb = card->card_api_cb;
	card->card_api_cb = NULL;
	__card_putcntrlblock(card,ret);
	if(cb) cb(chn,ret);
}

static void __erase_callback(s32 chn,s32 result)
{
	s32 ret;
	cardcallback cb = NULL;
	card_file *file = NULL;
	card_block *card = &cardmap[chn];
	ret = result;
	if(ret>=0) {
		file = card->curr_file;
		if((ret=__card_write(chn,(file->iblock*card->sector_size),card->sector_size,card->cmd_usr_buf,__write_callback))>=0) return;
	}

	cb = card->card_api_cb;
	card->card_api_cb = NULL;
	__card_putcntrlblock(card,ret);
	if(cb) cb(chn,ret);
}

static void __read_callback(s32 chn,s32 result)
{
	s32 ret;
	s32 len;
	cardcallback cb = NULL;
	card_file *file = NULL;
	card_block *card = 0;
	struct card_bat *fatblock = NULL;
	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return;
	card = &cardmap[chn];

	ret = result;
	file = card->curr_file;
	if(ret>=0) {
		if(file->len>=0) {
			file->len = file->len-(((file->offset+card->sector_size)&~(card->sector_size-1))-file->offset);
			if(file->len>0) {
				fatblock = __card_getbatblock(card);
				file->offset += (((file->offset+card->sector_size)&~(card->sector_size-1))-file->offset);
				file->iblock = fatblock->fat[file->iblock-CARD_SYSAREA];
				if(file->iblock<CARD_SYSAREA || file->iblock>=card->blocks) {
					ret = CARD_ERROR_BROKEN;
					goto exit;
				}
				len = file->len<card->sector_size?card->sector_size:file->len;
				if(__card_read(chn,(file->iblock*card->sector_size),len,card->cmd_usr_buf,__read_callback)>=0) return;

			}
		} else
			ret = CARD_ERROR_CANCELED;
	}

exit:
	cb = card->card_api_cb;
	card->card_api_cb = NULL;
	__card_putcntrlblock(card,ret);
	if(cb) cb(chn,ret);
}

static void __delete_callback(s32 chn,s32 result)
{
	s32 ret;
	cardcallback cb = NULL;
	card_block *card = &cardmap[chn];
	cb = card->card_api_cb;
	card->card_api_cb = NULL;

	ret = result;
	if(ret>=0 && (ret=__card_freeblock(chn,card->curr_fileblock,cb))>=0) return;

	__card_putcntrlblock(card,ret);
	if(cb) cb(chn,ret);
}

static void __format_callback(s32 chn,s32 result)
{
	s32 ret;
	cardcallback cb = NULL;
	card_block *card = &cardmap[chn];

	ret = result;
	if(ret>=0) {
		if((++card->format_step)<CARD_SYSAREA) {
			if((ret=__card_sectorerase(chn,(card->format_step*card->sector_size),__format_callback))>=0) return;
			goto exit;
		}
		if(card->format_step<10) {
			if((ret=__card_write(chn,((card->format_step-CARD_SYSAREA)*card->sector_size),8192,card->workarea+((card->format_step-CARD_SYSAREA)<<13),__format_callback))>=0) return;
			goto exit;
		}

		card->curr_dir = card->workarea+CARD_SYSDIR;
		memcpy(card->curr_dir,card->workarea+CARD_SYSDIR_BACK,8192);

		card->curr_fat = card->workarea+CARD_SYSBAT;
		memcpy(card->curr_fat,card->workarea+CARD_SYSBAT_BACK,8192);
	}
exit:
	cb = card->card_api_cb;
	card->card_api_cb = NULL;
	__card_putcntrlblock(card,ret);
	if(cb) cb(chn,ret);
}

static void __blockwritecallback(s32 chn,s32 result)
{
	s32 ret = CARD_ERROR_READY;
	cardcallback cb = NULL;
	card_block *card = &cardmap[chn];
	ret = result;
	if(ret>=0) {
		card->transfer_cnt += 128;
		card->cmd_sector_addr += 128;
		card->cmd_usr_buf += 128;
		if((--card->cmd_blck_cnt)>0) {
			if((ret=__card_writepage(chn,__blockwritecallback))>=CARD_ERROR_READY) return;
		}
	}

	if(!card->card_api_cb) __card_putcntrlblock(card,ret);

	cb = card->card_xfer_cb;
	if(cb) {
		card->card_xfer_cb = NULL;
		cb(chn,ret);
	}
}

static void __blockreadcallback(s32 chn,s32 result)
{
	s32 ret = CARD_ERROR_READY;
	cardcallback cb = NULL;
	card_block *card = &cardmap[chn];
	ret = result;
	if(ret>=0) {
		card->transfer_cnt += CARD_READSIZE;
		card->cmd_sector_addr += CARD_READSIZE;
		card->cmd_usr_buf += CARD_READSIZE;
		if((--card->cmd_blck_cnt)>0) {
			if((ret=__card_readsegment(chn,__blockreadcallback))>=CARD_ERROR_READY) return;
		}
	}

	if(!card->card_api_cb) __card_putcntrlblock(card,ret);
	cb = card->card_xfer_cb;
	if(cb) {
		card->card_xfer_cb = NULL;
		cb(chn,ret);
	}
}

static void __unlocked_callback(s32 chn,s32 result)
{
	s32 ret;
	card_block *card;
	cardcallback cb;
	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return;
	card = &cardmap[chn];

	ret = result;
	if(ret>=0) {
		card->card_unlock_cb = __unlocked_callback;
		if(EXI_Lock(chn,EXI_DEVICE_0,__card_unlockedhandler)==1) {
			card->card_unlock_cb = NULL;
			ret = __retry(chn);
		} else
			ret = 0;
	}
	if(ret<0) {
		if(card->cmd[0]==0xf3 || card->cmd[0]>=0xf5) return;
		else if(card->cmd[0]==0x52) {
			cb = card->card_tx_cb;
			if(cb) {
				card->card_tx_cb = NULL;
				cb(chn,ret);
			}
		} else if(card->cmd[0]>=0xf1) {
			cb = card->card_exi_cb;
			if(cb) {
				card->card_exi_cb = NULL;
				cb(chn,ret);
			}
		}
	}
}

static s32 __card_start(s32 chn,cardcallback tx_cb,cardcallback exi_cb)
{
	u32 level;
	card_block *card = NULL;
	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return CARD_ERROR_NOCARD;
	card = &cardmap[chn];

	_CPU_ISR_Disable(level);
	if(tx_cb) card->card_tx_cb = tx_cb;
	if(exi_cb) card->card_exi_cb = exi_cb;

	card->card_unlock_cb = __unlocked_callback;
	if(EXI_Lock(chn,EXI_DEVICE_0,__card_unlockedhandler)==0) {
		_CPU_ISR_Restore(level);
		return CARD_ERROR_BUSY;
	}
	card->card_unlock_cb = NULL;

	if(EXI_Select(chn,EXI_DEVICE_0,EXI_SPEED16MHZ)==0) {
		EXI_Unlock(chn);
		_CPU_ISR_Restore(level);
		return CARD_ERROR_NOCARD;
	}

	__setuptimeout(card);
	_CPU_ISR_Restore(level);

	return CARD_ERROR_READY;
}

static s32 __card_writepage(s32 chn,cardcallback callback)
{
	s32 ret;
	card_block *card = NULL;
	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return CARD_ERROR_NOCARD;
	card = &cardmap[chn];

	card->cmd[0] = 0xf2;
	card->cmd[1] = (card->cmd_sector_addr>>17)&0x3f;
	card->cmd[2] = (card->cmd_sector_addr>>9)&0xff;
	card->cmd[3] = (card->cmd_sector_addr>>7)&3;
	card->cmd[4] = card->cmd_sector_addr&0x7f;
	card->cmd_len = 5;
	card->cmd_mode = EXI_WRITE;
	card->cmd_retries = 3;

	ret = __card_start(chn,NULL,callback);
	if(ret<0) return ret;

	if(EXI_ImmEx(chn,card->cmd,card->cmd_len,EXI_WRITE)==1
		&& EXI_Dma(chn,card->cmd_usr_buf,128,card->cmd_mode,__card_txhandler)==1) return CARD_ERROR_READY;

	card->card_exi_cb = NULL;
	EXI_Deselect(chn);
	EXI_Unlock(chn);
	return CARD_ERROR_NOCARD;
}

static s32 __card_readsegment(s32 chn,cardcallback callback)
{
	u32 err;
	s32 ret;
	card_block *card = NULL;
	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return CARD_ERROR_NOCARD;
	card = &cardmap[chn];

	card->cmd[0] = 0x52;
	card->cmd[1] = (card->cmd_sector_addr&0xFE0000)>>17;
	card->cmd[2] = (card->cmd_sector_addr&0x01FE00)>>9;
	card->cmd[3] = (card->cmd_sector_addr&0x000180)>>7;
	card->cmd[4] = (card->cmd_sector_addr&0x00007F);
	card->cmd_len = 5;
	card->cmd_mode = EXI_READ;
	card->cmd_retries = 0;

	ret = __card_start(chn,callback,NULL);
	if(ret<0) return ret;

	err = 0;
	if(EXI_ImmEx(chn,card->cmd,card->cmd_len,EXI_WRITE)==0) err |= 0x01;
	if(EXI_ImmEx(chn,card->workarea+CARD_READSIZE,card->latency,EXI_WRITE)==0) err |= 0x02;
	if(EXI_Dma(chn,card->cmd_usr_buf,CARD_READSIZE,card->cmd_mode,__card_txhandler)==0) err |= 0x04;

	if(err) {
		card->card_tx_cb = NULL;
		EXI_Deselect(chn);
		EXI_Unlock(chn);
		return CARD_ERROR_NOCARD;
	}
	return CARD_ERROR_READY;
}

static void __card_fatwritecallback(s32 chn,s32 result)
{
	s32 ret;
	cardcallback cb = NULL;
	struct card_bat *fat1,*fat2;
	card_block *card = &cardmap[chn];
	ret = result;
	if(ret>=0) {
		fat1 = (card->workarea+0x6000);
		fat2 = (card->workarea+0x8000);
		if(card->curr_fat==fat1) {
			card->curr_fat = fat2;
			memcpy(fat2,fat1,8192);
		} else {
			card->curr_fat = fat1;
			memcpy(fat1,fat2,8192);
		}
	}
	if(!card->card_api_cb) __card_putcntrlblock(card,ret);
	cb = card->card_erase_cb;
	if(cb) {
		card->card_erase_cb = NULL;
		cb(chn,ret);
	}
}

static void __card_dirwritecallback(s32 chn,s32 result)
{
	s32 ret;
	cardcallback cb = NULL;
	struct card_dat *dir1,*dir2;
	card_block *card = &cardmap[chn];
	ret = result;
	if(ret>=0) {
		dir1 = (card->workarea+0x2000);
		dir2 = (card->workarea+0x4000);
		if(card->curr_dir==dir1) {
			card->curr_dir = dir2;
			memcpy(dir2,dir1,8192);
		} else {
			card->curr_dir = dir1;
			memcpy(dir1,dir2,8192);
		}
	}
	if(!card->card_api_cb) __card_putcntrlblock(card,ret);
	cb = card->card_erase_cb;
	if(cb) {
		card->card_erase_cb = NULL;
		cb(chn,ret);
	}
}

static s32 __card_write(s32 chn,u32 address,u32 block_len,void *buffer,cardcallback callback)
{
	s32 ret;
	card_block *card = NULL;
	if(chn<EXI_CHANNEL_0 || chn>= EXI_CHANNEL_2) return CARD_ERROR_FATAL_ERROR;
	card = &cardmap[chn];

	if(!card->attached) return CARD_ERROR_NOCARD;

	card->cmd_blck_cnt = block_len>>7;
	card->cmd_sector_addr = address;
	card->cmd_usr_buf = buffer;
	card->card_xfer_cb = callback;
	ret = __card_writepage(chn,__blockwritecallback);

	return ret;
}

static s32 __card_read(s32 chn,u32 address,u32 block_len,void *buffer,cardcallback callback)
{
	s32 ret;
	card_block *card = NULL;
	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return CARD_ERROR_NOCARD;
	card = &cardmap[chn];

	card->cmd_sector_addr = address;
	card->cmd_blck_cnt = block_len>>9;
	card->cmd_usr_buf = buffer;
	card->card_xfer_cb = callback;
	ret = __card_readsegment(chn,__blockreadcallback);

	return ret;
}

static s32 __card_formatregion(s32 chn,u32 encode,cardcallback callback)
{
	s32 ret;
	u16 tmp;
	u32 cnt;
	u64 time;
	u64 rnd_val;
	void *workarea,*memblock;
	cardcallback cb = NULL;
	card_block *card = NULL;
	struct card_header *header;
	struct card_bat *fatblock = NULL;
	struct card_dircntrl *dircntrl = NULL;
	syssram *sram;
	syssramex *sramex;
	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return CARD_ERROR_NOCARD;

	if((ret=__card_getcntrlblock(chn,&card))<0) return ret;

	header = workarea = card->workarea;
	memset(header,0xff,8192);

	tmp = _viReg[55];
	header->encoding = encode;

	sram = __SYS_LockSram();
	header->serial[5] = sram->counter_bias;
	header->serial[6] = sram->lang;
	__SYS_UnlockSram(0);

	cnt = 0;
	rnd_val = time = gettime();
	sramex = __SYS_LockSramEx();
	while(cnt<12) {
		rnd_val = (((rnd_val*(u64)0x0000000041c64e6d)+(u64)0x0000000000003039)>>16);
		((u8*)header->serial)[cnt] = (sramex->flash_id[chn][cnt]+(u32)rnd_val);

		rnd_val = (((rnd_val*(u64)0x0000000041c64e6d)+(u64)0x0000000000003039)>>16);
		rnd_val &= (u64)0x0000000000007fff;

		cnt++;
	}
	__SYS_UnlockSramEx(0);

	*(u64*)&(header->serial[3]) = time;
	header->serial[7] = tmp;
	header->device_id = 0;
	header->size = card->card_size;
	__card_checksum((u16*)header,508,&header->chksum1,&header->chksum2);

	cnt = 0;
	while(cnt<2) {
		memblock = workarea+((cnt+1)<<13);
		dircntrl = memblock+8128;
		memset(memblock,0xff,8192);
		__card_checksum(memblock,8188,&dircntrl->chksum1,&dircntrl->chksum2);
		cnt++;
	}

	cnt = 0;
	while(cnt<2) {
		memblock = workarea+((cnt+3)<<13);
		fatblock = memblock;
		memset(memblock,0,8192);
		fatblock->updated = cnt;
		fatblock->freeblocks = card->blocks-CARD_SYSAREA;
		fatblock->lastalloc = 4;
		__card_checksum(memblock+4,8188,&fatblock->chksum1,&fatblock->chksum2);
		cnt++;
	}

	cb = callback;
	if(!cb) cb = __card_defaultapicallback;
	card->card_api_cb = cb;

	DCStoreRange(card->workarea,0xA000);

	card->format_step = 0;
	if((ret=__card_sectorerase(chn,(card->sector_size*card->format_step),__format_callback))>=0) return ret;

	__card_putcntrlblock(card,ret);
	return ret;
}

static s32 __card_sectorerase(s32 chn,u32 sector,cardcallback callback)
{
	s32 ret;
	card_block *card = NULL;
	if(chn<EXI_CHANNEL_0 || chn>= EXI_CHANNEL_2) return CARD_ERROR_FATAL_ERROR;
	card = &cardmap[chn];

	if(sector%card->sector_size) return CARD_ERROR_FATAL_ERROR;

	card->cmd[0] = 0xf1;
	card->cmd[1] = (sector>>17)&0x7f;
	card->cmd[2] = (sector>>9)&0xff;
	card->cmd_len = 3;
	card->cmd_mode = -1;
	card->cmd_retries = 3;

	ret = __card_start(chn,NULL,callback);
	if(ret<0) return ret;

	if(EXI_ImmEx(chn,card->cmd,card->cmd_len,EXI_WRITE)==0) {
		card->card_exi_cb = NULL;
		return CARD_ERROR_NOCARD;
	}

	EXI_Deselect(chn);
	EXI_Unlock(chn);
	return ret;
}

static void __card_faterasecallback(s32 chn,s32 result)
{
	s32 ret;
	cardcallback cb = NULL;
	struct card_bat *fatblock = NULL;
	card_block *card = &cardmap[chn];
	ret = result;
	if(ret>=0) {
		fatblock = __card_getbatblock(card);
		if((ret=__card_write(chn,(((u32)fatblock-(u32)card->workarea)>>13)*card->sector_size,8192,fatblock,__card_fatwritecallback))>=0) return;
	}
	if(!card->card_api_cb) __card_putcntrlblock(card,ret);

	cb = card->card_erase_cb;
	if(cb) {
		card->card_erase_cb = NULL;
		cb(chn,ret);
	}
}

static void __card_direrasecallback(s32 chn,s32 result)
{
	s32 ret;
	cardcallback cb = NULL;
	struct card_dat *dirblock = NULL;
	card_block *card = &cardmap[chn];
	ret = result;
	if(ret>=0) {
		dirblock = __card_getdirblock(card);
		if((ret=__card_write(chn,(((u32)dirblock-(u32)card->workarea)>>13)*card->sector_size,8192,dirblock,__card_dirwritecallback))>=0) return;
	}
	if(!card->card_api_cb) __card_putcntrlblock(card,ret);

	cb = card->card_erase_cb;
	if(cb) {
		card->card_erase_cb = NULL;
		cb(chn,ret);
	}
}

static void __card_createfatcallback(s32 chn,s32 result)
{
	s32 ret;
	cardcallback cb = NULL;
	card_file *file = NULL;
	struct card_direntry *entry = NULL;
	struct card_dat *dirblock = NULL;
	card_block *card = &cardmap[chn];
	cb = card->card_api_cb;
	card->card_api_cb = NULL;

	dirblock = __card_getdirblock(card);

	file = card->curr_file;
	entry = &dirblock->entries[file->filenum];

	memset(entry->gamecode,0,4);
	memset(entry->company,0,2);
	if(card_gamecode[0]!=0xff) memcpy(entry->gamecode,card_gamecode,4);
	if(card_gamecode[0]!=0xff) memcpy(entry->company,card_company,2);
	entry->block = card->curr_fileblock;
	entry->permission = CARD_ATTRIB_PUBLIC;
	entry->pad_00 = 0xff;
	entry->copytimes = 0;
	entry->iconaddr = -1;
	entry->iconfmt = 0;
	entry->iconspeed = 0;
	entry->pad_01 = 0xffff;
	entry->iconspeed = (entry->iconspeed&~CARD_SPEED_MASK)|CARD_SPEED_FAST;
	entry->lastmodified = time(NULL);

	file->offset = 0;
	file->iblock = card->curr_fileblock;

	if((ret=__card_updatedir(chn,cb))<0) {
		__card_putcntrlblock(card,ret);
		if(cb) cb(chn,ret);
	}
}

static s32 __card_updatefat(s32 chn,struct card_bat *fatblock,cardcallback callback)
{
	card_block *card = NULL;
	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return CARD_ERROR_FATAL_ERROR;
	card = &cardmap[chn];

	if(!card->attached) return CARD_ERROR_NOCARD;

	++fatblock->updated;
	__card_checksum((u16*)(((u32)fatblock)+4),0x1ffc,&fatblock->chksum1,&fatblock->chksum2);
	DCStoreRange(fatblock,8192);
	card->card_erase_cb = callback;

	return __card_sectorerase(chn,(((u32)fatblock-(u32)card->workarea)>>13)*card->sector_size,__card_faterasecallback);
}

static s32 __card_updatedir(s32 chn,cardcallback callback)
{
	card_block *card = NULL;
	void *dirblock = NULL;
	struct card_dircntrl *dircntrl = NULL;
	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return CARD_ERROR_FATAL_ERROR;
	card = &cardmap[chn];

	if(!card->attached) return CARD_ERROR_NOCARD;

	dirblock = __card_getdirblock(card);
	dircntrl = dirblock+8128;
	++dircntrl->updated;
	__card_checksum((u16*)dirblock,0x1ffc,&dircntrl->chksum1,&dircntrl->chksum2);
	DCStoreRange(dirblock,0x2000);
	card->card_erase_cb = callback;

	return __card_sectorerase(chn,(((u32)dirblock-(u32)card->workarea)>>13)*card->sector_size,__card_direrasecallback);
}

static void __card_dounmount(s32 chn,s32 result)
{
	u32 level;
	card_block *card;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return;
	card = &cardmap[chn];

	_CPU_ISR_Disable(level);
	if(card->attached) {
		card->attached = 0;
		card->mount_step = 0;
		card->result = result;
		EXI_RegisterEXICallback(chn,NULL);
		EXI_Detach(chn);
		SYS_CancelAlarm(card->timeout_svc);
	}
	_CPU_ISR_Restore(level);
}

static s32 __card_domount(s32 chn)
{
	u8 status,kval;
	s32 ret = CARD_ERROR_READY;
	u32 sum;
	u32 id,idx,cnt;
	card_block *card;
	syssramex *sramex;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return CARD_ERROR_NOCARD;
	card = &cardmap[chn];
	if(card->mount_step==0) {
		ret = 0;
		id = 0;
		if(EXI_GetID(chn,EXI_DEVICE_0,&id)==0) ret = CARD_ERROR_NOCARD;
		else if(!__card_iscard(id)) ret = CARD_ERROR_WRONGDEVICE;

		if(ret<0) goto exit;
		card->cid = id;
		card->card_size = (id&0xfc);
		if(card->card_size) {
			idx = _ROTL(id,23)&0x1c;
			card->sector_size = card_sector_size[idx>>2];
			card->blocks = ((card->card_size<<20)>>3)/card->sector_size;

			if(card->blocks>0x0008) {
				idx = _ROTL(id,26)&0x1c;
				card->latency = card_latency[idx>>2];

				if((ret=__card_clearstatus(chn))<0) goto exit;
				if((ret=__card_readstatus(chn,&status))<0) goto exit;

				if(EXI_Probe(chn)==0) {
					ret = CARD_ERROR_NOCARD;
					goto exit;
				}
				if(!(status&CARD_STATUS_UNLOCKED)) {
					if((ret=__dounlock(chn,card->key))<0) goto exit;

					cnt = 0;
					sum = 0;
					sramex = __SYS_LockSramEx();
					while(cnt<12) {
						kval = ((u8*)card->key)[cnt];
						sramex->flash_id[chn][cnt] = kval;
						sum += kval;
						cnt++;
					}
					sum = (sum^-1)&0xff;
					sramex->flashID_chksum[chn] = (sum<<8)|sum;
					__SYS_UnlockSramEx(1);
					return ret;
				}
				card->mount_step = 1;

				cnt = 0;
				sum = 0;
				sramex = __SYS_LockSramEx();
				while(cnt<12) {
					sum += sramex->flash_id[chn][cnt];
					cnt++;
				}
				cnt = sramex->flashID_chksum[chn];
				__SYS_UnlockSramEx(0);

				sum = (sum^-1)&0xff;
				sum |= (sum<<8);
				if(cnt!=sum) {
					ret = CARD_ERROR_IOERROR;
					goto exit;
				}
			}
		}
	}
	if(card->mount_step==1) {
		card->mount_step = 2;
		if((ret=__card_enableinterrupt(chn,1))<0) goto exit;
		EXI_RegisterEXICallback(chn,__card_exihandler);
		EXI_Unlock(chn);

		DCInvalidateRange(card->workarea,0xA000);
	}

	if((ret=__card_read(chn,(card->sector_size*(card->mount_step-2)),card->sector_size,card->workarea+((card->mount_step-2)<<13),__card_mountcallback))<0) goto exit;
	return ret;

exit:
	EXI_Unlock(chn);
	__card_dounmount(chn,ret);

	return ret;
}

static void __card_mountcallback(s32 chn,s32 result)
{
	s32 ret;
	cardcallback cb;
	card_block *card = &cardmap[chn];

	ret = result;
	if(ret==CARD_ERROR_NOCARD || ret==CARD_ERROR_IOERROR) {
		__card_dounmount(chn,ret);
		__card_putcntrlblock(card,ret);
	}else if(ret==CARD_ERROR_UNLOCKED) {
		if((ret=__card_domount(chn))>=0) return;
	} else {
		if((++card->mount_step)<7) {
			if((ret=__card_domount(chn))>=0) return;
		} else {
			ret = __card_verify(card);
			__card_putcntrlblock(card,ret);
		}
	}

	cb = card->card_api_cb;
	card->card_api_cb = NULL;
	if(cb) cb(chn,ret);
}

static __inline__ void __card_srand(u32 val)
{
	crand_next = val;
}

static __inline__ u32 __card_rand()
{
	crand_next = (crand_next*0x41C64E6D)+12345;
	return _SHIFTR(crand_next,16,15);
}

static u32 __card_initval()
{
	u32 ticks = gettick();

	__card_srand(ticks);
	return ((0x7FEC8000|__card_rand())&~0x00000fff);
}

static u32 __card_dummylen()
{
	u32 ticks = gettick();
	u32 val = 0,cnt = 0,shift = 1;

	__card_srand(ticks);
	val = (__card_rand()&0x1f)+1;

	do {
		ticks = gettick();
		val = ticks<<shift;
		shift++;
		if(shift>16) shift = 1;
		__card_srand(val);
		val = (__card_rand()&0x1f)+1;
		cnt++;
	}while(val<4 && cnt<10);
	if(val<4) val = 4;

	return val;

}

static u32 exnor_1st(u32 a,u32 b)
{
	u32 c,d,e,f,r1,r2,r3,r4;

	c = 0;
	while(c<b) {
		d = (a>>23);
		e = (a>>15);
		f = (a>>7);
		r1 = (a^f);
		r2 = (e^r1);
		r3 = ~(d^r2);		//eqv(d,r2)
		e = (a>>1);
		r4 = ((r3<<30)&0x40000000);
		a = (e|r4);
		c++;
	};
	return a;
}

static u32 exnor(u32 a,u32 b)
{
	u32 c,d,e,f,r1,r2,r3,r4;

	c = 0;
	while(c<b) {
		d = (a<<23);
		e = (a<<15);
		f = (a<<7);
		r1 = (a^f);
		r2 = (e^r1);
		r3 = ~(d^r2);		//eqv(d,r2)
		e = (a<<1);
		r4 = ((r3>>30)&0x02);
		a = (e|r4);
		c++;
	};
	return a;
}

static u32 bitrev(u32 val)
{
	u32 cnt,val1,ret,shift,shift1;

	cnt = 0;
	ret = 0;
	shift = 1;
	shift1 = 0;
	while(cnt<32) {
		if(cnt<=15) {
			val1 = val&(1<<cnt);
			val1 <<= ((31-cnt)-shift1);
			ret |= val1;
			shift1++;
		} else if(cnt==31) {
			val1 = val>>31;
			ret |= val1;
		} else {
			val1 = 1;
			val1 = val&(1<<cnt);
			val1 >>= shift;
			ret |= val1;
			shift += 2;
		}
		cnt++;
	}
	return ret;
}

static s32 __card_readarrayunlock(s32 chn,u32 address,void *buffer,u32 len,u32 flag)
{
	s32 ret;
	u32 err;
	u8 regbuf[5];
	card_block *card = &cardmap[chn];
	if(EXI_Select(chn,EXI_DEVICE_0,EXI_SPEED16MHZ)==0) return CARD_ERROR_NOCARD;

	address &= 0xFFFFF000;
	memset(regbuf,0,5);

	regbuf[0] = 0x52;
	if(!flag) {
		regbuf[1] = ((address&0x60000000)>>29)&0xff;
		regbuf[2] = ((address&0x1FE00000)>>21)&0xff;
		regbuf[3] = ((address&0x00180000)>>19)&0xff;
		regbuf[4] = ((address&0x0007F000)>>12)&0xff;
	} else {
		regbuf[1] = (address>>24)&0xff;
		regbuf[2] = ((address&0x00FF0000)>>16)&0xff;
	}

	err = 0;
	if(EXI_ImmEx(chn,regbuf,5,EXI_WRITE)==0) err |= 0x01;
	if(EXI_ImmEx(chn,card->workarea+CARD_READSIZE,card->latency,EXI_WRITE)==0) err |= 0x02;
	if(EXI_ImmEx(chn,buffer,len,EXI_READ)==0) err |= 0x04;
	if(EXI_Deselect(chn)==0) err |= 0x08;

	if(err) ret = CARD_ERROR_NOCARD;
	else ret = CARD_ERROR_READY;

	return ret;
}

static void __dsp_initcallback(dsptask_t *task)
{
	u32 chn;
	card_block *card = NULL;
	chn = 0;
	while(chn<EXI_CHANNEL_2) {
		card = &cardmap[chn];
		if(&card->dsp_task==task) break;
		chn++;
	}
	if(chn>=EXI_CHANNEL_2) return;

	DSP_SendMailTo(0xFF000000);
	while(DSP_CheckMailTo());
	DSP_SendMailTo((u32)card->workarea);
	while(DSP_CheckMailTo());
}

static u8 tmp_buffer[64] ATTRIBUTE_ALIGN(32);
static void __dsp_donecallback(dsptask_t *task)
{

	u8 status;
	s32 ret;
	u32 chn,len,key;
	u32 workarea,val;
	card_block *card = NULL;
	chn = 0;
	while(chn<EXI_CHANNEL_2) {
		card = &cardmap[chn];
		if(&card->dsp_task==task) break;
		chn++;
	}
	if(chn>=EXI_CHANNEL_2) return;

	workarea = (u32)card->workarea;
	workarea = ((workarea+47)&~0x1f);
	key = ((u32*)workarea)[8];

	val = (key^card->cipher)&~0xffff;
	len = __card_dummylen();
	if(__card_readarrayunlock(chn,val,tmp_buffer,len,1)<0) {
		EXI_Unlock(chn);
		__card_mountcallback(chn,CARD_ERROR_NOCARD);
		return;
	}

	val = exnor(card->cipher,((len+card->latency+4)<<3)+1);
	{
		u32 a,b,c,r1,r2,r3;
		a = (val<<23);
		b = (val<<15);
		c = (val<<7);
		r1 = (val^c);
		r2 = (b^r1);
		r3 = ~(a^r2);		//eqv(a,r2)
		r1 = (val|(r3>>31));
		card->cipher = r1;
	}

	val = ((key<<16)^card->cipher)&~0xffff;
	len = __card_dummylen();
	if(__card_readarrayunlock(chn,val,tmp_buffer,len,1)<0) {
		EXI_Unlock(chn);
		__card_mountcallback(chn,CARD_ERROR_NOCARD);
		return;
	}

	ret = __card_readstatus(chn,&status);
	if(EXI_Probe(chn)==0) {
		EXI_Unlock(chn);
		__card_mountcallback(chn,CARD_ERROR_NOCARD);
		return;
	}
	if(!ret && !(status&CARD_STATUS_UNLOCKED)) {
		EXI_Unlock(chn);
		ret = CARD_ERROR_IOERROR;
	}
	__card_mountcallback(chn,ret);
}

static s32 __dounlock(s32 chn,u32 *key)
{
	u32 array_addr,len,val;
	u32 a,b,c,d,e;
	card_block *card = &cardmap[chn];
	u32 *workarea = card->workarea;
	u32 *cipher1 = (u32*)(((u32)card->workarea+47)&~31);
	u32 *cipher2 = &cipher1[8];
	array_addr = __card_initval();
	len = __card_dummylen();

	if(__card_readarrayunlock(chn,array_addr,tmp_buffer,len,0)<0) return CARD_ERROR_NOCARD;

	val = exnor_1st(array_addr,(len<<3)+1);
	{
		u32 a,b,c,r1,r2,r3;
		a = (val>>23);
		b = (val>>15);
		c = (val>>7);
		r1 = (val^c);
		r2 = (b^r1);
		r3 = ~(a^r2);		//eqv(a,r2)
		r1 = (val|(r3<<31));
		card->cipher = r1;
	}
	card->cipher = bitrev(card->cipher);

	array_addr = 0;
	len = __card_dummylen();
	if(__card_readarrayunlock(chn,array_addr,tmp_buffer,len+20,1)<0) return CARD_ERROR_NOCARD;

	a = ((u32*)tmp_buffer)[0];
	b = ((u32*)tmp_buffer)[1];
	c = ((u32*)tmp_buffer)[2];
	d = ((u32*)tmp_buffer)[3];
	e = ((u32*)tmp_buffer)[4];

	a = a^card->cipher;
	val = exnor(card->cipher,32);
	{
		u32 a,b,c,r1,r2,r3;
		a = (val<<23);
		b = (val<<15);
		c = (val<<7);
		r1 = (val^c);
		r2 = (b^r1);
		r3 = ~(a^r2);		//eqv(a,r2)
		r1 = (val|(r3>>31));
		card->cipher = r1;
	}

	b = b^card->cipher;
	val = exnor(card->cipher,32);
	{
		u32 a,b,c,r1,r2,r3;
		a = (val<<23);
		b = (val<<15);
		c = (val<<7);
		r1 = (val^c);
		r2 = (b^r1);
		r3 = ~(a^r2);		//eqv(a,r2)
		r1 = (val|(r3>>31));
		card->cipher = r1;
	}

	c = c^card->cipher;
	val = exnor(card->cipher,32);
	{
		u32 a,b,c,r1,r2,r3;
		a = (val<<23);
		b = (val<<15);
		c = (val<<7);
		r1 = (val^c);
		r2 = (b^r1);
		r3 = ~(a^r2);		//eqv(a,r2)
		r1 = (val|(r3>>31));
		card->cipher = r1;
	}

	d = d^card->cipher;
	val = exnor(card->cipher,32);
	{
		u32 a,b,c,r1,r2,r3;
		a = (val<<23);
		b = (val<<15);
		c = (val<<7);
		r1 = (val^c);
		r2 = (b^r1);
		r3 = ~(a^r2);		//eqv(a,r2)
		r1 = (val|(r3>>31));
		card->cipher = r1;
	}

	e = e^card->cipher;
	val = exnor(card->cipher,(len<<3));
	{
		u32 a,b,c,r1,r2,r3;
		a = (val<<23);
		b = (val<<15);
		c = (val<<7);
		r1 = (val^c);
		r2 = (b^r1);
		r3 = ~(a^r2);		//eqv(a,r2)
		r1 = (val|(r3>>31));
		card->cipher = r1;
	}

	val = exnor(card->cipher,33);
	{
		u32 a,b,c,r1,r2,r3;
		a = (val<<23);
		b = (val<<15);
		c = (val<<7);
		r1 = (val^c);
		r2 = (b^r1);
		r3 = ~(a^r2);		//eqv(a,r2)
		r1 = (val|(r3>>31));
		card->cipher = r1;
	}

	cipher1[0] = d;
	cipher1[1] = e;
	workarea[0] = (u32)cipher1;
	workarea[1] = 8;
#ifdef HW_RVL
	workarea[2] = 0x10000000; // use MEM2 base
#else
	workarea[2] = 0; // use ARAM base
#endif
	workarea[3] = (u32)cipher2;
	DCFlushRange(cipher1,8);
	DCInvalidateRange(cipher2,4);
	DCFlushRange(workarea,16);

	card->dsp_task.prio = 255;
	card->dsp_task.iram_maddr = (u16*)MEM_VIRTUAL_TO_PHYSICAL(_cardunlockdata);
	card->dsp_task.iram_len = 352;
	card->dsp_task.iram_addr = 0x0000;
	card->dsp_task.init_vec = 16;
	card->dsp_task.res_cb = NULL;
	card->dsp_task.req_cb = NULL;
	card->dsp_task.init_cb = __dsp_initcallback;
	card->dsp_task.done_cb = __dsp_donecallback;
	DSP_AddTask(&card->dsp_task);

	key[0] = a;
	key[1] = b;
	key[2] = c;

	return CARD_ERROR_READY;
}

s32 CARD_Init(const char *gamecode,const char *company)
{
	u32 i,level;

	if(card_inited) return CARD_ERROR_READY;
	if(gamecode && strlen(gamecode)<=4) memcpy(card_gamecode,gamecode,4);
	if(company && strlen(company)<=2) memcpy(card_company,company,2);

	_CPU_ISR_Disable(level);
	DSP_Init();

	memset(cardmap,0,sizeof(card_block)*2);
	for(i=0;i<2;i++) {
		cardmap[i].result = CARD_ERROR_NOCARD;
		LWP_InitQueue(&cardmap[i].wait_sync_queue);
		SYS_CreateAlarm(&cardmap[i].timeout_svc);
	}
	SYS_RegisterResetFunc(&card_resetinfo);
	card_inited = 1;
	_CPU_ISR_Restore(level);
	return CARD_ERROR_READY;
}

s32 CARD_Probe(s32 chn)
{
	return EXI_Probe(chn);
}

s32 CARD_ProbeEx(s32 chn,s32 *mem_size,s32 *sect_size)
{
	s32 ret;
	u32 level,card_id;
	card_block *card = NULL;
	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return CARD_ERROR_FATAL_ERROR;
	card = &cardmap[chn];

	_CPU_ISR_Disable(level);
	ret = EXI_ProbeEx(chn);
	if(ret<=0) {
		if(!ret) ret = CARD_ERROR_BUSY;
		else ret = CARD_ERROR_NOCARD;
		_CPU_ISR_Restore(level);
		return ret;
	}

	if(card->attached) {
		if(card->mount_step<1) {
			_CPU_ISR_Restore(level);
			return CARD_ERROR_BUSY;
		}
		if(mem_size) *mem_size = card->card_size;
		if(sect_size) *sect_size = card->sector_size;

		_CPU_ISR_Restore(level);
		return CARD_ERROR_READY;
	}

	if(EXI_GetState(chn)&EXI_FLAG_ATTACH) ret = CARD_ERROR_WRONGDEVICE;
	else {
		ret = CARD_ERROR_BUSY;
		if(EXI_GetID(chn,EXI_DEVICE_0,&card_id)) {
			if(!__card_iscard(card_id)) ret = CARD_ERROR_WRONGDEVICE;
			else {
				if(mem_size) *mem_size = card_id&0xFC;
				if(sect_size) {
					u32 idx = _ROTL(card_id,23)&0x1c;
					*sect_size = card_sector_size[idx>>2];
				}
				ret = CARD_ERROR_READY;
			}
		}
	}

	_CPU_ISR_Restore(level);
	return ret;
}

s32 CARD_MountAsync(s32 chn,void *workarea,cardcallback detach_cb,cardcallback attach_cb)
{
	s32 ret = CARD_ERROR_READY;
	u32 level;
	cardcallback attachcb = NULL;
	card_block *card = NULL;
	if(!workarea) return CARD_ERROR_NOCARD;
	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return CARD_ERROR_FATAL_ERROR;
	card = &cardmap[chn];

	_CPU_ISR_Disable(level);
	if(card->result==CARD_ERROR_BUSY) {
		_CPU_ISR_Restore(level);
		return CARD_ERROR_BUSY;
	}
	if(card->attached || !(EXI_GetState(chn)&EXI_FLAG_ATTACH)) {
		card->result = CARD_ERROR_BUSY;
		card->workarea = workarea;
		card->card_ext_cb = detach_cb;

		attachcb = attach_cb;
		if(!attachcb) attachcb = __card_defaultapicallback;
		card->card_api_cb = attachcb;
		card->card_exi_cb = NULL;

		if(!card->attached) {
			if(EXI_Attach(chn,__card_exthandler)==0) {
				card->result = CARD_ERROR_NOCARD;
				_CPU_ISR_Restore(level);
				return CARD_ERROR_NOCARD;
			}
		}
		card->mount_step = 0;
		card->attached = 1;
		EXI_RegisterEXICallback(chn,NULL);
		SYS_CancelAlarm(card->timeout_svc);
		card->curr_dir = NULL;
		card->curr_fat = NULL;
		_CPU_ISR_Restore(level);

		card->card_unlock_cb = __card_mountcallback;
		if(EXI_Lock(chn,EXI_DEVICE_0,__card_unlockedhandler)==0) return 0;

		card->card_unlock_cb = NULL;
		__card_domount(chn);
		return 1;
	}

	ret = CARD_ERROR_WRONGDEVICE;
	_CPU_ISR_Restore(level);
	return ret;
}

s32 CARD_Mount(s32 chn,void *workarea,cardcallback detach_cb)
{
	s32 ret;
	if((ret=CARD_MountAsync(chn,workarea,detach_cb,__card_synccallback))>=0) {
		ret = __card_sync(chn);
	}
	return ret;
}

s32 CARD_Unmount(s32 chn)
{
	s32 ret;
	card_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return CARD_ERROR_NOCARD;

	if((ret=__card_getcntrlblock(chn,&card))<0) ret = CARD_ERROR_NOCARD;

	__card_dounmount(chn,ret);
	return CARD_ERROR_READY;
}

s32 CARD_ReadAsync(card_file *file,void *buffer,u32 len,u32 offset,cardcallback callback)
{
	s32 ret;
	cardcallback cb = NULL;
	card_block *card = NULL;

	if(len<=0 || (len&0x1ff) || (offset>0 && (offset&0x1ff))) return CARD_ERROR_FATAL_ERROR;
	if((ret=__card_seek(file,len,offset,&card))<0) return ret;

	DCInvalidateRange(buffer,len);

	cb = callback;
	if(!cb) cb = __card_defaultapicallback;
	card->card_api_cb = cb;

	if(len>=(card->sector_size-(file->offset&(card->sector_size-1)))) len = (card->sector_size-(file->offset&(card->sector_size-1)));

	if((ret=__card_read(file->chn,(file->iblock*card->sector_size),len,buffer,__read_callback))<0) {
		__card_putcntrlblock(card,ret);
		return ret;
	}
	return 0;
}

s32 CARD_Read(card_file *file,void *buffer,u32 len,u32 offset)
{
	s32 ret;

	if((ret=CARD_ReadAsync(file,buffer,len,offset,__card_synccallback))>=0) {
		ret = __card_sync(file->chn);
	}
	return ret;
}

s32 CARD_WriteAsync(card_file *file,void *buffer,u32 len,u32 offset,cardcallback callback)
{
	s32 ret;
	cardcallback cb = NULL;
	card_block *card = NULL;

	if((ret=__card_seek(file,len,offset,&card))<0) return ret;
	if(len<0 || (len&(card->sector_size-1)) || (offset>0 && offset&(card->sector_size-1))) {
		__card_putcntrlblock(card,CARD_ERROR_FATAL_ERROR);
		return CARD_ERROR_FATAL_ERROR;
	}

	DCStoreRange(buffer,len);
	cb = callback;
	if(!cb) cb = __card_defaultapicallback;
	card->card_api_cb = cb;

	card->cmd_usr_buf = buffer;
	if((ret=__card_sectorerase(file->chn,(file->iblock*card->sector_size),__erase_callback))>=0) return ret;
	__card_putcntrlblock(card,ret);
	return ret;
}

s32 CARD_Write(card_file *file,void *buffer,u32 len,u32 offset)
{
	s32 ret;

	if((ret=CARD_WriteAsync(file,buffer,len,offset,__card_synccallback))>=0) {
		ret = __card_sync(file->chn);
	}
	return ret;
}

s32 CARD_CreateAsync(s32 chn,const char *filename,u32 size,card_file *file,cardcallback callback)
{
	u32 i,len;
	s32 ret,filenum;
	cardcallback cb = NULL;
	card_block *card = NULL;
	struct card_bat *fatblock = NULL;
	struct card_dat *dirblock = NULL;
	struct card_direntry *entry = NULL;
	len = strlen(filename);
	if(len>CARD_FILENAMELEN) return CARD_ERROR_NAMETOOLONG;

	if((ret=__card_getcntrlblock(chn,&card))<0) return ret;
	if(size<=0 || size%card->sector_size) return CARD_ERROR_FATAL_ERROR;

	dirblock = __card_getdirblock(card);

	filenum = -1;
	entry = dirblock->entries;
	for(i=0;i<CARD_MAXFILES;i++) {
		if(entry[i].gamecode[0]==0xff) {
			if(filenum==-1) filenum = i;
		} else if(memcmp(entry[i].filename,filename,len)==0) {
			if((card_gamecode[0]==0xff || card_company[0]==0xff)
				|| ((card_gamecode[0]!=0xff && memcmp(entry[i].gamecode,card_gamecode,4)==0)
				&& (card_company[0]!=0xff && memcmp(entry[i].company,card_company,2)==0))) {
				__card_putcntrlblock(card,CARD_ERROR_EXIST);
				return CARD_ERROR_EXIST;
			}
		}
	}
	if(filenum==-1) {
		__card_putcntrlblock(card,CARD_ERROR_NOENT);
		return CARD_ERROR_NOENT;
	}

	fatblock = __card_getbatblock(card);
	if((fatblock->freeblocks*card->sector_size)<size) {
		__card_putcntrlblock(card,CARD_ERROR_INSSPACE);
		return CARD_ERROR_INSSPACE;
	}

	cb = callback;
	if(!cb) cb = __card_defaultapicallback;
	card->card_api_cb = cb;

	entry[filenum].length = size/card->sector_size;
	memset(entry[filenum].filename,0,CARD_FILENAMELEN);
	memcpy(entry[filenum].filename,filename,len+1);

	card->curr_file = file;
	file->chn = chn;
	file->filenum = filenum;
	if((ret=__card_allocblock(chn,(size/card->sector_size),__card_createfatcallback))<0) {
		__card_putcntrlblock(card,ret);
		return ret;
	}

	return 0;
}

s32 CARD_Create(s32 chn,const char *filename,u32 size,card_file *file)
{
	s32 ret;

	if((ret=CARD_CreateAsync(chn,filename,size,file,__card_synccallback))>=0) {
		ret = __card_sync(chn);
	}
	return ret;
}

s32 CARD_CreateEntryAsync(s32 chn,card_dir *direntry,card_file *file,cardcallback callback)
{
	u32 i,len;
	s32 ret,filenum;
	cardcallback cb = NULL;
	card_block *card = NULL;
	struct card_bat *fatblock = NULL;
	struct card_dat *dirblock = NULL;
	struct card_direntry *entry = NULL;
	len = strlen((const char*)direntry->filename);
	if(len>CARD_FILENAMELEN) return CARD_ERROR_NAMETOOLONG;

	if((ret=__card_getcntrlblock(chn,&card))<0) return ret;
	if(direntry->filelen<=0 || direntry->filelen%card->sector_size) return CARD_ERROR_FATAL_ERROR;

	dirblock = __card_getdirblock(card);

	filenum = -1;
	entry = dirblock->entries;
	for(i=0;i<CARD_MAXFILES;i++) {
		if(entry[i].gamecode[0]==0xff) {
			if(filenum==-1) filenum = i;
		} else if(memcmp(entry[i].filename,direntry->filename,len)==0) {
			if((entry->gamecode[0]==0xff || entry->company[0]==0xff)
				|| ((entry->gamecode[0]!=0xff && memcmp(entry[i].gamecode,entry->gamecode,4)==0)
				&& (entry->company[0]!=0xff && memcmp(entry[i].company,entry->company,2)==0))) {
				__card_putcntrlblock(card,CARD_ERROR_EXIST);
				return CARD_ERROR_EXIST;
			}
		}
	}
	if(filenum==-1) {
		__card_putcntrlblock(card,CARD_ERROR_NOENT);
		return CARD_ERROR_NOENT;
	}

	fatblock = __card_getbatblock(card);
	if((fatblock->freeblocks*card->sector_size)<direntry->filelen) {
		__card_putcntrlblock(card,CARD_ERROR_INSSPACE);
		return CARD_ERROR_INSSPACE;
	}

	cb = callback;
	if(!cb) cb = __card_defaultapicallback;
	card->card_api_cb = cb;

	entry[filenum].length = direntry->filelen/card->sector_size;
	memset(entry[filenum].filename,0,CARD_FILENAMELEN);
	memcpy(entry[filenum].filename,direntry->filename,len+1);

	card->curr_file = file;
	file->chn = chn;
	file->filenum = filenum;
	if((ret=__card_allocblock(chn,(direntry->filelen/card->sector_size),__card_createfatcallback))<0) {
		__card_putcntrlblock(card,ret);
		return ret;
	}

	return 0;
}

s32 CARD_CreateEntry(s32 chn,card_dir *direntry,card_file *file)
{
	s32 ret;

	if((ret=CARD_CreateEntryAsync(chn,direntry,file,__card_synccallback))>=0) {
		ret = __card_sync(chn);
	}
	return ret;
}

s32 CARD_Open(s32 chn,const char *filename,card_file *file)
{
	s32 ret,fileno;
	struct card_dat *dirblock = NULL;
	card_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return CARD_ERROR_NOCARD;

	file->filenum = -1;
	if((ret=__card_getcntrlblock(chn,&card))<0) return ret;
	if((ret=__card_getfilenum(card,filename,(const char*)card_gamecode,(const char*)card_company,&fileno))<0) {
		__card_putcntrlblock(card,ret);
		return ret;
	}
	dirblock = __card_getdirblock(card);
	if(dirblock->entries[fileno].block<5 || dirblock->entries[fileno].block>=card->blocks) {
		__card_putcntrlblock(card,CARD_ERROR_BROKEN);
		return CARD_ERROR_BROKEN;
	}
	file->chn = chn;
	file->filenum = fileno;
	file->offset = 0;
	file->len = dirblock->entries[fileno].length*card->sector_size;
	file->iblock = dirblock->entries[fileno].block;

	__card_putcntrlblock(card,CARD_ERROR_READY);
	return CARD_ERROR_READY;
}

s32 CARD_OpenEntry(s32 chn,card_dir *entry,card_file *file)
{
	s32 ret,fileno;
	struct card_dat *dirblock = NULL;
	card_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return CARD_ERROR_NOCARD;

	file->filenum = -1;
	if((ret=__card_getcntrlblock(chn,&card))<0) return ret;
	if((ret=__card_getfilenum(card,(const char*)entry->filename,(const char*)entry->gamecode,(const char*)entry->company,&fileno))<0) {
		__card_putcntrlblock(card,ret);
		return ret;
	}

	dirblock = __card_getdirblock(card);
	if(dirblock->entries[fileno].block<5 || dirblock->entries[fileno].block>=card->blocks) {
		__card_putcntrlblock(card,CARD_ERROR_BROKEN);
		return CARD_ERROR_BROKEN;
	}

	file->chn = chn;
	file->filenum = entry->fileno;
	file->offset = 0;
	file->len = dirblock->entries[fileno].length*card->sector_size;
	file->iblock = dirblock->entries[fileno].block;

	__card_putcntrlblock(card,CARD_ERROR_READY);
	return CARD_ERROR_READY;
}

s32 CARD_Close(card_file *file)
{
	s32 ret;
	card_block *card = NULL;

	if(file->chn<EXI_CHANNEL_0 || file->chn>=EXI_CHANNEL_2)	return CARD_ERROR_NOCARD;
	if(file->filenum<0 || file->filenum>=CARD_MAXFILES) return CARD_ERROR_NOFILE;
	if((ret=__card_getcntrlblock(file->chn,&card))<0) return ret;

	file->chn = -1;
	__card_putcntrlblock(card,CARD_ERROR_READY);
	return CARD_ERROR_READY;
}

s32 CARD_DeleteAsync(s32 chn,const char *filename,cardcallback callback)
{
	s32 ret,fileno;
	cardcallback cb = NULL;
	card_block *card = NULL;
	struct card_dat *dirblock = NULL;
	struct card_direntry *entry = NULL;
	if((ret=__card_getcntrlblock(chn,&card))<0) return ret;
	if((ret=__card_getfilenum(card,filename,(const char*)card_gamecode,(const char*)card_company,&fileno))<0) {
		__card_putcntrlblock(card,ret);
		return ret;
	}

	dirblock = __card_getdirblock(card);
	entry = &dirblock->entries[fileno];

	card->curr_fileblock = entry->block;
	memset(entry,-1,sizeof(struct card_direntry));

	cb = callback;
	if(!cb) cb = __card_defaultapicallback;
	card->card_api_cb = cb;

	if((ret=__card_updatedir(chn,__delete_callback))>=0) return ret;

	__card_putcntrlblock(card,ret);
	return ret;
}

s32 CARD_Delete(s32 chn,const char *filename)
{
	s32 ret;
	if((ret=CARD_DeleteAsync(chn,filename,__card_synccallback))>=0) {
		ret = __card_sync(chn);
	}
	return ret;
}

s32 CARD_DeleteEntryAsync(s32 chn,card_dir *dir_entry,cardcallback callback)
{
	s32 ret;
	cardcallback cb = NULL;
	card_block *card = NULL;
	struct card_dat *dirblock = NULL;
	struct card_direntry *entry = NULL;
	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return CARD_ERROR_NOCARD;
	if((ret=__card_getcntrlblock(chn,&card))<0) return ret;

	dirblock = __card_getdirblock(card);
	entry = &dirblock->entries[dir_entry->fileno];

	card->curr_fileblock = entry->block;
	memset(entry,-1,sizeof(struct card_direntry));

	cb = callback;
	if(!cb) cb = __card_defaultapicallback;
	card->card_api_cb = cb;

	if((ret=__card_updatedir(chn,__delete_callback))>=0) return ret;

	__card_putcntrlblock(card,ret);
	return ret;
}

s32 CARD_DeleteEntry(s32 chn,card_dir *dir_entry)
{
	s32 ret;
	if((ret=CARD_DeleteEntryAsync(chn,dir_entry,__card_synccallback))>=0) {
		ret = __card_sync(chn);
	}
	return ret;
}

s32 CARD_FormatAsync(s32 chn,cardcallback callback)
{
	u32 enc;

	enc = SYS_GetFontEncoding();
	return __card_formatregion(chn,enc,callback);
}

s32 CARD_Format(s32 chn)
{
	s32 ret;
	u32 enc;

	enc = SYS_GetFontEncoding();
	if((ret=__card_formatregion(chn,enc,__card_synccallback))>=0) {
		ret = __card_sync(chn);
	}
	return ret;
}

s32 CARD_GetErrorCode(s32 chn)
{
	card_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return CARD_ERROR_NOCARD;
	card = &cardmap[chn];
	return card->result;
}

s32 __card_findnext(card_dir *dir)
{
	s32 ret;
	struct card_dat *dirblock = NULL;
	struct card_direntry *entries = NULL;
	card_block *card = NULL;

	if(dir->chn<EXI_CHANNEL_0 || dir->chn>=EXI_CHANNEL_2) return CARD_ERROR_NOCARD;
	if(dir->fileno>=CARD_MAXFILES) return CARD_ERROR_NOFILE;
	if((ret=__card_getcntrlblock(dir->chn,&card))<0) return ret;

	if(!card->attached) return CARD_ERROR_NOCARD;
	dirblock = __card_getdirblock(card);

	entries = dirblock->entries;
	do {
		//printf("%s\n", entries[dir->fileno].filename);
		if(entries[dir->fileno].gamecode[0]!=0xff) {
			if ((dir->showall || memcmp(entries[dir->fileno].gamecode,card_gamecode,4)==0)
				&& (dir->showall || memcmp(entries[dir->fileno].company,card_company,2)==0)) {
				dir->filelen = entries[dir->fileno].length*card->sector_size;
				memcpy(dir->filename, entries[dir->fileno].filename, CARD_FILENAMELEN);
				memcpy(dir->gamecode, entries[dir->fileno].gamecode, 4);
				memcpy(dir->company, entries[dir->fileno].company, 2);

				__card_putcntrlblock(card,CARD_ERROR_READY);
				return CARD_ERROR_READY;
			}
		}
		dir->fileno++;
	} while (dir->fileno < CARD_MAXFILES);
	__card_putcntrlblock(card,CARD_ERROR_NOFILE);
	return CARD_ERROR_NOFILE;
}

s32 CARD_FindFirst(s32 chn, card_dir *dir, bool showall)
{
	// initialise structure
	dir->chn = chn;
	dir->fileno = 0;
	dir->filelen = 0;
	dir->filename[0] = 0;
	dir->gamecode[0] = 0;
	dir->company[0] = 0;
	dir->showall = showall;
	return __card_findnext(dir);
}

s32 CARD_FindNext(card_dir *dir)
{
      dir->fileno++;

      return __card_findnext(dir);
}

s32 CARD_GetDirectory(s32 chn,card_dir *dir_entries,s32 *count,bool showall)
{
	s32 i,cnt;
	s32 ret = CARD_ERROR_READY;
	struct card_dat *dirblock = NULL;
	struct card_direntry *entries = NULL;
	card_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return CARD_ERROR_NOCARD;
	if((ret=__card_getcntrlblock(chn,&card))<0) return ret;

	if(!card->attached) return CARD_ERROR_NOCARD;
	dirblock = __card_getdirblock(card);

	entries = dirblock->entries;
	for(i=0,cnt=0;i<CARD_MAXFILES;i++) {
		if(entries[i].gamecode[0]!=0xff) {
			if(showall || ((card_gamecode[0]!=0xff && memcmp(entries[i].gamecode,card_gamecode,4)==0)
				&& (card_company[0]!=0xff && memcmp(entries[i].company,card_company,2)==0))) {
				dir_entries[cnt].fileno = i;
				dir_entries[cnt].permissions = entries[i].permission;
				dir_entries[cnt].filelen = entries[i].length*card->sector_size;
				memcpy(dir_entries[cnt].gamecode,entries[i].gamecode,4);
				memcpy(dir_entries[cnt].company,entries[i].company,2);
				memcpy(dir_entries[cnt].filename,entries[i].filename,CARD_FILENAMELEN);
				cnt++;
			}
		}
	}
	if(count) *count = cnt;
	if(cnt==0) ret = CARD_ERROR_NOFILE;
	__card_putcntrlblock(card,ret);
	return ret;
}

s32 CARD_GetSectorSize(s32 chn,u32 *sector_size)
{
	s32 ret;
	card_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return CARD_ERROR_NOCARD;
	if((ret=__card_getcntrlblock(chn,&card))<0) return ret;

	*sector_size = card->sector_size;
	ret = __card_putcntrlblock(card,CARD_ERROR_READY);

	return ret;
}

s32 CARD_GetBlockCount(s32 chn,u32 *block_count)
{
	s32 ret;
	card_block *card = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return CARD_ERROR_NOCARD;
	if((ret=__card_getcntrlblock(chn,&card))<0) return ret;

	*block_count = card->blocks;
	ret = __card_putcntrlblock(card,CARD_ERROR_READY);

	return ret;
}

s32 CARD_GetStatus(s32 chn,s32 fileno,card_stat *stats)
{
	s32 ret;
	card_block *card = NULL;
	struct card_dat *dirblock = NULL;
	struct card_direntry *entry = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return CARD_ERROR_NOCARD;
	if(fileno<0 || fileno>=CARD_MAXFILES) return CARD_ERROR_FATAL_ERROR;

	if((ret=__card_getcntrlblock(chn,&card))<0) return ret;

	dirblock = __card_getdirblock(card);
	if(dirblock) {
		entry = &dirblock->entries[fileno];
		memcpy(stats->gamecode,entry->gamecode,4);
		memcpy(stats->company,entry->company,2);
		memcpy(stats->filename,entry->filename,CARD_FILENAMELEN);
		stats->len = entry->length*card->sector_size;
		stats->time = entry->lastmodified;
		stats->banner_fmt = entry->bannerfmt;
		stats->icon_addr = entry->iconaddr;
		stats->icon_fmt = entry->iconfmt;
		stats->icon_speed = entry->iconspeed;
		stats->comment_addr = entry->commentaddr;
		__card_updateiconoffsets(entry,stats);
	}

	return __card_putcntrlblock(card,CARD_ERROR_READY);
}

s32 CARD_SetStatusAsync(s32 chn,s32 fileno,card_stat *stats,cardcallback callback)
{
	s32 ret;
	card_block *card = NULL;
	struct card_dat *dirblock = NULL;
	struct card_direntry *entry = NULL;

	if(chn<EXI_CHANNEL_0 || chn>=EXI_CHANNEL_2) return CARD_ERROR_NOCARD;
	if(fileno<0 || fileno>=CARD_MAXFILES) return CARD_ERROR_FATAL_ERROR;
	if(stats->icon_addr!=-1 && stats->icon_addr>CARD_READSIZE) return CARD_ERROR_FATAL_ERROR;
	if(stats->comment_addr!=-1 && stats->comment_addr>8128) return CARD_ERROR_FATAL_ERROR;
	if((ret=__card_getcntrlblock(chn,&card))<0) return ret;

	ret = CARD_ERROR_BROKEN;
	dirblock = __card_getdirblock(card);
	if(dirblock) {
		entry = &dirblock->entries[fileno];
		entry->bannerfmt = stats->banner_fmt;
		entry->iconaddr = stats->icon_addr;
		entry->iconfmt = stats->icon_fmt;
		entry->iconspeed = stats->icon_speed;
		entry->commentaddr = stats->comment_addr;
		__card_updateiconoffsets(entry,stats);

		if(entry->iconaddr==-1) entry->iconfmt = ((entry->iconfmt&~CARD_ICON_MASK)|CARD_ICON_CI);

		entry->lastmodified = time(NULL);
		if((ret=__card_updatedir(chn,callback))>=0) return ret;
	}

	return __card_putcntrlblock(card,ret);
}

s32 CARD_SetStatus(s32 chn,s32 fileno,card_stat *stats)
{
	s32 ret;

	if((ret=CARD_SetStatusAsync(chn,fileno,stats,__card_synccallback))>=0) {
		ret = __card_sync(chn);
	}
	return ret;
}

s32 CARD_GetAttributes(s32 chn,s32 fileno,u8 *attr)
{
	s32 ret;
	struct card_direntry entry;

	if((ret=__card_getstatusex(chn,fileno,&entry))==CARD_ERROR_READY) {
		*attr = entry.permission;
	}
	return ret;
}

s32 CARD_SetAttributesAsync(s32 chn,s32 fileno,u8 attr,cardcallback callback)
{
	s32 ret;
	struct card_direntry entry;

	if((ret=__card_getstatusex(chn,fileno,&entry))>=0) {
		entry.permission = attr;
		ret = __card_setstatusexasync(chn,fileno,&entry,callback);
	}
	return ret;
}

s32 CARD_SetAttributes(s32 chn,s32 fileno,u8 attr)
{
	s32 ret;

	if((ret=CARD_SetAttributesAsync(chn,fileno,attr,__card_synccallback))>=0) {
		ret = __card_sync(chn);
	}
	return ret;
}

s32 CARD_SetCompany(const char *company)
{
	u32 level,i;

	_CPU_ISR_Disable(level);
	for(i=0;i<2;i++) card_company[i] = 0xff;
	if(company && strlen(company)<=2) memcpy(card_company,company,2) ;
	_CPU_ISR_Restore(level);

	return CARD_ERROR_READY;
}

s32 CARD_SetGamecode(const char *gamecode)
{
	u32 level,i;

	_CPU_ISR_Disable(level);
	for(i=0;i<4;i++) card_gamecode[i] = 0xff;
	if(gamecode && strlen(gamecode)<=4) memcpy(card_gamecode,gamecode,4) ;
	_CPU_ISR_Restore(level);

	return CARD_ERROR_READY;
}
