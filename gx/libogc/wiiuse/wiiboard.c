/*
 *	wiiuse
 *
 *	Written By:
 *		Michael Laforest	< para >
 *		Email: < thepara (--AT--) g m a i l [--DOT--] com >
 *
 *	Copyright 2006-2007
 *
 *	This file is part of wiiuse.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 3 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *	$Header: /cvsroot/devkitpro/libogc/wiiuse/wiiboard.c,v 1.6 2008/05/26 19:24:53 shagkur Exp $
 *
 */

/**
 *	@file
 *	@brief Wii Fit Balance Board device.
 */

#include "wiiboard.h"

#include <stdio.h>                      /* for printf */
#include <string.h>                     /* for memset */

#include "io.h"

/**
 *	@brief Handle the handshake data from the wiiboard.
 *
 *	@param wb		A pointer to a wii_board_t structure.
 *	@param data		The data read in from the device.
 *	@param len		The length of the data block, in bytes.
 *
 *	@return	Returns 1 if handshake was successful, 0 if not.
 */
int wii_board_handshake(struct wiimote_t* wm, struct wii_board_t* wb, ubyte* data, uword len) 
{
#ifdef NEW_WIIUSE
	ubyte * bufptr;
#endif

	/* decrypt data */
#ifdef WITH_WIIUSE_DEBUG
	int i;
	printf("DECRYPTED DATA WIIBOARD\n");
	for (i = 0; i < len; ++i) {
		if (i % 16 == 0) {
			if (i != 0) {
				printf("\n");
			}

			printf("%X: ", 0x4a40000 + 32 + i);
		}
		printf("%02X ", data[i]);
	}
	printf("\n");
#endif

#if defined(MSB_FIRST) || defined(GEKKO)
	int offset = 0;

	if (data[offset]==0xff) {
		if (data[offset+16]==0xff) {
			WIIUSE_DEBUG("Wii Balance Board handshake appears invalid, trying again.");
			wiiuse_read_data(wm, data, WM_EXP_MEM_CALIBR, EXP_HANDSHAKE_LEN, wiiuse_handshake_expansion);
			return 0;
		}
		offset += 16;
	}

	wb->ctr[0] = (data[offset+4]<<8)|data[offset+5];
	wb->cbr[0] = (data[offset+6]<<8)|data[offset+7];
	wb->ctl[0] = (data[offset+8]<<8)|data[offset+9];
	wb->cbl[0] = (data[offset+10]<<8)|data[offset+11];

	wb->ctr[1] = (data[offset+12]<<8)|data[offset+13];
	wb->cbr[1] = (data[offset+14]<<8)|data[offset+15];
	wb->ctl[1] = (data[offset+16]<<8)|data[offset+17];
	wb->cbl[1] = (data[offset+18]<<8)|data[offset+19];

	wb->ctr[2] = (data[offset+20]<<8)|data[offset+21];
	wb->cbr[2] = (data[offset+22]<<8)|data[offset+23];
	wb->ctl[2] = (data[offset+24]<<8)|data[offset+25];
	wb->cbl[2] = (data[offset+26]<<8)|data[offset+27];
#else
	bufptr = data + 4;
	wb->ctr[0] = unbuffer_big_endian_uint16_t(&bufptr);
	wb->cbr[0] = unbuffer_big_endian_uint16_t(&bufptr);
	wb->ctl[0] = unbuffer_big_endian_uint16_t(&bufptr);
	wb->cbl[0] = unbuffer_big_endian_uint16_t(&bufptr);

	wb->ctr[1] = unbuffer_big_endian_uint16_t(&bufptr);
	wb->cbr[1] = unbuffer_big_endian_uint16_t(&bufptr);
	wb->ctl[1] = unbuffer_big_endian_uint16_t(&bufptr);
	wb->cbl[1] = unbuffer_big_endian_uint16_t(&bufptr);

	wb->ctr[2] = unbuffer_big_endian_uint16_t(&bufptr);
	wb->cbr[2] = unbuffer_big_endian_uint16_t(&bufptr);
	wb->ctl[2] = unbuffer_big_endian_uint16_t(&bufptr);
	wb->cbl[2] = unbuffer_big_endian_uint16_t(&bufptr);
#endif

	/* handshake done */
	wm->event = WIIUSE_WII_BOARD_INSERTED;
	wm->exp.type = EXP_WII_BOARD;

#ifdef WIIUSE_WIN32
	wm->timeout = WIIMOTE_DEFAULT_TIMEOUT;
#endif

	return 1; 
}


/**
 *	@brief The wii board disconnected.
 *
 *	@param cc		A pointer to a classic_ctrl_t structure.
 */
void wii_board_disconnected(struct wii_board_t* wb)
{
	memset(wb, 0, sizeof(struct wii_board_t));
}

#if !defined(MSB_FIRST) && !defined(GEKKO)
static float do_interpolate(uint16_t raw, uint16_t cal[3]) {
#define WIIBOARD_MIDDLE_CALIB 17.0f
	if (raw < cal[0]) {
		return 0.0f;
	} else if (raw < cal[1]) {
		return ((float)(raw - cal[0]) * WIIBOARD_MIDDLE_CALIB) / (float)(cal[1] - cal[0]);
	} else if (raw < cal[2]) {
		return ((float)(raw - cal[1]) * WIIBOARD_MIDDLE_CALIB) / (float)(cal[2] - cal[1]) + WIIBOARD_MIDDLE_CALIB;
	} else {
		return WIIBOARD_MIDDLE_CALIB * 2.0f;
	}
}
#endif


/**
 *	@brief Handle wii board event.
 *
 *	@param wb		A pointer to a wii_board_t structure.
 *	@param msg		The message specified in the event packet.
 */
void wii_board_event(struct wii_board_t* wb, ubyte* msg)
{ 
#if defined(MSB_FIRST) || defined(GEKKO)
	wb->rtr = (msg[0]<<8)|msg[1];
	wb->rbr = (msg[2]<<8)|msg[3];
	wb->rtl = (msg[4]<<8)|msg[5];
	wb->rbl = (msg[6]<<8)|msg[7];	
#else
	byte * bufPtr = msg;
	wb->rtr = unbuffer_big_endian_uint16_t(&bufPtr);
	wb->rbr = unbuffer_big_endian_uint16_t(&bufPtr);
	wb->rtl = unbuffer_big_endian_uint16_t(&bufPtr);
	wb->rbl = unbuffer_big_endian_uint16_t(&bufPtr);

	/*
		Interpolate values
		Calculations borrowed from wiili.org - No names to mention sadly :( http://www.wiili.org/index.php/Wii_Balance_Board_PC_Drivers page however!
	*/
	wb->tr = do_interpolate(wb->rtr, wb->ctr);
	wb->tl = do_interpolate(wb->rtl, wb->ctl);
	wb->br = do_interpolate(wb->rbr, wb->cbr);
	wb->bl = do_interpolate(wb->rbl, wb->cbl);
#endif
}
