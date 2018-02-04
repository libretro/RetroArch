/* This source as presented is a modified version of original wiiuse for use
 * with RetroArch, and must not be confused with the original software. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WIN32
	#include <unistd.h>
#else
	#include <Winsock2.h>
#endif

#include "definitions.h"
#include "wiiuse_internal.h"
#include "io.h"

static struct wiimote_t** __wm = NULL;

void wiiuse_send_next_command(struct wiimote_t *wm)
{
	struct cmd_blk_t *cmd = wm->cmd_head;

	if(!wm || !WIIMOTE_IS_CONNECTED(wm)) return;

	if(!cmd) return;
	if(cmd->state!=CMD_READY) return;

	cmd->state = CMD_SENT;
#ifdef HAVE_WIIUSE_RUMBLE
	if(WIIMOTE_IS_SET(wm,WIIMOTE_STATE_RUMBLE)) cmd->data[1] |= 0x01;
#endif

	//WIIUSE_DEBUG("Sending command: %02x %02x", cmd->data[0], cmd->data[1]);
	wiiuse_io_write(wm,cmd->data,cmd->len);
}

static inline void __wiiuse_push_command(struct wiimote_t *wm,struct cmd_blk_t *cmd)
{
	uint level;

	if(!wm || !cmd) return;

	cmd->next = NULL;
	cmd->state = CMD_READY;

	_CPU_ISR_Disable(level);
	if(wm->cmd_head==NULL) {
		wm->cmd_head = wm->cmd_tail = cmd;
		wiiuse_send_next_command(wm);
	} else {
		wm->cmd_tail->next = cmd;
		wm->cmd_tail = cmd;
	}
	_CPU_ISR_Restore(level);
}

#ifndef GEKKO
struct wiimote_t** wiiuse_init(int wiimotes) {
#else
extern void __wiiuse_sensorbar_enable(int enable);
struct wiimote_t** wiiuse_init(int wiimotes, wii_event_cb event_cb) {
#endif
	int i = 0;

	if (!wiimotes)
		return NULL;

	if (!__wm) {
		__wm = __lwp_heap_allocate(&__wkspace_heap, sizeof(struct wiimote_t*) * wiimotes);
		if(!__wm) return NULL;
		memset(__wm, 0, sizeof(struct wiimote_t*) * wiimotes);
	}

	for (i = 0; i < wiimotes; ++i) {
		if(!__wm[i])
			__wm[i] = __lwp_heap_allocate(&__wkspace_heap, sizeof(struct wiimote_t));

		memset(__wm[i], 0, sizeof(struct wiimote_t));
		__wm[i]->unid = i;

		#if defined(WIN32)
			__wm[i]->dev_handle = 0;
			__wm[i]->stack = WIIUSE_STACK_UNKNOWN;
			__wm[i]->normal_timeout = WIIMOTE_DEFAULT_TIMEOUT;
			__wm[i]->exp_timeout = WIIMOTE_EXP_TIMEOUT;
			__wm[i]->timeout = __wm[i]->normal_timeout;
		#elif defined(GEKKO)
			__wm[i]->sock = NULL;
			__wm[i]->bdaddr = *BD_ADDR_ANY;
			__wm[i]->event_cb = event_cb;
			wiiuse_init_cmd_queue(__wm[i]);
		#elif defined(unix)
			__wm[i]->bdaddr = *BDADDR_ANY;
			__wm[i]->out_sock = -1;
			__wm[i]->in_sock = -1;
		#endif

		__wm[i]->state = WIIMOTE_INIT_STATES;
		__wm[i]->flags = WIIUSE_INIT_FLAGS;

		__wm[i]->event = WIIUSE_NONE;

		__wm[i]->exp.type = EXP_NONE;

		wiiuse_set_aspect_ratio(__wm[i], WIIUSE_ASPECT_4_3);
		wiiuse_set_ir_position(__wm[i], WIIUSE_IR_ABOVE);

		__wm[i]->accel_calib.st_alpha = WIIUSE_DEFAULT_SMOOTH_ALPHA;
	}

	return __wm;
}

/**
 *	@brief Set flags for the specified wiimote.
 *
 *	@param wm			Pointer to a wiimote_t structure.
 *	@param enable		Flags to enable.
 *	@param disable		Flags to disable.
 *
 *	@return The flags set after 'enable' and 'disable' have been applied.
 *
 *	The values 'enable' and 'disable' may be any flags OR'ed together.
 *	Flags are defined in wiiuse.h.
 */
