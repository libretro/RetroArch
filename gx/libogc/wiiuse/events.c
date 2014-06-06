#include "wiiuse_internal.h"
#include "events.h"

#include "classic.h"                    /* for classic_ctrl_disconnected, etc */
#include "dynamics.h"                   /* for calculate_gforce, etc */
#include "guitar_hero_3.h"              /* for guitar_hero_3_disconnected, etc */
#include "ir.h"                         /* for calculate_basic_ir, etc */
#include "nunchuk.h"                    /* for nunchuk_disconnected, etc */
#include "wiiboard.h"                   /* for wii_board_disconnected, etc */
#include "motion_plus.h"                /* for motion_plus_disconnected, etc */

#ifdef WIN32
	#include <winsock2.h>
#endif

#include <sys/types.h>
#include <stdio.h>                      /* for printf, perror */
#include <stdlib.h>                     /* for free, malloc */
#include <string.h>                     /* for memcpy, memset */
#include <math.h>

#include "definitions.h"
#include "io.h"


static void event_data_read(struct wiimote_t *wm,ubyte *msg)
{
	ubyte err;
	ubyte len;
	uint16_t offset;
#ifdef NEW_WIIUSE
	struct read_req_t* req = wm->read_req;
#else
	struct op_t *op;
	struct cmd_blk_t *cmd = wm->cmd_head;
#endif

	wiiuse_pressed_buttons(wm,msg);
	
#ifdef NEW_WIIUSE
	/* find the next non-dirty request */
	while (req && req->dirty) {
		req = req->next;
	}

	/* if we don't have a request out then we didn't ask for this packet */
	if (!req) {
		WIIUSE_WARNING("Received data packet when no request was made.");
		return;
	}
#else
	if(!cmd) return;
	if(!(cmd->state==CMD_SENT && cmd->data[0]==WM_CMD_READ_DATA)) return;
#endif

	err = msg[2]&0x0f;

#ifdef NEW_WIIUSE
	if (err == 0x08) {
		WIIUSE_WARNING("Unable to read data - address does not exist.");
	} else if (err == 0x07) {
		WIIUSE_WARNING("Unable to read data - address is for write-only registers.");
	} else if (err) {
		WIIUSE_WARNING("Unable to read data - unknown error code %x.", err);
	}

	if (err) {
		/* this request errored out, so skip it and go to the next one */

		/* delete this request */
		wm->read_req = req->next;
		free(req);

		/* if another request exists send it to the wiimote */
		if (wm->read_req) {
			wiiuse_send_next_pending_read_request(wm);
		}

		return;
	}
#else
	op = (struct op_t*)cmd->data;
	if(err) {
		wm->cmd_head = cmd->next;

		cmd->state = CMD_DONE;
		if(cmd->cb!=NULL) cmd->cb(wm,op->buffer,(op->readdata.size - op->wait));

		__lwp_queue_append(&wm->cmdq,&cmd->node);
		wiiuse_send_next_command(wm);
		return;
	}
#endif

	len = ((msg[2]&0xf0)>>4)+1;
#ifdef GEKKO
	offset = BIG_ENDIAN_SHORT(*(uword*)(msg+3));
#else
	offset = from_big_endian_uint16_t(msg + 3);
#endif
#ifdef NEW_WIIUSE
	req->addr = (req->addr & 0xFFFF);

	req->wait -= len;
	if (req->wait >= req->size)
		/* this should never happen */
	{
		req->wait = 0;
	}

	WIIUSE_DEBUG("Received read packet:");
	WIIUSE_DEBUG("    Packet read offset:   %i bytes", offset);
	WIIUSE_DEBUG("    Request read offset:  %i bytes", req->addr);
	WIIUSE_DEBUG("    Read offset into buf: %i bytes", offset - req->addr);
	WIIUSE_DEBUG("    Read data size:       %i bytes", len);
	WIIUSE_DEBUG("    Still need:           %i bytes", req->wait);

	/* reconstruct this part of the data */
	memcpy((req->buf + offset - req->addr), (msg + 5), len);

#ifdef WITH_WIIUSE_DEBUG
	{
		int i = 0;
		printf("Read: ");
		for (; i < req->size - req->wait; ++i) {
			printf("%x ", req->buf[i]);
		}
		printf("\n");
	}
#endif

	/* if all data has been received, execute the read event callback or generate event */
	if (!req->wait) {
		if (req->cb) {
			/* this was a callback, so invoke it now */
			req->cb(wm, req->buf, req->size);

			/* delete this request */
			wm->read_req = req->next;
			free(req);
		} else {
			/*
			 *	This should generate an event.
			 *	We need to leave the event in the array so the client
			 *	can access it still.  We'll flag is as being 'dirty'
			 *	and give the client one cycle to use it.  Next event
			 *	we will remove it from the list.
			 */
			wm->event = WIIUSE_READ_DATA;
			req->dirty = 1;
		}

		/* if another request exists send it to the wiimote */
		if (wm->read_req) {
			wiiuse_send_next_pending_read_request(wm);
		}
	}
#else
	//printf("addr: %08x\noffset: %d\nlen: %d\n",req->addr,offset,len);

	op->readdata.addr = (op->readdata.addr&0xffff);
	op->wait -= len;
	if(op->wait>=op->readdata.size) op->wait = 0;

	/* reconstruct this part of the data */
	memcpy((op->buffer+offset-op->readdata.addr),(msg+5),len);

	/* if all data has been received, execute the read event callback or generate event */
	if(!op->wait) {
		wm->cmd_head = cmd->next;

		wm->event = WIIUSE_READ_DATA;
		cmd->state = CMD_DONE;
		if(cmd->cb!=NULL) {
			/* this was a callback, so invoke it now */
         cmd->cb(wm,op->buffer,op->readdata.size);
      }

		__lwp_queue_append(&wm->cmdq,&cmd->node);

		/* if another request exists send it to the wiimote */
		wiiuse_send_next_command(wm);
	}
#endif
	
}

