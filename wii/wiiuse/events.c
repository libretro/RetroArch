/* This source as presented is a modified version of original wiiuse for use 
 * with RetroArch, and must not be confused with the original software. */

#include <stdio.h>

#ifndef WIN32
	#include <sys/time.h>
	#include <unistd.h>
	#include <errno.h>
#else
	#include <winsock2.h>
#endif

#include <sys/types.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "dynamics.h"
#include "definitions.h"
#include "wiiuse_internal.h"
#include "events.h"
#include "nunchuk.h"
#include "classic.h"
#include "motion_plus.h"
#include "ir.h"
#include "io.h"


static void event_data_read(struct wiimote_t *wm,ubyte *msg)
{
	ubyte err;
	ubyte len;
	uword offset;
	struct op_t *op;
	struct cmd_blk_t *cmd = wm->cmd_head;

	wiiuse_pressed_buttons(wm,msg);
	
	if(!cmd) return;
	if(!(cmd->state==CMD_SENT && cmd->data[0]==WM_CMD_READ_DATA)) return;

	//printf("event_data_read(%p)\n",cmd);

	err = msg[2]&0x0f;
	op = (struct op_t*)cmd->data;
	if(err) {
		wm->cmd_head = cmd->next;

		cmd->state = CMD_DONE;
		if(cmd->cb!=NULL) cmd->cb(wm,op->buffer,(op->readdata.size - op->wait));

		__lwp_queue_append(&wm->cmdq,&cmd->node);
		wiiuse_send_next_command(wm);
		return;
	}

	len = ((msg[2]&0xf0)>>4)+1;
	offset = BIG_ENDIAN_SHORT(*(uword*)(msg+3));
	
	//printf("addr: %08x\noffset: %d\nlen: %d\n",req->addr,offset,len);

	op->readdata.addr = (op->readdata.addr&0xffff);
	op->wait -= len;
	if(op->wait>=op->readdata.size) op->wait = 0;

	memcpy((op->buffer+offset-op->readdata.addr),(msg+5),len);
	if(!op->wait) {
		wm->cmd_head = cmd->next;

		wm->event = WIIUSE_READ_DATA;
		cmd->state = CMD_DONE;
		if(cmd->cb!=NULL) cmd->cb(wm,op->buffer,op->readdata.size);

		__lwp_queue_append(&wm->cmdq,&cmd->node);
		wiiuse_send_next_command(wm);
	}
}

static void event_ack(struct wiimote_t *wm,ubyte *msg)
{
	struct cmd_blk_t *cmd = wm->cmd_head;

	wiiuse_pressed_buttons(wm,msg);

	if(!cmd || cmd->state!=CMD_SENT || cmd->data[0]==WM_CMD_READ_DATA || cmd->data[0]==WM_CMD_CTRL_STATUS || cmd->data[0]!=msg[2] || msg[3]) {
		//WIIUSE_WARNING("Unsolicited event ack: report %02x status %02x", msg[2], msg[3]);
		return;
	}

	//WIIUSE_DEBUG("Received ack for command %02x %02x", cmd->data[0], cmd->data[1]);

	wm->cmd_head = cmd->next;

	wm->event = WIIUSE_ACK;
	cmd->state = CMD_DONE;
	if(cmd->cb) cmd->cb(wm,NULL,0);

	__lwp_queue_append(&wm->cmdq,&cmd->node);
	wiiuse_send_next_command(wm);
}

static void event_status(struct wiimote_t *wm,ubyte *msg)
{
	int ir = 0;
	int attachment = 0;
#ifdef HAVE_WIIUSE_SPEAKER
	int speaker = 0;
#endif
	//int led[4]= {0};
	struct cmd_blk_t *cmd = wm->cmd_head;

	wiiuse_pressed_buttons(wm,msg);

	wm->event = WIIUSE_STATUS;
	//if(msg[2]&WM_CTRL_STATUS_BYTE1_LED_1) led[0] = 1;
	//if(msg[2]&WM_CTRL_STATUS_BYTE1_LED_2) led[1] = 1;
	//if(msg[2]&WM_CTRL_STATUS_BYTE1_LED_3) led[2] = 1;
	//if(msg[2]&WM_CTRL_STATUS_BYTE1_LED_4) led[3] = 1;

	if((msg[2]&WM_CTRL_STATUS_BYTE1_ATTACHMENT)==WM_CTRL_STATUS_BYTE1_ATTACHMENT) attachment = 1;
#ifdef HAVE_WIIUSE_SPEAKER
	if((msg[2]&WM_CTRL_STATUS_BYTE1_SPEAKER_ENABLED)==WM_CTRL_STATUS_BYTE1_SPEAKER_ENABLED) speaker = 1;
#endif
	if((msg[2]&WM_CTRL_STATUS_BYTE1_IR_ENABLED)==WM_CTRL_STATUS_BYTE1_IR_ENABLED) ir = 1;

	wm->battery_level = msg[5];

	if(!ir && WIIMOTE_IS_SET(wm,WIIMOTE_STATE_IR_INIT)) {
		WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_IR_INIT);
		wiiuse_set_ir(wm, 1);
		goto done;
	}
	if(ir && !WIIMOTE_IS_SET(wm,WIIMOTE_STATE_IR)) WIIMOTE_ENABLE_STATE(wm,WIIMOTE_STATE_IR);
	else if(!ir && WIIMOTE_IS_SET(wm,WIIMOTE_STATE_IR)) WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_IR);

