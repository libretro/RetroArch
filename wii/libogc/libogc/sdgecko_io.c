#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ogcsys.h>

#include "asm.h"
#include "processor.h"
#include "exi.h"
#include "lwp.h"
#include "system.h"
#include "semaphore.h"
#include "card_cmn.h"
//#include "card_fat.h"
#include "card_io.h"

// SDHC support
// added by emu_kidid
#define SECTOR_ADDRESSING					0
#define BYTE_ADDRESSING						1
#define TYPE_SD								0
#define TYPE_SDHC							1

#define MMC_ERROR_PARAM						0x0040
#define MMC_ERROR_ADDRESS					0x0020
#define MMC_ERROR_ERASE_SEQ					0x0010
#define MMC_ERROR_CRC						0x0008
#define MMC_ERROR_ILL						0x0004
#define MMC_ERROR_ERASE_RES					0x0002
#define MMC_ERROR_IDLE						0x0001

#define CARDIO_OP_INITFAILED				0x8000
#define CARDIO_OP_TIMEDOUT					0x4000
#define CARDIO_OP_IOERR_IDLE				0x2000
#define CARDIO_OP_IOERR_PARAM				0x1000
#define CARDIO_OP_IOERR_WRITE				0x0200
#define CARDIO_OP_IOERR_ADDR				0x0100
#define CARDIO_OP_IOERR_CRC					0x0002
#define CARDIO_OP_IOERR_ILL					0x0001
#define CARDIO_OP_IOERR_FATAL				(CARDIO_OP_IOERR_PARAM|CARDIO_OP_IOERR_WRITE|CARDIO_OP_IOERR_ADDR|CARDIO_OP_IOERR_CRC|CARDIO_OP_IOERR_ILL)

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

typedef s32 (*cardiocallback)(s32 drv_no);

u8 g_CID[MAX_DRIVE][16];
u8 g_CSD[MAX_DRIVE][16];
u8 g_CardStatus[MAX_DRIVE][64];

u8 g_mCode[MAX_MI_NUM] = { 0x03 };

u16 g_dCode[MAX_MI_NUM][MAX_DI_NUM] =
{
	{
		0x033f,			/* SD   8Mb */
		0x0383,			/* SD  16Mb */
		0x074b,			/* SD  32Mb */
		0x0edf,			/* SD  64Mb */
		0x0f03			/* SD 128Mb */
	}
};

static u8 _ioWPFlag;
static u8 _ioClrFlag;
static u32 _ioCardFreq;
static u32 _ioRetryCnt;
static cardiocallback _ioRetryCB = NULL;

static lwpq_t _ioEXILock[MAX_DRIVE];

static u32 _ioPageSize[MAX_DRIVE];
static u32 _ioFlag[MAX_DRIVE];
static u32 _ioError[MAX_DRIVE];
static bool _ioCardInserted[MAX_DRIVE];

static u8 _ioResponse[MAX_DRIVE][128];
static u8 _ioCrc7Table[256];
static u16 _ioCrc16Table[256];

// SDHC support
static u32 _initType[MAX_DRIVE];
static u32 _ioAddressingType[MAX_DRIVE];

extern unsigned long gettick();

static __inline__ u32 __check_response(s32 drv_no,u8 res)
{
	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	_ioError[drv_no] = 0;
	if(_ioFlag[drv_no]==INITIALIZING && res&MMC_ERROR_IDLE) {
		_ioError[drv_no] |= CARDIO_OP_IOERR_IDLE;
		return CARDIO_ERROR_READY;
	} else {
		if(res&MMC_ERROR_PARAM) _ioError[drv_no] |= CARDIO_OP_IOERR_PARAM;
		if(res&MMC_ERROR_ADDRESS) _ioError[drv_no] |= CARDIO_OP_IOERR_ADDR;
		if(res&MMC_ERROR_CRC) _ioError[drv_no] |= CARDIO_OP_IOERR_CRC;
		if(res&MMC_ERROR_ILL) _ioError[drv_no] |= CARDIO_OP_IOERR_ILL;
	}
	return ((_ioError[drv_no]&CARDIO_OP_IOERR_FATAL)?CARDIO_ERROR_INTERNAL:CARDIO_ERROR_READY);
}

static void __init_crc7()
{
	s32 i,j;
	u8 c,crc7;

	crc7 = 0;
	for(i=0;i<256;i++) {
		c = i;
		crc7 = 0;
		for(j=0;j<8;j++) {
			crc7 <<= 1;
			if((crc7^c)&0x80) crc7 ^= 0x09;
			c <<= 1;
		}
		crc7 &= 0x7f;
		_ioCrc7Table[i] = crc7;
	}
}

static u8 __make_crc7(void *buffer,u32 len)
{
	s32 i;
	u8 crc7;
	u8 *ptr;

	crc7 = 0;
	ptr = buffer;
	for(i=0;i<len;i++)
		crc7 = _ioCrc7Table[(u8)((crc7<<1)^ptr[i])];

	return ((crc7<<1)|1);
}