#ifndef NEW_WIIUSE
static void event_ack(struct wiimote_t *wm,ubyte *msg)
{
	struct cmd_blk_t *cmd = wm->cmd_head;

	wiiuse_pressed_buttons(wm,msg);

	if(!cmd) return;
	if(!cmd || cmd->state!=CMD_SENT || cmd->data[0]==WM_CMD_READ_DATA || cmd->data[0]==WM_CMD_CTRL_STATUS || cmd->data[0]!=msg[2]) {
		WIIUSE_WARNING("Unsolicited event ack: report %02x status %02x", msg[2], msg[3]);
		return;
	}
	if(msg[3]) {
		WIIUSE_WARNING("Command %02x %02x failed: status %02x", cmd->data[0], cmd->data[1], msg[3]);
		return;
	}

	WIIUSE_DEBUG("Received ack for command %02x %02x", cmd->data[0], cmd->data[1]);

	wm->cmd_head = cmd->next;

	wm->event = WIIUSE_ACK;
	cmd->state = CMD_DONE;
	if(cmd->cb) cmd->cb(wm,NULL,0);

	__lwp_queue_append(&wm->cmdq,&cmd->node);
	wiiuse_send_next_command(wm);
}
#endif

/**
 *	@brief Read the controller status.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *	@param msg		The message specified in the event packet.
 *
 *	Read the controller status and execute the registered status callback.
 */
static void event_status(struct wiimote_t *wm,ubyte *msg)
{
	int led[4] = {0, 0, 0, 0};
	int attachment = 0;
	int ir = 0;
	//int exp_changed = 0;
	int speaker = 0;
#if NEW_WIIUSE
	struct data_req_t* req = wm->data_req;
#else
	struct cmd_blk_t *cmd = wm->cmd_head;
#endif
   (void)led;

	/* initial handshake is not finished yet, ignore this */
	if (WIIMOTE_IS_SET(wm, WIIMOTE_STATE_HANDSHAKE) || !msg) {
		return;
	}

	/*
	 *	An event occurred.
	 *	This event can be overwritten by a more specific
	 *	event type during a handshake or expansion removal.
	 */
	wm->event = WIIUSE_STATUS;

	wiiuse_pressed_buttons(wm,msg);

	/* find what LEDs are lit */
	if(msg[2] & WM_CTRL_STATUS_BYTE1_LED_1) {
      led[0] = 1;
   }
	if(msg[2] & WM_CTRL_STATUS_BYTE1_LED_2) {
      led[1] = 1;
   }
	if(msg[2] & WM_CTRL_STATUS_BYTE1_LED_3) {
      led[2] = 1;
   }
	if(msg[2] & WM_CTRL_STATUS_BYTE1_LED_4) {
      led[3] = 1;
   }

#ifdef NEW_WIIUSE
	/* probe for Motion+ */
	if (!WIIMOTE_IS_SET(wm, WIIMOTE_STATE_MPLUS_PRESENT)) {
		wiiuse_probe_motion_plus(wm);
	}
#endif

	/* is an attachment connected to the expansion port? */
	if((msg[2] & WM_CTRL_STATUS_BYTE1_ATTACHMENT) == WM_CTRL_STATUS_BYTE1_ATTACHMENT) {
		WIIUSE_DEBUG("Attachment detected!");
      attachment = 1;
   }

	/* is the speaker enabled? */
	if((msg[2] & WM_CTRL_STATUS_BYTE1_SPEAKER_ENABLED) == WM_CTRL_STATUS_BYTE1_SPEAKER_ENABLED) {
		WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_SPEAKER);
   }

	/* is IR sensing enabled? */
	if((msg[2] & WM_CTRL_STATUS_BYTE1_IR_ENABLED) == WM_CTRL_STATUS_BYTE1_IR_ENABLED) {
      ir = 1;
   }

	/* find the battery level and normalize between 0 and 1 */
	wm->battery_level = (msg[5] / (float)WM_MAX_BATTERY_CODE);

	/* expansion port */
	if(attachment && !WIIMOTE_IS_SET(wm,WIIMOTE_STATE_EXP) && !WIIMOTE_IS_SET(wm,WIIMOTE_STATE_EXP_FAILED)
         && !WIIMOTE_IS_SET(wm,WIIMOTE_STATE_EXP_HANDSHAKE))
   {
		/* send the initialization code for the attachment */
#ifdef NEW_WIIUSE
		handshake_expansion(wm, NULL, 0);
		exp_changed = 1;
#else
      wiiuse_handshake_expansion_start(wm);
      goto done;
#endif
   } else if (!attachment && WIIMOTE_IS_SET(wm, WIIMOTE_STATE_EXP)) {
		/* attachment removed */
#ifdef NEW_WIIUSE
		disable_expansion(wm);
		exp_changed = 1;
#else
      wiiuse_disable_expansion(wm);
      goto done;
#endif
   }