#ifdef HAVE_WIIUSE_SPEAKER
	if(!speaker && WIIMOTE_IS_SET(wm,WIIMOTE_STATE_SPEAKER_INIT)) {
		WIIMOTE_DISABLE_STATE(wm,WIIMOTE_STATE_SPEAKER_INIT);
		wiiuse_set_speaker(wm,1);
		goto done;
	}
	if(speaker && !WIIMOTE_IS_SET(wm,WIIMOTE_STATE_SPEAKER)) WIIMOTE_ENABLE_STATE(wm,WIIMOTE_STATE_SPEAKER);
	else if(!speaker && WIIMOTE_IS_SET(wm,WIIMOTE_STATE_SPEAKER)) WIIMOTE_DISABLE_STATE(wm,WIIMOTE_STATE_SPEAKER);
#endif

	if(attachment) {
		if(!WIIMOTE_IS_SET(wm,WIIMOTE_STATE_EXP) && !WIIMOTE_IS_SET(wm,WIIMOTE_STATE_EXP_FAILED) && !WIIMOTE_IS_SET(wm,WIIMOTE_STATE_EXP_HANDSHAKE)) {
			wiiuse_handshake_expansion_start(wm);
			goto done;
		}
	} else {
		WIIMOTE_DISABLE_STATE(wm,WIIMOTE_STATE_EXP_FAILED);
		if(WIIMOTE_IS_SET(wm,WIIMOTE_STATE_EXP)) {
			wiiuse_disable_expansion(wm);
			goto done;
		}
	}
	wiiuse_set_report_type(wm,NULL);

done:
	if(!cmd) return;
	if(!(cmd->state==CMD_SENT && cmd->data[0]==WM_CMD_CTRL_STATUS)) return;

	wm->cmd_head = cmd->next;

	cmd->state = CMD_DONE;
	if(cmd->cb!=NULL) cmd->cb(wm,msg,6);
	
	__lwp_queue_append(&wm->cmdq,&cmd->node);
	wiiuse_send_next_command(wm);
}

static void handle_expansion(struct wiimote_t *wm,ubyte *msg)
{
	switch (wm->exp.type) {
		case EXP_NUNCHUK:
			nunchuk_event(&wm->exp.nunchuk, msg);
			break;
		case EXP_CLASSIC:
			classic_ctrl_event(&wm->exp.classic, msg);
			break;
 		case EXP_MOTION_PLUS:
 			motion_plus_event(&wm->exp.mp, msg);
 			break;
		default:
			break;
	}
}

/**
 *	@brief Called on a cycle where no significant change occurs.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 */
void idle_cycle(struct wiimote_t* wm) 
{
	/*
	 *	Smooth the angles.
	 *
	 *	This is done to make sure that on every cycle the orientation
	 *	angles are smoothed.  Normally when an event occurs the angles
	 *	are updated and smoothed, but if no packet comes in then the
	 *	angles remain the same.  This means the angle wiiuse reports
	 *	is still an old value.  Smoothing needs to be applied in this
	 *	case in order for the angle it reports to converge to the true
	 *	angle of the device.
	 */
	//printf("idle_cycle()\n");///
	if (WIIUSE_USING_ACC(wm) && WIIMOTE_IS_FLAG_SET(wm, WIIUSE_SMOOTHING)) {
		apply_smoothing(&wm->accel_calib, &wm->orient, SMOOTH_ROLL);
		apply_smoothing(&wm->accel_calib, &wm->orient, SMOOTH_PITCH);
	}
}

