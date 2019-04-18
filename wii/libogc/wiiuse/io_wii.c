/* This source as presented is a modified version of original wiiuse for use
 * with RetroArch, and must not be confused with the original software. */

#ifdef GEKKO

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "definitions.h"
#include "wiiuse_internal.h"
#include "events.h"
#include "io.h"
#include "lwp_wkspace.h"

#define MAX_COMMANDS					0x100
#define MAX_WIIMOTES					5

static vu32* const _ipcReg = (u32*)0xCD000000;
static u8 *__queue_buffer[MAX_WIIMOTES] = { 0, 0, 0, 0, 0 };

extern void parse_event(struct wiimote_t *wm);
extern void idle_cycle(struct wiimote_t* wm);
extern void hexdump(void *d, int len);

static __inline__ u32 ACR_ReadReg(u32 reg)
{
	return _ipcReg[reg>>2];
}

static __inline__ void ACR_WriteReg(u32 reg,u32 val)
{
	_ipcReg[reg>>2] = val;
}

static s32 __wiiuse_disconnected(void *arg,struct bte_pcb *pcb,u8 err)
{
	struct wiimote_listen_t *wml = (struct wiimote_listen_t*)arg;
	struct wiimote_t *wm = wml->wm;

	if(!wm) return ERR_OK;

	//printf("wiimote disconnected\n");
	WIIMOTE_DISABLE_STATE(wm, (WIIMOTE_STATE_IR|WIIMOTE_STATE_IR_INIT));
#ifdef HAVE_WIIUSE_SPEAKER
	WIIMOTE_DISABLE_STATE(wm, (WIIMOTE_STATE_SPEAKER|WIIMOTE_STATE_SPEAKER_INIT));
#endif
	WIIMOTE_DISABLE_STATE(wm, (WIIMOTE_STATE_EXP|WIIMOTE_STATE_EXP_HANDSHAKE|WIIMOTE_STATE_EXP_FAILED));
	WIIMOTE_DISABLE_STATE(wm,(WIIMOTE_STATE_CONNECTED|WIIMOTE_STATE_HANDSHAKE|WIIMOTE_STATE_HANDSHAKE_COMPLETE));

	while(wm->cmd_head) {
		__lwp_queue_append(&wm->cmdq,&wm->cmd_head->node);
		wm->cmd_head = wm->cmd_head->next;
	}
	wm->cmd_tail = NULL;

	if(wm->event_cb) wm->event_cb(wm,WIIUSE_DISCONNECT);

	wml->wm = NULL;
	return ERR_OK;
}

static s32 __wiiuse_receive(void *arg,void *buffer,u16 len)
{
	struct wiimote_listen_t *wml = (struct wiimote_listen_t*)arg;
	struct wiimote_t *wm = wml->wm;

	if(!wm || !buffer || len==0) return ERR_OK;

	//printf("__wiiuse_receive[%02x]\n",*(char*)buffer);
	wm->event = WIIUSE_NONE;

	memcpy(wm->event_buf,buffer,len);
	memset(&(wm->event_buf[len]),0,(MAX_PAYLOAD - len));
	parse_event(wm);

	if(wm->event!=WIIUSE_NONE) {
		if(wm->event_cb) wm->event_cb(wm,wm->event);
	}

	return ERR_OK;
}

static s32 __wiiuse_connected(void *arg,struct bte_pcb *pcb,u8 err)
{
	struct wiimote_listen_t *wml = (struct wiimote_listen_t*)arg;
	struct wiimote_t *wm;

	wm = wml->assign_cb(&wml->bdaddr);

	if(!wm) {
		bte_disconnect(wml->sock);
		return ERR_OK;
	}

	wml->wm = wm;

	wm->sock = wml->sock;
	wm->bdaddr = wml->bdaddr;

	//printf("__wiiuse_connected()\n");
	WIIMOTE_ENABLE_STATE(wm,(WIIMOTE_STATE_CONNECTED|WIIMOTE_STATE_HANDSHAKE));

	wm->handshake_state = 0;
	wiiuse_handshake(wm,NULL,0);

	return ERR_OK;
}

void __wiiuse_sensorbar_enable(int enable)
{
	u32 val;
	u32 level;

	level = IRQ_Disable();
	val = (ACR_ReadReg(0xc0)&~0x100);
	if(enable) val |= 0x100;
	ACR_WriteReg(0xc0,val);
	IRQ_Restore(level);
}

int wiiuse_register(struct wiimote_listen_t *wml, struct bd_addr *bdaddr, struct wiimote_t *(*assign_cb)(struct bd_addr *bdaddr))
{
	s32 err;

	if(!wml || !bdaddr || !assign_cb) return 0;

	wml->wm = NULL;
	wml->bdaddr = *bdaddr;
	wml->sock = bte_new();
	wml->assign_cb = assign_cb;
	if(wml->sock==NULL) return 0;

	bte_arg(wml->sock,wml);
	bte_received(wml->sock,__wiiuse_receive);
	bte_disconnected(wml->sock,__wiiuse_disconnected);

	err = bte_registerdeviceasync(wml->sock,bdaddr,__wiiuse_connected);
	if(err==ERR_OK) return 1;

	return 0;
}

void wiiuse_disconnect(struct wiimote_t *wm)
{
	if(wm==NULL || wm->sock==NULL) return;

	WIIMOTE_DISABLE_STATE(wm,WIIMOTE_STATE_CONNECTED);
	bte_disconnect(wm->sock);
}

void wiiuse_sensorbar_enable(int enable)
{
	__wiiuse_sensorbar_enable(enable);
}

void wiiuse_init_cmd_queue(struct wiimote_t *wm)
{
	u32 size;

	if (!__queue_buffer[wm->unid]) {
		size = (MAX_COMMANDS*sizeof(struct cmd_blk_t));
		__queue_buffer[wm->unid] = __lwp_heap_allocate(&__wkspace_heap,size);
		if(!__queue_buffer[wm->unid]) return;
	}

	__lwp_queue_initialize(&wm->cmdq,__queue_buffer[wm->unid],MAX_COMMANDS,sizeof(struct cmd_blk_t));
}

int wiiuse_io_write(struct wiimote_t *wm,ubyte *buf,int len)
{
	if(wm->sock) {
		return bte_senddata(wm->sock,buf,len);
	}

	return ERR_CONN;
}

#endif