#ifdef NEW_WIIUSE
#ifdef WIIUSE_WIN32
	if (!attachment) {
		WIIUSE_DEBUG("Setting timeout to normal %i ms.", wm->normal_timeout);
		wm->timeout = wm->normal_timeout;
	}
#endif
#endif

#ifdef NEW_WIIUSE
	/*
	 *	From now on the remote will only send status packets.
	 *	We need to send a WIIMOTE_CMD_REPORT_TYPE packet to
	 *	reenable other incoming reports.
	 */
	if (exp_changed && WIIMOTE_IS_SET(wm, WIIMOTE_STATE_IR)) {
		/*
		 *  Since the expansion status changed IR needs to
		 *  be reset for the new IR report mode.
		 */
		WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_IR);
		wiiuse_set_ir(wm, 1);
	} else {
		wiiuse_set_report_type(wm);
		return;
	}

	/* handling new Tx for changed exp */
	if (!req) {
		return;
	}
	if (!(req->state == REQ_SENT)) {
		return;
	}
	wm->data_req = req->next;
	req->state = REQ_DONE;
	/* if(req->cb!=NULL) req->cb(wm,msg,6); */
	free(req);
#else

	if(!ir && WIIMOTE_IS_SET(wm,WIIMOTE_STATE_IR_INIT)) {
		WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_IR_INIT);
		wiiuse_set_ir(wm, 1);
		goto done;
	}
	if(ir && !WIIMOTE_IS_SET(wm,WIIMOTE_STATE_IR)) WIIMOTE_ENABLE_STATE(wm,WIIMOTE_STATE_IR);
	else if(!ir && WIIMOTE_IS_SET(wm,WIIMOTE_STATE_IR)) WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_IR);

	if(!speaker && WIIMOTE_IS_SET(wm,WIIMOTE_STATE_SPEAKER_INIT)) {
		WIIMOTE_DISABLE_STATE(wm,WIIMOTE_STATE_SPEAKER_INIT);
		wiiuse_set_speaker(wm,1);
		goto done;
	}
	if(speaker && !WIIMOTE_IS_SET(wm,WIIMOTE_STATE_SPEAKER)) WIIMOTE_ENABLE_STATE(wm,WIIMOTE_STATE_SPEAKER);
	else if(!speaker && WIIMOTE_IS_SET(wm,WIIMOTE_STATE_SPEAKER)) WIIMOTE_DISABLE_STATE(wm,WIIMOTE_STATE_SPEAKER);

	wiiuse_set_report_type(wm,NULL);

done:
	if(!cmd) return;
	if(!(cmd->state==CMD_SENT && cmd->data[0]==WM_CMD_CTRL_STATUS)) return;

	wm->cmd_head = cmd->next;

	cmd->state = CMD_DONE;
	if(cmd->cb!=NULL) cmd->cb(wm,msg,6);
	
	__lwp_queue_append(&wm->cmdq,&cmd->node);
	wiiuse_send_next_command(wm);
#endif
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
		case EXP_GUITAR_HERO_3:
			guitar_hero_3_event(&wm->exp.gh3, msg);
			break;
 		case EXP_WII_BOARD:
 			wii_board_event(&wm->exp.wb, msg);
 			break;
 		case EXP_MOTION_PLUS:
#ifdef NEW_WIIUSE
		case EXP_MOTION_PLUS_CLASSIC:
		case EXP_MOTION_PLUS_NUNCHUK:
			motion_plus_event(&wm->exp.mp, wm->exp.type, msg);
#else
 			motion_plus_event(&wm->exp.mp, msg);
