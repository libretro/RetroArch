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

#ifndef VC_VCHI_FILESERVICE_DEFS_H
#define VC_VCHI_FILESERVICE_DEFS_H

#include "interface/vchi/vchi.h"

/* Definitions (not used by API) */

/* structure used by both side to communicate */
#define FILESERV_MAX_BULK_SECTOR  128   //must be power of two

#define FILESERV_SECTOR_LENGTH  512

#define FILESERV_MAX_BULK (FILESERV_MAX_BULK_SECTOR*FILESERV_SECTOR_LENGTH)

#define FILESERV_4CC  MAKE_FOURCC("FSRV")

typedef enum FILESERV_EVENT_T
{
   FILESERV_BULK_RX = 0,
   FILESERV_BULK_TX,
   FILESERV_BULK_RX_0,
   FILESERV_BULK_RX_1
}FILESERV_EVENT_T;
//this following structure has to equal VCHI_MAX_MSG_SIZE
#define FILESERV_MAX_DATA	(VCHI_MAX_MSG_SIZE - 40) //(VCHI_MAX_MSG_SIZE - 24)

typedef struct{
	uint32_t xid;		    //4 // transaction's ID, used to match cmds with response
   uint32_t cmd_code;    //4
   uint32_t params[4];   //16
   char  data[FILESERV_MAX_DATA];
}FILESERV_MSG_T;

typedef enum
{
   FILESERV_RESP_OK,
   FILESERV_RESP_ERROR,
   FILESERV_BULK_READ,
   FILESERV_BULK_WRITE,

} FILESERV_RESP_CODE_T;


/* Protocol (not used by API) version 1.2 */



#endif
