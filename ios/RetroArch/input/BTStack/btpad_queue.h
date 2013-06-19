/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013 - Jason Fetters
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __IOS_RARCH_BTPAD_QUEUE_H__
#define __IOS_RARCH_BTPAD_QUEUE_H__

void btpad_queue_reset();
void btpad_queue_run(uint32_t count);
void btpad_queue_process();

void btpad_queue_btstack_set_power_mode(uint8_t on);
void btpad_queue_hci_read_bd_addr();
void btpad_queue_hci_disconnect(uint16_t handle, uint8_t reason);
void btpad_queue_hci_inquiry(uint32_t lap, uint8_t length, uint8_t num_responses);
void btpad_queue_hci_remote_name_request(bd_addr_t bd_addr, uint8_t page_scan_repetition_mode, uint8_t reserved, uint16_t clock_offset);
void btpad_queue_hci_pin_code_request_reply(bd_addr_t bd_addr, bd_addr_t pin);
void btpad_queue_l2cap_register_service(uint16_t psm, uint16_t mtu);
void btpad_queue_l2cap_create_channel(bd_addr_t bd_addr, uint16_t psm);
void btpad_queue_l2cap_accept_connection(uint16_t cid);
void btpad_queue_l2cap_decline_connection(uint16_t cid, uint8_t reason);

#endif
