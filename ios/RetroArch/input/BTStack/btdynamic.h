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

#ifndef __IOS_RARCH_BTDYNAMIC_H__
#define __IOS_RARCH_BTDYNAMIC_H__

#include "btstack/utils.h"
#include "btstack/btstack.h"

#ifndef BUILDING_BTDYNAMIC
#define BTDIMPORT extern
#else
#define BTDIMPORT
#endif

bool load_btstack();

BTDIMPORT int (*bt_open_ptr)(void);
BTDIMPORT void (*bt_flip_addr_ptr)(bd_addr_t dest, bd_addr_t src);
BTDIMPORT btstack_packet_handler_t (*bt_register_packet_handler_ptr)(btstack_packet_handler_t handler);
BTDIMPORT int (*bt_send_cmd_ptr)(const hci_cmd_t *cmd, ...);
BTDIMPORT void (*bt_send_l2cap_ptr)(uint16_t local_cid, uint8_t *data, uint16_t len);
BTDIMPORT void (*run_loop_init_ptr)(RUN_LOOP_TYPE type);

BTDIMPORT const hci_cmd_t* btstack_get_system_bluetooth_enabled_ptr;
BTDIMPORT const hci_cmd_t* btstack_set_power_mode_ptr;
BTDIMPORT const hci_cmd_t* btstack_set_system_bluetooth_enabled_ptr;
BTDIMPORT const hci_cmd_t* hci_delete_stored_link_key_ptr;
BTDIMPORT const hci_cmd_t* hci_inquiry_ptr;
BTDIMPORT const hci_cmd_t* hci_inquiry_cancel_ptr;
BTDIMPORT const hci_cmd_t* hci_pin_code_request_reply_ptr;
BTDIMPORT const hci_cmd_t* hci_remote_name_request_ptr;
BTDIMPORT const hci_cmd_t* hci_remote_name_request_cancel_ptr;
BTDIMPORT const hci_cmd_t* hci_write_authentication_enable_ptr;
BTDIMPORT const hci_cmd_t* hci_write_inquiry_mode_ptr;
BTDIMPORT const hci_cmd_t* l2cap_create_channel_ptr;

#endif