/* Old way, realtime
static u8 __make_crc7(void *buffer,u32 len)
{
	u32 mask,cnt,bcnt;
	u32 res,val;
	u8 *ptr = (u8*)buffer;

	cnt = 0;
	res = 0;
	while(cnt<len) {
		bcnt = 0;
		mask = 0x80;
		while(bcnt<8) {
			res <<= 1;
			val = *ptr^((res>>bcnt)&0xff);
			if(mask&val) {
				res |= 0x01;
				if(!(res&0x0008)) res |= 0x0008;
				else res &= ~0x0008;

			} else if(res&0x0008) res |= 0x0008;
			else res &= ~0x0008;

			mask >>= 1;
			bcnt++;
		}
		ptr++;
		cnt++;
	}
	return (res<<1)&0xff;
}
*/
static void __init_crc16()
{
	s32 i,j;
	u16 crc16,c;

	for(i=0;i<256;i++) {
		crc16 = 0;
		c = ((u16)i)<<8;
		for(j=0;j<8;j++) {
			if((crc16^c)&0x8000) crc16 = (crc16<<1)^0x1021;
			else crc16 <<= 1;

			c <<= 1;
		}

		_ioCrc16Table[i] = crc16;
	}
}

static u16 __make_crc16(void *buffer,u32 len)
{
	s32 i;
	u8 *ptr;
	u16 crc16;

	crc16 = 0;
	ptr = buffer;
	for(i=0;i<len;i++)
		crc16 = (crc16<<8)^_ioCrc16Table[((crc16>>8)^(u16)(ptr[i]))];

	return crc16;
}

/* Old way, realtime
static u16 __make_crc16(void *buffer,u32 len)
{
	u32 mask,cnt,bcnt;
	u32 res,val,tmp;
	u8 *ptr = (u8*)buffer;

	cnt = 0;
	res = 0;
	while(cnt<len) {
		bcnt = 0;
		mask = 0x80;
		val = *ptr;
		while(bcnt<8) {
			tmp = val^((res>>(bcnt+8))&0xff);
			if(mask&tmp) {
				res = (res<<1)|0x0001;
				if(!(res&0x0020)) res |= 0x0020;
				else res &= ~0x0020;
				if(!(res&0x1000)) res |= 0x1000;
				else res &= ~0x1000;
			} else {
				res = (res<<1)&~0x0001;
				if(res&0x0020) res |= 0x0020;
				else res &= ~0x0020;
				if(res&0x1000) res |= 0x1000;
				else res &= ~0x1000;
			}
			mask >>= 1;
			bcnt++;
		}
		ptr++;
		cnt++;
	}

	return (res&0xffff);
}
*/

static u32 __card_checktimeout(s32 drv_no,u32 startT,u32 timeout)
{
	u32 endT,diff;
	u32 msec;

	endT = gettick();
	if(endT<startT) {
		diff = (endT+(-1-startT))+1;
	} else
		diff = (endT-startT);

	msec = (diff/TB_TIMER_CLOCK);
	if(msec<=timeout) return 0;

	_ioError[drv_no] |= CARDIO_OP_TIMEDOUT;
	return 1;
}

static s32 __exi_unlock(s32 chn,s32 dev)
{
	LWP_ThreadBroadcast(_ioEXILock[chn]);
	return 1;
}

static void __exi_wait(s32 drv_no)
{
	u32 ret;

	do {
		if((ret=EXI_Lock(drv_no,EXI_DEVICE_0,__exi_unlock))==1) break;
		LWP_ThreadSleep(_ioEXILock[drv_no]);
	} while(ret==0);
}

static s32 __card_exthandler(s32 chn,s32 dev)
{
	_ioCardInserted[chn] = FALSE;
	sdgecko_doUnmount(chn);
	sdgecko_ejectedCB(chn);
	EXI_Unlock(chn);
	return 1;
}

