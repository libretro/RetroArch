/*
 * This file is part of iMAME4all.
 *
 * Copyright (C) 2010 David Valdeita (Seleuco)
 *
 * based on:
 *
 *  wiiuse
 *
 *	Written By:
 *		Michael Laforest	< para >
 *		Email: < thepara (--AT--) g m a i l [--DOT--] com >
 *
 *	Copyright 2006-2007
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * In addition, as a special exception, Seleuco
 * gives permission to link the code of this program with
 * the MAME library (or with modified versions of MAME that use the
 * same license as MAME), and distribute linked combinations including
 * the two.  You must obey the GNU General Public License in all
 * respects for all of the code used other than MAME.  If you modify
 * this file, you may extend this exception to your version of the
 * file, but you are not obligated to do so.  If you do not wish to
 * do so, delete this exception statement from your version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include "../../../boolean.h"

#include "wiimote.h"

int wiimote_send(struct wiimote_t* wm, byte report_type, byte* msg, int len);
int wiimote_read_data(struct wiimote_t* wm, unsigned int addr, unsigned short len);
int wiimote_write_data(struct wiimote_t* wm, unsigned int addr, byte* data, byte len);
void wiimote_set_leds(struct wiimote_t* wm, int leds);
int classic_ctrl_handshake(struct wiimote_t* wm, struct classic_ctrl_t* cc, byte* data, unsigned short len);
void classic_ctrl_event(struct classic_ctrl_t* cc, byte* msg);

/**
 *	@brief Request the wiimote controller status.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *
 *	Controller status includes: battery level, LED status, expansions
 */
void wiimote_status(struct wiimote_t* wm)
{
   byte buf = 0;

   if (!wm || !WIIMOTE_IS_CONNECTED(wm))
      return;

#ifdef WIIMOTE_DBG
   printf("Requested wiimote status.\n");
#endif

   wiimote_send(wm, WM_CMD_CTRL_STATUS, &buf, 1);
}

void wiimote_data_report(struct wiimote_t* wm, byte type) {
	byte buf[2] = {0x0,0x0};

	if (!wm  || !WIIMOTE_IS_CONNECTED(wm))
		return;

    buf[1] = type;
//CUIDADO es un &buf?
	wiimote_send(wm, WM_CMD_REPORT_TYPE, buf, 2);
}


/**
 *	@brief	Set the enabled LEDs.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *	@param leds		What LEDs to enable.
 *
 *	\a leds is a bitwise or of WIIMOTE_LED_1, WIIMOTE_LED_2, WIIMOTE_LED_3, or WIIMOTE_LED_4.
 */
void wiimote_set_leds(struct wiimote_t* wm, int leds) {
	byte buf;

	if (!wm || !WIIMOTE_IS_CONNECTED(wm))
		return;

	/* remove the lower 4 bits because they control rumble */
	wm->leds = (leds & 0xF0);

	buf = wm->leds;

	wiimote_send(wm, WM_CMD_LED, &buf, 1);
}

/**
 *	@brief Find what buttons are pressed.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *	@param msg		The message specified in the event packet.
 */
void wiimote_pressed_buttons(struct wiimote_t* wm, byte* msg) {
	short now;

	/* convert to big endian */
	now = BIG_ENDIAN_SHORT(*(short*)msg) & WIIMOTE_BUTTON_ALL;

	/* buttons pressed now */
	wm->btns = now;
}

/**
 *	@brief Handle data from the expansion.
 *
 *	@param wm		A pointer to a wiimote_t structure.
 *	@param msg		The message specified in the event packet for the expansion.
 */
void wiimote_handle_expansion(struct wiimote_t* wm, byte* msg) {
	switch (wm->exp.type) {
		case EXP_CLASSIC:
			classic_ctrl_event(&wm->exp.cc.classic, msg);
			break;
		default:
			break;
	}
}