int wiiuse_set_flags(struct wiimote_t* wm, int enable, int disable) {
	if (!wm)	return 0;

	/* remove mutually exclusive flags */
	enable &= ~disable;
	disable &= ~enable;

	wm->flags |= enable;
	wm->flags &= ~disable;

	return wm->flags;
}

/**
 *	@brief	Set if the wiimote should report motion sensing.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *	@param status	1 to enable, 0 to disable.
 *
 *	Since reporting motion sensing sends a lot of data,
 *	the wiimote saves power by not transmitting it
 *	by default.
 */
void wiiuse_motion_sensing(struct wiimote_t* wm, int status)
{
	if (status) {
		if(WIIMOTE_IS_SET(wm,WIIMOTE_STATE_ACC)) return;
		WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_ACC);
	} else {
		if(!WIIMOTE_IS_SET(wm,WIIMOTE_STATE_ACC)) return;
		WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_ACC);
	}

	if(!WIIMOTE_IS_SET(wm,WIIMOTE_STATE_HANDSHAKE_COMPLETE)) return;

	wiiuse_status(wm,NULL);
}

/**
 *	@brief	Toggle the state of the rumble.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 */
#ifdef HAVE_WIIUSE_RUMBLE
void wiiuse_toggle_rumble(struct wiimote_t* wm)
{
	if (!wm) return;

	WIIMOTE_TOGGLE_STATE(wm, WIIMOTE_STATE_RUMBLE);
	if(!WIIMOTE_IS_SET(wm,WIIMOTE_STATE_HANDSHAKE_COMPLETE)) return;

	wiiuse_set_leds(wm,wm->leds,NULL);
}

/**
 *	@brief	Enable or disable the rumble.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *	@param status	1 to enable, 0 to disable.
 */
void wiiuse_rumble(struct wiimote_t* wm, int status)
{
	if (status && WIIMOTE_IS_SET(wm,WIIMOTE_STATE_RUMBLE)) return;
	else if(!status && !WIIMOTE_IS_SET(wm,WIIMOTE_STATE_RUMBLE)) return;
	wiiuse_toggle_rumble(wm);
}
#endif

void wiiuse_set_leds(struct wiimote_t *wm,int leds,cmd_blk_cb cb)
{
	ubyte buf;

	if(!wm || !WIIMOTE_IS_CONNECTED(wm)) return;

	wm->leds = (leds&0xf0);

	buf = wm->leds;
	wiiuse_sendcmd(wm,WM_CMD_LED,&buf,1,cb);
}

int wiiuse_set_report_type(struct wiimote_t *wm,cmd_blk_cb cb)
{
	ubyte buf[2];
	int motion,ir,exp;

	if(!wm || !WIIMOTE_IS_CONNECTED(wm)) return 0;

	buf[0] = (WIIMOTE_IS_FLAG_SET(wm, WIIUSE_CONTINUOUS) ? 0x04 : 0x00);	/* set to 0x04 for continuous reporting */
	buf[1] = 0x00;

	motion = WIIMOTE_IS_SET(wm, WIIMOTE_STATE_ACC) || WIIMOTE_IS_SET(wm, WIIMOTE_STATE_IR);
	exp = WIIMOTE_IS_SET(wm, WIIMOTE_STATE_EXP);
	ir = WIIMOTE_IS_SET(wm, WIIMOTE_STATE_IR);

	if (motion && ir && exp)	buf[1] = WM_RPT_BTN_ACC_IR_EXP;
	else if (motion && exp)		buf[1] = WM_RPT_BTN_ACC_EXP;
	else if (motion && ir)		buf[1] = WM_RPT_BTN_ACC_IR;
	else if (ir && exp)			buf[1] = WM_RPT_BTN_IR_EXP;
	else if (ir)				buf[1] = WM_RPT_BTN_ACC_IR;
	else if (exp)				buf[1] = WM_RPT_BTN_EXP;
	else if (motion)			buf[1] = WM_RPT_BTN_ACC;
	else						buf[1] = WM_RPT_BTN;

	//WIIUSE_DEBUG("Setting report type: 0x%x", buf[1]);

	wiiuse_sendcmd(wm,WM_CMD_REPORT_TYPE,buf,2,cb);
	return buf[1];
}

