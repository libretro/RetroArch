#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <gccore.h>
#include <lwp_threads.h>

#include "bt.h"
#include "bte.h"
#include "hci.h"
#include "l2cap.h"
#include "btmemb.h"
#include "physbusif.h"

#define STACKSIZE						32768
#define MQ_BOX_SIZE						256

/* Vendor specific OGF */
#define HCI_VENDOR_OGF					0x3f

/* Vendor specific OCF */
#define HCI_VENDOR_PATCH_START_OCF		0x4f
#define HCI_VENDOR_PATCH_CONT_OCF		0x4c
#define HCI_VENDOR_PATCH_END_OCF		0x4f

enum bte_state {
	STATE_NOTREADY = -1,
	STATE_READY = 0,
	STATE_CONNECTING,
	STATE_CONNECTED,
	STATE_DISCONNECTING,
	STATE_DISCONNECTED,
	STATE_SENDING,
	STATE_SENT,
	STATE_RECEIVING,
	STATE_RECEIVED,
	STATE_FAILED
};

struct bt_state
{
	err_t last_err;

	syswd_t timer_svc;
	lwpq_t hci_cmdq;
	u8_t hci_cmddone;
	u8_t hci_inited;

	u8_t num_maxdevs;
	u8_t num_founddevs;
	struct inquiry_info_ex *info;

	btecallback cb;
	void *usrdata;
};

struct ctrl_req_t
{
	u8 err;
	struct pbuf *p;
	struct bte_pcb *pcb;
	enum bte_state state;
	s32 (*sent)(void *arg,struct bte_pcb *pcb,u8 err);

	struct ctrl_req_t *next;
};

static struct bt_state btstate;
static u8_t bte_patch0[184] = {
	0x70,0x99,0x08,0x00,0x88,0x43,0xd1,0x07,0x09,0x0c,0x08,0x43,0xa0,0x62,0x19,0x23,
	0xdb,0x01,0x33,0x80,0x7c,0xf7,0x88,0xf8,0x28,0x76,0x80,0xf7,0x17,0xff,0x43,0x78,
	0xeb,0x70,0x19,0x23,0xdb,0x01,0x33,0x87,0x7c,0xf7,0xbc,0xfb,0x0b,0x60,0xa3,0x7b,
	0x01,0x49,0x0b,0x60,0x90,0xf7,0x96,0xfb,0xd8,0x1d,0x08,0x00,0x00,0xf0,0x04,0xf8,
	0x00,0x23,0x79,0xf7,0xe3,0xfa,0x00,0x00,0x00,0xb5,0x00,0x23,0x11,0x49,0x0b,0x60,
	0x1d,0x21,0xc9,0x03,0x0b,0x60,0x7d,0x20,0x80,0x01,0x01,0x38,0xfd,0xd1,0x0e,0x4b,
	0x0e,0x4a,0x13,0x60,0x47,0x20,0x00,0x21,0x96,0xf7,0x96,0xff,0x46,0x20,0x00,0x21,
	0x96,0xf7,0x92,0xff,0x0a,0x4a,0x13,0x68,0x0a,0x48,0x03,0x40,0x13,0x60,0x0a,0x4a,
	0x13,0x68,0x0a,0x48,0x03,0x40,0x13,0x60,0x09,0x4a,0x13,0x68,0x09,0x48,0x03,0x40,
	0x13,0x60,0x00,0xbd,0x24,0x80,0x0e,0x00,0x81,0x03,0x0f,0xfe,0x5c,0x00,0x0f,0x00,
	0x60,0xfc,0x0e,0x00,0xfe,0xff,0x00,0x00,0xfc,0xfc,0x0e,0x00,0xff,0x9f,0x00,0x00,
	0x30,0xfc,0x0e,0x00,0x7f,0xff,0x00,0x00
};
static u8_t bte_patch1[92] = {
	0x07,0x20,0xbc,0x65,0x01,0x00,0x84,0x42,0x09,0xd2,0x84,0x42,0x09,0xd1,0x21,0x84,
	0x5a,0x00,0x00,0x83,0xf0,0x74,0xff,0x09,0x0c,0x08,0x43,0x22,0x00,0x61,0x00,0x00,
	0x83,0xf0,0x40,0xfc,0x00,0x00,0x00,0x00,0x23,0xcc,0x9f,0x01,0x00,0x6f,0xf0,0xe4,
	0xfc,0x03,0x28,0x7d,0xd1,0x24,0x3c,0x62,0x01,0x00,0x28,0x20,0x00,0xe0,0x60,0x8d,
	0x23,0x68,0x25,0x04,0x12,0x01,0x00,0x20,0x1c,0x20,0x1c,0x24,0xe0,0xb0,0x21,0x26,
	0x74,0x2f,0x00,0x00,0x86,0xf0,0x18,0xfd,0x21,0x4f,0x3b,0x60
};

static u8 ppc_stack[STACKSIZE] ATTRIBUTE_ALIGN(8);

err_t acl_wlp_completed(void *arg,struct bd_addr *bdaddr);
err_t link_key_not(void *arg,struct bd_addr *bdaddr,u8_t *key);
err_t pin_req(void *arg,struct bd_addr *bdaddr);
err_t l2cap_connected(void *arg,struct l2cap_pcb *l2cappcb,u16_t result,u16_t status);
err_t l2cap_accepted(void *arg,struct l2cap_pcb *l2cappcb,err_t err);
err_t acl_conn_complete(void *arg,struct bd_addr *bdaddr);
err_t l2cap_disconnect_cfm(void *arg, struct l2cap_pcb *pcb);
err_t l2cap_disconnected_ind(void *arg, struct l2cap_pcb *pcb, err_t err);

err_t bte_input(void *arg,struct l2cap_pcb *pcb,struct pbuf *p,err_t err);
err_t bte_callback(void (*f)(void*),void *ctx);
err_t bte_hci_apply_patch_complete(void *arg,struct hci_pcb *pcb,u8_t ogf,u8_t ocf,u8_t result);
err_t bte_hci_patch_complete(void *arg,struct hci_pcb *pcb,u8_t ogf,u8_t ocf,u8_t result);
err_t bte_hci_initcore_complete(void *arg,struct hci_pcb *pcb,u8_t ogf,u8_t ocf,u8_t result);
err_t bte_hci_initsub_complete(void *arg,struct hci_pcb *pcb,u8_t ogf,u8_t ocf,u8_t result);
err_t bte_inquiry_complete(void *arg,struct hci_pcb *pcb,struct hci_inq_res *ires,u16_t result);
err_t bte_read_stored_link_key_complete(void *arg,struct hci_pcb *pcb,u8_t ogf,u8_t ocf,u8_t result);
err_t bte_read_bd_addr_complete(void *arg,struct hci_pcb *pcb,u8_t ogf,u8_t ocf,u8_t result);

