/*-------------------------------------------------------------

ipc.c -- Interprocess Communication with Starlet

Copyright (C) 2008
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)
Hector Martin (marcan)

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

#if defined(HW_RVL)

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <malloc.h>
#include <time.h>
#include <gcutil.h>
#include "asm.h"
#include "processor.h"
#include "lwp.h"
#include "irq.h"
#include "ipc.h"
#include "cache.h"
#include "system.h"
#include "lwp_heap.h"
#include "lwp_wkspace.h"

#define IPC_HEAP_SIZE			4096
#define IPC_REQUESTSIZE			64
#define IPC_NUMHEAPS			16

#define IOS_MAXFMT_PARAMS		32

#define IOS_OPEN				0x01
#define IOS_CLOSE				0x02
#define IOS_READ				0x03
#define IOS_WRITE				0x04
#define IOS_SEEK				0x05
#define IOS_IOCTL				0x06
#define IOS_IOCTLV				0x07

#define RELNCH_RELAUNCH 1
#define RELNCH_BACKGROUND 2

struct _ipcreq
{						//ipc struct size: 32
	u32 cmd;			//0
	s32 result;			//4
	union {				//8
		s32 fd;
		u32 req_cmd;
	};
	union {
		struct {
			char *filepath;
			u32 mode;
		} open;
		struct {
			void *data;
			u32 len;
		} read, write;
		struct {
			s32 where;
			s32 whence;
		} seek;
		struct {
			u32 ioctl;
			void *buffer_in;
			u32 len_in;
			void *buffer_io;
			u32 len_io;
		} ioctl;
		struct {
			u32 ioctl;
			u32 argcin;
			u32 argcio;
			struct _ioctlv *argv;
		} ioctlv;
		u32 args[5];
	};

	ipccallback cb;		//32
	void *usrdata;		//36
	u32 relnch;			//40
	lwpq_t syncqueue;	//44
	u32 magic;			//48 - used to avoid spurious responses, like from zelda.
	u8 pad1[12];		//52 - 60
} ATTRIBUTE_PACKED;

struct _ipcreqres
{
	u32 cnt_sent;
	u32 cnt_queue;
	u32 req_send_no;
	u32 req_queue_no;
	struct _ipcreq *reqs[16];
};

struct _ipcheap
{
	void *membase;
	u32 size;
	heap_cntrl heap;
};

struct _ioctlvfmt_bufent
{
	void *ipc_buf;
	void *io_buf;
	s32 copy_len;
};

struct _ioctlvfmt_cbdata
{
	ipccallback user_cb;
	void *user_data;
	s32 num_bufs;
	u32 hId;
	struct _ioctlvfmt_bufent *bufs;
};

static u32 IPC_REQ_MAGIC;

static s32 _ipc_hid = -1;
static s32 _ipc_mailboxack = 1;
static u32 _ipc_relnchFl = 0;
static u32 _ipc_initialized = 0;
static u32 _ipc_clntinitialized = 0;
static u64 _ipc_spuriousresponsecnt = 0;
static struct _ipcreq *_ipc_relnchRpc = NULL;

static void *_ipc_bufferlo = NULL;
static void *_ipc_bufferhi = NULL;
static void *_ipc_currbufferlo = NULL;
static void *_ipc_currbufferhi = NULL;

static u32 _ipc_seed = 0xffffffff;

static struct _ipcreqres _ipc_responses;

static struct _ipcheap _ipc_heaps[IPC_NUMHEAPS] =
{
	{NULL, 0, {}} // all other elements should be inited to zero, says C standard, so this should do
};

static vu32* const _ipcReg = (u32*)0xCD000000;

extern void __MaskIrq(u32 nMask);
extern void __UnmaskIrq(u32 nMask);
extern void* __SYS_GetIPCBufferLo(void);
extern void* __SYS_GetIPCBufferHi(void);

extern u32 gettick();

static __inline__ u32 IPC_ReadReg(u32 reg)
{
	return _ipcReg[reg];
}

static __inline__ void IPC_WriteReg(u32 reg,u32 val)
{
	_ipcReg[reg] = val;
}

static __inline__ void ACR_WriteReg(u32 reg,u32 val)
{
	_ipcReg[reg>>2] = val;
}

static __inline__ void* __ipc_allocreq()
{
	return iosAlloc(_ipc_hid,IPC_REQUESTSIZE);
}

static __inline__ void __ipc_freereq(void *ptr)
{
	iosFree(_ipc_hid,ptr);
}

static __inline__ void __ipc_srand(u32 seed)
{
	_ipc_seed = seed;
}

static __inline__ u32 __ipc_rand()
{
	_ipc_seed = (214013*_ipc_seed) + 2531011;
	return _ipc_seed;
}

static s32 __ioctlvfmtCB(s32 result,void *userdata)
{
	ipccallback user_cb;
	void *user_data;
	struct _ioctlvfmt_cbdata *cbdata;
	struct _ioctlvfmt_bufent *pbuf;

	cbdata = (struct _ioctlvfmt_cbdata*)userdata;

	// deal with data buffers
	if(cbdata->bufs) {
		pbuf = cbdata->bufs;
		while(cbdata->num_bufs--) {
			if(pbuf->ipc_buf) {
				// copy data if needed
				if(pbuf->io_buf && pbuf->copy_len)
					memcpy(pbuf->io_buf, pbuf->ipc_buf, pbuf->copy_len);
				// then free the buffer
				iosFree(cbdata->hId, pbuf->ipc_buf);
			}
			pbuf++;
		}
	}

	user_cb = cbdata->user_cb;
	user_data = cbdata->user_data;

	// free buffer list
	__lwp_wkspace_free(cbdata->bufs);

	// free callback data
	__lwp_wkspace_free(cbdata);

	// call the user callback
	if(user_cb)
		return user_cb(result, user_data);

	return result;
}

static s32 __ipc_queuerequest(struct _ipcreq *req)
{
	u32 cnt;
	u32 level;
	_CPU_ISR_Disable(level);

	cnt = (_ipc_responses.cnt_queue - _ipc_responses.cnt_sent);
	if(cnt>=16) {
		_CPU_ISR_Restore(level);
		return IPC_EQUEUEFULL;
	}

	_ipc_responses.reqs[_ipc_responses.req_queue_no] = req;
	_ipc_responses.req_queue_no = ((_ipc_responses.req_queue_no+1)&0x0f);
	_ipc_responses.cnt_queue++;

	_CPU_ISR_Restore(level);
	return IPC_OK;
}

static s32 __ipc_syncqueuerequest(struct _ipcreq *req)
{
	u32 cnt = (_ipc_responses.cnt_queue - _ipc_responses.cnt_sent);
	if(cnt>=16) {
		return IPC_EQUEUEFULL;
	}

	_ipc_responses.reqs[_ipc_responses.req_queue_no] = req;
	_ipc_responses.req_queue_no = ((_ipc_responses.req_queue_no+1)&0x0f);
	_ipc_responses.cnt_queue++;

	return IPC_OK;
}

static void __ipc_sendrequest()
{
	u32 ipc_send;
	struct _ipcreq *req;
	u32 cnt = (_ipc_responses.cnt_queue - _ipc_responses.cnt_sent);
	if(cnt>0) {
		req = _ipc_responses.reqs[_ipc_responses.req_send_no];
		if(req!=NULL) {
			req->magic = IPC_REQ_MAGIC;
			if(req->relnch&RELNCH_RELAUNCH) {
				_ipc_relnchFl = 1;
				_ipc_relnchRpc = req;
				if(!(req->relnch&RELNCH_BACKGROUND))
					_ipc_mailboxack--;
			}
			DCFlushRange(req,sizeof(struct _ipcreq));

			IPC_WriteReg(0,MEM_VIRTUAL_TO_PHYSICAL(req));
			_ipc_responses.req_send_no = ((_ipc_responses.req_send_no+1)&0x0f);
			_ipc_responses.cnt_sent++;
			_ipc_mailboxack--;

			ipc_send = ((IPC_ReadReg(1)&0x30)|0x01);
			IPC_WriteReg(1,ipc_send);
		}
	}
}

static void __ipc_replyhandler()
{
	u32 ipc_ack,cnt;
	ioctlv *v = NULL;
	struct _ipcreq *req = (struct _ipcreq*)IPC_ReadReg(2);
	if(req==NULL) return;

	ipc_ack = ((IPC_ReadReg(1)&0x30)|0x04);
	IPC_WriteReg(1,ipc_ack);
	ACR_WriteReg(48,0x40000000);

	req = MEM_PHYSICAL_TO_K0(req);
	DCInvalidateRange(req,32);

	if(req->magic==IPC_REQ_MAGIC) {
		if(req->req_cmd==IOS_READ) {
			if(req->read.data!=NULL) {
				req->read.data = MEM_PHYSICAL_TO_K0(req->read.data);
				if(req->result>0) DCInvalidateRange(req->read.data,req->result);
			}
		} else if(req->req_cmd==IOS_IOCTL) {
			if(req->ioctl.buffer_io!=NULL) {
				req->ioctl.buffer_io = MEM_PHYSICAL_TO_K0(req->ioctl.buffer_io);
				DCInvalidateRange(req->ioctl.buffer_io,req->ioctl.len_io);
			}
			DCInvalidateRange(req->ioctl.buffer_in,req->ioctl.len_in);
		} else if(req->req_cmd==IOS_IOCTLV) {
			if(req->ioctlv.argv!=NULL) {
				req->ioctlv.argv = MEM_PHYSICAL_TO_K0(req->ioctlv.argv);
				DCInvalidateRange(req->ioctlv.argv,((req->ioctlv.argcin+req->ioctlv.argcio)*sizeof(struct _ioctlv)));
			}

			cnt = 0;
			v = (ioctlv*)req->ioctlv.argv;
			while(cnt<(req->ioctlv.argcin+req->ioctlv.argcio)) {
				if(v[cnt].data!=NULL) {
					v[cnt].data = MEM_PHYSICAL_TO_K0(v[cnt].data);
					DCInvalidateRange(v[cnt].data,v[cnt].len);
				}
				cnt++;
			}
			if(_ipc_relnchFl && _ipc_relnchRpc==req) {
				_ipc_relnchFl = 0;
				if(_ipc_mailboxack<1) _ipc_mailboxack++;
			}

		}

		if(req->cb!=NULL) {
			req->cb(req->result,req->usrdata);
			__ipc_freereq(req);
		} else
			LWP_ThreadSignal(req->syncqueue);
	} else {
		// NOTE: we really want to find out if this ever happens
		// and take steps to prevent it beforehand (because it will
		// clobber memory, among other things). I suggest leaving this in
		// even in non-DEBUG mode. Maybe even cause a system halt.
		// It is the responsibility of the loader to clear these things,
		// but we want to find out if they happen so loaders can be fixed.
		_ipc_spuriousresponsecnt++;
	}
	ipc_ack = ((IPC_ReadReg(1)&0x30)|0x08);
	IPC_WriteReg(1,ipc_ack);
}

static void __ipc_ackhandler()
{
	u32 ipc_ack;
	ipc_ack = ((IPC_ReadReg(1)&0x30)|0x02);
	IPC_WriteReg(1,ipc_ack);
	ACR_WriteReg(48,0x40000000);

	if(_ipc_mailboxack<1) _ipc_mailboxack++;
	if(_ipc_mailboxack>0) {
		if(_ipc_relnchFl){
			_ipc_relnchRpc->result = 0;
			_ipc_relnchFl = 0;

			LWP_ThreadSignal(_ipc_relnchRpc->syncqueue);

			ipc_ack = ((IPC_ReadReg(1)&0x30)|0x08);
			IPC_WriteReg(1,ipc_ack);
		}
		__ipc_sendrequest();
	}

}

static void __ipc_interrupthandler(u32 irq,void *ctx)
{
	u32 ipc_int = IPC_ReadReg(1);
	if((ipc_int&0x0014)==0x0014) __ipc_replyhandler();

	ipc_int = IPC_ReadReg(1);
	if((ipc_int&0x0022)==0x0022) __ipc_ackhandler();
}

static s32 __ios_ioctlvformat_parse(const char *format,va_list args,struct _ioctlvfmt_cbdata *cbdata,s32 *cnt_in,s32 *cnt_io,struct _ioctlv **argv,s32 hId)
{
	s32 ret,i;
	void *pdata;
	void *iodata;
	char type,*ps;
	s32 len,maxbufs = 0;
	ioctlv *argp = NULL;
	struct _ioctlvfmt_bufent *bufp;

	if(hId == IPC_HEAP) hId = _ipc_hid;
	if(hId < 0) return IPC_EINVAL;

	maxbufs = strnlen(format,IOS_MAXFMT_PARAMS);
	if(maxbufs>=IOS_MAXFMT_PARAMS) return IPC_EINVAL;

	cbdata->hId = hId;
	cbdata->bufs = __lwp_wkspace_allocate((sizeof(struct _ioctlvfmt_bufent)*(maxbufs+1)));
	if(cbdata->bufs==NULL) return IPC_ENOMEM;

	argp = iosAlloc(hId,(sizeof(struct _ioctlv)*(maxbufs+1)));
	if(argp==NULL) {
		__lwp_wkspace_free(cbdata->bufs);
		return IPC_ENOMEM;
	}

	*argv = argp;
	bufp = cbdata->bufs;
	memset(argp,0,(sizeof(struct _ioctlv)*(maxbufs+1)));
	memset(bufp,0,(sizeof(struct _ioctlvfmt_bufent)*(maxbufs+1)));

	cbdata->num_bufs = 1;
	bufp->ipc_buf = argp;
	bufp++;

	*cnt_in = 0;
	*cnt_io = 0;

	ret = IPC_OK;
	while(*format) {
		type = tolower((int)*format);
		switch(type) {
			case 'b':
				pdata = iosAlloc(hId,sizeof(u8));
				if(pdata==NULL) {
					ret = IPC_ENOMEM;
					goto free_and_error;
				}
				*(u8*)pdata = va_arg(args,u32);
				argp->data = pdata;
				argp->len = sizeof(u8);
				bufp->ipc_buf = pdata;
				cbdata->num_bufs++;
				(*cnt_in)++;
				argp++;
				bufp++;
				break;
			case 'h':
				pdata = iosAlloc(hId,sizeof(u16));
				if(pdata==NULL) {
					ret = IPC_ENOMEM;
					goto free_and_error;
				}
				*(u16*)pdata = va_arg(args,u32);
				argp->data = pdata;
				argp->len = sizeof(u16);
				bufp->ipc_buf = pdata;
				cbdata->num_bufs++;
				(*cnt_in)++;
				argp++;
				bufp++;
				break;
			case 'i':
				pdata = iosAlloc(hId,sizeof(u32));
				if(pdata==NULL) {
					ret = IPC_ENOMEM;
					goto free_and_error;
				}
				*(u32*)pdata = va_arg(args,u32);
				argp->data = pdata;
				argp->len = sizeof(u32);
				bufp->ipc_buf = pdata;
				cbdata->num_bufs++;
				(*cnt_in)++;
				argp++;
				bufp++;
				break;
			case 'q':
				pdata = iosAlloc(hId,sizeof(u64));
				if(pdata==NULL) {
					ret = IPC_ENOMEM;
					goto free_and_error;
				}
				*(u64*)pdata = va_arg(args,u64);
				argp->data = pdata;
				argp->len = sizeof(u64);
				bufp->ipc_buf = pdata;
				cbdata->num_bufs++;
				(*cnt_in)++;
				argp++;
				bufp++;
				break;
			case 'd':
				argp->data = va_arg(args, void*);
				argp->len = va_arg(args, u32);
				(*cnt_in)++;
				argp++;
				break;
			case 's':
				ps = va_arg(args, char*);
				len = strnlen(ps,256);
				if(len>=256) {
					ret = IPC_EINVAL;
					goto free_and_error;
				}

				pdata = iosAlloc(hId,(len+1));
				if(pdata==NULL) {
					ret = IPC_ENOMEM;
					goto free_and_error;
				}
				memcpy(pdata,ps,(len+1));
				argp->data = pdata;
				argp->len = (len+1);
				bufp->ipc_buf = pdata;
				cbdata->num_bufs++;
				(*cnt_in)++;
				argp++;
				bufp++;
				break;
			case ':':
				format++;
				goto parse_io_params;
			default:
				ret = IPC_EINVAL;
				goto free_and_error;
		}
		format++;
	}

parse_io_params:
	while(*format) {
		type = tolower((int)*format);
		switch(type) {
			case 'b':
				pdata = iosAlloc(hId,sizeof(u8));
				if(pdata==NULL) {
					ret = IPC_ENOMEM;
					goto free_and_error;
				}
				iodata = va_arg(args,u8*);
				*(u8*)pdata = *(u8*)iodata;
				argp->data = pdata;
				argp->len = sizeof(u8);
				bufp->ipc_buf = pdata;
				bufp->io_buf = iodata;
				bufp->copy_len = sizeof(u8);
				cbdata->num_bufs++;
				(*cnt_io)++;
				argp++;
				bufp++;
				break;
			case 'h':
				pdata = iosAlloc(hId,sizeof(u16));
				if(pdata==NULL) {
					ret = IPC_ENOMEM;
					goto free_and_error;
				}
				iodata = va_arg(args,u16*);
				*(u16*)pdata = *(u16*)iodata;
				argp->data = pdata;
				argp->len = sizeof(u16);
				bufp->ipc_buf = pdata;
				bufp->io_buf = iodata;
				bufp->copy_len = sizeof(u16);
				cbdata->num_bufs++;
				(*cnt_io)++;
				argp++;
				bufp++;
				break;
			case 'i':
				pdata = iosAlloc(hId,sizeof(u32));
				if(pdata==NULL) {
					ret = IPC_ENOMEM;
					goto free_and_error;
				}
				iodata = va_arg(args,u32*);
				*(u32*)pdata = *(u32*)iodata;
				argp->data = pdata;
				argp->len = sizeof(u32);
				bufp->ipc_buf = pdata;
				bufp->io_buf = iodata;
				bufp->copy_len = sizeof(u32);
				cbdata->num_bufs++;
				(*cnt_io)++;
				argp++;
				bufp++;
				break;
			case 'q':
				pdata = iosAlloc(hId,sizeof(u64));
				if(pdata==NULL) {
					ret = IPC_ENOMEM;
					goto free_and_error;
				}
				iodata = va_arg(args,u64*);
				*(u64*)pdata = *(u64*)iodata;
				argp->data = pdata;
				argp->len = sizeof(u64);
				bufp->ipc_buf = pdata;
				bufp->io_buf = iodata;
				bufp->copy_len = sizeof(u64);
				cbdata->num_bufs++;
				(*cnt_io)++;
				argp++;
				bufp++;
				break;
			case 'd':
				argp->data = va_arg(args, void*);
				argp->len = va_arg(args, u32);
				(*cnt_io)++;
				argp++;
				break;
			default:
				ret = IPC_EINVAL;
				goto free_and_error;
		}
		format++;
	}
	return IPC_OK;

free_and_error:
	for(i=0;i<cbdata->num_bufs;i++) {
		if(cbdata->bufs[i].ipc_buf!=NULL) iosFree(hId,cbdata->bufs[i].ipc_buf);
	}
	__lwp_wkspace_free(cbdata->bufs);
	return ret;
}

static s32 __ipc_asyncrequest(struct _ipcreq *req)
{
	s32 ret;
	u32 level;

	ret = __ipc_queuerequest(req);
	if(ret) __ipc_freereq(req);
	else {
		_CPU_ISR_Disable(level);
		if(_ipc_mailboxack>0) __ipc_sendrequest();
		_CPU_ISR_Restore(level);
	}
	return ret;
}

static s32 __ipc_syncrequest(struct _ipcreq *req)
{
	s32 ret;
	u32 level;

	LWP_InitQueue(&req->syncqueue);

	_CPU_ISR_Disable(level);
	ret = __ipc_syncqueuerequest(req);
	if(ret==0) {
		if(_ipc_mailboxack>0) __ipc_sendrequest();
		LWP_ThreadSleep(req->syncqueue);
		ret = req->result;
	}
	_CPU_ISR_Restore(level);

	LWP_CloseQueue(req->syncqueue);
	return ret;
}

s32 iosCreateHeap(s32 size)
{
	s32 i,ret;
	s32 free;
	u32 level;
	u32 ipclo,ipchi;
	_CPU_ISR_Disable(level);

	i=0;
	while(i<IPC_NUMHEAPS) {
		if(_ipc_heaps[i].membase==NULL) break;
		i++;
	}
	if(i>=IPC_NUMHEAPS) {
		_CPU_ISR_Restore(level);
		return IPC_ENOHEAP;
	}

	ipclo = (((u32)IPC_GetBufferLo()+0x1f)&~0x1f);
	ipchi = (u32)IPC_GetBufferHi();
	free = (ipchi - (ipclo + size));
	if(free<0) return IPC_ENOMEM;

	_ipc_heaps[i].membase = (void*)ipclo;
	_ipc_heaps[i].size = size;

	ret = __lwp_heap_init(&_ipc_heaps[i].heap,(void*)ipclo,size,PPC_CACHE_ALIGNMENT);
	if(ret<=0) return IPC_ENOMEM;

	IPC_SetBufferLo((void*)(ipclo+size));
	_CPU_ISR_Restore(level);
	return i;
}

void* iosAlloc(s32 hid,s32 size)
{
	if(hid<0 || hid>=IPC_NUMHEAPS || size<=0) return NULL;
	return __lwp_heap_allocate(&_ipc_heaps[hid].heap,size);
}

void iosFree(s32 hid,void *ptr)
{
	if(hid<0 || hid>=IPC_NUMHEAPS || ptr==NULL) return;
	__lwp_heap_free(&_ipc_heaps[hid].heap,ptr);
}

void* IPC_GetBufferLo()
{
	return _ipc_currbufferlo;
}

void* IPC_GetBufferHi()
{
	return _ipc_currbufferhi;
}

void IPC_SetBufferLo(void *bufferlo)
{
	if(_ipc_bufferlo<=bufferlo) _ipc_currbufferlo = bufferlo;
}

void IPC_SetBufferHi(void *bufferhi)
{
	if(bufferhi<=_ipc_bufferhi) _ipc_currbufferhi = bufferhi;
}

void __IPC_Init(void)
{
	if(!_ipc_initialized) {
		_ipc_bufferlo = _ipc_currbufferlo = __SYS_GetIPCBufferLo();
		_ipc_bufferhi = _ipc_currbufferhi = __SYS_GetIPCBufferHi();
		_ipc_initialized = 1;
	}
}

u32 __IPC_ClntInit(void)
{
	if(!_ipc_clntinitialized) {
		_ipc_clntinitialized = 1;

		// generate a random request magic
		__ipc_srand(gettick());
		IPC_REQ_MAGIC = __ipc_rand();

		__IPC_Init();

		_ipc_hid = iosCreateHeap(IPC_HEAP_SIZE);
		IRQ_Request(IRQ_PI_ACR,__ipc_interrupthandler,NULL);
		__UnmaskIrq(IM_PI_ACR);
		IPC_WriteReg(1,56);
	}
	return IPC_OK;
}

void __IPC_Reinitialize(void)
{
	u32 level;

	_CPU_ISR_Disable(level);

	IPC_WriteReg(1,56);

	_ipc_mailboxack = 1;
	_ipc_relnchFl = 0;
	_ipc_relnchRpc = NULL;

	_ipc_responses.req_queue_no = 0;
	_ipc_responses.cnt_queue = 0;
	_ipc_responses.req_send_no = 0;
	_ipc_responses.cnt_sent = 0;

	_CPU_ISR_Restore(level);
}

s32 IOS_Open(const char *filepath,u32 mode)
{
	s32 ret;
	struct _ipcreq *req;

	if(filepath==NULL) return IPC_EINVAL;

	req = __ipc_allocreq();
	if(req==NULL) return IPC_ENOMEM;

	req->cmd = IOS_OPEN;
	req->cb = NULL;
	req->relnch = 0;

	DCFlushRange((void*)filepath,strnlen(filepath,IPC_MAXPATH_LEN) + 1);

	req->open.filepath	= (char*)MEM_VIRTUAL_TO_PHYSICAL(filepath);
	req->open.mode		= mode;

	ret = __ipc_syncrequest(req);

	if(req!=NULL) __ipc_freereq(req);
	return ret;
}

s32 IOS_OpenAsync(const char *filepath,u32 mode,ipccallback ipc_cb,void *usrdata)
{
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return IPC_ENOMEM;

	req->cmd = IOS_OPEN;
	req->cb = ipc_cb;
	req->usrdata = usrdata;
	req->relnch = 0;

	DCFlushRange((void*)filepath,strnlen(filepath,IPC_MAXPATH_LEN) + 1);

	req->open.filepath	= (char*)MEM_VIRTUAL_TO_PHYSICAL(filepath);
	req->open.mode		= mode;

	return __ipc_asyncrequest(req);
}

s32 IOS_Close(s32 fd)
{
	s32 ret;
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return IPC_ENOMEM;

	req->cmd = IOS_CLOSE;
	req->fd = fd;
	req->cb = NULL;
	req->relnch = 0;

	ret = __ipc_syncrequest(req);

	if(req!=NULL) __ipc_freereq(req);
	return ret;
}

s32 IOS_CloseAsync(s32 fd,ipccallback ipc_cb,void *usrdata)
{
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return IPC_ENOMEM;

	req->cmd = IOS_CLOSE;
	req->fd = fd;
	req->cb = ipc_cb;
	req->usrdata = usrdata;
	req->relnch = 0;

	return __ipc_asyncrequest(req);
}

s32 IOS_Read(s32 fd,void *buf,s32 len)
{
	s32 ret;
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return IPC_ENOMEM;

	req->cmd = IOS_READ;
	req->fd = fd;
	req->cb = NULL;
	req->relnch = 0;

	DCInvalidateRange(buf,len);
	req->read.data	= (void*)MEM_VIRTUAL_TO_PHYSICAL(buf);
	req->read.len	= len;

	ret = __ipc_syncrequest(req);

	if(req!=NULL) __ipc_freereq(req);
	return ret;
}

s32 IOS_ReadAsync(s32 fd,void *buf,s32 len,ipccallback ipc_cb,void *usrdata)
{
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return IPC_ENOMEM;

	req->cmd = IOS_READ;
	req->fd = fd;
	req->cb = ipc_cb;
	req->usrdata = usrdata;
	req->relnch = 0;

	DCInvalidateRange(buf,len);
	req->read.data	= (void*)MEM_VIRTUAL_TO_PHYSICAL(buf);
	req->read.len	= len;

	return __ipc_asyncrequest(req);
}

s32 IOS_Write(s32 fd,const void *buf,s32 len)
{
	s32 ret;
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return IPC_ENOMEM;

	req->cmd = IOS_WRITE;
	req->fd = fd;
	req->cb = NULL;
	req->relnch = 0;

	DCFlushRange((void*)buf,len);
	req->write.data	= (void*)MEM_VIRTUAL_TO_PHYSICAL(buf);
	req->write.len	= len;

	ret = __ipc_syncrequest(req);

	if(req!=NULL) __ipc_freereq(req);
	return ret;
}

s32 IOS_WriteAsync(s32 fd,const void *buf,s32 len,ipccallback ipc_cb,void *usrdata)
{
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return IPC_ENOMEM;

	req->cmd = IOS_WRITE;
	req->fd = fd;
	req->cb = ipc_cb;
	req->usrdata = usrdata;
	req->relnch = 0;

	DCFlushRange((void*)buf,len);
	req->write.data		= (void*)MEM_VIRTUAL_TO_PHYSICAL(buf);
	req->write.len		= len;

	return __ipc_asyncrequest(req);
}

s32 IOS_Seek(s32 fd,s32 where,s32 whence)
{
	s32 ret;
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return IPC_ENOMEM;

	req->cmd = IOS_SEEK;
	req->fd = fd;
	req->cb = NULL;
	req->relnch = 0;

	req->seek.where		= where;
	req->seek.whence	= whence;

	ret = __ipc_syncrequest(req);

	if(req!=NULL) __ipc_freereq(req);
	return ret;
}

s32 IOS_SeekAsync(s32 fd,s32 where,s32 whence,ipccallback ipc_cb,void *usrdata)
{
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return IPC_ENOMEM;

	req->cmd = IOS_SEEK;
	req->fd = fd;
	req->cb = ipc_cb;
	req->usrdata = usrdata;
	req->relnch = 0;

	req->seek.where		= where;
	req->seek.whence	= whence;

	return __ipc_asyncrequest(req);
}

s32 IOS_Ioctl(s32 fd,s32 ioctl,void *buffer_in,s32 len_in,void *buffer_io,s32 len_io)
{
	s32 ret;
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return IPC_ENOMEM;

	req->cmd = IOS_IOCTL;
	req->fd = fd;
	req->cb = NULL;
	req->relnch = 0;

	req->ioctl.ioctl		= ioctl;
	req->ioctl.buffer_in	= (void*)MEM_VIRTUAL_TO_PHYSICAL(buffer_in);
	req->ioctl.len_in		= len_in;
	req->ioctl.buffer_io	= (void*)MEM_VIRTUAL_TO_PHYSICAL(buffer_io);
	req->ioctl.len_io		= len_io;

	DCFlushRange(buffer_in,len_in);
	DCFlushRange(buffer_io,len_io);

	ret = __ipc_syncrequest(req);

	if(req!=NULL) __ipc_freereq(req);
	return ret;
}

s32 IOS_IoctlAsync(s32 fd,s32 ioctl,void *buffer_in,s32 len_in,void *buffer_io,s32 len_io,ipccallback ipc_cb,void *usrdata)
{
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return IPC_ENOMEM;

	req->cmd = IOS_IOCTL;
	req->fd = fd;
	req->cb = ipc_cb;
	req->usrdata = usrdata;
	req->relnch = 0;

	req->ioctl.ioctl		= ioctl;
	req->ioctl.buffer_in	= (void*)MEM_VIRTUAL_TO_PHYSICAL(buffer_in);
	req->ioctl.len_in		= len_in;
	req->ioctl.buffer_io	= (void*)MEM_VIRTUAL_TO_PHYSICAL(buffer_io);
	req->ioctl.len_io		= len_io;

	DCFlushRange(buffer_in,len_in);
	DCFlushRange(buffer_io,len_io);

	return __ipc_asyncrequest(req);
}

s32 IOS_Ioctlv(s32 fd,s32 ioctl,s32 cnt_in,s32 cnt_io,ioctlv *argv)
{
	s32 i,ret;
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return IPC_ENOMEM;

	req->cmd = IOS_IOCTLV;
	req->fd = fd;
	req->cb = NULL;
	req->relnch = 0;

	req->ioctlv.ioctl	= ioctl;
	req->ioctlv.argcin	= cnt_in;
	req->ioctlv.argcio	= cnt_io;
	req->ioctlv.argv	= (struct _ioctlv*)MEM_VIRTUAL_TO_PHYSICAL(argv);

	i = 0;
	while(i<cnt_in) {
		if(argv[i].data!=NULL && argv[i].len>0) {
			DCFlushRange(argv[i].data,argv[i].len);
			argv[i].data = (void*)MEM_VIRTUAL_TO_PHYSICAL(argv[i].data);
		}
		i++;
	}

	i = 0;
	while(i<cnt_io) {
		if(argv[cnt_in+i].data!=NULL && argv[cnt_in+i].len>0) {
			DCFlushRange(argv[cnt_in+i].data,argv[cnt_in+i].len);
			argv[cnt_in+i].data = (void*)MEM_VIRTUAL_TO_PHYSICAL(argv[cnt_in+i].data);
		}
		i++;
	}
	DCFlushRange(argv,((cnt_in+cnt_io)<<3));

	ret = __ipc_syncrequest(req);

	if(req!=NULL) __ipc_freereq(req);
	return ret;
}

s32 IOS_IoctlvAsync(s32 fd,s32 ioctl,s32 cnt_in,s32 cnt_io,ioctlv *argv,ipccallback ipc_cb,void *usrdata)
{
	s32 i;
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return IPC_ENOMEM;

	req->cmd = IOS_IOCTLV;
	req->fd = fd;
	req->cb = ipc_cb;
	req->usrdata = usrdata;
	req->relnch = 0;

	req->ioctlv.ioctl	= ioctl;
	req->ioctlv.argcin	= cnt_in;
	req->ioctlv.argcio	= cnt_io;
	req->ioctlv.argv	= (struct _ioctlv*)MEM_VIRTUAL_TO_PHYSICAL(argv);

	i = 0;
	while(i<cnt_in) {
		if(argv[i].data!=NULL && argv[i].len>0) {
			DCFlushRange(argv[i].data,argv[i].len);
			argv[i].data = (void*)MEM_VIRTUAL_TO_PHYSICAL(argv[i].data);
		}
		i++;
	}

	i = 0;
	while(i<cnt_io) {
		if(argv[cnt_in+i].data!=NULL && argv[cnt_in+i].len>0) {
			DCFlushRange(argv[cnt_in+i].data,argv[cnt_in+i].len);
			argv[cnt_in+i].data = (void*)MEM_VIRTUAL_TO_PHYSICAL(argv[cnt_in+i].data);
		}
		i++;
	}
	DCFlushRange(argv,((cnt_in+cnt_io)<<3));

	return __ipc_asyncrequest(req);
}

s32 IOS_IoctlvFormat(s32 hId,s32 fd,s32 ioctl,const char *format,...)
{
	s32 ret;
	va_list args;
	s32 cnt_in,cnt_io;
	struct _ioctlv *argv;
	struct _ioctlvfmt_cbdata *cbdata;

	cbdata = __lwp_wkspace_allocate(sizeof(struct _ioctlvfmt_cbdata));
	if(cbdata==NULL) return IPC_ENOMEM;

	memset(cbdata,0,sizeof(struct _ioctlvfmt_cbdata));

	va_start(args,format);
	ret = __ios_ioctlvformat_parse(format,args,cbdata,&cnt_in,&cnt_io,&argv,hId);
	va_end(args);
	if(ret<0) {
		__lwp_wkspace_free(cbdata);
		return ret;
	}

	ret = IOS_Ioctlv(fd,ioctl,cnt_in,cnt_io,argv);
	__ioctlvfmtCB(ret,cbdata);

	return ret;
}

s32 IOS_IoctlvFormatAsync(s32 hId,s32 fd,s32 ioctl,ipccallback usr_cb,void *usr_data,const char *format,...)
{
	s32 ret;
	va_list args;
	s32 cnt_in,cnt_io;
	struct _ioctlv *argv;
	struct _ioctlvfmt_cbdata *cbdata;

	cbdata = __lwp_wkspace_allocate(sizeof(struct _ioctlvfmt_cbdata));
	if(cbdata==NULL) return IPC_ENOMEM;

	memset(cbdata,0,sizeof(struct _ioctlvfmt_cbdata));

	va_start(args,format);
	ret = __ios_ioctlvformat_parse(format,args,cbdata,&cnt_in,&cnt_io,&argv,hId);
	va_end(args);
	if(ret<0) {
		__lwp_wkspace_free(cbdata);
		return ret;
	}

	cbdata->user_cb = usr_cb;
	cbdata->user_data = usr_data;
	return IOS_IoctlvAsync(fd,ioctl,cnt_in,cnt_io,argv,__ioctlvfmtCB,cbdata);
}

s32 IOS_IoctlvReboot(s32 fd,s32 ioctl,s32 cnt_in,s32 cnt_io,ioctlv *argv)
{
	s32 i,ret;
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return IPC_ENOMEM;

	req->cmd = IOS_IOCTLV;
	req->fd = fd;
	req->cb = NULL;
	req->relnch = RELNCH_RELAUNCH;

	req->ioctlv.ioctl	= ioctl;
	req->ioctlv.argcin	= cnt_in;
	req->ioctlv.argcio	= cnt_io;
	req->ioctlv.argv	= (struct _ioctlv*)MEM_VIRTUAL_TO_PHYSICAL(argv);

	i = 0;
	while(i<cnt_in) {
		if(argv[i].data!=NULL && argv[i].len>0) {
			DCFlushRange(argv[i].data,argv[i].len);
			argv[i].data = (void*)MEM_VIRTUAL_TO_PHYSICAL(argv[i].data);
		}
		i++;
	}

	i = 0;
	while(i<cnt_io) {
		if(argv[cnt_in+i].data!=NULL && argv[cnt_in+i].len>0) {
			DCFlushRange(argv[cnt_in+i].data,argv[cnt_in+i].len);
			argv[cnt_in+i].data = (void*)MEM_VIRTUAL_TO_PHYSICAL(argv[cnt_in+i].data);
		}
		i++;
	}
	DCFlushRange(argv,((cnt_in+cnt_io)<<3));

	ret = __ipc_syncrequest(req);

	if(req!=NULL) __ipc_freereq(req);
	return ret;
}

s32 IOS_IoctlvRebootBackground(s32 fd,s32 ioctl,s32 cnt_in,s32 cnt_io,ioctlv *argv)
{
	s32 i,ret;
	struct _ipcreq *req;

	req = __ipc_allocreq();
	if(req==NULL) return IPC_ENOMEM;

	req->cmd = IOS_IOCTLV;
	req->result = 0;
	req->fd = fd;
	req->cb = NULL;
	req->relnch = RELNCH_BACKGROUND|RELNCH_RELAUNCH;

	req->ioctlv.ioctl	= ioctl;
	req->ioctlv.argcin	= cnt_in;
	req->ioctlv.argcio	= cnt_io;
	req->ioctlv.argv	= (struct _ioctlv*)MEM_VIRTUAL_TO_PHYSICAL(argv);

	i = 0;
	while(i<cnt_in) {
		if(argv[i].data!=NULL && argv[i].len>0) {
			DCFlushRange(argv[i].data,argv[i].len);
			argv[i].data = (void*)MEM_VIRTUAL_TO_PHYSICAL(argv[i].data);
		}
		i++;
	}

	i = 0;
	while(i<cnt_io) {
		if(argv[cnt_in+i].data!=NULL && argv[cnt_in+i].len>0) {
			DCFlushRange(argv[cnt_in+i].data,argv[cnt_in+i].len);
			argv[cnt_in+i].data = (void*)MEM_VIRTUAL_TO_PHYSICAL(argv[cnt_in+i].data);
		}
		i++;
	}
	DCFlushRange(argv,((cnt_in+cnt_io)<<3));

	ret = __ipc_syncrequest(req);

	if(req!=NULL) __ipc_freereq(req);
	return ret;
}

#endif