/**
*	@brief Get initialization data from the wiimote.
*
*	@param wm		Pointer to a wiimote_t structure.
*	@param data		unused
*	@param len		unused
*
*	When first called for a wiimote_t structure, a request
*	is sent to the wiimote for initialization information.
*	This includes factory set accelerometer data.
*	The handshake will be concluded when the wiimote responds
*	with this data.
*/
int wiimote_handshake(struct wiimote_t* wm,  byte event, byte* data, unsigned short len) {

	if (!wm) return 0;

	while(1)
	{
#ifdef WIIMOTE_DBG
		printf("Handshake %d\n",wm->handshake_state);
#endif
		switch (wm->handshake_state)
      {
         case 0://no ha habido nunca handshake, debemos forzar un mensaje de staus para ver que pasa.
            {
               WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE);
               wiimote_set_leds(wm, WIIMOTE_LED_NONE);

               /* request the status of the wiimote to see if there is an expansion */
               wiimote_status(wm);

               wm->handshake_state=1;
               return 0;
            }
         case 1://estamos haciendo handshake o bien se necesita iniciar un nuevo handshake ya que se inserta(quita una expansion.
            {
               int attachment = 0;

               if(event != WM_RPT_CTRL_STATUS)
                  return 0;

               /* is an attachment connected to the expansion port? */
               if ((data[2] & WM_CTRL_STATUS_BYTE1_ATTACHMENT) == WM_CTRL_STATUS_BYTE1_ATTACHMENT)
                  attachment = 1;

#ifdef WIIMOTE_DBG
               printf("attachment %d %d\n",attachment,WIIMOTE_IS_SET(wm, WIIMOTE_STATE_EXP));
#endif

               /* expansion port */
               if (attachment && !WIIMOTE_IS_SET(wm, WIIMOTE_STATE_EXP))
               {
                  WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_EXP);

                  /* send the initialization code for the attachment */
#ifdef WIIMOTE_DBG
                  printf("haciendo el handshake de la expansion\n");
#endif

                  if(WIIMOTE_IS_SET(wm,WIIMOTE_STATE_HANDSHAKE_COMPLETE))
                  {
#ifdef WIIMOTE_DBG
                     printf("rehandshake\n");
#endif
                     WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE_COMPLETE);
                     WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE);//forzamos un handshake por si venimos de un hanshake completo
                  }

                  byte buf;
                  //Old way. initialize the extension was by writing the single encryption byte 0x00 to 0x(4)A40040
                  //buf = 0x00;
                  //wiimote_write_data(wm, WM_EXP_MEM_ENABLE, &buf, 1);

                  //NEW WAY 0x55 to 0x(4)A400F0, then writing 0x00 to 0x(4)A400FB. (support clones)
                  buf = 0x55;
                  wiimote_write_data(wm, 0x04A400F0, &buf, 1);
                  usleep(100000);
                  buf = 0x00;
                  wiimote_write_data(wm, 0x04A400FB, &buf, 1);

                  //check extension type!
                  usleep(100000);
                  wiimote_read_data(wm, WM_EXP_MEM_CALIBR+220, 4);
                  //wiimote_read_data(wm, WM_EXP_MEM_CALIBR, EXP_HANDSHAKE_LEN);

                  wm->handshake_state = 4;
                  return 0;

               } else if (!attachment && WIIMOTE_IS_SET(wm, WIIMOTE_STATE_EXP)) {
                  /* attachment removed */
                  WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_EXP);
                  wm->exp.type = EXP_NONE;

                  if(WIIMOTE_IS_SET(wm,WIIMOTE_STATE_HANDSHAKE_COMPLETE))
                  {
#ifdef WIIMOTE_DBG
                     printf("rehandshake\n");
#endif
                     WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE_COMPLETE);
                     WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE);//forzamos un handshake por si venimos de un hanshake completo
                  }
               }

               if(!attachment &&  WIIMOTE_IS_SET(wm,WIIMOTE_STATE_HANDSHAKE))
               {
                  wm->handshake_state = 2;
                  continue;
               }

               return 0;
            }
         case 2://find handshake no expansion
            {
#ifdef WIIMOTE_DBG
               printf("Finalizado HANDSHAKE SIN EXPANSION\n");
#endif
               wiimote_data_report(wm,WM_RPT_BTN);
               wm->handshake_state = 6;
               continue;
            }
         case 3://find handshake expansion
            {
#ifdef WIIMOTE_DBG
               printf("Finalizado HANDSHAKE CON EXPANSION\n");
#endif
               wiimote_data_report(wm,WM_RPT_BTN_EXP);
               wm->handshake_state = 6;
               continue;
            }
         case 4:
            {
               if(event !=  WM_RPT_READ)
                  return 0;

               int id = BIG_ENDIAN_LONG(*(int*)(data));

#ifdef WIIMOTE_DBG
               printf("Expansion id=0x%04x\n",id);
#endif

               if(id!=/*EXP_ID_CODE_CLASSIC_CONTROLLER*/0xa4200101)
               {
                  wm->handshake_state = 2;
                  //WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_EXP);
                  continue;
               }
               else
               {
                  usleep(100000);
                  wiimote_read_data(wm, WM_EXP_MEM_CALIBR, 16);//pedimos datos de calibracion del JOY!
                  wm->handshake_state = 5;
               }

               return 0;
            }
         case 5:
            {
               if(event !=  WM_RPT_READ)
                  return 0;

               classic_ctrl_handshake(wm, &wm->exp.cc.classic, data,len);
               wm->handshake_state = 3;
               continue;

            }
         case 6:
            {
               WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE);
               WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE_COMPLETE);
               wm->handshake_state = 1;
               if(wm->unid==0)
                  wiimote_set_leds(wm, WIIMOTE_LED_1);
               else if(wm->unid==1)
                  wiimote_set_leds(wm, WIIMOTE_LED_2);
               else if(wm->unid==2)
                  wiimote_set_leds(wm, WIIMOTE_LED_3);
               else if(wm->unid==3)
                  wiimote_set_leds(wm, WIIMOTE_LED_4);
               return 1;
            }
         default:
            {
               break;
            }
      }
	}
}