void parse_event(struct wiimote_t *wm)
{
	ubyte event;
	ubyte *msg;

	event = wm->event_buf[0];
	msg = wm->event_buf+1;
	//printf("parse_event(%02x,%p)\n",event,msg);
	switch(event) {
		case WM_RPT_CTRL_STATUS:
			event_status(wm,msg);
			return;
		case WM_RPT_READ:
			event_data_read(wm,msg);
			return;
		case WM_RPT_ACK:
			event_ack(wm,msg);
			return;
		case WM_RPT_BTN:
			wiiuse_pressed_buttons(wm,msg);
			break;
		case WM_RPT_BTN_ACC:
			wiiuse_pressed_buttons(wm,msg);

			wm->accel.x = (msg[2]<<2)|((msg[0]>>5)&3);
			wm->accel.y = (msg[3]<<2)|((msg[1]>>4)&2);
			wm->accel.z = (msg[4]<<2)|((msg[1]>>5)&2);
#ifndef GEKKO
			/* calculate the remote orientation */
			calculate_orientation(&wm->accel_calib, &wm->accel, &wm->orient, WIIMOTE_IS_FLAG_SET(wm, WIIUSE_SMOOTHING));

			/* calculate the gforces on each axis */
			calculate_gforce(&wm->accel_calib, &wm->accel, &wm->gforce);
#endif
			break;
		case WM_RPT_BTN_ACC_IR:
			wiiuse_pressed_buttons(wm,msg);

			wm->accel.x = (msg[2]<<2)|((msg[0]>>5)&3);
			wm->accel.y = (msg[3]<<2)|((msg[1]>>4)&2);
			wm->accel.z = (msg[4]<<2)|((msg[1]>>5)&2);
#ifndef GEKKO
			/* calculate the remote orientation */
			calculate_orientation(&wm->accel_calib, &wm->accel, &wm->orient, WIIMOTE_IS_FLAG_SET(wm, WIIUSE_SMOOTHING));

			/* calculate the gforces on each axis */
			calculate_gforce(&wm->accel_calib, &wm->accel, &wm->gforce);
#endif
			calculate_extended_ir(wm, msg+5);
			break;
		case WM_RPT_BTN_EXP:
			wiiuse_pressed_buttons(wm,msg);
			handle_expansion(wm,msg+2);
			break;
		case WM_RPT_BTN_ACC_EXP:
			/* button - motion - expansion */
			wiiuse_pressed_buttons(wm, msg);

			wm->accel.x = (msg[2]<<2)|((msg[0]>>5)&3);
			wm->accel.y = (msg[3]<<2)|((msg[1]>>4)&2);
			wm->accel.z = (msg[4]<<2)|((msg[1]>>5)&2);
#ifndef GEKKO
			calculate_orientation(&wm->accel_calib, &wm->accel, &wm->orient, WIIMOTE_IS_FLAG_SET(wm, WIIUSE_SMOOTHING));
			calculate_gforce(&wm->accel_calib, &wm->accel, &wm->gforce);
#endif
			handle_expansion(wm, msg+5);
			break;
		case WM_RPT_BTN_IR_EXP:
			wiiuse_pressed_buttons(wm,msg);
			calculate_basic_ir(wm, msg+2);
			handle_expansion(wm,msg+12);
			break;
		case WM_RPT_BTN_ACC_IR_EXP:
			/* button - motion - ir - expansion */
			wiiuse_pressed_buttons(wm, msg);

			wm->accel.x = (msg[2]<<2)|((msg[0]>>5)&3);
			wm->accel.y = (msg[3]<<2)|((msg[1]>>4)&2);
			wm->accel.z = (msg[4]<<2)|((msg[1]>>5)&2);
#ifndef GEKKO
			calculate_orientation(&wm->accel_calib, &wm->accel, &wm->orient, WIIMOTE_IS_FLAG_SET(wm, WIIUSE_SMOOTHING));
			calculate_gforce(&wm->accel_calib, &wm->accel, &wm->gforce);
#endif
			/* ir */
			calculate_basic_ir(wm, msg+5);

			handle_expansion(wm, msg+15);
			break;
		default:
			WIIUSE_WARNING("Unknown event, can not handle it [Code 0x%x].", event);
			return;
	}

	/* was there an event? */
	wm->event = WIIUSE_EVENT;
}

/**
 *	@brief Find what buttons are pressed.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *	@param msg		The message specified in the event packet.
 */
void wiiuse_pressed_buttons(struct wiimote_t* wm, ubyte* msg) {
	short now;

	/* convert to big endian */
	now = BIG_ENDIAN_SHORT(*(short*)msg) & WIIMOTE_BUTTON_ALL;

	/* preserve old btns pressed */
	wm->btns_last = wm->btns;

	/* pressed now & were pressed, then held */
	wm->btns_held = (now & wm->btns);

	/* were pressed or were held & not pressed now, then released */
	wm->btns_released = ((wm->btns | wm->btns_held) & ~now);

	/* buttons pressed now */
	wm->btns = now;
}