MEMB(bte_pcbs,sizeof(struct bte_pcb),MEMP_NUM_BTE_PCB);
MEMB(bte_ctrl_reqs,sizeof(struct ctrl_req_t),MEMP_NUM_BTE_CTRLS);

static void bte_reset_all()
{
	btmemb_init(&bte_pcbs);
	btmemb_init(&bte_ctrl_reqs);

	if(btstate.info!=NULL) free(btstate.info);

	btstate.info = NULL;
	btstate.hci_inited = 0;
	btstate.hci_cmddone = 0;
	btstate.num_founddevs = 0;
	btstate.last_err = ERR_OK;
}

static void bt_alarmhandler(syswd_t alarm,void *cbarg)
{
	__lwp_thread_dispatchdisable();
	SYS_SwitchFiber(0,0,0,0,(u32)l2cap_tmr,(u32)(&ppc_stack[STACKSIZE]));
	__lwp_thread_dispatchunnest();
}

static inline s32 __bte_waitcmdfinish(struct bt_state *state)
{
	u32 level;
	s32 ret;

	if(!state) return ERR_VAL;

	_CPU_ISR_Disable(level);
	while(!state->hci_cmddone)
		LWP_ThreadSleep(state->hci_cmdq);
	ret = state->last_err;
	_CPU_ISR_Restore(level);

	return ret;
}

static inline s32 __bte_cmdfinish(struct bt_state *state,err_t err)
{
	u32 level;

	if(!state) return ERR_VAL;

	_CPU_ISR_Disable(level);
	state->last_err = err;
	state->hci_cmddone = 1;
	if(state->cb!=NULL)
		state->cb(err,state->usrdata);
	else
		LWP_ThreadSignal(state->hci_cmdq);
	_CPU_ISR_Restore(level);

	return err;
}

static inline s32 __bte_waitrequest(struct ctrl_req_t *req)
{
	s32 err;
	u32 level;

	if(!req || !req->pcb) return ERR_VAL;

	_CPU_ISR_Disable(level);
	while(req->state!=STATE_SENT
		&& req->state!=STATE_FAILED)
	{
		LWP_ThreadSleep(req->pcb->cmdq);
	}
	err = req->err;
	_CPU_ISR_Restore(level);

	return err;
}

static inline void __bte_close_ctrl_queue(struct bte_pcb *pcb)
{
	struct ctrl_req_t *req;

	while(pcb->ctrl_req_head!=NULL) {
		req = pcb->ctrl_req_head;
		req->err = ERR_CLSD;
		req->state = STATE_DISCONNECTED;
		if(req->sent!=NULL) {
			req->sent(pcb->cbarg,pcb,ERR_CLSD);
			btmemb_free(&bte_ctrl_reqs,req);
		} else
			LWP_ThreadSignal(pcb->cmdq);

		pcb->ctrl_req_head = req->next;
	}
	pcb->ctrl_req_tail = NULL;
}

static s32 __bte_send_pending_request(struct bte_pcb *pcb)
{
	s32 err;
	struct ctrl_req_t *req;

	if(pcb->ctrl_req_head==NULL) return ERR_OK;
	if(pcb->state==STATE_DISCONNECTING || pcb->state==STATE_DISCONNECTED) return ERR_CLSD;

	req = pcb->ctrl_req_head;
	req->state = STATE_SENDING;

	err = l2ca_datawrite(pcb->ctl_pcb,req->p);
	btpbuf_free(req->p);

	if(err!=ERR_OK) {
		pcb->ctrl_req_head = req->next;

		req->err = err;
		req->state = STATE_FAILED;
		if(req->sent) {
			req->sent(pcb->cbarg,pcb,err);
			btmemb_free(&bte_ctrl_reqs,req);
		} else
			LWP_ThreadSignal(pcb->cmdq);
	}

	return err;
}

static s32 __bte_send_request(struct ctrl_req_t *req)
{
	s32 err;
	u32 level;

	req->next = NULL;
	req->err = ERR_VAL;
	req->state = STATE_READY;

	_CPU_ISR_Disable(level);
	if(req->pcb->ctrl_req_head==NULL) {
		req->pcb->ctrl_req_head = req->pcb->ctrl_req_tail = req;
		err = __bte_send_pending_request(req->pcb);
	} else {
		req->pcb->ctrl_req_tail->next = req;
		req->pcb->ctrl_req_tail = req;
		err = ERR_OK;
	}
	_CPU_ISR_Restore(level);

	return err;
}

static err_t __bte_shutdown_finished(void *arg,struct hci_pcb *pcb,u8_t ogf,u8_t ocf,u8_t result)
{
	err_t err;
	struct bt_state *state = (struct bt_state*)arg;

	if(state==NULL) return ERR_OK;

	state->hci_inited = 0;
	hci_cmd_complete(NULL);
	if(result==HCI_SUCCESS)
		err = ERR_OK;
	else
		err = ERR_CONN;

	physbusif_close();
	return __bte_cmdfinish(state,err);
}

static void bte_process_handshake(struct bte_pcb *pcb,u8_t param,void *buf,u16_t len)
{
	struct ctrl_req_t *req;

	LOG("bte_process_handshake(%p)\n",pcb);

	switch(param) {
		case HIDP_HSHK_SUCCESSFULL:
			req = pcb->ctrl_req_head;
			pcb->ctrl_req_head = req->next;

			req->err = ERR_OK;
			req->state = STATE_SENT;
			if(req->sent) {
				req->sent(pcb->cbarg,pcb,ERR_OK);
				btmemb_free(&bte_ctrl_reqs,req);
			} else
				LWP_ThreadSignal(pcb->cmdq);

			__bte_send_pending_request(pcb);
			break;
		case HIDP_HSHK_NOTREADY:
		case HIDP_HSHK_INV_REPORTID:
		case HIDP_HSHK_NOTSUPPORTED:
		case HIDP_HSHK_IVALIDPARAM:
		case HIDP_HSHK_UNKNOWNERROR:
			break;
		case HIDP_HSHK_FATALERROR:
			break;
		default:
			break;
	}
}

