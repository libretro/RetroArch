/* This source as presented is a modified version of original wiiuse for use 
 * with RetroArch, and must not be confused with the original software. */

#include <stdio.h>
#include <stdlib.h>

#include "definitions.h"
#include "wiiuse_internal.h"
#include "nunchuk.h"
#include "classic.h"
#include "motion_plus.h"
#include "io.h"
#include "lwp_wkspace.inl"

void wiiuse_handshake(struct wiimote_t *wm,ubyte *data,uword len)
{
	ubyte *buf = NULL;
	struct accel_t *accel = &wm->accel_calib;

	//printf("wiiuse_handshake(%d,%p,%d)\n",wm->handshake_state,data,len);

	switch(wm->handshake_state) {
		case 0:
			wm->handshake_state++;

			wiiuse_set_leds(wm,WIIMOTE_LED_NONE,NULL);

			buf = __lwp_wkspace_allocate(sizeof(ubyte)*8);
			wiiuse_read_data(wm,buf,WM_MEM_OFFSET_CALIBRATION,7,wiiuse_handshake);
			break;
		case 1:
			wm->handshake_state++;

			accel->cal_zero.x = ((data[0]<<2)|((data[3]>>4)&3));
			accel->cal_zero.y = ((data[1]<<2)|((data[3]>>2)&3));
			accel->cal_zero.z = ((data[2]<<2)|(data[3]&3));

			accel->cal_g.x = (((data[4]<<2)|((data[7]>>4)&3)) - accel->cal_zero.x);
			accel->cal_g.y = (((data[5]<<2)|((data[7]>>2)&3)) - accel->cal_zero.y);
			accel->cal_g.z = (((data[6]<<2)|(data[7]&3)) - accel->cal_zero.z);
			__lwp_wkspace_free(data);
			
			WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE);
			WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE_COMPLETE);

			wm->event = WIIUSE_CONNECT;
			wiiuse_status(wm,NULL);
			break;
		default:
			break;

	}
}

void wiiuse_handshake_expansion_start(struct wiimote_t *wm)
{
	if(WIIMOTE_IS_SET(wm,WIIMOTE_STATE_EXP) || WIIMOTE_IS_SET(wm,WIIMOTE_STATE_EXP_FAILED) || WIIMOTE_IS_SET(wm,WIIMOTE_STATE_EXP_HANDSHAKE))
		return;

	wm->expansion_state = 0;
	WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_EXP_HANDSHAKE);
	wiiuse_handshake_expansion(wm, NULL, 0);
}

void wiiuse_handshake_expansion(struct wiimote_t *wm,ubyte *data,uword len)
{
	int id;
	ubyte val;
	ubyte *buf = NULL;

	switch(wm->expansion_state) {
		/* These two initialization writes disable the encryption */
		case 0:
			wm->expansion_state = 1;
			val = 0x55;
			wiiuse_write_data(wm,WM_EXP_MEM_ENABLE1,&val,1,wiiuse_handshake_expansion);
			break;
		case 1:
			wm->expansion_state = 2;
			val = 0x00;
			wiiuse_write_data(wm,WM_EXP_MEM_ENABLE2,&val,1,wiiuse_handshake_expansion);
			break;
		case 2:
			wm->expansion_state = 3;
			buf = __lwp_wkspace_allocate(sizeof(ubyte)*EXP_HANDSHAKE_LEN);
			wiiuse_read_data(wm,buf,WM_EXP_MEM_CALIBR,EXP_HANDSHAKE_LEN,wiiuse_handshake_expansion);
			break;
		case 3:
			if(!data || !len) return;
			id = BIG_ENDIAN_LONG(*(int*)(&data[220]));

			switch(id) {
				case EXP_ID_CODE_NUNCHUK:
					if(!nunchuk_handshake(wm,&wm->exp.nunchuk,data,len)) return;
					break;
				case EXP_ID_CODE_CLASSIC_CONTROLLER:
				case EXP_ID_CODE_CLASSIC_CONTROLLER_NYKOWING:
				case EXP_ID_CODE_CLASSIC_CONTROLLER_NYKOWING2:
				case EXP_ID_CODE_CLASSIC_CONTROLLER_NYKOWING3:
				case EXP_ID_CODE_CLASSIC_CONTROLLER_GENERIC:
				case EXP_ID_CODE_CLASSIC_CONTROLLER_GENERIC2:
				case EXP_ID_CODE_CLASSIC_CONTROLLER_GENERIC3:
				case EXP_ID_CODE_CLASSIC_CONTROLLER_GENERIC4:
				case EXP_ID_CODE_CLASSIC_CONTROLLER_GENERIC5:
					if(!classic_ctrl_handshake(wm,&wm->exp.classic,data,len)) return;
					break;
				default:
					if(!classic_ctrl_handshake(wm,&wm->exp.classic,data,len)) return;
					/*WIIMOTE_DISABLE_STATE(wm,WIIMOTE_STATE_EXP_HANDSHAKE);
					WIIMOTE_ENABLE_STATE(wm,WIIMOTE_STATE_EXP_FAILED);
					__lwp_wkspace_free(data);
					wiiuse_status(wm,NULL);
					return;*/
			}
			__lwp_wkspace_free(data);

			WIIMOTE_DISABLE_STATE(wm,WIIMOTE_STATE_EXP_HANDSHAKE);
			WIIMOTE_ENABLE_STATE(wm,WIIMOTE_STATE_EXP);
			wiiuse_set_ir_mode(wm);
			wiiuse_status(wm,NULL);
			break;
	}
}

void wiiuse_disable_expansion(struct wiimote_t *wm)
{
	if(!WIIMOTE_IS_SET(wm, WIIMOTE_STATE_EXP)) return;

	/* tell the associated module the expansion was removed */
	switch(wm->exp.type) {
		case EXP_NUNCHUK:
			nunchuk_disconnected(&wm->exp.nunchuk);
			wm->event = WIIUSE_NUNCHUK_REMOVED;
			break;
		case EXP_CLASSIC:
			classic_ctrl_disconnected(&wm->exp.classic);
			wm->event = WIIUSE_CLASSIC_CTRL_REMOVED;
			break;
		case EXP_MOTION_PLUS:
 			motion_plus_disconnected(&wm->exp.mp);
 			wm->event = WIIUSE_MOTION_PLUS_REMOVED;
 			break;

		default:
			break;
	}

	WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_EXP);
	wm->exp.type = EXP_NONE;

	wiiuse_set_ir_mode(wm);
	wiiuse_status(wm,NULL);
}