static s32 __card_writecmd0(s32 drv_no)
{
	u8 crc;
	u32 cnt;
	u8 dummy[128];
	u8 cmd[5] = {0,0,0,0,0};

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	_ioClrFlag = 0xff;
	cmd[0] = 0x40;
	crc = __make_crc7(cmd,5);

	if(_ioWPFlag) {
		_ioClrFlag = 0x00;
		for(cnt=0;cnt<5;cnt++) cmd[cnt] ^= -1;
	}

	for(cnt=0;cnt<128;cnt++) dummy[cnt] = _ioClrFlag;

	__exi_wait(drv_no);

	if(EXI_SelectSD(drv_no,EXI_DEVICE_0,_ioCardFreq)==0) {
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_NOCARD;
	}

	cnt = 0;
	while(cnt<20) {
		if(EXI_ImmEx(drv_no,dummy,128,EXI_WRITE)==0) {
			EXI_Deselect(drv_no);
			EXI_Unlock(drv_no);
			return CARDIO_ERROR_IOERROR;
		}
		cnt++;
	}
	EXI_Deselect(drv_no);

	if(EXI_Select(drv_no,EXI_DEVICE_0,_ioCardFreq)==0) {
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_NOCARD;
	}

	crc |= 0x01;
	if(_ioWPFlag) crc ^= -1;
	if(EXI_ImmEx(drv_no,cmd,5,EXI_WRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}

	if(EXI_ImmEx(drv_no,&crc,1,EXI_WRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}

	EXI_Deselect(drv_no);
	EXI_Unlock(drv_no);
	return CARDIO_ERROR_READY;
}

static s32 __card_writecmd(s32 drv_no,void *buf,s32 len)
{
	u8 crc,*ptr;
	u8 dummy[32];
	u32 cnt;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	ptr = buf;
	ptr[0] |= 0x40;
	crc = __make_crc7(buf,len);

	if(_ioWPFlag) {
		for(cnt=0;cnt<len;cnt++) ptr[cnt] ^= -1;
	}

	__exi_wait(drv_no);

	if(EXI_Select(drv_no,EXI_DEVICE_0,_ioCardFreq)==0) {
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_NOCARD;
	}

	for(cnt=0;cnt<32;cnt++) dummy[cnt] = _ioClrFlag;

	if(EXI_ImmEx(drv_no,dummy,10,EXI_WRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}

	crc |= 0x01;
	if(_ioWPFlag) crc ^= -1;
	if(EXI_ImmEx(drv_no,buf,len,EXI_WRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}
	if(EXI_ImmEx(drv_no,&crc,1,EXI_WRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}

	EXI_Deselect(drv_no);
	EXI_Unlock(drv_no);
	return CARDIO_ERROR_READY;
}

static s32 __card_readresponse(s32 drv_no,void *buf,s32 len)
{
	u8 *ptr;
	s32 startT,ret;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	ret = CARDIO_ERROR_READY;
	ptr = buf;
	*ptr = _ioClrFlag;

	__exi_wait(drv_no);

	if(EXI_Select(drv_no,EXI_DEVICE_0,_ioCardFreq)==0) {
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_NOCARD;
	}

	if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}

	startT = gettick();
	while(*ptr&0x80) {
		*ptr = _ioClrFlag;
		if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
			EXI_Deselect(drv_no);
			EXI_Unlock(drv_no);
			return CARDIO_ERROR_IOERROR;
		}
		if(!(*ptr&0x80)) break;
		if(__card_checktimeout(drv_no,startT,500)!=0) {
			*ptr = _ioClrFlag;
			if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
				EXI_Deselect(drv_no);
				EXI_Unlock(drv_no);
				return CARDIO_ERROR_IOERROR;
			}
			if(*ptr&0x80) ret = CARDIO_ERROR_IOTIMEOUT;
			break;
		}
	}
	if(len>1 && ret==CARDIO_ERROR_READY) {
		*(++ptr) = _ioClrFlag;
		if(EXI_ImmEx(drv_no,ptr,len-1,EXI_READWRITE)==0) ret = CARDIO_ERROR_IOERROR;
	}

	EXI_Deselect(drv_no);
	EXI_Unlock(drv_no);
	return ret;
}

static s32 __card_stopreadresponse(s32 drv_no,void *buf,s32 len)
{
	u8 *ptr,tmp;
	s32 startT,ret;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	ptr = buf;

	__exi_wait(drv_no);

	if(EXI_Select(drv_no,EXI_DEVICE_0,_ioCardFreq)==0) {
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_NOCARD;
	}

	ret = CARDIO_ERROR_READY;
	*ptr = _ioClrFlag;
	if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}

	*ptr = _ioClrFlag;
	if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}

	startT = gettick();
	while(*ptr&0x80) {
		*ptr = _ioClrFlag;
		if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
			EXI_Deselect(drv_no);
			EXI_Unlock(drv_no);
			return CARDIO_ERROR_IOERROR;
		}
		if(!(*ptr&0x80)) break;
		if(__card_checktimeout(drv_no,startT,1500)!=0) {
			*ptr = _ioClrFlag;
			if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
				EXI_Deselect(drv_no);
				EXI_Unlock(drv_no);
				return CARDIO_ERROR_IOERROR;
			}
			if(*ptr&0x80) ret = CARDIO_ERROR_IOTIMEOUT;
			break;
		}
	}

	tmp = *ptr;
	while(*ptr!=0xff) {
		*ptr = _ioClrFlag;
		if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
			EXI_Deselect(drv_no);
			EXI_Unlock(drv_no);
			return CARDIO_ERROR_IOERROR;
		}
		if(*ptr==0xff) break;
		if(__card_checktimeout(drv_no,startT,1500)!=0) {
			*ptr = _ioClrFlag;
			if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
				EXI_Deselect(drv_no);
				EXI_Unlock(drv_no);
				return CARDIO_ERROR_IOERROR;
			}
			if(*ptr!=0xff) ret = CARDIO_ERROR_IOTIMEOUT;
			break;
		}
	}
	*ptr = tmp;

	if(len>1 && ret==CARDIO_ERROR_READY) {
		*(++ptr) = _ioClrFlag;
		if(EXI_ImmEx(drv_no,ptr,len-1,EXI_READWRITE)==0) ret = CARDIO_ERROR_IOERROR;
	}

	EXI_Deselect(drv_no);
	EXI_Unlock(drv_no);
	return ret;
}