static void bte_process_data(struct bte_pcb *pcb,u8_t param,void *buf,u16_t len)
{
	LOG("bte_process_data(%p)\n",pcb);
	switch(param) {
		case HIDP_DATA_RTYPE_INPUT:
			if(pcb->recv!=NULL) pcb->recv(pcb->cbarg,buf,len);
			break;
		case HIDP_DATA_RTYPE_OTHER:
		case HIDP_DATA_RTYPE_OUPUT:
		case HIDP_DATA_RTYPE_FEATURE:
			break;
		default:
			break;
	}
}

static err_t bte_process_input(void *arg,struct l2cap_pcb *pcb,struct pbuf *p,err_t err)
{
	u8 *buf;
	u16 len;
	u8 hdr,type,param;
	struct bte_pcb *bte = (struct bte_pcb*)arg;

	LOG("bte_process_input(%p,%p)\n",bte,p);

	if(bte->state==STATE_DISCONNECTING
		|| bte->state==STATE_DISCONNECTED) return ERR_CLSD;

	buf = p->payload;
	len = p->tot_len;

	len--;
	hdr = *buf++;
	type = (hdr&HIDP_HDR_TRANS_MASK);
	param = (hdr&HIDP_HDR_PARAM_MASK);
	switch(type) {
		case HIDP_TRANS_HANDSHAKE:
			bte_process_handshake(bte,param,buf,len);
			break;
		case HIDP_TRANS_HIDCONTROL:
			break;
		case HIDP_TRANS_DATA:
			bte_process_data(bte,param,buf,len);
			break;
		default:
			break;
	}
	return ERR_OK;
}

void BTE_Init()
{
	u32 level;
	struct timespec tb;

	LOG("BTE_Init()\n");

	memset(&btstate,0,sizeof(struct bt_state));

	hci_init();
	l2cap_init();
	physbusif_init();

	LWP_InitQueue(&btstate.hci_cmdq);
	SYS_CreateAlarm(&btstate.timer_svc);

	_CPU_ISR_Disable(level);
	bte_reset_all();
	hci_reset_all();
	l2cap_reset_all();
	physbusif_reset_all();

	hci_wlp_complete(acl_wlp_completed);
	hci_connection_complete(acl_conn_complete);
	_CPU_ISR_Restore(level);

	tb.tv_sec = 1;
	tb.tv_nsec = 0;
	SYS_SetPeriodicAlarm(btstate.timer_svc,&tb,&tb,bt_alarmhandler, NULL);
}

void BTE_Shutdown()
{
	u32 level;

	if(btstate.hci_inited==0) return;

	LOG("BTE_Shutdown()\n");

	_CPU_ISR_Disable(level);
	SYS_RemoveAlarm(btstate.timer_svc);
	btstate.cb = NULL;
	btstate.usrdata = NULL;
	btstate.hci_cmddone = 0;
	hci_arg(&btstate);
	hci_cmd_complete(__bte_shutdown_finished);
	hci_reset();
	__bte_waitcmdfinish(&btstate);
	_CPU_ISR_Restore(level);

	physbusif_shutdown();
}

s32 BTE_InitCore(btecallback cb)
{
	u32 level;

	_CPU_ISR_Disable(level);
	btstate.cb = cb;
	btstate.usrdata = NULL;
	btstate.hci_cmddone = 0;
	hci_arg(&btstate);
	hci_cmd_complete(bte_hci_initcore_complete);
	hci_reset();
	_CPU_ISR_Restore(level);

	return ERR_OK;
}

s32 BTE_ApplyPatch(btecallback cb)
{
	u32 level;
	u8 kick = 0;

	_CPU_ISR_Disable(level);
	btstate.cb = cb;
	btstate.usrdata = NULL;
	btstate.hci_cmddone = 0;
	hci_arg(&btstate);
	hci_cmd_complete(bte_hci_apply_patch_complete);
	hci_vendor_specific_command(HCI_VENDOR_PATCH_START_OCF,HCI_VENDOR_OGF,&kick,1);
	_CPU_ISR_Restore(level);

	return ERR_OK;
}

s32 BTE_InitSub(btecallback cb)
{
	u32 level;

	_CPU_ISR_Disable(level);
	btstate.cb = cb;
	btstate.usrdata = NULL;
	btstate.hci_cmddone = 0;
	hci_arg(&btstate);
	hci_cmd_complete(bte_hci_initsub_complete);
	hci_write_inquiry_mode(0x01);
	_CPU_ISR_Restore(level);

	return ERR_OK;
}

s32 BTE_ReadStoredLinkKey(struct linkkey_info *keys,u8 max_cnt,btecallback cb)
{
	u32 level;

	_CPU_ISR_Disable(level);
	btstate.cb = cb;
	btstate.usrdata = keys;
	btstate.num_maxdevs = max_cnt;
	btstate.hci_cmddone = 0;
	hci_arg(&btstate);
	hci_cmd_complete(bte_read_stored_link_key_complete);
	hci_read_stored_link_key();
	_CPU_ISR_Restore(level);

	return ERR_OK;
}

s32 BTE_ReadBdAddr(struct bd_addr *bdaddr, btecallback cb)
{
    u32 level;

    _CPU_ISR_Disable(level);
    btstate.cb = cb;
    btstate.usrdata = bdaddr;
    btstate.hci_cmddone = 0;
    hci_arg(&btstate);
    hci_cmd_complete(bte_read_bd_addr_complete);
    hci_read_bd_addr();
    _CPU_ISR_Restore(level);

    return ERR_OK;
}

void (*BTE_SetDisconnectCallback(void (*callback)(struct bd_addr *bdaddr,u8 reason)))(struct bd_addr *bdaddr,u8 reason)
{
	return l2cap_disconnect_bb(callback);
}

struct bte_pcb* bte_new()
{
	struct bte_pcb *pcb;

	if((pcb=btmemb_alloc(&bte_pcbs))==NULL) return NULL;

	memset(pcb,0,sizeof(struct bte_pcb));

	pcb->state = (u32)STATE_NOTREADY;
	LWP_InitQueue(&(pcb->cmdq));

	return pcb;
}

