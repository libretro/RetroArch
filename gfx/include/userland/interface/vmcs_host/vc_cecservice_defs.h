/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 * CEC service command enumeration and parameter types.
 */

/**
 * \file
 * This file contains definition shared by host side and
 * Videocore side CEC service:
 *
 * In general, a zero return value indicates success of the function
 * A non-zero value indicates VCHI error
 * A positive value indicates alternative return value (for some functions).
 *
 */


#ifndef _VC_CECSERVICE_DEFS_H_
#define _VC_CECSERVICE_DEFS_H_
#include "vcinclude/common.h"
#include "interface/vcos/vcos.h"
#include "interface/vcos/vcos_logging.h"
#include "interface/vchi/message_drivers/message.h"

//CEC VCOS logging stuff
#define CECHOST_LOG_CATEGORY (&cechost_log_category)
#define vc_cec_log_trace(...) _VCOS_LOG_X(CECHOST_LOG_CATEGORY, VCOS_LOG_TRACE, __VA_ARGS__)
#define vc_cec_log_warn(...)  _VCOS_LOG_X(CECHOST_LOG_CATEGORY, VCOS_LOG_WARN, __VA_ARGS__)
#define vc_cec_log_error(...) _VCOS_LOG_X(CECHOST_LOG_CATEGORY, VCOS_LOG_ERROR, __VA_ARGS__)
#define vc_cec_log_info(...)  _VCOS_LOG_X(CECHOST_LOG_CATEGORY, VCOS_LOG_INFO, __VA_ARGS__)
extern VCOS_LOG_CAT_T cechost_log_category; //The actual object lives in CEC host side service code

#define VC_CECSERVICE_VER 1
#define CECSERVICE_MSGFIFO_SIZE 1024
#define CECSERVICE_CLIENT_NAME MAKE_FOURCC("CECS")
#define CECSERVICE_NOTIFY_NAME MAKE_FOURCC("CECN")

//CEC service commands
typedef enum {
   VC_CEC_REGISTER_CMD = 0,
   VC_CEC_REGISTER_ALL,
   VC_CEC_DEREGISTER_CMD,
   VC_CEC_DEREGISTER_ALL,
   VC_CEC_SEND_MSG,
   VC_CEC_GET_LOGICAL_ADDR,
   VC_CEC_ALLOC_LOGICAL_ADDR,
   VC_CEC_RELEASE_LOGICAL_ADDR,
   VC_CEC_GET_TOPOLOGY,
   VC_CEC_SET_VENDOR_ID,
   VC_CEC_SET_OSD_NAME,
   VC_CEC_GET_PHYSICAL_ADDR,
   VC_CEC_GET_VENDOR_ID,

   //The following 3 commands are used when CEC middleware is 
   //running in passive mode (i.e. it does not allocate 
   //logical address automatically)
   VC_CEC_POLL_ADDR,
   VC_CEC_SET_LOGICAL_ADDR,
   VC_CEC_ADD_DEVICE,
   VC_CEC_SET_PASSIVE,
   //Add more commands here
   VC_CEC_END_OF_LIST
} VC_CEC_CMD_CODE_T;

//See vc_cec.h for details
//REGISTER_CMD
//Parameters: opcode to register (CEC_OPCODE_T sent as uint32)
//Reply: none

//REGISTER_ALL
//Parameters: none
//Reply: none

//DEREGISTER_CMD
//Parameters: opcode to deregister (CEC_OPCODE_T sent as uint32)
//Reply: none

//DEREGISTER_ALL
//Parameters: none
//Reply: none

//SEND_MSG
//Parameters: destination, length, 16 bytes buffer (message can only be at most 15 bytes however)
//Reply: none (callback)
typedef struct {
   uint32_t follower;
   uint32_t length;
   uint8_t payload[16]; //max. 15 bytes padded to 16
   uint32_t is_reply;   //non-zero if this is a reply, zero otherwise
} CEC_SEND_MSG_PARAM_T;

//GET_LOGICAL_ADDR
//Parameters: none
//Reply: logical address (uint8 returned as uint32)

//ALLOC_LOGICAL_ADDR
//Parameters: none
//Reply: none (callback)

//GET_TOPOLOGY
//Parameters: none
//Reply: topology (see VC_TOPOLOGY_T)

//SET_VENDOR_ID
//Parameters: vendor id (uint32)
//Reply: none

//Set OSD name
//Parameters: 14 byte char
//Reply: none
#define OSD_NAME_LENGTH 14

//GET_PHYSICAL_ADDR
//Parameter: none
//Reply: packed physical address returned as uint16

//GET_VENDOR_ID
//Parameter: logical address (CEC_AllDevices_T sent as uint32_t)
//Reply: (uint32_t vendor id)

//POLL_LOGICAL_ADDR (only for passive mode)
//Used by host to test a logical address to see if it is available for use
//Only available if CEC is compiled in passive mode and while the host
//is testing the availability of a logical address
//Parameter: logical address
//Reply: 

//SET_LOGICAL_ADDR [(only for passive mode) This will be changed when we support multiple logical addresses]
//Set the logical address used 
//Only available if CEC is compiled in passive mode
//Parameter: logical address, device type, vendor ID
//Reply: (int32_t - zero means success, non-zero otherwise)
//This function will result in a VC_CEC_LOGICAL_ADDR callback
typedef struct {
   uint32_t logical_address;
   uint32_t device_type;
   uint32_t vendor_id;
} CEC_SET_LOGICAL_ADDR_PARAM_T;
   
//ADD_DEVICE (only for passive mode)
//Only available if CEC is compiled in passive mode
//Parameter: logical address, physical address, device type, last device?
//Reply: (int32_t - zero means success, non-zero otherwise)
typedef struct {
   uint32_t logical_address;  /**<logical address */
   uint32_t physical_address; /**<16-bit packed physical address in big endian */
   uint32_t device_type;      /**<CEC_DEVICE_TYPE_T */
   uint32_t last_device;      /**<True (non-zero) or false (zero) */
} CEC_ADD_DEVICE_PARAM_T;

//SET PASSIVE (only for passive mode)
//Enable/disable passive mode
//Parameter: non-zero to enable passive mode, zero to disable
//Reply: (int32_t - zero means success, non-zero otherwise, non zero will be VCHI errors)

#endif //#ifndef _VC_CECSERVICE_DEFS_H_