static s32 __card_datares(s32 drv_no,void *buf)
{
	u8 *ptr;
	s32 startT,ret;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	ptr = buf;

	__exi_wait(drv_no);

	if(EXI_Select(drv_no,EXI_DEVICE_0,_ioCardFreq)==0) {
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_NOCARD;
	}

	ret = CARDIO_ERROR_READY;
	*ptr = _ioClrFlag;
	if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}
	startT = gettick();
	while(*ptr&0x10) {
		*ptr = _ioClrFlag;
		if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
			EXI_Deselect(drv_no);
			EXI_Unlock(drv_no);
			return CARDIO_ERROR_IOERROR;
		}
		if(!(*ptr&0x10)) break;
		if(__card_checktimeout(drv_no,startT,1500)!=0) {
			*ptr = _ioClrFlag;
			if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
				EXI_Deselect(drv_no);
				EXI_Unlock(drv_no);
				return CARDIO_ERROR_IOERROR;
			}
			if(*ptr&0x10) ret = CARDIO_ERROR_IOTIMEOUT;
			break;
		}
	}

	*(++ptr) = _ioClrFlag;
	if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}

	startT = gettick();
	while(!*ptr) {
		*ptr = _ioClrFlag;
		if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
			EXI_Deselect(drv_no);
			EXI_Unlock(drv_no);
			return CARDIO_ERROR_IOERROR;
		}
		if(*ptr) break;
		if(__card_checktimeout(drv_no,startT,1500)!=0) {
			*ptr = _ioClrFlag;
			if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
				EXI_Deselect(drv_no);
				EXI_Unlock(drv_no);
				return CARDIO_ERROR_IOERROR;
			}
			if(!*ptr) ret = CARDIO_ERROR_IOTIMEOUT;
			break;
		}
	}
	EXI_Deselect(drv_no);
	EXI_Unlock(drv_no);

	return ret;
}

static s32 __card_stopresponse(s32 drv_no)
{
	s32 ret;

	if((ret=__card_stopreadresponse(drv_no,_ioResponse[drv_no],1))!=0) return ret;
	ret = __check_response(drv_no,_ioResponse[drv_no][0]);

	return ret;
}

static s32 __card_dataresponse(s32 drv_no)
{
	s32 ret;
	u8 res;

	if((ret=__card_datares(drv_no,_ioResponse[drv_no]))!=0) return ret;
	res = _SHIFTR(_ioResponse[drv_no][0],1,3);
	if(res==0x0005) ret = CARDIO_OP_IOERR_CRC;
	else if(res==0x0006) ret = CARDIO_OP_IOERR_WRITE;

	return ret;
}