s32 bte_registerdeviceasync(struct bte_pcb *pcb,struct bd_addr *bdaddr,s32 (*conn_cfm)(void *arg,struct bte_pcb *pcb,u8 err))
{
	u32 level;
	s32 err = ERR_OK;
	struct l2cap_pcb *l2capcb = NULL;

	//printf("bte_registerdeviceasync()\n");
	_CPU_ISR_Disable(level);
	pcb->err = ERR_USE;
	pcb->data_pcb = NULL;
	pcb->ctl_pcb = NULL;
	pcb->conn_cfm = conn_cfm;
	pcb->state = (u32)STATE_CONNECTING;

	bd_addr_set(&(pcb->bdaddr),bdaddr);
	if((l2capcb=l2cap_new())==NULL) {
		err = ERR_MEM;
		goto error;
	}
	l2cap_arg(l2capcb,pcb);

	err = l2cap_connect_ind(l2capcb,bdaddr,HIDP_CONTROL_CHANNEL,l2cap_accepted);
	if(err!=ERR_OK) {
		l2cap_close(l2capcb);
		err = ERR_CONN;
		goto error;
	}

	if((l2capcb=l2cap_new())==NULL) {
		err = ERR_MEM;
		goto error;
	}
	l2cap_arg(l2capcb,pcb);

	err = l2cap_connect_ind(l2capcb,bdaddr,HIDP_DATA_CHANNEL,l2cap_accepted);
	if(err!=ERR_OK) {
		l2cap_close(l2capcb);
		err = ERR_CONN;
	}

error:
	_CPU_ISR_Restore(level);
	//printf("bte_registerdeviceasync(%02x)\n",err);
	return err;
}

s32 bte_inquiry(struct inquiry_info *info,u8 max_cnt,u8 flush)
{
	s32_t i;
	u32 level,fnd;
	err_t last_err;
	struct inquiry_info_ex *pinfo;

	last_err = ERR_OK;

	_CPU_ISR_Disable(level);
	if(btstate.num_founddevs==0 || flush==1) {
		btstate.hci_cmddone = 0;
		btstate.num_maxdevs = max_cnt;
		hci_inquiry(0x009E8B33,0x03,max_cnt,bte_inquiry_complete);
		last_err = __bte_waitcmdfinish(&btstate);
	}
	fnd = btstate.num_founddevs;
	pinfo = btstate.info;
	_CPU_ISR_Restore(level);

	if(last_err==ERR_OK) {
		for(i=0;i<fnd && i<max_cnt;i++) {
			bd_addr_set(&(info[i].bdaddr),&(pinfo[i].bdaddr));
			memcpy(info[i].cod,pinfo[i].cod,3);
		}
	}
	return (s32)((last_err==ERR_OK) ? fnd : last_err);
}

s32 bte_inquiry_ex(struct inquiry_info_ex *info,u8 max_cnt,u8 flush)
{
	s32_t i;
	u32 level,fnd;
	err_t last_err;
	struct inquiry_info_ex *pinfo;

	last_err = ERR_OK;

	_CPU_ISR_Disable(level);
	if(btstate.num_founddevs==0 || flush==1) {
		btstate.hci_cmddone = 0;
		btstate.num_maxdevs = max_cnt;
		hci_inquiry(0x009E8B33,0x03,max_cnt,bte_inquiry_complete);
		last_err = __bte_waitcmdfinish(&btstate);
	}
	fnd = btstate.num_founddevs;
	pinfo = btstate.info;
	_CPU_ISR_Restore(level);

	if(last_err==ERR_OK) {
		for(i=0;i<fnd && i<max_cnt;i++) {
			memcpy(info[i].cod,pinfo[i].cod,3);
			bd_addr_set(&(info[i].bdaddr),&(pinfo[i].bdaddr));
			info[i].psrm = pinfo[i].psrm;
			info[i].psm = pinfo[i].psm;
			info[i].co = pinfo[i].co;
		}
	}
	return (s32)((last_err==ERR_OK) ? fnd : last_err);
}

s32 bte_disconnect(struct bte_pcb *pcb)
{
	u32 level;
	err_t err = ERR_OK;

	if(pcb==NULL) return ERR_VAL;

	_CPU_ISR_Disable(level);
	pcb->state = (u32)STATE_DISCONNECTING;
	if(pcb->data_pcb!=NULL )
		err = l2ca_disconnect_req(pcb->data_pcb,l2cap_disconnect_cfm);
	else if(pcb->ctl_pcb!=NULL)
		err = l2ca_disconnect_req(pcb->ctl_pcb,l2cap_disconnect_cfm);
	_CPU_ISR_Restore(level);

	return err;
}

/*
s32 bte_connect(struct bte_pcb *pcb,struct bd_addr *bdaddr,u8 psm,s32 (*recv)(void *arg,void *buffer,u16 len))
{
	u32 level;
	err_t err = ERR_OK;

	if(pcb==NULL) return ERR_VAL;

	if((pcb->l2capcb=l2cap_new())==NULL) return ERR_MEM;

	pcb->psm = psm;
	pcb->recv = recv;
	bd_addr_set(&(pcb->bdaddr),bdaddr);

	_CPU_ISR_Disable(level);
	pcb->err = ERR_CONN;
	l2cap_arg(pcb->l2capcb,pcb);
	err = l2ca_connect_req(pcb->l2capcb,bdaddr,psm,HCI_ALLOW_ROLE_SWITCH,l2cap_connected);
	if(err==ERR_OK) {
		LWP_ThreadSleep(pcb->cmdq);
		err = pcb->err;
	}
	_CPU_ISR_Restore(level);

	return err;
}

s32 bte_connect_ex(struct bte_pcb *pcb,struct inquiry_info_ex *info,u8 psm,s32 (*recv)(void *arg,void *buffer,u16 len))
{
	err_t err;

	if((err=hci_reg_dev_info(&(info->bdaddr),info->cod,info->psrm,info->psm,info->co))!=ERR_OK) return err;
	return bte_connect(pcb,&(info->bdaddr),psm,recv);
}

s32 bte_listen(struct bte_pcb *pcb,struct bd_addr *bdaddr,u8 psm)
{
	s32 err;
	u32 level;
	struct l2cap_pcb *l2capcb = NULL;

	if(pcb==NULL) return ERR_VAL;

	if((l2capcb=l2cap_new())==NULL) return ERR_MEM;
	pcb->l2capcb = NULL;

	pcb->psm = psm;
	pcb->recv = NULL;
	bd_addr_set(&(pcb->bdaddr),bdaddr);

	_CPU_ISR_Disable(level);
	pcb->err = ERR_CONN;
	l2cap_arg(l2capcb,pcb);
	err = l2cap_connect_ind(l2capcb,psm,l2cap_accepted);
	if(err!=ERR_OK) l2cap_close(l2capcb);

	_CPU_ISR_Restore(level);
	return err;
}

s32 bte_accept(struct bte_pcb *pcb,s32 (*recv)(void *arg,void *buffer,u16 len))
{
	u32 level;
	err_t err = ERR_OK;

	if(pcb==NULL) return ERR_VAL;

	_CPU_ISR_Disable(level);
	pcb->recv = recv;
	while(pcb->l2capcb==NULL)
		LWP_ThreadSleep(pcb->cmdq);
	err = pcb->err;
	_CPU_ISR_Restore(level);

	return err;
}
*/