void wiiuse_status(struct wiimote_t *wm,cmd_blk_cb cb)
{
	ubyte buf;

	if(!wm || !WIIMOTE_IS_CONNECTED(wm)) return;

	buf = 0x00;
	wiiuse_sendcmd(wm,WM_CMD_CTRL_STATUS,&buf,1,cb);
}

int wiiuse_read_data(struct wiimote_t *wm,ubyte *buffer,uint addr,uword len,cmd_blk_cb cb)
{
	struct op_t *op;
	struct cmd_blk_t *cmd;

	if(!wm || !WIIMOTE_IS_CONNECTED(wm)) return 0;
	if(!buffer || !len) return 0;

	cmd = (struct cmd_blk_t*)__lwp_queue_get(&wm->cmdq);
	if(!cmd) return 0;

	cmd->cb = cb;
	cmd->len = 7;

	op = (struct op_t*)cmd->data;
	op->cmd = WM_CMD_READ_DATA;
	op->buffer = buffer;
	op->wait = len;
	op->readdata.addr = BIG_ENDIAN_LONG(addr);
	op->readdata.size = BIG_ENDIAN_SHORT(len);
	__wiiuse_push_command(wm,cmd);

	return 1;
}

int wiiuse_write_data(struct wiimote_t *wm,uint addr,ubyte *data,ubyte len,cmd_blk_cb cb)
{
	struct op_t *op;
	struct cmd_blk_t *cmd;

	if(!wm || !WIIMOTE_IS_CONNECTED(wm)) return 0;
	if(!data || !len) return 0;

	cmd = (struct cmd_blk_t*)__lwp_queue_get(&wm->cmdq);
	if(!cmd) return 0;

	cmd->cb = cb;
	cmd->len = 22;

	op = (struct op_t*)cmd->data;
	op->cmd = WM_CMD_WRITE_DATA;
	op->buffer = NULL;
	op->wait = 0;
	op->writedata.addr = BIG_ENDIAN_LONG(addr);
	op->writedata.size = (len&0x0f);
	memcpy(op->writedata.data,data,len);
	memset(op->writedata.data+len,0,(16 - len));
	__wiiuse_push_command(wm,cmd);

	return 1;
}

int wiiuse_write_streamdata(struct wiimote_t *wm,ubyte *data,ubyte len,cmd_blk_cb cb)
{
	struct cmd_blk_t *cmd;

	if(!wm || !WIIMOTE_IS_CONNECTED(wm)) return 0;
	if(!data || !len || len>20) return 0;

	cmd = (struct cmd_blk_t*)__lwp_queue_get(&wm->cmdq);
	if(!cmd) return 0;

	cmd->cb = cb;
	cmd->len = 22;
	cmd->data[0] = WM_CMD_STREAM_DATA;
	cmd->data[1] = (len<<3);
	memcpy(cmd->data+2,data,len);
	__wiiuse_push_command(wm,cmd);

	return 1;
}

int wiiuse_sendcmd(struct wiimote_t *wm,ubyte report_type,ubyte *msg,int len,cmd_blk_cb cb)
{
	struct cmd_blk_t *cmd;

	cmd = (struct cmd_blk_t*)__lwp_queue_get(&wm->cmdq);
	if(!cmd) return 0;

	cmd->cb = cb;
	cmd->len = (1+len);

	cmd->data[0] = report_type;
	memcpy(cmd->data+1,msg,len);
	if(report_type!=WM_CMD_READ_DATA && report_type!=WM_CMD_CTRL_STATUS)
		cmd->data[1] |= 0x02;

	//WIIUSE_DEBUG("Pushing command: %02x %02x", cmd->data[0], cmd->data[1]);
	__wiiuse_push_command(wm,cmd);

	return 1;
}