static s32 __card_dataread(s32 drv_no,void *buf,u32 len)
{
	u8 *ptr;
	u32 cnt;
	u8 res[2];
	u16 crc,crc_org;
	s32 startT,ret;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	__exi_wait(drv_no);

	if(EXI_Select(drv_no,EXI_DEVICE_0,_ioCardFreq)==0) {
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_NOCARD;
	}

	ret = CARDIO_ERROR_READY;
	ptr = buf;
	for(cnt=0;cnt<len;cnt++) ptr[cnt] = _ioClrFlag;
	if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}

	startT = gettick();
	while(*ptr!=0xfe) {
		*ptr = _ioClrFlag;
		if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
			EXI_Deselect(drv_no);
			EXI_Unlock(drv_no);
			return CARDIO_ERROR_IOERROR;
		}
		if(*ptr==0xfe) break;
		if(__card_checktimeout(drv_no,startT,1500)!=0) {
			*ptr = _ioClrFlag;
			if(EXI_ImmEx(drv_no,ptr,1,EXI_READWRITE)==0) {
				EXI_Deselect(drv_no);
				EXI_Unlock(drv_no);
				return CARDIO_ERROR_IOERROR;
			}
			if(*ptr!=0xfe) ret = CARDIO_ERROR_IOTIMEOUT;
			break;
		}
	}

	*ptr = _ioClrFlag;
	if(EXI_ImmEx(drv_no,ptr,len,EXI_READWRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}

	/* sleep 1us*/
	usleep(1);

	res[0] = res[1] = _ioClrFlag;
	if(EXI_ImmEx(drv_no,res,2,EXI_READWRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}
	crc_org = ((res[0]<<8)&0xff00)|(res[1]&0xff);

	EXI_Deselect(drv_no);
	EXI_Unlock(drv_no);

	crc = __make_crc16(buf,len);
	if(crc!=crc_org) ret = CARDIO_OP_IOERR_CRC;
	return ret;
}

static s32 __card_multidatawrite(s32 drv_no,void *buf,u32 len)
{
	u8 dummy[32];
	u16 crc;
	u32 cnt;
	s32 ret;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	for(cnt=0;cnt<32;cnt++) dummy[cnt] = _ioClrFlag;
	crc = __make_crc16(buf,len);

	__exi_wait(drv_no);

	if(EXI_Select(drv_no,EXI_DEVICE_0,_ioCardFreq)==0) {
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_NOCARD;
	}

	dummy[0] = 0xfc;
	if(EXI_ImmEx(drv_no,dummy,1,EXI_WRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}

	if(EXI_ImmEx(drv_no,buf,len,EXI_WRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}

	/* sleep 1us*/
	usleep(1);

	ret = CARDIO_ERROR_READY;
	if(EXI_ImmEx(drv_no,&crc,2,EXI_WRITE)==0) ret = CARDIO_ERROR_IOERROR;

	EXI_Deselect(drv_no);
	EXI_Unlock(drv_no);

	return ret;
}

static s32 __card_multiwritestop(s32 drv_no)
{
	s32 ret,cnt,startT;
	u8 dummy[32];

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	for(cnt=0;cnt<32;cnt++) dummy[cnt] = _ioClrFlag;

	__exi_wait(drv_no);

	if(EXI_Select(drv_no,EXI_DEVICE_0,_ioCardFreq)==0) {
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_NOCARD;
	}

	ret = CARDIO_ERROR_READY;
	dummy[0] = 0xfd;
	if(_ioWPFlag) dummy[0] = 0x02;		//!0xfd
	if(EXI_ImmEx(drv_no,dummy,1,EXI_WRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}

	dummy[0] = _ioClrFlag;
	if(EXI_ImmEx(drv_no,dummy,1,EXI_READWRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}

	dummy[0] = _ioClrFlag;
	if(EXI_ImmEx(drv_no,dummy,1,EXI_READWRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}

	dummy[0] = _ioClrFlag;
	if(EXI_ImmEx(drv_no,dummy,1,EXI_READWRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}

	dummy[0] = _ioClrFlag;
	if(EXI_ImmEx(drv_no,dummy,1,EXI_READWRITE)==0) {
		EXI_Deselect(drv_no);
		EXI_Unlock(drv_no);
		return CARDIO_ERROR_IOERROR;
	}

	startT = gettick();
	ret = CARDIO_ERROR_READY;
	while(dummy[0]==0) {
		dummy[0] = _ioClrFlag;
		if(EXI_ImmEx(drv_no,dummy,1,EXI_READWRITE)==0) {
			EXI_Deselect(drv_no);
			EXI_Unlock(drv_no);
			return CARDIO_ERROR_IOERROR;
		}
		if(dummy[0]) break;
		if(__card_checktimeout(drv_no,startT,1500)!=0) {
			dummy[0] = _ioClrFlag;
			if(EXI_ImmEx(drv_no,dummy,1,EXI_READWRITE)==0) {
				EXI_Deselect(drv_no);
				EXI_Unlock(drv_no);
				return CARDIO_ERROR_IOERROR;
			}
			if(!dummy[0]) ret = CARDIO_ERROR_IOTIMEOUT;
			break;
		}
	}

	EXI_Deselect(drv_no);
	EXI_Unlock(drv_no);
	return ret;
}

static s32 __card_response1(s32 drv_no)
{
	s32 ret;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	if((ret=__card_readresponse(drv_no,_ioResponse[drv_no],1))!=0) return ret;
	return __check_response(drv_no,_ioResponse[drv_no][0]);
}

static s32 __card_response2(s32 drv_no)
{
	u32 ret;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	if((ret=__card_readresponse(drv_no,_ioResponse[drv_no],2))!=0) return ret;
	if(!(_ioResponse[drv_no][0]&0x7c) && !(_ioResponse[drv_no][1]&0x9e)) return CARDIO_ERROR_READY;
	return CARDIO_ERROR_FATALERROR;
}

static s32 __card_sendappcmd(s32 drv_no)
{
	s32 ret;
	u8 ccmd[5] = {0,0,0,0,0};

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	ccmd[0] = 0x37;
	if((ret=__card_writecmd(drv_no,ccmd,5))!=0) {
		return ret;
	}
	if((ret=__card_readresponse(drv_no,_ioResponse[drv_no],1))!=0) return ret;
	ret = __check_response(drv_no,_ioResponse[drv_no][0]);

	return ret;
}

static s32 __card_sendopcond(s32 drv_no)
{
	u8 ccmd[5] = {0,0,0,0,0};
	s32 ret;
	s32 startT;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;
	ret = 0;
	startT = gettick();
	do {
		if(_initType[drv_no]==TYPE_SDHC) {
			__card_sendappcmd(drv_no);
			ccmd[0] = 0x29;
			ccmd[1] = 0x40;
		} else
			ccmd[0] = 0x01;

		if((ret=__card_writecmd(drv_no,ccmd,5))!=0) {
			return ret;
		}
		if((ret=__card_readresponse(drv_no,_ioResponse[drv_no],1))!=0) return ret;
		if((ret=__check_response(drv_no,_ioResponse[drv_no][0]))!=0) return ret;
		if(!(_ioError[drv_no]&CARDIO_OP_IOERR_IDLE)) return CARDIO_ERROR_READY;

		ret = __card_checktimeout(drv_no,startT,1500);
	} while(ret==0);

	if(_initType[drv_no]==TYPE_SDHC) {
		__card_sendappcmd(drv_no);
		ccmd[0] = 0x29;
		ccmd[1] = 0x40;
	} else
		ccmd[0] = 0x01;

	if((ret=__card_writecmd(drv_no,ccmd,5))!=0) {
		return ret;
	}
	if((ret=__card_readresponse(drv_no,_ioResponse[drv_no],1))!=0) return ret;
	if((ret=__check_response(drv_no,_ioResponse[drv_no][0]))!=0) return ret;
	if(_ioError[drv_no]&CARDIO_OP_IOERR_IDLE) return CARDIO_ERROR_IOERROR;

	return CARDIO_ERROR_READY;
}

static s32 __card_sendCMD8(s32 drv_no)
{
	s32 ret;
	u8 ccmd[5] = {0,0,0,0,0};

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	ccmd[0] = 0x08;
	ccmd[3] = 0x01;
	ccmd[4] = 0xAA;
	if((ret=__card_writecmd(drv_no,ccmd,5))!=0){
		return ret;
	}
	if((ret=__card_readresponse(drv_no,_ioResponse[drv_no],5))!=0) return ret;
	ret = __check_response(drv_no,_ioResponse[drv_no][0]);

	return ret;
}

static s32 __card_sendCMD58(s32 drv_no)
{
	s32 ret;
	u8 ccmd[5] = {0,0,0,0,0};

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	ccmd[0]= 0x3A;
	if((ret=__card_writecmd(drv_no,ccmd,5))!=0) {
		return ret;
	}
	if((ret=__card_readresponse(drv_no,_ioResponse[drv_no],5))!=0) return ret;
	ret = __check_response(drv_no,_ioResponse[drv_no][0]);

	return ret;
}

static s32 __card_sendcmd(s32 drv_no,u8 cmd,u8 *arg)
{
	u8 ccmd[5] = {0,0,0,0,0};

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	ccmd[0] = cmd;
	if(arg) {
		ccmd[1] = arg[0];
		ccmd[2] = arg[1];
		ccmd[3] = arg[2];
		ccmd[4] = arg[3];
	}
	return __card_writecmd(drv_no,ccmd,5);
}

static s32 __card_setblocklen(s32 drv_no,u32 block_len)
{
	u8 cmd[5];
	s32 ret;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;
	if(block_len>PAGE_SIZE512) block_len = PAGE_SIZE512;

	cmd[0] = 0x10;
	cmd[1] = (block_len>>24)&0xff;
	cmd[2] = (block_len>>16)&0xff;
	cmd[3] = (block_len>>8)&0xff;
	cmd[4] = block_len&0xff;
	if((ret=__card_writecmd(drv_no,cmd,5))!=0) {
		return ret;
	}
	if((ret=__card_readresponse(drv_no,_ioResponse[drv_no],1))<0) return ret;
	ret = __check_response(drv_no,_ioResponse[drv_no][0]);

	return ret;
}

static s32 __card_readcsd(s32 drv_no)
{
	u8 ccmd[5] = {0,0,0,0,0};
	s32 ret;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;
	ret = 0;
	ccmd[0] = 0x09;
	if((ret=__card_writecmd(drv_no,ccmd,5))!=0) {
		return ret;
	}
	if((ret=__card_readresponse(drv_no,_ioResponse[drv_no],1))!=0) return ret;
	ret = __check_response(drv_no,_ioResponse[drv_no][0]);
	if(ret==0) {
		if((ret=__card_dataread(drv_no,g_CSD[drv_no],16))!=0) return ret;
	}
	return ret;
}

static s32 __card_readcid(s32 drv_no)
{
	u8 ccmd[5] = {0,0,0,0,0};
	s32 ret;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;
	ret = 0;
	ccmd[0] = 0x0A;
	if((ret=__card_writecmd(drv_no,ccmd,5))!=0) {
		return ret;
	}
	if((ret=__card_readresponse(drv_no,_ioResponse[drv_no],1))!=0) return ret;
	ret = __check_response(drv_no,_ioResponse[drv_no][0]);
	if(ret==0) {
		if((ret=__card_dataread(drv_no,g_CID[drv_no],16))!=0) return ret;
	}
	return ret;
}

static s32 __card_sd_status(s32 drv_no)
{
	s32 ret;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;
	if(_ioPageSize[drv_no]!=64) {
		_ioPageSize[drv_no] = 64;
		if((ret=__card_setblocklen(drv_no,_ioPageSize[drv_no]))!=0) return ret;
	}
	if((ret=__card_sendappcmd(drv_no))!=0) return ret;
	if((ret=__card_sendcmd(drv_no,0x0d,NULL))!=0) return ret;
	if((ret=__card_response2(drv_no))!=0) return ret;
	ret = __card_dataread(drv_no,g_CardStatus[drv_no],64);

	return ret;
}

static s32 __card_softreset(s32 drv_no)
{
	s32 ret;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;
	ret = 0;
	if((ret=__card_writecmd0(drv_no))!=0) {
		return ret;
	}

	if((ret=__card_readresponse(drv_no,_ioResponse[drv_no],1))!=0) return ret;
	return __check_response(drv_no,_ioResponse[drv_no][0]);
}

static bool __card_check(s32 drv_no)
{
	s32 ret;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return FALSE;
	while((ret=EXI_ProbeEx(drv_no))==0);
	if(ret!=1) return FALSE;

	if(!(EXI_GetState(drv_no)&EXI_FLAG_ATTACH)) {
		if(EXI_Attach(drv_no,__card_exthandler)==0) return FALSE;
		sdgecko_insertedCB(drv_no);
	}
	return TRUE;
}

static s32 __card_retrycb(s32 drv_no)
{
	_ioRetryCB = NULL;
	_ioRetryCnt++;
	return sdgecko_initIO(drv_no);
}

static void __convert_sector(s32 drv_no,u32 sector_no,u8 *arg)
{
	if(_ioAddressingType[drv_no]==BYTE_ADDRESSING) {
		arg[0] = (sector_no>>15)&0xff;
		arg[1] = (sector_no>>7)&0xff;
		arg[2] = (sector_no<<1)&0xff;
		arg[3] = (sector_no<<9)&0xff;
	} else if(_ioAddressingType[drv_no]==SECTOR_ADDRESSING) {
		arg[0] = (sector_no>>24)&0xff;
		arg[1] = (sector_no>>16)&0xff;
		arg[2] = (sector_no>>8)&0xff;
		arg[3] = sector_no&0xff;
	}
}

void sdgecko_initIODefault()
{
	u32 i;
	__init_crc7();
	__init_crc16();
	for(i=0;i<MAX_DRIVE;++i) {
		_ioRetryCnt = 0;
		_ioError[i] = 0;
		_ioCardInserted[i] = FALSE;
		_ioFlag[i] = NOT_INITIALIZED;
		_ioAddressingType[i] = BYTE_ADDRESSING;
		_initType[i] = TYPE_SD;
		LWP_InitQueue(&_ioEXILock[i]);
	}
}

s32 sdgecko_initIO(s32 drv_no)
{
	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	u32 id = 0;
	EXI_GetID(drv_no,EXI_DEVICE_0,&id);
	if ( id != -1 ) return CARDIO_ERROR_NOCARD;

	if(_ioRetryCnt>5) {
		_ioRetryCnt = 0;
		return CARDIO_ERROR_IOERROR;
	}

	_ioCardInserted[drv_no] = __card_check(drv_no);

	if(_ioCardInserted[drv_no]==TRUE) {
		_ioWPFlag = 0;
		_ioCardFreq = EXI_SPEED16MHZ;
		_initType[drv_no] = TYPE_SD;
		_ioFlag[drv_no] = INITIALIZING;
		_ioAddressingType[drv_no] = BYTE_ADDRESSING;
		if(__card_softreset(drv_no)!=0) {
			_ioWPFlag = 1;
			if(__card_softreset(drv_no)!=0) goto exit;
		}

		if(__card_sendCMD8(drv_no)!=0) goto exit;
		if((_ioResponse[drv_no][3]==1) && (_ioResponse[drv_no][4]==0xAA)) _initType[drv_no] = TYPE_SDHC;

		if(__card_sendopcond(drv_no)!=0) goto exit;
		if(__card_readcsd(drv_no)!=0) goto exit;
		if(__card_readcid(drv_no)!=0) goto exit;

		if(_initType[drv_no]==TYPE_SDHC) {
			if(__card_sendCMD58(drv_no)!=0) goto exit;
			if(_ioResponse[drv_no][1]&0x40) _ioAddressingType[drv_no] = SECTOR_ADDRESSING;
		}

		_ioPageSize[drv_no] = 1<<WRITE_BL_LEN(drv_no);
		if(__card_setblocklen(drv_no,_ioPageSize[drv_no])!=0) goto exit;

		if(__card_sd_status(drv_no)!=0) goto exit;

		_ioRetryCnt = 0;
		_ioFlag[drv_no] = INITIALIZED;
		return CARDIO_ERROR_READY;
exit:
		_ioRetryCB = __card_retrycb;
		return sdgecko_doUnmount(drv_no);
	}
	return CARDIO_ERROR_NOCARD;
}

s32 sdgecko_preIO(s32 drv_no)
{
	s32 ret;

	if(_ioFlag[drv_no]!=INITIALIZED) {
		ret = sdgecko_initIO(drv_no);
		if(ret!=CARDIO_ERROR_READY) {
			return ret;
		}
	}
	return CARDIO_ERROR_READY;
}

s32 sdgecko_readCID(s32 drv_no)
{
	s32 ret;

	if(drv_no<EXI_CHANNEL_0 || drv_no>=EXI_CHANNEL_2) return CARDIO_ERROR_NOCARD;
	ret = sdgecko_preIO(drv_no);
	if(ret!=0) return ret;

	return __card_readcid(drv_no);
}

s32 sdgecko_readCSD(s32 drv_no)
{
	s32 ret;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;
	ret = sdgecko_preIO(drv_no);
	if(ret!=0) return ret;

	return __card_readcsd(drv_no);
}

s32 sdgecko_readStatus(s32 drv_no)
{
	s32 ret;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;
	ret = sdgecko_preIO(drv_no);
	if(ret!=0) return ret;

	return __card_sd_status(drv_no);
}

// Multiple sector read by emu_kidid
s32 sdgecko_readSectors(s32 drv_no,u32 sector_no,u32 num_sectors,void *buf)
{
	u32 i;
	s32 ret;
	u8 arg[4];
	char *ptr = (char*)buf;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	ret = sdgecko_preIO(drv_no);
	if(ret!=0) return ret;

	if(num_sectors<1) return CARDIO_ERROR_INTERNAL;

	// Must be 512b, otherwise fail!
	if(PAGE_SIZE512!=_ioPageSize[drv_no]) {
		_ioPageSize[drv_no] = PAGE_SIZE512;
		if((ret=__card_setblocklen(drv_no,PAGE_SIZE512))!=0) return ret;
	}

	// SDHC support fix
	__convert_sector(drv_no,sector_no,arg);

	if((ret=__card_sendcmd(drv_no,0x12,arg))!=0) return ret;
	if((ret=__card_response1(drv_no))!=0) return ret;

	for(i=0;i<num_sectors;i++) {
		if((ret=__card_dataread(drv_no,ptr,_ioPageSize[drv_no]))!=0) return ret;
		ptr += _ioPageSize[drv_no];
	}

	if((ret=__card_sendcmd(drv_no,0x0C,NULL))!=0) return ret;
	return __card_stopresponse(drv_no);
}

s32 sdgecko_writeSectors(s32 drv_no,u32 sector_no,u32 num_sectors,const void *buf)
{
	u32 i;
	s32 ret;
	u8 arg[4];
	char *ptr = (char*)buf;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	ret = sdgecko_preIO(drv_no);
	if(ret!=0) return ret;

	if(num_sectors<1) return CARDIO_ERROR_INTERNAL;

	if(PAGE_SIZE512!=_ioPageSize[drv_no]) {
		_ioPageSize[drv_no] = PAGE_SIZE512;
		if((ret=__card_setblocklen(drv_no,_ioPageSize[drv_no]))!=0) return ret;
	}

	// send SET_WRITE_BLK_ERASE_CNT cmd
	arg[0] = (num_sectors>>24)&0xff;
	arg[1] = (num_sectors>>16)&0xff;
	arg[2] = (num_sectors>>8)&0xff;
	arg[3] = num_sectors&0xff;
	if((ret=__card_sendappcmd(drv_no))!=0) return ret;
	if((ret=__card_sendcmd(drv_no,0x17,arg))!=0) return ret;
	if((ret=__card_response1(drv_no))!=0) return ret;

	// SDHC support fix
	__convert_sector(drv_no,sector_no,arg);

	if((ret=__card_sendcmd(drv_no,0x19,arg))!=0) return ret;
	if((ret=__card_response1(drv_no))!=0) return ret;

	for(i=0;i<num_sectors;i++) {
		if((ret=__card_multidatawrite(drv_no,ptr,_ioPageSize[drv_no]))!=0) return ret;
		if((ret=__card_dataresponse(drv_no))!=0) {
			if((ret=__card_sendcmd(drv_no,0x0C,arg))!=0) return ret;
			return __card_stopresponse(drv_no);
		}
		ptr += _ioPageSize[drv_no];
	}

	if((ret=__card_multiwritestop(drv_no))!=0) return ret;
	if((ret=__card_sendcmd(drv_no,0x0D,NULL))!=0) return ret;
	return __card_response2(drv_no);
}

s32 sdgecko_doUnmount(s32 drv_no)
{
	s32 ret;

	if(drv_no<0 || drv_no>=MAX_DRIVE) return CARDIO_ERROR_NOCARD;

	if(__card_check(drv_no)==TRUE && _ioFlag[drv_no]!=NOT_INITIALIZED) {
		if((ret=__card_sendappcmd(drv_no))!=0) goto exit;
		if((ret=__card_sendcmd(drv_no,0x2a,NULL))!=0) goto exit;
		ret = __card_response1(drv_no);
	}
	_ioFlag[drv_no] = NOT_INITIALIZED;

exit:
	if(_ioCardInserted[drv_no]==TRUE) {
		_ioCardInserted[drv_no] = FALSE;
		EXI_Detach(drv_no);
	}
	if(_ioRetryCB)
		return _ioRetryCB(drv_no);

	return CARDIO_ERROR_READY;
}

static void (*pfCallbackIN[MAX_DRIVE])(s32) = {NULL, NULL};
static void (*pfCallbackOUT[MAX_DRIVE])(s32) = {NULL, NULL};

void sdgecko_insertedCB(s32 drv_no)
{
	if(pfCallbackIN[drv_no])
		pfCallbackIN[drv_no](drv_no);
}

void sdgecko_ejectedCB(s32 drv_no)
{
	if(pfCallbackOUT[drv_no])
		pfCallbackOUT[drv_no](drv_no);
}

void sdgecko_setSpeed(u32 freq)
{
	_ioCardFreq = freq;
}

u32 sdgecko_getPageSize(s32 drv_no)
{
	return _ioPageSize[drv_no];
}

u32 sdgecko_setPageSize(s32 drv_no, int size)
{
	if(_ioPageSize[drv_no]!=size)
		_ioPageSize[drv_no] = size;

	return __card_setblocklen(drv_no, _ioPageSize[drv_no]);
}

u32 sdgecko_getAddressingType(s32 drv_no)
{
	return _ioAddressingType[drv_no];
}