s32 bte_senddata(struct bte_pcb *pcb,void *message,u16 len)
{
	err_t err;
	struct pbuf *p;

	if(pcb==NULL || message==NULL || len==0) return ERR_VAL;
	if(pcb->state==STATE_DISCONNECTING || pcb->state==STATE_DISCONNECTED) return ERR_CLSD;

	if((p=btpbuf_alloc(PBUF_RAW,(1 + len),PBUF_RAM))==NULL) {
		ERROR("bte_senddata: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}

	((u8*)p->payload)[0] = (HIDP_TRANS_DATA|HIDP_DATA_RTYPE_OUPUT);
	memcpy(p->payload+1,message,len);

	err = l2ca_datawrite(pcb->data_pcb,p);
	btpbuf_free(p);

	return err;
}

s32 bte_sendmessageasync(struct bte_pcb *pcb,void *message,u16 len,s32 (*sent)(void *arg,struct bte_pcb *pcb,u8 err))
{
	struct pbuf *p;
	struct ctrl_req_t *req;

	//printf("bte_sendmessageasync()\n");

	if(pcb==NULL || message==NULL || len==0) return ERR_VAL;
	if(pcb->state==STATE_DISCONNECTING || pcb->state==STATE_DISCONNECTED) return ERR_CLSD;

	if((req=btmemb_alloc(&bte_ctrl_reqs))==NULL) {
		ERROR("bte_sendmessageasync: Could not allocate memory for request\n");
		return ERR_MEM;
	}

	if((p=btpbuf_alloc(PBUF_RAW,(1 + len),PBUF_RAM))==NULL) {
		ERROR("bte_sendmessageasync: Could not allocate memory for pbuf\n");
		btmemb_free(&bte_ctrl_reqs,req);
		return ERR_MEM;
	}

	((u8*)p->payload)[0] = (HIDP_TRANS_SETREPORT|HIDP_DATA_RTYPE_OUPUT);
	memcpy(p->payload+1,message,len);

	req->p = p;
	req->pcb = pcb;
	req->sent = sent;
	return __bte_send_request(req);
}

s32 bte_sendmessage(struct bte_pcb *pcb,void *message,u16 len)
{
	s32 err = ERR_VAL;
	struct pbuf *p;
	struct ctrl_req_t *req;

	//printf("bte_sendmessage()\n");

	if(pcb==NULL || message==NULL || len==0) return ERR_VAL;
	if(pcb->state==STATE_DISCONNECTING || pcb->state==STATE_DISCONNECTED) return ERR_CLSD;

	if((req=btmemb_alloc(&bte_ctrl_reqs))==NULL) {
		ERROR("bte_sendmessage: Could not allocate memory for request\n");
		return ERR_MEM;
	}

	if((p=btpbuf_alloc(PBUF_RAW,(1 + len),PBUF_RAM))==NULL) {
		ERROR("bte_sendmessage: Could not allocate memory for pbuf\n");
		btmemb_free(&bte_ctrl_reqs,req);
		return ERR_MEM;
	}

	((u8*)p->payload)[0] = (HIDP_TRANS_SETREPORT|HIDP_DATA_RTYPE_OUPUT);
	memcpy(p->payload+1,message,len);

	req->p = p;
	req->pcb = pcb;
	req->sent = NULL;
	err = __bte_send_request(req);
	if(err==ERR_OK) err = __bte_waitrequest(req);

	btmemb_free(&bte_ctrl_reqs,req);
	return err;
}

void bte_arg(struct bte_pcb *pcb,void *arg)
{
	u32 level;
	_CPU_ISR_Disable(level);
	pcb->cbarg = arg;
	_CPU_ISR_Restore(level);
}

void bte_received(struct bte_pcb *pcb, s32 (*recv)(void *arg,void *buffer,u16 len))
{
	u32 level;
	_CPU_ISR_Disable(level);
	pcb->recv = recv;
	_CPU_ISR_Restore(level);
}

void bte_disconnected(struct bte_pcb *pcb,s32 (disconn_cfm)(void *arg,struct bte_pcb *pcb,u8 err))
{
	u32 level;
	_CPU_ISR_Disable(level);
	pcb->disconn_cfm = disconn_cfm;
	_CPU_ISR_Restore(level);
}

err_t acl_wlp_completed(void *arg,struct bd_addr *bdaddr)
{
	//hci_sniff_mode(bdaddr,200,100,10,10);
	return ERR_OK;
}

err_t acl_conn_complete(void *arg,struct bd_addr *bdaddr)
{
	//printf("acl_conn_complete\n");
	//memcpy(&(btstate.acl_bdaddr),bdaddr,6);

	hci_write_link_policy_settings(bdaddr,0x0005);
	return ERR_OK;
}

err_t pin_req(void *arg,struct bd_addr *bdaddr)
{
	//printf("pin_req\n");
	return ERR_OK;
}

err_t l2cap_disconnected_ind(void *arg, struct l2cap_pcb *pcb, err_t err)
{
	struct bte_pcb *bte = (struct bte_pcb*)arg;

	if(bte==NULL) return ERR_OK;

	bte->state = (u32)STATE_DISCONNECTING;
	switch(l2cap_psm(pcb)) {
		case HIDP_CONTROL_CHANNEL:
			l2cap_close(bte->ctl_pcb);
			bte->ctl_pcb = NULL;
			break;
		case HIDP_DATA_CHANNEL:
			l2cap_close(bte->data_pcb);
			bte->data_pcb = NULL;
			break;
	}
	if(bte->data_pcb==NULL && bte->ctl_pcb==NULL) {
		bte->err = ERR_OK;
		bte->state = (u32)STATE_DISCONNECTED;
		__bte_close_ctrl_queue(bte);
		if(bte->disconn_cfm!=NULL) bte->disconn_cfm(bte->cbarg,bte,ERR_OK);
	}
	return ERR_OK;
}

err_t l2cap_disconnect_cfm(void *arg, struct l2cap_pcb *pcb)
{
	struct bte_pcb *bte = (struct bte_pcb*)arg;

	if(bte==NULL) return ERR_OK;

	switch(l2cap_psm(pcb)) {
		case HIDP_CONTROL_CHANNEL:
			l2cap_close(bte->ctl_pcb);
			bte->ctl_pcb = NULL;
			if(bte->data_pcb!=NULL)
				l2ca_disconnect_req(bte->data_pcb,l2cap_disconnect_cfm);
			break;
		case HIDP_DATA_CHANNEL:
			l2cap_close(bte->data_pcb);
			bte->data_pcb = NULL;
			if(bte->ctl_pcb!=NULL)
				l2ca_disconnect_req(bte->ctl_pcb,l2cap_disconnect_cfm);
			break;
	}
	if(bte->data_pcb==NULL && bte->ctl_pcb==NULL) {
		bte->err = ERR_OK;
		bte->state = (u32)STATE_DISCONNECTED;
		__bte_close_ctrl_queue(bte);
		if(bte->disconn_cfm!=NULL) bte->disconn_cfm(bte->cbarg,bte,ERR_OK);

		hci_cmd_complete(NULL);
		hci_disconnect(&(bte->bdaddr),HCI_OTHER_END_TERMINATED_CONN_USER_ENDED);
	}

	return ERR_OK;
}

err_t link_key_not(void *arg,struct bd_addr *bdaddr,u8_t *key)
{
	//printf("link_key_not\n");
	return hci_write_stored_link_key(bdaddr,key);
}

/*
err_t l2cap_connected(void *arg,struct l2cap_pcb *l2cappcb,u16_t result,u16_t status)
{
	struct bte_pcb *btepcb = (struct bte_pcb*)arg;

	printf("l2cap_connected(%02x)\n",result);
	if(result==L2CAP_CONN_SUCCESS) {
		l2cap_recv(l2cappcb,bte_input);
		l2cap_disconnect_ind(l2cappcb,l2cap_disconnected_ind);
		btepcb->err = ERR_OK;
	} else {
		l2cap_close(l2cappcb);
		btepcb->err = ERR_CONN;
	}

	if(btepcb->conn_cfm) btepcb->conn_cfm(btepcb->cbarg,btepcb,btepcb->err);
	LWP_ThreadSignal(btepcb->cmdq);
	return ERR_OK;
}
*/
err_t l2cap_accepted(void *arg,struct l2cap_pcb *l2cappcb,err_t err)
{
	struct bte_pcb *btepcb = (struct bte_pcb*)arg;

	//printf("l2cap_accepted(%02x)\n",err);
	if(err==ERR_OK) {
		l2cap_recv(l2cappcb,bte_process_input);
		l2cap_disconnect_ind(l2cappcb,l2cap_disconnected_ind);
		switch(l2cap_psm(l2cappcb)) {
			case HIDP_CONTROL_CHANNEL:
				btepcb->ctl_pcb = l2cappcb;
				break;
			case HIDP_DATA_CHANNEL:
				btepcb->data_pcb = l2cappcb;
				break;
		}
		if(btepcb->data_pcb && btepcb->ctl_pcb) {
			btepcb->err = ERR_OK;
			btepcb->state = (u32)STATE_CONNECTED;
			if(btepcb->conn_cfm) btepcb->conn_cfm(btepcb->cbarg,btepcb,ERR_OK);
		}
	} else {
		l2cap_close(l2cappcb);
		btepcb->err = ERR_CONN;
		btepcb->conn_cfm(btepcb->cbarg,btepcb,ERR_CONN);
	}

	return ERR_OK;
}

err_t bte_inquiry_complete(void *arg,struct hci_pcb *pcb,struct hci_inq_res *ires,u16_t result)
{
	u8_t i;
	struct hci_inq_res *p;
	struct bt_state *state = (struct bt_state*)arg;

	if(result==HCI_SUCCESS) {
		if(ires!=NULL) {

			if(btstate.info!=NULL) free(btstate.info);
			btstate.info = NULL;
			btstate.num_maxdevs = 0;
			btstate.num_founddevs = 0;

			p = ires;
			while(p!=NULL) {
				btstate.num_founddevs++;
				p = p->next;
			}

			p = ires;
			btstate.info = (struct inquiry_info_ex*)malloc(sizeof(struct inquiry_info_ex)*btstate.num_founddevs);
			for(i=0;i<btstate.num_founddevs && p!=NULL;i++) {
				bd_addr_set(&(btstate.info[i].bdaddr),&(p->bdaddr));
				memcpy(btstate.info[i].cod,p->cod,3);
				btstate.info[i].psrm = p->psrm;
				btstate.info[i].psm = p->psm;
				btstate.info[i].co = p->co;

				printf("bdaddr: %02x:%02x:%02x:%02x:%02x:%02x\n",p->bdaddr.addr[0],p->bdaddr.addr[1],p->bdaddr.addr[2],p->bdaddr.addr[3],p->bdaddr.addr[4],p->bdaddr.addr[5]);
				printf("cod:    %02x%02x%02x\n",p->cod[0],p->cod[1],p->cod[2]);
				printf("psrm:   %02x\n",p->psrm);
				printf("psm:   %02x\n",p->psm);
				printf("co:   %04x\n",p->co);
				p = p->next;
			}
			__bte_cmdfinish(state,ERR_OK);
		} else
			hci_inquiry(0x009E8B33,0x03,btstate.num_maxdevs,bte_inquiry_complete);
	}
	return ERR_OK;
}

err_t bte_read_stored_link_key_complete(void *arg,struct hci_pcb *pcb,u8_t ogf,u8_t ocf,u8_t result)
{
	u8_t i = 0;
	struct hci_link_key *p;
	struct linkkey_info *keys;
	struct bt_state *state = (struct bt_state*)arg;

	if(!pcb) return ERR_CONN;

	LOG("bte_read_stored_link_key_complete(%02x,%p)\n",result,pcb->keyres);

	if(state==NULL) return ERR_VAL;
	if(!(ogf==HCI_HC_BB_OGF && ocf==HCI_R_STORED_LINK_KEY_OCF)) return __bte_cmdfinish(state,ERR_CONN);

	if(result==HCI_SUCCESS) {
		keys = (struct linkkey_info*)state->usrdata;
		if(pcb->keyres!=NULL && keys!=NULL) {
			for(i=0,p=pcb->keyres;i<state->num_maxdevs && p!=NULL;i++) {
				bd_addr_set(&(keys[i].bdaddr),&(p->bdaddr));
				memcpy(keys[i].key,p->key,16);

				p = p->next;
			}
		}
		LOG("bte_read_stored_link_key_complete(%02x,%p,%d)\n",result,pcb->keyres,i);
		__bte_cmdfinish(state,i);
		return ERR_OK;
	}

	return __bte_cmdfinish(state,ERR_VAL);
}

err_t bte_read_bd_addr_complete(void *arg,struct hci_pcb *pcb,u8_t ogf,u8_t ocf,u8_t result)
{
    struct bd_addr *bdaddr;
    struct bt_state *state = (struct bt_state*)arg;

    if(!pcb) return ERR_CONN;

    LOG("bte_read_bd_addr_complete(%02x,%p)\n", result, &pcb->bdaddr);

    if(state==NULL) return ERR_VAL;

    if(!(ogf==HCI_INFO_PARAM_OGF && ocf==HCI_R_BD_ADDR_OCF)) return __bte_cmdfinish(state,ERR_CONN);

    if(result == HCI_SUCCESS) {
        bdaddr = (struct bd_addr *)state->usrdata;
        if (bdaddr != NULL) {
            bdaddr->addr[0] = pcb->bdaddr.addr[5];
            bdaddr->addr[1] = pcb->bdaddr.addr[4];
            bdaddr->addr[2] = pcb->bdaddr.addr[3];
            bdaddr->addr[3] = pcb->bdaddr.addr[2];
            bdaddr->addr[4] = pcb->bdaddr.addr[1];
            bdaddr->addr[5] = pcb->bdaddr.addr[0];
        }
        LOG("bte_read_bd_addr_complete(%02x,%p,%d)\n",result,bdaddr,i);
        __bte_cmdfinish(state,ERR_OK);
        return ERR_OK;
    }

    return __bte_cmdfinish(state,ERR_VAL);
}

/* new init with patching */
err_t bte_hci_initcore_complete2(void *arg,struct hci_pcb *pcb,u8_t ogf,u8_t ocf,u8_t result)
{
	err_t err = ERR_OK;
	u8_t dev_cod[] = {0x04, 0x02,0x40};
	struct bt_state *state = (struct bt_state*)arg;

	LOG("bte_hci_initcore_complete2(%02x,%02x)\n",ogf,ocf);
	switch(ogf) {
		case HCI_HC_BB_OGF:
			if(ocf==HCI_WRITE_INQUIRY_MODE) {
				if(result==HCI_SUCCESS) {
					hci_write_page_scan_type(0x01);
				} else
					err = ERR_CONN;
			} else if(ocf==HCI_WRITE_PAGE_SCAN_TYPE) {
				if(result==HCI_SUCCESS) {
					hci_write_inquiry_scan_type(0x01);
				} else
					err = ERR_CONN;
			} else if(ocf==HCI_WRITE_INQUIRY_SCAN_TYPE) {
				if(result==HCI_SUCCESS) {
					hci_write_cod(dev_cod);
				} else
					err = ERR_CONN;
			} else if(ocf==HCI_WRITE_COD) {
				if(result==HCI_SUCCESS) {
					hci_write_page_timeout(0x2000);
				} else
					err = ERR_CONN;
			} else if(ocf==HCI_WRITE_PAGE_TIMEOUT) {
				if(result==HCI_SUCCESS) {
					state->hci_inited = 1;
					hci_cmd_complete(NULL);
					return __bte_cmdfinish(state,ERR_OK);
				} else
					err = ERR_CONN;
			}
			break;
		default:
			LOG("Unknown command complete event. OGF = 0x%x OCF = 0x%x\n", ogf, ocf);
			err = ERR_CONN;
			break;
	}

	if(err!=ERR_OK) __bte_cmdfinish(state,err);
	return err;
}

err_t bte_hci_initcore_complete(void *arg,struct hci_pcb *pcb,u8_t ogf,u8_t ocf,u8_t result)
{
	err_t err = ERR_OK;
	u8_t dev_cod[] = {0x00, 0x1f,0x00};
	struct bt_state *state = (struct bt_state*)arg;

	LOG("bte_hci_initcore_complete(%02x,%02x)\n",ogf,ocf);
	switch(ogf) {
		case HCI_INFO_PARAM:
			if(ocf==HCI_READ_BUFFER_SIZE) {
				if(result==HCI_SUCCESS) {
					hci_write_cod(dev_cod);
				} else
					err = ERR_CONN;
			} else if(ocf==HCI_READ_LOCAL_VERSION) {
				if(result==HCI_SUCCESS) {
					hci_read_bd_addr();
				} else
					err = ERR_CONN;
			} else if(ocf==HCI_READ_BD_ADDR) {
				if(result==HCI_SUCCESS) {
					hci_read_local_features();
				} else
					err = ERR_CONN;
			} else if(ocf==HCI_READ_LOCAL_FEATURES) {
				if(result==HCI_SUCCESS) {
					hci_cmd_complete(bte_hci_initcore_complete2);
					hci_write_inquiry_mode(0x01);
				} else
					err = ERR_CONN;
			}
			break;
		case HCI_HC_BB_OGF:
			if(ocf==HCI_RESET) {
				if(result==HCI_SUCCESS) {
					hci_read_buffer_size();
				} else
					err = ERR_CONN;
			} else if(ocf==HCI_WRITE_COD) {
				if(result==HCI_SUCCESS) {
					hci_write_local_name((u8_t*)"",1);
				} else
					err = ERR_CONN;
			} else if(ocf==HCI_WRITE_LOCAL_NAME) {
				if(result==HCI_SUCCESS) {
					hci_write_pin_type(0x00);
				} else
					err = ERR_CONN;
			} else if(ocf==HCI_WRITE_PIN_TYPE) {
				if(result==HCI_SUCCESS) {
					hci_host_buffer_size();
				} else
					err = ERR_CONN;
			} else if(ocf==HCI_HOST_BUF_SIZE) {
				if(result==HCI_SUCCESS) {
					hci_read_local_version();
				} else
					err = ERR_CONN;
			}
			break;
		default:
			LOG("Unknown command complete event. OGF = 0x%x OCF = 0x%x\n", ogf, ocf);
			err = ERR_CONN;
			break;
	}

	if(err!=ERR_OK) __bte_cmdfinish(state,err);
	return err;
}

err_t bte_hci_apply_patch_complete(void *arg,struct hci_pcb *pcb,u8_t ogf,u8_t ocf,u8_t result)
{
	err_t err = ERR_OK;
	struct bt_state *state = (struct bt_state*)arg;

	LOG("bte_hci_apply_patch_complete(%02x,%02x,%02x)\n",ogf,ocf,result);
	switch(ogf) {
		case HCI_VENDOR_OGF:
			if(ocf==HCI_VENDOR_PATCH_START_OCF) {
				if(result==HCI_SUCCESS) {
					err = hci_vendor_specific_command(HCI_VENDOR_PATCH_CONT_OCF,HCI_VENDOR_OGF,bte_patch0,184);
				} else
					err = ERR_CONN;
			} else if(ocf==HCI_VENDOR_PATCH_CONT_OCF) {
				if(result==HCI_SUCCESS) {
					hci_cmd_complete(bte_hci_patch_complete);
					err = hci_vendor_specific_command(HCI_VENDOR_PATCH_END_OCF,HCI_VENDOR_OGF,bte_patch1,92);
				} else
					err = ERR_CONN;
			}
			break;
		default:
			LOG("Unknown command complete event. OGF = 0x%x OCF = 0x%x\n", ogf, ocf);
			err = ERR_CONN;
			break;
	}

	if(err!=ERR_OK) __bte_cmdfinish(state,err);
	return err;
}

err_t bte_hci_patch_complete(void *arg,struct hci_pcb *pcb,u8_t ogf,u8_t ocf,u8_t result)
{
	err_t err = ERR_OK;
	u8_t dev_cod[] = {0x04, 0x02,0x40};
	struct bt_state *state = (struct bt_state*)arg;

	LOG("bte_hci_patch_complete(%02x,%02x,%02x)\n",ogf,ocf,result);
	switch(ogf) {
		case HCI_INFO_PARAM:
			if(ocf==HCI_READ_BUFFER_SIZE) {
				if(result==HCI_SUCCESS) {
					hci_write_cod(dev_cod);
				} else
					err = ERR_CONN;
			} else if(ocf==HCI_READ_LOCAL_VERSION) {
				if(result==HCI_SUCCESS) {
					hci_read_bd_addr();
				} else
					err = ERR_CONN;
			} else if(ocf==HCI_READ_BD_ADDR) {
				if(result==HCI_SUCCESS) {
					hci_read_local_features();
				} else
					err = ERR_CONN;
			} else if(ocf==HCI_READ_LOCAL_FEATURES) {
				if(result==HCI_SUCCESS) {
					hci_cmd_complete(NULL);
					return __bte_cmdfinish(state,ERR_OK);
				} else
					err = ERR_CONN;
			}
			break;
		case HCI_HC_BB_OGF:
			if(ocf==HCI_RESET) {
				if(result==HCI_SUCCESS) {
					hci_read_buffer_size();
				} else
					err = ERR_CONN;
			} else if(ocf==HCI_WRITE_COD) {
				if(result==HCI_SUCCESS) {
					hci_write_local_name((u8_t*)"",1);
				} else
					err = ERR_CONN;
			} else if(ocf==HCI_WRITE_LOCAL_NAME) {
				if(result==HCI_SUCCESS) {
					hci_write_pin_type(0x00);
				} else
					err = ERR_CONN;
			} else if(ocf==HCI_WRITE_PIN_TYPE) {
				if(result==HCI_SUCCESS) {
					hci_host_buffer_size();
				} else
					err = ERR_CONN;
			} else if(ocf==HCI_HOST_BUF_SIZE) {
				if(result==HCI_SUCCESS) {
					hci_read_local_version();
				} else
					err = ERR_CONN;
			}
			break;
		case HCI_VENDOR_OGF:
			if(ocf==HCI_VENDOR_PATCH_END_OCF) {
				if(result==HCI_SUCCESS) {
					err = hci_reset();
				} else
					err = ERR_CONN;
			}
			break;
		default:
			LOG("Unknown command complete event. OGF = 0x%x OCF = 0x%x\n", ogf, ocf);
			err = ERR_CONN;
			break;
	}

	if(err!=ERR_OK) __bte_cmdfinish(state,err);
	return err;
}

err_t bte_hci_initsub_complete(void *arg,struct hci_pcb *pcb,u8_t ogf,u8_t ocf,u8_t result)
{
	err_t err = ERR_OK;
	u8_t dev_cod[] = {0x00, 0x04,0x48};
	struct bt_state *state = (struct bt_state*)arg;

	LOG("bte_hci_initsub_complete(%02x,%02x)\n",ogf,ocf);
	switch(ogf) {
		case HCI_HC_BB_OGF:
			if(ocf==HCI_WRITE_INQUIRY_MODE) {
				if(result==HCI_SUCCESS) {
					hci_write_page_scan_type(0x01);
				} else
					err = ERR_CONN;
			} else if(ocf==HCI_WRITE_PAGE_SCAN_TYPE) {
				if(result==HCI_SUCCESS) {
					hci_write_inquiry_scan_type(0x01);
				} else
					err = ERR_CONN;
			} else if(ocf==HCI_WRITE_INQUIRY_SCAN_TYPE) {
				if(result==HCI_SUCCESS) {
					hci_write_cod(dev_cod);
				} else
					err = ERR_CONN;
			} else if(ocf==HCI_WRITE_COD) {
				if(result==HCI_SUCCESS) {
					hci_write_page_timeout(0x8000);
				} else
					err = ERR_CONN;
			} else if(ocf==HCI_WRITE_PAGE_TIMEOUT) {
				if(result==HCI_SUCCESS) {
					hci_write_local_name((u8_t*)"Wii",4);
				} else
					err = ERR_CONN;
			} else if(ocf==HCI_WRITE_LOCAL_NAME) {
				if(result==HCI_SUCCESS) {
					hci_write_scan_enable(0x02);
				} else
					err = ERR_CONN;
			} else if(ocf==HCI_WRITE_SCAN_ENABLE) {
				if(result==HCI_SUCCESS) {
					hci_cmd_complete(NULL);
					return __bte_cmdfinish(state,ERR_OK);
				} else
					err = ERR_CONN;
			}
			break;
		default:
			LOG("Unknown command complete event. OGF = 0x%x OCF = 0x%x\n", ogf, ocf);
			err = ERR_CONN;
			break;

	}

	if(err!=ERR_OK) __bte_cmdfinish(state,err);
	return err;
}