/**
 *	@brief	Send a packet to the wiimote.
 *
 *	@param wm			Pointer to a wiimote_t structure.
 *	@param report_type	The report type to send (WIIMOTE_CMD_LED, WIIMOTE_CMD_RUMBLE, etc). Found in wiimote.h
 *	@param msg			The payload.
 *	@param len			Length of the payload in bytes.
 *
 *	This function should replace any write()s directly to the wiimote device.
 */
int wiimote_send(struct wiimote_t* wm, byte report_type, byte* msg, int len)
{
   byte buf[32];

   buf[0] = WM_SET_REPORT | WM_BT_OUTPUT;
   buf[1] = report_type;

   memcpy(buf+2, msg, len);

#ifdef WIIMOTE_DBG
   int x = 2;
   printf("[DEBUG] (id %i) SEND: (%x) %.2x ", wm->unid, buf[0], buf[1]);
   for (; x < len+2; ++x)
      printf("%.2x ", buf[x]);
   printf("\n");
#endif

   apple_pad_send_control(wm->connection, buf, len + 2);
   return 1;
}

/**
 *	@brief	Read data from the wiimote (event version).
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *	@param addr		The address of wiimote memory to read from.
 *	@param len		The length of the block to be read.
 *
 *	The library can only handle one data read request at a time
 *	because it must keep track of the buffer and other
 *	events that are specific to that request.  So if a request
 *	has already been made, subsequent requests will be added
 *	to a pending list and be sent out when the previous
 *	finishes.
 */
int wiimote_read_data(struct wiimote_t* wm, unsigned int addr, unsigned short len) {
	//No puden ser mas de 16 lo leido o vendra en trozos!

	if (!wm || !WIIMOTE_IS_CONNECTED(wm))
		return 0;
	if (!len /*|| len > 16*/)
		return 0;

	byte buf[6];

	/* the offset is in big endian */
	*(int*)(buf) = BIG_ENDIAN_LONG(addr);

	/* the length is in big endian */
	*(short*)(buf + 4) = BIG_ENDIAN_SHORT(len);

#ifdef WIIMOTE_DBG
	printf("Request read at address: 0x%x  length: %i", addr, len);
#endif
	wiimote_send(wm, WM_CMD_READ_DATA, buf, 6);

	return 1;
}

/**
 *	@brief	Write data to the wiimote.
 *
 *	@param wm			Pointer to a wiimote_t structure.
 *	@param addr			The address to write to.
 *	@param data			The data to be written to the memory location.
 *	@param len			The length of the block to be written.
 */
int wiimote_write_data(struct wiimote_t* wm, unsigned int addr, byte* data, byte len) {
	byte buf[21] = {0};		/* the payload is always 23 */

	if (!wm || !WIIMOTE_IS_CONNECTED(wm))
		return 0;
	if (!data || !len)
		return 0;

#ifdef WIIMOTE_DBG
	printf("Writing %i bytes to memory location 0x%x...\n", len, addr);

   int i = 0;
   printf("Write data is: ");
   for (; i < len; ++i)
      printf("%x ", data[i]);
   printf("\n");
#endif

	/* the offset is in big endian */
	*(int*)(buf) = BIG_ENDIAN_LONG(addr);

	/* length */
	*(byte*)(buf + 4) = len;

	/* data */
	memcpy(buf + 5, data, len);

	wiimote_send(wm, WM_CMD_WRITE_DATA, buf, 21);
	return 1;
}


/////////////////////// CLASSIC  /////////////////

int classic_ctrl_handshake(struct wiimote_t* wm, struct classic_ctrl_t* cc, byte* data, unsigned short len)
{
   memset(cc, 0, sizeof(*cc));
	wm->exp.type = EXP_CLASSIC;
	return 1;
}

static float normalize_and_interpolate(float min, float max, float t)
{
   return (min == max) ? 0.0f : (t - min) / (max - min);
}

static void process_axis(struct axis_t* axis, byte raw)
{
   if (!axis->has_center)
   {
      axis->has_center = true;
      axis->min = raw - 2;
      axis->center = raw;
      axis->max = raw + 2;
   }

   if (raw < axis->min) axis->min = raw;
   if (raw > axis->max) axis->max = raw;
   axis->raw_value = raw;

   if (raw < axis->center)
      axis->value = -normalize_and_interpolate(axis->center, axis->min, raw);
   else if (raw > axis->center)
      axis->value =  normalize_and_interpolate(axis->center, axis->max, raw);
   else
      axis->value = 0;
}

void classic_ctrl_event(struct classic_ctrl_t* cc, byte* msg)
{
   cc->btns = ~BIG_ENDIAN_SHORT(*(short*)(msg + 4)) & CLASSIC_CTRL_BUTTON_ALL;
	process_axis(&cc->ljs.x, (msg[0] & 0x3F));
   process_axis(&cc->ljs.y, (msg[1] & 0x3F));
   process_axis(&cc->rjs.x, ((msg[0] & 0xC0) >> 3) | ((msg[1] & 0xC0) >> 5) | ((msg[2] & 0x80) >> 7));
   process_axis(&cc->rjs.y, (msg[2] & 0x1F));
}