#endif
 			break;
		default:
			break;
	}
}

/**
 *	@brief Clear out all old 'dirty' read requests.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 */
void clear_dirty_reads(struct wiimote_t* wm) {
   //TODO/FIXME
#ifdef NEW_WIIUSE
	struct read_req_t* req = wm->read_req;

	while (req && req->dirty) {
		WIIUSE_DEBUG("Cleared old read request for address: %x", req->addr);

		wm->read_req = req->next;
		free(req);
		req = wm->read_req;
	}
#endif
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

	/* clear out any old read requests */
	clear_dirty_reads(wm);
}

/**
 *	@brief Handle accel data in a wiimote message.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *	@param msg		The message specified in the event packet.
 */
static void handle_wm_accel(struct wiimote_t* wm, ubyte* msg) {
	wm->accel.x = msg[2];
	wm->accel.y = msg[3];
	wm->accel.z = msg[4];

	/* calculate the remote orientation */
	calculate_orientation(&wm->accel_calib, &wm->accel, &wm->orient, WIIMOTE_IS_FLAG_SET(wm, WIIUSE_SMOOTHING));

	/* calculate the gforces on each axis */
	calculate_gforce(&wm->accel_calib, &wm->accel, &wm->gforce);
}

void parse_event(struct wiimote_t *wm)
{
	ubyte event;
	ubyte *msg;

	event = wm->event_buf[0];
	msg = wm->event_buf+1;

	switch(event) {
		case WM_RPT_BTN:
         /* button */
			wiiuse_pressed_buttons(wm,msg);
			break;
		case WM_RPT_BTN_ACC:
         /* button - motion */
			wiiuse_pressed_buttons(wm,msg);

         handle_wm_accel(wm, msg);

			break;
		case WM_RPT_READ:
         /* data read */
			event_data_read(wm,msg);

         /* yeah buttons may be pressed, but this wasn't an "event" */
			return;
		case WM_RPT_CTRL_STATUS:
         /* controller status */
			event_status(wm,msg);

         /* don't execute the event callback */
			return;
		case WM_RPT_BTN_EXP:
         /* button - expansion */
			wiiuse_pressed_buttons(wm,msg);
			handle_expansion(wm,msg+2);

			break;
		case WM_RPT_BTN_ACC_EXP:
			/* button - motion - expansion */
			wiiuse_pressed_buttons(wm, msg);

         handle_wm_accel(wm, msg);

			handle_expansion(wm, msg+5);

			break;
#ifndef NEW_WIIUSE
		case WM_RPT_ACK:
			event_ack(wm,msg);
			return;
#endif
		case WM_RPT_BTN_ACC_IR:
         /* button - motion - ir */
			wiiuse_pressed_buttons(wm,msg);

         handle_wm_accel(wm, msg);

         /* ir */
			calculate_extended_ir(wm, msg+5);

			break;
		case WM_RPT_BTN_IR_EXP:
         /* button - ir - expansion */
			wiiuse_pressed_buttons(wm,msg);
			calculate_basic_ir(wm, msg+2);

         /* ir */
         handle_expansion(wm,msg+12);

			break;
		case WM_RPT_BTN_ACC_IR_EXP:
			/* button - motion - ir - expansion */
			wiiuse_pressed_buttons(wm, msg);

         handle_wm_accel(wm, msg);

			handle_expansion(wm, msg+15);

			/* ir */
			calculate_basic_ir(wm, msg+5);

			break;
#ifdef NEW_WIIUSE
		case WM_RPT_WRITE: {
				event_data_write(wm, msg);
				break;
			}
#endif
		default:
			WIIUSE_WARNING("Unknown event, can not handle it [Code 0x%x].", event);
			return;
	}

   //TODO/FIXME
	/* was there an event? */
   //if (state_changed(wm)) {
      wm->event = WIIUSE_EVENT;
   //}
}

/**
 *	@brief Find what buttons are pressed.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *	@param msg		The message specified in the event packet.
 */
void wiiuse_pressed_buttons(struct wiimote_t* wm, ubyte* msg) {
	short now;

#ifdef GEKKO
	/* convert to big endian */
	now = BIG_ENDIAN_SHORT(*(short*)msg) & WIIMOTE_BUTTON_ALL;
#else
	/* convert from big endian */
	now = from_big_endian_uint16_t(msg) & WIIMOTE_BUTTON_ALL;
#endif

#ifndef NEW_WIIUSE
	/* preserve old btns pressed */
	wm->btns_last = wm->btns;
#endif

	/* pressed now & were pressed, then held */
	wm->btns_held = (now & wm->btns);

	/* were pressed or were held & not pressed now, then released */
	wm->btns_released = ((wm->btns | wm->btns_held) & ~now);

	/* buttons pressed now */
	wm->btns = now;
}
